//------------------------------------------------------------------------------------------------
//! TIER A cases - OVT_VirtualMovementMath, the pure arithmetic behind virtual group movement.
//!
//! Everything asserted here is a static with no engine state behind it: projecting a position onto
//! a waypoint polyline, classifying a plan as stationary, picking the next target index, resolving
//! and applying a step, draining a wait, ordering handles and spotting an external move. The
//! subject class is deliberately free of every system, registry and game-state reference, which is
//! what lets these claims be asserted in the cheapest tier in the tree.
//!
//! WHAT IS NOT HERE. The tick itself - enumeration, slicing, the spawned gate, the ground snap and
//! the write - needs live systems and is asserted in the Init tier. Placement quality, the water
//! rule and the live/dormant handback are play-test items by design.
//!
//! ⚠ vector.Distance is +1 ULP off at exactly 1000 m and 2000 m (project memory). No case below
//! asserts an exact distance boundary: every distance claim carries a tolerance and is taken at a
//! small, safe magnitude (tens to hundreds of metres).
//!
//! No retry attribute is used anywhere - this quality bar bans it outright.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Projection: which leg of a plan was this position walking?
//!
//! This is the whole of the stateless resume. Progress is never serialized, so after a load, a
//! restart, a spawn cycle or a consumer teleporting a group, "which waypoint am I heading for" is
//! rebuilt by projecting the group's actual position onto the plan polyline. The answer must be the
//! END index of the nearest leg - the index to keep walking toward - and it must be defined for the
//! three awkward inputs as well: a single-point plan (0), an empty plan (-1) and the closing leg of
//! a looping plan (0, not the count).
//!
//! The off-route claim is the insertion seam: a group delivered kilometres from its route answers
//! the nearest leg, which is exactly what it should walk toward, with no registration argument and
//! no notification.
//!
//! FAIL PROOF (edit recorded; execution belongs to the phase's suite run): make ProjectOntoPlan
//! return `i` instead of `i + 1` when it records a new best, and the on-leg assertion goes red
//! (it answers 0 instead of 1). Drop the `if (cycle)` closing-leg block and the looping-plan
//! assertion goes red (it answers 2 instead of 0).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_VirtualMovement_Projection : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// An L-shaped three-point route: A(0,0) -> B(100,0) -> C(100,100).
		array<vector> route = {};
		route.Insert(Vector(0, 0, 0));
		route.Insert(Vector(100, 0, 0));
		route.Insert(Vector(100, 0, 100));

		// Standing halfway along the first leg: the target is that leg's END index.
		int target = OVT_VirtualMovementMath.ProjectOntoPlan(route, false, Vector(50, 12, 0));
		if (target != 1)
		{
			SetFailure("A position halfway along leg A->B projected to index %1, expected 1 (the leg's END index)", target.ToString());
			return true;
		}

		// Nearer the second leg: the later leg wins, even though the first leg is still 80 m away.
		target = OVT_VirtualMovementMath.ProjectOntoPlan(route, false, Vector(100, 3, 80));
		if (target != 2)
		{
			SetFailure("A position on leg B->C projected to index %1, expected 2", target.ToString());
			return true;
		}

		// Off-route entirely (a delivery drop behind the start): the NEAREST leg, which is A->B.
		target = OVT_VirtualMovementMath.ProjectOntoPlan(route, false, Vector(-200, 0, -50));
		if (target != 1)
		{
			SetFailure("An off-route position behind A projected to index %1, expected 1 - the nearest leg is A->B", target.ToString());
			return true;
		}

		// A single-point plan has exactly one answer.
		array<vector> single = {};
		single.Insert(Vector(500, 0, 500));

		target = OVT_VirtualMovementMath.ProjectOntoPlan(single, false, Vector(0, 0, 0));
		if (target != 0)
		{
			SetFailure("A single-point plan projected to index %1, expected 0", target.ToString());
			return true;
		}

		// An empty plan has no answer at all, and must say so rather than pointing at index 0.
		array<vector> empty = {};

		target = OVT_VirtualMovementMath.ProjectOntoPlan(empty, false, Vector(0, 0, 0));
		if (target != -1)
		{
			SetFailure("An empty plan projected to index %1, expected -1", target.ToString());
			return true;
		}

		// A null plan is the same claim: nothing to walk.
		target = OVT_VirtualMovementMath.ProjectOntoPlan(null, false, Vector(0, 0, 0));
		if (target != -1)
		{
			SetFailure("A null plan projected to index %1, expected -1", target.ToString());
			return true;
		}

		// The closing leg of a LOOPING plan walks back to index 0. Standing on the C->A diagonal is
		// 50 m from both open legs and 0 m from the closing one, so the answer must be 0.
		target = OVT_VirtualMovementMath.ProjectOntoPlan(route, true, Vector(50, 0, 50));
		if (target != 0)
		{
			SetFailure("A position on the closing leg of a looping plan projected to index %1, expected 0", target.ToString());
			return true;
		}

		// Without the loop, the very same position is NOT on a closing leg - it falls to an open one.
		target = OVT_VirtualMovementMath.ProjectOntoPlan(route, false, Vector(50, 0, 50));
		if (target == 0)
		{
			SetFailure("A non-looping plan answered index 0 for a mid-route position - the closing leg does not exist without the loop flag");
			return true;
		}

		Print("Projection answers the nearest leg's end index, including single-point (0), empty (-1) and the closing leg of a looping plan (0)");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Plan classification: the plan IS the opt-in, so this predicate is the whole contract.
