//------------------------------------------------------------------------------------------------
//! PURE STATICS - the objective PLAN arithmetic. Nothing here resolves a manager, reads a config or
//! looks at a clock.
//!
//! THE HARD RULE, inherited verbatim from OVT_ObjectivePhaseRules.c:4-8 and from
//! OVT_ObjectiveSelection.c:4-6: every function here is a function of its arguments and nothing
//! else. Anything the caller already knows is passed IN as a number or a string. That is what makes
//! the plan machine assertable in the cheapest test tier, which is the entire point of splitting it
//! out - see OVT_TEST_Logic_ObjectivePlanRules.c.
//!
//! ⚠ THE TIER RULE IS ENFORCED BY A DIRECTORY-WIDE GREP THAT DOES NOT DISTINGUISH CODE FROM PROSE.
//! The banned identifiers may not appear anywhere in this file, comments included, so this header is
//! worded around them deliberately. The same trap has been tripped twice in this tree by a header
//! that quoted its own rule.
//!
//! WHAT LIVES HERE. The objective framework (occupying/objectives Phase 2) moved doctrine out of the
//! director and into authored data. Four questions came with it and all four are arithmetic:
//!   1. AN AUTHORED ATTRIBUTE OR THE DIFFICULTY SETTING? Every module attribute that has a difficulty
//!      twin defaults to the USE_DIFFICULTY sentinel. ResolveWithDifficulty() is the one place that
//!      decides, so no module re-derives it and no module looks a field name up by string.
//!   2. WHICH PLAN WINS? ResolvePlanScore() and SelectBestPlanIndex(), with the priority MULTIPLIER
//!      convention stated once (higher wins - the opposite of a deployment config's int priority, and
//!      that difference is deliberate. See D8).
//!   3. HAS THE PHASE'S CONDITION SET PASSED, OR HAS ANY ABORT FIRED? AllConditionsMet() is an AND
//!      over the phase's condition modules and AnyAbort() is an OR over its abort modules. The empty
//!      answers are opposite and both are load-bearing - see each method.
//!   4. WHERE IS A NAMED PHASE IN A PLAN? PhaseIndexOf(), which is how a persisted phase NAME becomes
//!      a runtime index and how a deployment-side condition will name a phase span from Phase 4
//!      onward. An unknown name answers NO_PHASE_INDEX rather than 0, because 0 is a real phase.
//------------------------------------------------------------------------------------------------
class OVT_ObjectivePlanRules
{
	//! What a module attribute holds when it means "use the campaign's difficulty setting instead of
	//! me". ⚠ IT IS A FLOOR, NOT A MAGIC NUMBER: any negative authored value is treated as the
	//! sentinel, because -1 typed as -2 is a mis-key and never an intent.
	static const int USE_DIFFICULTY = -1;

	//! What ResolveWithDifficulty() answers when neither the authored value nor the difficulty value is
	//! usable. Zero, not the sentinel: the caller is asking for a NUMBER, and handing back -1 would let
	//! a sentinel escape into a countdown, a radius or a cap where it would read as "forever".
	static const int NO_VALUE = 0;

	//! Answered by SelectBestPlanIndex() when nothing was eligible. Deliberately NOT 0 - zero is a real
	//! plan index, and handing a caller "plan 0" of an empty or wholly ineligible registry would commit
	//! an objective nobody selected. Matches OVT_ObjectiveSelection.NOTHING_TO_SELECT's reasoning.
	static const int NOTHING_TO_SELECT = -1;

	//! Answered by PhaseIndexOf() for a name no plan carries. Not 0, for the same reason.
	static const int NO_PHASE_INDEX = -1;

	//! The multiplier a plan that authors nothing behaves as. 1 = "score me on my selector alone".
	static const float DEFAULT_PRIORITY = 1;

	//------------------------------------------------------------------------------------------------
	//! An authored attribute, or the campaign's difficulty setting when the attribute is the sentinel.
	//!
	//! ⚠ THE SENTINEL IS THE DEFAULT ON EVERY ATTRIBUTE THAT HAS A DIFFICULTY TWIN, so the shipped
	//! configs author -1 and behave exactly as the hard-coded machine did, and a server owner who wants
	//! one plan to differ overrides one number. The mapping from "which difficulty field" to "which
	//! attribute" is made by the CALLER in code (it passes the field in), never by a string lookup of a
	//! field name here - a string lookup is a rename away from silently reading zero.
	//!
	//! ⚠ IT DOES NOT CLAMP, AND THAT IS DELIBERATE. An absurd authored value is honoured verbatim,
	//! because "absurd" is domain knowledge this function does not have: 20 000 is nonsense for a
	//! concurrency cap and perfectly sane for an anchor radius in metres. Where a range genuinely
	//! exists it is clamped at the point that knows it - RequiredSabotageMissions() next door is the
	//! model - and never here, where a shared clamp would be wrong for every second caller.
	//! \param[in] authored The module attribute. Negative (USE_DIFFICULTY) defers to the difficulty value.
	//! \param[in] difficultyValue The campaign's setting for the twin field.
	//! \return The authored value when it is authored, the difficulty value when it is usable, else NO_VALUE.
	static int ResolveWithDifficulty(int authored, int difficultyValue)
	{
		if (authored > USE_DIFFICULTY)
			return authored;

		if (difficultyValue < 0)
			return NO_VALUE;

		return difficultyValue;
	}

