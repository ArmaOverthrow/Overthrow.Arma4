//------------------------------------------------------------------------------------------------
//! TIER D' - QUARANTINED. THIS SUITE IS RED BY DESIGN AND IS IN NO GROUP CONFIG.
//!
//! IT IS NEVER PART OF A DEFAULT OR CI RUN. IT MUST NOT BE ADDED TO OVT_TestGroup_Fast,
//! OVT_TestGroup_All, OR ANY FUTURE GROUP. IT EXITS 1 TODAY, ON PURPOSE, BECAUSE THE BRANCH HAS
//! NO WORKING SAVE PATH IN EITHER PERSISTENCE SYSTEM (findings.md 1.7).
//!
//! ITS EXIT CODE IS THE `vanilla-persistence` MIGRATION'S ACCEPTANCE CRITERION:
//!
//!     exit 1  =  the migration is not complete
//!     exit 0  =  the migration is complete
//!
//! WHOEVER COMPLETES THAT MIGRATION SHOULD RUN THIS SUITE UNTIL IT IS GREEN, THEN DELETE THE
//! QUARANTINE (THIS HEADER), ADD THE SUITE TO OVT_TestGroup_All, AND UPDATE
//! docs/features/core/persistence/.
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
//! The one trigger is OVT_TEST_PersistenceRoundTripGate.TriggerSave() below - a single annotated
//! call, reached by every case through one helper so that it exists exactly once in the tree.
//! Everything else - what is written, what is read back - goes through Overthrow's public manager
//! API, which is why this suite can be the gate at all: it does not know or care whether the
//! storage underneath is EPF or vanilla, only that what went in comes back.
//! ===========================================================================================
//!
//! ---------------------------------------------------------------------------------------------
//! RUN RECIPE / ACCEPTANCE PROCEDURE (also in tools/README.md)
//!
//!   # fresh campaign - REQUIRED, see the anti-vacuous-pass design below
//!   .scripts/reset_save.sh --profile OverthrowCI
//!   # or, once fixtures exist, a known saved state:
//!   .scripts/activate_save.sh <name> --profile OverthrowCI
//!
//!   tools/run-tests.sh OVT_TEST_PersistenceRoundTripSuite
//!
//!   exit 1 today, with `Persistence capability absent: ...` in .tmp/run-tests/junit.xml
//!   exit 0 once the migration lands  <- that flip is the acceptance criterion
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
//!     CLOSURE: no STATE-KIND case depends on order - each independently requires the capability
//!     before it does anything else, and each is content to find a save already present.
//!     ONE ORDER DEPENDENCY DOES EXIST, deliberately, and it is the capability case's
//!     RequireFreshSession() half: HasSaveGame() must be FALSE before this suite's first save, which
//!     holds only because `..._Capability_...` sorts alphabetically first and therefore runs before
//!     any other case has called RequireSaveCapability(). The condition that would break it, stated
//!     so it can be checked: a case added to this suite whose class name sorts BEFORE the capability
//!     case AND that triggers a save. Such a case would turn the capability gate's fresh-session
//!     check into a "precondition violated" failure. Keep the capability case first, or keep any
//!     earlier-sorting case save-free.
//!
//! ---------------------------------------------------------------------------------------------
//! THE RELOAD MECHANISM IS WRITTEN FOR RUNG L1 AND IS CURRENTLY UNREACHABLE.
//!
//! Phase 1 selected rung L3 (no save path), so the capability gate fails before any case reaches
//! the reload. The reload is nevertheless implemented rather than stubbed, so that the suite can go
//! green on its own the day persistence works: it re-requests the test world through the same
//! framework helper the suite Setup uses, waits for the transition, and then requires a started
//! campaign to have come back - which, with a working persistence system, is what loading a save
//! means. Finding 1.9 proved the script VM survives a framework-initiated in-session transition,
//! but a transition initiated from a Main step has never been exercised; whoever completes the
//! migration should expect to iterate here, and should not assume this part is proven.
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
//! The single seam this suite is allowed to touch, plus the shared round-trip machinery.
//!
//! Everything persistence-layer-facing in the entire test tree is in this one class, so that a
//! reviewer checking Decision 4 has exactly one place to look and the cases stay behaviour-level.
//------------------------------------------------------------------------------------------------
class OVT_TEST_PersistenceRoundTripGate
{
	//! The failure text a reviewer reads in junit.xml when the migration is not done.
	static const string CAPABILITY_ABSENT =
		"Persistence capability absent: SaveGame() produced no save (HasSaveGame() still false). The vanilla-persistence migration is not complete.";

