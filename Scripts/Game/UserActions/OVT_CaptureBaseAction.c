class OVT_CaptureBaseAction : ScriptedUserAction
{	
	
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		
		OVT_BaseControllerComponent base = of.GetNearestBase(pOwnerEntity.GetOrigin());
		
		if(base.IsOccupyingFaction())
		{
			base.StartCapture();
		}
 	}
}