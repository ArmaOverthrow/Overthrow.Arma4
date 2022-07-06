class OVT_BaseUpgradeCheckpoints : OVT_BasePatrolUpgrade
{		
	protected ref array<ref EntityID> m_aSlotsFilled;
	protected IEntity m_SpawnedCheckpoint;
	
	override void PostInit()
	{
		super.PostInit();
		
		m_aSlotsFilled = new array<ref EntityID>;
	}
	
	override int Spend(int resources, float threat)
	{
		int spent = 0;
		
		foreach(EntityID id : m_BaseController.m_LargeRoadSlots)
		{
			if(m_aSlotsFilled.Contains(id)) continue;
			if(resources < 60) break;
			IEntity slot = GetGame().GetWorld().FindEntityByID(id);
			spent += 60;
			resources -= 60;
			m_SpawnedCheckpoint = SpawnCheckpoint(slot, m_Faction.m_aLargeCheckpointPrefab);
			m_aSlotsFilled.Insert(id);
			
			if(resources < (m_Config.m_Difficulty.baseResourceCost * 4)) break;
			int newres = BuyPatrol(threat, m_Faction.m_aGroupInfantryPrefabSlots[0], slot.GetOrigin());
			spent += newres;
			resources -= newres;
		}
		
		foreach(EntityID id : m_BaseController.m_MediumRoadSlots)
		{
			if(m_aSlotsFilled.Contains(id)) continue;
			if(resources < 40) break;
			IEntity slot = GetGame().GetWorld().FindEntityByID(id);
			spent += 40;
			resources -= 40;
			m_SpawnedCheckpoint = SpawnCheckpoint(slot, m_Faction.m_aMediumCheckpointPrefab);
			if(!m_SpawnedCheckpoint) continue;
			
			m_aSlotsFilled.Insert(id);
			
			if(resources < (m_Config.m_Difficulty.baseResourceCost * 4)) break;
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
		aigroup.AddWaypoint(m_Config.SpawnDefendWaypoint(m_SpawnedCheckpoint.GetOrigin()));
	}
	
	protected IEntity SpawnCheckpoint(IEntity slot, ResourceName res)
	{
		EntitySpawnParams spawn_params = EntitySpawnParams();
		spawn_params.TransformMode = ETransformMode.WORLD;
		vector mat[4];
		slot.GetTransform(mat);
		spawn_params.Transform = mat;		
		IEntity ent = GetGame().SpawnEntityPrefab(Resource.Load(res), GetGame().GetWorld(), spawn_params);
		return ent;
	}
	
	void ~OVT_BaseUpgradeCheckpoints()
	{
		if(m_aSlotsFilled)
		{
			m_aSlotsFilled.Clear();
			m_aSlotsFilled = null;
		}
	}
}