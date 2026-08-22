//------------------------------------------------------------------------------------------------
//! One High Command group's persisted record. A dedicated class, NOT the live OVT_HighCommandRecord
//! (implementation.md §3.11) - the persisted class name is frozen into every save as `$type`, while
//! the live record's wire shape still gets to change.
//!
//! FIELD ORDER IS THE FORMAT: binary contexts are positional, so a new field may only be APPENDED.
//------------------------------------------------------------------------------------------------
class OVT_PersistedHighCommandGroup
{
	string groupId;
	string owner;
	string entryKey;
	int stance;
	vector destination;
	vector lastPosition;
	ref array<string> memberBodyIds = {};
	ref array<ResourceName> memberPrefabs = {};
}

//------------------------------------------------------------------------------------------------
//! Persists the High Command group table held by OVT_HighCommandManagerComponent.
//!
//! BINDING. Listed in the ComponentSerializers block of the game-mode configuration in
//! Configs/Systems/Persistence/Overthrow.conf.
//!
//! SCOPE (PHASE 1): records only. Member arrays are written from the start (empty until Phase 2
//! starts populating them) so the format never needs a version bump for them. The live group
//! ENTITY, its observer and its waypoints are not covered here - those are Phase 2+ state and are
//! rebuilt from the record on load, not persisted directly.
//!
//! POST-LOAD. No RPC, deliberately - deserialization runs while the world is still being built, and
//! clients receive the whole table through the manager's own JIP payload once Phase 6 adds it.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first.
//------------------------------------------------------------------------------------------------
class OVT_HighCommandManagerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_HighCommandManagerComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes one record per group.
	//! \param[in] owner The game mode entity owning the High Command manager.
	//! \param[in] component The manager being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow High Command manager.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_HighCommandManagerComponent highCommand = OVT_HighCommandManagerComponent.Cast(component);
		if (!highCommand)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		// The LOCAL NAME IS THE PROPERTY NAME - Write() derives the key from the variable it is
		// handed - so `records` must be spelled identically in Deserialize below.
		array<ref OVT_PersistedHighCommandGroup> records = new array<ref OVT_PersistedHighCommandGroup>();
		BuildRecords(highCommand, records);

		context.Write(records);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the group records back and hands them to the manager to apply.
	//! \param[in] owner The game mode entity owning the High Command manager.
	//! \param[in] component The manager being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_HighCommandManagerComponent highCommand = OVT_HighCommandManagerComponent.Cast(component);
		if (!highCommand)
			return false;

		// No version means no payload - every save taken before this feature existed.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		array<ref OVT_PersistedHighCommandGroup> records = new array<ref OVT_PersistedHighCommandGroup>();
		if (!context.Read(records))
			return AbortUnreadablePayload();

		highCommand.ApplyPersistedGroups(records);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Snapshots every live record into its persisted shape.
	//! \param[in] highCommand The manager being saved.
	//! \param[out] records Receives one persisted record per live group with an owner.
	protected void BuildRecords(notnull OVT_HighCommandManagerComponent highCommand, notnull array<ref OVT_PersistedHighCommandGroup> records)
	{
		if (!highCommand.m_mGroups)
			return;

		for (int i = 0; i < highCommand.m_mGroups.Count(); i++)
		{
			string groupId = highCommand.m_mGroups.GetKey(i);
			OVT_HighCommandRecord record = highCommand.m_mGroups.GetElement(i);

			if (groupId == "" || !record || record.m_sOwnerPersistentId == "")
				continue;

			OVT_PersistedHighCommandGroup persisted = new OVT_PersistedHighCommandGroup();
			persisted.groupId = groupId;
			persisted.owner = record.m_sOwnerPersistentId;
			persisted.entryKey = record.m_sEntryKey;
			persisted.stance = record.m_iStance;
			persisted.destination = record.m_vDestination;
			persisted.lastPosition = record.m_vLastKnownPosition;
			persisted.memberBodyIds = record.m_aMemberBodyIds;
			persisted.memberPrefabs = record.m_aMemberPrefabs;

			records.Insert(persisted);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Reports an unreadable payload and consumes it without touching the live group table.
	//! \return True - the payload is consumed either way; nothing was applied.
	protected bool AbortUnreadablePayload()
	{
		Print("[Overthrow] Could not read the High Command group table - the live group table is left exactly as it is", LogLevel.ERROR);
		return true;
	}
}
