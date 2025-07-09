//! Map location type for shops
//! Handles display of different shop types on the map
[BaseContainerProps()]
class OVT_MapLocationShop : OVT_MapLocationType
{
	[Attribute("", UIWidgets.Object, "Shop Type Configurations")]
	protected ref array<ref OVT_ShopTypeInfo> m_aShopTypes;
	
	[Attribute(defvalue: "shop", UIWidgets.EditBox, "Default icon name for unknown shop types")]
	protected string m_sDefaultIconName;
	
	[Attribute(defvalue: "Shop", UIWidgets.EditBox, "Default display name for unknown shop types")]
	protected string m_sDefaultDisplayName;

	//! Populate shop locations
	override void PopulateLocations(array<ref OVT_MapLocationData> locations)
	{
		if (!locations)
			return;
		
		// Get all shop components from the economy manager
		OVT_EconomyManagerComponent economyManager = OVT_Global.GetEconomy();
		if (!economyManager)
			return;
		
		// Get all shops as RplIds
		array<RplId> shopIds = economyManager.GetAllShops();
		
		foreach (RplId shopId : shopIds)
		{
			// Get the entity from the RplId
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(shopId));
			if (!rpl)
				continue;
			
			IEntity entity = rpl.GetEntity();
			if (!entity || !entity.GetWorld())
				continue;
			
			// Get the shop component
			OVT_ShopComponent shop = OVT_ShopComponent.Cast(entity.FindComponent(OVT_ShopComponent));
			if (!shop)
				continue;
			
			// Create location data for this shop
			OVT_MapLocationData locationData = new OVT_MapLocationData(entity.GetOrigin(), entity.GetName(), ClassName());
			locationData.m_ShopType = shop.m_ShopType;
			locationData.m_pEntity = entity;
			locationData.m_RplID = shopId;
			
			locations.Insert(locationData);
		}
	}
	
	//! Get location name based on shop type
	override string GetLocationName(OVT_MapLocationData location)
	{
		if (!location)
			return m_sDefaultDisplayName;
		
		// Find the shop type configuration
		OVT_ShopTypeInfo shopTypeInfo = GetShopTypeInfo(location.m_ShopType);
		if (shopTypeInfo && !shopTypeInfo.m_sDisplayName.IsEmpty())
			return shopTypeInfo.m_sDisplayName;
		
		return m_sDefaultDisplayName;
	}
	
	//! Setup icon widget based on shop type
	override void SetupIconWidget(Widget iconWidget, OVT_MapLocationData location, bool isSmall = false)
	{
		if (!iconWidget || !location)
			return;
		
		ImageWidget imageWidget = ImageWidget.Cast(iconWidget.FindAnyWidget("Icon"));
		if (!imageWidget)
			return;
		
		// Find the shop type configuration
		OVT_ShopTypeInfo shopTypeInfo = GetShopTypeInfo(location.m_ShopType);
		string iconName = m_sDefaultIconName;
		
		if (shopTypeInfo && !shopTypeInfo.m_sIconName.IsEmpty())
			iconName = shopTypeInfo.m_sIconName;
		
		// Set the icon
		imageWidget.LoadImageFromSet(0, m_IconImageset, iconName);
		imageWidget.SetImage(0);
		
		// Set visibility
		imageWidget.SetVisible(true);
	}
	
	//! Get shop type info for a given shop type
	protected OVT_ShopTypeInfo GetShopTypeInfo(OVT_ShopType shopType)
	{
		if (!m_aShopTypes)
			return null;
		
		foreach (OVT_ShopTypeInfo shopTypeInfo : m_aShopTypes)
		{
			if (shopTypeInfo && shopTypeInfo.m_ShopType == shopType)
				return shopTypeInfo;
		}
		
		return null;
	}
}