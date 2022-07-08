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
		
		EntitySpawnParams spawnParams = new EntitySpawnParams;
		spawnParams.TransformMode = ETransformMode.WORLD;
		
		BaseWorld world = GetGame().GetWorld();
		
		spawnPosition = OVT_Global.FindSafeSpawnPosition(spawnPosition);		
		spawnParams.Transform[3] = spawnPosition;
		
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		
		OVT_Faction faction = config.GetFactionByType(m_Faction);
		
		IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(faction.GetRandomGroupByType(m_GroupType)), world, spawnParams);
		
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if(group)
		{
			config.GivePatrolWaypoints(group, m_PatrolType, job.location);
		}
		
		if(m_bSetAsJobIdentity)
		{
			RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));		
			job.entity = rpl.Id();
		}		
		return false;
	}
}