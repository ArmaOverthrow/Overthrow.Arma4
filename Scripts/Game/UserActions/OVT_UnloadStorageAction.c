//------------------------------------------------------------------------------------------------
//! "Unload vehicle into storage" - sweeps the nearest vehicle's vanilla inventory into its own
//! ledger, then moves that whole ledger into this box.
//!
//! THE SWEEP IS THE ASYMMETRY (D6). "Loot a battlefield, drive to a box, Unload" has to end with
//! everything in the box, and a loot run lands in the truck's VANILLA inventory - so this half
//! converts first and Load deliberately does not.
//------------------------------------------------------------------------------------------------
class OVT_UnloadStorageAction : OVT_StorageVehicleActionBase
{
	//------------------------------------------------------------------------------------------------
	//! \param[in] pOwnerEntity The box.
	//! \param[in] pUserEntity The acting character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!CanBePerformedScript(pUserEntity))
			return;

		IEntity vehicle = ResolveVehicle(pOwnerEntity);
		if (!vehicle)
			return;

		if (VehicleIsEmpty(vehicle))
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-VehicleEmpty");
			return;
		}

		SendMove(vehicle, pOwnerEntity, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the vehicle has nothing to give: an empty ledger AND an empty vanilla inventory. Both
	//! halves matter because the sweep is what turns the second into the first.
	//! \param[in] vehicle The vehicle being unloaded.
	//! \return True when there is nothing to move.
	protected bool VehicleIsEmpty(IEntity vehicle)
	{
		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(vehicle);
		if (storage && storage.GetTotalCount() > 0)
			return false;

		InventoryStorageManagerComponent inventory = OVT_StorageUtils.GetInventoryManager(vehicle);
		if (!inventory)
			return true;

		array<IEntity> items = {};
		inventory.GetItems(items);

		return items.IsEmpty();
	}
}
