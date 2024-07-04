class OVT_ResistanceFactionManagerClass: OVT_ComponentClass
{	
}

class OVT_FOBData
{
	[NonSerialized()]
	int id;
	
	int faction;	
	vector location;
	
	[NonSerialized()]
	ref array<ref EntityID> garrisonEntities = {};
	
	ref array<ref ResourceName> garrison = {};
	
	bool IsOccupyingFaction()
	{
		return faction == OVT_Global.GetConfig().GetOccupyingFactionIndex();
	}
}

class OVT_VehicleUpgrades : ScriptAndConfig
{
	[Attribute()]
	ResourceName m_pBasePrefab;
	
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_VehicleUpgrade> m_aUpgrades;	
}

class OVT_VehicleUpgrade : ScriptAndConfig
{
	[Attribute()]
	ResourceName m_pUpgradePrefab;
	
	[Attribute(defvalue: "100", desc: "Cost (multiplied by difficulty)")]
	int m_iCost;
}

class OVT_ResistanceFactionManager: OVT_Component
{	
	[Attribute("", UIWidgets.Object)]
	ResourceName m_pMobileFOBPrefab;

	[Attribute("", UIWidgets.Object)]
	ResourceName m_pMobileFOBDeployedPrefab;
	
	[Attribute()]
	ResourceName m_rPlaceablesConfigFile;
	
	ref OVT_PlaceablesConfig m_PlaceablesConfig;
	
	[Attribute()]
	ResourceName m_rBuildablesConfigFile;
	
	ref OVT_BuildablesConfig m_BuildablesConfig;
	
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_VehicleUpgrades> m_aVehicleUpgrades;
	
	[Attribute("", UIWidgets.Object)]
	ResourceName m_pHiredCivilianPrefab;
	
	ref array<ref OVT_FOBData> m_FOBs;
	ref array<ref OVT_CampData> m_Camps;
	
	OVT_PlayerManagerComponent m_Players;
	
	protected IEntity m_TempVehicle;
	protected SCR_AIGroup m_TempGroup;
	
	ref ScriptInvoker m_OnPlace = new ScriptInvoker();
	ref ScriptInvoker m_OnBuild = new ScriptInvoker();
	
	static OVT_ResistanceFactionManager s_Instance;
	
