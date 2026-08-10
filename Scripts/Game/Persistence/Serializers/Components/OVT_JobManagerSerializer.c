//------------------------------------------------------------------------------------------------
//! VERSION 1 ONLY. FROZEN. DO NOT EDIT THIS CLASS - NOT ONE FIELD, NOT ONE CHARACTER OF ITS NAME.
//!
//! This is the exact record layout every campaign save written before the stable-id migration
//! carries. It is read by OVT_JobManagerSerializer.DeserializeVersion1() and converted; nothing
//! writes it any more. The CURRENT record is OVT_PersistedJobV2, and that is the one to change.
//!
//! ================================================================================================
//! WHY THE CLASS NAME IS PART OF THE FILE FORMAT - MEASURED, NOT ASSUMED (2026-08-09)
//! ================================================================================================
//! The persistence binary container writes a TYPE DISCRIMINATOR into every object it stores:
//! SCR_PersistenceBinarySaveContext calls EnableTypeDiscriminator(true) (see the base game's
//! Scripts/Game/Plugins/Persistence/System/SCR_PersistenceSerializationContext.c:34-43), so each
//! record in the stream is preceded by `$type` = the concrete class name. It is visible verbatim in
//! a real save blob: `$type\0OVT_PersistedJob\0` heads every job record.
//!
//! On load the container CREATES THE INSTANCE FROM THAT NAME. Reading a version 1 payload into a
//! renamed-but-identical class was measured with an in-memory round trip through
//! SCR_PersistenceBinarySaveContext / SCR_PersistenceBinaryLoadContext, and it does not merely lose
//! the type - IT FAILS THE READ ENTIRELY AND LEAVES THE STREAM UNUSABLE:
//!
//!     write class A, read into array<ref A>  -> ok=true,  3 records, counters intact
//!     write class A, read into array<ref B>  -> ok=FALSE, 0 records, AND every following
//!                                               property (countIndices, countValues) also FALSE
//!
//! (B was a byte-for-byte copy of A with a different name. The reads were ordered B, C, A so that A
//! succeeding LAST rules out "the container can only be read once".)
//!
//! So a shadow class called OVT_PersistedJobV1 would have handed the manager an EMPTY board and
//! EMPTY counter maps for every version 1 save - which ApplyPersistedJobs() would have applied,
//! wiping the job board and every lifetime counter off a live campaign with no error and no crash.
//! The freeze therefore has to be applied to the class the payload actually NAMES. That is this one.
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
//! VERSION 1 ONLY. FROZEN. Same rule and same reason as OVT_PersistedJob above: the name
//! `OVT_PersistedPlayerJobCounts` is written into every version 1 save as the `$type` discriminator
//! and is the only name that can read one back. The current record is OVT_PersistedPlayerJobCountsV2.
//------------------------------------------------------------------------------------------------
class OVT_PersistedPlayerJobCounts
{
	string playerId;

	ref array<int> jobIndices = {};
	ref array<int> counts = {};
}

//------------------------------------------------------------------------------------------------
//! One job board entry's persisted record, version 2. THIS IS THE CURRENT ONE.
//!
//! Mirrors the storable half of OVT_Job, except that the job is named by its STABLE ID
//! (OVT_JobConfig.m_sId) rather than by its position in m_aJobConfigs. A position does not survive a
//! reorder or a trim of the config list: the record stays in range, its stage index stays valid, and
//! it is silently restored onto a DIFFERENT job which then pays out that job's reward. An id either
//! resolves to the job it named or resolves to nothing and is dropped with a log line.
//!
//! `entity` IS ABSENT ON PURPOSE. OVT_Job.entity is an RplId - a replication handle that is only
//! meaningful inside the session that issued it. EPF serialized the whole OVT_Job object, RplId
//! included, so every loaded job came back pointing at whatever entity happened to hold that handle
//! next. A stale handle is strictly worse than no handle, so it is not written; see
//! OVT_JobManagerComponent.ApplyPersistedJobs() for what happens to the one stage that needs it.
//!
//! FIELD ORDER IS THE FORMAT, AND SO IS THE CLASS NAME. Reflection reads and writes these members in
//! declaration order; a new field may only ever be APPENDED. Renaming the class breaks every version
//! 2 save on disk - see the measurement in OVT_PersistedJob's header for exactly how, and how
//! silently.
//------------------------------------------------------------------------------------------------
class OVT_PersistedJobV2
{
	string jobId;
	vector location;
	int townId;
	int baseId;
	int stage;
	string owner;
	bool accepted;

