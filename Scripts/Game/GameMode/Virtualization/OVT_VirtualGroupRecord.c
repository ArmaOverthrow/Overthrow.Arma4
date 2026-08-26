//------------------------------------------------------------------------------------------------
//! Virtualization core - the registry record and its registration-time input types.
//!
//! THIS FILE IS THE EPIC'S CONTRACT (implementation.md §3.3). Four downstream features
//! (`civilians`, `movement`, `integration`, `base-defense-migration`) are planned against these
//! shapes; treat any change after Phase 2 as breaking and record it in context.md.
//!
//! SERVER ONLY. Nothing here replicates, and nothing here has a client half (D7).
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Waypoint kinds a consumer may ask for in a registration plan. Core maps each onto the
//! centralized waypoint helpers on OVT_OverthrowConfigComponent when it builds the real
//! AIWaypoint entities (Phase 3, T3.2).
//------------------------------------------------------------------------------------------------
enum OVT_EVirtualWaypointType
{
	MOVE,
	PATROL,
	WAIT,
	DEFEND,
	CYCLE,
	//! Walk to the point and SEARCH around it for the parameter's seconds - the vanilla Search & Destroy
	//! waypoint, which has the men investigate navmesh positions (house interiors included, where a
	//! building has them) inside a fixed radius until its holding time expires, then completes. The
	//! parameter is the HOLD in seconds, exactly like WAIT; the radius is core's constant. While dormant
	//! it is a WAIT: arrive, hold, advance. APPENDED 2026-08-21 (occupying/deployments town sweep) -
	//! additive, at the end, so persisted plans authored before it read back unchanged.
	SEARCH
}

//------------------------------------------------------------------------------------------------
//! Registration-time INPUT ONLY (not persisted state - the built AIWaypoint entities are the
//! persistent truth, via vanilla's AIWaypoint.conf). Core turns this into real waypoint
//! entities attached to the group at registration; they survive dormancy with the group.
//!
//! The three arrays are PARALLEL: entry i of m_aTypes and m_aParams describes m_aPositions[i].
//! OVT_VirtualizationMath.ValidateWaypointPlan() is the integrity check core runs before it
//! accepts a plan; a plan that fails it is refused with a WARNING rather than half-built.
//------------------------------------------------------------------------------------------------
class OVT_VirtualWaypointPlan
{
	ref array<vector> m_aPositions = {};
	ref array<int> m_aTypes = {};       //!< OVT_EVirtualWaypointType, parallel to m_aPositions
	ref array<float> m_aParams = {};    //!< per-type parameter (wait seconds, patrol radius), parallel
	bool m_bCycle;
}

//------------------------------------------------------------------------------------------------
//! Registry bookkeeping. Lifecycle state (dormant counts, position, spawned members) lives on
//! the SCR_AIGroup entity - the engine's durable record. This class only carries what the
//! engine does not know: who owns the group and how it was composed.
//!
//! ⚠ m_aSlotAlive is the AUTHORITATIVE survivor truth (D2 + Phase 1 amendment: "the mask is the
//! only roster truth"). The engine's dormant alive/dead counts are scratch values that provably
//! corrupt themselves when a despawn lands mid-refill (context.md T1.2: 6→2 with zero kills), so
//! core re-asserts SetDormantCounts() from this mask after every despawn (Phase 3, T3.5) and
//! never reads the engine counts as truth when the mask is populated.
//------------------------------------------------------------------------------------------------
class OVT_VirtualGroupRecord
{
	int m_iHandle;                      //!< stable identity, survives save/load
	string m_sOwnerSystem;              //!< "deployment", "tower_garrison", "economy_delivery", ...
	string m_sOwnerKey;                 //!< consumer-defined identity used to reclaim after load

	string m_sFactionKey;               //!< "USSR" - NEVER the faction index (D3)
	string m_sGroupRegistryName;        //!< "light_patrol"
	ResourceName m_rResolvedPrefab;     //!< fallback if the registry entry is gone

	int m_iSpawnDistanceOverride = -1;  //!< -1 = use the global config value
	int m_eImportance;                  //!< SCR_EAISpawnImportance stamped at registration (D4)

	vector m_vPosition;                 //!< where the group was registered, kept in step with
	                                    //!< SetPosition(). The group entity's origin is the live
	                                    //!< truth while it exists; this is what answers when it does
	                                    //!< not, and what a re-creation would anchor on.

	ref OVT_VirtualWaypointPlan m_Plan; //!< the plan the owned waypoints were built from, KEPT rather
	                                    //!< than dropped: waypoint entities cannot self-spawn on load
	                                    //!< under Overthrow's config (see api.md "Persistence"), so
	                                    //!< rebuilding them from the plan is the restore path.

	ref array<int> m_aSlotAlive = {};   //!< 1/0 per prefab roster slot, captured at registration.
	                                    //!< Core's AUTHORITATIVE survivor truth (D2) - persisted,
	                                    //!< pushed to the modded group, corrects engine counts.
	                                    //!< EMPTY UNTIL PHASE 3: the roster size is only knowable
	                                    //!< once the group entity exists.

	SCR_AIGroup m_Group;                //!< the engine-side record; null only between wipe and cleanup
	EntityID m_GroupId;                 //!< m_Group's id. The SAFE handle: every internal touch of
	                                    //!< m_Group goes through FindEntityByID(m_GroupId) first,
	                                    //!< because the engine can delete the group entity under us
	                                    //!< (eliminate-when-reached, world teardown) with no callback.
	ref array<AIWaypoint> m_aOwnedWaypoints = {};  //!< created from the plan; core deletes on unregister (D6)

	//! True from the moment the engine announces a dormant teardown (GetOnMembersDespawning) until
	//! the group next materialises a member. THE teardown-window guard (D2a): a kill event that
	//! arrives while this is set describes a member the engine is deleting, not a casualty.
	bool m_bDespawning;
}

//------------------------------------------------------------------------------------------------
//! One entry of the member -> (handle, slot) reverse map (D2).
//!
//! Built as members materialise (the modded SCR_AIGroup.AddAIEntityToGroup seam) and torn down when
//! the engine despawns them, so it only ever describes members that exist RIGHT NOW. That is what
//! lets the death hook answer "which roster slot just died?" from a victim entity alone.
//------------------------------------------------------------------------------------------------
class OVT_VirtualMemberSlot : Managed
{
	int m_iHandle;
	int m_iSlot;
}
