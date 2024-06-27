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
	
	protected int m_iAgentIndex = 0;	
	
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
	
	override void Setup()
	{
		float range = 7;
		if(m_SlotSize == OVT_SlotType.SLOT_MEDIUM) range = 15;
		if(m_SlotSize == OVT_SlotType.SLOT_LARGE) range = 23;
		
		GetGame().GetWorld().QueryEntitiesBySphere(m_vPos, range, FillCompartments, null, EQueryEntitiesFlags.ALL);
				
	}
	
	protected bool FillCompartments(IEntity entity)
	{
		SCR_BaseCompartmentManagerComponent compartment = EPF_Component<SCR_BaseCompartmentManagerComponent>.Find(entity);
		if(!compartment) return true;		
		
		compartment.SpawnDefaultOccupants({ECompartmentType.TURRET});
		
		return true;
	}
	
	override int GetResources()
	{
		if(!m_Spawned) return 0;
		
		OVT_FactionComposition comp = GetCompositionConfig(m_sCompositionTag);	
		if(!comp) return 0;
		
		return comp.m_iCost;
	}
	
	override OVT_BaseUpgradeData Serialize()
	{
		OVT_BaseUpgradeData struct = new OVT_BaseUpgradeData();
		struct.type = ClassName();
		struct.pos = m_vPos;	
		struct.tag = m_sCompositionTag;	
		
		return struct;		
	}
	
	override bool Deserialize(OVT_BaseUpgradeData struct)
	{
		if(!m_BaseController.IsOccupyingFaction()) return true;
		m_vPos = struct.pos;
		
		Setup();
		
		return true;
	}
}