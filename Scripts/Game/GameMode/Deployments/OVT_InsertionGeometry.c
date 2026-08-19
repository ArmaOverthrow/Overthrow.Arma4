//------------------------------------------------------------------------------------------------
//! Insertion geometry - PURE, SYSTEM-FREE arithmetic about getting a force from A to B.
//!
//! ===========================================================================================
//! HARD RULE: NOTHING IN THIS FILE MAY TOUCH A SYSTEM, THE GAME MODE OR ANY LIVE STATE.
//! ===========================================================================================
//!
//! Seven questions live here, and they are the seven an insertion keeps asking:
//!
//!     is this hop short enough to walk?                             ShouldWalk
//!     where, short of the target, do we put them down?              LZPointOnLine
//!     are we at the drop point?                                     IsInsideArrivalRadius
//!     are we there AND stopped?                                     HasArrived
//!     have we waited long enough for it to stop?                    IsSettleGraceExpired
//!     has the truck stopped being a truck?                          IsStuck
//!     is anyone ever coming back for that truck?                    IsAbandonedTruckCollectable
//!
//! WHY THIS IS A PURE FILE AND NOT METHODS ON THE MODULE. Every one of these decisions diverts a
//! live convoy onto a different path, and the path they divert onto - dismount and walk - is the one
//! that must work when everything else has failed. A wrong sign or a missing clamp in any of them is
//! silent: the men still exist, they just arrive somewhere nobody asked for, or never arrive at all.
//! None of that is a script error and none of it shows up in a log, so it is written where the
//! cheapest test tier can pin it row by row, with no world, no clock and no truck.
//!
//! CLAMPED EVERYWHERE, DELIBERATELY. Every function below is total: there is no argument combination
//! that produces a NaN, an infinity, a point behind the source or a point beyond the target. The
//! callers hand in numbers derived from live geometry - a distance between two moving things, an
//! authored standoff somebody may have typed as 5000, an elapsed time that can be zero on the frame a
//! deployment is created - and a pure function that only behaves for well-formed input is a pure
//! function that will be blamed for somebody else's edge case.
//!
//! GEOMETRY ONLY, LIKE THE PLAN FACTORY NEXT DOOR. Y is interpolated between the two endpoints and
//! nothing here knows what the ground is doing; the caller road-snaps or surface-clamps whatever it
//! is about to put a vehicle or a waypoint on.
//------------------------------------------------------------------------------------------------
class OVT_InsertionGeometry
{
	//! Below this separation, two positions are the same place and there is no direction between them.
	//! Chosen well above float noise at map coordinates (tens of thousands of metres, where a float32
	//! ULP is around 4 mm) and well below any distance an insertion would act on.
	static const float MIN_SEPARATION = 0.05;

	//------------------------------------------------------------------------------------------------
	//! Whether a force should simply walk to its objective rather than be driven there.
	//!
	//! A NON-POSITIVE THRESHOLD MEANS "NEVER WALK BY DISTANCE", which is how a config says "always send
	//! a truck, however short the hop". It is not the same as a threshold of one metre: the distance
	//! could legitimately be zero (a deployment created on top of its own source base), and `0 <= 0`
	//! would then walk when the author asked for a truck.
	//!
	//! ⚠ THIS IS ONE OF SIX WAYS TO END UP WALKING and one of only two that are a decision rather than a
	//! failure. The other decision is the ORIGIN refusing to provide a transport at all
	//! (OVT_DeploymentSourceProvider.SourceProvidesTransport - a forward operating base has no motor
	//! pool), and it is asked separately because it is a property of the place rather than of the
	//! distance. The four failures - a refused reservation, a missing vehicle prefab, a stuck truck and
	//! a destroyed truck - are handled by the caller and reach the same place. Walking is never an
	//! error state.
	//! \param[in] distance How far the source is from the target, in metres.
	//! \param[in] threshold The authored walk threshold. Non-positive disables the rule.
	//! \return True when the force should go on foot.
	static bool ShouldWalk(float distance, float threshold)
	{
		if (threshold <= 0)
			return false;

		// A negative distance cannot come from vector.Distance, but this function takes a bare float
		// and a caller measuring a signed offset would otherwise get "drive" for a target behind it.
		if (distance < 0)
			distance = 0;

		return distance <= threshold;
	}

