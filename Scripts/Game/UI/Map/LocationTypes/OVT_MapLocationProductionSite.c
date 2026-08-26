//! Map presence for resource production sites.
//!
//! DISCOVERY, NOT REGISTRATION (D8). The site set is authored in the world file and therefore
//! identical on every machine, so this reads OVT_ResourceProductionManagerComponent.GetSites() -
//! already discovered by every machine at Init - rather than a marker registry. Only m_EntityID is
//! ever set on the record; m_RplID stays at its RplId.Invalid() default (BUG-188).
//!
//! COLOUR AND ACCESS COME FROM ONE PREDICATE. GetIconColor never compares owner/isPrivate itself -
//! it hands them to OVT_ResourceProductionRules.ColourState, the same function the storage gate uses,
//! so the icon can never disagree with what the player is actually allowed to open.
//!
//! STOCK IS READ FROM THE ENTITY, NEVER THE RECORD. BuildInfoRows resolves location.GetEntity() at
//! panel-open time and reads its OVT_ResourceStoreComponent directly, mirroring
//! OVT_MapLocationResourcePile - a registry must not become a second source of truth for state that
//! already lives, replicated, on the entity.
[BaseContainerProps(), OVT_MapLocationTypeTitle()]
class OVT_MapLocationProductionSite : OVT_MapLocationType
{
	[Attribute(defvalue: "1 1 1 1", UIWidgets.ColorPicker, desc: "Icon colour while the site is unowned", category: "Icon")]
	protected ref Color m_UnownedSiteColor;

	[Attribute(defvalue: "0 1 0 1", UIWidgets.ColorPicker, desc: "Icon colour when the viewer may access the site (theirs, public, or resistance-owned)", category: "Icon")]
	protected ref Color m_AccessibleSiteColor;

	[Attribute(defvalue: "1 1 1 0.5", UIWidgets.ColorPicker, desc: "Icon colour when the site is privately owned by somebody else", category: "Icon")]
	protected ref Color m_PrivateSiteColor;

	//! The resistance-treasury owner sentinel. OVT_ProductionSiteData.owner spells this literally.
	protected static const string RESISTANCE_OWNER = "resistance";

