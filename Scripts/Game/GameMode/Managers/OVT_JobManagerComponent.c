//------------------------------------------------------------------------------------------------
//! Manages game jobs, including their configuration, lifecycle, assignment, and state tracking across towns and bases.
//! Handles job creation based on conditions, player acceptance/declination, stage progression, rewards, and replication.
class OVT_JobManagerComponentClass: OVT_ComponentClass
{
};

//------------------------------------------------------------------------------------------------
//! Represents an active job and its current state within the game world.
//! Stores information about the job type, location, associated town or base, current stage, assigned entity (if any),
//! owner player, acceptance status, and players who have declined it.
class OVT_Job
{
	int jobIndex; //!< Index referencing the OVT_JobConfig in the manager's list.
	vector location; //!< World position relevant to the current job stage.
	int townId = -1; //!< ID of the town associated with this job, or -1 if none.
	int baseId = -1; //!< ID of the base associated with this job, or -1 if none.
	int stage; //!< Current stage index of the job progression.
	RplId entity; //!< Replication ID of an entity associated with the job (e.g., a target vehicle).
	string owner; //!< Persistent ID of the player who owns/accepted the job. Empty if unassigned or public.
	bool accepted; //!< Flag indicating if the job has been accepted by a player.
	ref array<string> declined = {}; //!< List of persistent IDs of players who have declined this job.
	bool failed; //!< Flag indicating if the job has failed.

	//------------------------------------------------------------------------------------------------
	//! Retrieves the town data associated with this job.
	//! \return OVT_TownData object for the town, or null if townId is invalid.
	OVT_TownData GetTown()
	{
		return OVT_Global.GetTowns().m_Towns[townId];
	}

	//------------------------------------------------------------------------------------------------
	//! Retrieves the base controller component associated with this job.
	//! \return OVT_BaseControllerComponent object for the base, or null if baseId is invalid.
	OVT_BaseControllerComponent GetBase()
	{
		return OVT_Global.GetOccupyingFaction().GetBaseByIndex(baseId);
	}
}

//------------------------------------------------------------------------------------------------
//! Component responsible for managing all aspects of jobs within the Overthrow game mode.
//! Handles job spawning, tracking, player interaction, stage progression, rewards, and network synchronization.
class OVT_JobManagerComponent: OVT_Component
{
	[Attribute("", UIWidgets.Object)]
	protected ref array<ref OVT_JobConfig> m_aJobConfigs; //!< Array of configurations for available job types.

	ref array<ref OVT_Job> m_aJobs; //!< List of currently active or available jobs.

	ref set<int> m_aGlobalJobs; //!< Set of job indices currently active globally (for GLOBAL_UNIQUE jobs).
	ref map<int, ref set<int>> m_aTownJobs; //!< Map associating town IDs with sets of active job indices in that town.
	ref map<int, ref set<int>> m_aBaseJobs; //!< Map associating base IDs with sets of active job indices at that base.
	ref map<int, int> m_aJobCounts; //!< Map tracking the total count of times each job index has been started.
	ref map<string, ref map<int, int>> m_mPlayerJobCounts; //!< Map tracking how many times each player (by persistent ID) has started each job index.

	protected OVT_TownManagerComponent m_Towns; //!< Reference to the town manager.
	protected OVT_OccupyingFactionManager m_OccupyingFaction; //!< Reference to the occupying faction manager.

	protected const int JOB_FREQUENCY = 10000; //!< Frequency in milliseconds for the job update check timer.

	vector m_vCurrentWaypoint; //!< Waypoint vector, potentially used by job stages (usage seems unclear/incomplete here).

