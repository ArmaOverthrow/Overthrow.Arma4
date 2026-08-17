//------------------------------------------------------------------------------------------------
//! Marks an SCR_AIGroup as an Overthrow INACTIVE-RECRUIT group, and owns its defend waypoint's
//! lifetime.
//!
//! THE GROUP ENTITY IS THE RECORD. There is deliberately NO server-side registry of inactive
//! groups (decision D6). A registry would be a second truth that can drift out of step with the
//! world, and it would hold pointers into a group class that DELETES ITSELF one frame after its
//! last member leaves (SCR_AIGroup.OnEmpty -> CallLater(DeleteEntityAndChildren, 1),
//! Entities/SCR_AIGroup.c:2442-2455). So everything the recruit manager needs is derived from the
//! world instead: "is this group one of ours?" is answered by finding THIS component on it, and by
//! nothing else.
//!
//! WHY THE WAYPOINT LIVES HERE. AIGroup.AddWaypoint() does NOT take ownership - a waypoint is an
//! ordinary world entity that vanilla destroys explicitly for the ones IT spawned
//! (SCR_AIGroup.DestroyEntities, :1871-1886, called from ~SCR_AIGroup). Ours is spawned by the
//! manager, so without OnDelete() below every inactive group that emptied would leave a defend
//! waypoint standing in the world forever. Holding it on the component - rather than in a manager
//! map - means the waypoint cannot outlive its group even when the group is destroyed by a path we
//! did not write, which is the common case (vanilla's delete-when-empty).
//!
//! SERVER-SIDE STATE ONLY, NEVER REPLICATED. Both fields are meaningful only where the AI actually
//! runs. Clients are told which recruits are inactive through the recruit manager's own broadcast
//! (RpcDo_RecruitActiveStateChanged) and its JIP payload; they are never told anything about the
//! groups, because the grouping has no player-observable identity (decision D8).
//!
//! ================= IT IS ALSO AN AI OBSERVER (virtualization/integration Phase 6, D16) =============
//! A parked squad makes the virtualization core park an engine observer that FOLLOWS this group
//! entity, so registered enemy groups inside its spawn ring materialise with NO PLAYER ANYWHERE NEAR
//! - "leave a squad in a town and the town's patrol comes to life" is the behaviour that asks for.
//!
//! ⚠ THAT IS ALSO WHAT IT COSTS. For as long as this group stands there, every registered group
//! inside the ring stays materialised with its AI running - a squad parked near a radio tower holds
//! that tower's garrison awake indefinitely. The operator's off-switch is the virtualization
//! manager's m_bRecruitGroupsAreObservers attribute (default ON), read below.
//!
//! WHY THE WIRING LIVES HERE and not in the recruit manager: this component is created with the
//! group and destroyed with it, by every route including vanilla's delete-when-empty. That is the
//! same argument the waypoint above rests on, and it means the observer cannot outlive the squad it
//! is following even when the group dies through a path Overthrow did not write. There is no manager
//! plumbing, no registry and no subscription into the recruit transfer paths.
//! ===================================================================================================
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Overthrow", description: "Marks an AI group as an Overthrow inactive-recruit group and owns its defend waypoint")]
class OVT_InactiveRecruitGroupComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class OVT_InactiveRecruitGroupComponent : ScriptComponent
{
	//! Persistent id of the player whose inactive recruits this group holds.
	//!
	//! Server-only. It is bookkeeping for diagnostics and for keeping one owner's parked squad from
	//! being confused with another's in a log - the manager never selects a host group by reading
	//! this, because it reaches groups through the OWNER'S OWN recruit records in the first place.
	protected string m_sOwnerPersistentId;

	//! The defend waypoint this group was given when it was created.
	//!
	//! NOT a `ref`. Entity lifetime belongs to the engine; a strong reference to an IEntity from a
	//! component is how a deleted entity gets kept alive as a zombie. This is a plain pointer that
	//! is only ever dereferenced in OnDelete(), one line before the entity it names is destroyed.
	protected AIWaypoint m_Waypoint;

	//------------------------------------------------------------------------------------------------
	//! THE SERVER-SIDE CREATION HOOK for the AI observer (D16). Queues the install for the next frame.
	//!
	//! The group prefab is replicated, so this event also runs on every client - where
	//! Replication.IsServer() is false and the manager's own guard would refuse anyway.
	//!
	//! ⚠ WHY IT IS DEFERRED BY A FRAME INSTEAD OF ADDING THE OBSERVER RIGHT HERE. Core keys its
	//! observer map on the followed entity's EntityID, and an entity that is not world-registered yet
	//! answers GetID() with EntityID.INVALID - a value EVERY unregistered entity shares. Core refuses
	//! those outright (it measured two entities colliding on one map entry), so an add made from
	//! inside this entity's own initialisation - which is what OnPostInit is, one call deep inside the
	//! prefab spawn that created it - would be refused and the squad would silently never become an
	//! observer. One call-queue hop puts the add on a frame where the entity unambiguously exists in
	//! the world, which is the state core's stale-entity sweep also assumes (it drops any observer
	//! whose id no longer resolves through FindEntityByID).
	//!
	//! The queued call is cancelled in OnDelete, so a group created and destroyed in the same frame -
	//! the recruit manager's own refusal paths do exactly that - never parks an observer at all.
	//! \param[in] owner The group entity this component was created on.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		if (!GetGame() || !GetGame().GetCallqueue())
			return;

