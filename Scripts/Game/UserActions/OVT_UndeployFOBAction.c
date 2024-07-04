class OVT_UndeployFOBAction : ScriptedUserAction
{	
	
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		IEntity parent = pOwnerEntity.GetParent();
		OVT_Global.GetServer().UndeployFOB(parent);
 	}
		
	override bool GetActionNameScript(out string outName)
	{
		return false;
	}	
			
	override bool CanBeShownScript(IEntity user) {
		if(!OVT_Global.GetPlayers().LocalPlayerIsOfficer()) return false;
		return true;
	}
	
	override bool HasLocalEffectOnlyScript() { return true; };
}