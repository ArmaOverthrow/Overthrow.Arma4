modded class SCR_InventoryMenuUI : SCR_InventoryMenuUI
{
    override void HighlightAvailableStorages(SCR_InventorySlotUI itemSlot)
    {
        if (!itemSlot)
            return;
        
        if (!m_pActiveHoveredStorageUI)
            return;
        
        InventoryItemComponent itemComp = itemSlot.GetInventoryItemComponent();
        if (!itemComp)
            return;
        
        IEntity itemEntity = itemComp.GetOwner();
        if (!itemEntity)
            return;
        
        SCR_EntityCatalogManagerComponent entityCatalogManager = SCR_EntityCatalogManagerComponent.GetInstance();
        string smallestContainerPrefab = "";
        
        if (entityCatalogManager)
        {
            SCR_EntityCatalog resourceContainerCatalog = entityCatalogManager.GetEntityCatalogOfType(EEntityCatalogType.SUPPLY_CONTAINER_ITEM); 
            if (resourceContainerCatalog)
            {
                array<SCR_EntityCatalogEntry> entries = {};
                array<SCR_BaseEntityCatalogData> data = {};
                
                resourceContainerCatalog.GetEntityListWithData(SCR_ResourceContainerItemData, entries, data);
                
                if (!entries.IsEmpty())
                {
                    int minResourceValue;
                    int currentEntryValue;
                    
                    SCR_ResourceContainerItemData datum = SCR_ResourceContainerItemData.Cast(data[0]);
                    
                    minResourceValue = datum.GetMaxResourceValue();
                    
                    if (!minResourceValue)
                        minResourceValue = 0;
    
                    foreach (int idx, SCR_EntityCatalogEntry entry: entries)
                    {
                        datum = SCR_ResourceContainerItemData.Cast(data[idx]);
                        currentEntryValue = datum.GetMaxResourceValue();
                            
                        if (currentEntryValue > minResourceValue)
                            break;
                        
                        minResourceValue = currentEntryValue;
                        
                        if (!entry)
                            continue;
                        
                        smallestContainerPrefab = entry.GetPrefab();
                    }
                }
            }
        }
        
        BaseInventoryStorageComponent contStorage;
        array<BaseInventoryStorageComponent> contStorageOwnedStorages = {};
        SCR_EquipmentStorageComponent equipmentStorage;
        IEntity smallestContainerEntity;
        
        if (smallestContainerPrefab != "")
            smallestContainerEntity = GetGame().SpawnEntityPrefabLocal(Resource.Load(smallestContainerPrefab));
        
        foreach (SCR_InventoryStorageBaseUI storageBaseUI: m_aStorages)
        {   
            if (!storageBaseUI)
                continue;
            if (storageBaseUI.Type() == SCR_InventoryStorageLootUI)
                continue;
            if (storageBaseUI == m_pActiveHoveredStorageUI)
                continue;
            
            contStorage = storageBaseUI.GetStorage();    
            if (!contStorage)
                continue;
            
            float itemWeight = itemComp.GetTotalWeight();
            float totalWeightWithInsertedItem;
            
            totalWeightWithInsertedItem = storageBaseUI.GetTotalRoundedUpWeight(contStorage);
            totalWeightWithInsertedItem += Math.Round(itemWeight * 100) * 0.01;
            
            storageBaseUI.UpdateTotalWeight(totalWeightWithInsertedItem);
            
            float totalOccupiedVolumeWithInsertedItem;
            totalOccupiedVolumeWithInsertedItem = storageBaseUI.GetOccupiedVolume(contStorage);
            totalOccupiedVolumeWithInsertedItem += itemComp.GetTotalVolume();    

            contStorageOwnedStorages.Clear();
            contStorage.GetOwnedStorages(contStorageOwnedStorages, 1, false);
            contStorageOwnedStorages.Insert(contStorage);
            
            bool shouldUpdateVolumePercentage = true;
            
            // Check to see if the itemEntity can fit into any equipment Storages so that volume is not updated in those cases.
            BaseInventoryStorageComponent validStorage = m_InventoryManager.FindStorageForInsert(itemEntity, contStorage, EStoragePurpose.PURPOSE_EQUIPMENT_ATTACHMENT);
            if (validStorage)
            {
                // ... остальной оригинальный код ...
            }
        }
    }
}