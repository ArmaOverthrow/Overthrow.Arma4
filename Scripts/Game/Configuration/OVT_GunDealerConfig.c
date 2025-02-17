[BaseContainerProps(configRoot : true)]
class OVT_GunDealerConfig : ScriptAndConfig
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_ShopInventoryItem> m_aGunDealerItems;
	
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_PrefabItemCostConfig> m_aGunDealerItemPrefabs;
}