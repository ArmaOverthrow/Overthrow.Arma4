//------------------------------------------------------------------------------------------------
//! One production site's persisted state. FROZEN.
//!
//! ⚠ The class NAME and the FIELD ORDER are both the save format - the container writes the name into
//! every save as a `$type` discriminator and reflects over these members in declaration order. A new
//! field may only ever be APPENDED, behind a version bump.
//------------------------------------------------------------------------------------------------
class OVT_PersistedProductionSite
{
	vector location;
	string owner;
	bool isPrivate;
	float carry;

	//! EVERY ledger line, not just the produced one - a site accepts Put. This is the shape
	//! OVT_ResourceStoreComponent.ApplyPersisted consumes.
	ref array<ref OVT_PersistedResourceLine> stock = {};
}

//------------------------------------------------------------------------------------------------
//! Persists every production site's ownership, privacy, fractional carry and stock.
//!
//! BINDING. Listed in the ComponentSerializers block of the game-mode configuration in
//! Configs/Systems/Persistence/Overthrow.conf.
//!
//! SCOPE. The site SET is world data, rediscovered on every machine, so only the mutable fields are
//! written; a record is matched back by nearest SQUARED distance within 10 m (D2).
//!
//! NOTHING HERE TOUCHES AN ENTITY (R4). Deserialization runs while the world is still being built, so
//! Deserialize checks its single Read(), hands the array to the manager's staging queue and returns.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first.
//!
//! ⚠ `sites` is spelled identically in Serialize and Deserialize: Write()/Read() derive the property
//! key from the LOCAL VARIABLE'S NAME, and a renamed local reads an EMPTY array and reports SUCCESS.
//------------------------------------------------------------------------------------------------
class OVT_ResourceProductionManagerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_ResourceProductionManagerComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes one record per discovered site.
	//! \param[in] owner The game mode entity owning the production manager.
	//! \param[in] component The production manager being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow production manager.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_ResourceProductionManagerComponent production = OVT_ResourceProductionManagerComponent.Cast(component);
		if (!production)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		array<ref OVT_PersistedProductionSite> sites = new array<ref OVT_PersistedProductionSite>();
		BuildSites(production, sites);

		context.Write(sites);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the site records back and STAGES them on the manager. Applies nothing here.
	//! \param[in] owner The game mode entity owning the production manager.
	//! \param[in] component The production manager being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_ResourceProductionManagerComponent production = OVT_ResourceProductionManagerComponent.Cast(component);
		if (!production)
			return false;

		// No version means no payload - a save taken before this feature existed. Returning HERE,
		// before any Read(), is what keeps such a campaign loadable.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		// ⚠ `sites` MUST be spelled exactly as Serialize spells it.
		array<ref OVT_PersistedProductionSite> sites = new array<ref OVT_PersistedProductionSite>();
		if (!context.Read(sites))
		{
			Print("[Overthrow] Could not read the production site records from the save payload. Nothing is staged and nothing is applied, so every site keeps the state discovery just gave it rather than being silently emptied. An empty array reads back successfully, so this is a save-format fault", LogLevel.ERROR);
			return true;
		}

		production.StagePersisted(sites);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Copies every live site record, plus its store's stock lines, into the payload array.
	//! \param[in] production The manager being saved.
	//! \param[out] sites Receives one record per discovered site.
	protected void BuildSites(notnull OVT_ResourceProductionManagerComponent production, notnull array<ref OVT_PersistedProductionSite> sites)
	{
		array<ref OVT_ProductionSiteData> live = production.GetSites();
		if (!live)
			return;

		foreach (OVT_ProductionSiteData rec : live)
		{
			if (!rec)
				continue;

			OVT_PersistedProductionSite record = new OVT_PersistedProductionSite();
			record.location = rec.location;
			record.owner = rec.owner;
			record.isPrivate = rec.isPrivate;
			record.carry = rec.carry;

			WriteStock(rec, record);

			sites.Insert(record);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Copies one site's WHOLE ledger onto its payload record.
	//! \param[in] rec The live site record.
	//! \param[out] record The payload record being filled.
	protected void WriteStock(notnull OVT_ProductionSiteData rec, notnull OVT_PersistedProductionSite record)
	{
		if (!rec.entity)
		{
			Print(string.Format("[Overthrow] The production site at %1 has no entity at save time, so its stock cannot be read and is saved as empty.", rec.location.ToString()), LogLevel.WARNING);
			return;
		}

		OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(rec.entity);
		if (!store)
			return;

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

			OVT_PersistedResourceLine line = new OVT_PersistedResourceLine();
			line.resourceId = ids[i];
			line.quantity = quantities[i];

			record.stock.Insert(line);
		}
	}
}
