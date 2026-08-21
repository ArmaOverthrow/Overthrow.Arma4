//------------------------------------------------------------------------------------------------
//! Calling the assault on an occupying-faction base. Same reasoning as OVT_StartUprisingAction:
//! it is a long hold performed on ground the occupying faction still holds, so it is illegal and
//! being seen doing it - at any point in the hold - makes the player wanted.
//------------------------------------------------------------------------------------------------
class OVT_CaptureBaseAction : ScriptedUserAction
{	
	//---------------------------------------------------------
	//! Report the illegal act as it BEGINS, not when it completes - the hold is what gets seen.
	override void OnActionStart(IEntity pUserEntity)
	{
		OVT_IllegalActionComponent illegal = OVT_ControllerComponent<OVT_IllegalActionComponent>.Get();
		if(illegal)
			illegal.ReportActionStarted(OVT_EIllegalAction.BASE_ASSAULT);
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
		
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();		
		OVT_BaseData base = of.GetNearestBase(pOwnerEntity.GetOrigin());
		
		if(base && base.IsOccupyingFaction())
		{
			// The server takes the capture position from the caller's own character (BUG-025), so no
			// position is sent - this action only has to say "I am asking".
			OVT_CampaignRequestComponent campaign = OVT_ControllerComponent<OVT_CampaignRequestComponent>.Get();
			if(!campaign) return;

			campaign.StartBaseCapture();
		}
 	}
	
	override bool CanBeShownScript(IEntity user)
	{
		if(OVT_Global.GetOccupyingFaction().m_bQRFActive) return false;
		return OVT_ComponentFinder<OVT_BaseControllerComponent>.Find(GetOwner()).IsOccupyingFaction();
	}
	
	override bool HasLocalEffectOnlyScript() { return true; };
}