//------------------------------------------------------------------------------------------------
//! One recruit's persisted record.
//!
//! m_bIsOnline IS ABSENT ON PURPOSE. It does not describe the recruit, it describes whether their
//! BODY is currently spawned in the world - a fact about this session, not about the campaign. On a
//! restore no body exists yet, so a stored "true" would be a lie the rest of the mod acts on:
//! GetPlayerRecruitsInRadius() and the recruit UI both filter on it.
//!
//! The last known position IS stored, because it is where a REBUILT body is placed. It is not where a
//! RESTORED one appears - a body spawned back from storage arrives at the transform stored with it -
//! but the two are the same spot, and the position is what the fresh-body fallback has to go on.
//!
//! bodyPersistenceId is what turns "a recruit" back into "that recruit": when the owning player is
//! back in the world and has a group, OVT_RecruitManagerComponent.SpawnRecruitBody() asks the
//! persistence system for that stored character and links it to this record.
//!
//! FIELD ORDER IS THE FORMAT. Reflection writes and reads these members in declaration order, and the
//! save context is binary, so a new field may only ever be APPENDED - never inserted, never reordered,
//! never removed. inactive is the version 3 addition and therefore sits last.
//------------------------------------------------------------------------------------------------
class OVT_PersistedRecruit
{
	string recruitId;
	string ownerPersistentId;
	string name;
	int kills;
	int xp;
	int level;
	bool isTraining;
	float trainingCompleteTime;
	vector lastKnownPosition;
	int townId;

	ref array<string> skillKeys = {};
	ref array<int> skillLevels = {};

	//! Version 2. Persistence id of the recruit's stored BODY, so the same character - with the gear it
	//! was carrying - can be spawned back instead of a fresh one. Empty when no body has been stored.
	string bodyPersistenceId;

	//! Version 3. Whether the recruit was left INACTIVE - owned, but out of its owner's group, holding
	//! the position it was parked at. A campaign fact the player chose, not a fact about this session,
	//! which is why it is stored where m_bIsOnline deliberately is not.
	bool inactive;
}

