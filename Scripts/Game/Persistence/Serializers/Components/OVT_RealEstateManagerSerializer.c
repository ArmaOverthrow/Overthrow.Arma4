//------------------------------------------------------------------------------------------------
//! Everything one player owns (or rents), as the position keys the owner manager indexes by.
//!
//! THE POSITIONS ARE STORED AS THE KEY STRINGS, NOT AS VECTORS, AND THAT IS DELIBERATE.
//! OVT_OwnerManagerComponent keys ownership by vector.ToString(false) and every lookup - including
//! GetOwnerIDFromPos(), which is how the rest of the mod asks "who owns this building?" - rebuilds
//! that string from the building's origin. Storing the key verbatim means the restored entry is
//! byte-identical to the one the live session produced, with no float round-tripping in between and
//! no world query needed while the world is still loading. EPF stored vectors and had to re-derive
//! the key by finding the nearest building to each one at load time, which additionally required the
//! building to already exist.
//------------------------------------------------------------------------------------------------
class OVT_PersistedOwnership
{
	string persistentId;

	//! Position keys, exactly as OVT_OwnerManagerComponent stores them.
	ref array<string> positions = {};
}

//------------------------------------------------------------------------------------------------
//! One warehouse and its stock.
//!
//! NO ID FIELD, ON PURPOSE. OVT_WarehouseData.id is the entry's INDEX in m_aWarehouses by
//! construction (OVT_RealEstateManagerComponent.SetOwner sets it to the array count when it appends)
//! and DoAddToWarehouse/DoTakeFromWarehouse index the array with it directly. Storing the id would
//! let a save whose ids had drifted break that invariant on load, so it is re-derived from the
//! rebuilt array instead.
//------------------------------------------------------------------------------------------------
class OVT_PersistedWarehouse
{
	vector location;
	string owner;
	bool isPrivate;
	bool isLinked;

	ref array<string> itemIds = {};
	ref array<int> itemCounts = {};
}

//------------------------------------------------------------------------------------------------
//! Persists building ownership, rentals and warehouse stock from OVT_RealEstateManagerComponent.
//!
//! BINDING. Listed in the ComponentSerializers block of the game-mode configuration in
//! Configs/Systems/Persistence/Overthrow.conf.
//!
//! WHAT IS AND IS NOT COVERED. Ownership is a MAPPING from a world position to a player, so this
//! serializer restores the mapping and nothing else: the buildings themselves are world entities
//! that come back with the world, and the player's home spawn point is part of the player record
//! (OVT_PlayerData.home, OVT_PlayerManagerSerializer). Player-placed structures and their contents
//! are separate tracked entities and are Phase 3 scope.
//!
//! POST-LOAD. No RPC, deliberately. Deserialization happens while the world is still being built,
//! and every client - joining or rejoining - receives owners, renters and warehouses through
//! RplSave/RplLoad on this component and its base (OVT_OwnerManagerComponent.c:263-336,
//! OVT_RealEstateManagerComponent.c:567-630).
//!
//! ORDERING NOTE FOR A SESSION STARTED FROM A SAVE POINT. OVT_RealEstateManagerComponent.OnPostLoad()
//! scans the map for unowned starting homes and is reached from the game mode's OnWorldPostProcess
//! via a CallLater, i.e. at least a frame after the world has finished post-processing; component
//! deserialization runs at entity-finalize time, before that. Restored ownership is therefore in
//! place before the starting-home pool is built and already-owned houses are correctly excluded.
//! This ordering was the same under EPF.
//!
//! IDEMPOTENT ON A LIVE SESSION. Deserialize also runs when saved data is re-applied to a running
//! campaign (OVT_PersistenceManagerComponent.ReapplyLatestSaveData). ApplyPersistedRealEstate()
//! re-points each position at its saved owner - detaching it from whoever holds it now, if anyone -
//! and never inserts a duplicate, so a second pass changes nothing.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first.
//------------------------------------------------------------------------------------------------
class OVT_RealEstateManagerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_RealEstateManagerComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes owned buildings, rented buildings and warehouses.
	//! \param[in] owner The game mode entity owning the real estate manager.
	//! \param[in] component The real estate manager being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow real estate manager.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_RealEstateManagerComponent realEstate = OVT_RealEstateManagerComponent.Cast(component);
		if (!realEstate)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		array<ref OVT_PersistedOwnership> ownedRecords = new array<ref OVT_PersistedOwnership>();
		WriteOwnership(realEstate.m_mOwned, ownedRecords);
		context.Write(ownedRecords);

		array<ref OVT_PersistedOwnership> rented = new array<ref OVT_PersistedOwnership>();
		WriteOwnership(realEstate.m_mRented, rented);
		context.Write(rented);

		array<ref OVT_PersistedWarehouse> warehouses = new array<ref OVT_PersistedWarehouse>();
		if (realEstate.m_aWarehouses)
		{
			foreach (OVT_WarehouseData warehouse : realEstate.m_aWarehouses)
			{
				if (!warehouse)
					continue;

				OVT_PersistedWarehouse record = new OVT_PersistedWarehouse();
				record.location = warehouse.location;
				record.owner = warehouse.owner;
				record.isPrivate = warehouse.isPrivate;
				record.isLinked = warehouse.isLinked;

				if (warehouse.inventory)
				{
					for (int i = 0; i < warehouse.inventory.Count(); i++)
					{
						string itemId = warehouse.inventory.GetKey(i);
						if (itemId == "")
							continue;

						record.itemIds.Insert(itemId);
						record.itemCounts.Insert(warehouse.inventory.GetElement(i));
					}
				}

				warehouses.Insert(record);
			}
		}
		context.Write(warehouses);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads ownership and warehouses back and hands them to the manager to apply.
	//! \param[in] owner The game mode entity owning the real estate manager.
	//! \param[in] component The real estate manager being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_RealEstateManagerComponent realEstate = OVT_RealEstateManagerComponent.Cast(component);
		if (!realEstate)
			return false;

		// See OVT_TownManagerSerializer.Deserialize() - no version means no payload for this component.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		array<ref OVT_PersistedOwnership> ownedRecords = new array<ref OVT_PersistedOwnership>();
		context.Read(ownedRecords);

		array<ref OVT_PersistedOwnership> rented = new array<ref OVT_PersistedOwnership>();
		context.Read(rented);

		array<ref OVT_PersistedWarehouse> warehouses = new array<ref OVT_PersistedWarehouse>();
		context.Read(warehouses);

		realEstate.ApplyPersistedRealEstate(ownedRecords, rented, warehouses);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Flattens one of the owner manager's player-to-positions maps into save records.
	//! \param[in] source The live map, may be null.
	//! \param[out] records Receives one record per player that holds at least one position.
	protected void WriteOwnership(map<string, ref array<string>> source, array<ref OVT_PersistedOwnership> records)
	{
		if (!source || !records)
			return;

		for (int i = 0; i < source.Count(); i++)
		{
			string persistentId = source.GetKey(i);
			if (persistentId == "")
				continue;

			array<string> positions = source.GetElement(i);
			if (!positions || positions.IsEmpty())
				continue;

			OVT_PersistedOwnership record = new OVT_PersistedOwnership();
			record.persistentId = persistentId;

			foreach (string position : positions)
			{
				if (position == "")
					continue;

				record.positions.Insert(position);
			}

			if (record.positions.IsEmpty())
				continue;

			records.Insert(record);
		}
	}
}
