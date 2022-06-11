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
	
	ref map<int, ref set<EntityID>> m_mOwned;
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
		m_mOwned = new map<int, ref set<EntityID>>;
		m_aAllVehicleShops = new array<EntityID>;	
		m_aEntitySearch = new array<EntityID>;
		
		m_RealEstate = OVT_RealEstateManagerComponent.GetInstance();
	}
		
	void SetOwner(int playerId, EntityID entityId)
	{
		if(!m_mOwned.Contains(playerId)) m_mOwned[playerId] = new set<EntityID>;
		set<EntityID> owner = m_mOwned[playerId];
		owner.Insert(entityId);
	}
	
	bool IsOwner(int playerId, EntityID entityId)
	{
		if(!m_mOwned.Contains(playerId)) return false;
		set<EntityID> owner = m_mOwned[playerId];
		return owner.Contains(entityId);
	}
	
	set<EntityID> GetOwned(int playerId)
	{
		if(!m_mOwned.Contains(playerId)) return new set<EntityID>;
		return m_mOwned[playerId];
	}
	
	void SpawnStartingCar(int playerId)
	{
		IEntity home = m_RealEstate.GetHome(playerId);
		
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
		
		if(ownerId > -1) SetOwner(ownerId, ent.GetID());
		
		return ent;
	}
}