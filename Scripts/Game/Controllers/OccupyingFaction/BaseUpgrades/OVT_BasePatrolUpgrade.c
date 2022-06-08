class OVT_BasePatrolUpgrade : OVT_BaseUpgrade
{
	ref array<ref EntityID> m_Groups;
	
	override void PostInit()
	{
		m_Groups = new array<ref EntityID>;
	}
	
	override int GetResources()
	{
		int res = 0;
		foreach(EntityID id : m_Groups)
		{
			SCR_AIGroup group = GetGroup(id);
			res += group.GetAgentsCount() * m_Config.m_Difficulty.resourcesPerSoldier;			
		}
		return res;
	}
	
	int BuyPatrol()
	{
		return 0;
	}
	
	override int Spend(int resources)
	{
		int spent = 0;
		
		while(resources > 0)
		{			
			int newres = BuyPatrol();
			
			if(newres > resources){
				newres = resources;
				//todo: delete some soldiers when overspending
			}
			
			spent += newres;
			resources -= newres;
		}
		
		return spent;
	}
	
	protected SCR_AIGroup GetGroup(EntityID id)
	{
		IEntity ent = GetGame().GetWorld().FindEntityByID(id);
		return SCR_AIGroup.Cast(ent);
	}
	
	protected AIWaypoint SpawnWaypoint(ResourceName res, vector pos)
	{
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;
		AIWaypoint wp = AIWaypoint.Cast(GetGame().SpawnEntityPrefab(Resource.Load(res), null, params));
		return wp;
	}
	
	protected AIWaypoint SpawnPatrolWaypoint(vector pos)
	{
		AIWaypoint wp = SpawnWaypoint(m_Config.m_pPatrolWaypointPrefab, pos);
		return wp;
	}
	
	protected SCR_EntityWaypoint SpawnGetInWaypoint(IEntity target)
	{
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = target.GetOrigin();
		SCR_EntityWaypoint wp = SCR_EntityWaypoint.Cast(GetGame().SpawnEntityPrefab(Resource.Load(m_Config.m_pGetInWaypointPrefab), null, params));
		
		wp.SetEntity(target);
		
		return wp;
	}
	
	protected SCR_TimedWaypoint SpawnWaitWaypoint(vector pos, float time)
	{
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;
		SCR_TimedWaypoint wp = SCR_TimedWaypoint.Cast(GetGame().SpawnEntityPrefab(Resource.Load(m_Config.m_pWaitWaypointPrefab), null, params));
		
		return wp;
	}
	
	protected SCR_SmartActionWaypoint SpawnActionWaypoint(vector pos, IEntity target, string action)
	{
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;
		SCR_SmartActionWaypoint wp = SCR_SmartActionWaypoint.Cast(GetGame().SpawnEntityPrefab(Resource.Load(m_Config.m_pSmartActionWaypointPrefab), null, params));
		
		wp.SetSmartActionEntity(target, action);
		
		return wp;
	}
}