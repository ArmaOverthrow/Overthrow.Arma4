class OVT_SetPriorityFOBAction : ScriptedUserAction
{	
	
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		OVT_Global.GetServer().SetPriorityFOB(pOwnerEntity);
 	}
		
	override bool GetActionNameScript(out string outName)
	{
		return false;
	}	
			
	override bool CanBeShownScript(IEntity user) {
		// Only officers can set FOB priority
		if(!OVT_Global.GetPlayers().LocalPlayerIsOfficer()) return false;
		return true;
	}
	
	override bool HasLocalEffectOnlyScript() { return true; };
}