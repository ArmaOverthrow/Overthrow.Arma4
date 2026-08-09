//------------------------------------------------------------------------------------------------
//! Takes an entity out of play WITHOUT taking it out of the world - the "reservation" half of
//! BUG-086.
//!
//! WHY THIS EXISTS, AND WHAT IT REPLACES. Overthrow used to park a disconnected player's body and an
//! offline owner's locked vehicle in STORAGE: PersistenceSystem.Save() + StopTracking(keepData) +
//! delete, then RequestSpawn() by id when the owner came back. That is vanilla's own idiom - but
//! vanilla only ever uses it across a save/load boundary, and it turns out the kept record is NOT
//! durable. Measured on a dedicated server 2026-08-05, IN SESSION and with no restart: a body and a
//! vehicle released milliseconds apart at the same disconnect were BOTH unresolvable ten minutes
//! later (`Persistence answered NOT_FOUND`, `could not be read back from storage - rebuilt ...
//! (contents lost)`). Nothing may depend on a record outliving its entity.
//!
//! SO THE ENTITY IS NEVER DESTROYED. It stays alive, stays TRACKED (so it serializes normally and is
//! instantiated again by SelfSpawn after a restart), and keeps its own transform - which is what makes
//! "you come back standing where you logged out, carrying what you were carrying" true by construction
//! rather than by a round trip through storage. Fidelity is perfect because nothing is serialized and
//! nothing is rebuilt.
//!
//! THIS IS VANILLA'S RESERVATION MODEL WITH AN OVERTHROW-LENGTH TIMEOUT. SCR_ReconnectComponent keeps
//! a disconnected player's LIVE entity in SCR_ReconnectData.m_ReservedEntity for 120 s and never
//! serializes it. Overthrow keeps it indefinitely (user decision, 2026-08-05: no expiry - full
//! restoration matters more than the memory, and Overthrow servers are 10-20 players), and unlike
//! vanilla it also HIDES it, because a body standing lootable for two weeks would make an offline
//! player a free supply crate.
//!
//! WHAT THE FLAGS ACTUALLY BUY, STATED HONESTLY. ClearFlags/SetFlags are LOCAL engine calls - they are
//! not replicated (there is no replication anywhere in the IEntity proto, and every vanilla caller,
//! e.g. SCR_FlagComponent.c:56-64, invokes them on the machine that needs the effect). On the
//! authority, which is the machine that matters:
//!
//!   - TRACEABLE cleared -> server-side traces miss it. Bullets, AI perception and use-actions are all
//!     resolved on the authority, so a reserved body cannot be shot, seen by AI or interacted with;
//!   - ACTIVE cleared    -> the engine stops updating it, so it does not simulate, animate or tick;
//!   - VISIBLE cleared   -> rendering, which a dedicated server does not do at all. It is cleared
//!     anyway because it is the pair vanilla always clears with TRACEABLE, and because it doubles as
//!     this class's IsReserved() test.
//!
//! THE RESIDUAL, AND WHERE TO FIX IT IF THE PLAY-TEST SHOWS IT MATTERS: a CLIENT already streaming the
//! entity in keeps rendering its own copy, so a player walking past a logged-out player's spot may see
//! a motionless body. It cannot be shot, looted or interacted with (all authority-side), so this is
//! cosmetic - but if it needs solving, Reserve()/Release() are the only two places to change.
//!
//! NON-RECURSIVE ON PURPOSE. `recursively: true` would also clear VISIBLE on every child entity - the
//! character's clothing, weapons and the items inside them - and the restore could not be exact,
//! because GetFlags() only reports the root. Re-showing an item that the inventory system had
//! deliberately hidden is a worse bug than the one being fixed. Only the root is touched, and the
//! properties that matter (traces, simulation) are root properties.
//!
//! SERVER ONLY BY CONVENTION: every caller is authority-side. Nothing here needs a persistence system,
//! so it is safe to call in any world.
//------------------------------------------------------------------------------------------------
class OVT_PersistenceReservation
{
	//------------------------------------------------------------------------------------------------
	//! Hides an entity and stops it simulating, leaving it alive, tracked and where it stands.
	//!
	//! IDEMPOTENT: an entity that is already reserved is left alone, so the periodic sweeps that
	//! re-reserve after a world load can run as often as they like.
	//!
	//! DELIBERATELY DOES NOT TOUCH PERSISTENCE. The entity must stay tracked - that is the whole point -
	//! so there is no Save()/StopTracking() here. Callers that want the record refreshed call
	//! OVT_PersistenceTracking.Save() themselves, which is a write, not a release.
	//! \param[in] entity The entity to reserve. Null is tolerated.
	//! \return True when the entity is now reserved (including when it already was).
	static bool Reserve(IEntity entity)
	{
		if (!entity)
			return false;

		if (IsReserved(entity))
			return true;

		// A character that was walking when its player left would keep its movement input. Vanilla does
		// exactly this on the path this class replaces (SCR_BaseGameMode.c:967-969, the reconnect branch
		// of OnPlayerDisconnected), and it must happen while the entity is still ACTIVE to take effect.
		CharacterControllerComponent charController = CharacterControllerComponent.Cast(entity.FindComponent(CharacterControllerComponent));
		if (charController)
			charController.SetMovement(0, vector.Forward);

		entity.ClearFlags(EntityFlags.VISIBLE | EntityFlags.TRACEABLE);
		entity.ClearFlags(EntityFlags.ACTIVE);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Brings a reserved entity back into play, exactly where it was left.
	//!
	//! IDEMPOTENT: an entity that is not reserved is left alone.
	//! \param[in] entity The entity to release. Null is tolerated.
	//! \return True when the entity is now in play (including when it already was).
	static bool Release(IEntity entity)
	{
		if (!entity)
			return false;

		if (!IsReserved(entity))
			return true;

		// ACTIVE first: the entity has to be updating again before it is put back in front of anything
		// that can trace it.
		entity.SetFlags(EntityFlags.ACTIVE);
		entity.SetFlags(EntityFlags.VISIBLE | EntityFlags.TRACEABLE);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether an entity is currently reserved.
	//!
	//! STATELESS BY DESIGN - the answer is read off the entity itself rather than from a registry, so
	//! it cannot disagree with reality and no map has to be persisted, pruned or repaired. The one
	//! consequence, and it is the wanted one: after a world load a self-spawned entity comes back with
	//! its PREFAB's flags, i.e. not reserved, and the sweeps that run after a load re-reserve it.
	//!
	//! Testing VISIBLE is vanilla's own idiom for this question (SCR_EditablePreviewComponent.c:55,
	//! SCR_MultiPartDeployableItemComponent.c:672).
	//! \param[in] entity The entity to ask about. Null answers false.
	//! \return True when the entity has been hidden by Reserve().
	static bool IsReserved(IEntity entity)
	{
		if (!entity)
			return false;

		return !(entity.GetFlags() & EntityFlags.VISIBLE);
	}
}
