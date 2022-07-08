class OVT_ResistanceFactionManagerClass: OVT_ComponentClass
{	
}

class OVT_Placeable : ScriptAndConfig
{
	[Attribute()]
	string name;
		
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Object Prefabs", params: "et")]
	ref array<ResourceName> m_aPrefabs;
	
	[Attribute("", UIWidgets.ResourceNamePicker, "", "edds")]
	ResourceName m_tPreview;
	
	[Attribute(defvalue: "100", desc: "Cost (multiplied by difficulty)")]
	int m_iCost;
	
	[Attribute(defvalue: "0", desc: "Place on walls")]
	bool m_bPlaceOnWall;
	
	[Attribute(defvalue: "0", desc: "Can place it anywhere")]
	bool m_bIgnoreLocation;
	
	[Attribute(defvalue: "0", desc: "Cannot place near towns or bases")]
	bool m_bAwayFromTownsBases;
	
	[Attribute(defvalue: "0", desc: "Must be placed near a town")]
	bool m_bNearTown;
	
	[Attribute("", UIWidgets.Object)]
	ref OVT_PlaceableHandler handler;
}

class OVT_Buildable : ScriptAndConfig
{
	[Attribute()]
	string name;
		
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Structure Prefabs", params: "et")]
	ref array<ResourceName> m_aPrefabs;
	
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Furniture Prefabs", params: "et")]
	ref array<ResourceName> m_aFurniturePrefabs;
	
	[Attribute("", UIWidgets.ResourceNamePicker, "", "edds")]
	ResourceName m_tPreview;
	
	[Attribute(defvalue: "100", desc: "Cost (multiplied by difficulty)")]
	int m_iCost;
	
	[Attribute(defvalue: "0", desc: "Can build at a base")]
	bool m_bBuildAtBase;
	
	[Attribute(defvalue: "0", desc: "Can build in a town")]
	bool m_bBuildInTown;
	
	[Attribute(defvalue: "0", desc: "Can build in a village")]
	bool m_bBuildInVillage;
	
	[Attribute(defvalue: "0", desc: "Can build at an FOB")]
	bool m_bBuildAtFOB;
	
	[Attribute("", UIWidgets.Object)]
	ref OVT_PlaceableHandler handler;
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
	ref array<ref OVT_Placeable> m_aPlaceables;
	
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_Buildable> m_aBuildables;
	
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_VehicleUpgrades> m_aVehicleUpgrades;
	
	ref array<vector> m_FOBs;
	ref array<EntityID> m_Placed;
	ref array<EntityID> m_Built;
	
	ref array<string> m_Officers;
	ref map<ref string,ref vector> m_mCamps;
	ref map<ref string,ref vector> m_mPlayerPositions;
	
