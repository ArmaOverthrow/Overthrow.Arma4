[ComponentEditorProps(category: "Overthrow/Components/Damage", description: "Binary intact/ruined destruction for an Overthrow buildable structure")]
class OVT_StructureDestructionComponentClass : SCR_DestructionMultiPhaseComponentClass {};

//------------------------------------------------------------------------------------------------
//! Binary destruction for a built Overthrow structure: phase 0 intact, phase 1 ruined, and back.
//! The entity is never deleted - removal stays OVT_ResistanceFactionManager.DestroyPlacedItem()'s job.
//!
//! It exists because the vanilla base class has two holes (both verified, 1.8.0.10):
//!  1. GoToDamagePhase(0, ...) restores the phase but never schedules the mesh change - the guard at
//!     SCR_DestructionMultiPhaseComponent.c:227-229 returns first, because GetDamagePhaseData(0) is
//!     null by design. RepairToIntact() adds the CallLater super refused to.
//!  2. Vanilla's own broadcast cannot deliver a repair (its receiver only changes phase inside
//!     `if (phaseData)`, :434-449), so RpcDo_ApplyPhase below is the whole live-replication story.
//!
//! ! Adds NO navmesh call: super.GoToDamagePhase() already runs RegenerateNavmeshDelayed() (:189).
//! ! JIP/stream-in come from the base class (OnRplSave/OnRplLoad) and need an ACTIVE RplComponent.
//!
//! Rationale, the S1-S6 spike record and the vanilla-update checklist: docs/features/core/damage/.
//------------------------------------------------------------------------------------------------
class OVT_StructureDestructionComponent : SCR_DestructionMultiPhaseComponent
{
	static const int PHASE_INTACT = 0;
	static const int PHASE_RUINED = 1;

	//! Metres the ruin may stand above the terrain before the ground pool is skipped, and the lift given to
	//! the pool so it does not z-fight the surface.
	static const float GROUND_FIRE_HEIGHT_TOLERANCE = 1.0;
	static const float GROUND_FIRE_HEIGHT_OFFSET = 0.1;

	//! Vanilla's own multi-phase destruction bank (Prefabs/MP/MPDestructionManager.et:18). Its events are
	//! named SOUND_MPD_<material>, i.e. the prefab's inherited m_eMaterialSoundType.
	static const ResourceName DEFAULT_SOUND_PROJECT = "{5B79C73C52E6A74A}Sounds/Destruction/Multiphase/Destruction_Multiphase.acp";

	[Attribute("{EEAC86461B982EE4}Particles/Props/Explosion_Generic.ptc", UIWidgets.ResourcePickerThumbnail, "One-shot explosion raised on every machine when the structure is ruined", params: "ptc", category: "Overthrow Destruction")]
	protected ResourceName m_ExplosionParticle;

	[Attribute("{6D89EA548ABDDF25}Particles/Enviroment/Building_Explosion_Debris_Brick.ptc", UIWidgets.ResourcePickerThumbnail, "One-shot debris shower raised with the explosion", params: "ptc", category: "Overthrow Destruction")]
	protected ResourceName m_DebrisParticle;

	[Attribute("{4D5CD8B2B5DE8916}Particles/Vehicle/Vehicle_fire_engine_medium.ptc", UIWidgets.ResourcePickerThumbnail, "Retained flame burning at the ruin's bounding-box centre", params: "ptc", category: "Overthrow Destruction")]
	protected ResourceName m_FireParticle;

	[Attribute("{A9259561960FD620}Particles/Vehicle/Vehicle_fire_ground_medium.ptc", UIWidgets.ResourcePickerThumbnail, "Retained flat fire pool on the terrain under the ruin", params: "ptc", category: "Overthrow Destruction")]
	protected ResourceName m_GroundFireParticle;

	[Attribute("{3F7B398D4D154CC9}Particles/Vehicle/Vehicle_smoke_damaged_medium_01.ptc", UIWidgets.ResourcePickerThumbnail, "Retained smoke column rising from the ruin", params: "ptc", category: "Overthrow Destruction")]
	protected ResourceName m_SmokeParticle;

	[Attribute("120", UIWidgets.Slider, "Seconds the ruin burns for", "0 3600 1", category: "Overthrow Destruction")]
	protected float m_fFireSeconds;

