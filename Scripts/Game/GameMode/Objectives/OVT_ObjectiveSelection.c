//------------------------------------------------------------------------------------------------
//! PURE STATICS - which place the occupying faction decides to take back, and how hard it leans on
//! it once it has decided.
//!
//! ===========================================================================================
//! HARD RULE, INHERITED VERBATIM FROM OVT_DeploymentSelection.c:4-6:
//!   NOTHING IN THIS FILE MAY TOUCH A SYSTEM, THE GAME MODE OR ANY LIVE STATE.
//! ===========================================================================================
//!
//! Every function below is a function of its arguments and nothing else. It resolves nothing,
//! queries nothing and reads no clock. Anything the caller already knows - a population, a support
//! percentage, a distance, whether a broadcast still reaches - is passed IN as a number, which is
//! the "already-present set is an argument" rule that made the escalation contract assertable in
//! the cheapest test tier. The caller does the looking up; this file does the arithmetic.
//!
//! PREDICTABILITY IS THE DESIGN CONSTRAINT, NOT AN ACCIDENT. The score is a plain weighted sum of
//! named constants, there is no jitter anywhere in it, and ties break on the caller's input order.
//! An experienced player is supposed to be able to guess the next target; the deployment evaluator's
//! +-20 % jitter exists to spread routine work, but an objective that moves for no reason is exactly
//! what this feature exists to end.
//!
//! THE TWO SCALES ARE DELIBERATELY COMPARABLE. A town saturates at
//! TOWN_POPULATION_WEIGHT + TOWN_SUPPORT_COLLAPSE_WEIGHT + PROXIMITY_WEIGHT + TOWER_COVERAGE_WEIGHT
//! and a base at BASE_PRIZE_WEIGHT + BASE_THREAT_WEIGHT + PROXIMITY_WEIGHT + TOWER_COVERAGE_WEIGHT,
//! and those two totals are equal on purpose: towns and bases compete in ONE sorted list, so if the
//! ceilings drifted apart the loser would simply stop being picked and nobody would notice why.
//------------------------------------------------------------------------------------------------
class OVT_ObjectiveSelection
{
	//! What SelectBestIndex() answers when there is nothing to pick - no candidates at all, or every
	//! one of them sitting out its blacklist round.
	static const int NOTHING_TO_SELECT = -1;

	//! The biggest prize a town can be, at POPULATION_REFERENCE inhabitants or more.
	static const float TOWN_POPULATION_WEIGHT = 40.0;

	//! Population that counts as a full-value prize. Above it the term saturates, so a capital does
	//! not outrank every other consideration put together.
	static const float POPULATION_REFERENCE = 400.0;

	//! What a fully collapsed town (0 % support) is worth on top of its size. This is where the
	//! retired town-suppression trigger went: a support-collapsed town is no longer an instant battle,
	//! it is a HIGH-WEIGHT OBJECTIVE that the ramp then works toward in the open.
	static const float TOWN_SUPPORT_COLLAPSE_WEIGHT = 30.0;

	//! What a base is worth for being a base at all. Bases have no population, so this stands in for
	//! the size term and is set just above a mid-size town: the occupying faction would rather retake
	//! its own barracks than lean on a village.
	static const float BASE_PRIZE_WEIGHT = 45.0;

	//! What the resistance being ACTIVE around a base is worth. Where the fighting already is, is
	//! where the counter-attack goes.
	static const float BASE_THREAT_WEIGHT = 25.0;

	//! Threat that counts as a full-value hotspot. Above it the term saturates.
	static const float THREAT_REFERENCE = 40.0;

	//! What being right next door to a place the occupying faction still holds is worth. Falls off
	//! linearly to zero at the caller's maximum useful distance - past that, a supply line is not
	//! worth running and the term contributes nothing rather than going negative.
	static const float PROXIMITY_WEIGHT = 25.0;

	//! What still being able to broadcast over the objective is worth. Small on purpose: it breaks
	//! ties between otherwise comparable targets rather than deciding the campaign.
	static const float TOWER_COVERAGE_WEIGHT = 10.0;

	//------------------------------------------------------------------------------------------------
	//! Clamps a fraction into [0, 1].
	//! \param[in] value The fraction to clamp.
	//! \return The value, never below 0 and never above 1.
	static float Clamp01(float value)
	{
		if (value < 0)
			return 0;

		if (value > 1)
			return 1;

		return value;
	}

