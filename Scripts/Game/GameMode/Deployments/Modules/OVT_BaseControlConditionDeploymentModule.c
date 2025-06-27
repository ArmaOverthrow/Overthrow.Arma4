//! Base control condition module for deployments
//! Checks if the nearest base is controlled by the deployment's faction
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_BaseControlConditionDeploymentModule : OVT_BaseConditionDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;
	
	[Attribute(defvalue: "1000", desc: "Maximum distance to check for bases")]
	float m_fMaxDistance;
	
	[Attribute(defvalue: "true", desc: "If true, condition passes when faction controls base. If false, passes when faction does NOT control base")]
	bool m_bRequireControl;
	
	protected OVT_BaseData m_NearestBase;
	
	//------------------------------------------------------------------------------------------------
	override bool EvaluateCondition()
	{
		if (!m_ParentDeployment)
			return false;
			
		vector deploymentPos = m_ParentDeployment.GetPosition();
		int factionIndex = m_ParentDeployment.GetControllingFaction();
		
		// Find nearest base
		OVT_OccupyingFactionManager occupyingFaction = OVT_Global.GetOccupyingFaction();
		if (!occupyingFaction)
			return false;
			
		m_NearestBase = occupyingFaction.GetNearestBase(deploymentPos);
		if (!m_NearestBase)
			return false;
			
		// Check distance
		float distance = vector.Distance(deploymentPos, m_NearestBase.location);
		if (distance > m_fMaxDistance)
			return false;
			
		// Check control
		bool isControlled = (m_NearestBase.faction == factionIndex);
		
		// Return based on requirement
		return (isControlled == m_bRequireControl);
	}
	
	//------------------------------------------------------------------------------------------------
	override bool EvaluateStaticCondition(vector position, int factionIndex, float threatLevel)
	{
		// Find nearest base
		OVT_OccupyingFactionManager occupyingFaction = OVT_Global.GetOccupyingFaction();
		if (!occupyingFaction)
			return false;
			
		OVT_BaseData nearestBase = occupyingFaction.GetNearestBase(position);
		if (!nearestBase)
			return false;
			
		// Check distance
		float distance = vector.Distance(position, nearestBase.location);
		if (distance > m_fMaxDistance)
			return false;
			
		// Check control
		bool isControlled = (nearestBase.faction == factionIndex);
		
		// Return based on requirement
		return (isControlled == m_bRequireControl);
	}
	
	//------------------------------------------------------------------------------------------------
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_BaseControlConditionDeploymentModule clone = new OVT_BaseControlConditionDeploymentModule();
		
		// Copy configuration
		clone.m_sModuleName = m_sModuleName;
		clone.m_fMaxDistance = m_fMaxDistance;
		clone.m_bRequireControl = m_bRequireControl;
		
		return clone;
	}
	
	//------------------------------------------------------------------------------------------------
	OVT_BaseData GetNearestBase()
	{
		return m_NearestBase;
	}
	
	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Base Control Condition Module: %1", m_sModuleName));
		Print(string.Format("  Max Distance: %1m", m_fMaxDistance));
		string requireControl = "No";
		if (m_bRequireControl)
			requireControl = "Yes";
		Print(string.Format("  Require Control: %1", requireControl));
		
		if (m_NearestBase)
		{
			Print(string.Format("  Base Faction: %1", m_NearestBase.faction));
		}
	}
}