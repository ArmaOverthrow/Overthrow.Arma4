//------------------------------------------------------------------------------------------------
//! Virtual movement - one dormant group's progress along its own waypoint plan.
//!
//! ============================== TRANSIENT. NOTHING HERE IS EVER SAVED ==============================
//! No serializer, no payload, no entry in Configs/Systems/Persistence/Overthrow.conf, no replicated
//! property, no attribute. EVERY field below is re-derivable from GetPosition(handle) plus
//! GetRecord(handle).m_Plan, which is the whole reason this feature needs no persistence of its own:
//! core already saves the LIVE group origin, so after a load the group is in the right place and the
//! only thing missing - which leg it was walking - is recovered by projecting that position back onto
//! the plan polyline (OVT_VirtualMovementMath.ProjectOntoPlan).
//!
//! That makes losing the whole state map self-healing rather than a defect: a world teardown, a
//! Continue, a restart, a spawn cycle or a consumer teleporting a group all cost at most one re-walk
//! of part of one leg. A "movement forgot where a patrol was" report is EXPECTED BEHAVIOUR.
//!
//! The one thing projection cannot recover is DIRECTION on a non-cycling ping-pong route: a resumed
//! group may walk its route the other way. Accepted by the requirements - nobody can observe where a
//! dormant patrol "should" have been.
//! ==================================================================================================
//!
//! Owned exclusively by OVT_VirtualMovementManagerComponent's state map (server-only, and the map is
//! never allocated on a client). Plain Managed: no attributes, no serialization, no lifecycle.
//------------------------------------------------------------------------------------------------
class OVT_VirtualMovementState : Managed
{
	//! Index into m_Plan.m_aPositions the group is walking toward. Re-derived by projecting the
	//! group's actual position onto the plan when the state is (re)built.
	int m_iTargetIndex;

	//! +1 / -1 walk direction, flipped at either end of a NON-cycling multi-point route (the ping-pong
	//! of D8). Always +1 for a cycling plan, and always +1 on a fresh derivation - projection cannot
	//! recover which way the group was going.
	int m_iDirection;

	//! Seconds left at a WAIT waypoint; 0 means "not waiting". Drained by the per-group dt, never by a
	//! per-tick assumption.
	float m_fWaitRemaining;

	//! LATCHED once there is nothing left to advance: a DEFEND waypoint was reached, or the route ran
	//! out with nowhere to go. A latched state is kept in the map ON PURPOSE - dropping it would let the
	//! next pass re-derive a movable plan and walk the group away from the post it just took up.
	bool m_bStationary;

	//! The unclamped straight-line accumulator. The WATER RULE needs it: progress keeps accruing
	//! through a leg that crosses water even though nothing is written while it does, so the group
	//! reappears advancing on the far side instead of deadlocking at the shore forever.
	vector m_vVirtual;

	//! The last position handed to SetPosition() - THE EXTERNAL-MOVE ORACLE. Compared (in XZ only,
	//! because every write is ground-snapped) against what the registry reports each pass; a gap means
	//! somebody else moved this group and the state must be thrown away and re-derived.
	vector m_vLastWritten;

	//! World time in MILLISECONDS at the last touch - the per-group dt source. Stamped by every branch
	//! that ends a pass, so a group can never bank elapsed time while it is waiting or stationary and
	//! then teleport when it resumes.
	float m_fLastTickMs;
}
