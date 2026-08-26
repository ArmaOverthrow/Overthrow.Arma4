//------------------------------------------------------------------------------------------------
//! OVT_VehicleRequestComponent is actually ON the local player's OVT_OverthrowController.
//!
//! WHY THIS CASE EXISTS, AND WHY IT IS SEPARATE FROM OVT_TEST_Init_Controller_ComponentsResolve.
//! Phase 2 of the controller migration deleted six handlers from the legacy comms monolith in the same
//! change that created this component. That makes the "written, compiled, never wired to the prefab"
//! failure worse than usual: there is no longer an old path to fall back to, so a missing prefab block
//! means locking a car, claiming a car, upgrading, repairing, importing and buying a vehicle ALL stop
//! working - with no compile error, no runtime error and no log line, because every call site
//! null-guards the accessor by contract and simply returns. The migration plan therefore requires one
//! Init assertion per new component, added in the phase that creates it (D11), rather than trusting a
//! prefab edit that an agent cannot make in Workbench.
//!
//! TWO CLAIMS, ONE PRECONDITION:
//!   1. the component resolves through the epic-level accessor at all;
//!   2. what comes back is the instance carried by THIS player's own controller entity - not merely
//!      "a component of that type from somewhere", which a Get() that searched the wrong entity would
//!      also satisfy, and which would mean every vehicle request went through another player's seam.
//!
//! THE POLL IS A PRECONDITION, NOT A RETRY (no maxAttempts anywhere in this project).
//! OVT_PlayerManagerComponent.SetupPlayer() spawns the controller when the player enters the world,
//! which is not instantaneous at world load. Expiry is itself a named failure carrying the diagnosis,
//! so nothing here can pass by being asked at a luckier moment.
//!
//! PROVEN ABLE TO FAIL (2026-08-14): the OVT_VehicleRequestComponent block was removed from
//! Prefabs/GameMode/OVT_OverthrowController.et - the literal defect this case is built to catch - and
//! the run failed with "OVT_ControllerComponent<OVT_VehicleRequestComponent>.Get() returned null while
//! a controller entity exists"; block restored, it passes.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Controller_VehicleRequestResolves : SCR_AutotestCaseBase
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
				SetFailure("OVT_Global.GetController() was still null after %1 polls (local player id %2). Nothing on the controller seam is reachable from this machine, so this case cannot say anything about OVT_VehicleRequestComponent either way.",
					m_iPolls.ToString(),
					SCR_PlayerController.GetLocalPlayerId().ToString());
				return true;
			}

			return false;
		}

		// Claim 1 - it is wired.
		OVT_VehicleRequestComponent viaAccessor = OVT_ControllerComponent<OVT_VehicleRequestComponent>.Get();
		if (!viaAccessor)
		{
			SetFailure("OVT_ControllerComponent<OVT_VehicleRequestComponent>.Get() returned null while a controller entity exists. Prefabs/GameMode/OVT_OverthrowController.et is not carrying the component, so lock/unlock, claim, upgrade, repair, import and buy-vehicle all silently never happen - the legacy comms-monolith handlers they used to ride were deleted in the same phase.");
			return true;
		}

		// Claim 2 - it is OUR controller's instance, not somebody else's.
		OVT_VehicleRequestComponent onEntity = OVT_VehicleRequestComponent.Cast(controller.FindComponent(OVT_VehicleRequestComponent));
		if (viaAccessor != onEntity)
		{
			SetFailure("OVT_ControllerComponent<OVT_VehicleRequestComponent>.Get() did not return the instance on the local player's own controller entity. Every vehicle request would then be sent through another player's seam, and the server would resolve the caller as that player.");
			return true;
		}

		PrintFormat("Controller seam: OVT_VehicleRequestComponent resolves off the local controller (found after %1 poll(s))", m_iPolls.ToString());
		return true;
	}
}
