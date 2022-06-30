[BaseContainerProps()]
class OVT_EconomyMoneyStruct : SCR_JsonApiStruct
{
	string m_sPlayerId;
	int m_iMoney;
		
	void OVT_EconomyMoneyStruct()
	{
		RegV("m_sPlayerId");
		RegV("m_iMoney");
	}
}