//!
//! A consumer that wants a group to stay put registers no plan or an all-DEFEND plan; there is no
//! movement flag anywhere in the feature. If this predicate ever answered false for a DEFEND-only
//! plan, every garrison in the campaign would wander off its post while nobody was looking.
//!
//! The ragged case is the defensive half: a plan whose type array disagrees with its position array
//! is one the registry would have REFUSED, and walking it would read types against the wrong
//! positions. It answers stationary rather than guessing.
//!
//! FAIL PROOF (edit recorded): change the ragged guard to `return false;` and the ragged assertion
//! goes red. Change the loop's test to `if (types[i] == OVT_EVirtualWaypointType.DEFEND) return
//! false;` and both the all-DEFEND and the mixed assertions go red.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_VirtualMovement_PlanClassification : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<vector> positions = {};
		array<int> types = {};

		// No positions at all: a registration with no plan is a garrison.
		if (!OVT_VirtualMovementMath.IsStationaryPlan(positions, types))
		{
			SetFailure("An empty plan was classified as movable - a group with no plan must never be advanced");
			return true;
		}

		// Null arrays are the same claim, and must not throw.
		if (!OVT_VirtualMovementMath.IsStationaryPlan(null, null))
		{
			SetFailure("A null plan was classified as movable");
			return true;
		}

		// Two positions, both DEFEND: the group is at its post, wherever the second point is.
		positions.Insert(Vector(0, 0, 0));
		positions.Insert(Vector(150, 0, 0));
		types.Insert(OVT_EVirtualWaypointType.DEFEND);
		types.Insert(OVT_EVirtualWaypointType.DEFEND);

		if (!OVT_VirtualMovementMath.IsStationaryPlan(positions, types))
		{
			SetFailure("An all-DEFEND plan was classified as movable - garrisons would wander off their posts");
			return true;
		}

		// One point switched to PATROL: the plan now has something to advance along.
		types.Set(1, OVT_EVirtualWaypointType.PATROL);
		if (OVT_VirtualMovementMath.IsStationaryPlan(positions, types))
		{
			SetFailure("A DEFEND + PATROL plan was classified as stationary - a single movable point is enough to move");
			return true;
		}

		// A plain MOVE/PATROL pair is the ordinary patrol case.
		types.Set(0, OVT_EVirtualWaypointType.MOVE);
		if (OVT_VirtualMovementMath.IsStationaryPlan(positions, types))
		{
			SetFailure("A MOVE + PATROL plan was classified as stationary");
			return true;
		}

		// RAGGED: two positions, one type. Refused by being treated as stationary.
		array<int> ragged = {};
		ragged.Insert(OVT_EVirtualWaypointType.MOVE);

		if (!OVT_VirtualMovementMath.IsStationaryPlan(positions, ragged))
		{
			SetFailure("A ragged plan (2 positions, 1 type) was classified as movable - types would be read against the wrong positions");
			return true;
		}

		// Ragged the other way round is the same refusal.
		array<int> tooMany = {};
		tooMany.Insert(OVT_EVirtualWaypointType.MOVE);
		tooMany.Insert(OVT_EVirtualWaypointType.MOVE);
		tooMany.Insert(OVT_EVirtualWaypointType.MOVE);

		if (!OVT_VirtualMovementMath.IsStationaryPlan(positions, tooMany))
		{
			SetFailure("A ragged plan (2 positions, 3 types) was classified as movable");
			return true;
		}

		Print("Plan classification: empty, null, all-DEFEND and ragged plans are stationary; MOVE/PATROL and mixed plans are not");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Index advance: looping plans wrap, non-looping multi-point routes ping-pong.
