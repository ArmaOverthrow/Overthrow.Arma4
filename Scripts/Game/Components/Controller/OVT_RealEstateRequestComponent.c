[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative real estate and warehouse requests for one player")]
class OVT_RealEstateRequestComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative real estate and warehouse requests, on the per-player OVT_OverthrowController.
//!
//! Phase 3 of the controller migration (docs/features/core/controller-migration/implementation.md §4).
//! Replaced eight handlers on the legacy comms monolith (deleted in Phase 10): set-home, buy/sell/rent/
//! stop-renting a building, and the three warehouse movements. Project rule (overthrow-controller.md):
//! every client->server RPC lives on a controller component like this one.
//!
//! WHY BUILDING REQUESTS CARRY NO BUILDING ARGUMENT. They never did, and that is the good part of the
//! original design: the server resolves the target with re.GetNearestBuilding(caller's origin), so the
//! caller can only ever act on the building they are standing at. The menu resolves the same building
//! locally for its labels and its advisory checks, but the authority never trusts that answer. Carried
//! unchanged - dropping only the client-supplied playerId (plan G3/D3).
//!
//! WHAT CHANGED IN THE MOVE:
//!
//! 1. NO REQUEST CARRIES AN IDENTITY ARGUMENT (plan G3/D3). The monolith sat on the caller's CHARACTER
//!    and laundered a client-supplied playerId through ResolveSenderPlayerId(). This component sits on
//!    the caller's CONTROLLER ENTITY, so the caller is ResolveOwningPlayerId() and the parameter is
//!    DELETED rather than ignored.
//!
//! 2. THE THREE WAREHOUSE HANDLERS GAIN THE VALIDATION THEY NEVER HAD. AddToWarehouse and
//!    TakeFromWarehouse were bare one-line forwards with no checks at all: any client could push any
//!    arbitrary string, in any quantity, into any warehouse id, or drain one. They now check count > 0,
//!    warehouse id in range, and - the important one - that the string names a REGISTERED RESOURCE
//!    (economy.IsRegisteredResource). Warehouse inventories are keyed by raw ResourceName string and are
//!    persisted, so an unregistered string was a permanent, un-priced, un-spawnable row in the save.
//!
//! 3. THE TWO OVT_Global WAREHOUSE HELPERS ARE GONE (plan G8/T3.x). TransferToWarehouse() and
//!    TakeFromWarehouseToVehicle() now live on OVT_RealEstateManagerComponent where the state they
//!    mutate lives. This component delegates to the manager, never to a static.
//!
//! 4. RpcAsk_SetBuildingHome IS NOT HERE, AND ITS ABSENCE IS THE POINT (plan §3.7/D6). It had zero
//!    callers repo-wide and no validation whatsoever - it let any client set any player's home to any
//!    replicated entity on the map. Real estate's own IsHome/SetAsHome fix re-adds a validated version
//!    when that work lands; see docs/features/economy/real-estate/context.md.
//!
//! WHY THE PUBLIC ENTRY POINTS BRANCH ON Replication.IsServer(). An RplRcver.Server RPC marshalled by
//! the authority is delivered to nobody (the BUG-045/052/088 family), so the monolith's unconditional
//! Rpc() meant a LISTEN-SERVER HOST could not buy, sell, rent or set home at all. The direct-call branch
//! is the project's standard fix, not an optimisation.
//------------------------------------------------------------------------------------------------
class OVT_RealEstateRequestComponent : OVT_ControllerRequestComponent
{
	//! How far the requesting player may be from a vehicle before the server refuses to load warehouse
	//! stock into it. The same 15 m the vehicle seam uses for lock/claim/upgrade/repair, so no request is
	//! refused here that another seam would have accepted.
	protected const float VEHICLE_MAX_DISTANCE = 15;

	//------------------------------------------------------------------------------------------------
	// PUBLIC ENTRY POINTS - client side.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Set the local player's home/respawn position to where their character is standing.
	void SetHome()
	{
		if(Replication.IsServer())
		{
			RpcAsk_SetHome();
		}else{
			Rpc(RpcAsk_SetHome);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Buy the building the local player's character is standing at.
	//! \param[in] useResistanceFunds True to pay from (and register ownership to) the resistance account.
	void BuyBuilding(bool useResistanceFunds)
	{
		if(Replication.IsServer())
		{
			RpcAsk_BuyBuilding(useResistanceFunds);
		}else{
			Rpc(RpcAsk_BuyBuilding, useResistanceFunds);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Sell the building the local player's character is standing at.
	//! \param[in] useResistanceFunds True to sell a resistance-owned building into the resistance account.
	void SellBuilding(bool useResistanceFunds)
	{
		if(Replication.IsServer())
		{
			RpcAsk_SellBuilding(useResistanceFunds);
		}else{
			Rpc(RpcAsk_SellBuilding, useResistanceFunds);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Rent the building the local player's character is standing at.
	//! \param[in] useResistanceFunds True to rent on the resistance account.
	void RentBuilding(bool useResistanceFunds)
	{
		if(Replication.IsServer())
		{
			RpcAsk_RentBuilding(useResistanceFunds);
		}else{
			Rpc(RpcAsk_RentBuilding, useResistanceFunds);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Give up the rental on the building the local player's character is standing at.
	//! \param[in] useResistanceFunds True to release a resistance-held rental.
	void StopRentingBuilding(bool useResistanceFunds)
	{
		if(Replication.IsServer())
		{
			RpcAsk_StopRentingBuilding(useResistanceFunds);
		}else{
			Rpc(RpcAsk_StopRentingBuilding, useResistanceFunds);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Add stock to a warehouse.
	//! \param[in] warehouseId Index into the real estate manager's warehouse list.
	//! \param[in] id ResourceName string of the item.
	//! \param[in] count How many.
	void AddToWarehouse(int warehouseId, string id, int count)
	{
		if(Replication.IsServer())
		{
			RpcAsk_AddToWarehouse(warehouseId, id, count);
		}else{
			Rpc(RpcAsk_AddToWarehouse, warehouseId, id, count);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Remove stock from a warehouse.
	//! \param[in] warehouseId Index into the real estate manager's warehouse list.
	//! \param[in] id ResourceName string of the item.
	//! \param[in] count How many.
	void TakeFromWarehouse(int warehouseId, string id, int count)
	{
		if(Replication.IsServer())
		{
			RpcAsk_TakeFromWarehouse(warehouseId, id, count);
		}else{
			Rpc(RpcAsk_TakeFromWarehouse, warehouseId, id, count);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Move stock out of a warehouse and into a vehicle's cargo.
	//! \param[in] warehouseId Index into the real estate manager's warehouse list.
	//! \param[in] id ResourceName string of the item.
	//! \param[in] qty How many (clamped server-side to what the warehouse holds and what fits).
	//! \param[in] vehicle The receiving vehicle.
	void TakeFromWarehouseToVehicle(int warehouseId, string id, int qty, IEntity vehicle)
	{
		RplComponent rpl = GetEntityRpl(vehicle);
		if(!rpl) return;

		if(Replication.IsServer())
		{
			RpcAsk_TakeFromWarehouseToVehicle(warehouseId, id, qty, rpl.Id());
		}else{
			Rpc(RpcAsk_TakeFromWarehouseToVehicle, warehouseId, id, qty, rpl.Id());
		}
	}

	//------------------------------------------------------------------------------------------------
	// SERVER HANDLERS - REAL ESTATE
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: set the caller's home position to their character's position.
	//!
	//! The position is taken from the character, never from the payload, so there is nothing to spoof:
	//! the only remaining precondition is that the caller HAS a character (a request from a dead or
	//! pre-spawn client has no position to mean).
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetHome()
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;

		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		if(!re) return;

		re.SetHomePos(playerId, player.GetOrigin());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: buy the building nearest the caller's character.
	//!
	//! Carried verbatim from the monolith: the building is resolved server-side from the caller's own
	//! position; a building that is already owned or rented is not for sale; a resistance-funded purchase
	//! requires the caller to be an officer; and the affordability test runs against whichever account is
	//! actually being charged. The menu performs the same affordability check for UX, but on a client its
	//! answer is advisory only (plan §3.4).
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_BuyBuilding(bool useResistanceFunds)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;

		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		if(!re) return;

		IEntity building = re.GetNearestBuilding(player.GetOrigin());
		if(!building) return;

		EntityID entId = building.GetID();
		if(re.IsOwned(entId) || re.IsRented(entId)) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		int cost = re.GetBuyPrice(building);

		if(useResistanceFunds)
		{
			OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
			if(!resistance || !resistance.IsOfficer(playerId)) return;
			if(!economy.ResistanceHasMoney(cost)) return;

			economy.TakeResistanceMoney(cost);
			re.SetOwnerPersistentId("resistance", building);
		}else{
			OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
			if(!players) return;

			string persId = players.GetPersistentIDFromPlayerID(playerId);
			if(persId == "") return;
			if(!economy.PlayerHasMoney(persId, cost)) return;

			economy.TakePlayerMoneyPersistentId(persId, cost);
			re.SetOwner(playerId, building);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sell the building nearest the caller's character.
	//!
	//! Carried verbatim, including the "not your last house" rule (m_mOwned[persId].Count() == 1), which
	//! exists because a player with no house has no home to respawn at. The two branches gate different
	//! things: the resistance branch needs an officer AND a resistance-owned building; the personal
	//! branch needs the caller to own it, it not to be their home, and it not to be their only one.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SellBuilding(bool useResistanceFunds)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;

		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		if(!re) return;

		IEntity building = re.GetNearestBuilding(player.GetOrigin());
		if(!building) return;

		EntityID entId = building.GetID();

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		int cost = re.GetBuyPrice(building);

		if(useResistanceFunds)
		{
			OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
			if(!resistance || !resistance.IsOfficer(playerId)) return;
			if(re.GetOwnerID(building) != "resistance") return;

			economy.AddResistanceMoney(cost);
		}else{
			OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
			if(!players) return;

			string persId = players.GetPersistentIDFromPlayerID(playerId);
			if(persId == "") return;
			if(!re.IsOwner(persId, entId)) return;
			if(re.IsHome(persId, entId)) return;
			if(re.m_mOwned.Contains(persId) && re.m_mOwned[persId].Count() == 1) return;

			economy.AddPlayerMoneyPersistentId(persId, cost);
		}

		re.SetOwner(-1, building);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: rent the building nearest the caller's character.
	//!
	//! The lattice is carried verbatim and is subtler than it looks:
	//!   - an owner may "rent" their own building (it costs nothing and just marks it occupied);
	//!   - an officer renting on the resistance account counts as an owner of a resistance-owned house;
	//!   - a building that is the caller's home, is already rented, or is owned by SOMEBODY ELSE, is out.
	//! The charge is therefore conditional on !isOwner, and lands on whichever account was named.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RentBuilding(bool useResistanceFunds)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;

		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		if(!re) return;

		IEntity building = re.GetNearestBuilding(player.GetOrigin());
		if(!building) return;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return;

		EntityID entId = building.GetID();
		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if(persId == "") return;

		if(useResistanceFunds)
		{
			OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
			if(!resistance || !resistance.IsOfficer(playerId)) return;
		}

		bool isOwner = re.IsOwner(persId, entId);
		if(useResistanceFunds && re.GetOwnerID(building) == "resistance") isOwner = true;

		if(re.IsHome(persId, entId) || re.IsRented(entId) || (re.IsOwned(entId) && !isOwner)) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

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

	//------------------------------------------------------------------------------------------------
	//! Server: release the rental on the building nearest the caller's character.
	//!
	//! Carried verbatim: only the recorded renter may stop the rental, and an officer additionally counts
	//! as the renter of a resistance-held one. There is no refund, so there is no account to charge.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_StopRentingBuilding(bool useResistanceFunds)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;

		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		if(!re) return;

		IEntity building = re.GetNearestBuilding(player.GetOrigin());
		if(!building) return;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return;

		EntityID entId = building.GetID();
		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if(persId == "") return;

		bool isRenter = re.IsRenter(persId, entId);
		if(useResistanceFunds)
		{
			OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
			if(!resistance || !resistance.IsOfficer(playerId)) return;
			if(re.GetRenterID(building) == "resistance") isRenter = true;
		}
		if(!isRenter) return;

		re.SetRenter(-1, building);
	}

	//------------------------------------------------------------------------------------------------
	// SERVER HANDLERS - WAREHOUSES
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: add stock to a warehouse.
	//!
	//! ZERO VALIDATION BEFORE THIS PHASE - a bare forward to DoAddToWarehouse. The count and range checks
	//! the manager already performs are duplicated here on purpose: a rejected request should be rejected
	//! at the seam, where it can be logged with a reason (Q9), rather than disappearing into a manager
	//! that returns silently. The registered-resource check is the genuinely new one; see the class
	//! header for why an arbitrary string in a warehouse is worse than it looks.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddToWarehouse(int warehouseId, string id, int count)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		if(!re) return;

		if(!ValidateWarehouseRequest(playerId, "add", re, warehouseId, id, count)) return;

		re.DoAddToWarehouse(warehouseId, id, count);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: remove stock from a warehouse.
	//!
	//! Same three checks as the add path, plus a stock test: the manager floors the quantity at zero, so
	//! an over-take was previously not a crash but it WAS a silent free deletion of another player's
	//! stock with no trace. Refusing it makes the request auditable.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TakeFromWarehouse(int warehouseId, string id, int count)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		if(!re) return;

		if(!ValidateWarehouseRequest(playerId, "take", re, warehouseId, id, count)) return;

		if(!WarehouseHasStock(re, warehouseId, id, count))
		{
			RejectWarehouseRequest(playerId, "take", "the warehouse does not hold that many");
			return;
		}

		re.DoTakeFromWarehouse(warehouseId, id, count);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: move stock out of a warehouse and into a vehicle's cargo.
	//!
	//! Carried: qty > 0 and the warehouse-id range check (the only two the monolith had). Added: the
	//! registered-resource check (the string is fed straight to TrySpawnPrefabToStorage, so an
	//! unregistered one is an arbitrary client-chosen prefab spawn), the caller being AT the vehicle, and
	//! PlayerMayUseVehicle - without which any client could unload the resistance's entire warehouse into
	//! a stranger's locked truck on the other side of the map.
	//!
	//! Quantity is deliberately NOT bounded beyond what the warehouse holds: the manager clamps to stock
	//! and pays only for what actually fitted, so a large qty costs nothing it did not deliver.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TakeFromWarehouseToVehicle(int warehouseId, string id, int qty, RplId vehicleId)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		if(!re) return;

		if(!ValidateWarehouseRequest(playerId, "take-to-vehicle", re, warehouseId, id, qty)) return;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!character) return;

		IEntity vehicle = ResolveEntity(vehicleId);
		if(!vehicle) return;

		if(vector.Distance(character.GetOrigin(), vehicle.GetOrigin()) > VEHICLE_MAX_DISTANCE)
		{
			RejectWarehouseRequest(playerId, "take-to-vehicle", "the player is not at the vehicle");
			return;
		}

		if(!PlayerMayUseVehicle(playerId, vehicle))
		{
			RejectWarehouseRequest(playerId, "take-to-vehicle", "the vehicle is locked and owned by somebody else");
			return;
		}

		re.TakeFromWarehouseToVehicle(warehouseId, id, qty, vehicleId);
	}

	//------------------------------------------------------------------------------------------------
	// HELPERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The three checks every warehouse request shares: a positive quantity, a warehouse id that names a
	//! real warehouse, and a resource string the economy actually knows about.
	//!
	//! IsRegisteredResource is the load-bearing one. Warehouse inventories are map<string,int> keyed by
	//! raw ResourceName and are persisted verbatim, and the take-to-vehicle path feeds the key straight
	//! to TrySpawnPrefabToStorage. An unregistered string was therefore both a permanent junk row in the
	//! save and an arbitrary client-chosen prefab spawn.
	//! \param[in] playerId Caller, for the rejection log.
	//! \param[in] request Which request, for the rejection log.
	//! \param[in] re The real estate manager.
	//! \param[in] warehouseId Requested warehouse index.
	//! \param[in] id Requested resource string.
	//! \param[in] count Requested quantity.
	//! \return True when the request may proceed.
	protected bool ValidateWarehouseRequest(int playerId, string request, OVT_RealEstateManagerComponent re, int warehouseId, string id, int count)
	{
		if(count <= 0)
		{
			RejectWarehouseRequest(playerId, request, "quantity was not positive");
			return false;
		}

		if(!re.m_aWarehouses || warehouseId < 0 || warehouseId >= re.m_aWarehouses.Count())
		{
			RejectWarehouseRequest(playerId, request, "no warehouse with that id");
			return false;
		}

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return false;

		if(!economy.IsRegisteredResource(id))
		{
			RejectWarehouseRequest(playerId, request, "that resource is not registered in the economy");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a warehouse currently holds at least count of a resource.
	//! \param[in] re The real estate manager.
	//! \param[in] warehouseId Warehouse index (already range-checked).
	//! \param[in] id Resource string.
	//! \param[in] count How many are wanted.
	//! \return True when the stock is there.
	protected bool WarehouseHasStock(OVT_RealEstateManagerComponent re, int warehouseId, string id, int count)
	{
		OVT_WarehouseData warehouse = re.m_aWarehouses[warehouseId];
		if(!warehouse || !warehouse.inventory) return false;
		if(!warehouse.inventory.Contains(id)) return false;

		return warehouse.inventory[id] >= count;
	}

	//------------------------------------------------------------------------------------------------
	//! Logs a rejected warehouse request with its reason.
	//!
	//! Quality bar Q9: a rejected request is never indistinguishable from a dropped packet. These log
	//! rather than notify because every one of these checks is NEW in this phase and the warehouse menu
	//! already gates the legitimate cases client-side - a player who has never seen a message here should
	//! not start seeing one.
	//! \param[in] playerId The rejected player.
	//! \param[in] request Which request was rejected.
	//! \param[in] reason Why.
	protected void RejectWarehouseRequest(int playerId, string request, string reason)
	{
		Print(string.Format("[OVT_RealEstateRequestComponent] Rejected warehouse %1 request from player %2: %3", request, playerId.ToString(), reason), LogLevel.WARNING);
	}
}