	//------------------------------------------------------------------------------------------------
	//! The landing zone: a point on the source->target line, `standoff` metres short of the target.
	//!
	//! THREE CLAMPS, AND EVERY ONE OF THEM IS A REAL CASE:
	//!   - STANDOFF >= SEPARATION answers the SOURCE END. A 300 m standoff on a 250 m hop would
	//!     otherwise put the landing zone 50 m BEHIND the source - on the far side of the base the
	//!     truck is leaving - and the convoy would drive away from its objective. The source is the
	//!     honest answer: "there is no room to stand off, so do not go anywhere".
	//!   - A NON-POSITIVE STANDOFF answers the TARGET. That is a config saying "drive them all the way
	//!     in", and it must not be turned into a point past the target by a sign slip.
	//!   - SOURCE == TARGET answers the SOURCE. There is no line to sit on and no direction to measure;
	//!     a normalised zero vector is a NaN, and a NaN landing zone poisons every distance check made
	//!     against it for the rest of the drive.
	//!
	//! THE RESULT IS ALWAYS ON THE CLOSED SEGMENT [source, target] - never before the source, never
	//! past the target. That single property is what makes "short of the target" true whatever anybody
	//! authors, and it is asserted directly rather than inferred.
	//! \param[in] source Where the convoy starts.
	//! \param[in] target Where the force is going.
	//! \param[in] standoff How far short of the target to stop, in metres.
	//! \return A point on the segment; Y is interpolated and the caller is expected to snap it.
	static vector LZPointOnLine(vector source, vector target, float standoff)
	{
		float separation = vector.Distance(source, target);

		if (separation < MIN_SEPARATION)
			return source;

		if (standoff <= 0)
			return target;

		if (standoff >= separation)
			return source;

		vector direction = vector.Direction(target, source).Normalized();

		return target + (direction * standoff);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the convoy is within the landing zone's radius - the PLACE half of arriving, with
	//! nothing said about whether it is still moving.
	//!
	//! A NON-POSITIVE RADIUS IS STILL INSIDE AT ZERO DISTANCE. A config that authors nothing here must
	//! not produce a convoy that drives onto its landing zone and keeps going forever; the comparison
	//! is `<=` so a truck that is exactly there is inside whatever the radius says.
	//!
	//! SPLIT OUT OF HasArrived rather than inlined in it, because THREE different questions want the
	//! place without the stillness: the stall test's arrival exemption (a truck inside the radius is
	//! never stranded, however it is behaving), the settle grace that bounds the wait for it to stop,
	//! and the return leg, where nobody is getting out and a truck rolling through its own base gate
	//! has still got home.
	//! \param[in] distanceToLZ How far the convoy is from the landing zone, in metres.
	//! \param[in] arrivalRadius How close counts as arrived, in metres.
	//! \return True when the convoy is at the landing zone, moving or not.
	static bool IsInsideArrivalRadius(float distanceToLZ, float arrivalRadius)
	{
		if (distanceToLZ < 0)
			distanceToLZ = 0;

		if (arrivalRadius < 0)
			arrivalRadius = 0;

		return distanceToLZ <= arrivalRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the convoy is close enough to the landing zone AND has stopped enough to put its
	//! passengers down.
	//!
	//! ⚠ INSIDE THE RADIUS BUT STILL MOVING IS NOT ARRIVED - IT IS "NOT YET", AND THAT IS THE WHOLE
	//! REASON THIS FUNCTION TAKES A SPEED. Disembarking is a teleport out of a moving vehicle: the
	//! engine puts the passenger beside the truck with the truck's own velocity still on him, and a
	//! transport braking hard into its drop point throws the force it just delivered across the road and
	//! injures it (user play-test, 2026-08-19, after the transport prefab's friction was raised to stop
	//! trucks getting stuck - it stopped them sticking and made them brake much harder). Distance alone
	//! opens the doors on the first tick the truck is inside the radius, which is precisely the tick it
	//! is decelerating hardest. Waiting costs a tick or two; not waiting costs casualties.
	//!
	//! ⚠ THIS FUNCTION CANNOT BE THE ONLY WAY AN INSERTION ENDS, and the caller must not treat it as
	//! one. A truck that reaches the radius and never settles - a slope, a nudge from another vehicle,
	//! an AI driver that keeps creeping, plain physics jitter - would otherwise ride around with its men
	//! aboard forever. The bound is IsSettleGraceExpired(), which the caller ORs with this; read the two
	//! together.
	//!
	//! ⚠ THE SPEED HANDED IN IS NOT SpeedFromTravel'S, AND IT MUST NOT BE. Everything else in this file
	//! that takes a speed is asking whether the convoy is COVERING GROUND, and the tick average is the
	//! right - indeed the only trustworthy - reading for that. This one is asking whether the transport
	//! is about to throw a passenger, and an average over a ~10 s deployment update cannot answer it: the
	//! tick in which a truck brakes from road speed to a standstill still averages several m/s, so a
	//! transport that had genuinely stopped could not be recognised until the tick after the one it
	//! stopped in, and the force sat inside it for twenty seconds (author play-test, 2026-08-19). The
	//! caller therefore reads the vehicle's live velocity for this test and keeps the average for
	//! IsStuck. Both are still just floats here, which is the point of this file - but a caller that
	//! passes the same number to both has reintroduced the defect.
	//!
	//! A NON-POSITIVE SPEED THRESHOLD DISABLES THE SPEED CONDITION and gives back the distance-only test
	//! this function used to be. That is an off-switch, not a defensive branch: it is what a server that
	//! would rather have a rough drop than a slow one authors, and it is what the return leg passes.
	//!
	//! THE COMPARISON IS `<=`, matching the radius, so a threshold of zero would arrive only on a truck
	//! that has not moved at all - except that zero takes the off-switch above first. The two together
	//! mean there is no threshold that can make a stationary truck fail to arrive.
	//! \param[in] distanceToLZ How far the convoy is from the landing zone, in metres.
	//! \param[in] arrivalRadius How close counts as arrived, in metres.
	//! \param[in] speed How fast it is going, in metres per second. Negative reads as stopped.
	//! \param[in] settleSpeedThreshold At or below this it counts as stopped. Non-positive disables the
	//!            speed condition entirely.
	//! \return True when the convoy should drop its passengers.
	static bool HasArrived(float distanceToLZ, float arrivalRadius, float speed, float settleSpeedThreshold)
	{
		if (!IsInsideArrivalRadius(distanceToLZ, arrivalRadius))
			return false;

		if (settleSpeedThreshold <= 0)
			return true;

		if (speed < 0)
			speed = 0;

		return speed <= settleSpeedThreshold;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a convoy that is AT its landing zone has been given long enough to come to a stop.
	//!
	//! ==========================================================================================
	//! THIS IS THE THING THAT MAKES THE SPEED CONDITION IN HasArrived SAFE. Without it, "inside the
	//! radius but still moving means keep watching" is an unbounded wait, and an unbounded wait on a
	//! condition physics may never satisfy is an insertion that never delivers - the worst failure this
	//! feature has, because the men still exist, still cost resources and never appear.
	//! ==========================================================================================
	//!
	//! IT IS NOT A NEW CLOCK. The counter the caller feeds it is ticks INSIDE THE RADIUS, and the budget
	//! it spends is the module's existing stall allowance - the same authored number that says how long
	//! a truck may make no progress before its force walks. One knob, one meaning: "this is how long we
	//! are prepared to watch a transport not do what it should".
	//!
	//! ⚠ EXPIRING MEANS ARRIVED, NOT STRANDED. The truck is ON the landing zone; the force belongs on the
	//! ground there and the transport belongs back home. Handing this to the stall path instead would
	//! dump the men at the right place by the wrong route and abandon a perfectly good truck.
	//!
	//! A NON-POSITIVE BUDGET EXPIRES IMMEDIATELY, which is deliberate and is the same off-switch as
	//! everywhere else in this file: an operator who disabled the stall test gets a convoy that arrives
	//! on distance alone, exactly as it did before the speed condition existed. It can never mean "wait
	//! forever".
	//! \param[in] ticksInsideRadius Consecutive update ticks the convoy has been inside the radius,
	//!            counting the current one. Non-positive is treated as none.
	//! \param[in] graceTicks How many it may spend settling. Non-positive means none at all.
	//! \return True when the convoy should be put down whether or not it has stopped.
	static bool IsSettleGraceExpired(int ticksInsideRadius, int graceTicks)
	{
		if (graceTicks <= 0)
			return true;

		if (ticksInsideRadius < 0)
			ticksInsideRadius = 0;

		return ticksInsideRadius >= graceTicks;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a convoy has stopped making progress and its passengers should get out and walk.
	//!
	//! ⚠ ARRIVAL WINS OVER STUCKNESS, ALWAYS, AND THAT ORDER IS THE WHOLE POINT OF THE distanceToLZ
	//! ARGUMENT. A truck standing still ON its landing zone is not stuck - it is finished - and the two
	//! outcomes are not interchangeable: arriving drops the passengers, hands them to the behaviour
	//! module's plan and sends the truck home, while being stuck dumps them where they stand and
	//! abandons the vehicle. Every convoy that ever succeeds spends at least one tick motionless at its
	//! destination, so without this test every successful insertion would ALSO report as stuck.
	//!
	//! ⚠ THE EXEMPTION IS THE RADIUS, NOT HasArrived, AND IT MUST STAY THAT WAY. Since arriving also
	//! requires the transport to have STOPPED, a speed-aware exemption would let a truck that is inside
	//! the radius and still rolling be called stranded - and it would be called stranded at exactly the
	//! moment it is braking into its own drop point, by exactly the same stationary-for-N-ticks counter
	//! that a settling truck trips. Anything inside the radius is at the landing zone and is the arrival
	//! path's business, however it is behaving; the only two ways out of that radius are arriving now and
	//! arriving when the settle grace expires (see IsSettleGraceExpired).
	//!
	//! A NON-POSITIVE TICK LIMIT DISABLES THE TEST, which is the operator's off-switch for a server
	//! where the road AI is doing something unhelpful; the convoy then only ever ends by arriving or by
	//! losing its truck.
	//!
	//! SPEED IS COMPARED WITH `<`, so a threshold of zero means "only a truck that has not moved AT ALL
	//! counts", which is the strictest reading and the safe one - the cost of calling a moving truck
	//! stuck is a force dumped in open country hundreds of metres from anywhere.
	//! \param[in] speed The convoy's current speed, in metres per second.
	//! \param[in] speedThreshold Below this it is not making progress.
	//! \param[in] ticksBelow How many consecutive ticks it has been below the threshold.
	//! \param[in] ticksLimit How many such ticks are allowed. Non-positive disables the test.
	//! \param[in] distanceToLZ How far it is from the landing zone, in metres.
	//! \param[in] arrivalRadius The arrival radius, in metres.
	//! \return True when the passengers should dismount where they are and walk.
	static bool IsStuck(float speed, float speedThreshold, int ticksBelow, int ticksLimit, float distanceToLZ, float arrivalRadius)
	{
		if (ticksLimit <= 0)
			return false;

		// At the landing zone beats stuck. See the header - this is not an optimisation, and it is
		// deliberately the radius rather than the full arrival test.
		if (IsInsideArrivalRadius(distanceToLZ, arrivalRadius))
			return false;

		if (speed >= speedThreshold)
			return false;

		return ticksBelow >= ticksLimit;
	}

	//------------------------------------------------------------------------------------------------
	//! The stuck counter after one more observation of the convoy's speed.
	//!
	//! CONSECUTIVE, NOT CUMULATIVE. A truck that crawls for a tick, moves for a tick and crawls again
	//! is negotiating a junction, not stuck; only an unbroken run of motionless ticks means the road AI
	//! has genuinely given up. Resetting to zero on any movement is what makes that true, and doing it
	//! here rather than in the caller is what makes it assertable.
	//! \param[in] speed The convoy's current speed, in metres per second.
	//! \param[in] speedThreshold Below this it is not making progress.
	//! \param[in] ticksBelow The counter as it stands.
	//! \return The new counter: one higher, or zero.
	static int AdvanceStuckTicks(float speed, float speedThreshold, int ticksBelow)
	{
		if (speed >= speedThreshold)
			return 0;

		if (ticksBelow < 0)
			return 1;

		return ticksBelow + 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a transport nobody is coming back for should be taken away now.
	//!
	//! ⚠ WHY THERE IS A DEADLINE AT ALL, when the module deliberately does NOT delete a stranded truck at
	//! the moment it strands. That decision - "a stuck one is a landmark and a lootable, and it is
	//! released with everything else when the deployment ends" - is right for an operation that ENDS.
	//! The forward-base deployment does not: it stands for as long as the base does, so its truck was
	//! never collected at all, and a route that reliably strands trucks silts up with them (user
	//! play-test, 2026-08-19). The deadline turns "never" into "eventually" without turning it into
	//! "immediately", which would throw the landmark away.
	//!
	//! ⚠ A PLAYER NEARBY IS AN ABSOLUTE HOLD, NOT A DELAY, and it is checked here rather than folded into
	//! the caller's counter on purpose: a truck must never vanish in front of somebody who is looting it,
	//! fighting over it or walking towards it. The hold does NOT reset the count - the moment the last
	//! player leaves the area, an overdue transport goes - so a player parked beside one does not earn it
	//! a second full lifetime, and one who arrives at the last minute keeps it for as long as he stays.
	//!
	//! A NON-POSITIVE LIMIT DISABLES COLLECTION ENTIRELY, which is the pre-2026-08-19 behaviour written
	//! down as an off-switch rather than left as an accident.
	//! \param[in] ticksSinceAbandoned Update ticks since the transport was left standing.
	//! \param[in] ticksLimit How many are allowed. Non-positive never collects.
	//! \param[in] playerNearby Whether any live player is close enough to notice it go.
	//! \return True when the transport should be deleted, subject to the caller's own ownership vetoes.
	static bool IsAbandonedTruckCollectable(int ticksSinceAbandoned, int ticksLimit, bool playerNearby)
	{
		if (ticksLimit <= 0)
			return false;

		// Before the deadline test, so an overdue truck with somebody standing at it stays overdue rather
		// than being collected the instant the predicate is asked.
		if (playerNearby)
			return false;

		return ticksSinceAbandoned >= ticksLimit;
	}

	//------------------------------------------------------------------------------------------------
	//! A convoy's speed, measured from where it was and where it is.
	//!
	//! ⚠ DERIVED FROM TWO POSITIONS RATHER THAN READ OFF THE PHYSICS, ON PURPOSE. The question the
	//! caller is really asking is "is this vehicle getting closer to anywhere", and a physics velocity
	//! answers a different one: a truck spinning its wheels against a wall, a truck lying on its roof
	//! with a spinning axle, and a truck the AI is rocking back and forth all report motion while
	//! covering no ground. Two origins one update apart cannot be fooled by any of them, and it needs
	//! no engine call, which is also what lets this be tested.
	//!
	//! A NON-POSITIVE ELAPSED TIME ANSWERS ZERO rather than dividing. That is the first observation of
	//! a convoy (there is no previous position yet) and any duplicated update, and answering zero is
	//! safe: it costs one stuck tick out of the limit, and the limit exists to absorb exactly that.
	//! \param[in] previous Where the convoy was at the last observation.
	//! \param[in] current Where it is now.
	//! \param[in] elapsedSeconds How long between the two.
	//! \return Metres per second, never negative.
	static float SpeedFromTravel(vector previous, vector current, float elapsedSeconds)
	{
		if (elapsedSeconds <= 0)
			return 0;

		return vector.Distance(previous, current) / elapsedSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! WHICH AUTHORED VEHICLE SPAWN MARKER A TRANSPORT SHOULD USE: the nearest one to the source that
	//! nothing is standing on.
	//!
	//! ⚠ DELIBERATELY NOT RANDOM, unlike OVT_BaseControllerComponent.GetRandomVehiclePatrolSpawn(). A
	//! patrol picks at random so successive patrols do not all form up in the same corner; an insertion
	//! spawns ONE truck and wants the same answer every time it is asked, so a play-test that saw the
	//! truck in a bad place can be repeated and so a test can assert the choice. Ties (two markers the
	//! same distance from the source, which is what a symmetric authored pair gives you) keep the LOWER
	//! INDEX, i.e. the order the base controller discovered them in - the comparison is strict `<` for
	//! exactly that reason.
	//!
	//! PURE, and that is the point: the caller does the world queries (resolve the markers, ask whether
	//! each is blocked) and hands the decidable part here, where it can be asserted without a world.
	//! \param[in] positions The candidate marker positions, in the base controller's own order.
	//! \param[in] blocked Parallel to positions: true where something is already standing there. An
	//!            index past the end of this array is treated as UNBLOCKED, so a caller that could not
	//!            run the occupancy test still gets a marker rather than nothing.
	//! \param[in] source Where the force is setting out from - what "nearest" is measured against.
	//! \return The index into positions to use, or -1 when there is no usable marker at all (empty
	//!         list, or every one of them blocked), which is the caller's cue to fall back to the road.
	static int ChooseSpawnMarker(notnull array<vector> positions, notnull array<bool> blocked, vector source)
	{
		int best = -1;
		float bestDistance = 0;

		for (int i = 0; i < positions.Count(); i++)
		{
			if (i < blocked.Count() && blocked[i])
				continue;

			float distance = vector.Distance(positions[i], source);

			if (best != -1 && distance >= bestDistance)
				continue;

			best = i;
			bestDistance = distance;
		}

		return best;
	}
}
