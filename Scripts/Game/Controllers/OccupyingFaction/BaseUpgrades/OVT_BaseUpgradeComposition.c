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
		
		if(ent) return comp.m_iCost;
		return 0;
	}	
	
	override OVT_BaseUpgradeStruct Serialize()
	{
		OVT_BaseUpgradeStruct struct = super.Serialize();
		struct.m_sTag = m_sCompositionTag;
		return struct;
	}
	
	override int GetResources()
	{
		if(!m_Spawned) return 0;
		
		OVT_FactionComposition comp = GetCompositionConfig(m_sCompositionTag);	
		if(!comp) return 0;
		
		return comp.m_iCost;
	}
}