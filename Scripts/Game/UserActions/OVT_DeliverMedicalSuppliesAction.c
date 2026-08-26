
class OVT_DeliverMedicalSuppliesAction : ScriptedUserAction
{
	
	#ifndef DISABLE_INVENTORY
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{		
		
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		
		OVT_TownData town = towns.GetNearestTown(pOwnerEntity.GetOrigin());
		
		if(!IsInTownRange(pOwnerEntity, town))
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
		
		// Guard the seam BEFORE the success hint: an unavailable controller (dedicated client before owner
		// assignment) must not tell the player their supplies were delivered (P6-5).
		OVT_CampaignRequestComponent campaign = OVT_ControllerComponent<OVT_CampaignRequestComponent>.Get();
		if(!campaign) return;

		SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-MedicalSuppliesDelivered $" + cost.ToString());

		campaign.DeliverMedicalSupplies(pOwnerEntity);
	}
	
	override bool HasLocalEffectOnlyScript() { return true; };
	
	//------------------------------------------------------------------------------------------------
	//! Greys the action out away from a town instead of letting the player hold it and be told no. The
	//! same test PerformAction makes, so the button and the act can never disagree.
	//! \param[in] user The acting character.
	//! \return True when the vehicle is inside the nearest settlement's own radius.
	override event bool CanBePerformedScript(IEntity user)
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if(!towns) return false;
		
		if(!IsInTownRange(GetOwner(), towns.GetNearestTown(GetOwner().GetOrigin())))
		{
			SetCannotPerformReason("#OVT-MedicalSupplies_TooFar");
			return false;
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Whether \a vehicle is within the settlement's size-dependent radius of its centre.
	//! \param[in] vehicle The vehicle carrying the supplies.
	//! \param[in] town The nearest town, village or city.
	//! \return True when the delivery point has been reached.
	protected bool IsInTownRange(IEntity vehicle, OVT_TownData town)
	{
		if(!vehicle || !town) return false;
		
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if(!towns) return false;
		
		return vector.Distance(town.location, vehicle.GetOrigin()) <= towns.GetTownRange(town);
	}
	
	override event bool CanBeShownScript(IEntity user)
	{		
		OVT_OverthrowGameMode ot = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(!ot) return false;
		
		OVT_PlayerOwnerComponent playerowner = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(GetOwner());
		if(!playerowner) return false;
		if(!playerowner.IsLocked()) return true;
				
		string ownerUid = playerowner.GetPlayerOwnerUid();		
		string playerUid = OVT_Global.GetPlayers().GetPersistentIDFromControlledEntity(user);
		if(ownerUid != playerUid) return false;
		
		return true;
	}
	
	#endif		
};