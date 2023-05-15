class OVT_JobManagerComponentClass: OVT_ComponentClass
{
};

//Represents an active job and it's current state
class OVT_Job
{
	int jobIndex;
	vector location;
	int townId = -1;
	int baseId = -1;
	int stage;
	RplId entity;
	string owner;
	bool accepted;
	ref array<string> declined = {};
	
	OVT_TownData GetTown()
	{
		return OVT_Global.GetTowns().m_Towns[townId];
	}
	
	OVT_BaseControllerComponent GetBase()
	{
		return OVT_Global.GetOccupyingFaction().GetBaseByIndex(baseId);
	}
}

class OVT_JobManagerComponent: OVT_Component
{
	[Attribute()]
	ref array<ResourceName> m_aJobConfigFiles;
	
	ref array<ref OVT_JobConfig> m_aJobConfigs;
	ref array<ref OVT_Job> m_aJobs;
	
	ref set<int> m_aGlobalJobs;
	ref map<int, ref set<int>> m_aTownJobs;
	ref map<int, ref set<int>> m_aBaseJobs;
	ref map<int, int> m_aJobCounts;
	ref map<string, map<int, int>> m_mPlayerJobCounts;
	
	protected OVT_TownManagerComponent m_Towns;
	protected OVT_OccupyingFactionManager m_OccupyingFaction;
	
	protected const int JOB_FREQUENCY = 10000;
	
	vector m_vCurrentWaypoint;
	
	static OVT_JobManagerComponent s_Instance;	
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
	
