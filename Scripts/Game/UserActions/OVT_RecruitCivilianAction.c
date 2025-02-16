class OVT_RecruitCivilianAction : ScriptedUserAction
{	
	bool m_bHasBeenRecruited;
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		if(m_bHasBeenRecruited) return;
		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(pUserEntity.FindComponent( SCR_InventoryStorageManagerComponent ));
		if(!inventory) return;
		
		bool hasDrugs = false;
		autoptr array<IEntity> items = new array<IEntity>;
		inventory.GetItems(items);
		
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();		
		
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(SCR_PlayerController.GetLocalControlledEntity());
		string persId = players.GetPersistentIDFromPlayerID(playerId);
		
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!playerController)
			return;
			
		int cost = config.m_Difficulty.baseRecruitCost;
		
		if(!economy.PlayerHasMoney(persId, cost)) {
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-CannotAfford");
			return;
		}
		
		economy.TakePlayerMoney(playerId, cost);
		
		m_bHasBeenRecruited = true;
		
		SCR_PlayerControllerGroupComponent groupController = SCR_PlayerControllerGroupComponent.Cast(playerController.FindComponent(SCR_PlayerControllerGroupComponent));
		if (!groupController)
			return;
		
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(pOwnerEntity);
		if (!character)
			return;
		
		FactionAffiliationComponent aff = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
		if(!aff)
			return;
		
		aff.SetAffiliatedFactionByKey("CIV");
		
		if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(character) == 0)
			groupController.RequestAddAIAgent(character, playerId);
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