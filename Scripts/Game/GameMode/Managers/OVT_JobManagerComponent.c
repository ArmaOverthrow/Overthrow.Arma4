class OVT_JobManagerComponentClass: OVT_ComponentClass
{
};

enum OVT_JobFlags
{	
	ACTIVE = 1,
	GLOBAL_UNIQUE = 2
};

//A config for a job that has start conditions and a number of stages
[BaseContainerProps(configRoot : true)]
class OVT_JobConfig
{
	[Attribute()]
	string m_sTitle;
	
	[Attribute()]
	string m_sDescription;
	
	[Attribute("0")]
	bool m_bBaseOnly;
	
	[Attribute("100")]
	int m_iReward;
	
	[Attribute(defvalue: "0", desc:"Maximum number of times this job will spawn")]
	int m_iMaxTimes;
	
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_JobCondition> m_aConditions;
	
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_JobStageConfig> m_aStages;
	
	[Attribute("1", uiwidget: UIWidgets.Flags, "", "", ParamEnumArray.FromEnum(OVT_JobFlags))]
	OVT_JobFlags flags;
}

class OVT_JobStageConfig : ScriptAndConfig
{
	[Attribute()]
	string m_sTitle;
	
	[Attribute()]
	string m_sDescription;
	
	[Attribute("0")]
	int m_iTimeout;
	
	[Attribute("", UIWidgets.Object)]
	ref OVT_JobStage m_Handler;
}

//Represents an active job and it's current state
class OVT_Job
{
	int jobIndex;
	vector location;
	int townId = -1;
	int baseId = -1;
	int stage;
	RplId entity;
	
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
	
	void OVT_JobManagerComponent()
	{
		m_aJobConfigs = new array<ref OVT_JobConfig>;
		m_aGlobalJobs = new set<int>;	
		m_aTownJobs = new map<int, ref set<int>>;
		m_aBaseJobs = new map<int, ref set<int>>;
		m_aJobs = new array<ref OVT_Job>;
		m_aJobCounts = new map<int, int>;
	}
	
	void Init(IEntity owner)
	{
		LoadConfigs();
		
		m_Towns = OVT_Global.GetTowns();
		m_OccupyingFaction = OVT_Global.GetOccupyingFaction();
				
	}
	
	void PostGameStart()
	{
		GetGame().GetCallqueue().CallLater(CheckUpdate, JOB_FREQUENCY, true, GetOwner());		
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
			OVT_JobConfig config = GetConfig(job.jobIndex);
			OVT_JobStageConfig stage = config.m_aStages[job.stage];
			if(!stage.m_Handler.OnTick(job))
			{
				//Go to next stage
				job.stage = job.stage + 1;
				if(job.stage > config.m_aStages.Count() - 1)
				{								
					//No more stages, job finished
					if(stage.m_Handler.playerId > -1 && config.m_iReward > 0)
					{
						//Reward a player
						OVT_Global.GetEconomy().AddPlayerMoney(stage.m_Handler.playerId, config.m_iReward);
						SCR_HintManagerComponent.GetInstance().ShowCustom(config.m_sTitle, "#OVT-Jobs_Completed");
						Rpc(RpcDo_NotifyJobCompleted, job.jobIndex);
					}
					
					remove.Insert(job);
					if(config.flags & OVT_JobFlags.GLOBAL_UNIQUE) m_aGlobalJobs.Remove(m_aGlobalJobs.Find(job.jobIndex));
					if(job.townId > -1)
						m_aTownJobs[job.townId].Remove(m_aTownJobs[job.townId].Find(job.jobIndex));
					if(job.baseId > -1)
						m_aBaseJobs[job.baseId].Remove(m_aBaseJobs[job.baseId].Find(job.jobIndex));
					Rpc(RpcDo_RemoveJob, job.jobIndex, job.townId, job.baseId);
					
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
								
								Rpc(RpcDo_RemoveJob, job.jobIndex, job.townId, job.baseId);
								dorpc = false;
								break;
							}
						}else{
							done = true;
						}
					}
					if(dorpc)
						Rpc(RpcDo_UpdateJob, job.jobIndex, job.location, job.townId, job.baseId, job.stage, job.entity);
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
					OVT_BaseControllerComponent base = m_OccupyingFaction.GetBase(data.entId);
					if((config.flags & OVT_JobFlags.GLOBAL_UNIQUE) && m_aGlobalJobs.Contains(index)) break;			
					if(m_aBaseJobs.Contains(baseId) && m_aBaseJobs[baseId].Contains(index)) continue;
					
					OVT_TownData town = m_Towns.GetNearestTown(base.GetOwner().GetOrigin());
					
					bool start = true;
					foreach(OVT_JobCondition condition : config.m_aConditions)
					{
						if(!condition.ShouldStart(town, data)){
							start = false;
							break;
						}
					}
					
					if(start && config.m_aStages.Count() > 0)
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
						
						OVT_Job job = StartJob(index, null, data);
						
						//Update clients
						if(job)
							Rpc(RpcDo_UpdateJob, index, job.location, -1, baseId, job.stage, job.entity);
					}
				}
				continue;
			}
			foreach(OVT_TownData town : m_Towns.m_Towns)
			{
				if((config.flags & OVT_JobFlags.GLOBAL_UNIQUE) && m_aGlobalJobs.Contains(index)) break;			
				if(m_aTownJobs.Contains(town.id) && m_aTownJobs[town.id].Contains(index)) continue;
				
				bool start = true;
				foreach(OVT_JobCondition condition : config.m_aConditions)
				{
					if(!condition.ShouldStart(town, null)){
						start = false;
						break;
					}
				}
				
				if(start && config.m_aStages.Count() > 0)
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
					
					OVT_Job job = StartJob(index, town, null);
					
					//Update clients
					if(job)
						Rpc(RpcDo_UpdateJob, index, job.location, town.id, -1, job.stage, job.entity);
				}
			}
		}
		
		
		
	}
	
	OVT_Job StartJob(int index, OVT_TownData town, OVT_BaseData base)
	{
		OVT_JobConfig config = GetConfig(index);
		OVT_Job job = new OVT_Job();
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
		}
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{	
					
		//Recieve JIP active jobs
		int length;
		
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
						
			m_aJobs.Insert(job);
		}
		return true;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_UpdateJob(int index, vector location, int townId, int baseId, int stage, RplId entity)
	{
		bool updated = false;
		foreach(OVT_Job job : m_aJobs)
		{
			if(job.jobIndex == index && (job.townId == townId || job.baseId == baseId))
			{
				job.stage = stage;
				job.location = location;
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
			
			m_aJobs.Insert(job);
		}
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RemoveJob(int index, int townId, int baseId)
	{
		OVT_Job foundjob;
		foreach(OVT_Job job : m_aJobs)
		{
			if(job.jobIndex == index && (job.townId == townId || job.baseId == baseId))
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