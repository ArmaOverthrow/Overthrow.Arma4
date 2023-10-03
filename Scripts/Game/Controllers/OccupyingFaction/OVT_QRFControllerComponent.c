class OVT_QRFControllerComponentClass: OVT_ComponentClass
{
};
vector Goodqrfpos = "0 0 0";
vector Goodqrfbasepos = "0 0 0";
class OVT_QRFControllerComponent: OVT_Component
{
	[RplProp()]
	int m_iWinningFaction = -1;
	
	[RplProp()]
	int m_iPoints = 0;
			
	int m_iTimer = 120000;
	
	ref array<int> m_Groups;
	
	protected const int UPDATE_FREQUENCY = 10000;
	const float QRF_RANGE = 750;
	const float QRF_DEPTH = 200;
	const float QRF_POINT_RANGE = 220;
	
	ref ScriptInvoker m_OnFinished = new ScriptInvoker();
	
	int m_iUsedResources = 0;
	
	OVT_OccupyingFactionManager m_OccupyingFaction;
	
	ref array<ResourceName> m_aSpawnQueue = {};
	ref array<vector> m_aSpawnPositions = {};
	ref array<vector> m_aSpawnTargets = {};
	
	ref array<vector> m_Bases = {};
	int m_iResourcesLeft = 0;
	int m_iLZMax = 750;
	int m_iLZMin = 250;
	int m_iPreferredDirection = -1;
	
	override void OnPostInit(IEntity owner)
	{
		m_iPoints = 0;
		m_iWinningFaction = -1;		
		
		super.OnPostInit(owner);
		m_OccupyingFaction = OVT_Global.GetOccupyingFaction();
					
		if(!Replication.IsServer()) return;
		
		GetGame().GetCallqueue().CallLater(CheckUpdateTimer, 1000, true, owner);		
		
		m_Groups = new array<int>;
		Replication.BumpMe();		
		
		GetGame().GetCallqueue().CallLater(CheckUpdatePoints, UPDATE_FREQUENCY, true, owner);		
	}
	
	void Start()
	{
		SendTroops();
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
		OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
		foreach(int id : m_Groups)
		{
			OVT_VirtualizedGroupData data = OVT_VirtualizedGroupData.Get(id);
			if(data && data.isSpawned)
			{			
				IEntity group = world.FindEntityByID(data.spawnedEntity);
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
			}else{
				data.remove = true;
			}
		}
	}
	
	void Cleanup()
	{
		m_aSpawnQueue.Clear();
		m_aSpawnTargets.Clear();
		m_aSpawnPositions.Clear();
		
		BaseWorld world = GetGame().GetWorld();
		foreach(int id : m_Groups)
		{
			OVT_VirtualizedGroupData data = OVT_VirtualizedGroupData.Get(id);
			if(data)
			{	
				data.remove = true;
			}
		}
		
		m_Groups.Clear();
		
		SCR_EntityHelper.DeleteEntityAndChildren(GetOwner());
	}
	
