//! Shop type configuration for map display
//! Associates shop types with their display names and icons
[BaseContainerProps()]
class OVT_ShopTypeInfo
{
	[Attribute(defvalue: OVT_ShopType.SHOP_GENERAL.ToString(), UIWidgets.ComboBox, "Shop Type", "", ParamEnumArray.FromEnum(OVT_ShopType))]
	OVT_ShopType m_ShopType;
	
	[Attribute(defvalue: "", UIWidgets.EditBox, "Display Name")]
	string m_sDisplayName;
	
	[Attribute(defvalue: "", UIWidgets.EditBox, "Icon Name")]
	string m_sIconName;
}