class OVT_TownControllerComponentClass: OVT_ComponentClass
{
};

class OVT_TownControllerComponent: OVT_Component
{
	[Attribute("")]
	string m_sName;
	
	[Attribute("1", UIWidgets.ComboBox, "Town size", "", ParamEnumArray.FromEnum(OVT_TownSize) )]
	OVT_TownSize m_Size;	
		
	[Attribute("400", desc:"Target population, and population at game start")]
	int m_iPopulation;
	
	[Attribute("800", UIWidgets.Slider, "Range to spawn civilians", "50 500 10")]
	int m_iTownRange;
	
	[Attribute("400", UIWidgets.Slider, "Minimum distance to spawn QRF", "50 1000 25")]
	int m_iAttackDistanceMin;
	
	[Attribute("800", UIWidgets.Slider, "Maximum distance to spawn QRF", "100 1000 25")]
	int m_iAttackDistanceMax;
	
	[Attribute("-1", UIWidgets.Slider, "Preferred direction to spawn QRF (randomized slightly, -1 means any direction)", "-1 359 1")]
	int m_iAttackPreferredDirection;
	
	protected OVT_TownManagerComponent m_TownManager;
	protected OVT_EconomyManagerComponent m_Economy;
	protected OVT_TownData m_Town;
	protected EntityID m_GunDealerID;

	protected bool m_bCiviliansSpawned;

	protected ref array<ref EntityID> m_aCivilians;
	
#ifdef WORKBENCH
	protected ref Shape m_aRangeShape;
	protected ref Shape m_aDirectionArrow;
	
	//Draw town range as a sphere and attack direction as an arrow
	override int _WB_GetAfterWorldUpdateSpecs(IEntity owner, IEntitySource src)
	{
		return EEntityFrameUpdateSpecs.CALL_WHEN_ENTITY_SELECTED;
	}
	