	ref array<string> declined = {};
}

//------------------------------------------------------------------------------------------------
//! How many times one player has been given each job type, version 2. THIS IS THE CURRENT ONE.
//!
//! The live shape is map<string, map<int,int>>; a nested map is not a shape the binary context is
//! known to round-trip, so each player's counts travel as two index-aligned primitive arrays. Same
//! technique as OVT_DeploymentManagerSerializer uses for its faction resource map. The keys are
//! stable job ids for the same reason OVT_PersistedJobV2 uses one: a counter keyed by position caps
//! the wrong job the moment the config list changes.
//------------------------------------------------------------------------------------------------
class OVT_PersistedPlayerJobCountsV2
{
	string playerId;

	ref array<string> jobIds = {};
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
//!
//! ================================================================================================
//! VERSION HISTORY
//! ================================================================================================
//!   1 - job records and counter maps keyed by JOB INDEX, a position in m_aJobConfigs.
//!   2 - the same data keyed by OVT_JobConfig.m_sId, a stable string id. Records moved from
//!       OVT_PersistedJob / OVT_PersistedPlayerJobCounts to OVT_PersistedJobV2 /
//!       OVT_PersistedPlayerJobCountsV2, because the class name is written into the payload and a
//!       version 1 payload can only be read back under the name it was written with (measured - see
//!       OVT_PersistedJob's header).
//!
//! ================================================================================================
//! VERSION 1 SUPPORT POLICY, WITH A CONCRETE REMOVAL TRIGGER
//! ================================================================================================
//! Version 1 support is KEPT until the next save-format-breaking change to this component, and then
//! it goes. Stated as a trigger rather than a date so it cannot rot:
//!
//!     THE NEXT TIME OVT_PersistedJobV2 CHANGES SHAPE, VERSION 1 GOES AND VERSION 2 BECOMES THE
//!     FLOOR: DeserializeVersion1(), OVT_PersistedJob, OVT_PersistedPlayerJobCounts,
//!     LEGACY_V1_JOB_IDS, LegacyIdForIndex() and IsRetiredLegacyId() are all deleted together, and
//!     the guard below becomes `if (version < 2) return true;`.
//!
//! Not "forever by inertia", and not "one release". The reason it is kept at all is asymmetric cost:
//! the version 1 path is ~40 lines of pure mapping and costs one integer comparison on the version 2
//! path, while dropping it early wipes the job board and every lifetime counter off a live campaign
//! SILENTLY - the `version < 1` guard would see a perfectly valid payload and read it as a normal
//! load. The reason it does not stay forever is that a frozen class nobody exercises is a liability;
//! the trigger above is the moment its cost stops being nearly zero.
//------------------------------------------------------------------------------------------------
class OVT_JobManagerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! THE VERSION 1 JOB LIST, FROZEN. THIS TABLE IS HISTORY, NOT CONFIGURATION - NEVER EDIT IT.
	//!
	//! It records what m_aJobConfigs contained when version 1 payloads were being written: twelve
	//! entries, in this order, and their stable ids. It is not a list of the jobs that exist now and
	//! it must not be kept "up to date" with one. Adding a job to the game does NOT add a row here;
	//! removing one does NOT remove a row. A row is only ever right or wrong about the past, and every
	//! row here is right about it.
	//!
	//! Confirmed against Prefabs/GameMode/OVT_OverthrowGameMode.et:26-48 (the m_aJobConfigs block
	//! spans :25-49) and independently against a runtime probe of the Eden campaign, which printed the
	//! same twelve job titles in the same order. The authoritative prose copy lives in
	//! docs/features/new-player-experience/starter-jobs-retirement/context.md.
	//!
	//! Edit this table and every version 1 save silently restores its jobs onto the wrong jobs - which
	//! is the exact failure the stable id was introduced to remove.
	static const ref array<string> LEGACY_V1_JOB_IDS = {
		"assassinate-traitor",    // 0
		"base-recon",             // 1
		"find-gun-dealer",        // 2  RETIRED
		"raise-support",          // 3
		"find-shop",              // 4  RETIRED
		"place-equipment-box",    // 5  RETIRED
		"recruit-a-civilian",     // 6  RETIRED
		"place-a-camp",           // 7  RETIRED
		"propaganda-run",         // 8
		"pirate-radio",           // 9
		"sabotage-radio-tower",   // 10
		"assassinate-officer"     // 11
	};

