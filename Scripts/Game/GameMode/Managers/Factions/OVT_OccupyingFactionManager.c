class OVT_OccupyingFactionManagerClass: OVT_ComponentClass
{
}

class OVT_BaseUpgradeData : Managed
{
	string type;
	int resources;
	ref array<ref OVT_BaseUpgradeGroupData> groups = {};
	string tag = "";
	vector pos;
}

class OVT_BaseUpgradeGroupData : Managed
{
	string prefab;
	vector position;
}

class OVT_BaseData : Managed
{
	[NonSerialized()]
	int id;

	int faction;

	vector location;
	ref array<vector> slotsFilled = {};

	ref array<ref OVT_BaseUpgradeData> upgrades = {};

	[NonSerialized()]
	EntityID entId;

	[NonSerialized()]
	ref array<ref EntityID> garrisonEntities = {};

	ref array<ref ResourceName> garrison = {};
	
	[SortAttribute(),NonSerialized()]
	int sortBy;

	bool IsOccupyingFaction()
	{
		return faction == OVT_Global.GetConfig().GetOccupyingFactionIndex();
	}

	static OVT_BaseData Get(vector pos)
	{
		return OVT_Global.GetOccupyingFaction().GetNearestBase(pos);
	}
}

class OVT_RadioTowerData : Managed
{
	[NonSerialized()]
	int id;

	int faction;
	vector location;

