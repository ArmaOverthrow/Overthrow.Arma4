[EntityEditorProps(category: "Overthrow/Managers", description: "Registry of virtualized AI groups and ambient spawn sources")]
class OVT_VirtualizationManagerComponentClass : OVT_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Virtualization core - the server-side registry of virtualized AI groups (implementation.md §3.3).
//!
//! WHAT THIS IS. A thin Overthrow layer over the Reforger 1.8 native AI lifecycle. The ENGINE owns
//! proximity, the spawn queue, dormancy and group-state persistence
//! (SCR_EAIGroupLifecyclePolicy.ProximityDriven + ObserversSystem + ChimeraAIWorld's budgeted
//! queue + SCR_AIGroupSerializer). Core owns the registry, the config surface, slot-accurate
//! survivor truth and wipe bookkeeping. Core NEVER polls proximity for tracked groups.
//!
//! SERVER ONLY (D7). Nothing here replicates: no replicated properties, no remote calls, no JIP
//! payload, no client half - the feature's Definition of Done greps this directory to prove it.
//! The server guard sits in OnPostInit BEFORE any collection is allocated, so a client's copy of
//! this component holds nothing at all and every public entry point below fails closed.
//!
//! ============================ SCOPE AS OF PHASE 5 ============================
//! Tracked groups are fully wired: RegisterGroup builds a real dormant SCR_AIGroup entity with the
//! engine's ProximityDriven policy, resolved distances, importance, waypoint entities and per-slot
//! survivor mask. Deaths, wipes, count correction, force spawn/despawn and teardown are live. Ambient
//! spawn sources are live (Phase 4). Registry persistence is live (Phase 5) and is ROUTE B: core
//! writes complete re-creation state and rebuilds its own group entities on load - vanilla persists
//! none of them. See "REGISTRY PERSISTENCE" below and api.md §8.
//! =============================================================================
//------------------------------------------------------------------------------------------------
class OVT_VirtualizationManagerComponent : OVT_Component
{
	//------------------------------------------------------------------------------------------------
	// ATTRIBUTES
	//------------------------------------------------------------------------------------------------

	[Attribute(defvalue: "1", desc: "Default SCR_EAISpawnImportance for registrations that do not choose one (0 LOW, 1 NORMAL, 2 HIGH, 3 CRITICAL). Overthrow policy is NORMAL - vanilla's LOW default is capped at 50% of the AI budget and evicted first (D4)")]
	protected int m_iDefaultImportance;

	[Attribute(defvalue: "1.15", desc: "Despawn distance = spawn distance x this. The gap is the engine's anti-thrash band")]
	protected float m_fDespawnHysteresis;

	[Attribute(defvalue: "1750", desc: "Fallback spawn distance used only when Overthrow_Config.json has not been loaded yet")]
	protected int m_iFallbackSpawnDistance;

	[Attribute(defvalue: "2000", desc: "Ambient source tick interval in ms. THE only tick core owns - tracked groups are the engine's job")]
	protected int m_iAmbientTickIntervalMs;

	[Attribute(defvalue: "8", desc: "Ambient sources evaluated per tick. The round-robin slice size: with 40 sources and 8 per tick every source is evaluated once per 10 s")]
	protected int m_iAmbientSourcesPerTick;

	[Attribute(defvalue: "3", desc: "Ambient entities spawned per source per tick. A source that rolled 20 civilians fills over several ticks instead of hitching one frame")]
	protected int m_iAmbientSpawnsPerTick;

	[Attribute(defvalue: "1.15", desc: "Ambient despawn distance = spawn distance x this. The gap is the anti-thrash band, mirroring the engine's own group hysteresis")]
	protected float m_fAmbientDespawnHysteresis;

	[Attribute(desc: "Optional registry of authored ambient spawn sources. CORE SHIPS THIS EMPTY - consumers (`civilians`) author the content and point this at their own .conf")]
	protected ref OVT_AmbientSpawnSourceRegistry m_AmbientRegistry;

	[Attribute(defvalue: "false", desc: "Keep registered group entities persistence-tracked (exempt from the BUG-118 untrack). OFF until a registered-groups-only self-spawn path exists - a tracked group that cannot self-spawn writes a record nothing will ever claim. See api.md 'Persistence'")]
	protected bool m_bPersistGroupEntities;

	[Attribute(defvalue: "true", desc: "A player's PARKED RECRUIT GROUP acts as an AI observer: registered groups inside its spawn ring materialise with no player anywhere near. That is the feature AND the budget risk - a squad parked in a town holds that town's garrisons and patrols spawned for as long as it stands there. Switch OFF to make parked recruits inert again (virtualization/integration D16). Read by the recruit group's own marker component; core never applies it to anyone else's AddEntityObserver call")]
	protected bool m_bRecruitGroupsAreObservers;

	[Attribute(defvalue: "false", desc: "DEBUG ONLY: log every ambient activation, per-tick spawn batch and despawn with timestamps. This is what the Phase 4 play-test ('a source of 20 spawns over several ticks') reads. Leave off for normal play")]
	protected bool m_bDebugAmbientLogging;

	[Attribute(defvalue: "false", desc: "DEBUG ONLY: register one virtual group near the campaign start position at PostGameStart, so core can be play-tested with no consumer wired. Leave off for normal play")]
	protected bool m_bDebugRegisterTestGroup;

	[Attribute(defvalue: "600", desc: "Debug test group: metres east of the campaign start position to register it at. With a count above 1 this is the RADIUS of the disc the groups are spread over")]
	protected float m_fDebugTestGroupOffset;

	[Attribute(defvalue: "1", desc: "DEBUG ONLY: how many test groups to register. 1 keeps the single-group affordance the Phase 3 play-test uses; ~40 is the scale probe (implementation.md T6.1 / §6 step 8) - fast-travel in and watch the fill. Registration wall time is logged either way")]
	protected int m_iDebugTestGroupCount;

	[Attribute(defvalue: "", desc: "Debug test group: group registry name to register from the occupying faction, e.g. 'rifle_squad'. EMPTY uses the faction's first resolvable registry entry; a name that does not resolve falls back to that too, with a warning")]
	protected string m_sDebugTestGroupName;

	[Attribute(defvalue: "-1", desc: "DEBUG ONLY: per-test-group spawn distance in metres, or -1 for the global virtualizationSpawnDistance. Set ~300 (despawn ring ~345 m) to watch the engine spawn/despawn from a GM camera, which is itself an observer and cannot be switched off at the global 1750 m ring. ⚠ NOT 0: a resolved distance of 0 means the Manual policy (never materialise by proximity), not a tiny ring")]
	protected int m_iDebugTestGroupSpawnDistance;

	//------------------------------------------------------------------------------------------------
	// MEMBER VARIABLES
	//------------------------------------------------------------------------------------------------

	//! handle -> record. Allocated on the SERVER ONLY; null everywhere else, which is what makes
	//! every entry point below fail closed on a client.
	protected ref map<int, ref OVT_VirtualGroupRecord> m_mRecords;

	//! group entity -> handle. The engine hands core its own group back in every lifecycle signal
	//! (GetOnMembersDespawning, GetOnEliminatedWhenReached, the ExpandOneMember seam); this is how
	//! those become a handle without walking the registry.
	protected ref map<EntityID, int> m_mHandleByGroup;

	//! member entity -> (handle, slot). Built as members materialise, dropped as they despawn - so
	//! it only ever describes members that exist right now, which is exactly what makes it a safe
	//! answer to "which roster slot did this corpse occupy?" (D2).
	protected ref map<EntityID, ref OVT_VirtualMemberSlot> m_mMemberSlots;

	//! True once the game-mode kill invoker has been subscribed, so a second Init cannot double-fire
	//! every death (and OnDelete knows whether to unsubscribe).
	protected bool m_bKillHookInstalled;

	//! Monotonic handle counter. Persisted (Phase 5) so a handle is NEVER reused across a reload -
	//! a consumer holding a stale handle must get "unknown", never someone else's group.
	protected int m_iNextHandle;

	//! Round-robin cursor for the ambient tick: where the last tick's slice stopped.
	protected int m_iAmbientCursor;

	//! ambient handle -> instance. THE owner of the instances (the order array below holds handles,
	//! not objects, so a source exists in exactly one place).
	protected ref map<int, ref OVT_AmbientSpawnSourceInstance> m_mAmbientSources;

	//! Ambient handles in registration order - what the round-robin slice indexes into. A map has no
	//! stable iteration order, and a round-robin over an unstable order can starve a source forever.
	protected ref array<int> m_aAmbientOrder;

	//! ambient entity -> owning source handle. THE reverse map that makes ReleaseAmbientEntity O(1)
	//! (api.md §4): `civilians` calls it on every recruited civilian, potentially mid-crowd.
	protected ref map<EntityID, int> m_mAmbientByEntity;

	//! Monotonic ambient handle counter, deliberately SEPARATE from the group counter: ambient
	//! handles are never persisted, so they must not consume numbers from the persisted sequence.
	protected int m_iNextAmbientHandle;

	//! True once the ambient CallLater is running, so it can never be installed twice (two ticks
	//! would double every spawn budget and halve the round-robin period).
	protected bool m_bAmbientTickRunning;

	//! True once the debug observer probe (m_bDebugRegisterTestGroup only) is running, so it can never
	//! be installed twice and double every log block.
	protected bool m_bDebugObserverTickRunning;

	//! followed entity -> SP observer key (the AI OBSERVERS section at the bottom of this file).
	//!
	//! THIS MAP IS THE ANSWER TO "does that entity have an observer?", never the engine: application of
	//! an InsertObserverSP/RemoveObserverSP is DEFERRED BY ONE FRAME both ways (measured, T1.9), so a
	//! consumer that asked the engine would read a false negative right after adding and a false leak
	//! right after removing. Keyed on EntityID rather than on the entity so a stale entry can be swept
	//! after its entity is gone.
	protected ref map<EntityID, int> m_mEntityObservers;

	//! Monotonic SP observer key counter, namespaced away from every other key this mod parks (the
	//! Phase 1 spike used 770019). NEVER persisted and NEVER replicated: a key is meaningful only to
	//! the local ObserversSystem it was inserted into, so a saved one would name nothing on load.
	protected int m_iNextObserverKey;

	protected ref ScriptInvoker m_OnGroupWiped;
	protected ref ScriptInvoker m_OnRecordsRestored;

	//------------------------------------------------------------------------------------------------
	// STATIC
	//------------------------------------------------------------------------------------------------

	static OVT_VirtualizationManagerComponent s_Instance;

	//------------------------------------------------------------------------------------------------
	//! \return The manager on the game mode, or null before the game mode exists / when the
	//! component is not on the prefab.
	//!
	//! ⚠ RE-RESOLVES ACROSS A WORLD (R7). A static outlives a campaign - measured in the Phase 1 spike:
	//! statics survive both a Continue and a new campaign in the same process. The cached instance is
	//! therefore only trusted while it still belongs to the game mode that exists NOW; a manager left
	//! over from the previous campaign is dropped and looked up again, so the second campaign of a
	//! session can never be handed the first one's registry.
	static OVT_VirtualizationManagerComponent GetInstance()
	{
		BaseGameMode gameMode = GetGame().GetGameMode();

		// No game mode yet (very early init, or a world without one): answer with whatever OnPostInit
		// claimed rather than dropping a perfectly good instance.
		if (!gameMode)
			return s_Instance;

		IEntity gameModeEntity = gameMode;
		if (s_Instance && s_Instance.GetOwner() != gameModeEntity)
			s_Instance = null;

		if (!s_Instance)
			s_Instance = OVT_VirtualizationManagerComponent.Cast(gameMode.FindComponent(OVT_VirtualizationManagerComponent));

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	// LIFECYCLE
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server guard FIRST, allocation second (the OVT_DeploymentManagerComponent shape) - a client
	//! that allocated the registry would look initialised and silently accept registrations that can
	//! never reach the authority.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		// Claimed here rather than lazily, because the modded SCR_AIGroup reaches the manager through
		// this static as members materialise and must not depend on somebody having called
		// GetInstance() first. OnDelete clears it.
		s_Instance = this;

		if (!Replication.IsServer())
			return;

		m_mRecords = new map<int, ref OVT_VirtualGroupRecord>();
		m_mHandleByGroup = new map<EntityID, int>();
		m_mMemberSlots = new map<EntityID, ref OVT_VirtualMemberSlot>();
		m_iNextHandle = 1;

		m_mAmbientSources = new map<int, ref OVT_AmbientSpawnSourceInstance>();
		m_aAmbientOrder = new array<int>();
		m_mAmbientByEntity = new map<EntityID, int>();
		m_iNextAmbientHandle = 1;
		m_iAmbientCursor = 0;

		// Allocated behind the same server guard as everything else, so AddEntityObserver on a client
		// fails closed on the null map rather than parking an observer nothing would ever remove.
		m_mEntityObservers = new map<EntityID, int>();
		m_iNextObserverKey = ENTITY_OBSERVER_KEY_BASE;
	}

	//------------------------------------------------------------------------------------------------
	//! Called from OVT_OverthrowGameMode.EOnInit, after the deployment manager.
	//!
	//! Installs the ONE subscription core owns: the game-mode kill invoker, raised by the modded
	//! damage manager for every character including AI. Deaths must come from there and never from
	//! the group's agent-removal invoker, which cannot tell a casualty from the engine's own dormancy
	//! teardown (D2a) - counting a teardown as deaths would wipe every record on the first despawn.
	//! \param[in] owner The game mode entity.
	void Init(IEntity owner)
	{
		if (!Replication.IsServer())
			return;

		if (!m_bKillHookInstalled)
		{
			// The OWNER first (Init is called from the game mode's own EOnInit, where GetGame() may not
			// name it yet), so the subscription and OnDelete's matching unsubscribe always talk about the
			// same game mode - the one this component is actually on.
			OVT_OverthrowGameMode gameMode = OVT_OverthrowGameMode.Cast(owner);
			if (!gameMode)
				gameMode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());

			if (gameMode && gameMode.GetOnCharacterKilled())
			{
				gameMode.GetOnCharacterKilled().Insert(OnCharacterKilled);
				m_bKillHookInstalled = true;
			}
			else
			{
				Print("[Overthrow] Virtualization: no game-mode kill invoker - member deaths will NOT be recorded", LogLevel.WARNING);
			}
		}

