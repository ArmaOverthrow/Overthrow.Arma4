//------------------------------------------------------------------------------------------------
//! One job board entry's persisted record. Mirrors the storable half of OVT_Job.
//!
//! `entity` IS ABSENT ON PURPOSE. OVT_Job.entity is an RplId - a replication handle that is only
//! meaningful inside the session that issued it. EPF serialized the whole OVT_Job object, RplId
//! included, so every loaded job came back pointing at whatever entity happened to hold that handle
//! next. A stale handle is strictly worse than no handle, so it is not written; see
//! OVT_JobManagerComponent.ApplyPersistedJobs() for what happens to the one stage that needs it.
//------------------------------------------------------------------------------------------------
class OVT_PersistedJob
{
	int jobIndex;
	vector location;
	int townId;
	int baseId;
	int stage;
	string owner;
	bool accepted;

	ref array<string> declined = {};
}

//------------------------------------------------------------------------------------------------
//! How many times one player has been given each job type.
//!
//! The live shape is map<string, map<int,int>>; a nested map is not a shape the binary context is
//! known to round-trip, so each player's counts travel as two index-aligned primitive arrays. Same
//! technique as OVT_DeploymentManagerSerializer uses for its faction resource map.
//------------------------------------------------------------------------------------------------
class OVT_PersistedPlayerJobCounts
{
	string playerId;

	ref array<int> jobIndices = {};
	ref array<int> counts = {};
}

