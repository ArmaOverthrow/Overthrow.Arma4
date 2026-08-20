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

	//! Vanilla's own multi-phase destruction bank (Prefabs/MP/MPDestructionManager.et:18). Its events are
	//! named SOUND_MPD_<material>, i.e. the prefab's inherited m_eMaterialSoundType.
	static const ResourceName DEFAULT_SOUND_PROJECT = "{5B79C73C52E6A74A}Sounds/Destruction/Multiphase/Destruction_Multiphase.acp";

	[Attribute("{6D89EA548ABDDF25}Particles/Enviroment/Building_Explosion_Debris_Brick.ptc", UIWidgets.ResourcePickerThumbnail, "Explosion particle raised on every machine when the structure is ruined", params: "ptc", category: "Overthrow Destruction")]
	protected ResourceName m_ExplosionParticle;

	[Attribute("{B0D882F853DE2783}Particles/Enviroment/Building_Explosion_Smoke.ptc", UIWidgets.ResourcePickerThumbnail, "Smoke particle raised on every machine when the structure is ruined", params: "ptc", category: "Overthrow Destruction")]
	protected ResourceName m_SmokeParticle;

	[Attribute("", UIWidgets.Auto, desc: "Positional one-shot played when the structure is ruined", category: "Overthrow Destruction")]
	protected ref SCR_AudioSourceConfiguration m_AudioSourceConfiguration;

	//! The owner's own model while intact, learned at runtime. Empty is LEGITIMATE - a composition root
	//! carries no model of its own - which is why the cached flag is separate from the emptiness test.
	protected ResourceName m_sIntactModel;
	protected bool m_bIntactModelCached;

	//! One deferral only, so a save applied mid-spawn cannot loop on a structure that never has a model.
	protected bool m_bRestoreDeferred;

	//! Built once, on the first ruin, when no config is authored on the prefab.
	protected ref SCR_AudioSourceConfiguration m_FallbackAudioConfiguration;

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

		GoToDamagePhase(PHASE_RUINED, false);

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

		if (phase == PHASE_INTACT)
			RepairToIntact();
		else
			GoToDamagePhase(phase, false);

		// Rpc() ARITY, HAND-AUDITED: RpcDo_ApplyPhase(int, bool) - two primitives, two passed.
		Rpc(RpcDo_ApplyPhase, phase, false);
		RpcDo_ApplyPhase(phase, false);
	}

	//------------------------------------------------------------------------------------------------
	//! The one wire message. The server invokes it locally as well as broadcasting, because the engine
	//! never loops a broadcast back to the sender; the guard is what stops the phase being driven twice
	//! there, while the effects branch runs everywhere.
	//! \param[in] phase The phase to apply.
	//! \param[in] withEffects Whether to raise the destruction effects.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_ApplyPhase(int phase, bool withEffects)
	{
		if (!Replication.IsServer())
		{
			if (phase >= PHASE_INTACT && phase < GetNumDamagePhases())
				GoToDamagePhase(phase, false);
		}

		if (withEffects)
			RaiseEffects();

		if (Replication.IsServer())
			ApplySupportStationState(phase == PHASE_INTACT);
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
	//! Explosion, smoke and a positional one-shot at the structure. Runs on every machine.
	//!
	//! Scripted rather than authored per damage phase: vanilla's SpawnDestroyObjects() early-returns
	//! without Physics (several of our structures are static geometry) and takes a notnull hit info
	//! ours would have to fake. PlayParticleEffect_CompleteDestruction needs neither.
	protected void RaiseEffects()
	{
		IEntity owner = GetOwner();
		if (!owner || owner.IsDeleted())
			return;

		SCR_DestructionCommon.PlayParticleEffect_CompleteDestruction(owner, m_ExplosionParticle, EDamageType.EXPLOSIVE, true);
		SCR_DestructionCommon.PlayParticleEffect_CompleteDestruction(owner, m_SmokeParticle, EDamageType.EXPLOSIVE, true);

		RaiseSound(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! ! NOT SCR_DestructionUtility.PlaySound(): that one hard-returns without SCR_MPDestructionManager,
	//! which is spawned late (up to 10 s on a dedicated server) and was never guaranteed to exist here.
	//! Every bail below is silent - an unset config, a missing sound bank and an out-of-range listener
	//! are all "no sound", not faults.
	//! \param[in] owner The structure being ruined.
	protected void RaiseSound(notnull IEntity owner)
	{
		SCR_AudioSourceConfiguration configuration = ResolveAudioConfiguration(owner);
		if (!configuration)
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

		SCR_AudioSource audioSource = soundManager.CreateAudioSource(owner, configuration, position);
		if (!audioSource)
			return;

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

		if (m_FallbackAudioConfiguration)
			return m_FallbackAudioConfiguration;

		SCR_DestructionMultiPhaseComponentClass componentData = SCR_DestructionMultiPhaseComponentClass.Cast(GetComponentData(owner));
		if (!componentData || componentData.m_eMaterialSoundType <= 0)
			return null;

		m_FallbackAudioConfiguration = new SCR_AudioSourceConfiguration();
		m_FallbackAudioConfiguration.m_sSoundProject = DEFAULT_SOUND_PROJECT;
		m_FallbackAudioConfiguration.m_sSoundEventName = SCR_SoundEvent.SOUND_MPD_ + typename.EnumToString(SCR_EMaterialSoundTypeBreak, componentData.m_eMaterialSoundType);
		m_FallbackAudioConfiguration.m_eFlags = EAudioSourceConfigurationFlag.Static | EAudioSourceConfigurationFlag.FinishWhenEntityDestroyed;

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
}
