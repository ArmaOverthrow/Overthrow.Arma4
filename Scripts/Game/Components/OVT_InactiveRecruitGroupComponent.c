//------------------------------------------------------------------------------------------------
//! Marks an SCR_AIGroup as an Overthrow INACTIVE-RECRUIT group, and owns the lifetime of its hold
//! waypoints (the move/wait pair and the cycle that reruns them).
//!
//! THE GROUP ENTITY IS THE RECORD. There is deliberately NO server-side registry of inactive
//! groups (decision D6). A registry would be a second truth that can drift out of step with the
//! world, and it would hold pointers into a group class that DELETES ITSELF one frame after its
//! last member leaves (SCR_AIGroup.OnEmpty -> CallLater(DeleteEntityAndChildren, 1),
//! Entities/SCR_AIGroup.c:2442-2455). So everything the recruit manager needs is derived from the
//! world instead: "is this group one of ours?" is answered by finding THIS component on it, and by
//! nothing else.
//!
//! WHY THE WAYPOINTS LIVE HERE. AIGroup.AddWaypoint() does NOT take ownership - a waypoint is an
//! ordinary world entity that vanilla destroys explicitly for the ones IT spawned
//! (SCR_AIGroup.DestroyEntities, :1871-1886, called from ~SCR_AIGroup). Ours are spawned by the
//! manager, so without OnDelete() below every inactive group that emptied would leave its hold
//! waypoints standing in the world forever. Holding them on the component - rather than in a
//! manager map - means no waypoint can outlive its group even when the group is destroyed by a
//! path we did not write, which is the common case (vanilla's delete-when-empty).
//!
//! SERVER-SIDE STATE ONLY, NEVER REPLICATED. Both fields are meaningful only where the AI actually
//! runs. Clients are told which recruits are inactive through the recruit manager's own broadcast
//! (RpcDo_RecruitActiveStateChanged) and its JIP payload; they are never told anything about the
//! groups, because the grouping has no player-observable identity (decision D8).
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Overthrow", description: "Marks an AI group as an Overthrow inactive-recruit group and owns its hold waypoints")]
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

	//! Every waypoint this group was given when it was created - since the wander fix that is
	//! three: the move and wait waypoints of the hold loop, and the cycle waypoint that reruns
	//! them. Only the cycle is on the group's own waypoint list; SetWaypoints() does not parent
	//! its children to anything, so each one has to be owned and deleted here individually.
	//!
	//! The entities are NOT held by `ref` (the array container is). Entity lifetime belongs to the
	//! engine; a strong reference to an IEntity from a component is how a deleted entity gets kept
	//! alive as a zombie. These are plain pointers that are only ever dereferenced in OnDelete(),
	//! one line before the entities they name are destroyed.
	protected ref array<AIWaypoint> m_aOwnedWaypoints = {};

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
	//! Hands one of this group's waypoints to the component to dispose of.
	//!
	//! Call this AFTER the waypoint is wired to the group (AddWaypoint() for the cycle,
	//! SetWaypoints() for its children), and only for a waypoint the caller spawned for THIS group:
	//! the component deletes whatever it is given when the group dies.
	//! \param[in] waypoint The waypoint to own. Null is accepted and simply means "nothing to clean".
	void AddOwnedWaypoint(AIWaypoint waypoint)
	{
		if (waypoint)
			m_aOwnedWaypoints.Insert(waypoint);
	}

	//------------------------------------------------------------------------------------------------
	//! Destroys the group's waypoints so none outlives its group.
	//!
	//! This is the ONLY place an inactive group's waypoints are disposed of, deliberately: the group
	//! is destroyed by vanilla's delete-when-empty far more often than by anything Overthrow calls,
	//! so cleanup hung off any of the manager's own code paths would be cleanup that mostly does not
	//! run. Hanging it off the entity's own deletion covers every route at once.
	//!
	//! Removed from the group before being deleted, which is the order vanilla uses for the
	//! waypoints it spawned itself (SCR_AIGroup.DestroyEntities :1878-1882) and the order Overthrow
	//! already uses when it re-waypoints a live group
	//! (OVT_MultiTownPatrolBehaviorDeploymentModule.c:229-230). Deleting first would leave the
	//! group's waypoint list holding a freed entity for as long as the group takes to finish dying.
	//! RemoveWaypoint() on a cycle's child that was never on the group's own list is a harmless
	//! no-op, so every owned waypoint gets the same two steps.
	//! \param[in] owner The group entity being destroyed.
	override void OnDelete(IEntity owner)
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(owner);

		foreach (AIWaypoint waypoint : m_aOwnedWaypoints)
		{
			if (!waypoint)
				continue;

			if (group)
				group.RemoveWaypoint(waypoint);

			SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
		}

		m_aOwnedWaypoints.Clear();

		super.OnDelete(owner);
	}
}
