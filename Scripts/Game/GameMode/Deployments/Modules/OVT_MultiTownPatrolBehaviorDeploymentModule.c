//! Multi-town patrol behavior module for deployments
//! Creates patrol waypoints through multiple nearby towns
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_MultiTownPatrolBehaviorDeploymentModule : OVT_BaseBehaviorDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;
	
	[Attribute(defvalue: "3", desc: "Maximum number of towns to patrol")]
	int m_iMaxTowns;
	
	[Attribute(defvalue: "5000", desc: "Maximum radius to search for towns")]
	float m_fSearchRadius;
	
	[Attribute(defvalue: "60", desc: "Time to wait at each town waypoint (seconds)")]
	int m_iWaitTimeAtTown;
	
	[Attribute(defvalue: "true", desc: "If true, returns to start position and completes. If false, loops indefinitely")]
	bool m_bReturnToStart;
	
	[Attribute(defvalue: "true", desc: "If true, deployment self-destructs after completing patrol (only if m_bReturnToStart is true)")]
	bool m_bDeleteOnComplete;
	
	[Attribute(defvalue: "true", desc: "If true, recovers resources when patrol completes successfully")]
	bool m_bRecoverResourcesOnComplete;
	
	[Attribute(defvalue: "0.5", desc: "Fraction of resources to recover (0-1)")]
	float m_fResourceRecoveryFraction;
	
	protected ref array<ref OVT_TownData> m_aPatrolRoute;
	protected ref array<AIWaypoint> m_aWaypoints;
	protected int m_iCurrentWaypointIndex;
	protected vector m_vStartPosition;
	protected bool m_bPatrolActive;
	protected bool m_bReturningToBase;
	protected bool m_bAssignedWaypoints;
	
	//------------------------------------------------------------------------------------------------
	void OVT_MultiTownPatrolBehaviorDeploymentModule()
	{
		m_aPatrolRoute = new array<ref OVT_TownData>;
		m_aWaypoints = new array<AIWaypoint>;
		m_iCurrentWaypointIndex = 0;
		m_bPatrolActive = false;
		m_bReturningToBase = false;
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnActivate()
	{
		super.OnActivate();
		
		if (!m_ParentDeployment)
			return;
			
		m_vStartPosition = m_ParentDeployment.GetPosition();
		
		// Plan the patrol route
		if (PlanPatrolRoute())
		{
			// Waypoints will be created in OnUpdate
			m_bPatrolActive = true;
		}
		else
		{
			Print("Failed to plan patrol route - no suitable towns found", LogLevel.WARNING);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnDeactivate()
	{
		super.OnDeactivate();
		
		// Clean up waypoints
		foreach (AIWaypoint waypoint : m_aWaypoints)
		{
			if (waypoint)
				SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
		}
		
		m_aWaypoints.Clear();
		m_aPatrolRoute.Clear();
		m_bPatrolActive = false;
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnUpdate(int deltaTime)
	{
		super.OnUpdate(deltaTime);
		
		if (!m_bPatrolActive || !m_ParentDeployment)
			return;

		if (!m_bAssignedWaypoints)
		{
			CreatePatrolWaypoints();
			return;
		}
			
		// Check if patrol is complete
		if (CheckPatrolComplete())
		{
			OnPatrolComplete();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_MultiTownPatrolBehaviorDeploymentModule clone = new OVT_MultiTownPatrolBehaviorDeploymentModule();
		
		// Copy configuration
		clone.m_sModuleName = m_sModuleName;
		clone.m_iMaxTowns = m_iMaxTowns;
		clone.m_fSearchRadius = m_fSearchRadius;
		clone.m_iWaitTimeAtTown = m_iWaitTimeAtTown;
		clone.m_bReturnToStart = m_bReturnToStart;
		clone.m_bDeleteOnComplete = m_bDeleteOnComplete;
		clone.m_bRecoverResourcesOnComplete = m_bRecoverResourcesOnComplete;
		clone.m_fResourceRecoveryFraction = m_fResourceRecoveryFraction;
		
		return clone;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool PlanPatrolRoute()
	{
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		if (!townManager)
			return false;
			
		// Get all towns within search radius
		array<ref OVT_TownData> nearbyTowns = new array<ref OVT_TownData>;
		array<ref OVT_TownData> allTowns = townManager.m_Towns;
		
		foreach (OVT_TownData town : allTowns)
		{
			float distance = vector.Distance(m_vStartPosition, town.location);
			if (distance <= m_fSearchRadius && distance > 100) // Don't include the starting town
			{
				nearbyTowns.Insert(town);
			}
		}
		
		if (nearbyTowns.IsEmpty())
			return false;
			
		// Build optimized route using nearest neighbor algorithm
		array<ref OVT_TownData> remainingTowns = new array<ref OVT_TownData>;
		foreach (OVT_TownData town : nearbyTowns)
			remainingTowns.Insert(town);
			
		vector currentPos = m_vStartPosition;
		
		while (!remainingTowns.IsEmpty() && m_aPatrolRoute.Count() < m_iMaxTowns)
		{
			// Find nearest unvisited town
			OVT_TownData nearestTown = null;
			float nearestDistance = float.MAX;
			int nearestIndex = -1;
			
			for (int i = 0; i < remainingTowns.Count(); i++)
			{
				float distance = vector.Distance(currentPos, remainingTowns[i].location);
				if (distance < nearestDistance)
				{
					nearestDistance = distance;
					nearestTown = remainingTowns[i];
					nearestIndex = i;
				}
			}
			
			if (nearestTown)
			{
				m_aPatrolRoute.Insert(nearestTown);
				currentPos = nearestTown.location;
				remainingTowns.Remove(nearestIndex);
			}
		}
		
		Print(string.Format("Planned patrol route with %1 towns", m_aPatrolRoute.Count()), LogLevel.NORMAL);
		foreach (OVT_TownData town : m_aPatrolRoute)
		{
			Print(string.Format("  - %1", OVT_Global.GetTowns().GetNearestTownName(town.location)), LogLevel.NORMAL);
		}
		
		return !m_aPatrolRoute.IsEmpty();
	}
	
	//------------------------------------------------------------------------------------------------
	protected void CreatePatrolWaypoints()
	{
		if (!m_ParentDeployment)
			return;
			
		// Get all vehicle spawning modules
		array<OVT_BaseSpawningDeploymentModule> spawningModules = m_ParentDeployment.GetSpawningModules();
		
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			OVT_VehicleSpawningDeploymentModule vehicleModule = OVT_VehicleSpawningDeploymentModule.Cast(spawningModule);
			if (vehicleModule)
			{
				CreateWaypointsForVehicles(vehicleModule);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void CreateWaypointsForVehicles(OVT_VehicleSpawningDeploymentModule vehicleModule)
	{
		array<Vehicle> vehicles = vehicleModule.GetOperationalVehicles();
		
		int waypointCount = 0;
		foreach (Vehicle vehicle : vehicles)
		{
			// Get the AI group controlling this vehicle
			SCR_AIGroup aiGroup = GetVehicleAIGroup(vehicle);
			if (!aiGroup)
				continue;
				
			// Clear existing waypoints
			array<AIWaypoint> waypoints = {};
			aiGroup.GetWaypoints(waypoints);
			
			foreach(AIWaypoint wp : waypoints)
			{
				aiGroup.RemoveWaypoint(wp);
				SCR_EntityHelper.DeleteEntityAndChildren(wp);
			}
			
			// Create waypoints for each town
			foreach (OVT_TownData town : m_aPatrolRoute)
			{
				AIWaypoint waypoint = CreateTownWaypoint(town, aiGroup);
				if (waypoint)
				{
					aiGroup.AddWaypoint(waypoint);
					m_aWaypoints.Insert(waypoint);
					waypointCount++;
					if(m_iWaitTimeAtTown > 0)
					{
						AIWaypoint wait = OVT_Global.GetConfig().SpawnWaitWaypoint(waypoint.GetOrigin(), m_iWaitTimeAtTown);
						if(wait)
						{
							aiGroup.AddWaypoint(wait);
							m_aWaypoints.Insert(wait);
							waypointCount++;
						}
					}
				}
				
			}
			
			// Add return waypoint if needed
			if (m_bReturnToStart)
			{
				AIWaypoint returnWaypoint = CreateReturnWaypoint(aiGroup);
				if (returnWaypoint)
				{
					aiGroup.AddWaypoint(returnWaypoint);
					m_aWaypoints.Insert(returnWaypoint);
					waypointCount++;
				}
			}
			else
			{
				// Create cycle waypoint to loop
				OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
				if (config)
				{
					AIWaypointCycle cycleWaypoint = AIWaypointCycle.Cast(config.SpawnWaypoint(config.m_pCycleWaypointPrefab, aiGroup.GetOrigin()));
					if (cycleWaypoint)
					{
						cycleWaypoint.SetRerunCounter(-1); // Infinite loops
						aiGroup.AddWaypoint(cycleWaypoint);
						m_aWaypoints.Insert(cycleWaypoint);
						waypointCount++;
					}
				}
			}
		}
		m_bAssignedWaypoints = waypointCount > 0;
	}
	
	//------------------------------------------------------------------------------------------------
	protected SCR_AIGroup GetVehicleAIGroup(Vehicle vehicle)
	{
		if (!vehicle)
			return null;
			
		// Get the driver
		SCR_BaseCompartmentManagerComponent compartmentManager = SCR_BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(SCR_BaseCompartmentManagerComponent)
		);
		
		if (!compartmentManager)
			return null;
			
		array<BaseCompartmentSlot> compartments = {};
		compartmentManager.GetCompartments(compartments);
		
		foreach (BaseCompartmentSlot slot : compartments)
		{
			if (slot.GetType() == ECompartmentType.PILOT)
			{
				IEntity occupant = slot.GetOccupant();
				if (occupant)
				{
					AIControlComponent aiControl = AIControlComponent.Cast(occupant.FindComponent(AIControlComponent));
					if (aiControl)
					{
						AIAgent agent = aiControl.GetAIAgent();
						if (agent)
							return SCR_AIGroup.Cast(agent.GetParentGroup());
					}
				}
			}
		}
		
		return null;
	}
	
	//------------------------------------------------------------------------------------------------
	protected AIWaypoint CreateTownWaypoint(OVT_TownData town, SCR_AIGroup group)
	{
		if (!town || !group)
			return null;
			
		// Find a good position in the town (near center but on a road if possible)
		vector waypointPos = FindTownPatrolPosition(town);
		
		// Create patrol waypoint using the config component
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return null;
			
		AIWaypoint waypoint = config.SpawnPatrolWaypoint(waypointPos);
		if (!waypoint)
			return null;
		
		// Store for cleanup later
		m_aWaypoints.Insert(waypoint);
		
		return waypoint;
	}
	
	//------------------------------------------------------------------------------------------------
	protected AIWaypoint CreateReturnWaypoint(SCR_AIGroup group)
	{
		if (!group)
			return null;
			
		// Create patrol waypoint for return to start
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return null;
		
		vector roadPos = OVT_Global.FindNearestRoad(m_vStartPosition);
			
		AIWaypoint waypoint = config.SpawnPatrolWaypoint(roadPos);
		if (!waypoint)
			return null;
		
		// Store for cleanup later
		m_aWaypoints.Insert(waypoint);
		
		return waypoint;
	}
	
	//------------------------------------------------------------------------------------------------
	protected vector FindTownPatrolPosition(OVT_TownData town)
	{
		if (!town)
			return vector.Zero;
			
		// Try to find a road position near town center
		vector roadPos = OVT_Global.FindNearestRoad(town.location);
		if (vector.Distance(roadPos, town.location) < 200)
			return roadPos;
			
		// Otherwise return town center
		return town.location;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool CheckPatrolComplete()
	{
		if (!m_bAssignedWaypoints)
			return false;

		if (!m_bReturnToStart)
			return false; // Never completes if looping
			
		// Check if all vehicles have completed their patrol
		array<OVT_BaseSpawningDeploymentModule> spawningModules = m_ParentDeployment.GetSpawningModules();
		
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			OVT_VehicleSpawningDeploymentModule vehicleModule = OVT_VehicleSpawningDeploymentModule.Cast(spawningModule);
			if (vehicleModule)
			{
				array<SCR_AIGroup> groups = vehicleModule.GetSpawnedGroups();
				foreach (SCR_AIGroup aiGroup : groups)
				{	
					array<AIWaypoint> waypoints = {};
					int waypointCount = aiGroup.GetWaypoints(waypoints);
					if (waypointCount > 0)
						return false; // Still has waypoints
				}
			}
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void OnPatrolComplete()
	{
		Print(string.Format("Patrol completed for deployment '%1'", m_ParentDeployment.GetDeploymentName()), LogLevel.NORMAL);
		
		m_bPatrolActive = false;
		
		// Check if units were eliminated
		bool wasEliminated = false;
		array<OVT_BaseSpawningDeploymentModule> spawningModules = m_ParentDeployment.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule module : spawningModules)
		{
			if (module.AreSpawnedUnitsEliminated())
			{
				wasEliminated = true;
				break;
			}
		}
		
		// Recover resources if patrol completed successfully (not eliminated)
		if (!wasEliminated && m_bRecoverResourcesOnComplete)
		{
			RecoverResources();
		}
		
		// Request deployment deletion if configured
		if (m_bDeleteOnComplete)
		{
			OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
			if (manager)
			{
				Print(string.Format("Patrol complete - deleting deployment '%1'", m_ParentDeployment.GetDeploymentName()), LogLevel.NORMAL);
				manager.DeleteDeployment(m_ParentDeployment);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void RecoverResources()
	{
		if (!m_ParentDeployment)
			return;
			
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
			return;
			
		// Calculate resources to recover
		int totalResources = m_ParentDeployment.GetResourcesInvested();
		int recoveredResources = (int)(totalResources * m_fResourceRecoveryFraction);
		
		if (recoveredResources > 0)
		{
			int factionIndex = m_ParentDeployment.GetControllingFaction();
			manager.AddFactionResources(factionIndex, recoveredResources);
			
			Print(string.Format("Recovered %1 resources from completed patrol (%.0f%% of %2)", 
				recoveredResources, m_fResourceRecoveryFraction * 100, totalResources), LogLevel.NORMAL);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Debug methods
	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Multi-Town Patrol Module: %1", m_sModuleName));
		Print(string.Format("  Max Towns: %1", m_iMaxTowns));
		Print(string.Format("  Search Radius: %1m", m_fSearchRadius));
		string patrolActive = "No";
		if (m_bPatrolActive)
			patrolActive = "Yes";
		Print(string.Format("  Patrol Active: %1", patrolActive));
		
		if (!m_aPatrolRoute.IsEmpty())
		{
			Print(string.Format("  Route (%1 towns):", m_aPatrolRoute.Count()));
			foreach (OVT_TownData town : m_aPatrolRoute)
			{
				Print(string.Format("    - %1", OVT_Global.GetTowns().GetNearestTownName(town.location)));
			}
		}
		
		string returnToStart = "No";
		if (m_bReturnToStart)
			returnToStart = "Yes";
		Print(string.Format("  Return to Start: %1", returnToStart));
		
		string deleteOnComplete = "No";
		if (m_bDeleteOnComplete)
			deleteOnComplete = "Yes";
		Print(string.Format("  Delete on Complete: %1", deleteOnComplete));
	}
}