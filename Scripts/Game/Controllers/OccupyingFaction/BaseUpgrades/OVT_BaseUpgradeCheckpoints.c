class OVT_BaseUpgradeCheckpoints : OVT_BasePatrolUpgrade
{		
	protected IEntity m_SpawnedCheckpoint;
		
	override int Spend(int resources, float threat)
	{
		int spent = 0;

		foreach(EntityID id : m_BaseController.m_LargeRoadSlots)
		{
			if(m_BaseController.m_aSlotsFilled.Contains(id)) continue;
			if(resources < 60) break;
			IEntity slot = GetGame().GetWorld().FindEntityByID(id);
			if(!slot) continue;

			// Only pay and fill the slot after a successful spawn — a failed spawn must not
			// burn resources or permanently block the slot
			m_SpawnedCheckpoint = SpawnCheckpoint(slot, m_Faction.m_aLargeCheckpointPrefab);
			if(!m_SpawnedCheckpoint) continue;

			spent += 60;
			resources -= 60;
			m_BaseController.m_aSlotsFilled.Insert(id);

			if(resources < (OVT_Global.GetConfig().m_Difficulty.baseResourceCost * 4)) break;
			int newres = BuyPatrol(threat, m_Faction.m_aGroupInfantryPrefabSlots[0], slot.GetOrigin());
			spent += newres;
			resources -= newres;
		}

		foreach(EntityID id : m_BaseController.m_MediumRoadSlots)
		{
			if(m_BaseController.m_aSlotsFilled.Contains(id)) continue;
			if(resources < 40) break;
			IEntity slot = GetGame().GetWorld().FindEntityByID(id);
			if(!slot) continue;

			m_SpawnedCheckpoint = SpawnCheckpoint(slot, m_Faction.m_aMediumCheckpointPrefab);
			if(!m_SpawnedCheckpoint) continue;

			spent += 40;
			resources -= 40;
			m_BaseController.m_aSlotsFilled.Insert(id);

			if(resources < (OVT_Global.GetConfig().m_Difficulty.baseResourceCost * 4)) break;
			int newres = BuyPatrol(threat, m_Faction.m_aGroupInfantryPrefabSlots[0], slot.GetOrigin());
			spent += newres;
			resources -= newres;
		}

		return spent;
	}
	
	override void AddWaypoints(SCR_AIGroup aigroup)
	{
		if(!m_SpawnedCheckpoint) return;
		if(!aigroup) return;
		aigroup.AddWaypoint(OVT_Global.GetConfig().SpawnDefendWaypoint(m_SpawnedCheckpoint.GetOrigin()));
	}
	
	protected IEntity SpawnCheckpoint(IEntity slot, ResourceName res)
	{
		vector mat[4];
		slot.GetTransform(mat);
		IEntity ent = OVT_Global.SpawnEntityPrefabMatrix(res, mat);

		// Include the checkpoint composition in save points, like OVT_SlottedBaseUpgrade.SpawnInSlot.
		// The manager save restores m_aSlotsFilled, so without tracking the structures vanish on
		// load while their road slots stay marked occupied — blocking rebuild forever. The
		// checkpoint prefabs descend from CompositionBase.et, which vanilla's Composition.conf
		// already configures; tracking is the only missing half.
		OVT_PersistenceTracking.Track(ent);

		return ent;
	}
}