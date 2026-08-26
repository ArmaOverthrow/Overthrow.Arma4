class OVT_SpawnGroupJobStage : OVT_JobStage
{
	[Attribute("1", UIWidgets.ComboBox, "Faction type", "", ParamEnumArray.FromEnum(OVT_FactionType) )]
	OVT_FactionType m_Faction;
	
	[Attribute("1", UIWidgets.ComboBox, "Group type", "", ParamEnumArray.FromEnum(OVT_GroupType) )]
	OVT_GroupType m_GroupType;
	
	[Attribute("1", UIWidgets.ComboBox, "Patrol type", "", ParamEnumArray.FromEnum(OVT_PatrolType) )]
	OVT_PatrolType m_PatrolType;
	
	[Attribute()]
	vector m_vPositionOffset;
	
	[Attribute()]
	bool m_bSetAsJobIdentity;
	
	override bool OnStart(OVT_Job job)
	{
		vector spawnPosition = job.location + m_vPositionOffset;
		
		BaseWorld world = GetGame().GetWorld();
		
		spawnPosition = OVT_WorldUtils.FindSafeSpawnPosition(spawnPosition, "-0.5 0 -0.5", "0.5 2 0.5", true);	
		
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		
		OVT_Faction faction = config.GetFactionByType(m_Faction);
		
		// Resolved through the faction's GROUP REGISTRY (OVT_Faction.GetGroupPrefabByType). The legacy
		// prefab-slot arrays this used to read were retired with the base-defense migration; the enum
		// stays because it is this stage's authored surface, and the faction maps it to a registry name.
		IEntity entity = OVT_Global.SpawnEntityPrefab(faction.GetGroupPrefabByType(m_GroupType), spawnPosition);
		
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if(group)
		{
			config.GivePatrolWaypoints(group, m_PatrolType, job.location);
		}
		
		// GM group registry. Index -1: a job's town/base ids live on the job record, which is
		// already replicated; the reason string carries the job's stable identity instead.
		OVT_GMGroupRegistry.Tag(entity, OVT_EGroupOrigin.JOB, -1, GetJobOriginReason(job));
		
		if(m_bSetAsJobIdentity)
		{
			RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));		
			job.entity = rpl.Id();
		}		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Stable identity of the job config that owns this stage, for the GM group registry.
	//!
	//! Every dereference is guarded and the fallback is a constant: this runs inside a spawn path
	//! whose behaviour must not change, so it must be incapable of throwing.
	//! \param[in] job The job being started.
	//! \return The config's m_sId, or "Job" when it cannot be resolved.
	protected string GetJobOriginReason(OVT_Job job)
	{
		if(!job) return "Job";
		
		OVT_JobManagerComponent jobs = OVT_Global.GetJobs();
		if(!jobs) return "Job";
		if(job.jobIndex < 0 || job.jobIndex >= jobs.GetJobConfigCount()) return "Job";
		
		OVT_JobConfig jobConfig = jobs.GetConfig(job.jobIndex);
		if(!jobConfig) return "Job";
		
		return jobConfig.m_sId;
	}
}