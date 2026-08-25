modded class SCR_GarbageSystem
{
	//! Override the base game garbage collection to protect player-owned and deployment-owned vehicles
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

			// 🔴 AND A VEHICLE A DEPLOYMENT IS STILL USING (author, 2026-08-26). The player-owner test
			// above never matches one - the occupying faction's vehicles have no player owner - so a
			// patrol truck carried vanilla's ordinary lifetime and could be collected mid-patrol,
			// taking its mounted crew with it, with nothing in any Overthrow log.
			//
			// ⚠ ASKED LAST, and only for vehicles, so the common case is one Cast and one component
			// lookup. OVT_DeploymentManagerComponent.IsDeploymentVehicle() walks live deployments -
			// see its header for why it is a walk and not a registry.
			OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
			if (deployments && deployments.IsDeploymentVehicle(entity))
			{
				return -1;
			}
		}
		
		// For all other entities, use the base game logic
		return super.OnInsertRequested(entity, lifetime);
	}
}
