//------------------------------------------------------------------------------------------------
/*
	Object responsible for handling respawn logic on the authority side with data loaded from EPF
*/
[BaseContainerProps(category: "Respawn")]
class OVT_PersistentRespawnLogic : SCR_SpawnLogic
{
	protected ref set<int> m_DisconnectingPlayers = new set<int>();
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Player Prefab", params: "et")]
	protected ResourceName m_PlayerPrefab;

	//------------------------------------------------------------------------------------------------
	override void OnPlayerRegistered_S(int playerId)
	{
		super.OnPlayerRegistered_S(playerId);
		
		
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(!mode.IsInitialized())
		{
			//Game has not started yet
			OnPlayerRegisterFailed(playerId);
			return;
		}
		
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		mode.PreparePlayer(playerId, persId);
		
		// In cases where we pushed provided player into disconnecting ones, but never resolved it,
		// ensure that this player is no longer marked as disconnecting
		int indexOf = m_DisconnectingPlayers.Find(playerId);
		if (indexOf != -1)
		{
			m_DisconnectingPlayers.Remove(indexOf);
		}
		
		// In certain cases, the player can receive a controlled entity (e.g. spawn from camera position)
		// during the first game tick and since our spawn operation would usually be enqueued (before this)
		// and processed only after (the entity is given), it would result in losing the initial entity.
		// TODO@AS: Possibly improve this on gc->script level
		GetGame().GetCallqueue().CallLater(DoInitialSpawn, 0, false, playerId);
	}
	
	//------------------------------------------------------------------------------------------------
	void OnPlayerRegisterFailed(int playerId)
	{
		int delay = Math.RandomFloat(900, 1100);
		GetGame().GetCallqueue().CallLater(OnPlayerRegistered_S, delay, false, playerId);
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnPlayerDisconnected_S(int playerId, KickCauseCode cause, int timeout)
	{
		super.OnPlayerDisconnected_S(playerId, cause, timeout);
		m_DisconnectingPlayers.Insert(playerId);
	}

	
	//------------------------------------------------------------------------------------------------
	private void DoInitialSpawn(int playerId)
	{
		// Probe reconnection component first
		IEntity returnedEntity;
		if (ResolveReconnection(playerId, returnedEntity))
		{
			// User was reconnected, their entity was returned
			return;
		}	
		
		// Spawn player the usual way, if no entity has been given yet
		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		IEntity controlledEntity = playerController.GetControlledEntity();
		if (controlledEntity)
			return;
		
		Spawn(playerId);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerEntityLost_S(int playerId)
	{
		super.OnPlayerEntityLost_S(playerId);
		Spawn(playerId);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerSpawnFailed_S(int playerId)
	{
		super.OnPlayerSpawnFailed_S(playerId);

		int delay = Math.RandomFloat(900, 1100);
		GetGame().GetCallqueue().CallLater(Spawn, delay, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void Spawn(int playerId)
	{
		// Player is disconnecting (and disappearance of controlled entity started this feedback loop).
		// Simply ignore such requests as it would create unwanted entities.
		int indexOf = m_DisconnectingPlayers.Find(playerId);
		if (indexOf != -1)
		{
			m_DisconnectingPlayers.Remove(indexOf);
			return;
		}
		
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(!mode || !mode.IsInitialized())
		{
			OnPlayerSpawnFailed_S(playerId);
			return;
		}
		
		Faction targetFaction = OVT_Global.GetConfig().GetPlayerFactionData();
		
		// Request both
		if (!GetPlayerFactionComponent_S(playerId).RequestFaction(targetFaction))
		{
			// Try again later
		}

		Faction faction = GetPlayerFactionComponent_S(playerId).GetAffiliatedFaction();
		if (!faction)
		{
			OnPlayerSpawnFailed_S(playerId);
			return;
		}
		
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		vector home = OVT_Global.GetRealEstate().GetHome(persId);

		SCR_FreeSpawnData data = new SCR_FreeSpawnData(m_PlayerPrefab, home, "0 0 0");
		if (GetPlayerRespawnComponent_S(playerId).CanSpawn(data))
			DoSpawn(playerId, data);
		else
			OnPlayerSpawnFailed_S(playerId);
	}

	protected void DoSpawn(int playerId, SCR_SpawnData data)
	{
		if (!GetPlayerRespawnComponent_S(playerId).RequestSpawn(data))
		{
			// Try again later
		}
	}
	
	override void OnPlayerSpawned_S(int playerId, IEntity entity)
	{
		InventoryStorageManagerComponent storageManager = EPF_Component<InventoryStorageManagerComponent>.Find(entity);
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
};
