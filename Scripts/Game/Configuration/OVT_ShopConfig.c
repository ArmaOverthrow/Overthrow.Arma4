[BaseContainerProps(configRoot : true)]
class OVT_ShopConfig : ScriptAndConfig
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_ShopInventoryConfig> m_aShopConfigs;
}

class OVT_ShopInventoryConfig : ScriptAndConfig
{
	[Attribute("1", UIWidgets.ComboBox, "Shop type", "", ParamEnumArray.FromEnum(OVT_ShopType) )]
	OVT_ShopType type;
	
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_ShopInventoryItem> m_aInventoryItems;
}