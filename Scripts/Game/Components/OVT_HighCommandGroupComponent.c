//------------------------------------------------------------------------------------------------
//! Marks an SCR_AIGroup as a player-owned HIGH COMMAND group and owns everything about it that lives
//! outside the entity graph: its AI observer and its waypoints (implementation.md §3.5).
//!
//! Server-side state only, never replicated - clients read the manager's records, never the entity.
//!
//! ⚠ NEVER SetLifecyclePolicy on this group. Manual (the engine default) is what "always live" means;
//! ProximityDriven would delete its bodies at 800 m. Never OVT_EntitySpawningAPI.Cleanup* either.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Overthrow", description: "Marks an AI group as a High Command group and owns its observer and waypoints")]
class OVT_HighCommandGroupComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class OVT_HighCommandGroupComponent : ScriptComponent
{
	protected string m_sGroupId;

	protected string m_sOwnerPersistentId;

	//! The group's vehicle, for the refuel tick and the dismiss teardown. INVALID for a foot group.
	protected EntityID m_VehicleEntityId;

	//! Plain pointers, not `ref`: entity lifetime belongs to the engine, and a strong reference from
	//! a component is how a deleted entity is kept alive as a zombie.
	protected ref array<AIWaypoint> m_aOwnedWaypoints = {};

	//------------------------------------------------------------------------------------------------
	//! Queues the observer install for the next call-queue hop.
	//!
	//! ⚠ Deferred, never inline: an entity still inside its own spawn answers GetID() with
	//! EntityID.INVALID, which core refuses - a value every unregistered entity shares, so keying on
	//! it would collide two entities on one map entry (virtualization/core api.md §3).
	//! \param[in] owner The group entity.
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
	//! Parks the AI observer that makes this group wake dormant content around it (D2).
	protected void InstallObserver()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		// The consumer reads its own off-switch (D3); AddEntityObserver serves everybody and consults
		// nobody's knob.
		if (!virtualization.GetHighCommandGroupsAreObservers())
			return;

		if (!virtualization.AddEntityObserver(owner))
			Print("[Overthrow] A High Command group could not be made an AI observer - it will not wake dormant AI around it. See the virtualization manager's warning above for the reason", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Records which group record this entity is (set by the manager immediately after spawn).
	//! \param[in] groupId The record's group id.
	void SetGroupId(string groupId)
	{
		m_sGroupId = groupId;
	}

	//------------------------------------------------------------------------------------------------
	//! \return This group's record id, or an empty string when it was never stamped.
	string GetGroupId()
	{
		return m_sGroupId;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] ownerPersistentId The owning player's persistent id.
	void SetOwnerPersistentId(string ownerPersistentId)
	{
		m_sOwnerPersistentId = ownerPersistentId;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The owning player's persistent id, or an empty string when never stamped.
	string GetOwnerPersistentId()
	{
		return m_sOwnerPersistentId;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] vehicleId The group's vehicle, or EntityID.INVALID for a foot group.
	void SetVehicleEntityId(EntityID vehicleId)
	{
		m_VehicleEntityId = vehicleId;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The group's vehicle id; EntityID.INVALID for a foot group.
	EntityID GetVehicleEntityId()
	{
		return m_VehicleEntityId;
	}

	//------------------------------------------------------------------------------------------------
	//! Hands one of this group's waypoints to the component to dispose of. Call it AFTER wiring the
	//! waypoint to the group; whatever is given here is deleted when the group dies or is re-ordered.
	//! \param[in] waypoint The waypoint to own. Null means "nothing to clean".
	void AddOwnedWaypoint(AIWaypoint waypoint)
	{
		if (waypoint)
			m_aOwnedWaypoints.Insert(waypoint);
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many waypoints this group currently owns.
	int GetOwnedWaypointCount()
	{
		return m_aOwnedWaypoints.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Destroys every waypoint this group owns, so none outlives the order that spawned it.
	//!
	//! REMOVE THEN DELETE, in that order - vanilla's own order (SCR_AIGroup.DestroyEntities) and
	//! Overthrow's (OVT_MultiTownPatrolBehaviorDeploymentModule.c:229-230). Deleting first leaves the
	//! group's waypoint list holding a freed entity. RemoveWaypoint on a cycle child that was never on
	//! the group's own list is a harmless no-op, so every owned waypoint gets both steps.
	void ClearOwnedWaypoints()
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(GetOwner());

		foreach (AIWaypoint waypoint : m_aOwnedWaypoints)
		{
			if (!waypoint)
				continue;

			if (group)
				group.RemoveWaypoint(waypoint);

			SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
		}

		m_aOwnedWaypoints.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Takes down everything this group holds outside the entity graph, by EVERY deletion route -
	//! dismiss, a wipe (vanilla's delete-when-empty) and world teardown alike.
	//!
	//! ORDER IS LOAD-BEARING. The queued install is cancelled FIRST: a group created and destroyed in
	//! one frame would otherwise park an observer following a dead entity. The observer removal is
	//! UNCONDITIONAL: it is the one piece of state nobody else can name a key for, and removing one
	//! that was never added is a silent no-op.
	//! \param[in] owner The group entity being destroyed.
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(InstallObserver);

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (virtualization)
			virtualization.RemoveEntityObserver(owner);

		ClearOwnedWaypoints();

		OVT_HighCommandManagerComponent manager = OVT_HighCommandManagerComponent.GetInstance();
		if (manager && owner)
			manager.OnGroupEntityDeleted(m_sGroupId, owner.GetID());

		super.OnDelete(owner);
	}
}
