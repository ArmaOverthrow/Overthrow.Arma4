class OVT_VirtualizationManagerComponentClass: OVT_ComponentClass
{
};

enum OVT_GroupOrder
{
	PATROL,		//Will walk to each waypoint and execute a search & destroy
	ATTACK		//Will run to each waypoint and execute a search & destroy
}

enum OVT_GroupSpeed
{
	WALKING=2,
	RUNNING=4,
	DRIVING_SLOW=10,
	DRIVING_FAST=22,
	FLYING_SLOW=55,
	FLYING_FAST=80
}

class OVT_VirtualizedGroupData : EPF_PersistentScriptedState
{
	int id;					//
	vector pos;				//Current position
	ResourceName prefab; 	//Group prefab
	ref array<vector> waypoints = new array<vector>; //the current waypoints for this group
	OVT_GroupOrder order;	//Current order for this group (determines what type of waypoints are spawned)
	bool isTrigger = false; //Group can spawn nearby virtualized AI like a player can
	float altitude = 0;		//Current altitude above land
	OVT_GroupSpeed speed;	//Travel speed (when virtualized)
	bool cycleWaypoints = true;
	int currentWaypoint = 0;
	
	[NonSerialized()]
	bool remove = false; //Remove this group on next update
	
	[NonSerialized()]
	bool isSpawned = false;
	
	[NonSerialized()]
	EntityID spawnedEntity;	
	
	[NonSerialized()]
	ref array<EntityID> waypointEntities = new array<EntityID>;
	
	void SetWaypoints(array<vector> newWaypoints)
	{
		waypoints.Clear();
		waypoints = newWaypoints;
		
		if(isSpawned)
		{
			SCR_AIGroup aigroup = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(spawnedEntity));
			if(!aigroup) return;
			if(aigroup.GetAgentsCount() == 0) return;
			
			array<AIWaypoint> waypoints = {};
			aigroup.GetWaypoints(waypoints);
			
			foreach(AIWaypoint wp : waypoints)
			{
				aigroup.RemoveWaypoint(wp);
				SCR_EntityHelper.DeleteEntityAndChildren(wp);
			}
			OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
			virt.SetWaypoints(aigroup, this);
		}		
	}
	
	static OVT_VirtualizedGroupData Get(int groupId)
	{
		OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
		return virt.GetData(groupId);
	}
}

const int VIRTUALIZATION_FREQUENCY = 5000;
const int VIRTUALIZATION_QUEUE_FREQUENCY = 200;
const int SPAWN_DISTANCE = 2500;

class OVT_VirtualizationManagerComponent: OVT_Component
{
	protected ref map<int, ref OVT_VirtualizedGroupData> m_mGroups;
	protected int nextId = 0;
	
	protected ref array<int> m_aSpawnQueue;
	protected ref array<int> m_aDespawnQueue;
	
