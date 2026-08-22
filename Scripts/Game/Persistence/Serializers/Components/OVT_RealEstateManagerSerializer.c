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
//! VERSION 1 ONLY. FROZEN. DO NOT EDIT THIS CLASS - not one field, not one character of its name, and
//! not the ORDER of its fields.
//!
//! The persistence binary container writes the concrete class name into every save as a `$type`
//! discriminator and creates the instance from that name on load, so this is the only class that can
//! read a version 1 warehouse payload. See OVT_PersistedJob's header for the measurement.
//!
//! `legacyLinkFlag` IS THE RETIRED LINK FLAG, RENAMED AND NOTHING ELSE. Warehouse linking was never
//! implemented (the flag was written but never set true anywhere) and logistics/storage deletes it,
//! but the slot still has to be declared: whichever way the container keys a record's members - by
//! name or by declaration order - a bool of the same type in the same position reads a version 1
//! payload correctly. DELETING it would be safe only under name keying, so it is renamed, not removed.
//!
//! NO ID FIELD, ON PURPOSE. OVT_WarehouseData.id is the entry's INDEX in m_aWarehouses by
//! construction, so a save whose ids had drifted would break that invariant on load; it is re-derived
//! from the rebuilt array instead.
//------------------------------------------------------------------------------------------------
class OVT_PersistedWarehouse
{
	vector location;
	string owner;
	bool isPrivate;
	bool legacyLinkFlag;

	ref array<string> itemIds = {};
	ref array<int> itemCounts = {};
}

//------------------------------------------------------------------------------------------------
//! One warehouse's ownership and privacy, version 2. THIS IS THE CURRENT ONE.
//!
//! The stock is gone: it lives on the building's own OVT_StorageComponent and is written by
//! OVT_StorageComponentSerializer (logistics/storage D2). A version 1 save's stock is handed to
//! OVT_RealEstateManagerComponent.QueueWarehouseMigration() on load and lands in the same place.
//!
//! FIELD ORDER IS THE FORMAT, AND SO IS THE CLASS NAME. A new field may only ever be APPENDED.
//------------------------------------------------------------------------------------------------
class OVT_PersistedWarehouseV2
{
	vector location;
	string owner;
	bool isPrivate;
}