	//! Phase constants for the per-case round-trip state machine.
	static const int PHASE_MUTATE_AND_SAVE = 0;
	static const int PHASE_AWAIT_RELOAD = 1;
	static const int PHASE_ASSERT = 2;

	//! Frame polls allowed for the world to come back after a reload request.
	//! A diagnostic backstop, not a retry: exceeding it fails with what was observed instead of
	//! hanging until the harness timeout.
	static const int MAX_RELOAD_POLLS = 1200;

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
	//! Triggers a save and requires that one now exists.
	//!
	//! Called by EVERY case before it does anything else, so no case depends on another having run.
	//! \return An empty string when a save was produced, otherwise a diagnostic naming the gap.
	static string RequireSaveCapability()
	{
		string diagnostic;
		OVT_PersistenceManagerComponent persistence = ResolvePersistence(diagnostic);
		if (!persistence)
			return diagnostic;

		// ===========================================================================================
		// THE SINGLE PERMITTED PERSISTENCE-LAYER SEAM IN THE ENTIRE TEST TREE (Decision 4).
		// Chosen over the player-facing RequestSave() RPC because it is server-side, needs no player
		// entity, and is the method that RPC ends up calling anyway. Nothing else in Scripts/Game/Tests/
		// may touch the persistence layer, and no assertion anywhere may name a storage type.
		persistence.SaveGame();
		// ===========================================================================================

		if (!persistence.HasSaveGame())
			return CAPABILITY_ABSENT;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Requests the in-session reload that a round trip needs (rung L1).
	//!
	//! Uses the same framework helper the suite Setup uses, so it inherits Overthrow's world and
	//! addon selection from OVT_AutotestFramework.c rather than restating it.
	//! \return An empty string when the transition was requested, otherwise a diagnostic.
	static string RequestSessionReload()
	{
		ResourceName world = SCR_AutotestHelper.GetDefaultWorld();
		ResourceName systems = SCR_AutotestHelper.GetDefaultSystemsConfig();

		if (!SCR_AutotestHelper.WorldOpenFile(world, systems))
			return "Reload failed: the world transition request was rejected for " + world;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! True while the requested reload is still in flight.
	//! \return True when a world transition is requested or running.
	static bool ReloadInProgress()
	{
		return GameStateTransitions.IsTransitionRequestedOrInProgress();
	}

	//------------------------------------------------------------------------------------------------
	//! Requires that the reloaded session came back as a running campaign.
	//!
	//! With a working persistence system, loading a save IS what makes the campaign running again
	//! after a world load - the suite's own campaign-start Setup step does not run a second time.
	//! A reload that comes back to the start menu therefore means the save was not loaded.
	//! \return An empty string when a started campaign is present, otherwise a diagnostic.
	static string RequireRestoredCampaign()
	{
		OVT_OverthrowGameMode mode = OVT_Global.GetOverthrow();
		if (!mode)
			return "Reload restored no Overthrow game mode: the persistence layer did not bring the session back.";

		if (!mode.HasGameStarted())
			return "Reload came back to a campaign that was never started: the save was not loaded. The vanilla-persistence migration is not complete.";

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
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 30)]
class OVT_TEST_PersistenceRoundTrip_Capability_SaveGameProducesASave : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		string fresh = OVT_TEST_PersistenceRoundTripGate.RequireFreshSession();
		if (fresh != "")
		{
			SetResultFailure(fresh);
			return true;
		}

		string gate = OVT_TEST_PersistenceRoundTripGate.RequireSaveCapability();
		if (gate != "")
		{
			SetResultFailure(gate);
			return true;
		}

		Print("Persistence capability present: SaveGame() produced a save");
		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Player money survives a save and a reload.
//!
//! Mutate to a distinctive amount, save, DIRTY the amount, reload, assert the saved amount came
//! back. The dirty step is what makes a no-op reload fail (closure 2).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 30)]
class OVT_TEST_PersistenceRoundTrip_PlayerMoney_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	//! Written before the save. Not a value the campaign start would produce.
	static const int SAVED_MONEY = 424242;

	//! Written after the save and before the reload, so a reload that restores nothing is caught.
	static const int DIRTY_MONEY = 7;

	protected int m_iPhase;
	protected int m_iReloadPolls;
	protected string m_sPersId;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			string gate = OVT_TEST_PersistenceRoundTripGate.RequireSaveCapability();
			if (gate != "")
			{
				SetResultFailure(gate);
				return true;
			}

			OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
			if (!economy)
			{
				SetResultFailure("OVT_Global.GetEconomy() is null");
				return true;
			}

			string diagnostic;
			m_sPersId = OVT_TEST_PersistenceSubject.ResolveLocalPersistentId(diagnostic);
			if (m_sPersId == "")
			{
				SetResultFailure("Cannot resolve the persistent player ID: %1", diagnostic);
				return true;
			}

			int playerId = OVT_TEST_PersistenceSubject.ResolveLocalPlayerId(diagnostic);
			if (playerId < 1)
			{
				SetResultFailure("Cannot resolve the runtime player ID: %1", diagnostic);
				return true;
			}

			// Set an exact known balance, then save it.
			economy.TakePlayerMoney(playerId, economy.GetPlayerMoney(m_sPersId));
			economy.AddPlayerMoney(playerId, SAVED_MONEY);

			gate = OVT_TEST_PersistenceRoundTripGate.RequireSaveCapability();
			if (gate != "")
			{
				SetResultFailure(gate);
				return true;
			}

			// Dirty it: a reload that restores nothing now cannot pass.
			economy.TakePlayerMoney(playerId, SAVED_MONEY - DIRTY_MONEY);

			string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
			if (reload != "")
			{
				SetResultFailure(reload);
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
					SetResultFailure("Reload never completed: still in transition after %1 polls", m_iReloadPolls.ToString());
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
			SetResultFailure(restored);
			return true;
		}

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
		{
			SetResultFailure("OVT_Global.GetEconomy() is null after the reload");
			return true;
		}

		int money = economy.GetPlayerMoney(m_sPersId);
		if (money != SAVED_MONEY)
		{
			SetResultFailure("Money did not survive the round trip: saved %1, read back %2 (dirty value was %3)",
				SAVED_MONEY.ToString(), money.ToString(), DIRTY_MONEY.ToString());
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Player XP and skill levels survive a save and a reload.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 30)]
class OVT_TEST_PersistenceRoundTrip_PlayerSkills_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	//! XP written before the save.
	static const int SAVED_XP = 900;