	[Attribute("600", UIWidgets.Slider, "Seconds the ruin smokes for", "0 3600 1", category: "Overthrow Destruction")]
	protected float m_fSmokeSeconds;

	[Attribute("", UIWidgets.Auto, desc: "Positional material-break one-shot played when the structure is ruined", category: "Overthrow Destruction")]
	protected ref SCR_AudioSourceConfiguration m_AudioSourceConfiguration;

	[Attribute("{E4EF3755472EC669}Sounds/Particles/Logistics/Explosion/TNT/Particles_Explosions_TNT_Large.acp", UIWidgets.ResourceNamePicker, "Sound bank of the long-range blast raised when the structure is ruined", params: "acp", category: "Overthrow Destruction")]
	protected ResourceName m_ExplosionSoundProject;

	[Attribute("SOUND_EXPLOSION", UIWidgets.EditBox, "Event to play from the blast bank", category: "Overthrow Destruction")]
	protected string m_sExplosionSoundEvent;

	//! The owner's own model while intact, learned at runtime. Empty is LEGITIMATE - a composition root
	//! carries no model of its own - which is why the cached flag is separate from the emptiness test.
	protected ResourceName m_sIntactModel;
	protected bool m_bIntactModelCached;

	//! One deferral only, so a save applied mid-spawn cannot loop on a structure that never has a model.
	protected bool m_bRestoreDeferred;

	//! Built once, on the first ruin, when no config is authored on the prefab.
	protected ref SCR_AudioSourceConfiguration m_FallbackAudioConfiguration;

	//! Built once, on the first ruin, from the two blast attributes above (BD30).
	protected ref SCR_AudioSourceConfiguration m_ExplosionAudioConfiguration;

	//! Retained emitters, alive until their timer stops them or the structure is repaired.
	protected ParticleEffectEntity m_FireEffect;
	protected ParticleEffectEntity m_GroundFireEffect;
	protected ParticleEffectEntity m_SmokeEffect;

	//! Set around every phase drive that must not be seen or heard: a save restore, a stream-in, and
	//! the silent side of RuinIt/RpcDo_ApplyPhase.
	protected bool m_bSuppressEffects;

	//------------------------------------------------------------------------------------------------
	//! Whether this structure is currently a ruin. Safe on a client and before init - GetDamagePhase()
	//! answers 0 while the base class's phase data is unallocated.
	//! \return True when the structure is in any phase above intact.
	bool IsRuined()
	{
		return GetDamagePhase() != PHASE_INTACT;
	}

	//------------------------------------------------------------------------------------------------
	//! AUTHORITY ONLY. Drives the structure to the ruined phase and tells every machine.
	//! \param[in] withEffects Whether the explosion, smoke and sound are raised. False for a load.
	void RuinIt(bool withEffects = true)
	{
		if (!Replication.IsServer())
			return;

		// 🔴 With no authored phase, GoToDamagePhase(1) is "past the final phase" and DELETES the
		// entity (:176-183).
		if (GetNumDamagePhases() <= PHASE_RUINED)
		{
			Print("[OVT_StructureDestructionComponent] RuinIt refused: no damage phase is authored on this prefab, and driving past the final phase would delete the structure", LogLevel.WARNING);
			return;
		}

		if (IsRuined())
			return;

		m_bSuppressEffects = !withEffects;
		GoToDamagePhase(PHASE_RUINED, false);
		m_bSuppressEffects = false;

		// Rpc() ARITY, HAND-AUDITED (BUG-090 - a wrong count compiles clean and dies at the wire):
		// RpcDo_ApplyPhase(int phase, bool withEffects) takes two primitives; two are passed.
		Rpc(RpcDo_ApplyPhase, PHASE_RUINED, withEffects);
		RpcDo_ApplyPhase(PHASE_RUINED, withEffects);
	}

	//------------------------------------------------------------------------------------------------
	//! AUTHORITY ONLY. Restores the intact mesh and tells every machine. Never raises effects.
	void RepairIt()
	{
		if (!Replication.IsServer())
			return;

		if (!IsRuined())
			return;

		RepairToIntact();

		Rpc(RpcDo_ApplyPhase, PHASE_INTACT, false);
		RpcDo_ApplyPhase(PHASE_INTACT, false);
	}

