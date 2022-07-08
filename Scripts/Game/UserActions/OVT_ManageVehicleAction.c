class OVT_ManageVehicleAction : ScriptedUserAction
{	
	protected ref array<IEntity> m_Vehicles;
	
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		if (!CanBePerformedScript(pUserEntity))
		 	return;
		
		m_Vehicles = new array<IEntity>;	
		GetGame().GetWorld().QueryEntitiesBySphere(pOwnerEntity.GetOrigin(), 3, null, FilterVehicleEntities, EQueryEntitiesFlags.ALL);
			
		if(m_Vehicles.Count() == 0)
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-NoVehicleOnRamp");
			return;
		}
		
		//Make sure noone is driving it
		Vehicle veh = Vehicle.Cast(m_Vehicles[0]);
		SCR_BaseCompartmentManagerComponent access = SCR_BaseCompartmentManagerComponent.Cast(veh.FindComponent(SCR_BaseCompartmentManagerComponent));
		array<IEntity> pilots = {};
		access.GetOccupantsOfType(pilots, ECompartmentType.Pilot);
		
		if(pilots.Count() > 0)
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-DriverMustExit");
			return;
		}
		
		OVT_UIManagerComponent ui = OVT_UIManagerComponent.Cast(pUserEntity.FindComponent(OVT_UIManagerComponent));
		if(!ui) return;
		
		OVT_ManageVehicleContext context = OVT_ManageVehicleContext.Cast(ui.GetContext(OVT_ManageVehicleContext));
		if(!context) return;
		
		context.m_Vehicle = veh;
		context.ShowLayout();		
 	}
		
	override bool GetActionNameScript(out string outName)
	{
		return false;
	}	
	
	override bool CanBePerformedScript(IEntity user)
 	{		
		RplComponent genericRpl = RplComponent.Cast(user.FindComponent( RplComponent ));
		if (!genericRpl)
			return false;
		
		return genericRpl.IsOwner();
 	}
	
	protected bool FilterVehicleEntities(IEntity entity)
	{
		if(entity.ClassName() == "Vehicle")
		{
			m_Vehicles.Insert(entity);
		}
		return false;
	}
	
	override bool CanBeShownScript(IEntity user) {
		return true;
	}
}