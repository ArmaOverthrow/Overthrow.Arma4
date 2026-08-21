//------------------------------------------------------------------------------------------------
//! THE MODDER SEAM FOR TARGET. Answers "out of everything on the map, what is THIS plan worth
//! attacking?" for OVT_ObjectiveConfig.
//!
//! Shaped on OVT_DeploymentSourceProvider and OVT_ObjectiveTargetResolver, deliberately: three seams
//! in one epic that answer a "where" question should not be three different shapes. A plan authors
//! exactly ONE of these, and a doctrine that wants to attack something the campaign has never
//! attacked before is a new selector plus a .conf - no change to the director, no change to any other
//! plan.
//!
//! THE CONTRACT, and every implementation must hold to all five points:
//!   1. IT SCORES, IT DOES NOT PICK. ScoreCandidates() fills a score per candidate and returns
//!      whether it claimed any of them. Which candidate wins, whether the blacklist excludes it, and
//!      how the plan's priority multiplier ranks it against a rival plan are all the runner's, in one
//!      place, through the pure statics.
//!   2. ONE SCORE PER CANDIDATE, ALWAYS, INCLUDING THE ONES IT DOES NOT WANT. The arrays are
//!      parallel and the statics refuse ragged input outright. A candidate whose kind this selector
//!      does not claim is written as zero and EXCLUDED BY THE MASK, never by the score - zero is a
//!      legal score, and a selector that tried to say "not mine" with a number would be able to win
//!      a candidate it has no doctrine for. See OVT_ObjectiveCandidateSet.BuildSelectionMask().
//!   3. DECLARE EVERY SOURCE IT READS, IN GetCandidateSources(). The runner collects the UNION of
//!      every eligible plan's sources exactly once per round, so a source that is scored but not
//!      declared is a source that is never collected and therefore never scored - silently. The
//!      registry's validator refuses a selector that declares none at all.
//!   4. ARITHMETIC ONLY - NO WORLD, NO MANAGERS, NO CLOCK. Every number a selector needs is already
//!      on the candidate set, resolved once. This is what makes N plans cost one pass over the world
//!      (D6) and it is why the shipped selectors can be exercised on a hand-built set with no
//!      campaign behind them at all.
//!   5. NO JITTER, EVER. Predictability is a stated design constraint of this feature: an
//!      experienced player is supposed to be able to guess the next target from the map. There is no
//!      randomness anywhere in the selection path, ties break on the candidate set's own order, and a
//!      selector that rolled a die would make the whole ramp unreadable.
//!
//! ⚠ THE BASE ANSWERS "NOTHING" ON PURPOSE. A plan whose m_Selector is unauthored, or authored as
//! the base class, claims no sources and scores nothing - and the validator names it and skips the
//! plan rather than letting it sit in the registry competing for objectives it can never describe.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_ObjectiveTargetSelector
{
	//------------------------------------------------------------------------------------------------
	//! Which candidate sources this selector can score.
	//! \return OVT_EObjectiveCandidateSource flags. Zero means "nothing", which the validator rejects.
	int GetCandidateSources()
	{
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! What every candidate in the round is worth to this plan.
	//! \param[in] candidates The round's shared candidate set. Read-only to a selector.
	//! \param[out] scores Receives one score per candidate, in the set's order. Cleared first, and
	//!             always filled to the set's length - a candidate this selector does not claim gets a
	//!             zero and is excluded by the mask, not by its score.
	//! \return True when this selector claimed at least one candidate.
	bool ScoreCandidates(notnull OVT_ObjectiveCandidateSet candidates, notnull array<float> scores)
	{
		scores.Clear();

		int count = candidates.Count();
		for (int i = 0; i < count; i++)
		{
			scores.Insert(0);
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Human-readable name for warnings, selection logs and the registry's validator.
	//! \return The selector's name.
	string GetSelectorName()
	{
		return "none";
	}

	//------------------------------------------------------------------------------------------------
	//! The maximum useful distance this selector's proximity term falls off over.
	//!
	//! ⚠ THE SENTINEL IS THE SHIPPED VALUE. A non-positive authored value means "use the director's own
	//! setting", which is what every shipped plan authors, so the proximity term is identical to the
	//! single-list machine's by default and a server owner who wants one doctrine to reach further
	//! overrides one number.
	//! \param[in] authored The selector's own attribute.
	//! \param[in] candidates The round's set, which carries the director's setting.
	//! \return The distance to use, in metres.
	protected float ResolveMaxUsefulDistance(float authored, notnull OVT_ObjectiveCandidateSet candidates)
	{
		if (authored > 0)
			return authored;

		return candidates.GetMaxUsefulDistance();
	}
}