//------------------------------------------------------------------------------------------------
//! Persists the job board: the active jobs and the counters that limit how often each may be offered.
//!
//! BINDING. Listed in the ComponentSerializers block of the game-mode configuration in
//! Configs/Systems/Persistence/Overthrow.conf.
//!
//! THIS REPLACES A RE-ENTRANT RESTORE, AND THAT IS THE WHOLE POINT.
//! EPF stored the job table inside OVT_ResistanceSaveData and, for every loaded job, called
//! OVT_JobManagerComponent.RunJobToCurrentStage(), which re-runs OnStart / OnTick / OnEnd for every
//! stage below the current one. Those handlers are not accessors - OVT_SpawnGroupJobStage.OnStart()
//! spawns an AI group, OVT_SpawnCivilianJobStage.OnStart() spawns a civilian. Restoring a job at
//! stage 3 therefore spawned its target all over again, and re-applying the same save to a live
//! session (OVT_PersistenceManagerComponent.ReapplyLatestSaveData) would have done it a second time
//! on top of a job already running.
//!
//! IT WAS ALSO NEVER NEEDED. Every stage a job can actually REST at uses the base
//! OVT_JobStage.OnStart(), which does nothing: OVT_WaitTillJobAcceptedJobStage,
//! OVT_WaitTillPlayerInRangeJobStage, OVT_WaitTillSupportJobStage, OVT_WaitTillDeadJobStage,
//! OVT_HasRecruitJobStage and OVT_PlaceableItemJobStage all override OnTick only. The stages that DO
//! have side effects (spawn group, spawn civilian, find house, get shop / dealer location) every one
//! return false from OnStart, which means "this stage is done, advance" - a job is never parked on
//! one of them. Restoring stage, location and ownership verbatim and running nothing is therefore
//! both idempotent AND behaviourally correct, not a compromise.
//!
//! WHAT IS DERIVED RATHER THAN STORED. m_aGlobalJobs, m_aTownJobs and m_aBaseJobs are occupancy sets
//! - "job type X is already running here". They are rebuilt from the restored job list using the
//! manager's own rules, so they cannot drift out of step with it when a job is dropped on load. EPF
//! stored them separately and had no way to keep the two consistent.
//!
//! WHAT IS STORED BUT NOT DERIVABLE. m_aJobCounts and m_mPlayerJobCounts are lifetime counters that
//! enforce m_iMaxTimes / m_iMaxTimesPlayer. They count jobs that have already finished and left the
//! board, so nothing on the board implies them.
//!
//! POST-LOAD. No RPC. Deserialization runs while the world is still being built; clients receive the
//! board through the manager's JIP RplSave / RplLoad and its normal RpcDo_UpdateJob traffic.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first.
//------------------------------------------------------------------------------------------------
class OVT_JobManagerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_JobManagerComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the active jobs, then the global and per-player job counters.
	//! \param[in] owner The game mode entity owning the job manager.
	//! \param[in] component The job manager being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow job manager.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_JobManagerComponent jobs = OVT_JobManagerComponent.Cast(component);
		if (!jobs)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		array<ref OVT_PersistedJob> jobRecords = new array<ref OVT_PersistedJob>();
		if (jobs.m_aJobs)
		{
			foreach (OVT_Job job : jobs.m_aJobs)
			{
				if (!job)
					continue;

				jobRecords.Insert(WriteJob(job));
			}
		}
		context.Write(jobRecords);

		array<int> countIndices = new array<int>();
		array<int> countValues = new array<int>();
		if (jobs.m_aJobCounts)
		{
			for (int i = 0; i < jobs.m_aJobCounts.Count(); i++)
			{
				countIndices.Insert(jobs.m_aJobCounts.GetKey(i));
				countValues.Insert(jobs.m_aJobCounts.GetElement(i));
			}
		}
		context.Write(countIndices);
		context.Write(countValues);

		array<ref OVT_PersistedPlayerJobCounts> playerCounts = new array<ref OVT_PersistedPlayerJobCounts>();
		if (jobs.m_mPlayerJobCounts)
		{
			for (int i = 0; i < jobs.m_mPlayerJobCounts.Count(); i++)
			{
				OVT_PersistedPlayerJobCounts record = new OVT_PersistedPlayerJobCounts();
				record.playerId = jobs.m_mPlayerJobCounts.GetKey(i);

				map<int, int> counts = jobs.m_mPlayerJobCounts.GetElement(i);
				if (counts)
				{
					for (int c = 0; c < counts.Count(); c++)
					{
						record.jobIndices.Insert(counts.GetKey(c));
						record.counts.Insert(counts.GetElement(c));
					}
				}

				playerCounts.Insert(record);
			}
		}
		context.Write(playerCounts);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the job board back and hands it to the manager to apply.
	//! \param[in] owner The game mode entity owning the job manager.
	//! \param[in] component The job manager being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_JobManagerComponent jobs = OVT_JobManagerComponent.Cast(component);
		if (!jobs)
			return false;

		// No version means no payload - see OVT_TownManagerSerializer.Deserialize(). Without the guard
		// an absent payload would wipe the board and every lifetime counter off a running campaign.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		array<ref OVT_PersistedJob> jobRecords = new array<ref OVT_PersistedJob>();
		context.Read(jobRecords);

		array<int> countIndices = new array<int>();
		context.Read(countIndices);

		array<int> countValues = new array<int>();
		context.Read(countValues);

		array<ref OVT_PersistedPlayerJobCounts> playerCounts = new array<ref OVT_PersistedPlayerJobCounts>();
		context.Read(playerCounts);

		jobs.ApplyPersistedJobs(jobRecords, countIndices, countValues, playerCounts);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Copies one live job into a save record.
	//! \param[in] job The live job. Callers null-check before calling.
	//! \return A populated record. Never null.
	protected OVT_PersistedJob WriteJob(OVT_Job job)
	{
		OVT_PersistedJob record = new OVT_PersistedJob();
		record.jobIndex = job.jobIndex;
		record.location = job.location;
		record.townId = job.townId;
		record.baseId = job.baseId;
		record.stage = job.stage;
		record.owner = job.owner;
		record.accepted = job.accepted;

		if (job.declined)
		{
			foreach (string persistentId : job.declined)
			{
				if (persistentId == "")
					continue;

				record.declined.Insert(persistentId);
			}
		}

		return record;
	}
}
