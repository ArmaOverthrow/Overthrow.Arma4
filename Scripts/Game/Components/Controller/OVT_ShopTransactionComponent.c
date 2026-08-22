//------------------------------------------------------------------------------------------------
//! Outcome of a sell request, as reported back to the requesting client. Sent as an int over the
//! wire (RPC payloads stay ints and RplIds - implementation plan D7).
//------------------------------------------------------------------------------------------------
enum OVT_ShopSellResult
{
	OK,					//!< At least one item was sold.
	SHOP_NOT_FOUND,		//!< The shop RplId did not resolve to a shop on the server.
	OUT_OF_RANGE,		//!< The seller (or the vehicle) was too far from the shop.
	SHOP_DOES_NOT_BUY,	//!< This shop has no sell path at all (vehicle/procurement/zero-rate dealer).
	NOTHING_SOLD,		//!< Request was valid but no candidate item survived the eligibility rules.
	NOT_VEHICLE_OWNER,	//!< The vehicle is locked and owned by somebody else.
	VEHICLE_OCCUPIED	//!< Somebody is still in the driver's seat.
}

[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative shop buying, selling and vehicle re-arm for one player")]
class OVT_ShopTransactionComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative selling, on the per-player OVT_OverthrowController entity.
//!
//! Replaced the legacy comms monolith's RpcAsk_Sell, which trusted the client's item list and
//! enumerated the player's inventory with PURPOSE_ANY (so it could delete worn gear). Project rule
//! (overthrow-controller.md): every client->server RPC lives on a controller component like this one.
//!
//! Extends OVT_ControllerRequestComponent rather than OVT_BaseServerProgressComponent (implementation
//! plan D9): a sell completes inside one frame, so a progress dialog would only flash, and the result
//! carries money, which the progress base's (transferred, skipped) pair cannot express. The request
//! base supplies ResolveOwningPlayerId(), ResolveEntity(), GetEntityRpl() and ShouldRespondLocally(),
//! which used to be private copies in this file.
//!
//! Two entry points - the shop menu and the vehicle trunk action - feed ONE server routine
//! (ExecuteSell) different candidate lists. Implementing them separately would guarantee that one of
//! them eventually forgot the gun-dealer multiplier or the IsSoldAtShop check (D2).
//!
//! P4 OF THE CONTROLLER MIGRATION ADDED BUYING HERE (plan D5). The legacy comms monolith's RpcAsk_Buy
//! is gone; RpcAsk_BuyItems replaces it, with the same 30 m gate, the same server-derived price and the
//! same spawn/insert/equip loop - plus a quantity bound, a resource-id check, and the §3.6(a) authority
//! fix that makes a purchase actually take money and stock (see RpcAsk_BuyItems).
//!
//! Also carries the stop-gap helicopter re-arm purchase (RearmVehicle): money for ammunition is a
//! shop transaction even though no shop entity is involved, and the alternative was a whole new
//! controller component for one RPC pair that the logistics epic intends to replace anyway.
//------------------------------------------------------------------------------------------------
class OVT_ShopTransactionComponent : OVT_ControllerRequestComponent
{
	//! How far the selling player may be from the shop before the server rejects the sale.
	//! Deliberately the same 30 m the legacy comms monolith already applied to buying (and applied to
	//! the legacy sell): the shop UI needs interaction range, this is that plus latency/movement
	//! slack, and using one number means a sell is never rejected where a buy would be accepted.
	//! For the trunk path this bounds the VEHICLE's distance to the shop.
	protected const float SHOP_MAX_DISTANCE = 30;

	//! How far the player may be from the vehicle whose cargo they are selling. Tighter than the shop
	//! range because the user action already requires the player to be standing at the trunk; the
	//! slack only has to cover a long vehicle plus latency.
	protected const float VEHICLE_MAX_DISTANCE = 15;

	//! Upper bound on one buy request. NEW in P4 of the controller migration - the monolith bounded
	//! nothing, and every unit is spawned unconditionally, so this is the same reasoning that put a
	//! bound on the import path (BUG-033). 100 is deliberately the same number Import uses.
	protected const int MAX_BUY_QUANTITY = 100;

	//! Fired on the requesting client only. Args: (int soldCount, int totalEarned, int skippedCount,
	//! int OVT_ShopSellResult). Display only - see RpcDo_SellResult.
	ref ScriptInvoker m_OnSellResult = new ScriptInvoker();

	//------------------------------------------------------------------------------------------------
	//! Sell items the local player is carrying, from the shop menu.
	//!
	//! Always names ONE resource: the menu's "nothing selected" sentinel is also -1, so accepting a
	//! sell-everything form here would turn a click with no selection into a sale of an arbitrary
	//! carried item. Sell All is expressed as a large quantity of one resource, not as a wildcard.
	//! \param[in] shop The shop being sold to.
	//! \param[in] resourceId Resource id to sell. Must be a real id.
	//! \param[in] quantity How many to sell (the client's observed count; the server clamps it to what
	//! is actually held and deletable). -1 sells every copy the player is carrying.
	void SellItems(OVT_ShopComponent shop, int resourceId, int quantity)
	{
		if(!shop || resourceId < 0) return;

		RplComponent shopRpl = GetEntityRpl(shop.GetOwner());
		if(!shopRpl) return;

		if(Replication.IsServer())
		{
			RpcAsk_SellItems(shopRpl.Id(), resourceId, quantity);
		}else{
			Rpc(RpcAsk_SellItems, shopRpl.Id(), resourceId, quantity);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Sell everything in a vehicle's cargo that the shop buys, from the trunk user action.
	//! One request regardless of cargo size (quality bar Q4).
	//! \param[in] vehicle The vehicle whose cargo is being sold.
	//! \param[in] shop The shop being sold to.
	void SellVehicleCargo(IEntity vehicle, OVT_ShopComponent shop)
	{
		if(!vehicle || !shop) return;

		RplComponent vehicleRpl = GetEntityRpl(vehicle);
		if(!vehicleRpl) return;

		RplComponent shopRpl = GetEntityRpl(shop.GetOwner());
		if(!shopRpl) return;

		if(Replication.IsServer())
		{
			RpcAsk_SellVehicleCargo(vehicleRpl.Id(), shopRpl.Id());
		}else{
			Rpc(RpcAsk_SellVehicleCargo, vehicleRpl.Id(), shopRpl.Id());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sell from the requesting player's own containers.
	//!
	//! Validation order (nothing the client sent is trusted beyond naming the shop and the resource):
	//! 1. we are the server
	//! 2. the request shape is sane (quantity != 0, resource id a real id)
	//! 3. the requesting player is resolved from THIS controller entity, never from the payload
	//! 4. that player has a controlled character
	//! 5. the shop resolves from its RplId
	//! 6. the character is within SHOP_MAX_DISTANCE of the shop
	//! 7. the shop buys from players at all (type / procurement / gun-dealer multiplier)
	//! then the candidate list is re-scanned server-side and ExecuteSell re-derives every price.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SellItems(RplId shopId, int resourceId, int quantity)
	{
		if(!Replication.IsServer()) return;

		if(quantity == 0 || quantity < -1) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		// R7: an out-of-range id from a stale or malicious client must not index the resource array.
		// This also rejects the -1 wildcard, which this entry point deliberately does not accept.
		if(!economy.IsValidResourceId(resourceId)) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!character) return;

		OVT_ShopComponent shop = ResolveShop(shopId);
		if(!shop)
		{
			SendSellResult(playerId, 0, 0, 0, OVT_ShopSellResult.SHOP_NOT_FOUND);
			return;
		}

		if(vector.Distance(character.GetOrigin(), shop.GetOwner().GetOrigin()) > SHOP_MAX_DISTANCE)
		{
			SendSellResult(playerId, 0, 0, 0, OVT_ShopSellResult.OUT_OF_RANGE);
			return;
		}

		if(!ShopBuysHere(shop))
		{
			SendSellResult(playerId, 0, 0, 0, OVT_ShopSellResult.SHOP_DOES_NOT_BUY);
			return;
		}

		InventoryStorageManagerComponent inventory = OVT_SellableItemScanner.GetCharacterInventory(character);
		if(!inventory) return;

		array<IEntity> candidates = new array<IEntity>;
		OVT_SellableItemScanner.CollectUnequippedItems(character, candidates);

		int soldCount, skippedCount;
		int earned = ExecuteSell(playerId, shop, candidates, inventory, resourceId, quantity, soldCount, skippedCount);

		SendSellResult(playerId, soldCount, earned, skippedCount, ResultFor(soldCount));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sell a vehicle's cargo.
	//!
	//! Validation order: server -> requesting player -> character -> vehicle -> shop -> player near
	//! vehicle -> vehicle near shop -> shop buys -> lock/ownership -> nobody in the driver's seat ->
	//! cargo storage exists. Lock/ownership and the driver rule are the same checks
	//! OVT_UnloadStorageAction applies client-side, re-derived here because a user action's gates are
	//! not authority.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SellVehicleCargo(RplId vehicleId, RplId shopId)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!character) return;

		IEntity vehicle = ResolveEntity(vehicleId);
		if(!vehicle)
		{
			// No dedicated code for "the vehicle went away between click and RPC"; from the player's
			// point of view nothing was sold.
			SendSellResult(playerId, 0, 0, 0, OVT_ShopSellResult.NOTHING_SOLD);
			return;
		}

		OVT_ShopComponent shop = ResolveShop(shopId);
		if(!shop)
		{
			SendSellResult(playerId, 0, 0, 0, OVT_ShopSellResult.SHOP_NOT_FOUND);
			return;
		}

		if(vector.Distance(character.GetOrigin(), vehicle.GetOrigin()) > VEHICLE_MAX_DISTANCE)
		{
			SendSellResult(playerId, 0, 0, 0, OVT_ShopSellResult.OUT_OF_RANGE);
			return;
		}

		if(vector.Distance(vehicle.GetOrigin(), shop.GetOwner().GetOrigin()) > SHOP_MAX_DISTANCE)
		{
			SendSellResult(playerId, 0, 0, 0, OVT_ShopSellResult.OUT_OF_RANGE);
			return;
		}

		if(!ShopBuysHere(shop))
		{
			SendSellResult(playerId, 0, 0, 0, OVT_ShopSellResult.SHOP_DOES_NOT_BUY);
			return;
		}

		if(!PlayerMayUseVehicle(playerId, vehicle))
		{
			SendSellResult(playerId, 0, 0, 0, OVT_ShopSellResult.NOT_VEHICLE_OWNER);
			return;
		}

		if(VehicleHasPilot(vehicle))
		{
			SendSellResult(playerId, 0, 0, 0, OVT_ShopSellResult.VEHICLE_OCCUPIED);
			return;
		}

		SCR_VehicleInventoryStorageManagerComponent vehicleStorage = OVT_SellableItemScanner.GetVehicleCargoStorage(vehicle);
		if(!vehicleStorage)
		{
			SendSellResult(playerId, 0, 0, 0, OVT_ShopSellResult.NOTHING_SOLD);
			return;
		}

		array<IEntity> candidates = new array<IEntity>;
		OVT_SellableItemScanner.CollectCargoItems(vehicle, candidates);

		int soldCount, skippedCount;
		// -1 / -1: no resource filter and no quantity clamp - the trunk sells everything eligible.
		int earned = ExecuteSell(playerId, shop, candidates, vehicleStorage, -1, -1, soldCount, skippedCount);

		SendSellResult(playerId, soldCount, earned, skippedCount, ResultFor(soldCount));
	}

	//------------------------------------------------------------------------------------------------
	// BUYING - migrated from the legacy comms monolith's RpcAsk_Buy in P4 of the controller migration.
	//
	// Buy lives with sell (plan D5) because the two halves share the 30 m gate, the price model and the
	// stock table: implementing them on separate components guarantees that one of them eventually
	// forgets a rule. Vehicle buying is the deliberate exception and rides OVT_VehicleRequestComponent.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Buy items from a shop for the local player.
	//! \param[in] shop The shop being bought from.
	//! \param[in] id Resource id to buy.
	//! \param[in] num How many. Bounded 1..MAX_BUY_QUANTITY server-side.
	void BuyItems(OVT_ShopComponent shop, int id, int num)
	{
		if(!shop || id < 0 || num < 1) return;

		RplComponent shopRpl = GetEntityRpl(shop.GetOwner());
		if(!shopRpl) return;

		if(Replication.IsServer())
		{
			RpcAsk_BuyItems(shopRpl.Id(), id, num);
		}else{
			Rpc(RpcAsk_BuyItems, shopRpl.Id(), id, num);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: spawn the requested items into the buyer's inventory, charge for what was delivered and
	//! decrement the shop's stock.
	//!
	//! THIS HANDLER FIXES A LIVE ECONOMY EXPLOIT (plan §3.6(a)). The monolith's version took the money
	//! with Rpc(RpcAsk_TakePlayerMoney, ...) and decremented stock with Rpc(RpcAsk_TakeFromInventory, ...)
	//! from INSIDE this server-side handler. Both targets are RplRcver.Server handlers, and an
	//! RplRcver.Server RPC marshalled by the authority is delivered to nobody (the BUG-045/052/088
	//! family) - so a remote client received its items, was never charged, and the shop's stock never
	//! moved. The vehicle path at the same time called the identical two handlers DIRECTLY: somebody
	//! fixed vehicles and not items. Both are now direct mutations. See docs/bugs/BUG-161.md.
	//!
	//! Validation order, the same order RpcAsk_SellItems uses:
	//! 1. we are the server
	//! 2. request shape (1..MAX_BUY_QUANTITY, a real resource id) - both NEW in this phase, the monolith
	//!    bounded neither, so a single click could ask for an unbounded prefab spawn loop and an
	//!    out-of-range id indexed the resource array
	//! 3. the buyer is resolved from THIS controller entity, never from the payload
	//! 4. the buyer has a character with an inventory
	//! 5. the shop resolves from its RplId
	//! 6. the buyer is within SHOP_MAX_DISTANCE of the shop (the menu's gate is client-side only)
	//! 7. the price is re-derived server-side and the buyer can afford the whole request
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_BuyItems(RplId shopId, int id, int num)
	{
		if(!Replication.IsServer()) return;

		// Carried from BUG-033's import bound: every unit below is spawned unconditionally, so an
		// unbounded num is an unbounded prefab spawn loop on the server.
		if(num < 1 || num > MAX_BUY_QUANTITY) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		// R7: an out-of-range id from a stale or malicious client must not index the resource array.
		if(!economy.IsValidResourceId(id)) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;

		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(player.FindComponent(SCR_InventoryStorageManagerComponent));
		if(!inventory) return;

		OVT_ShopComponent shop = ResolveShop(shopId);
		if(!shop || !shop.GetOwner()) return;

		// The shop UI's proximity gate is client-side only - re-check it here
		if(vector.Distance(player.GetOrigin(), shop.GetOwner().GetOrigin()) > SHOP_MAX_DISTANCE) return;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return;

		string playerPersId = players.GetPersistentIDFromPlayerID(playerId);
		if(playerPersId == "") return;

		// Same cost calculation the menu used, re-derived here so client and server agree
		int unitCost = economy.GetShopBuyPrice(id, shop, player.GetOrigin(), playerId);
		int totalCost = unitCost * num;
		if(!economy.PlayerHasMoney(playerPersId, totalCost))
		{
			SendBuyFailureNotification(playerId, "PurchaseFailedInsufficientFunds");
			return;
		}

		ResourceName itemResource = economy.GetResource(id);
		if(itemResource == "") return;

		// Spawn and insert one at a time until the inventory refuses one, so a request that only
		// partly fits is charged only for what was delivered.
		int successfulPurchases = 0;

		for(int i = 0; i < num; i++)
		{
			IEntity spawnedItem = SpawnItemForPlayer(itemResource, player.GetOrigin());
			if(!spawnedItem) break;

			bool itemHandled = false;
			if(inventory.TryInsertItem(spawnedItem))
			{
				successfulPurchases++;
				itemHandled = true;
			}
			else
			{
				// Nowhere to stow it: a weapon may still be equippable straight into the hands
				CharacterControllerComponent charController = CharacterControllerComponent.Cast(player.FindComponent(CharacterControllerComponent));
				if(charController)
				{
					BaseWeaponComponent weaponComp = BaseWeaponComponent.Cast(spawnedItem.FindComponent(BaseWeaponComponent));
					if(weaponComp && weaponComp.CanBeEquipped(charController) == ECanBeEquippedResult.OK)
					{
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
				// Never leave a paid-for-nothing entity lying at the buyer's feet
				SCR_EntityHelper.DeleteEntityAndChildren(spawnedItem);
				break;
			}
		}

		if(successfulPurchases > 0)
		{
			int actualCost = successfulPurchases * unitCost;

			// §3.6(a): a direct debit and a direct stock decrement, not two client->server asks.
			// shop.TakeFromInventory() is itself a plain server-side method as of this phase (§3.7),
			// so there is exactly one definition of "remove stock and stream the row".
			economy.DoTakePlayerMoney(playerId, actualCost);
			shop.TakeFromInventory(id, successfulPurchases);

			economy.m_OnPlayerBuy.Invoke(playerId, actualCost);
			economy.m_OnPlayerTransaction.Invoke(playerId, shop, true, actualCost);

			// Only a partial delivery needs saying; a full one is self-evident
			if(successfulPurchases < num)
				SendBuyPartialNotification(playerId, successfulPurchases, num);
		}
		else
		{
			// Funds were sufficient and nothing was delivered: the only way to get here is that the
			// item could neither be inserted nor equipped. Silence used to make this read as a dead
			// Buy button; the localized message has existed since the notification config was
			// written, it was simply never sent.
			SendBuyFailureNotification(playerId, "PurchaseFailedInventoryFull");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns one purchased item at the buyer's feet, ready to be inserted or equipped.
	//! Carried verbatim from the legacy comms monolith's SpawnItemForPlayer (including the null world
	//! parent, which is what the buy path has always used).
	//! \param[in] itemResource The item's prefab.
	//! \param[in] location Where to spawn it.
	//! \return The spawned entity, or null.
	protected IEntity SpawnItemForPlayer(ResourceName itemResource, vector location)
	{
		if(itemResource.IsEmpty()) return null;

		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = location;

		Resource resource = Resource.Load(itemResource);
		if(!resource)
		{
			Print(string.Format("[OVT_ShopTransactionComponent] Failed to load resource: %1", itemResource), LogLevel.WARNING);
			return null;
		}

		return GetGame().SpawnEntityPrefab(resource, null, params);
	}

	//------------------------------------------------------------------------------------------------
	//! Sends one of the existing purchase-failure notifications to a single player (quality bar Q9).
	//! \param[in] playerId The player to notify.
	//! \param[in] messageTag Notification tag from the notification config.
	protected void SendBuyFailureNotification(int playerId, string messageTag)
	{
		OVT_NotificationManagerComponent notify = OVT_Global.GetNotify();
		if(notify)
			notify.SendTextNotification(messageTag, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Tells the buyer that only part of their order fitted.
	//! \param[in] playerId The player to notify.
	//! \param[in] successCount How many were delivered.
	//! \param[in] totalRequested How many were asked for.
	protected void SendBuyPartialNotification(int playerId, int successCount, int totalRequested)
	{
		OVT_NotificationManagerComponent notify = OVT_Global.GetNotify();
		if(notify)
			notify.SendTextNotification("PurchasePartialSuccess", playerId, successCount.ToString(), totalRequested.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! Fully re-arm an armed vehicle for money, from the helicopter Re-arm user action. A purchase
	//! like any other here, except the goods are ammunition already bolted to the buyer's aircraft.
	//! \param[in] vehicle The vehicle to re-arm.
	void RearmVehicle(IEntity vehicle)
	{
		if(!vehicle) return;

		RplComponent vehicleRpl = GetEntityRpl(vehicle);
		if(!vehicleRpl) return;

		// The authority never loops an RplRcver.Server RPC back to itself (BUG-164), so a listen
		// host / SP player runs the handler in place.
		if(Replication.IsServer())
		{
			RpcAsk_RearmVehicle(vehicleRpl.Id());
		}else{
			Rpc(RpcAsk_RearmVehicle, vehicleRpl.Id());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: restock every weapon on a vehicle and charge the difficulty-scaled price.
	//!
	//! Validation order (the client named only the vehicle, nothing else is trusted): server ->
	//! requesting player -> character -> vehicle -> player near vehicle -> vehicle on a built
	//! helipad at a resistance-held base -> something actually missing -> funds. The helipad, need
	//! and price rules are the same OVT_VehicleRearmUtils code the user action's gates run, so a
	//! request the action offered is one this handler accepts. Charged only after the restock, and
	//! only refused-with-a-toast for the one failure a player can do something about (money).
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RearmVehicle(RplId vehicleId)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!character) return;

		IEntity vehicle = ResolveEntity(vehicleId);
		if(!vehicle) return;

		if(vector.Distance(character.GetOrigin(), vehicle.GetOrigin()) > VEHICLE_MAX_DISTANCE) return;

		OVT_VehicleRearmUtils rearmUtils = new OVT_VehicleRearmUtils();
		if(!rearmUtils.IsOnHelipadAtFriendlyBase(vehicle.GetOrigin())) return;

		if(!OVT_VehicleRearmUtils.NeedsRearm(vehicle)) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return;

		int cost = OVT_VehicleRearmUtils.GetRearmCost();
		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if(!economy.PlayerHasMoney(persId, cost))
		{
			OVT_Global.GetNotify().SendTextNotification("CannotAfford", playerId);
			return;
		}

		OVT_VehicleRearmUtils.PerformRearm(vehicle);
		economy.TakePlayerMoney(playerId, cost);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the outcome of a sell request, for display only.
	//!
	//! This RPC must never mutate money or inventory. The authoritative money arrives through the
	//! economy manager's player-money broadcast and the shop stock through
	//! OVT_ShopComponent.RpcDo_SetInventory, so a lost packet here costs a toast, not a transaction.
	//! \param[in] soldCount Items actually deleted and paid for.
	//! \param[in] totalEarned Money credited, server-computed.
	//! \param[in] skippedCount Candidates the shop would not buy.
	//! \param[in] result OVT_ShopSellResult value.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SellResult(int soldCount, int totalEarned, int skippedCount, int result)
	{
		if(m_OnSellResult)
			m_OnSellResult.Invoke(soldCount, totalEarned, skippedCount, result);
	}

	//------------------------------------------------------------------------------------------------
	//! The one sell routine. Both entry points call this; only the candidate list, the storage manager
	//! and the two filters differ.
	//!
	//! Follows the project's delete-then-count aggregation (lifted from OVT_Global.TransferToWarehouse):
	//! the collated map is incremented ONLY inside a successful TryDeleteItem, so a partial failure
	//! credits honestly and restocks the shop with exactly what was taken.
	//! \param[in] playerId Runtime id of the paying player.
	//! \param[in] shop The buying shop.
	//! \param[in] candidates Entities to consider, already scanned server-side.
	//! \param[in] manager The storage manager that owns those entities (character or vehicle).
	//! \param[in] filterResourceId -1 sells every eligible item, otherwise only that resource.
	//! \param[in] maxItems Clamp on how many items to sell; -1 is unlimited.
	//! \param[out] soldCount Items actually deleted.
	//! \param[out] skippedCount Candidates that were offered to the rules and rejected. Items of a
	//! different resource than filterResourceId are NOT counted - they were never being sold. A
	//! container that still holds something IS counted here: it is skipped rather than sold, because
	//! deleting it would take its contents with it uncredited (see HasStoredContents).
	//! \return Total money credited.
	protected int ExecuteSell(int playerId, OVT_ShopComponent shop,
							  array<IEntity> candidates,
							  InventoryStorageManagerComponent manager,
							  int filterResourceId, int maxItems,
							  out int soldCount, out int skippedCount)
	{
		soldCount = 0;
		skippedCount = 0;

		if(!shop || !candidates || !manager) return 0;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return 0;

		IEntity shopEntity = shop.GetOwner();
		if(!shopEntity) return 0;

		vector shopPos = shopEntity.GetOrigin();
		float multiplier = OVT_ShopSellRules.GetSellMultiplier(shop.m_ShopType, GetGunDealerMultiplier());
		bool typeHasRules = ShopTypeHasInventoryRules(economy, shop.m_ShopType);

		// Resolved the same way GetSellPriceAtOffset resolves it internally, so the buy-cap check
		// and the price read agree on which town's stock they are talking about. -1 (no town in
		// range of this shop) disables the cap, matching the price model, which has no scarcity
		// term there either.
		int townId = -1;
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if(towns)
		{
			OVT_TownData town = towns.GetNearestTown(shopPos);
			if(town) townId = towns.GetTownID(town);
		}

		map<int, int> collated = new map<int, int>;

		int total = 0;

		foreach(IEntity item : candidates)
		{
			if(!item) continue;
			if(maxItems >= 0 && soldCount >= maxItems) break;

			ResourceName res = OVT_Global.GetPrefabName(item);
			if(res.IsEmpty())
			{
				skippedCount++;
				continue;
			}

			// R7: looted gear that never entered the resource DB has no id of its own and would
			// otherwise resolve to id 0, i.e. some other item's price.
			if(!economy.IsRegisteredResource(res))
			{
				skippedCount++;
				continue;
			}

			int id = economy.GetInventoryId(res);

			// Not what was asked for: silently ignored rather than reported as skipped.
			if(filterResourceId != -1 && id != filterResourceId) continue;

			// Eligibility is the shop-type config, which is broader than the shop's rolled stock -
			// that is what makes selling a weapon variant the dealer does not stock work (F11).
			// See ResourceIsAccepted for the un-configured shop types (gun dealer, drug store).
			if(!ResourceIsAccepted(economy, shop.m_ShopType, typeHasRules, id))
			{
				skippedCount++;
				continue;
			}

			// Marginal pricing (BUG-117): unit i of a resource is priced as if the i units sold
			// before it had already entered the town's stock, so a bulk dump rides the scarcity
			// curve down instead of collecting the pre-sale price for every unit. collated holds
			// exactly the units of this id already deleted this call. Once the curve reaches zero
			// the offset stops growing (skips do not collate), so every further unit of that id
			// prices zero too and the dump terminates instead of overstocking the town.
			int alreadySold = 0;
			if(collated.Contains(id)) alreadySold = collated[id];

			// Hard buy cap (BUG-117 knob 4): a town saturated to the cap refuses further units
			// outright instead of accepting an unbounded glut at ever-lower prices. alreadySold
			// keeps one bulk sale from blowing through the cap before the stock is restocked.
			if(townId >= 0 && !economy.CanTownAbsorbStock(townId, id, alreadySold))
			{
				skippedCount++;
				continue;
			}

			int unitPrice = ResolveUnitPrice(economy, id, shopPos, multiplier, alreadySold);
			if(unitPrice <= 0)
			{
				skippedCount++;
				continue;
			}

			// Last gate before the item is destroyed: a container that still holds something is never
			// deleted, because TryDeleteItem takes its contents with it and nothing pays for them.
			if(HasStoredContents(item))
			{
				skippedCount++;
				continue;
			}

			if(!manager.TryDeleteItem(item))
			{
				skippedCount++;
				continue;
			}

			soldCount++;
			total = total + unitPrice;

			if(!collated.Contains(id)) collated[id] = 0;
			collated[id] = collated[id] + 1;
		}

		if(soldCount == 0) return 0;

		economy.DoAddPlayerMoney(playerId, total);
		RestockShop(shop, collated);

		// Same two events, with the same signatures, the legacy sell path fired: skills XP (I1) and
		// the stability/support transaction modifiers with isBuying = false (I2).
		economy.m_OnPlayerSell.Invoke(playerId, total);
		economy.m_OnPlayerTransaction.Invoke(playerId, shop, false, total);

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a candidate is a container that currently holds something.
	//!
	//! Why this gate exists at all: InventoryStorageManagerComponent.TryDeleteItem on a container
	//! cascades into its contents (proven at runtime during BUG-083), and the scanners deliberately
	//! return RECURSIVE candidate lists - the bandages inside a stowed backpack are candidates in their
	//! own right. So a "Sell All" of a spare backpack whose contents fail the resource filter used to
	//! delete the pack, take the bandages with it, and credit nothing for them; on the trunk path the
	//! same loop credited them or not purely on enumeration order. Neither is acceptable, so the ONE
	//! item is refused instead. It counts as skipped, so the player's toast still says something
	//! happened, and emptying the pack (or selling its contents first) makes it sellable.
	//!
	//! Consequence worth knowing on the trunk path: when the contents happen to be enumerated after the
	//! container, they sell and the now-empty container survives that click. A second Sell then takes
	//! it. Under-selling is the fail-safe direction; the previous behaviour destroyed value.
	//!
	//! The test is BaseUniversalInventoryStorageComponent - deliberately the SAME class
	//! OVT_SellableItemScanner.IsEquipped uses to answer "is this thing container content", so there is
	//! one definition of "container" on both sides of the scan. What that catches, and what it
	//! deliberately does not (all references are vanilla prefabs):
	//!
	//! - backpacks (Items/Core/Backpack_Base.et:39), uniforms/jackets/pants
	//!   (Characters/Core/Uniform_Base.et:35) and load-bearing parts
	//!   (Characters/Core/EquipmentPart_Base.et:29) declare SCR_UniversalInventoryStorageComponent, so a
	//!   stowed one with anything in it is skipped
	//! - WEAPONS DO NOT TRIP IT. A rifle's magazine and its optics live in
	//!   SCR_WeaponAttachmentsStorageComponent (Weapons/Core/Weapon_Base.et:14), which descends from
	//!   ScriptedBaseInventoryStorageComponent and NOT from the universal one. A loaded weapon still
	//!   sells with its magazine and attachments - the legacy sell path's behaviour, which this
	//!   feature's risk R5 explicitly accepts
	//! - an assembled vest does not trip it either: its baked-in suspenders and pouches sit in a
	//!   ClothNodeStorageComponent (Characters/Vests/Vest_ALICE/Vest_ALICE_assembled_base.et:4), also
	//!   not universal, so vests stay sellable at a clothes shop
	//! - an EMPTY container is not a container as far as this is concerned. Selling spare packs is
	//!   legitimate and keeps working
	//!
	//! FindComponents rather than FindComponent because one entity may declare more than one storage
	//! and only one of them has to be occupied for the delete to destroy something. The base-typename
	//! lookup is polymorphic - the same thing OVT_SpawnLogic's starting-items path relies on when it
	//! asks a cloth item for BaseInventoryStorageComponent.
	//! \param[in] item The candidate about to be deleted.
	//! \return True when the item owns at least one non-empty universal storage.
	protected bool HasStoredContents(IEntity item)
	{
		if(!item) return false;

		array<Managed> storages = new array<Managed>;
		item.FindComponents(BaseUniversalInventoryStorageComponent, storages);

		foreach(Managed found : storages)
		{
			// Read through the base storage type: GetOwnedItems is declared there, and the concrete
			// class is whatever the prefab picked (SCR_UniversalInventoryStorageComponent in practice).
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(found);
			if(!storage) continue;

			// Default includeChildComponents = true on purpose: anything nested deeper goes in the same
			// cascade, so it has to count as contents too.
			array<InventoryItemComponent> contents = new array<InventoryItemComponent>;
			storage.GetOwnedItems(contents);

			if(!contents.IsEmpty()) return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The unit sell price this shop pays for a resource, at an optional hypothetical stock offset.
	//!
	//! This used to memoise per resource for the duration of a bulk sell, which is exactly what made
	//! every unit of a dump collect the pre-sale price (BUG-117). Pricing is now per unit: the
	//! nearest-town and nearest-port lookups repeat once per item, but both are short registry scans
	//! and TryDeleteItem dwarfs them.
	//! \param[in] economy The economy manager.
	//! \param[in] id The resource id.
	//! \param[in] shopPos The shop's world position (prices are location dependent).
	//! \param[in] multiplier The shop's sell multiplier.
	//! \param[in] stockOffset Units of this resource already sold earlier in the same bulk sale.
	//! \return The unit price.
	protected int ResolveUnitPrice(OVT_EconomyManagerComponent economy, int id, vector shopPos, float multiplier, int stockOffset = 0)
	{
		int unitPrice = economy.GetSellPriceAtOffset(id, shopPos, stockOffset);
		// Written as the legacy sell and the shop menu write it, so client and server agree to the
		// currency unit; GetSellMultiplier returns exactly 1.0 everywhere but a gun dealer.
		unitPrice = unitPrice * multiplier;

		return unitPrice;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a shop buys one specific resource at all.
	//!
	//! Public because the sell browser must grey out exactly the rows the server will refuse; sharing
	//! one implementation is the only way the offer and the authority cannot drift (D3). Note this is
	//! the PER-ITEM question - ShopBuysHere / OVT_ShopSellRules.ShopBuysFromPlayers answers the
	//! per-shop one, and both must pass.
	//! \param[in] shop The shop.
	//! \param[in] id The resource id.
	//! \return True when this shop's type accepts that resource.
	bool ShopBuysResource(OVT_ShopComponent shop, int id)
	{
		if(!shop) return false;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return false;

		if(!ResourceIsAccepted(economy, shop.m_ShopType, ShopTypeHasInventoryRules(economy, shop.m_ShopType), id))
			return false;

		// Buy cap (BUG-117 knob 4): a town saturated on this item greys the row out in the sell
		// browser with the same not-bought-here treatment, so the offer matches what ExecuteSell
		// will refuse. Town resolution mirrors ExecuteSell's.
		IEntity shopEntity = shop.GetOwner();
		if(shopEntity)
		{
			OVT_TownManagerComponent towns = OVT_Global.GetTowns();
			if(towns)
			{
				OVT_TownData town = towns.GetNearestTown(shopEntity.GetOrigin());
				if(town && !economy.CanTownAbsorbStock(towns.GetTownID(town), id, 0))
					return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Per-item eligibility, with the no-rules fallback hoisted out of the loop.
	//!
	//! DELIBERATE DEVIATION from the plan's flat "IsSoldAtShopCached gates every item". Configs/System/
	//! ShopConfig.conf only configures SHOP_GENERAL, SHOP_ELECTRONIC, SHOP_CLOTHES and SHOP_VEHICLE.
	//! SHOP_GUNDEALER (stocked separately from OVT_GunDealerConfig.m_aGunDealerItemPrefabs) and
	//! SHOP_DRUG have NO inventory rules, so IsSoldAtShopCached answers false for every item there -
	//! a strict gate would silently kill the gun-dealer sell path entirely, which is the single most
	//! used sell destination in the game and an explicit acceptance criterion of this feature.
	//!
	//! So: a shop type WITH rules is gated by them (a clothes shop still refuses a rifle); a shop type
	//! WITHOUT rules accepts any registered, priced resource - which is exactly what those shops did
	//! before this feature. Checking the dealer's own prefab list instead would break F11, because
	//! that list is the ROLLED STOCK and the requirement is that an unstocked variant still sells.
	//! \param[in] economy The economy manager.
	//! \param[in] shopType OVT_ShopType value of the shop.
	//! \param[in] typeHasRules Result of ShopTypeHasInventoryRules for that type.
	//! \param[in] id The resource id.
	//! \return True when the resource is accepted.
	protected bool ResourceIsAccepted(OVT_EconomyManagerComponent economy, int shopType, bool typeHasRules, int id)
	{
		if(!typeHasRules) return true;

		return economy.IsSoldAtShopCached(id, shopType);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a shop type has any configured inventory rules at all.
	//! \param[in] economy The economy manager.
	//! \param[in] shopType OVT_ShopType value of the shop.
	//! \return True when the type's config lists at least one inventory rule.
	protected bool ShopTypeHasInventoryRules(OVT_EconomyManagerComponent economy, int shopType)
	{
		if(!economy) return false;

		OVT_ShopInventoryConfig config = economy.GetShopConfig(shopType);
		if(!config || !config.m_aInventoryItems) return false;

		return !config.m_aInventoryItems.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! The unit sell price a shop pays for a resource, for callers that need to display it.
	//!
	//! Public because the sell browser must show the number the server will actually pay; sharing one
	//! implementation is the only way the shown price and the credited price cannot drift.
	//! \param[in] shop The shop.
	//! \param[in] id The resource id.
	//! \return The unit price, or 0 when it cannot be resolved.
	int GetSellUnitPrice(OVT_ShopComponent shop, int id)
	{
		if(!shop || !shop.GetOwner()) return 0;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return 0;

		float multiplier = OVT_ShopSellRules.GetSellMultiplier(shop.m_ShopType, GetGunDealerMultiplier());
		return ResolveUnitPrice(economy, id, shop.GetOwner().GetOrigin(), multiplier);
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the sold items back to the shop's stock and broadcasts each changed row.
	//!
	//! Delegates row by row to OVT_ShopComponent.AddToInventory, which as of P4 of the controller
	//! migration is a plain server-side mutation plus StreamInventory(). This used to open-code the
	//! mutation because AddToInventory forwarded to a client->server ask on the legacy comms component,
	//! which is not a server-side call path (§3.6(a)); that is fixed, so there is one definition again.
	//! \param[in] shop The shop to restock.
	//! \param[in] collated Resource id -> number actually taken from the seller.
	protected void RestockShop(OVT_ShopComponent shop, map<int, int> collated)
	{
		if(!shop || !collated) return;

		for(int i = 0; i < collated.Count(); i++)
		{
			shop.AddToInventory(collated.GetKey(i), collated.GetElement(i));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Delivers the result to the requesting client.
	//!
	//! On a listen server the requester IS this machine's local player, and an Owner-targeted RPC to
	//! ourselves is not merely redundant, it is never delivered - so the invoker is fired directly and
	//! the RPC is not sent. On a dedicated server GetLocalPlayerId() is 0 and this always goes over the
	//! wire. ShouldRespondLocally() is the shared form of that test (OVT_ControllerRequestComponent).
	protected void SendSellResult(int playerId, int soldCount, int totalEarned, int skippedCount, int result)
	{
		if(ShouldRespondLocally(playerId))
		{
			RpcDo_SellResult(soldCount, totalEarned, skippedCount, result);
			return;
		}

		Rpc(RpcDo_SellResult, soldCount, totalEarned, skippedCount, result);
	}

	//------------------------------------------------------------------------------------------------
	//! OK when anything sold, NOTHING_SOLD otherwise.
	protected int ResultFor(int soldCount)
	{
		if(soldCount > 0) return OVT_ShopSellResult.OK;
		return OVT_ShopSellResult.NOTHING_SOLD;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this shop has a sell path at all, derived server-side from the shop's own type and
	//! procurement flag plus the difficulty config - never from anything the client sent.
	protected bool ShopBuysHere(OVT_ShopComponent shop)
	{
		if(!shop) return false;
		return OVT_ShopSellRules.ShopBuysFromPlayers(shop.m_ShopType, shop.m_bProcurement, GetGunDealerMultiplier());
	}

	//------------------------------------------------------------------------------------------------
	//! The difficulty config's gun dealer sell multiplier, read the same way the shop menu reads it
	//! (OVT_ShopContext.SelectItem). Falls back to 0, which makes a gun dealer refuse to buy rather
	//! than pay an unconfigured rate.
	protected float GetGunDealerMultiplier()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(!config || !config.m_Difficulty) return 0;

		return config.m_Difficulty.gunDealerSellPriceMultiplier;
	}

	// PlayerMayUseVehicle() moved to OVT_ControllerRequestComponent in P2 of the controller migration -
	// OVT_VehicleRequestComponent needs the identical lock/ownership rule for upgrade and repair, and a
	// second copy would be free to drift from this one.

	//------------------------------------------------------------------------------------------------
	//! Whether anybody is still in a pilot/driver compartment.
	protected bool VehicleHasPilot(IEntity vehicle)
	{
		SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
		if(!compartments) return false;

		array<IEntity> pilots = {};
		compartments.GetOccupantsOfType(pilots, ECompartmentType.PILOT);

		return !pilots.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves a shop component from a networked entity reference. Every step null-guarded (Q7).
	//!
	//! A RUINED SHOP RESOLVES TO NOTHING (core/damage D15). This is the server-side half of the gate -
	//! the menu cannot be opened at a ruin at all, so reaching here means a stale menu or a lying
	//! client - and it is the only per-request hook the three shop handlers share.
	protected OVT_ShopComponent ResolveShop(RplId shopId)
	{
		IEntity shopEntity = ResolveEntity(shopId);
		if(!shopEntity) return null;

		if(!OVT_StructureDamage.IsUsable(shopEntity))
		{
			Print("[OVT_ShopTransactionComponent] Shop request refused: the structure is a ruin", LogLevel.WARNING);
			return null;
		}

		return OVT_ShopComponent.Cast(shopEntity.FindComponent(OVT_ShopComponent));
	}

}
