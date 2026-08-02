//------------------------------------------------------------------------------------------------
//! One town's persisted state.
//!
//! A DEDICATED RECORD, NOT OVT_TownData ITSELF. The live class carries runtime-only members
//! (size, which is re-derived from the map marker or town controller every session) and its shape is
//! free to change with gameplay work; a save format must not be. Writing an explicit record also
//! makes the field ORDER visible in one place, which is what binary contexts key off.
//!
//! MODIFIERS TRAVEL AS PARALLEL ARRAYS of id and timer rather than as OVT_TownModifierData objects,
//! so that a modifier id the config no longer defines can be dropped on load without leaving a
//! half-built object behind. See OVT_TownManagerComponent.ApplyPersistedTowns().
//------------------------------------------------------------------------------------------------
class OVT_PersistedTown
{
	//! Town centre. This is the MATCH KEY - towns are world-derived, so a save cannot create one.
	vector location;

	int population;
	int targetPopulation;

	//! Stored for diagnostics and as the fallback when the modifier system cannot recalculate.
	int stability;

	int support;
	int faction;
	float areaHeat;
	vector gunDealerPosition;

	ref array<int> stabilityModifierIds = {};
	ref array<int> stabilityModifierTimers = {};
	ref array<int> supportModifierIds = {};
	ref array<int> supportModifierTimers = {};
}

//------------------------------------------------------------------------------------------------
//! Persists town control, population, support, stability and the modifier lists behind them.
//!
//! BINDING. Listed in the ComponentSerializers block of the game-mode configuration in
//! Configs/Systems/Persistence/Overthrow.conf.
//!
//! TOWNS ARE WORLD-DERIVED, SO THE SAVE ONLY CARRIES THEIR STATE.
//! OVT_TownManagerComponent.InitializeTowns() builds the town list from OVT_TownController prefabs
//! (or, on legacy maps, from city/town/village map markers) during Init, which runs in the game
//! mode's EOnInit - BEFORE component deserialization (EComponentDeserializeEvent.AFTER_ENTITY_FINALIZE).
//! A restored session therefore always has the same towns in the same places, and the save's job is
//! only to say what happened to them. Records are matched to live towns BY LOCATION, exactly as
//! EPF's OVT_TownSaveData.ApplyTo did (GetNearestTown), so a map edit that moves a town still lands
//! its state on the nearest surviving one instead of dropping it.
//!
//! WHY STABILITY IS RESTORED THROUGH ITS MODIFIERS AND NOT AS A RAW NUMBER.
//! Nothing in Overthrow sets stability directly: every path is
//! modifier added/removed -> OVT_TownModifierSystem.Recalculate() -> stored value. That
//! recalculation is the invariant the town manager maintains, so a save that restored the raw field
//! but not the modifier list would leave a town whose stability disagrees with its own modifiers -
//! and the first CheckUpdateModifiers() tick would silently "correct" it back. The modifier lists
//! are therefore the source of truth on load and stability is recomputed from them; the raw value is
//! still written, and is used verbatim when the modifier system or its config is unavailable.
//!
//! SUPPORT IS THE OPPOSITE CASE and is restored raw ON PURPOSE. Its recalculation is RELATIVE -
//! Recalculate(supportModifiers, town.support, 0, town.population) folds the CURRENT support into
//! the result - so re-running it on load would apply the same modifiers a second time.
//!
//! POST-LOAD. No RPC, deliberately. Deserialization happens while the world is still being built,
//! and every client - joining or rejoining - receives population, target population, stability,
//! support, faction and both modifier lists through the component's own RplSave/RplLoad JIP payload
//! (OVT_TownManagerComponent.c:1198-1291), which is the authoritative path for clients.
//!
//! IDEMPOTENT ON A LIVE SESSION. Deserialize also runs when saved data is re-applied to a running
//! campaign (OVT_PersistenceManagerComponent.ReapplyLatestSaveData). ApplyPersistedTowns() is plain
//! assignment plus a clear-and-rebuild of the two modifier lists, so a second pass produces exactly
//! the same state as the first.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first.
//------------------------------------------------------------------------------------------------
class OVT_TownManagerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_TownManagerComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes one record per live town.
	//! \param[in] owner The game mode entity owning the town manager.
	//! \param[in] component The town manager being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow town manager.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_TownManagerComponent towns = OVT_TownManagerComponent.Cast(component);
		if (!towns)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		array<ref OVT_PersistedTown> records = new array<ref OVT_PersistedTown>();

		array<ref OVT_TownData> liveTowns = towns.GetTowns();
		if (liveTowns)
		{
			foreach (OVT_TownData town : liveTowns)
			{
				if (!town)
					continue;

				OVT_PersistedTown record = new OVT_PersistedTown();
				record.location = town.location;
				record.population = town.population;
				record.targetPopulation = town.targetPopulation;
				record.stability = town.stability;
				record.support = town.support;
				record.faction = town.faction;
				record.areaHeat = town.areaHeat;
				record.gunDealerPosition = town.gunDealerPosition;

				WriteModifiers(town.stabilityModifiers, record.stabilityModifierIds, record.stabilityModifierTimers);
				WriteModifiers(town.supportModifiers, record.supportModifierIds, record.supportModifierTimers);

				records.Insert(record);
			}
		}

		context.Write(records);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the town records back and hands them to the manager to apply.
	//! \param[in] owner The game mode entity owning the town manager.
	//! \param[in] component The town manager being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_TownManagerComponent towns = OVT_TownManagerComponent.Cast(component);
		if (!towns)
			return false;

		// No version means this component has no payload in the stored record - a save written before
		// this serializer existed. Applying the zero-valued defaults that would be read out of nothing
		// is strictly worse than leaving the live campaign alone, so every serializer bails here.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		array<ref OVT_PersistedTown> records = new array<ref OVT_PersistedTown>();
		context.Read(records);

		towns.ApplyPersistedTowns(records);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Flattens a modifier list into the two parallel arrays the record stores.
	//! \param[in] modifiers Live modifier list, may be null.
	//! \param[out] ids Receives one entry per modifier.
	//! \param[out] timers Receives the matching timer for each entry.
	protected void WriteModifiers(array<ref OVT_TownModifierData> modifiers, array<int> ids, array<int> timers)
	{
		if (!modifiers || !ids || !timers)
			return;

		foreach (OVT_TownModifierData modifier : modifiers)
		{
			if (!modifier)
				continue;

			ids.Insert(modifier.id);
			timers.Insert(modifier.timer);
		}
	}
}
