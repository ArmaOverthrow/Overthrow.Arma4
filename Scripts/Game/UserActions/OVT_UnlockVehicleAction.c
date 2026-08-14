class OVT_UnlockVehicleAction : ScriptedUserAction
{	
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		if (!CanBeShownScript(pUserEntity))
		 	return;
		
		OVT_PlayerOwnerComponent playerowner = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(GetOwner());
		if(playerowner)
		{
			// See OVT_LockVehicleAction: local-effect-only action, so this is always the performing
			// player's own controller seam, and the null guard covers pre-assignment.
			OVT_VehicleRequestComponent vehicles = OVT_ControllerComponent<OVT_VehicleRequestComponent>.Get();
			if(vehicles)
				vehicles.SetVehicleLock(GetOwner(), false);
		}
 	}
	
	override bool CanBeShownScript(IEntity user)
 	{		
		OVT_OverthrowGameMode ot = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(!ot) return false;
		
		OVT_PlayerOwnerComponent playerowner = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(GetOwner());
		if(!playerowner || !playerowner.IsLocked()) return false;
		
		string ownerUid = playerowner.GetPlayerOwnerUid();
		if(ownerUid == "") return false;
		
		string playerUid = OVT_Global.GetPlayers().GetPersistentIDFromControlledEntity(user);
		if(ownerUid != playerUid) return false;
		
		return true;
 	}
	
	override bool CanBePerformedScript(IEntity user)
 	{		
		RplComponent genericRpl = RplComponent.Cast(user.FindComponent( RplComponent ));
		if (!genericRpl)
			return false;
		
		return genericRpl.IsOwner();
 	}
	
	override bool HasLocalEffectOnlyScript() { return true; };
}