	//------------------------------------------------------------------------------------------------
	//! AUTHORITY ONLY, SILENT. The save loader's entry point. It still broadcasts: a listen host doing a
	//! Continue can already have clients connected (BUG-104 class).
	//! \param[in] phase The phase to restore, 0 or 1.
	void RestorePhase(int phase)
	{
		if (!Replication.IsServer())
			return;

		if (phase < PHASE_INTACT || phase >= GetNumDamagePhases())
			return;

		if (phase == GetDamagePhase())
			return;

		// ORDER IS LOAD-BEARING: the intact model can only be learned from the owner's own VObject, so
		// it has to be cached BEFORE the ruin mesh replaces it - a structure restored as a ruin has no
		// other record of what to repair to (GetOriginalResourceName() is empty for it). A save can be
		// applied while the entity is still assembling, so an absent model gets exactly one frame to
		// arrive; after that whatever is there is taken, because an empty model is legitimate for a
		// composition root and must not be waited on forever.
		if (!m_bIntactModelCached && !m_bRestoreDeferred)
		{
			IEntity owner = GetOwner();
			if (owner && !owner.GetVObject())
			{
				ScriptCallQueue callQueue = GetGame().GetCallqueue();
				if (callQueue)
				{
					m_bRestoreDeferred = true;
					callQueue.CallLater(RestorePhase, 0, false, phase);
					return;
				}
			}
		}

		CacheIntactModel();

		m_bSuppressEffects = true;
		if (phase == PHASE_INTACT)
			RepairToIntact();
		else
			GoToDamagePhase(phase, false);
		m_bSuppressEffects = false;

		// Rpc() ARITY, HAND-AUDITED: RpcDo_ApplyPhase(int, bool) - two primitives, two passed.
		Rpc(RpcDo_ApplyPhase, phase, false);
		RpcDo_ApplyPhase(phase, false);
	}

	//------------------------------------------------------------------------------------------------
	//! The one wire message, and now ONLY the client's phase drive - effects and support stations moved
	//! onto the SetDamagePhase funnel below (BD27), which this reaches through GoToDamagePhase. The
	//! server still invokes it locally because the engine never loops a broadcast back to the sender;
	//! the authority guard is what stops the phase being driven twice there.
	//! \param[in] phase The phase to apply.
	//! \param[in] withEffects Whether the funnel raises the destruction effects.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_ApplyPhase(int phase, bool withEffects)
	{
		if (Replication.IsServer())
			return;

		if (phase < PHASE_INTACT || phase >= GetNumDamagePhases())
			return;

		m_bSuppressEffects = !withEffects;
		GoToDamagePhase(phase, false);
		m_bSuppressEffects = false;
	}

