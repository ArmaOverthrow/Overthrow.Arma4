[BaseContainerProps()]
class OVT_VehiclesStruct : SCR_JsonApiStruct
{
	protected ref array<ref OVT_VehicleStruct> m_aVehicles = {};
	
	override bool Serialize()
	{
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		
		foreach(EntityID id : vehicles.m_aVehicles)
		{
			IEntity ent = GetGame().GetWorld().FindEntityByID(id);
			string owner = vehicles.GetOwnerID(ent);
			if(owner == "") continue;
			
			if(ent)
			{								
				OVT_VehicleStruct veh = new OVT_VehicleStruct();
				if(veh.Parse(ent))
					m_aVehicles.Insert(veh);
			}
		}
		
		return true;
	}
	
	override bool Deserialize()
	{
		foreach(OVT_VehicleStruct veh : m_aVehicles)
		{
			veh.Spawn();
		}
		
		return true;
	}
	
	void OVT_VehiclesStruct()
	{
		RegV("m_aVehicles");
	}
}

class OVT_VehicleStruct : SCR_JsonApiStruct
{
	string m_sPlayerId;
	ResourceName m_sResource;
	vector m_vPosition;
	vector m_vAngles;
	float m_fFuel;
	float m_fHealth;
	ref array<string> m_aInventory = {};
	
	IEntity Spawn()
	{
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		vector mat[4];		
		Math3D.AnglesToMatrix(m_vAngles, mat);
		mat[3] = m_vPosition;
		
		IEntity ent = vehicles.SpawnVehicleMatrix(m_sResource, mat, m_sPlayerId);
		
		SCR_DamageManagerComponent dmg = SCR_DamageManagerComponent.Cast(ent.FindComponent(SCR_DamageManagerComponent));
		if(dmg)
			dmg.SetHealthScaled(m_fHealth);
		
		SCR_FuelConsumptionComponent fuel = SCR_FuelConsumptionComponent.Cast(ent.FindComponent(SCR_FuelConsumptionComponent));
		if(fuel)
		{
			BaseFuelNode node = fuel.GetCurrentFuelTank();			
			node.SetFuel(m_fFuel);
		}
		
		InventoryStorageManagerComponent inv = InventoryStorageManagerComponent.Cast(ent.FindComponent(InventoryStorageManagerComponent));
		if(inv)
		{
			foreach(string res : m_aInventory)
			{
				inv.TrySpawnPrefabToStorage(res);
			}
		}
		
		return ent;
	}
	
	bool Parse(IEntity ent)
	{
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		SCR_DamageManagerComponent dmg = SCR_DamageManagerComponent.Cast(ent.FindComponent(SCR_DamageManagerComponent));
		if(dmg)
		{
			if(dmg.IsDestroyed()) return false;
		
			m_fHealth = dmg.GetHealth();
		}
		string owner = vehicles.GetOwnerID(ent);				
		m_sPlayerId = owner;
		m_sResource = ent.GetPrefabData().GetPrefabName();
		
		vector mat[4];
		ent.GetTransform(mat);
		m_vAngles = Math3D.MatrixToAngles(mat);
		m_vPosition = mat[3];
		
		SCR_FuelConsumptionComponent fuel = SCR_FuelConsumptionComponent.Cast(ent.FindComponent(SCR_FuelConsumptionComponent));
		if(fuel)
		{
			BaseFuelNode node = fuel.GetCurrentFuelTank();
			m_fFuel = node.GetFuel();
		}		
		
		InventoryStorageManagerComponent inv = InventoryStorageManagerComponent.Cast(ent.FindComponent(InventoryStorageManagerComponent));
		if(inv)
		{
			array<IEntity> items = new array<IEntity>;
			int count = inv.GetItems(items);
			if(count > 0)
			{
				foreach(IEntity item : items)
				{
					m_aInventory.Insert(item.GetPrefabData().GetPrefabName());
				}
			}
		}
				
		return true;
	}
		
	void OVT_VehicleStruct()
	{
		RegV("m_sPlayerId");
		RegV("m_sResource");
		RegV("m_vPosition");
		RegV("m_vAngles");
		RegV("m_fFuel");
		RegV("m_fHealth");
		RegV("m_aInventory");
	}
}