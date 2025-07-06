[BaseContainerProps(configRoot : true)]
class OVT_PricesConfig
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_PriceConfig> m_aPrices;		
}

class OVT_PriceConfig : ScriptAndConfig
{
	[Attribute("2", desc: "Type of item", uiwidget: UIWidgets.SearchComboBox, enums: ParamEnumArray.FromEnum(SCR_EArsenalItemType))]
	SCR_EArsenalItemType m_eItemType;
	
	[Attribute("2", desc: "Item mode", uiwidget: UIWidgets.SearchComboBox, enums: ParamEnumArray.FromEnum(SCR_EArsenalItemMode))]
	SCR_EArsenalItemMode m_eItemMode;
	
	[Attribute(desc: "String to search in prefab name, blank for all")]
	string m_sFind;
	
	[Attribute("50", desc: "The cost of the items found, will override any above this one")]
	int cost;
	
	[Attribute("5", desc: "Demand Multiplier")]
	int demand;
	
	[Attribute("0", desc: "If true, will not be available for sale anywhere")]
	bool hidden;
}