class OVT_PlayerCommsComponentClass: OVT_ComponentClass
{
};

class OVT_PlayerCommsComponent: OVT_Component
{
	bool takingMoney = false;
	bool addingMoney = false;
	
	void RequestSave()
	{
		Rpc(RpcAsk_RequestSave);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_RequestSave()
	{
		OVT_OverthrowGameMode overthrow = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(!overthrow) return;
		OVT_PersistenceManagerComponent persistence = overthrow.GetPersistence();
		if(persistence && persistence.IsActive())
		{
			persistence.SaveGame();
		}
	}
	
	void SendNotification(string tag, int playerId = -1, string param1 = "", string param2="", string param3="")
	{
		Rpc(RpcAsk_SendNotification, tag, playerId, param1, param2, param3);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_SendNotification(string tag, int playerId, string param1, string param2, string param3)
	{
		OVT_Global.GetNotify().SendTextNotification(tag,playerId,param1,param2,param3);
	}
	
	void AddSupporters(vector location, int num)
	{
		Rpc(RpcAsk_AddSupporters, location, num);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_AddSupporters(vector location, int num)
	{
		OVT_Global.GetTowns().AddSupport(location, num);		
	}
	
	void BuySkill(int playerId, string key)
	{
		Rpc(RpcAsk_BuySkill, playerId, key);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_BuySkill(int playerId, string key)
	{
		OVT_Global.GetSkills().AddSkillLevel(playerId, key);
	}
	
	void StartBaseCapture(vector loc)
	{		
		Rpc(RpcAsk_StartBaseCapture, loc);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_StartBaseCapture(vector loc)
	{	
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		OVT_BaseData data = of.GetNearestBase(loc);
		OVT_BaseControllerComponent base = of.GetBase(data.entId);
		of.StartBaseQRF(base);
	}
	
	void LootIntoVehicle(IEntity vehicle)
	{
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		Rpc(RpcAsk_LootIntoVehicle, rpl.Id());
	}
	
	ref array<IEntity> m_aLootBodies;
	RplId m_LootVehicle;
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LootIntoVehicle(RplId vehicleId)
	{	
		m_LootVehicle = vehicleId;
		RplComponent rplComp = RplComponent.Cast(Replication.FindItem(vehicleId));
		if(!rplComp) return;
		
		IEntity vehicle = rplComp.GetEntity();
		
		UniversalInventoryStorageComponent vehicleStorage = EPF_Component<UniversalInventoryStorageComponent>.Find(vehicle);
		if(!vehicleStorage) return;
		
		InventoryStorageManagerComponent vehicleStorageMgr = EPF_Component<InventoryStorageManagerComponent>.Find(vehicle);
		if(!vehicleStorageMgr) return;
		
		m_aLootBodies = new array<IEntity>;
		
		//Query the nearby world for weapons and bodies
		OVT_Global.GetNearbyBodiesAndWeapons(vehicle.GetOrigin(), 25, m_aLootBodies);
			
		if(m_aLootBodies.Count() == 0) return;
		
		GetGame().GetCallqueue().CallLater(LootNextBody, 100);
	}
	
	protected void LootNextBody()
	{
		if(m_aLootBodies.Count() == 0) return;
		RplComponent rplComp = RplComponent.Cast(Replication.FindItem(m_LootVehicle));
		if(!rplComp) return;
		
		IEntity vehicle = rplComp.GetEntity();
		
		UniversalInventoryStorageComponent vehicleStorage = EPF_Component<UniversalInventoryStorageComponent>.Find(vehicle);
		if(!vehicleStorage) return;
		
		InventoryStorageManagerComponent vehicleStorageMgr = EPF_Component<InventoryStorageManagerComponent>.Find(vehicle);
		if(!vehicleStorageMgr) return;
		
		IEntity body = m_aLootBodies[0];
		
		InventoryStorageManagerComponent inv = EPF_Component<InventoryStorageManagerComponent>.Find(body);
		if(!inv) {
			//Might be a weapon
			WeaponComponent weapon = EPF_Component<WeaponComponent>.Find(body);
			if(weapon)
			{
				vehicleStorageMgr.TryInsertItem(body);					
			}
		}else{
			bool allMovesSucceeded = LootBody(inv, vehicleStorage);
			if(allMovesSucceeded) {
				SCR_EntityHelper.DeleteEntityAndChildren(body);
			}
		}
		
		m_aLootBodies.Remove(0);
		
		if(m_aLootBodies.Count() == 0) return;
		
		GetGame().GetCallqueue().CallLater(LootNextBody, 100);
	}

	bool LootBody(InventoryStorageManagerComponent inv, UniversalInventoryStorageComponent vehicleStorage)
	{
		array<IEntity> items = new array<IEntity>;
		inv.GetItems(items);
		if(items.Count() == 0) return false;
		bool allMovesSucceeded = true;
		foreach(IEntity item : items)
		{
			//Ignore clothes (but get helmets, backpacks, etc)
			BaseLoadoutClothComponent cloth = EPF_Component<BaseLoadoutClothComponent>.Find(item);
			if(cloth && cloth.GetAreaType())
			{
				if(cloth.GetAreaType().ClassName() == "LoadoutPantsArea") continue;
				if(cloth.GetAreaType().ClassName() == "LoadoutJacketArea") continue;
				if(cloth.GetAreaType().ClassName() == "LoadoutBootsArea") continue;
			}
			bool couldMove = inv.TryMoveItemToStorage(item, vehicleStorage);
			allMovesSucceeded = allMovesSucceeded && couldMove;
		}
		return allMovesSucceeded;
	}
	
	void DeliverMedicalSupplies(IEntity vehicle)
	{
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		Rpc(RpcAsk_DeliverMedicalSupplies, rpl.Id());
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_DeliverMedicalSupplies(RplId vehicleId)
	{	
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		
		RplComponent rplComp = RplComponent.Cast(Replication.FindItem(vehicleId));
		if(!rplComp) return;
		
		IEntity vehicle = rplComp.GetEntity();
		
		OVT_TownData town = towns.GetNearestTown(vehicle.GetOrigin());
		
		SCR_VehicleInventoryStorageManagerComponent vehicleStorage = SCR_VehicleInventoryStorageManagerComponent.Cast(vehicle.FindComponent(SCR_VehicleInventoryStorageManagerComponent));
		if(!vehicleStorage)
		{
			return;
		}
				
		array<IEntity> items = new array<IEntity>;
		vehicleStorage.GetItems(items);
		if(items.Count() == 0) {
			return;
		}
		
		int cost = 0;
		foreach(IEntity item : items)
		{
			ResourceName res = item.GetPrefabData().GetPrefabName();
			if(!economy.IsSoldAtShop(res, OVT_ShopType.SHOP_DRUG)) continue;
			if(!vehicleStorage.TryDeleteItem(item)){				
				continue;
			}
			cost += economy.GetPriceByResource(res, town.location);
		}
		
		if(cost == 0)
		{
			return;
		}
				
		int townID = OVT_Global.GetTowns().GetTownID(town);
		
		int supportValue = Math.Floor(cost / 10);
		for(int t=0; t<supportValue; t++)
		{
			towns.TryAddSupportModifierByName(townID, "MedicalSupplies");
			towns.TryAddStabilityModifierByName(townID, "MedicalSupplies");
		}
		
		// Play sound
		SimpleSoundComponent simpleSoundComp = SimpleSoundComponent.Cast(vehicle.FindComponent(SimpleSoundComponent));
		if (simpleSoundComp)
		{
			vector mat[4];
			vehicle.GetWorldTransform(mat);
			
			simpleSoundComp.SetTransformation(mat);
			simpleSoundComp.PlayStr("UNLOAD_VEHICLE");
		}	
	}
	
	void SetVehicleLock(IEntity vehicle, bool locked)	
	{	
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));	
		Rpc(RpcAsk_SetVehicleLock, rpl.Id(), locked);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_SetVehicleLock(RplId vehicle, bool locked)	
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(vehicle));
		IEntity entity = rpl.GetEntity();
		OVT_PlayerOwnerComponent playerOwner = EPF_Component<OVT_PlayerOwnerComponent>.Find(entity);
		if(playerOwner) playerOwner.SetLocked(locked);
	}
	
	//REAL ESTATE
	
	void SetHome(int playerId)
	{		
		Rpc(RpcAsk_SetHome, playerId);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetHome(int playerId)
	{	
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		OVT_Global.GetRealEstate().SetHomePos(playerId, player.GetOrigin());
	}
	
	void SetBuildingHome(int playerId, IEntity building)
	{		
		RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
		Rpc(RpcAsk_SetBuildingHome, playerId, rpl.Id());
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetBuildingHome(int playerId, RplId id)
	{	
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
		OVT_Global.GetRealEstate().SetHome(playerId, rpl.GetEntity());
	}
	
	void SetBuildingOwner(int playerId, IEntity building)
	{
		Rpc(RpcAsk_SetBuildingOwner, playerId, building.GetOrigin());
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetBuildingOwner(int playerId, vector pos)
	{	
		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		IEntity building = re.GetNearestBuilding(pos);
		if(!building) return;
		re.SetOwner(playerId, building);
	}
	
	void SetBuildingOwner(string playerPersistentId, IEntity building)
	{
		Rpc(RpcAsk_SetBuildingOwnerPersistent, playerPersistentId, building.GetOrigin());
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetBuildingOwnerPersistent(string playerId, vector pos)
	{	
		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		IEntity building = re.GetNearestBuilding(pos);
		if(!building) return;
		re.SetOwnerPersistentId(playerId, building);
	}
	
	void SetBuildingRenter(int playerId, vector pos)
	{		
		Rpc(RpcAsk_SetBuildingRenter, playerId, pos);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetBuildingRenter(int playerId, vector pos)
	{	
		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		IEntity building = re.GetNearestBuilding(pos, 5);
		if(!building) return;
		OVT_Global.GetRealEstate().SetRenter(playerId, building);
	}
	
	void SetBuildingRenter(string playerId, vector pos)
	{		
		Rpc(RpcAsk_SetBuildingRenterPersistent, playerId, pos);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetBuildingRenterPersistent(string playerId, vector pos)
	{	
		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		IEntity building = re.GetNearestBuilding(pos, 5);
		if(!building) return;
		OVT_Global.GetRealEstate().SetRenterPersistentId(playerId, building);
	}
	
	//SHOPS
	
	void Buy(OVT_ShopComponent shop, int id, int num, int playerId)
	{
		RplComponent rpl = RplComponent.Cast(shop.GetOwner().FindComponent(RplComponent));
		Rpc(RpcAsk_Buy, rpl.Id(), id, num, playerId);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Buy(RplId shopId, int id, int num, int playerId)
	{
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;
		
		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(player.FindComponent( SCR_InventoryStorageManagerComponent ));
		if(!inventory) return;
		
		string playerPersId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		
		int cost = economy.GetBuyPrice(id, player.GetOrigin(),playerId);		
		if(!economy.PlayerHasMoney(playerPersId, cost)) return;
		
		int total = 0;
		int totalnum = 0;
		for(int i = 0; i<num; i++)
		{		
			if(inventory.TrySpawnPrefabToStorage(economy.GetResource(id)))
			{
				total += cost;
				totalnum++;
			}
		}
		if(total > 0)
		{
			Rpc(RpcAsk_TakePlayerMoney, playerId, total);
			Rpc(RpcAsk_TakeFromInventory, shopId, id, totalnum);
			economy.m_OnPlayerBuy.Invoke(playerId, total);
		}
		
	}
	
	void ImportToVehicle(int id, int qty, IEntity vehicle, int playerId)
	{
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if(!rpl) return;
		
		Rpc(RpcAsk_ImportToVehicle, id, qty, rpl.Id(), playerId)	
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_ImportToVehicle(int id, int qty, RplId vehicleId, int playerId)
	{
		IEntity vehicle = RplComponent.Cast(Replication.FindItem(vehicleId)).GetEntity();		
		if(!vehicle) return;
		
		InventoryStorageManagerComponent storage = InventoryStorageManagerComponent.Cast(vehicle.FindComponent(InventoryStorageManagerComponent));				
		if(!storage) return;
		
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		
		int cost = qty * economy.GetPrice(id);
		if(!economy.PlayerHasMoney(persId, cost)) return;
		
		int actual = 0;
		ResourceName res = economy.GetResource(id);
		
		for(int i = 0; i < qty; i++)
		{
			if(storage.TrySpawnPrefabToStorage(res))
			{
				actual++;
			}
		}
		
		economy.DoTakePlayerMoney(playerId, actual * economy.GetPrice(id));
	}
	
	void BuyVehicle(OVT_ShopComponent shop, int id, int playerId)
	{
		RplComponent rpl = RplComponent.Cast(shop.GetOwner().FindComponent(RplComponent));
		Rpc(RpcAsk_BuyVehicle, rpl.Id(), id, playerId);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_BuyVehicle(RplId shopId, int id, int playerId)
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;
		
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(shopId));
		OVT_ShopComponent shop = OVT_ShopComponent.Cast(rpl.GetEntity().FindComponent(OVT_ShopComponent));
		if(!shop) return;
		
		string playerPersId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		
		int cost = economy.GetBuyPrice(id, player.GetOrigin(), playerId);		
		if(shop.m_bProcurement)
		{
			cost = economy.GetPrice(id);
			cost = cost * OVT_Global.GetConfig().m_Difficulty.procurementMultiplier;
		}
		if(!economy.PlayerHasMoney(playerPersId, cost)) return;
		
		//Try to spawn the vehicle in the parking for this shop
		OVT_ParkingComponent parking = EPF_Component<OVT_ParkingComponent>.Find(shop.GetOwner());
		if(parking)
		{
			vector mat[4];
			if(parking.GetParkingSpot(mat, economy.GetParkingType(id)))
			{
				OVT_Global.GetVehicles().SpawnVehicleMatrix(economy.GetResource(id), mat, playerPersId);
				RpcAsk_TakePlayerMoney(playerId, cost);
				if(!shop.m_bProcurement)
					RpcAsk_TakeFromInventory(shopId, id, 1);
				return;
			}			
		}
			
		//Try to spawn the vehicle anywhere nearby	
		if(OVT_Global.GetVehicles().SpawnVehicleNearestParking(economy.GetResource(id), player.GetOrigin(), playerPersId))
		{
			RpcAsk_TakePlayerMoney(playerId, cost);
			if(!shop.m_bProcurement)
				RpcAsk_TakeFromInventory(shopId, id, 1);
		}
	}
	
	void AddToShopInventory(OVT_ShopComponent shop, int id, int num)
	{
		num = Math.Round(num);
		RplComponent rpl = RplComponent.Cast(shop.GetOwner().FindComponent(RplComponent));
		Rpc(RpcAsk_AddToInventory, rpl.Id(), id, num);
	}
		
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddToInventory(RplId shopId, int id, int num)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(shopId));
		OVT_ShopComponent shop = OVT_ShopComponent.Cast(rpl.GetEntity().FindComponent(OVT_ShopComponent));
		
		if(!shop.m_aInventory.Contains(id))
		{
			shop.m_aInventory[id] = 0;
		}		
		shop.m_aInventory[id] = shop.m_aInventory[id] + num;
		shop.StreamInventory(id);
	}
	
	void TakeFromShopInventory(OVT_ShopComponent shop, RplId id, int num)
	{
		RplComponent rpl = RplComponent.Cast(shop.GetOwner().FindComponent(RplComponent));
		Rpc(RpcAsk_TakeFromInventory, rpl.Id(), id, num);
	}	
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TakeFromInventory(RplId shopId, RplId id, int num)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(shopId));
		OVT_ShopComponent shop = OVT_ShopComponent.Cast(rpl.GetEntity().FindComponent(OVT_ShopComponent));
				
		if(!shop.m_aInventory.Contains(id)) return;		
		shop.m_aInventory[id] = shop.m_aInventory[id] - num;		
		if(shop.m_aInventory[id] < 0) shop.m_aInventory[id] = 0;
		shop.StreamInventory(id);
	}
	
	//ECONOMY
	
	void AddPlayerMoney(int playerId, int amount, bool doEvent=false)
	{
		//Stop money glitch
		if(addingMoney) return;
		addingMoney = true;
		Rpc(RpcAsk_AddPlayerMoney, playerId, amount, doEvent);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddPlayerMoney(int playerId, int amount, bool doEvent)
	{
		OVT_Global.GetEconomy().DoAddPlayerMoney(playerId, amount);
		Rpc(RpcDo_DoneAddingMoney);	
		if(doEvent)
		{
			OVT_Global.GetEconomy().m_OnPlayerSell.Invoke(playerId, amount);
		}		
	}
	
	void TakePlayerMoney(int playerId, int amount)
	{
		//Stop money glitch
		if(takingMoney) return;
		takingMoney = true;
		Rpc(RpcAsk_TakePlayerMoney, playerId, amount);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TakePlayerMoney(int playerId, int amount)
	{
		OVT_Global.GetEconomy().DoTakePlayerMoney(playerId, amount);	
		Rpc(RpcDo_DoneTakingMoney);	
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_DoneTakingMoney()
	{
		takingMoney = false;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_DoneAddingMoney()
	{
		addingMoney = false;
	}
	
	void AddResistanceMoney(int amount)
	{
		Rpc(RpcAsk_AddResistanceMoney, amount);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddResistanceMoney(int amount)
	{
		OVT_Global.GetEconomy().DoAddResistanceMoney(amount);		
	}
	
	void SetResistanceTax(float amount)
	{
		Rpc(RpcAsk_SetResistanceTax, amount);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetResistanceTax(float amount)
	{
		OVT_Global.GetEconomy().DoSetResistanceTax(amount);		
	}
	
	void TakeResistanceMoney(int amount)
	{
		Rpc(RpcAsk_TakeResistanceMoney, amount);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TakeResistanceMoney(int amount)
	{
		OVT_Global.GetEconomy().DoTakeResistanceMoney(amount);		
	}
	
	//PLACING
	void PlaceItem(int placeableIndex, int prefabIndex, vector pos, vector angles, int playerId)
	{
		Rpc(RpcAsk_PlaceItem, placeableIndex, prefabIndex, pos, angles, playerId);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_PlaceItem(int placeableIndex, int prefabIndex, vector pos, vector angles, int playerId)
	{
		OVT_Global.GetResistanceFaction().PlaceItem(placeableIndex, prefabIndex, pos, angles, playerId);
	}
	
	//BUILDING
	void BuildItem(int buildableIndex, int prefabIndex, vector pos, vector angles, int playerId)
	{
		Rpc(RpcAsk_BuildItem, buildableIndex, prefabIndex, pos, angles, playerId);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_BuildItem(int buildableIndex, int prefabIndex, vector pos, vector angles, int playerId)
	{
		OVT_Global.GetResistanceFaction().BuildItem(buildableIndex, prefabIndex, pos, angles, playerId);
	}
	
	//BASES
	void AddGarrison(OVT_BaseData base, ResourceName res)
	{
		OVT_Faction faction = OVT_Global.GetConfig().GetPlayerFaction();
		int index = faction.m_aGroupPrefabSlots.Find(res);
		if(index == -1) return;
		Rpc(RpcAsk_AddGarrison, base.id, index);		
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddGarrison(int baseId, int prefabIndex)
	{
		OVT_Global.GetResistanceFaction().AddGarrison(baseId, prefabIndex);
	}
	
	void AddGarrisonFOB(OVT_CampData base, ResourceName res)
	{
		OVT_Faction faction = OVT_Global.GetConfig().GetPlayerFaction();
		int index = faction.m_aGroupPrefabSlots.Find(res);
		if(index == -1) return;
		Rpc(RpcAsk_AddGarrisonFOB, base.location, index);		
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddGarrisonFOB(vector pos, int prefabIndex)
	{
		OVT_ResistanceFactionManager rf = OVT_Global.GetResistanceFaction();
		OVT_CampData fob = rf.GetNearestCampData(pos);
		rf.AddGarrisonFOB(fob, prefabIndex);
	}
	
	//INVENTORY
	void TransferStorage(IEntity from, IEntity to)
	{
		RplComponent fromRpl = RplComponent.Cast(from.FindComponent(RplComponent));
		RplComponent toRpl = RplComponent.Cast(to.FindComponent(RplComponent));
		
		if(!fromRpl || !toRpl) return;
		
		Rpc(RpcAsk_TransferStorage, fromRpl.Id(), toRpl.Id());
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TransferStorage(RplId from, RplId to)
	{
		OVT_Global.TransferStorage(from, to);
	}
	
	void TransferToWarehouse(IEntity from)
	{
		RplComponent fromRpl = RplComponent.Cast(from.FindComponent(RplComponent));
		
		if(!fromRpl) return;
		
		Rpc(RpcAsk_TransferToWarehouse, fromRpl.Id());
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TransferToWarehouse(RplId from)
	{
		OVT_Global.TransferToWarehouse(from);
	}
	
	//VEHICLES
	void DeployFOB(IEntity vehicle)
	{
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		
		Rpc(RpcAsk_DeployFOB, rpl.Id());
	}	
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_DeployFOB(RplId vehicle)
	{
		OVT_Global.GetResistanceFaction().DeployFOB(vehicle);
	}
	
	void UndeployFOB(IEntity vehicle)
	{
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		
		Rpc(RpcAsk_UndeployFOB, rpl.Id());
	}	
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_UndeployFOB(RplId vehicle)
	{
		OVT_Global.GetResistanceFaction().UndeployFOB(vehicle);
	}
	
	void UpgradeVehicle(Vehicle vehicle, OVT_VehicleUpgrade upgrade)
	{
		int id = OVT_Global.GetEconomy().GetInventoryId(upgrade.m_pUpgradePrefab);
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		
		Rpc(RpcAsk_UpgradeVehicle, rpl.Id(), id);
	}	
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_UpgradeVehicle(RplId vehicle, int id)
	{
		OVT_Global.GetVehicles().UpgradeVehicle(vehicle, id);
	}
	
	void RepairVehicle(Vehicle vehicle)
	{		
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		
		Rpc(RpcAsk_RepairVehicle, rpl.Id());
	}	
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RepairVehicle(RplId vehicle)
	{
		OVT_Global.GetVehicles().RepairVehicle(vehicle);
	}
	
	void SpawnGunner(IEntity turret, int playerId = -1)
	{		
		RplComponent rpl = RplComponent.Cast(turret.FindComponent(RplComponent));
		
		Rpc(RpcAsk_SpawnGunner, rpl.Id(), playerId);
	}	
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SpawnGunner(RplId turret, int playerId)
	{
		OVT_Global.GetResistanceFaction().SpawnGunner(turret, playerId);
	}
	
	//WAREHOUSES
	void AddToWarehouse(int warehouseId, string id, int count)
	{
		Rpc(RpcAsk_AddToWarehouse, warehouseId, id, count);
	}	
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddToWarehouse(int warehouseId, string id, int count)
	{
		OVT_Global.GetRealEstate().DoAddToWarehouse(warehouseId, id, count);
	}
	
	void TakeFromWarehouse(int warehouseId, string id, int count)
	{
		Rpc(RpcAsk_TakeFromWarehouse, warehouseId, id, count);
	}	
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TakeFromWarehouse(int warehouseId, string id, int count)
	{
		OVT_Global.GetRealEstate().DoTakeFromWarehouse(warehouseId, id, count);
	}
	
	void TakeFromWarehouseToVehicle(int warehouseId, string id, int qty, IEntity vehicle)	
	{
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		Rpc(RpcAsk_TakeFromWarehouseToVehicle, warehouseId, id, qty, rpl.Id());
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_TakeFromWarehouseToVehicle(int warehouseId, string id, int qty, RplId vehicleId)	
	{
		OVT_Global.TakeFromWarehouseToVehicle(warehouseId, id, qty, vehicleId);
	}
	
	//JOBS
	void AcceptJob(OVT_Job job, int playerId)	
	{		
		Rpc(RpcAsk_AcceptJob, job.jobIndex, job.townId, job.baseId, playerId);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_AcceptJob(int jobIndex, int townId, int baseId, int playerId)	
	{
		OVT_JobManagerComponent jobs = OVT_Global.GetJobs();
		OVT_JobConfig config = jobs.GetConfig(jobIndex);
		foreach(OVT_Job job : jobs.m_aJobs)
		{
			if(job.jobIndex != jobIndex) continue;
			if(config.m_bPublic)
			{
				if(!job.accepted && job.owner == "" && job.townId == townId && job.baseId == baseId) {
					jobs.AcceptJob(job, playerId);
				}
			}else{
				string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
				if(!job.accepted && job.owner == persId && job.townId == townId && job.baseId == baseId) {
					jobs.AcceptJob(job, playerId);
				}
			}
		}
	}
	void DeclineJob(OVT_Job job, int playerId)	
	{		
		Rpc(RpcAsk_DeclineJob, job.jobIndex, job.townId, job.baseId, playerId);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_DeclineJob(int jobIndex, int townId, int baseId, int playerId)	
	{
		OVT_JobManagerComponent jobs = OVT_Global.GetJobs();
		OVT_JobConfig config = jobs.GetConfig(jobIndex);
		foreach(OVT_Job job : jobs.m_aJobs)
		{
			if(job.jobIndex != jobIndex) continue;
			if(config.m_bPublic)
			{
				if(job.townId == townId && job.baseId == baseId) {
					jobs.DeclineJob(job, playerId);
				}
			}else{
				string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
				if(job.owner == persId && job.townId == townId && job.baseId == baseId) {
					jobs.DeclineJob(job, playerId);
				}
			}
		}
	}
	
	void RequestFastTravel(int playerId, vector pos)	
	{		
		Rpc(RpcAsk_RequestFastTravel, playerId, pos);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_RequestFastTravel(int playerId, vector pos)	
	{
		SCR_Global.TeleportPlayer(playerId, pos);
	}	
	
}