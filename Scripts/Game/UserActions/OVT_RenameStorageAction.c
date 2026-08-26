//------------------------------------------------------------------------------------------------
//! "Rename storage" - opens an edit-box dialog and asks the server for the new name.
//!
//! The name replicates, so it shows in the action label, in the transfer screen's destination picker
//! and on the map for every player. Permission is "anyone who may open the holder", enforced
//! server-side; the 1-32 length test below is local feedback only and the server repeats it.
//------------------------------------------------------------------------------------------------
class OVT_RenameStorageAction : OVT_StorageActionBase
{
	//! The recruit-rename preset: a confirm/cancel dialog whose content layout is one edit box. Its
	//! title and message are recruit-specific and are overridden below.
	protected const ResourceName DIALOG_PRESETS = "{272B6C4030554E27}Configs/UI/Dialogs/DialogPresets_Campaign.conf";

	//! Longest name the server accepts (OVT_StorageRequestComponent.NAME_MAX_LENGTH).
	protected const int NAME_MAX_LENGTH = 32;

	//! The open dialog. Held so the confirm handler can read its edit box and so both handlers can
	//! unsubscribe themselves; this action instance outlives the dialog.
	protected SCR_ConfigurableDialogUi m_Dialog;

	//------------------------------------------------------------------------------------------------
	//! \param[in] pOwnerEntity The holder.
	//! \param[in] pUserEntity The acting character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!CanBePerformedScript(pUserEntity))
			return;

		if (m_Dialog)
			return;

		SCR_ConfigurableDialogUi dialog = SCR_ConfigurableDialogUi.CreateFromPreset(DIALOG_PRESETS, "RENAME_RECRUIT");
		if (!dialog)
			return;

		dialog.SetTitle("#OVT-Storage_RenameTitle");
		dialog.SetMessage("#OVT-Storage_RenameMessage");

		SCR_EditBoxComponent editBox = SCR_EditBoxComponent.GetEditBoxComponent("EditBox", dialog.GetRootWidget());
		if (editBox)
		{
			OVT_StorageComponent storage = GetStorage();
			if (storage)
				editBox.SetValue(storage.GetCustomName());
		}

		m_Dialog = dialog;
		m_Dialog.m_OnConfirm.Insert(OnRenameConfirmed);
		m_Dialog.m_OnCancel.Insert(OnRenameCancel);
	}

	//------------------------------------------------------------------------------------------------
	//! Confirm: validate locally for immediate feedback, then ask the server.
	protected void OnRenameConfirmed()
	{
		if (!m_Dialog)
			return;

		SCR_EditBoxComponent editBox = SCR_EditBoxComponent.GetEditBoxComponent("EditBox", m_Dialog.GetRootWidget());

		string newName = "";
		if (editBox)
			newName = editBox.GetValue();

		ForgetDialog();

		if (newName.IsEmpty() || newName.Length() > NAME_MAX_LENGTH)
		{
			SCR_HintManagerComponent hints = SCR_HintManagerComponent.GetInstance();
			if (hints)
				hints.ShowCustom("#OVT-Storage_NameInvalid");

			return;
		}

		RplId holder = GetHolderId();
		if (!holder.IsValid())
			return;

		OVT_StorageRequestComponent requests = GetRequests();
		if (!requests)
			return;

		requests.RequestRenameHolder(holder, newName);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRenameCancel()
	{
		ForgetDialog();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes both listeners and drops the reference. Called from either outcome, so a second rename
	//! in the same session opens a fresh dialog with fresh handlers.
	protected void ForgetDialog()
	{
		if (!m_Dialog)
			return;

		m_Dialog.m_OnConfirm.Remove(OnRenameConfirmed);
		m_Dialog.m_OnCancel.Remove(OnRenameCancel);

		m_Dialog = null;
	}
}
