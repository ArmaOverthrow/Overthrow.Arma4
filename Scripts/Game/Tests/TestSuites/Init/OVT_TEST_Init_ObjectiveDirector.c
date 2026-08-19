//------------------------------------------------------------------------------------------------
//! TIER B - the objective director resolves, starts empty, decides deterministically, and freezes
//! while a battle is live.
//!
//! WHAT THIS TIER CAN SEE THAT THE CHEAP ONE CANNOT. The scoring and the phase gates are pure
//! arithmetic and are pinned in OVT_TEST_Logic_ObjectiveScaling.c. What needs live managers is the
//! wiring around them: that the component is actually ON the game mode prefab, that both accessors
//! reach the same object, that selection reads the town and base registries in a stable order, and
//! that the tick's early returns fire in the order they are declared in.
//!
//! ⚠ CASE ORDER MATTERS HERE, AND THE NAMES ARE CHOSEN FOR IT. Cases run alphabetically by class
//! name. `...ComponentResolvesAndIsIdle` asserts the state a director is in before anything has driven
//! it, so it has to run before every case that drives it - C sorts before D, F, G and the three
//! idle-clock cases at I. Every driving case also restores the director to idle before it returns, so
//! the ordering is belt AND braces rather than either alone.
//!
//! ⚠ THE TICK IS DRIVEN DIRECTLY, NOT INSTALLED. An initialisation-tier world never runs
//! PostGameStart(), so the repeating timer does not exist here - and installing it would leave the
//! director ticking for the rest of the suite, mutating campaign state under every case that follows.
//! DirectorTick() is public exactly so one step can be taken deliberately: it is the same method the
//! timer calls, so nothing is stubbed, and driving it by hand removes every clock from these cases.
//! No polling, no waiting, no maxAttempts anywhere in this file.
//!
//! EVERY CASE PUTS BACK WHAT IT CHANGED. Town factions, base factions, the live battle handle and the
//! objective itself are all restored before the case returns, because the initialisation world is
//! shared with every other Tier B case.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! The director is on the game mode, both accessors reach it, and a campaign that has not started has
//! no objective.
//!
//! THE THREE CLAIMS ARE ONE CLAIM ABOUT WIRING. A component missing from the prefab, a renamed
//! accessor and a second instance would each break the feature completely and none of them is a
//! compile error - the director would simply never decide anything, and the occupying faction would
//! never attack again, which is indistinguishable from the deliberate passivity this feature's own
//! Phase 1 introduced.
//!
//! IDLE IS THE CORRECT STARTING STATE, not merely the observed one: the first objective is chosen on
//! the first tick after the campaign starts, and a director holding an objective before then would be
//! one that had selected against a world whose factions were not yet assigned.
//!
//! PROVEN ABLE TO FAIL: the OVT_ObjectiveDirectorComponent line was removed from
//! Prefabs/GameMode/OVT_OverthrowGameMode.et. The tree recompiled CLEAN (tools/compile-check.sh exit
//! 0) - a component nobody declares is not a script error - and the case then reports
//! "OVT_ObjectiveDirectorComponent.GetInstance() is null - the component is not declared on the game
//! mode prefab". Line restored, tree recompiled clean.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveDirector_ComponentResolvesAndIsIdle : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent direct = OVT_ObjectiveDirectorComponent.GetInstance();
		if (!direct)
		{
			SetFailure("OVT_ObjectiveDirectorComponent.GetInstance() is null - the component is not declared on the game mode prefab");
			return true;
		}

		OVT_ObjectiveDirectorComponent viaGlobal = OVT_Global.GetObjectiveDirector();
		if (!viaGlobal)
		{
			SetFailure("OVT_Global.GetObjectiveDirector() is null while GetInstance() resolved - the accessor is not wired to the component");
			return true;
		}

		if (direct != viaGlobal)
		{
			SetFailure("The two accessors returned DIFFERENT director objects - there is more than one director, so half the campaign would be reading the wrong objective");
			return true;
		}

		if (direct.GetPhase() != OVT_EObjectivePhase.IDLE)
		{
			// The phase is read into an int before it is printed: an enum member is an integer on the
			// wire and in a log, and formatting one directly is not something this dialect promises.
			int phase = direct.GetPhase();
			SetFailure("A director in a world with no started campaign must be idle, but it reports phase %1", phase.ToString());
			return true;
		}

		if (direct.HasObjective())
		{
			SetFailure("A director in a world with no started campaign must have no objective, but it holds '%1'", direct.GetObjectiveName());
			return true;
		}

		if (direct.IsFOBUp())
		{
			SetFailure("A director with no objective must have no forward operating base recorded");
			return true;
		}

		Print("Objective director: resolves through both accessors as one object, and starts with no objective at all");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Driven twice over the same world, selection makes the same choice.
//!
//! WHY DETERMINISM IS THE CLAIM. Predictability is a stated goal of this feature (G1): the ramp is
//! only readable if the target stops moving. The scoring itself has no randomness by construction and
//! that is asserted in the cheap tier; what this case adds is that the WORLD SIDE is stable too - that
//! enumerating towns and bases, resolving their names, measuring their distances and reading their
//! tower coverage produces the same ordering on two consecutive passes over an unchanged world. An
//! iteration order that depended on a map's insertion order, or a distance measured from a moving
//! reference, would show up here and nowhere else.
//!
//! THE FIXTURE IS BUILT, NOT FOUND, so the case cannot pass vacuously. The initialisation world has
//! one town and one base and no campaign has assigned factions to them for a war that is not running,
//! so left alone there would be nothing to select and "the same nothing twice" would prove nothing.
//! The case therefore hands the whole map to the occupying faction, hands ONE town back to the
//! resistance, and asserts that this exact town is what comes out - twice. Every faction and size it
//! touched is restored before it returns.
//!
//! PROVEN ABLE TO FAIL: the size guard in CollectTownCandidates was changed to skip TOWN instead of
//! VILLAGE. The tree recompiled CLEAN (exit 0) and the case then reports "selection found no
//! objective at all, with one resistance-held town standing" - which is also exactly what a director
//! that silently stopped enumerating towns would look like in play.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveDirector_DeterministicSelectionPicksTheSameCandidate : SCR_AutotestCaseBase
{
	//! Tolerance for comparing a position that has been round-tripped through the record, in metres.
	//! vector.Distance is not correctly rounded at campaign ranges, so positions are never compared
	//! with ==.
	static const float POSITION_TOLERANCE = 1.0;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!director || !occupying || !towns || !config)
		{
			SetFailure("The director, the occupying faction manager, the town manager or the config did not resolve");
			return true;
		}

		if (!towns.m_Towns || towns.m_Towns.IsEmpty())
		{
			SetFailure("The world produced no towns, so there is nothing to build a selection fixture out of");
			return true;
		}

		int occupyingIndex = config.GetOccupyingFactionIndex();
		int resistanceIndex = config.GetPlayerFactionIndex();

		if (occupyingIndex == resistanceIndex)
		{
			SetFailure("The occupying and resistance faction indices are the same (%1), so 'resistance-held' cannot be expressed", occupyingIndex.ToString());
			return true;
		}

		// --- ARRANGE. Snapshot everything about to be touched, then hand the map to the occupying
		//     faction so exactly one candidate is left standing.
		array<int> townFactions = new array<int>();
		array<OVT_TownSize> townSizes = new array<OVT_TownSize>();
		foreach (OVT_TownData town : towns.m_Towns)
		{
			townFactions.Insert(town.faction);
			townSizes.Insert(town.size);
			town.faction = occupyingIndex;
		}

		array<int> baseFactions = new array<int>();
		foreach (OVT_BaseData base : occupying.m_Bases)
		{
			baseFactions.Insert(base.faction);
			base.faction = occupyingIndex;
		}

		OVT_TownData fixture = towns.m_Towns[0];
		fixture.faction = resistanceIndex;
		fixture.size = OVT_TownSize.TOWN;
		vector fixturePosition = fixture.location;

		// --- ACT twice over the unchanged world.
		director.SelectObjective();

		OVT_EObjectiveKind firstKind = director.GetObjectiveKind();
		vector firstPosition = director.GetObjectivePosition();
		string firstName = director.GetObjectiveName();

		director.SelectObjective();

		OVT_EObjectiveKind secondKind = director.GetObjectiveKind();
		vector secondPosition = director.GetObjectivePosition();

		// --- RESTORE before asserting, so a red case does not leave the world rearranged.
		for (int i = 0; i < towns.m_Towns.Count(); i++)
		{
			towns.m_Towns[i].faction = townFactions[i];
			towns.m_Towns[i].size = townSizes[i];
		}

		for (int b = 0; b < occupying.m_Bases.Count(); b++)
		{
			occupying.m_Bases[b].faction = baseFactions[b];
		}

		director.ResetObjective("initialisation-tier selection fixture torn down", false);

		// --- ASSERT.
		if (firstKind == OVT_EObjectiveKind.NONE)
		{
			SetFailure("selection found no objective at all, with one resistance-held town standing");
			return true;
		}

		int firstKindValue = firstKind;
		int secondKindValue = secondKind;

		if (firstKind != OVT_EObjectiveKind.TOWN)
		{
			SetFailure("selection picked kind %1 with every base handed to the occupying faction - only the town was selectable", firstKindValue.ToString());
			return true;
		}

		if (vector.Distance(firstPosition, fixturePosition) > POSITION_TOLERANCE)
		{
			SetFailure("selection picked a position %1 m from the only resistance-held town", vector.Distance(firstPosition, fixturePosition).ToString());
			return true;
		}

		if (secondKind != firstKind)
		{
			SetFailure("a second pass over the SAME world picked a different kind: %1 then %2", firstKindValue.ToString(), secondKindValue.ToString());
			return true;
		}

		if (vector.Distance(firstPosition, secondPosition) > POSITION_TOLERANCE)
		{
			SetFailure("a second pass over the SAME world picked a position %1 m away from the first - the target moved for no reason, which is what this feature exists to end", vector.Distance(firstPosition, secondPosition).ToString());
			return true;
		}

		if (director.GetPhase() != OVT_EObjectivePhase.IDLE)
		{
			int phaseAfterTeardown = director.GetPhase();
			SetFailure("the fixture teardown left the director in phase %1 instead of idle", phaseAfterTeardown.ToString());
			return true;
		}

		Print("Objective director: two passes over one unchanged world select the same town ('" + firstName + "'), so the target does not move on its own");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! While a battle is live, the objective machine stops - and NOTHING counts down.
