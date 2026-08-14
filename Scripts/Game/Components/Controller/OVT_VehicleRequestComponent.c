[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative vehicle requests for one player")]
class OVT_VehicleRequestComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative vehicle requests, on the per-player OVT_OverthrowController entity.
//!
//! Phase 2 of the controller migration (docs/features/core/controller-migration/implementation.md §4).
//! Replaced six handlers on the legacy comms monolith (deleted in Phase 10): lock/unlock, claim,
//! upgrade, repair, import-to-vehicle and buy-vehicle. Project rule (overthrow-controller.md): every
//! client->server RPC lives on a controller component like this one.
//!
//! WHAT CHANGED IN THE MOVE, AND WHY EACH CHANGE IS NOT A REWRITE:
//!
//! 1. NO REQUEST CARRIES AN IDENTITY ARGUMENT ANY MORE (plan G3/D3). The monolith sat on the caller's
//!    CHARACTER, so it laundered a client-supplied playerId through ResolveSenderPlayerId(). This
//!    component sits on the caller's CONTROLLER ENTITY, so the caller is ResolveOwningPlayerId() and
//!    the parameter is DELETED rather than ignored - an argument that does not exist cannot be trusted
//!    by a future edit.
//!
//! 2. THE CHARACTER-VS-GAME-MODE BRANCHES COLLAPSE. RpcAsk_SetVehicleLock and RpcAsk_ClaimUnownedVehicle
//!    guarded their BUG-087 ownership/proximity checks behind "is GetOwner() a ChimeraCharacter?",
//!    because the monolith ALSO lived on the game-mode prefab where the caller was trusted server-side
//!    code. On the controller the caller is always a player, so the checks are unconditional. That is a
//!    tightening and it is intended: the persistence reservation model's "a locked vehicle stays hidden
//!    for its owner" split is only sound while lock state cannot be set by a non-owner.
//!
//! 3. UPGRADE AND REPAIR GAIN THE VALIDATION THEY NEVER HAD. Both were bare forwards to the manager:
//!    any client could name any RplId on the map and upgrade or repair it from anywhere, for free.
//!    They now carry the ownership / proximity / affordability trio. See RpcAsk_UpgradeVehicle for the
//!    charging story, which is the one genuine behaviour change in this file.
//!
//! 4. BUY-VEHICLE STOPS PAYING WITH AN RPC (plan §3.6(a)). The monolith took the money by calling
//!    Rpc(RpcAsk_TakePlayerMoney, ...) / RpcAsk_TakeFromInventory from INSIDE a server-side handler.
//!    An RplRcver.Server RPC marshalled by the authority is delivered to nobody (the BUG-045/052/088
//!    family), so those were dropped packets dressed as calls. The money and the stock decrement are
//!    now direct manager/shop mutations.
//!
//! WHY THE PUBLIC ENTRY POINTS BRANCH ON Replication.IsServer() WHEN THE MONOLITH'S DID NOT. Same
//! reason as (4), one level up. Every entry point here is reachable from a LISTEN-SERVER HOST, who is
//! the authority: the monolith's unconditional Rpc() was an RplRcver.Server request marshalled by the
//! server, i.e. delivered to nobody, so locking your own car as the host silently did nothing. The
//! direct-call branch is the project's standard fix (OVT_ShopTransactionComponent uses the same shape),
//! not an optimisation.
//------------------------------------------------------------------------------------------------
class OVT_VehicleRequestComponent : OVT_ControllerRequestComponent
{
	//! How far a vehicle may be from the requesting player's character before the server rejects a
	//! lock/unlock, an ownership claim, an upgrade or a repair (vehicle length plus latency/movement
	//! slack). Carried verbatim from the legacy comms monolith, and deliberately the same 15 m
	//! OVT_ShopTransactionComponent applies to the trunk sell.
	protected const float VEHICLE_MAX_DISTANCE = 15;

	//! How far the buying player may be from the shop before the server rejects a vehicle purchase.
	//! The same 30 m buying and selling already use, so no request is refused here that another seam
	//! would have accepted.
	protected const float SHOP_MAX_DISTANCE = 30;

	//! How far the importing player AND the receiving vehicle may be from a port before the server
	//! rejects an import (the port UI requires 20 m, plus latency/movement slack).
	protected const float IMPORT_MAX_PORT_DISTANCE = 30;

	//! Upper bound on one import request. Carried from BUG-033: without it a single click could ask the
	//! server to spawn an unbounded number of prefabs into a car.
	protected const int IMPORT_MAX_QUANTITY = 100;

	//------------------------------------------------------------------------------------------------
	// PUBLIC ENTRY POINTS - client side. Each does the client-side wrapping (null guards, RplId lookup)
	// and then either calls the handler directly (we are the authority) or sends it.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Lock or unlock a vehicle the local player owns.
	//! \param[in] vehicle The vehicle to lock/unlock.
	//! \param[in] locked True to lock.
	void SetVehicleLock(IEntity vehicle, bool locked)
	{
		RplComponent rpl = GetEntityRpl(vehicle);
		if(!rpl) return;

		if(Replication.IsServer())
		{
			RpcAsk_SetVehicleLock(rpl.Id(), locked);
		}else{
			Rpc(RpcAsk_SetVehicleLock, rpl.Id(), locked);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Claim an unowned vehicle the local player has just got into the driver's seat of.
	//! \param[in] vehicle The vehicle to claim.
	void ClaimUnownedVehicle(IEntity vehicle)
	{
		RplComponent rpl = GetEntityRpl(vehicle);
		if(!rpl) return;

		if(Replication.IsServer())
		{
			RpcAsk_ClaimUnownedVehicle(rpl.Id());
		}else{
			Rpc(RpcAsk_ClaimUnownedVehicle, rpl.Id());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Buy a configured upgrade for a vehicle. The upgrade is named by its resource id; the server
	//! re-derives the price from the same config the menu drew the card from.
	//! \param[in] vehicle The vehicle to upgrade.
	//! \param[in] upgrade The chosen upgrade config entry.
	void UpgradeVehicle(Vehicle vehicle, OVT_VehicleUpgrade upgrade)
	{
		if(!vehicle || !upgrade) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		int id = economy.GetInventoryId(upgrade.m_pUpgradePrefab);

		RplComponent rpl = GetEntityRpl(vehicle);
		if(!rpl) return;

		if(Replication.IsServer())
		{
			RpcAsk_UpgradeVehicle(rpl.Id(), id);
		}else{
			Rpc(RpcAsk_UpgradeVehicle, rpl.Id(), id);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Fully repair a vehicle.
	//! \param[in] vehicle The vehicle to repair.
	void RepairVehicle(Vehicle vehicle)
	{
		RplComponent rpl = GetEntityRpl(vehicle);
		if(!rpl) return;

		if(Replication.IsServer())
		{
			RpcAsk_RepairVehicle(rpl.Id());
		}else{
			Rpc(RpcAsk_RepairVehicle, rpl.Id());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Import goods from a port directly into a vehicle's cargo.
	//! \param[in] id Resource id to import.
	//! \param[in] qty How many.
	//! \param[in] vehicle The receiving vehicle.
	void ImportToVehicle(int id, int qty, IEntity vehicle)
	{
		RplComponent rpl = GetEntityRpl(vehicle);
		if(!rpl) return;

		if(Replication.IsServer())
		{
			RpcAsk_ImportToVehicle(id, qty, rpl.Id());
		}else{
			Rpc(RpcAsk_ImportToVehicle, id, qty, rpl.Id());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Buy a vehicle from a vehicle shop or a procurement point.
	//! \param[in] shop The shop being bought from.
	//! \param[in] id Resource id of the vehicle.
	void BuyVehicle(OVT_ShopComponent shop, int id)
	{
		if(!shop) return;

		RplComponent rpl = GetEntityRpl(shop.GetOwner());
		if(!rpl) return;

		if(Replication.IsServer())
		{
			RpcAsk_BuyVehicle(rpl.Id(), id);
		}else{
			Rpc(RpcAsk_BuyVehicle, rpl.Id(), id);
		}
	}

	//------------------------------------------------------------------------------------------------
	// SERVER HANDLERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: set a vehicle's lock state.
	//!
	//! BUG-087, carried verbatim except that the checks are no longer conditional (see the class
	//! header): only the vehicle's recorded owner may change its lock state, and only from nearby.
	//! An UNOWNED vehicle has no owner to authorise the change, so GetPlayerOwnerUid() == "" can never
	//! equal a real caller uid and an unowned car can never be locked - claim it first.
	//!
	//! Rejections are silent here, exactly as before: the lock/unlock user action is broadcast, so
	//! every OTHER player's machine also runs PerformAction and reaches this handler through its own
	//! controller. Those requests are SUPPOSED to be refused, and notifying or logging them would turn
	//! one player's click into a message on everyone else's screen.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetVehicleLock(RplId vehicleId, bool locked)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!character) return;

		IEntity vehicle = ResolveEntity(vehicleId);
		if(!vehicle) return;

		OVT_PlayerOwnerComponent playerOwner = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(vehicle);
		if(!playerOwner) return;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return;

		string callerUid = players.GetPersistentIDFromPlayerID(playerId);
		if(callerUid == "") return;

		if(playerOwner.GetPlayerOwnerUid() != callerUid) return;

		if(vector.Distance(character.GetOrigin(), vehicle.GetOrigin()) > VEHICLE_MAX_DISTANCE) return;

		playerOwner.SetLocked(locked);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: take ownership of a vehicle nobody owns.
	//!
	//! Carried from BUG-087. Ownership never changes once set, so a claim is only ever valid on an
	//! unowned vehicle; and because a claim comes from sitting in the driver's seat, the claimant must
	//! actually be at the vehicle - otherwise a client could claim, and then legitimately lock, every
	//! unowned vehicle on the map. Silent rejection for the same broadcast reason as the lock handler.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_ClaimUnownedVehicle(RplId vehicleId)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!character) return;

		IEntity vehicle = ResolveEntity(vehicleId);
		if(!vehicle) return;

		OVT_PlayerOwnerComponent playerOwner = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(vehicle);
		if(!playerOwner) return;

		string currentOwner = playerOwner.GetPlayerOwnerUid();
		if(currentOwner != "") return;

		if(vector.Distance(character.GetOrigin(), vehicle.GetOrigin()) > VEHICLE_MAX_DISTANCE) return;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return;

		string playerUid = players.GetPersistentIDFromPlayerID(playerId);
		if(playerUid == "") return;

		playerOwner.SetPlayerOwner(playerUid);
		playerOwner.SetLocked(false);

		// Register vehicle for despawn/respawn management
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		if(vehicles && Vehicle.Cast(vehicle))
		{
			vehicles.RegisterPlayerVehicle(playerUid, vehicle);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: replace a vehicle with its upgraded variant, and charge for it.
	//!
	//! WHAT THIS HANDLER USED TO BE: one line, OVT_Global.GetVehicles().UpgradeVehicle(vehicle, id),
	//! with no caller resolution, no ownership test, no proximity test and no price. Every gate lived
	//! in OVT_ManageVehicleContext, i.e. on the client.
	//!
	//! THE CHARGE MOVED SERVER-SIDE, AND IT HAD TO. OVT_VehicleManagerComponent.UpgradeVehicle does not
	//! charge - the MENU did, by calling economy.TakeLocalPlayerMoney() just before sending this
	//! request, which on a client is a second, independent client->server RPC. Leaving that in place and
	//! adding an affordability check here would have made the two race: the debit lands first and the
	//! upgrade is then refused for insufficient funds. So the menu's debit is deleted (its
	//! LocalPlayerHasMoney check stays as advisory UX per plan §3.4) and the money is taken here, in the
	//! same handler that performs the upgrade.
	//!
	//! ResolveUpgradeCost() is doing double duty as the resource validation: an id that is not a
	//! configured upgrade FOR THIS VEHICLE has no price, and is refused. That is strictly stronger than
	//! the IsValidResourceId() the plan asked for - it also stops a client fitting a truck's upgrade to
	//! a motorcycle.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_UpgradeVehicle(RplId vehicleId, int id)
	{
		if(!Replication.IsServer()) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		if(!economy.IsValidResourceId(id)) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!character) return;

		IEntity vehicle = ResolveEntity(vehicleId);
		if(!vehicle) return;

		if(!PlayerMayUseVehicle(playerId, vehicle))
		{
			RejectVehicleRequest(playerId, "upgrade", "the vehicle is locked and owned by somebody else");
			return;
		}

		if(vector.Distance(character.GetOrigin(), vehicle.GetOrigin()) > VEHICLE_MAX_DISTANCE)
		{
			RejectVehicleRequest(playerId, "upgrade", "the player is not at the vehicle");
			return;
		}

		int cost = ResolveUpgradeCost(vehicle, economy.GetResource(id));
		if(cost < 0)
		{
			RejectVehicleRequest(playerId, "upgrade", "that resource is not a configured upgrade for this vehicle");
			return;
		}

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return;

		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if(persId == "") return;

		if(!economy.PlayerHasMoney(persId, cost))
		{
			SendBuyFailureNotification(playerId, "PurchaseFailedInsufficientFunds");
			return;
		}

		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		if(!vehicles) return;

		economy.DoTakePlayerMoney(playerId, cost);
		vehicles.UpgradeVehicle(vehicleId, id);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: fully repair a vehicle.
	//!
	//! Also a bare forward before this phase. Gains the same ownership and proximity gates as the
	//! upgrade path.
	//!
	//! NO AFFORDABILITY CHECK, DELIBERATELY: repair costs nothing today - neither the manager nor the
	//! menu charges for it - so there is no price to check against. Adding one would be a gameplay
	//! change, which this migration is explicitly not allowed to make (plan §2, G6).
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RepairVehicle(RplId vehicleId)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!character) return;

		IEntity vehicle = ResolveEntity(vehicleId);
		if(!vehicle) return;

		if(!PlayerMayUseVehicle(playerId, vehicle))
		{
			RejectVehicleRequest(playerId, "repair", "the vehicle is locked and owned by somebody else");
			return;
		}

		if(vector.Distance(character.GetOrigin(), vehicle.GetOrigin()) > VEHICLE_MAX_DISTANCE)
		{
			RejectVehicleRequest(playerId, "repair", "the player is not at the vehicle");
			return;
		}

		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		if(!vehicles) return;

		vehicles.RepairVehicle(vehicleId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: import goods from a port into a vehicle's cargo.
	//!
	//! BUG-033 and BUG-102, carried verbatim: the port UI's permission, proximity and catalogue gates
	//! are client-side only, so every one of them is re-checked here.
	//!
	//! EVERY REJECTION BELOW TELLS THE PLAYER WHY. A bare return is indistinguishable from a dropped
	//! packet, which is precisely what made the occupying-faction gate read as a broken button
	//! (BUG-102). Do not "tidy" these into silent returns.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_ImportToVehicle(int id, int qty, RplId vehicleId)
	{
		if(!Replication.IsServer()) return;

		if(qty <= 0 || qty > IMPORT_MAX_QUANTITY) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(!config) return;

		if(!economy.IsValidResourceId(id) || economy.IsVehicle(id))
		{
			SendBuyFailureNotification(playerId, "ImportNotAvailable");
			return;
		}

		if(economy.ItemIsFromFaction(id, config.GetOccupyingFactionIndex()))
		{
			SendBuyFailureNotification(playerId, "ImportNotAvailable");
			return;
		}

		OVT_PlayerData player = OVT_PlayerData.Get(playerId);
		if(!player || !player.HasPermission("Import"))
		{
			SendBuyFailureNotification(playerId, "ImportNoPermission");
			return;
		}

		ResourceName res = economy.GetResource(id);
		if(res == "")
		{
			SendBuyFailureNotification(playerId, "ImportNotAvailable");
			return;
		}

		// Items no standard shop stocks are the extended catalogue the port only offers at Trade L5
		if(!economy.IsSoldAtAnyNonVehicleShop(res) && !player.HasPermission("IllegalImports"))
		{
			SendBuyFailureNotification(playerId, "ImportNotAvailable");
			return;
		}

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!character) return;

		if(economy.DistanceToNearestPort(character.GetOrigin()) > IMPORT_MAX_PORT_DISTANCE)
		{
			SendBuyFailureNotification(playerId, "ImportTooFarFromPort");
			return;
		}

		IEntity vehicle = ResolveEntity(vehicleId);
		if(!vehicle) return;

		if(economy.DistanceToNearestPort(vehicle.GetOrigin()) > IMPORT_MAX_PORT_DISTANCE)
		{
			SendBuyFailureNotification(playerId, "ImportTooFarFromPort");
			return;
		}

		InventoryStorageManagerComponent storage = InventoryStorageManagerComponent.Cast(vehicle.FindComponent(InventoryStorageManagerComponent));
		if(!storage) return;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return;

		string persId = players.GetPersistentIDFromPlayerID(playerId);

		int cost = qty * economy.GetPrice(id);
		if(!economy.PlayerHasMoney(persId, cost))
		{
			SendBuyFailureNotification(playerId, "PurchaseFailedInsufficientFunds");
			return;
		}

		int actual = 0;

		for(int i = 0; i < qty; i++)
		{
			if(storage.TrySpawnPrefabToStorage(res))
			{
				actual++;
			}
		}

		// Pay for what actually fitted, not for what was asked for.
		economy.DoTakePlayerMoney(playerId, actual * economy.GetPrice(id));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: buy a vehicle and spawn it at the shop's parking, or at the nearest parking to the buyer.
	//!
	//! Carried: the 30 m shop gate, the procurement price model, the affordability check and the
	//! parking-spot fallback.
	//!
	//! FIXED IN THE MOVE (plan §3.6(a)): the money and the stock decrement were done by calling
	//! RpcAsk_TakePlayerMoney / RpcAsk_TakeFromInventory from inside this server-side handler. Those are
	//! RplRcver.Server handlers, and this IS the server - the vehicle path called them directly rather
	//! than through Rpc() (which is what the item-buy path did, and why buying an item took no money at
	//! all), so it happened to work, but it routed a purchase through two client->server request
	//! endpoints for no reason. Both are now what they always meant: a direct economy debit and a direct
	//! stock mutation with a stream.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_BuyVehicle(RplId shopId, int id)
	{
		if(!Replication.IsServer()) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		if(!economy.IsValidResourceId(id)) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;

		IEntity shopEntity = ResolveEntity(shopId);
		if(!shopEntity) return;

		OVT_ShopComponent shop = OVT_ShopComponent.Cast(shopEntity.FindComponent(OVT_ShopComponent));
		if(!shop || !shop.GetOwner()) return;

		// The shop UI's proximity gate is client-side only - re-check it here
		if(vector.Distance(player.GetOrigin(), shop.GetOwner().GetOrigin()) > SHOP_MAX_DISTANCE) return;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return;

		string playerPersId = players.GetPersistentIDFromPlayerID(playerId);
		if(playerPersId == "") return;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(!config || !config.m_Difficulty) return;

		int cost = economy.GetBuyPrice(id, player.GetOrigin(), playerId);
		if(shop.m_bProcurement)
		{
			cost = economy.GetPrice(id);
			cost = cost * config.m_Difficulty.procurementMultiplier;
			cost = cost * config.m_Difficulty.vehiclePriceMultiplier;
		}

		if(!economy.PlayerHasMoney(playerPersId, cost))
		{
			SendBuyFailureNotification(playerId, "PurchaseFailedInsufficientFunds");
			return;
		}

		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		if(!vehicles) return;

		//Try to spawn the vehicle in the parking for this shop
		OVT_ParkingComponent parking = OVT_ComponentFinder<OVT_ParkingComponent>.Find(shop.GetOwner());
		if(parking)
		{
			vector mat[4];
			if(parking.GetParkingSpot(mat, economy.GetParkingType(id)))
			{
				vehicles.SpawnVehicleMatrix(economy.GetResource(id), mat, playerPersId);
				economy.DoTakePlayerMoney(playerId, cost);
				if(!shop.m_bProcurement)
					TakeFromShopStock(shop, id, 1);
				return;
			}
		}

		//Try to spawn the vehicle anywhere nearby
		if(vehicles.SpawnVehicleNearestParking(economy.GetResource(id), player.GetOrigin(), playerPersId))
		{
			economy.DoTakePlayerMoney(playerId, cost);
			if(!shop.m_bProcurement)
				TakeFromShopStock(shop, id, 1);
		}
	}

	//------------------------------------------------------------------------------------------------
	// HELPERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The configured cost of one upgrade FOR ONE SPECIFIC VEHICLE, read server-side from the same
	//! OVT_ResistanceFactionManager.m_aVehicleUpgrades table the menu drew its cards from.
	//!
	//! The base-prefab match is what makes this an authorisation check as well as a price lookup: an
	//! upgrade only has a cost in the context of the vehicle it is listed under.
	//! \param[in] vehicle The vehicle being upgraded.
	//! \param[in] upgradeRes The upgrade's prefab, resolved from the requested resource id.
	//! \return The cost, or -1 when that upgrade is not offered for that vehicle.
	protected int ResolveUpgradeCost(IEntity vehicle, ResourceName upgradeRes)
	{
		if(!vehicle || upgradeRes == "") return -1;

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if(!resistance || !resistance.m_aVehicleUpgrades) return -1;

		ResourceName baseRes = OVT_Global.GetPrefabName(vehicle);
		if(baseRes == "") return -1;

		foreach(OVT_VehicleUpgrades upgrades : resistance.m_aVehicleUpgrades)
		{
			if(!upgrades || upgrades.m_pBasePrefab != baseRes) continue;
			if(!upgrades.m_aUpgrades) continue;

			foreach(OVT_VehicleUpgrade upgrade : upgrades.m_aUpgrades)
			{
				if(!upgrade) continue;
				if(upgrade.m_pUpgradePrefab == upgradeRes) return upgrade.m_iCost;
			}
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes stock from a shop and broadcasts the changed row.
	//!
	//! Delegates to OVT_ShopComponent.TakeFromInventory, which as of P4 of the controller migration is a
	//! plain server-side mutation plus StreamInventory(). This used to open-code the mutation because
	//! TakeFromInventory forwarded to a client->server ask on the legacy comms component, which is not a
	//! server-side call path (plan §3.6(a)); that is fixed, so there is one definition again.
	//! \param[in] shop The shop to take from.
	//! \param[in] id Resource id.
	//! \param[in] num How many to remove.
	protected void TakeFromShopStock(OVT_ShopComponent shop, int id, int num)
	{
		if(!shop) return;

		shop.TakeFromInventory(id, num);
	}

	//------------------------------------------------------------------------------------------------
	//! Logs a rejected upgrade/repair with its reason.
	//!
	//! Quality bar Q9: a rejected request is never indistinguishable from a dropped packet. These two
	//! paths log rather than notify because the checks are NEW in this phase - a player who has never
	//! seen a message here should not start seeing one for a request the client already gates.
	//! \param[in] playerId The rejected player.
	//! \param[in] request Which request was rejected.
	//! \param[in] reason Why.
	protected void RejectVehicleRequest(int playerId, string request, string reason)
	{
		Print(string.Format("[OVT_VehicleRequestComponent] Rejected %1 request from player %2: %3", request, playerId.ToString(), reason), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Sends one of the existing purchase-failure notifications to a single player.
	//! \param[in] playerId The player to notify.
	//! \param[in] messageTag Notification tag from the notification config.
	protected void SendBuyFailureNotification(int playerId, string messageTag)
	{
		OVT_NotificationManagerComponent notify = OVT_Global.GetNotify();
		if(notify)
		{
			notify.SendTextNotification(messageTag, playerId);
		}
	}
}
