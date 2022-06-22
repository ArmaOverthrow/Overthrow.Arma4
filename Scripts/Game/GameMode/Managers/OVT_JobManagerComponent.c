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
	
	[Attribute("100")]
	int m_iReward;
	
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
	int townId;
	int stage;
	RplId entity;
}

class OVT_JobManagerComponent: OVT_Component
{
	[Attribute()]
	ref array<ResourceName> m_aJobConfigFiles;
	
	ref array<ref OVT_JobConfig> m_aJobConfigs;
	ref array<ref OVT_Job> m_aJobs;
	
	ref set<int> m_aGlobalJobs;
	ref map<int, ref set<int>> m_aTownJobs;
	
	protected OVT_TownManagerComponent m_Towns;
	
	protected const int JOB_FREQUENCY = 10000;
	
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
		m_aJobs = new array<ref OVT_Job>;
	}
	
	void Init(IEntity owner)
	{
		LoadConfigs();
		
		m_Towns = OVT_Global.GetTowns();
		
		if(!Replication.IsServer()) return;
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
					}
					
					remove.Insert(job);
					if(config.flags & OVT_JobFlags.GLOBAL_UNIQUE) m_aGlobalJobs.Remove(m_aGlobalJobs.Find(job.jobIndex));
					m_aTownJobs[job.townId].Remove(m_aTownJobs[job.townId].Find(job.jobIndex));
					Rpc(RpcDo_RemoveJob, job.jobIndex, job.townId);
					
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
								m_aTownJobs[job.townId].Remove(m_aTownJobs[job.townId].Find(job.jobIndex));
								Rpc(RpcDo_RemoveJob, job.jobIndex, job.townId);
								dorpc = false;
								break;
							}
						}else{
							done = true;
						}
					}
					if(dorpc)
						Rpc(RpcDo_UpdateJob, job.jobIndex, job.location, job.townId, job.stage, job.entity);
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
			if(config.flags & OVT_JobFlags.GLOBAL_UNIQUE && m_aGlobalJobs.Contains(index)) continue;			
			foreach(OVT_TownData town : m_Towns.m_Towns)
			{
				if(m_aTownJobs.Contains(town.id) && m_aTownJobs[town.id].Contains(index)) continue;
				
				bool start = true;
				foreach(OVT_JobCondition condition : config.m_aConditions)
				{
					if(!condition.ShouldStart(town)){
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
					
					OVT_Job job = new OVT_Job();
					job.jobIndex = index;
					job.townId = town.id;
					job.stage = 0;
					
					m_aJobs.Insert(job);
					
					bool done = false;
					bool dorpc = true;
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
								dorpc = false;
								break;
							}
						}else{
							done = true;
						}
					}
					
					//Update clients
					if(dorpc)
						Rpc(RpcDo_UpdateJob, index, job.location, town.id, job.stage, job.entity);
				}
			}
		}
		
		
		
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
			if (!reader.Read(job.stage, 32)) return false;	
			if (!reader.ReadRplId(job.entity)) return false;		
						
			m_aJobs.Insert(job);
		}
		return true;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_UpdateJob(int index, vector location, int townId, int stage, RplId entity)
	{
		bool updated = false;
		foreach(OVT_Job job : m_aJobs)
		{
			if(job.jobIndex == index && job.townId == townId)
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
			job.stage = stage;
			job.entity = entity;
			
			m_aJobs.Insert(job);
		}
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RemoveJob(int index, int townId)
	{
		OVT_Job foundjob;
		foreach(OVT_Job job : m_aJobs)
		{
			if(job.jobIndex == index && job.townId == townId)
			{
				foundjob = job;
			}
		}		
		if(foundjob)
		{
			m_aJobs.RemoveItem(foundjob);
		}
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