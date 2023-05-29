class OVT_SpawnCivilianJobStage : OVT_JobStage
{
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Unit Prefab", params: "et")]
	ResourceName m_pPrefab;
	
	override bool OnStart(OVT_Job job)
	{
		vector spawnPosition = job.location;
				
		BaseWorld world = GetGame().GetWorld();
		
		spawnPosition = OVT_Global.FindSafeSpawnPosition(spawnPosition);			
		
		IEntity entity = OVT_Global.SpawnEntityPrefab(m_pPrefab, spawnPosition);
		
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));		
		job.entity = rpl.Id();
		
		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if(fac)
		{
			fac.SetAffiliatedFactionByKey("");
		}
		
		return false;
	}
}