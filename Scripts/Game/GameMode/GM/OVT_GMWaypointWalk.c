//------------------------------------------------------------------------------------------------
//! The server-side read of one AI group's route. STRICTLY READ-ONLY, AND NEVER A WORLD SCAN.
//!
//! THE ONLY ENTRY POINT IS A GROUP, AND THAT IS A CORRECTNESS PROPERTY, NOT A STYLE CHOICE.
//! Overthrow leaks detached waypoint entities in several places - QRF group lists are never drained,
//! and until virtualization/integration Phase 5 the deployments framework's shared group cleanup
//! deleted a group's soldiers but not the group - so a world
//! scan for AIWaypoint entities would happily surface orphans belonging to nothing and draw a route
//! nobody is walking. Asking the GROUP for its waypoints can only ever return waypoints that group
//! actually holds. Do not add a scan, a cache keyed on anything but a group, or a lookup by position.
//!
//! NOTHING HERE WRITES. No AddWaypoint, no AddWaypointAt, no RemoveWaypoint, no RemoveWaypointAt, no
//! CompleteWaypoint, no SetWaypoints, no SetCompletionRadius, no SetCompletionType. A visualization
//! feature that quietly changes AI behaviour is worse than no visualization, and the feature's
//! quality bar greps this file for exactly those names.
//!
//! WHY THE FLATTEN EXISTS. Overthrow's perimeter patrols put ALL of their waypoints inside one
//! AIWaypointCycle (OVT_OverthrowConfigComponent.c:524-551), so a group with an eight-point patrol
//! route reports a SINGLE waypoint - the cycle container, sitting at the group's spawn position. Left
//! unflattened, the most common route in the game draws as one meaningless leg. Flatten replaces a
//! cycle with its children and marks the route cyclic.
//!
//! THE WALK IS CORRECT WHETHER OR NOT THE ENGINE ALREADY EXPANDS CYCLES. AIWaypointCycle.PerformOn
//! inserts the cycled waypoints (and the cycle itself) into the running group's waypoint list, so a
//! group that is executing a cycle may report either shape. If GetWaypoints() already hands back the
//! children, Flatten is a pass-through that emits each one once - the duplicate guard below is what
//! keeps the re-inserted cycle container from expanding a second time - and if it hands back the
//! container, Flatten expands it. Neither path prints anything and neither can loop.
//------------------------------------------------------------------------------------------------
class OVT_GMWaypointWalk
{
	//! How deep the expansion goes: level 1 is the group's own list, level 2 is one cycle's children.
	//! A cycle found among those children is EMITTED AS ITSELF rather than expanded - it classifies as
	//! CYCLE and draws as a single vertex. Two levels covers every shape Overthrow authors, and a
	//! fixed cap means a malformed cycle-inside-itself costs one comparison rather than the server.
	static const int MAX_DEPTH = 2;

	//------------------------------------------------------------------------------------------------
	//! Flattens a waypoint list, expanding one level of AIWaypointCycle children.
	//!
	//! \param[in] src The list to flatten; null or empty is a normal, silent no-op.
	//! \param[in] max Hard cap on emitted waypoints. Zero or less emits nothing.
	//! \param[out] flat Caller-allocated destination, CLEARED first. Never contains a cycle container
	//!             that was expanded, never contains the same entity twice.
	//! \param[out] cyclic True when any expanded cycle was found, i.e. the route closes back on itself.
	//! \param[out] truncated True when the cap stopped the walk before the source ran out.
	//! \return The number of waypoints written into flat.
	static int Flatten(array<AIWaypoint> src, int max, out array<AIWaypoint> flat, out bool cyclic, out bool truncated)
	{
		cyclic = false;
		truncated = false;

		if (!flat)
			return 0;

		flat.Clear();

		if (!src || max <= 0)
			return 0;

		// Every container this walk has already opened. A cycle that contains itself - directly or
		// through a sibling - is therefore opened once and never again, which is the whole defence
		// against a malformed route hanging the server.
		array<AIWaypoint> expanded = new array<AIWaypoint>();
		array<AIWaypoint> children = new array<AIWaypoint>();

		foreach (AIWaypoint waypoint : src)
		{
			if (!waypoint)
				continue;

			AIWaypointCycle cycle = AIWaypointCycle.Cast(waypoint);
			if (!cycle)
			{
				if (!Emit(waypoint, max, flat, truncated))
					return flat.Count();

				continue;
			}

			// A cycle is a cycle whether or not it turns out to hold anything, so the route is marked
			// before the children are read: an empty cycle still says "this group loops".
			cyclic = true;

			if (expanded.Find(cycle) != -1)
				continue; // already opened once - see MAX_DEPTH

			expanded.Insert(cycle);

			children.Clear();
			cycle.GetWaypoints(children);

			foreach (AIWaypoint child : children)
			{
				if (!child)
					continue;

				// Depth 2 is the floor: a nested cycle is emitted as an entry of its own rather than
				// opened, so the walk is bounded by construction rather than by hoping routes are sane.
				if (!Emit(child, max, flat, truncated))
					return flat.Count();
			}
		}

		return flat.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! One group's flattened route, plus which of its entries the group is currently executing.
	//!
	//! currentIndex is found by IDENTITY comparison, never by position or by class: two waypoints of
	//! the same kind at the same place are two different answers. It is -1 when the group's current
	//! waypoint is not one of the emitted entries at all - the group has none, the engine reported the
	//! cycle container rather than a child, or the cap dropped it. All three are first-class states in
	//! which nothing is highlighted, and none of them prints.
	//! \param[in] group The group whose route to read; null gives an empty answer.
	//! \param[in] max Hard cap on emitted waypoints.
	//! \param[out] flat Caller-allocated destination, cleared first.
	//! \param[out] currentIndex Index into flat of the current waypoint, or -1.
	//! \param[out] cyclic True when the route closes back on itself.
	//! \param[out] truncated True when the cap stopped the walk.
	//! \return The number of waypoints written into flat.
	static int Collect(AIGroup group, int max, out array<AIWaypoint> flat, out int currentIndex, out bool cyclic, out bool truncated)
	{
		currentIndex = -1;
		cyclic = false;
		truncated = false;

		if (!flat)
			return 0;

		flat.Clear();

		if (!group)
			return 0;

		array<AIWaypoint> waypoints = new array<AIWaypoint>();
		group.GetWaypoints(waypoints);

		int count = Flatten(waypoints, max, flat, cyclic, truncated);
		if (count < 1)
			return 0;

		AIWaypoint current = group.GetCurrentWaypoint();
		if (!current)
			return count;

		foreach (int i, AIWaypoint waypoint : flat)
		{
			if (waypoint == current)
			{
				currentIndex = i;
				break;
			}
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Appends one waypoint, honouring the cap and skipping anything already emitted.
	//! \param[in] waypoint The waypoint to append; assumed non-null.
	//! \param[in] max Hard cap on emitted waypoints.
	//! \param[out] flat Destination list.
	//! \param[out] truncated Set when the cap refused this waypoint.
	//! \return True while the walk may continue; false once the cap has been hit.
	protected static bool Emit(AIWaypoint waypoint, int max, out array<AIWaypoint> flat, out bool truncated)
	{
		if (flat.Count() >= max)
		{
			truncated = true;
			return false;
		}

		// A waypoint the engine listed twice - the cycle container re-inserted by PerformOn is the
		// realistic case - is one vertex, not two.
		if (flat.Find(waypoint) != -1)
			return true;

		flat.Insert(waypoint);

		return true;
	}
}
