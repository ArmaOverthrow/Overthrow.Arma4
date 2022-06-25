class OVT_BasePatrolUpgrade : OVT_BaseUpgrade
{
	[Attribute("1", desc: "Deactivate patrols when noone around")]
	bool m_bDeactivate;
	
	ref array<ref EntityID> m_Groups;
	ref array<ref ResourceName> m_ProxiedGroups;
	ref array<ref vector> m_ProxiedPositions;	
	int m_iProxedResources = 0;
	
	protected const int DEACTIVATE_FREQUENCY = 10000;
	protected const int DEACTIVATE_RANGE = 2500;
	
	override void PostInit()
	{
		m_Groups = new array<ref EntityID>;
		m_ProxiedGroups = new array<ref ResourceName>;
		m_ProxiedPositions = new array<ref vector>;
		
		if(m_bDeactivate)
		{
			//Stagger these updates
			float freq = s_AIRandomGenerator.RandFloatXY(DEACTIVATE_FREQUENCY - 1000, DEACTIVATE_FREQUENCY + 1000);
			
			GetGame().GetCallqueue().CallLater(CheckUpdate, freq, true, m_BaseController.GetOwner());	
		}
	}
	
	protected void CheckUpdate()
	{
		if(PlayerInRange())
		{
			foreach(int i, ResourceName res : m_ProxiedGroups)
			{
				BuyPatrol(0, res, m_ProxiedPositions[i]);
			}
			m_ProxiedGroups.Clear();
			m_ProxiedPositions.Clear();
			m_iProxedResources = 0;
		}else{
			foreach(EntityID id : m_Groups)
			{
				SCR_AIGroup group = GetGroup(id);
				m_iProxedResources += group.GetAgentsCount() * m_Config.m_Difficulty.baseResourceCost;
				m_ProxiedGroups.Insert(group.GetPrefabData().GetPrefabName());
				m_ProxiedPositions.Insert(group.GetOrigin());
				SCR_Global.DeleteEntityAndChildren(group);
			}
			m_Groups.Clear();
		}
	}
	
	protected bool PlayerInRange()
	{		
		bool active = false;
		array<int> players = new array<int>;
		PlayerManager mgr = GetGame().GetPlayerManager();
		int numplayers = mgr.GetPlayers(players);
		
		if(numplayers > 0)
		{
			foreach(int playerID : players)
			{
				IEntity player = mgr.GetPlayerControlledEntity(playerID);
				if(!player) continue;
				float distance = vector.Distance(player.GetOrigin(), m_BaseController.GetOwner().GetOrigin());
				if(distance < DEACTIVATE_RANGE)
				{
					active = true;
				}
			}
		}
		
		return active;
	}
	
	protected int BuyPatrol(float threat, ResourceName res = "", vector pos = "0 0 0")
	{
		OVT_Faction faction = m_Config.GetOccupyingFaction();
				
		if(res == ""){
			res = faction.m_aGroupInfantryPrefabSlots.GetRandomElement();
			if(threat > 25) res = faction.m_aHeavyInfantryPrefabSlots.GetRandomElement();
		}
		
		BaseWorld world = GetGame().GetWorld();
			
		EntitySpawnParams spawnParams = new EntitySpawnParams;
		spawnParams.TransformMode = ETransformMode.WORLD;
		
		if(pos[0] == 0)
			pos = m_BaseController.GetOwner().GetOrigin();
		
		float surfaceY = world.GetSurfaceY(pos[0], pos[2]);
		if (pos[1] < surfaceY)
		{
			pos[1] = surfaceY;
		}
		
		spawnParams.Transform[3] = pos;
		IEntity group = GetGame().SpawnEntityPrefab(Resource.Load(res), world, spawnParams);
		
		m_Groups.Insert(group.GetID());
		
		SCR_AIGroup aigroup = SCR_AIGroup.Cast(group);
		
		AddWaypoints(aigroup);
		
		int newres = aigroup.m_aUnitPrefabSlots.Count() * m_Config.m_Difficulty.baseResourceCost;
			
		return newres;
	}
	
	protected void AddWaypoints(SCR_AIGroup aigroup)
	{
		
	}
	
	override int GetResources()
	{
		int res = 0;
		foreach(EntityID id : m_Groups)
		{
			SCR_AIGroup group = GetGroup(id);
			res += group.GetAgentsCount() * m_Config.m_Difficulty.baseResourceCost;			
		}
		return res + m_iProxedResources;
	}
	
	override int Spend(int resources, float threat)
	{
		int spent = 0;
		
		while(resources > 0)
		{			
			int newres = BuyPatrol(threat);
			
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
	
	protected AIWaypoint SpawnDefendWaypoint(vector pos, int preset = 0)
	{
		AIWaypoint wp = SpawnWaypoint(m_Config.m_pDefendWaypointPrefab, pos);
		SCR_DefendWaypoint defend = SCR_DefendWaypoint.Cast(wp);
		defend.SetCurrentDefendPreset(preset);
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
	
	void ~OVT_BasePatrolUpgrade()
	{
		GetGame().GetCallqueue().Remove(CheckUpdate);	
		
		if(m_Groups)
		{
			m_Groups.Clear();
			m_Groups = null;
		}
		if(m_ProxiedGroups)
		{
			m_ProxiedGroups.Clear();
			m_ProxiedGroups = null;
		}
		if(m_ProxiedPositions)
		{
			m_ProxiedPositions.Clear();
			m_ProxiedPositions = null;
		}		
	}
}