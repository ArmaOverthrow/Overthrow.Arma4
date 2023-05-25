class OVT_CaptureBaseAction : ScriptedUserAction
{	
	
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();		
		OVT_BaseData base = of.GetNearestBase(pOwnerEntity.GetOrigin());
		
		if(base.IsOccupyingFaction())
		{
			OVT_Global.GetServer().StartBaseCapture(base.location);
		}
 	}
	
	override bool HasLocalEffectOnlyScript() { return true; };
}