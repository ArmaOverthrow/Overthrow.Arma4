//------------------------------------------------------------------------------------------------
//! Virtual movement - PURE arithmetic. No engine state of any kind is read here.
//!
//! ===========================================================================================
//! HARD RULE: NOTHING IN THIS FILE MAY REACH FOR A SYSTEM, A REGISTRY OR ANY GAME STATE.
//! Every symbol is an argument, a constant or Math. That is what lets the cheapest test tier
//! (which loads nothing at all) assert the whole of the progression logic.
//! ===========================================================================================
//!
//! Everything the virtual movement tick decides that is expressible as a pure function of its
//! inputs lives here: projecting a position onto a waypoint polyline, classifying a plan as
//! stationary, picking the next target index, resolving a step, advancing toward a target,
//! draining a wait, ordering handles and spotting an externally moved group.
//!
//! Y IS NEVER COMPARED. Every write the tick makes is ground-snapped, so Y changes on its own on
//! every pass; a comparison that included it would report motion that never happened. Every
//! distance here is therefore XZ only, and AdvanceTowardsXZ() passes the caller's Y straight
//! through untouched.
//!
//! The only engine symbols referenced are Math, the Vector() constructor and the enum
//! OVT_EVirtualWaypointType (an int constant set) - none of which need any live state.
//------------------------------------------------------------------------------------------------
class OVT_VirtualMovementMath
{
	//! Arrival tolerance in metres. Also what makes a large step land ON a waypoint instead of
	//! oscillating around it: arrival triggers at max(step, this).
	static const float ARRIVAL_RADIUS_M = 10;

	//! Caps a single step after a stalled call queue or a debugger pause, so nothing ever jumps
	//! hundreds of metres in one write.
	static const float MAX_STEP_SECONDS = 30;

	//! XZ distance above which "somebody else moved this group" is assumed. Comfortably above
	//! floating point noise, and Y (the ground snap) is not part of the comparison at all.
	static const float EXTERNAL_MOVE_EPSILON_M = 1;

	//------------------------------------------------------------------------------------------------
	//! Planar distance between two positions, ignoring Y.
	//! \param[in] a First position.
	//! \param[in] b Second position.
	//! \return Distance in metres on the XZ plane, never negative.
	static float DistanceXZ(vector a, vector b)
	{
		float dx = b[0] - a[0];
		float dz = b[2] - a[2];

		return Math.Sqrt(dx * dx + dz * dz);
	}

	//------------------------------------------------------------------------------------------------
	//! Planar distance from a point to the SEGMENT a->b (not the infinite line), ignoring Y.
	//!
	//! The projection primitive. A DEGENERATE segment (a == b in XZ) answers the plain point
	//! distance rather than dividing by zero - a plan may legitimately repeat a position.
	//! \param[in] p The point.
	//! \param[in] a Segment start.
	//! \param[in] b Segment end.
	//! \return Distance in metres on the XZ plane.
	static float DistancePointToSegmentXZ(vector p, vector a, vector b)
	{
		float abx = b[0] - a[0];
		float abz = b[2] - a[2];

		float lengthSq = abx * abx + abz * abz;
		if (lengthSq <= 0)
			return DistanceXZ(p, a);

		float t = ((p[0] - a[0]) * abx + (p[2] - a[2]) * abz) / lengthSq;
		if (t < 0)
			t = 0;

		if (t > 1)
			t = 1;

		vector closest = Vector(a[0] + abx * t, p[1], a[2] + abz * t);

		return DistanceXZ(p, closest);
	}

