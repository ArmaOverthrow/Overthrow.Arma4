//------------------------------------------------------------------------------------------------
//! Base class for every Overthrow autotest suite.
//!
//! This is the single inheritance point for all Overthrow suites.
//! World selection is NOT done here - it comes from OVT_AutotestFramework.c via the modded
//! SCR_AutotestHelper statics, which the inherited GetWorldFile()/GetWorldSystemsConfigFile()
//! already route to. Overriding those here would defeat the single integration point.
//! (A suite that wants NO world at all - the pure-logic tier - overrides GetWorldFile() itself
//! to return ResourceName.Empty; that is a per-suite decision, not a shared one.)
//!
//! What this class adds on top of SCR_AutotestSuiteBase:
//!
//!  1. RequiresStartedCampaign() - an opt-in virtual, default false. A suite that needs a running
//!     Overthrow campaign overrides it to return true.
//!  2. Setup_StartCampaign() - a Setup step that starts the campaign for those suites and is a
//!     no-op for everyone else. Base-class Setup steps run AFTER the inherited BI steps
//!     (Setup_OpenWorld / Setup_AwaitWorld / Setup_CloseMenus) and BEFORE derived-suite and case
//!     steps, so the world is fully loaded by the time this runs (verified empirically, see
//!     docs/features/dev-ops/test-coverage/findings.md 1.6).
//!  3. ResolveManager() - resolves a manager component from the LIVE game mode. DEFENSIVE ONLY,
//!     see the method's own header.
//!
//! IMPORTANT - the campaign start is a TEST concern and nothing else.
//! Shipped code must never rely on it, call it, or assume anything about it. In the real game the
//! campaign is started by the player pressing Start in OVT_StartGameContext (or by a dedicated
//! server auto-starting); this class merely reproduces that same public two-call sequence so a
//! suite can assert on a started campaign. It lives here rather than being copy-pasted into each
//! campaign-tier suite because the sequence has sharp edges (non-idempotent, difficulty preset
//! must be picked by NAME, a start menu that opens milliseconds later) that a reader would not
//! spot in a copy.
//------------------------------------------------------------------------------------------------
class OVT_TEST_SuiteBase : SCR_AutotestSuiteBase
{
	//! Difficulty preset the campaign tiers run on, selected BY NAME.
	//!
	//! NEVER select the preset by index. The test world's layer declares an m_aDifficultyPresets
	//! override, and Enfusion APPENDS that array to the game mode prefab's four presets rather than
	//! replacing them: at runtime there are 5 presets, index 0 is 'Easy' and 'Test World' is index 4
	//! (findings.md 1.3c). An index-based selection silently runs the campaign tiers on Easy and
	//! invalidates every economy anchor.
	static const string CAMPAIGN_DIFFICULTY_PRESET = "Test World";

	//! Poll budget for the campaign to report IsInitialized().
	//! Observed cost on 1.7.0.54 is ONE poll / 71 ms (findings.md 1.3), so this is a diagnostic
	//! backstop, not a retry: exceeding it fails the suite with a message that says what was
	//! actually observed, instead of hanging until the harness timeout with no explanation.
	static const int MAX_START_POLLS = 600;

	//! Polls spent waiting for IsInitialized() since the start sequence was issued.
	protected int m_iStartPolls;

