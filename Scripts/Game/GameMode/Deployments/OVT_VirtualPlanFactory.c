//------------------------------------------------------------------------------------------------
//! Waypoint plans for virtualized deployment groups - PURE, SYSTEM-FREE geometry.
//!
//! ===========================================================================================
//! HARD RULE: NOTHING IN THIS FILE MAY TOUCH A SYSTEM, THE GAME MODE OR ANY LIVE STATE.
//! ===========================================================================================
//!
//! A plan is registration-time INPUT: the virtualization core turns it into the real waypoints a
//! group owns, and a group's plan is also the ONLY opt-in there is for being walked while dormant.
//! A plan with nothing movable in it is a garrison that holds its post; a plan with patrol or move
//! points in it is a patrol that keeps patrolling with nobody watching. That is the whole contract,
//! and it is why these four builders are worth pinning in the cheapest tier in the tree.
//!
//! GEOMETRY ONLY. Every position handed back is a raw offset from the arguments: no road snapping,
//! no surface clamp, no reachability check. The CALLER snaps what it wants snapped (the shipped
//! perimeter patrol snaps its four corners onto roads, exactly as the hand-authored version did),
//! and core clamps Y onto the surface when it builds the waypoint itself.
//!
//! THE THREE ARRAYS ARE PARALLEL. Positions, types and parameters are consumed by index and a
//! ragged plan is refused outright at registration rather than half-built, so every builder below
//! appends to all three or to none of them. An EMPTY plan is legal and means "no waypoints".
//!
//! BEARINGS. Yaw is measured in the XZ plane, and Y is passed through from the centre - a plan is
//! authored flat and gets its height when the waypoint is created.
//------------------------------------------------------------------------------------------------
class OVT_VirtualPlanFactory
{
	//! Corners of a perimeter patrol. Four, 90 degrees apart, matching the hand-authored geometry
	//! this replaces.
	static const int PERIMETER_POINTS = 4;

	//! Degrees between two perimeter corners.
	static const float PERIMETER_STEP_DEGREES = 90;

	//! Below this, a direction has no meaningful bearing and 0 is used instead.
	static const float BEARING_EPSILON = 0.001;

	//------------------------------------------------------------------------------------------------
	//! One DEFEND point, and nothing to advance along.
	//!
	//! This is what makes a garrison a garrison: DEFEND is not movable, so a dormant group holding
	//! this plan is never walked anywhere, whatever the tick does around it.
	//! \param[in] centre The post to hold.
	//! \param[in] radius Completion radius for the point, in metres; 0 leaves the prefab's own.
	//! \return A one-point, non-cycling plan.
	static OVT_VirtualWaypointPlan BuildDefendPlan(vector centre, float radius)
	{
		OVT_VirtualWaypointPlan plan = new OVT_VirtualWaypointPlan();

		AddPoint(plan, centre, OVT_EVirtualWaypointType.DEFEND, radius);
		plan.m_bCycle = false;

		return plan;
	}

	//------------------------------------------------------------------------------------------------
	//! Four corners 90 degrees apart around a centre, each with a pause on it.
	//!
	//! The first corner sits on the bearing from fromPosition TOWARDS the centre and one radius
	//! BEYOND it, which is the geometry the hand-authored perimeter patrol asked for: the corner a
	//! group walks to first is the one on the far side of what it is circling, so it sets off across
	//! the area rather than turning on the spot.
	//!
	//! Each corner is immediately followed by a WAIT at the SAME position, so a patrol pauses where
	//! it arrived instead of pacing without rest. The durations are passed in rather than rolled
	//! here: a roll would make this unassertable, and the caller already owns the band it wants.
	//!
	//! The plan CYCLES - a perimeter patrol that stopped at its fourth corner would be a patrol that
	//! guards one quarter of the area for the rest of the campaign.
	//! \param[in] centre What is being circled.
	//! \param[in] fromPosition Where the group is now; only its bearing to the centre is used.
	//! \param[in] radius Distance from the centre to each corner, in metres.
	//! \param[in] waitSeconds One duration per corner; missing or short entries pause for 0.
	//! \return An 8-entry cycling plan: PATROL, WAIT, PATROL, WAIT, PATROL, WAIT, PATROL, WAIT.
	static OVT_VirtualWaypointPlan BuildPerimeterPlan(vector centre, vector fromPosition, float radius, array<float> waitSeconds)
	{
		OVT_VirtualWaypointPlan plan = new OVT_VirtualWaypointPlan();

		float bearing = BearingBetween(fromPosition, centre);

		for (int i = 0; i < PERIMETER_POINTS; i++)
		{
			float corner = bearing + (i * PERIMETER_STEP_DEGREES);
			vector position = centre + (vector.FromYaw(corner) * radius);
			position[1] = centre[1];

			AddPoint(plan, position, OVT_EVirtualWaypointType.PATROL, 0);
			AddPoint(plan, position, OVT_EVirtualWaypointType.WAIT, WaitAt(waitSeconds, i));
		}

		plan.m_bCycle = true;

		return plan;
	}

