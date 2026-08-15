//------------------------------------------------------------------------------------------------
//! Keeps the AI navmesh in step with the structures Overthrow adds to and removes from the world.
//!
//! WHY THIS EXISTS. The navmesh is BAKED INTO THE WORLD FILE. Anything spawned at runtime is
//! invisible to pathfinding until something asks the engine to re-carve the tiles it covers, and
//! anything deleted at runtime leaves its carved hole behind forever. Vanilla exposes this as
//! SCR_AIWorld.RequestNavmeshRebuildEntity()/RequestNavmeshRebuildAreas(); this class is the one
//! place Overthrow calls them, so every path that changes the world's obstacles is accounted for
//! in a single file.
//!
//! THE PATHS THAT NEED IT. Placing (OVT_ResistanceFactionManager.PlaceItem), building (BuildItem),
//! base upgrades (OVT_SlottedBaseUpgrade), removal (RemovePlacedItem and the camp / FOB area
//! cleanups), and - the one that was missed - RESTORING A SAVE. Placeables and buildables are
//! SelfSpawn 1 in Configs/Systems/Persistence/Overthrow.conf, so the persistence system respawns
//! them on load; the navmesh that comes back with the world knows nothing about them. Vanilla does
//! not cover this either: there is not one navmesh reference in the whole of
//! scripts/Game/Plugins/Persistence. Without the load-side rebuild every structure built in an
//! earlier session is something the AI walks straight into.
//!
//! IMMEDIATE VERSUS QUEUED. RebuildNow() is for a single object appearing in response to a player
//! action - the player wants their squad to walk around the thing they just built, now. Queue() is
//! for the bulk cases (a save restoring dozens of objects at once) and for deletions, and does two
//! things a loop of RebuildNow() calls cannot:
//!
//!   1. It MERGES overlapping work. GetNavmeshRebuildAreas() expands an existing area rather than
//!      adding a new one when the bounds are within MAX_NAVMESH_REBUILD_SIZE of it, but only for
//!      areas in the array it is handed - so accumulating every entity into ONE array turns a base
//!      full of sandbags into a handful of tiles instead of one rebuild request each.
//!   2. It DELAYS. For a deletion the areas must be captured while the entity still exists but the
//!      rebuild must happen after it is gone, which is exactly what vanilla does at
//!      SCR_EditableEntityComponent.c:864 and SCR_CampaignBuildingDisassemblyUserAction.c:237.
//!
//! CAPTURE BEFORE YOU DELETE. Queue() reads the entity's bounds when it is CALLED, not when the
//! rebuild runs, and stores plain vectors afterwards. That is what makes it safe on the removal
//! path and why removal callers must call it BEFORE SCR_EntityHelper.DeleteEntityAndChildren(),
//! never after.
//!
//! SERVER ONLY IN PRACTICE, WITHOUT A SERVER CHECK. Every caller is already server-side (the place
//! and build endpoints, the removal endpoint, and the persistence serializers, which only ever run
//! on the authority). AI pathfinding is server-side too, so a client rebuild would be wasted work
//! rather than wrong work. No guard is added here because none of the call sites can reach it from
//! a client.
//------------------------------------------------------------------------------------------------
class OVT_NavmeshRebuild
{
	//! How long queued work waits before it is issued. Doubles as the grace period a deletion needs
	//! to actually complete - vanilla uses the same 1000 ms for exactly that reason.
	static const int COALESCE_DELAY_MS = 1000;

	protected static ref OVT_NavmeshRebuild s_Instance;

	//! Accumulated (min bounds, max bounds) pairs awaiting a flush. Held as an instance member so the
	//! flush can be scheduled as an instance method - CallLater binds a method on an object.
	protected ref array<ref Tuple2<vector, vector>> m_aAreas;

	//! Parallel to m_aAreas: whether the road network needs rebuilding for that area too.
	protected ref array<bool> m_aRedoRoads;

	protected bool m_bFlushScheduled;

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the navmesh around a single entity and its children immediately.
	//!
	//! Use at a player-driven spawn site (place, build, upgrade) where the object needs to be visible
	//! to pathfinding as soon as it appears. Null entity and null AI world are both tolerated: the AI
	//! world guard matters because the call sites that used to do this inline had none, and a VM error
	//! there would abort the rest of the placement - including ownership stamping and the persistence
	//! tracking call that follows it.
	//! \param[in] entity The freshly spawned entity.
	static void RebuildNow(IEntity entity)
	{
		if (!entity)
			return;

		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return;

		aiWorld.RequestNavmeshRebuildEntity(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Captures the area an entity occupies now and schedules a merged rebuild of everything queued.
	//!
	//! Two uses, both of which depend on the capture happening at call time:
	//!  - BULK APPEARANCE (a save restoring many placeables): each entity adds its bounds to the same
	//!    array, so overlapping structures collapse into one rebuild request.
	//!  - DISAPPEARANCE: call this while the entity is still in the world, THEN delete it. The delay
	//!    is what lets the rebuild see the world without it.
	//! \param[in] entity The entity whose surroundings need rebuilding.
	static void Queue(IEntity entity)
	{
		if (!entity)
			return;

		if (!s_Instance)
			s_Instance = new OVT_NavmeshRebuild();

		s_Instance.QueueInternal(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Accumulates an entity's rebuild areas and makes sure a flush is coming.
	//! \param[in] entity The entity to measure. Already null-checked by Queue().
	protected void QueueInternal(notnull IEntity entity)
	{
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return;

		if (!m_aAreas)
		{
			m_aAreas = new array<ref Tuple2<vector, vector>>();
			m_aRedoRoads = new array<bool>();
		}

		// Expands m_aAreas in place, merging into a neighbouring area when there is one - this is the
		// whole reason the array is a member and not a local.
		aiWorld.GetNavmeshRebuildAreas(entity, m_aAreas, m_aRedoRoads);

		// An entity with no mesh, no physics, or physics outside the navmesh layers contributes
		// nothing. Scheduling a flush for it would burn a frame on an empty request.
		if (m_aAreas.IsEmpty())
			return;

		if (m_bFlushScheduled)
			return;

		m_bFlushScheduled = true;
		GetGame().GetCallqueue().CallLater(Flush, COALESCE_DELAY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Issues every area accumulated since the flush was scheduled, as one batch, and resets.
	protected void Flush()
	{
		m_bFlushScheduled = false;

		if (!m_aAreas || m_aAreas.IsEmpty())
			return;

		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (aiWorld)
		{
			Print("[Overthrow] Navmesh rebuild: " + m_aAreas.Count() + " area(s)", LogLevel.VERBOSE);
			aiWorld.RequestNavmeshRebuildAreas(m_aAreas, m_aRedoRoads);
		}

		// Cleared whether or not the request went out - a stale area would be re-issued on every
		// subsequent flush for the rest of the session.
		m_aAreas.Clear();
		m_aRedoRoads.Clear();
	}
}
