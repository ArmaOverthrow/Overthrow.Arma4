//------------------------------------------------------------------------------------------------
//! "Clear inventory" - empties the holder's VANILLA inventory, never its ledger. Officer-only.
//!
//! This is how the leftovers a sweep deliberately refuses (part-used magazines, containers that
//! still hold something) get discarded. The officer test here is a client mirror; the server runs
//! its own.
//------------------------------------------------------------------------------------------------
class OVT_ClearVanillaInventoryAction : OVT_StorageActionBase
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

		requests.RequestClearVanillaInventory(holder);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] user The acting character.
	//! \return True when the local player is an officer and the holder has a vanilla inventory.
	override bool CanBeShownScript(IEntity user)
	{
		if (!OVT_StorageUtils.GetInventoryManager(GetOwner()))
			return false;

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance || !resistance.IsLocalPlayerOfficer())
			return false;

		return super.CanBeShownScript(user);
	}
}