	static OVT_ResistanceFactionManager GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_ResistanceFactionManager.Cast(pGameMode.FindComponent(OVT_ResistanceFactionManager));
		}

		return s_Instance;
	}
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		m_Players = OVT_Global.GetPlayers();
		
		if (SCR_Global.IsEditMode()) return;
		LoadConfigs();
	}
	
	void OVT_ResistanceFactionManager(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_FOBs = new array<ref OVT_FOBData>;
	}
	
	void Init(IEntity owner)
	{
		GetGame().GetCallqueue().CallLater(RegisterUpgrades, 0);		
	}
	
	protected void LoadConfigs()
	{
		Resource holder = BaseContainerTools.LoadContainer(m_rPlaceablesConfigFile);
		if (holder)		
		{
			OVT_PlaceablesConfig obj = OVT_PlaceablesConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(holder.GetResource().ToBaseContainer()));
			if(obj)
			{
				m_PlaceablesConfig = obj;
			}
		}
		
		holder = BaseContainerTools.LoadContainer(m_rBuildablesConfigFile);
		if (holder)		
		{
			OVT_BuildablesConfig obj = OVT_BuildablesConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(holder.GetResource().ToBaseContainer()));
			if(obj)
			{
				m_BuildablesConfig = obj;
			}
		}
	}
	
	void RegisterUpgrades()
	{
		//Register vehicle upgrade resources with the economy
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		foreach(OVT_VehicleUpgrades upgrades : m_aVehicleUpgrades)
		{
			foreach(OVT_VehicleUpgrade upgrade : upgrades.m_aUpgrades)
			{
				economy.RegisterResource(upgrade.m_pUpgradePrefab);				
			}
		}
	}
	
	void PostGameStart()
	{
		GetGame().GetCallqueue().CallLater(SpawnGarrisons, 0);
	}	
	
	protected void SpawnGarrisons()
	{	
		foreach(OVT_FOBData fob : m_FOBs)
		{
			foreach(ResourceName res : fob.garrison)
			{
				fob.garrisonEntities.Insert(OVT_Global.GetResistanceFaction().SpawnGarrisonFOB(fob, res).GetID());
			}
		}
	}
	
	bool IsOfficer(int playerId)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(persId);
		return player.isOfficer;
	}
	
	bool IsLocalPlayerOfficer()
	{
		return IsOfficer(SCR_PlayerController.GetLocalPlayerId());
	}
	
	void AddOfficer(int playerId)
	{
		RpcDo_AddOfficer(playerId);
		Rpc(RpcDo_AddOfficer, playerId);
	}
	
	IEntity PlaceItem(int placeableIndex, int prefabIndex, vector pos, vector angles, int playerId, bool runHandler = true)
	{
		OVT_ResistanceFactionManager config = OVT_Global.GetResistanceFaction();
		OVT_Placeable placeable = config.m_PlaceablesConfig.m_aPlaceables[placeableIndex];
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		
		ResourceName res = placeable.m_aPrefabs[prefabIndex];
				
		vector mat[4];
		Math3D.AnglesToMatrix(angles, mat);
		mat[3] = pos;
		
		IEntity entity = OVT_Global.SpawnEntityPrefabMatrix(res, mat);
		
		if(placeable.handler && runHandler)
		{
			if(!placeable.handler.OnPlace(entity, playerId))
			{
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
				return null;
			}
		}
		
		economy.TakePlayerMoney(playerId, m_Config.GetPlaceableCost(placeable));
		
		SCR_AIWorld aiworld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		aiworld.RequestNavmeshRebuildEntity(entity);
		
		m_OnPlace.Invoke(entity, placeable, playerId);
		
		OVT_PlayerOwnerComponent playerowner = EPF_Component<OVT_PlayerOwnerComponent>.Find(entity);
		if(playerowner)
		{
			string playerUid = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
			playerowner.SetPlayerOwner(playerUid);
			playerowner.SetLocked(false);
		}
		
		return entity;
	}
	
	IEntity BuildItem(int buildableIndex, int prefabIndex, vector pos, vector angles, int playerId, bool runHandler = true)
	{
		OVT_ResistanceFactionManager config = OVT_Global.GetResistanceFaction();
		OVT_Buildable buildable = config.m_BuildablesConfig.m_aBuildables[buildableIndex];
		ResourceName res = buildable.m_aPrefabs[prefabIndex];
		
		vector mat[4];
		Math3D.AnglesToMatrix(angles, mat);
		mat[3] = pos;
		
		IEntity entity = OVT_Global.SpawnEntityPrefabMatrix(res, mat);
		
		SCR_AIWorld aiworld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		aiworld.RequestNavmeshRebuildEntity(entity);
		
		if(buildable.handler && runHandler)
		{
			buildable.handler.OnPlace(entity, playerId);
		}
		
		m_OnBuild.Invoke(entity, buildable, playerId);
		
		return entity;
	}
	
	void AddGarrison(int baseId, int prefabIndex, bool takeSupporters = true)
	{
		OVT_BaseData base = OVT_Global.GetOccupyingFaction().m_Bases[baseId];
		OVT_Faction faction = OVT_Global.GetConfig().GetPlayerFaction();
		ResourceName res = faction.m_aGroupPrefabSlots[prefabIndex];
				
		SCR_AIGroup group = SpawnGarrison(base, res);
			
		base.garrisonEntities.Insert(group.GetID());	
		
		if(takeSupporters)
		{
			OVT_Global.GetTowns().TakeSupportersFromNearestTown(base.location, group.m_aUnitPrefabSlots.Count());
		}
	}
	
	void AddGarrisonFOB(OVT_FOBData fob, int prefabIndex, bool takeSupporters = true)
	{		
		OVT_Faction faction = OVT_Global.GetConfig().GetPlayerFaction();
		ResourceName res = faction.m_aGroupPrefabSlots[prefabIndex];
				
		SCR_AIGroup group = SpawnGarrisonFOB(fob, res);
		
		fob.garrisonEntities.Insert(group.GetID());
				
		if(takeSupporters)
		{
			OVT_Global.GetTowns().TakeSupportersFromNearestTown(fob.location, group.m_aUnitPrefabSlots.Count());
		}
	}
	
	SCR_AIGroup SpawnGarrison(OVT_BaseData base, ResourceName res)
	{		
		IEntity entity = OVT_Global.SpawnEntityPrefab(res, base.location);
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		AddPatrolWaypoints(group, base);
		return group;
	}
	
	SCR_AIGroup SpawnGarrisonFOB(OVT_FOBData fob, ResourceName res)
	{	
		IEntity entity = OVT_Global.SpawnEntityPrefab(res, fob.location + "1 0 0");
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		
		AIWaypoint wp = OVT_Global.GetConfig().SpawnDefendWaypoint(fob.location);
		group.AddWaypoint(wp);	
		
		return group;
	}
	
	protected void AddPatrolWaypoints(SCR_AIGroup aigroup, OVT_BaseData base)
	{
		OVT_BaseControllerComponent controller = OVT_Global.GetOccupyingFaction().GetBase(base.entId);
		array<AIWaypoint> queueOfWaypoints = new array<AIWaypoint>();
		
		if(controller.m_AllCloseSlots.Count() > 2)
		{
			AIWaypoint firstWP;
			for(int i=0; i< 3; i++)
			{
				IEntity randomSlot = GetGame().GetWorld().FindEntityByID(controller.m_AllCloseSlots.GetRandomElement());
				AIWaypoint wp = OVT_Global.GetConfig().SpawnPatrolWaypoint(randomSlot.GetOrigin());
				if(i==0) firstWP = wp;
				queueOfWaypoints.Insert(wp);
				
				AIWaypoint wait = OVT_Global.GetConfig().SpawnWaitWaypoint(randomSlot.GetOrigin(), s_AIRandomGenerator.RandFloatXY(45, 75));								
				queueOfWaypoints.Insert(wait);
			}
			AIWaypointCycle cycle = AIWaypointCycle.Cast(OVT_Global.GetConfig().SpawnWaypoint(OVT_Global.GetConfig().m_pCycleWaypointPrefab, firstWP.GetOrigin()));
			cycle.SetWaypoints(queueOfWaypoints);
			cycle.SetRerunCounter(-1);
			aigroup.AddWaypoint(cycle);
		}
	}
	
	void RegisterFOB(IEntity ent, int playerId)
	{	
		vector pos = ent.GetOrigin();	
		OVT_FOBData fob = new OVT_FOBData;
		fob.faction = OVT_Global.GetConfig().GetPlayerFactionIndex();
		fob.location = pos;
		m_FOBs.Insert(fob);
				
		Rpc(RpcDo_RegisterFOB, pos);
		OVT_Global.GetNotify().SendTextNotification("PlacedFOB",-1,OVT_Global.GetPlayers().GetPlayerName(playerId),OVT_Global.GetTowns().GetTownName(pos));
	}
	
	void UnregisterFOB(vector pos)
	{	
		OVT_FOBData fob = GetNearestFOBData(pos);
		
		m_FOBs.RemoveItem(fob);
				
		Rpc(RpcDo_UnregisterFOB, pos);		
	}
	
	void RegisterCamp(IEntity ent, int playerId)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		vector pos = ent.GetOrigin();
				
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(persId);
		if(player)
		{
			player.camp = pos;
		}
				
		Rpc(RpcDo_RegisterCamp, pos, playerId);
	}
	
	protected bool FindAndDeleteCamps(IEntity ent)
	{
		string res = ent.GetPrefabData().GetPrefabName();
		if(res.Contains("TentSmallUS"))
		{
			SCR_EntityHelper.DeleteEntityAndChildren(ent);
		}
		return false;
	}
	
	float DistanceToCamp(vector pos, string playerId)
	{
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(playerId);
		if(!player) return 99999;
		if(player.camp[0] == 0) return 99999;
		
		return vector.Distance(player.camp, pos);
	}
	
	vector GetNearestFOB(vector pos)
	{
		vector nearestBase;
		float nearest = -1;
		foreach(OVT_FOBData fob : m_FOBs)
		{
			if(fob.IsOccupyingFaction()) continue;
			float distance = vector.Distance(fob.location, pos);
			if(nearest == -1 || distance < nearest){
				nearest = distance;
				nearestBase = fob.location;
			}
		}
		return nearestBase;
	}
	
	OVT_FOBData GetNearestFOBData(vector pos)
	{
		OVT_FOBData nearestBase;
		float nearest = -1;
		foreach(OVT_FOBData fob : m_FOBs)
		{
			if(fob.IsOccupyingFaction()) continue;
			float distance = vector.Distance(fob.location, pos);
			if(nearest == -1 || distance < nearest){
				nearest = distance;
				nearestBase = fob;
			}
		}
		return nearestBase;
	}
	
	OVT_CampData GetNearestCampData(vector pos, string playerPersId = "")
	{
		OVT_CampData nearestCamp;
		float nearest = -1;
		foreach(OVT_CampData camp : m_Camps)
		{
			if(playerPersId != "" && camp.playerPersId != playerPersId) continue;
			if(playerPersId == "" && !camp.isPublic) continue;
			
			float distance = vector.Distance(camp.location, pos);
			if(nearest == -1 || distance < nearest){
				nearest = distance;
				nearestCamp = camp;
			}
		}
		return nearestCamp;
	}
	
	vector GetNearestCamp(vector pos, string playerPersId = "")
	{
		OVT_CampData nearestCamp;
		float nearest = -1;
		foreach(OVT_CampData camp : m_Camps)
		{
			if(playerPersId != "" && camp.playerPersId != playerPersId) continue;
			if(playerPersId == "" && !camp.isPublic) continue;
			
			float distance = vector.Distance(camp.location, pos);
			if(nearest == -1 || distance < nearest){
				nearest = distance;
				nearestCamp = camp;
			}
		}
		return nearestCamp.location;
	}
	
	protected void MoveInGunner()
	{
		array<AIAgent> agents = {};
		m_TempGroup.GetAgents(agents);
		if(agents.Count() == 0) return;
		
		AIAgent dude = agents[0];
		IEntity ent = dude.GetControlledEntity();
		
		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(ent.FindComponent(SCR_CompartmentAccessComponent));
		if(!access) return;
		
		access.MoveInVehicle(m_TempVehicle, ECompartmentType.TURRET);
	}
	
	void SpawnGunner(RplId turret, int playerId = -1, bool takeSupporter = true)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(turret));
		if(!rpl) return;
		
		IEntity turretEntity = rpl.GetEntity();	
		IEntity vehicle = turretEntity.GetParent();
		if(!vehicle) vehicle = turretEntity;	
				
		IEntity group = OVT_Global.SpawnEntityPrefab(m_pHiredCivilianPrefab, vehicle.GetOrigin());
		SCR_AIGroup aigroup = SCR_AIGroup.Cast(group);
		if(!aigroup) return;
		
		OVT_Global.RandomizeCivilianClothes(aigroup);
		
		m_TempVehicle = vehicle;
		m_TempGroup = aigroup;
		
		GetGame().GetCallqueue().CallLater(MoveInGunner, 5);
		
		if(takeSupporter)
		{
			OVT_Global.GetTowns().TakeSupportersFromNearestTown(turretEntity.GetOrigin());
		}		
	}
	
	void DeployFOB(RplId vehicle)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(vehicle));
		if(!rpl) return;
		IEntity entity = rpl.GetEntity();

		OVT_VehicleManagerComponent vm = OVT_Global.GetVehicles();

		string ownerId = vm.GetOwnerID(entity);

		vector mat[4];
		entity.GetTransform(mat);

		IEntity newveh = vm.SpawnVehicleMatrix(m_pMobileFOBDeployedPrefab, mat, ownerId);
		RplComponent newrpl = RplComponent.Cast(newveh.FindComponent(RplComponent));

		OVT_Global.GetVehicles().m_aVehicles.RemoveItem(entity.GetID());

		OVT_Global.TransferStorage(vehicle, newrpl.Id());
		SCR_EntityHelper.DeleteEntityAndChildren(entity);	

		RegisterFOB(entity, -1);		

	}
	
	void UndeployFOB(RplId vehicle)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(vehicle));
		if(!rpl) return;
		IEntity entity = rpl.GetEntity();

		OVT_VehicleManagerComponent vm = OVT_Global.GetVehicles();

		string ownerId = vm.GetOwnerID(entity);

		vector mat[4];
		entity.GetTransform(mat);

		IEntity newveh = vm.SpawnVehicleMatrix(m_pMobileFOBPrefab, mat, ownerId);
		RplComponent newrpl = RplComponent.Cast(newveh.FindComponent(RplComponent));

		OVT_Global.GetVehicles().m_aVehicles.RemoveItem(entity.GetID());

		OVT_Global.TransferStorage(vehicle, newrpl.Id());
		SCR_EntityHelper.DeleteEntityAndChildren(entity);		

		UnregisterFOB(entity.GetOrigin());
	}
	
	//RPC Methods	
	override bool RplSave(ScriptBitWriter writer)
	{	
			
		//Send JIP FOBs
		writer.WriteInt(m_FOBs.Count()); 
		for(int i=0; i<m_FOBs.Count(); i++)
		{
			OVT_FOBData fob = m_FOBs[i];
			writer.Write(fob.faction, 32);
			writer.WriteVector(fob.location);
		}
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{		
				
		//Recieve JIP FOBs
		int length;
		string id;
		vector pos;
		
		if (!reader.ReadInt(length)) return false;
		for(int i=0; i<length; i++)
		{			
			OVT_FOBData fob = new OVT_FOBData;			
			if (!reader.Read(fob.faction, 32)) return false;
			if (!reader.ReadVector(fob.location)) return false;
			m_FOBs.Insert(fob);
		}
		return true;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RegisterFOB(vector pos)
	{
		OVT_FOBData fob = new OVT_FOBData;
		fob.location = pos;
		fob.faction = m_Config.GetPlayerFactionIndex();
		m_FOBs.Insert(fob);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_UnregisterFOB(vector pos)
	{
		OVT_FOBData fob = GetNearestFOBData(pos);
		m_FOBs.RemoveItem(fob);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RegisterCamp(vector pos, int playerId)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(persId);
		if(player)
		{
			player.camp = pos;
		}
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_UnregisterCamp(vector pos)
	{
		OVT_CampData camp = GetNearestCampData(pos);
		m_Camps.RemoveItem(camp);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_AddOfficer(int playerId)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		if(IsOfficer(playerId)) return;
		
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(persId);
		if(!player) return;
		
		player.isOfficer = true;
		if(playerId == SCR_PlayerController.GetLocalPlayerId())
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-NewOfficerYou", "", 10, true);
		}else{
			string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
			SCR_HintManagerComponent.GetInstance().ShowCustom(playerName + " #OVT-NewOfficer", "", 10, true);
		}
	}
	
	void ~OVT_ResistanceFactionManager()
	{		
		if(m_FOBs)
		{
			m_FOBs.Clear();
			m_FOBs = null;
		}	
		
		if(m_Camps)
		{
			m_Camps.Clear();
			m_Camps = null;
		}		
	}
}