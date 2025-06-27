//! UserAction for saving player equipment loadouts from equipment boxes
class OVT_SaveLoadoutAction : OVT_DialogUserAction
{
	//! Perform the save loadout action
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		SetUserEntity(pUserEntity);
		
		// Show name input dialog
		ShowSaveLoadoutDialog();
	}
	
	//! Get the action name for UI display
	override bool GetActionNameScript(out string outName)
	{
		outName = "#OVT-SaveLoadout";
		return true;
	}
	
	// Inherits CanBeShownScript and HasLocalEffectOnlyScript from base class
	
	//! Show dialog for entering loadout name
	protected void ShowSaveLoadoutDialog()
	{
		// Create configurable dialog with EditBox support (automatically closes interaction menu)
		SCR_ConfigurableDialogUi dialog = CreateDialog(
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
		
		// Set up callbacks
		dialog.m_OnConfirm.Insert(OnSaveConfirmed);
		dialog.m_OnCancel.Insert(OnSaveCancel);
	}
	
	//! Handle save confirmation
	protected void OnSaveConfirmed()
	{
		SCR_ConfigurableDialogUi dialog = GetDialog();
		IEntity userEntity = GetUserEntity();
		
		if (!dialog || !userEntity)
			return;
		
		// Get the entered loadout name
		SCR_EditBoxComponent editBox = SCR_EditBoxComponent.GetEditBoxComponent("EditBox", dialog.GetRootWidget());
		if (!editBox)
		{
			SCR_HintManagerComponent.ShowCustomHint("Failed to get loadout name", "Error", 3.0);
			ClearDialog();
			return;
		}
		
		string loadoutName = editBox.GetValue();
		
		// Validate name
		if (loadoutName.IsEmpty() || loadoutName.Length() > 32)
		{
			SCR_HintManagerComponent.ShowCustomHint("Invalid name length (1-32 characters)", "Error", 3.0);
			ClearDialog();
			return;
		}
		
		// Get player persistent ID
		string playerId = OVT_Global.GetPlayers().GetPersistentIDFromControlledEntity(userEntity);
		if (playerId.IsEmpty())
		{
			SCR_HintManagerComponent.ShowCustomHint("Failed to get player ID", "Error", 3.0);
			ClearDialog();
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
		
		ClearDialog();
	}
	
	//! Handle save cancellation
	protected void OnSaveCancel()
	{
		ClearDialog();
	}
}