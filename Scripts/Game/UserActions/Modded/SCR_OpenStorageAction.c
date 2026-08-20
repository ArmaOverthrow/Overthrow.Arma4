//------------------------------------------------------------------------------------------------
modded class SCR_OpenStorageAction : SCR_InventoryAction
{
	//------------------------------------------------------------------------------------------------
	//! Hidden while the structure this storage belongs to is a ruin (core/damage D15). This is the
	//! GENERIC container action - it appears on anything that carries a storage - so it is the one
	//! that would show up first on a buildable that gains one. Contents are untouched by a phase
	//! change; repairing is how they come back.
	//! \param[in] user The acting character.
	//! \return True unless the owner is a ruined structure.
	override bool CanBeShownScript(IEntity user)
	{
		if (!OVT_StructureDamage.IsUsable(GetOwner()))
			return false;

		return super.CanBeShownScript(user);
	}

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
		
		OVT_PlayerOwnerComponent playerowner = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(GetOwner());
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