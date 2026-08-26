//------------------------------------------------------------------------------------------------
//! Buy stock: an unowned production site's one resource, sold at 80% of the live import price.
//!
//! A logistics/ui consumer and nothing else - the eight hooks plus the GetSummaryText() override
//! the base reserved for a consumer whose value column is a price. No widget code lives here.
//!
//! ONE ROW, ALWAYS LISTED. The site produces exactly one resource, so BuildEntries never filters -
//! at zero stock the row stays on screen DISABLED with a reason (B13), it is never hidden.
//------------------------------------------------------------------------------------------------
class OVT_ProductionSiteBuyContext : OVT_TransferContext
{
	//! The one mode this screen offers.
	protected const int MODE_BUY = 0;

	//! Entry-id prefix, matching OVT_ResourceTransferContext so a resource id can never collide with
	//! a ResourceName (which always starts "{").
	protected const string RES_PREFIX = "res:";

	//! Row cap independent of stock - requirement §3.8.
	protected const int MAX_BUY_QUANTITY = 1000;

	//! How far the destination picker looks for a nearby truck.
	protected const float DESTINATION_RADIUS = 25;

	//! Coalescing delay for a contents change, matching OVT_ResourceTransferContext.
	protected const int LIVE_REFRESH_MS = 250;

	//! The site this screen was opened on, and its networked name.
	protected IEntity m_Site;
	protected RplId m_SiteId;

	//! The store whose contents invoker this screen subscribed to. Cached because it outlives the
	//! layout and OnClose must remove exactly what OnShow inserted.
	protected OVT_ResourceStoreComponent m_SubscribedStore;

	//! The request component this screen subscribed to, for the same reason.
	protected OVT_ResourceRequestComponent m_SubscribedRequests;

	//! THE LATCH. True between an opened checkout and its single reply; set before the ask, because
	//! on a listen host the whole reply fan runs inside it.
	protected bool m_bCheckoutPending;

	//! The refusal waiting to be drawn. Empty when there is none.
	protected string m_sPendingError;