		GetGame().GetCallqueue().CallLater(InstallObserver, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Parks the AI observer that makes this squad pull dormant content awake. One frame after
	//! OnPostInit, so the group entity is world-registered and has a real EntityID.
	//!
	//! Nothing about the group is read: the observer follows the group ENTITY with zero offsets, so it
	//! needs no position, no faction and no members, and it is correct from here until the entity is
	//! destroyed.
	//!
	//! Silently does nothing when there is no virtualization manager (a world without the game mode -
	//! the autotest worlds spawn this prefab too) or when the operator has switched the behaviour off.
	//! A refusal by core IS logged, because a parked squad that quietly fails to wake anything is the
	//! exact failure this feature exists to prevent.
	protected void InstallObserver()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		// THE OFF-SWITCH IS READ HERE, NOT INSIDE THE CORE CALL. AddEntityObserver serves every
		// consumer; this attribute is about parked recruits only, so the consumer is what consults it.
		if (!virtualization.GetRecruitGroupsAreObservers())
			return;

		if (!virtualization.AddEntityObserver(owner))
			Print("[Overthrow] An inactive-recruit group could not be made an AI observer - the squad will not wake dormant AI near it. See the virtualization manager's warning above for the reason", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Records which player's parked recruits this group holds.
	//! \param[in] ownerPersistentId The owning player's persistent id.
	void SetOwnerPersistentId(string ownerPersistentId)
	{
		m_sOwnerPersistentId = ownerPersistentId;
	}

	//------------------------------------------------------------------------------------------------
	//! The player whose parked recruits this group holds.
	//! \return The owner's persistent id, or an empty string when the group was never stamped.
	string GetOwnerPersistentId()
	{
		return m_sOwnerPersistentId;
	}

	//------------------------------------------------------------------------------------------------
	//! Hands this group's defend waypoint to the component to dispose of.
	//!
	//! Call this AFTER AddWaypoint(), and only for a waypoint the caller spawned for THIS group: the
	//! component deletes whatever it is given when the group dies.
	//! \param[in] waypoint The waypoint to own. Null is accepted and simply means "nothing to clean".
	void SetWaypoint(AIWaypoint waypoint)
	{
		m_Waypoint = waypoint;
	}

	//------------------------------------------------------------------------------------------------
	//! This group's defend waypoint, or null when it was never given one.
	//! \return The waypoint.
	AIWaypoint GetWaypoint()
	{
		return m_Waypoint;
	}

	//------------------------------------------------------------------------------------------------
	//! Destroys the group's waypoint so none outlives its group.
	//!
	//! This is the ONLY place an inactive group's waypoint is disposed of, deliberately: the group
	//! is destroyed by vanilla's delete-when-empty far more often than by anything Overthrow calls,
	//! so cleanup hung off any of the manager's own code paths would be cleanup that mostly does not
	//! run. Hanging it off the entity's own deletion covers every route at once.
	//!
	//! Removed from the group before being deleted, which is the order vanilla uses for the
	//! waypoints it spawned itself (SCR_AIGroup.DestroyEntities :1878-1882) and the order Overthrow
	//! already uses when it re-waypoints a live group
	//! (OVT_MultiTownPatrolBehaviorDeploymentModule.c:229-230). Deleting first would leave the
	//! group's waypoint list holding a freed entity for as long as the group takes to finish dying.
	//!
	//! IT ALSO TAKES THE AI OBSERVER DOWN, and that half runs FIRST and UNCONDITIONALLY. First,
	//! because it is the one piece of state held outside this world's entity graph - an observer left
	//! behind keeps every registered group near this spot materialised for the rest of the session,
	//! with nobody able to name the key that would remove it. Unconditionally, because asking "did I
	//! add one?" would only re-derive an answer the manager already holds: removing an entity it never
	//! parked an observer for is a silent no-op, and doing it regardless also cleans up an observer
	//! that got here by some route this component did not write.
	//!
	//! The QUEUED install is cancelled before anything else. A group can be created and destroyed
	//! inside one frame - the recruit manager deletes a group whose first member could not be added -
	//! and a pending install firing after that would park an observer following a dead entity, which
	//! only the manager's 2 s sweep would ever clean up.
	//! \param[in] owner The group entity being destroyed.
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(InstallObserver);

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (virtualization)
			virtualization.RemoveEntityObserver(owner);

		if (m_Waypoint)
		{
			SCR_AIGroup group = SCR_AIGroup.Cast(owner);
			if (group)
				group.RemoveWaypoint(m_Waypoint);

			SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
			m_Waypoint = null;
		}

		super.OnDelete(owner);
	}
}
