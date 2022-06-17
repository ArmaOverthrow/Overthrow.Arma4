class OVT_TownControllerComponentClass: OVT_ComponentClass
{
};

class OVT_TownControllerComponent: OVT_Component
{
	protected OVT_TownManagerComponent m_TownManager;
	protected OVT_EconomyManagerComponent m_Economy;
	
	protected OVT_TownData m_Town;
	protected EntityID m_GunDealerID;
	
	override void OnPostInit(IEntity owner)
	{	
		super.OnPostInit(owner);
		
		if (SCR_Global.IsEditMode()) return;
		
		m_TownManager = OVT_Global.GetTowns();
		m_Economy = OVT_Global.GetEconomy();
		m_Town = m_TownManager.GetNearestTown(GetOwner().GetOrigin());
		
		if(m_Town.size > 1)
			GetGame().GetCallqueue().CallLater(SpawnGunDealer, 0);
	}
	
	protected void SpawnGunDealer()
	{
		
		IEntity house = m_TownManager.GetRandomHouseInTown(m_Town);
		
		vector spawnPosition = house.GetOrigin();
		
		EntitySpawnParams spawnParams = new EntitySpawnParams;
		spawnParams.TransformMode = ETransformMode.WORLD;
		
		BaseWorld world = GetGame().GetWorld();
		
		//Snap to the nearest navmesh point
		AIPathfindingComponent pathFindindingComponent = AIPathfindingComponent.Cast(this.FindComponent(AIPathfindingComponent));
		if (pathFindindingComponent && pathFindindingComponent.GetClosestPositionOnNavmesh(spawnPosition, "10 10 10", spawnPosition))
		{
			float groundHeight = world.GetSurfaceY(spawnPosition[0], spawnPosition[2]);
			if (spawnPosition[1] < groundHeight)
				spawnPosition[1] = groundHeight;
		}
		
		spawnParams.Transform[3] = spawnPosition;
		IEntity dealer = GetGame().SpawnEntityPrefab(Resource.Load(m_Config.m_pGunDealerPrefab), world, spawnParams);
		
		m_GunDealerID = dealer.GetID();
		
		OVT_ShopComponent shop = OVT_ShopComponent.Cast(dealer.FindComponent(OVT_ShopComponent));
		
		foreach(OVT_ShopInventoryItem item : m_Economy.m_aGunDealerItems)
		{
			RplId id = m_Economy.GetInventoryId(item.prefab);
			if(!m_Economy.HasPrice(id))
			{
				m_Economy.SetPrice(id, item.cost);
			}
			int num = Math.Round(s_AIRandomGenerator.RandFloatXY(1,item.maxAtStart));
			
			shop.AddToInventory(id, num);
			
			shop.m_aInventoryItems.Insert(item);
		}
		
		FactionAffiliationComponent faction = FactionAffiliationComponent.Cast(dealer.FindComponent(FactionAffiliationComponent));
		if(!faction){
			Print("Gun dealer spawn prefab is missing FactionAffiliationComponent!");
		}else{
			faction.SetAffiliatedFactionByKey("");
		}
		
		m_Economy.RegisterGunDealer(m_GunDealerID);
	}
}