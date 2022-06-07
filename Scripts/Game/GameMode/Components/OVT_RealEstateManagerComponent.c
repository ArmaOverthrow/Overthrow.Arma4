class OVT_RealEstateManagerComponentClass: OVT_ComponentClass
{
};

class OVT_RealEstateManagerComponent: OVT_Component
{
	ref map<int, EntityID> m_mHomes;
	
	protected OVT_TownManagerComponent m_Town;
	
	static OVT_RealEstateManagerComponent s_Instance;
	
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
	
	override void OnPostInit(IEntity owner)
	{	
		super.OnPostInit(owner);
		
		if (SCR_Global.IsEditMode()) return;
		
		m_mHomes = new map<int, EntityID>;
		m_Town = OVT_TownManagerComponent.Cast(GetOwner().FindComponent(OVT_TownManagerComponent));
		
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, CheckPubDisableMarker, FilterPubEntities, EQueryEntitiesFlags.STATIC);
	}
	
	protected bool CheckPubDisableMarker(IEntity entity)
	{
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (mapdesc){
			mapdesc.Item().SetVisible(false);
		}
		return true;
	}
	
	protected bool FilterPubEntities(IEntity entity)
	{
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (mapdesc){
			if(mapdesc.GetBaseType() == EMapDescriptorType.MDT_PUB) return true;
		}
				
		return false;	
	}
	
	void SetHome(int playerId, EntityID entityId)
	{
		m_mHomes[playerId] = entityId;
	}
	
	IEntity GetHome(int playerId)
	{
		
		if(!m_mHomes.Contains(playerId))
		{
			IEntity newHome = m_Town.GetRandomHouse();
			SetHome(playerId, newHome.GetID());
			SpawnStartingCar(newHome.GetOrigin());
			
			m_Config.SpawnMarkerLocal(newHome.GetOrigin(),EMapDescriptorType.MDT_PUB);
		}
		return GetGame().GetWorld().FindEntityByID(m_mHomes[playerId]);
	}
	
	void SpawnStartingCar(vector pos)
	{
		vector roadPos = OVT_NearestRoad.Find(pos);
		
		Print(roadPos);
	}
}