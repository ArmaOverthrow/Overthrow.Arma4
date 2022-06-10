class OVT_RealEstateManagerComponentClass: OVT_ComponentClass
{
};

class OVT_RealEstateManagerComponent: OVT_Component
{
	ref map<int, EntityID> m_mHomes;
	ref map<int, ref set<EntityID>> m_mOwned;
	
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
		m_mOwned = new map<int, ref set<EntityID>>;
		m_Town = OVT_TownManagerComponent.Cast(GetOwner().FindComponent(OVT_TownManagerComponent));	
	}
	
	void SetHome(int playerId, EntityID entityId)
	{		
		m_mHomes[playerId] = entityId;
	}
	
	void SetOwner(int playerId, EntityID entityId)
	{
		if(!m_mOwned.Contains(playerId)) m_mOwned[playerId] = new set<EntityID>;
		set<EntityID> owner = m_mOwned[playerId];
		owner.Insert(entityId);
	}
	
	bool IsOwner(int playerId, EntityID entityId)
	{
		if(!m_mOwned.Contains(playerId)) return false;
		set<EntityID> owner = m_mOwned[playerId];
		return owner.Contains(entityId);
	}
	
	set<EntityID> GetOwned(int playerId)
	{
		if(!m_mOwned.Contains(playerId)) return new set<EntityID>;
		return m_mOwned[playerId];
	}
	
	IEntity GetNearestOwned(int playerId, vector pos)
	{
		if(!m_mOwned.Contains(playerId)) return null;
		
		BaseWorld world = GetGame().GetWorld();
		
		float nearest = 999999;
		IEntity nearestEnt;		
		
		set<EntityID> owner = m_mOwned[playerId];
		foreach(EntityID id : owner)
		{
			IEntity ent = world.FindEntityByID(id);
			float dist = vector.Distance(ent.GetOrigin(), pos);
			if(dist < nearest)
			{
				nearest = dist;
				nearestEnt = ent;
			}
		}
		
		return nearestEnt;
	}
	
	IEntity GetHome(int playerId)
	{
		
		if(!m_mHomes.Contains(playerId))
		{
			IEntity newHome = m_Town.GetRandomHouse();
			SetOwner(playerId, newHome.GetID());
			SetHome(playerId, newHome.GetID());			
			SpawnStartingCar(newHome.GetOrigin());
		}
		return GetGame().GetWorld().FindEntityByID(m_mHomes[playerId]);
	}
	
	void SpawnStartingCar(vector pos)
	{
		vector roadPos = OVT_NearestRoad.Find(pos);
		
		Print(roadPos);
	}
}