//! Mans curated sniper positions (entities carrying OVT_SniperPositionComponent) within base
//! range with a two-man sniper team. Same placement rules as OVT_BaseUpgradeTowerGuard: members
//! are teleported onto the marker (AI cannot path to arbitrary elevated spots) facing the
//! marker's forward direction, and get NO waypoint - an idle group keeps full threat/attack
//! reactions, which is the configuration proven to actually engage.
class OVT_BaseUpgradeSniperPosition : OVT_BasePatrolUpgrade
{
	//! A two-man team costs two infantry shares
	protected const int TEAM_SIZE = 2;

	//! Marker-local offset between team members so the spotter stands beside the sniper
	protected const vector TEAM_MEMBER_OFFSET = "1.2 0 0";

	ref array<ref EntityID> m_Positions;
	ref map<ref EntityID,ref EntityID> m_PositionGuards;

	//! Marker entity waiting for each group's (frame-deferred) member spawns, keyed by group
	ref map<ref EntityID,ref EntityID> m_PendingMarkers;
	//! How many of the group's members have been placed so far
	ref map<ref EntityID,int> m_PendingPlaced;

	override void PostInit()
	{
		super.PostInit();

		m_Positions = new array<ref EntityID>;
		m_PositionGuards = new map<ref EntityID,ref EntityID>;
		m_PendingMarkers = new map<ref EntityID,ref EntityID>;
		m_PendingPlaced = new map<ref EntityID,int>;

		FindPositions();
	}

	protected void FindPositions()
	{
		GetGame().GetWorld().QueryEntitiesBySphere(m_BaseController.GetOwner().GetOrigin(), OVT_Global.GetConfig().m_Difficulty.baseRange, CheckPositionAddToArray, FilterPositionEntities, EQueryEntitiesFlags.ALL);
	}

	protected bool FilterPositionEntities(IEntity entity)
	{
		if(entity.FindComponent(OVT_SniperPositionComponent)) return true;
		return false;
	}

	protected bool CheckPositionAddToArray(IEntity entity)
	{
		m_Positions.Insert(entity.GetID());
		return true;
	}

	protected override void CheckUpdate()
	{
		if(!m_BaseController.IsOccupyingFaction())
		{
			CheckClean();
			return;
		}
		bool inrange = PlayerInRange() && !m_occupyingFactionManager.m_CurrentQRF;
		if(inrange && !m_bSpawned)
		{
			// Only deduct what was actually spent - teams can fail to place (marker destroyed),
			// and the remainder is still paid-for resources
			int spent = Spend(m_iProxedResources, OVT_Global.GetOccupyingFaction().m_iThreat);
			m_iProxedResources -= spent;
			if(m_iProxedResources < 0) m_iProxedResources = 0;
			m_bSpawned = true;
		}else if(!inrange && m_bSpawned){
			foreach(EntityID id : m_Groups)
			{
				SCR_AIGroup group = GetGroup(id);
				if(!group) continue;
				m_iProxedResources += group.GetAgentsCount() * OVT_Global.GetConfig().m_Difficulty.baseResourceCost;

				SCR_EntityHelper.DeleteEntityAndChildren(group);
			}
			m_Groups.Clear();
			m_PositionGuards.Clear();
			m_PendingMarkers.Clear();
			m_PendingPlaced.Clear();
			m_bSpawned = false;
		}else{
			CheckClean();
		}
	}

