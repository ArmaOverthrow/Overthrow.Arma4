//------------------------------------------------------------------------------------------------
//! PURE STATICS - where the occupying faction's forward operating base is allowed to stand, and how
//! good a candidate spot is. NO WORLD, NO MANAGER, NO ENTITY.
//!
//! THE HARD RULE, inherited from OVT_ObjectivePhaseRules and OVT_DeploymentSelection: every function
//! here is a function of its arguments and nothing else. It never resolves a manager, never reads a
//! config, never traces a box and never asks the terrain how high it is. The director does all the
//! looking - the ocean read, the clearance trace, the four surface probes, the road search - and hands
//! the ANSWERS in as numbers. That is what makes siting assertable in the cheapest test tier, which
//! matters more here than anywhere else in the feature: the generated path is the PRIMARY path and no
//! authored marker exists in any world today (R17), so nothing else will notice if it silently starts
//! rejecting everything.
//!
//! ⚠ EVERY PREDICATE FAILS CLOSED. A degenerate band, a ragged exclusion list and a candidate sitting
//! exactly on an exclusion boundary are all refusals, not passes. A refusal costs one blacklisted
//! objective and a WARNING line naming the attempt count; a wrong pass puts an enemy forward base
//! inside a town the resistance holds, on top of a player's own camp, or halfway up a cliff, where it
//! is visible to everyone and fixable by nobody.
//------------------------------------------------------------------------------------------------
class OVT_FOBSiting
{
	//! Weight of the flatness term in ScoreSite(). THE LARGEST, because it is the only one whose
	//! failure is visible as a broken-looking world: a structure on a slope floats at one corner and
	//! sinks at another, and no amount of tactical merit makes up for that.
	static const float FLATNESS_WEIGHT = 3;

	//! Weight of the elevation term. A forward base that looks down on its objective is the tactical
	//! preference, but it is only a preference - flat ground on the level beats a good view on a slope.
	static const float ELEVATION_WEIGHT = 2;

	//! Weight of the near-a-road term. THE SMALLEST, and deliberately not zero: the supply truck that
	//! raises the base has to reach it, and the ones that follow have to keep reaching it, but the
	//! insertion module already snaps its own landing zone to a road and walks the last leg, so being
	//! off-road costs convenience rather than correctness.
	static const float ROAD_WEIGHT = 1;

	//! Distance in metres at which the near-a-road term reaches zero. Matches the insertion module's
	//! own road search bound (OVT_WorldUtils.ROAD_SPAWN_MAX_DISTANCE, 200 m) so "too far from a road to
	//! score" and "too far from a road for a truck to be put on one" are the same distance rather than
	//! two numbers that can drift apart.
	static const float ROAD_USEFUL_DISTANCE = 200;

	//! Returned by ScoreSite() and its three terms when an argument is unusable. Zero rather than a
	//! sentinel: a site that scores nothing is still a site, and the caller's job is to prefer the best
	//! it found, not to distinguish "bad" from "broken".
	static const float NO_SCORE = 0;