	//------------------------------------------------------------------------------------------------
	//! How reachable a candidate is, as a fraction of the caller's maximum useful distance.
	//!
	//! LINEAR, AND ZERO AT OR PAST THE LIMIT rather than negative. A distant candidate must be worth
	//! LESS than a close one, never worth less than nothing - a negative term would let a far-away
	//! target drop below a candidate that scored zero on every other input, which inverts the sort in
	//! exactly the case (a quiet map with one distant prize) where the machine most needs to pick
	//! something.
	//! \param[in] distance How far the candidate is from the nearest place the faction still holds.
	//! \param[in] maxUsefulDistance Distance at which the term reaches zero. Zero or negative disables
	//! the term entirely, which is the honest answer when the faction holds nothing to measure from.
	//! \return A fraction in [0, 1].
	static float ProximityScore(float distance, float maxUsefulDistance)
	{
		if (maxUsefulDistance <= 0)
			return 0;

		if (distance >= maxUsefulDistance)
			return 0;

		float clamped = distance;
		if (clamped < 0)
			clamped = 0;

		return 1.0 - (clamped / maxUsefulDistance);
	}

	//------------------------------------------------------------------------------------------------
	//! What a resistance-held town or city is worth as the next objective.
	//!
	//! ⚠ TAKES NUMBERS, NEVER A TOWN RECORD. The caller has already resolved every input; see the
	//! header for why that is the rule rather than a preference.
	//! \param[in] population Inhabitants. Saturates at POPULATION_REFERENCE; negative reads as none.
	//! \param[in] supportPercentage Support for the resistance, 0-100. Values outside that band are
	//! clamped, because a percentage that is not one is a caller bug and must not swing the sort.
	//! \param[in] distanceToNearestHeldBase Distance to the nearest base the faction still holds.
	//! \param[in] maxUsefulDistance Distance at which the proximity term reaches zero.
	//! \param[in] hasTowerCoverage Whether an occupying broadcast still reaches the town.
	//! \return A score on the shared 0..105 scale. Never negative.
	static float ScoreTown(int population, int supportPercentage, float distanceToNearestHeldBase, float maxUsefulDistance, bool hasTowerCoverage)
	{
		float sizeTerm = TOWN_POPULATION_WEIGHT * Clamp01(population / POPULATION_REFERENCE);

		// COLLAPSED SUPPORT IS THE HIGH-VALUE SIGNAL, so the term is inverted: 0 % support pays the
		// full weight, 100 % pays nothing.
		float collapseTerm = TOWN_SUPPORT_COLLAPSE_WEIGHT * (1.0 - Clamp01(supportPercentage / 100.0));

		float reachTerm = PROXIMITY_WEIGHT * ProximityScore(distanceToNearestHeldBase, maxUsefulDistance);

		float coverageTerm = 0;
		if (hasTowerCoverage)
			coverageTerm = TOWER_COVERAGE_WEIGHT;

		return sizeTerm + collapseTerm + reachTerm + coverageTerm;
	}

	//------------------------------------------------------------------------------------------------
	//! What a resistance-held base is worth as the next objective.
	//!
	//! ⚠ TAKES NUMBERS, NEVER A BASE RECORD - same rule as ScoreTown.
	//! \param[in] threat Campaign threat measured at the base. Saturates at THREAT_REFERENCE; negative
	//! reads as none.
	//! \param[in] distanceToNearestHeldBase Distance to the nearest base the faction still holds.
	//! \param[in] maxUsefulDistance Distance at which the proximity term reaches zero.
	//! \param[in] hasTowerCoverage Whether an occupying broadcast still reaches the base.
	//! \return A score on the shared 0..105 scale. Never negative.
	static float ScoreBase(int threat, float distanceToNearestHeldBase, float maxUsefulDistance, bool hasTowerCoverage)
	{
		float threatTerm = BASE_THREAT_WEIGHT * Clamp01(threat / THREAT_REFERENCE);

		float reachTerm = PROXIMITY_WEIGHT * ProximityScore(distanceToNearestHeldBase, maxUsefulDistance);

		float coverageTerm = 0;
		if (hasTowerCoverage)
			coverageTerm = TOWER_COVERAGE_WEIGHT;

		return BASE_PRIZE_WEIGHT + threatTerm + reachTerm + coverageTerm;
	}

