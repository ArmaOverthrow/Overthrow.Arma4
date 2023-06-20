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
	
	protected bool m_bRegistered;
	
	override void OnPostInit(IEntity owner)
	{	
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT | EntityEvent.FRAME);
	}
	
	override void EOnFrame(IEntity owner, float timeSlice) //!EntityEvent.FRAME
	{		
		if(!m_bRegistered)
		{
			if(!m_UiInfo) return;
			if(!m_bShowOnMap) return;
			m_bRegistered = true;
			if(m_bMustOwnBase)
			{
				OVT_BaseData base = OVT_Global.GetOccupyingFaction().GetNearestBase(owner.GetOrigin());
				float dist = vector.Distance(base.location, owner.GetOrigin());
				if(dist > base.range) return;
			}
			OVT_MapIcons.RegisterPOI(m_UiInfo, owner.GetOrigin(), m_bMustOwnBase);
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