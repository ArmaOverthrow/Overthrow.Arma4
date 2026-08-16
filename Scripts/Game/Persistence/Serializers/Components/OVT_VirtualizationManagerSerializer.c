//------------------------------------------------------------------------------------------------
//! One virtualized AI group's persisted state - EVERYTHING needed to RE-CREATE it.
//!
//! A DEDICATED RECORD, NOT OVT_VirtualGroupRecord ITSELF. The live class carries runtime-only members
//! (the SCR_AIGroup pointer, its EntityID, the owned AIWaypoint entities, the dormant-teardown flag),
//! none of which mean anything in a save, and its shape is free to change with gameplay work; a save
//! format must not be. Writing an explicit record also makes the field ORDER visible in one place,
//! which is what binary contexts key off.
//!
//! ⚠ THE FIELD ORDER BELOW IS THE BINARY FORMAT AND IS FROZEN. Binary contexts are POSITIONAL: the
//! order these members are declared in is the order they are written and read. New fields are APPENDED
//! at the end behind a version bump, never inserted, never reordered, never removed.
//!
//! WHY THE PAYLOAD IS THIS BIG - ROUTE B. The original design (implementation.md §3.6) persisted only
//! bookkeeping and let vanilla's SCR_AIGroupSerializer bring the group entities back, relinking by
//! UUID. Phases 1 and 3 proved that unbuildable: a runtime-spawned group can only be granted
//! `SelfSpawn` by a .conf rule, rules are matched with ENGINE-NATIVE matchers only (a scripted
//! IsMatch is never consulted - BUG-018), the narrowest native matcher is EntityClass "AIGroup" - a
//! superset that also catches every AI group whose transient untrack the retry queue gave up on, so
//! granting it class-wide would duplicate garrisons and patrols on every load - and the per-instance
//! alternative MarkForSelfSpawn is save-corrupting (BUG-116). So core persists complete re-creation
//! state and OVT_VirtualizationManagerComponent.ApplyPersistedRegistry() rebuilds the group entities
//! itself. Every field here is something that rebuild needs.
//!
//! FACTION IS A KEY, NEVER AN INDEX (D3). Faction indices are positional - whatever order the faction
//! manager happened to register factions in - so an index that meant "USSR" in one session names
//! something else in the next. The registry NAME and the resolved PREFAB are both stored as well: they
//! are the second and third steps of the load-time resolution, so a renamed registry entry or a
//! removed faction mod degrades to "spawn what it was" and finally to "drop it with a warning", never
//! to "spawn the wrong thing".
//!
//! THE SURVIVOR MASK IS THE ROSTER TRUTH (D2) and travels as a parallel int array, one entry per prefab
//! slot, non-zero = alive. It is what makes "a group that lost 3 of 8 comes back with exactly its 5
//! surviving slots" survive a save and not just a despawn. Member CHARACTERS are deliberately not
//! persisted: the mask is the roster, and persisting both would double-count it on load.
//!
//! THE WAYPOINT PLAN TRAVELS AS THREE PARALLEL ARRAYS plus the cycle flag - the same shape
//! OVT_VirtualWaypointPlan uses in memory, flattened so that a plan whose arrays no longer line up can
//! be dropped on load without leaving a half-built object behind. The waypoint ENTITIES are not
//! persisted (they are tracked but `SelfSpawn 0`, and must stay that way - every legacy Overthrow
//! spawner builds waypoints through the same helpers, so a self-spawn rule would resurrect every
//! garrison and patrol waypoint in the save); they are rebuilt from this plan.
//------------------------------------------------------------------------------------------------
class OVT_PersistedVirtualGroup
{
	//! Stable identity. This is the MATCH KEY - records are re-applied by handle, in place.
	int handle;

	string ownerSystem;
	string ownerKey;

	string factionKey;
	string groupRegistryName;
	ResourceName resolvedPrefab;

	int spawnDistanceOverride;
	int importance;

	//! The group entity's origin AT SAVE TIME, not the position it was registered at - `movement`
	//! advances dormant group entities, so the registration position goes stale.
	vector position;

	//! 1/0 per prefab roster slot.
	ref array<int> slotAlive = {};

	ref array<vector> waypointPositions = {};
	ref array<int> waypointTypes = {};
	ref array<float> waypointParams = {};
	bool waypointCycle;
}

