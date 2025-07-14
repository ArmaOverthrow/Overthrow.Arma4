
class OVT_DeliverMedicalSuppliesAction : ScriptedUserAction
{
	
	#ifndef DISABLE_INVENTORY
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{		
		
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		
		OVT_TownData town = towns.GetNearestTown(pOwnerEntity.GetOrigin());
		
		float dist = vector.Distance(town.location, pOwnerEntity.GetOrigin());
		float range = towns.m_iCityRange;
		if(town.size == 1) range = towns.m_iVillageRange;
		if(town.size == 2) range = towns.m_iTownRange;
		
		if(dist > range)
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-MedicalSupplies_TooFar");
			return;
		}
		
		SCR_VehicleInventoryStorageManagerComponent vehicleStorage = SCR_VehicleInventoryStorageManagerComponent.Cast(pOwnerEntity.FindComponent(SCR_VehicleInventoryStorageManagerComponent));
		if(!vehicleStorage)
		{
			return;
		}
				
		autoptr array<IEntity> items = new array<IEntity>;
		vehicleStorage.GetItems(items);
		if(items.Count() == 0) {
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-VehicleEmpty");
			return;
		}
		
		int cost = 0;
		foreach(IEntity item : items)
		{
			ResourceName res = item.GetPrefabData().GetPrefabName();
			if(!economy.IsSoldAtShop(res, OVT_ShopType.SHOP_DRUG)) continue;			
			cost += economy.GetPriceByResource(res, town.location);
		}
		
		if(cost == 0)
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-NoMedicalSupplies");
			return;
		}
		
		SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-MedicalSuppliesDelivered $" + cost.ToString());
		
		OVT_Global.GetServer().DeliverMedicalSupplies(pOwnerEntity);
	}
	
	override bool HasLocalEffectOnlyScript() { return true; };
	
	override event bool CanBeShownScript(IEntity user)
	{		
		OVT_OverthrowGameMode ot = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(!ot) return false;
		
		OVT_PlayerOwnerComponent playerowner = EPF_Component<OVT_PlayerOwnerComponent>.Find(GetOwner());
		if(!playerowner) return false;
		if(!playerowner.IsLocked()) return true;
				
		string ownerUid = playerowner.GetPlayerOwnerUid();		
		string playerUid = OVT_Global.GetPlayers().GetPersistentIDFromControlledEntity(user);
		if(ownerUid != playerUid) return false;
		
		return true;
	}
	
	#endif		
};