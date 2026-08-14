class OVT_CaptureBaseAction : ScriptedUserAction
{	
	
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