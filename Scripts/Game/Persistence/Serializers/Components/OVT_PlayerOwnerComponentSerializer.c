//------------------------------------------------------------------------------------------------
//! Persists who owns an entity and whether they have locked it.
//!
//! BINDING - THREE CONFIGURATIONS, ALL IN Configs/Systems/Persistence/Overthrow.conf. An entity gets
//! exactly ONE EntityPersistenceConfig, so this serializer has to be listed in every configuration
//! whose entities carry the component:
//!
//!   1. The PLACEABLE configuration (Overthrow group, rule on OVT_PlaceableComponent). Today the only
//!      placeable carrying the component is the placed ammo box
//!      (Prefabs/Props/Military/AmmoBoxes/OVT_AmmoBox_Placed.et), which is exactly the prefab EPF
//!      listed OVT_PlayerOwnerData on.
//!   2. Vanilla's CAR configuration {64C6B4937723DA61}, overridden by GUID in the Vehicles group.
//!      Overthrow overrides vanilla's Prefabs/Vehicles/Core/Wheeled_Base.et {62F416029692CE40} - the
//!      very prefab that configuration's rule matches - and adds OVT_PlayerOwnerComponent to it, so
//!      every wheeled vehicle in the game has an owner and a lock state to restore.
//!   3. Vanilla's HELICOPTER configuration {64EE8D74EB8192BA}, same story via
//!      Prefabs/Vehicles/Core/Helicopter_Base.et {CBD8C2393BE87581}.
//!
//! Without 2 and 3 a restored vehicle comes back unowned and unlocked - free to anyone, and no longer
//! yours to sell.
//!
//! NOT BOUND ON CHARACTERS, deliberately. The recruit prefab carries the component too, and a recruit
//! body IS stored and spawned back by id (OVT_RecruitManagerComponent.RequestPersistedRecruitBody) -
//! but ownership is not read back from that record. It does not need to be: nothing spawns a recruit
//! body except the recruit manager, which knows from the RECORD who owns it and applies that in
//! AttachRecruitBody(). Binding this here would restore the same value one step later and one step
//! less reliably.
//!
//! WHY THE APPLIES ARE GUARDED BY A DIFFERENCE CHECK. SetPlayerOwner() and SetLocked() both call
//! Replication.BumpMe() and fire a script invoker. Writing a value that is already there would dirty
//! the component and wake up every listener for nothing, and this Deserialize can run against a LIVE
//! entity (OVT_PersistenceManagerComponent.ReapplyLatestSaveData). Comparing first makes a repeat
//! application a true no-op rather than merely a harmless one.
//!
//! NO RPC. BumpMe is a replication dirty flag, not a remote call; clients receive both fields
//! through the component's own RplProp members.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first.
//------------------------------------------------------------------------------------------------
class OVT_PlayerOwnerComponentSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_PlayerOwnerComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the owning player's persistent id and the lock state.
	//! \param[in] owner The owned entity.
	//! \param[in] component The player owner component being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow player owner component.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_PlayerOwnerComponent playerOwner = OVT_PlayerOwnerComponent.Cast(component);
		if (!playerOwner)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		const string ownerUid = playerOwner.GetPlayerOwnerUid();
		context.Write(ownerUid);

		const bool locked = playerOwner.IsLocked();
		context.Write(locked);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Restores ownership and lock state, touching neither when they already match.
	//! \param[in] owner The owned entity.
	//! \param[in] component The player owner component being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_PlayerOwnerComponent playerOwner = OVT_PlayerOwnerComponent.Cast(component);
		if (!playerOwner)
			return false;

		// No version means no payload for this component - see OVT_TownManagerSerializer.Deserialize().
		// Without the guard an absent payload would hand a live, owned object back to nobody.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		string ownerUid;
		context.Read(ownerUid);

		bool locked;
		context.Read(locked);

		if (playerOwner.GetPlayerOwnerUid() != ownerUid)
			playerOwner.SetPlayerOwner(ownerUid);

		if (playerOwner.IsLocked() != locked)
			playerOwner.SetLocked(locked);

		return true;
	}
}
