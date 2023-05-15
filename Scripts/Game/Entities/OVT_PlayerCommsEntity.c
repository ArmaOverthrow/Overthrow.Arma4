class OVT_PlayerCommsEntityClass: GenericEntityClass
{
};

class OVT_PlayerCommsEntity: GenericEntity
{
	void OVT_PlayerCommsEntity(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT);
	}
	
	override void EOnInit(IEntity owner)
	{
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		mode.m_Server = this;
	}
	
	void RequestSave()
	{
		Rpc(RpcAsk_RequestSave);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_RequestSave()
	{
		SCR_SaveLoadComponent save = SCR_SaveLoadComponent.Cast(GetGame().GetGameMode().FindComponent(SCR_SaveLoadComponent));
		if(save)
		{
			save.Save();
		}
	}
	
	void RegisterPersistentID(string persistentID)
	{
		int int1,int2,int3;
		OVT_PlayerManagerComponent.EncodeIDAsInts(persistentID, int1, int2, int3);
		
		int playerId = SCR_PlayerController.GetLocalPlayerId();
		
		Rpc(RpcAsk_SetID, playerId, int1, int2, int3);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_SetID(int playerId, int id1, int id2, int id3)
	{		
		string persistentID = "" + id1;
		if(id2 > -1) persistentID += id2.ToString();
		if(id3 > -1) persistentID += id3.ToString();
					
		Print("Registering persistent ID with server: " + persistentID);
		OVT_Global.GetPlayers().RegisterPlayer(playerId, persistentID);		
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
		RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
		Rpc(RpcAsk_SetBuildingOwner, playerId, rpl.Id());
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetBuildingOwner(int playerId, RplId id)
	{	
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
		OVT_Global.GetRealEstate().SetOwner(playerId, rpl.GetEntity());
	}
	
	void SetBuildingRenter(int playerId, IEntity building)
	{		
		RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
		Rpc(RpcAsk_SetBuildingRenter, playerId, rpl.Id());
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetBuildingRenter(int playerId, RplId id)
	{	
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
		OVT_Global.GetRealEstate().SetRenter(playerId, rpl.GetEntity());
	}
	
	//SHOPS
	
	void Buy(OVT_ShopComponent shop, int id, int num, int playerId)
	{
		RplComponent rpl = RplComponent.Cast(shop.GetOwner().FindComponent(RplComponent));
		Rpc(RpcAsk_Buy, rpl.Id(), id, num, playerId);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcAsk_Buy(RplId shopId, int id, int num, int playerId)
	{
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;
		
		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(player.FindComponent( SCR_InventoryStorageManagerComponent ));
		if(!inventory) return;
		
		string playerPersId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		
		int cost = economy.GetBuyPrice(id, player.GetOrigin());		
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
		}
		
	}
	
	void ImportToVehicle(int id, int qty, IEntity vehicle, int playerId)
	{
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if(!rpl) return;
		
		Rpc(RpcAsk_ImportToVehicle, id, qty, rpl.Id(), playerId)	
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcAsk_ImportToVehicle(int id, int qty, RplId vehicleId, int playerId)
	{
		IEntity vehicle = RplComponent.Cast(Replication.FindItem(vehicleId)).GetEntity();		
		if(!vehicle) return;
		
		InventoryStorageManagerComponent storage = InventoryStorageManagerComponent.Cast(vehicle.FindComponent(InventoryStorageManagerComponent));				
		if(!storage) return;
		
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		OVT_ShopInventoryItem item = OVT_Global.GetEconomy().GetInventoryItem(id);
		
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		
		int cost = qty * item.cost;
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
		
		economy.DoTakePlayerMoney(playerId, actual * item.cost);
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
		
		string playerPersId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		
		int cost = economy.GetBuyPrice(id, player.GetOrigin());		
		if(!economy.PlayerHasMoney(playerPersId, cost)) return;
		
		if(OVT_Global.GetVehicles().SpawnVehicleNearestParking(economy.GetResource(id), player.GetOrigin(), playerPersId))
		{
			RpcAsk_TakePlayerMoney(playerId, cost);
			RpcAsk_TakeFromInventory(shopId, id, 1);
		}
	}
	
	void AddToShopInventory(OVT_ShopComponent shop, int id, int num)
	{
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
	
	void AddPlayerMoney(int playerId, int amount)
	{
		Rpc(RpcAsk_AddPlayerMoney, playerId, amount);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddPlayerMoney(int playerId, int amount)
	{
		OVT_Global.GetEconomy().DoAddPlayerMoney(playerId, amount);		
	}
	
	void TakePlayerMoney(int playerId, int amount)
	{
		Rpc(RpcAsk_TakePlayerMoney, playerId, amount);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TakePlayerMoney(int playerId, int amount)
	{
		OVT_Global.GetEconomy().DoTakePlayerMoney(playerId, amount);		
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
	
	void AddGarrisonFOB(OVT_FOBData base, ResourceName res)
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
		OVT_FOBData fob = rf.GetNearestFOBData(pos);
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
	protected void RpcAsk_TransferToWarehouse(RplId from, RplId to)
	{
		OVT_Global.TransferToWarehouse(from);
	}
	
	//VEHICLES
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
	void AddToWarehouse(int warehouseId, int id, int count)
	{
		Rpc(RpcAsk_AddToWarehouse, warehouseId, id, count);
	}	
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddToWarehouse(int warehouseId, int id, int count)
	{
		OVT_Global.GetRealEstate().DoAddToWarehouse(warehouseId, id, count);
	}
	
	void TakeFromWarehouse(int warehouseId, int id, int count)
	{
		Rpc(RpcAsk_TakeFromWarehouse, warehouseId, id, count);
	}	
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TakeFromWarehouse(int warehouseId, int id, int count)
	{
		OVT_Global.GetRealEstate().DoTakeFromWarehouse(warehouseId, id, count);
	}
	
	void TakeFromWarehouseToVehicle(int warehouseId, int id, int qty, IEntity vehicle)	
	{
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		Rpc(RpcAsk_TakeFromWarehouseToVehicle, warehouseId, id, qty, rpl.Id());
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_TakeFromWarehouseToVehicle(int warehouseId, int id, int qty, RplId vehicleId)	
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
}