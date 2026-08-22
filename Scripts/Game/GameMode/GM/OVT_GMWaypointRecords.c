//------------------------------------------------------------------------------------------------
//! The record shapes carried by the Game Master WAYPOINT ROUTE fan, and the client-side store they
//! land in. The campaign snapshot's own record shapes live next door in OVT_GMRecords.c.
//!
//! ONE SET OF CLASSES, USED ON BOTH SIDES - the same rule OVT_GMRecords.c states and for the same
//! reason. The server reads a route with OVT_GMWaypointWalk, the fan carries it FIELD BY FIELD as RPC
//! parameters (never as objects and never as array<> parameters - neither crosses the wire, and a
//! hand-rolled bitstream is not an option because ScriptBitWriter hard-crashes on first use from
//! script), and the client rebuilds the identical classes inside OVT_GMWaypointRoute.
//!
//! POSITION IS SENT HERE, UNLIKE EVERY OTHER GM RECORD, AND THAT IS THE WHOLE POINT OF THE FEATURE.
//! Vanilla waypoint prefabs carry no RplComponent (Prefabs/AI/Waypoints/AIWaypoint_Base.et has exactly
//! one component, Persistence), so a waypoint entity EXISTS ONLY ON THE SERVER: there is no RplId to
//! resolve and no entity for a client to ask. The group's own position is the one vertex a client can
//! re-read live, which is why m_vGroupPos is a snapshot fallback rather than the source of truth for
//! the first vertex (the renderer re-reads the group entity every frame; see the plan's 3.4).
//!
//! ONE ROUTE, NOT A KEYED STORE (plan 5 D10). At most one route is ever live - the GM's current
//! selection - so the component holds one OVT_GMWaypointRoute plus a staging twin, exactly the shape
//! the snapshot fan uses. If gm-map later wants several at once, promoting one route to a small array
//! is a contained change inside one class.
//!
//! They are plain Managed data with behaviour limited to Clear()/CopyFrom(); anything that looks like
//! geometry belongs in OVT_GMWaypointFormat (pure, Logic-testable) and anything that looks like a
//! world read belongs in the renderer.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! One waypoint of one route: where it is and what kind it is.
//!
//! NO RplId AND NO ENTITY REFERENCE - see the file header. The index is the record's identity within
//! its route and is what m_iCurrentIndex points at; it is the server's emission order, which is the
//! order the group walks them.
//------------------------------------------------------------------------------------------------
class OVT_GMWaypointRecord : Managed
{
	//! Position in the route, 0-based, in the server's emission order. THE JOIN KEY within a route.
	int m_iIndex;

	//! The waypoint entity's GetOrigin() at the moment the route was read. A snapshot: waypoints do
	//! not move, and this feature does not re-poll (plan 5 D3).
	vector m_vPos;

	//! An OVT_EGMWaypointType value, as int because that is what the wire carries. UNKNOWN for any
	//! prefab not in OVT_GMWaypointFormat's table - never a guess.
	int m_iType;
}

//------------------------------------------------------------------------------------------------
//! One AI group's route as the client knows it.
//!
//! m_bComplete is what separates "no route" from "a route with no waypoints": an EMPTY ANSWER IS
//! STILL AN ANSWER. A group that resolves but holds no waypoints, an RplId that resolves to nothing,
//! and a non-group all commit a complete route with zero waypoints, and a consumer that treated that
//! as "nothing arrived" would leave the previous group's route on screen forever.
//!
//! m_aWaypoints is NEVER NULL - a consumer that asks before any route lands gets an empty array
//! rather than a null check it will forget to write.
//------------------------------------------------------------------------------------------------
class OVT_GMWaypointRoute : Managed
{
	//! The route closes back from its last waypoint to its first: the group's waypoint list held an
	//! AIWaypointCycle. Only meaningful with at least two waypoints (plan 3.4).
	static const int FLAG_CYCLIC = 1;

	//! The group held more waypoints than the server's cap and the tail was dropped. Real Overthrow
	//! routes are nine waypoints or fewer, so this should never be set - which is why the server also
	//! logs it.
	static const int FLAG_TRUNCATED = 2;

	//! The group entity's RplId - THE JOIN KEY, and the only entity reference in this feature that
	//! means the same thing on both machines. A consumer resolves it for the group's LIVE position.
	RplId m_GroupRplId;

	//! Index into m_aWaypoints of the waypoint the group is currently executing, or -1 when it is not
	//! one of them at all (the group has none, the engine reported the cycle container rather than a
	//! child, or truncation dropped it). All three are first-class states in which nothing is
	//! highlighted and nothing is printed (plan 5 D5).
	int m_iCurrentIndex = -1;

	//! Bitfield of FLAG_* above.
	int m_iFlags;

	//! Whether a complete Begin...End fan has been committed into this route. False on a freshly
	//! cleared store and on one whose fan is still in flight. See the class header: a complete route
	//! with zero waypoints is a real answer, and this flag is what makes it distinguishable.
	bool m_bComplete;

	//! The group's position when the server read the route. A fallback only: the renderer re-reads the
	//! group entity every frame so a moving group stays connected to its route.
	vector m_vGroupPos;

	//! The route's waypoints in walking order. Never null.
	ref array<ref OVT_GMWaypointRecord> m_aWaypoints = new array<ref OVT_GMWaypointRecord>();

	//------------------------------------------------------------------------------------------------
	//! Whether the route closes back on itself.
	//! \return True when FLAG_CYCLIC is set.
	bool IsCyclic()
	{
		return (m_iFlags & FLAG_CYCLIC) != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the server's cap dropped waypoints from this route.
	//! \return True when FLAG_TRUNCATED is set.
	bool IsTruncated()
	{
		return (m_iFlags & FLAG_TRUNCATED) != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether there is anything to draw: a committed route holding at least one waypoint.
	//! \return True when a consumer should draw this route.
	bool HasRoute()
	{
		return m_bComplete && m_aWaypoints.Count() > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Copies another route's values into this one. Used by the commit path to move a fully staged
	//! route into the live store in ONE step, so no reader can ever observe half a fan.
	//! \param[in] other The staging route.
	void CopyFrom(OVT_GMWaypointRoute other)
	{
		if(!other) return;

		m_GroupRplId = other.m_GroupRplId;
		m_iCurrentIndex = other.m_iCurrentIndex;
		m_iFlags = other.m_iFlags;
		m_bComplete = other.m_bComplete;
		m_vGroupPos = other.m_vGroupPos;

		// Element copies, not an array-object swap: the staging route keeps its array for the next fan
		// and refills it with FRESH record objects, so the two stores never alias a record that is
		// about to be rewritten (the same rule OVT_GMCampaignState.CopyRecords follows).
		m_aWaypoints.Clear();
		foreach(OVT_GMWaypointRecord record : other.m_aWaypoints)
		{
			m_aWaypoints.Insert(record);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Drops everything. Called when a new request is issued and when the editor closes: a stale route
	//! must never be mistaken for the current selection's.
	void Clear()
	{
		m_GroupRplId = RplId.Invalid();
		m_iCurrentIndex = -1;
		m_iFlags = 0;
		m_bComplete = false;
		m_vGroupPos = vector.Zero;

		m_aWaypoints.Clear();
	}
}
