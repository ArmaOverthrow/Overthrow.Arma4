class OVT_PlayerCommsComponentClass: OVT_ComponentClass
{
};

class OVT_PlayerCommsComponent: OVT_Component
{
	bool takingMoney = false;
	
	//! Client-side: fired with (bool success) when a save this client asked for has finished on the
	//! server. Subscribe BEFORE calling RequestSave().
	protected ref ScriptInvoker m_OnSaveResult = new ScriptInvoker();

	//------------------------------------------------------------------------------------------------
	//! \return Invoker fired with (bool success) when a requested save completes on the server.
	ScriptInvoker GetOnSaveResult()
	{
		return m_OnSaveResult;
	}

	void RequestSave()
	{
		Rpc(RpcAsk_RequestSave);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_RequestSave()
	{
		OVT_OverthrowGameMode overthrow = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(!overthrow)
		{
			Rpc(RpcDo_SaveResult, false);
			return;
		}

		OVT_PersistenceManagerComponent persistence = overthrow.GetPersistence();
		if(!persistence)
		{
			Rpc(RpcDo_SaveResult, false);
			return;
		}

		// The save is asynchronous, so the outcome is reported from its completion invoker rather
		// than assumed here (BUG-006: the menu used to claim success unconditionally).
		persistence.GetOnSaveFinished().Remove(OnServerSaveFinished);
		persistence.GetOnSaveFinished().Insert(OnServerSaveFinished);
		persistence.SaveGame();
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side handler for OVT_PersistenceManagerComponent.GetOnSaveFinished().
	//! \param[in] success Whether the save point was actually committed.
	protected void OnServerSaveFinished(bool success)
	{
		OVT_OverthrowGameMode overthrow = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(overthrow)
		{
			OVT_PersistenceManagerComponent persistence = overthrow.GetPersistence();
			if(persistence)
				persistence.GetOnSaveFinished().Remove(OnServerSaveFinished);
		}

		Rpc(RpcDo_SaveResult, success);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SaveResult(bool success)
	{
		m_OnSaveResult.Invoke(success);
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
		if(!of || of.m_bQRFActive) return;

		// Remote callers reach this handler on their own character's component, so the
		// character is the server-side truth — ignore the client-supplied vector for them.
		// On the host this component sits on the game mode entity and loc is trusted.
		ChimeraCharacter character = ChimeraCharacter.Cast(GetOwner());
		if(character)
		{
			CharacterControllerComponent characterController = character.GetCharacterController();
			if(characterController && characterController.IsDead()) return;
			loc = character.GetOrigin();
		}

		OVT_BaseData data = of.GetNearestBase(loc);
		if(!data) return;
		if(!data.IsOccupyingFaction()) return;
		if(character && vector.Distance(character.GetOrigin(), data.location) > OVT_Global.GetConfig().m_Difficulty.baseCloseRange) return;

		OVT_BaseControllerComponent base = of.GetBase(data.entId);
		if(!base) return;
		of.StartBaseQRF(base);
	}
	
	void InstantCaptureBase(vector loc, int playerId)
	{
		Rpc(RpcAsk_InstantCaptureBase, loc, playerId);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_InstantCaptureBase(vector loc, int playerId)
	{
		// Debug cheat (DiagMenu 254). The DiagMenu gate is client-side only, so compile the
		// handler out of release builds — otherwise any modified client can flip any base.
#ifdef WORKBENCH
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		OVT_BaseData data = of.GetNearestBase(loc);
		if(!data) return;
		
		OVT_BaseControllerComponent base = of.GetBase(data.entId);
		
		// Determine the winning faction based on current control
		int winningFactionIndex;
		if (base.IsOccupyingFaction())
		{
			// Currently occupied by enemy, capture for resistance
			winningFactionIndex = OVT_Global.GetConfig().GetPlayerFactionIndex();
		}
		else
		{
			// Currently controlled by resistance, capture for occupying faction
			winningFactionIndex = OVT_Global.GetConfig().GetOccupyingFactionIndex();
		}
		
		// Instantly change base control
		of.ChangeBaseControl(base, winningFactionIndex);
#endif
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
		OVT_PlayerOwnerComponent playerOwner = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(entity);
		if(playerOwner) playerOwner.SetLocked(locked);
	}
	
	void ClaimUnownedVehicle(IEntity vehicle, int playerId)
	{
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		Rpc(RpcAsk_ClaimUnownedVehicle, rpl.Id(), playerId);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_ClaimUnownedVehicle(RplId vehicleId, int playerId)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(vehicleId));
		if (!rpl) return;
		
		IEntity vehicle = rpl.GetEntity();
		if (!vehicle) return;
		
		OVT_PlayerOwnerComponent playerOwner = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(vehicle);
		if (!playerOwner) return;
		
		// Only set owner if vehicle is currently unowned
		string currentOwner = playerOwner.GetPlayerOwnerUid();
		if (currentOwner == "")
		{
			string playerUid = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
			playerOwner.SetPlayerOwner(playerUid);
			playerOwner.SetLocked(false);
			
			// Register vehicle for despawn/respawn management
			OVT_VehicleManagerComponent vehicleManager = OVT_Global.GetVehicles();
			if (vehicleManager && Vehicle.Cast(vehicle))
			{
				vehicleManager.RegisterPlayerVehicle(playerUid, vehicle);
			}
		}
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
	
	void BuyBuilding(int playerId, bool useResistanceFunds)
	{
		Rpc(RpcAsk_BuyBuilding, playerId, useResistanceFunds);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_BuyBuilding(int playerId, bool useResistanceFunds)
	{
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;

		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		IEntity building = re.GetNearestBuilding(player.GetOrigin());
		if(!building) return;

		EntityID entId = building.GetID();
		if(re.IsOwned(entId) || re.IsRented(entId)) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		int cost = re.GetBuyPrice(building);

		if(useResistanceFunds)
		{
			if(!OVT_Global.GetResistanceFaction().IsOfficer(playerId)) return;
			if(!economy.ResistanceHasMoney(cost)) return;
			economy.TakeResistanceMoney(cost);
			re.SetOwnerPersistentId("resistance", building);
		}else{
			string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
			if(!economy.PlayerHasMoney(persId, cost)) return;
			economy.TakePlayerMoneyPersistentId(persId, cost);
			re.SetOwner(playerId, building);
		}
	}

	void SellBuilding(int playerId, bool useResistanceFunds)
	{
		Rpc(RpcAsk_SellBuilding, playerId, useResistanceFunds);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SellBuilding(int playerId, bool useResistanceFunds)
	{
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;

		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		IEntity building = re.GetNearestBuilding(player.GetOrigin());
		if(!building) return;

		EntityID entId = building.GetID();
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		int cost = re.GetBuyPrice(building);

		if(useResistanceFunds)
		{
			if(!OVT_Global.GetResistanceFaction().IsOfficer(playerId)) return;
			if(re.GetOwnerID(building) != "resistance") return;
			economy.AddResistanceMoney(cost);
		}else{
			string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
			if(!re.IsOwner(persId, entId)) return;
			if(re.IsHome(persId, entId)) return;
			if(re.m_mOwned.Contains(persId) && re.m_mOwned[persId].Count() == 1) return;
			economy.AddPlayerMoneyPersistentId(persId, cost);
		}
		re.SetOwner(-1, building);
	}

	void RentBuilding(int playerId, bool useResistanceFunds)
	{
		Rpc(RpcAsk_RentBuilding, playerId, useResistanceFunds);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RentBuilding(int playerId, bool useResistanceFunds)
	{
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;

		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		IEntity building = re.GetNearestBuilding(player.GetOrigin());
		if(!building) return;

		EntityID entId = building.GetID();
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);

		if(useResistanceFunds && !OVT_Global.GetResistanceFaction().IsOfficer(playerId)) return;

		bool isOwner = re.IsOwner(persId, entId);
		if(useResistanceFunds && re.GetOwnerID(building) == "resistance") isOwner = true;

		if(re.IsHome(persId, entId) || re.IsRented(entId) || (re.IsOwned(entId) && !isOwner)) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!isOwner)
		{
			int cost = re.GetRentPrice(building);
			if(useResistanceFunds)
			{
				if(!economy.ResistanceHasMoney(cost)) return;
				economy.TakeResistanceMoney(cost);
			}else{
				if(!economy.PlayerHasMoney(persId, cost)) return;
				economy.TakePlayerMoneyPersistentId(persId, cost);
			}
		}

		if(useResistanceFunds)
		{
			re.SetRenterPersistentId("resistance", building);
		}else{
			re.SetRenter(playerId, building);
		}
	}

	void StopRentingBuilding(int playerId, bool useResistanceFunds)
	{
		Rpc(RpcAsk_StopRentingBuilding, playerId, useResistanceFunds);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_StopRentingBuilding(int playerId, bool useResistanceFunds)
	{
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;

		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		IEntity building = re.GetNearestBuilding(player.GetOrigin());
		if(!building) return;

		EntityID entId = building.GetID();
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);

		bool isRenter = re.IsRenter(persId, entId);
		if(useResistanceFunds)
		{
			if(!OVT_Global.GetResistanceFaction().IsOfficer(playerId)) return;
			if(re.GetRenterID(building) == "resistance") isRenter = true;
		}
		if(!isRenter) return;

		re.SetRenter(-1, building);
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
		
		// Get shop component for pricing
		RplComponent shopRpl = RplComponent.Cast(Replication.FindItem(shopId));
		if(!shopRpl) return;
		OVT_ShopComponent shop = OVT_ShopComponent.Cast(shopRpl.GetEntity().FindComponent(OVT_ShopComponent));
		if(!shop) return;
		
		// Use same cost calculation as client to ensure consistency
		int unitCost = economy.GetShopBuyPrice(id, shop, player.GetOrigin(), playerId);
		int totalCost = unitCost * num;
		if(!economy.PlayerHasMoney(playerPersId, totalCost)) 
		{
			SendBuyFailureNotification(playerId, "PurchaseFailedInsufficientFunds");
			return;
		}
		
		// Check if inventory is completely full before attempting any purchases
		ResourceName itemResource = economy.GetResource(id);
		if(!inventory.CanInsertResource(itemResource, EStoragePurpose.PURPOSE_DEPOSIT))
		{
			// For now, we'll try to proceed anyway - the item might be equippable
			// If it truly can't be handled, the purchase will fail gracefully below
		}
		
		// Attempt to spawn and insert items one by one until inventory is full
		int successfulPurchases = 0;
		
		for(int i = 0; i < num; i++)
		{
			// Try to spawn the item
			IEntity spawnedItem = SpawnItemForPlayer(itemResource, player.GetOrigin());
			if(!spawnedItem)
			{
				// Failed to spawn, stop here
				break;
			}
			
			// Try to insert into player inventory
			bool itemHandled = false;
			if(inventory.TryInsertItem(spawnedItem))
			{
				successfulPurchases++;
				itemHandled = true;
			}
			else
			{
				// If can't insert in inventory, try to equip directly as weapon
				CharacterControllerComponent charController = CharacterControllerComponent.Cast(player.FindComponent(CharacterControllerComponent));
				if(charController)
				{
					// Check if it's a weapon
					BaseWeaponComponent weaponComp = BaseWeaponComponent.Cast(spawnedItem.FindComponent(BaseWeaponComponent));
					if(weaponComp && weaponComp.CanBeEquipped(charController) == ECanBeEquippedResult.OK)
					{
						// Try to equip as weapon
						if(charController.TryEquipRightHandItem(spawnedItem, EEquipItemType.EEquipTypeWeapon))
						{
							successfulPurchases++;
							itemHandled = true;
						}
					}
				}
			}
			
			if(!itemHandled)
			{
				// Failed to insert or equip - clean up and stop
				SCR_EntityHelper.DeleteEntityAndChildren(spawnedItem);
				break;
			}
		}
		
		// Handle results
		if(successfulPurchases > 0)
		{
			// Take money for successful purchases only
			int actualCost = successfulPurchases * unitCost;
			Rpc(RpcAsk_TakePlayerMoney, playerId, actualCost);
			Rpc(RpcAsk_TakeFromInventory, shopId, id, successfulPurchases);
			economy.m_OnPlayerBuy.Invoke(playerId, actualCost);
			
			// Trigger transaction event
			economy.m_OnPlayerTransaction.Invoke(playerId, shop, true, actualCost);
			
			// Notify player only for partial purchases (failures)
			if(successfulPurchases < num)
			{
				SendBuyPartialNotification(playerId, successfulPurchases, num);
			}
		}
		// Complete failure - fail silently
	}
	
	void Sell(OVT_ShopComponent shop, int id, int num, int playerId)
	{
		RplComponent rpl = RplComponent.Cast(shop.GetOwner().FindComponent(RplComponent));
		if(!rpl) return;
		Rpc(RpcAsk_Sell, rpl.Id(), id, num, playerId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Sell(RplId shopId, int id, int num, int playerId)
	{
		if(num <= 0) return;

		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;

		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(player.FindComponent( SCR_InventoryStorageManagerComponent ));
		if(!inventory) return;

		RplComponent shopRpl = RplComponent.Cast(Replication.FindItem(shopId));
		if(!shopRpl) return;
		OVT_ShopComponent shop = OVT_ShopComponent.Cast(shopRpl.GetEntity().FindComponent(OVT_ShopComponent));
		if(!shop) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();

		int unitPrice = economy.GetSellPrice(id, shop.GetOwner().GetOrigin());
		if(shop.m_ShopType == OVT_ShopType.SHOP_GUNDEALER)
		{
			unitPrice = unitPrice * OVT_Global.GetConfig().m_Difficulty.gunDealerSellPriceMultiplier;
		}
		if(unitPrice <= 0) return;

		ResourceName res = economy.GetResource(id);

		autoptr array<IEntity> items = new array<IEntity>;
		inventory.GetItems(items);

		int sold = 0;
		foreach(IEntity ent : items)
		{
			if(sold >= num) break;
			string prefab = ent.GetPrefabData().GetPrefabName();
			//Chris - Make this work better for variants
			if (prefab == "{63E8322E2ADD4AA7}Prefabs/Weapons/Rifles/AK74/Rifle_AK74_GP25.et")
			{
				prefab = "{FA5C25BF66A53DCF}Prefabs/Weapons/Rifles/AK74/Rifle_AK74.et";
			}
			if (prefab == "{EB404DC9E1BCB750}Prefabs/Weapons/Rifles/AK74/Rifle_AK74N_1P29.et" || prefab == "{BC6C9476FB3219A7}Prefabs/Weapons/Rifles/AK74/Rifle_AK74N_GP25.et")
			{
				prefab = "{96DFD2E7E63B3386}Prefabs/Weapons/Rifles/AK74/Rifle_AK74N.et";
			}
			if (res == "{7A82FE978603F137}Prefabs/Weapons/Launchers/RPG7/Launcher_RPG7.et" && prefab == "{E8A55396050E1762}Prefabs/Weapons/Launchers/RPG7/Launcher_RPG7_PGO7.et")
			{
				prefab = res;
			}
			if (res == "{E8A55396050E1762}Prefabs/Weapons/Launchers/RPG7/Launcher_RPG7_PGO7.et" && prefab == "{7A82FE978603F137}Prefabs/Weapons/Launchers/RPG7/Launcher_RPG7.et")
			{
				prefab = res;
			}
			if(prefab == res)
			{
				if(inventory.TryDeleteItem(ent))
				{
					sold++;
				}
			}
		}

		if(sold > 0)
		{
			int total = unitPrice * sold;
			OVT_Global.GetEconomy().DoAddPlayerMoney(playerId, total);
			economy.m_OnPlayerSell.Invoke(playerId, total);
			RpcAsk_AddToInventory(shopId, id, sold);
			economy.m_OnPlayerTransaction.Invoke(playerId, shop, false, total);
		}
	}

	void SellDrugs(int playerId)
	{
		Rpc(RpcAsk_SellDrugs, playerId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SellDrugs(int playerId)
	{
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;

		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(player.FindComponent( SCR_InventoryStorageManagerComponent ));
		if(!inventory) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();

		autoptr array<IEntity> items = new array<IEntity>;
		inventory.GetItems(items);

		foreach(IEntity ent : items)
		{
			ResourceName res = ent.GetPrefabData().GetPrefabName();
			if(!res.Contains("DrugsWeed_01")) continue;
			int id = economy.GetInventoryId(res);
			if(inventory.TryDeleteItem(ent))
			{
				int cost = economy.GetBuyPrice(id, player.GetOrigin()) * 1.25;
				economy.DoAddPlayerMoney(playerId, cost);
			}
			break;
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
			cost = cost * OVT_Global.GetConfig().m_Difficulty.vehiclePriceMultiplier;
		}
		if(!economy.PlayerHasMoney(playerPersId, cost)) return;
		
		//Try to spawn the vehicle in the parking for this shop
		OVT_ParkingComponent parking = OVT_ComponentFinder<OVT_ParkingComponent>.Find(shop.GetOwner());
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
		if(num <= 0) return;
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(shopId));
		if(!rpl) return;
		OVT_ShopComponent shop = OVT_ShopComponent.Cast(rpl.GetEntity().FindComponent(OVT_ShopComponent));
		if(!shop) return;

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
		if(num <= 0) return;
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(shopId));
		if(!rpl) return;
		OVT_ShopComponent shop = OVT_ShopComponent.Cast(rpl.GetEntity().FindComponent(OVT_ShopComponent));
		if(!shop) return;

		if(!shop.m_aInventory.Contains(id)) return;		
		shop.m_aInventory[id] = shop.m_aInventory[id] - num;		
		if(shop.m_aInventory[id] < 0) shop.m_aInventory[id] = 0;
		shop.StreamInventory(id);
	}
	
	//ECONOMY

	//------------------------------------------------------------------------------------------------
	//! Resolves which player this component instance belongs to on the server. Client instances live
	//! on the player's controlled character, so the sender cannot spoof another player's id; the game
	//! mode's own copy (used by server-side code) has no player, so the claimed id is kept there.
	protected int ResolveSenderPlayerId(int claimedPlayerId)
	{
		int ownerId = SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(GetOwner());
		if(ownerId > 0) return ownerId;
		return claimedPlayerId;
	}

	//------------------------------------------------------------------------------------------------
	//! Donates the calling player's own money to the resistance funds.
	//! The balance check, debit and credit all happen on the server.
	void DonateToResistance(int playerId, int amount)
	{
		Rpc(RpcAsk_DonateToResistance, playerId, amount);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_DonateToResistance(int playerId, int amount)
	{
		playerId = ResolveSenderPlayerId(playerId);
		if(amount <= 0) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		if(!economy.PlayerHasMoney(persId, amount)) return;

		economy.DoTakePlayerMoney(playerId, amount);
		economy.DoAddResistanceMoney(amount);
		SendNotification("PlayerDonated", -1, OVT_Global.GetPlayers().GetPlayerName(playerId), amount.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! Sends resistance funds to a player. Officer status, resistance funds and the target player are
	//! all validated on the server, where the debit and credit both happen.
	void SendResistanceFunds(int fromPlayerId, int toPlayerId, int amount)
	{
		Rpc(RpcAsk_SendResistanceFunds, fromPlayerId, toPlayerId, amount);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SendResistanceFunds(int fromPlayerId, int toPlayerId, int amount)
	{
		fromPlayerId = ResolveSenderPlayerId(fromPlayerId);
		if(amount <= 0) return;
		if(!OVT_Global.GetResistanceFaction().IsOfficer(fromPlayerId)) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy.ResistanceHasMoney(amount)) return;

		string toPersId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(toPlayerId);
		if(!OVT_Global.GetPlayers().GetPlayer(toPersId)) return;

		economy.DoTakeResistanceMoney(amount);
		economy.DoAddPlayerMoney(toPlayerId, amount);
		SendNotification("PlayerSentFunds", toPlayerId, amount.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! Sends money from one player to another. The sender is resolved server-side and their balance
	//! checked there, where the debit and credit both happen.
	void SendMoneyToPlayer(int fromPlayerId, int toPlayerId, int amount)
	{
		Rpc(RpcAsk_SendMoneyToPlayer, fromPlayerId, toPlayerId, amount);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SendMoneyToPlayer(int fromPlayerId, int toPlayerId, int amount)
	{
		fromPlayerId = ResolveSenderPlayerId(fromPlayerId);
		if(amount <= 0) return;
		if(fromPlayerId == toPlayerId) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		string fromPersId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(fromPlayerId);
		if(!economy.PlayerHasMoney(fromPersId, amount)) return;

		string toPersId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(toPlayerId);
		if(!OVT_Global.GetPlayers().GetPlayer(toPersId)) return;

		economy.DoTakePlayerMoney(fromPlayerId, amount);
		economy.DoAddPlayerMoney(toPlayerId, amount);
		SendNotification("PlayerSentMoney", toPlayerId, OVT_Global.GetPlayers().GetPlayerName(fromPlayerId), amount.ToString());
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
	
	void SetResistanceTax(float amount)
	{
		Rpc(RpcAsk_SetResistanceTax, amount);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetResistanceTax(float amount)
	{
		OVT_Global.GetEconomy().DoSetResistanceTax(amount);		
	}
	
	//PLACING
	void PlaceItem(int placeableIndex, int prefabIndex, vector pos, vector angles, int playerId)
	{
		Rpc(RpcAsk_PlaceItem, placeableIndex, prefabIndex, pos, angles, playerId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_PlaceItem(int placeableIndex, int prefabIndex, vector pos, vector angles, int playerId)
	{
		playerId = ResolveSenderPlayerId(playerId);
		OVT_Global.GetResistanceFaction().PlaceItem(placeableIndex, prefabIndex, pos, angles, playerId);
	}

	void RemovePlacedItem(RplId entityId, int playerId)
	{
		Rpc(RpcAsk_RemovePlacedItem, entityId, playerId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RemovePlacedItem(RplId entityId, int playerId)
	{
		playerId = ResolveSenderPlayerId(playerId);
		OVT_Global.GetResistanceFaction().RemovePlacedItem(entityId, playerId);
	}

	//BUILDING
	void BuildItem(int buildableIndex, int prefabIndex, vector pos, vector angles, int playerId)
	{
		Rpc(RpcAsk_BuildItem, buildableIndex, prefabIndex, pos, angles, playerId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_BuildItem(int buildableIndex, int prefabIndex, vector pos, vector angles, int playerId)
	{
		playerId = ResolveSenderPlayerId(playerId);
		OVT_Global.GetResistanceFaction().BuildItem(buildableIndex, prefabIndex, pos, angles, playerId);
	}
	
	//OFFICERS
	//------------------------------------------------------------------------------------------------
	//! Promotes a player to officer. The promoting player is resolved server-side and must be an
	//! officer themselves; the promotion is applied and broadcast by the server so every client and
	//! the promotee's persisted record learn about it.
	void AddOfficer(int playerId)
	{
		Rpc(RpcAsk_AddOfficer, playerId, SCR_PlayerController.GetLocalPlayerId());
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddOfficer(int playerId, int promoterId)
	{
		promoterId = ResolveSenderPlayerId(promoterId);

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if(!resistance.IsOfficer(promoterId)) return;
		if(resistance.IsOfficer(playerId)) return;

		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		if(!OVT_Global.GetPlayers().GetPlayer(persId)) return;

		resistance.AddOfficer(playerId);
	}

	//BASES
	void AddGarrison(OVT_BaseData base, ResourceName res)
	{
		OVT_Faction faction = OVT_Global.GetConfig().GetPlayerFaction();
		int index = faction.m_aGroupPrefabSlots.Find(res);
		if(index == -1) return;
		Rpc(RpcAsk_AddGarrison, base.id, index, SCR_PlayerController.GetLocalPlayerId());
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddGarrison(int baseId, int prefabIndex, int playerId)
	{
		playerId = ResolveSenderPlayerId(playerId);
		OVT_Global.GetResistanceFaction().AddGarrison(baseId, prefabIndex, true, playerId);
	}

	void AddGarrisonCamp(OVT_CampData base, ResourceName res)
	{
		OVT_Faction faction = OVT_Global.GetConfig().GetPlayerFaction();
		int index = faction.m_aGroupPrefabSlots.Find(res);
		if(index == -1) return;
		Rpc(RpcAsk_AddGarrisonCamp, base.location, index, SCR_PlayerController.GetLocalPlayerId());
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddGarrisonCamp(vector pos, int prefabIndex, int playerId)
	{
		playerId = ResolveSenderPlayerId(playerId);
		OVT_ResistanceFactionManager rf = OVT_Global.GetResistanceFaction();
		OVT_CampData fob = rf.GetNearestCampData(pos);
		// The registry may be empty (null) and the claimed position is client-supplied - only
		// accept a purchase made at the camp itself
		if(!fob || vector.Distance(fob.location, pos) > 50) return;
		rf.AddGarrisonCamp(fob, prefabIndex, true, playerId);
	}

	void AddGarrisonFOB(OVT_FOBData base, ResourceName res)
	{
		OVT_Faction faction = OVT_Global.GetConfig().GetPlayerFaction();
		int index = faction.m_aGroupPrefabSlots.Find(res);
		if(index == -1) return;
		Rpc(RpcAsk_AddGarrisonFOB, base.location, index, SCR_PlayerController.GetLocalPlayerId());
	}
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddGarrisonFOB(vector pos, int prefabIndex, int playerId)
	{
		playerId = ResolveSenderPlayerId(playerId);
		OVT_ResistanceFactionManager rf = OVT_Global.GetResistanceFaction();
		OVT_FOBData fob = rf.GetNearestFOBData(pos);
		if(!fob || vector.Distance(fob.location, pos) > 50) return;
		rf.AddGarrisonFOB(fob, prefabIndex, true, playerId);
	}
	
	//VEHICLES
	void DeployFOB(IEntity vehicle)
	{
		if(!vehicle) return;
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		
		// Get the local player ID to pass to the server
		IEntity playerEntity = SCR_PlayerController.GetLocalControlledEntity();
		int playerId = SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(playerEntity);
		
		Rpc(RpcAsk_DeployFOB, rpl.Id(), playerId);
	}	
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_DeployFOB(RplId vehicle, int playerId)
	{
		OVT_Global.GetResistanceFaction().DeployFOB(vehicle, playerId);
	}
	
	void UndeployFOB(IEntity vehicle)
	{
		if(!vehicle) return;
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		
		// Get the local player ID to pass to the server
		IEntity playerEntity = SCR_PlayerController.GetLocalControlledEntity();
		int playerId = SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(playerEntity);
		
		Rpc(RpcAsk_UndeployFOB, rpl.Id(), playerId);
	}	
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_UndeployFOB(RplId vehicle, int playerId)
	{
		OVT_Global.GetResistanceFaction().UndeployFOB(vehicle, playerId);
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
		
	void RecruitCivilian(IEntity civilian, int playerId = -1)
	{		
		RplComponent rpl = RplComponent.Cast(civilian.FindComponent(RplComponent));
		
		Rpc(RpcAsk_RecruitCivilian, rpl.Id(), playerId);
	}	
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RecruitCivilian(RplId civilian, int playerId)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(civilian));
		if (!rpl) return;
		
		IEntity civilianEntity = rpl.GetEntity();
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(civilianEntity);
		if (!character) return;
		
		OVT_RecruitManagerComponent recruitManager = OVT_RecruitManagerComponent.GetInstance();
		if (!recruitManager) return;
		
		recruitManager.RecruitCivilian(character, playerId);
	}
	
	void RecruitFromTent(vector tentPos, int playerId)
	{		
		Rpc(RpcAsk_RecruitFromTent, tentPos, playerId);
	}	
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RecruitFromTent(vector tentPos, int playerId)
	{
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		OVT_RecruitManagerComponent recruitManager = OVT_RecruitManagerComponent.GetInstance();
		
		if (!townManager || !recruitManager) return;
		
		// Take supporters from nearest town (1 supporter per recruit)
		townManager.TakeSupportersFromNearestTown(tentPos, 1);
		
		// Spawn recruit at tent location
		SCR_ChimeraCharacter recruit = recruitManager.SpawnRecruit(tentPos + "2 0 2"); // Offset from tent
		if (recruit)
		{
			// Add to recruit manager
			recruitManager.RecruitCivilian(recruit, playerId);
		}
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
		if(qty <= 0) return;
		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		if(warehouseId < 0 || warehouseId >= re.m_aWarehouses.Count()) return;
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
	
	void RequestFastTravelWithRecruits(int playerId, vector pos, float recruitRadius)	
	{		
		Rpc(RpcAsk_RequestFastTravelWithRecruits, playerId, pos, recruitRadius);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_RequestFastTravelWithRecruits(int playerId, vector pos, float recruitRadius)	
	{
		// Get player's persistent ID
		string playerPersistentId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		if (playerPersistentId.IsEmpty())
			return;
		
		// Get player entity for position reference
		IEntity playerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!playerEntity)
			return;
		
		// Teleport player first
		SCR_Global.TeleportPlayer(playerId, pos);
		
		// Get nearby recruits
		OVT_RecruitManagerComponent recruitManager = OVT_RecruitManagerComponent.GetInstance();
		if (!recruitManager)
			return;
			
		array<IEntity> nearbyRecruits = recruitManager.GetPlayerRecruitEntitiesInRadius(playerPersistentId, playerEntity.GetOrigin(), recruitRadius);
		
		// Teleport each nearby recruit to the same destination
		int recruitIndex = 0;
		foreach (IEntity recruitEntity : nearbyRecruits)
		{
			if (!recruitEntity)
				continue;
				
			// Calculate offset position in a circle around the player destination
			float angle = (recruitIndex * 360.0 / nearbyRecruits.Count()) * Math.DEG2RAD;
			float radius = 3.0 + (recruitIndex * 0.5); // Start at 3m and expand outward
			vector offset = Vector(Math.Sin(angle) * radius, 0, Math.Cos(angle) * radius);
			vector recruitPos = pos + offset;
			
			// Find a safe position near the calculated spot
			recruitPos = OVT_Global.FindSafeSpawnPosition(recruitPos);
			
			// Teleport the recruit
			recruitEntity.SetOrigin(recruitPos);
			recruitIndex++;
		}
	}
	
	//LOADOUTS
	
	//! Save a loadout for a player
	void SaveLoadout(string playerId, string loadoutName, string description = "", bool isOfficerTemplate = false)
	{
		Rpc(RpcAsk_SaveLoadout, playerId, loadoutName, description, isOfficerTemplate);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SaveLoadout(string playerId, string loadoutName, string description, bool isOfficerTemplate)
	{
		// Get the player entity
		OVT_PlayerManagerComponent playerMgr = OVT_Global.GetPlayers();
		int playerIdInt = playerMgr.GetPlayerIDFromPersistentID(playerId);
		IEntity playerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerIdInt);
		
		if (!playerEntity)
		{
			Print(string.Format("[OVT_PlayerCommsComponent] Could not find player entity for ID: %1", playerId), LogLevel.ERROR);
			return;
		}
		
		// Get loadout manager
		OVT_LoadoutManagerComponent loadoutManager = OVT_Global.GetLoadouts();
		if (!loadoutManager)
		{
			Print("[OVT_PlayerCommsComponent] Loadout manager not available", LogLevel.ERROR);
			return;
		}
		
		// Save the loadout
		if (isOfficerTemplate)
		{
			loadoutManager.SaveOfficerTemplate(playerId, loadoutName, playerEntity, description);
		}
		else
		{
			loadoutManager.SaveLoadout(playerId, loadoutName, playerEntity, description);
		}
	}
	
	//! Load a loadout for a player
	void LoadLoadout(string playerId, string loadoutName)
	{
		Rpc(RpcAsk_LoadLoadout, playerId, loadoutName);
	}
	
	//! Load a loadout for a player from equipment box
	void LoadLoadoutFromBox(string playerId, string loadoutName, IEntity equipmentBox, IEntity targetEntity)
	{		
		RplComponent equipmentBoxRpl = RplComponent.Cast(equipmentBox.FindComponent(RplComponent));
		RplComponent targetEntityRpl = RplComponent.Cast(targetEntity.FindComponent(RplComponent));
		
		if (!equipmentBoxRpl || !targetEntityRpl)
		{
			Print(string.Format("[OVT_PlayerCommsComponent] Could not get RplComponent - EquipmentBox: %1, TargetEntity: %2", 
				!equipmentBoxRpl, !targetEntityRpl), LogLevel.ERROR);
			return;
		}
		
		Rpc(RpcAsk_LoadLoadoutFromBox, playerId, loadoutName, equipmentBoxRpl.Id(), targetEntityRpl.Id());
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LoadLoadout(string playerId, string loadoutName)
	{
		// Get the player entity
		OVT_PlayerManagerComponent playerMgr = OVT_Global.GetPlayers();
		int playerIdInt = playerMgr.GetPlayerIDFromPersistentID(playerId);
		IEntity playerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerIdInt);
		
		if (!playerEntity)
		{
			Print(string.Format("[OVT_PlayerCommsComponent] Could not find player entity for ID: %1", playerId), LogLevel.ERROR);
			return;
		}
		
		// Get loadout manager
		OVT_LoadoutManagerComponent loadoutManager = OVT_Global.GetLoadouts();
		if (!loadoutManager)
		{
			Print("[OVT_PlayerCommsComponent] Loadout manager not available", LogLevel.ERROR);
			return;
		}
		
		// Load the loadout
		loadoutManager.LoadLoadout(playerId, loadoutName, playerEntity);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LoadLoadoutFromBox(string playerId, string loadoutName, RplId equipmentBoxId, RplId targetEntityId)
	{
		
		// Get equipment box entity
		RplComponent equipmentBoxRpl = RplComponent.Cast(Replication.FindItem(equipmentBoxId));
		if (!equipmentBoxRpl)
		{
			Print(string.Format("[OVT_PlayerCommsComponent] Could not find equipment box with RplId: %1", equipmentBoxId), LogLevel.ERROR);
			return;
		}
		IEntity equipmentBox = equipmentBoxRpl.GetEntity();
		
		// Get target entity
		RplComponent targetEntityRpl = RplComponent.Cast(Replication.FindItem(targetEntityId));
		if (!targetEntityRpl)
		{
			Print(string.Format("[OVT_PlayerCommsComponent] Could not find target entity with RplId: %1", targetEntityId), LogLevel.ERROR);
			return;
		}
		IEntity targetEntity = targetEntityRpl.GetEntity();
		
		// Get loadout manager
		OVT_LoadoutManagerComponent loadoutManager = OVT_Global.GetLoadouts();
		if (!loadoutManager)
		{
			Print("[OVT_PlayerCommsComponent] Loadout manager not available", LogLevel.ERROR);
			return;
		}
		
		// Load the loadout from equipment box
		loadoutManager.LoadLoadout(playerId, loadoutName, targetEntity, equipmentBox);
	}
	
	//! Delete a loadout (client to server)
	void DeleteLoadout(string playerId, string loadoutName, bool isOfficerTemplate = false)
	{
		Rpc(RpcAsk_DeleteLoadout, playerId, loadoutName, isOfficerTemplate);
	}
	
	//! Server-side RPC handler for deleting loadouts
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_DeleteLoadout(string playerId, string loadoutName, bool isOfficerTemplate)
	{
		OVT_LoadoutManagerComponent loadoutManager = OVT_Global.GetLoadouts();
		if (!loadoutManager)
		{
			Print("[OVT_PlayerCommsComponent] LoadoutManager not found", LogLevel.ERROR);
			return;
		}
		
		// Delete the loadout
		loadoutManager.DeleteLoadout(playerId, loadoutName, isOfficerTemplate);
	}
	
	//! Set possessed entity on server and notify client to open inventory
	void SetPossessedEntityAndOpenInventory(int playerId, IEntity targetEntity)
	{
		RplComponent rpl = RplComponent.Cast(targetEntity.FindComponent(RplComponent));
		if (!rpl)
		{
			Print("[OVT_PlayerCommsComponent] Target entity has no RplComponent", LogLevel.ERROR);
			return;
		}
		
		Rpc(RpcAsk_SetPossessedEntityAndOpenInventory, playerId, rpl.Id());
	}
	
	//! Server-side RPC handler for setting possessed entity and notifying client
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetPossessedEntityAndOpenInventory(int playerId, RplId targetEntityId)
	{
		// Get the target entity from RplId
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(targetEntityId));
		if (!rpl)
		{
			Print("[OVT_PlayerCommsComponent] Could not find target entity", LogLevel.ERROR);
			return;
		}
		
		IEntity targetEntity = rpl.GetEntity();
		if (!targetEntity)
		{
			Print("[OVT_PlayerCommsComponent] Target entity is null", LogLevel.ERROR);
			return;
		}
		
		// Get player controller
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!playerController)
		{
			Print("[OVT_PlayerCommsComponent] Player controller not found", LogLevel.ERROR);
			return;
		}
		
		// Set possessed entity on server
		playerController.SetPossessedEntity(targetEntity);
		
		// Notify the specific client to open inventory
		RplId playerControllerId = Replication.FindId(playerController);
		RpcDo_OpenInventory(targetEntityId, playerId, playerControllerId);
		Rpc(RpcDo_OpenInventory, targetEntityId, playerId, playerControllerId);
	}
	
	//! Client-side RPC handler to open inventory
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_OpenInventory(RplId targetEntityId, int playerId, RplId playerControllerId)
	{
		// Check if this is for the local player first
		int localPlayerId = SCR_PlayerController.GetLocalPlayerId();
		if (localPlayerId != playerId)
			return;
						
		// Get the target entity from RplId
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(targetEntityId));
		if (!rpl)
		{
			Print("[OVT_PlayerCommsComponent] Client: Could not find target entity", LogLevel.ERROR);
			return;
		}
		
		IEntity targetEntity = rpl.GetEntity();
		if (!targetEntity)
		{
			Print("[OVT_PlayerCommsComponent] Client: Target entity is null", LogLevel.ERROR);
			return;
		}
		
		// Open inventory on client
		SCR_InventoryStorageManagerComponent inventoryManager = SCR_InventoryStorageManagerComponent.Cast(
			targetEntity.FindComponent(SCR_InventoryStorageManagerComponent)
		);
		
		if (inventoryManager)
		{
			// Set up close listener on the client side
			inventoryManager.m_OnInventoryOpenInvoker.Insert(OnClientInventoryStateChanged);
			inventoryManager.OpenInventory();
		}
		else
		{
			Print("[OVT_PlayerCommsComponent] Client: No inventory manager found", LogLevel.ERROR);
		}
	}
	
	//! Client-side inventory state change handler
	protected void OnClientInventoryStateChanged(bool isOpen)
	{
		Print(string.Format("[OVT_PlayerCommsComponent] Client: Inventory state changed - isOpen: %1", isOpen), LogLevel.NORMAL);
		
		// When inventory closes on client, notify server to restore possession
		if (!isOpen)
		{
			Print("[OVT_PlayerCommsComponent] Client: Inventory closed, requesting possession restore", LogLevel.NORMAL);
			
			// Get the player controller which maintains authority
			SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
			if (playerController)
			{
				// Use the player controller's method which can send RPCs even when possessed
				playerController.RequestRestorePossession();
			}
			else
			{
				Print("[OVT_PlayerCommsComponent] Client: Could not get player controller", LogLevel.ERROR);
			}
		}
	}
	
	//! Restore possessed entity on server and notify client inventory closed
	void RestorePossessedEntity(int playerId)
	{
		Rpc(RpcAsk_RestorePossessedEntity, playerId);
	}
	
	//! Server-side RPC handler for restoring possessed entity
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RestorePossessedEntity(int playerId)
	{
		Print(string.Format("[OVT_PlayerCommsComponent] Server: Restoring possession for player %1", playerId), LogLevel.NORMAL);
		
		// Get player controller
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!playerController)
		{
			Print("[OVT_PlayerCommsComponent] Player controller not found for restore", LogLevel.ERROR);
			return;
		}
		
		IEntity currentPossessed = playerController.GetControlledEntity();
		Print(string.Format("[OVT_PlayerCommsComponent] Current possessed entity: %1", currentPossessed), LogLevel.NORMAL);
		
		// Restore possession to null (back to original entity)
		playerController.SetPossessedEntity(null);
		
		IEntity restoredEntity = playerController.GetControlledEntity();
		Print(string.Format("[OVT_PlayerCommsComponent] Restored to entity: %1", restoredEntity), LogLevel.NORMAL);
	}
	
	//! Request recruit dismissal from client
	void DismissRecruit(string recruitId)
	{
		Rpc(RpcAsk_DismissRecruit, recruitId);
	}
	
	//! Server-side RPC handler for recruit dismissal requests
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_DismissRecruit(string recruitId)
	{
		// Get the recruit manager
		OVT_RecruitManagerComponent recruitManager = OVT_RecruitManagerComponent.GetInstance();
		if (!recruitManager)
		{
			Print("[OVT_PlayerCommsComponent] Server: Recruit manager not found", LogLevel.ERROR);
			return;
		}
		
		// Validate the recruit exists
		OVT_RecruitData recruit = recruitManager.GetRecruit(recruitId);
		if (!recruit)
		{
			Print("[OVT_PlayerCommsComponent] Server: Recruit not found for dismissal: " + recruitId, LogLevel.ERROR);
			return;
		}
		
		// Find and delete the recruit entity on server
		IEntity recruitEntity = recruitManager.FindRecruitEntity(recruitId);
		if (recruitEntity)
		{
			// Remove from group first
			AIControlComponent aiControl = AIControlComponent.Cast(recruitEntity.FindComponent(AIControlComponent));
			if (aiControl)
			{
				AIAgent agent = aiControl.GetAIAgent();
				if (agent && agent.GetParentGroup())
				{
					agent.GetParentGroup().RemoveAgent(agent);
				}
			}
			
			// Delete entity on server
			SCR_EntityHelper.DeleteEntityAndChildren(recruitEntity);
		}
		
		// Remove from manager (this will broadcast to all clients)
		recruitManager.RemoveRecruit(recruitId);
		
		Print("[OVT_PlayerCommsComponent] Server: Dismissed recruit: " + recruitId, LogLevel.NORMAL);
	}
	
	void SetCampPrivacy(OVT_CampData camp, bool isPrivate)
	{
		Rpc(RpcAsk_SetCampPrivacy, camp.location, isPrivate);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_SetCampPrivacy(vector pos, bool isPrivate)
	{
		OVT_ResistanceFactionManager rf = OVT_Global.GetResistanceFaction();
		rf.SetCampPrivacy(pos, isPrivate);
	}
	
	void DeleteCamp(OVT_CampData camp)
	{
		// Store camp location for callback
		m_vDeleteCampLocation = camp.location;
		
		// Find the camp entity to get its RplId
		BaseWorld world = GetGame().GetWorld();
		world.QueryEntitiesBySphere(camp.location, 10, null, FindCampEntityCallback, EQueryEntitiesFlags.ALL);
	}
	
	protected vector m_vDeleteCampLocation;
	
	protected bool FindCampEntityCallback(IEntity entity)
	{
		if (!entity) 
			return false;
		
		// Check if this is a camp entity by looking for the manage camp action
		ActionsManagerComponent actionsManager = ActionsManagerComponent.Cast(entity.FindComponent(ActionsManagerComponent));
		if (actionsManager)
		{
			array<BaseUserAction> actions = {};
			actionsManager.GetActionsList(actions);
			foreach (BaseUserAction action : actions)
			{
				if (OVT_ManageCampAction.Cast(action))
				{
					RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
					if (rpl)
					{
						Rpc(RpcAsk_DeleteCamp, rpl.Id(), m_vDeleteCampLocation);
						return true; // Found the camp, stop searching
					}
				}
			}
		}
		
		return false; // Continue searching
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_DeleteCamp(RplId campEntityId, vector pos)
	{
		OVT_ResistanceFactionManager rf = OVT_Global.GetResistanceFaction();
		rf.RemoveCamp(campEntityId, pos);
	}
	
	void SetPriorityFOB(IEntity fobEntity)
	{
		if (!fobEntity) return;
		RplComponent rpl = RplComponent.Cast(fobEntity.FindComponent(RplComponent));
		if (!rpl) return;
		
		Rpc(RpcAsk_SetPriorityFOB, rpl.Id());
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_SetPriorityFOB(RplId fobEntityId)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(fobEntityId));
		if (!rpl) return;
		IEntity fobEntity = rpl.GetEntity();
		if (!fobEntity) return;
		
		OVT_ResistanceFactionManager rf = OVT_Global.GetResistanceFaction();
		rf.SetPriorityFOB(fobEntity);
	}
	
	//! Helper methods for item purchasing
	
	//! Spawn an item for the player
	protected IEntity SpawnItemForPlayer(ResourceName itemResource, vector location)
	{
		if (itemResource.IsEmpty()) return null;
		
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = location;
		
		Resource resource = Resource.Load(itemResource);
		if (!resource) 
		{
			Print(string.Format("[OVT_PlayerCommsComponent] Failed to load resource: %1", itemResource), LogLevel.WARNING);
			return null;
		}
		
		IEntity spawnedItem = GetGame().SpawnEntityPrefab(resource, null, params);
		return spawnedItem;
	}
	
	//! Send failure notification to player
	protected void SendBuyFailureNotification(int playerId, string messageTag)
	{
		OVT_NotificationManagerComponent notificationManager = OVT_Global.GetNotify();
		if (notificationManager)
		{
			notificationManager.SendTextNotification(messageTag, playerId);
		}
	}
	
	//! Send partial success notification to player
	protected void SendBuyPartialNotification(int playerId, int successCount, int totalRequested)
	{
		OVT_NotificationManagerComponent notificationManager = OVT_Global.GetNotify();
		if (notificationManager)
		{
			notificationManager.SendTextNotification("PurchasePartialSuccess", playerId, successCount.ToString(), totalRequested.ToString());
		}
	}
}