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
		OVT_ResistanceFactionManager rf = OVT_Global.GetResistanceFaction();
		
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		FactionManager factions = GetGame().GetFactionManager();
		foreach(OVT_BaseData base : of.m_Bases)
		{
			OVT_BaseDataStruct b = new OVT_BaseDataStruct();
			b.pos = base.location;
			b.faction = factions.GetFactionByIndex(base.faction).GetFactionKey();
			
			if(base.IsOccupyingFaction())
			{
				OVT_BaseControllerComponent controller = of.GetBase(base.entId);
				foreach(OVT_BaseUpgrade upgrade : controller.m_aBaseUpgrades)
				{
					b.upgrades.Insert(upgrade.Serialize(rdb));
				}
			}else{
				foreach(EntityID id : base.garrison)
				{
					SCR_AIGroup aigroup = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(id));
					if(aigroup && aigroup.GetAgentsCount() > 0)
					{
						string resource = aigroup.GetPrefabData().GetPrefabName();
	
						int res = rdb.Find(resource);
						if(res == -1)
						{
							rdb.Insert(resource);
							res = rdb.Count() - 1;
						}		
						b.garrison.Insert(res);
					}
				}							
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
	ref array<int> garrison = {};
	
	void OVT_BaseDataStruct()
	{
		RegV("pos");
		RegV("faction");
		RegV("upgrades");
		RegV("garrison");
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