	void OVT_JobManagerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_aJobConfigs = new array<ref OVT_JobConfig>;
		m_aGlobalJobs = new set<int>;	
		m_aTownJobs = new map<int, ref set<int>>;
		m_aBaseJobs = new map<int, ref set<int>>;
		m_aJobs = new array<ref OVT_Job>;
		m_aJobCounts = new map<int, int>;
		m_mPlayerJobCounts = new map<string, map<int, int>>;
	}
	
	void Init(IEntity owner)
	{
		LoadConfigs();
		
		m_Towns = OVT_Global.GetTowns();
		m_OccupyingFaction = OVT_Global.GetOccupyingFaction();
				
	}
	
	void PostGameStart()
	{
		GetGame().GetCallqueue().CallLater(CheckUpdate, 250, false, GetOwner());
		GetGame().GetCallqueue().CallLater(CheckUpdate, JOB_FREQUENCY, true, GetOwner());		
	}
	
	void AcceptJob(OVT_Job job, int playerId)
	{	
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		job.accepted = true;	
		job.owner = persId;
		if(!Replication.IsServer())
		{
			OVT_Global.GetServer().AcceptJob(job, playerId);
		}else{
			StreamJobUpdate(job);
		}
	}
	
	void DeclineJob(OVT_Job job, int playerId)
	{			
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		OVT_JobConfig config = GetConfig(job.jobIndex);
		
		if(!config.m_bPublic)
		{
			m_aJobs.Remove(m_aJobs.Find(job));
		}else{
			job.declined.Insert(persId);
		}		
		if(!Replication.IsServer())
		{
			OVT_Global.GetServer().DeclineJob(job, playerId);
		}
	}
	
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
	
	void StreamJobUpdate(OVT_Job job)
	{
		int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(job.owner);
		Rpc(RpcDo_UpdateJob, job.jobIndex, job.location, job.townId, job.baseId, job.stage, job.entity, playerId, job.accepted);
	}
	
	protected void LoadConfigs()
	{
		foreach(ResourceName res : m_aJobConfigFiles)
		{
			Resource holder = BaseContainerTools.LoadContainer(res);
			if (holder)		
			{
				OVT_JobConfig obj = OVT_JobConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(holder.GetResource().ToBaseContainer()));
				if(obj)
				{
					m_aJobConfigs.Insert(obj);
				}
			}
		}		
	}
	
	OVT_JobConfig GetConfig(int index)
	{
		return m_aJobConfigs[index];
	}
	
	protected void CheckUpdate()
	{
		//Update active jobs
		array<OVT_Job> remove = new array<OVT_Job>;
		foreach(OVT_Job job : m_aJobs)
		{
			if(!job.accepted) continue;
			int ownerId = -1;
			if(job.owner != "")
			{
				ownerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(job.owner);
			}
			OVT_JobConfig config = GetConfig(job.jobIndex);
			OVT_JobStageConfig stage = config.m_aStages[job.stage];
			if(!stage.m_Handler.OnTick(job))
			{
				//Go to next stage
				job.stage = job.stage + 1;
				if(job.stage > config.m_aStages.Count() - 1)
				{								
					//No more stages, job finished
					
					//Reward the player
					int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(job.owner);
					OVT_Global.GetEconomy().AddPlayerMoney(playerId, config.m_iReward);
					SCR_HintManagerComponent.GetInstance().ShowCustom(config.m_sTitle, "#OVT-Jobs_Completed");
					Rpc(RpcDo_NotifyJobCompleted, job.jobIndex);
										
					remove.Insert(job);
					if(config.flags & OVT_JobFlags.GLOBAL_UNIQUE) m_aGlobalJobs.Remove(m_aGlobalJobs.Find(job.jobIndex));
					if(job.townId > -1 && m_aTownJobs.Contains(job.townId) && m_aTownJobs[job.townId].Contains(job.jobIndex)){
						m_aTownJobs[job.townId].Remove(m_aTownJobs[job.townId].Find(job.jobIndex));
					}
					if(job.baseId > -1 && m_aBaseJobs.Contains(job.baseId) && m_aBaseJobs[job.baseId].Contains(job.jobIndex)){
						m_aBaseJobs[job.baseId].Remove(m_aBaseJobs[job.baseId].Find(job.jobIndex));
					}
					Rpc(RpcDo_RemoveJob, job.jobIndex, job.townId, job.baseId, ownerId);
					
					break;
				}else{
					//Next stage, run OnStart until it returns true
					bool done = false;
					bool dorpc = true;
					while(!done)
					{
						stage = config.m_aStages[job.stage];
						if(!stage.m_Handler.OnStart(job)) {
							//Go to the next stage
							job.stage++;
							if(job.stage > config.m_aStages.Count() - 1)
							{
								remove.Insert(job);
								if(config.flags & OVT_JobFlags.GLOBAL_UNIQUE) m_aGlobalJobs.Remove(m_aGlobalJobs.Find(job.jobIndex));
								if(job.townId > -1)
									m_aTownJobs[job.townId].Remove(m_aTownJobs[job.townId].Find(job.jobIndex));
								if(job.baseId > -1)
									m_aBaseJobs[job.baseId].Remove(m_aBaseJobs[job.baseId].Find(job.jobIndex));
								
								Rpc(RpcDo_RemoveJob, job.jobIndex, job.townId, job.baseId, ownerId);
								dorpc = false;
								break;
							}
						}else{
							done = true;
						}
					}
					if(dorpc)
						Rpc(RpcDo_UpdateJob, job.jobIndex, job.location, job.townId, job.baseId, job.stage, job.entity, ownerId, job.accepted);
				}
			}
		}
		
		foreach(OVT_Job job : remove)
		{
			m_aJobs.RemoveItem(job);
		}
		
		//Check if we need to start any jobs
		foreach(int index, OVT_JobConfig config : m_aJobConfigs)
		{
			if(!m_aJobCounts.Contains(index))
			{
				m_aJobCounts[index] = 0;
			}
			if(config.m_iMaxTimes != 0 && m_aJobCounts[index] >= config.m_iMaxTimes) continue;
			
			if((config.flags & OVT_JobFlags.GLOBAL_UNIQUE) && m_aGlobalJobs.Contains(index)) continue;			
			if(config.m_bBaseOnly)
			{
				foreach(int baseId, OVT_BaseData data : m_OccupyingFaction.m_Bases)
				{
					if(data.entId == 0) continue;
					OVT_BaseControllerComponent base = m_OccupyingFaction.GetBase(data.entId);
					if((config.flags & OVT_JobFlags.GLOBAL_UNIQUE) && m_aGlobalJobs.Contains(index)) break;			
					if(m_aBaseJobs.Contains(baseId) && m_aBaseJobs[baseId].Contains(index)) continue;
					
					OVT_TownData town = m_Towns.GetNearestTown(base.GetOwner().GetOrigin());
										
					if(JobShouldStart(config, null, data, -1) && config.m_aStages.Count() > 0)
					{
						if(!m_aBaseJobs.Contains(baseId))
						{
							m_aBaseJobs[baseId] = new set<int>;
						}
						m_aBaseJobs[baseId].Insert(index);
						
						if(config.flags & OVT_JobFlags.GLOBAL_UNIQUE)
						{
							m_aGlobalJobs.Insert(index);
						}
						
						OVT_Job job = StartJob(index, null, data, "");
						
						//Update clients
						if(job)
							Rpc(RpcDo_UpdateJob, index, job.location, -1, baseId, job.stage, job.entity, -1, job.accepted);
					}
				}
				continue;
			}
			foreach(OVT_TownData town : m_Towns.m_Towns)
			{
				if(!config.m_bPublic)
				{
					//Player allocated job
					OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
					for(int i =0; i < players.m_mPlayers.Count(); i++)
					{
						string persId = players.m_mPlayers.GetKey(i);
						OVT_PlayerData player = players.m_mPlayers[persId];
						if(player.IsOffline()) continue;
						map<int,int> playerJobs = m_mPlayerJobCounts[persId];
						if(!playerJobs)
						{
							playerJobs = new map<int,int>;
							m_mPlayerJobCounts[persId] = playerJobs;
						}
						
						bool start = JobShouldStart(config, town, null, player.id);
										
						if(start && playerJobs.Contains(index) && config.m_iMaxTimes > 0){
							start = playerJobs[index] < config.m_iMaxTimes;
						}
						
						if(start && config.m_aStages.Count() > 0)
						{
							playerJobs[index] = playerJobs[index] + 1;
							OVT_Job job = StartJob(index, town, null, persId);
							if(job)
								Rpc(RpcDo_UpdateJob, index, job.location, town.id, -1, job.stage, job.entity, player.id, job.accepted);
						}
					}
				}else{
					//Public Job
					if((config.flags & OVT_JobFlags.GLOBAL_UNIQUE) && m_aGlobalJobs.Contains(index)) break;			
					if(m_aTownJobs.Contains(town.id) && m_aTownJobs[town.id].Contains(index)) continue;
								
					if(JobShouldStart(config, town, null, -1) && config.m_aStages.Count() > 0)
					{
						if(!m_aTownJobs.Contains(town.id))
						{
							m_aTownJobs[town.id] = new set<int>;
						}
						m_aTownJobs[town.id].Insert(index);
						
						if(config.flags & OVT_JobFlags.GLOBAL_UNIQUE)
						{
							m_aGlobalJobs.Insert(index);
						}
						
						OVT_Job job = StartJob(index, town, null, "");
						
						//Update clients
						if(job)
							Rpc(RpcDo_UpdateJob, index, job.location, town.id, -1, job.stage, job.entity, -1, job.accepted);
					}
				}
			}
		}
		
		
		
	}
	
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
	
	OVT_Job StartJob(int index, OVT_TownData town, OVT_BaseData base, string owner)
	{
		OVT_JobConfig config = GetConfig(index);
		OVT_Job job = new OVT_Job();
		job.owner = owner;
		job.jobIndex = index;
		if(town)
		{
			job.townId = town.id;
			job.location = town.location;
		}
		if(base)
		{
			job.baseId = base.id;
			job.location = base.location;
		}
		
		job.stage = 0;
		
		m_aJobs.Insert(job);					
		m_aJobCounts[index] = m_aJobCounts[index] + 1;
		
		bool done = false;		
		while(!done)
		{
			OVT_JobStageConfig stage = config.m_aStages[job.stage];
			if(!stage.m_Handler.OnStart(job)) {
				//Go to the next stage
				job.stage++;
				if(job.stage > config.m_aStages.Count() - 1)
				{								
					//Job finished without any ticks??
					m_aJobs.RemoveItem(job);
					return null;					
				}
			}else{
				done = true;
			}
		}
		
		return job;
	}
	
	//RPC Methods
	
	override bool RplSave(ScriptBitWriter writer)
	{		
		
		//Send JIP active jobs
		writer.Write(m_aJobs.Count(), 32); 
		for(int i; i<m_aJobs.Count(); i++)
		{
			OVT_Job job = m_aJobs[i];
			writer.Write(job.jobIndex, 32);
			writer.WriteVector(job.location);
			writer.Write(job.townId, 32);
			writer.Write(job.baseId, 32);
			writer.Write(job.stage, 32);
			writer.WriteRplId(job.entity);	
			writer.WriteString(job.owner);
			writer.WriteBool(job.accepted);	
			writer.Write(job.declined.Count(),32);
			for(int t; t<job.declined.Count(); t++)
			{
				writer.WriteString(job.declined[t]);
			}
		}
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{	
					
		//Recieve JIP active jobs
		int length, declength;
		string persId;
		
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{
			OVT_Job job = new OVT_Job();
			
			if (!reader.Read(job.jobIndex, 32)) return false;
			if (!reader.ReadVector(job.location)) return false;		
			if (!reader.Read(job.townId, 32)) return false;		
			if (!reader.Read(job.baseId, 32)) return false;	
			if (!reader.Read(job.stage, 32)) return false;	
			if (!reader.ReadRplId(job.entity)) return false;	
			if (!reader.ReadString(persId)) return false;
			job.owner = persId;
			if(!reader.ReadBool(job.accepted)) return false;
			if(!reader.Read(declength,32)) return false;
			for(int t; t<declength; t++)
			{
				if(!reader.ReadString(persId)) return false;
				job.declined.Insert(persId);
			}
						
			m_aJobs.Insert(job);
		}
		return true;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_UpdateJob(int index, vector location, int townId, int baseId, int stage, RplId entity, int ownerId, bool accepted)
	{
		string persId = "";
		if(ownerId > -1)
			persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(ownerId);
		bool updated = false;
		foreach(OVT_Job job : m_aJobs)
		{
			if(job.jobIndex == index && (job.townId == townId || job.baseId == baseId))
			{
				job.stage = stage;
				job.location = location;
				job.owner = persId;
				job.accepted = accepted;
				updated = true;
			}
		}
		if(!updated)
		{
			//Create it
			OVT_Job job = new OVT_Job();
			job.jobIndex = index;
			job.location = location;
			job.townId = townId;
			job.baseId = baseId;
			job.stage = stage;
			job.entity = entity;
			job.owner = persId;
			job.accepted = accepted;
			
			m_aJobs.Insert(job);
		}
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RemoveJob(int index, int townId, int baseId, int ownerId)
	{
		string persId = "";
		if(ownerId > -1)
			persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(ownerId);
		
		OVT_Job foundjob;
		foreach(OVT_Job job : m_aJobs)
		{
			if(job.jobIndex == index && (job.townId == townId || job.baseId == baseId) && job.owner == persId)
			{
				foundjob = job;
			}
		}		
		if(foundjob)
		{
			m_aJobs.RemoveItem(foundjob);
		}
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_NotifyJobCompleted(int index)
	{
		OVT_JobConfig config = GetConfig(index);
		SCR_HintManagerComponent.GetInstance().ShowCustom(config.m_sTitle, "#OVT-Jobs_Completed");
	}
	
	void ~OVT_JobManagerComponent()
	{
		GetGame().GetCallqueue().Remove(CheckUpdate);
		
		if(m_aJobConfigs)
		{
			m_aJobConfigs.Clear();
			m_aJobConfigs = null;
		}
		if(m_aGlobalJobs)
		{
			m_aGlobalJobs.Clear();
			m_aGlobalJobs = null;
		}
		if(m_aTownJobs)
		{
			m_aTownJobs.Clear();
			m_aTownJobs = null;
		}
		if(m_aJobs)
		{
			m_aJobs.Clear();
			m_aJobs = null;
		}
	}
}