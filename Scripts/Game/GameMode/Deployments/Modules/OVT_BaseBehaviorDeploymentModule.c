[BaseContainerProps(configRoot: true)]
class OVT_BaseBehaviorDeploymentModule : OVT_BaseDeploymentModule
{
	//------------------------------------------------------------------------------------------------
	// Common behavior functionality
	//------------------------------------------------------------------------------------------------
	protected array<SCR_AIGroup> GetManagedGroups()
	{
		array<SCR_AIGroup> groups = new array<SCR_AIGroup>;
		
		if (!m_ParentDeployment)
			return groups;
			
		// Get all spawning modules and extract their AI groups
		array<OVT_BaseSpawningDeploymentModule> spawningModules = m_ParentDeployment.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			array<IEntity> entities = spawningModule.GetSpawnedEntities();
			foreach (IEntity entity : entities)
			{
				// Check if entity is an AI group or has AI group component
				SCR_AIGroup group = SCR_AIGroup.Cast(entity);
				if (group)
				{
					groups.Insert(group);
					continue;
				}
				
				// Check if entity is part of an AI group
				AIAgent agent = AIAgent.Cast(entity.FindComponent(AIAgent));
				if (agent && agent.GetParentGroup())
				{
					SCR_AIGroup parentGroup = SCR_AIGroup.Cast(agent.GetParentGroup());
					if (parentGroup && !groups.Contains(parentGroup))
						groups.Insert(parentGroup);
				}
			}
		}
		
		return groups;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void ApplyBehaviorToGroup(SCR_AIGroup group)
	{
		// Override in derived classes to implement specific behavior
	}
	
	//------------------------------------------------------------------------------------------------
	protected void RemoveBehaviorFromGroup(SCR_AIGroup group)
	{
		// Override in derived classes to clean up behavior
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnActivate()
	{
		super.OnActivate();
		
		// Apply behavior to all current groups
		array<SCR_AIGroup> groups = GetManagedGroups();
		foreach (SCR_AIGroup group : groups)
		{
			ApplyBehaviorToGroup(group);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnDeactivate()
	{
		super.OnDeactivate();
		
		// Remove behavior from all groups
		array<SCR_AIGroup> groups = GetManagedGroups();
		foreach (SCR_AIGroup group : groups)
		{
			RemoveBehaviorFromGroup(group);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnUpdate(int deltaTime)
	{
		super.OnUpdate(deltaTime);
		
		// Reapply behavior to any new groups that may have spawned
		array<SCR_AIGroup> groups = GetManagedGroups();
		foreach (SCR_AIGroup group : groups)
		{
			if (!HasBehaviorApplied(group))
				ApplyBehaviorToGroup(group);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Utility methods for AI group management
	//------------------------------------------------------------------------------------------------
	protected bool HasBehaviorApplied(SCR_AIGroup group)
	{
		// Override in derived classes to track which groups have behavior applied
		// Default implementation assumes behavior needs to be reapplied
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void ClearGroupWaypoints(SCR_AIGroup group)
	{
		if (!group)
			return;
			
		// Remove all existing waypoints
		array<AIWaypoint> waypoints = new array<AIWaypoint>;
		group.GetWaypoints(waypoints);
		
		foreach (AIWaypoint waypoint : waypoints)
		{
			group.RemoveWaypoint(waypoint);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected AIWaypoint CreateMoveWaypoint(vector position)
	{
		AIWaypoint waypoint = OVT_Global.GetConfig().SpawnPatrolWaypoint(position);
		return waypoint;
	}
	
	//------------------------------------------------------------------------------------------------
	protected AIWaypoint CreateDefendWaypoint(vector position)
	{
		AIWaypoint waypoint = OVT_Global.GetConfig().SpawnDefendWaypoint(position);
		return waypoint;
	}
	
	//------------------------------------------------------------------------------------------------
	protected AIWaypoint CreatePatrolWaypoint(vector position)
	{
		// Create a move waypoint for patrol purposes
		return CreateMoveWaypoint(position);
	}
	
	//------------------------------------------------------------------------------------------------
	// Utility methods for position finding
	//------------------------------------------------------------------------------------------------
	protected vector GetRandomPositionInRadius(vector center, float radius)
	{
		float angle = Math.RandomFloat01() * Math.PI2;
		float distance = Math.RandomFloat(0, radius);
		
		vector offset = Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);
		return center + offset;
	}
	
	//------------------------------------------------------------------------------------------------
	protected vector GetPositionOnCircle(vector center, float radius, float angle)
	{
		vector offset = Vector(Math.Cos(angle) * radius, 0, Math.Sin(angle) * radius);
		return center + offset;
	}
	
	//------------------------------------------------------------------------------------------------
	protected array<vector> GeneratePerimeterPositions(vector center, float radius, int count)
	{
		array<vector> positions = new array<vector>;
		
		for (int i = 0; i < count; i++)
		{
			float angle = (i / (float)count) * Math.PI2;
			vector position = GetPositionOnCircle(center, radius, angle);
			positions.Insert(position);
		}
		
		return positions;
	}
	
	//------------------------------------------------------------------------------------------------
	protected vector FindNearestDefendPosition(vector fromPosition)
	{
		if (!m_ParentDeployment)
			return fromPosition;
			
		// Look for nearby defend slots (like in the base upgrade system)
		float nearestDistance = float.MAX;
		vector nearestPosition = fromPosition;
		
		// Query for nearby sentinel components (guard positions)
		array<IEntity> entities = new array<IEntity>;
		GetGame().GetWorld().QueryEntitiesBySphere(fromPosition, 200, null, FilterDefendEntities, EQueryEntitiesFlags.ALL);
		
		return nearestPosition;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool FilterDefendEntities(IEntity entity)
	{
		// Look for entities with sentinel components (guard positions)
		SCR_AISmartActionSentinelComponent sentinel = SCR_AISmartActionSentinelComponent.Cast(entity.FindComponent(SCR_AISmartActionSentinelComponent));
		return sentinel != null;
	}
}