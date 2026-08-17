[BaseContainerProps(configRoot: true)]
class OVT_BaseSpawningDeploymentModule : OVT_BaseDeploymentModule
{
	protected ref array<ref EntityID> m_aSpawnedEntities;
	protected bool m_bSpawnedUnitsEliminated; // Flag to track if all spawned units have been killed

	//! Owner system tag every deployment-registered group carries. ONE system for the whole framework -
	//! town patrols, tower garrisons and vehicle crews - separated by owner keys, not by a tag per
	//! config. It lives on the base class because every spawning module that registers anything has to
	//! agree on it: two modules disagreeing here would each be unable to reclaim the other's groups.
	static const string OWNER_SYSTEM = "deployment";

	//! spawnDistanceOverride meaning "use the global virtualizationSpawnDistance" - 1750 m as shipped,
	//! which is the same ring the deleted per-marker proximity toggle used to spawn and delete on.
	static const int SPAWN_DISTANCE_GLOBAL = -1;

	//------------------------------------------------------------------------------------------------
	override void Initialize(OVT_DeploymentComponent parent)
	{
		super.Initialize(parent);
		m_aSpawnedEntities = new array<ref EntityID>;
	}
	
	array<IEntity> GetSpawnedEntities()
	{
		array<IEntity> entities = new array<IEntity>;
		return entities;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this module CHOOSES each group's position, rather than rolling one near the marker.
	//!
	//! ⚠ FALSE IS THE SHIPPED ANSWER AND MUST STAY THE DEFAULT. The plain infantry module rolls a ring
	//! point and then hands it to OVT_WorldUtils.FindNearestRoad, whose search is 500 m wide and ignores
	//! m_fSpawnRadius entirely - integration MEASURED a tower garrison registering on its access road
	//! instead of at its tower. A registration reached that way is an artefact, not a station.
	//!
	//! TRUE means the position came from a deliberate decision the module can defend: a placement
	//! provider's post (a tower walkway, a sniper marker, a defend position) or the composition this
	//! module just built. Only those positions are safe to hold.
	//!
	//! THE ONE CONSUMER is OVT_PatrolBehaviorDeploymentModule's DEFEND branch, which anchors its hold
	//! point on the group when this is true and on the deployment marker when it is false. Getting it
	//! backwards either walks a placed garrison off its post towards the base flag, or parks a tower
	//! garrison on the road the snap dropped it on.
	//! \return Whether this module's registration positions are deliberate stations.
	bool StationsGroupsDeliberately()
	{
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	// Common spawning functionality
	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnEntity(ResourceName prefab, vector position, vector rotation = vector.Zero)
	{
		if (!prefab || prefab.IsEmpty())
			return null;
			
		// Create transform matrix
		vector mat[4];
		Math3D.AnglesToMatrix(rotation, mat);
		mat[3] = position;
		
		// Spawn through unified API (TODO: create unified spawning API)
		IEntity entity = OVT_WorldUtils.SpawnEntityPrefabMatrix(prefab, mat);
		if (entity)
		{
			m_aSpawnedEntities.Insert(entity.GetID());
		}
		
		return entity;
	}
	
	//------------------------------------------------------------------------------------------------
	// Virtualization
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Brings this module's registered groups up to the count it wants. IDEMPOTENT by contract:
	//! reclaim first (FindGroupsByOwner), then register only the shortfall - "converge to wanted",
	//! never "spawn wanted".
	//!
	//! Reached from three places that may run in any order and any number of times: the deployment's
	//! first update tick, the deployment manager's records-restored fan-out, and the reinforcement
	//! path. A module that does not register groups leaves this alone.
	void EnsureGroups()
	{
		// Nothing to converge on in a module that spawns by hand.
	}

	//------------------------------------------------------------------------------------------------
	//! One registered group has been wiped out. Fanned out from the deployment manager (D7); a module
	//! that does not own the handle ignores it.
	//! \param[in] handle The wiped group's registry handle.
	void OnVirtualGroupWiped(int handle)
	{
		// Not one of mine until a subclass says otherwise.
	}

	//------------------------------------------------------------------------------------------------
	//! How many virtualization-registered groups this module currently holds.
	//!
	//! ZERO IS A CORRECT ANSWER, NOT A MISSING IMPLEMENTATION. A module that spawns entities rather
	//! than registering groups - parked vehicles, for one - genuinely fields none, and a reader that
	//! sums this across a deployment's modules gets the truth either way.
	//!
	//! READ-ONLY AND CHEAP BY CONTRACT. Its consumer is OVT_GMSnapshotBuilder, which is forbidden from
	//! calling anything that allocates, spends or spawns and runs once per Game Master per poll.
	//! \return The registered group count; 0 for a module that registers none.
	int GetRegisteredGroupCount()
	{
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The owner key this module's groups are registered under - the deployment's persisted virtual key
	//! plus a tag identifying this module within it.
	//!
	//! DERIVED, NEVER STORED. Both halves are already stable (the deployment key is persisted, the
	//! module tag is the authored name or this module's position), so a second copy of the composed
	//! string would only be another thing that could go stale.
	//!
	//! The module name has to be passed in because it is an [Attribute] on each concrete subclass -
	//! moving it onto this base class would change the authored config surface, which D1 freezes.
	//! \param[in] moduleName This module's authored m_sModuleName; may be empty.
	//! \return The owner key, or an empty string when there is no deployment to key against.
	protected string BuildOwnerKey(string moduleName)
	{
		if (!m_ParentDeployment)
			return "";

		string deploymentKey = m_ParentDeployment.EnsureVirtualKey();
		if (deploymentKey.IsEmpty())
			return "";

		return OVT_DeploymentVirtualKey.OwnerKey(deploymentKey, OVT_DeploymentVirtualKey.ModuleTag(moduleName, GetSpawningIndex()));
	}

	//------------------------------------------------------------------------------------------------
	//! This module's position among its deployment's spawning modules - the fallback identity for a
	//! module with no authored name.
	//! \return The index, or 0 when it cannot be resolved.
	protected int GetSpawningIndex()
	{
		if (!m_ParentDeployment)
			return 0;

		array<OVT_BaseSpawningDeploymentModule> spawningModules = m_ParentDeployment.GetSpawningModules();
		for (int i = 0; i < spawningModules.Count(); i++)
		{
			if (spawningModules[i] == this)
				return i;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Records why this group exists, for the Game Master's group icons. Index -1: a deployment has no
	//! base or town id, and its position is resolved from the record's RplId by whoever renders it.
	//!
	//! RE-TAGGING ON RECLAIM IS NOT OPTIONAL. The registry keys on the group entity's EntityID and core
	//! re-creates that entity on load with a fresh one, so a reclaimed group loses its provenance
	//! unless it is tagged again.
	//! \param[in] group The registered group's entity. Null is a no-op.
	protected void TagForGameMaster(SCR_AIGroup group)
	{
		if (!group || !m_ParentDeployment)
			return;

		OVT_GMGroupRegistry.Tag(group, OVT_EGroupOrigin.DEPLOYMENT, -1, m_ParentDeployment.GetDeploymentName());
	}

	//------------------------------------------------------------------------------------------------
	//! The faction KEY for a faction index. The core resolves a composition by (factionKey, groupName)
	//! and NEVER by an index, because indices are positional and move between saves; a deployment
	//! stores an index, so this is the one place the two meet.
	//!
	//! Deliberately NOT through OVT_OverthrowFactionManager.GetOverthrowFactionByIndex(), which
	//! dereferences GetFactionByIndex(index) unguarded and would VME on a stale index.
	//! \param[in] factionIndex The deployment's controlling faction index.
	//! \return The key, or an empty string when the index resolves to nothing.
	protected string ResolveFactionKey(int factionIndex)
	{
		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return "";

		Faction faction = factionManager.GetFactionByIndex(factionIndex);
		if (!faction)
			return "";

		return faction.GetFactionKey();
	}

	//------------------------------------------------------------------------------------------------
	//! The waypoint plan to register a group with, taken from the FIRST behavior module that has an
	//! opinion about it.
	//!
	//! THE ORDER INVERTS HERE. Waypoints used to be bolted onto a group after it spawned; the
	//! virtualization core builds them from a plan at registration and owns them from then on, so the
	//! behavior module has to answer BEFORE the group exists. First non-null answer wins, and a
	//! deployment with no opinionated behavior module registers a group with no waypoints - which is
	//! a garrison, and is legal.
	//! \param[in] groupPosition Where the group is about to be registered; a plan may be built around
	//!            it (a perimeter patrol starts on the group's own bearing).
	//! \return The plan, or null when no behavior module has one.
	protected OVT_VirtualWaypointPlan ResolveVirtualPlan(vector groupPosition)
	{
		if (!m_ParentDeployment)
			return null;

		array<OVT_BaseBehaviorDeploymentModule> behaviorModules = m_ParentDeployment.GetBehaviorModules();
		foreach (OVT_BaseBehaviorDeploymentModule behaviorModule : behaviorModules)
		{
			if (!behaviorModule)
				continue;

			OVT_VirtualWaypointPlan plan = behaviorModule.BuildVirtualPlan(groupPosition);
			if (plan)
				return plan;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	// Elimination tracking
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether everything this module put in the world has been killed.
	//!
	//! NO LONGER POLLED. There used to be a virtual "check whether my units are still alive" hook here,
	//! and every implementation of it counted a group's agents - a test Reforger 1.8's spawn queue and
	//! dormancy make outright wrong, because a perfectly alive dormant or spawn-queued group reports
	//! zero agents. A module that registers its groups now learns about a wipe through
	//! OnVirtualGroupWiped(), which the core raises off the survivor mask and only on a real wipe.
	bool AreSpawnedUnitsEliminated()
	{
		return m_bSpawnedUnitsEliminated;
	}

	void SetSpawnedUnitsEliminated(bool eliminated)
	{
		m_bSpawnedUnitsEliminated = eliminated;
	}
}