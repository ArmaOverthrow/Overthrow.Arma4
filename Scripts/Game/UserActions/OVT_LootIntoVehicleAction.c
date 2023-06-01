
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
		
		OVT_Global.GetServer().LootIntoVehicle(pOwnerEntity);
		
		SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-BodiesLooted");
	}
	
	override bool HasLocalEffectOnlyScript() { return true; };
	
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}
	
	#endif	
	
	
};