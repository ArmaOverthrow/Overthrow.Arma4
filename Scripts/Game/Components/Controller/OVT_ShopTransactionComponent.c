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

[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative shop selling for one player")]
class OVT_ShopTransactionComponentClass : OVT_ComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative selling, on the per-player OVT_OverthrowController entity.
//!
//! Replaces the legacy OVT_PlayerCommsComponent.RpcAsk_Sell, which trusted the client's item list and
//! enumerated the player's inventory with PURPOSE_ANY (so it could delete worn gear). Project rule
//! (overthrow-controller.md): no new client->server RPCs go on OVT_PlayerCommsComponent.
//!
//! Extends plain OVT_Component rather than OVT_BaseServerProgressComponent (implementation plan D9):
//! a sell completes inside one frame, so a progress dialog would only flash, and the result carries
//! money, which the progress base's (transferred, skipped) pair cannot express.
//!
//! Two entry points - the shop menu and the vehicle trunk action - feed ONE server routine
//! (ExecuteSell) different candidate lists. Implementing them separately would guarantee that one of
//! them eventually forgot the gun-dealer multiplier or the IsSoldAtShop check (D2).
//------------------------------------------------------------------------------------------------
class OVT_ShopTransactionComponent : OVT_Component
{
	//! How far the selling player may be from the shop before the server rejects the sale.
	//! Deliberately the same 30 m OVT_PlayerCommsComponent already applies to buying (and applied to
	//! the legacy sell): the shop UI needs interaction range, this is that plus latency/movement
	//! slack, and using one number means a sell is never rejected where a buy would be accepted.
	//! For the trunk path this bounds the VEHICLE's distance to the shop.
	protected const float SHOP_MAX_DISTANCE = 30;

	//! How far the player may be from the vehicle whose cargo they are selling. Tighter than the shop
	//! range because the user action already requires the player to be standing at the trunk; the
	//! slack only has to cover a long vehicle plus latency.
	protected const float VEHICLE_MAX_DISTANCE = 15;

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

		map<int, int> collated = new map<int, int>;
		map<int, int> unitPrices = new map<int, int>;

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

			int unitPrice = ResolveUnitPrice(economy, unitPrices, id, shopPos, multiplier);
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
	//! The unit sell price this shop pays for a resource, memoised for the duration of one bulk sell.
	//!
	//! GetSellPrice does a nearest-town and nearest-port lookup on every call, which a trunk full of
	//! identical magazines would otherwise repeat once per magazine.
	//! \param[in] economy The economy manager.
	//! \param[in,out] cache Per-sale price memo, keyed by resource id.
	//! \param[in] id The resource id.
	//! \param[in] shopPos The shop's world position (prices are location dependent).
	//! \param[in] multiplier The shop's sell multiplier.
	//! \return The unit price.
	protected int ResolveUnitPrice(OVT_EconomyManagerComponent economy, map<int, int> cache, int id, vector shopPos, float multiplier)
	{
		if(cache && cache.Contains(id)) return cache[id];

		int unitPrice = economy.GetSellPrice(id, shopPos);
		// Written as the legacy sell and the shop menu write it, so client and server agree to the
		// currency unit; GetSellMultiplier returns exactly 1.0 everywhere but a gun dealer.
		unitPrice = unitPrice * multiplier;

		if(cache) cache[id] = unitPrice;
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

		return ResourceIsAccepted(economy, shop.m_ShopType, ShopTypeHasInventoryRules(economy, shop.m_ShopType), id);
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
		return ResolveUnitPrice(economy, null, id, shop.GetOwner().GetOrigin(), multiplier);
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the sold items back to the shop's stock and broadcasts each changed row.
	//!
	//! Mutates and streams directly, exactly as the legacy sell path did by calling its own
	//! RpcAsk_AddToInventory handler in-process: OVT_ShopComponent.AddToInventory routes through a
	//! client->server ask on the legacy comms component, which is not a server-side call path.
	//! \param[in] shop The shop to restock.
	//! \param[in] collated Resource id -> number actually taken from the seller.
	protected void RestockShop(OVT_ShopComponent shop, map<int, int> collated)
	{
		if(!shop || !shop.m_aInventory || !collated) return;

		for(int i = 0; i < collated.Count(); i++)
		{
			int id = collated.GetKey(i);
			int num = collated.GetElement(i);
			if(num <= 0) continue;

			if(!shop.m_aInventory.Contains(id)) shop.m_aInventory[id] = 0;
			shop.m_aInventory[id] = shop.m_aInventory[id] + num;
			shop.StreamInventory(id);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Delivers the result to the requesting client.
	//!
	//! On a listen server the requester IS this machine's local player, and an Owner-targeted RPC to
	//! ourselves is at best redundant - so the invoker is fired directly and the RPC is not sent. On a
	//! dedicated server GetLocalPlayerId() is 0 and this always goes over the wire.
	protected void SendSellResult(int playerId, int soldCount, int totalEarned, int skippedCount, int result)
	{
		if(playerId > 0 && playerId == SCR_PlayerController.GetLocalPlayerId())
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
	//! Which player this controller belongs to, resolved on the SERVER from the controller entity
	//! this component sits on.
	//!
	//! This is the whole anti-spoofing story: a remote client can only reach this handler through the
	//! controller entity it owns, so the identity comes from the entity, never from the payload (the
	//! legacy comms component solves the same problem by living on the player's character). The scan
	//! is over connected players only, which is single digits.
	//! \return Runtime player id, or -1.
	protected int ResolveOwningPlayerId()
	{
		OVT_OverthrowController owner = OVT_OverthrowController.Cast(GetOwner());
		if(!owner) return -1;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return -1;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if(!playerManager) return -1;

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);

		foreach(int playerId : playerIds)
		{
			OVT_OverthrowController controller = players.GetController(playerId);
			if(controller == owner) return playerId;
		}

		return -1;
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

	//------------------------------------------------------------------------------------------------
	//! Lock/ownership rule for the trunk sell, lifted from OVT_UnloadStorageAction: an unlocked
	//! vehicle, or one with no recorded owner, is fair game; a locked vehicle may only be emptied by
	//! its owner.
	//! \param[in] playerId The requesting player.
	//! \param[in] vehicle The vehicle.
	//! \return True when the player may sell this vehicle's cargo.
	protected bool PlayerMayUseVehicle(int playerId, IEntity vehicle)
	{
		OVT_PlayerOwnerComponent playerOwner = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(vehicle);
		if(!playerOwner || !playerOwner.IsLocked()) return true;

		string ownerUid = playerOwner.GetPlayerOwnerUid();
		if(ownerUid == "") return true;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return false;

		return ownerUid == players.GetPersistentIDFromPlayerID(playerId);
	}

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
	protected OVT_ShopComponent ResolveShop(RplId shopId)
	{
		IEntity shopEntity = ResolveEntity(shopId);
		if(!shopEntity) return null;

		return OVT_ShopComponent.Cast(shopEntity.FindComponent(OVT_ShopComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! RplId -> entity. RplId, never EntityID, is the only entity reference that means the same thing
	//! on both machines.
	protected IEntity ResolveEntity(RplId rplId)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(rplId));
		if(!rpl) return null;

		return rpl.GetEntity();
	}

	//------------------------------------------------------------------------------------------------
	//! The RplComponent of an entity, or null when it is not replicated (and therefore cannot be
	//! named across the network).
	protected RplComponent GetEntityRpl(IEntity entity)
	{
		if(!entity) return null;

		return RplComponent.Cast(entity.FindComponent(RplComponent));
	}
}
