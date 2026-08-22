//------------------------------------------------------------------------------------------------
//! Why a given Overthrow-spawned AI group exists.
//!
//! UNKNOWN is the zero value and is deliberately never written by any call site: an entry carrying it
//! means a Tag() call passed a default-constructed origin, which is a bug the Campaign-tier case
//! asserts against. Values are appended at the END when a new spawn site appears - the enum is not
//! persisted and not replicated, so reordering costs nothing today, but the snapshot fan sends the
//! integer and a client built against a different order would mislabel every group.
//------------------------------------------------------------------------------------------------
enum OVT_EGroupOrigin
{
	UNKNOWN,
	BASE_PATROL,
	BASE_DEFENCE,
	BASE_SNIPER,
	TOWER_GUARD,
	TOWN_PATROL,
	RADIO_TOWER_GARRISON,
	QRF,
	DEPLOYMENT,
	JOB
}

//------------------------------------------------------------------------------------------------
//! One registry entry: the provenance of a single AI group.
//!
//! Plain Managed data, held by value in the registry's map. It never crosses the wire itself - the
//! snapshot builder copies its three fields into an RPC alongside the group's RplId.
//------------------------------------------------------------------------------------------------
class OVT_GMGroupOrigin : Managed
{
	//! An OVT_EGroupOrigin value. Stored as int because that is what the wire carries.
	int m_iType;

	//! The base index, town id or radio-tower id that produced the group, or -1 when the origin has
	//! no meaningful index (QRF, deployments, jobs). Base indices are positional into
	//! OVT_OccupyingFactionManager.m_Bases, which is the join key clients already receive through JIP.
	int m_iIndex;

	//! Free text naming the concrete producer - the upgrade's ClassName(), the deployment's config
	//! name, the job's stable id. Diagnostic detail for a GM, never parsed by anything.
	string m_sReason;

	//------------------------------------------------------------------------------------------------
	//! \param[in] type An OVT_EGroupOrigin value.
	//! \param[in] index Base index / town id / tower id, or -1.
	//! \param[in] reason Concrete producer name.
	void OVT_GMGroupOrigin(int type, int index, string reason)
	{
		m_iType = type;
		m_iIndex = index;
		m_sReason = reason;
	}
}

//------------------------------------------------------------------------------------------------
//! SERVER-SIDE record of which Overthrow subsystem spawned which AI group, and what for.
//!
//! WHY IT EXISTS. A Game Master looking at a group standing in a field can see the entity but not the
//! campaign decision behind it. Overthrow has no chokepoint for group spawns - eleven distinct spawn
//! statements across ten files - so provenance is recorded where it is known: at the spawn site, one
//! statement, right beside the existing tracking insertion.
//!
//! IT TAGS AND IT SWEEPS. IT NEVER UNTAGS, AND THERE MUST BE NO UNTAG METHOD ON IT.
//! The alternative was surveyed and rejected on evidence: there are 27 delete/drop sites for these
//! groups and the existing cleanup already leaks in documented ways - QRF groups are never deleted at
//! all (OVT_QRFControllerComponent.m_Groups is filled and drained nowhere), and the deployments
//! framework's old shared group cleanup deleted a group's soldiers but not the group entity (that
//! helper and its whole file are gone as of virtualization/integration Phase 5, which is the cheapest
//! possible fix for it). Mirroring that with 27 untag calls would inherit
//! every one of those leaks and would silently rot the moment a future feature adds a spawn site.
//! Sweep() drops entries whose EntityID no longer resolves, which handles all of them uniformly and
//! cannot be forgotten. A spawn site that forgets to Tag() produces a MISSING record, never a WRONG
//! one - a missing icon, not a lie.
//!
//! !! LIVENESS IS ENTITY RESOLUTION AND NOTHING ELSE. Do NOT prune on GetAgentsCount() == 0.
//! Reforger 1.8's AI spawn queue and dormancy break the "0 agents = dead" inference (recorded in this
//! project's reforger-1.8-update.md, still unfixed): a perfectly alive group whose members are queued
//! or dormant reports zero agents, and pruning on that would lose its origin permanently - there is
//! no re-tag path, because the group is only tagged once, at spawn.
//!
//! NEVER PERSISTED, NEVER REPLICATED. Modded/SCR_AIGroup untracks every AI group from persistence
//! (BUG-118) and Overthrow rebuilds all of them from manager state on every boot, so a saved registry
//! would name entities that no longer exist. The only reader is OVT_GMSnapshotBuilder, on the server;
//! records reach clients as owner-targeted RPCs in the GM snapshot fan, keyed on RplId.
//!
//! A SCRIPTED SINGLETON, NOT A MANAGER. It has no entity, no replication, no config surface and one
//! consumer. A manager component would cost a prefab edit on OVT_OverthrowGameMode.et plus an
//! OVT_Global accessor for no benefit. Note the consequence: the instance lives for the process, not
//! for the campaign, which is exactly why Sweep() has to be correct - a world change leaves every
//! entry stale, and the first sweep after it clears them all.
//------------------------------------------------------------------------------------------------
class OVT_GMGroupRegistry
{
	//-----------------------------------------------------------------------
	// STATIC
	//-----------------------------------------------------------------------

