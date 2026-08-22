//------------------------------------------------------------------------------------------------
//! "Load storage into vehicle" - moves this box's whole LEDGER into the nearest vehicle's ledger.
//!
//! STORAGE ONLY, AND IT DOES NOT SWEEP (D6). A box's vanilla inventory may hold a rifle a player
//! deliberately parked there, and converting it silently would be a surprise with no undo. Unload is
//! the asymmetric half: it sweeps, because the truck it is emptying has just been looted.
//------------------------------------------------------------------------------------------------
class OVT_LoadStorageAction : OVT_StorageVehicleActionBase
{
	//------------------------------------------------------------------------------------------------
	//! \param[in] pOwnerEntity The box.
	//! \param[in] pUserEntity The acting character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!CanBePerformedScript(pUserEntity))
			return;

		OVT_StorageComponent storage = GetStorage();
		if (!storage)
			return;

		if (storage.GetTotalCount() <= 0)
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-StorageEmpty");
			return;
		}

		IEntity vehicle = ResolveVehicle(pOwnerEntity);
		if (!vehicle)
			return;

		SendMove(pOwnerEntity, vehicle, false);
	}
}
