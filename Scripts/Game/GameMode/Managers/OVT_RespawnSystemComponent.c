//------------------------------------------------------------------------------------------------
class OVT_RespawnSystemComponentClass : EPF_BaseRespawnSystemComponentClass
{
};

//! Scripted implementation that handles spawning and respawning of players.
//! Should be attached to a GameMode entity.
[ComponentEditorProps(icon: HYBRID_COMPONENT_ICON)]
class OVT_RespawnSystemComponent : EPF_BaseRespawnSystemComponent
{	
	protected OVT_OverthrowGameMode m_Overthrow;
	
	[Attribute(defvalue: "{3A99A99836F6B3DC}Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et")]
	ResourceName m_rDefaultPrefab;
	
	override protected ResourceName GetCreationPrefab(int playerId, string characterPersistenceId)
	{
		return m_rDefaultPrefab;
	}
	
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
	
	protected override void GetCreationPosition(int playerId, string characterPersistenceId, out vector position, out vector yawPitchRoll)
	{
		OVT_PlayerData player = OVT_PlayerData.Get(characterPersistenceId);
		if(player)
		{
			position = player.home;
			yawPitchRoll = "0 0 0";
		}
	}
	
	protected override void OnCharacterCreated(int playerId, string characterPersistenceId, IEntity character)
	{
		super.OnCharacterCreated(playerId, characterPersistenceId, character);
		
		InventoryStorageManagerComponent storageManager = EPF_Component<InventoryStorageManagerComponent>.Find(character);
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