	//------------------------------------------------------------------------------------------------
	//! The stateless resume heuristic: which leg of a plan was this position walking?
	//!
	//! Answers the END index of the leg whose segment is nearest the given position - i.e. the
	//! index to keep walking toward. An OFF-ROUTE position (a group delivered kilometres away, or
	//! one a consumer teleported) answers the nearest leg, which is exactly the sane thing to head
	//! for. Ties answer the earliest leg, so the result is deterministic.
	//! \param[in] positions The plan's waypoint positions, in plan order.
	//! \param[in] cycle True when the plan loops, which adds the closing leg (last -> first).
	//! \param[in] position The position to project.
	//! \return The target index, 0 for a single-point plan, -1 for a null or empty plan.
	static int ProjectOntoPlan(array<vector> positions, bool cycle, vector position)
	{
		if (!positions || positions.IsEmpty())
			return -1;

		int count = positions.Count();
		if (count == 1)
			return 0;

		int best = -1;
		float bestDistance = 0;

		for (int i = 0; i < count - 1; i++)
		{
			float distance = DistancePointToSegmentXZ(position, positions[i], positions[i + 1]);
			if (best < 0 || distance < bestDistance)
			{
				best = i + 1;
				bestDistance = distance;
			}
		}

		// The closing leg of a looping plan walks back to index 0.
		if (cycle)
		{
			float closing = DistancePointToSegmentXZ(position, positions[count - 1], positions[0]);
			if (closing < bestDistance)
			{
				best = 0;
				bestDistance = closing;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Is there anything in this plan to advance along?
	//!
	//! The plan IS the opt-in: a group registered with no plan, or with a plan whose every point is
	//! DEFEND, is saying "this group belongs here" and is never advanced. A RAGGED plan (the two
	//! arrays disagree on count) answers TRUE as well - a plan that the registry would have refused
	//! must never be walked, because the types would be read against the wrong positions.
	//! \param[in] positions The plan's waypoint positions.
	//! \param[in] types OVT_EVirtualWaypointType per position, parallel to positions.
	//! \return True when the plan must never be advanced.
	static bool IsStationaryPlan(array<vector> positions, array<int> types)
	{
		if (!positions || positions.IsEmpty())
			return true;

		if (!types)
			return true;

		if (types.Count() != positions.Count())
			return true;

		for (int i = 0; i < types.Count(); i++)
		{
			if (types[i] != OVT_EVirtualWaypointType.DEFEND)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Picks the index to walk toward after arriving at `current`.
	//!
	//! A LOOPING plan wraps to 0 and always walks forward. A non-looping plan with two or more
	//! points PING-PONGS: `direction` is flipped at either end and handed back, because a patrol
	//! that reaches the end of its route and then freezes for the rest of the campaign is the exact
	//! frozen-route defect this whole feature exists to remove.
	//!
	//! A plan with one point (or none) has nowhere to go and answers -1, which the caller latches as
	//! stationary. That holds for a looping single-point plan too: wrapping onto itself would
	//! re-arrive forever.
	//! \param[in] current The index just arrived at.
	//! \param[in] count Number of points in the plan.
	//! \param[in] cycle True when the plan loops.
	//! \param[inout] direction +1 / -1 walk direction; flipped in place at either end of a
	//!                        non-looping route, and forced to +1 for a looping one.
	//! \return The next target index, or -1 when there is nowhere to go.
	static int NextTargetIndex(int current, int count, bool cycle, inout int direction)
	{
		if (count <= 1)
			return -1;

		int index = current;
		if (index < 0)
			index = 0;

		if (index > count - 1)
			index = count - 1;

		if (cycle)
		{
			direction = 1;
			return (index + 1) % count;
		}

		int step = -1;
		if (direction >= 0)
			step = 1;

		int next = index + step;

		if (next > count - 1)
		{
			step = -1;
			next = index - 1;
		}
		else if (next < 0)
		{
			step = 1;
			next = index + 1;
		}

		direction = step;

		return next;
	}

	//------------------------------------------------------------------------------------------------
	//! How far to move this pass.
	//!
	//! Elapsed time is measured per group, not assumed per pass, because a round-robin touches a
	//! group less often as the registry grows - a fixed per-pass step would make every group crawl
	//! at scale. The elapsed value is clamped so a stalled queue or a paused debugger cannot bank
	//! time into one huge jump, and a NEGATIVE elapsed (a time reset) floors at 0 rather than
	//! throwing the group backwards.
	//! \param[in] speedMs Metres per second; 0 or negative means "do not move".
	//! \param[in] dtSeconds Seconds since this group was last touched.
	//! \param[in] maxStepSeconds Cap on the elapsed value, normally MAX_STEP_SECONDS.
	//! \return Step distance in metres, never negative.
	static float ResolveStepDistance(float speedMs, float dtSeconds, float maxStepSeconds)
	{
		if (speedMs <= 0)
			return 0;

		float cap = maxStepSeconds;
		if (cap < 0)
			cap = 0;

		float dt = dtSeconds;
		if (dt < 0)
			dt = 0;

		if (dt > cap)
			dt = cap;

		return speedMs * dt;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves `from` toward `target` by `step` metres on the XZ plane, CLAMPED to the target.
	//!
	//! Never overshoots: a step longer than the remaining distance lands exactly on the target and
	//! reports arrival, with no remainder carried into the next leg. Arrival also triggers inside
	//! ARRIVAL_RADIUS_M, which is what stops a small step oscillating around a waypoint forever.
	//!
	//! Y IS PASSED THROUGH from `from` and never read from `target` - the caller ground-snaps.
	//! \param[in] from Current position.
	//! \param[in] target Position being walked to.
	//! \param[in] step Distance to cover this pass, from ResolveStepDistance().
	//! \param[out] arrived True when the returned position IS the target (in XZ).
	//! \return The new position, with `from`'s Y.
	static vector AdvanceTowardsXZ(vector from, vector target, float step, out bool arrived)
	{
		float remaining = DistanceXZ(from, target);

		float threshold = step;
		if (threshold < ARRIVAL_RADIUS_M)
			threshold = ARRIVAL_RADIUS_M;

		if (remaining <= threshold)
		{
			arrived = true;
			return Vector(target[0], from[1], target[2]);
		}

		arrived = false;

		if (step <= 0)
			return from;

		float t = step / remaining;

		float x = from[0] + (target[0] - from[0]) * t;
		float z = from[2] + (target[2] - from[2]) * t;

		return Vector(x, from[1], z);
	}

	//------------------------------------------------------------------------------------------------
	//! Drains a wait timer, floored at 0.
	//!
	//! A negative elapsed value cannot ADD time back onto the timer, so a clock reset can never
	//! extend a wait.
	//! \param[in] remaining Seconds left on the wait.
	//! \param[in] dtSeconds Seconds elapsed since the last touch.
	//! \return The seconds left afterwards, never below 0.
	static float DrainWait(float remaining, float dtSeconds)
	{
		float dt = dtSeconds;
		if (dt < 0)
			dt = 0;

		float left = remaining - dt;
		if (left < 0)
			left = 0;

		return left;
	}

	//------------------------------------------------------------------------------------------------
	//! In-place ascending insertion sort over registration handles.
	//!
	//! HAND-ROLLED ON PURPOSE. The tree's only array Sort() call sites are object arrays and the
	//! ordering semantics for an int array were never verified; a hand-rolled sort is also directly
	//! assertable in the cheapest test tier. Handles come from a monotonic counter, so ascending
	//! order IS registration order - which is what makes the round-robin stable rather than one that
	//! can starve a group forever.
	//! \param[in] handles The array to order in place; a null or single-element array is a no-op.
	static void SortHandlesAscending(array<int> handles)
	{
		if (!handles)
			return;

		int count = handles.Count();
		if (count < 2)
			return;

		for (int i = 1; i < count; i++)
		{
			int value = handles[i];
			int j = i - 1;

			while (j >= 0 && handles[j] > value)
			{
				handles[j + 1] = handles[j];
				j--;
			}

			handles[j + 1] = value;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Did somebody other than the movement tick move this group?
	//!
	//! XZ ONLY. The recorded position is re-read every pass and compared against the last position
	//! the tick itself wrote; a gap means the group was placed somewhere by someone else, and the
	//! progression state must be re-derived from scratch. Comparing Y would fire on every single
	//! pass, because the tick ground-snaps what it writes.
	//! \param[in] recorded The position read back this pass.
	//! \param[in] lastWritten The last position the tick wrote.
	//! \param[in] epsilon Tolerance in metres, normally EXTERNAL_MOVE_EPSILON_M.
	//! \return True when the difference exceeds the tolerance.
	static bool IsExternalMove(vector recorded, vector lastWritten, float epsilon)
	{
		return DistanceXZ(recorded, lastWritten) > epsilon;
	}
}