	//------------------------------------------------------------------------------------------------
	//! Whether a candidate sits in the band between the supplying base and the objective.
	//!
	//! INCLUSIVE AT BOTH ENDS, because the band is authored as "somewhere between these two distances"
	//! and a candidate landing exactly on a bound is inside what was asked for.
	//!
	//! ⚠ A DEGENERATE BAND (min >= max) ACCEPTS NOTHING. It means the caller's geometry collapsed - the
	//! supplying base and the objective are the same place, or the fractions were misauthored - and the
	//! honest answer is "there is nowhere to put it", which the caller turns into a blacklist and a log
	//! line. Accepting everything instead would put the forward base on top of the objective it is
	//! supposed to be advancing on, which is the one placement the whole middle phase exists to avoid.
	//! \param[in] distanceToObjective How far the candidate is from the objective, in metres.
	//! \param[in] min Nearest the forward base may stand to the objective.
	//! \param[in] max Furthest it may stand from the objective.
	//! \return True when the candidate is inside the band.
	static bool IsInBand(float distanceToObjective, float min, float max)
	{
		if (min >= max)
			return false;

		if (distanceToObjective < min)
			return false;

		return distanceToObjective <= max;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a candidate is clear of every place a forward base may not be built.
	//!
	//! THE TWO LISTS ARE PARALLEL AND ARE MATCHED BY INDEX: exclusions[i] is a position and radii[i] is
	//! how far from it nothing may be built. They are two arrays rather than one record list for the
	//! same reason the save payload's blacklist is: primitive arrays are what every tier round-trips
	//! without a class, and the pairing is re-associated by index at the one place that reads it.
	//!
	//! ⚠ STRICTLY OUTSIDE. A candidate exactly ON an exclusion radius is NOT clear. The radii are the
	//! campaign's own "too close" distances, and a forward base standing precisely on the line of one
	//! is a forward base standing at the edge of a town, a base or a player's camp.
	//!
	//! ⚠ A RAGGED PAIR IS A REFUSAL, NOT AN APPROXIMATION. Different counts mean the caller lost track
	//! of which radius belongs to which place, and there is no safe way to guess: taking the shorter
	//! count silently stops excluding whatever fell off the end. Nothing in the game would show it.
	//!
	//! A NON-POSITIVE RADIUS EXCLUDES NOTHING, so a caller can leave a place in the list with its
	//! exclusion disabled rather than rebuilding the arrays.
	//! \param[in] candidate The position being judged.
	//! \param[in] exclusions Places nothing may be built near. Null or empty means nothing to avoid.
	//! \param[in] radii How far from each of them, same order, same length.
	//! \return True when the candidate is clear of all of them.
	static bool IsClearOfExclusions(vector candidate, array<vector> exclusions, array<float> radii)
	{
		if (!exclusions || !radii)
			return !exclusions && !radii;

		if (exclusions.Count() != radii.Count())
			return false;

		int count = exclusions.Count();
		for (int i = 0; i < count; i++)
		{
			if (radii[i] <= 0)
				continue;

			if (vector.Distance(candidate, exclusions[i]) <= radii[i])
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! How good a candidate is, once it has already passed every hard test.
	//!
	//! A PLAIN WEIGHTED SUM WITH NO RANDOMNESS, exactly like objective selection: the site an objective
	//! gets should be the best one the sampler found, every time, so that a bad placement is a tuning
	//! question with a reproducible answer rather than a roll nobody can repeat.
	//!
	//! ⚠ THE FIRST TWO ARGUMENTS ARE ALREADY-NORMALISED FRACTIONS and the third is METRES. That is not
	//! an oversight: flatness and elevation both need world knowledge the caller has and this class
	//! must not (a probe tolerance, a useful height gain), so they arrive as 0-1 through FlatnessScore()
	//! and ElevationScore(); distance to a road is a plain length with one meaningful scale, so it is
	//! normalised here against ROAD_USEFUL_DISTANCE and the caller does not get to disagree about it.
	//! \param[in] flatness 0-1, 1 being perfectly level. From FlatnessScore().
	//! \param[in] elevation 0-1, 1 being the full useful height advantage. From ElevationScore().
	//! \param[in] distanceToRoad Metres to the nearest road. Negative means none was found.
	//! \return A score in [0, FLATNESS_WEIGHT + ELEVATION_WEIGHT + ROAD_WEIGHT].
	static float ScoreSite(float flatness, float elevation, float distanceToRoad)
	{
		float score = Clamp01(flatness) * FLATNESS_WEIGHT;
		score = score + Clamp01(elevation) * ELEVATION_WEIGHT;
		score = score + RoadScore(distanceToRoad) * ROAD_WEIGHT;

		return score;
	}

	//------------------------------------------------------------------------------------------------
	//! How level a candidate is, from the spread of the caller's height probes.
	//!
	//! ONE MINUS THE FRACTION OF THE TOLERANCE USED UP, floored at zero. A spread of nothing is a
	//! perfect 1; a spread at or past the tolerance is 0, and the caller is expected to have REJECTED
	//! it outright before ever asking for a score - this term exists to order the sites that passed,
	//! not to admit the ones that did not.
	//! \param[in] heightSpread Metres between the highest and lowest probe. Negative reads as zero.
	//! \param[in] tolerance The spread at which the term reaches zero. Non-positive disables the term,
	//!            which scores every candidate as perfectly flat rather than dividing by zero.
	//! \return 0-1.
	static float FlatnessScore(float heightSpread, float tolerance)
	{
		if (tolerance <= 0)
			return 1;

		if (heightSpread <= 0)
			return 1;

		if (heightSpread >= tolerance)
			return NO_SCORE;

		return 1 - (heightSpread / tolerance);
	}

	//------------------------------------------------------------------------------------------------
	//! How much of a height advantage a candidate has over its objective.
	//!
	//! ONLY UPWARDS COUNTS. A forward base BELOW its objective is not penalised, it simply scores
	//! nothing for elevation - the flatness and road terms still order it against its peers. Penalising
	//! it would make a valley the last resort even when it is the only flat ground in the band.
	//! \param[in] heightAboveObjective Metres the candidate stands above the objective. Negative or
	//!            zero scores nothing.
	//! \param[in] usefulGain The height at which the term saturates. Non-positive disables the term.
	//! \return 0-1.
	static float ElevationScore(float heightAboveObjective, float usefulGain)
	{
		if (usefulGain <= 0)
			return NO_SCORE;

		if (heightAboveObjective <= 0)
			return NO_SCORE;

		if (heightAboveObjective >= usefulGain)
			return 1;

		return heightAboveObjective / usefulGain;
	}

	//------------------------------------------------------------------------------------------------
	//! How well served by roads a candidate is.
	//!
	//! ⚠ A NEGATIVE DISTANCE MEANS "NO ROAD WAS FOUND", NOT "ON A ROAD". The caller's road search is
	//! bounded, so the natural way to say "the search came up empty" is a negative, and reading that as
	//! zero distance would score the most isolated candidate in the band as the best served.
	//! \param[in] distanceToRoad Metres to the nearest road, or negative when none was found.
	//! \return 0-1.
	static float RoadScore(float distanceToRoad)
	{
		if (distanceToRoad < 0)
			return NO_SCORE;

		if (distanceToRoad >= ROAD_USEFUL_DISTANCE)
			return NO_SCORE;

		return 1 - (distanceToRoad / ROAD_USEFUL_DISTANCE);
	}

	//------------------------------------------------------------------------------------------------
	//! Where along the supply line to sample, as a fraction of the way from the source to the
	//! objective, for one attempt out of a deterministic lattice.
	//!
	//! ⚠ DETERMINISTIC, NOT RANDOM, AND THAT IS A DESIGN CONSTRAINT RATHER THAN A CONVENIENCE. This
	//! feature exists so the resistance can READ the occupying faction's intent; a forward base that
	//! appears in a different place each time the same campaign reaches the same state is exactly the
	//! unpredictability being retired. It also means a bad site is reproducible, and that "the sampler
	//! found nothing" can be reasoned about from the band alone.
	//!
	//! The lattice walks the band from the SOURCE end towards the objective, so an early hit is a
	//! forward base close to its own supply line - which is the shorter, safer resupply and the one the
	//! starvation rule is kindest to.
	//! \param[in] step Which step along the band, 0-based.
	//! \param[in] steps How many steps the lattice has. 1 or fewer samples the near end only.
	//! \return A fraction in [0, 1].
	static float BandFraction(int step, int steps)
	{
		if (steps <= 1)
			return 0;

		int clamped = step;
		if (clamped < 0)
			clamped = 0;

		if (clamped > steps - 1)
			clamped = steps - 1;

		// ⚠ BOTH OPERANDS ARE MADE FLOATS EXPLICITLY. An int/int division in this language truncates
		// inside a pure-int expression, and a lattice whose every step evaluated to 0 or 1 would sample
		// the two ends of the band twenty-four times and nothing in between - with no symptom beyond
		// forward bases always landing at exactly one of two distances.
		float position = clamped;
		float span = steps - 1;

		return position / span;
	}

	//------------------------------------------------------------------------------------------------
	//! How far to push one attempt off the supply line, in multiples of the caller's lateral spread.
	//!
	//! THREE OFFSETS PER STEP: on the line, and one spread to each side. A forward base sitting exactly
	//! on the road between two places is the first thing a player drives into; the lateral offsets are
	//! what let the sampler find the field behind the treeline instead. The zero offset is FIRST so a
	//! band with clear ground straight down the middle uses it and the base ends up where the supply
	//! run is shortest.
	//! \param[in] lane Which lateral lane, 0-based.
	//! \param[in] lanes How many lanes the lattice has.
	//! \return A signed multiplier: 0, then -1, +1, -2, +2 and so on.
	static float LateralOffset(int lane, int lanes)
	{
		if (lanes <= 1 || lane <= 0)
			return 0;

		int clamped = lane;
		if (clamped > lanes - 1)
			clamped = lanes - 1;

		int magnitude = (clamped + 1) / 2;

		if (clamped % 2 == 1)
			return -magnitude;

		return magnitude;
	}

	//------------------------------------------------------------------------------------------------
	//! Clamps a fraction into 0-1.
	//! \param[in] value The fraction.
	//! \return The clamped value.
	static float Clamp01(float value)
	{
		if (value <= 0)
			return 0;

		if (value >= 1)
			return 1;

		return value;
	}
}
