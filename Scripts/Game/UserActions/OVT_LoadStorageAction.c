
class OVT_LoadStorageAction : SCR_InventoryAction
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
		
		SCR_UniversalInventoryStorageComponent vehicleStorage = SCR_UniversalInventoryStorageComponent.Cast(nearestVeh.FindComponent(SCR_UniversalInventoryStorageComponent));
		if(!vehicleStorage)
		{
			return;
		}
		
		SCR_InventoryStorageManagerComponent boxStorage = SCR_InventoryStorageManagerComponent.Cast(pOwnerEntity.FindComponent(SCR_InventoryStorageManagerComponent));
		if(!boxStorage)
		{
			return;
		}
		
		array<IEntity> items = new array<IEntity>;
		boxStorage.GetItems(items);
		if(items.Count() == 0) {
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-StorageEmpty");
			return;
		}
		
		OVT_Global.GetServer().TransferStorage(pOwnerEntity, nearestVeh);
		
		SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-VehicleLoaded");
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