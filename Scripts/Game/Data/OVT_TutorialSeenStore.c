//------------------------------------------------------------------------------------------------
//! The in-memory half of the "already seen this tip" record.
//!
//! This class holds the SET LOGIC and nothing else. It makes no engine call and knows nothing about
//! where the durable copy lives: ids are injected as a plain array and read back as one. That split
//! is deliberate - the store's rules (exact match, idempotent marking, version invalidation, the
//! cap) are the part that can silently corrupt a player's state, so they live where a Logic-tier
//! case can pin them.
//!
//! WHERE THE DURABLE COPY LIVES (changed 2026-08-18): the player's campaign record,
//! OVT_PlayerData.m_aSeenTutorials, persisted by OVT_PlayerManagerSerializer version 4 and owned by
//! the server. This store is the owning client's session mirror, filled by the server's state push
//! and reported back to on every dismissal (OVT_TutorialComponent). Per CAMPAIGN and per PLAYER,
//! not per machine: a new playthrough shows the tips again, by decision. The per-machine
//! ModuleGameSettings profile store this used to feed proved unreliable and was deleted outright.
//------------------------------------------------------------------------------------------------
class OVT_TutorialSeenStore
{
	//! Schema version of the id list this class understands. Bump it when the MEANING of a stored id
	//! changes; a stored list at any other version is discarded rather than half-trusted.
	static const int CURRENT_VERSION = 1;

	//! Hard cap on remembered ids. Well above any plausible entry count (the feature ships two), so
	//! reaching it means something is generating ids rather than authoring them, and the right
	//! response is to stop growing rather than to evict - evicting would re-show a tip the player
	//! has already dismissed, which is the one thing this store exists to prevent.
	static const int MAX_SEEN = 512;

	protected ref set<string> m_aSeen;
	protected int m_iVersion;
	protected bool m_bCapWarned;

	//------------------------------------------------------------------------------------------------
	void OVT_TutorialSeenStore()
	{
		m_aSeen = new set<string>();
		m_iVersion = CURRENT_VERSION;
		m_bCapWarned = false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The entry id. Exact and case-sensitive: ids are authored, stable and lowercase,
	//! so a near-miss is a bug to be seen and not a lookup to be smoothed over.
	//! \return True when this entry has already been shown on this profile.
	bool HasSeen(string id)
	{
		if (id == "")
			return false;

		return m_aSeen.Contains(id);
	}

	//------------------------------------------------------------------------------------------------
	//! Records an entry as shown. Idempotent - marking an id already present changes nothing and is
	//! not an error, because the dismiss path can legitimately run twice (timer and explicit).
	//! \param[in] id The entry id. An empty id is refused.
	//! \return True when the id is in the set afterwards; false when it was refused by the cap.
	bool MarkSeen(string id)
	{
		if (id == "")
			return false;

		if (m_aSeen.Contains(id))
			return true;

		if (m_aSeen.Count() >= MAX_SEEN)
		{
			WarnCapOnce();
			return false;
		}

		m_aSeen.Insert(id);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the whole set from a stored id list.
	//!
	//! A VERSION MISMATCH CLEARS THE SET and adopts the current version, rather than importing a
	//! list whose ids may no longer mean what they meant. That costs a player one repeat of each tip
	//! after a schema change, which is strictly better than the alternative failure - a store that
	//! half-loads and can never be repaired without deleting it by hand.
	//!
	//! \param[in] ids The stored ids. Null is treated as an empty list, because the settings loader
	//! can legitimately hand back a null array for an absent member.
	//! \param[in] version The version the ids were stored at.
	void LoadFrom(array<string> ids, int version)
	{
		m_aSeen.Clear();
		m_iVersion = CURRENT_VERSION;
		m_bCapWarned = false;

		if (version != CURRENT_VERSION)
			return;

		if (!ids)
			return;

		foreach (string id : ids)
		{
			MarkSeen(id);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Copies the set out for storage: every remembered id exactly once, nothing lost, nothing
	//! duplicated.
	//!
	//! The order is the SET'S canonical order, not the order ids were marked in. `set<T>` is a
	//! sorted container (its Find is documented O(log n)), so a round trip is order-normalising by
	//! construction. That is fine and is relied on: nothing reads this list positionally - it is a
	//! membership record - and a canonical order makes the stored profile block diff-stable.
	//!
	//! \param[out] ids Receives the ids. Allocated when null and cleared otherwise, so a caller
	//! reusing a buffer never writes a stale id back to the profile.
	void WriteTo(out array<string> ids)
	{
		if (!ids)
			ids = new array<string>();
		else
			ids.Clear();

		for (int i = 0; i < m_aSeen.Count(); i++)
		{
			ids.Insert(m_aSeen.Get(i));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many ids are remembered.
	int Count()
	{
		return m_aSeen.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The schema version this set is held at.
	int GetVersion()
	{
		return m_iVersion;
	}

	//------------------------------------------------------------------------------------------------
	//! Empties the set. Used by the version-mismatch path and by manual resets during testing.
	void Clear()
	{
		m_aSeen.Clear();
		m_bCapWarned = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Logs the cap warning exactly once per store. Once, because the caller that filled the store
	//! will keep calling: a per-refusal warning would be the log spam, not the diagnosis.
	protected void WarnCapOnce()
	{
		if (m_bCapWarned)
			return;

		m_bCapWarned = true;

		Print("[Overthrow] OVT_TutorialSeenStore is full at " + MAX_SEEN.ToString() + " ids; further ids are refused and existing ones are kept", LogLevel.WARNING);
	}
}
