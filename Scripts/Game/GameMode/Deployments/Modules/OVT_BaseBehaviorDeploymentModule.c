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
	// "IS MY FORCE THERE, AND IS ANYBODY ELSE?" - the two questions a hold-a-place behavior asks
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! How many living members of this deployment's force are standing inside a circle.
	//!
	//! ⚠ IT COUNTS THROUGH THE VIRTUALIZATION RECORD, NEVER THROUGH AGENTS, and that is not an
	//! optimisation. Under Reforger 1.8 a perfectly alive DORMANT or spawn-queued group reports ZERO
	//! agents, so a behavior that counted bodies would decide its own force was dead every time the
	//! last player drove away - the exact mistake OVT_RadioTowerCaptureBehaviorDeploymentModule's
	//! header documents removing from the tower loop. GetAliveMemberCount() is answered off the
	//! survivor mask and GetPosition() off the record, and both are true dormant or spawned.
	//!
	//! WHOLE GROUPS, NOT INDIVIDUALS. The core tracks one position per group, so a group counts its
	//! living members if the GROUP is inside the circle. For a hold radius measured in tens of metres
	//! against a squad that moves as one, that is the same answer.
	//! \param[in] centre Where the circle is.
	//! \param[in] radius Its radius in metres. Non-positive counts nothing.
	//! \return Living members of this deployment's registered force inside the circle.
	protected int CountAliveRegisteredMembersWithin(vector centre, float radius)
	{
		if (radius <= 0)
			return 0;

		if (!m_ParentDeployment)
			return 0;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return 0;

		array<int> handles = new array<int>();

		array<OVT_BaseSpawningDeploymentModule> spawningModules = m_ParentDeployment.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			if (spawningModule)
				spawningModule.CollectRegisteredHandles(handles);
		}

		int alive = 0;
		foreach (int handle : handles)
		{
			if (!virtualization.IsRegistered(handle))
				continue;

			int members = virtualization.GetAliveMemberCount(handle);
			if (members < 1)
				continue;

			if (vector.Distance(virtualization.GetPosition(handle), centre) > radius)
				continue;

			alive = alive + members;
		}

		return alive;
	}

	//------------------------------------------------------------------------------------------------
	//! Distance to the nearest connected player.
	//!
	//! NOBODY CONNECTED ANSWERS float.MAX, matching OVT_BaseConditionDeploymentModule.GetPlayerProximity()
	//! - so "is an enemy standing on this place" reads false on an empty server rather than true, and a
	//! hold timer runs on a dedicated server nobody has joined yet.
	//!
	//! ⚠ PLAYERS ONLY, DELIBERATELY. "Armed resistance is present" could be asked of every AI agent in
	//! the world, and it would cost a sphere query on every behavior module of every deployment on the
	//! map every ten seconds. A player is what the ramp is being read by and interrupted by; a resistance
	//! patrol walking past is not what the design means by contested.
	//! \param[in] position The place to measure from.
	//! \return Metres to the nearest player, or float.MAX when nobody is connected.
	protected float NearestPlayerDistance(vector position)
	{
		float nearest = float.MAX;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return nearest;

		array<int> players = new array<int>();
		playerManager.GetPlayers(players);

		foreach (int playerId : players)
		{
			IEntity player = playerManager.GetPlayerControlledEntity(playerId);
			if (!player)
				continue;

			float distance = vector.Distance(player.GetOrigin(), position);
			if (distance < nearest)
				nearest = distance;
		}

		return nearest;
	}

	//------------------------------------------------------------------------------------------------
	// COLLECTING THE DEPLOYMENT FROM A BEHAVIOR THAT HAS FINISHED ITS JOB
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Asks for this deployment to be taken down, NEXT FRAME.
	//!
	//! ⚠ THE ONE-FRAME DEFERRAL IS THE ENTIRE POINT AND IT IS NOT DEFENSIVE PADDING. DestroyDeployment()
	//! clears m_aActiveModules and then `delete GetOwner()`, and UpdateDeployment() is in the middle of
	//! a `foreach` over a local list of behavior modules when this is called - a list of WEAK references
	//! into the objects that were just thrown away. OVT_ReinforcementBehaviorDeploymentModule gets away
	//! with deleting inline only because every config authors it LAST among the behavior modules, which
	//! is a rule stated in three config headers and enforced by nothing. A behavior that finishes its
	//! mission is not last and cannot be: it has to run before the reinforcement module, or the
	//! reinforcement module rebuys the force in the same pass that decided the mission was over.
	//!
	//! IDEMPOTENT. Called once per module instance; a second call while one is pending is ignored.
	//! \param[in] reason What finished, for the log line. Empty logs nothing.
	protected void RequestDeploymentCollection(string reason)
	{
		if (m_bCollectionRequested)
			return;

		m_bCollectionRequested = true;

		if (reason != "")
			Print(string.Format("[Overthrow] Deployment collection requested: %1", reason), LogLevel.NORMAL);

		GetGame().GetCallqueue().CallLater(CollectParentDeployment, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the deployment down, one frame after the behavior asked. Runs outside any module walk.
	protected void CollectParentDeployment()
	{
		m_bCollectionRequested = false;

		if (!m_ParentDeployment)
			return;

		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
			return;

		manager.DeleteDeployment(m_ParentDeployment);
	}

	//------------------------------------------------------------------------------------------------
	//! ⚠ THE QUEUED COLLECTION MUST BE CANCELLED HERE. A deployment torn down by anything else - the
	//! objective director's reset, the reinforcement module's condition check, a Game Master - runs
	//! Cleanup() on every module, and a call still sitting in the queue would then fire into a module
	//! whose parent has already gone.
	override protected void OnCleanup()
	{
		GetGame().GetCallqueue().Remove(CollectParentDeployment);
		m_bCollectionRequested = false;

		super.OnCleanup();
	}

	//! True while a deferred collection is sitting in the call queue.
	protected bool m_bCollectionRequested;

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