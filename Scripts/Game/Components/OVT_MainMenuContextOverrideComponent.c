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
	
	//! Map registration used to live here, driven from EOnFrame. It now belongs to
	//! OVT_MapMarkerComponent + OVT_MapMarkerManagerComponent: the three prefabs that used to register
	//! a POI this way carry a marker component instead. What remains here is purely the in-world
	//! interaction-menu role.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if(SCR_Global.IsEditMode())
			return;

		SetEventMask(owner, EntityEvent.INIT);
	}

	bool CanShow(IEntity player)
	{
		if(!player) return false;
		bool isDriver = false;
		
		SCR_CompartmentAccessComponent compartment = SCR_CompartmentAccessComponent.Cast(player.FindComponent(SCR_CompartmentAccessComponent));
				
		if(compartment && compartment.IsInCompartment() && compartment.GetCompartmentType(compartment.GetCompartment()) == ECompartmentType.PILOT){
			isDriver = true;
		}
		
		if(m_bMustOwnBase)
		{
			OVT_BaseData base = OVT_Global.GetOccupyingFaction().GetNearestBase(GetOwner().GetOrigin());			
			if(base && base.IsOccupyingFaction()) return false;
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