	//------------------------------------------------------------------------------------------------
	//! The index of the candidate to commit to, out of parallel score/blacklist lists.
	//!
	//! HIGHEST SCORE WINS, and a strictly greater-than comparison is what keeps a tie on the FIRST
	//! entry - so two equally attractive candidates are taken in the order the caller supplied them
	//! (registry order, then discovery order), deterministically, run after run.
	//!
	//! RAGGED INPUT IS REFUSED OUTRIGHT rather than clamped to the shorter list, exactly as
	//! OVT_DeploymentSelection.SelectNextConfigIndex() refuses it. The two lists are built side by side
	//! by one caller; a mismatch is a programming error, and picking through a mis-aligned mask could
	//! commit to the very candidate that was supposed to be sitting out.
	//! \param[in] scores Every candidate's score, in the caller's order.
	//! \param[in] blacklisted Whether each candidate is sitting out this round. May be null, meaning
	//! none of them are.
	//! \return An index into scores, or NOTHING_TO_SELECT.
	static int SelectBestIndex(array<float> scores, array<bool> blacklisted)
	{
		if (!scores || scores.IsEmpty())
			return NOTHING_TO_SELECT;

		if (blacklisted && blacklisted.Count() != scores.Count())
			return NOTHING_TO_SELECT;

		int best = NOTHING_TO_SELECT;
		float bestScore = 0;

		int count = scores.Count();
		for (int i = 0; i < count; i++)
		{
			if (blacklisted && blacklisted[i])
				continue;

			// Strictly greater-than: the first candidate at a given score keeps the win, so ties
			// resolve to the caller's order.
			if (best == NOTHING_TO_SELECT || scores[i] > bestScore)
			{
				best = i;
				bestScore = scores[i];
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! The blacklist key for a position.
	//!
	//! ROUNDED TO WHOLE METRES ON THE HORIZONTAL PLANE, and height is dropped. The key has to survive
	//! a save and come back matching, and a stored position is not guaranteed to round-trip to the
	//! last bit; metre granularity is far finer than the spacing between any two candidates and far
	//! coarser than any drift a codec can introduce. Height is dropped because a candidate's recorded
	//! altitude is whatever the terrain was when it was discovered.
	//! \param[in] position The candidate's position.
	//! \return A stable key.
	static string PositionKey(vector position)
	{
		int x = (int)Math.Round(position[0]);
		int z = (int)Math.Round(position[2]);

		return x.ToString() + "," + z.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a candidate is sitting out its blacklist round.
	//!
	//! AN EXHAUSTED ENTRY IS NOT BLACKLISTED. Rounds at or below zero read as expired, so the caller
	//! may prune lazily (or not at all) without the answer changing - which is what makes "decay, then
	//! ask" and "ask, then prune, then decay" agree.
	//!
	//! ⚠ RAGGED OR ABSENT INPUT ANSWERS "NOT BLACKLISTED", which is the OPPOSITE of
	//! SelectBestIndex()'s refusal, and deliberately so: the two failure modes are not symmetric. A
	//! broken score/mask pairing that picks anyway could commit to the wrong target; a broken blacklist
	//! that answers "no" merely re-picks a target that failed once, and the phase machine handles that
	//! by failing it again. Answering "yes" on ragged input would blacklist the entire map and idle the
	//! occupying faction forever, with no log line to explain it.
	//! \param[in] keys Blacklisted keys. May be null.
	//! \param[in] roundsLeft Rounds remaining per key, same order, same length. May be null.
	//! \param[in] key The candidate's key.
	//! \return True only when that exact key is present with rounds still to serve.
	static bool IsBlacklisted(array<string> keys, array<int> roundsLeft, string key)
	{
		if (!keys || !roundsLeft)
			return false;

		if (keys.Count() != roundsLeft.Count())
			return false;

		if (key == "")
			return false;

		int count = keys.Count();
		for (int i = 0; i < count; i++)
		{
			if (keys[i] != key)
				continue;

			if (roundsLeft[i] > 0)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Serves one round of every blacklist entry.
	//!
	//! FLOORED AT ZERO, NEVER NEGATIVE. An entry that has served its time sits at exactly zero and
	//! stays there, so a caller that decays more often than it selects cannot drive an entry so far
	//! below zero that a later increment fails to re-blacklist it.
	//! \param[inout] roundsLeft Rounds remaining per entry. May be null, which is a no-op.
	static void DecayBlacklist(inout array<int> roundsLeft)
	{
		if (!roundsLeft)
			return;

		int count = roundsLeft.Count();
		for (int i = 0; i < count; i++)
		{
			if (roundsLeft[i] > 0)
				roundsLeft[i] = roundsLeft[i] - 1;
			else
				roundsLeft[i] = 0;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Which structure a sabotage mission demolishes next: THE CHEAPEST ONE STILL STANDING.
	//!
	//! ⚠ COST IS THE ONLY ORDERING KEY THAT EXISTS, AND THAT IS A DECISION RATHER THAN A COMPROMISE.
	//! There is no size, no footprint and no importance attribute anywhere on a buildable or a
	//! placeable - price is the game's own statement of how much a structure is worth, and a saboteur
	//! working outward from the sandbags to the garage is both the cheapest thing to model and the most
	//! legible thing to watch. The shipped prices make the intended order come out on their own:
	//! bunkers 750, tents 1000, guard tower 1200, ramp/helipad 1500, fuel depot 2000, garage 8000.
	//!
	//! ⚠ THE MIRROR OF SelectBestIndex, DOWNWARD. Same shape, same ragged-input refusal, same
	//! first-wins tie rule - so ties resolve to the caller's discovery order and repeated calls over an
	//! unchanged list walk the list in a stable order rather than oscillating between two equal prices.
	//! \param[in] costs One authored cost per candidate structure.
	//! \param[in] alreadyDestroyed Parallel flags. May be null, meaning nothing has been destroyed yet;
	//!            a length mismatch is refused outright rather than half-honoured.
	//! \return The index of the cheapest candidate not yet destroyed, or NOTHING_TO_SELECT.
	static int NextTargetIndex(array<int> costs, array<bool> alreadyDestroyed)
	{
		if (!costs || costs.IsEmpty())
			return NOTHING_TO_SELECT;

		if (alreadyDestroyed && alreadyDestroyed.Count() != costs.Count())
			return NOTHING_TO_SELECT;

		int best = NOTHING_TO_SELECT;
		int bestCost = 0;

		int count = costs.Count();
		for (int i = 0; i < count; i++)
		{
			if (alreadyDestroyed && alreadyDestroyed[i])
				continue;

			// Strictly less-than, and the first surviving candidate seeds the comparison rather than a
			// literal - which is what lets a NEGATIVE cost (a mod authoring one, a corrupt config) sort
			// first instead of being skipped by a zero seed.
			if (best == NOTHING_TO_SELECT || costs[i] < bestCost)
			{
				best = i;
				bestCost = costs[i];
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Folds an objective bias into a candidate's evaluation score. (Authored here in Phase 2; the one
	//! call site lands in the deployment evaluator in Phase 3.)
	//!
	//! TWO INVARIANTS, AND BOTH ARE LOAD-BEARING:
	//!   1. NO ANCHOR IS BYTE-IDENTICAL TO TODAY. A non-positive radius, a non-positive weight, or a
	//!      candidate at or beyond the radius all return the score UNCHANGED. Every campaign with no
	//!      objective, every faction with no director and every test world that never starts one
	//!      therefore sorts exactly as it did before this feature existed.
	//!   2. THE BIAS IS BOUNDED BY weight. The most an anchor can ever add is its full weight, at
	//!      distance zero, falling off linearly to nothing at the radius. It can never subtract. So
	//!      "objective-adjacent work is bought first, but only up to weight" is arithmetic rather than
	//!      an impression - a candidate whose base score already beats score + weight still wins.
	//! \param[in] score The candidate's score before the bias.
	//! \param[in] distanceToAnchor How far the candidate is from the objective.
	//! \param[in] radius The anchor's radius. Non-positive means no anchor.
	//! \param[in] weight The most the anchor may add. Non-positive means no anchor.
	//! \return The biased score, never below the input and never above score + weight.
	static float ApplyAnchorBias(float score, float distanceToAnchor, float radius, float weight)
	{
		if (radius <= 0)
			return score;

		if (weight <= 0)
			return score;

		if (distanceToAnchor >= radius)
			return score;

		float distance = distanceToAnchor;
		if (distance < 0)
			distance = 0;

		return score + (weight * (1.0 - (distance / radius)));
	}
}
