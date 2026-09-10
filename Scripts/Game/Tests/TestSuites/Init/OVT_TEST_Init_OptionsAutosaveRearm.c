//------------------------------------------------------------------------------------------------
//! An autosave enable, disable and interval change all re-arm OVT_PersistenceManagerComponent's
//! repeating timer through the options registry, with no restart of the campaign.
//!
//! The Init tier world never starts the campaign, so StartAutosaves() has not run on its own. This
//! case calls it directly, then drives the two autosave options through OVT_OptionsManagerComponent
//! .SetOption() and reads IsAutosaveScheduled() after each change. SetOption() is server-authoritative
//! but calls its RpcDo_SetOption handler directly before the broadcast, so the whole chain - normalize,
//! store, invoke, OnOptionChanged, RestartAutosaves - runs synchronously inside one call. Nothing here
//! reads a wall clock or waits on the timer to fire.
//!
//! PROVEN ABLE TO FAIL: with OVT_PersistenceManagerComponent.OnOptionChanged() ignoring the enabled id
//! (an early return with no id check), the case fails at the post-disable assertion because the flag
//! stays true. Restored, it passes.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_OptionsAutosaveRearm : SCR_AutotestCaseBase
{
	//! Frame polls allowed for both managers to resolve off the game mode.
	static const int MAX_POLLS = 300;

	protected int m_iPolls;

	//! The values in effect before this case started, so it leaves the registry as it found it.
	protected string m_sStartEnabled;
	protected string m_sStartInterval;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OverthrowGameMode mode = OVT_Global.GetOverthrow();
		OVT_PersistenceManagerComponent persistence = null;
		if (mode)
			persistence = mode.GetPersistence();

		OVT_OptionsManagerComponent options = OVT_Global.GetOptions();

		if (!persistence || !options)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				string persistenceState = "resolved";
				if (!persistence)
					persistenceState = "null";

				string optionsState = "resolved";
				if (!options)
					optionsState = "null";

				SetFailure("Still unresolved after %1 frames: persistence manager %2, options manager %3. The autosave re-arm chain needs both.", m_iPolls.ToString(), persistenceState, optionsState);
				return true;
			}

			return false;
		}

		m_sStartEnabled = options.GetValue(OVT_OptionsManagerComponent.OPTION_AUTOSAVE_ENABLED);
		m_sStartInterval = options.GetValue(OVT_OptionsManagerComponent.OPTION_AUTOSAVE_INTERVAL);

		persistence.StartAutosaves();

		if (!persistence.IsAutosaveScheduled())
		{
			SetFailure("StartAutosaves() left IsAutosaveScheduled() false. Either the campaign default is disabled on this world, or the scheduling guard is broken.");
			return true;
		}

		options.SetOption(OVT_OptionsManagerComponent.OPTION_AUTOSAVE_ENABLED, "0");

		if (persistence.IsAutosaveScheduled())
		{
			SetFailure("IsAutosaveScheduled() is still true after autosave.enabled was set to 0. OnOptionChanged() did not re-arm the timer, or it ignored the enabled id.");
			Restore(persistence, options);
			return true;
		}

		options.SetOption(OVT_OptionsManagerComponent.OPTION_AUTOSAVE_ENABLED, "1");

		if (!persistence.IsAutosaveScheduled())
		{
			SetFailure("IsAutosaveScheduled() is still false after autosave.enabled was set back to 1. An enable did not re-arm the timer.");
			Restore(persistence, options);
			return true;
		}

		options.SetOption(OVT_OptionsManagerComponent.OPTION_AUTOSAVE_INTERVAL, "300");

		if (!persistence.IsAutosaveScheduled())
		{
			SetFailure("IsAutosaveScheduled() went false after an interval change alone. RestartAutosaves() must remove the old timer and add a new one in the same call, not just remove it.");
			Restore(persistence, options);
			return true;
		}

		Restore(persistence, options);

		PrintFormat("Options autosave re-arm: enable, disable, enable and interval change all resolved after %1 poll(s)", m_iPolls.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts both options back where this case found them, then stops the timer this case started.
	//! \param[in] persistence The persistence manager under test.
	//! \param[in] options The options manager under test.
	protected void Restore(OVT_PersistenceManagerComponent persistence, OVT_OptionsManagerComponent options)
	{
		options.SetOption(OVT_OptionsManagerComponent.OPTION_AUTOSAVE_ENABLED, m_sStartEnabled);
		options.SetOption(OVT_OptionsManagerComponent.OPTION_AUTOSAVE_INTERVAL, m_sStartInterval);
		persistence.StopAutosaves();
	}
}
