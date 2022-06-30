[BaseContainerProps()]
class OVT_OccupyingFactionStruct : SCR_JsonApiStruct
{
	ref array<ref OVT_BaseDataStruct> m_aBases = {};
	int m_iResources;
	float m_fThreat;
	
	override bool Serialize()
	{
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		FactionManager factions = GetGame().GetFactionManager();
		foreach(OVT_BaseData base : of.m_Bases)
		{
			OVT_BaseDataStruct b = new OVT_BaseDataStruct();
			b.m_vLocation = base.location;
			b.m_sFaction = factions.GetFactionByIndex(base.faction).GetFactionKey();
			OVT_BaseControllerComponent controller = of.GetBase(base.entId);
			foreach(OVT_BaseUpgrade upgrade : controller.m_aBaseUpgrades)
			{
				b.m_aUpgrades.Insert(upgrade.Serialize());
			}
			m_aBases.Insert(b);
		}
		
		m_iResources = of.m_iResources;
		m_fThreat = of.m_iThreat;
		
		return true;
	}
	
	override bool Deserialize()
	{		
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		of.m_LoadStruct = this;
		of.m_bDistributeInitial = false;
		foreach(OVT_BaseData data : of.m_Bases)
		{
			//Load data from save if we need to
			OVT_BaseDataStruct struct;
			foreach(OVT_BaseDataStruct s : m_aBases)
			{
				float dist = vector.Distance(data.location, s.m_vLocation);
				if(dist < 100)
				{
					struct = s;
					break;
				}
			}
			
			if(struct)
			{
				Faction fac = GetGame().GetFactionManager().GetFactionByKey(struct.m_sFaction);
				if(fac)
				{
					data.faction = GetGame().GetFactionManager().GetFactionIndex(fac);
				}	
							
			}
		}
			
		return true;
	}
	
	void OVT_OccupyingFactionStruct()
	{
		RegV("m_aBases");
		RegV("m_iResources");
		RegV("m_fThreat");
	}
}

class OVT_BaseDataStruct : SCR_JsonApiStruct
{
	vector m_vLocation;
	string m_sFaction;
	ref array<ref OVT_BaseUpgradeStruct> m_aUpgrades = {};
	
	void OVT_BaseDataStruct()
	{
		RegV("m_vLocation");
		RegV("m_sFaction");
		RegV("m_aUpgrades");
	}
}