//------------------------------------------------------------------------------------------------
//! The options manager resolves off the game mode, and both built-in definitions exist.
//!
//! Proven able to fail by removing the OVT_OptionsManagerComponent block from
//! Prefabs/GameMode/OVT_OverthrowGameMode.et (the accessor stays null forever) and separately by
//! emptying RegisterDefaults (HasOption then answers false for both built-in ids).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_OptionsManagerResolves : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the game mode's components to post-init.
	static const int MAX_POLLS = 300;

	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OptionsManagerComponent manager = OVT_Global.GetOptions();
		if (!manager)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("OVT_Global.GetOptions() is still null after %1 frames. Prefabs/GameMode/OVT_OverthrowGameMode.et has lost its OVT_OptionsManagerComponent block.", m_iPolls.ToString());
				return true;
			}

			return false;
		}

		if (!manager.HasOption(OVT_OptionsManagerComponent.OPTION_AUTOSAVE_ENABLED))
		{
			SetFailure("RegisterDefaults() never registered '%1'.", OVT_OptionsManagerComponent.OPTION_AUTOSAVE_ENABLED);
			return true;
		}

		if (!manager.HasOption(OVT_OptionsManagerComponent.OPTION_AUTOSAVE_INTERVAL))
		{
			SetFailure("RegisterDefaults() never registered '%1'.", OVT_OptionsManagerComponent.OPTION_AUTOSAVE_INTERVAL);
			return true;
		}

		OVT_OptionDefinition enabledDef = manager.GetDefinition(OVT_OptionsManagerComponent.OPTION_AUTOSAVE_ENABLED);
		if (enabledDef.m_eType != OVT_EOptionType.TOGGLE)
		{
			SetFailure("'%1' registered as the wrong type, expected TOGGLE.", OVT_OptionsManagerComponent.OPTION_AUTOSAVE_ENABLED);
			return true;
		}

		OVT_OptionDefinition intervalDef = manager.GetDefinition(OVT_OptionsManagerComponent.OPTION_AUTOSAVE_INTERVAL);
		if (intervalDef.m_eType != OVT_EOptionType.SLIDER)
		{
			SetFailure("'%1' registered as the wrong type, expected SLIDER.", OVT_OptionsManagerComponent.OPTION_AUTOSAVE_INTERVAL);
			return true;
		}

		PrintFormat("Options seam: the manager resolves with both built-in definitions registered (found after %1 poll(s))", m_iPolls.ToString());
		return true;
	}
}
