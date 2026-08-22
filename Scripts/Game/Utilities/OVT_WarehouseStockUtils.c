//------------------------------------------------------------------------------------------------
//! Sourcing items from REGISTERED warehouses in range (implementation.md §3.7, D6).
//!
//! "Registered" means a building with an OVT_WarehouseData record, filtered by the shipped
//! PlayerMayUseWarehouse privacy/rental rule. Not any OVT_StorageComponent holder: a player's own
//! backpack, a parked truck and a loot crate are not a supply chain.
//!
//! SERVER ONLY. Storage contents are pulled on open, never replicated, so a client physically cannot
//! compute a coverage figure - which is why the whole purchase quote is server-side and
//! CONFIG_STREAM_VERSION does not move for High Command.
//!
//! Every accumulator here is NEW-ED PER CALL. OVT_InventoryManagerComponent.m_aContainerSearchResults
//! (:497) is the singleton two concurrent searches overwrite for each other, and nothing in this file
//! is allowed to repeat it.
//------------------------------------------------------------------------------------------------
class OVT_WarehouseStockUtils
{
	//------------------------------------------------------------------------------------------------
	//! Every registered warehouse's storage within radius of a point that this player may use.
	//!
	//! Two filters, in this order: a warehouse RECORD within OVT_RealEstateManagerComponent's own
	//! WAREHOUSE_MATCH_RANGE of the holder (that is what "registered" means), then the shipped
	//! accessibility rule. PlayerMayUseWarehouse answers true for anything that is NOT a warehouse, so
	//! the record test has to come first or every crate in range would pass.
	//! \param[in] pos Centre of the search.
	//! \param[in] radius Search radius in metres - always OVT_HighCommandRules.WAREHOUSE_RANGE.
	//! \param[in] persistentId The asking player.
	//! \param[out] stores Receives the storages, in query order. Allocated if null, cleared otherwise.
	//! \return How many stores were collected.
	static int CollectStores(vector pos, float radius, string persistentId, out array<OVT_StorageComponent> stores)
	{
		if (!stores)
			stores = new array<OVT_StorageComponent>();

		stores.Clear();

		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if (!realEstate || !realEstate.m_aWarehouses)
			return 0;

		array<IEntity> holders = new array<IEntity>();
		OVT_StorageHolderQuery query = new OVT_StorageHolderQuery();
		query.Run(pos, radius, holders);

		foreach (IEntity holder : holders)
		{
			if (!holder)
				continue;

			if (!IsRegisteredWarehouse(realEstate, holder))
				continue;

			if (!realEstate.PlayerMayUseWarehouse(persistentId, holder))
				continue;

			OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(holder);
			if (storage)
				stores.Insert(storage);
		}

		return stores.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! How many of one resource the collected stores hold between them.
	//! \param[in] stores Stores from CollectStores().
	//! \param[in] res Prefab ResourceName, never an economy id.
	//! \return The total held; 0 for an empty name or an absent line.
	static int CountAvailable(notnull array<OVT_StorageComponent> stores, string res)
	{
		if (res == "")
			return 0;

		int total = 0;

		foreach (OVT_StorageComponent store : stores)
		{
			if (!store)
				continue;

			OVT_StorageLedger ledger = store.GetLedger();
			if (!ledger)
				continue;

			total += ledger.Count(res);
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Drains up to qty of one resource from the collected stores, in query order. SERVER ONLY.
	//!
	//! The CLAMP LIVES IN THE LEDGER, not here: Take() already answers with what it actually had, so a
	//! caller asking for the full need gets exactly min(need, available) without a second copy of that
	//! rule to drift from CountAvailable's.
	//!
	//! PublishCount() is called once per store that gave something up, never per line - the batch
	//! boundary the storage component's header defines.
	//! \param[in] stores Stores from CollectStores().
	//! \param[in] res Prefab ResourceName.
	//! \param[in] qty How many to take at most.
	//! \return How many were actually taken.
	static int TakeUpTo(notnull array<OVT_StorageComponent> stores, string res, int qty)
	{
		if (!Replication.IsServer())
			return 0;

		if (res == "" || qty <= 0)
			return 0;

		int remaining = qty;
		int taken = 0;

		foreach (OVT_StorageComponent store : stores)
		{
			if (remaining <= 0)
				break;

			if (!store)
				continue;

			OVT_StorageLedger ledger = store.GetLedger();
			if (!ledger)
				continue;

			int got = ledger.Take(res, remaining);
			if (got <= 0)
				continue;

			taken += got;
			remaining -= got;

			store.PublishCount();
		}

		return taken;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a storage holder is a building the real-estate manager holds a warehouse record for.
	//!
	//! THE SHIPPED LOOKUP, NOT A COPY OF IT. PlayerMayUseWarehouse resolves the record with exactly this
	//! call, so re-implementing the distance test here would give the two halves of the filter two
	//! different boundaries at the match range.
	//! \param[in] realEstate The real-estate manager.
	//! \param[in] holder The candidate holder.
	//! \return True when a warehouse record sits within WAREHOUSE_MATCH_RANGE of it.
	protected static bool IsRegisteredWarehouse(notnull OVT_RealEstateManagerComponent realEstate, notnull IEntity holder)
	{
		return realEstate.GetNearestWarehouse(holder.GetOrigin(), OVT_RealEstateManagerComponent.WAREHOUSE_MATCH_RANGE) != null;
	}
}
