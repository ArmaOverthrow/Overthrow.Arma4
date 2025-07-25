class OVT_SpawnCivilianJobStage : OVT_JobStage
{
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Unit Prefab", params: "et")]
	ResourceName m_pPrefab;
	
	override bool OnStart(OVT_Job job)
	{
		vector spawnPosition = job.location;
				
		BaseWorld world = GetGame().GetWorld();
		
		spawnPosition = OVT_Global.FindSafeSpawnPosition(spawnPosition);			
		
		ResourceName prefabToSpawn = m_pPrefab;
		if(prefabToSpawn.IsEmpty())
		{
			// Get officer from the current faction (e.g., occupying faction)
			OVT_Faction faction = OVT_Global.GetConfig().GetOccupyingFaction();
			prefabToSpawn = faction.GetGroupPrefabByName("officer");
		}
		
		IEntity entity = OVT_Global.SpawnEntityPrefab(prefabToSpawn, spawnPosition);
		
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));		
		job.entity = rpl.Id();
		
		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if(fac)
		{
			OVT_Faction occupying = OVT_Global.GetConfig().GetOccupyingFaction();
			fac.SetAffiliatedFactionByKey(occupying.GetFactionKey());
		}
		
		return false;
	}
}