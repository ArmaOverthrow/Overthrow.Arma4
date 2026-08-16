//------------------------------------------------------------------------------------------------
//! Virtualization core - PURE, WORLD-FREE arithmetic.
//!
//! ===========================================================================================
//! HARD RULE: NOTHING IN THIS FILE MAY TOUCH A MANAGER, THE GAME MODE, THE WORLD OR AN ENTITY.
//! ===========================================================================================
//!
//! Every decision the virtualization manager makes that is expressible as a pure function of its
//! inputs lives here, so the Fast Logic tier (which loads no world at all) can assert it. The
//! manager is then a thin caller: resolve inputs, call one of these, stamp the result.
//!
//! The only engine symbols referenced are the enum SCR_EAISpawnImportance (an int constant set),
//! Math, and the global s_AIRandomGenerator - none of which need a world.
//------------------------------------------------------------------------------------------------
class OVT_VirtualizationMath
{
	//! Despawn distance = spawn distance x this, unless a caller supplies its own hysteresis.
	//! Mirrors the shape of vanilla's own 600/800 default pair (SCR_AIGroup.c:124-126).
	static const float DEFAULT_DESPAWN_HYSTERESIS = 1.15;

	//! Overthrow policy default when a consumer does not choose a tier (D4). Vanilla's own default
	//! is LOW (0.50 budget cap, evicted first), which starves campaign hostiles.
	static const int DEFAULT_IMPORTANCE = SCR_EAISpawnImportance.NORMAL;

