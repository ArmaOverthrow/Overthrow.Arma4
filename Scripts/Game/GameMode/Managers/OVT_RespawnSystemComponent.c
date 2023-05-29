//------------------------------------------------------------------------------------------------
class OVT_RespawnSystemComponentClass : SCR_RespawnSystemComponentClass
{
};

//! Scripted implementation that handles spawning and respawning of players.
//! Should be attached to a GameMode entity.
[ComponentEditorProps(icon: HYBRID_COMPONENT_ICON)]
class OVT_RespawnSystemComponent : SCR_RespawnSystemComponent
{
	[Attribute(defvalue: "{37578B1666981FCE}Prefabs/Characters/Core/Character_Base.et")]
	ResourceName m_rDefaultPrefab;
	
	protected ref map<GenericEntity, int> m_mLoadingCharacters = new map<GenericEntity, int>();
	protected ref map<int, GenericEntity> m_mPreparedCharacters = new map<int, GenericEntity>();
	
	protected OVT_OverthrowGameMode m_Overthrow;

	//------------------------------------------------------------------------------------------------
	void PrepareCharacter(int playerId, string playerUid, EPF_CharacterSaveData saveData)
	{
		GenericEntity playerEntity;
		
		m_Overthrow.PreparePlayer(playerId, playerUid);
		
		if (saveData)
		{
			// Spawn character from data
			EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
			
			vector spawnAngles = Vector(saveData.m_pTransformation.m_vAngles[1], saveData.m_pTransformation.m_vAngles[0], saveData.m_pTransformation.m_vAngles[2]);
			if(spawnAngles[0] == float.INFINITY) spawnAngles = "0 0 0";
			playerEntity = DoSpawn(m_rDefaultPrefab, saveData.m_pTransformation.m_vOrigin, spawnAngles);

			EPF_PersistenceComponent persistenceComponent = EPF_Component<EPF_PersistenceComponent>.Find(playerEntity);
			if (persistenceComponent)
			{
				// Remember which entity was for what player id
				m_mLoadingCharacters.Set(playerEntity, playerId);

				persistenceComponent.GetOnAfterLoadEvent().Insert(OnCharacterLoaded);
				if (persistenceComponent.Load(saveData))
					return;

				// On failure remove again
				persistenceComponent.GetOnAfterLoadEvent().Remove(OnCharacterLoaded);
				m_mLoadingCharacters.Remove(playerEntity);
			}

			Debug.Error(string.Format("Failed to apply save-data '%1:%2' to character.", saveData.Type().ToString(), saveData.GetId()));
			SCR_EntityHelper.DeleteEntityAndChildren(playerEntity);
			playerEntity = null;
		}

		if (!playerEntity)
		{
			OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(playerUid);
			
			playerEntity = DoSpawn(m_rDefaultPrefab, player.home);
			
			InventoryStorageManagerComponent storageManager = EPF_Component<InventoryStorageManagerComponent>.Find(playerEntity);
			foreach (OVT_LoadoutSlot loadoutItem : OVT_Global.GetConfig().m_CivilianLoadout.m_aSlots)
			{
				IEntity slotEntity = SpawnDefaultCharacterItem(storageManager, loadoutItem);
				if (!slotEntity) continue;
				
				if (!storageManager.TryInsertItem(slotEntity, EStoragePurpose.PURPOSE_LOADOUT_PROXY))
				{
					SCR_EntityHelper.DeleteEntityAndChildren(slotEntity);
				}
			}

			EPF_PersistenceComponent persistenceComponent = EPF_Component<EPF_PersistenceComponent>.Find(playerEntity);
			if (persistenceComponent)
			{
				persistenceComponent.SetPersistentId(playerUid);
			}
			else
			{
				Print(string.Format("Could not create new character, prefab '%1' is missing component '%2'.", m_rDefaultPrefab, EPF_PersistenceComponent), LogLevel.ERROR);
				SCR_EntityHelper.DeleteEntityAndChildren(playerEntity);
				return;
			}
			
			

			m_mPreparedCharacters.Set(playerId, playerEntity);
		}
	}
	