	[NonSerialized()]
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
	WAREHOUSE,
	CAMP
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
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "QRF Controller Prefab", params: "et", category: "Controllers")]
	ResourceName m_pQRFControllerPrefab;


	bool m_bDistributeInitial = true;

	int m_iResources;
	float m_iThreat;
	ref array<ref OVT_BaseData> m_Bases = new array<ref OVT_BaseData>;
	ref array<ref OVT_RadioTowerData> m_RadioTowers = new array<ref OVT_RadioTowerData>;

	ref array<ref OVT_TargetData> m_aKnownTargets = new array<ref OVT_TargetData>;

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

	int m_bCounterAttackTimeout = 0;

	const int OF_UPDATE_FREQUENCY = 60000;

	ref ScriptInvoker<IEntity> m_OnAIKilled = new ScriptInvoker<IEntity>;
	ref ScriptInvoker<OVT_BaseControllerComponent> m_OnBaseControlChanged = new ScriptInvoker<OVT_BaseControllerComponent>;
	ref ScriptInvoker<IEntity> m_OnPlayerLoot = new ScriptInvoker<IEntity>;

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

	void Init(IEntity owner)
	{
		if(!Replication.IsServer()) 
		{
			// On clients, set up base faction affiliations after JIP data is loaded
			GetGame().GetCallqueue().CallLater(SetClientBaseFactions, 1000);
			return;
		}

		Faction playerFaction = GetGame().GetFactionManager().GetFactionByKey(m_Config.m_sPlayerFaction);
		m_iPlayerFactionIndex = GetGame().GetFactionManager().GetFactionIndex(playerFaction);

		Faction occupyingFaction = GetGame().GetFactionManager().GetFactionByKey(m_Config.m_sOccupyingFaction);
		m_iOccupyingFactionIndex = GetGame().GetFactionManager().GetFactionIndex(occupyingFaction);

		OVT_Global.GetTowns().m_OnTownControlChange.Insert(OnTownControlChanged);

		InitializeBases();
	}
	
	void SetClientBaseFactions()
	{
		if (Replication.IsServer()) return;
		
		// Iterate through all bases and set their faction affiliations on the client
		foreach (OVT_BaseData base : m_Bases)
		{
			BaseWorld world = GetOwner().GetWorld();
			world.QueryEntitiesBySphere(base.location, 5, CheckBaseAndSetFaction, null, EQueryEntitiesFlags.STATIC);
		}
	}
	
	bool CheckBaseAndSetFaction(IEntity entity)
	{
		OVT_BaseControllerComponent baseController = OVT_BaseControllerComponent.Cast(entity.FindComponent(OVT_BaseControllerComponent));
		if (!baseController) return true;
		
		//Initialize the base controller for the client
		baseController.InitBaseClient();		
		
		// Find the base data for this location
		OVT_BaseData baseData = GetNearestBase(entity.GetOrigin());
		if (!baseData) return true;
		
		// Set the faction affiliation
		SCR_FactionAffiliationComponent affiliation = EPF_Component<SCR_FactionAffiliationComponent>.Find(entity);
		if (affiliation)
		{
			FactionManager factionManager = GetGame().GetFactionManager();
			Faction faction = factionManager.GetFactionByIndex(baseData.faction);
			if (faction)
			{
				affiliation.SetAffiliatedFaction(faction);
			}
		}
		
		return true;
	}

	void NewGameStart()
	{
		OVT_Global.GetConfig().m_iOccupyingFactionIndex = -1;
		m_iThreat = m_Config.m_Difficulty.baseThreat;
		m_iResources = m_Config.m_Difficulty.maxQRF;
		
		foreach(OVT_BaseData data : m_Bases)
		{
			data.faction = OVT_Global.GetConfig().GetOccupyingFactionIndex();
		}
		
		// Allocate initial resources to deployment manager
		AllocateDeploymentResources(m_Config.m_Difficulty.baseResourcesPerTick);
	}

	void PostGameStart()
	{
		float timeMul = 6;
		OVT_TimeAndWeatherHandlerComponent tw = OVT_TimeAndWeatherHandlerComponent.Cast(GetGame().GetGameMode().FindComponent(OVT_TimeAndWeatherHandlerComponent));

		if(tw) timeMul = tw.GetDayTimeMultiplier();
		
		UpdateKnownTargets();

		GetGame().GetCallqueue().CallLater(InitBaseControllers, 0);

		GetGame().GetCallqueue().CallLater(CheckUpdate, OF_UPDATE_FREQUENCY / timeMul, true, GetOwner());

		GetGame().GetCallqueue().CallLater(CheckRadioTowers, 9000, true, GetOwner());

		if(m_bDistributeInitial)
			GetGame().GetCallqueue().CallLater(DistributeInitialResources, 5000);
	}

	void CheckRadioTowers()
	{
		OVT_Faction faction = OVT_Global.GetConfig().GetOccupyingFaction();
		foreach(OVT_RadioTowerData tower : m_RadioTowers)
		{
			if(!tower.IsOccupyingFaction()) continue;
			bool inrange = OVT_Global.PlayerInRange(tower.location, OVT_Global.GetConfig().m_iMilitarySpawnDistance) && !m_CurrentQRF;
			if(inrange)
			{
				if(tower.garrison.Count() == 0)
				{
					//Spawn in radio defense

					vector pos = tower.location + "5 0 0";

					float surfaceY = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
					if (pos[1] < surfaceY)
					{
						pos[1] = surfaceY;
					}
					
					OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
					
					int numGroups = s_AIRandomGenerator.RandInt(config.m_Difficulty.patrolGroupsMin,config.m_Difficulty.patrolGroupsMax);

					for(int t = 0; t < numGroups; t++)
					{
						IEntity group = OVT_Global.SpawnEntityPrefab(faction.m_aTowerDefensePatrolPrefab, pos);
						tower.garrison.Insert(group.GetID());
						SCR_AIGroup aigroup = SCR_AIGroup.Cast(group);
						AIWaypoint wp = OVT_Global.GetConfig().SpawnDefendWaypoint(pos);
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
						ChangeRadioTowerControl(tower, OVT_Global.GetConfig().GetPlayerFactionIndex());
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

	void ChangeRadioTowerControl(OVT_RadioTowerData tower, int faction)
	{
		if(faction == tower.faction) return;
		tower.faction = faction;
		Rpc(RpcDo_SetRadioTowerFaction, tower.location, faction);

		string townName = OVT_Global.GetTowns().GetTownName(tower.location);

		if(faction == OVT_Global.GetConfig().GetOccupyingFactionIndex())
		{
			OVT_Global.GetNotify().SendTextNotification("RadioTowerControlledOccupying",-1,townName);
			OVT_Global.GetNotify().SendExternalNotifications("RadioTowerControlledOccupying",townName);
		}else{
			OVT_Global.GetNotify().SendTextNotification("RadioTowerControlledResistance",-1,townName);
			OVT_Global.GetNotify().SendExternalNotifications("RadioTowerControlledResistance",townName);
		}
	}

	void OnTownControlChanged(OVT_TownData town)
	{
		if(town.faction == m_iPlayerFactionIndex)
		{
			m_iThreat += town.size * 150;
		}
	}

	OVT_BaseData GetNearestBase(vector pos)
	{
		OVT_BaseData nearestBase;
		float nearest = -1;
		foreach(OVT_BaseData data : m_Bases)
		{
			float distance = vector.Distance(data.location, pos);
			if(nearest == -1 || distance < nearest){
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
		float nearest = -1;
		foreach(OVT_RadioTowerData data : m_RadioTowers)
		{
			float distance = vector.Distance(data.location, pos);
			if(nearest == -1 || distance < nearest){
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
		float nearest = -1;
		foreach(OVT_BaseData data : m_Bases)
		{
			if(!data.IsOccupyingFaction()) continue;
			float distance = vector.Distance(data.location, pos);
			if(nearest == -1 || distance < nearest){
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

	protected void InitBaseControllers()
	{
		OVT_ResistanceFactionManager rf = OVT_Global.GetResistanceFaction();
		OVT_Faction resistance = m_Config.GetPlayerFaction();

		foreach(int index, OVT_BaseData data : m_Bases)
		{
			OVT_BaseControllerComponent base = GetBase(data.entId);
			base.InitBase();
			base.SetControllingFaction(data.faction, true);

			if(base.IsOccupyingFaction())
			{
				if(data.upgrades)
				{
					foreach(OVT_BaseUpgradeData upgrade : data.upgrades)
					{
						OVT_BaseUpgrade up = base.FindUpgrade(upgrade.type, upgrade.tag);
						up.Deserialize(upgrade);
					}
				}
				if(data.slotsFilled)
				{
					foreach(vector slotPos : data.slotsFilled)
					{
						IEntity slot = base.GetNearestSlot(slotPos);
						if(slot) base.m_aSlotsFilled.Insert(slot.GetID());
					}
				}
			}else{
				foreach(ResourceName res : data.garrison)
				{
					data.garrisonEntities.Insert(OVT_Global.GetResistanceFaction().SpawnGarrison(data, res).GetID());
				}
			}
		}
	}

	protected void DistributeInitialResources()
	{
		//Distribute initial resources		
		foreach(OVT_BaseData data : m_Bases)
		{
			OVT_BaseControllerComponent base = GetBase(data.entId);
			int toSpend = Math.Floor(m_Config.m_Difficulty.startingResources * base.m_fStartingResourcesMultiplier);
			Print("[Overthrow.OccupyingFactionManager] Distributing " + toSpend.ToString() + " resources to " + base.m_sName);
			base.SpendResources(toSpend, m_iThreat);
		}
		UpdateSpecops();	
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
		float nearest = -1;
		foreach(OVT_TargetData data : m_aKnownTargets)
		{
			float distance = vector.Distance(data.location, pos);
			if(nearest == -1 || distance < nearest){
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
		m_CurrentQRF.m_iLZMin = base.m_iAttackDistanceMin;
		m_CurrentQRF.m_iLZMax = base.m_iAttackDistanceMax;
		m_CurrentQRF.m_iPreferredDirection = base.m_iAttackPreferredDirection;
		m_CurrentQRF.m_iDirectionVariance = base.m_iAttackDirectionVariance;
		
		if(base.m_iAttackPreferredDirection > -1)
			Print("[Overthrow] QRF starting from preferred direction: " + base.m_iAttackPreferredDirection.ToString() + " +/- " + base.m_iAttackDirectionVariance.ToString());
		
		m_CurrentQRF.Start();
		
		RplComponent rpl = RplComponent.Cast(m_CurrentQRF.GetOwner().FindComponent(RplComponent));

		m_CurrentQRF.m_OnFinished.Insert(OnQRFFinishedBase);
		m_CurrentQRFBase = base;

		m_bQRFActive = true;
		m_vQRFLocation = base.GetOwner().GetOrigin();
		m_iCurrentQRFBase= GetBaseIndex(data);

		OVT_Global.GetNotify().SendTextNotification("BaseBattle", -1, base.m_sName);
		OVT_Global.GetNotify().SendExternalNotifications("BaseBattle", base.m_sName);

		Rpc(RpcDo_SetQRFBase, m_iCurrentQRFBase);
		Rpc(RpcDo_SetQRFActive, m_vQRFLocation);
	}

	void StartTownQRF(OVT_TownData town)
	{
		if(m_CurrentQRF) return;

		int townID = OVT_Global.GetTowns().GetTownID(town);

		m_CurrentQRF = SpawnQRFController(town.location);
		
		// Find the town controller to get QRF parameters
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		EntityID townControllerID = townManager.m_TownControllers.Get(townID);
		if(townControllerID)
		{
			IEntity townEntity = GetGame().GetWorld().FindEntityByID(townControllerID);
			if(townEntity)
			{
				OVT_TownControllerComponent townController = OVT_TownControllerComponent.Cast(townEntity.FindComponent(OVT_TownControllerComponent));
				if(townController)
				{
					m_CurrentQRF.m_iLZMin = townController.m_iAttackDistanceMin;
					m_CurrentQRF.m_iLZMax = townController.m_iAttackDistanceMax;
					m_CurrentQRF.m_iPreferredDirection = townController.m_iAttackPreferredDirection;
					m_CurrentQRF.m_iDirectionVariance = townController.m_iAttackDirectionVariance;
					
					if(townController.m_iAttackPreferredDirection > -1)
						Print("[Overthrow] Town QRF starting from preferred direction: " + townController.m_iAttackPreferredDirection.ToString() + " +/- " + townController.m_iAttackDirectionVariance.ToString());
				}
			}
		}
		
		RplComponent rpl = RplComponent.Cast(m_CurrentQRF.GetOwner().FindComponent(RplComponent));
		
		m_CurrentQRF.Start();

		m_CurrentQRF.m_OnFinished.Insert(OnQRFFinishedTown);
		m_CurrentQRFTown = town;

		m_bQRFActive = true;
		m_vQRFLocation = town.location;
		m_iCurrentQRFTown = townID;

		string type = "Village";
		if(town.size == 2) type = "Town";
		if(town.size == 3) type = "City";
		OVT_Global.GetNotify().SendTextNotification(type + "Battle", -1, OVT_Global.GetTowns().GetTownName(townID));
		OVT_Global.GetNotify().SendExternalNotifications(type + "Battle", OVT_Global.GetTowns().GetTownName(townID));

		Rpc(RpcDo_SetQRFTown, m_iCurrentQRFTown);
		Rpc(RpcDo_SetQRFActive, m_vQRFLocation);
	}

	void OnQRFFinishedBase()
	{
		if(m_CurrentQRF.m_iWinningFaction != m_CurrentQRFBase.GetControllingFaction())
		{
			ChangeBaseControl(m_CurrentQRFBase, m_CurrentQRF.m_iWinningFaction);
		}

		SCR_EntityHelper.DeleteEntityAndChildren(m_CurrentQRF.GetOwner());
		m_CurrentQRF = null;

		m_bQRFActive = false;
		m_iCurrentQRFBase = -1;
		m_iCurrentQRFTown = -1;

		Rpc(RpcDo_SetQRFInactive);
	}
	
	void ChangeBaseControl(OVT_BaseControllerComponent base, int newFactionIndex)
	{
		string townName = OVT_Global.GetTowns().GetTownName(base.GetOwner().GetOrigin());
		if(base.IsOccupyingFaction())
		{
			m_iThreat += 250;
			OVT_Global.GetNotify().SendTextNotification("BaseControlledResistance",-1,townName);
			OVT_Global.GetNotify().SendExternalNotifications("BaseControlledResistance",townName);
		}else{
			m_iThreat -= 250;
			OVT_Global.GetNotify().SendTextNotification("BaseControlledOccupying",-1,townName);
			OVT_Global.GetNotify().SendExternalNotifications("BaseControlledOccupying",townName);
		}
		
		OVT_BaseData baseData = GetNearestBase(base.GetOwner().GetOrigin());
		int baseIndex = GetBaseIndex(baseData);
		
		m_Bases[baseIndex].faction = newFactionIndex;
		base.SetControllingFaction(newFactionIndex);
		Rpc(RpcDo_SetBaseFaction, baseIndex, newFactionIndex);
	}

	void OnQRFFinishedTown()
	{
		int townID = OVT_Global.GetTowns().GetTownID(m_CurrentQRFTown);
		if(m_CurrentQRF.m_iWinningFaction != m_CurrentQRFTown.faction)
		{
			//This town has changed control
			string type = "Town";
			if(m_CurrentQRFTown.size > 2) type = "City";
			if(m_CurrentQRFTown.IsOccupyingFaction())
			{
				m_iThreat += 250;
				OVT_Global.GetTowns().TryAddSupportModifierByName(townID, "RecentBattlePositive");
			}else{
				m_iThreat -= 250;
				OVT_Global.GetTowns().TryAddSupportModifierByName(townID, "RecentBattleNegative");
				//All supporters in this town abandon the resistance (and avoids the battle looping)
				OVT_Global.GetTowns().ResetSupport(m_CurrentQRFTown);
			}
			OVT_Global.GetTowns().ChangeTownControl(m_CurrentQRFTown, m_CurrentQRF.m_iWinningFaction);

		}else{
			//This town has NOT changed control, but we still need to add modifiers
			if(m_CurrentQRFTown.IsOccupyingFaction())
			{
				OVT_Global.GetTowns().TryAddSupportModifierByName(townID, "RecentBattleNegative");
				//All supporters in this town abandon the resistance (and avoids the battle looping)
				OVT_Global.GetTowns().ResetSupport(m_CurrentQRFTown);
			}else{
				m_iThreat += 250;
				OVT_Global.GetTowns().TryAddSupportModifierByName(townID, "RecentBattlePositive");
			}
		}

		OVT_Global.GetTowns().TryAddStabilityModifierByName(townID, "RecentBattle");

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
		data.entId = ent.GetID();
		data.id = m_Bases.Count();
		data.location = ent.GetOrigin();
		data.faction = m_Config.GetOccupyingFactionIndex();

		m_Bases.Insert(data);
		return true;
	}

	OVT_QRFControllerComponent SpawnQRFController(vector loc)
	{
		IEntity qrf = OVT_Global.SpawnEntityPrefab(m_pQRFControllerPrefab, loc);
		return OVT_QRFControllerComponent.Cast(qrf.FindComponent(OVT_QRFControllerComponent));
	}

	bool FilterBaseEntities(IEntity entity)
	{
		OVT_BaseControllerComponent controller = OVT_BaseControllerComponent.Cast(entity.FindComponent(OVT_BaseControllerComponent));
		if(controller) return true;

		return false;
	}

	bool CheckTransmitterTowerAdd(IEntity ent)
	{
		int occupyingFactionIndex = OVT_Global.GetConfig().GetOccupyingFactionIndex();

		OVT_RadioTowerData data = new OVT_RadioTowerData;
		data.id = m_RadioTowers.Count();
		data.location = ent.GetOrigin();
		data.faction = occupyingFactionIndex;

		m_RadioTowers.Insert(data);

		return true;
	}

	bool FilterTransmitterTowerEntities(IEntity entity)
	{
		OVT_TowerControllerComponent controller = OVT_TowerControllerComponent.Cast(entity.FindComponent(OVT_TowerControllerComponent));
		if(controller) return true;

		return false;
	}

	int GetThreatByLocation(vector pos)
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		int villageRange = towns.m_iVillageRange;
		int townRange = towns.m_iTownRange;
		int cityRange = towns.m_iCityRange;
		
		
		int score = 0;
		foreach(OVT_TargetData target : m_aKnownTargets)
		{
			float distance = vector.Distance(target.location, pos);
			if(distance < 1000)
			{
				float distanceFactor = 1.0 - (distance / 1000);
				if(target.type == OVT_TargetType.BASE)
				{
					score += (int)Math.Round(10 * distanceFactor);
				}
				if(target.type == OVT_TargetType.BROADCAST_TOWER)
				{
					score += (int)Math.Round(5 * distanceFactor);
				}
				if(target.type == OVT_TargetType.FOB)
				{
					score += (int)Math.Round(5 * distanceFactor);
				}
				if(target.type == OVT_TargetType.WAREHOUSE)
				{
					score += 1;
				}
			}
		}
		foreach(OVT_TownData town : OVT_Global.GetTowns().m_Towns)
		{
			int range = villageRange;
			if(town.size == 2) range = townRange;
			if(town.size == 3) range = cityRange;
			
			float distance = vector.Distance(town.location, pos);
			if(distance > range * 3) continue;
			
			float distanceFactor = 1.0 - (distance / ((float)range * 3));
			
			if(town.IsOccupyingFaction())
			{
				int supportScore = (int)Math.Round(((float)town.SupportPercentage() / 100) * distanceFactor * 5 * town.size);
				int stabilityScore = (int)Math.Round((1 - ((float)town.stability / 100)) * distanceFactor * 5 * town.size);				
				score += supportScore + stabilityScore;
			}else{
				int townScore = (int)Math.Round(5 * distanceFactor * town.size);
				score += townScore;
			}	
		}
		return score;
	}

	int GetThreatLevel() {return m_iThreat;}

	int GetBaseThreat(OVT_BaseData base)
	{
		return GetThreatByLocation(base.location);
	}

	void CheckUpdate()
	{
		m_bCounterAttackTimeout--;
		if(m_bCounterAttackTimeout < 0) m_bCounterAttackTimeout = 0;
		
		if(!m_Time)
		{
			ChimeraWorld world = GetOwner().GetWorld();
			m_Time = world.GetTimeAndWeatherManager();
		}

		PlayerManager mgr = GetGame().GetPlayerManager();
		if(mgr.GetPlayerCount() == 0)
		{
			//Clear dead bodies when no players are online
			SCR_AIWorld aiworld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
			autoptr array<AIAgent> agents = new array<AIAgent>;
			aiworld.GetAIAgents(agents);
			foreach(AIAgent agent : agents)
			{
				DamageManagerComponent dmg = DamageManagerComponent.Cast(agent.FindComponent(DamageManagerComponent));
				if(dmg && dmg.IsDestroyed())
				{
					//Is dead, remove body
					SCR_EntityHelper.DeleteEntityAndChildren(agent);
				}
			}
			return;
		}

		TimeContainer time = m_Time.GetTime();

		//We dont get/spend resources or reduce threat during a QRF
		if(m_CurrentQRF) return;

		//Every 6 hrs get more resources
		if((time.m_iHours == 0
			|| time.m_iHours == 6
			|| time.m_iHours == 12
			|| time.m_iHours == 18)
			 &&
			time.m_iMinutes == 0)
		{
			int newResources = GainResources();

			int toSpend = Math.Floor((float)newResources * 0.8);
			UpdateKnownTargets();

			//sort bases by threat score
			array<OVT_BaseData> sortedBases = new array<OVT_BaseData>;
			foreach(OVT_BaseData data : m_Bases)
			{
				if(!data.IsOccupyingFaction()) continue;
				data.sortBy = GetBaseThreat(data);
				sortedBases.Insert(data);
			}
			sortedBases.Sort(true);	

			int perBase = Math.Floor((float)toSpend / sortedBases.Count());		
			
			foreach(OVT_BaseData data : sortedBases)
			{				
				OVT_BaseControllerComponent base = GetBase(data.entId);

				//Dont spawn stuff if a player is watching lol
				if(OVT_Global.PlayerInRange(data.location, OVT_Global.GetConfig().m_Difficulty.baseCloseRange+100)) continue;

				m_iResources -= base.SpendResources(m_iResources, m_iThreat);

				if(m_iResources <= 0) {
					m_iResources = 0;
					break;
				}

				if(toSpend <= 0) {
					break;
				}
			}
			UpdateSpecops();
			Print("[Overthrow.OccupyingFactionManager] Reserve Resources: " + m_iResources.ToString());

		}
		//If we have a surplus of resources, try to take a random base back
		float rand = s_AIRandomGenerator.RandFloat01();
		if(time.m_iMinutes == 0 && m_iResources > 2000 && m_bCounterAttackTimeout == 0 && rand > 0.9)
		{
			Print("[Overthrow.OccupyingFactionManager] Surplus of resources, attempting counter attack");
			OVT_BaseData randomBase = m_Bases[s_AIRandomGenerator.RandInt(0,m_Bases.Count()-1)];
			if(!randomBase.IsOccupyingFaction())
			{
				OVT_BaseControllerComponent base = GetBase(randomBase.entId);
				StartBaseQRF(base);
				int timeout = Math.RandomIntInclusive(m_Config.m_Difficulty.counterAttackTimeout - 20, m_Config.m_Difficulty.counterAttackTimeout + 20);
				m_bCounterAttackTimeout = timeout; //Hold off counter attacks for a little
				return;
			}
		}

		//Every 15 mins reduce threat and check if we wanna start a battle for a town
		if(time.m_iMinutes == 0
			|| time.m_iMinutes == 15
			|| time.m_iMinutes == 30
			|| time.m_iMinutes == 45)
		{
			int threatReduce = Math.Ceil((float)m_iThreat * OVT_Global.GetDifficulty().threatReductionFactor);
			m_iThreat -= threatReduce;
			if(m_iThreat < 0) m_iThreat = 0;
			
			Print("[Overthrow.OccupyingFactionManager] Reduced Threat to: " + m_iThreat.ToString());

			int playerFaction = m_Config.GetPlayerFactionIndex();
			int occupyingFaction = m_Config.GetOccupyingFactionIndex();

			foreach(OVT_TownData town : OVT_Global.GetTowns().m_Towns)
			{
				if(town.size == 1) continue;
				if(!OVT_Global.PlayerInRange(town.location, 300)) continue;

				int support = town.SupportPercentage();
				if(town.IsOccupyingFaction())
				{
					if(support > 75)
					{
						StartTownQRF(town);
						break;
					}
				}else{
					if(support < 25)
					{
						StartTownQRF(town);
						break;
					}
				}
			}
		}
	}

	protected void UpdateSpecops()
	{
		if(m_iResources > OVT_Global.GetConfig().m_Difficulty.maxQRF)
		{
			//Do Specops
			foreach(OVT_TargetData target : m_aKnownTargets)
			{
				if(target.completed) continue;
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
				if(m_iResources < OVT_Global.GetConfig().m_Difficulty.maxQRF) {
					break;
				}
			}
		}
	}

	void UpdateKnownTargets()
	{
		//To-Do: target discovery not by magic
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		
		foreach(OVT_CampData fob : resistance.m_Camps)
		{
			if(!IsKnownTarget(fob.location))
			{
				OVT_TargetData target = new OVT_TargetData();
				target.location = fob.location;
				target.type = OVT_TargetType.CAMP;
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
		
		foreach(OVT_RadioTowerData data : m_RadioTowers)
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
				target.type = OVT_TargetType.BROADCAST_TOWER;
				target.order = OVT_OrderType.ATTACK;
				m_aKnownTargets.Insert(target);
			}
		}

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
		
		foreach(OVT_CampData fob : resistance.m_Camps)
		{
			if(!IsKnownTarget(fob.location))
			{
				OVT_TargetData target = new OVT_TargetData();
				target.location = fob.location;
				target.type = OVT_TargetType.CAMP;
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

	int GainResources()
	{
		Print("[Overthrow.OccupyingFactionManager] Gaining Resources");
		Print("[Overthrow.OccupyingFactionManager] Current Threat: " + m_iThreat.ToString());
		float threatFactor = m_iThreat / 1000;
		if(threatFactor > 4) threatFactor = 4;
		int newResources = m_Config.m_Difficulty.baseResourcesPerTick + (m_Config.m_Difficulty.resourcesPerTick * threatFactor);

		int numPlayersOnline = GetGame().GetPlayerManager().GetPlayerCount();

		//Scale resources by number of players online
		if(numPlayersOnline > 32)
		{
			newResources *= 6;
		}else if(numPlayersOnline > 24)
		{
			newResources *= 5;
		}else if(numPlayersOnline > 16)
		{
			newResources *= 4;
		}else if(numPlayersOnline > 8)
		{
			newResources *= 3;
		}else if(numPlayersOnline > 4)
		{
			newResources *= 2;
		}

		m_iResources += newResources;

		Print ("[Overthrow.OccupyingFactionManager] Gained Resources: " + newResources.ToString());
		
		// Allocate resources to deployment manager if it's running low
		AllocateDeploymentResourcesIfNeeded(newResources);

		return newResources;
	}

	//------------------------------------------------------------------------------------------------
	// Deployment Manager Resource Allocation
	//------------------------------------------------------------------------------------------------
	protected void AllocateDeploymentResources(int amount)
	{
		OVT_DeploymentManagerComponent deploymentManager = OVT_Global.GetDeploymentManager();
		if (!deploymentManager)
			return;
			
		int occupyingFactionIndex = OVT_Global.GetConfig().GetOccupyingFactionIndex();
		deploymentManager.AddFactionResources(occupyingFactionIndex, amount);
		
		Print(string.Format("[Overthrow.OccupyingFactionManager] Allocated %1 resources to deployment manager", amount));
	}
	
	//------------------------------------------------------------------------------------------------
	protected void AllocateDeploymentResourcesIfNeeded(int newResources)
	{
		OVT_DeploymentManagerComponent deploymentManager = OVT_Global.GetDeploymentManager();
		if (!deploymentManager)
			return;
			
		int occupyingFactionIndex = OVT_Global.GetConfig().GetOccupyingFactionIndex();
		int deploymentResources = deploymentManager.GetFactionResources(occupyingFactionIndex);
		
		// If deployment manager has less than 500 resources and we have surplus
		if (deploymentResources < 500 && m_iResources > 1000)
		{
			int toAllocate = Math.Min(newResources / 2, m_iResources - 1000);
			if (toAllocate > 0)
			{
				AllocateDeploymentResources(toAllocate);
				m_iResources -= toAllocate;
			}
		}
	}

	void OnAIKilled(IEntity ai, IEntity instigator)
	{
		if(!Replication.IsServer()) return;

		m_iThreat += 5;

		m_OnAIKilled.Invoke(ai, instigator);
	}

	//RPC Methods

	override bool RplSave(ScriptBitWriter writer)
	{
		//Send JIP factions
		writer.WriteString(m_Config.m_sOccupyingFaction);
		writer.WriteString(m_Config.m_sPlayerFaction);

		//Send JIP bases
		writer.WriteInt(m_Bases.Count());
		for(int i=0; i<m_Bases.Count(); i++)
		{
			OVT_BaseData data = m_Bases[i];
			writer.WriteVector(data.location);
			writer.WriteInt(data.faction);
		}

		//Send JIP radio towers
		writer.WriteInt(m_RadioTowers.Count());
		for(int i=0; i<m_RadioTowers.Count(); i++)
		{
			OVT_RadioTowerData data = m_RadioTowers[i];
			writer.WriteVector(data.location);
			writer.WriteInt(data.faction);
		}

		writer.WriteVector(m_vQRFLocation);
		writer.WriteInt(m_iQRFPoints);
		writer.WriteInt(m_iQRFTimer);
		writer.WriteBool(m_bQRFActive);

		return true;
	}

	override bool RplLoad(ScriptBitReader reader)
	{
		int length;
		RplId id;

		if(!reader.ReadString(m_Config.m_sOccupyingFaction)) return false;
		if(!reader.ReadString(m_Config.m_sPlayerFaction)) return false;

		FactionManager fm = GetGame().GetFactionManager();
		m_Config.m_iOccupyingFactionIndex = fm.GetFactionIndex(fm.GetFactionByKey(m_Config.m_sOccupyingFaction));
		m_Config.m_iPlayerFactionIndex = fm.GetFactionIndex(fm.GetFactionByKey(m_Config.m_sPlayerFaction));

		//Recieve JIP bases
		if (!reader.ReadInt(length)) return false;
		for(int i=0; i<length; i++)
		{
			OVT_BaseData base = new OVT_BaseData();

			if (!reader.ReadVector(base.location)) return false;
			if (!reader.ReadInt(base.faction)) return false;

			base.id = i;
			m_Bases.Insert(base);
		}

		//Recieve JIP radio towers
		if (!reader.ReadInt(length)) return false;
		for(int i=0; i<length; i++)
		{
			OVT_RadioTowerData base = new OVT_RadioTowerData();

			if (!reader.ReadVector(base.location)) return false;
			if (!reader.ReadInt(base.faction)) return false;

			base.id = i;
			m_RadioTowers.Insert(base);
		}
		if (!reader.ReadVector(m_vQRFLocation)) return false;
		if (!reader.ReadInt(m_iQRFPoints)) return false;
		if (!reader.ReadInt(m_iQRFTimer)) return false;
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
		SetRadioTowerFaction(pos, faction);
	}

	void SetRadioTowerFaction(vector pos, int faction)
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

}
