//! Infantry spawning module for deployments
//! Handles spawning and management of infantry groups
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
	
	protected ref array<SCR_AIGroup> m_aSpawnedGroups;
	protected int m_iSpawnedCount;
	
	//------------------------------------------------------------------------------------------------
	void OVT_InfantrySpawningDeploymentModule()
	{
		m_aSpawnedGroups = new array<SCR_AIGroup>;
		m_iSpawnedCount = 0;
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
		
		if (!m_ParentDeployment)
			return;
		
		// Check if groups have been eliminated previously - if so, don't spawn unless reinforcement resets this
		if (m_bSpawnedUnitsEliminated || m_ParentDeployment.GetSpawnedUnitsEliminated())
		{			
			return;
		}
			
		SpawnInfantryGroups();
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnDeactivate()
	{
		super.OnDeactivate();
				
		// Clean up spawned groups
		foreach (SCR_AIGroup group : m_aSpawnedGroups)
		{
			if (group)
				OVT_EntitySpawningAPI.CleanupGroup(group);
		}
		
		m_aSpawnedGroups.Clear();
		m_iSpawnedCount = 0;
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnUpdate(int deltaTime)
	{
		super.OnUpdate(deltaTime);
		
		// Check for destroyed groups and handle reinforcement
		CheckGroupStatus();
	}
	
	//------------------------------------------------------------------------------------------------
	override array<IEntity> GetSpawnedEntities()
	{
		array<IEntity> entities = new array<IEntity>;
		
		foreach (SCR_AIGroup group : m_aSpawnedGroups)
		{
			if (group)
				entities.Insert(group);
		}
		
		return entities;
	}
	
	//------------------------------------------------------------------------------------------------
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_InfantrySpawningDeploymentModule clone = new OVT_InfantrySpawningDeploymentModule();
		
		// Copy configuration
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
		
		return clone;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void SpawnInfantryGroups()
	{
		if (!m_ParentDeployment || m_sGroupType.IsEmpty())
			return;
			
		vector deploymentPos = m_ParentDeployment.GetPosition();
		int factionIndex = m_ParentDeployment.GetControllingFaction();
		
		// Determine spawn position based on settings
		vector baseSpawnPos = deploymentPos;
		if (m_bSpawnAtNearestBase)
		{
			vector nearestBasePos = GetNearestControlledBasePosition(factionIndex);
			if (nearestBasePos != vector.Zero)
			{
				baseSpawnPos = nearestBasePos;
				Print(string.Format("Infantry will spawn at nearest base: %1", baseSpawnPos.ToString()), LogLevel.NORMAL);
			}
			else
			{
				Print("No controlled base found, aborting initial spawn", LogLevel.WARNING);
				return;
			}
		}
		
		// Calculate actual group count based on difficulty and town size
		m_iActualGroupCount = CalculateGroupCount(deploymentPos);
		
		// Get the group prefab from faction registry
		ResourceName groupPrefab = GetGroupPrefabFromFaction(factionIndex);
		if (groupPrefab.IsEmpty())
		{
			Print(string.Format("Failed to get group prefab for type '%1' from faction %2", m_sGroupType, factionIndex), LogLevel.ERROR);
			return;
		}
		
		for (int i = 0; i < m_iActualGroupCount; i++)
		{
			vector spawnPos = GetRandomSpawnPosition(baseSpawnPos);
			
			SCR_AIGroup group = OVT_EntitySpawningAPI.SpawnInfantryGroup(
				groupPrefab, 
				spawnPos, 
				"0 0 0", 
				factionIndex
			);
			
			if (group)
			{
				m_aSpawnedGroups.Insert(group);
				m_iSpawnedCount++;
				
				Print(string.Format("Spawned infantry group %1 (%2) at %3", i + 1, m_sGroupType, spawnPos.ToString()), LogLevel.VERBOSE);
			}
			else
			{
				Print(string.Format("Failed to spawn infantry group %1 (%2)", i + 1, m_sGroupType), LogLevel.ERROR);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected vector GetRandomSpawnPosition(vector center)
	{
		float angle = Math.RandomFloat01() * Math.PI2;
		float distance = Math.RandomFloat(10, m_fSpawnRadius);
		
		vector offset = Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);
		vector spawnPos = center + offset;
		
		//Find nearest road
		vector roadPos = OVT_Global.FindNearestRoad(spawnPos);
				
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
	override protected bool CheckIfUnitsEliminated()
	{
		// Clean up dead/destroyed groups first
		for (int i = m_aSpawnedGroups.Count() - 1; i >= 0; i--)
		{
			SCR_AIGroup group = m_aSpawnedGroups[i];
			if (!group || !IsGroupAlive(group))
			{
				// Group is destroyed or dead, remove from array
				m_aSpawnedGroups.Remove(i);
			}
		}
		
		// Return true if all groups have been eliminated (spawned some but none alive)
		return m_aSpawnedGroups.IsEmpty() && m_iSpawnedCount > 0;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void CheckGroupStatus()
	{
		// Use the base class elimination checking
		bool wasEliminated = m_bSpawnedUnitsEliminated;
		bool isNowEliminated = CheckIfUnitsEliminated();
				
		// Update elimination state if changed
		if (!wasEliminated && isNowEliminated)
		{
			m_bSpawnedUnitsEliminated = true;
			
			// Check if ALL spawning modules in this deployment have been eliminated
			if (m_ParentDeployment)
				m_ParentDeployment.CheckAllSpawningModulesEliminated();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool IsGroupAlive(SCR_AIGroup group)
	{
		if (!group)
			return false;
			
		// Simple check: if group has no agents, it's dead
		return group.GetAgentsCount() > 0;
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
	bool Reinforce(int groupsNeeded)
	{
		if (!m_ParentDeployment)
			return false;
			
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
			return false;
			
		// Check if we can afford the reinforcement
		if (!CanReinforce(groupsNeeded))
			return false;
			
		int factionIndex = m_ParentDeployment.GetControllingFaction();
		int totalCost = groupsNeeded * m_iReinforcementCost;
		
		// Deduct resources and spawn reinforcements
		manager.SubtractFactionResources(factionIndex, totalCost);
		
		vector deploymentPos = m_ParentDeployment.GetPosition();
		
		// Determine spawn position for reinforcements
		vector baseSpawnPos = deploymentPos;
		if (m_bReinforceFromNearestBase)
		{
			vector nearestBasePos = GetNearestControlledBasePosition(factionIndex);
			if (nearestBasePos != vector.Zero)
			{
				baseSpawnPos = nearestBasePos;
				Print(string.Format("Reinforcements will spawn at nearest base: %1", baseSpawnPos.ToString()), LogLevel.NORMAL);
			}
			else
			{
				Print("No controlled base found, aborting reinforcement", LogLevel.WARNING);
				return false;
			}
		}
		
		int successfulSpawns = 0;
		
		// Get the group prefab from faction registry
		ResourceName groupPrefab = GetGroupPrefabFromFaction(factionIndex);
		if (groupPrefab.IsEmpty())
		{
			Print(string.Format("Failed to get group prefab for reinforcement, type '%1' from faction %2", m_sGroupType, factionIndex), LogLevel.ERROR);
			return false;
		}
		
		for (int i = 0; i < groupsNeeded; i++)
		{
			vector spawnPos = GetRandomSpawnPosition(baseSpawnPos);
			
			SCR_AIGroup group = OVT_EntitySpawningAPI.SpawnInfantryGroup(
				groupPrefab, 
				spawnPos, 
				"0 0 0", 
				factionIndex
			);
			
			if (group)
			{
				m_aSpawnedGroups.Insert(group);
				successfulSpawns++;
				Print(string.Format("Reinforcement group (%1) spawned at %2", m_sGroupType, spawnPos.ToString()), LogLevel.NORMAL);
			}
		}
		
		// Reset elimination flag if reinforcements were successful
		if (successfulSpawns > 0)
		{
			m_bSpawnedUnitsEliminated = false;
			
			// Re-check all spawning modules since this one is no longer eliminated
			if (m_ParentDeployment)
				m_ParentDeployment.CheckAllSpawningModulesEliminated();
		}
		
		Print(string.Format("Reinforced with %1/%2 groups, cost: %3 resources", successfulSpawns, groupsNeeded, totalCost), LogLevel.NORMAL);
		return successfulSpawns > 0;
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
	// Faction group registry integration
	//------------------------------------------------------------------------------------------------
	protected ResourceName GetGroupPrefabFromFaction(int factionIndex)
	{
		// Get the OVT_Faction directly by index
		OVT_Faction ovtFaction = OVT_Global.GetFactions().GetOverthrowFactionByIndex(factionIndex);
		if (!ovtFaction)
		{
			Print(string.Format("Failed to get OVT_Faction for index %1", factionIndex), LogLevel.ERROR);
			return "";
		}
		
		// Initialize group registry if needed
		ovtFaction.InitializeGroupRegistry();
		
		// Get the group prefab from the registry
		ResourceName groupPrefab = ovtFaction.GetGroupPrefabByName(m_sGroupType);
		if (groupPrefab.IsEmpty())
		{
			Print(string.Format("Group type '%1' not found in faction '%2' registry", m_sGroupType, ovtFaction.GetFactionKey()), LogLevel.WARNING);
		}
		
		return groupPrefab;
	}
	
	//------------------------------------------------------------------------------------------------
	// Status methods for behavior modules
	//------------------------------------------------------------------------------------------------
	int GetAliveGroupCount()
	{
		int aliveCount = 0;
		foreach (SCR_AIGroup group : m_aSpawnedGroups)
		{
			if (group && IsGroupAlive(group))
				aliveCount++;
		}
		return aliveCount;
	}
	
	//------------------------------------------------------------------------------------------------
	int GetMissingGroupCount()
	{
		return Math.Max(0, GetMaxGroupCount() - GetAliveGroupCount());
	}
	
	//------------------------------------------------------------------------------------------------
	int GetMaxGroupCount()
	{
		// Return the actual calculated group count, or max if not calculated yet
		if (m_iActualGroupCount > 0)
			return m_iActualGroupCount;
		return m_iMaxGroupCount;
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
		Print(string.Format("  Groups: %1/%2 alive (calculated: %3, min: %4, max: %5)", GetAliveGroupCount(), GetMaxGroupCount(), m_iActualGroupCount, m_iMinGroupCount, m_iMaxGroupCount));
		Print(string.Format("  Group Type: %1", m_sGroupType));
		string townScaling = "No";
		if (m_bScaleByTownSize)
			townScaling = "Yes";
		Print(string.Format("  Town Size Scaling: %1", townScaling));
		string reinforcementStatus = "Disabled";
		if (m_bAllowReinforcement)
			reinforcementStatus = "Enabled";
		Print(string.Format("  Reinforcement: %1", reinforcementStatus));
	}
}