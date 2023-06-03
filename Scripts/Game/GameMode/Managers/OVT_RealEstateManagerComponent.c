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

class OVT_RealEstateManagerComponent: OVT_OwnerManagerComponent
{		
	protected OVT_TownManagerComponent m_Town;
	
	static OVT_RealEstateManagerComponent s_Instance;
	
	protected ref array<IEntity> m_aEntitySearch;
	
	ref array<ref OVT_WarehouseData> m_aWarehouses;
	
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
		m_aEntitySearch = new array<IEntity>;
		m_aWarehouses = new array<ref OVT_WarehouseData>;
	}
	
	override void OnPostInit(IEntity owner)
	{	
		super.OnPostInit(owner);
		
		if (SCR_Global.IsEditMode()) return;		
		
		m_Town = OVT_TownManagerComponent.Cast(GetOwner().FindComponent(OVT_TownManagerComponent));	
	}
	
	override void SetOwner(int playerId, IEntity building)
	{
		super.SetOwner(playerId, building);
		
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
			warehouseData.owner = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);				
			Rpc(RpcDo_SetWarehouseOwner, building.GetOrigin(), playerId);
		}
	}
			
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
	
	void AddToWarehouse(OVT_WarehouseData warehouse, ResourceName res, int count = 1)
	{		
		if(Replication.IsServer())
		{
			DoAddToWarehouse(warehouse.id, res, count);
			return;
		}
		OVT_Global.GetServer().AddToWarehouse(warehouse.id, res, count);
	}
	
	void DoAddToWarehouse(int warehouseId, string id, int count)
	{
		OVT_WarehouseData warehouse = m_aWarehouses[warehouseId];
		if(!warehouse.inventory.Contains(id)) warehouse.inventory[id] = 0;
		warehouse.inventory[id] = warehouse.inventory[id] + count;
		Rpc(RpcDo_SetWarehouseInventory, warehouseId, id, warehouse.inventory[id]);
	}
	
	void TakeFromWarehouse(OVT_WarehouseData warehouse, ResourceName res, int count = 1)
	{		
		if(Replication.IsServer())
		{
			DoTakeFromWarehouse(warehouse.id, res, count);
			return;
		}
		OVT_Global.GetServer().TakeFromWarehouse(warehouse.id, res, count);
	}
	
	void DoTakeFromWarehouse(int warehouseId, string id, int count)
	{
		OVT_WarehouseData warehouse = m_aWarehouses[warehouseId];
		if(!warehouse.inventory.Contains(id)) warehouse.inventory[id] = 0;
		warehouse.inventory[id] = warehouse.inventory[id] - count;
		if(warehouse.inventory[id] < 0) warehouse.inventory[id] = 0;
		Rpc(RpcDo_SetWarehouseInventory, warehouseId, id, warehouse.inventory[id]);
	}
	
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
	
	void SetHomePos(int playerId, vector pos)
	{	
		DoSetHome(playerId, pos);
		Rpc(RpcDo_SetHome, playerId, pos);
	}
	
	bool IsHome(string playerId, EntityID entityId)
	{
		IEntity building = GetGame().GetWorld().FindEntityByID(entityId);
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(playerId);
		float dist = vector.Distance(building.GetOrigin(), player.home);
		return dist < 1;
	}
	
	IEntity GetNearestOwned(string playerId, vector pos)
	{
		if(!m_mOwned.Contains(playerId)) return null;
		
		float nearest = -1;
		IEntity nearestEnt;		
		
		set<RplId> owner = m_mOwned[playerId];
		foreach(RplId id : owner)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			IEntity ent = rpl.GetEntity();
			float dist = vector.Distance(ent.GetOrigin(), pos);
			if(nearest == -1 || dist < nearest)
			{
				nearest = dist;
				nearestEnt = ent;
			}
		}
		
		return nearestEnt;
	}
	
	IEntity GetNearestRented(string playerId, vector pos)
	{
		if(!m_mRented.Contains(playerId)) return null;
		
		float nearest = -1;
		IEntity nearestEnt;		
		
		set<RplId> owner = m_mRented[playerId];
		foreach(RplId id : owner)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			IEntity ent = rpl.GetEntity();
			float dist = vector.Distance(ent.GetOrigin(), pos);
			if(nearest == -1 || dist < nearest)
			{
				nearest = dist;
				nearestEnt = ent;
			}
		}
		
		return nearestEnt;
	}
	
	IEntity GetNearestBuilding(vector pos, float range = 40)
	{
		m_aEntitySearch.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(pos, range, null, FilterBuildingToArray, EQueryEntitiesFlags.STATIC);
		
		if(m_aEntitySearch.Count() == 0)
		{
			return null;
		}
		float nearest = range;
		IEntity nearestEnt;	
		
		foreach(IEntity ent : m_aEntitySearch)
		{
			float dist = vector.Distance(ent.GetOrigin(), pos);
			if(dist < nearest)
			{
				nearest = dist;
				nearestEnt = ent;
			}
		}
		return nearestEnt;
	}
	
	bool BuildingIsOwnable(IEntity entity)
	{
		if(!entity) return false;
		if(entity.ClassName() != "SCR_DestructibleBuildingEntity")
		{
			return false;
		}
		
		ResourceName res = entity.GetPrefabData().GetPrefabName();
		foreach(OVT_RealEstateConfig config : m_Config.m_aBuildingTypes)
		{
			foreach(ResourceName s : config.m_aResourceNameFilters)
			{
				if(res.IndexOf(s) > -1) return true;
			}
		}
		return false;
	}
	
	OVT_RealEstateConfig GetConfig(IEntity entity)
	{
		if(!entity) return null;
		if(entity.ClassName() != "SCR_DestructibleBuildingEntity")
		{
			return null;
		}
		
		ResourceName res = entity.GetPrefabData().GetPrefabName();
		foreach(OVT_RealEstateConfig config : m_Config.m_aBuildingTypes)
		{
			foreach(ResourceName s : config.m_aResourceNameFilters)
			{
				if(res.IndexOf(s) > -1) return config;
			}
		}
		return null;
	}
	
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
	
	bool FilterBuildingToArray(IEntity entity)
	{
		if(entity.ClassName() == "SCR_DestructibleBuildingEntity")
		{
			m_aEntitySearch.Insert(entity);
		}
		return false;
	}
	
	vector GetHome(string playerId)
	{				
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(playerId);
		if(!player) return "0 0 0";
				
		return player.home;
	}
	
	void TeleportHome(int playerId)
	{
		RpcDo_TeleportHome(playerId);
		Rpc(RpcDo_TeleportHome, playerId);
	}
	
	//RPC Methods
	
	override bool RplSave(ScriptBitWriter writer)
	{
		super.RplSave(writer);
		
		//Send JIP warehouses
		writer.WriteInt(m_aWarehouses.Count());
		for(int i=0; i<m_aWarehouses.Count(); i++)
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

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetHome(int playerId, vector loc)
	{
		DoSetHome(playerId, loc);	
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetWarehouseInventory(int warehouseId, string id, int qty)
	{
		m_aWarehouses[warehouseId].inventory[id] = qty;
	}
	
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
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_TeleportHome(int playerId)
	{
		int localId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(SCR_PlayerController.GetLocalControlledEntity());
		if(playerId != localId) return;
		
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		vector spawn = OVT_Global.FindSafeSpawnPosition(OVT_Global.GetRealEstate().GetHome(persId));
		SCR_Global.TeleportPlayer(spawn);
	}
	
	void DoSetHome(int playerId, vector loc)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(persId);
		if(!player) return;
		player.home = loc;
	}
	
	void ~OVT_RealEstateManagerComponent()
	{		
		if(m_aEntitySearch)
		{
			m_aEntitySearch.Clear();
			m_aEntitySearch = null;
		}
	}
}