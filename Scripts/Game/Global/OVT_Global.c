class OVT_Global {
	static OVT_OverthrowConfigComponent GetConfig()
	{
		return OVT_Global.GetConfig();
	}
	
	static OVT_EconomyManagerComponent GetEconomy()
	{
		return OVT_Global.GetEconomy();
	}
	
	static OVT_PlayerManagerComponent GetPlayers()
	{
		return OVT_PlayerManagerComponent.GetInstance();
	}
	
	static OVT_RealEstateManagerComponent GetRealEstate()
	{
		return OVT_Global.GetRealEstate();
	}
	
	static OVT_VehicleManagerComponent GetVehicles()
	{
		return OVT_Global.GetVehicles();
	}
	
	static OVT_TownManagerComponent GetTowns()
	{
		return OVT_Global.GetTowns();
	}
	
	static OVT_OccupyingFactionManager GetOccupyingFaction()
	{
		return OVT_Global.GetOccupyingFaction();
	}
}