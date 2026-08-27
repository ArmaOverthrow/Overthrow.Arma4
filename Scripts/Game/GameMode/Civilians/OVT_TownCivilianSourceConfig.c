//------------------------------------------------------------------------------------------------
//! Everything ONE ambient civilian owns, so that all of it can die with it (implementation.md §3.3).
//!
//! WHY IT EXISTS. The tracked entity is a one-man SCR_AIGroup, but a civilian is really four things:
//! the group entity, the member character the engine's spawn queue puts in it, the AIWaypoint
//! entities its route is made of, and the fact that a member has ever existed. Only the first of
//! those is visible to core. The other three live here, keyed by the group's EntityID, which is why
//! nothing this feature creates can outlive the civilian it was created for (quality bar Q4/Q5).
//------------------------------------------------------------------------------------------------
class OVT_AmbientCivilianRecord : Managed
{
	//! The group entity this record is about. The map key too - held as a field so a record pulled out
	//! of the map on its own still knows what it belongs to.
	EntityID m_GroupId;

	//! Has a member EVER materialised in this group?
	//!
	//! ⚠ THE DEAD-CHECK'S PRECONDITION, AND IT IS NOT OPTIONAL. Member spawning goes through the
	//! engine's importance-ordered queue in 1.8, so a group that was built this frame is legitimately
	//! memberless for one or more frames. A naive "no agents means dead" predicate would prune every
	//! civilian the tick after it was created and a town would never fill. Set from GetOnAgentAdded().
	bool m_bSeenAgent;

	//! EVERY AIWaypoint entity built for this civilian - the route legs AND the cycle that drives them.
	//! Deleted together with the civilian on despawn, on prune and on release; nothing else deletes
	//! them, and nothing else may.
	ref array<AIWaypoint> m_aWaypoints;

	//! Which authored civilian type this one was rolled from (diagnostics, and Phase 4's per-type
	//! clothing).
	string m_sTypeName;

	//------------------------------------------------------------------------------------------------
	void OVT_AmbientCivilianRecord()
	{
		m_aWaypoints = new array<AIWaypoint>();
	}
}

//------------------------------------------------------------------------------------------------
//! ONE TOWN'S CROWD - the runtime per-town instance of the authored template (implementation.md §3.3,
//! §3.5). This is the "controller" of the civilians feature in all but name.
//!
//! WHAT IT IS. core's seam takes a config object, not a component, so per-town state has to live on
//! a config: RollCount() takes no arguments and could not otherwise know which town it is being asked
//! about. One of these is built per activated town by OVT_CivilianAmbienceManagerComponent and handed
//! to RegisterAmbientSource(); core owns the proximity decision, the anti-thrash band, the once-per-
//! activation roll and the ≤3-per-tick fill, and calls the seven methods below.
//!
//! THE TEMPLATE IS READ BY REFERENCE, NEVER COPIED (decision D5). Only the base-class fields core
//! reads DIRECTLY are set on this instance - m_sSourceName, m_fRadius (the town's range),
//! m_iSpawnDistanceOverride and the m_iMinCount/m_iMaxCount density clamps. Everything else is read
//! through m_Template, so a template that gains an attribute cannot silently go missing here.
//!
//! ⚠ SERVER ONLY, AND NEVER PERSISTED (decision D9). Nothing here replicates and nothing here is
//! written to a save: a despawn discards the whole crowd and the next approach re-rolls it. Do not
//! give this object state that has to survive a despawn - if one civilian must survive, it is
//! released out of ambient ownership (ReleaseAmbientEntity) and stops being this object's business.
//!
//! THE THREE LIFETIME RULES, because getting any of them wrong leaks entities for the session:
//!  1. A GROUP HUSK MUST OUTLIVE ITS MEMBER. Vanilla SCR_AIGroup deletes an emptied group ~1 ms after
//!     its last member dies (m_bDeleteWhenEmpty defaults ON, SCR_AIGroup.c OnEmpty). If that ran, the
//!     tracked entity would be gone before the next 2 s prune, core would drop a plain null WITHOUT
//!     calling OnEntityPruned, and this civilian's five waypoint entities would leak forever - which
//!     is exactly what the retired spawner did. OnEntitySpawned therefore switches it OFF and this
//!     class becomes responsible for the husk.
//!  2. DELETING A GROUP DOES NOT DELETE ITS MEMBERS (defect F-A). SCR_AIGroup spawns members as
//!     WORLD-ROOT entities, so OnEntityDespawning deletes them explicitly, using vanilla's own idiom
//!     from SCR_AIGroup.DespawnMembers.
//!  3. A CORPSE IS NOT A COMPANION. OnEntityPruned deletes the waypoints and the emptied husk and
//!     deliberately LEAVES THE BODY where it fell - a player may be looting it, and it is world-root
//!     so nothing cascades to it.
//------------------------------------------------------------------------------------------------
class OVT_TownCivilianSourceConfig : OVT_AmbientSpawnSourceConfig
{
	//------------------------------------------------------------------------------------------------
	// CONSTANTS
	//------------------------------------------------------------------------------------------------

	//! Ceiling on the cached doorway/POI candidate list. A city inside an 800 m sphere holds hundreds
	//! of buildings and the crowd only ever draws one point at a time, so more than this buys nothing
	//! and costs memory for the whole session.
	static const int MAX_PLACEMENT_CANDIDATES = 256;

	//! How far outside a building's footprint its doorstep candidate is placed, in metres. Far enough
	//! that a civilian is never standing inside the wall the bounding box ends at.
	static const float BUILDING_STANDOFF = 1.5;

	//! WANDER's clamps. See BuildWanderRoute for why both ends are held.
	static const int MIN_WANDER_POINTS = 2;
	static const int MAX_WANDER_POINTS = 6;

