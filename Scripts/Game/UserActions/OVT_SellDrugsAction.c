class OVT_SellDrugsAction : OVT_BaseCivilianUserAction
{	
	//---------------------------------------------------------
 	override protected void PerformCivilianAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(pUserEntity.FindComponent( SCR_InventoryStorageManagerComponent ));
		if(!inventory) return;
		
		bool hasDrugs = false;
		autoptr array<IEntity> items = new array<IEntity>;
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
					if(s_AIRandomGenerator.RandFloat01() > 0.25)	
						MarkAsPerformed();	
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
		
	override bool GetActionNameScript(out string outName)
	{
		return false;
	}
}