	static OVT_JobManagerComponent s_Instance; //!< Singleton instance.
	//------------------------------------------------------------------------------------------------
	//! Gets the singleton instance of the Job Manager.
	//! \return The static OVT_JobManagerComponent instance.
	static OVT_JobManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_JobManagerComponent.Cast(pGameMode.FindComponent(OVT_JobManagerComponent));
		}

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Constructor for the OVT_JobManagerComponent. Initializes internal collections.
	void OVT_JobManagerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_aGlobalJobs = new set<int>;
		m_aTownJobs = new map<int, ref set<int>>;
		m_aBaseJobs = new map<int, ref set<int>>;
		m_aJobs = new array<ref OVT_Job>;
		m_aJobCounts = new map<int, int>;
		m_mPlayerJobCounts = new map<string, ref map<int, int>>;
	}

	//------------------------------------------------------------------------------------------------
	//! Initializes component references after creation.
	//! \param owner The entity this component is attached to.
	void Init(IEntity owner)
	{
		m_Towns = OVT_Global.GetTowns();
		m_OccupyingFaction = OVT_Global.GetOccupyingFaction();
	}

	//------------------------------------------------------------------------------------------------
	//! Called after the game has started. Sets up periodic update checks.
	void PostGameStart()
	{
		GetGame().GetCallqueue().CallLater(CheckUpdate, 250, false, GetOwner()); // Initial check soon after start
		GetGame().GetCallqueue().CallLater(CheckUpdate, JOB_FREQUENCY, true, GetOwner()); // Periodic check
	}

	//------------------------------------------------------------------------------------------------
	//! Marks a job as accepted by a player and assigns ownership.
	//! Synchronizes the change with the server if called on the client.
	//! \param job The job being accepted.
	//! \param playerId The PlayerID of the player accepting the job.
	void AcceptJob(OVT_Job job, int playerId)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		job.accepted = true;
		job.owner = persId;
		if(!Replication.IsServer())
		{
			OVT_Global.GetServer().AcceptJob(job, playerId);
		}else{
			StreamJobUpdate(job); // Directly stream update if on server
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Marks a job as declined by a player.
	//! If the job is not public, it is removed immediately. Otherwise, the player is added to the declined list.
	//! Synchronizes the change with the server if called on the client.
	//! \param job The job being declined.
	//! \param playerId The PlayerID of the player declining the job.
	void DeclineJob(OVT_Job job, int playerId)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		OVT_JobConfig config = GetConfig(job.jobIndex);

		if(!config.m_bPublic)
		{
			// Non-public jobs are removed immediately when declined by the assigned player
			m_aJobs.Remove(m_aJobs.Find(job));
		}else{
			// Public jobs track who declined them
			job.declined.Insert(persId);
		}
		if(!Replication.IsServer())
		{
			OVT_Global.GetServer().DeclineJob(job, playerId);
		}
		// Note: No server-side stream update needed here as decline either removes the job (handled later)
		// or just adds to declined list (implicitly handled by clients not seeing it offered again).
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the OnStart, OnTick, and OnEnd logic for all stages up to the job's current stage.
	//! Used primarily for bringing newly loaded/streamed jobs up to their correct state.
	//! \param job The job to process.
	void RunJobToCurrentStage(OVT_Job job)
	{
		OVT_JobConfig config = GetConfig(job.jobIndex);
		for(int i=0; i<job.stage; i++)
		{
			OVT_JobStageConfig stage = config.m_aStages[i];
			stage.m_Handler.OnStart(job);
			stage.m_Handler.OnTick(job);
			stage.m_Handler.OnEnd(job);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Sends an RPC to update the state of a specific job across all clients.
	//! \param job The job whose state needs to be broadcast.
	void StreamJobUpdate(OVT_Job job)
	{
		int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(job.owner);
		Rpc(RpcDo_UpdateJob, job.jobIndex, job.location, job.townId, job.baseId, job.stage, job.entity, playerId, job.accepted);
	}

	//------------------------------------------------------------------------------------------------
	//! Retrieves the configuration object for a given job index.
	//! \param index The index of the job configuration.
	//! \return The OVT_JobConfig object.
	OVT_JobConfig GetConfig(int index)
	{
		return m_aJobConfigs[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Periodically called method to update active jobs and check for new job starts.
	//! Handles job stage progression, completion, rewards, and spawning new jobs based on conditions.
	//! This method performs the core server-side job logic loop.
	protected void CheckUpdate()
	{
		// Update active jobs
		array<OVT_Job> remove = new array<OVT_Job>;
		foreach(OVT_Job job : m_aJobs)
		{
			if(!job.accepted) continue; // Only process accepted jobs

			int ownerId = -1;
			if(job.owner != "")
			{
				ownerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(job.owner);
			}

			OVT_JobConfig config = GetConfig(job.jobIndex);
			OVT_JobStageConfig stage = config.m_aStages[job.stage];

			// Tick current stage
			if(!stage.m_Handler.OnTick(job))
			{
				// OnTick returned false, meaning the stage finished. Progress to the next.
				stage.m_Handler.OnEnd(job); // Finalize the finished stage
				job.stage = job.stage + 1;

				if(job.stage > config.m_aStages.Count() - 1)
				{
					// No more stages, job completed
					// Reward the player
					int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(job.owner);
					if(playerId > -1) // Ensure player exists
					{
						if(!job.failed) {
						if(config.m_iReward > 0)
						{
							OVT_Global.GetEconomy().AddPlayerMoney(playerId, config.m_iReward);
						}
						if(config.m_iRewardXP > 0)
						{
							OVT_Global.GetSkills().GiveXP(playerId, config.m_iRewardXP);
						}
						SCR_HintManagerComponent.GetInstance().ShowCustom(config.m_sTitle, "#OVT-Jobs_Completed"); // Show hint to player
						}

						// Grant reward items
						if(config.m_aRewardItems.Count() > 0)
						{
							IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
							if(player)
							{
								SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(player.FindComponent( SCR_InventoryStorageManagerComponent ));
								if(inventory)
								{
									foreach(ResourceName res : config.m_aRewardItems)
									{
										if(!inventory.TrySpawnPrefabToStorage(res))
										{
											// Consider spawning on ground if inventory is full
											Print("OVT_JobManager: Failed to add reward item " + res + " to player " + job.owner + " inventory.", LogLevel.WARNING);
										}
									}
								}
							}
						}
					}

					// Notify clients of completion (mainly for hint)
					Rpc(RpcDo_NotifyJobCompleted, job.jobIndex);

					// Mark for removal and clean up tracking sets
					remove.Insert(job);
					if(config.flags & OVT_JobFlags.GLOBAL_UNIQUE) m_aGlobalJobs.Remove(m_aGlobalJobs.Find(job.jobIndex));
					if(job.townId > -1 && m_aTownJobs.Contains(job.townId) && m_aTownJobs[job.townId].Contains(job.jobIndex)){
						m_aTownJobs[job.townId].Remove(m_aTownJobs[job.townId].Find(job.jobIndex));
					}
					if(job.baseId > -1 && m_aBaseJobs.Contains(job.baseId) && m_aBaseJobs[job.baseId].Contains(job.jobIndex)){
						m_aBaseJobs[job.baseId].Remove(m_aBaseJobs[job.baseId].Find(job.jobIndex));
					}
					Rpc(RpcDo_RemoveJob, job.jobIndex, job.townId, job.baseId, ownerId); // Notify clients to remove

					continue; // Process next job
				}else{
					// Start the new stage, skipping stages where OnStart returns false immediately
					bool done = false;
					bool dorpc = true;
					while(!done)
					{
						stage = config.m_aStages[job.stage];
						if(!stage.m_Handler.OnStart(job)) {
							// Stage cannot start (condition not met?), skip to the next
							job.stage++;
							if(job.stage > config.m_aStages.Count() - 1)
							{
								// Skipped all remaining stages, job effectively finishes early/fails
								Print("OVT_JobManager: Job " + config.m_sTitle + " finished prematurely due to failing OnStart conditions.", LogLevel.DEBUG);
								remove.Insert(job);
								if(config.flags & OVT_JobFlags.GLOBAL_UNIQUE) m_aGlobalJobs.Remove(m_aGlobalJobs.Find(job.jobIndex));
								if(job.townId > -1 && m_aTownJobs.Contains(job.townId) && m_aTownJobs[job.townId].Contains(job.jobIndex)){
									m_aTownJobs[job.townId].Remove(m_aTownJobs[job.townId].Find(job.jobIndex));
								}
								if(job.baseId > -1 && m_aBaseJobs.Contains(job.baseId) && m_aBaseJobs[job.baseId].Contains(job.jobIndex)){
									m_aBaseJobs[job.baseId].Remove(m_aBaseJobs[job.baseId].Find(job.jobIndex));
								}

								Rpc(RpcDo_RemoveJob, job.jobIndex, job.townId, job.baseId, ownerId);
								dorpc = false; // Don't send update RPC if we're removing
								break; // Exit inner while loop
							}
						}else{
							// OnStart succeeded, this stage is now active
							done = true;
						}
					}
					if(dorpc) // Send update RPC if the job didn't finish prematurely
						Rpc(RpcDo_UpdateJob, job.jobIndex, job.location, job.townId, job.baseId, job.stage, job.entity, ownerId, job.accepted);
				}
			}
		}

		// Remove completed/failed jobs from the active list
		foreach(OVT_Job job : remove)
		{
			m_aJobs.RemoveItem(job);
		}

		// Check if we need to start any new jobs
		foreach(int index, OVT_JobConfig config : m_aJobConfigs)
		{
			// Initialize job count if not present
			if(!m_aJobCounts.Contains(index))
			{
				m_aJobCounts[index] = 0;
			}
			// Check max global start limit
			if(config.m_iMaxTimes != 0 && m_aJobCounts[index] >= config.m_iMaxTimes) continue;

			// Check global uniqueness flag
			if((config.flags & OVT_JobFlags.GLOBAL_UNIQUE) && m_aGlobalJobs.Contains(index)) continue;

			// Handle Base-Only Jobs
			if(config.m_bBaseOnly)
			{
				foreach(int baseId, OVT_BaseData data : m_OccupyingFaction.m_Bases)
				{
					if(data.entId == 0) continue; // Skip invalid bases
					OVT_BaseControllerComponent base = m_OccupyingFaction.GetBase(data.entId);
					if(!base) continue; // Skip if base controller not found

					// Re-check global uniqueness within the loop (although checked above, belt-and-suspenders)
					if((config.flags & OVT_JobFlags.GLOBAL_UNIQUE) && m_aGlobalJobs.Contains(index)) break;

					// Check if this job type is already active at this specific base
					if(m_aBaseJobs.Contains(baseId) && m_aBaseJobs[baseId].Contains(index)) continue;

					// Check conditions for starting the job at this base
					if(JobShouldStart(config, null, data, -1) && config.m_aStages.Count() > 0)
					{
						// Ensure the set for this base exists
						if(!m_aBaseJobs.Contains(baseId))
						{
							m_aBaseJobs[baseId] = new set<int>;
						}
						m_aBaseJobs[baseId].Insert(index); // Mark job as active at this base

						// Mark globally if needed
						if(config.flags & OVT_JobFlags.GLOBAL_UNIQUE)
						{
							m_aGlobalJobs.Insert(index);
						}

						// Start the job logic
						OVT_Job job = StartJob(index, null, data, ""); // Empty owner for base/public jobs

						// Update clients about the new job
						if(job)
							Rpc(RpcDo_UpdateJob, index, job.location, -1, baseId, job.stage, job.entity, -1, job.accepted);
					}
				}
				continue; // Move to the next job config if this was base-only
			}

			// Handle Town Jobs (Public or Player-Specific)
			foreach(OVT_TownData town : m_Towns.m_Towns)
			{
				int townID = OVT_Global.GetTowns().GetTownID(town);
				if(!config.m_bPublic)
				{
					// Player allocated job - check for each eligible player
					OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
					for(int i =0; i < players.m_mPlayers.Count(); i++)
					{
						string persId = players.m_mPlayers.GetKey(i);
						OVT_PlayerData player = players.m_mPlayers[persId];
						if(player.IsOffline()) continue; // Skip offline players

						// Initialize player job counts if needed
						map<int,int> playerJobs = m_mPlayerJobCounts[persId];
						if(!playerJobs)
						{
							playerJobs = new map<int,int>;
							m_mPlayerJobCounts[persId] = playerJobs;
						}

						// Check start conditions for this player and town
						bool start = JobShouldStart(config, town, null, player.id);

						// Check player-specific max start limit
						if(start && config.m_iMaxTimesPlayer > 0) // Assuming m_iMaxTimesPlayer exists or similar logic
						{
							int currentCount = 0;
							if(playerJobs.Contains(index)) currentCount = playerJobs[index];
							start = currentCount < config.m_iMaxTimesPlayer;
						}

						if(start && config.m_aStages.Count() > 0)
						{
							// Increment player's count for this job
							int currentCount = 0;
							if(playerJobs.Contains(index)) currentCount = playerJobs[index];
							playerJobs[index] = currentCount + 1;

							// Start the job, assigning the player as owner
							OVT_Job job = StartJob(index, town, null, persId);
							if(job)
								// Send update specifically targeting this player (or make it broadcast if needed)
								Rpc(RpcDo_UpdateJob, index, job.location, townID, -1, job.stage, job.entity, player.id, job.accepted);
						}
					}
				} else {
					// Public Job
					// Re-check global uniqueness
					if((config.flags & OVT_JobFlags.GLOBAL_UNIQUE) && m_aGlobalJobs.Contains(index)) break; // Stop checking towns for this job if globally unique and active

					// Check if this job type is already active in this specific town
					if(m_aTownJobs.Contains(townID) && m_aTownJobs[townID].Contains(index)) continue;

					// Check conditions for starting the job in this town
					if(JobShouldStart(config, town, null, -1) && config.m_aStages.Count() > 0)
					{
						// Ensure the set for this town exists
						if(!m_aTownJobs.Contains(townID))
						{
							m_aTownJobs[townID] = new set<int>;
						}
						m_aTownJobs[townID].Insert(index); // Mark job as active in this town

						// Mark globally if needed
						if(config.flags & OVT_JobFlags.GLOBAL_UNIQUE)
						{
							m_aGlobalJobs.Insert(index);
						}

						// Start the job logic
						OVT_Job job = StartJob(index, town, null, ""); // Empty owner for public jobs

						// Update clients about the new public job
						if(job)
							Rpc(RpcDo_UpdateJob, index, job.location, townID, -1, job.stage, job.entity, -1, job.accepted);
					}
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Checks if a specific job configuration should start based on its conditions.
	//! \param config The job configuration to check.
	//! \param town The town context (null if not town-specific).
	//! \param base The base context (null if not base-specific).
	//! \param playerId The player ID context (-1 if not player-specific).
	//! \return True if all conditions are met, false otherwise.
	protected bool JobShouldStart(OVT_JobConfig config, OVT_TownData town, OVT_BaseData base, int playerId)
	{
		bool start = true;
		foreach(OVT_JobCondition condition : config.m_aConditions)
		{
			if(!condition.ShouldStart(town, base, playerId)){
				start = false;
				break;
			}
		}
		return start;
	}

	//------------------------------------------------------------------------------------------------
	//! Creates and initializes a new job instance based on configuration and context.
	//! Runs the OnStart handler for the initial stage(s), potentially skipping stages.
	//! Adds the job to the active jobs list.
	//! \param index The index of the job configuration.
	//! \param town The town context for the job (can be null).
	//! \param base The base context for the job (can be null).
	//! \param owner The persistent ID of the player assigned to the job (empty for public/unassigned).
	//! \return The newly created OVT_Job instance, or null if the job finishes immediately during OnStart checks.
	OVT_Job StartJob(int index, OVT_TownData town, OVT_BaseData base, string owner)
	{
		int townID = -1;
		if(town) townID = OVT_Global.GetTowns().GetTownID(town);

		OVT_JobConfig config = GetConfig(index);
		OVT_Job job = new OVT_Job();
		job.owner = owner;
		job.jobIndex = index;
		if(town)
		{
			job.townId = townID;
			job.location = town.location; // Initial location often set by town/base
		}
		if(base)
		{
			job.baseId = base.id;
			job.location = base.location; // Initial location
		}

		job.stage = 0;
		job.accepted = (owner != ""); // Assume accepted if initially assigned to a player
		job.failed = false; // Initialize failed flag

		m_aJobs.Insert(job);
		m_aJobCounts[index] = m_aJobCounts[index] + 1; // Increment global start count

		// Run OnStart for the initial stage(s), skipping if necessary
		bool done = false;
		while(!done)
		{
			if(job.stage >= config.m_aStages.Count())
			{
				// Should not happen if config validation ensures stages exist, but handle defensively
				Print("OVT_JobManager: Job " + config.m_sTitle + " has no valid stages to start.", LogLevel.ERROR);
				m_aJobs.RemoveItem(job);
				// Potentially need to decrement m_aJobCounts here if we consider this a failed start
				return null;
			}
			OVT_JobStageConfig stage = config.m_aStages[job.stage];
			if(!stage.m_Handler.OnStart(job)) {
				// Stage condition not met, try the next stage
				job.stage++;
			}else{
				// Stage successfully started
				done = true;
			}
		}

		// If we skipped all stages (unlikely with proper design, but possible)
		if (job.stage >= config.m_aStages.Count())
		{
			Print("OVT_JobManager: Job " + config.m_sTitle + " finished immediately during initial OnStart checks.", LogLevel.WARNING);
			m_aJobs.RemoveItem(job);
			// Clean up tracking sets as if it completed instantly (or failed)
			if(config.flags & OVT_JobFlags.GLOBAL_UNIQUE) m_aGlobalJobs.Remove(m_aGlobalJobs.Find(job.jobIndex));
			if(job.townId > -1 && m_aTownJobs.Contains(job.townId) && m_aTownJobs[job.townId].Contains(job.jobIndex)){
				m_aTownJobs[job.townId].Remove(m_aTownJobs[job.townId].Find(job.jobIndex));
			}
			if(job.baseId > -1 && m_aBaseJobs.Contains(job.baseId) && m_aBaseJobs[job.baseId].Contains(job.jobIndex)){
				m_aBaseJobs[job.baseId].Remove(m_aBaseJobs[job.baseId].Find(job.jobIndex));
			}
			// No need for RPC remove as it was never RPC'd as added
			return null;
		}


		return job;
	}

	//------------------------------------------------------------------------------------------------
	// RPC Methods
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Saves the state of active jobs for JIP (Join In Progress) players.
	//! \param writer The bit writer to serialize data into.
	//! \return True on success.
	override bool RplSave(ScriptBitWriter writer)
	{
		// Send JIP active jobs
		writer.WriteInt(m_aJobs.Count());
		for(int i=0; i<m_aJobs.Count(); i++)
		{
			OVT_Job job = m_aJobs[i];
			writer.WriteInt(job.jobIndex);
			writer.WriteVector(job.location);
			writer.WriteInt(job.townId);
			writer.WriteInt(job.baseId);
			writer.WriteInt(job.stage);
			writer.WriteRplId(job.entity);
			writer.WriteString(job.owner);
			writer.WriteBool(job.accepted);
			writer.WriteBool(job.failed); // Save failed flag
			// Serialize declined list for public jobs
			writer.WriteInt(job.declined.Count());
			for(int t=0; t<job.declined.Count(); t++)
			{
				writer.WriteString(job.declined[t]);
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Loads the state of active jobs for JIP (Join In Progress) players.
	//! \param reader The bit reader to deserialize data from.
	//! \return True on success, false on read error.
	override bool RplLoad(ScriptBitReader reader)
	{
		// Receive JIP active jobs
		int length, declength;
		string persId;
		RplId jobEntityRplId; // Temporary variable to read RplId
		bool jobFailed; // Temporary variable to read failed flag

		if (!reader.ReadInt(length)) return false;
		m_aJobs.Clear(); // Clear existing jobs before loading JIP data
		for(int i=0; i<length; i++)
		{
			OVT_Job job = new OVT_Job();

			if (!reader.ReadInt(job.jobIndex)) return false;
			if (!reader.ReadVector(job.location)) return false;
			if (!reader.ReadInt(job.townId)) return false;
			if (!reader.ReadInt(job.baseId)) return false;
			if (!reader.ReadInt(job.stage)) return false;
			if (!reader.ReadRplId(jobEntityRplId)) return false; // Read into temporary variable
			job.entity = jobEntityRplId; // Assign to the job's RplId field
			if (!reader.ReadString(persId)) return false;
			job.owner = persId;
			if(!reader.ReadBool(job.accepted)) return false;
			if(!reader.ReadBool(jobFailed)) return false; // Read failed flag
			job.failed = jobFailed; // Assign failed flag
			// Deserialize declined list
			if(!reader.ReadInt(declength)) return false;
			job.declined.Clear();
			for(int t=0; t<declength; t++)
			{
				if(!reader.ReadString(persId)) return false;
				job.declined.Insert(persId);
			}

			m_aJobs.Insert(job);
			// Optional: Could call RunJobToCurrentStage here if needed,
			// but stage handlers might rely on world state not yet fully loaded on JIP.
			// It might be safer to handle stage initialization logic elsewhere post-JIP.
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! RPC called on clients to create or update the state of a job.
	//! \param index Job configuration index.
	//! \param location Current job location.
	//! \param townId Associated town ID (-1 if none).
	//! \param baseId Associated base ID (-1 if none).
	//! \param stage Current job stage index.
	//! \param entity RplId of the associated entity.
	//! \param ownerId PlayerID of the owner (-1 if unassigned/public).
	//! \param accepted Whether the job is currently accepted.
	//! \param failed Whether the job has failed.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_UpdateJob(int index, vector location, int townId, int baseId, int stage, RplId entity, int ownerId, bool accepted)
	{
		string persId = "";
		if(ownerId > -1)
			persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(ownerId);

		bool updated = false;
		// Try to find and update an existing job matching index and location context
		foreach(OVT_Job job : m_aJobs)
		{
			// Match based on job index AND either townId or baseId matching the context it was started in
			if(job.jobIndex == index && ((job.townId != -1 && job.townId == townId) || (job.baseId != -1 && job.baseId == baseId) || (townId == -1 && baseId == -1))) // Handle jobs not tied to town/base
			{
				job.stage = stage;
				job.location = location;
				job.owner = persId;
				job.accepted = accepted;
				job.entity = entity; // Update associated entity RplId
				updated = true;
				// Optional: Trigger UI update or RunJobToCurrentStage if needed on client side update
				break; // Assume only one job matches this specific context
			}
		}

		if(!updated)
		{
			// Job not found, create a new local representation
			OVT_Job job = new OVT_Job();
			job.jobIndex = index;
			job.location = location;
			job.townId = townId;
			job.baseId = baseId;
			job.stage = stage;
			job.entity = entity;
			job.owner = persId;
			job.accepted = accepted;
			// Declined list is managed server-side and implicitly handled by job availability

			m_aJobs.Insert(job);
			// Optional: Trigger UI update for the new job
		}
	}

	//------------------------------------------------------------------------------------------------
	//! RPC called on clients to remove a job from their local list.
	//! \param index Job configuration index.
	//! \param townId Associated town ID (-1 if none). Used for matching context.
	//! \param baseId Associated base ID (-1 if none). Used for matching context.
	//! \param ownerId PlayerID of the owner (-1 if unassigned/public). Used for matching context, esp. non-public jobs.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RemoveJob(int index, int townId, int baseId, int ownerId)
	{
		string persId = "";
		if(ownerId > -1)
			persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(ownerId);

		OVT_Job foundjob = null;
		// Find the specific job instance to remove based on index, context (town/base), and owner
		foreach(OVT_Job job : m_aJobs)
		{
			// Match index, context (town OR base OR neither), AND owner
			if(job.jobIndex == index &&
			   ((job.townId != -1 && job.townId == townId) || (job.baseId != -1 && job.baseId == baseId) || (townId == -1 && baseId == -1)) &&
			   job.owner == persId)
			{
				foundjob = job;
				break;
			}
		}
		if(foundjob)
		{
			m_aJobs.RemoveItem(foundjob);
			// Optional: Trigger UI update to remove the job display
		} else {
			// Log if we received a remove RPC for a job we don't seem to have. Might indicate sync issue or late RPC.
			Print("OVT_JobManager(Client): Received RpcDo_RemoveJob for job index " + index + " (town/base: " + townId + "/" + baseId + ", owner: " + persId + ") but no matching job found.", LogLevel.DEBUG);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! RPC called on the owning client when their job is successfully completed, primarily for showing hints.
	//! \param index The index of the completed job config (used to get title).
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)] // Broadcast, but likely only relevant to owner? Could be RplRcver.Owner if tied to player controller
	protected void RpcDo_NotifyJobCompleted(int index)
	{
		// Check if this client is the owner? Might not be necessary if hint manager handles it.
		// int localPlayerId = GetGame().GetPlayerController().GetPlayerId();
		// string localPersId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(localPlayerId);
		// Find the job to see if localPersId matches job.owner? Job might already be removed by RpcDo_RemoveJob.
		// Safest to just show the hint regardless, assuming the server only sends this when appropriate.

		OVT_JobConfig config = GetConfig(index);
		if(config)
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom(config.m_sTitle, "#OVT-Jobs_Completed");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! RPC called on clients to set the failed status of a job.
	//! \param index Job configuration index.
	//! \param townId Associated town ID (-1 if none).
	//! \param baseId Associated base ID (-1 if none).
	//! \param failed Whether the job has failed.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetJobFailed(int index, int townId, int baseId, bool failed)
	{
		foreach(OVT_Job job : m_aJobs)
		{
			if(job.jobIndex == index && ((job.townId != -1 && job.townId == townId) || (job.baseId != -1 && job.baseId == baseId) || (townId == -1 && baseId == -1)))
			{
				job.failed = failed;
				break;
			}
		}
	}

}