class OVT_TownManagerComponentClass: OVT_ComponentClass
{
};

class OVT_TownModifierData : Managed
{
	int id;
	int timer;
}

class OVT_TownData : Managed
{
	int id;
	vector location;
	int population;
	int stability;
	int support;
	int faction;
	int size;
	
	ref array<ref OVT_TownModifierData> stabilityModifiers = {};
	ref array<ref OVT_TownModifierData> supportModifiers = {};
	
	vector gunDealerPosition;
	
	int SupportPercentage()
	{
		return Math.Round((support / population) * 100);
	}
	
	OVT_Faction ControllingFaction()
	{
		return OVT_Faction.Cast(GetGame().GetFactionManager().GetFactionByIndex(faction));
	}
	
	bool IsOccupyingFaction()
	{
		return faction == OVT_Global.GetConfig().GetOccupyingFactionIndex();
	}
}

class OVT_TownManagerComponent: OVT_Component
{
	[Attribute( defvalue: "1200", desc: "Range to search cities for houses")]
	int m_iCityRange;
	
	[Attribute( defvalue: "600", desc: "Range to search towns for houses")]
	int m_iTownRange;
	
	[Attribute( defvalue: "250", desc: "Range to search villages for houses")]
	int m_iVillageRange;
	
	[Attribute( defvalue: "2", desc: "Default occupants per house")]
	int m_iDefaultHouseOccupants;
	
	[Attribute( defvalue: "3", desc: "Occupants per villa house")]
	int m_iVillaOccupants;
	
	[Attribute( defvalue: "5", desc: "Occupants per town house")]
	int m_iTownOccupants;
	
	[Attribute("", UIWidgets.Object)]	
	ref array<ref OVT_TownModifierSystem> m_aTownModifiers;
	
	[Attribute("", UIWidgets.Object)]	
	ref array<ref string> m_aIgnoreTowns;
	
	protected int m_iTownCount=0;
	
	ref array<ref OVT_TownData> m_Towns;
	ref array<ref string> m_TownNames;
	
	protected IEntity m_EntitySearched;
	
	protected OVT_TownData m_CheckTown;
	
	protected ref array<ref EntityID> m_Houses;
	
	OVT_RealEstateManagerComponent m_RealEstate;
	
	ref ScriptInvoker<IEntity> m_OnTownControlChange = new ScriptInvoker<IEntity>;
	
	const int MODIFIER_FREQUENCY = 10000;
	protected int m_iSupportCounter = 0;
	protected const int SUPPORT_FREQUENCY = 6; // * MODIFIER_FREQUENCY
	 
