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
	
	[Attribute("")]
	string m_sGroupName;
	
	override bool OnStart(OVT_Job job)
	{
		vector spawnPosition = job.location + m_vPositionOffset;
		
		BaseWorld world = GetGame().GetWorld();
		
		spawnPosition = OVT_Global.FindSafeSpawnPosition(spawnPosition);	
		
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		
		OVT_Faction faction = config.GetFactionByType(m_Faction);
		ResourceName groupPrefab;
		if(m_GroupType == OVT_GroupType.SPECIAL_FORCES && m_sGroupName == "")
		{
			groupPrefab = faction.GetGroupPrefabByName("special_forces");
		}else if(m_sGroupName != ""){
			groupPrefab = faction.GetGroupPrefabByName(m_sGroupName);
		}else{
			groupPrefab = faction.GetRandomGroupByType(m_GroupType);
		}
		
		IEntity entity = OVT_Global.SpawnEntityPrefab(groupPrefab, spawnPosition);
		
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