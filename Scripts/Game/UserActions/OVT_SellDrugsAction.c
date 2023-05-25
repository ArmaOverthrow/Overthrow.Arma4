class OVT_SellDrugsAction : ScriptedUserAction
{	
	bool m_bHasBeenSold;
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		if(m_bHasBeenSold) return;
		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(pUserEntity.FindComponent( SCR_InventoryStorageManagerComponent ));
		if(!inventory) return;
		
		bool hasDrugs = false;
		array<IEntity> items = new array<IEntity>;
		inventory.GetItems(items);
		
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(SCR_PlayerController.GetLocalControlledEntity());
		
		foreach(IEntity ent : items)
		{
			ResourceName res = ent.GetPrefabData().GetPrefabName();
			if(res.Contains("DrugsWeed_01"))
			{
				int id = economy.GetInventoryId(res);
				if(inventory.TryDeleteItem(ent))
				{					
					int cost = economy.GetBuyPrice(id, pUserEntity.GetOrigin()) * 1.25;
					economy.AddPlayerMoney(playerId, cost);		
					m_bHasBeenSold = true;	
					hasDrugs = true;		
					break;
				}
			}
		}
		
		if(!hasDrugs)
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-NoDrugs");
		}
 	}
	
	override bool CanBePerformedScript(IEntity user)
	{
		return !m_bHasBeenSold;
	}
		
	override bool GetActionNameScript(out string outName)
	{
		return false;
	}	
	
	override bool CanBeShownScript(IEntity user) {
		return !m_bHasBeenSold;
	}
	
	override bool HasLocalEffectOnlyScript() { return true; };
}