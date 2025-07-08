[BaseContainerProps(configRoot : true)]
class OVT_PricesConfig
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_PriceConfig> m_aPrices;		
}

[BaseContainerProps(), OVT_PriceConfigEntry()]
class OVT_PriceConfig
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

//~ Custom title to show config
class OVT_PriceConfigEntry : BaseContainerCustomTitle
{
	//------------------------------------------------------------------------------------------------
	override bool _WB_GetCustomTitle(BaseContainer source, out string title)	
	{	
		bool hidden;
		if (!source.Get("hidden", hidden))
			return false;
		if(hidden)
		{
			title = "Hide   ~   ";
		}

		SCR_EArsenalItemType type;
		if (!source.Get("m_eItemType", type))
			return false;
		
		title = title + typename.EnumToString(SCR_EArsenalItemType, type) + " / ";

		SCR_EArsenalItemMode mode;
		if (!source.Get("m_eItemMode", mode))
			return false;
		
		title = title + typename.EnumToString(SCR_EArsenalItemMode, mode);
		
		string find;
		if (!source.Get("m_sFind", find))
			return false;
		
		if(find != "")
		{
			title = title + " '" + find + "'";
		}else{
			title = title + " Default";
		}

		if(hidden){
			return true;
		}

		int cost;
		if (!source.Get("cost", cost))
			return false;
		
		title = title + " = " + cost.ToString();

		return true; 
	}
}