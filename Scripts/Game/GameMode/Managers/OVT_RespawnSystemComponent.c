//------------------------------------------------------------------------------------------------
class OVT_RespawnSystemComponentClass : EPF_BasicRespawnSystemComponentClass
{
};

//! Scripted implementation that handles spawning and respawning of players.
//! Should be attached to a GameMode entity.
[ComponentEditorProps(icon: HYBRID_COMPONENT_ICON)]
class OVT_RespawnSystemComponent : EPF_BasicRespawnSystemComponent
{	
	protected OVT_OverthrowGameMode m_Overthrow;
	
	protected ref map<int, IEntity> m_mNeedRandomize = new map<int, IEntity>();
	
	protected override void OnUidAvailable(int playerId)
	{
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(!mode.IsInitialized())
		{
			//Game has not started yet
			OnPlayerRegisterFailed(playerId);
			return;
		}
		
		string playerUid = EPF_Utils.GetPlayerUID(playerId);
		if (!playerUid)
			return;
		
		mode.PreparePlayer(playerId, playerUid);
		
		super.OnUidAvailable(playerId);		
	}
	
	void OnPlayerRegisterFailed(int playerId)
	{
		int delay = Math.RandomFloat(900, 1100);
		GetGame().GetCallqueue().CallLater(OnPlayerRegistered_S, delay, false, playerId);
	}

	protected override void LoadCharacter(int playerId, string playerUid, EPF_CharacterSaveData saveData)
	{
		IEntity playerEntity;

		if (saveData)
		{
			//PrintFormat("Loading existing character '%1'...", playerUid);

			vector position = saveData.m_pTransformation.m_vOrigin;
			SCR_WorldTools.FindEmptyTerrainPosition(position, position, 2);
			saveData.m_pTransformation.m_vOrigin = position + "0 0.1 0"; // Anti lethal terrain clipping

			playerEntity = saveData.Spawn();

			EPF_PersistenceComponent persistenceComponent = EPF_Component<EPF_PersistenceComponent>.Find(playerEntity);
			if (persistenceComponent)
			{
				m_mLoadingCharacters.Set(playerId, playerEntity);

				if (EPF_DeferredApplyResult.IsPending(saveData))
				{
					Tuple3<int, EPF_CharacterSaveData, EPF_PersistenceComponent> context(playerId, saveData, persistenceComponent);
					EDF_ScriptInvokerCallback callback(this, "OnCharacterLoadedCallback", context);
					persistenceComponent.GetOnAfterLoadEvent().Insert(callback.Invoke);

					// TODO: Remove hard loading time limit when we know all spawn block bugs are fixed.
					GetGame().GetCallqueue().CallLater(OnCharacterLoaded, 5000, false, playerId, saveData, persistenceComponent);
				}
				else
				{
					OnCharacterLoaded(playerId, saveData, persistenceComponent);
				}

				return;
			}
		}
		
		vector home = OVT_Global.GetRealEstate().GetHome(playerUid);

		playerEntity = EPF_Utils.SpawnEntityPrefab(m_rDefaultPrefab, home + "0 0.1 0", "0 0 0");
		m_mLoadingCharacters.Set(playerId, playerEntity);
		m_mNeedRandomize.Set(playerId, playerEntity);

		EPF_PersistenceComponent persistenceComponent = EPF_Component<EPF_PersistenceComponent>.Find(playerEntity);
		if (persistenceComponent)
		{
			persistenceComponent.SetPersistentId(playerUid);
			HandoverToPlayer(playerId, playerEntity);
		}
		else
		{
			Print(string.Format("Could not create new character, prefab '%1' is missing component '%2'.", m_rDefaultPrefab, EPF_PersistenceComponent), LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(playerEntity);
			return;
		}
	}
	
	protected override void HandoverToPlayer(int playerId, IEntity character)
	{
		//PrintFormat("HandoverToPlayer(%1, %2)", playerId, character);
		SCR_PlayerController playerController = SCR_PlayerController.Cast(m_pPlayerManager.GetPlayerController(playerId));
		EDF_ScriptInvokerCallback callback(this, "OnHandoverComplete", new Tuple1<int>(playerId));
		playerController.m_OnControlledEntityChanged.Insert(callback.Invoke);

		playerController.SetInitialMainEntity(character);

		m_pGameMode.OnPlayerEntityChanged_S(playerId, null, character);

		SCR_RespawnComponent respawn = SCR_RespawnComponent.Cast(playerController.GetRespawnComponent());
		respawn.SGetOnSpawn().Invoke(); // TODO: Check if this is needed, the base game added it as a hack?!?
		respawn.NotifySpawn(character);
	}
	
	protected override void OnHandoverComplete(Managed context)
	{
		super.OnHandoverComplete(context);
		Tuple1<int> typedContext = Tuple1<int>.Cast(context);
		int playerId = typedContext.param1;
		
		if(m_mNeedRandomize.Contains(playerId))
		{
			IEntity entity = m_mNeedRandomize[playerId];
			InventoryStorageManagerComponent storageManager = EPF_Component<InventoryStorageManagerComponent>.Find(entity);
			if(storageManager)
			{
				foreach (OVT_LoadoutSlot loadoutItem : OVT_Global.GetConfig().m_CivilianLoadout.m_aSlots)
				{
					IEntity slotEntity = SpawnDefaultCharacterItem(storageManager, loadoutItem);
					if (!slotEntity) continue;
					
					if (!storageManager.TryInsertItem(slotEntity, EStoragePurpose.PURPOSE_LOADOUT_PROXY))
					{
						SCR_EntityHelper.DeleteEntityAndChildren(slotEntity);
					}
				}
			}
			m_mNeedRandomize.Remove(playerId);
		}
		m_mLoadingCharacters.Remove(playerId);
	}
	
	protected IEntity SpawnDefaultCharacterItem(InventoryStorageManagerComponent storageManager, OVT_LoadoutSlot loadoutItem)
	{
		if(!storageManager) return null;
		
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
				if(!spawnedItem) continue;

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
}