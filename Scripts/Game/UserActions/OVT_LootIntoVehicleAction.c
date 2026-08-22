//------------------------------------------------------------------------------------------------
//! "Loot battlefield" - collects nearby bodies and every loose item around them into this vehicle's
//! LEDGER, on the storage feature's per-player job engine.
//!
//! The work is one LOOT job on OVT_StorageRequestComponent: chunked over the call queue, reported on
//! the controller's progress bar, and unfiltered except for the three base garments a body keeps.
//! Nothing here reaches OVT_InventoryManagerComponent any more, and nothing here writes English at
//! the player.
//------------------------------------------------------------------------------------------------
class OVT_LootIntoVehicleAction : ScriptedUserAction
{
	//! What the job collects from. Bounded again on the server.
	protected const float LOOT_RADIUS = 25;

	//------------------------------------------------------------------------------------------------
	//! \param[in] pOwnerEntity The vehicle, or a part of it.
	//! \param[in] pUserEntity The acting character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		IEntity holder = ResolveHolder(pOwnerEntity);
		if (!holder)
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-Storage_NoCapacity");
			return;
		}

		array<IEntity> lootables = {};
		OVT_StorageLootQuery query = new OVT_StorageLootQuery();
		query.Run(holder.GetOrigin(), LOOT_RADIUS, lootables);

		if (lootables.IsEmpty())
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-NoDeadBodiesNearby");
			return;
		}

		OVT_StorageRequestComponent requests = OVT_ControllerComponent<OVT_StorageRequestComponent>.Get();
		if (requests && requests.IsBusy())
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-Storage_Busy");
			return;
		}

		OVT_ContainerTransferComponent transfer = OVT_ControllerComponent<OVT_ContainerTransferComponent>.Get();
		if (!transfer)
			return;

		// The hint goes FIRST: on a listen host the request below runs the server's gates inline, and a
		// refusal hint drawn inside it would be overwritten by this one.
		SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-BodiesLooted");

		transfer.LootBattlefield(holder, LOOT_RADIUS);
	}

	//------------------------------------------------------------------------------------------------
	//! The entity that actually carries the ledger and the inventory. The action can be authored on a
	//! compartment or a part, so the parent is tried when the owner is not a holder itself.
	//! \param[in] pOwnerEntity The entity the action sits on.
	//! \return The holder, or null when neither it nor its parent is one.
	protected IEntity ResolveHolder(IEntity pOwnerEntity)
	{
		if (!pOwnerEntity)
			return null;

		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(pOwnerEntity);
		if (storage && storage.GetCapacity() != OVT_StorageComponent.NO_CAPACITY)
			return pOwnerEntity;

		IEntity parent = pOwnerEntity.GetParent();
		if (!parent)
			return null;

		storage = OVT_StorageUtils.GetStorage(parent);
		if (storage && storage.GetCapacity() != OVT_StorageComponent.NO_CAPACITY)
			return parent;

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The work goes over the container transfer component's validated handler, so nothing here needs
	//! to run on the server.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] user The acting character.
	//! \return Always true; every refusal is reported by PerformAction or by the server.
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}
}