	//! How far a loiterer will snap to a cached doorway/POI candidate, in metres. Beyond this it stays
	//! exactly where it spawned rather than walking somewhere "better".
	static const float LOITER_SNAP_DISTANCE = 25;

	//------------------------------------------------------------------------------------------------
	// BOUND STATE (written once by the manager at activation)
	//------------------------------------------------------------------------------------------------

	//! The authored declaration this crowd is made from. Read by reference; never copied field by field.
	protected OVT_CivilianAmbienceConfig m_Template;

	//! Index into OVT_TownManagerComponent.m_Towns. The town's POPULATION IS NEVER BAKED - it is read
	//! live on every RollCount(), so a town that grew or was depopulated since the last visit gets the
	//! new number with no re-registration.
	protected int m_iTownId;

	//! The settlement's size, for the type filter.
	protected OVT_TownSize m_eTownSize;

	//! The civilian types this town may roll, resolved once at activation from the template, the town's
	//! size and (from Phase 4) the town's own allow-list.
	protected ref array<ref OVT_CivilianTypeConfig> m_aAllowedTypes;

	//! Doorway/POI points for this town, BUILT LAZILY ON FIRST USE and cached for the session.
	//!
	//! ⚠ NEVER BUILT AT REGISTRATION. Every one of the map's ~20 towns registers at campaign start, and
	//! filling this then would mean 20 world queries over a 800 m sphere in the same frame, for towns a
	//! player may never visit. It is filled on the first spawn this town ever does instead - by which
	//! point a player is standing in the spawn ring and the query is paid for by somebody who is
	//! actually there - and then reused for the rest of the session.
	protected ref array<vector> m_aPlacementCache;

	//! Has the lazy build run? Kept separate from "the cache is empty", because a town that genuinely
	//! has no candidates (no authored spawn points, no ownable buildings) must not re-query on every
	//! single spawn for the rest of the session.
	protected bool m_bPlacementCacheBuilt;

	//! Where core registered this source. Held so a suspended town (a QRF) can be re-registered at the
	//! same place without having to resolve a town record that may have moved in the meantime.
	protected vector m_vTownLocation;

	//------------------------------------------------------------------------------------------------
	// LIVE STATE
	//------------------------------------------------------------------------------------------------

	//! group EntityID -> everything that civilian owns. THE bookkeeping this whole class exists for.
	protected ref map<EntityID, ref OVT_AmbientCivilianRecord> m_mCivilians;

	//! The type name RollPrefab() last chose, consumed by the OnEntitySpawned() that immediately
	//! follows it.
	//!
	//! ⚠ THIS STAMP IS THE ONLY THING THAT KNOWS WHICH TYPE A CIVILIAN IS. Every type spawns the SAME
	//! shared Group_CIV.et (a per-type prefab could author nothing the loadout does not overwrite), so
	//! the spawned prefab can never be mapped back to a type - a reverse lookup would resolve all six
	//! types to whichever one is listed first, and every civilian in the world would wear the same
	//! clothes.
	//!
	//! ⚠ THIS IS ONLY SAFE BECAUSE CORE BUILDS ONE ENTITY AT A TIME. SpawnAmbientEntity() calls
	//! RollPrefab() -> RollPosition() -> spawn -> OnEntitySpawned() synchronously, with no other
	//! entity of this source interleaved, so the value cannot be read by the wrong spawn. If core ever
	//! batches, RollPrefab must start returning the type instead.
	protected string m_sPendingTypeName;

	//------------------------------------------------------------------------------------------------
	void OVT_TownCivilianSourceConfig()
	{
		m_mCivilians = new map<EntityID, ref OVT_AmbientCivilianRecord>();
		m_aAllowedTypes = new array<ref OVT_CivilianTypeConfig>();
		m_aPlacementCache = new array<vector>();
		m_iTownId = -1;
	}

	//------------------------------------------------------------------------------------------------
	// BINDING
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Binds this instance to one town. Called exactly once, by the manager, before registration.
	//! \param[in] template The authored declaration to read through.
	//! \param[in] townId Index into the town manager's town list.
	//! \param[in] townSize The settlement's size, for the type filter.
	//! \param[in] allowedTypes The types resolved for this town; may be empty (the town then rolls
	//!        nothing, which is loud rather than silent - RollPrefab returns Empty and core stops).
	void Bind(notnull OVT_CivilianAmbienceConfig template, int townId, OVT_TownSize townSize, array<ref OVT_CivilianTypeConfig> allowedTypes)
	{
		m_Template = template;
		m_iTownId = townId;
		m_eTownSize = townSize;

		m_aAllowedTypes.Clear();
		if (allowedTypes)
		{
			foreach (OVT_CivilianTypeConfig type : allowedTypes)
			{
				if (type)
					m_aAllowedTypes.Insert(type);
			}
		}
	}

	//! \return The authored template this crowd reads through; null only if Bind() was never called.
	OVT_CivilianAmbienceConfig GetTemplate()
	{
		return m_Template;
	}

	//! \return The town this crowd belongs to, or -1 when unbound.
	int GetTownId()
	{
		return m_iTownId;
	}

	//! Where this source is (and is re-)registered. Set by the manager at activation.
	//! \param[in] location The town's location.
	void SetTownLocation(vector location)
	{
		m_vTownLocation = location;
	}

	//! \return The position this source was registered at.
	vector GetTownLocation()
	{
		return m_vTownLocation;
	}

	//! \return How many doorway/POI placement candidates this town has cached; 0 before the lazy build
	//! has ever run.
	int GetPlacementCandidateCount()
	{
		if (!m_aPlacementCache)
			return 0;

		return m_aPlacementCache.Count();
	}

	//! \return The settlement size this crowd filtered its types against.
	OVT_TownSize GetTownSize()
	{
		return m_eTownSize;
	}