	protected IEntity SpawnDefaultCharacterItem(InventoryStorageManagerComponent storageManager, OVT_LoadoutSlot loadoutItem)
	{
		int selection = s_AIRandomGenerator.RandInt(0, loadoutItem.m_aChoices.Count() - 1);
		ResourceName prefab = loadoutItem.m_aChoices[selection];
		
		IEntity slotEntity = GetGame().SpawnEntityPrefab(Resource.Load(prefab));
		if (!slotEntity) return null;
		
		if (loadoutItem.m_aStoredItems)
		{
			array<Managed> outComponents();
			slotEntity.FindComponents(BaseInventoryStorageComponent, outComponents);

			foreach (ResourceName storedItem : loadoutItem.m_aStoredItems)
			{				
				IEntity spawnedItem = GetGame().SpawnEntityPrefab(Resource.Load(storedItem));

				foreach (Managed componentRef : outComponents)
				{
					BaseInventoryStorageComponent storageComponent = BaseInventoryStorageComponent.Cast(componentRef);
					
					if(storageComponent.GetPurpose() & EStoragePurpose.PURPOSE_DEPOSIT){
						if (!storageManager.TryInsertItemInStorage(spawnedItem, storageComponent)) continue;
	
						InventoryItemComponent inventoryItemComponent = InventoryItemComponent.Cast(spawnedItem.FindComponent(InventoryItemComponent));
						if (inventoryItemComponent && !inventoryItemComponent.GetParentSlot()) continue;
						break;	
					}		
				}
			}
		}
		
		return slotEntity;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCharacterLoaded(EPF_PersistenceComponent persistenceComponent, EPF_EntitySaveData saveData)
	{
		// We only want to know this once
		persistenceComponent.GetOnAfterLoadEvent().Remove(OnCharacterLoaded);

		GenericEntity playerEntity = GenericEntity.Cast(persistenceComponent.GetOwner());
		int playerId = m_mLoadingCharacters.Get(playerEntity);
		m_mLoadingCharacters.Remove(playerEntity);

		EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
		SCR_CharacterInventoryStorageComponent inventoryStorage = EPF_Component<SCR_CharacterInventoryStorageComponent>.Find(playerEntity);
		if (inventoryStorage)
		{
			EPF_CharacterInventoryStorageComponentSaveData charInventorySaveData = EPF_ComponentSaveDataGetter<EPF_CharacterInventoryStorageComponentSaveData>.GetFirst(saveData);
			if (charInventorySaveData && charInventorySaveData.m_aQuickSlotEntities)
			{
				array<RplId> quickBarRplIds();
				// Init with invalid ids
				int nQuickslots = inventoryStorage.GetQuickSlotItems().Count();
				quickBarRplIds.Reserve(nQuickslots);
				for (int i = 0; i < nQuickslots; i++)
				{
					quickBarRplIds.Insert(RplId.Invalid());
				}

				foreach (EPF_PersistentQuickSlotItem quickSlot : charInventorySaveData.m_aQuickSlotEntities)
				{
					IEntity slotEntity = persistenceManager.FindEntityByPersistentId(quickSlot.m_sEntityId);
					if (slotEntity && quickSlot.m_iIndex < quickBarRplIds.Count())
					{
						RplComponent replication = RplComponent.Cast(slotEntity.FindComponent(RplComponent));
						if (replication) quickBarRplIds.Set(quickSlot.m_iIndex, replication.Id());
					}
				}

				// Apply quick item slots serverside to avoid inital sync back from client with same data
				inventoryStorage.EPF_Rpc_UpdateQuickSlotItems(quickBarRplIds);

				SCR_RespawnComponent respawnComponent = SCR_RespawnComponent.Cast(GetGame().GetPlayerManager().GetPlayerRespawnComponent(playerId));
				if(respawnComponent)
					respawnComponent.EPF_SetQuickBarItems(quickBarRplIds);
			}
		}

		m_mPreparedCharacters.Set(playerId, playerEntity);
		return; //Wait a few frame for character and weapon controller and gadgets etc to be setup
	}

	//------------------------------------------------------------------------------------------------
	bool IsReadyForSpawn(int playerId)
	{
		return m_mPreparedCharacters.Contains(playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected override GenericEntity RequestSpawn(int playerId)
	{
		GenericEntity playerEntity = m_mPreparedCharacters.Get(playerId);
		if (playerEntity)
		{
			m_mPreparedCharacters.Remove(playerId);
			return playerEntity;
		}

		Debug.Error("Attempt to spawn a character that has not finished processing. IsReadyForSpawn was not checked?");
		return null;
	}

	//------------------------------------------------------------------------------------------------
	override void OnInit(IEntity owner)
	{
		// Hard override to not rely on faction or loadout manager
		m_pGameMode = SCR_BaseGameMode.Cast(owner);

		if (m_pGameMode)
		{
			// Use replication of the parent
			m_pRplComponent = RplComponent.Cast(m_pGameMode.FindComponent(RplComponent));
		}

		// Hard skip the rest of super implementation >:)
		
		m_Overthrow = OVT_OverthrowGameMode.Cast(owner);
	}
}