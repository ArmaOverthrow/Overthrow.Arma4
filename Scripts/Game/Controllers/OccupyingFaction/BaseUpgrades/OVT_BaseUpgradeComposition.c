enum OVT_SlotType
{
	SLOT_SMALL,
	SLOT_MEDIUM,
	SLOT_LARGE
}

class OVT_BaseUpgradeComposition : OVT_SlottedBaseUpgrade
{
	[Attribute()]
	string m_sCompositionTag;
	
	[Attribute("0", UIWidgets.ComboBox, "Slot size", "", ParamEnumArray.FromEnum(OVT_SlotType) )]
	OVT_SlotType m_SlotSize;
	
	override int Spend(int resources, float threat)
	{
		if(m_Spawned) return 0;
		
		OVT_FactionComposition comp = GetCompositionConfig(m_sCompositionTag);	
		if(!comp) return 0;
			
		if(comp.m_iCost > resources) return 0;
		
		IEntity ent;
		switch(m_SlotSize)
		{
			case OVT_SlotType.SLOT_SMALL:
				ent = SpawnCompositionInSmallSlot(comp);
				break;
			case OVT_SlotType.SLOT_MEDIUM:
				ent = SpawnCompositionInMediumSlot(comp);
				break;
			case OVT_SlotType.SLOT_LARGE:
				ent = SpawnCompositionInLargeSlot(comp);
				break;
			
		}
		
		if(ent) {
			FillAmmoboxes(ent);
			return comp.m_iCost;
		}
		return 0;
	}	
	
	protected void FillAmmoboxes(IEntity entity)
	{
		SlotManagerComponent slots = EPF_Component<SlotManagerComponent>.Find(entity);
		if(!slots) return;
		array<EntitySlotInfo> slotInfos();
		slots.GetSlotInfos(slotInfos);
		
		array<ResourceName> prefabs();
		OVT_Global.GetEconomy().GetAllNonClothingOccupyingFactionItems(prefabs);
				
		foreach(EntitySlotInfo slot : slotInfos)
		{
			IEntity box = slot.GetAttachedEntity();
			if(!box) continue;
			SCR_InventoryStorageManagerComponent toStorage = SCR_InventoryStorageManagerComponent.Cast(box.FindComponent(SCR_InventoryStorageManagerComponent));
			if(!toStorage) continue;
			int numItems = s_AIRandomGenerator.RandInt(15,40);
			for(int i = 0; i<numItems; i++)
			{
				int itemIndex = s_AIRandomGenerator.RandInt(0,prefabs.Count()-1);
				ResourceName res = prefabs[itemIndex];
				toStorage.TrySpawnPrefabToStorage(res);
			}
		}
	}
	
	override int GetResources()
	{
		if(!m_Spawned) return 0;
		
		OVT_FactionComposition comp = GetCompositionConfig(m_sCompositionTag);	
		if(!comp) return 0;
		
		return comp.m_iCost;
	}
}