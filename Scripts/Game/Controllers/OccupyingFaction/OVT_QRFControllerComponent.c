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
	
	int m_iUsedResources = 0;
	
	OVT_OccupyingFactionManager m_OccupyingFaction;
	
	ref array<ResourceName> m_aSpawnQueue = {};
	ref array<vector> m_aSpawnPositions = {};
	ref array<vector> m_aSpawnTargets = {};
	
	ref array<vector> m_Bases = {};
	int m_iResourcesLeft = 0;
	
	override void OnPostInit(IEntity owner)
	{
		m_iPoints = 0;
		m_iWinningFaction = -1;		
		
		super.OnPostInit(owner);
		m_OccupyingFaction = OVT_Global.GetOccupyingFaction();
					
		if(!Replication.IsServer()) return;
		
		GetGame().GetCallqueue().CallLater(CheckUpdateTimer, 1000, true, owner);		
		
		m_Groups = new array<ref EntityID>;
		SendTroops();
		Replication.BumpMe();		
		
		GetGame().GetCallqueue().CallLater(CheckUpdatePoints, UPDATE_FREQUENCY, true, owner);		
	}
	
	protected void CheckUpdateTimer()
	{
		if(m_iTimer < 105000) //Wait 15 seconds for everything to despawn
			SpawnFromQueue();
		
		m_iTimer -= 1000;
		
		if(m_iTimer < 0)
		{
			m_iTimer = 0;
			return;
		}
		
		m_OccupyingFaction.UpdateQRFTimer(m_iTimer);
		Replication.BumpMe();
	}
	
	void KillAll()
	{
		BaseWorld world = GetGame().GetWorld();
		foreach(EntityID id : m_Groups)
		{
			IEntity group = world.FindEntityByID(id);
			if(!group) continue;
			SCR_AIGroup aigroup = SCR_AIGroup.Cast(group);
			if(!aigroup) continue;
			autoptr array<AIAgent> agents = new array<AIAgent>;
			aigroup.GetAgents(agents);
			foreach(AIAgent agent : agents)
			{
				DamageManagerComponent damageManager = DamageManagerComponent.Cast(agent.FindComponent(DamageManagerComponent));
				if (damageManager && damageManager.IsDamageHandlingEnabled())
					damageManager.SetHealthScaled(0);
			}
		}
	}
	
	string GetWinningFactionKey()
	{
		FactionManager fm = GetGame().GetFactionManager();
		return fm.GetFactionByIndex(m_iWinningFaction).GetFactionKey();
	}

	
	protected void CheckUpdatePoints()
	{
		BaseWorld world = GetGame().GetWorld();
				
		if(m_iTimer <= 0)
		{
			int enemyNum = 0;
			int playerNum = 0;
			int enemyTotal = 0;
			
			array<AIAgent> groups();
			GetGame().GetAIWorld().GetAIAgents(groups);
			foreach(AIAgent group : groups)
			{				
				if(!group) continue;
				IEntity entity = group.GetControlledEntity();
				if(!entity) continue;
				SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
				if(!character) continue;
				if(character.GetFactionKey() != OVT_Global.GetConfig().GetOccupyingFactionData().GetFactionKey()) continue;
				float dist = vector.Distance(character.GetOrigin(),GetOwner().GetOrigin());
				if(dist < QRF_POINT_RANGE)
				{
					enemyNum += 1;
				}
				if(dist < QRF_RANGE)
				{
					enemyTotal += 1;
				}	
			}
			
			autoptr array<int> players = new array<int>;
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
						OVT_Global.GetSkills().GiveXP(playerID, 2);
						playerNum++;
					}
				}
			}
			
			if(playerNum > 0 && enemyTotal == 0){
				//push towards resistance fast
				m_iPoints += 5;
			}else{
				if(playerNum == enemyNum)
				{
					//push towards zero
					if(m_iPoints > 0) m_iPoints--;
					if(m_iPoints < 0) m_iPoints++;
				}else{
					if(playerNum > enemyNum)
					{
						//push towards resistance
						m_iPoints++;
					}else{
						//push towards OF
						m_iPoints--;
					}
				}	
			}		
			
			int toWin = m_Config.m_Difficulty.QRFPointsToWin;
			
			if(m_iPoints > toWin) m_iPoints = toWin;
			if(m_iPoints < -toWin) m_iPoints = -toWin;
			
			m_OccupyingFaction.UpdateQRFPoints(m_iPoints);		
			
			if(m_iPoints > 0) m_iWinningFaction = OVT_Global.GetConfig().GetPlayerFactionIndex();
			if(m_iPoints < 0) m_iWinningFaction = OVT_Global.GetConfig().GetOccupyingFactionIndex();
			if(m_iPoints == 0) m_iWinningFaction = -1;
			
			if(m_iPoints >= toWin || m_iPoints <= -toWin)
			{
				//We have a winner		
				m_OnFinished.Invoke();
				GetGame().GetCallqueue().Remove(CheckUpdatePoints);
				GetGame().GetCallqueue().Remove(CheckUpdateTimer);
			}
		}
	}
	
	protected void SendTroops()
	{
		m_aSpawnQueue.Clear();
		m_aSpawnPositions.Clear();
		m_aSpawnTargets.Clear();
		
		vector qrfpos = GetOwner().GetOrigin();
		
		//Get valid bases to use for QRF
		
		foreach(OVT_BaseData data : m_OccupyingFaction.m_Bases)
		{			
			vector pos = data.location;
			float dist = vector.Distance(pos, qrfpos);
			
			if(!data.IsOccupyingFaction()) continue;
			if(dist < 20) continue; //QRF is for this base, ignore it
			
			m_Bases.Insert(pos);
		}
		
		if(m_Bases.Count() == 0)
		{
			Print("[Overthrow.QRFControllerComponent] Final Base Detected");
			//Temporary for when the OF has no bases left but this one
			//To-Do: organize an external force to come from the sea/air
			m_Bases.Insert(qrfpos + "250 0 100");
		}
		
		int resources = m_OccupyingFaction.m_iResources;
		if(resources <= 400) resources = 400; //Emergency resources (minimum size QRF)
		
		int max = OVT_Global.GetConfig().m_Difficulty.maxQRF;
		int numPlayersOnline = GetGame().GetPlayerManager().GetPlayerCount();
		
		//Scale max QRF size by number of players online
		if(numPlayersOnline > 32)
		{
			max *= 6;
		}else if(numPlayersOnline > 24)
		{
			max *= 5;
		}else if(numPlayersOnline > 16)
		{
			max *= 4;
		}else if(numPlayersOnline > 8)
		{
			max *= 3;
		}else if(numPlayersOnline > 4)
		{
			max *= 2;
		}
		
		if(resources > max)
		{
			resources = max;
		}
		
		Print("[Overthrow.QRFControllerComponent] Allocated QRF Size: " + resources.ToString());
		
		m_iResourcesLeft = resources;
		SendWave();		
	}
	
	protected int SendWave()
	{
		int spent = 0;
		int allocate = Math.Floor(m_iResourcesLeft / m_Bases.Count());
		
		if(allocate > (16 * OVT_Global.GetConfig().m_Difficulty.baseResourceCost))
		{
			allocate = 16 * OVT_Global.GetConfig().m_Difficulty.baseResourceCost;
		}
		
		foreach(vector base : m_Bases)
		{
			if(m_iResourcesLeft <= 0) break;
			int allocated = 0;
			
			vector lz = GetLandingZone(base);
			vector target = GetTargetZone();
			int ii = 0;
			while(allocated < allocate && ii < 6)
			{
				ii++;
				allocated += SpawnTroops(lz, target);
			}
			spent += allocated;
			m_iResourcesLeft -= allocated;
			Print("[Overthrow.QRFControllerComponent] Sent wave from " + lz.ToString() + ": " + allocated.ToString());
		}
		
		if(m_iResourcesLeft > 0)
		{
			//leftover resources, schedule another wave
			GetGame().GetCallqueue().CallLater(SendWave, s_AIRandomGenerator.RandInt(240000, 480000));
		}
		
		m_iUsedResources += spent;
		
		Print("[Overthrow.QRFControllerComponent] Wave complete: " + spent.ToString());
		
		return spent;
	}
	
	protected int SpawnTroops(vector pos, vector targetPos)
	{
		OVT_Faction faction = OVT_Global.GetConfig().GetOccupyingFaction();
						
		ResourceName res = faction.m_aGroupPrefabSlots.GetRandomElement();
		
		m_aSpawnQueue.Insert(res);
		m_aSpawnPositions.Insert(pos);
		m_aSpawnTargets.Insert(targetPos);
				
		int newres = 8 * OVT_Global.GetConfig().m_Difficulty.baseResourceCost;
			
		return newres;
	}
	
	protected void SpawnFromQueue()
	{
		if(m_aSpawnQueue.Count() == 0) return;
		
		ResourceName res = m_aSpawnQueue[0];
		vector pos = m_aSpawnPositions[0];
		vector targetPos = m_aSpawnTargets[0];
		
		BaseWorld world = GetGame().GetWorld();
			
		EntitySpawnParams spawnParams = new EntitySpawnParams;
		spawnParams.TransformMode = ETransformMode.WORLD;						
		spawnParams.Transform[3] = pos;
		IEntity group = GetGame().SpawnEntityPrefab(Resource.Load(res), world, spawnParams);
				
		SCR_AIGroup aigroup = SCR_AIGroup.Cast(group);
		m_Groups.Insert(group.GetID());
		
		aigroup.AddWaypoint(OVT_Global.GetConfig().SpawnSearchAndDestroyWaypoint(targetPos));
		aigroup.AddWaypoint(OVT_Global.GetConfig().SpawnDefendWaypoint(targetPos));
		
		m_aSpawnQueue.Remove(0);
		m_aSpawnPositions.Remove(0);
		m_aSpawnTargets.Remove(0);
	}
	
	protected vector GetTargetZone()
	{		
		return OVT_Global.GetRandomNonOceanPositionNear(GetOwner().GetOrigin(),QRF_POINT_RANGE);
	}
	
	protected vector GetLandingZone(vector pos)
	{
		vector qrfpos = GetOwner().GetOrigin();
		vector dir = vector.Direction(qrfpos, pos);		
		dir.Normalize();
		
		float distToPos = vector.Distance(qrfpos, pos);
		
		if(distToPos < QRF_RANGE) return pos; //Just walk, its close
		
		float dist = QRF_RANGE;
		vector checkpos = s_AIRandomGenerator.GenerateRandomPointInRadius(0,50,qrfpos + (dir * dist));
		
		BaseWorld world = GetGame().GetWorld();
		
		int i = 0;
		while(i < 30 && dist < distToPos)
		{
			i++;			
			
			if(!OVT_Global.IsOceanAtPosition(checkpos))
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
			//Randomize the position a little bit
			checkpos = s_AIRandomGenerator.GenerateRandomPointInRadius(0,50,qrfpos + (dir * dist));
		}
		
		if(dist > distToPos) return pos;
						
		return checkpos;
	}
		
	void ~OVT_QRFControllerComponent()
	{
		GetGame().GetCallqueue().Remove(CheckUpdateTimer);
		GetGame().GetCallqueue().Remove(CheckUpdatePoints);
		m_Groups.Clear();
		m_aSpawnQueue.Clear();
		m_aSpawnTargets.Clear();
		m_aSpawnPositions.Clear();
	}
}