	//! \return The civilian types this town may roll. Never null.
	array<ref OVT_CivilianTypeConfig> GetAllowedTypes()
	{
		return m_aAllowedTypes;
	}

	//! \return How many civilians this crowd currently has bookkeeping for.
	int GetLiveCivilianCount()
	{
		if (!m_mCivilians)
			return 0;

		return m_mCivilians.Count();
	}

	//------------------------------------------------------------------------------------------------
	// THE SEVEN OVERRIDES
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! How many civilians this town owes for ONE activation.
	//!
	//! Reads the town's LIVE population (never a registration-time copy) and hands it, the authored
	//! rate, the operator multiplier and both clamps to the world-free formula. Everything a server
	//! owner can change - civilianDensityMultiplier, maxCiviliansPerTown - takes effect on the next
	//! approach with no re-registration.
	//!
	//! It is also the crowd's ONE housekeeping point: core rolls it exactly once per activation, at
	//! the moment when this source is provably owed nothing, so any record still in the map is an
	//! orphan and is swept here (see SweepOrphanedRecords).
	//! \return The number of civilians to build, never negative.
	override int RollCount()
	{
		SweepOrphanedRecords();

		if (!m_Template)
			return 0;

		OVT_TownData town = ResolveTown();
		if (!town)
			return 0;

		float multiplier = 1;
		int hardCap = 0;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (config && config.m_ConfigFile)
		{
			multiplier = config.m_ConfigFile.civilianDensityMultiplier;
			hardCap = config.m_ConfigFile.maxCiviliansPerTown;
		}

		return OVT_CivilianAmbienceMath.ResolveTownCivilianCount(town.population, m_Template.m_fPopulationRate,
			multiplier, m_iMinCount, m_iMaxCount, hardCap);
	}

	//------------------------------------------------------------------------------------------------
	//! Picks ONE civilian's type, by weight, out of the set this town allows.
	//!
	//! Deliberately does NOT fall back to the base class's m_aPrefabs: a town whose allowed set came
	//! out empty is a mis-authored config, and answering Empty stops the activation loudly (core logs
	//! it) instead of quietly populating the town with whatever happened to be in the base array.
	//! \return The type's one-man group prefab, or Empty when nothing is eligible.
	override ResourceName RollPrefab()
	{
		m_sPendingTypeName = "";

		if (!m_aAllowedTypes || m_aAllowedTypes.IsEmpty())
			return ResourceName.Empty;

		array<int> weights = new array<int>();
		foreach (OVT_CivilianTypeConfig type : m_aAllowedTypes)
		{
			weights.Insert(type.m_iWeight);
		}

		int total = OVT_CivilianAmbienceMath.SumWeights(weights);

		// RandInt is MAX-EXCLUSIVE and RandInt(n, n) raises an engine error, so the total is both the
		// right bound and the thing that has to be positive before we draw at all.
		if (total <= 0)
			return ResourceName.Empty;

		int index = OVT_CivilianAmbienceMath.PickWeightedIndex(weights, s_AIRandomGenerator.RandInt(0, total));
		if (index < 0 || index >= m_aAllowedTypes.Count())
			return ResourceName.Empty;

		OVT_CivilianTypeConfig chosen = m_aAllowedTypes[index];
		m_sPendingTypeName = chosen.m_sTypeName;

		return chosen.m_rGroupPrefab;
	}

	//------------------------------------------------------------------------------------------------
	//! Where ONE civilian goes: a doorway/POI point some of the time, a road point the rest of it.
	//!
	//! WHY A MIX AND NOT ONE OR THE OTHER. The retired spawner put every civilian on the nearest road,
	//! which is why a town's crowd read as a queue walking down the middle of the street. Placing all
	//! of them at doorways instead would be the same mistake pointed the other way (nobody ever crosses
	//! the square). m_fBuildingPlacementChance is the authored share, so the balance is a .conf edit.
	//!
	//! NEITHER PATH CAN PUT A CIVILIAN IN THE SEA. The road path scatters through
	//! GetRandomNonOceanPositionNear; the candidate path is built from building footprints and authored
	//! spawn points, and rejects anything the same ocean test dislikes at BUILD time (see
	//! CollectPlacementCandidate) - so a cached candidate is dry by construction.
	//! \param[in] origin The town's location.
	//! \param[in] radius The town's range.
	//! \return A point with ground under it.
	override vector RollPosition(vector origin, float radius)
	{
		if (radius <= 0)
			return origin;

		float chance = 0;
		if (m_Template)
			chance = m_Template.m_fBuildingPlacementChance;

		if (chance > 0)
		{
			// Lazily, and only when the authored mix actually wants candidates: a server that authors
			// 0 never pays for the query at all.
			EnsurePlacementCache(origin, radius);

			if (m_aPlacementCache && !m_aPlacementCache.IsEmpty() && s_AIRandomGenerator.RandFloat01() < chance)
				return m_aPlacementCache.GetRandomElement();
		}

		vector scattered = OVT_WorldUtils.GetRandomNonOceanPositionNear(origin, radius);

		return OVT_WorldUtils.FindNearestRoad(scattered);
	}

	//------------------------------------------------------------------------------------------------
	//! Dresses, hooks and routes ONE freshly built civilian, and opens its record.
	//!
	//! THE ORDER MATTERS. The husk is taken off vanilla's delete-when-empty path FIRST (lifetime rule
	//! 1 in the class header), the record is opened SECOND so the agent callback below can always find
	//! it, and the route is built LAST because it is the only step that can fail harmlessly.
	//! \param[in] entity The one-man group core just spawned.
	//! \param[in] sourcePosition The town's location - NOT the group's, which the scatter already moved.
	override void OnEntitySpawned(IEntity entity, vector sourcePosition)
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if (!group)
		{
			Print("[Overthrow] CivilianAmbience: an ambient civilian spawned as something that is not an SCR_AIGroup - the authored type prefab must be a one-man GROUP", LogLevel.WARNING);
			return;
		}

