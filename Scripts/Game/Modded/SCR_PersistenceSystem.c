//------------------------------------------------------------------------------------------------
//! BUG-116 INSTRUMENTATION. Reports what the persistence system throws away while loading a save.
//!
//! THE DEFECT THIS EXISTS TO DIAGNOSE. On the live dedicated server, hundreds of stored inventory
//! items fail to come back on every restart, and the engine says so in a native error with no mod
//! context:
//!
//!   DEFAULT (E): [PERSISTENCE] Attempted to deserialize meta data for an already existing
//!   id:019fe18f-…. Instance:@"ENTITY:…" ('GenericEntity','…/Magazine_…') at <0,0,0> @"{…}….et".
//!
//! Two things that error does NOT say are exactly the two things needed to fix it:
//!   1. whether the rejected instance is then DESTROYED (i.e. whether the item is really lost, or
//!      merely left untracked) - so far an inference from the player's symptom, not a measurement;
//!   2. WHICH CONTAINER it was being restored into, which is half of the duplicated pair.
//!
//! Five reproduction attempts on a local dedicated server failed to trigger the fault (items at
//! rest, a stack split across two containers, eight restart cycles past the reporting server's save
//! size, looting a player corpse, looting an Overthrow-spawned AI body), so this ships instead of
//! another guess: the next restart of an affected server answers both questions from its log.
//!
//! WHY THIS HOOK. `HandleDelete` is the scripted event the persistence system raises for every
//! entity it destroys, and its own documentation lists the reason we are hunting:
//! "Reasons for deletion include: - Removed in previous session and the change is being applied on
//! load - **Deserialize failure that was configured not to be tolerated**"
//! (PersistenceSystem.c). So a rejected item passes through here, still attached to the storage it
//! was being restored into - which is what makes naming the container possible at all.
//!
//! WHAT IT CANNOT SEE, stated so nobody re-derives it: the rejected id itself. Assignment already
//! failed by the time the entity reaches deletion, so `GetId()` answers null. The id is in the
//! engine's own error line; this hook supplies the container and the fate. Pair them by prefab and
//! by order - both sequences are emitted in the same load, in the same order.
//!
//! COST WHEN NOTHING IS WRONG: one integer increment per deleted entity and one summary line per
//! load. Deletions outside a load are counted but not itemised.
//------------------------------------------------------------------------------------------------
modded class SCR_PersistenceSystem
{
	//! Detail lines emitted per load before falling back to counting only. The reporting server
	//! produced 744 rejections in a single load; itemising all of them would bury the summary and
	//! the per-container tally is the useful part anyway.
	protected static const int OVT_MAX_DETAIL_LINES = 25;

	//! Deletions seen since the current load began.
	protected int m_iOvtDeletedThisLoad;

	//! Detail lines emitted so far this load.
	protected int m_iOvtDetailLines;

	//! Persistent id of the container an item was being restored into -> how many it lost.
	//! Keyed by id string so a container that is itself untracked lands under "" rather than
	//! colliding with a real record.
	protected ref map<string, int> m_mOvtLostByContainer;

	//! Prefab of the deleted entity -> how many. Matches the engine's own error lines by prefab.
	protected ref map<string, int> m_mOvtLostByPrefab;

	//------------------------------------------------------------------------------------------------
	//! Logs every entity the persistence system destroys, then hands it to vanilla unchanged.
	//! \param[in] entity The entity being deleted.
	override protected void HandleDelete(IEntity entity)
	{
		OVT_RecordPersistenceDelete(entity);

		super.HandleDelete(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Emits the per-load summary, then hands off to vanilla's notification handling.
	//! \param[in] success False when deserialization failed in a way the engine considers fatal.
	override protected void OnAfterLoad(bool success)
	{
		OVT_ReportLoadDeletions(success);

		super.OnAfterLoad(success);
	}

	//------------------------------------------------------------------------------------------------
	//! Accumulates one deletion. Kept separate from the override so the override stays a two-liner
	//! that is obviously still calling super.
	//! \param[in] entity The entity being deleted.
	protected void OVT_RecordPersistenceDelete(IEntity entity)
	{
		if (!entity)
			return;

		if (!m_mOvtLostByContainer)
			m_mOvtLostByContainer = new map<string, int>();

		if (!m_mOvtLostByPrefab)
			m_mOvtLostByPrefab = new map<string, int>();

		m_iOvtDeletedThisLoad++;

		string prefab = OVT_PrefabNameOf(entity);
		int prefabCount = 0;
		if (m_mOvtLostByPrefab.Contains(prefab))
			prefabCount = m_mOvtLostByPrefab[prefab];

		m_mOvtLostByPrefab.Set(prefab, prefabCount + 1);

		// The container is the nearest ancestor the system has an id for. An item being restored
		// into a storage is a hierarchy child of it, so this walks straight up to the box, the
		// vehicle or the character it was going into.
		string containerId;
		string containerPrefab;
		IEntity parent = entity.GetParent();
		while (parent)
		{
			UUID parentUuid = GetId(parent);
			if (!parentUuid.IsNull())
			{
				containerId = parentUuid;
				containerPrefab = OVT_PrefabNameOf(parent);
				break;
			}

			parent = parent.GetParent();
		}

		int containerCount = 0;
		if (m_mOvtLostByContainer.Contains(containerId))
			containerCount = m_mOvtLostByContainer[containerId];

		m_mOvtLostByContainer.Set(containerId, containerCount + 1);

		if (m_iOvtDetailLines >= OVT_MAX_DETAIL_LINES)
			return;

		m_iOvtDetailLines++;

		if (containerId.IsEmpty())
		{
			PrintFormat("[Overthrow] persistence deleted %1 - no tracked container in its hierarchy", prefab);
			return;
		}

		PrintFormat("[Overthrow] persistence deleted %1 - was being restored into %2 (id %3)",
			prefab, containerPrefab, containerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Prints the per-load tally and resets it. Silent on a clean load, which is the normal case and
	//! must stay quiet on servers that do not have this fault.
	//! \param[in] success The engine's own verdict on the load.
	protected void OVT_ReportLoadDeletions(bool success)
	{
		if (m_iOvtDeletedThisLoad == 0)
		{
			OVT_ResetLoadDeletionTally();
			return;
		}

		PrintFormat("[Overthrow] PERSISTENCE LOAD LOSS: the persistence system destroyed %1 entities during this load (engine load success: %2). Each one is a stored record that did NOT come back - see BUG-116.",
			m_iOvtDeletedThisLoad, success, level: LogLevel.WARNING);

		if (m_mOvtLostByContainer)
		{
			for (int i = 0; i < m_mOvtLostByContainer.Count(); i++)
			{
				string containerId = m_mOvtLostByContainer.GetKey(i);
				int lost = m_mOvtLostByContainer.GetElement(i);

				if (containerId.IsEmpty())
				{
					PrintFormat("[Overthrow]   %1 lost from entities with no tracked container (loose, or the parent was not attached yet)", lost);
					continue;
				}

				PrintFormat("[Overthrow]   %1 lost from container %2", lost, containerId);
			}
		}

		if (m_mOvtLostByPrefab)
		{
			for (int i = 0; i < m_mOvtLostByPrefab.Count(); i++)
			{
				PrintFormat("[Overthrow]   %1 x %2", m_mOvtLostByPrefab.GetElement(i), m_mOvtLostByPrefab.GetKey(i));
			}
		}

		OVT_ResetLoadDeletionTally();
	}

	//------------------------------------------------------------------------------------------------
	//! Clears the tally so a second load in the same session starts from zero.
	protected void OVT_ResetLoadDeletionTally()
	{
		m_iOvtDeletedThisLoad = 0;
		m_iOvtDetailLines = 0;

		if (m_mOvtLostByContainer)
			m_mOvtLostByContainer.Clear();

		if (m_mOvtLostByPrefab)
			m_mOvtLostByPrefab.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab an entity came from, for log lines. Falls back to the class name so a deletion is
	//! never reported as an empty string.
	//! \param[in] entity The entity to name.
	//! \return Its prefab resource name, or its class name when it has no prefab data.
	protected string OVT_PrefabNameOf(notnull IEntity entity)
	{
		EntityPrefabData prefabData = entity.GetPrefabData();
		if (prefabData)
		{
			ResourceName prefabName = prefabData.GetPrefabName();
			if (!prefabName.IsEmpty())
				return prefabName;
		}

		return entity.ToString();
	}
}
