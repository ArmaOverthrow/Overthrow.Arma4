//------------------------------------------------------------------------------------------------
//! BUG-118: AI groups, their prefab-spawned members and their prefab-defined waypoints must never
//! write persistence records.
//!
//! Overthrow rebuilds every AI group from manager state on every boot (garrisons, patrols, QRFs,
//! deployments, town civilians - decision v2-5 in core/persistence), and their persistence configs
//! are scoped `SelfSpawn 0` accordingly. But the `Persistence` component on Character_Base.et and
//! Group_Base.et still REGISTERED them all, so every session wrote a full generation of records
//! that no later session could ever claim or delete - ~490 permanently orphaned records per idle
//! restart, unbounded save growth.
//!
//! This class is the chokepoint for all three entity kinds:
//!   - the group entity itself (EOnInit),
//!   - every member the group spawns from its own prefab list (AddAIEntityToGroup - the tail of
//!     SpawnGroupMember for both the deferred and the SpawnAllImmediately paths),
//!   - every waypoint the group spawns from its prefab (AddWaypointsDynamic).
//!
//! WHAT MUST STAY TRACKED, whichever of those paths it arrives on - see EntityMustStayTracked():
//!   - recruit bodies (parking one into an inactive group DOES go through AddAIEntityToGroup);
//!   - player bodies.
//! Those two are exactly the categories recalled from storage by id (m_sBodyPersistenceId).
//!
//! Untracking is UntrackTransient() rather than a bare StopTracking() because the native
//! registration is lazy and lands frames after spawn - see OVT_PersistenceManagerComponent.
modded class SCR_AIGroup
{
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		OVT_PersistenceManagerComponent.UntrackTransient(this);
	}

	//------------------------------------------------------------------------------------------------
	override bool AddAIEntityToGroup(IEntity entity)
	{
		bool added = super.AddAIEntityToGroup(entity);

		// NOT unconditional: OVT_RecruitManagerComponent parks recruits into inactive groups through
		// this method, and untracking one there would drop its body record and its gear.
		if (!EntityMustStayTracked(entity))
			OVT_PersistenceManagerComponent.UntrackTransient(entity);

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! The two character categories that are recalled from storage by id and must keep their records:
	//! player bodies and recruit bodies. Both group entry points ask this.
	//! \param[in] entity The character joining a group.
	//! \return True when it must stay persistence-tracked.
	protected static bool EntityMustStayTracked(IEntity entity)
	{
		if (!entity)
			return false;

		if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity) > 0)
			return true;

		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();

		return recruits && recruits.GetRecruitFromEntity(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! The catch-all for AI that reaches a group WITHOUT passing AddAIEntityToGroup. Measured case
	//! (BUG-118 residual): one `Character_USSR_Randomized` variant per boot self-joined a group
	//! through its own AI activation ("some other system was faster" - AddAgent's own comment) and
	//! leaked a 32-record tree per restart. OnAgentAdded fires for EVERY agent that enters ANY
	//! group, whatever spawned it, so the exclusions here are what protect the two character
	//! categories that must STAY tracked:
	//!   - player bodies (player-controlled at join time, on every vanilla join path);
	//!   - recruit bodies (registered in the recruit manager BEFORE the group add on all three
	//!     flows - AddRecruit, AttachRecruitBody, and the existing-entity re-add).
	override void OnAgentAdded(AIAgent child)
	{
		super.OnAgentAdded(child);

		if (!Replication.IsServer())
			return;

		if (!child)
			return;

		IEntity entity = child.GetControlledEntity();
		if (!entity)
			return;

		if (EntityMustStayTracked(entity))
			return;

		OVT_PersistenceManagerComponent.UntrackTransient(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla never gives the commanding slave group a faction (CreatePlayableGroup sets only the
	//! master's), and an Overthrow master is CIV anyway because players are registered civilian
	//! (OVT_SpawnLogic.SetCivilianFaction). A faction-less group fails
	//! SCR_AIGroupUtilityComponent.IsMilitary(), so the group brain - perception clusters, danger
	//! sharing, combat-mode evaluation - never runs for recruit squads. Slave groups only ever hold
	//! AI (recruits), so stamp them with the resistance faction; SetFaction also re-asserts the
	//! affiliation on every current member (BUG-146).
	override void SetSlave(SCR_AIGroup group)
	{
		super.SetSlave(group);

		if (!Replication.IsServer())
			return;
		if (!group || group.GetFaction())
			return;

		OVT_OverthrowConfigComponent config = OVT_OverthrowConfigComponent.GetInstance();
		if (!config)
			return;

		Faction faction = GetGame().GetFactionManager().GetFactionByKey(config.m_sPlayerFaction);
		if (faction)
			group.SetFaction(faction);
	}

	//------------------------------------------------------------------------------------------------
	override void AddWaypointsDynamic(out array<IEntity> entityInstanceList, array<ref SCR_WaypointPrefabLocation> prefabs)
	{
		super.AddWaypointsDynamic(entityInstanceList, prefabs);

		if (!entityInstanceList)
			return;

		foreach (IEntity waypointEntity : entityInstanceList)
		{
			OVT_PersistenceManagerComponent.UntrackTransient(waypointEntity);
		}
	}
}
