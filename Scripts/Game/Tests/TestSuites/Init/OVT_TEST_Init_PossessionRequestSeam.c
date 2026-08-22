//------------------------------------------------------------------------------------------------
//! OVT_PossessionRequestComponent is actually ON the local player's OVT_OverthrowController.
//!
//! WHY THIS CASE EXISTS. Phase 7 of the controller migration deleted the possession handler, its client
//! response and the whole BUG-147 close/restore lifecycle from the legacy comms monolith in the same
//! change that created this component. There is no longer an old path to fall back to, so a "written,
//! compiled, never wired to the prefab" mistake means the commanding menu's "Open Inventory" command
//! silently does nothing - no compile error, no runtime error and no log line, because
//! OVT_OpenInventoryCommand null-guards the accessor by contract and returns false.
//!
//! THIS ONE IS ALSO THE JOIN BETWEEN A REQUEST AND A STATEFUL CLIENT LIFECYCLE. The close subscription
//! and the 300 ms deferred RequestRestorePossession() live on this component, so an unwired component
//! would not merely drop the request: there would be nothing left holding the recruit's inventory
//! manager, and possession - if it were ever granted by some other path - would never be handed back.
//!
//! TWO CLAIMS, ONE PRECONDITION:
//!   1. the component resolves through the epic-level accessor at all;
//!   2. what comes back is the instance carried by THIS player's own controller entity - not merely "a
//!      component of that type from somewhere", which a Get() that searched the wrong entity would also
//!      satisfy, and which here would be an ownership hole: the server resolves who is asking to POSSESS
//!      a character from the controller the request arrived on, so a request sent through another
//!      player's seam would be evaluated - and granted - against that player's recruits.
//!
//! THE POLL IS A PRECONDITION, NOT A RETRY (no maxAttempts anywhere in this project).
//! OVT_PlayerManagerComponent.SetupPlayer() spawns the controller when the player enters the world, which
//! is not instantaneous at world load. Expiry is itself a named failure carrying the diagnosis, so nothing
//! here can pass by being asked at a luckier moment.
//!
//! WHAT IT DELIBERATELY DOES NOT ASSERT: that possession works. Possessing a character needs a granted
//! controller, an owned recruit and a live inventory menu, and the restore path is a 300 ms deferral on
//! top of that - none of which the Init world has. That is §6/F11's manual probe (open and close a
//! recruit's inventory three times, expect exactly three restores and normal facing afterwards).
//!
//! PROVEN ABLE TO FAIL (2026-08-14): the OVT_PossessionRequestComponent block was removed from
//! Prefabs/GameMode/OVT_OverthrowController.et - the literal defect this case is built to catch - and the
//! run failed with "OVT_ControllerComponent<OVT_PossessionRequestComponent>.Get() returned null while a
//! controller entity exists"; block restored, it passes.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Controller_PossessionRequestResolves : SCR_AutotestCaseBase
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
				SetFailure("OVT_Global.GetController() was still null after %1 polls (local player id %2). Nothing on the controller seam is reachable from this machine, so this case cannot say anything about OVT_PossessionRequestComponent either way.",
					m_iPolls.ToString(),
					SCR_PlayerController.GetLocalPlayerId().ToString());
				return true;
			}

			return false;
		}

		// Claim 1 - it is wired.
		OVT_PossessionRequestComponent viaAccessor = OVT_ControllerComponent<OVT_PossessionRequestComponent>.Get();
		if (!viaAccessor)
		{
			SetFailure("OVT_ControllerComponent<OVT_PossessionRequestComponent>.Get() returned null while a controller entity exists. Prefabs/GameMode/OVT_OverthrowController.et is not carrying the component, so the commanding menu's Open Inventory command on a recruit silently does nothing - the legacy comms-monolith handler and the BUG-147 close/restore lifecycle it used to ride were deleted in the same phase.");
			return true;
		}

		// Claim 2 - it is OUR controller's instance, not somebody else's.
		OVT_PossessionRequestComponent onEntity = OVT_PossessionRequestComponent.Cast(controller.FindComponent(OVT_PossessionRequestComponent));
		if (viaAccessor != onEntity)
		{
			SetFailure("OVT_ControllerComponent<OVT_PossessionRequestComponent>.Get() did not return the instance on the local player's own controller entity. The possess request would then be sent through another player's seam and the server would evaluate ownership - and grant control of a character - against that player instead.");
			return true;
		}

		PrintFormat("Controller seam: OVT_PossessionRequestComponent resolves off the local controller (found after %1 poll(s))", m_iPolls.ToString());
		return true;
	}
}
