class OVT_BlackMarketStabilityModifier : OVT_StabilityModifier
{
	
	override void OnPostInit()
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (economy)
		{
			economy.m_OnPlayerTransaction.Insert(OnPlayerTransaction);
		}
	}
	
	override void OnDestroy()
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (economy)
		{
			economy.m_OnPlayerTransaction.Remove(OnPlayerTransaction);
		}
	}
	
	protected void OnPlayerTransaction(int playerId, OVT_ShopComponent shop, bool isBuying, int amount)
	{
		if (!shop)
			return;
			
		// Only trigger for gun dealer transactions
		if (shop.m_ShopType != OVT_ShopType.SHOP_GUNDEALER)
			return;
			
		// Only significant transactions (over $1000)
		if (amount < 1000)
			return;
			
		// Add modifier to the town where the shop is located
		OVT_TownData town = shop.GetTown();
		if (town)
		{
			int townID = m_Towns.GetTownID(town);
			m_Towns.TryAddStabilityModifier(townID, m_iIndex);
		}
	}
}