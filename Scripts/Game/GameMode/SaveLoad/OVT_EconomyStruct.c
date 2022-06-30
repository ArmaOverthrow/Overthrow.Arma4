[BaseContainerProps()]
class OVT_EconomyStruct : SCR_JsonApiStruct
{
	protected int m_iResistanceMoney;
	protected ref array<ref OVT_EconomyMoneyStruct> m_aMoneyStructs = {};
	
	override bool Serialize()
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		for(int i; i<economy.m_mMoney.Count(); i++)
		{	
			OVT_EconomyMoneyStruct d = new OVT_EconomyMoneyStruct();
			d.m_sPlayerId = economy.m_mMoney.GetKey(i);
			d.m_iMoney = economy.m_mMoney.GetElement(i);
			
			m_aMoneyStructs.Insert(d);
		}		
		
		return true;
	}
	
	override bool Deserialize()
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		foreach(OVT_EconomyMoneyStruct d : m_aMoneyStructs)
		{
			economy.m_mMoney[d.m_sPlayerId] = d.m_iMoney;
		}
		
		return true;
	}
	
	void OVT_EconomyStruct()
	{
		RegV("m_aMoneyStructs");
		RegV("m_iResistanceMoney");
	}
}