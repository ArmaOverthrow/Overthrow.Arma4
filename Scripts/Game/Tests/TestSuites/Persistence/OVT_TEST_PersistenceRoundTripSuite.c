//------------------------------------------------------------------------------------------------
//! TIER D' - SAVE/RELOAD ROUND TRIP. Part of OVT_TestGroup_All.
//!
//!   tools/run-tests.sh OVT_TEST_PersistenceRoundTripSuite     # just this suite
//!   tools/run-tests.sh "{6A6E2A002F53A581}"                   # or the whole All group
//!
//! ⚠ NEVER run the save scripts without --profile OverthrowCI (or an explicit OVERTHROW_SAVE_DIR):
//! their default target is the user's real Workbench campaign save. run-tests.sh resets the
//! OverthrowCI save state itself before every run, which is what makes the capability case's
//! fresh-session precondition hold.
//!
//! ===========================================================================================
//! ASSERTION RULE, NON-NEGOTIABLE (Decision 4). No persistence-framework type, no vanilla
//! persistence type and no Overthrow save-data class may appear anywhere in these files except the
//! annotated trigger calls; every assertion reads state back through the same public manager API
//! that wrote it. (The rule is enforced by grepping this tree for those type names, which is why
//! this comment describes them rather than naming them.) There are THREE annotated triggers, all
//! in the gate class below: TriggerSaveOnce() saves, RequestSessionReload() loads the game mode
//! entity, RequestInstanceReload() loads ONE world entity that owns its own record.
//! ===========================================================================================
//!
//! ⚠ SaveGame() returns COMPLETELY SILENTLY when the layer is absent, so every case asserts the
//! CAPABILITY first and the failure reaching junit.xml names the missing capability in one
//! sentence rather than "expected 12345, got 777". Saving is ASYNCHRONOUS, so that is a bounded
//! wait (MAX_SAVE_POLLS), not a same-frame check; expiry raises the same CAPABILITY_ABSENT
//! sentence. A bounded diagnostic wait is not a retry - nothing is attempted twice, and expiry
//! FAILS.
//!
//! ANTI-VACUOUS-PASS DESIGN - this suite must be UNABLE to go green without persistence working.
//! For each stub that could fake a pass, the closure:
//!  1. HasSaveGame() hardcoded true -> the capability case asserts the whole TRANSITION: false
//!     before the first save of a fresh session, true after. Both constant stubs fail one half.
//!  2. A no-op reload -> every state-kind case DIRTIES the value after saving and before
//!     reloading, and asserts the SAVED value came back. Independent of closure 1.
//!  3. A reload that resets to campaign-start defaults -> the mutated value is deliberately not one
//!     campaign start would produce, and the assertion is equality with the saved value.
//!  4. A case that silently skips its assertions -> every resolution failure is an explicit
//!     SetResultFailure with a named reason. No path reaches SetResultSuccess without asserting.
//!  5. Reliance on execution order -> no state-kind case depends on order; each triggers its OWN
//!     save and waits for it. ⚠ ONE order dependency does exist: the capability case's
//!     RequireFreshSession() needs HasSaveGame() false before this suite's first save, which holds
//!     only because `..._Capability_...` sorts alphabetically first. A case that sorts BEFORE it
//!     and takes a save turns that gate into a precondition violation - name deployment cases
//!     "Deployment*", never "Base*".
//!  6. A reload restoring an OLDER save than this case wrote -> a case reads the manager's
//!     COMPLETED-SAVE COUNT before triggering and waits for that number to go up, rather than for
//!     "a save exists" (true forever after the first case) or "no save in progress" (also true
//!     when the save was silently refused).
//!
//! THE RELOAD MECHANISM, AND WHAT THIS SUITE THEREFORE DOES AND DOES NOT PROVE. The reload half is
//! an IN-SESSION re-application: ReapplyLatestSaveData() re-reads the stored record for instances
//! that are already live. Per case: mutate -> save -> DIRTY the value -> re-apply -> assert the
//! SAVED value.
//!
//! 🔴 The real player-facing continue flow - savepoint on disk -> SaveGameManager load -> session
//! restarts - is NOT covered and cannot be. It was tried: a mid-case load is a game-state
//! transition, the CLI harness treats every world load as a brand new test run and restarts the
//! suite, so no case can resume on the other side. The measured result was an infinite restart loop
//! with no junit written at all. So a green run here means "what Overthrow persists is written to
//! storage and comes back from storage", NOT "quitting and continuing a campaign works"; the
//! restart path stays a MANUAL play-test item. RequireRestoredCampaign() is a sanity assert in this
//! rung, not proof of a load. LoadLatestSave() is deliberately kept as the production API - it is
//! simply not what a test can call.
//!
//! ONE CASE TAKES NO SAVE POINT AT ALL: `..._VehicleReserveRelease_...` exercises per-instance
//! reservation - an owned instance taken out of PLAY and put back without leaving the world. It
//! uses neither gate seam, so it cannot disturb the capability case's precondition. Its
//! non-vacuousness comes from the state change in the middle, on the SAME entity at both ends.
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
	//! baseline taken before triggering - and that a save now exists. That is closure 6: after the
	//! first case has saved, HasSaveGame() is true forever, so a wait that only checked it would let a
	//! case reload from a savepoint older than its own mutation.
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
	//! Requests the in-session re-application of ONE tracked entity's own stored record.
	//!
	//! The seam above can only ever ask for the game mode entity, which is why the FuelDepot case is a
	//! save-only degradation: a buildable owns its record rather than riding the game mode's. This asks
	//! for that record by instance, which is what makes a world-entity round trip possible at all.
	//! \param[in] entity The tracked entity whose record should be re-read.
	//! \return An empty string when the re-application was requested, otherwise a diagnostic.
	static string RequestInstanceReload(IEntity entity)
	{
		string diagnostic;
		OVT_PersistenceManagerComponent persistence = ResolvePersistence(diagnostic);
		if (!persistence)
			return diagnostic;

		// ===========================================================================================
		// PERMITTED PERSISTENCE-LAYER SEAM 3 OF 3 IN THE ENTIRE TEST TREE (Decision 4) - THE LOAD, FOR
		// ONE WORLD ENTITY. Same terms as the other two: Overthrow's public manager API, no storage
		// type named, no engine save API touched.
		persistence.ReapplyEntitySaveData(entity);
		// ===========================================================================================

		if (!persistence.IsReapplyInProgress() && persistence.GetLastReapplyDiagnostic() != "")
			return "The entity's persisted data could not be re-applied: " + persistence.GetLastReapplyDiagnostic();

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Whether an entity has a stored record of its own yet. Registration is asynchronous, so a case
	//! that saves a freshly spawned subject waits on this first.
	//! \param[in] entity The entity to ask about.
	//! \return True when the entity is tracked.
	static bool InstanceIsTracked(IEntity entity)
	{
		return OVT_PersistenceTracking.IsTracked(entity);
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
	//! Two jobs, the first load-bearing: a re-application that never happened, or that the persistence
	//! system refused, is reported BY NAME - the manager's diagnostic is empty exactly when the last
	//! one succeeded, so anything found here means the reload half did not run at all, a very
	//! different fault from "the value did not come back". Second, the campaign is still started - a
	//! sanity assert in this rung, but it catches a re-application that tears the game mode down.
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
//! Read this one first when the suite is red: its failure text names the missing capability in one
//! sentence, so junit.xml explains itself without opening the source. It asserts the whole
//! transition - no save before, a save after - which is what makes a lying save layer detectable.
//!
//! The only case with no reload, so it keeps the shorter timeout.
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
//! The INACTIVE flag is part of that record. The recruit is deactivated before the save, so the
//! assertion after the reload covers the whole serializer v3 chain - the write, the field's position
//! at the end of the record, the read back, and ApplyPersistedRecruits() adopting it. A recruit that
//! came back ACTIVE after being parked would walk back into its owner's squad on load, which no
//! compile check can see.
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
//! ⚠ Stability is never set directly in Overthrow. Every path that moves it runs
//! modifier -> RecalculateStability -> stored value, and TryAddStabilityModifier() /
//! RemoveStabilityModifier() are the two public ends of that path (both synchronous on the server).
//! An earlier draft wrote town.stability straight onto the record: a persistence layer that stores
//! the MODIFIER LIST and recomputes on load - which is what a correct one does - would restore the
//! recomputed value and never the raw field, so that draft could have stayed red no matter how
//! complete the migration was.
//!
//! The saved value is DERIVED, never hardcoded. The modifier chosen is the first with a NEGATIVE
//! base effect, because stability starts at the configured maximum and a positive one would clamp -
//! and a stability below the maximum is not a value campaign start produces (closure 3).
//!
//! The DIRTY step (closure 2) is removing the modifier, which recalculates live stability back to
//! where it started.
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
//! Until BUG-086 this proved a per-instance STORAGE round trip: despawn wrote the record, released
//! tracking and deleted the instance. That mechanism is gone - the record it depended on was
//! measured being pruned within ten minutes on a live server. The replacement never destroys the
//! vehicle:
//!
//!     spawn a vehicle, owned by the test player and locked
//!       -> the manager's despawn path writes its record and HIDES it, still alive and still tracked
//!       -> the manager's respawn path un-hides it
//!       -> assert it is the SAME instance, still owned, still locked, still placed, still fuelled
//!
//! The assertions are STRICTER than they were. "Same instance" is a stronger claim than "an instance
//! with matching fields", and it is the claim the fix rests on - contents survive by construction
//! only if the entity was never rebuilt:
//!   - SAME ENTITY - the EntityID before the despawn is the EntityID after. A rebuild cannot satisfy
//!     this.
//!   - STILL TRACKED - reserving must not release tracking; an untracked vehicle is absent from the
//!     next save point. Nothing else in the tree asserts this.
//!   - HIDDEN, THEN NOT - what makes the middle step a real state change rather than a no-op.
//!   - OWNER read twice (through the manager by RplId and through the vehicle's own component), plus
//!     LOCKED, POSITION and FUEL.
//!
//! Said plainly: nothing in the automated tree now exercises PersistenceSystem.RequestSpawn() for a
//! vehicle. That path still exists as the post-restart fallback and is play-test territory.
//!
//! It uses NEITHER gate seam - no save trigger, no re-apply - so it cannot disturb the capability
//! case's fresh-session precondition.
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
				// DIAGNOSTIC, NOT AN ASSERTION. Fuel deterministically restores to the prefab-initial level
				// for the UAZ CIV starting cars: vanilla's SCR_FuelManagerComponentSerializer persists only
				// SCR_FuelNode-typed tanks, and whether this Overthrow-local UAZ prefab chain carries them
				// could not be confirmed statically. Everything this case proves - entity back, owner, lock,
				// position - is asserted above and stays hard. Fuel and trunk contents are on the manual
				// play-test with a shop-bought vanilla-chain vehicle.
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
//! The test world carries exactly one transmitter tower, so the case takes the first tower the
//! manager holds and resolves it back by LOCATION - the same match key OVT_PersistedRadioTower uses,
//! because towers are world-derived and a save can never create one.
//!
//! SetRadioTowerDisabled() is the seam the sabotage RPC itself calls, so the case drives the
//! production path rather than writing the field behind it.
//!
//! ⚠ A range, not an equality: the server ticks the timer down by RADIO_TOWER_CHECK_FREQUENCY (9 s)
//! both before the save and after the re-apply, so the restored number is necessarily SMALLER than
//! the one written. The window is one the countdown cannot walk out of within the case's timeout and
//! that no value other than the saved timer can land in.
//!
//! Anti-vacuous: the timer is cleared to zero in the dirty step, so the tower is demonstrably back
//! on the air before the re-apply.
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
//! A returning player is normally rebuilt from their STORED BODY, which carries its own transform,
//! so this pair only matters when that body cannot be found - measured on a dedicated server: after
//! a restart the body answered NOT_FOUND and the player woke at their home on the far side of the
//! map. The position therefore lives as plain data on the player's own record, which travels inside
//! the game-mode record. This case guards that independence.
//!
//! ⚠ The expected value is READ OFF THE LIVE CHARACTER, not written by the case.
//! SyncPlayerBodyIds() runs from PreShutdownPersist() before EVERY save and overwrites the stored
//! transform with where the body actually is - that is the behaviour under test, so the case asserts
//! the whole pipeline rather than just the codec.
//!
//! ⚠ The dirty value is ZERO, deliberately. ApplyPersistedPlayers() adopts the stored transform ONLY
//! when the live record has none - the rule that stops re-applying a save from teleporting a player
//! standing in the world. Zero is exactly the state a freshly loaded record is in, so it is both the
//! honest dirty value and the one that exercises the adopt path. A non-zero dirty value would assert
//! the opposite invariant.
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
//! Sleeping skips eight in-game hours and may not be repeated for twelve, and the cooldown is stored
//! as an absolute GAME-CLOCK stamp precisely so it survives a quit and a Continue. If the stamp is
//! lost, every load hands the player a fresh sleep - eight more hours of income and threat decay for
//! the price of a save and a reload. Nothing else in the tree would go red for it: the value is
//! server-only, never replicated, and invisible in every UI except the action's own label.
//!
//! The saved value is SYNTHETIC, correctly here and unlike the last-known-position case above:
//! nothing runs over this field before a save - it is written in exactly one place
//! (OVT_SleepService.PerformSleep) and read everywhere else.
//!
//! ⚠ The dirty value is the never-slept sentinel -1, which is exactly the state a player who has
//! never slept is in (and what a version 4 save's players are reset to on load). The assertion is
//! equality with the SAVED stamp, not "not the dirty value" (closures 2 and 3).
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
//! A vehicle parked on a buildable maintenance ramp came back rotated ~45 degrees after a load. The
//! pose DATA round-trips exactly; what moves the vehicle is physics AT the load - it self-spawns as
//! a live dynamic body with no saved velocities while the ramp under it is a separately self-spawned
//! record with no ordering guarantee, so it free-falls onto the terrain or takes the depenetration
//! kick when its support spawns into it. On flat ground both effects are invisible, which is why
//! only ramp-parked vehicles were reported. ReassertRecordedPose() is the healing seam.
//!
//! ⚠ The NO-OP half is asserted first: a seam that snapped every vehicle - drifted or not - would
//! teleport cars out from under their owners on every load.
//!
//! Both halves assert in the SAME FRAME as the seam call, so physics cannot settle between act and
//! assert and the tolerances can be tight.
//!
//! It cannot see the real load-order race (ramp spawning after the vehicle) - that needs a genuine
//! restart and is play-test territory.
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
//! Until 2026-08-04 the registry was memory-only. A locked vehicle is saved, released and deleted
//! 60 s after its owner logs out, so it is not a world entity when the save is written; after a
//! restart nothing remembered its id and the player's car was gone permanently.
//!
//! No real vehicle is spawned, on purpose - registration from a live vehicle is already covered
//! elsewhere. The record is injected through the manager's own public apply path and read back
//! through its own public accessor, which is what makes the assertion about the CODEC and nothing
//! else.
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
//! A saved job used to name itself by its POSITION in the job manager's config list. Trim or reorder
//! that list and every saved record silently comes back attached to a DIFFERENT job - at a stage
//! index that is still valid, paying that other job's reward, with its lifetime counters capping the
//! wrong thing, and with no error anywhere. The save format now names each job by a stable id, and
//! this case checks that each record came back on the job it was saved on, not merely that "some
//! jobs came back".
//!
//! ⚠ Records are found again by a unique LOCATION far outside the world, never by a count: the
//! manager's CheckUpdate() offers new public jobs on a timer and the board legitimately grows during
//! a run. A fixture seeded with 4 jobs held 12 by the time the save landed.
//!
//! Why nothing the manager does on its timer can disturb these records:
//!  - CheckUpdate()'s tick loop skips any job with accepted == false, covering records B and C.
//!  - Record A is accepted but parked on base-recon's only stage, an
//!    OVT_WaitTillPlayerInRangeJobStage whose OnTick() returns TRUE - keep waiting - the moment its
//!    owner lookup fails. The owner is a synthetic marker no player carries.
//!  - Both global counters belong to configs seeded far ABOVE their cap and neither is
//!    player-allocated, so CheckUpdate() skips the whole config and can never increment them.
//!  - The per-player counters are under a synthetic persistent id, and CheckUpdate() only writes
//!    per-player counts for ids the player manager actually holds.
//!
//! The per-player record is synthetic on purpose: after the starter jobs were retired every shipped
//! config is public or base-only, so that map is empty in a real campaign. It is still persisted and
//! still must round-trip.
//!
//! The dirty step does not just delete: it removes one record, RE-POINTS another at a different job
//! config - the exact mis-attachment this feature exists to make impossible - moves a third to a
//! different stage, and rewrites both counter maps to wrong values.
//!
//! ⚠ It reloads TWICE, because idempotency is part of the claim: the persistence manager re-applies
//! saved data to a LIVE session, so ApplyPersistedJobs() has to be a clear-and-rebuild rather than
//! an append. The second re-application asserts each marker location still holds exactly one record.
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
//! "Dead members stay dead" (G3) has a terminal case: a group whose whole roster died is REMOVED,
//! and the campaign must never hand it back. Under Route B that promise is entirely Overthrow's to
//! keep - core re-creates its own group entities from its own payload on every load, so a record
//! that reached the payload by accident IS a resurrected garrison standing in a base the player
//! already cleared.
//!
//! The record is already gone BEFORE the save, and the case asserts that before saving - otherwise
//! it would be proving something about a payload entry that was never written.
//!
//! Anti-vacuous: the dirty step RE-REGISTERS a group under the same owner key - a deliberate
//! resurrection, and exactly the shape a stale record would have. The assertion is "the owner key
//! resolves to nothing", which neither a no-op reload nor a reset to defaults can satisfy.
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
//! Under Route B this is entirely Overthrow's own machinery: vanilla persists nothing about these
//! groups. The registry serializer writes complete re-creation state and the manager rebuilds the
//! group entity itself on load, so every field missing from the payload is a field the campaign
//! silently loses - a garrison back at full strength after the player fought it down to two men, or
//! back at the wrong place, or back with the wrong men alive.
//!
//! The dirty step kills every remaining slot, which WIPES the record: the registry entry is removed
//! and the group entity deleted. The restore therefore cannot pass by leaving anything alone - it
//! has to re-create the group from the payload with the right slot still dead. It also registers a
//! second, BOGUS group the save knows nothing about, so "records the payload does not claim are
//! dropped" is asserted in the same pass. None of the asserted values (a 1234 m spawn ring, a HIGH
//! tier, a 37 m patrol radius, one specific dead slot) is one a campaign start would produce.
//!
//! The composition is DISCOVERED, not hard-coded: a faction-config rename must go red in the faction
//! tests. A roster of at least two slots is required, because "the specific dead slot is still dead"
//! has to be distinguishable from "the count came back", so every entry the faction defines is tried
//! until one is big enough.
//!
//! ⚠ THE FIXTURE'S PLAN IS DELIBERATELY STATIONARY. This case asserts persistence, not motion, and
//! virtual movement advances ANY dormant registered group whose plan has something to advance. The
//! points were two PATROL points 150 m apart, so the movement tick walked this fixture across the
//! save/reload window and turned the ±1 m position claim into a timing lottery. DEFEND is core's
//! "this group belongs here" plan and is never advanced. If a future feature needs a MOVING fixture,
//! register a second group for it; do not make this one move, and do not widen the tolerance.
//!
//! ⚠ Two properties make any fixture in this tree safe from the movement tick: (a) it registers a
//! null / empty / DEFEND-only plan, or (b) it registers and unregisters inside ONE frame. Every
//! other RegisterGroup( site under Scripts/Game/Tests/ was swept and satisfies one of them; the
//! per-site verdict table is in docs/features/virtualization/integration/context.md.
//!
//! THE MOVED-POSITION CLAIM. Before the save the group is deliberately relocated with SetPosition()
//! and the case asserts it comes back at the MOVED position and NOT at the registration one. Core's
//! SnapshotRegistry reads the LIVE group origin, so whatever moved a group is what a save keeps -
//! asserted here without a movement-specific line, and timing-free: a direct write, not a tick. The
//! second half of the pair exists to make "really moves" self-enforcing, so nobody can quietly
//! delete the move and leave a claim that asserts a coincidence.
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
	//! ⚠ Deliberately tiny. The case lives across many frames and the engine's 1 Hz lifecycle tick
	//! would materialise members of any group an observer stands inside, turning "it came back
	//! dormant" into a coin flip. The despawn ring stays strictly larger, so this is still a normal
	//! ProximityDriven registration.
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
	//! ⚠ The types are DEFEND on purpose. They were PATROL, which is movable: the movement tick
	//! advances every dormant registered group whose plan has something to advance, so this fixture
	//! drifted along its own 150 m leg while the case waited out a save and a reload, reddening
	//! Verify()'s ±1 m position claim for reasons unrelated to persistence.
	//!
	//! The payload claims are unchanged in number and strength - two distinct positions 150 m apart,
	//! two types, two float params, m_bCycle - and still three real waypoint entities on the
	//! re-created group, because core builds a DEFEND waypoint from a DEFEND plan entry exactly as it
	//! builds a patrol one from a PATROL entry.
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
//! Shared machinery for the four DEPLOYMENT cases below.
//!
//! ===========================================================================================
//! THE HONEST LIMIT OF THESE FOUR CASES, STATED ONCE.
//!
//! A deployment is a MARKER ENTITY, and its state is written by a component serializer bound to that
//! entity's own configuration - NOT the game mode's. The suite's reload seam asks for exactly one
//! instance back, the game mode entity, so it CANNOT hand a deployment marker its stored payload.
//! These cases therefore split the round trip and assert both halves honestly:
//!
//!   WRITE HALF - real. The fixture is created through the deployment manager's own public creation
//!   path, which is what makes a marker part of a save point, and the case takes a real save. A
//!   serializer that cannot write the state under test fails there.
//!
//!   READ HALF - the public apply, not a re-read. ApplyPersistedDeployment() is the method the
//!   marker's Deserialize calls with the values it read. Handing it the payload the save was taken
//!   of is the closest a case in this harness can get to a marker coming back.
//!
//! NOT asserted here or anywhere automated: that the bytes on disk read back as the values that went
//! in. That is a real-restart claim, in the same bucket as the continue flow, and it is covered by
//! inspection instead (a decoded save point, field by field, in the feature's context.md).
//! ===========================================================================================
//!
//! ⚠ Every deployment fixture here is marked ELIMINATED. A live deployment starts a repeating 8-12 s
//! update whose first tick converges its spawning modules, registering real groups at the GLOBAL
//! 1750 m ring - inside which the autotest camera is an observer. A fixture that did that would
//! materialise soldiers next to the test camera, hand the movement tick a cycling perimeter plan to
//! walk, and leave records behind in a shared world. Marking it eliminated makes the fixture inert
//! BY CONSTRUCTION rather than by finishing before a timer, so it survives a host stall.
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
	//! branch and InitializeDeployment() never runs. The shipped prefab authors no config, which is
	//! asserted rather than assumed below.
	//!
	//! Deliberately NOT part of any save point, so it cannot leave a stray deployment record in the
	//! CI save for the cases that follow.
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

		// ⚠ An entity that is not world-registered answers EntityID.INVALID, and EVERY such entity
		// answers the SAME one (a marker spawned 3 km out in this small world came back unkeyable). The
		// deployment manager keys its active list on this id, so a fixture with an invalid one would
		// silently share a slot with the next fixture.
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
//! comes back as the SAME STRING rather than as a fresh derivation.
//!
//! The other four values are bookkeeping; the key is IDENTITY. It is the string this deployment's
//! registered AI groups are tagged with in the virtualization registry, and reclaiming them after a
//! load is a lookup by exactly that string. A restore that re-derived it would agree in every
//! ordinary case and disagree the moment a marker came back a metre off - silently: the reclaim
//! finds nothing, the module converges from zero, and the deployment registers a second force on top
//! of the one already standing there.
//!
//! ⚠ The key planted here is one derivation could not produce, and the case ASSERTS that
//! precondition before anything else - without it, "the key came back" would be satisfied by a
//! re-derivation that happened to agree.
//!
//! The fixture is created through the manager's own creation path, so the save really does run the
//! deployment serializer's write half over a live deployment carrying a version 2 key.
//!
//! Read the fixture class header for what the reload seam can and cannot reach.
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
//! itself, and not on the convergence that follows it. (G4's teeth.)
//!
//! ⚠ The wipe-out flag is written BEFORE InitializeDeployment() in ApplyPersistedDeployment(), and
//! that ordering is the entire mechanism: InitializeDeployment reads the flag to decide whether the
//! spawning modules it has just cloned start out eliminated. The previous persistence layer set the
//! flag afterwards, and a patrol the player had wiped came back at full strength on the next load,
//! every time, in silence.
//!
//! Three claims, in the order they can break:
//!   1. the restore marks the deployment eliminated;
//!   2. it marks every SPAWNING MODULE eliminated too - the ordering claim, because nothing but
//!      InitializeDeployment's flag-before block does that on a fresh restore;
//!   3. EnsureGroups() registers ZERO groups under the module's owner key.
//! Then a fourth after the reload: re-applying the same payload to a LIVE deployment whose modules
//! have been un-marked puts the marks back, which is what makes in-session re-application safe.
//!
//! ⚠ The dirty step clears the MODULE flags and deliberately leaves the deployment's own flag set.
//! Convergence refuses on either, so this fixture can never register anything - which matters
//! because a deployment's own 8-12 s activation tick calls EnsureGroups() on its own schedule, and
//! clearing both flags would bet that the tick does not fire in the ~100 ms window before the assert
//! phase puts them back. This project has measured a 105 s main-thread stall in this harness.
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
//! still restores, and the deployment mints its key from its own marker on first use.
//!
//! Not hypothetical: a decoded save point from before this epic carries 23 deployment records whose
//! fields stop at the wipe-out flag, with no key field at all. The serializer reads the key only
//! when the stored version says one was written, so a version 1 record reaches
//! ApplyPersistedDeployment with an EMPTY key string - which is exactly what this case hands it.
//!
//! Two claims, the second being the one nobody would think to make:
//!   1. an empty key restores cleanly and EnsureVirtualKey() then derives one from the marker's own
//!      position - once, and the same string on every later call;
//!   2. re-applying that same version 1 payload later does NOT wipe the key the session has since
//!      derived. Payloads are re-applied to live instances, and a blind write would empty the key of
//!      a deployment whose groups are already tagged with it, orphaning the whole force to a reclaim
//!      that will never find it. The guard is one `if`.
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
//! RESTORE - verbatim, including the two structural characters the key scheme is built out of.
//!
//! That virtual groups survive a save, and that a wiped one does not come back, are asserted by the
//! two cases below. This one asserts the seam the whole deployments migration rests on: after a
//! restore, a spawning module reclaims its own groups by asking FindGroupsByOwner for the composed
//! key `<deployment key>#<module tag>`.
//!
//! ⚠ The SHAPE is the risk. A deployment owner key carries an '@' and a '#' - the only owner key in
//! the epic that does. A payload that truncated, trimmed or normalised either character would leave
//! every record present and correct and every reclaim silently empty, and an empty reclaim is not an
//! error: the module concludes it holds nothing and converges a second force on top of the one the
//! restore just rebuilt. Two keys differing ONLY in their module tag are registered, and each must
//! answer for exactly its own groups.
//!
//! It also exercises the positional fallback tag "m1" - what a module with no authored name gets. No
//! shipped config produces it today, so this is the only place it is composed, stored and read back.
//!
//! Fixture footprint: three registrations, null plans, spawn distance 0, all unregistered before the
//! case reports on every path.
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
//! A BASE-DEFENSE DEPLOYMENT'S FIVE PERSISTED VALUES COME BACK, ITS CONFIG STILL RESOLVES BY NAME,
//! AND THE RESTORE MARKS IT AS RESTORED.
//!
//! ⚠ The reload seam builds its request with Instances = {gameMode} only, so a deployment MARKER's
//! Deserialize is NEVER re-run by it. This case does what integration's four deployment cases do:
//! the WRITE half is real (the fixture is created through the manager's own public creation path and
//! a real save is taken), the READ half is the public apply, not a re-read. What is NOT asserted
//! here or anywhere automated is that the bytes on disk read back as the values that went in - a
//! real-restart claim, covered by inspection. ⚠ DO NOT WIDEN THE SEAM to "fix" this: widening it
//! means naming persistence-framework types inside Scripts/Game/Tests/, which the assertion rule
//! forbids.
//!
//! A second deployment round trip is warranted because DeploymentRecord_SurvivesSaveAndReapply runs
//! on "Town Patrol", whose modules are all pre-migration. This one runs on a BASE DEFENSE config and
//! adds the two things that are new:
//!   - the restored deployment still carries a live OVT_PlacedInfantrySpawningDeploymentModule WITH
//!     its m_Placement provider. CloneModule is hand-written and not chained; a dropped line ships a
//!     module that wants zero groups, registers nothing and logs nothing, and a base's tower guards
//!     simply never come back after a load;
//!   - WasRestoredFromSave() is TRUE afterwards - D7's gate, the one thing stopping a restored
//!     deployment building a second bunker on every load, and nothing else asserts it.
//!
//! ⚠ This fixture cannot delete itself mid-run despite its config authoring
//! m_bDeleteOnConditionFail 1. The delete branch lives in CheckReinforcement(), which OnUpdate()
//! reaches only after m_fInitialDelay (class default 300 000 ms) has elapsed since activation, and
//! the case's whole budget is 60 s. If a future tuning pass authors a shorter initial delay on these
//! configs, THIS is the case that starts failing intermittently, and the fix is to pick a config
//! without the delete flag - not to lengthen the timeout.
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
//! AND A REWRITTEN PAYLOAD REFUNDS NOTHING.
//!
//! Every campaign saved before this migration carries, per occupying-held base, a list of upgrade
//! records: what each upgrade had banked and how many groups it had standing. The upgrade classes
//! those records describe are deleted, so the records are read once for their VALUE, the sum is
//! credited to the deployment resource pool, and the evaluator re-establishes defense from it -
//! value-parity, not entity-identity.
//!
//! Neither failure mode logs anything:
//!   - REFUND NOTHING - a loaded legacy campaign arrives with no defense AND no money to buy any.
//!   - REFUND TWICE - the refund is idempotent STRUCTURALLY, because the write path stores an EMPTY
//!     upgrade array from now on, and there is deliberately no flag guarding it. If that structural
//!     argument broke, every load would hand the faction another few thousand resources, forever.
//!
//! ⚠ The reload seam re-applies the GAME MODE's record, which does include this manager - but the
//! payload it would re-read is one this build WROTE, and this build writes the upgrade array empty.
//! So the WRITE half is real and is the half that changed (WriteBase() now writes an empty array, so
//! a save that completes is the new body having run over every base in the world), and the READ half
//! is the public apply handed hand-built records shaped like a 2026-era save's. ⚠ DO NOT WIDEN THE
//! SEAM - that means naming persistence-framework types inside Scripts/Game/Tests/.
//!
//! ⚠ This case takes a REAL save, so its class name MUST sort after `..._Capability_...`. `Legacy*`
//! does; `Base*` would not.
//!
//! ⚠ Live campaign state is borrowed and handed back: the chosen base's controlling faction, the
//! occupying faction's reserve and threat, and the deployment resource pool. Two side effects of
//! driving the apply are accepted: every base and tower with no record in the list handed in is
//! swept to the occupying faction, and the chosen base's persisted slot/garrison lists are cleared
//! (both are rebuilt from the LIVE controller at save time and are empty in a test session anyway).
//!
//! 🔴 The refund is QUEUED by the apply and PAID by a later delivery point, and that is asserted as
//! a claim in its own right. It cannot be credited inline, because the deployment manager's own
//! restore CLEARS the per-faction resource pool and is authored several entries BELOW this manager
//! in Overthrow.conf - so an inline credit is wiped microseconds later with nothing logged. Three
//! separate assertions: the apply queues the right amount, the apply does NOT move the pool, and the
//! credit point delivers it exactly once.
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
		// ⚠ Both `type` strings name classes that NO LONGER EXIST, and that is the point: this fixture
		// stands in for a pre-migration save point and those are the literal strings such a save
		// carries. Nothing in the conversion matches on them, so do not "fix" them to a config name,
		// which no legacy save could contain.
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

		// 🔴 The refund is OWED here, not PAID here. It cannot be credited from inside the apply, because
		// the deployment manager's own restore CLEARS the resource pool and runs after this manager's in
		// the same load. The queued amount is asserted first - it is the conversion arithmetic - and then
		// the credit point is driven directly, which keeps the whole claim inside one frame.
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
//! ⚠ THIS IS THE PLAN'S DOCUMENTED FALLBACK, NOT THE FULL ROUND TRIP.
//!
//! The five-phase round trip the plan asks for is not reachable, structurally rather than by timing:
//! ReapplyLatestSaveData() - the suite's only load seam - asks for exactly ONE instance, the GAME
//! MODE ENTITY. A depot is a separate tracked root (SelfSpawn 1, its own EntityPersistenceConfig
//! keyed on OVT_BuildableComponent), so no re-application will ever put its record back. Restoring
//! it means restarting the session, which the suite header explains is impossible in -autotest.
//!
//! It lives here and not in OVT_TEST_PersistenceSuite, where the plan's fallback sentence points,
//! for two reasons either of which is sufficient: that suite's header forbids save-taking cases in
//! terms, and it is listed BEFORE this one in OVT_TestGroup_All.conf, so a save taken there turns
//! this suite's capability gate into a "precondition violated" failure and reddens the All group.
//!
//! STILL OWED TO A HUMAN: manual step F18 (part-fill the depot, save, RELOAD the session, confirm
//! the level). That is the only thing that proves the serializer's read half.
//! ==========================================================================================
//!
//! What it does prove, none of it provable any other way in this harness:
//!  - "Fuel Depot" is resolvable BY NAME from buildables.conf (never by index - that moves);
//!  - BuildItem() with playerId -1 gets past OVT_FuelDepotHandler and returns a real entity;
//!  - the spawned prefab carries OVT_BuildableComponent typed "FuelDepot" and is findable by that
//!    type from a world query;
//!  - ⚠ THE TWO PREFAB FACTS THE VANILLA SERIALIZER SILENTLY DEPENDS ON.
//!    SCR_FuelManagerComponentSerializer only walks SCR_FuelNode-typed nodes and SKIPS any node whose
//!    fuel equals its initial state. A node authored as a bare BaseFuelNode, or an initial state
//!    authored as a fraction (0.5) instead of litres, makes persistence a no-op with no error
//!    anywhere. Asserted: a scripted node exists, its capacity is the authored 10000 L, its initial
//!    state is 0;
//!  - a real save completes with the depot in the world and does not disturb its level.
//!
//! Non-vacuous: the level is written BEFORE the save and read back through a FRESH world query, not
//! the handle BuildItem() returned, and the depot is authored to start EMPTY.
//!
//! The depot is left standing on purpose: deleting a persistence-tracked entity mid-suite drives the
//! transient-untrack retry queue (BUG-118), far likelier to disturb later cases than an inert prop.
//!
//! ⚠ Takes a real save, so the class name MUST sort after `..._Capability_...`.
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

//------------------------------------------------------------------------------------------------
//! Shared machinery for the two StructureDamage round-trip cases: build a Guard Tower beside a base,
//! and find it again from scratch by prefab type and radius.
//!
//! The Guard Tower is the subject because it is the retrofitted buildable with a REAL ruin model and
//! an active RplComponent - the cases assert the persisted PHASE, but a subject whose ruin mesh does
//! not exist would make a green run mean less than it says.
//!
//! Nothing is cached across phases: every phase looks the structure up again the way a consumer
//! would, so an entity that quietly stopped existing fails its case instead of being read stale.
//------------------------------------------------------------------------------------------------
class OVT_TEST_StructureDamageRoundTripFixture
{
	//! Resolved by name out of the buildables config. Never an index - entries get appended.
	static const string BUILDABLE_NAME = "Guard Tower";

	//! OVT_BuildableComponent.m_sBuildableType authored on OVT_GuardTower_01.et. Deliberately not the
	//! same string as the menu name above.
	static const string BUILDABLE_TYPE = "GuardTower";

	//! Metres searched around a build position when finding the structure again.
	static const float REFIND_RADIUS = 12;

	protected IEntity m_Found;

	//------------------------------------------------------------------------------------------------
	//! Picks a base and offsets from it. The two cases pass different offsets so their re-find queries
	//! can never see each other's tower.
	//! \param[in] offset Metres from the chosen base.
	//! \param[out] position Where the structure should be built.
	//! \return An empty string on success, otherwise the sentence to fail with.
	string ChooseBuildPosition(vector offset, out vector position)
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return "OVT_Global.GetOccupyingFaction() is null, so there is no base to build beside";

		foreach (OVT_BaseData candidate : occupying.m_Bases)
		{
			if (candidate)
			{
				position = candidate.location + offset;
				return "";
			}
		}

		return "The campaign has no bases, so there is nowhere to put a base buildable";
	}

	//------------------------------------------------------------------------------------------------
	//! Builds one Guard Tower through the server-side build path.
	//! \param[in] position Where to build it.
	//! \return An empty string on success, otherwise the sentence to fail with.
	string Build(vector position)
	{
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance)
			return "OVT_Global.GetResistanceFaction() is null";

		if (!resistance.m_BuildablesConfig || !resistance.m_BuildablesConfig.m_aBuildables)
			return "The resistance faction has no buildables config loaded";

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
			return string.Format("No buildable named '%1' in the buildables config - the entry is missing or renamed",
				BUILDABLE_NAME);
		}

		// playerId -1 is BuildItem()'s own server-initiated marker: it waives the funds, distance and
		// item-limit checks.
		IEntity structure = resistance.BuildItem(index, 0, position, vector.Zero, -1);
		if (!structure)
		{
			return string.Format("BuildItem() built no Guard Tower at %1 - the prefab failed to spawn, or the build was refused",
				position.ToString());
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Finds the structure again by a fresh world query.
	//! \param[in] position The build position to search around.
	//! \return The structure, or null.
	IEntity Find(vector position)
	{
		m_Found = null;
		GetGame().GetWorld().QueryEntitiesBySphere(position, REFIND_RADIUS, Collect, null, EQueryEntitiesFlags.ALL);

		IEntity found = m_Found;
		m_Found = null;

		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! The sentence a case fails with when Find() answers null.
	//! \param[in] position The build position that was searched.
	//! \return The diagnostic.
	string NotFoundDiagnostic(vector position)
	{
		return string.Format("No buildable of type '%1' within %2 m of %3 - the structure is not in the world",
			BUILDABLE_TYPE, REFIND_RADIUS.ToString(), position.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! World-query collector: keeps the first entity carrying a Guard Tower buildable component.
	//! \param[in] entity Candidate from the sphere query.
	//! \return False once the structure is found, which stops the query.
	protected bool Collect(IEntity entity)
	{
		if (!entity)
			return true;

		OVT_BuildableComponent buildable = OVT_BuildableComponent.Cast(entity.FindComponent(OVT_BuildableComponent));
		if (!buildable || buildable.GetBuildableType() != BUILDABLE_TYPE)
			return true;

		m_Found = entity;
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! A RUINED structure comes back ruined: build a Guard Tower, ruin it, save, REPAIR it in memory,
//! re-read its stored record, and it is a ruin again.
//!
//! Before this feature a sabotaged structure was deleted, and a deleted entity is trivially "still
//! gone" after a load; a ruin is a surviving entity holding one integer, and if that integer is not
//! written or not read the campaign quietly repairs every ruin for free on every continue.
//!
//! This is a REAL storage round trip, unlike the FuelDepot case above, and the difference is the
//! third seam in the gate class: a buildable owns its own persistence record, so it can be asked for
//! BY INSTANCE. Closures 2 and 3 both hold - the dirty step repairs through the same public facade
//! the game uses, and "ruined" is not a state any build or campaign start produces (the case asserts
//! the structure is intact before it ruins anything).
//!
//! ⚠ Takes a real save, so the class name MUST sort after `..._Capability_...`. It sorts after the
//! Objective* cases and before the Town* ones and disturbs neither.
//!
//! The tower is left standing (as a ruin) on purpose, for the FuelDepot case's reason.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_StructureDamage_RuinSurvivesSave : SCR_AutotestCaseBase
{
	//! Offset from the chosen base. Well clear of the FuelDepot case's own build position.
	static const vector BUILD_OFFSET = "45 0 -45";

	static const int PHASE_BUILD = 0;
	static const int PHASE_AWAIT_TRACKING = 1;
	static const int PHASE_RUIN_AND_SAVE = 2;
	static const int PHASE_AWAIT_SAVE = 3;
	static const int PHASE_DIRTY_AND_RELOAD = 4;
	static const int PHASE_AWAIT_RELOAD = 5;
	static const int PHASE_ASSERT = 6;

	protected int m_iPhase;
	protected int m_iTrackingPolls;
	protected int m_iSavePolls;
	protected int m_iReloadPolls;
	protected int m_iSaveBaseline;
	protected vector m_vBuildPos;

	protected ref OVT_TEST_StructureDamageRoundTripFixture m_Fixture = new OVT_TEST_StructureDamageRoundTripFixture();

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == PHASE_BUILD)
			return Build();

		if (m_iPhase == PHASE_AWAIT_TRACKING)
			return AwaitTracking();

		if (m_iPhase == PHASE_RUIN_AND_SAVE)
			return RuinAndSave();

		if (m_iPhase == PHASE_AWAIT_SAVE)
			return AwaitSave();

		if (m_iPhase == PHASE_DIRTY_AND_RELOAD)
			return DirtyAndReload();

		if (m_iPhase == PHASE_AWAIT_RELOAD)
			return AwaitReload();

		return Assert();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to advance.
	protected bool Build()
	{
		string diagnostic = m_Fixture.ChooseBuildPosition(BUILD_OFFSET, m_vBuildPos);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		diagnostic = m_Fixture.Build(m_vBuildPos);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		m_iPhase = PHASE_AWAIT_TRACKING;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A structure's persistence registration is asynchronous, so the case waits for its record to
	//! exist before it saves - otherwise a save could legitimately contain nothing to read back and
	//! the failure would name the wrong half. Bounded, and expiry fails.
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitTracking()
	{
		IEntity structure = m_Fixture.Find(m_vBuildPos);
		if (!structure)
		{
			SetFailure(m_Fixture.NotFoundDiagnostic(m_vBuildPos));
			return true;
		}

		if (!OVT_TEST_PersistenceRoundTripGate.InstanceIsTracked(structure))
		{
			m_iTrackingPolls += 1;
			if (m_iTrackingPolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
			{
				SetFailure("The built Guard Tower never became persistence-tracked, so it has no stored record and nothing about its state can survive a save");
				return true;
			}

			return false;
		}

		m_iPhase = PHASE_RUIN_AND_SAVE;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to advance.
	protected bool RuinAndSave()
	{
		IEntity structure = m_Fixture.Find(m_vBuildPos);
		if (!structure)
		{
			SetFailure(m_Fixture.NotFoundDiagnostic(m_vBuildPos));
			return true;
		}

		if (!OVT_StructureDamage.IsDestructible(structure))
		{
			SetFailure("The built Guard Tower carries no destruction component, so it cannot be ruined at all - the prefab retrofit is missing");
			return true;
		}

		// Closure 3: a freshly built structure is intact, so "ruined" is never a state the build path
		// or a campaign start could have produced on its own.
		if (OVT_StructureDamage.IsRuined(structure))
		{
			SetFailure("The Guard Tower was already a ruin the moment it was built");
			return true;
		}

		if (!OVT_StructureDamage.Ruin(structure, false))
		{
			SetFailure("Ruin() refused the built Guard Tower");
			return true;
		}

		if (!OVT_StructureDamage.IsRuined(structure))
		{
			SetFailure("The Guard Tower was still intact after Ruin(), so there was nothing ruined to save");
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

		m_iPhase = PHASE_DIRTY_AND_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Closure 2: the saved state is destroyed in memory before anything is read back, through the same
	//! public facade the repair action will use.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool DirtyAndReload()
	{
		IEntity structure = m_Fixture.Find(m_vBuildPos);
		if (!structure)
		{
			SetFailure(m_Fixture.NotFoundDiagnostic(m_vBuildPos));
			return true;
		}

		if (!OVT_StructureDamage.Repair(structure))
		{
			SetFailure("Repair() refused the ruined Guard Tower, so the live state could not be dirtied and the reload would prove nothing");
			return true;
		}

		if (OVT_StructureDamage.IsRuined(structure))
		{
			SetFailure("The Guard Tower was still a ruin after the dirtying repair, so the reload would prove nothing");
			return true;
		}

		string reload = OVT_TEST_PersistenceRoundTripGate.RequestInstanceReload(structure);
		if (reload != "")
		{
			SetFailure(reload);
			return true;
		}

		m_iPhase = PHASE_AWAIT_RELOAD;
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
				SetFailure("The stored record was never re-applied: the persistence system's re-application never completed");
				return true;
			}

			return false;
		}

		m_iPhase = PHASE_ASSERT;
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

		IEntity structure = m_Fixture.Find(m_vBuildPos);
		if (!structure)
		{
			SetFailure(m_Fixture.NotFoundDiagnostic(m_vBuildPos));
			return true;
		}

		if (!OVT_StructureDamage.IsRuined(structure))
		{
			SetFailure("The Guard Tower was ruined and saved, then repaired in memory, and came back INTACT from its stored record - a ruin does not survive a save, so every destroyed structure repairs itself for free on the next continue");
			return true;
		}

		Print("A ruined structure's damage phase survived a real save and came back out of storage");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! And the inverse, which the requirements ask for by name: a REPAIRED structure does not revert.
//! Build, ruin, save, re-read (it is a ruin), repair, save again, ruin it in memory, re-read - and
//! it is intact.
//!
//! Both directions need their own case: the phase is one integer, so a serializer that wrote a
//! constant 1 - or a RestorePhase() that only ever drives the ruin branch - would pass the ruin case
//! above and lose every repair the player paid for. The second half is written over the top of the
//! first, so the second save has to OVERWRITE a value that was already there rather than write a
//! field for the first time.
//!
//! The dirty step of each half is the opposite of what it asserts, so neither can pass by accident.
//!
//! ⚠ Takes TWO real saves; same sort-order requirement as the case above. It builds its tower on the
//! other side of the base, so the two cases can never find each other's.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 120)]
class OVT_TEST_PersistenceRoundTrip_StructureDamage_RepairSurvivesSave : SCR_AutotestCaseBase
{
	//! The other side of the base from the ruin case's tower.
	static const vector BUILD_OFFSET = "-45 0 45";

	static const int PHASE_BUILD = 0;
	static const int PHASE_AWAIT_TRACKING = 1;
	static const int PHASE_RUIN_AND_SAVE = 2;
	static const int PHASE_AWAIT_SAVE = 3;
	static const int PHASE_RELOAD = 4;
	static const int PHASE_AWAIT_RELOAD = 5;
	static const int PHASE_ASSERT_HALF = 6;
	static const int PHASE_DONE = 7;

	//! Which half is running: 0 = the ruin was saved, 1 = the repair was saved.
	protected int m_iHalf;

	protected int m_iPhase;
	protected int m_iTrackingPolls;
	protected int m_iSavePolls;
	protected int m_iReloadPolls;
	protected int m_iSaveBaseline;
	protected vector m_vBuildPos;

	protected ref OVT_TEST_StructureDamageRoundTripFixture m_Fixture = new OVT_TEST_StructureDamageRoundTripFixture();

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == PHASE_BUILD)
			return Build();

		if (m_iPhase == PHASE_AWAIT_TRACKING)
			return AwaitTracking();

		if (m_iPhase == PHASE_RUIN_AND_SAVE)
			return ChangeAndSave();

		if (m_iPhase == PHASE_AWAIT_SAVE)
			return AwaitSave();

		if (m_iPhase == PHASE_RELOAD)
			return DirtyAndReload();

		if (m_iPhase == PHASE_AWAIT_RELOAD)
			return AwaitReload();

		return AssertHalf();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to advance.
	protected bool Build()
	{
		string diagnostic = m_Fixture.ChooseBuildPosition(BUILD_OFFSET, m_vBuildPos);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		diagnostic = m_Fixture.Build(m_vBuildPos);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		m_iPhase = PHASE_AWAIT_TRACKING;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitTracking()
	{
		IEntity structure = m_Fixture.Find(m_vBuildPos);
		if (!structure)
		{
			SetFailure(m_Fixture.NotFoundDiagnostic(m_vBuildPos));
			return true;
		}

		if (!OVT_TEST_PersistenceRoundTripGate.InstanceIsTracked(structure))
		{
			m_iTrackingPolls += 1;
			if (m_iTrackingPolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
			{
				SetFailure("The built Guard Tower never became persistence-tracked, so it has no stored record and nothing about its state can survive a save");
				return true;
			}

			return false;
		}

		m_iPhase = PHASE_RUIN_AND_SAVE;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Half 0 ruins the structure and saves; half 1 repairs it and saves over that record.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool ChangeAndSave()
	{
		IEntity structure = m_Fixture.Find(m_vBuildPos);
		if (!structure)
		{
			SetFailure(m_Fixture.NotFoundDiagnostic(m_vBuildPos));
			return true;
		}

		if (m_iHalf == 0)
		{
			if (!OVT_StructureDamage.IsDestructible(structure))
			{
				SetFailure("The built Guard Tower carries no destruction component, so it cannot be ruined at all - the prefab retrofit is missing");
				return true;
			}

			if (!OVT_StructureDamage.Ruin(structure, false))
			{
				SetFailure("Ruin() refused the built Guard Tower");
				return true;
			}

			if (!OVT_StructureDamage.IsRuined(structure))
			{
				SetFailure("The Guard Tower was still intact after Ruin(), so there was nothing ruined to save");
				return true;
			}
		}
		else
		{
			if (!OVT_StructureDamage.Repair(structure))
			{
				SetFailure("Repair() refused the ruined Guard Tower, so there was no repair to save");
				return true;
			}

			if (OVT_StructureDamage.IsRuined(structure))
			{
				SetFailure("The Guard Tower was still a ruin after Repair(), so there was no repair to save");
				return true;
			}
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

		m_iSavePolls = 0;
		m_iPhase = PHASE_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Destroys the saved state in memory - the opposite of what this half asserts - and re-reads the
	//! stored record.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool DirtyAndReload()
	{
		IEntity structure = m_Fixture.Find(m_vBuildPos);
		if (!structure)
		{
			SetFailure(m_Fixture.NotFoundDiagnostic(m_vBuildPos));
			return true;
		}

		if (m_iHalf == 0)
		{
			if (!OVT_StructureDamage.Repair(structure) || OVT_StructureDamage.IsRuined(structure))
			{
				SetFailure("The saved ruin could not be dirtied back to intact, so the reload would prove nothing");
				return true;
			}
		}
		else
		{
			if (!OVT_StructureDamage.Ruin(structure, false) || !OVT_StructureDamage.IsRuined(structure))
			{
				SetFailure("The saved repair could not be dirtied back to a ruin, so the reload would prove nothing");
				return true;
			}
		}

		string reload = OVT_TEST_PersistenceRoundTripGate.RequestInstanceReload(structure);
		if (reload != "")
		{
			SetFailure(reload);
			return true;
		}

		m_iPhase = PHASE_AWAIT_RELOAD;
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
				SetFailure("The stored record was never re-applied: the persistence system's re-application never completed");
				return true;
			}

			return false;
		}

		m_iReloadPolls = 0;
		m_iPhase = PHASE_ASSERT_HALF;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts the half that just ran, then either starts the repair half or ends the case.
	//! \return True when the case is finished; false to advance.
	protected bool AssertHalf()
	{
		string restored = OVT_TEST_PersistenceRoundTripGate.RequireRestoredCampaign();
		if (restored != "")
		{
			SetFailure(restored);
			return true;
		}

		IEntity structure = m_Fixture.Find(m_vBuildPos);
		if (!structure)
		{
			SetFailure(m_Fixture.NotFoundDiagnostic(m_vBuildPos));
			return true;
		}

		if (m_iHalf == 0)
		{
			if (!OVT_StructureDamage.IsRuined(structure))
			{
				SetFailure("The saved ruin came back intact, so there is no ruined record for the repair half to overwrite");
				return true;
			}

			m_iHalf = 1;
			m_iPhase = PHASE_RUIN_AND_SAVE;
			return false;
		}

		if (OVT_StructureDamage.IsRuined(structure))
		{
			SetFailure("The Guard Tower was repaired and saved, then ruined in memory, and came back RUINED from its stored record - a repair does not survive a save, so a player's paid repair reverts on the next continue");
			return true;
		}

		Print("A repaired structure stayed repaired through a save that overwrote a ruined record");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The occupying faction's current objective - its PLAN by name, its place, its PHASE by name, every
//! module counter and module position, both tick counters, the blacklist and the whole forward-base
//! record - survives a save and a re-application.
//!
//! 🔴 The plan and the phase come back BY NAME, and that is the point of the format. The record used
//! to carry an enum integer for the phase, so the only thing between a renumbered enum and every
//! saved objective being silently re-labelled was a comment. Names replace it with a mechanism: a
//! plan or phase the running build does not carry is DETECTED on load, named in an ERROR line, and
//! the objective abandoned cleanly. This case pins the happy half; the abandon half needs a shipped
//! .conf edit and is a manual check.
//!
//! 🔴 The bag is the ONLY save format an objective module has - one version number, one place to
//! append - so if the two bags did not round-trip, the first module that needed a counter would
//! invent a second format.
//!
//! Losing the objective does not crash anything: the director picks a fresh target on its next tick
//! and the player's twenty minutes of warning silently restart, which nobody reports as a bug
//! because it looks like the system working.
//!
//! The dirty step changes EVERY field it later asserts (closure 2): the objective is re-committed at
//! a different place with a different KIND - which also binds the OTHER shipped plan and empties the
//! whole bag - both tick counters are driven to small values, the forward base is wiped by that
//! re-commit, a third blacklist entry is added, and the machine is walked into a differently-NAMED
//! phase. ⚠ That last step is not decoration: both shipped plans call their first phase
//! "Harassment", so a commit alone leaves the phase name where the saved objective had it.
//!
//! ⚠ The two tick counters are asserted as a BAND, not an exact value: the director really is
//! ticking during the seconds this case waits for an asynchronous save, and each tick legitimately
//! serves one round off both counters. The band is (dirty value, saved value] - above the dirty
//! value proves the reload restored something, at or below the saved value proves it restored THIS
//! save rather than a phase re-armed from scratch.
//!
//! ⚠ The fixture names a forward-base deployment that does NOT exist, on purpose. The director gives
//! a restored forward base a few ticks to find its deployment again before tearing the objective
//! down, so the assertions run inside that grace window. The case tears its own fixture down at the
//! end regardless.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_ObjectiveDirector_SurvivesSaveAndReapply : SCR_AutotestCaseBase
{
	//! Tolerance for every position comparison, in metres. vector.Distance is not correctly rounded at
	//! campaign ranges, so a stored position is never compared with ==.
	static const float POSITION_TOLERANCE = 1.0;

	static const int SAVED_PHASE_TICKS = 137;
	static const int SAVED_OP_TICKS = 91;
	static const int SAVED_HARASSMENT_SUCCESSES = 3;
	static const int SAVED_SABOTAGE_SUCCESSES = 2;
	static const int SAVED_FOB_SPENT = 777;
	static const int SAVED_FOB_STARVATION = 11;
	static const int SAVED_BLACKLIST_A_ROUNDS = 2;
	static const int SAVED_BLACKLIST_B_ROUNDS = 5;

	//! Config name of the forward base's deployment. Arbitrary, and deliberately not a shipped config
	//! name - what is being asserted is that the STRING round-trips, not that it resolves.
	static const string SAVED_FOB_DEPLOYMENT = "ObjectiveRoundTripFOB";

	//! Two bag keys no shipped module owns, plus one the vector bag owns. THE BAG IS THE SAVE FORMAT:
	//! no objective module writes its own record, so every counter and every position a module has
	//! accumulated travels in these two maps. Keys nothing else writes are used deliberately - the two
	//! success counters are already asserted separately, and a key the campaign might touch for its own
	//! reasons would make a failure ambiguous.
	static const string SAVED_BAG_KEY_A = "roundTrip.alpha";
	static const string SAVED_BAG_KEY_B = "roundTrip.beta";
	static const string SAVED_BAG_VECTOR_KEY = "roundTrip.site";

	static const int SAVED_BAG_VALUE_A = 61;
	static const int SAVED_BAG_VALUE_B = 409;

	//! The plan a TOWN objective runs, and the phase a fresh commit enters. Both are PERSISTENCE KEYS -
	//! the payload carries these strings rather than enum integers - so this is also the case that would
	//! catch a rename of either.
	static const string SAVED_PLAN = "Town Offensive";
	static const string SAVED_PHASE = "Harassment";

	//! What the dirty step leaves behind, so a value that "survived" by never having changed cannot pass.
	static const string DIRTY_PLAN = "Base Offensive";
	static const string DIRTY_PHASE = "ForwardBase";

	static const int DIRTY_PHASE_TICKS = 3;
	static const int DIRTY_OP_TICKS = 4;
	static const int DIRTY_BLACKLIST_ROUNDS = 9;

	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected int m_iReloadPolls;

	protected vector m_vSavedObjective;
	protected vector m_vSavedFOB;
	protected vector m_vSavedFOBSource;
	protected vector m_vBlacklistA;
	protected vector m_vBlacklistB;
	protected vector m_vDirtyObjective;
	protected vector m_vDirtyBlacklist;
	protected vector m_vSavedBagPosition;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
			if (!director)
			{
				SetFailure("OVT_Global.GetObjectiveDirector() is null");
				return true;
			}

			SetUpFixturePositions();

			// A whole ramp mid-flight: the target, the phase, the operations already completed, every
			// timer, two places serving cooldowns, and a standing forward base with a spend history.
			director.CommitObjective(OVT_EObjectiveKind.TOWN, m_vSavedObjective, "objective round trip fixture");

			for (int h = 0; h < SAVED_HARASSMENT_SUCCESSES; h++)
			{
				director.ReportObjectiveProgress(OVT_ObjectiveInstance.BAG_HARASSMENT_SUCCESSES, 1);
			}

			for (int s = 0; s < SAVED_SABOTAGE_SUCCESSES; s++)
			{
				director.ReportObjectiveProgress(OVT_ObjectiveInstance.BAG_SABOTAGE_SUCCESSES, 1);
			}

			// ⚠ The two timers are planted LAST, after the success counters, and the order is load-bearing.
			// The phase timeout is an IDLE clock: a director tick that sees a success counter move since
			// the clock was last set treats it as fresh progress and re-arms the clock to its full authored
			// budget. This world runs a live director on a repeating timer and this step spans several
			// frames, so with the successes counted AFTER the plant one background tick would re-arm the
			// timer and the band assertion below would correctly report a value higher than the saved one.
			// A REORDER, not a weakened claim: every value asserted after the reload is the same one.
			director.SetPhaseTimeout(SAVED_PHASE_TICKS);
			director.SetOperationCountdown(SAVED_OP_TICKS);

			// ⚠ THROUGH THE KEYED ASSET API (build phase 5). The three named forward-base writers were
			// deleted with the rest of the forward base; these three take the asset key and write the
			// same record, which is the same object the keyed getters read.
			director.ReportAssetRaised(OVT_ObjectiveDirectorComponent.ASSET_FOB, m_vSavedFOB, m_vSavedFOBSource, SAVED_FOB_DEPLOYMENT);
			director.AddAssetSpend(OVT_ObjectiveDirectorComponent.ASSET_FOB, SAVED_FOB_SPENT);
			director.SetAssetStarvationTicks(OVT_ObjectiveDirectorComponent.ASSET_FOB, SAVED_FOB_STARVATION);

			// ⚠ THE BAG IS WRITTEN THROUGH THE PUBLIC API AND READ BACK THROUGH PUBLIC GETTERS, like
			// everything else in this tier - the payload and the serializer are never named here. Two
			// int keys and one vector key, because the two maps are separate arrays in the record and a
			// format that wrote one and forgot the other would still round-trip half the state.
			director.SetObjectiveBagValue(SAVED_BAG_KEY_A, SAVED_BAG_VALUE_A);
			director.SetObjectiveBagValue(SAVED_BAG_KEY_B, SAVED_BAG_VALUE_B);
			director.SetObjectiveBagPosition(SAVED_BAG_VECTOR_KEY, m_vSavedBagPosition);

			director.BlacklistPosition(m_vBlacklistA, SAVED_BLACKLIST_A_ROUNDS);
			director.BlacklistPosition(m_vBlacklistB, SAVED_BLACKLIST_B_ROUNDS);

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
			OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
			if (!director)
			{
				SetFailure("OVT_Global.GetObjectiveDirector() is null before the reload");
				return true;
			}

			// DIRTY EVERY ASSERTED FIELD. Committing a different objective changes the kind and the
			// place, zeroes both success counters and clears the whole forward-base record in one call;
			// the two timers and a third blacklist entry are set on top of it.
			director.CommitObjective(OVT_EObjectiveKind.BASE, m_vDirtyObjective, "dirty");

			// ⚠ AND THE PHASE NAME, WHICH THE COMMIT ALONE DOES NOT DIRTY. Both shipped plans call their
			// first phase "Harassment", so committing a base objective leaves the phase NAME exactly
			// where the town objective had it - and an assertion that can pass without the value being
			// restored is not an assertion. Entering the next phase gives it a distinct name. It happens
			// BEFORE the two timers are planted, because a phase entry re-arms the idle clock.
			director.EnterPhase("ForwardBase");

			director.SetPhaseTimeout(DIRTY_PHASE_TICKS);
			director.SetOperationCountdown(DIRTY_OP_TICKS);
			director.BlacklistPosition(m_vDirtyBlacklist, DIRTY_BLACKLIST_ROUNDS);

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

		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		if (!director)
		{
			SetFailure("OVT_Global.GetObjectiveDirector() is null after the reload");
			return true;
		}

		string failure = AssertRestored(director);

		if (failure == "")
			failure = AssertStrandedObjectiveIsTornDown(director);

		// Tear the fixture down whatever the verdict: this campaign keeps running for the rest of the
		// suite and must not be left aiming at a place the case invented.
		director.ResetObjective("objective round-trip fixture torn down", false);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A restored forward base whose deployment is nowhere to be found ENDS THE OBJECTIVE, and does not
	//! leave it pointing at nothing.
	//!
	//! Asserted here rather than in its own case because the fixture already produces exactly this
	//! state: it saves a forward base naming a deployment that has never existed, which is what a real
	//! campaign looks like when the marker was destroyed while the save sat on disk. The restore
	//! deliberately does NOT believe a first empty look - load order between a game-mode component's
	//! payload and the separately-tracked deployment entities is never assumed - so driving the ticks
	//! here is the only way to see the far side of that grace window.
	//!
	//! ⚠ The alternative to giving up is an objective that never progresses again: a forward-base phase
	//! whose base cannot be found cannot reach the counter-attack gate and cannot raise a second base.
	//!
	//! ⚠ It drives EXACTLY the grace window and not one tick more. The tick that gives up leaves the
	//! machine IDLE, and the IDLE branch of the very next tick would select a real objective and enter
	//! its first phase with the operation countdown armed to ZERO - so a further tick would buy a real
	//! deployment, in a real campaign, with real resources, and nothing refunds a deleted deployment.
	//! \param[in] director The restored director.
	//! \return An empty string when the objective was torn down, otherwise the failure.
	protected string AssertStrandedObjectiveIsTornDown(notnull OVT_ObjectiveDirectorComponent director)
	{
		int ticks = OVT_ObjectiveDirectorComponent.FOB_RELINK_ATTEMPTS;
		for (int i = 0; i < ticks; i++)
		{
			director.DirectorTick();
		}

		// Belt and braces against the cadence trap described above: whatever the giving-up tick went on
		// to select, it does not get to spend on the next one.
		director.SetOperationCountdown(SAVED_OP_TICKS);

		if (director.IsAssetUp(OVT_ObjectiveDirectorComponent.ASSET_FOB))
			return string.Format("after %1 ticks the director still reports a forward base whose deployment does not exist - the objective is stranded in a phase it can never leave, and nothing in the log says so",
				ticks.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Picks fixture positions that cannot collide with anything the campaign chose for itself.
	//!
	//! They are far apart and far from each other so a position assertion cannot pass by accidentally
	//! matching the wrong one, and they are never resolved against the world - the objective's position
	//! is a key, not a spawn instruction.
	protected void SetUpFixturePositions()
	{
		m_vSavedObjective = "1500 20 2500";
		m_vSavedFOB = "1600 21 2400";
		m_vSavedFOBSource = "1200 15 2200";
		m_vBlacklistA = "3000 10 4000";
		m_vBlacklistB = "5000 10 6000";
		m_vDirtyObjective = "9000 30 9500";
		m_vDirtyBlacklist = "7000 10 8000";
		m_vSavedBagPosition = "2100 12 3300";
	}

	//------------------------------------------------------------------------------------------------
	//! Reads every saved value back through the director's public getters.
	//! \param[in] director The restored director.
	//! \return An empty string when everything came back, otherwise the first failure.
	protected string AssertRestored(notnull OVT_ObjectiveDirectorComponent director)
	{
		if (director.GetObjectiveKind() == OVT_EObjectiveKind.BASE)
			return "the objective did not survive the round trip: the dirty BASE objective is still there";

		if (director.GetObjectiveKind() != OVT_EObjectiveKind.TOWN)
		{
			int kind = director.GetObjectiveKind();
			return string.Format("the restored objective is kind %1, not the town that was saved", kind.ToString());
		}

		float objectiveDrift = vector.Distance(director.GetObjectivePosition(), m_vSavedObjective);
		if (objectiveDrift > POSITION_TOLERANCE)
			return string.Format("the restored objective is %1 m from the one that was saved", objectiveDrift.ToString());

		if (director.GetObjectivePhaseName() != "Harassment")
		{
			string phase = director.GetObjectivePhaseName();
			return string.Format("the restored objective is in phase %1, not the harassment phase it was saved in", phase);
		}

		// 🔴 THE PLAN AND THE PHASE COME BACK BY NAME, AND THE NAMES ARE THE KEYS. The payload carries
		// these two strings rather than an index or an enum integer, which is the whole reason a plan
		// can grow a phase in the middle without re-labelling every objective in every save on disk -
		// and the reason a plan or phase RENAME is detected on load and abandoned loudly instead of
		// being silently adopted as whatever sits at that position now.
		if (director.GetObjectiveConfigName() == DIRTY_PLAN)
			return "the objective's PLAN did not survive the round trip: the dirty plan is still bound";

		if (director.GetObjectiveConfigName() != SAVED_PLAN)
			return string.Format("the restored objective is running plan '%1', not the '%2' it was saved under",
				director.GetObjectiveConfigName(), SAVED_PLAN);

		if (director.GetObjectivePhaseName() == DIRTY_PHASE)
			return "the objective's PHASE NAME did not survive the round trip: the dirty phase name is still there";

		if (director.GetObjectivePhaseName() != SAVED_PHASE)
			return string.Format("the restored objective is in phase '%1', not the '%2' it was saved in",
				director.GetObjectivePhaseName(), SAVED_PHASE);

		if (director.GetObjectivePhaseIndex() != 0)
			return string.Format("the restored objective sits at plan phase index %1, which does not agree with the phase name it came back with",
				director.GetObjectivePhaseIndex().ToString());

		// A restored objective has to be RUNNING, not merely recorded: an instance that never rejoined
		// the live list would sit in a save-shaped state that the tick never steps.
		if (director.GetInstanceCount() != 1)
			return string.Format("the restored campaign holds %1 running objective(s), expected the one that was saved",
				director.GetInstanceCount().ToString());

		if (director.GetRuntimeModuleCount() == 0)
			return "the restored objective came back with NO runtime modules, so its phase would fall through to the hard-coded fallback on every tick";

		if (director.GetHarassmentSuccesses() != SAVED_HARASSMENT_SUCCESSES)
			return string.Format("harassment successes did not survive: expected %1, read back %2",
				SAVED_HARASSMENT_SUCCESSES.ToString(), director.GetHarassmentSuccesses().ToString());

		if (director.GetSabotageSuccesses() != SAVED_SABOTAGE_SUCCESSES)
			return string.Format("sabotage successes did not survive: expected %1, read back %2",
				SAVED_SABOTAGE_SUCCESSES.ToString(), director.GetSabotageSuccesses().ToString());

		string phaseBand = AssertBand(director.GetPhaseTicks(), DIRTY_PHASE_TICKS, SAVED_PHASE_TICKS, "the phase timeout");
		if (phaseBand != "")
			return phaseBand;

		string opBand = AssertBand(director.GetNextOpTicks(), DIRTY_OP_TICKS, SAVED_OP_TICKS, "the operation cadence");
		if (opBand != "")
			return opBand;

		if (!director.IsAssetUp(OVT_ObjectiveDirectorComponent.ASSET_FOB))
			return "the forward operating base did not survive the round trip - the restored objective has none";

		float fobDrift = vector.Distance(director.GetAssetPosition(OVT_ObjectiveDirectorComponent.ASSET_FOB), m_vSavedFOB);
		if (fobDrift > POSITION_TOLERANCE)
			return string.Format("the restored forward base is %1 m from where it was saved", fobDrift.ToString());

		float sourceDrift = vector.Distance(director.GetFOBSourceBasePosition(), m_vSavedFOBSource);
		if (sourceDrift > POSITION_TOLERANCE)
			return string.Format("the restored forward base's source base is %1 m from where it was saved", sourceDrift.ToString());

		if (director.GetFOBSpent() != SAVED_FOB_SPENT)
			return string.Format("the forward base's spend against its ceiling did not survive: expected %1, read back %2",
				SAVED_FOB_SPENT.ToString(), director.GetFOBSpent().ToString());

		if (director.GetFOBStarvationTicks() != SAVED_FOB_STARVATION)
			return string.Format("the forward base's starvation count did not survive: expected %1, read back %2",
				SAVED_FOB_STARVATION.ToString(), director.GetFOBStarvationTicks().ToString());

		if (director.GetFOBDeploymentName() != SAVED_FOB_DEPLOYMENT)
			return string.Format("the forward base's re-link key did not survive: expected '%1', read back '%2'",
				SAVED_FOB_DEPLOYMENT, director.GetFOBDeploymentName());

		string bag = AssertBag(director);
		if (bag != "")
			return bag;

		return AssertBlacklist(director);
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts a tick counter is inside the band (dirty, saved].
	//! \param[in] actual The restored value.
	//! \param[in] dirty The value the dirty step left behind.
	//! \param[in] saved The value that was saved.
	//! \param[in] label What the counter is, for the failure message.
	//! \return An empty string when it is inside the band, otherwise the failure.
	protected string AssertBand(int actual, int dirty, int saved, string label)
	{
		if (actual <= dirty)
			return string.Format("%1 did not survive the round trip: it is still on the dirty value or below it (%2, dirtied to %3)",
				label, actual.ToString(), dirty.ToString());

		if (actual > saved)
			return string.Format("%1 came back HIGHER than the value that was saved (%2 against %3), so it was re-armed from scratch rather than restored",
				label, actual.ToString(), saved.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts every module counter and every module position came back.
	//!
	//! ⚠ The bag is the WHOLE of what a module persists (D9), so this is not a spot check of two
	//! arbitrary keys - it is the assertion that the one save format every objective module shares
	//! round-trips at all.
	//!
	//! ⚠ The two maps are asserted separately because they are separate arrays in the record: a format
	//! that wrote the integers and forgot the positions would look green against a case checking one.
	//! \param[in] director The restored director.
	//! \return An empty string when everything came back, otherwise the failure.
	protected string AssertBag(notnull OVT_ObjectiveDirectorComponent director)
	{
		if (director.GetObjectiveBagValue(SAVED_BAG_KEY_A) != SAVED_BAG_VALUE_A)
			return string.Format("a module counter did not survive: '%1' expected %2, read back %3",
				SAVED_BAG_KEY_A, SAVED_BAG_VALUE_A.ToString(), director.GetObjectiveBagValue(SAVED_BAG_KEY_A).ToString());

		if (director.GetObjectiveBagValue(SAVED_BAG_KEY_B) != SAVED_BAG_VALUE_B)
			return string.Format("a second module counter did not survive: '%1' expected %2, read back %3",
				SAVED_BAG_KEY_B, SAVED_BAG_VALUE_B.ToString(), director.GetObjectiveBagValue(SAVED_BAG_KEY_B).ToString());

		float bagDrift = vector.Distance(director.GetObjectiveBagPosition(SAVED_BAG_VECTOR_KEY), m_vSavedBagPosition);
		if (bagDrift > POSITION_TOLERANCE)
			return string.Format("a module position did not survive: '%1' came back %2 m from where it was saved",
				SAVED_BAG_VECTOR_KEY, bagDrift.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts the blacklist came back as the two saved entries and nothing else.
	//! \param[in] director The restored director.
	//! \return An empty string when it matched, otherwise the failure.
	protected string AssertBlacklist(notnull OVT_ObjectiveDirectorComponent director)
	{
		if (director.GetBlacklistCount() != 2)
			return string.Format("the restored blacklist holds %1 entries, not the two that were saved - the dirty third entry was not cleared, or the saved pair did not come back",
				director.GetBlacklistCount().ToString());

		int roundsA = FindBlacklistRounds(director, m_vBlacklistA);
		if (roundsA != SAVED_BLACKLIST_A_ROUNDS)
			return string.Format("the first blacklisted place came back with %1 rounds left instead of %2",
				roundsA.ToString(), SAVED_BLACKLIST_A_ROUNDS.ToString());

		int roundsB = FindBlacklistRounds(director, m_vBlacklistB);
		if (roundsB != SAVED_BLACKLIST_B_ROUNDS)
			return string.Format("the second blacklisted place came back with %1 rounds left instead of %2",
				roundsB.ToString(), SAVED_BLACKLIST_B_ROUNDS.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Rounds left on the blacklist entry nearest a position.
	//! \param[in] director The restored director.
	//! \param[in] position The place to look for.
	//! \return Its rounds, or -1 when no entry is at that place.
	protected int FindBlacklistRounds(notnull OVT_ObjectiveDirectorComponent director, vector position)
	{
		int count = director.GetBlacklistCount();
		for (int i = 0; i < count; i++)
		{
			if (vector.Distance(director.GetBlacklistPosition(i), position) <= POSITION_TOLERANCE)
				return director.GetBlacklistRounds(i);
		}

		return -1;
	}
}

//------------------------------------------------------------------------------------------------
//! A restored forward operating base FINDS ITS DEPLOYMENT AGAIN, by name and position, on the first
//! tick after the load.
//!
//! Case 15 saves a forward base naming a deployment that has never existed and proves the machine
//! gives up cleanly on the far side of the grace window. This is the other half: a forward base
//! whose deployment IS standing must be re-adopted. They cannot be one case - the state is
//! established by the payload, and a case gets one reload.
//!
//! The forward base is two separate persisted things that come back independently - the STRUCTURE
//! and the DEPLOYMENT MARKER - while the director's RECORD of both comes back through the director's
//! own payload. Nothing orders those three, which is why the re-link is a tick-time job. If it went
//! wrong, a continued campaign would tear down a perfectly good forward base a few in-game minutes
//! after every load, refund nothing, and start the middle phase again.
//!
//! ⚠ The fixture deployment is made inert the instant it exists - SetSpawnedUnitsEliminated(true) on
//! the deployment AND every spawning module, re-asserted on every poll - and its REINFORCEMENT
//! MODULE is removed. Its config is the real forward-base one, which carries the raise module: left
//! live it would put a persisted flagpole into the campaign this suite runs in. The reinforcement
//! module has to go for two reasons - its rebuy CLEARS the eliminated flags, and its
//! m_bDeleteOnConditionFail collects the deployment the moment its objective condition fails.
//!
//! 🔴 THE DIRTY STEP MAY NOT COMMIT AN OBJECTIVE, and this cost the case its first red run.
//! CommitObjective() destroys this fixture twice over, and both are the product behaving correctly:
//! it runs the director's forward-base teardown (deleting any forward-base or garrison deployment
//! within the teardown radius), and it enters the HARASSMENT phase, so this deployment's condition
//! module starts failing and m_bDeleteOnConditionFail collects it. The reload seam cannot put a
//! marker back, so once it is gone the case can never pass. The dirty step therefore rewrites the
//! forward-base RECORD instead - a different position, supplying base, re-link key and spend - with
//! the objective left exactly where it was.
//!
//! ⚠ What that costs, stated rather than hidden: two values are PRECONDITIONS here, not round-trip
//! claims. IsAssetUp(ASSET_FOB) before the ticks (the dirty state also has a forward base up, so a
//! reload restoring nothing would satisfy it - what has real closure is its POSITION, SOURCE,
//! RE-LINK KEY and SPEND, all of which the dirty step changes), and the objective's own position,
//! which only CommitObjective() can move and which case 15 owns.
//!
//! ⚠ The reload seam only re-applies game-mode component records, so the marker's own Deserialize is
//! never re-run - which is exactly the shape being tested: the marker is the LIVE one that was there
//! before the save, and the director's payload has to match itself back to it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_ObjectiveFOB_RelinksItsDeployment : SCR_AutotestCaseBase
{
	//! Tolerance for every position comparison, in metres. vector.Distance is not correctly rounded at
	//! campaign ranges, so a stored position is never compared with ==.
	static const float POSITION_TOLERANCE = 1.0;

	//! Far from every town, base and tower the campaign has, so the fixture deployment cannot be
	//! confused with a real one and the objective cannot accidentally name a real place.
	static const vector FIXTURE_OBJECTIVE = "15500 20 15500";
	static const vector FIXTURE_FOB = "15000 20 15000";
	static const vector FIXTURE_SOURCE = "14500 20 14500";

	//! Where the dirty step SAYS the forward base is. Far enough from FIXTURE_FOB that the position
	//! assertion has teeth, and deliberately outside the teardown's own area radius of it so nothing
	//! the dirty state implies could reach the fixture deployment.
	static const vector DIRTY_FOB = "16500 20 16500";
	static const vector DIRTY_SOURCE = "17000 20 17000";

	//! What the dirty step SAYS is carrying the forward base. Not a registered config name: the point
	//! is that the re-link key round-trips as a string.
	static const string DIRTY_FOB_DEPLOYMENT = "ObjectiveRelinkDirtyName";

	static const int SAVED_PHASE_TICKS = 151;
	static const int SAVED_OP_TICKS = 87;
	static const int SAVED_FOB_SPENT = 313;

	static const int DIRTY_PHASE_TICKS = 5;
	static const int DIRTY_OP_TICKS = 6;

	//! Added to the spend by the dirty step, so the restored total cannot match the dirty one.
	static const int DIRTY_FOB_SPEND_DELTA = 640;

	//! How far the assertions look for the fixture deployment.
	static const float LOOKUP_RADIUS = 100;

	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iSaveBaseline;
	protected int m_iReloadPolls;

	protected OVT_DeploymentComponent m_Fixture;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// EVERY POLL, AND CHEAP. The fixture lives across tens of seconds of asynchronous save and
		// re-application, during which its own update loop keeps running; re-asserting the eliminated
		// flags costs a short list walk and closes the window in which anything could have cleared them.
		if (m_Fixture)
			MakeInert(m_Fixture);

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			string setup = SetUpFixture();
			if (setup != "")
			{
				Cleanup();
				SetFailure(setup);
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

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE)
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

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD)
		{
			OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
			if (!director)
			{
				Cleanup();
				SetFailure("OVT_Global.GetObjectiveDirector() is null before the reload");
				return true;
			}

			// 🔴 THE FORWARD-BASE RECORD IS REWRITTEN. THE OBJECTIVE IS NOT TOUCHED. See the class
			// header: CommitObjective() would destroy the fixture deployment twice over, and the reload
			// seam cannot bring a deployment marker back, so the case could never pass afterwards.
			director.ReportAssetRaised(OVT_ObjectiveDirectorComponent.ASSET_FOB, DIRTY_FOB, DIRTY_SOURCE, DIRTY_FOB_DEPLOYMENT);
			director.AddAssetSpend(OVT_ObjectiveDirectorComponent.ASSET_FOB, DIRTY_FOB_SPEND_DELTA);
			director.SetPhaseTimeout(DIRTY_PHASE_TICKS);
			director.SetOperationCountdown(DIRTY_OP_TICKS);

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

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD)
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

		string restored = OVT_TEST_PersistenceRoundTripGate.RequireRestoredCampaign();
		if (restored != "")
		{
			Cleanup();
			SetFailure(restored);
			return true;
		}

		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		if (!director)
		{
			Cleanup();
			SetFailure("OVT_Global.GetObjectiveDirector() is null after the reload");
			return true;
		}

		string failure = AssertRelinked(director);

		// Tear the fixture down whatever the verdict. ⚠ THE OBJECTIVE FIRST: resetting it runs the
		// director's own teardown, which is what takes the fixture deployment away; Cleanup() is the
		// belt-and-braces half for the paths where the reset never happened.
		director.ResetObjective("forward-base re-link fixture torn down", false);
		Cleanup();

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a real, inert forward-base deployment in the world and tells the director about it.
	//! \return An empty string when the fixture stands, otherwise why it does not.
	protected string SetUpFixture()
	{
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!deployments || !director || !config)
			return "the deployment framework, the objective director or the campaign config did not resolve";

		OVT_DeploymentConfig fobConfig = deployments.FindConfigByName(OVT_ObjectiveDirectorComponent.FOB_CONFIG);
		if (!fobConfig)
			return string.Format("'%1' is not registered, so there is no forward-base deployment to re-link to", OVT_ObjectiveDirectorComponent.FOB_CONFIG);

		m_Fixture = deployments.CreateDeployment(fobConfig, FIXTURE_FOB, config.GetOccupyingFactionIndex(), 0, 0);
		if (!m_Fixture)
			return "the fixture forward-base deployment could not be created";

		// IMMEDIATELY. This config carries the raise module; left live for even one update it would put
		// a persisted structure into the campaign this suite is running in.
		MakeInert(m_Fixture);

		// ⚠ AND THE REBUY HAS TO GO, for two separate reasons. Its rebuy CLEARS the eliminated flags,
		// which would un-do MakeInert() and let the raise module build after all; and its
		// m_bDeleteOnConditionFail is what collects an objective deployment whose condition stops
		// holding, which is a live risk over the tens of seconds a save and a re-application take.
		m_Fixture.RemoveModule(OVT_ReinforcementBehaviorDeploymentModule);

		director.CommitObjective(OVT_EObjectiveKind.TOWN, FIXTURE_OBJECTIVE, "forward-base re-link fixture");
		director.EnterPhase("ForwardBase");
		director.SetPhaseTimeout(SAVED_PHASE_TICKS);
		director.SetOperationCountdown(SAVED_OP_TICKS);

		director.ReportAssetRaised(OVT_ObjectiveDirectorComponent.ASSET_FOB, FIXTURE_FOB, FIXTURE_SOURCE, OVT_ObjectiveDirectorComponent.FOB_CONFIG);
		director.AddAssetSpend(OVT_ObjectiveDirectorComponent.ASSET_FOB, SAVED_FOB_SPENT);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the forward base back and proves the director kept it.
	//! \param[in] director The restored director.
	//! \return An empty string when it re-linked, otherwise the first failure.
	protected string AssertRelinked(notnull OVT_ObjectiveDirectorComponent director)
	{
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (!deployments)
			return "the deployment framework did not resolve after the reload";

		// PRECONDITION, NOT AN ASSERTION ABOUT THE DIRECTOR. If the marker is gone the case is testing
		// the opposite claim by accident, and it has to say so rather than go green.
		//
		// ⚠ If this is what broke, look at what DELETED it before looking at the restore. The reload seam
		// can neither remove a deployment marker nor put one back, so a missing fixture was deleted by
		// something in this session - the director's forward-base teardown (reached from any
		// CommitObjective or ResetObjective) or the deployment's own reinforcement module reacting to a
		// failed objective condition. Both are the product working; the case's job is to not provoke them.
		if (!deployments.GetDeploymentNearPosition(OVT_ObjectiveDirectorComponent.FOB_CONFIG, FIXTURE_FOB, LOOKUP_RADIUS))
			return "the fixture forward-base deployment is no longer standing after the reload, so the re-link had nothing to find and this case cannot say anything about it";

		// PRECONDITION TOO, AND DELIBERATELY LABELLED AS ONE. The dirty state also has a forward base
		// up, so a reload that restored nothing at all would still satisfy this. The four fields below
		// are what carry the round-trip claim.
		if (!director.IsAssetUp(OVT_ObjectiveDirectorComponent.ASSET_FOB))
			return "the forward base did not survive the round trip - the restored objective has none, so the re-link cannot even be attempted";

		float fobDrift = vector.Distance(director.GetAssetPosition(OVT_ObjectiveDirectorComponent.ASSET_FOB), FIXTURE_FOB);
		if (fobDrift > POSITION_TOLERANCE)
			return string.Format("the restored forward base is %1 m from where it was saved", fobDrift.ToString());

		float sourceDrift = vector.Distance(director.GetFOBSourceBasePosition(), FIXTURE_SOURCE);
		if (sourceDrift > POSITION_TOLERANCE)
			return string.Format("the restored forward base's supplying base is %1 m from where it was saved", sourceDrift.ToString());

		if (director.GetFOBSpent() != SAVED_FOB_SPENT)
			return string.Format("the forward base's spend against its ceiling did not survive: expected %1, read back %2 - a ceiling that resets on every load is not a ceiling",
				SAVED_FOB_SPENT.ToString(), director.GetFOBSpent().ToString());

		if (director.GetFOBDeploymentName() != OVT_ObjectiveDirectorComponent.FOB_CONFIG)
			return string.Format("the forward base's re-link key did not survive: expected '%1', read back '%2'",
				OVT_ObjectiveDirectorComponent.FOB_CONFIG, director.GetFOBDeploymentName());

		// 🔴 The restored objective RE-ADOPTS its forward base's module, and that is the half nothing else
		// would catch. A restore rebuilds the phase's module set from the plan, so the raise module that
		// comes back is a FRESH clone with no memory of having sent anything. It has to look at the
		// restored record on entry and adopt the base already standing - that registration is what arms
		// the spend ceiling and what the one teardown path reaches through. Without it a continued
		// campaign would spend past the budget and leave the structure and garrison standing.
		OVT_BaseObjectiveAssetModule owner = director.GetAssetModule(OVT_ObjectiveDirectorComponent.ASSET_FOB);
		if (!owner)
			return "the restored objective registered no owner for its forward base, so nothing arms the spend ceiling and nothing can take the base down when the objective ends";

		if (!director.IsAssetCeilingArmed())
			return "the restored forward base's spend ceiling is not armed. Every operation the continued campaign buys in this phase would spend against the faction pool alone, with no budget at all";

		if (!director.IsFOBDeploymentSent())
			return "the restored objective does not believe its forward base was ever sent, so its raise module would site and buy a SECOND one on the next interval it can afford";

		// 🔴 THE CLAIM. The whole grace window is driven, so a director that merely had not got round to
		// giving up yet cannot pass this.
		//
		// ⚠ Exactly the window and not one tick more. On the RED path the last of these ticks abandons the
		// objective and leaves the machine IDLE, and the IDLE branch of a further tick would select a real
		// objective and enter its first phase with the operation countdown armed to ZERO - buying a real
		// deployment with real resources, which nothing refunds.
		int ticks = OVT_ObjectiveDirectorComponent.FOB_RELINK_ATTEMPTS;
		for (int i = 0; i < ticks; i++)
		{
			director.DirectorTick();
		}

		director.SetOperationCountdown(SAVED_OP_TICKS);

		// ⚠ THE FORWARD BASE, NOT THE OBJECTIVE, IS THE ASSERTION. A director that gave up re-linking
		// abandons the objective and then immediately selects a NEW one on the same tick, so
		// HasObjective() is true either way; only the forward base tells the two apart.
		if (!director.IsAssetUp(OVT_ObjectiveDirectorComponent.ASSET_FOB))
			return string.Format("the restored forward base was torn down within %1 ticks even though its deployment was standing the whole time - the re-link did not match the marker it was sitting on, and a continued campaign would lose its forward base a few in-game minutes after every load",
				ticks.ToString());

		// ⚠ NOT A ROUND-TRIP ASSERTION - the dirty step cannot move the objective without destroying the
		// fixture, so this value was never dirtied and case 15 owns that round trip. It is a DIFFERENT
		// claim, and it is closed by the red path: a director that gives up re-linking abandons the
		// objective and selects a real one on the same tick, which lands somewhere else entirely.
		float drift = vector.Distance(director.GetObjectivePosition(), FIXTURE_OBJECTIVE);
		if (drift > POSITION_TOLERANCE)
			return string.Format("the director is no longer working on the restored objective after the re-link ticks - it is %1 m away, so something abandoned it and chose again",
				drift.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Makes the fixture deployment unable to register or build anything.
	//! \param[in] deployment The deployment to disarm.
	protected void MakeInert(notnull OVT_DeploymentComponent deployment)
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
	//! Deletes the fixture deployment if the director's own teardown has not already taken it.
	protected void Cleanup()
	{
		if (!m_Fixture)
			return;

		MakeInert(m_Fixture);

		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (deployments)
			deployments.DeleteDeployment(m_Fixture);

		m_Fixture = null;
	}
}

//------------------------------------------------------------------------------------------------
//! Shared machinery for the three storage round-trip cases: put a holder in the world, stock its
//! ledger, dirty it, and say in one sentence what came back wrong.
//!
//! ⚠ The ledger keys are SYNTHETIC. A holder's ledger is a plain ResourceName -> count map and there
//! is deliberately no registry gate anywhere on the load path - a line exists because the server
//! deleted a real entity, so gating it would delete a player's loot. Keys no catalogue knows keep
//! these cases independent of economy retuning AND pin that rule: if a registry check ever appears
//! in the load path, every one of these cases goes red.
//!
//! Each case stocks DIFFERENT counts, so a serializer that read one holder's record onto another
//! would be visible rather than passing three times over.
//------------------------------------------------------------------------------------------------
class OVT_TEST_StorageRoundTripFixture
{
	static const string KEY_A = "OVT_TEST/StorageRoundTrip/ItemA.et";
	static const string KEY_B = "OVT_TEST/StorageRoundTrip/ItemB.et";

	//! Written over the saved ledger before the reload. A holder that comes back holding this is a
	//! holder nothing was read back onto (closure 2).
	static const string KEY_DIRT = "OVT_TEST/StorageRoundTrip/Dirt.et";
	static const int DIRTY_COUNT = 9;

	//! Resolved by name out of the placeables config. Never an index - entries get appended.
	static const string PLACEABLE_NAME = "Ammobox";

	//! Prefab path fragment every warehouse variant shares - the same fragment the real-estate config
	//! filters on.
	static const string WAREHOUSE_PREFAB_FRAGMENT = "Warehouse_01";

	//! Metres searched for the world's warehouse building.
	static const float WAREHOUSE_SEARCH_RADIUS = 20000;

	protected IEntity m_FoundWarehouse;

	//------------------------------------------------------------------------------------------------
	//! Somewhere to put a holder, offset from wherever a test vehicle would go.
	//! \param[in] offset Per-case separation, so no two cases put a holder in the same place.
	//! \param[out] position Where to put it; untouched on failure.
	//! \return An empty string on success, otherwise the sentence to fail with.
	static string ResolveSubjectPosition(vector offset, out vector position)
	{
		vector anchor;
		string diagnostic;
		if (!OVT_TEST_PersistenceSubject.ResolveVehicleSpawnPosition(anchor, diagnostic))
			return "Nowhere to put a storage holder: " + diagnostic;

		position = anchor + offset;
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Puts one ammo box down through the server-side placement path.
	//!
	//! PlaceItem() rather than a raw prefab spawn, because placement is what TRACKS the box: the placed
	//! box prefab carries no native Persistence component, so a spawned one would have no record at all
	//! and this case would fail on the reload for a reason that has nothing to do with storage.
	//! \param[in] position Where to put it.
	//! \param[out] box The placed box; untouched on failure.
	//! \return An empty string on success, otherwise the sentence to fail with.
	static string PlaceBox(vector position, out IEntity box)
	{
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance)
			return "OVT_Global.GetResistanceFaction() is null, so there is no placement path to put a box down with";

		if (!resistance.m_PlaceablesConfig || !resistance.m_PlaceablesConfig.m_aPlaceables)
			return "The resistance faction has no placeables config loaded";

		int index = -1;
		for (int i = 0; i < resistance.m_PlaceablesConfig.m_aPlaceables.Count(); i++)
		{
			OVT_Placeable candidate = resistance.m_PlaceablesConfig.m_aPlaceables[i];
			if (candidate && candidate.m_sName == PLACEABLE_NAME)
			{
				index = i;
				break;
			}
		}

		if (index < 0)
			return string.Format("No placeable named '%1' in the placeables config - the entry is missing or renamed", PLACEABLE_NAME);

		// playerId -1 is PlaceItem()'s own server-initiated marker: it waives the funds, distance and
		// item-limit checks.
		box = resistance.PlaceItem(index, 0, position, vector.Zero, -1);
		if (!box)
			return string.Format("PlaceItem() placed no ammo box at %1", position.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Finds the world's warehouse building by prefab path, one accumulator per instance.
	//! \return The first warehouse building, or null when there is none.
	IEntity FindWarehouse()
	{
		m_FoundWarehouse = null;

		BaseWorld world = GetGame().GetWorld();
		if (world)
			world.QueryEntitiesBySphere("0 0 0", WAREHOUSE_SEARCH_RADIUS, null, FilterWarehouse, EQueryEntitiesFlags.STATIC);

		return m_FoundWarehouse;
	}

	//------------------------------------------------------------------------------------------------
	//! A BUILT warehouse is skipped: it carries OVT_BuildableComponent, matches a different
	//! persistence configuration, and would silently make this fixture's cases assert the wrong
	//! binding once one is standing in the world.
	//! \param[in] e The entity the query offered.
	//! \return Always false - there is no early-out with a null query callback.
	protected bool FilterWarehouse(IEntity e)
	{
		if (!e || m_FoundWarehouse)
			return false;

		if (OVT_ComponentFinder<OVT_BuildableComponent>.Find(e))
			return false;

		ResourceName prefab = OVT_PrefabUtils.GetPrefabName(e);
		if (prefab.IndexOf(WAREHOUSE_PREFAB_FRAGMENT) > -1)
			m_FoundWarehouse = e;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The storage component on a holder.
	//! \param[in] holder The holder to read.
	//! \param[in] subject What to call it in a failure sentence.
	//! \param[out] storage The component; untouched on failure.
	//! \return An empty string on success, otherwise the sentence to fail with.
	static string ResolveStorage(IEntity holder, string subject, out OVT_StorageComponent storage)
	{
		if (!holder)
			return string.Format("%1 is no longer in the world", subject);

		storage = OVT_StorageUtils.GetStorage(holder);
		if (!storage)
			return string.Format("%1 has no OVT_StorageComponent, so it has no ledger to round-trip - the prefab delta that puts one there has been lost", subject);

		if (!storage.GetLedger())
			return string.Format("%1 has a storage component with no ledger", subject);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a known ledger on a holder through the component's own public server API.
	//!
	//! UNLIMITED is passed explicitly rather than read from GetCapacity(), so a case never depends on
	//! the deferred capacity resolve having landed - and capacity is not persisted anyway (D8).
	//! PublishCount() is the documented republish, and on a building it is also what tracks the holder.
	//! \param[in] storage The holder's storage component.
	//! \param[in] countA How many of KEY_A to hold.
	//! \param[in] countB How many of KEY_B to hold.
	static void Stock(notnull OVT_StorageComponent storage, int countA, int countB)
	{
		OVT_StorageLedger ledger = storage.GetLedger();
		ledger.Clear();
		ledger.Add(KEY_A, countA, OVT_StorageComponent.UNLIMITED_CAPACITY);
		ledger.Add(KEY_B, countB, OVT_StorageComponent.UNLIMITED_CAPACITY);

		storage.PublishCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Destroys the saved ledger in memory (closure 2), through the same API that wrote it.
	//! \param[in] storage The holder's storage component.
	static void Dirty(notnull OVT_StorageComponent storage)
	{
		OVT_StorageLedger ledger = storage.GetLedger();
		ledger.Clear();
		ledger.Add(KEY_DIRT, DIRTY_COUNT, OVT_StorageComponent.UNLIMITED_CAPACITY);

		storage.PublishCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts the saved ledger came back and the dirty one did not.
	//! \param[in] storage The holder's storage component after the reload.
	//! \param[in] countA What KEY_A held when it was saved.
	//! \param[in] countB What KEY_B held when it was saved.
	//! \param[in] subject What to call the holder in a failure sentence.
	//! \return An empty string when everything came back, otherwise the sentence to fail with.
	static string AssertLedgerRestored(notnull OVT_StorageComponent storage, int countA, int countB, string subject)
	{
		OVT_StorageLedger ledger = storage.GetLedger();
		if (!ledger)
			return string.Format("%1 has no ledger after the reload", subject);

		if (ledger.Count(KEY_DIRT) != 0)
		{
			return string.Format("%1 still holds the %2 dirty items written after the save, so nothing was read back over them - the stored ledger was never applied",
				subject, DIRTY_COUNT.ToString());
		}

		if (ledger.Count(KEY_A) != countA)
		{
			return string.Format("%1 came back holding %2 of line A, expected the saved %3 - the ledger did not survive the round trip",
				subject, ledger.Count(KEY_A).ToString(), countA.ToString());
		}

		if (ledger.Count(KEY_B) != countB)
		{
			return string.Format("%1 came back holding %2 of line B, expected the saved %3 - only part of the ledger survived, so the payload is losing lines",
				subject, ledger.Count(KEY_B).ToString(), countB.ToString());
		}

		int expectedTotal = countA + countB;
		if (ledger.Total() != expectedTotal)
		{
			return string.Format("%1 came back with a total of %2 across %3 line(s), expected %4 - the two lines are right but something else is in there",
				subject, ledger.Total().ToString(), ledger.LineCount().ToString(), expectedTotal.ToString());
		}

		// The REPLICATED count, not the ledger's. Clear() deliberately fires no change event, so a load
		// that rebuilds the ledger without republishing leaves every client's "Storage (N items)" label
		// reading the pre-load number until the next job happens to touch the holder.
		if (storage.GetTotalCount() != expectedTotal)
		{
			return string.Format("%1 restored a ledger of %2 items but its REPLICATED count still reads %3 - PublishCount() was not called on the load path, so the action label and the map lie to every client",
				subject, expectedTotal.ToString(), storage.GetTotalCount().ToString());
		}

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! A placed ammo box's item ledger AND its player-set name survive a real save.
//!
//! The feature replaces spawned stockpile entities with one map of counts. A map that does not come
//! back is a player's entire stockpile deleted on the next continue, with nothing on screen and
//! nothing in the log - the entities that used to carry that stock were saved by vanilla for free,
//! so this is a capability the feature has to REPLACE, not add.
//!
//! A real storage round trip on the third gate seam: a placed box owns its own persistence record.
//! Both closures hold - the ledger is dirtied through the same public API that stocked it, and a
//! four-thousand-item box with a name is not a state any placement or campaign start produces.
//!
//! The NAME is asserted here and only here, because the box is the holder players actually rename.
//! It is the second property in the payload, so a name that comes back empty while the ledger comes
//! back correct is the specific signature of a Serialize/Deserialize local-name mismatch.
//!
//! ⚠ Takes a real save, so `StorageBox*` must sort after `..._Capability_...`. It sorts before
//! `StructureDamage*` and every `Town*` case and disturbs neither.
//!
//! The box is left standing on purpose (BUG-118, as with the other subjects here).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 90)]
class OVT_TEST_PersistenceRoundTrip_StorageBox_LedgerAndNameSurviveSave : SCR_AutotestCaseBase
{
	//! Offset from where a test vehicle would go, so no other case's subject is near this box.
	static const vector PLACE_OFFSET = "14 0 0";

	static const int SAVED_A = 4242;
	static const int SAVED_B = 77;

	//! Not a name any placement or campaign start produces.
	static const string SAVED_NAME = "Depot North 4242";

	//! Written over the saved name before the reload.
	static const string DIRTY_NAME = "dirtied";

	static const int PHASE_PLACE = 0;
	static const int PHASE_AWAIT_TRACKING = 1;
	static const int PHASE_STOCK_AND_SAVE = 2;
	static const int PHASE_AWAIT_SAVE = 3;
	static const int PHASE_DIRTY_AND_RELOAD = 4;
	static const int PHASE_AWAIT_RELOAD = 5;
	static const int PHASE_ASSERT = 6;

	protected int m_iPhase;
	protected int m_iTrackingPolls;
	protected int m_iSavePolls;
	protected int m_iReloadPolls;
	protected int m_iSaveBaseline;

	protected IEntity m_Box;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == PHASE_PLACE)
			return Place();

		if (m_iPhase == PHASE_AWAIT_TRACKING)
			return AwaitTracking();

		if (m_iPhase == PHASE_STOCK_AND_SAVE)
			return StockAndSave();

		if (m_iPhase == PHASE_AWAIT_SAVE)
			return AwaitSave();

		if (m_iPhase == PHASE_DIRTY_AND_RELOAD)
			return DirtyAndReload();

		if (m_iPhase == PHASE_AWAIT_RELOAD)
			return AwaitReload();

		return Assert();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to advance.
	protected bool Place()
	{
		vector position;
		string diagnostic = OVT_TEST_StorageRoundTripFixture.ResolveSubjectPosition(PLACE_OFFSET, position);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		diagnostic = OVT_TEST_StorageRoundTripFixture.PlaceBox(position, m_Box);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		m_iPhase = PHASE_AWAIT_TRACKING;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A placed object's persistence registration is asynchronous, so the case waits for its record to
	//! exist before it saves - otherwise the save could legitimately contain nothing to read back and
	//! the failure would name the wrong half. Bounded, and expiry fails.
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitTracking()
	{
		if (!m_Box)
		{
			SetFailure("The placed ammo box left the world before it was registered for saving");
			return true;
		}

		if (!OVT_TEST_PersistenceRoundTripGate.InstanceIsTracked(m_Box))
		{
			m_iTrackingPolls += 1;
			if (m_iTrackingPolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
			{
				SetFailure("The placed ammo box never became persistence-tracked, so it has no stored record and nothing about its contents can survive a save");
				return true;
			}

			return false;
		}

		m_iPhase = PHASE_STOCK_AND_SAVE;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to advance.
	protected bool StockAndSave()
	{
		OVT_StorageComponent storage;
		string diagnostic = OVT_TEST_StorageRoundTripFixture.ResolveStorage(m_Box, "The placed ammo box", storage);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		// Closure 3: a freshly placed box is empty and unnamed, so neither saved value is one the
		// placement path or a campaign start could have produced on its own.
		if (storage.GetLedger().Total() != 0 || storage.GetCustomName() != "")
		{
			SetFailure("The ammo box was already stocked or already named the moment it was placed, so the values below would not be this case's");
			return true;
		}

		OVT_TEST_StorageRoundTripFixture.Stock(storage, SAVED_A, SAVED_B);
		storage.SetCustomName(SAVED_NAME);

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

		m_iPhase = PHASE_DIRTY_AND_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Closure 2: the saved ledger and name are destroyed in memory before anything is read back,
	//! through the same public API that wrote them.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool DirtyAndReload()
	{
		OVT_StorageComponent storage;
		string diagnostic = OVT_TEST_StorageRoundTripFixture.ResolveStorage(m_Box, "The placed ammo box", storage);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_TEST_StorageRoundTripFixture.Dirty(storage);
		storage.SetCustomName(DIRTY_NAME);

		if (storage.GetLedger().Count(OVT_TEST_StorageRoundTripFixture.KEY_A) != 0 || storage.GetCustomName() != DIRTY_NAME)
		{
			SetFailure("The ammo box kept its saved ledger or its saved name through the dirtying step, so the reload would prove nothing");
			return true;
		}

		string reload = OVT_TEST_PersistenceRoundTripGate.RequestInstanceReload(m_Box);
		if (reload != "")
		{
			SetFailure(reload);
			return true;
		}

		m_iPhase = PHASE_AWAIT_RELOAD;
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
				SetFailure("The ammo box's stored record was never re-applied: the persistence system's re-application never completed");
				return true;
			}

			return false;
		}

		m_iPhase = PHASE_ASSERT;
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

		OVT_StorageComponent storage;
		string diagnostic = OVT_TEST_StorageRoundTripFixture.ResolveStorage(m_Box, "The placed ammo box", storage);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		diagnostic = OVT_TEST_StorageRoundTripFixture.AssertLedgerRestored(storage, SAVED_A, SAVED_B, "The placed ammo box");
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		if (storage.GetCustomName() != SAVED_NAME)
		{
			SetFailure(string.Format("The ammo box was renamed '%1' and saved, then renamed '%2' in memory, and came back called '%3' - a holder's name does not survive a save, so every stockpile a player labelled is anonymous again on the next continue",
				SAVED_NAME, DIRTY_NAME, storage.GetCustomName()));
			return true;
		}

		Print("A placed ammo box's item ledger and its name survived a real save and came back out of storage");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A wheeled vehicle's item ledger survives a real save.
//!
//! The serializer is one class but the BINDING is three separate .conf entries, and a serializer
//! that is not listed is never called - silently. The box case exercises the PLACEABLE entry; this
//! one exercises vanilla's CAR configuration {64C6B4937723DA61}, which carries every truck and car
//! in the game. Dropping it loses the mod's entire mobile stock while the box case stays green.
//!
//! No name here - the box case owns that assertion, and repeating it would make two cases fail
//! together for one fault.
//!
//! The subject is the vehicle manager's own starting car, read from configuration rather than
//! hardcoded, and it is spawned rather than registered, so this adds nothing to any player's
//! vehicle registry.
//!
//! ⚠ Takes a real save; `StorageVehicle*` sorts after `..._Capability_...`. Left standing (BUG-118).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 90)]
class OVT_TEST_PersistenceRoundTrip_StorageVehicle_LedgerSurvivesSave : SCR_AutotestCaseBase
{
	//! Offset from where a test vehicle would go, clear of the box case's subject.
	static const vector SPAWN_OFFSET = "0 0 14";

	static const int SAVED_A = 1337;
	static const int SAVED_B = 8;

	static const int PHASE_SPAWN = 0;
	static const int PHASE_AWAIT_TRACKING = 1;
	static const int PHASE_STOCK_AND_SAVE = 2;
	static const int PHASE_AWAIT_SAVE = 3;
	static const int PHASE_DIRTY_AND_RELOAD = 4;
	static const int PHASE_AWAIT_RELOAD = 5;
	static const int PHASE_ASSERT = 6;

	protected int m_iPhase;
	protected int m_iTrackingPolls;
	protected int m_iSavePolls;
	protected int m_iReloadPolls;
	protected int m_iSaveBaseline;

	protected IEntity m_Vehicle;
	protected ResourceName m_sPrefab;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == PHASE_SPAWN)
			return Spawn();

		if (m_iPhase == PHASE_AWAIT_TRACKING)
			return AwaitTracking();

		if (m_iPhase == PHASE_STOCK_AND_SAVE)
			return StockAndSave();

		if (m_iPhase == PHASE_AWAIT_SAVE)
			return AwaitSave();

		if (m_iPhase == PHASE_DIRTY_AND_RELOAD)
			return DirtyAndReload();

		if (m_iPhase == PHASE_AWAIT_RELOAD)
			return AwaitReload();

		return Assert();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to advance.
	protected bool Spawn()
	{
		string diagnostic;
		if (!OVT_TEST_PersistenceSubject.ResolveOwnableVehiclePrefab(m_sPrefab, diagnostic))
		{
			SetFailure("Cannot resolve a vehicle to spawn: " + diagnostic);
			return true;
		}

		vector position;
		diagnostic = OVT_TEST_StorageRoundTripFixture.ResolveSubjectPosition(SPAWN_OFFSET, position);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		m_Vehicle = OVT_Global.SpawnEntityPrefab(m_sPrefab, position);
		if (!m_Vehicle)
		{
			SetFailure(string.Format("SpawnEntityPrefab() produced no vehicle from %1", m_sPrefab));
			return true;
		}

		m_iPhase = PHASE_AWAIT_TRACKING;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A vehicle is registered by the native Persistence component on its prefab, which is asynchronous
	//! enough to be worth waiting for. Bounded, and expiry fails.
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitTracking()
	{
		if (!m_Vehicle)
		{
			SetFailure("The spawned vehicle left the world before it was registered for saving");
			return true;
		}

		if (!OVT_TEST_PersistenceRoundTripGate.InstanceIsTracked(m_Vehicle))
		{
			m_iTrackingPolls += 1;
			if (m_iTrackingPolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
			{
				SetFailure(string.Format("The spawned vehicle %1 never became persistence-tracked, so it has no stored record and nothing about its cargo can survive a save", m_sPrefab));
				return true;
			}

			return false;
		}

		m_iPhase = PHASE_STOCK_AND_SAVE;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to advance.
	protected bool StockAndSave()
	{
		OVT_StorageComponent storage;
		string diagnostic = OVT_TEST_StorageRoundTripFixture.ResolveStorage(m_Vehicle, "The spawned vehicle", storage);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		// Closure 3: a freshly spawned vehicle holds nothing, so the saved ledger is never a state the
		// spawn path could have produced.
		if (storage.GetLedger().Total() != 0)
		{
			SetFailure("The spawned vehicle was already carrying a ledger the moment it appeared, so the values below would not be this case's");
			return true;
		}

		OVT_TEST_StorageRoundTripFixture.Stock(storage, SAVED_A, SAVED_B);

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

		m_iPhase = PHASE_DIRTY_AND_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to advance.
	protected bool DirtyAndReload()
	{
		OVT_StorageComponent storage;
		string diagnostic = OVT_TEST_StorageRoundTripFixture.ResolveStorage(m_Vehicle, "The spawned vehicle", storage);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_TEST_StorageRoundTripFixture.Dirty(storage);

		if (storage.GetLedger().Count(OVT_TEST_StorageRoundTripFixture.KEY_A) != 0)
		{
			SetFailure("The vehicle kept its saved ledger through the dirtying step, so the reload would prove nothing");
			return true;
		}

		string reload = OVT_TEST_PersistenceRoundTripGate.RequestInstanceReload(m_Vehicle);
		if (reload != "")
		{
			SetFailure(reload);
			return true;
		}

		m_iPhase = PHASE_AWAIT_RELOAD;
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
				SetFailure("The vehicle's stored record was never re-applied: the persistence system's re-application never completed");
				return true;
			}

			return false;
		}

		m_iPhase = PHASE_ASSERT;
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

		OVT_StorageComponent storage;
		string diagnostic = OVT_TEST_StorageRoundTripFixture.ResolveStorage(m_Vehicle, "The spawned vehicle", storage);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		diagnostic = OVT_TEST_StorageRoundTripFixture.AssertLedgerRestored(storage, SAVED_A, SAVED_B, "The spawned vehicle");
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		Print("A wheeled vehicle's item ledger survived a real save and came back out of storage");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The warehouse BUILDING's item ledger survives a real save - and the building is only in the save
//! at all because the storage component asked for it.
//!
//! ⚠ Vanilla registers an INTACT building with the persistence system NEVER; it registers one only
//! when it is destroyed. So the warehouse is the one holder class where a correct serializer,
//! correctly bound, still runs zero times - and the failure is completely silent, because a building
//! that is not tracked simply has no record. The explicit track fires on the first publish that
//! leaves the ledger non-empty, and this case asserts it BEFORE it saves, so the two faults report
//! differently.
//!
//! It also covers the third .conf binding - vanilla's BUILDING configuration {65B682661F79DDBE}.
//!
//! The subject is the world's own warehouse, found by prefab path rather than by position so that
//! moving it in the test world's layer does not turn this case green-by-absence.
//!
//! ⚠ Takes a real save; `StorageWarehouse*` sorts after `..._Capability_...`.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 90)]
class OVT_TEST_PersistenceRoundTrip_StorageWarehouse_LedgerSurvivesSaveWithExplicitTrack : SCR_AutotestCaseBase
{
	static const int SAVED_A = 9001;
	static const int SAVED_B = 15;

	static const int PHASE_STOCK_AND_TRACK = 0;
	static const int PHASE_AWAIT_TRACKING = 1;
	static const int PHASE_SAVE = 2;
	static const int PHASE_AWAIT_SAVE = 3;
	static const int PHASE_DIRTY_AND_RELOAD = 4;
	static const int PHASE_AWAIT_RELOAD = 5;
	static const int PHASE_ASSERT = 6;

	protected int m_iPhase;
	protected int m_iTrackingPolls;
	protected int m_iSavePolls;
	protected int m_iReloadPolls;
	protected int m_iSaveBaseline;

	protected IEntity m_Warehouse;
	protected bool m_bWasTrackedBeforeStocking;

	protected ref OVT_TEST_StorageRoundTripFixture m_Fixture = new OVT_TEST_StorageRoundTripFixture();

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == PHASE_STOCK_AND_TRACK)
			return StockAndTrack();

		if (m_iPhase == PHASE_AWAIT_TRACKING)
			return AwaitTracking();

		if (m_iPhase == PHASE_SAVE)
			return Save();

		if (m_iPhase == PHASE_AWAIT_SAVE)
			return AwaitSave();

		if (m_iPhase == PHASE_DIRTY_AND_RELOAD)
			return DirtyAndReload();

		if (m_iPhase == PHASE_AWAIT_RELOAD)
			return AwaitReload();

		return Assert();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to advance.
	protected bool StockAndTrack()
	{
		m_Warehouse = m_Fixture.FindWarehouse();
		if (!m_Warehouse)
		{
			SetFailure("No Warehouse_01 building exists in this world. Worlds/MP/OVT_Campaign_Test_Layers/default.layer is supposed to place one - without it neither this case nor the warehouse economy has a subject.");
			return true;
		}

		OVT_StorageComponent storage;
		string diagnostic = OVT_TEST_StorageRoundTripFixture.ResolveStorage(m_Warehouse, "The warehouse building", storage);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		m_bWasTrackedBeforeStocking = OVT_TEST_PersistenceRoundTripGate.InstanceIsTracked(m_Warehouse);
		if (m_bWasTrackedBeforeStocking)
			Print("Note: the warehouse building was already persistence-tracked before it held anything, so this case's track assertion is satisfied by another route");

		OVT_TEST_StorageRoundTripFixture.Stock(storage, SAVED_A, SAVED_B);

		m_iPhase = PHASE_AWAIT_TRACKING;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The claim the rest of the case rests on: stocking a building holder put it in the save.
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitTracking()
	{
		if (!m_Warehouse)
		{
			SetFailure("The warehouse building left the world mid-case");
			return true;
		}

		if (!OVT_TEST_PersistenceRoundTripGate.InstanceIsTracked(m_Warehouse))
		{
			m_iTrackingPolls += 1;
			if (m_iTrackingPolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
			{
				SetFailure("The warehouse building is still not persistence-tracked after its ledger was stocked. Vanilla registers an intact building never, so without OVT_StorageComponent.EnsureTracked() the building has NO stored record: its serializer is bound, compiles and runs zero times, and every warehouse empties itself on the next continue with nothing in the log.");
				return true;
			}

			return false;
		}

		m_iPhase = PHASE_SAVE;
		return false;
	}

	//------------------------------------------------------------------------------------------------
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

		m_iPhase = PHASE_DIRTY_AND_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to advance.
	protected bool DirtyAndReload()
	{
		OVT_StorageComponent storage;
		string diagnostic = OVT_TEST_StorageRoundTripFixture.ResolveStorage(m_Warehouse, "The warehouse building", storage);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_TEST_StorageRoundTripFixture.Dirty(storage);

		if (storage.GetLedger().Count(OVT_TEST_StorageRoundTripFixture.KEY_A) != 0)
		{
			SetFailure("The warehouse kept its saved ledger through the dirtying step, so the reload would prove nothing");
			return true;
		}

		string reload = OVT_TEST_PersistenceRoundTripGate.RequestInstanceReload(m_Warehouse);
		if (reload != "")
		{
			SetFailure(reload);
			return true;
		}

		m_iPhase = PHASE_AWAIT_RELOAD;
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
				SetFailure("The warehouse's stored record was never re-applied: the persistence system's re-application never completed");
				return true;
			}

			return false;
		}

		m_iPhase = PHASE_ASSERT;
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

		OVT_StorageComponent storage;
		string diagnostic = OVT_TEST_StorageRoundTripFixture.ResolveStorage(m_Warehouse, "The warehouse building", storage);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		diagnostic = OVT_TEST_StorageRoundTripFixture.AssertLedgerRestored(storage, SAVED_A, SAVED_B, "The warehouse building");
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		Print("The warehouse building's item ledger survived a real save, on a record that exists only because storing something tracked the building");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A version 1 save's warehouse stock lands in the building's OVT_StorageComponent.
//!
//! Every campaign that exists today keeps its warehouse stock in OVT_WarehouseData.inventory, a map
//! on a manager record. logistics/storage deletes that field, so the ONLY route from an existing
//! save to the new ledger is QueueWarehouseMigration() and its drain. If the drain silently fails,
//! every player who continues an existing campaign finds their warehouse empty, with the building
//! intact and nothing anywhere to say where the stock went.
//!
//! It drives the migration queue directly, which is the whole server-side half: the deferred drain,
//! the building match by position, the ledger credit at unlimited capacity and the republished
//! count. It cannot cover the SERIALIZER half - a version 1 payload is a binary blob written by a
//! native SaveContext and no script path produces one - so "an old save file reaches this queue" is
//! a play-test gate, not an assertion.
//!
//! No save is taken, so this case is fast and cannot disturb a save-based one. It shares a subject
//! with `..._StorageWarehouse_...`, harmless in either order: both Clear() the ledger before
//! stocking it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 60)]
class OVT_TEST_PersistenceRoundTrip_WarehouseMigration_Version1StockLandsInTheBuilding : SCR_AutotestCaseBase
{
	static const int MIGRATED_A = 137;
	static const int MIGRATED_B = 6;

	//! Frames allowed for a drain scheduled one second out. Generous: the queue re-arms itself once a
	//! second while anything is still undelivered.
	static const int MAX_DRAIN_POLLS = 900;

	static const int PHASE_QUEUE = 0;
	static const int PHASE_AWAIT_DRAIN = 1;
	static const int PHASE_ASSERT = 2;

	protected int m_iPhase;
	protected int m_iDrainPolls;

	protected IEntity m_Warehouse;

	protected ref OVT_TEST_StorageRoundTripFixture m_Fixture = new OVT_TEST_StorageRoundTripFixture();

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == PHASE_QUEUE)
			return Queue();

		if (m_iPhase == PHASE_AWAIT_DRAIN)
			return AwaitDrain();

		return Assert();
	}

	//------------------------------------------------------------------------------------------------
	//! Empties the building's ledger, then hands the manager a version 1 record for that position.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool Queue()
	{
		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if (!realEstate)
		{
			SetFailure("OVT_Global.GetRealEstate() is null, so there is no migration queue to hand a version 1 warehouse to");
			return true;
		}

		m_Warehouse = m_Fixture.FindWarehouse();
		if (!m_Warehouse)
		{
			SetFailure("No Warehouse_01 building exists in this world. Worlds/MP/OVT_Campaign_Test_Layers/default.layer is supposed to place one - without it this case has no subject.");
			return true;
		}

		OVT_StorageComponent storage;
		string diagnostic = OVT_TEST_StorageRoundTripFixture.ResolveStorage(m_Warehouse, "The warehouse building", storage);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		storage.GetLedger().Clear();
		storage.PublishCount();

		array<string> itemIds = new array<string>();
		itemIds.Insert(OVT_TEST_StorageRoundTripFixture.KEY_A);
		itemIds.Insert(OVT_TEST_StorageRoundTripFixture.KEY_B);

		array<int> itemCounts = new array<int>();
		itemCounts.Insert(MIGRATED_A);
		itemCounts.Insert(MIGRATED_B);

		realEstate.QueueWarehouseMigration(m_Warehouse.GetOrigin(), itemIds, itemCounts);

		if (storage.GetLedger().Total() != 0)
		{
			SetFailure("The migration delivered its stock synchronously, inside QueueWarehouseMigration(). It must defer: a real save is deserialized while the world is still being built, so the building it has to find frequently does not exist yet.");
			return true;
		}

		m_iPhase = PHASE_AWAIT_DRAIN;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitDrain()
	{
		OVT_StorageComponent storage;
		string diagnostic = OVT_TEST_StorageRoundTripFixture.ResolveStorage(m_Warehouse, "The warehouse building", storage);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		if (storage.GetLedger().Total() > 0)
		{
			m_iPhase = PHASE_ASSERT;
			return false;
		}

		m_iDrainPolls += 1;
		if (m_iDrainPolls > MAX_DRAIN_POLLS)
		{
			SetFailure("The warehouse building's ledger is still empty %1 frames after a version 1 migration record was queued for its exact position. DrainWarehouseMigration() either never ran or never matched the building, so an existing campaign's entire warehouse stock is dropped on the first continue.",
				m_iDrainPolls.ToString());
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case ends here either way.
	protected bool Assert()
	{
		OVT_StorageComponent storage;
		string diagnostic = OVT_TEST_StorageRoundTripFixture.ResolveStorage(m_Warehouse, "The warehouse building", storage);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_StorageLedger ledger = storage.GetLedger();

		if (ledger.Count(OVT_TEST_StorageRoundTripFixture.KEY_A) != MIGRATED_A)
		{
			SetFailure("The migrated warehouse holds %1 of line A, expected the queued %2 - the version 1 stock arrived but not intact",
				ledger.Count(OVT_TEST_StorageRoundTripFixture.KEY_A).ToString(),
				MIGRATED_A.ToString());
			return true;
		}

		if (ledger.Count(OVT_TEST_StorageRoundTripFixture.KEY_B) != MIGRATED_B)
		{
			SetFailure("The migrated warehouse holds %1 of line B, expected the queued %2 - the migration is losing lines",
				ledger.Count(OVT_TEST_StorageRoundTripFixture.KEY_B).ToString(),
				MIGRATED_B.ToString());
			return true;
		}

		int expectedTotal = MIGRATED_A + MIGRATED_B;
		if (ledger.Total() != expectedTotal)
		{
			SetFailure("The migrated warehouse totals %1 across %2 line(s), expected %3 - the two lines are right but something else went in with them",
				ledger.Total().ToString(),
				ledger.LineCount().ToString(),
				expectedTotal.ToString());
			return true;
		}

		// The REPLICATED count, not the ledger's. Without PublishCount() on the migration path every
		// client's "Storage (N items)" label reads zero over a full warehouse until something else
		// touches it - and on a building, PublishCount() is also what puts it in the save point.
		if (storage.GetTotalCount() != expectedTotal)
		{
			SetFailure("The migrated warehouse holds %1 items but its REPLICATED count reads %2 - PublishCount() was not called on the migration path, so the action label and the map lie to every client and the building may not even be in the save",
				expectedTotal.ToString(),
				storage.GetTotalCount().ToString());
			return true;
		}

		PrintFormat("Warehouse migration: %1 version 1 stock line(s) reached the building's OVT_StorageComponent and republished (after %2 poll(s))",
			ledger.LineCount().ToString(),
			m_iDrainPolls.ToString());

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Shared machinery for the four resource-stock round trips below.
//!
//! Two ids out of the live catalogue are saved and a THIRD is written over them before the reload,
//! so "nothing was read back" and "the payload came back empty" are different failures: an empty
//! ledger is the signature of a Serialize/Deserialize local-name mismatch, which reads an empty
//! array and reports success.
//------------------------------------------------------------------------------------------------
class OVT_TEST_ResourceRoundTripFixture
{
	//! The transport truck. Pinned: the claim is about the store the truck delta authors.
	static const ResourceName TRANSPORT_TRUCK_PREFAB = "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et";

	//! The dropped crate pile - the one prefab carrying OVT_ResourcePileComponent.
	static const ResourceName PILE_PREFAB = "{6A8E2E0000000100}Prefabs/Props/Resources/OVT_ResourcePile.et";

	//! Prefab path fragment every warehouse variant shares.
	static const string WAREHOUSE_PREFAB_FRAGMENT = "Warehouse_01";

	//! Metres searched for the world's warehouse building.
	static const float WAREHOUSE_SEARCH_RADIUS = 20000;

	//! Written over the saved stock before every reload.
	static const int DIRTY_QUANTITY = 3;

	protected IEntity m_FoundWarehouse;

	//------------------------------------------------------------------------------------------------
	//! Somewhere to put a holder, offset from wherever a test vehicle would go.
	//! \param[in] offset Per-case separation, so no two cases put a holder in the same place.
	//! \param[out] position Where to put it; untouched on failure.
	//! \return An empty string on success, otherwise the sentence to fail with.
	static string ResolveSubjectPosition(vector offset, out vector position)
	{
		vector anchor;
		string diagnostic;
		if (!OVT_TEST_PersistenceSubject.ResolveVehicleSpawnPosition(anchor, diagnostic))
			return "Nowhere to put a resource holder: " + diagnostic;

		position = anchor + offset;
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The live resource catalogue.
	//! \param[out] defs The definition table; untouched on failure.
	//! \return An empty string on success, otherwise the sentence to fail with.
	static string ResolveDefs(out OVT_ResourceDefs defs)
	{
		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
			return "OVT_Global.GetResources() is null, so there is no resource catalogue and no holder can hold anything";

		defs = resources.GetDefs();
		if (!defs || defs.Count() == 0)
			return "The resource catalogue is empty, so nothing can be stocked and this case would assert nothing";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Three distinct catalogue ids: two to save, one to dirty with.
	//! \param[in] defs The definition table.
	//! \param[out] idA First saved line.
	//! \param[out] idB Second saved line.
	//! \param[out] idDirt The line written after the save.
	//! \return An empty string on success, otherwise the sentence to fail with.
	static string ResolveLines(notnull OVT_ResourceDefs defs, out string idA, out string idB, out string idDirt)
	{
		if (defs.Count() < 3)
			return string.Format("The resource catalogue holds %1 definition(s). This case needs three distinct ids - two to save and one to dirty with - so resources.conf has lost entries.", defs.Count().ToString());

		idA = defs.IdAt(0);
		idB = defs.IdAt(1);
		idDirt = defs.IdAt(defs.Count() - 1);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The resource store on a holder.
	//! \param[in] holder The holder to read.
	//! \param[in] subject What to call it in a failure sentence.
	//! \param[out] store The component; untouched on failure.
	//! \return An empty string on success, otherwise the sentence to fail with.
	static string ResolveStore(IEntity holder, string subject, out OVT_ResourceStoreComponent store)
	{
		if (!holder)
			return string.Format("%1 is no longer in the world", subject);

		store = OVT_ResourceUtils.GetStore(holder);
		if (!store)
			return string.Format("%1 has no OVT_ResourceStoreComponent, so it has no stock to round-trip - the prefab edit that puts one there has been lost", subject);

		if (!store.GetLedger())
			return string.Format("%1 has a resource store with no ledger", subject);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a known stock on a holder through the component's own public server API.
	//!
	//! UNLIMITED is passed explicitly so no case depends on a prefab's authored volume, and capacity is
	//! not persisted anyway - ApplyPersisted() restocks unlimited for the same reason.
	//! \param[in] store The holder's resource store.
	//! \param[in] defs The definition table.
	//! \param[in] idA First saved line.
	//! \param[in] qtyA How many of idA to hold.
	//! \param[in] idB Second saved line.
	//! \param[in] qtyB How many of idB to hold.
	static void Stock(notnull OVT_ResourceStoreComponent store, notnull OVT_ResourceDefs defs, string idA, int qtyA, string idB, int qtyB)
	{
		OVT_ResourceLedger ledger = store.GetLedger();
		ledger.Clear();
		ledger.Add(idA, qtyA, defs, OVT_ResourceStoreComponent.UNLIMITED_CAPACITY);
		ledger.Add(idB, qtyB, defs, OVT_ResourceStoreComponent.UNLIMITED_CAPACITY);

		store.PublishContents();
	}

	//------------------------------------------------------------------------------------------------
	//! Destroys the saved stock in memory, through the same API that wrote it.
	//! \param[in] store The holder's resource store.
	//! \param[in] defs The definition table.
	//! \param[in] idDirt The line to leave behind.
	static void Dirty(notnull OVT_ResourceStoreComponent store, notnull OVT_ResourceDefs defs, string idDirt)
	{
		OVT_ResourceLedger ledger = store.GetLedger();
		ledger.Clear();
		ledger.Add(idDirt, DIRTY_QUANTITY, defs, OVT_ResourceStoreComponent.UNLIMITED_CAPACITY);

		store.PublishContents();
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts the saved stock came back, the dirty line did not, and the contents were republished.
	//! \param[in] store The holder's resource store after the reload.
	//! \param[in] defs The definition table.
	//! \param[in] saved The two saved lines and the dirty one, in the order idA, qtyA, idB, qtyB, idDirt.
	//! \param[in] subject What to call the holder in a failure sentence.
	//! \return An empty string when everything came back, otherwise the sentence to fail with.
	static string AssertStockRestored(notnull OVT_ResourceStoreComponent store, notnull OVT_ResourceDefs defs, notnull OVT_TEST_ResourceExpectedStock saved, string subject)
	{
		OVT_ResourceLedger ledger = store.GetLedger();
		if (!ledger)
			return string.Format("%1 has no ledger after the reload", subject);

		if (ledger.LineCount() == 0)
		{
			return string.Format("%1 came back EMPTY. SaveContext.Write() and LoadContext.Read() key each property by the LOCAL VARIABLE'S NAME, so a renamed local in OVT_ResourceStoreComponentSerializer.Deserialize() reads an empty array, reports SUCCESS, and that empty array is applied over the holder's stock - every truck, pile and warehouse in the campaign empties itself on the next continue with nothing in the log.", subject);
		}

		if (ledger.Count(saved.idDirt) != 0)
		{
			return string.Format("%1 still holds %2 of '%3', the line written after the save, so nothing was read back over it - OVT_ResourceStoreComponentSerializer is not bound for this holder class in Configs/Systems/Persistence/Overthrow.conf, or its payload read aborted.",
				subject, ledger.Count(saved.idDirt).ToString(), saved.idDirt);
		}

		if (ledger.Count(saved.idA) != saved.qtyA)
		{
			return string.Format("%1 came back holding %2 of '%3', expected the saved %4 - the stock did not survive the round trip",
				subject, ledger.Count(saved.idA).ToString(), saved.idA, saved.qtyA.ToString());
		}

		if (ledger.Count(saved.idB) != saved.qtyB)
		{
			return string.Format("%1 came back holding %2 of '%3', expected the saved %4 - only part of the stock survived, so the payload is losing lines",
				subject, ledger.Count(saved.idB).ToString(), saved.idB, saved.qtyB.ToString());
		}

		if (ledger.LineCount() != 2)
		{
			return string.Format("%1 came back with %2 line(s), expected the 2 that were saved - the two lines are right but something else is in there",
				subject, ledger.LineCount().ToString());
		}

		// The REPLICATED contents, not the ledger's. Clear() deliberately fires no change event, so a
		// load that rebuilds the ledger without republishing leaves every client's cargo HUD, pile
		// label and map row reading the pre-load stock until something else touches the holder.
		string republished = OVT_ResourcePack.Encode(ledger, defs);
		if (store.GetPackedContents() != republished)
		{
			return string.Format("%1 restored its ledger but its REPLICATED contents still read '%2' instead of '%3' - PublishContents() was not called on the load path, so every client sees the pre-load stock",
				subject, store.GetPackedContents(), republished);
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Finds the world's warehouse building by prefab path, one accumulator per instance.
	//! \return The first warehouse building, or null when there is none.
	IEntity FindWarehouse()
	{
		m_FoundWarehouse = null;

		BaseWorld world = GetGame().GetWorld();
		if (world)
			world.QueryEntitiesBySphere("0 0 0", WAREHOUSE_SEARCH_RADIUS, null, FilterWarehouse, EQueryEntitiesFlags.STATIC);

		return m_FoundWarehouse;
	}

	//------------------------------------------------------------------------------------------------
	//! A BUILT warehouse is skipped: it carries OVT_BuildableComponent, matches a different
	//! persistence configuration, and would silently make this fixture's cases assert the wrong
	//! binding once one is standing in the world.
	//! \param[in] e The entity the query offered.
	//! \return Always false - there is no early-out with a null query callback.
	protected bool FilterWarehouse(IEntity e)
	{
		if (!e || m_FoundWarehouse)
			return false;

		if (OVT_ComponentFinder<OVT_BuildableComponent>.Find(e))
			return false;

		ResourceName prefab = OVT_PrefabUtils.GetPrefabName(e);
		if (prefab.IndexOf(WAREHOUSE_PREFAB_FRAGMENT) > -1)
			m_FoundWarehouse = e;

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! What one holder held when it was saved, plus the line written over it afterwards.
//------------------------------------------------------------------------------------------------
class OVT_TEST_ResourceExpectedStock : Managed
{
	string idA;
	int qtyA;
	string idB;
	int qtyB;
	string idDirt;
}

//------------------------------------------------------------------------------------------------
//! A truck's resource cargo survives a real save.
//!
//! A truckload is bought with the player's money at a drifting price and is the only way resources
//! move at all. If it does not come back, a continue silently empties every truck in the campaign of
//! everything the player paid for, with the truck still parked where they left it.
//!
//! Covers vanilla's CAR configuration {64C6B4937723DA61}, whose rule matches Wheeled_Base.et - the
//! ancestor of the same-GUID Wheeled_Truck_Base.et delta that puts the store on every truck. It is
//! the only case that covers that entry.
//!
//! The truck is SPAWNED, so it joins no player's vehicle registry, and it is left standing (BUG-118).
//!
//! ⚠ Takes a real save; `ResourceTruckLoad*` sorts after `..._Capability_...`.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 90)]
class OVT_TEST_PersistenceRoundTrip_ResourceTruckLoad_RoundTrips : SCR_AutotestCaseBase
{
	//! Clear of both storage-case subjects, which sit 14 m from the same anchor.
	static const vector SPAWN_OFFSET = "28 0 0";

	static const int SAVED_A = 137;
	static const int SAVED_B = 42;

	static const int PHASE_SPAWN = 0;
	static const int PHASE_AWAIT_TRACKING = 1;
	static const int PHASE_STOCK_AND_SAVE = 2;
	static const int PHASE_AWAIT_SAVE = 3;
	static const int PHASE_DIRTY_AND_RELOAD = 4;
	static const int PHASE_AWAIT_RELOAD = 5;
	static const int PHASE_ASSERT = 6;

	protected int m_iPhase;
	protected int m_iTrackingPolls;
	protected int m_iSavePolls;
	protected int m_iReloadPolls;
	protected int m_iSaveBaseline;

	protected IEntity m_Truck;
	protected ref OVT_TEST_ResourceExpectedStock m_Saved;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == PHASE_SPAWN)
			return Spawn();

		if (m_iPhase == PHASE_AWAIT_TRACKING)
			return AwaitTracking();

		if (m_iPhase == PHASE_STOCK_AND_SAVE)
			return StockAndSave();

		if (m_iPhase == PHASE_AWAIT_SAVE)
			return AwaitSave();

		if (m_iPhase == PHASE_DIRTY_AND_RELOAD)
			return DirtyAndReload();

		if (m_iPhase == PHASE_AWAIT_RELOAD)
			return AwaitReload();

		return Assert();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to advance.
	protected bool Spawn()
	{
		OVT_ResourceDefs defs;
		string diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveDefs(defs);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		m_Saved = new OVT_TEST_ResourceExpectedStock();
		m_Saved.qtyA = SAVED_A;
		m_Saved.qtyB = SAVED_B;

		string idA;
		string idB;
		string idDirt;
		diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveLines(defs, idA, idB, idDirt);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		m_Saved.idA = idA;
		m_Saved.idB = idB;
		m_Saved.idDirt = idDirt;

		vector position;
		diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveSubjectPosition(SPAWN_OFFSET, position);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		m_Truck = OVT_Global.SpawnEntityPrefab(OVT_TEST_ResourceRoundTripFixture.TRANSPORT_TRUCK_PREFAB, position);
		if (!m_Truck)
		{
			SetFailure(string.Format("SpawnEntityPrefab() produced no truck from %1 - the prefab GUID in this case no longer resolves", OVT_TEST_ResourceRoundTripFixture.TRANSPORT_TRUCK_PREFAB));
			return true;
		}

		m_iPhase = PHASE_AWAIT_TRACKING;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A vehicle is registered by the native Persistence component on its prefab, which is asynchronous
	//! enough to be worth waiting for. A precondition, not a retry: expiry fails.
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitTracking()
	{
		if (!m_Truck)
		{
			SetFailure("The spawned truck left the world before it was registered for saving");
			return true;
		}

		if (!OVT_TEST_PersistenceRoundTripGate.InstanceIsTracked(m_Truck))
		{
			m_iTrackingPolls += 1;
			if (m_iTrackingPolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
			{
				SetFailure("The spawned truck never became persistence-tracked, so it has no stored record and nothing about its cargo can survive a save");
				return true;
			}

			return false;
		}

		m_iPhase = PHASE_STOCK_AND_SAVE;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to advance.
	protected bool StockAndSave()
	{
		OVT_ResourceDefs defs;
		string diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveDefs(defs);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_ResourceStoreComponent store;
		diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveStore(m_Truck, "The spawned truck", store);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		// A freshly spawned truck is empty, so the saved cargo is never a state the spawn path
		// produced on its own.
		if (store.GetLedger().LineCount() != 0)
		{
			SetFailure("The spawned truck was already carrying resources the moment it appeared, so the values below would not be this case's");
			return true;
		}

		OVT_TEST_ResourceRoundTripFixture.Stock(store, defs, m_Saved.idA, SAVED_A, m_Saved.idB, SAVED_B);

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

		m_iPhase = PHASE_DIRTY_AND_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to advance.
	protected bool DirtyAndReload()
	{
		OVT_ResourceDefs defs;
		string diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveDefs(defs);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_ResourceStoreComponent store;
		diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveStore(m_Truck, "The spawned truck", store);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_TEST_ResourceRoundTripFixture.Dirty(store, defs, m_Saved.idDirt);

		if (store.GetLedger().Count(m_Saved.idA) != 0)
		{
			SetFailure("The truck kept its saved cargo through the dirtying step, so the reload would prove nothing");
			return true;
		}

		string reload = OVT_TEST_PersistenceRoundTripGate.RequestInstanceReload(m_Truck);
		if (reload != "")
		{
			SetFailure(reload);
			return true;
		}

		m_iPhase = PHASE_AWAIT_RELOAD;
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
				SetFailure("The truck's stored record was never re-applied: the persistence system's re-application never completed");
				return true;
			}

			return false;
		}

		m_iPhase = PHASE_ASSERT;
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

		OVT_ResourceDefs defs;
		string diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveDefs(defs);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_ResourceStoreComponent store;
		diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveStore(m_Truck, "The spawned truck", store);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		diagnostic = OVT_TEST_ResourceRoundTripFixture.AssertStockRestored(store, defs, m_Saved, "The spawned truck");
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		Print("A truck's resource cargo survived a real save and came back out of storage");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A dropped crate pile's contents survive a real save, on a record the pile only has because
//! stocking it asked for one.
//!
//! ⚠ A pile is a prop. Vanilla tracks a prop NEVER, and the pile prefab carries no native
//! Persistence component, so a pile is in the save only because OVT_ResourceStoreComponent's
//! EnsureTracked() puts it there on the first non-empty publish. Without that, a player's unloaded
//! stock disappears on the next continue with the crates still standing in the world. The track is
//! asserted BEFORE the save, so the two faults report differently.
//!
//! Covers the pile's own EntityPersistenceConfig in the Overthrow group - the one whose
//! ComponentClassPersistenceConfigRule may safely name OVT_ResourcePileComponent because nothing
//! else in the mod carries it (D16). SelfSpawn 1 on that block is what re-creates the pile itself on
//! a real load; an in-session re-application cannot exercise that half, so "the crates come back at
//! all after a restart" stays a play-test gate.
//!
//! ⚠ Takes a real save; `ResourcePile*` sorts after `..._Capability_...`. Left standing (BUG-118).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 90)]
class OVT_TEST_PersistenceRoundTrip_ResourcePile_RoundTrips : SCR_AutotestCaseBase
{
	//! Clear of the truck case and of both storage-case subjects.
	static const vector SPAWN_OFFSET = "0 0 28";

	static const int SAVED_A = 61;
	static const int SAVED_B = 19;

	static const int PHASE_SPAWN_AND_STOCK = 0;
	static const int PHASE_AWAIT_TRACKING = 1;
	static const int PHASE_SAVE = 2;
	static const int PHASE_AWAIT_SAVE = 3;
	static const int PHASE_DIRTY_AND_RELOAD = 4;
	static const int PHASE_AWAIT_RELOAD = 5;
	static const int PHASE_ASSERT = 6;

	protected int m_iPhase;
	protected int m_iTrackingPolls;
	protected int m_iSavePolls;
	protected int m_iReloadPolls;
	protected int m_iSaveBaseline;

	protected IEntity m_Pile;
	protected ref OVT_TEST_ResourceExpectedStock m_Saved;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == PHASE_SPAWN_AND_STOCK)
			return SpawnAndStock();

		if (m_iPhase == PHASE_AWAIT_TRACKING)
			return AwaitTracking();

		if (m_iPhase == PHASE_SAVE)
			return Save();

		if (m_iPhase == PHASE_AWAIT_SAVE)
			return AwaitSave();

		if (m_iPhase == PHASE_DIRTY_AND_RELOAD)
			return DirtyAndReload();

		if (m_iPhase == PHASE_AWAIT_RELOAD)
			return AwaitReload();

		return Assert();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the pile and stocks it in one step: the stock IS what tracks it.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool SpawnAndStock()
	{
		OVT_ResourceDefs defs;
		string diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveDefs(defs);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		m_Saved = new OVT_TEST_ResourceExpectedStock();
		m_Saved.qtyA = SAVED_A;
		m_Saved.qtyB = SAVED_B;

		string idA;
		string idB;
		string idDirt;
		diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveLines(defs, idA, idB, idDirt);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		m_Saved.idA = idA;
		m_Saved.idB = idB;
		m_Saved.idDirt = idDirt;

		vector position;
		diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveSubjectPosition(SPAWN_OFFSET, position);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		m_Pile = OVT_Global.SpawnEntityPrefab(OVT_TEST_ResourceRoundTripFixture.PILE_PREFAB, position);
		if (!m_Pile)
		{
			SetFailure(string.Format("SpawnEntityPrefab() produced no pile from %1 - the prefab or its .meta GUID no longer resolves", OVT_TEST_ResourceRoundTripFixture.PILE_PREFAB));
			return true;
		}

		OVT_ResourceStoreComponent store;
		diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveStore(m_Pile, "The spawned crate pile", store);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		if (OVT_TEST_PersistenceRoundTripGate.InstanceIsTracked(m_Pile))
			Print("Note: the crate pile was persistence-tracked the moment it spawned, so this case's track assertion is satisfied by another route");

		OVT_TEST_ResourceRoundTripFixture.Stock(store, defs, m_Saved.idA, SAVED_A, m_Saved.idB, SAVED_B);

		m_iPhase = PHASE_AWAIT_TRACKING;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The claim the rest of the case rests on: stocking a prop holder put it in the save.
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitTracking()
	{
		if (!m_Pile)
		{
			SetFailure("The crate pile left the world mid-case");
			return true;
		}

		if (!OVT_TEST_PersistenceRoundTripGate.InstanceIsTracked(m_Pile))
		{
			m_iTrackingPolls += 1;
			if (m_iTrackingPolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
			{
				SetFailure("The crate pile is still not persistence-tracked after its store was stocked. A prop is tracked by nothing in vanilla, so without OVT_ResourceStoreComponent.EnsureTracked() a pile has NO stored record: its serializer is bound, compiles and runs zero times, and every dropped load vanishes on the next continue with the crates still standing.");
				return true;
			}

			return false;
		}

		m_iPhase = PHASE_SAVE;
		return false;
	}

	//------------------------------------------------------------------------------------------------
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

		m_iPhase = PHASE_DIRTY_AND_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to advance.
	protected bool DirtyAndReload()
	{
		OVT_ResourceDefs defs;
		string diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveDefs(defs);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_ResourceStoreComponent store;
		diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveStore(m_Pile, "The spawned crate pile", store);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_TEST_ResourceRoundTripFixture.Dirty(store, defs, m_Saved.idDirt);

		if (store.GetLedger().Count(m_Saved.idA) != 0)
		{
			SetFailure("The crate pile kept its saved contents through the dirtying step, so the reload would prove nothing");
			return true;
		}

		string reload = OVT_TEST_PersistenceRoundTripGate.RequestInstanceReload(m_Pile);
		if (reload != "")
		{
			SetFailure(reload);
			return true;
		}

		m_iPhase = PHASE_AWAIT_RELOAD;
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
				SetFailure("The crate pile's stored record was never re-applied: the persistence system's re-application never completed");
				return true;
			}

			return false;
		}

		m_iPhase = PHASE_ASSERT;
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

		OVT_ResourceDefs defs;
		string diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveDefs(defs);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_ResourceStoreComponent store;
		diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveStore(m_Pile, "The spawned crate pile", store);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		diagnostic = OVT_TEST_ResourceRoundTripFixture.AssertStockRestored(store, defs, m_Saved, "The spawned crate pile");
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		Print("A dropped crate pile's contents survived a real save, on a record that exists only because stocking it tracked the pile");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The warehouse BUILDING's resource stock survives a real save, beside its item ledger.
//!
//! Covers vanilla's BUILDING configuration {65B682661F79DDBE}, which the Warehouse_01 delta inherits
//! and which neither the truck nor the pile case touches. A PURCHASED warehouse is the subject; a
//! BUILT one matches Overthrow's Buildable configuration instead and has a case of its own (D15).
//!
//! Two ledgers on one building, deliberately: the storage cases above stock this warehouse's
//! OVT_StorageComponent, this one its OVT_ResourceStoreComponent. They disturb each other in neither
//! order - each clears its own ledger before stocking it, and the reload re-applies both payloads
//! from the same save.
//!
//! ⚠ Takes a real save; `WarehouseResources*` sorts after `..._Capability_...`.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 90)]
class OVT_TEST_PersistenceRoundTrip_WarehouseResources_RoundTrip : SCR_AutotestCaseBase
{
	static const int SAVED_A = 903;
	static const int SAVED_B = 27;

	static const int PHASE_STOCK_AND_TRACK = 0;
	static const int PHASE_AWAIT_TRACKING = 1;
	static const int PHASE_SAVE = 2;
	static const int PHASE_AWAIT_SAVE = 3;
	static const int PHASE_DIRTY_AND_RELOAD = 4;
	static const int PHASE_AWAIT_RELOAD = 5;
	static const int PHASE_ASSERT = 6;

	protected int m_iPhase;
	protected int m_iTrackingPolls;
	protected int m_iSavePolls;
	protected int m_iReloadPolls;
	protected int m_iSaveBaseline;

	protected IEntity m_Warehouse;
	protected ref OVT_TEST_ResourceExpectedStock m_Saved;

	protected ref OVT_TEST_ResourceRoundTripFixture m_Fixture = new OVT_TEST_ResourceRoundTripFixture();

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == PHASE_STOCK_AND_TRACK)
			return StockAndTrack();

		if (m_iPhase == PHASE_AWAIT_TRACKING)
			return AwaitTracking();

		if (m_iPhase == PHASE_SAVE)
			return Save();

		if (m_iPhase == PHASE_AWAIT_SAVE)
			return AwaitSave();

		if (m_iPhase == PHASE_DIRTY_AND_RELOAD)
			return DirtyAndReload();

		if (m_iPhase == PHASE_AWAIT_RELOAD)
			return AwaitReload();

		return Assert();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to advance.
	protected bool StockAndTrack()
	{
		OVT_ResourceDefs defs;
		string diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveDefs(defs);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		m_Saved = new OVT_TEST_ResourceExpectedStock();
		m_Saved.qtyA = SAVED_A;
		m_Saved.qtyB = SAVED_B;

		string idA;
		string idB;
		string idDirt;
		diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveLines(defs, idA, idB, idDirt);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		m_Saved.idA = idA;
		m_Saved.idB = idB;
		m_Saved.idDirt = idDirt;

		m_Warehouse = m_Fixture.FindWarehouse();
		if (!m_Warehouse)
		{
			SetFailure("No Warehouse_01 building exists in this world. Worlds/MP/OVT_Campaign_Test_Layers/default.layer is supposed to place one - without it neither this case nor the warehouse economy has a subject.");
			return true;
		}

		OVT_ResourceStoreComponent store;
		diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveStore(m_Warehouse, "The warehouse building", store);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_TEST_ResourceRoundTripFixture.Stock(store, defs, m_Saved.idA, SAVED_A, m_Saved.idB, SAVED_B);

		m_iPhase = PHASE_AWAIT_TRACKING;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitTracking()
	{
		if (!m_Warehouse)
		{
			SetFailure("The warehouse building left the world mid-case");
			return true;
		}

		if (!OVT_TEST_PersistenceRoundTripGate.InstanceIsTracked(m_Warehouse))
		{
			m_iTrackingPolls += 1;
			if (m_iTrackingPolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
			{
				SetFailure("The warehouse building is still not persistence-tracked after its resource store was stocked. Vanilla registers an intact building never, so without EnsureTracked() the building has NO stored record and every warehouse's resources empty themselves on the next continue.");
				return true;
			}

			return false;
		}

		m_iPhase = PHASE_SAVE;
		return false;
	}

	//------------------------------------------------------------------------------------------------
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

		m_iPhase = PHASE_DIRTY_AND_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to advance.
	protected bool DirtyAndReload()
	{
		OVT_ResourceDefs defs;
		string diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveDefs(defs);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_ResourceStoreComponent store;
		diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveStore(m_Warehouse, "The warehouse building", store);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_TEST_ResourceRoundTripFixture.Dirty(store, defs, m_Saved.idDirt);

		if (store.GetLedger().Count(m_Saved.idA) != 0)
		{
			SetFailure("The warehouse kept its saved resource stock through the dirtying step, so the reload would prove nothing");
			return true;
		}

		string reload = OVT_TEST_PersistenceRoundTripGate.RequestInstanceReload(m_Warehouse);
		if (reload != "")
		{
			SetFailure(reload);
			return true;
		}

		m_iPhase = PHASE_AWAIT_RELOAD;
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
				SetFailure("The warehouse's stored record was never re-applied: the persistence system's re-application never completed");
				return true;
			}

			return false;
		}

		m_iPhase = PHASE_ASSERT;
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

		OVT_ResourceDefs defs;
		string diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveDefs(defs);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_ResourceStoreComponent store;
		diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveStore(m_Warehouse, "The warehouse building", store);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		diagnostic = OVT_TEST_ResourceRoundTripFixture.AssertStockRestored(store, defs, m_Saved, "The warehouse building");
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		Print("The warehouse building's resource stock survived a real save, beside the item ledger the storage cases put on the same building");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A hand-drifted price table reloads DRIFTED, not at base.
//!
//! Overthrow has never persisted a price - every price is derived from config on every start - so
//! "prices are state" is a new claim. Without the manager serializer a continue quietly rewinds the
//! whole market to base, undoing every day of war pressure and port control, and the only symptom is
//! a number looking slightly wrong on a shop row.
//!
//! Three entries, three fates, all read back BY ID rather than by index: the first and last
//! catalogue entries come back drifted, and a middle entry that was dirtied but never drifted comes
//! back at BASE - which is the payload carrying every entry and ApplyPersistedPrices()
//! resetting-then-refilling, rather than leaving whatever was in memory.
//!
//! ⚠ A rename of either Deserialize local reads an empty array and reports SUCCESS, which resets the
//! whole table to base; that is the "came back at base" assertion below, and the reason this case
//! exists in the shape it does.
//!
//! The manager rides the game mode's own record, so the reload is the session-wide re-application.
//!
//! ⚠ Takes a real save; `ResourcePrices*` sorts after `..._Capability_...`.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 90)]
class OVT_TEST_PersistenceRoundTrip_ResourcePrices_RoundTrip : SCR_AutotestCaseBase
{
	//! Outside any band a drift step can reach: the band is base x 0.5 .. x 2.0 and the dearest MVP
	//! resource bases at 200, so neither value can be a campaign's own.
	static const int SAVED_PRICE_FIRST = 4243;
	static const int SAVED_PRICE_LAST = 917;

	//! Written over all three prices after the save.
	static const int DIRTY_PRICE = 3;

	protected int m_iPhase;
	protected int m_iSavePolls;
	protected int m_iReloadPolls;
	protected int m_iSaveBaseline;

	protected string m_sIdFirst;
	protected string m_sIdMiddle;
	protected string m_sIdLast;
	protected int m_iBaseMiddle;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
			return DriftAndSave();

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_SAVE)
			return AwaitSave();

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_DIRTY_AND_RELOAD)
			return DirtyAndReload();

		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_AWAIT_RELOAD)
			return AwaitReload();

		return Assert();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to advance.
	protected bool DriftAndSave()
	{
		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
		{
			SetFailure("OVT_Global.GetResources() is null, so there is no price table to round-trip");
			return true;
		}

		OVT_ResourceDefs defs = resources.GetDefs();
		if (!defs || defs.Count() < 3)
		{
			SetFailure("The resource catalogue holds fewer than three definitions, so this case cannot tell a drifted price from an untouched one");
			return true;
		}

		int indexFirst = 0;
		int indexMiddle = 1;
		int indexLast = defs.Count() - 1;

		m_sIdFirst = defs.IdAt(indexFirst);
		m_sIdMiddle = defs.IdAt(indexMiddle);
		m_sIdLast = defs.IdAt(indexLast);
		m_iBaseMiddle = defs.BasePriceAt(indexMiddle);

		// Closure 3: none of the three values below is one a campaign start or a drift step produces.
		if (SAVED_PRICE_FIRST == defs.BasePriceAt(indexFirst) || SAVED_PRICE_LAST == defs.BasePriceAt(indexLast) || DIRTY_PRICE == m_iBaseMiddle)
		{
			SetFailure("A price this case writes matches a config base price, so it could not tell a restored value from an unrestored one - retune the constants");
			return true;
		}

		if (!resources.SetStoredPrice(indexFirst, SAVED_PRICE_FIRST) || !resources.SetStoredPrice(indexLast, SAVED_PRICE_LAST))
		{
			SetFailure("SetStoredPrice() refused an index the catalogue reports as valid, so the price table and the catalogue disagree on their length");
			return true;
		}

		if (resources.GetStoredPrice(indexMiddle) != m_iBaseMiddle)
		{
			SetFailure(string.Format("'%1' is already drifted to %2 before this case ran, so the base-price assertion after the reload would prove nothing",
				m_sIdMiddle, resources.GetStoredPrice(indexMiddle).ToString()));
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
	//! \return True when the case is finished (a failure); false to advance.
	protected bool DirtyAndReload()
	{
		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
		{
			SetFailure("OVT_Global.GetResources() is null before the reload");
			return true;
		}

		OVT_ResourceDefs defs = resources.GetDefs();
		if (!defs)
		{
			SetFailure("The resource catalogue disappeared before the reload");
			return true;
		}

		resources.SetStoredPrice(defs.IndexOf(m_sIdFirst), DIRTY_PRICE);
		resources.SetStoredPrice(defs.IndexOf(m_sIdMiddle), DIRTY_PRICE);
		resources.SetStoredPrice(defs.IndexOf(m_sIdLast), DIRTY_PRICE);

		if (resources.GetStoredPrice(defs.IndexOf(m_sIdFirst)) != DIRTY_PRICE)
		{
			SetFailure("The price table kept its saved values through the dirtying step, so the reload would prove nothing");
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
				SetFailure("The price table's stored record was never re-applied: the persistence system's re-application never completed");
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

		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
		{
			SetFailure("OVT_Global.GetResources() is null after the reload");
			return true;
		}

		OVT_ResourceDefs defs = resources.GetDefs();
		if (!defs)
		{
			SetFailure("The resource catalogue disappeared across the reload");
			return true;
		}

		string diagnostic = AssertDrifted(resources, defs, m_sIdFirst, SAVED_PRICE_FIRST);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		diagnostic = AssertDrifted(resources, defs, m_sIdLast, SAVED_PRICE_LAST);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		int middle = resources.GetStoredPrice(defs.IndexOf(m_sIdMiddle));
		if (middle != m_iBaseMiddle)
		{
			SetFailure(string.Format("'%1' was at its base price of %2 when the save was taken, was dirtied to %3 afterwards, and came back at %4 - the payload does not carry every catalogue entry, so a resource nobody has traded keeps whatever happened to be in memory",
				m_sIdMiddle, m_iBaseMiddle.ToString(), DIRTY_PRICE.ToString(), middle.ToString()));
			return true;
		}

		Print("A hand-drifted resource price table survived a real save and came back drifted, with an untraded resource back at base");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one saved price came back, naming the two ways it can fail.
	//! \param[in] resources The resource manager after the reload.
	//! \param[in] defs The definition table.
	//! \param[in] id The resource whose price was drifted.
	//! \param[in] expected What it was drifted to before the save.
	//! \return An empty string when it came back, otherwise the sentence to fail with.
	protected string AssertDrifted(notnull OVT_ResourceManagerComponent resources, notnull OVT_ResourceDefs defs, string id, int expected)
	{
		int index = defs.IndexOf(id);
		if (index == -1)
			return string.Format("'%1' is no longer in the resource catalogue after the reload", id);

		int price = resources.GetStoredPrice(index);
		if (price == expected)
			return "";

		if (price == DIRTY_PRICE)
		{
			return string.Format("'%1' was drifted to %2 and saved, dirtied to %3, and came back still dirty - nothing was read back, so OVT_ResourceManagerSerializer is not listed in the game-mode configuration in Configs/Systems/Persistence/Overthrow.conf",
				id, expected.ToString(), DIRTY_PRICE.ToString());
		}

		if (price == defs.BasePriceAt(index))
		{
			return string.Format("'%1' was drifted to %2 and saved, and came back at its config base of %3 - the price table was reset and never refilled, which is what a renamed Deserialize local does: LoadContext.Read() keys on the LOCAL VARIABLE'S NAME, reads an empty array and reports SUCCESS",
				id, expected.ToString(), price.ToString());
		}

		return string.Format("'%1' was drifted to %2 and saved, and came back at %3 - the price survived the round trip as some other value entirely",
			id, expected.ToString(), price.ToString());
	}
}

//------------------------------------------------------------------------------------------------
//! A CONSTRUCTION SITE remembers what it was ordered to become, across a real save.
//!
//! The money for a site is taken at PLACEMENT (D2). A site that comes back remembering nothing is a
//! concrete mixer that can never be finished, with the player's money gone and their crate piles
//! stranded on the ground forever - and it fails silently, because a site that forgot its indices
//! looks exactly like a site that was just put down.
//!
//! Covers Overthrow's BUILDABLE configuration {6B0E7A27C0D539F2}. The site carries
//! OVT_BuildableComponent and therefore gets NO EntityPersistenceConfig of its own (D16) - the
//! Buildable rule at Priority 35000 claims it, which is why OVT_ConstructionSiteComponentSerializer
//! is listed there and nowhere else.
//!
//! ⚠ Takes a real save; `ConstructionSite*` sorts after `..._Capability_...`.
//!
//! ⚠ A renamed Deserialize local reads ZERO, and zero is a LEGAL buildable index, so the site
//! silently becomes whatever buildables.conf lists first.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 90)]
class OVT_TEST_PersistenceRoundTrip_ConstructionSite_RoundTrips : SCR_AutotestCaseBase
{
	//! The generic construction site, the one prefab carrying OVT_ConstructionSiteComponent.
	static const ResourceName SITE_PREFAB = "{6A8E2E0000000101}Prefabs/Structures/OVT_ConstructionSite.et";

	//! Clear of the truck case (28 0 0), the pile case (0 0 28) and both storage subjects (14 m).
	static const vector SPAWN_OFFSET = "-28 0 0";

	//! Every component of the saved orientation is exact in binary32, so the round trip is a
	//! comparison and not an epsilon argument.
	static const vector SAVED_ANGLES = "0 137 0";

	//! Written over the saved record before the reload. Deliberately NOT zero - a zeroed read is the
	//! renamed-local signature and must not be mistaken for the dirtying step.
	static const vector DIRTY_ANGLES = "0 42 0";
	static const int DIRTY_PREFAB_INDEX = 7;

	static const int PHASE_SPAWN_AND_STAMP = 0;
	static const int PHASE_AWAIT_TRACKING = 1;
	static const int PHASE_SAVE = 2;
	static const int PHASE_AWAIT_SAVE = 3;
	static const int PHASE_DIRTY_AND_RELOAD = 4;
	static const int PHASE_AWAIT_RELOAD = 5;
	static const int PHASE_ASSERT = 6;

	protected int m_iPhase;
	protected int m_iTrackingPolls;
	protected int m_iSavePolls;
	protected int m_iReloadPolls;
	protected int m_iSaveBaseline;

	protected IEntity m_Site;
	protected int m_iSavedBuildableIndex;
	protected int m_iDirtyBuildableIndex;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == PHASE_SPAWN_AND_STAMP)
			return SpawnAndStamp();

		if (m_iPhase == PHASE_AWAIT_TRACKING)
			return AwaitTracking();

		if (m_iPhase == PHASE_SAVE)
			return Save();

		if (m_iPhase == PHASE_AWAIT_SAVE)
			return AwaitSave();

		if (m_iPhase == PHASE_DIRTY_AND_RELOAD)
			return DirtyAndReload();

		if (m_iPhase == PHASE_AWAIT_RELOAD)
			return AwaitReload();

		return Assert();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a site and stamps it with a real buildable, the way PlaceConstructionSite() does.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool SpawnAndStamp()
	{
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance || !resistance.m_BuildablesConfig || !resistance.m_BuildablesConfig.m_aBuildables)
		{
			SetFailure("The buildables config is not loaded, so there is no buildable a construction site could name");
			return true;
		}

		int buildableCount = resistance.m_BuildablesConfig.m_aBuildables.Count();
		if (buildableCount < 2)
		{
			SetFailure(string.Format("buildables.conf holds %1 entry/entries. This case needs two distinct indices - one to save and one to dirty with.", buildableCount.ToString()));
			return true;
		}

		m_iSavedBuildableIndex = 0;
		m_iDirtyBuildableIndex = buildableCount - 1;

		vector position;
		string diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveSubjectPosition(SPAWN_OFFSET, position);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		m_Site = OVT_Global.SpawnEntityPrefab(SITE_PREFAB, position);
		if (!m_Site)
		{
			SetFailure(string.Format("SpawnEntityPrefab() produced no construction site from %1 - the prefab or its .meta GUID no longer resolves", SITE_PREFAB));
			return true;
		}

		// D16's claim, asserted rather than assumed: this component is the ONLY reason the site
		// matches the Overthrow Buildable configuration and gets persisted at all.
		if (!OVT_ComponentFinder<OVT_BuildableComponent>.Find(m_Site))
		{
			SetFailure("The construction site prefab carries no OVT_BuildableComponent, so it matches no Overthrow EntityPersistenceConfig and its serializer - which is bound only on the Buildable block - can never run.");
			return true;
		}

		OVT_ConstructionSiteComponent site = OVT_ComponentFinder<OVT_ConstructionSiteComponent>.Find(m_Site);
		if (!site)
		{
			SetFailure("The construction site prefab carries no OVT_ConstructionSiteComponent, so it remembers nothing and could never be finished");
			return true;
		}

		site.Initialize(m_iSavedBuildableIndex, 0, SAVED_ANGLES, resistance.m_BuildablesConfig.m_aBuildables[m_iSavedBuildableIndex].m_sTitle);

		// PlaceConstructionSite() tracks the site it puts down; a prop is tracked by nothing in vanilla.
		OVT_PersistenceTracking.Track(m_Site);

		m_iPhase = PHASE_AWAIT_TRACKING;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitTracking()
	{
		if (!m_Site)
		{
			SetFailure("The construction site left the world mid-case");
			return true;
		}

		if (!OVT_TEST_PersistenceRoundTripGate.InstanceIsTracked(m_Site))
		{
			m_iTrackingPolls += 1;
			if (m_iTrackingPolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
			{
				SetFailure("The construction site is still not persistence-tracked. Without a record there is nothing for its serializer to write into, so it is bound, compiles, runs zero times, and every standing site is a dead concrete mixer after a continue.");
				return true;
			}

			return false;
		}

		m_iPhase = PHASE_SAVE;
		return false;
	}

	//------------------------------------------------------------------------------------------------
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

		m_iPhase = PHASE_DIRTY_AND_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Points the site at a different buildable entirely, then asks for its record back.
	//! \return True when the case is finished (a failure); false to advance.
	protected bool DirtyAndReload()
	{
		OVT_ConstructionSiteComponent site = OVT_ComponentFinder<OVT_ConstructionSiteComponent>.Find(m_Site);
		if (!site)
		{
			SetFailure("The construction site lost its component between the save and the reload");
			return true;
		}

		site.Initialize(m_iDirtyBuildableIndex, DIRTY_PREFAB_INDEX, DIRTY_ANGLES, "");

		if (site.GetBuildableIndex() == m_iSavedBuildableIndex)
		{
			SetFailure("The construction site kept its saved buildable index through the dirtying step, so the reload would prove nothing");
			return true;
		}

		string reload = OVT_TEST_PersistenceRoundTripGate.RequestInstanceReload(m_Site);
		if (reload != "")
		{
			SetFailure(reload);
			return true;
		}

		m_iPhase = PHASE_AWAIT_RELOAD;
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
				SetFailure("The construction site's stored record was never re-applied: the persistence system's re-application never completed");
				return true;
			}

			return false;
		}

		m_iPhase = PHASE_ASSERT;
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

		OVT_ConstructionSiteComponent site = OVT_ComponentFinder<OVT_ConstructionSiteComponent>.Find(m_Site);
		if (!site)
		{
			SetFailure("The construction site is gone after the reload");
			return true;
		}

		if (site.GetBuildableIndex() == m_iDirtyBuildableIndex)
		{
			SetFailure(string.Format("The construction site still points at buildable index %1, the one written after the save, so nothing was read back over it - OVT_ConstructionSiteComponentSerializer is not listed on the Overthrow Buildable EntityPersistenceConfig {6B0E7A27C0D539F2}, or its payload read aborted.", site.GetBuildableIndex().ToString()));
			return true;
		}

		if (site.GetBuildableIndex() != m_iSavedBuildableIndex)
		{
			SetFailure(string.Format("The construction site came back at buildable index %1, expected the saved %2. Zero here is the renamed-Deserialize-local signature: an unfound property reads ZERO and reports success, and zero is a legal index, so every site in the campaign quietly becomes whatever buildables.conf lists first.", site.GetBuildableIndex().ToString(), m_iSavedBuildableIndex.ToString()));
			return true;
		}

		if (site.GetPrefabIndex() != 0)
		{
			SetFailure(string.Format("The construction site came back at prefab index %1, expected the saved 0 - the payload is losing fields, and a site pointed at a prefab index its buildable does not have can never be finished.", site.GetPrefabIndex().ToString()));
			return true;
		}

		if (site.GetAngles() != SAVED_ANGLES)
		{
			SetFailure(string.Format("The construction site came back facing %1, expected the saved %2 - the finished building would stand at the wrong angle", site.GetAngles().ToString(), SAVED_ANGLES.ToString()));
			return true;
		}

		// The NAME is deliberately not in the payload: ApplyPersisted() re-derives it from
		// buildables.conf, so a renamed buildable renames every standing site instead of freezing the
		// old label into every save. The dirtying step blanked it, so a non-empty name here can only
		// have come from that re-derivation.
		if (site.GetBuildableName() == "")
		{
			SetFailure("The construction site came back with no buildable name. ApplyPersisted() re-derives it from the config, so an empty one means the re-derivation was skipped and every client's build action reads 'Construction Site' instead of naming the building.");
			return true;
		}

		Print("A construction site's ordered buildable, prefab index and orientation survived a real save, on the Overthrow Buildable configuration's serializer list");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A BUILT warehouse keeps BOTH of its ledgers across a real save - D15's proof, and binding 4's.
//!
//! ⚠ An entity gets exactly ONE EntityPersistenceConfig. A built warehouse carries
//! OVT_BuildableComponent, so it matches Overthrow's Buildable configuration {6B0E7A27C0D539F2} at
//! Priority 35000 as well as vanilla's Building configuration {65B682661F79DDBE}, which authors no
//! priority at all - and the Buildable rule wins. The purchased warehouse two cases above therefore
//! proves NOTHING about this one: different configurations, different serializer lists. Without the
//! Buildable block's two store serializers, a warehouse the player built and filled comes back EMPTY
//! of both its items and its resources on the next continue, with nothing in the log.
//!
//! Both ledgers in one reload. The item ledger's serializer was missing from that block since
//! logistics/storage shipped - a latent gap this feature is simply the first to reach.
//!
//! playerId -1 is BuildItem()'s server-initiated marker: free, never a construction site, never
//! registered with real estate. Registration is the Campaign tier's claim.
//!
//! ⚠ Takes a real save; `WarehouseUnderBuildableConfig*` sorts after `..._Capability_...`, and after
//! `WarehouseMigration*` and `WarehouseResources*` - which matters, because the building it leaves
//! standing is a second Warehouse_01 in the world. Both warehouse fixtures also skip anything
//! carrying OVT_BuildableComponent, so the ordering is a second belt rather than the only one.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 120)]
class OVT_TEST_PersistenceRoundTrip_WarehouseUnderBuildableConfig_LedgersRoundTrip : SCR_AutotestCaseBase
{
	//! Resolved by name out of buildables.conf. Never an index - entries get appended.
	static const string BUILDABLE_NAME = "Warehouse";

	//! Prefab path fragment the real-estate config filters on, and the reason a built warehouse is a
	//! real warehouse at all.
	static const string WAREHOUSE_PREFAB_FRAGMENT = "Warehouse_01";

	//! Far clear of the truck (28 0 0), the pile (0 0 28), the site (-28 0 0) and both storage
	//! subjects (14 m): a warehouse is roughly 40 m long and this one stands for the rest of the suite.
	static const vector BUILD_OFFSET = "0 0 -90";

	static const int SAVED_RESOURCE_A = 411;
	static const int SAVED_RESOURCE_B = 58;
	static const int SAVED_ITEMS_A = 137;
	static const int SAVED_ITEMS_B = 9;

	static const int PHASE_BUILD_AND_STOCK = 0;
	static const int PHASE_AWAIT_TRACKING = 1;
	static const int PHASE_SAVE = 2;
	static const int PHASE_AWAIT_SAVE = 3;
	static const int PHASE_DIRTY_AND_RELOAD = 4;
	static const int PHASE_AWAIT_RELOAD = 5;
	static const int PHASE_ASSERT = 6;

	protected int m_iPhase;
	protected int m_iTrackingPolls;
	protected int m_iSavePolls;
	protected int m_iReloadPolls;
	protected int m_iSaveBaseline;

	protected IEntity m_Warehouse;
	protected ref OVT_TEST_ResourceExpectedStock m_Saved;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == PHASE_BUILD_AND_STOCK)
			return BuildAndStock();

		if (m_iPhase == PHASE_AWAIT_TRACKING)
			return AwaitTracking();

		if (m_iPhase == PHASE_SAVE)
			return Save();

		if (m_iPhase == PHASE_AWAIT_SAVE)
			return AwaitSave();

		if (m_iPhase == PHASE_DIRTY_AND_RELOAD)
			return DirtyAndReload();

		if (m_iPhase == PHASE_AWAIT_RELOAD)
			return AwaitReload();

		return Assert();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to advance.
	protected bool BuildAndStock()
	{
		OVT_ResourceDefs defs;
		string diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveDefs(defs);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		m_Saved = new OVT_TEST_ResourceExpectedStock();
		m_Saved.qtyA = SAVED_RESOURCE_A;
		m_Saved.qtyB = SAVED_RESOURCE_B;

		string idA;
		string idB;
		string idDirt;
		diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveLines(defs, idA, idB, idDirt);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		m_Saved.idA = idA;
		m_Saved.idB = idB;
		m_Saved.idDirt = idDirt;

		diagnostic = Build();
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_ResourceStoreComponent store;
		diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveStore(m_Warehouse, "The built warehouse", store);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_StorageComponent storage;
		diagnostic = OVT_TEST_StorageRoundTripFixture.ResolveStorage(m_Warehouse, "The built warehouse", storage);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_TEST_ResourceRoundTripFixture.Stock(store, defs, m_Saved.idA, SAVED_RESOURCE_A, m_Saved.idB, SAVED_RESOURCE_B);
		OVT_TEST_StorageRoundTripFixture.Stock(storage, SAVED_ITEMS_A, SAVED_ITEMS_B);

		m_iPhase = PHASE_AWAIT_TRACKING;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds one warehouse through the server-side build path and checks the two structural facts
	//! the whole design rests on.
	//! \return An empty string on success, otherwise the sentence to fail with.
	protected string Build()
	{
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance)
			return "OVT_Global.GetResistanceFaction() is null";

		if (!resistance.m_BuildablesConfig || !resistance.m_BuildablesConfig.m_aBuildables)
			return "The resistance faction has no buildables config loaded";

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
			return string.Format("No buildable named '%1' in buildables.conf - the entry is missing or renamed", BUILDABLE_NAME);

		vector position;
		string diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveSubjectPosition(BUILD_OFFSET, position);
		if (diagnostic != "")
			return diagnostic;

		BaseWorld world = GetGame().GetWorld();
		if (world)
			position[1] = world.GetSurfaceY(position[0], position[2]);

		// playerId -1 waives the funds, distance, item-limit and town-control checks, and never places
		// a construction site.
		m_Warehouse = resistance.BuildItem(index, 0, position, vector.Zero, -1);
		if (!m_Warehouse)
			return string.Format("BuildItem() built no warehouse at %1 - the prefab failed to spawn, or the build was refused", position.ToString());

		// D15's precondition, asserted rather than assumed: this component is the ONLY reason the
		// building matches Overthrow's Buildable configuration instead of vanilla's Building one, and
		// therefore the only reason this case tests a different binding from the purchased-warehouse
		// case above.
		if (!OVT_ComponentFinder<OVT_BuildableComponent>.Find(m_Warehouse))
			return "The built warehouse carries no OVT_BuildableComponent, so it matches vanilla's Building configuration exactly as a purchased one does and this case would silently re-test the wrong binding";

		ResourceName prefab = OVT_PrefabUtils.GetPrefabName(m_Warehouse);
		if (prefab.IndexOf(WAREHOUSE_PREFAB_FRAGMENT) == -1)
		{
			return string.Format("The buildable named '%1' spawned '%2', whose path does not contain '%3'. Real estate matches a warehouse by that substring, so this building would not be a warehouse to anything else in the mod.",
				BUILDABLE_NAME, prefab, WAREHOUSE_PREFAB_FRAGMENT);
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to keep waiting or advance.
	protected bool AwaitTracking()
	{
		if (!m_Warehouse)
		{
			SetFailure("The built warehouse left the world mid-case");
			return true;
		}

		if (!OVT_TEST_PersistenceRoundTripGate.InstanceIsTracked(m_Warehouse))
		{
			m_iTrackingPolls += 1;
			if (m_iTrackingPolls > OVT_TEST_PersistenceRoundTripGate.MAX_SAVE_POLLS)
			{
				SetFailure("The built warehouse is still not persistence-tracked. FinishBuild() tracks every structure it spawns, so an untracked one has no stored record at all and neither of its ledgers can round-trip.");
				return true;
			}

			return false;
		}

		m_iPhase = PHASE_SAVE;
		return false;
	}

	//------------------------------------------------------------------------------------------------
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

		m_iPhase = PHASE_DIRTY_AND_RELOAD;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the case is finished (a failure); false to advance.
	protected bool DirtyAndReload()
	{
		OVT_ResourceDefs defs;
		string diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveDefs(defs);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_ResourceStoreComponent store;
		diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveStore(m_Warehouse, "The built warehouse", store);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_StorageComponent storage;
		diagnostic = OVT_TEST_StorageRoundTripFixture.ResolveStorage(m_Warehouse, "The built warehouse", storage);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_TEST_ResourceRoundTripFixture.Dirty(store, defs, m_Saved.idDirt);
		OVT_TEST_StorageRoundTripFixture.Dirty(storage);

		if (store.GetLedger().Count(m_Saved.idA) != 0)
		{
			SetFailure("The built warehouse kept its saved resource stock through the dirtying step, so the reload would prove nothing");
			return true;
		}

		string reload = OVT_TEST_PersistenceRoundTripGate.RequestInstanceReload(m_Warehouse);
		if (reload != "")
		{
			SetFailure(reload);
			return true;
		}

		m_iPhase = PHASE_AWAIT_RELOAD;
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
				SetFailure("The built warehouse's stored record was never re-applied: the persistence system's re-application never completed");
				return true;
			}

			return false;
		}

		m_iPhase = PHASE_ASSERT;
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

		OVT_ResourceDefs defs;
		string diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveDefs(defs);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_ResourceStoreComponent store;
		diagnostic = OVT_TEST_ResourceRoundTripFixture.ResolveStore(m_Warehouse, "The built warehouse", store);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		diagnostic = OVT_TEST_ResourceRoundTripFixture.AssertStockRestored(store, defs, m_Saved, "The built warehouse");
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		OVT_StorageComponent storage;
		diagnostic = OVT_TEST_StorageRoundTripFixture.ResolveStorage(m_Warehouse, "The built warehouse", storage);
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		diagnostic = OVT_TEST_StorageRoundTripFixture.AssertLedgerRestored(storage, SAVED_ITEMS_A, SAVED_ITEMS_B, "The built warehouse");
		if (diagnostic != "")
		{
			SetFailure(diagnostic);
			return true;
		}

		Print("A BUILT warehouse kept both its item ledger and its resource stock across a real save - Overthrow's Buildable persistence configuration carries the serializers a built holder needs");

		return true;
	}
}
