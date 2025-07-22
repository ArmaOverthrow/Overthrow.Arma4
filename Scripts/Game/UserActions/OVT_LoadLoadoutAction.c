//! UserAction for opening the loadout management interface
class OVT_LoadLoadoutAction : ScriptedUserAction
{
	
	//! Perform the load loadout action
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// Open the loadouts management UI using UI manager
		OVT_UIManagerComponent uiManager = OVT_UIManagerComponent.Cast(pUserEntity.FindComponent(OVT_UIManagerComponent));
		if (!uiManager)
			return;
		
		// Get the loadouts context and set the equipment box
		OVT_LoadoutsContext loadoutsContext = OVT_LoadoutsContext.Cast(uiManager.GetContext(OVT_LoadoutsContext));
		if (loadoutsContext)
		{
			loadoutsContext.SetEquipmentBox(pOwnerEntity);
		}
		// Removed invalid cast and SetEquipmentBox for GunBuilderUI_SlotCategoryUIContainer
		uiManager.ShowContext(OVT_LoadoutsContext);
	}
	
	//! Get the action name for UI display
	override bool GetActionNameScript(out string outName)
	{
		outName = "#OVT-LoadLoadout";
		return true;
	}
	
	//! Check if action can be shown
	override bool CanBeShownScript(IEntity user)
	{
		// Only show if user has inventory manager (is a character)
		InventoryStorageManagerComponent storageManager = EPF_Component<InventoryStorageManagerComponent>.Find(user);
		return storageManager != null;
	}
	
	//! Local effect only (no network sync needed for UI)
	override bool HasLocalEffectOnlyScript() 
	{ 
		return true; 
	}
}