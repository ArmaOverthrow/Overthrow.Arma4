
class OVT_OpenStorageAction : SCR_InventoryAction
{
	#ifndef DISABLE_INVENTORY
	//------------------------------------------------------------------------------------------------
	override protected void PerformActionInternal(SCR_InventoryStorageManagerComponent manager, IEntity pOwnerEntity, IEntity pUserEntity)
	{		
		manager.SetStorageToOpen( pOwnerEntity );
		manager.OpenInventory();
		
		// Play sound
		RplComponent rplComp = RplComponent.Cast(pOwnerEntity.FindComponent(RplComponent));
		if (rplComp)
			manager.PlayItemSound(pOwnerEntity, "SOUND_OPEN");			
		else
		{
			SimpleSoundComponent simpleSoundComp = SimpleSoundComponent.Cast(pOwnerEntity.FindComponent(SimpleSoundComponent));
			if (simpleSoundComp)
			{
				vector mat[4];
				pOwnerEntity.GetWorldTransform(mat);
				
				simpleSoundComp.SetTransformation(mat);
				simpleSoundComp.PlayStr("SOUND_OPEN");
			}
		}
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
	
	#endif
	
	
};