class OVT_Global {
	static OVT_PlayerCommsEntity s_Server;
	
	static OVT_PlayerCommsEntity GetServer()
	{				
		return OVT_OverthrowGameMode.Cast(GetGame().GetGameMode()).m_Server;
	}
	
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
	
	static OVT_ResistanceFactionManager GetResistanceFaction()
	{
		return OVT_ResistanceFactionManager.GetInstance();
	}
	
	static OVT_JobManagerComponent GetJobs()
	{
		return OVT_JobManagerComponent.GetInstance();
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
	
	static int NearestPlayer(vector pos)
	{		
		array<int> players = new array<int>;
		PlayerManager mgr = GetGame().GetPlayerManager();
		int numplayers = mgr.GetPlayers(players);
		
		float nearestDist = 9999999;
		int nearest = -1;
		
		if(numplayers > 0)
		{
			foreach(int playerID : players)
			{
				IEntity player = mgr.GetPlayerControlledEntity(playerID);
				if(!player) continue;
				float distance = vector.Distance(player.GetOrigin(), pos);
				if(distance < nearestDist)
				{
					nearestDist = distance;
					nearest = playerID;
				}
			}
		}
		
		return nearest;
	}
	
	static vector FindSafeSpawnPosition(vector pos, vector mins = "-0.5 0 -0.5", vector maxs = "0.5 2 0.5")
	{
		//a crude and brute-force way to find a spawn position, try to improve this later
		vector foundpos = pos;
		int i = 0;
		
		BaseWorld world = GetGame().GetWorld();
		float ground = world.GetSurfaceY(pos[0],pos[2]);
				
		while(i < 30)
		{
			i++;
			
			//Get a random vector in a 3m radius sphere centered on pos and above the ground
			vector checkpos = s_AIRandomGenerator.GenerateRandomPointInRadius(0,3,pos,false);
			checkpos[1] = pos[1] + s_AIRandomGenerator.RandFloatXY(0, 2);
						
			//check if a box on that position collides with anything
			autoptr TraceBox trace = new TraceBox;
			trace.Flags = TraceFlags.ENTS;
			trace.Start = checkpos;
			trace.Mins = mins;
			trace.Maxs = maxs;
			
			float result = world.TracePosition(trace, null);
				
			if (result < 0)
			{
				//collision, try again
				continue;
			}else{
				//no collision, this pos is safe
				foundpos = checkpos;
				break;
			}
		}
		
		return foundpos;
	}
	
	static void TransferStorage(RplId from, RplId to)
	{
		IEntity fromEntity = RplComponent.Cast(Replication.FindItem(from)).GetEntity();
		IEntity toEntity = RplComponent.Cast(Replication.FindItem(to)).GetEntity();
		
		if(!fromEntity || !toEntity) return;
		
		InventoryStorageManagerComponent fromStorage = InventoryStorageManagerComponent.Cast(fromEntity.FindComponent(InventoryStorageManagerComponent));		
		UniversalInventoryStorageComponent toStorage = UniversalInventoryStorageComponent.Cast(toEntity.FindComponent(UniversalInventoryStorageComponent));
		
		if(!toStorage || !fromStorage) return;
				
		array<IEntity> items = new array<IEntity>;
		fromStorage.GetItems(items);
		if(items.Count() == 0) return;
				
		foreach(IEntity item : items)
		{
			if(!item) continue;
			fromStorage.TryMoveItemToStorage(item, toStorage);				
		}
		
		// Play sound if one is defined
		SimpleSoundComponent simpleSoundComp = SimpleSoundComponent.Cast(toEntity.FindComponent(SimpleSoundComponent));
		if(!simpleSoundComp)
		{
			simpleSoundComp = SimpleSoundComponent.Cast(fromEntity.FindComponent(SimpleSoundComponent));
		}
		if (simpleSoundComp)
		{
			vector mat[4];
			toEntity.GetWorldTransform(mat);
			
			simpleSoundComp.SetTransformation(mat);
			simpleSoundComp.PlayStr("LOAD_VEHICLE");
		}	
	}
	
	static void TransferToWarehouse(RplId from)
	{
		OVT_RealEstateManagerComponent realEstate = GetRealEstate();
		IEntity fromEntity = RplComponent.Cast(Replication.FindItem(from)).GetEntity();
		
		if(!fromEntity) return;
		
		InventoryStorageManagerComponent fromStorage = InventoryStorageManagerComponent.Cast(fromEntity.FindComponent(InventoryStorageManagerComponent));		
				
		if(!fromStorage) return;
		
		OVT_WarehouseData warehouse = realEstate.GetNearestWarehouse(fromEntity.GetOrigin(), 50);
		if(!warehouse) return;
						
		array<IEntity> items = new array<IEntity>;
		fromStorage.GetItems(items);
		if(items.Count() == 0) return;
		
		map<ResourceName,int> collated = new map<ResourceName,int>;
				
		foreach(IEntity item : items)
		{
			if(!item) continue;
			EntityPrefabData data = item.GetPrefabData();
			if(fromStorage.TryDeleteItem(item))
			{				
				if(!data) continue;
				ResourceName res = data.GetPrefabName();
				if(!collated.Contains(res)) collated[res] = 0;
				collated[res] = collated[res] + 1;
			}
		}
		
		for(int i=0; i<collated.Count(); i++)
		{
			ResourceName res = collated.GetKey(i);
			realEstate.AddToWarehouse(warehouse, res, collated[res]);
		}
		
		// Play sound if one is defined
		SimpleSoundComponent simpleSoundComp = SimpleSoundComponent.Cast(fromEntity.FindComponent(SimpleSoundComponent));
		if (simpleSoundComp)
		{
			vector mat[4];
			fromEntity.GetWorldTransform(mat);
			
			simpleSoundComp.SetTransformation(mat);
			simpleSoundComp.PlayStr("LOAD_VEHICLE");
		}	
	}
	
	static void TakeFromWarehouseToVehicle(int warehouseId, int id, int qty, RplId from)
	{
		IEntity fromEntity = RplComponent.Cast(Replication.FindItem(from)).GetEntity();		
		if(!fromEntity) return;
		
		InventoryStorageManagerComponent fromStorage = InventoryStorageManagerComponent.Cast(fromEntity.FindComponent(InventoryStorageManagerComponent));				
		if(!fromStorage) return;
		
		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		
		OVT_WarehouseData warehouse = re.m_aWarehouses[warehouseId];
		ResourceName res = OVT_Global.GetEconomy().GetResource(id);
		
		if(warehouse.inventory[id] < qty) qty = warehouse.inventory[id];
		int actual = 0;
		
		for(int i = 0; i < qty; i++)
		{
			if(fromStorage.TrySpawnPrefabToStorage(res))
			{
				actual++;
			}
		}
		
		re.TakeFromWarehouse(warehouse, res, actual);
	}
	
	//Credit to Arkensor / EveronLife
	//https://github.com/Arkensor/EveronLife/blob/c2adaff17f297ffe1785b173da6fb3874b17b146/src/Scripts/Game/Core/EL_Utils.c#L7
	
	//------------------------------------------------------------------------------------------------
	//! Gets the Bohemia UID
	//! \param playerId Index of the player inside player manager
	//! \return the uid as string
	static string GetPlayerUID(int playerId)
	{
		if (!Replication.IsServer())
		{
			Debug.Error("GetPlayerUID can only be used on the server and after OnPlayerAuditSuccess.");
			return string.Empty;
		}

		string uid = GetGame().GetBackendApi().GetPlayerUID(playerId);
		if (!uid)
		{
			if (RplSession.Mode() == RplMode.Dedicated)
			{
				Debug.Error("Dedicated server is not correctly configured to connect to the BI backend.\nSee https://community.bistudio.com/wiki/Arma_Reforger:Server_Hosting#gameHostRegisterBindAddress");
				return string.Empty;
			}

			uid = string.Format("LOCAL_UID_%1", playerId);
		}

		return uid;
	}

	//------------------------------------------------------------------------------------------------
	//! Gets the Bohemia UID
	//! \param player Instance of the player
	//! \return the uid as string
	static string GetPlayerUID(IEntity player)
	{
		return GetPlayerUID(GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(player));
	}
}