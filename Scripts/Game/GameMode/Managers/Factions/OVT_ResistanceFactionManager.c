class OVT_ResistanceFactionManagerClass: OVT_ComponentClass
{	
}

class OVT_CampData
{
	[NonSerialized()]
	int id;
	
	string name;	
	vector location;
	string owner;
	
	[NonSerialized()]
	ref array<ref EntityID> garrisonEntities = {};
	
	ref array<ref ResourceName> garrison = {};
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
	
	[Attribute("", UIWidgets.Object)]
	ResourceName m_pMobileFOBPrefab;
	
	[Attribute("", UIWidgets.Object)]
	ResourceName m_pMobileFOBDeployedPrefab;
	
	ref array<ref OVT_CampData> m_Camps;
	
	OVT_PlayerManagerComponent m_Players;
	
	protected IEntity m_TempVehicle;
	protected SCR_AIGroup m_TempGroup;
	
	[RplProp()]
	bool m_bFOBDeployed = false;
	[RplProp()]
	vector m_vFOBLocation;
	
	ref ScriptInvoker m_OnPlace = new ref ScriptInvoker();
	ref ScriptInvoker m_OnBuild = new ref ScriptInvoker();
	
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
		m_Camps = new array<ref OVT_CampData>;
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
		foreach(OVT_CampData fob : m_Camps)
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
	
	void DeployFOB(RplId vehicle)
	{		
		if(m_bFOBDeployed) return;
		
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
		
		m_bFOBDeployed = true;
		m_vFOBLocation = newveh.GetOrigin();
		Replication.BumpMe();	
	}
	
	void UndeployFOB(RplId vehicle)
	{		
		if(!m_bFOBDeployed) return;
		
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
		
		m_bFOBDeployed = false;
		Replication.BumpMe();
	}
	
	IEntity PlaceItem(int placeableIndex, int prefabIndex, vector pos, vector angles, int playerId, bool runHandler = true)
	{
		OVT_ResistanceFactionManager config = OVT_Global.GetResistanceFaction();
		OVT_Placeable placeable = config.m_PlaceablesConfig.m_aPlaceables[placeableIndex];
		ResourceName res = placeable.m_aPrefabs[prefabIndex];
		
		vector mat[4];
		Math3D.AnglesToMatrix(angles, mat);
		mat[3] = pos;
		
		IEntity entity = OVT_Global.SpawnEntityPrefabMatrix(res, mat);
		
		SCR_AIWorld aiworld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		aiworld.RequestNavmeshRebuildEntity(entity);
		
		if(placeable.handler && runHandler)
		{
			placeable.handler.OnPlace(entity, playerId);
		}
		
		m_OnPlace.Invoke(entity, placeable, playerId);
		
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
		OVT_Faction faction = m_Config.GetPlayerFaction();
		ResourceName res = faction.m_aGroupPrefabSlots[prefabIndex];
				
		SCR_AIGroup group = SpawnGarrison(base, res);
			
		base.garrisonEntities.Insert(group.GetID());	
		
		if(takeSupporters)
		{
			OVT_Global.GetTowns().TakeSupportersFromNearestTown(base.location, group.m_aUnitPrefabSlots.Count());
		}
	}
	