	protected void CheckUpdatePoints()
	{
		BaseWorld world = GetGame().GetWorld();
				
		if(m_iTimer <= 0)
		{
			OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
			
			int enemyNum = virt.GetNumGroupsInRange(GetOwner().GetOrigin(), QRF_POINT_RANGE) * 4;
			int playerNum = 0;
			int enemyTotal = virt.GetNumGroupsInRange(GetOwner().GetOrigin(), QRF_RANGE) * 4;
			
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
		Goodqrfpos = "0 0 0";
		Goodqrfbasepos = "0 0 0";
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
		
		Print("Distance min: " + m_iLZMin.ToString());
		Print("Distance max: " + m_iLZMax.ToString());
		
		vector qrfpos = GetOwner().GetOrigin();
		
		foreach(vector base : m_Bases)
		{
			if(m_iResourcesLeft <= 0) break;
			int allocated = 0;
			
			vector lz = GetLandingZone();
			vector target = GetTargetZone(qrfpos);
			int ii = 0;
			while(allocated < allocate && ii < 6)
			{
				ii++;
				allocated += SpawnTroops(lz, target);
			}
			spent += allocated;
			m_iResourcesLeft -= allocated;
			Print("[Overthrow.QRFControllerComponent] Sent troops from " + lz.ToString() + ": " + allocated.ToString());
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
	
	//Chris Schedule WP
	// Corrected waypoint creation and handling
	protected AIWaypoint CreateWaypoint(string waypointType, vector targetPos) {
	    switch (waypointType) {
	        case "SearchAndDestroy":
	            return OVT_Global.GetConfig().SpawnSearchAndDestroyWaypoint(targetPos);
			case "DefendBase":
	            return OVT_Global.GetConfig().SpawnDefendBaseWaypoint(targetPos);
	        case "GetIn"://nearest
	            return OVT_Global.GetConfig().SpawnGetInWaypoint(targetPos);
	        case "GetOut":
	            return OVT_Global.GetConfig().SpawnGetOutWaypoint(targetPos);
	        case "Loiter":
	            return OVT_Global.GetConfig().SpawnLoiterWaypoint(targetPos);
	        case "Patrol":
	            return OVT_Global.GetConfig().SpawnBasicPatrolWaypoint(targetPos);
	        case "Cycle":
	            return OVT_Global.GetConfig().SpawnLoiterWaypoint(targetPos);
	        case "DefendBase":
	            return OVT_Global.GetConfig().SpawnBasicCycleWaypoint(targetPos);
	        case "Scout":
	            return OVT_Global.GetConfig().SpawnScoutWaypoint(targetPos);
	        case "Defend":
	            return OVT_Global.GetConfig().SpawnDefendWaypoint(targetPos);
	        default:
	            return OVT_Global.GetConfig().SpawnSearchAndDestroyWaypoint(targetPos); // Default case, ensures return
	    }
		return OVT_Global.GetConfig().SpawnSearchAndDestroyWaypoint(targetPos);
	}

	protected void AddWaypoint(vector targetPos, SCR_AIGroup aigroup, string waypointType)
	{
	    if (aigroup) // Check if the AI group still exists
	    {
	        AIWaypoint waypoint = CreateWaypoint(waypointType, targetPos);
	        aigroup.AddWaypoint(waypoint);
	    }
	}
		
	protected void ScheduleWaypoint(vector targetPos, float delay, SCR_AIGroup aigroup, string waypointType)
	{
	    // Correctly scheduling a waypoint addition without assignment
		GetGame().GetCallqueue().CallLater(AddWaypoint, delay * 1000, false, targetPos, aigroup, waypointType);
	}
	//----------------------------------------------------
	protected void SpawnFromQueue()
	{
		if(m_aSpawnQueue.Count() == 0) return;
		
		OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
		
		ResourceName res = m_aSpawnQueue[0];
		vector pos = m_aSpawnPositions[0];
		vector targetPos = m_aSpawnTargets[0];
		
		array<vector> waypoints();
		waypoints.Insert(targetPos);
		
		OVT_VirtualizedGroupData data = virt.Create(res, pos, waypoints, false, OVT_GroupOrder.ATTACK, OVT_GroupSpeed.RUNNING, false);		
		m_Groups.Insert(data.id);		
		
		m_aSpawnQueue.Remove(0);
		m_aSpawnPositions.Remove(0);
		m_aSpawnTargets.Remove(0);
	}

	bool IsZeroVector(vector vec)
	{
    	return vec[0] == 0 && vec[1] == 0 && vec[2] == 0;
	}
	
	protected vector GetRandomDirection()
	{
		float angle = Math.RandomFloatInclusive(0, 359); // Random angle for azimuth		
		if(m_iPreferredDirection > -1)
		{
			float min = m_iPreferredDirection - 30 - 90;
			if(min < 0) min += 360;			
			if(min > 360) min -= 360;
			float max = m_iPreferredDirection + 30 - 90;
			if(max < 0) max += 360;			
			if(max > 360) max -= 360;
			angle = Math.RandomFloatInclusive(min, max);
		}		
	 
		vector dir = {Math.Cos(angle * Math.DEG2RAD), 0, Math.Sin(angle * Math.DEG2RAD)};
	    return dir.Normalized(); // Return a normalized direction vector
	}
	
	protected vector GetTargetZone(vector origin)
	{
	    origin = GetOwner().GetOrigin(); // Starting position
	    float searchRadius = 50.0; // Define the radius
	    int maxAttempts = 450; // Maximum attempts to find a valid position
	    int attempts = 0;
	    vector targetZone;
	    while (attempts < maxAttempts)
	    {
	        attempts++;
	
	        // Generate a random position within the radius
	        targetZone = s_AIRandomGenerator.GenerateRandomPointInRadius(0, searchRadius, origin);
	
	        // Check if the position is not in the ocean
	        if (!OVT_Global.IsOceanAtPosition(targetZone))
	        {
	            Print("[Debug] Found valid target zone: " + targetZone);
				Goodqrfbasepos = targetZone;
	            return targetZone; // Return the valid position
	        }
	    }
		//Reuse any good qrf position if found
		if (!IsZeroVector(Goodqrfbasepos)){return Goodqrfbasepos;}
	    // If no valid position is found, return the original position as fallback
	    Print("[Debug] No valid target zone found. Returning origin as fallback.");
		//
	    return origin;
	}
	
	protected vector GetLandingZone()
	{
		//Reuse any good QRF position if found
		if (!IsZeroVector(Goodqrfpos)){return Goodqrfpos;}
	    vector qrfpos = GetOwner().GetOrigin(); // Position of the QRF
	    vector dir = GetRandomDirection();//dir = vector.Direction(qrfpos, pos);	    
		
		float distance = Math.RandomFloatInclusive(m_iLZMin,m_iLZMax);
		Print("Distance: " + distance.ToString());
	
	    vector checkpos = qrfpos + (dir * distance); 
		vector safepos = checkpos;
	
	    BaseWorld world = GetGame().GetWorld();
	
	    int maxAttempts = 450; // Maximum attempts to find a valid position
	    int attempts = 0;
		
	    while (attempts < maxAttempts)
	    {
	        attempts++;
				
	        // Ensure the position is not in the ocean
	        if (!OVT_Global.IsOceanAtPosition(checkpos))
	        {
				safepos = checkpos;
	            // Check for a clear landing zone (10x10x10)
	            vector mins = "-5 0 -5";
	            vector maxs = "5 10 5";
	            autoptr TraceBox trace = new TraceBox;
	            trace.Flags = TraceFlags.ENTS;
	            trace.Start = checkpos;
	            trace.Mins = mins;
	            trace.Maxs = maxs;
	            trace.Exclude = GetOwner();
	            float result = GetOwner().GetWorld().TracePosition(trace, null);
	            // If a clear LZ is found, return it
	            if (result >= 0)
	            {
					Goodqrfpos = checkpos;
					Print("Found LZ: " + checkpos.ToString());
	                return checkpos;
	            }
	        }
	
	        // Randomize direction and try again
  		    dir = GetRandomDirection(); // Get a new random direction each time
			distance = Math.RandomFloatInclusive(m_iLZMin,m_iLZMax);
			Print("Distance: " + distance.ToString());
      		checkpos = qrfpos + (dir * distance); // Update check position
	    }	    
	    // Default to the last checked position if no better options were found
	    return safepos;
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