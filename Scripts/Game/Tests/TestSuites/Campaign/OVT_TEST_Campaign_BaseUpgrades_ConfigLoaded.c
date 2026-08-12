//------------------------------------------------------------------------------------------------
//! The base controller's upgrade list arrives from its CONFIG, not the prefab (2026-08-13 move).
//!
//! WHAT IT MEASURES: OVT_BaseController.et no longer carries an inline m_aBaseUpgrades array - it
//! points m_BaseUpgradesConfig at Configs/BaseUpgrades/overthrowBaseUpgrades.conf, and
//! InitializeBase() copies that config's entries into the runtime m_aBaseUpgrades array the
//! scheduler, serializer and FindUpgrade() all iterate. Two failure modes are asserted apart:
//!  1. m_BaseUpgradesConfig is null -> the prefab->conf reference did not resolve (broken GUID,
//!     missing .meta, deleted conf). This is the wiring this case exists to guard.
//!  2. m_aBaseUpgrades is null/empty after base init -> the reference resolved but InitializeBase()
//!     never ran or never populated the runtime list.
//!
//! Base controllers are initialized by InitBaseControllers via CallLater(0) out of PostGameStart -
//! first frame after the campaign start (suite header settling budgets) - so this polls with the
//! suite's standard bounded backstop instead of asserting on its own first tick.
//!
//! No magic counts (house rule): the conf ships 10 entries today, but the assertion is >= 1 so
//! authoring changes to the default loadout don't fail a wiring test.
//!
//! PROVEN ABLE TO FAIL: with the prefab's `: "{0756DED5D4018095}Configs/BaseUpgrades/
//! overthrowBaseUpgrades.conf"` reference removed from m_BaseUpgradesConfig (empty object instead),
//! this case goes red on the "m_BaseUpgradesConfig is null" assertion (method: temporary prefab
//! edit, run, revert - 2026-08-13).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 30)]
class OVT_TEST_Campaign_BaseUpgrades_ConfigLoaded : SCR_AutotestCaseBase
{
	//! Diagnostic backstop. Base controllers init on the first frame after the campaign start;
	//! this is a bound on the explanation, not a retry budget.
	static const int MAX_POLLS = 600;

	//! Polls spent waiting for the base controller to populate its upgrade list.
	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
		{
			SetResultFailure("OVT_Global.GetOccupyingFaction() is null");
			return true;
		}

		if (occupying.m_Bases.Count() < 1)
		{
			SetResultFailure("No bases registered: m_Bases.Count() = %1", occupying.m_Bases.Count().ToString());
			return true;
		}

		OVT_BaseControllerComponent baseController = occupying.GetBaseByIndex(0);
		if (!baseController)
		{
			SetResultFailure("Base 0 is registered but GetBaseByIndex(0) resolved no OVT_BaseControllerComponent");
			return true;
		}

		// Wiring: the prefab's reference to overthrowBaseUpgrades.conf resolved into an instance.
		// This is attribute deserialization, not init timing - null here never heals, so fail now.
		if (!baseController.m_BaseUpgradesConfig)
		{
			SetResultFailure("m_BaseUpgradesConfig is null - the prefab's reference to Configs/BaseUpgrades/overthrowBaseUpgrades.conf did not resolve");
			return true;
		}

		// Runtime list: InitializeBase() copied the config entries. First frame after start, so poll.
		if (baseController.m_aBaseUpgrades && baseController.m_aBaseUpgrades.Count() >= 1)
		{
			PrintFormat("Base upgrades populated from config: %1 upgrade(s) after %2 poll(s)",
				baseController.m_aBaseUpgrades.Count().ToString(), m_iPolls.ToString());
			SetResultSuccess();
			return true;
		}

		m_iPolls += 1;
		if (m_iPolls > MAX_POLLS)
		{
			string listState = "null";
			if (baseController.m_aBaseUpgrades)
				listState = baseController.m_aBaseUpgrades.Count().ToString() + " entries";
			SetResultFailure("m_aBaseUpgrades never populated within %1 polls (config resolved with %2 entries, runtime list: %3) - InitializeBase() did not copy the config",
				m_iPolls.ToString(), baseController.m_BaseUpgradesConfig.m_aBaseUpgrades.Count().ToString(), listState);
			return true;
		}

		return false; // keep polling
	}
}
