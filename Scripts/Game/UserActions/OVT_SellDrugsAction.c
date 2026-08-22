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
		
		// The seller is resolved server-side from the controller entity the request arrives on, so no
		// player id is sent (controller migration G3/D3).
		OVT_EconomyRequestComponent economyRequests = OVT_ControllerComponent<OVT_EconomyRequestComponent>.Get();
		if(!economyRequests) return;
		
		foreach(IEntity ent : items)
		{
			ResourceName res = ent.GetPrefabData().GetPrefabName();
			if(res.Contains("DrugsWeed_01"))
			{
				economyRequests.SellDrugs(pOwnerEntity);
				if(s_AIRandomGenerator.RandFloat01() > 0.25)
					MarkAsPerformed();
				hasDrugs = true;
				break;
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