//------------------------------------------------------------------------------------------------
//! Persists building ownership, rentals and the warehouse records from OVT_RealEstateManagerComponent.
//!
//! WAREHOUSE STOCK IS NOT HERE ANY MORE. It lives on each building's OVT_StorageComponent and is
//! written by OVT_StorageComponentSerializer (logistics/storage D2). A version 1 payload's stock is
//! read here and handed to the manager's migration queue, which delivers it into the building.
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
//! and every client - joining or rejoining - receives owners, renters and warehouse records through
//! RplSave/RplLoad on this component and its base.
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
	//! Version 2 dropped the two stock arrays and the never-implemented link flag from every warehouse
	//! record. Version 1 payloads are still read, and their stock is migrated into the buildings.
	static const int CURRENT_VERSION = 2;

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

		context.WriteValue("version", CURRENT_VERSION);

		array<ref OVT_PersistedOwnership> ownedRecords = new array<ref OVT_PersistedOwnership>();
		WriteOwnership(realEstate.m_mOwned, ownedRecords);
		context.Write(ownedRecords);

		array<ref OVT_PersistedOwnership> rented = new array<ref OVT_PersistedOwnership>();
		WriteOwnership(realEstate.m_mRented, rented);
		context.Write(rented);

		array<ref OVT_PersistedWarehouseV2> warehouses = new array<ref OVT_PersistedWarehouseV2>();
		if (realEstate.m_aWarehouses)
		{
			foreach (OVT_WarehouseData warehouse : realEstate.m_aWarehouses)
			{
				if (!warehouse)
					continue;

				OVT_PersistedWarehouseV2 record = new OVT_PersistedWarehouseV2();
				record.location = warehouse.location;
				record.owner = warehouse.owner;
				record.isPrivate = warehouse.isPrivate;

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

		if (version == 1)
			return DeserializeVersion1(realEstate, context);

		return DeserializeVersion2(realEstate, context);
	}

	//------------------------------------------------------------------------------------------------
	//! Reads a version 2 payload - ownership, rentals and stock-free warehouse records.
	//!
	//! EVERY READ IS CHECKED. A failed LoadContext.Read() leaves the destination non-null and EMPTY,
	//! and ApplyPersistedRealEstate() applies what it is given: empty ownership over a running campaign
	//! reads as "nobody owns anything". A read failure aborts and leaves the live maps alone.
	//! \param[in] realEstate The real estate manager. Callers null-check before calling.
	//! \param[in] context Load context positioned immediately after the version value.
	//! \return True - the payload is consumed either way.
	protected bool DeserializeVersion2(notnull OVT_RealEstateManagerComponent realEstate, notnull LoadContext context)
	{
		// The local names ARE the property names: LoadContext.Read() derives the key from the variable
		// it is handed, so these have to match what Serialize() called them.
		array<ref OVT_PersistedOwnership> ownedRecords = new array<ref OVT_PersistedOwnership>();
		if (!context.Read(ownedRecords))
			return AbortUnreadablePayload(2, "owned buildings");

		array<ref OVT_PersistedOwnership> rented = new array<ref OVT_PersistedOwnership>();
		if (!context.Read(rented))
			return AbortUnreadablePayload(2, "rented buildings");

		array<ref OVT_PersistedWarehouseV2> warehouses = new array<ref OVT_PersistedWarehouseV2>();
		if (!context.Read(warehouses))
			return AbortUnreadablePayload(2, "the warehouse records");

		realEstate.ApplyPersistedRealEstate(ownedRecords, rented, warehouses);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads a version 1 payload through the frozen record class and converts it.
	//!
	//! THE LOCAL NAMES ARE PART OF THE FORMAT, so these three carry the names the version 1 WRITER
	//! used - ownedRecords, rented, warehouses. They are unchanged in version 2, which is why only the
	//! warehouse element TYPE differs between the two readers.
	//!
	//! The stock half of every record is handed to the manager's migration queue rather than applied
	//! here: the building that receives it frequently does not exist yet at deserialization time.
	//! \param[in] realEstate The real estate manager. Callers null-check before calling.
	//! \param[in] context Load context positioned immediately after the version value.
	//! \return True - the payload is consumed either way.
	protected bool DeserializeVersion1(notnull OVT_RealEstateManagerComponent realEstate, notnull LoadContext context)
	{
		array<ref OVT_PersistedOwnership> ownedRecords = new array<ref OVT_PersistedOwnership>();
		if (!context.Read(ownedRecords))
			return AbortUnreadablePayload(1, "owned buildings");

		array<ref OVT_PersistedOwnership> rented = new array<ref OVT_PersistedOwnership>();
		if (!context.Read(rented))
			return AbortUnreadablePayload(1, "rented buildings");

		array<ref OVT_PersistedWarehouse> warehouses = new array<ref OVT_PersistedWarehouse>();
		if (!context.Read(warehouses))
			return AbortUnreadablePayload(1, "the warehouse records");

		array<ref OVT_PersistedWarehouseV2> converted = new array<ref OVT_PersistedWarehouseV2>();

		foreach (OVT_PersistedWarehouse legacyRecord : warehouses)
		{
			if (!legacyRecord)
				continue;

			OVT_PersistedWarehouseV2 record = new OVT_PersistedWarehouseV2();
			record.location = legacyRecord.location;
			record.owner = legacyRecord.owner;
			record.isPrivate = legacyRecord.isPrivate;

			converted.Insert(record);
		}

		realEstate.ApplyPersistedRealEstate(ownedRecords, rented, converted);

		// AFTER the records are applied, so GetNearestWarehouse() can already see the warehouse the
		// stock belongs to when the first drain runs.
		foreach (OVT_PersistedWarehouse stockRecord : warehouses)
		{
			if (!stockRecord)
				continue;

			realEstate.QueueWarehouseMigration(stockRecord.location, stockRecord.itemIds, stockRecord.itemCounts);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Consumes an unreadable payload without applying any part of it.
	//! \param[in] version The payload version being read.
	//! \param[in] what Which property failed, for the log line.
	//! \return Always true - the payload is consumed, the live manager is untouched.
	protected bool AbortUnreadablePayload(int version, string what)
	{
		Print(string.Format("[OVT_RealEstateManagerSerializer] Version %1 payload is unreadable at '%2'. Nothing was applied - the live ownership, rental and warehouse records are unchanged.",
			version.ToString(), what), LogLevel.ERROR);

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