	void AddGarrisonFOB(OVT_CampData fob, int prefabIndex, bool takeSupporters = true)
	{		
		OVT_Faction faction = m_Config.GetPlayerFaction();
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
	
	SCR_AIGroup SpawnGarrisonFOB(OVT_CampData fob, ResourceName res)
	{	
		IEntity entity = OVT_Global.SpawnEntityPrefab(res, fob.location + "1 0 0");
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		
		AIWaypoint wp = m_Config.SpawnDefendWaypoint(fob.location);
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
				AIWaypoint wp = m_Config.SpawnPatrolWaypoint(randomSlot.GetOrigin());
				if(i==0) firstWP = wp;
				queueOfWaypoints.Insert(wp);
				
				AIWaypoint wait = m_Config.SpawnWaitWaypoint(randomSlot.GetOrigin(), s_AIRandomGenerator.RandFloatXY(45, 75));								
				queueOfWaypoints.Insert(wait);
			}
			AIWaypointCycle cycle = AIWaypointCycle.Cast(m_Config.SpawnWaypoint(m_Config.m_pCycleWaypointPrefab, firstWP.GetOrigin()));
			cycle.SetWaypoints(queueOfWaypoints);
			cycle.SetRerunCounter(-1);
			aigroup.AddWaypoint(cycle);
		}
	}
	
	void RegisterCamp(IEntity ent, int playerId)
	{
		vector pos = ent.GetOrigin();	
		
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);	
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(persId);
		OVT_CampData fob = new OVT_CampData;
		fob.owner = persId;
		if(player)
		{
			if(player.camp[0] != 0)
			{
				RpcDo_RemoveCamp(player.camp);
				Rpc(RpcDo_RemoveCamp, player.camp);
				GetGame().GetWorld().QueryEntitiesBySphere(player.camp, 10, null, FindAndDeleteCamps);
			}			
			player.camp = pos;
			fob.name = "#OVT-Place_Camp " + player.name;
		}
		
		fob.location = pos;
		m_Camps.Insert(fob);
				
		Rpc(RpcDo_RegisterCamp, pos, fob.name, playerId);
		OVT_Global.GetNotify().SendTextNotification("PlacedCamp",-1,OVT_Global.GetPlayers().GetPlayerName(playerId),OVT_Global.GetTowns().GetTownName(pos));
	}
	
	protected bool FindAndDeleteCamps(IEntity ent)
	{
		if(ent.ClassName() != "GenericEntity") return false;
		string res = EPF_Utils.GetPrefabName(ent);
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
	
	vector GetNearestCamp(vector pos)
	{
		vector nearestBase;
		float nearest = -1;
		foreach(OVT_CampData fob : m_Camps)
		{
			float distance = vector.Distance(fob.location, pos);
			if(nearest == -1 || distance < nearest){
				nearest = distance;
				nearestBase = fob.location;
			}
		}
		return nearestBase;
	}
	
	OVT_CampData GetNearestCampData(vector pos)
	{
		OVT_CampData nearestBase;
		float nearest = -1;
		foreach(OVT_CampData fob : m_Camps)
		{
			float distance = vector.Distance(fob.location, pos);
			if(nearest == -1 || distance < nearest){
				nearest = distance;
				nearestBase = fob;
			}
		}
		return nearestBase;
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
		
		access.MoveInVehicle(m_TempVehicle, ECompartmentType.Turret);
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
	
	//RPC Methods	
	override bool RplSave(ScriptBitWriter writer)
	{	
		writer.WriteBool(m_bFOBDeployed);
		writer.WriteVector(m_vFOBLocation);
			
		//Send JIP Camps
		writer.WriteInt(m_Camps.Count()); 
		for(int i=0; i<m_Camps.Count(); i++)
		{
			OVT_CampData fob = m_Camps[i];
			writer.WriteString(fob.name);
			writer.WriteVector(fob.location);
		}
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{		
		if(!reader.ReadBool(m_bFOBDeployed)) return false;
		if(!reader.ReadVector(m_vFOBLocation)) return false;
				
		//Recieve JIP Camps
		int length;
		string id;
		vector pos;
		
		if (!reader.ReadInt(length)) return false;
		for(int i=0; i<length; i++)
		{			
			OVT_CampData fob = new OVT_CampData;			
			if (!reader.ReadString(fob.name)) return false;
			if (!reader.ReadVector(fob.location)) return false;
			m_Camps.Insert(fob);
		}
		return true;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RegisterCamp(vector pos, string name, int playerId)
	{		
		OVT_CampData fob = new OVT_CampData;
		fob.location = pos;
		fob.name = name;
		m_Camps.Insert(fob);
		
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		fob.owner = persId;
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(persId);
		if(player)
		{
			player.camp = pos;
		}
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RemoveCamp(vector pos)
	{		
		int index = -1;
		foreach(int t, OVT_CampData camp : m_Camps)
		{
			if(camp.location == pos)
			{
				index = t;
			}
		}
		if(index > -1)
		{
			m_Camps.Remove(index);
		}
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
		if(m_Camps)
		{
			m_Camps.Clear();
			m_Camps = null;
		}			
	}
}