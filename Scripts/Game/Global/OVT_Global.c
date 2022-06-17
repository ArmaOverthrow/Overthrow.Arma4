class OVT_Global {
	static OVT_OverthrowConfigComponent GetConfig()
	{
		return OVT_OverthrowConfigComponent.GetInstance();
	}
	
	static OVT_EconomyManagerComponent GetEconomy()
	{
		return OVT_EconomyManagerComponent.GetInstance();
	}
	
	static OVT_PlayerManagerComponent GetPlayers()
	{
		return OVT_PlayerManagerComponent.GetInstance();
	}
	
	static OVT_RealEstateManagerComponent GetRealEstate()
	{
		return OVT_RealEstateManagerComponent.GetInstance();
	}
	
	static OVT_VehicleManagerComponent GetVehicles()
	{
		return OVT_VehicleManagerComponent.GetInstance();
	}
	
	static OVT_TownManagerComponent GetTowns()
	{
		return OVT_TownManagerComponent.GetInstance();
	}
	
	static OVT_OccupyingFactionManager GetOccupyingFaction()
	{
		return OVT_OccupyingFactionManager.GetInstance();
	}
	
	static bool PlayerInRange(vector pos, int range)
	{		
		bool active = false;
		array<int> players = new array<int>;
		PlayerManager mgr = GetGame().GetPlayerManager();
		int numplayers = mgr.GetPlayers(players);
		
		if(numplayers > 0)
		{
			foreach(int playerID : players)
			{
				IEntity player = mgr.GetPlayerControlledEntity(playerID);
				if(!player) continue;
				float distance = vector.Distance(player.GetOrigin(), pos);
				if(distance < range)
				{
					active = true;
				}
			}
		}
		
		return active;
	}
}