		// LIFETIME RULE 1. Without this the engine deletes the group ~1 ms after its member dies, core
		// sees a null instead of a dead entity, OnEntityPruned never runs, and this civilian's
		// waypoints leak for the rest of the session.
		group.SetDeleteWhenEmpty(false);

		// EntityIDs are recycled by the engine. A record still filed under this id can only belong to a
		// civilian that no longer exists, and overwriting it silently would strand that civilian's
		// waypoints - so it is closed properly first. Normally a no-op.
		DiscardRecord(group.GetID());

		OVT_AmbientCivilianRecord record = new OVT_AmbientCivilianRecord();
		record.m_GroupId = group.GetID();
		record.m_sTypeName = m_sPendingTypeName;
		m_mCivilians.Set(group.GetID(), record);

		// GetOnAgentAdded is the ONLY hook that can dress a member: the engine's queue creates it some
		// frames after the group, so nothing can be done to the character at this point - it does not
		// exist yet.
		//
		// ⚠ ONE CALLBACK, NOT TWO. Until Phase 4 the global OVT_LoadoutUtils.RandomizeCivilianClothes was
		// inserted alongside this one; it is now done INSIDE OnCivilianAgentAdded, because the clothes a
		// civilian gets depend on the type it was rolled from and only the record knows which that was.
		group.GetOnAgentAdded().Insert(OnCivilianAgentAdded);