	//------------------------------------------------------------------------------------------------
	//! Emits one record per discovered production site.
	//! Returns cleanly when the manager is not up yet - an empty map beats a script error.
	//! \param[out] locations Array to append this type's records to
	override void PopulateLocations(array<ref OVT_MapLocationData> locations)
	{
		OVT_ResourceProductionManagerComponent production = OVT_Global.GetProduction();
		if (!production)
			return;

		array<ref OVT_ProductionSiteData> sites = production.GetSites();
		if (!sites)
			return;

		foreach (OVT_ProductionSiteData rec : sites)
		{
			if (!rec || !rec.entity || !rec.entity.GetWorld())
				continue;

			OVT_MapLocationData locationData = new OVT_MapLocationData(rec.location, ResolveSiteName(rec.entity), ClassName());
			locationData.m_EntityID = rec.entity.GetID();

			locations.Insert(locationData);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The produced resource's map icon, falling back to the type's authored crate icon when the
	//! resource has no quad of its own yet (R5).
	//! \param[in] location The record being described
	//! \return A quad name in m_IconImageset
	override string GetIconName(OVT_MapLocationData location)
	{
		OVT_Resource resource = ResolveResourceDef(location);
		if (resource && resource.m_sMapIconName != "")
			return resource.m_sMapIconName;

		return m_sIconName;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] location The record being described
	//! \return White unowned, green accessible, half-alpha white private-not-yours
	override Color GetIconColor(OVT_MapLocationData location)
	{
		IEntity site = location.GetEntity();
		if (!site)
			return m_UnownedSiteColor;

		OVT_ResourceProductionManagerComponent production = OVT_Global.GetProduction();
		if (!production)
			return m_UnownedSiteColor;

		int state = OVT_ResourceProductionRules.ColourState(GetCurrentPlayerID(), production.GetSiteOwner(site), production.IsSitePrivate(site));

		if (state == 1)
			return m_AccessibleSiteColor;

		if (state == 2)
			return m_PrivateSiteColor;

		return m_UnownedSiteColor;
	}

	//------------------------------------------------------------------------------------------------
	//! Shared info panel: status, owner (while owned), stock, rate, and the buy price while unowned.
	//! \param[in] location The record being described
	//! \param[in] rowsContainer The shared panel's rows container
	override protected void BuildInfoRows(OVT_MapLocationData location, Widget rowsContainer)
	{
		if (!location || !rowsContainer)
			return;

		// The site may have streamed out since the map was opened
		IEntity site = location.GetEntity();
		if (!site)
			return;

		OVT_ResourceProductionManagerComponent production = OVT_Global.GetProduction();
		if (!production)
			return;

		string siteOwner = production.GetSiteOwner(site);
		bool sitePrivate = production.IsSitePrivate(site);

		AddInfoRow(rowsContainer, "#OVT-Map_Row_Status", ResolveStatusKey(siteOwner, sitePrivate));
		AddOwnerRow(rowsContainer, siteOwner);
		AddStockRows(rowsContainer, site);
		AddRateRow(rowsContainer, site);
		AddBuyPriceRow(rowsContainer, site, siteOwner);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] location The record being described
	//! \return The same status key BuildInfoRows shows
	override string GetLocationDescription(OVT_MapLocationData location)
	{
		if (!location)
			return "#OVT-Unowned";

		IEntity site = location.GetEntity();
		if (!site)
			return "#OVT-Unowned";

		OVT_ResourceProductionManagerComponent production = OVT_Global.GetProduction();
		if (!production)
			return "#OVT-Unowned";

		return ResolveStatusKey(production.GetSiteOwner(site), production.IsSitePrivate(site));
	}

	//------------------------------------------------------------------------------------------------
	//! Objective ownership status text - display only, never an access decision, so it is not routed
	//! through ColourState/MayAccessStore (those answer "can THIS viewer open it", not "what is this
	//! site's status"). Resistance sites read Public: MayAccessStore grants everyone entry to one
	//! regardless of the isPrivate flag RpcAsk_BuySite sets.
	//! \param[in] siteOwner "" unowned, "resistance", or a player persistent id
	//! \param[in] sitePrivate Meaningless while unowned
	//! \return "#OVT-Unowned" / "#OVT-Public" / "#OVT-Private"
	protected string ResolveStatusKey(string siteOwner, bool sitePrivate)
	{
		if (siteOwner.IsEmpty())
			return "#OVT-Unowned";

		if (siteOwner == RESISTANCE_OWNER)
			return "#OVT-Public";

		if (sitePrivate)
			return "#OVT-Private";

		return "#OVT-Public";
	}

	//------------------------------------------------------------------------------------------------
	//! Appends the owner's name row, omitted entirely while unowned.
	//! \param[in] rowsContainer The shared panel's rows container
	//! \param[in] siteOwner "" unowned, "resistance", or a player persistent id
	protected void AddOwnerRow(Widget rowsContainer, string siteOwner)
	{
		if (siteOwner.IsEmpty())
			return;

		string value;
		if (siteOwner == RESISTANCE_OWNER)
		{
			value = "#OVT-Resistance";
		}
		else if (m_Players)
		{
			value = m_Players.GetPlayerName(siteOwner);
		}

		if (value.IsEmpty())
			return;

		AddInfoRow(rowsContainer, "#OVT-Owner", value);
	}

	//------------------------------------------------------------------------------------------------
	//! Appends one row per resource the site's store holds, plus a used/total volume row.
	//! Reads OVT_ResourceStoreComponent on the ENTITY - never a copy on the manager's record.
	//! \param[in] rowsContainer The shared panel's rows container
	//! \param[in] site The site entity
	protected void AddStockRows(Widget rowsContainer, IEntity site)
	{
		OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(site);
		if (!store)
			return;

		OVT_ResourceLedger ledger = store.GetLedger();
		if (!ledger)
			return;

		array<string> ids = new array<string>();
		array<int> quantities = new array<int>();
		ledger.GetLines(ids, quantities);

		for (int i = 0; i < ids.Count(); i++)
		{
			if (quantities[i] <= 0)
				continue;

			AddInfoRow(rowsContainer, OVT_ResourceUtils.ResolveResourceTitle(ids[i]), quantities[i].ToString());
		}

		string used = OVT_ResourceUtils.FormatCubicMetres(store.GetUsedLitres());
		string total = OVT_ResourceUtils.FormatCubicMetres(store.GetCapacityLitres());

		AddInfoRow(rowsContainer, "#OVT-ProdSite_Row_Volume", WidgetManager.Translate("#OVT-ProdSite_Row_VolumeValue", used, total));
	}

	//------------------------------------------------------------------------------------------------
	//! Appends the authored production rate.
	//! \param[in] rowsContainer The shared panel's rows container
	//! \param[in] site The site entity
	protected void AddRateRow(Widget rowsContainer, IEntity site)
	{
		OVT_ResourceProductionComponent production = OVT_ComponentFinder<OVT_ResourceProductionComponent>.Find(site);
		if (!production)
			return;

		string value = WidgetManager.Translate("#OVT-ProdSite_Row_RateValue", production.GetUnitsPerHour().ToString(-1, 1));
		AddInfoRow(rowsContainer, "#OVT-ProdSite_Row_Rate", value);
	}

	//------------------------------------------------------------------------------------------------
	//! Appends the purchase price, only while the site is unowned. Same two inputs as
	//! OVT_BuySiteAction.GetCost - the price shown is the price the action charges.
	//! \param[in] rowsContainer The shared panel's rows container
	//! \param[in] site The site entity
	//! \param[in] siteOwner "" unowned, "resistance", or a player persistent id
	protected void AddBuyPriceRow(Widget rowsContainer, IEntity site, string siteOwner)
	{
		if (!siteOwner.IsEmpty())
			return;

		OVT_ResourceProductionComponent production = OVT_ComponentFinder<OVT_ResourceProductionComponent>.Find(site);
		if (!production)
			return;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return;

		int cost = OVT_ResourceProductionRules.BuyCost(production.GetBaseCost(), config.GetRealEstateCostMultiplier());

		AddInfoRow(rowsContainer, "#OVT-ProdSite_Row_BuyPrice", OVT_MoneyFormat.FormatMoney(cost));
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] site The site entity
	//! \return The site's authored name key, falling back to the type's display name
	protected string ResolveSiteName(IEntity site)
	{
		OVT_ResourceProductionComponent production = OVT_ComponentFinder<OVT_ResourceProductionComponent>.Find(site);
		if (!production)
			return GetDisplayName();

		string name = production.GetSiteName();
		if (name == "")
			return GetDisplayName();

		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] location The record being described
	//! \return The catalogue entry for this site's produced resource, or null
	protected OVT_Resource ResolveResourceDef(OVT_MapLocationData location)
	{
		if (!location)
			return null;

		IEntity site = location.GetEntity();
		if (!site)
			return null;

		OVT_ResourceProductionComponent production = OVT_ComponentFinder<OVT_ResourceProductionComponent>.Find(site);
		if (!production)
			return null;

		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
			return null;

		OVT_ResourcesConfig config = resources.GetResourcesConfig();
		if (!config || !config.m_aResources)
			return null;

		string id = production.GetResourceId();

		foreach (OVT_Resource res : config.m_aResources)
		{
			if (!res || res.m_sId != id)
				continue;

			return res;
		}

		return null;
	}
}
