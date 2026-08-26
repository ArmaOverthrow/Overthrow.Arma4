//------------------------------------------------------------------------------------------------
//! Insertion geometry - pure, system-free arithmetic about getting a force from A to B.
//!
//! Nothing in this file may touch a system, the game mode or any live state. Every function is
//! total and clamped: no argument combination produces a NaN, an infinity, a point behind the
//! source or a point beyond the target. Y is interpolated; the caller road-snaps or
//! surface-clamps whatever it puts a vehicle or waypoint on.
//------------------------------------------------------------------------------------------------
class OVT_InsertionGeometry
{
	//! Below this separation two positions are the same place and there is no direction between them.
	static const float MIN_SEPARATION = 0.05;

	//------------------------------------------------------------------------------------------------
	//! Whether a force should simply walk to its objective rather than be driven there.
	//! \param[in] distance How far the source is from the target, in metres.
	//! \param[in] threshold The authored walk threshold. Non-positive disables the rule.
	//! \return True when the force should go on foot.
	static bool ShouldWalk(float distance, float threshold)
	{
		if (threshold <= 0)
			return false;

		if (distance < 0)
			distance = 0;

		return distance <= threshold;
	}

	//------------------------------------------------------------------------------------------------
	//! The landing zone: a point on the source->target line, `standoff` metres short of the target.
	//!
	//! The result is always on the closed segment [source, target] - a standoff longer than the hop
	//! returns the source rather than a point behind it.
	//! \param[in] source Where the convoy starts.
	//! \param[in] target Where the force is going.
	//! \param[in] standoff How far short of the target to stop, in metres. Non-positive drives all the way in.
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

	//! The least of the authored standoff a road-snapped landing zone must still keep. Below this the
	//! road is rejected and the geometric point is used instead.
	static const float MIN_RETAINED_STANDOFF = 0.75;

	//------------------------------------------------------------------------------------------------
	//! Whether a road-snapped landing zone is still far enough from the objective to be one.
	//!
	//! 🔴 THE SNAP HAD NO OPINION ABOUT THE TARGET, AND THAT IS THE BUG (author, 2026-08-25: "a
	//! sabotage insertion is driving all the way into a base, its supposed to drop them off a little
	//! away"). ResolveLandingZone() puts a point m_fLZStandoffDistance short of the objective and then
	//! hands it to FindNearestRoadSpawn with a 200 m search - and a base's own access road is a road.
	//! With sabotage's authored 300 m standoff the snap could legally move the drop to 100 m from the
	//! objective, which is inside the wire; the convoy then drove in and parked, because as far as the
	//! module was concerned it had arrived at its landing zone.
	//!
	//! ⚠ THE ROAD IS STILL WANTED. A truck that stops in a field is a truck the dismounts walk further
	//! from and a wreck the next convoy has to path around, so this rejects a bad road rather than
	//! refusing to snap at all.
	//! \param[in] candidate The road position the snap offered.
	//! \param[in] target The objective.
	//! \param[in] standoff The authored standoff. Non-positive accepts anything - a config that asks
	//!            for no standoff is asking to be driven to the door.
	//! \return True when the candidate keeps at least MIN_RETAINED_STANDOFF of the authored distance.
	static bool IsAcceptableLZ(vector candidate, vector target, float standoff)
	{
		if (standoff <= 0)
			return true;

		return vector.Distance(candidate, target) >= standoff * MIN_RETAINED_STANDOFF;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the convoy is within the landing zone's radius - the place half of arriving, with
	//! nothing said about whether it is still moving.
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
	//! ⚠ `speed` must be the vehicle's LIVE velocity, not SpeedFromTravel's tick average - the average
	//! over a ~10 s update still reads several m/s on the tick a truck brakes to a standstill.
	//! Disembarking carries the truck's velocity onto the passenger, so dropping while moving injures
	//! the force. The caller must OR this with IsSettleGraceExpired() or a never-settling truck rides
	//! around with its men forever.
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
	//! Expiring means ARRIVED, not stranded - the truck is on the landing zone. This is the bound that
	//! makes HasArrived's speed condition safe.
	//! \param[in] ticksInsideRadius Consecutive update ticks the convoy has been inside the radius,
	//!            counting the current one. Non-positive is treated as none.
	//! \param[in] graceTicks How many it may spend settling. Non-positive expires immediately.
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
	//! ⚠ The arrival exemption is IsInsideArrivalRadius, NOT HasArrived. A speed-aware exemption would
	//! call a truck stranded at exactly the moment it is braking into its own drop point. Anything
	//! inside the radius belongs to the arrival path however it is behaving.
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

		if (IsInsideArrivalRadius(distanceToLZ, arrivalRadius))
			return false;

		if (speed >= speedThreshold)
			return false;

		return ticksBelow >= ticksLimit;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a transport that has never had a working driver should give up and let its force walk.
	//!
	//! Separate from IsStuck: that asks whether the road AI gave up, which presupposes one. ⚠ Note the
	//! opposite polarity to IsSettleGraceExpired - a non-positive budget disables this test rather
	//! than expiring it, because expiring here would abandon the drive.
	//! \param[in] ticksUncrewed Consecutive update ticks with nobody at the wheel, counting the current
	//!            one. Non-positive is treated as none.
	//! \param[in] ticksLimit How many are allowed. Non-positive disables the test.
	//! \return True when the force should dismount and walk because no driver ever turned up.
	static bool IsUncrewedGraceExpired(int ticksUncrewed, int ticksLimit)
	{
		if (ticksLimit <= 0)
			return false;

		if (ticksUncrewed < 0)
			ticksUncrewed = 0;

		return ticksUncrewed >= ticksLimit;
	}

	//------------------------------------------------------------------------------------------------
	//! The stuck counter after one more observation of the convoy's speed.
	//!
	//! Consecutive, not cumulative - any movement resets it to zero.
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
	//! A nearby player is an absolute hold and does NOT reset the count, so an overdue transport goes
	//! the moment the last player leaves.
	//! \param[in] ticksSinceAbandoned Update ticks since the transport was left standing.
	//! \param[in] ticksLimit How many are allowed. Non-positive never collects.
	//! \param[in] playerNearby Whether any live player is close enough to notice it go.
	//! \return True when the transport should be deleted, subject to the caller's own ownership vetoes.
	static bool IsAbandonedTruckCollectable(int ticksSinceAbandoned, int ticksLimit, bool playerNearby)
	{
		if (ticksLimit <= 0)
			return false;

		if (playerNearby)
			return false;

		return ticksSinceAbandoned >= ticksLimit;
	}

	//------------------------------------------------------------------------------------------------
	//! A convoy's speed, measured from where it was and where it is.
	//!
	//! Derived from two positions rather than the physics velocity: a truck spinning its wheels or
	//! being rocked by the AI reports motion while covering no ground.
	//! \param[in] previous Where the convoy was at the last observation.
	//! \param[in] current Where it is now.
	//! \param[in] elapsedSeconds How long between the two. Non-positive answers zero.
	//! \return Metres per second, never negative.
	static float SpeedFromTravel(vector previous, vector current, float elapsedSeconds)
	{
		if (elapsedSeconds <= 0)
			return 0;

		return vector.Distance(previous, current) / elapsedSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Which authored vehicle spawn marker a transport should use: the nearest unblocked one to the source.
	//!
	//! Deliberately deterministic, unlike GetRandomVehiclePatrolSpawn - ties keep the lower index
	//! (strict `<`) so a play-test is repeatable.
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