	//------------------------------------------------------------------------------------------------
	//! A plan's rank: what its selector scored, scaled by what the plan is worth.
	//!
	//! ⚠ HIGHER WINS, AND THE FIELD IS A FLOAT CALLED m_fPriority BECAUSE A DEPLOYMENT CONFIG'S
	//! m_iPriority IS AN INT WHERE LOWER WINS (D8). Two opposite conventions in one tree is a real
	//! cost; it is paid because silently inverting a convention under the same field name produces a
	//! plan that never runs and a report nobody can reproduce. The type and the prefix make the
	//! difference visible in the .conf itself.
	//!
	//! ⚠ A MULTIPLIER OF ZERO EXCLUDES A PLAN OUTRIGHT, and that is a supported authoring gesture -
	//! "ship this doctrine but do not run it" - not an accident to be defended against. A NEGATIVE
	//! multiplier is a mis-key rather than an intent, so it is floored at zero here as well as
	//! rejected by the registry's validator; without the floor a plan could out-rank every other by
	//! scoring a large negative selector score.
	//! \param[in] selectorScore What the plan's selector scored its best candidate. Negative is clamped to 0.
	//! \param[in] priorityMultiplier The plan's m_fPriority. Negative is clamped to 0.
	//! \return The plan's comparable rank. Never negative.
	static float ResolvePlanScore(float selectorScore, float priorityMultiplier)
	{
		float score = selectorScore;
		if (score < 0)
			score = 0;

		float priority = priorityMultiplier;
		if (priority < 0)
			priority = 0;

		return score * priority;
	}

