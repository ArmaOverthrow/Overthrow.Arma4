//------------------------------------------------------------------------------------------------
//! Shared gate for every Open Storage user action: the holder must have a ledger, must have a
//! capacity, must not be a ruin, and must not be locked to somebody else.
//!
//! This is a CLIENT MIRROR of OVT_StorageRequestComponent.MayUseHolder and nothing more - it decides
//! whether a button is worth showing. The server re-checks everything, including the distance, which
//! is why nothing here measures one (the action context's own radius already did).
//------------------------------------------------------------------------------------------------
class OVT_StorageActionBase : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	//! \return The entity that actually carries the ledger. These actions are also hosted on truck
	//! cargo beds - vanilla's real "door_rear" tailgate context lives on the bed, not the vehicle root
	//! - so the storage is one hop up from the owner there.
	protected IEntity Holder()
	{
		return OVT_StorageUtils.ResolveStorageHolder(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! \return The holder's storage component, or null.
	protected OVT_StorageComponent GetStorage()
	{
		return OVT_StorageUtils.GetStorage(Holder());
	}

	//------------------------------------------------------------------------------------------------
	//! \return The holder's networked name, or RplId.Invalid().
	protected RplId GetHolderId()
	{
		return OVT_StorageUtils.GetHolderId(Holder());
	}

	//------------------------------------------------------------------------------------------------
	//! \return The local player's storage request component, or null.
	protected OVT_StorageRequestComponent GetRequests()
	{
		return OVT_ControllerComponent<OVT_StorageRequestComponent>.Get();
	}

	//------------------------------------------------------------------------------------------------
	//! Hidden on a holder with no ledger, no capacity (an illegal or armed vehicle, a helicopter) or
	//! a ruined structure.
	//! \param[in] user The acting character.
	//! \return True when the action may be drawn.
	override bool CanBeShownScript(IEntity user)
	{
		if (!OVT_StructureDamage.IsUsable(Holder()))
			return false;

		OVT_StorageComponent storage = GetStorage();
		if (!storage)
			return false;

		return storage.GetCapacity() != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Mirrors OVT_OpenStorageAction's lock rule so a locked crate or vehicle refuses with a reason
	//! instead of sending a request the server will refuse.
	//! \param[in] user The acting character.
	//! \return True when the action may be performed.
	override bool CanBePerformedScript(IEntity user)
	{
		if (!user)
			return false;

		if (!WarehouseIsOpenTo(user))
		{
			SetCannotPerformReason("#OVT-Storage_NoAccess");
			return false;
		}

		RplComponent userRpl = RplComponent.Cast(user.FindComponent(RplComponent));
		if (!userRpl)
			return false;

		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if (!mode)
			return userRpl.IsOwner();

		OVT_PlayerOwnerComponent playerOwner = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(Holder());
		if (!playerOwner || !playerOwner.IsLocked())
			return userRpl.IsOwner();

		string ownerUid = playerOwner.GetPlayerOwnerUid();
		if (ownerUid == "")
			return userRpl.IsOwner();

		string playerUid = OVT_Global.GetPlayers().GetPersistentIDFromControlledEntity(user);
		if (ownerUid != playerUid)
		{
			SetCannotPerformReason("#OVT-Locked");
			return false;
		}

		return userRpl.IsOwner();
	}

	//------------------------------------------------------------------------------------------------
	//! The warehouse half of the client mirror: the same body the server gate and the vehicle menu's
	//! warehouse buttons call (I5). Anything that is not a warehouse building passes.
	//! \param[in] user The acting character.
	//! \return True when this holder is not a warehouse, or is one this player may open.
	protected bool WarehouseIsOpenTo(IEntity user)
	{
		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if (!realEstate)
			return true;

		string playerUid = OVT_Global.GetPlayers().GetPersistentIDFromControlledEntity(user);

		return realEstate.PlayerMayUseWarehouse(playerUid, Holder());
	}

	//------------------------------------------------------------------------------------------------
	//! The work goes over the request component, so nothing here needs to run on the server.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}
}
