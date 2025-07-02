
class OVT_LootIntoVehicleAction : SCR_VehicleActionBase
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
		
		// Use new battlefield looting system
		OVT_Global.LootBattlefield(pOwnerEntity, 25.0);
		
		SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-BodiesLooted");
	}
	
	override bool HasLocalEffectOnlyScript() { return true; };
	
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}
	
	#endif	
	
	
};