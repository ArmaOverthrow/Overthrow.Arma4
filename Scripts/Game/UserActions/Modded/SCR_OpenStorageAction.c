//------------------------------------------------------------------------------------------------
modded class SCR_OpenStorageAction : SCR_InventoryAction
{
	override bool CanBePerformedScript(IEntity user)
 	{
		if (!user)
			return false;
		Managed genericInventoryManager = user.FindComponent( SCR_InventoryStorageManagerComponent );
		if (!genericInventoryManager)
			return false;
		RplComponent genericRpl = RplComponent.Cast(user.FindComponent( RplComponent ));
		if (!genericRpl)
			return false;
		
		OVT_OverthrowGameMode ot = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(!ot) return genericRpl.IsOwner();
		
		OVT_PlayerOwnerComponent playerowner = EPF_Component<OVT_PlayerOwnerComponent>.Find(GetOwner());
		if(!playerowner || !playerowner.IsLocked()) return genericRpl.IsOwner();
		
		string ownerUid = playerowner.GetPlayerOwnerUid();
		if(ownerUid == "") return genericRpl.IsOwner();
		
		string playerUid = OVT_Global.GetPlayers().GetPersistentIDFromControlledEntity(user);
		if(ownerUid != playerUid)
		{
			SetCannotPerformReason("#OVT-Locked");
			return false;
		}
		
		return genericRpl.IsOwner();
 	}	
};