//! Shop type configuration for map display
//! Associates shop types with their display names and icons
[BaseContainerProps(), OVT_ShopTypeInfoTitle()]
class OVT_ShopTypeInfo
{
	[Attribute(defvalue: OVT_ShopType.SHOP_GENERAL.ToString(), UIWidgets.ComboBox, "Shop Type", "", ParamEnumArray.FromEnum(OVT_ShopType))]
	OVT_ShopType m_ShopType;
	
	[Attribute(defvalue: "", UIWidgets.EditBox, "Display Name")]
	string m_sDisplayName;
	
	[Attribute(defvalue: "", UIWidgets.EditBox, "Icon Name")]
	string m_sIconName;
}

//! Custom title class for OVT_ShopTypeInfo
class OVT_ShopTypeInfoTitle : BaseContainerCustomTitle
{
	override bool _WB_GetCustomTitle(BaseContainer source, out string title)
	{
		int shopTypeInt;
		if (!source.Get("m_ShopType", shopTypeInt))
			return false;
		
		OVT_ShopType shopType = shopTypeInt;
		title = typename.EnumToString(OVT_ShopType, shopType);
		
		return true;
	}
}