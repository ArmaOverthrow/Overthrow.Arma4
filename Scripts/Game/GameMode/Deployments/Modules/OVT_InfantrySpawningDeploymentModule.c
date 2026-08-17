//------------------------------------------------------------------------------------------------
//! Infantry spawning module for deployments: registers this deployment's foot groups with the
//! virtualization core and holds their handles.
//!
//! WHAT CHANGED. This module used to spawn real soldiers whenever a player came within 1750 m and
//! delete every one of them again when the last player left, so a patrol shot down to one man came
//! back at full strength as soon as you walked away and returned. It now REGISTERS its groups; the
//! engine materialises and despawns them by observer proximity, and the core's survivor mask
//! remembers which men died - across despawn AND across save/load.
//!
//! EnsureGroups() IS THE WHOLE MODULE. It is written as CONVERGE TO WANTED, never SPAWN WANTED:
//! reclaim from the registry first (FindGroupsByOwner, keyed on the deployment's persisted virtual
//! key), then register only the shortfall. That is what makes it safe from activation, from the
//! manager's records-restored fan-out and from the rebuy path, in any order and any number of times -
//! and it is what stops a continued campaign registering a second copy of every patrol on top of the
//! one the core has already restored.
//!
//! HANDLES ARE NEVER PERSISTED - the owner key is, and the handles are re-found from it.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_InfantrySpawningDeploymentModule : OVT_BaseSpawningDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;

	[Attribute(defvalue: "light_patrol", desc: "Group type name from faction registry")]
	string m_sGroupType;

	[Attribute(defvalue: "1", desc: "Minimum number of groups to spawn")]
	int m_iMinGroupCount;

	[Attribute(defvalue: "3", desc: "Maximum number of groups to spawn")]
	int m_iMaxGroupCount;

	[Attribute(defvalue: "false", desc: "Scale group count by nearest town size")]
	bool m_bScaleByTownSize;

	protected int m_iActualGroupCount; // Calculated group count based on difficulty and town size

	[Attribute(defvalue: "50", desc: "Spawn radius around deployment position")]
	float m_fSpawnRadius;

	[Attribute(defvalue: "30", desc: "Resource cost per group")]
	int m_iCostPerGroup;

	[Attribute(defvalue: "true", desc: "Allow reinforcement when groups are destroyed")]
	bool m_bAllowReinforcement;

	[Attribute(defvalue: "15", desc: "Reinforcement cost per group")]
	int m_iReinforcementCost;

	[Attribute(defvalue: "false", desc: "Spawn initial groups at nearest faction-controlled base instead of deployment location")]
	bool m_bSpawnAtNearestBase;

	[Attribute(defvalue: "true", desc: "Spawn reinforcement groups at nearest faction-controlled base instead of deployment location")]
	bool m_bReinforceFromNearestBase;

	//! NEVER leave a registration unstamped: an unstamped group inherits vanilla's LOW tier, is capped
	//! at half the AI budget and is evicted first - exactly how a garrison silently fails to appear.
	[Attribute(defvalue: "1", UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(SCR_EAISpawnImportance), desc: "AI spawn-budget tier for this module's groups (LOW 0.50 of budget, NORMAL 0.70, HIGH 0.90, CRITICAL 1.00). Garrisons the player travelled to fight should be HIGH")]
	SCR_EAISpawnImportance m_eImportance;

	//! OWNER_SYSTEM and SPAWN_DISTANCE_GLOBAL live on OVT_BaseSpawningDeploymentModule - the vehicle
	//! module registers under the same owner system, and two spawning modules disagreeing about that
	//! string would each be unable to reclaim the other's groups.

	//! Registry handles this module currently holds. NOT persisted and NOT authoritative: the registry
	//! is, and this list is re-derived from it by every convergence pass.
	protected ref array<int> m_aHandles;

	//! High-water mark of how many groups this module has ever held, so an empty handle list can be
	//! told apart from "never had any". Monotonic - a wipe never lowers it.
	protected int m_iSpawnedEver;

	//------------------------------------------------------------------------------------------------
	void OVT_InfantrySpawningDeploymentModule()
	{
		m_aHandles = new array<int>();
		m_iSpawnedEver = 0;
	}

	//------------------------------------------------------------------------------------------------
	override int GetResourceCost()
	{
		// Use max group count for resource cost calculation to ensure we have enough resources
		return m_iMaxGroupCount * m_iCostPerGroup;
	}

	//------------------------------------------------------------------------------------------------
	override void OnActivate()
	{
		super.OnActivate();

		// No eliminated check here any more - the convergence owns that gate, so every caller gets the
		// same answer whether it arrives through activation, a restore or a rebuy.
		EnsureGroups();
	}

	//------------------------------------------------------------------------------------------------
	//! DELIBERATELY A NO-OP. Deactivation used to delete every soldier this module had spawned - the ad
	//! hoc virtualization the core replaced - and doing it now would throw away the survivor mask this
	//! whole feature exists to keep. Deactivate() means "stop ticking"; only Cleanup() means "over".
	override void OnDeactivate()
	{
		super.OnDeactivate();
	}

	//------------------------------------------------------------------------------------------------
	//! The deployment is over: release every group back to the core. UnregisterGroup respects held
	//! members, so a group somebody is fighting is retired in place rather than deleted out from under
	//! them, and it takes the group's owned waypoints with it.
	//!
	//! THERE IS NO OnDelete COUNTERPART HERE OR ON THE DEPLOYMENT. One would also fire at
	//! quit-to-menu and would erase the very records persistence exists to keep.
	override protected void OnCleanup()
	{
		super.OnCleanup();

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (virtualization)
		{
			// Reclaim before releasing. A deployment deleted before this module ever converged - a
			// restored one whose condition failed on its first evaluation, say - holds an empty list
			// while the registry still holds its groups, and those would be orphaned for the rest of
			// the campaign with no owner left to find them.
			string ownerKey = GetOwnerKey();
			if (!ownerKey.IsEmpty())
				ReclaimHandles(virtualization, ownerKey);

			foreach (int handle : m_aHandles)
			{
				virtualization.UnregisterGroup(handle);
			}
		}

		m_aHandles.Clear();
	}

	//------------------------------------------------------------------------------------------------
	override array<IEntity> GetSpawnedEntities()
	{
		array<IEntity> entities = new array<IEntity>;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return entities;

		foreach (int handle : m_aHandles)
		{
			// "Record exists, entity does not" is a legitimate runtime state, so null is skipped.
			SCR_AIGroup group = virtualization.GetGroup(handle);
			if (group)
				entities.Insert(group);
		}

		return entities;
	}

	//------------------------------------------------------------------------------------------------
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_InfantrySpawningDeploymentModule clone = new OVT_InfantrySpawningDeploymentModule();

		// EVERY attribute has to appear here - a forgotten one silently ships the class default instead
		// of the authored value (that is how m_fMaxCruiseSpeed was lost on the vehicle module). Forget
		// m_eImportance and a HIGH-tier garrison ships at NORMAL and quietly loses the AI budget race.
		clone.m_sModuleName = m_sModuleName;
		clone.m_sGroupType = m_sGroupType;
		clone.m_iMinGroupCount = m_iMinGroupCount;
		clone.m_iMaxGroupCount = m_iMaxGroupCount;
		clone.m_bScaleByTownSize = m_bScaleByTownSize;
		clone.m_fSpawnRadius = m_fSpawnRadius;
		clone.m_iCostPerGroup = m_iCostPerGroup;
		clone.m_bAllowReinforcement = m_bAllowReinforcement;
		clone.m_iReinforcementCost = m_iReinforcementCost;
		clone.m_bSpawnAtNearestBase = m_bSpawnAtNearestBase;
		clone.m_bReinforceFromNearestBase = m_bReinforceFromNearestBase;
		clone.m_eImportance = m_eImportance;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	// Virtualization
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Brings this module's registered groups up to the count it wants. ALWAYS SAFE TO CALL.
	override void EnsureGroups()
	{
		ConvergeGroups(m_bSpawnAtNearestBase);
	}

	//------------------------------------------------------------------------------------------------
	//! Reclaim, then register the shortfall. The whole of this module's registration behaviour, and the
	//! one path both activation and the paid-for rebuy go through.
	//!
	//! Every step is load-bearing: RECLAIM first, so a restore, a re-apply and an activation converge on
	//! the SAME groups instead of stacking a second force on the first; STOP IF WIPED, because a
	//! deployment whose force was killed does not get a new one on any path except a rebuy; and take the
	//! wanted count from GetMaxGroupCount(), which rolls ONCE and remembers, so a re-roll cannot quietly
	//! grow or shrink a force that already exists.
	//! \param[in] fromNearestBase Anchor new groups on the nearest controlled base rather than on the
	//!            deployment. Initial registrations and rebuys author this separately.
	//! \return How many groups this pass newly registered; 0 when it only reclaimed.
	protected int ConvergeGroups(bool fromNearestBase)
	{
		if (!m_ParentDeployment)
			return 0;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return 0;

		string ownerKey = GetOwnerKey();
		if (ownerKey.IsEmpty())
			return 0;

		ReclaimHandles(virtualization, ownerKey);

		if (m_bSpawnedUnitsEliminated || m_ParentDeployment.GetSpawnedUnitsEliminated())
			return 0;

		int missing = OVT_DeploymentVirtualKey.MissingCount(GetMaxGroupCount(), m_aHandles.Count());
		if (missing <= 0)
			return 0;

		return RegisterGroups(virtualization, ownerKey, missing, fromNearestBase);
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the handle list from the registry and re-tags every group it finds.
	//!
	//! THE REGISTRY IS THE TRUTH: the list is cleared and re-derived rather than merged into, so a
	//! handle that stopped being ours cannot survive in it. Re-tagging is not optional - the core
	//! re-creates a group's ENTITY on load with a fresh EntityID and the Game Master registry is keyed
	//! on EntityID, so a reclaimed group loses its provenance unless it is tagged again.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] ownerKey This module's owner key.
	protected void ReclaimHandles(notnull OVT_VirtualizationManagerComponent virtualization, string ownerKey)
	{
		m_aHandles.Clear();

		array<int> found = virtualization.FindGroupsByOwner(OWNER_SYSTEM, ownerKey);
		foreach (int foundHandle : found)
		{
			if (!virtualization.IsRegistered(foundHandle))
				continue;

			m_aHandles.Insert(foundHandle);
			TagForGameMaster(virtualization.GetGroup(foundHandle));
		}

		if (m_aHandles.Count() > m_iSpawnedEver)
			m_iSpawnedEver = m_aHandles.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Registers `count` more groups under this module's owner key.
	//!
	//! Position and plan are resolved per group: the ring-and-road-snap spread is the one the
	//! hand-spawned version used, and the plan comes from whichever behaviour module has an opinion -
	//! asked BEFORE the group exists, because the core builds the waypoints at registration and owns
	//! them from then on.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] ownerKey This module's owner key.
	//! \param[in] count How many groups to add.
	//! \param[in] fromNearestBase Anchor the batch on the nearest controlled base.
	//! \return How many were actually registered.
	protected int RegisterGroups(notnull OVT_VirtualizationManagerComponent virtualization, string ownerKey, int count, bool fromNearestBase)
	{
		int factionIndex = m_ParentDeployment.GetControllingFaction();

		string factionKey = ResolveFactionKey(factionIndex);
		if (factionKey.IsEmpty())
		{
			Print(string.Format("[Overthrow] Deployment '%1': faction index %2 resolves to no faction key, cannot register infantry",
				m_ParentDeployment.GetDeploymentName(), factionIndex), LogLevel.WARNING);
			return 0;
		}

		if (m_sGroupType.IsEmpty())
		{
			Print(string.Format("[Overthrow] Deployment '%1': infantry module has no group type authored",
				m_ParentDeployment.GetDeploymentName()), LogLevel.WARNING);
			return 0;
		}

		vector baseSpawnPos;
		if (!ResolveSpawnAnchor(factionIndex, fromNearestBase, baseSpawnPos))
			return 0;

		int registered = 0;

		for (int i = 0; i < count; i++)
		{
			vector spawnPos = GetRandomSpawnPosition(baseSpawnPos);
			OVT_VirtualWaypointPlan plan = ResolveVirtualPlan(spawnPos);

			int handle = virtualization.RegisterGroup(OWNER_SYSTEM, ownerKey, factionKey, m_sGroupType,
				spawnPos, plan, SPAWN_DISTANCE_GLOBAL, m_eImportance);

			if (handle == -1)
			{
				// A refusal is structural - an unresolvable composition, a malformed plan - so every
				// remaining iteration would fail identically. Stop; the next convergence retries.
				Print(string.Format("[Overthrow] Deployment '%1': registration of '%2' (%3) was refused, stopping at %4/%5 groups",
					m_ParentDeployment.GetDeploymentName(), m_sGroupType, factionKey, registered, count), LogLevel.WARNING);
				break;
			}

			m_aHandles.Insert(handle);
			registered++;

			TagForGameMaster(virtualization.GetGroup(handle));
		}

		if (m_aHandles.Count() > m_iSpawnedEver)
			m_iSpawnedEver = m_aHandles.Count();

		return registered;
	}

	//------------------------------------------------------------------------------------------------
	//! One registered group has been wiped out - every slot on its roster is dead.
	//!
	//! FIRED ONLY ON A REAL WIPE. The core raises this off the survivor mask, never off an agent count,
	//! so a group that merely went dormant can no longer be mistaken for a dead one - which is what the
	//! deleted agent-count poll did every time nobody was nearby.
	//! \param[in] handle The wiped group's registry handle.
	override void OnVirtualGroupWiped(int handle)
	{
		int index = m_aHandles.Find(handle);
		if (index == -1)
			return;

		m_aHandles.Remove(index);

		if (!m_aHandles.IsEmpty())
			return;

		// An empty list only means elimination if this module ever held anything.
		if (m_iSpawnedEver <= 0 || m_bSpawnedUnitsEliminated)
			return;

		m_bSpawnedUnitsEliminated = true;

		Print(string.Format("[Overthrow] Deployment infantry module '%1' has been wiped out", m_sModuleName), LogLevel.NORMAL);

		if (m_ParentDeployment)
			m_ParentDeployment.CheckAllSpawningModulesEliminated();
	}

	//------------------------------------------------------------------------------------------------
	//! The owner key this module's groups are registered under. Composed by the base class from the
	//! deployment's persisted virtual key and this module's authored name.
	//! \return The owner key, or an empty string when there is no deployment to key against.
	protected string GetOwnerKey()
	{
		return BuildOwnerKey(m_sModuleName);
	}

	//------------------------------------------------------------------------------------------------
	//! Where a batch of groups should be registered around.
	//! \param[in] factionIndex The deployment's controlling faction.
	//! \param[in] fromNearestBase Whether the batch comes out of the nearest controlled base.
	//! \param[out] anchor The resolved anchor position.
	//! \return False when a base was asked for and there is no controlled one to use.
	protected bool ResolveSpawnAnchor(int factionIndex, bool fromNearestBase, out vector anchor)
	{
		anchor = m_ParentDeployment.GetPosition();

		if (!fromNearestBase)
			return true;

		vector nearestBasePos = GetNearestControlledBasePosition(factionIndex);
		if (nearestBasePos == vector.Zero)
		{
			Print("No controlled base found, aborting infantry registration", LogLevel.WARNING);
			return false;
		}

		anchor = nearestBasePos;
		Print(string.Format("Infantry will be registered at nearest base: %1", anchor.ToString()), LogLevel.VERBOSE);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected vector GetRandomSpawnPosition(vector center)
	{
		float angle = Math.RandomFloat01() * Math.PI2;
		float distance = Math.RandomFloat(10, m_fSpawnRadius);

		vector offset = Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);
		vector spawnPos = center + offset;

		//Find nearest road
		vector roadPos = OVT_WorldUtils.FindNearestRoad(spawnPos);

		return roadPos;
	}

	//------------------------------------------------------------------------------------------------
	protected int CalculateGroupCount(vector position)
	{
		// Get difficulty configuration
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			Print("Failed to get config, using default group count", LogLevel.WARNING);
			return m_iMinGroupCount;
		}

		// Base randomized group count from difficulty settings
		int numGroups = s_AIRandomGenerator.RandInt(Math.Ceil((float)config.m_Difficulty.patrolGroupsMin * 0.5), Math.Ceil((float)config.m_Difficulty.patrolGroupsMax * 0.5));

		// Scale by town size if enabled
		if (m_bScaleByTownSize)
		{
			OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
			if (townManager)
			{
				OVT_TownData nearestTown = townManager.GetNearestTown(position);
				if (nearestTown)
				{
					// Scale by town size: multiply by town size (1-4)
					numGroups = numGroups * nearestTown.size;
					Print(string.Format("Scaling groups by town size %1: %2 groups", nearestTown.size, numGroups), LogLevel.VERBOSE);
				}
			}
		}

		// Clamp to min/max bounds
		numGroups = Math.Clamp(numGroups, m_iMinGroupCount, m_iMaxGroupCount);

		Print(string.Format("Calculated %1 groups for deployment (min: %2, max: %3)", numGroups, m_iMinGroupCount, m_iMaxGroupCount), LogLevel.VERBOSE);

		return numGroups;
	}

	//------------------------------------------------------------------------------------------------
	bool CanReinforce(int groupsNeeded)
	{
		if (!m_bAllowReinforcement)
			return false;

		if (!m_ParentDeployment)
			return false;

		// Check if deployment manager has resources for reinforcement
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
			return false;

		int factionIndex = m_ParentDeployment.GetControllingFaction();
		int availableResources = manager.GetFactionResources(factionIndex);
		int totalCost = groupsNeeded * m_iReinforcementCost;

		return availableResources >= totalCost;
	}

	//------------------------------------------------------------------------------------------------
	//! Buys `groupsNeeded` replacement groups and registers them through the same convergence
	//! everything else uses.
	//!
	//! THE FLAGS HAVE TO COME OFF FIRST - BOTH OF THEM. The convergence refuses to register for a module
	//! that is eliminated OR whose deployment is, which is the guarantee that a wiped force never
	//! resurrects itself; a rebuy is the one path allowed through it, so it clears both, converges, and
	//! then lets CheckAllSpawningModulesEliminated() recompute the deployment-wide truth from whatever
	//! the modules actually ended up holding. Clearing only this module's flag would leave a deployment
	//! with a second, still-eliminated spawning module blocking its own rebuy.
	//!
	//! Resources are charged up front and not refunded on a failed registration, which is what this path
	//! has always done.
	//! \param[in] groupsNeeded How many groups to buy.
	//! \return True when at least one group was registered.
	bool Reinforce(int groupsNeeded)
	{
		if (!m_ParentDeployment || groupsNeeded <= 0)
			return false;

		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
			return false;

		// Check if we can afford the reinforcement
		if (!CanReinforce(groupsNeeded))
			return false;

		int factionIndex = m_ParentDeployment.GetControllingFaction();
		int totalCost = groupsNeeded * m_iReinforcementCost;

		// Deduct resources and register reinforcements
		manager.SubtractFactionResources(factionIndex, totalCost);

		// Raise the count this module wants to hold; the convergence reads it back out of
		// GetMaxGroupCount() and registers exactly the difference.
		m_iActualGroupCount = m_aHandles.Count() + groupsNeeded;

		bool wasEliminated = m_bSpawnedUnitsEliminated;
		m_bSpawnedUnitsEliminated = false;
		m_ParentDeployment.SetSpawnedUnitsEliminated(false);

		int successfulSpawns = ConvergeGroups(m_bReinforceFromNearestBase);

		if (successfulSpawns <= 0)
			m_bSpawnedUnitsEliminated = wasEliminated;

		// Recompute the deployment-wide flag from what the modules now actually hold, on both paths.
		m_ParentDeployment.CheckAllSpawningModulesEliminated();

		if (successfulSpawns <= 0)
		{
			Print(string.Format("Reinforcement bought 0/%1 groups, cost: %2 resources", groupsNeeded, totalCost), LogLevel.WARNING);
			return false;
		}

		Print(string.Format("Reinforced with %1/%2 groups, cost: %3 resources", successfulSpawns, groupsNeeded, totalCost), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected vector GetNearestControlledBasePosition(int factionIndex)
	{
		if (!m_ParentDeployment)
			return vector.Zero;

		vector deploymentPos = m_ParentDeployment.GetPosition();

		// Get occupying faction manager to check bases
		OVT_OccupyingFactionManager ofManager = OVT_Global.GetOccupyingFaction();
		if (!ofManager)
			return vector.Zero;

		// Get nearest base and check if it's controlled by our faction
		OVT_BaseData nearestBase = ofManager.GetNearestBase(deploymentPos);
		if (nearestBase && nearestBase.faction == factionIndex)
		{
			return nearestBase.location;
		}

		return vector.Zero;
	}

	//------------------------------------------------------------------------------------------------
	// Status methods for behavior modules
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \return How many registered groups this module currently holds.
	int GetGroupCount()
	{
		return m_aHandles.Count();
	}

	//------------------------------------------------------------------------------------------------
	int GetMissingGroupCount()
	{
		return OVT_DeploymentVirtualKey.MissingCount(GetMaxGroupCount(), m_aHandles.Count());
	}

	//------------------------------------------------------------------------------------------------
	//! The count this module wants to hold, rolled ONCE and remembered - a re-roll on a later pass
	//! would quietly grow or shrink a force that already exists.
	//! \return The wanted group count.
	int GetMaxGroupCount()
	{
		if (m_iActualGroupCount > 0)
			return m_iActualGroupCount;

		if (!m_ParentDeployment)
			return m_iMaxGroupCount;

		m_iActualGroupCount = CalculateGroupCount(m_ParentDeployment.GetPosition());

		return m_iActualGroupCount;
	}

	//------------------------------------------------------------------------------------------------
	int GetReinforcementCost()
	{
		return m_iReinforcementCost;
	}

	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Infantry Module: %1", m_sModuleName));
		Print(string.Format("  Groups: %1/%2 registered (ever held: %3, min: %4, max: %5)", m_aHandles.Count(), GetMaxGroupCount(), m_iSpawnedEver, m_iMinGroupCount, m_iMaxGroupCount));
		Print(string.Format("  Group Type: %1", m_sGroupType));
		Print(string.Format("  Owner Key: %1", GetOwnerKey()));
		Print(string.Format("  Importance: %1", typename.EnumToString(SCR_EAISpawnImportance, m_eImportance)));
		string townScaling = "No";
		if (m_bScaleByTownSize)
			townScaling = "Yes";
		Print(string.Format("  Town Size Scaling: %1", townScaling));
		string reinforcementStatus = "Disabled";
		if (m_bAllowReinforcement)
			reinforcementStatus = "Enabled";
		Print(string.Format("  Reinforcement: %1", reinforcementStatus));

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		foreach (int handle : m_aHandles)
		{
			Print(string.Format("    Handle %1: %2/%3 alive at %4", handle,
				virtualization.GetAliveMemberCount(handle), virtualization.GetMemberCount(handle),
				virtualization.GetPosition(handle).ToString()));
		}
	}
}
