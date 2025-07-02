class OVT_RecruitFromTentAction : ScriptedUserAction
{	
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
			SCR_HintManagerComponent.ShowCustomHint("#OVT-RecruitLimitReached");
			return;
		}
		
		// Check if nearest town has supporters
		vector tentPos = pOwnerEntity.GetOrigin();
		if (!townManager.NearestTownHasSupporters(tentPos))
		{
			SCR_HintManagerComponent.ShowCustomHint("#OVT-NoSupportersAvailable");
			return;
		}
			
		// Reduced cost for tent recruitment (typically 50% of base cost)
		int cost = Math.Round(config.m_Difficulty.baseRecruitCost * 0.5);
		
		if(!economy.LocalPlayerHasMoney(cost)) {
			SCR_HintManagerComponent.ShowCustomHint("#OVT-CannotAfford");
			return;
		}
		
		economy.TakeLocalPlayerMoney(cost);
		
		// Take supporters from nearest town (1 supporter per recruit)
		townManager.TakeSupportersFromNearestTown(tentPos, 1);
		
		// Spawn recruit at tent location
		SCR_ChimeraCharacter recruit = recruitManager.SpawnRecruit(tentPos + "2 0 2"); // Offset from tent
		if (recruit)
		{
			// Add to recruit manager
			recruitManager.RecruitCivilian(recruit, playerId);
			
			SCR_HintManagerComponent.ShowCustomHint("#OVT-RecruitedFromTent");
		}
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