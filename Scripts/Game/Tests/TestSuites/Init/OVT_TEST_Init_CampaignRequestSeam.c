//------------------------------------------------------------------------------------------------
//! OVT_CampaignRequestComponent is actually ON the local player's OVT_OverthrowController.
//!
//! WHY THIS CASE EXISTS. Phase 8 of the controller migration deleted the base-capture, medical-supplies,
//! loot-wanted-check and save handlers from the legacy comms monolith in the same change that created this
//! component - and emptied that monolith completely. There is no old path left to fall back to, so a
//! "written, compiled, never wired to the prefab" mistake means capturing a base, delivering medical
//! supplies, the authoritative loot escalation and SAVING FROM THE MAIN MENU all silently stop working,
//! with no compile error, no runtime error and no log line, because every call site null-guards the
//! accessor by contract. The migration plan therefore requires one Init assertion per new component,
//! added in the phase that creates it (D11), rather than trusting a prefab edit an agent cannot make in
//! Workbench.
//!
//! THE SAVE PATH IS WHY THIS ONE MATTERS MOST. OVT_MainMenuContext.Save() treats a null seam as an
//! immediate "#OVT-SaveFailed", so an unwired component would not even look like a bug - it would look
//! like persistence itself failing.
//!
//! TWO CLAIMS, ONE PRECONDITION:
//!   1. the component resolves through the epic-level accessor at all;
//!   2. what comes back is the instance carried by THIS player's own controller entity - not merely "a
//!      component of that type from somewhere", which a Get() that searched the wrong entity would also
//!      satisfy. Here the consequence is concrete: the save result comes back RplRcver.Owner on the
//!      requesting controller, so a menu subscribed to another player's instance would never be told the
//!      outcome, and the server would resolve the capture position from the wrong character.
//!
//! THE POLL IS A PRECONDITION, NOT A RETRY (no maxAttempts anywhere in this project).
//! OVT_PlayerManagerComponent.SetupPlayer() spawns the controller when the player enters the world, which
//! is not instantaneous at world load. Expiry is itself a named failure carrying the diagnosis, so nothing
//! here can pass by being asked at a luckier moment.
//!
//! PROVEN ABLE TO FAIL (2026-08-14): the OVT_CampaignRequestComponent block was removed from
//! Prefabs/GameMode/OVT_OverthrowController.et - the literal defect this case is built to catch - and the
//! run failed with "OVT_ControllerComponent<OVT_CampaignRequestComponent>.Get() returned null while a
//! controller entity exists"; block restored, it passes.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_Controller_CampaignRequestResolves : SCR_AutotestCaseBase
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
				SetFailure("OVT_Global.GetController() was still null after %1 polls (local player id %2). Nothing on the controller seam is reachable from this machine, so this case cannot say anything about OVT_CampaignRequestComponent either way.",
					m_iPolls.ToString(),
					SCR_PlayerController.GetLocalPlayerId().ToString());
				return true;
			}

			return false;
		}

		// Claim 1 - it is wired.
		OVT_CampaignRequestComponent viaAccessor = OVT_ControllerComponent<OVT_CampaignRequestComponent>.Get();
		if (!viaAccessor)
		{
			SetFailure("OVT_ControllerComponent<OVT_CampaignRequestComponent>.Get() returned null while a controller entity exists. Prefabs/GameMode/OVT_OverthrowController.et is not carrying the component, so starting a base capture, delivering medical supplies, the server-side loot wanted check and saving from the main menu all silently never happen - the legacy comms-monolith handlers they used to ride were deleted in the same phase, and the save menu reports #OVT-SaveFailed for a save it never asked for.");
			return true;
		}

		// Claim 2 - it is OUR controller's instance, not somebody else's.
		OVT_CampaignRequestComponent onEntity = OVT_CampaignRequestComponent.Cast(controller.FindComponent(OVT_CampaignRequestComponent));
		if (viaAccessor != onEntity)
		{
			SetFailure("OVT_ControllerComponent<OVT_CampaignRequestComponent>.Get() did not return the instance on the local player's own controller entity. The save result is an RplRcver.Owner reply on the requesting controller, so the menu would subscribe to an invoker the server never fires, and the base capture would be positioned on another player's character.");
			return true;
		}

		// The save invoker is part of this component's public contract (BUG-006): OVT_MainMenuContext
		// subscribes to it BEFORE requesting, and shows nothing until it fires. A null invoker would be a
		// VME in the menu rather than a failed save.
		if (!viaAccessor.GetOnSaveResult())
		{
			SetFailure("OVT_CampaignRequestComponent.GetOnSaveResult() returned null. OVT_MainMenuContext.Save() subscribes to it before sending the request, so this is a VME in the pause menu, and the BUG-006 contract - never claim success on send - has nothing left to report through.");
			return true;
		}

		PrintFormat("Controller seam: OVT_CampaignRequestComponent resolves off the local controller with a live save invoker (found after %1 poll(s))", m_iPolls.ToString());
		return true;
	}
}
