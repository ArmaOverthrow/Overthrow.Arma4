//------------------------------------------------------------------------------------------------
//! TEMPORARY DIAGNOSTIC PROBE (2026-08-02) - QUARANTINED, IN NO GROUP CONFIG. DELETE WHEN THE
//! start-menu session-exit investigation closes.
//!
//! Reproduces the user's Workbench play state headlessly: the test world loaded, the campaign NOT
//! started, the start menu pending - then simply idles for ~60 seconds. The user's Workbench play
//! session dies ~10 s in (silent exit back to the editor, no log line). If this probe survives its
//! full idle, the trigger is Workbench-play-specific (e.g. WORKBENCH-define code paths); if the
//! session dies mid-idle, the fault is in Overthrow game code and can be bisected headlessly.
//------------------------------------------------------------------------------------------------
class OVT_TEST_IdleProbeSuite : OVT_TEST_SuiteBase
{
	//------------------------------------------------------------------------------------------------
	//! The user's broken state is the PRE-campaign start menu - never start the campaign here.
	override bool RequiresStartedCampaign()
	{
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Idles ~60 s in the pre-start-menu state, logging a heartbeat every ~5 s.
[Test(suite: OVT_TEST_IdleProbeSuite, timeoutS: 90)]
class OVT_TEST_IdleProbe_SessionSurvivesStartMenu : SCR_AutotestCaseBase
{
	//! ~60 s at 60 fps.
	static const int IDLE_FRAMES = 3600;

	//! Heartbeat interval in frames.
	static const int HEARTBEAT_FRAMES = 300;

	protected int m_iFrames;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Setup)]
	void Setup()
	{
		m_iFrames = 0;
		OVT_IdleProbeLoadingWatch.Subscribe();
	}

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		m_iFrames += 1;

		if (m_iFrames % HEARTBEAT_FRAMES == 0)
		{
			OVT_OverthrowGameMode mode = OVT_Global.GetOverthrow();
			bool hasMode = (mode != null);
			bool started = false;
			if (mode)
				started = mode.HasGameStarted();
			string beat = string.Format("[IdleProbe] frame %1: game mode present = %2, campaign started = %3, loading exit seen = %4",
				m_iFrames.ToString(), hasMode.ToString(), started.ToString(), OVT_IdleProbeLoadingWatch.s_bLoadingExitSeen.ToString());
			Print(beat);
		}

		if (m_iFrames < IDLE_FRAMES)
			return false;

		// Surviving the idle IS the result - the session did not kill itself.
		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Standalone watcher (case classes inherit member Print/PrintFormat that shadow the globals).
class OVT_IdleProbeLoadingWatch
{
	static bool s_bLoadingExitSeen;
	protected static bool s_bSubscribed;

	//------------------------------------------------------------------------------------------------
	static void Subscribe()
	{
		if (s_bSubscribed)
			return;
		s_bSubscribed = true;
		ArmaReforgerLoadingAnim.m_onExitLoadingScreen.Insert(OnLoadingScreenExit);
	}

	//------------------------------------------------------------------------------------------------
	protected static void OnLoadingScreenExit()
	{
		s_bLoadingExitSeen = true;
		Print("[IdleProbe] LOADING SCREEN EXIT EVENT FIRED - engine considers loading finished");
	}
}

//------------------------------------------------------------------------------------------------
//! Same probe with a STARTED campaign: does the engine finish loading once the campaign runs?
class OVT_TEST_IdleProbeStartedSuite : OVT_TEST_SuiteBase
{
}

//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_IdleProbeStartedSuite, timeoutS: 90)]
class OVT_TEST_IdleProbeStarted_LoadingCompletesWithCampaign : SCR_AutotestCaseBase
{
	//! ~20 s at 60 fps - long enough for any post-start loading settle.
	static const int IDLE_FRAMES = 1200;

	static const int HEARTBEAT_FRAMES = 300;

	protected int m_iFrames;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Setup)]
	void Setup()
	{
		m_iFrames = 0;
		OVT_IdleProbeLoadingWatch.Subscribe();
	}

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		m_iFrames += 1;

		if (m_iFrames % HEARTBEAT_FRAMES == 0)
		{
			OVT_OverthrowGameMode mode = OVT_Global.GetOverthrow();
			bool started = false;
			if (mode)
				started = mode.HasGameStarted();
			string beat = string.Format("[IdleProbeStarted] frame %1: campaign started = %2, loading exit seen = %3",
				m_iFrames.ToString(), started.ToString(), OVT_IdleProbeLoadingWatch.s_bLoadingExitSeen.ToString());
			Print(beat);
		}

		if (m_iFrames < IDLE_FRAMES)
			return false;

		SetResultSuccess();
		return true;
	}
}
