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
		// Don't allow recruiting someone who is already a recruit
		if (IsRecruit(GetOwner()))
			return false;
			
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
		// Don't show for recruits
		if (IsRecruit(GetOwner()))
			return false;
			
		return !m_bHasBeenRecruited;
	}
	
	override bool HasLocalEffectOnlyScript() { return true; }
	
	//! Check if the entity is a recruit
	protected bool IsRecruit(IEntity entity)
	{
		if (!entity)
			return false;
			
		OVT_RecruitManagerComponent recruitManager = OVT_Global.GetRecruits();
		if (!recruitManager)
			return false;
			
		return recruitManager.GetRecruitFromEntity(entity) != null;
	}
}