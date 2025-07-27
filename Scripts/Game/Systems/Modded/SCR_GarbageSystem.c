modded class SCR_GarbageSystem
{
	//! Override the base game garbage collection to protect player-owned vehicles and convoy vehicles
	override protected float OnInsertRequested(IEntity entity, float lifetime)
	{
		// Check if this is a vehicle with a player owner component
		if (Vehicle.Cast(entity))
		{
			OVT_PlayerOwnerComponent playerOwnerComp = OVT_PlayerOwnerComponent.Cast(entity.FindComponent(OVT_PlayerOwnerComponent));
			if (playerOwnerComp)
			{
				string ownerUID = playerOwnerComp.GetPlayerOwnerUid();
				// If vehicle has a player owner or is a convoy vehicle, never garbage collect it
				if (ownerUID != "" || ownerUID == "CONVOY_SYSTEM")
				{
					return -1; // Negative value prevents insertion into garbage system
				}
			}
			
			// Also check if this is a convoy vehicle by ID tracking
			if (TSE_ConvoyEventManagerComponent.IsConvoyVehicle(entity))
			{
				return -1; // Protect convoy vehicles from garbage collection
			}
		}
		
		// For all other entities, use the base game logic
		return super.OnInsertRequested(entity, lifetime);
	}
}