	protected static ref OVT_GMGroupRegistry s_Instance;

	//-----------------------------------------------------------------------
	// MEMBER VARIABLES
	//-----------------------------------------------------------------------

	//! Group EntityID -> why it was spawned. Strong ref on the value: the map is the only owner of
	//! every origin object, and the entities it is keyed on are not.
	protected ref map<EntityID, ref OVT_GMGroupOrigin> m_mOrigins;

	//! Scratch list reused by Sweep() so a sweep per snapshot does not allocate. Never read outside it.
	protected ref array<EntityID> m_aSweepScratch;

	//-----------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------

	void OVT_GMGroupRegistry()
	{
		m_mOrigins = new map<EntityID, ref OVT_GMGroupOrigin>();
		m_aSweepScratch = new array<EntityID>();
	}

	//------------------------------------------------------------------------------------------------
	//! Lazily created on first use, which is the first Tag() call of the session.
	//! \return The process-wide registry. Never null.
	static OVT_GMGroupRegistry GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new OVT_GMGroupRegistry();
		}

		return s_Instance;
	}

	//-----------------------------------------------------------------------
	// TAGGING
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Records why a freshly spawned group exists. THE ONLY WRITE PATH.
	//!
	//! Static so that every call site is genuinely one statement dropped in beside the spawn's existing
	//! tracking insertion, changing no control flow. Both guards below are what make that safe:
	//!  - a null group is ignored, so a site may tag unconditionally after a spawn that can fail;
	//!  - a non-server caller is ignored, so any client-local throwaway group is excluded structurally
	//!    rather than by anyone remembering to exclude it.
	//!
	//! Re-tagging an EntityID overwrites the previous entry. That is intended: EntityIDs are recycled by
	//! the engine, and the newest spawn is by definition the current truth for that id.
	//!
	//! \param[in] group The spawned group entity. Null is a no-op.
	//! \param[in] originType An OVT_EGroupOrigin value.
	//! \param[in] originIndex Base index / town id / tower id, or -1 when there is none.
	//! \param[in] reason Concrete producer name, for GM diagnostics.
	static void Tag(IEntity group, int originType, int originIndex, string reason)
	{
		if (!group) return;
		if (!Replication.IsServer()) return;

		GetInstance().Set(group.GetID(), originType, originIndex, reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Instance half of Tag(). Kept separate so the static stays a pure guard.
	//! \param[in] id The group's EntityID.
	//! \param[in] originType An OVT_EGroupOrigin value.
	//! \param[in] originIndex Base index / town id / tower id, or -1.
	//! \param[in] reason Concrete producer name.
	protected void Set(EntityID id, int originType, int originIndex, string reason)
	{
		m_mOrigins.Set(id, new OVT_GMGroupOrigin(originType, originIndex, reason));
	}

	//-----------------------------------------------------------------------
	// SWEEPING
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Drops every entry whose group entity no longer exists. THE ONLY REMOVAL PATH.
	//!
	//! Called by the snapshot builder immediately before it walks the registry, so a GM never sees a
	//! record for a dead group and the map cannot grow without bound across a long campaign.
	//!
	//! Entity resolution is the ONLY liveness test used here - see the class header on why agent counts
	//! are not one under Reforger 1.8.
	//!
	//! Keys are collected first and removed afterwards: mutating a map while iterating it is undefined
	//! in EnforceScript, and the index-based walk below would skip entries as the map compacts.
	void Sweep()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world) return;

		m_aSweepScratch.Clear();

		for (int i = 0; i < m_mOrigins.Count(); i++)
		{
			EntityID id = m_mOrigins.GetKey(i);
			if (!world.FindEntityByID(id))
			{
				m_aSweepScratch.Insert(id);
			}
		}

		foreach (EntityID dead : m_aSweepScratch)
		{
			m_mOrigins.Remove(dead);
		}

		m_aSweepScratch.Clear();
	}

	//-----------------------------------------------------------------------
	// READING
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The recorded provenance of one group.
	//! \param[in] id The group's EntityID.
	//! \return Its origin, or null when the group was never tagged or has already been swept.
	OVT_GMGroupOrigin Find(EntityID id)
	{
		OVT_GMGroupOrigin origin;
		if (!m_mOrigins.Find(id, origin)) return null;

		return origin;
	}

	//------------------------------------------------------------------------------------------------
	//! Entries currently held, including any not yet swept.
	//! \return The entry count.
	int Count()
	{
		return m_mOrigins.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Copies the whole registry out in two index-aligned arrays.
	//!
	//! Two parallel arrays rather than one map copy because the caller (the snapshot builder, and the
	//! Campaign-tier case) wants a stable ordered walk and neither owns the entries.
	//!
	//! \param[out] ids Receives every tagged group's EntityID. Cleared first.
	//! \param[out] origins Receives the matching origins, index-aligned with ids. Cleared first.
	void GetAll(out array<EntityID> ids, out array<ref OVT_GMGroupOrigin> origins)
	{
		if (ids) ids.Clear();
		if (origins) origins.Clear();

		for (int i = 0; i < m_mOrigins.Count(); i++)
		{
			if (ids) ids.Insert(m_mOrigins.GetKey(i));
			if (origins) origins.Insert(m_mOrigins.GetElement(i));
		}
	}
}
