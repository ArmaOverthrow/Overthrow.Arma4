//------------------------------------------------------------------------------------------------
//! OVT_FOBRequestComponent is actually ON the local player's OVT_OverthrowController.
//!
//! WHY THIS CASE EXISTS, AND WHY IT IS SEPARATE FROM THE RESISTANCE ONE. Phase 5 created TWO components
//! and deleted the seven FOB/camp handlers from the legacy comms monolith in the same change. The two are
//! wired by two independent prefab blocks, so one can be present while the other is missing - and a
//! single case asserting "the resistance seam resolves" would pass happily while deploy, undeploy,
//! set-priority, both garrison purchases, camp privacy and delete-camp were all silently dead. Each
//! block therefore gets its own assertion (plan D11).
//!
//! TWO CLAIMS, ONE PRECONDITION: it resolves at all, and what resolves is THIS player's own instance -
//! not merely a component of that type from somewhere, which would mean the server attributed every FOB
//! request to a different player.
//!
//! THE POLL IS A PRECONDITION, NOT A RETRY (no maxAttempts anywhere in this project). The controller is
//! spawned by OVT_PlayerManagerComponent.SetupPlayer() when the player enters the world, which is not
//! instantaneous at world load; expiry is a named failure carrying its own diagnosis.
//!
//! PROVEN ABLE TO FAIL (2026-08-14): the OVT_FOBRequestComponent block was removed from
//! Prefabs/GameMode/OVT_OverthrowController.et - the literal defect this case is built to catch - and
//! the run failed with "OVT_ControllerComponent<OVT_FOBRequestComponent>.Get() returned null while a
//! controller entity exists"; block restored, it passes.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Controller_FOBRequestResolves : SCR_AutotestCaseBase
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
				SetFailure("OVT_Global.GetController() was still null after %1 polls (local player id %2). Nothing on the controller seam is reachable from this machine, so this case cannot say anything about OVT_FOBRequestComponent either way.",
					m_iPolls.ToString(),
					SCR_PlayerController.GetLocalPlayerId().ToString());
				return true;
			}

			return false;
		}

		// Claim 1 - it is wired.
		OVT_FOBRequestComponent viaAccessor = OVT_ControllerComponent<OVT_FOBRequestComponent>.Get();
		if (!viaAccessor)
		{
			SetFailure("OVT_ControllerComponent<OVT_FOBRequestComponent>.Get() returned null while a controller entity exists. Prefabs/GameMode/OVT_OverthrowController.et is not carrying the component, so deploy, undeploy, set-priority, camp/FOB garrison purchases, camp privacy and delete-camp all silently never happen - the legacy comms-monolith handlers they used to ride were deleted in the same phase.");
			return true;
		}

		// Claim 2 - it is OUR controller's instance, not somebody else's.
		OVT_FOBRequestComponent onEntity = OVT_FOBRequestComponent.Cast(controller.FindComponent(OVT_FOBRequestComponent));
		if (viaAccessor != onEntity)
		{
			SetFailure("OVT_ControllerComponent<OVT_FOBRequestComponent>.Get() did not return the instance on the local player's own controller entity. Every FOB and camp request would then be sent through another player's seam, and the server would resolve the caller - and the camp ownership test - as that player.");
			return true;
		}

		PrintFormat("Controller seam: OVT_FOBRequestComponent resolves off the local controller (found after %1 poll(s))", m_iPolls.ToString());
		return true;
	}
}
