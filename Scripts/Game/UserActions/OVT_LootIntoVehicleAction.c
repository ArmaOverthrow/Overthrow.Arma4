
class OVT_LootIntoVehicleAction : ScriptedUserAction
{
	
	#ifndef DISABLE_INVENTORY
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{		
		array<IEntity> bodies();
		
		//Query the nearby world for weapons and bodies
		OVT_Global.GetNearbyBodiesAndWeapons(pOwnerEntity.GetOrigin(), 25, bodies);
			
		if(bodies.Count() == 0)
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-NoDeadBodiesNearby");
			return;
		}		
		
		// Check if container transfer component is available
		OVT_ContainerTransferComponent transfer = OVT_Global.GetContainerTransfer();
		if (!transfer || !transfer.IsAvailable())
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("Looting system busy, try again later");
			return;
		}
		
		// Determine the actual vehicle entity with storage
		IEntity vehicleEntity = pOwnerEntity;
		
		// Check if the entity has inventory storage, if not try parent
		InventoryStorageManagerComponent storageManager = InventoryStorageManagerComponent.Cast(
			vehicleEntity.FindComponent(InventoryStorageManagerComponent));
		
		if (!storageManager)
		{
			// Try parent entity (for vehicle compartments)
			IEntity parentEntity = vehicleEntity.GetParent();
			if (parentEntity)
			{
				storageManager = InventoryStorageManagerComponent.Cast(
					parentEntity.FindComponent(InventoryStorageManagerComponent));
				
				if (storageManager)
				{
					vehicleEntity = parentEntity;
				}
			}
		}
		
		if (!storageManager)
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("Vehicle has no storage capacity");
			return;
		}
		
		// Use new battlefield looting system with the correct entity
		OVT_Global.LootBattlefield(vehicleEntity, 25.0);
		
		SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-BodiesLooted");
	}
	
	override bool HasLocalEffectOnlyScript() { return true; };
	
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}
	
	#endif	
	
	
};