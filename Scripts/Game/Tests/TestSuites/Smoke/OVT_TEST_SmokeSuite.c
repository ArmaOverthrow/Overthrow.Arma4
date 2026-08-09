//------------------------------------------------------------------------------------------------
//! Smoke suite - proves the autotest harness itself runs inside the Overthrow mod.
//! Contains no Overthrow assertions; it must pass on any world the harness can load.
//------------------------------------------------------------------------------------------------
class OVT_TEST_SmokeSuite : OVT_TEST_SuiteBase
{
}

//------------------------------------------------------------------------------------------------
//! Verifies the harness executes Setup -> Main (multi-tick) -> TearDown and reports green.
//! Also logs (never asserts) whether Overthrow systems happen to be present in the test world.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_SmokeSuite, timeoutS: 30)]
class OVT_TEST_Smoke_HarnessRuns : SCR_AutotestCaseBase
{
	//! Number of Main-step ticks required before the case reports success.
	static const int REQUIRED_TICKS = 5;

	//! Tick counter. Class instances are not recreated between runs, so this is reset in Setup.
	protected int m_iTickCount;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Setup)]
	void Setup()
	{
		m_iTickCount = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Runs for several ticks to exercise the framework's re-run (bool step) pattern.
	[Step(EStage.Main)]
	bool Execute()
	{
		m_iTickCount += 1;

		if (m_iTickCount < REQUIRED_TICKS)
		{
			PrintFormat("Smoke tick %1 of %2", m_iTickCount.ToString(), REQUIRED_TICKS.ToString());
			return false; // keep running
		}

		// ----------------------------------------------------------------------------------------
		// DELIBERATE LOG-ONLY DIAGNOSTICS - THESE ARE NOT ASSERTIONS.
		// The default autotest world has no Overthrow game mode, so any Overthrow system may
		// legitimately be absent. Failing here would make the smoke test depend on Overthrow,
		// which is exactly what it must not do. Feature #3 (test-coverage) reads this evidence
		// from autotest.log to decide what an Overthrow-capable test world needs to provide.
		// ----------------------------------------------------------------------------------------
		OVT_OverthrowGameMode gameMode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		bool hasGameMode = (gameMode != null);
		PrintFormat("Diagnostic (log only): OVT_OverthrowGameMode present = %1", hasGameMode.ToString());

		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		bool hasTownManager = (townManager != null);
		PrintFormat("Diagnostic (log only): OVT_Global.GetTowns() non-null = %1", hasTownManager.ToString());

		SetResultSuccess();
		return true; // finish the test
	}

	//------------------------------------------------------------------------------------------------
	[Step(EStage.TearDown)]
	void TearDown()
	{
		Print("Smoke test teardown complete");
	}
}
