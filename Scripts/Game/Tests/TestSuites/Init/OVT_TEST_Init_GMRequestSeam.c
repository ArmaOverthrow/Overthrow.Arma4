//------------------------------------------------------------------------------------------------
//! OVT_GMRequestComponent is actually ON the local player's OVT_OverthrowController.
//!
//! WHY THIS CASE EXISTS. This component is the ONLY seam through which a Game Master receives
//! campaign state - threat, both occupying-faction resource pools, the two schedule countdowns - and
//! every consumer of it (the Overthrow panel, HUD icons, waypoint visualisation, the GM map layers)
//! reaches it through OVT_ControllerComponent<OVT_GMRequestComponent>.Get() and null-guards the
//! result by contract. A missing prefab block therefore produces NO compile error, NO runtime error
//! and NO log line: the editor hook is never installed, no request is ever sent, and every GM surface
//! in the epic is simply blank forever. That is the exact defect this case is built to catch, and it
//! is a defect an agent cannot see because the .et edit is not compiled.
//!
//! TWO CLAIMS, ONE PRECONDITION:
//!   1. the component resolves through the epic-level accessor at all;
//!   2. what comes back is the instance carried by THIS player's own controller entity - not merely
//!      "a component of that type from somewhere", which a Get() that searched the wrong entity would
//!      also satisfy, and which for THIS component would mean the client polled through another
//!      player's seam: the server would resolve the caller as that player and gate the request on
//!      THEIR Game Master role.
//!
//! THE POLL IS A PRECONDITION, NOT A RETRY (no maxAttempts anywhere in this project).
//! OVT_PlayerManagerComponent.SetupPlayer() spawns the controller when the player enters the world,
//! which is not instantaneous at world load. Expiry is itself a named failure carrying the diagnosis,
//! so nothing here can pass by being asked at a luckier moment.
//!
//! HOW TO PROVE IT CAN FAIL, and the state of that proof. The inversion is to delete the
//! OVT_GMRequestComponent block from Prefabs/GameMode/OVT_OverthrowController.et and re-run: claim 1
//! then fails with "OVT_ControllerComponent<OVT_GMRequestComponent>.Get() returned null while a
//! controller entity exists". That is the literal defect this case exists for, and it is the same
//! inversion already executed for the sibling case OVT_TEST_Init_Controller_EconomyRequestResolves
//! (2026-08-14). It has NOT yet been executed for this case: the implementing agent's gate stops at
//! tools/compile-check.sh and the suites are run by the orchestrator afterwards, so record the
//! inversion result in context.md at that point rather than assuming it here.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Controller_GMRequestResolves : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the local player's controller to be spawned and registered.
	static const int MAX_POLLS = 300;

	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OverthrowController controller = OVT_Global.GetController();
		if (!controller)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("OVT_Global.GetController() was still null after %1 polls (local player id %2). Nothing on the controller seam is reachable from this machine, so this case cannot say anything about OVT_GMRequestComponent either way.",
					m_iPolls.ToString(),
					SCR_PlayerController.GetLocalPlayerId().ToString());
				return true;
			}

			return false;
		}

		// Claim 1 - it is wired.
		OVT_GMRequestComponent viaAccessor = OVT_ControllerComponent<OVT_GMRequestComponent>.Get();
		if (!viaAccessor)
		{
			SetFailure("OVT_ControllerComponent<OVT_GMRequestComponent>.Get() returned null while a controller entity exists. Prefabs/GameMode/OVT_OverthrowController.et is not carrying the component, so opening Game Master installs no editor hook, sends no snapshot request and populates no campaign state - every GM surface in the epic stays blank with nothing in any log.");
			return true;
		}

		// Claim 2 - it is OUR controller's instance, not somebody else's.
		OVT_GMRequestComponent onEntity = OVT_GMRequestComponent.Cast(controller.FindComponent(OVT_GMRequestComponent));
		if (viaAccessor != onEntity)
		{
			SetFailure("OVT_ControllerComponent<OVT_GMRequestComponent>.Get() did not return the instance on the local player's own controller entity. Snapshot requests would then be sent through another player's seam, and the server would resolve the caller - and gate the Game Master check - on that other player.");
			return true;
		}

		PrintFormat("Controller seam: OVT_GMRequestComponent resolves off the local controller (found after %1 poll(s))", m_iPolls.ToString());
		return true;
	}
}
