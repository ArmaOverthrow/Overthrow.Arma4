//------------------------------------------------------------------------------------------------
//! Patrol behavior for deployments: supplies the waypoint PLAN its groups are registered with.
//!
//! THE ORDER INVERTED HERE. This module used to spawn nothing and bolt waypoints onto whatever groups
//! it found afterwards, on a 60 s poll, remembering which ones it had already processed. The
//! virtualization core now builds a group's waypoints from a plan AT REGISTRATION and owns them for
//! the group's whole life - a consumer that creates or deletes a waypoint on a registered group
//! corrupts the record core persists. So the question this module answers moved from "what waypoints
//! does this group need now?" to "what plan should this group be registered with?", and it has to
//! answer BEFORE the group exists.
//!
//! Everything the old shape needed is therefore gone: the processed-group list, the poll, the
//! reapply-on-new-groups path and the waypoint authoring itself. What is left is one pure function of
//! the module's authored attributes and the position the group is about to be registered at.
//!
//! THE PLAN IS THE OPT-IN FOR BEING WALKED WHILE DORMANT. A DEFEND plan has nothing movable in it, so
//! a group holding one holds its post whatever the movement tick does around it; a PERIMETER plan
//! cycles, so a dormant patrol keeps walking its corners with nobody watching. That difference is the
//! whole reason this class serves both a town patrol and (from Phase 4) a tower garrison without a
//! flag anywhere.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_PatrolBehaviorDeploymentModule : OVT_BaseBehaviorDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;

	[Attribute(defvalue: "1", UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(OVT_PatrolType), desc: "Type of patrol behavior. DEFEND holds one post and is never walked; PERIMETER circles the centre and IS walked while dormant")]
	OVT_PatrolType m_ePatrolType;

	[Attribute(defvalue: "200", desc: "Patrol radius for perimeter patrols")]
	float m_fPatrolRadius;

	//! RETAINED BUT INERT. There is no poll left to interval: waypoints are built once, at
	//! registration, by the virtualization core. Kept declared so an authored config that still sets it
	//! parses cleanly (the shipped config surface is frozen, D1).
	[Attribute(defvalue: "60", desc: "NO LONGER USED - patrol waypoints are built once at group registration, not applied on a poll")]
	float m_fCheckInterval;

	//! RETAINED BUT INERT, for the same reason. Every group this deployment registers gets its plan at
	//! registration, so "new groups" are covered by construction and there is nothing to opt out of.
	[Attribute(defvalue: "true", desc: "NO LONGER USED - every registered group gets its plan at registration")]
	bool m_bApplyToNewGroups;

	[Attribute(defvalue: "false", desc: "Use town center as patrol center instead of deployment position")]
	bool m_bUseNearestTownCenter;

	//! Shortest pause rolled for a perimeter corner, in seconds. The band is inherited verbatim from the
	//! hand-authored perimeter helper on OVT_OverthrowConfigComponent that this replaces.
	static const float WAIT_SECONDS_MIN = 45;

	//! Longest pause rolled for a perimeter corner, in seconds.
	static const float WAIT_SECONDS_MAX = 75;

	//------------------------------------------------------------------------------------------------
	//! The plan a group registered at groupPosition should carry.
	//!
	//! ROLLED PER GROUP, which is why the wait durations and the starting bearing are not shared: two
	//! patrols around one town centre set off in different directions and pause for different lengths,
	//! exactly as the four hand-authored waypoint sets did.
	//!
	//! ROAD SNAPPING IS APPLIED HERE, not in the factory - the factory is world-free geometry and
	//! cannot ask where the roads are. Each PATROL corner is pulled onto the nearest road and the WAIT
	//! that follows it copies the snapped position, so a patrol pauses where it actually arrived
	//! instead of at the point the geometry asked for.
	//!
	//! ⚠ THE TWO TYPES ANCHOR ON DIFFERENT THINGS, DELIBERATELY. PERIMETER circles GetPatrolCenter()
	//! (the deployment marker, or the town centre when m_bUseNearestTownCenter is set) because circling
	//! the PLACE is what a patrol is for. DEFEND holds groupPosition, because a garrison holds where it
	//! was stationed - see the branch's own comment for what anchoring it on the marker instead does to
	//! every config that places its groups away from that marker.
	//! \param[in] groupPosition Where the group is about to be registered.
	//! \return The plan, or null when this module's patrol type has nothing to say.
	override OVT_VirtualWaypointPlan BuildVirtualPlan(vector groupPosition)
	{
		// Resolved before the type split so the call order is unchanged for every type; only PERIMETER
		// reads it.
		vector centre = GetPatrolCenter();

		// The same fallback the hand-authored helper carried: an unset centre means "circle where you
		// are". It is also what makes this method answerable off a config template with no deployment
		// behind it, which is how the Init tier asserts the shipped Town Patrol's plan shape.
		if (centre == vector.Zero)
			centre = groupPosition;

		if (m_ePatrolType == OVT_PatrolType.DEFEND)
		{
			// ⚠ A DEFEND WAYPOINT TELLS LIVE AI TO GO TO THAT POINT AND HOLD IT, so where it is anchored
			// decides where a garrison ends up standing - and the right anchor is NOT the same for every
			// config. GetPatrolCenter() answers the DEPLOYMENT MARKER whenever there is a live deployment
			// (only a config TEMPLATE gets the groupPosition fallback above), so anchoring there
			// unconditionally walks every deliberately-placed garrison off its post towards the marker:
			// the placed module stands men on tower walkways, sniper markers and defend positions up to
			// baseRange away, and the composition module stands them on a bunker or a road checkpoint.
			// Every legacy caller anchored on the group instead - SpawnDefendWaypoint(aigroup.GetOrigin())
			// in the slotted upgrade, and the checkpoint's own structure origin in the checkpoint upgrade.
			//
			// ⚠ BUT THE GROUP POSITION IS ONLY TRUSTWORTHY WHEN THE SPAWNING MODULE CHOSE IT. The plain
			// infantry module rolls a ring point and road-snaps it through a 500 m search that ignores
			// m_fSpawnRadius, and integration MEASURED that putting a tower garrison on its access road.
			// Anchoring on that would park the garrison on the road instead of at the tower - the mirror
			// image of the bug above, on a shipped config. So the anchor follows
			// StationsGroupsDeliberately(): the group when its position is a chosen post, the marker when
			// it is a rolled-and-snapped artefact. Deployment_TowerGarrison.conf therefore keeps exactly
			// today's behaviour.
			//
			// PERIMETER keeps GetPatrolCenter()'s answer unconditionally, and must: circling the TOWN is
			// the entire point of a town patrol, and it is the one type where the centre is deliberately
			// not where the group stands.
			//
			// Radius 0 leaves the defend waypoint prefab's own completion radius alone - parity with
			// SpawnDefendWaypoint(pos), which is all the old DEFEND branch did.
			vector holdPoint = centre;
			if (GroupsAreStationedDeliberately())
				holdPoint = groupPosition;

			return OVT_VirtualPlanFactory.BuildDefendPlan(holdPoint, 0);
		}

		if (m_ePatrolType != OVT_PatrolType.PERIMETER)
			return null;

		float radius = m_fPatrolRadius;
		if (radius <= 0)
		{
			// Parity again: an unauthored radius meant "circle at whatever distance you already are".
			radius = vector.Distance(groupPosition, centre);
		}

		array<float> waitSeconds = new array<float>();
		for (int i = 0; i < OVT_VirtualPlanFactory.PERIMETER_POINTS; i++)
		{
			waitSeconds.Insert(s_AIRandomGenerator.RandFloatXY(WAIT_SECONDS_MIN, WAIT_SECONDS_MAX));
		}

		OVT_VirtualWaypointPlan plan = OVT_VirtualPlanFactory.BuildPerimeterPlan(centre, groupPosition, radius, waitSeconds);
		SnapPatrolPointsToRoads(plan);

		return plan;
	}

	//------------------------------------------------------------------------------------------------
	//! Pulls every patrol corner onto the nearest road, taking its pause with it.
	//!
	//! Walks the plan by TYPE rather than by index arithmetic, so it keeps working if the factory's
	//! interleaving ever changes: a PATROL point is snapped, and a WAIT copies whatever point came
	//! before it. Only the positions move - the three parallel arrays keep their lengths, so the plan
	//! stays registrable.
	//! \param[in] plan The plan to snap in place. Null is a no-op.
	protected void SnapPatrolPointsToRoads(OVT_VirtualWaypointPlan plan)
	{
		if (!plan || !plan.m_aPositions || !plan.m_aTypes)
			return;

		int count = plan.m_aPositions.Count();
		if (plan.m_aTypes.Count() < count)
			return;

		for (int i = 0; i < count; i++)
		{
			if (plan.m_aTypes[i] == OVT_EVirtualWaypointType.PATROL)
			{
				plan.m_aPositions.Set(i, OVT_WorldUtils.FindNearestRoad(plan.m_aPositions[i]));
				continue;
			}

			if (plan.m_aTypes[i] == OVT_EVirtualWaypointType.WAIT && i > 0)
				plan.m_aPositions.Set(i, plan.m_aPositions[i - 1]);
		}
	}

	//------------------------------------------------------------------------------------------------
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_PatrolBehaviorDeploymentModule clone = new OVT_PatrolBehaviorDeploymentModule();

		// Copy configuration. EVERY attribute has to appear here - a forgotten one silently ships the
		// class default instead of the authored value, which is how m_fMaxCruiseSpeed was lost on the
		// vehicle module for a whole release (D1's standing trap).
		clone.m_sModuleName = m_sModuleName;
		clone.m_ePatrolType = m_ePatrolType;
		clone.m_fPatrolRadius = m_fPatrolRadius;
		clone.m_fCheckInterval = m_fCheckInterval;
		clone.m_bApplyToNewGroups = m_bApplyToNewGroups;
		clone.m_bUseNearestTownCenter = m_bUseNearestTownCenter;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this deployment's groups are put where somebody decided they should stand.
	//!
	//! ANY spawning module answering true is enough, and that is exact rather than sloppy for every
	//! config that ships: no config mixes a deliberate module with a rolling one. A future config that
	//! did would want the anchor decided per module, which needs the spawning module to be passed down
	//! into BuildVirtualPlan - a bigger change than any shipped config justifies today.
	//!
	//! NO DEPLOYMENT answers TRUE, deliberately: a config TEMPLATE has no marker to anchor on at all
	//! (GetPatrolCenter() returns Zero and the caller has already fallen back to the group position), so
	//! the group position is the only meaningful answer and this keeps the two paths agreeing.
	//! \return Whether the DEFEND anchor should be the group rather than the marker.
	protected bool GroupsAreStationedDeliberately()
	{
		if (!m_ParentDeployment)
			return true;

		array<OVT_BaseSpawningDeploymentModule> spawningModules = m_ParentDeployment.GetSpawningModules();
		if (!spawningModules)
			return false;

		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			if (spawningModule && spawningModule.StationsGroupsDeliberately())
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	//! What the patrol circles: the nearest town centre when this module is authored to use one, and
	//! the deployment's own position otherwise.
	//!
	//! Kept public-facing behaviour unchanged from the waypoint-authoring version, because it is also
	//! what makes the DEFEND branch usable as a garrison's "hold this post".
	//! \return The centre, or vector.Zero when there is no deployment to ask.
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

		vector patrolCenter = GetPatrolCenter();
		Print(string.Format("  Patrol Center: %1", patrolCenter.ToString()));

		OVT_VirtualWaypointPlan plan = BuildVirtualPlan(patrolCenter);
		if (!plan)
		{
			Print("  Plan: none (this patrol type has no opinion)");
			return;
		}

		string cycles = "No";
		if (plan.m_bCycle)
			cycles = "Yes";
		Print(string.Format("  Plan: %1 point(s), cycles: %2", plan.m_aPositions.Count(), cycles));
	}
}
