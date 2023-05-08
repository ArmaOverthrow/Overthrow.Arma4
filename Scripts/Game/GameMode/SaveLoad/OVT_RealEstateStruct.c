[BaseContainerProps()]
class OVT_RealEstateStruct : OVT_BaseSaveStruct
{
	protected ref array<ref OVT_RealEstatePlayerStruct> players = {};
	protected ref array<ref OVT_WarehouseStruct> warehouses = {};
		
	override bool Serialize()
	{
		players.Clear();
		
		map<string, OVT_RealEstatePlayerStruct> playerStructs = new map<string, OVT_RealEstatePlayerStruct>;
		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		OVT_EconomyManagerComponent ec = OVT_Global.GetEconomy();
		
		for(int i; i<re.m_mHomes.Count(); i++)
		{	
			OVT_RealEstatePlayerStruct struct = new OVT_RealEstatePlayerStruct();
			
			struct.id = re.m_mHomes.GetKey(i);
			struct.home = re.m_mHomes.GetElement(i);
			
			playerStructs[struct.id] = struct;
			players.Insert(struct);
		}
		
		for(int i; i<re.m_mOwned.Count(); i++)
		{
			string playerId = re.m_mOwned.GetKey(i);
			OVT_RealEstatePlayerStruct struct;
			
			if(!playerStructs.Contains(playerId))
			{
				struct = new OVT_RealEstatePlayerStruct();
				struct.id = playerId;
				players.Insert(struct);
			}else{
				struct = playerStructs[playerId];
			}
			
			foreach(RplId id : re.m_mOwned[playerId])
			{
				RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
				IEntity ent = rpl.GetEntity();
				OVT_VectorStruct vec = new OVT_VectorStruct();
				vec.pos = ent.GetOrigin();
				struct.ownedProperty.Insert(vec);
			}
		}
		
		for(int i; i<re.m_mRented.Count(); i++)
		{
			string playerId = re.m_mRented.GetKey(i);
			OVT_RealEstatePlayerStruct struct;
			
			if(!playerStructs.Contains(playerId))
			{
				struct = new OVT_RealEstatePlayerStruct();
				struct.id = playerId;
				players.Insert(struct);
			}else{
				struct = playerStructs[playerId];
			}
			
			foreach(RplId id : re.m_mOwned[playerId])
			{
				RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
				IEntity ent = rpl.GetEntity();
				OVT_VectorStruct vec = new OVT_VectorStruct();
				vec.pos = ent.GetOrigin();
				struct.rentedProperty.Insert(vec);
			}
		}
		
		foreach(OVT_WarehouseData warehouse : re.m_aWarehouses)		
		{
			OVT_WarehouseStruct struct = new OVT_WarehouseStruct;
			struct.id = warehouse.id;
			struct.location = warehouse.location;
			struct.owner = warehouse.owner;
			struct.isPrivate = warehouse.isPrivate;
			struct.isLinked = warehouse.isLinked;
			struct.inventory = new array<ref OVT_InventoryStruct>;
			for(int i=0; i<warehouse.inventory.Count(); i++)
			{
				int qty = warehouse.inventory.GetElement(i);
				if(qty < 1) continue;
				
				int resourceId = warehouse.inventory.GetKey(i);
				ResourceName resource = ec.GetResource(resourceId);
				int res = rdb.Find(resource);
				if(res == -1)
				{
					rdb.Insert(resource);
					res = rdb.Count() - 1;
				}
				OVT_InventoryStruct inv = new OVT_InventoryStruct;
				inv.id = res;
				inv.qty = qty;
				struct.inventory.Insert(inv);
			}
			warehouses.Insert(struct);
		}
		
		return true;
	}
	
	override bool Deserialize()
	{
		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		OVT_EconomyManagerComponent ec = OVT_Global.GetEconomy();
		
		foreach(OVT_RealEstatePlayerStruct struct : players)
		{
			re.m_mHomes[struct.id] = struct.home;
									
			re.m_mOwned[struct.id] = new set<RplId>;
			
			foreach(OVT_VectorStruct loc : struct.ownedProperty)
			{
				vector vec = loc.pos;
				IEntity ent = re.GetNearestBuilding(vec,2);
				if(ent)
				{
					RplComponent rpl = RplComponent.Cast(ent.FindComponent(RplComponent));
					re.m_mOwned[struct.id].Insert(rpl.Id());
				}
			}
			foreach(OVT_VectorStruct loc : struct.rentedProperty)
			{
				vector vec = loc.pos;
				IEntity ent = re.GetNearestBuilding(vec,2);
				if(ent)
				{
					RplComponent rpl = RplComponent.Cast(ent.FindComponent(RplComponent));
					re.m_mRented[struct.id].Insert(rpl.Id());
				}
			}	
		}
		
		foreach(OVT_WarehouseStruct struct : warehouses)
		{
			OVT_WarehouseData warehouse = new OVT_WarehouseData;
			warehouse.id = struct.id;
			warehouse.location = struct.location;
			warehouse.owner = struct.owner;
			warehouse.isPrivate = struct.isPrivate;
			warehouse.isLinked = struct.isLinked;
			warehouse.inventory = new map<int,int>;
			foreach(OVT_InventoryStruct inv : struct.inventory)
			{
				int resourceId = inv.id;
				ResourceName resource = rdb[resourceId];
				int res = ec.GetInventoryId(resource);
				warehouse.inventory[res] = inv.qty;
			}
			re.m_aWarehouses.Insert(warehouse);
		}
		
		return true;
	}
	
	void OVT_RealEstateStruct()
	{
		RegV("players");
		RegV("warehouses");
	}
}