	//------------------------------------------------------------------------------------------------
	//! THE FUNNEL (BD27). Every phase change on every machine ends here - ours, vanilla's weapons path,
	//! vanilla's own broadcast receiver and the JIP load - so the effects and the support stations hang
	//! off this rather than off our RPC. GM "Neutralize" reaches it through
	//! SCR_DamageManagerComponent.Kill() -> OnDamage()'s m_TotalDestruction branch, which never touches
	//! our code.
	//! \param[in] damagePhase
	override protected void SetDamagePhase(int damagePhase)
	{
		int previous = GetDamagePhase();

		super.SetDamagePhase(damagePhase);

		if (previous == PHASE_INTACT && damagePhase > PHASE_INTACT)
			OnBecameRuin();
		else if (previous > PHASE_INTACT && damagePhase == PHASE_INTACT)
			OnBecameIntact();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBecameRuin()
	{
		if (Replication.IsServer())
			ApplySupportStationState(false);

		if (!m_bSuppressEffects)
			RaiseEffects();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBecameIntact()
	{
		if (Replication.IsServer())
			ApplySupportStationState(true);

		StopRuinEffects();
	}

	//------------------------------------------------------------------------------------------------
	//! Wrapped so a stream-in or JIP load never replays the destruction of something that was already a
	//! ruin when the client connected.
	protected override event bool OnRplLoad(ScriptBitReader reader)
	{
		m_bSuppressEffects = true;
		bool loaded = super.OnRplLoad(reader);
		m_bSuppressEffects = false;

		return loaded;
	}

	//------------------------------------------------------------------------------------------------
	//! AUTHORITY ONLY. Switches the structure's vanilla support stations off with the ruin and back on
	//! with the repair - the fuel depot's dispenser, the ramp's repair and salvage bays, the medical
	//! tent's aid station.
	//!
	//! Hiding this structure's OWN user actions is not enough: a support station is also reached from
	//! the consumer's side, so a vehicle parked at a wrecked depot would still find it and refuel.
	//! SCR_BaseSupportStationComponent.SetEnabled() is vanilla's own server-side switch, broadcasts
	//! itself and is a no-op when the state already matches; SCR_BaseSupportStationComponent.IsValid()
	//! refuses a disabled station from either direction.
	//!
	//! ! A repair re-enables EVERY station it finds. None of the eight buildables authors one disabled,
	//! and remembering which were would mean holding component references across a phase change.
	//! \param[in] usable False while the structure is a ruin.
	protected void ApplySupportStationState(bool usable)
	{
		IEntity owner = GetOwner();
		if (!owner || owner.IsDeleted())
			return;

		SetSupportStationsEnabled(owner, usable);

		// One level down, the same walk the facade's Resolve() does - the ramp carries its stations on
		// the same child this component sits on, the depot and the medical tent on the root itself.
		IEntity child = owner.GetChildren();
		while (child)
		{
			SetSupportStationsEnabled(child, usable);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entity The entity to switch. One entity can carry several stations - the ramp has a
	//! repair bay and a salvage bay - so this asks for all of them, not the first.
	//! \param[in] enabled Whether its support stations should work.
	protected void SetSupportStationsEnabled(notnull IEntity entity, bool enabled)
	{
		array<Managed> components = {};
		entity.FindComponents(SCR_BaseSupportStationComponent, components);

		foreach (Managed component : components)
		{
			SCR_BaseSupportStationComponent station = SCR_BaseSupportStationComponent.Cast(component);
			if (station)
				station.SetEnabled(enabled);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Phase 0 is routed to RepairToIntact(); everything else is the base class's business.
	//! \param[in] damagePhase
	//! \param[in] delayMeshChange
	override void GoToDamagePhase(int damagePhase, bool delayMeshChange)
	{
		CacheIntactModel();

		if (damagePhase == PHASE_INTACT)
		{
			RepairToIntact(delayMeshChange);
			return;
		}

		super.GoToDamagePhase(damagePhase, delayMeshChange);
	}

	//------------------------------------------------------------------------------------------------
	//! An empty model name means "this entity had no model of its own", which is the intact state of a
	//! composition root - so it clears the object rather than doing nothing.
	//! \param[in] modelName
	//! \param[in] useMaterialFromParent
	override protected void ChangeModel(ResourceName modelName, bool useMaterialFromParent = false)
	{
		if (modelName != ResourceName.Empty)
		{
			super.ChangeModel(modelName, useMaterialFromParent);
			return;
		}

		IEntity owner = GetOwner();
		if (!owner || owner.IsDeleted())
			return;

		owner.SetObject(null, string.Empty);
		owner.Update();
	}

	//------------------------------------------------------------------------------------------------
	//! super does everything phase 0 needs except the mesh - including cancelling a pending ChangeModel,
	//! which is why ChangeModel stays the scheduled function here. Runs on every machine, so no
	//! authority check.
	//! \param[in] delayMeshChange Whether the prefab's authored mesh-change delay applies.
	protected void RepairToIntact(bool delayMeshChange = false)
	{
		if (!IsRuined())
			return;

		super.GoToDamagePhase(PHASE_INTACT, delayMeshChange);

		ScriptCallQueue callQueue = GetGame().GetCallqueue();
		if (!callQueue)
			return;

		int delay;
		if (delayMeshChange)
		{
			SCR_DestructionMultiPhaseComponentClass componentData = SCR_DestructionMultiPhaseComponentClass.Cast(GetComponentData(GetOwner()));
			if (componentData)
				delay = componentData.m_fMeshChangeDelay;
		}

		callQueue.CallLater(ChangeModel, delay, param1: m_sIntactModel, param2: false);
	}

	//------------------------------------------------------------------------------------------------
	//! A one-shot explosion plus debris, then a retained fire and smoke column that outlive the blast
	//! (BD28). Runs on every machine, never on a headless server.
	//!
	//! Scripted rather than authored per damage phase: vanilla's SpawnDestroyObjects() early-returns
	//! without Physics (several of our structures are static geometry) and takes a notnull hit info
	//! ours would have to fake. PlayParticleEffect_CompleteDestruction needs neither.
	protected void RaiseEffects()
	{
		if (System.IsConsoleApp())
			return;

		IEntity owner = GetOwner();
		if (!owner || owner.IsDeleted())
			return;

		SCR_DestructionCommon.PlayParticleEffect_CompleteDestruction(owner, m_ExplosionParticle, EDamageType.EXPLOSIVE, true);
		SCR_DestructionCommon.PlayParticleEffect_CompleteDestruction(owner, m_DebrisParticle, EDamageType.EXPLOSIVE, true);

		StartRetainedEffects(owner);

		RaiseSound(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! The flame rides the structure at its bounding-box centre; the ground pool is UNPARENTED at a
	//! terrain-snapped world position, which is how vanilla spawns each of them
	//! (SCR_FlammableHitZone.UpdateFireEffects vs StartDestructionGroundFire). Smoke follows the flame.
	//! Both are stopped by their own timers - a ParticleEffectEntity has no lifetime field.
	//! \param[in] owner The structure being ruined.
	protected void StartRetainedEffects(notnull IEntity owner)
	{
		ScriptCallQueue callQueue = GetGame().GetCallqueue();

		vector mins;
		vector maxs;
		owner.GetBounds(mins, maxs);
		vector localCentre = vector.Lerp(mins, maxs, 0.5);

		if (m_fFireSeconds > 0)
		{
			bool started;

			if (!m_FireEffect && m_FireParticle != ResourceName.Empty)
			{
				m_FireEffect = SpawnAttachedEffect(owner, m_FireParticle, localCentre);
				started = started || m_FireEffect != null;
			}

			if (!m_GroundFireEffect && m_GroundFireParticle != ResourceName.Empty)
			{
				m_GroundFireEffect = SpawnGroundEffect(owner, m_GroundFireParticle);
				started = started || m_GroundFireEffect != null;
			}

			if (started && callQueue)
				callQueue.CallLater(StopRuinFire, m_fFireSeconds * 1000);
		}

		if (!m_SmokeEffect && m_SmokeParticle != ResourceName.Empty && m_fSmokeSeconds > 0)
		{
			m_SmokeEffect = SpawnAttachedEffect(owner, m_SmokeParticle, localCentre);
			if (m_SmokeEffect && callQueue)
				callQueue.CallLater(StopRuinSmoke, m_fSmokeSeconds * 1000);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] owner The structure the emitter follows.
	//! \param[in] particle The .ptc to play.
	//! \param[in] localOffset Offset from the owner's origin, in its own space.
	//! \return The spawned emitter, or null.
	protected ParticleEffectEntity SpawnAttachedEffect(notnull IEntity owner, ResourceName particle, vector localOffset)
	{
		ParticleEffectEntitySpawnParams spawnParams = new ParticleEffectEntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.LOCAL;
		spawnParams.Transform[3] = localOffset;
		spawnParams.FollowParent = owner;
		spawnParams.PlayOnSpawn = true;
		spawnParams.UseFrameEvent = true;
		spawnParams.DeleteWhenStopped = true;

		return ParticleEffectEntity.SpawnParticleEffect(particle, spawnParams);
	}

	//------------------------------------------------------------------------------------------------
	//! The flat fire pool. UNPARENTED and in WORLD space, snapped to the terrain under the structure's
	//! bounding-box centre - parenting it with a local offset buries it inside the ruin mesh, because most
	//! Overthrow buildables have their origin at the foundation (BD28).
	//! \param[in] owner The structure being ruined.
	//! \param[in] particle The .ptc to play.
	//! \return The spawned emitter, or null.
	protected ParticleEffectEntity SpawnGroundEffect(notnull IEntity owner, ResourceName particle)
	{
		BaseWorld world = owner.GetWorld();
		if (!world)
			return null;

		vector mins;
		vector maxs;
		owner.GetWorldBounds(mins, maxs);
		vector position = vector.Lerp(mins, maxs, 0.5);

		float surfaceY = world.GetSurfaceY(position[0], position[2]);

		// Vanilla's own tolerance: a structure standing well clear of the terrain gets no ground pool.
		if (mins[1] - surfaceY > GROUND_FIRE_HEIGHT_TOLERANCE)
			return null;

		position[1] = surfaceY + GROUND_FIRE_HEIGHT_OFFSET;

		ParticleEffectEntitySpawnParams spawnParams = new ParticleEffectEntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform[3] = position;
		spawnParams.PlayOnSpawn = true;
		spawnParams.UseFrameEvent = true;
		spawnParams.DeleteWhenStopped = true;

		return ParticleEffectEntity.SpawnParticleEffect(particle, spawnParams);
	}

	//------------------------------------------------------------------------------------------------
	//! Stops both retained emitters now and drops their pending timers. Safe to call when nothing burns.
	void StopRuinEffects()
	{
		ScriptCallQueue callQueue = GetGame().GetCallqueue();
		if (callQueue)
		{
			callQueue.Remove(StopRuinFire);
			callQueue.Remove(StopRuinSmoke);
		}

		StopRuinFire();
		StopRuinSmoke();
	}

	//------------------------------------------------------------------------------------------------
	protected void StopRuinFire()
	{
		if (m_FireEffect)
		{
			SCR_ParticleHelper.StopParticleEmissionAndLights(m_FireEffect);
			m_FireEffect = null;
		}

		if (m_GroundFireEffect)
		{
			SCR_ParticleHelper.StopParticleEmissionAndLights(m_GroundFireEffect);
			m_GroundFireEffect = null;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void StopRuinSmoke()
	{
		if (!m_SmokeEffect)
			return;

		SCR_ParticleHelper.StopParticleEmissionAndLights(m_SmokeEffect);
		m_SmokeEffect = null;
	}

	//------------------------------------------------------------------------------------------------
	//! TWO LAYERS (BD30): vanilla's big-explosion bank first - the blast a player hundreds of metres
	//! away is meant to hear and take a bearing from - then the multi-phase material break underneath it
	//! for the structure's own character. The break event alone is a close-range noise.
	//!
	//! ! NOT SCR_DestructionUtility.PlaySound(): that one hard-returns without SCR_MPDestructionManager,
	//! which is spawned late (500 ms, or 10 s on a dedicated server), so a ruin inside that window would
	//! be silent.
	//! Every bail below is silent - an unset config, a missing sound bank and an out-of-range listener
	//! are all "no sound", not faults.
	//! \param[in] owner The structure being ruined.
	protected void RaiseSound(notnull IEntity owner)
	{
		if (System.IsConsoleApp())
			return;

		World world = owner.GetWorld();
		if (!world)
			return;

		SCR_SoundManagerModule soundManager = SCR_SoundManagerModule.GetInstance(world);
		if (!soundManager)
			return;

		vector mins;
		vector maxs;
		owner.GetWorldBounds(mins, maxs);
		vector position = vector.Lerp(mins, maxs, 0.5);

		PlayExplosionSound(soundManager, owner, position);
		PlayMaterialBreakSound(soundManager, owner, position);
	}

	//------------------------------------------------------------------------------------------------
	//! The blast. SCR_SoundManagerModule sets the Distance signal itself, which is what selects the
	//! bank's far layers, and the EnvironmentSignals flag gives it its tail - the same call shape and the
	//! same flags vanilla's own FuelTank_03 destruction uses (Prefabs/.../FuelTank_03_base.et:31-36).
	//! \param[in] soundManager
	//! \param[in] owner The structure being ruined.
	//! \param[in] position World position to play at.
	protected void PlayExplosionSound(notnull SCR_SoundManagerModule soundManager, notnull IEntity owner, vector position)
	{
		if (m_ExplosionSoundProject == ResourceName.Empty || m_sExplosionSoundEvent == string.Empty)
			return;

		if (!m_ExplosionAudioConfiguration)
		{
			m_ExplosionAudioConfiguration = new SCR_AudioSourceConfiguration();
			m_ExplosionAudioConfiguration.m_eFlags = EAudioSourceConfigurationFlag.Static | EAudioSourceConfigurationFlag.EnvironmentSignals | EAudioSourceConfigurationFlag.FinishWhenEntityDestroyed;
		}

		m_ExplosionAudioConfiguration.m_sSoundProject = m_ExplosionSoundProject;
		m_ExplosionAudioConfiguration.m_sSoundEventName = m_sExplosionSoundEvent;

		SCR_AudioSource audioSource = soundManager.CreateAudioSource(owner, m_ExplosionAudioConfiguration, position);
		if (!audioSource)
			return;

		soundManager.PlayAudioSource(audioSource);
	}

	//------------------------------------------------------------------------------------------------
	//! The quieter structural layer. Its two signals are mandatory - without them the multi-phase bank's
	//! events resolve to nothing audible (BD29).
	//! \param[in] soundManager
	//! \param[in] owner The structure being ruined.
	//! \param[in] position World position to play at.
	protected void PlayMaterialBreakSound(notnull SCR_SoundManagerModule soundManager, notnull IEntity owner, vector position)
	{
		SCR_AudioSourceConfiguration configuration = ResolveAudioConfiguration(owner);
		if (!configuration)
			return;

		SCR_AudioSource audioSource = soundManager.CreateAudioSource(owner, configuration, position);
		if (!audioSource)
			return;

		audioSource.SetSignalValue(SCR_AudioSource.PHASES_TO_DESTROYED_PHASE_SIGNAL_NAME, GetNumDamagePhases() - GetDamagePhase() - 1);
		audioSource.SetSignalValue(SCR_AudioSource.ENTITY_SIZE_SIGNAL_NAME, SCR_DestructionUtility.GetDestructibleSize(owner));

		soundManager.PlayAudioSource(audioSource);
	}

	//------------------------------------------------------------------------------------------------
	//! The authored config when a prefab carries one, otherwise vanilla's own multi-phase destruction
	//! bank keyed by the prefab's material type - so no prefab has to author sound for a ruin to be
	//! audible, and one that wants a different sound still wins.
	//! \param[in] owner The structure being ruined.
	//! \return A valid configuration, or null when there is nothing to play.
	protected SCR_AudioSourceConfiguration ResolveAudioConfiguration(notnull IEntity owner)
	{
		if (m_AudioSourceConfiguration && m_AudioSourceConfiguration.IsValid())
			return m_AudioSourceConfiguration;

		SCR_DestructionMultiPhaseComponentClass componentData = SCR_DestructionMultiPhaseComponentClass.Cast(GetComponentData(owner));
		if (!componentData || componentData.m_eMaterialSoundType <= 0)
			return null;

		string eventName = SCR_SoundEvent.SOUND_MPD_ + typename.EnumToString(SCR_EMaterialSoundTypeBreak, componentData.m_eMaterialSoundType);

		// Vanilla's own config once the manager has spawned; ours while it has not (D6: a null manager is
		// never a fault). Rewriting its event name per play is what SCR_DestructionUtility.PlaySound does.
		SCR_MPDestructionManager destructionManager = SCR_MPDestructionManager.GetInstance();
		if (destructionManager)
		{
			SCR_AudioSourceConfiguration managerConfiguration = destructionManager.GetAudioSourceConfiguration();
			if (managerConfiguration)
			{
				managerConfiguration.m_sSoundEventName = eventName;
				return managerConfiguration;
			}
		}

		if (!m_FallbackAudioConfiguration)
		{
			m_FallbackAudioConfiguration = new SCR_AudioSourceConfiguration();
			m_FallbackAudioConfiguration.m_sSoundProject = DEFAULT_SOUND_PROJECT;
			m_FallbackAudioConfiguration.m_eFlags = EAudioSourceConfigurationFlag.Static | EAudioSourceConfigurationFlag.FinishWhenEntityDestroyed;
		}

		m_FallbackAudioConfiguration.m_sSoundEventName = eventName;

		return m_FallbackAudioConfiguration;
	}

	//------------------------------------------------------------------------------------------------
	//! Learns the owner's intact model once, and only while the structure IS intact - so one that came
	//! back from a save or streamed in as a ruin never records the ruin as the thing to repair to.
	protected void CacheIntactModel()
	{
		if (m_bIntactModelCached)
			return;

		if (GetDamagePhase() != PHASE_INTACT)
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		m_bIntactModelCached = true;

		VObject vObject = owner.GetVObject();
		if (vObject)
			m_sIntactModel = vObject.GetResourceName();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		CacheIntactModel();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		StopRuinEffects();

		super.OnDelete(owner);
	}
}
