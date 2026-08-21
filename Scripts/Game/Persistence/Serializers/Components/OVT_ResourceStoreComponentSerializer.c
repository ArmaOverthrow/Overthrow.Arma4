//------------------------------------------------------------------------------------------------
//! Persists one resource holder's stock: a truck's cargo, a crate pile, a warehouse's resources.
//!
//! BINDING - FOUR CONFIGURATIONS IN Configs/Systems/Persistence/Overthrow.conf. An entity gets
//! exactly ONE EntityPersistenceConfig, so a ComponentClassPersistenceConfigRule on
//! OVT_ResourceStoreComponent would hijack every truck, warehouse and building away from the
//! configuration it already matches. The serializer is listed on each instead: vanilla's CAR
//! {64C6B4937723DA61}, vanilla's BUILDING {65B682661F79DDBE}, Overthrow's BUILDABLE
//! {6B0E7A27C0D539F2} (a BUILT holder sees only that one - D15), and the pile's own configuration,
//! whose rule may safely name OVT_ResourcePileComponent because nothing else carries it.
//!
//! THE PAYLOAD IS KEYED ON THE STABLE ID STRING, not on the catalogue index, so adding a resource
//! to resources.conf never shifts saved stock. OVT_PersistedResourceLine is declared beside the
//! component and its class name and field order are the save format - see its header.
//!
//! CAPACITY IS NOT PERSISTED. It is a prefab attribute; ApplyPersisted() therefore restocks at
//! unlimited capacity so a retuned truck volume never silently eats a player's load.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first.
//------------------------------------------------------------------------------------------------
class OVT_ResourceStoreComponentSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_ResourceStoreComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes every non-empty ledger line.
	//! \param[in] owner The holder entity.
	//! \param[in] component The resource store being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not a resource store.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_ResourceStoreComponent store = OVT_ResourceStoreComponent.Cast(component);
		if (!store)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		// The LOCAL NAME IS THE PROPERTY NAME - Write() derives the key from the variable it is handed
		// - so `lines` has to be spelled identically in Deserialize below.
		array<ref OVT_PersistedResourceLine> lines = new array<ref OVT_PersistedResourceLine>();
		BuildLines(store, lines);
		context.Write(lines);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Restores the holder's stock, or leaves it exactly as it is.
	//! \param[in] owner The holder entity.
	//! \param[in] component The resource store being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_ResourceStoreComponent store = OVT_ResourceStoreComponent.Cast(component);
		if (!store)
			return false;

		// No version means no payload for this component - every save taken before this feature
		// existed. Without the guard those holders would be "restored" to nothing, which is the same
		// bytes as a real empty holder and therefore indistinguishable from a wipe.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		// A failed Read() leaves its destination non-null and EMPTY, so applying it would read as
		// "this holder is empty now". The read is checked and the apply happens only after it.
		array<ref OVT_PersistedResourceLine> lines = new array<ref OVT_PersistedResourceLine>();
		if (!context.Read(lines))
			return AbortUnreadablePayload(owner);

		// An empty catalogue can never explain non-empty saved stock, and ApplyPersisted() would drop
		// every line against it and republish the holder as empty - a wipe, from a broken resources.conf.
		if (lines.Count() > 0 && !CatalogueIsUsable())
		{
			Print(string.Format("[Overthrow] '%1' has saved resource stock but the resource catalogue is empty, so every line would be dropped. The stock is left unapplied rather than published as nothing - check that resources.conf loaded", OVT_PrefabUtils.GetPrefabName(owner)), LogLevel.ERROR);
			return true;
		}

		store.ApplyPersisted(lines);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the resource catalogue exists and holds at least one definition.
	protected bool CatalogueIsUsable()
	{
		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
			return false;

		OVT_ResourceDefs defs = resources.GetDefs();
		if (!defs)
			return false;

		return defs.Count() > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Converts the live ledger into payload records.
	//! \param[in] store The holder being saved.
	//! \param[out] lines Receives one record per non-empty ledger line.
	protected void BuildLines(notnull OVT_ResourceStoreComponent store, notnull array<ref OVT_PersistedResourceLine> lines)
	{
		OVT_ResourceLedger ledger = store.GetLedger();
		if (!ledger)
			return;

		array<string> ids = new array<string>();
		array<int> quantities = new array<int>();
		ledger.GetLines(ids, quantities);

		for (int i = 0; i < ids.Count(); i++)
		{
			if (ids[i] == "" || quantities[i] <= 0)
				continue;

			OVT_PersistedResourceLine record = new OVT_PersistedResourceLine();
			record.resourceId = ids[i];
			record.quantity = quantities[i];
			lines.Insert(record);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Reports an unreadable payload and consumes it without touching the live holder.
	//! \param[in] owner The holder whose record could not be read.
	//! \return True - the payload is consumed either way; nothing was applied.
	protected bool AbortUnreadablePayload(notnull IEntity owner)
	{
		Print(string.Format("[Overthrow] Could not read the resource stock of '%1' - its ledger is left exactly as it is rather than being replaced with nothing. An empty holder reads back successfully, so this is a save-format fault, not an empty holder", OVT_PrefabUtils.GetPrefabName(owner)), LogLevel.ERROR);
		return true;
	}
}