	//------------------------------------------------------------------------------------------------
	//! An RplId member is an engine handle, not an int - it is given a value here so IsValid() is
	//! answerable before any site has been set.
	void OVT_ProductionSiteBuyContext()
	{
		m_SiteId = RplId.Invalid();
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the site before ShowContext, the OVT_ResourceTransferContext.SetHolder shape.
	//! \param[in] site The production site entity.
	void SetSite(IEntity site)
	{
		m_Site = site;
		m_SiteId = OVT_ResourceUtils.GetHolderId(site);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The site this screen is open on, or null.
	IEntity GetSite()
	{
		return m_Site;
	}

	//-----------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Subscribes BEFORE super.OnShow(), which draws the first frame.
	override void OnShow()
	{
		m_bCheckoutPending = false;
		m_sPendingError = "";

		// The site may have been re-created since SetSite (a re-open after a respawn).
		if (m_Site)
			m_SiteId = OVT_ResourceUtils.GetHolderId(m_Site);

		Subscribe();

		super.OnShow();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes exactly what OnShow inserted: the store's contents invoker, both request invokers and
	//! the pending callbacks. All of them outlive the layout.
	override void OnClose()
	{
		GetGame().GetCallqueue().Remove(LiveRefresh);
		GetGame().GetCallqueue().Remove(ShowResourceError);

		Unsubscribe();

		super.OnClose();

		m_bCheckoutPending = false;
		m_sPendingError = "";
	}

	//-----------------------------------------------------------------------
	// THE EIGHT HOOKS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	override void BuildModes(out array<int> modes, out array<string> labelKeys)
	{
		modes.Clear();
		labelKeys.Clear();

		modes.Insert(MODE_BUY);
		labelKeys.Insert("#OVT-ProdSite_Buy");
	}

	//------------------------------------------------------------------------------------------------
	//! One row: the site's own resource, at 80% of the live import price, capped by real stock.
	//! Zero stock never hides the row - it lists disabled with a reason (B13).
	override void BuildEntries(int mode, OVT_TransferListModel model)
	{
		if (!model)
			return;

		if (!m_Site)
			return;

		OVT_ResourceProductionComponent production = OVT_ComponentFinder<OVT_ResourceProductionComponent>.Find(m_Site);
		if (!production)
			return;

		string id = production.GetResourceId();
		if (id == "")
			return;

		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
			return;

		OVT_ResourceDefs defs = resources.GetDefs();
		if (!defs)
			return;

		int defIndex = defs.IndexOf(id);
		if (defIndex < 0)
			return;

		int stock = SiteStock();

		OVT_TransferEntry entry = new OVT_TransferEntry();
		entry.m_sId = RES_PREFIX + id;
		entry.m_sDisplayName = OVT_ResourceUtils.ResolveResourceTitle(id);
		entry.m_eImageKind = EOVT_TransferImageKind.TEXTURE;
		entry.m_sImage = ResolveIcon(id);
		entry.m_eValueKind = EOVT_TransferValueKind.PRICE;
		entry.m_iValue = OVT_ResourceProductionRules.SitePrice(resources.GetPrice(defIndex), OVT_ResourceProductionRules.SITE_SELL_RATIO);
		entry.m_iMaxQuantity = Math.Min(stock, MAX_BUY_QUANTITY);
		entry.m_iCategoryId = 0;
		entry.m_bEnabled = true;
		entry.m_sDisabledReasonKey = "";

		if (stock <= 0)
		{
			entry.m_bEnabled = false;
			entry.m_sDisabledReasonKey = "#OVT-ProdSite_NoStock";
		}

		model.Add(entry);
	}

	//------------------------------------------------------------------------------------------------
	//! One row, so no tab row.
	override string GetCategoryLabelKey(int categoryId)
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Every other usable holder within DESTINATION_RADIUS of the site, minus the site itself, nearest
	//! first. Nearest-first rather than query order, which is spatially arbitrary and would reshuffle
	//! the picker under the player's selection between two refreshes that found the same holders.
	override void BuildDestinations(out array<ref OVT_TransferDestination> dests)
	{
		dests.Clear();

		if (!m_Site)
			return;

		array<IEntity> holders = new array<IEntity>();

		// One query object per call; the shared-accumulator singleton is the defect this feature
		// exists to stop repeating (B9).
		OVT_ResourceHolderQuery query = new OVT_ResourceHolderQuery();
		query.Run(m_Site.GetOrigin(), DESTINATION_RADIUS, holders);

		array<IEntity> sorted = new array<IEntity>();
		SortByDistance(holders, m_Site.GetOrigin(), sorted);

		int index = 0;
		foreach (IEntity holder : sorted)
		{
			if (holder == m_Site)
				continue;

			OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(holder);
			if (!store)
				continue;

			RplId id = OVT_ResourceUtils.GetHolderId(holder);
			if (!id.IsValid())
				continue;

			OVT_TransferDestination dest = new OVT_TransferDestination();
			dest.m_sId = "holder" + index.ToString();
			dest.m_sLabel = store.GetDisplayName();
			dest.m_Entity = holder;

			dests.Insert(dest);
			index++;
		}
	}

	//------------------------------------------------------------------------------------------------
	override void FillDetails(OVT_TransferEntry entry, out string name, out string value, out string body)
	{
		name = entry.m_sDisplayName;
		value = FormatValue(entry.m_iValue, entry.m_eValueKind);
		body = "";

		string id = ResourceIdOf(entry.m_sId);

		OVT_Resource res = FindResource(id);
		if (res && res.m_sDescription != "")
			body = WidgetManager.Translate(res.m_sDescription);

		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
			return;

		OVT_ResourceDefs defs = resources.GetDefs();
		if (!defs)
			return;

		int defIndex = defs.IndexOf(id);
		if (defIndex < 0)
			return;

		float cubicMetres = defs.LitresAt(defIndex) / 1000.0;

		string perUnit = WidgetManager.Translate("#OVT-Resource_PerUnit",
			cubicMetres.ToString(-1, 2), defs.KgAt(defIndex).ToString(-1, 1));

		if (body == "")
		{
			body = perUnit;
			return;
		}

		body = body + "\n" + perUnit;
	}

	//------------------------------------------------------------------------------------------------
	//! One row, added whole in one press.
	override bool IsAddAllAllowed(int mode)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Advisory only - the server re-derives every one of these. Order matches context.md's mirror of
	//! the server's refusal ladder: destination, stock, cargo space, money.
	//! \param[in] lines The cart's lines.
	//! \param[in] dest The picker's selection - the truck receiving the stock.
	//! \return "" when the cart may be committed, otherwise an #OVT- key.
	override string ValidateCart(array<ref OVT_TransferCartLine> lines, OVT_TransferDestination dest)
	{
		if (!dest || !dest.m_Entity)
			return "#OVT-Resource_NeedTruck";

		OVT_ResourceStoreComponent receiver = OVT_ResourceUtils.GetStore(dest.m_Entity);
		if (!receiver)
			return "#OVT-Resource_NeedTruck";

		int cartQty = CartQuantity(lines);
		if (cartQty > SiteStock())
			return "#OVT-Resource_NotEnough";

		int cartLitres = CartLitres(lines);
		if (cartLitres > receiver.GetFreeLitres())
			return "#OVT-Resource_NoCargoSpace";

		int cartPrice = CartPrice(lines);
		if (m_Economy && !m_Economy.PlayerHasMoney(m_sPlayerID, cartPrice))
			return "#OVT-Resource_NoMoney";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! ONE CHECKOUT: TransferBegin, one TransferLine per line, TransferCommit.
	//!
	//! On a listen host the entire checkout runs inside this method - every ask invokes its handler
	//! directly, so the move and the reply have both happened before it returns. THE LATCH IS SET
	//! BEFORE THE FIRST ASK for exactly that reason; setting it after would latch over a reply that
	//! already cleared it.
	//! \param[in] lines The cart's lines.
	//! \param[in] dest The picker's selection - the truck receiving the stock.
	override void OnAccept(array<ref OVT_TransferCartLine> lines, OVT_TransferDestination dest)
	{
		if (!lines || lines.IsEmpty())
			return;

		if (!dest || !dest.m_Entity)
			return;

		if (!m_SiteId.IsValid())
			return;

		if (m_bCheckoutPending)
			return;

		OVT_ResourceRequestComponent requests = GetRequests();
		if (!requests)
			return;

		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
			return;

		OVT_ResourceDefs defs = resources.GetDefs();
		if (!defs)
			return;

		RplId destId = OVT_ResourceUtils.GetHolderId(dest.m_Entity);
		if (!destId.IsValid())
		{
			ShowPersistentMessage("#OVT-Resource_NeedTruck");
			return;
		}

		m_bCheckoutPending = true;

		int seq = requests.RequestTransferBegin(m_SiteId, destId, EOVT_ResourceOp.SITE_BUY, lines.Count());
		if (seq == OVT_ResourceRequestComponent.SEQ_NONE)
		{
			m_bCheckoutPending = false;
			return;
		}

		for (int i = 0; i < lines.Count(); i++)
		{
			OVT_TransferCartLine line = lines[i];
			if (!line)
				continue;

			requests.RequestTransferLine(seq, i, defs.IndexOf(ResourceIdOf(line.m_sId)), line.m_iQuantity);
		}

		requests.RequestTransferCommit(seq, lines.Count());
	}

	//------------------------------------------------------------------------------------------------
	//! The total cost and total volume the cart represents, since the base's default only names cost.
	//! \return Already-resolved text, empty on an empty cart.
	override string GetSummaryText()
	{
		if (m_Cart.Count() == 0)
			return "";

		int cartPrice = CartPrice(m_Cart.GetLines());
		int cartLitres = CartLitres(m_Cart.GetLines());

		return WidgetManager.Translate("#OVT-ProdSite_Summary",
			OVT_MoneyFormat.FormatMoney(cartPrice), FormatCubicMetres(cartLitres));
	}

	//-----------------------------------------------------------------------
	// LIVE REFRESH
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The site's replicated contents changed - by this player's own checkout, or by another's.
	//! \param[in] store The store that changed.
	protected void OnContentsChanged(OVT_ResourceStoreComponent store)
	{
		if (!m_bIsActive || !m_wRoot)
			return;

		ScheduleLiveRefresh();
	}

	//------------------------------------------------------------------------------------------------
	//! A checkout finished.
	//! \param[in] movedLitres Litres that moved.
	//! \param[in] earned Money paid to the player; always 0 on this screen.
	//! \param[in] spent Money charged to the player.
	protected void OnTransferResult(int movedLitres, int earned, int spent)
	{
		m_bCheckoutPending = false;

		if (!m_bIsActive || !m_wRoot)
			return;

		ScheduleLiveRefresh();
	}

	//------------------------------------------------------------------------------------------------
	//! A request was refused.
	//!
	//! THE MESSAGE IS DRAWN ON THE NEXT CALL-QUEUE PASS, NEVER HERE. On a listen host the refusal
	//! lands inside OnAccept, which sits inside the base's Accept() - and Accept() prints its own
	//! "order placed" message after OnAccept returns. Drawing synchronously would tell the player the
	//! order succeeded when the server refused it.
	//! \param[in] messageKey Localization key naming the refusal.
	protected void OnResourceError(string messageKey)
	{
		m_bCheckoutPending = false;

		if (!m_bIsActive || !m_wRoot)
			return;

		m_sPendingError = messageKey;

		GetGame().GetCallqueue().Remove(ShowResourceError);
		GetGame().GetCallqueue().CallLater(ShowResourceError, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Draws the refusal once the base call that carried it has unwound.
	protected void ShowResourceError()
	{
		if (!m_bIsActive || !m_wRoot)
			return;

		if (m_sPendingError == "")
			return;

		ShowPersistentMessage(m_sPendingError);
		m_sPendingError = "";
	}

	//------------------------------------------------------------------------------------------------
	//! Coalesces a burst of contents changes into one redraw.
	protected void ScheduleLiveRefresh()
	{
		GetGame().GetCallqueue().Remove(LiveRefresh);
		GetGame().GetCallqueue().CallLater(LiveRefresh, LIVE_REFRESH_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! The coalesced redraw. Refresh() reconciles the cart and restores focus by itself.
	protected void LiveRefresh()
	{
		if (!m_bIsActive || !m_wRoot)
			return;

		Refresh();
	}

	//-----------------------------------------------------------------------
	// SUBSCRIPTIONS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Every subscription here has a matching Remove in Unsubscribe(). Both targets outlive the
	//! layout, so a missed Remove is an extra redraw per site for the rest of the session.
	protected void Subscribe()
	{
		OVT_ResourceRequestComponent requests = OVT_ControllerComponent<OVT_ResourceRequestComponent>.Get();
		if (requests)
		{
			m_SubscribedRequests = requests;
			m_SubscribedRequests.GetOnTransferResult().Insert(OnTransferResult);
			m_SubscribedRequests.GetOnResourceError().Insert(OnResourceError);
		}

		OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(m_Site);
		if (store)
		{
			m_SubscribedStore = store;
			m_SubscribedStore.GetOnContentsChanged().Insert(OnContentsChanged);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Unsubscribe()
	{
		if (m_SubscribedRequests)
		{
			m_SubscribedRequests.GetOnTransferResult().Remove(OnTransferResult);
			m_SubscribedRequests.GetOnResourceError().Remove(OnResourceError);
		}

		if (m_SubscribedStore)
			m_SubscribedStore.GetOnContentsChanged().Remove(OnContentsChanged);

		m_SubscribedRequests = null;
		m_SubscribedStore = null;
	}

	//-----------------------------------------------------------------------
	// HELPERS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \return The local player's resource request component, or null.
	protected OVT_ResourceRequestComponent GetRequests()
	{
		if (m_SubscribedRequests)
			return m_SubscribedRequests;

		return OVT_ControllerComponent<OVT_ResourceRequestComponent>.Get();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The site's current stock of its own resource, 0 when unresolvable.
	protected int SiteStock()
	{
		if (!m_Site)
			return 0;

		OVT_ResourceProductionComponent production = OVT_ComponentFinder<OVT_ResourceProductionComponent>.Find(m_Site);
		if (!production)
			return 0;

		OVT_ResourceStoreComponent store = production.GetStore();
		if (!store)
			return 0;

		OVT_ResourceLedger ledger = store.GetLedger();
		if (!ledger)
			return 0;

		return ledger.Count(production.GetResourceId());
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] lines The cart's lines.
	//! \return The summed quantity across every line.
	protected int CartQuantity(array<ref OVT_TransferCartLine> lines)
	{
		if (!lines)
			return 0;

		int total = 0;
		foreach (OVT_TransferCartLine line : lines)
		{
			if (!line)
				continue;

			total += line.m_iQuantity;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] lines The cart's lines.
	//! \return The whole cart's price.
	protected int CartPrice(array<ref OVT_TransferCartLine> lines)
	{
		if (!lines)
			return 0;

		int total = 0;
		foreach (OVT_TransferCartLine line : lines)
		{
			if (!line)
				continue;

			total += line.m_iQuantity * line.m_iUnitValue;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] lines The cart's lines.
	//! \return The whole cart's volume in litres.
	protected int CartLitres(array<ref OVT_TransferCartLine> lines)
	{
		if (!lines)
			return 0;

		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
			return 0;

		OVT_ResourceDefs defs = resources.GetDefs();
		if (!defs)
			return 0;

		int total = 0;
		foreach (OVT_TransferCartLine line : lines)
		{
			if (!line)
				continue;

			total += line.m_iQuantity * defs.LitresPerUnit(ResourceIdOf(line.m_sId));
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entryId A row or cart-line id.
	//! \return The bare resource id behind it.
	protected string ResourceIdOf(string entryId)
	{
		if (!entryId.StartsWith(RES_PREFIX))
			return entryId;

		return entryId.Substring(RES_PREFIX.Length(), entryId.Length() - RES_PREFIX.Length());
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id A bare resource id.
	//! \return Its catalogue entry, or null when the catalogue does not know it.
	protected OVT_Resource FindResource(string id)
	{
		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
			return null;

		OVT_ResourcesConfig config = resources.GetResourcesConfig();
		if (!config || !config.m_aResources)
			return null;

		foreach (OVT_Resource res : config.m_aResources)
		{
			if (res && res.m_sId == id)
				return res;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id A bare resource id.
	//! \return The row icon, or "" when the catalogue authors none (the base then draws no image).
	protected ResourceName ResolveIcon(string id)
	{
		OVT_Resource res = FindResource(id);
		if (!res)
			return "";

		return res.m_tIcon;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] litres A volume in integer litres.
	//! \return The same volume in m3, one decimal.
	protected string FormatCubicMetres(int litres)
	{
		return OVT_ResourceUtils.FormatCubicMetres(litres);
	}

	//------------------------------------------------------------------------------------------------
	//! Insertion sort by distance, ascending and stable.
	//! \param[in] holders The entities to order.
	//! \param[in] origin The point to measure from.
	//! \param[out] sorted Receives the ordered entities; cleared first.
	protected void SortByDistance(array<IEntity> holders, vector origin, out array<IEntity> sorted)
	{
		sorted.Clear();
		if (!holders)
			return;

		foreach (IEntity holder : holders)
		{
			if (!holder)
				continue;

			float distance = vector.DistanceSq(holder.GetOrigin(), origin);

			int insertAt = sorted.Count();
			for (int i = 0; i < sorted.Count(); i++)
			{
				if (vector.DistanceSq(sorted[i].GetOrigin(), origin) > distance)
				{
					insertAt = i;
					break;
				}
			}

			sorted.InsertAt(holder, insertAt);
		}
	}
}
