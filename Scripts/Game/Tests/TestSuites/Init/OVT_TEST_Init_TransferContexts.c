//------------------------------------------------------------------------------------------------
//! Both transfer consumers are registered, are OVT_TransferContext subclasses, and share ONE layout
//! and ONE ActionContext name. Nothing in the toolchain enforces any of that.
//!
//! ActivateContext's return for an unknown name could not be settled statically, so claim 4 runs a
//! nonsense-name negative control first and SKIPS itself rather than asserting something vacuous.
//! The poll is a precondition with a named failure on expiry, not a retry.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_TransferContexts_ShareOneScreen : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the local player's UI manager to exist and carry its contexts.
	static const int MAX_POLLS = 300;

	//! A context name no conf declares. The negative control for the ActivateContext claim.
	static const string UNKNOWN_CONTEXT = "OverthrowNoSuchContextExistsAnywhere";

	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_UIManagerComponent ui = OVT_Global.GetUI();
		if (!ui)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("OVT_Global.GetUI() was still null after %1 polls (local player id %2, local controlled entity %3). No UI context on this machine is reachable: either the player never entered the world, or Character_Player.et lost its OVT_UIManagerComponent.",
					m_iPolls.ToString(),
					SCR_PlayerController.GetLocalPlayerId().ToString(),
					BoolWord(SCR_PlayerController.GetLocalControlledEntity() != null));
				return true;
			}

			return false;
		}

		// Claim 1 - both consumers are registered on the player prefab.
		OVT_UIContext port = ui.GetContext(OVT_PortContext);
		if (!port)
		{
			SetFailure("OVT_UIManagerComponent.GetContext(OVT_PortContext) returned null. The context is not in m_aContexts on Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et, so the port Import screen can never open - no compile error, no runtime error, no log line.");
			return true;
		}

		OVT_UIContext warehouse = ui.GetContext(OVT_WarehouseContext);
		if (!warehouse)
		{
			SetFailure("OVT_UIManagerComponent.GetContext(OVT_WarehouseContext) returned null. The context is not in m_aContexts on Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et, so the warehouse Take screen can never open.");
			return true;
		}

		// Claim 2 - both are the shared base, not a fork of it.
		if (!OVT_TransferContext.Cast(port))
		{
			SetFailure("OVT_PortContext is not an OVT_TransferContext subclass (it is a %1). The port screen has left the shared transfer base, so the hooks, the cart and the focus model no longer apply to it.",
				port.ClassName());
			return true;
		}

		if (!OVT_TransferContext.Cast(warehouse))
		{
			SetFailure("OVT_WarehouseContext is not an OVT_TransferContext subclass (it is a %1). The warehouse screen has left the shared transfer base.",
				warehouse.ClassName());
			return true;
		}

		// Claim 3 - ONE layout and ONE ActionContext name. This is the requirement, mechanically pinned.
		if (port.m_Layout != warehouse.m_Layout)
		{
			SetFailure("The two transfer consumers point at DIFFERENT layouts: port '%1', warehouse '%2'. logistics/ui exists to make them one screen; a second layout means a fix to one stops reaching the other.",
				port.m_Layout,
				warehouse.m_Layout);
			return true;
		}

		if (port.m_sContextName != warehouse.m_sContextName)
		{
			SetFailure("The two transfer consumers declare DIFFERENT ActionContext names: port '%1', warehouse '%2'. One shared ActionContext is what keeps their bindings, and the conflict checker's view of them, identical.",
				port.m_sContextName,
				warehouse.m_sContextName);
			return true;
		}

		// Claim 4 - the shared ActionContext is one the input manager knows. Asserted only when the
		// negative control proves the answer means something.
		if (!AssertSharedContextActivates(port.m_sContextName))
			return true;

		PrintFormat("Transfer contexts: OVT_PortContext and OVT_WarehouseContext both resolve, both subclass OVT_TransferContext, and both use layout '%1' / context '%2' (UI manager found after %3 poll(s))",
			port.m_Layout,
			port.m_sContextName,
			m_iPolls.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Activates a nonsense context name first: if the engine says true for that too, the positive
	//! claim proves nothing and is skipped instead of asserted.
	//! \param[in] contextName The shared ActionContext name both consumers declare.
	//! \return False when the case has been failed.
	protected bool AssertSharedContextActivates(string contextName)
	{
		InputManager input = GetGame().GetInputManager();
		if (!input)
		{
			SetFailure("GetGame().GetInputManager() returned null, so no menu on this machine can receive an input action at all.");
			return false;
		}

		if (input.ActivateContext(UNKNOWN_CONTEXT))
		{
			PrintFormat("Transfer contexts: ActivateContext('%1') returned true for a name no conf declares, so ActivateContext('%2') would prove nothing - claim skipped, not asserted.",
				UNKNOWN_CONTEXT,
				contextName);
			return true;
		}

		if (!input.ActivateContext(contextName))
		{
			SetFailure("InputManager.ActivateContext('%1') returned false while the same call for an undeclared name also returns false. Configs/System/chimeraInputCommon.conf has no such ActionContext, so every binding on the transfer screen - including MenuUp/Down/Left/Right - is dead and a gamepad cannot move inside it.",
				contextName);
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Renders a bool for a failure message without a ternary (EnforceScript has none).
	//! \param[in] value The value to describe.
	//! \return "yes" or "no".
	protected string BoolWord(bool value)
	{
		if (value)
			return "yes";

		return "no";
	}
}
