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
	//! \param[in] owner The group entity being destroyed.
	override void OnDelete(IEntity owner)
	{
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
