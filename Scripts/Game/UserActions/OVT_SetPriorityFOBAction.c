class OVT_SetPriorityFOBAction : ScriptedUserAction
{	
	
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
 	{
		OVT_FOBRequestComponent requests = OVT_ControllerComponent<OVT_FOBRequestComponent>.Get();
		if(!requests) return;

		// pOwnerEntity is the cargo-canvas slot CHILD the action lives on, not the FOB truck - it has
		// no RplComponent of its own, so the RplId lookup fails and the action silently does nothing
		// (BUG-190). GetParent() is what the sibling OVT_DeployFOBAction/OVT_UndeployFOBAction
		// already pass.
		requests.SetPriorityFOB(pOwnerEntity.GetParent());
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