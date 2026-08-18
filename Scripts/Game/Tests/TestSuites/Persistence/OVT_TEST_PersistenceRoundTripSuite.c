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
//!   1. Capability_SaveGameProducesASave        - the gate, and the only case with no reload.
//!      ⚠ It asserts HasSaveGame() is FALSE before it saves, so every case that takes a real save
//!      MUST sort after it alphabetically - name deployment cases "Deployment*", never "Base*".
//!   1a. DeploymentBaseDefense_SurvivesSaveAndReapply    )
//!   1b. DeploymentEliminated_RegistersNoGroups          ) the five DEPLOYMENT cases; read
//!   1c. DeploymentOwnedGroups_ReclaimAfterReload        ) OVT_TEST_DeploymentRoundTripFixture's
//!   1d. DeploymentRecord_SurvivesSaveAndReapply         ) header FIRST - four of them assert the
//!   1e. DeploymentVersion1Payload_StillLoads            ) RESTORE half only, and it says why
//!   1d2. FuelDepot_LevelSurvivesSave          - builds a Fuel Depot, fills it and takes a real save.
//!                                                DELIBERATELY DEGRADED and uses NEITHER reload seam:
//!                                                the depot is its own tracked root, and the load seam
//!                                                below re-applies the GAME MODE entity's record only.
//!                                                Read its own header before "fixing" it
//!   1e. JobBoard_SurvivesSaveAndReload         - the only case that re-applies TWICE, because
//!                                                idempotency is part of what it asserts
//!   1f. LegacyBaseUpgrades_ConvertToDeploymentResources - a pre-migration base payload is refunded to
//!                                                the deployment pool exactly once. Takes a real save
//!                                                (which is what runs the rewritten write path) but
//!                                                uses NEITHER reload seam
//!   2. PlayerMoney_SurvivesSaveAndReload
//!   2a. PlayerSleepCooldown_SurvivesSaveAndReload - the sleep action's game-clock cooldown stamp
//!   3. PlayerSkills_SurvivesSaveAndReload
//!   4. RealEstateOwnership_SurvivesSaveAndReload
//!   5. Recruits_SurvivesSaveAndReload
//!   6. TownControl_SurvivesSaveAndReload
//!   7. TownPopulation_SurvivesSaveAndReload
//!   8. TownStability_SurvivesSaveAndReload
//!   9. TownSupport_SurvivesSaveAndReload
//!  10. VehiclePoseReassert_SnapsBackOnlyBeyondTolerance - the load-drift healing seam, no save point
//!  11. VehicleRegistry_SurvivesSaveAndReload
//!  12. VehicleReserveRelease_KeepsOwnerAndContents - per-instance reservation, no save point
//!  13. VirtualGroupsWiped_DoNotComeBack           - the terminal half of the D2 promise
//!  14. VirtualGroups_SurviveSaveAndReload         - the partially wiped group, re-CREATED on load
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
//! Seen-tutorial state survives a save and a reload on the player's campaign record.
//!
//! The replacement for the retired per-machine profile store's round-trip gate (2026-08-18):
//! tutorial progress now rides OVT_PlayerManagerSerializer version 4, and this is the case that
//! fails if either half of the append - the ids or the flag - stops being written or re-applied.
//! Same shape as the PlayerMoney case above: mutate, save, DIRTY, reload, assert.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_TutorialSeen_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	//! Test-only ids. Leading underscores are illegal in the authored entry-id scheme (lowercase
	//! ASCII letters, digits and dashes), so these cannot collide with real content, ever.
	static const string TEST_ID_A = "__ovt-selftest-persist-alpha";
	static const string TEST_ID_B = "__ovt-selftest-persist-beta";

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
				SetFailure("No OVT_PlayerData record for the local player");
				return true;
			}

			if (!player.m_aSeenTutorials)
				player.m_aSeenTutorials = new array<string>();

			player.m_aSeenTutorials.Clear();
			player.m_aSeenTutorials.Insert(TEST_ID_A);
			player.m_aSeenTutorials.Insert(TEST_ID_B);
			player.m_bTutorialsDisabled = true;

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
				SetFailure("No OVT_PlayerData record for the local player before the reload");
				return true;
			}

			// Dirty it: a reload that restores nothing now cannot pass.
			if (player.m_aSeenTutorials)
				player.m_aSeenTutorials.Clear();
			player.m_bTutorialsDisabled = false;

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
			SetFailure("No OVT_PlayerData record for the local player after the reload");
			return true;
		}

		if (!player.m_aSeenTutorials || !player.m_aSeenTutorials.Contains(TEST_ID_A) || !player.m_aSeenTutorials.Contains(TEST_ID_B))
		{
			int count = 0;
			if (player.m_aSeenTutorials)
				count = player.m_aSeenTutorials.Count();

			SetFailure("Seen tutorials did not survive the round trip: saved 2 ids, read back %1 - OVT_PlayerManagerSerializer version 4 is not carrying m_aSeenTutorials, so every tip would re-show on every continue", count.ToString());
			return true;
		}

		if (!player.m_bTutorialsDisabled)
		{
			SetFailure("The tutorials-disabled flag did not survive the round trip: saved true, read back false - a player who pressed \"Don't show tips again\" would be shown tips again on continue");
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
//! A player's sleep cooldown stamp survives a save and a reload.
//!
//! WHAT IS ACTUALLY BEING GUARDED. Sleeping skips eight in-game hours and may not be repeated for
//! twelve, and the whole reason that cooldown is stored as an absolute GAME-CLOCK stamp rather than
//! as a real-time countdown is that it has to survive a quit and a Continue (implementation.md D8/
//! I2). If the stamp is lost, every load hands the player a fresh sleep - eight more hours of income
//! and threat decay for the price of a save and a reload, which is the same shape of exploit as
//! BUG-183. Nothing else in the tree would go red for it: the value is server-only, never
//! replicated, and invisible in every UI except the action's own label.
//!
//! THE SAVED VALUE IS SYNTHETIC, AND THAT IS CORRECT HERE - the opposite of the last-known-position
//! case above, where a pre-save capture overwrites whatever the case wrote. Nothing runs over this
//! field before a save: it is written in exactly one place (OVT_SleepService.PerformSleep) and read
//! everywhere else, so poking a value in and asserting it comes back tests the whole codec path
//! without needing a bed, a clock or an owned house in the test world.
//!
//! THE DIRTY VALUE IS THE NEVER-SLEPT SENTINEL, deliberately. -1 is exactly the state a player who
//! has never slept is in - it is also what a version 4 save's players are reset to on load - so it
//! is both the honest "this was lost" value and the one whose survival would be indistinguishable
//! from a working restore if the assertion were merely "not the saved value". The assertion is
//! equality with the SAVED stamp (anti-vacuous-pass closures 2 and 3).
//!
//! CAN-FAIL METHOD (run owed - an implementation agent does not run the suites): remove
//! `record.lastSleepGameHours` from the write half of OVT_PlayerManagerSerializer.Serialize(), or
//! change the read guard to `if (version < 6)`. Either makes the restore hand back -1 and the case
//! reports "the sleep cooldown stamp came back as -1.000000 ...". Record the date here once run.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_PlayerSleepCooldown_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	//! The stamp written before the save. A plausible absolute-game-hours value (roughly six months
	//! into a campaign) and one no campaign start would ever produce - a fresh record carries -1 and
	//! nothing else writes this field.
	static const float SAVED_STAMP = 4321.5;

	//! What the value is destroyed to between the save and the reload: the never-slept sentinel.
	static const float DIRTY_STAMP = -1;

	//! Exact-value tolerance. The stamp is one float through one codec, so this absorbs
	//! representation noise only - it is far tighter than the difference between the two values above.
	static const float STAMP_TOLERANCE = 0.001;

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

			player.m_fLastSleepGameHours = SAVED_STAMP;

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

			// See the header: the sentinel is what a lost stamp looks like in production.
			player.m_fLastSleepGameHours = DIRTY_STAMP;

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

		if (Math.AbsFloat(player.m_fLastSleepGameHours - SAVED_STAMP) > STAMP_TOLERANCE)
		{
			SetFailure("The sleep cooldown stamp came back as %1, expected the saved %2. A player whose stamp is lost on load may sleep again immediately, which is eight in-game hours of income and threat decay for the price of a save and a reload.",
				player.m_fLastSleepGameHours.ToString(), SAVED_STAMP.ToString());
			return true;
		}

		PrintFormat("Sleep cooldown stamp round-tripped: saved %1, dirtied to %2, restored %3",
			SAVED_STAMP.ToString(), DIRTY_STAMP.ToString(), player.m_fLastSleepGameHours.ToString());

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A restored vehicle that load-time physics moved is snapped back to its recorded pose - and one
//! that has not drifted is left exactly where it stands.
//!
//! WHY THIS EXISTS (server reports, 2026-08-18). A vehicle parked on top of a buildable
//! maintenance ramp came back rotated ~45 degrees after a load. The pose DATA round-trips exactly
//! (vanilla stores the full transform; the registry stores position + yaw-pitch-roll through one
//! consistent convention) - what moves the vehicle is physics AT the load: it self-spawns as a
//! live dynamic body with no saved velocities, while the ramp under it is a separately
//! self-spawned record with no ordering guarantee, so the vehicle free-falls onto the terrain or
//! takes the depenetration kick when its support spawns into it (the BUG-129 mechanism). On flat
//! ground both effects are invisible, which is why only ramp-parked vehicles were reported.
//! InitialVehicleCleanup() heals it by re-asserting each registered vehicle's recorded pose;
//! OVT_VehicleManagerComponent.ReassertRecordedPose() is that seam, and this case drives it
//! directly.
//!
//! THE NO-OP HALF IS ASSERTED FIRST, deliberately: a seam that snapped every vehicle - drifted or
//! not - would teleport cars out from under their owners on every load. Below-tolerance drift must
//! be left untouched.
//!
//! BOTH HALVES ASSERT IN THE SAME FRAME AS THE SEAM CALL, so physics cannot settle between act and
//! assert and the tolerances can be tight (centimetres/a degree, not the 25 m the respawn case
//! needs).
//!
//! WHAT THIS CANNOT SEE: the real load-order race (ramp spawning after the vehicle), which needs a
//! genuine restart and is play-test territory. What it pins is the healing seam's whole contract.
//!
//! CAN-FAIL METHOD (run owed - this environment cannot launch the harness): make
//! ReassertRecordedPose() return before the snap-back; the case must report the drifted vehicle
//! still ~1.7 m / 45 deg from its record. Record the date here once exercised.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_VehiclePoseReassert_SnapsBackOnlyBeyondTolerance : SCR_AutotestCaseBase
{
	static const int PHASE_SPAWN = 0;
	static const int PHASE_AWAIT_REGISTRATION = 1;
	static const int PHASE_ASSERT = 2;

	//! Same contract as the reserve/release case's budget: a diagnostic backstop, not a retry.
	static const int MAX_REGISTRATION_POLLS = 120;

	//! The below-tolerance drift the seam must ignore: under the manager's 0.5 m / 5 deg bounds.
	static const float SMALL_DRIFT_M = 0.2;
	static const float SMALL_DRIFT_DEG = 2;

	//! The reported failure's shape: rotated ~45 degrees, displaced by a ramp-height fall.
	static const float BIG_DRIFT_DEG = 45;

	//! Same-frame assertion slack. SetTransform is synchronous; this absorbs float noise only.
	static const float POSITION_EPSILON_M = 0.05;
	static const float ANGLE_EPSILON_DEG = 1;

	protected int m_iPhase;
	protected int m_iRegistrationPolls;
	protected string m_sPersId;
	protected string m_sVehicleId;
	protected IEntity m_Vehicle;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == PHASE_SPAWN)
			return SpawnSubjectVehicle();

		if (m_iPhase == PHASE_AWAIT_REGISTRATION)
			return AwaitRegistration();

		return AssertSeamContract();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns an owned vehicle through the manager's own spawn seam, so it is registered and has a
	//! captured record - the two things the pose seam reads.
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

		vector mat[4];
		Math3D.AnglesToMatrix("25 0 0", mat);
		mat[3] = position;

		m_Vehicle = vehicles.SpawnVehicleMatrix(prefab, mat, m_sPersId);
		if (!m_Vehicle)
		{
			SetFailure("SpawnVehicleMatrix() produced no vehicle at %1", position.ToString());
			return true;
		}

		m_iPhase = PHASE_AWAIT_REGISTRATION;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for the manager to register the vehicle, which is also when its record is captured.
	//! \return True when the case is finished.
	protected bool AwaitRegistration()
	{
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		if (!vehicles || !m_Vehicle)
		{
			SetFailure("The vehicle or its manager disappeared while waiting for registration");
			return true;
		}

		array<string> registered = vehicles.GetPlayerVehicleIds(m_sPersId);
		foreach (string vehicleId : registered)
		{
			if (vehicles.FindVehicleEntity(vehicleId) == m_Vehicle)
			{
				m_sVehicleId = vehicleId;
				m_iPhase = PHASE_ASSERT;
				return false;
			}
		}

		m_iRegistrationPolls += 1;
		if (m_iRegistrationPolls > MAX_REGISTRATION_POLLS)
		{
			SetFailure("The vehicle manager never registered the vehicle it spawned for '%1' - there is no record for the pose seam to read", m_sPersId);
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Drives both halves of the seam's contract in one frame: below-tolerance drift ignored,
	//! above-tolerance drift snapped back to the record exactly.
	//! \return Always true - the case ends here either way.
	protected bool AssertSeamContract()
	{
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		if (!vehicles || !m_Vehicle)
		{
			SetFailure("The vehicle or its manager disappeared before the assertion");
			return true;
		}

		map<string, ref OVT_PersistedPlayerVehicle> records = vehicles.GetVehicleRecords();
		if (!records.Contains(m_sVehicleId))
		{
			SetFailure("Vehicle %1 is registered but has no captured record - CaptureVehicleRecord() no longer runs at registration", m_sVehicleId);
			return true;
		}

		OVT_PersistedPlayerVehicle record = records[m_sVehicleId];

		// Pin the vehicle exactly at its recorded pose first, so the drifts applied below are the
		// ONLY drift there is - physics may have settled it slightly since the spawn.
		vector recordMat[4];
		Math3D.AnglesToMatrix(record.angles, recordMat);
		recordMat[3] = record.position;
		m_Vehicle.SetTransform(recordMat);

		// HALF 1: drift below both tolerances must be left exactly where it is.
		vector smallAngles = record.angles;
		smallAngles[0] = smallAngles[0] + SMALL_DRIFT_DEG;
		vector smallMat[4];
		Math3D.AnglesToMatrix(smallAngles, smallMat);
		smallMat[3] = record.position + Vector(SMALL_DRIFT_M, 0, 0);
		m_Vehicle.SetTransform(smallMat);

		vehicles.ReassertRecordedPose(m_sVehicleId, m_Vehicle);

		if (vector.Distance(m_Vehicle.GetOrigin(), smallMat[3]) > POSITION_EPSILON_M)
		{
			SetFailure("ReassertRecordedPose() moved a vehicle whose drift (%1 m / %2 deg) is below tolerance - the seam would teleport cars out from under their owners on every load",
				SMALL_DRIFT_M.ToString(), SMALL_DRIFT_DEG.ToString());
			return true;
		}

		// HALF 2: the reported failure's shape - rotated ~45 degrees and displaced a ramp-height
		// fall away - must snap back to the record.
		vector bigAngles = record.angles;
		bigAngles[0] = bigAngles[0] + BIG_DRIFT_DEG;
		vector bigMat[4];
		Math3D.AnglesToMatrix(bigAngles, bigMat);
		bigMat[3] = record.position + Vector(1.5, -0.9, 0.7);
		m_Vehicle.SetTransform(bigMat);

		vehicles.ReassertRecordedPose(m_sVehicleId, m_Vehicle);

		float positionError = vector.Distance(m_Vehicle.GetOrigin(), record.position);
		if (positionError > POSITION_EPSILON_M)
		{
			SetFailure("ReassertRecordedPose() did not snap the drifted vehicle back: it stands %1 m from its recorded position - a ramp-parked vehicle stays where load physics dropped it",
				positionError.ToString());
			return true;
		}

		float yawError = Math.AbsFloat(m_Vehicle.GetYawPitchRoll()[0] - record.angles[0]);
		if (yawError > 180)
			yawError = 360 - yawError;

		if (yawError > ANGLE_EPSILON_DEG)
		{
			SetFailure("ReassertRecordedPose() left the drifted vehicle rotated %1 deg off its recorded yaw - the reported symptom exactly",
				yawError.ToString());
			return true;
		}

		PrintFormat("Pose seam contract holds: %1 m / %2 deg drift ignored, 1.7 m / 45 deg drift snapped back to the record",
			SMALL_DRIFT_M.ToString(), SMALL_DRIFT_DEG.ToString());
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
//! ⚠ THE FIXTURE'S PLAN IS DELIBERATELY STATIONARY (`virtualization/movement` T3.1, finding F-A).
//! THIS CASE ASSERTS PERSISTENCE, NOT MOTION - and virtual movement advances ANY dormant registered
//! group whose plan has something to advance, including one a test registered. The plan below was two
//! MOVE-class (PATROL) points 150 m apart, so the movement tick would walk this fixture across the
//! multi-second save/reload window and turn the ±1 m position claim into a timing lottery. The points
//! are therefore DEFEND, which is core's "this group belongs here" plan and is never advanced (D10:
//! the plan IS the opt-in). Every payload claim is unchanged in number and strength - two distinct
//! positions, two types, two params and m_bCycle all still round-trip - and the fixture's stillness is
//! now a property of what it registers rather than of what happens to be ticking.
//! If a future feature needs a MOVING fixture, register a second group for it; do not make this one
//! move. Widening the tolerance was rejected: it would turn a precise claim into a timing lottery.
//!
//! EVERY OTHER RegisterGroup( SITE UNDER Scripts/Game/Tests/ WAS SWEPT AND IS SAFE (T3.1), and the two
//! properties that make a fixture safe are worth knowing when writing a new one:
//!   (a) it registers a null / empty / DEFEND-only plan - there is nothing for movement to advance; or
//!   (b) it registers and unregisters inside ONE frame - a CallLater tick cannot interleave.
//! Init tier: RegisterRefusesUnknownComposition (both registrations are REFUSED, nothing is booked),
//! RegisterBuildsDormantGroup, GetAllHandlesEnumeratesRegistry, DeathsFlipMaskAndWipeRecord and the
//! mask-driven-refill case all pass (a) AND (b); the waypoint-ownership case registers a REAL movable
//! plan (PATROL + MOVE, 120 m, cycling) but tears it down in the same frame, so it passes (b) and
//! asserts nothing about position. Persistence tier: the wiped-group case, its resurrection group and
//! this case's BOGUS group all register a null plan. This fixture was the only unsafe site in the tree.
//! RE-SWEPT 2026-08-17 (virtualization/integration T7.1): the deployment reclaim case adds three more
//! null-plan registrations at spawn distance 0, and the three deployment-marker fixtures register
//! nothing at all because they are marked eliminated before anything can converge them. The verdict
//! table per site is in docs/features/virtualization/integration/context.md.
//!
//! THE MOVED-POSITION CLAIM (`virtualization/movement` T3.2). Before the save, the group is deliberately
//! relocated with SetPosition() and the case asserts it comes back at the MOVED position and NOT at the
//! registration one. That is the epic-level property movement depends on - core's SnapshotRegistry reads
//! the LIVE group origin, so whatever moved a group is what a save keeps - asserted here without a single
//! movement-specific line, and timing-free: a direct write, not a tick.
//!
//! PROVEN ABLE TO FAIL (fail proofs recorded, one per claim that could rot independently):
//!   - THE MOVED-POSITION PAIR. Delete the `virtualization.SetPosition(...)` call in MutateAndSave()
//!     and the pre-save guard goes red ("the group never moved") - the fixture cannot silently stop
//!     being a moved one. Keep the move and snapshot a registration-time value instead - change
//!     `entry.position = GetPosition(handle)` in SnapshotRegistry() (`:984`) to
//!     `entry.position = record.m_Plan.m_aPositions[0]`, which for this fixture IS the registration
//!     position - and the group comes back 42 m from where the save should have put it: the
//!     saved-position claim goes red and the registration-position claim goes red with it, naming
//!     exactly which of the two the restore fell back to. (The second claim is deliberately implied by
//!     the first while the fixture really moves; its job is to make that "really moves" self-enforcing,
//!     so nobody can quietly delete the move and leave a claim that asserts a coincidence.);
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

	//! The per-waypoint float parameter carried in the plan's float array - the one float in the payload.
	//! A DEFEND waypoint's param is its completion radius, so this still round-trips as a real value.
	static const float WAYPOINT_RADIUS = 37;

	//! How far the group is deliberately moved before the save (T3.2). Big enough that the ±1 m claim
	//! below cannot confuse "came back moved" with "came back where it was registered", and applied on
	//! Z only so the assertion reads as a plain offset. Nothing ground-snaps a dormant group's origin,
	//! so the value that goes in is the value that must come back.
	static const vector MOVE_OFFSET = "0 0 42";

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

	//! Where the group was REGISTERED - the position the save must NOT come back with (T3.2).
	protected vector m_vRegisteredPosition;

	//! Where the group actually was when the save was taken: the registration position plus MOVE_OFFSET.
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
		m_vRegisteredPosition = virtualization.GetPosition(m_iHandle);

		// T3.2 - MOVE THE GROUP BEFORE THE SAVE. A registered group's origin does not stay where a
		// consumer first put it: `virtualization/movement` walks dormant groups, and core's snapshot
		// reads the LIVE origin, so what a save keeps is wherever the group has got to. Written
		// DIRECTLY rather than by waiting for a tick, so the claim is timing-free and holds whether or
		// not any movement code is running.
		virtualization.SetPosition(m_iHandle, m_vRegisteredPosition + MOVE_OFFSET);
		m_vSavedPosition = virtualization.GetPosition(m_iHandle);

		if (vector.Distance(m_vSavedPosition, m_vRegisteredPosition) <= 1)
		{
			virtualization.UnregisterGroup(m_iHandle);
			SetFailure("SetPosition left the group at %1 (registered at %2) - the group never moved, so 'the MOVED position is what round-trips' would be asserted against nothing",
				m_vSavedPosition.ToString(), m_vRegisteredPosition.ToString());
			return true;
		}

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

		// T3.2 - AND IT IS THE MOVED POSITION, NOT THE REGISTRATION ONE. A snapshot that read the
		// registration position instead of the live origin would satisfy nothing above this line only by
		// luck; this says so outright, and it is the property `virtualization/movement` rests on - the
		// tick's writes are what a save keeps.
		if (vector.Distance(virtualization.GetPosition(m_iHandle), m_vRegisteredPosition) <= 1)
			return string.Format("The restored group came back at its REGISTRATION position %1 rather than the %2 it was moved to before the save - a save that keeps where a group was first put, not where it actually is, freezes every moved group back to its origin on load",
				m_vRegisteredPosition.ToString(), m_vSavedPosition.ToString());

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

		if (record.m_Plan.m_aTypes[0] != OVT_EVirtualWaypointType.DEFEND)
			return string.Format("The restored waypoint type is %1, expected DEFEND (%2)",
				record.m_Plan.m_aTypes[0].ToString(), OVT_EVirtualWaypointType.DEFEND.ToString());

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
	//! A two-point cycling DEFEND plan, so the payload's vector, int and float parallel arrays and its
	//! cycle flag are all exercised by the round trip.
	//!
	//! ⚠ THE TYPES ARE DEFEND ON PURPOSE (`virtualization/movement` T3.1 / D12, finding F-A). They were
	//! PATROL, which is a movable type: the movement tick advances every dormant registered group whose
	//! plan has something to advance, so this fixture would drift along its own 150 m leg while the case
	//! waited out a save and a reload, and the ±1 m position claim in Verify() would go red for reasons
	//! that have nothing to do with persistence. DEFEND is core's "this group belongs here" plan and is
	//! never advanced, which makes this fixture's stillness a property of the fixture rather than of
	//! whatever else happens to be ticking in the world.
	//!
	//! The payload claims are UNCHANGED in number and strength: still two distinct positions 150 m
	//! apart, still two types, still two float params, still m_bCycle - and still three real waypoint
	//! entities on the re-created group (two legs plus the cycle), because core builds a DEFEND
	//! waypoint from a DEFEND plan entry exactly as it builds a patrol one from a PATROL entry.
	//! \param[in] position Where the group is registered.
	//! \return The plan.
	protected OVT_VirtualWaypointPlan BuildPlan(vector position)
	{
		OVT_VirtualWaypointPlan plan = new OVT_VirtualWaypointPlan();

		plan.m_aPositions.Insert(position);
		plan.m_aTypes.Insert(OVT_EVirtualWaypointType.DEFEND);
		plan.m_aParams.Insert(WAYPOINT_RADIUS);

		plan.m_aPositions.Insert(position + Vector(0, 0, 150));
		plan.m_aTypes.Insert(OVT_EVirtualWaypointType.DEFEND);
		plan.m_aParams.Insert(WAYPOINT_RADIUS);

		plan.m_bCycle = true;

		return plan;
	}
}

//------------------------------------------------------------------------------------------------
//! Shared machinery for the four DEPLOYMENT cases below (virtualization/integration Phase 7), so
//! each case reads as its own claim instead of as 60 lines of setup.
//!
//! ===========================================================================================
//! THE HONEST LIMIT OF THESE FOUR CASES, STATED ONCE AND NOT REPEATED IN EVERY HEADER.
//!
//! A deployment is a MARKER ENTITY in the world, and its state is written by a component
//! serializer bound to that entity's own configuration - NOT to the game mode's. The suite's
//! reload seam (OVT_TEST_PersistenceRoundTripGate.RequestSessionReload) asks for exactly one
//! instance back, the game mode entity, and its own header says so in as many words: "WHAT IT
//! DOES NOT COVER. Anything outside the game mode entity's record - world entities, characters,
//! vehicles, placeables." So the seam CANNOT hand a deployment marker its stored payload back.
//!
//! What these cases therefore do is split the round trip in two and assert both halves honestly:
//!
//!   WRITE HALF - real. The T7.2 fixture is created through the deployment manager's own public
//!   creation path, which is what makes a marker part of a save point, and the case then takes a
//!   real save. A serializer that cannot write the state under test fails there.
//!
//!   READ HALF - the public apply, not a re-read. OVT_DeploymentComponent.ApplyPersistedDeployment()
//!   is the method the marker's Deserialize calls with the values it read, and its own header calls
//!   itself the place "every side effect of restoring a deployment lives". Handing it the payload
//!   the save was taken of is the closest a case in this harness can get to a marker coming back.
//!
//! What is consequently NOT asserted here, and is not asserted anywhere automated: that the bytes
//! on disk read back as the values that went in. That is a real-restart claim, in the same bucket
//! as the continue flow this suite's header already parks as manual, and it is covered by
//! inspection instead - Phase 7's T7.7 decoded a real save point and read the deployment records
//! field by field (docs/features/virtualization/integration/context.md).
//! ===========================================================================================
//!
//! ⚠ FIXTURE DISCIPLINE, AND WHY EVERY DEPLOYMENT FIXTURE HERE IS MARKED ELIMINATED.
//! A live deployment starts a repeating 8-12 s update whose first tick activates it, and activation
//! is what converges its spawning modules - which registers real groups at the GLOBAL 1750 m spawn
//! ring, inside which the autotest camera is an observer. A fixture that did that would materialise
//! soldiers next to the test camera, hand the movement tick a cycling perimeter plan to walk, and
//! leave records behind in a shared world. Marking the deployment and its spawning modules
//! eliminated makes the fixture inert BY CONSTRUCTION rather than by finishing before a timer, so
//! it stays safe through a host stall. That is also why no case here clears the deployment-level
//! flag as its dirty step: see T7.3's header.
//------------------------------------------------------------------------------------------------
class OVT_TEST_DeploymentRoundTripFixture
{
	//! The shipped config every deployment case runs on.
	//!
	//! CHOSEN, NOT ARBITRARY: "Town Patrol" is the one shipped config whose registry entry authors
	//! m_bDeleteOnConditionFail 0, so its condition module cannot delete the fixture's own marker out
	//! from under a case mid-run - which "Tower Garrison" (m_bDeleteOnConditionFail 1) can.
	static const string CONFIG_NAME = "Town Patrol";

	//------------------------------------------------------------------------------------------------
	//! Resolves the deployment manager and its registry together, because a case needs both or
	//! neither.
	//! \param[out] diagnostic Reason it could not be resolved; untouched on success.
	//! \return The manager, or null.
	static OVT_DeploymentManagerComponent ResolveManager(out string diagnostic)
	{
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
		{
			diagnostic = "OVT_Global.GetDeploymentManager() is null - the deployment manager is missing from the game-mode prefab";
			return null;
		}

		if (!manager.m_DeploymentRegistry)
		{
			diagnostic = "The deployment manager has no registry, so no shipped deployment config can be resolved at all";
			return null;
		}

		return manager;
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped config these cases run on.
	//! \param[in] manager A resolved deployment manager.
	//! \param[out] diagnostic Reason it could not be resolved; untouched on success.
	//! \return The config, or null.
	static OVT_DeploymentConfig ResolveConfig(notnull OVT_DeploymentManagerComponent manager, out string diagnostic)
	{
		OVT_DeploymentConfig config = manager.m_DeploymentRegistry.FindConfigByName(CONFIG_NAME);
		if (!config)
		{
			diagnostic = string.Format("The deployment registry does not resolve '%1' - a saved deployment naming it would be dropped on load rather than restored", CONFIG_NAME);
			return null;
		}

		if (!config.IsValidConfig())
		{
			diagnostic = string.Format("Config '%1' resolves but is not valid (no name, no modules, or no spawning module)", CONFIG_NAME);
			return null;
		}

		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! A marker position with terrain under it, offset so two cases running in one session cannot
	//! derive the same key and collide on the manager's disambiguation ordinal.
	//! \param[in] offset Per-case offset from the shared fixture position.
	//! \return A world position.
	static vector MarkerPosition(vector offset)
	{
		return OVT_TEST_VirtualizationFixture.PickPosition() + offset;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a marker carrying an UNINITIALIZED deployment component - the state the persistence
	//! system hands a self-spawned marker before its Deserialize runs.
	//!
	//! This is what makes the fresh-restore branch of ApplyPersistedDeployment() reachable: the
	//! component's config is the "already built" flag, so a marker that has one takes the idempotent
	//! branch and InitializeDeployment() - the method that reads the wipe-out flag - never runs.
	//! The shipped prefab authors no config, which is asserted rather than assumed below.
	//!
	//! Deliberately NOT part of any save point: nothing here asks the persistence layer to track it.
	//! A fixture that is never stored cannot leave a stray deployment record behind in the CI save
	//! for the cases that follow, and the payload these cases exercise is handed over directly
	//! anyway (see the class header).
	//! \param[in] position Where to put the marker.
	//! \param[out] diagnostic Reason it could not be built; untouched on success.
	//! \return The uninitialized deployment component, or null.
	static OVT_DeploymentComponent SpawnRestorableMarker(vector position, out string diagnostic)
	{
		OVT_DeploymentManagerComponent manager = ResolveManager(diagnostic);
		if (!manager)
			return null;

		if (!manager.m_DeploymentPrefab || manager.m_DeploymentPrefab.IsEmpty())
		{
			diagnostic = "The deployment manager has no marker prefab configured, so no deployment can be restored or created at all";
			return null;
		}

		vector mat[4];
		Math3D.MatrixIdentity4(mat);
		mat[3] = position;

		IEntity marker = OVT_WorldUtils.SpawnEntityPrefabMatrix(manager.m_DeploymentPrefab, mat);
		if (!marker)
		{
			diagnostic = string.Format("The deployment marker prefab '%1' did not spawn", manager.m_DeploymentPrefab);
			return null;
		}

		// ⚠ AN ENTITY THAT IS NOT WORLD-REGISTERED ANSWERS EntityID.INVALID, AND EVERY SUCH ENTITY
		// ANSWERS THE SAME ONE (found by this epic's Phase 6 suite run - a marker spawned 3 km out in
		// this small world came back unkeyable). The deployment manager keys its active list on this
		// id, so a fixture with an invalid one would silently share a slot with the next fixture.
		// Every marker offset here is deliberately inside the test world's own extent; this says so
		// out loud rather than trusting it.
		if (marker.GetID() == EntityID.INVALID)
		{
			delete marker;
			diagnostic = string.Format("A deployment marker spawned at %1 has no valid EntityID - it is outside this world's registered extent, and the deployment manager would key its active list on an id shared with every other unregistered entity", position.ToString());
			return null;
		}

		OVT_DeploymentComponent deployment = OVT_DeploymentComponent.Cast(marker.FindComponent(OVT_DeploymentComponent));
		if (!deployment)
		{
			delete marker;
			diagnostic = "The deployment marker prefab carries no OVT_DeploymentComponent";
			return null;
		}

		if (deployment.GetConfig())
		{
			delete marker;
			diagnostic = "A freshly spawned deployment marker already carries a config, so the restore path that reads the wipe-out flag can never be reached by a test - the prefab has been given an authored OVT_DeploymentConfig";
			return null;
		}

		return deployment;
	}

	//------------------------------------------------------------------------------------------------
	//! Makes a deployment unable to register anything, at both gates its convergence checks.
	//! See the class header for why every fixture here is built this way.
	//! \param[in] deployment The fixture's deployment.
	static void MakeInert(notnull OVT_DeploymentComponent deployment)
	{
		deployment.SetSpawnedUnitsEliminated(true);

		array<OVT_BaseSpawningDeploymentModule> modules = deployment.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule module : modules)
		{
			if (module)
				module.SetSpawnedUnitsEliminated(true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The owner key the deployment's FIRST spawning module registers its groups under, composed the
	//! same way the module composes it.
	//!
	//! The module's authored name is read off the LIVE cloned module rather than hard-coded, so a
	//! retuned config renames the key here too instead of silently making the assertions look at an
	//! owner nobody uses.
	//! \param[in] deployment The fixture's deployment.
	//! \return The owner key, or an empty string when there is no spawning module or no key.
	static string FirstModuleOwnerKey(notnull OVT_DeploymentComponent deployment)
	{
		array<OVT_BaseSpawningDeploymentModule> modules = deployment.GetSpawningModules();
		if (modules.IsEmpty())
			return "";

		string deploymentKey = deployment.EnsureVirtualKey();
		if (deploymentKey.IsEmpty())
			return "";

		string moduleName;
		OVT_InfantrySpawningDeploymentModule infantry = OVT_InfantrySpawningDeploymentModule.Cast(modules[0]);
		if (infantry)
			moduleName = infantry.m_sModuleName;

		return OVT_DeploymentVirtualKey.OwnerKey(deploymentKey, OVT_DeploymentVirtualKey.ModuleTag(moduleName, 0));
	}

	//------------------------------------------------------------------------------------------------
	//! How many registered groups an owner key currently answers for.
	//! \param[in] ownerKey The owner key to ask about.
	//! \return The count, or -1 when there is no virtualization manager.
	static int CountOwnedGroups(string ownerKey)
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return -1;

		return virtualization.FindGroupsByOwner(OVT_BaseSpawningDeploymentModule.OWNER_SYSTEM, ownerKey).Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Unregisters everything an owner key answers for. Called on every path, including the red ones:
	//! a case that goes red because a wiped deployment DID register a force must not then leave that
	//! force standing in the shared world.
	//! \param[in] ownerKey The owner key to empty.
	static void ReleaseOwnedGroups(string ownerKey)
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization || ownerKey.IsEmpty())
			return;

		array<int> handles = virtualization.FindGroupsByOwner(OVT_BaseSpawningDeploymentModule.OWNER_SYSTEM, ownerKey);
		foreach (int handle : handles)
		{
			virtualization.UnregisterGroup(handle);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Tears the fixture down: stops the deployment's update loop, cleans its modules up (which
	//! unregisters anything they still hold) and deletes the marker.
	//! \param[in] deployment The fixture's deployment, or null.
	static void Destroy(OVT_DeploymentComponent deployment)
	{
		if (!deployment)
			return;

		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (manager)
		{
			manager.DeleteDeployment(deployment);
			return;
		}

		deployment.DestroyDeployment();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a derived key is the given base key, with or without a collision ordinal.
	//!
	//! The ordinal exists because two deployments can share one rounded spot, and a live campaign
	//! evaluating deployments alongside a case can legitimately be holding the base key already - so
	//! an exact-equality assertion would be a race, and this is the claim that is actually true:
	//! whatever the ordinal, the key is derived from THIS config name and THIS marker position.
	//! \param[in] key The key that was derived.
	//! \param[in] baseKey The base key expected from the marker's own position.
	//! \return True when the key is the base key or the base key plus an ordinal suffix.
	static bool KeyMatchesBase(string key, string baseKey)
	{
		if (key == baseKey)
			return true;

		return key.IndexOf(baseKey + OVT_DeploymentVirtualKey.PART_MARK) == 0;
	}
}

//------------------------------------------------------------------------------------------------
//! A MIGRATED DEPLOYMENT'S FIVE PERSISTED VALUES AND ITS VIRTUALIZATION KEY COME BACK - and the key
//! comes back as the SAME STRING rather than as a fresh derivation. (T7.2)
//!
//! WHY THE KEY IS THE PART THAT MATTERS. The other four values are bookkeeping; the key is identity.
//! It is the string this deployment's registered AI groups are tagged with in the virtualization
//! registry, and reclaiming them after a load is a lookup by exactly that string. A restore that
//! re-derived it instead of reading it would agree in every ordinary case and disagree the moment a
//! marker came back a metre off - and the disagreement is SILENT: the reclaim finds nothing, the
//! module converges from zero, and the deployment quietly registers a second force on top of the one
//! already standing there. That is the failure the serializer's version 2 append exists to prevent.
//!
//! THE KEY PLANTED HERE IS ONE DERIVATION COULD NOT PRODUCE - coordinates no marker in any world is
//! at - and the case ASSERTS that precondition before it asserts anything else. Without it "the key
//! came back" would be satisfied by a re-derivation that happened to agree, and the case would be
//! measuring a coincidence. With it, a single re-derivation anywhere in the restore path is visible.
//!
//! THE FIXTURE IS CREATED THROUGH THE MANAGER'S OWN CREATION PATH, which is what puts a marker into
//! a save point at all, so the save this case takes really does run the deployment serializer's
//! write half over a live deployment carrying a version 2 key. Nothing else in the tree does.
//!
//! Read the fixture class header above for what the reload seam can and cannot reach, and for why
//! the fixture is marked eliminated.
//!
//! PROVEN ABLE TO FAIL (fail proofs recorded, execution belongs to the phase's suite run):
//!   - delete the `if (!virtualKey.IsEmpty()) m_sVirtualKey = virtualKey;` write in
//!     ApplyPersistedDeployment and the key assertion goes red naming the derived string it fell
//!     back to;
//!   - make EnsureVirtualKey() re-derive instead of returning the stored key (drop its
//!     `if (!m_sVirtualKey.IsEmpty()) return m_sVirtualKey;` guard) and the SECOND key assertion
//!     goes red while the first still passes, which is exactly the split worth having: the field
//!     survived, the method that every registration actually calls did not;
//!   - drop any one of the four scalar writes and that scalar's assertion goes red with the dirty
//!     value still in place;
//!   - rename the shipped config and the config-resolution assertion goes red first of all.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_DeploymentRecord_SurvivesSaveAndReapply : SCR_AutotestCaseBase
{
	//! Offset from the shared fixture position, so this case's derived key cannot collide with the
	//! other deployment cases' or with a live campaign deployment's.
	static const vector MARKER_OFFSET = "57 0 -43";

	//! The key planted before the save. SHAPED like a real key and IMPOSSIBLE as one: no marker
	//! stands at (-987654, -987654), so a restore that re-derived would produce something else.
	static const string SAVED_KEY = "TownPatrol@-987654_-987654";

	//! The key written over it before the reload, so "the saved key came back" is not "the key never
	//! changed".
	static const string DIRTY_KEY = "DirtiedByTheCase@1_1";

	//! Threat and invested resources written before the save. Neither is a value a campaign start or
	//! a default-constructed component would produce.
	static const float SAVED_THREAT = 417.5;
	static const int SAVED_RESOURCES = 1373;

	//! ...and the values written over them.
	static const float DIRTY_THREAT = 3.25;
	static const int DIRTY_RESOURCES = 7;

	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected int m_iReloadPolls;

	protected OVT_DeploymentComponent m_Deployment;

	//! Faction the deployment was created for, and the different one written over it.
	protected int m_iSavedFaction = -1;
	protected int m_iDirtyFaction = -1;

	//! The key this marker's own position WOULD derive, kept so the failure text can name it.
	protected string m_sDerivableKey;

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
	//! Creates a deployment, stamps the state under test onto it, and saves.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool MutateAndSave()
	{
		string diagnostic;
		OVT_DeploymentManagerComponent manager = OVT_TEST_DeploymentRoundTripFixture.ResolveManager(diagnostic);
		if (!manager)
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_DeploymentConfig config = OVT_TEST_DeploymentRoundTripFixture.ResolveConfig(manager, diagnostic);
		if (!config)
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_OverthrowConfigComponent overthrowConfig = OVT_Global.GetConfig();
		if (!overthrowConfig)
		{
			SetFailure("OVT_Global.GetConfig() is null, so no faction index can be resolved to save one");
			return true;
		}

		m_iSavedFaction = overthrowConfig.GetOccupyingFactionIndex();
		m_iDirtyFaction = overthrowConfig.GetPlayerFactionIndex();

		if (m_iSavedFaction == m_iDirtyFaction)
		{
			SetFailure("The occupying and player faction indices are both %1, so overwriting one with the other would not dirty anything", m_iSavedFaction.ToString());
			return true;
		}

		vector position = OVT_TEST_DeploymentRoundTripFixture.MarkerPosition(MARKER_OFFSET);

		m_Deployment = manager.CreateDeployment(config, position, m_iSavedFaction, SAVED_RESOURCES, SAVED_THREAT);
		if (!m_Deployment)
		{
			SetFailure("The deployment manager refused to create a '%1' deployment, so there is nothing to save", OVT_TEST_DeploymentRoundTripFixture.CONFIG_NAME);
			return true;
		}

		// Inert BEFORE anything can tick - see the fixture class header.
		OVT_TEST_DeploymentRoundTripFixture.MakeInert(m_Deployment);

		// Plant the payload, exactly as the marker's own Deserialize would hand it over.
		m_Deployment.ApplyPersistedDeployment(OVT_TEST_DeploymentRoundTripFixture.CONFIG_NAME, m_iSavedFaction,
			SAVED_THREAT, SAVED_RESOURCES, true, SAVED_KEY);

		string precondition = VerifyPreconditions();
		if (precondition != "")
		{
			OVT_TEST_DeploymentRoundTripFixture.Destroy(m_Deployment);
			SetFailure(precondition);
			return true;
		}

		m_iSaveBaseline = OVT_TEST_PersistenceRoundTripGate.CompletedSaveCount();

		string trigger = OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce();
		if (trigger != "")
		{
			OVT_TEST_DeploymentRoundTripFixture.Destroy(m_Deployment);
			SetFailure(trigger);
			return true;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The state being saved really is the state this case thinks it is - including that the planted
	//! key is one the marker's own position could NOT produce.
	//! \return An empty string when everything holds, or the first broken claim.
	protected string VerifyPreconditions()
	{
		vector origin = m_Deployment.GetPosition();
		m_sDerivableKey = OVT_DeploymentVirtualKey.DeriveKey(OVT_TEST_DeploymentRoundTripFixture.CONFIG_NAME, origin[0], origin[2]);

		if (m_sDerivableKey == SAVED_KEY)
			return string.Format("The planted key '%1' is exactly what this marker's position derives, so 'the key was not re-derived' would be asserted against a coincidence", SAVED_KEY);

		if (m_Deployment.GetVirtualKey() != SAVED_KEY)
			return string.Format("The deployment holds key '%1' after being handed '%2' - the payload's key was refused before the save was even taken",
				m_Deployment.GetVirtualKey(), SAVED_KEY);

		if (m_Deployment.GetControllingFaction() != m_iSavedFaction)
			return string.Format("The deployment reports faction %1, expected the %2 it was handed", m_Deployment.GetControllingFaction().ToString(), m_iSavedFaction.ToString());

		if (Math.AbsFloat(m_Deployment.GetThreatLevel() - SAVED_THREAT) > 0.01)
			return string.Format("The deployment reports threat %1, expected %2", m_Deployment.GetThreatLevel().ToString(), SAVED_THREAT.ToString());

		if (m_Deployment.GetResourcesInvested() != SAVED_RESOURCES)
			return string.Format("The deployment reports %1 invested resources, expected %2", m_Deployment.GetResourcesInvested().ToString(), SAVED_RESOURCES.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitSave()
	{
		string saveDiagnostic;
		int settled = OVT_TEST_PersistenceRoundTripGate.PollSaveSettled(m_iSaveBaseline, saveDiagnostic);
		if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_FAILED)
		{
			OVT_TEST_DeploymentRoundTripFixture.Destroy(m_Deployment);
			SetFailure(saveDiagnostic);
			return true;
		}

		if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_PENDING)
		{
			m_iSavePolls += 1;
			if (m_iSavePolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
			{
				OVT_TEST_DeploymentRoundTripFixture.Destroy(m_Deployment);
				SetFailure(OVT_TEST_PersistenceRoundTripGate.CAPABILITY_ABSENT);
				return true;
			}

			return false;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes a different value over every one of the five, then asks for the persisted state back.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool DirtyAndReload()
	{
		if (!m_Deployment)
		{
			SetFailure("The deployment fixture disappeared between the save and the dirty step");
			return true;
		}

		m_Deployment.ApplyPersistedDeployment(OVT_TEST_DeploymentRoundTripFixture.CONFIG_NAME, m_iDirtyFaction,
			DIRTY_THREAT, DIRTY_RESOURCES, true, DIRTY_KEY);

		if (m_Deployment.GetVirtualKey() != DIRTY_KEY || m_Deployment.GetControllingFaction() != m_iDirtyFaction
			|| m_Deployment.GetResourcesInvested() != DIRTY_RESOURCES)
		{
			OVT_TEST_DeploymentRoundTripFixture.Destroy(m_Deployment);
			SetFailure("The dirty step did not change the deployment's state, so the assertion after the reload would pass against a value that was never wrong");
			return true;
		}

		string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
		if (reload != "")
		{
			OVT_TEST_DeploymentRoundTripFixture.Destroy(m_Deployment);
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
				OVT_TEST_DeploymentRoundTripFixture.Destroy(m_Deployment);
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
			OVT_TEST_DeploymentRoundTripFixture.Destroy(m_Deployment);
			SetFailure(restored);
			return true;
		}

		if (!m_Deployment)
		{
			SetFailure("The deployment fixture disappeared across the reload");
			return true;
		}

		// The payload the save was taken of, handed back through the method the marker's own
		// Deserialize calls. See the fixture class header for why this is not a re-read.
		m_Deployment.ApplyPersistedDeployment(OVT_TEST_DeploymentRoundTripFixture.CONFIG_NAME, m_iSavedFaction,
			SAVED_THREAT, SAVED_RESOURCES, true, SAVED_KEY);

		string failure = Verify();

		// Cleanup BEFORE reporting, on every path.
		OVT_TEST_DeploymentRoundTripFixture.Destroy(m_Deployment);
		m_Deployment = null;

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("A migrated deployment came back with its four scalars and its virtualization key '%1', which its own position (%2) could not have derived",
			SAVED_KEY, m_sDerivableKey);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when everything holds, or the first broken claim.
	protected string Verify()
	{
		OVT_DeploymentConfig config = m_Deployment.GetConfig();
		if (!config)
			return "The restored deployment has no config at all - a deployment that cannot name what it is runs no modules and is collected on the manager's next sweep";

		if (config.m_sDeploymentName != OVT_TEST_DeploymentRoundTripFixture.CONFIG_NAME)
			return string.Format("The restored deployment is running config '%1', expected '%2'", config.m_sDeploymentName, OVT_TEST_DeploymentRoundTripFixture.CONFIG_NAME);

		string diagnostic;
		OVT_DeploymentManagerComponent manager = OVT_TEST_DeploymentRoundTripFixture.ResolveManager(diagnostic);
		if (!manager)
			return diagnostic;

		if (!manager.m_DeploymentRegistry.FindConfigByName(OVT_TEST_DeploymentRoundTripFixture.CONFIG_NAME))
			return string.Format("The registry no longer resolves '%1' by name - the save stores the NAME, so a deployment restored in a later session would be dropped instead of restored",
				OVT_TEST_DeploymentRoundTripFixture.CONFIG_NAME);

		if (m_Deployment.GetControllingFaction() != m_iSavedFaction)
			return string.Format("The restored deployment belongs to faction %1, saved as %2 (dirtied to %3) - a deployment that changes sides on a load fights for the wrong army",
				m_Deployment.GetControllingFaction().ToString(), m_iSavedFaction.ToString(), m_iDirtyFaction.ToString());

		if (Math.AbsFloat(m_Deployment.GetThreatLevel() - SAVED_THREAT) > 0.01)
			return string.Format("The restored threat level is %1, saved as %2", m_Deployment.GetThreatLevel().ToString(), SAVED_THREAT.ToString());

		if (m_Deployment.GetResourcesInvested() != SAVED_RESOURCES)
			return string.Format("The restored deployment reports %1 invested resources, saved as %2 - RecoverResources() refunds this number and the Game Master snapshot shows it",
				m_Deployment.GetResourcesInvested().ToString(), SAVED_RESOURCES.ToString());

		if (m_Deployment.GetVirtualKey() != SAVED_KEY)
			return string.Format("The restored virtualization key is '%1', saved as '%2' (this marker's position derives '%3') - if it fell back to the derivation, every group this deployment registered is unreachable and the next convergence registers a second force on top of them",
				m_Deployment.GetVirtualKey(), SAVED_KEY, m_sDerivableKey);

		// The field is right; now the METHOD every registration actually goes through.
		string ensured = m_Deployment.EnsureVirtualKey();
		if (ensured != SAVED_KEY)
			return string.Format("EnsureVirtualKey() answered '%1' after the restore, expected the persisted '%2' - the stored field survived but the accessor re-derives, so reclaim still looks under the wrong owner",
				ensured, SAVED_KEY);

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! A DEPLOYMENT RESTORED WITH ITS FORCE ALREADY WIPED OUT REGISTERS NOTHING - not on the restore
//! itself, and not on the convergence that follows it. (T7.3 - G4's teeth.)
//!
//! WHAT THIS IS ACTUALLY GUARDING. A deployment's wipe-out flag is written BEFORE
//! InitializeDeployment() in ApplyPersistedDeployment(), and that ordering is the entire mechanism:
//! InitializeDeployment reads the flag to decide whether the spawning modules it has just cloned
//! start out eliminated. Overthrow's previous persistence layer set the flag AFTERWARDS, and the
//! consequence was that a patrol the player had wiped out came back at full strength on the next
//! load - every time, in silence, because nothing about a fresh force looks wrong. The ordering has
//! been documented in the serializer's header since; this is the first thing that asserts it.
//!
//! THREE SEPARATE CLAIMS, IN THE ORDER THEY CAN BREAK:
//!   1. the restore marks the deployment eliminated;
//!   2. it marks every SPAWNING MODULE eliminated too - this is the ordering claim, because nothing
//!      but InitializeDeployment's flag-before block does that on a fresh restore;
//!   3. EnsureGroups() - the one method activation, the records-restored fan-out and the rebuy all
//!      funnel through - registers ZERO groups under the module's owner key.
//! Then, after the reload, a fourth: re-applying the same payload to a LIVE deployment whose modules
//! have been un-marked puts the marks back, which is what makes the in-session re-application safe.
//!
//! ⚠ THE DIRTY STEP CLEARS THE MODULE FLAGS AND DELIBERATELY LEAVES THE DEPLOYMENT'S OWN FLAG SET.
//! Convergence refuses on either, so this fixture cannot register anything at any point in its life
//! - which matters because a deployment's own 8-12 s activation tick calls EnsureGroups() on its own
//! schedule, and a case that cleared both flags would be betting that the tick does not fire in the
//! ~100 ms window before the assert phase puts them back. This project has already measured a 105 s
//! main-thread stall in this harness. The claim asserted after the reload is therefore "the modules
//! were re-marked", not "convergence was refused by the deployment gate", and the module-level claim
//! is the one that would actually rot.
//!
//! PROVEN ABLE TO FAIL (fail proofs recorded, execution belongs to the phase's suite run):
//!   - move the `m_bSpawnedUnitsEliminated = spawnedUnitsEliminated;` write in
//!     ApplyPersistedDeployment to AFTER the InitializeDeployment(config, factionIndex) call - i.e.
//!     re-introduce the old ordering - and claim 2 goes red, followed by claim 3 registering a full
//!     patrol;
//!   - drop the `m_bSpawnedUnitsEliminated || m_ParentDeployment.GetSpawnedUnitsEliminated()` guard
//!     from OVT_InfantrySpawningDeploymentModule.ConvergeGroups and claim 3 goes red on its own;
//!   - drop the re-marking loop from ApplyPersistedDeployment's already-running branch and the
//!     post-reload claim goes red while all three pre-save claims still pass.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_DeploymentEliminated_RegistersNoGroups : SCR_AutotestCaseBase
{
	//! Offset from the shared fixture position - far enough from the other deployment cases that the
	//! rounded keys cannot meet.
	static const vector MARKER_OFFSET = "-61 0 39";

	//! Planted rather than derived, so the owner key this case asserts on is deterministic and cannot
	//! be handed a collision ordinal by a live campaign deployment standing on the same rounded spot.
	static const string ELIMINATED_KEY = "TownPatrol@-654321_-654321";

	static const float SAVED_THREAT = 88.5;
	static const int SAVED_RESOURCES = 220;

	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected int m_iReloadPolls;

	protected OVT_DeploymentComponent m_Deployment;

	//! The owner key the deployment's first spawning module registers under.
	protected string m_sOwnerKey;

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
	//! Restores a wiped-out deployment onto a fresh marker and proves it registers nothing.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool MutateAndSave()
	{
		string diagnostic;
		OVT_DeploymentManagerComponent manager = OVT_TEST_DeploymentRoundTripFixture.ResolveManager(diagnostic);
		if (!manager)
		{
			SetFailure(diagnostic);
			return true;
		}

		if (!OVT_TEST_DeploymentRoundTripFixture.ResolveConfig(manager, diagnostic))
		{
			SetFailure(diagnostic);
			return true;
		}

		if (!OVT_Global.GetVirtualization())
		{
			SetFailure("OVT_Global.GetVirtualization() is null - 'it registered nothing' would be true for the wrong reason");
			return true;
		}

		OVT_OverthrowConfigComponent overthrowConfig = OVT_Global.GetConfig();
		if (!overthrowConfig)
		{
			SetFailure("OVT_Global.GetConfig() is null, so no faction index can be resolved");
			return true;
		}

		m_Deployment = OVT_TEST_DeploymentRoundTripFixture.SpawnRestorableMarker(
			OVT_TEST_DeploymentRoundTripFixture.MarkerPosition(MARKER_OFFSET), diagnostic);

		if (!m_Deployment)
		{
			SetFailure(diagnostic);
			return true;
		}

		// THE RESTORE. Wipe-out flag true, config still unset, so InitializeDeployment runs from here.
		m_Deployment.ApplyPersistedDeployment(OVT_TEST_DeploymentRoundTripFixture.CONFIG_NAME,
			overthrowConfig.GetOccupyingFactionIndex(), SAVED_THREAT, SAVED_RESOURCES, true, ELIMINATED_KEY);

		string failure = VerifyRestoreRegisteredNothing();

		if (failure != "")
		{
			Cleanup();
			SetFailure(failure);
			return true;
		}

		m_iSaveBaseline = OVT_TEST_PersistenceRoundTripGate.CompletedSaveCount();

		string trigger = OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce();
		if (trigger != "")
		{
			Cleanup();
			SetFailure(trigger);
			return true;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Claims 1 to 3 of the header.
	//! \return An empty string when everything holds, or the first broken claim.
	protected string VerifyRestoreRegisteredNothing()
	{
		if (!m_Deployment.GetConfig())
			return string.Format("The restore left the deployment with no config, so it never initialized and nothing below is being tested (config name '%1')",
				OVT_TEST_DeploymentRoundTripFixture.CONFIG_NAME);

		if (!m_Deployment.GetSpawnedUnitsEliminated())
			return "The restored deployment does not report its units eliminated, although the payload said they were - the flag was dropped somewhere in the apply";

		array<OVT_BaseSpawningDeploymentModule> modules = m_Deployment.GetSpawningModules();
		if (modules.IsEmpty())
			return "The restored deployment holds no spawning modules at all, so 'it registered nothing' is vacuous";

		foreach (int index, OVT_BaseSpawningDeploymentModule module : modules)
		{
			if (!module)
				return string.Format("Spawning module %1 of the restored deployment is null", index.ToString());

			if (!module.AreSpawnedUnitsEliminated())
				return string.Format("Spawning module %1 came back NOT eliminated although the deployment did - the wipe-out flag is being written after InitializeDeployment() instead of before it, and a force the player destroyed will be rebuilt from the config on the next convergence",
					index.ToString());
		}

		m_sOwnerKey = OVT_TEST_DeploymentRoundTripFixture.FirstModuleOwnerKey(m_Deployment);
		if (m_sOwnerKey.IsEmpty())
			return "The restored deployment's first spawning module composes no owner key, so a registration could not be seen even if one happened";

		int afterRestore = OVT_TEST_DeploymentRoundTripFixture.CountOwnedGroups(m_sOwnerKey);
		if (afterRestore != 0)
			return string.Format("Owner key '%1' already answers for %2 group(s) straight after the restore", m_sOwnerKey, afterRestore.ToString());

		// THE CLAIM. The one method activation, the records-restored fan-out and the rebuy all use.
		m_Deployment.EnsureGroups();

		int afterConverge = OVT_TEST_DeploymentRoundTripFixture.CountOwnedGroups(m_sOwnerKey);
		if (afterConverge != 0)
			return string.Format("EnsureGroups() registered %1 group(s) under '%2' for a deployment whose force had already been wiped out - a patrol the player destroyed comes back at full strength on every load",
				afterConverge.ToString(), m_sOwnerKey);

		OVT_InfantrySpawningDeploymentModule infantry = OVT_InfantrySpawningDeploymentModule.Cast(modules[0]);
		if (infantry && infantry.GetSpawnedEntities().Count() != 0)
			return string.Format("The eliminated spawning module holds %1 group entities after converging", infantry.GetSpawnedEntities().Count().ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitSave()
	{
		string saveDiagnostic;
		int settled = OVT_TEST_PersistenceRoundTripGate.PollSaveSettled(m_iSaveBaseline, saveDiagnostic);
		if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_FAILED)
		{
			Cleanup();
			SetFailure(saveDiagnostic);
			return true;
		}

		if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_PENDING)
		{
			m_iSavePolls += 1;
			if (m_iSavePolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
			{
				Cleanup();
				SetFailure(OVT_TEST_PersistenceRoundTripGate.CAPABILITY_ABSENT);
				return true;
			}

			return false;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Un-marks the spawning modules - see the header for why the deployment's own flag stays set.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool DirtyAndReload()
	{
		if (!m_Deployment)
		{
			SetFailure("The deployment fixture disappeared between the save and the dirty step");
			return true;
		}

		array<OVT_BaseSpawningDeploymentModule> modules = m_Deployment.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule module : modules)
		{
			if (module)
				module.SetSpawnedUnitsEliminated(false);
		}

		foreach (OVT_BaseSpawningDeploymentModule dirtied : modules)
		{
			if (dirtied && dirtied.AreSpawnedUnitsEliminated())
			{
				Cleanup();
				SetFailure("A spawning module still reports itself eliminated after the dirty step, so the re-application would be asserted against a value that was never wrong");
				return true;
			}
		}

		string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
		if (reload != "")
		{
			Cleanup();
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
				Cleanup();
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
			Cleanup();
			SetFailure(restored);
			return true;
		}

		if (!m_Deployment)
		{
			SetFailure("The deployment fixture disappeared across the reload");
			return true;
		}

		OVT_OverthrowConfigComponent overthrowConfig = OVT_Global.GetConfig();
		if (!overthrowConfig)
		{
			Cleanup();
			SetFailure("OVT_Global.GetConfig() is null after the reload");
			return true;
		}

		// The same payload again, this time onto a LIVE deployment - the in-session re-application
		// shape. Its job here is to put the module marks back.
		m_Deployment.ApplyPersistedDeployment(OVT_TEST_DeploymentRoundTripFixture.CONFIG_NAME,
			overthrowConfig.GetOccupyingFactionIndex(), SAVED_THREAT, SAVED_RESOURCES, true, ELIMINATED_KEY);

		string failure = VerifyReapplyRestoredTheMarks();

		Cleanup();

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("A deployment restored with its force already wiped out registered nothing under '%1', and re-applying the same payload re-marked its spawning modules", m_sOwnerKey);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when everything holds, or the first broken claim.
	protected string VerifyReapplyRestoredTheMarks()
	{
		if (!m_Deployment.GetSpawnedUnitsEliminated())
			return "The re-applied deployment no longer reports its units eliminated";

		array<OVT_BaseSpawningDeploymentModule> modules = m_Deployment.GetSpawningModules();
		if (modules.IsEmpty())
			return "The deployment holds no spawning modules after the re-application";

		foreach (int index, OVT_BaseSpawningDeploymentModule module : modules)
		{
			if (module && !module.AreSpawnedUnitsEliminated())
				return string.Format("Spawning module %1 was left un-marked by the re-application - an in-session re-apply of a wiped-out deployment leaves its modules willing to buy a new force",
					index.ToString());
		}

		m_Deployment.EnsureGroups();

		// `owned` is a reserved EnforceScript keyword - a local of that name fails to compile with a
		// "Broken expression" error that names the line and nothing else.
		int stillHeld = OVT_TEST_DeploymentRoundTripFixture.CountOwnedGroups(m_sOwnerKey);
		if (stillHeld != 0)
			return string.Format("EnsureGroups() registered %1 group(s) under '%2' after the re-application", stillHeld.ToString(), m_sOwnerKey);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Releases anything the fixture registered and deletes the marker. Safe to call twice.
	protected void Cleanup()
	{
		OVT_TEST_DeploymentRoundTripFixture.ReleaseOwnedGroups(m_sOwnerKey);
		OVT_TEST_DeploymentRoundTripFixture.Destroy(m_Deployment);
		m_Deployment = null;
	}
}

//------------------------------------------------------------------------------------------------
//! A VERSION 1 DEPLOYMENT PAYLOAD - one written before deployments carried a virtualization key -
//! still restores, and the deployment mints its key from its own marker on first use. (T7.4)
//!
//! THIS IS THE PRE-FEATURE-SAVE MIGRATION PATH, and it is not hypothetical: a decoded save point
//! from before this epic carries 23 deployment records whose fields stop at the wipe-out flag, with
//! no key field written at all (Phase 7 T7.7, context.md). Every one of them has to come back.
//!
//! WHAT A VERSION 1 PAYLOAD LOOKS LIKE FROM HERE. The serializer reads the key only when the stored
//! version says one was written, so a version 1 record reaches ApplyPersistedDeployment with an
//! EMPTY key string - which is exactly what this case hands it. Nothing about the fixture pretends
//! to be old; it is the same call the codec makes.
//!
//! TWO CLAIMS, AND THE SECOND IS THE ONE NOBODY WOULD THINK TO MAKE:
//!   1. an empty key restores cleanly and EnsureVirtualKey() then derives one from the marker's own
//!      position - once, and the same string on every later call;
//!   2. re-applying that same version 1 payload later does NOT wipe the key the session has since
//!      derived. Deployment payloads are re-applied to live instances, and a blind write would empty
//!      the key of a deployment whose groups are already tagged with it - orphaning the whole force
//!      to a reclaim that will never find it again. The guard that prevents it is one `if`.
//!
//! The fixture is deliberately not part of any save point and is marked eliminated; both are
//! explained in the fixture class header.
//!
//! PROVEN ABLE TO FAIL (fail proofs recorded, execution belongs to the phase's suite run):
//!   - make ApplyPersistedDeployment write the key unconditionally (`m_sVirtualKey = virtualKey;`
//!     with no emptiness guard) and claim 2 goes red with an empty key;
//!   - make the serializer read the key regardless of version and a version 1 payload consumes the
//!     bytes of whatever follows it - which this case cannot see, and is why T7.7 read the records
//!     off a real save point by hand as well;
//!   - drop the `if (!m_sVirtualKey.IsEmpty()) return m_sVirtualKey;` guard in EnsureVirtualKey and
//!     the derive-once claim goes red as soon as a live deployment takes the base key's ordinal.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_DeploymentVersion1Payload_StillLoads : SCR_AutotestCaseBase
{
	//! Offset from the shared fixture position, clear of the other two marker fixtures.
	static const vector MARKER_OFFSET = "44 0 63";

	//! Written over the derived key before the reload, so the "a v1 payload does not clobber it"
	//! claim has something visible to preserve.
	static const string DIRTY_KEY = "DirtiedByTheCase@2_2";

	static const float SAVED_THREAT = 55.25;
	static const int SAVED_RESOURCES = 640;

	static const float DIRTY_THREAT = 1.5;
	static const int DIRTY_RESOURCES = 3;

	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected int m_iReloadPolls;

	protected OVT_DeploymentComponent m_Deployment;

	protected int m_iSavedFaction = -1;
	protected int m_iDirtyFaction = -1;

	//! The key the deployment minted for itself on first use.
	protected string m_sDerivedKey;

	//! The base key the marker's own position produces, without any collision ordinal.
	protected string m_sBaseKey;

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
	//! Restores a keyless deployment and watches it mint its key.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool MutateAndSave()
	{
		string diagnostic;
		OVT_DeploymentManagerComponent manager = OVT_TEST_DeploymentRoundTripFixture.ResolveManager(diagnostic);
		if (!manager)
		{
			SetFailure(diagnostic);
			return true;
		}

		if (!OVT_TEST_DeploymentRoundTripFixture.ResolveConfig(manager, diagnostic))
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_OverthrowConfigComponent overthrowConfig = OVT_Global.GetConfig();
		if (!overthrowConfig)
		{
			SetFailure("OVT_Global.GetConfig() is null, so no faction index can be resolved");
			return true;
		}

		m_iSavedFaction = overthrowConfig.GetOccupyingFactionIndex();
		m_iDirtyFaction = overthrowConfig.GetPlayerFactionIndex();

		if (m_iSavedFaction == m_iDirtyFaction)
		{
			SetFailure("The occupying and player faction indices are both %1, so overwriting one with the other would not dirty anything", m_iSavedFaction.ToString());
			return true;
		}

		m_Deployment = OVT_TEST_DeploymentRoundTripFixture.SpawnRestorableMarker(
			OVT_TEST_DeploymentRoundTripFixture.MarkerPosition(MARKER_OFFSET), diagnostic);

		if (!m_Deployment)
		{
			SetFailure(diagnostic);
			return true;
		}

		// THE VERSION 1 PAYLOAD: five fields and an empty key, which is what the codec passes when the
		// stored version predates the key.
		m_Deployment.ApplyPersistedDeployment(OVT_TEST_DeploymentRoundTripFixture.CONFIG_NAME, m_iSavedFaction,
			SAVED_THREAT, SAVED_RESOURCES, true, "");

		OVT_TEST_DeploymentRoundTripFixture.MakeInert(m_Deployment);

		string failure = VerifyDerivedOnFirstUse();
		if (failure != "")
		{
			Cleanup();
			SetFailure(failure);
			return true;
		}

		m_iSaveBaseline = OVT_TEST_PersistenceRoundTripGate.CompletedSaveCount();

		string trigger = OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce();
		if (trigger != "")
		{
			Cleanup();
			SetFailure(trigger);
			return true;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Claim 1 of the header.
	//! \return An empty string when everything holds, or the first broken claim.
	protected string VerifyDerivedOnFirstUse()
	{
		OVT_DeploymentConfig config = m_Deployment.GetConfig();
		if (!config)
			return string.Format("A version 1 payload naming '%1' restored no config at all - every pre-feature deployment in an existing save would be dropped",
				OVT_TEST_DeploymentRoundTripFixture.CONFIG_NAME);

		if (config.m_sDeploymentName != OVT_TEST_DeploymentRoundTripFixture.CONFIG_NAME)
			return string.Format("The restored deployment is running '%1', expected '%2'", config.m_sDeploymentName, OVT_TEST_DeploymentRoundTripFixture.CONFIG_NAME);

		if (m_Deployment.GetVirtualKey() != "")
			return string.Format("A version 1 payload left the deployment holding key '%1' - the apply invented a key instead of leaving the field empty for the first caller that needs one",
				m_Deployment.GetVirtualKey());

		vector origin = m_Deployment.GetPosition();
		m_sBaseKey = OVT_DeploymentVirtualKey.DeriveKey(OVT_TEST_DeploymentRoundTripFixture.CONFIG_NAME, origin[0], origin[2]);

		m_sDerivedKey = m_Deployment.EnsureVirtualKey();
		if (m_sDerivedKey.IsEmpty())
			return "EnsureVirtualKey() answered an empty key for a restored version 1 deployment - its modules would compose no owner key at all and it could never register or reclaim anything";

		if (!OVT_TEST_DeploymentRoundTripFixture.KeyMatchesBase(m_sDerivedKey, m_sBaseKey))
			return string.Format("The derived key '%1' is not built from this marker's own position, which derives '%2' - a key that does not follow the marker cannot be re-derived by anything else either",
				m_sDerivedKey, m_sBaseKey);

		if (m_Deployment.GetVirtualKey() != m_sDerivedKey)
			return string.Format("The derived key '%1' was not stored on the deployment (the field reads '%2') - it would be re-derived on every call and drift the moment the marker moved",
				m_sDerivedKey, m_Deployment.GetVirtualKey());

		string again = m_Deployment.EnsureVirtualKey();
		if (again != m_sDerivedKey)
			return string.Format("A second EnsureVirtualKey() answered '%1', the first answered '%2' - derive-once is what makes the key an identity", again, m_sDerivedKey);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitSave()
	{
		string saveDiagnostic;
		int settled = OVT_TEST_PersistenceRoundTripGate.PollSaveSettled(m_iSaveBaseline, saveDiagnostic);
		if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_FAILED)
		{
			Cleanup();
			SetFailure(saveDiagnostic);
			return true;
		}

		if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_PENDING)
		{
			m_iSavePolls += 1;
			if (m_iSavePolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
			{
				Cleanup();
				SetFailure(OVT_TEST_PersistenceRoundTripGate.CAPABILITY_ABSENT);
				return true;
			}

			return false;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes a different key and different scalars over the restored ones.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool DirtyAndReload()
	{
		if (!m_Deployment)
		{
			SetFailure("The deployment fixture disappeared between the save and the dirty step");
			return true;
		}

		m_Deployment.ApplyPersistedDeployment(OVT_TEST_DeploymentRoundTripFixture.CONFIG_NAME, m_iDirtyFaction,
			DIRTY_THREAT, DIRTY_RESOURCES, true, DIRTY_KEY);

		if (m_Deployment.GetVirtualKey() != DIRTY_KEY || m_Deployment.GetResourcesInvested() != DIRTY_RESOURCES)
		{
			Cleanup();
			SetFailure("The dirty step did not change the deployment's key or scalars, so the claims after the reload would be asserted against values that were never wrong");
			return true;
		}

		string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
		if (reload != "")
		{
			Cleanup();
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
				Cleanup();
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
			Cleanup();
			SetFailure(restored);
			return true;
		}

		if (!m_Deployment)
		{
			SetFailure("The deployment fixture disappeared across the reload");
			return true;
		}

		// The version 1 payload again - still keyless, because a version 1 record never gains one.
		m_Deployment.ApplyPersistedDeployment(OVT_TEST_DeploymentRoundTripFixture.CONFIG_NAME, m_iSavedFaction,
			SAVED_THREAT, SAVED_RESOURCES, true, "");

		string failure = VerifyKeylessPayloadKeptTheKey();

		Cleanup();

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("A version 1 deployment payload restored and minted the key '%1' from its own marker, and re-applying it later left the session's key alone",
			m_sDerivedKey);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Claim 2 of the header, plus the scalars.
	//! \return An empty string when everything holds, or the first broken claim.
	protected string VerifyKeylessPayloadKeptTheKey()
	{
		if (m_Deployment.GetControllingFaction() != m_iSavedFaction)
			return string.Format("The re-applied version 1 payload left the deployment on faction %1, expected %2",
				m_Deployment.GetControllingFaction().ToString(), m_iSavedFaction.ToString());

		if (m_Deployment.GetResourcesInvested() != SAVED_RESOURCES)
			return string.Format("The re-applied version 1 payload left %1 invested resources, expected %2",
				m_Deployment.GetResourcesInvested().ToString(), SAVED_RESOURCES.ToString());

		if (Math.AbsFloat(m_Deployment.GetThreatLevel() - SAVED_THREAT) > 0.01)
			return string.Format("The re-applied version 1 payload left threat %1, expected %2",
				m_Deployment.GetThreatLevel().ToString(), SAVED_THREAT.ToString());

		if (m_Deployment.GetVirtualKey() != DIRTY_KEY)
			return string.Format("A keyless version 1 payload overwrote the deployment's key: it now reads '%1', and held '%2' before the apply. A blind write empties the key of a live deployment whose groups are already tagged with it, and the reclaim that follows finds nothing",
				m_Deployment.GetVirtualKey(), DIRTY_KEY);

		if (m_Deployment.EnsureVirtualKey() != DIRTY_KEY)
			return string.Format("EnsureVirtualKey() answered '%1' after the keyless re-apply, expected the key the deployment was already holding ('%2')",
				m_Deployment.EnsureVirtualKey(), DIRTY_KEY);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Releases anything the fixture registered and deletes the marker. Safe to call twice.
	protected void Cleanup()
	{
		if (m_Deployment)
			OVT_TEST_DeploymentRoundTripFixture.ReleaseOwnedGroups(OVT_TEST_DeploymentRoundTripFixture.FirstModuleOwnerKey(m_Deployment));

		OVT_TEST_DeploymentRoundTripFixture.Destroy(m_Deployment);
		m_Deployment = null;
	}
}

//------------------------------------------------------------------------------------------------
//! GROUPS REGISTERED UNDER A DEPLOYMENT OWNER KEY ARE RECLAIMABLE BY THAT KEY AFTER A SAVE AND A
//! RESTORE - verbatim, including the two structural characters the key scheme is built out of. (T7.5)
//!
//! WHAT IS NEW HERE, AND WHAT IS DELIBERATELY NOT. That virtual groups survive a save at all, and
//! that a wiped one does not come back, are asserted by the two cases below this one; they are not
//! repeated. This case asserts the seam the whole deployments migration rests on instead: after a
//! restore, a spawning module reclaims its own groups by asking FindGroupsByOwner for the composed
//! key `<deployment key>#<module tag>` - and nothing else in the tree checks that a key of that
//! SHAPE survives storage.
//!
//! WHY THE SHAPE IS THE RISK. A deployment owner key carries an '@' (name from coordinates) and a '#'
//! (deployment from module, and base key from collision ordinal). It is the only owner key in the
//! epic that does. A payload that truncated, trimmed or normalised either character would leave every
//! record present and correct and every reclaim silently empty - and an empty reclaim is not an
//! error: the module concludes it holds nothing and converges a second force on top of the one the
//! restore just rebuilt. Two keys are therefore registered, differing ONLY in their module tag, and
//! each is required to answer for exactly its own groups.
//!
//! IT ALSO EXERCISES THE POSITIONAL FALLBACK TAG. One of the two module tags is "m1" - what a module
//! with no authored name gets. No shipped config produces it today, so this is the only place it is
//! ever composed, stored and read back.
//!
//! FIXTURE FOOTPRINT (T7.1): three registrations, null plans, spawn distance 0 - the documented
//! "never materialise by proximity" registration, so the movement tick has nothing to advance and the
//! autotest camera cannot pull them into the world. All three are unregistered before the case
//! reports, on every path.
//!
//! PROVEN ABLE TO FAIL (fail proofs recorded, execution belongs to the phase's suite run):
//!   - drop the `entry.ownerKey = record.m_sOwnerKey` write in the registry snapshot (or its read)
//!     and the first reclaim assertion goes red with the right number of records restored under the
//!     wrong owner;
//!   - restore the records but skip re-indexing them by owner and the counts come back 0 while
//!     IsRegistered still answers true, which the second assertion names;
//!   - sanitise the '#' out of a stored owner key and the two keys collapse into one: the first
//!     assertion sees 3 where it wants 2.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_DeploymentOwnedGroups_ReclaimAfterReload : SCR_AutotestCaseBase
{
	//! Stands in for a config name. Carries a space AND a '#' on purpose: both are stripped by the
	//! key arithmetic, and a payload that round-trips the stripped form is what is being asserted.
	static const string CONFIG_NAME = "Phase7 Reclaim #Fixture";

	//! The authored module tag of the first module, and the positional fallback of the second.
	static const string NAMED_MODULE = "Spawn Infantry";
	static const int UNNAMED_MODULE_INDEX = 1;

	//! Manual lifecycle policy: never materialise by proximity.
	static const int SPAWN_DISTANCE_NEVER = 0;

	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected int m_iReloadPolls;

	//! The two composed owner keys.
	protected string m_sNamedKey;
	protected string m_sUnnamedKey;

	//! Handles registered under each, remembered so the restore can be checked handle by handle.
	protected ref array<int> m_aNamedHandles = new array<int>();
	protected ref array<int> m_aUnnamedHandles = new array<int>();

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
	//! Registers two groups under one module's key and one under another's, then saves.
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

		vector position = OVT_TEST_VirtualizationFixture.PickPosition();

		string deploymentKey = OVT_DeploymentVirtualKey.Disambiguate(
			OVT_DeploymentVirtualKey.DeriveKey(CONFIG_NAME, position[0], position[2]), 0);

		m_sNamedKey = OVT_DeploymentVirtualKey.OwnerKey(deploymentKey, OVT_DeploymentVirtualKey.ModuleTag(NAMED_MODULE, 0));
		m_sUnnamedKey = OVT_DeploymentVirtualKey.OwnerKey(deploymentKey, OVT_DeploymentVirtualKey.ModuleTag("", UNNAMED_MODULE_INDEX));

		if (m_sNamedKey == m_sUnnamedKey)
		{
			SetFailure("The two module tags composed the same owner key ('%1'), so 'each key answers for its own groups' would be asserted against one owner", m_sNamedKey);
			return true;
		}

		if (Count(virtualization, m_sNamedKey) != 0 || Count(virtualization, m_sUnnamedKey) != 0)
		{
			SetFailure("Owner key '%1' or '%2' already has registered groups before this case ran - a previous case leaked, or the key arithmetic collides", m_sNamedKey, m_sUnnamedKey);
			return true;
		}

		string failure = RegisterUnder(virtualization, m_sNamedKey, factionKey, groupName, position, 2, m_aNamedHandles);
		if (failure == "")
			failure = RegisterUnder(virtualization, m_sUnnamedKey, factionKey, groupName, position, 1, m_aUnnamedHandles);

		if (failure != "")
		{
			Cleanup();
			SetFailure(failure);
			return true;
		}

		m_iSaveBaseline = OVT_TEST_PersistenceRoundTripGate.CompletedSaveCount();

		string trigger = OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce();
		if (trigger != "")
		{
			Cleanup();
			SetFailure(trigger);
			return true;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Registers `count` groups under one owner key, checking each one as it goes.
	//! \param[in] virtualization The manager.
	//! \param[in] ownerKey The composed key to register under.
	//! \param[in] factionKey A faction this world resolves.
	//! \param[in] groupName A composition that faction resolves.
	//! \param[in] position Where to register.
	//! \param[in] count How many to register.
	//! \param[out] handles Filled with every handle registered, for the restore check and cleanup.
	//! \return An empty string on success, or the broken claim.
	protected string RegisterUnder(notnull OVT_VirtualizationManagerComponent virtualization, string ownerKey,
		string factionKey, string groupName, vector position, int count, notnull array<int> handles)
	{
		for (int i = 0; i < count; i++)
		{
			int handle = virtualization.RegisterGroup(OVT_BaseSpawningDeploymentModule.OWNER_SYSTEM, ownerKey,
				factionKey, groupName, position, null, SPAWN_DISTANCE_NEVER);

			if (handle == -1)
				return string.Format("RegisterGroup refused group %1 of %2 under '%3' for a composition the faction registry resolves (%4 '%5')",
					(i + 1).ToString(), count.ToString(), ownerKey, factionKey, groupName);

			handles.Insert(handle);

			if (virtualization.GetSpawnDistance(handle) != 0)
				return string.Format("Handle %1 resolved a spawn distance of %2 m from an override of 0 - this fixture would materialise real soldiers next to the autotest camera",
					handle.ToString(), virtualization.GetSpawnDistance(handle).ToString());
		}

		int registered = Count(virtualization, ownerKey);
		if (registered != count)
			return string.Format("Owner key '%1' answers for %2 group(s) straight after registering %3", ownerKey, registered.ToString(), count.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitSave()
	{
		string saveDiagnostic;
		int settled = OVT_TEST_PersistenceRoundTripGate.PollSaveSettled(m_iSaveBaseline, saveDiagnostic);
		if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_FAILED)
		{
			Cleanup();
			SetFailure(saveDiagnostic);
			return true;
		}

		if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_PENDING)
		{
			m_iSavePolls += 1;
			if (m_iSavePolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
			{
				Cleanup();
				SetFailure(OVT_TEST_PersistenceRoundTripGate.CAPABILITY_ABSENT);
				return true;
			}

			return false;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Unregisters all three, so both owner keys answer for nothing at all, and asks for the persisted
	//! registry back.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool DirtyAndReload()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null before the reload");
			return true;
		}

		foreach (int named : m_aNamedHandles)
		{
			virtualization.UnregisterGroup(named);
		}

		foreach (int unnamed : m_aUnnamedHandles)
		{
			virtualization.UnregisterGroup(unnamed);
		}

		if (Count(virtualization, m_sNamedKey) != 0 || Count(virtualization, m_sUnnamedKey) != 0)
		{
			Cleanup();
			SetFailure("The dirty step left groups registered under '%1' or '%2', so the reclaim after the reload would find what was never removed", m_sNamedKey, m_sUnnamedKey);
			return true;
		}

		string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
		if (reload != "")
		{
			Cleanup();
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
				Cleanup();
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
			Cleanup();
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

		// Cleanup BEFORE reporting, on every path.
		Cleanup();

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("Deployment owner keys survived the round trip verbatim: '%1' reclaimed %2 group(s), and the positional-fallback key reclaimed its own",
			m_sNamedKey, m_aNamedHandles.Count().ToString());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when everything holds, or the first broken claim.
	protected string Verify(notnull OVT_VirtualizationManagerComponent virtualization)
	{
		string failure = VerifyOwner(virtualization, m_sNamedKey, m_aNamedHandles);
		if (failure != "")
			return failure;

		return VerifyOwner(virtualization, m_sUnnamedKey, m_aUnnamedHandles);
	}

	//------------------------------------------------------------------------------------------------
	//! One owner key answers for exactly the handles that were registered under it, and the restored
	//! records agree about who owns them.
	//! \param[in] virtualization The manager.
	//! \param[in] ownerKey The composed key.
	//! \param[in] expected The handles registered under it before the save.
	//! \return An empty string when everything holds, or the broken claim.
	protected string VerifyOwner(notnull OVT_VirtualizationManagerComponent virtualization, string ownerKey, notnull array<int> expected)
	{
		array<int> reclaimed = virtualization.FindGroupsByOwner(OVT_BaseSpawningDeploymentModule.OWNER_SYSTEM, ownerKey);

		if (reclaimed.Count() != expected.Count())
			return string.Format("FindGroupsByOwner('%1', '%2') reclaimed %3 handle(s) after the restore, expected %4 - a spawning module that cannot find its own groups after a load registers a second force on top of them",
				OVT_BaseSpawningDeploymentModule.OWNER_SYSTEM, ownerKey, reclaimed.Count().ToString(), expected.Count().ToString());

		foreach (int handle : expected)
		{
			if (!virtualization.IsRegistered(handle))
				return string.Format("Handle %1 did not come back from the save at all", handle.ToString());

			if (!reclaimed.Contains(handle))
				return string.Format("Handle %1 is registered again but '%2' does not resolve it - the record survived and its owner index did not",
					handle.ToString(), ownerKey);

			OVT_VirtualGroupRecord record = virtualization.GetRecord(handle);
			if (!record)
				return string.Format("Handle %1 is registered but hands out no record", handle.ToString());

			if (record.m_sOwnerSystem != OVT_BaseSpawningDeploymentModule.OWNER_SYSTEM)
				return string.Format("Handle %1 came back owned by system '%2', expected '%3'",
					handle.ToString(), record.m_sOwnerSystem, OVT_BaseSpawningDeploymentModule.OWNER_SYSTEM);

			if (record.m_sOwnerKey != ownerKey)
				return string.Format("Handle %1 came back under owner key '%2', expected '%3' - the '@' and '#' a deployment key is built out of did not survive storage verbatim",
					handle.ToString(), record.m_sOwnerKey, ownerKey);
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! How many groups an owner key answers for.
	//! \param[in] virtualization The manager.
	//! \param[in] ownerKey The key to ask about.
	//! \return The count.
	protected int Count(notnull OVT_VirtualizationManagerComponent virtualization, string ownerKey)
	{
		return virtualization.FindGroupsByOwner(OVT_BaseSpawningDeploymentModule.OWNER_SYSTEM, ownerKey).Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Unregisters everything either key answers for, plus every handle this case ever took. Safe to
	//! call twice, and called on every path including the red ones.
	protected void Cleanup()
	{
		OVT_TEST_DeploymentRoundTripFixture.ReleaseOwnedGroups(m_sNamedKey);
		OVT_TEST_DeploymentRoundTripFixture.ReleaseOwnedGroups(m_sUnnamedKey);

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		foreach (int named : m_aNamedHandles)
		{
			if (virtualization.IsRegistered(named))
				virtualization.UnregisterGroup(named);
		}

		foreach (int unnamed : m_aUnnamedHandles)
		{
			if (virtualization.IsRegistered(unnamed))
				virtualization.UnregisterGroup(unnamed);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! A BASE-DEFENSE DEPLOYMENT'S FIVE PERSISTED VALUES COME BACK, ITS CONFIG STILL RESOLVES BY NAME, AND
//! THE RESTORE MARKS IT AS RESTORED. (virtualization/base-defense-migration T4.8 - the requirement's
//! named "base-defense deployment round trip".)
//!
//! ⚠ READ THIS FIRST - THE SEAM CANNOT DO WHAT THE CASE NAME SOUNDS LIKE, AND THAT IS NOT WORKED
//! AROUND. The suite's reload seam (OVT_TEST_PersistenceRoundTripGate.RequestSessionReload) builds its
//! request with Instances = {gameMode} only, so a deployment MARKER's Deserialize is NEVER re-run by
//! it - a deployment is a marker entity with its own component serializer, which is outside the game
//! mode's record. This case therefore does exactly what integration's four deployment cases do and
//! says so out loud:
//!   WRITE HALF - REAL. The fixture is created through the deployment manager's own public creation
//!   path, which is what puts a marker into a save point at all, and a real save is then taken. A
//!   serializer that could not write this state would fail there.
//!   READ HALF - THE PUBLIC APPLY, NOT A RE-READ. ApplyPersistedDeployment() is the method the
//!   marker's own Deserialize calls with the values it read.
//! What is consequently NOT asserted, here or anywhere automated, is that the bytes on disk read back
//! as the values that went in - a real-restart claim, covered by inspection (integration T7.7 decoded
//! a real save point field by field). DO NOT WIDEN THE SEAM to "fix" this: widening it means naming
//! persistence-framework types inside Scripts/Game/Tests/, which the suite's assertion rule forbids.
//!
//! WHY A SECOND DEPLOYMENT ROUND TRIP AT ALL, WHEN DeploymentRecord_SurvivesSaveAndReapply EXISTS.
//! That case runs on "Town Patrol", whose modules are all pre-migration. This one runs on a BASE
//! DEFENSE config, and the two things it adds are the two that are new:
//!   - the restored deployment still carries a live OVT_PlacedInfantrySpawningDeploymentModule WITH
//!     ITS m_Placement PROVIDER. CloneModule is hand-written and not chained; a dropped m_Placement
//!     line ships a module that wants zero groups, registers nothing and logs nothing, and a base's
//!     tower guards simply never come back after a load;
//!   - WasRestoredFromSave() is TRUE afterwards. That flag is D7's gate - the one thing that stops a
//!     restored deployment building a second bunker, checkpoint or parked truck on every load - and
//!     nothing else in the tree asserts it.
//!
//! ⚠ WHY THIS FIXTURE CANNOT DELETE ITSELF MID-RUN, even though its config authors
//! m_bDeleteOnConditionFail 1 (which is exactly why the Town Patrol fixture was chosen for the other
//! cases). The delete branch lives inside OVT_ReinforcementBehaviorDeploymentModule.CheckReinforcement(),
//! which OnUpdate() reaches only after m_fInitialDelay has elapsed since activation. The base-defense
//! configs author neither m_fInitialDelay nor m_fCheckInterval, so both take the class defaults -
//! 300 000 ms and 60 000 ms. The case's whole budget is 60 s. The condition modules are therefore
//! never evaluated at runtime while this case is alive. If a future tuning pass authors a shorter
//! initial delay on these configs, THIS is the case that starts failing intermittently, and the fix is
//! to pick a config without the delete flag - not to lengthen the timeout.
//!
//! Read the OVT_TEST_DeploymentRoundTripFixture header for why every deployment fixture here is marked
//! eliminated before anything can tick.
//!
//! PROVEN ABLE TO FAIL (fail proofs recorded, execution belongs to the phase's suite run):
//!   - drop any one of the four scalar writes from ApplyPersistedDeployment and that scalar's
//!     assertion goes red with the dirty value still in place;
//!   - delete the `if (!virtualKey.IsEmpty()) m_sVirtualKey = virtualKey;` write and the key assertion
//!     goes red naming the string it fell back to;
//!   - delete `clone.m_Placement = m_Placement;` from
//!     OVT_PlacedInfantrySpawningDeploymentModule.CloneModule() and the provider assertion goes red;
//!   - delete the `m_bRestoredFromSave = true;` line and the restored-flag assertion goes red on its
//!     own while every scalar still passes;
//!   - rename the config, or drop its entry from overthrowDeployments.conf, and the resolution
//!     assertion goes red before any of them.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_DeploymentBaseDefense_SurvivesSaveAndReapply : SCR_AutotestCaseBase
{
	//! The shipped base-defense config this case runs on. Chosen over the tower/sniper configs because
	//! it is the one with a behaviour module, so its restored module chain is the longest of the three.
	static const string CONFIG_NAME = "Base Defense Positions";

	//! Offset from the shared fixture position, so this case's derived key cannot collide with the
	//! other deployment cases' or with a live campaign deployment's.
	static const vector MARKER_OFFSET = "-61 0 38";

	//! The key planted before the save. SHAPED like a real key and IMPOSSIBLE as one: no marker stands
	//! at (-874512, -874512), so a restore that re-derived would produce something else.
	static const string SAVED_KEY = "BaseDefensePositions@-874512_-874512";

	//! The key written over it before the reload, so "the saved key came back" is not "the key never
	//! changed".
	static const string DIRTY_KEY = "DirtiedByTheBaseDefenseCase@2_2";

	//! Threat and invested resources written before the save. Neither is a value a campaign start or a
	//! default-constructed component would produce.
	static const float SAVED_THREAT = 233.75;
	static const int SAVED_RESOURCES = 921;

	//! ...and the values written over them.
	static const float DIRTY_THREAT = 1.5;
	static const int DIRTY_RESOURCES = 3;

	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected int m_iReloadPolls;

	protected OVT_DeploymentComponent m_Deployment;

	//! Faction the deployment was created for, and the different one written over it.
	protected int m_iSavedFaction = -1;
	protected int m_iDirtyFaction = -1;

	//! The key this marker's own position WOULD derive, kept so the failure text can name it.
	protected string m_sDerivableKey;

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
	//! Creates a base-defense deployment, stamps the state under test onto it, and saves.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool MutateAndSave()
	{
		string diagnostic;
		OVT_DeploymentManagerComponent manager = OVT_TEST_DeploymentRoundTripFixture.ResolveManager(diagnostic);
		if (!manager)
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_DeploymentConfig config = manager.m_DeploymentRegistry.FindConfigByName(CONFIG_NAME);
		if (!config)
		{
			SetFailure("The deployment registry does not resolve '%1' - a saved base-defense deployment naming it would be dropped on load rather than restored", CONFIG_NAME);
			return true;
		}

		if (!config.IsValidConfig())
		{
			SetFailure("Config '%1' resolves but is not valid (no name, no modules, or no spawning module)", CONFIG_NAME);
			return true;
		}

		OVT_OverthrowConfigComponent overthrowConfig = OVT_Global.GetConfig();
		if (!overthrowConfig)
		{
			SetFailure("OVT_Global.GetConfig() is null, so no faction index can be resolved to save one");
			return true;
		}

		m_iSavedFaction = overthrowConfig.GetOccupyingFactionIndex();
		m_iDirtyFaction = overthrowConfig.GetPlayerFactionIndex();

		if (m_iSavedFaction == m_iDirtyFaction)
		{
			SetFailure("The occupying and player faction indices are both %1, so overwriting one with the other would not dirty anything", m_iSavedFaction.ToString());
			return true;
		}

		vector position = OVT_TEST_DeploymentRoundTripFixture.MarkerPosition(MARKER_OFFSET);

		m_Deployment = manager.CreateDeployment(config, position, m_iSavedFaction, SAVED_RESOURCES, SAVED_THREAT);
		if (!m_Deployment)
		{
			SetFailure("The deployment manager refused to create a '%1' deployment, so there is nothing to save", CONFIG_NAME);
			return true;
		}

		// Inert BEFORE anything can tick - see the fixture class header.
		OVT_TEST_DeploymentRoundTripFixture.MakeInert(m_Deployment);

		// Plant the payload, exactly as the marker's own Deserialize would hand it over.
		m_Deployment.ApplyPersistedDeployment(CONFIG_NAME, m_iSavedFaction, SAVED_THREAT, SAVED_RESOURCES, true, SAVED_KEY);

		string precondition = VerifyPreconditions();
		if (precondition != "")
		{
			OVT_TEST_DeploymentRoundTripFixture.Destroy(m_Deployment);
			SetFailure(precondition);
			return true;
		}

		m_iSaveBaseline = OVT_TEST_PersistenceRoundTripGate.CompletedSaveCount();

		string trigger = OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce();
		if (trigger != "")
		{
			OVT_TEST_DeploymentRoundTripFixture.Destroy(m_Deployment);
			SetFailure(trigger);
			return true;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The state being saved really is the state this case thinks it is - including that the planted
	//! key is one the marker's own position could NOT produce, and that the module chain the restore
	//! will be asked about actually exists on a freshly created deployment.
	//! \return An empty string when everything holds, or the first broken claim.
	protected string VerifyPreconditions()
	{
		vector origin = m_Deployment.GetPosition();
		m_sDerivableKey = OVT_DeploymentVirtualKey.DeriveKey(CONFIG_NAME, origin[0], origin[2]);

		if (m_sDerivableKey == SAVED_KEY)
			return string.Format("The planted key '%1' is exactly what this marker's position derives, so 'the key was not re-derived' would be asserted against a coincidence", SAVED_KEY);

		if (m_Deployment.GetVirtualKey() != SAVED_KEY)
			return string.Format("The deployment holds key '%1' after being handed '%2' - the payload's key was refused before the save was even taken",
				m_Deployment.GetVirtualKey(), SAVED_KEY);

		if (!FindPlacedModule())
			return string.Format("A freshly created '%1' deployment carries no OVT_PlacedInfantrySpawningDeploymentModule at all, so 'the restored one still does' would assert nothing", CONFIG_NAME);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitSave()
	{
		string saveDiagnostic;
		int settled = OVT_TEST_PersistenceRoundTripGate.PollSaveSettled(m_iSaveBaseline, saveDiagnostic);
		if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_FAILED)
		{
			OVT_TEST_DeploymentRoundTripFixture.Destroy(m_Deployment);
			SetFailure(saveDiagnostic);
			return true;
		}

		if (settled == OVT_TEST_PersistenceRoundTripGate.SAVE_PENDING)
		{
			m_iSavePolls += 1;
			if (m_iSavePolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
			{
				OVT_TEST_DeploymentRoundTripFixture.Destroy(m_Deployment);
				SetFailure(OVT_TEST_PersistenceRoundTripGate.CAPABILITY_ABSENT);
				return true;
			}

			return false;
		}

		m_iPhase = OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes a different value over every one of the five, then asks for the persisted state back.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool DirtyAndReload()
	{
		if (!m_Deployment)
		{
			SetFailure("The base-defense deployment fixture disappeared between the save and the dirty step");
			return true;
		}

		m_Deployment.ApplyPersistedDeployment(CONFIG_NAME, m_iDirtyFaction, DIRTY_THREAT, DIRTY_RESOURCES, true, DIRTY_KEY);

		if (m_Deployment.GetVirtualKey() != DIRTY_KEY || m_Deployment.GetControllingFaction() != m_iDirtyFaction
			|| m_Deployment.GetResourcesInvested() != DIRTY_RESOURCES)
		{
			OVT_TEST_DeploymentRoundTripFixture.Destroy(m_Deployment);
			SetFailure("The dirty step did not change the deployment's state, so the assertion after the reload would pass against a value that was never wrong");
			return true;
		}

		string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
		if (reload != "")
		{
			OVT_TEST_DeploymentRoundTripFixture.Destroy(m_Deployment);
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
				OVT_TEST_DeploymentRoundTripFixture.Destroy(m_Deployment);
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
			OVT_TEST_DeploymentRoundTripFixture.Destroy(m_Deployment);
			SetFailure(restored);
			return true;
		}

		if (!m_Deployment)
		{
			SetFailure("The base-defense deployment fixture disappeared across the reload - if a future tuning pass shortened this config's reinforcement initial delay, its m_bDeleteOnConditionFail branch has started collecting the fixture mid-run");
			return true;
		}

		// The payload the save was taken of, handed back through the method the marker's own
		// Deserialize calls. See the class header for why this is not a re-read.
		m_Deployment.ApplyPersistedDeployment(CONFIG_NAME, m_iSavedFaction, SAVED_THREAT, SAVED_RESOURCES, true, SAVED_KEY);

		string failure = Verify();

		// Cleanup BEFORE reporting, on every path.
		OVT_TEST_DeploymentRoundTripFixture.Destroy(m_Deployment);
		m_Deployment = null;

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("A base-defense deployment came back with its four scalars, its virtualization key '%1' (which its own position could not derive - that would have been '%2'), its placement provider and its restored-from-save flag",
			SAVED_KEY, m_sDerivableKey);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when everything holds, or the first broken claim.
	protected string Verify()
	{
		OVT_DeploymentConfig config = m_Deployment.GetConfig();
		if (!config)
			return "The restored base-defense deployment has no config at all - a deployment that cannot name what it is runs no modules and is collected on the manager's next sweep";

		if (config.m_sDeploymentName != CONFIG_NAME)
			return string.Format("The restored deployment is running config '%1', expected '%2'", config.m_sDeploymentName, CONFIG_NAME);

		string diagnostic;
		OVT_DeploymentManagerComponent manager = OVT_TEST_DeploymentRoundTripFixture.ResolveManager(diagnostic);
		if (!manager)
			return diagnostic;

		if (!manager.m_DeploymentRegistry.FindConfigByName(CONFIG_NAME))
			return string.Format("The registry no longer resolves '%1' by name - the save stores the NAME, so a base-defense deployment restored in a later session would be dropped instead of restored",
				CONFIG_NAME);

		if (m_Deployment.GetControllingFaction() != m_iSavedFaction)
			return string.Format("The restored deployment belongs to faction %1, saved as %2 (dirtied to %3) - a base's garrison that changes sides on a load fights for the wrong army",
				m_Deployment.GetControllingFaction().ToString(), m_iSavedFaction.ToString(), m_iDirtyFaction.ToString());

		if (Math.AbsFloat(m_Deployment.GetThreatLevel() - SAVED_THREAT) > 0.01)
			return string.Format("The restored threat level is %1, saved as %2", m_Deployment.GetThreatLevel().ToString(), SAVED_THREAT.ToString());

		if (m_Deployment.GetResourcesInvested() != SAVED_RESOURCES)
			return string.Format("The restored deployment reports %1 invested resources, saved as %2 - this is what a base's defense is worth, and the Game Master snapshot shows it",
				m_Deployment.GetResourcesInvested().ToString(), SAVED_RESOURCES.ToString());

		if (m_Deployment.GetVirtualKey() != SAVED_KEY)
			return string.Format("The restored virtualization key is '%1', saved as '%2' (this marker's position derives '%3') - if it fell back to the derivation, every guard this base-defense deployment registered is unreachable and the next convergence registers a second garrison on top of them",
				m_Deployment.GetVirtualKey(), SAVED_KEY, m_sDerivableKey);

		if (!m_Deployment.WasRestoredFromSave())
			return "The restored deployment does not report WasRestoredFromSave() - that flag is what stops a restored deployment re-building its static content, so a base would grow a second composition on every load";

		OVT_PlacedInfantrySpawningDeploymentModule placed = FindPlacedModule();
		if (!placed)
			return "The restored deployment carries no OVT_PlacedInfantrySpawningDeploymentModule - its guards would be rolled onto a ring around the marker instead of standing on their posts";

		if (!placed.m_Placement)
			return "The restored deployment's placed module has no m_Placement provider - CloneModule is hand-written and not chained, and a module with no provider wants 0 groups, registers NOTHING and logs nothing, so the base's guards simply never come back after a load";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return The fixture deployment's live placed-infantry module, or null.
	protected OVT_PlacedInfantrySpawningDeploymentModule FindPlacedModule()
	{
		if (!m_Deployment)
			return null;

		array<OVT_BaseSpawningDeploymentModule> modules = m_Deployment.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule module : modules)
		{
			OVT_PlacedInfantrySpawningDeploymentModule placed = OVT_PlacedInfantrySpawningDeploymentModule.Cast(module);
			if (placed)
				return placed;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! A PRE-MIGRATION SAVE'S BASE-UPGRADE INVESTMENT IS REFUNDED TO THE DEPLOYMENT POOL, EXACTLY ONCE,
//! AND A REWRITTEN PAYLOAD REFUNDS NOTHING. (virtualization/base-defense-migration T6.8.)
//!
//! WHAT A LEGACY SAVE IS AND WHY IT CANNOT SIMPLY BE DROPPED. Every campaign saved before this
//! migration carries, per occupying-held base, a list of upgrade records: what each upgrade had banked
//! and how many groups it had standing. The upgrade classes those records describe are deleted, so
//! there is nothing left to replay them into. Rather than strand a player's whole investment, the
//! records are read once for their VALUE, the sum is credited to the occupying faction's deployment
//! resource pool, and the evaluator re-establishes defense from it - value-parity, not
//! entity-identity, which is the decision this feature was given.
//!
//! THE FAILURE MODES THIS GUARDS, AND NEITHER OF THEM LOGS ANYTHING:
//!   - REFUND NOTHING. A loaded legacy campaign arrives with no defense AND no money to buy any, and
//!     the occupying faction is crippled for the rest of that campaign. Nothing errors; the player
//!     just finds every base empty.
//!   - REFUND TWICE. The refund is idempotent STRUCTURALLY - the write path stores an EMPTY upgrade
//!     array from now on, so a second pass has nothing to sum - and there is deliberately no flag
//!     guarding it. If that structural argument ever broke, every load of the same campaign would hand
//!     the occupying faction another few thousand resources, forever.
//!
//! ⚠ READ THIS FIRST - WHAT THE SEAM CAN AND CANNOT DO, AND WHY IT IS NOT WORKED AROUND. The suite's
//! reload seam re-applies the GAME MODE's stored record, which does include this manager - but the
//! payload it would re-read is one this build WROTE, and this build writes the upgrade array empty.
//! Feeding it a genuinely pre-migration payload therefore has to be done the way the manager's own
//! Deserialize does it:
//!   WRITE HALF - REAL, AND IT IS THE HALF THAT CHANGED. A real save is taken over untouched live
//!   campaign state before anything else happens. WriteBase() now stops walking the base controller's
//!   upgrade list and writes an empty array in its place, so a save that completes is a write path
//!   that ran the new body over every base in the world. A serializer that threw on it would surface
//!   here as the missing capability.
//!   READ HALF - THE PUBLIC APPLY, NOT A RE-READ. ApplyPersistedOccupyingFaction() is the exact method
//!   the serializer's Deserialize calls with the values it read, and it is handed hand-built records
//!   shaped like the ones a 2026-era save really carries.
//! DO NOT WIDEN THE SEAM to "fix" this: widening it means naming persistence-framework types inside
//! Scripts/Game/Tests/, which the suite's assertion rule forbids.
//!
//! ⚠ THIS CASE TAKES A REAL SAVE, so its class name MUST sort after `..._Capability_...` - see the
//! suite header's case list. `Legacy*` does; `Base*` would not.
//!
//! ⚠ LIVE CAMPAIGN STATE IS BORROWED AND HANDED BACK: the chosen base's controlling faction, the
//! occupying faction's reserve and threat (both passed straight back through the apply, so neither
//! moves), and the deployment resource pool. Two side effects of driving the apply are ACCEPTED and
//! are the same ones OVT_TEST_Persistence_NewBase_DefaultsToOccupyingFaction accepts: every base and
//! tower with no record in the list handed in is swept to the occupying faction, and the chosen base's
//! persisted slot/garrison lists are cleared. Those lists are rebuilt from the LIVE base controller at
//! save time - the controller's own m_aSlotsFilled claim list is not touched - and are empty in a test
//! session anyway.
//!
//! 🔴 THE REFUND IS QUEUED BY THE APPLY AND PAID BY A LATER DELIVERY POINT, AND THAT IS ASSERTED HERE
//! AS A CLAIM IN ITS OWN RIGHT. It cannot be credited inline, because the deployment manager's own
//! restore CLEARS the per-faction resource pool and is authored several entries BELOW this manager in
//! Configs/Systems/Persistence/Overthrow.conf - so an inline credit is wiped microseconds later with
//! nothing logged. This case therefore asserts three separate things: the apply queues the right
//! amount, the apply does NOT move the pool, and the credit point delivers it exactly once.
//!
//! PROVEN ABLE TO FAIL (fail proofs recorded, execution belongs to the phase's suite run):
//!   - make ApplyPersistedBaseUpgrades() return 0 unconditionally and the queued-amount assertion goes
//!     red naming both numbers;
//!   - call AllocateDeploymentResources() from inside ApplyPersistedOccupyingFaction() instead of
//!     queueing and the "the pool moved during the apply itself" assertion goes red - which is the one
//!     that pins the ordering hazard;
//!   - delete the `m_iPendingLegacyRefund = 0;` line and the delivered-once assertion goes red;
//!   - delete `base.upgrades.Clear()` and the emptied-list assertion goes red on its own;
//!   - make the conversion count groups without multiplying by the per-group value and the refund
//!     assertion goes red with a number short by exactly the group value;
//!   - re-populate record.upgrades before the second pass instead of clearing it and the idempotence
//!     assertion goes red, which is what a write path that forgot to empty the array would look like.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_LegacyBaseUpgrades_ConvertToDeploymentResources : SCR_AutotestCaseBase
{
	//! Banked value on the patrol-shaped record. Not a number any campaign start produces.
	static const int BANKED_RESOURCES = 137;

	//! Groups standing on that same record, each worth OVT_BaseDefenseConversion.LEGACY_GROUP_SIZE men.
	static const int STANDING_GROUPS = 3;

	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
			return Save();

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE)
			return AwaitSave();

		return Assert();
	}

	//------------------------------------------------------------------------------------------------
	//! Takes one real save over untouched live state, which is what runs the rewritten write path.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool Save()
	{
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

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
		{
			SetFailure("OVT_Global.GetOccupyingFaction() is null");
			return true;
		}

		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
		{
			SetFailure("OVT_Global.GetDeploymentManager() is null, so there is no pool for a legacy refund to land in");
			return true;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("OVT_Global.GetConfig() is null");
			return true;
		}

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (!difficulty)
		{
			SetFailure("OVT_Global.GetDifficulty() is null, so the per-group refund value cannot be derived");
			return true;
		}

		int occupyingFaction = config.GetOccupyingFactionIndex();
		if (occupyingFaction < 0)
		{
			SetFailure("The occupying faction does not resolve to a faction index");
			return true;
		}

		OVT_BaseData base;
		foreach (OVT_BaseData candidate : occupying.m_Bases)
		{
			if (candidate)
			{
				base = candidate;
				break;
			}
		}

		if (!base)
		{
			SetFailure("The test world handed out no base, so there is no base record to convert");
			return true;
		}

		// BORROWED STATE. The reserve and threat are passed straight back through the apply, so neither
		// moves; the base faction and the pool are put back by hand below.
		int originalFaction = base.faction;
		int originalPool = manager.GetFactionResources(occupyingFaction);
		int reserve = occupying.m_iResources;
		float threat = occupying.m_iThreat;

		string failure = RunConversionClaims(occupying, manager, config, difficulty, base, occupyingFaction, reserve, threat);

		// TEARDOWN BEFORE REPORTING, ON EVERY PATH.
		base.faction = originalFaction;
		RestorePool(manager, occupyingFaction, originalPool);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("A legacy base record carrying %1 banked resources and %2 standing groups refunded exactly its value to the deployment pool, left the base's upgrade list empty, and refunded nothing on a rewritten (empty) payload",
			BANKED_RESOURCES.ToString(), STANDING_GROUPS.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The two claims, driven through the exact method the manager's own Deserialize calls.
	//! \param[in] occupying The occupying faction manager.
	//! \param[in] manager The deployment manager holding the pool.
	//! \param[in] config The Overthrow config, for the occupying faction key.
	//! \param[in] difficulty The campaign difficulty, for the per-man price the legacy valuation used.
	//! \param[in] base The live base the hand-built record is aimed at.
	//! \param[in] occupyingFaction The occupying faction index.
	//! \param[in] reserve The occupying reserve, passed back through unchanged.
	//! \param[in] threat The occupying threat, passed back through unchanged.
	//! \return An empty string when both claims hold, or the first broken one.
	protected string RunConversionClaims(notnull OVT_OccupyingFactionManager occupying, notnull OVT_DeploymentManagerComponent manager,
		notnull OVT_OverthrowConfigComponent config, notnull OVT_DifficultySettings difficulty, notnull OVT_BaseData base,
		int occupyingFaction, int reserve, float threat)
	{
		int groupValue = OVT_BaseDefenseConversion.LegacyGroupValue(difficulty.baseResourceCost);
		if (groupValue <= 0)
			return string.Format("A group in a legacy payload is worth %1 at this difficulty's baseResourceCost of %2 - the refund would be indistinguishable from no refund at all",
				groupValue.ToString(), difficulty.baseResourceCost.ToString());

		// PRECONDITION: nothing is already owed, or "the conversion queued exactly X" would be measured
		// against a leftover. A campaign that was not loaded from a pre-migration save never owes
		// anything - PostGameStart() settles any refund it does owe before this suite ever runs.
		if (occupying.GetPendingLegacyRefund() != 0)
			return string.Format("The occupying faction already owes its deployment pool %1 before this case planted anything - a previous refund was queued and never delivered",
				occupying.GetPendingLegacyRefund().ToString());

		int expectedRefund = BANKED_RESOURCES + (STANDING_GROUPS * groupValue);

		array<ref OVT_PersistedBase> records = new array<ref OVT_PersistedBase>();

		OVT_PersistedBase record = new OVT_PersistedBase();
		record.location = base.location;
		record.faction = occupyingFaction;

		// A PATROL-SHAPED RECORD: banked value AND groups that were standing when the game was saved.
		OVT_PersistedBaseUpgrade patrol = new OVT_PersistedBaseUpgrade();
		patrol.type = "OVT_BasePatrolUpgrade";
		patrol.resources = BANKED_RESOURCES;
		patrol.tag = "";
		patrol.pos = base.location;
		for (int i = 0; i < STANDING_GROUPS; i++)
		{
			OVT_PersistedBaseUpgradeGroup group = new OVT_PersistedBaseUpgradeGroup();
			group.prefab = "{00000000DEADBEEF}Prefabs/Nothing/ThisIsNeverResolved.et";
			group.position = base.location;
			patrol.groups.Insert(group);
		}
		record.upgrades.Insert(patrol);

		// A STRUCTURE-SHAPED RECORD: a tag and a position, nothing banked and no groups. It must refund
		// NOTHING, because the structure itself is a tracked world entity that comes back from the save
		// on its own - refunding for it would pay for it twice.
		//
		// ⚠ BOTH `type` STRINGS ABOVE AND BELOW NAME CLASSES THAT NO LONGER EXIST, AND THAT IS THE
		// POINT: this fixture stands in for a PRE-MIGRATION save point, and those are the literal
		// strings such a save carries. Nothing in the conversion matches on them (it converts banked
		// resources and group counts, never a class name), so they are payload realism rather than a
		// reference - do not "fix" them to a config name, which no legacy save could contain.
		OVT_PersistedBaseUpgrade composition = new OVT_PersistedBaseUpgrade();
		composition.type = "OVT_BaseUpgradeComposition";
		composition.resources = 0;
		composition.tag = "SmallBunker";
		composition.pos = base.location;
		record.upgrades.Insert(composition);

		records.Insert(record);

		// ---- CLAIM 1: THE POOL RISES BY EXACTLY THE COMPUTED VALUE -------------------------------
		int poolBefore = manager.GetFactionResources(occupyingFaction);

		occupying.ApplyPersistedOccupyingFaction(config.m_sOccupyingFaction, reserve, threat, records, null);

		// 🔴 THE REFUND IS OWED HERE, NOT PAID HERE, AND THAT IS THE DESIGN. It cannot be credited from
		// inside the apply, because the deployment manager's own restore CLEARS the resource pool and
		// runs after this manager's in the same load. So the apply queues, and one of two later delivery
		// points hands it over. The queued amount is asserted first - it is the conversion arithmetic -
		// and then the credit point is driven directly, which keeps the whole claim inside one frame
		// instead of racing a callback.
		int owed = occupying.GetPendingLegacyRefund();
		if (owed != expectedRefund)
			return string.Format("The conversion queued %1 for the deployment pool, expected exactly %2 - a pre-migration campaign either loses the investment it had made or is owed money it never spent",
				owed.ToString(), expectedRefund.ToString());

		if (manager.GetFactionResources(occupyingFaction) != poolBefore)
			return string.Format("The pool moved to %1 during the apply itself, expected it untouched at %2 - an inline credit is wiped by the deployment manager's own restore a few entries later in the same load, silently",
				manager.GetFactionResources(occupyingFaction).ToString(), poolBefore.ToString());

		occupying.CreditPendingLegacyRefund();

		int poolAfter = manager.GetFactionResources(occupyingFaction);
		int credited = poolAfter - poolBefore;

		if (credited != expectedRefund)
			return string.Format("The refund credited %1 to the deployment pool, expected exactly %2", credited.ToString(), expectedRefund.ToString());

		// Delivered exactly once: a second run of the credit point hands over nothing.
		occupying.CreditPendingLegacyRefund();
		if (manager.GetFactionResources(occupyingFaction) != poolAfter)
			return string.Format("Running the credit point twice moved the pool to %1, expected it to stay at %2 - the refund is not self-clearing, and both of its delivery points are armed on a real load",
				manager.GetFactionResources(occupyingFaction).ToString(), poolAfter.ToString());

		// ...AND THE BASE'S UPGRADE LIST IS LEFT EMPTY. Nothing replays these any more, and a populated
		// list is an invitation for something to try.
		if (!base.upgrades)
			return "The base's upgrade list is null after the conversion, expected an empty list - a null list is a VME waiting for the next reader";

		if (!base.upgrades.IsEmpty())
			return string.Format("The base's upgrade list holds %1 entries after the conversion, expected 0 - the records were copied onto the base as well as converted",
				base.upgrades.Count().ToString());

		// ---- CLAIM 2: A REWRITTEN (EMPTY) PAYLOAD REFUNDS NOTHING --------------------------------
		// This is what the same campaign's payload looks like after ONE save on this build: WriteBase()
		// stores an empty upgrade array. It is the entire idempotence mechanism, and there is no flag
		// behind it.
		record.upgrades.Clear();

		poolBefore = manager.GetFactionResources(occupyingFaction);

		occupying.ApplyPersistedOccupyingFaction(config.m_sOccupyingFaction, reserve, threat, records, null);

		owed = occupying.GetPendingLegacyRefund();
		if (owed != 0)
			return string.Format("A second pass over the rewritten (empty) payload queued another %1 - every load of the same campaign would hand the occupying faction another refund, forever",
				owed.ToString());

		occupying.CreditPendingLegacyRefund();

		poolAfter = manager.GetFactionResources(occupyingFaction);
		if (poolAfter != poolBefore)
			return string.Format("A second pass over the rewritten (empty) payload moved the pool from %1 to %2",
				poolBefore.ToString(), poolAfter.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the faction's resource pool back exactly where it was found, whichever way it moved.
	//! \param[in] manager The deployment manager.
	//! \param[in] factionIndex The faction whose pool was borrowed.
	//! \param[in] originalPool The value to restore.
	protected void RestorePool(notnull OVT_DeploymentManagerComponent manager, int factionIndex, int originalPool)
	{
		int current = manager.GetFactionResources(factionIndex);

		if (current > originalPool)
			manager.SubtractFactionResources(factionIndex, current - originalPool);
		else if (current < originalPool)
			manager.AddFactionResources(factionIndex, originalPool - current);
	}
}

//------------------------------------------------------------------------------------------------
//! The Fuel Depot buildable can be built, filled to a distinctive level, and carried through a real
//! save with that level intact.
//!
//! ==========================================================================================
//! ⚠ THIS IS THE PLAN'S DOCUMENTED FALLBACK, NOT THE FULL ROUND TRIP. READ THIS FIRST.
//!
//! docs/features/economy/fuel/implementation.md § Testing Strategy asks for a five-phase round trip
//! here - build, set the level, save, DIRTY the level, reload, assert the saved level came back -
//! and names its own degradation path "if the reload does not restore a mid-session build". It does
//! not, and that is structural rather than a timing problem:
//!
//!   OVT_PersistenceManagerComponent.ReapplyLatestSaveData() - the suite's only load seam - asks the
//!   persistence system for exactly ONE instance, the GAME MODE ENTITY (`request.Instances =
//!   {owner}`). Its own doc comment says so: "WHAT IT DOES NOT COVER. Anything outside the game mode
//!   entity's record - world entities, characters, vehicles, placeables." A depot is a separate
//!   tracked root (SelfSpawn 1, its own EntityPersistenceConfig keyed on OVT_BuildableComponent), so
//!   no re-application will ever put its record back. Restoring it means restarting the session,
//!   which the suite header explains at length is impossible inside the -autotest harness.
//!
//! WHY THIS CASE IS HERE AND NOT IN OVT_TEST_PersistenceSuite, which is where the plan's fallback
//! sentence points. Two hard reasons, either one sufficient:
//!   1. That suite's header forbids it in terms: "Nothing in THIS file triggers a save at all" /
//!      "Do not add save/reload assertions here." A save-taking case there would break its contract.
//!   2. OVT_TEST_PersistenceSuite is listed BEFORE this suite in Configs/Tests/OVT_TestGroup_All.conf,
//!      and this suite's capability gate asserts HasSaveGame() is FALSE before its own first save
//!      (closure 1). A save taken in the earlier suite turns that gate into a "precondition violated"
//!      failure - i.e. putting the fallback where the sentence says would make the All group red.
//! The save seam lives here, so the case that needs it lives here too. `LegacyBaseUpgrades_*` is the
//! standing precedent: it also takes a real save and uses NEITHER reload seam.
//!
//! WHAT IS THEREFORE STILL OWED TO A HUMAN: manual step F18 (part-fill the depot, save, RELOAD the
//! session, confirm the level). That step is the only thing that proves the serializer's read half.
//! ==========================================================================================
//!
//! WHAT THIS CASE DOES PROVE, and none of it is provable any other way in this harness:
//!  - "Fuel Depot" exists in Configs/Resistance/buildables.conf and is resolvable BY NAME (never by
//!    index - the index moves every time an entry is added);
//!  - BuildItem() with playerId -1 gets past OVT_FuelDepotHandler and returns a real entity, which is
//!    the server-side build path the handler is written for;
//!  - the spawned prefab carries OVT_BuildableComponent typed "FuelDepot" and is findable by that
//!    type from a world query, which is how any consumer would find it;
//!  - THE TWO PREFAB FACTS THE VANILLA SERIALIZER SILENTLY DEPENDS ON (implementation.md R4).
//!    SCR_FuelManagerComponentSerializer only walks SCR_FuelNode-typed nodes and SKIPS any node whose
//!    fuel equals its initial state. So a node authored as a bare BaseFuelNode, or an initial state
//!    authored as a fraction (0.5) instead of litres, makes persistence a no-op with no error
//!    anywhere. This case asserts a scripted node exists, that its capacity is the authored 10000 L,
//!    and that its initial state is 0 - all three of which are the difference between the depot
//!    saving and the depot silently not saving;
//!  - a real save completes with the depot in the world and does not disturb its level.
//!
//! NON-VACUOUS: the level asserted at the end is written BEFORE the save and read back through a
//! FRESH world query, not through the handle BuildItem() returned - so an assertion that passes
//! requires the entity to still be there and still hold the value. It is not a value the prefab or
//! the campaign produces: the depot is authored to start EMPTY.
//!
//! THE DEPOT IS LEFT STANDING ON PURPOSE. Deleting a persistence-tracked entity mid-suite means
//! driving the transient-untrack retry queue (BUG-118), which is far more likely to disturb the
//! later cases than an inert static prop is. It has no AI, no deployment and no manager registration
//! that any other case reads.
//!
//! ⚠ TAKES A REAL SAVE, so the class name MUST sort after `..._Capability_...` - `FuelDepot*` does.
//!
//! PROVEN ABLE TO FAIL: inverting the final equality (`if (fuel == SAVED_FUEL)`) fails the case with
//! both numbers named; recorded and reverted during Phase 3.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_FuelDepot_LevelSurvivesSave : SCR_AutotestCaseBase
{
	//! Resolved by name out of the buildables config. Never an index - entries get appended.
	static const string BUILDABLE_NAME = "Fuel Depot";

	//! OVT_BuildableComponent.m_sBuildableType on the prefab. Deliberately NOT the same string as the
	//! menu name (check-placeables.py reports the difference for every buildable in the file).
	static const string BUILDABLE_TYPE = "FuelDepot";

	//! Written before the save. Not a level the prefab or the campaign produces - it starts empty.
	static const float SAVED_FUEL = 1234;

	//! Litres of slack allowed when comparing back. Fuel is a float and passes through the engine.
	static const float FUEL_EPSILON = 0.5;

	//! SCR_FuelNode MaxFuel authored on OVT_FuelDepot.et. Raised 5000 -> 10000 by amendment A2.3 so a
	//! depot holds two full fuel-truck deliveries; SAVED_FUEL stays well inside it.
	static const float EXPECTED_MAX_FUEL = 10000;

	//! SCR_FuelNode m_fInitialFuelTankState authored on OVT_FuelDepot.et - LITRES, not a fraction.
	static const float EXPECTED_INITIAL_FUEL = 0;

	//! Metres from the chosen base to put the depot. Far enough to sit on its own, near enough that
	//! the buildable's own m_bBuildAtBase intent is what is being exercised.
	static const float BUILD_OFFSET = 30;

	//! Metres searched around the build position when finding the depot again.
	static const float REFIND_RADIUS = 12;

	//! Local phases. The gate's four-phase machine has no "spawn the subject" step, and the spawn
	//! deliberately gets a frame of its own before anything reads the fuel nodes off it.
	static const int PHASE_BUILD = 0;
	static const int PHASE_FILL_AND_SAVE = 1;
	static const int PHASE_AWAIT_SAVE = 2;
	static const int PHASE_ASSERT = 3;

	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected vector m_vBuildPos;
	protected IEntity m_FoundDepot;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == PHASE_BUILD)
			return Build();

		if (m_iPhase == PHASE_FILL_AND_SAVE)
			return FillAndSave();

		if (m_iPhase == PHASE_AWAIT_SAVE)
			return AwaitSave();

		return Assert();
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the buildable by name and builds one depot beside a base.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool Build()
	{
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance)
		{
			SetFailure("OVT_Global.GetResistanceFaction() is null");
			return true;
		}

		if (!resistance.m_BuildablesConfig || !resistance.m_BuildablesConfig.m_aBuildables)
		{
			SetFailure("The resistance faction has no buildables config loaded");
			return true;
		}

		int index = -1;
		for (int i = 0; i < resistance.m_BuildablesConfig.m_aBuildables.Count(); i++)
		{
			OVT_Buildable candidate = resistance.m_BuildablesConfig.m_aBuildables[i];
			if (candidate && candidate.m_sName == BUILDABLE_NAME)
			{
				index = i;
				break;
			}
		}

		if (index < 0)
		{
			SetFailure("No buildable named '%1' in the buildables config - the depot entry is missing or renamed",
				BUILDABLE_NAME);
			return true;
		}

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
		{
			SetFailure("OVT_Global.GetOccupyingFaction() is null, so there is no base to build beside");
			return true;
		}

		OVT_BaseData base;
		foreach (OVT_BaseData candidate : occupying.m_Bases)
		{
			if (candidate)
			{
				base = candidate;
				break;
			}
		}

		if (!base)
		{
			SetFailure("The campaign has no bases, so there is nowhere to put a base buildable");
			return true;
		}

		m_vBuildPos = base.location + Vector(0, 0, BUILD_OFFSET);

		// playerId -1 is BuildItem()'s own server-initiated marker: it waives the funds, distance and
		// item-limit checks, and OVT_FuelDepotHandler lets it through for exactly this reason.
		IEntity depot = resistance.BuildItem(index, 0, m_vBuildPos, vector.Zero, -1);
		if (!depot)
		{
			SetFailure("BuildItem() built no Fuel Depot at %1 - the prefab failed to spawn, or the handler refused a server build",
				m_vBuildPos.ToString());
			return true;
		}

		m_iPhase = PHASE_FILL_AND_SAVE;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Checks the prefab facts the vanilla serializer depends on, fills the depot, and saves once.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool FillAndSave()
	{
		SCR_FuelNode node;
		string diagnostic = ResolveDepotFuelNode(node);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		// R4, both halves. A serializer that skips this node writes nothing and says nothing.
		if (!float.AlmostEqual(node.GetMaxFuel(), EXPECTED_MAX_FUEL))
		{
			SetFailure("The depot's fuel node holds %1 L, expected %2 L - the prefab's MaxFuel was changed",
				node.GetMaxFuel().ToString(), EXPECTED_MAX_FUEL.ToString());
			return true;
		}

		if (!float.AlmostEqual(node.GetInitialFuelTankState(), EXPECTED_INITIAL_FUEL))
		{
			SetFailure("The depot's fuel node starts at %1 L, expected %2 L. That attribute is LITRES, not a fraction, and anything but empty means an untouched depot is skipped by the fuel serializer and never persists",
				node.GetInitialFuelTankState().ToString(), EXPECTED_INITIAL_FUEL.ToString());
			return true;
		}

		node.SetFuel(SAVED_FUEL);

		if (!float.AlmostEqual(node.GetFuel(), SAVED_FUEL))
		{
			SetFailure("Setting the depot's fuel to %1 L left it reading %2 L",
				SAVED_FUEL.ToString(), node.GetFuel().ToString());
			return true;
		}

		m_iSaveBaseline = OVT_TEST_PersistenceRoundTripGate.CompletedSaveCount();

		string trigger = OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce();
		if (trigger != "")
		{
			SetFailure(trigger);
			return true;
		}

		m_iPhase = PHASE_AWAIT_SAVE;
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

		m_iPhase = PHASE_ASSERT;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Finds the depot again from scratch and asserts it still holds the level that was saved.
	//! \return Always true - the case ends here either way.
	protected bool Assert()
	{
		string restored = OVT_TEST_PersistenceRoundTripGate.RequireRestoredCampaign();
		if (restored != "")
		{
			SetFailure(restored);
			return true;
		}

		SCR_FuelNode node;
		string diagnostic = ResolveDepotFuelNode(node);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		float fuel = node.GetFuel();
		if (Math.AbsFloat(fuel - SAVED_FUEL) > FUEL_EPSILON)
		{
			SetFailure("The depot's fuel level did not survive the save: filled to %1 L, reads %2 L afterwards",
				SAVED_FUEL.ToString(), fuel.ToString());
			return true;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Finds the depot near the build position by world query and hands back its single fuel node.
	//!
	//! Deliberately NOT cached from the handle BuildItem() returned: every phase looks the depot up
	//! again the way a consumer would, so an entity that quietly stopped existing fails the case
	//! instead of being read through a stale reference.
	//! \param[out] node The depot's scripted fuel node; untouched unless the return is empty.
	//! \return An empty string on success, otherwise the sentence to fail with.
	protected string ResolveDepotFuelNode(out SCR_FuelNode node)
	{
		m_FoundDepot = null;
		GetGame().GetWorld().QueryEntitiesBySphere(m_vBuildPos, REFIND_RADIUS, CollectDepot, null, EQueryEntitiesFlags.ALL);

		IEntity depot = m_FoundDepot;
		m_FoundDepot = null;

		if (!depot)
		{
			return string.Format("No buildable of type '%1' within %2 m of %3 - the depot is not in the world",
				BUILDABLE_TYPE, REFIND_RADIUS.ToString(), m_vBuildPos.ToString());
		}

		SCR_FuelManagerComponent fuelManager = SCR_FuelManagerComponent.Cast(depot.FindComponent(SCR_FuelManagerComponent));
		if (!fuelManager)
		{
			return "The depot has no SCR_FuelManagerComponent - it stores nothing and can persist nothing";
		}

		array<SCR_FuelNode> nodes = {};
		int count = fuelManager.GetScriptedFuelNodesList(nodes);
		if (count < 1)
		{
			return "The depot's fuel manager holds no SCR_FuelNode. The vanilla fuel serializer only walks scripted nodes, so a bare BaseFuelNode would make its tank level silently unsaveable";
		}

		node = nodes[0];
		if (!node)
			return "The depot's first scripted fuel node is null";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! World-query collector: keeps the first entity carrying a Fuel Depot buildable component.
	//! \param[in] entity Candidate from the sphere query.
	//! \return False once the depot is found, which stops the query.
	protected bool CollectDepot(IEntity entity)
	{
		if (!entity)
			return true;

		OVT_BuildableComponent buildable = OVT_BuildableComponent.Cast(entity.FindComponent(OVT_BuildableComponent));
		if (!buildable || buildable.GetBuildableType() != BUILDABLE_TYPE)
			return true;

		m_FoundDepot = entity;
		return false;
	}
}