	static OVT_TownManagerComponent s_Instance;	
	static OVT_TownManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_TownManagerComponent.Cast(pGameMode.FindComponent(OVT_TownManagerComponent));
		}

		return s_Instance;
	}
	
	void OVT_TownManagerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_Towns = new array<ref OVT_TownData>;
		m_TownNames = new array<ref string>;
	}
	
	void Init(IEntity owner)
	{		
		m_RealEstate = OVT_Global.GetRealEstate();		
		foreach(OVT_TownModifierSystem system : m_aTownModifiers)
		{
			system.Init();
		}		
		
		InitializeTowns();
		
		if(!Replication.IsServer()) return;
		SetupTowns();
	}
	
	void PostGameStart()
	{
		GetGame().GetCallqueue().CallLater(CheckUpdateModifiers, MODIFIER_FREQUENCY, true, GetOwner());		
		GetGame().GetCallqueue().CallLater(SpawnTownControllers, 0);
	}
	
	/*
	Town Modifier Systems
	*/
	OVT_TownModifierSystem GetModifierSystem(typename typeName)
	{
		foreach(OVT_TownModifierSystem system : m_aTownModifiers)
		{
			if(system.ClassName() == typeName.ToString())
			{
				return system;
			}
		}
		return null;
	}
	
	protected void CheckUpdateModifiers()
	{
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		
		m_iSupportCounter++;
		bool dosupport = false;
		if(m_iSupportCounter > SUPPORT_FREQUENCY)
		{
			m_iSupportCounter = 0;
			dosupport = true;
		}
		
		foreach(OVT_TownData town : m_Towns)
		{
			bool recalc = false;
			OVT_TownModifierSystem system = GetModifierSystem(OVT_TownStabilityModifierSystem);
			if(system && town.stabilityModifiers)
				if(system.OnTick(town.stabilityModifiers, town)) recalc = true;
			
			system = GetModifierSystem(OVT_TownSupportModifierSystem);
			if(system && town.supportModifiers)
				system.OnTick(town.supportModifiers, town);
						
			if(!Replication.IsServer()) continue;
			
			bool hasEnemyTower = false;
			bool hasFriendlyTower = false;
			foreach(OVT_RadioTowerData tower : of.m_RadioTowers)
			{				
				float dist = vector.Distance(town.location, tower.location);
				if(dist < m_Config.m_Difficulty.radioTowerRange)
				{
					if(tower.IsOccupyingFaction())
					{
						hasEnemyTower = true;
					}else{
						hasFriendlyTower = true;
					}
				}
			}			
			
			if(hasEnemyTower)
			{
				RemoveSupportModifierByName(town.id, "NearbyRadioTowerPositive");
				TryAddSupportModifierByName(town.id, "NearbyRadioTowerNegative");
			}else if(hasFriendlyTower)
			{
				RemoveSupportModifierByName(town.id, "NearbyRadioTowerNegative");
				TryAddSupportModifierByName(town.id, "NearbyRadioTowerPositive");
			}	
			
			bool hasEnemyBase = false;
			bool hasFriendlyBase = false;	
			
			foreach(OVT_BaseData base : of.m_Bases)
			{				
				float dist = vector.Distance(town.location, base.location);
				if(dist < m_Config.m_Difficulty.baseSupportRange)
				{
					if(base.IsOccupyingFaction())
					{
						hasEnemyBase = true;
					}else{
						hasFriendlyBase = true;
					}
				}
			}
			
			if(hasEnemyBase)
			{
				RemoveSupportModifierByName(town.id, "NearbyBasePositive");
				TryAddSupportModifierByName(town.id, "NearbyBaseNegative");
			}else if(hasFriendlyBase)
			{
				RemoveSupportModifierByName(town.id, "NearbyBaseNegative");
				TryAddSupportModifierByName(town.id, "NearbyBasePositive");
			}	
			
			if(recalc) RecalculateStability(town.id);
						
			if(dosupport)
			{
				//We always recalculate support modifiers and add/remove supporters, but less often
				RecalculateSupport(town.id);
			}
		}
	}
	
	void GetTotalPopulationStats(out int population, out int supporters)
	{
		population = 0;
		supporters = 0;
		foreach(OVT_TownData town : m_Towns)
		{
			population += town.population;
			supporters += town.support;
		}
	}

	void RemoveStabilityModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];		
		int i = GetModifierIndex(town.stabilityModifiers, index);
		if(i > -1)
		{
			town.stabilityModifiers.Remove(i);
			Rpc(RpcDo_RemoveStabilityModifier, townId, index);
			RecalculateStability(townId);
		}		
	}
	
	protected int GetModifierIndex(array<ref OVT_TownModifierData> modifiers, int id)
	{
		int i = -1;
		if(!modifiers) return -1;
		foreach(int index, OVT_TownModifierData data : modifiers)
		{
			if(!data) continue;
			if(data.id == id)
			{
				i = index;
				break;
			}
		}
		return i;
	}
		
	void TryAddStabilityModifierByName(int townId, string name)
	{
		OVT_TownModifierSystem system = GetModifierSystem(OVT_TownStabilityModifierSystem);
		system.TryAddByName(townId, name);
	}
	
	void TryAddSupportModifierByName(int townId, string name)
	{
		OVT_TownModifierSystem system = GetModifierSystem(OVT_TownSupportModifierSystem);
		system.TryAddByName(townId, name);
	}
	
	void RemoveSupportModifierByName(int townId, string name)
	{
		OVT_TownModifierSystem system = GetModifierSystem(OVT_TownSupportModifierSystem);
		system.RemoveByName(townId, name);
	}
	
	void TryAddStabilityModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];
		OVT_ModifierConfig mod = GetModifierSystem(OVT_TownStabilityModifierSystem).m_Config.m_aModifiers[index];
		//if(!(mod.flags & OVT_StabilityModifierFlags.ACTIVE)) return;
		if(GetModifierIndex(town.stabilityModifiers, index) == -1)
		{
			AddStabilityModifier(townId, index);
		}else if(mod.flags & OVT_ModifierFlags.STACKABLE)
		{
			//Is stackable, so stack it
			int num = 0;
			foreach(OVT_TownModifierData data : town.stabilityModifiers)
			{
				if(data.id == index) num++;
			}
			if(num < mod.stackLimit)
				AddStabilityModifier(townId, index);
		}else{
			//Is not stackable, reset timer
			int i = GetModifierIndex(town.stabilityModifiers, index);			
			town.stabilityModifiers[i].timer = mod.timeout;
			Rpc(RpcDo_ResetStabilityModifier, townId, index);
		}
	}
	
	void AddStabilityModifier(int townId, int index)
	{
		Rpc(RpcAsk_AddStabilityModifier, townId, index);
	}
	
		
	void RemoveSupportModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];		
		int i = GetModifierIndex(town.supportModifiers, index);
		if(i > -1)
		{
			town.supportModifiers.Remove(i);
			Rpc(RpcDo_RemoveSupportModifier, townId, index);
		}		
	}
	
	void TryAddSupportModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];
		OVT_ModifierConfig mod = GetModifierSystem(OVT_TownSupportModifierSystem).m_Config.m_aModifiers[index];
		//if(!(mod.flags & OVT_StabilityModifierFlags.ACTIVE)) return;
		if(GetModifierIndex(town.supportModifiers, index) == -1)
		{
			AddSupportModifier(townId, index);
		}else if(mod.flags & OVT_ModifierFlags.STACKABLE)
		{
			//Is stackable, so stack it
			int num = 0;
			foreach(OVT_TownModifierData data : town.supportModifiers)
			{
				if(data.id == index) num++;
			}
			if(num < mod.stackLimit)
				AddSupportModifier(townId, index);
		}else if(mod.timeout > 0){
			//Is not stackable, reset timer
			int i = GetModifierIndex(town.supportModifiers, index);			
			town.supportModifiers[i].timer = mod.timeout;
			Rpc(RpcDo_ResetSupportModifier, townId, index);
		}
	}
	
	void AddSupportModifier(int townId, int index)
	{
		Rpc(RpcAsk_AddSupportModifier, townId, index);
	}
	
	protected void RecalculateStability(int townId)
	{
		OVT_TownData town = m_Towns[townId];		
		OVT_TownModifierSystem system = GetModifierSystem(OVT_TownStabilityModifierSystem);
		
		int stab = system.Recalculate(town.stabilityModifiers);
		
		if(stab != town.stability)
		{
			town.stability = stab;
			Rpc(RpcDo_SetStability, townId, stab);
		}		
	}
	
	protected void RecalculateSupport(int townId)
	{
		Faction playerFaction = GetGame().GetFactionManager().GetFactionByKey(m_Config.m_sPlayerFaction);
		int playerFactionIndex = GetGame().GetFactionManager().GetFactionIndex(playerFaction);
		
		Faction occupyingFaction = GetGame().GetFactionManager().GetFactionByKey(m_Config.m_sOccupyingFaction);
		int occupyingFactionIndex = GetGame().GetFactionManager().GetFactionIndex(occupyingFaction);
				
		OVT_TownData town = m_Towns[townId];		
		OVT_TownModifierSystem system = GetModifierSystem(OVT_TownSupportModifierSystem);
		
		int newsupport = system.Recalculate(town.supportModifiers, town.support, 0, town.population);
		
		if(newsupport != town.support)
		{
			town.support = newsupport;
			Rpc(RpcDo_SetSupport, townId, newsupport);
		}
		
		//Villages only, see if we can peacefully flip it to the resistance (or back)
		if(town.size > 1) return;
		
		int support = town.SupportPercentage();		
		if(town.faction != playerFactionIndex && support >= 75 && town.stability >= 50)
		{
			//Chance this village will flip to the resistance
			if(support >= 85 || s_AIRandomGenerator.RandFloat01() < 0.5)
			{
				ChangeTownControl(town, playerFactionIndex);
			}			
		}else if(town.faction == playerFactionIndex && support < 25)
		{
			//Chance this village will flip back to the OF
			if(support < 15 || s_AIRandomGenerator.RandFloat01() < 0.5)
			{
				ChangeTownControl(town, occupyingFactionIndex);
			}
		}
	}
	
	void ResetSupport(OVT_TownData town)
	{
		town.support = 0;
		Rpc(RpcDo_SetSupport, town.id, 0);
	}
	
	void ChangeTownControl(OVT_TownData town, int faction)
	{
		town.faction = faction;
		m_OnTownControlChange.Invoke(town);
		Rpc(RpcDo_SetTownFaction, town.id, faction);
		string type = "Village";
		if(town.size == 2) type = "Town";
		if(town.size == 3) type = "City";
		
		string factionType = "Resistance";
		if(faction != m_Config.GetPlayerFactionIndex()) factionType = "Occupying";
		
		OVT_Global.GetPlayers().HintMessageAll(type + "Controlled" + factionType, town.id);		
	}
	
	IEntity GetRandomHouse()
	{
		m_Houses = new array<ref EntityID>;
		OVT_TownData town;
		IEntity house;
		int i = 0;
		
		while(!house && i < 20)
		{
			i++;
			town = m_Towns.GetRandomElement();
			GetGame().GetWorld().QueryEntitiesBySphere(town.location, m_iCityRange, CheckHouseAddToArray, FilterHouseEntities, EQueryEntitiesFlags.STATIC);
			if(m_Houses.Count() == 0) continue;
			house = GetGame().GetWorld().FindEntityByID(m_Houses.GetRandomElement());
			if(m_RealEstate.IsOwned(house.GetID())) house = null;			
		}
				
		return house;
	}
	
	IEntity GetRandomStartingHouse()
	{
		m_Houses = new array<ref EntityID>;
		OVT_TownData town;
		IEntity house;
		int i = 0;
		float dist;
		
		while(!house && i < 20)
		{
			i++;
			town = m_Towns.GetRandomElement();
				
			GetGame().GetWorld().QueryEntitiesBySphere(town.location, m_iCityRange, CheckHouseAddToArray, FilterStartingHouseEntities, EQueryEntitiesFlags.STATIC);
			if(m_Houses.Count() == 0) continue;
			house = GetGame().GetWorld().FindEntityByID(m_Houses.GetRandomElement());
			if(m_RealEstate.IsOwned(house.GetID())) house = null;			
		}
				
		return house;
	}
	
	IEntity GetNearestHouse(vector pos)
	{
		m_Houses = new array<ref EntityID>;		
		GetGame().GetWorld().QueryEntitiesBySphere(pos, 25, CheckHouseAddToArray, FilterHouseEntities, EQueryEntitiesFlags.STATIC);
		
		float nearest = 26;
		IEntity nearestEnt;		
		
		foreach(EntityID id : m_Houses)
		{			
			IEntity ent = GetGame().GetWorld().FindEntityByID(id);
			float dist = vector.Distance(ent.GetOrigin(), pos);
			if(dist < nearest)
			{
				nearest = dist;
				nearestEnt = ent;
			}
		}
		
		return nearestEnt;
	}
	
	IEntity GetNearestStartingHouse(vector pos)
	{
		m_Houses = new array<ref EntityID>;		
		GetGame().GetWorld().QueryEntitiesBySphere(pos, 25, CheckHouseAddToArray, FilterStartingHouseEntities, EQueryEntitiesFlags.STATIC);
		
		float nearest = 26;
		IEntity nearestEnt;		
		
		foreach(EntityID id : m_Houses)
		{			
			IEntity ent = GetGame().GetWorld().FindEntityByID(id);
			float dist = vector.Distance(ent.GetOrigin(), pos);
			if(dist < nearest)
			{
				nearest = dist;
				nearestEnt = ent;
			}
		}
		
		return nearestEnt;
	}
	
	IEntity GetRandomHouseInTown(OVT_TownData town)
	{
		m_Houses = new array<ref EntityID>;		
		
		GetGame().GetWorld().QueryEntitiesBySphere(town.location, m_iTownRange, CheckHouseAddToArray, FilterHouseEntities, EQueryEntitiesFlags.STATIC);
		
		return GetGame().GetWorld().FindEntityByID(m_Houses.GetRandomElement());
	}
	
	OVT_TownData GetNearestTown(vector pos)
	{
		OVT_TownData nearestTown;
		float nearest = 9999999;
		foreach(OVT_TownData town : m_Towns)
		{
			float distance = vector.Distance(town.location, pos);
			if(distance < nearest){
				nearest = distance;
				nearestTown = town;
			}
		}
		return nearestTown;
	}
	
	float GetTownRange(OVT_TownData town)
	{
		float range = m_iCityRange;
		if(town.size == 1) range = m_iVillageRange;
		if(town.size == 2) range = m_iTownRange;
		return range;
	}
	
	OVT_TownData GetNearestTownInRange(vector pos)
	{
		foreach(OVT_TownData town : m_Towns)
		{
			float distance = vector.Distance(town.location, pos);
			int range = m_iCityRange;
			if(town.size == 2) range = m_iTownRange;
			if(town.size == 1) range = m_iVillageRange;
			if(distance <= range){
				return town;
			}
		}
		return null;
	}
	
	SCR_MapDescriptorComponent GetNearestTownMarker(vector pos)
	{	
		m_EntitySearched = null;	
		GetGame().GetWorld().QueryEntitiesBySphere(pos, 5, null, FindTownMarker, EQueryEntitiesFlags.STATIC);
		if(!m_EntitySearched) return null;
		
		return SCR_MapDescriptorComponent.Cast(m_EntitySearched.FindComponent(SCR_MapDescriptorComponent));
	}
	
	OVT_TownData GetTown(int townId)
	{
		return m_Towns[townId];
	}
	
	string GetTownName(int townId)
	{
		if(m_TownNames[townId] == "")
		{
			SCR_MapDescriptorComponent desc = GetNearestTownMarker(m_Towns[townId].location);
			m_TownNames[townId] = desc.Item().GetDisplayName();
		}
		return m_TownNames[townId];
	}
	
	SCR_MapDescriptorComponent GetNearestBusStop(vector pos)
	{	
		m_EntitySearched = null;	
		GetGame().GetWorld().QueryEntitiesBySphere(pos, 15, null, FindBusStop, EQueryEntitiesFlags.STATIC);
		if(!m_EntitySearched) return null;
		
		return SCR_MapDescriptorComponent.Cast(m_EntitySearched.FindComponent(SCR_MapDescriptorComponent));
	}
	
	void GetTownsWithinDistance(vector pos, float maxDistance, out array<ref OVT_TownData> towns)
	{
		foreach(OVT_TownData town : m_Towns)
		{
			float distance = vector.Distance(town.location, pos);
			if(distance < maxDistance){
				towns.Insert(town);
			}
		}
	}
	
	protected void InitializeTowns()
	{
		#ifdef OVERTHROW_DEBUG
		Print("Finding cities, towns and villages");
		#endif	
		
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, CheckCityTownAddPopulation, FilterCityTownEntities, EQueryEntitiesFlags.STATIC);
		
	}
	
	protected void SpawnTownControllers()
	{
		foreach(OVT_TownData town : m_Towns)
		{
			EntitySpawnParams spawnParams = new EntitySpawnParams;
			spawnParams.TransformMode = ETransformMode.WORLD;		
			spawnParams.Transform[3] = town.location;
			IEntity controller = GetGame().SpawnEntityPrefab(Resource.Load(m_Config.m_pTownControllerPrefab), GetGame().GetWorld(), spawnParams);
		}
	}
	
	protected bool CheckCityTownAddPopulation(IEntity entity)
	{	
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (mapdesc){
			ProcessTown(entity, mapdesc);
		}
		return true;
	}
	
	protected void ProcessTown(IEntity entity, MapDescriptorComponent mapdesc)
	{
		OVT_TownData town = new OVT_TownData();
		
		Faction faction = GetGame().GetFactionManager().GetFactionByKey(m_Config.m_sOccupyingFaction);
			
		town.id = m_iTownCount;
		town.location = entity.GetOrigin();
		town.population = 0;
		town.support = 0;
		town.faction = GetGame().GetFactionManager().GetFactionIndex(faction);
		town.stability = 100;
		
		m_iTownCount++;
		
		if(mapdesc.GetBaseType() == EMapDescriptorType.MDT_NAME_VILLAGE) town.size = 1;
		if(mapdesc.GetBaseType() == EMapDescriptorType.MDT_NAME_TOWN) town.size = 2;
		if(mapdesc.GetBaseType() == EMapDescriptorType.MDT_NAME_CITY) town.size = 3;
		
		m_Towns.Insert(town);
		m_TownNames.Insert(mapdesc.Item().GetDisplayName());
	}
	
	protected void SetupTowns()
	{
		foreach(OVT_TownData town : m_Towns)
		{
			m_CheckTown = town;
		
			int range = m_iTownRange;
			if(town.size == 1) range = m_iVillageRange;
			if(town.size == 3) range = m_iCityRange;
			
			GetGame().GetWorld().QueryEntitiesBySphere(town.location, range, CheckHouseAddPopulation, FilterHouseEntities, EQueryEntitiesFlags.STATIC);
		}
	}
	
	protected bool FilterCityTownEntities(IEntity entity) 
	{		
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (mapdesc){	
			if(m_aIgnoreTowns.Find(mapdesc.Item().GetDisplayName()) > -1) return false;
			int type = mapdesc.GetBaseType();
			if(type == EMapDescriptorType.MDT_NAME_CITY) return true;
			if(type == EMapDescriptorType.MDT_NAME_VILLAGE) return true;
			if(type == EMapDescriptorType.MDT_NAME_TOWN) return true;
		}
				
		return false;		
	}
	
	protected bool FindTownMarker(IEntity entity) 
	{		
		bool got = false;
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (mapdesc){	
			int type = mapdesc.GetBaseType();
			if(type == EMapDescriptorType.MDT_NAME_CITY) got = true;
			if(type == EMapDescriptorType.MDT_NAME_VILLAGE) got = true;
			if(type == EMapDescriptorType.MDT_NAME_TOWN) got = true;
		}
		
		if(got)
		{
			m_EntitySearched = entity;
		}
				
		return false;		
	}
	
	protected bool FindBusStop(IEntity entity) 
	{		
		bool got = false;
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (mapdesc){	
			int type = mapdesc.GetBaseType();
			if(type == EMapDescriptorType.MDT_BUSSTOP) got = true;
		}
		
		if(got)
		{
			m_EntitySearched = entity;
		}
				
		return false;		
	}
	

	void TakeSupportersFromNearestTown(vector pos, int num = 1)
	{
		OVT_TownData town = GetNearestTown(pos);
		if(town.support < num || town.population < num) return;
		RpcDo_SetSupport(town.id, town.support - num);
		Rpc(RpcDo_SetSupport, town.id, town.support - num);
		
		RpcDo_SetPopulation(town.id, town.population - num);
		Rpc(RpcDo_SetPopulation, town.id, town.population - num);
	}
	
	protected bool CheckHouseAddPopulation(IEntity entity)
	{
		VObject mesh = entity.GetVObject();
		if(mesh){
			string res = mesh.GetResourceName();
			int pop = m_iDefaultHouseOccupants;
			if(res.IndexOf("/Villa/") > -1)
				pop = m_iVillaOccupants;
			if(res.IndexOf("/Town/") > -1)
				pop = m_iTownOccupants;
			
			m_CheckTown.population += pop;
		}
				
		return true;
	}
	
	protected bool CheckHouseAddToArray(IEntity entity)
	{
		m_Houses.Insert(entity.GetID());
				
		return true;
	}
	
	protected bool FilterHouseEntities(IEntity entity) 
	{
		if(entity.ClassName() == "SCR_DestructibleBuildingEntity"){
			ResourceName prefab = entity.GetPrefabData().GetPrefabName();
			foreach(string s : m_Config.m_aStartingHouseFilters)
			{
				if(prefab.IndexOf(s) > -1) return false;
			}	
			
			VObject mesh = entity.GetVObject();
			
			if(mesh){
				string res = mesh.GetResourceName();
				if(res.IndexOf("/Military/") > -1) return false;
				if(res.IndexOf("/Industrial/") > -1) return false;
				if(res.IndexOf("/Recreation/") > -1) return false;
				
				if(res.IndexOf("/Houses/") > -1){
					if(res.IndexOf("_ruin") > -1) return false;
					if(res.IndexOf("/Shed/") > -1) return false;
					if(res.IndexOf("/Garage/") > -1) return false;
					if(res.IndexOf("/HouseAddon/") > -1) return false;
					return true;
				}
					
			}
		}
		return false;
	}
	
	protected bool FilterStartingHouseEntities(IEntity entity) 
	{
		if(entity.ClassName() == "SCR_DestructibleBuildingEntity"){
			ResourceName res = entity.GetPrefabData().GetPrefabName();
			if(res.IndexOf("_furniture") > -1) return false;
			foreach(string s : m_Config.m_aStartingHouseFilters)
			{
				if(res.IndexOf(s) > -1) return true;
			}			
		}
		return false;
	}
	
	//RPC Methods
	
	override bool RplSave(ScriptBitWriter writer)
	{	
			
		//Send JIP towns
		int length = m_Towns.Count();
		Print("Writing " + length + " towns");
		writer.WriteInt(length); 
		for(int i; i<length; i++)
		{			
			OVT_TownData town = m_Towns[i];
			Print("Writing town ID " + town.id);
			writer.WriteVector(town.location);
			writer.WriteInt(town.population);
			writer.WriteInt(town.stability);
			writer.WriteInt(town.support);
			writer.WriteInt(town.faction);
			
			int count = town.stabilityModifiers.Count();
			Print("Writing " + count + " stability modifiers");
			writer.WriteInt(count);
			for(int t; t<count; t++)
			{
				writer.WriteInt(town.stabilityModifiers[t].id);
				writer.WriteInt(town.stabilityModifiers[t].timer);
			}
			count = town.supportModifiers.Count();
			Print("Writing " + count + " support modifiers");
			writer.WriteInt(count);
			for(int t; t<count; t++)
			{
				writer.WriteInt(town.supportModifiers[t].id);
				writer.WriteInt(town.supportModifiers[t].timer);
			}						
		}
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{		
				
		//Recieve JIP towns
		int length, modlength;
		vector pos;
		
		if (!reader.ReadInt(length)) return false;
		Print("Replicating " + length + " towns");
		for(int i; i<length; i++)
		{
			if (!reader.ReadVector(pos)) return false;
			OVT_TownData town = GetNearestTown(pos);
			
			Print("Replicating town " + town.id);			
				
			if (!reader.ReadInt(town.population)) return false;		
			if (!reader.ReadInt(town.stability)) return false;		
			if (!reader.ReadInt(town.support)) return false;		
			if (!reader.ReadInt(town.faction)) return false;
				
			int stabilitylength;
			if (!reader.ReadInt(stabilitylength)) return false;
			Print("Replicating " + stabilitylength + " stability mods");
			for(int t = 0; t<stabilitylength; t++)
			{
				OVT_TownModifierData mod = new OVT_TownModifierData;
				if (!reader.ReadInt(mod.id)) return false;
				if (!reader.ReadInt(mod.timer)) return false;
				town.stabilityModifiers.Insert(mod);
			}	
			int supportlength;
			if (!reader.ReadInt(supportlength)) return false;
			Print("Replicating " + supportlength + " support mods");
			for(int t = 0; t<supportlength; t++)
			{
				OVT_TownModifierData mod = new OVT_TownModifierData;
				if (!reader.ReadInt(mod.id)) return false;
				if (!reader.ReadInt(mod.timer)) return false;
				town.supportModifiers.Insert(mod);
			}
		}
		return true;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddStabilityModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];
		OVT_TownModifierSystem system = GetModifierSystem(OVT_TownStabilityModifierSystem);		
		OVT_ModifierConfig mod = system.m_Config.m_aModifiers[index];
		
		OVT_TownModifierData data = new OVT_TownModifierData;
		data.id = index;
		data.timer = mod.timeout;
		
		town.stabilityModifiers.Insert(data);
		
		RecalculateStability(townId);		
		Rpc(RpcDo_AddStabilityModifier, townId, index, mod.timeout);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddSupportModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];
		OVT_TownModifierSystem system = GetModifierSystem(OVT_TownSupportModifierSystem);		
		OVT_ModifierConfig mod = system.m_Config.m_aModifiers[index];
		
		OVT_TownModifierData data = new OVT_TownModifierData;
		data.id = index;
		data.timer = mod.timeout;
		
		town.supportModifiers.Insert(data);
				
		Rpc(RpcDo_AddSupportModifier, townId, index, mod.timeout);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetStability(int townId, int value)
	{
		OVT_TownData town = m_Towns[townId];
		town.stability = value;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetSupport(int townId, int value)
	{
		OVT_TownData town = m_Towns[townId];
		town.support = value;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetPopulation(int townId, int value)
	{
		OVT_TownData town = m_Towns[townId];
		town.population = value;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_AddStabilityModifier(int townId, int index, int timer)
	{
		OVT_TownData town = m_Towns[townId];
		OVT_TownModifierData data = new OVT_TownModifierData;
		data.id = index;
		data.timer = timer;
		
		town.stabilityModifiers.Insert(data);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_AddSupportModifier(int townId, int index, int timer)
	{
		OVT_TownData town = m_Towns[townId];
		
		OVT_TownModifierData data = new OVT_TownModifierData;
		data.id = index;
		data.timer = timer;
		
		town.supportModifiers.Insert(data);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RemoveStabilityModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];		
		int i = GetModifierIndex(town.stabilityModifiers, index);
		if(i > -1)
		{
			town.stabilityModifiers.Remove(i);
		}
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RemoveSupportModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];		
		int i = GetModifierIndex(town.supportModifiers, index);
		if(i > -1)
		{
			town.supportModifiers.Remove(i);
		}
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_ResetStabilityModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];
		OVT_TownModifierSystem system = GetModifierSystem(OVT_TownStabilityModifierSystem);		
		OVT_ModifierConfig mod = system.m_Config.m_aModifiers[index];
		int i = GetModifierIndex(town.stabilityModifiers, index);
		if(i > -1)
		{
			town.stabilityModifiers[i].timer = mod.timeout;
		}
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_ResetSupportModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];
		OVT_TownModifierSystem system = GetModifierSystem(OVT_TownSupportModifierSystem);		
		OVT_ModifierConfig mod = system.m_Config.m_aModifiers[index];
		int i = GetModifierIndex(town.supportModifiers, index);
		if(i > -1)
		{
			town.supportModifiers[i].timer = mod.timeout;
		}
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetTownFaction(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];
		town.faction = index;
	}
	
	
	void ~OVT_TownManagerComponent()
	{
		if(m_Towns)
		{
			m_Towns.Clear();
			m_Towns = null;
		}
		if(m_TownNames)
		{
			m_TownNames.Clear();
			m_TownNames = null;
		}
		if(m_Houses)
		{
			m_Houses.Clear();
			m_Houses = null;
		}
	}
}