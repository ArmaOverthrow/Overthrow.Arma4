class OVT_SetPriorityFOBAction : ScriptedUserAction
{	
	
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
 	{
		// pOwnerEntity is the cargo-canvas slot CHILD the action lives on, not the FOB truck - its
		// origin is a local offset, so the server's 10 m position match against the FOB record never
		// hit and the action silently did nothing (BUG-186). GetParent() is what the sibling
		// OVT_DeployFOBAction/OVT_UndeployFOBAction already pass.
		OVT_Global.GetServer().SetPriorityFOB(pOwnerEntity.GetParent());
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