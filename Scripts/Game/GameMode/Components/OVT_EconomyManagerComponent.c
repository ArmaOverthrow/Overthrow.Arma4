class OVT_EconomyManagerComponentClass: OVT_ComponentClass
{
};

class OVT_EconomyManagerComponent: OVT_Component
{
	protected ref map<int, int> m_mMoney;
	protected int m_iResistanceMoney = 0;
	
	static OVT_EconomyManagerComponent s_Instance;
	
	static OVT_EconomyManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_EconomyManagerComponent.Cast(pGameMode.FindComponent(OVT_EconomyManagerComponent));
		}

		return s_Instance;
	}
	
	int GetPlayerMoney(int playerId)
	{
		if(!m_mMoney.Contains(playerId)) return 0;
		return m_mMoney[playerId];
	}
	
	bool PlayerHasMoney(int playerId, int amount)
	{
		if(!m_mMoney.Contains(playerId)) return false;
		return m_mMoney[playerId] >= amount;
	}
	
	void AddPlayerMoney(int playerId, int amount)
	{
		if(!m_mMoney.Contains(playerId)) m_mMoney[playerId] = 0;
		m_mMoney[playerId] = m_mMoney[playerId] + amount;
	}
	
	void TakePlayerMoney(int playerId, int amount)
	{
		if(!m_mMoney.Contains(playerId)) return;
		m_mMoney[playerId] = m_mMoney[playerId] - amount;
		if(m_mMoney[playerId] < 0) m_mMoney[playerId] = 0;
	}
	
	void AddResistanceMoney(int amount)
	{
		m_iResistanceMoney += amount;
	}
	
	void TakeResistanceMoney(int amount)
	{
		m_iResistanceMoney -= amount;
		if(m_iResistanceMoney < 0) m_iResistanceMoney = 0;
	}
	
	bool ResistanceHasMoney(int amount)
	{
		return m_iResistanceMoney >= amount;
	}
	
	int GetResistanceMoney()
	{
		return m_iResistanceMoney;
	}
	
	void Init(IEntity owner)
	{		
		m_mMoney = new map<int, int>;
	}
}