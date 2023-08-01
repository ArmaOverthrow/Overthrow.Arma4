[EPF_ComponentSaveDataType(OVT_ResistanceFactionManager), BaseContainerProps()]
class OVT_ResistanceSaveDataClass : EPF_ComponentSaveDataClass
{
};

[EDF_DbName.Automatic()]
class OVT_ResistanceSaveData : EPF_ComponentSaveData
{
	ref array<ref OVT_CampData> m_Camps;
	string m_sPlayerFactionKey;
	
	//Jobs
	ref set<int> m_aGlobalJobs;
	ref map<int, ref set<int>> m_aTownJobs;
	ref map<int, ref set<int>> m_aBaseJobs;
	ref map<int, int> m_aJobCounts;	
	ref array<ref OVT_Job> m_aJobs;
	
	ref set<string> m_aJobCountPlayerIds;
	ref array<ref map<int, int>> m_mPlayerJobCounts;
	
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{		
		OVT_ResistanceFactionManager resistance = OVT_ResistanceFactionManager.Cast(component);
		
		m_Camps = new array<ref OVT_CampData>;
		m_sPlayerFactionKey = OVT_Global.GetConfig().m_sPlayerFaction;
		
		foreach(OVT_CampData fob : resistance.m_Camps)
		{
			fob.garrison.Clear();
			foreach(EntityID id : fob.garrisonEntities)
			{
				SCR_AIGroup aigroup = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(id));
				if(!aigroup) continue;
				if(aigroup.GetAgentsCount() > 0)
				{
					ResourceName res = EPF_Utils.GetPrefabName(aigroup);
					fob.garrison.Insert(res);					
				}
			}	
			m_Camps.Insert(fob);
		}
		
		//Jobs
		
		m_aGlobalJobs = new set<int>;
		m_aTownJobs = new map<int, ref set<int>>;
		m_aBaseJobs = new map<int, ref set<int>>;
		m_aJobCounts = new map<int, int>;
		m_aJobCountPlayerIds = new set<string>;
		m_mPlayerJobCounts = new array<ref map<int, int>>;
		m_aJobs = new array<ref OVT_Job>;
		
		OVT_JobManagerComponent jobs = OVT_Global.GetJobs();
		
		foreach(int id : jobs.m_aGlobalJobs)
		{
			m_aGlobalJobs.Insert(id);
		}
		
		for(int t=0; t<jobs.m_aTownJobs.Count(); t++)
		{
			int townId = jobs.m_aTownJobs.GetKey(t);
			m_aTownJobs[townId] = new set<int>;
			foreach(int id : jobs.m_aTownJobs[townId])
			{
				m_aTownJobs[townId].Insert(id);
			}
		}
		
		for(int t=0; t<jobs.m_aBaseJobs.Count(); t++)
		{
			int baseId = jobs.m_aBaseJobs.GetKey(t);
			m_aBaseJobs[baseId] = new set<int>;
			foreach(int id : jobs.m_aBaseJobs[baseId])
			{
				m_aBaseJobs[baseId].Insert(id);
			}
		}
		
		for(int t=0; t<jobs.m_aJobCounts.Count(); t++)
		{
			int id = jobs.m_aJobCounts.GetKey(t);
			m_aJobCounts[id] = jobs.m_aJobCounts[id];			
		}
		
		for(int t=0; t<jobs.m_mPlayerJobCounts.Count(); t++)
		{
			string playerId = jobs.m_mPlayerJobCounts.GetKey(t);
			m_aJobCountPlayerIds.Insert(playerId);
			map<int, int> counts = new map<int, int>;
			for(int i=0; i<jobs.m_mPlayerJobCounts[playerId].Count(); i++)			
			{
				int id = jobs.m_mPlayerJobCounts[playerId].GetKey(i);				
				counts[id] = jobs.m_mPlayerJobCounts[playerId][id];
			}
			m_mPlayerJobCounts.Insert(counts);
		}
		
		foreach(OVT_Job job : jobs.m_aJobs)
		{
			m_aJobs.Insert(job);
		}
		
		return EPF_EReadResult.OK;
	}

	override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_ResistanceFactionManager resistance = OVT_ResistanceFactionManager.Cast(component);

		if (m_sPlayerFactionKey.IsEmpty())
		{
			Print("Player faction key is invalid, setting to FIA", LogLevel.WARNING);
			m_sPlayerFactionKey = "FIA";
		}

		OVT_Global.GetConfig().m_sPlayerFaction = m_sPlayerFactionKey;
		int playerFactionIndex = GetGame().GetFactionManager().GetFactionIndex(GetGame().GetFactionManager().GetFactionByKey(m_sPlayerFactionKey));
		OVT_Global.GetConfig().m_iPlayerFactionIndex = playerFactionIndex;

		foreach(OVT_CampData fob : m_Camps)
		{	
			fob.id = resistance.m_Camps.Count();			
			resistance.m_Camps.Insert(fob);
		}
		
		//Jobs
		OVT_JobManagerComponent jobs = OVT_Global.GetJobs();
		
		//Backwards save compatibility
		if(!m_aGlobalJobs) return EPF_EApplyResult.OK;
		
		foreach(int id : m_aGlobalJobs)
		{
			jobs.m_aGlobalJobs.Insert(id);
		}
		
		for(int t=0; t<m_aTownJobs.Count(); t++)
		{
			int townId = m_aTownJobs.GetKey(t);
			jobs.m_aTownJobs[townId] = new set<int>;
			foreach(int id : m_aTownJobs[townId])
			{
				jobs.m_aTownJobs[townId].Insert(id);
			}
		}
		
		for(int t=0; t<m_aBaseJobs.Count(); t++)
		{
			int baseId = m_aBaseJobs.GetKey(t);
			jobs.m_aBaseJobs[baseId] = new set<int>;
			foreach(int id : m_aBaseJobs[baseId])
			{
				jobs.m_aBaseJobs[baseId].Insert(id);
			}
		}
		
		for(int t=0; t<m_aJobCounts.Count(); t++)
		{
			int id = m_aJobCounts.GetKey(t);
			jobs.m_aJobCounts[id] = m_aJobCounts[id];			
		}
		
		foreach(int i, string playerId : m_aJobCountPlayerIds)
		{
			jobs.m_mPlayerJobCounts[playerId] = m_mPlayerJobCounts[i];	
		}
		
		foreach(OVT_Job job : m_aJobs)
		{
			jobs.m_aJobs.Insert(job);
		}

		return EPF_EApplyResult.OK;
	}
}