	//------------------------------------------------------------------------------------------------
	//! Resolves the spawn distance for one registration.
	//!
	//! Any NEGATIVE override means "use the server's configured global" (-1 is the documented
	//! sentinel; other negatives are treated the same way rather than producing a nonsense ring).
	//! 0 and huge values are BOTH legitimate: 0 means "never materialise by proximity" (an economy
	//! agent that should stay virtual), a huge value means "always spawned" - issue #100's ask.
	//! \param[in] distanceOverride Per-registration override, negative for "use the global".
	//! \param[in] globalDefault The configured virtualizationSpawnDistance.
	//! \return The distance to stamp on the group's lifecycle policy, never negative.
	static int ResolveSpawnDistance(int distanceOverride, int globalDefault)
	{
		if (distanceOverride < 0)
		{
			if (globalDefault < 0)
				return 0;

			return globalDefault;
		}

		return distanceOverride;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the despawn distance from an already-resolved spawn distance.
	//!
	//! The gap between the two IS the engine's anti-thrash band (SCR_AIGroup.c:3025-3027), so the
	//! despawn distance may never be smaller than the spawn distance - a hysteresis below 1 is
	//! clamped rather than honoured. A spawn distance of 0 ("never materialise") stays 0: widening
	//! it would hand the group a despawn ring it can never be inside of.
	//! \param[in] spawnDistance Resolved spawn distance.
	//! \param[in] hysteresis Multiplier, clamped up to 1.0.
	//! \return The despawn distance, always >= spawnDistance.
	static int ResolveDespawnDistance(int spawnDistance, float hysteresis)
	{
		if (spawnDistance <= 0)
			return 0;

		float factor = hysteresis;
		if (factor < 1.0)
			factor = 1.0;

		int despawn = Math.Round(spawnDistance * factor);
		if (despawn < spawnDistance)
			despawn = spawnDistance;

		return despawn;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the SCR_EAISpawnImportance tier for one registration (D4).
	//!
	//! Anything outside the enum's range - including the documented -1 "unset" sentinel - falls back
	//! to the configured default; a configured default that is itself out of range falls back to
	//! NORMAL, so a registration can never end up at vanilla's budget-starved LOW by accident.
	//! \param[in] requested Tier asked for by the consumer, or -1 for "Overthrow's default".
	//! \param[in] configuredDefault The manager's configured default tier.
	//! \return A valid SCR_EAISpawnImportance value.
	static int ResolveImportance(int requested, int configuredDefault)
	{
		if (IsValidImportance(requested))
			return requested;

		if (IsValidImportance(configuredDefault))
			return configuredDefault;

		return DEFAULT_IMPORTANCE;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] importance Candidate tier value.
	//! \return True when the value is one of LOW / NORMAL / HIGH / CRITICAL.
	static bool IsValidImportance(int importance)
	{
		return importance >= SCR_EAISpawnImportance.LOW && importance <= SCR_EAISpawnImportance.CRITICAL;
	}

	//------------------------------------------------------------------------------------------------
	//! Parallel-array integrity check for a waypoint plan.
	//!
	//! An EMPTY plan is valid (a group with no waypoints is a legitimate garrison); a RAGGED plan is
	//! not, because the arrays are consumed by index and a short m_aParams would silently give the
	//! last waypoint someone else's radius. A type outside the enum is refused for the same reason:
	//! it would otherwise fall through to whatever the builder's default branch happens to be.
	//! \param[in] positions Waypoint positions.
	//! \param[in] types OVT_EVirtualWaypointType per position.
	//! \param[in] waypointParams Per-type parameter per position.
	//! \return True when the plan can be built exactly as written.
	static bool ValidateWaypointPlan(array<vector> positions, array<int> types, array<float> waypointParams)
	{
		if (!positions || !types || !waypointParams)
			return false;

		if (types.Count() != positions.Count())
			return false;

		if (waypointParams.Count() != positions.Count())
			return false;

		for (int i = 0; i < types.Count(); i++)
		{
			if (types[i] < OVT_EVirtualWaypointType.MOVE || types[i] > OVT_EVirtualWaypointType.CYCLE)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Counts the living slots in a survivor mask (D2). Any non-zero entry is alive.
	//! \param[in] mask One entry per roster slot.
	//! \return Number of living slots; 0 for a null or empty mask.
	static int CountAlive(array<int> mask)
	{
		if (!mask)
			return 0;

		int alive = 0;
		for (int i = 0; i < mask.Count(); i++)
		{
			if (mask[i] != 0)
				alive++;
		}

		return alive;
	}

	//------------------------------------------------------------------------------------------------
	//! Wipe test over a survivor mask.
	//!
	//! A null or EMPTY mask is deliberately NOT a wipe: an empty mask means "roster unknown"
	//! (a record whose group entity has not been built yet), and reporting that as a wipe would
	//! delete records that never had a member in the first place.
	//! \param[in] mask One entry per roster slot.
	//! \return True when the mask has slots and none of them is alive.
	static bool IsWiped(array<int> mask)
	{
		if (!mask || mask.IsEmpty())
			return false;

		return CountAlive(mask) == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The D2 refill selector: which roster slot should the next queued member spawn as?
	//!
	//! Vanilla always picks slotIndex == current agent count (SCR_AIGroup.c:2731), a first-N refill
	//! that structurally destroys identity - a group that lost its slot-1 machinegunner comes back
	//! with the MG alive and a tail rifleman missing instead. Core's ExpandOneMember override
	//! (Phase 3, T3.3) calls this instead: the first slot that is alive in the mask and has not
	//! already been materialised this activation.
	//! \param[in] mask Survivor mask, one entry per roster slot.
	//! \param[in] spawnedSlots Slot indices already materialised (may be null or empty).
	//! \return The slot index to spawn, or -1 when the group is fully materialised / wiped.
	static int NextSlotToSpawn(array<int> mask, array<int> spawnedSlots)
	{
		if (!mask)
			return -1;

		for (int i = 0; i < mask.Count(); i++)
		{
			if (mask[i] == 0)
				continue;

			if (spawnedSlots && spawnedSlots.Contains(i))
				continue;

			return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Round-robin slice for the ambient tick (Phase 4): the indices to evaluate this tick.
	//!
	//! Wraps around the end of the collection, and never returns the same index twice in one slice
	//! (a slice larger than the collection is clamped to the whole collection) - so a source cannot
	//! be evaluated twice in one tick and spawn its roll twice.
	//! \param[in] count Number of sources.
	//! \param[in] sliceSize How many to evaluate this tick.
	//! \param[in] cursor Where the last tick stopped.
	//! \return The indices to evaluate, in evaluation order; empty when there is nothing to do.
	static array<int> SliceIndices(int count, int sliceSize, int cursor)
	{
		array<int> indices = new array<int>();

		if (count <= 0 || sliceSize <= 0)
			return indices;

		int take = sliceSize;
		if (take > count)
			take = count;

		int start = cursor;
		if (start < 0)
			start = 0;

		start = start % count;

		for (int i = 0; i < take; i++)
		{
			indices.Insert((start + i) % count);
		}

		return indices;
	}

	//------------------------------------------------------------------------------------------------
	//! Advances the ambient round-robin cursor past the slice SliceIndices() just returned.
	//! \param[in] cursor Current cursor.
	//! \param[in] sliceSize The slice size used.
	//! \param[in] count Number of sources.
	//! \return The next cursor, always within [0, count); 0 when there is nothing to iterate.
	static int AdvanceCursor(int cursor, int sliceSize, int count)
	{
		if (count <= 0)
			return 0;

		int step = sliceSize;
		if (step < 0)
			step = 0;

		int start = cursor;
		if (start < 0)
			start = 0;

		return (start + step) % count;
	}

	//------------------------------------------------------------------------------------------------
	//! Inclusive random count, guarded against two RandInt traps.
	//!
	//! ⚠ RandInt is MAX-EXCLUSIVE, and RandInt(n, n) raises an ENGINE ERROR rather than returning n.
	//! Every "roll between min and max inclusive" in this feature goes through here: the equality
	//! case short-circuits, and the general case adds the +1 the exclusive bound needs. An inverted
	//! pair is swapped rather than refused, because a mis-authored config should still spawn.
	//! \param[in] min Lowest allowed value.
	//! \param[in] max Highest allowed value (INCLUSIVE).
	//! \return A value in [min, max].
	static int RollCountSafe(int min, int max)
	{
		int lo = min;
		int hi = max;

		if (hi < lo)
		{
			lo = max;
			hi = min;
		}

		if (lo == hi)
			return lo;

		return s_AIRandomGenerator.RandInt(lo, hi + 1);
	}
}
