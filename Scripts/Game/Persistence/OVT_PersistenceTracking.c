//------------------------------------------------------------------------------------------------
//! Registers entities that Overthrow spawns at runtime with the vanilla persistence system.
//!
//! WHY THIS EXISTS. An instance is only saved if the system has been told to TRACK it. There are
//! exactly three ways to get tracked (vanilla-api-reference.md 1.4):
//!
//!   1. the native `Persistence` component sitting on the prefab,
//!   2. PersistenceSystem.StartTracking() from script,
//!   3. a conf-declared PersistentState.
//!
//! Overthrow spawns its world objects from prefabs it does not own (vanilla props, signs, sandbags,
//! compositions) or from prefabs that predate this migration, so route 1 is not available without
//! editing dozens of prefabs. Vanilla hits the same wall and answers it the same way - see
//! SCR_EditableEntityComponent.c:2266-2271, "Register the various entities from catalog that we do
//! not want to put persistence component on all the base prefabs manually."
//!
//! WHAT TRACKING DOES NOT DO. It does not choose a serializer. The PersistenceConfig an instance
//! gets is decided entirely by the RULES in Configs/Systems/Persistence/Overthrow.conf and the
//! vanilla confs it inherits. Tracking an entity no rule matches is harmless and simply stores
//! nothing - which is why calling this on every spawn of a mixed prefab list is safe.
//!
//! SERVER ONLY, WITHOUT A SERVER CHECK. The persistence system is registered SystemLocation Server,
//! so GetByEntityWorld() returns null on a client and every call here becomes a no-op. Callers do
//! not need to guard.
//------------------------------------------------------------------------------------------------
class OVT_PersistenceTracking
{
	//------------------------------------------------------------------------------------------------
	//! Makes an entity known to the persistence system so it is included in save points.
	//!
	//! Safe to call more than once on the same entity: the system ignores instances it already
	//! tracks. Registration is LAZY (the vanilla default), so the cost at the spawn site is a lookup
	//! and a flag, not serialization work.
	//! \param[in] entity The freshly spawned entity. Null is tolerated and reported as not tracked.
	//! \return True when the entity is now tracked; false on clients, in worlds with no persistence
	//! system, or when the entity was null.
	static bool Track(IEntity entity)
	{
		if (!entity)
			return false;

		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetByEntityWorld(entity);
		if (!persistence)
			return false;

		return persistence.StartTracking(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the persistence system currently tracks an entity.
	//!
	//! Ask this before Track() when an entity may ALREADY be registered by another route - a prefab
	//! carrying the native Persistence component, or an instance the system itself just spawned from
	//! stored data (PersistenceSystem.RequestSpawn). Re-registering is a no-op in the engine, but the
	//! question is what makes the intent legible at the call site.
	//! \param[in] entity The entity to ask about.
	//! \return True when the entity is tracked; false on clients, in worlds with no persistence system,
	//! or when the entity was null.
	static bool IsTracked(IEntity entity)
	{
		if (!entity)
			return false;

		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetByEntityWorld(entity);
		if (!persistence)
			return false;

		return persistence.IsTracked(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! The persistent identity of a tracked entity, as a string usable as a map key.
	//!
	//! This replaces the per-entity id the old persistence framework's component handed out. The id only
	//! exists while the system tracks the entity, so an untracked entity, a client, or a world with no
	//! persistence system all answer with an empty string - callers MUST treat that as "not manageable"
	//! rather than as a key, because a null UUID still stringifies to a zero-filled value that every
	//! untracked entity would share.
	//! \param[in] entity The entity to identify.
	//! \return Its persistent id, or an empty string.
	static string GetPersistentId(IEntity entity)
	{
		if (!entity)
			return string.Empty;

		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetByEntityWorld(entity);
		if (!persistence)
			return string.Empty;

		UUID id = persistence.GetId(entity);
		if (id.IsNull())
			return string.Empty;

		string persistentId = id;
		return persistentId;
	}

	//------------------------------------------------------------------------------------------------
	//! Re-evaluates the configuration rules for an already tracked entity.
	//!
	//! An instance is matched to a PersistenceConfig ONCE, when it starts being tracked. When the fact a
	//! rule tests changes afterwards - a player stops controlling something - the matched configuration
	//! is stale until somebody asks for a re-match. This is that ask. Vanilla uses it the same way when
	//! possession changes (SCR_SpawnLogic.c:167-173).
	//!
	//! TWO WARNINGS. Only NATIVE conf-bound rules are re-evaluated - the engine never consults a
	//! script-defined rule (measured; see MarkForSelfSpawn), so a re-match can only ever land on what
	//! the .conf files declare. And a re-match RESETS any scripted config applied via SetConfig, so
	//! never call this on a corpse MarkForSelfSpawn() has marked.
	//!
	//! Untracked instances are ignored rather than passed through: they have no configuration to reload,
	//! and asking anyway is a needless native call on every entity a caller sweeps over.
	//! \param[in] entity The entity to re-match.
	//! \return True when the entity was tracked and its configuration has been re-evaluated; false on
	//! clients, in worlds with no persistence system, for untracked entities, or when the entity was null.
	static bool ReloadConfig(IEntity entity)
	{
		if (!entity)
			return false;

		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetByEntityWorld(entity);
		if (!persistence)
			return false;

		if (!persistence.IsTracked(entity))
			return false;

		return persistence.ReloadConfig(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! DO NOT USE - this mechanism does not work, and using it corrupts the save. Left in place only so
	//! the finding is not rediscovered a fourth time; it has no callers.
	//!
	//! MEASURED 2026-08-04 by decoding save blobs directly: SetConfig() marks the instance's
	//! configuration as SCRIPTED, and a scripted configuration is serialized with an EMPTY store name,
	//! with the config inlined instead. The loader resolves configurations BY store name, so every such
	//! record fails on load with "Unable to locate configuruation ''" followed by "Attempted to
	//! deserialize meta data without configuration for id:<uuid>", and the instance is dropped. Across
	//! eleven save files: 17 scripted records, 17 empty store names, zero readable. Vanilla contains no
	//! GetConfig/SetConfig call site anywhere in its scripts, so this engine path is untested by BI.
	//!
	//! THE WORKING MECHANISM IS `SelfSpawn` DECLARED IN A .conf (see Configs/Systems/Persistence/
	//! Overthrow.conf), which is what vanilla itself uses - Mission.conf re-enables SelfSpawn on the
	//! player-character config exactly this way.
	//!
	//! Marks a tracked entity's persistence configuration to spawn the entity back on load.
	//!
	//! WHY THIS EXISTS (BUG-018). The corpse-persistence design first shipped as a script-defined
	//! PersistenceConfigRule subclass bound in Overthrow.conf, matching dead characters at high
	//! priority. MEASURED RESULT: the engine never consults a script-defined rule - its IsMatch was
	//! called zero times across world load, initial tracking of every character, and 300 forced
	//! ReloadConfig re-matches (OVT_TEST_Init_Persistence_DeadCharacterConfigSelfSpawns, 2026-08-03).
	//! Every rule vanilla itself binds in a .conf is a NATIVE generated class; there is no vanilla
	//! precedent for a scripted one, and the scripted IsMatch event is dead code on the conf path.
	//!
	//! So the self-spawn decision is made HERE, through the API pair vanilla exposes for exactly this
	//! kind of runtime adjustment: GetConfig() hands back the instance's matched configuration,
	//! m_bSelfSpawn is flipped, SetConfig() applies it. Everything else about the matched config -
	//! collection, serializers, parent handling, self-delete - is deliberately kept, so the record is
	//! written exactly as it always was, plus the one bit that makes it come back.
	//!
	//! CAVEAT, LOAD-BEARING: PersistenceSystem.ReloadConfig() RESETS a scripted config back to the
	//! rule-matched one (vanilla API note). Do not call ReloadConfig on an entity after marking it,
	//! or the mark is silently lost.
	//! \param[in] entity The tracked entity that must come back on load.
	//! \return True when the configuration now self-spawns; false on clients, in worlds with no
	//! persistence system, for untracked entities, or when the entity was null.
	static bool MarkForSelfSpawn(IEntity entity)
	{
		if (!entity)
			return false;

		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetByEntityWorld(entity);
		if (!persistence)
			return false;

		if (!persistence.IsTracked(entity))
			return false;

		EntityPersistenceConfig config = EntityPersistenceConfig.Cast(persistence.GetConfig(entity));
		if (!config)
			return false;

		if (config.m_bSelfSpawn)
			return true;

		config.m_bSelfSpawn = true;
		return persistence.SetConfig(entity, config);
	}

	//------------------------------------------------------------------------------------------------
	//! Writes one instance's record now, without taking a save point.
	//!
	//! PersistenceSystem.Save() takes a SINGLE instance - it is not a global save (that is
	//! OVT_PersistenceManagerComponent.SaveGame()). Use it before deliberately removing an entity whose
	//! data should outlive it.
	//! \param[in] entity The entity to write.
	//! \return True when the write was accepted.
	static bool Save(IEntity entity)
	{
		if (!entity)
			return false;

		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetByEntityWorld(entity);
		if (!persistence)
			return false;

		return persistence.Save(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Stops tracking an entity, optionally keeping its stored record.
	//!
	//! `keepData = true` is the vanilla "despawn but do not forget" idiom - SCR_SpawnLogic.c:107-108 and
	//! :215-216 use exactly this before a disconnecting player's controller and character are destroyed,
	//! so their record still lands in the next save point. Deleting a tracked entity WITHOUT this drops
	//! its record as well (SelfDelete defaults to on).
	//! \param[in] entity The entity to release.
	//! \param[in] keepData True to leave the stored record in place.
	//! \return True when the entity was being tracked and has now been released.
	static bool Untrack(IEntity entity, bool keepData)
	{
		if (!entity)
			return false;

		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetByEntityWorld(entity);
		if (!persistence)
			return false;

		bool removeData = true;
		if (keepData)
			removeData = false;

		return persistence.StopTracking(entity, removeData);
	}
}
