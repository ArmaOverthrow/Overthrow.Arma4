
class OVT_UnloadStorageAction : SCR_InventoryAction
{
	protected ref array<IEntity> m_Vehicles;
	
	#ifndef DISABLE_INVENTORY
	//------------------------------------------------------------------------------------------------
	override protected void PerformActionInternal(SCR_InventoryStorageManagerComponent manager, IEntity pOwnerEntity, IEntity pUserEntity)
	{		
		m_Vehicles = new array<IEntity>;	
		GetGame().GetWorld().QueryEntitiesBySphere(pOwnerEntity.GetOrigin(), 10, null, FilterVehicleEntities, EQueryEntitiesFlags.ALL);
			
		if(m_Vehicles.Count() == 0)
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-NoVehiclesNearby");
			return;
		}
		
		//Find nearest
		float nearestDist = 15;
		IEntity nearestVeh;
		foreach(IEntity ent : m_Vehicles)
		{
			float dist = vector.Distance(ent.GetOrigin(), pOwnerEntity.GetOrigin());
			if(dist < nearestDist)
			{
				nearestVeh = ent;
				nearestDist = dist;
			}
		}
		
		//Make sure noone is driving it
		Vehicle veh = Vehicle.Cast(nearestVeh);
		SCR_BaseCompartmentManagerComponent access = SCR_BaseCompartmentManagerComponent.Cast(veh.FindComponent(SCR_BaseCompartmentManagerComponent));
		array<IEntity> pilots = {};
		access.GetOccupantsOfType(pilots, ECompartmentType.Pilot);
		
		if(pilots.Count() > 0)
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-DriverMustExit");
			return;
		}
		
		
		
		SCR_VehicleInventoryStorageManagerComponent vehicleStorage = SCR_VehicleInventoryStorageManagerComponent.Cast(nearestVeh.FindComponent(SCR_VehicleInventoryStorageManagerComponent));
		if(!vehicleStorage)
		{
			return;
		}
		
		SCR_UniversalInventoryStorageComponent boxStorage = SCR_UniversalInventoryStorageComponent.Cast(pOwnerEntity.FindComponent(SCR_UniversalInventoryStorageComponent));
		if(!boxStorage)
		{
			return;
		}
		
		
		
		autoptr array<IEntity> items = new array<IEntity>;
		vehicleStorage.GetItems(items);
		if(items.Count() == 0) {
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-VehicleEmpty");
			return;
		}
		
		OVT_Global.GetServer().TransferStorage(nearestVeh, pOwnerEntity);	
		SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-VehicleUnloaded");
	}
	
	#endif	
	
	protected bool FilterVehicleEntities(IEntity entity)
	{
		if(entity.ClassName() == "Vehicle")
		{
			m_Vehicles.Insert(entity);
		}
		return false;
	}
};