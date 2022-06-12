class OVT_VehicleManagerComponentClass: OVT_ComponentClass
{
};

class OVT_VehicleManagerComponent: OVT_Component
{	
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_ShopInventoryConfig> m_aVehicleShopConfigs;
	ref array<ref OVT_ShopInventoryConfig> m_aVehicleShopConfigsPacked = new array<ref OVT_ShopInventoryConfig>();

	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Players starting car", params: "et", category: "Vehicles")]
	ResourceName m_pStartingCarPrefab;
	
	ref map<int, ref set<RplId>> m_mOwned;
	ref array<EntityID> m_aAllVehicleShops;
	
	ref array<EntityID> m_aEntitySearch;
	
	OVT_RealEstateManagerComponent m_RealEstate;
		
	static OVT_VehicleManagerComponent s_Instance;	
	
	static OVT_VehicleManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_VehicleManagerComponent.Cast(pGameMode.FindComponent(OVT_VehicleManagerComponent));
		}

		return s_Instance;
	}
	
	void Init(IEntity owner)
	{		
		m_mOwned = new map<int, ref set<RplId>>;
		m_aAllVehicleShops = new array<EntityID>;	
		m_aEntitySearch = new array<EntityID>;
		
		m_RealEstate = OVT_RealEstateManagerComponent.GetInstance();
	}
	
	void SetOwner(int playerId, IEntity vehicle)
	{
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		Rpc(RpcAsk_SetOwner, playerId, rpl.Id());	
	}
	
	bool IsOwner(int playerId, EntityID entityId)
	{
		if(!m_mOwned.Contains(playerId)) return false;
		IEntity vehicle = GetGame().GetWorld().FindEntityByID(entityId);
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		set<RplId> owner = m_mOwned[playerId];
		return owner.Contains(rpl.Id());
	}
	
	set<EntityID> GetOwned(int playerId)
	{
		if(!m_mOwned.Contains(playerId)) return new set<EntityID>;
		set<EntityID> entities = new set<EntityID>;
		foreach(RplId id : m_mOwned[playerId])
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			entities.Insert(rpl.GetEntity().GetID());
		}
		return entities;
	}
	
	void SpawnStartingCar(IEntity home, int playerId)
	{		
		vector mat[4];
		
		//Try to find a nice kerb to park next to
		if(FindNearestKerbParking(home.GetOrigin(), 20, mat))
		{
			Print("Spawning car next to kerb");
			SpawnVehicleMatrix(m_pStartingCarPrefab, mat, playerId);
		}else{
			Print("Spawned car next to house");
			//try to spawn next to the house
			vector outMat[4];
			
			home.GetTransform(mat);
			mat[3] = mat[3] + (mat[2] * 10);
			vector pos = mat[3];
		
			vector angles = Math3D.MatrixToAngles(mat);
			angles[0] = angles[0] - 90;
			Math3D.AnglesToMatrix(angles, outMat);
			outMat[3] = pos;
			
			SpawnVehicleMatrix(m_pStartingCarPrefab, outMat, playerId);
		}
	}
	
	bool FindNearestKerbParking(vector pos, float range, out vector outMat[4])
	{
		m_aEntitySearch.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(pos, range, null, FilterKerbAddToArray, EQueryEntitiesFlags.STATIC);
		
		if(m_aEntitySearch.Count() == 0) return false;
		
		float nearestDistance = range;
		IEntity nearest;
		
		foreach(EntityID id : m_aEntitySearch)
		{
			IEntity kerb = GetGame().GetWorld().FindEntityByID(id);
			float distance = vector.Distance(kerb.GetOrigin(), pos);
			if(distance < nearestDistance)
			{
				nearest = kerb;
				nearestDistance = distance;
			}
		}
		
		vector mat[4];
		
		nearest.GetTransform(mat);
			
		mat[3] = mat[3] + (mat[2] * 3);
		
		vector p = mat[3];
	
		vector angles = Math3D.MatrixToAngles(mat);
		angles[0] = angles[0] - 90;
		Math3D.AnglesToMatrix(angles, outMat);
		outMat[3] = p;
		
		return true;
		
	}
	
	bool FilterKerbAddToArray(IEntity entity)
	{
		if(entity.ClassName() == "StaticModelEntity"){
			VObject mesh = entity.GetVObject();
			
			if(mesh){
				string res = mesh.GetResourceName();
				if(res.IndexOf("Pavement_") > -1) m_aEntitySearch.Insert(entity.GetID());
				if(res.IndexOf("Kerb_") > -1) m_aEntitySearch.Insert(entity.GetID());				
			}
		}
		return false;
	}
	
	IEntity SpawnVehicle(ResourceName prefab, vector pos,  int ownerId = 0, float rotation = 0)
	{
		
	}
	
	IEntity SpawnVehicleBehind(ResourceName prefab, IEntity entity, int ownerId)
	{
		vector mat[4];
			
		entity.GetTransform(mat);
		mat[3] = mat[3] + (mat[2] * -5);
		vector pos = mat[3];
			
		vector angles = Math3D.MatrixToAngles(mat);
		angles[0] = angles[0] - 90;
		Math3D.AnglesToMatrix(angles, mat);
		mat[3] = pos;
		
		return SpawnVehicleMatrix(prefab, mat, ownerId);
	}
	
	IEntity SpawnVehicleMatrix(ResourceName prefab, vector mat[4], int ownerId = -1)
	{
		EntitySpawnParams spawnParams = new EntitySpawnParams;
		spawnParams.TransformMode = ETransformMode.WORLD;		
		spawnParams.Transform = mat;
		
		IEntity ent = GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), spawnParams);
		if(!ent)
		{
			Print("Failure to spawn vehicle");
			return null;
		}
		
		if(ownerId > -1) SetOwner(ownerId, ent);
		
		return ent;
	}
	
	//RPC Methods
	override bool RplSave(ScriptBitWriter writer)
	{		
		//Send JIP owned vehicles
		writer.Write(m_mOwned.Count(), 32); 
		for(int i; i<m_mOwned.Count(); i++)
		{		
			set<RplId> ownedArray = m_mOwned.GetElement(i);
			
			writer.Write(m_mOwned.GetKey(i),32);
			writer.Write(ownedArray.Count(),32);
			for(int t; t<ownedArray.Count(); t++)
			{	
				writer.WriteRplId(ownedArray[t]);
			}
		}
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{	
		int length, playerId, ownedlength;
		RplId id;
			
		//Recieve JIP owned vehicles
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{
			if (!reader.Read(playerId, 32)) return false;
			m_mOwned[playerId] = new set<RplId>;
			
			if (!reader.Read(ownedlength, 32)) return false;
			for(int t; t<ownedlength; t++)
			{
				if (!reader.ReadRplId(id)) return false;
				m_mOwned[playerId].Insert(id);
			}			
		}
		return true;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_SetOwner(int playerId, RplId id)
	{
		DoSetOwner(playerId, id);		
		Rpc(RpcDo_SetOwner, playerId, id);		
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetOwner(int playerId, RplId id)
	{
		DoSetOwner(playerId, id);	
	}
	
	void DoSetOwner(int playerId, RplId id)
	{
		if(!m_mOwned.Contains(playerId)) m_mOwned[playerId] = new set<RplId>;
		set<RplId> owner = m_mOwned[playerId];
		owner.Insert(id);
	}
}