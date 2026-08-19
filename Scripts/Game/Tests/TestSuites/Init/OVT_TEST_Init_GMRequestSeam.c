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

//------------------------------------------------------------------------------------------------
//! Every campaign scalar survives a snapshot commit, and every one of them is dropped again by
//! Clear() - including the objective pair occupying/counter-attacks appended.
//!
//! 🔴 THE THREE-METHOD TRAP THIS CASE EXISTS FOR. A new scalar on OVT_GMCampaignState has to be added
//! in THREE places: the declaration, CopyFrom() and Clear(). Missing it from CopyFrom() is loud - the
//! panel row never fills in and somebody notices in a minute. Missing it from Clear() is silent and
//! WRONG-LOOKING RATHER THAN EMPTY: the editor closes, the store is emptied, a second campaign starts
//! in the same client session, and one row keeps displaying a value from a game that has ended. Nothing
//! compiles differently, nothing logs, and the only symptom is a Game Master reading a stale town name
//! off a panel that is otherwise correct.
//!
//! ⚠ IT ASSERTS OVER ALL OF THEM, NOT JUST THE NEW PAIR, AND THAT IS THE DURABLE PART. The next field
//! appended to this class gets the same protection only if this case is extended, so it is written as
//! a checklist a reader can compare against the class declaration line by line.
//!
//! ⚠ m_fReceivedWorldTime IS DELIBERATELY NOT A CopyFrom CLAIM. The commit path stamps it after the
//! copy - it is "when did this land on this machine", not part of the payload - so it is asserted on
//! the Clear() half only, where it genuinely must go back to zero (it is what HasData() answers on, and
//! a store that still claims to hold data after being emptied is the worst version of this bug).
//!
//! IT IS A TIER B CASE RATHER THAN A TIER A ONE because the class it exercises reaches for the world
//! clock in its countdown readers; nothing here calls those, but the tier rule is about the class, not
//! about the path taken through it.
//!
//! HOW TO PROVE IT CAN FAIL: delete `m_sObjectiveName = "";` from OVT_GMCampaignState.Clear() and
//! re-run. The tree compiles clean - a field nobody clears is not a script error - and the case reports
//! "Clear() left the objective name behind: 'Chotain' should be empty". Same for any other line in
//! either method.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_GMCampaignState_CarriesAndClearsEveryScalar : SCR_AutotestCaseBase
{
	//! Tolerance for the float scalars. They are copied verbatim rather than computed, so this is only
	//! guarding against the tier's own rule about comparing floats, not against arithmetic.
	protected const float TOLERANCE = 0.0001;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- ARRANGE a staging store in which no two fields share a value, so a copy that crossed two
		//     of them over cannot pass.
		OVT_GMCampaignState staging = new OVT_GMCampaignState();
		staging.m_fThreat = 3.75;
		staging.m_iOFResources = 12345;
		staging.m_iOFDeploymentResources = 6789;
		staging.m_iFlags = OVT_GMCampaignState.FLAG_DISTRIBUTION_SUPPRESSED_QRF;
		staging.m_iDistributionAmount = 401;
		staging.m_fDistributionSeconds = 88.5;
		staging.m_iPayoutAmount = 902;
		staging.m_fPayoutSeconds = 177.25;
		staging.m_iReportedRecordCount = 57;
		staging.m_sObjectiveName = "Chotain";
		staging.m_iObjectivePhase = OVT_EObjectivePhase.COUNTER_QRF;

		// --- ACT: the commit path, which is one CopyFrom.
		OVT_GMCampaignState live = new OVT_GMCampaignState();
		live.CopyFrom(staging);

		// --- ASSERT the payload survived, field by field.
		if (Math.AbsFloat(live.m_fThreat - 3.75) > TOLERANCE)
		{
			SetFailure("CopyFrom lost the threat: got %1, expected 3.75", live.m_fThreat.ToString());
			return true;
		}

		if (live.m_iOFResources != 12345)
		{
			SetFailure("CopyFrom lost the occupying faction's reserve: got %1, expected 12345", live.m_iOFResources.ToString());
			return true;
		}

		if (live.m_iOFDeploymentResources != 6789)
		{
			SetFailure("CopyFrom lost the deployment pool: got %1, expected 6789", live.m_iOFDeploymentResources.ToString());
			return true;
		}

		if (live.m_iFlags != OVT_GMCampaignState.FLAG_DISTRIBUTION_SUPPRESSED_QRF)
		{
			SetFailure("CopyFrom lost the suppression flags: got %1", live.m_iFlags.ToString());
			return true;
		}

		if (live.m_iDistributionAmount != 401)
		{
			SetFailure("CopyFrom lost the distribution amount: got %1, expected 401", live.m_iDistributionAmount.ToString());
			return true;
		}

		if (Math.AbsFloat(live.m_fDistributionSeconds - 88.5) > TOLERANCE)
		{
			SetFailure("CopyFrom lost the distribution countdown: got %1, expected 88.5", live.m_fDistributionSeconds.ToString());
			return true;
		}

		if (live.m_iPayoutAmount != 902)
		{
			SetFailure("CopyFrom lost the payout amount: got %1, expected 902", live.m_iPayoutAmount.ToString());
			return true;
		}

		if (Math.AbsFloat(live.m_fPayoutSeconds - 177.25) > TOLERANCE)
		{
			SetFailure("CopyFrom lost the payout countdown: got %1, expected 177.25", live.m_fPayoutSeconds.ToString());
			return true;
		}

		if (live.m_iReportedRecordCount != 57)
		{
			SetFailure("CopyFrom lost the server's record count: got %1, expected 57", live.m_iReportedRecordCount.ToString());
			return true;
		}

		// --- THE NEW PAIR. A Game Master panel with these missing shows two permanently blank rows.
		if (live.m_sObjectiveName != "Chotain")
		{
			SetFailure("CopyFrom lost the objective name: got '%1', expected 'Chotain' - the panel's objective row would be permanently blank", live.m_sObjectiveName);
			return true;
		}

		if (live.m_iObjectivePhase != OVT_EObjectivePhase.COUNTER_QRF)
		{
			SetFailure("CopyFrom lost the objective phase: got %1, expected 3", live.m_iObjectivePhase.ToString());
			return true;
		}

		// --- ACT again: the editor closes.
		live.m_fReceivedWorldTime = 999;
		live.Clear();

		// --- ASSERT nothing was left behind. This half is the one with no other symptom.
		if (live.HasData())
		{
			SetFailure("Clear() left the store claiming to hold data - every consumer gates on HasData(), so a stale snapshot would be drawn as a live one");
			return true;
		}

		if (Math.AbsFloat(live.m_fThreat) > TOLERANCE || live.m_iOFResources != 0 || live.m_iOFDeploymentResources != 0 || live.m_iFlags != 0)
		{
			SetFailure("Clear() left a resource scalar behind: threat %1, reserve %2, pool %3", live.m_fThreat.ToString(), live.m_iOFResources.ToString(), live.m_iOFDeploymentResources.ToString());
			return true;
		}

		if (live.m_iDistributionAmount != 0 || Math.AbsFloat(live.m_fDistributionSeconds) > TOLERANCE || live.m_iPayoutAmount != 0 || Math.AbsFloat(live.m_fPayoutSeconds) > TOLERANCE)
		{
			SetFailure("Clear() left a schedule scalar behind: distribution %1 in %2 s, payout %3", live.m_iDistributionAmount.ToString(), live.m_fDistributionSeconds.ToString(), live.m_iPayoutAmount.ToString());
			return true;
		}

		if (live.m_iReportedRecordCount != 0)
		{
			SetFailure("Clear() left the record count behind: %1", live.m_iReportedRecordCount.ToString());
			return true;
		}

		if (live.m_sObjectiveName != "")
		{
			SetFailure("Clear() left the objective name behind: '%1' should be empty - the next campaign in this session would open showing the previous one's target", live.m_sObjectiveName);
			return true;
		}

		if (live.m_iObjectivePhase != OVT_EObjectivePhase.IDLE)
		{
			SetFailure("Clear() left the objective phase behind: %1 should be 0 - the next campaign in this session would open claiming a ramp that is not running", live.m_iObjectivePhase.ToString());
			return true;
		}

		Print("GM campaign store: every campaign scalar survives a commit and every one of them is dropped when the editor closes, so no row can carry a value out of the campaign it belonged to");

		return true;
	}
}
