class OVT_RealEstateManagerComponentClass: OVT_OwnerManagerComponentClass
{
};

class OVT_WarehouseData : Managed
{
	int id;
	vector location;
	string owner;
	bool isPrivate;
	bool isLinked;
	
	ref map<string,int> inventory;	
}

//------------------------------------------------------------------------------------------------
//! Manages real estate ownership, renting, warehouses, and starting homes within the game mode.
//! Provides functionality for players to buy, rent, and manage properties, including setting home spawn points
//! and utilizing warehouse storage.
class OVT_RealEstateManagerComponent: OVT_OwnerManagerComponent
{		
	protected OVT_TownManagerComponent m_Town;
	
	static OVT_RealEstateManagerComponent s_Instance;
	
	protected ref array<EntityID> m_aStartingHomes;
	protected ref array<EntityID> m_aTownStartingHomes;
	int m_iStartingTownId = -1;
	
	ref array<ref OVT_WarehouseData> m_aWarehouses;
	
	//------------------------------------------------------------------------------------------------
	//! Returns the singleton instance of the OVT_RealEstateManagerComponent
	//! \return The singleton instance
	static OVT_RealEstateManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_RealEstateManagerComponent.Cast(pGameMode.FindComponent(OVT_RealEstateManagerComponent));
		}

		return s_Instance;
	}
	
	void OVT_RealEstateManagerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{		
		m_aWarehouses = new array<ref OVT_WarehouseData>;
		m_aEntitySearch = new array<IEntity>;		
		m_aStartingHomes = new array<EntityID>;
		m_aTownStartingHomes = new array<EntityID>;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Called after the entity loading process. Finds potential starting homes across the map.
	//! \param[in] owner The entity this component is attached to
	void OnPostLoad(IEntity owner)	
	{
		#ifdef OVERTHROW_DEBUG
		Print("Finding starting homes");
		#endif

		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, null, FindStartingHomeEntities, EQueryEntitiesFlags.STATIC);
		
		Print("Found " + m_aStartingHomes.Count() + " Starting Homes");
	}
	
	//------------------------------------------------------------------------------------------------
	//! Callback function used by QueryEntitiesBySphere to identify potential starting home buildings.
	//! Checks if the building is a destructible building, not furniture, matches starting house filters, and is not already owned.
	//! \param[in] entity The entity to check
	//! \return true if the entity is added to the starting homes list, false otherwise
	bool FindStartingHomeEntities(IEntity entity)
	{
		if(entity.ClassName() == "SCR_DestructibleBuildingEntity"){
			ResourceName res = entity.GetPrefabData().GetPrefabName();
			if(res.IndexOf("_furniture") > -1) return false;	
			OVT_TownManagerComponent towns = OVT_Global.GetTowns();		
			foreach(string s : OVT_Global.GetConfig().m_aStartingHouseFilters)
			{
				if(res.IndexOf(s) > -1) {
					EntityID id = entity.GetID();
					OVT_TownData closestTown = towns.GetNearestTown(entity.GetOrigin());
					if(towns.m_aIgnoreTowns.Find(towns.GetTownName(towns.GetTownID(closestTown))) > -1) return false;
					if(!IsOwned(id))
						m_aStartingHomes.Insert(id);
					continue;
				}
			}			
		}
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Called after the entity initialization process. Gets a reference to the town manager component.
	//! \param[in] owner The entity this component is attached to
	override void OnPostInit(IEntity owner)
	{	
		super.OnPostInit(owner);
		
		if (SCR_Global.IsEditMode()) return;		
		
		m_Town = OVT_TownManagerComponent.Cast(GetOwner().FindComponent(OVT_TownManagerComponent));	
	}
	
	//------------------------------------------------------------------------------------------------
	//! Selects a new town to be the source of starting homes and populates the town-specific starting homes list.
	//! Attempts to find a town with suitable starting homes that hasn't been used recently.
	void NewStartingTown()
	{
		int attempts = 0;
		while(attempts < 50)
		{
			attempts++;
			OVT_TownData town = m_Town.GetRandomTown();
			int townId = m_Town.GetTownID(town);
			if(town && townId != m_iStartingTownId)
			{
				m_iStartingTownId = townId;
				m_aTownStartingHomes.Clear();
				foreach(EntityID id : m_aStartingHomes)
				{
					IEntity ent = GetGame().GetWorld().FindEntityByID(id);
					OVT_TownData nearestTown = m_Town.GetNearestTown(ent.GetOrigin());
					int nearestId = m_Town.GetTownID(nearestTown);
					if(nearestId == m_iStartingTownId)
					{
						m_aTownStartingHomes.Insert(id);
					}
				}
				if(m_aTownStartingHomes.Count() > 0) 
				{
					Print("New Starting Home Town: " + m_Town.GetTownName(m_iStartingTownId));
					return;
				}
			}
		}
		//Cannot find a new starting town
		m_iStartingTownId = -1;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets a random, unowned starting house from the currently selected starting town.
	//! If no starting town is selected or the current town runs out of houses, it calls NewStartingTown().
	//! Removes the selected house from the available pool.
	//! \return A random starting house entity, or null if none are available.
	IEntity GetRandomStartingHouse()
	{
		int numHouses = m_aStartingHomes.Count();
		if(numHouses == 0) return null;
		
		if(m_iStartingTownId == -1 || m_aTownStartingHomes.Count() == 0)
		{
			NewStartingTown();
		}

		if(m_iStartingTownId == -1) return null;
				
		int i = s_AIRandomGenerator.RandInt(0, m_aTownStartingHomes.Count() - 1);
				
		EntityID id = m_aTownStartingHomes[i];
		m_aTownStartingHomes.Remove(i);
		int index = m_aStartingHomes.Find(id);
		if(index > -1) m_aStartingHomes.Remove(index);

		return GetGame().GetWorld().FindEntityByID(id);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Sets the owner of a building. If the building is a warehouse, creates or updates warehouse data.
	//! \param[in] playerId The ID of the player to set as owner
	//! \param[in] building The building entity to assign ownership to
	override void SetOwner(int playerId, IEntity building)
	{
		super.SetOwner(playerId, building);
		
		OVT_RealEstateConfig config = GetConfig(building);
		if(!config) return;
		
		if(config.m_IsWarehouse)
		{
			bool hasData = false;
			OVT_WarehouseData warehouseData;
			if(!m_aWarehouses)
			{
				m_aWarehouses = new array<ref OVT_WarehouseData>();
			}
			foreach(OVT_WarehouseData warehouse : m_aWarehouses)
			{
				if(vector.Distance(warehouse.location, building.GetOrigin()) < 10)
				{
					hasData = true;
					warehouseData = warehouse;
					break;
				}
			}
			if(!hasData)
			{
				warehouseData = new OVT_WarehouseData;
				warehouseData.location = building.GetOrigin();
				warehouseData.inventory = new map<string,int>;
				warehouseData.id = m_aWarehouses.Count();
				m_aWarehouses.Insert(warehouseData);
				
			}
			warehouseData.owner = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);				
			Rpc(RpcDo_SetWarehouseOwner, building.GetOrigin(), playerId);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Sets the owner of a building using a persistent ID. If the building is a warehouse, creates or updates warehouse data.
	//! \param[in] persId The persistent ID of the player to set as owner
	//! \param[in] building The building entity to assign ownership to
	override void SetOwnerPersistentId(string persId, IEntity building)
	{
		super.SetOwnerPersistentId(persId, building);
		
		OVT_RealEstateConfig config = GetConfig(building);
		if(!config) return;
		
		if(config.m_IsWarehouse)
		{
			bool hasData = false;
			OVT_WarehouseData warehouseData;
			foreach(OVT_WarehouseData warehouse : m_aWarehouses)
			{
				if(vector.Distance(warehouse.location, building.GetOrigin()) < 10)
				{
					hasData = true;
					warehouseData = warehouse;
					break;
				}
			}
			if(!hasData)
			{
				warehouseData = new OVT_WarehouseData;
				warehouseData.location = building.GetOrigin();
				warehouseData.inventory = new map<string,int>;
				warehouseData.id = m_aWarehouses.Count();
				m_aWarehouses.Insert(warehouseData);
				
			}
			warehouseData.owner = persId;
			Rpc(RpcDo_SetWarehouseOwnerPersistent, building.GetOrigin(), persId);
		}
	}
			
	//------------------------------------------------------------------------------------------------
	//! Finds the nearest warehouse to a given position within an optional range.
	//! \param[in] pos The position to search near
	//! \param[in] range Optional maximum distance to search (default: 9999999)
	//! \return The OVT_WarehouseData of the nearest warehouse, or null if none found within range
	OVT_WarehouseData GetNearestWarehouse(vector pos, int range=9999999)
	{
		OVT_WarehouseData nearestWarehouse;
		float nearest = range;
		foreach(OVT_WarehouseData warehouse : m_aWarehouses)
		{
			float distance = vector.Distance(warehouse.location, pos);
			if(distance < nearest){
				nearest = distance;
				nearestWarehouse = warehouse;
			}
		}
		return nearestWarehouse;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the inventory of a specific warehouse. If the warehouse is linked, it aggregates inventory from all linked warehouses.
	//! \param[in] warehouse The warehouse data object
	//! \return A map representing the inventory (resource name string -> quantity int)
	map<string,int> GetWarehouseInventory(OVT_WarehouseData warehouse)
	{
		if(warehouse.isLinked)
		{
			map<string,int> items = new map<string,int>;
			foreach(OVT_WarehouseData w : m_aWarehouses)
			{
				if(!w.isLinked) continue;
				for(int i=0; i<w.inventory.Count(); i++)
				{
					string id = w.inventory.GetKey(i);
					if(!items.Contains(id)) items[id] = 0;
					items[id] = items[id] + w.inventory[id];
				}
			}
			return items;
		}else{
			return warehouse.inventory;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Adds items to a warehouse. Handles server/client logic.
	//! \param[in] warehouse The warehouse data object
	//! \param[in] res The ResourceName of the item to add
	//! \param[in] count The quantity to add (default: 1)
	void AddToWarehouse(OVT_WarehouseData warehouse, ResourceName res, int count = 1)
	{		
		if(Replication.IsServer())
		{
			DoAddToWarehouse(warehouse.id, res, count);
			return;
		}
		OVT_Global.GetServer().AddToWarehouse(warehouse.id, res, count);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Server-side logic to add items to a warehouse's inventory and replicate the change.
	//! \param[in] warehouseId The ID of the warehouse
	//! \param[in] id The ResourceName string of the item to add
	//! \param[in] count The quantity to add
	void DoAddToWarehouse(int warehouseId, string id, int count)
	{
		OVT_WarehouseData warehouse = m_aWarehouses[warehouseId];
		if(!warehouse.inventory.Contains(id)) warehouse.inventory[id] = 0;
		warehouse.inventory[id] = warehouse.inventory[id] + count;
		Rpc(RpcDo_SetWarehouseInventory, warehouseId, id, warehouse.inventory[id]);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Takes items from a warehouse. Handles server/client logic.
	//! \param[in] warehouse The warehouse data object
	//! \param[in] res The ResourceName of the item to take
	//! \param[in] count The quantity to take (default: 1)
	void TakeFromWarehouse(OVT_WarehouseData warehouse, ResourceName res, int count = 1)
	{		
		if(Replication.IsServer())
		{
			DoTakeFromWarehouse(warehouse.id, res, count);
			return;
		}
		OVT_Global.GetServer().TakeFromWarehouse(warehouse.id, res, count);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Server-side logic to remove items from a warehouse's inventory and replicate the change. Ensures quantity doesn't go below zero.
	//! \param[in] warehouseId The ID of the warehouse
	//! \param[in] id The ResourceName string of the item to take
	//! \param[in] count The quantity to take
	void DoTakeFromWarehouse(int warehouseId, string id, int count)
	{
		OVT_WarehouseData warehouse = m_aWarehouses[warehouseId];
		if(!warehouse.inventory.Contains(id)) warehouse.inventory[id] = 0;
		warehouse.inventory[id] = warehouse.inventory[id] - count;
		if(warehouse.inventory[id] < 0) warehouse.inventory[id] = 0;
		Rpc(RpcDo_SetWarehouseInventory, warehouseId, id, warehouse.inventory[id]);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Sets the home spawn location for a player based on a building entity. Uses OVT_SpawnPointComponent if available.
	//! Replicates the change to clients.
	//! \param[in] playerId The ID of the player
	//! \param[in] building The building entity to set as home
	void SetHome(int playerId, IEntity building)
	{	
		OVT_SpawnPointComponent spawn = OVT_SpawnPointComponent.Cast(building.FindComponent(OVT_SpawnPointComponent));
		vector pos = building.GetOrigin();
		if(spawn)
		{
			pos = spawn.GetSpawnPoint();
		}
		DoSetHome(playerId, pos);
		Rpc(RpcDo_SetHome, playerId, pos);		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Sets the home spawn location for a player directly using a position vector.
	//! Replicates the change to clients.
	//! \param[in] playerId The ID of the player
	//! \param[in] pos The position vector for the home spawn
	void SetHomePos(int playerId, vector pos)
	{	
		DoSetHome(playerId, pos);
		Rpc(RpcDo_SetHome, playerId, pos);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Checks if a given building entity is the registered home for a player.
	//! \param[in] playerId The persistent ID of the player
	//! \param[in] entityId The EntityID of the building to check
	//! \return true if the building's origin is very close to the player's home position, false otherwise
	bool IsHome(string playerId, EntityID entityId)
	{
		IEntity building = GetGame().GetWorld().FindEntityByID(entityId);
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(playerId);
		if(!player) return false;
		float dist = vector.Distance(building.GetOrigin(), player.home);
		return dist < 1;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Finds the nearest building owned by a specific player to a given position, within an optional range.
	//! \param[in] playerId The persistent ID of the player
	//! \param[in] pos The position to search near
	//! \param[in] range Optional maximum distance to search (default: -1, no limit)
	//! \return The nearest owned building entity, or null if none found
	IEntity GetNearestOwned(string playerId, vector pos, float range = -1)
	{
		if(!m_mOwned.Contains(playerId)) return null;
		
		float nearest = -1;
		vector nearestPos;		
		
		array<string> owner = m_mOwned[playerId];
		foreach(string buildingPosString : owner)
		{			
			vector buildingPos = buildingPosString.ToVector();
			float dist = vector.Distance(buildingPos, pos);
			if(range > -1 && dist > range) continue;
			if(nearest == -1 || dist < nearest)
			{
				nearest = dist;
				nearestPos = buildingPos;
			}
		}
		
		return GetNearestBuilding(nearestPos);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Finds the nearest building rented by a specific player to a given position, within an optional range.
	//! \param[in] playerId The persistent ID of the player
	//! \param[in] pos The position to search near
	//! \param[in] range Optional maximum distance to search (default: -1, no limit)
	//! \return The nearest rented building entity, or null if none found
	IEntity GetNearestRented(string playerId, vector pos, float range = -1)
	{
		if(!m_mRented.Contains(playerId)) return null;
		
		float nearest = -1;
		vector nearestPos;		
		
		array<string> owner = m_mRented[playerId];
		foreach(string buildingPosString : owner)
		{
			vector buildingPos = buildingPosString.ToVector();
			float dist = vector.Distance(buildingPos, pos);
			if(range > -1 && dist > range) continue;
			if(nearest == -1 || dist < nearest)
			{
				nearest = dist;
				nearestPos = buildingPos;
			}
		}
		
		return GetNearestBuilding(nearestPos);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Checks if a building entity is of a type that can be owned according to the configuration.
	//! \param[in] entity The building entity to check
	//! \return true if the building is ownable, false otherwise
	bool BuildingIsOwnable(IEntity entity)
	{
		if(!entity) return false;
		if(entity.ClassName() != "SCR_DestructibleBuildingEntity")
		{
			return false;
		}

		ResourceName res = entity.GetPrefabData().GetPrefabName();
		foreach(OVT_RealEstateConfig config : OVT_Global.GetConfig().m_aBuildingTypes)
		{
			foreach(ResourceName s : config.m_aResourceNameFilters)
			{
				if(res.IndexOf(s) > -1) return true;
			}
		}
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the real estate configuration associated with a building entity based on its prefab resource name.
	//! \param[in] entity The building entity
	//! \return The matching OVT_RealEstateConfig, or null if none found or not an ownable building type
	OVT_RealEstateConfig GetConfig(IEntity entity)
	{
		if(!entity) return null;
		if(entity.ClassName() != "SCR_DestructibleBuildingEntity")
		{
			return null;
		}
		
		ResourceName res = entity.GetPrefabData().GetPrefabName();
		foreach(OVT_RealEstateConfig config : OVT_Global.GetConfig().m_aBuildingTypes)
		{
			foreach(ResourceName s : config.m_aResourceNameFilters)
			{
				if(res.IndexOf(s) > -1) return config;
			}
		}
		return null;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Calculates the purchase price for a building based on its config and the nearest town's economy.
	//! \param[in] entity The building entity
	//! \return The calculated purchase price, or 0 if no config found
	int GetBuyPrice(IEntity entity)
	{
		OVT_RealEstateConfig config = GetConfig(entity);
		if(!config) return 0;
		
		OVT_TownData town = m_Town.GetNearestTown(entity.GetOrigin());
		
		if(config.m_IsWarehouse)
		{
			return config.m_BasePrice;
		}
		
		return config.m_BasePrice + (config.m_BasePrice * (config.m_DemandMultiplier * town.population * ((float)town.stability / 100)));
	}
	
	//------------------------------------------------------------------------------------------------
	//! Calculates the rental price for a building based on its config and the nearest town's economy.
	//! \param[in] entity The building entity
	//! \return The calculated rental price, or 0 if no config found
	int GetRentPrice(IEntity entity)
	{
		OVT_RealEstateConfig config = GetConfig(entity);
		if(!config) return 0;
		
		OVT_TownData town = m_Town.GetNearestTown(entity.GetOrigin());
		
		if(config.m_IsWarehouse)
		{
			return config.m_BaseRent;
		}
		
		return config.m_BaseRent + (config.m_BaseRent * (config.m_DemandMultiplier * town.population * ((float)town.stability / 100)));
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the home spawn position vector for a given player.
	//! \param[in] playerId The persistent ID of the player
	//! \return The player's home position vector, or "0 0 0" if player data not found
	vector GetHome(string playerId)
	{				
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(playerId);
		if(!player) return "0 0 0";
				
		return player.home;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Initiates teleporting a player to their registered home spawn location.
	//! \param[in] playerId The ID of the player to teleport
	void TeleportHome(int playerId)
	{
		RpcDo_TeleportHome(playerId);
		Rpc(RpcDo_TeleportHome, playerId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Saves replication data for the component, including warehouse information.
	//! \param[in,out] writer The ScriptBitWriter to write data to
	//! \return true on success
	override bool RplSave(ScriptBitWriter writer)
	{
		super.RplSave(writer);
		
		//Send JIP warehouses
		int count = 0;
		if(m_aWarehouses) count = m_aWarehouses.Count();
		writer.WriteInt(count);
		for(int i=0; i<count; i++)
		{
			OVT_WarehouseData data = m_aWarehouses[i];
			writer.WriteInt(data.id);
			writer.WriteVector(data.location);
			writer.WriteString(data.owner);
			writer.WriteBool(data.isLinked);
			writer.WriteBool(data.isPrivate);
			writer.WriteInt(data.inventory.Count());
			for(int ii; ii<m_aWarehouses.Count(); ii++)
			{
				writer.WriteString(data.inventory.GetKey(ii));
				writer.WriteInt(data.inventory.GetElement(ii));
			}
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Loads replication data for the component, including warehouse information for JIP clients.
	//! \param[in] reader The ScriptBitReader to read data from
	//! \return true on success, false on failure
	override bool RplLoad(ScriptBitReader reader)
	{
		if(!super.RplLoad(reader)) return false;
		
		int length, ownedlength, id, qty;
		string res;
		string playerId;
		vector loc;
		
		//Recieve JIP warehouses
		if (!reader.ReadInt(length)) return false;
		for(int i=0; i<length; i++)
		{
			OVT_WarehouseData data = new OVT_WarehouseData;
			if (!reader.ReadInt(data.id)) return false;
			if (!reader.ReadVector(data.location)) return false;	
			if (!reader.ReadString(data.owner)) return false;
			if (!reader.ReadBool(data.isLinked)) return false;
			if (!reader.ReadBool(data.isPrivate)) return false;
			
			data.inventory = new map<string,int>;
			
			if (!reader.ReadInt(ownedlength)) return false;
			for(int ii; ii<length; ii++)
			{
				if (!reader.ReadString(res)) return false;
				if (!reader.ReadInt(qty)) return false;
				data.inventory[res] = qty;
			}
			m_aWarehouses.Insert(data);			
		}
		return true;
	}	

	//------------------------------------------------------------------------------------------------
	//! RPC handler to set the home location for a player. Called on all clients.
	//! \param[in] playerId The ID of the player
	//! \param[in] loc The new home location vector
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetHome(int playerId, vector loc)
	{
		DoSetHome(playerId, loc);	
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC handler to update the quantity of a specific item in a specific warehouse's inventory. Called on all clients.
	//! \param[in] warehouseId The ID of the warehouse
	//! \param[in] id The ResourceName string of the item
	//! \param[in] qty The new quantity of the item
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetWarehouseInventory(int warehouseId, string id, int qty)
	{
		m_aWarehouses[warehouseId].inventory[id] = qty;
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC handler to set the owner of a warehouse based on its location. Creates warehouse data if it doesn't exist. Called on all clients.
	//! \param[in] location The location vector of the warehouse
	//! \param[in] playerId The ID of the new owner player
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetWarehouseOwner(vector location, int playerId)
	{
		bool hasData = false;
		OVT_WarehouseData warehouseData;
		foreach(OVT_WarehouseData warehouse : m_aWarehouses)
		{
			if(vector.Distance(warehouse.location, location) < 10)
			{
				hasData = true;
				warehouseData = warehouse;
				break;
			}
		}
		if(!hasData)
		{
			warehouseData = new OVT_WarehouseData;
			warehouseData.location = location;
			warehouseData.inventory = new map<string,int>;
			warehouseData.id = m_aWarehouses.Count();
			m_aWarehouses.Insert(warehouseData);
			
		}
		warehouseData.owner = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC handler to set the owner of a warehouse based on its location using a persistent ID. Creates warehouse data if it doesn't exist. Called on all clients.
	//! \param[in] location The location vector of the warehouse
	//! \param[in] playerId The persistent ID of the new owner player
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetWarehouseOwnerPersistent(vector location, string playerId)
	{
		bool hasData = false;
		OVT_WarehouseData warehouseData;
		foreach(OVT_WarehouseData warehouse : m_aWarehouses)
		{
			if(vector.Distance(warehouse.location, location) < 10)
			{
				hasData = true;
				warehouseData = warehouse;
				break;
			}
		}
		if(!hasData)
		{
			warehouseData = new OVT_WarehouseData;
			warehouseData.location = location;
			warehouseData.inventory = new map<string,int>;
			warehouseData.id = m_aWarehouses.Count();
			m_aWarehouses.Insert(warehouseData);
			
		}
		warehouseData.owner = playerId;
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC handler to teleport a player to their home. Only executes on the target player's client.
	//! \param[in] playerId The ID of the player to teleport
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_TeleportHome(int playerId)
	{
		int localId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(SCR_PlayerController.GetLocalControlledEntity());
		if(playerId != localId) return;
		
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		vector spawn = OVT_Global.FindSafeSpawnPosition(OVT_Global.GetRealEstate().GetHome(persId));
		SCR_Global.TeleportPlayer(localId, spawn);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Internal logic to set the home location in the player's data.
	//! \param[in] playerId The ID of the player
	//! \param[in] loc The new home location vector
	void DoSetHome(int playerId, vector loc)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(persId);
		if(!player) return;
		player.home = loc;
	}
	
}