//------------------------------------------------------------------------------------------------
//! Selects the tutorial entries an event occurrence should fire, in the order they should be shown.
//!
//! Pure and world-free on purpose: the SAME selection runs on the server (deciding what to send a
//! player) and on the client (deciding what a client-local hook produced), and the only way those
//! two cannot drift apart is for both to call one function that a Logic-tier case can pin.
//!
//! An entry declares SEVERAL triggers as alternatives, not as a conjunction - the entry fires when
//! ANY of them matches, and it is returned exactly ONCE however many of them matched.
//------------------------------------------------------------------------------------------------
class OVT_TutorialMatcher
{
	//------------------------------------------------------------------------------------------------
	//! Finds every enabled entry with at least one trigger matching this event occurrence.
	//!
	//! Results are ordered by DESCENDING priority, and within one priority by DECLARATION order -
	//! the order the entries appear in the array the caller passed. That second half matters as much
	//! as the first: it is what makes two entries at the default priority of 0 show in a
	//! predictable, author-controlled sequence rather than an arbitrary one.
	//!
	//! \param[in] entries The entry list. A null list is not an error; it produces no matches.
	//! \param[in] ctx The event occurrence. A null context produces no matches.
	//! \param[out] ids Receives the matching entry ids. ALWAYS a valid array, never null, and always
	//! cleared first - a caller that reuses one buffer across calls never sees a stale id.
	static void FindMatches(array<ref OVT_TutorialEntryConfig> entries, OVT_TutorialEventContext ctx, out array<string> ids)
	{
		if (!ids)
			ids = new array<string>();
		else
			ids.Clear();

		if (!entries || !ctx)
			return;

		// Priorities are tracked alongside the ids so the stable insert below can compare against
		// what is already in the result without looking the entry up again.
		array<int> priorities = new array<int>();

		foreach (OVT_TutorialEntryConfig entry : entries)
		{
			if (!entry)
				continue;

			if (!entry.m_bEnabled)
				continue;

			if (entry.m_sId == "")
				continue;

			if (!EntryMatches(entry, ctx))
				continue;

			InsertByPriority(entry.m_sId, entry.m_iPriority, ids, priorities);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Whether any of one entry's triggers fires for this event occurrence.
	//! \param[in] entry The entry. Assumed non-null and enabled.
	//! \param[in] ctx The event occurrence.
	//! \return True on the FIRST matching trigger - an entry is selected once, not once per trigger.
	protected static bool EntryMatches(OVT_TutorialEntryConfig entry, OVT_TutorialEventContext ctx)
	{
		if (!entry.m_aTriggers)
			return false;

		foreach (OVT_TutorialTrigger trigger : entry.m_aTriggers)
		{
			if (!trigger)
				continue;

			if (trigger.Matches(ctx))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Stable insert: places the id after every entry of equal or higher priority and before every
	//! entry of lower priority, which yields priority-descending order with declaration order
	//! preserved inside each priority band.
	//! \param[in] id The entry id being placed.
	//! \param[in] priority The entry's priority.
	//! \param[inout] ids The result list.
	//! \param[inout] priorities The parallel priority list, kept in step with ids.
	protected static void InsertByPriority(string id, int priority, array<string> ids, array<int> priorities)
	{
		int position = ids.Count();

		for (int i = 0; i < priorities.Count(); i++)
		{
			if (priorities.Get(i) < priority)
			{
				position = i;
				break;
			}
		}

		// InsertAt's index must be BELOW Count(); appending is Insert()'s job.
		if (position >= ids.Count())
		{
			ids.Insert(id);
			priorities.Insert(priority);
		}
		else
		{
			ids.InsertAt(id, position);
			priorities.InsertAt(priority, position);
		}
	}
}
