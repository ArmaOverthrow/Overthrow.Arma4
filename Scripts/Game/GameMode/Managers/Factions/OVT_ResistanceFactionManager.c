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

class OVT_ResistanceFactionManager: OVT_Component
{	
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_Placeable> m_aPlaceables;
	
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_Buildable> m_aBuildables;
	
	ref array<vector> m_FOBs;
	ref array<EntityID> m_Placed;
	ref array<EntityID> m_Built;
	
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
		Rpc(RpcAsk_RegisterFOB, ent.GetOrigin(), playerId);
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
		writer.Write(m_FOBs.Count(), 32); 
		for(int i; i<m_FOBs.Count(); i++)
		{
			writer.WriteVector(m_FOBs[i]);
		}
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{				
		//Recieve JIP FOBs
		int length;
		vector fob;
		
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{			
			if (!reader.ReadVector(fob)) return false;
			m_FOBs.Insert(fob);
		}
		return true;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RegisterFOB(vector pos, int playerId)
	{
		m_FOBs.Insert(pos);
				
		Rpc(RpcDo_RegisterFOB, pos);
		m_Players.HintMessageAll("PlacedFOB",-1,playerId);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RegisterFOB(vector pos)
	{
		m_FOBs.Insert(pos);
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