	//------------------------------------------------------------------------------------------------
	//! The same four corners, but on an AUTHORED square rather than on the group's own bearing.
	//!
	//! WHY THIS EXISTS AND WHAT IT IS FOR. BuildPerimeterPlan's ring is anchored on whoever is walking
	//! it, and the shipped consumer then pulls each corner onto the nearest road - which is right for a
	//! town (roads ring a town) and wrong for a base (roads run THROUGH a base, so the "perimeter"
	//! collapses onto the access road). A base designer instead authors ONE square on the base
	//! controller - a radius and a rotation - and every PERIMETER_BASE deployment at that base walks
	//! that square. Amendment A1, 2026-08-18, from the play-test: "the road positions make sense for
	//! town patrols but not the base garrisons... I'd actually like to make them a little authored".
	//!
	//! THE SQUARE'S ORIENTATION IS AUTHORED; WHICH CORNER YOU START AT IS NOT. The four corners are at
	//! rotationDeg + k*90 and nowhere else, so every group at one base walks the same four points. But
	//! the corner a group walks to FIRST is the one nearest the bearing from fromPosition towards the
	//! centre - the same convention BuildPerimeterPlan uses, and for the same reason: a group sets off
	//! across the area rather than turning on the spot, and two garrisons approaching from different
	//! sides do not walk the square in lockstep.
	//!
	//! Everything else is BuildPerimeterPlan verbatim: a WAIT at each corner's own position, no
	//! completion radius on the corners, and the plan CYCLES.
	//!
	//! GEOMETRY ONLY, like everything else here - the caller ground-snaps (a base's square crosses real
	//! terrain and the corners arrive at the centre's Y).
	//! \param[in] centre What is being circled - the base.
	//! \param[in] fromPosition Where the group is now; only its bearing to the centre is used, and only
	//!            to choose which authored corner comes first.
	//! \param[in] radius Distance from the centre to each corner, in metres.
	//! \param[in] rotationDeg Yaw of the FIRST authored corner, in degrees. The other three follow at
	//!            +90, +180 and +270.
	//! \param[in] waitSeconds One duration per corner, in walk order; missing or short entries pause 0.
	//! \return An 8-entry cycling plan: PATROL, WAIT, PATROL, WAIT, PATROL, WAIT, PATROL, WAIT.
	static OVT_VirtualWaypointPlan BuildSquarePerimeterPlan(vector centre, vector fromPosition, float radius, float rotationDeg, array<float> waitSeconds)
	{
		OVT_VirtualWaypointPlan plan = new OVT_VirtualWaypointPlan();

		int start = StartCornerIndex(BearingBetween(fromPosition, centre), rotationDeg);

		for (int i = 0; i < PERIMETER_POINTS; i++)
		{
			// The wrap is taken into its OWN int local on purpose: inlined into the float expression
			// below, EnforceScript promotes both operands and refuses % outright ("Unknown operator").
			int cornerIndex = (start + i) % PERIMETER_POINTS;

			float corner = rotationDeg + (cornerIndex * PERIMETER_STEP_DEGREES);
			vector position = centre + (vector.FromYaw(corner) * radius);
			position[1] = centre[1];

			AddPoint(plan, position, OVT_EVirtualWaypointType.PATROL, 0);
			AddPoint(plan, position, OVT_EVirtualWaypointType.WAIT, WaitAt(waitSeconds, i));
		}

		plan.m_bCycle = true;

		return plan;
	}

