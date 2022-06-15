class OVT_KeyButtonComponent : SCR_ButtonComponent
{
	protected string m_sData;
	
	void SetData(string data)
	{
		m_sData = data;
	}
	
	string GetData()
	{
		return m_sData;
	}
}