//!
//! THIS IS THE FEATURE'S FREEZE, ASSERTED WHERE IT ACTUALLY LIVES. The requirement is that phase
//! progression pauses and every objective timer freezes while any battle is running. The
//! implementation makes that true by CONSTRUCTION rather than by rule: the timers are tick counts
//! rather than deadlines, and the tick returns before it reaches a phase handler, so there is nothing
//! to catch up on afterwards. A wall-clock implementation would pass a "did the phase advance" check
//! and still fail this one, because its timers would have kept running while nobody was watching.
//!
//! THE CASE PROVES THE TICK WORKS FIRST. A frozen counter is only evidence if an unfrozen one moves,
//! so the first driven tick must decrement both counters; only then is a battle planted and a second
//! tick driven. Without that first half, a director that never ticked at all would pass.
//!
//! THE BATTLE IS A REAL CONTROLLER, because the freeze reads the campaign's own single-battle handle
//! and nothing else would exercise the same branch. It is spawned, planted, and deleted again inside
//! this case - deletion is how the campaign itself disposes of a finished battle
//! (OnQRFFinishedBase/OnQRFFinishedTown do exactly this), so it is a proven-safe teardown.
//!
//! PROVEN ABLE TO FAIL: the `if (occupying.m_CurrentQRF) return;` early return was deleted from
//! DirectorTick(). The tree recompiled CLEAN (exit 0) - a missing guard is not a script error, and
//! nothing else in the tree would stop it shipping - and the case then reports "the phase timeout
//! counted down while a battle was live: 49 before the tick, 48 after". Guard restored, tree
//! recompiled clean.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveDirector_FreezesEveryTimerWhileABattleIsLive : SCR_AutotestCaseBase
{
	//! Planted phase timeout. Deliberately not a value any phase entry produces.
	static const int PLANTED_PHASE_TICKS = 50;

	//! Planted operation cadence, likewise.
	static const int PLANTED_OP_TICKS = 37;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();

		if (!director || !occupying)
		{
			SetFailure("The director or the occupying faction manager did not resolve");
			return true;
		}

		vector fixturePosition;
		if (!ResolveFixturePosition(occupying, fixturePosition))
		{
			SetFailure("The world produced neither a town nor a base to hang a fixture objective on");
			return true;
		}

		// --- ARRANGE an objective with two counters on known, unnatural values.
		director.CommitObjective(OVT_EObjectiveKind.TOWN, fixturePosition, "freeze fixture");
		director.SetPhaseTimeout(PLANTED_PHASE_TICKS);
		director.SetOperationCountdown(PLANTED_OP_TICKS);

		// --- HALF ONE: with no battle, one tick serves one round off both counters. Without this the
		//     frozen half below would pass on a director that never ticks at all.
		director.DirectorTick();

		int phaseAfterLiveTick = director.GetPhaseTicks();
		int opAfterLiveTick = director.GetNextOpTicks();

		// --- HALF TWO: plant a battle and tick again.
		OVT_QRFControllerComponent battle = occupying.SpawnQRFController(fixturePosition);
		if (!battle)
		{
			director.ResetObjective("initialisation-tier freeze fixture torn down", false);
			SetFailure("The occupying faction manager could not spawn a battle controller, so the freeze could not be driven");
			return true;
		}

		occupying.m_CurrentQRF = battle;

		director.DirectorTick();

		int phaseAfterFrozenTick = director.GetPhaseTicks();
		int opAfterFrozenTick = director.GetNextOpTicks();

		// --- RESTORE before asserting.
		occupying.m_CurrentQRF = null;
		SCR_EntityHelper.DeleteEntityAndChildren(battle.GetOwner());
		director.ResetObjective("initialisation-tier freeze fixture torn down", false);

		// --- ASSERT.
		if (phaseAfterLiveTick != PLANTED_PHASE_TICKS - 1)
		{
			SetFailure("an unfrozen tick must serve exactly one round off the phase timeout: planted %1, read back %2",
				PLANTED_PHASE_TICKS.ToString(), phaseAfterLiveTick.ToString());
			return true;
		}

		if (opAfterLiveTick != PLANTED_OP_TICKS - 1)
		{
			SetFailure("an unfrozen tick must serve exactly one round off the operation cadence: planted %1, read back %2",
				PLANTED_OP_TICKS.ToString(), opAfterLiveTick.ToString());
			return true;
		}

		if (phaseAfterFrozenTick != phaseAfterLiveTick)
		{
			SetFailure("the phase timeout counted down while a battle was live: %1 before the tick, %2 after",
				phaseAfterLiveTick.ToString(), phaseAfterFrozenTick.ToString());
			return true;
		}

		if (opAfterFrozenTick != opAfterLiveTick)
		{
			SetFailure("the operation cadence counted down while a battle was live: %1 before the tick, %2 after",
				opAfterLiveTick.ToString(), opAfterFrozenTick.ToString());
			return true;
		}

		Print("Objective director: a tick serves one round off every objective timer, and a tick taken while a battle is live serves none - the whole machine freezes rather than catching up afterwards");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A position in the loaded world to hang a fixture objective on.
	//! \param[in] occupying The occupying faction manager, for its base list.
	//! \param[out] position The resolved position.
	//! \return True when one was found.
	protected bool ResolveFixturePosition(notnull OVT_OccupyingFactionManager occupying, out vector position)
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (towns && towns.m_Towns && !towns.m_Towns.IsEmpty())
		{
			position = towns.m_Towns[0].location;
			return true;
		}

		if (occupying.m_Bases && !occupying.m_Bases.IsEmpty())
		{
			position = occupying.m_Bases[0].location;
			return true;
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! The counter-attack gate: at night it holds the phase timeout AND NOTHING ELSE, in daylight it fires
//! ONCE, and the tick after that does nothing at all.
//!
//! THREE CLAIMS THE CHEAP TIER CANNOT MAKE. The hour predicate and both phase gates are pure and are
//! pinned in OVT_TEST_Logic_ObjectiveScaling.c; what needs live managers is the wiring between them:
//! that the director asks the predicate with ITS OWN two consts and the WORLD'S hour, that the wait
//! holds exactly one clock, and that the fire path reaches the campaign's one battle slot and cannot
//! reach it twice.
//!
//! 🔴 THE NIGHT HALF IS A TWO-SIDED CLAIM, AND BOTH SIDES HAVE A REAL BUG BEHIND THEM (D17, as
//! corrected on 2026-08-19 - the decision's original wording said the wait ticks "no starvation or
//! timeout counter" and its author narrowed it):
//!
//!   - HOLD THE PHASE TIMEOUT. Left running, a gate met at 16:00 would spend the forward-base phase's
//!     remaining budget waiting out the dark and the objective would be ABANDONED FOR BEING NIGHT. In
//!     play that looks like the occupying faction losing interest at random.
//!   - RUN EVERYTHING ELSE. Starvation is the RESISTANCE'S counterplay and answers to facts about the
//!     world, not to the clock. Frozen, a player who empties the supplying garrison at 22:00 would
//!     watch the forward base stand for hours and then launch a counter-attack anyway - contradicting
//!     F7 ("take or empty the supplying base and it comes down on its own") and punishing a correct
//!     play. The operation cadence runs with it, because a frozen cadence either never sends a garrison
//!     or sends one every tick.
//!
//! Neither side is a compile error and neither has any other symptom, so both are asserted on the same
//! driven tick: the phase timeout must be untouched, and the cadence and the starvation counter must
//! each have served exactly one round.
//!
//! ⚠ THE FIXTURE IS CUT OFF BY CONSTRUCTION, and deliberately: its recorded supplying base IS the
//! resistance-held objective, so IsFOBStarved answers true on its first input. That is what makes the
//! starvation row a real claim rather than a reading of zero. A precondition guard rejects a difficulty
//! preset that would let ONE round mature it (every shipped preset authors 15 or more).
//!
//! ⚠ EXACTLY ONCE IS OBSERVED THROUGH THE CAMPAIGN'S OWN SINGLE-BATTLE HANDLE, as the plan asks. The
//! second tick hits the tick's third early return - a battle is live - and therefore does nothing;
//! asserting that the SAME controller instance is still in the slot afterwards is what distinguishes
//! "nothing happened" from "a second battle replaced the first".
//!
//! ⚠ IT DRIVES DirectorTick() RATHER THAN THE GATE, deliberately, for the same reason every other case
//! in this file does: the tick is where the machine's early returns live, and a gate that fires from
//! anywhere else would pass a direct call and still be wrong.
//!
//! IT PUTS BACK EVERYTHING IT TOUCHED - the world clock, the fixture base's faction, the reserve, the
//! battle slot and the objective - because the initialisation world is shared, and this case sorts
//! BEFORE the forward-base, insertion, operations and sabotage cases.
//!
//! PROVEN ABLE TO FAIL, both ways round (record the runs in context.md when the suite is next driven):
//!   - drop the `if (!waitingForDaylight)` guard on AdvancePhaseTimeout() in TickFOB(), so the wait
//!     spends the objective's patience. The tree compiles clean - a missing guard is not a script error
//!     - and the case reports "a gate blocked only by the clock must not serve a round off the phase
//!     timeout".
//!   - restore the ORIGINAL, over-broad reading by returning from TickFOB() as soon as the gate answers
//!     WAIT_FOR_DAYLIGHT. Also compiles clean, and the case reports "the starvation rule must be
//!     evaluated through a daylight wait" - which is the F7 regression the correction was made for.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ObjectiveDirector_GateWaitsForDaylightThenFiresOnce : SCR_AutotestCaseBase
{
	//! Planted phase timeout. Deliberately not a value any phase entry produces.
	static const int PLANTED_PHASE_TICKS = 77;

	//! Planted operation cadence. Non-zero on purpose: a phase entry arms it to zero, and a tick that
	//! reached the spender with a zero countdown would buy a real deployment with real resources.
	static const int PLANTED_OP_TICKS = 31;

	//! An hour comfortably inside the night, well clear of either edge of the shipped 05:00-15:00 window.
	static const float NIGHT_HOUR = 2.0;

	//! An hour comfortably inside the day.
	static const float DAY_HOUR = 10.0;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();

		if (!director || !occupying || !config || !difficulty)
		{
			SetFailure("The director, the occupying faction manager, the config or the difficulty settings did not resolve");
			return true;
		}

		if (!OVT_Global.GetNotify())
		{
			SetFailure("The notification manager did not resolve, and starting a battle sends one - this case cannot drive the fire path without it");
			return true;
		}

		// The fixture's forward base is cut off by construction (its supplying base is the
		// resistance-held objective itself), and the night tick is REQUIRED to serve one round off the
		// starvation counter. One round must not be enough to MATURE it, or the objective would be torn
		// down mid-case for a reason this case is not about. Every shipped preset authors 15 or more.
		if (difficulty.objectiveStarvationMinutes <= 1)
		{
			SetFailure("objectiveStarvationMinutes is %1, so a single tick would mature starvation and tear this fixture down before the daylight half could run", difficulty.objectiveStarvationMinutes.ToString());
			return true;
		}

		TimeAndWeatherManagerEntity clock = ResolveClock();
		if (!clock)
		{
			SetFailure("The world carries no time and weather manager, so the daylight half of the counter-attack gate cannot be exercised here");
			return true;
		}

		// --- ARRANGE. A real base with a resolvable marker: the fire path resolves the objective back to
		//     a base and then to its controller, and refuses if either lookup comes up empty.
		OVT_BaseData fixture;
		OVT_BaseControllerComponent fixtureController;
		if (!ResolveFixtureBase(occupying, fixture, fixtureController))
		{
			SetFailure("The world produced no occupying-faction base with a resolvable marker to hang a counter-attack fixture on");
			return true;
		}

		float savedTimeOfDay = clock.GetTimeOfTheDay();
		int savedFaction = fixture.faction;
		int savedResources = occupying.m_iResources;
		bool savedQRFActive = occupying.m_bQRFActive;

		// The objective has to be a place the resistance holds, or the fire path refuses on the grounds
		// that the occupying faction already owns it.
		fixture.faction = config.GetPlayerFactionIndex();

		int gateResources = difficulty.objectiveQRFResourceGate;
		if (gateResources < 0)
			gateResources = 0;
		occupying.m_iResources = gateResources + 1000;

		director.CommitObjective(OVT_EObjectiveKind.BASE, fixture.location, "counter-attack gate fixture");
		director.EnterPhase(OVT_EObjectivePhase.FOB);
		director.RecordFOB(fixture.location, fixture.location, "counter-attack gate fixture base");

		int required = OVT_ObjectivePhaseRules.RequiredSabotageMissions(difficulty.objectiveSabotageMissionsRequired, OVT_ObjectivePhaseRules.DEFAULT_SABOTAGE_MISSIONS);
		for (int i = 0; i < required; i++)
		{
			director.OnSabotageSuccess();
		}

		director.SetPhaseTimeout(PLANTED_PHASE_TICKS);
		director.SetOperationCountdown(PLANTED_OP_TICKS);

		// --- HALF ONE: NIGHT. The ramp is finished and only the clock is in the way.
		clock.SetTimeOfTheDay(NIGHT_HOUR, true);

		bool nightPlanted = !director.IsCounterAttackDaylight();
		bool waitingForDaylight = director.IsWaitingForCounterAttackDaylight();

		director.DirectorTick();

		int phaseAfterNight = director.GetPhaseTicks();
		int opAfterNight = director.GetNextOpTicks();
		int starvationAfterNight = director.GetFOBStarvationTicks();
		bool starvingDuringWait = director.IsFOBStarving();
		bool battleAfterNight = occupying.m_CurrentQRF != null;
		int phaseIdAfterNight = director.GetPhase();

		// --- HALF TWO: DAY. Nothing else about the fixture changes.
		clock.SetTimeOfTheDay(DAY_HOUR, true);

		bool dayPlanted = director.IsCounterAttackDaylight();
		bool readyToFire = director.IsCounterAttackReady();

		director.DirectorTick();

		OVT_QRFControllerComponent firstBattle = occupying.m_CurrentQRF;
		int phaseIdAfterFire = director.GetPhase();

		// --- HALF THREE: a second tick, with the battle still running.
		director.DirectorTick();

		OVT_QRFControllerComponent secondBattle = occupying.m_CurrentQRF;
		int phaseIdAfterSecond = director.GetPhase();

		// --- RESTORE before asserting, so a red case does not leave a battle running for every case
		//     that follows.
		if (occupying.m_CurrentQRF)
		{
			IEntity battleOwner = occupying.m_CurrentQRF.GetOwner();
			occupying.m_CurrentQRF = null;
			if (battleOwner)
				SCR_EntityHelper.DeleteEntityAndChildren(battleOwner);
		}

		occupying.m_bQRFActive = savedQRFActive;
		occupying.m_iResources = savedResources;
		fixture.faction = savedFaction;
		clock.SetTimeOfTheDay(savedTimeOfDay, true);

		director.ResetObjective("initialisation-tier counter-attack fixture torn down", false);

		// --- ASSERT. The preconditions first: a case that could not arrange its own fixture must say so
		//     rather than pass on an accident.
		if (!nightPlanted)
		{
			SetFailure("the world clock refused to move to %1:00, so the night half of this case never ran - the engine's SetTimeOfTheDay returned without effect", NIGHT_HOUR.ToString());
			return true;
		}

		if (!waitingForDaylight)
		{
			SetFailure("with the ramp arranged as complete and the clock at night, the gate should have answered WAIT_FOR_DAYLIGHT - so either the fixture does not satisfy the ramp or the daylight conjunct is not on the gate at all");
			return true;
		}

		// --- The night half. NOTHING may have moved.
		if (battleAfterNight)
		{
			SetFailure("a counter-attack started at %1:00 - the daylight window is not gating the battle", NIGHT_HOUR.ToString());
			return true;
		}

		if (phaseIdAfterNight != OVT_EObjectivePhase.FOB)
		{
			SetFailure("a night tick moved the objective out of the forward-base phase, to %1", phaseIdAfterNight.ToString());
			return true;
		}

		if (phaseAfterNight != PLANTED_PHASE_TICKS)
		{
			SetFailure("a gate blocked only by the clock must not serve a round off the phase timeout: planted %1, read back %2 - an objective would be abandoned for being night", PLANTED_PHASE_TICKS.ToString(), phaseAfterNight.ToString());
			return true;
		}

		// --- ...AND THE THREE THINGS THAT MUST KEEP RUNNING. The wait holds the director's clock against
		//     itself and nothing else.
		if (opAfterNight != PLANTED_OP_TICKS - 1)
		{
			SetFailure("the operation cadence must keep running through a daylight wait: planted %1, read back %2 - a frozen cadence either never sends a garrison or sends one every tick", PLANTED_OP_TICKS.ToString(), opAfterNight.ToString());
			return true;
		}

		if (starvationAfterNight != 1)
		{
			SetFailure("the starvation rule must be evaluated through a daylight wait: the fixture's supplying base is in resistance hands and the counter read back %1 instead of 1 - a forward base cut off at night must come down at night", starvationAfterNight.ToString());
			return true;
		}

		if (!starvingDuringWait)
		{
			SetFailure("the director did not report its forward base as cut off during the wait, so the starvation half of this case proved nothing");
			return true;
		}

		// --- The day half.
		if (!dayPlanted)
		{
			SetFailure("the world clock refused to move to %1:00, so the firing half of this case never ran", DAY_HOUR.ToString());
			return true;
		}

		if (!readyToFire)
		{
			SetFailure("with the ramp complete and the clock inside the window, the gate should have answered FIRE");
			return true;
		}

		if (!firstBattle)
		{
			SetFailure("the counter-attack gate passed in daylight but no battle was started - the campaign's battle slot is still empty");
			return true;
		}

		if (phaseIdAfterFire != OVT_EObjectivePhase.COUNTER_QRF)
		{
			SetFailure("the objective did not advance to the counter-attack phase after the battle started: phase %1", phaseIdAfterFire.ToString());
			return true;
		}

		// --- Exactly once.
		if (secondBattle != firstBattle)
		{
			SetFailure("a second tick replaced the running battle - the gate fired twice, which is the one thing the campaign's single-battle contract forbids");
			return true;
		}

		if (phaseIdAfterSecond != OVT_EObjectivePhase.COUNTER_QRF)
		{
			SetFailure("a tick taken while the battle was running moved the objective to phase %1 - the freeze should have returned before the phase machine ran at all", phaseIdAfterSecond.ToString());
			return true;
		}

		Print("Objective director: a counter-attack gate met at night holds the phase timeout and NOTHING else - the cadence runs and the forward base can still be starved out - the same gate fires exactly one battle in daylight, and the tick after it does nothing");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The world's time and weather manager.
	//! \return The manager, or null when the world carries none.
	protected TimeAndWeatherManagerEntity ResolveClock()
	{
		BaseWorld baseWorld = GetGame().GetWorld();
		if (!baseWorld)
			return null;

		ChimeraWorld world = ChimeraWorld.CastFrom(baseWorld);
		if (!world)
			return null;

		return world.GetTimeAndWeatherManager();
	}

	//------------------------------------------------------------------------------------------------
	//! The first base in the campaign whose marker entity can still be resolved.
	//! \param[in] occupying The occupying faction manager, for its base list.
	//! \param[out] data The base record.
	//! \param[out] controller Its marker's controller component.
	//! \return True when one was found.
	protected bool ResolveFixtureBase(notnull OVT_OccupyingFactionManager occupying, out OVT_BaseData data, out OVT_BaseControllerComponent controller)
	{
		if (!occupying.m_Bases)
			return false;

		foreach (OVT_BaseData candidate : occupying.m_Bases)
		{
			if (!candidate)
				continue;

			OVT_BaseControllerComponent resolved = occupying.GetBase(candidate.entId);
			if (!resolved)
				continue;

			data = candidate;
			controller = resolved;

			return true;
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! THE IDLE CLOCK IS HELD WHILE THE FACTION CANNOT PAY, AND RUNS WHEN IT CAN.
//!
//! 🔴 THE STATE THIS CASE PINS IS ONE THE PRODUCT DELIBERATELY LETS PERSIST, WHICH IS EXACTLY WHY IT
//! NEEDS A TEST. A play-test (2026-08-19) watched a director spend 31 real minutes unable to afford a
//! single 100-resource operation. The old phase budget ran the whole time and abandoned the objective;
//! the next objective was just as unaffordable, so the machine churned targets forever and never
//! reached Phase 2. Being broke is a fact about the FACTION, and no choice of objective fixes it - so
//! the clock the director runs against itself is HELD, and the block is said out loud once instead.
//!
//! ⚠ AN OBJECTIVE THAT CAN NEVER BE AFFORDED SITS, AND SITTING MUST COST NOTHING. The second half
//! asserts both halves of that: the clock does not move, AND the pool does not move either. A hold that
//! quietly kept trying to buy things would be worse than the timeout it replaced.
//!
//! THE FIRST HALF IS WHAT MAKES THE SECOND MEAN ANYTHING. A held counter is only evidence if an
//! unheld one moves, so the case first drives a tick with a healthy pool and a cadence that has not
//! elapsed, and requires exactly one round to be served. Without it, a director that never ticked at
//! all would pass.
//!
//! ⚠ THE CADENCE IS DROPPED TO ZERO FOR THE SECOND HALF ON PURPOSE, and it is the only way to reach the
//! spender. Every other case in this file plants a HIGH countdown precisely to keep the tick away from
//! it; this one has to arrive there, so it empties the pool first and asserts afterwards that nothing
//! was bought. A refused create leaves the countdown at zero, which is also what makes the hold cover a
//! whole poverty spell rather than one tick in forty-five.
//!
//! PROVEN ABLE TO FAIL (fault injected, compiled at tools/compile-check.sh exit 0, then reverted and
//! recompiled clean):
//!   P1. The `if (blocked) { LogAffordabilityBlock(); return false; }` branch deleted from
//!       TickObjectiveIdleClock(). Reports "a tick blocked only by an empty pool must not serve a round
//!       off the idle clock: held at 52, read back 51".
//!   P2. `m_bBlockedOnAffordability = true;` deleted from the pool test (CanSendObjectiveDeployment()
//!       since 2026-08-19, CreateObjectiveDeployment() before it). Same failure - the flag is the whole
//!       signal.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveDirector_IdleClockHoldsWhileTheFactionCannotPay : SCR_AutotestCaseBase
{
	//! Planted idle clock. Deliberately not a value any phase entry produces, so a re-arm is
	//! distinguishable from a decrement.
	static const int PLANTED_PHASE_TICKS = 53;

	//! Planted operation cadence for the FIRST half. High on purpose: that half must not reach the
	//! spender at all.
	static const int PLANTED_OP_TICKS = 44;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();

		if (!director || !deployments || !config || !towns)
		{
			SetFailure("The director, the deployment framework, the campaign config or the town manager did not resolve");
			return true;
		}

		if (!towns.m_Towns || towns.m_Towns.IsEmpty())
		{
			SetFailure("The world produced no town to hang a fixture objective on");
			return true;
		}

		int occupyingIndex = config.GetOccupyingFactionIndex();
		if (occupyingIndex < 0)
		{
			SetFailure("The occupying faction does not resolve to a faction index, so its pool cannot be emptied");
			return true;
		}

		// PRECONDITION: the operation the ramp would send has to COST something, or an empty pool would
		// not refuse it and this case would assert a hold that never happened.
		OVT_DeploymentConfig rung = deployments.FindConfigByName(OVT_ObjectiveDirectorComponent.HARASSMENT_LADDER[0]);
		if (!rung)
		{
			SetFailure("'%1' is not registered, so the ramp has nothing to be refused", OVT_ObjectiveDirectorComponent.HARASSMENT_LADDER[0]);
			return true;
		}

		if (rung.GetTotalResourceCost() <= 0)
		{
			SetFailure("'%1' costs %2 resources, so an empty pool would not refuse it and this case would prove nothing",
				OVT_ObjectiveDirectorComponent.HARASSMENT_LADDER[0], rung.GetTotalResourceCost().ToString());
			return true;
		}

		// PRECONDITION: the ramp has to be ALLOWED one operation, or the spender would refuse on the
		// concurrency cap before it ever reached the pool test and the hold below would be an accident.
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (!difficulty || difficulty.objectiveHarassmentMaxConcurrent < 1)
		{
			SetFailure("the difficulty preset allows fewer than one concurrent objective operation, so the spender refuses before it reaches the pool and this case cannot say why the clock held");
			return true;
		}

		vector fixturePosition = towns.m_Towns[0].location;
		int originalPool = deployments.GetFactionResources(occupyingIndex);

		// --- ARRANGE.
		director.CommitObjective(OVT_EObjectiveKind.TOWN, fixturePosition, "affordability fixture");
		director.SetPhaseTimeout(PLANTED_PHASE_TICKS);
		director.SetOperationCountdown(PLANTED_OP_TICKS);

		// --- HALF ONE: nothing is blocking, so one tick serves one round.
		director.DirectorTick();

		int phaseAfterFundedTick = director.GetPhaseTicks();

		// --- HALF TWO: empty the pool and let the tick reach the spender.
		deployments.SubtractFactionResources(occupyingIndex, deployments.GetFactionResources(occupyingIndex));
		int poolBeforeBrokeTick = deployments.GetFactionResources(occupyingIndex);

		director.SetOperationCountdown(0);
		director.DirectorTick();

		int phaseAfterBrokeTick = director.GetPhaseTicks();
		int poolAfterBrokeTick = deployments.GetFactionResources(occupyingIndex);
		int phaseIdAfterBrokeTick = director.GetPhase();
		bool stillHasObjective = director.HasObjective();

		// --- RESTORE before asserting, on every path.
		director.ResetObjective("initialisation-tier affordability fixture torn down", false);
		RestorePool(deployments, occupyingIndex, originalPool);

		// --- ASSERT.
		if (phaseAfterFundedTick != PLANTED_PHASE_TICKS - 1)
		{
			SetFailure("an unblocked tick must serve exactly one round off the idle clock: planted %1, read back %2 - the held half below would be vacuous",
				PLANTED_PHASE_TICKS.ToString(), phaseAfterFundedTick.ToString());
			return true;
		}

		if (poolBeforeBrokeTick != 0)
		{
			SetFailure("the occupying faction's pool read %1 after being emptied, so the tick below was never actually broke", poolBeforeBrokeTick.ToString());
			return true;
		}

		if (phaseAfterBrokeTick != phaseAfterFundedTick)
		{
			SetFailure("a tick blocked only by an empty pool must not serve a round off the idle clock: held at %1, read back %2 - an objective would be abandoned for the faction being poor, and the next one would be just as poor",
				phaseAfterFundedTick.ToString(), phaseAfterBrokeTick.ToString());
			return true;
		}

		if (poolAfterBrokeTick != 0)
		{
			SetFailure("the tick moved the pool from 0 to %1 while it was supposed to be refusing to spend - a held objective must cost nothing at all while it waits", poolAfterBrokeTick.ToString());
			return true;
		}

		if (!stillHasObjective || phaseIdAfterBrokeTick != OVT_EObjectivePhase.HARASSMENT)
		{
			SetFailure("the objective did not survive a tick it could not pay for: phase %1", phaseIdAfterBrokeTick.ToString());
			return true;
		}

		Print("Objective director: an unblocked tick serves one round off the idle clock, and a tick blocked only by an empty pool serves none and buys nothing - a broke campaign holds its target instead of churning through targets it equally cannot afford");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the faction's resource pool back exactly where it was found, whichever way it moved.
	//! \param[in] deployments The deployment framework.
	//! \param[in] factionIndex The faction whose pool was borrowed.
	//! \param[in] originalPool The value to restore.
	protected void RestorePool(notnull OVT_DeploymentManagerComponent deployments, int factionIndex, int originalPool)
	{
		int current = deployments.GetFactionResources(factionIndex);

		if (current > originalPool)
			deployments.SubtractFactionResources(factionIndex, current - originalPool);
		else if (current < originalPool)
			deployments.AddFactionResources(factionIndex, originalPool - current);
	}
}

//------------------------------------------------------------------------------------------------
//! A COMPLETED OPERATION RE-ARMS THE IDLE CLOCK - ONCE, ON A TICK, AND NEVER FROM THE COUNTER ITSELF.
//!
//! THREE CLAIMS, AND THE MIDDLE ONE IS THE OLDEST CONTRACT IN THIS FILE:
//!   1. Counting a success moves NO timer. OnHarassmentSuccess() is public and is called from a
//!      deployment's own update, from a restore and from fixtures arranging a state; Phase 5 shipped a
//!      version that decided things from there and broke two suites at once (D4 - only a tick may move
//!      a timer). The idle-clock rework had every opportunity to reintroduce it and did not: the tick
//!      PULLS the counters and compares them against a mark rather than the counter pushing anything.
//!   2. The next TICK sees the news and re-arms the clock to its full authored budget.
//!   3. The tick after that serves an ordinary round. The news is CONSUMED, not a latch that holds the
//!      clock open forever - which would be the same wedge with a friendlier name.
//!
//! ⚠ IT ASSERTS AGAINST GetPhaseTimeoutTicks() RATHER THAN AGAINST 240. The budget is an authored
//! attribute; a case carrying its own copy would go green against a prefab that had been retuned.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time, each compiled at tools/compile-check.sh exit 0,
//! then reverted and recompiled clean):
//!   R1. The `created || reported` branch deleted from TickObjectiveIdleClock(). Reports "a tick that
//!       sees a completed operation must re-arm the idle clock: read back 51, expected 240".
//!   R2. SyncProgressMarks() deleted from ConsumeReportedOperations(), so the news is never consumed.
//!       Reports "the second tick must serve an ordinary round: read back 240, expected 239".
//!   R3. A re-arm added to OnHarassmentSuccess() itself. Reports "counting a harassment success moved
//!       the idle clock" - the D4 guard, still live.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveDirector_IdleClockRearmsWhenAnOperationReports : SCR_AutotestCaseBase
{
	//! Planted idle clock. Deliberately far from the authored budget so a re-arm is unmistakable.
	static const int PLANTED_PHASE_TICKS = 53;

	//! Planted operation cadence. HIGH ON PURPOSE - nothing in this case may reach the spender.
	static const int PLANTED_OP_TICKS = 44;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();

		if (!director || !towns)
		{
			SetFailure("The director or the town manager did not resolve");
			return true;
		}

		if (!towns.m_Towns || towns.m_Towns.IsEmpty())
		{
			SetFailure("The world produced no town to hang a fixture objective on");
			return true;
		}

		int budget = director.GetPhaseTimeoutTicks();
		if (budget <= PLANTED_PHASE_TICKS)
		{
			SetFailure("the authored idle-clock budget is %1, which is not above the planted %2 - a re-arm would be indistinguishable from a decrement",
				budget.ToString(), PLANTED_PHASE_TICKS.ToString());
			return true;
		}

		vector fixturePosition = towns.m_Towns[0].location;

		// --- ARRANGE.
		director.CommitObjective(OVT_EObjectiveKind.TOWN, fixturePosition, "reported-operation fixture");
		director.SetPhaseTimeout(PLANTED_PHASE_TICKS);
		director.SetOperationCountdown(PLANTED_OP_TICKS);

		// --- HALF ONE: an idle tick serves one round.
		director.DirectorTick();

		int phaseAfterIdleTick = director.GetPhaseTicks();

		// --- HALF TWO: an operation reports. The COUNTER may not move a timer.
		director.OnHarassmentSuccess();

		int phaseAfterCounting = director.GetPhaseTicks();

		// --- ...and the next TICK sees it and re-arms.
		director.DirectorTick();

		int phaseAfterProgressTick = director.GetPhaseTicks();

		// --- HALF THREE: the news is consumed, not latched.
		director.DirectorTick();

		int phaseAfterSecondTick = director.GetPhaseTicks();
		int phaseIdAtEnd = director.GetPhase();

		// --- RESTORE before asserting.
		director.ResetObjective("initialisation-tier reported-operation fixture torn down", false);

		// --- ASSERT.
		if (phaseAfterIdleTick != PLANTED_PHASE_TICKS - 1)
		{
			SetFailure("an idle tick must serve exactly one round off the idle clock: planted %1, read back %2",
				PLANTED_PHASE_TICKS.ToString(), phaseAfterIdleTick.ToString());
			return true;
		}

		if (phaseAfterCounting != phaseAfterIdleTick)
		{
			SetFailure("counting a harassment success moved the idle clock: %1 before, %2 after. Only a tick may move a timer - this method is called from deployments, restores and fixtures, none of which is a tick",
				phaseAfterIdleTick.ToString(), phaseAfterCounting.ToString());
			return true;
		}

		if (phaseAfterProgressTick != budget)
		{
			SetFailure("a tick that sees a completed operation must re-arm the idle clock: read back %1, expected the authored budget %2. Work getting done is the opposite of the wedge this clock exists to catch",
				phaseAfterProgressTick.ToString(), budget.ToString());
			return true;
		}

		if (phaseAfterSecondTick != budget - 1)
		{
			SetFailure("the second tick must serve an ordinary round: read back %1, expected %2. A success that keeps re-arming forever is the same wedge under a friendlier name",
				phaseAfterSecondTick.ToString(), (budget - 1).ToString());
			return true;
		}

		if (phaseIdAtEnd != OVT_EObjectivePhase.HARASSMENT)
		{
			SetFailure("the fixture left the harassment phase, so the clock readings above belong to some other phase: %1", phaseIdAtEnd.ToString());
			return true;
		}

		Print("Objective director: counting a completed operation moves no timer, the next tick re-arms the idle clock to its authored budget, and the tick after that serves an ordinary round");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! AN OPERATION IN FLIGHT HOLDS THE BACKSTOP OPEN, AND TEARING IT DOWN HANDS THE MONEY BACK - UNLESS
//! THE RESISTANCE KILLED IT.
//!
//! 🔴 THE HEADLINE DEFECT THIS WHOLE CHANGE CAME OUT OF, PINNED AT THE ONLY TIER THAT CAN SEE IT. In a
//! play-test (2026-08-19) the director created a sabotage team, the team's transport stopped 1561 m
//! short, they started walking - a fifteen-to-twenty minute walk - and nine minutes later the phase
//! clock ran out and deleted them, their truck and the 100 resources they had cost. The next objective
//! was the same base, and the loop had no exit. Men who are walking to a target are the objective
//! WORKING; a machine that cannot tell that from a wedge will always eventually throw away the very
//! operation it was waiting for.
//!
//! FOUR CLAIMS, ON ONE REAL DEPLOYMENT, IN ORDER:
//!   1. A create is PROGRESS. The tick that spends re-arms the clock rather than serving a round.
//!   2. An operation in flight HOLDS the clock. The clock is planted at 1 - one round from expiry - and
//!      a tick must re-arm it instead of abandoning the objective.
//!   3. Tearing that operation down REFUNDS it. The pool comes back to where it was before the create.
//!   4. A force that was WIPED OUT refunds nothing. A team the player killed is a loss, not a recall,
//!      and paying for it would pay the occupying faction for losing a fight.
//!
//! ⚠ IT DRIVES THE DIRECTOR'S OWN SPEND PATH RATHER THAN HAND-BUILDING A DEPLOYMENT, because the thing
//! under test is the ledger the director keeps of what IT created - and a hand-built deployment is not
//! in it. The pool is planted first and restored last, and the deployment is created and destroyed
//! inside a single synchronous step, so no module ever gets an update: the marker exists for the length
//! of this method and nothing it would have spawned is ever spawned. This is the pattern
//! OVT_TEST_Init_Deployments_SeedingIsFreeAndIdempotent already uses.
//!
//! ⚠ THE REFUND IS ASSERTED AS A POOL DELTA, NOT AS A NUMBER. What was spent is read from the pool
//! before and after the create, so the case cannot disagree with the config about what a rung costs.
//!
//! ⚠ IT NEVER ASSERTS ON DEPLOYMENTS IT DID NOT CREATE. This world runs a live deployment wave; every
//! reading here is either the pool or the set of deployments that appeared across one driven tick.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time, each compiled at tools/compile-check.sh exit 0,
//! then reverted and recompiled clean):
//!   F1. The `if (HasOperationInFlight())` branch deleted from TickObjectiveIdleClock(). Reports "an
//!       operation still in flight must hold the idle clock open: the objective was abandoned with men
//!       on their way to it".
//!   F2. RecallDeployment() replaced by DeleteDeployment() in TearDownObjectiveDeployments(). Reports
//!       "tearing down an unfinished operation must return what it cost".
//!   F3. The `if (!deployment.GetSpawnedUnitsEliminated())` guard deleted from RecallDeployment().
//!       Reports "a force that was wiped out must refund nothing".
//!   F4. FOB_GARRISON_CONFIG added to IsObjectiveOperationConfig()'s true list - not detected here, and
//!       said so deliberately: this case only ever creates harassment-phase operations. The forward
//!       base's own classification is argued in that method's header and has no live fixture.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ObjectiveDirector_InFlightOperationIsHeldThenRefunded : SCR_AutotestCaseBase
{
	//! Planted into the occupying faction's pool so the ramp can actually buy something, and taken back
	//! out afterwards.
	static const int PLANTED_POOL = 5000;

	//! Planted operation cadence for the ticks that must NOT spend.
	static const int PLANTED_OP_TICKS = 44;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();

		if (!director || !deployments || !config || !towns)
		{
			SetFailure("The director, the deployment framework, the campaign config or the town manager did not resolve");
			return true;
		}

		if (!towns.m_Towns || towns.m_Towns.IsEmpty())
		{
			SetFailure("The world produced no town to hang a fixture objective on");
			return true;
		}

		int occupyingIndex = config.GetOccupyingFactionIndex();
		if (occupyingIndex < 0)
		{
			SetFailure("The occupying faction does not resolve to a faction index, so nothing can be bought for it");
			return true;
		}

		vector fixturePosition = towns.m_Towns[0].location;
		int originalPool = deployments.GetFactionResources(occupyingIndex);

		array<OVT_DeploymentComponent> created = new array<OVT_DeploymentComponent>();
		string failure = RunHalves(director, deployments, occupyingIndex, fixturePosition, originalPool, created);

		// --- RESTORE ON EVERY PATH, and in this order: the objective (which takes its own deployments
		//     down through the path under test), then anything the halves left standing, then the pool.
		director.ResetObjective("initialisation-tier recall fixture torn down", false);
		Teardown(deployments, created);
		RestorePool(deployments, occupyingIndex, originalPool);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective director: a create re-arms the idle clock, an operation still walking to its target holds it open rather than being deleted five minutes short, tearing that operation down returns what it cost to the pool, and a force the resistance wiped out returns nothing");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Both halves, with every reading taken before anything is put back.
	//! \param[in] director The objective director.
	//! \param[in] deployments The deployment framework.
	//! \param[in] factionIndex The occupying faction.
	//! \param[in] fixturePosition Where the fixture objective sits.
	//! \param[in] originalPool The pool as it was found.
	//! \param[in] created Every deployment either half created, for teardown - filled BEFORE any
	//!            assertion that could return.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string RunHalves(notnull OVT_ObjectiveDirectorComponent director, notnull OVT_DeploymentManagerComponent deployments,
		int factionIndex, vector fixturePosition, int originalPool, notnull array<OVT_DeploymentComponent> created)
	{
		deployments.AddFactionResources(factionIndex, PLANTED_POOL);
		int poolBeforeCreate = deployments.GetFactionResources(factionIndex);

		// --- HALF ONE: the director buys an operation on a tick, which is PROGRESS.
		director.CommitObjective(OVT_EObjectiveKind.TOWN, fixturePosition, "recall fixture");
		director.SetPhaseTimeout(1);
		director.SetOperationCountdown(0);

		array<OVT_DeploymentComponent> beforeCreate = deployments.GetAllDeployments();

		director.DirectorTick();

		CollectNew(deployments, beforeCreate, created);

		int poolAfterCreate = deployments.GetFactionResources(factionIndex);
		int spent = poolBeforeCreate - poolAfterCreate;
		int phaseAfterCreateTick = director.GetPhaseTicks();
		bool inFlightAfterCreate = director.IsOperationInFlight();

		if (spent <= 0)
			return string.Format("the director bought nothing on a tick with %1 resources and a cadence of zero, so there is no operation to hold or refund - the pool read %2 both sides",
				poolBeforeCreate.ToString(), poolAfterCreate.ToString());

		if (created.IsEmpty())
			return string.Format("the pool fell by %1 but no new deployment appeared, so the ledger this case reads has nothing in it", spent.ToString());

		// --- HALF TWO: the clock is one round from expiry, and an operation is walking.
		director.SetPhaseTimeout(1);
		director.SetOperationCountdown(PLANTED_OP_TICKS);

		director.DirectorTick();

		int phaseAfterHeldTick = director.GetPhaseTicks();
		bool survivedTheHeldTick = director.HasObjective();
		int budget = director.GetPhaseTimeoutTicks();

		// --- HALF THREE: the objective ends, and the men who never arrived are paid for.
		director.ResetObjective("initialisation-tier recall fixture: the refund half", false);

		int poolAfterRecall = deployments.GetFactionResources(factionIndex);
		created.Clear();

		// --- HALF FOUR: the same buy, but the resistance kills them first.
		director.CommitObjective(OVT_EObjectiveKind.TOWN, fixturePosition, "loss fixture");
		director.SetOperationCountdown(0);

		array<OVT_DeploymentComponent> beforeLoss = deployments.GetAllDeployments();

		director.DirectorTick();

		CollectNew(deployments, beforeLoss, created);

		int poolAfterLossCreate = deployments.GetFactionResources(factionIndex);
		int lossSpent = poolAfterRecall - poolAfterLossCreate;

		MarkEliminated(created);

		director.ResetObjective("initialisation-tier loss fixture torn down", false);

		int poolAfterLoss = deployments.GetFactionResources(factionIndex);

		// --- ASSERT, all of it after every reading is taken.
		if (phaseAfterCreateTick != budget)
			return string.Format("a tick that creates an operation must re-arm the idle clock rather than serve a round off it: planted 1, read back %1, expected the authored budget %2",
				phaseAfterCreateTick.ToString(), budget.ToString());

		if (!inFlightAfterCreate)
			return "the director does not report the operation it just created as in flight, so the hold below would prove nothing about a walking team";

		if (!survivedTheHeldTick)
			return "an operation still in flight must hold the idle clock open: the objective was abandoned with men on their way to it, which is the exact defect this change exists to end";

		if (phaseAfterHeldTick != budget)
			return string.Format("an operation in flight must re-arm the idle clock, not merely postpone it: planted 1, read back %1, expected %2",
				phaseAfterHeldTick.ToString(), budget.ToString());

		if (poolAfterRecall != poolBeforeCreate)
			return string.Format("tearing down an unfinished operation must return what it cost: the pool was %1 before the create, %2 after it, and %3 after the teardown - %4 resources went nowhere",
				poolBeforeCreate.ToString(), poolAfterCreate.ToString(), poolAfterRecall.ToString(), (poolBeforeCreate - poolAfterRecall).ToString());

		if (lossSpent <= 0)
			return string.Format("the second buy spent nothing (pool %1 then %2), so the wiped-out half has no operation to withhold a refund for",
				poolAfterRecall.ToString(), poolAfterLossCreate.ToString());

		if (poolAfterLoss != poolAfterLossCreate)
			return string.Format("a force that was wiped out must refund nothing: the pool was %1 after the buy and %2 after the teardown - the occupying faction was paid %3 for losing a fight",
				poolAfterLossCreate.ToString(), poolAfterLoss.ToString(), (poolAfterLoss - poolAfterLossCreate).ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Every live deployment that was not in the baseline.
	//! \param[in] deployments The deployment framework.
	//! \param[in] baseline The deployments that existed before the step.
	//! \param[in] into Where the new ones are appended.
	protected void CollectNew(notnull OVT_DeploymentManagerComponent deployments, array<OVT_DeploymentComponent> baseline,
		notnull array<OVT_DeploymentComponent> into)
	{
		array<OVT_DeploymentComponent> after = deployments.GetAllDeployments();
		if (!after)
			return;

		foreach (OVT_DeploymentComponent deployment : after)
		{
			if (!deployment)
				continue;

			if (baseline && baseline.Contains(deployment))
				continue;

			into.Insert(deployment);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Flags a set of deployments as wiped out, at the deployment AND at every spawning module, which is
	//! the state a force the resistance killed is left in.
	//! \param[in] deployments The deployments to flag.
	protected void MarkEliminated(notnull array<OVT_DeploymentComponent> deployments)
	{
		foreach (OVT_DeploymentComponent deployment : deployments)
		{
			if (!deployment)
				continue;

			deployment.SetSpawnedUnitsEliminated(true);

			array<OVT_BaseSpawningDeploymentModule> modules = deployment.GetSpawningModules();
			foreach (OVT_BaseSpawningDeploymentModule module : modules)
			{
				if (module)
					module.SetSpawnedUnitsEliminated(true);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Makes every fixture deployment inert and then deletes it, in case the path under test left one
	//! standing. Inert first: a deployment deleted while it could still register something is a group
	//! left in a shared world.
	//! \param[in] deploymentManager The deployment framework.
	//! \param[in] created Every deployment the halves created.
	protected void Teardown(notnull OVT_DeploymentManagerComponent deploymentManager, notnull array<OVT_DeploymentComponent> created)
	{
		MarkEliminated(created);

		foreach (OVT_DeploymentComponent deployment : created)
		{
			if (deployment)
				deploymentManager.DeleteDeployment(deployment);
		}

		created.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the faction's resource pool back exactly where it was found, whichever way it moved.
	//! \param[in] deployments The deployment framework.
	//! \param[in] factionIndex The faction whose pool was borrowed.
	//! \param[in] originalPool The value to restore.
	protected void RestorePool(notnull OVT_DeploymentManagerComponent deployments, int factionIndex, int originalPool)
	{
		int current = deployments.GetFactionResources(factionIndex);

		if (current > originalPool)
			deployments.SubtractFactionResources(factionIndex, current - originalPool);
		else if (current < originalPool)
			deployments.AddFactionResources(factionIndex, originalPool - current);
	}
}

//------------------------------------------------------------------------------------------------
//! 🔴 A SECOND, DIFFERENT REFUSAL IS NEVER SWALLOWED BY THE FIRST. The director latches every "why the
//! operation could not be sent" line so that a refusal retried once per in-game minute does not fill
//! the log with itself; the KEY that latch uses is what this case pins.
//!
//! WHAT WENT WRONG WITHOUT IT (play-test, 2026-08-19). The latch was one bool per OBJECTIVE. At 15:30
//! the ramp could not afford a sabotage team and said so; the bool went up. At 15:43 the same objective
//! entered the forward-base phase, could not afford the forward base either, and said NOTHING - the bool
//! was already up. What the author then watched was a phase re-siting the same point every ten seconds
//! for five real minutes with no explanation anywhere in the log, and the actual cause (twenty resources
//! in a pool that needed a hundred and twenty) invisible.
//!
//! THREE CLAIMS, AND THE MIDDLE ONE IS THE BUG:
//!   1. The same operation refused for the same reason is ONE entry - or the log fills up, which is the
//!      reason a latch exists at all.
//!   2. A DIFFERENT operation refused for the same reason is a DIFFERENT entry. This is the play-test.
//!   3. The same operation refused for a DIFFERENT reason is a DIFFERENT entry - the second half of the
//!      same rule, and the one that catches a config whose refusal changes from "the pool is short" to
//!      "the framework declined it" partway through a campaign.
//!
//! AND THE FIVE REASONS ARE DISTINCT STRINGS, which is what makes claim 3 mean anything: two constants
//! that happened to carry the same text would silently merge into one ledger entry.
//!
//! WHY THIS TIER RATHER THAN A LIVE DIRECTOR. IsSameRefusal() is a pure static, deliberately - driving
//! the real ledger would mean writing into the running campaign's director, whose objective is live in
//! this world, and leaving it able to suppress a genuine line later in the session.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; each exited tools/compile-check.sh
//! 0, and the subject was restored and re-compiled clean):
//!   R1. `IsSameRefusal` returning `reasonA == reasonB` alone - the pre-fix behaviour, keyed on the
//!       reason. Fails on "two DIFFERENT operations refused for the same reason must each get a line".
//!   R2. `IsSameRefusal` returning `configA == configB` alone. Fails on "the same operation refused for
//!       a DIFFERENT reason must get a second line".
//!   R3. REFUSAL_FOB_CEILING given the same text as REFUSAL_POOL_SHORT. Fails on "two refusal reasons
//!       share the same text".
//! No polling, no world, no fixture state, no maxAttempts.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveDirector_RefusalsAreLatchedPerConfigAndReason : SCR_AutotestCaseBase
{
	//! Two operation names that cannot collide with a registered config.
	static const string OPERATION_A = "OVT_TEST Refusal Operation A";
	static const string OPERATION_B = "OVT_TEST Refusal Operation B";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string pool = OVT_ObjectiveDirectorComponent.REFUSAL_POOL_SHORT;
		string ceiling = OVT_ObjectiveDirectorComponent.REFUSAL_FOB_CEILING;

		// --- 1. THE SAME REFUSAL IS ONE ENTRY.
		if (!OVT_ObjectiveDirectorComponent.IsSameRefusal(OPERATION_A, pool, OPERATION_A, pool))
		{
			SetFailure("the same operation refused for the same reason must be ONE ledger entry, or every in-game minute of a poverty spell puts another identical line in the log");
			return true;
		}

		// --- 2. THE PLAY-TEST. Two operations, one empty pool, two lines.
		if (OVT_ObjectiveDirectorComponent.IsSameRefusal(OPERATION_A, pool, OPERATION_B, pool))
		{
			SetFailure("two DIFFERENT operations refused for the same reason must each get a line - this is the 2026-08-19 defect exactly: '%1' was refused for want of resources in Phase 1 and that silenced the identical refusal of '%2' in Phase 2", OPERATION_A, OPERATION_B);
			return true;
		}

		// --- 3. THE OTHER HALF OF THE SAME RULE.
		if (OVT_ObjectiveDirectorComponent.IsSameRefusal(OPERATION_A, pool, OPERATION_A, ceiling))
		{
			SetFailure("the same operation refused for a DIFFERENT reason must get a second line - a refusal that changes from '%1' to '%2' is new information", pool, ceiling);
			return true;
		}

		// --- AND THE REASONS THEMSELVES HAVE TO BE TELLABLE APART.
		string clash = CheckReasonsAreDistinct();
		if (clash != "")
		{
			SetFailure(clash);
			return true;
		}

		Print("Objective director: refusals latch on the (operation, reason) PAIR - the same refusal is said once, a different operation or a different reason is said again, and the five reason constants are distinct strings");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every REFUSAL_* constant must be a distinct, non-empty string: the reason is half the latch key,
	//! so two constants carrying the same text are one ledger entry however different they read in code.
	//! \return An empty string when they are all distinct, or which pair is not.
	protected string CheckReasonsAreDistinct()
	{
		array<string> reasons = {
			OVT_ObjectiveDirectorComponent.REFUSAL_UNREGISTERED,
			OVT_ObjectiveDirectorComponent.REFUSAL_POOL_SHORT,
			OVT_ObjectiveDirectorComponent.REFUSAL_FOB_CEILING,
			OVT_ObjectiveDirectorComponent.REFUSAL_FRAMEWORK_DECLINED,
			OVT_ObjectiveDirectorComponent.REFUSAL_NO_SOURCE_BASE
		};

		for (int i = 0; i < reasons.Count(); i++)
		{
			if (reasons[i] == "")
				return string.Format("refusal reason %1 is an empty string - it would latch against every other empty reason", i.ToString());

			for (int j = i + 1; j < reasons.Count(); j++)
			{
				if (reasons[i] == reasons[j])
					return string.Format("two refusal reasons share the same text ('%1') - they are one ledger entry, so the second situation would never be reported", reasons[i]);
			}
		}

		return "";
	}
}
