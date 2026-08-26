//------------------------------------------------------------------------------------------------
//! One REGISTERED ambient source: a config paired with a place in the world (implementation.md
//! §3.4, T4.2).
//!
//! The config is the declaration ("what a market crowd is"); this is the instance ("the crowd in
//! Levie"). Many instances can share one config, which is why the config carries no per-position
//! state and this carries no authored fields.
//!
//! ⚠ NOTHING HERE IS EVER PERSISTED. m_aEntities, m_bActive and the spawn-progress pair are all
//! discarded on despawn and never written to a save; the next approach re-rolls from the config.
//! Phase 5's serializer asserts this by construction - the ambient collections are simply not in its
//! payload.
//------------------------------------------------------------------------------------------------
class OVT_AmbientSpawnSourceInstance : Managed
{
	//! Handle handed back by RegisterAmbientSource. Monotonic, never reused within a session.
	int m_iHandle;

	//! The declaration this instance spawns from. A `ref` even though the registry usually owns the
	//! config too: a consumer may build a config in code and drop its own reference immediately.
	ref OVT_AmbientSpawnSourceConfig m_Config;

	//! Where the source sits. Proximity is measured from here, and the scatter is centred on it.
	vector m_vPosition;

	//! Consumer-defined identity, so a consumer can find its own sources back without holding handles.
	string m_sOwnerKey;

	//! Entities this source currently owns and will delete on despawn.
	//!
	//! Deliberately NOT a `ref array<ref IEntity>`: entities are world-owned, not script-owned, and
	//! the manager re-checks every element before use because a plain entity pointer is all that can
	//! be held across frames. Released entities (ownership transfer) are removed from here, which is
	//! the whole mechanism by which they survive the next despawn.
	ref array<IEntity> m_aEntities;

	//! True between an activation (an observer arrived) and a despawn (they all left).
	bool m_bActive;

	//! SPAWN-PROGRESS STATE. The count is rolled ONCE per activation - a source that rolled 20
	//! civilians must not spawn 20 in one frame - and then spent across ticks: m_iRollTarget is what
	//! this activation asked for, m_iSpawnCursor is how many of them have been built so far. Both are
	//! reset on despawn, because a despawn keeps nothing.
	int m_iRollTarget;
	int m_iSpawnCursor;

	//------------------------------------------------------------------------------------------------
	void OVT_AmbientSpawnSourceInstance()
	{
		m_aEntities = new array<IEntity>();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when this activation still owes entities.
	bool HasPendingSpawns()
	{
		return m_bActive && m_iSpawnCursor < m_iRollTarget;
	}

	//------------------------------------------------------------------------------------------------
	//! Begins an activation with a freshly rolled entity count. Idempotent by the m_bActive guard, so
	//! a second proximity hit cannot re-roll a source that is already filling.
	//! \param[in] rolledCount What the config's RollCount() returned.
	void BeginActivation(int rolledCount)
	{
		if (m_bActive)
			return;

		m_bActive = true;
		m_iRollTarget = rolledCount;
		if (m_iRollTarget < 0)
			m_iRollTarget = 0;

		m_iSpawnCursor = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Forgets everything about the current activation. The manager deletes the entities; this only
	//! clears the bookkeeping, so the next approach re-rolls from the config.
	void ResetActivation()
	{
		m_bActive = false;
		m_iRollTarget = 0;
		m_iSpawnCursor = 0;

		if (m_aEntities)
			m_aEntities.Clear();
	}
}
