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
//!
//! FOUR TYPES, THREE OF THEM MOVABLE (amendment A1, 2026-08-18; TOWN_SWEEP 2026-08-21):
//!   DEFEND         - one post, never walked.
//!   PERIMETER      - a ring around the centre, EACH CORNER ROAD-SNAPPED. The old town patrol; kept
//!                    for any config that still wants a road ring.
//!   PERIMETER_BASE - the nearest base controller's AUTHORED square (m_fPerimeterRadius /
//!                    m_fPerimeterRotation ± a few degrees of jitter), ground-snapped and NOT
//!                    road-snapped. The base garrison.
//!   TOWN_SWEEP     - the town patrol now. Rolled PER GROUP: usually a house-to-house SEARCH route
//!                    through a handful of the town's houses, otherwise a LOOSE ring of random radius
//!                    up to the town's own range, ground-snapped and not pulled onto roads. See
//!                    BuildTownSweepPlan for why the road ring had to go.
//! PERIMETER and PERIMETER_BASE produce plans of the SAME KIND - 4 PATROL corners, 4 WAITs, cycling -
//! so everything downstream (the movement tick, core's waypoint builder, the GM waypoint walk) treats
//! them identically and nothing outside this file has to know which one built a plan. TOWN_SWEEP's
//! ring is that kind too; its house route is N SEARCH points, cycling, and SEARCH is a WAIT to the
//! movement tick and a Search & Destroy waypoint to live AI.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_PatrolBehaviorDeploymentModule : OVT_BaseBehaviorDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;

	[Attribute(defvalue: "1", UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(OVT_PatrolType), desc: "Type of patrol behavior. DEFEND holds one post and is never walked; PERIMETER walks a ROAD-SNAPPED ring around the centre; PERIMETER_BASE walks the nearest base controller's AUTHORED square, un-snapped (bases); TOWN_SWEEP rolls per group between searching m_iSweepHouseCount of the town's houses one after another and a loose un-snapped ring of random radius up to the town's range (towns). All but DEFEND are walked while dormant")]
	OVT_PatrolType m_ePatrolType;

	[Attribute(defvalue: "200", desc: "Patrol radius for PERIMETER patrols. PERIMETER_BASE ignores this and uses the base controller's own m_fPerimeterRadius, falling back to this only when there is no base in range")]
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

	[Attribute(defvalue: "5", desc: "TOWN_SWEEP only: how many of the town's houses one group's search route visits before it starts over. Rolled fresh per group, so two patrols in one town search different houses")]
	int m_iSweepHouseCount;

	[Attribute(defvalue: "0.7", desc: "TOWN_SWEEP only: the chance (0-1), rolled per group, that a group searches houses rather than walking the loose ring. A town with no searchable house always gets the ring")]
	float m_fSweepHouseChance;

	//! Shortest pause rolled for a perimeter corner, in seconds. The band is inherited verbatim from the
	//! hand-authored perimeter helper on OVT_OverthrowConfigComponent that this replaces.
	static const float WAIT_SECONDS_MIN = 45;

	//! Longest pause rolled for a perimeter corner, in seconds.
	static const float WAIT_SECONDS_MAX = 75;

	//! How far, in degrees, one PERIMETER_BASE patrol's square may be rotated off the base's authored
	//! rotation. Rolled FRESH every time a plan is built, so successive garrisons - and the same
	//! garrison after a rebuy - do not tread one line. Small on purpose: the point of an authored
	//! square is that a designer decided where it goes, and this is the wobble around that decision,
	//! not a second decision. The Workbench viz draws this band as two faint squares.
	static const float PERIMETER_ROTATION_JITTER_DEG = 10;

	//! Shortest time a sweep spends searching one house, in seconds. Long enough for the Search & Destroy
	//! activity to get a fireteam through its grid once (walk up, poke the rooms, look round the yard),
	//! short enough that a five-house route comes back round inside a quarter of an hour.
	static const float SEARCH_SECONDS_MIN = 60;

	//! Longest time a sweep spends searching one house, in seconds.
	static const float SEARCH_SECONDS_MAX = 120;

	//! The smallest loose ring, as a fraction of the town's range. A ring much tighter than this around
	//! a village centre is four men turning on the spot in the square; much looser than the town range
	//! is a ring through the fields outside it, which is not a town patrol.
	static const float LOOSE_RING_MIN_FRACTION = 0.3;

	//! The largest loose ring, as a fraction of the town's range - the town's own edge.
	static const float LOOSE_RING_MAX_FRACTION = 1.0;

	//! Scratch for the house query callbacks. A member because the engine's query API takes instance
	//! methods and no context; filled and drained inside FindHousePositions, never read elsewhere.
	protected ref array<vector> m_aHouseScratch;

	//------------------------------------------------------------------------------------------------
	//! The plan a group registered at groupPosition should carry.
	//!
	//! ROLLED PER GROUP, which is why the wait durations and the starting bearing are not shared: two
	//! patrols around one town centre set off in different directions and pause for different lengths,
	//! exactly as the four hand-authored waypoint sets did.
	//!
	//! WORLD CONTACT IS APPLIED HERE, not in the factory - the factory is world-free geometry and cannot
	//! ask where the roads or the hillsides are. PERIMETER pulls each PATROL corner onto the nearest
	//! road and the WAIT that follows it copies the snapped position, so a patrol pauses where it
	//! actually arrived; PERIMETER_BASE snaps every point onto the terrain surface instead and touches
	//! no road at all (see BuildAuthoredSquarePlan for why a base must not be road-snapped).
	//!
	//! ⚠ THE TYPES ANCHOR ON DIFFERENT THINGS, DELIBERATELY. Both PERIMETER types circle
	//! GetPatrolCenter() (the deployment marker, or the town centre when m_bUseNearestTownCenter is
	//! set) because circling the PLACE is what a patrol is for. DEFEND holds groupPosition, because a garrison holds where it
	//! was stationed - see the branch's own comment for what anchoring it on the marker instead does to
	//! every config that places its groups away from that marker.
	//! \param[in] groupPosition Where the group is about to be registered.
	//! \return The plan, or null when this module's patrol type has nothing to say.
	override OVT_VirtualWaypointPlan BuildVirtualPlan(vector groupPosition)
	{
		// Resolved before the type split so the call order is unchanged for every type; only the two
		// PERIMETER types read it.
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
			// Both PERIMETER types keep GetPatrolCenter()'s answer unconditionally, and must: circling the
			// TOWN (or the BASE) is the entire point of a patrol, and they are the types where the centre
			// is deliberately not where the group stands.
			//
			// Radius 0 leaves the defend waypoint prefab's own completion radius alone - parity with
			// SpawnDefendWaypoint(pos), which is all the old DEFEND branch did.
			vector holdPoint = centre;
			if (GroupsAreStationedDeliberately())
				holdPoint = groupPosition;

			return OVT_VirtualPlanFactory.BuildDefendPlan(holdPoint, 0);
		}

		if (m_ePatrolType == OVT_PatrolType.PERIMETER_BASE)
			return BuildAuthoredSquarePlan(centre, groupPosition);

		if (m_ePatrolType == OVT_PatrolType.TOWN_SWEEP)
			return BuildTownSweepPlan(centre, groupPosition);

		if (m_ePatrolType != OVT_PatrolType.PERIMETER)
			return null;

		float radius = m_fPatrolRadius;
		if (radius <= 0)
		{
			// Parity again: an unauthored radius meant "circle at whatever distance you already are".
			radius = vector.Distance(groupPosition, centre);
		}

		OVT_VirtualWaypointPlan plan = OVT_VirtualPlanFactory.BuildPerimeterPlan(centre, groupPosition, radius, RollCornerWaits());
		SnapPatrolPointsToRoads(plan);

		return plan;
	}

	//------------------------------------------------------------------------------------------------
	//! The PERIMETER_BASE plan: the nearest base's AUTHORED square, jittered a little, NOT road-snapped.
	//!
	//! ================== WHY BASES DO NOT GET THE ROAD-SNAPPED RING (amendment A1) ==============
	//! From the play-test, verbatim: "the garrison waypoints aren't great. the road positions make
	//! sense for town patrols but not the base garrisons. i'd actually like to make them a little
	//! authored". A town's roads run AROUND it, so snapping a ring's corners onto them produces a real
	//! patrol route; a base's roads run THROUGH it, so the same snap collapses the "perimeter" onto the
	//! access road and every garrison walks the same strip of tarmac. So this branch takes its geometry
	//! from two numbers a designer authored on the base controller and snaps nothing to a road.
	//!
	//! Plain PERIMETER is untouched and still means the town patrol's road-snapped ring - the two are
	//! separate enum members precisely so neither can drift into the other.
	//! ==========================================================================================
	//!
	//! THE JITTER IS ROLLED HERE, ONCE PER PLAN. Every deployment at a base would otherwise walk the
	//! identical four points; ±PERIMETER_ROTATION_JITTER_DEG spreads successive garrisons (and a
	//! garrison rebought after a wipe) around the authored square without moving it.
	//!
	//! NO BASE IN RANGE IS A CONFIG ERROR, AND IT WARNS RATHER THAN FAILING SILENT. The plan is still
	//! built - an un-snapped square at rotation 0 around the deployment - because a mis-authored config
	//! that patrols the wrong shape is far easier to notice than one whose groups stand still forever.
	//! \param[in] centre What is being circled - GetPatrolCenter()'s answer.
	//! \param[in] groupPosition Where the group is about to be registered.
	//! \return An 8-entry cycling plan, ground-snapped.
	protected OVT_VirtualWaypointPlan BuildAuthoredSquarePlan(vector centre, vector groupPosition)
	{
		float radius = m_fPatrolRadius;
		float rotation = 0;

		OVT_BaseControllerComponent controller = OVT_BaseControllerComponent.FindNearestBaseControllerWithin(centre, OVT_DeploymentManagerComponent.BASE_CLASSIFICATION_RADIUS);
		if (controller)
		{
			// A base that authors a radius of 0 keeps the module's own - the same "an unauthored value
			// means fall through to the next opinion" rule the PERIMETER branch uses.
			if (controller.m_fPerimeterRadius > 0)
				radius = controller.m_fPerimeterRadius;

			rotation = controller.m_fPerimeterRotation;
		}
		else
		{
			Print(string.Format("[Overthrow] Deployment '%1' authors PERIMETER_BASE but no base controller is within %2 m of its patrol centre - falling back to an un-snapped square at rotation 0. Either the config is authored at the wrong location type, or the base marker is missing",
				DescribeOwner(), OVT_DeploymentManagerComponent.BASE_CLASSIFICATION_RADIUS.ToString()), LogLevel.WARNING);
		}

		if (radius <= 0)
		{
			// Last resort, and the same parity fallback the PERIMETER branch carries: circle at
			// whatever distance the group already stands at.
			radius = vector.Distance(groupPosition, centre);
		}

		rotation += s_AIRandomGenerator.RandFloatXY(-PERIMETER_ROTATION_JITTER_DEG, PERIMETER_ROTATION_JITTER_DEG);

		OVT_VirtualWaypointPlan plan = OVT_VirtualPlanFactory.BuildSquarePerimeterPlan(centre, groupPosition, radius, rotation, RollCornerWaits());
		SnapPlanPointsToGround(plan);

		return plan;
	}

	//------------------------------------------------------------------------------------------------
	//! The TOWN_SWEEP plan: per group, either a house-to-house search route or a loose un-snapped ring.
	//!
	//! ================== WHY THE TOWN PATROL LEFT THE ROADS (2026-08-21) ==========================
	//! From the play-test, verbatim: "right now they just do a road-snapped perimeter but its kinda
	//! boring, as well as means they do end up standing in the middle of a road waiting on each one,
	//! getting in the way of the director's insertion missions or giving the player an easy target for
	//! their vehicle. Id like the town patrols to be going from house to house 'searching' them and just
	//! being general douchebags to the civilians". So the corners stopped being road corners. What a
	//! group gets instead is ROLLED HERE, ONCE, when its plan is built:
	//!
	//!   HOUSE ROUTE (m_fSweepHouseChance) - m_iSweepHouseCount houses picked at random from every
	//!   ownable house inside the town's range (player-owned ones included - the author's call: a patrol
	//!   that searches YOUR house is the point), nearest-neighbour ordered from where the group starts so
	//!   it reads as a route rather than a commute, one SEARCH point per house with its own rolled hold.
	//!   Live, SEARCH is the vanilla Search & Destroy waypoint pinned to the house (core fixes the radius
	//!   to one building), so the men walk up, investigate navmesh spots in and around it, and leave when
	//!   the hold runs out. Dormant, it is a WAIT, so the route keeps turning with nobody watching.
	//!
	//!   LOOSE RING (otherwise, and ALWAYS when the town has no searchable house) - the same four-corner
	//!   cycling plan PERIMETER builds, at a radius rolled between LOOSE_RING_MIN_FRACTION and
	//!   LOOSE_RING_MAX_FRACTION of the town's range, ground-snapped and NOT road-snapped. Two patrols
	//!   on one town therefore walk rings of different sizes in different directions, and neither parks
	//!   on the tarmac. The one exception is a corner that lands in the sea (coastal towns with a large
	//!   range): that corner alone is pulled onto the nearest road, because a corner nobody can reach
	//!   stalls the live patrol at it forever, and a road is the nearest thing to "still on the town".
	//!
	//! THE TOWN'S AUTHORED RANGE (OVT_TownControllerComponent.m_iTownRange, the one the designer set on the
	//! placed controller and the civilian crowd already uses), NOT m_fPatrolRadius, sizes both halves,
	//! because the point is to cover the town the patrol is in - Eden's run from 100 m to 213 m and one
	//! authored radius fits none of them. See ResolveTownRange for the fallbacks. The radius attribute is
	//! the last resort for a plan built with no town to ask (a template, or a marker dropped in the open),
	//! exactly as it is for PERIMETER.
	//! ==========================================================================================
	//! \param[in] centre What is being swept - GetPatrolCenter()'s answer (the town centre, normally).
	//! \param[in] groupPosition Where the group is about to be registered; the route starts here.
	//! \return A cycling plan, never null.
	protected OVT_VirtualWaypointPlan BuildTownSweepPlan(vector centre, vector groupPosition)
	{
		float range = ResolveTownRange(centre);

		if (s_AIRandomGenerator.RandFloat01() < m_fSweepHouseChance)
		{
			array<vector> houses = FindHousePositions(centre, range);
			if (houses && !houses.IsEmpty())
			{
				array<vector> picked = PickRandomSites(houses, m_iSweepHouseCount);
				array<vector> route = OVT_VirtualPlanFactory.OrderNearestNeighbour(groupPosition, picked);

				array<float> holds = new array<float>();
				for (int i = 0; i < route.Count(); i++)
				{
					holds.Insert(s_AIRandomGenerator.RandFloatXY(SEARCH_SECONDS_MIN, SEARCH_SECONDS_MAX));
				}

				return OVT_VirtualPlanFactory.BuildSearchPlan(route, holds);
			}
		}

		float radius = range * s_AIRandomGenerator.RandFloatXY(LOOSE_RING_MIN_FRACTION, LOOSE_RING_MAX_FRACTION);
		if (radius <= 0)
			radius = vector.Distance(groupPosition, centre);

		OVT_VirtualWaypointPlan plan = OVT_VirtualPlanFactory.BuildPerimeterPlan(centre, groupPosition, radius, RollCornerWaits());
		SnapLooseRing(plan);

		return plan;
	}

	//------------------------------------------------------------------------------------------------
	//! How far the town around `centre` reaches - the range AUTHORED on its town controller.
	//!
	//! ⚠ NOT OVT_TownManagerComponent.GetTownRange(). That is the deprecated per-SIZE fallback (250/400/
	//! 600 by village/town/city); every shipped map places OVT_TownController prefabs in a towns.layer and
	//! authors m_iTownRange on each (Eden's run 100-213 m), and the civilian crowd already sizes itself from
	//! that same field. Sizing the patrol off the size table would sweep houses two to four times further
	//! out than the town the designer drew - the first build of this did exactly that, caught by the author.
	//! The manager's table is kept only as the fallback for a town with no controller (the deprecated
	//! auto-detected path), and m_fPatrolRadius for no town at all.
	//!
	//! Asked of the town manager by POSITION rather than through the town conditional module, so a plan
	//! built off a config template (no deployment, no sibling modules) still sizes itself to the real town
	//! nearest the position it was handed; the Init tier relies on that.
	//! \param[in] centre The position to size for.
	//! \return The town's range in metres, or m_fPatrolRadius when there is no town to ask.
	protected float ResolveTownRange(vector centre)
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
			return m_fPatrolRadius;

		OVT_TownData town = towns.GetNearestTown(centre);
		if (!town)
			return m_fPatrolRadius;

		OVT_TownControllerComponent controller = FindTownController(towns, town);
		if (controller && controller.m_iTownRange > 0)
			return controller.m_iTownRange;

		float range = towns.GetTownRange(town);
		if (range > 0)
			return range;

		return m_fPatrolRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! The town controller entity the manager keeps for `town`, index-aligned with its town list.
	//! \param[in] towns The town manager.
	//! \param[in] town The town.
	//! \return Its controller component, or null when the town has none (or the ID no longer resolves).
	protected OVT_TownControllerComponent FindTownController(notnull OVT_TownManagerComponent towns, notnull OVT_TownData town)
	{
		int townId = towns.GetTownID(town);
		if (townId < 0 || !towns.m_TownControllers || townId >= towns.m_TownControllers.Count())
			return null;

		EntityID controllerId = towns.m_TownControllers[townId];
		if (!controllerId)
			return null;

		IEntity entity = GetGame().GetWorld().FindEntityByID(controllerId);
		if (!entity)
			return null;

		return OVT_TownControllerComponent.Cast(entity.FindComponent(OVT_TownControllerComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Every searchable house within `range` of `centre`, as positions.
	//!
	//! "Searchable" is the real-estate manager's own ownable-building filter - the same one the town
	//! manager uses to count population and hand out homes - so a patrol searches exactly the buildings
	//! the game already calls houses, and nothing else (no barns, no bunkers, no ruins). OWNED houses are
	//! deliberately NOT excluded.
	//! \param[in] centre Where to look.
	//! \param[in] range How far, in metres.
	//! \return House positions; empty when there are none or nothing can be queried.
	protected array<vector> FindHousePositions(vector centre, float range)
	{
		m_aHouseScratch = new array<vector>();

		BaseWorld world = GetGame().GetWorld();
		if (world && range > 0)
			world.QueryEntitiesBySphere(centre, range, AddHouseToScratch, FilterHouseEntities, EQueryEntitiesFlags.STATIC);

		array<vector> houses = m_aHouseScratch;
		m_aHouseScratch = null;
		return houses;
	}

	//------------------------------------------------------------------------------------------------
	//! Query add-callback for FindHousePositions.
	protected bool AddHouseToScratch(IEntity entity)
	{
		if (m_aHouseScratch && entity)
			m_aHouseScratch.Insert(entity.GetOrigin());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Query filter for FindHousePositions: an ownable building, per the real-estate manager.
	protected bool FilterHouseEntities(IEntity entity)
	{
		if (!entity || entity.ClassName() != "SCR_DestructibleBuildingEntity")
			return false;

		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if (!realEstate)
			return false;

		return realEstate.BuildingIsOwnable(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Up to `count` distinct sites rolled at random from `sites`, in roll order.
	//!
	//! A partial Fisher-Yates on a copy: each roll swaps a random unpicked entry into the next slot,
	//! so no site is picked twice and the input is untouched. ⚠ RandInt is MAX-EXCLUSIVE, which is
	//! why the upper bound is Count() and not Count() - 1.
	//! \param[in] sites The pool.
	//! \param[in] count How many to take; clamped to the pool's size, 0 or less takes none.
	//! \return The picked sites.
	protected array<vector> PickRandomSites(notnull array<vector> sites, int count)
	{
		array<vector> pool = new array<vector>();
		pool.Copy(sites);

		if (count > pool.Count())
			count = pool.Count();

		array<vector> picked = new array<vector>();
		for (int i = 0; i < count; i++)
		{
			int roll = s_AIRandomGenerator.RandInt(i, pool.Count());
			vector chosen = pool[roll];
			pool.Set(roll, pool[i]);
			pool.Set(i, chosen);
			picked.Insert(chosen);
		}

		return picked;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a loose ring's corners on the ground, and pulls any corner in the sea onto the nearest road.
	//!
	//! Walks the plan by TYPE like SnapPatrolPointsToRoads does: a PATROL corner is checked and snapped,
	//! and a WAIT copies whatever point came before it, so the pause stays where the patrol arrives.
	//! \param[in] plan The plan to snap in place. Null is a no-op.
	protected void SnapLooseRing(OVT_VirtualWaypointPlan plan)
	{
		if (!plan || !plan.m_aPositions || !plan.m_aTypes)
			return;

		int count = plan.m_aPositions.Count();
		if (plan.m_aTypes.Count() < count)
			return;

		BaseWorld world = GetGame().GetWorld();

		for (int i = 0; i < count; i++)
		{
			if (plan.m_aTypes[i] == OVT_EVirtualWaypointType.PATROL)
			{
				vector position = plan.m_aPositions[i];

				if (OVT_WorldUtils.IsOceanAtPosition(position))
					position = OVT_WorldUtils.FindNearestRoad(position);
				else if (world)
					position[1] = world.GetSurfaceY(position[0], position[2]);

				plan.m_aPositions.Set(i, position);
				continue;
			}

			if (plan.m_aTypes[i] == OVT_EVirtualWaypointType.WAIT && i > 0)
				plan.m_aPositions.Set(i, plan.m_aPositions[i - 1]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One rolled pause per corner.
	//!
	//! ROLLED PER GROUP, which is why it is not shared or cached: two patrols around one centre pause
	//! for different lengths, exactly as the four hand-authored waypoint sets did.
	//! \return PERIMETER_POINTS durations, in seconds.
	protected array<float> RollCornerWaits()
	{
		array<float> waitSeconds = new array<float>();
		for (int i = 0; i < OVT_VirtualPlanFactory.PERIMETER_POINTS; i++)
		{
			waitSeconds.Insert(s_AIRandomGenerator.RandFloatXY(WAIT_SECONDS_MIN, WAIT_SECONDS_MAX));
		}

		return waitSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts every point of a plan on the terrain surface, in place.
	//!
	//! WHY THE AUTHORED SQUARE NEEDS THIS AND THE ROAD-SNAPPED RING DOES NOT: FindNearestRoad() answers
	//! a real road position, which is already on the ground. A square corner is raw geometry at the
	//! CENTRE'S Y, so on any base that is not perfectly flat it arrives buried in a hillside or hanging
	//! in the air - and a completion radius smaller than that Y error can never be reached by live AI.
	//! This is the same GetSurfaceY() clamp the virtualization core applies when it spawns the waypoint
	//! entity, done here as well so the PERSISTED plan carries positions a person could stand on too.
	//!
	//! ⚠ IT DOES NOT AVOID WATER. A coastal base with a large authored radius can put a corner in the
	//! sea, and nothing moves it: relocating one corner would stop the square being the square that was
	//! authored. The Workbench viz is the mitigation - the designer sees the square where they set it.
	//! \param[in] plan The plan to snap in place. Null is a no-op.
	protected void SnapPlanPointsToGround(OVT_VirtualWaypointPlan plan)
	{
		if (!plan || !plan.m_aPositions)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		for (int i = 0; i < plan.m_aPositions.Count(); i++)
		{
			vector position = plan.m_aPositions[i];
			position[1] = world.GetSurfaceY(position[0], position[2]);
			plan.m_aPositions.Set(i, position);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Something a warning can name this module by, whether or not there is a deployment behind it.
	//! \return The deployment name, or this module's own name.
	protected string DescribeOwner()
	{
		if (m_ParentDeployment)
			return m_ParentDeployment.GetDeploymentName();

		return m_sModuleName;
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
		clone.m_iSweepHouseCount = m_iSweepHouseCount;
		clone.m_fSweepHouseChance = m_fSweepHouseChance;

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
			OVT_DeploymentLog.Debug("Patrol behavior: No town found, using deployment position as patrol center");
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
