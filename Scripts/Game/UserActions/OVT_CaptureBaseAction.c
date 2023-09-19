class OVT_CaptureBaseAction : ScriptedUserAction
{	
	
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		if(OVT_Global.GetOccupyingFaction().m_bQRFActive) return;
		
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();		
		OVT_BaseData base = of.GetNearestBase(pOwnerEntity.GetOrigin());
		
		if(base.IsOccupyingFaction())
		{
			OVT_Global.GetServer().StartBaseCapture(base.location);
		}
 	}
	
	override bool CanBeShownScript(IEntity user)
	{
		if(OVT_Global.GetOccupyingFaction().m_bQRFActive) return false;
		return EPF_Component<OVT_BaseControllerComponent>.Find(GetOwner()).IsOccupyingFaction();
	}
	
	override bool HasLocalEffectOnlyScript() { return true; };
}