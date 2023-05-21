class OVT_OccupyingFactionManagerClass: OVT_ComponentClass
{	
}

class OVT_BaseData
{
	int id;
	int faction;
	int closeRange;
	int range;
	vector location;
	EntityID entId;
	ref array<ref EntityID> garrison = {};
	
	bool IsOccupyingFaction()
	{
		return faction == OVT_Global.GetConfig().GetOccupyingFactionIndex();
	}
}

class OVT_RadioTowerData
{
	int id;
	int faction;	
	vector location;	
	ref array<ref EntityID> garrison = {};
		
	bool IsOccupyingFaction()
	{
		return faction == OVT_Global.GetConfig().GetOccupyingFactionIndex();
	}
}

enum OVT_TargetType
{
	BASE,
	BROADCAST_TOWER,
	FOB,
	WAREHOUSE
}

enum OVT_OrderType
{
	ATTACK,
	DEFEND,
	DESTROY
}

class OVT_TargetData
{
	OVT_TargetType type;
	vector location;
	int assignedBase;
	bool completed;
	OVT_OrderType order;
}

class OVT_OccupyingFactionManager: OVT_Component
{	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Base Controller Prefab", params: "et", category: "Controllers")]
	ResourceName m_pBaseControllerPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "QRF Controller Prefab", params: "et", category: "Controllers")]
	ResourceName m_pQRFControllerPrefab;
	

	bool m_bDistributeInitial = true;
	OVT_OccupyingFactionStruct m_LoadStruct;
	
	int m_iResources;
	float m_iThreat;
	ref array<ref OVT_BaseData> m_Bases;
	ref array<ref OVT_RadioTowerData> m_RadioTowers;
	ref array<ref vector> m_BasesToSpawn;
	
	ref array<ref OVT_TargetData> m_aKnownTargets;
	
	protected int m_iOccupyingFactionIndex;
	protected int m_iPlayerFactionIndex;
	
	OVT_QRFControllerComponent m_CurrentQRF;
	protected OVT_BaseControllerComponent m_CurrentQRFBase;
	protected OVT_TownData m_CurrentQRFTown;
	
	bool m_bQRFActive = false;	
	vector m_vQRFLocation = "0 0 0";
	int m_iCurrentQRFBase = -1;	
	int m_iCurrentQRFTown = -1;	
	int m_iQRFPoints = 0;	
	int m_iQRFTimer = 0;
	
	const int OF_UPDATE_FREQUENCY = 60000;
	
	ref ScriptInvoker<IEntity> m_OnAIKilled = new ScriptInvoker<IEntity>;
	ref ScriptInvoker<OVT_BaseControllerComponent> m_OnBaseControlChanged = new ScriptInvoker<OVT_BaseControllerComponent>;
	
	static OVT_OccupyingFactionManager s_Instance;
	
	static OVT_OccupyingFactionManager GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_OccupyingFactionManager.Cast(pGameMode.FindComponent(OVT_OccupyingFactionManager));
		}

		return s_Instance;
	}
	
	void OVT_OccupyingFactionManager(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_Bases = new array<ref OVT_BaseData>;	
		m_RadioTowers = new array<ref OVT_RadioTowerData>;	
		m_BasesToSpawn = new array<ref vector>;	
		m_aKnownTargets = new array<ref OVT_TargetData>;
	}
	
	void Init(IEntity owner)
	{	
		if(!Replication.IsServer()) return;
			
		m_iThreat = m_Config.m_Difficulty.baseThreat;
		m_iResources = m_Config.m_Difficulty.startingResources;
		
		Faction playerFaction = GetGame().GetFactionManager().GetFactionByKey(m_Config.m_sPlayerFaction);
		m_iPlayerFactionIndex = GetGame().GetFactionManager().GetFactionIndex(playerFaction);
		
		Faction occupyingFaction = GetGame().GetFactionManager().GetFactionByKey(m_Config.m_sOccupyingFaction);
		m_iOccupyingFactionIndex = GetGame().GetFactionManager().GetFactionIndex(occupyingFaction);
				
		OVT_Global.GetTowns().m_OnTownControlChange.Insert(OnTownControlChanged);
		
		InitializeBases();
	}
	
	void NewGameStart()
	{
		m_Config.m_iOccupyingFactionIndex = -1;
		foreach(OVT_BaseData data : m_Bases)
		{
			data.faction = m_Config.GetOccupyingFactionIndex();
		}
	}
	
	void PostGameStart()
	{
		GetGame().GetCallqueue().CallLater(SpawnBaseControllers, 0);
		
		GetGame().GetCallqueue().CallLater(CheckUpdate, OF_UPDATE_FREQUENCY / m_Config.m_iTimeMultiplier, true, GetOwner());		
		
		GetGame().GetCallqueue().CallLater(CheckRadioTowers, 9000, true, GetOwner());	
		
		if(m_bDistributeInitial)
			GetGame().GetCallqueue().CallLater(DistributeInitialResources, 100);
	}
	
	void CheckRadioTowers()
	{
		OVT_Faction faction = m_Config.GetOccupyingFaction();
		foreach(OVT_RadioTowerData tower : m_RadioTowers)
		{
			if(!tower.IsOccupyingFaction()) continue;
			if(OVT_Global.PlayerInRange(tower.location, 2500))
			{
				if(tower.garrison.Count() == 0)
				{
					//Spawn in radio defense
					BaseWorld world = GetGame().GetWorld();			
					EntitySpawnParams spawnParams = new EntitySpawnParams;
					spawnParams.TransformMode = ETransformMode.WORLD;
					
					vector pos = tower.location + "5 0 0";
					
					float surfaceY = world.GetSurfaceY(pos[0], pos[2]);
					if (pos[1] < surfaceY)
					{
						pos[1] = surfaceY;
					}
					
					spawnParams.Transform[3] = pos;
					
					for(int t = 0; t < m_Config.m_Difficulty.radioTowerGroups; t++)
					{
						IEntity group = GetGame().SpawnEntityPrefab(Resource.Load(faction.m_aTowerDefensePatrolPrefab), world, spawnParams);
						tower.garrison.Insert(group.GetID());
						SCR_AIGroup aigroup = SCR_AIGroup.Cast(group);
						AIWaypoint wp = m_Config.SpawnDefendWaypoint(pos);
						aigroup.AddWaypoint(wp);
					}
				}else{
					//Check if dead
					array<EntityID> remove = {};
					foreach(EntityID id : tower.garrison)
					{
						IEntity ent = GetGame().GetWorld().FindEntityByID(id);
						SCR_AIGroup group = SCR_AIGroup.Cast(ent);
						if(!group)
						{
							remove.Insert(id);
						}else if(group.GetAgentsCount() == 0)
						{
							SCR_EntityHelper.DeleteEntityAndChildren(group);
							remove.Insert(id);
						}
					}
					foreach(EntityID id : remove)			
					{
						tower.garrison.RemoveItem(id);
					}
					if(tower.garrison.Count() == 0)
					{
						//radio tower changes hands
						tower.faction = m_Config.GetPlayerFactionIndex();
						Rpc(RpcDo_SetRadioTowerFaction, tower.location, tower.faction);
						OVT_TownData town = OVT_Global.GetTowns().GetNearestTown(tower.location);
						OVT_Global.GetPlayers().HintMessageAll("RadioTowerControlledResistance",town.id,-1);
					}
				}
			}else{
				if(tower.garrison.Count() > 0)
				{
					//Despawn defense
					foreach(EntityID id : tower.garrison)
					{
						IEntity ent = GetGame().GetWorld().FindEntityByID(id);
						SCR_EntityHelper.DeleteEntityAndChildren(ent);
					}
					tower.garrison.Clear();
				}
			}
		}
	}
	
	void OnTownControlChanged(OVT_TownData town)
	{
		if(town.faction == m_iPlayerFactionIndex)
		{
			m_iThreat += town.size * 15;
		}
	}
	
	OVT_BaseData GetNearestBase(vector pos)
	{
		OVT_BaseData nearestBase;
		float nearest = 9999999;
		foreach(OVT_BaseData data : m_Bases)
		{			
			float distance = vector.Distance(data.location, pos);
			if(distance < nearest){
				nearest = distance;
				nearestBase = data;
			}
		}
		if(!nearestBase) return null;
		return nearestBase;
	}
	
	OVT_RadioTowerData GetNearestRadioTower(vector pos)
	{
		OVT_RadioTowerData nearestBase;
		float nearest = 9999999;
		foreach(OVT_RadioTowerData data : m_RadioTowers)
		{			
			float distance = vector.Distance(data.location, pos);
			if(distance < nearest){
				nearest = distance;
				nearestBase = data;
			}
		}
		if(!nearestBase) return null;
		return nearestBase;
	}
	
	OVT_BaseData GetNearestOccupiedBase(vector pos)
	{
		OVT_BaseData nearestBase;
		float nearest = 9999999;
		foreach(OVT_BaseData data : m_Bases)
		{
			if(!data.IsOccupyingFaction()) continue;
			float distance = vector.Distance(data.location, pos);
			if(distance < nearest){
				nearest = distance;
				nearestBase = data;
			}
		}
		if(!nearestBase) return null;
		return nearestBase;
	}
	
	void GetBasesWithinDistance(vector pos, float maxDistance, out array<OVT_BaseData> bases)
	{
		foreach(OVT_BaseData base : m_Bases)
		{			
			float distance = vector.Distance(base.location, pos);
			if(distance < maxDistance){				
				bases.Insert(base);
			}
		}
	}
	
	void RecoverResources(int resources)
	{
		m_iResources += resources;
	}
	
	void InitializeBases()
	{
		#ifdef OVERTHROW_DEBUG
		Print("Finding bases");
		#endif
		
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, CheckBaseAdd, FilterBaseEntities, EQueryEntitiesFlags.STATIC);
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, CheckTransmitterTowerAdd, FilterTransmitterTowerEntities, EQueryEntitiesFlags.STATIC);
	}
	
	protected void SpawnBaseControllers()
	{		
		OVT_ResistanceFactionManager rf = OVT_Global.GetResistanceFaction();
		OVT_Faction resistance = m_Config.GetPlayerFaction();
		
		foreach(int index, vector pos : m_BasesToSpawn)
		{		
			IEntity baseEntity = SpawnBaseController(pos);
			
			OVT_BaseControllerComponent base = OVT_BaseControllerComponent.Cast(baseEntity.FindComponent(OVT_BaseControllerComponent));
			OVT_BaseData data = GetNearestBase(pos);
			data.closeRange = base.m_iCloseRange;
			data.range = base.m_iRange;
			data.entId = baseEntity.GetID();	
			base.SetControllingFaction(data.faction, false);
			
			if(m_LoadStruct)
			{
				//Loading a save game
				OVT_BaseDataStruct struct;
				foreach(OVT_BaseDataStruct s : m_LoadStruct.bases)
				{
					if(s.pos == pos)
					{
						struct = s;
						break;
					}
				}
				if(struct)
				{
					if(data.IsOccupyingFaction())
					{
						foreach(OVT_BaseUpgradeStruct upgrade : struct.upgrades)
						{
							OVT_BaseUpgrade up = base.FindUpgrade(upgrade.type, upgrade.tag);
							up.Deserialize(upgrade, m_LoadStruct.rdb);
						}	
					}else{
						foreach(int res : struct.garrison)
						{
							ResourceName resource = m_LoadStruct.rdb[res];
							int prefabIndex = resistance.m_aGroupPrefabSlots.Find(resource);
							if(prefabIndex > -1)
							{
								rf.AddGarrison(data.id, prefabIndex,false);
							}							
						}
					}					
				}
			}					
		}
		m_BasesToSpawn.Clear();
	}
	
	protected void DistributeInitialResources()
	{
		//Distribute initial resources
		
		int resourcesPerBase = Math.Floor(m_iResources / m_Bases.Count());
		
		foreach(OVT_BaseData data : m_Bases)
		{
			OVT_BaseControllerComponent base = GetBase(data.entId);
			m_iResources -= base.SpendResources(resourcesPerBase, m_iThreat);			
			
			if(m_iResources <= 0) break;
		}
		UpdateSpecops();
		if(m_iResources < 0) m_iResources = 0;
		Print("OF Remaining Resources: " + m_iResources);
	}
	
	OVT_BaseControllerComponent GetBase(EntityID id)
	{		
		IEntity marker = GetGame().GetWorld().FindEntityByID(id);
		return OVT_BaseControllerComponent.Cast(marker.FindComponent(OVT_BaseControllerComponent));
	}
	
	OVT_BaseControllerComponent GetBaseByIndex(int index)
	{
		return GetBase(m_Bases[index].entId);
	}
	
	int GetBaseIndex(OVT_BaseData base)
	{
		return m_Bases.Find(base);
	}
	
	OVT_TargetData GetNearestKnownTarget(vector pos)
	{
		OVT_TargetData nearestTarget;
		float nearest = 9999999;
		foreach(OVT_TargetData data : m_aKnownTargets)
		{			
			float distance = vector.Distance(data.location, pos);
			if(distance < nearest){
				nearest = distance;
				nearestTarget = data;
			}
		}
		if(!nearestTarget) return null;
		return nearestTarget;
	}
	
	void UpdateQRFTimer(int timer)
	{
		m_iQRFTimer = timer;
		Rpc(RpcDo_SetQRFTimer, timer);
	}
	
	void UpdateQRFPoints(int points)
	{
		m_iQRFPoints = points;
		Rpc(RpcDo_SetQRFPoints, points);
	}
	
	void StartBaseQRF(OVT_BaseControllerComponent base)
	{
		if(m_CurrentQRF) return;
		
		OVT_BaseData data = GetNearestBase(base.GetOwner().GetOrigin());
		
		m_CurrentQRF = SpawnQRFController(base.GetOwner().GetOrigin());
		RplComponent rpl = RplComponent.Cast(m_CurrentQRF.GetOwner().FindComponent(RplComponent));
		
		m_CurrentQRF.m_OnFinished.Insert(OnQRFFinishedBase);
		m_CurrentQRFBase = base;
		
		m_bQRFActive = true;
		m_vQRFLocation = base.GetOwner().GetOrigin();
		m_iCurrentQRFBase= GetBaseIndex(data);
		
		Rpc(RpcDo_SetQRFBase, m_iCurrentQRFBase);
		Rpc(RpcDo_SetQRFActive, m_vQRFLocation);
		
		Replication.BumpMe();
	}
	
	void StartTownQRF(OVT_TownData town)
	{
		if(m_CurrentQRF) return;
		
		m_CurrentQRF = SpawnQRFController(town.location);
		RplComponent rpl = RplComponent.Cast(m_CurrentQRF.GetOwner().FindComponent(RplComponent));
		
		m_CurrentQRF.m_OnFinished.Insert(OnQRFFinishedTown);
		m_CurrentQRFTown = town;
		
		m_bQRFActive = true;
		m_vQRFLocation = town.location;
		m_iCurrentQRFTown = town.id;
		
		Rpc(RpcDo_SetQRFTown, m_iCurrentQRFBase);
		Rpc(RpcDo_SetQRFActive, m_vQRFLocation);
		
		Replication.BumpMe();
	}
	
	void OnQRFFinishedBase()
	{	
		if(m_CurrentQRF.m_iWinningFaction != m_CurrentQRFBase.m_iControllingFaction)
		{
			if(m_CurrentQRFBase.IsOccupyingFaction())
			{
				m_iThreat += 50;
				OVT_Global.GetPlayers().HintMessageAll("BaseControlledResistance");
			}else{
				OVT_Global.GetPlayers().HintMessageAll("BaseControlledOccupying");
			}
			m_CurrentQRFBase.SetControllingFaction(m_CurrentQRF.m_iWinningFaction);
			m_Bases[m_iCurrentQRFBase].faction = m_CurrentQRF.m_iWinningFaction;
			Rpc(RpcDo_SetBaseFaction, m_iCurrentQRFBase, m_CurrentQRF.m_iWinningFaction);
		}		
		
		//Recover used resources
		OVT_Global.GetOccupyingFaction().m_iResources = m_CurrentQRF.m_iUsedResources;
				
		SCR_EntityHelper.DeleteEntityAndChildren(m_CurrentQRF.GetOwner());
		m_CurrentQRF = null;
		
		m_bQRFActive = false;
		m_iCurrentQRFBase = -1;
		m_iCurrentQRFTown = -1;
		
		Rpc(RpcDo_SetQRFInactive);
	}
	
	void OnQRFFinishedTown()
	{	
		if(m_CurrentQRF.m_iWinningFaction != m_CurrentQRFTown.faction)
		{
			string type = "Town";
			if(m_CurrentQRFTown.size > 2) type = "City";
			if(m_CurrentQRFTown.IsOccupyingFaction())
			{
				m_iThreat += 50;
				OVT_Global.GetPlayers().HintMessageAll(type + "ControlledResistance");
				OVT_Global.GetTowns().TryAddSupportModifierByName(m_CurrentQRFTown.id, "RecentBattlePositive");
			}else{
				OVT_Global.GetPlayers().HintMessageAll(type + "ControlledOccupying");
				OVT_Global.GetTowns().TryAddSupportModifierByName(m_CurrentQRFTown.id, "RecentBattleNegative");
				OVT_Global.GetTowns().ResetSupport(m_CurrentQRFTown);	
			}			
			OVT_Global.GetTowns().TryAddStabilityModifierByName(m_CurrentQRFTown.id, "RecentBattle");
			OVT_Global.GetTowns().ChangeTownControl(m_CurrentQRFTown, m_CurrentQRF.m_iWinningFaction);			
					
		}
		
		//Recover used resources
		OVT_Global.GetOccupyingFaction().m_iResources = m_CurrentQRF.m_iUsedResources;
				
		SCR_EntityHelper.DeleteEntityAndChildren(m_CurrentQRF.GetOwner());
		m_CurrentQRF = null;
		
		m_bQRFActive = false;
		m_iCurrentQRFBase = -1;
		m_iCurrentQRFTown = -1;
		
		Rpc(RpcDo_SetQRFInactive);
	}
	
	void WinBattle()
	{
		if(!m_CurrentQRF) return;
		m_CurrentQRF.KillAll();
	}
	
	bool CheckBaseAdd(IEntity ent)
	{		
		#ifdef OVERTHROW_DEBUG
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		OVT_TownData closestTown = townManager.GetNearestTown(ent.GetOrigin());
		Print("Adding base near " + closestTown.name);
		#endif
				
		int occupyingFactionIndex = m_Config.GetOccupyingFactionIndex();
		
		OVT_BaseData data = new OVT_BaseData();
		data.id = m_Bases.Count();
		data.location = ent.GetOrigin();
		data.faction = occupyingFactionIndex;
		
		m_Bases.Insert(data);
		
		m_BasesToSpawn.Insert(ent.GetOrigin());		
		return true;
	}
	
	IEntity SpawnBaseController(vector loc)
	{
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = loc;
		return GetGame().SpawnEntityPrefab(Resource.Load(m_pBaseControllerPrefab), GetGame().GetWorld(), params));
		
	}
	
	OVT_QRFControllerComponent SpawnQRFController(vector loc)
	{
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = loc;
		IEntity qrf = GetGame().SpawnEntityPrefab(Resource.Load(m_pQRFControllerPrefab), GetGame().GetWorld(), params);
		return OVT_QRFControllerComponent.Cast(qrf.FindComponent(OVT_QRFControllerComponent));	
	}
	
	bool FilterBaseEntities(IEntity entity)
	{
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (mapdesc){	
			int type = mapdesc.GetBaseType();
			if(type == EMapDescriptorType.MDT_NAME_GENERIC) {				
				if(mapdesc.Item().GetDisplayName() == "#AR-MapLocation_Military") return true;
			}
		}
				
		return false;	
	}
	
	bool CheckTransmitterTowerAdd(IEntity ent)
	{		
		int occupyingFactionIndex = m_Config.GetOccupyingFactionIndex();
		
		OVT_RadioTowerData data = new OVT_RadioTowerData;
		data.id = m_RadioTowers.Count();
		data.location = ent.GetOrigin();
		data.faction = occupyingFactionIndex;
				
		m_RadioTowers.Insert(data);
				
		return true;
	}
	
	bool FilterTransmitterTowerEntities(IEntity entity)
	{
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (mapdesc){	
			int type = mapdesc.GetBaseType();
			if(type == EMapDescriptorType.MDT_TRANSMITTER) {				
				return true;
			}
		}
				
		return false;	
	}
	
	void CheckUpdate()
	{
		PlayerManager mgr = GetGame().GetPlayerManager();		
		if(mgr.GetPlayerCount() == 0)
		{
			return;
		}
		
		TimeContainer time = m_Time.GetTime();		
		
		//Every 6 hrs get more resources
		if((time.m_iHours == 0 
			|| time.m_iHours == 6 
			|| time.m_iHours == 12 
			|| time.m_iHours == 18)
			 && 
			time.m_iMinutes == 0)
		{
			GainResources();
		}
		
		//We dont spend resources or reduce threat during a QRF
		if(m_CurrentQRF) return;
		
		//Every hour distribute resources we have, if any
		if(m_iResources > 0 && time.m_iMinutes == 0)
		{
			UpdateKnownTargets();
			//To-Do: prioritize bases that need it/are under threat
			foreach(OVT_BaseData data : m_Bases)
			{
				if(!data.IsOccupyingFaction()) continue;
				OVT_BaseControllerComponent base = GetBase(data.entId);	
				
				//Dont spawn stuff if a player is watching lol
				if(OVT_Global.PlayerInRange(data.location, base.m_iCloseRange+100)) continue;			
				
				m_iResources -= base.SpendResources(m_iResources, m_iThreat);			
				
				if(m_iResources <= 0) {
					m_iResources = 0;
					break;
				}
			}
			UpdateSpecops();
			Print("OF Reserve Resources: " + m_iResources);
		}
		
		//Every 15 mins reduce threat and check if we wanna start a battle for a town
		if(time.m_iMinutes == 0 
			|| time.m_iMinutes == 15 
			|| time.m_iMinutes == 30 
			|| time.m_iMinutes == 45)			
		{
			m_iThreat -= 1;
			if(m_iThreat < 0) m_iThreat = 0;
			
			int playerFaction = m_Config.GetPlayerFactionIndex();
			int occupyingFaction = m_Config.GetOccupyingFactionIndex();
			
			foreach(OVT_TownData town : OVT_Global.GetTowns().m_Towns)
			{
				if(town.size == 1) continue;
				int support = town.SupportPercentage();		
				if(town.faction == occupyingFaction)
				{
					if(support > 75 && town.stability >= 50)
					{
						//Chance this town will start a battle
						if(support >= 85 || s_AIRandomGenerator.RandFloat01() < 0.5)
						{
							StartTownQRF(town);
						}	
					}
				}else{
					if(support < 25)
					{
						//Chance this town will start a battle
						if(support <= 15 || s_AIRandomGenerator.RandFloat01() < 0.5)
						{
							StartTownQRF(town);
						}	
					}
				}
			}
		}
	}
	
	protected void UpdateSpecops()
	{
		if(m_iResources > m_Config.m_Difficulty.maxQRF)
		{
			//Do Specops
			foreach(OVT_TargetData target : m_aKnownTargets)
			{
				if(target.completed || target.type == OVT_TargetType.BROADCAST_TOWER) continue;
				OVT_BaseData data = GetNearestOccupiedBase(target.location);
				if(!data) break;
				OVT_BaseControllerComponent base = GetBase(data.entId);
				OVT_BaseUpgradeSpecops upgrade = OVT_BaseUpgradeSpecops.Cast(base.FindUpgrade("OVT_BaseUpgradeSpecops"));
				if(upgrade && !upgrade.HasTarget())
				{
					m_iResources -= upgrade.SetTarget(target);
				}
				
				if(m_iResources <= 0) {
					m_iResources = 0;
					break;
				}
				if(m_iResources < m_Config.m_Difficulty.maxQRF) {
					break;
				}
			}
		}	
	}
	
	void UpdateKnownTargets()
	{
		//To-Do: target discovery not by magic
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		
		foreach(OVT_FOBData fob : resistance.m_FOBs)
		{
			if(!IsKnownTarget(fob.location))
			{
				OVT_TargetData target = new OVT_TargetData();
				target.location = fob.location;
				target.type = OVT_TargetType.FOB;
				target.order = OVT_OrderType.ATTACK;
				m_aKnownTargets.Insert(target);
			}
		}
		
		foreach(OVT_BaseData data : m_Bases)
		{
			if(data.IsOccupyingFaction()){
				if(IsKnownTarget(data.location))
				{
					m_aKnownTargets.RemoveItem(GetNearestKnownTarget(data.location));
				}
				continue;
			}
			if(!IsKnownTarget(data.location))
			{
				OVT_TargetData target = new OVT_TargetData();
				target.location = data.location;
				target.type = OVT_TargetType.BASE;
				target.order = OVT_OrderType.ATTACK;
				m_aKnownTargets.Insert(target);
			}
		}
	}
	
	bool IsKnownTarget(vector pos)
	{
		foreach(OVT_TargetData target : m_aKnownTargets)
		{
			if(vector.Distance(target.location, pos) < 1)
			{
				return true;
			}
		}
		return false;
	}
	
	void OnBaseControlChange(OVT_BaseControllerComponent base)
	{
		if(m_OnBaseControlChanged) m_OnBaseControlChanged.Invoke(base);
	}
	
	void GainResources()
	{
		float threatFactor = m_iThreat / 100;
		if(threatFactor > 1) threatFactor = 1;
		int newResources = m_Config.m_Difficulty.baseResourcesPerTick + (m_Config.m_Difficulty.resourcesPerTick * threatFactor);
		
		m_iResources += newResources;
		
		Print ("OF Gained Resources: " + newResources.ToString());			
	}
	
	void OnAIKilled(IEntity ai, IEntity insitgator)
	{
		if(!Replication.IsServer()) return;
		
		m_iThreat += 1;
		
		m_OnAIKilled.Invoke(ai);
	}
	
	//RPC Methods
	
	override bool RplSave(ScriptBitWriter writer)
	{
				
		//Send JIP bases
		writer.Write(m_Bases.Count(), 32); 
		for(int i; i<m_Bases.Count(); i++)
		{
			OVT_BaseData data = m_Bases[i];
			writer.WriteVector(data.location);
			writer.Write(data.faction, 32);
			writer.Write(data.closeRange, 32);
			writer.Write(data.range, 32);
		}
		
		//Send JIP radio towers
		writer.Write(m_RadioTowers.Count(), 32); 
		for(int i; i<m_RadioTowers.Count(); i++)
		{
			OVT_RadioTowerData data = m_RadioTowers[i];
			writer.WriteVector(data.location);
			writer.Write(data.faction, 32);
		}
		
		writer.WriteVector(m_vQRFLocation);
		writer.Write(m_iQRFPoints, 32);
		writer.Write(m_iQRFTimer, 32);
		writer.WriteBool(m_bQRFActive);
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{		
		int length;
		RplId id;		
		
		//Recieve JIP bases
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{	
			OVT_BaseData base = new OVT_BaseData();
					
			if (!reader.ReadVector(base.location)) return false;
			if (!reader.Read(base.faction, 32)) return false;
			if (!reader.Read(base.closeRange, 32)) return false;
			if (!reader.Read(base.range, 32)) return false;
			
			base.id = i;
			m_Bases.Insert(base);
		}
		
		//Recieve JIP radio towers
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{	
			OVT_RadioTowerData base = new OVT_RadioTowerData();
					
			if (!reader.ReadVector(base.location)) return false;
			if (!reader.Read(base.faction, 32)) return false;
			
			base.id = i;
			m_RadioTowers.Insert(base);
		}
		if (!reader.ReadVector(m_vQRFLocation)) return false;
		if (!reader.Read(m_iQRFPoints,32)) return false;
		if (!reader.Read(m_iQRFTimer,32)) return false;
		if (!reader.ReadBool(m_bQRFActive)) return false;
		
		return true;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetQRFActive(vector pos)
	{
		m_vQRFLocation = pos;
		m_bQRFActive = true;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetQRFBase(int base)
	{
		m_iCurrentQRFBase = base;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetQRFTown(int townId)
	{
		m_iCurrentQRFTown = townId;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetQRFPoints(int points)
	{
		m_iQRFPoints = points;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetBaseFaction(int index, int faction)
	{
		m_Bases[index].faction = faction;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetRadioTowerFaction(vector pos, int faction)
	{
		OVT_RadioTowerData tower = GetNearestRadioTower(pos);
		if(!tower) return;
		
		tower.faction = faction;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetQRFTimer(int timer)
	{
		m_iQRFTimer = timer;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetQRFInactive()
	{
		m_bQRFActive = false;
		m_iCurrentQRFBase = -1;
		m_iCurrentQRFTown = -1;
	}
	
	void ~OVT_OccupyingFactionManager()
	{
		GetGame().GetCallqueue().Remove(CheckUpdate);	
		
		if(m_Bases)
		{
			m_Bases.Clear();
			m_Bases = null;
		}			
	}
}