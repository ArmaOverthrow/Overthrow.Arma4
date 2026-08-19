//------------------------------------------------------------------------------------------------
//! Insertion geometry - PURE, SYSTEM-FREE arithmetic about getting a force from A to B.
//!
//! ===========================================================================================
//! HARD RULE: NOTHING IN THIS FILE MAY TOUCH A SYSTEM, THE GAME MODE OR ANY LIVE STATE.
//! ===========================================================================================
//!
//! Four questions live here, and they are the four an insertion keeps asking:
//!
//!     is this hop short enough to walk?                             ShouldWalk
//!     where, short of the target, do we put them down?              LZPointOnLine
//!     are we there?                                                 HasArrived
//!     has the truck stopped being a truck?                          IsStuck
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
	//! ⚠ THIS IS ONE OF FIVE WAYS TO END UP WALKING and the only one that is a decision rather than a
	//! failure. The other four - a refused reservation, a missing vehicle prefab, a stuck truck and a
	//! destroyed truck - are handled by the caller and reach the same place. Walking is never an error
	//! state.
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
	//! Whether the convoy is close enough to the landing zone to put its passengers down.
	//!
	//! A NON-POSITIVE RADIUS STILL ARRIVES AT ZERO DISTANCE. A config that authors nothing here must
	//! not produce a convoy that drives onto its landing zone and keeps going forever; the comparison
	//! is `<=` so a truck that is exactly there has arrived whatever the radius says.
	//! \param[in] distanceToLZ How far the convoy is from the landing zone, in metres.
	//! \param[in] arrivalRadius How close counts as arrived, in metres.
	//! \return True when the convoy should drop its passengers.
	static bool HasArrived(float distanceToLZ, float arrivalRadius)
	{
		if (distanceToLZ < 0)
			distanceToLZ = 0;

		if (arrivalRadius < 0)
			arrivalRadius = 0;

		return distanceToLZ <= arrivalRadius;
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

		// Finished beats stuck. See the header - this is not an optimisation.
		if (HasArrived(distanceToLZ, arrivalRadius))
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
