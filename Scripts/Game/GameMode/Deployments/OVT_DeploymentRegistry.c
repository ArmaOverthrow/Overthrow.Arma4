[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sRegistryName")]
class OVT_DeploymentRegistry : ScriptAndConfig
{
	[Attribute(desc: "Name of this deployment registry")]
	string m_sRegistryName;
	
	[Attribute(desc: "All available deployment configurations")]
	ref array<ref OVT_DeploymentConfig> m_aDeploymentConfigs;
	
	//------------------------------------------------------------------------------------------------
	void OVT_DeploymentRegistry()
	{
		if (!m_aDeploymentConfigs)
			m_aDeploymentConfigs = new array<ref OVT_DeploymentConfig>;
	}
	
	//------------------------------------------------------------------------------------------------
	// Utility methods for finding configs
	//------------------------------------------------------------------------------------------------
	OVT_DeploymentConfig FindConfigByName(string name)
	{
		foreach (OVT_DeploymentConfig config : m_aDeploymentConfigs)
		{
			if (config.m_sDeploymentName == name)
				return config;
		}
		return null;
	}
	
	//------------------------------------------------------------------------------------------------
	array<OVT_DeploymentConfig> GetConfigsForFaction(OVT_FactionType factionType)
	{
		array<OVT_DeploymentConfig> configs = new array<OVT_DeploymentConfig>;
		
		foreach (OVT_DeploymentConfig config : m_aDeploymentConfigs)
		{
			if (config.CanFactionUse(factionType))
				configs.Insert(config);
		}
		
		return configs;
	}
	
	//------------------------------------------------------------------------------------------------
	array<OVT_DeploymentConfig> GetConfigsByPriority(int maxPriority = 20)
	{
		array<OVT_DeploymentConfig> configs = new array<OVT_DeploymentConfig>;
		
		foreach (OVT_DeploymentConfig config : m_aDeploymentConfigs)
		{
			if (config.m_iPriority <= maxPriority)
				configs.Insert(config);
		}
		
		// Sort by priority (lower number = higher priority)
		configs.Sort();
		
		return configs;
	}
	
	//------------------------------------------------------------------------------------------------
	array<OVT_DeploymentConfig> GetConfigsInCostRange(int minCost, int maxCost)
	{
		array<OVT_DeploymentConfig> configs = new array<OVT_DeploymentConfig>;
		
		foreach (OVT_DeploymentConfig config : m_aDeploymentConfigs)
		{
			int cost = config.GetTotalResourceCost();
			if (cost >= minCost && cost <= maxCost)
				configs.Insert(config);
		}
		
		return configs;
	}
	
	//------------------------------------------------------------------------------------------------
	bool ValidateAllConfigs()
	{
		bool allValid = true;
		
		foreach (OVT_DeploymentConfig config : m_aDeploymentConfigs)
		{
			if (!config.IsValidConfig())
			{
				Print(string.Format("Invalid deployment config: %1", config.m_sDeploymentName), LogLevel.ERROR);
				allValid = false;
			}
		}
		
		return allValid;
	}
	
	//------------------------------------------------------------------------------------------------
	void PrintRegistryInfo()
	{
		Print(string.Format("Deployment Registry: %1", m_sRegistryName));
		Print(string.Format("  Total Configs: %1", m_aDeploymentConfigs.Count()));
		
		map<int, int> factionCounts = new map<int, int>;
		
		foreach (OVT_DeploymentConfig config : m_aDeploymentConfigs)
		{
			// Count deployments available to each faction type
			if (config.m_iAllowedFactionTypes & OVT_FactionTypeFlag.OCCUPYING_FACTION)
			{
				int count = factionCounts.Get(OVT_FactionTypeFlag.OCCUPYING_FACTION);
				factionCounts.Set(OVT_FactionTypeFlag.OCCUPYING_FACTION, count + 1);
			}
			if (config.m_iAllowedFactionTypes & OVT_FactionTypeFlag.RESISTANCE_FACTION)
			{
				int count = factionCounts.Get(OVT_FactionTypeFlag.RESISTANCE_FACTION);
				factionCounts.Set(OVT_FactionTypeFlag.RESISTANCE_FACTION, count + 1);
			}
			if (config.m_iAllowedFactionTypes & OVT_FactionTypeFlag.SUPPORTING_FACTION)
			{
				int count = factionCounts.Get(OVT_FactionTypeFlag.SUPPORTING_FACTION);
				factionCounts.Set(OVT_FactionTypeFlag.SUPPORTING_FACTION, count + 1);
			}
		}
		
		foreach (int factionType, int count : factionCounts)
		{
			string factionName = "";
			switch (factionType)
			{
				case OVT_FactionType.OCCUPYING_FACTION: factionName = "Occupying"; break;
				case OVT_FactionType.RESISTANCE_FACTION: factionName = "Resistance"; break;
				case OVT_FactionType.SUPPORTING_FACTION: factionName = "Supporting"; break;
			}
			Print(string.Format("  %1: %2 configs", factionName, count));
		}
	}
}