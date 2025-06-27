//! Additional metadata for loadouts
class OVT_LoadoutMetadata
{
	string m_sCategory;			//! Loadout category (e.g., "Assault", "Sniper", "Support")
	string m_sVersion;			//! Version string for compatibility
	int m_iUsageCount;			//! Number of times loadout has been applied
	bool m_bIsFavorite;			//! User marked as favorite
	ref array<string> m_aTags;	//! User-defined tags
	
	//! Constructor
	void OVT_LoadoutMetadata()
	{
		m_sCategory = "";
		m_sVersion = "1.0";
		m_iUsageCount = 0;
		m_bIsFavorite = false;
		m_aTags = new array<string>();
	}
	
	//! Add tag to loadout
	void AddTag(string tag)
	{
		if (m_aTags.Find(tag) == -1)
			m_aTags.Insert(tag);
	}
	
	//! Remove tag from loadout
	void RemoveTag(string tag)
	{
		int index = m_aTags.Find(tag);
		if (index != -1)
			m_aTags.Remove(index);
	}
	
	//! Check if loadout has tag
	bool HasTag(string tag)
	{
		return m_aTags.Find(tag) != -1;
	}
	
	//! Increment usage counter
	void IncrementUsage()
	{
		m_iUsageCount++;
	}
	
	//! Get tags array
	array<string> GetTags()
	{
		return m_aTags;
	}
}