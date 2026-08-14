class OVT_LockVehicleAction : ScriptedUserAction
{		
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		if (!CanBeShownScript(pUserEntity))
		 	return;
		
		OVT_PlayerOwnerComponent playerowner = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(GetOwner());
		if(playerowner)
		{
			// HasLocalEffectOnlyScript() is true, so this runs ONLY on the performing player's machine -
			// the local controller is always the right seam. Null-guarded because the accessor answers
			// null before ownership assignment (and forever on a dedicated server, which never performs
			// a user action). On a listen host this now works at all: the legacy path sent an
			// RplRcver.Server Rpc from the authority, which is delivered to nobody.
			OVT_VehicleRequestComponent vehicles = OVT_ControllerComponent<OVT_VehicleRequestComponent>.Get();
			if(vehicles)
				vehicles.SetVehicleLock(GetOwner(), true);
		}
 	}
	
	override bool CanBeShownScript(IEntity user)
 	{		
		OVT_OverthrowGameMode ot = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(!ot) return false;
		
		OVT_PlayerOwnerComponent playerowner = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(GetOwner());
		if(!playerowner || playerowner.IsLocked()) return false;
		
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