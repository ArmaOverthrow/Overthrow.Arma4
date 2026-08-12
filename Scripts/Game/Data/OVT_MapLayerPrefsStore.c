//------------------------------------------------------------------------------------------------
//! The in-memory half of the per-profile "which map layers has this player hidden" record.
//!
//! This class holds the SET LOGIC and nothing else. It makes no engine call, never touches
//! GetGameUserSettings, and knows nothing about BaseContainer: the settings module is injected as a
//! plain array of keys and read back as one. That split is deliberate and is the entire reason the
//! preference half of this feature is split three ways - the store's rules (namespaced keys, exact
//! match, idempotent hide/show, version invalidation, the cap) are the part that can silently
//! corrupt a player's map, so they live where a Logic-tier case can pin them, and the unverifiable
//! BaseContainer plumbing is an accessor of its own. It mirrors OVT_TutorialSeenStore exactly.
//!
//! IT STORES HIDDEN KEYS, AND ABSENT MEANS VISIBLE. That is the whole representation, and it is the
//! minimal one that also gets the defaults right: a location type or a canvas layer added later is
//! absent from every existing record and therefore defaults visible, with no migration and no schema
//! bump. A key->bool map could hold "visible" for a key that is also in a hidden list; a set cannot
//! disagree with itself.
//!
//! The record is per MACHINE and per PROFILE, not per campaign and not per player record - a filter
//! is a client-side PRESENTATION PREFERENCE, and it follows the player across saves and servers.
//! It is emphatically NOT campaign visibility: what a campaign chooses to reveal is a different
//! concept that must never share a field with this one.
//------------------------------------------------------------------------------------------------
class OVT_MapLayerPrefsStore
{
	//! Schema version of the key list this class understands. Bump it when the MEANING of a stored
	//! key changes; a stored list at any other version is discarded rather than half-trusted.
	static const int CURRENT_VERSION = 1;

	//! Hard cap on remembered hidden keys. Well above any plausible row count (the panel ships
	//! seventeen rows), so reaching it means something is generating keys rather than enumerating
	//! them, and the right response is to stop growing rather than to evict - evicting would silently
	//! re-show a layer the player deliberately turned off, which is the one thing this store exists
	//! to prevent.
	static const int MAX_HIDDEN = 256;

	//! Namespace prefix for a location type, keyed by class name.
	static const string TYPE_PREFIX = "type:";

	//! Namespace prefix for a canvas layer (and for the hand-built player-marker row), keyed by
	//! layer id.
	static const string LAYER_PREFIX = "layer:";

	protected ref set<string> m_aHidden;
	protected int m_iVersion;
	protected bool m_bCapWarned;

	//------------------------------------------------------------------------------------------------
	void OVT_MapLayerPrefsStore()
	{
		m_aHidden = new set<string>();
		m_iVersion = CURRENT_VERSION;
		m_bCapWarned = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds the preference key for a location type.
	//!
	//! NAMESPACE-PREFIXED so a layer id can never collide with a class name. The two id spaces are
	//! authored independently - one is a script class name, the other a hand-written config string -
	//! and nothing stops both spelling the same word. A prefix costs one string concat and removes
	//! the whole category of question.
	//!
	//! \param[in] className The location type's class name, as ClassName() reports it.
	//! \return The namespaced key, or "" when the class name is empty.
	static string TypeKey(string className)
	{
		if (className == "")
			return "";

		return TYPE_PREFIX + className;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds the preference key for a canvas layer.
	//! \param[in] layerId The layer's authored id.
	//! \return The namespaced key, or "" when the id is empty.
	static string LayerKey(string layerId)
	{
		if (layerId == "")
			return "";

		return LAYER_PREFIX + layerId;
	}

	//------------------------------------------------------------------------------------------------
	//! The default that makes "everything on" work: a key nobody has hidden is VISIBLE.
	//!
	//! \param[in] key A namespaced key from TypeKey or LayerKey. Exact and case-sensitive: keys are
	//! derived from class names and authored ids where case carries meaning, so a near miss is a bug
	//! to be seen and not a lookup to be smoothed over.
	//! \return True when the thing this key names should be drawn.
	bool IsVisible(string key)
	{
		if (key == "")
			return true;

		return !m_aHidden.Contains(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Hides or shows one key. Idempotent in BOTH directions - hiding something already hidden and
	//! showing something never hidden are both no-ops and neither is an error, because the panel can
	//! legitimately re-apply the same state (a rebuilt row reports its live value back).
	//!
	//! \param[in] key A namespaced key. An empty key is refused rather than remembered as a blank.
	//! \param[in] hidden True to hide, false to show.
	void SetHidden(string key, bool hidden)
	{
		if (key == "")
			return;

		if (!hidden)
		{
			int index = m_aHidden.Find(key);
			if (index != -1)
				m_aHidden.Remove(index);

			return;
		}

		if (m_aHidden.Contains(key))
			return;

		if (m_aHidden.Count() >= MAX_HIDDEN)
		{
			WarnCapOnce();
			return;
		}

		m_aHidden.Insert(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the whole set from a stored key list.
	//!
	//! A VERSION MISMATCH CLEARS THE SET and adopts the current version, rather than importing keys
	//! that may no longer mean what they meant. That costs a player one re-toggle of each filter
	//! after a schema change, which is strictly better than the alternative failure - a store that
	//! half-loads and can never be repaired without editing a profile by hand. The direction of that
	//! trade is safe here precisely because the default is "visible": a cleared store shows the map
	//! the player saw before this feature existed.
	//!
	//! \param[in] keys The stored keys. Null is treated as an empty list, because the settings loader
	//! can legitimately hand back a null array for an absent member.
	//! \param[in] version The version the keys were stored at.
	void LoadFrom(array<string> keys, int version)
	{
		m_aHidden.Clear();
		m_iVersion = CURRENT_VERSION;
		m_bCapWarned = false;

		if (version != CURRENT_VERSION)
			return;

		if (!keys)
			return;

		foreach (string key : keys)
		{
			SetHidden(key, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Copies the set out for storage: every hidden key exactly once, nothing lost, nothing
	//! duplicated.
	//!
	//! The order is the SET'S canonical order, not the order keys were hidden in. `set<T>` is a
	//! sorted container, so a round trip is order-normalising by construction. That is fine and is
	//! relied on: nothing reads this list positionally - it is a membership record - and a canonical
	//! order makes the stored profile block diff-stable.
	//!
	//! \param[out] keys Receives the keys. Allocated when null and CLEARED otherwise, so a caller
	//! reusing a buffer never writes a stale key back to the profile.
	void WriteTo(out array<string> keys)
	{
		if (!keys)
			keys = new array<string>();
		else
			keys.Clear();

		for (int i = 0; i < m_aHidden.Count(); i++)
		{
			keys.Insert(m_aHidden.Get(i));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many keys are hidden.
	int Count()
	{
		return m_aHidden.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The schema version this set is held at.
	int GetVersion()
	{
		return m_iVersion;
	}

	//------------------------------------------------------------------------------------------------
	//! Empties the set - every layer becomes visible again. Used by the version-mismatch path and by
	//! a manual reset.
	void Clear()
	{
		m_aHidden.Clear();
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

		Print("[Overthrow] OVT_MapLayerPrefsStore is full at " + MAX_HIDDEN.ToString() + " hidden keys; further keys are refused and existing ones are kept", LogLevel.WARNING);
	}
}
