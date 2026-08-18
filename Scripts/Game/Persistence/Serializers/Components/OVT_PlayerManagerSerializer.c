//------------------------------------------------------------------------------------------------
//! One player's persisted state, keyed by persistent (platform) ID.
//!
//! THE STORED-VS-DERIVED SPLIT IS THE POINT OF THIS CLASS. OVT_PlayerData carries three kinds of
//! member and only one of them belongs in a save:
//!
//!   STORED     - name, home, camp, money, initialized, isOfficer, kills, xp, levelNotified, skills.
//!                These are the campaign's record of the player and are what this record holds.
//!   SESSION    - id (the runtime player ID) and firstSpawn. Both are [NonSerialized()] on
//!                OVT_PlayerData and both are meaningless across sessions; id is re-established by
//!                OVT_PlayerManagerComponent.SetupPlayer() when the player connects.
//!   DERIVED    - priceMultiplier, stealthMultiplier, diplomacy, permissions. Also [NonSerialized()],
//!                and deliberately so: they are OUTPUTS of the skill levels, produced by replaying
//!                every earned skill level's effects. Persisting them would let a save and the skill
//!                config disagree, and a mod update that rebalanced a skill could never reach an
//!                existing campaign. They are rebuilt on load instead - see ApplyPersistedPlayers().
//!
//! Skills travel as parallel key/level arrays rather than as a map so a key the skill config no
//! longer defines can be dropped one entry at a time.
//!
//! FIELD ORDER IS THE FORMAT. Reflection writes and reads these members in declaration order, and the
//! save context is binary, so a new field may only ever be APPENDED - never inserted, never reordered,
//! never removed. bodyPersistenceId is the version 2 addition and therefore sits last.
//------------------------------------------------------------------------------------------------
class OVT_PersistedPlayer
{
	string persistentId;
	string name;
	vector home;
	vector camp;
	int money;

	//! Whether this player has been through first-time setup. Load-bearing on restore: the game
	//! mode's FinalizePlayerPreparation() hands out starting cash to players whose record says false.
	bool initialized;

	bool isOfficer;
	int kills;
	int xp;
	int levelNotified;

	ref array<string> skillKeys = {};
	ref array<int> skillLevels = {};

	//! Version 2. Persistence id of the player's stored CHARACTER, so the same body - with the gear it
	//! was carrying - can be spawned back on a continue or a reconnect instead of a fresh one. Empty
	//! when no body has been stored, and empty for a player who was DEAD when the save was taken
	//! (death clears it - see OVT_PlayerData.m_sBodyPersistenceId).
	string bodyPersistenceId;

	//! Version 3. Where the body above was standing, and which way it faced. Zero means "no stored
	//! position". This is the half that does NOT depend on the character record surviving: when the
	//! body cannot be spawned back, the player is still rebuilt here rather than at their home.
	vector lastKnownPosition;
	vector lastKnownAngles;

	//! Version 4. Tutorial entry ids this player has dismissed. Moved into the campaign save from the
	//! per-machine profile (2026-08-18) - see OVT_PlayerData.m_aSeenTutorials for the reasoning.
	ref array<string> seenTutorialIds = {};

	//! Version 4. The player's "Don't show tips again" choice.
	bool tutorialsDisabled;
}

