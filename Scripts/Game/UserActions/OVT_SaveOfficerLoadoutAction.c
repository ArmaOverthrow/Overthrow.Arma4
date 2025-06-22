//! UserAction for officers to save template loadouts that everyone can see
class OVT_SaveOfficerLoadoutAction : ScriptedUserAction
{
	protected SCR_ConfigurableDialogUi m_SaveDialog;
	protected IEntity m_UserEntity;
	
	//! Perform the save officer loadout action
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		m_UserEntity = pUserEntity;
		
		// Show name input dialog
		ShowSaveOfficerLoadoutDialog();
	}
	
	//! Get the action name for UI display
	override bool GetActionNameScript(out string outName)
	{
		outName = "#OVT-SaveOfficerLoadout";
		return true;
	}
	
	//! Check if action can be shown (only for officers)
	override bool CanBeShownScript(IEntity user)
	{
		// Only show if user has inventory manager (is a character)
		InventoryStorageManagerComponent storageManager = EPF_Component<InventoryStorageManagerComponent>.Find(user);
		if (!storageManager)
			return false;
		
		// Check if user is an officer
		string playerId = OVT_Global.GetPlayers().GetPersistentIDFromControlledEntity(user);
		if (playerId.IsEmpty())
			return false;
		
		OVT_PlayerManagerComponent playerMgr = OVT_Global.GetPlayers();
		int playerIdInt = playerMgr.GetPlayerIDFromPersistentID(playerId);
		
		return OVT_Global.GetResistanceFaction().IsOfficer(playerIdInt);
	}
	
	//! Local effect only (no network sync needed for UI)
	override bool HasLocalEffectOnlyScript() 
	{ 
		return true; 
	}
	
	//! Show dialog for entering officer loadout name
	protected void ShowSaveOfficerLoadoutDialog()
	{
		// Create configurable dialog with EditBox support
		SCR_ConfigurableDialogUi dialog = SCR_ConfigurableDialogUi.CreateFromPreset(
			"{272B6C4030554E27}Configs/UI/Dialogs/DialogPresets_Campaign.conf", 
			"RENAME_RECRUIT" // Reuse the rename dialog preset which has an EditBox
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
			editBox.SetValue("Officer Template"); // Default name for officer templates
		}
		
		// Store dialog reference for callback
		m_SaveDialog = dialog;
		
		// Set up callbacks
		dialog.m_OnConfirm.Insert(OnSaveOfficerConfirmed);
		dialog.m_OnCancel.Insert(OnSaveOfficerCancel);
	}
	
	//! Handle save confirmation
	protected void OnSaveOfficerConfirmed()
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
		
		// Save the officer template loadout via RPC for multiplayer support
		OVT_PlayerCommsComponent comms = OVT_Global.GetServer();
		if (comms)
		{
			comms.SaveLoadout(playerId, loadoutName, "", true); // Officer template
			SCR_HintManagerComponent.ShowCustomHint(
				string.Format("Officer template '%1' saved successfully!", loadoutName), 
				"Officer Template Saved", 
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
	protected void OnSaveOfficerCancel()
	{
		m_SaveDialog = null;
	}
}