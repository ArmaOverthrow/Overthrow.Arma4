class OVT_RecruitCivilianAction : ScriptedUserAction
{	
	bool m_bHasBeenRecruited;
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		if(m_bHasBeenRecruited) return;
		
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		OVT_RecruitManagerComponent recruitManager = OVT_RecruitManagerComponent.GetInstance();
		
		if (!recruitManager)
			return;
		
		int playerId = SCR_PlayerController.GetLocalPlayerId();
		string persId = players.GetPersistentIDFromPlayerID(playerId);
		
		// Check recruit limit
		if (!recruitManager.CanRecruit(persId))
		{
			SCR_HintManagerComponent.ShowCustomHint("#OVT-RecruitLimitReached");
			return;
		}
			
		int cost = config.m_Difficulty.baseRecruitCost;
		
		if(!economy.LocalPlayerHasMoney(cost)) {
			SCR_HintManagerComponent.ShowCustomHint("#OVT-CannotAfford");
			return;
		}
		
		economy.TakeLocalPlayerMoney(cost);
		m_bHasBeenRecruited = true;
		
		// Call server to handle the actual recruitment
		OVT_Global.GetServer().RecruitCivilian(pOwnerEntity, playerId);
 	}
	
	override bool CanBePerformedScript(IEntity user)
	{
		return !m_bHasBeenRecruited;
	}
		
	override bool GetActionNameScript(out string outName)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		int cost = config.m_Difficulty.baseRecruitCost;
		outName = "#OVT-RecruitCivilian ($" + cost.ToString() + ")";
		return true;
	}	
	
	override bool CanBeShownScript(IEntity user) {
		return !m_bHasBeenRecruited;
	}
	
	override bool HasLocalEffectOnlyScript() { return true; }
}