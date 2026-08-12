class OVT_BaseUpgradeTowerGuard : OVT_BasePatrolUpgrade
{
	ref array<ref EntityID> m_Towers;
	ref map<ref EntityID,ref EntityID> m_TowerGuards;

	//! Post positions waiting for a member to spawn, keyed by group. SCR_AIGroup spawns members
	//! across frames AND snaps them to the closest navmesh point, so a guard can only be placed
	//! from OnGuardAgentAdded - never synchronously after spawning the group.
	ref map<ref EntityID, vector> m_PendingPosts;

	//! Tower-local offset from the cabin CoverPost to the open-air walkway ringing it (forward =
	//! the authored look direction). Inside the cabin the window glass blinds perception and
	//! blocks the muzzle.
	protected const vector WALKWAY_OFFSET = "0 0 1.5";

	override void PostInit()
	{
		super.PostInit();

		m_Towers = new array<ref EntityID>;
		m_TowerGuards = new map<ref EntityID,ref EntityID>;
		m_PendingPosts = new map<ref EntityID, vector>;

		FindTowers();
	}

	protected void FindTowers()
	{
		GetGame().GetWorld().QueryEntitiesBySphere(m_BaseController.GetOwner().GetOrigin(), OVT_Global.GetConfig().m_Difficulty.baseRange, CheckTowerAddToArray, FilterTowerEntities, EQueryEntitiesFlags.ALL);
	}

	protected bool FilterTowerEntities(IEntity entity)
	{
		if(entity.ClassName() == "SCR_DestructibleBuildingEntity") {

			SCR_MapDescriptorComponent mapdesc = SCR_MapDescriptorComponent.Cast(entity.FindComponent(SCR_MapDescriptorComponent));
			if(mapdesc)
			{
				EMapDescriptorType type = mapdesc.GetBaseType();
				if(type == EMapDescriptorType.MDT_TOWER) return true;
			}
		}
		return false;
	}

	protected bool CheckTowerAddToArray(IEntity entity)
	{
		m_Towers.Insert(entity.GetID());
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
			// Only deduct what was actually spent - guards can fail to place (no accessible cover
			// post), and the remainder is still paid-for resources
			int spent = Spend(m_iProxedResources, OVT_Global.GetOccupyingFaction().m_iThreat);
			m_iProxedResources -= spent;
			if(m_iProxedResources < 0) m_iProxedResources = 0;
			m_bSpawned = true;
		}else if(!inrange && m_bSpawned){
			foreach(EntityID towerID, EntityID groupID : m_TowerGuards)
			{
				ReleasePosts(towerID);
			}
			foreach(EntityID id : m_Groups)
			{
				SCR_AIGroup group = GetGroup(id);
				if(!group) continue;
				m_iProxedResources += group.GetAgentsCount() * OVT_Global.GetConfig().m_Difficulty.baseResourceCost;

				SCR_EntityHelper.DeleteEntityAndChildren(group);
			}
			m_Groups.Clear();
			m_TowerGuards.Clear();
			m_PendingPosts.Clear();
			m_bSpawned = false;
		}else{
			CheckClean();
		}
	}

	override int Spend(int resources, float threat)
	{
		int spent = 0;

		foreach(EntityID id : m_Towers)
		{
			if(resources < OVT_Global.GetConfig().m_Difficulty.baseResourceCost) break;

			bool needsGuard = false;
			if(!m_TowerGuards.Contains(id))
			{
				needsGuard = true;
			}else{
				//Check if still alive
				SCR_AIGroup group = GetGroup(m_TowerGuards[id]);
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
					m_iProxedResources += OVT_Global.GetConfig().m_Difficulty.baseResourceCost;
					spent += OVT_Global.GetConfig().m_Difficulty.baseResourceCost;
				}else{
					if(BuyGuard(id))
					{
						m_bSpawned = true;
						resources -= OVT_Global.GetConfig().m_Difficulty.baseResourceCost;
						spent += OVT_Global.GetConfig().m_Difficulty.baseResourceCost;
					}
				}
			}
		}

		return spent;
	}

	protected bool BuyGuard(EntityID towerID)
	{
		// A destroyed tower is replaced by its ruin entity and this ID goes stale
		IEntity tower = GetGame().GetWorld().FindEntityByID(towerID);
		if(!tower) return false;

		SCR_AISmartActionSentinelComponent sentinel = FindCoverPost(tower);
		if(!sentinel) return false;

		// ActionOffset is in the tower's local space (see vanilla SCR_AIGetSmartActionSentinelParams).
		// The guard stands on the open-air walkway ringing the cabin, NOT on the authored CoverPost
		// inside it: the cabin's window glass blinds AI perception traces (the wanted system's LOS
		// trace included) and obstructs the muzzle, so a guard in the cabin never sees or engages.
		vector mat[4];
		tower.GetWorldTransform(mat);
		vector postPos = mat[3] + (sentinel.GetActionOffset() + WALKWAY_OFFSET).Multiply3(mat);

		SCR_AIGroup group = SCR_AIGroup.Cast(OVT_Global.SpawnEntityPrefab(OVT_Global.GetConfig().GetOccupyingFaction().m_aGroupSniperPrefab, tower.GetOrigin()));
		if(!group) return false;

		// AI cannot path up ladders (no script-side ladder support exists), so the sniper is
		// teleported onto the walkway once his entity exists - the same approach vanilla takes for
		// elevated posts (SCR_AIAllocateActionsForDefendActivity.OccupySA).
		//
		// He deliberately gets NO waypoint: every post waypoint available here parks him in the
		// blind cabin (both defend-waypoint presets resolve to the cabin's sentinel actions, and a
		// raw CoverPost smart action is a pose loop with no enemy check and no fire node - the only
		// fire nodes in the AI live in the attack behavior). An idle group keeps full threat and
		// attack reactions, which is the configuration proven to engage.
		m_PendingPosts[group.GetID()] = postPos;
		group.GetOnAgentAdded().Insert(OnGuardAgentAdded);

		m_Groups.Insert(group.GetID());
		m_TowerGuards[towerID] = group.GetID();

		return true;
	}

	protected SCR_AISmartActionSentinelComponent FindCoverPost(IEntity tower)
	{
		array<Managed> outComponents = {};
		tower.FindComponents(SCR_AISmartActionSentinelComponent, outComponents);

		foreach (Managed outComponent : outComponents)
		{
			SCR_AISmartActionSentinelComponent component = SCR_AISmartActionSentinelComponent.Cast(outComponent);
			if (!component)
				continue;
			array<string> outTags = {};
			component.GetTags(outTags);
			if (outTags.Contains("CoverPost") && component.IsActionAccessible())
				return component;
		}

		return null;
	}

	protected void OnGuardAgentAdded(AIAgent agent)
	{
		if(!agent) return;
		SCR_AIGroup group = SCR_AIGroup.Cast(agent.GetParentGroup());
		if(!group) return;

		EntityID groupID = group.GetID();
		if(!m_PendingPosts.Contains(groupID)) return;

		BaseGameEntity character = BaseGameEntity.Cast(agent.GetControlledEntity());
		if(character)
		{
			vector mat[4];
			Math3D.MatrixIdentity4(mat);
			mat[3] = m_PendingPosts[groupID];
			character.Teleport(mat);
		}

		m_PendingPosts.Remove(groupID);
		group.GetOnAgentAdded().Remove(OnGuardAgentAdded);
	}

	//! A reserved post is only auto-released by the action ending or its user dying
	//! (SCR_AISmartActionComponent.ReserveAction) - deleting a proxied guard would leave the
	//! post inaccessible forever and block every future BuyGuard on this tower
	protected void ReleasePosts(EntityID towerID)
	{
		IEntity tower = GetGame().GetWorld().FindEntityByID(towerID);
		if(!tower) return;

		array<Managed> outComponents = {};
		tower.FindComponents(SCR_AISmartActionSentinelComponent, outComponents);

		foreach (Managed outComponent : outComponents)
		{
			SCR_AISmartActionComponent component = SCR_AISmartActionComponent.Cast(outComponent);
			if (!component)
				continue;
			if (!component.IsActionAccessible())
				component.ReleaseAction();
		}
	}

	override OVT_BaseUpgradeData Serialize()
	{
		// Deliberately NOT OVT_BasePatrolUpgrade's group records: replaying a tower guard through
		// BuyPatrol restores a waypointless rifleman at the tower base. Bank the value instead;
		// the next in-range tick re-buys through BuyGuard onto the post.
		OVT_BaseUpgradeData struct = new OVT_BaseUpgradeData();
		struct.type = ClassName();
		struct.resources = GetResources();
		return struct;
	}

	override bool Deserialize(OVT_BaseUpgradeData struct)
	{
		if(!m_BaseController.IsOccupyingFaction()) return true;

		m_iProxedResources = struct.resources;

		// Saves from before this override recorded live guards as group records with resources = 0
		foreach(OVT_BaseUpgradeGroupData g : struct.groups)
		{
			m_iProxedResources += OVT_Global.GetConfig().m_Difficulty.baseResourceCost;
		}

		return true;
	}

}
