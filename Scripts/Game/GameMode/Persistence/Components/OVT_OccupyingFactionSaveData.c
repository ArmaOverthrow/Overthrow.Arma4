[EPF_ComponentSaveDataType(OVT_OccupyingFactionManager), BaseContainerProps()]
class OVT_OccupyingFactionSaveDataClass : EPF_ComponentSaveDataClass
{
};

[EDF_DbName.Automatic()]
class OVT_OccupyingFactionSaveData : EPF_ComponentSaveData
{	
	int m_iResources;
	float m_iThreat;
	ref array<ref OVT_BaseData> m_Bases;
	ref array<ref OVT_RadioTowerData> m_RadioTowers;
	string m_sOccupyingFactionKey;
	
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{		
		OVT_OccupyingFactionManager of = OVT_OccupyingFactionManager.Cast(component);
		
		m_iResources = of.m_iResources;
		m_iThreat = of.m_iThreat;		
		m_RadioTowers = of.m_RadioTowers;
		m_sOccupyingFactionKey = OVT_Global.GetConfig().m_sOccupyingFaction;
		
		m_Bases = new array<ref OVT_BaseData>;
		
		foreach(OVT_BaseData base : of.m_Bases)
		{
			base.upgrades.Clear();
			base.slotsFilled.Clear();
			if(base.IsOccupyingFaction())
			{
				OVT_BaseControllerComponent controller = of.GetBase(base.entId);
				foreach(OVT_BaseUpgrade upgrade : controller.m_aBaseUpgrades)
				{
					OVT_BaseUpgradeData data = upgrade.Serialize();
					if(data)
						base.upgrades.Insert(data);
				}
				foreach(EntityID id : controller.m_aSlotsFilled)
				{
					IEntity ent = GetGame().GetWorld().FindEntityByID(id);
					if(ent) base.slotsFilled.Insert(ent.GetOrigin());
				}
			}else{
				base.garrison.Clear();
				foreach(EntityID id : base.garrisonEntities)
				{
					SCR_AIGroup aigroup = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(id));
					if(!aigroup) continue;
					if(aigroup.GetAgentsCount() > 0)
					{
						ResourceName res = EPF_Utils.GetPrefabName(aigroup);
						base.garrison.Insert(res);					
					}
				}	
			}
			m_Bases.Insert(base);
		}
		
		return EPF_EReadResult.OK;
	}
	
	override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_OccupyingFactionManager of = OVT_OccupyingFactionManager.Cast(component);
		
		of.m_iResources = m_iResources;
		of.m_iThreat = m_iThreat;		
		of.m_bDistributeInitial = false;
		
		OVT_Global.GetConfig().SetOccupyingFaction(m_sOccupyingFactionKey);
		
		foreach(OVT_BaseData base : m_Bases)
		{
			OVT_BaseData existing = of.GetNearestBase(base.location);
			if(!existing) continue;
			if (base.faction < 0)
			{
				Print("Uninitialized faction found for base, setting to default.", LogLevel.WARNING);
				base.faction = OVT_Global().GetConfig().GetOccupyingFactionIndex();
			}
			existing.faction = base.faction;
			existing.upgrades = base.upgrades;
			existing.slotsFilled = base.slotsFilled;
			existing.garrison = base.garrison;
		}
		
		foreach(OVT_RadioTowerData tower : m_RadioTowers)
		{
			OVT_RadioTowerData existing = of.GetNearestRadioTower(tower.location);
			if(!existing) continue;
			existing.faction = tower.faction;
		}
		
		return EPF_EApplyResult.OK;
	}
}