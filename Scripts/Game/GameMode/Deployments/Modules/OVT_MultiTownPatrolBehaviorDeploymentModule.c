//------------------------------------------------------------------------------------------------
//! Multi-town patrol behaviour for deployments: supplies the ROUTE PLAN its deployment's vehicle
//! crews are registered with, and decides when that route is finished.
//!
//! THE ORDER INVERTED HERE, exactly as it did for the town patrol. This module used to wait for the
//! vehicle module to put crews in the world, then hunt down each vehicle's driver, strip whatever
//! waypoints its group had and bolt a fresh chain on. The virtualization core now builds a group's
//! waypoints from a plan AT REGISTRATION and owns them for the group's whole life - a consumer that
//! creates or deletes a waypoint on a registered group corrupts the record core persists. So the
//! question moved from "what waypoints does this vehicle's group need now?" to "what plan should a
//! crew be registered with?", and it has to be answerable BEFORE any crew exists.
//!
//! ROUTE PLANNING STAYED, WAYPOINT AUTHORING WENT. The nearest-neighbour tour of nearby towns below
//! is the same algorithm it always was; what disappeared is the five methods that turned it into
//! AIWaypoint entities, the array that tracked them and the teardown that deleted each of them
//! twice.
//!
//! ⚠ THE ROUTE IS PLANNED LAZILY, NOT IN OnActivate. A deployment activates its SPAWNING modules
//! before its behaviour modules, and the spawning module asks for a plan while it registers - which
//! is before this module's OnActivate has ever run. Planning on first ask is what makes the answer
//! non-empty; OnActivate merely arms the completion check.
//------------------------------------------------------------------------------------------------
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

	//! How close a crew has to get to the route's final point to count as home, in metres. Generous
	//! on purpose: the final point is road-snapped and a vehicle parks where the driver stops it,
	//! not on the waypoint.
	static const float ROUTE_COMPLETE_DISTANCE = 100;

	//! Ceiling on how far a crew has to get from that final point before coming back to it can mean
	//! anything, in metres. WITHOUT THIS LATCH THE PATROL COMPLETES ON ITS FIRST TICK: the crew is
	//! registered beside its vehicle at the base it left from, which is inside the completion radius,
	//! so "everyone is home" is true before anybody has gone anywhere.
	static const float ROUTE_DEPARTURE_DISTANCE = 300;

	//! Floor on the same threshold, in metres. A route whose only town is a few hundred metres away
	//! would never take a crew 300 m from home, and a departure latch that can never trip is a
	//! deployment that can never finish, never refund and never delete itself. Comfortably larger than
	//! ROUTE_COMPLETE_DISTANCE so leaving and arriving can never both be true of one position.
	static const float ROUTE_MIN_DEPARTURE_DISTANCE = 150;

	//! What fraction of the route's own reach counts as having left. Half, so a crew that got to its
	//! farthest town has unambiguously departed whatever the scale of the route.
	static const float ROUTE_DEPARTURE_FRACTION = 0.5;

	protected ref array<ref OVT_TownData> m_aPatrolRoute;
	protected vector m_vStartPosition;

	//! Where the route's LAST plan point ended up, after the road snap. This is what completion is
	//! measured against; the raw start position is only the fallback for a route never built.
	protected vector m_vRouteEndPosition;

	protected bool m_bPatrolActive;

	//! Set once any crew has been farther than ROUTE_DEPARTURE_DISTANCE from the route's end. Latches
	//! true and stays there - a patrol does not un-depart.
	protected bool m_bPatrolDeparted;

	//------------------------------------------------------------------------------------------------
	void OVT_MultiTownPatrolBehaviorDeploymentModule()
	{
		m_aPatrolRoute = new array<ref OVT_TownData>;
		m_bPatrolActive = false;
		m_bPatrolDeparted = false;
	}

	//------------------------------------------------------------------------------------------------
	override void OnActivate()
	{
		super.OnActivate();

		if (!m_ParentDeployment)
			return;

		// Normally a no-op by the time this runs: the spawning module asked for a plan while it was
		// registering its crews, which planned the route already. It stays here so a deployment whose
		// spawning module registered nothing still knows where its route was meant to go.
		if (!EnsureRoutePlanned(m_ParentDeployment.GetPosition()))
		{
			Print("Failed to plan patrol route - no suitable towns found", LogLevel.WARNING);
			return;
		}

		m_bPatrolActive = true;
	}

	//------------------------------------------------------------------------------------------------
	//! DELIBERATELY A NO-OP. This used to delete every waypoint entity the module had authored - and,
	//! because each of them had been inserted into the tracking array twice, to delete each of them
	//! twice. Waypoints belong to the virtualization core now: it builds them at registration and
	//! deletes them again when the group is unregistered or wiped, so there is nothing here to tear
	//! down and touching a registered group's waypoints would corrupt the record core persists.
	override void OnDeactivate()
	{
		super.OnDeactivate();
	}

	//------------------------------------------------------------------------------------------------
	override void OnUpdate(int deltaTime)
	{
		super.OnUpdate(deltaTime);

		if (!m_bPatrolActive || !m_ParentDeployment)
			return;

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

		// Copy configuration. EVERY attribute has to appear here - a forgotten one silently ships the
		// class default instead of the authored value (D1's standing trap).
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
	// Virtualization
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The plan a crew registered at groupPosition should carry: drive the tour, pause in each town,
	//! and - as both shipped vehicle configs author it - come home at the end.
	//!
	//! ANSWERED BEFORE ANY CREW EXISTS, which is why the route is planned here rather than in
	//! OnActivate (see the class header). Every crew of one deployment gets the SAME route: the
	//! nearest-neighbour tour is deterministic, so asking twice returns the same stops.
	//!
	//! ROAD SNAPPING IS APPLIED HERE, not in the factory - the factory is world-free geometry and
	//! cannot ask where the roads are. Town stops go through the same "road if it is close enough,
	//! town centre otherwise" rule the hand-authored version used, and the closing point is snapped
	//! onto the nearest road to the deployment exactly as the old return waypoint was.
	//!
	//! ⚠ Cycle = !m_bReturnToStart. The two are mutually exclusive by construction in the factory: a
	//! route that comes home has a natural end, one that does not loops so it keeps running. Both
	//! shipped vehicle configs leave m_bReturnToStart at its default true, so neither produces a
	//! cycling plan.
	//! \param[in] groupPosition Where the crew is about to be registered; the fallback route origin
	//!            when there is no deployment to take one from.
	//! \return The plan, or null when no route could be planned.
	override OVT_VirtualWaypointPlan BuildVirtualPlan(vector groupPosition)
	{
		if (!EnsureRoutePlanned(groupPosition))
			return null;

		array<vector> stops = new array<vector>();
		foreach (OVT_TownData town : m_aPatrolRoute)
		{
			stops.Insert(FindTownPatrolPosition(town));
		}

		if (stops.IsEmpty())
			return null;

		// The closing point and the point completion is measured against are THE SAME VALUE, resolved
		// once by EnsureRoutePlanned - see the note there.
		return OVT_VirtualPlanFactory.BuildRoutePlan(stops, m_iWaitTimeAtTown, m_bReturnToStart, m_vRouteEndPosition);
	}

	//------------------------------------------------------------------------------------------------
	//! Plans the tour if it has not been planned yet. Idempotent, and safe to call from anywhere.
	//!
	//! ⚠ THE ROUTE'S END POINT IS RESOLVED HERE, NOT IN BuildVirtualPlan, and that is load-bearing.
	//! After a load the crews are RECLAIMED rather than re-registered, so no plan is ever built that
	//! session and nothing would have set the end point - completion would then be measured against
	//! the raw deployment position while the crews were driving home to the ROAD beside it, which the
	//! shared snap can put up to 500 m away. Resolving it on the same call OnActivate makes keeps the
	//! plan's closing point and the completion target the same value on every path.
	//! \param[in] fallbackStart Route origin to use when there is no deployment position to take one
	//!            from - the position of whatever is asking.
	//! \return True when there is a route to walk.
	protected bool EnsureRoutePlanned(vector fallbackStart)
	{
		if (!m_aPatrolRoute.IsEmpty())
			return true;

		if (m_vStartPosition == vector.Zero)
		{
			if (m_ParentDeployment)
				m_vStartPosition = m_ParentDeployment.GetPosition();

			if (m_vStartPosition == vector.Zero)
				m_vStartPosition = fallbackStart;
		}

		m_vRouteEndPosition = OVT_WorldUtils.FindNearestRoad(m_vStartPosition);

		return PlanPatrolRoute();
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
	protected vector FindTownPatrolPosition(OVT_TownData town)
	{
		if (!town)
			return vector.Zero;

		// Try to find a road position near town center
		vector roadPos = OVT_WorldUtils.FindNearestRoad(town.location);
		if (vector.Distance(roadPos, town.location) < 200)
			return roadPos;

		// Otherwise return town center
		return town.location;
	}

	//------------------------------------------------------------------------------------------------
	//! Has the tour finished?
	//!
	//! MEASURED, NOT COUNTED. The old test asked each crew's group how many waypoints it had left and
	//! called the patrol over when the answer reached zero. Waypoints belong to the core now and a
	//! consumer must not touch them, so completion is read off POSITION instead: every crew is within
	//! ROUTE_COMPLETE_DISTANCE of the route's final plan point, and at least one of them has been
	//! GetDepartureDistance() away from it at some point - the second half is what stops a patrol
	//! completing on its first tick while it is still parked where it started.
	//!
	//! NO CREWS LEFT is the one case position cannot answer, and it is the same case the old check
	//! answered by finding an empty group list: it means either the crews were killed - the patrol is
	//! over, and OnPatrolComplete withholds the refund because the module is flagged eliminated - or
	//! none has been registered yet, which is not a completion at all. The eliminated flag is the only
	//! thing that can tell the two apart.
	//! \return True when the deployment should be wound up.
	protected bool CheckPatrolComplete()
	{
		if (!m_bReturnToStart)
			return false; // Never completes if looping

		if (!m_ParentDeployment)
			return false;

		array<vector> crewPositions = CollectCrewPositions();

		if (crewPositions.IsEmpty())
			return AnySpawningModuleEliminated();

		vector home = GetRouteEndPosition();

		float farthest = 0;
		foreach (vector crewPosition : crewPositions)
		{
			float distance = vector.Distance(crewPosition, home);
			if (distance > farthest)
				farthest = distance;
		}

		if (farthest >= GetDepartureDistance())
			m_bPatrolDeparted = true;

		if (!m_bPatrolDeparted)
			return false;

		return farthest <= ROUTE_COMPLETE_DISTANCE;
	}

	//------------------------------------------------------------------------------------------------
	//! How far from home counts as having left, for THIS route.
	//!
	//! Scaled to the route rather than fixed, because the two failure modes sit on opposite sides of a
	//! constant: a threshold below the completion radius would let one position be both a departure and
	//! an arrival and the patrol would finish on its first tick, while a threshold beyond anywhere the
	//! route actually goes would never trip and the deployment would never finish at all. The shipped
	//! configs search 2500 m and are always capped at the ceiling; the floor is what covers a route
	//! whose only town happens to be just outside the 100 m exclusion.
	//! \return The departure threshold in metres, between the floor and the ceiling.
	protected float GetDepartureDistance()
	{
		float reach = 0;
		foreach (OVT_TownData town : m_aPatrolRoute)
		{
			float distance = vector.Distance(GetRouteEndPosition(), town.location);
			if (distance > reach)
				reach = distance;
		}

		float threshold = reach * ROUTE_DEPARTURE_FRACTION;

		if (threshold > ROUTE_DEPARTURE_DISTANCE)
			return ROUTE_DEPARTURE_DISTANCE;

		if (threshold < ROUTE_MIN_DEPARTURE_DISTANCE)
			return ROUTE_MIN_DEPARTURE_DISTANCE;

		return threshold;
	}

	//------------------------------------------------------------------------------------------------
	//! Where every crew of this deployment is right now.
	//!
	//! Asked through the virtualization core rather than off a group entity, because a crew's position
	//! is valid whether or not its members exist in the world at this instant - the record answers
	//! either way. Handles that are no longer registered are skipped rather than treated as being at
	//! the origin.
	//! \return One position per live crew; empty when there are none.
	protected array<vector> CollectCrewPositions()
	{
		array<vector> positions = new array<vector>();

		if (!m_ParentDeployment)
			return positions;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return positions;

		array<OVT_BaseSpawningDeploymentModule> spawningModules = m_ParentDeployment.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			OVT_VehicleSpawningDeploymentModule vehicleModule = OVT_VehicleSpawningDeploymentModule.Cast(spawningModule);
			if (!vehicleModule)
				continue;

			array<int> handles = vehicleModule.GetCrewHandles();
			foreach (int handle : handles)
			{
				if (!virtualization.IsRegistered(handle))
					continue;

				positions.Insert(virtualization.GetPosition(handle));
			}
		}

		return positions;
	}

	//------------------------------------------------------------------------------------------------
	//! The point the route ends at, after the road snap that built it.
	//! \return The final plan point, or the raw start position when no plan was ever built.
	protected vector GetRouteEndPosition()
	{
		if (m_vRouteEndPosition != vector.Zero)
			return m_vRouteEndPosition;

		return m_vStartPosition;
	}

	//------------------------------------------------------------------------------------------------
	//! Has anything this deployment put in the world been wiped out?
	//! \return True when at least one spawning module is flagged eliminated.
	protected bool AnySpawningModuleEliminated()
	{
		if (!m_ParentDeployment)
			return false;

		array<OVT_BaseSpawningDeploymentModule> spawningModules = m_ParentDeployment.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			if (spawningModule && spawningModule.AreSpawnedUnitsEliminated())
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! ⚠ THIS DELETES ITS OWN DEPLOYMENT FROM INSIDE ITS OWN UPDATE FRAME. That is the shape it has
	//! always had and it is left exactly as it was: DeleteDeployment tears the deployment down, which
	//! cleans up the very modules this call is running inside. m_bPatrolActive is cleared FIRST so a
	//! second pass can never re-enter it.
	protected void OnPatrolComplete()
	{
		Print(string.Format("Patrol completed for deployment '%1'", m_ParentDeployment.GetDeploymentName()), LogLevel.NORMAL);

		m_bPatrolActive = false;

		// Check if units were eliminated
		bool wasEliminated = AnySpawningModuleEliminated();

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
		string patrolDeparted = "No";
		if (m_bPatrolDeparted)
			patrolDeparted = "Yes";
		Print(string.Format("  Departed: %1", patrolDeparted));

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
		Print(string.Format("  Route End: %1", GetRouteEndPosition().ToString()));

		string deleteOnComplete = "No";
		if (m_bDeleteOnComplete)
			deleteOnComplete = "Yes";
		Print(string.Format("  Delete on Complete: %1", deleteOnComplete));
	}
}
