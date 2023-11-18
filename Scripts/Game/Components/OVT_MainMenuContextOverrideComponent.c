class OVT_MainMenuContextOverrideComponentClass : OVT_ComponentClass
{}

class OVT_MainMenuContextOverrideComponent : OVT_Component
{
	[Attribute()]
	string m_ContextName;
		
	[Attribute()]
	ref SCR_UIInfo m_UiInfo;
	
	[Attribute("5")]
	float m_fRange;
	
	[Attribute("0")]
	bool m_bMustOwnBase;
	
	[Attribute("0")]
	bool m_bMustBeDriving;
	
	[Attribute("1")]
	bool m_bShowOnMap;
	
	protected bool m_bRegistered = false;
	
	override void OnPostInit(IEntity owner)
	{		
		super.OnPostInit(owner);
		if(SCR_Global.IsEditMode())
			return;
		
		SetEventMask(owner, EntityEvent.INIT | EntityEvent.FRAME);
	}
	
	override void EOnFrame(IEntity owner, float timeSlice) //!EntityEvent.FRAME
	{		
		if(!m_bRegistered)
		{
			if(!m_bShowOnMap || !m_UiInfo) 
			{
				m_bRegistered = true;
				return;
			}
			if(!owner) return;			
			if(m_bMustOwnBase)
			{
				OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
				if(!of) return;
				OVT_BaseData base = of.GetNearestBase(owner.GetOrigin());
				if(!base) return;
				float dist = vector.Distance(base.location, owner.GetOrigin());
				if(dist > 220) return;
			}
			OVT_MapIcons.RegisterPOI(m_UiInfo, owner.GetOrigin(), m_bMustOwnBase);
			m_bRegistered = true;
		}
	}
	
	bool CanShow(IEntity player)
	{
		if(!player) return false;
		bool isDriver = false;
		
		SCR_CompartmentAccessComponent compartment = SCR_CompartmentAccessComponent.Cast(player.FindComponent(SCR_CompartmentAccessComponent));
				
		if(compartment && compartment.IsInCompartment() && compartment.GetCompartmentType(compartment.GetCompartment()) == ECompartmentType.Pilot){
			isDriver = true;
		}
		
		if(m_bMustOwnBase)
		{
			OVT_BaseData base = OVT_Global.GetOccupyingFaction().GetNearestBase(GetOwner().GetOrigin());			
			if(base.IsOccupyingFaction()) return false;
			return true;
		}		
		if(m_bMustBeDriving)
		{
			if(isDriver) return true;
			return false;
		}
		return false;
	}
}