[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sDeploymentName")]
class OVT_DeploymentConfig : ScriptAndConfig
{
	[Attribute(desc: "Name of this deployment type")]
	string m_sDeploymentName;
	
	[Attribute(desc: "Modules that make up this deployment")]
	ref array<ref OVT_BaseDeploymentModule> m_aModules;
	
	[Attribute(desc: "Faction types allowed to use this deployment")]
	ref array<string> m_aAllowedFactionTypes;
	
	[Attribute(defvalue: "100", desc: "Base resource cost to create this deployment")]
	int m_iBaseCost;
	
	[Attribute(defvalue: "0", desc: "Minimum threat level required")]
	int m_iMinimumThreatLevel;
	
	[Attribute(defvalue: "5", desc: "Priority (1-20, lower = higher priority)")]
	int m_iPriority;
	
	[Attribute(defvalue: "1000", desc: "Range for proximity-based activation")]
	float m_fActivationRange;
	
	[Attribute(defvalue: "-1", desc: "Resource allocation limit (-1 = no limit)")]
	int m_iResourceAllocation;
	
	//------------------------------------------------------------------------------------------------
	void OVT_DeploymentConfig()
	{
		if (!m_aModules)
			m_aModules = new array<ref OVT_BaseDeploymentModule>;
			
		if (!m_aAllowedFactionTypes)
			m_aAllowedFactionTypes = new array<string>;
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
	bool CanFactionUse(string factionType)
	{
		if (!m_aAllowedFactionTypes || m_aAllowedFactionTypes.IsEmpty())
			return true; // No restrictions
			
		return m_aAllowedFactionTypes.Contains(factionType);
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
		
		if (!m_aAllowedFactionTypes.IsEmpty())
		{
			Print("  Allowed Factions:");
			foreach (string factionType : m_aAllowedFactionTypes)
			{
				Print(string.Format("    - %1", factionType));
			}
		}
	}
}