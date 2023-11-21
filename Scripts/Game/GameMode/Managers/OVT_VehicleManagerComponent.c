class OVT_VehicleManagerComponentClass: OVT_OwnerManagerComponentClass
{
};

class OVT_VehicleManagerComponent: OVT_OwnerManagerComponent
{	

	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Players starting cars", params: "et", category: "Vehicles")]
	ref array<ResourceName> m_pStartingCarPrefabs;
	
	[Attribute()]
	ref SCR_EntityCatalogMultiList m_CivilianVehicleEntityCatalog;
		
	ref array<EntityID> m_aAllVehicleShops;	
	ref array<EntityID> m_aEntitySearch;
	
	ref array<ref EntityID> m_aVehicles;
	
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
	
	void OVT_VehicleManagerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{		
		m_aAllVehicleShops = new array<EntityID>;	
		m_aEntitySearch = new array<EntityID>;
		m_aVehicles = new array<ref EntityID>;
	}
	
	void Init(IEntity owner)
	{			
		m_RealEstate = OVT_Global.GetRealEstate();
	}
	
	void SpawnStartingCar(IEntity home, string playerId)
	{		
		vector mat[4];
		
		int i = s_AIRandomGenerator.RandInt(0, m_pStartingCarPrefabs.Count()-1);
		ResourceName prefab = m_pStartingCarPrefabs[i];
		
		//Find us a parking spot
		IEntity veh;
		
		if(GetParkingSpot(home, mat, OVT_ParkingType.PARKING_CAR, true))
		{
			
			veh = SpawnVehicleMatrix(prefab, mat, playerId);
			
		}else if(FindNearestKerbParking(home.GetOrigin(), 20, mat))
		{
			Print("Unable to find OVT_ParkingComponent in starting house prefab. Trying to spawn car next to a kerb.");
			veh = SpawnVehicleMatrix(prefab, mat, playerId);
			
		}else{
			Print("Failure to spawn player's starting car. Add OVT_ParkingComponent to all starting house prefabs in config");			
		}
		
		if(veh)
		{
			OVT_PlayerOwnerComponent playerowner = EPF_Component<OVT_PlayerOwnerComponent>.Find(veh);
			if(playerowner) playerowner.SetLocked(true);
		}
	}
	
	bool GetParkingSpot(IEntity building, out vector outMat[4], OVT_ParkingType type = OVT_ParkingType.PARKING_CAR, bool skipObstructionCheck = false)
	{
		OVT_ParkingComponent parking = OVT_ParkingComponent.Cast(building.FindComponent(OVT_ParkingComponent));
		if(!parking) return false;
		return parking.GetParkingSpot(outMat, type, skipObstructionCheck);
	}
	
	bool GetNearestParkingSpot(vector pos, out vector outMat[4], OVT_ParkingType type = OVT_ParkingType.PARKING_CAR)
	{
		m_aEntitySearch.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(pos, 15, null, FilterParkingAddToArray, EQueryEntitiesFlags.ALL);
		
		if(m_aEntitySearch.Count() == 0) return false;
		
		return GetParkingSpot(GetGame().GetWorld().FindEntityByID(m_aEntitySearch[0]), outMat, type);
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
		
		if(!nearest) return false;
		
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
	
	bool FilterParkingAddToArray(IEntity entity)
	{
		if(entity.FindComponent(OVT_ParkingComponent)){
			m_aEntitySearch.Insert(entity.GetID());
		}
		return false;
	}
	
	IEntity SpawnVehicle(ResourceName prefab, vector pos,  int ownerId = 0, float rotation = 0)
	{
		
	}
	
	IEntity SpawnVehicleNearestParking(ResourceName prefab, vector pos,  string ownerId = "")
	{
		vector mat[4];
		if(!GetNearestParkingSpot(pos, mat))
		{
			if(!FindNearestKerbParking(pos, 30, mat))
			{				
				return null;
			}
		}
		return SpawnVehicleMatrix(prefab, mat, ownerId);
	}
	
	IEntity SpawnVehicleBehind(ResourceName prefab, IEntity entity, string ownerId="")
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
	
	IEntity SpawnVehicleMatrix(ResourceName prefab, vector mat[4], string ownerId = "")
	{		
		IEntity ent = OVT_Global.SpawnEntityPrefabMatrix(prefab, mat);
		if(!ent)
		{
			Print("Failure to spawn vehicle");
			return null;
		}
				
		if(ownerId != "") 
		{
			SetOwnerPersistentId(ownerId, ent);
			OVT_PlayerOwnerComponent playerowner = EPF_Component<OVT_PlayerOwnerComponent>.Find(ent);
			if(playerowner)
			{
				playerowner.SetPlayerOwner(ownerId);
			}
		}
		
		m_aVehicles.Insert(ent.GetID());
		
		return ent;
	}
	
	void UpgradeVehicle(RplId vehicle, int id)
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		
		ResourceName res = economy.GetResource(id);
		
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(vehicle));
		if(!rpl) return;
		IEntity entity = rpl.GetEntity();
		
		string ownerId = GetOwnerID(entity);
		
		vector mat[4];
		entity.GetTransform(mat);
		
		IEntity newveh = SpawnVehicleMatrix(res, mat, ownerId);
		RplComponent newrpl = RplComponent.Cast(newveh.FindComponent(RplComponent));
		
		m_aVehicles.RemoveItem(entity.GetID());
		
		OVT_Global.TransferStorage(vehicle, newrpl.Id());
		SCR_EntityHelper.DeleteEntityAndChildren(entity);		
	}
	
	void RepairVehicle(RplId vehicle)
	{					
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(vehicle));
		if(!rpl) return;
		IEntity entity = rpl.GetEntity();
		
		SCR_VehicleDamageManagerComponent dmg = SCR_VehicleDamageManagerComponent.Cast(entity.FindComponent(SCR_VehicleDamageManagerComponent));
		if(dmg)
		{
			dmg.FullHeal();
			dmg.SetHealthScaled(dmg.GetMaxHealth());
		}
		
		FuelManagerComponent fuel = FuelManagerComponent.Cast(entity.FindComponent(FuelManagerComponent));	
		if(fuel)
		{
			array<BaseFuelNode> nodes();
			fuel.GetFuelNodesList(nodes);
			foreach(BaseFuelNode node : nodes)
			{
				node.SetFuel(node.GetMaxFuel());
			}
		}
	}
	
	void ~OVT_VehicleManagerComponent()
	{
		if(m_aAllVehicleShops)
		{
			m_aAllVehicleShops.Clear();
			m_aAllVehicleShops = null;
		}
		if(m_aEntitySearch)
		{
			m_aEntitySearch.Clear();
			m_aEntitySearch = null;
		}
	}
}