//!
//! A patrol that reaches the end of its route and then freezes for the rest of the campaign is the
//! exact frozen-route defect this feature exists to remove, so a non-looping route flips direction
//! at BOTH ends rather than stopping. The direction is carried out through the inout parameter,
//! which is the only piece of progress that projection cannot recover after a load - so it is
//! asserted in both directions here, not just forward.
//!
//! A single-point plan answers -1: there is nowhere to ping-pong to, and wrapping a looping
//! single-point plan onto itself would re-arrive forever.
//!
//! FAIL PROOF (edit recorded): delete the `if (next > count - 1)` branch and the forward-end flip
//! assertion goes red (it answers 3 on a three-point route). Change the `count <= 1` guard to
//! `count <= 0` and the single-point assertion goes red (it answers 0 instead of -1).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_VirtualMovement_IndexAdvance : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		int direction = 1;

		// LOOPING: 0 -> 1 -> 2 -> 0, forever.
		int next = OVT_VirtualMovementMath.NextTargetIndex(0, 3, true, direction);
		if (next != 1)
		{
			SetFailure("A looping plan advanced from index 0 to %1, expected 1", next.ToString());
			return true;
		}

		next = OVT_VirtualMovementMath.NextTargetIndex(2, 3, true, direction);
		if (next != 0)
		{
			SetFailure("A looping plan advanced from the last index to %1, expected the wrap to 0", next.ToString());
			return true;
		}

		// A looping plan always walks forward, whatever direction it was carrying.
		direction = -1;
		next = OVT_VirtualMovementMath.NextTargetIndex(1, 3, true, direction);
		if (next != 2 || direction != 1)
		{
			SetFailure("A looping plan carrying direction -1 advanced to %1 with direction %2, expected index 2 and direction +1", next.ToString(), direction.ToString());
			return true;
		}

		// NON-LOOPING: walk forward to the end...
		direction = 1;
		next = OVT_VirtualMovementMath.NextTargetIndex(0, 3, false, direction);
		if (next != 1 || direction != 1)
		{
			SetFailure("A non-looping plan advanced from 0 to %1 with direction %2, expected index 1 and direction +1", next.ToString(), direction.ToString());
			return true;
		}

		next = OVT_VirtualMovementMath.NextTargetIndex(1, 3, false, direction);
		if (next != 2 || direction != 1)
		{
			SetFailure("A non-looping plan advanced from 1 to %1 with direction %2, expected index 2 and direction +1", next.ToString(), direction.ToString());
			return true;
		}

		// ...then flip at the FAR end rather than freezing there.
		next = OVT_VirtualMovementMath.NextTargetIndex(2, 3, false, direction);
		if (next != 1 || direction != -1)
		{
			SetFailure("A non-looping plan at its last index advanced to %1 with direction %2, expected the ping-pong back to index 1 with direction -1", next.ToString(), direction.ToString());
			return true;
		}

		// Walking back down carries the flipped direction.
		next = OVT_VirtualMovementMath.NextTargetIndex(1, 3, false, direction);
		if (next != 0 || direction != -1)
		{
			SetFailure("A non-looping plan walking back advanced from 1 to %1 with direction %2, expected index 0 and direction -1", next.ToString(), direction.ToString());
			return true;
		}

		// ...and flips again at the NEAR end. Both ends, not just the far one.
		next = OVT_VirtualMovementMath.NextTargetIndex(0, 3, false, direction);
		if (next != 1 || direction != 1)
		{
			SetFailure("A non-looping plan at index 0 walking backwards advanced to %1 with direction %2, expected the ping-pong to index 1 with direction +1", next.ToString(), direction.ToString());
			return true;
		}

		// A two-point route is a pure ping-pong: 0 <-> 1.
		direction = 1;
		next = OVT_VirtualMovementMath.NextTargetIndex(1, 2, false, direction);
		if (next != 0 || direction != -1)
		{
			SetFailure("A two-point route advanced from 1 to %1 with direction %2, expected index 0 and direction -1", next.ToString(), direction.ToString());
			return true;
		}

		// A single point has nowhere to go, looping or not, and latches stationary.
		direction = 1;
		next = OVT_VirtualMovementMath.NextTargetIndex(0, 1, false, direction);
		if (next != -1)
		{
			SetFailure("A single-point plan advanced to %1, expected -1", next.ToString());
			return true;
		}

		next = OVT_VirtualMovementMath.NextTargetIndex(0, 1, true, direction);
		if (next != -1)
		{
			SetFailure("A looping single-point plan advanced to %1, expected -1 - wrapping onto itself would re-arrive forever", next.ToString());
			return true;
		}

		// An empty plan is the same answer.
		next = OVT_VirtualMovementMath.NextTargetIndex(0, 0, true, direction);
		if (next != -1)
		{
			SetFailure("An empty plan advanced to %1, expected -1", next.ToString());
			return true;
		}

		Print("Index advance wraps when looping, ping-pongs at BOTH ends when not, and answers -1 for single-point and empty plans");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Stepping: how far to move, and landing on the target without overshooting it.
