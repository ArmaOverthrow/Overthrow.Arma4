//------------------------------------------------------------------------------------------------
//! OVT_ResistanceRequestComponent is actually ON the local player's OVT_OverthrowController.
//!
//! WHY THIS CASE EXISTS. Phase 5 of the controller migration deleted six handlers and one owner response
//! from the legacy comms monolith in the same change that created this component. There is therefore no
//! old path left to fall back on: if the prefab block is missing, placing an item, building, removing a
//! placed object, promoting an officer, buying a base garrison and converting a supporter ALL stop
//! working at once - with no compile error, no runtime error and no log line, because every call site
//! null-guards the accessor by contract and simply returns. The plan requires one Init assertion per new
//! component, added in the phase that creates it (D11), precisely because an agent cannot make the
//! Workbench prefab edit and must not assume it happened.
//!
//! TWO CLAIMS, ONE PRECONDITION:
//!   1. the component resolves through the epic-level accessor at all;
//!   2. what comes back is the instance carried by THIS player's own controller entity - not merely "a
//!      component of that type from somewhere", which a Get() searching the wrong entity would also
//!      satisfy, and which would mean every request was attributed to another player server-side.
//!
//! THE POLL IS A PRECONDITION, NOT A RETRY (no maxAttempts anywhere in this project).
//! OVT_PlayerManagerComponent.SetupPlayer() spawns the controller when the player enters the world,
//! which is not instantaneous at world load. Expiry is itself a named failure carrying the diagnosis, so
//! nothing here can pass by being asked at a luckier moment.
//!
//! PROVEN ABLE TO FAIL (2026-08-14): the OVT_ResistanceRequestComponent block was removed from
//! Prefabs/GameMode/OVT_OverthrowController.et - the literal defect this case is built to catch - and
//! the run failed with "OVT_ControllerComponent<OVT_ResistanceRequestComponent>.Get() returned null
//! while a controller entity exists"; block restored, it passes.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Controller_ResistanceRequestResolves : SCR_AutotestCaseBase
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
				SetFailure("OVT_Global.GetController() was still null after %1 polls (local player id %2). Nothing on the controller seam is reachable from this machine, so this case cannot say anything about OVT_ResistanceRequestComponent either way.",
					m_iPolls.ToString(),
					SCR_PlayerController.GetLocalPlayerId().ToString());
				return true;
			}

			return false;
		}

		// Claim 1 - it is wired.
		OVT_ResistanceRequestComponent viaAccessor = OVT_ControllerComponent<OVT_ResistanceRequestComponent>.Get();
		if (!viaAccessor)
		{
			SetFailure("OVT_ControllerComponent<OVT_ResistanceRequestComponent>.Get() returned null while a controller entity exists. Prefabs/GameMode/OVT_OverthrowController.et is not carrying the component, so place, build, remove-placed, add-officer, add-garrison and convert-supporter all silently never happen - the legacy comms-monolith handlers they used to ride were deleted in the same phase.");
			return true;
		}

		// Claim 2 - it is OUR controller's instance, not somebody else's.
		OVT_ResistanceRequestComponent onEntity = OVT_ResistanceRequestComponent.Cast(controller.FindComponent(OVT_ResistanceRequestComponent));
		if (viaAccessor != onEntity)
		{
			SetFailure("OVT_ControllerComponent<OVT_ResistanceRequestComponent>.Get() did not return the instance on the local player's own controller entity. Every resistance request would then be sent through another player's seam, and the server would resolve the caller - and charge, and assign ownership - as that player.");
			return true;
		}

		PrintFormat("Controller seam: OVT_ResistanceRequestComponent resolves off the local controller (found after %1 poll(s))", m_iPolls.ToString());
		return true;
	}
}
