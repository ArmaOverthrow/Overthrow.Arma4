//------------------------------------------------------------------------------------------------
//! Persists the LIVE resource price table from OVT_ResourceManagerComponent.
//!
//! BINDING. Listed in the ComponentSerializers block of the game-mode configuration in
//! Configs/Systems/Persistence/Overthrow.conf.
//!
//! SCOPE. The drifted, band-clamped STORED price per resource - nothing else on the manager is
//! state. The catalogue and every base price are rebuilt from resources.conf on load, and the
//! difficulty multiplier is applied at read time, so neither belongs in a save.
//!
//! PRICES TRAVEL AS TWO INDEX-ALIGNED ARRAYS keyed on the stable resource id, the
//! OVT_DeploymentManagerSerializer sparse-map shape. Catalogue INDICES are positional - adding a
//! resource to resources.conf shifts every one after it - so the id is written explicitly and
//! re-associated on load. An id the config no longer knows is dropped with a warning; an id the save
//! does not know starts at base.
//!
//! POST-LOAD. No RPC: deserialization runs while the world is still being built, and clients get the
//! table through the manager's own JIP payload and the drift broadcast.
//!
//! IDEMPOTENT ON A LIVE SESSION. ApplyPersistedPrices() resets the whole table to base and refills
//! it, so a second pass lands on the same table.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first.
//------------------------------------------------------------------------------------------------
class OVT_ResourceManagerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_ResourceManagerComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes one (id, stored price) pair per catalogue entry.
	//! \param[in] owner The game mode entity owning the resource manager.
	//! \param[in] component The resource manager being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow resource manager.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_ResourceManagerComponent resources = OVT_ResourceManagerComponent.Cast(component);
		if (!resources)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		// The LOCAL NAMES ARE THE PROPERTY NAMES - Write() derives the key from the variable it is
		// handed - so `priceIds` and `priceValues` have to be spelled identically in Deserialize below.
		array<string> priceIds = new array<string>();
		array<int> priceValues = new array<int>();
		BuildPrices(resources, priceIds, priceValues);

		context.Write(priceIds);
		context.Write(priceValues);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Restores the price table, or leaves it exactly as it is.
	//! \param[in] owner The game mode entity owning the resource manager.
	//! \param[in] component The resource manager being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_ResourceManagerComponent resources = OVT_ResourceManagerComponent.Cast(component);
		if (!resources)
			return false;

		// No version means no payload - every save taken before this feature existed. Those campaigns
		// keep the table InitPricesToBase() built, which is the correct answer for them.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		// Both reads are checked and BOTH happen before anything is applied, so a fault can never
		// half-apply ids without their values.
		array<string> priceIds = new array<string>();
		if (!context.Read(priceIds))
			return AbortUnreadablePayload("the resource ids");

		array<int> priceValues = new array<int>();
		if (!context.Read(priceValues))
			return AbortUnreadablePayload("the stored prices");

		if (priceIds.Count() != priceValues.Count())
		{
			Print(string.Format("[Overthrow] The saved resource price table holds %1 id(s) against %2 value(s). The pair is index-aligned by construction, so the payload is corrupt and the live table is left exactly as it is", priceIds.Count().ToString(), priceValues.Count().ToString()), LogLevel.ERROR);
			return true;
		}

		OVT_ResourceDefs defs = resources.GetDefs();
		if (priceIds.Count() > 0 && (!defs || defs.Count() == 0))
		{
			Print("[Overthrow] The save holds a resource price table but the catalogue is empty, so every id would be dropped. The live table is left as it is - check that resources.conf loaded", LogLevel.ERROR);
			return true;
		}

		resources.ApplyPersistedPrices(priceIds, priceValues);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the whole catalogue's stored prices into the two payload arrays.
	//! \param[in] resources The manager being saved.
	//! \param[out] priceIds Receives one stable resource id per catalogue entry.
	//! \param[out] priceValues Receives the matching stored price.
	protected void BuildPrices(notnull OVT_ResourceManagerComponent resources, notnull array<string> priceIds, notnull array<int> priceValues)
	{
		OVT_ResourceDefs defs = resources.GetDefs();
		if (!defs)
			return;

		for (int i = 0; i < defs.Count(); i++)
		{
			string id = defs.IdAt(i);
			if (id == "")
				continue;

			priceIds.Insert(id);
			priceValues.Insert(resources.GetStoredPrice(i));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Reports an unreadable payload and consumes it without touching the live price table.
	//! \param[in] what Which half of the payload failed.
	//! \return True - the payload is consumed either way; nothing was applied.
	protected bool AbortUnreadablePayload(string what)
	{
		Print(string.Format("[Overthrow] Could not read %1 from the resource price payload - the live price table is left exactly as it is rather than being reset to base. An empty table reads back successfully, so this is a save-format fault", what), LogLevel.ERROR);
		return true;
	}
}
