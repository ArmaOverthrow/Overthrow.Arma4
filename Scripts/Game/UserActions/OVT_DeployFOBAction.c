class OVT_DeployFOBAction : ScriptedUserAction
{	
	
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
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
	
	override bool CanBePerformedScript(IEntity user) {
		// Check if too close to enemy bases
		
		vector fobPos = user.GetOrigin();
		OVT_OccupyingFactionManager occupyingFaction = OVT_Global.GetOccupyingFaction();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		
		// Check distance to ALL bases (occupying faction and resistance)
		foreach(OVT_BaseData base : occupyingFaction.m_Bases)
		{
			float distance = vector.Distance(base.location, fobPos);
			// Use base close range + extra buffer (50m)
			float restrictedDistance = config.m_Difficulty.baseCloseRange + 50;
			
			if(distance < restrictedDistance)
			{
				SetCannotPerformReason("#OVT-TooCloseBase");
				return false;
			}
		}
		
		// Check distance to ALL radio towers (occupying faction and resistance)
		foreach(OVT_RadioTowerData tower : occupyingFaction.m_RadioTowers)
		{
			float distance = vector.Distance(tower.location, fobPos);
			// Radio towers have 20m range + extra buffer (50m)
			float restrictedDistance = 20 + 50;
			
			if(distance < restrictedDistance)
			{
				SetCannotPerformReason("#OVT-TooCloseToRadioTower");
				return false;
			}
		}
		
		return true;
	}
	
	override bool HasLocalEffectOnlyScript() { return true; };
}