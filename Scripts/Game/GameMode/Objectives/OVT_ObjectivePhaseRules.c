//------------------------------------------------------------------------------------------------
//! PURE STATICS - the objective director's phase arithmetic. NO WORLD, NO MANAGER, NO ENTITY.
//!
//! THE HARD RULE, stated once and inherited from OVT_DeploymentSelection.c:4-6: every function here
//! is a function of its arguments and nothing else. It never resolves a manager, never reads a
//! config, never touches an entity and never looks at a clock. Anything the caller already knows is
//! passed IN as a number - that is what makes the whole phase machine assertable in the cheapest
//! test tier, which is the entire point of splitting it out.
//!
//! WHAT LIVES HERE NOW. occupying/counter-attacks Phase 1 retired the old random counter-attack and
//! authored the twelve difficulty knobs that replace it, so the two rules that CONSUME those knobs
//! landed with them: how many sabotage missions a base objective owes before it may be counter-attacked,
//! and which rung of the harassment group ladder a given success count buys. Phase 2 added the rest -
//! the three phase gates, the starvation predicate, the FOB budget ceiling and the tick-down - so the
//! whole progression of the objective machine is now decidable from integers alone.
//!
//! ⚠ EVERY DURATION THE MACHINE TRACKS IS A TICK COUNT, NEVER A DEADLINE (D4). TickDown() below is
//! the only way any of them moves. That is what makes "all objective timers freeze while a battle is
//! live" a property of an early return rather than a rule someone has to remember to apply to each
//! timer, and it is why the whole progression is assertable here with no clock in sight.
//------------------------------------------------------------------------------------------------
class OVT_ObjectivePhaseRules
{
	//! Fallback used when BOTH the authored value and the caller's fallback are unusable. Chosen to
	//! match the declared default of objectiveSabotageMissionsRequired so a broken config plays like
	//! an unauthored one rather than like a free win.
	static const int DEFAULT_SABOTAGE_MISSIONS = 4;

	//! Anything above this is treated as a typo (a mis-keyed 40 for 4), not as a design intent: at one
	//! mission per harassment interval it would take an in-game week to reach Phase 3, which reads in
	//! play as "the occupying faction never counter-attacks" and has no other diagnosis.
	static const int MAX_SABOTAGE_MISSIONS = 20;

	//! Returned by HarassmentLadderIndex when there is no ladder to index into. Deliberately NOT 0:
	//! zero is a valid rung, and handing a caller "rung 0" of an empty ladder converts a config fault
	//! into an out-of-range read at the registry.
	static const int NO_LADDER_RUNG = -1;

	//! Support percentage a town objective must be driven BELOW before the forward operating base is
	//! raised. Half the town: the harassment ramp has visibly worked, but the town is still worth
	//! defending, so the resistance gets its warning while it can still act on it.
	static const int TOWN_FOB_SUPPORT_THRESHOLD = 50;

	//! Completed sabotage missions a BASE objective owes before the forward operating base is raised.
	//! One: the base equivalent of "the harassment ramp has visibly worked". See BasePhase2Gate for why
	//! this needs no companion test and the town gate does.
	static const int BASE_FOB_SABOTAGE_THRESHOLD = 1;

	//! Support percentage a town objective must be driven BELOW before the counter-attack may fire.
	//! A quarter of the town: this is the point at which the occupying faction judges it can hold the
	//! place once it has taken it.
	static const int TOWN_QRF_SUPPORT_THRESHOLD = 25;

	//! The FOB spend ceiling, as a multiple of one FOB's cost. Derived rather than authored (§3.6):
	//! a separate difficulty knob nobody ever tunes is a knob that goes stale, and the ceiling is only
	//! ever meaningful relative to what the structure itself cost.
	static const int FOB_CEILING_MULTIPLIER = 3;