	override int Spend(int resources, float threat)
	{
		int spent = 0;
		int teamCost = OVT_Global.GetConfig().m_Difficulty.baseResourceCost * TEAM_SIZE;

		foreach(EntityID id : m_Positions)
		{
			if(resources < teamCost) break;

			IEntity marker = GetGame().GetWorld().FindEntityByID(id);
			if(!marker) continue;

			OVT_SniperPositionComponent position = OVT_SniperPositionComponent.Cast(marker.FindComponent(OVT_SniperPositionComponent));
			if(position && threat < position.m_iMinimumThreat) continue;

			bool needsGuard = false;
			if(!m_PositionGuards.Contains(id))
			{
				needsGuard = true;
			}else{
				//Check if still alive
				SCR_AIGroup group = GetGroup(m_PositionGuards[id]);
				if(group)
				{
					if(group.GetAgentsCount() == 0) needsGuard = true;
				}else{
					needsGuard = true;
				}
			}
			if(needsGuard)
			{
				if(!PlayerInRange()){
					m_iProxedResources += teamCost;
					spent += teamCost;
				}else{
					if(BuyTeam(id))
					{
						m_bSpawned = true;
						resources -= teamCost;
						spent += teamCost;
					}
				}
			}
		}

		return spent;
	}

	protected bool BuyTeam(EntityID positionID)
	{
		IEntity marker = GetGame().GetWorld().FindEntityByID(positionID);
		if(!marker) return false;

		ResourceName teamPrefab = OVT_Global.GetConfig().GetOccupyingFaction().m_aGroupSniperTeamPrefab;
		if(teamPrefab == "") return false;

		SCR_AIGroup group = SCR_AIGroup.Cast(OVT_Global.SpawnEntityPrefab(teamPrefab, marker.GetOrigin()));
		if(!group) return false;

		// Members are teleported onto the marker as the frame-deferred member spawns deliver
		// them (SCR_AIGroup snaps spawned members to ground navmesh). No waypoint on purpose.
		m_PendingMarkers[group.GetID()] = positionID;
		m_PendingPlaced[group.GetID()] = 0;
		group.GetOnAgentAdded().Insert(OnTeamAgentAdded);

		m_Groups.Insert(group.GetID());
		m_PositionGuards[positionID] = group.GetID();

		return true;
	}

	protected void OnTeamAgentAdded(AIAgent agent)
	{
		if(!agent) return;
		SCR_AIGroup group = SCR_AIGroup.Cast(agent.GetParentGroup());
		if(!group) return;

		EntityID groupID = group.GetID();
		if(!m_PendingMarkers.Contains(groupID)) return;

		IEntity marker = GetGame().GetWorld().FindEntityByID(m_PendingMarkers[groupID]);
		if(!marker)
		{
			m_PendingMarkers.Remove(groupID);
			m_PendingPlaced.Remove(groupID);
			group.GetOnAgentAdded().Remove(OnTeamAgentAdded);
			return;
		}

		int placed = m_PendingPlaced[groupID];

		BaseGameEntity character = BaseGameEntity.Cast(agent.GetControlledEntity());
		if(character)
		{
			// Face the marker's forward direction; each member one sideways step apart
			float slot = placed;
			vector mat[4];
			marker.GetWorldTransform(mat);
			mat[3] = mat[3] + (TEAM_MEMBER_OFFSET * slot).Multiply3(mat);
			character.Teleport(mat);
		}

		placed++;
		m_PendingPlaced[groupID] = placed;

		if(placed >= group.m_aUnitPrefabSlots.Count())
		{
			m_PendingMarkers.Remove(groupID);
			m_PendingPlaced.Remove(groupID);
			group.GetOnAgentAdded().Remove(OnTeamAgentAdded);
		}
	}

	override OVT_BaseUpgradeData Serialize()
	{
		// Same contract as OVT_BaseUpgradeTowerGuard: bank the value instead of recording group
		// replay data - the next in-range tick re-buys through BuyTeam onto the markers.
		OVT_BaseUpgradeData struct = new OVT_BaseUpgradeData();
		struct.type = ClassName();
		struct.resources = GetResources();
		return struct;
	}

	override bool Deserialize(OVT_BaseUpgradeData struct)
	{
		if(!m_BaseController.IsOccupyingFaction()) return true;
		m_iProxedResources = struct.resources;
		return true;
	}

}
