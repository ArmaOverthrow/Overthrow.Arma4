class OVT_SpawnCivilianJobStage : OVT_JobStage
{
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Unit Prefab", params: "et")]
	ResourceName m_pPrefab;
	
	override bool OnStart(OVT_Job job)
	{
		vector spawnPosition = job.location;
		
		EntitySpawnParams spawnParams = new EntitySpawnParams;
		spawnParams.TransformMode = ETransformMode.WORLD;
		
		BaseWorld world = GetGame().GetWorld();
		
		spawnPosition = OVT_Global.FindSafeSpawnPosition(spawnPosition);		
		spawnParams.Transform[3] = spawnPosition;
		
		IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(m_pPrefab), world, spawnParams);
		
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