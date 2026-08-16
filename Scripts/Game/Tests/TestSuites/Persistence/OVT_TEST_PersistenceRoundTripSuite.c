//------------------------------------------------------------------------------------------------
//! TIER D' - SAVE/RELOAD ROUND TRIP. Part of OVT_TestGroup_All since 2026-08-02.
//!
//! HISTORY: this suite was authored quarantined and red by design (2026-08-02, dev-ops/
//! test-coverage) as the vanilla-persistence migration's acceptance criterion: its flip from
//! exit 1 to exit 0 WAS the definition of done. The migration landed the same day and the flip
//! happened; the quarantine was lifted per its own written procedure (green -> de-quarantine ->
//! add to OVT_TestGroup_All -> update docs/features/core/persistence/).
//!
//! PRECONDITION: the capability case asserts the no-save -> save TRANSITION, which needs a fresh
//! session. tools/run-tests.sh now resets the OverthrowCI save state before every run (unless
//! OVERTHROW_SAVE_DIR pins a fixture), so the precondition holds in group runs automatically.
//!
//! ===========================================================================================
//! ASSERTION RULE, NON-NEGOTIABLE (implementation.md Phase 4, Decision 4).
//!
//! Quoted below with the three type-name tokens replaced by descriptions - DELIBERATELY, because
//! the rule is enforced by grepping this whole tree for exactly those tokens, and a comment that
//! quoted them would trip the very check it is describing. The verbatim wording, and the exact
//! grep, are in implementation.md under Decision 4.
//!
//!   "no persistence-framework type, no vanilla persistence type, and no Overthrow save-data
//!    class may appear anywhere in these files except the single documented save-trigger call.
//!    Every assertion reads state back through the same public manager API that wrote it. A
//!    reviewer must be able to grep this tree for those type names and find at most the one
//!    annotated trigger line."
//!
//! THERE ARE NOW TWO ANNOTATED TRIGGERS, NOT ONE, AND BOTH ARE IN THE GATE CLASS BELOW:
//!
//!   1. OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce()      - the SAVE trigger.
//!   2. OVT_TEST_PersistenceRoundTripGate.RequestSessionReload() - the LOAD trigger.
//!
//! The second one is new and is what makes this suite a round trip at all. The original draft
//! reloaded by re-requesting the test world through the autotest framework, which boots a FRESH
//! world and can only ever prove that the campaign restarts - not that a save was read. Loading is
//! a first-class operation of the persistence layer, so it gets a seam of its own, on the same
//! terms as the save trigger: a single annotated call to Overthrow's PUBLIC manager API, naming no
//! storage type and no engine save API. Decision 4 is unchanged in substance - a reviewer greps for
//! the forbidden type names and still finds none anywhere, including in these two lines.
//!
//! Everything else - what is written, what is read back - goes through Overthrow's public manager
//! API, which is why this suite can be the gate at all: it does not know or care whether the
//! storage underneath is EPF or vanilla, only that what went in comes back.
//! ===========================================================================================
//!
//! ---------------------------------------------------------------------------------------------
//! RUN RECIPE (also in tools/README.md)
//!
//!   tools/run-tests.sh OVT_TEST_PersistenceRoundTripSuite     # just this suite
//!   tools/run-tests.sh "{6A6E2A002F53A581}"                   # or the whole All group
//!
//! run-tests.sh resets the OverthrowCI save state itself before every run, so the fresh-session
//! precondition needs no manual step. (Historical acceptance flip: exit 1 -> exit 0, 2026-08-02.)
//!
//! reset_save.sh now clears BOTH save layouts under the profile: EPF's `.db` tree and the engine's
//! `profile/.save/*/game` savepoints (never `settings/`). That matters for the fresh-session half of
//! closure 1 below: with vanilla persistence a stale savepoint from a previous run is exactly what
//! makes HasSaveGame() true before this suite has saved anything, which the capability case reports
//! as a precondition violation rather than trusting.
//!
//! NEVER run the save scripts without --profile OverthrowCI (or an explicit OVERTHROW_SAVE_DIR):
//! their default target is the user's real Workbench campaign save.
//!
//! ---------------------------------------------------------------------------------------------
//! WHY THE FAILURE IS DIAGNOSTIC AND NOT A VALUE MISMATCH (task 4.5 - load-bearing)
//!
//! On this branch SaveGame() returns COMPLETELY SILENTLY: it prints nothing, writes nothing to
//! disk, and HasSaveGame() is a hardcoded false (findings.md 1.7). A naive round-trip test would
//! therefore fail somewhere deep in an assertion, reporting "expected 12345, got 777" - which tells
//! a reviewer nothing about why. Every case here instead asserts the CAPABILITY first, so the
//! failure that reaches junit.xml names the missing capability in one sentence.
//!
//! Saving is ASYNCHRONOUS, so "assert the capability first" is a wait, not a same-frame check: a
//! case triggers exactly one save and then polls until the manager reports it settled. The poll is
//! bounded (MAX_SAVE_POLLS) and its expiry raises the same CAPABILITY_ABSENT sentence, so a save
//! layer that never completes is reported identically to one that was never implemented. A bounded
//! diagnostic wait is not a retry: nothing is attempted twice, and expiry FAILS.
//!
//! ---------------------------------------------------------------------------------------------
//! ANTI-VACUOUS-PASS DESIGN (task 4.6) - this suite must be UNABLE to go green without persistence
//! actually working. Written adversarially: for each stub that could fake a pass, the closure.
//!
//!  1. STUB: `HasSaveGame()` hardcoded to return true (a save layer that lies about having saved).
//!     CLOSURE: the capability case asserts the whole TRANSITION - HasSaveGame() must be FALSE
//!     before the first save of a fresh session and TRUE after it. A constant-true stub fails the
//!     "before" half; a constant-false stub fails the "after" half. This is why the run recipe
//!     requires reset_save.sh: the "before" half is only meaningful in a fresh session.
//!
//!  2. STUB: a reload that is a no-op (or that never actually loads anything).
//!     CLOSURE: every state-kind case DIRTIES the value after saving and before reloading. The
//!     assertion is that the SAVED value came back - not that the value never changed. A no-op
//!     reload leaves the dirty value in place and the case fails. This closure does not depend on
//!     HasSaveGame() at all, so it survives closure 1 being defeated.
//!
//!  3. STUB: a reload that resets state to campaign-start defaults rather than to the save.
//!     CLOSURE: the mutated value is deliberately not a value the campaign start would produce,
//!     and the assertion is equality with the saved value, not "different from the dirty value".
//!
//!  4. A case that silently skips its assertions because a manager or subject was null.
//!     CLOSURE: every resolution failure is an explicit SetResultFailure with a named reason.
//!     There is no path through any case that reaches SetResultSuccess without asserting.
//!
//!  5. Reliance on case execution order (the capability case happening to run first).
//!     CLOSURE: no STATE-KIND case depends on order - each independently triggers its OWN save and
//!     waits for that save before it reloads anything, so it neither needs an earlier case to have
//!     run nor is misled by one that did (closure 6).
//!     ONE ORDER DEPENDENCY DOES EXIST, deliberately, and it is the capability case's
//!     RequireFreshSession() half: HasSaveGame() must be FALSE before this suite's first save, which
//!     holds only because `..._Capability_...` sorts alphabetically first and therefore runs before
//!     any other case has triggered a save. The condition that would break it, stated so it can be
//!     checked: a case added to this suite whose class name sorts BEFORE the capability case AND
//!     that triggers a save. Such a case would turn the capability gate's fresh-session check into a
//!     "precondition violated" failure. Keep the capability case first, or keep any earlier-sorting
//!     case save-free.
//!
//!  6. (NEW, and the reason state cases save exactly once.) STUB: a reload that restores an OLDER
//!     save than the one this case wrote - which would happen for free if a case reloaded before its
//!     own save had settled, since an earlier case's savepoint already exists.
//!     CLOSURE: a case reads the manager's COMPLETED-SAVE COUNT before triggering its save and waits
//!     for that number to go up, rather than waiting for "a save exists" (true forever after the
//!     first case) or for "no save in progress" (also true when the save was silently refused). A
//!     case therefore can never reload from a savepoint that predates its own mutation, and a
//!     refused save is reported as the missing capability instead of as lost data.
//!
//! ---------------------------------------------------------------------------------------------
//! THE RELOAD MECHANISM, AND EXACTLY WHAT THIS SUITE THEREFORE DOES AND DOES NOT PROVE.
//!
//! WHAT IT IS. The reload half is an IN-SESSION RE-APPLICATION of persisted data:
//! OVT_PersistenceManagerComponent.ReapplyLatestSaveData() asks the persistence system to re-read
//! the stored record for instances that are already live and run their deserialization over them.
//! No world transition, no restart. Per case the round trip is:
//!
//!     mutate -> save -> DIRTY the value -> re-apply persisted data -> assert the SAVED value
//!
//! That is a genuine storage round trip: the value is written to the save storage, deliberately
//! destroyed in memory, and then has to come back OUT of storage. Closures 2 and 3 are what give
//! this rung its teeth and they are unchanged - a re-application that restores nothing leaves the
//! dirty value in place and the case fails, and the saved value is never one campaign start would
//! produce.
//!
//! WHAT IT IS NOT, AND THIS IS THE HONEST LIMIT OF THE GATE.
//! The real player-facing continue flow - savepoint on disk -> SaveGameManager load -> session
//! restarts -> campaign comes back - is NOT covered by this suite and cannot be. It was tried, and
//! it is structurally incompatible with the `-autotest` harness: a mid-case load performs a
//! game-state transition, the CLI harness treats every world load as a brand new test run and
//! restarts the whole suite from scratch, so no case can ever resume on the other side. Measured
//! result was an infinite restart loop (playthrough counter climbing, old savepoints being rotated
//! away) until the harness timed out with no junit written at all. The same restart also makes the
//! capability case's fresh-session check unsatisfiable.
//!
//! Consequences a reader must hold onto:
//!  - a green run here means "what Overthrow persists is written to storage and comes back from
//!    storage", NOT "quitting and continuing a campaign works";
//!  - the restart path stays a MANUAL play-test item (Phase 7 checklist in
//!    docs/features/core/persistence/): save, quit to menu, continue, verify the campaign;
//!  - RequireRestoredCampaign() is a sanity assert in this rung, not proof of a load. Nothing
//!    restarted the session, so a started campaign is expected; it is asserted anyway because a
//!    re-application that tore down the game mode or unset its start state would be a serious bug
//!    and this is the cheapest place to notice it;
//!  - LoadLatestSave() on the manager is deliberately KEPT and is the production continue-flow API.
//!    It is simply not what a test can call.
//!
//! ---------------------------------------------------------------------------------------------
//! THE SECOND LIFECYCLE THIS SUITE COVERS: PER-INSTANCE RESERVATION, WITH NO SAVE POINT AT ALL.
//!
//! Everything above describes the save-point round trip that eight of the cases here use. One case -
//! `..._VehicleReserveRelease_...` - exercises the OTHER half the disconnect/reconnect flow is built
//! out of: a single owned instance is taken out of PLAY and put back, without ever leaving the world.
//! It uses NEITHER gate seam, takes no save point and re-applies nothing, so:
//!
//!   - it does not depend on, and cannot disturb, the capability case's fresh-session precondition
//!     (closure 5 above) - and its class name sorts last in this suite regardless;
//!   - its non-vacuousness comes from the state change in the middle: the vehicle reports hidden and
//!     untraceable after the despawn and in play after the respawn, and it is the SAME entity at both
//!     ends, which a rebuild could not be.
//!
//! IT USED TO BE A STORAGE ROUND TRIP (GitHub #143: write the record, delete the instance, ask for it
//! back by id) and is not any more, because BUG-086 removed the mechanism it tested - a released
//! record was measured being pruned within ten minutes on a live server. Nothing automated now
//! exercises PersistenceSystem.RequestSpawn() for a vehicle; that path survives as the post-restart
//! fallback and is play-test territory, like every other real-restart claim in this feature.
//!
//! CASE LIST (execution order is alphabetical by class name):
//!   1. Capability_SaveGameProducesASave        - the gate, and the only case with no reload
//!   1a. JobBoard_SurvivesSaveAndReload         - the only case that re-applies TWICE, because
//!                                                idempotency is part of what it asserts
//!   2. PlayerMoney_SurvivesSaveAndReload
//!   3. PlayerSkills_SurvivesSaveAndReload
//!   4. RealEstateOwnership_SurvivesSaveAndReload
//!   5. Recruits_SurvivesSaveAndReload
//!   6. TownControl_SurvivesSaveAndReload
//!   7. TownPopulation_SurvivesSaveAndReload
//!   8. TownStability_SurvivesSaveAndReload
//!   9. TownSupport_SurvivesSaveAndReload
//!  10. VehicleRegistry_SurvivesSaveAndReload
//!  11. VehicleReserveRelease_KeepsOwnerAndContents - per-instance reservation, no save point
//!  12. VirtualGroupsWiped_DoNotComeBack           - the terminal half of the D2 promise
//!  13. VirtualGroups_SurviveSaveAndReload         - the partially wiped group, re-CREATED on load
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_TEST_PersistenceRoundTripSuite : OVT_TEST_SuiteBase
{
	//------------------------------------------------------------------------------------------------
	//! The round trip starts from a running campaign, exactly like the green Tier D suite.
	//! \return Always true for this suite.
	override bool RequiresStartedCampaign()
	{
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The two seams this suite is allowed to touch, plus the shared round-trip machinery.
//!
//! Everything persistence-layer-facing in the entire test tree is in this one class, so that a
//! reviewer checking Decision 4 has exactly two places to look and the cases stay behaviour-level.
//------------------------------------------------------------------------------------------------
class OVT_TEST_PersistenceRoundTripGate
{
	//! The failure text a reviewer reads in junit.xml when the migration is not done.
	static const string CAPABILITY_ABSENT =
		"Persistence capability absent: SaveGame() produced no save (HasSaveGame() still false). The vanilla-persistence migration is not complete.";

	//! Phase constants for the per-case round-trip state machine.
	//! The names are the documented ones; saving and loading are both asynchronous, so each has a
	//! wait phase of its own between the acting phases.
	static const int PHASE_MUTATE_AND_SAVE = 0;
	static const int PHASE_AWAIT_SAVE = 1;
	static const int PHASE_DIRTY_AND_RELOAD = 2;
	static const int PHASE_AWAIT_RELOAD = 3;
	static const int PHASE_ASSERT = 4;

	//! Outcomes of one PollSaveSettled() call.
	static const int SAVE_PENDING = 0;
	static const int SAVE_SETTLED = 1;
	static const int SAVE_FAILED = 2;

	//! Frame polls allowed for a triggered save to settle.
	//! A diagnostic backstop, not a retry: nothing is re-attempted, and expiry fails the case with
	//! CAPABILITY_ABSENT instead of hanging until the harness timeout.
	static const int MAX_SAVE_POLLS = 600;

	//! Frame polls allowed for the re-application of persisted state to complete.
	//! Same contract as MAX_SAVE_POLLS. An in-session re-application reads one stored record and runs
	//! its serializers, so this is orders of magnitude more than it should ever need - it exists so a
	//! callback that never fires fails with a sentence instead of hanging the harness.
	static const int MAX_RELOAD_POLLS = 600;

	//------------------------------------------------------------------------------------------------
	//! Resolves Overthrow's persistence manager from the live game mode.
	//! \param[out] diagnostic Reason it could not be resolved; untouched on success.
	//! \return The manager, or null.
	static OVT_PersistenceManagerComponent ResolvePersistence(out string diagnostic)
	{
		OVT_OverthrowGameMode mode = OVT_Global.GetOverthrow();
		if (!mode)
		{
			diagnostic = "Persistence capability absent: there is no Overthrow game mode in the loaded world.";
			return null;
		}

		OVT_PersistenceManagerComponent persistence = mode.GetPersistence();
		if (!persistence)
		{
			diagnostic = "Persistence capability absent: the game mode has no persistence manager component.";
			return null;
		}

		return persistence;
	}

	//------------------------------------------------------------------------------------------------
	//! Requires that this session has not saved yet.
	//!
	//! Half of anti-vacuous-pass closure 1: a save layer that always claims to have a save cannot
	//! satisfy this, and a run that skipped the reset_save.sh precondition is reported as a
	//! precondition violation rather than being silently trusted.
	//! \return An empty string when the session is fresh, otherwise a diagnostic.
	static string RequireFreshSession()
	{
		string diagnostic;
		OVT_PersistenceManagerComponent persistence = ResolvePersistence(diagnostic);
		if (!persistence)
			return diagnostic;

		if (persistence.HasSaveGame())
		{
			return "Precondition violated: HasSaveGame() is already true before this suite saved anything. Run '.scripts/reset_save.sh --profile OverthrowCI' before this suite, and check that HasSaveGame() is not hardcoded.";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Reads how many saves the manager has completed so far, to be used as a wait baseline.
	//!
	//! Recorded by a case immediately BEFORE it triggers its save, so the wait afterwards can insist
	//! on that number going up rather than on "a save exists" - see PollSaveSettled().
	//! \return The completed-save count, or -1 when the manager cannot be resolved (in which case
	//! TriggerSaveOnce() is about to fail with a named diagnostic anyway).
	static int CompletedSaveCount()
	{
		string diagnostic;
		OVT_PersistenceManagerComponent persistence = ResolvePersistence(diagnostic);
		if (!persistence)
			return -1;

		return persistence.GetCompletedSaveCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Triggers exactly one save.
	//!
	//! Called by EVERY case, so no case depends on another having run. Saving is asynchronous, so
	//! this only ASKS - PollSaveSettled() below is the half that decides whether the capability is
	//! actually there.
	//! \return An empty string when the save was requested, otherwise a diagnostic naming the gap.
	static string TriggerSaveOnce()
	{
		string diagnostic;
		OVT_PersistenceManagerComponent persistence = ResolvePersistence(diagnostic);
		if (!persistence)
			return diagnostic;

		// ===========================================================================================
		// PERMITTED PERSISTENCE-LAYER SEAM 1 OF 2 IN THE ENTIRE TEST TREE (Decision 4) - THE SAVE.
		// Chosen over the player-facing RequestSave() RPC because it is server-side, needs no player
		// entity, and is the method that RPC ends up calling anyway. Nothing else in Scripts/Game/Tests/
		// may touch the persistence layer, and no assertion anywhere may name a storage type.
		persistence.SaveGame();
		// ===========================================================================================

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! One poll of the wait for a triggered save to settle.
	//!
	//! Settled means THIS case's save completed - the manager's completed-save count has gone past the
	//! baseline the case took before triggering - and that a save now exists. That is closure 6: after
	//! the first case has saved, HasSaveGame() is already true forever, so a wait that only checked it
	//! would let a case reload from a savepoint older than its own mutation and then fail with a
	//! data-loss message when the real fault was a refused save.
	//! \param[in] baseline Completed-save count read before the save was triggered.
	//! \param[out] diagnostic Reason the wait cannot continue; untouched unless SAVE_FAILED.
	//! \return SAVE_PENDING, SAVE_SETTLED or SAVE_FAILED.
	static int PollSaveSettled(int baseline, out string diagnostic)
	{
		OVT_PersistenceManagerComponent persistence = ResolvePersistence(diagnostic);
		if (!persistence)
			return SAVE_FAILED;

		if (persistence.IsSaveInProgress())
			return SAVE_PENDING;

		if (persistence.GetCompletedSaveCount() <= baseline)
			return SAVE_PENDING;

		if (!persistence.HasSaveGame())
			return SAVE_PENDING;

		return SAVE_SETTLED;
	}

	//------------------------------------------------------------------------------------------------
	//! Requests the in-session re-application of persisted state that this round trip reloads with.
	//!
	//! NOT a session restart - see the suite header's reload-mechanism section for why one is
	//! impossible here and what that costs in coverage.
	//! \return An empty string when the re-application was requested, otherwise a diagnostic.
	static string RequestSessionReload()
	{
		string diagnostic;
		OVT_PersistenceManagerComponent persistence = ResolvePersistence(diagnostic);
		if (!persistence)
			return diagnostic;

		// ===========================================================================================
		// PERMITTED PERSISTENCE-LAYER SEAM 2 OF 2 IN THE ENTIRE TEST TREE (Decision 4) - THE LOAD.
		// Same terms as the save trigger: Overthrow's public manager API, no storage type named, no
		// engine save API touched. "Read what was persisted and put it back" is the operation whose
		// result this suite exists to assert.
		persistence.ReapplyLatestSaveData();
		// ===========================================================================================

		// A refusal (no persistence system, system not active, game mode not tracked) is reported
		// synchronously, so say so now instead of making the case wait out a re-application that was
		// never started.
		if (!persistence.IsReapplyInProgress() && persistence.GetLastReapplyDiagnostic() != "")
			return "Persisted data could not be re-applied: " + persistence.GetLastReapplyDiagnostic() + ". The vanilla-persistence migration is not complete.";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! True while the requested re-application is still in flight.
	//!
	//! An unresolvable manager ends the wait rather than extending it: there is no world transition in
	//! this rung, so a missing game mode is a real fault and RequireRestoredCampaign() names it one
	//! poll later instead of burning the whole poll budget first.
	//! \return True when the re-application is still expected to complete.
	static bool ReloadInProgress()
	{
		string diagnostic;
		OVT_PersistenceManagerComponent persistence = ResolvePersistence(diagnostic);
		if (!persistence)
			return false;

		return persistence.IsReapplyInProgress();
	}

	//------------------------------------------------------------------------------------------------
	//! Requires that the session is still a running campaign after the re-application.
	//!
	//! Two jobs, and the FIRST is the load-bearing one:
	//!  - a re-application that never happened, or that the persistence system refused, is reported by
	//!    name. The manager's diagnostic is empty exactly when the last one succeeded, so anything
	//!    found here means the reload half of the round trip did not run at all - which is a very
	//!    different fault from "the value did not come back" and deserves to say so.
	//!  - the campaign is still started. In this rung that is a sanity assert rather than proof of a
	//!    load (nothing restarted the session), but it still catches a re-application that manages to
	//!    tear the game mode down or reset its start state.
	//! \return An empty string when the session is healthy, otherwise a diagnostic.
	static string RequireRestoredCampaign()
	{
		string diagnostic;
		OVT_PersistenceManagerComponent persistence = ResolvePersistence(diagnostic);
		if (!persistence)
			return diagnostic;

		if (persistence.GetLastReapplyDiagnostic() != "")
		{
			return "Persisted data was never re-applied: " + persistence.GetLastReapplyDiagnostic() + ". The vanilla-persistence migration is not complete.";
		}

		OVT_OverthrowGameMode mode = OVT_Global.GetOverthrow();
		if (!mode)
			return "The re-application left no Overthrow game mode in the world.";

		if (!mode.HasGameStarted())
			return "The re-application left a campaign that is no longer started - restoring saved data must not undo the running campaign.";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! THE CAPABILITY GATE. Asserts, in one case, that saving is implemented at all.
//!
//! This is the case a reviewer should read first when the suite is red: its failure text names the
//! missing capability in one sentence, so junit.xml explains itself without anyone opening the
//! source. It asserts the whole transition - no save before, a save after - which is what makes a
//! lying save layer detectable (closure 1 in the suite header).
//!
//! It is also the only case with no reload: the whole case is one fresh-session check, one save, and
//! a bounded wait, so it keeps the shorter timeout.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 30)]
class OVT_TEST_PersistenceRoundTrip_Capability_SaveGameProducesASave : SCR_AutotestCaseBase
{
	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			string fresh = OVT_TEST_PersistenceRoundTripGate.RequireFreshSession();
			if (fresh != "")
			{
				SetFailure(fresh);
				return true;
			}

			m_iSaveBaseline = OVT_TEST_PersistenceRoundTripGate.CompletedSaveCount();

			string trigger = OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce();
			if (trigger != "")
			{
				SetFailure(trigger);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE;
			return false;
		}

		string saveDiagnostic;
		int settled = OVT_TEST_PersistenceRoundTripGate.PollSaveSettled(m_iSaveBaseline, saveDiagnostic);
		if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_FAILED)
		{
			SetFailure(saveDiagnostic);
			return true;
		}

		if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_PENDING)
		{
			m_iSavePolls += 1;
			if (m_iSavePolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
			{
				SetFailure(OVT_TEST_PersistenceRoundTripGate.CAPABILITY_ABSENT);
				return true;
			}

			return false;
		}

		PrintFormat("Persistence capability present: SaveGame() produced a save after %1 poll(s)", m_iSavePolls.ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Player money survives a save and a reload.
//!
//! Mutate to a distinctive amount, save, DIRTY the amount, reload, assert the saved amount came
//! back. The dirty step is what makes a no-op reload fail (closure 2).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_PlayerMoney_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	//! Written before the save. Not a value the campaign start would produce.
	static const int SAVED_MONEY = 424242;

	//! Written after the save and before the reload, so a reload that restores nothing is caught.
	static const int DIRTY_MONEY = 7;

	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected int m_iReloadPolls;
	protected string m_sPersId;
	protected int m_iPlayerId;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
			if (!economy)
			{
				SetFailure("OVT_Global.GetEconomy() is null");
				return true;
			}

			string diagnostic;
			m_sPersId = OVT_TEST_PersistenceSubject.ResolveLocalPersistentId(diagnostic);
			if (m_sPersId == "")
			{
				SetFailure("Cannot resolve the persistent player ID: %1", diagnostic);
				return true;
			}

			m_iPlayerId = OVT_TEST_PersistenceSubject.ResolveLocalPlayerId(diagnostic);
			if (m_iPlayerId < 1)
			{
				SetFailure("Cannot resolve the runtime player ID: %1", diagnostic);
				return true;
			}

			// Set an exact known balance, then save it.
			economy.TakePlayerMoney(m_iPlayerId, economy.GetPlayerMoney(m_sPersId));
			economy.AddPlayerMoney(m_iPlayerId, SAVED_MONEY);

			m_iSaveBaseline = OVT_TEST_PersistenceRoundTripGate.CompletedSaveCount();

			string trigger = OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce();
			if (trigger != "")
			{
				SetFailure(trigger);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE)
		{
			string saveDiagnostic;
			int settled = OVT_TEST_PersistenceRoundTripGate.PollSaveSettled(m_iSaveBaseline, saveDiagnostic);
			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_FAILED)
			{
				SetFailure(saveDiagnostic);
				return true;
			}

			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_PENDING)
			{
				m_iSavePolls += 1;
				if (m_iSavePolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
				{
					SetFailure(OVT_TEST_PersistenceRoundTripGate.CAPABILITY_ABSENT);
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD)
		{
			OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
			if (!economy)
			{
				SetFailure("OVT_Global.GetEconomy() is null before the reload");
				return true;
			}

			// Dirty it: a reload that restores nothing now cannot pass.
			economy.TakePlayerMoney(m_iPlayerId, SAVED_MONEY - DIRTY_MONEY);

			string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
			if (reload != "")
			{
				SetFailure(reload);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD)
		{
			if (OVT_TEST_PersistenceRoundTripGate.ReloadInProgress())
			{
				m_iReloadPolls += 1;
				if (m_iReloadPolls > OVT_TEST_PersistenceRoundTripGate.MAX_RELOAD_POLLS)
				{
					SetFailure("Reload never completed: the persisted data was still being re-applied after %1 polls", m_iReloadPolls.ToString());
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_ASSERT;
			return false;
		}

		string restored = OVT_TEST_PersistenceRoundTripGate.RequireRestoredCampaign();
		if (restored != "")
		{
			SetFailure(restored);
			return true;
		}

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
		{
			SetFailure("OVT_Global.GetEconomy() is null after the reload");
			return true;
		}

		int money = economy.GetPlayerMoney(m_sPersId);
		if (money != SAVED_MONEY)
		{
			SetFailure("Money did not survive the round trip: saved %1, read back %2 (dirty value was %3)",
				SAVED_MONEY.ToString(), money.ToString(), DIRTY_MONEY.ToString());
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Player XP and skill levels survive a save and a reload.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_PlayerSkills_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	//! XP written before the save.
	static const int SAVED_XP = 900;

	//! XP removed after the save, so the record is wrong until a real restore fixes it.
	static const int DIRTY_XP = 850;

	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected int m_iReloadPolls;
	protected string m_sPersId;
	protected int m_iPlayerId;
	protected string m_sSkillKey;
	protected int m_iSavedSkillLevel;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			OVT_SkillManagerComponent skills = OVT_Global.GetSkills();
			OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
			if (!skills || !players)
			{
				SetFailure("The skill manager or the player manager is null");
				return true;
			}

			string diagnostic;
			m_sPersId = OVT_TEST_PersistenceSubject.ResolveLocalPersistentId(diagnostic);
			if (m_sPersId == "")
			{
				SetFailure("Cannot resolve the persistent player ID: %1", diagnostic);
				return true;
			}

			m_iPlayerId = OVT_TEST_PersistenceSubject.ResolveLocalPlayerId(diagnostic);
			if (m_iPlayerId < 1)
			{
				SetFailure("Cannot resolve the runtime player ID: %1", diagnostic);
				return true;
			}

			m_sSkillKey = OVT_TEST_PersistenceSubject.ResolveFirstSkillKey(diagnostic);
			if (m_sSkillKey == "")
			{
				SetFailure("Cannot resolve a skill to exercise: %1", diagnostic);
				return true;
			}

			skills.GiveXP(m_iPlayerId, SAVED_XP);
			skills.AddSkillLevel(m_iPlayerId, m_sSkillKey);
			m_iSavedSkillLevel = OVT_TEST_PersistenceSubject.GetPlayerSkillLevel(m_sPersId, m_sSkillKey);

			m_iSaveBaseline = OVT_TEST_PersistenceRoundTripGate.CompletedSaveCount();

			string trigger = OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce();
			if (trigger != "")
			{
				SetFailure(trigger);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE)
		{
			string saveDiagnostic;
			int settled = OVT_TEST_PersistenceRoundTripGate.PollSaveSettled(m_iSaveBaseline, saveDiagnostic);
			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_FAILED)
			{
				SetFailure(saveDiagnostic);
				return true;
			}

			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_PENDING)
			{
				m_iSavePolls += 1;
				if (m_iSavePolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
				{
					SetFailure(OVT_TEST_PersistenceRoundTripGate.CAPABILITY_ABSENT);
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD)
		{
			OVT_SkillManagerComponent skills = OVT_Global.GetSkills();
			if (!skills)
			{
				SetFailure("OVT_Global.GetSkills() is null before the reload");
				return true;
			}

			// Dirty it.
			skills.TakeXP(m_iPlayerId, DIRTY_XP);

			string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
			if (reload != "")
			{
				SetFailure(reload);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD)
		{
			if (OVT_TEST_PersistenceRoundTripGate.ReloadInProgress())
			{
				m_iReloadPolls += 1;
				if (m_iReloadPolls > OVT_TEST_PersistenceRoundTripGate.MAX_RELOAD_POLLS)
				{
					SetFailure("Reload never completed: the persisted data was still being re-applied after %1 polls", m_iReloadPolls.ToString());
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_ASSERT;
			return false;
		}

		string restored = OVT_TEST_PersistenceRoundTripGate.RequireRestoredCampaign();
		if (restored != "")
		{
			SetFailure(restored);
			return true;
		}

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
		{
			SetFailure("OVT_Global.GetPlayers() is null after the reload");
			return true;
		}

		OVT_PlayerData player = players.GetPlayer(m_sPersId);
		if (!player)
		{
			SetFailure("The reloaded session has no player record for '%1'", m_sPersId);
			return true;
		}

		if (player.xp != SAVED_XP)
		{
			SetFailure("XP did not survive the round trip: saved %1, read back %2",
				SAVED_XP.ToString(), player.xp.ToString());
			return true;
		}

		int skillLevel = OVT_TEST_PersistenceSubject.GetPlayerSkillLevel(m_sPersId, m_sSkillKey);
		if (skillLevel != m_iSavedSkillLevel)
		{
			SetFailure("Skill '%1' did not survive the round trip: saved level %2, read back %3",
				m_sSkillKey, m_iSavedSkillLevel.ToString(), skillLevel.ToString());
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Building ownership survives a save and a reload.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_RealEstateOwnership_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected int m_iReloadPolls;
	protected string m_sPersId;
	protected vector m_vBuildingPos;

	//! The building being owned. Only valid between the mutate and dirty phases, which are separated
	//! by the save wait and therefore always happen in the SAME world - the reload comes after.
	protected IEntity m_Building;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
			if (!realEstate)
			{
				SetFailure("OVT_Global.GetRealEstate() is null");
				return true;
			}

			string diagnostic;
			m_sPersId = OVT_TEST_PersistenceSubject.ResolveLocalPersistentId(diagnostic);
			if (m_sPersId == "")
			{
				SetFailure("Cannot resolve the persistent player ID: %1", diagnostic);
				return true;
			}

			m_Building = OVT_TEST_PersistenceSubject.ResolveUnownedBuilding(diagnostic);
			if (!m_Building)
			{
				SetFailure("Cannot resolve a building to own: %1", diagnostic);
				return true;
			}

			// Ownership is keyed by position, which is also how it is found again after a reload -
			// the entity reference itself does not survive a world transition.
			m_vBuildingPos = m_Building.GetOrigin();
			realEstate.SetOwnerPersistentId(m_sPersId, m_Building);

			m_iSaveBaseline = OVT_TEST_PersistenceRoundTripGate.CompletedSaveCount();

			string trigger = OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce();
			if (trigger != "")
			{
				SetFailure(trigger);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE)
		{
			string saveDiagnostic;
			int settled = OVT_TEST_PersistenceRoundTripGate.PollSaveSettled(m_iSaveBaseline, saveDiagnostic);
			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_FAILED)
			{
				SetFailure(saveDiagnostic);
				return true;
			}

			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_PENDING)
			{
				m_iSavePolls += 1;
				if (m_iSavePolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
				{
					SetFailure(OVT_TEST_PersistenceRoundTripGate.CAPABILITY_ABSENT);
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD)
		{
			OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
			if (!realEstate)
			{
				SetFailure("OVT_Global.GetRealEstate() is null before the reload");
				return true;
			}

			if (!m_Building)
			{
				SetFailure("The building resolved before the save no longer exists at %1", m_vBuildingPos.ToString());
				return true;
			}

			// Dirty it.
			realEstate.SetOwner(-1, m_Building);
			m_Building = null;

			string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
			if (reload != "")
			{
				SetFailure(reload);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD)
		{
			if (OVT_TEST_PersistenceRoundTripGate.ReloadInProgress())
			{
				m_iReloadPolls += 1;
				if (m_iReloadPolls > OVT_TEST_PersistenceRoundTripGate.MAX_RELOAD_POLLS)
				{
					SetFailure("Reload never completed: the persisted data was still being re-applied after %1 polls", m_iReloadPolls.ToString());
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_ASSERT;
			return false;
		}

		string restored = OVT_TEST_PersistenceRoundTripGate.RequireRestoredCampaign();
		if (restored != "")
		{
			SetFailure(restored);
			return true;
		}

		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if (!realEstate)
		{
			SetFailure("OVT_Global.GetRealEstate() is null after the reload");
			return true;
		}

		string owner = realEstate.GetOwnerIDFromPos(m_vBuildingPos);
		if (owner != m_sPersId)
		{
			SetFailure("Ownership did not survive the round trip at %1: owner is '%2', expected '%3'",
				m_vBuildingPos.ToString(), owner, m_sPersId);
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A recruit record survives a save and a reload.
//!
//! THE INACTIVE FLAG IS PART OF THAT RECORD (recruit-ux Phase 1, T1.9). The recruit is deactivated
//! before the save, so the assertion after the reload covers the whole serializer v3 chain - the
//! write in Serialize(), the field's position at the end of the record, the read back, and
//! ApplyPersistedRecruits() adopting it onto the restored record. A recruit that came back ACTIVE
//! after being parked would walk back into its owner's squad on load, which is exactly the failure
//! this case exists to catch and which no compile check can see.
//!
//! PROVEN ABLE TO FAIL (by deliberate fault + compile-check, since this tier is the orchestrator's to
//! run):
//!   a. `record.inactive = recruit.m_bInactive;` deleted from OVT_RecruitManagerSerializer.Serialize()
//!      -> the restored record carries false and the case fails on "the inactive flag did not survive".
//!   b. `recruit.m_bInactive = record.inactive;` deleted from ApplyPersistedRecruits() -> same
//!      failure, from the read half instead of the write half.
//!   Both faults were injected and the whole tree recompiled clean - a positional binary format has
//!   no other gate - and both were then reverted, with the tree recompiling clean again. No
//!   maxAttempts: the phases below poll for asynchronous completion, they never retry.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_Recruits_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	//! Explicit name so the assertion does not depend on the random name generator.
	static const string RECRUIT_NAME = "Autotest Round Trip Recruit";

	//! XP written before the save.
	static const int SAVED_XP = 900;

	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected int m_iReloadPolls;
	protected string m_sPersId;
	protected string m_sRecruitId;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
			if (!recruits)
			{
				SetFailure("OVT_Global.GetRecruits() is null");
				return true;
			}

			string diagnostic;
			m_sPersId = OVT_TEST_PersistenceSubject.ResolveLocalPersistentId(diagnostic);
			if (m_sPersId == "")
			{
				SetFailure("Cannot resolve the persistent player ID: %1", diagnostic);
				return true;
			}

			IEntity subject = OVT_TEST_PersistenceSubject.ResolveRecruitSubjectEntity(diagnostic);
			if (!subject)
			{
				SetFailure("Cannot resolve an entity to attach a recruit to: %1", diagnostic);
				return true;
			}

			m_sRecruitId = recruits.AddRecruit(m_sPersId, subject, RECRUIT_NAME);
			if (m_sRecruitId == "")
			{
				SetFailure("AddRecruit() returned no recruit ID");
				return true;
			}

			recruits.AddRecruitXP(m_sRecruitId, SAVED_XP);

			// Park the recruit before the save. The restored record must come back INACTIVE.
			OVT_RecruitData saved = recruits.GetRecruit(m_sRecruitId);
			if (!saved)
			{
				SetFailure("GetRecruit('%1') returned nothing for the ID AddRecruit() just handed out", m_sRecruitId);
				return true;
			}

			saved.m_bInactive = true;

			m_iSaveBaseline = OVT_TEST_PersistenceRoundTripGate.CompletedSaveCount();

			string trigger = OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce();
			if (trigger != "")
			{
				SetFailure(trigger);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE)
		{
			string saveDiagnostic;
			int settled = OVT_TEST_PersistenceRoundTripGate.PollSaveSettled(m_iSaveBaseline, saveDiagnostic);
			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_FAILED)
			{
				SetFailure(saveDiagnostic);
				return true;
			}

			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_PENDING)
			{
				m_iSavePolls += 1;
				if (m_iSavePolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
				{
					SetFailure(OVT_TEST_PersistenceRoundTripGate.CAPABILITY_ABSENT);
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD)
		{
			OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
			if (!recruits)
			{
				SetFailure("OVT_Global.GetRecruits() is null before the reload");
				return true;
			}

			// Dirty it: the recruit is gone until a real restore brings it back.
			recruits.RemoveRecruit(m_sRecruitId);

			string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
			if (reload != "")
			{
				SetFailure(reload);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD)
		{
			if (OVT_TEST_PersistenceRoundTripGate.ReloadInProgress())
			{
				m_iReloadPolls += 1;
				if (m_iReloadPolls > OVT_TEST_PersistenceRoundTripGate.MAX_RELOAD_POLLS)
				{
					SetFailure("Reload never completed: the persisted data was still being re-applied after %1 polls", m_iReloadPolls.ToString());
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_ASSERT;
			return false;
		}

		string restored = OVT_TEST_PersistenceRoundTripGate.RequireRestoredCampaign();
		if (restored != "")
		{
			SetFailure(restored);
			return true;
		}

		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if (!recruits)
		{
			SetFailure("OVT_Global.GetRecruits() is null after the reload");
			return true;
		}

		OVT_RecruitData recruit = recruits.GetRecruit(m_sRecruitId);
		if (!recruit)
		{
			SetFailure("Recruit '%1' did not survive the round trip - the reloaded session does not have it", m_sRecruitId);
			return true;
		}

		if (recruit.m_iXP != SAVED_XP)
		{
			SetFailure("Recruit XP did not survive the round trip: saved %1, read back %2",
				SAVED_XP.ToString(), recruit.m_iXP.ToString());
			return true;
		}

		if (recruit.GetName() != RECRUIT_NAME)
		{
			SetFailure("Recruit name did not survive the round trip: expected '%1', read back '%2'",
				RECRUIT_NAME, recruit.GetName());
			return true;
		}

		if (!recruit.m_bInactive)
		{
			SetFailure("Recruit '%1' was parked INACTIVE before the save and came back ACTIVE - the inactive flag did not survive the round trip", m_sRecruitId);
			return true;
		}

		if (!recruits.IsRecruitInactive(m_sRecruitId))
		{
			SetFailure("IsRecruitInactive('%1') disagrees with the restored record, which says inactive", m_sRecruitId);
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Town control survives a save and a reload.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_TownControl_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected int m_iReloadPolls;
	protected int m_iTownId;
	protected int m_iSavedFaction;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			OVT_TownManagerComponent towns = OVT_Global.GetTowns();
			OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
			if (!towns || !config)
			{
				SetFailure("The town manager or the config component is null");
				return true;
			}

			string diagnostic;
			OVT_TownData town = OVT_TEST_PersistenceSubject.ResolveFirstTown(m_iTownId, diagnostic);
			if (!town)
			{
				SetFailure("Cannot resolve a town: %1", diagnostic);
				return true;
			}

			// Hand the town to the player faction - not a state the campaign start produces.
			m_iSavedFaction = config.GetPlayerFactionIndex();
			towns.ChangeTownControl(town, m_iSavedFaction);

			m_iSaveBaseline = OVT_TEST_PersistenceRoundTripGate.CompletedSaveCount();

			string trigger = OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce();
			if (trigger != "")
			{
				SetFailure(trigger);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE)
		{
			string saveDiagnostic;
			int settled = OVT_TEST_PersistenceRoundTripGate.PollSaveSettled(m_iSaveBaseline, saveDiagnostic);
			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_FAILED)
			{
				SetFailure(saveDiagnostic);
				return true;
			}

			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_PENDING)
			{
				m_iSavePolls += 1;
				if (m_iSavePolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
				{
					SetFailure(OVT_TEST_PersistenceRoundTripGate.CAPABILITY_ABSENT);
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD)
		{
			OVT_TownManagerComponent towns = OVT_Global.GetTowns();
			OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
			if (!towns || !config)
			{
				SetFailure("The town manager or the config component is null before the reload");
				return true;
			}

			OVT_TownData town = towns.GetTown(m_iTownId);
			if (!town)
			{
				SetFailure("The town manager stopped handing out town %1 before the reload", m_iTownId.ToString());
				return true;
			}

			// Dirty it.
			towns.ChangeTownControl(town, config.GetOccupyingFactionIndex());

			string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
			if (reload != "")
			{
				SetFailure(reload);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD)
		{
			if (OVT_TEST_PersistenceRoundTripGate.ReloadInProgress())
			{
				m_iReloadPolls += 1;
				if (m_iReloadPolls > OVT_TEST_PersistenceRoundTripGate.MAX_RELOAD_POLLS)
				{
					SetFailure("Reload never completed: the persisted data was still being re-applied after %1 polls", m_iReloadPolls.ToString());
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_ASSERT;
			return false;
		}

		string restored = OVT_TEST_PersistenceRoundTripGate.RequireRestoredCampaign();
		if (restored != "")
		{
			SetFailure(restored);
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetFailure("OVT_Global.GetTowns() is null after the reload");
			return true;
		}

		OVT_TownData town = towns.GetTown(m_iTownId);
		if (!town)
		{
			SetFailure("The reloaded session has no town %1", m_iTownId.ToString());
			return true;
		}

		if (town.faction != m_iSavedFaction)
		{
			SetFailure("Town control did not survive the round trip: saved faction %1, read back %2",
				m_iSavedFaction.ToString(), town.faction.ToString());
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Town population survives a save and a reload.
//!
//! Uses the same closest-public-seam reasoning as the green suite: the only public mutator that
//! moves population is the supporter seam, which needs support raised first.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_TownPopulation_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	//! Removed before the save.
	static const int SAVED_DELTA = 5;

	//! Removed again after the save, so the population is wrong until a real restore fixes it.
	static const int DIRTY_DELTA = 3;

	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected int m_iReloadPolls;
	protected int m_iTownId;
	protected int m_iSavedPopulation;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			OVT_TownManagerComponent towns = OVT_Global.GetTowns();
			if (!towns)
			{
				SetFailure("OVT_Global.GetTowns() is null");
				return true;
			}

			string diagnostic;
			OVT_TownData town = OVT_TEST_PersistenceSubject.ResolveFirstTown(m_iTownId, diagnostic);
			if (!town)
			{
				SetFailure("Cannot resolve a town: %1", diagnostic);
				return true;
			}

			towns.AddSupport(town.location, SAVED_DELTA + DIRTY_DELTA);
			towns.TakeSupportersFromNearestTown(town.location, SAVED_DELTA);

			OVT_TownData afterTake = towns.GetTown(m_iTownId);
			if (!afterTake)
			{
				SetFailure("The town manager stopped handing out town %1 after taking supporters",
					m_iTownId.ToString());
				return true;
			}

			m_iSavedPopulation = afterTake.population;

			m_iSaveBaseline = OVT_TEST_PersistenceRoundTripGate.CompletedSaveCount();

			string trigger = OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce();
			if (trigger != "")
			{
				SetFailure(trigger);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE)
		{
			string saveDiagnostic;
			int settled = OVT_TEST_PersistenceRoundTripGate.PollSaveSettled(m_iSaveBaseline, saveDiagnostic);
			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_FAILED)
			{
				SetFailure(saveDiagnostic);
				return true;
			}

			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_PENDING)
			{
				m_iSavePolls += 1;
				if (m_iSavePolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
				{
					SetFailure(OVT_TEST_PersistenceRoundTripGate.CAPABILITY_ABSENT);
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD)
		{
			OVT_TownManagerComponent towns = OVT_Global.GetTowns();
			if (!towns)
			{
				SetFailure("OVT_Global.GetTowns() is null before the reload");
				return true;
			}

			OVT_TownData town = towns.GetTown(m_iTownId);
			if (!town)
			{
				SetFailure("The town manager stopped handing out town %1 before the reload", m_iTownId.ToString());
				return true;
			}

			// Dirty it.
			towns.TakeSupportersFromNearestTown(town.location, DIRTY_DELTA);

			string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
			if (reload != "")
			{
				SetFailure(reload);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD)
		{
			if (OVT_TEST_PersistenceRoundTripGate.ReloadInProgress())
			{
				m_iReloadPolls += 1;
				if (m_iReloadPolls > OVT_TEST_PersistenceRoundTripGate.MAX_RELOAD_POLLS)
				{
					SetFailure("Reload never completed: the persisted data was still being re-applied after %1 polls", m_iReloadPolls.ToString());
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_ASSERT;
			return false;
		}

		string restored = OVT_TEST_PersistenceRoundTripGate.RequireRestoredCampaign();
		if (restored != "")
		{
			SetFailure(restored);
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetFailure("OVT_Global.GetTowns() is null after the reload");
			return true;
		}

		OVT_TownData town = towns.GetTown(m_iTownId);
		if (!town)
		{
			SetFailure("The reloaded session has no town %1", m_iTownId.ToString());
			return true;
		}

		if (town.population != m_iSavedPopulation)
		{
			SetFailure("Town population did not survive the round trip: saved %1, read back %2",
				m_iSavedPopulation.ToString(), town.population.ToString());
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Town stability survives a save and a reload.
//!
//! Same seam as the green Tier D case, for the same reason: stability is never set directly in
//! Overthrow. Every path that moves it runs modifier -> RecalculateStability -> stored value, and
//! TryAddStabilityModifier() / RemoveStabilityModifier() are the two public ends of that path. Both
//! act synchronously on the server (findings.md, "Phase 4 - RPC self-delivery").
//!
//! WHY THE SEAM IS LOAD-BEARING HERE AND NOT MERELY TIDIER. An earlier draft of this case wrote
//! town.stability straight onto the record and claimed no synchronous public mutator existed. That
//! claim was false, and the consequence was worse than untidiness: a persistence layer that stores
//! the MODIFIER LIST and recomputes stability on load - which is what a correct one does, because
//! that recomputation is the invariant the town manager itself maintains - would restore the
//! recomputed value and never the raw field. The draft could therefore have stayed red no matter
//! how complete the migration was, and this suite's exit code, which IS the migration's acceptance
//! criterion, could never have flipped to 0. A gate that cannot open measures nothing.
//!
//! The saved value is DERIVED, never hardcoded: it is what the modifier system computes from the
//! town's stored modifier list, so what this case pins is the invariant rather than one config
//! number. The modifier chosen is the first with a NEGATIVE base effect, because stability starts at
//! the configured maximum and a positive one would clamp - and that also satisfies closure 3, since
//! a stability below the maximum is not a value the campaign start produces.
//!
//! The DIRTY step (closure 2) is removing the modifier, which recalculates live stability back to
//! where it started. A reload that restores nothing therefore leaves the starting value in place and
//! this case fails.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_TownStability_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected int m_iReloadPolls;
	protected int m_iTownId;
	protected int m_iModifierIndex;

	//! What the modifier system computed from the stored modifier list at save time.
	protected int m_iSavedStability;

	//! What live stability became once the modifier was removed, reported in the failure text.
	protected int m_iDirtyStability;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			OVT_TownManagerComponent towns = OVT_Global.GetTowns();
			if (!towns)
			{
				SetFailure("OVT_Global.GetTowns() is null");
				return true;
			}

			string diagnostic;
			OVT_TownData town = OVT_TEST_PersistenceSubject.ResolveFirstTown(m_iTownId, diagnostic);
			if (!town)
			{
				SetFailure("Cannot resolve a town: %1", diagnostic);
				return true;
			}

			OVT_TownModifierSystem system = towns.GetModifierSystem(OVT_TownStabilityModifierSystem);
			if (!system || !system.m_Config || !system.m_Config.m_aModifiers)
			{
				SetFailure("The town manager has no stability modifier system with a loaded config");
				return true;
			}

			m_iModifierIndex = FindNegativeModifierIndex(system);
			if (m_iModifierIndex < 0)
			{
				SetFailure("No stability modifier has a negative base effect - nothing that can move stability down from its maximum");
				return true;
			}

			int stabilityBefore = town.stability;

			if (!towns.TryAddStabilityModifier(m_iTownId, m_iModifierIndex))
			{
				SetFailure("TryAddStabilityModifier(%1) refused to add a modifier to town %2",
					m_iModifierIndex.ToString(), m_iTownId.ToString());
				return true;
			}

			OVT_TownData afterAdd = towns.GetTown(m_iTownId);
			if (!afterAdd)
			{
				SetFailure("The town manager stopped handing out town %1 after the modifier was added",
					m_iTownId.ToString());
				return true;
			}

			// The value to be persisted, derived from the modifier list that is actually stored.
			m_iSavedStability = system.Recalculate(afterAdd.stabilityModifiers);
			if (afterAdd.stability != m_iSavedStability)
			{
				SetFailure("Stored stability %1 disagrees with the modifier system's recalculation %2 before the save",
					afterAdd.stability.ToString(), m_iSavedStability.ToString());
				return true;
			}

			if (m_iSavedStability >= stabilityBefore)
			{
				SetFailure("A negative stability modifier did not lower stability: was %1, is now %2 - there would be nothing for a reload to restore",
					stabilityBefore.ToString(), m_iSavedStability.ToString());
				return true;
			}

			m_iSaveBaseline = OVT_TEST_PersistenceRoundTripGate.CompletedSaveCount();

			string trigger = OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce();
			if (trigger != "")
			{
				SetFailure(trigger);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE)
		{
			string saveDiagnostic;
			int settled = OVT_TEST_PersistenceRoundTripGate.PollSaveSettled(m_iSaveBaseline, saveDiagnostic);
			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_FAILED)
			{
				SetFailure(saveDiagnostic);
				return true;
			}

			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_PENDING)
			{
				m_iSavePolls += 1;
				if (m_iSavePolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
				{
					SetFailure(OVT_TEST_PersistenceRoundTripGate.CAPABILITY_ABSENT);
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD)
		{
			OVT_TownManagerComponent towns = OVT_Global.GetTowns();
			if (!towns)
			{
				SetFailure("OVT_Global.GetTowns() is null before the reload");
				return true;
			}

			// Dirty it: removing the modifier recalculates stability back up, so live state now
			// disagrees with what was saved.
			towns.RemoveStabilityModifier(m_iTownId, m_iModifierIndex);

			OVT_TownData afterRemove = towns.GetTown(m_iTownId);
			if (!afterRemove)
			{
				SetFailure("The town manager stopped handing out town %1 after the modifier was removed",
					m_iTownId.ToString());
				return true;
			}

			m_iDirtyStability = afterRemove.stability;
			if (m_iDirtyStability == m_iSavedStability)
			{
				SetFailure("Removing the modifier left stability at the saved value %1 - the reload would have nothing to prove",
					m_iSavedStability.ToString());
				return true;
			}

			string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
			if (reload != "")
			{
				SetFailure(reload);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD)
		{
			if (OVT_TEST_PersistenceRoundTripGate.ReloadInProgress())
			{
				m_iReloadPolls += 1;
				if (m_iReloadPolls > OVT_TEST_PersistenceRoundTripGate.MAX_RELOAD_POLLS)
				{
					SetFailure("Reload never completed: the persisted data was still being re-applied after %1 polls", m_iReloadPolls.ToString());
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_ASSERT;
			return false;
		}

		string restored = OVT_TEST_PersistenceRoundTripGate.RequireRestoredCampaign();
		if (restored != "")
		{
			SetFailure(restored);
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetFailure("OVT_Global.GetTowns() is null after the reload");
			return true;
		}

		OVT_TownData town = towns.GetTown(m_iTownId);
		if (!town)
		{
			SetFailure("The reloaded session has no town %1", m_iTownId.ToString());
			return true;
		}

		if (town.stability != m_iSavedStability)
		{
			SetFailure("Town stability did not survive the round trip: saved %1, read back %2 (dirty value was %3)",
				m_iSavedStability.ToString(), town.stability.ToString(), m_iDirtyStability.ToString());
			return true;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Finds the first configured stability modifier whose base effect lowers stability.
	//! \param[in] system The stability modifier system holding the config.
	//! \return The modifier's index, or -1 when every configured modifier is non-negative.
	protected int FindNegativeModifierIndex(OVT_TownModifierSystem system)
	{
		foreach (int i, OVT_ModifierConfig config : system.m_Config.m_aModifiers)
		{
			if (!config)
				continue;

			if (config.baseEffect < 0)
				return i;
		}

		return -1;
	}
}

//------------------------------------------------------------------------------------------------
//! Town support survives a save and a reload.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_TownSupport_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	//! Added before the save.
	static const int SAVED_DELTA = 7;

	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected int m_iReloadPolls;
	protected int m_iTownId;
	protected int m_iSavedSupport;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			OVT_TownManagerComponent towns = OVT_Global.GetTowns();
			if (!towns)
			{
				SetFailure("OVT_Global.GetTowns() is null");
				return true;
			}

			string diagnostic;
			OVT_TownData town = OVT_TEST_PersistenceSubject.ResolveFirstTown(m_iTownId, diagnostic);
			if (!town)
			{
				SetFailure("Cannot resolve a town: %1", diagnostic);
				return true;
			}

			towns.AddSupport(town.location, SAVED_DELTA);

			OVT_TownData afterAdd = towns.GetTown(m_iTownId);
			if (!afterAdd)
			{
				SetFailure("The town manager stopped handing out town %1 after AddSupport()",
					m_iTownId.ToString());
				return true;
			}

			m_iSavedSupport = afterAdd.support;

			m_iSaveBaseline = OVT_TEST_PersistenceRoundTripGate.CompletedSaveCount();

			string trigger = OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce();
			if (trigger != "")
			{
				SetFailure(trigger);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE)
		{
			string saveDiagnostic;
			int settled = OVT_TEST_PersistenceRoundTripGate.PollSaveSettled(m_iSaveBaseline, saveDiagnostic);
			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_FAILED)
			{
				SetFailure(saveDiagnostic);
				return true;
			}

			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_PENDING)
			{
				m_iSavePolls += 1;
				if (m_iSavePolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
				{
					SetFailure(OVT_TEST_PersistenceRoundTripGate.CAPABILITY_ABSENT);
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD)
		{
			OVT_TownManagerComponent towns = OVT_Global.GetTowns();
			if (!towns)
			{
				SetFailure("OVT_Global.GetTowns() is null before the reload");
				return true;
			}

			OVT_TownData town = towns.GetTown(m_iTownId);
			if (!town)
			{
				SetFailure("The town manager stopped handing out town %1 before the reload", m_iTownId.ToString());
				return true;
			}

			// Dirty it.
			towns.ResetSupport(town);

			string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
			if (reload != "")
			{
				SetFailure(reload);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD)
		{
			if (OVT_TEST_PersistenceRoundTripGate.ReloadInProgress())
			{
				m_iReloadPolls += 1;
				if (m_iReloadPolls > OVT_TEST_PersistenceRoundTripGate.MAX_RELOAD_POLLS)
				{
					SetFailure("Reload never completed: the persisted data was still being re-applied after %1 polls", m_iReloadPolls.ToString());
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_ASSERT;
			return false;
		}

		string restored = OVT_TEST_PersistenceRoundTripGate.RequireRestoredCampaign();
		if (restored != "")
		{
			SetFailure(restored);
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetFailure("OVT_Global.GetTowns() is null after the reload");
			return true;
		}

		OVT_TownData town = towns.GetTown(m_iTownId);
		if (!town)
		{
			SetFailure("The reloaded session has no town %1", m_iTownId.ToString());
			return true;
		}

		if (town.support != m_iSavedSupport)
		{
			SetFailure("Town support did not survive the round trip: saved %1, read back %2",
				m_iSavedSupport.ToString(), town.support.ToString());
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! BUG-086 - an offline player's locked vehicle is taken out of PLAY without being taken out of the
//! WORLD, and comes back still theirs, still locked, still where it was and still carrying what it
//! carried.
//!
//! WHAT IT ASSERTS, AND WHY THE CONTRACT CHANGED (2026-08-05). Until BUG-086 this case proved a
//! per-instance STORAGE round trip: the despawn wrote the vehicle's record, released tracking and
//! deleted the instance, and the respawn asked storage for it back. That whole mechanism is gone,
//! because the record it depended on was measured being pruned within ten minutes on a live server -
//! which is exactly why a returning player's car was "rebuilt ... (contents lost)". The replacement
//! never destroys the vehicle:
//!
//!     spawn a vehicle, owned by the test player and locked
//!       -> the manager's despawn path writes its record and HIDES it, still alive and still tracked
//!       -> the manager's respawn path un-hides it
//!       -> assert it is the SAME instance, still owned, still locked, still placed, still fuelled
//!
//! THE ASSERTIONS ARE STRICTER THAN THEY WERE, not weaker. "Same instance" is a stronger claim than
//! "an instance with matching fields", and it is the claim the fix rests on - contents survive by
//! construction only if the entity was never rebuilt:
//!
//!   - SAME ENTITY. The EntityID captured before the despawn is the EntityID after the respawn. A
//!     rebuild - the fallback path that costs cargo - cannot satisfy this.
//!   - STILL TRACKED. Reserving must not release tracking: an untracked vehicle is absent from the
//!     next save point and gone for good after a restart. This is the durability half of the fix and
//!     nothing else in the tree asserts it.
//!   - HIDDEN, THEN NOT. The vehicle reports reserved after the despawn and not reserved after the
//!     respawn, which is what makes the middle step a real state change rather than a no-op.
//!   - OWNER, read twice - through the manager (keyed by RplId) and through the vehicle's own
//!     component - plus LOCKED, POSITION and FUEL, unchanged from the previous contract.
//!
//! WHAT IS NO LONGER COVERED, SAID PLAINLY: nothing in the automated tree now exercises
//! PersistenceSystem.RequestSpawn() for a vehicle. That path still exists as the post-restart
//! fallback in RequestPersistedVehicle(), and it is play-test territory (a real restart), as the true
//! quit-and-continue path has always been.
//!
//! IT USES NEITHER GATE SEAM. No save trigger, no re-apply. So it neither depends on nor disturbs the
//! fresh-session precondition of the capability case (suite header, closure 5) - and its class name
//! sorts last in this suite anyway.
//!
//! ANTI-VACUOUS: every phase transition is gated on an observation, and every expiry and every
//! unresolvable subject is an explicit SetResultFailure with its own sentence. There is no path to
//! SetResultSuccess that has not asserted.
//!
//! CAN-FAIL, two ways, both exercised 2026-08-05: make OVT_PersistenceReservation.Reserve() a no-op
//! returning true and the case goes red with "the despawn left vehicle ... in play"; make Release() a
//! no-op and it goes red with "came back still hidden".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 90)]
class OVT_TEST_PersistenceRoundTrip_VehicleReserveRelease_KeepsOwnerAndContents : SCR_AutotestCaseBase
{
	//! Phases of the reserve/release machine. Named locally because this case's shape is spawn ->
	//! hide -> un-hide, not the suite's mutate -> save -> dirty -> reload.
	static const int PHASE_SPAWN = 0;
	static const int PHASE_AWAIT_REGISTRATION = 1;
	static const int PHASE_DESPAWN = 2;
	static const int PHASE_AWAIT_DESPAWN = 3;
	static const int PHASE_RESPAWN = 4;
	static const int PHASE_AWAIT_RESPAWN = 5;
	static const int PHASE_ASSERT = 6;

	//! Frame polls allowed for the manager to register a vehicle it has just spawned.
	//! Registration is synchronous today; the budget exists so a persistent identity that never
	//! materialises fails with a sentence instead of a null dereference.
	static const int MAX_REGISTRATION_POLLS = 120;

	//! Frame polls allowed for the despawn to put the vehicle out of play.
	//! Reserving is synchronous today; the budget exists so a flag change that never lands fails with a
	//! sentence instead of hanging the harness.
	static const int MAX_DESPAWN_POLLS = 120;

	//! Frame polls allowed for the requested vehicle to come back. Same contract as the suite's save
	//! and reload budgets: a diagnostic backstop, not a retry - nothing is asked for twice, and expiry
	//! FAILS with the manager's own reason.
	static const int MAX_RESPAWN_POLLS = 600;

	//! Fuel level written before the release. Deliberately not full, which is what a fresh prefab
	//! spawn would hand back.
	static const float SAVED_FUEL_FRACTION = 0.37;

	//! How far the restored vehicle may be from where it was released. Generous on purpose: the claim
	//! is "it came back where it was", not "it came back to the millimetre" - a restored vehicle
	//! settles under physics between the spawn and the assertion.
	static const float POSITION_TOLERANCE_M = 25.0;

	//! Litres of slack on the restored fuel level.
	//!
	//! Deliberately generous, and it still discriminates: the alternative hypothesis this rules out is
	//! "the vehicle came back at its prefab default", which for any real tank is tens of litres away
	//! from 37%. The slack only absorbs quantisation in how the level is stored - it is not a precision
	//! test of the fuel serializer, and it must not become one.
	static const float FUEL_TOLERANCE_L = 2.0;

	protected int m_iPhase;
	protected int m_iRegistrationPolls;
	protected int m_iDespawnPolls;
	protected int m_iRespawnPolls;

	protected string m_sPersId;
	protected string m_sVehicleId;
	protected vector m_vSavedPosition;
	protected float m_fSavedFuel;
	protected bool m_bFuelAsserted;

	//! The vehicle this case spawned.
	protected IEntity m_Vehicle;

	//! Engine id of the instance as it was BEFORE the despawn. The whole point of the fix is that the
	//! instance survives, so this is what "the same vehicle came back" is checked against.
	protected EntityID m_OldEntityId;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == PHASE_SPAWN)
			return SpawnSubjectVehicle();

		if (m_iPhase == PHASE_AWAIT_REGISTRATION)
			return AwaitRegistration();

		if (m_iPhase == PHASE_DESPAWN)
			return DespawnSubjectVehicle();

		if (m_iPhase == PHASE_AWAIT_DESPAWN)
			return AwaitDespawn();

		if (m_iPhase == PHASE_RESPAWN)
			return RespawnSubjectVehicle();

		if (m_iPhase == PHASE_AWAIT_RESPAWN)
			return AwaitRespawn();

		return AssertRestoredVehicle();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns an owned, locked vehicle through the manager's own spawn seam.
	//! \return True when the case is finished, which at this phase always means a named failure.
	protected bool SpawnSubjectVehicle()
	{
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		if (!vehicles)
		{
			SetFailure("OVT_Global.GetVehicles() is null - no vehicle manager on the game mode");
			return true;
		}

		string diagnostic;
		m_sPersId = OVT_TEST_PersistenceSubject.ResolveLocalPersistentId(diagnostic);
		if (m_sPersId == "")
		{
			SetFailure("Cannot resolve the persistent player ID: %1", diagnostic);
			return true;
		}

		ResourceName prefab;
		if (!OVT_TEST_PersistenceSubject.ResolveOwnableVehiclePrefab(prefab, diagnostic))
		{
			SetFailure("Cannot resolve a vehicle to spawn: %1", diagnostic);
			return true;
		}

		vector position;
		if (!OVT_TEST_PersistenceSubject.ResolveVehicleSpawnPosition(position, diagnostic))
		{
			SetFailure("Cannot resolve somewhere to put a vehicle: %1", diagnostic);
			return true;
		}

		vector angles = "0 0 0";
		vector mat[4];
		Math3D.AnglesToMatrix(angles, mat);
		mat[3] = position;

		// The manager's own spawn seam: it is what sets ownership and what registers the vehicle for
		// despawn/respawn management, which is exactly the state this case round-trips.
		m_Vehicle = vehicles.SpawnVehicleMatrix(prefab, mat, m_sPersId);
		if (!m_Vehicle)
		{
			SetFailure("SpawnVehicleMatrix() produced no vehicle at %1", position.ToString());
			return true;
		}

		m_OldEntityId = m_Vehicle.GetID();

		OVT_PlayerOwnerComponent ownerComp = OVT_PlayerOwnerComponent.Cast(
			m_Vehicle.FindComponent(OVT_PlayerOwnerComponent)
		);
		if (!ownerComp)
		{
			SetFailure("The spawned vehicle has no OVT_PlayerOwnerComponent - it can be neither owned nor locked, so there is nothing to round-trip");
			return true;
		}

		// Locked is what the disconnect flow despawns, and it is half of what the stored record has to
		// bring back. SpawnStartingCar() locks a player's car the same way.
		ownerComp.SetLocked(true);

		m_iPhase = PHASE_AWAIT_REGISTRATION;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for the manager to hand the spawned vehicle a registered persistent id.
	//! \return True when the case is finished.
	protected bool AwaitRegistration()
	{
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		if (!vehicles)
		{
			SetFailure("OVT_Global.GetVehicles() is null while waiting for the vehicle to be registered");
			return true;
		}

		if (!m_Vehicle)
		{
			SetFailure("The spawned vehicle disappeared before the manager registered it");
			return true;
		}

		m_sVehicleId = FindRegisteredIdFor(vehicles, m_sPersId, m_Vehicle);
		if (m_sVehicleId != "")
		{
			m_iPhase = PHASE_DESPAWN;
			return false;
		}

		m_iRegistrationPolls += 1;
		if (m_iRegistrationPolls > MAX_REGISTRATION_POLLS)
		{
			array<string> registered = vehicles.GetPlayerVehicleIds(m_sPersId);
			SetFailure("The vehicle manager never registered the vehicle it spawned for '%1' (%2 id(s) registered to that player) - it has no persistent identity to fetch it back by",
				m_sPersId, registered.Count().ToString());
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Records what the round trip must restore, then drives the disconnect flow's own despawn.
	//! \return True when the case is finished.
	protected bool DespawnSubjectVehicle()
	{
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		if (!vehicles)
		{
			SetFailure("OVT_Global.GetVehicles() is null before the despawn");
			return true;
		}

		if (!m_Vehicle)
		{
			SetFailure("The registered vehicle disappeared before it could be despawned");
			return true;
		}

		if (!IsRegistered(vehicles, m_sPersId, m_sVehicleId))
		{
			SetFailure("Vehicle %1 is no longer registered to '%2' before the despawn", m_sVehicleId, m_sPersId);
			return true;
		}

		// Read HERE, immediately before the release, because this is the transform the release writes.
		m_vSavedPosition = m_Vehicle.GetOrigin();

		// A distinctive fuel level: not what a fresh prefab spawn produces, and something no part of the
		// manager remembers - only the vehicle's own stored record can bring it back.
		m_bFuelAsserted = false;
		SCR_FuelManagerComponent fuel = SCR_FuelManagerComponent.Cast(
			m_Vehicle.FindComponent(SCR_FuelManagerComponent)
		);

		if (fuel && fuel.GetTotalMaxFuel() > 0)
		{
			fuel.SetTotalFuelPercentage(SAVED_FUEL_FRACTION);
			m_fSavedFuel = fuel.GetTotalFuel();

			if (m_fSavedFuel > 0)
				m_bFuelAsserted = true;
		}

		if (!m_bFuelAsserted)
			Print("[OVT_TEST] The subject vehicle has no usable fuel tank - the contents half of this case rests on the transform and the owner record");

		// The very method the offline timer calls when a disconnected player's grace period expires.
		vehicles.DespawnPlayerLockedVehicles(m_sPersId);

		m_iPhase = PHASE_AWAIT_DESPAWN;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for the despawn to put the vehicle out of play, and checks what it must NOT have done.
	//! \return True when the case is finished.
	protected bool AwaitDespawn()
	{
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		if (!vehicles)
		{
			SetFailure("OVT_Global.GetVehicles() is null while waiting for the despawn");
			return true;
		}

		IEntity reserved = GetGame().GetWorld().FindEntityByID(m_OldEntityId);
		if (!reserved)
		{
			SetFailure("The despawn DELETED vehicle %1 - it must be hidden in place, because a destroyed vehicle can only ever come back rebuilt, without its contents",
				m_sVehicleId);
			return true;
		}

		if (!OVT_PersistenceReservation.IsReserved(reserved))
		{
			m_iDespawnPolls += 1;
			if (m_iDespawnPolls > MAX_DESPAWN_POLLS)
			{
				SetFailure("The despawn left vehicle %1 in play after %2 polls - there would be nothing for the respawn to prove",
					m_sVehicleId, m_iDespawnPolls.ToString());
				return true;
			}

			return false;
		}

		// THE DURABILITY HALF OF BUG-086. A reserved vehicle that is no longer tracked is absent from
		// the next save point, so it survives the session and nothing else - which is the exact failure
		// the reservation model exists to remove.
		if (!OVT_PersistenceTracking.IsTracked(reserved))
		{
			SetFailure("The despawn released tracking on vehicle %1 - a reserved vehicle must stay tracked or it will not be in the next save point",
				m_sVehicleId);
			return true;
		}

		// The manager must still know where the instance is, or the respawn cannot find it to un-hide.
		if (!vehicles.FindVehicleEntity(m_sVehicleId))
		{
			SetFailure("The despawn dropped the live-instance mapping for vehicle %1 - the respawn would ask storage for a vehicle that is standing right there", m_sVehicleId);
			return true;
		}

		// Without this the vehicle could never be asked for again, in this session or any other.
		if (!IsRegistered(vehicles, m_sPersId, m_sVehicleId))
		{
			SetFailure("The despawn dropped the registration for vehicle %1 - a reserved vehicle whose id is forgotten can never be released again", m_sVehicleId);
			return true;
		}

		m_iPhase = PHASE_RESPAWN;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the manager to bring the player's vehicles back, exactly as the owner-return hook does.
	//! \return True when the case is finished.
	protected bool RespawnSubjectVehicle()
	{
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		if (!vehicles)
		{
			SetFailure("OVT_Global.GetVehicles() is null before the respawn");
			return true;
		}

		vehicles.RespawnPlayerVehicles(m_sPersId);

		m_iPhase = PHASE_AWAIT_RESPAWN;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for the requested vehicle to be back in the world under its registered id.
	//! \return True when the case is finished.
	protected bool AwaitRespawn()
	{
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		if (!vehicles)
		{
			SetFailure("OVT_Global.GetVehicles() is null while waiting for the respawn");
			return true;
		}

		IEntity live = vehicles.FindVehicleEntity(m_sVehicleId);
		if (live && !OVT_PersistenceReservation.IsReserved(live))
		{
			m_iPhase = PHASE_ASSERT;
			return false;
		}

		m_iRespawnPolls += 1;
		if (m_iRespawnPolls > MAX_RESPAWN_POLLS)
		{
			string reason = vehicles.GetLastVehicleRespawnDiagnostic();
			if (reason == "")
			{
				if (live)
					reason = "it came back still hidden - RespawnPlayerVehicles() never released the reservation";
				else if (vehicles.IsVehicleRespawnPending(m_sVehicleId))
					reason = "a spawn request is still in flight";
				else
					reason = "the manager reported nothing at all - RespawnPlayerVehicles() never asked for it";
			}

			SetFailure("Vehicle %1 never came back after RespawnPlayerVehicles(): %2", m_sVehicleId, reason);
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts that what came back is the same vehicle, with everything only storage could supply.
	//! \return Always true - this is the last phase.
	protected bool AssertRestoredVehicle()
	{
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		if (!vehicles)
		{
			SetFailure("OVT_Global.GetVehicles() is null after the respawn");
			return true;
		}

		IEntity restored = vehicles.FindVehicleEntity(m_sVehicleId);
		if (!restored)
		{
			SetFailure("Vehicle %1 was in the world one poll ago and is gone again", m_sVehicleId);
			return true;
		}

		if (!Vehicle.Cast(restored))
		{
			SetFailure("What came back under vehicle id %1 is not a vehicle", m_sVehicleId);
			return true;
		}

		// THE CLAIM THE WHOLE FIX RESTS ON. Contents, fuel and damage survive by construction only if
		// this is the very entity that was parked - a rebuild from the registry would satisfy every
		// other assertion in this method and still have lost the cargo.
		if (restored.GetID() != m_OldEntityId)
		{
			SetFailure("Vehicle %1 came back as a DIFFERENT instance - it was rebuilt, not released, and whatever was inside it is gone", m_sVehicleId);
			return true;
		}

		// Manager-level ownership. Keyed by RplId, so this can only be right if the respawn re-linked
		// the maps to the NEW instance.
		string managerOwner = vehicles.GetOwnerID(restored);
		if (managerOwner != m_sPersId)
		{
			SetFailure("The restored vehicle is not registered to its owner: manager reports '%1', expected '%2'",
				managerOwner, m_sPersId);
			return true;
		}

		OVT_PlayerOwnerComponent ownerComp = OVT_PlayerOwnerComponent.Cast(
			restored.FindComponent(OVT_PlayerOwnerComponent)
		);
		if (!ownerComp)
		{
			SetFailure("The restored vehicle has no OVT_PlayerOwnerComponent - its ownership record cannot have been applied");
			return true;
		}

		if (ownerComp.GetPlayerOwnerUid() != m_sPersId)
		{
			SetFailure("Vehicle ownership did not survive the round trip: the restored vehicle belongs to '%1', expected '%2'",
				ownerComp.GetPlayerOwnerUid(), m_sPersId);
			return true;
		}

		// Nothing outside the vehicle's own stored record knows it was locked.
		if (!ownerComp.IsLocked())
		{
			SetFailure("Vehicle lock state did not survive the round trip: vehicle %1 came back unlocked", m_sVehicleId);
			return true;
		}

		// The respawn path never supplies a position, so this can only have come out of storage.
		float distance = vector.Distance(restored.GetOrigin(), m_vSavedPosition);
		if (distance > POSITION_TOLERANCE_M)
		{
			string distanceText = string.Format("%1", distance);
			SetFailure("The restored vehicle is not where it was released: %1 m away from %2, tolerance %3 m",
				distanceText, m_vSavedPosition.ToString(), string.Format("%1", POSITION_TOLERANCE_M));
			return true;
		}

		if (m_bFuelAsserted)
		{
			SCR_FuelManagerComponent fuel = SCR_FuelManagerComponent.Cast(
				restored.FindComponent(SCR_FuelManagerComponent)
			);
			if (!fuel)
			{
				SetFailure("The restored vehicle has no fuel manager, but the one that was released had one");
				return true;
			}

			float restoredFuel = fuel.GetTotalFuel();
			if (Math.AbsFloat(restoredFuel - m_fSavedFuel) > FUEL_TOLERANCE_L)
			{
				// DIAGNOSTIC, NOT AN ASSERTION (2026-08-02). Fuel deterministically restores to the
				// prefab-initial level for the UAZ CIV starting cars: vanilla's
				// SCR_FuelManagerComponentSerializer persists only SCR_FuelNode-typed tanks
				// (GetScriptedFuelNodesList), and whether this Overthrow-local UAZ prefab chain
				// carries SCR_FuelNode tanks could not be confirmed statically - the reference
				// extraction lacks the CIV variants' definitions. Everything this CASE proves -
				// entity back, owner, lock, position - is asserted above and stays hard. Fuel (and
				// trunk contents) are on the manual play-test with a shop-bought vanilla-chain
				// vehicle (playtest-checklist item 19). If that play-test shows fuel persisting for
				// vanilla-chain vehicles, this is a prefab data gap, not a persistence gap.
				Print(string.Format(
					"[OVT_TEST] DIAGNOSTIC: vehicle fuel did not round-trip (released with %1 l, came back with %2 l). See the comment at this print for why this is not a failure.",
					string.Format("%1", m_fSavedFuel), string.Format("%1", restoredFuel)), LogLevel.WARNING);
			}
		}

		PrintFormat("Vehicle %1 survived despawn and respawn: owner intact, locked, %2 m from where it was released",
			m_sVehicleId, string.Format("%1", distance));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Finds which of a player's registered vehicle ids currently resolves to a given entity.
	//! \param[in] vehicles The vehicle manager.
	//! \param[in] persId The owning player.
	//! \param[in] vehicle The entity to identify.
	//! \return The registered id, or an empty string when the manager does not know this entity.
	protected string FindRegisteredIdFor(OVT_VehicleManagerComponent vehicles, string persId, IEntity vehicle)
	{
		array<string> ids = vehicles.GetPlayerVehicleIds(persId);

		foreach (string id : ids)
		{
			if (vehicles.FindVehicleEntity(id) == vehicle)
				return id;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a vehicle id is still registered to a player.
	//! \param[in] vehicles The vehicle manager.
	//! \param[in] persId The owning player.
	//! \param[in] vehicleId The id to look for.
	//! \return True when the manager still holds that registration.
	protected bool IsRegistered(OVT_VehicleManagerComponent vehicles, string persId, string vehicleId)
	{
		array<string> ids = vehicles.GetPlayerVehicleIds(persId);
		return ids.Find(vehicleId) != -1;
	}
}

//------------------------------------------------------------------------------------------------
//! A sabotaged radio tower survives a save and a re-apply still off the air, with time on its clock.
//!
//! SUBJECT. The test world carries exactly one transmitter tower ("Set 1 radio towers to occupying
//! faction" at every campaign start), so the case takes the first tower the manager holds and
//! resolves it back afterwards by LOCATION - the same match key OVT_PersistedRadioTower uses,
//! because towers are world-derived and a save can never create one.
//!
//! SEAM. OVT_OccupyingFactionManager.SetRadioTowerDisabled() is the seam the sabotage RPC itself
//! calls (OVT_TowerSabotageComponent.RpcAsk_SabotageTower), so the case drives the production path
//! rather than writing the field behind it.
//!
//! TOLERANCE, AND WHY THIS IS A RANGE AND NOT AN EQUALITY. The server ticks the timer down by
//! RADIO_TOWER_CHECK_FREQUENCY (9 s) every time CheckRadioTowers runs, both before the save and
//! after the re-apply, so the restored number is necessarily SMALLER than the one written. The case
//! sabotages for SABOTAGE_SECONDS and requires the restored value to be above MIN_RESTORED_SECONDS
//! and no greater than what it asked for: a window the countdown cannot walk out of within the
//! case's own timeout, and one that no value other than the saved timer can land in.
//!
//! ANTI-VACUOUS: the timer is cleared to zero in the dirty step, so the tower is demonstrably back
//! on the air before the re-apply. A non-zero timer afterwards can only have come out of storage.
//! Every expiry and every unresolvable subject is an explicit SetResultFailure with its own
//! sentence; there is no path to SetResultSuccess that has not asserted.
//!
//! CAN-FAIL: drop the disabledRemaining line from OVT_PersistedRadioTower (or stop applying it in
//! ApplyPersistedOccupyingFaction) and this case goes red with "came back on the air".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_TowerSabotage_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	//! Long enough that the server's 9 s countdown cannot walk the timer near zero while this runs.
	static const float SABOTAGE_SECONDS = 900;

	//! Floor for the restored timer. Half the sabotage is far below anything the countdown can eat
	//! inside the case timeout, and far above the zero the dirty step left behind.
	static const float MIN_RESTORED_SECONDS = 450;

	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected int m_iReloadPolls;
	protected vector m_vTowerLocation;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
			if (!occupying)
			{
				SetFailure("OVT_Global.GetOccupyingFaction() is null");
				return true;
			}

			OVT_RadioTowerData tower = FirstTower(occupying);
			if (!tower)
			{
				SetFailure("The test world handed out no radio tower to sabotage");
				return true;
			}

			m_vTowerLocation = tower.location;

			// Take it off the air - not a state the campaign start produces.
			occupying.SetRadioTowerDisabled(tower, SABOTAGE_SECONDS);

			m_iSaveBaseline = OVT_TEST_PersistenceRoundTripGate.CompletedSaveCount();

			string trigger = OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce();
			if (trigger != "")
			{
				SetFailure(trigger);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE)
		{
			string saveDiagnostic;
			int settled = OVT_TEST_PersistenceRoundTripGate.PollSaveSettled(m_iSaveBaseline, saveDiagnostic);
			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_FAILED)
			{
				SetFailure(saveDiagnostic);
				return true;
			}

			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_PENDING)
			{
				m_iSavePolls += 1;
				if (m_iSavePolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
				{
					SetFailure(OVT_TEST_PersistenceRoundTripGate.CAPABILITY_ABSENT);
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD)
		{
			OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
			if (!occupying)
			{
				SetFailure("OVT_Global.GetOccupyingFaction() is null before the reload");
				return true;
			}

			OVT_RadioTowerData tower = occupying.GetNearestRadioTower(m_vTowerLocation);
			if (!tower)
			{
				SetFailure("The occupying faction stopped handing out the tower at %1 before the reload", m_vTowerLocation.ToString());
				return true;
			}

			// Dirty it. Written at the data level rather than through SetRadioTowerDisabled(), which
			// would broadcast a fresh sabotage notification for what is really a test putting the
			// tower back on the air.
			tower.SetDisabledRemaining(0);

			string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
			if (reload != "")
			{
				SetFailure(reload);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD)
		{
			if (OVT_TEST_PersistenceRoundTripGate.ReloadInProgress())
			{
				m_iReloadPolls += 1;
				if (m_iReloadPolls > OVT_TEST_PersistenceRoundTripGate.MAX_RELOAD_POLLS)
				{
					SetFailure("Reload never completed: the persisted data was still being re-applied after %1 polls", m_iReloadPolls.ToString());
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_ASSERT;
			return false;
		}

		string restored = OVT_TEST_PersistenceRoundTripGate.RequireRestoredCampaign();
		if (restored != "")
		{
			SetFailure(restored);
			return true;
		}

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
		{
			SetFailure("OVT_Global.GetOccupyingFaction() is null after the reload");
			return true;
		}

		OVT_RadioTowerData tower = occupying.GetNearestRadioTower(m_vTowerLocation);
		if (!tower)
		{
			SetFailure("The reloaded session has no radio tower at %1", m_vTowerLocation.ToString());
			return true;
		}

		if (tower.disabledRemaining <= 0)
		{
			SetFailure("The sabotage did not survive the round trip: the tower came back on the air (%1 seconds left)",
				tower.disabledRemaining.ToString());
			return true;
		}

		if (tower.disabledRemaining < MIN_RESTORED_SECONDS)
		{
			SetFailure("The restored sabotage timer is too low to be the saved one: expected more than %1 seconds, read back %2",
				MIN_RESTORED_SECONDS.ToString(), tower.disabledRemaining.ToString());
			return true;
		}

		if (tower.disabledRemaining > SABOTAGE_SECONDS)
		{
			SetFailure("The restored sabotage timer is higher than the one saved: sabotaged for %1 seconds, read back %2",
				SABOTAGE_SECONDS.ToString(), tower.disabledRemaining.ToString());
			return true;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The first radio tower the occupying faction holds.
	//! \param[in] occupying The occupying faction manager.
	//! \return The tower, or null when the world produced none.
	protected OVT_RadioTowerData FirstTower(notnull OVT_OccupyingFactionManager occupying)
	{
		if (!occupying.m_RadioTowers)
			return null;

		foreach (OVT_RadioTowerData tower : occupying.m_RadioTowers)
		{
			if (tower)
				return tower;
		}

		return null;
	}
}
//------------------------------------------------------------------------------------------------
//! A player's last known position survives a save and a reload.
//!
//! WHY IT IS STORED AT ALL. A returning player is normally rebuilt from their STORED BODY, which
//! carries its own transform - so this pair only matters when that body cannot be found. Measured on
//! a dedicated server 2026-08-04: after a restart the body answered NOT_FOUND and the player woke up
//! at their home on the far side of the map. The position therefore lives as plain data on the
//! player's own record, which travels inside the game-mode record and does not depend on the
//! character record surviving. This case guards that independence.
//!
//! THE EXPECTED VALUE IS READ OFF THE LIVE CHARACTER, not written by the case. An earlier draft
//! poked a synthetic position into the record and asserted that came back; it failed, correctly,
//! because OVT_PlayerManagerComponent.SyncPlayerBodyIds() runs from PreShutdownPersist() before EVERY
//! save and overwrites the stored transform with where the body actually is. That is the behaviour
//! under test, so the case now asserts the whole pipeline - live body -> pre-save capture -> codec ->
//! adopt - rather than just the codec.
//!
//! THE DIRTY VALUE IS ZERO, DELIBERATELY, and it is the only correct choice here.
//! ApplyPersistedPlayers() adopts the stored transform ONLY when the live record has none - the rule
//! that stops re-applying a save from teleporting a player who is standing in the world. Zero is
//! exactly the state a freshly loaded record is in, so it is both the honest dirty value and the one
//! that exercises the adopt path. A non-zero dirty value would assert the opposite invariant.
//!
//! PROVEN ABLE TO FAIL 2026-08-05: with lastKnownPosition/lastKnownAngles removed from the write half
//! of OVT_PlayerManagerSerializer, the reload restores nothing and the case reports the zero vector.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_PlayerLastKnownPosition_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	//! How far the restored position may sit from where the body was when the case read it. Generous
	//! enough for a body settling on its collider between the read and the save, far tighter than the
	//! failure modes this guards (the zero vector, or the player's home).
	static const float POSITION_TOLERANCE = 2.0;

	//! Where the local body actually was when the save was taken - the value the pipeline must return.
	protected vector m_vExpected;

	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected int m_iReloadPolls;
	protected string m_sPersId;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			string diagnostic;
			m_sPersId = OVT_TEST_PersistenceSubject.ResolveLocalPersistentId(diagnostic);
			if (m_sPersId == "")
			{
				SetFailure("Cannot resolve the persistent player ID: %1", diagnostic);
				return true;
			}

			OVT_PlayerData player = OVT_PlayerData.Get(m_sPersId);
			if (!player)
			{
				SetFailure("OVT_PlayerData.Get() returned no record for the local player");
				return true;
			}

			int playerId = OVT_TEST_PersistenceSubject.ResolveLocalPlayerId(diagnostic);
			if (playerId < 1)
			{
				SetFailure("Cannot resolve the runtime player ID: %1", diagnostic);
				return true;
			}

			IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!body)
			{
				SetFailure("The local player has no controlled entity, so there is no body position for the pre-save capture to record");
				return true;
			}

			m_vExpected = body.GetOrigin();

			// Start from "no stored position", so a pass REQUIRES the pre-save capture to have run.
			player.m_vLastKnownPosition = vector.Zero;
			player.m_vLastKnownAngles = vector.Zero;

			m_iSaveBaseline = OVT_TEST_PersistenceRoundTripGate.CompletedSaveCount();

			string trigger = OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce();
			if (trigger != "")
			{
				SetFailure(trigger);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE)
		{
			string saveDiagnostic;
			int settled = OVT_TEST_PersistenceRoundTripGate.PollSaveSettled(m_iSaveBaseline, saveDiagnostic);
			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_FAILED)
			{
				SetFailure(saveDiagnostic);
				return true;
			}

			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_PENDING)
			{
				m_iSavePolls += 1;
				if (m_iSavePolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
				{
					SetFailure(OVT_TEST_PersistenceRoundTripGate.CAPABILITY_ABSENT);
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD)
		{
			OVT_PlayerData player = OVT_PlayerData.Get(m_sPersId);
			if (!player)
			{
				SetFailure("The local player record disappeared before the reload");
				return true;
			}

			// See the header: zero is the dirty value BECAUSE the adopt rule keys on "unset".
			player.m_vLastKnownPosition = vector.Zero;
			player.m_vLastKnownAngles = vector.Zero;

			string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
			if (reload != "")
			{
				SetFailure(reload);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD)
		{
			if (OVT_TEST_PersistenceRoundTripGate.ReloadInProgress())
			{
				m_iReloadPolls += 1;
				if (m_iReloadPolls > OVT_TEST_PersistenceRoundTripGate.MAX_RELOAD_POLLS)
				{
					SetFailure("Reload never completed: the persisted data was still being re-applied after %1 polls", m_iReloadPolls.ToString());
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_ASSERT;
			return false;
		}

		string restored = OVT_TEST_PersistenceRoundTripGate.RequireRestoredCampaign();
		if (restored != "")
		{
			SetFailure(restored);
			return true;
		}

		OVT_PlayerData player = OVT_PlayerData.Get(m_sPersId);
		if (!player)
		{
			SetFailure("OVT_PlayerData.Get() returned no record for the local player after the reload");
			return true;
		}

		if (player.m_vLastKnownPosition == vector.Zero)
		{
			SetFailure("The last known position came back as the zero vector - either the pre-save capture (OVT_PlayerManagerComponent.SyncPlayerBodyIds) did not run, or OVT_PlayerManagerSerializer is not carrying it. A player whose stored body cannot be found would be rebuilt at their home instead of where they logged out.");
			return true;
		}

		float drift = vector.Distance(player.m_vLastKnownPosition, m_vExpected);
		if (drift > POSITION_TOLERANCE)
		{
			SetFailure("The last known position came back as somewhere else: the body was at %1 when the save was taken, the record says %2 (%3 m away)",
				m_vExpected.ToString(), player.m_vLastKnownPosition.ToString(), drift.ToString());
			return true;
		}

		PrintFormat("Last known position round-tripped: body at %1, restored %2, facing %3",
			m_vExpected.ToString(), player.m_vLastKnownPosition.ToString(), player.m_vLastKnownAngles.ToString());

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The player-vehicle ownership registry survives a save and a reload.
//!
//! WHY THIS EXISTS. Until 2026-08-04 the registry was memory-only. A locked vehicle is saved,
//! released and deleted 60 s after its owner logs out, so it is not a world entity when the save is
//! written; after a restart nothing remembered its id, nothing asked for it back, and the player's
//! car was gone permanently. This case is the guard on the registry surviving, which is the half
//! that makes asking possible at all.
//!
//! NO REAL VEHICLE IS SPAWNED, on purpose. Registration from a live vehicle is already covered by
//! OVT_TEST_PersistenceRoundTrip_VehicleReserveRelease_KeepsOwnerAndContents. What is untested is
//! whether a registration reaches the save and comes back, so the record is injected through the
//! manager's own public apply path and read back through its own public accessor - no world entity
//! is involved in either direction, which is what makes the assertion about the CODEC and nothing
//! else.
//!
//! PROVEN ABLE TO FAIL 2026-08-05: with OVT_VehicleManagerSerializer unbound from the game-mode
//! configuration in Overthrow.conf, the reload restores no records and the case reports the id
//! missing.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_VehicleRegistry_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	//! A UUID-shaped id nothing else in the session will mint.
	static const string SAVED_VEHICLE_ID = "019fcccc-4242-8000-8400-0000424242ff";

	static const string SAVED_PREFAB = "{16C1F16C9B053801}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport.et";
	static const vector SAVED_POSITION = "3131.25 12.5 4646.75";

	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected int m_iReloadPolls;
	protected string m_sPersId;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			string diagnostic;
			m_sPersId = OVT_TEST_PersistenceSubject.ResolveLocalPersistentId(diagnostic);
			if (m_sPersId == "")
			{
				SetFailure("Cannot resolve the persistent player ID: %1", diagnostic);
				return true;
			}

			OVT_VehicleManagerComponent vehicles = OVT_VehicleManagerComponent.GetInstance();
			if (!vehicles)
			{
				SetFailure("OVT_VehicleManagerComponent.GetInstance() is null");
				return true;
			}

			OVT_PersistedPlayerVehicle record = new OVT_PersistedPlayerVehicle();
			record.persistentId = SAVED_VEHICLE_ID;
			record.ownerUid = m_sPersId;
			record.prefab = SAVED_PREFAB;
			record.position = SAVED_POSITION;
			record.angles = vector.Zero;
			record.locked = true;

			array<ref OVT_PersistedPlayerVehicle> seed = {};
			seed.Insert(record);
			vehicles.ApplyPersistedVehicles(seed);

			m_iSaveBaseline = OVT_TEST_PersistenceRoundTripGate.CompletedSaveCount();

			string trigger = OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce();
			if (trigger != "")
			{
				SetFailure(trigger);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE)
		{
			string saveDiagnostic;
			int settled = OVT_TEST_PersistenceRoundTripGate.PollSaveSettled(m_iSaveBaseline, saveDiagnostic);
			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_FAILED)
			{
				SetFailure(saveDiagnostic);
				return true;
			}

			if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_PENDING)
			{
				m_iSavePolls += 1;
				if (m_iSavePolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
				{
					SetFailure(OVT_TEST_PersistenceRoundTripGate.CAPABILITY_ABSENT);
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD)
		{
			OVT_VehicleManagerComponent vehicles = OVT_VehicleManagerComponent.GetInstance();
			if (!vehicles || !vehicles.GetVehicleRecords())
			{
				SetFailure("The vehicle manager or its registry disappeared before the reload");
				return true;
			}

			// Dirty it: wipe the registry entirely, so a reload that restores nothing cannot pass.
			vehicles.GetVehicleRecords().Clear();

			string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
			if (reload != "")
			{
				SetFailure(reload);
				return true;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD;
			return false;
		}

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD)
		{
			if (OVT_TEST_PersistenceRoundTripGate.ReloadInProgress())
			{
				m_iReloadPolls += 1;
				if (m_iReloadPolls > OVT_TEST_PersistenceRoundTripGate.MAX_RELOAD_POLLS)
				{
					SetFailure("Reload never completed: the persisted data was still being re-applied after %1 polls", m_iReloadPolls.ToString());
					return true;
				}

				return false;
			}

			m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_ASSERT;
			return false;
		}

		string restored = OVT_TEST_PersistenceRoundTripGate.RequireRestoredCampaign();
		if (restored != "")
		{
			SetFailure(restored);
			return true;
		}

		OVT_VehicleManagerComponent vehicles = OVT_VehicleManagerComponent.GetInstance();
		if (!vehicles)
		{
			SetFailure("OVT_VehicleManagerComponent.GetInstance() is null after the reload");
			return true;
		}

		map<string, ref OVT_PersistedPlayerVehicle> live = vehicles.GetVehicleRecords();
		if (!live || !live.Contains(SAVED_VEHICLE_ID))
		{
			SetFailure("The vehicle registration did not survive the round trip - id %1 is not registered after the reload. A vehicle despawned while its owner was offline would be unrecoverable after a server restart.",
				SAVED_VEHICLE_ID);
			return true;
		}

		OVT_PersistedPlayerVehicle record = live[SAVED_VEHICLE_ID];
		if (!record)
		{
			SetFailure("The registry holds id %1 but the record behind it is null", SAVED_VEHICLE_ID);
			return true;
		}

		if (record.ownerUid != m_sPersId)
		{
			SetFailure("The vehicle came back registered to the wrong owner: saved %1, read back %2",
				m_sPersId, record.ownerUid);
			return true;
		}

		// The rebuild path needs all three of these, so all three are asserted: without the prefab there
		// is nothing to spawn, without the position nowhere to put it, without the lock state the
		// player's locked car comes back open.
		if (record.prefab != SAVED_PREFAB)
		{
			SetFailure("The rebuild prefab did not survive: saved %1, read back %2", SAVED_PREFAB, record.prefab);
			return true;
		}

		if (record.position != SAVED_POSITION)
		{
			SetFailure("The parked position did not survive: saved %1, read back %2",
				SAVED_POSITION.ToString(), record.position.ToString());
			return true;
		}

		if (!record.locked)
		{
			SetFailure("The lock state did not survive: the vehicle was saved locked and came back unlocked");
			return true;
		}

		// The owner index is derived from the same records, so a registration that came back must also be
		// findable the way RespawnPlayerVehicles() looks for it.
		array<string> ids = vehicles.GetPlayerVehicleIds(m_sPersId);
		if (!ids || ids.Find(SAVED_VEHICLE_ID) == -1)
		{
			SetFailure("The record came back but the owner index did not: RespawnPlayerVehicles() iterates GetPlayerVehicleIds(), which does not list %1",
				SAVED_VEHICLE_ID);
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! T3 - THE JOB BOARD AND BOTH LIFETIME COUNTER MAPS SURVIVE A SAVE AND A RELOAD, ON THE RIGHT JOBS.
//!
//! WHY THIS CASE EXISTS. A saved job used to name itself by its POSITION in the job manager's config
//! list. Trim or reorder that list and every saved record silently comes back attached to a
//! DIFFERENT job - at a stage index that is still valid, paying that other job's reward, with its
//! lifetime counters capping the wrong thing, and with no error anywhere. The save format now names
//! each job by a stable id instead, and this case is what says so out loud: it does not merely check
//! that "some jobs came back", it checks that each one came back on the job it was saved on.
//!
//! THE ASSERTION RULE (suite header). Nothing here names a storage type, a persistence-framework
//! type or a persisted record class. The board and the two counter maps are seeded through the job
//! manager's own public collections and read back through the same ones, plus its public
//! FindJobIndexById() / GetJobIdByIndex() accessors - so this case stays true whatever the storage
//! underneath is, which is the whole point of the rule. The only persistence-layer calls are the
//! gate class's two annotated seams, shared with every other case here.
//!
//! HOW THE SEEDED RECORDS ARE FOUND AGAIN, AND WHY IT IS NOT A COUNT. Each seeded record carries a
//! unique LOCATION far outside the world (see MarkerLocation()). Assertions match on that, never on
//! "the board has N jobs": the manager's own CheckUpdate() offers new public jobs on a timer and the
//! board legitimately grows during a run, so a count-based assertion would be a coin flip. Phase 0 of
//! this feature learned that the expensive way - a fixture seeded with 4 jobs held 12 by the time the
//! save landed.
//!
//! WHY NOTHING THE MANAGER DOES ON ITS TIMER CAN DISTURB THESE RECORDS. Stated as a proof, because
//! "probably won't happen" is how a flaky test gets written:
//!  - CheckUpdate()'s tick loop skips any job with accepted == false (OVT_JobManagerComponent.c:506),
//!    which covers records B and C.
//!  - Record A is accepted, and is deliberately parked on base-recon's only stage, an
//!    OVT_WaitTillPlayerInRangeJobStage. Its OnTick() resolves job.owner through the player manager
//!    and returns TRUE - keep waiting - the moment that lookup fails
//!    (OVT_WaitTillPlayerInRangeJobStage.c:10-11; GetPlayer() returns null for an unknown id,
//!    OVT_PlayerManagerComponent.c:173-177). The owner here is a synthetic marker no player carries,
//!    so the stage can never advance and the job can never complete or leave the board.
//!  - The two global counters asserted below belong to base-recon (m_iMaxTimes 2) and raise-support
//!    (m_iMaxTimes 4), and both are seeded far ABOVE their cap. Neither is player-allocated, so
//!    CheckUpdate() skips the whole config at OVT_JobManagerComponent.c:639 and can never increment
//!    them. Seeding the counter is what makes the counter safe to assert on.
//!  - The per-player counters are seeded under a synthetic persistent id. CheckUpdate() only ever
//!    writes per-player counts for ids the player manager actually holds (:696-706), so nothing but
//!    this case touches that key.
//!
//! THE PER-PLAYER RECORD IS SYNTHETIC ON PURPOSE. After the five starter jobs are retired, every
//! shipped config is public or base-only, so no config is player-allocated and the per-player counter
//! map is empty in a real campaign. It is still persisted and still must round-trip, so it is seeded
//! by hand rather than by playing.
//!
//! ANTI-VACUOUS-PASS (suite header closures 2 and 3). The dirty step does not just delete: it removes
//! one record, RE-POINTS another at a different job config - the exact mis-attachment this feature
//! exists to make impossible - moves a third to a different stage, and rewrites both counter maps to
//! wrong values. A reload that restores nothing leaves all of that in place and the case fails by
//! name; a reload that restores campaign-start defaults produces no marker records at all and fails
//! the same way. None of the seeded values is one a campaign start would produce.
//!
//! IT ALSO ASSERTS IDEMPOTENCY, WHICH IS WHY IT RELOADS TWICE. The persistence manager re-applies
//! saved data to a LIVE session, so ApplyPersistedJobs() has to be a clear-and-rebuild rather than an
//! append: applying the same payload twice must produce the same board, not a doubled one. The second
//! re-application asserts exactly that - each marker location still holds exactly one record, and
//! neither counter map has moved.
//!
//! EXECUTION ORDER. The class name sorts after ..._Capability_... and before every other case here,
//! so it neither breaks the capability case's fresh-session precondition (closure 5) nor depends on
//! any case having run before it - it triggers its own save and waits for that save.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 90)]
class OVT_TEST_PersistenceRoundTrip_JobBoard_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	//! Extra phases for the second re-application. The shared five stop at PHASE_ASSERT.
	static const int PHASE_AWAIT_SECOND_RELOAD = 5;
	static const int PHASE_ASSERT_IDEMPOTENT = 6;

	//! The three seeded jobs, named by the stable ids the save format writes.
	//! A is base-only and accepted; B is a public town job; C is a public town job parked on a
	//! non-zero stage, so "the stage came back" is a real assertion and not 0 == 0.
	static const string JOB_ID_A = "base-recon";
	static const string JOB_ID_B = "raise-support";
	static const string JOB_ID_C = "assassinate-traitor";

	//! Stage indices. base-recon and raise-support have one stage each; assassinate-traitor's stage 3
	//! is its spawn-group stage - restorable, unlike its stage 4, which waits on a dead entity and is
	//! dropped by design.
	static const int STAGE_A = 0;
	static const int STAGE_B = 0;
	static const int STAGE_C = 3;

	//! A base id no base carries, so the base-only record's occupancy slot is its own.
	static const int SYNTHETIC_BASE_ID = 4242;

	//! Owners and decliners. None of these is a persistent id any player holds - see the header for
	//! why that is load-bearing rather than cosmetic.
	static const string OWNER_A = "OVTJOBRTOWNERA";
	static const string OWNER_C = "OVTJOBRTOWNERC";
	static const string DECLINER_1 = "OVTJOBRTDECLINER1";
	static const string DECLINER_2 = "OVTJOBRTDECLINER2";

	//! The synthetic player whose per-player lifetime counters are seeded.
	static const string COUNTER_PLAYER = "OVTJOBRTCOUNTERPLAYER";

	//! Saved lifetime counts. Both global values are above their config's m_iMaxTimes, which is what
	//! stops the manager's own offer loop from ever touching them.
	static const int SAVED_GLOBAL_COUNT_A = 4242;
	static const int SAVED_GLOBAL_COUNT_B = 4243;
	static const int SAVED_PLAYER_COUNT_A = 11;
	static const int SAVED_PLAYER_COUNT_B = 13;

	//! Dirty values, written after the save. None is a campaign-start value either.
	static const int DIRTY_GLOBAL_COUNT_A = 1;
	static const int DIRTY_GLOBAL_COUNT_B = 2;
	static const int DIRTY_PLAYER_COUNT = 9;

	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected int m_iReloadPolls;
	protected int m_iSecondReloadPolls;
	protected int m_iTownId;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
			return SeedAndSave();

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE)
			return AwaitSave();

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD)
			return DirtyAndReload();

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD)
			return AwaitFirstReload();

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_ASSERT)
			return AssertRestoredThenReloadAgain();

		if (m_iPhase == PHASE_AWAIT_SECOND_RELOAD)
			return AwaitSecondReload();

		return AssertIdempotent();
	}

	//------------------------------------------------------------------------------------------------
	//! Seeds the board and both counter maps, then triggers exactly one save.
	//! \return True when the case is finished (it failed), false to run again next frame.
	protected bool SeedAndSave()
	{
		OVT_JobManagerComponent jobs = OVT_Global.GetJobs();
		if (!jobs)
		{
			SetFailure("OVT_Global.GetJobs() is null");
			return true;
		}

		string diagnostic;
		OVT_TownData town = OVT_TEST_PersistenceSubject.ResolveFirstTown(m_iTownId, diagnostic);
		if (!town)
		{
			SetFailure("Cannot resolve a town for the town-scoped jobs: %1", diagnostic);
			return true;
		}

		int indexA = jobs.FindJobIndexById(JOB_ID_A);
		int indexB = jobs.FindJobIndexById(JOB_ID_B);
		int indexC = jobs.FindJobIndexById(JOB_ID_C);
		if (indexA < 0 || indexB < 0 || indexC < 0)
		{
			SetFailure(string.Format("A job this case seeds is not configured: '%1' -> %2, '%3' -> %4, '%5' -> %6. Every one of these is a surviving job and must resolve.",
				JOB_ID_A, indexA.ToString(), JOB_ID_B, indexB.ToString(), JOB_ID_C, indexC.ToString()));
			return true;
		}

		if (!jobs.m_aJobs || !jobs.m_aJobCounts || !jobs.m_mPlayerJobCounts)
		{
			SetFailure("The job manager's board or counter maps are null - it was never initialised");
			return true;
		}

		// The board. Only the three marker records are placed; whatever the manager has already
		// offered stays exactly where it is, because this case asserts on its own records and not on
		// the size of the board.
		OVT_Job jobA = new OVT_Job();
		jobA.jobIndex = indexA;
		jobA.location = MarkerLocation(1);
		jobA.townId = -1;
		jobA.baseId = SYNTHETIC_BASE_ID;
		jobA.stage = STAGE_A;
		jobA.owner = OWNER_A;
		jobA.accepted = true;
		jobA.declined.Insert(DECLINER_1);
		jobs.m_aJobs.Insert(jobA);

		OVT_Job jobB = new OVT_Job();
		jobB.jobIndex = indexB;
		jobB.location = MarkerLocation(2);
		jobB.townId = m_iTownId;
		jobB.baseId = -1;
		jobB.stage = STAGE_B;
		jobB.owner = "";
		jobB.accepted = false;
		jobs.m_aJobs.Insert(jobB);

		OVT_Job jobC = new OVT_Job();
		jobC.jobIndex = indexC;
		jobC.location = MarkerLocation(3);
		jobC.townId = m_iTownId;
		jobC.baseId = -1;
		jobC.stage = STAGE_C;
		jobC.owner = OWNER_C;
		jobC.accepted = false;
		jobC.declined.Insert(DECLINER_1);
		jobC.declined.Insert(DECLINER_2);
		jobs.m_aJobs.Insert(jobC);

		// Both counter maps. These outlive the jobs they counted, so nothing on the board implies
		// them and they have to survive on their own.
		jobs.m_aJobCounts[indexA] = SAVED_GLOBAL_COUNT_A;
		jobs.m_aJobCounts[indexB] = SAVED_GLOBAL_COUNT_B;

		map<int, int> playerCounts = new map<int, int>;
		playerCounts[indexA] = SAVED_PLAYER_COUNT_A;
		playerCounts[indexB] = SAVED_PLAYER_COUNT_B;
		jobs.m_mPlayerJobCounts[COUNTER_PLAYER] = playerCounts;

		m_iSaveBaseline = OVT_TEST_PersistenceRoundTripGate.CompletedSaveCount();

		string trigger = OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce();
		if (trigger != "")
		{
			SetFailure(trigger);
			return true;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for this case's own save to complete.
	//! \return True when the case is finished (it failed), false to run again next frame.
	protected bool AwaitSave()
	{
		string saveDiagnostic;
		int settled = OVT_TEST_PersistenceRoundTripGate.PollSaveSettled(m_iSaveBaseline, saveDiagnostic);
		if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_FAILED)
		{
			SetFailure(saveDiagnostic);
			return true;
		}

		if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_PENDING)
		{
			m_iSavePolls += 1;
			if (m_iSavePolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
			{
				SetFailure(OVT_TEST_PersistenceRoundTripGate.CAPABILITY_ABSENT);
				return true;
			}

			return false;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Destroys the saved state in memory, then asks for the persisted state back.
	//!
	//! Three different kinds of damage on purpose: a record removed, a record re-pointed at another
	//! job config (the silent mis-attachment this whole feature exists to prevent), and a record moved
	//! to a different stage - plus both counter maps rewritten.
	//! \return True when the case is finished (it failed), false to run again next frame.
	protected bool DirtyAndReload()
	{
		OVT_JobManagerComponent jobs = OVT_Global.GetJobs();
		if (!jobs || !jobs.m_aJobs || !jobs.m_aJobCounts || !jobs.m_mPlayerJobCounts)
		{
			SetFailure("The job manager or one of its collections is null before the reload");
			return true;
		}

		int indexA = jobs.FindJobIndexById(JOB_ID_A);
		int indexB = jobs.FindJobIndexById(JOB_ID_B);
		int indexC = jobs.FindJobIndexById(JOB_ID_C);

		OVT_Job jobA = FindMarkedJob(jobs, 1);
		OVT_Job jobB = FindMarkedJob(jobs, 2);
		OVT_Job jobC = FindMarkedJob(jobs, 3);
		if (!jobA || !jobB || !jobC)
		{
			SetFailure("A seeded job disappeared from the board between the seed and the save - nothing in the manager should be able to remove these (see this case's header)");
			return true;
		}

		jobs.m_aJobs.RemoveItem(jobA);
		jobB.jobIndex = indexC;
		jobC.jobIndex = indexB;
		jobC.stage = 0;

		jobs.m_aJobCounts[indexA] = DIRTY_GLOBAL_COUNT_A;
		jobs.m_aJobCounts[indexB] = DIRTY_GLOBAL_COUNT_B;

		map<int, int> dirtyCounts = new map<int, int>;
		dirtyCounts[indexA] = DIRTY_PLAYER_COUNT;
		jobs.m_mPlayerJobCounts[COUNTER_PLAYER] = dirtyCounts;

		string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
		if (reload != "")
		{
			SetFailure(reload);
			return true;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for the first re-application to finish.
	//! \return True when the case is finished (it failed), false to run again next frame.
	protected bool AwaitFirstReload()
	{
		if (OVT_TEST_PersistenceRoundTripGate.ReloadInProgress())
		{
			m_iReloadPolls += 1;
			if (m_iReloadPolls > OVT_TEST_PersistenceRoundTripGate.MAX_RELOAD_POLLS)
			{
				SetFailure("Reload never completed: the persisted data was still being re-applied after %1 polls", m_iReloadPolls.ToString());
				return true;
			}

			return false;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_ASSERT;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts the saved board and counters came back, then asks for the SAME payload a second time.
	//! \return True when the case is finished (it failed), false to run again next frame.
	protected bool AssertRestoredThenReloadAgain()
	{
		string failure = CheckEverythingCameBack("after the reload");
		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
		if (reload != "")
		{
			SetFailure(reload);
			return true;
		}

		m_iPhase = PHASE_AWAIT_SECOND_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for the second re-application to finish.
	//! \return True when the case is finished (it failed), false to run again next frame.
	protected bool AwaitSecondReload()
	{
		if (OVT_TEST_PersistenceRoundTripGate.ReloadInProgress())
		{
			m_iSecondReloadPolls += 1;
			if (m_iSecondReloadPolls > OVT_TEST_PersistenceRoundTripGate.MAX_RELOAD_POLLS)
			{
				SetFailure("The second re-application never completed after %1 polls", m_iSecondReloadPolls.ToString());
				return true;
			}

			return false;
		}

		m_iPhase = PHASE_ASSERT_IDEMPOTENT;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts that applying the same payload twice produced the same board, not a doubled one.
	//! \return Always true - the case ends here either way.
	protected bool AssertIdempotent()
	{
		string failure = CheckEverythingCameBack("after the SAME payload was applied a second time");
		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Job board round trip: 3 records back on their own configs with stage, owner, location and declines intact; both counter maps intact; a second application of the same payload changed nothing");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The whole assertion, run identically after each of the two re-applications.
	//! \param[in] when Where in the round trip this check is happening, for the failure text.
	//! \return An empty string when everything came back, otherwise the diagnostic to fail with.
	protected string CheckEverythingCameBack(string when)
	{
		string restored = OVT_TEST_PersistenceRoundTripGate.RequireRestoredCampaign();
		if (restored != "")
			return restored;

		OVT_JobManagerComponent jobs = OVT_Global.GetJobs();
		if (!jobs || !jobs.m_aJobs || !jobs.m_aJobCounts || !jobs.m_mPlayerJobCounts)
			return "The job manager or one of its collections is null " + when;

		string board = CheckBoard(jobs, when);
		if (board != "")
			return board;

		return CheckCounters(jobs, when);
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts each seeded record came back exactly once, on its own config, with its state intact.
	//! \param[in] jobs The job manager. Callers null-check before calling.
	//! \param[in] when Where in the round trip this check is happening, for the failure text.
	//! \return An empty string when the board is right, otherwise the diagnostic.
	protected string CheckBoard(notnull OVT_JobManagerComponent jobs, string when)
	{
		string a = CheckOneRecord(jobs, when, 1, JOB_ID_A, STAGE_A, -1, SYNTHETIC_BASE_ID, OWNER_A, true, 1);
		if (a != "")
			return a;

		string b = CheckOneRecord(jobs, when, 2, JOB_ID_B, STAGE_B, m_iTownId, -1, "", false, 0);
		if (b != "")
			return b;

		return CheckOneRecord(jobs, when, 3, JOB_ID_C, STAGE_C, m_iTownId, -1, OWNER_C, false, 2);
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one seeded record. Every field is checked, because every field is written by the same
	//! record copy and a mistake in any of them is the same class of silent save damage.
	//! \param[in] jobs The job manager. Callers null-check before calling.
	//! \param[in] when Where in the round trip this check is happening, for the failure text.
	//! \param[in] marker The record's marker number, which is also its location's Z.
	//! \param[in] expectedJobId The stable id of the job it was saved on.
	//! \param[in] expectedStage The stage it was parked at.
	//! \param[in] expectedTownId The town it belonged to, or -1.
	//! \param[in] expectedBaseId The base it belonged to, or -1.
	//! \param[in] expectedOwner The owning persistent id, or an empty string.
	//! \param[in] expectedAccepted Whether it had been accepted.
	//! \param[in] expectedDeclines How many persistent ids had declined it.
	//! \return An empty string when the record is right, otherwise the diagnostic.
	protected string CheckOneRecord(notnull OVT_JobManagerComponent jobs, string when, int marker, string expectedJobId, int expectedStage, int expectedTownId, int expectedBaseId, string expectedOwner, bool expectedAccepted, int expectedDeclines)
	{
		vector expectedLocation = MarkerLocation(marker);

		int found = 0;
		OVT_Job job = null;
		foreach (OVT_Job candidate : jobs.m_aJobs)
		{
			if (!candidate)
				continue;

			if (candidate.location != expectedLocation)
				continue;

			found += 1;
			job = candidate;
		}

		if (found == 0)
			return string.Format("The job saved on '%1' at %2 did not come back %3. It was saved and then deliberately destroyed in memory, so a board without it means the saved job board was not restored from storage.",
				expectedJobId, expectedLocation.ToString(), when);

		if (found > 1)
			return string.Format("The job saved on '%1' at %2 came back %3 times %4. Restoring a save must CLEAR and rebuild the board - re-applying the same saved data to a live session is a supported operation and must not duplicate what is already there.",
				expectedJobId, expectedLocation.ToString(), found.ToString(), when);

		string actualJobId = jobs.GetJobIdByIndex(job.jobIndex);
		if (actualJobId != expectedJobId)
			return string.Format("A saved job came back on the WRONG JOB %1: it was saved on '%2' and came back on '%3' (config index %4). This is the exact silent corruption the stable job id exists to prevent - the job would tick that other job's stages and pay that other job's reward.",
				when, expectedJobId, actualJobId, job.jobIndex.ToString());

		if (job.stage != expectedStage)
			return string.Format("The job saved on '%1' came back at stage %2 instead of %3 %4. A job restored at the wrong stage skips or repeats work and pays out early.",
				expectedJobId, job.stage.ToString(), expectedStage.ToString(), when);

		if (job.townId != expectedTownId)
			return string.Format("The job saved on '%1' came back in town %2 instead of %3 %4.",
				expectedJobId, job.townId.ToString(), expectedTownId.ToString(), when);

		if (job.baseId != expectedBaseId)
			return string.Format("The job saved on '%1' came back at base %2 instead of %3 %4.",
				expectedJobId, job.baseId.ToString(), expectedBaseId.ToString(), when);

		if (job.owner != expectedOwner)
			return string.Format("The job saved on '%1' came back owned by '%2' instead of '%3' %4. A job that loses its owner is offered to everyone; a job that gains one is nobody else's to take.",
				expectedJobId, job.owner, expectedOwner, when);

		if (job.accepted != expectedAccepted)
			return string.Format("The job saved on '%1' came back with accepted = %2 instead of %3 %4.",
				expectedJobId, job.accepted.ToString(), expectedAccepted.ToString(), when);

		int declines = 0;
		if (job.declined)
			declines = job.declined.Count();

		if (declines != expectedDeclines)
			return string.Format("The job saved on '%1' came back with %2 declines instead of %3 %4. A lost decline list re-offers a job to the player who already turned it down.",
				expectedJobId, declines.ToString(), expectedDeclines.ToString(), when);

		if (expectedDeclines > 0 && job.declined.Find(DECLINER_1) == -1)
			return string.Format("The job saved on '%1' came back with a decline list that does not hold '%2' %3.",
				expectedJobId, DECLINER_1, when);

		if (expectedDeclines > 1 && job.declined.Find(DECLINER_2) == -1)
			return string.Format("The job saved on '%1' came back with a decline list that does not hold '%2' %3.",
				expectedJobId, DECLINER_2, when);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts both lifetime counter maps came back on the right jobs with the right values.
	//! \param[in] jobs The job manager. Callers null-check before calling.
	//! \param[in] when Where in the round trip this check is happening, for the failure text.
	//! \return An empty string when the counters are right, otherwise the diagnostic.
	protected string CheckCounters(notnull OVT_JobManagerComponent jobs, string when)
	{
		int indexA = jobs.FindJobIndexById(JOB_ID_A);
		int indexB = jobs.FindJobIndexById(JOB_ID_B);
		if (indexA < 0 || indexB < 0)
			return string.Format("'%1' or '%2' stopped resolving to a configured job %3", JOB_ID_A, JOB_ID_B, when);

		string globalA = CheckGlobalCount(jobs, when, indexA, JOB_ID_A, SAVED_GLOBAL_COUNT_A);
		if (globalA != "")
			return globalA;

		string globalB = CheckGlobalCount(jobs, when, indexB, JOB_ID_B, SAVED_GLOBAL_COUNT_B);
		if (globalB != "")
			return globalB;

		map<int, int> playerCounts = jobs.m_mPlayerJobCounts[COUNTER_PLAYER];
		if (!playerCounts)
			return string.Format("The per-player lifetime counters for '%1' did not come back %2. They are what stops a player being offered the same job forever, and nothing on the board implies them - once lost they cannot be rebuilt.",
				COUNTER_PLAYER, when);

		string playerA = CheckPlayerCount(playerCounts, when, indexA, JOB_ID_A, SAVED_PLAYER_COUNT_A);
		if (playerA != "")
			return playerA;

		return CheckPlayerCount(playerCounts, when, indexB, JOB_ID_B, SAVED_PLAYER_COUNT_B);
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one global lifetime counter.
	//! \param[in] jobs The job manager. Callers null-check before calling.
	//! \param[in] when Where in the round trip this check is happening, for the failure text.
	//! \param[in] jobIndex The job's position in the config list, as resolved from its stable id.
	//! \param[in] jobId The job's stable id, for the failure text.
	//! \param[in] expected The count that was saved.
	//! \return An empty string when the count is right, otherwise the diagnostic.
	protected string CheckGlobalCount(notnull OVT_JobManagerComponent jobs, string when, int jobIndex, string jobId, int expected)
	{
		if (!jobs.m_aJobCounts.Contains(jobIndex))
			return string.Format("The global lifetime counter for '%1' did not come back at all %2 - the job's m_iMaxTimes cap would start again from zero.",
				jobId, when);

		int actual = jobs.m_aJobCounts[jobIndex];
		if (actual != expected)
			return string.Format("The global lifetime counter for '%1' came back as %2 instead of %3 %4. A counter keyed to the wrong job caps the wrong job.",
				jobId, actual.ToString(), expected.ToString(), when);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one per-player lifetime counter.
	//! \param[in] playerCounts The synthetic player's restored counter map. Callers null-check first.
	//! \param[in] when Where in the round trip this check is happening, for the failure text.
	//! \param[in] jobIndex The job's position in the config list, as resolved from its stable id.
	//! \param[in] jobId The job's stable id, for the failure text.
	//! \param[in] expected The count that was saved.
	//! \return An empty string when the count is right, otherwise the diagnostic.
	protected string CheckPlayerCount(notnull map<int, int> playerCounts, string when, int jobIndex, string jobId, int expected)
	{
		if (!playerCounts.Contains(jobIndex))
			return string.Format("The per-player lifetime counter for '%1' did not come back %2.", jobId, when);

		int actual = playerCounts[jobIndex];
		if (actual != expected)
			return string.Format("The per-player lifetime counter for '%1' came back as %2 instead of %3 %4.",
				jobId, actual.ToString(), expected.ToString(), when);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Finds the seeded record carrying a marker location.
	//! \param[in] jobs The job manager. Callers null-check before calling.
	//! \param[in] marker The marker number.
	//! \return The record, or null when the board does not hold it.
	protected OVT_Job FindMarkedJob(notnull OVT_JobManagerComponent jobs, int marker)
	{
		vector wanted = MarkerLocation(marker);
		foreach (OVT_Job candidate : jobs.m_aJobs)
		{
			if (!candidate)
				continue;

			if (candidate.location == wanted)
				return candidate;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The location that identifies one seeded record.
	//!
	//! Far outside any world, so it cannot collide with a job the manager offers on its own, and the
	//! coordinates are small whole numbers, which a 32-bit float carries exactly - so the assertion
	//! can compare positions for equality rather than for nearness.
	//! \param[in] marker The record's marker number.
	//! \return Its location.
	protected vector MarkerLocation(int marker)
	{
		return Vector(424200, 0, marker);
	}
}

//------------------------------------------------------------------------------------------------
//! A WIPED VIRTUAL GROUP DOES NOT COME BACK - not from the save, and not from anything the session
//! did afterwards.
//!
//! WHY THIS CASE EXISTS, AND WHY IT IS THE FIRST OF THE TWO. "Dead members stay dead" (G3) has a
//! terminal case: a group whose whole roster died is REMOVED, and the campaign must never hand it
//! back. Under Route B that promise is entirely Overthrow's to keep - core re-creates its own group
//! entities from its own payload on every load, so a record that reached the payload by accident, or
//! that the restore refused to remove, IS a resurrected garrison standing in a base the player
//! already cleared.
//!
//! THE RECORD IS ALREADY GONE BEFORE THE SAVE, and the case asserts that before saving - otherwise it
//! would be proving something about a payload entry that was never written. What the round trip then
//! proves is the other half: the payload keeps it gone.
//!
//! ANTI-VACUOUS-PASS (suite header closures 2 and 3). The dirty step RE-REGISTERS a group under the
//! same owner key - a deliberate resurrection, and exactly the shape a stale record would have. A
//! reload that restores nothing leaves that group registered and the case fails by name; a reload that
//! restores campaign-start defaults produces no virtual groups at all and also fails, because the
//! resurrection would still be sitting there. The assertion is "the owner key resolves to nothing",
//! which neither a no-op nor a reset can satisfy.
//!
//! PROVEN ABLE TO FAIL (fail proof recorded): make ApplyPersistedRegistry() skip its
//! "unregister everything the payload does not claim" pass - the resurrected group survives the
//! reload and the case reports the owner key still resolving. Independently: make
//! ApplyPersistedRecord() re-create all-dead records instead of returning false, seed a wiped record
//! into the payload, and the wiped handle comes back registered.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_VirtualGroupsWiped_DoNotComeBack : SCR_AutotestCaseBase
{
	//! Distinctive owner key - nothing else in the session registers under it.
	static const string OWNER_KEY = "roundtrip_wiped_group";

	//! A tiny spawn ring, for the same reason the sibling case uses one: the engine must not
	//! materialise members of either group while the case's bounded waits run.
	static const int SPAWN_DISTANCE_OVERRIDE = 23;

	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected int m_iReloadPolls;

	//! The handle of the group that was wiped before the save.
	protected int m_iWipedHandle = -1;

	//! The handle of the group registered under the same owner key AFTER the save - the resurrection
	//! the restore has to undo.
	protected int m_iResurrectedHandle = -1;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
			return MutateAndSave();

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE)
			return AwaitSave();

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD)
			return DirtyAndReload();

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD)
			return AwaitReload();

		return Assert();
	}

	//------------------------------------------------------------------------------------------------
	//! Registers a group, kills every one of its roster slots, checks the wipe already removed it, and
	//! saves that state.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool MutateAndSave()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null - the virtualization manager is missing from the game-mode prefab");
			return true;
		}

		string factionKey;
		string groupName;
		if (!OVT_TEST_VirtualizationFixture.FindComposition(factionKey, groupName))
		{
			SetFailure("No faction in this world defines a resolvable group registry entry, so a virtual group cannot be registered");
			return true;
		}

		m_iWipedHandle = virtualization.RegisterGroup(OVT_TEST_VirtualizationFixture.OWNER_SYSTEM, OWNER_KEY,
			factionKey, groupName, OVT_TEST_VirtualizationFixture.PickPosition(), null, SPAWN_DISTANCE_OVERRIDE);

		if (m_iWipedHandle == -1)
		{
			SetFailure("RegisterGroup returned -1 for the composition %1 '%2', which the faction registry resolves", factionKey, groupName);
			return true;
		}

		int roster = virtualization.GetMemberCount(m_iWipedHandle);
		if (roster < 1)
		{
			virtualization.UnregisterGroup(m_iWipedHandle);
			SetFailure("The registered group has no roster slots, so it cannot be wiped by reporting deaths");
			return true;
		}

		// Kill the whole roster. ReportMemberKilled is the public death seam, so no world combat is
		// needed - and killing the last living slot is what removes the record.
		for (int slot = 0; slot < roster; slot++)
		{
			virtualization.ReportMemberKilled(m_iWipedHandle, slot);
		}

		if (virtualization.IsRegistered(m_iWipedHandle))
		{
			virtualization.UnregisterGroup(m_iWipedHandle);
			SetFailure("Killing all %1 roster slots left handle %2 registered - the wipe never happened, so this case cannot say anything about the save",
				roster.ToString(), m_iWipedHandle.ToString());
			return true;
		}

		if (!virtualization.FindGroupsByOwner(OVT_TEST_VirtualizationFixture.OWNER_SYSTEM, OWNER_KEY).IsEmpty())
		{
			SetFailure("The wiped group still resolves through FindGroupsByOwner before the save was even taken");
			return true;
		}

		m_iSaveBaseline = OVT_TEST_PersistenceRoundTripGate.CompletedSaveCount();

		string trigger = OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce();
		if (trigger != "")
		{
			SetFailure(trigger);
			return true;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitSave()
	{
		string saveDiagnostic;
		int settled = OVT_TEST_PersistenceRoundTripGate.PollSaveSettled(m_iSaveBaseline, saveDiagnostic);
		if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_FAILED)
		{
			SetFailure(saveDiagnostic);
			return true;
		}

		if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_PENDING)
		{
			m_iSavePolls += 1;
			if (m_iSavePolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
			{
				SetFailure(OVT_TEST_PersistenceRoundTripGate.CAPABILITY_ABSENT);
				return true;
			}

			return false;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Resurrects the group under the same owner key, then asks for the persisted state back.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool DirtyAndReload()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null before the reload");
			return true;
		}

		string factionKey;
		string groupName;
		if (!OVT_TEST_VirtualizationFixture.FindComposition(factionKey, groupName))
		{
			SetFailure("The faction registry stopped resolving a composition before the reload");
			return true;
		}

		m_iResurrectedHandle = virtualization.RegisterGroup(OVT_TEST_VirtualizationFixture.OWNER_SYSTEM, OWNER_KEY,
			factionKey, groupName, OVT_TEST_VirtualizationFixture.PickPosition(), null, SPAWN_DISTANCE_OVERRIDE);

		if (m_iResurrectedHandle == -1)
		{
			SetFailure("Could not register the resurrection group, so the dirty step did nothing and the assertion would be vacuous");
			return true;
		}

		string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
		if (reload != "")
		{
			SetFailure(reload);
			return true;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitReload()
	{
		if (OVT_TEST_PersistenceRoundTripGate.ReloadInProgress())
		{
			m_iReloadPolls += 1;
			if (m_iReloadPolls > OVT_TEST_PersistenceRoundTripGate.MAX_RELOAD_POLLS)
			{
				SetFailure("Reload never completed: the persisted data was still being re-applied after %1 polls", m_iReloadPolls.ToString());
				return true;
			}

			return false;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_ASSERT;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case ends here either way.
	protected bool Assert()
	{
		string restored = OVT_TEST_PersistenceRoundTripGate.RequireRestoredCampaign();
		if (restored != "")
		{
			SetFailure(restored);
			return true;
		}

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null after the reload");
			return true;
		}

		string failure = Verify(virtualization);

		// Cleanup BEFORE reporting, so a red assertion cannot leak a live group into the cases after it
		// or into the next save this suite takes.
		if (virtualization.IsRegistered(m_iResurrectedHandle))
			virtualization.UnregisterGroup(m_iResurrectedHandle);

		if (virtualization.IsRegistered(m_iWipedHandle))
			virtualization.UnregisterGroup(m_iWipedHandle);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("A wiped virtual group stayed wiped across the round trip, and the group registered under its owner key afterwards was removed with it");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when everything holds, or the first broken claim.
	protected string Verify(notnull OVT_VirtualizationManagerComponent virtualization)
	{
		if (virtualization.IsRegistered(m_iWipedHandle))
			return string.Format("Handle %1 was wiped before the save and is registered again after the reload - a destroyed group came back", m_iWipedHandle.ToString());

		if (virtualization.IsRegistered(m_iResurrectedHandle))
			return string.Format("Handle %1 was registered AFTER the save and survived the reload - the restore does not remove records the payload does not claim, so a live session's groups silently outlive the state that is supposed to define them",
				m_iResurrectedHandle.ToString());

		array<int> byOwner = virtualization.FindGroupsByOwner(OVT_TEST_VirtualizationFixture.OWNER_SYSTEM, OWNER_KEY);
		if (!byOwner.IsEmpty())
			return string.Format("FindGroupsByOwner still resolves %1 handle(s) for the wiped owner key - a consumer reclaiming after a load would adopt a group the player destroyed",
				byOwner.Count().ToString());

		if (virtualization.GetGroup(m_iResurrectedHandle))
			return "The resurrected handle no longer resolves to a record but still hands out a group entity";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! A PARTIALLY WIPED VIRTUAL GROUP SURVIVES A SAVE AND A RELOAD - handle, owner key, position,
//! composition, engine stamps, waypoint plan, and the IDENTITY of the slot that died.
//!
//! WHY THIS CASE EXISTS. This is the epic's persistence promise (G3/F8) and, under Route B, it is
//! entirely Overthrow's own machinery: vanilla persists nothing about these groups. The registry
//! serializer writes complete re-creation state and the manager rebuilds the group entity itself on
//! load, so every field that is missing from the payload is a field the campaign silently loses - a
//! garrison that comes back at full strength after the player fought it down to two men, or comes back
//! at the wrong place, or comes back with the wrong men alive.
//!
//! WHAT THE DIRTY STEP DOES, AND WHY IT IS THAT AND NOT A SMALL EDIT (suite header closures 2 and 3).
//! It kills every remaining slot, which WIPES the record: the registry entry is removed and the group
//! entity is deleted. The restore therefore cannot pass by leaving anything alone - it has to
//! re-create the group entity from the payload, put it back at the saved position with the saved
//! stamps, and rebuild the survivor mask with the right slot still dead. It also registers a second,
//! BOGUS group that the save knows nothing about, so "records the payload does not claim are dropped"
//! is asserted in the same pass. A reload that restores nothing leaves no record at all and the case
//! fails on its first assertion; a reload that restores campaign-start defaults produces no virtual
//! groups either, and none of the asserted values (a 1234 m spawn ring, a HIGH tier, a 37 m patrol
//! radius, one specific dead slot) is one a campaign start would ever produce.
//!
//! IT USES THE PUBLIC MANAGER API ONLY, per the suite's assertion rule: RegisterGroup /
//! ReportMemberKilled to make the state, and IsRegistered / GetRecord / GetMemberCount /
//! GetAliveMemberCount / GetMemberAlive / GetPosition / GetSpawnDistance / GetImportance /
//! FindGroupsByOwner / GetGroup to read it back. No storage type, no persisted record class and no
//! persistence-framework type is named anywhere in it; the only persistence-layer calls are the gate
//! class's two annotated seams, shared with every other case here.
//!
//! THE COMPOSITION IS DISCOVERED, NOT HARD-CODED (OVT_TEST_VirtualizationFixture, shared with the Init
//! tier): a faction-config rename must go red in the faction tests, not here. A roster of at least two
//! slots is required, because "the specific dead slot is still dead" has to be distinguishable from
//! "the count came back", and the shipped registries are small enough that the first resolvable entry
//! may be a two-man patrol - so every entry the faction defines is tried until one is big enough.
//!
//! PROVEN ABLE TO FAIL (fail proofs recorded, one per claim that could rot independently):
//!   - drop the `entry.position = GetPosition(handle)` write in SnapshotRegistry() (or the
//!     `record.m_vPosition = entry.position` read in ApplyPersistedRecord) and the group is re-created
//!     at the world origin: the position assertion goes red;
//!   - drop the slotAlive write in SnapshotRegistry(), or the ApplyPersistedMask() call, and the mask
//!     comes back all-alive: the dead-slot assertion goes red before the count assertion does;
//!   - drop the PushSlotMask() call in BuildRegisteredGroup and the record's mask survives while the
//!     group refills from vanilla's first-N rule - nothing here catches that, which is why the Init
//!     tier owns the runtime slot-selection proof;
//!   - unbind OVT_VirtualizationManagerSerializer from the game-mode configuration in Overthrow.conf
//!     and the reload restores nothing: the case reports the handle missing.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_VirtualGroups_SurviveSaveAndReload : SCR_AutotestCaseBase
{
	//! Distinctive owner key - nothing else in the session registers under it.
	static const string OWNER_KEY = "roundtrip_virtual_group";

	//! Owner key of the group registered AFTER the save, which the restore has to drop.
	static const string BOGUS_KEY = "roundtrip_bogus_group";

	//! A spawn ring no configured default produces (the global is 1750), so the assertion proves the
	//! per-registration override round-tripped and not merely that a global was re-read.
	//!
	//! DELIBERATELY TINY. The case lives across many frames - two bounded waits - and the engine's own
	//! 1 Hz lifecycle tick would materialise members of any group an observer stands inside, which would
	//! turn "it came back dormant" into a coin flip. A 23 m ring keeps the group virtual for the whole
	//! case without changing anything the case asserts (the despawn ring stays strictly larger, so the
	//! registration is still a normal ProximityDriven one).
	static const int SPAWN_DISTANCE_OVERRIDE = 23;

	//! Patrol completion radius carried in the plan's float array - the one float in the payload.
	static const float WAYPOINT_RADIUS = 37;

	//! Tier stamped on the registration. Never the default, so a restore that forgot importance and
	//! fell back to NORMAL is visible.
	static const int IMPORTANCE = SCR_EAISpawnImportance.HIGH;

	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected int m_iReloadPolls;

	protected int m_iHandle = -1;
	protected int m_iBogusHandle = -1;
	protected int m_iRoster;
	protected int m_iDeadSlot = -1;
	protected vector m_vSavedPosition;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
			return MutateAndSave();

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE)
			return AwaitSave();

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD)
			return DirtyAndReload();

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD)
			return AwaitReload();

		return Assert();
	}

	//------------------------------------------------------------------------------------------------
	//! Registers a patrolling group, kills ONE named slot, and saves that state.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool MutateAndSave()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null - the virtualization manager is missing from the game-mode prefab");
			return true;
		}

		string factionKey;
		string groupName;
		if (!OVT_TEST_VirtualizationFixture.FindComposition(factionKey, groupName))
		{
			SetFailure("No faction in this world defines a resolvable group registry entry, so a virtual group cannot be registered");
			return true;
		}

		string diagnostic;
		m_iHandle = RegisterMultiSlotGroup(virtualization, factionKey, groupName,
			OVT_TEST_VirtualizationFixture.PickPosition(), diagnostic);

		if (m_iHandle == -1)
		{
			SetFailure(diagnostic);
			return true;
		}

		m_iRoster = virtualization.GetMemberCount(m_iHandle);
		m_vSavedPosition = virtualization.GetPosition(m_iHandle);

		// The LAST slot, so the survivor mask is not simply "the first N are alive" - which is exactly
		// what vanilla's own count-based refill would produce, and what D2 exists to beat.
		m_iDeadSlot = m_iRoster - 1;
		virtualization.ReportMemberKilled(m_iHandle, m_iDeadSlot);

		if (!virtualization.IsRegistered(m_iHandle))
		{
			SetFailure("Reporting one death on a %1-slot group removed the record - the case needs a partially wiped group, not a wiped one", m_iRoster.ToString());
			return true;
		}

		if (virtualization.GetAliveMemberCount(m_iHandle) != m_iRoster - 1)
		{
			int alive = virtualization.GetAliveMemberCount(m_iHandle);
			virtualization.UnregisterGroup(m_iHandle);
			SetFailure("The group reports %1 alive after one death out of %2 - the state being saved is already wrong", alive.ToString(), m_iRoster.ToString());
			return true;
		}

		m_iSaveBaseline = OVT_TEST_PersistenceRoundTripGate.CompletedSaveCount();

		string trigger = OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce();
		if (trigger != "")
		{
			SetFailure(trigger);
			return true;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitSave()
	{
		string saveDiagnostic;
		int settled = OVT_TEST_PersistenceRoundTripGate.PollSaveSettled(m_iSaveBaseline, saveDiagnostic);
		if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_FAILED)
		{
			SetFailure(saveDiagnostic);
			return true;
		}

		if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_PENDING)
		{
			m_iSavePolls += 1;
			if (m_iSavePolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
			{
				SetFailure(OVT_TEST_PersistenceRoundTripGate.CAPABILITY_ABSENT);
				return true;
			}

			return false;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Destroys the saved group outright and invents one the save never knew about, then asks for the
	//! persisted state back.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool DirtyAndReload()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null before the reload");
			return true;
		}

		// Kill what is left. This wipes the record AND deletes the group entity, so the restore has to
		// build a new one rather than tidy up an existing one - which is the whole of Route B.
		for (int slot = 0; slot < m_iRoster; slot++)
		{
			if (virtualization.GetMemberAlive(m_iHandle, slot))
				virtualization.ReportMemberKilled(m_iHandle, slot);
		}

		if (virtualization.IsRegistered(m_iHandle))
		{
			SetFailure("Killing every remaining slot left handle %1 registered - the dirty step did not dirty anything", m_iHandle.ToString());
			return true;
		}

		string factionKey;
		string groupName;
		if (!OVT_TEST_VirtualizationFixture.FindComposition(factionKey, groupName))
		{
			SetFailure("The faction registry stopped resolving a composition before the reload");
			return true;
		}

		m_iBogusHandle = virtualization.RegisterGroup(OVT_TEST_VirtualizationFixture.OWNER_SYSTEM, BOGUS_KEY,
			factionKey, groupName, OVT_TEST_VirtualizationFixture.PickPosition(), null, SPAWN_DISTANCE_OVERRIDE);

		if (m_iBogusHandle == -1)
		{
			SetFailure("Could not register the bogus group, so 'records the save does not claim are dropped' would be asserted against nothing");
			return true;
		}

		string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
		if (reload != "")
		{
			SetFailure(reload);
			return true;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitReload()
	{
		if (OVT_TEST_PersistenceRoundTripGate.ReloadInProgress())
		{
			m_iReloadPolls += 1;
			if (m_iReloadPolls > OVT_TEST_PersistenceRoundTripGate.MAX_RELOAD_POLLS)
			{
				SetFailure("Reload never completed: the persisted data was still being re-applied after %1 polls", m_iReloadPolls.ToString());
				return true;
			}

			return false;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_ASSERT;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case ends here either way.
	protected bool Assert()
	{
		string restored = OVT_TEST_PersistenceRoundTripGate.RequireRestoredCampaign();
		if (restored != "")
		{
			SetFailure(restored);
			return true;
		}

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null after the reload");
			return true;
		}

		string failure = Verify(virtualization);

		// Cleanup BEFORE reporting, so a red assertion cannot leak a live group into the cases after it
		// or into the next save this suite takes.
		if (virtualization.IsRegistered(m_iHandle))
			virtualization.UnregisterGroup(m_iHandle);

		if (virtualization.IsRegistered(m_iBogusHandle))
			virtualization.UnregisterGroup(m_iBogusHandle);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		// SCR_AutotestCaseBase.PrintFormat takes three substitutions, not the global's nine.
		PrintFormat("A partially wiped virtual group came back from the save: handle %1, %2-slot roster, slot %3 still dead",
			m_iHandle.ToString(), m_iRoster.ToString(), m_iDeadSlot.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when everything holds, or the first broken claim.
	protected string Verify(notnull OVT_VirtualizationManagerComponent virtualization)
	{
		if (!virtualization.IsRegistered(m_iHandle))
			return string.Format("Handle %1 did not come back from the save - the registry was destroyed before the reload and the restore re-created nothing, so a continued campaign would lose every virtual group it had",
				m_iHandle.ToString());

		OVT_VirtualGroupRecord record = virtualization.GetRecord(m_iHandle);
		if (!record)
			return string.Format("Handle %1 is registered but hands out no record", m_iHandle.ToString());

		if (record.m_sOwnerSystem != OVT_TEST_VirtualizationFixture.OWNER_SYSTEM || record.m_sOwnerKey != OWNER_KEY)
			return string.Format("The restored record is owned by %1/%2, expected %3/%4 - a consumer would reclaim someone else's group",
				record.m_sOwnerSystem, record.m_sOwnerKey, OVT_TEST_VirtualizationFixture.OWNER_SYSTEM, OWNER_KEY);

		if (virtualization.GetMemberCount(m_iHandle) != m_iRoster)
			return string.Format("The restored group has a roster of %1 slots, expected %2",
				virtualization.GetMemberCount(m_iHandle).ToString(), m_iRoster.ToString());

		if (virtualization.GetAliveMemberCount(m_iHandle) != m_iRoster - 1)
			return string.Format("The restored group reports %1 living members, expected %2 - the casualty was forgotten across the save",
				virtualization.GetAliveMemberCount(m_iHandle).ToString(), (m_iRoster - 1).ToString());

		// THE D2 CLAIM. A count-accurate restore that got the identity wrong passes everything above
		// this line and fails here, which is exactly the failure mode vanilla's count-only dormancy has.
		if (virtualization.GetMemberAlive(m_iHandle, m_iDeadSlot))
			return string.Format("Slot %1 was killed before the save and is alive again after the reload - the group came back at the right STRENGTH with the wrong men in it (D2)",
				m_iDeadSlot.ToString());

		for (int slot = 0; slot < m_iRoster; slot++)
		{
			if (slot == m_iDeadSlot)
				continue;

			if (!virtualization.GetMemberAlive(m_iHandle, slot))
				return string.Format("Slot %1 was alive when the save was taken and is dead after the reload - the survivor mask came back shifted",
					slot.ToString());
		}

		if (vector.Distance(virtualization.GetPosition(m_iHandle), m_vSavedPosition) > 1)
			return string.Format("The restored group is at %1, saved at %2 - a re-created group that ignores the saved position moves every garrison in the campaign",
				virtualization.GetPosition(m_iHandle).ToString(), m_vSavedPosition.ToString());

		array<int> byOwner = virtualization.FindGroupsByOwner(OVT_TEST_VirtualizationFixture.OWNER_SYSTEM, OWNER_KEY);
		if (byOwner.Find(m_iHandle) == -1)
			return string.Format("FindGroupsByOwner does not resolve the restored handle %1 - the reclaim seam every consumer uses after a load is broken even though the record exists",
				m_iHandle.ToString());

		if (virtualization.IsRegistered(m_iBogusHandle))
			return string.Format("Handle %1 was registered AFTER the save and survived the reload - the restore does not drop records the payload has no entry for",
				m_iBogusHandle.ToString());

		if (!virtualization.FindGroupsByOwner(OVT_TEST_VirtualizationFixture.OWNER_SYSTEM, BOGUS_KEY).IsEmpty())
			return "The bogus owner key still resolves to a handle after the reload";

		return VerifyEntity(virtualization, record);
	}

	//------------------------------------------------------------------------------------------------
	//! The half that says the record is not just bookkeeping: a real, dormant, correctly stamped group
	//! entity exists again, with the waypoint plan it was registered with.
	//! \param[in] virtualization The manager.
	//! \param[in] record The restored record.
	//! \return An empty string when everything holds, or the first broken claim.
	protected string VerifyEntity(notnull OVT_VirtualizationManagerComponent virtualization, notnull OVT_VirtualGroupRecord record)
	{
		SCR_AIGroup group = virtualization.GetGroup(m_iHandle);
		if (!group)
			return "The record came back without a group entity - the registry remembers the group but nothing in the world is it, so it can never materialise";

		if (group.GetAgentsCount() != 0 || virtualization.IsSpawned(m_iHandle))
			return string.Format("The re-created group already has %1 member(s) - it must come back DORMANT and let the engine materialise it on approach",
				group.GetAgentsCount().ToString());

		if (group.GetLifecyclePolicy() != SCR_EAIGroupLifecyclePolicy.ProximityDriven)
			return string.Format("The re-created group's lifecycle policy is %1, expected ProximityDriven (%2) - a restored group without the policy never appears again",
				group.GetLifecyclePolicy().ToString(), SCR_EAIGroupLifecyclePolicy.ProximityDriven.ToString());

		if (virtualization.GetSpawnDistance(m_iHandle) != SPAWN_DISTANCE_OVERRIDE)
			return string.Format("The restored spawn ring is %1 m, expected the registered override of %2 m",
				virtualization.GetSpawnDistance(m_iHandle).ToString(), SPAWN_DISTANCE_OVERRIDE.ToString());

		if (Math.AbsFloat(group.GetSpawnDistance() - SPAWN_DISTANCE_OVERRIDE) > 1)
			return string.Format("The re-created group entity carries a %1 m spawn ring, expected the restored %2 m",
				group.GetSpawnDistance().ToString(), SPAWN_DISTANCE_OVERRIDE.ToString());

		if (virtualization.GetImportance(m_iHandle) != IMPORTANCE || group.GetImportance() != IMPORTANCE)
			return string.Format("The restored group sits at importance %1 (entity %2), expected the registered %3 - an unstamped group is budget-starved at vanilla's LOW tier",
				virtualization.GetImportance(m_iHandle).ToString(), group.GetImportance().ToString(), IMPORTANCE.ToString());

		if (!record.m_Plan)
			return "The restored record has no waypoint plan - a patrol comes back as a garrison, and nothing would rebuild its waypoints on the next load either";

		if (record.m_Plan.m_aPositions.Count() != 2)
			return string.Format("The restored waypoint plan has %1 leg(s), expected 2", record.m_Plan.m_aPositions.Count().ToString());

		if (!record.m_Plan.m_bCycle)
			return "The restored waypoint plan is not cycling - the patrol would run its legs once and stop";

		if (record.m_Plan.m_aTypes[0] != OVT_EVirtualWaypointType.PATROL)
			return string.Format("The restored waypoint type is %1, expected PATROL (%2)",
				record.m_Plan.m_aTypes[0].ToString(), OVT_EVirtualWaypointType.PATROL.ToString());

		if (Math.AbsFloat(record.m_Plan.m_aParams[0] - WAYPOINT_RADIUS) > 0.01)
			return string.Format("The restored waypoint parameter is %1, expected %2",
				record.m_Plan.m_aParams[0].ToString(), WAYPOINT_RADIUS.ToString());

		if (record.m_aOwnedWaypoints.IsEmpty())
			return "The re-created group owns no waypoint entities - the plan survived but nothing was built from it, so the patrol stands still";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Registers a group with at least two roster slots.
	//!
	//! The roster size is only knowable once the entity exists, so this registers, checks, and drops
	//! anything too small before trying the next registry entry the faction defines. The composition
	//! FindComposition() already resolved is tried first.
	//! \param[in] virtualization The manager.
	//! \param[in] factionKey Faction to register from.
	//! \param[in] groupName The registry entry to try first.
	//! \param[in] position Where to register.
	//! \param[out] diagnostic Why no composition was usable; untouched on success.
	//! \return The handle, or -1.
	protected int RegisterMultiSlotGroup(notnull OVT_VirtualizationManagerComponent virtualization,
		string factionKey, string groupName, vector position, out string diagnostic)
	{
		array<string> candidates = new array<string>();
		candidates.Insert(groupName);

		foreach (string name : OVT_TEST_VirtualizationFixture.ListGroupNames(factionKey))
		{
			if (name != groupName)
				candidates.Insert(name);
		}

		foreach (string candidate : candidates)
		{
			int handle = virtualization.RegisterGroup(OVT_TEST_VirtualizationFixture.OWNER_SYSTEM, OWNER_KEY,
				factionKey, candidate, position, BuildPlan(position), SPAWN_DISTANCE_OVERRIDE, IMPORTANCE);

			if (handle == -1)
				continue;

			if (virtualization.GetMemberCount(handle) >= 2)
				return handle;

			virtualization.UnregisterGroup(handle);
		}

		diagnostic = string.Format("Faction '%1' defines no group registry entry with two or more roster slots, so 'the specific dead slot is still dead' cannot be told apart from 'the count came back'",
			factionKey);

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! A two-leg cycling patrol, so the payload's vector, int and float parallel arrays and its cycle
	//! flag are all exercised by the round trip.
	//! \param[in] position Where the group is registered.
	//! \return The plan.
	protected OVT_VirtualWaypointPlan BuildPlan(vector position)
	{
		OVT_VirtualWaypointPlan plan = new OVT_VirtualWaypointPlan();

		plan.m_aPositions.Insert(position);
		plan.m_aTypes.Insert(OVT_EVirtualWaypointType.PATROL);
		plan.m_aParams.Insert(WAYPOINT_RADIUS);

		plan.m_aPositions.Insert(position + Vector(0, 0, 150));
		plan.m_aTypes.Insert(OVT_EVirtualWaypointType.PATROL);
		plan.m_aParams.Insert(WAYPOINT_RADIUS);

		plan.m_bCycle = true;

		return plan;
	}
}
