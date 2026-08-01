//------------------------------------------------------------------------------------------------
//! Base class for every Overthrow autotest suite.
//!
//! This is deliberately near-empty and is the single inheritance point for all Overthrow suites.
//! World selection is NOT done here - it comes from OVT_AutotestFramework.c via the modded
//! SCR_AutotestHelper statics, which the inherited GetWorldFile()/GetWorldSystemsConfigFile()
//! already route to. Overriding those here would defeat the single integration point.
//!
//! Shared setup/teardown for future Overthrow suites (feature #3, test-coverage) belongs here.
//------------------------------------------------------------------------------------------------
class OVT_TEST_SuiteBase : SCR_AutotestSuiteBase
{
}
