class OVT_QRFControllerComponentClass: OVT_ComponentClass
{
};

class OVT_QRFControllerComponent: OVT_Component
{
	[RplProp()]
	int m_iWinningFaction = -1;
	
	[RplProp()]
	int m_iPoints = 0;
			
	int m_iTimer = 120000;
	
	ref array<ref EntityID> m_Groups;
	
	protected const int UPDATE_FREQUENCY = 10000;
	const int QRF_RANGE = 1000;
	const int QRF_POINT_RANGE = 220;
	
	ref ScriptInvoker m_OnFinished = new ScriptInvoker();
	
	OVT_OccupyingFactionManager m_OccupyingFaction;
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_OccupyingFaction = OVT_Global.GetOccupyingFaction();
		
		GetGame().GetCallqueue().CallLater(CheckUpdateTimer, 1000, true, owner);		
		
		if(!Replication.IsServer()) return;
		
		m_Groups = new array<ref EntityID>;
		SendTroops();		
		
		GetGame().GetCallqueue().CallLater(CheckUpdatePoints, UPDATE_FREQUENCY, true, owner);		
	}
	
	protected void CheckUpdateTimer()
	{
		m_iTimer -= 1000;
		
		m_OccupyingFaction.UpdateQRFTimer(m_iTimer);
		
		if(m_iTimer <= 0)
		{
			m_iTimer = 0;
			GetGame().GetCallqueue().Remove(CheckUpdateTimer);
		}
	}
	
	protected void CheckUpdatePoints()
	{
		BaseWorld world = GetGame().GetWorld();
		int enemyNum = 0;
		int playerNum = 0;
		int enemyTotal = 0;
		
		if(m_iTimer <= 0)
		{
			foreach(EntityID id : m_Groups)
			{
				IEntity group = world.FindEntityByID(id);
				if(!group) continue;
				SCR_AIGroup aigroup = SCR_AIGroup.Cast(group);
				if(!aigroup) continue;
				float dist = vector.Distance(group.GetOrigin(),GetOwner().GetOrigin());
				int num = aigroup.GetAgentsCount();
				if(dist < QRF_POINT_RANGE)
				{
					enemyNum += num;
				}
				enemyTotal += num;
			}
			
			array<int> players = new array<int>;
			PlayerManager mgr = GetGame().GetPlayerManager();
			int numplayers = mgr.GetPlayers(players);
			
			if(numplayers > 0)
			{
				foreach(int playerID : players)
				{
					IEntity player = mgr.GetPlayerControlledEntity(playerID);
					if(!player) continue;
					float distance = vector.Distance(player.GetOrigin(), GetOwner().GetOrigin());
					if(distance < QRF_POINT_RANGE)
					{
						playerNum++;
					}
				}
			}
			
			if(enemyTotal < 2){
				//push towards resistance fast
				m_iPoints += 5;
			}else{
				if(playerNum == 0 && enemyNum == 0)
				{
					//push towards zero
					if(m_iPoints > 0) m_iPoints--;
					if(m_iPoints < 0) m_iPoints++;
				}else{
					if(playerNum > 0 && enemyNum < 5)
					{
						//push towards resistance
						m_iPoints++;
					}else{
						//push towards OF
						m_iPoints--;
					}
				}	
			}		
			
			if(m_iPoints > 100) m_iPoints = 100;
			
			m_OccupyingFaction.UpdateQRFPoints(m_iPoints);		
			
			if(m_iPoints > 0) m_iWinningFaction = m_Config.GetPlayerFactionIndex();
			if(m_iPoints < 0) m_iWinningFaction = m_Config.GetOccupyingFactionIndex();
			if(m_iPoints == 0) m_iWinningFaction = -1;
			
			if(m_iPoints >= 100 || m_iPoints <= -100)
			{
				//We have a winner		
				m_OnFinished.Invoke();
				GetGame().GetCallqueue().Remove(CheckUpdatePoints);
			}
		}
	}
	
	protected void SendTroops()
	{
		vector qrfpos = GetOwner().GetOrigin();
		
		//Get valid bases to use for QRF
		SCR_SortedArray<OVT_BaseControllerComponent> bases = new SCR_SortedArray<OVT_BaseControllerComponent>;
		foreach(OVT_BaseData data : m_OccupyingFaction.m_Bases)
		{			
			OVT_BaseControllerComponent base = m_OccupyingFaction.GetBase(data.entId);
			vector pos = base.GetOwner().GetOrigin();
			float dist = vector.Distance(pos, qrfpos);
			
			if(!base.IsOccupyingFaction()) continue;
			if(dist < 20) continue; //QRF is for this base, ignore it
			
			bases.Insert(dist, base);
		}
		
		int resources = m_OccupyingFaction.m_iResources;
		if(resources <= 200) resources = 200; //Emergency resources (minimum size QRF)
		
		if(resources > m_Config.m_Difficulty.maxQRF)
		{
			resources = m_Config.m_Difficulty.maxQRF;
		}
		
		int spent = 0;
		int allocate = Math.Floor(resources / bases.Count());
		
		for(int i = bases.Count() - 1; i>=0; i--)
		{
			if(resources <= 0) break;
			int allocated = 0;
			OVT_BaseControllerComponent base = bases[i];
			vector lz = GetLandingZone(base.GetOwner().GetOrigin());
			vector target = GetTargetZone(lz);
			int ii = 0;
			while(allocated < allocate && ii < 6)
			{
				ii++;
				allocated += SpawnTroops(lz, target);
			}
			spent += allocated;
			resources -= allocated;
		}
		
		m_OccupyingFaction.m_iResources = m_OccupyingFaction.m_iResources - spent;
		if(m_OccupyingFaction.m_iResources < 0) m_OccupyingFaction.m_iResources = 0;
	}
	
	protected int SpawnTroops(vector pos, vector targetPos)
	{
		OVT_Faction faction = m_Config.GetOccupyingFaction();
						
		ResourceName res = faction.m_aGroupPrefabSlots.GetRandomElement();
				
		BaseWorld world = GetGame().GetWorld();
			
		EntitySpawnParams spawnParams = new EntitySpawnParams;
		spawnParams.TransformMode = ETransformMode.WORLD;						
		spawnParams.Transform[3] = pos;
		IEntity group = GetGame().SpawnEntityPrefab(Resource.Load(res), world, spawnParams);
				
		SCR_AIGroup aigroup = SCR_AIGroup.Cast(group);
		m_Groups.Insert(group.GetID());
		
		aigroup.AddWaypoint(SpawnMoveWaypoint(targetPos));
				
		int newres = aigroup.m_aUnitPrefabSlots.Count() * m_Config.m_Difficulty.baseResourceCost;
			
		return newres;
	}
	
	protected vector GetTargetZone(vector pos)
	{
		vector qrfpos = GetOwner().GetOrigin();
		vector dir = vector.Direction(qrfpos, pos);		
		dir.Normalize();
		return qrfpos + (dir * (QRF_POINT_RANGE * 0.05));
	}
	
	protected vector GetLandingZone(vector pos)
	{
		vector qrfpos = GetOwner().GetOrigin();
		vector dir = vector.Direction(qrfpos, pos);		
		dir.Normalize();
		
		float distToPos = vector.Distance(qrfpos, pos);
		
		if(distToPos < QRF_RANGE) return pos; //Just walk, its close
		
		float dist = QRF_RANGE;
		vector checkpos = qrfpos + (dir * dist);
		
		BaseWorld world = GetGame().GetWorld();
		
		int i = 0;
		while(i < 20 && dist < distToPos)
		{
			i++;	
			
			checkpos[1] = world.GetSurfaceY(checkpos[0],checkpos[2]);
			//Check is not ocean
			if(world.GetOceanHeight(checkpos[0], checkpos[2]) < checkpos[1])
			{		
				//Check for clear LZ (20x20x20)
				vector mins = "-10 0 -10";
				vector maxs = "10 20 10";
				autoptr TraceBox trace = new TraceBox;
				trace.Flags = TraceFlags.ENTS;
				trace.Start = checkpos;
				trace.Mins = mins;
				trace.Maxs = maxs;
				trace.Exclude = GetOwner();
				
				float result = GetOwner().GetWorld().TracePosition(trace, null);
					
				if (result >= 0)
				{				
					break;
				}
			}
			
			dist += 20;
			checkpos = qrfpos + (dir * dist);
		}
		
		if(dist > distToPos) return pos;
				
		return checkpos;
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
	
	protected AIWaypoint SpawnMoveWaypoint(vector pos)
	{
		AIWaypoint wp = SpawnWaypoint(m_Config.m_pMoveWaypointPrefab, pos);
		return wp;
	}
	
	void ~OVT_QRFControllerComponent()
	{
		GetGame().GetCallqueue().Remove(CheckUpdateTimer);
		GetGame().GetCallqueue().Remove(CheckUpdatePoints);
	}
}