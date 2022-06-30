class OVT_BaseUpgradeParkedVehicles : OVT_BaseUpgrade
{
	[Attribute()]
	int m_iNumberOfCars;
	
	[Attribute()]
	int m_iNumberOfTrucks;
	
	ref array<ref EntityID> m_Cars;
	ref array<ref EntityID> m_Trucks;
	
	OVT_VehicleManagerComponent m_Vehicles;
			
	override void PostInit()
	{
		m_Cars = new array<ref EntityID>;	
		m_Trucks = 	new array<ref EntityID>;
		
		m_Vehicles = OVT_Global.GetVehicles();	
	}
	
	override int GetResources()
	{
		int res = 0;
		foreach(EntityID id : m_Cars)
		{
			res += m_Config.m_Difficulty.baseResourceCost * 3;	
		}
		foreach(EntityID id : m_Trucks)
		{
			res += m_Config.m_Difficulty.baseResourceCost * 6;	
		}
		return res;
	}
	
	override int Spend(int resources, float threat)
	{
		int spent = 0;
		
		if(m_Cars.Count() < m_iNumberOfCars)
		{
			//Add just 1 car
			int newres = BuyCar();			
			if(newres > resources){
				newres = resources;
			}			
			spent += newres;
			resources -= newres;
		}
		
		if(m_Trucks.Count() < m_iNumberOfTrucks)
		{
			//Add just 1 truck
			int newres = BuyTruck();			
			if(newres > resources){
				newres = resources;
			}			
			spent += newres;
			resources -= newres;
		}
		
		
		return spent;
	}
	
	protected int BuyCar()
	{
		int spent = 0;
		//Try to find a parking spot
		if(m_BaseController.m_Parking.Count() == 0) return 0;
		
		OVT_Faction faction = m_Config.GetOccupyingFaction();
		if(faction.m_aVehicleCarPrefabSlots.Count() == 0) return 0;
		
		EntityID parkingBuildingID = m_BaseController.m_Parking.GetRandomElement();
		IEntity parkingBuilding = GetGame().GetWorld().FindEntityByID(parkingBuildingID);
		
		vector spot[4];
		if(m_Vehicles.GetParkingSpot(parkingBuilding, spot))
		{
			IEntity veh = m_Vehicles.SpawnVehicleMatrix(faction.m_aVehicleCarPrefabSlots.GetRandomElement(), spot);
			if(veh){
				spent += m_Config.m_Difficulty.baseResourceCost * 3;
				m_Cars.Insert(veh.GetID());
			}			
		}
		return spent;
	}
	
	protected int BuyTruck()
	{
		int spent = 0;
		//Try to find a parking spot
		if(m_BaseController.m_Parking.Count() == 0) return 0;
		
		OVT_Faction faction = m_Config.GetOccupyingFaction();
		if(faction.m_aVehicleTruckPrefabSlots.Count() == 0) return 0;
		
		EntityID parkingBuildingID = m_BaseController.m_Parking.GetRandomElement();
		IEntity parkingBuilding = GetGame().GetWorld().FindEntityByID(parkingBuildingID);
		
		vector spot[4];
		if(m_Vehicles.GetParkingSpot(parkingBuilding, spot, OVT_ParkingType.PARKING_TRUCK))
		{
			IEntity veh = m_Vehicles.SpawnVehicleMatrix(faction.m_aVehicleTruckPrefabSlots.GetRandomElement(), spot);
			if(veh){
				spent += m_Config.m_Difficulty.baseResourceCost * 6;
				m_Trucks.Insert(veh.GetID());
			}			
		}
		return spent;
	}
	
	override OVT_BaseUpgradeStruct Serialize()
	{
		OVT_BaseUpgradeStruct struct = super.Serialize();
		
		struct.m_iResources = 0; //Do not respend any resources
		
		foreach(EntityID id : m_Cars)
		{
			IEntity ent = GetGame().GetWorld().FindEntityByID(id);
			if(ent)
			{
				OVT_VehicleStruct veh = new OVT_VehicleStruct();
				if(veh.Parse(ent))
				{
					struct.m_aVehicles.Insert(veh);
				}
			}			
		}
		
		foreach(EntityID id : m_Trucks)
		{
			IEntity ent = GetGame().GetWorld().FindEntityByID(id);
			if(ent)
			{
				OVT_VehicleStruct veh = new OVT_VehicleStruct();
				if(veh.Parse(ent))
				{
					struct.m_aVehicles.Insert(veh);
				}
			}			
		}
		
		return struct;
	}
	
	override bool Deserialize(OVT_BaseUpgradeStruct struct)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		OVT_Faction faction = config.GetOccupyingFaction();
		
		foreach(OVT_VehicleStruct veh : struct.m_aVehicles)
		{
			IEntity ent = veh.Spawn();
			if(faction.m_aVehicleCarPrefabSlots.Find(veh.m_sResource) > -1)
			{
				m_Cars.Insert(ent.GetID());
			}else{
				m_Trucks.Insert(ent.GetID());
			}
		}
		
		return true;
	}
}