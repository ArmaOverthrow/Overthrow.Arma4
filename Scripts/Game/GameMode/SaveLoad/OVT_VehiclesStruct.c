[BaseContainerProps()]
class OVT_VehiclesStruct : OVT_BaseSaveStruct
{
	protected ref array<ref OVT_VehicleStruct> vehicles = {};
	
	override bool Serialize()
	{
		vehicles.Clear();
		
		OVT_VehicleManagerComponent vehMgr = OVT_Global.GetVehicles();
		
		foreach(EntityID id : vehMgr.m_aVehicles)
		{
			IEntity ent = GetGame().GetWorld().FindEntityByID(id);
			string owner = vehMgr.GetOwnerID(ent);
			if(owner == "") continue;
			
			if(ent)
			{								
				OVT_VehicleStruct veh = new OVT_VehicleStruct();
				if(veh.Parse(ent,rdb))
					vehicles.Insert(veh);
			}
		}
		
		return true;
	}
	
	override bool Deserialize()
	{
		foreach(OVT_VehicleStruct veh : vehicles)
		{
			veh.Spawn(rdb);
		}
		
		return true;
	}
	
	void OVT_VehiclesStruct()
	{
		RegV("vehicles");
	}
}

class OVT_VehicleStruct : SCR_JsonApiStruct
{
	string owner;
	int res;
	vector pos;
	vector ang;
	float fuel;
	float health;
	ref array<int> inv = {};
	
	IEntity Spawn(array<string> resources)
	{
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		vector mat[4];		
		Math3D.AnglesToMatrix(ang, mat);
		mat[3] = pos;
		
		IEntity ent = vehicles.SpawnVehicleMatrix(resources[res], mat, owner);
		
		SCR_DamageManagerComponent dmg = SCR_DamageManagerComponent.Cast(ent.FindComponent(SCR_DamageManagerComponent));
		if(dmg)
			dmg.SetHealthScaled(health);
		
		SCR_FuelConsumptionComponent f = SCR_FuelConsumptionComponent.Cast(ent.FindComponent(SCR_FuelConsumptionComponent));
		if(f)
		{
			BaseFuelNode node = f.GetCurrentFuelTank();			
			node.SetFuel(fuel);
		}
		
		InventoryStorageManagerComponent invMgr = InventoryStorageManagerComponent.Cast(ent.FindComponent(InventoryStorageManagerComponent));
		if(invMgr)
		{
			foreach(int id : inv)
			{
				invMgr.TrySpawnPrefabToStorage(resources[id]);
			}
		}
		
		return ent;
	}
	
	bool Parse(IEntity ent, inout array<string> resources)
	{
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		SCR_DamageManagerComponent dmg = SCR_DamageManagerComponent.Cast(ent.FindComponent(SCR_DamageManagerComponent));
		if(dmg)
		{
			if(dmg.IsDestroyed()) return false;
		
			health = dmg.GetHealth();
		}
		owner = vehicles.GetOwnerID(ent);	
		string resource = ent.GetPrefabData().GetPrefabName();
		
		res = resources.Find(resource);
		if(res == -1)
		{
			resources.Insert(resource);
			res = resources.Count() - 1;
		}		
		
		vector mat[4];
		ent.GetTransform(mat);
		ang = Math3D.MatrixToAngles(mat);
		pos = mat[3];
		
		SCR_FuelConsumptionComponent f = SCR_FuelConsumptionComponent.Cast(ent.FindComponent(SCR_FuelConsumptionComponent));
		if(f)
		{
			BaseFuelNode node = f.GetCurrentFuelTank();
			fuel = node.GetFuel();
		}		
		
		InventoryStorageManagerComponent invMgr = InventoryStorageManagerComponent.Cast(ent.FindComponent(InventoryStorageManagerComponent));
		if(invMgr)
		{
			array<IEntity> items = new array<IEntity>;
			int count = invMgr.GetItems(items);
			if(count > 0)
			{
				foreach(IEntity item : items)
				{
					string r = item.GetPrefabData().GetPrefabName();
					int id = resources.Find(r);
					if(id == -1)
					{
						resources.Insert(r);
						id = resources.Count() - 1;
					}
					inv.Insert(id);
				}
			}
		}
				
		return true;
	}
		
	void OVT_VehicleStruct()
	{
		RegV("owner");
		RegV("res");
		RegV("pos");
		RegV("ang");
		RegV("fuel");
		RegV("health");
		RegV("inv");
	}
}