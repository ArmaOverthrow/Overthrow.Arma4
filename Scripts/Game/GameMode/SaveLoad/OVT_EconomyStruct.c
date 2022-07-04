[BaseContainerProps()]
class OVT_EconomyStruct : OVT_BaseSaveStruct
{
	protected int funds;
	protected float tax;
	protected ref array<ref OVT_EconomyMoneyStruct> players = {};
	
	override bool Serialize()
	{
		players.Clear();
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		for(int i; i<economy.m_mMoney.Count(); i++)
		{	
			OVT_EconomyMoneyStruct d = new OVT_EconomyMoneyStruct();
			d.m_sPlayerId = economy.m_mMoney.GetKey(i);
			d.m_iMoney = economy.m_mMoney.GetElement(i);
			
			players.Insert(d);
		}		
		
		funds = economy.m_iResistanceMoney;
		tax = economy.m_fResistanceTax;
		
		return true;
	}
	
	override bool Deserialize()
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		foreach(OVT_EconomyMoneyStruct d : players)
		{
			economy.m_mMoney[d.m_sPlayerId] = d.m_iMoney;
		}
		
		economy.m_iResistanceMoney = funds;
		economy.m_fResistanceTax = tax;
		
		return true;
	}
	
	void OVT_EconomyStruct()
	{
		RegV("players");
		RegV("funds");
		RegV("tax");
	}
}