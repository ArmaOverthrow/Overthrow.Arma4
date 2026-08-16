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
//! WHAT DELIBERATELY DOES NOT PASS THROUGH HERE, so it stays tracked:
//!   - recruit bodies: spawned individually by OVT_RecruitManagerComponent and added through the
//!     RequestAddAIAgent/AddAgent path, never through AddAIEntityToGroup;
//!   - player bodies: possessed characters join groups via AddAgentFromControlledEntity.
//! Those two are exactly the categories recalled from storage by id (m_sBodyPersistenceId).
//!
//! Untracking is UntrackTransient() rather than a bare StopTracking() because the native
//! registration is lazy and lands frames after spawn - see OVT_PersistenceManagerComponent.
//!
//! ================== THE ONE EXEMPTION (virtualization/core, T3.1) ==================
//! Groups the virtualization manager registers are NOT rebuild-on-boot AI: they are the campaign's
//! durable record of a virtual group and are the one category that could legitimately round-trip
//! through save/load (the engine keeps the group entity alive through dormancy precisely to be that
//! record). They opt out of the untrack above through the ArmPersistenceExemption() one-shot below -
//! armed immediately before the spawn, exactly like vanilla's own SCR_AIGroup.IgnoreSpawning(). The
//! same one-shot is armed by Modded/SCR_AIGroupSerializer for every group vanilla's persistence
//! RESTORES from a record, so a restored group is not untracked (and its record deleted) a second
//! after it comes back.
//!
//! ⚠ The manager keeps this switched OFF for now (m_bPersistGroupEntities) - a tracked group that
//! cannot self-spawn on load writes a record nothing can claim, which is the BUG-118 shape again.
//! See docs/features/virtualization/core/api.md §8 for why self-spawn cannot yet be granted narrowly
//! enough, and what Phase 5 has to decide.
//!
//! Member characters of an exempt group stay untracked on purpose: the roster truth is core's
//! per-slot mask (D2) plus the engine's dormant counts, both of which live on the group record.
//! Persisting the live characters as well would double-count the roster on load.
//! ===================================================================================
modded class SCR_AIGroup
{
	//------------------------------------------------------------------------------------------------
	// VIRTUALIZATION (virtualization/core D2 + T3.1) - everything below is inert for a group core
	// never registered: no mask means vanilla behaviour, byte for byte.
	//------------------------------------------------------------------------------------------------

	//! One-shot, consumed by the next SCR_AIGroup EOnInit. Same contract (and same hazard) as
	//! vanilla's s_bIgnoreSpawning: arm it immediately before the spawn call and clear it after.
	protected static bool s_bOVT_PersistenceExempt;

	//! True when this group was spawned while the exemption was armed - i.e. it is a registered
	//! virtual group (or one vanilla's persistence just restored), so it stays persistence-tracked.
	protected bool m_bOVT_PersistenceExempt;

	//! THE roster truth (D2). One entry per prefab slot, non-zero = alive. Shared by reference with
	//! the owning OVT_VirtualGroupRecord, so a death recorded on the record is visible here with no
	//! push. Null/empty = this group is not registered and every override below defers to vanilla.
	protected ref array<int> m_aOVT_SlotAlive;

	//! Slot indices materialised during the CURRENT activation. Cleared on every despawn, because
	//! dormancy deletes the member entities and the next activation re-materialises from scratch.
	protected ref array<int> m_aOVT_SpawnedSlots;

	//! The slot ExpandOneMember is spawning right now, or -1. Read by AddAIEntityToGroup, which is
	//! the only place the freshly spawned member entity is observable (SpawnGroupMember returns a
	//! bool, not the entity).
	protected int m_iOVT_PendingSlot = -1;

	//------------------------------------------------------------------------------------------------
	//! Arms the persistence exemption for the NEXT group entity that runs EOnInit.
	//!
	//! Mirrors SCR_AIGroup.IgnoreSpawning: a static one-shot rather than a per-entity call, because
	//! the caller has no entity handle until SpawnEntityPrefab returns - and EOnInit has already run
	//! by then. Callers MUST clear it again after the spawn (ArmPersistenceExemption(false)) so a
	//! failed or non-group spawn cannot leak the flag onto an unrelated group.
	//! \param[in] exempt True to keep the next group persistence-tracked.
	static void ArmPersistenceExemption(bool exempt)
	{
		s_bOVT_PersistenceExempt = exempt;
	}

	//! \return True when this group is exempt from the BUG-118 untrack, i.e. it is persistable.
	bool IsOVTPersistenceExempt()
	{
		return m_bOVT_PersistenceExempt;
	}

	//------------------------------------------------------------------------------------------------
	//! Hands this group core's survivor mask (D2). Pass the record's own array - it is stored by
	//! reference on purpose, so ReportMemberKilled needs no second push.
	//! \param[in] mask One entry per prefab roster slot, non-zero = alive.
	void SetOVTSlotMask(array<int> mask)
	{
		m_aOVT_SlotAlive = mask;

		if (!m_aOVT_SpawnedSlots)
			m_aOVT_SpawnedSlots = new array<int>();
	}

	//! Drops the mask, returning the group to vanilla refill behaviour.
	void ClearOVTSlotMask()
	{
		m_aOVT_SlotAlive = null;

		if (m_aOVT_SpawnedSlots)
			m_aOVT_SpawnedSlots.Clear();

		m_iOVT_PendingSlot = -1;
	}

	//! \return The survivor mask, or null for a group core does not own.
	array<int> GetOVTSlotMask()
	{
		return m_aOVT_SlotAlive;
	}

	//! \return True when core owns this group's roster truth.
	bool HasOVTSlotMask()
	{
		return m_aOVT_SlotAlive && !m_aOVT_SlotAlive.IsEmpty();
	}

	//! \return Slot indices materialised during the current activation; null for an unowned group.
	array<int> GetOVTSpawnedSlots()
	{
		return m_aOVT_SpawnedSlots;
	}

	//------------------------------------------------------------------------------------------------
	//! Re-asserts the engine's dormant alive/dead counts from the mask (D2b, T3.5).
	//!
	//! WHY THIS IS MANDATORY, not defensive (Phase 1, measured): DespawnMembers records
	//! `alive = GetAgentsCount()` at despawn time (SCR_AIGroup.c:2876), so ANY despawn landing during
	//! an in-progress refill writes the not-yet-spawned slots down as DEAD. The ratchet is permanent -
	//! refill capacity is `totalSlots - dormantDead` (:2726-2729) and nothing engine-side ever
	//! re-corrects it. A 6-man group was observed ratcheting 6 -> 4 -> 2 with zero kills.
	//!
	//! Deliberately does NOTHING when the engine has never recorded dormant counts
	//! (GetDormantAliveCount() == -1, the "never despawned" sentinel): writing counts onto a group
	//! that has never been dormant would flip IsDormant() while its members are alive.
	void ReassertOVTDormantCounts()
	{
		if (!HasOVTSlotMask())
			return;

		if (GetDormantAliveCount() < 0)
			return;

		int alive = OVT_VirtualizationMath.CountAlive(m_aOVT_SlotAlive);
		SetDormantCounts(alive, m_aOVT_SlotAlive.Count() - alive);
	}

	//------------------------------------------------------------------------------------------------
	//! Slot-accurate refill (D2). Vanilla always spawns `slotIndex == current agent count` - a
	//! first-N refill that structurally destroys identity, so a group that lost its slot-1 MG comes
	//! back with the MG alive and a tail rifleman missing instead. With a mask set, this spawns the
	//! next slot that is ALIVE in the mask and has not been materialised yet.
	//!
	//! Falls through to vanilla for every group core does not own, so unregistered groups (garrisons,
	//! QRFs, deployments, recruit squads, other mods) are untouched.
	//! \return True when a member was created; false at capacity or on a transient spawn failure.
	override bool ExpandOneMember()
	{
		if (!HasOVTSlotMask())
			return super.ExpandOneMember();

		if (!m_aUnitPrefabSlots || m_aUnitPrefabSlots.IsEmpty())
			return false;

		int slotIndex = OVT_VirtualizationMath.NextSlotToSpawn(m_aOVT_SlotAlive, m_aOVT_SpawnedSlots);
		if (slotIndex < 0)
			return false;

		// A mask longer than the roster can only mean the prefab changed under a persisted record.
		// Refuse rather than index out of bounds; the record is corrected on the next relink.
		if (slotIndex >= m_aUnitPrefabSlots.Count())
			return false;

		bool snapToTerrain = true;
		if (s_bIgnoreSnapToTerrain)
		{
			snapToTerrain = false;
			s_bIgnoreSnapToTerrain = false;
		}

		m_iOVT_PendingSlot = slotIndex;
		bool spawned = SpawnGroupMember(snapToTerrain, slotIndex, m_aUnitPrefabSlots[slotIndex], false, false);
		m_iOVT_PendingSlot = -1;

		if (!spawned)
			return false;

		m_aOVT_SpawnedSlots.Insert(slotIndex);

		// Same completion notification vanilla raises, evaluated against the mask instead of the
		// engine's counts (queue-driven spawning never reaches SpawnGroupMember's isLast branch).
		if (IsExpandComplete())
		{
			if (Event_OnAllDelayedEntitySpawned)
				Event_OnAllDelayedEntitySpawned.Invoke(this);

			InvokeEventOnInit();
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Completion test the queue dispatcher uses to tell "at capacity, drop the request" apart from
	//! "transient failure, retry". For a masked group that question is answered by the mask, not by
	//! the engine's (corruptible) dormant counts.
	//! \return True when every mask-alive slot has been materialised.
	override bool IsExpandComplete()
	{
		if (!HasOVTSlotMask())
			return super.IsExpandComplete();

		return OVT_VirtualizationMath.NextSlotToSpawn(m_aOVT_SlotAlive, m_aOVT_SpawnedSlots) < 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Synchronous materialisation. Only reachable on a non-Chimera world (RequestSpawn's fallback,
	//! SCR_AIGroup.c:2698-2704) or from native code, but vanilla's version fills slots 0..n-1 and
	//! would silently break slot accuracy there, so masked groups go through the same seam the queue
	//! uses. Bounded by the mask size - ExpandOneMember records every slot it materialises.
	override void SpawnMembers()
	{
		if (!HasOVTSlotMask())
		{
			super.SpawnMembers();
			return;
		}

		if (GetAgentsCount() > 0)
			return;

		int budget = m_aOVT_SlotAlive.Count();
		for (int i = 0; i < budget; i++)
		{
			if (!ExpandOneMember())
				break;
		}

		SCR_EditableGroupComponent editable = SCR_EditableGroupComponent.Cast(FindComponent(SCR_EditableGroupComponent));
		if (editable)
			editable.OnDormantStateChanged();
	}

	//------------------------------------------------------------------------------------------------
	//! Dormant transition. Vanilla's bookkeeping runs first (it needs the live agent count), then the
	//! mask overwrites the counts it just wrote - see ReassertOVTDormantCounts for why that is
	//! mandatory. The per-activation slot list is dropped because the member entities are gone.
	override void DespawnMembers()
	{
		super.DespawnMembers();

		m_iOVT_PendingSlot = -1;
		if (m_aOVT_SpawnedSlots)
			m_aOVT_SpawnedSlots.Clear();

		ReassertOVTDormantCounts();
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		// Consume the one-shot BEFORE super, which is where vanilla consumes its own statics.
		bool persistenceExempt = s_bOVT_PersistenceExempt;
		s_bOVT_PersistenceExempt = false;
		m_bOVT_PersistenceExempt = persistenceExempt;

		super.EOnInit(owner);

		// Registered virtual groups and groups restored from a save are the campaign's durable
		// records - they are the one category that must NOT be untracked (virtualization/core T3.1).
		if (persistenceExempt)
			return;

		OVT_PersistenceManagerComponent.UntrackTransient(this);
	}

	//------------------------------------------------------------------------------------------------
	override bool AddAIEntityToGroup(IEntity entity)
	{
		bool added = super.AddAIEntityToGroup(entity);

		// The only place a freshly spawned member entity is observable, so the virtualization
		// member -> slot reverse map (which the death hook resolves victims through) is built here.
		if (m_iOVT_PendingSlot >= 0 && entity)
			OVT_VirtualizationManagerComponent.NotifyMemberSpawned(this, m_iOVT_PendingSlot, entity);

		// The group's own member spawning is the only Overthrow path into this method (vanilla's
		// other callers are ScenarioFramework, which Overthrow does not use), so everything that
		// arrives here is rebuild-on-boot AI. Members of a registered virtual group are transient
		// too: the roster truth is the mask on the group record, not a character record each.
		OVT_PersistenceManagerComponent.UntrackTransient(entity);

		return added;
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

		if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity) > 0)
			return;

		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		if (recruits && recruits.GetRecruitFromEntity(entity))
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
