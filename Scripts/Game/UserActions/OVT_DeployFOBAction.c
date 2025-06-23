class OVT_DeployFOBAction : ScriptedUserAction
{	
	
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		Print(pOwnerEntity);
		Print(pOwnerEntity.GetParent());
		OVT_Global.GetServer().DeployFOB(pOwnerEntity.GetParent());
 	}
		
	override bool GetActionNameScript(out string outName)
	{
		return false;
	}	
			
	override bool CanBeShownScript(IEntity user) {
		// Check server config to see if FOB deployment is restricted to officers only
		if(OVT_Global.GetConfig().m_ConfigFile.mobileFOBOfficersOnly)
		{
			if(!OVT_Global.GetPlayers().LocalPlayerIsOfficer()) return false;
		}
		return true;
	}
	
	override bool HasLocalEffectOnlyScript() { return true; };
}