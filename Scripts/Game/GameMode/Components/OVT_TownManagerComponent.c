class OVT_TownManagerComponentClass: OVT_ComponentClass
{
};

class OVT_TownData : Managed
{
	int id;
	vector location;
	int population;
	int stability;
	int support;
	int faction;
	int size;
	ref array<ref int> stabilityModifiers = new array<ref int>;
	ref array<ref int> supportModifiers = new array<ref int>;
	
	ref array<ref int> stabilityModifierTimers = new array<ref int>;
	ref array<ref int> supportModifierTimers = new array<ref int>;
	
	int SupportPercentage()
	{
		return Math.Round((support / population) * 100);
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
	
	protected int m_iTownCount=0;
	
	ref array<ref OVT_TownData> m_Towns;
	protected IEntity m_EntitySearched;
	
	protected OVT_TownData m_CheckTown;
	
	protected ref array<ref EntityID> m_Houses;
	
	OVT_RealEstateManagerComponent m_RealEstate;
	
	ref OVT_StabilityModifiersConfig m_StabilityModifiers;
	ref OVT_SupportModifiersConfig m_SupportModifiers;
	
	protected const int MODIFIER_FREQUENCY = 10000;
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
	
	void OVT_TownManagerComponent()
	{
		m_Towns = new array<ref OVT_TownData>;
	}
	
	void Init(IEntity owner)
	{		
		m_RealEstate = OVT_Global.GetRealEstate();		
		LoadConfig();
		
		if(!Replication.IsServer()) return;
		InitializeTowns();
	}
	
	void PostGameStart()
	{
		GetGame().GetCallqueue().CallLater(CheckUpdateModifiers, MODIFIER_FREQUENCY, true, GetOwner());		
		GetGame().GetCallqueue().CallLater(SpawnTownControllers, 0);
	}
	
	protected void CheckUpdateModifiers()
	{
		m_iSupportCounter++;
		bool dosupport = false;
		if(m_iSupportCounter > SUPPORT_FREQUENCY)
		{
			m_iSupportCounter = 0;
			dosupport = true;
		}
		
		foreach(OVT_TownData town : m_Towns)
		{
			//Check if we need to time out any stability modifiers
			bool recalc = false;
			array<int> remove = new array<int>;
			foreach(int i, int index : town.stabilityModifiers)
			{
				town.stabilityModifierTimers[i] = town.stabilityModifierTimers[i] - MODIFIER_FREQUENCY / 1000;
				if(town.stabilityModifierTimers[i] <= 0)
				{
					recalc = true;
					remove.Insert(i);
				}else{
					OVT_StabilityModifierConfig mod = m_StabilityModifiers.m_aStabilityModifiers[index];
					if(mod.handler){
						if(!mod.handler.OnActiveTick(town))
						{
							remove.Insert(i);
							recalc = true;
						}
					}
				}
			}
			
			//Call stability modifier OnTicks
			foreach(int i, OVT_StabilityModifierConfig config : m_StabilityModifiers.m_aStabilityModifiers)
			{
				if(config.handler)
				{
					config.handler.OnTick(town);
				}
			}
			
			//Remove modifiers tagged for removal
			foreach(int i : remove)
			{
				town.stabilityModifierTimers.Remove(i);
				town.stabilityModifiers.Remove(i);
			}
			if(recalc && Replication.IsServer()) RecalculateStability(town.id);
			
			//Check if we need to time out any support modifiers
			remove.Clear();
			foreach(int i, int index : town.supportModifiers)
			{
				town.supportModifierTimers[i] = town.supportModifierTimers[i] - MODIFIER_FREQUENCY / 1000;
				if(town.supportModifierTimers[i] <= 0)
				{					
					remove.Insert(i);
				}else{
					OVT_SupportModifierConfig mod = m_SupportModifiers.m_aSupportModifiers[index];
					if(mod.handler){
						if(!mod.handler.OnActiveTick(town))
						{
							remove.Insert(i);
						}
					}
				}
			}
			
			//Call support modifier OnTicks
			foreach(int i, OVT_SupportModifierConfig config : m_SupportModifiers.m_aSupportModifiers)
			{
				if(config.handler)
				{
					config.handler.OnTick(town);
				}
			}
			
			//Remove modifiers tagged for removal
			foreach(int i : remove)
			{
				town.supportModifiers.Remove(i);
				town.supportModifierTimers.Remove(i);
			}
			
			if(dosupport && Replication.IsServer())
			{
				//We always recalculate support modifiers and add/remove supporters, but less often
				RecalculateSupport(town.id);
			}
		}
	}
	
	protected void LoadConfig()
	{
		Resource holder = BaseContainerTools.LoadContainer("{FD430627EAC5BDBC}Configs/Modifiers/stabilityModifiers.conf");
		if (holder)		
		{
			OVT_StabilityModifiersConfig obj = OVT_StabilityModifiersConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(holder.GetResource().ToBaseContainer()));
			if(obj)
			{
				m_StabilityModifiers = obj;
				InitStability();
			}
		}
		
		holder = BaseContainerTools.LoadContainer("{AB1726E5220F21F1}Configs/Modifiers/supportModifiers.conf");
		if (holder)		
		{
			OVT_SupportModifiersConfig obj = OVT_SupportModifiersConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(holder.GetResource().ToBaseContainer()));
			if(obj)
			{
				m_SupportModifiers = obj;
				InitSupport();
			}
		}
	}
	
	protected void InitStability()
	{
		if(!Replication.IsServer()) return;
		foreach(int i, OVT_StabilityModifierConfig config : m_StabilityModifiers.m_aStabilityModifiers)
		{
			if(config.handler)
			{
				config.handler.m_sName = config.name;
				config.handler.m_iIndex = i;
				config.handler.Init();
				config.handler.OnPostInit();
				
				foreach(OVT_TownData town : m_Towns)
				{
					config.handler.OnStart(town);
				}
			}
		}
	}
	
	protected void InitSupport()
	{
		if(!Replication.IsServer()) return;
		foreach(int i, OVT_SupportModifierConfig config : m_SupportModifiers.m_aSupportModifiers)
		{
			if(config.handler)
			{
				config.handler.m_sName = config.name;
				config.handler.m_iIndex = i;
				config.handler.Init();
				config.handler.OnPostInit();
				
				foreach(OVT_TownData town : m_Towns)
				{
					config.handler.OnStart(town);
				}
			}
		}
	}
	
	void TryAddStabilityModifierByName(int townId, string name)
	{
		foreach(int i, OVT_StabilityModifierConfig config : m_StabilityModifiers.m_aStabilityModifiers)
		{
			if(config.name == name)
			{
				TryAddStabilityModifier(townId, i);
				return;
			}
		}
	}
	
	void RemoveStabilityModifierByName(int townId, string name)
	{
		foreach(int i, OVT_StabilityModifierConfig config : m_StabilityModifiers.m_aStabilityModifiers)
		{
			if(config.name == name)
			{
				RemoveStabilityModifier(townId, i);
				return;
			}
		}
	}
	
	void RemoveStabilityModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];		
		int i = town.stabilityModifiers.Find(index);
		if(i > -1)
		{
			town.stabilityModifierTimers.Remove(i);
			town.stabilityModifiers.Remove(i);
			Rpc(RpcDo_RemoveStabilityModifier, townId, index);
			RecalculateStability(townId);
		}		
	}
	
	void TryAddStabilityModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];
		OVT_StabilityModifierConfig mod = m_StabilityModifiers.m_aStabilityModifiers[index];
		//if(!(mod.flags & OVT_StabilityModifierFlags.ACTIVE)) return;
		if(!town.stabilityModifiers.Contains(index))
		{
			AddStabilityModifier(townId, index);
		}else if(mod.flags & OVT_StabilityModifierFlags.STACKABLE)
		{
			//Is stackable, so stack it
			AddStabilityModifier(townId, index);
		}else{
			//Is not stackable, reset timer
			int i = town.stabilityModifiers.Find(index);
			town.stabilityModifierTimers[i] = mod.timeout;
			Rpc(RpcDo_ResetStabilityModifier, townId, index);
		}
	}
	
	void AddStabilityModifier(int townId, int index)
	{
		Rpc(RpcAsk_AddStabilityModifier, townId, index);
	}
	
	
	
	void TryAddSupportModifierByName(int townId, string name)
	{
		foreach(int i, OVT_SupportModifierConfig config : m_SupportModifiers.m_aSupportModifiers)
		{
			if(config.name == name)
			{
				TryAddSupportModifier(townId, i);
				return;
			}
		}
	}
	
	void RemoveSupportModifierByName(int townId, string name)
	{
		foreach(int i, OVT_SupportModifierConfig config : m_SupportModifiers.m_aSupportModifiers)
		{
			if(config.name == name)
			{
				RemoveSupportModifier(townId, i);
				return;
			}
		}
	}
	
	void RemoveSupportModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];		
		int i = town.supportModifiers.Find(index);
		if(i > -1)
		{
			town.supportModifierTimers.Remove(i);
			town.supportModifiers.Remove(i);
			Rpc(RpcDo_RemoveSupportModifier, townId, index);
		}		
	}
	
	void TryAddSupportModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];
		OVT_SupportModifierConfig mod = m_SupportModifiers.m_aSupportModifiers[index];
		//if(!(mod.flags & OVT_StabilityModifierFlags.ACTIVE)) return;
		if(!town.supportModifiers.Contains(index))
		{
			AddSupportModifier(townId, index);
		}else if(mod.flags & OVT_SupportModifierFlags.STACKABLE)
		{
			//Is stackable, so stack it
			int num = 0;
			foreach(int i : town.supportModifiers)
			{
				if(i == index) num++;
			}
			if(num < mod.stackLimit)
				AddSupportModifier(townId, index);
		}else{
			//Is not stackable, reset timer
			int i = town.supportModifiers.Find(index);
			town.supportModifierTimers[i] = mod.timeout;
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
		
		float newStability = 100;
		foreach(int index : town.stabilityModifiers)
		{
			OVT_StabilityModifierConfig mod = m_StabilityModifiers.m_aStabilityModifiers[index];
			newStability += mod.baseEffect;
		}
		if(newStability > 100) newStability = 100;
		if(newStability < 0) newStability = 0;
		int stab = Math.Round(newStability);
		
		if(stab != town.stability)
		{
			town.stability = stab;
			Rpc(RpcDo_SetStability, townId, stab);
		}		
	}
	
	protected void RecalculateSupport(int townId)
	{		
		OVT_TownData town = m_Towns[townId];
		
		int newsupport = town.support;
		float supportmods = 0;
		foreach(int index : town.supportModifiers)
		{
			OVT_SupportModifierConfig mod = m_SupportModifiers.m_aSupportModifiers[index];
			supportmods += mod.baseEffect;
		}
		if(supportmods > 100) supportmods = 100;
		if(supportmods < -100) supportmods = -100;
		
		if(supportmods > 75)
		{
			//Add a supporter to the cause
			newsupport++;
		}else if(supportmods < -75)
		{
			//Remove a supporter from the cause
			newsupport--;
		}else if(supportmods > 0)
		{
			//Maybe add a supporter
			if(s_AIRandomGenerator.RandFloatXY(0, 100) < supportmods)
			{
				newsupport++;
			}
		}else if(supportmods < 0)
		{
			//Maybe remove a supporter
			if(s_AIRandomGenerator.RandFloatXY(0, 100) < Math.AbsInt(supportmods))
			{
				newsupport--;
			}
		}
		
		if(newsupport < 0) newsupport = 0;
		if(newsupport > town.population) newsupport = town.population;
		
		if(newsupport != town.support)
		{
			town.support = newsupport;
			Rpc(RpcDo_SetSupport, townId, newsupport);
		}		
	}
	
	IEntity GetRandomHouse()
	{
		m_Houses = new array<ref EntityID>;
		OVT_TownData town = m_Towns.GetRandomElement();
		
		GetGame().GetWorld().QueryEntitiesBySphere(town.location, m_iCityRange, CheckHouseAddToArray, FilterHouseEntities, EQueryEntitiesFlags.STATIC);
		
		return GetGame().GetWorld().FindEntityByID(m_Houses.GetRandomElement());
	}
	
	IEntity GetRandomStartingHouse()
	{
		m_Houses = new array<ref EntityID>;
		OVT_TownData town;
		IEntity house;
		int i = 0;
		
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
		
		m_iTownCount++;
		
		if(mapdesc.GetBaseType() == EMapDescriptorType.MDT_NAME_VILLAGE) town.size = 1;
		if(mapdesc.GetBaseType() == EMapDescriptorType.MDT_NAME_TOWN) town.size = 2;
		if(mapdesc.GetBaseType() == EMapDescriptorType.MDT_NAME_CITY) town.size = 3;
		
		m_CheckTown = town;
		
		int range = m_iTownRange;
		if(town.size == 1) range = m_iVillageRange;
		if(town.size == 3) range = m_iCityRange;
		
		town.stability = 100;
		
		GetGame().GetWorld().QueryEntitiesBySphere(entity.GetOrigin(), range, CheckHouseAddPopulation, FilterHouseEntities, EQueryEntitiesFlags.STATIC);
		
		#ifdef OVERTHROW_DEBUG
		Print(town.name + ": pop. " + town.population.ToString());
		#endif
		
		m_Towns.Insert(town);
		
	}
	
	protected bool FilterCityTownEntities(IEntity entity) 
	{		
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (mapdesc){	
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
		writer.Write(m_Towns.Count(), 32); 
		for(int i; i<m_Towns.Count(); i++)
		{
			OVT_TownData town = m_Towns[i];
			writer.Write(town.id, 32);
			writer.WriteVector(town.location);
			writer.Write(town.population, 32);
			writer.Write(town.stability, 32);
			writer.Write(town.support, 32);
			writer.Write(town.faction, 32);
			writer.Write(town.size, 32);
			
			writer.Write(town.stabilityModifiers.Count(), 32);
			for(int t; t<town.stabilityModifiers.Count(); i++)
			{
				writer.Write(town.stabilityModifiers[t], 32);
			}
			writer.Write(town.supportModifiers.Count(), 32);
			for(int t; t<town.supportModifiers.Count(); i++)
			{
				writer.Write(town.supportModifiers[t], 32);
			}
			
			writer.Write(town.stabilityModifierTimers.Count(), 32);
			for(int t; t<town.stabilityModifierTimers.Count(); i++)
			{
				writer.Write(town.stabilityModifierTimers[t], 32);
			}
			writer.Write(town.supportModifierTimers.Count(), 32);
			for(int t; t<town.supportModifierTimers.Count(); i++)
			{
				writer.Write(town.supportModifierTimers[t], 32);
			}
		}
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{				
		//Recieve JIP towns
		int length, modlength, mod;
		
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{
			OVT_TownData town = new OVT_TownData();
			
			if (!reader.Read(town.id, 32)) return false;
			if (!reader.ReadVector(town.location)) return false;		
			if (!reader.Read(town.population, 32)) return false;		
			if (!reader.Read(town.stability, 32)) return false;		
			if (!reader.Read(town.support, 32)) return false;		
			if (!reader.Read(town.faction, 32)) return false;		
			if (!reader.Read(town.size, 32)) return false;	
				
			if (!reader.Read(modlength, 32)) return false;
			for(int t = 0; t<modlength; t++)
			{
				if (!reader.Read(mod, 32)) return false;
				town.stabilityModifiers.Insert(mod);
			}	
			if (!reader.Read(modlength, 32)) return false;
			for(int t = 0; t<modlength; t++)
			{
				if (!reader.Read(mod, 32)) return false;
				town.supportModifiers.Insert(mod);
			}
			
			if (!reader.Read(modlength, 32)) return false;
			for(int t = 0; t<modlength; t++)
			{
				if (!reader.Read(mod, 32)) return false;
				town.stabilityModifierTimers.Insert(mod);
			}	
			if (!reader.Read(modlength, 32)) return false;
			for(int t = 0; t<modlength; t++)
			{
				if (!reader.Read(mod, 32)) return false;
				town.supportModifierTimers.Insert(mod);
			}
			
			m_Towns.Insert(town);
		}
		return true;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddStabilityModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];
		OVT_StabilityModifierConfig mod = m_StabilityModifiers.m_aStabilityModifiers[index];
		town.stabilityModifiers.Insert(index);
		town.stabilityModifierTimers.Insert(mod.timeout);
		
		RecalculateStability(townId);		
		Rpc(RpcDo_AddStabilityModifier, townId, index, mod.timeout);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddSupportModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];
		OVT_SupportModifierConfig mod = m_SupportModifiers.m_aSupportModifiers[index];
		town.supportModifiers.Insert(index);
		town.supportModifierTimers.Insert(mod.timeout);
				
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
	protected void RpcDo_AddStabilityModifier(int townId, int index, int timer)
	{
		OVT_TownData town = m_Towns[townId];
		town.stabilityModifiers.Insert(index);
		town.stabilityModifierTimers.Insert(timer);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_AddSupportModifier(int townId, int index, int timer)
	{
		OVT_TownData town = m_Towns[townId];
		town.supportModifiers.Insert(index);
		town.supportModifierTimers.Insert(timer);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RemoveStabilityModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];		
		int i = town.stabilityModifiers.Find(index);
		if(i > -1)
		{
			town.stabilityModifierTimers.Remove(i);
			town.stabilityModifiers.Remove(i);
		}
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RemoveSupportModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];		
		int i = town.supportModifiers.Find(index);
		if(i > -1)
		{
			town.supportModifierTimers.Remove(i);
			town.supportModifiers.Remove(i);
		}
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_ResetStabilityModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];
		OVT_StabilityModifierConfig mod = m_StabilityModifiers.m_aStabilityModifiers[index];
		int i = town.stabilityModifiers.Find(index);
		if(i > -1)
		{
			town.stabilityModifierTimers[i] = mod.timeout;
		}
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_ResetSupportModifier(int townId, int index)
	{
		OVT_TownData town = m_Towns[townId];
		OVT_StabilityModifierConfig mod = m_StabilityModifiers.m_aStabilityModifiers[index];
		int i = town.stabilityModifiers.Find(index);
		if(i > -1)
		{
			town.stabilityModifierTimers[i] = mod.timeout;
		}
	}
	
	void ~OVT_TownManagerComponent()
	{
		if(m_Towns)
		{
			m_Towns.Clear();
			m_Towns = null;
		}
		if(m_Houses)
		{
			m_Houses.Clear();
			m_Houses = null;
		}
	}
}