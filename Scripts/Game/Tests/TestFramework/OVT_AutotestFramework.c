//------------------------------------------------------------------------------------------------
//! Overthrow's integration with the shipped SCR_Autotest framework.
//!
//! This file is the ENTIRE integration point between Overthrow and the autotest framework.
//! The framework calls into SCR_AutotestHelper for every game specific decision (which world to
//! load, which systems config to use, how to transition into that world, and which launch
//! parameters to use when the Workbench spawns a test run). Modding those statics here is all
//! that is required to make every Overthrow test suite run in an Overthrow world with the
//! Overthrow addon loaded.
//!
//! Do NOT scatter world/addon selection across individual suites - change it here instead.
//------------------------------------------------------------------------------------------------
modded class SCR_AutotestHelper
{
	//! Dedicated lightweight world used as the default host for Overthrow tests.
	static const ResourceName WORLD_OVT_TEST = "{D87EF7EED4210569}Worlds/MP/OVT_Campaign_Test.ent";

	//! Addons loaded for every test run: base game (Arma Reforger) followed by Overthrow.
	//! Comma separated bare GUIDs, no braces - this is the format GameStateTransitions expects.
	static const string ADDONS_OVT = "58D0FB3206B6F859,59B657D731E2A11D";

	//------------------------------------------------------------------------------------------------
	//! Game specific default world for testing.
	override static ResourceName GetDefaultWorld()
	{
		return WORLD_OVT_TEST;
	}

	//------------------------------------------------------------------------------------------------
	//! Game specific default systems config for testing.
	//! Keeps whatever is already loaded, matching the base game behaviour.
	override static ResourceName GetDefaultSystemsConfig()
	{
		return GetGame().GetSystemsConfig();
	}

	//------------------------------------------------------------------------------------------------
	//! Game specific launch arguments, used when launching tests in new window from WB.
	override static string GetDefaultLaunchParams()
	{
		return "-profile OverthrowCI\n-logLevel debug\n-noFocus\n-noThrow\n-window\n";
	}

	//------------------------------------------------------------------------------------------------
	//! Game specific way to transition to world for testing.
	override static bool RequestScenarioChangeTransition(ResourceName mapResource, ResourceName worldSystemsConfigResourceName)
	{
		string mapPath = mapResource.GetPath();
		return GameStateTransitions.RequestScenarioChangeTransition(
			mapPath,
			worldSystemsConfigResourceName,
			addonList: ADDONS_OVT
		);
	}
}