	//! XP removed after the save, so the record is wrong until a real restore fixes it.
	static const int DIRTY_XP = 850;

	protected int m_iPhase;
	protected int m_iReloadPolls;
	protected string m_sPersId;
	protected string m_sSkillKey;
	protected int m_iSavedSkillLevel;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			string gate = OVT_TEST_PersistenceRoundTripGate.RequireSaveCapability();
			if (gate != "")
			{
				SetResultFailure(gate);
				return true;
			}

			OVT_SkillManagerComponent skills = OVT_Global.GetSkills();
			OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
			if (!skills || !players)
			{
				SetResultFailure("The skill manager or the player manager is null");
				return true;
			}

			string diagnostic;
			m_sPersId = OVT_TEST_PersistenceSubject.ResolveLocalPersistentId(diagnostic);
			if (m_sPersId == "")
			{
				SetResultFailure("Cannot resolve the persistent player ID: %1", diagnostic);
				return true;
			}

			int playerId = OVT_TEST_PersistenceSubject.ResolveLocalPlayerId(diagnostic);
			if (playerId < 1)
			{
				SetResultFailure("Cannot resolve the runtime player ID: %1", diagnostic);
				return true;
			}

			m_sSkillKey = OVT_TEST_PersistenceSubject.ResolveFirstSkillKey(diagnostic);
			if (m_sSkillKey == "")
			{
				SetResultFailure("Cannot resolve a skill to exercise: %1", diagnostic);
				return true;
			}

