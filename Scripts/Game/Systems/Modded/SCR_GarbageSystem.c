modded class SCR_GarbageSystem
{
	//! Override the base game garbage collection to protect player-owned vehicles
	override protected float OnInsertRequested(IEntity entity, float lifetime)
	{
		// Check if this is a vehicle with a player owner component
		if (Vehicle.Cast(entity))
		{
			OVT_PlayerOwnerComponent playerOwnerComp = OVT_PlayerOwnerComponent.Cast(entity.FindComponent(OVT_PlayerOwnerComponent));
			if (playerOwnerComp)
			{
				string ownerUID = playerOwnerComp.GetPlayerOwnerUid();
				// If vehicle has a player owner, never garbage collect it
				if (ownerUID != "")
				{
					return -1; // Negative value prevents insertion into garbage system
				}
			}
		}
		
		// For all other entities, use the base game logic
		return super.OnInsertRequested(entity, lifetime);
	}
}