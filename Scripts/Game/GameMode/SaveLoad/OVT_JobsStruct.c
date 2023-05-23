[BaseContainerProps()]
class OVT_JobsStruct : OVT_BaseSaveStruct
{
	ref array<ref OVT_JobStruct> jobs = {};
	ref set<int> global;
	ref array<ref OVT_IDMapStruct> towns = {};
	ref array<ref OVT_IDMapStruct> bases = {};
	ref array<ref OVT_PlayerIDIntMapStruct> players = {};
		
	override bool Serialize()
	{
		jobs.Clear();
		towns.Clear();
		bases.Clear();
		players.Clear();
		OVT_JobManagerComponent jm = OVT_Global.GetJobs();
		
		foreach(OVT_Job job : jm.m_aJobs)
		{
			OVT_JobStruct struct = new OVT_JobStruct;
			struct.jobIndex = job.jobIndex;
			struct.location = job.location;
			struct.townId = job.townId;
			struct.baseId = job.baseId;
			struct.stage = job.stage;
			struct.owner = job.owner;
			struct.accepted = job.accepted;
			struct.declined = job.declined;
			jobs.Insert(struct);
		}		
		
		global = jm.m_aGlobalJobs;
		
		for(int i = 0; i < jm.m_aTownJobs.Count(); i++)
		{
			OVT_IDMapStruct struct = new OVT_IDMapStruct;
			struct.ids = jm.m_aTownJobs.GetElement(i);
			struct.id = jm.m_aTownJobs.GetKey(i);
			towns.Insert(struct);
		}
		
		for(int i = 0; i < jm.m_aBaseJobs.Count(); i++)
		{
			OVT_IDMapStruct struct = new OVT_IDMapStruct;
			struct.ids = jm.m_aBaseJobs.GetElement(i);
			struct.id = jm.m_aBaseJobs.GetKey(i);
			bases.Insert(struct);
		}
		
		for(int i = 0; i < jm.m_mPlayerJobCounts.Count(); i++)
		{
			OVT_PlayerIDIntMapStruct struct = new OVT_PlayerIDIntMapStruct;			
			struct.id = jm.m_mPlayerJobCounts.GetKey(i);
			map<int,int> counts = jm.m_mPlayerJobCounts.GetElement(i);
			if(counts){
				for(int t = 0; t < counts.Count(); t++)
				{
					OVT_InventoryStruct s = new OVT_InventoryStruct;
					s.id = jm.m_mPlayerJobCounts.GetElement(t).GetKey(t);
					s.qty = jm.m_mPlayerJobCounts.GetElement(t).GetElement(t);
					struct.ids.Insert(s);
				}
				players.Insert(struct);
			}
		}
		
		return true;
	}
	
	override bool Deserialize()
	{		
		OVT_JobManagerComponent jm = OVT_Global.GetJobs();
		
		foreach(OVT_JobStruct struct : jobs)
		{
			OVT_Job job = new OVT_Job;
			job.jobIndex = struct.jobIndex;
			job.location = struct.location;
			job.townId = struct.townId;
			job.baseId = struct.baseId;
			job.stage = struct.stage;
			job.owner = struct.owner;
			job.accepted = struct.accepted;
			job.declined = struct.declined;
			jm.m_aJobs.Insert(job);
			
			jm.RunJobToCurrentStage(job);
		}	
		
		foreach(OVT_IDMapStruct struct : towns)
		{
			jm.m_aTownJobs[struct.id] = struct.ids;
		}	
		
		foreach(OVT_IDMapStruct struct : bases)
		{
			jm.m_aBaseJobs[struct.id] = struct.ids;
		}
		
		foreach(OVT_PlayerIDIntMapStruct struct : players)
		{
			jm.m_mPlayerJobCounts[struct.id] = new map<int,int>;
			foreach(OVT_InventoryStruct s : struct.ids){
				jm.m_mPlayerJobCounts[struct.id][s.id] = s.qty;
			}
		}
			
		return true;
	}
	
	void OVT_JobsStruct()
	{
		RegV("jobs");
		RegV("global");
		RegV("towns");
		RegV("bases");
		RegV("players");
	}
}

class OVT_JobStruct : SCR_JsonApiStruct
{
	int jobIndex;
	vector location;
	int townId = -1;
	int baseId = -1;
	int stage = 0;
	string owner;
	bool accepted;
	ref array<string> declined = {};
	
	void OVT_BaseDataStruct()
	{
		RegV("jobIndex");
		RegV("location");
		RegV("townId");
		RegV("baseId");
		RegV("stage");
		RegV("owner");
		RegV("accepted");
		RegV("declined");
	}
}