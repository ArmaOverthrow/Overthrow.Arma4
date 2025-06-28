//! Patrol behavior module for deployments
//! Assigns patrol waypoints to spawned AI groups
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_PatrolBehaviorDeploymentModule : OVT_BaseBehaviorDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;
	
	[Attribute(defvalue: "1", UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(OVT_PatrolType), desc: "Type of patrol behavior")]
	OVT_PatrolType m_ePatrolType;
	
	[Attribute(defvalue: "200", desc: "Patrol radius for perimeter patrols")]
	float m_fPatrolRadius;
	
	[Attribute(defvalue: "60", desc: "Check interval for applying patrol behavior to new groups (seconds)")]
	float m_fCheckInterval;
	
	[Attribute(defvalue: "true", desc: "Apply patrol behavior to groups spawned after activation")]
	bool m_bApplyToNewGroups;
	
	[Attribute(defvalue: "false", desc: "Use town center as patrol center instead of deployment position")]
	bool m_bUseNearestTownCenter;
	
	protected float m_fLastCheckTime;
	protected ref array<SCR_AIGroup> m_aProcessedGroups;
	
	//------------------------------------------------------------------------------------------------
	void OVT_PatrolBehaviorDeploymentModule()
	{
		m_fLastCheckTime = 0;
		m_aProcessedGroups = new array<SCR_AIGroup>;
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnActivate()
	{
		super.OnActivate();
		m_fLastCheckTime = GetGame().GetWorld().GetWorldTime();
		
		// Apply patrol behavior to existing groups
		ApplyPatrolBehaviorToExistingGroups();
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnDeactivate()
	{
		super.OnDeactivate();
		m_aProcessedGroups.Clear();
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnUpdate(int deltaTime)
	{
		super.OnUpdate(deltaTime);
		
		if (!m_bApplyToNewGroups)
			return;
		
		float currentTime = GetGame().GetWorld().GetWorldTime();
		
		// Check for new groups periodically
		if (currentTime - m_fLastCheckTime >= m_fCheckInterval)
		{
			m_fLastCheckTime = currentTime;
			CheckForNewGroups();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_PatrolBehaviorDeploymentModule clone = new OVT_PatrolBehaviorDeploymentModule();
		
		// Copy configuration
		clone.m_sModuleName = m_sModuleName;
		clone.m_ePatrolType = m_ePatrolType;
		clone.m_fPatrolRadius = m_fPatrolRadius;
		clone.m_fCheckInterval = m_fCheckInterval;
		clone.m_bApplyToNewGroups = m_bApplyToNewGroups;
		clone.m_bUseNearestTownCenter = m_bUseNearestTownCenter;
		
		return clone;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void ApplyPatrolBehaviorToExistingGroups()
	{
		if (!m_ParentDeployment)
			return;
		
		// Get all spawning modules and their spawned entities
		array<OVT_BaseSpawningDeploymentModule> spawningModules = m_ParentDeployment.GetSpawningModules();
				
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			array<IEntity> spawnedEntities = spawningModule.GetSpawnedEntities();
			foreach (IEntity entity : spawnedEntities)
			{
				SCR_AIGroup group = SCR_AIGroup.Cast(entity);
				if (group && !IsGroupProcessed(group))
				{
					ApplyPatrolBehaviorToGroup(group);
					m_aProcessedGroups.Insert(group);
				}
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void CheckForNewGroups()
	{
		if (!m_ParentDeployment)
			return;
		
		// Get all spawning modules and check for new groups
		array<OVT_BaseSpawningDeploymentModule> spawningModules = m_ParentDeployment.GetSpawningModules();

		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			array<IEntity> spawnedEntities = spawningModule.GetSpawnedEntities();
			foreach (IEntity entity : spawnedEntities)
			{
				SCR_AIGroup group = SCR_AIGroup.Cast(entity);
				if (group && !IsGroupProcessed(group))
				{
					ApplyPatrolBehaviorToGroup(group);
					m_aProcessedGroups.Insert(group);
					Print(string.Format("Applied patrol behavior to new group at %1", group.GetOrigin().ToString()), LogLevel.VERBOSE);
				}
			}
		}
		
		// Clean up destroyed groups from processed list
		for (int i = m_aProcessedGroups.Count() - 1; i >= 0; i--)
		{
			SCR_AIGroup group = m_aProcessedGroups[i];
			if (!group)
			{
				m_aProcessedGroups.Remove(i);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void ApplyPatrolBehaviorToGroup(SCR_AIGroup group)
	{
		if (!group || !m_ParentDeployment)
			return;
		
		vector patrolCenter = GetPatrolCenter();
		
		// Clear any existing waypoints first
		array<AIWaypoint> waypoints = {};
		group.GetWaypoints(waypoints);
		
		foreach(AIWaypoint wp : waypoints)
		{
			group.RemoveWaypoint(wp);
		}
		
		// Get the config component for waypoint creation
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			Print("Patrol behavior: Cannot get config component", LogLevel.ERROR);
			return;
		}
		
		// Apply patrol behavior based on type
		config.GivePatrolWaypoints(group, m_ePatrolType, patrolCenter, m_fPatrolRadius);
		
		string patrolTypeName = typename.EnumToString(OVT_PatrolType, m_ePatrolType);
		string groupPos = group.GetOrigin().ToString();
		string centerPos = patrolCenter.ToString();
		Print(string.Format("Applied %1 patrol to group at %2, center: %3", patrolTypeName, groupPos, centerPos), LogLevel.VERBOSE);
	}
	
	//------------------------------------------------------------------------------------------------
	protected vector GetPatrolCenter()
	{
		if (!m_ParentDeployment)
			return vector.Zero;
		
		if (m_bUseNearestTownCenter)
		{
			// Try to get town center from town conditional module
			OVT_TownConditionalDeploymentModule townCondition = OVT_TownConditionalDeploymentModule.Cast(
				m_ParentDeployment.GetModule(OVT_TownConditionalDeploymentModule)
			);
			
			if (townCondition)
			{
				OVT_TownData nearestTown = townCondition.GetNearestTown();
				if (nearestTown)
				{
					return nearestTown.location;
				}
			}
			
			// Fallback to deployment position if no town found
			Print("Patrol behavior: No town found, using deployment position as patrol center", LogLevel.VERBOSE);
		}
		
		return m_ParentDeployment.GetPosition();
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool IsGroupProcessed(SCR_AIGroup group)
	{
		return m_aProcessedGroups.Contains(group);
	}
	
	//------------------------------------------------------------------------------------------------
	string GetPatrolTypeString()
	{
		return typename.EnumToString(OVT_PatrolType, m_ePatrolType);
	}
	
	//------------------------------------------------------------------------------------------------
	// Debug methods
	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Patrol Behavior Module: %1", m_sModuleName));
		Print(string.Format("  Patrol Type: %1", GetPatrolTypeString()));
		Print(string.Format("  Patrol Radius: %1m", m_fPatrolRadius));
		string useTownCenter = "No";
		if (m_bUseNearestTownCenter)
			useTownCenter = "Yes";
		Print(string.Format("  Use Town Center: %1", useTownCenter));
		
		string applyToNew = "No";
		if (m_bApplyToNewGroups)
			applyToNew = "Yes";
		Print(string.Format("  Apply to New Groups: %1", applyToNew));
		Print(string.Format("  Processed Groups: %1", m_aProcessedGroups.Count()));
		
		vector patrolCenter = GetPatrolCenter();
		Print(string.Format("  Patrol Center: %1", patrolCenter.ToString()));
		
		// List processed groups
		foreach (SCR_AIGroup group : m_aProcessedGroups)
		{
			if (group)
			{
				Print(string.Format("    Group at: %1", group.GetOrigin().ToString()));
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void ForceReapplyPatrolBehavior()
	{
		m_aProcessedGroups.Clear();
		ApplyPatrolBehaviorToExistingGroups();
		Print("Patrol behavior: Force reapplied to all groups", LogLevel.NORMAL);
	}
}