//------------------------------------------------------------------------------------------------
//! Persists the recruit records held by OVT_RecruitManagerComponent.
//!
//! BINDING. Listed in the ComponentSerializers block of the game-mode configuration in
//! Configs/Systems/Persistence/Overthrow.conf.
//!
//! SCOPE: RECORDS, PLUS A POINTER TO THE BODY. A recruit is two things - a record (who they are, who
//! owns them, what they have earned) and a character entity in the world. This serializer covers the
//! record. The BODY is persisted by VANILLA, not here: a recruit body is an ordinary tracked character
//! and vanilla's AI-character configuration already serializes it with its whole inventory. What this
//! record adds is the body's persistence id, which is what lets the manager ask for that exact body
//! back (PersistenceSystem.RequestSpawn) rather than build a new one - so carried equipment survives.
//!
//! A recruit body only exists while its owner is in the game (it is released OFFLINE_DESPAWN_TIME
//! after they leave), so a restored recruit still comes back offline and gets its body the moment its
//! owner returns - see OVT_RecruitManagerComponent.RespawnPlayerRecruits().
//!
//! THE OWNER INDEX IS DERIVED, NOT STORED. EPF persisted m_mRecruitsByOwner alongside the recruits
//! and could therefore load a save whose two halves disagreed. Every record already names its owner,
//! so the index is rebuilt from the records themselves and cannot desync.
//!
//! POST-LOAD. No RPC, deliberately. Deserialization happens while the world is still being built,
//! and every client - joining or rejoining - receives the whole recruit table through the
//! component's own RplSave/RplLoad JIP payload (OVT_RecruitManagerComponent.c:1290-1426). The
//! per-recruit broadcast RPCs exist for live changes, which this is not.
//!
//! IDEMPOTENT ON A LIVE SESSION. Deserialize also runs when saved data is re-applied to a running
//! campaign (OVT_PersistenceManagerComponent.ReapplyLatestSaveData). ApplyPersistedRecruits() fills
//! the EXISTING record when there is one - preserving its live online flag, its entity mapping and any
//! body id it already holds - and never inserts a duplicate id into an owner's list.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first.
//!
//! VERSION HISTORY.
//!   1 - recruit records (identity, progression, skills, last known position, hometown)
//!   2 - adds OVT_PersistedRecruit.bodyPersistenceId, appended after the skill arrays
//!   3 - adds OVT_PersistedRecruit.inactive, appended after bodyPersistenceId. A version 1 or 2 save
//!       has no inactive recruits by definition (the state did not exist), so those recruits load
//!       back ACTIVE - see ClearInactiveFlags() below.
//------------------------------------------------------------------------------------------------
class OVT_RecruitManagerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_RecruitManagerComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes one record per recruit.
	//! \param[in] owner The game mode entity owning the recruit manager.
	//! \param[in] component The recruit manager being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow recruit manager.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.Cast(component);
		if (!recruits)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 3);

		array<ref OVT_PersistedRecruit> records = new array<ref OVT_PersistedRecruit>();

		if (recruits.m_mRecruits)
		{
			for (int i = 0; i < recruits.m_mRecruits.Count(); i++)
			{
				string recruitId = recruits.m_mRecruits.GetKey(i);
				OVT_RecruitData recruit = recruits.m_mRecruits.GetElement(i);

				if (recruitId == "")
					continue;

				if (!recruit)
					continue;

				// A recruit with no owner cannot be given back to anybody on load.
				if (recruit.m_sOwnerPersistentId == "")
					continue;

				OVT_PersistedRecruit record = new OVT_PersistedRecruit();
				record.recruitId = recruitId;
				record.ownerPersistentId = recruit.m_sOwnerPersistentId;
				record.name = recruit.m_sName;
				record.kills = recruit.m_iKills;
				record.xp = recruit.m_iXP;
				record.level = recruit.m_iLevel;
				record.isTraining = recruit.m_bIsTraining;
				record.trainingCompleteTime = recruit.m_fTrainingCompleteTime;
				record.lastKnownPosition = recruit.m_vLastKnownPosition;
				record.townId = recruit.m_iTownId;

				// Version 2. The manager keeps this current - it captures the id when a body is attached
				// and refreshes it in SyncRecruitPositions(), which OVT_OverthrowGameMode.PreShutdownPersist
				// runs immediately before this serializer. A codec must not go looking for the body itself.
				record.bodyPersistenceId = recruit.m_sBodyPersistenceId;

				// Version 3. Written LAST in the record, because field order is the format.
				record.inactive = recruit.m_bInactive;

				if (recruit.m_mSkills)
				{
					for (int s = 0; s < recruit.m_mSkills.Count(); s++)
					{
						string key = recruit.m_mSkills.GetKey(s);
						if (key == "")
							continue;

						record.skillKeys.Insert(key);
						record.skillLevels.Insert(recruit.m_mSkills.GetElement(s));
					}
				}

				records.Insert(record);
			}
		}

		context.Write(records);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the recruit records back and hands them to the manager to apply.
	//! \param[in] owner The game mode entity owning the recruit manager.
	//! \param[in] component The recruit manager being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.Cast(component);
		if (!recruits)
			return false;

		// See OVT_TownManagerSerializer.Deserialize() - no version means no payload for this component.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		array<ref OVT_PersistedRecruit> records = new array<ref OVT_PersistedRecruit>();
		context.Read(records);

		// A version 1 payload was written before the record carried a body id. The field is the LAST
		// one declared, so an older payload simply has no data for it - but whatever the reader left
		// there is not ours, so it is cleared rather than trusted. Recruits from such a save get a
		// fresh civilian body, which is exactly what version 1 promised.
		if (version < 2)
			ClearBodyPersistenceIds(records);

		// Same rule for the version 3 field: it is the LAST one declared, so a version 1 or 2 payload
		// carries no data for it - and whatever the reader left there is not ours, so it is cleared
		// rather than trusted. Every recruit from such a save comes back ACTIVE, which is the only
		// state those versions could describe.
		if (version < 3)
			ClearInactiveFlags(records);

		recruits.ApplyPersistedRecruits(records);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Blanks the version 2 body id on every record of an older payload.
	//! \param[in] records Records just read, may be null.
	protected void ClearBodyPersistenceIds(array<ref OVT_PersistedRecruit> records)
	{
		if (!records)
			return;

		foreach (OVT_PersistedRecruit record : records)
		{
			if (record)
				record.bodyPersistenceId = "";
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Blanks the version 3 inactive flag on every record of an older payload.
	//! \param[in] records Records just read, may be null.
	protected void ClearInactiveFlags(array<ref OVT_PersistedRecruit> records)
	{
		if (!records)
			return;

		foreach (OVT_PersistedRecruit record : records)
		{
			if (record)
				record.inactive = false;
		}
	}
}