	//------------------------------------------------------------------------------------------------
	//! The five version 1 jobs that no longer exist. FROZEN, for the same reason as the table above.
	//!
	//! A version 1 record naming one of these is DROPPED rather than restored, and it is dropped
	//! unconditionally - not "if the config happens to be missing". The five .conf files outlive this
	//! change by a phase (they are needed so the conversion can be exercised against live configs
	//! before they go), so a resolve-based test would give a different answer this week from the one
	//! it gives next week. The contract a player's save is held to must not depend on which day the
	//! build was cut.
	static const ref array<string> RETIRED_V1_JOB_IDS = {
		"find-gun-dealer",
		"find-shop",
		"place-equipment-box",
		"recruit-a-civilian",
		"place-a-camp"
	};

	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_JobManagerComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! The stable job id that a version 1 job index named.
	//!
	//! PURE. No world, no manager, no config list - it reads the frozen table and nothing else, which
	//! is what lets it be tested world-free and what makes it answer the same way in ten years.
	//! \param[in] index A job index as written by a version 1 payload.
	//! \return The id that index named, or an empty string when the index is outside the twelve.
	static string LegacyIdForIndex(int index)
	{
		if (index < 0 || index >= LEGACY_V1_JOB_IDS.Count())
			return "";

		return LEGACY_V1_JOB_IDS[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a version 1 job id names one of the five retired starter jobs.
	//!
	//! PURE, for the same reasons as LegacyIdForIndex().
	//! \param[in] jobId A stable job id. An empty id is never retired (it is not an id at all).
	//! \return True when the id names a job that was removed from the game.
	static bool IsRetiredLegacyId(string jobId)
	{
		if (jobId == "")
			return false;

		for (int i = 0; i < RETIRED_V1_JOB_IDS.Count(); i++)
		{
			if (RETIRED_V1_JOB_IDS[i] == jobId)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the active jobs, then the global and per-player job counters. Version 2: every job is
	//! named by its stable id.
	//! \param[in] owner The game mode entity owning the job manager.
	//! \param[in] component The job manager being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow job manager.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_JobManagerComponent jobs = OVT_JobManagerComponent.Cast(component);
		if (!jobs)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 2);

		// Every config with no m_sId is reported ONCE, however many board entries and counter entries
		// it turns out to own - the message already says both are lost.
		ref set<int> reportedIdlessConfigs = new set<int>();

		array<ref OVT_PersistedJobV2> jobRecords = new array<ref OVT_PersistedJobV2>();
		if (jobs.m_aJobs)
		{
			foreach (OVT_Job job : jobs.m_aJobs)
			{
				if (!job)
					continue;

				string jobId = jobs.GetJobIdByIndex(job.jobIndex);
				if (jobId == "")
				{
					ReportIdlessConfig(jobs, job.jobIndex, reportedIdlessConfigs);
					continue;
				}

				jobRecords.Insert(WriteJob(job, jobId));
			}
		}
		context.Write(jobRecords);

		array<string> countIds = new array<string>();
		array<int> countValues = new array<int>();
		if (jobs.m_aJobCounts)
		{
			for (int i = 0; i < jobs.m_aJobCounts.Count(); i++)
			{
				int jobIndex = jobs.m_aJobCounts.GetKey(i);

				string jobId = jobs.GetJobIdByIndex(jobIndex);
				if (jobId == "")
				{
					ReportIdlessConfig(jobs, jobIndex, reportedIdlessConfigs);
					continue;
				}

				countIds.Insert(jobId);
				countValues.Insert(jobs.m_aJobCounts.GetElement(i));
			}
		}
		context.Write(countIds);
		context.Write(countValues);

		array<ref OVT_PersistedPlayerJobCountsV2> playerCounts = new array<ref OVT_PersistedPlayerJobCountsV2>();
		if (jobs.m_mPlayerJobCounts)
		{
			for (int i = 0; i < jobs.m_mPlayerJobCounts.Count(); i++)
			{
				OVT_PersistedPlayerJobCountsV2 record = new OVT_PersistedPlayerJobCountsV2();
				record.playerId = jobs.m_mPlayerJobCounts.GetKey(i);

				map<int, int> counts = jobs.m_mPlayerJobCounts.GetElement(i);
				if (counts)
				{
					for (int c = 0; c < counts.Count(); c++)
					{
						int jobIndex = counts.GetKey(c);

						string jobId = jobs.GetJobIdByIndex(jobIndex);
						if (jobId == "")
						{
							ReportIdlessConfig(jobs, jobIndex, reportedIdlessConfigs);
							continue;
						}

						record.jobIds.Insert(jobId);
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
	//!
	//! Version 1 payloads are read through the FROZEN record classes and converted to version 2
	//! records before they reach the manager, so the manager only ever sees id-keyed data and there is
	//! exactly one apply path.
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

		if (version == 1)
			return DeserializeVersion1(jobs, context);

		// Version 2 and anything newer. A payload from a FUTURE build cannot be read correctly here,
		// and DeserializeVersion2's read guard is what makes that safe: a failed read leaves the live
		// board alone instead of replacing it with an empty one.
		return DeserializeVersion2(jobs, context);
	}

	//------------------------------------------------------------------------------------------------
	//! Reads a version 2 payload - stable ids throughout - and applies it.
	//!
	//! EVERY READ IS CHECKED, AND THAT IS NOT DEFENSIVE PADDING. A failed LoadContext.Read() leaves
	//! the destination array non-null and EMPTY, and ApplyPersistedJobs() applies whatever it is
	//! given: an empty board and empty counter maps would be applied as "this campaign has no jobs and
	//! has never run one". Measured 2026-08-09 in an in-memory round trip: a genuinely empty board
	//! reads back as ok=TRUE with zero records, and ok=FALSE only ever came back from a real read
	//! failure. The two cases are therefore distinguishable, and a read failure aborts.
	//! \param[in] jobs The job manager. Callers null-check before calling.
	//! \param[in] context Load context positioned immediately after the version value.
	//! \return True - the payload was consumed either way; a fault leaves the live board untouched.
	protected bool DeserializeVersion2(notnull OVT_JobManagerComponent jobs, notnull LoadContext context)
	{
		// The local names ARE the property names: LoadContext.Read() derives the key from the variable
		// it is handed, so these have to match what Serialize() called them. Measured 2026-08-09 -
		// writing `jobRecords` and reading the same payload into a local called `readJobs` FAILS.
		array<ref OVT_PersistedJobV2> jobRecords = new array<ref OVT_PersistedJobV2>();
		if (!context.Read(jobRecords))
			return AbortUnreadablePayload(2, "the job board");

		array<string> countIds = new array<string>();
		if (!context.Read(countIds))
			return AbortUnreadablePayload(2, "the global job counter keys");

		array<int> countValues = new array<int>();
		if (!context.Read(countValues))
			return AbortUnreadablePayload(2, "the global job counter values");

		array<ref OVT_PersistedPlayerJobCountsV2> playerCounts = new array<ref OVT_PersistedPlayerJobCountsV2>();
		if (!context.Read(playerCounts))
			return AbortUnreadablePayload(2, "the per-player job counters");

		jobs.ApplyPersistedJobs(jobRecords, countIds, countValues, playerCounts);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads a version 1 payload through the frozen record classes and converts it to version 2.
	//!
	//! THE LOCAL NAMES ARE PART OF THE FORMAT. LoadContext.Read() derives the property name from the
	//! variable it is handed, so these four locals carry the names the version 1 WRITER used -
	//! jobRecords, countIndices, countValues, playerCounts. This was measured on 2026-08-09, not
	//! assumed: reading the same payload into an identically-typed local with a different name
	//! returns false and reads nothing.
	//! \param[in] jobs The job manager. Callers null-check before calling.
	//! \param[in] context Load context positioned immediately after the version value.
	//! \return True - the payload was consumed either way; a fault leaves the live board untouched.
	protected bool DeserializeVersion1(notnull OVT_JobManagerComponent jobs, notnull LoadContext context)
	{
		array<ref OVT_PersistedJob> jobRecords = new array<ref OVT_PersistedJob>();
		if (!context.Read(jobRecords))
			return AbortUnreadablePayload(1, "the job board");

		array<int> countIndices = new array<int>();
		if (!context.Read(countIndices))
			return AbortUnreadablePayload(1, "the global job counter keys");

		array<int> countValues = new array<int>();
		if (!context.Read(countValues))
			return AbortUnreadablePayload(1, "the global job counter values");

		array<ref OVT_PersistedPlayerJobCounts> playerCounts = new array<ref OVT_PersistedPlayerJobCounts>();
		if (!context.Read(playerCounts))
			return AbortUnreadablePayload(1, "the per-player job counters");

		// One WARNING per retired job, not one per record. The message says the job's lifetime counters
		// go with it, so repeating it once for the board entry and again for each counter entry would
		// contradict its own wording and bury the interesting lines.
		ref set<string> reportedRetiredIds = new set<string>();

		array<ref OVT_PersistedJobV2> convertedJobs = new array<ref OVT_PersistedJobV2>();
		foreach (OVT_PersistedJob legacyRecord : jobRecords)
		{
			if (!legacyRecord)
				continue;

			string jobId = ResolveLegacyIndex(legacyRecord.jobIndex, reportedRetiredIds);
			if (jobId == "")
				continue;

			convertedJobs.Insert(ConvertJob(legacyRecord, jobId));
		}

		array<string> convertedCountIds = new array<string>();
		array<int> convertedCountValues = new array<int>();
		int globalPairs = countIndices.Count();
		if (countValues.Count() < globalPairs)
			globalPairs = countValues.Count();

		for (int i = 0; i < globalPairs; i++)
		{
			string jobId = ResolveLegacyIndex(countIndices[i], reportedRetiredIds);
			if (jobId == "")
				continue;

			convertedCountIds.Insert(jobId);
			convertedCountValues.Insert(countValues[i]);
		}

		array<ref OVT_PersistedPlayerJobCountsV2> convertedPlayerCounts = new array<ref OVT_PersistedPlayerJobCountsV2>();
		foreach (OVT_PersistedPlayerJobCounts legacyPlayerRecord : playerCounts)
		{
			if (!legacyPlayerRecord)
				continue;

			OVT_PersistedPlayerJobCountsV2 converted = new OVT_PersistedPlayerJobCountsV2();
			converted.playerId = legacyPlayerRecord.playerId;

			int playerPairs = 0;
			if (legacyPlayerRecord.jobIndices && legacyPlayerRecord.counts)
			{
				playerPairs = legacyPlayerRecord.jobIndices.Count();
				if (legacyPlayerRecord.counts.Count() < playerPairs)
					playerPairs = legacyPlayerRecord.counts.Count();
			}

			for (int i = 0; i < playerPairs; i++)
			{
				string jobId = ResolveLegacyIndex(legacyPlayerRecord.jobIndices[i], reportedRetiredIds);
				if (jobId == "")
					continue;

				converted.jobIds.Insert(jobId);
				converted.counts.Insert(legacyPlayerRecord.counts[i]);
			}

			convertedPlayerCounts.Insert(converted);
		}

		Print(string.Format("[Overthrow] Migrated a version 1 job payload: %1 of %2 board entries and %3 of %4 global counters carried forward",
			convertedJobs.Count(), jobRecords.Count(), convertedCountIds.Count(), globalPairs), LogLevel.NORMAL);

		jobs.ApplyPersistedJobs(convertedJobs, convertedCountIds, convertedCountValues, convertedPlayerCounts);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Turns a version 1 job index into the stable id it named, or reports why it cannot be carried.
	//! \param[in] legacyIndex The index a version 1 payload wrote.
	//! \param[in,out] reportedRetiredIds Retired ids already logged in this load. Never null.
	//! \return The stable id, or an empty string when the entry must be dropped.
	protected string ResolveLegacyIndex(int legacyIndex, notnull set<string> reportedRetiredIds)
	{
		string jobId = LegacyIdForIndex(legacyIndex);
		if (jobId == "")
		{
			Print(string.Format("[Overthrow] Dropping a saved job with legacy index %1 - the version 1 job list had 12 entries", legacyIndex), LogLevel.WARNING);
			return "";
		}

		if (IsRetiredLegacyId(jobId))
		{
			if (!reportedRetiredIds.Contains(jobId))
			{
				reportedRetiredIds.Insert(jobId);
				Print(string.Format("[Overthrow] Dropping saved job '%1' from a version 1 save - that job was retired and no longer exists. Its lifetime counters are dropped with it", jobId), LogLevel.WARNING);
			}

			return "";
		}

		return jobId;
	}

	//------------------------------------------------------------------------------------------------
	//! Copies a frozen version 1 record into a version 2 record, id already resolved.
	//! \param[in] legacyRecord The version 1 record. Callers null-check before calling.
	//! \param[in] jobId The stable id the record's index named. Callers reject empty ids first.
	//! \return A populated version 2 record. Never null.
	protected OVT_PersistedJobV2 ConvertJob(OVT_PersistedJob legacyRecord, string jobId)
	{
		OVT_PersistedJobV2 record = new OVT_PersistedJobV2();
		record.jobId = jobId;
		record.location = legacyRecord.location;
		record.townId = legacyRecord.townId;
		record.baseId = legacyRecord.baseId;
		record.stage = legacyRecord.stage;
		record.owner = legacyRecord.owner;
		record.accepted = legacyRecord.accepted;

		if (legacyRecord.declined)
		{
			foreach (string persistentId : legacyRecord.declined)
			{
				if (persistentId == "")
					continue;

				record.declined.Insert(persistentId);
			}
		}

		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Reports an unreadable payload and declines to touch the board.
	//!
	//! Returning true - "consumed" - is deliberate. The alternative, false, is what a serializer
	//! returns when the payload was not for it, and it does not undo anything; the meaningful choice
	//! here is between applying garbage and applying nothing, and applying nothing keeps whatever the
	//! campaign already had.
	//! \param[in] version The payload version that failed to read.
	//! \param[in] what Which part of the payload could not be read, for the log line.
	//! \return Always true.
	protected bool AbortUnreadablePayload(int version, string what)
	{
		Print(string.Format("[Overthrow] Could not read %1 from a version %2 job payload - the job board and its lifetime counters are left exactly as they are rather than being replaced with nothing. This is a save-format fault: an empty board reads back successfully, so this is not one", what, version), LogLevel.ERROR);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Reports, once, a job config that cannot be saved because it carries no stable id.
	//!
	//! Impossible for shipped content - OVT_TEST_Init_Jobs_StableIdsAreUniqueAndResolve fails the
	//! build if any config's m_sId is empty - but reachable for a third-party job config, and a loud
	//! error beats a job quietly vanishing from every continued campaign.
	//! \param[in] jobs The job manager, for the config's title.
	//! \param[in] jobIndex The position of the offending config in m_aJobConfigs.
	//! \param[in,out] reported Indices already reported in this save. Never null.
	protected void ReportIdlessConfig(notnull OVT_JobManagerComponent jobs, int jobIndex, notnull set<int> reported)
	{
		if (reported.Contains(jobIndex))
			return;

		reported.Insert(jobIndex);

		string title = "(no config at this index)";
		if (jobIndex >= 0 && jobIndex < jobs.GetJobConfigCount())
		{
			OVT_JobConfig config = jobs.GetConfig(jobIndex);
			if (config)
				title = config.m_sTitle;
		}

		Print(string.Format("[Overthrow] Job config '%1' (index %2) has no m_sId - its board entries and lifetime counters cannot be saved", title, jobIndex), LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	//! Copies one live job into a version 2 save record.
	//! \param[in] job The live job. Callers null-check before calling.
	//! \param[in] jobId The job's stable id. Callers reject empty ids first.
	//! \return A populated record. Never null.
	protected OVT_PersistedJobV2 WriteJob(OVT_Job job, string jobId)
	{
		OVT_PersistedJobV2 record = new OVT_PersistedJobV2();
		record.jobId = jobId;
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
