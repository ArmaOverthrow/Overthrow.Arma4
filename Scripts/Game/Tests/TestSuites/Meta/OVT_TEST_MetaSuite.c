//------------------------------------------------------------------------------------------------
//! META SUITE - THIS SUITE IS NEVER PART OF A DEFAULT OR CI RUN.
//!
//! IT EXISTS SOLELY TO VERIFY THAT THE RED/FAILURE PATH OF THE AUTOTEST HARNESS STAYS
//! OBSERVABLE. IT CONTAINS A TEST THAT ALWAYS FAILS ON PURPOSE. RUN IT MANUALLY ONLY,
//! AND NEVER ADD IT TO ANY DEFAULT OR AUTOMATED TEST RUN - IT WILL ALWAYS REPORT RED.
//------------------------------------------------------------------------------------------------
class OVT_TEST_MetaSuite : OVT_TEST_SuiteBase
{
}

//------------------------------------------------------------------------------------------------
//! ALWAYS FAILS BY DESIGN. Used to confirm failures are surfaced by the runner and the report.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_MetaSuite)]
class OVT_TEST_Meta_AlwaysFails : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		SetFailure("Deliberate failure: proves the harness reports red");
		return true;
	}
}
