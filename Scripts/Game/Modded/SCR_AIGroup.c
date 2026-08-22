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

	//! Latch: core's ForceSpawn explicitly asked this MANUAL-policy group to materialise. Armed by
	//! ArmOVTManualSpawn() before the RequestSpawn, honoured at queue-dispatch time (the request sits
	//! in the engine queue for seconds), cleared on DespawnMembers so every force spawn re-arms.
	protected bool m_bOVT_ManualSpawnArmed;

	//------------------------------------------------------------------------------------------------
	// SPAWN-QUEUE INSTRUMENTATION (insertion investigation, 2026-08-21)
	//
	// ==========================================================================================
	// 🔴 THE ONE QUESTION THREE ROUNDS OF PLAY-TESTING COULD NOT ANSWER: was this group ASKED and
	// REFUSED, or was it NEVER ASKED?
	// ==========================================================================================
	// A transport crew registered on a 100 km ring sat at zero members for a minute at a time while
	// every observable we had said it should be spawning: the ring was fine, an observer was inside it,
	// the AI budget allowed it, no evictions, the pop-in band was clear, the mask read 2 of 2 and the
	// refill seam still had slots to fill. Every one of those is a statement about the group's
	// ELIGIBILITY. None of them says whether ChimeraAIWorld's spawn queue ever actually dispatched it,
	// and "eligible but never dispatched" and "dispatched but refused every time" are completely
	// different bugs with no overlap in their fixes.
	//
	// These counters answer it in one play-test:
	//   requests HIGH, expands ZERO  - the queue is taking our requests and never coming back to us.
	//                                  A throughput/ordering problem, not a script one. See
	//                                  OVT_InsertionSpawningDeploymentModule.CREW_IMPORTANCE.
	//   requests HIGH, expands HIGH  - we ARE being dispatched and refusing. The refusal string names
	//                                  which branch, and every one of them is ours to fix.
	//   requests ZERO                - nothing is asking at all: the lifecycle tick is not running on
	//                                  this entity. A completely different investigation.
	//
	// ⚠ THEY COUNT EVERY GROUP IN THE WORLD, NOT JUST OURS, and that is deliberate - four ints and a
	// string per group entity, incremented on paths that already exist, with no allocation and no work
	// unless somebody asks for the string. Scoping them to masked groups would have made the "requests
	// ZERO" case unreadable, because a group core does not own is exactly what a comparison needs.
	//------------------------------------------------------------------------------------------------

	//! How many times RequestSpawn has been called on this group - i.e. how many times somebody asked
	//! the engine queue for members. The lifecycle tick alone contributes about one per second while a
	//! ProximityDriven group is empty and in range.
	protected int m_iOVT_SpawnRequests;

	//! How many times the queue has actually dispatched ExpandOneMember at this group. THE number.
	protected int m_iOVT_ExpandCalls;

	//! Of those, how many produced no member.
	protected int m_iOVT_ExpandRefusals;

	//! Of the refusals, how many got all the way to SpawnGroupMember and had IT refuse. That call has
	//! exactly two false returns in vanilla and both are the navmesh tile branch
	//! (SCR_AIGroup.c:1692-1719), so a non-zero count here reads "the navmesh under the spawn point is
	//! not loaded" and nothing else.
	protected int m_iOVT_SpawnMemberFailures;

	//! Which branch refused most recently, in words.
	protected string m_sOVT_LastExpandRefusal;

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
	//------------------------------------------------------------------------------------------------
	//! Core's ForceSpawn calls this before RequestSpawn so the dispatch is let through. Inert for
	//! ProximityDriven groups (the guard below never engages for them).
	void ArmOVTManualSpawn()
	{
		m_bOVT_ManualSpawnArmed = true;
	}

	//------------------------------------------------------------------------------------------------
	//! True when every spawn dispatch must be refused: this group is core-owned (masked), stamped
	//! "never materialise by proximity" (Manual policy, spawnDistanceOverride 0), and core has not
	//! armed a force spawn.
	//!
	//! WHY THIS EXISTS. Vanilla enqueues a FULL spawn at entity init when the group prefab sets
	//! m_bSpawnImmediately (SCR_AIGroup.c:2595-2601), and the queue re-validates only OBSERVER
	//! presence at dispatch time - so any observer (a GM camera, the autotest camera) materialises
	//! the group seconds after registration regardless of the policy core stamped. The refusal has
	//! to happen at DISPATCH, not at init: the init-time request is queued before core has stamped
	//! the mask or the policy.
	protected bool RefusesUnrequestedManualSpawn()
	{
		return HasOVTSlotMask()
			&& GetLifecyclePolicy() == SCR_EAIGroupLifecyclePolicy.Manual
			&& !m_bOVT_ManualSpawnArmed;
	}

	//------------------------------------------------------------------------------------------------
	//! ONE LINE THAT SEPARATES "never dispatched" FROM "dispatched and refused". See the counter block
	//! above for why those two are the whole question.
	//! \return A compact human-readable state of this group's dealings with the engine spawn queue.
	string GetOVTSpawnQueueDiagnostic()
	{
		string refusal = m_sOVT_LastExpandRefusal;
		if (refusal.IsEmpty())
			refusal = "none";

		return string.Format("%1 spawn request(s) made, queue dispatched %2, %3 refused (%4 at SpawnGroupMember, i.e. navmesh), last refusal: %5",
			m_iOVT_SpawnRequests.ToString(), m_iOVT_ExpandCalls.ToString(), m_iOVT_ExpandRefusals.ToString(),
			m_iOVT_SpawnMemberFailures.ToString(), refusal);
	}

	//------------------------------------------------------------------------------------------------
	//! Counts every ask, then hands straight on to vanilla.
	//!
	//! ⚠ IT CHANGES NOTHING AND MUST NOT. The whole value of the count is that it is taken on the real
	//! path with the real arguments; a wrapper that filtered, throttled or deduplicated would be
	//! measuring itself. The defaults are repeated verbatim from SCR_AIGroup.c:2678 so a caller that
	//! omits them reaches vanilla with exactly what it would have had.
	//! \param[in] slotsWanted How many members the queue should ultimately spawn; -1 derives it.
	//! \param[in] observerRange Drop the request when no observer is within this at dispatch time.
	override void RequestSpawn(int slotsWanted = -1, float observerRange = 0)
	{
		m_iOVT_SpawnRequests = m_iOVT_SpawnRequests + 1;

		super.RequestSpawn(slotsWanted, observerRange);
	}

	override bool ExpandOneMember()
	{
		if (!HasOVTSlotMask())
		{
			// ⚠ COUNTED FOR UNMASKED GROUPS TOO. A group core does not own is the control sample: if
			// garrisons are being dispatched and registered crews are not, that comparison IS the bug
			// report, and it is unavailable if only our own groups are counted.
			m_iOVT_ExpandCalls = m_iOVT_ExpandCalls + 1;

			bool vanillaSpawned = super.ExpandOneMember();
			if (!vanillaSpawned)
			{
				m_iOVT_ExpandRefusals = m_iOVT_ExpandRefusals + 1;
				m_sOVT_LastExpandRefusal = "vanilla refused (no mask on this group)";
			}

			return vanillaSpawned;
		}

		m_iOVT_ExpandCalls = m_iOVT_ExpandCalls + 1;

		// Manual-policy guard: refuse the dispatch outright. IsExpandComplete answers TRUE for the
		// same condition so the dispatcher books the request as complete and drops it, rather than
		// retrying a group that will never accept.
		if (RefusesUnrequestedManualSpawn())
			return RefuseExpand("Manual policy and no force spawn armed");

		if (!m_aUnitPrefabSlots || m_aUnitPrefabSlots.IsEmpty())
			return RefuseExpand("the group prefab declares no member slots");

		int slotIndex = OVT_VirtualizationMath.NextSlotToSpawn(m_aOVT_SlotAlive, m_aOVT_SpawnedSlots);
		if (slotIndex < 0)
			return RefuseExpand("every slot the mask calls alive has already been materialised");

		// A mask longer than the roster can only mean the prefab changed under a persisted record.
		// Refuse rather than index out of bounds; the record is corrected on the next relink.
		if (slotIndex >= m_aUnitPrefabSlots.Count())
			return RefuseExpand("the mask is longer than the prefab roster");

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
		{
			// ⚠ VANILLA HAS EXACTLY TWO false RETURNS IN SpawnGroupMember AND BOTH ARE THE NAVMESH TILE
			// BRANCH (SCR_AIGroup.c:1692-1719); a prefab that fails to spawn returns TRUE (:1740). So this
			// counter reads "the navmesh under the spawn point was not loaded" and nothing else - and that
			// branch gives up and spawns anyway after NAVMESH_STALL_LIMIT (30) attempts, so a count that
			// keeps climbing past 30 means something has reset the stall counter, which only
			// DespawnMembers does.
			m_iOVT_SpawnMemberFailures = m_iOVT_SpawnMemberFailures + 1;

			return RefuseExpand("SpawnGroupMember refused - the navmesh tile under the spawn point");
		}

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
	//! Books a refusal and its reason, and answers false so the caller can `return RefuseExpand(...)`.
	//!
	//! A helper rather than four copies of two lines, because the reason strings are the entire point:
	//! a refusal count with no reason is the same dead end the whole investigation has been stuck in.
	//! \param[in] reason Why this dispatch produced nobody.
	//! \return False, always.
	protected bool RefuseExpand(string reason)
	{
		m_iOVT_ExpandRefusals = m_iOVT_ExpandRefusals + 1;
		m_sOVT_LastExpandRefusal = reason;

		return false;
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

		// The other half of the Manual-policy guard (see ExpandOneMember): "complete" makes the
		// queue drop the request instead of retrying it every dispatch forever.
		if (RefusesUnrequestedManualSpawn())
			return true;

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

		// Manual-policy guard - the synchronous fallback path (non-Chimera worlds) refuses too.
		if (RefusesUnrequestedManualSpawn())
			return;

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

		// A force-spawned Manual group goes back to refusing dispatches the moment it despawns -
		// every ForceSpawn re-arms.
		m_bOVT_ManualSpawnArmed = false;

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

		// A High Command member body is the campaign's durable record of that man's GEAR (D8): its
		// persistence id is what the load walk asks the persistence system for, and an untracked body
		// has no id at all. Registered with the HC manager BEFORE this call, exactly as recruits are.
		OVT_HighCommandManagerComponent highCommand = OVT_HighCommandManagerComponent.GetInstance();
		if (highCommand && highCommand.IsMemberBody(entity))
			return added;

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
	//! group, whatever spawned it, so the exclusions here are what protect the three character
	//! categories that must STAY tracked:
	//!   - player bodies (player-controlled at join time, on every vanilla join path);
	//!   - recruit bodies (registered in the recruit manager BEFORE the group add on all three
	//!     flows - AddRecruit, AttachRecruitBody, and the existing-entity re-add);
	//!   - High Command member bodies (registered in the HC manager BEFORE the group add, on both
	//!     flows - SpawnMembers and the load walk's AdoptRestoredBody). Same precedent, same
	//!     ordering requirement, no second mechanism.
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

		OVT_HighCommandManagerComponent highCommand = OVT_HighCommandManagerComponent.GetInstance();
		if (highCommand && highCommand.IsMemberBody(entity))
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
