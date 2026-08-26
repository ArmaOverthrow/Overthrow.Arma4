//------------------------------------------------------------------------------------------------
//! One persisted ledger line: a prefab ResourceName and how many are held.
//!
//! DELIBERATELY NOT OVT_StorageLine, which is the live domain record the ledger, the wire fan and the
//! UI all pass around. The binary container writes the concrete CLASS NAME into every save as a
//! `$type` discriminator and creates the instance from it on load, so whichever class is written here
//! is frozen for the life of every save file - name and field order both (measured 2026-08-09; the
//! full measurement is in OVT_PersistedJob's header). Binding that freeze to a class Phases 4-9 still
//! have to shape would be a trap, so the payload gets its own record and the serializer maps between
//! the two. The mapping is also the one place a shape change is forced to be noticed.
//!
//! FIELD ORDER IS THE FORMAT and so is the class name. A new field may only ever be APPENDED.
//------------------------------------------------------------------------------------------------
class OVT_PersistedStorageLine
{
	string prefab;
	int count;
}

//------------------------------------------------------------------------------------------------
//! Persists one storage holder's item ledger and its player-set name.
//!
//! BINDING - THREE CONFIGURATIONS, ALL IN Configs/Systems/Persistence/Overthrow.conf. An entity gets
//! exactly ONE EntityPersistenceConfig, and a ComponentClassPersistenceConfigRule on
//! OVT_StorageComponent would hijack vehicles, boxes and buildings away from the configurations they
//! already match. So the serializer is appended to each of those instead:
//!
//!   1. Vanilla's CAR configuration {64C6B4937723DA61}, whose rule matches
//!      Prefabs/Vehicles/Core/Wheeled_Base.et {62F416029692CE40} - the same prefab Overthrow's
//!      same-GUID delta puts the component on, so every wheeled vehicle is covered by one entry.
//!      HELICOPTER {64EE8D74EB8192BA} is deliberately NOT touched: helicopters get no storage.
//!   2. Overthrow's PLACEABLE configuration {6B0E7A215A7FD39C}, which is how a PLACED ammo box is
//!      stored. A bare OVT_AmmoBox_Base / _Cache matches vanilla's StorageHolder.conf instead and is
//!      not covered - accepted, because only placed boxes are player stockpiles.
//!   3. Vanilla's BUILDING configuration {65B682661F79DDBE} (rule: Prefabs/Structures/Core/
//!      Building_Base.et), which the Warehouse_01 delta inherits.
//!
//! CAPACITY IS NOT PERSISTED (D8). It is prefab data resolved from economy data; re-deriving it on
//! load means a retuned prefab or price config reaches old saves, whereas a persisted copy would
//! freeze the old value into every existing campaign. ApplyPersisted() therefore adds at unlimited
//! capacity - over-cap on load is the strictly better failure.
//!
//! A WAREHOUSE BUILDING IS ONLY TRACKED BECAUSE THE COMPONENT ASKS FOR IT. Vanilla registers an
//! intact building with the persistence system never - only a DESTROYED one
//! (SCR_DestructibleBuildingComponent.GoToDestroyedState). OVT_StorageComponent.EnsureTracked() is
//! what puts a stocked or renamed one in the save point; without it this serializer would compile,
//! bind, and still never run on a warehouse.
//!
//! IDEMPOTENT ON A LIVE SESSION. ApplyPersisted() clears and rebuilds the ledger from the payload and
//! republishes the count, so re-applying the same record twice lands on the same state.
//!
//! FORMAT. Version first, then the name, then one array of line records.
//------------------------------------------------------------------------------------------------
class OVT_StorageComponentSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_StorageComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the holder's custom name and every ledger line.
	//! \param[in] owner The holder entity.
	//! \param[in] component The storage component being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow storage component.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_StorageComponent storage = OVT_StorageComponent.Cast(component);
		if (!storage)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		// The LOCAL NAMES ARE THE PROPERTY NAMES - Write() derives the key from the variable it is
		// handed - so `customName` and `lines` have to be spelled identically in Deserialize below.
		string customName = storage.GetCustomName();
		context.Write(customName);

		array<ref OVT_PersistedStorageLine> lines = new array<ref OVT_PersistedStorageLine>();
		BuildLines(storage, lines);
		context.Write(lines);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Restores the holder's name and ledger, or leaves both exactly as they are.
	//! \param[in] owner The holder entity.
	//! \param[in] component The storage component being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_StorageComponent storage = OVT_StorageComponent.Cast(component);
		if (!storage)
			return false;

		// No version means no payload for this component - see OVT_TownManagerSerializer.Deserialize().
		// Without the guard an absent payload would empty a live holder and unname it.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		// EVERY READ IS CHECKED. A failed Read() leaves its destination non-null and EMPTY, and
		// applying that over live state means "this holder is empty now and has no name". Both reads
		// happen BEFORE anything is applied, so a fault can never half-apply a name without a ledger.
		string customName;
		if (!context.Read(customName))
			return AbortUnreadablePayload(owner, "the holder's name");

		array<ref OVT_PersistedStorageLine> lines = new array<ref OVT_PersistedStorageLine>();
		if (!context.Read(lines))
			return AbortUnreadablePayload(owner, "the item ledger");

		array<ref OVT_StorageLine> applied = new array<ref OVT_StorageLine>();
		foreach (OVT_PersistedStorageLine record : lines)
		{
			if (!record)
				continue;

			OVT_StorageLine line = new OVT_StorageLine();
			line.m_sRes = record.prefab;
			line.m_iCount = record.count;
			applied.Insert(line);
		}

		storage.ApplyPersisted(customName, applied);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Converts the live ledger into payload records.
	//! \param[in] storage The holder being saved.
	//! \param[out] lines Receives one record per ledger line.
	protected void BuildLines(notnull OVT_StorageComponent storage, notnull array<ref OVT_PersistedStorageLine> lines)
	{
		OVT_StorageLedger ledger = storage.GetLedger();
		if (!ledger)
			return;

		array<string> res = new array<string>();
		array<int> counts = new array<int>();
		ledger.GetLines(res, counts);

		for (int i = 0; i < res.Count(); i++)
		{
			if (res[i] == "" || counts[i] <= 0)
				continue;

			OVT_PersistedStorageLine record = new OVT_PersistedStorageLine();
			record.prefab = res[i];
			record.count = counts[i];
			lines.Insert(record);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Reports an unreadable payload and consumes it without touching the live holder.
	//! \param[in] owner The holder whose record could not be read.
	//! \param[in] what Which part of the payload failed.
	//! \return True - the payload is consumed either way; nothing was applied.
	protected bool AbortUnreadablePayload(notnull IEntity owner, string what)
	{
		Print(string.Format("[Overthrow] Could not read %1 from the storage payload of '%2' - its ledger and name are left exactly as they are rather than being replaced with nothing. An empty holder reads back successfully, so this is a save-format fault, not an empty holder", what, OVT_PrefabUtils.GetPrefabName(owner)), LogLevel.ERROR);
		return true;
	}
}
