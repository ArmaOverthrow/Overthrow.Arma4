//------------------------------------------------------------------------------------------------
//! Stateless character-loadout helpers: applying the configured civilian loadout to a character and
//! spawning a single loadout slot item.
//!
//! Split out of OVT_Global (see OVT_WorldUtils). No forwarders - all three are called on this class
//! directly.
class OVT_LoadoutUtils : Managed
{
	//! Randomize clothes for civilian. Accounts for config properties like SkipChance and PlayerOnly.
	static void RandomizeCivilianClothes(AIAgent agent)
	{
		IEntity civ = agent.GetControlledEntity();
		ApplyCivilianLoadout(civ);
	}
	
	//! Apply the GLOBAL civilian loadout (Configs/Civilians/CivilianClothes.conf) to any character entity.
	//! The one-argument form every pre-existing caller uses; it delegates so there is exactly one
	//! implementation to keep correct.
	static void ApplyCivilianLoadout(IEntity character)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return;

		ApplyCivilianLoadout(character, config.m_CivilianLoadout);
	}

	//------------------------------------------------------------------------------------------------
	//! Apply a SPECIFIC loadout config to any character entity.
	//!
	//! ⚠ THIS OVERWRITES THE CHARACTER'S CLOTHING, which is why per-type configs exist at all - AND why
	//! there are no per-type civilian prefabs: every ambient civilian is built from the one shared
	//! Group_CIV.et and a type's whole look is the per-type loadout applied here, because anything the
	//! prefab authored in these slots would have been re-dressed out of the global pool anyway. A slot
	//! the config does NOT author is left alone, so a per-type file that omits (say) Shoes keeps
	//! whatever the base prefab authored there.
	//! \param[in] character The character to dress. Safe when it has no inventory.
	//! \param[in] config The loadout to apply; null or slot-less is a no-op.
	static void ApplyCivilianLoadout(IEntity character, OVT_LoadoutConfig config)
	{
		if (!config || !config.m_aSlots)
			return;

		InventoryStorageManagerComponent storageManager = OVT_ComponentFinder<InventoryStorageManagerComponent>.Find(character);
		if (!storageManager)
			return;
		foreach (OVT_LoadoutSlot loadoutItem : config.m_aSlots)
		{
			if (loadoutItem.m_bPlayerOnly) continue;
			
			if (loadoutItem.m_fSkipChance > 0)
			{
				float rnd = s_AIRandomGenerator.RandFloat01();
				if(rnd <= loadoutItem.m_fSkipChance) continue; 
			}
			
			IEntity slotEntity = OVT_LoadoutUtils.SpawnDefaultCharacterItem(storageManager, loadoutItem);
			if (!slotEntity) continue;
			
			array<BaseInventoryStorageComponent> storages = new array<BaseInventoryStorageComponent>;
			storageManager.GetStorages(storages, EStoragePurpose.PURPOSE_LOADOUT_PROXY);
			
			BaseInventoryStorageComponent loadoutStorage;
			int suitableSlotId = -1;
			if (!storages.IsEmpty()) {
				loadoutStorage = storages[0];
				InventoryStorageSlot suitableSlot = loadoutStorage.FindSuitableSlotForItem(slotEntity);
				if (suitableSlot) {
					suitableSlotId = suitableSlot.GetID();
				}
			}
			
			if (!loadoutStorage || suitableSlotId == -1 || !storageManager.TryReplaceItem(slotEntity, loadoutStorage, suitableSlotId))
			{
				SCR_EntityHelper.DeleteEntityAndChildren(slotEntity);
			}
		}
	}
	
	static IEntity SpawnDefaultCharacterItem(InventoryStorageManagerComponent storageManager, OVT_LoadoutSlot loadoutItem)
	{
		// RandInt is max-EXCLUSIVE and RandInt(0, 0) raises an engine error, so a slot a modder authored
		// with no choices at all has to be refused before the draw rather than after it.
		if (!loadoutItem.m_aChoices || loadoutItem.m_aChoices.IsEmpty())
			return null;

		int selection = s_AIRandomGenerator.RandInt(0, loadoutItem.m_aChoices.Count());
		ResourceName prefab = loadoutItem.m_aChoices[selection];
		
		EntitySpawnParams spawnParams();
		spawnParams.Transform[3] = storageManager.GetOwner().GetOrigin();
		
		IEntity slotEntity = GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), spawnParams);
		if (!slotEntity) return null;
		
		return slotEntity;
	}
}
