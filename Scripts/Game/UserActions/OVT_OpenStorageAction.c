
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
			manager.PlayItemSound(rplComp.Id(), "SOUND_OPEN");
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
	
	#endif
	
	
};