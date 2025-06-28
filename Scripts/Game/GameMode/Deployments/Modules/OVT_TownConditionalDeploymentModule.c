//! Town-based conditional deployment module
//! Provides conditions based on town properties and state
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_TownConditionalDeploymentModule : OVT_BaseConditionDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;
	
	[Attribute(defvalue: "500", desc: "Maximum distance from town center to consider deployment as 'in town'")]
	float m_fMaxTownDistance;
	
	[Attribute(defvalue: "1", desc: "Minimum town size required (1=village, 2=town, 3=city)")]
	int m_iMinTownSize;
	
	[Attribute(defvalue: "3", desc: "Maximum town size allowed (1=village, 2=town, 3=city)")]
	int m_iMaxTownSize;
	
	[Attribute(defvalue: "0", desc: "Minimum support percentage required")]
	int m_iMinSupportPercentage;
	
	[Attribute(defvalue: "100", desc: "Maximum support percentage allowed")]
	int m_iMaxSupportPercentage;
	
	[Attribute(defvalue: "0", desc: "Minimum stability required")]
	int m_iMinStability;
	
	[Attribute(defvalue: "100", desc: "Maximum stability allowed")]
	int m_iMaxStability;
	
	[Attribute(defvalue: "false", desc: "Require town to be controlled by deployment faction")]
	bool m_bRequireFactionControl;
	
	protected OVT_TownData m_CachedTown;
	protected float m_fLastCheckTime;
	protected float m_fCacheTimeout;
	
	//------------------------------------------------------------------------------------------------
	void OVT_TownConditionalDeploymentModule()
	{
		m_fCacheTimeout = 30.0; // Cache town data for 30 seconds
		m_fLastCheckTime = 0;
		m_CachedTown = null;
	}
	
	//------------------------------------------------------------------------------------------------
	override bool EvaluateCondition()
	{
		if (!m_ParentDeployment)
			return false;
		
		OVT_TownData town = GetNearestTown();
		if (!town)
		{
			Print("Town conditional: No town found within range", LogLevel.VERBOSE);
			return false;
		}
		
		// Check town size requirements
		if (town.size < m_iMinTownSize || town.size > m_iMaxTownSize)
		{
			Print(string.Format("Town conditional: Town size %1 outside range [%2-%3]", 
				town.size, m_iMinTownSize, m_iMaxTownSize), LogLevel.VERBOSE);
			return false;
		}
		
		// Check support requirements
		int supportPercentage = town.SupportPercentage();
		if (supportPercentage < m_iMinSupportPercentage || supportPercentage > m_iMaxSupportPercentage)
		{
			Print(string.Format("Town conditional: Support %1%% outside range [%2%%-3%%]", 
				supportPercentage, m_iMinSupportPercentage, m_iMaxSupportPercentage), LogLevel.VERBOSE);
			return false;
		}
		
		// Check stability requirements
		if (town.stability < m_iMinStability || town.stability > m_iMaxStability)
		{
			Print(string.Format("Town conditional: Stability %1 outside range [%2-%3]", 
				town.stability, m_iMinStability, m_iMaxStability), LogLevel.VERBOSE);
			return false;
		}
		
		// Check faction control if required
		if (m_bRequireFactionControl)
		{
			int deploymentFaction = m_ParentDeployment.GetControllingFaction();
			if (town.faction != deploymentFaction)
			{
				Print(string.Format("Town conditional: Town controlled by faction %1, deployment by faction %2", 
					town.faction, deploymentFaction), LogLevel.VERBOSE);
				return false;
			}
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_TownConditionalDeploymentModule clone = new OVT_TownConditionalDeploymentModule();
		
		// Copy configuration
		clone.m_sModuleName = m_sModuleName;
		clone.m_fMaxTownDistance = m_fMaxTownDistance;
		clone.m_iMinTownSize = m_iMinTownSize;
		clone.m_iMaxTownSize = m_iMaxTownSize;
		clone.m_iMinSupportPercentage = m_iMinSupportPercentage;
		clone.m_iMaxSupportPercentage = m_iMaxSupportPercentage;
		clone.m_iMinStability = m_iMinStability;
		clone.m_iMaxStability = m_iMaxStability;
		clone.m_bRequireFactionControl = m_bRequireFactionControl;
		
		return clone;
	}
	
	//------------------------------------------------------------------------------------------------
	// Town-specific utility methods
	//------------------------------------------------------------------------------------------------
	OVT_TownData GetNearestTown()
	{
		if (!m_ParentDeployment)
			return null;
		
		// Check cache first
		float currentTime = GetGame().GetWorld().GetWorldTime();
		if (m_CachedTown && (currentTime - m_fLastCheckTime) < m_fCacheTimeout)
		{
			return m_CachedTown;
		}
		
		// Get fresh town data
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		if (!townManager)
			return null;
		
		vector deploymentPos = m_ParentDeployment.GetPosition();
		OVT_TownData nearestTown = townManager.GetNearestTown(deploymentPos);
		
		if (nearestTown)
		{
			// Check if town is within acceptable distance
			float distance = vector.Distance(deploymentPos, nearestTown.location);
			if (distance > m_fMaxTownDistance)
			{
				Print(string.Format("Town conditional: Nearest town at distance %1m exceeds max %2m", 
					distance, m_fMaxTownDistance), LogLevel.VERBOSE);
				return null;
			}
		}
		
		// Update cache
		m_CachedTown = nearestTown;
		m_fLastCheckTime = currentTime;
		
		return nearestTown;
	}
	
	//------------------------------------------------------------------------------------------------
	float GetDistanceToTown()
	{
		OVT_TownData town = GetNearestTown();
		if (!town || !m_ParentDeployment)
			return -1;
		
		return vector.Distance(m_ParentDeployment.GetPosition(), town.location);
	}
	
	//------------------------------------------------------------------------------------------------
	string GetTownName()
	{
		OVT_TownData town = GetNearestTown();
		if (!town)
			return "";
		
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		if (!townManager)
			return "";
		
		// Find town index to get name
		for (int i = 0; i < townManager.m_Towns.Count(); i++)
		{
			if (townManager.m_Towns[i] == town)
			{
				if (i < townManager.m_TownNames.Count())
					return townManager.m_TownNames[i];
				break;
			}
		}
		
		return string.Format("Town_%1", town.location.ToString());
	}
	
	//------------------------------------------------------------------------------------------------
	bool IsInTown()
	{
		return GetNearestTown() != null;
	}
	
	//------------------------------------------------------------------------------------------------
	bool IsTownControlledByFaction(int factionIndex)
	{
		OVT_TownData town = GetNearestTown();
		if (!town)
			return false;
		
		return town.faction == factionIndex;
	}
	
	//------------------------------------------------------------------------------------------------
	string GetTownSizeDescription()
	{
		OVT_TownData town = GetNearestTown();
		if (!town)
			return "No Town";
		
		switch (town.size)
		{
			case 1: 
				return "Village";
			case 2: 
				return "Town";
			case 3: 
				return "City";
		}
		
		return "Unknown";
	}
	
	//------------------------------------------------------------------------------------------------
	// Debug methods
	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Town Conditional Module: %1", m_sModuleName));
		Print(string.Format("  Max Distance: %1m", m_fMaxTownDistance));
		Print(string.Format("  Size Range: %1-%2", m_iMinTownSize, m_iMaxTownSize));
		Print(string.Format("  Support Range: %1%%-2%%", m_iMinSupportPercentage, m_iMaxSupportPercentage));
		Print(string.Format("  Stability Range: %1-%2", m_iMinStability, m_iMaxStability));
		string requireFactionControl = "No";
		if (m_bRequireFactionControl)
			requireFactionControl = "Yes";
		Print(string.Format("  Require Faction Control: %1", requireFactionControl));
		
		OVT_TownData town = GetNearestTown();
		if (town)
		{
			Print(string.Format("  Current Town: %1", GetTownName()));
			Print(string.Format("  Distance: %1m", GetDistanceToTown()));
			Print(string.Format("  Size: %1 (%2)", town.size, GetTownSizeDescription()));
			Print(string.Format("  Support: %1%%", town.SupportPercentage()));
			Print(string.Format("  Stability: %1", town.stability));
			Print(string.Format("  Faction: %1", town.faction));
		}
		else
		{
			Print("  No town within range");
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void InvalidateCache()
	{
		m_CachedTown = null;
		m_fLastCheckTime = 0;
	}
}