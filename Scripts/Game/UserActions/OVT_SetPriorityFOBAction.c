class OVT_SetPriorityFOBAction : ScriptedUserAction
{	
	
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		OVT_FOBRequestComponent requests = OVT_ControllerComponent<OVT_FOBRequestComponent>.Get();
		if(!requests) return;

		requests.SetPriorityFOB(pOwnerEntity);
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