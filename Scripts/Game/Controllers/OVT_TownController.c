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
		
		m_TownManager = OVT_TownManagerComponent.GetInstance();
		m_Economy = OVT_EconomyManagerComponent.GetInstance();
		m_Town = m_TownManager.GetNearestTown(GetOwner().GetOrigin());
		
		if(m_Town.size > 1)
			SpawnGunDealer();
	}
	
	protected void SpawnGunDealer()
	{
		//Print("Spawning gun dealer in " + m_Town.name);
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
			if(!m_Economy.HasPrice(item.prefab))
			{
				m_Economy.SetPrice(item.prefab, item.cost);
			}
			int num = Math.Round(s_AIRandomGenerator.RandFloatXY(1,item.maxAtStart));
			
			shop.AddToInventory(item.prefab, num);
			
			shop.m_aInventoryItems.Insert(item);
		}
		
		m_Economy.RegisterGunDealer(m_GunDealerID);
	}
}