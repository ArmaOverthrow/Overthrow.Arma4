//------------------------------------------------------------------------------------------------
//! TIER A cases - the objective director's decision arithmetic: which place it picks
//! (OVT_ObjectiveSelection) and how the ramp progresses once it has (OVT_ObjectivePhaseRules).
//!
//! Every subject here is a static function of plain numbers. Nothing in this file resolves a
//! manager, a controller or a world, and nothing needs to. See OVT_TEST_LogicSuite.c for the tier
//! rule and the house rules - including the reviewer grep over this directory, which does not
//! distinguish code from comments, so neither banned identifier appears anywhere below, prose
//! included.
//!
//! WHY THIS FILE EXISTS. occupying/counter-attacks Phase 1 deleted the random hourly counter-attack;
//! Phase 2 replaced it with a machine that picks ONE objective and works toward it in three phases.
//! Every rule below fails SILENTLY when it is wrong. A sabotage requirement that clamps to the wrong
//! side lets the counter-attack fire immediately (or never). A harassment ramp that runs off the end
//! of its ladder reads in play as "the groups stopped growing". A selection that breaks ties the
//! other way makes the target unpredictable, which is the single thing this feature exists to end. A
//! phase gate off by one on the wrong side of its threshold either never fires or fires at once. None
//! of them is a compile fault, and none of them shows up in a log.
//!
//! THE INVERSION IS NOT ASSERTED HERE. objectiveSabotageMissionsRequired scales BACKWARDS across the
//! presets - Easy demands six, Insane two (G11/D10) - but that ordering is a property of five loaded
//! .conf files, which this tier cannot see by construction. It is pinned in the Init tier instead
//! (OVT_TEST_InitSuite, ..._DifficultyObjectiveSabotageInversion). What IS pinned here is the half
//! that lives in code: whatever the presets say, an unusable value never reaches the phase machine.
//!
//! CAN-FAIL PROOFS. Running a suite is the orchestrator's job, not an implementation agent's
//! (.claude/test-policy.md), so each proof below is a fault that was injected into the subject one at
//! a time and compiled - every one exited tools/compile-check.sh with 0, which is the point: none of
//! these are syntax errors and nothing else in the tree would stop them reaching players. The subject
//! was restored and re-compiled clean afterwards. The exact resulting failure text is recorded per
//! case, computed directly from the rows.
//!
//! No maxAttempts anywhere: every subject is a pure function with no clock, no RNG and no world, and
//! cannot flake.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! OVT_ObjectivePhaseRules.RequiredSabotageMissions - trust the authored value, or do not.
//!
//! Three claims, each with its own rows:
//!   1. A SANE AUTHORED VALUE PASSES THROUGH UNCHANGED, including both ends of the accepted band
//!      (1 and MAX_SABOTAGE_MISSIONS). This is the row that stops a defensive clamp from quietly
//!      overriding a tuner.
//!   2. AN UNUSABLE AUTHORED VALUE FALLS BACK - zero (the value an unauthored int field actually
//!      holds), negative, and absurd-high. Zero is the important one: Difficulty_TestWorld.conf
//!      authors none of these fields on purpose, so zero is the value the test world really passes.
//!   3. AN UNUSABLE FALLBACK FALLS BACK AGAIN, to DEFAULT_SABOTAGE_MISSIONS, rather than returning
//!      the caller's junk. A zero requirement means "Phase 3 may fire with no sabotage at all",
//!      which is the exact unpredictability this feature was built to end.
//!
//! CAN-FAIL, three faults, injected and compiled separately because the case makes three
//! independent claims and one fault must not stand in for another:
//!   M1. RELAX THE UPPER BOUND - `if (authored >= 1)` in place of the banded first test, so an
//!       absurd value is trusted. Compiled clean (exit 0). The case then fails on
//!       "an absurd authored value (999) must fall back: got 999, expected 4".
//!   M2. ACCEPT ZERO - `if (authored >= 0 && authored <= MAX_SABOTAGE_MISSIONS)`. Compiled clean
//!       (exit 0). The case then fails on
//!       "an unauthored field holds 0 and must fall back: got 0, expected 5".
//!   M3. DROP THE SECOND GUARD - `return fallback;` in place of the banded fallback test and the
//!       DEFAULT return. Compiled clean (exit 0). The case then fails on
//!       "a junk fallback resolves to the declared default: got 0, expected 4".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveScaling_RequiredSabotageMissions_ClampsUnusableToFallback : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- Claim 1: a sane authored value is passed straight through, at every preset's value.
		if (!ExpectMissions(6, 4, 6, "Easy's authored six"))
			return true;

		if (!ExpectMissions(5, 4, 5, "Normal's authored five"))
			return true;

		if (!ExpectMissions(4, 4, 4, "Hard's authored four"))
			return true;

		if (!ExpectMissions(3, 4, 3, "Extreme's authored three"))
			return true;

		if (!ExpectMissions(2, 4, 2, "Insane's authored two"))
			return true;

		// --- ...including both ends of the accepted band, so the guard is not one off at either end.
		if (!ExpectMissions(1, 4, 1, "one mission is a legitimate setting"))
			return true;

		if (!ExpectMissions(OVT_ObjectivePhaseRules.MAX_SABOTAGE_MISSIONS, 4,
			OVT_ObjectivePhaseRules.MAX_SABOTAGE_MISSIONS, "the top of the accepted band is accepted"))
			return true;

		// --- Claim 2: an unusable authored value falls back to the caller's value.
		if (!ExpectMissions(0, 5, 5, "an unauthored field holds 0 and must fall back"))
			return true;

		if (!ExpectMissions(-3, 5, 5, "a negative authored value must fall back"))
			return true;

		if (!ExpectMissions(999, 4, 4, "an absurd authored value (999) must fall back"))
			return true;

		if (!ExpectMissions(OVT_ObjectivePhaseRules.MAX_SABOTAGE_MISSIONS + 1, 4, 4,
			"one past the top of the band must fall back"))
			return true;

		// --- Claim 3: a junk fallback does not survive either.
		if (!ExpectMissions(0, 0, OVT_ObjectivePhaseRules.DEFAULT_SABOTAGE_MISSIONS,
			"a junk fallback resolves to the declared default"))
			return true;

		if (!ExpectMissions(0, -7, OVT_ObjectivePhaseRules.DEFAULT_SABOTAGE_MISSIONS,
			"a negative fallback resolves to the declared default"))
			return true;

		if (!ExpectMissions(0, 5000, OVT_ObjectivePhaseRules.DEFAULT_SABOTAGE_MISSIONS,
			"an absurd fallback resolves to the declared default"))
			return true;

		// --- ...but a sane fallback is still honoured, so claim 3 has not swallowed claim 2.
		if (!ExpectMissions(-1, 2, 2, "a sane fallback survives an unusable authored value"))
			return true;

		Print("ObjectiveScaling: an authored sabotage requirement in [1,20] is trusted verbatim; 0, negative and absurd fall back, and a junk fallback resolves to the declared default rather than to a free Phase 3");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one sabotage-requirement row, naming the row in the failure message.
	//! \param[in] authored The difficulty value handed in.
	//! \param[in] fallback The caller's fallback.
	//! \param[in] expected The requirement the rule must produce.
	//! \param[in] label Human description of the row, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectMissions(int authored, int fallback, int expected, string label)
	{
		int actual = OVT_ObjectivePhaseRules.RequiredSabotageMissions(authored, fallback);

		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_ObjectivePhaseRules.HarassmentLadderIndex - the ramp climbs, then stops climbing.
//!
//! The harassment ladder is the existing faction group registry walked in ascending size, so the
//! index this returns is read straight into an array. Two things therefore have to be true at once,
//! and they pull in opposite directions:
//!   1. THE RAMP CLIMBS. Each success buys the next rung, which is the "groups of increasing size"
//!      the requirements ask for and the only warning a player gets that the objective is escalating.
//!   2. THE RAMP NEVER LEAVES THE ARRAY. It saturates at the top rung - it does not wrap back to a
//!      two-man patrol, and it does not run one past the end. An out-of-range read here would be an
//!      engine error in the spawning module, far away from the arithmetic that caused it.
//!
//! The empty-ladder row is the third claim: NO_LADDER_RUNG, not 0. A faction registry with no
//! matching entries is a config fault, and "rung 0 of nothing" turns it into a crash somewhere else.
//!
//! CAN-FAIL, three faults, injected and compiled separately:
//!   M4. SATURATE ONE LATE - `if (successes > rungs) return rungs - 1;` in place of `>=`, so the
//!       exactly-full count reads one past the end. Compiled clean (exit 0). The case then fails on
//!       "a fourth success on a four-rung ladder saturates: got 4, expected 3".
//!   M5. DROP THE NEGATIVE GUARD - `if (successes == 0) return 0;` in place of `<= 0`. Compiled
//!       clean (exit 0). The case then fails on
//!       "a negative success count reads as none: got -2, expected 0".
//!   M6. RETURN 0 FOR AN EMPTY LADDER - `if (rungs <= 0) return 0;`. Compiled clean (exit 0). The
//!       case then fails on "an empty ladder yields no rung at all: got 0, expected -1".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveScaling_HarassmentLadder_SaturatesAtTopRung : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- Claim 1: on the real four-rung ladder, each success buys the next rung.
		if (!ExpectRung(0, 4, 0, "no successes yet starts at the bottom rung"))
			return true;

		if (!ExpectRung(1, 4, 1, "one success climbs one rung"))
			return true;

		if (!ExpectRung(2, 4, 2, "two successes climb two rungs"))
			return true;

		if (!ExpectRung(3, 4, 3, "three successes reach the top rung"))
			return true;

		// --- Claim 2: and then it stops, rather than wrapping or running off the end.
		if (!ExpectRung(4, 4, 3, "a fourth success on a four-rung ladder saturates"))
			return true;

		if (!ExpectRung(5, 4, 3, "a fifth success stays saturated"))
			return true;

		if (!ExpectRung(500, 4, 3, "a very long objective stays saturated rather than wrapping"))
			return true;

		// --- A negative count is treated as none, not as an index below the array.
		if (!ExpectRung(-2, 4, 0, "a negative success count reads as none"))
			return true;

		// --- A one-rung ladder is saturated from the first success onward.
		if (!ExpectRung(0, 1, 0, "a single-rung ladder starts at its only rung"))
			return true;

		if (!ExpectRung(9, 1, 0, "a single-rung ladder cannot climb"))
			return true;

		// --- Claim 3: no ladder means no rung, and that is NOT rung zero.
		if (!ExpectRung(0, 0, OVT_ObjectivePhaseRules.NO_LADDER_RUNG, "an empty ladder yields no rung at all"))
			return true;

		if (!ExpectRung(7, 0, OVT_ObjectivePhaseRules.NO_LADDER_RUNG, "successes cannot conjure a rung out of an empty ladder"))
			return true;

		if (!ExpectRung(7, -3, OVT_ObjectivePhaseRules.NO_LADDER_RUNG, "a negative rung count yields no rung"))
			return true;

		Print("ObjectiveScaling: the harassment ramp climbs one rung per success and saturates at the top of the ladder; an empty ladder yields no rung rather than rung zero");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one ladder row, naming the row in the failure message.
	//! \param[in] successes Completed operations at the objective.
	//! \param[in] rungs Size of the group ladder.
	//! \param[in] expected The index the rule must produce.
	//! \param[in] label Human description of the row, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectRung(int successes, int rungs, int expected, string label)
	{
		int actual = OVT_ObjectivePhaseRules.HarassmentLadderIndex(successes, rungs);

		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_ObjectiveSelection.SelectBestIndex - the highest score wins, and a tie is not a coin toss.
//!
//! WHY THE TIE RULE IS THE IMPORTANT HALF. Predictability is a stated goal of this feature (G1): an
//! experienced player is supposed to be able to look at the map and guess where the occupying faction
//! will go next. Two equally attractive candidates therefore have to resolve the SAME way every time,
//! and the only stable order available is the caller's - registry and discovery order. A comparison
//! written with >= instead of > would hand the win to the LAST tied candidate, which is still
//! deterministic in a single session but flips the moment the discovery order changes, which is
//! exactly the "why did it pick that one" report nobody can reproduce.
//!
//! THE REFUSAL ROWS MATTER TOO. A null score list, an empty one and a mask of the wrong length all
//! answer NOTHING_TO_SELECT rather than guessing: picking through a mis-aligned mask could commit to
//! the very candidate that was supposed to be sitting out its cooldown.
//!
//! CAN-FAIL, three faults, injected into OVT_ObjectiveSelection.c separately:
//!   M7. TIES TO THE LAST ENTRY - `scores[i] >= bestScore` in place of the strict comparison.
//!       Compiled clean (exit 0). The case then fails on
//!       "a tie at the top goes to the FIRST candidate: got 2, expected 0".
//!   M8. IGNORE THE MASK - the `if (blacklisted && blacklisted[i]) continue;` guard deleted.
//!       Compiled clean (exit 0). The case then fails on
//!       "a blacklisted top score is skipped: got 0, expected 1".
//!   M9. ANSWER THROUGH A RAGGED MASK INSTEAD OF REFUSING IT - the count-mismatch guard returning 0
//!       (the first candidate) rather than NOTHING_TO_SELECT. Compiled clean (exit 0). The case then
//!       fails on "a mask of the wrong length is refused outright: got 0, expected -1".
//!       ⚠ The guard is deliberately NOT deleted outright for this proof: without it the loop reads
//!       one past the end of the shorter mask, which is an out-of-range read rather than a clean red.
//!       Returning the wrong ANSWER is the defect being pinned; the read is a second, worse one.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveScaling_SelectBestIndex_HighestWinsTiesByInputOrder : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- Claim 1: the highest score wins, wherever it sits in the list.
		array<float> ascending = {1.0, 5.0, 3.0};
		if (!ExpectBest(ascending, null, 1, "the highest score wins from the middle"))
			return true;

		array<float> topFirst = {9.5, 2.0, 3.0};
		if (!ExpectBest(topFirst, null, 0, "the highest score wins from the front"))
			return true;

		array<float> topLast = {1.0, 2.0, 3.5};
		if (!ExpectBest(topLast, null, 2, "the highest score wins from the back"))
			return true;

		array<float> single = {0.0};
		if (!ExpectBest(single, null, 0, "one candidate with a zero score is still the pick"))
			return true;

		// --- Claim 2: ties go to the caller's order, which is what makes the target guessable.
		array<float> tiedAtTop = {7.0, 4.0, 7.0};
		if (!ExpectBest(tiedAtTop, null, 0, "a tie at the top goes to the FIRST candidate"))
			return true;

		array<float> tiedLater = {1.0, 6.0, 6.0, 6.0};
		if (!ExpectBest(tiedLater, null, 1, "a three-way tie goes to the earliest of them"))
			return true;

		// --- Claim 3: the mask is honoured, entry by entry.
		array<float> masked = {8.0, 5.0, 1.0};
		array<bool> topOut = {true, false, false};
		if (!ExpectBest(masked, topOut, 1, "a blacklisted top score is skipped"))
			return true;

		array<bool> noneOut = {false, false, false};
		if (!ExpectBest(masked, noneOut, 0, "an all-clear mask changes nothing"))
			return true;

		array<bool> allOut = {true, true, true};
		if (!ExpectBest(masked, allOut, OVT_ObjectiveSelection.NOTHING_TO_SELECT, "an all-blacklisted set selects nothing"))
			return true;

		// --- Claim 4: nothing to pick from, and broken input, both answer "nothing".
		array<float> empty = {};
		if (!ExpectBest(empty, null, OVT_ObjectiveSelection.NOTHING_TO_SELECT, "an empty candidate set selects nothing"))
			return true;

		if (!ExpectBest(null, null, OVT_ObjectiveSelection.NOTHING_TO_SELECT, "a null candidate set selects nothing"))
			return true;

		array<bool> tooShort = {false, false};
		if (!ExpectBest(masked, tooShort, OVT_ObjectiveSelection.NOTHING_TO_SELECT, "a mask of the wrong length is refused outright"))
			return true;

		Print("ObjectiveSelection: the highest score wins, a tie goes to the caller's order so the target stays guessable, a blacklisted candidate is skipped, and an empty or mis-paired input selects nothing");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one selection row, naming the row in the failure message.
	//! \param[in] scores Candidate scores.
	//! \param[in] blacklisted Which of them are sitting out. May be null.
	//! \param[in] expected The index the rule must produce.
	//! \param[in] label Human description of the row, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectBest(array<float> scores, array<bool> blacklisted, int expected, string label)
	{
		int actual = OVT_ObjectiveSelection.SelectBestIndex(scores, blacklisted);

		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! SABOTAGE: which structure a mission takes next, and when a base objective has been worked on
//! enough to move on.
//!
//! 🔴 THE ORDERING RULE IS THE ONLY THING STANDING BETWEEN A PLAYER AND LOSING THEIR GARAGE FIRST.
//! Sabotage destroys player-built structures permanently - no rubble, no refund, no repair - and the
//! design's single concession is that it works from the cheapest thing outward, so the expensive
//! thing somebody spent an hour earning is the LAST to go and there is time to respond. An ordering
//! rule that picked the dearest, or that picked arbitrarily, would be the same feature with the
//! opposite feel, and nothing else in the tree would notice.
//!
//! ⚠ COST IS THE ONLY ORDERING KEY THAT EXISTS. Neither OVT_Buildable nor OVT_Placeable carries a
//! size, a footprint or an importance attribute; m_iCost is the game's own statement of worth. That is
//! a decision, not a proxy for something better that was unavailable.
//!
//! THE SHIPPED-PRICES ROW IS THE POINT OF THIS CASE. Walking the eight authored buildable prices with
//! a destroyed-mask reproduces the whole intended sequence - bunkers, both tents, the guard tower, the
//! ramp and helipad, the fuel depot, and the garage last - which is the claim the requirements make in
//! prose and nothing else in the suite checks.
//!
//! ⚠ TIES GO TO INPUT ORDER, for the same reason SelectBestIndex's do: the enumerator is a sphere
//! query, and two equally-priced tents must be taken in a stable order rather than oscillating.
//!
//! BasePhase2Gate IS ASSERTED HERE TOO because it is the other half of the same mechanic: one
//! completed mission is what lets a base objective leave the harassment phase.
//!
//! CAN-FAIL, five faults, injected separately:
//!   M22. TAKE THE DEAREST - `costs[i] > bestCost` in place of the strict less-than, in
//!        OVT_ObjectiveSelection.NextTargetIndex. Compiled clean (exit 0). The case then fails on
//!        "the cheapest wins from the middle: got 2, expected 1" and, more tellingly, the shipped-price
//!        row fails at its first step with the garage.
//!   M23. TIES TO THE LAST ENTRY - `costs[i] <= bestCost`. Compiled clean (exit 0). The case then
//!        fails on "a tie at the bottom goes to the FIRST candidate: got 2, expected 0".
//!   M24. SEED THE COMPARISON WITH A LITERAL - `int bestCost = 0;` used as a real seed by dropping the
//!        `best == NOTHING_TO_SELECT ||` short-circuit. Compiled clean (exit 0). The case then fails on
//!        "a negative cost sorts first: got -1, expected 0" - the row that exists for exactly this,
//!        because every non-negative row passes with the literal seed and hides it.
//!   M25. IGNORE THE DESTROYED MASK - the `if (alreadyDestroyed && alreadyDestroyed[i]) continue;`
//!        guard deleted. Compiled clean (exit 0). The case then fails on
//!        "the cheapest one already taken is skipped: got 0, expected 2", which in the live module is a
//!        mission that demolishes the same (already deleted) entity for the rest of its life and never
//!        touches the second-cheapest.
//!   M26. GATE A BASE ON ZERO MISSIONS - `successes >= 0` in OVT_ObjectivePhaseRules.BasePhase2Gate.
//!        Compiled clean (exit 0). The case then fails on
//!        "no completed mission does not open the base gate: got 1, expected 0" - which in the live
//!        director fires the transition on the phase's own ENTRY tick, skipping the sabotage phase
//!        entirely and destroying nothing at all before the forward base goes up.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveScaling_SabotageTargetsCheapestFirst : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- Claim 1: the cheapest wins, wherever it sits in the list.
		array<int> middle = {1200, 750, 8000};
		if (!ExpectTarget(middle, null, 1, "the cheapest wins from the middle"))
			return true;

		array<int> front = {750, 1200, 8000};
		if (!ExpectTarget(front, null, 0, "the cheapest wins from the front"))
			return true;

		array<int> back = {8000, 1200, 750};
		if (!ExpectTarget(back, null, 2, "the cheapest wins from the back"))
			return true;

		array<int> single = {8000};
		if (!ExpectTarget(single, null, 0, "one candidate is the pick however dear it is"))
			return true;

		// --- Claim 2: ties go to the caller's order.
		array<int> tiedAtBottom = {1000, 1500, 1000};
		if (!ExpectTarget(tiedAtBottom, null, 0, "a tie at the bottom goes to the FIRST candidate"))
			return true;

		array<int> tiedLater = {8000, 1000, 1000, 1000};
		if (!ExpectTarget(tiedLater, null, 1, "a three-way tie goes to the earliest of them"))
			return true;

		// --- Claim 3: the destroyed mask is honoured, entry by entry.
		array<int> masked = {750, 1200, 1000};
		array<bool> cheapestGone = {true, false, false};
		if (!ExpectTarget(masked, cheapestGone, 2, "the cheapest one already taken is skipped"))
			return true;

		array<bool> noneGone = {false, false, false};
		if (!ExpectTarget(masked, noneGone, 0, "an all-clear mask changes nothing"))
			return true;

		array<bool> allGone = {true, true, true};
		if (!ExpectTarget(masked, allGone, OVT_ObjectiveSelection.NOTHING_TO_SELECT, "an all-destroyed base offers nothing"))
			return true;

		// --- Claim 4: nothing to pick from, and broken input, both answer "nothing".
		array<int> empty = {};
		if (!ExpectTarget(empty, null, OVT_ObjectiveSelection.NOTHING_TO_SELECT, "a base with no structures offers nothing"))
			return true;

		if (!ExpectTarget(null, null, OVT_ObjectiveSelection.NOTHING_TO_SELECT, "a null candidate set offers nothing"))
			return true;

		array<bool> tooShort = {false, false};
		if (!ExpectTarget(masked, tooShort, OVT_ObjectiveSelection.NOTHING_TO_SELECT, "a mask of the wrong length is refused outright"))
			return true;

		// --- Claim 5: a negative cost sorts first and does not crash. Not hypothetical for a modded
		// config, and the row exists because it is the ONLY one that catches a literal-seeded
		// comparison - every non-negative row passes with a zero seed.
		array<int> negative = {-50, 750, 1000};
		if (!ExpectTarget(negative, null, 0, "a negative cost sorts first"))
			return true;

		// --- Claim 6: the eight SHIPPED buildable prices come out in the documented order, walked end
		// to end with the mask a real mission maintains. Listed deliberately NOT in price order.
		array<int> shipped = {1200, 1000, 1000, 1500, 750, 8000, 1500, 2000};
		array<bool> taken = {false, false, false, false, false, false, false, false};
		array<int> expectedOrder = {4, 1, 2, 0, 3, 6, 7, 5};

		foreach (int step, int expected : expectedOrder)
		{
			int actual = OVT_ObjectiveSelection.NextTargetIndex(shipped, taken);
			if (actual != expected)
			{
				// ⚠ SetFailure takes at most THREE parameters, so the step and the expectation share one.
				SetFailure("the shipped prices demolish in ascending order: at step %1 it took index %2, cost %3",
					step.ToString() + " (expected index " + expected.ToString() + ")",
					actual.ToString(),
					shipped[actual].ToString());
				return true;
			}

			taken[actual] = true;
		}

		if (OVT_ObjectiveSelection.NextTargetIndex(shipped, taken) != OVT_ObjectiveSelection.NOTHING_TO_SELECT)
		{
			SetFailure("a fully demolished base still offers a target");
			return true;
		}

		// --- Claim 7: one completed mission is the base objective's forward-base gate, and zero is not.
		if (!ExpectBaseGate(0, false, "no completed mission does not open the base gate"))
			return true;

		if (!ExpectBaseGate(1, true, "one completed mission opens the base gate"))
			return true;

		if (!ExpectBaseGate(4, true, "more than one still opens it"))
			return true;

		if (!ExpectBaseGate(-1, false, "a negative counter does not open it"))
			return true;

		Print("ObjectiveSelection: sabotage takes the cheapest structure still standing, ties by discovery order, the eight shipped prices come out bunkers-first and garage-last, and a base objective needs one completed mission to leave harassment");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one target-ordering row, naming the row in the failure message.
	//! \param[in] costs Candidate structure costs.
	//! \param[in] alreadyDestroyed Which of them are gone. May be null.
	//! \param[in] expected The index the rule must produce.
	//! \param[in] label Human description of the row, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectTarget(array<int> costs, array<bool> alreadyDestroyed, int expected, string label)
	{
		int actual = OVT_ObjectiveSelection.NextTargetIndex(costs, alreadyDestroyed);

		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one base forward-base gate row.
	//! \param[in] successes Completed sabotage missions.
	//! \param[in] expected Whether the gate must open.
	//! \param[in] label Human description of the row, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectBaseGate(int successes, bool expected, string label)
	{
		bool actual = OVT_ObjectivePhaseRules.BasePhase2Gate(successes);

		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_ObjectiveSelection blacklist arithmetic - a cooldown, not a ban.
//!
//! An objective that could not be built toward sits out ONE selection round and then becomes
//! eligible again. Two rules make that work, and they have to agree:
//!   1. DECAY NEVER GOES NEGATIVE. An entry that has served its time rests at exactly zero. Left to
//!      run negative, a place decayed a few times more than it was selected would need several
//!      re-blacklistings before it counted as blacklisted again - so a repeatedly-failing objective
//!      would be re-picked immediately, fail again, and loop.
//!   2. AN EXHAUSTED ENTRY IS NOT BLACKLISTED, whether or not the caller has pruned it. That is what
//!      "drops at zero" means in practice, and asserting it through IsBlacklisted rather than through
//!      a list length is what makes the two rules provably consistent.
//!
//! THE KEY ROWS ARE PART OF THE SAME CLAIM. The blacklist survives a save as POSITIONS, and positions
//! are matched back through PositionKey - so the key has to be stable under the sub-metre wobble a
//! codec can introduce and under whatever height the terrain was, and it has to still distinguish two
//! genuinely different places.
//!
//! CAN-FAIL, three faults, injected into OVT_ObjectiveSelection.c separately:
//!   M10. LET DECAY GO NEGATIVE - `roundsLeft[i] = roundsLeft[i] - 1;` unconditionally. Compiled
//!        clean (exit 0). The case then fails on the first row that reaches zero,
//!        "an already-served entry stays at zero: got -1, expected 0".
//!   M11. IGNORE THE ROUNDS IN IsBlacklisted - `return true;` on a key match regardless of rounds.
//!        Compiled clean (exit 0). The case then fails on
//!        "an entry that has served its round is no longer blacklisted: got 1, expected 0".
//!   M12. KEY ON THE FULL VECTOR - PositionKey building its key from all three components. Compiled
//!        clean (exit 0). The case then fails on the first key row,
//!        "sub-metre wobble and height are not part of the key: '100,5,200' and '100,12,200'" - a
//!        saved blacklist entry would stop matching its own place the moment the recorded altitude
//!        differed by a hair.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveScaling_Blacklist_DecaysToZeroAndExpires : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- Claim 1: decay serves exactly one round and floors at zero.
		array<int> rounds = {3, 1, 0};
		OVT_ObjectiveSelection.DecayBlacklist(rounds);

		if (!ExpectRound(rounds, 0, 2, "a three-round entry has two left after one round"))
			return true;

		if (!ExpectRound(rounds, 1, 0, "a one-round entry has served its time"))
			return true;

		if (!ExpectRound(rounds, 2, 0, "an already-served entry stays at zero"))
			return true;

		OVT_ObjectiveSelection.DecayBlacklist(rounds);
		OVT_ObjectiveSelection.DecayBlacklist(rounds);
		OVT_ObjectiveSelection.DecayBlacklist(rounds);

		if (!ExpectRound(rounds, 1, 0, "an exhausted entry rests at zero rather than going negative"))
			return true;

		if (!ExpectRound(rounds, 0, 0, "every entry is exhausted after enough rounds, and none is negative"))
			return true;

		// A null list is a no-op rather than an error - a campaign with nothing blacklisted is normal.
		OVT_ObjectiveSelection.DecayBlacklist(null);

		// --- Claim 2: only an entry with rounds still to serve counts as blacklisted.
		array<string> keys = {"100,200", "300,400"};
		array<int> serving = {1, 0};

		if (!ExpectBlacklisted(keys, serving, "100,200", true, "an entry with a round still to serve is blacklisted"))
			return true;

		if (!ExpectBlacklisted(keys, serving, "300,400", false, "an entry that has served its round is no longer blacklisted"))
			return true;

		if (!ExpectBlacklisted(keys, serving, "500,600", false, "a place that was never blacklisted is not blacklisted"))
			return true;

		if (!ExpectBlacklisted(keys, serving, "", false, "an empty key matches nothing"))
			return true;

		if (!ExpectBlacklisted(null, serving, "100,200", false, "a null key list blacklists nothing"))
			return true;

		if (!ExpectBlacklisted(keys, null, "100,200", false, "a null rounds list blacklists nothing"))
			return true;

		array<int> tooShort = {1};
		if (!ExpectBlacklisted(keys, tooShort, "100,200", false, "a mis-paired blacklist blacklists nothing rather than everything"))
			return true;

		// --- Claim 3: the key is stable where it has to be and distinct where it has to be.
		if (!ExpectSameKey("100.2 5 200.4", "100.4 12 200.3", true, "sub-metre wobble and height are not part of the key"))
			return true;

		if (!ExpectSameKey("100 0 200", "100 999 200", true, "height is not part of the key"))
			return true;

		if (!ExpectSameKey("100 0 200", "102 0 200", false, "two metres apart is a different place"))
			return true;

		if (!ExpectSameKey("100 0 200", "100 0 203", false, "three metres apart on the other axis is a different place"))
			return true;

		Print("ObjectiveSelection: a blacklist entry serves one round per selection, floors at zero rather than going negative, stops counting as blacklisted the moment it is exhausted, and is keyed to whole metres on the ground plane so it survives a save");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one decayed round count.
	//! \param[in] rounds The decayed list.
	//! \param[in] index Which entry.
	//! \param[in] expected The count it must hold.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectRound(array<int> rounds, int index, int expected, string label)
	{
		int actual = rounds[index];

		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one blacklist lookup.
	//! \param[in] keys Blacklisted keys.
	//! \param[in] rounds Rounds each still owes.
	//! \param[in] key The key to look up.
	//! \param[in] expected Whether it must read as blacklisted.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectBlacklisted(array<string> keys, array<int> rounds, string key, bool expected, string label)
	{
		bool actual = OVT_ObjectiveSelection.IsBlacklisted(keys, rounds, key);

		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts whether two positions share a blacklist key.
	//! \param[in] a First position.
	//! \param[in] b Second position.
	//! \param[in] expectedSame Whether the two must produce the same key.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectSameKey(vector a, vector b, bool expectedSame, string label)
	{
		string keyA = OVT_ObjectiveSelection.PositionKey(a);
		string keyB = OVT_ObjectiveSelection.PositionKey(b);

		bool same = keyA == keyB;
		if (same == expectedSame)
			return true;

		SetFailure("%1: '%2' and '%3'", label, keyA, keyB);

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_ObjectivePhaseRules phase gates - every threshold asserted on both sides.
//!
//! WHY EVERY BOUNDARY GETS ITS OWN ROW. These three predicates are the only thing standing between
//! "the occupying faction builds up visibly for tens of in-game minutes" and "the counter-attack
//! arrives out of nowhere". A gate off by one in the permissive direction fires the battle a phase
//! early; off by one the other way it never fires at all and the objective ages out. Both read in
//! play as a pacing problem, neither produces an error, and the difference between them is a single
//! comparison operator.
//!
//! THE CONJUNCTS ARE ASSERTED INDEPENDENTLY, one row per reason to refuse, because an && chain that
//! lost a term still passes every row where that term happened to be true.
//!
//! CAN-FAIL, three faults, injected into OVT_ObjectivePhaseRules.c separately:
//!   M13. RELAX THE TOWN GATE TO <= - the counter-attack then fires with the town exactly on the
//!        threshold. Compiled clean (exit 0). The case then fails on
//!        "a town exactly on the counter-attack threshold has NOT qualified".
//!   M14. DROP THE fobUp CONJUNCT from BasePhase3Gate. Compiled clean (exit 0). The case then fails
//!        on "a base objective with no forward base may not counter-attack: got 1, expected 0" - the
//!        battle would start with no wave source and the whole middle phase would be skippable.
//!   M15. TRUST A ZERO REQUIREMENT - the `if (demanded < 1)` clamp deleted from BasePhase3Gate.
//!        Compiled clean (exit 0). The case then fails on
//!        "a zero requirement is not a free counter-attack: got 1, expected 0".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveScaling_PhaseGates_AssertedOnBothSides : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- The forward-base gate: strictly below half support.
		if (!ExpectTownFOB(49, true, "a town one point below the forward-base threshold qualifies"))
			return true;

		if (!ExpectTownFOB(50, false, "a town exactly on the forward-base threshold has NOT qualified"))
			return true;

		if (!ExpectTownFOB(51, false, "a town above the forward-base threshold has not qualified"))
			return true;

		if (!ExpectTownFOB(0, true, "a fully collapsed town qualifies"))
			return true;

		if (!ExpectTownFOB(100, false, "a fully supportive town does not qualify"))
			return true;

		// --- The town counter-attack gate: support, the forward base, and the reserve, independently.
		if (!ExpectTownQRF(24, true, 1500, 1500, true, "everything met fires the counter-attack"))
			return true;

		if (!ExpectTownQRF(25, true, 1500, 1500, false, "a town exactly on the counter-attack threshold has NOT qualified"))
			return true;

		if (!ExpectTownQRF(24, false, 1500, 1500, false, "a town objective with no forward base may not counter-attack"))
			return true;

		if (!ExpectTownQRF(24, true, 1499, 1500, false, "one resource short of the gate waits"))
			return true;

		if (!ExpectTownQRF(24, true, 0, 0, true, "an unset resource gate gates nothing"))
			return true;

		// --- The base counter-attack gate: sabotage count, the forward base, and the reserve.
		if (!ExpectBaseQRF(4, 4, true, 1500, 1500, true, "the required sabotage count exactly met fires"))
			return true;

		if (!ExpectBaseQRF(5, 4, true, 1500, 1500, true, "more than required still fires"))
			return true;

		if (!ExpectBaseQRF(3, 4, true, 1500, 1500, false, "one sabotage mission short waits"))
			return true;

		if (!ExpectBaseQRF(4, 4, false, 1500, 1500, false, "a base objective with no forward base may not counter-attack"))
			return true;

		if (!ExpectBaseQRF(4, 4, true, 1499, 1500, false, "one resource short of the gate waits"))
			return true;

		if (!ExpectBaseQRF(0, 0, true, 99999, 0, false, "a zero requirement is not a free counter-attack"))
			return true;

		if (!ExpectBaseQRF(OVT_ObjectivePhaseRules.DEFAULT_SABOTAGE_MISSIONS, 0, true, 99999, 0, true, "a zero requirement falls back to the declared default and is then satisfiable"))
			return true;

		// --- The resource gate on its own, since two gates share it.
		if (!ExpectResourceGate(0, 0, true, "an unset gate is always met"))
			return true;

		if (!ExpectResourceGate(0, -50, true, "a negative gate is always met"))
			return true;

		if (!ExpectResourceGate(1500, 1500, true, "exactly the gate is enough - the gate IS the expected cost"))
			return true;

		if (!ExpectResourceGate(1499, 1500, false, "one short of the gate is not enough"))
			return true;

		Print("ObjectivePhaseRules: both counter-attack gates refuse on each of their three conjuncts independently, every support threshold is strict, and a zero sabotage requirement falls back to the declared default rather than opening the gate");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one forward-base gate row.
	//! \param[in] support Town support percentage.
	//! \param[in] expected Whether the gate must open.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectTownFOB(int support, bool expected, string label)
	{
		bool actual = OVT_ObjectivePhaseRules.TownPhase2Gate(support);

		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one town counter-attack gate row.
	//! \param[in] support Town support percentage.
	//! \param[in] fobUp Whether the forward base is standing.
	//! \param[in] reserve The occupying faction's reserve.
	//! \param[in] gate The reserve demanded.
	//! \param[in] expected Whether the gate must open.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectTownQRF(int support, bool fobUp, int reserve, int gate, bool expected, string label)
	{
		bool actual = OVT_ObjectivePhaseRules.TownPhase3Gate(support, fobUp, reserve, gate);

		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one base counter-attack gate row.
	//! \param[in] successes Completed sabotage missions.
	//! \param[in] required How many the difficulty demands.
	//! \param[in] fobUp Whether the forward base is standing.
	//! \param[in] reserve The occupying faction's reserve.
	//! \param[in] gate The reserve demanded.
	//! \param[in] expected Whether the gate must open.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectBaseQRF(int successes, int required, bool fobUp, int reserve, int gate, bool expected, string label)
	{
		bool actual = OVT_ObjectivePhaseRules.BasePhase3Gate(successes, required, fobUp, reserve, gate);

		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one resource gate row.
	//! \param[in] reserve The occupying faction's reserve.
	//! \param[in] gate The reserve demanded.
	//! \param[in] expected Whether the gate must be met.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectResourceGate(int reserve, int gate, bool expected, string label)
	{
		bool actual = OVT_ObjectivePhaseRules.MeetsResourceGate(reserve, gate);

		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_ObjectivePhaseRules.IsFOBStarved - three separate ways to be cut off.
//!
//! The forward operating base is abandoned after it has been starved for a difficulty-scaled number
//! of in-game minutes, and starvation is the OR of three unrelated conditions: the base supplying it
//! has been taken, its own garrison is dead, or the resistance is camped on it. Each is asserted
//! ALONE, with the other two healthy, because an OR chain that lost a term still answers correctly on
//! every row where another term happens to be true - and the term most likely to be lost (the player
//! one) is the term that makes the forward base removable by fighting rather than by waiting.
//!
//! CAN-FAIL, two faults, injected into OVT_ObjectivePhaseRules.c separately:
//!   M16. DROP THE PLAYER TERM - `return false;` in place of `return playerPresent;`. Compiled clean
//!        (exit 0). The case then fails on
//!        "the resistance camped on it starves it on its own: got 0, expected 1".
//!   M17. REQUIRE A DEAD GARRISON RATHER THAN A THINNED ONE - `aliveGroups < 0`. Compiled clean
//!        (exit 0). The case then fails on
//!        "no garrison left starves it on its own: got 0, expected 1".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveScaling_Starvation_EachInputIndependently : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- Healthy on every input: not starved. This row is what stops the case passing vacuously.
		if (!ExpectStarved(true, 2, false, false, "a supplied, garrisoned, unmolested forward base is not starved"))
			return true;

		if (!ExpectStarved(true, 1, false, false, "a single surviving group is still a garrison"))
			return true;

		// --- Each input alone, with the other two healthy.
		if (!ExpectStarved(false, 2, false, true, "losing the source base starves it on its own"))
			return true;

		if (!ExpectStarved(true, 0, false, true, "no garrison left starves it on its own"))
			return true;

		if (!ExpectStarved(true, 2, true, true, "the resistance camped on it starves it on its own"))
			return true;

		// --- And in combination, so no term cancels another out.
		if (!ExpectStarved(false, 0, true, true, "everything wrong at once is still starved"))
			return true;

		if (!ExpectStarved(false, 0, false, true, "two of the three is starved"))
			return true;

		// --- A negative group count is a caller bug, and it must not read as a healthy garrison.
		if (!ExpectStarved(true, -1, false, true, "a negative group count reads as no garrison"))
			return true;

		Print("ObjectivePhaseRules: each of the three starvation inputs cuts the forward base off on its own, and all three healthy is the only combination that is not starved");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one starvation row.
	//! \param[in] sourceHeld Whether the supplying base is still held.
	//! \param[in] aliveGroups Groups still alive at the forward base.
	//! \param[in] playerPresent Whether an enemy player is on it.
	//! \param[in] expected Whether it must read as starved.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectStarved(bool sourceHeld, int aliveGroups, bool playerPresent, bool expected, string label)
	{
		bool actual = OVT_ObjectivePhaseRules.IsFOBStarved(sourceHeld, aliveGroups, playerPresent);

		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_ObjectivePhaseRules - the forward base's spend ceiling, and the machine's whole sense of time.
//!
//! THE CEILING IS THE ACCOUNTING RULE (G5). The director never holds money: every resource it spends
//! leaves the one deployment pool at the moment the deployment is created, and the forward base's
//! "budget" is nothing but a running total compared against this number. Derive it wrong and the ramp
//! either cannot afford its own structure or spends the faction's entire defence budget on one
//! objective - and this epic's history is broken bookkeeping (BUG-026/027/029), so the derivation gets
//! an assertion of its own rather than a comment.
//!
//! TickDown IS ONE LINE AND IS ASSERTED ANYWAY, because it is the ONLY way any timer in the machine
//! moves. Every phase timeout, every operation cadence and the starvation clock all run through it,
//! so a floor that let a counter go negative would let a phase that had already expired keep
//! expiring, and "the objective froze while a battle was live" - which is a stated requirement -
//! would stop being provable without a clock.
//!
//! CAN-FAIL, two faults, injected into OVT_ObjectivePhaseRules.c separately:
//!   M18. LET THE CEILING BE EXCLUSIVE - `spent < ceiling` in WithinFOBCeiling. Compiled clean
//!        (exit 0). The case then fails on
//!        "landing exactly on the ceiling is spending the budget, not exceeding it: got 0, expected 1".
//!   M19. DROP TickDown's FLOOR - `return value - 1;` unconditionally. Compiled clean (exit 0). The
//!        case then fails on "a spent counter rests at zero: got -1, expected 0".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveScaling_CeilingAndTickDown : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- The ceiling derives from the authored cost, at every shipped value of it.
		if (!ExpectCeiling(400, 1200, "the shipped forward-base cost yields three times itself"))
			return true;

		if (!ExpectCeiling(1, OVT_ObjectivePhaseRules.FOB_CEILING_MULTIPLIER, "the multiplier is applied, not added"))
			return true;

		if (!ExpectCeiling(0, 0, "an unauthored cost yields no budget at all"))
			return true;

		if (!ExpectCeiling(-400, 0, "a negative cost yields no budget rather than a negative one"))
			return true;

		// --- Below, at, and above the ceiling.
		if (!ExpectWithin(0, 1200, true, "having spent nothing is inside the ceiling"))
			return true;

		if (!ExpectWithin(1199, 1200, true, "one under the ceiling is inside it"))
			return true;

		if (!ExpectWithin(1200, 1200, true, "landing exactly on the ceiling is spending the budget, not exceeding it"))
			return true;

		if (!ExpectWithin(1201, 1200, false, "one over the ceiling is refused"))
			return true;

		if (!ExpectWithin(0, 0, false, "a zero ceiling refuses everything, so a misauthored cost fails closed"))
			return true;

		if (!ExpectWithin(0, -100, false, "a negative ceiling refuses everything"))
			return true;

		// --- The one line every timer in the machine runs through.
		if (!ExpectTick(45, 44, "a countdown serves one tick"))
			return true;

		if (!ExpectTick(1, 0, "the last tick lands on zero"))
			return true;

		if (!ExpectTick(0, 0, "a spent counter rests at zero"))
			return true;

		if (!ExpectTick(-5, 0, "a counter that was already negative is corrected to zero, not driven further down"))
			return true;

		Print("ObjectivePhaseRules: the forward-base ceiling is three times the authored cost and fails closed when that cost is unusable; every timer in the machine counts down through one flooring rule");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one ceiling derivation.
	//! \param[in] cost The authored forward-base cost.
	//! \param[in] expected The ceiling it must produce.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectCeiling(int cost, int expected, string label)
	{
		int actual = OVT_ObjectivePhaseRules.FOBBudgetCeiling(cost);

		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one ceiling test.
	//! \param[in] spent The running total.
	//! \param[in] ceiling The ceiling.
	//! \param[in] expected Whether the spend must be permitted.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectWithin(int spent, int ceiling, bool expected, string label)
	{
		bool actual = OVT_ObjectivePhaseRules.WithinFOBCeiling(spent, ceiling);

		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one countdown step.
	//! \param[in] value Ticks remaining before the step.
	//! \param[in] expected Ticks remaining after it.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectTick(int value, int expected, string label)
	{
		int actual = OVT_ObjectivePhaseRules.TickDown(value);

		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_ObjectiveSelection scoring - the two scales are comparable, and every term is bounded.
//!
//! WHY THIS IS ASSERTED AT ALL, when the weights are just constants: towns and bases compete in ONE
//! sorted list. If a term saturated at a different value than its weight, or if the proximity term
//! went negative past its limit, one whole KIND of objective would quietly stop being picked - the
//! occupying faction would only ever go after towns, or only ever after bases, and nothing in the
//! game or the log would say why. The saturation rows are what pin the shared ceiling; the falloff
//! rows are what stop a distant candidate scoring below a worthless one.
//!
//! FLOATS ARE COMPARED WITH THE TIER'S EPSILON, never with ==.
//!
//! CAN-FAIL, two faults, injected into OVT_ObjectiveSelection.c separately:
//!   M20. LET THE PROXIMITY TERM GO NEGATIVE - the `distance >= maxUsefulDistance` guard deleted from
//!        ProximityScore. Compiled clean (exit 0). The case then fails on
//!        "a candidate past the useful distance scores the same as one exactly at it".
//!   M21. DROP THE POPULATION CLAMP - Clamp01 removed from ScoreTown's size term. Compiled clean
//!        (exit 0). The case then fails on
//!        "a huge population saturates rather than outweighing every other term".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveScaling_Scoring_TermsAreBoundedAndComparable : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		float townCeiling = OVT_ObjectiveSelection.TOWN_POPULATION_WEIGHT + OVT_ObjectiveSelection.TOWN_SUPPORT_COLLAPSE_WEIGHT + OVT_ObjectiveSelection.PROXIMITY_WEIGHT + OVT_ObjectiveSelection.TOWER_COVERAGE_WEIGHT;
		float baseCeiling = OVT_ObjectiveSelection.BASE_PRIZE_WEIGHT + OVT_ObjectiveSelection.BASE_THREAT_WEIGHT + OVT_ObjectiveSelection.PROXIMITY_WEIGHT + OVT_ObjectiveSelection.TOWER_COVERAGE_WEIGHT;

		// --- THE SHARED CEILING. Towns and bases are sorted against each other, so a drift here would
		//     silently retire one whole kind of objective.
		if (!OVT_TEST_LogicFixture.FloatEquals(townCeiling, baseCeiling))
		{
			SetFailure("the town and base scales must saturate at the same value or one kind stops being picked: town %1, base %2",
				townCeiling.ToString(), baseCeiling.ToString());
			return true;
		}

		// --- Saturation: everything at its best pays exactly the ceiling and no more.
		float bestTown = OVT_ObjectiveSelection.ScoreTown(99999, 0, 0, 5000, true);
		if (!ExpectScore(bestTown, townCeiling, "a huge population saturates rather than outweighing every other term"))
			return true;

		float bestBase = OVT_ObjectiveSelection.ScoreBase(99999, 0, 5000, true);
		if (!ExpectScore(bestBase, baseCeiling, "a huge threat saturates at the base ceiling"))
			return true;

		// --- The floor: nothing going for it scores zero for a town, and the flat prize for a base.
		float worstTown = OVT_ObjectiveSelection.ScoreTown(0, 100, 5000, 5000, false);
		if (!ExpectScore(worstTown, 0, "a tiny, fully supportive, unreachable, unheard town is worth nothing"))
			return true;

		float worstBase = OVT_ObjectiveSelection.ScoreBase(0, 5000, 5000, false);
		if (!ExpectScore(worstBase, OVT_ObjectiveSelection.BASE_PRIZE_WEIGHT, "a quiet, unreachable, unheard base is still worth being a base"))
			return true;

		// --- The proximity falloff is linear, and it stops at zero rather than going negative.
		if (!ExpectProximity(0, 1000, 1.0, "a candidate on top of a held base scores the full term"))
			return true;

		if (!ExpectProximity(500, 1000, 0.5, "half the useful distance scores half the term"))
			return true;

		if (!ExpectProximity(1000, 1000, 0, "exactly at the useful distance scores nothing"))
			return true;

		if (!ExpectProximity(9000, 1000, 0, "a candidate past the useful distance scores the same as one exactly at it"))
			return true;

		if (!ExpectProximity(-50, 1000, 1.0, "a negative distance reads as being on top of it"))
			return true;

		if (!ExpectProximity(100, 0, 0, "with no useful distance the term is disabled rather than dividing by zero"))
			return true;

		// --- Collapsed support is the high-value signal, so the term is inverted.
		float collapsed = OVT_ObjectiveSelection.ScoreTown(0, 0, 5000, 5000, false);
		float supportive = OVT_ObjectiveSelection.ScoreTown(0, 100, 5000, 5000, false);
		if (collapsed <= supportive)
		{
			SetFailure("a support-collapsed town must outscore a supportive one of the same size - that is what replaced the retired suppression trigger: collapsed %1, supportive %2",
				collapsed.ToString(), supportive.ToString());
			return true;
		}

		Print("ObjectiveSelection: town and base scores saturate at the same ceiling so both kinds stay selectable, the proximity term falls off linearly and stops at zero, and a collapsed town outscores a supportive one of the same size");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one score against an expected value, within the tier's epsilon.
	//! \param[in] actual The score produced.
	//! \param[in] expected The score expected.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectScore(float actual, float expected, string label)
	{
		if (OVT_TEST_LogicFixture.FloatEquals(actual, expected))
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one proximity fraction.
	//! \param[in] distance Distance to the nearest held base.
	//! \param[in] maxUseful Distance at which the term reaches zero.
	//! \param[in] expected The fraction expected.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectProximity(float distance, float maxUseful, float expected, string label)
	{
		float actual = OVT_ObjectiveSelection.ProximityScore(distance, maxUseful);

		return ExpectScore(actual, expected, label);
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_FOBSiting - where the occupying faction's forward operating base is allowed to stand.
//!
//! WHY THIS IS THE MOST LOAD-BEARING CASE IN THE FILE. Nothing else checks the siting rules. No
//! curated OVT_FOBPosition marker exists in any shipped world, so the GENERATED path is the only path
//! a real campaign ever takes, and its hard tests all live here as pure predicates. Every one of them
//! fails silently in the direction that matters:
//!   - a degenerate band that ACCEPTED everything would put a forward base on top of the objective it
//!     is supposed to be advancing on, and the phase would look like it worked;
//!   - a ragged exclusion pair that took the shorter list would stop excluding whatever fell off the
//!     end - a player's own camp, most likely, since camps are appended last;
//!   - a boundary that read as clear would plant an enemy flag exactly on the edge of a town;
//!   - a road term that read "no road found" as "on a road" would score the most isolated candidate in
//!     the band as the best served, which is the opposite of the preference.
//! None of them is a compile fault, none of them logs anything, and all of them are invisible until
//! somebody walks the map.
//!
//! THE LATTICE HELPERS ARE ASSERTED TOO, and they are why the sampler is deterministic: the attempt
//! bound is a lattice size rather than a retry budget, so "the sampler found nothing" is a statement
//! about the band and not about luck.
//!
//! FLOATS ARE COMPARED WITH THE TIER'S EPSILON, never with ==.
//!
//! CAN-FAIL, five faults injected into OVT_FOBSiting.c one at a time and compiled. Every one exited
//! tools/compile-check.sh with 0 - a degenerate band accepted, a boundary flipped, a ragged pair
//! tolerated, a sentinel misread and a weight reordered are none of them script errors. The subject
//! was restored and re-compiled clean afterwards.
//!   S1. ACCEPT THE DEGENERATE BAND - `if (min >= max) return false;` deleted from IsInBand, so a
//!       collapsed band falls through to the two ordinary bound tests and a candidate standing exactly
//!       on the collapsed bound passes both. Compiled clean (exit 0). The case then fails on
//!       "a collapsed band accepts nothing even at its own bound: got true, expected false".
//!       ⚠ Only the on-the-bound row can catch this one - every other distance is refused by the
//!       ordinary tests with or without the guard, which is why that row exists and is marked.
//!   S2. TOLERATE A RAGGED PAIR - the `exclusions.Count() != radii.Count()` refusal replaced with a
//!       walk over the shorter list. Compiled clean (exit 0). The case then fails on
//!       "a ragged exclusion pair is refused outright rather than half-checked: got true, expected
//!       false".
//!   S3. FLIP THE BOUNDARY - `<` in place of `<=` in IsClearOfExclusions's distance test, so a
//!       candidate exactly on an exclusion radius reads as clear. Compiled clean (exit 0). The case
//!       then fails on "a candidate exactly on an exclusion radius is NOT clear: got true, expected
//!       false".
//!   S4. MISREAD THE NO-ROAD SENTINEL - `if (distanceToRoad < 0) return NO_SCORE;` deleted from
//!       RoadScore, so -1 scores better than being on a road. Compiled clean (exit 0). The case then
//!       fails on "no road found scores nothing: got 1.005, expected 0".
//!   S5. DROP THE ELEVATION FLOOR - `if (heightAboveObjective <= 0) return NO_SCORE;` deleted from
//!       ElevationScore, so a site BELOW its objective scores a negative and drags the total under
//!       zero. Compiled clean (exit 0). The case then fails on
//!       "a site below its objective scores nothing for elevation rather than a penalty: got
//!       -0.333333, expected 0".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveScaling_FOBSiting : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!CheckBand())
			return true;

		if (!CheckExclusions())
			return true;

		if (!CheckScoring())
			return true;

		if (!CheckLattice())
			return true;

		Print("FOBSiting: the band is inclusive at both ends and a collapsed one accepts nothing; exclusions refuse a ragged pair outright and treat the boundary itself as too close; the score is bounded by its three weights and reads the no-road sentinel as no road; the sampling lattice is deterministic, on-line first, and never runs off either end");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The band, including the degenerate one.
	//! \return True when every row held.
	protected bool CheckBand()
	{
		if (!ExpectBand(600, 400, 900, true, "a candidate in the middle of the band is in it"))
			return false;

		if (!ExpectBand(400, 400, 900, true, "a candidate exactly on the near bound is in the band"))
			return false;

		if (!ExpectBand(900, 400, 900, true, "a candidate exactly on the far bound is in the band"))
			return false;

		if (!ExpectBand(399, 400, 900, false, "a candidate one metre inside the near bound is out"))
			return false;

		if (!ExpectBand(901, 400, 900, false, "a candidate one metre past the far bound is out"))
			return false;

		// 🔴 THE ROW THAT MATTERS MOST, AND IT HAS TO BE THE ONE STANDING ON THE COLLAPSED BOUND. A
		// collapsed band means the supplying base is on top of the objective, or the fractions were
		// misauthored, and accepting anything would site the forward base inside the place it is meant
		// to be advancing on. ⚠ A candidate at any OTHER distance is refused by the two ordinary bound
		// tests whether or not the degenerate guard is there, so only this row can catch its removal.
		if (!ExpectBand(500, 500, 500, false, "a collapsed band accepts nothing even at its own bound"))
			return false;

		if (!ExpectBand(0, 500, 500, false, "a collapsed band accepts nothing away from its bound either"))
			return false;

		if (!ExpectBand(600, 900, 400, false, "an inverted band accepts nothing"))
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The exclusion test, including the empty, ragged and boundary rows.
	//! \return True when every row held.
	protected bool CheckExclusions()
	{
		array<vector> none = {};
		array<float> noRadii = {};

		if (!ExpectClear("100 0 100", none, noRadii, true, "an empty exclusion list excludes nothing"))
			return false;

		if (!ExpectClear("100 0 100", null, null, true, "two null lists exclude nothing"))
			return false;

		// A caller that produced one list and not the other has lost the pairing, and there is no safe
		// way to guess which.
		array<vector> one = {"0 0 0"};
		if (!ExpectClear("100 0 100", one, null, false, "one null list of a pair is refused"))
			return false;

		// ⚠ WRITE THE GEOMETRY OUT BEFORE ADDING A ROW HERE. Two exclusions on the X axis:
		//     A at x=0    with radius 300  -> excludes x in [-300, 300]
		//     B at x=1000 with radius 500  -> excludes x in [ 500, 1500]
		//   so the ONLY clear band between them is x in (300, 500), and the only clear ground outside
		//   them is x < -300 or x > 1500. This row set was first authored with a "between two
		//   exclusions" candidate at x=700, which is 300 m from B and therefore squarely INSIDE it -
		//   the row asserted something the arithmetic makes impossible, and the case went red on
		//   correct production code. The corrected in-gap candidate and the x=700 candidate with its
		//   real answer are BOTH kept below, so the mistake is documented where it was made.
		array<vector> places = {"0 0 0", "1000 0 0"};
		array<float> radii = {300, 500};

		if (!ExpectClear("400 0 0", places, radii, true, "a candidate in the gap between two exclusions and outside both is clear"))
			return false;

		if (!ExpectClear("200 0 0", places, radii, false, "a candidate inside the first exclusion is not clear"))
			return false;

		if (!ExpectClear("600 0 0", places, radii, false, "a candidate inside the second exclusion is not clear"))
			return false;

		// ⚠ THE ROW THAT WAS WRONG, KEPT WITH ITS REAL ANSWER. Being far from the FIRST exclusion says
		// nothing about the second, and the two radii differ - which is exactly the pairing mistake a
		// loop reading radii[j] for exclusions[i] would produce, so the row still earns its place.
		if (!ExpectClear("700 0 0", places, radii, false, "a candidate well clear of the first exclusion but inside the SECOND, larger one is not clear"))
			return false;

		// STRICTLY OUTSIDE. Exactly on the line is too close.
		if (!ExpectClear("300 0 0", places, radii, false, "a candidate exactly on an exclusion radius is NOT clear"))
			return false;

		if (!ExpectClear("301 0 0", places, radii, true, "a candidate one metre outside an exclusion radius is clear"))
			return false;

		// 🔴 The ragged pair. Nothing else in the tree would notice a caller that appended a position
		// without its radius.
		array<vector> ragged = {"0 0 0", "1000 0 0", "2000 0 0"};
		if (!ExpectClear("9000 0 9000", ragged, radii, false, "a ragged exclusion pair is refused outright rather than half-checked"))
			return false;

		array<vector> disabled = {"0 0 0"};
		array<float> zeroRadius = {0};
		if (!ExpectClear("0 0 0", disabled, zeroRadius, true, "an exclusion with no radius excludes nothing, so a place can be left in the list with its rule off"))
			return false;

		array<float> negativeRadius = {-100};
		if (!ExpectClear("0 0 0", disabled, negativeRadius, true, "an exclusion with a negative radius excludes nothing"))
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The three score terms and the sum of them.
	//! \return True when every row held.
	protected bool CheckScoring()
	{
		// --- Flatness. One minus the fraction of the tolerance used up.
		if (!ExpectFlatness(0, 2.5, 1, "perfectly level ground scores the full term"))
			return false;

		if (!ExpectFlatness(1.25, 2.5, 0.5, "half the tolerance scores half the term"))
			return false;

		if (!ExpectFlatness(2.5, 2.5, 0, "exactly at the tolerance scores nothing"))
			return false;

		if (!ExpectFlatness(9, 2.5, 0, "well past the tolerance scores the same as exactly at it, never a negative"))
			return false;

		if (!ExpectFlatness(-1, 2.5, 1, "a negative spread reads as level rather than as better than level"))
			return false;

		if (!ExpectFlatness(3, 0, 1, "with no tolerance the term is disabled rather than dividing by zero"))
			return false;

		// --- Elevation. Only upwards counts.
		if (!ExpectElevation(30, 30, 1, "the full useful gain scores the full term"))
			return false;

		if (!ExpectElevation(15, 30, 0.5, "half the useful gain scores half the term"))
			return false;

		if (!ExpectElevation(90, 30, 1, "more than the useful gain saturates rather than outweighing flatness"))
			return false;

		if (!ExpectElevation(0, 30, 0, "level with the objective scores nothing"))
			return false;

		if (!ExpectElevation(-10, 30, 0, "a site below its objective scores nothing for elevation rather than a penalty"))
			return false;

		if (!ExpectElevation(10, 0, 0, "with no useful gain the term is disabled"))
			return false;

		// --- The road term, and the sentinel that is the whole reason it is a separate function.
		if (!ExpectRoad(0, 1, "a candidate on a road scores the full term"))
			return false;

		if (!ExpectRoad(OVT_FOBSiting.ROAD_USEFUL_DISTANCE * 0.5, 0.5, "half the useful distance scores half the term"))
			return false;

		if (!ExpectRoad(OVT_FOBSiting.ROAD_USEFUL_DISTANCE, 0, "exactly at the useful distance scores nothing"))
			return false;

		if (!ExpectRoad(OVT_FOBSiting.ROAD_USEFUL_DISTANCE + 50, 0, "past the useful distance scores nothing"))
			return false;

		// 🔴 A NEGATIVE MEANS THE SEARCH CAME UP EMPTY, NOT THAT THE SITE IS ON A ROAD.
		if (!ExpectRoad(-1, 0, "no road found scores nothing"))
			return false;

		// --- The sum. Bounded by its weights at both ends, whatever it is handed.
		float best = OVT_FOBSiting.ScoreSite(1, 1, 0);
		float ceiling = OVT_FOBSiting.FLATNESS_WEIGHT + OVT_FOBSiting.ELEVATION_WEIGHT + OVT_FOBSiting.ROAD_WEIGHT;
		if (!ExpectFloat(best, ceiling, "a perfect site scores exactly the sum of the three weights"))
			return false;

		if (!ExpectFloat(OVT_FOBSiting.ScoreSite(0, 0, -1), 0, "a site with nothing going for it scores nothing, never a negative"))
			return false;

		if (!ExpectFloat(OVT_FOBSiting.ScoreSite(5, 5, 0), ceiling, "out-of-range fractions are clamped rather than multiplying the weights"))
			return false;

		if (!ExpectFloat(OVT_FOBSiting.ScoreSite(-5, -5, -1), 0, "negative fractions are clamped to nothing"))
			return false;

		// THE THREE TERMS ARE STRICTLY ORDERED, which is the whole claim the weights make: level ground
		// beats a view, and a view beats being near a road. Asserted as an ordering rather than against
		// the numbers themselves so re-tuning the weights does not have to re-tune the case - only
		// reversing the design's own priorities does.
		float levelOnly = OVT_FOBSiting.ScoreSite(1, 0, -1);
		float elevatedOnly = OVT_FOBSiting.ScoreSite(0, 1, -1);
		float roadOnly = OVT_FOBSiting.ScoreSite(0, 0, 0);

		if (levelOnly <= elevatedOnly)
		{
			SetFailure("level ground must outscore a height advantage - a structure on a slope looks broken and no view makes up for it: level %1, elevated %2",
				levelOnly.ToString(), elevatedOnly.ToString());

			return false;
		}

		if (elevatedOnly <= roadOnly)
		{
			SetFailure("a height advantage must outscore being near a road: elevated %1, road %2",
				elevatedOnly.ToString(), roadOnly.ToString());

			return false;
		}

		if (roadOnly <= 0)
		{
			SetFailure("being on a road must be worth something on its own, or the supply preference does nothing: got %1",
				roadOnly.ToString());

			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The deterministic sampling lattice.
	//! \return True when every row held.
	protected bool CheckLattice()
	{
		// The band is walked from one end to the other, inclusively, so both bounds are actually tried.
		if (!ExpectFloat(OVT_FOBSiting.BandFraction(0, 8), 0, "the first step of the lattice sits on one end of the band"))
			return false;

		if (!ExpectFloat(OVT_FOBSiting.BandFraction(7, 8), 1, "the last step sits on the other end"))
			return false;

		if (!ExpectFloat(OVT_FOBSiting.BandFraction(4, 9), 0.5, "the middle step of an odd lattice sits halfway"))
			return false;

		if (!ExpectFloat(OVT_FOBSiting.BandFraction(-3, 8), 0, "a step below the lattice is clamped rather than sampling behind the band"))
			return false;

		if (!ExpectFloat(OVT_FOBSiting.BandFraction(99, 8), 1, "a step past the lattice is clamped rather than sampling beyond it"))
			return false;

		if (!ExpectFloat(OVT_FOBSiting.BandFraction(3, 1), 0, "a one-step lattice samples one end and does not divide by zero"))
			return false;

		if (!ExpectFloat(OVT_FOBSiting.BandFraction(0, 0), 0, "an empty lattice does not divide by zero"))
			return false;

		// ON THE LINE FIRST. A forward base straight down the supply line is the shortest resupply, so
		// lane 0 has to be the un-offset one.
		if (!ExpectFloat(OVT_FOBSiting.LateralOffset(0, 3), 0, "the first lane sits on the supply line itself"))
			return false;

		if (!ExpectFloat(OVT_FOBSiting.LateralOffset(1, 3), -1, "the second lane steps one spread to one side"))
			return false;

		if (!ExpectFloat(OVT_FOBSiting.LateralOffset(2, 3), 1, "the third lane steps one spread to the other side"))
			return false;

		if (!ExpectFloat(OVT_FOBSiting.LateralOffset(3, 5), -2, "the fourth lane steps two spreads out"))
			return false;

		if (!ExpectFloat(OVT_FOBSiting.LateralOffset(4, 5), 2, "the fifth lane steps two spreads out the other way"))
			return false;

		if (!ExpectFloat(OVT_FOBSiting.LateralOffset(9, 3), 1, "a lane past the lattice is clamped to the last one"))
			return false;

		if (!ExpectFloat(OVT_FOBSiting.LateralOffset(-4, 3), 0, "a negative lane sits on the line"))
			return false;

		if (!ExpectFloat(OVT_FOBSiting.LateralOffset(2, 1), 0, "a single-lane lattice never leaves the line"))
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one band row.
	//! \param[in] distance Distance from the objective.
	//! \param[in] min Nearest bound.
	//! \param[in] max Furthest bound.
	//! \param[in] expected Whether it must be in the band.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectBand(float distance, float min, float max, bool expected, string label)
	{
		return ExpectBool(OVT_FOBSiting.IsInBand(distance, min, max), expected, label);
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one exclusion row.
	//! \param[in] candidate The position judged.
	//! \param[in] exclusions Places to avoid.
	//! \param[in] radii How far from each.
	//! \param[in] expected Whether it must read as clear.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectClear(vector candidate, array<vector> exclusions, array<float> radii, bool expected, string label)
	{
		return ExpectBool(OVT_FOBSiting.IsClearOfExclusions(candidate, exclusions, radii), expected, label);
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one flatness row.
	//! \param[in] spread Metres between the highest and lowest probe.
	//! \param[in] tolerance The spread at which the term reaches zero.
	//! \param[in] expected The fraction expected.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectFlatness(float spread, float tolerance, float expected, string label)
	{
		return ExpectFloat(OVT_FOBSiting.FlatnessScore(spread, tolerance), expected, label);
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one elevation row.
	//! \param[in] gain Metres above the objective.
	//! \param[in] useful The gain at which the term saturates.
	//! \param[in] expected The fraction expected.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectElevation(float gain, float useful, float expected, string label)
	{
		return ExpectFloat(OVT_FOBSiting.ElevationScore(gain, useful), expected, label);
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one road row.
	//! \param[in] distance Metres to the nearest road, or negative for none.
	//! \param[in] expected The fraction expected.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectRoad(float distance, float expected, string label)
	{
		return ExpectFloat(OVT_FOBSiting.RoadScore(distance), expected, label);
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one float, within the tier's epsilon.
	//! \param[in] actual What was produced.
	//! \param[in] expected What was wanted.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectFloat(float actual, float expected, string label)
	{
		if (OVT_TEST_LogicFixture.FloatEquals(actual, expected))
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one boolean row.
	//! \param[in] actual What was produced.
	//! \param[in] expected What was wanted.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectBool(bool actual, bool expected, string label)
	{
		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! The counter-attack may only BEGIN in daylight, and the window is half-open, and it wraps (D17).
//!
//! WHAT THE PREDICATE IS FOR. The user's rule is "no counter-attack at night". It sits on the GATE
//! rather than in the battle layer: a siege that has begun finishes whatever the clock does, and only
//! the decision to begin is time-of-day sensitive. That makes it a pure function of three integers,
//! which is the only shape a clock rule can have and still be asserted without a world.
//!
//! WHY THE BOUNDARIES ARE THE INTERESTING ROWS. The shipped window is 05:00 to 15:00 and the two
//! plausible mistakes are both boundary mistakes: an inclusive end hour silently extends every
//! counter-attack window by an hour into the afternoon, and an exclusive start hour costs the first
//! hour of the morning. Neither shows up anywhere but here - in play, "the counter-attack started at
//! 15:20 once" is not a reproducible observation.
//!
//! ⚠ THE WRAPPING WINDOW IS COVERED EVEN THOUGH NOTHING SHIPS ONE. The shipped bounds do not wrap, so
//! every wrapping row here is dead in the current build - and that is exactly why it is asserted now.
//! The bounds are two consts precisely so an operator can move them, and the first person to author
//! 22 -> 04 will get either a correct answer or a window that can never open, with nothing to tell
//! them which. Four rows on both sides of both its edges.
//!
//! ⚠ THE TWO "NO RESTRICTION" ROWS ANSWER TRUE, WHICH IS THE FAIL-OPEN DIRECTION, AND THAT IS
//! DELIBERATE. A zero-width window and an out-of-range bound are both unauthorable through the
//! shipped consts, so the only way to reach them is a future edit - and failing CLOSED on a bad window
//! would stop the occupying faction counter-attacking at all, which is the one symptom this whole
//! feature exists to end and the last one anybody would trace back to a bounds typo.
//!
//! CAN-FAIL, four faults, injected one at a time and compiled. All four exited
//! tools/compile-check.sh with 0:
//!   W1. MAKE THE END INCLUSIVE - `hour <= endHour`. Compiled clean (exit 0). The case fails on
//!       "15:00 is the first hour OUTSIDE the shipped window: got true, expected false".
//!   W2. MAKE THE START EXCLUSIVE - `hour > startHour`. Compiled clean (exit 0). The case fails on
//!       "05:00 is the first hour INSIDE the shipped window: got false, expected true".
//!   W3. DELETE THE WRAP BRANCH - return the ordinary comparison in every case. Compiled clean (exit
//!       0). ⚠ Every shipped row still passes, which is the whole reason the wrapping rows exist; the
//!       case fails on "a window authored 22 -> 04 must be open at 23:00: got false, expected true".
//!   W4. FAIL CLOSED ON A ZERO-WIDTH WINDOW - `return false` for start == end. Compiled clean (exit 0).
//!       The case fails on "a zero-width window cannot be an intent and must not stop the campaign:
//!       got false, expected true".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_ObjectiveScaling_CounterAttackWindow : SCR_AutotestCaseBase
{
	//! The shipped bounds, restated here rather than read off the director: this tier may not resolve a
	//! component, and a case that read the production consts could not catch one of them being changed.
	protected const int SHIPPED_START = 5;
	protected const int SHIPPED_END = 15;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- Inside the shipped window, well away from either edge.
		if (!ExpectWindow(10, SHIPPED_START, SHIPPED_END, true, "mid-morning is inside the shipped window"))
			return true;

		// --- BOTH BOUNDARIES, which is what "half-open" means and the only place it can be seen.
		if (!ExpectWindow(5, SHIPPED_START, SHIPPED_END, true, "05:00 is the first hour INSIDE the shipped window"))
			return true;

		if (!ExpectWindow(4, SHIPPED_START, SHIPPED_END, false, "04:00 is the last hour before the shipped window opens"))
			return true;

		if (!ExpectWindow(14, SHIPPED_START, SHIPPED_END, true, "14:00 is the last hour a counter-attack may start"))
			return true;

		if (!ExpectWindow(15, SHIPPED_START, SHIPPED_END, false, "15:00 is the first hour OUTSIDE the shipped window"))
			return true;

		// --- Outside it, both ends of the day, including the hour the user's rule is really about.
		if (!ExpectWindow(0, SHIPPED_START, SHIPPED_END, false, "midnight is outside the shipped window"))
			return true;

		if (!ExpectWindow(23, SHIPPED_START, SHIPPED_END, false, "the last hour of the day is outside the shipped window"))
			return true;

		if (!ExpectWindow(18, SHIPPED_START, SHIPPED_END, false, "the evening is outside the shipped window"))
			return true;

		// --- A WRAPPING WINDOW, 22 -> 04, on both sides of both its edges. Nothing ships this; the
		//     bounds are consts an operator may move, and a wrap that silently never opened would be
		//     indistinguishable from the occupying faction having stopped attacking.
		if (!ExpectWindow(22, 22, 4, true, "a window authored 22 -> 04 must open exactly at 22:00"))
			return true;

		if (!ExpectWindow(21, 22, 4, false, "a window authored 22 -> 04 must still be shut at 21:00"))
			return true;

		if (!ExpectWindow(23, 22, 4, true, "a window authored 22 -> 04 must be open at 23:00"))
			return true;

		if (!ExpectWindow(0, 22, 4, true, "a window authored 22 -> 04 must carry across midnight"))
			return true;

		if (!ExpectWindow(3, 22, 4, true, "03:00 is the last hour inside a 22 -> 04 window"))
			return true;

		if (!ExpectWindow(4, 22, 4, false, "04:00 is the first hour outside a 22 -> 04 window"))
			return true;

		if (!ExpectWindow(12, 22, 4, false, "midday is outside a 22 -> 04 window"))
			return true;

		// --- The two ways of saying "no restriction". Both answer TRUE - see the header for why failing
		//     closed here would be the worse bug.
		if (!ExpectWindow(3, 7, 7, true, "a zero-width window cannot be an intent and must not stop the campaign"))
			return true;

		if (!ExpectWindow(3, -1, 15, true, "a start hour outside 0-23 is not a window anyone authored"))
			return true;

		if (!ExpectWindow(3, 5, 24, true, "an end hour outside 0-23 is not a window anyone authored"))
			return true;

		// --- A full day against the shipped window, so the count of open hours is exactly ten. A fault
		//     that shifted the whole window by an hour would pass several rows above and fail here.
		int openHours = 0;
		for (int hour = 0; hour < 24; hour++)
		{
			if (OVT_ObjectivePhaseRules.IsCounterAttackWindow(hour, SHIPPED_START, SHIPPED_END))
				openHours++;
		}

		if (openHours != 10)
		{
			SetFailure("the shipped 05:00-15:00 window must be open for exactly ten hours of the day, but it is open for %1",
				openHours.ToString());
			return true;
		}

		Print("Counter-attack window: half-open at both ends, ten hours wide as shipped, correct across midnight when an operator authors a wrapping window, and never so broken that the occupying faction stops attacking altogether");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one hour against one window.
	//! \param[in] hour The world clock's hour.
	//! \param[in] startHour First hour the window is open, inclusive.
	//! \param[in] endHour First hour it is closed again, exclusive.
	//! \param[in] expected What the predicate must answer.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectWindow(int hour, int startHour, int endHour, bool expected, string label)
	{
		bool actual = OVT_ObjectivePhaseRules.IsCounterAttackWindow(hour, startHour, endHour);

		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}
}
