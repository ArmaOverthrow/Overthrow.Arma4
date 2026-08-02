//------------------------------------------------------------------------------------------------
//! One resistance camp's persisted record. Mirrors the stored half of OVT_CampData.
//!
//! id is absent: it is the entry's INDEX in m_Camps by construction (every insertion site sets it to
//! the array count) and is re-derived on load. persistentId is the stable identity and is what a
//! record is matched back to on a live session.
//------------------------------------------------------------------------------------------------
class OVT_PersistedCamp
{
	string persistentId;
	string name;
	vector location;
	string owner;
	bool isPrivate;

	ref array<ResourceName> garrison = {};
}

//------------------------------------------------------------------------------------------------
//! One resistance FOB's persisted record. Mirrors the stored half of OVT_FOBData.
//------------------------------------------------------------------------------------------------
class OVT_PersistedFOB
{
	string persistentId;
	string name;
	vector location;
	string owner;
	bool isPriority;

	ref array<ResourceName> garrison = {};
}

//------------------------------------------------------------------------------------------------
//! Persists the resistance's own state: which faction the player fights for, and the camps and FOBs
//! they have built.
//!
//! BINDING. Listed in the ComponentSerializers block of the game-mode configuration in
//! Configs/Systems/Persistence/Overthrow.conf.
//!
//! THE PLAYER FACTION KEY IS PART OF THE SAVE for the same reason the occupying faction key is: it
//! is a campaign decision, and every player-faction index elsewhere in the save is relative to it.
//! Unlike EPF's version this does NOT fall back to a hardcoded "FIA" when the stored key is empty -
//! the live configuration already holds a valid faction, and overwriting it with a guess is strictly
//! worse than leaving it alone.
//!
//! SCOPE, AND WHAT IS DELIBERATELY NOT HERE.
//!
//!  * CAMP AND FOB RECORDS are restored (name, position, owner, privacy/priority, garrison prefabs).
//!    Their physical compositions in the world are separate tracked entities and are Phase 3 scope;
//!    garrisons come back because OVT_ResistanceFactionManager.PostGameStart() replays the prefab
//!    lists through SpawnGarrisons(), which is the same path a campaign start takes.
//!
//!  * THE JOB SYSTEM IS DEFERRED TO PHASE 3, and that is a decision, not an oversight. EPF's
//!    OVT_ResistanceSaveData reached across into OVT_JobManagerComponent and stored the whole job
//!    table, then on load called OVT_JobManagerComponent.RunJobToCurrentStage() for every job. That
//!    call is NOT a data restore - it re-executes each job stage, with the side effects each stage
//!    has. Persisted data can now also be re-applied to a LIVE session
//!    (OVT_PersistenceManagerComponent.ReapplyLatestSaveData), where re-executing every running
//!    job's stages would fire them a second time on top of themselves. Jobs need a serializer of
//!    their own, on OVT_JobManagerComponent, with a restore path that is idempotent - not a
//!    cross-manager copy of a re-entrant one. Until then a continued campaign starts with a fresh
//!    job board.
//!
//! POST-LOAD. No RPC, deliberately. Deserialization happens while the world is still being built,
//! and clients receive camps and FOBs through the manager's normal replication.
//!
//! IDEMPOTENT ON A LIVE SESSION. Records are matched by persistent id (falling back to position) and
//! updated in place; only genuinely missing ones are inserted. EPF's version inserted
//! unconditionally, which would duplicate every camp in the campaign on a re-application.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first.
//------------------------------------------------------------------------------------------------
class OVT_ResistanceManagerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_ResistanceFactionManager;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the player faction key, then the camps and FOBs.
	//! \param[in] owner The game mode entity owning the resistance manager.
	//! \param[in] component The resistance manager being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow resistance manager.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_ResistanceFactionManager resistance = OVT_ResistanceFactionManager.Cast(component);
		if (!resistance)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		string playerFactionKey;
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (config)
			playerFactionKey = config.m_sPlayerFaction;
		context.Write(playerFactionKey);

		array<ref OVT_PersistedCamp> camps = new array<ref OVT_PersistedCamp>();
		if (resistance.m_Camps)
		{
			foreach (OVT_CampData camp : resistance.m_Camps)
			{
				if (!camp)
					continue;

				OVT_PersistedCamp record = new OVT_PersistedCamp();
				record.persistentId = camp.persistentId;
				record.name = camp.name;
				record.location = camp.location;
				record.owner = camp.owner;
				record.isPrivate = camp.isPrivate;
				WriteGarrison(camp.garrisonEntities, camp.garrison, record.garrison);

				camps.Insert(record);
			}
		}
		context.Write(camps);

		array<ref OVT_PersistedFOB> fobs = new array<ref OVT_PersistedFOB>();
		if (resistance.m_FOBs)
		{
			foreach (OVT_FOBData fob : resistance.m_FOBs)
			{
				if (!fob)
					continue;

				OVT_PersistedFOB record = new OVT_PersistedFOB();
				record.persistentId = fob.persistentId;
				record.name = fob.name;
				record.location = fob.location;
				record.owner = fob.owner;
				record.isPriority = fob.isPriority;
				WriteGarrison(fob.garrisonEntities, fob.garrison, record.garrison);

				fobs.Insert(record);
			}
		}
		context.Write(fobs);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the resistance state back and hands it to the manager to apply.
	//! \param[in] owner The game mode entity owning the resistance manager.
	//! \param[in] component The resistance manager being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_ResistanceFactionManager resistance = OVT_ResistanceFactionManager.Cast(component);
		if (!resistance)
			return false;

		// See OVT_TownManagerSerializer.Deserialize() - no version means no payload for this component.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		string playerFactionKey;
		context.Read(playerFactionKey);

		array<ref OVT_PersistedCamp> camps = new array<ref OVT_PersistedCamp>();
		context.Read(camps);

		array<ref OVT_PersistedFOB> fobs = new array<ref OVT_PersistedFOB>();
		context.Read(fobs);

		resistance.ApplyPersistedResistance(playerFactionKey, camps, fobs);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Snapshots a garrison into prefab names.
	//!
	//! Prefers the LIVE groups when there are any - an empty group is one that has been wiped out and
	//! must not be brought back - and falls back to the prefab list already on the record, which is
	//! what a camp that has not spawned its garrison yet carries.
	//! \param[in] garrisonEntities Live AI group entity ids, may be null.
	//! \param[in] storedGarrison The prefab list already on the live record, may be null.
	//! \param[out] target Receives the prefab names to persist.
	protected void WriteGarrison(array<ref EntityID> garrisonEntities, array<ref ResourceName> storedGarrison, array<ResourceName> target)
	{
		if (!target)
			return;

		bool foundLiveGroup = false;

		if (garrisonEntities)
		{
			foreach (EntityID groupId : garrisonEntities)
			{
				SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(groupId));
				if (!group)
					continue;

				foundLiveGroup = true;

				if (group.GetAgentsCount() < 1)
					continue;

				ResourceName prefab = GetPrefabResourceName(group);
				if (prefab.IsEmpty())
					continue;

				target.Insert(prefab);
			}
		}

		if (foundLiveGroup || !storedGarrison)
			return;

		foreach (ResourceName prefab : storedGarrison)
		{
			if (prefab.IsEmpty())
				continue;

			target.Insert(prefab);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Gets the prefab an entity was spawned from, resolving prefab inheritance the way the engine
	//! does. Same implementation as OVT_Global.GetPrefabName(), kept local so this serializer has no
	//! dependency outside the persistence layer.
	//! \param[in] entity The entity to look up.
	//! \return Its prefab resource name, or an empty string.
	protected ResourceName GetPrefabResourceName(IEntity entity)
	{
		if (!entity)
			return ResourceName.Empty;

		EntityPrefabData prefabData = entity.GetPrefabData();
		if (!prefabData)
			return ResourceName.Empty;

		return SCR_BaseContainerTools.GetPrefabResourceName(prefabData.GetPrefab());
	}
}
