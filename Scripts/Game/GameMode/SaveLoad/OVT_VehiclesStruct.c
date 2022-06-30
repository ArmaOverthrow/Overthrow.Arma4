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
	vector m_vRight;
	vector m_vUp;
	vector m_vForward;
	float m_fFuel;
	float m_fHealth;
	
	IEntity Spawn()
	{
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		vector mat[4];
		mat[0] = m_vRight;
		mat[1] = m_vUp;
		mat[2] = m_vForward;
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
		
		m_vRight = mat[0];
		m_vUp = mat[1];
		m_vForward = mat[2];
		m_vPosition = mat[3];
		
		SCR_FuelConsumptionComponent fuel = SCR_FuelConsumptionComponent.Cast(ent.FindComponent(SCR_FuelConsumptionComponent));
		if(fuel)
		{
			BaseFuelNode node = fuel.GetCurrentFuelTank();
			m_fFuel = node.GetFuel();
		}		
		return true;
	}
		
	void OVT_VehicleStruct()
	{
		RegV("m_sPlayerId");
		RegV("m_sResource");
		RegV("m_vPosition");
		RegV("m_vRight");
		RegV("m_vUp");
		RegV("m_vForward");
		RegV("m_fFuel");
		RegV("m_fHealth");
	}
}