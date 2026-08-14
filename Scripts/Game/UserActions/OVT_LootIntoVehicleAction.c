
class OVT_LootIntoVehicleAction : ScriptedUserAction
{
	
	#ifndef DISABLE_INVENTORY
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{		
		array<IEntity> bodies();
		
		//Query the nearby world for weapons and bodies
		OVT_WorldUtils.GetNearbyBodiesAndWeapons(pOwnerEntity.GetOrigin(), 25, bodies);
			
		if(bodies.Count() == 0)
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-NoDeadBodiesNearby");
			return;
		}		
		
		// Check if container transfer component is available
		OVT_ContainerTransferComponent transfer = OVT_ControllerComponent<OVT_ContainerTransferComponent>.Get();
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
		
		// Use new battlefield looting system with the correct entity.
		// Re-checked rather than reusing the availability test above: resolving the vehicle entity
		// ran arbitrary code in between, and this is the call that actually starts the transfer.
		// (Body inlined from the former OVT_Global.LootBattlefield wrapper - domain logic does not
		// belong on the service locator, and this was its only caller.)
		transfer = OVT_ControllerComponent<OVT_ContainerTransferComponent>.Get();
		if (transfer && transfer.IsAvailable())
		{
			transfer.LootBattlefield(vehicleEntity, 25.0);
		}

		SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-BodiesLooted");
	}
	
	override bool HasLocalEffectOnlyScript() { return true; };
	
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}
	
	#endif	
	
	
};