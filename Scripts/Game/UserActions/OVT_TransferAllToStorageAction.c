//------------------------------------------------------------------------------------------------
//! "Transfer all to storage" - converts the holder's whole vanilla inventory into its ledger.
//!
//! Attachments and loaded magazines go in as separate items; part-used magazines stay behind. All of
//! that is the server's sweep, not this action's - the action is one request.
//------------------------------------------------------------------------------------------------
class OVT_TransferAllToStorageAction : OVT_StorageActionBase
{
	//------------------------------------------------------------------------------------------------
	//! \param[in] pOwnerEntity The holder.
	//! \param[in] pUserEntity The acting character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!CanBePerformedScript(pUserEntity))
			return;

		RplId holder = GetHolderId();
		if (!holder.IsValid())
			return;

		OVT_StorageRequestComponent requests = GetRequests();
		if (!requests)
			return;

		requests.RequestTransferAllToStorage(holder);
	}

	//------------------------------------------------------------------------------------------------
	//! Additionally hidden on a holder with no vanilla inventory to sweep - a warehouse building.
	//! \param[in] user The acting character.
	//! \return True when the action may be drawn.
	override bool CanBeShownScript(IEntity user)
	{
		if (!OVT_StorageUtils.GetInventoryManager(GetOwner()))
			return false;

		return super.CanBeShownScript(user);
	}
}
