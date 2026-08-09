//------------------------------------------------------------------------------------------------
//! Persists the campaign's difficulty selection from OVT_OverthrowConfigComponent.
//!
//! BINDING. Listed in the ComponentSerializers block of the game-mode configuration in
//! Configs/Systems/Persistence/Overthrow.conf, alongside vanilla's eight component serializers.
//!
//! WHAT IS PERSISTED, AND WHY IT IS NOT THE WHOLE OBJECT.
//! EPF's OVT_ConfigSaveData stored the entire OVT_DifficultySettings instance and, on load, replaced
//! config.m_Difficulty with the deserialized copy. Two problems with reproducing that literally:
//!
//!  1. m_Difficulty ALIASES a preset object and is mutated in place (docs/features/core/config
//!     context.md). Storing a whole authored config object in a save also freezes ~45 balance
//!     numbers into every save file, so a mod update could never reach an existing campaign.
//!  2. Only a handful of those numbers can actually DIFFER at runtime from what the authored preset
//!     says: DoStartGame() (OVT_OverthrowGameMode.c:240-266) copies exactly five values out of
//!     Overthrow_Config.json into the live difficulty object, and nothing else writes to it.
//!
//! So the save carries the campaign's DECISIONS - which preset, plus those five overridable values -
//! and the authored preset supplies the rest. On load the preset is re-selected by NAME using the
//! same statement shipped code uses (OVT_OverthrowGameMode.c:250), and the five values are written
//! INTO the live object rather than swapping the reference, so nothing that already holds
//! m_Difficulty sees it change underneath.
//!
//! IDEMPOTENT ON A LIVE SESSION. Deserialize also runs when saved data is re-applied to a running
//! campaign (OVT_PersistenceManagerComponent.ReapplyLatestSaveData). Both halves are safe to repeat:
//! the preset is only re-selected when the live selection's name DISAGREES with the save, so a
//! second pass swaps nothing; and the five values are plain assignments of the same numbers.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first.
//------------------------------------------------------------------------------------------------
class OVT_ConfigManagerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_OverthrowConfigComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the difficulty selection and the runtime-overridable difficulty values.
	//! \param[in] owner The game mode entity owning the config component.
	//! \param[in] component The config component being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or DEFAULT when there is no difficulty object to describe.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		const OVT_OverthrowConfigComponent config = OVT_OverthrowConfigComponent.Cast(component);
		if (!config)
			return ESerializeResult.ERROR;

		OVT_DifficultySettings difficulty = config.m_Difficulty;
		if (!difficulty)
			return ESerializeResult.DEFAULT;

		context.WriteValue("version", 1);

		const string difficultyName = difficulty.name;
		context.Write(difficultyName);

		const bool showPlayerOnMap = difficulty.showPlayerOnMap;
		context.Write(showPlayerOnMap);

		const int startingCash = difficulty.startingCash;
		context.Write(startingCash);

		const float gunDealerSellPriceMultiplier = difficulty.gunDealerSellPriceMultiplier;
		context.Write(gunDealerSellPriceMultiplier);

		const float procurementMultiplier = difficulty.procurementMultiplier;
		context.Write(procurementMultiplier);

		const float vehiclePriceMultiplier = difficulty.vehiclePriceMultiplier;
		context.Write(vehiclePriceMultiplier);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Re-selects the saved preset by name and applies the saved overridable values in place.
	//! \param[in] owner The game mode entity owning the config component.
	//! \param[in] component The config component being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_OverthrowConfigComponent config = OVT_OverthrowConfigComponent.Cast(component);
		if (!config)
			return false;

		int version;
		context.ReadValue("version", version);

		// Absent payload (older save): do not apply zero-valued defaults over live state.
		if (version < 1)
			return true;

		string difficultyName;
		context.Read(difficultyName);

		bool showPlayerOnMap;
		context.Read(showPlayerOnMap);

		int startingCash;
		context.Read(startingCash);

		float gunDealerSellPriceMultiplier;
		context.Read(gunDealerSellPriceMultiplier);

		float procurementMultiplier;
		context.Read(procurementMultiplier);

		float vehiclePriceMultiplier;
		context.Read(vehiclePriceMultiplier);

		// Re-select the preset the campaign was started on. Only ever swaps the reference when the
		// live selection actually disagrees with the save, so the common case mutates nothing.
		if (difficultyName != "" && (!config.m_Difficulty || config.m_Difficulty.name != difficultyName))
		{
			OVT_DifficultySettings preset = FindDifficultyPreset(config, difficultyName);
			if (preset)
			{
				config.m_Difficulty = preset;
			}
			else
			{
				Print(string.Format("[Overthrow] Saved difficulty preset '%1' no longer exists - keeping the configured one", difficultyName), LogLevel.WARNING);
			}
		}

		OVT_DifficultySettings difficulty = config.m_Difficulty;
		if (!difficulty)
		{
			Print("[Overthrow] No difficulty settings to restore into - the configured campaign difficulty is unchanged", LogLevel.WARNING);
			return true;
		}

		difficulty.showPlayerOnMap = showPlayerOnMap;
		difficulty.startingCash = startingCash;
		difficulty.gunDealerSellPriceMultiplier = gunDealerSellPriceMultiplier;
		difficulty.procurementMultiplier = procurementMultiplier;
		difficulty.vehiclePriceMultiplier = vehiclePriceMultiplier;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Finds a configured difficulty preset by its authored name.
	//! \param[in] config The config component holding the preset list.
	//! \param[in] presetName Name to match exactly against OVT_DifficultySettings.name.
	//! \return The matching preset, or null when the list has no entry with that name.
	protected OVT_DifficultySettings FindDifficultyPreset(notnull OVT_OverthrowConfigComponent config, string presetName)
	{
		if (!config.m_aDifficultyPresets)
			return null;

		foreach (OVT_DifficultySettings preset : config.m_aDifficultyPresets)
		{
			if (preset && preset.name == presetName)
				return preset;
		}

		return null;
	}
}
