class OVT_RealEstateManagerComponentClass: OVT_ComponentClass
{
};

class OVT_RealEstateManagerComponent: OVT_Component
{
	ref map<int, EntityID> m_mHomes;
	
	protected OVT_TownManagerComponent m_Town;
	
	override void OnPostInit(IEntity owner)
	{	
		super.OnPostInit(owner);
		
		if (SCR_Global.IsEditMode()) return;
		
		m_mHomes = new map<int, EntityID>;
		m_Town = OVT_TownManagerComponent.Cast(GetOwner().FindComponent(OVT_TownManagerComponent));
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
		}
		return GetGame().GetWorld().FindEntityByID(m_mHomes[playerId]);
	}
}