			skills.GiveXP(playerId, SAVED_XP);
			skills.AddSkillLevel(playerId, m_sSkillKey);
			m_iSavedSkillLevel = OVT_TEST_PersistenceSubject.GetPlayerSkillLevel(m_sPersId, m_sSkillKey);

			gate = OVT_TEST_PersistenceRoundTripGate.RequireSaveCapability();
			if (gate != "")
			{
				SetResultFailure(gate);
				return true;
			}

			skills.TakeXP(playerId, DIRTY_XP);

			string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
			if (reload != "")
			{
				SetResultFailure(reload);
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
					SetResultFailure("Reload never completed: still in transition after %1 polls", m_iReloadPolls.ToString());
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
			SetResultFailure(restored);
			return true;
		}

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
		{
			SetResultFailure("OVT_Global.GetPlayers() is null after the reload");
			return true;
		}

		OVT_PlayerData player = players.GetPlayer(m_sPersId);
		if (!player)
		{
			SetResultFailure("The reloaded session has no player record for '%1'", m_sPersId);
			return true;
		}

		if (player.xp != SAVED_XP)
		{
			SetResultFailure("XP did not survive the round trip: saved %1, read back %2",
				SAVED_XP.ToString(), player.xp.ToString());
			return true;
		}

		int skillLevel = OVT_TEST_PersistenceSubject.GetPlayerSkillLevel(m_sPersId, m_sSkillKey);
		if (skillLevel != m_iSavedSkillLevel)
		{
			SetResultFailure("Skill '%1' did not survive the round trip: saved level %2, read back %3",
				m_sSkillKey, m_iSavedSkillLevel.ToString(), skillLevel.ToString());
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Building ownership survives a save and a reload.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 30)]
class OVT_TEST_PersistenceRoundTrip_RealEstateOwnership_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	protected int m_iPhase;
	protected int m_iReloadPolls;
	protected string m_sPersId;
	protected vector m_vBuildingPos;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			string gate = OVT_TEST_PersistenceRoundTripGate.RequireSaveCapability();
			if (gate != "")
			{
				SetResultFailure(gate);
				return true;
			}

			OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
			if (!realEstate)
			{
				SetResultFailure("OVT_Global.GetRealEstate() is null");
				return true;
			}

			string diagnostic;
			m_sPersId = OVT_TEST_PersistenceSubject.ResolveLocalPersistentId(diagnostic);
			if (m_sPersId == "")
			{
				SetResultFailure("Cannot resolve the persistent player ID: %1", diagnostic);
				return true;
			}

			IEntity building = OVT_TEST_PersistenceSubject.ResolveUnownedBuilding(diagnostic);
			if (!building)
			{
				SetResultFailure("Cannot resolve a building to own: %1", diagnostic);
				return true;
			}

			// Ownership is keyed by position, which is also how it is found again after a reload -
			// the entity reference itself does not survive a world transition.
			m_vBuildingPos = building.GetOrigin();
			realEstate.SetOwnerPersistentId(m_sPersId, building);

			gate = OVT_TEST_PersistenceRoundTripGate.RequireSaveCapability();
			if (gate != "")
			{
				SetResultFailure(gate);
				return true;
			}

			// Dirty it.
			realEstate.SetOwner(-1, building);

			string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
			if (reload != "")
			{
				SetResultFailure(reload);
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
					SetResultFailure("Reload never completed: still in transition after %1 polls", m_iReloadPolls.ToString());
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
			SetResultFailure(restored);
			return true;
		}

		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if (!realEstate)
		{
			SetResultFailure("OVT_Global.GetRealEstate() is null after the reload");
			return true;
		}

		string owner = realEstate.GetOwnerIDFromPos(m_vBuildingPos);
		if (owner != m_sPersId)
		{
			SetResultFailure("Ownership did not survive the round trip at %1: owner is '%2', expected '%3'",
				m_vBuildingPos.ToString(), owner, m_sPersId);
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A recruit record survives a save and a reload.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 30)]
class OVT_TEST_PersistenceRoundTrip_Recruits_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	//! Explicit name so the assertion does not depend on the random name generator.
	static const string RECRUIT_NAME = "Autotest Round Trip Recruit";

	//! XP written before the save.
	static const int SAVED_XP = 900;

	protected int m_iPhase;
	protected int m_iReloadPolls;
	protected string m_sPersId;
	protected string m_sRecruitId;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			string gate = OVT_TEST_PersistenceRoundTripGate.RequireSaveCapability();
			if (gate != "")
			{
				SetResultFailure(gate);
				return true;
			}

			OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
			if (!recruits)
			{
				SetResultFailure("OVT_Global.GetRecruits() is null");
				return true;
			}

			string diagnostic;
			m_sPersId = OVT_TEST_PersistenceSubject.ResolveLocalPersistentId(diagnostic);
			if (m_sPersId == "")
			{
				SetResultFailure("Cannot resolve the persistent player ID: %1", diagnostic);
				return true;
			}

			IEntity subject = OVT_TEST_PersistenceSubject.ResolveRecruitSubjectEntity(diagnostic);
			if (!subject)
			{
				SetResultFailure("Cannot resolve an entity to attach a recruit to: %1", diagnostic);
				return true;
			}

			m_sRecruitId = recruits.AddRecruit(m_sPersId, subject, RECRUIT_NAME);
			if (m_sRecruitId == "")
			{
				SetResultFailure("AddRecruit() returned no recruit ID");
				return true;
			}

			recruits.AddRecruitXP(m_sRecruitId, SAVED_XP);

			gate = OVT_TEST_PersistenceRoundTripGate.RequireSaveCapability();
			if (gate != "")
			{
				SetResultFailure(gate);
				return true;
			}

			// Dirty it: the recruit is gone until a real restore brings it back.
			recruits.RemoveRecruit(m_sRecruitId);

			string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
			if (reload != "")
			{
				SetResultFailure(reload);
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
					SetResultFailure("Reload never completed: still in transition after %1 polls", m_iReloadPolls.ToString());
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
			SetResultFailure(restored);
			return true;
		}

		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if (!recruits)
		{
			SetResultFailure("OVT_Global.GetRecruits() is null after the reload");
			return true;
		}

		OVT_RecruitData recruit = recruits.GetRecruit(m_sRecruitId);
		if (!recruit)
		{
			SetResultFailure("Recruit '%1' did not survive the round trip - the reloaded session does not have it", m_sRecruitId);
			return true;
		}

		if (recruit.m_iXP != SAVED_XP)
		{
			SetResultFailure("Recruit XP did not survive the round trip: saved %1, read back %2",
				SAVED_XP.ToString(), recruit.m_iXP.ToString());
			return true;
		}

		if (recruit.GetName() != RECRUIT_NAME)
		{
			SetResultFailure("Recruit name did not survive the round trip: expected '%1', read back '%2'",
				RECRUIT_NAME, recruit.GetName());
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Town control survives a save and a reload.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 30)]
class OVT_TEST_PersistenceRoundTrip_TownControl_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	protected int m_iPhase;
	protected int m_iReloadPolls;
	protected int m_iTownId;
	protected int m_iSavedFaction;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			string gate = OVT_TEST_PersistenceRoundTripGate.RequireSaveCapability();
			if (gate != "")
			{
				SetResultFailure(gate);
				return true;
			}

			OVT_TownManagerComponent towns = OVT_Global.GetTowns();
			OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
			if (!towns || !config)
			{
				SetResultFailure("The town manager or the config component is null");
				return true;
			}

			string diagnostic;
			OVT_TownData town = OVT_TEST_PersistenceSubject.ResolveFirstTown(m_iTownId, diagnostic);
			if (!town)
			{
				SetResultFailure("Cannot resolve a town: %1", diagnostic);
				return true;
			}

			// Hand the town to the player faction - not a state the campaign start produces.
			m_iSavedFaction = config.GetPlayerFactionIndex();
			towns.ChangeTownControl(town, m_iSavedFaction);

			gate = OVT_TEST_PersistenceRoundTripGate.RequireSaveCapability();
			if (gate != "")
			{
				SetResultFailure(gate);
				return true;
			}

			// Dirty it.
			towns.ChangeTownControl(town, config.GetOccupyingFactionIndex());

			string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
			if (reload != "")
			{
				SetResultFailure(reload);
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
					SetResultFailure("Reload never completed: still in transition after %1 polls", m_iReloadPolls.ToString());
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
			SetResultFailure(restored);
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetResultFailure("OVT_Global.GetTowns() is null after the reload");
			return true;
		}

		OVT_TownData town = towns.GetTown(m_iTownId);
		if (!town)
		{
			SetResultFailure("The reloaded session has no town %1", m_iTownId.ToString());
			return true;
		}

		if (town.faction != m_iSavedFaction)
		{
			SetResultFailure("Town control did not survive the round trip: saved faction %1, read back %2",
				m_iSavedFaction.ToString(), town.faction.ToString());
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Town population survives a save and a reload.
//!
//! Uses the same closest-public-seam reasoning as the green suite: the only public mutator that
//! moves population is the supporter seam, which needs support raised first.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 30)]
class OVT_TEST_PersistenceRoundTrip_TownPopulation_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	//! Removed before the save.
	static const int SAVED_DELTA = 5;

	//! Removed again after the save, so the population is wrong until a real restore fixes it.
	static const int DIRTY_DELTA = 3;

	protected int m_iPhase;
	protected int m_iReloadPolls;
	protected int m_iTownId;
	protected int m_iSavedPopulation;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			string gate = OVT_TEST_PersistenceRoundTripGate.RequireSaveCapability();
			if (gate != "")
			{
				SetResultFailure(gate);
				return true;
			}

			OVT_TownManagerComponent towns = OVT_Global.GetTowns();
			if (!towns)
			{
				SetResultFailure("OVT_Global.GetTowns() is null");
				return true;
			}

			string diagnostic;
			OVT_TownData town = OVT_TEST_PersistenceSubject.ResolveFirstTown(m_iTownId, diagnostic);
			if (!town)
			{
				SetResultFailure("Cannot resolve a town: %1", diagnostic);
				return true;
			}

			towns.AddSupport(town.location, SAVED_DELTA + DIRTY_DELTA);
			towns.TakeSupportersFromNearestTown(town.location, SAVED_DELTA);

			OVT_TownData afterTake = towns.GetTown(m_iTownId);
			if (!afterTake)
			{
				SetResultFailure("The town manager stopped handing out town %1 after taking supporters",
					m_iTownId.ToString());
				return true;
			}

			m_iSavedPopulation = afterTake.population;

			gate = OVT_TEST_PersistenceRoundTripGate.RequireSaveCapability();
			if (gate != "")
			{
				SetResultFailure(gate);
				return true;
			}

			// Dirty it.
			towns.TakeSupportersFromNearestTown(town.location, DIRTY_DELTA);

			string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
			if (reload != "")
			{
				SetResultFailure(reload);
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
					SetResultFailure("Reload never completed: still in transition after %1 polls", m_iReloadPolls.ToString());
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
			SetResultFailure(restored);
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetResultFailure("OVT_Global.GetTowns() is null after the reload");
			return true;
		}

		OVT_TownData town = towns.GetTown(m_iTownId);
		if (!town)
		{
			SetResultFailure("The reloaded session has no town %1", m_iTownId.ToString());
			return true;
		}

		if (town.population != m_iSavedPopulation)
		{
			SetResultFailure("Town population did not survive the round trip: saved %1, read back %2",
				m_iSavedPopulation.ToString(), town.population.ToString());
			return true;
		}

		SetResultSuccess();
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
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 30)]
class OVT_TEST_PersistenceRoundTrip_TownStability_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	protected int m_iPhase;
	protected int m_iReloadPolls;
	protected int m_iTownId;
	protected int m_iModifierIndex;

	//! What the modifier system computed from the stored modifier list at save time.
	protected int m_iSavedStability;

	//! What live stability became once the modifier was removed, reported in the failure text.
	protected int m_iDirtyStability;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			string gate = OVT_TEST_PersistenceRoundTripGate.RequireSaveCapability();
			if (gate != "")
			{
				SetResultFailure(gate);
				return true;
			}

			OVT_TownManagerComponent towns = OVT_Global.GetTowns();
			if (!towns)
			{
				SetResultFailure("OVT_Global.GetTowns() is null");
				return true;
			}

			string diagnostic;
			OVT_TownData town = OVT_TEST_PersistenceSubject.ResolveFirstTown(m_iTownId, diagnostic);
			if (!town)
			{
				SetResultFailure("Cannot resolve a town: %1", diagnostic);
				return true;
			}

			OVT_TownModifierSystem system = towns.GetModifierSystem(OVT_TownStabilityModifierSystem);
			if (!system || !system.m_Config || !system.m_Config.m_aModifiers)
			{
				SetResultFailure("The town manager has no stability modifier system with a loaded config");
				return true;
			}

			m_iModifierIndex = FindNegativeModifierIndex(system);
			if (m_iModifierIndex < 0)
			{
				SetResultFailure("No stability modifier has a negative base effect - nothing that can move stability down from its maximum");
				return true;
			}

			int stabilityBefore = town.stability;

			if (!towns.TryAddStabilityModifier(m_iTownId, m_iModifierIndex))
			{
				SetResultFailure("TryAddStabilityModifier(%1) refused to add a modifier to town %2",
					m_iModifierIndex.ToString(), m_iTownId.ToString());
				return true;
			}

			OVT_TownData afterAdd = towns.GetTown(m_iTownId);
			if (!afterAdd)
			{
				SetResultFailure("The town manager stopped handing out town %1 after the modifier was added",
					m_iTownId.ToString());
				return true;
			}

			// The value to be persisted, derived from the modifier list that is actually stored.
			m_iSavedStability = system.Recalculate(afterAdd.stabilityModifiers);
			if (afterAdd.stability != m_iSavedStability)
			{
				SetResultFailure("Stored stability %1 disagrees with the modifier system's recalculation %2 before the save",
					afterAdd.stability.ToString(), m_iSavedStability.ToString());
				return true;
			}

			if (m_iSavedStability >= stabilityBefore)
			{
				SetResultFailure("A negative stability modifier did not lower stability: was %1, is now %2 - there would be nothing for a reload to restore",
					stabilityBefore.ToString(), m_iSavedStability.ToString());
				return true;
			}

			gate = OVT_TEST_PersistenceRoundTripGate.RequireSaveCapability();
			if (gate != "")
			{
				SetResultFailure(gate);
				return true;
			}

			// Dirty it: removing the modifier recalculates stability back up, so live state now
			// disagrees with what was saved.
			towns.RemoveStabilityModifier(m_iTownId, m_iModifierIndex);

			OVT_TownData afterRemove = towns.GetTown(m_iTownId);
			if (!afterRemove)
			{
				SetResultFailure("The town manager stopped handing out town %1 after the modifier was removed",
					m_iTownId.ToString());
				return true;
			}

			m_iDirtyStability = afterRemove.stability;
			if (m_iDirtyStability == m_iSavedStability)
			{
				SetResultFailure("Removing the modifier left stability at the saved value %1 - the reload would have nothing to prove",
					m_iSavedStability.ToString());
				return true;
			}

			string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
			if (reload != "")
			{
				SetResultFailure(reload);
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
					SetResultFailure("Reload never completed: still in transition after %1 polls", m_iReloadPolls.ToString());
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
			SetResultFailure(restored);
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetResultFailure("OVT_Global.GetTowns() is null after the reload");
			return true;
		}

		OVT_TownData town = towns.GetTown(m_iTownId);
		if (!town)
		{
			SetResultFailure("The reloaded session has no town %1", m_iTownId.ToString());
			return true;
		}

		if (town.stability != m_iSavedStability)
		{
			SetResultFailure("Town stability did not survive the round trip: saved %1, read back %2 (dirty value was %3)",
				m_iSavedStability.ToString(), town.stability.ToString(), m_iDirtyStability.ToString());
			return true;
		}

		SetResultSuccess();
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
[Test(suite: OVT_TEST_PersistenceRoundTripSuite, timeoutS: 30)]
class OVT_TEST_PersistenceRoundTrip_TownSupport_SurvivesSaveAndReload : SCR_AutotestCaseBase
{
	//! Added before the save.
	static const int SAVED_DELTA = 7;

