//------------------------------------------------------------------------------------------------
//! TIER C - a STARTED campaign. World loaded, managers alive, and the two-call start sequence run
//! by OVT_TEST_SuiteBase before any case in this suite executes.
//!
//! What this tier is for: the state that only exists AFTER OVT_OverthrowGameMode.DoStartNewGame()
//! and DoStartGame() have run - town activation, shop stocking, income. Everything that is already
//! true at world load belongs one tier down, in OVT_TEST_InitSuite, where it costs no campaign
//! start; everything that is pure maths belongs in OVT_TEST_LogicSuite, where it costs no world.
//!
//! THE CAMPAIGN START IS NOT THIS SUITE'S CODE. It lives in OVT_TEST_SuiteBase behind
//! RequiresStartedCampaign(), because the sequence has sharp edges (non-idempotent; the difficulty
//! preset must be selected by NAME, since the test world's layer APPENDS its preset rather than
//! replacing the prefab's four; a start menu that opens milliseconds later) that a copy in each
//! suite would eventually get wrong. See that file's header, and findings.md 1.3.
//!
//! SETTLING BUDGETS (findings.md 1.4, measured - not read off the CallLater constants):
//!   game mode flags        same frame        timeoutS 30
//!   base controllers       first frame       timeoutS 30
//!   shop inventory         <= 604 ms         timeoutS 30
//!   occupying resources    ~6.5 s            timeoutS 45   (not asserted here)
//!   deployments            ~12 s             timeoutS 60   (out of scope for this feature)
//! Every case in this suite uses 30, and every case that cannot answer on its first tick polls with
//! a bounded backstop so that a change in settling behaviour fails with a message rather than with
//! a bare harness timeout.
//!
//! WHAT THIS SUITE MUST NOT ASSERT
//!  - GARRISONS. OVT_BaseData.garrison is never populated in the test world - measured over a 16 s
//!    window (findings.md 1.4). A garrison assertion would be red on arrival and would say nothing
//!    about Overthrow.
//!  - DEPLOYMENTS. They arrive ~12 s after the start; covering them would triple this suite's
//!    runtime for an area this feature scoped out.
//!  - Magic counts. The test world has exactly ONE town and ONE base.
//!
//! EXPECTED CONSOLE NOISE: a campaign start emits 62 deterministic virtual-machine exceptions about
//! 6.5 s in, from a null dereference in the occupying faction's initial resource distribution
//! (findings.md, Bugs found #1). They are a pre-existing gameplay bug, they do not fail a case, and
//! console error counts are not a usable signal for this suite until that bug is fixed.
//!
//! HOUSE RULES
//!  - Cases run alphabetically by class name and MUST be independent. A case that mutates campaign
//!    state restores it through the same public API before it returns.
//!  - Finish promptly. The town manager's modifier tick runs every 10 s from the campaign start and
//!    recalculates support, stability and population; a case still polling by then would race it,
//!    and that would look like flakiness rather than the design error it is.
//!  - [BaseContainerProps()] is MANDATORY on a concrete suite class, or a group config silently
//!    instantiates nothing (findings.md 1.10). Phase 6 puts this suite in the All group.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_TEST_CampaignSuite : OVT_TEST_SuiteBase
{
	//------------------------------------------------------------------------------------------------
	//! Opts this suite into the base class's campaign-start Setup step.
	//! \return Always true for this suite.
	override bool RequiresStartedCampaign()
	{
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The campaign really started, and it started on the difficulty preset the suite base selected.
//!
//! Two claims, both cheap and both load-bearing for every other case in this tier:
//!  1. HasGameStarted() and IsInitialized() are BOTH true. They are separate flags set at different
//!     points of DoStartGame() - the first before the managers are told to start, the second after -
//!     so a half-completed start shows up as the first being true and the second false.
//!  2. The difficulty in force is the one named by OVT_TEST_SuiteBase.CAMPAIGN_DIFFICULTY_PRESET.
//!     This is the assertion that catches the silent failure mode the suite base was written to
//!     avoid: selecting the preset by INDEX quietly runs the campaign tiers on 'Easy' (500 starting
//!     cash instead of 100000), which does not fail anything - it just makes every economy
//!     expectation subtly wrong.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 30)]
class OVT_TEST_Campaign_GameMode_IsStartedAndInitialized : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_OverthrowGameMode mode = OVT_Global.GetOverthrow();
		if (!mode)
		{
			SetResultFailure("OVT_Global.GetOverthrow() is null in a campaign-tier case");
			return true;
		}

		if (!mode.HasGameStarted())
		{
			SetResultFailure("HasGameStarted() is false after the suite's campaign-start Setup step");
			return true;
		}

		if (!mode.IsInitialized())
		{
			SetResultFailure("IsInitialized() is false although HasGameStarted() is true - DoStartGame() did not run to completion");
			return true;
		}

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (!difficulty)
		{
			SetResultFailure("OVT_Global.GetDifficulty() is null after the campaign started");
			return true;
		}

		if (difficulty.name != OVT_TEST_SuiteBase.CAMPAIGN_DIFFICULTY_PRESET)
		{
			SetResultFailure("The campaign is running on difficulty preset '%1', expected '%2' - the suite base's by-name selection did not take effect",
				difficulty.name, OVT_TEST_SuiteBase.CAMPAIGN_DIFFICULTY_PRESET);
			return true;
		}

		PrintFormat("Campaign started: HasGameStarted and IsInitialized true, difficulty '%1', starting cash %2",
			difficulty.name, difficulty.startingCash.ToString());

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Towns are ACTIVATED by the campaign start, not merely discovered by it.
//!
//! Town discovery happens at world load (asserted one tier down, in OVT_TEST_Init_Towns_ArePopulated).
//! Activation is a different thing and happens only at campaign start:
//! OVT_TownManagerComponent.PostGameStart() walks every registered town controller and calls
//! ActivateTown(), which - for any town larger than a village - spawns that town's gun dealer and
//! writes the dealer's position back onto the town record.
//!
//! That written-back position is the observable this case waits for. It is the end of a four-link
//! chain (game mode -> town manager PostGameStart -> town controller ActivateTown -> dealer spawned
//! and recorded), so a break anywhere along it shows up here, and it cannot be true before the
//! campaign starts: a freshly discovered town's dealer position is the zero vector.
//!
//! The dealer spawn is deferred by one frame (CallLater(..., 0)), so the case polls. The poll has a
//! bounded backstop that reports what it last saw, rather than letting the case die on the harness
//! timeout with no explanation.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 30)]
class OVT_TEST_Campaign_Towns_AreActivated : SCR_AutotestCaseBase
{
	//! Diagnostic backstop. The dealer is expected within a frame or two of the campaign start;
	//! this is a bound on the explanation, not a retry budget.
	static const int MAX_POLLS = 600;

	//! Polls spent waiting for a town to report an activated state.
	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetResultFailure("OVT_Global.GetTowns() is null");
			return true;
		}

		array<ref OVT_TownData> townList = towns.GetTowns();
		if (!townList || townList.Count() < 1)
		{
			SetResultFailure("No towns are registered, so none can have been activated");
			return true;
		}

		if (towns.m_TownControllers.Count() < 1)
		{
			SetResultFailure("No town controllers are registered, so PostGameStart() had nothing to activate");
			return true;
		}

		int activated = CountActivatedTowns(townList);
		if (activated > 0)
		{
			PrintFormat("Towns activated: %1 of %2 have a gun dealer recorded after %3 poll(s)",
				activated.ToString(), townList.Count().ToString(), m_iPolls.ToString());
			SetResultSuccess();
			return true;
		}

		m_iPolls += 1;
		if (m_iPolls > MAX_POLLS)
		{
			SetResultFailure("No town was activated within %1 polls: %2 town(s) and %3 town controller(s) are registered, but no town has a gun dealer position recorded",
				m_iPolls.ToString(), townList.Count().ToString(), towns.m_TownControllers.Count().ToString());
			return true;
		}

		return false; // keep polling
	}

	//------------------------------------------------------------------------------------------------
	//! Counts towns whose controller has finished activating them.
	//! \param[in] townList The manager's town records.
	//! \return Number of towns with a gun dealer position written back onto the record.
	protected int CountActivatedTowns(array<ref OVT_TownData> townList)
	{
		int activated = 0;

		foreach (OVT_TownData town : townList)
		{
			if (!town)
				continue;

			if (town.gunDealerPosition != vector.Zero)
				activated += 1;
		}

		return activated;
	}
}