//------------------------------------------------------------------------------------------------
//! Persists the per-player campaign records held by OVT_PlayerManagerComponent.
//!
//! BINDING. Listed in the ComponentSerializers block of the game-mode configuration in
//! Configs/Systems/Persistence/Overthrow.conf.
//!
//! THIS IS WHERE PLAYER MONEY AND XP LIVE. OVT_EconomyManagerComponent.AddPlayerMoney() and the
//! skill manager's GiveXP()/AddSkillLevel() all write OVT_PlayerData, which the PLAYER manager owns -
//! the economy serializer only carries the resistance treasury. Read OVT_EconomyManagerSerializer's
//! header before assuming otherwise.
//!
//! SCOPE: RECORDS, PLUS A POINTER TO THE BODY. A player is two things - a campaign record (money, XP,
//! home, skills) and a character entity in the world. This serializer covers the record. The BODY is
//! persisted by VANILLA: a player character is an ordinary tracked character (Character_Base.et carries
//! the native Persistence component) and vanilla's character configuration already serializes it with
//! its whole inventory. What this record adds is the body's persistence id, which is what lets
//! OVT_SpawnLogic ask for that exact body back (PersistenceSystem.RequestSpawn) on the next spawn
//! rather than build a fresh one - so carried equipment survives a quit/continue and a reconnect.
//! It does NOT survive death: the id is cleared when the player is killed.
//!
//! VALIDATION ON SAVE mirrors EPF's OVT_PlayerSaveData.ReadFrom(): records with an empty ID, and
//! records that were never initialised (no name, not initialized, no money), are skipped rather than
//! written. Those are placeholder entries created by a connection that never completed setup, and
//! writing them would resurrect ghosts on every load.
//!
//! POST-LOAD. Applying a record fires m_OnPlayerDataLoaded, which is what EPF did and what
//! OVT_SkillManagerComponent subscribes to in order to replay skill effects onto the derived fields.
//! No RPC: deserialization happens while the world is still being built, and every client - joining
//! or rejoining - receives the whole player table through the component's own RplSave/RplLoad JIP
//! payload (OVT_PlayerManagerComponent.c:381-457).
//!
//! IDEMPOTENT ON A LIVE SESSION. Deserialize also runs when saved data is re-applied to a running
//! campaign (OVT_PersistenceManagerComponent.ReapplyLatestSaveData). ApplyPersistedPlayers() updates
//! the EXISTING record object in place rather than replacing it (so nothing already holding a
//! reference - or the runtime player ID inside it - is invalidated) and resets the derived fields
//! before replaying skill effects, so a second pass cannot double-apply them.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first.
//!
//! VERSION HISTORY.
//!   1 - player records (identity, home/camp, money, officer/init flags, progression, skills)
//!   2 - adds OVT_PersistedPlayer.bodyPersistenceId, appended after the skill arrays
//!   3 - adds lastKnownPosition/lastKnownAngles, appended after the body id, so a player comes back
//!       where they logged out even when their stored body cannot be found (2026-08-04 dedicated
//!       play-test: the body answered NOT_FOUND after a server restart and the player woke up at home)
//!   4 - adds seenTutorialIds/tutorialsDisabled, appended after the transform: tutorial progress moved
//!       into the campaign save from the unreliable per-machine profile (2026-08-18, user decision -
//!       tips were re-showing on every load; a new campaign showing them again is accepted)
//------------------------------------------------------------------------------------------------
class OVT_PlayerManagerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_PlayerManagerComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes one record per initialised player.
	//! \param[in] owner The game mode entity owning the player manager.
	//! \param[in] component The player manager being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow player manager.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_PlayerManagerComponent players = OVT_PlayerManagerComponent.Cast(component);
		if (!players)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 4);

		array<ref OVT_PersistedPlayer> records = new array<ref OVT_PersistedPlayer>();

		if (players.m_mPlayers)
		{
			for (int i = 0; i < players.m_mPlayers.Count(); i++)
			{
				string persistentId = players.m_mPlayers.GetKey(i);
				OVT_PlayerData player = players.m_mPlayers.GetElement(i);

				if (persistentId == "")
					continue;

				if (!player)
					continue;

				// A connection that never finished setup leaves an empty placeholder behind. Saving it
				// would recreate it on every load, forever.
				if (player.name.IsEmpty() && !player.initialized && player.money == 0)
					continue;

				OVT_PersistedPlayer record = new OVT_PersistedPlayer();
				record.persistentId = persistentId;
				record.name = player.name;
				record.home = player.home;
				record.camp = player.camp;
				record.money = player.money;
				record.initialized = player.initialized;
				record.isOfficer = player.isOfficer;
				record.kills = player.kills;
				record.xp = player.xp;
				record.levelNotified = player.levelNotified;

				// Version 2. The player manager keeps this current - it captures the id when a body is
				// handed over, refreshes it in SyncPlayerBodyIds() (which
				// OVT_OverthrowGameMode.PreShutdownPersist runs immediately before this serializer) and
				// clears it on death. A codec must not go looking for the body itself.
				record.bodyPersistenceId = player.m_sBodyPersistenceId;

				// Version 3. Kept current by the same three writers as the id above, and deliberately
				// written even when the id could not be captured.
				record.lastKnownPosition = player.m_vLastKnownPosition;
				record.lastKnownAngles = player.m_vLastKnownAngles;

				// Version 4. The tutorial component's server half keeps these current as the owning
				// client dismisses tips; a null array on a record that predates the field writes empty.
				if (player.m_aSeenTutorials)
					record.seenTutorialIds.Copy(player.m_aSeenTutorials);
				record.tutorialsDisabled = player.m_bTutorialsDisabled;

				if (player.skills)
				{
					for (int s = 0; s < player.skills.Count(); s++)
					{
						string key = player.skills.GetKey(s);
						if (key == "")
							continue;

						record.skillKeys.Insert(key);
						record.skillLevels.Insert(player.skills.GetElement(s));
					}
				}

				records.Insert(record);
			}
		}

		context.Write(records);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the player records back and hands them to the manager to apply.
	//! \param[in] owner The game mode entity owning the player manager.
	//! \param[in] component The player manager being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_PlayerManagerComponent players = OVT_PlayerManagerComponent.Cast(component);
		if (!players)
			return false;

		// See OVT_TownManagerSerializer.Deserialize() - no version means no payload for this component.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		array<ref OVT_PersistedPlayer> records = new array<ref OVT_PersistedPlayer>();
		context.Read(records);

		// A version 1 payload was written before the record carried a body id. The field is the LAST
		// one declared, so an older payload simply has no data for it - but whatever the reader left
		// there is not ours, so it is cleared rather than trusted. Players from such a save get a fresh
		// civilian body, which is exactly what version 1 promised.
		if (version < 2)
			ClearBodyPersistenceIds(records);

		// Same reasoning one version on: a version 2 payload has no data for the appended transform, so
		// whatever the reader left in those members is not ours. Zeroing it makes such a player fall
		// through to their home, which is what version 2 promised.
		if (version < 3)
			ClearLastKnownTransforms(records);

		// And again for version 4: an older payload carries no tutorial state, and "no state" means
		// every tip shows again - which is exactly what such a save's players were seeing anyway.
		if (version < 4)
			ClearTutorialState(records);

		players.ApplyPersistedPlayers(records);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Blanks the version 2 body id on every record of an older payload.
	//! \param[in] records Records just read, may be null.
	protected void ClearBodyPersistenceIds(array<ref OVT_PersistedPlayer> records)
	{
		if (!records)
			return;

		foreach (OVT_PersistedPlayer record : records)
		{
			if (record)
				record.bodyPersistenceId = "";
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Blanks the version 4 tutorial state on every record of an older payload.
	//! \param[in] records Records just read, may be null.
	protected void ClearTutorialState(array<ref OVT_PersistedPlayer> records)
	{
		if (!records)
			return;

		foreach (OVT_PersistedPlayer record : records)
		{
			if (record)
			{
				if (record.seenTutorialIds)
					record.seenTutorialIds.Clear();
				record.tutorialsDisabled = false;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Blanks the version 3 transform on every record of an older payload.
	//! \param[in] records Records just read, may be null.
	protected void ClearLastKnownTransforms(array<ref OVT_PersistedPlayer> records)
	{
		if (!records)
			return;

		foreach (OVT_PersistedPlayer record : records)
		{
			if (record)
			{
				record.lastKnownPosition = vector.Zero;
				record.lastKnownAngles = vector.Zero;
			}
		}
	}
}