//!
//! Elapsed time is measured per group rather than assumed per tick, because the round-robin touches
//! a group less often as the registry grows. The clamp is what stops a stalled call queue banking
//! ten minutes of time into one 900 m jump; the negative floor is what stops a clock reset throwing
//! a group backwards.
//!
//! AdvanceTowardsXZ must CLAMP to the target - a step longer than the remaining distance lands
//! exactly ON the waypoint, with no remainder carried into the next leg, or a fast group would
//! oscillate around every waypoint forever. Y is passed through from the caller's position, never
//! taken from the target, because the tick ground-snaps every write itself.
//!
//! ⚠ All distances here are tens to hundreds of metres, well away from the 1000 m / 2000 m
//! magnitudes where vector.Distance is a ULP off, and every float claim carries a tolerance.
//!
//! FAIL PROOF (edit recorded): drop the `if (dt > cap)` clamp in ResolveStepDistance and the
//! clamp assertion goes red (150 instead of 45). Remove the `remaining <= threshold` early return
//! in AdvanceTowardsXZ and the overshoot assertion goes red (it lands 400 m past the target).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_VirtualMovement_Stepping : SCR_AutotestCaseBase
{
	protected const float EPSILON = 0.01;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// The ordinary case: 1.5 m/s over 10 s is 15 m.
		float step = OVT_VirtualMovementMath.ResolveStepDistance(1.5, 10, OVT_VirtualMovementMath.MAX_STEP_SECONDS);
		if (Math.AbsFloat(step - 15) > EPSILON)
		{
			SetFailure("1.5 m/s over 10 s resolved to a %1 m step, expected 15 m", step.ToString());
			return true;
		}

		// A banked 100 s clamps to MAX_STEP_SECONDS (30 s), i.e. 45 m, not 150 m.
		step = OVT_VirtualMovementMath.ResolveStepDistance(1.5, 100, OVT_VirtualMovementMath.MAX_STEP_SECONDS);
		if (Math.AbsFloat(step - 45) > EPSILON)
		{
			SetFailure("1.5 m/s over a banked 100 s resolved to a %1 m step, expected the 30 s clamp to give 45 m", step.ToString());
			return true;
		}

		// A negative elapsed (a clock reset) floors at 0 rather than moving the group backwards.
		step = OVT_VirtualMovementMath.ResolveStepDistance(1.5, -20, OVT_VirtualMovementMath.MAX_STEP_SECONDS);
		if (Math.AbsFloat(step) > EPSILON)
		{
			SetFailure("A negative elapsed resolved to a %1 m step, expected 0", step.ToString());
			return true;
		}

		// Zero elapsed is zero movement.
		step = OVT_VirtualMovementMath.ResolveStepDistance(1.5, 0, OVT_VirtualMovementMath.MAX_STEP_SECONDS);
		if (Math.AbsFloat(step) > EPSILON)
		{
			SetFailure("A zero elapsed resolved to a %1 m step, expected 0", step.ToString());
			return true;
		}

		// A non-positive speed never moves anything, whatever the elapsed value.
		step = OVT_VirtualMovementMath.ResolveStepDistance(0, 10, OVT_VirtualMovementMath.MAX_STEP_SECONDS);
		if (Math.AbsFloat(step) > EPSILON)
		{
			SetFailure("A speed of 0 resolved to a %1 m step, expected 0", step.ToString());
			return true;
		}

		step = OVT_VirtualMovementMath.ResolveStepDistance(-4, 10, OVT_VirtualMovementMath.MAX_STEP_SECONDS);
		if (Math.AbsFloat(step) > EPSILON)
		{
			SetFailure("A negative speed resolved to a %1 m step, expected 0", step.ToString());
			return true;
		}

		// Advance 40 m of a 100 m leg. Y comes from the START position, never from the target.
		vector from = Vector(0, 5, 0);
		vector target = Vector(100, 77, 0);
		bool arrived = true;

		vector moved = OVT_VirtualMovementMath.AdvanceTowardsXZ(from, target, 40, arrived);
		if (arrived)
		{
			SetFailure("A 40 m step along a 100 m leg reported arrival");
			return true;
		}

		if (Math.AbsFloat(moved[0] - 40) > EPSILON || Math.AbsFloat(moved[2]) > EPSILON)
		{
			SetFailure("A 40 m step along a 100 m leg landed at %1, expected (40, *, 0)", moved.ToString());
			return true;
		}

		if (Math.AbsFloat(moved[1] - 5) > EPSILON)
		{
			SetFailure("A step rewrote Y to %1, expected the caller's own 5 - the tick ground-snaps its own writes", moved[1].ToString());
			return true;
		}

		// A step LONGER than the remaining distance lands exactly ON the target, and says so.
		arrived = false;
		moved = OVT_VirtualMovementMath.AdvanceTowardsXZ(from, target, 500, arrived);
		if (!arrived)
		{
			SetFailure("A 500 m step along a 100 m leg did not report arrival");
			return true;
		}

		if (Math.AbsFloat(moved[0] - 100) > EPSILON || Math.AbsFloat(moved[2]) > EPSILON)
		{
			SetFailure("A 500 m step along a 100 m leg landed at %1, expected the target's XZ (100, *, 0) - it must not overshoot", moved.ToString());
			return true;
		}

		if (Math.AbsFloat(moved[1] - 5) > EPSILON)
		{
			SetFailure("An arriving step took Y from the target (%1), expected the caller's own 5", moved[1].ToString());
			return true;
		}

		// Inside the arrival radius, a tiny step still arrives instead of creeping forever.
		arrived = false;
		moved = OVT_VirtualMovementMath.AdvanceTowardsXZ(Vector(0, 5, 0), Vector(6, 0, 0), 0.5, arrived);
		if (!arrived)
		{
			SetFailure("A position 6 m from its target (inside the 10 m arrival radius) did not report arrival");
			return true;
		}

		if (Math.AbsFloat(moved[0] - 6) > EPSILON)
		{
			SetFailure("An arrival inside the radius landed at %1, expected the target's X of 6", moved.ToString());
			return true;
		}

		Print("Stepping clamps elapsed time, floors at 0 for a negative/zero elapsed and a non-positive speed, and lands ON the target without overshooting");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Wait drain: a WAIT waypoint pauses a dormant group, and can never pause it forever.
//!
//! The remaining time is drained by the SAME per-group elapsed value the step uses, so a group in a
//! round-robin waits the duration its plan asked for regardless of how often it is touched. A
//! zero-duration wait is already finished, which is what makes a plan authored with no parameter
//! advance immediately rather than stalling the route.
//!
//! FAIL PROOF (edit recorded): remove the `if (left < 0) left = 0;` floor and the over-drain
//! assertion goes red (-4 instead of 0). Remove the `if (dt < 0) dt = 0;` guard and the clock-reset
//! assertion goes red (15 instead of 5).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_VirtualMovement_WaitDrain : SCR_AutotestCaseBase
{
	protected const float EPSILON = 0.01;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// The ordinary drain.
		float left = OVT_VirtualMovementMath.DrainWait(60, 2);
		if (Math.AbsFloat(left - 58) > EPSILON)
		{
			SetFailure("A 60 s wait drained by 2 s left %1 s, expected 58", left.ToString());
			return true;
		}

		// Draining past the end floors at 0 - a negative remainder would read as "still waiting".
		left = OVT_VirtualMovementMath.DrainWait(1, 5);
		if (Math.AbsFloat(left) > EPSILON)
		{
			SetFailure("A 1 s wait drained by 5 s left %1 s, expected 0", left.ToString());
			return true;
		}

		// A zero-duration wait is already finished, so the route advances on the same pass.
		left = OVT_VirtualMovementMath.DrainWait(0, 0);
		if (Math.AbsFloat(left) > EPSILON)
		{
			SetFailure("A zero-duration wait reported %1 s remaining, expected 0", left.ToString());
			return true;
		}

		// An already-negative remainder is normalised rather than propagated.
		left = OVT_VirtualMovementMath.DrainWait(-3, 1);
		if (Math.AbsFloat(left) > EPSILON)
		{
			SetFailure("A negative remainder drained to %1 s, expected 0", left.ToString());
			return true;
		}

		// A negative elapsed cannot ADD time back onto a wait.
		left = OVT_VirtualMovementMath.DrainWait(5, -10);
		if (Math.AbsFloat(left - 5) > EPSILON)
		{
			SetFailure("A 5 s wait drained by a negative elapsed left %1 s, expected it unchanged at 5", left.ToString());
			return true;
		}

		Print("Wait drain never falls below 0, treats a zero-duration wait as finished and ignores a negative elapsed");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Handle ordering: the round-robin's stability depends entirely on this sort.
//!
//! The registry hands back its handles in map order, which has no guaranteed stability - and a
//! round-robin over an unstable order can starve one group forever. Handles come from a monotonic
//! counter, so ascending order IS registration order: sorting here makes the rotation deterministic
//! without asking the registry to maintain a second ordered structure.
//!
//! The sort is hand-rolled rather than array.Sort() because the tree's only Sort() call sites are
//! object arrays and int-array ordering semantics were never verified - which also means it needs
//! its own assertions, including the two degenerate inputs an insertion sort is most likely to trip
//! over.
//!
//! FAIL PROOF (edit recorded): change the insertion loop's guard from `handles[j] > value` to
//! `handles[j] < value` and the reversed-array assertion goes red (it comes back descending).
//! Change `for (int i = 1; ...)` to `for (int i = 2; ...)` and the reversed-array assertion goes red
//! as well (the second element is never placed).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_VirtualMovement_HandleOrdering : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// Empty: a no-op that must not throw, because an empty registry is the normal early state.
		array<int> handles = {};
		OVT_VirtualMovementMath.SortHandlesAscending(handles);
		if (!handles.IsEmpty())
		{
			SetFailure("Sorting an empty array produced %1 entries", handles.Count().ToString());
			return true;
		}

		// Null must be survivable too.
		OVT_VirtualMovementMath.SortHandlesAscending(null);

		// Single element.
		handles.Insert(7);
		OVT_VirtualMovementMath.SortHandlesAscending(handles);
		if (handles.Count() != 1 || handles[0] != 7)
		{
			SetFailure("Sorting a single-element array gave %1 entries starting with %2, expected 1 entry of 7", handles.Count().ToString(), handles[0].ToString());
			return true;
		}

		// Already sorted: unchanged, and nothing lost.
		array<int> sorted = {};
		sorted.Insert(1);
		sorted.Insert(2);
		sorted.Insert(3);

		OVT_VirtualMovementMath.SortHandlesAscending(sorted);
		if (sorted.Count() != 3 || sorted[0] != 1 || sorted[1] != 2 || sorted[2] != 3)
		{
			SetFailure("Sorting an already-sorted array gave (%1, %2, %3), expected (1, 2, 3)", sorted[0].ToString(), sorted[1].ToString(), sorted[2].ToString());
			return true;
		}

		// Fully reversed: the worst case for an insertion sort.
		array<int> reversed = {};
		reversed.Insert(9);
		reversed.Insert(5);
		reversed.Insert(3);
		reversed.Insert(1);

		OVT_VirtualMovementMath.SortHandlesAscending(reversed);
		if (reversed.Count() != 4)
		{
			SetFailure("Sorting a reversed array gave %1 entries, expected 4 - nothing may be lost", reversed.Count().ToString());
			return true;
		}

		if (reversed[0] != 1 || reversed[1] != 3 || reversed[2] != 5 || reversed[3] != 9)
		{
			string order = reversed[0].ToString() + ", " + reversed[1].ToString() + ", " + reversed[2].ToString() + ", " + reversed[3].ToString();
			SetFailure("Sorting a reversed array gave (%1), expected (1, 3, 5, 9)", order);
			return true;
		}

		Print("Handle ordering is ascending and non-destructive on empty, single, sorted and reversed arrays");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! External-move detection: one comparison covers adoption, teleport and hand-back.
//!
//! Each pass compares the position read back against the last position the tick itself wrote. A gap
//! means somebody else placed the group - a delivery, a consumer teleport, or live AI handing back
//! at a position the tick never chose - and the progression state is re-derived from scratch. That
//! single comparison is the whole insertion/extraction seam: no invoker, no notification, no
//! registration argument.
//!
//! It must be XZ ONLY. Every write is ground-snapped, so Y changes on its own on every pass; a
//! comparison that included Y would report an external move every single time and re-derive state
//! forever, silently defeating the ping-pong direction it just recovered.
//!
//! FAIL PROOF (edit recorded): make IsExternalMove call vector.Distance instead of DistanceXZ and
//! the pure-Y assertion goes red (a 38 m ground-snap difference reads as an external move).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_VirtualMovement_ExternalMove : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		vector written = Vector(1200, 5, 800);

		// The same position is obviously not an external move.
		if (OVT_VirtualMovementMath.IsExternalMove(written, written, OVT_VirtualMovementMath.EXTERNAL_MOVE_EPSILON_M))
		{
			SetFailure("An unchanged position was reported as an external move");
			return true;
		}

		// A pure Y difference is the GROUND SNAP, not somebody moving the group.
		vector snapped = Vector(1200, 43, 800);
		if (OVT_VirtualMovementMath.IsExternalMove(snapped, written, OVT_VirtualMovementMath.EXTERNAL_MOVE_EPSILON_M))
		{
			SetFailure("A 38 m difference in Y alone was reported as an external move - the tick ground-snaps every write, so this would fire every pass");
			return true;
		}

		// 5 m of XZ is somebody else's doing.
		vector teleported = Vector(1205, 5, 800);
		if (!OVT_VirtualMovementMath.IsExternalMove(teleported, written, OVT_VirtualMovementMath.EXTERNAL_MOVE_EPSILON_M))
		{
			SetFailure("A 5 m XZ difference was NOT reported as an external move - a delivered or teleported group would never be adopted");
			return true;
		}

		// Well inside the tolerance is float noise, not a move. (0.2 m, nowhere near the 1 m bound.)
		vector noisy = Vector(1200.2, 5, 800);
		if (OVT_VirtualMovementMath.IsExternalMove(noisy, written, OVT_VirtualMovementMath.EXTERNAL_MOVE_EPSILON_M))
		{
			SetFailure("A 0.2 m XZ difference was reported as an external move, well inside the 1 m tolerance");
			return true;
		}

		// A diagonal move counts on both axes, not just one.
		vector diagonal = Vector(1203, 5, 804);
		if (!OVT_VirtualMovementMath.IsExternalMove(diagonal, written, OVT_VirtualMovementMath.EXTERNAL_MOVE_EPSILON_M))
		{
			SetFailure("A 5 m diagonal XZ move was not reported as an external move");
			return true;
		}

		Print("External-move detection ignores a pure ground-snap Y change and catches a 5 m XZ displacement");
		return true;
	}
}
