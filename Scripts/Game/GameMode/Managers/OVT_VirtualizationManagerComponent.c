class OVT_VirtualizationManagerComponentClass: OVT_ComponentClass
{
};

enum OVT_GroupOrder
{
	PATROL,
	SEARCH_AND_DESTROY,
	ATTACK
}

enum OVT_GroupSpeed
{
	WALKING=2,
	DRIVING_SLOW=10,
	DRIVING_FAST=22,
	FLYING_SLOW=55,
	FLYING_FAST=80
}

class OVT_VirtualizedGroupData : Managed
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
	bool isSpawned = false;
	
	[NonSerialized()]
	EntityID spawnedEntity;	
}

const int VIRTUALIZATION_FREQUENCY = 5000;
const int SPAWN_DISTANCE = 50;

class OVT_VirtualizationManagerComponent: OVT_Component
{
	protected ref map<int, OVT_VirtualizedGroupData> m_mGroups;
	protected int nextId = 0;
	
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
	
	protected void CheckUpdate()
	{		
		array<int> toRemove();
		
		for(int i=0; i<m_mGroups.Count(); i++)
		{
			OVT_VirtualizedGroupData groupData = m_mGroups.GetElement(i);
			vector target = groupData.waypoints[groupData.currentWaypoint];
			
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
					Despawn(groupData);
				}
			}else{			
				if(groupData.waypoints.Count() == 0) return;
			
				float distanceTravelled = (float)groupData.speed * ((float)VIRTUALIZATION_FREQUENCY / 1000);
				
				groupData.pos = SCR_Math3D.MoveTowards(groupData.pos, target, distanceTravelled);
				
				if(OVT_Global.PlayerInRange(groupData.pos, SPAWN_DISTANCE))
				{
					Spawn(groupData);
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
	}
	
	protected void Spawn(OVT_VirtualizedGroupData data)
	{
		
	}
	
	protected void Despawn(OVT_VirtualizedGroupData data)
	{
		
	}
	
	void OVT_VirtualizationManagerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_mGroups = new map<int, OVT_VirtualizedGroupData>;
	}
	
	void ~OVT_VirtualizationManagerComponent()
	{
		if(m_mGroups)
		{
			m_mGroups.Clear();
			m_mGroups = null;
		}
	}
}