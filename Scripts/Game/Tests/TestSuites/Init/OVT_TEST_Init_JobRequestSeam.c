//------------------------------------------------------------------------------------------------
//! OVT_JobRequestComponent is actually ON the local player's OVT_OverthrowController.
//!
//! WHY THIS CASE EXISTS. Phase 8 of the controller migration deleted the two job handlers from
//! the legacy comms monolith in the same change that created this component - and emptied that monolith
//! completely. There is no old path left to fall back to, so a "written, compiled, never wired to the
//! prefab" mistake means accepting and declining a job silently stop working on every client: no compile
//! error, no runtime error and no log line, because the call sites in OVT_JobManagerComponent null-guard
//! the accessor by contract and simply return. The migration plan therefore requires one Init assertion
//! per new component, added in the phase that creates it (D11), rather than trusting a prefab edit an
//! agent cannot make in Workbench.
//!
//! TWO CLAIMS, ONE PRECONDITION:
//!   1. the component resolves through the epic-level accessor at all;
//!   2. what comes back is the instance carried by THIS player's own controller entity - not merely "a
//!      component of that type from somewhere", which a Get() that searched the wrong entity would also
//!      satisfy. Here that would be an ownership hole rather than a mis-route: the server writes
//!      job.owner from the caller it resolves off the controller the request arrived on, so a request
//!      sent through another player's seam would assign THEM the job and its reward.
//!
//! THE POLL IS A PRECONDITION, NOT A RETRY (no maxAttempts anywhere in this project).
//! OVT_PlayerManagerComponent.SetupPlayer() spawns the controller when the player enters the world, which
//! is not instantaneous at world load. Expiry is itself a named failure carrying the diagnosis, so nothing
//! here can pass by being asked at a luckier moment.
//!
//! PROVEN ABLE TO FAIL (2026-08-14): the OVT_JobRequestComponent block was removed from
//! Prefabs/GameMode/OVT_OverthrowController.et - the literal defect this case is built to catch - and the
//! run failed with "OVT_ControllerComponent<OVT_JobRequestComponent>.Get() returned null while a
//! controller entity exists"; block restored, it passes.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Controller_JobRequestResolves : SCR_AutotestCaseBase
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
				SetFailure("OVT_Global.GetController() was still null after %1 polls (local player id %2). Nothing on the controller seam is reachable from this machine, so this case cannot say anything about OVT_JobRequestComponent either way.",
					m_iPolls.ToString(),
					SCR_PlayerController.GetLocalPlayerId().ToString());
				return true;
			}

			return false;
		}

		// Claim 1 - it is wired.
		OVT_JobRequestComponent viaAccessor = OVT_ControllerComponent<OVT_JobRequestComponent>.Get();
		if (!viaAccessor)
		{
			SetFailure("OVT_ControllerComponent<OVT_JobRequestComponent>.Get() returned null while a controller entity exists. Prefabs/GameMode/OVT_OverthrowController.et is not carrying the component, so accepting and declining a job silently never reach the server - the legacy comms-monolith handlers they used to ride were deleted in the same phase.");
			return true;
		}

		// Claim 2 - it is OUR controller's instance, not somebody else's.
		OVT_JobRequestComponent onEntity = OVT_JobRequestComponent.Cast(controller.FindComponent(OVT_JobRequestComponent));
		if (viaAccessor != onEntity)
		{
			SetFailure("OVT_ControllerComponent<OVT_JobRequestComponent>.Get() did not return the instance on the local player's own controller entity. Every job request would then be sent through another player's seam, and the server would resolve the caller - and therefore the job's owner - as that player.");
			return true;
		}

		PrintFormat("Controller seam: OVT_JobRequestComponent resolves off the local controller (found after %1 poll(s))", m_iPolls.ToString());
		return true;
	}
}
