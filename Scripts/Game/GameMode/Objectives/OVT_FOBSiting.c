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

	//! One turn, for the one place a negative angle has to be brought back into range.
	static const float FULL_CIRCLE = 360;

	//! What FacingYaw() answers when there is no direction to face - an UNROTATED structure.
	//!
	//! ⚠ A DEFINED ANSWER, NOT A SENTINEL, for the reason OVT_QRFBearing.DEFAULT_DEGREES records:
	//! atan2(0, 0) is not specified to be anything, and a yaw that is not a number goes straight into a
	//! spawn transform, where every basis vector becomes NaN and the structure is placed nowhere at all.
	//! Zero is the value the forward base had before it was ever turned, so the degenerate case is
	//! exactly the old behaviour rather than a new failure.
	static const float NO_FACING = 0;

	//------------------------------------------------------------------------------------------------
	//! WHICH WAY A STRUCTURE STANDING AT `from` HAS TO BE TURNED TO LOOK AT `to`.
	//!
	//! ⚠ THE ANSWER IS AN ENTITY YAW IN THE Math3D.AnglesToMatrix FRAME - the (yaw, pitch, roll) vector
	//! OVT_BaseSpawningDeploymentModule.GetUprightSpawnRotation() builds - AND NOT A COMPASS BEARING.
	//! The two differ by the axis they measure from and by their sign, and OVT_QRFBearing next door
	//! answers the other one: it exists to lay a wave's landing zone out on a SIDE of a town and works
	//! in the 0 = North = -Z convention, so borrowing it here would turn every forward base a quarter
	//! circle and nothing would say so. Grounded rather than assumed: AnglesToMatrix's own documented
	//! example (Core/generated/Math/Math3D.c) shows yaw 70, pitch 15 producing a forward row of
	//! <0.9077, 0.2588, 0.3304> = (sin y cos p, sin p, cos y cos p), so an entity at yaw t faces
	//! (sin t, 0, cos t) and the yaw that faces a delta is atan2(dx, dz).
	//!
	//! WHAT "FACE" MEANS FOR THE THING BEING TURNED: the entity's own +Z, which is the axis
	//! OVT_FOBPosition's Workbench arrow draws (transform[2]) and the axis the shipped forward-base
	//! prefab puts its hedgehogs and barbed wire on, with the tent and the stores behind at -Z. So a
	//! base turned by this function has its wire towards whatever it was pointed at.
	//!
	//! HEIGHT IS DISCARDED. A structure's heading is a flat rotation; the objective being 200 m up a
	//! hill must not tip it.
	//!
	//! PURE, TOTAL AND DETERMINISTIC, like everything else in this file - which is what lets the
	//! generated path pick a facing at all. The old behaviour was an identity rotation on every
	//! unauthored forward base in the campaign, and "the prefab happens to be unrotated" is not a
	//! decision anybody made.
	//! \param[in] from Where the structure stands.
	//! \param[in] to What it should look at.
	//! \return A yaw in [0, 360) for GetUprightSpawnRotation(); NO_FACING when the two coincide.
	static float FacingYaw(vector from, vector to)
	{
		float dx = to[0] - from[0];
		float dz = to[2] - from[2];

		// Before the transcendental, never after it. See NO_FACING.
		if (dx == 0 && dz == 0)
			return NO_FACING;

		float degrees = Math.Atan2(dx, dz) * Math.RAD2DEG;

		// atan2 answers (-180, 180], so ONE addition is enough and the range is closed by construction -
		// no loop, nothing to run away. Wrapped rather than left signed only so a log line and a test row
		// read the same way round as a compass does.
		if (degrees < 0)
			degrees = degrees + FULL_CIRCLE;

		return degrees;
	}

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
	//! Whether a candidate stands in the SUPPLY CORRIDOR: the band, on the supply side, and not too far
	//! off the line.
	//!
	//! ==========================================================================================
	//! 🔴 WHY THIS EXISTS AT ALL: IsInBand() IS A RING, AND A RING HAS NO SIDE. The band is a distance
	//! from the objective and nothing else, so it is satisfied just as well by a point BEHIND the
	//! objective - on the far side from every base the faction owns - as by one between the objective
	//! and its supply base. The generated lattice never noticed, because it builds its candidates as
	//! `objective - along * standoff` and is therefore on the supply side by construction. The AUTHORED
	//! path had no such protection: it queried a sphere around the objective and band-checked what came
	//! back, so a marker anywhere in the ring could win on terrain score alone.
	//!
	//! A play-test found it exactly that way (2026-08-20): a marker 968 m from the objective, inside an
	//! 847-1816 m band, but on the OPPOSITE SIDE from the only nearby base the occupying faction held.
	//! The forward base was raised behind its own objective and supplied from a base 1.2 km further
	//! away than the one the director had recorded as its source. Author: *"authored markers should use
	//! the same corridor math used for non-authored markers. I assumed they did."*
	//! ==========================================================================================
	//!
	//! IT IS THE LATTICE'S OWN SHAPE, WRITTEN DOWN. The generated sampler walks `standoff` across the
	//! band and `lateral` across +/-spread; this asks whether an arbitrary point falls in the region
	//! those two ranges sweep. So an authored marker is admitted exactly when the sampler could have
	//! produced it, which is what "the same corridor math" means and why the bound is the same constant
	//! rather than a second, looser one.
	//!
	//! ⚠ THE ALONG-AXIS DISTANCE IS NEGATED, and getting that backwards is the whole failure this
	//! prevents. `along` runs FROM the source TO the objective, and a valid candidate is BEHIND the
	//! objective along that axis - the sampler's own `objective - along * standoff`. So the projection
	//! of (candidate - objective) onto `along` is NEGATIVE for a good site, and its magnitude is the
	//! standoff. A sign slip here re-admits precisely the far-side markers this was written to reject.
	//!
	//! ⚠ DEGENERATE GEOMETRY ACCEPTS NOTHING, the same rule and for the same reason as IsInBand: a
	//! source on top of the objective has no direction to measure a corridor along, and "put it
	//! anywhere" is the one answer that must never be inferred from "I cannot tell".
	//! \param[in] candidate The position being judged.
	//! \param[in] objective Where the objective is.
	//! \param[in] source The base the forward base would be supplied from.
	//! \param[in] bandMin Nearest it may stand to the objective.
	//! \param[in] bandMax Furthest it may stand from the objective.
	//! \param[in] lateralSpread How far off the supply line it may sit, in metres. Non-positive means
	//!            the line itself, which no real point satisfies - callers pass the sampler's spread.
	//! \return True when the candidate is inside the corridor.
	static bool IsInSupplyCorridor(vector candidate, vector objective, vector source, float bandMin, float bandMax, float lateralSpread)
	{
		if (bandMin >= bandMax)
			return false;

		vector toObjective = objective - source;
		toObjective[1] = 0;

		float separation = toObjective.Length();
		if (separation <= 0)
			return false;

		vector along = toObjective.Normalized();
		vector lateral = Vector(-along[2], 0, along[0]);

		vector rel = candidate - objective;
		rel[1] = 0;

		// NEGATED: see the header. A good candidate sits behind the objective along the supply axis.
		float alongDistance = -(rel[0] * along[0] + rel[2] * along[2]);

		if (!IsInBand(alongDistance, bandMin, bandMax))
			return false;

		float lateralDistance = Math.AbsFloat(rel[0] * lateral[0] + rel[2] * lateral[2]);

		return lateralDistance <= lateralSpread;
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
		// the two ends of the band once per lattice point and nothing in between - with no symptom beyond
		// forward bases always landing at exactly one of two distances.
		float position = clamped;
		float span = steps - 1;

		return position / span;
	}

	//------------------------------------------------------------------------------------------------
	//! How far to push one attempt off the supply line, as a FRACTION of the caller's lateral spread.
	//!
	//! ON THE LINE FIRST, THEN OUTWARD IN ALTERNATING PAIRS. A forward base sitting exactly on the road
	//! between two places is the first thing a player drives into; the lateral offsets are what let the
	//! sampler find the field behind the treeline instead. The zero offset is lane 0 so a band with clear
	//! ground straight down the middle uses it and the base ends up where the supply run is shortest.
	//!
	//! 🔴 THE RETURN IS A FRACTION IN [-1, +1] AND THE CALLER'S SPREAD IS A MAXIMUM, NOT A STEP. This
	//! used to return the raw ring index - 0, -1, +1, -2, +2 - which is the same thing only while there
	//! are exactly three lanes. It was written when there were, and going to five (user, 2026-08-19)
	//! would have produced offsets of 0, +-spread, +-2*spread: a corridor SILENTLY TWICE AS WIDE as the
	//! "400 m either side" that was asked for, with forward bases 800 m off the supply line and nothing
	//! anywhere saying so. The lanes are now distributed EVENLY across the full span, so five lanes at a
	//! 400 m spread are 0, +-200, +-400 and the outermost lane is the spread exactly - whatever the lane
	//! count becomes.
	//!
	//! ⚠ THE INVARIANT A FUTURE LANE CHANGE MUST NOT BREAK: |LateralOffset(lane, lanes)| <= 1 for every
	//! lane and every lane count, and == 1 for the outermost lane of any lattice with more than one.
	//! Both halves are asserted in OVT_TEST_Logic_ObjectiveScaling's lattice case, precisely because
	//! widening the corridor by accident produces no error, no warning and no visible symptom beyond
	//! forward bases turning up somewhere nobody agreed to.
	//!
	//! AN EVEN LANE COUNT IS LOPSIDED, NOT WRONG. Four lanes give 0, -0.5, +0.5, -1: the sequence pairs
	//! low-to-high and simply runs out mid-pair, so one side gets the extra ring. Still bounded by the
	//! spread, which is the property that matters; the shipped count is odd.
	//! \param[in] lane Which lateral lane, 0-based. Clamped into the lattice.
	//! \param[in] lanes How many lanes the lattice has.
	//! \return A signed fraction of the caller's maximum spread, in [-1, +1].
	static float LateralOffset(int lane, int lanes)
	{
		if (lanes <= 1 || lane <= 0)
			return 0;

		int clamped = lane;
		if (clamped > lanes - 1)
			clamped = lanes - 1;

		// Which ring out from the line this lane sits on: lanes 1 and 2 are the first ring either side,
		// 3 and 4 the second, and so on.
		int ring = (clamped + 1) / 2;

		// The outermost ring this lattice can produce, which is what the fraction is measured against.
		// It is what lane == lanes - 1 evaluates to above, written directly so the normalisation cannot
		// drift away from the sequence it normalises.
		int outermost = lanes / 2;
		if (outermost < 1)
			return 0;

		// ⚠ BOTH OPERANDS MADE FLOATS EXPLICITLY, for the reason BandFraction() records: an int/int
		// division truncates inside a pure-int expression, and every inner ring would collapse to 0 -
		// putting five lanes back on three positions with no symptom at all.
		float position = ring;
		float span = outermost;

		float fraction = position / span;

		if (clamped % 2 == 1)
			return -fraction;

		return fraction;
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
