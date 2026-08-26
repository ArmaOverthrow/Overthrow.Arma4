//------------------------------------------------------------------------------------------------
//! Civilian ambience - PURE arithmetic, and nothing else.
//!
//! ===========================================================================================
//! HARD RULE: NOTHING IN THIS FILE MAY REACH OUTSIDE ITSELF. No static accessor, no component
//! lookup, no entity, no scene, no loaded state of any kind - every method here is a pure
//! function of its arguments and can be called before anything at all has been loaded.
//! ===========================================================================================
//!
//! Every decision about "how many civilians, of which kind" that is expressible as a pure function
//! of its inputs lives here, so the cheapest test tier can assert it. The callers are then thin:
//! read the live numbers, call one of these, act on the answer.
//!
//! That tier greps its own directory - comments included - for the identifiers that reach live
//! systems, which is exactly why the density formula is a static here rather than a method on a
//! config class that would need one of those identifiers to reach its inputs.
//!
//! The only engine symbol referenced is Math, which needs nothing loaded.
//------------------------------------------------------------------------------------------------
class OVT_CivilianAmbienceMath
{
	//------------------------------------------------------------------------------------------------
	//! How many ambient civilians one town should be carrying right now.
	//!
	//! The five steps, in order, and each one is a decision somebody could get wrong:
	//!  1. `population <= 0` -> 0. A depopulated town has NO crowd, and the minimum clamp must not
	//!     resurrect one. This is why the population test comes first and not inside the clamp.
	//!  2. `rate * multiplier <= 0` -> 0. A multiplier of 0 is the documented "turn civilians off"
	//!     switch for a server operator, so it has to survive the clamp too.
	//!  3. `raw = Round(population * rate * multiplier)`. Rounded, not truncated: a village of 27 at
	//!     rate 0.1 is 3 people, not 2.
	//!  4. Clamp into [minCount, maxCount] - the source's own floor and ceiling, so a rate change can
	//!     never produce a crowd nobody authored for. The ceiling is applied last, so a mis-authored
	//!     pair with max below min resolves to max rather than to a crowd bigger than the ceiling.
	//!  5. `hardCap > 0` -> min(count, hardCap). `hardCap <= 0` means NO CAP - deliberately, so an
	//!     operator config file written before the cap existed (absent key, so 0 after a raw read)
	//!     can never silently zero a server's civilians.
	//!
	//! Parity with the hand-rolled spawner this replaces: 283 population at rate 0.1 -> 28,
	//! 38 -> 4, 27 -> 3.
	//! \param[in] population The town's LIVE population. Never a cached registration-time copy.
	//! \param[in] rate Civilians per head of population (authored, e.g. 0.1).
	//! \param[in] multiplier Operator density multiplier; 0 turns the crowd off entirely.
	//! \param[in] minCount Floor applied when the town has any population at all.
	//! \param[in] maxCount Ceiling applied to the rolled figure.
	//! \param[in] hardCap Absolute per-town ceiling; 0 or below means uncapped.
	//! \return The number of civilians to spawn, never negative.
	static int ResolveTownCivilianCount(int population, float rate, float multiplier, int minCount, int maxCount, int hardCap)
	{
		// Step 1 - an empty town beats every clamp below.
		if (population <= 0)
			return 0;

		// Step 2 - "off" beats every clamp below too. A negative rate or multiplier is nonsense and is
		// read as off rather than being allowed to produce a negative raw figure.
		float density = rate * multiplier;
		if (density <= 0)
			return 0;

		// Step 3 - float maths deliberately: population is an int but rate and multiplier are not, and
		// an int-only expression here would truncate every fractional civilian away.
		int count = Math.Round(population * density);

		// Step 4 - floor then ceiling.
		if (count < minCount)
			count = minCount;

		if (count > maxCount)
			count = maxCount;

		// Step 5 - the absolute cap, opt-in.
		if (hardCap > 0 && count > hardCap)
			count = hardCap;

		// A mis-authored negative clamp may not turn into a negative count.
		if (count < 0)
			count = 0;

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! May a civilian type appear in a settlement of this size?
	//!
	//! Two independent filters, both of which must pass:
	//!  - MINIMUM SIZE. `townSize >= typeMinSize`, on the size ordinals (village 1, town 2, city 3,
	//!    capital 4 - larger is bigger). This is what keeps a city-only look out of a hamlet without
	//!    anybody authoring a per-town list.
	//!  - EXPLICIT ALLOW-LIST. When a settlement authors a list of type names, ONLY those names are
	//!    allowed. An absent list (null) or an EMPTY one is "not authored" and allows everything the
	//!    size filter allowed - an empty list must never mean "no civilians here", because that is
	//!    exactly what every un-authored settlement carries.
	//!
	//! Name matching is EXACT and case-sensitive, the same rule the source registry's own name lookup
	//! uses; a name that never matches produces an empty allowed set loudly rather than a subtly
	//! different crowd.
	//! \param[in] typeMinSize Smallest settlement size ordinal this type may appear in.
	//! \param[in] townSize This settlement's size ordinal.
	//! \param[in] allowedNames Explicitly allowed type names, or null/empty for "no restriction".
	//! \param[in] typeName The candidate type's name.
	//! \return True when the type may be rolled here.
	static bool TypeAllowed(int typeMinSize, int townSize, array<string> allowedNames, string typeName)
	{
		if (townSize < typeMinSize)
			return false;

		if (!allowedNames || allowedNames.IsEmpty())
			return true;

		foreach (string allowed : allowedNames)
		{
			if (allowed == typeName)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Picks an index from a weight table, given an already-drawn roll.
	//!
	//! The roll is an ARGUMENT, not a draw made in here, so this stays deterministic and assertable in
	//! the cheapest tier - the caller draws once (SumWeights() gives it the exclusive upper bound it
	//! needs) and this turns that number into an index.
	//!
	//! WEIGHT SEMANTICS. A weight of 0 or below means "never picked" and is skipped rather than
	//! treated as 1: a modder disabling one entry by zeroing its weight must not get it back.
	//!
	//! OUT-OF-RANGE ROLLS ARE DETERMINISTIC, NOT AN ERROR (a caller may have drawn against a stale
	//! total after a config reload):
	//!  - roll below 0 -> the FIRST entry with a positive weight;
	//!  - roll at or above the total -> the LAST entry with a positive weight;
	//!  - no entries, or no positive weight anywhere -> -1, which the caller must treat as "nothing to
	//!    roll" rather than as index 0.
	//! \param[in] weights One weight per candidate, parallel to the caller's own candidate list.
	//! \param[in] roll A number in [0, SumWeights(weights)); out-of-range values clamp as above.
	//! \return The chosen index, or -1 when nothing is eligible.
	static int PickWeightedIndex(array<int> weights, int roll)
	{
		int total = SumWeights(weights);
		if (total <= 0)
			return -1;

		int clamped = roll;
		if (clamped < 0)
			clamped = 0;

		if (clamped >= total)
			clamped = total - 1;

		int cursor = 0;
		for (int i = 0; i < weights.Count(); i++)
		{
			int weight = weights[i];
			if (weight <= 0)
				continue;

			cursor += weight;
			if (clamped < cursor)
				return i;
		}

		// Unreachable while `clamped` is inside [0, total): the accumulator ends at exactly `total`.
		// Kept as a named answer rather than a fallthrough, so a future edit that breaks the invariant
		// produces "nothing eligible" instead of index 0.
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! The exclusive upper bound a caller should roll against before calling PickWeightedIndex().
	//!
	//! Exists because a weighted pick is unusable without it: a caller that drew against anything else
	//! would silently stop honouring the weights.
	//! \param[in] weights One weight per candidate.
	//! \return The sum of the POSITIVE weights; 0 when nothing is eligible.
	static int SumWeights(array<int> weights)
	{
		if (!weights)
			return 0;

		int total = 0;
		for (int i = 0; i < weights.Count(); i++)
		{
			if (weights[i] > 0)
				total += weights[i];
		}

		return total;
	}
}