//------------------------------------------------------------------------------------------------
//! Persists the virtualization registry: which virtual AI groups exist, who owns them, how they were
//! composed, where they are, which of their roster slots are still alive, and what waypoints they run.
//!
//! BINDING. Listed in the ComponentSerializers block of the game-mode configuration in
//! Configs/Systems/Persistence/Overthrow.conf.
//!
//! THIS FILE IS A PURE CODEC. It reads through OVT_VirtualizationManagerComponent.SnapshotRegistry()
//! and writes through ApplyPersistedRegistry(); every side effect of restoring a registry - re-creating
//! the group entities, re-stamping the engine lifecycle, rebuilding the waypoints, pushing the survivor
//! mask, dropping records whose composition no longer resolves - lives on the manager, where it can be
//! read next to the code that maintains the same invariants at runtime.
//!
//! ROUTE B: THE GROUPS IN THIS PAYLOAD ARE RE-CREATED BY OVERTHROW, NOT RESTORED BY VANILLA.
//! No SCR_AIGroup, AIUnit or AIWaypoint record is written for a virtual group - Overthrow untracks all
//! three unconditionally (Modded/SCR_AIGroup.c, the BUG-118 fix) and their persistence configs are
//! scoped `SelfSpawn 0` in the same Overthrow.conf. The manager's per-group tracking exemption exists
//! (Phase 3) but is switched OFF (m_bPersistGroupEntities), because tracking without a self-spawn grant
//! writes a record nothing can ever claim, and self-spawn cannot be granted narrowly enough - see the
//! record class above and api.md §8. Consequence worth stating plainly: this component serializer is
//! the ONLY thing that remembers a virtual group across a save, so a field missing here is a field the
//! campaign loses.
//!
//! AMBIENT SPAWN SOURCES ARE ABSENT BY CONSTRUCTION, and this is the assertion (T5.5). They are
//! transient by design - a despawn discards the roll and the next approach re-rolls from config - and
//! they live in collections this file does not name, under a handle counter (m_iNextAmbientHandle) that
//! is deliberately separate from the persisted one. Nothing ambient can reach this payload without a
//! new field being added above, which is a deliberate act.
//!
//! IDEMPOTENT ON A LIVE SESSION. Deserialize also runs when saved data is re-applied to a running
//! campaign (OVT_PersistenceManagerComponent.ReapplyLatestSaveData). ApplyPersistedRegistry matches by
//! handle, updates in place, re-creates only what is missing and unregisters what the payload does not
//! claim, so a second pass produces exactly the same registry as the first.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first, then the
//! handle counter, then the records. Append-only forever.
//------------------------------------------------------------------------------------------------
class OVT_VirtualizationManagerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_VirtualizationManagerComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the handle counter and one record per restorable virtual group.
	//! \param[in] owner The game mode entity owning the virtualization manager.
	//! \param[in] component The virtualization manager being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow virtualization manager.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_VirtualizationManagerComponent.Cast(component);
		if (!virtualization)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		// The counter is written even when there are no records: handles must never be reused across a
		// reload, and a campaign whose groups were all wiped still has to remember how far it counted.
		context.WriteValue("nextHandle", virtualization.GetNextHandle());

		array<ref OVT_PersistedVirtualGroup> records = new array<ref OVT_PersistedVirtualGroup>();
		virtualization.SnapshotRegistry(records);

		context.Write(records);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the registry back and hands it to the manager to apply.
	//! \param[in] owner The game mode entity owning the virtualization manager.
	//! \param[in] component The virtualization manager being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_VirtualizationManagerComponent.Cast(component);
		if (!virtualization)
			return false;

		// No version means this component has no payload in the stored record - a save written before
		// this serializer existed. Applying what would be read out of nothing (an empty registry and a
		// zeroed handle counter) would UNREGISTER every group the running campaign has, so every
		// serializer bails here. See OVT_TownManagerSerializer.Deserialize().
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		int nextHandle;
		context.ReadValue("nextHandle", nextHandle);

		array<ref OVT_PersistedVirtualGroup> records = new array<ref OVT_PersistedVirtualGroup>();
		context.Read(records);

		virtualization.ApplyPersistedRegistry(records, nextHandle);

		return true;
	}
}
