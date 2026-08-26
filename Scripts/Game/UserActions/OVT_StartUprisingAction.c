//------------------------------------------------------------------------------------------------
//! Raising the flag on an occupied town. A long hold, in the open, in the middle of a town the
//! occupying faction still holds - so it is ILLEGAL, and being seen at any point during the hold
//! makes the player wanted (OVT_IllegalActionComponent opens the window server-side; the escalation
//! itself is OVT_PlayerWantedComponent's, gated on actually being observed).
//------------------------------------------------------------------------------------------------
class OVT_StartUprisingAction : ScriptedUserAction
{
	//---------------------------------------------------------
	//! Report the illegal act as it BEGINS, not when it completes - the hold is what gets seen.
	override void OnActionStart(IEntity pUserEntity)
	{
		OVT_IllegalActionComponent illegal = OVT_ControllerComponent<OVT_IllegalActionComponent>.Get();
		if(illegal)
			illegal.ReportActionStarted(OVT_EIllegalAction.UPRISING);
	}

	//---------------------------------------------------------
	//! Let go of it early and nobody has anything on you.
	override void OnActionCanceled(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		OVT_IllegalActionComponent illegal = OVT_ControllerComponent<OVT_IllegalActionComponent>.Get();
		if(illegal)
			illegal.ReportActionCancelled();
	}

	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
 	{
		if(OVT_Global.GetOccupyingFaction().m_bQRFActive) return;

		OVT_UprisingRequestComponent uprising = OVT_ControllerComponent<OVT_UprisingRequestComponent>.Get();
		if(!uprising)
		{
			Print("[Overthrow] No OVT_UprisingRequestComponent on the local controller - uprising request dropped", LogLevel.ERROR);
			return;
		}

		int townId = OVT_Global.GetTowns().GetNearestTownId(pOwnerEntity.GetOrigin());
		uprising.RequestStartUprising(townId);
 	}

	override bool CanBeShownScript(IEntity user)
	{
		if(OVT_Global.GetOccupyingFaction().m_bQRFActive) return false;

		OVT_TownData town = OVT_Global.GetTowns().GetNearestTown(GetOwner().GetOrigin());
		if(!town) return false;

		// Villages flip peacefully through support, no battle to start
		if(town.size == OVT_TownSize.VILLAGE) return false;

		return town.IsOccupyingFaction();
	}

	override bool CanBePerformedScript(IEntity user)
	{
		OVT_TownData town = OVT_Global.GetTowns().GetNearestTown(GetOwner().GetOrigin());
		if(!town) return false;

		if(town.SupportPercentage() <= OVT_OccupyingFactionManager.UPRISING_SUPPORT_THRESHOLD)
		{
			SetCannotPerformReason("#OVT-SupportTooLow");
			return false;
		}

		return true;
	}

	override bool HasLocalEffectOnlyScript() { return true; };
}
