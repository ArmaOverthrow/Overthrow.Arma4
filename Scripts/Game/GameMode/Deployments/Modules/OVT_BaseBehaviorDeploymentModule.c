[BaseContainerProps(configRoot: true)]
class OVT_BaseBehaviorDeploymentModule : OVT_BaseDeploymentModule
{
	//------------------------------------------------------------------------------------------------
	//! The waypoint plan this behavior wants a group registered with, or null for "I have no opinion".
	//!
	//! ASKED BEFORE THE GROUP EXISTS. The virtualization core builds a group's waypoints from its plan
	//! at registration and owns them from then on, so a behavior that wants to shape where a group
	//! goes has to say so up front rather than bolting waypoints on after it spawns. Null is the
	//! honest answer for a behavior that is about something else entirely (reinforcement, capture),
	//! and the spawning module simply asks the next one.
	//! \param[in] groupPosition Where the group is about to be registered. A plan may be built around
	//!            it - a perimeter patrol starts on the group's own bearing to its centre.
	//! \return The plan, or null.
	OVT_VirtualWaypointPlan BuildVirtualPlan(vector groupPosition)
	{
		return null;
	}

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
	// NO WAYPOINT HELPERS LIVE HERE ANY MORE.
	//
	// There used to be four - one that stripped a group's waypoints and three that built new ones - and
	// all four had zero callers by the time the virtualization core took ownership of waypoints. A
	// behavior module says what plan a group should be REGISTERED with (BuildVirtualPlan above); the
	// core turns that into waypoint entities, records them, and deletes them again on unregister. A
	// consumer that creates or removes a waypoint on a registered group corrupts the record the core
	// persists, so the helpers that made that easy to do are gone rather than merely unused.
	//------------------------------------------------------------------------------------------------
	
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