		BuildRoute(group, record, sourcePosition);
	}

	//------------------------------------------------------------------------------------------------
	//! Takes ONE civilian down, member first (defect F-A).
	//!
	//! Core deletes the GROUP entity immediately after this returns - and deleting a group does NOT
	//! delete its members, because SCR_AIGroup spawns them as world-root entities. The retired spawner
	//! deleted only the group, so every despawn left its civilians standing in the town; this deletes
	//! them explicitly, with vanilla's own idiom (SCR_AIGroup.DespawnMembers).
	//! \param[in] entity The group about to be deleted.
	override void OnEntityDespawning(IEntity entity)
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if (!group)
			return;

		DeleteGroupMembers(group);
		DiscardRecord(group.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! Is this civilian finished?
	//!
	//! A GROUP CARRIES NO DAMAGE MANAGER, so core's stock predicate answers false forever and a dead
	//! civilian would never be pruned - this override is the entire reason the hook exists (api.md §4).
	//! The predicate is "I have seen a member AND there are none now"; the first half is mandatory,
	//! because the engine's spawn queue leaves a freshly built group legitimately memberless for one or
	//! more frames and a naive count test would prune the whole crowd on the tick after it was built.
	//!
	//! It reads an engine budget EVICTION as death too. That is accepted (decision D8): the crowd
	//! thins for this visit and is re-rolled fresh on the next approach, which is the ambient contract.
	//! \param[in] entity The group to judge.
	//! \return True once this civilian's member has existed and is gone.
	override bool IsEntityDead(IEntity entity)
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if (!group)
			return false;

		OVT_AmbientCivilianRecord record = m_mCivilians.Get(group.GetID());
		if (!record)
			return false;

		if (!record.m_bSeenAgent)
			return false;

		return group.GetAgentsCount() == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! LEAVE THE BODY, DELETE THE COMPANIONS.
	//!
	//! Core has already dropped this entity from the source's list and from its reverse map, so it is
	//! nobody's any more. The waypoints and the emptied group husk existed only to serve it and are
	//! deleted here; THE CORPSE IS DELIBERATELY LEFT WHERE IT FELL - a player may be looting it, and
	//! it is a world-root entity so deleting the husk cannot cascade to it.
	//! \param[in] entity The group that has just stopped being ambient.
	override void OnEntityPruned(IEntity entity)
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if (!group)
			return;

		DiscardRecord(group.GetID());

		SCR_EntityHelper.DeleteEntityAndChildren(group);
	}

	//------------------------------------------------------------------------------------------------
	// RELEASE (the recruitment path)
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Hands one civilian's leftovers over after the manager released it from ambient ownership.
	//!
	//! ⚠ OnEntityPruned NEVER FIRES FOR A RELEASED ENTITY - a release removes it from core's list
	//! before any prune can see it - so this is an explicit cleanup, not a hook.
	//!
	//! THE HUSK IS HANDED BACK TO THE ENGINE RATHER THAN DELETED HERE, and that is deliberate: at this
	//! point the recruit's agent is STILL IN THIS GROUP (the release runs before the recruit is moved
	//! into its owner's slave group, so that a refused recruit never releases). Deleting the group out
	//! from under a live agent is exactly the kind of thing that orphans a player's recruit. Re-arming
	//! vanilla's delete-when-empty instead means the engine deletes the husk the instant the agent
	//! leaves - one call later - and, if the transfer somehow fails, the recruit simply keeps a group
	//! of its own instead of being destroyed.
	//! \param[in] groupId The released group.
	//! \return True when this crowd owned that group and has now let go of it.
	bool ReleaseCivilian(EntityID groupId)
	{
		OVT_AmbientCivilianRecord record = m_mCivilians.Get(groupId);
		if (!record)
			return false;

		// The route belongs to the crowd, not to the recruit: a recruited civilian must stop walking
		// its ambient patrol the moment it changes hands.
		DiscardRecord(groupId);

		IEntity entity = GetGame().GetWorld().FindEntityByID(groupId);
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if (group)
			group.SetDeleteWhenEmpty(true);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	// INTERNALS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Set on the first member the engine's queue puts in a group: dress-adjacent state that cannot be
	//! done at spawn time because the character does not exist yet.
	//!
	//! DEFECT F-B IS FIXED HERE. The invoker publishes an AIAgent, and the retired spawner's callback
	//! took an IEntity and looked for OVT_PlayerWantedComponent ON THE AGENT, where that component does
	//! not exist - so town civilians have been running with the wanted system live since the callback
	//! was written. Resolving GetControlledEntity() first is the whole fix.
	//!
	//! ⚠ DO NOT "OPTIMISE" DisableWantedSystem() INTO A FLAG WRITE. Its internal SetWantedLevel(0) is
	//! what tells the other machines, and a civilian that kept a stale non-zero level would stay
	//! visibly wanted on every client.
	//!
	//! IT ALSO DRESSES THE CIVILIAN, per its rolled type (Phase 4), AND THAT IS THE ONLY THING A TYPE
	//! LOOKS LIKE. Every type is built from the same shared Group_CIV.et - there are no per-type
	//! prefabs, because ApplyCivilianLoadout OVERWRITES the visible clothing slots and a per-type
	//! prefab's authored look could never survive it. The type's own loadout wins when it authors one,
	//! and the global CivilianClothes.conf is the fallback for a type that authors none.
	//! \param[in] agent The member that just materialised.
	protected void OnCivilianAgentAdded(AIAgent agent)
	{
		if (!agent)
			return;

		AIGroup parent = agent.GetParentGroup();
		OVT_AmbientCivilianRecord record;
		if (parent)
			record = m_mCivilians.Get(parent.GetID());

		IEntity character = agent.GetControlledEntity();
		if (character)
		{
			OVT_LoadoutConfig loadout;
			if (record)
				loadout = ResolveTypeLoadout(record.m_sTypeName);

			if (loadout)
			{
				OVT_LoadoutUtils.ApplyCivilianLoadout(character, loadout);
			}
			else
			{
				OVT_LoadoutUtils.ApplyCivilianLoadout(character);
			}

			OVT_PlayerWantedComponent wanted = OVT_PlayerWantedComponent.Cast(character.FindComponent(OVT_PlayerWantedComponent));
			if (wanted)
				wanted.DisableWantedSystem();
		}

		if (!record)
			return;

		// The dead-check's precondition. Until this is true the civilian is "not built yet", never
		// "dead".
		record.m_bSeenAgent = true;
	}

	//------------------------------------------------------------------------------------------------
	//! The clothing a civilian type is dressed from, when it authors its own.
	//!
	//! Resolved BY NAME out of the town's allowed set rather than remembered as a pointer on the
	//! record: the set has at most a handful of entries, the lookup happens once per civilian, and a
	//! name cannot dangle when a template is re-authored the way a held object reference could.
	//! \param[in] typeName The type the civilian was rolled from.
	//! \return That type's loadout, or null for "use the global CivilianClothes.conf".
	protected OVT_LoadoutConfig ResolveTypeLoadout(string typeName)
	{
		if (typeName == "" || !m_aAllowedTypes)
			return null;

		foreach (OVT_CivilianTypeConfig type : m_aAllowedTypes)
		{
			if (!type || type.m_sTypeName != typeName)
				continue;

			if (type.m_Loadout && type.m_Loadout.m_aSlots && !type.m_Loadout.m_aSlots.IsEmpty())
				return type.m_Loadout;

			return null;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds this civilian's route and records every waypoint entity it creates.
	//!
	//! THREE BEHAVIOURS, ONE EXIT. Whichever archetype is rolled, the legs it produced are wrapped in a
	//! single infinite cycle here and EVERY waypoint entity built along the way is in the record before
	//! this method returns. That is the deletion contract (quality bar Q4): a waypoint that never
	//! reaches a record is a waypoint nothing will ever delete, so the archetype builders below insert
	//! into the record at the moment of creation rather than handing entities back to be filed later.
	//!
	//! The wait bounds and the point count come from the authored archetype - and they finally MEAN
	//! something as of Phase 3's F-C fix (SpawnWaitWaypoint used to drop its duration, so every pause
	//! in Overthrow was the prefab's authored 60 s).
	//! \param[in] group The civilian.
	//! \param[in] record Its record - every waypoint built below is inserted into it.
	//! \param[in] sourcePosition The town's location, which further route points are scattered around.
	protected void BuildRoute(notnull SCR_AIGroup group, notnull OVT_AmbientCivilianRecord record, vector sourcePosition)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return;

		float waitMin = 15;
		float waitMax = 50;
		int pointCount = 2;
		OVT_ECivilianArchetype behaviour = OVT_ECivilianArchetype.PINGPONG;

		OVT_CivilianArchetypeConfig archetype = RollArchetype();
		if (archetype)
		{
			waitMin = archetype.m_fWaitMin;
			waitMax = archetype.m_fWaitMax;
			pointCount = archetype.m_iPointCount;
			behaviour = archetype.m_eArchetype;
		}

		if (waitMin < 0)
			waitMin = 0;

		if (waitMax < waitMin)
			waitMax = waitMin;

		array<AIWaypoint> route = new array<AIWaypoint>();

		if (behaviour == OVT_ECivilianArchetype.WANDER)
		{
			BuildWanderRoute(config, route, record, sourcePosition, pointCount, waitMin, waitMax);
		}
		else if (behaviour == OVT_ECivilianArchetype.LOITER)
		{
			BuildLoiterRoute(config, route, record, group.GetOrigin(), waitMin, waitMax);
		}
		else
		{
			BuildPingPongRoute(config, route, record, group.GetOrigin(), sourcePosition, waitMin, waitMax);
		}

		if (route.IsEmpty())
			return;

		AIWaypointCycle cycle = AIWaypointCycle.Cast(config.SpawnWaypoint(config.m_pCycleWaypointPrefab, group.GetOrigin()));
		if (!cycle)
			return;

		// Recorded BEFORE it is wired up, so a failure after this point still deletes it with the
		// civilian rather than leaving it in the world.
		record.m_aWaypoints.Insert(cycle);

		cycle.SetWaypoints(route);
		cycle.SetRerunCounter(-1);

		group.AddWaypoint(cycle);
	}

	//------------------------------------------------------------------------------------------------
	//! PINGPONG - the retired spawner's exact route, kept as the parity behaviour.
	//!
	//! Walk to a second point, pause, walk back to where you spawned, pause, forever. It reads as
	//! somebody with an errand, which is why it is still the most common archetype.
	//! \param[in] config The waypoint-prefab holder.
	//! \param[inout] route The cycle's ordered waypoint list.
	//! \param[in] record The owning civilian's record.
	//! \param[in] spawnPosition Where this civilian materialised - the leg it comes back to.
	//! \param[in] sourcePosition The town's location, which the far point is scattered around.
	//! \param[in] waitMin Shortest pause at a stop.
	//! \param[in] waitMax Longest pause at a stop.
	protected void BuildPingPongRoute(notnull OVT_OverthrowConfigComponent config, notnull array<AIWaypoint> route, notnull OVT_AmbientCivilianRecord record, vector spawnPosition, vector sourcePosition, float waitMin, float waitMax)
	{
		AddRouteLeg(config, route, record, RollPosition(sourcePosition, m_fRadius), waitMin, waitMax);
		AddRouteLeg(config, route, record, spawnPosition, waitMin, waitMax);
	}

	//------------------------------------------------------------------------------------------------
	//! WANDER - N scattered points around the town, short pauses, cycled.
	//!
	//! The point count is clamped rather than trusted: a mis-authored 0 or 1 would produce a cycle a
	//! civilian completes instantly and stands in forever, and a mis-authored 50 would give one
	//! civilian fifty patrol waypoints plus fifty wait waypoints (a town's worth of entities for one
	//! person). Two is the smallest count that is still a route; six is as far as anybody can watch one
	//! civilian walk before it stops reading as wandering and starts reading as a delivery driver.
	//! \param[in] config The waypoint-prefab holder.
	//! \param[inout] route The cycle's ordered waypoint list.
	//! \param[in] record The owning civilian's record.
	//! \param[in] sourcePosition The town's location, which every point is scattered around.
	//! \param[in] pointCount The authored number of route points.
	//! \param[in] waitMin Shortest pause at a stop.
	//! \param[in] waitMax Longest pause at a stop.
	protected void BuildWanderRoute(notnull OVT_OverthrowConfigComponent config, notnull array<AIWaypoint> route, notnull OVT_AmbientCivilianRecord record, vector sourcePosition, int pointCount, float waitMin, float waitMax)
	{
		int points = pointCount;
		if (points < MIN_WANDER_POINTS)
			points = MIN_WANDER_POINTS;

		if (points > MAX_WANDER_POINTS)
			points = MAX_WANDER_POINTS;

		for (int i = 0; i < points; i++)
		{
			AddRouteLeg(config, route, record, RollPosition(sourcePosition, m_fRadius), waitMin, waitMax);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! LOITER - one spot, a long pause, cycled: somebody who is not going anywhere.
	//!
	//! THE SPOT IS WHERE THE CIVILIAN ALREADY IS, snapped to a cached doorway/POI candidate when one is
	//! close by. That is deliberate: RollPosition() has already decided (by m_fBuildingPlacementChance)
	//! whether this civilian belongs at a doorway or on the street, and marching a loiterer across town
	//! to a "better" spot the moment it spawns is exactly the behaviour a loiterer is not supposed to
	//! have.
	//!
	//! A MOVE waypoint rather than a patrol one: patrol behaviour makes an idle civilian shuffle around
	//! its point, which is what this archetype exists to stop.
	//! \param[in] config The waypoint-prefab holder.
	//! \param[inout] route The cycle's ordered waypoint list.
	//! \param[in] record The owning civilian's record.
	//! \param[in] spawnPosition Where this civilian materialised.
	//! \param[in] waitMin Shortest pause at the spot.
	//! \param[in] waitMax Longest pause at the spot.
	protected void BuildLoiterRoute(notnull OVT_OverthrowConfigComponent config, notnull array<AIWaypoint> route, notnull OVT_AmbientCivilianRecord record, vector spawnPosition, float waitMin, float waitMax)
	{
		vector spot = ClosestPlacementCandidate(spawnPosition, LOITER_SNAP_DISTANCE);

		AIWaypoint move = config.SpawnMoveWaypoint(spot);
		if (move)
		{
			record.m_aWaypoints.Insert(move);
			route.Insert(move);
		}

		AIWaypoint hold = config.SpawnWaitWaypoint(spot, RollWait(waitMin, waitMax));
		if (hold)
		{
			record.m_aWaypoints.Insert(hold);
			route.Insert(hold);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One leg of a route: walk there, then stand there for a while.
	//! \param[in] config The waypoint-prefab holder.
	//! \param[inout] route The cycle's ordered waypoint list.
	//! \param[in] record The owning civilian's record.
	//! \param[in] position Where the leg goes.
	//! \param[in] waitMin Shortest pause at the leg's end.
	//! \param[in] waitMax Longest pause at the leg's end.
	protected void AddRouteLeg(notnull OVT_OverthrowConfigComponent config, notnull array<AIWaypoint> route, notnull OVT_AmbientCivilianRecord record, vector position, float waitMin, float waitMax)
	{
		AIWaypoint patrol = config.SpawnPatrolWaypoint(position);
		if (patrol)
		{
			record.m_aWaypoints.Insert(patrol);
			route.Insert(patrol);
		}

		AIWaypoint hold = config.SpawnWaitWaypoint(position, RollWait(waitMin, waitMax));
		if (hold)
		{
			record.m_aWaypoints.Insert(hold);
			route.Insert(hold);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! How long this stop lasts.
	//! \param[in] waitMin Shortest pause. Already floored at 0 by the caller.
	//! \param[in] waitMax Longest pause. Already raised to waitMin by the caller when mis-authored.
	//! \return A duration in seconds. RandFloatXY is never asked for an empty range.
	protected float RollWait(float waitMin, float waitMax)
	{
		if (waitMax > waitMin)
			return s_AIRandomGenerator.RandFloatXY(waitMin, waitMax);

		return waitMin;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds this town's doorway/POI candidate list, ONCE, on the first spawn that wants one.
	//!
	//! ⚠ THE FLAG IS SET BEFORE THE QUERY, not after. A town with no authored spawn points and no
	//! ownable buildings legitimately produces an empty list, and "empty" must not be read as "not
	//! built yet" - otherwise that town would run the whole sphere query again for every civilian it
	//! ever spawns, forever.
	//!
	//! WHY NOT AT REGISTRATION. Every town on the map registers at campaign start. Twenty 800 m sphere
	//! queries in one frame, for towns nobody may ever visit, is a start-up hitch bought for nothing.
	//! Here it is paid once, by a town a player is standing next to.
	//! \param[in] origin The town's location.
	//! \param[in] radius The town's range.
	protected void EnsurePlacementCache(vector origin, float radius)
	{
		if (m_bPlacementCacheBuilt)
			return;

		m_bPlacementCacheBuilt = true;

		if (!m_aPlacementCache)
			m_aPlacementCache = new array<vector>();

		if (radius <= 0)
			return;

		GetGame().GetWorld().QueryEntitiesBySphere(origin, radius, null, CollectPlacementCandidate, EQueryEntitiesFlags.STATIC);

		Print(string.Format("[Overthrow] CivilianAmbience: town %1 cached %2 doorway/POI placement candidate(s)",
			m_iTownId.ToString(), m_aPlacementCache.Count().ToString()), LogLevel.VERBOSE);
	}

	//------------------------------------------------------------------------------------------------
	//! One candidate, or none, from one static entity. Used as the query FILTER (the tree's idiom -
	//! collect in the filter, always answer false, so the engine never builds a second list).
	//!
	//! TWO KINDS OF CANDIDATE, in preference order:
	//!  1. AN AUTHORED SPAWN POINT. OVT_SpawnPointComponent points are hand-placed at doorways and
	//!     usable interior spots - they are the best civilian positions in the world and they cost
	//!     nothing to find.
	//!  2. A BUILDING'S DOORSTEP. Derived from the world bounding box: a point just outside the
	//!     footprint, at a direction rolled once and then frozen in the cache. The ORIGIN itself is
	//!     never used - it is the middle of the building, i.e. inside a wall.
	//! \param[in] entity The static entity the query offered.
	//! \return Always false - this is a collector, not a filter.
	protected bool CollectPlacementCandidate(IEntity entity)
	{
		if (!entity)
			return false;

		if (m_aPlacementCache.Count() >= MAX_PLACEMENT_CANDIDATES)
			return false;

		// HasSpawnPoints() gates this: GetSpawnPoint() answers with the building's own origin when
		// nothing is authored, and caching that would place civilians inside the shell.
		OVT_SpawnPointComponent spawnPoint = OVT_SpawnPointComponent.Cast(entity.FindComponent(OVT_SpawnPointComponent));
		if (spawnPoint && spawnPoint.HasSpawnPoints())
		{
			vector authored = spawnPoint.GetSpawnPoint();
			if (authored != vector.Zero && !OVT_WorldUtils.IsOceanAtPosition(authored))
			{
				m_aPlacementCache.Insert(authored);
				return false;
			}
		}

		if (entity.ClassName() != "SCR_DestructibleBuildingEntity")
			return false;

		vector doorstep;
		if (ResolveBuildingDoorstep(entity, doorstep))
			m_aPlacementCache.Insert(doorstep);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A standing spot just outside one building.
	//!
	//! The direction is rolled ONCE, when the cache is built, and then frozen - so a town's candidate
	//! set is a fixed set of places rather than a new scatter every spawn, and two civilians drawing
	//! the same candidate really do stand in the same doorway.
	//! \param[in] entity The building.
	//! \param[out] position The resolved spot, valid only when this returns true.
	//! \return True when the building produced a dry spot.
	protected bool ResolveBuildingDoorstep(notnull IEntity entity, out vector position)
	{
		vector mins, maxs;
		entity.GetWorldBounds(mins, maxs);

		float halfX = (maxs[0] - mins[0]) * 0.5;
		float halfZ = (maxs[2] - mins[2]) * 0.5;

		float standoff = halfX;
		if (halfZ > standoff)
			standoff = halfZ;

		standoff += BUILDING_STANDOFF;

		vector center = entity.GetOrigin();

		float angle = s_AIRandomGenerator.RandFloatXY(0, Math.PI2);
		float x = center[0] + Math.Sin(angle) * standoff;
		float z = center[2] + Math.Cos(angle) * standoff;

		position = Vector(x, 0, z);
		if (OVT_WorldUtils.IsOceanAtPosition(position))
			return false;

		position = Vector(x, GetGame().GetWorld().GetSurfaceY(x, z), z);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The cached doorway/POI candidate nearest a position, when one is close enough to be worth
	//! walking to.
	//! \param[in] position The position to search around.
	//! \param[in] maxDistance How far a candidate may be and still be chosen.
	//! \return The nearest candidate inside the distance, or `position` itself when there is none.
	protected vector ClosestPlacementCandidate(vector position, float maxDistance)
	{
		if (!m_aPlacementCache || m_aPlacementCache.IsEmpty())
			return position;

		vector best = position;
		float bestDistance = maxDistance;

		foreach (vector candidate : m_aPlacementCache)
		{
			float distance = vector.Distance(candidate, position);
			if (distance >= bestDistance)
				continue;

			bestDistance = distance;
			best = candidate;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Rolls one behaviour out of the template's authored archetypes.
	//! \return The chosen archetype, or null when the template authors none (the caller then uses the
	//!         parity defaults).
	protected OVT_CivilianArchetypeConfig RollArchetype()
	{
		if (!m_Template || !m_Template.m_aArchetypes || m_Template.m_aArchetypes.IsEmpty())
			return null;

		array<int> weights = new array<int>();
		foreach (OVT_CivilianArchetypeConfig archetype : m_Template.m_aArchetypes)
		{
			weights.Insert(archetype.m_iWeight);
		}

		int total = OVT_CivilianAmbienceMath.SumWeights(weights);
		if (total <= 0)
			return null;

		int index = OVT_CivilianAmbienceMath.PickWeightedIndex(weights, s_AIRandomGenerator.RandInt(0, total));
		if (index < 0 || index >= m_Template.m_aArchetypes.Count())
			return null;

		return m_Template.m_aArchetypes[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes a group's member characters (defect F-A).
	//!
	//! Vanilla's own idiom, from SCR_AIGroup.DespawnMembers: iterate the agents, resolve the entity
	//! each one controls, delete THAT. Deleting the group would leave every one of them standing.
	//!
	//! ⚠ A CHARACTER SOMEBODY OWNS IS NEVER DELETED HERE, whatever group it is standing in. Ownership
	//! is stamped by RecruitCivilian() BEFORE it asks for the release, so an owned character in an
	//! ambient group can only mean the release did not take (an agent that failed to resolve for a
	//! frame, a manager that was not there). Without this guard that one-in-a-thousand case would
	//! delete a player's freshly bought recruit at the next despawn - a far worse outcome than the
	//! group husk this guard leaves behind, which the orphan sweep collects anyway.
	//! \param[in] group The group whose members must go.
	protected void DeleteGroupMembers(notnull SCR_AIGroup group)
	{
		array<AIAgent> agents = new array<AIAgent>();
		group.GetAgents(agents);

		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity controlled = agent.GetControlledEntity();
			if (!controlled)
				continue;

			OVT_PlayerOwnerComponent owner = OVT_PlayerOwnerComponent.Cast(controlled.FindComponent(OVT_PlayerOwnerComponent));
			if (owner && !owner.GetPlayerOwnerUid().IsEmpty())
			{
				Print("[Overthrow] CivilianAmbience: refusing to despawn an ambient civilian that a player owns - it was recruited but never released, so it is being left alone", LogLevel.WARNING);
				continue;
			}

			RplComponent.DeleteRplEntity(controlled, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes everything a civilian's record owns and forgets the record.
	//!
	//! THE ONE PLACE WAYPOINTS ARE DELETED. Every path that ends a civilian - despawn, prune, release,
	//! the orphan sweep - comes through here, so "every creation site is paired with a deletion site"
	//! is one function to check rather than four.
	//! \param[in] groupId The civilian's group.
	protected void DiscardRecord(EntityID groupId)
	{
		OVT_AmbientCivilianRecord record = m_mCivilians.Get(groupId);
		if (!record)
			return;

		m_mCivilians.Remove(groupId);

		if (!record.m_aWaypoints)
			return;

		foreach (AIWaypoint waypoint : record.m_aWaypoints)
		{
			if (!waypoint)
				continue;

			SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
		}

		record.m_aWaypoints.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Drops records that no path could close, and deletes what they still own.
	//!
	//! WHY IT IS NEEDED AT ALL. Core cannot hook the one case where the group entity is destroyed by
	//! something else entirely (a GM deleting it, a world edit): the entity reference simply becomes
	//! null, core's prune drops the null one branch BEFORE it would consult this config, and
	//! OnEntityPruned never runs. Nothing would then delete that civilian's five waypoints.
	//!
	//! WHY HERE. Core rolls the count exactly once per activation, at the only moment this source is
	//! provably owed nothing: a despawn closed every record, a prune closed every record it saw, and a
	//! release closed its own. Anything still in the map at that instant is by definition an orphan.
	//!
	//! It is deliberately conservative about the world: it always deletes the WAYPOINTS (they can only
	//! ever have served this crowd) but only deletes a surviving husk when that husk has no members -
	//! a group that somehow still holds an agent is left alone rather than risking a live character.
	protected void SweepOrphanedRecords()
	{
		if (!m_mCivilians || m_mCivilians.IsEmpty())
			return;

		// The keys are snapshotted first: DiscardRecord() removes from the very map being walked.
		array<EntityID> stale = new array<EntityID>();
		for (int i = 0; i < m_mCivilians.Count(); i++)
		{
			stale.Insert(m_mCivilians.GetKey(i));
		}

		int huskCount = 0;

		foreach (EntityID groupId : stale)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(groupId);

			DiscardRecord(groupId);

			SCR_AIGroup group = SCR_AIGroup.Cast(entity);
			if (!group)
				continue;

			if (group.GetAgentsCount() > 0)
				continue;

			SCR_EntityHelper.DeleteEntityAndChildren(group);
			huskCount++;
		}

		Print(string.Format("[Overthrow] CivilianAmbience: swept %1 orphaned civilian record(s) (%2 empty husk(s)) for town %3 before rolling a new crowd",
			stale.Count().ToString(), huskCount.ToString(), m_iTownId.ToString()), LogLevel.VERBOSE);
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves this crowd's town, bounds-checked.
	//!
	//! GetTown() indexes its array directly, so an id that is out of range is an engine error rather
	//! than a null - which is why the bound is checked here and not there.
	//! \return The town data, or null when the id does not name one.
	protected OVT_TownData ResolveTown()
	{
		if (m_iTownId < 0)
			return null;

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || !towns.m_Towns)
			return null;

		if (m_iTownId >= towns.m_Towns.Count())
			return null;

		return towns.GetTown(m_iTownId);
	}
}