	//------------------------------------------------------------------------------------------------
	//! Opt-in for the campaign-start Setup step below. Override and return true in a suite whose
	//! cases need a running campaign (the Campaign and Persistence tiers).
	//! Default false: the Smoke, Meta and Init suites are completely unaffected by this class.
	//! \return True if this suite needs the campaign started before its cases run.
	bool RequiresStartedCampaign()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the Overthrow campaign for suites that opted in, exactly the way the start menu does.
	//!
	//! No-op (returns true immediately) when RequiresStartedCampaign() is false, which is every
	//! suite that does not override it.
	//!
	//! Sequence, in order, all of it settled empirically in findings.md 1.3 / 1.3d:
	//!  - pick the difficulty preset BY NAME (see CAMPAIGN_DIFFICULTY_PRESET);
	//!  - DoStartNewGame() then DoStartGame(), guarded by !HasGameStarted() because neither method
	//!    is idempotent or re-entrancy guarded - a second call re-runs every PostGameStart() and
	//!    re-registers repeating timers;
	//!  - CloseLayout() on the start-game context. Not required for the start to succeed, but the
	//!    single-player start menu opens ~4 ms AFTER the suite Setup steps run, so any Setup step
	//!    that polls for more than one frame races it. CloseLayout() is a no-op when the layout is
	//!    not shown, so calling it on every poll is free;
	//!  - IsInitialized() as the completion condition. It is true in the same frame today; the poll
	//!    survives a future async change at no cost.
	//! \return True when the campaign is initialized (or the suite did not opt in), false to poll again.
	[TestStep(TestStage.Setup)]
	bool Setup_StartCampaign()
	{
		if (!RequiresStartedCampaign())
			return true;

		OVT_OverthrowGameMode mode = OVT_Global.GetOverthrow();
		if (!mode)
		{
			SetFailure(SCR_AutotestFailure.Create("Campaign start failed: no OVT_OverthrowGameMode in the loaded world"));
			return true;
		}

		if (!mode.HasGameStarted())
		{
			OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
			if (!config)
			{
				SetFailure(SCR_AutotestFailure.Create("Campaign start failed: no OVT_OverthrowConfigComponent on the game mode"));
				return true;
			}

			OVT_DifficultySettings preset = FindDifficultyPreset(config, CAMPAIGN_DIFFICULTY_PRESET);
			if (!preset)
			{
				SetFailure(SCR_AutotestFailure.Create("Campaign start failed: no difficulty preset named '%1' in m_aDifficultyPresets", CAMPAIGN_DIFFICULTY_PRESET));
				return true;
			}

			config.m_Difficulty = preset;
			PrintFormat("[OVT_TEST] Campaign start: difficulty preset selected by name = '%1'", preset.name);

			mode.DoStartNewGame();
			mode.DoStartGame();

			m_iStartPolls = 0;
		}

		OVT_StartGameContext startContext = mode.GetStartGameContext();
		if (startContext)
			startContext.CloseLayout();

		if (mode.IsInitialized())
		{
			PrintFormat("[OVT_TEST] Campaign start: initialized after %1 poll(s)", m_iStartPolls.ToString());
			return true;
		}

		m_iStartPolls += 1;
		if (m_iStartPolls > MAX_START_POLLS)
		{
			SetFailure(SCR_AutotestFailure.Create("Campaign start failed: IsInitialized() still false after %1 polls (HasGameStarted = %2)",
				m_iStartPolls.ToString(), mode.HasGameStarted().ToString()));
			return true;
		}

		return false; // keep polling
	}

	//------------------------------------------------------------------------------------------------
	//! Finds a difficulty preset by its configured name.
	//! \param[in] config The Overthrow config component holding the preset list.
	//! \param[in] presetName Name to match exactly (OVT_DifficultySettings.name).
	//! \return The matching preset, or null when the list has no entry with that name.
	OVT_DifficultySettings FindDifficultyPreset(OVT_OverthrowConfigComponent config, string presetName)
	{
		if (!config.m_aDifficultyPresets)
			return null;

		foreach (OVT_DifficultySettings preset : config.m_aDifficultyPresets)
		{
			if (preset && preset.name == presetName)
				return preset;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves a manager component from the LIVE game mode entity.
	//!
	//! DEFENSIVE ONLY - this is documentation of intent, not a flake fix.
	//! Every Overthrow manager caches a `static s_Instance` and nothing in Overthrow ever nulls it,
	//! which reads like a classic stale-singleton hazard given the harness loads the world up to
	//! three times per launch. It was measured and it does NOT manifest on 1.7.0.54: the statics are
	//! weak references and the engine nulls them when the component is destroyed, so GetInstance()
	//! re-resolves against the live game mode. OVT_Global agreed with FindComponent in 100% of
	//! samples, including across a suite boundary with a world reload between them (findings.md 1.5).
	//!
	//! Suites may therefore use OVT_Global directly. Use this helper when a case wants to be explicit
	//! that it is reading the live game mode (and it keeps the pattern available should a manager
	//! ever hold a strong ref, which would break the self-healing behaviour above).
	//! No test may be written to DEPEND on the stale case.
	//! \param[in] managerType Component typename to look up on the game mode entity.
	//! \return The component, or null when there is no game mode or no such component.
	Managed ResolveManager(typename managerType)
	{
		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
			return null;

		return gameMode.FindComponent(managerType);
	}
}
