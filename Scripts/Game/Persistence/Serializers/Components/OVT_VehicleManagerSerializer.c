//------------------------------------------------------------------------------------------------
//! One player-owned vehicle registration, as it travels in the save.
//!
//! FIELD ORDER IS THE FORMAT. Reflection writes and reads these members in declaration order and the
//! save context is binary, so a new field may only ever be APPENDED - never inserted, never reordered,
//! never removed.
//!
//! This is also the LIVE shape: OVT_VehicleManagerComponent keeps a map of these rather than a
//! parallel runtime class, so the thing that is saved and the thing the manager reasons about cannot
//! drift apart.
//------------------------------------------------------------------------------------------------
class OVT_PersistedPlayerVehicle
{
	//! The vehicle's persistence id - the key, and what RequestSpawn is asked for.
	string persistentId;

	//! Persistent id of the owning player.
	string ownerUid;

	//! Prefab the vehicle was spawned from, so it can be rebuilt when its own record is unreadable.
	ResourceName prefab;

	//! Where it was last seen standing, and which way it faced.
	vector position;
	vector angles;

	//! Whether the owner had it locked.
	bool locked;
}

//------------------------------------------------------------------------------------------------
//! Persists which vehicles belong to which player, and enough about each to rebuild it.
//!
//! BINDING. Listed in the ComponentSerializers block of the game-mode configuration in
//! Configs/Systems/Persistence/Overthrow.conf.
//!
//! WHY THIS EXISTS (2026-08-04 dedicated play-test). OVT_VehicleManagerComponent's ownership registry
//! was memory-only. A player's locked vehicle is saved, released and deleted 60 s after they log out
//! (OFFLINE_VEHICLE_DESPAWN_TIME), so it is not a world entity when the save is written; after a
//! restart nothing remembered its id, nothing asked for it, and the player's car was gone for good.
//! The registry is the missing half, and it is small: one row per owned vehicle.
//!
//! WHAT THIS DOES NOT PERSIST. The vehicle's own contents - fuel, damage, inventory - which vanilla's
//! Car/Helicopter configs already serialize on the vehicle instance itself. This record is the
//! POINTER to that instance plus the minimum needed to reconstruct a stand-in if the pointer no longer
//! resolves. Keeping the two separate is deliberate: duplicating cargo here would give two answers to
//! the same question and no way to tell which is current.
//!
//! LIVE INSTANCES ARE NOT RE-SPAWNED FROM THIS. Vehicles standing in the world when the save is taken
//! come back through vanilla's own vehicle persistence; applying these records only rebuilds the
//! manager's bookkeeping so those vehicles are managed again, and asks for the despawned ones.
//!
//! IDEMPOTENT ON A LIVE SESSION. ApplyPersistedVehicles() replaces records by id and only appends ids
//! it does not already hold, so re-applying a save to a running campaign cannot duplicate a
//! registration.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first.
//!
//! VERSION HISTORY.
//!   1 - vehicle registrations (id, owner, prefab, transform, lock state)
//------------------------------------------------------------------------------------------------
class OVT_VehicleManagerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_VehicleManagerComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes one record per registered player vehicle.
	//! \param[in] owner The game mode entity owning the vehicle manager.
	//! \param[in] component The vehicle manager being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow vehicle manager.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_VehicleManagerComponent vehicles = OVT_VehicleManagerComponent.Cast(component);
		if (!vehicles)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		array<ref OVT_PersistedPlayerVehicle> records = new array<ref OVT_PersistedPlayerVehicle>();

		// The manager keeps these current - it captures a record at every registration, refreshes them
		// from SyncVehicleRecords() immediately before this runs, and captures once more just before a
		// vehicle is despawned. A codec must not go looking at the world itself.
		map<string, ref OVT_PersistedPlayerVehicle> live = vehicles.GetVehicleRecords();
		if (live)
		{
			for (int i = 0; i < live.Count(); i++)
			{
				OVT_PersistedPlayerVehicle record = live.GetElement(i);
				if (!record)
					continue;

				// A record with no id or no owner cannot be acted on when it comes back, and writing it
				// would grow the save with rows nothing can ever use.
				if (record.persistentId.IsEmpty() || record.ownerUid.IsEmpty())
					continue;

				records.Insert(record);
			}
		}

		context.Write(records);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the registrations back and hands them to the manager to apply.
	//! \param[in] owner The game mode entity owning the vehicle manager.
	//! \param[in] component The vehicle manager being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_VehicleManagerComponent vehicles = OVT_VehicleManagerComponent.Cast(component);
		if (!vehicles)
			return false;

		// See OVT_TownManagerSerializer.Deserialize() - no version means no payload for this component.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		array<ref OVT_PersistedPlayerVehicle> records = new array<ref OVT_PersistedPlayerVehicle>();
		context.Read(records);

		vehicles.ApplyPersistedVehicles(records);

		return true;
	}
}
