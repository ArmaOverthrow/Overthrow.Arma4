//------------------------------------------------------------------------------------------------
//! OVT_RealEstateRequestComponent is actually ON the local player's OVT_OverthrowController.
//!
//! WHY THIS CASE EXISTS. Phase 3 of the controller migration deleted eight handlers from
//! the legacy comms monolith in the same change that created this component. There is no old path left
//! to fall back to, so a missing prefab block means buying, selling, renting and un-renting a building,
//! and setting your home silently stop working - with no compile error,
//! no runtime error and no log line, because every call site null-guards the accessor by contract and
//! simply returns. The plan therefore requires one Init assertion per new component, added in the phase
//! that creates it (D11), rather than trusting a prefab edit an agent cannot make in Workbench.
//!
//! TWO CLAIMS, ONE PRECONDITION:
//!   1. the component resolves through the epic-level accessor at all;
//!   2. what comes back is the instance carried by THIS player's own controller entity - not merely
//!      "a component of that type from somewhere", which a Get() that searched the wrong entity would
//!      also satisfy, and which would mean every request resolved the caller as another player.
//!
//! THE POLL IS A PRECONDITION, NOT A RETRY (no maxAttempts anywhere in this project).
//! OVT_PlayerManagerComponent.SetupPlayer() spawns the controller when the player enters the world,
//! which is not instantaneous at world load. Expiry is itself a named failure carrying the diagnosis,
//! so nothing here can pass by being asked at a luckier moment.
//!
//! PROVEN ABLE TO FAIL (2026-08-14): the OVT_RealEstateRequestComponent block was removed from
//! Prefabs/GameMode/OVT_OverthrowController.et - the literal defect this case is built to catch - and
//! the run failed with "OVT_ControllerComponent<OVT_RealEstateRequestComponent>.Get() returned null
//! while a controller entity exists"; block restored, it passes.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Controller_RealEstateRequestResolves : SCR_AutotestCaseBase
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
				SetFailure("OVT_Global.GetController() was still null after %1 polls (local player id %2). Nothing on the controller seam is reachable from this machine, so this case cannot say anything about OVT_RealEstateRequestComponent either way.",
					m_iPolls.ToString(),
					SCR_PlayerController.GetLocalPlayerId().ToString());
				return true;
			}

			return false;
		}

		// Claim 1 - it is wired.
		OVT_RealEstateRequestComponent viaAccessor = OVT_ControllerComponent<OVT_RealEstateRequestComponent>.Get();
		if (!viaAccessor)
		{
			SetFailure("OVT_ControllerComponent<OVT_RealEstateRequestComponent>.Get() returned null while a controller entity exists. Prefabs/GameMode/OVT_OverthrowController.et is not carrying the component, so buy/sell/rent/stop-renting a building and set-home silently never happen - the legacy comms-monolith handlers they used to ride were deleted in the same phase.");
			return true;
		}

		// Claim 2 - it is OUR controller's instance, not somebody else's.
		OVT_RealEstateRequestComponent onEntity = OVT_RealEstateRequestComponent.Cast(controller.FindComponent(OVT_RealEstateRequestComponent));
		if (viaAccessor != onEntity)
		{
			SetFailure("OVT_ControllerComponent<OVT_RealEstateRequestComponent>.Get() did not return the instance on the local player's own controller entity. Every real estate request would then be sent through another player's seam, and the server would resolve the caller - and therefore the building's new owner - as that player.");
			return true;
		}

		PrintFormat("Controller seam: OVT_RealEstateRequestComponent resolves off the local controller (found after %1 poll(s))", m_iPolls.ToString());
		return true;
	}
}