	protected override void _WB_AfterWorldUpdate(IEntity owner, float timeSlice)
	{		
		m_aRangeShape = Shape.CreateSphere(Color.FromRGBA(255,255,255,20).PackToInt(), ShapeFlags.TRANSP | ShapeFlags.NOOUTLINE, owner.GetOrigin(), (float)m_iTownRange);
		
		// Draw attack preferred direction arrow if set
		if (m_iAttackPreferredDirection != -1)
		{
			vector townPos = owner.GetOrigin();
			// Convert to radians directly - preferred direction is already in compass bearings (0° = North)
			float directionRad = m_iAttackPreferredDirection * Math.DEG2RAD;
			
			// Calculate arrow start and end points
			// In Arma: X = East, Z = South
			// For compass bearings: 0° = North, 90° = East, 180° = South, 270° = West
			// North = -Z, East = +X, South = +Z, West = -X
			vector from = townPos + Vector(Math.Sin(directionRad) * m_iAttackDistanceMax, 0, -Math.Cos(directionRad) * m_iAttackDistanceMax);
			vector to = townPos + Vector(Math.Sin(directionRad) * m_iAttackDistanceMin, 0, -Math.Cos(directionRad) * m_iAttackDistanceMin);
			
			// Draw arrow with semi-transparent blue color (to distinguish from base arrows)
			m_aDirectionArrow = Shape.CreateArrow(from, to, 5, Color.FromRGBA(0, 0, 255, 255).PackToInt(), ShapeFlags.ONCE | ShapeFlags.NOZBUFFER | ShapeFlags.TRANSP | ShapeFlags.DOUBLESIDE | ShapeFlags.NOOUTLINE);
		}
		
		super._WB_AfterWorldUpdate(owner, timeSlice);
	}
#endif

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode()) return;

		m_TownManager = OVT_Global.GetTowns();
		m_Economy = OVT_Global.GetEconomy();
		m_Town = m_TownManager.GetNearestTown(GetOwner().GetOrigin());
		m_aCivilians = new array<ref EntityID>;
		
	}
	
	void ActivateTown()
	{
		m_Town = m_TownManager.GetNearestTown(GetOwner().GetOrigin());
		
		if(m_Town.size != OVT_TownSize.VILLAGE)
			GetGame().GetCallqueue().CallLater(SpawnGunDealer, 0);

		GetGame().GetCallqueue().CallLater(CheckSpawnCivilian, 10000, true);

		CheckSpawnCivilian();
	}
	

	protected void CheckSpawnCivilian()
	{
		bool inrange = OVT_Global.PlayerInRange(m_Town.location, OVT_Global.GetConfig().m_iCivilianSpawnDistance) && !OVT_Global.GetOccupyingFaction().m_CurrentQRF;
		if(m_bCiviliansSpawned && !inrange)
		{
			DespawnCivilians();
		}else if(!m_bCiviliansSpawned && inrange){
			SpawnCivilians();
		}
	}

	protected void DespawnCivilians()
	{
		m_bCiviliansSpawned = false;
		foreach(EntityID id : m_aCivilians)
		{
			SCR_AIGroup aigroup = GetGroup(id);
			SCR_EntityHelper.DeleteEntityAndChildren(aigroup);
		}

		m_aCivilians.Clear();
	}

	protected SCR_AIGroup GetGroup(EntityID id)
	{
		IEntity ent = GetGame().GetWorld().FindEntityByID(id);
		return SCR_AIGroup.Cast(ent);
	}

	protected void SpawnCivilians()
	{
		if(m_bCiviliansSpawned) return;
		m_bCiviliansSpawned = true;
		int numciv = Math.Round((float)m_Town.population * OVT_Global.GetConfig().m_fCivilianSpawnRate);

		for(int i=0; i<numciv; i++)
		{
			SpawnCivilian();
		}
	}

	protected void SpawnCivilian()
	{
		vector spawnPosition = OVT_Global.GetRandomNonOceanPositionNear(m_Town.location, m_iTownRange);

		vector targetPos = OVT_Global.GetRandomNonOceanPositionNear(m_Town.location, m_iTownRange);
		targetPos = OVT_Global.FindNearestRoad(targetPos);
		
		BaseWorld world = GetGame().GetWorld();

		spawnPosition = OVT_Global.FindNearestRoad(spawnPosition);
		IEntity civ = OVT_Global.SpawnEntityPrefab(OVT_Global.GetConfig().m_pCivilianPrefab, spawnPosition);

		EntityID civId = civ.GetID();

		m_aCivilians.Insert(civId);

		SCR_AIGroup aigroup = SCR_AIGroup.Cast(civ);
		aigroup.GetOnAgentAdded().Insert(OVT_Global.RandomizeCivilianClothes);
		aigroup.GetOnAgentAdded().Insert(DisableCivilianWantedSystem);
		
		array<AIWaypoint> queueOfWaypoints = new array<AIWaypoint>();

		queueOfWaypoints.Insert(OVT_Global.GetConfig().SpawnPatrolWaypoint(targetPos));
		queueOfWaypoints.Insert(OVT_Global.GetConfig().SpawnWaitWaypoint(targetPos, s_AIRandomGenerator.RandFloatXY(15, 50)));

		queueOfWaypoints.Insert(OVT_Global.GetConfig().SpawnPatrolWaypoint(spawnPosition));
		queueOfWaypoints.Insert(OVT_Global.GetConfig().SpawnWaitWaypoint(spawnPosition, s_AIRandomGenerator.RandFloatXY(15, 50)));

		AIWaypointCycle cycle = AIWaypointCycle.Cast(OVT_Global.GetConfig().SpawnWaypoint(OVT_Global.GetConfig().m_pCycleWaypointPrefab, targetPos));
		cycle.SetWaypoints(queueOfWaypoints);
		cycle.SetRerunCounter(-1);
		aigroup.AddWaypoint(cycle);
	}
	
	//! Callback to disable wanted system for newly spawned civilians
	protected void DisableCivilianWantedSystem(IEntity characterEntity)
	{
		if (!characterEntity)
			return;
			
		OVT_PlayerWantedComponent wantedComp = OVT_PlayerWantedComponent.Cast(characterEntity.FindComponent(OVT_PlayerWantedComponent));
		if (wantedComp)
		{
			wantedComp.DisableWantedSystem();
		}
	}

	protected void SpawnGunDealer()
	{
		vector spawnPosition;

		if(m_Town.gunDealerPosition && m_Town.gunDealerPosition[0] != 0)
		{
			spawnPosition = m_Town.gunDealerPosition;
		}else{
			IEntity house = m_TownManager.GetRandomHouseInTown(m_Town);
			spawnPosition = house.GetOrigin();
		}

		BaseWorld world = GetGame().GetWorld();

		spawnPosition = OVT_Global.FindSafeSpawnPosition(spawnPosition);

		IEntity dealer = OVT_Global.SpawnEntityPrefab(OVT_Global.GetConfig().m_pGunDealerPrefab, spawnPosition);

		m_GunDealerID = dealer.GetID();

		m_Town.gunDealerPosition = spawnPosition;

		int townID = m_TownManager.GetTownID(m_Town);

		OVT_ShopComponent shop = OVT_ShopComponent.Cast(dealer.FindComponent(OVT_ShopComponent));
		shop.m_iTownId = townID;

		foreach(OVT_PrefabItemCostConfig item : m_Economy.m_GunDealerConfig.m_aGunDealerItemPrefabs)
		{
			int id = m_Economy.GetInventoryId(item.m_sEntityPrefab);
			int num = Math.Round(s_AIRandomGenerator.RandInt(item.minStock,item.maxStock));

			shop.AddToInventory(id, num);
		}

		int occupyingFactionId = OVT_Global.GetConfig().GetOccupyingFactionIndex();
		int supportingFactionId = OVT_Global.GetConfig().GetSupportingFactionIndex();

		foreach(OVT_ShopInventoryItem item : m_Economy.m_GunDealerConfig.m_aGunDealerItems)
		{
			array<SCR_EntityCatalogEntry> entries();
			m_Economy.FindInventoryItems(item.m_eItemType, item.m_eItemMode, item.m_sFind, entries);

			if(item.m_bSingleRandomItem)
			{
				SCR_EntityCatalogEntry found;
				int t = 0;
				while(!entries.IsEmpty() && !found && t < 20)
				{
					t++;
					int index = s_AIRandomGenerator.RandInt(0,entries.Count()-1);
					SCR_EntityCatalogEntry check = entries[index];
					int id = m_Economy.GetInventoryId(check.GetPrefab());
					if(!item.m_bIncludeOccupyingFactionItems && m_Economy.ItemIsFromFaction(id, occupyingFactionId)) continue;
					if(!item.m_bIncludeSupportingFactionItems && m_Economy.ItemIsFromFaction(id, supportingFactionId)) continue;
					if(!item.m_bIncludeOtherFactionItems) continue;
					found = check;
				}
				if(found)
				{
					int id = m_Economy.GetInventoryId(found.GetPrefab());
					int num = Math.Round(s_AIRandomGenerator.RandFloatXY(1,m_Economy.GetTownMaxStock(townID, id)));
					
					shop.AddToInventory(id, num);
				}
			}else{
				foreach(SCR_EntityCatalogEntry entry : entries)
				{
					int id = m_Economy.GetInventoryId(entry.GetPrefab());
					if(!item.m_bIncludeOccupyingFactionItems && m_Economy.ItemIsFromFaction(id, occupyingFactionId)) continue;
					if(!item.m_bIncludeSupportingFactionItems && m_Economy.ItemIsFromFaction(id, supportingFactionId)) continue;
					if(!item.m_bIncludeOtherFactionItems) continue;

					int num = Math.Round(s_AIRandomGenerator.RandFloatXY(1,m_Economy.GetTownMaxStock(townID, id)));

					shop.AddToInventory(id, num);
				}
			}
		}

		GetGame().GetCallqueue().CallLater(SetDealerFaction, 500);

		m_Economy.RegisterGunDealer(m_GunDealerID);
	}

	protected void SetDealerFaction()
	{
		IEntity dealer = GetGame().GetWorld().FindEntityByID(m_GunDealerID);
		FactionAffiliationComponent faction = FactionAffiliationComponent.Cast(dealer.FindComponent(FactionAffiliationComponent));
		if(!faction){
			Print("Gun dealer spawn prefab is missing FactionAffiliationComponent!");
		}else{
			faction.SetAffiliatedFactionByKey("");
		}
	}
}
