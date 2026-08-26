//------------------------------------------------------------------------------------------------
//! Which side of the shop the browser is showing.
//!
//! BUY lists the shop's stock (or a procurement shop's vehicle catalogue); SELL lists what the
//! player is carrying in containers. The two are different row sets over the same grid, which is why
//! switching resets the page and the selection (implementation plan D5).
//------------------------------------------------------------------------------------------------
enum OVT_ShopMenuMode
{
	BUY,
	SELL
}

//------------------------------------------------------------------------------------------------
//! The shop menu: a category-tabbed, paginated browser over one shop, in Buy or Sell mode.
//!
//! Everything the browser draws comes from ONE OVT_ShopBrowserModel rebuilt per refresh, and every
//! page number comes from that model's arithmetic - the legacy Count()/15 integer division in here
//! is what made the tail of a 57-item shop unreachable (BUG-024), and not inheriting it is a
//! requirement of this rework (D6).
//!
//! Sell mode NEVER asks IsSoldAtShopCached directly. Both the grey-out decision and the displayed
//! unit price come from OVT_ShopTransactionComponent.ShopBuysResource / GetSellUnitPrice, i.e. from
//! the same code the server will run when it honours the request. Gun dealers and drug stores have
//! no configured inventory rules at all and accept any priced item, so a menu that asked the raw
//! cache would grey out every row at the game's most used sell destination (D3, and the Phase 3
//! deviation recorded in the feature's context notes).
//------------------------------------------------------------------------------------------------
class OVT_ShopContext : OVT_TabHostContext
{
	[Attribute("{6A7C4E1C77B31E40}UI/Layouts/Menu/ShopMenu/ShopMenu_Tab.layout", uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout for one category tab", params: "layout")]
	ResourceName m_TabLayout;

	//! Cards authored in ShopMenu.layout's BrowserGrid. The grid is fixed, so this is the page size.
	protected const int CARDS_PER_PAGE = 15;

	//! How long an in-menu message stays up before it starts to fade.
	protected const int MESSAGE_TIMEOUT_MS = 4000;

	//! Opacity units per second for the toast fade-out - 2.0 is a half-second fade from full.
	protected const float MESSAGE_FADE_SPEED = 2.0;

	//! Refresh delay after a SELL, for the case where the items the server deleted have not yet
	//! replicated out of this client's own inventory. Harmless on a host, where the first refresh
	//! already saw the truth. Shop STOCK no longer needs this: it arrives on m_OnInventoryChanged.
	protected const int TRANSACTION_RECHECK_MS = 400;

	//! Coalescing delay for stock changes. A restock streams one entry per item, so a refresh per
	//! event would rebuild the whole model dozens of times in one tick; one pending Refresh covers
	//! the burst and still lands in the same frame the player would notice.
	protected const int STOCK_REFRESH_MS = 50;

	//! How long a sell request may stay outstanding before the buttons are re-armed anyway. The reply
	//! is a reliable RPC, but the DISPLAY path is not guaranteed to arrive (BUG-090 loses it entirely
	//! on a listen host), and a guard that can wedge the only sell button is worse than the bug it
	//! fixes. Generous enough to cover a bad connection, short enough not to feel broken.
	protected const int SELL_TIMEOUT_MS = 5000;

	protected OVT_ShopComponent m_Shop;
	protected int m_iPageNum = 0;
	protected int m_SelectedResource = -1;
	protected ResourceName m_SelectedResourceName;
	protected int m_iNumPages = 0;

	protected OVT_ShopMenuMode m_eMode = OVT_ShopMenuMode.BUY;
	protected OVT_ShopCategory m_eTab = OVT_ShopCategory.ALL;

	//! Rows currently browsable, rebuilt per refresh from the shop stock or the player's containers.
	protected ref OVT_ShopBrowserModel m_Model = new OVT_ShopBrowserModel();

	//! Resource id -> display name. UIInfo resolution loads and scans a prefab container, which is far
	//! too expensive to repeat per row per refresh; names never change, so one lookup each is enough.
	protected ref map<int, string> m_mDisplayNames = new map<int, string>();

	//! Categories the currently instantiated tab widgets represent, in order. Lets a refresh tell a
	//! selection change (repaint) from a tab-set change (rebuild).
	protected ref array<OVT_ShopCategory> m_aTabOrder = new array<OVT_ShopCategory>();

	//! Whether this shop has a sell path at all (type / procurement / gun-dealer multiplier).
	protected bool m_bShopBuys;
	protected float m_fGunDealerMultiplier;

	//! State of the currently selected row, so the action buttons do not have to re-derive it.
	protected int m_iSelectedQuantity;
	protected bool m_bSelectedEnabled;

	//! True between sending a sell request and receiving its result. A sell RPC is fire-and-forget and
	//! the quantity it carries is what THIS menu last observed - on a remote client that count is stale
	//! until the sold items replicate away, so a second click re-sends the same quantity, the server
	//! finds nothing left, and the "sold nothing" reply overwrites the toast for the sale that did
	//! happen (BUG-100). One request at a time is the whole fix; while this is set the Sell and
	//! Sell All buttons refuse.
	protected bool m_bSellInFlight;

	//! Widgets and handlers cached at show time. Cached rather than re-found because OnClose must
	//! remove exactly the handlers OnShow inserted.
	protected Widget m_wBuyButton;
	protected Widget m_wSellButton;
	protected Widget m_wSellAllButton;
	protected Widget m_wModeBuyButton;
	protected Widget m_wModeSellButton;
	protected Widget m_wPrevCategoryButton;
	protected Widget m_wNextCategoryButton;
	protected Widget m_wHeaderRow;
	protected Widget m_wTabs;

	protected SCR_InputButtonComponent m_BuyAction;
	protected SCR_InputButtonComponent m_SellAction;
	protected SCR_InputButtonComponent m_SellAllAction;
	protected SCR_InputButtonComponent m_PrevAction;
	protected SCR_InputButtonComponent m_NextAction;
	protected SCR_InputButtonComponent m_CloseAction;
	protected SCR_InputButtonComponent m_ModeBuyAction;
	protected SCR_InputButtonComponent m_ModeSellAction;
	protected SCR_InputButtonComponent m_PrevCategoryAction;
	protected SCR_InputButtonComponent m_NextCategoryAction;

	//! The transaction component this context subscribed to, so it unsubscribes from the same one.
	protected OVT_ShopTransactionComponent m_Transactions;

	//! The shop whose stock invoker this context subscribed to. Held separately from m_Shop because
	//! SetShop() may already have pointed m_Shop at the next shop by the time OnClose runs.
	protected OVT_ShopComponent m_SubscribedShop;

	//! The notification manager this context subscribed to, for server results the (hidden) HUD strip
	//! cannot show while the menu is open.
	protected OVT_NotificationManagerComponent m_Notify;

	//------------------------------------------------------------------------------------------------
	override void PostInit()
	{
		if(SCR_Global.IsEditMode()) return;
		m_Economy.m_OnPlayerMoneyChanged.Insert(OnPlayerMoneyChanged);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPlayerMoneyChanged(string playerId, int amount)
	{
		if(playerId != m_sPlayerID) return;
		if(!m_bIsActive || !m_wRoot) return;

		TextWidget money = TextWidget.Cast(m_wRoot.FindAnyWidget("PlayerMoney"));
		if(money) money.SetText(OVT_MoneyFormat.FormatMoney(amount));
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the shop this menu browses. Called by OVT_ShopAction / OVT_GunDealerAction /
	//! OVT_MainMenuContext before the layout is shown.
	//! \param[in] shop The shop to browse.
	void SetShop(OVT_ShopComponent shop)
	{
		m_Shop = shop;
	}

	//-----------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	override void OnShow()
	{
		m_iPageNum = 0;
		m_eMode = OVT_ShopMenuMode.BUY;
		m_eTab = OVT_ShopCategory.ALL;
		m_bSellInFlight = false;
		ClearSelection();

		ResolveSellEligibility();
		WireWidgets();
		SubscribeSellResults();
		SubscribeInventoryChanges();
		SubscribeNotifications();

		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Tears down every handler OnShow inserted. The layout is rebuilt from scratch on every show, so
	//! the widget handlers would be recreated anyway - but the sell-result invoker lives on a
	//! long-lived component and WOULD accumulate one subscription per shop visit.
	override void OnClose()
	{
		GetGame().GetCallqueue().Remove(HideMessage);

		// Both scheduled redraws, because they are separate queue entries now (see ScheduleRefresh).
		// Refresh itself is never queued directly - only ever through one of these two trampolines - so
		// cancelling the pair cancels everything that could redraw a closed menu.
		GetGame().GetCallqueue().Remove(RefreshPostSell);
		GetGame().GetCallqueue().Remove(RefreshFromStockEvent);

		// The in-flight guard is menu state, and the menu is going away. Leaving the timeout queued
		// would re-arm buttons that no longer exist.
		GetGame().GetCallqueue().Remove(OnSellTimeout);
		m_bSellInFlight = false;

		// The layout is about to be destroyed; a running fade would keep animating a dead widget.
		ResetMessage();

		if(m_BuyAction) m_BuyAction.m_OnActivated.Remove(Buy);
		if(m_SellAction) m_SellAction.m_OnActivated.Remove(Sell);
		if(m_SellAllAction) m_SellAllAction.m_OnActivated.Remove(SellAll);
		if(m_PrevAction) m_PrevAction.m_OnActivated.Remove(PreviousPage);
		if(m_NextAction) m_NextAction.m_OnActivated.Remove(NextPage);
		if(m_CloseAction) m_CloseAction.m_OnActivated.Remove(CloseLayout);
		if(m_ModeBuyAction) m_ModeBuyAction.m_OnActivated.Remove(ModeBuy);
		if(m_ModeSellAction) m_ModeSellAction.m_OnActivated.Remove(ModeSell);
		if(m_PrevCategoryAction) m_PrevCategoryAction.m_OnActivated.Remove(PreviousCategory);
		if(m_NextCategoryAction) m_NextCategoryAction.m_OnActivated.Remove(NextCategory);

		if(m_Transactions && m_Transactions.m_OnSellResult)
			m_Transactions.m_OnSellResult.Remove(OnSellResult);

		// These two live on components that outlive the menu, so a missed Remove is one extra Refresh
		// (and one extra toast) per shop visit for the rest of the session.
		if(m_SubscribedShop && m_SubscribedShop.m_OnInventoryChanged)
			m_SubscribedShop.m_OnInventoryChanged.Remove(OnShopInventoryChanged);

		if(m_Notify && m_Notify.m_OnNotification)
			m_Notify.m_OnNotification.Remove(OnNotification);

		m_Transactions = null;
		m_SubscribedShop = null;
		m_Notify = null;

		m_BuyAction = null;
		m_SellAction = null;
		m_SellAllAction = null;
		m_PrevAction = null;
		m_NextAction = null;
		m_CloseAction = null;
		m_ModeBuyAction = null;
		m_ModeSellAction = null;
		m_PrevCategoryAction = null;
		m_NextCategoryAction = null;

		m_wBuyButton = null;
		m_wSellButton = null;
		m_wSellAllButton = null;
		m_wModeBuyButton = null;
		m_wModeSellButton = null;
		m_wPrevCategoryButton = null;
		m_wNextCategoryButton = null;
		m_wHeaderRow = null;
		m_wTabs = null;

		// The whole layout is destroyed on close, so the tab widgets are gone with it; forgetting the
		// order here means the next show rebuilds them instead of trusting a stale match.
		m_aTabOrder.Clear();
		m_Model.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this shop buys from players at all, plus the gun-dealer rate the details pane shows.
	//! Derived exactly as the server derives it, from one rule set (OVT_ShopSellRules).
	protected void ResolveSellEligibility()
	{
		m_fGunDealerMultiplier = 0;
		m_bShopBuys = false;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(config && config.m_Difficulty)
			m_fGunDealerMultiplier = config.m_Difficulty.gunDealerSellPriceMultiplier;

		if(!m_Shop) return;

		m_bShopBuys = OVT_ShopSellRules.ShopBuysFromPlayers(m_Shop.m_ShopType, m_Shop.m_bProcurement, m_fGunDealerMultiplier);
	}

	//------------------------------------------------------------------------------------------------
	//! Finds and wires every button in the layout. Every lookup is null-guarded so a stale layout
	//! degrades to a missing button rather than a script error (quality bar Q7).
	protected void WireWidgets()
	{
		if(!m_wRoot) return;

		m_wHeaderRow = m_wRoot.FindAnyWidget("HeaderRow");
		m_wTabs = m_wRoot.FindAnyWidget("Tabs");

		m_wBuyButton = m_wRoot.FindAnyWidget("BuyButton");
		if(m_wBuyButton)
		{
			m_BuyAction = SCR_InputButtonComponent.Cast(m_wBuyButton.FindHandler(SCR_InputButtonComponent));
			if(m_BuyAction) m_BuyAction.m_OnActivated.Insert(Buy);
		}

		m_wSellButton = m_wRoot.FindAnyWidget("SellButton");
		if(m_wSellButton)
		{
			m_SellAction = SCR_InputButtonComponent.Cast(m_wSellButton.FindHandler(SCR_InputButtonComponent));
			if(m_SellAction) m_SellAction.m_OnActivated.Insert(Sell);
		}

		m_wSellAllButton = m_wRoot.FindAnyWidget("SellAllButton");
		if(m_wSellAllButton)
		{
			m_SellAllAction = SCR_InputButtonComponent.Cast(m_wSellAllButton.FindHandler(SCR_InputButtonComponent));
			if(m_SellAllAction) m_SellAllAction.m_OnActivated.Insert(SellAll);
		}

		m_wModeBuyButton = m_wRoot.FindAnyWidget("ModeBuyButton");
		if(m_wModeBuyButton)
		{
			m_ModeBuyAction = SCR_InputButtonComponent.Cast(m_wModeBuyButton.FindHandler(SCR_InputButtonComponent));
			if(m_ModeBuyAction) m_ModeBuyAction.m_OnActivated.Insert(ModeBuy);
		}

		m_wModeSellButton = m_wRoot.FindAnyWidget("ModeSellButton");
		if(m_wModeSellButton)
		{
			m_ModeSellAction = SCR_InputButtonComponent.Cast(m_wModeSellButton.FindHandler(SCR_InputButtonComponent));
			if(m_ModeSellAction) m_ModeSellAction.m_OnActivated.Insert(ModeSell);
		}

		m_wPrevCategoryButton = m_wRoot.FindAnyWidget("PrevCategoryButton");
		if(m_wPrevCategoryButton)
		{
			m_PrevCategoryAction = SCR_InputButtonComponent.Cast(m_wPrevCategoryButton.FindHandler(SCR_InputButtonComponent));
			if(m_PrevCategoryAction) m_PrevCategoryAction.m_OnActivated.Insert(PreviousCategory);
		}

		m_wNextCategoryButton = m_wRoot.FindAnyWidget("NextCategoryButton");
		if(m_wNextCategoryButton)
		{
			m_NextCategoryAction = SCR_InputButtonComponent.Cast(m_wNextCategoryButton.FindHandler(SCR_InputButtonComponent));
			if(m_NextCategoryAction) m_NextCategoryAction.m_OnActivated.Insert(NextCategory);
		}

		Widget prevButton = m_wRoot.FindAnyWidget("PrevButton");
		if(prevButton)
		{
			m_PrevAction = SCR_InputButtonComponent.Cast(prevButton.FindHandler(SCR_InputButtonComponent));
			if(m_PrevAction) m_PrevAction.m_OnActivated.Insert(PreviousPage);
		}

		Widget nextButton = m_wRoot.FindAnyWidget("NextButton");
		if(nextButton)
		{
			m_NextAction = SCR_InputButtonComponent.Cast(nextButton.FindHandler(SCR_InputButtonComponent));
			if(m_NextAction) m_NextAction.m_OnActivated.Insert(NextPage);
		}

		Widget closeButton = m_wRoot.FindAnyWidget("CloseButton");
		if(closeButton)
		{
			m_CloseAction = SCR_InputButtonComponent.Cast(closeButton.FindHandler(SCR_InputButtonComponent));
			if(m_CloseAction) m_CloseAction.m_OnActivated.Insert(CloseLayout);
		}

		ResetMessage();
	}

	//------------------------------------------------------------------------------------------------
	//! Listens for sell outcomes so the browser can rebuild from what the SERVER actually did.
	protected void SubscribeSellResults()
	{
		m_Transactions = OVT_ControllerComponent<OVT_ShopTransactionComponent>.Get();
		if(!m_Transactions || !m_Transactions.m_OnSellResult) return;

		m_Transactions.m_OnSellResult.Insert(OnSellResult);
	}

	//------------------------------------------------------------------------------------------------
	//! Listens for stock changes on the browsed shop, so the grid follows the server's stock instead of
	//! guessing when it might have arrived (F14).
	protected void SubscribeInventoryChanges()
	{
		if(!m_Shop || !m_Shop.m_OnInventoryChanged) return;

		m_SubscribedShop = m_Shop;
		m_SubscribedShop.m_OnInventoryChanged.Insert(OnShopInventoryChanged);
	}

	//------------------------------------------------------------------------------------------------
	//! Listens for notifications addressed to this player. The HUD strip that normally shows them is
	//! hidden while this menu is open, so without this a failed purchase would be silent (F15).
	protected void SubscribeNotifications()
	{
		m_Notify = OVT_Global.GetNotify();
		if(!m_Notify || !m_Notify.m_OnNotification)
		{
			m_Notify = null;
			return;
		}

		m_Notify.m_OnNotification.Insert(OnNotification);
	}

	//------------------------------------------------------------------------------------------------
	//! One stock entry of the browsed shop changed on the server.
	//! \param[in] id Resource id whose stock changed.
	//! \param[in] amount The new stock level.
	protected void OnShopInventoryChanged(int id, int amount)
	{
		if(!m_bIsActive || !m_wRoot) return;

		ScheduleStockRefresh();
	}

	//------------------------------------------------------------------------------------------------
	//! A notification for this player arrived while the menu is open.
	//!
	//! Only the buy-path outcomes are surfaced in the menu: everything else (town captured, QRF
	//! inbound) belongs to the HUD strip and would be noise on top of the browser.
	//! \param[in] tag Notification tag from OVT_NotificationManagerComponent.
	//! \param[in] param1 First localization parameter.
	//! \param[in] param2 Second localization parameter.
	//! \param[in] param3 Third localization parameter.
	protected void OnNotification(string tag, string param1, string param2, string param3)
	{
		if(!m_bIsActive || !m_wRoot) return;
		if(!IsBuyResultTag(tag)) return;
		if(!m_Notify) return;

		SCR_SimpleMessagePreset preset = m_Notify.GetMessagePreset(tag);
		if(!preset || !preset.m_UIInfo) return;

		ShowMessageFormat(preset.m_UIInfo.GetDescription(), param1, param2, param3);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] tag Notification tag.
	//! \return True when the tag is a result of a purchase made from this menu.
	protected bool IsBuyResultTag(string tag)
	{
		if(tag == "PurchaseFailedInventoryFull") return true;
		if(tag == "PurchaseFailedInsufficientFunds") return true;
		if(tag == "PurchasePartialSuccess") return true;

		return false;
	}

	//-----------------------------------------------------------------------
	// BROWSING STATE
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Switches the browsed category. Called by OVT_ShopMenuTabComponent.
	//!
	//! Changing the row set invalidates the page index and the selection, so both are reset - the
	//! whole interaction contract of D5 in three lines.
	//! \param[in] category The tab that was clicked. ALL is the "no filter" tab.
	void SelectTab(OVT_ShopCategory category)
	{
		m_eTab = category;
		m_iPageNum = 0;
		ClearSelection();
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Switches between browsing the shop's stock and browsing the player's own items.
	//!
	//! The tab resets to ALL as well as the page, because the populated categories of the two modes
	//! are unrelated - keeping a tab that the new mode has no rows for would land the player on an
	//! empty grid.
	//! \param[in] mode The mode to switch to.
	void SetMode(OVT_ShopMenuMode mode)
	{
		if(m_eMode == mode) return;
		if(mode == OVT_ShopMenuMode.SELL && !CanSellHere()) return;

		m_eMode = mode;
		m_eTab = OVT_ShopCategory.ALL;
		m_iPageNum = 0;
		ClearSelection();
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The mode currently browsed.
	OVT_ShopMenuMode GetMode()
	{
		return m_eMode;
	}

	//! OVT_TabHostContext: tab pick and active-tab query, forwarded to the category tab.
	override void SelectTabId(int tabId)
	{
		OVT_ShopCategory category = tabId;
		SelectTab(category);
	}

	override bool IsTabIdActive(int tabId)
	{
		return m_eTab == tabId;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The category tab currently browsed.
	OVT_ShopCategory GetTab()
	{
		return m_eTab;
	}

	//------------------------------------------------------------------------------------------------
	//! ModeBuyButton / OverthrowShopModeBuy.
	void ModeBuy(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		SetMode(OVT_ShopMenuMode.BUY);
	}

	//------------------------------------------------------------------------------------------------
	//! ModeSellButton / OverthrowShopModeSell.
	void ModeSell(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		SetMode(OVT_ShopMenuMode.SELL);
	}

	//------------------------------------------------------------------------------------------------
	//! PrevCategoryButton / OverthrowShopPrevCategory.
	void PreviousCategory(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		CycleTab(-1);
	}

	//------------------------------------------------------------------------------------------------
	//! NextCategoryButton / OverthrowShopNextCategory.
	void NextCategory(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		CycleTab(1);
	}

	//------------------------------------------------------------------------------------------------
	//! Steps through the visible tabs, wrapping at both ends, so a controller never needs to point at
	//! an individual tab.
	//! \param[in] delta -1 for the previous tab, +1 for the next.
	void CycleTab(int delta)
	{
		array<OVT_ShopCategory> tabs = new array<OVT_ShopCategory>();
		BuildTabOrder(tabs);

		// Fewer than two populated categories means there is no tab row to cycle through.
		if(tabs.Count() < 2) return;

		int index = tabs.Find(m_eTab);
		if(index < 0) index = 0;

		index = index + delta;
		if(index < 0) index = tabs.Count() - 1;
		if(index >= tabs.Count()) index = 0;

		SelectTab(tabs[index]);
	}

	//------------------------------------------------------------------------------------------------
	//! The tabs the current model earns: ALL followed by every populated category, or nothing at all
	//! when fewer than two categories are populated (F2).
	//! \param[out] tabs Receives the tab order. Cleared first; empty means "no tab row".
	protected void BuildTabOrder(out array<OVT_ShopCategory> tabs)
	{
		if(!tabs) return;

		tabs.Clear();

		array<OVT_ShopCategory> categories = new array<OVT_ShopCategory>();
		m_Model.GetPopulatedCategories(categories);

		if(categories.Count() < 2) return;

		tabs.Insert(OVT_ShopCategory.ALL);

		foreach(OVT_ShopCategory category : categories)
		{
			tabs.Insert(category);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Whether Sell mode can be offered: the shop must buy from players AND the controller component
	//! that performs the sale must exist, because without it every Sell click would silently do
	//! nothing.
	protected bool CanSellHere()
	{
		if(!m_bShopBuys) return false;

		OVT_ShopTransactionComponent transactions = OVT_ControllerComponent<OVT_ShopTransactionComponent>.Get();
		if(!transactions) return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	void PreviousPage()
	{
		if(!m_wRoot) return;

		m_iPageNum = OVT_ShopBrowserModel.ClampPage(m_iPageNum - 1, m_iNumPages);
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	void NextPage()
	{
		if(!m_wRoot) return;

		m_iPageNum = OVT_ShopBrowserModel.ClampPage(m_iPageNum + 1, m_iNumPages);
		Refresh();
	}

	//-----------------------------------------------------------------------
	// REFRESH
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the whole browser: model, tabs, page, cards, details and buttons.
	void Refresh()
	{
		if(!m_Shop) return;
		if(!m_wRoot) return;
		if(!m_bIsActive) return;
		if(!m_Economy) return;

		TextWidget money = TextWidget.Cast(m_wRoot.FindAnyWidget("PlayerMoney"));
		if(money) money.SetText(OVT_MoneyFormat.FormatMoney(m_Economy.GetPlayerMoney(m_sPlayerID)));

		BuildModel();

		// A mode whose sell path went away (shop type re-read, controller lost) must not leave the
		// browser stranded in an empty Sell grid.
		if(m_eMode == OVT_ShopMenuMode.SELL && !CanSellHere())
		{
			m_eMode = OVT_ShopMenuMode.BUY;
			m_eTab = OVT_ShopCategory.ALL;
			m_iPageNum = 0;
			ClearSelection();
			BuildModel();
		}

		if(m_eTab != OVT_ShopCategory.ALL && !m_Model.HasCategory(m_eTab))
		{
			m_eTab = OVT_ShopCategory.ALL;
			m_iPageNum = 0;
		}

		RefreshHeader();

		array<ref OVT_ShopBrowserItem> filtered = new array<ref OVT_ShopBrowserItem>();
		m_Model.FilterByCategory(m_eTab, filtered);

		m_iNumPages = OVT_ShopBrowserModel.GetPageCount(filtered.Count(), CARDS_PER_PAGE);
		m_iPageNum = OVT_ShopBrowserModel.ClampPage(m_iPageNum, m_iNumPages);

		TextWidget pages = TextWidget.Cast(m_wRoot.FindAnyWidget("Pages"));
		if(pages) pages.SetText((m_iPageNum + 1).ToString() + "/" + m_iNumPages);

		// Selection preservation: keep the selected resource if it is still browsable in this tab,
		// otherwise fall back to the first card of the page (drawn below).
		if(m_SelectedResource != -1 && !ContainsResource(filtered, m_SelectedResource))
			ClearSelection();

		DrawCards();

		// A preserved selection must be re-read from the freshly built model, or the details pane keeps
		// showing the quantity and price from before the transaction.
		if(m_SelectedResource != -1 && !m_SelectedResourceName.IsEmpty())
			SelectItem(m_SelectedResourceName);

		RefreshActionButtons();
	}

	//------------------------------------------------------------------------------------------------
	//! Builds the row set for the current mode and sorts it alphabetically (Definition of Done F3).
	protected void BuildModel()
	{
		m_Model.Clear();

		if(m_eMode == OVT_ShopMenuMode.SELL)
		{
			BuildSellModel();
		}else{
			BuildBuyModel();
		}

		m_Model.SortByDisplayName();
	}

	//------------------------------------------------------------------------------------------------
	//! Buy rows: the shop's replicated stock, or a procurement shop's parking-filtered vehicle
	//! catalogue. Same items and same prices as before this rework - only the pagination moved.
	protected void BuildBuyModel()
	{
		if(!m_Shop || !m_Economy) return;

		IEntity shopEntity = m_Shop.GetOwner();
		if(!shopEntity) return;

		vector shopPos = shopEntity.GetOrigin();

		if(m_Shop.m_bProcurement)
		{
			array<ResourceName> vehicles = new array<ResourceName>();
			CollectProcurementVehicles(vehicles);

			foreach(ResourceName res : vehicles)
			{
				if(!m_Economy.IsRegisteredResource(res)) continue;

				int vehicleId = m_Economy.GetInventoryId(res);
				int vehicleBuy = m_Economy.GetShopBuyPrice(vehicleId, m_Shop, shopPos, m_iPlayerID);

				// -1 keeps the stock corner hidden, exactly as the procurement browser looked before
				// the card's stock counter was fixed: a procurement shop has no stock to speak of.
				AddRow(vehicleId, res, vehicleBuy, -1, true, "");
			}

			return;
		}

		// We only read m_Shop.m_aInventory because m_Shop.m_aInventoryItems is not replicated
		for(int i = 0; i < m_Shop.m_aInventory.Count(); i++)
		{
			int id = m_Shop.m_aInventory.GetKey(i);
			if(!m_Economy.IsValidResourceId(id)) continue;

			ResourceName res = m_Economy.GetResource(id);
			if(res.IsEmpty()) continue;

			int buy = m_Economy.GetShopBuyPrice(id, m_Shop, shopPos, m_iPlayerID);
			int qty = m_Shop.GetStock(id);

			AddRow(id, res, buy, qty, true, "");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The procurement (garage / helipad) vehicle list, lifted unchanged from the legacy Refresh():
	//! every non-occupying-faction vehicle, filtered to the parking types this site has, minus the
	//! mobile FOB when it is restricted to officers.
	//! \param[out] vehicles Receives the buyable vehicle prefabs.
	protected void CollectProcurementVehicles(out array<ResourceName> vehicles)
	{
		if(!vehicles) return;

		vehicles.Clear();

		if(!m_Shop || !m_Economy) return;

		OVT_ParkingComponent parking = OVT_ComponentFinder<OVT_ParkingComponent>.Find(m_Shop.GetOwner());
		if(!parking) return;

		array<OVT_ParkingType> parkingTypes = new array<OVT_ParkingType>();
		parking.GetParkingTypes(parkingTypes);

		// Get all vehicles (both legal and illegal) like regular vehicle shops do
		array<ResourceName> allVehicles = new array<ResourceName>();
		m_Economy.GetAllNonOccupyingFactionVehicles(allVehicles, true); // includeIllegal = true

		// Filter by parking types like regular vehicle shops do
		foreach(ResourceName res : allVehicles)
		{
			int id = m_Economy.GetInventoryId(res);
			OVT_ParkingType parkingType = m_Economy.GetParkingType(id);
			if(parkingTypes.Contains(parkingType))
			{
				vehicles.Insert(res);
			}
		}

		// Filter out Mobile FOB vehicles if restricted to officers only
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(config && config.m_ConfigFile && config.m_ConfigFile.mobileFOBOfficersOnly && !OVT_Global.GetPlayers().LocalPlayerIsOfficer())
		{
			for(int j = vehicles.Count() - 1; j >= 0; j--)
			{
				string vehiclePath = vehicles[j];
				if(vehiclePath.Contains("OverthrowMobileFOB"))
				{
					vehicles.Remove(j);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Sell rows: what the local player is carrying in containers, aggregated by resource id.
	//!
	//! The client-side scan is the SAME OVT_SellableItemScanner call the server re-runs before it
	//! deletes anything, so "what the menu offered" and "what the server will sell" are one rule (D3).
	//! Anything that never entered the resource database is skipped rather than priced as item 0 (R7).
	protected void BuildSellModel()
	{
		if(!m_Shop || !m_Economy) return;

		OVT_ShopTransactionComponent transactions = OVT_ControllerComponent<OVT_ShopTransactionComponent>.Get();
		if(!transactions) return;

		IEntity character = GetLocalCharacter();
		if(!character) return;

		array<IEntity> items = new array<IEntity>();
		OVT_SellableItemScanner.CollectUnequippedItems(character, items);

		map<int, int> counts = new map<int, int>();

		foreach(IEntity item : items)
		{
			if(!item) continue;

			ResourceName res = OVT_Global.GetPrefabName(item);
			if(res.IsEmpty()) continue;

			// Same resolution the server sells by: an uncatalogued variant lists under the registered
			// prefab it inherits.
			ResourceName pricing = m_Economy.ResolvePricingResource(res);
			if(pricing.IsEmpty()) continue;

			int id = m_Economy.GetInventoryId(pricing);
			if(!counts.Contains(id)) counts[id] = 0;
			counts[id] = counts[id] + 1;
		}

		for(int i = 0; i < counts.Count(); i++)
		{
			int id = counts.GetKey(i);
			int qty = counts.GetElement(i);
			if(qty <= 0) continue;

			if(!m_Economy.IsValidResourceId(id)) continue;

			ResourceName res = m_Economy.GetResource(id);
			if(res.IsEmpty()) continue;

			// Both answers come from the transaction component, never from IsSoldAtShopCached: shop
			// types with no configured rules (gun dealer, drug store) accept any priced item, and a
			// stricter client-side gate would grey out rows the server would happily buy.
			int unitPrice = transactions.GetSellUnitPrice(m_Shop, id);
			bool shopBuys = transactions.ShopBuysResource(m_Shop, id);

			// The scanner already excluded everything worn, held, slung or holstered (D13), so the
			// equipped half of the rule is settled before it gets here.
			bool enabled = OVT_ShopSellRules.CanSellItem(false, shopBuys, unitPrice);
			string reason = OVT_ShopSellRules.GetBlockReasonKey(false, shopBuys, unitPrice);

			AddRow(id, res, unitPrice, qty, enabled, reason);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Appends one row to the model.
	//! \param[in] id Economy resource id - the wire format for buy/sell requests.
	//! \param[in] res Prefab, for the card preview.
	//! \param[in] price Buy price in Buy mode, unit sell price in Sell mode.
	//! \param[in] qty Stock held (Buy) or copies carried (Sell). -1 hides the card's stock corner.
	//! \param[in] enabled False draws the card dimmed and blocks the sell buttons.
	//! \param[in] reasonKey #OVT- key naming why, empty when enabled.
	protected void AddRow(int id, ResourceName res, int price, int qty, bool enabled, string reasonKey)
	{
		OVT_ShopBrowserItem row = new OVT_ShopBrowserItem();

		row.m_iResourceId = id;
		row.m_sResource = res;
		row.m_sDisplayName = ResolveDisplayName(id, res);
		row.m_eCategory = m_Economy.GetItemCategory(id);
		row.m_iUnitPrice = price;
		row.m_iQuantity = qty;
		row.m_bEnabled = enabled;
		row.m_sDisabledReasonKey = reasonKey;

		m_Model.Add(row);
	}

	//------------------------------------------------------------------------------------------------
	//! Localized display name for a resource, memoised for the life of this context.
	//!
	//! Translated rather than raw, because the sort is supposed to be alphabetical by what the player
	//! READS - sorting raw "#AR-Item_..." keys would produce an order with no relation to the grid.
	//! \param[in] id Resource id, the cache key.
	//! \param[in] res The prefab to resolve.
	//! \return A non-empty, sortable name.
	protected string ResolveDisplayName(int id, ResourceName res)
	{
		if(m_mDisplayNames.Contains(id)) return m_mDisplayNames[id];

		string name = "";

		if(m_Economy && m_Economy.IsVehicle(res))
		{
			SCR_EditableVehicleUIInfo vehicleInfo = OVT_PrefabUtils.GetVehicleUIInfo(res);
			if(vehicleInfo)
			{
				name = vehicleInfo.GetName();
			}else{
				SCR_EditableEntityUIInfo editableInfo = OVT_PrefabUtils.GetEditableUIInfo(res);
				if(editableInfo) name = editableInfo.GetName();
			}
		}else{
			UIInfo info = OVT_PrefabUtils.GetItemUIInfo(res);
			if(info) name = info.GetName();
		}

		if(name == "") name = res;

		string translated = WidgetManager.Translate(name);
		if(translated != "") name = translated;

		m_mDisplayNames[id] = name;
		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the tab row and the mode toggle, and hides the header entirely when it would be empty.
	protected void RefreshHeader()
	{
		bool showToggle = CanSellHere();

		if(m_wModeBuyButton) m_wModeBuyButton.SetVisible(showToggle);
		if(m_wModeSellButton) m_wModeSellButton.SetVisible(showToggle);

		ApplyToggleVisual(m_wModeBuyButton, m_ModeBuyAction, m_eMode == OVT_ShopMenuMode.BUY);
		ApplyToggleVisual(m_wModeSellButton, m_ModeSellAction, m_eMode == OVT_ShopMenuMode.SELL);

		bool showTabs = RefreshTabs();

		// The category steppers are meaningless without a tab row, and a hidden SCR_InputButtonComponent
		// also refuses its keybind - so hiding them disables the shortcut too.
		if(m_wPrevCategoryButton) m_wPrevCategoryButton.SetVisible(showTabs);
		if(m_wNextCategoryButton) m_wNextCategoryButton.SetVisible(showTabs);

		if(m_wHeaderRow) m_wHeaderRow.SetVisible(showToggle || showTabs);
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the category tabs from the categories the current model actually populates.
	//!
	//! Fewer than two populated categories means no tab row at all (F2) - which is exactly why a
	//! vehicle shop, whose every row lands in OTHER, still looks like it always did.
	//! \return True when a tab row is shown.
	protected bool RefreshTabs()
	{
		if(!m_wTabs) return false;

		array<OVT_ShopCategory> tabs = new array<OVT_ShopCategory>();
		BuildTabOrder(tabs);

		if(tabs.Count() < 2)
		{
			ClearTabs();
			m_wTabs.SetVisible(false);
			return false;
		}

		m_wTabs.SetVisible(true);

		// THE TAB WIDGETS ARE ONLY REBUILT WHEN THE TAB SET ITSELF CHANGES. Clicking a tab changes
		// which tab is selected, never which tabs exist, so the widgets survive - which keeps gamepad
		// focus where the player put it, and keeps a tab from destroying itself inside its own click.
		if(!TabOrderMatches(tabs)) RebuildTabs(tabs);

		UpdateTabSelection();

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] tabs The tab order the model now wants.
	//! \return True when the instantiated tabs are already exactly that.
	protected bool TabOrderMatches(array<OVT_ShopCategory> tabs)
	{
		if(!tabs) return false;
		if(!m_wTabs || !m_wTabs.GetChildren()) return false;
		if(m_aTabOrder.Count() != tabs.Count()) return false;

		for(int i = 0; i < tabs.Count(); i++)
		{
			if(m_aTabOrder[i] != tabs[i]) return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Destroys every tab widget and forgets the order.
	protected void ClearTabs()
	{
		m_aTabOrder.Clear();

		if(!m_wTabs) return;

		while(m_wTabs.GetChildren())
			m_wTabs.RemoveChild(m_wTabs.GetChildren());
	}

	//------------------------------------------------------------------------------------------------
	//! Instantiates the tab row from scratch.
	//! \param[in] tabs The tab order to build.
	protected void RebuildTabs(array<OVT_ShopCategory> tabs)
	{
		ClearTabs();

		if(!tabs) return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if(!workspace || m_TabLayout.IsEmpty()) return;

		foreach(OVT_ShopCategory category : tabs)
		{
			if(CreateTab(workspace, category)) m_aTabOrder.Insert(category);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Instantiates one tab into the tab row.
	//! \param[in] workspace The widget workspace.
	//! \param[in] category The category the tab selects.
	//! \return True when the tab was created and wired.
	protected bool CreateTab(WorkspaceWidget workspace, OVT_ShopCategory category)
	{
		if(!workspace || !m_wTabs) return false;

		Widget w = workspace.CreateWidgets(m_TabLayout, m_wTabs);
		if(!w) return false;

		OVT_ShopMenuTabComponent tab = OVT_ShopMenuTabComponent.Cast(w.FindHandler(OVT_ShopMenuTabComponent));
		if(!tab) return false;

		tab.Init(category, OVT_ShopCategoryHelper.GetLabelKey(category), this, category == m_eTab);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Repaints the selected state of the existing tab widgets.
	protected void UpdateTabSelection()
	{
		if(!m_wTabs) return;

		Widget child = m_wTabs.GetChildren();
		while(child)
		{
			OVT_ShopMenuTabComponent tab = OVT_ShopMenuTabComponent.Cast(child.FindHandler(OVT_ShopMenuTabComponent));
			if(tab) tab.SetSelected(tab.GetTabId() == m_eTab);

			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Selected / unselected look for a mode-toggle button.
	//!
	//! The button keeps its keybind hint (it is a WLib_NavigationButton like Buy and Sell), so the
	//! active mode is marked by tinting its label with the menu accent rather than by disabling
	//! anything - a greyed-out button would read as "you cannot do this".
	//! \param[in] button The button widget. May be null.
	//! \param[in] action The button's input component. May be null.
	//! \param[in] selected Whether this button is the active one.
	protected void ApplyToggleVisual(Widget button, SCR_InputButtonComponent action, bool selected)
	{
		if(button)
		{
			if(selected)
			{
				button.SetOpacity(1.0);
			}else{
				button.SetOpacity(0.6);
			}
		}

		if(!action) return;

		if(selected)
		{
			action.SetLabelColor(Color.FromInt(0xFFC26414));
		}else{
			action.SetLabelColor(Color.White);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Draws one page of rows onto the fixed 15-card grid, and blanks the cards it did not use.
	//! The page slice comes from the model, so no index arithmetic happens here (D6).
	protected void DrawCards()
	{
		Widget grid = m_wRoot.FindAnyWidget("BrowserGrid");
		if(!grid) return;

		array<ref OVT_ShopBrowserItem> pageItems = new array<ref OVT_ShopBrowserItem>();
		m_Model.GetPageItems(m_eTab, m_iPageNum, CARDS_PER_PAGE, pageItems);

		int wi = 0;

		foreach(OVT_ShopBrowserItem item : pageItems)
		{
			if(wi >= CARDS_PER_PAGE) break;

			Widget w = grid.FindWidget("ShopMenu_Card" + wi);
			if(!w)
			{
				wi++;
				continue;
			}

			if(wi == 0 && m_SelectedResource == -1)
				SelectItem(item.m_sResource);

			w.SetVisible(true);
			w.SetEnabled(true);
			w.SetOpacity(1);

			OVT_ShopMenuCardComponent card = OVT_ShopMenuCardComponent.Cast(w.FindHandler(OVT_ShopMenuCardComponent));
			if(card) card.Init(item.m_sResource, item.m_iUnitPrice, item.m_iQuantity, this, item.m_bEnabled, item.m_sDisabledReasonKey);

			wi++;
		}

		for(; wi < CARDS_PER_PAGE; wi++)
		{
			Widget w = grid.FindWidget("ShopMenu_Card" + wi);
			if(!w) continue;

			w.SetOpacity(0);
			// Blanked cards are disabled as well as invisible, so a click on empty space cannot
			// re-select whatever the card was showing on the previous page.
			w.SetEnabled(false);
		}

		if(pageItems.IsEmpty()) ClearDetails();
	}

	//------------------------------------------------------------------------------------------------
	//! Shows / hides and enables / disables the details-pane action buttons for the current mode and
	//! selection. Buy is a Buy-mode button and Sell / Sell All are Sell-mode buttons: a hidden
	//! SCR_InputButtonComponent also refuses its keybind, so this is what stops "S" selling a shop
	//! row the player does not even own.
	//!
	//! THIS EXCLUSIVITY IS LOAD-BEARING FOR THE GAMEPAD BINDINGS: OverthrowShopSellAll shares
	//! gamepad "x" with OverthrowShopBuy, which is only safe because BuyButton and SellAllButton are
	//! never visible at the same time. If that ever changes, give Sell All its own pad input.
	protected void RefreshActionButtons()
	{
		bool sellMode = m_eMode == OVT_ShopMenuMode.SELL;

		if(m_wBuyButton) m_wBuyButton.SetVisible(!sellMode);
		if(m_wSellButton) m_wSellButton.SetVisible(sellMode);
		if(m_wSellAllButton) m_wSellAllButton.SetVisible(sellMode);

		bool canSell = false;
		if(sellMode && m_SelectedResource >= 0 && m_bSelectedEnabled && m_iSelectedQuantity > 0)
			canSell = true;

		// A request already went out and the quantities on screen are provisional until it comes back
		// (BUG-100). Disabling the SCR_InputButtonComponent also refuses its keybind, so the guard
		// covers the "S" key and the gamepad face button, not just the mouse.
		if(m_bSellInFlight) canSell = false;

		if(m_SellAction) m_SellAction.SetEnabled(canSell);
		if(m_SellAllAction) m_SellAllAction.SetEnabled(canSell);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] rows Rows to search.
	//! \param[in] id Resource id to look for.
	//! \return True when the id is still browsable in the current tab.
	protected bool ContainsResource(array<ref OVT_ShopBrowserItem> rows, int id)
	{
		if(!rows) return false;

		foreach(OVT_ShopBrowserItem row : rows)
		{
			if(row.m_iResourceId == id) return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id Resource id to look for.
	//! \return The model row for that id, or null.
	protected OVT_ShopBrowserItem FindRow(int id)
	{
		array<ref OVT_ShopBrowserItem> rows = m_Model.GetItems();
		if(!rows) return null;

		foreach(OVT_ShopBrowserItem row : rows)
		{
			if(row.m_iResourceId == id) return row;
		}

		return null;
	}

	//-----------------------------------------------------------------------
	// SELECTION / DETAILS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Selects a row and fills the details pane. Called by the card component on click.
	//! \param[in] res The prefab of the clicked card.
	override void SelectItem(ResourceName res)
	{
		if(!m_wRoot || !m_Economy) return;
		if(res.IsEmpty()) return;
		if(!m_Economy.IsRegisteredResource(res)) return;

		int id = m_Economy.GetInventoryId(res);
		m_SelectedResource = id;
		m_SelectedResourceName = res;

		TextWidget typeName = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedTypeName"));
		TextWidget details = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDetails"));
		TextWidget desc = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDescription"));

		if(m_eMode == OVT_ShopMenuMode.SELL)
		{
			ShowSellDetails(id, res, typeName, details, desc);
		}else{
			ShowBuyDetails(id, res, typeName, details, desc);
		}

		RefreshActionButtons();
	}

	//------------------------------------------------------------------------------------------------
	//! Buy-mode details pane. Deliberately identical to the pre-rework text, prices included.
	protected void ShowBuyDetails(int id, ResourceName res, TextWidget typeName, TextWidget details, TextWidget desc)
	{
		m_bSelectedEnabled = true;
		m_iSelectedQuantity = 0;

		if(!m_Shop) return;

		IEntity shopEntity = m_Shop.GetOwner();
		if(!shopEntity) return;

		int buy, sell, qty, max;

		if(m_Shop.m_bProcurement)
		{
			buy = m_Economy.GetShopBuyPrice(id, m_Shop, shopEntity.GetOrigin(), m_iPlayerID);
			sell = buy;
			qty = 100;
			max = 100;
		}else{
			buy = m_Economy.GetShopBuyPrice(id, m_Shop, shopEntity.GetOrigin(), m_iPlayerID);
			sell = m_Economy.GetSellPrice(id, shopEntity.GetOrigin());
			if(m_Shop.m_ShopType == OVT_ShopType.SHOP_GUNDEALER)
			{
				sell = sell * m_fGunDealerMultiplier;
			}
			qty = m_Shop.GetStock(id);
			OVT_TownData town = m_Shop.GetTown();
			int townID = OVT_Global.GetTowns().GetTownID(town);
			max = m_Economy.GetTownMaxStock(townID, id);
		}

		m_iSelectedQuantity = qty;

		if(m_Economy.IsVehicle(res))
		{
			SCR_EditableVehicleUIInfo info = OVT_PrefabUtils.GetVehicleUIInfo(res);
			if(info)
			{
				if(typeName) typeName.SetText(info.GetName());
				if(desc) desc.SetText(info.GetDescription());
			}else{
				SCR_EditableEntityUIInfo uiinfo = OVT_PrefabUtils.GetEditableUIInfo(res);
				if(uiinfo)
				{
					if(typeName) typeName.SetText(uiinfo.GetName());
					if(desc) desc.SetText(uiinfo.GetDescription());
				}
			}

			if(!details) return;

			if(m_Shop.m_bProcurement)
			{
				details.SetText(OVT_MoneyFormat.FormatMoney(buy));
			}else{
				details.SetText(OVT_MoneyFormat.FormatMoney(buy) + "\n" + qty + " #OVT-Shop_InStock");
			}
		}else{
			UIInfo info = OVT_PrefabUtils.GetItemUIInfo(res);
			if(!info) return;

			if(typeName) typeName.SetText(info.GetName());
			if(details) details.SetText("#OVT-Shop_Buying: " + OVT_MoneyFormat.FormatMoney(buy) + "\n#OVT-Shop_Selling: " + OVT_MoneyFormat.FormatMoney(sell) + "\n" + qty + "/" + max + " #OVT-Shop_InStock");
			if(desc) desc.SetText(info.GetDescription());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Sell-mode details pane: what the player holds, what this shop pays for one, and - when the shop
	//! will not buy it - which rule said no (F9).
	protected void ShowSellDetails(int id, ResourceName res, TextWidget typeName, TextWidget details, TextWidget desc)
	{
		OVT_ShopBrowserItem row = FindRow(id);

		m_iSelectedQuantity = 0;
		m_bSelectedEnabled = false;

		int unitPrice = 0;
		string reasonKey = "";

		if(row)
		{
			m_iSelectedQuantity = row.m_iQuantity;
			m_bSelectedEnabled = row.m_bEnabled;
			unitPrice = row.m_iUnitPrice;
			reasonKey = row.m_sDisabledReasonKey;
		}

		UIInfo info = OVT_PrefabUtils.GetItemUIInfo(res);
		if(info)
		{
			if(typeName) typeName.SetText(info.GetName());
			if(desc) desc.SetText(info.GetDescription());
		}

		if(!details) return;

		string text = "#OVT-Shop_Selling: " + OVT_MoneyFormat.FormatMoney(unitPrice) + "\n" + m_iSelectedQuantity + " #OVT-Shop_Owned";
		if(!m_bSelectedEnabled && reasonKey != "") text = text + "\n" + reasonKey;

		details.SetText(text);
	}

	//------------------------------------------------------------------------------------------------
	//! Empties the details pane. Used when a page has no rows at all.
	protected void ClearDetails()
	{
		if(!m_wRoot) return;

		TextWidget typeName = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedTypeName"));
		TextWidget details = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDetails"));
		TextWidget desc = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDescription"));

		if(typeName) typeName.SetText("");
		if(details) details.SetText("");
		if(desc) desc.SetText("");
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearSelection()
	{
		m_SelectedResource = -1;
		m_SelectedResourceName = ResourceName.Empty;
		m_iSelectedQuantity = 0;
		m_bSelectedEnabled = false;
	}

	//-----------------------------------------------------------------------
	// TRANSACTIONS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Buys one unit of the selected row. Unchanged from before the rework, guarded for mode.
	void Buy(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(m_eMode != OVT_ShopMenuMode.BUY) return;
		if(!m_Shop) return;
		if(m_SelectedResource < 0) return;

		if(!m_Shop.m_bProcurement && m_Shop.GetStock(m_SelectedResource) < 1) return;

		int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(m_sPlayerID);
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;

		int cost = m_Economy.GetShopBuyPrice(m_SelectedResource, m_Shop, m_Shop.GetOwner().GetOrigin(), m_iPlayerID);

		if(!m_Economy.PlayerHasMoney(m_sPlayerID, cost)) return;

		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(player.FindComponent( SCR_InventoryStorageManagerComponent ));
		if(!inventory) return;

		if(m_Shop.m_ShopType == OVT_ShopType.SHOP_VEHICLE)
		{
			// Vehicle buying rides OVT_VehicleRequestComponent, not the shop transaction component
			// (implementation plan D5): it ends in a spawned vehicle and needs parking resolution.
			OVT_VehicleRequestComponent vehicles = OVT_ControllerComponent<OVT_VehicleRequestComponent>.Get();
			if(!vehicles) return;

			vehicles.BuyVehicle(m_Shop, m_SelectedResource);
			CloseLayout();
			return;
		}

		// Item buying rides OVT_ShopTransactionComponent alongside selling (implementation plan D5):
		// both halves share the 30 m gate, the price model and the stock table.
		OVT_ShopTransactionComponent transactions = OVT_ControllerComponent<OVT_ShopTransactionComponent>.Get();
		if(!transactions) return;

		transactions.BuyItems(m_Shop, m_SelectedResource, 1);
		SelectItem(m_SelectedResourceName);

		// No refresh is scheduled here on purpose. The stock decrement redraws the grid when the shop
		// actually reports it (OnShopInventoryChanged), which is both sooner on a host and correct on a
		// client with worse latency than the old fixed 400 ms guess.
	}

	//------------------------------------------------------------------------------------------------
	//! Sells one unit of the selected row.
	void Sell(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		RequestSell(1);
	}

	//------------------------------------------------------------------------------------------------
	//! Sells every copy of the selected row the player is carrying, in ONE request (quality bar Q4).
	//! The quantity sent is the count this menu observed; the server clamps it to what is actually
	//! held and credits only what it managed to delete (D4).
	void SellAll(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		RequestSell(m_iSelectedQuantity);
	}

	//------------------------------------------------------------------------------------------------
	//! Sends one sell request for the selected row.
	//! \param[in] quantity How many to sell.
	protected void RequestSell(int quantity)
	{
		if(m_eMode != OVT_ShopMenuMode.SELL) return;
		if(!m_Shop) return;
		if(m_SelectedResource < 0) return;
		if(!m_bSelectedEnabled) return;
		if(quantity < 1) return;

		// One sale at a time. Dropping the extra click outright is deliberate: queueing it would send a
		// quantity derived from an inventory the player has already sold out of.
		if(m_bSellInFlight) return;

		OVT_ShopTransactionComponent transactions = OVT_ControllerComponent<OVT_ShopTransactionComponent>.Get();
		if(!transactions) return;

		m_bSellInFlight = true;
		RefreshActionButtons();
		GetGame().GetCallqueue().Remove(OnSellTimeout);
		GetGame().GetCallqueue().CallLater(OnSellTimeout, SELL_TIMEOUT_MS, false);

		transactions.SellItems(m_Shop, m_SelectedResource, quantity);
	}

	//------------------------------------------------------------------------------------------------
	//! Re-arms the sell buttons when no result came back. Never invents a toast - the sale may well
	//! have happened and simply lost its reply (BUG-090); all this does is stop the guard becoming a
	//! permanent lock. A refresh is still worth doing, because the inventory has probably changed.
	protected void OnSellTimeout()
	{
		if(!m_bSellInFlight) return;

		m_bSellInFlight = false;
		if(!m_bIsActive) return;

		RefreshActionButtons();
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! What the SERVER did with a sell request. Display and refresh only - money and inventory arrive
	//! through their own broadcasts, never from here.
	//! \param[in] soldCount Items actually sold.
	//! \param[in] totalEarned Money credited.
	//! \param[in] skippedCount Candidates the shop would not buy.
	//! \param[in] result OVT_ShopSellResult value.
	protected void OnSellResult(int soldCount, int totalEarned, int skippedCount, int result)
	{
		// Cleared before the active check so a result that lands as the menu closes cannot leave the
		// guard set for the next visit.
		GetGame().GetCallqueue().Remove(OnSellTimeout);
		m_bSellInFlight = false;

		if(!m_bIsActive) return;

		RefreshActionButtons();

		if(soldCount > 0)
		{
			ShowMessage("#OVT-Shop_SoldItems", soldCount, OVT_MoneyFormat.FormatMoney(totalEarned));
		}
		else if(skippedCount > 0)
		{
			ShowMessage("#OVT-Shop_SkippedItems", skippedCount, "");
		}
		else
		{
			ShowMessage("#OVT-Shop_SoldNothing", 0, "");
		}

		Refresh();

		// A remote client can receive this before the deleted items have replicated away; one late
		// re-read costs nothing and keeps the quantities honest.
		ScheduleRefresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Queues one refresh a moment from now, for state that arrives from the server after the click.
	//! Never changes what was requested - it only redraws, keeping the page and the selection.
	//!
	//! Still used after a SELL: the sold items leave THIS client's inventory by replication, which the
	//! sell result can beat. Shop stock is no longer guessed at - see OnShopInventoryChanged.
	//!
	//! Schedules RefreshPostSell, NOT Refresh. The two schedulers must own SEPARATE callqueue entries:
	//! a sale is always followed by a stock broadcast, so when both queued the same method the 50 ms
	//! stock refresh replaced this 400 ms recheck with one that fires BEFORE the sold items have
	//! replicated out of the local inventory - and then nothing was left pending to correct it.
	protected void ScheduleRefresh()
	{
		GetGame().GetCallqueue().Remove(RefreshPostSell);
		GetGame().GetCallqueue().CallLater(RefreshPostSell, TRANSACTION_RECHECK_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Callqueue trampoline owned by ScheduleRefresh. Exists only so the post-sell recheck has an
	//! identity of its own in the queue; the work is the same Refresh everything else does.
	protected void RefreshPostSell()
	{
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Coalesces a burst of stock changes into one redraw. Remove-before-CallLater keeps exactly one
	//! pending stock callback, so a 40-item restock costs one Refresh, not forty - and because it only
	//! ever touches its own trampoline, coalescing stock events can no longer cancel a post-sell
	//! recheck.
	protected void ScheduleStockRefresh()
	{
		GetGame().GetCallqueue().Remove(RefreshFromStockEvent);
		GetGame().GetCallqueue().CallLater(RefreshFromStockEvent, STOCK_REFRESH_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Callqueue trampoline owned by ScheduleStockRefresh. Same reasoning as RefreshPostSell: a
	//! distinct method is what makes Remove() selective.
	protected void RefreshFromStockEvent()
	{
		Refresh();
	}

	//-----------------------------------------------------------------------
	// MESSAGES
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Shows a transient localized message over the grid.
	//! \param[in] key #OVT- localization key, with %1 / %2 placeholders.
	//! \param[in] param1 First placeholder value.
	//! \param[in] param2 Second placeholder value.
	protected void ShowMessage(string key, int param1, string param2)
	{
		TextWidget message = PrepareMessage();
		if(!message) return;

		message.SetTextFormat(key, param1, param2);

		ArmMessageTimeout();
	}

	//------------------------------------------------------------------------------------------------
	//! Shows a transient message built from an already-resolved localization key with string
	//! parameters - the shape the notification presets use.
	//! \param[in] key Localization key, with %1 / %2 / %3 placeholders.
	//! \param[in] param1 First placeholder value.
	//! \param[in] param2 Second placeholder value.
	//! \param[in] param3 Third placeholder value.
	protected void ShowMessageFormat(string key, string param1, string param2, string param3)
	{
		TextWidget message = PrepareMessage();
		if(!message) return;

		message.SetTextFormat(key, param1, param2, param3);

		ArmMessageTimeout();
	}

	//------------------------------------------------------------------------------------------------
	//! Makes the toast visible at full opacity, cancelling any fade left over from the previous one.
	//! \return The toast widget, or null when the layout has no Message widget.
	protected TextWidget PrepareMessage()
	{
		if(!m_wRoot) return null;

		TextWidget message = TextWidget.Cast(m_wRoot.FindAnyWidget("Message"));
		if(!message) return null;

		// A second result arriving mid-fade must not inherit the fade: stop it, or the new text shows up
		// half-transparent and then vanishes on the old animation's schedule.
		AnimateWidget.StopAnimation(message, WidgetAnimationOpacity);

		message.SetOpacity(1);
		message.SetVisible(true);

		return message;
	}

	//------------------------------------------------------------------------------------------------
	//! (Re)starts the auto-hide countdown. One pending callback, always.
	protected void ArmMessageTimeout()
	{
		GetGame().GetCallqueue().Remove(HideMessage);
		GetGame().GetCallqueue().CallLater(HideMessage, MESSAGE_TIMEOUT_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Hard-clears the toast with no animation. Used at show time (a fresh layout must start blank) and
	//! at close time (an animation must never outlive the widget it is animating).
	protected void ResetMessage()
	{
		if(!m_wRoot) return;

		TextWidget message = TextWidget.Cast(m_wRoot.FindAnyWidget("Message"));
		if(!message) return;

		AnimateWidget.StopAnimation(message, WidgetAnimationOpacity);

		message.SetText("");
		message.SetOpacity(0);
		message.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Fades the toast out and hides it. Called on the timeout.
	protected void HideMessage()
	{
		if(!m_wRoot) return;

		TextWidget message = TextWidget.Cast(m_wRoot.FindAnyWidget("Message"));
		if(!message) return;

		// toggleVisibility = the animation hides the widget when it reaches zero, so nothing has to be
		// queued behind the fade.
		AnimateWidget.Opacity(message, 0, MESSAGE_FADE_SPEED, true);
	}

	//-----------------------------------------------------------------------
	// HELPERS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The character whose inventory Sell mode browses - resolved the same way Buy resolves it, so
	//! possession is handled identically on both sides of the menu.
	//! \return The local player's controlled entity, or the context owner as a fallback.
	protected IEntity GetLocalCharacter()
	{
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(players)
		{
			int playerId = players.GetPlayerIDFromPersistentID(m_sPlayerID);
			if(playerId > 0)
			{
				IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
				if(player) return player;
			}
		}

		return m_Owner;
	}
}