	protected int m_iPhase;
	protected int m_iReloadPolls;
	protected int m_iTownId;
	protected int m_iSavedSupport;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		if (m_iPhase == OVT_TEST_PersistenceRoundTripGate.PHASE_MUTATE_AND_SAVE)
		{
			string gate = OVT_TEST_PersistenceRoundTripGate.RequireSaveCapability();
			if (gate != "")
			{
				SetResultFailure(gate);
				return true;
			}

			OVT_TownManagerComponent towns = OVT_Global.GetTowns();
			if (!towns)
			{
				SetResultFailure("OVT_Global.GetTowns() is null");
				return true;
			}

			string diagnostic;
			OVT_TownData town = OVT_TEST_PersistenceSubject.ResolveFirstTown(m_iTownId, diagnostic);
			if (!town)
			{
				SetResultFailure("Cannot resolve a town: %1", diagnostic);
				return true;
			}

			towns.AddSupport(town.location, SAVED_DELTA);

			OVT_TownData afterAdd = towns.GetTown(m_iTownId);
			if (!afterAdd)
			{
				SetResultFailure("The town manager stopped handing out town %1 after AddSupport()",
					m_iTownId.ToString());
				return true;
			}

			m_iSavedSupport = afterAdd.support;

			gate = OVT_TEST_PersistenceRoundTripGate.RequireSaveCapability();
			if (gate != "")
			{
				SetResultFailure(gate);
				return true;
			}

			// Dirty it.
			towns.ResetSupport(town);

			string reload = OVT_TEST_PersistenceRoundTripGate.RequestSessionReload();
			if (reload != "")
			{
				SetResultFailure(reload);
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
					SetResultFailure("Reload never completed: still in transition after %1 polls", m_iReloadPolls.ToString());
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
			SetResultFailure(restored);
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetResultFailure("OVT_Global.GetTowns() is null after the reload");
			return true;
		}

		OVT_TownData town = towns.GetTown(m_iTownId);
		if (!town)
		{
			SetResultFailure("The reloaded session has no town %1", m_iTownId.ToString());
			return true;
		}

		if (town.support != m_iSavedSupport)
		{
			SetResultFailure("Town support did not survive the round trip: saved %1, read back %2",
				m_iSavedSupport.ToString(), town.support.ToString());
			return true;
		}

		SetResultSuccess();
		return true;
	}
}
