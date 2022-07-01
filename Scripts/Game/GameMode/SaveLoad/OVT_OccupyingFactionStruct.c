[BaseContainerProps()]
class OVT_OccupyingFactionStruct : OVT_BaseSaveStruct
{
	ref array<ref OVT_BaseDataStruct> bases = {};
	int resources;
	float threat;
	
	override bool Serialize()
	{
		bases.Clear();
		
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		FactionManager factions = GetGame().GetFactionManager();
		foreach(OVT_BaseData base : of.m_Bases)
		{
			OVT_BaseDataStruct b = new OVT_BaseDataStruct();
			b.pos = base.location;
			b.faction = factions.GetFactionByIndex(base.faction).GetFactionKey();
			OVT_BaseControllerComponent controller = of.GetBase(base.entId);
			foreach(OVT_BaseUpgrade upgrade : controller.m_aBaseUpgrades)
			{
				b.upgrades.Insert(upgrade.Serialize(rdb));
			}
			bases.Insert(b);
		}
		
		resources = of.m_iResources;
		threat = of.m_iThreat;
		
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
			foreach(OVT_BaseDataStruct s : bases)
			{
				float dist = vector.Distance(data.location, s.pos);
				if(dist < 100)
				{
					struct = s;
					break;
				}
			}
			
			if(struct)
			{
				Faction fac = GetGame().GetFactionManager().GetFactionByKey(struct.faction);
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
		RegV("bases");
		RegV("resources");
		RegV("threat");
	}
}

class OVT_BaseDataStruct : SCR_JsonApiStruct
{
	vector pos;
	string faction;
	ref array<ref OVT_BaseUpgradeStruct> upgrades = {};
	
	void OVT_BaseDataStruct()
	{
		RegV("pos");
		RegV("faction");
		RegV("upgrades");
	}
}

class OVT_BaseUpgradeStruct : SCR_JsonApiStruct
{
	string type;
	int resources;
	ref array<ref OVT_BaseUpgradeGroupStruct> groups = {};
	ref array<ref OVT_VehicleStruct> vehicles = {};
	string tag = "";
		
	void OVT_BaseUpgradeStruct()
	{
		RegV("type");
		RegV("resources");
		RegV("groups");
		RegV("vehicles");
		RegV("tag");
	}
}

class OVT_BaseUpgradeGroupStruct : SCR_JsonApiStruct
{
	int type;
	vector pos;
	
	void OVT_BaseUpgradeGroupStruct()
	{
		RegV("type");
		RegV("pos");
	}
}