	//------------------------------------------------------------------------------------------------
	//! Which of an authored square's four corners lies nearest a bearing.
	//!
	//! The +0.5 truncation is a deliberate rounding-to-nearest without Math.Round: normalised, the
	//! quotient is in [0, 4), so the shifted value truncates to 0..4 and the wrap brings 4 home to 0.
	//! A bearing exactly between two corners answers the higher one, which is arbitrary and stable.
	//! \param[in] bearing Yaw the group is coming from, in degrees.
	//! \param[in] rotationDeg Yaw of the square's first corner, in degrees.
	//! \return 0..3.
	protected static int StartCornerIndex(float bearing, float rotationDeg)
	{
		float delta = NormalizeDegrees(bearing - rotationDeg);
		int index = (delta / PERIMETER_STEP_DEGREES) + 0.5;

		return index % PERIMETER_POINTS;
	}

	//------------------------------------------------------------------------------------------------
	//! An angle folded into [0, 360).
	//!
	//! Written out rather than done with a modulo because EnforceScript's % is integer-only and keeps
	//! the sign of its left operand: a negative authored rotation would otherwise fold to a negative
	//! corner index and read off the end of the square.
	//! \param[in] degrees Any angle.
	//! \return The same angle in [0, 360).
	protected static float NormalizeDegrees(float degrees)
	{
		float wrapped = degrees - (Math.Floor(degrees / 360) * 360);

		if (wrapped < 0)
			wrapped += 360;

		if (wrapped >= 360)
			wrapped -= 360;

		return wrapped;
	}

	//------------------------------------------------------------------------------------------------
	//! A MOVE and a WAIT per stop, and optionally one MOVE back to where the group started.
	//!
	//! Cycling and returning to the start are the two ways a route can avoid ending in a dead stop,
	//! and they are mutually exclusive: a route that comes home has a natural end and must not also
	//! loop, while one that does not come home loops so that it keeps running.
	//!
	//! An empty stop list yields an EMPTY plan rather than a lone closing point. A route with no
	//! stops has nowhere to come back from, and an empty plan is legal - it registers a group with no
	//! waypoints, which is a garrison.
	//! \param[in] stops The route, in order; may be null or empty.
	//! \param[in] waitSeconds How long to pause at each stop, in seconds.
	//! \param[in] returnToStart Append a closing MOVE at startPosition and do not cycle.
	//! \param[in] startPosition Where the closing MOVE goes; ignored unless returnToStart.
	//! \return The plan; empty when there are no stops.
	static OVT_VirtualWaypointPlan BuildRoutePlan(array<vector> stops, float waitSeconds, bool returnToStart, vector startPosition)
	{
		OVT_VirtualWaypointPlan plan = new OVT_VirtualWaypointPlan();
		plan.m_bCycle = !returnToStart;

		if (!stops || stops.IsEmpty())
			return plan;

		foreach (vector stop : stops)
		{
			AddPoint(plan, stop, OVT_EVirtualWaypointType.MOVE, 0);
			AddPoint(plan, stop, OVT_EVirtualWaypointType.WAIT, waitSeconds);
		}

		if (returnToStart)
			AddPoint(plan, startPosition, OVT_EVirtualWaypointType.MOVE, 0);

		return plan;
	}

	//------------------------------------------------------------------------------------------------
	//! One SEARCH point per site, each holding for its own rolled duration, cycling.
	//!
	//! THE TOWN SWEEP'S HOUSE ROUTE. A SEARCH point is the vanilla Search & Destroy waypoint pinned to one
	//! building (core fixes the radius; the plan carries the hold), so a group walking this plan goes
	//! house to house, pokes around and inside each one for its hold, and moves on - and while dormant
	//! the movement tick treats SEARCH as a WAIT, so the patrol keeps touring the houses with nobody
	//! watching and materialises AT one rather than on a road corner.
	//!
	//! NO WAIT FOLLOWS A SEARCH. The hold IS the pause; a WAIT after it would stand the men outside the
	//! house they just searched for a second spell, which is the road-corner loitering this replaces.
	//!
	//! THE SITES ARE TAKEN IN THE ORDER GIVEN - the caller decides the route (OrderNearestNeighbour is
	//! the shipped answer) because the caller knows where the group starts. Holds are passed in rather
	//! than rolled, so the shape is assertable and the caller owns the band.
	//!
	//! CYCLES, like the perimeter: a sweep that searched its last house and stopped would be a garrison
	//! on somebody's doorstep for the rest of the campaign. An EMPTY site list builds an empty plan,
	//! which is legal ("no waypoints") - the shipped caller never hands one in, it falls back to the
	//! loose ring instead.
	//! \param[in] sites Building positions, in route order.
	//! \param[in] holdSeconds One duration per site; missing or short entries hold for 0.
	//! \return A cycling plan with one SEARCH entry per site.
	static OVT_VirtualWaypointPlan BuildSearchPlan(array<vector> sites, array<float> holdSeconds)
	{
		OVT_VirtualWaypointPlan plan = new OVT_VirtualWaypointPlan();
		plan.m_bCycle = true;

		if (!sites)
			return plan;

		for (int i = 0; i < sites.Count(); i++)
		{
			AddPoint(plan, sites[i], OVT_EVirtualWaypointType.SEARCH, WaitAt(holdSeconds, i));
		}

		return plan;
	}