		// VERBOSE, like every other informational line in this feature (T6.6): the only things core
		// prints at NORMAL or above are operator-actionable problems - a composition that will not
		// resolve, a record being dropped - and the debug affordances, which are off by default.
		Print(string.Format("[Overthrow] VirtualizationManager initialized (global spawn distance %1 m, default importance %2)",
			GetGlobalSpawnDistance().ToString(), OVT_VirtualizationMath.ResolveImportance(-1, m_iDefaultImportance).ToString()), LogLevel.VERBOSE);
	}

	//------------------------------------------------------------------------------------------------
	//! Called from OVT_OverthrowGameMode.DoStartGame(). Registers the debug group when the (default
	//! off) attribute asks for it - core ships with no consumers, so this is the only way to
	//! play-test the engine lifecycle before `civilians` or `integration` land (T3.9, R10).
	void PostGameStart()
	{
		if (!Replication.IsServer())
			return;

		// The one tick core owns (T4.3). Started here rather than at OnPostInit so it never runs
		// against a half-built world, and idempotent so a consumer registering a source before the
		// game starts cannot end up with two.
		EnsureAmbientTick();

		if (m_bDebugRegisterTestGroup)
		{
			RegisterDebugTestGroup();
			EnsureDebugObserverTick();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Restart hygiene (R7). A second campaign in the same session must inherit nothing: no ambient
	//! tick, no kill subscription pointing at a dead component, no group entities from the previous
	//! world, and no stale s_Instance.
	override void OnDelete(IEntity owner)
	{
		// The call queue lives on the GAME, not on the world, so an entry left here would survive into
		// the next campaign of the same session and tick a dead manager (R7). Null-guarded because a
		// teardown can run late enough that the game object is already going away.
		ScriptCallQueue queue;
		if (GetGame())
			queue = GetGame().GetCallqueue();

		if (queue)
		{
			queue.Remove(AmbientTick);

			// The debug observer probe, same reasoning: a repeating entry that outlived its manager
			// would log the previous campaign's registry forever.
			queue.Remove(DebugObserverTick);

			// The restore announcement is the only other call-queue entry this feature owns, and it holds
			// a reference to this component - a pending one would fire into a dead manager on the next
			// world, re-firing GetOnRecordsRestored() from the previous campaign's latch.
			queue.Remove(CompleteRecordsRestore);
		}

		m_bRecordsRestoredPending = false;
		m_iRecordsRestoredWaits = 0;
		m_bAmbientTickRunning = false;
		m_bDebugObserverTickRunning = false;

		ClearAmbientState();

		// ⚠ THE ONE THING THIS TEARDOWN DESTROYS. Everything else below is detach-don't-destroy
		// (DetachAllRegisteredGroups / ClearAmbientState both say so at length) because records and
		// their entities are things the NEXT world may still want. An SP observer is not a record: it
		// is a live entry in an engine system, keyed by an integer this component is the only holder
		// of, and one left behind pins whatever it is standing over materialised for the rest of the
		// PROCESS - across a quit-to-menu, across a second campaign, with nothing left that could
		// name the key to remove it. So observers are removed, and they are removed first.
		RemoveAllEntityObservers();

		if (m_bKillHookInstalled)
		{
			// THE OWNER's game mode, not GetGame()'s. A teardown can run after the next campaign's game
			// mode exists, and unsubscribing from THAT one would leave this component's subscription on
			// the dying invoker while pretending to have cleaned it up.
			OVT_OverthrowGameMode gameMode = OVT_OverthrowGameMode.Cast(owner);
			if (!gameMode && GetGame())
				gameMode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());

			if (gameMode && gameMode.GetOnCharacterKilled())
				gameMode.GetOnCharacterKilled().Remove(OnCharacterKilled);

			m_bKillHookInstalled = false;
		}

		DetachAllRegisteredGroups();

		// Statics outlive a world (the Phase 1 spike measured exactly this: a static survives a
		// Continue and a new campaign in the same process), so both of core's are reset here.
		SCR_AIGroup.ArmPersistenceExemption(false);

		if (s_Instance == this)
			s_Instance = null;

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! World-teardown cleanup (T3.11 / R7): detaches everything core hooked and empties the registry
	//! so a second campaign in the same session inherits nothing.
	//!
	//! ⚠ DELIBERATELY DESTROYS NOTHING. Registered group entities and their waypoints are
	//! persistence-TRACKED, and deleting a tracked entity drops its stored record with it (SelfDelete
	//! defaults on) - so "delete everything live at teardown" would erase, at quit-to-menu, exactly
	//! the records this feature exists to keep. The world being torn down takes the entities anyway;
	//! what must not survive it is script state, and that is what this clears.
	//!
	//! The destructive path is UnregisterGroup (a consumer really removing a group) and the wipe
	//! cleanup - both of which untrack ON PURPOSE.
	protected void DetachAllRegisteredGroups()
	{
		if (!m_mRecords)
			return;

		foreach (int handle, OVT_VirtualGroupRecord record : m_mRecords)
		{
			SCR_AIGroup group = ResolveGroup(record);
			if (group)
			{
				group.GetOnMembersDespawning().Remove(OnGroupMembersDespawning);
				group.GetOnEliminatedWhenReached().Remove(OnGroupEliminatedWhenReached);
				group.ClearOVTSlotMask();
			}

			record.m_Group = null;
		}

		m_mRecords.Clear();

		if (m_mHandleByGroup)
			m_mHandleByGroup.Clear();

		if (m_mMemberSlots)
			m_mMemberSlots.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! World-teardown cleanup for the ambient half (R7).
	//!
	//! ⚠ DELIBERATELY DESTROYS NOTHING, for a different reason than the tracked half: ambient
	//! entities are transient by construction and the world being torn down takes them anyway, while
	//! deleting entities from inside a teardown is how a shutdown turns into an error cascade. What
	//! must not survive a world is SCRIPT state, and that is what this drops - so a second campaign in
	//! the same session starts with no sources, no reverse-map entries and a cursor at 0.
	protected void ClearAmbientState()
	{
		if (m_mAmbientSources)
			m_mAmbientSources.Clear();

		if (m_aAmbientOrder)
			m_aAmbientOrder.Clear();

		if (m_mAmbientByEntity)
			m_mAmbientByEntity.Clear();

		m_iAmbientCursor = 0;
	}

	//------------------------------------------------------------------------------------------------
	// TRACKED GROUPS - REGISTRATION
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Registers a virtual AI group and returns its handle.
	//!
	//! Builds a real group entity in the world with ZERO members (SCR_AIGroup.IgnoreSpawning), stamps
	//! the engine's ProximityDriven lifecycle policy with the resolved distances and importance,
	//! turns the plan into owned AIWaypoint entities, captures the roster into the survivor mask and
	//! opts the group into persistence. The engine's own 1 Hz lifecycle tick materialises it when an
	//! observer arrives - core NEVER polls proximity for a tracked group.
	//!
	//! ⚠ SetEliminateWhenReached is deliberately NOT stamped here (Phase 1 amendment 3). With it set,
	//! an observer inside veryNearBlockDist (150 m default) of a dormant group makes the engine DELETE
	//! the group entity outright - and observers include non-players (the deploy-point preload MP
	//! observer, cameras, the optics far-observer). It is enabled only once the mask reports the group
	//! wiped, as the cleanup mechanism for an already-dead record.
	//!
	//! Composition is resolved by (factionKey, groupName) - never a faction index, which is
	//! positional across saves (D3).
	//! \param[in] ownerSystem Owning system tag, e.g. "deployment" (free-form string - the Economy
	//!            2.0 seam, §3.7).
	//! \param[in] ownerKey Consumer-defined identity used to reclaim the handle after a load.
	//! \param[in] factionKey Faction key, e.g. "USSR".
	//! \param[in] groupName Group registry name on that faction, e.g. "light_patrol".
	//! \param[in] position Where the group belongs (Phase 3 spawns the entity here).
	//! \param[in] plan Waypoint plan, or null for a group with no waypoints.
	//! \param[in] spawnDistanceOverride Metres, or -1 to use virtualizationSpawnDistance.
	//! \param[in] importance SCR_EAISpawnImportance, or -1 for Overthrow's default (NORMAL - D4).
	//! \return The handle, or -1 on failure (not server, not initialised, unresolvable composition,
	//!         ragged waypoint plan).
	int RegisterGroup(string ownerSystem, string ownerKey, string factionKey, string groupName,
	                  vector position, OVT_VirtualWaypointPlan plan = null,
	                  int spawnDistanceOverride = -1, int importance = -1)
	{
		if (!Replication.IsServer())
		{
			Print("[Overthrow] Virtualization: RegisterGroup called on a client - registration is server-only", LogLevel.WARNING);
			return -1;
		}

		if (!m_mRecords)
		{
			Print("[Overthrow] Virtualization: RegisterGroup called before the manager was initialised", LogLevel.WARNING);
			return -1;
		}

		OVT_OverthrowFactionManager factionManager = OVT_Global.GetFactions();
		if (!factionManager)
		{
			Print("[Overthrow] Virtualization: RegisterGroup found no faction manager", LogLevel.WARNING);
			return -1;
		}

		OVT_Faction faction = factionManager.GetOverthrowFactionByKey(factionKey);
		if (!faction)
		{
			Print(string.Format("[Overthrow] Virtualization: RegisterGroup could not resolve faction key '%1' (owner %2/%3)",
				factionKey, ownerSystem, ownerKey), LogLevel.WARNING);
			return -1;
		}

		ResourceName prefab = faction.GetGroupPrefabByName(groupName);
		if (prefab == ResourceName.Empty)
		{
			Print(string.Format("[Overthrow] Virtualization: faction '%1' has no group registry entry named '%2' (owner %3/%4)",
				factionKey, groupName, ownerSystem, ownerKey), LogLevel.WARNING);
			return -1;
		}

		if (plan && !OVT_VirtualizationMath.ValidateWaypointPlan(plan.m_aPositions, plan.m_aTypes, plan.m_aParams))
		{
			Print(string.Format("[Overthrow] Virtualization: refusing registration for %1/%2 - the waypoint plan's parallel arrays do not line up",
				ownerSystem, ownerKey), LogLevel.WARNING);
			return -1;
		}

		// The record is filled in FIRST and then handed to the shared builder - the same one the
		// persistence restore uses (T5.2), so a group a consumer registered and a group core re-created
		// from a save are the same object, built the same way, in the same order.
		//
		// Nothing is booked and no handle is burned until the world actually holds the group: a record
		// whose entity could not be built would be persisted, restored and dropped again on every later
		// load.
		OVT_VirtualGroupRecord record = new OVT_VirtualGroupRecord();
		record.m_iHandle = m_iNextHandle;
		record.m_sOwnerSystem = ownerSystem;
		record.m_sOwnerKey = ownerKey;
		record.m_sFactionKey = factionKey;
		record.m_sGroupRegistryName = groupName;
		record.m_rResolvedPrefab = prefab;
		record.m_iSpawnDistanceOverride = spawnDistanceOverride;
		record.m_eImportance = OVT_VirtualizationMath.ResolveImportance(importance, m_iDefaultImportance);
		record.m_vPosition = position;
		record.m_Plan = plan;

		if (!BuildRegisteredGroup(record))
		{
			Print(string.Format("[Overthrow] Virtualization: could not spawn group prefab '%1' for %2/%3",
				prefab, ownerSystem, ownerKey), LogLevel.WARNING);
			return -1;
		}

		m_iNextHandle++;
		m_mRecords.Insert(record.m_iHandle, record);

		Print(string.Format("[Overthrow] Virtualization: registered %1/%2 as handle %3 (%4 '%5', %6 slots, spawn %7 m, importance %8) at %9",
			ownerSystem, ownerKey, record.m_iHandle.ToString(), factionKey, groupName,
			record.m_aSlotAlive.Count().ToString(), GetSpawnDistance(record.m_iHandle).ToString(),
			record.m_eImportance.ToString(), position.ToString()), LogLevel.VERBOSE);

		return record.m_iHandle;
	}

	//------------------------------------------------------------------------------------------------
	//! Materialises everything a record owns in the world and wires the record to it.
	//!
	//! THE ONE PLACE A VIRTUAL GROUP ENTITY IS BUILT. Shared by RegisterGroup and by the persistence
	//! restore (ApplyPersistedRecord, T5.2) - which is the whole reason it exists: under Route B a
	//! saved group is not restored by vanilla, it is RE-CREATED by this manager, and a re-created group
	//! that differed from a registered one in any stamp would be a silently different group.
	//!
	//! DELIBERATELY CONTAINS NO VALIDATION. Server guard, composition resolution and waypoint-plan
	//! integrity stay with the callers, because the two callers validate different things: RegisterGroup
	//! validates a consumer's ARGUMENTS and refuses outright, while the restore validates a PAYLOAD and
	//! has fallbacks (a stored prefab when the registry entry is gone, a dropped plan when the stored
	//! arrays are ragged). Folding those together would give one caller the other's failure modes.
	//!
	//! The caller owns m_mRecords: this only inserts the group -> handle reverse entry, because that one
	//! belongs with the entity it describes.
	//! \param[in] record A record whose composition, position, plan and (optionally) mask are filled.
	//! \return True when the group entity exists and the record is wired to it.
	protected bool BuildRegisteredGroup(notnull OVT_VirtualGroupRecord record)
	{
		// Step 1-2: the entity, with ZERO members.
		SCR_AIGroup group = SpawnVirtualGroupEntity(record.m_rResolvedPrefab, record.m_vPosition);
		if (!group)
			return false;

		record.m_Group = group;
		record.m_GroupId = group.GetID();
		record.m_bDespawning = false;

		// Step 3: faction, then lifecycle policy + distances, then importance.
		Faction gameFaction = GetGame().GetFactionManager().GetFactionByKey(record.m_sFactionKey);
		if (gameFaction)
			group.SetFaction(gameFaction);

		ApplyLifecyclePolicy(record, group);

		// Step 4: waypoint entities from the plan, owned by the record (D6 - core never leaks one).
		BuildOwnedWaypoints(record, group, record.m_Plan);

		// Step 5: roster. A fresh registration captures the prefab's slot list all-alive; a restore
		// arrives with the saved mask already in the record and only has it resized against the roster.
		ReconcileSlotMask(record, group);
		PushSlotMask(record, group);

		// Step 6: persistence opt-in. The exemption armed around the spawn kept the group out of the
		// BUG-118 untrack; this makes the tracking explicit and defuses any queued release.
		OpenPersistenceTracking(group);

		// Step 7: the engine's own wipe-relevant signals.
		group.GetOnMembersDespawning().Insert(OnGroupMembersDespawning);
		group.GetOnEliminatedWhenReached().Insert(OnGroupEliminatedWhenReached);

		m_mHandleByGroup.Insert(record.m_GroupId, record.m_iHandle);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Brings a record's survivor mask into agreement with the roster its group prefab actually
	//! declares, IN PLACE.
	//!
	//! Two callers, two situations:
	//!   REGISTRATION - the mask is empty, the roster is unknown until the entity exists, and every
	//!                  slot starts alive. That is the D2 capture step.
	//!   RESTORE      - the mask came out of a save and the PREFAB may have changed under it (a faction
	//!                  mod updated its compositions between sessions). The overlap keeps its saved
	//!                  truth; a roster that GREW gains DEAD slots, never alive ones, because "dead
	//!                  members stay dead" (G3) must not be quietly inverted into "a group gets bigger
	//!                  when someone edits a prefab"; a roster that SHRANK loses its tail.
	//!
	//! ⚠ THE ARRAY IDENTITY IS PRESERVED ON PURPOSE. SetOVTSlotMask stores this exact array by
	//! reference on the modded group, so growing/shrinking it in place is what keeps the group and the
	//! record looking at one mask (Phase 3 handoff).
	//! \param[in] record The record whose mask is being sized.
	//! \param[in] group Its group entity, whose m_aUnitPrefabSlots is the roster.
	protected void ReconcileSlotMask(notnull OVT_VirtualGroupRecord record, notnull SCR_AIGroup group)
	{
		int rosterSize = 0;
		if (group.m_aUnitPrefabSlots)
			rosterSize = group.m_aUnitPrefabSlots.Count();

		if (rosterSize == 0)
		{
			Print(string.Format("[Overthrow] Virtualization: group prefab '%1' declares no member slots - handle %2 will never materialise anyone",
				record.m_rResolvedPrefab, record.m_iHandle.ToString()), LogLevel.WARNING);
		}

		if (!record.m_aSlotAlive)
			record.m_aSlotAlive = {};

		int stored = record.m_aSlotAlive.Count();
		if (stored == rosterSize)
			return;

		if (stored > 0)
		{
			Print(string.Format("[Overthrow] Virtualization: handle %1 was saved with %2 roster slots but '%3' now declares %4 - resizing the survivor mask",
				record.m_iHandle.ToString(), stored.ToString(), record.m_rResolvedPrefab, rosterSize.ToString()), LogLevel.WARNING);
		}

		int fill = 0;
		if (stored == 0)
			fill = 1;

		while (record.m_aSlotAlive.Count() > rosterSize)
		{
			record.m_aSlotAlive.Remove(record.m_aSlotAlive.Count() - 1);
		}

		while (record.m_aSlotAlive.Count() < rosterSize)
		{
			record.m_aSlotAlive.Insert(fill);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Creates the group entity with no members at all.
	//!
	//! Two statics are armed around the spawn and cleared immediately after, because both are one-shots
	//! consumed by the next SCR_AIGroup EOnInit and a failed (or non-group) spawn would otherwise leak
	//! them onto whatever spawns next - the same discipline vanilla's own SCR_DefenderSpawnerComponent
	//! uses (:545-549):
	//!   IgnoreSpawning       - vanilla's; makes EOnInit skip member spawning entirely.
	//!   ArmPersistenceExemption - Overthrow's; makes the modded EOnInit skip the BUG-118 untrack, so
	//!                          this one group stays persistence-tracked while every other AI group
	//!                          in the tree stays transient.
	//! \param[in] prefab Resolved group prefab.
	//! \param[in] position Where the group belongs.
	//! \return The group, or null when the prefab did not produce one.
	protected SCR_AIGroup SpawnVirtualGroupEntity(ResourceName prefab, vector position)
	{
		SCR_AIGroup.IgnoreSpawning(true);
		SCR_AIGroup.ArmPersistenceExemption(m_bPersistGroupEntities);

		IEntity entity = OVT_Global.SpawnEntityPrefab(prefab, position);

		SCR_AIGroup.IgnoreSpawning(false);
		SCR_AIGroup.ArmPersistenceExemption(false);

		if (!entity)
			return null;

		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if (!group)
		{
			Print(string.Format("[Overthrow] Virtualization: '%1' is not an SCR_AIGroup prefab", prefab), LogLevel.WARNING);
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
			return null;
		}

		// IgnoreSpawning failing would mean a group's worth of characters materialising at
		// registration time, far from any player - loud, because it is silent otherwise.
		if (group.GetAgentsCount() > 0)
		{
			Print(string.Format("[Overthrow] Virtualization: group '%1' spawned %2 member(s) despite IgnoreSpawning - despawning them",
				prefab, group.GetAgentsCount().ToString()), LogLevel.WARNING);
			group.DespawnMembers();
		}

		return group;
	}

	//------------------------------------------------------------------------------------------------
	//! Stamps (or re-stamps) the engine lifecycle policy, distances and importance from the record.
	//! Phase 5 calls this again on relink, which is why it is idempotent and reads everything it
	//! needs from the record.
	//!
	//! A resolved spawn distance of 0 is the documented "never materialise by proximity" registration
	//! (api.md §3). It is expressed as the Manual policy rather than as a 0 m ring, because
	//! SetLifecyclePolicy IGNORES non-positive distances (:2920-2925) - a ProximityDriven group
	//! stamped with 0 would silently keep vanilla's 600/800 m defaults and materialise anyway.
	//! \param[in] record The record to read.
	//! \param[in] group The group to stamp.
	protected void ApplyLifecyclePolicy(OVT_VirtualGroupRecord record, notnull SCR_AIGroup group)
	{
		int spawnDistance = OVT_VirtualizationMath.ResolveSpawnDistance(record.m_iSpawnDistanceOverride, GetGlobalSpawnDistance());

		if (spawnDistance <= 0)
		{
			group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
		}
		else
		{
			int despawnDistance = OVT_VirtualizationMath.ResolveDespawnDistance(spawnDistance, m_fDespawnHysteresis);
			group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.ProximityDriven, spawnDistance, despawnDistance, -1);
		}

		group.SetImportance(record.m_eImportance);
	}

	//------------------------------------------------------------------------------------------------
	//! Hands the record's survivor mask to the modded group (D2). Call this whenever the record's
	//! array IDENTITY changes - registration, and Phase 5's relink after a restore.
	//! \param[in] record The record owning the mask.
	//! \param[in] group The group to push it to.
	void PushSlotMask(OVT_VirtualGroupRecord record, SCR_AIGroup group)
	{
		if (!record || !group)
			return;

		group.SetOVTSlotMask(record.m_aSlotAlive);
	}

	//------------------------------------------------------------------------------------------------
	// TRACKED GROUPS - WAYPOINT OWNERSHIP (D6)
	//
	// A waypoint is an ordinary world entity and AddWaypoint() does NOT take ownership of it, so a
	// spawner that tears a group down and forgets its waypoints leaves them standing for the rest of
	// the session. Nearly every spawner in this tree has shipped that leak at some point - the
	// deployments framework's own group-cleanup helper detached waypoints without deleting them,
	// right up until the file was deleted outright in virtualization/integration Phase 5. Core
	// records every waypoint it creates and deletes it on unregister or wipe, and because waypoints
	// are persistence-tracked entities in 1.8, deletion untracks them too.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Turns a registration plan into real waypoint entities attached to the group.
	//!
	//! The plan's three arrays are parallel and were validated before the group was built, so this
	//! consumes them by index without re-checking. An empty or null plan is legal (a garrison with no
	//! waypoints is a normal registration).
	//! \param[in] record The record that will own the waypoints.
	//! \param[in] group The group to attach them to.
	//! \param[in] plan The registration plan, or null.
	protected void BuildOwnedWaypoints(OVT_VirtualGroupRecord record, notnull SCR_AIGroup group, OVT_VirtualWaypointPlan plan)
	{
		if (!plan || !plan.m_aPositions || plan.m_aPositions.IsEmpty())
			return;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			Print("[Overthrow] Virtualization: no config component - waypoint plan ignored", LogLevel.WARNING);
			return;
		}

		array<AIWaypoint> built = new array<AIWaypoint>();

		for (int i = 0; i < plan.m_aPositions.Count(); i++)
		{
			AIWaypoint waypoint = CreatePlannedWaypoint(config, plan.m_aTypes[i], plan.m_aPositions[i], plan.m_aParams[i]);
			if (!waypoint)
			{
				Print(string.Format("[Overthrow] Virtualization: waypoint %1 of the plan (type %2) could not be built",
					i.ToString(), plan.m_aTypes[i].ToString()), LogLevel.WARNING);
				continue;
			}

			record.m_aOwnedWaypoints.Insert(waypoint);
			built.Insert(waypoint);
			group.AddWaypoint(waypoint);
		}

		if (!plan.m_bCycle || built.Count() < 2)
			return;

		// The cycle waypoint replays the ones before it forever - vanilla's own idiom, copied from
		// OVT_OverthrowConfigComponent.GivePatrolWaypoints (:558-561).
		AIWaypointCycle cycle = AIWaypointCycle.Cast(config.SpawnBasicCycleWaypoint(group.GetOrigin()));
		if (!cycle)
		{
			Print("[Overthrow] Virtualization: cycle waypoint prefab did not produce an AIWaypointCycle", LogLevel.WARNING);
			return;
		}

		cycle.SetWaypoints(built);
		cycle.SetRerunCounter(-1);

		record.m_aOwnedWaypoints.Insert(cycle);
		group.AddWaypoint(cycle);
	}

	//! Completion radius of a planned SEARCH waypoint, in metres. The vanilla Search & Destroy
	//! activity lays its investigation grid over exactly this square around the point, so it is "how
	//! much of the ground around one building the men poke into" - one house and its yard, not the
	//! street behind it. The plan cannot carry it (one parameter per point, and that one is the hold),
	//! so it is core's constant rather than a consumer's attribute.
	static const float SEARCH_WAYPOINT_RADIUS_M = 15;

	//------------------------------------------------------------------------------------------------
	//! Builds one planned waypoint through the centralized helpers on the config component.
	//!
	//! ⚠ WAIT durations are not honoured by the engine-side helper: SpawnWaitWaypoint(pos, time)
	//! accepts a duration and never applies it (OVT_OverthrowConfigComponent.c:506). The parameter is
	//! passed through anyway so the call site is correct the day that is fixed.
	//! \param[in] config The config component holding the waypoint prefabs.
	//! \param[in] type OVT_EVirtualWaypointType.
	//! \param[in] position Where the waypoint goes.
	//! \param[in] waypointParam Per-type parameter (completion radius, wait seconds).
	//! \return The waypoint, or null when the type has no prefab.
	protected AIWaypoint CreatePlannedWaypoint(notnull OVT_OverthrowConfigComponent config, int type, vector position, float waypointParam)
	{
		// Play-test fix (2026-08-17): plans are authored in XZ - the Y is whatever the consumer had to hand (a town
		// anchor's, a flat offset's), so a waypoint spawned at it verbatim sits under a slope or in the
		// air, and a completion radius smaller than that Y error can never be reached by live AI. The
		// ENTITY is snapped to the surface; the plan payload is untouched (positions round-trip
		// verbatim, so no persistence claim moves).
		BaseWorld snapWorld = GetOwner().GetWorld();
		if (snapWorld)
			position[1] = snapWorld.GetSurfaceY(position[0], position[2]);

		AIWaypoint waypoint;

		switch (type)
		{
			case OVT_EVirtualWaypointType.MOVE:
				waypoint = config.SpawnMoveWaypoint(position);
				break;

			case OVT_EVirtualWaypointType.PATROL:
				waypoint = config.SpawnPatrolWaypoint(position);
				break;

			case OVT_EVirtualWaypointType.WAIT:
				waypoint = config.SpawnWaitWaypoint(position, waypointParam);
				break;

			case OVT_EVirtualWaypointType.DEFEND:
				waypoint = config.SpawnDefendWaypoint(position);
				break;

			case OVT_EVirtualWaypointType.CYCLE:
				waypoint = config.SpawnBasicCycleWaypoint(position);
				break;

			case OVT_EVirtualWaypointType.SEARCH:
				// The RELAXED house search (OVT_AIWaypoint_HouseSearch.et, see OVT_HouseSearchAI.c) - vanilla's
				// Search & Destroy with the threat posture removed; the helper falls back to vanilla S&D if
				// the game-mode prefab does not author it.
				waypoint = config.SpawnHouseSearchWaypoint(position);
				if (waypoint)
				{
					// The vanilla prefab authors a 30 m radius and a 600 s hold - an area sweep. A
					// SEARCH point is one building: the radius is pinned to the footprint and the hold
					// comes from the plan, like WAIT's. Both are set BEFORE the waypoint is added to the
					// group, because the activity reads the radius once, when it builds its grid.
					waypoint.SetCompletionRadius(SEARCH_WAYPOINT_RADIUS_M);

					SCR_TimedWaypoint timed = SCR_TimedWaypoint.Cast(waypoint);
					if (timed && waypointParam > 0)
						timed.SetHoldingTime(waypointParam);
				}
				break;
		}

		// WAIT's and SEARCH's parameter is a duration, not a radius - applying it as a completion
		// radius would give a 45-second wait a 45 m radius.
		if (waypoint && waypointParam > 0 && type != OVT_EVirtualWaypointType.WAIT && type != OVT_EVirtualWaypointType.SEARCH)
			waypoint.SetCompletionRadius(waypointParam);

		return waypoint;
	}

	//------------------------------------------------------------------------------------------------
	//! Detaches, untracks and deletes every waypoint the record owns (D6). Idempotent.
	//! \param[in] record The record to strip.
	//! \param[in] group Its group, or null when the entity is already gone.
	protected void DeleteOwnedWaypoints(OVT_VirtualGroupRecord record, SCR_AIGroup group)
	{
		if (!record || !record.m_aOwnedWaypoints)
			return;

		foreach (AIWaypoint waypoint : record.m_aOwnedWaypoints)
		{
			if (!waypoint)
				continue;

			if (group)
				group.RemoveWaypoint(waypoint);

			// Waypoints are persistence-tracked entities in 1.8, so a deletion that does not untrack
			// leaves a record nothing will ever claim (the BUG-118 shape, one layer down).
			OVT_PersistenceManagerComponent.CancelUntrackTransient(waypoint);
			OVT_PersistenceTracking.Untrack(waypoint, false);

			SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
		}

		record.m_aOwnedWaypoints.Clear();
	}

	//------------------------------------------------------------------------------------------------
	// TRACKED GROUPS - PERSISTENCE OPT-IN (T3.1 step 6, the Phase 1 T1.3 verdict)
	//
	// THE PROBLEM. Overthrow untracks EVERY AI group unconditionally (Modded/SCR_AIGroup.EOnInit, the
	// BUG-118 fix), so a registered group is invisible to persistence - runtime-confirmed in the Phase
	// 1 spike (IsTracked=no, persistentId='' both at spawn and after the untrack retry queue drained).
	//
	// WHAT IS BUILT. A per-group exemption from that untrack, armed as a one-shot around the spawn
	// (SpawnVirtualGroupEntity) and around the persistence system's own restore spawn
	// (Modded/SCR_AIGroupSerializer), plus the explicit pair below - which also survives the FAST path
	// of UntrackTransient, where an entity already tracked at EOnInit is released immediately with no
	// queue entry left to cancel.
	//
	// WHY IT IS OFF BY DEFAULT (m_bPersistGroupEntities). Tracking alone is not enough: the record
	// also has to be able to re-instantiate the group on load, and `SelfSpawn` can only be granted by
	// a .conf rule matched with ENGINE-NATIVE matchers (entity class / prefab / component class -
	// scripted IsMatch rules are never consulted, BUG-018). None of them can name "this runtime
	// instance", and the narrowest available - EntityClass "AIGroup" - is a superset that also catches
	// every AI group whose untrack the retry queue GAVE UP on (60 s ceiling,
	// OVT_PersistenceManagerComponent.TRANSIENT_UNTRACK_MAX_ATTEMPTS, whose own comment records that
	// such an entity "will produce a record the next save"). Granting SelfSpawn class-wide would turn
	// those harmless orphan records into duplicated garrisons and patrols on every load. The
	// per-instance alternative, MarkForSelfSpawn/SetConfig, is banned outright (save-corrupting,
	// BUG-116).
	//
	// ⚠ PHASE 5 SETTLED THIS AND WENT THE OTHER WAY (Route B - see "REGISTRY PERSISTENCE" below).
	// Core persists complete re-creation state and rebuilds its own group entities on load, so no
	// vanilla AIGroup record is needed or wanted and m_bPersistGroupEntities stays FALSE. Everything in
	// this section is a dormant seam, kept for the day something else grants an AI group SelfSpawn;
	// switching it on today only writes orphan records, because the registry restore re-creates the
	// group either way.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Makes a registered group's entity persistence-tracked, and keeps it that way.
	//! \param[in] group The group entity.
	protected void OpenPersistenceTracking(notnull SCR_AIGroup group)
	{
		if (!m_bPersistGroupEntities)
			return;

		// Defuse a queued release first: if the exemption did not reach EOnInit for any reason, the
		// group is sitting in the transient-untrack retry queue and would be released a second later
		// (BUG-118's IsTracked retry queue).
		OVT_PersistenceManagerComponent.CancelUntrackTransient(group);

		OVT_PersistenceTracking.Track(group);
	}

	//! \return True when registered group entities are being kept persistence-tracked.
	bool ArePersistedGroupEntities()
	{
		return m_bPersistGroupEntities;
	}

	//------------------------------------------------------------------------------------------------
	//! Releases a group's persistence record when core is done with it - a wiped or unregistered
	//! group must not leave a record behind, or the next load restores a group nothing owns.
	//! \param[in] group The group entity.
	protected void ClosePersistenceTracking(SCR_AIGroup group)
	{
		if (!group)
			return;

		// Cancel first: a pending queue entry would fire on an entity that no longer exists.
		OVT_PersistenceManagerComponent.CancelUntrackTransient(group);
		OVT_PersistenceTracking.Untrack(group, false);
	}

	//------------------------------------------------------------------------------------------------
	// REGISTRY PERSISTENCE - ROUTE B: CORE RE-CREATES ITS OWN GROUPS (Phase 5, T5.1-T5.3)
	//
	// THE RULING. Phase 1 and Phase 3 killed the original design (§3.6: vanilla's SCR_AIGroupSerializer
	// persists the group entities, core relinks them by UUID). There is no safe way to grant a
	// runtime-spawned group SelfSpawn: rules are matched with engine-native matchers only (scripted
	// IsMatch is never consulted - BUG-018), the narrowest available matcher is EntityClass "AIGroup" -
	// a SUPERSET that also catches every AI group whose untrack the retry queue gave up on, so
	// class-wide SelfSpawn would duplicate garrisons and patrols on every load - and the per-instance
	// alternative MarkForSelfSpawn is save-corrupting (BUG-116).
	//
	// SO CORE PERSISTS COMPLETE RE-CREATION STATE AND REBUILDS THE GROUPS ITSELF. The payload carries
	// composition, position, importance, spawn-distance override, the survivor mask and the waypoint
	// plan, and ApplyPersistedRegistry below runs the SAME builder registration runs. Consequences,
	// stated so nobody has to re-derive them:
	//   - No vanilla AIGroup record is written. m_bPersistGroupEntities stays FALSE and the Phase 3
	//     exemption seam stays dormant. Turning it on under Route B only produces orphan records.
	//   - There is no UUID and no WhenAvailable relink. GetOnRecordsRestored() is a latch over THIS
	//     manager's own work, not over vanilla's.
	//   - Member characters and waypoint entities are never persisted. The mask is the roster truth and
	//     the plan is the waypoint truth; both are rebuilt from the payload.
	//   - A record whose mask is all-dead is NEVER re-created. Wiped stays wiped (G3/F5).
	//
	// AMBIENT SOURCES ARE ABSENT BY CONSTRUCTION (T5.5): everything below reads and writes m_mRecords
	// and m_iNextHandle only. The ambient collections and their separate handle counter are not named
	// anywhere in this section or in the serializer.
	//------------------------------------------------------------------------------------------------

	//! Frame-deferred one-shot: true between the end of a restore pass and the invoker actually firing.
	protected bool m_bRecordsRestoredPending;

	//! How many times the deferred announcement has waited for the campaign to finish starting.
	protected int m_iRecordsRestoredWaits;

	//! Ceiling on that wait (~10 s at 250 ms). A diagnostic bound, not a retry: expiry fires the
	//! invoker anyway, because a consumer that never hears "records are back" is worse than one that
	//! hears it early.
	protected static const int RECORDS_RESTORED_MAX_WAITS = 40;

	//------------------------------------------------------------------------------------------------
	//! Flattens the registry into the persistence payload (T5.1). The serializer's ONLY read of this
	//! manager, which is what keeps it a pure codec.
	//!
	//! POSITION COMES FROM THE LIVE GROUP, not from the record's registration position: `movement` will
	//! advance dormant group entities, so the registration position is stale the moment anything moves.
	//! GetPosition() reads the entity origin and falls back to the record only when the entity is gone.
	//! \param[out] records Receives one entry per restorable record.
	void SnapshotRegistry(notnull array<ref OVT_PersistedVirtualGroup> records)
	{
		if (!m_mRecords)
			return;

		foreach (int handle, OVT_VirtualGroupRecord record : m_mRecords)
		{
			if (!record)
				continue;

			// A wiped record is removed at wipe time, so nothing all-dead should be in the registry at
			// all - but writing one would produce a saved group that must never come back, so the
			// invariant is enforced here as well as where it is maintained.
			if (OVT_VirtualizationMath.IsWiped(record.m_aSlotAlive))
				continue;

			OVT_PersistedVirtualGroup entry = new OVT_PersistedVirtualGroup();
			entry.handle = handle;
			entry.ownerSystem = record.m_sOwnerSystem;
			entry.ownerKey = record.m_sOwnerKey;
			entry.factionKey = record.m_sFactionKey;
			entry.groupRegistryName = record.m_sGroupRegistryName;
			entry.resolvedPrefab = record.m_rResolvedPrefab;
			entry.spawnDistanceOverride = record.m_iSpawnDistanceOverride;
			entry.importance = record.m_eImportance;
			entry.position = GetPosition(handle);

			if (record.m_aSlotAlive)
			{
				foreach (int alive : record.m_aSlotAlive)
				{
					entry.slotAlive.Insert(alive);
				}
			}

			WritePersistedPlan(record.m_Plan, entry);

			records.Insert(entry);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return The next handle the registry would hand out. Persisted so a handle is NEVER reused
	//!         across a reload - a consumer holding a stale handle must get "unknown", never someone
	//!         else's group. Deliberately NOT the ambient counter, which is never persisted.
	int GetNextHandle()
	{
		return m_iNextHandle;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies a persisted registry payload: re-creates what it describes, drops what it does not
	//! (T5.2). The write half of Route B, and the serializer's only write into this manager.
	//!
	//! IDEMPOTENT ON A LIVE SESSION, by the same contract as ApplyPersistedDeployment: records are
	//! matched BY HANDLE, an existing one is updated in place, and a live record the payload does not
	//! claim is unregistered. A second application of the same payload therefore produces exactly the
	//! same registry - which matters, because saved data is also re-applied to an already-running
	//! campaign (OVT_PersistenceManagerComponent.ReapplyLatestSaveData).
	//!
	//! SYNCHRONOUS ON PURPOSE. Every dependency it needs exists by the time a component serializer runs
	//! (AFTER_ENTITY_FINALIZE): a world to spawn into, and - for the group entity itself - nothing else,
	//! because the payload carries the resolved prefab. So records and their group entities are queryable
	//! the moment Deserialize returns, and no consumer can observe a half-restored registry. Only the
	//! ANNOUNCEMENT is deferred; see ScheduleRecordsRestored().
	//! \param[in] records The payload, oldest handle order irrelevant; may be null or empty.
	//! \param[in] nextHandle The handle counter as it stood when the save was written.
	void ApplyPersistedRegistry(array<ref OVT_PersistedVirtualGroup> records, int nextHandle)
	{
		if (!Replication.IsServer())
			return;

		if (!m_mRecords)
		{
			Print("[Overthrow] Virtualization: a saved registry arrived before the manager was initialised - it cannot be restored", LogLevel.WARNING);
			return;
		}

		// Every handle the payload claims, so a live record can be told apart from a stale one without
		// a nested scan.
		set<int> claimed = new set<int>();
		int highestHandle = -1;

		if (records)
		{
			foreach (OVT_PersistedVirtualGroup entry : records)
			{
				if (!entry)
					continue;

				claimed.Insert(entry.handle);

				if (entry.handle > highestHandle)
					highestHandle = entry.handle;
			}
		}

		// Anything the payload does not claim is gone: a group registered after the save was taken, or
		// one this session invented. UnregisterGroup is the right removal because it also takes the
		// entity and its waypoints - a dropped record that left a live group behind would leave a
		// garrison in the world that nothing owns.
		array<int> stale = new array<int>();
		foreach (int liveHandle, OVT_VirtualGroupRecord live : m_mRecords)
		{
			if (!claimed.Contains(liveHandle))
				stale.Insert(liveHandle);
		}

		foreach (int staleHandle : stale)
		{
			UnregisterGroup(staleHandle);
		}

		int restored = 0;
		int dropped = 0;

		if (records)
		{
			foreach (OVT_PersistedVirtualGroup entry : records)
			{
				if (!entry)
					continue;

				if (ApplyPersistedRecord(entry))
					restored++;
				else
					dropped++;
			}
		}

		RestoreHandleCounter(nextHandle, highestHandle);

		Print(string.Format("[Overthrow] Virtualization: restored %1 virtual group(s), dropped %2, removed %3 not in the save; next handle %4",
			restored.ToString(), dropped.ToString(), stale.Count().ToString(), m_iNextHandle.ToString()), LogLevel.VERBOSE);

		ScheduleRecordsRestored();
	}

	//------------------------------------------------------------------------------------------------
	//! Restores one saved record, re-creating its group entity when there is none.
	//! \param[in] entry One payload entry.
	//! \return True when the record is live afterwards; false when it was deliberately dropped.
	protected bool ApplyPersistedRecord(notnull OVT_PersistedVirtualGroup entry)
	{
		// WIPED STAYS WIPED (G3/F5). A save should never contain an all-dead record - the registry drops
		// them at wipe time and SnapshotRegistry refuses to write one - so this is the belt on the
		// braces, and it is the one that matters: re-creating it would resurrect a group the player
		// already destroyed.
		if (OVT_VirtualizationMath.IsWiped(entry.slotAlive))
		{
			Print(string.Format("[Overthrow] Virtualization: saved handle %1 (%2/%3) has no living slots - it stays wiped",
				entry.handle.ToString(), entry.ownerSystem, entry.ownerKey), LogLevel.VERBOSE);
			return false;
		}

		ResourceName prefab = ResolvePersistedComposition(entry);
		if (prefab == ResourceName.Empty)
			return false;

		OVT_VirtualGroupRecord record = m_mRecords.Get(entry.handle);
		bool isNew = !record;

		if (isNew)
		{
			record = new OVT_VirtualGroupRecord();
			record.m_iHandle = entry.handle;
		}

		record.m_sOwnerSystem = entry.ownerSystem;
		record.m_sOwnerKey = entry.ownerKey;
		record.m_sFactionKey = entry.factionKey;
		record.m_sGroupRegistryName = entry.groupRegistryName;
		record.m_rResolvedPrefab = prefab;
		record.m_iSpawnDistanceOverride = entry.spawnDistanceOverride;
		record.m_eImportance = OVT_VirtualizationMath.ResolveImportance(entry.importance, m_iDefaultImportance);
		record.m_vPosition = entry.position;
		record.m_Plan = ReadPersistedPlan(entry);

		ApplyPersistedMask(record, entry.slotAlive);

		if (isNew)
			m_mRecords.Insert(record.m_iHandle, record);

		SCR_AIGroup group = ResolveGroup(record);
		if (!group)
		{
			// The entity is gone (a fresh session, or a record whose group was deleted since the save):
			// re-create it. Everything that described the OLD entity goes first:
			//   - its waypoint refs, because BuildOwnedWaypoints appends rather than replaces;
			//   - its member -> slot entries, so a kill event can never resolve to the new group;
			//   - its group -> handle entry, because EntityIDs are recycled and a stale one would
			//     answer for whatever the engine hands out next (the Phase 4 release-path hazard).
			DeleteOwnedWaypoints(record, null);
			ForgetMembers(record);
			m_mHandleByGroup.Remove(record.m_GroupId);

			if (!BuildRegisteredGroup(record))
			{
				Print(string.Format("[Overthrow] Virtualization: could not re-create saved handle %1 (%2/%3) from prefab '%4' - dropping the record",
					entry.handle.ToString(), entry.ownerSystem, entry.ownerKey, prefab), LogLevel.WARNING);

				m_mRecords.Remove(record.m_iHandle);
				return false;
			}

			group = ResolveGroup(record);
		}
		else
		{
			// Already live: put the saved state back onto the entity that exists rather than building a
			// second one. This is the re-apply-to-a-running-campaign path.
			group.SetOrigin(entry.position);
			ApplyLifecyclePolicy(record, group);
			ReconcileSlotMask(record, group);
			PushSlotMask(record, group);
		}

		AssertDormantCountsFromMask(record, group);

		// A prefab that SHRANK can truncate the last living slot out of the mask (ReconcileSlotMask).
		// The result would be a record that can never materialise anyone, so it is treated as the wipe
		// it now is rather than kept as an immortal husk.
		if (OVT_VirtualizationMath.IsWiped(record.m_aSlotAlive))
		{
			Print(string.Format("[Overthrow] Virtualization: saved handle %1 (%2/%3) has no living slot left after resizing against '%4' - dropping the record",
				entry.handle.ToString(), entry.ownerSystem, entry.ownerKey, prefab), LogLevel.WARNING);

			UnregisterGroup(record.m_iHandle);
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Three-step composition resolution for a saved record (D3 / R3):
	//!   1. faction KEY -> Overthrow faction (never a faction index, which is positional across saves),
	//!   2. group registry NAME on that faction -> prefab,
	//!   3. the prefab that was resolved when the group was registered, stored for exactly this case.
	//! All three failing is a record whose composition no longer exists in this installation - a faction
	//! mod removed, a registry entry deleted - and it is DROPPED with a warning naming the key, never
	//! resurrected as something else.
	//! \param[in] entry One payload entry.
	//! \return The prefab to build, or ResourceName.Empty to drop the record.
	protected ResourceName ResolvePersistedComposition(notnull OVT_PersistedVirtualGroup entry)
	{
		OVT_OverthrowFactionManager factionManager = OVT_Global.GetFactions();
		if (factionManager)
		{
			OVT_Faction faction = factionManager.GetOverthrowFactionByKey(entry.factionKey);
			if (faction)
			{
				ResourceName prefab = faction.GetGroupPrefabByName(entry.groupRegistryName);
				if (prefab != ResourceName.Empty)
					return prefab;
			}
		}

		if (entry.resolvedPrefab != ResourceName.Empty)
		{
			Print(string.Format("[Overthrow] Virtualization: faction key '%1' / group '%2' no longer resolve for saved handle %3 - falling back to the stored prefab '%4'",
				entry.factionKey, entry.groupRegistryName, entry.handle.ToString(), entry.resolvedPrefab), LogLevel.WARNING);

			return entry.resolvedPrefab;
		}

		Print(string.Format("[Overthrow] Virtualization: dropping saved handle %1 (%2/%3) - faction key '%4' and group '%5' no longer resolve and no prefab was stored",
			entry.handle.ToString(), entry.ownerSystem, entry.ownerKey, entry.factionKey, entry.groupRegistryName), LogLevel.WARNING);

		return ResourceName.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Refills a record's survivor mask from a payload, IN PLACE.
	//!
	//! ⚠ THE ARRAY IDENTITY MUST NOT CHANGE. The modded group holds this exact array by reference
	//! (SetOVTSlotMask), so assigning a NEW array here would leave the group reading an orphaned copy
	//! that no death ever writes to again - the group would refill dead slots forever. Callers still
	//! call PushSlotMask afterwards, because a group core has just re-created has no mask at all yet.
	//! \param[in] record The record to write into.
	//! \param[in] stored The saved mask; may be null.
	protected void ApplyPersistedMask(notnull OVT_VirtualGroupRecord record, array<int> stored)
	{
		if (!record.m_aSlotAlive)
			record.m_aSlotAlive = {};

		record.m_aSlotAlive.Clear();

		if (!stored)
			return;

		foreach (int alive : stored)
		{
			record.m_aSlotAlive.Insert(alive);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the engine's dormant alive/dead counts from the survivor mask onto a memberless group.
	//!
	//! WHY THIS IS NOT ReassertOVTDormantCounts(). That one deliberately no-ops while the engine's
	//! count is the -1 "never despawned" sentinel, because writing counts onto a group whose members are
	//! alive would flip IsDormant() under them. A restored group is the case where that reasoning
	//! inverts: it has no members and never had any this session, and until the counts are written the
	//! engine treats it as a first activation - RequestSpawn asks the queue for ONE member
	//! (SCR_AIGroup.c:2681-2691) and vanilla's refill capacity (totalSlots - dormantDead, :2726-2729)
	//! describes a full roster instead of the survivors. Writing them makes a partially-wiped group ask
	//! for exactly its survivors the moment an observer arrives.
	//! \param[in] record The restored record.
	//! \param[in] group Its group entity, or null.
	protected void AssertDormantCountsFromMask(notnull OVT_VirtualGroupRecord record, SCR_AIGroup group)
	{
		if (!group)
			return;

		if (!record.m_aSlotAlive || record.m_aSlotAlive.IsEmpty())
			return;

		// Never onto a group whose members exist - that is the case the sentinel protects.
		if (group.GetAgentsCount() > 0)
			return;

		int alive = OVT_VirtualizationMath.CountAlive(record.m_aSlotAlive);
		group.SetDormantCounts(alive, record.m_aSlotAlive.Count() - alive);
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the handle counter back where the save left it, so handles are never reused across a load.
	//!
	//! Clamped up to the payload's own highest handle + 1: a payload whose counter disagrees with its
	//! records (an older format, a hand-edited save) must not hand the next registration a handle one of
	//! its own restored records is already using.
	//! \param[in] storedNext The counter as saved.
	//! \param[in] highestHandle Highest handle in the payload, or -1 when it was empty.
	protected void RestoreHandleCounter(int storedNext, int highestHandle)
	{
		int next = storedNext;

		if (next <= highestHandle)
			next = highestHandle + 1;

		if (next < 0)
			next = 0;

		m_iNextHandle = next;
	}

	//------------------------------------------------------------------------------------------------
	//! Flattens a waypoint plan into the payload's parallel arrays. An absent or ragged plan writes
	//! nothing, which restores as "a group with no waypoints" - a legitimate registration.
	//! \param[in] plan The record's plan, or null.
	//! \param[out] entry The payload entry to fill.
	protected void WritePersistedPlan(OVT_VirtualWaypointPlan plan, notnull OVT_PersistedVirtualGroup entry)
	{
		if (!plan)
			return;

		if (!OVT_VirtualizationMath.ValidateWaypointPlan(plan.m_aPositions, plan.m_aTypes, plan.m_aParams))
			return;

		foreach (vector position : plan.m_aPositions)
		{
			entry.waypointPositions.Insert(position);
		}

		foreach (int type : plan.m_aTypes)
		{
			entry.waypointTypes.Insert(type);
		}

		foreach (float waypointParam : plan.m_aParams)
		{
			entry.waypointParams.Insert(waypointParam);
		}

		entry.waypointCycle = plan.m_bCycle;
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds a waypoint plan from the payload's parallel arrays.
	//!
	//! A RAGGED payload is refused rather than half-built, exactly as a ragged registration argument is:
	//! the three arrays are consumed by index, so a short params array would give the last waypoint
	//! someone else's radius. The group still restores - without waypoints, which is a garrison rather
	//! than a corrupt patrol.
	//! \param[in] entry One payload entry.
	//! \return The plan, or null when there is none / it does not line up.
	protected OVT_VirtualWaypointPlan ReadPersistedPlan(notnull OVT_PersistedVirtualGroup entry)
	{
		if (!entry.waypointPositions || entry.waypointPositions.IsEmpty())
			return null;

		if (!OVT_VirtualizationMath.ValidateWaypointPlan(entry.waypointPositions, entry.waypointTypes, entry.waypointParams))
		{
			Print(string.Format("[Overthrow] Virtualization: saved handle %1 has a waypoint plan whose parallel arrays do not line up - restoring the group without waypoints",
				entry.handle.ToString()), LogLevel.WARNING);

			return null;
		}

		OVT_VirtualWaypointPlan plan = new OVT_VirtualWaypointPlan();

		foreach (vector position : entry.waypointPositions)
		{
			plan.m_aPositions.Insert(position);
		}

		foreach (int type : entry.waypointTypes)
		{
			plan.m_aTypes.Insert(type);
		}

		foreach (float waypointParam : entry.waypointParams)
		{
			plan.m_aParams.Insert(waypointParam);
		}

		plan.m_bCycle = entry.waypointCycle;

		return plan;
	}

	//------------------------------------------------------------------------------------------------
	//! Arms the one-shot announcement that ends a restore (T5.3).
	//!
	//! WHY IT IS DEFERRED AT ALL, when the restore itself is synchronous. Serializer ORDER is not a
	//! contract (D9): a consumer subscribed to GetOnRecordsRestored() may not have deserialized its own
	//! state yet when this manager deserializes, and reclaiming into a half-loaded consumer is exactly
	//! the ordering bug the invoker exists to remove. One call-queue hop puts every subscriber safely
	//! after the whole deserialize pass.
	//!
	//! WHY IT ALSO WAITS FOR THE CAMPAIGN. On a continued campaign the game mode's own serializer
	//! schedules DoStartGame() from the same pass, so PostGameStart() has not run yet. Consumers
	//! register their groups in PostGameStart, so firing before it would tell them to reclaim records
	//! for a system that has not started - the wait makes "after PostGameStart()" the contract on every
	//! path, including the in-session re-apply (where the campaign is already started and the wait is a
	//! single frame).
	protected void ScheduleRecordsRestored()
	{
		m_bRecordsRestoredPending = true;
		m_iRecordsRestoredWaits = 0;

		GetGame().GetCallqueue().Remove(CompleteRecordsRestore);
		GetGame().GetCallqueue().CallLater(CompleteRecordsRestore, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Fires GetOnRecordsRestored() exactly once per restore, once the campaign is running.
	protected void CompleteRecordsRestore()
	{
		if (!m_bRecordsRestoredPending)
			return;

		// A latch belonging to a PREVIOUS campaign must never announce into this one (R7). OnDelete
		// removes the queue entry, but a teardown that never ran it would otherwise leave this firing
		// after the next world claimed the static.
		if (s_Instance != this)
		{
			m_bRecordsRestoredPending = false;
			return;
		}

		OVT_OverthrowGameMode gameMode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if (gameMode && !gameMode.HasGameStarted() && m_iRecordsRestoredWaits < RECORDS_RESTORED_MAX_WAITS)
		{
			m_iRecordsRestoredWaits++;
			GetGame().GetCallqueue().CallLater(CompleteRecordsRestore, 250, false);
			return;
		}

		m_bRecordsRestoredPending = false;
		m_iRecordsRestoredWaits = 0;

		Print(string.Format("[Overthrow] Virtualization: %1 restored record(s) are queryable - announcing", GetGroupCount().ToString()), LogLevel.VERBOSE);

		if (m_OnRecordsRestored)
			m_OnRecordsRestored.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes a record and everything core created for it (T3.7).
	//!
	//! HELD MEMBERS. When a member is held by another system (in a vehicle, player-engaged - the
	//! engine's HasHeldMember rule), core does NOT rip the characters out from under it. Instead the
	//! group is retired in place: mask cleared so it can never refill, dormant counts zeroed so the
	//! engine refuses to respawn it forever, eliminate-when-reached enabled so the husk is cleaned up
	//! when someone reaches it, and the engine's own lifecycle tick despawns the live members when
	//! the last observer leaves. The record is gone either way - the consumer asked for that.
	//!
	//! Idempotent.
	//! \param[in] handle Handle from RegisterGroup.
	//! \return True when a record was removed; false when the handle is unknown.
	bool UnregisterGroup(int handle)
	{
		if (!m_mRecords)
			return false;

		OVT_VirtualGroupRecord record = m_mRecords.Get(handle);
		if (!record)
			return false;

		SCR_AIGroup group = ResolveGroup(record);

		ForgetMembers(record);
		DeleteOwnedWaypoints(record, group);
		ClosePersistenceTracking(group);

		// Unconditionally, because a record whose entity the engine already deleted still owns its
		// reverse-map entry - and a stale group->handle entry would answer for whatever entity id the
		// engine hands out next.
		m_mHandleByGroup.Remove(record.m_GroupId);

		if (group)
		{
			group.GetOnMembersDespawning().Remove(OnGroupMembersDespawning);
			group.GetOnEliminatedWhenReached().Remove(OnGroupEliminatedWhenReached);

			if (group.GetAgentsCount() > 0 && group.HasHeldMember())
			{
				Print(string.Format("[Overthrow] Virtualization: handle %1 has a held member - retiring the group in place, the engine despawns it when its observers leave",
					handle.ToString()), LogLevel.VERBOSE);
				RetireGroupEntity(group);
			}
			else
			{
				if (group.GetAgentsCount() > 0)
					group.DespawnMembers();

				group.ClearOVTSlotMask();
				SCR_EntityHelper.DeleteEntityAndChildren(group);
			}
		}

		record.m_Group = null;
		m_mRecords.Remove(handle);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a group beyond use without deleting it: no mask (so the refill seam cannot pick a slot),
	//! dormant alive 0 (so RequestSpawn refuses forever - SCR_AIGroup.c:2680-2688) and
	//! eliminate-when-reached (so the engine deletes the husk when an observer arrives).
	//!
	//! This is the ONE place SetEliminateWhenReached is stamped, and only ever on a group that is
	//! already dead to core - never at registration (Phase 1 amendment 3).
	//! \param[in] group The group to retire.
	protected void RetireGroupEntity(notnull SCR_AIGroup group)
	{
		int rosterSize = 0;
		if (group.m_aUnitPrefabSlots)
			rosterSize = group.m_aUnitPrefabSlots.Count();

		group.ClearOVTSlotMask();
		group.SetDormantCounts(0, rosterSize);
		group.SetEliminateWhenReached(true);
	}

	//------------------------------------------------------------------------------------------------
	// TRACKED GROUPS - QUERIES
	//------------------------------------------------------------------------------------------------

	//! \param[in] handle Handle from RegisterGroup.
	//! \return True when the handle names a live record.
	bool IsRegistered(int handle)
	{
		if (!m_mRecords)
			return false;

		return m_mRecords.Contains(handle);
	}

	//! \return Number of registered groups; 0 on a client or before initialisation.
	int GetGroupCount()
	{
		if (!m_mRecords)
			return 0;

		return m_mRecords.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side read access to a record. Callers must not mutate what they get back - the setters
	//! on this manager exist so that engine state and the record stay in step.
	//! \param[in] handle Handle from RegisterGroup.
	//! \return The record, or null when the handle is unknown.
	OVT_VirtualGroupRecord GetRecord(int handle)
	{
		if (!m_mRecords)
			return null;

		return m_mRecords.Get(handle);
	}

	//------------------------------------------------------------------------------------------------
	//! The reclaim seam (R3). After a load a consumer calls this with the SAME (ownerSystem,
	//! ownerKey) it registered with and gets its handles back without ever having persisted them.
	//! Call it from GetOnRecordsRestored(), never from your own deserialize (D9).
	//! \param[in] ownerSystem Owning system tag used at registration.
	//! \param[in] ownerKey Owner key used at registration.
	//! \return Matching handles, newest last; an empty array when there are none.
	array<int> FindGroupsByOwner(string ownerSystem, string ownerKey)
	{
		array<int> handles = new array<int>();
		if (!m_mRecords)
			return handles;

		foreach (int handle, OVT_VirtualGroupRecord record : m_mRecords)
		{
			if (record.m_sOwnerSystem == ownerSystem && record.m_sOwnerKey == ownerKey)
				handles.Insert(handle);
		}

		return handles;
	}

	//------------------------------------------------------------------------------------------------
	//! Every handle owned by one system, whatever its owner key.
	//! \param[in] ownerSystem Owning system tag used at registration.
	//! \return Matching handles; an empty array when there are none.
	array<int> FindGroupsBySystem(string ownerSystem)
	{
		array<int> handles = new array<int>();
		if (!m_mRecords)
			return handles;

		foreach (int handle, OVT_VirtualGroupRecord record : m_mRecords)
		{
			if (record.m_sOwnerSystem == ownerSystem)
				handles.Insert(handle);
		}

		return handles;
	}

	//------------------------------------------------------------------------------------------------
	//! Every registered handle, whoever owns it. Added 2026-08-17 for `virtualization/movement`, which
	//! advances EVERY registered group: owner systems are free-form strings, so FindGroupsBySystem
	//! cannot enumerate them.
	//!
	//! The order is the registry map's own and is NOT stable - a caller that needs a stable order (a
	//! round-robin, for instance) must sort what it gets back. Server-only like every other entry
	//! point here: a client, or a call before initialisation, gets an empty array.
	//! \return Every live handle; an empty array when there are none.
	array<int> GetAllHandles()
	{
		array<int> handles = new array<int>();
		if (!m_mRecords)
			return handles;

		foreach (int handle, OVT_VirtualGroupRecord record : m_mRecords)
		{
			handles.Insert(handle);
		}

		return handles;
	}

	//------------------------------------------------------------------------------------------------
	//! Which plan waypoint is the group's LIVE current target? (additive 2026-08-17, play-test fix)
	//!
	//! Position-only projection cannot recover DIRECTION on a route whose legs coincide (a two-point
	//! cycling patrol walks the same line both ways), so a group that despawned walking home would be
	//! resumed walking out again. The group entity knows the truth: its current waypoint. This matches
	//! that entity against the record's owned, plan-built waypoints by identity and answers the plan
	//! index. The cycle entity (appended last when the plan cycles) answers 0 - a group running it is
	//! wrapping back to the first point.
	//!
	//! -1 when it cannot answer truthfully: no record, no group entity, no current waypoint, or an
	//! owned-waypoint list that does not match the plan 1:1 (a plan point that failed to build shifts
	//! every index after it - refuse rather than guess). Callers fall back to their own heuristic
	//! (movement: polyline projection).
	//! \param[in] handle Handle from RegisterGroup.
	//! \return Plan index of the group's current waypoint, or -1 when unknowable.
	int GetCurrentPlanIndex(int handle)
	{
		OVT_VirtualGroupRecord record = GetRecord(handle);
		if (!record || !record.m_Plan || !record.m_Plan.m_aPositions)
			return -1;

		SCR_AIGroup group = ResolveGroup(record);
		if (!group)
			return -1;

		array<AIWaypoint> ownedWaypoints = record.m_aOwnedWaypoints;
		if (!ownedWaypoints || ownedWaypoints.IsEmpty())
			return -1;

		int planCount = record.m_Plan.m_aPositions.Count();
		bool hasCycleEntity = record.m_Plan.m_bCycle && planCount >= 2;
		int expected = planCount;
		if (hasCycleEntity)
			expected = planCount + 1;

		if (ownedWaypoints.Count() != expected)
			return -1;

		AIWaypoint current = group.GetCurrentWaypoint();
		if (!current)
			return -1;

		int index = ownedWaypoints.Find(current);
		if (index == -1)
			return -1;

		if (hasCycleEntity && index == planCount)
			return 0;

		return index;
	}

	//------------------------------------------------------------------------------------------------
	// TRACKED GROUPS - STATE
	//
	// Thin wrappers over the group entity, safe while dormant AND safe while the entity does not
	// exist at all (every Phase 2 record). Nothing below dereferences m_Group unguarded: a wiped
	// group's entity is deleted by the engine, so "the record exists but the entity is gone" is a
	// legitimate runtime state, not just a Phase 2 artefact.
	//------------------------------------------------------------------------------------------------

	//! \param[in] handle Handle from RegisterGroup.
	//! \return The engine-side group (exists dormant or spawned), or null when there is no entity.
	SCR_AIGroup GetGroup(int handle)
	{
		return ResolveGroup(GetRecord(handle));
	}

	//------------------------------------------------------------------------------------------------
	//! The ONLY way core reads a record's group entity.
	//!
	//! The engine can delete a group entity with no callback to core - eliminate-when-reached deletes
	//! it from inside its own frame event (SCR_AIGroup.c:3058), and a world teardown takes it too - so
	//! the record holds an EntityID and every touch re-resolves through it. A record whose entity is
	//! gone is a legitimate runtime state, not an error: it answers null and clears its stale pointer.
	//! \param[in] record The record to resolve, or null.
	//! \return The live group entity, or null.
	protected SCR_AIGroup ResolveGroup(OVT_VirtualGroupRecord record)
	{
		if (!record || !record.m_Group)
			return null;

		IEntity owner = GetOwner();
		if (!owner)
			return record.m_Group;

		BaseWorld world = owner.GetWorld();
		if (!world)
			return record.m_Group;

		if (!world.FindEntityByID(record.m_GroupId))
		{
			record.m_Group = null;
			return null;
		}

		return record.m_Group;
	}

	//------------------------------------------------------------------------------------------------
	//! Deliberately asks ONLY for the agent count. An earlier form also required !IsDormant(), but
	//! the engine's dormant flag is derived from the recorded dormant COUNTS, which core overwrites
	//! from the mask (D2b) - so a re-materialised group could answer "dormant" while its members
	//! stand in the world. Members existing is the whole question.
	//! \param[in] handle Handle from RegisterGroup.
	//! \return True when member characters exist in the world right now.
	bool IsSpawned(int handle)
	{
		SCR_AIGroup group = GetGroup(handle);
		if (!group)
			return false;

		return group.GetAgentsCount() > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Living members.
	//!
	//! MASK FIRST (Phase 1 amendment 4 / context.md "Core-layer defensive requirements" #1): when the
	//! survivor mask is populated it IS the answer, because the engine's dormant counts provably
	//! corrupt themselves when a despawn lands mid-refill. The engine numbers are only consulted for
	//! a group core has no mask for.
	//! \param[in] handle Handle from RegisterGroup.
	//! \return Living member count; 0 for an unknown handle.
	int GetAliveMemberCount(int handle)
	{
		OVT_VirtualGroupRecord record = GetRecord(handle);
		if (!record)
			return 0;

		if (record.m_aSlotAlive && !record.m_aSlotAlive.IsEmpty())
			return OVT_VirtualizationMath.CountAlive(record.m_aSlotAlive);

		if (!record.m_Group)
			return 0;

		if (record.m_Group.IsDormant())
			return record.m_Group.GetDormantAliveCount();

		return record.m_Group.GetAgentsCount();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] handle Handle from RegisterGroup.
	//! \return Full roster size (slot count), 0 until Phase 3 captures it at registration.
	int GetMemberCount(int handle)
	{
		OVT_VirtualGroupRecord record = GetRecord(handle);
		if (!record || !record.m_aSlotAlive)
			return 0;

		return record.m_aSlotAlive.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Per-slot survivor truth (D2) - the identity-accurate half of "dead members stay dead".
	//! \param[in] handle Handle from RegisterGroup.
	//! \param[in] slotIndex Roster slot index.
	//! \return True when that slot is alive; false for an unknown handle or an out-of-range slot.
	bool GetMemberAlive(int handle, int slotIndex)
	{
		OVT_VirtualGroupRecord record = GetRecord(handle);
		if (!record || !record.m_aSlotAlive)
			return false;

		if (slotIndex < 0 || slotIndex >= record.m_aSlotAlive.Count())
			return false;

		return record.m_aSlotAlive[slotIndex] != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] handle Handle from RegisterGroup.
	//! \return The group's origin - valid dormant or spawned. Falls back to the record's last known
	//!         position when the entity is gone; vector.Zero for an unknown handle.
	vector GetPosition(int handle)
	{
		OVT_VirtualGroupRecord record = GetRecord(handle);
		if (!record)
			return vector.Zero;

		SCR_AIGroup group = ResolveGroup(record);
		if (!group)
			return record.m_vPosition;

		return group.GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the group. This is `movement`'s entry point and is only meaningful WHILE DORMANT -
	//! moving a group whose members exist teleports nothing, it just relocates the record entity.
	//! \param[in] handle Handle from RegisterGroup.
	//! \param[in] position New origin.
	void SetPosition(int handle, vector position)
	{
		OVT_VirtualGroupRecord record = GetRecord(handle);
		if (!record)
			return;

		record.m_vPosition = position;

		SCR_AIGroup group = ResolveGroup(record);
		if (!group)
			return;

		group.SetOrigin(position);
	}

	//------------------------------------------------------------------------------------------------
	// TRACKED GROUPS - DEATH ACCOUNTING
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Records a member death BY ROSTER SLOT (D2).
	//!
	//! Called internally by the death hook (Phase 3, T3.4, driven by
	//! OVT_OverthrowGameMode.GetOnCharacterKilled() - never the group's agent-removal invoker, which
	//! cannot tell a death from the engine's own dormancy teardown - D2a). Public so consumers and
	//! tests can report deaths they observed themselves.
	//!
	//! Killing the last living slot wipes the record exactly as an in-world wipe would:
	//! OnGroupWiped fires BEFORE the record is removed, so a subscriber can still read it.
	//! Idempotent per slot - re-reporting an already-dead slot changes nothing and cannot
	//! double-fire the wipe.
	//! \param[in] handle Handle from RegisterGroup.
	//! \param[in] slotIndex Roster slot of the member that died.
	void ReportMemberKilled(int handle, int slotIndex)
	{
		OVT_VirtualGroupRecord record = GetRecord(handle);
		if (!record || !record.m_aSlotAlive)
			return;

		if (slotIndex < 0 || slotIndex >= record.m_aSlotAlive.Count())
		{
			Print(string.Format("[Overthrow] Virtualization: ReportMemberKilled(%1, %2) is out of range for a roster of %3 slots",
				handle.ToString(), slotIndex.ToString(), record.m_aSlotAlive.Count().ToString()), LogLevel.WARNING);
			return;
		}

		if (record.m_aSlotAlive[slotIndex] == 0)
			return;

		record.m_aSlotAlive[slotIndex] = 0;

		SCR_AIGroup group = ResolveGroup(record);

		// T3.5: the mask is the roster truth, so the engine's dormant counts are re-derived from it
		// the moment it changes. Deliberately a no-op while the group has never been dormant - see
		// SCR_AIGroup.ReassertOVTDormantCounts.
		if (group)
			group.ReassertOVTDormantCounts();

		if (!OVT_VirtualizationMath.IsWiped(record.m_aSlotAlive))
			return;

		// T3.6: WIPED. The mask is what decides this - never the group's OnEmpty, which fires on
		// EVERY dormant teardown (Phase 1 finding), and never the engine's elimination signal alone.
		Print(string.Format("[Overthrow] Virtualization: handle %1 (%2/%3) wiped - last of %4 slots died",
			handle.ToString(), record.m_sOwnerSystem, record.m_sOwnerKey, record.m_aSlotAlive.Count().ToString()), LogLevel.VERBOSE);

		// Fired BEFORE removal, so a subscriber can still read the record it is being told about.
		if (m_OnGroupWiped)
			m_OnGroupWiped.Invoke(handle);

		CleanUpWipedRecord(handle, record, group);
	}

	//------------------------------------------------------------------------------------------------
	//! Retires everything a wiped record owns and drops it from the registry (T3.6).
	//!
	//! HOW THE HUSK DIES, and why it is done this way. The group entity is retired first (mask
	//! cleared, dormant alive 0 so RequestSpawn refuses forever, eliminate-when-reached enabled) and
	//! is deleted DIRECTLY only when no member characters exist - a wipe reported on a dormant group,
	//! which is also the test path.
	//!
	//! With members still in the world the last one's death is mid-flight: this runs from the kill
	//! invoker, inside the damage handler, and vanilla itself refuses to delete a group from inside
	//! that cascade (SCR_AIGroup.OnEmpty defers by 1 ms - "doing it directly in this event would be
	//! unsafe"). So the husk is left to the engine, which has two ways to take it:
	//!   - eliminate-when-reached, the LIKELY one - you can only wipe a group in combat by standing
	//!     near it, and the next 1 Hz LifecycleTick over an empty group with an observer inside
	//!     veryNearBlockDist deletes the entity (SCR_AIGroup.c:3038-3059);
	//!   - vanilla's own empty handling, asserted with SetDeleteWhenEmpty(true), which fires unless
	//!     the engine already considers the group dormant.
	//! A husk that outlives both is inert: dormant alive 0 means the engine refuses to spawn it
	//! forever, and it is deleted the moment anyone walks up to it. The record is gone either way.
	//! \param[in] handle The wiped handle.
	//! \param[in] record Its record.
	//! \param[in] group Its group entity, or null when the entity is already gone.
	protected void CleanUpWipedRecord(int handle, notnull OVT_VirtualGroupRecord record, SCR_AIGroup group)
	{
		ForgetMembers(record);
		DeleteOwnedWaypoints(record, group);
		ClosePersistenceTracking(group);
		m_mHandleByGroup.Remove(record.m_GroupId);

		if (group)
		{
			group.GetOnMembersDespawning().Remove(OnGroupMembersDespawning);
			group.GetOnEliminatedWhenReached().Remove(OnGroupEliminatedWhenReached);

			RetireGroupEntity(group);

			if (group.GetAgentsCount() > 0)
				group.SetDeleteWhenEmpty(true);
			else
				SCR_EntityHelper.DeleteEntityAndChildren(group);
		}

		record.m_Group = null;
		m_mRecords.Remove(handle);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops every member -> (handle, slot) entry belonging to a record. Called whenever the record's
	//! members stop existing (dormant teardown, wipe, unregister) so a later kill event can never
	//! resolve a deleted member to a live slot.
	//! \param[in] record The record whose members are going away.
	protected void ForgetMembers(OVT_VirtualGroupRecord record)
	{
		if (!record || !m_mMemberSlots)
			return;

		array<EntityID> stale = new array<EntityID>();
		foreach (EntityID memberId, OVT_VirtualMemberSlot entry : m_mMemberSlots)
		{
			if (entry && entry.m_iHandle == record.m_iHandle)
				stale.Insert(memberId);
		}

		foreach (EntityID memberId : stale)
		{
			m_mMemberSlots.Remove(memberId);
		}
	}

	//------------------------------------------------------------------------------------------------
	// TRACKED GROUPS - ENGINE SIGNALS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The modded SCR_AIGroup calls this as each member materialises into a known roster slot (D2).
	//! Static because the call site is an entity override with no manager reference of its own.
	//! \param[in] group The group that spawned the member.
	//! \param[in] slotIndex The roster slot it was spawned as.
	//! \param[in] member The member entity.
	static void NotifyMemberSpawned(SCR_AIGroup group, int slotIndex, IEntity member)
	{
		if (!s_Instance)
			return;

		s_Instance.OnMemberSpawned(group, slotIndex, member);
	}

	//------------------------------------------------------------------------------------------------
	//! Records member -> (handle, slot) so the death hook can answer "which slot just died?".
	//! \param[in] group The group that spawned the member.
	//! \param[in] slotIndex The roster slot it was spawned as.
	//! \param[in] member The member entity.
	protected void OnMemberSpawned(SCR_AIGroup group, int slotIndex, IEntity member)
	{
		if (!group || !member || !m_mMemberSlots || !m_mHandleByGroup)
			return;

		int handle;
		if (!m_mHandleByGroup.Find(group.GetID(), handle))
			return;

		OVT_VirtualGroupRecord record = GetRecord(handle);
		if (!record)
			return;

		// A member exists again, so the dormant-teardown window is over.
		record.m_bDespawning = false;

		OVT_VirtualMemberSlot entry = new OVT_VirtualMemberSlot();
		entry.m_iHandle = handle;
		entry.m_iSlot = slotIndex;

		m_mMemberSlots.Set(member.GetID(), entry);
	}

	//------------------------------------------------------------------------------------------------
	//! The engine is about to delete this group's members for dormancy. THE teardown-window guard
	//! (D2a): the reverse-map entries go first, so the deletions that follow cannot be mistaken for
	//! casualties by anything downstream, and the flag makes the intent explicit for the kill hook.
	//! \param[in] group The group going dormant.
	protected void OnGroupMembersDespawning(SCR_AIGroup group)
	{
		if (!group || !m_mHandleByGroup)
			return;

		int handle;
		if (!m_mHandleByGroup.Find(group.GetID(), handle))
			return;

		OVT_VirtualGroupRecord record = GetRecord(handle);
		if (!record)
			return;

		record.m_bDespawning = true;
		ForgetMembers(record);
	}

	//------------------------------------------------------------------------------------------------
	//! The engine decided an observer reached this group while it had no members, and is deleting it
	//! (SCR_AIGroup.c:3045-3059). Core only ever opts a group into that AFTER the mask says it is
	//! wiped, so this normally arrives for a husk whose record is already gone - but a group could
	//! also be eliminated by something else, so the mask is re-checked rather than trusted.
	//! \param[in] group The group being eliminated.
	protected void OnGroupEliminatedWhenReached(SCR_AIGroup group)
	{
		if (!group || !m_mHandleByGroup)
			return;

		int handle;
		if (!m_mHandleByGroup.Find(group.GetID(), handle))
			return;

		OVT_VirtualGroupRecord record = GetRecord(handle);
		if (!record)
		{
			m_mHandleByGroup.Remove(group.GetID());
			return;
		}

		Print(string.Format("[Overthrow] Virtualization: engine eliminated handle %1 (%2/%3) on reach - dropping the record",
			handle.ToString(), record.m_sOwnerSystem, record.m_sOwnerKey), LogLevel.VERBOSE);

		if (m_OnGroupWiped)
			m_OnGroupWiped.Invoke(handle);

		// The engine deletes the entity itself the moment this returns, so nothing here may delete
		// it - hand null to the cleanup and let it strip everything else.
		ForgetMembers(record);
		DeleteOwnedWaypoints(record, group);
		ClosePersistenceTracking(group);

		group.GetOnMembersDespawning().Remove(OnGroupMembersDespawning);
		group.GetOnEliminatedWhenReached().Remove(OnGroupEliminatedWhenReached);
		m_mHandleByGroup.Remove(record.m_GroupId);

		record.m_Group = null;
		m_mRecords.Remove(handle);
	}

	//------------------------------------------------------------------------------------------------
	//! The game mode's universal character-killed invoker (D2a - the ONLY death source core trusts).
	//!
	//! Resolves the victim through the member reverse map, which by construction only holds members
	//! that exist right now: a member deleted by a dormant teardown was removed from it before the
	//! deletion, so a teardown can never be read as a wipe (R13 - "catastrophic and silent").
	//! \param[in] victim The character that died.
	//! \param[in] instigator Whoever killed it (unused - core does not care who).
	protected void OnCharacterKilled(IEntity victim, IEntity instigator)
	{
		if (!victim || !m_mMemberSlots)
			return;

		OVT_VirtualMemberSlot entry = m_mMemberSlots.Get(victim.GetID());
		if (!entry)
			return;

		m_mMemberSlots.Remove(victim.GetID());

		OVT_VirtualGroupRecord record = GetRecord(entry.m_iHandle);
		if (!record)
			return;

		// Belt and braces on top of the reverse-map teardown: anything arriving inside the engine's
		// despawn window describes a member being deleted, not one being killed.
		if (record.m_bDespawning)
			return;

		ReportMemberKilled(entry.m_iHandle, entry.m_iSlot);
	}

	//------------------------------------------------------------------------------------------------
	// TRACKED GROUPS - LIFECYCLE OVERRIDES
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Materialises a group regardless of proximity (SCR_AIGroup.RequestSpawn, :2678).
	//!
	//! FORCE IS A NUDGE, NOT A PIN. The request joins the engine's importance-ordered spawn queue and
	//! is re-validated against observer proximity at dispatch time, and the group's own 1 Hz lifecycle
	//! tick will despawn it again as soon as no observer is inside its despawn ring. To keep a group
	//! materialised, register it with a huge spawnDistanceOverride instead.
	//!
	//! A group the mask reports wiped is refused by the engine anyway (dormant alive 0 -> RequestSpawn
	//! returns, :2680-2688), which is the "a wiped group never comes back" guarantee.
	//! \param[in] handle Handle from RegisterGroup.
	void ForceSpawn(int handle)
	{
		SCR_AIGroup group = GetGroup(handle);
		if (!group)
		{
			Print(string.Format("[Overthrow] Virtualization: ForceSpawn(%1) - no group entity", handle.ToString()), LogLevel.WARNING);
			return;
		}

		// A Manual-policy ("never materialise by proximity") group refuses every dispatch core did
		// not ask for - this IS core asking. Inert for ProximityDriven groups.
		group.ArmOVTManualSpawn();
		group.RequestSpawn();
	}

	//------------------------------------------------------------------------------------------------
	//! Despawns a group's members regardless of proximity (SCR_AIGroup.DespawnMembers, :2864).
	//!
	//! Held members (in a vehicle, player-engaged) are reported but not protected here: the engine's
	//! own despawn deletes them, and a caller that explicitly asks for a despawn gets one. The
	//! per-slot mask is unaffected - despawning is not dying.
	//! \param[in] handle Handle from RegisterGroup.
	void ForceDespawn(int handle)
	{
		SCR_AIGroup group = GetGroup(handle);
		if (!group)
		{
			Print(string.Format("[Overthrow] Virtualization: ForceDespawn(%1) - no group entity", handle.ToString()), LogLevel.WARNING);
			return;
		}

		if (group.GetAgentsCount() == 0)
			return;

		if (group.HasHeldMember())
		{
			Print(string.Format("[Overthrow] Virtualization: ForceDespawn(%1) is despawning a group with a held member (in a vehicle or player-engaged)",
				handle.ToString()), LogLevel.VERBOSE);
		}

		group.DespawnMembers();
	}

	//------------------------------------------------------------------------------------------------
	// DEBUG AFFORDANCE (T3.9)
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Registers a single virtual group with a small patrol plan so core can be play-tested with no
	//! consumer wired (R10 - nothing consumes core until `civilians` and `integration` land).
	//!
	//! Anchored on the campaign's starting town when the real-estate manager has already picked one,
	//! otherwise on the first registered town, and the resolved position is PRINTED - the tester has
	//! to be able to walk to it. Off by default; §6 steps 1-6 of the plan are what it exists for.
	protected void RegisterDebugTestGroup()
	{
		// RECLAIM BEFORE REGISTER - the pattern api.md tells every consumer to use, exercised here so
		// the debug affordance is also the first proof of it. On a continued campaign the registry
		// serializer has already re-created these groups by the time PostGameStart runs, and registering
		// a second set would leave the tester walking up to overlapping patrols.
		//
		// BY SYSTEM, not by key: the scale probe registers many owner keys under the one "debug" system.
		array<int> existing = FindGroupsBySystem("debug");
		if (!existing.IsEmpty())
		{
			PrintFormat("[Overthrow] Virtualization: DEBUG %1 test group(s) came back from the save (first is handle %2 at %3) - not registering more",
				existing.Count().ToString(), existing[0].ToString(), GetPosition(existing[0]).ToString());
			return;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || !towns.m_Towns || towns.m_Towns.IsEmpty())
		{
			Print("[Overthrow] Virtualization: debug test group asked for, but this world has no towns to anchor it on", LogLevel.WARNING);
			return;
		}

		int townId = 0;
		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if (realEstate && realEstate.m_iStartingTownId >= 0 && realEstate.m_iStartingTownId < towns.m_Towns.Count())
			townId = realEstate.m_iStartingTownId;

		vector anchor = towns.m_Towns[townId].location;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return;

		int count = m_iDebugTestGroupCount;
		if (count < 1)
			count = 1;

		string groupName = ResolveDebugTestGroupName(config.m_sOccupyingFaction);

		// T6.1 / G12: the wall clock around the whole burst is the number §6 step 8 compares against the
		// Phase 1 scale probe (100 groups in 73 ms). One line, whatever the count, so the single-group
		// affordance reads the same as it always did.
		int startTick = System.GetTickCount();

		int registered = 0;
		int firstHandle = -1;

		for (int i = 0; i < count; i++)
		{
			vector position = ResolveDebugTestGroupPosition(anchor, i, count);

			string ownerKey = "test_group";
			if (count > 1)
				ownerKey = string.Format("test_group_%1", i.ToString());

			int handle = RegisterDebugTestGroupAt(config, ownerKey, groupName, position);
			if (handle == -1)
				continue;

			registered++;

			if (firstHandle == -1)
				firstHandle = handle;
		}

		int elapsed = System.GetTickCount() - startTick;

		if (registered == 0)
		{
			PrintFormat("[Overthrow] Virtualization: DEBUG test group registration refused (faction '%1', group '%2')",
				config.m_sOccupyingFaction, groupName);
			return;
		}

		PrintFormat("[Overthrow] Virtualization: DEBUG registered %1 of %2 test group(s) in %3 ms near %4 - first is handle %5 at %6, %7 slots, spawn ring %8 m",
			registered.ToString(), count.ToString(), elapsed.ToString(), towns.GetTownName(townId),
			firstHandle.ToString(), GetPosition(firstHandle).ToString(), GetMemberCount(firstHandle).ToString(),
			GetSpawnDistance(firstHandle).ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! Where the i-th debug test group goes.
	//!
	//! A single group keeps the original behaviour EXACTLY - m_fDebugTestGroupOffset metres due east of
	//! the anchor - because the Phase 3 play-test steps are written against walking to that spot.
	//!
	//! A scale probe instead spreads the groups over a disc of that radius on a golden-angle spiral, so
	//! ~40 groups cover a town-sized area evenly with no two on top of each other. Even coverage is the
	//! point: the measurement is "how does the engine's queue behave when a fast travel drops an observer
	//! into a dense area", and a stack of 40 groups at one point would all share one observer check.
	//! \param[in] anchor The town position everything is measured from.
	//! \param[in] index Which group this is.
	//! \param[in] count How many are being registered.
	//! \return The registration position.
	protected vector ResolveDebugTestGroupPosition(vector anchor, int index, int count)
	{
		if (count <= 1)
			return anchor + Vector(m_fDebugTestGroupOffset, 0, 0);

		// sqrt spacing = equal area per group; 137.5 deg = the golden angle, which is what keeps
		// successive points from lining up into spokes.
		float slot = index + 1;
		float total = count;
		float radius = m_fDebugTestGroupOffset * Math.Sqrt(slot / total);
		float angle = index * 137.5;

		float x = radius * Math.Cos(angle * Math.DEG2RAD);
		float z = radius * Math.Sin(angle * Math.DEG2RAD);

		return anchor + Vector(x, 0, z);
	}

	//------------------------------------------------------------------------------------------------
	//! Registers one debug group with a two-leg cycling patrol, so the affordance also exercises the
	//! waypoint half (T3.2/D6).
	//! \param[in] config The config component, for the occupying faction key.
	//! \param[in] ownerKey Owner key to register under.
	//! \param[in] groupName The already-resolved group registry name (ResolveDebugTestGroupName).
	//! \param[in] position Where it goes.
	//! \return The handle, or -1 when the registration was refused.
	protected int RegisterDebugTestGroupAt(notnull OVT_OverthrowConfigComponent config, string ownerKey, string groupName, vector position)
	{
		// Play-test fix (2026-08-17): the anchor position arrives with a flat Y (a town anchor's, spread over a disc for
		// the scale probe), and the far leg is 150 m away on whatever slope is there. Snap both plan
		// points at authoring time so the registered record, the save and the debug log all carry
		// positions a person could stand on. (CreatePlannedWaypoint snaps the spawned ENTITY for every
		// consumer; this fixes the debug affordance's own authored data.)
		BaseWorld snapWorld = GetOwner().GetWorld();
		if (snapWorld)
			position[1] = snapWorld.GetSurfaceY(position[0], position[2]);

		vector farPoint = position + Vector(0, 0, 150);
		if (snapWorld)
			farPoint[1] = snapWorld.GetSurfaceY(farPoint[0], farPoint[2]);

		OVT_VirtualWaypointPlan plan = new OVT_VirtualWaypointPlan();
		plan.m_aPositions.Insert(position);
		plan.m_aTypes.Insert(OVT_EVirtualWaypointType.PATROL);
		plan.m_aParams.Insert(30);
		plan.m_aPositions.Insert(farPoint);
		plan.m_aTypes.Insert(OVT_EVirtualWaypointType.PATROL);
		plan.m_aParams.Insert(30);
		plan.m_bCycle = true;

		// The spawn-distance override is the affordance's own: a GM camera is an engine-registered
		// observer that cannot be switched off from script, so at the global 1750 m ring a tester
		// spectating the group is holding it awake and can never watch a despawn. A small ring
		// (~300 m, despawning at ~345 m) lets the camera sit outside both rings and drive the
		// lifecycle by flying in and out. -1 keeps the global distance.
		return RegisterGroup("debug", ownerKey, config.m_sOccupyingFaction, groupName,
			position, plan, m_iDebugTestGroupSpawnDistance, SCR_EAISpawnImportance.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Picks the group registry entry the debug affordance registers.
	//!
	//! EMPTY (the default) means "whatever this faction defines first that actually resolves", which is
	//! what keeps the affordance working on a faction config it has never seen. A NAME is honoured when
	//! it resolves and otherwise falls back to that same first entry with a warning - a typo'd name must
	//! not silently turn the play-test into "core registered nothing".
	//! \param[in] factionKey The occupying faction key.
	//! \return A registry name, or an empty string when the faction resolves nothing at all (RegisterGroup
	//!         then refuses and says so).
	protected string ResolveDebugTestGroupName(string factionKey)
	{
		OVT_OverthrowFactionManager factionManager = OVT_Global.GetFactions();
		if (!factionManager)
			return m_sDebugTestGroupName;

		OVT_Faction faction = factionManager.GetOverthrowFactionByKey(factionKey);
		if (!faction)
			return m_sDebugTestGroupName;

		if (m_sDebugTestGroupName != "" && faction.GetGroupPrefabByName(m_sDebugTestGroupName) != ResourceName.Empty)
			return m_sDebugTestGroupName;

		string first = "";
		array<string> names = faction.GetAvailableGroupNames();
		if (names)
		{
			foreach (string name : names)
			{
				if (faction.GetGroupPrefabByName(name) == ResourceName.Empty)
					continue;

				first = name;
				break;
			}
		}

		if (m_sDebugTestGroupName != "")
		{
			Print(string.Format("[Overthrow] Virtualization: DEBUG group '%1' does not resolve on faction '%2' - falling back to '%3'",
				m_sDebugTestGroupName, factionKey, first), LogLevel.WARNING);
		}

		return first;
	}

	//------------------------------------------------------------------------------------------------
	// DEBUG OBSERVER PROBE - "why will this group not despawn?"
	//
	// Observers are NOT players (Phase 1 T1.2). The GM free camera is one; the deploy-point spawn
	// preload (SCR_SpawnRequestComponent.StartSpawnPreload) parks a FIXED MP observer at the deploy
	// position with pEntity = null, which only upgrades to follow-the-character on the preload-finished
	// RPC. Either of those sitting next to a debug test group keeps it awake forever no matter where
	// the player walks - and from inside the engine's lifecycle there is nothing to see. So this
	// prints the whole observer table next to the group's own rings, and lets the tester read off
	// which entry is holding it.
	//
	// ⚠ READ-ONLY. It never calls InsertObserverSP/InsertObserverMP - a null-entity SP insert froze
	// the client once already (context.md gotcha 0).
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Installs the 5 s observer probe once. Only ever called behind m_bDebugRegisterTestGroup, so
	//! normal play never pays for it and never sees a line of it.
	protected void EnsureDebugObserverTick()
	{
		if (m_bDebugObserverTickRunning)
			return;

		if (!Replication.IsServer())
			return;

		GetGame().GetCallqueue().CallLater(DebugObserverTick, 5000, true);
		m_bDebugObserverTickRunning = true;
	}

	//------------------------------------------------------------------------------------------------
	//! One compact block per tick describing the FIRST debug test group: its state, its lifecycle
	//! rings and the engine's answer on them, the nearest player-controlled character, and every
	//! observer the world holds with its distance to the group.
	//!
	//! Prefixed [Overthrow] VIRT-DEBUG: on every line so the whole run greps out of a log at once.
	protected void DebugObserverTick()
	{
		// SELF-CANCEL FOR A DEAD WORLD (R7), the AmbientTick arbiter: the call queue outlives a
		// campaign, so an entry whose OnDelete never ran would keep logging the previous world.
		if (s_Instance != this)
		{
			ScriptCallQueue queue;
			if (GetGame())
				queue = GetGame().GetCallqueue();

			if (queue)
				queue.Remove(DebugObserverTick);

			m_bDebugObserverTickRunning = false;
			return;
		}

		int handle = FindFirstDebugGroupHandle();
		if (handle == -1)
		{
			Print("[Overthrow] VIRT-DEBUG: no group registered under the 'debug' system yet", LogLevel.NORMAL);
			return;
		}

		OVT_VirtualGroupRecord record = GetRecord(handle);
		if (!record)
			return;

		SCR_AIGroup group = GetGroup(handle);
		vector position = GetPosition(handle);

		if (!group)
		{
			PrintFormat("[Overthrow] VIRT-DEBUG: handle %1 (%2/%3) has NO group entity at %4",
				handle.ToString(), record.m_sOwnerSystem, record.m_sOwnerKey, position.ToString());
			return;
		}

		int spawnDistance = GetSpawnDistance(handle);
		int despawnDistance = OVT_VirtualizationMath.ResolveDespawnDistance(spawnDistance, m_fDespawnHysteresis);

		string dormant = "no";
		if (group.IsDormant())
			dormant = "yes";

		string inSpawn = "no";
		if (spawnDistance > 0 && group.HasObserverInRange(spawnDistance))
			inSpawn = "yes";

		string inDespawn = "no";
		if (despawnDistance > 0 && group.HasObserverInRange(despawnDistance))
			inDespawn = "yes";

		PrintFormat("[Overthrow] VIRT-DEBUG: handle %1 (%2/%3) at %4 | agents %5, dormant %6, dormantAlive %7, dormantDead %8",
			handle.ToString(), record.m_sOwnerSystem, record.m_sOwnerKey, group.GetOrigin().ToString(),
			group.GetAgentsCount().ToString(), dormant, group.GetDormantAliveCount().ToString(),
			group.GetDormantDeadCount().ToString());

		PrintFormat("[Overthrow] VIRT-DEBUG:   rings spawn %1 m / despawn %2 m | engine says observerInSpawnRing %3, observerInDespawnRing %4",
			spawnDistance.ToString(), despawnDistance.ToString(), inSpawn, inDespawn);

		LogNearestPlayer(group.GetOrigin());
		LogObserverTable(group.GetOrigin(), despawnDistance);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The lowest handle owned by the "debug" system (so the block always describes the SAME
	//!         group tick after tick - map iteration order is not stable), or -1 when there is none.
	protected int FindFirstDebugGroupHandle()
	{
		int first = -1;

		foreach (int handle : FindGroupsBySystem("debug"))
		{
			if (first == -1 || handle < first)
				first = handle;
		}

		return first;
	}

	//------------------------------------------------------------------------------------------------
	//! Distance from the group to the nearest player-CONTROLLED character. Deliberately separate from
	//! the observer table below: when the nearest player is 4 km out and an observer is 20 m away,
	//! the gap between these two lines IS the diagnosis.
	//! \param[in] origin The group origin.
	protected void LogNearestPlayer(vector origin)
	{
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
		{
			Print("[Overthrow] VIRT-DEBUG:   nearest player: no player manager", LogLevel.NORMAL);
			return;
		}

		array<int> ids = new array<int>();
		players.GetPlayers(ids);

		float nearest = -1;
		int nearestId = -1;

		foreach (int playerId : ids)
		{
			IEntity controlled = players.GetPlayerControlledEntity(playerId);
			if (!controlled)
				continue;

			float distance = vector.Distance(controlled.GetOrigin(), origin);
			if (nearest < 0 || distance < nearest)
			{
				nearest = distance;
				nearestId = playerId;
			}
		}

		if (nearestId == -1)
		{
			PrintFormat("[Overthrow] VIRT-DEBUG:   nearest player: none controlling a character (%1 connected)", ids.Count().ToString());
			return;
		}

		PrintFormat("[Overthrow] VIRT-DEBUG:   nearest player: %1 (id %2) at %3 m",
			players.GetPlayerName(nearestId), nearestId.ToString(), ((int)nearest).ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! The observer table - THE point of this probe. Counts and positions from GetObserversSP (local
	//! cameras, including the GM free camera) and GetObserversMP (connected players and fixed
	//! inserts such as the deploy-point preload), each with its distance to the group and a tag when
	//! it is inside the despawn ring, i.e. when it is the thing keeping the group awake.
	//!
	//! ⚠ THE ENGINE HANDS BACK "2D positions" IN A vector AND DOES NOT SAY WHICH COMPONENTS. Both
	//! readings are printed (XZ and XY) rather than guessing: one of them will track the group
	//! sensibly and the other will be nonsense, and one look at the log settles it. The in-ring tag
	//! uses whichever is nearer, so it can never miss a holder by picking the wrong pair.
	//! \param[in] origin The group origin.
	//! \param[in] despawnDistance The despawn ring in metres, for the tag.
	protected void LogObserverTable(vector origin, int despawnDistance)
	{
		ObserversSystem observers = GetObserversSystem();
		if (!observers)
		{
			Print("[Overthrow] VIRT-DEBUG:   observers: this world has no ObserversSystem", LogLevel.NORMAL);
			return;
		}

		array<vector> sp = new array<vector>();
		array<vector> mp = new array<vector>();

		observers.GetObserversSP(sp);
		observers.GetObserversMP(mp);

		PrintFormat("[Overthrow] VIRT-DEBUG:   observers: %1 SP (local cameras), %2 MP (players + fixed inserts)",
			sp.Count().ToString(), mp.Count().ToString());

		LogObserverRows("SP", sp, origin, despawnDistance);
		LogObserverRows("MP", mp, origin, despawnDistance);
	}

	//------------------------------------------------------------------------------------------------
	//! One row per observer in a table.
	//! \param[in] label "SP" or "MP".
	//! \param[in] entries The positions the engine handed back.
	//! \param[in] origin The group origin.
	//! \param[in] despawnDistance The despawn ring in metres.
	protected void LogObserverRows(string label, notnull array<vector> entries, vector origin, int despawnDistance)
	{
		for (int i = 0; i < entries.Count(); i++)
		{
			vector entry = entries[i];

			float dxz = vector.Distance(Vector(entry[0], origin[1], entry[2]), origin);
			float dxy = vector.Distance(Vector(entry[0], origin[1], entry[1]), origin);

			float nearer = dxz;
			if (dxy < nearer)
				nearer = dxy;

			string tag = "";
			if (despawnDistance > 0 && nearer <= despawnDistance)
				tag = "  <-- INSIDE DESPAWN RING (this one is holding the group)";

			PrintFormat("[Overthrow] VIRT-DEBUG:     %1[%2] raw %3 | asXZ %4 m, asXY %5 m%6",
				label, i.ToString(), entry.ToString(), ((int)dxz).ToString(), ((int)dxy).ToString(), tag);
		}
	}

	//------------------------------------------------------------------------------------------------
	// EVENTS - all server-side ScriptInvokers
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Fired with (int handle) when a group is wiped - BEFORE the record is removed, so a subscriber
	//! can still read the record it is being told about.
	//! \return The invoker, created on first use.
	ScriptInvoker GetOnGroupWiped()
	{
		if (!m_OnGroupWiped)
			m_OnGroupWiped = new ScriptInvoker();

		return m_OnGroupWiped;
	}

	//------------------------------------------------------------------------------------------------
	//! Fired once, with no arguments, after persisted records have been re-linked to their restored
	//! group entities (Phase 5, T5.3). THE reclaim point for consumers (D9) - reclaiming from your
	//! own deserialize races the group entities' restoration.
	//! \return The invoker, created on first use.
	ScriptInvoker GetOnRecordsRestored()
	{
		if (!m_OnRecordsRestored)
			m_OnRecordsRestored = new ScriptInvoker();

		return m_OnRecordsRestored;
	}

	//------------------------------------------------------------------------------------------------
	// CONFIG
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The operator-editable global spawn distance (Overthrow_Config.json, D5).
	//! \return virtualizationSpawnDistance, or the component's fallback before the config is loaded.
	int GetGlobalSpawnDistance()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config || !config.m_ConfigFile)
			return m_iFallbackSpawnDistance;

		return config.m_ConfigFile.virtualizationSpawnDistance;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the spawn distance a handle's group would be stamped with (D5).
	//! \param[in] handle Handle from RegisterGroup.
	//! \return Metres; 0 for an unknown handle.
	int GetSpawnDistance(int handle)
	{
		OVT_VirtualGroupRecord record = GetRecord(handle);
		if (!record)
			return 0;

		return OVT_VirtualizationMath.ResolveSpawnDistance(record.m_iSpawnDistanceOverride, GetGlobalSpawnDistance());
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] handle Handle from RegisterGroup.
	//! \return The SCR_EAISpawnImportance tier the record was registered with; -1 for an unknown
	//!         handle (never vanilla's silent LOW default - D4/F14).
	int GetImportance(int handle)
	{
		OVT_VirtualGroupRecord record = GetRecord(handle);
		if (!record)
			return -1;

		return record.m_eImportance;
	}

	//------------------------------------------------------------------------------------------------
	// AMBIENT SPAWN SOURCES (Phase 4 - T4.3 to T4.7)
	//
	// WHY THIS HALF EXISTS AT ALL. Ambient entities are loose IEntities - town civilians, parked
	// vehicles - not groups, so the engine's GROUP lifecycle cannot drive them. This is therefore the
	// ONE tick core owns; every tracked group in the registry above is still the engine's business and
	// no CallLater anywhere touches one.
	//
	// HOW IT DIFFERS FROM A TRACKED GROUP, in one line: a group remembers who died, a source
	// remembers nothing. A despawn deletes the entities and forgets the roll; the next approach
	// RE-ROLLS from config. Nothing ambient is ever persisted, by construction.
	//
	// PROXIMITY IS THE ENGINE'S OBSERVER SET, NEVER A PLAYER LOOP (D11). ObserversSystem is what the
	// engine's own group lifecycle and vanilla's SCR_AmbientPatrolSystem ask, and it covers more than
	// players: local cameras, connected players, fixed MP inserts (the deploy-point spawn preload
	// parks one at the deploy point until its destructor runs) and the optics far observer. Two
	// consequences a consumer must plan for: a source near campaign start may be permanently
	// activated, and a source can activate with no player anywhere near it. Mixing in a
	// player-distance loop would make core's "nearby" mean two different things at once, so there
	// isn't one.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Registers an ambient spawn source at a position and returns its handle.
	//!
	//! Registration spawns NOTHING. The source is dormant until the tick finds an observer inside its
	//! spawn ring, exactly like a tracked group - which is what makes it safe to register a whole
	//! town's worth of sources at campaign start.
	//!
	//! The config may be authored (from m_AmbientRegistry) or built in code; the instance keeps its
	//! own reference either way, so a consumer may drop its own.
	//! \param[in] config The source declaration - what to roll, how many, how far to scatter.
	//! \param[in] position Where the source sits. Proximity is measured from here.
	//! \param[in] ownerKey Consumer-defined identity, so a consumer can recognise its own sources.
	//! \return The handle, or -1 on failure (not server, not initialised).
	int RegisterAmbientSource(notnull OVT_AmbientSpawnSourceConfig config, vector position, string ownerKey)
	{
		if (!Replication.IsServer())
		{
			Print("[Overthrow] Virtualization: RegisterAmbientSource called on a client - ambient sources are server-only", LogLevel.WARNING);
			return -1;
		}

		if (!m_mAmbientSources)
		{
			Print("[Overthrow] Virtualization: RegisterAmbientSource called before the manager was initialised", LogLevel.WARNING);
			return -1;
		}

		OVT_AmbientSpawnSourceInstance instance = new OVT_AmbientSpawnSourceInstance();
		instance.m_iHandle = m_iNextAmbientHandle;
		instance.m_Config = config;
		instance.m_vPosition = position;
		instance.m_sOwnerKey = ownerKey;

		m_iNextAmbientHandle++;
		m_mAmbientSources.Insert(instance.m_iHandle, instance);
		m_aAmbientOrder.Insert(instance.m_iHandle);

		// A consumer may register before DoStartGame (or in a world where PostGameStart never runs at
		// all, e.g. the autotest world), so the tick is ensured here too. Idempotent.
		EnsureAmbientTick();

		return instance.m_iHandle;
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes a source's live entities and drops the source (T4.3).
	//!
	//! The entities go through the config's OnEntityDespawning hook exactly as an ordinary despawn
	//! would, so a consumer that detaches things there does not need a second teardown path. Entities
	//! already RELEASED are not touched - that is the whole point of releasing them.
	//! \param[in] handle Handle from RegisterAmbientSource.
	//! \return True when a source was removed; false when the handle is unknown.
	bool UnregisterAmbientSource(int handle)
	{
		if (!m_mAmbientSources)
			return false;

		OVT_AmbientSpawnSourceInstance instance = m_mAmbientSources.Get(handle);
		if (!instance)
			return false;

		DespawnAmbientSource(instance);

		m_mAmbientSources.Remove(handle);

		int orderIndex = m_aAmbientOrder.Find(handle);
		if (orderIndex != -1)
			m_aAmbientOrder.Remove(orderIndex);

		// The cursor indexes into the order array, which just got shorter - SliceIndices() takes it
		// modulo the count anyway, but clamping here keeps the round-robin from skipping a source on
		// the tick after a removal.
		if (m_aAmbientOrder.IsEmpty())
			m_iAmbientCursor = 0;
		else
			m_iAmbientCursor = m_iAmbientCursor % m_aAmbientOrder.Count();

		return true;
	}

	//! \return Number of registered ambient sources; 0 on a client or before initialisation.
	int GetAmbientSourceCount()
	{
		if (!m_mAmbientSources)
			return 0;

		return m_mAmbientSources.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The entities a source currently owns.
	//!
	//! A COPY, and pruned first: the caller gets only entities that exist right now, and mutating the
	//! result cannot corrupt the source's own list.
	//! \param[in] handle Handle from RegisterAmbientSource.
	//! \return The live entities; an empty array for an unknown handle.
	array<IEntity> GetAmbientEntities(int handle)
	{
		array<IEntity> entities = new array<IEntity>();
		if (!m_mAmbientSources)
			return entities;

		OVT_AmbientSpawnSourceInstance instance = m_mAmbientSources.Get(handle);
		if (!instance)
			return entities;

		PruneAmbientEntities(instance);
		entities.Copy(instance.m_aEntities);

		return entities;
	}

	//------------------------------------------------------------------------------------------------
	//! OWNERSHIP TRANSFER (T4.6). The entity stops being ambient: it is removed from its source's list
	//! and the reverse map, so the source's next despawn will NOT delete it and the next activation
	//! will not count it. This is what `civilians` calls when a player recruits a civilian, and what a
	//! future vehicle-theft path calls.
	//!
	//! O(1) - a crowd of released civilians must not turn recruitment into a walk of every source.
	//!
	//! THE LIST IS THE PROOF, NOT THE MAP. The reverse map is keyed by EntityID and the engine is free
	//! to hand a retired id out again, so a map hit alone could name an entity that merely INHERITED a
	//! dead ambient entity's id. The claim is only honoured when the source's own list still holds
	//! this exact pointer; a hit that fails that check drops the stale entry and answers false.
	//! \param[in] entity The ambient entity to release.
	//! \return True when the entity was ambient and has been released; false when it was never
	//!         ambient (already released, or spawned by something else entirely).
	bool ReleaseAmbientEntity(notnull IEntity entity)
	{
		if (!m_mAmbientByEntity)
			return false;

		int handle;
		if (!m_mAmbientByEntity.Find(entity.GetID(), handle))
			return false;

		OVT_AmbientSpawnSourceInstance instance = m_mAmbientSources.Get(handle);
		if (!instance || !instance.m_aEntities)
		{
			m_mAmbientByEntity.Remove(entity.GetID());
			return false;
		}

		int index = instance.m_aEntities.Find(entity);
		if (index == -1)
		{
			m_mAmbientByEntity.Remove(entity.GetID());
			return false;
		}

		instance.m_aEntities.Remove(index);
		m_mAmbientByEntity.Remove(entity.GetID());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The authored registry, or null when none is configured (core's default).
	OVT_AmbientSpawnSourceRegistry GetAmbientRegistry()
	{
		return m_AmbientRegistry;
	}

	//------------------------------------------------------------------------------------------------
	//! Convenience lookup so a consumer does not have to hold the registry itself.
	//! \param[in] name The config's m_sSourceName.
	//! \return The config, or null when no registry is configured or nothing carries that name.
	OVT_AmbientSpawnSourceConfig FindAmbientSourceConfig(string name)
	{
		if (!m_AmbientRegistry)
			return null;

		return m_AmbientRegistry.FindByName(name);
	}

	//------------------------------------------------------------------------------------------------
	// AMBIENT - THE TICK
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Installs the ambient CallLater once. Idempotent: two ticks would double every per-tick budget
	//! and halve the round-robin period, and neither would be visible in a log.
	protected void EnsureAmbientTick()
	{
		if (m_bAmbientTickRunning)
			return;

		if (!Replication.IsServer())
			return;

		int interval = m_iAmbientTickIntervalMs;
		if (interval <= 0)
			interval = 2000;

		GetGame().GetCallqueue().CallLater(AmbientTick, interval, true);
		m_bAmbientTickRunning = true;
	}

	//------------------------------------------------------------------------------------------------
	//! The ambient round-robin (T4.3).
	//!
	//! WHY A SLICE AND NOT THE WHOLE LIST. Every source evaluated costs an ObserversSystem query and,
	//! when it is filling, up to m_iAmbientSpawnsPerTick prefab spawns. A town's worth of sources
	//! evaluated every 2 s would put all of that on one frame; a slice of 8 spreads it, and the cursor
	//! guarantees fairness - with 40 sources and a slice of 8, every source is evaluated once per 10 s
	//! and none can starve.
	//!
	//! The slice is resolved to INSTANCES before any of them is evaluated, because a config hook
	//! (OnEntitySpawned) may legally unregister a source mid-slice - iterating the order array by
	//! index while it changes underneath would skip or double-evaluate its neighbours.
	protected void AmbientTick()
	{
		// SELF-CANCEL FOR A DEAD WORLD (R7). The call queue outlives a campaign, so a repeating entry
		// whose OnDelete never ran would keep ticking a manager from the previous world - and once the
		// next campaign's manager installs its own tick, both would run and every ambient budget would
		// be spent twice. The static is the arbiter: exactly one manager can be s_Instance.
		if (s_Instance != this)
		{
			ScriptCallQueue queue;
			if (GetGame())
				queue = GetGame().GetCallqueue();

			if (queue)
				queue.Remove(AmbientTick);

			m_bAmbientTickRunning = false;
			return;
		}

		// AI OBSERVERS RIDE THIS TICK (integration Phase 6). Deliberately ABOVE the "no sources"
		// early-out below: the two halves are unrelated, and a manager with observers but no ambient
		// source would otherwise never sweep. See SweepEntityObservers for what it is for.
		SweepEntityObservers();

		if (!m_mAmbientSources || !m_aAmbientOrder || m_aAmbientOrder.IsEmpty())
			return;

		ObserversSystem observers = GetObserversSystem();
		if (!observers)
			return;

		array<int> slice = OVT_VirtualizationMath.SliceIndices(m_aAmbientOrder.Count(), m_iAmbientSourcesPerTick, m_iAmbientCursor);
		m_iAmbientCursor = OVT_VirtualizationMath.AdvanceCursor(m_iAmbientCursor, m_iAmbientSourcesPerTick, m_aAmbientOrder.Count());

		array<int> handles = new array<int>();
		foreach (int index : slice)
		{
			handles.Insert(m_aAmbientOrder[index]);
		}

		foreach (int handle : handles)
		{
			// Re-resolved per source: the previous source's hooks may have removed this one.
			OVT_AmbientSpawnSourceInstance instance = m_mAmbientSources.Get(handle);
			if (!instance)
				continue;

			EvaluateAmbientSource(instance, observers);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One source's proximity decision and its share of this tick's spawn budget.
	//!
	//! THE TWO RINGS. Spawn ring = the source's own override, or the manager's global spawn distance
	//! when it declines to choose. Despawn ring = spawn x m_fAmbientDespawnHysteresis, and the gap
	//! between them is the anti-thrash band: an observer standing exactly on the spawn ring would
	//! otherwise spawn and despawn a crowd every 2 s forever. Activation asks the SPAWN ring, despawn
	//! asks the wider DESPAWN ring, so once a source is up an observer has to walk further out than
	//! they walked in to take it down.
	//!
	//! A resolved spawn ring of 0 is the documented "never materialise by proximity" source: it never
	//! activates, and one already up is taken down.
	//! \param[in] instance The source to evaluate.
	//! \param[in] observers The engine's observer set.
	protected void EvaluateAmbientSource(notnull OVT_AmbientSpawnSourceInstance instance, notnull ObserversSystem observers)
	{
		OVT_AmbientSpawnSourceConfig config = instance.m_Config;
		if (!config)
			return;

		// T4.7: stale refs are dropped BEFORE anything reads the list, so neither the despawn path nor
		// a consumer's count ever touches a deleted entity.
		PruneAmbientEntities(instance);

		int spawnDistance = OVT_VirtualizationMath.ResolveSpawnDistance(config.m_iSpawnDistanceOverride, GetGlobalSpawnDistance());
		if (spawnDistance <= 0)
		{
			if (instance.m_bActive)
				DespawnAmbientSource(instance);

			return;
		}

		int despawnDistance = OVT_VirtualizationMath.ResolveDespawnDistance(spawnDistance, m_fAmbientDespawnHysteresis);

		// Squared in FLOAT deliberately: a "keep this always spawned" source registered with a huge
		// ring would overflow an int square and wrap negative, which reads as "nobody is ever near".
		float x = instance.m_vPosition[0];
		float z = instance.m_vPosition[2];

		if (!instance.m_bActive)
		{
			float spawnRangeSq = spawnDistance;
			spawnRangeSq = spawnRangeSq * spawnRangeSq;

			if (!observers.HasObserverWithinRangeSq(x, z, spawnRangeSq))
				return;

			// T4.4: the count is rolled ONCE here, not per tick - a source that rolled 12 civilians
			// owes 12 for this whole activation, however many ticks it takes to build them.
			instance.BeginActivation(config.RollCount());

			if (m_bDebugAmbientLogging)
				PrintFormat("[Overthrow] Virtualization AMBIENT: source '%1' (handle %2) ACTIVATED at %3 - rolled %4 entities, %5 per tick",
					config.m_sSourceName, instance.m_iHandle.ToString(), instance.m_vPosition.ToString(),
					instance.m_iRollTarget.ToString(), m_iAmbientSpawnsPerTick.ToString());
		}
		else
		{
			float despawnRangeSq = despawnDistance;
			despawnRangeSq = despawnRangeSq * despawnRangeSq;

			if (!observers.HasObserverWithinRangeSq(x, z, despawnRangeSq))
			{
				DespawnAmbientSource(instance);
				return;
			}
		}

		SpendAmbientSpawnBudget(instance, config);
	}

	//------------------------------------------------------------------------------------------------
	//! Builds up to m_iAmbientSpawnsPerTick of a source's outstanding entities (T4.4).
	//!
	//! This is the anti-hitch rule the acceptance criteria name: a source that rolled 20 civilians
	//! must not spawn 20 in one frame. With the default budget of 3 and a 2 s tick, 20 civilians
	//! materialise over ~14 s of the player's approach.
	//! \param[in] instance The activated source.
	//! \param[in] config Its config.
	protected void SpendAmbientSpawnBudget(notnull OVT_AmbientSpawnSourceInstance instance, notnull OVT_AmbientSpawnSourceConfig config)
	{
		if (!instance.HasPendingSpawns())
			return;

		int budget = m_iAmbientSpawnsPerTick;
		if (budget <= 0)
			budget = 1;

		int built = 0;

		while (budget > 0 && instance.HasPendingSpawns())
		{
			budget--;

			// The cursor advances whether or not the entity was built: a source whose prefab list is
			// empty or whose prefab fails to spawn must finish its activation rather than retry the
			// same failure every 2 s forever.
			instance.m_iSpawnCursor++;

			if (SpawnAmbientEntity(instance, config))
				built++;
		}

		if (m_bDebugAmbientLogging)
			PrintFormat("[Overthrow] Virtualization AMBIENT: source '%1' (handle %2) spawned %3 this tick - %4 of %5 done",
				config.m_sSourceName, instance.m_iHandle.ToString(), built.ToString(),
				instance.m_iSpawnCursor.ToString(), instance.m_iRollTarget.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! Rolls and builds ONE ambient entity, records it and runs the config's dressing hook.
	//! \param[in] instance The source that owns it.
	//! \param[in] config Its config.
	//! \return The entity, or null when it could not be built.
	protected IEntity SpawnAmbientEntity(notnull OVT_AmbientSpawnSourceInstance instance, notnull OVT_AmbientSpawnSourceConfig config)
	{
		ResourceName prefab = config.RollPrefab();
		if (prefab == ResourceName.Empty)
		{
			Print(string.Format("[Overthrow] Virtualization: ambient source '%1' (handle %2) rolled no prefab - it has none authored",
				config.m_sSourceName, instance.m_iHandle.ToString()), LogLevel.WARNING);
			return null;
		}

		vector position = config.RollPosition(instance.m_vPosition, config.m_fRadius);

		IEntity entity = OVT_Global.SpawnEntityPrefab(prefab, position);
		if (!entity)
		{
			Print(string.Format("[Overthrow] Virtualization: ambient source '%1' could not spawn prefab '%2'",
				config.m_sSourceName, prefab), LogLevel.WARNING);
			return null;
		}

		// Recorded BEFORE the hook runs, so a hook that immediately releases the entity (a source that
		// hands its spawns to another system) finds it in the reverse map.
		instance.m_aEntities.Insert(entity);
		m_mAmbientByEntity.Insert(entity.GetID(), instance.m_iHandle);

		config.OnEntitySpawned(entity, instance.m_vPosition);

		return entity;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes a source down (T4.5): hook, delete, forget.
	//!
	//! NO STATE IS KEPT. Not which prefabs were rolled, not how many survived, not where they stood -
	//! the next approach re-rolls from config. That is the ambient contract and the reason nothing
	//! ambient is ever persisted; a consumer that needs an entity to survive must have released it
	//! (ReleaseAmbientEntity), and a released entity is not in this list to begin with.
	//! \param[in] instance The source to take down.
	protected void DespawnAmbientSource(notnull OVT_AmbientSpawnSourceInstance instance)
	{
		OVT_AmbientSpawnSourceConfig config = instance.m_Config;

		if (m_bDebugAmbientLogging && instance.m_aEntities)
		{
			string sourceName;
			if (config)
				sourceName = config.m_sSourceName;

			PrintFormat("[Overthrow] Virtualization AMBIENT: source '%1' (handle %2) DESPAWNING %3 entities - the roll is discarded, the next approach re-rolls",
				sourceName, instance.m_iHandle.ToString(), instance.m_aEntities.Count().ToString());
		}

		if (instance.m_aEntities)
		{
			// Iterated over a SNAPSHOT: OnEntityDespawning is consumer code and may legally call
			// ReleaseAmbientEntity, which mutates the very list this loop walks.
			array<IEntity> doomed = new array<IEntity>();
			doomed.Copy(instance.m_aEntities);

			foreach (IEntity entity : doomed)
			{
				if (!entity)
					continue;

				// Re-checked against the LIVE list: an earlier hook may have released this one, and a
				// released entity is exactly the thing a despawn must not delete.
				if (instance.m_aEntities.Find(entity) == -1)
					continue;

				// The hook runs while the entity is STILL owned, so releasing from inside
				// OnEntityDespawning works - that is the documented "save this one" escape hatch, and
				// it would fail silently if the reverse-map entry had already been dropped.
				if (config)
					config.OnEntityDespawning(entity);

				if (instance.m_aEntities.Find(entity) == -1)
					continue;

				if (m_mAmbientByEntity)
					m_mAmbientByEntity.Remove(entity.GetID());

				SCR_EntityHelper.DeleteEntityAndChildren(entity);
			}
		}

		instance.ResetActivation();
	}

	//------------------------------------------------------------------------------------------------
	//! Drops entities that no longer exist, or that exist as a corpse/wreck, from a source's list
	//! (T4.7).
	//!
	//! WHY IT MATTERS. Without it a source's list grows a null for every civilian a player shot, a
	//! consumer counting GetAmbientEntities() counts corpses as live crowd, and the reverse map keeps
	//! entries under EntityIDs the engine is free to hand out again. (A stale map entry cannot produce
	//! a wrong release on its own - ReleaseAmbientEntity re-verifies against the source's list - but
	//! it is still a leak that grows for the life of the source.)
	//!
	//! A dead body is deliberately pruned rather than deleted: deleting corpses out from under a
	//! player who is looting one is a worse bug than a body that outlives its source. The source stops
	//! owning it, exactly as a release would.
	//!
	//! TWO CONFIG HOOKS RUN FROM HERE (`civilians` T1.1/T1.2, additive 2026-08-17): IsEntityDead() is
	//! the predicate, and OnEntityPruned() fires AFTER both removals so a consumer can delete the
	//! pruned entity's companions - waypoints, an emptied group husk - while the body itself stays.
	//! \param[in] instance The source to prune.
	protected void PruneAmbientEntities(notnull OVT_AmbientSpawnSourceInstance instance)
	{
		if (!instance.m_aEntities || instance.m_aEntities.IsEmpty())
			return;

		OVT_AmbientSpawnSourceConfig config = instance.m_Config;

		// Backwards, because removing shifts everything after the removed index down one.
		for (int i = instance.m_aEntities.Count() - 1; i >= 0; i--)
		{
			IEntity entity = instance.m_aEntities[i];

			if (!entity)
			{
				instance.m_aEntities.Remove(i);
				continue;
			}

			// T1.1 (`civilians`): the CONFIG owns the predicate. The base class's default IS this
			// method's body, moved there unchanged, so a source that does not override it prunes
			// exactly as it did before the hook existed. IsAmbientEntityDead stays as the fallback for
			// the one state that has no config to ask - an instance whose config went away.
			bool dead;
			if (config)
				dead = config.IsEntityDead(entity);
			else
				dead = IsAmbientEntityDead(entity);

			if (!dead)
				continue;

			if (m_mAmbientByEntity)
				m_mAmbientByEntity.Remove(entity.GetID());

			instance.m_aEntities.Remove(i);

			// T1.2 (`civilians`): AFTER both removals, never before - the hook may not be handed an
			// entity core still believes it owns, because a consumer's first move is usually to delete
			// the entity's companions and a half-owned entity would then be walked by the next despawn.
			if (!config)
				continue;

			config.OnEntityPruned(entity);

			// A hook is allowed to touch the world, and touching the world can touch this list (a
			// companion of its own that is itself ambient, or a ReleaseAmbientEntity call). The
			// backwards walk only survives a shrink if the cursor is pulled back inside the array.
			if (i > instance.m_aEntities.Count())
				i = instance.m_aEntities.Count();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The FALLBACK dead-check, kept for the one state that has no config to ask (an instance whose
	//! config reference went away). The overridable copy of this exact body lives on
	//! OVT_AmbientSpawnSourceConfig.IsEntityDead() and is what PruneAmbientEntities normally calls.
	//! \param[in] entity An ambient entity.
	//! \return True when the entity is destroyed (a body, a wreck) and should stop being counted as
	//!         one of its source's live spawns.
	protected bool IsAmbientEntityDead(notnull IEntity entity)
	{
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(entity.FindComponent(SCR_DamageManagerComponent));
		if (!damage)
			return false;

		return damage.GetState() == EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	//! The engine's observer set - the ONLY proximity source this feature uses (D11).
	//! \return The system, or null when the world has none (nothing ambient can tick without it).
	protected ObserversSystem GetObserversSystem()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return null;

		// FindSystem lives on ChimeraWorld, not BaseWorld - the cast is the access, not a safety net.
		ChimeraWorld world = ChimeraWorld.CastFrom(owner.GetWorld());
		if (!world)
			return null;

		return ObserversSystem.Cast(world.FindSystem(ObserversSystem));
	}

	//------------------------------------------------------------------------------------------------
	// AI OBSERVERS (virtualization/integration Phase 6 - D14/D16)
	//
	// WHAT THIS IS FOR. The engine materialises a dormant registered group when an OBSERVER comes
	// inside its spawn ring, and an observer is NOT a player: local cameras, MP inserts and the optics
	// far observer all count (D2 finding 3). These four methods let Overthrow park one of its own,
	// FOLLOWING AN ENTITY, so something that is not a player can pull content awake. The first
	// consumer is a squad of parked recruits left alone in a town.
	//
	// ⚠ THIS IS THE MOST EXPENSIVE THING IN THE FILE, PER CALL. One observer holds EVERY registered
	// group inside its ring materialised for as long as it exists - a squad parked in a town keeps
	// that town's tower garrison and its patrols spawned, with their AI running, whether or not
	// anybody is there to watch them. That is the feature and it is the budget risk in one sentence,
	// which is why the consumer is expected to carry an off-switch (m_bRecruitGroupsAreObservers is
	// the parked-recruit group's, read by that group's own marker component - core never applies it
	// to anybody else's AddEntityObserver call).
	//
	// ⚠ APPLICATION IS DEFERRED BY ONE FRAME, INSERT AND REMOVE. Measured 2026-08-17 by the Phase 1
	// gate case (integration T1.9): "server-side InsertObserverSP(key, 0, 0, entity) is HONOURED,
	// application DEFERRED (invisible same-frame, visible within the settling budget) | settled after
	// 1 frame(s), removal settled after 1". So HasEntityObserver and GetEntityObserverCount answer
	// from m_mEntityObservers and NEVER from the engine: an engine query taken right after an add
	// reads a false negative, and one taken right after a remove reads a false leak - the first
	// version of the gate case went red on exactly that.
	//
	// ⚠ NEVER A NULL ENTITY. InsertObserverSP(key, x, z, null) - the documented fixed-position form -
	// hard-FROZE the game client the one time a test case parked one (core context.md gotcha 0), and
	// it has zero vanilla script callers in the whole 1.8 tree. AddEntityObserver refuses null before
	// it does anything else, and the guard is repeated immediately above the call itself so a later
	// edit cannot separate them.
	//
	// KEYS are a namespaced monotonic counter (ENTITY_OBSERVER_KEY_BASE and up). The SP key space has
	// no vanilla script user at all, so a key only has to avoid Overthrow's own - the Phase 1 gate
	// case owns 770019. They are session-local by construction: NEVER persisted, NEVER replicated. A
	// saved key would name nothing on load, and a key sent to a client would name a different
	// machine's observer table.
	//------------------------------------------------------------------------------------------------

	//! First SP observer key this manager hands out; it counts up from here for the life of the
	//! component. Deliberately far above the Phase 1 gate case's 770019, so a spike run and a live
	//! campaign in the same process cannot collide on one key.
	static const int ENTITY_OBSERVER_KEY_BASE = 771000;

	//------------------------------------------------------------------------------------------------
	//! Parks an engine observer that FOLLOWS an entity, so registered groups near it materialise even
	//! with no player anywhere near. Server-only, idempotent per entity.
	//!
	//! Zero offsets are passed, so the observer IS the entity's own position and moves with it - there
	//! is nothing to keep updated and nothing to re-park when the entity walks away.
	//!
	//! ⚠ REFUSES A NULL ENTITY with a WARNING (it would hard-freeze the client - see the section
	//! header), and REFUSES AN ENTITY WITH AN INVALID EntityID with a WARNING (it cannot be keyed -
	//! see the note in the body). ⚠ The effect is not visible to the engine's own proximity queries
	//! until the NEXT frame; HasEntityObserver below is unaffected, because it reads core's map.
	//! \param[in] entity The entity to follow. Never null, and world-registered.
	//! \return True when the entity has an observer on it after this call - including when it already
	//!         had one, in which case the same key is reused and nothing is leaked.
	bool AddEntityObserver(IEntity entity)
	{
		if (!entity)
		{
			Print("[Overthrow] Virtualization: AddEntityObserver refused a NULL entity - a null-entity InsertObserverSP hard-freezes the client (core context.md gotcha 0). Nothing was parked", LogLevel.WARNING);
			return false;
		}

		if (!Replication.IsServer())
			return false;

		// Null on a client and before OnPostInit - the same fail-closed shape as every other entry
		// point in this file.
		if (!m_mEntityObservers)
			return false;

		EntityID id = entity.GetID();

		// ⚠ THE SECOND REFUSAL, AND IT IS NOT DEFENSIVE PADDING - it is a measured hazard (integration
		// Phase 6 suite run, 2026-08-17). An entity that is not world-registered answers GetID() with
		// EntityID.INVALID, and EVERY such entity answers with the SAME value: keying the map on it
		// makes two unrelated entities share one entry, so the second AddEntityObserver silently
		// hijacks the first one's key, HasEntityObserver answers true for something nobody added, and
		// removing either one removes the other's observer. That is a leak and a wrong-answer bug at
		// once, and it is completely invisible - which is why an unkeyable entity is refused loudly
		// instead of stored. The caller's fix is to hand this an entity that exists in the world (a
		// spawn that has completed, not one still inside its own init).
		if (id == EntityID.INVALID)
		{
			Print("[Overthrow] Virtualization: AddEntityObserver refused an entity with an INVALID EntityID - it is not world-registered yet, cannot be keyed, and would collide in the observer map with every other unregistered entity. Nothing was parked; add the observer once the entity exists in the world", LogLevel.WARNING);
			return false;
		}

		ObserversSystem observers = GetObserversSystem();
		if (!observers)
		{
			Print("[Overthrow] Virtualization: AddEntityObserver found no ObserversSystem in this world - nothing will ever be pulled awake by an entity observer here", LogLevel.WARNING);
			return false;
		}

		// IDEMPOTENT PER ENTITY. An existing entry keeps its key and is re-inserted rather than
		// skipped: the engine header documents the key as "insert or UPDATE", the gate case measured
		// two inserts on one key settling at +1 (never +2), and re-inserting re-points the key at the
		// entity handed in - which is the correct answer in the one case a stale entry could survive
		// the sweep below, an EntityID the engine recycled for a different entity.
		int key;
		if (!m_mEntityObservers.Find(id, key))
		{
			key = m_iNextObserverKey;
			m_iNextObserverKey++;
		}

		// ⚠ THE ONLY InsertObserverSP CALL SITE IN THE MOD, and this guard is not redundant paranoia:
		// a null here FREEZES the client outright (core context.md gotcha 0), so the check lives
		// next to the call as well as at the top of the method, where a later edit could separate the
		// two. Zero offsets = follow the entity.
		if (!entity)
			return false;

		observers.InsertObserverSP(key, 0, 0, entity);

		// Booked after the engine call, so the map can never claim an observer that was not asked for.
		m_mEntityObservers.Set(id, key);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the observer following an entity.
	//!
	//! ⚠ Like the insert, the removal is applied by the engine on the NEXT frame - core's own view
	//! (HasEntityObserver / GetEntityObserverCount) is correct immediately, the engine's proximity
	//! queries lag by one frame. That is a false LEAK if you go looking for it, not a real one.
	//! \param[in] entity The entity that was being followed.
	//! \return True when an observer was removed; false for null, for an invalid EntityID, for an
	//!         unknown entity and on a client. All four are safe, silent no-ops.
	bool RemoveEntityObserver(IEntity entity)
	{
		if (!entity)
			return false;

		if (!m_mEntityObservers)
			return false;

		EntityID id = entity.GetID();

		// Quiet here, loud in Add. An unkeyable entity can never have had an observer parked for it
		// (Add refuses it), so this is an ordinary "no", not a caller error - and answering it from
		// the map would mean looking up the shared INVALID key and removing somebody else's observer.
		if (id == EntityID.INVALID)
			return false;

		return RemoveEntityObserverById(id);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether an entity currently has one of core's observers on it.
	//!
	//! ⚠ ANSWERED FROM CORE'S OWN MAP, never from the engine, because engine application is deferred
	//! by a frame in both directions (see the section header). This is therefore true the instant
	//! AddEntityObserver returns and false the instant RemoveEntityObserver returns, which is what a
	//! consumer's bookkeeping actually needs.
	//! \param[in] entity The entity to ask about.
	//! \return True when core parked an observer on it and has not removed or swept it. False for
	//!         null, for an entity with an invalid EntityID, and on a client.
	bool HasEntityObserver(IEntity entity)
	{
		if (!entity)
			return false;

		if (!m_mEntityObservers)
			return false;

		EntityID id = entity.GetID();

		// The same refusal as Add, and for the same reason: every unregistered entity shares one
		// EntityID value, so answering this from the map would report "yes" for an entity nobody ever
		// added an observer for as soon as any other unregistered entity had been refused... or, worse,
		// stored. Refusing to key on it is what makes the map's answer trustworthy.
		if (id == EntityID.INVALID)
			return false;

		return m_mEntityObservers.Contains(id);
	}

	//------------------------------------------------------------------------------------------------
	//! How many entity observers core is currently holding.
	//!
	//! Core's count, not the engine's: GetObserversSP() also contains cameras, the deploy-point MP
	//! preload insert and anything else the session parked, none of which is ours to count.
	//! \return The count; 0 on a client and before initialisation.
	int GetEntityObserverCount()
	{
		if (!m_mEntityObservers)
			return 0;

		return m_mEntityObservers.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a player's PARKED RECRUIT GROUP should act as an AI observer (D16).
	//!
	//! The operator's off-switch, read by the parked group's own marker component. It is deliberately
	//! NOT consulted by AddEntityObserver: that method serves every consumer, and one consumer's knob
	//! must not silently disable another's.
	//! \return The authored attribute; true as shipped.
	bool GetRecruitGroupsAreObservers()
	{
		return m_bRecruitGroupsAreObservers;
	}

	//------------------------------------------------------------------------------------------------
	//! The removal both the public method and the stale sweep funnel through.
	//!
	//! The map entry is dropped even when the ObserversSystem cannot be resolved (a world already
	//! going away): keeping it would only make this manager claim an observer it can no longer name.
	//! \param[in] id The followed entity's id.
	//! \return True when an entry existed and has now gone.
	protected bool RemoveEntityObserverById(EntityID id)
	{
		// Both callers already guard, but "every accessor in this file is null-safe" is the rule the
		// whole component is written to, and a future third caller should not have to know.
		if (!m_mEntityObservers)
			return false;

		int key;
		if (!m_mEntityObservers.Find(id, key))
			return false;

		ObserversSystem observers = GetObserversSystem();
		if (observers)
			observers.RemoveObserverSP(key);

		m_mEntityObservers.Remove(id);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops observers whose entity no longer exists. Folded into the ambient tick (2 s).
	//!
	//! WHY IT IS NEEDED AT ALL. A consumer removes its own observer from the marker component's
	//! OnDelete, which covers every route an entity is normally destroyed by. This is the backstop for
	//! the routes it does not: a consumer that forgets, an entity destroyed by something that never
	//! runs script, or a component whose OnDelete could not reach the manager. The cost of missing one
	//! is an observer standing over an empty patch of map, holding everything near it spawned, for the
	//! rest of the session - which is exactly the failure the whole section is written to avoid.
	//!
	//! The stale ids are collected before any of them is removed: RemoveEntityObserverById writes to
	//! the map this walks.
	protected void SweepEntityObservers()
	{
		if (!m_mEntityObservers || m_mEntityObservers.IsEmpty())
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		BaseWorld world = owner.GetWorld();
		if (!world)
			return;

		array<EntityID> stale = new array<EntityID>();

		foreach (EntityID id, int key : m_mEntityObservers)
		{
			if (!world.FindEntityByID(id))
				stale.Insert(id);
		}

		foreach (EntityID id : stale)
		{
			RemoveEntityObserverById(id);

			Print("[Overthrow] Virtualization: swept an entity observer whose entity is gone - its owner did not remove it", LogLevel.VERBOSE);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Removes EVERY entity observer. World teardown only (OnDelete).
	//!
	//! ⚠ The one destructive act in this component's teardown, and it has to be: an SP observer is
	//! engine state keyed by an integer only this component holds, so one left behind survives the
	//! world, survives a Continue and keeps whatever it stands over materialised for the rest of the
	//! PROCESS, with nothing left able to name the key that would remove it.
	protected void RemoveAllEntityObservers()
	{
		if (!m_mEntityObservers)
			return;

		ObserversSystem observers = GetObserversSystem();
		if (!observers)
		{
			// No world left to remove them from - the observers went with it. What must not survive is
			// this component's script state, and that is what the clear is for.
			m_mEntityObservers.Clear();
			return;
		}

		foreach (EntityID id, int key : m_mEntityObservers)
		{
			observers.RemoveObserverSP(key);
		}

		m_mEntityObservers.Clear();
	}
}