	//------------------------------------------------------------------------------------------------
	//! How many completed sabotage missions a base objective owes before Phase 3 may fire.
	//!
	//! The authored value is INVERTED across the difficulty presets on purpose (G11/D10): Easy demands
	//! six, Insane demands two, so a new player gets more warning. That inversion is authored in the
	//! .conf files and is NOT recomputed here - this function only decides whether to trust what it
	//! was handed.
	//!
	//! \param[in] authored The difficulty setting's value. 0, negative or absurd means unusable.
	//! \param[in] fallback The value to use instead; if it is unusable too, DEFAULT_SABOTAGE_MISSIONS.
	//! \return A mission count of at least 1 and at most MAX_SABOTAGE_MISSIONS.
	static int RequiredSabotageMissions(int authored, int fallback)
	{
		if (authored >= 1 && authored <= MAX_SABOTAGE_MISSIONS)
			return authored;

		if (fallback >= 1 && fallback <= MAX_SABOTAGE_MISSIONS)
			return fallback;

		return DEFAULT_SABOTAGE_MISSIONS;
	}

	//------------------------------------------------------------------------------------------------
	//! Which rung of the harassment group ladder the next operation draws from.
	//!
	//! The ladder is the existing faction group registry walked in ascending size
	//! (light_patrol -> light_fireteam -> rifle_squad -> heavy_infantry), which is why this needs no
	//! new registry entries. One success buys one rung; the ramp SATURATES at the top rung rather than
	//! wrapping or running off the end, so a long-running objective keeps sending the biggest group it
	//! has instead of starting over with a two-man patrol.
	//!
	//! \param[in] successes Completed operations at this objective. Negative is treated as none.
	//! \param[in] rungs Number of entries in the ladder. Zero or fewer yields NO_LADDER_RUNG.
	//! \return A rung index in [0, rungs - 1], or NO_LADDER_RUNG when there is no ladder.
	static int HarassmentLadderIndex(int successes, int rungs)
	{
		if (rungs <= 0)
			return NO_LADDER_RUNG;

		if (successes <= 0)
			return 0;

		if (successes >= rungs)
			return rungs - 1;

		return successes;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a TOWN objective has been softened enough to raise the forward operating base.
	//!
	//! STRICTLY BELOW the threshold, so a town sitting exactly on it has not qualified yet. The gate is
	//! crossed by the harassment ramp pushing support DOWN, so the strict test means the last debuff
	//! has to actually land rather than merely reach the line.
	//! \param[in] supportPercentage The town's current support for the resistance, 0-100.
	//! \return True when the objective may advance out of harassment.
	static bool TownPhase2Gate(int supportPercentage)
	{
		return supportPercentage < TOWN_FOB_SUPPORT_THRESHOLD;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a BASE objective has been worked on enough to raise the forward operating base.
	//!
	//! ⚠ ONE COMPLETED MISSION, WHICH IS THE WHOLE GATE, AND THERE IS DELIBERATELY NO SECOND CONJUNCT
	//! HERE - unlike the town gate, whose caller adds one. The town gate needs the extra "this ramp did
	//! it" test because a town can ALREADY be under its support threshold at the moment it is chosen,
	//! so the gate could fire on the phase's own entry tick and skip the harassment phase outright. A
	//! base objective cannot: the counter starts at zero on every commit and only a completed sabotage
	//! mission raises it, so the entry tick always refuses and the phase is always actually played.
	//!
	//! MORE THAN ONE IS THE PHASE 3 GATE'S JOB, not this one's. The counter-attack demands
	//! objectiveSabotageMissionsRequired (six on Easy, two on Insane - see RequiredSabotageMissions);
	//! this gate only asks whether the ramp has proved it can reach the base at all, which is what the
	//! forward base is then built to sustain.
	//! \param[in] successes Completed sabotage missions at this objective.
	//! \return True when the objective may advance out of harassment.
	static bool BasePhase2Gate(int successes)
	{
		return successes >= BASE_FOB_SABOTAGE_THRESHOLD;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a TOWN objective may fire its counter-attack.
	//!
	//! THREE CONJUNCTS, ALL REQUIRED, and the last two are what stop the battle starting on credit:
	//! the forward operating base must be standing (it is the wave source and the whole point of the
	//! middle phase), and the reserve must actually cover the gate. Every one of them is a separate
	//! reason to WAIT, never a reason to abandon the objective.
	//!
	//! ⚠ THE DAYLIGHT WINDOW IS NOT ONE OF THEM. This class may not read a clock, so the caller reads the
	//! hour and conjoins IsCounterAttackWindow() itself (D17). Keeping it out of here is not tidiness:
	//! being outside the window is a reason to WAIT and costs the objective nothing, whereas every
	//! conjunct below is a reason the ramp is not finished yet.
	//! \param[in] supportPercentage The town's current support for the resistance, 0-100.
	//! \param[in] fobUp Whether the forward operating base is standing.
	//! \param[in] reserve The occupying faction's reserve resources.
	//! \param[in] gate The reserve the difficulty setting demands. Zero or negative means no gate.
	//! \return True when every condition this class can see is met.
	static bool TownPhase3Gate(int supportPercentage, bool fobUp, int reserve, int gate)
	{
		if (!fobUp)
			return false;

		if (supportPercentage >= TOWN_QRF_SUPPORT_THRESHOLD)
			return false;

		return MeetsResourceGate(reserve, gate);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a BASE objective may fire its counter-attack.
	//!
	//! The same three conjuncts as the town gate, with completed sabotage missions in place of
	//! collapsed support. The required count is the INVERTED difficulty value - see
	//! RequiredSabotageMissions, which the caller resolves first.
	//! \param[in] successes Completed sabotage missions at this objective.
	//! \param[in] required How many the difficulty demands, already clamped.
	//! \param[in] fobUp Whether the forward operating base is standing.
	//! \param[in] reserve The occupying faction's reserve resources.
	//! \param[in] gate The reserve the difficulty setting demands. Zero or negative means no gate.
	//! \return True when every condition this class can see is met.
	static bool BasePhase3Gate(int successes, int required, bool fobUp, int reserve, int gate)
	{
		if (!fobUp)
			return false;

		// A non-positive requirement would mean "counter-attack with no sabotage at all", which is the
		// unpredictability this feature exists to end. RequiredSabotageMissions() already refuses to
		// produce one; this is the second line of defence for a caller that skipped it.
		int demanded = required;
		if (demanded < 1)
			demanded = DEFAULT_SABOTAGE_MISSIONS;

		if (successes < demanded)
			return false;

		return MeetsResourceGate(reserve, gate);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the world clock is inside the window a counter-attack may BEGIN in (D17).
	//!
	//! ⚠ IT GATES THE DECISION TO START, NEVER THE BATTLE ITSELF. A counter-attack that has begun runs
	//! to its conclusion whatever the clock does; only the caller's gate consults this. That is what
	//! makes it a pure hour predicate rather than something the battle layer has to keep asking.
	//!
	//! HALF-OPEN, [start, end). The start hour is inside the window and the end hour is outside it, so
	//! the shipped 5..15 means "from 05:00:00 up to but not including 15:00:00" - the last minute a
	//! counter-attack may start is 14:59. Half-open is what makes two adjacent windows tile without
	//! overlapping, and it is the same convention every other range in this class uses.
	//!
	//! ⚠ IT HANDLES A WINDOW THAT CROSSES MIDNIGHT even though the shipped values do not. An operator who
	//! later authors 22 -> 4 gets the four hours either side of midnight, not a silent never - a window
	//! expressed as start > end is unambiguous and refusing it would be a trap rather than a validation.
	//!
	//! TWO WAYS TO SAY "NO RESTRICTION", both of which answer TRUE rather than failing closed:
	//!   - an hour outside 0-23 in either bound is not a window anyone authored, and
	//!   - a zero-width window (start == end) cannot be a real intent, because a half-open [5, 5) can
	//!     never contain anything.
	//! Failing closed on either would stop the occupying faction ever counter-attacking, which is the
	//! one symptom this feature exists to end and the one nobody would diagnose as a config fault.
	//! \param[in] hour The world clock's hour, 0-23.
	//! \param[in] startHour First hour the window is open, inclusive.
	//! \param[in] endHour First hour the window is closed again, exclusive.
	//! \return True when a counter-attack may begin now.
	static bool IsCounterAttackWindow(int hour, int startHour, int endHour)
	{
		if (startHour < 0 || startHour > 23 || endHour < 0 || endHour > 23)
			return true;

		if (startHour == endHour)
			return true;

		if (startHour < endHour)
			return hour >= startHour && hour < endHour;

		// The window crosses midnight: it is open from the start hour to the end of the day, and again
		// from the start of the day to the end hour.
		return hour >= startHour || hour < endHour;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the reserve covers the counter-attack's resource gate.
	//!
	//! AT the gate passes: the gate is what the battle is expected to cost, and demanding a margin on
	//! top of it would make the authored number mean something other than what it says.
	//! \param[in] reserve The occupying faction's reserve resources.
	//! \param[in] gate The reserve demanded. Zero or negative means the difficulty gates nothing.
	//! \return True when the counter-attack is affordable.
	static bool MeetsResourceGate(int reserve, int gate)
	{
		if (gate <= 0)
			return true;

		return reserve >= gate;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the forward operating base is cut off this tick.
	//!
	//! THREE INDEPENDENT FAILURES, ANY ONE OF WHICH STARVES IT: the base that supplies it has been
	//! taken, its own garrison is dead, or the resistance is standing on it. Each is separately
	//! sufficient because each separately means the same thing - nothing more is arriving.
	//!
	//! ⚠ THIS ANSWERS ABOUT ONE TICK, NOT ABOUT THE OUTCOME. Starvation is only fatal after it has
	//! held for objectiveStarvationMinutes, and recovery on any tick zeroes that count; the caller owns
	//! the accumulation because only the caller knows how long a tick is.
	//! \param[in] sourceHeld Whether the supplying base is still held by the same faction.
	//! \param[in] aliveGroups Registered groups still alive at the forward base.
	//! \param[in] playerPresent Whether an enemy player is inside the close range.
	//! \return True when the forward base is cut off this tick.
	static bool IsFOBStarved(bool sourceHeld, int aliveGroups, bool playerPresent)
	{
		if (!sourceHeld)
			return true;

		if (aliveGroups < 1)
			return true;

		return playerPresent;
	}

	//------------------------------------------------------------------------------------------------
	//! The spend ceiling for one forward operating base.
	//!
	//! ⚠ THIS IS A CEILING, NOT A WALLET. Nothing is reserved, held or moved by it: the director counts
	//! what it has already spent from the ONE deployment pool against this number and stops. The
	//! conserved-total identity the base-defense migration established therefore still holds - see G5.
	//! \param[in] fobCost The difficulty's objectiveFOBCost. Non-positive yields a zero ceiling, which
	//! WithinFOBCeiling() then refuses everything against.
	//! \return The most that may be spent at the forward base, including the structure itself.
	static int FOBBudgetCeiling(int fobCost)
	{
		if (fobCost <= 0)
			return 0;

		return fobCost * FOB_CEILING_MULTIPLIER;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a running total is still inside the forward base's spend ceiling.
	//!
	//! INCLUSIVE AT THE CEILING, because the caller asks with the prospective spend already added:
	//! "would spending this take me past the ceiling". Landing exactly on it is spending the budget,
	//! not exceeding it. A zero or negative ceiling refuses everything, which is what makes a
	//! misauthored objectiveFOBCost fail closed rather than fund an unbounded ramp.
	//! \param[in] spent The total that would have been spent.
	//! \param[in] ceiling The ceiling from FOBBudgetCeiling().
	//! \return True when the spend is permitted.
	static bool WithinFOBCeiling(int spent, int ceiling)
	{
		if (ceiling <= 0)
			return false;

		return spent <= ceiling;
	}

	//------------------------------------------------------------------------------------------------
	//! Serves one tick of a countdown.
	//!
	//! FLOORED AT ZERO. Every timer in the machine is a plain integer decremented once per tick and
	//! nothing stores a deadline (D4), so this one line is the whole of the machine's sense of time -
	//! which is exactly why "the objective froze while a battle was live" is provable without a clock.
	//! \param[in] value Ticks remaining.
	//! \return One fewer, never below zero.
	static int TickDown(int value)
	{
		if (value <= 0)
			return 0;

		return value - 1;
	}
}
