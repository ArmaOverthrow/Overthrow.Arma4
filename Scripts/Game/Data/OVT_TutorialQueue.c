//------------------------------------------------------------------------------------------------
//! The client's pending-tutorial queue: what to show next, once something can be shown at all.
//!
//! Popups are shown ONE at a time, and a trigger can fire while a menu is open, while another popup
//! is up, or while the player is dead. This holds those entries until the gate opens rather than
//! dropping them (which is the visible half of the requirement "the popup appears after the menu
//! closes, not never and not on top").
//!
//! Ordering is priority-descending with FIFO inside each priority band, and the order is
//! established on INSERT rather than on removal: the queue is always sorted, so TryDequeue is a
//! constant-time head removal and there is no sort to be unstable.
//!
//! Pure and world-free - no timers, no widgets, no components. The 1000 ms pump that drains it and
//! the gate that decides whether it may drain live in the component.
//------------------------------------------------------------------------------------------------
class OVT_TutorialQueue
{
	//! Hard cap on pending entries. A queue longer than this is a bug upstream, not a backlog: it
	//! would take the player minutes to read through, by which time the tips are about something
	//! they did long ago.
	static const int MAX_QUEUE = 8;

	protected ref array<string> m_aIds;
	protected ref array<int> m_aPriorities;
	protected int m_iDroppedCount;

	//------------------------------------------------------------------------------------------------
	void OVT_TutorialQueue()
	{
		m_aIds = new array<string>();
		m_aPriorities = new array<int>();
		m_iDroppedCount = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Queues an entry id, keeping the queue sorted by descending priority and FIFO within a priority.
	//!
	//! Two refusals, both silent to the caller and both deliberate:
	//!  - An id ALREADY QUEUED is a no-op. The same trigger firing twice before the first popup has
	//!    been shown must not queue the same tip twice.
	//!  - An enqueue past MAX_QUEUE is DROPPED AND COUNTED. It never displaces an item already in
	//!    the queue: an entry that has been waiting has already earned its place, and evicting it
	//!    for a newcomer would make the visible behaviour depend on arrival timing.
	//!
	//! \param[in] id The entry id to queue. An empty id is refused.
	//! \param[in] priority The entry's priority; higher dequeues first.
	//! \return True when the id was queued, false when it was refused.
	bool Enqueue(string id, int priority)
	{
		if (id == "")
			return false;

		if (Contains(id))
			return false;

		if (m_aIds.Count() >= MAX_QUEUE)
		{
			m_iDroppedCount++;
			return false;
		}

		int position = m_aIds.Count();

		for (int i = 0; i < m_aPriorities.Count(); i++)
		{
			if (m_aPriorities.Get(i) < priority)
			{
				position = i;
				break;
			}
		}

		// InsertAt's index must be BELOW Count(); appending is Insert()'s job.
		if (position >= m_aIds.Count())
		{
			m_aIds.Insert(id);
			m_aPriorities.Insert(priority);
		}
		else
		{
			m_aIds.InsertAt(id, position);
			m_aPriorities.InsertAt(priority, position);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes and returns the highest-priority queued id.
	//! \param[out] id Receives the id. On an EMPTY queue this is left completely untouched, so a
	//! caller that ignores the return value cannot mistake a stale variable for a fresh result.
	//! \return True when an id was dequeued.
	bool TryDequeue(out string id)
	{
		if (m_aIds.IsEmpty())
			return false;

		id = m_aIds.Get(0);

		// RemoveOrdered, NEVER Remove: array.Remove() is the fast swap-with-last removal, which
		// would drop the lowest-priority entry into the head slot and destroy the whole ordering.
		m_aIds.RemoveOrdered(0);
		m_aPriorities.RemoveOrdered(0);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many entries are waiting.
	int Count()
	{
		return m_aIds.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The entry id to look for. Exact, case-sensitive.
	//! \return True when the id is already queued.
	bool Contains(string id)
	{
		return m_aIds.Contains(id);
	}

	//------------------------------------------------------------------------------------------------
	//! Empties the queue. The drop count is NOT reset: it is a session diagnostic, not queue state.
	void Clear()
	{
		m_aIds.Clear();
		m_aPriorities.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many enqueues have been dropped for exceeding MAX_QUEUE this session.
	int GetDroppedCount()
	{
		return m_iDroppedCount;
	}
}