	//------------------------------------------------------------------------------------------------
	//! Orders a set of sites into a walkable route: from `start`, always the nearest unvisited next.
	//!
	//! A handful of houses rolled at random from a whole town, taken in roll order, is a zig-zag that
	//! crosses the town between every pair - the men spend the patrol commuting. Nearest-neighbour from
	//! where the group actually starts is the cheapest ordering that reads as a ROUTE, and for five or
	//! six sites it is within a few percent of optimal; anything cleverer would be unassertable for no
	//! visible gain. Ties keep input order.
	//!
	//! PURE: a new array is returned, the input is untouched, and a null or empty input answers an
	//! empty array.
	//! \param[in] start Where the group sets off from.
	//! \param[in] sites The positions to order.
	//! \return The same positions, nearest-neighbour ordered from `start`.
	static array<vector> OrderNearestNeighbour(vector start, array<vector> sites)
	{
		array<vector> ordered = new array<vector>();
		if (!sites || sites.IsEmpty())
			return ordered;

		array<vector> remaining = new array<vector>();
		remaining.Copy(sites);

		vector current = start;
		while (!remaining.IsEmpty())
		{
			int nearestIndex = 0;
			float nearestDistance = vector.DistanceSq(current, remaining[0]);

			for (int i = 1; i < remaining.Count(); i++)
			{
				float distance = vector.DistanceSq(current, remaining[i]);
				if (distance < nearestDistance)
				{
					nearestDistance = distance;
					nearestIndex = i;
				}
			}

			current = remaining[nearestIndex];
			ordered.Insert(current);
			remaining.RemoveOrdered(nearestIndex);
		}

		return ordered;
	}

	//------------------------------------------------------------------------------------------------
	//! Appends to all three arrays at once. The ONE place a point is added, so the arrays cannot
	//! drift out of step and hand core a ragged plan.
	//! \param[in] plan The plan being built.
	//! \param[in] position Where the point goes.
	//! \param[in] type An OVT_EVirtualWaypointType.
	//! \param[in] waypointParam Wait seconds for WAIT and SEARCH, completion radius for everything else.
	protected static void AddPoint(notnull OVT_VirtualWaypointPlan plan, vector position, int type, float waypointParam)
	{
		plan.m_aPositions.Insert(position);
		plan.m_aTypes.Insert(type);
		plan.m_aParams.Insert(waypointParam);
	}

	//------------------------------------------------------------------------------------------------
	//! Yaw from one position to another, in degrees, in the XZ plane.
	//!
	//! Two positions on top of each other have no bearing at all; 0 is answered rather than whatever
	//! a zero-length direction happens to produce, so a degenerate call still builds a legal plan
	//! instead of a plan full of NaNs.
	//! \param[in] from Where the bearing is taken from.
	//! \param[in] to What it points at.
	//! \return Yaw in degrees, or 0 when the two positions share a spot.
	protected static float BearingBetween(vector from, vector to)
	{
		vector direction = vector.Direction(from, to);

		if (Math.AbsFloat(direction[0]) < BEARING_EPSILON && Math.AbsFloat(direction[2]) < BEARING_EPSILON)
			return 0;

		return direction.ToYaw();
	}

	//------------------------------------------------------------------------------------------------
	//! The pause for one corner, defaulting to none.
	//!
	//! A short or absent list is not an error: a caller that does not care about pausing passes
	//! nothing and gets a patrol that walks its corners without stopping.
	//! \param[in] waitSeconds The durations the caller supplied; may be null.
	//! \param[in] index Which corner is being built.
	//! \return The supplied duration, or 0.
	protected static float WaitAt(array<float> waitSeconds, int index)
	{
		if (!waitSeconds)
			return 0;

		if (index < 0 || index >= waitSeconds.Count())
			return 0;

		return waitSeconds[index];
	}
}
