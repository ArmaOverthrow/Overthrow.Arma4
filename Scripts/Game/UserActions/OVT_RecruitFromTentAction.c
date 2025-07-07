class OVT_RecruitFromTentAction : ScriptedUserAction
{	
	[Attribute()]
	protected ref SCR_HintUIInfo m_RecruitLimitReachedHint;
	
	[Attribute()]
	protected ref SCR_HintUIInfo m_NoSupportersAvailableHint;

	[Attribute()]
	protected ref SCR_HintUIInfo m_CannotAffordHint;
	
	[Attribute()]
	protected ref SCR_HintUIInfo m_RecruitedFromTentHint;

	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		OVT_RecruitManagerComponent recruitManager = OVT_RecruitManagerComponent.GetInstance();
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		
		if (!recruitManager || !townManager)
			return;
		
		int playerId = SCR_PlayerController.GetLocalPlayerId();
		string persId = players.GetPersistentIDFromPlayerID(playerId);
		
		// Check recruit limit
		if (!recruitManager.CanRecruit(persId))
		{
			SCR_HintManagerComponent.ShowHint(m_RecruitLimitReachedHint);
			return;
		}
		
		// Check if nearest town has supporters
		vector tentPos = pOwnerEntity.GetOrigin();
		if (!townManager.NearestTownHasSupporters(tentPos))
		{
			SCR_HintManagerComponent.ShowHint(m_NoSupportersAvailableHint);
			return;
		}
			
		// Reduced cost for tent recruitment (typically 50% of base cost)
		int cost = Math.Round(config.m_Difficulty.baseRecruitCost * 0.5);
		
		if(!economy.LocalPlayerHasMoney(cost)) {
			SCR_HintManagerComponent.ShowHint(m_CannotAffordHint);
			return;
		}
		
		economy.TakeLocalPlayerMoney(cost);
		
		// Call server to handle the actual recruitment
		OVT_Global.GetServer().RecruitFromTent(tentPos, playerId);
		
		SCR_HintManagerComponent.ShowHint(m_RecruitedFromTentHint);
 	}
	
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}
		
	override bool GetActionNameScript(out string outName)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		int cost = Math.Round(config.m_Difficulty.baseRecruitCost * 0.5);
		outName = "#OVT-RecruitFromTent ($" + cost.ToString() + ")";
		return true;
	}	
	
	override bool CanBeShownScript(IEntity user) {
		return true;
	}

	override bool HasLocalEffectOnlyScript() { return true; }
}