	//------------------------------------------------------------------------------------------------
	//! Which plan to commit to: the highest score among the eligible ones.
	//!
	//! ⚠ TIES GO TO THE EARLIER ENTRY, WHICH MEANS THE AUTHORED ORDER OF THE REGISTRY IS THE
	//! TIE-BREAK. Predictability is a stated requirement of this feature and there is no jitter
	//! anywhere in the selection path; a strict `>` is what makes the authored order the answer rather
	//! than whatever the comparison happened to do. This mirrors OVT_ObjectiveSelection.SelectBestIndex().
	//!
	//! ⚠ THE TWO ARRAYS ARE PARALLEL AND A LENGTH MISMATCH SELECTS NOTHING, rather than reading past
	//! the end of the shorter one. A caller that built them separately and got them out of step has a
	//! fault this function cannot fix and must not hide.
	//! \param[in] scores One rank per plan, from ResolvePlanScore().
	//! \param[in] eligible One flag per plan: faction, instance cap and chance roll already applied.
	//! \return The winning index, or NOTHING_TO_SELECT when nothing was eligible.
	static int SelectBestPlanIndex(array<float> scores, array<bool> eligible)
	{
		if (!scores || !eligible)
			return NOTHING_TO_SELECT;

		int count = scores.Count();
		if (count == 0 || eligible.Count() != count)
			return NOTHING_TO_SELECT;

		int best = NOTHING_TO_SELECT;
		float bestScore = 0;

		for (int i = 0; i < count; i++)
		{
			if (!eligible[i])
				continue;

			if (best != NOTHING_TO_SELECT && scores[i] <= bestScore)
				continue;

			best = i;
			bestScore = scores[i];
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a phase's condition modules have ALL answered true, which is what advances a phase.
	//!
	//! ⚠ AN EMPTY CONDITION SET ANSWERS TRUE, AND THAT IS THE LOAD-BEARING HALF. A phase with no
	//! advance conditions is one that ends some other way - a terminal operation, or the idle-clock
	//! abort - and answering false for it would be indistinguishable from "this phase is blocked
	//! forever", which is the one failure mode with no symptom a player could report. The registry's
	//! validator is what refuses a phase that has NEITHER a condition NOR a terminal operation; this
	//! function's job is arithmetic, not policy.
	//! \param[in] results One answer per condition module, in the phase's authored order.
	//! \return True when every entry is true, and true for an empty or null set.
	static bool AllConditionsMet(array<bool> results)
	{
		if (!results)
			return true;

		foreach (bool result : results)
		{
			if (!result)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether ANY of a phase's abort modules has fired, which ends the objective.
	//!
	//! ⚠ THE EMPTY ANSWER IS THE OPPOSITE OF AllConditionsMet()'s, AND BOTH ARE RIGHT. A phase with no
	//! aborts must not abort; a phase with no conditions must not be stuck. Getting either one
	//! backwards produces a machine that either never advances or never runs at all, so the two are
	//! stated next to each other and pinned by two cases that name each other.
	//! \param[in] results One answer per abort module, in the phase's authored order.
	//! \return True when any entry is true. False for an empty or null set.
	static bool AnyAbort(array<bool> results)
	{
		if (!results)
			return false;

		foreach (bool result : results)
		{
			if (result)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Where a named phase sits in a plan's authored phase list.
	//!
	//! THE PHASE NAME IS THE PERSISTENCE KEY (D2 - the save carries names, not enum integers, which is
	//! what killed the "never renumber" constraint the old record header carried). This is the one
	//! place a name becomes an index, so a save naming a phase the running build no longer has is
	//! detected in exactly one place and abandoned cleanly rather than adopted as phase 0.
	//!
	//! ⚠ COMPARISON IS EXACT AND CASE-SENSITIVE. A phase name is an identifier a config author types
	//! twice - once in the plan, once in whatever names it - and a fuzzy match would let two phases
	//! that differ only in case both resolve, which is the duplicate the validator exists to catch.
	//! \param[in] names The plan's phase names, in authored order.
	//! \param[in] name The name to find. An empty name matches nothing.
	//! \return The index, or NO_PHASE_INDEX when the name is unknown, empty, or the list is empty.
	static int PhaseIndexOf(array<string> names, string name)
	{
		if (!names || name == "")
			return NO_PHASE_INDEX;

		int count = names.Count();
		for (int i = 0; i < count; i++)
		{
			if (names[i] == name)
				return i;
		}

		return NO_PHASE_INDEX;
	}

	//------------------------------------------------------------------------------------------------
	//! WHICH PHASES A DEPLOYMENT BELONGS TO, in PLAN-INDEX space: an INCLUSIVE range, asked once and
	//! answered the same way by a creation gate and a runtime gate.
	//!
	//! The index-space twin of OVT_ObjectivePhaseRules.PhaseInRange(), which asked the same question of
	//! the hard-coded phase enum. A deployment config names its span by PHASE NAME now (the name is the
	//! persistence key and the only thing a plan and a config can agree on), the names are resolved to
	//! indices by PhaseIndexOf() above, and this compares them.
	//!
	//! 🔴 THE RANGE REPLACED AN EQUALITY TEST AND THE EQUALITY TEST WAS A DEADLOCK. A base objective is
	//! promoted out of its ramp phase by its FIRST completed sabotage mission and needs up to six of
	//! them to earn its counter-attack; with every ramp config scoped to the ramp phase alone, the
	//! promotion itself made the remaining five unsendable and unreachable. Towns deadlocked the same
	//! way one step later, because the stacking support debuff that drives support down is applied by
	//! harassment operations. THE RANGE IS THE WHOLE OF THAT FIX - do not regress it to an equality
	//! test.
	//!
	//! ⚠ A RANGE, NOT A MINIMUM, AND THE UPPER BOUND IS THE POINT. "This phase or later" would keep
	//! sending ramp teams into the battle the ramp was building up to, where men walking in to soften a
	//! place that is already being stormed are noise.
	//!
	//! ⚠ AN UNRESOLVED NAME REFUSES, IT DOES NOT PASS. NO_PHASE_INDEX is negative, so a config naming a
	//! phase its plan does not have belongs to no phase at all - which is the same answer PhaseInRange()
	//! gave for a config authored to the IDLE phase, for the same reason: a span nobody can locate is
	//! not a span.
	//! \param[in] index The plan-phase index the objective is in right now.
	//! \param[in] fromIndex The first phase index this deployment belongs to.
	//! \param[in] throughIndex The last it may continue working in, INCLUSIVE. Below fromIndex means
	//!            "fromIndex only" - the collapse is OVT_ObjectivePhaseRules.EffectiveLastPhase()'s and
	//!            is not restated here, so a reader and the live predicate can never disagree.
	//! \return True when the deployment belongs to the phase the objective is in.
	static bool PhaseIndexInRange(int index, int fromIndex, int throughIndex)
	{
		if (fromIndex < 0)
			return false;

		if (index < fromIndex)
			return false;

		return index <= OVT_ObjectivePhaseRules.EffectiveLastPhase(fromIndex, throughIndex);
	}
}