	static OVT_VirtualizationManagerComponent s_Instance;	
	static OVT_VirtualizationManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_VirtualizationManagerComponent.Cast(pGameMode.FindComponent(OVT_VirtualizationManagerComponent));
		}

		return s_Instance;
	}
	
	void PostGameStart()
	{
		GetGame().GetCallqueue().CallLater(CheckUpdate, VIRTUALIZATION_FREQUENCY, true, GetOwner());				
		GetGame().GetCallqueue().CallLater(CheckQueue, VIRTUALIZATION_QUEUE_FREQUENCY, true, GetOwner());				
	}
	
	OVT_VirtualizedGroupData Create(ResourceName prefab, vector pos, array<vector> waypoints, bool isTrigger = false, OVT_GroupOrder order = OVT_GroupOrder.PATROL, OVT_GroupSpeed speed = OVT_GroupSpeed.WALKING, bool cycleWaypoints = true)
	{
		OVT_VirtualizedGroupData data();
		data.id = nextId;
		data.prefab = prefab;
		data.pos = pos;
		data.waypoints = waypoints;
		data.isTrigger = isTrigger;
		data.order = order;
		data.speed = speed;
		data.cycleWaypoints = cycleWaypoints;
		
		m_mGroups[data.id] = data;
		
		nextId++;
		
		return data;
	}
	
	OVT_VirtualizedGroupData GetData(int id)
	{
		if(!m_mGroups.Contains(id)) return null;
		return m_mGroups[id];
	}
	
	bool GetGroupsArray(inout array<ref OVT_VirtualizedGroupData> groups)
	{
		for(int t=0; t<m_mGroups.Count(); t++)
		{
			OVT_VirtualizedGroupData group = m_mGroups.GetElement(t);
			groups.Insert(group);
		}
		return true;
	}
	
	bool GetGroupsIdArray(inout array<string> groups)
	{
		for(int t=0; t<m_mGroups.Count(); t++)
		{
			OVT_VirtualizedGroupData group = m_mGroups.GetElement(t);
			groups.Insert(group.GetPersistentId());
		}
		return true;
	}
	
	bool LoadGroupsIdArray(array<string> groups)
	{
		foreach(string persId : groups)
		{
			OVT_VirtualizedGroupData data = EPF_PersistentScriptedStateLoader<OVT_VirtualizedGroupData>.Load(persId);
			m_mGroups[data.id] = data;			
		}
		return true;
	}
	
	protected void CheckQueue()
	{
		if(m_aSpawnQueue.Count() > 0)
		{
			int id = m_aSpawnQueue[0];
			OVT_VirtualizedGroupData groupData = GetData(id);
			m_aSpawnQueue.Remove(0);
			if(groupData && !m_aDespawnQueue.Contains(id))
			{
				Spawn(groupData);
			}
		}
		
		if(m_aDespawnQueue.Count() > 0)
		{
			int id = m_aDespawnQueue[0];
			OVT_VirtualizedGroupData groupData = GetData(id);
			m_aDespawnQueue.Remove(0);
			if(groupData)
			{
				Despawn(groupData);
			}
		}
	}
	
	int GetNumGroupsInRange(vector pos, float range)
	{
		int num = 0;
		for(int t=0; t<m_mGroups.Count(); t++)
		{
			OVT_VirtualizedGroupData group = m_mGroups.GetElement(t);
			if(group.isTrigger) continue;
			float dist = vector.Distance(pos, group.pos);
			if(dist <= range)
			{
				num++;
			}
		}
		return num;
	}
	
	protected void CheckUpdate()
	{		
		autoptr array<int> toRemove();
		
		for(int i=0; i<m_mGroups.Count(); i++)
		{
			OVT_VirtualizedGroupData groupData = m_mGroups.GetElement(i);
			if(!groupData) {
				m_mGroups.RemoveElement(i);
				continue;
			}
			if(m_aSpawnQueue.Contains(groupData.id) || m_aDespawnQueue.Contains(groupData.id)) continue;
			
			vector target = groupData.pos;
			if(groupData.waypoints && groupData.waypoints.Count() > 0)
			{
				target = groupData.waypoints[groupData.currentWaypoint];
			}
			
			if(groupData.remove)
			{
				if(groupData.isSpawned) Despawn(groupData);
				toRemove.Insert(groupData.id);
				continue;
			}
			
			if(groupData.isSpawned)
			{				
				IEntity group = GetGame().GetWorld().FindEntityByID(groupData.spawnedEntity);
				if(!group)
				{
					toRemove.Insert(groupData.id);
					continue;
				}
				
				groupData.pos = group.GetOrigin();
				
				if(!OVT_Global.PlayerInRange(groupData.pos, SPAWN_DISTANCE))
				{
					m_aDespawnQueue.Insert(groupData.id);
				}
			}else{			
				if(groupData.waypoints.Count() > 0)
				{			
					float distanceTravelled = (float)groupData.speed * ((float)VIRTUALIZATION_FREQUENCY / 1000);				
					groupData.pos = SCR_Math3D.MoveTowards(groupData.pos, target, distanceTravelled);
				}
				
				if(OVT_Global.PlayerInRange(groupData.pos, SPAWN_DISTANCE))
				{
					m_aSpawnQueue.Insert(groupData.id);
				}
			}
			
			float distance = vector.Distance(groupData.pos, target);
			if(distance < 1)
			{
				groupData.currentWaypoint++;
				if(groupData.currentWaypoint >= groupData.waypoints.Count())
				{
					groupData.currentWaypoint = 0;
					if(!groupData.cycleWaypoints)
					{
						groupData.waypoints.Clear();
					}
				}
			}
		}
		
		foreach(int id : toRemove)
		{
			m_mGroups.Remove(id);
		}
	}
	
	protected void Spawn(OVT_VirtualizedGroupData data)
	{
		vector pos = data.pos;
		BaseWorld world = GetGame().GetWorld();
		float ground = world.GetSurfaceY(pos[0],pos[2]);
		pos[1] = ground;
		
		IEntity entity = OVT_Global.SpawnEntityPrefab(data.prefab, pos);
		data.spawnedEntity = entity.GetID();
		data.isSpawned = true;
		SetWaypoints(entity, data);
	}
	
	protected void Despawn(OVT_VirtualizedGroupData data)
	{
		IEntity entity = GetGame().GetWorld().FindEntityByID(data.spawnedEntity);
		SCR_EntityHelper.DeleteEntityAndChildren(entity);
		
		data.spawnedEntity = null;
		data.isSpawned = false;
		
		foreach(EntityID id : data.waypointEntities)
		{
			IEntity wp = GetGame().GetWorld().FindEntityByID(id);
			SCR_EntityHelper.DeleteEntityAndChildren(wp);
		}
		data.waypointEntities.Clear();
	}
	
	void SetWaypoints(IEntity entity, OVT_VirtualizedGroupData data)
	{
		int numWaypoints = data.waypoints.Count();
		SCR_AIGroup aigroup = SCR_AIGroup.Cast(entity);
		
		if(numWaypoints == 0) {
			AIWaypoint defend = OVT_Global.GetConfig().SpawnDefendWaypoint(data.pos);
			aigroup.AddWaypoint(defend);
			return;
		}
		
		array<AIWaypoint> queueOfWaypoints = new array<AIWaypoint>();
		
		vector firstWP;
		if(!aigroup) return;
		
		for(int i=data.currentWaypoint; i<data.currentWaypoint+numWaypoints; i++)
		{
			int index = i;
			if(i >= numWaypoints)
			{
				if(!data.cycleWaypoints) break;
				index = i - numWaypoints;
			}
			vector wp = data.waypoints[index];
			if(i == data.currentWaypoint) firstWP = wp;
			
			if(data.order == OVT_GroupOrder.PATROL)
			{
				AIWaypoint patrol = OVT_Global.GetConfig().SpawnPatrolWaypoint(wp);
				if(data.cycleWaypoints)
					queueOfWaypoints.Insert(patrol);
				else
					aigroup.AddWaypoint(patrol);				
			}	
						
			AIWaypoint searchdestroy = OVT_Global.GetConfig().SpawnSearchAndDestroyWaypoint(wp);			
			if(data.cycleWaypoints)
				queueOfWaypoints.Insert(searchdestroy);
			else
				aigroup.AddWaypoint(searchdestroy);		
			
			if(data.order == OVT_GroupOrder.ATTACK)
			{
				AIWaypoint defend = OVT_Global.GetConfig().SpawnDefendWaypoint(wp);
				if(data.cycleWaypoints)
					queueOfWaypoints.Insert(defend);
				else
					aigroup.AddWaypoint(defend);				
			}	
		}
		
		if(data.cycleWaypoints)
		{
			AIWaypointCycle cycle = AIWaypointCycle.Cast(OVT_Global.GetConfig().SpawnWaypoint(OVT_Global.GetConfig().m_pCycleWaypointPrefab, firstWP));
			cycle.SetWaypoints(queueOfWaypoints);
			cycle.SetRerunCounter(-1);
			aigroup.AddWaypoint(cycle);
		}
	}
	
	void OVT_VirtualizationManagerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_mGroups = new map<int, ref OVT_VirtualizedGroupData>;
		m_aSpawnQueue = new array<int>;
		m_aDespawnQueue = new array<int>;
	}
	
	void ~OVT_VirtualizationManagerComponent()
	{
		if(m_mGroups)
		{
			m_mGroups.Clear();
			m_mGroups = null;
		}
		
		if(m_aSpawnQueue)
		{
			m_aSpawnQueue.Clear();
			m_aSpawnQueue = null;
		}
		
		if(m_aDespawnQueue)
		{
			m_aDespawnQueue.Clear();
			m_aDespawnQueue = null;
		}
		
		GetGame().GetCallqueue().Remove(CheckUpdate);
		GetGame().GetCallqueue().Remove(CheckQueue);
	}
}