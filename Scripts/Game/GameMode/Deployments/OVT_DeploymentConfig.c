// Location type flags for deployments
enum OVT_LocationTypeFlag
{
	TOWN = 1,
	BASE = 2,
	PORT = 4,
	AIRFIELD = 8,
	RADIO_TOWER = 16,
	CHECKPOINT = 32,
	OPEN_TERRAIN = 64
}

[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sDeploymentName")]
class OVT_DeploymentConfig : ScriptAndConfig
{
	[Attribute(desc: "Name of this deployment type")]
	string m_sDeploymentName;
	
	[Attribute(desc: "Modules that make up this deployment")]
	ref array<ref OVT_BaseDeploymentModule> m_aModules;
	
	[Attribute("1", UIWidgets.Flags, enums: ParamEnumArray.FromEnum(OVT_FactionTypeFlag))]
	OVT_FactionTypeFlag m_iAllowedFactionTypes;
	
	[Attribute("1", UIWidgets.Flags, enums: ParamEnumArray.FromEnum(OVT_LocationTypeFlag), desc: "Valid location types for this deployment")]
	OVT_LocationTypeFlag m_iAllowedLocationTypes;
	
	[Attribute(defvalue: "100", desc: "Base resource cost to create this deployment")]
	int m_iBaseCost;
	
	[Attribute(defvalue: "0", desc: "Minimum threat level required")]
	int m_iMinimumThreatLevel;
	
	[Attribute(defvalue: "5", desc: "Priority (1-20, lower = higher priority)")]
	int m_iPriority;
	
	[Attribute(defvalue: "1000", desc: "Range for proximity-based activation")]
	float m_fActivationRange;
	
	[Attribute(defvalue: "1", desc: "When false, will always be activated/spawned regardless of player proximity")]
	bool m_bEnableProximityActivation;
	
	[Attribute(defvalue: "-1", desc: "Resource allocation limit (-1 = no limit)")]
	int m_iResourceAllocation;
	
	[Attribute(defvalue: "100", desc: "Chance this deployment will be created (0-100, where 100 = 100% chance)")]
	float m_fChance;
	
	[Attribute(defvalue: "-1", desc: "Maximum number of active instances of this deployment type (-1 = no limit)")]
	int m_iMaxInstances;
	
	//------------------------------------------------------------------------------------------------
	void OVT_DeploymentConfig()
	{
		if (!m_aModules)
			m_aModules = new array<ref OVT_BaseDeploymentModule>;
			
		// Set default to allow only occupying faction
		if (m_iAllowedFactionTypes == 0)
			m_iAllowedFactionTypes = 1;
	}
	
	//------------------------------------------------------------------------------------------------
	// Validation and utility methods
	//------------------------------------------------------------------------------------------------
	bool IsValidConfig()
	{
		if (m_sDeploymentName.IsEmpty())
			return false;
			
		if (!m_aModules || m_aModules.IsEmpty())
			return false;
			
		// Check that we have at least one spawning module
		bool hasSpawningModule = false;
		foreach (OVT_BaseDeploymentModule module : m_aModules)
		{
			if (OVT_BaseSpawningDeploymentModule.Cast(module))
			{
				hasSpawningModule = true;
				break;
			}
		}
		
		if (!hasSpawningModule)
		{
			Print(string.Format("Deployment config '%1' has no spawning modules", m_sDeploymentName), LogLevel.WARNING);
			return false;
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	bool CanFactionUse(OVT_FactionTypeFlag factionType)
	{
		if (m_iAllowedFactionTypes == 7)
			return true; // No restrictions
			
		return (factionType & m_iAllowedFactionTypes) != 0;
	}
	
	//------------------------------------------------------------------------------------------------
	bool CanUseLocationType(OVT_LocationTypeFlag locationType)
	{
		if (m_iAllowedLocationTypes == 0)
			return true; // No restrictions if not set
			
		return (locationType & m_iAllowedLocationTypes) != 0;
	}
	
	//------------------------------------------------------------------------------------------------
	int GetTotalResourceCost(int difficultyMultiplier = 1)
	{
		int totalCost = m_iBaseCost * difficultyMultiplier;
		
		// Add module-specific costs
		foreach (OVT_BaseDeploymentModule module : m_aModules)
		{
			totalCost += module.GetResourceCost();
		}
		
		return totalCost;
	}
	
	//------------------------------------------------------------------------------------------------
	array<OVT_BaseSpawningDeploymentModule> GetSpawningModules()
	{
		array<OVT_BaseSpawningDeploymentModule> spawningModules = new array<OVT_BaseSpawningDeploymentModule>;
		
		foreach (OVT_BaseDeploymentModule module : m_aModules)
		{
			OVT_BaseSpawningDeploymentModule spawningModule = OVT_BaseSpawningDeploymentModule.Cast(module);
			if (spawningModule)
				spawningModules.Insert(spawningModule);
		}
		
		return spawningModules;
	}
	
	//------------------------------------------------------------------------------------------------
	array<OVT_BaseBehaviorDeploymentModule> GetBehaviorModules()
	{
		array<OVT_BaseBehaviorDeploymentModule> behaviorModules = new array<OVT_BaseBehaviorDeploymentModule>;
		
		foreach (OVT_BaseDeploymentModule module : m_aModules)
		{
			OVT_BaseBehaviorDeploymentModule behaviorModule = OVT_BaseBehaviorDeploymentModule.Cast(module);
			if (behaviorModule)
				behaviorModules.Insert(behaviorModule);
		}
		
		return behaviorModules;
	}
	
	//------------------------------------------------------------------------------------------------
	array<OVT_BaseConditionDeploymentModule> GetConditionModules()
	{
		array<OVT_BaseConditionDeploymentModule> conditionModules = new array<OVT_BaseConditionDeploymentModule>;
		
		foreach (OVT_BaseDeploymentModule module : m_aModules)
		{
			OVT_BaseConditionDeploymentModule conditionModule = OVT_BaseConditionDeploymentModule.Cast(module);
			if (conditionModule)
				conditionModules.Insert(conditionModule);
		}
		
		return conditionModules;
	}
	
	//------------------------------------------------------------------------------------------------
	bool RequiresSlots()
	{
		foreach (OVT_BaseDeploymentModule module : m_aModules)
		{
			// Check if any spawning modules require specific slots
			OVT_BaseSpawningDeploymentModule spawningModule = OVT_BaseSpawningDeploymentModule.Cast(module);
			if (spawningModule)
			{
				// This would need to be implemented in derived classes
				// For now, assume no slot requirements
			}
		}
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	string GetRequiredSlotType()
	{
		// Return the most restrictive slot type required by any module
		// This would need to be implemented based on specific module requirements
		return "";
	}
	
	//------------------------------------------------------------------------------------------------
	// Debug and logging
	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Deployment Config: %1", m_sDeploymentName));
		Print(string.Format("  Base Cost: %1", m_iBaseCost));
		Print(string.Format("  Priority: %1", m_iPriority));
		Print(string.Format("  Min Threat: %1", m_iMinimumThreatLevel));
		Print(string.Format("  Modules: %1", m_aModules.Count()));
		
		foreach (OVT_BaseDeploymentModule module : m_aModules)
		{
			Print(string.Format("    - %1", module.Type().ToString()));
		}
		
		if (m_iAllowedFactionTypes != 0)
		{
			Print("  Allowed Factions:");
			if (m_iAllowedFactionTypes & OVT_FactionTypeFlag.OCCUPYING_FACTION)
				Print("    - Occupying");
			if (m_iAllowedFactionTypes & OVT_FactionTypeFlag.RESISTANCE_FACTION)
				Print("    - Resistance");
			if (m_iAllowedFactionTypes & OVT_FactionTypeFlag.SUPPORTING_FACTION)
				Print("    - Supporting");
		}
	}
}