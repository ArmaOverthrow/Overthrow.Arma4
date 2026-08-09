//! Map location type for shops
//! Handles display of different shop types on the map
//!
//! INFO PANEL (implementation plan section 4.6): the relative price indicator. A shop-wide remoteness
//! badge, then up to three bargains and three rip-offs from this town's supply, as caret glyphs.
//! NO PRICE, NO CURRENCY, NO PERCENTAGE AND NO STOCK COUNT is rendered - the shop menu remains the
//! only place with real numbers, and that is the whole reason this indicator exists.
[BaseContainerProps(), OVT_MapLocationTypeTitle()]
class OVT_MapLocationShop : OVT_MapLocationShopBase
{
	[Attribute("", UIWidgets.Object, "Shop Type Configurations")]
	protected ref array<ref OVT_ShopTypeInfo> m_aShopTypes;

	[Attribute(defvalue: "shop", UIWidgets.EditBox, "Default icon name for unknown shop types")]
	protected string m_sDefaultIconName;

	[Attribute(defvalue: "Shop", UIWidgets.EditBox, "Default display name for unknown shop types")]
	protected string m_sDefaultDisplayName;

	//------------------------------------------------------------------------------------------------
	//! Builds the price indicator. Runs ONCE, when the panel opens (Q-7) - never from CanFastTravel
	//! or ShouldShowLocation, both of which run per element on every zoom change.
	//!
	//! THREE OUTCOMES, AND NONE OF THEM IS AN EMPTY SECTION:
	//!  - a vehicle shop gets one explanatory line and nothing else. GetSellPriceAtOffset returns the
	//!    flat base price for anything in m_aAllVehicles (:556), BEFORE both adjustment terms, so
	//!    carets would be uniformly neutral and the badge would be a claim about a term that is not
	//!    applied;
	//!  - a shop with nothing in stock gets one line saying so;
	//!  - a shop whose every item sits inside the neutral band gets one line saying so, with the
	//!    heading and the row section left hidden.
	//! \param[in] locationInfoWidget The instantiated OVT_MapInfoShop.layout
	//! \param[in] location The shop record being described
	override protected void OnSetupLocationInfo(Widget locationInfoWidget, OVT_MapLocationData location)
	{
		if (!locationInfoWidget || !location)
			return;

		Widget rows = locationInfoWidget.FindAnyWidget(WIDGET_ROWS);

		if (location.m_ShopType == OVT_ShopType.SHOP_VEHICLE)
		{
			AddInfoRow(rows, "", "#OVT-Map_Shop_VehicleFlat");
			return;
		}

		if (!m_Economy)
			return;

		// The badge is the shop-wide half of the model and is meaningful even when no item earns a
		// caret, so it is painted before the rows are known.
		ShowRemotenessBadge(locationInfoWidget, m_PriceIndicator.GetRemotenessLevel(m_Economy, location.m_vPosition));

		OVT_ShopComponent shop = GetShopForLocation(location);
		if (!shop)
			return;

		array<ref OVT_MapShopPriceRow> scored = new array<ref OVT_MapShopPriceRow>();
		int stockedCount = m_PriceIndicator.CollectScarcityRows(m_Economy, m_TownManager, shop, location.m_vPosition, scored);

		if (stockedCount == 0)
		{
			AddInfoRow(rows, "", "#OVT-Map_Shop_NoStock");
			return;
		}

		array<ref OVT_MapShopPriceRow> selected = new array<ref OVT_MapShopPriceRow>();
		m_PriceIndicator.SelectExtremes(scored, MAX_ROWS_PER_DIRECTION, selected);

		// Names are resolved ONLY for rows that survived selection: UIInfo resolution loads and scans
		// a prefab container (N8) and a general store can stock a hundred ids. Resolving before the
		// heading is drawn also guarantees the heading never appears above nothing.
		array<ref OVT_MapShopPriceRow> named = new array<ref OVT_MapShopPriceRow>();

		foreach (OVT_MapShopPriceRow row : selected)
		{
			row.m_sDisplayName = m_PriceIndicator.ResolveDisplayName(m_Economy, row.m_iResourceId);

			if (!row.m_sDisplayName.IsEmpty())
				named.Insert(row);
		}

		if (named.IsEmpty())
		{
			AddInfoRow(rows, "", "#OVT-Map_Shop_NothingUnusual");
			return;
		}

		// K7 - the scarcity term reads TOWN stock, which sums across every shop in the town, so two
		// shops in one town produce identical carets. That is a property of Overthrow's economy, not
		// a defect here, and the panel says so rather than implying this shop sets its own prices.
		ShowSectionHeader(locationInfoWidget, "#OVT-Map_Shop_LocalSupply");

		Widget scarcityRows = locationInfoWidget.FindAnyWidget(WIDGET_SCARCITY_ROWS);

		// Goes into the caret container, not the top-level Rows: the layout draws Rows above the badge
		// and the heading, and this sentence only makes sense directly under "Local supply".
		AddInfoRow(scarcityRows, "", "#OVT-Map_Shop_TownWide");

		foreach (OVT_MapShopPriceRow namedRow : named)
		{
			AddCaretRow(scarcityRows, namedRow.m_eLevel, OVT_MapShopPriceBands.GetDirectionKey(namedRow.m_eLevel), namedRow.m_sDisplayName);
		}
	}

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
	
	//! Get icon name based on shop type
	override string GetIconName(OVT_MapLocationData location)
	{
		if (!location)
			return m_sDefaultIconName;
		
		// Find the shop type configuration
		OVT_ShopTypeInfo shopTypeInfo = GetShopTypeInfo(location.m_ShopType);
		if (shopTypeInfo && !shopTypeInfo.m_sIconName.IsEmpty())
			return shopTypeInfo.m_sIconName;
		
		return m_sDefaultIconName;
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