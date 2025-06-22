//! UserAction for saving player equipment loadouts from equipment boxes
class OVT_SaveLoadoutAction : ScriptedUserAction
{
	protected SCR_ConfigurableDialogUi m_SaveDialog;
	protected IEntity m_UserEntity;
	
	//! Perform the save loadout action
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		m_UserEntity = pUserEntity;
		
		// Show name input dialog
		ShowSaveLoadoutDialog();
	}
	
	//! Get the action name for UI display
	override bool GetActionNameScript(out string outName)
	{
		outName = "#OVT-SaveLoadout";
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
	
	//! Show dialog for entering loadout name
	protected void ShowSaveLoadoutDialog()
	{
		// Create configurable dialog with EditBox support
		SCR_ConfigurableDialogUi dialog = SCR_ConfigurableDialogUi.CreateFromPreset(
			"{272B6C4030554E27}Configs/UI/Dialogs/DialogPresets_Campaign.conf", 
			"SAVE_LOADOUT" // Use dedicated save loadout dialog with medium layout
		);
		
		if (!dialog)
		{
			SCR_HintManagerComponent.ShowCustomHint("Could not open save dialog", "Error", 3.0);
			return;
		}
		
		// Try to access EditBox and set placeholder text
		SCR_EditBoxComponent editBox = SCR_EditBoxComponent.GetEditBoxComponent("EditBox", dialog.GetRootWidget());
		if (editBox)
		{
			editBox.SetValue("My Loadout"); // Default name
		}
		
		// Store dialog reference for callback
		m_SaveDialog = dialog;
		
		// Set up callbacks
		dialog.m_OnConfirm.Insert(OnSaveConfirmed);
		dialog.m_OnCancel.Insert(OnSaveCancel);
	}
	
	//! Handle save confirmation
	protected void OnSaveConfirmed()
	{
		if (!m_SaveDialog || !m_UserEntity)
			return;
		
		// Get the entered loadout name
		SCR_EditBoxComponent editBox = SCR_EditBoxComponent.GetEditBoxComponent("EditBox", m_SaveDialog.GetRootWidget());
		if (!editBox)
		{
			SCR_HintManagerComponent.ShowCustomHint("Failed to get loadout name", "Error", 3.0);
			m_SaveDialog = null;
			return;
		}
		
		string loadoutName = editBox.GetValue();
		
		// Validate name
		if (loadoutName.IsEmpty() || loadoutName.Length() > 32)
		{
			SCR_HintManagerComponent.ShowCustomHint("Invalid name length (1-32 characters)", "Error", 3.0);
			m_SaveDialog = null;
			return;
		}
		
		// Get player persistent ID
		string playerId = OVT_Global.GetPlayers().GetPersistentIDFromControlledEntity(m_UserEntity);
		if (playerId.IsEmpty())
		{
			SCR_HintManagerComponent.ShowCustomHint("Failed to get player ID", "Error", 3.0);
			m_SaveDialog = null;
			return;
		}
		
		// Save the loadout via RPC for multiplayer support
		OVT_PlayerCommsComponent comms = OVT_Global.GetServer();
		if (comms)
		{
			comms.SaveLoadout(playerId, loadoutName, "", false); // Regular loadout, not officer template
			SCR_HintManagerComponent.ShowCustomHint(
				string.Format("Loadout '%1' saved successfully!", loadoutName), 
				"Loadout Saved", 
				3.0
			);
		}
		else
		{
			SCR_HintManagerComponent.ShowCustomHint("Communication component not available", "Error", 3.0);
		}
		
		m_SaveDialog = null;
	}
	
	//! Handle save cancellation
	protected void OnSaveCancel()
	{
		m_SaveDialog = null;
	}
}