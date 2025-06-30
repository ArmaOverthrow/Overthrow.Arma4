//! Individual equipment item in a loadout
class OVT_LoadoutItem
{
	string m_sResourceName;					//! Prefab resource name
	int m_iSlotIndex;						//! Exact slot index within storage component
	int m_iStoragePriority;					//! Storage component priority (to identify storage)
	EStoragePurpose m_eStoragePurpose;		//! Storage component purpose (to identify storage)
	bool m_bIsEquipped;						//! True if item is equipped (in hands, worn, etc)
	
	ref array<string> m_aAttachments;		//! Weapon attachments
	ref map<string, string> m_mProperties;	//! Custom properties
	ref array<ref OVT_LoadoutItem> m_aChildItems; //! Items contained within this item (for containers like backpacks)
	
	//! Constructor (required for EPF serialization)
	void OVT_LoadoutItem()
	{
		m_sResourceName = "";
		m_iSlotIndex = -1;
		m_iStoragePriority = -1;
		m_eStoragePurpose = EStoragePurpose.PURPOSE_ANY;
		m_bIsEquipped = false;
		
		m_aAttachments = new array<string>();
		m_mProperties = new map<string, string>();
		m_aChildItems = new array<ref OVT_LoadoutItem>();
	}
	
	//! Add attachment to item
	void AddAttachment(string attachmentResourceName)
	{
		if (m_aAttachments.Find(attachmentResourceName) == -1)
			m_aAttachments.Insert(attachmentResourceName);
	}
	
	//! Remove attachment from item
	void RemoveAttachment(string attachmentResourceName)
	{
		int index = m_aAttachments.Find(attachmentResourceName);
		if (index != -1)
			m_aAttachments.Remove(index);
	}
	
	//! Set custom property
	void SetProperty(string key, string value)
	{
		m_mProperties.Set(key, value);
	}
	
	//! Get custom property
	string GetProperty(string key)
	{
		string value;
		m_mProperties.Find(key, value);
		return value;
	}
	
	//! Check if item has attachments
	bool HasAttachments()
	{
		return m_aAttachments && m_aAttachments.Count() > 0;
	}
	
	//! Get attachments array
	array<string> GetAttachments()
	{
		return m_aAttachments;
	}
	
	//! Get properties map
	map<string, string> GetProperties()
	{
		return m_mProperties;
	}
	
	//! Add child item (for containers)
	void AddChildItem(OVT_LoadoutItem childItem)
	{
		if (!m_aChildItems)
			m_aChildItems = new array<ref OVT_LoadoutItem>();
		m_aChildItems.Insert(childItem);
	}
	
	//! Get child items
	array<ref OVT_LoadoutItem> GetChildItems()
	{
		return m_aChildItems;
	}
	
	//! Check if item has child items (is a container with contents)
	bool HasChildItems()
	{
		return m_aChildItems && m_aChildItems.Count() > 0;
	}
	
	//! Clear all child items
	void ClearChildItems()
	{
		if (m_aChildItems)
			m_aChildItems.Clear();
	}
}