	OVT_PlayerManagerComponent m_Players;
	
	
	
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
	}
	
	void OVT_ResistanceFactionManager()
	{
		m_FOBs = new array<vector>;	
		m_Placed = new array<EntityID>;	
		m_Built = new array<EntityID>;	
		m_Officers = new array<string>;
		m_mCamps = new map<ref string,ref vector>;
		m_mPlayerPositions = new map<ref string,ref vector>;
	}
	
	void Init(IEntity owner)
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
	
	bool IsOfficer(int playerId)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		return m_Officers.Contains(persId);
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
		OVT_Placeable placeable = config.m_aPlaceables[placeableIndex];
		ResourceName res = placeable.m_aPrefabs[prefabIndex];
		
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		vector mat[4];
		Math3D.AnglesToMatrix(angles, mat);
		mat[3] = pos;
		params.Transform = mat;
		
		IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(res), GetGame().GetWorld(), params);
		
		if(placeable.handler && runHandler)
		{
			placeable.handler.OnPlace(entity, playerId);
		}
		
		RegisterPlaceable(entity);
		
		return entity;
	}
	
	IEntity BuildItem(int buildableIndex, int prefabIndex, vector pos, vector angles, int playerId, bool runHandler = true)
	{
		OVT_ResistanceFactionManager config = OVT_Global.GetResistanceFaction();
		OVT_Buildable buildable = config.m_aBuildables[buildableIndex];
		ResourceName res = buildable.m_aPrefabs[prefabIndex];
		
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		vector mat[4];
		Math3D.AnglesToMatrix(angles, mat);
		mat[3] = pos;
		params.Transform = mat;
		
		IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(res), GetGame().GetWorld(), params);
		
		if(buildable.handler && runHandler)
		{
			buildable.handler.OnPlace(entity, playerId);
		}
		
		RegisterBuildable(entity);
		
		return entity;
	}
	
	void AddGarrison(int baseId, int prefabIndex)
	{
		OVT_BaseData base = OVT_Global.GetOccupyingFaction().m_Bases[baseId];
		OVT_Faction faction = m_Config.GetPlayerFaction();
		ResourceName res = faction.m_aGroupPrefabSlots[prefabIndex];
				
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = base.location;
		
		IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(res), GetGame().GetWorld(), params);
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		AddPatrolWaypoints(group, base);
			
		base.garrison.Insert(entity.GetID());	
	}
	
	protected void AddPatrolWaypoints(SCR_AIGroup aigroup, OVT_BaseData base)
	{
		OVT_BaseControllerComponent controller = OVT_Global.GetOccupyingFaction().GetBase(base.entId);
		array<AIWaypoint> queueOfWaypoints = new array<AIWaypoint>();
		
		if(controller.m_AllCloseSlots.Count() > 2)
		{
			AIWaypoint firstWP;
			for(int i; i< 3; i++)
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
	
	void RegisterPlaceable(IEntity ent)
	{
		m_Placed.Insert(ent.GetID());
	}
	
	void RegisterBuildable(IEntity ent)
	{
		m_Built.Insert(ent.GetID());
	}
	
	void RegisterFOB(IEntity ent, int playerId)
	{	
		vector pos = ent.GetOrigin();	
		m_FOBs.Insert(pos);
				
		Rpc(RpcDo_RegisterFOB, pos);
		m_Players.HintMessageAll("PlacedFOB",-1,playerId);
	}
	
	void RegisterCamp(IEntity ent, int playerId)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		vector pos = ent.GetOrigin();
		if(m_mCamps.Contains(persId))
		{
			GetGame().GetWorld().QueryEntitiesBySphere(m_mCamps[persId], 15, null, FindAndDeleteCamps, EQueryEntitiesFlags.ALL);
		}
		m_mCamps[persId] = pos;
				
		Rpc(RpcDo_RegisterCamp, pos, playerId);
	}
	
	protected bool FindAndDeleteCamps(IEntity ent)
	{
		string res = ent.GetPrefabData().GetPrefabName();
		if(res.Contains("TentSmallUS"))
		{
			SCR_Global.DeleteEntityAndChildren(ent);
		}
		return false;
	}
	
	float DistanceToNearestCamp(vector pos)
	{
		float nearest = 999999;
		for(int i =0; i<m_mCamps.Count(); i++)
		{
			float dist = vector.Distance(pos, m_mCamps.GetElement(i));
			if(dist < nearest) nearest = dist;
		}
		return nearest;
	}
	
	vector GetNearestFOB(vector pos)
	{
		vector nearestBase;
		float nearest = 9999999;
		foreach(vector fob : m_FOBs)
		{			
			float distance = vector.Distance(fob, pos);
			if(distance < nearest){
				nearest = distance;
				nearestBase = fob;
			}
		}
		return nearestBase;
	}
	
	//RPC Methods	
	override bool RplSave(ScriptBitWriter writer)
	{		
		//Send JIP FOBs
		writer.WriteInt(m_FOBs.Count()); 
		for(int i; i<m_FOBs.Count(); i++)
		{
			writer.WriteVector(m_FOBs[i]);
		}
		
		//Send JIP officers
		writer.WriteInt(m_Officers.Count()); 
		for(int i; i<m_Officers.Count(); i++)
		{
			RPL_WritePlayerID(writer, m_Officers[i]);
		}
		
		//Send JIP Camps
		writer.WriteInt(m_mCamps.Count()); 
		for(int i; i<m_mCamps.Count(); i++)
		{
			RPL_WritePlayerID(writer, m_mCamps.GetKey(i));
			writer.WriteVector(m_mCamps.GetElement(i));
		}
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{				
		//Recieve JIP FOBs
		int length;
		string id;
		vector fob;
		
		if (!reader.ReadInt(length)) return false;
		for(int i; i<length; i++)
		{			
			if (!reader.ReadVector(fob)) return false;
			m_FOBs.Insert(fob);
		}
		
		//Recieve JIP Officers
		if (!reader.ReadInt(length)) return false;
		for(int i; i<length; i++)
		{			
			if (!RPL_ReadPlayerID(reader, id)) return false;
			m_Officers.Insert(id);
		}
		
		//Recieve JIP Camps
		if (!reader.ReadInt(length)) return false;
		for(int i; i<length; i++)
		{			
			if (!RPL_ReadPlayerID(reader, id)) return false;
			if (!reader.ReadVector(fob)) return false;
			m_mCamps[id] = fob;
		}
		return true;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RegisterFOB(vector pos)
	{
		m_FOBs.Insert(pos);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RegisterCamp(vector pos, int playerId)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		m_mCamps[persId] = pos;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_AddOfficer(int playerId)
	{
		m_Officers.Insert(OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId));
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
	}
}