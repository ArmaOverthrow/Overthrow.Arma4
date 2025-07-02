//! Base class for user actions that need to display dialogs
//! Provides common functionality for dialog handling and interaction menu management
class OVT_DialogUserAction : ScriptedUserAction
{
	protected SCR_ConfigurableDialogUi m_Dialog;
	protected IEntity m_UserEntity;
	
	//! Close the context interaction menu to prevent z-order conflicts with dialogs
	protected void CloseInteractionMenu()
	{
		// Get the player controller
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController)
			return;
		
		// Get the interaction handler component
		SCR_InteractionHandlerComponent interactionHandler = SCR_InteractionHandlerComponent.Cast(
			playerController.FindComponent(SCR_InteractionHandlerComponent)
		);
		
		if (interactionHandler)
		{
			// Try to get the HUD manager and find the interaction display
			HUDManagerComponent hudManager = HUDManagerComponent.Cast(playerController.FindComponent(HUDManagerComponent));
			if (hudManager)
			{
				array<BaseInfoDisplay> displayInfos = {};
				int count = hudManager.GetInfoDisplays(displayInfos);
				for (int i = 0; i < count; i++)
				{
					SCR_BaseInteractionDisplay current = SCR_BaseInteractionDisplay.Cast(displayInfos[i]);
					if (current)
					{
						current.HideDisplay();
						break;
					}
				}
			}
		}
	}
	
	//! Create a dialog from preset with automatic interaction menu closure
	protected SCR_ConfigurableDialogUi CreateDialog(string configPath, string presetName)
	{
		// Close interaction menu first to prevent Z-order conflicts
		CloseInteractionMenu();
		
		// Create the dialog
		SCR_ConfigurableDialogUi dialog = SCR_ConfigurableDialogUi.CreateFromPreset(configPath, presetName);
		
		if (dialog)
		{
			m_Dialog = dialog;
		}
		
		return dialog;
	}
	
	//! Get the current dialog (if any)
	protected SCR_ConfigurableDialogUi GetDialog()
	{
		return m_Dialog;
	}
	
	//! Clear the dialog reference
	protected void ClearDialog()
	{
		m_Dialog = null;
	}
	
	//! Get the user entity that triggered the action
	protected IEntity GetUserEntity()
	{
		return m_UserEntity;
	}
	
	//! Set the user entity (should be called in PerformAction)
	protected void SetUserEntity(IEntity userEntity)
	{
		m_UserEntity = userEntity;
	}
	
	//! Local effect only by default (most dialog actions don't need network sync)
	override bool HasLocalEffectOnlyScript() 
	{ 
		return true; 
	}
	
	//! Default implementation - can be overridden by derived classes
	override bool CanBeShownScript(IEntity user)
	{
		// Only show if user has inventory manager (is a character)
		InventoryStorageManagerComponent storageManager = EPF_Component<InventoryStorageManagerComponent>.Find(user);
		return storageManager != null;
	}
}