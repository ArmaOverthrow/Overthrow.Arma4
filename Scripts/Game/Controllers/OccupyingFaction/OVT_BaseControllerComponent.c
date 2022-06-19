class OVT_BaseControllerComponentClass: OVT_ComponentClass
{
};

class OVT_BaseControllerComponent: OVT_Component
{
	[Attribute(defvalue: "0", UIWidgets.EditBox, desc: "Resources to allocate for testing only")]
	int m_iTestingResources;
	
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_BaseUpgrade> m_aBaseUpgrades;
					
	int m_iRange = 250;
	int m_iCloseRange = 150;
	
	string m_sName;	
	
	ref array<ref EntityID> m_AllSlots;
	ref array<ref EntityID> m_AllCloseSlots;
	ref array<ref EntityID> m_SmallSlots;
	ref array<ref EntityID> m_MediumSlots;
	ref array<ref EntityID> m_LargeSlots;
	ref array<ref EntityID> m_SmallRoadSlots;
	ref array<ref EntityID> m_MediumRoadSlots;
	ref array<ref EntityID> m_LargeRoadSlots;
	ref array<ref EntityID> m_Parking;
		
	protected OVT_OccupyingFactionManager m_occupyingFactionManager;
	
	protected const int UPGRADE_UPDATE_FREQUENCY = 10000;
	
	EntityID m_Flag;	
	
	override void OnPostInit(IEntity owner)
	{	
		super.OnPostInit(owner);
		
		if(!Replication.IsServer()) return;
		if (SCR_Global.IsEditMode()) return;
		
		m_occupyingFactionManager = OVT_Global.GetOccupyingFaction();
		
		InitializeBase();
		
		GetGame().GetCallqueue().CallLater(UpdateUpgrades, UPGRADE_UPDATE_FREQUENCY, true, owner);		
	}
	
	protected void UpdateUpgrades()
	{
		foreach(OVT_BaseUpgrade upgrade : m_aBaseUpgrades)
		{
			upgrade.OnUpdate(UPGRADE_UPDATE_FREQUENCY);
		}
	}
	
	void InitializeBase()
	{		
		m_AllSlots = new array<ref EntityID>;
		m_AllCloseSlots = new array<ref EntityID>;
		m_SmallSlots = new array<ref EntityID>;
		m_MediumSlots = new array<ref EntityID>;
		m_LargeSlots = new array<ref EntityID>;
		m_SmallRoadSlots = new array<ref EntityID>;
		m_MediumRoadSlots = new array<ref EntityID>;
		m_LargeRoadSlots = new array<ref EntityID>;
		m_Parking = new array<ref EntityID>;
		
		FindSlots();
		FindParking();
		
		//Spawn a flag
		
		vector pos = GetOwner().GetOrigin();
		
		float groundHeight = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
		if (pos[1] < groundHeight)
			pos[1] = groundHeight;
		
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;
		IEntity flag = GetGame().SpawnEntityPrefab(Resource.Load(m_Config.GetOccupyingFaction().m_aFlagPolePrefab), null, params);
		
		foreach(OVT_BaseUpgrade upgrade : m_aBaseUpgrades)
		{
			upgrade.Init(this, m_occupyingFactionManager, m_Config);
		}
		
		//Spend testing resources (if any)
		if(m_iTestingResources > 0){
			SpendResources(m_iTestingResources);
		}
	}
	
	OVT_BaseUpgrade FindUpgrade(typename type)
	{
		foreach(OVT_BaseUpgrade upgrade : m_aBaseUpgrades)
		{
			if(upgrade.ClassName() == type.ToString()) return upgrade;
		}
		return null;
	}
	
	void FindSlots()
	{
		GetGame().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(), m_iRange, CheckSlotAddToArray, FilterSlotEntities, EQueryEntitiesFlags.ALL);
	}
	
	bool FilterSlotEntities(IEntity entity)
	{
		if(entity.ClassName() == "SCR_SiteSlotEntity") return true;
		return false;
	}
	
	bool CheckSlotAddToArray(IEntity entity)
	{
		m_AllSlots.Insert(entity.GetID());
		
		float distance = vector.Distance(entity.GetOrigin(), GetOwner().GetOrigin());
		if(distance < m_iCloseRange)
		{
			m_AllCloseSlots.Insert(entity.GetID());
		}
		
		string name = entity.GetPrefabData().GetPrefabName();
		if(name.IndexOf("FlatSmall") > -1) m_SmallSlots.Insert(entity.GetID());
		if(name.IndexOf("FlatMedium") > -1) m_MediumSlots.Insert(entity.GetID());
		if(name.IndexOf("FlatLarge") > -1) m_LargeSlots.Insert(entity.GetID());
		if(name.IndexOf("RoadSmall") > -1) m_SmallRoadSlots.Insert(entity.GetID());
		if(name.IndexOf("RoadMedium") > -1) m_MediumRoadSlots.Insert(entity.GetID());
		if(name.IndexOf("RoadLarge") > -1) m_LargeRoadSlots.Insert(entity.GetID());
		
		return true;
	}
	
	void FindParking()
	{
		GetGame().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(), m_iRange, null, FilterParkingEntities, EQueryEntitiesFlags.ALL);
	}
	
	bool FilterParkingEntities(IEntity entity)
	{
		if(entity.FindComponent(OVT_ParkingComponent)) {
			m_Parking.Insert(entity.GetID());
		}
		return false;
	}
	
	int SpendResources(int resources, float threat = 0)
	{
		int spent = 0;
		
		for(int priority = 1; priority < 100; priority++)
		{
			if(resources <= 0) break;
			foreach(OVT_BaseUpgrade upgrade : m_aBaseUpgrades)
			{
				if(resources <= 0) break;
				if(upgrade.m_iPriority == priority)
				{					
					int allocate = upgrade.m_iResourceAllocation;
					int newres = 0;
					if(allocate < 0)
					{
						//Ignore allocation, spend recklessly
						newres = upgrade.Spend(resources);
					}else{
						if(resources < allocate) allocate = resources;
						newres = upgrade.SpendToAllocation();
					}
					
					spent += newres;
					resources -= newres;
				}
			}
		}
		
		return spent;
	}
	
	void ~OVT_BaseControllerComponent()
	{
		if(m_aBaseUpgrades)
		{
			m_aBaseUpgrades.Clear();
			m_aBaseUpgrades = null;
		}
		if(m_AllSlots)
		{
			m_AllSlots.Clear();
			m_AllSlots = null;
		}
		if(m_AllCloseSlots)
		{
			m_AllCloseSlots.Clear();
			m_AllCloseSlots = null;
		}
		if(m_SmallSlots)
		{
			m_SmallSlots.Clear();
			m_SmallSlots = null;
		}
		if(m_MediumSlots)
		{
			m_MediumSlots.Clear();
			m_MediumSlots = null;
		}
		if(m_LargeSlots)
		{
			m_LargeSlots.Clear();
			m_LargeSlots = null;
		}
		if(m_SmallRoadSlots)
		{
			m_SmallRoadSlots.Clear();
			m_SmallRoadSlots = null;
		}
		if(m_MediumRoadSlots)
		{
			m_MediumRoadSlots.Clear();
			m_MediumRoadSlots = null;
		}
		if(m_LargeRoadSlots)
		{
			m_LargeRoadSlots.Clear();
			m_LargeRoadSlots = null;
		}
		if(m_Parking)
		{
			m_Parking.Clear();
			m_Parking = null;
		}
	}
}