[EntityEditorProps(category: "Overthrow", description: "Tracks the owner of this entity", color: "0 0 255 255")]
class OVT_PlayerOwnerComponentClass: ScriptComponentClass
{
};

class OVT_PlayerOwnerComponent : ScriptComponent
{
	[RplProp()]
	protected string m_sOwnerUID;
	[RplProp()]
	protected bool m_bIsLocked = false;
	
	private ref ScriptInvoker m_OnOwnerChange;
	private ref ScriptInvoker m_OnLockChange;
	
	ScriptInvoker GetOnOwnerChange()
	{
		if (!m_OnOwnerChange)
			m_OnOwnerChange = new ScriptInvoker();
		return m_OnOwnerChange;
	}
	
	ScriptInvoker GetOnLockChange()
	{
		if (!m_OnLockChange)
			m_OnLockChange = new ScriptInvoker();
		return m_OnLockChange;
	}
	
	OVT_PlayerData GetPlayerOwner()
	{
		return OVT_PlayerData.Get(m_sOwnerUID);
	}
	
	string GetPlayerOwnerUid()
	{
		return m_sOwnerUID;
	}
	
	void SetLocked(bool val)
	{
		m_bIsLocked = val;
		Replication.BumpMe();
		if(m_OnLockChange)
			m_OnLockChange.Invoke(this);
	}
	
	bool IsLocked()
	{
		return m_bIsLocked;
	}
	
	void SetPlayerOwner(string playerUid)
	{
		m_sOwnerUID = playerUid;
		Replication.BumpMe();
		if(m_OnOwnerChange)
			m_OnOwnerChange.Invoke(this);
	}
	
	void ClearPlayerOwner()
	{
		m_sOwnerUID = "";
	}
};