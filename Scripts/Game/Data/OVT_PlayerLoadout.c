//! Individual player loadout data class
class OVT_PlayerLoadout : EPF_PersistentScriptedState
{
	string m_sLoadoutName;					//! User-defined name
	string m_sPlayerId;						//! Owner's player ID  
	string m_sDescription;					//! Optional description
	int m_iCreatedTimestamp;				//! Creation time
	int m_iLastUsedTimestamp;				//! Last applied time
	bool m_bIsTemplate;						//! Can be used by others
	bool m_bIsOfficerTemplate;				//! Officer-created template (shown first)
	ref array<ref OVT_LoadoutItem> m_aItems;//! Equipment items
	ref array<string> m_aQuickSlotItems;	//! Quick slot item prefabs (by slot index)
	ref OVT_LoadoutMetadata m_Metadata;		//! Additional metadata
	
	//! Constructor
	void OVT_PlayerLoadout()
	{
		// Initialize with defaults (for both EPF loading and manual creation)
		m_sLoadoutName = "";
		m_sPlayerId = "";
		m_sDescription = "";
		m_iCreatedTimestamp = 0;
		m_iLastUsedTimestamp = 0;
		m_bIsTemplate = false;
		m_bIsOfficerTemplate = false;
		m_aItems = new array<ref OVT_LoadoutItem>();
		m_aQuickSlotItems = new array<string>();
		m_Metadata = new OVT_LoadoutMetadata();
	}
	
	//! Initialize with parameters (call after constructor)
	void Initialize(string loadoutName, string playerId, string description = "")
	{
		m_sLoadoutName = loadoutName;
		m_sPlayerId = playerId;
		m_sDescription = description;
		m_iCreatedTimestamp = System.GetUnixTime();
		m_iLastUsedTimestamp = m_iCreatedTimestamp;
	}
	
	//! Initialize loadout with basic data (deprecated - use constructor with parameters instead)
	void InitLoadout(string loadoutName, string playerId, string description = "")
	{
		m_sLoadoutName = loadoutName;
		m_sPlayerId = playerId;
		m_sDescription = description;
		m_iCreatedTimestamp = System.GetUnixTime();
		m_iLastUsedTimestamp = m_iCreatedTimestamp;
	}
	
	//! Add item to loadout
	void AddItem(OVT_LoadoutItem item)
	{
		if (item)
			m_aItems.Insert(item);
	}
	
	//! Remove item from loadout
	void RemoveItem(int index)
	{
		if (index >= 0 && index < m_aItems.Count())
			m_aItems.Remove(index);
	}
	
	//! Clear all items
	void ClearItems()
	{
		m_aItems.Clear();
	}
	
	//! Get item by index
	OVT_LoadoutItem GetItem(int index)
	{
		if (index >= 0 && index < m_aItems.Count())
			return m_aItems[index];
		return null;
	}
	
	//! Get items count
	int GetItemCount()
	{
		return m_aItems.Count();
	}
	
	//! Get all items
	array<ref OVT_LoadoutItem> GetItems()
	{
		return m_aItems;
	}
	
	//! Update last used timestamp
	void UpdateLastUsed()
	{
		m_iLastUsedTimestamp = System.GetUnixTime();
		if (m_Metadata)
			m_Metadata.IncrementUsage();
	}
	
	//! Check if loadout belongs to player
	bool BelongsToPlayer(string playerId)
	{
		return m_sPlayerId == playerId;
	}
	
	//! Check if loadout can be used by other players
	bool IsAvailableToOthers()
	{
		return m_bIsTemplate;
	}
	
	//! Set as template (usable by others)
	void SetAsTemplate(bool isTemplate)
	{
		m_bIsTemplate = isTemplate;
	}
	
	//! Set as officer template (shown first, officer-only)
	void SetAsOfficerTemplate(bool isOfficerTemplate)
	{
		m_bIsOfficerTemplate = isOfficerTemplate;
		if (isOfficerTemplate)
			m_bIsTemplate = true; // Officer templates are also regular templates
	}
	
	//! Check if this is an officer template
	bool IsOfficerTemplate()
	{
		return m_bIsOfficerTemplate;
	}
	
	//! Get unique identifier for this loadout
	string GetLoadoutKey()
	{
		return string.Format("%1_%2", m_sPlayerId, m_sLoadoutName);
	}
	
	//! Set quick slot items
	void SetQuickSlotItems(array<string> quickSlotItems)
	{
		m_aQuickSlotItems.Clear();
		if (quickSlotItems)
		{
			foreach (string item : quickSlotItems)
			{
				m_aQuickSlotItems.Insert(item);
			}
		}
	}
	
	//! Get quick slot items
	array<string> GetQuickSlotItems()
	{
		return m_aQuickSlotItems;
	}
	
	
	//! Generate deterministic ID for this loadout
	string GetDeterministicId()
	{
		// Create deterministic ID: loadout_playerid_loadoutname (sanitized)
		string sanitizedName = SanitizeForId(m_sLoadoutName);
		string sanitizedPlayerId = SanitizeForId(m_sPlayerId);
		return string.Format("loadout_%1_%2", sanitizedPlayerId, sanitizedName);
	}
	
	//! Sanitize string for use in IDs
	protected string SanitizeForId(string input)
	{
		string result = "";
		for (int i = 0; i < input.Length(); i++)
		{
			string char = input.Get(i);
			// Only allow alphanumeric and underscore
			if ((char >= "a" && char <= "z") || 
			    (char >= "A" && char <= "Z") || 
			    (char >= "0" && char <= "9") || 
			    char == "_")
			{
				result += char;
			}
			else
			{
				result += "_"; // Replace special chars with underscore
			}
		}
		return result;
	}
}