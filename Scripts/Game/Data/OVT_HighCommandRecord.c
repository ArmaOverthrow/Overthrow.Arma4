//! How a High Command group behaves once it reaches its destination. APPEND-ONLY: this crosses the
//! wire and the save, so inserting a member re-points every existing value at the wrong behaviour.
enum OVT_EHighCommandStance
{
	//! Hunker at the destination.
	DEFEND,
	//! Cycle a perimeter around the destination.
	PATROL,
	//! Search & destroy at the destination.
	ATTACK
}

//------------------------------------------------------------------------------------------------
//! One High Command group's record - the only thing a client ever reads (implementation.md §3.2).
//!
//! World-free by design: nothing here may call a manager or dereference an entity, or Logic-tier
//! coverage of it silently dies. Live entity handles are [NonSerialized()] and server-only; the
//! member arrays are save-only (D8) and never cross the wire.
//------------------------------------------------------------------------------------------------
class OVT_HighCommandRecord : Managed
{
	string m_sGroupId;
	string m_sOwnerPersistentId;

	//! Names the authored OVT_HighCommandGroupEntry - display name, map icon and composition.
	string m_sEntryKey;

	int m_iStance = OVT_EHighCommandStance.DEFEND;

	vector m_vDestination;
	vector m_vLastKnownPosition;

	//! OVT_HighCommandStatus bits.
	int m_iStatusFlags;

	int m_iAliveMembers;
	int m_iTotalMembers;

	//! Save-only (D8): persisted body UUIDs, so a converted recruit's gear survives a reload.
	ref array<string> m_aMemberBodyIds = {};

	//! Save-only (D8): the fallback prefab per member when a stored body does not come back.
	ref array<ResourceName> m_aMemberPrefabs = {};

	//! [NonSerialized] SERVER ONLY - never wire, never save.
	EntityID m_GroupEntityId;

	//! [NonSerialized] SERVER ONLY - never wire, never save.
	EntityID m_VehicleEntityId;
}
