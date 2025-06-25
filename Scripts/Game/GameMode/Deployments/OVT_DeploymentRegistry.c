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
	array<OVT_DeploymentConfig> GetConfigsForFaction(string factionType)
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
		
		map<string, int> factionCounts = new map<string, int>;
		
		foreach (OVT_DeploymentConfig config : m_aDeploymentConfigs)
		{
			foreach (string factionType : config.m_aAllowedFactionTypes)
			{
				int count = factionCounts.Get(factionType);
				factionCounts.Set(factionType, count + 1);
			}
		}
		
		foreach (string factionType, int count : factionCounts)
		{
			Print(string.Format("  %1: %2 configs", factionType, count));
		}
	}
}