//------------------------------------------------------------------------------------------------
//! Which of the two focusable columns the player is browsing.
//------------------------------------------------------------------------------------------------
enum EOVT_TransferPane
{
	LIST,
	CART
}

//------------------------------------------------------------------------------------------------
//! One place a transfer can be sent. Both first consumers produce zero or one of these (the
//! occupied vehicle), so the picker is hidden and the destination is either that vehicle or null.
//------------------------------------------------------------------------------------------------
class OVT_TransferDestination : Managed
{
	string m_sId;
	string m_sLabel;
	IEntity m_Entity;
}

//------------------------------------------------------------------------------------------------
//! The shared transfer screen: a categorized list on the left, details over a cart on the right, a
//! destination picker under the cart, and one Accept.
//!
//! The base owns the layout, the widget lookups, the header, the tabs, focus, the input bindings and
//! the cart; a consumer overrides eight small hooks and writes no widget code. Nothing here may
//! assume a resource ledger, a volume cap, an Export mode or a second destination - the
//! PREFAB/TEXTURE image-kind pair is the whole "must show items and resources" story.
//------------------------------------------------------------------------------------------------
class OVT_TransferContext : OVT_TabHostContext
{
	[Attribute("{6A8E2C1000000002}UI/Layouts/Menu/TransferMenu/TransferMenu_Row.layout", uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout for one list row", params: "layout")]
	ResourceName m_RowLayout;

	[Attribute("{6A8E2C1000000003}UI/Layouts/Menu/TransferMenu/TransferMenu_CartLine.layout", uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout for one cart line", params: "layout")]
	ResourceName m_CartLineLayout;

	[Attribute("{6A7C4E1C77B31E40}UI/Layouts/Menu/ShopMenu/ShopMenu_Tab.layout", uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout for one category tab", params: "layout")]
	ResourceName m_TabLayout;

	//! How long an in-menu message stays up before it starts to fade.
	protected const int MESSAGE_TIMEOUT_MS = 4000;

	//! Opacity units per second for the toast fade-out - 2.0 is a half-second fade from full.
	protected const float MESSAGE_FADE_SPEED = 2.0;

	//! Coalescing delay for a consumer that has no change invoker and must re-read after a request.
	protected const int TRANSACTION_RECHECK_MS = 400;

	protected const int QTY_SMALL = 1;
	protected const int QTY_LARGE = 10;

	//! Accent orange, the selected colour used across Overthrow menus.
	protected const int COLOR_SELECTED = 0xFFC26414;

	protected int m_iMode;
	protected int m_iTab = OVT_TransferListModel.CATEGORY_ALL;
	protected int m_iDestination;

	protected EOVT_TransferPane m_ePane = EOVT_TransferPane.LIST;
	protected int m_iListIndex;
	protected int m_iCartIndex;

	protected bool m_bMenuNavListening;
	protected bool m_bMessagePersistent;
	protected bool m_bRebuildingDestinations;

	//! Items currently in the picker. Below two destinations the picker is never touched at all.
	protected int m_iDestinationItems;
	protected string m_sMessageKey;

	protected ref OVT_TransferListModel m_Model = new OVT_TransferListModel();
	protected ref OVT_TransferCartModel m_Cart = new OVT_TransferCartModel();

	protected ref array<int> m_aModes = new array<int>();
	protected ref array<string> m_aModeLabels = new array<string>();
	protected ref array<ref OVT_TransferDestination> m_aDestinations = new array<ref OVT_TransferDestination>();

	//! Tab ids the instantiated tab widgets represent, in order. Lets a refresh tell a selection
	//! change (repaint) from a tab-set change (rebuild).
	protected ref array<int> m_aTabOrder = new array<int>();

	//! Row and cart-line widgets currently drawn, in draw order. Cleared before their widgets are
	//! destroyed, so no entry can ever dangle.
	protected ref array<Widget> m_aRowWidgets = new array<Widget>();
	protected ref array<Widget> m_aCartWidgets = new array<Widget>();

	protected Widget m_wHeaderRow;
	protected Widget m_wTabs;
	protected Widget m_wMode1Button;
	protected Widget m_wMode2Button;
	protected Widget m_wPrevCategoryButton;
	protected Widget m_wNextCategoryButton;
	protected Widget m_wListRows;
	protected Widget m_wCartLines;
	protected Widget m_wCartEmptyLabel;
	protected Widget m_wDestinationSpin;
	protected Widget m_wQty1Button;
	protected Widget m_wQty10Button;
	protected Widget m_wQtyAllButton;
	protected Widget m_wAcceptButton;
	protected Widget m_wCloseButton;

	protected SCR_InputButtonComponent m_Mode1Action;
	protected SCR_InputButtonComponent m_Mode2Action;
	protected SCR_InputButtonComponent m_PrevCategoryAction;
	protected SCR_InputButtonComponent m_NextCategoryAction;
	protected SCR_InputButtonComponent m_Qty1Action;
	protected SCR_InputButtonComponent m_Qty10Action;
	protected SCR_InputButtonComponent m_QtyAllAction;
	protected SCR_InputButtonComponent m_AcceptAction;
	protected SCR_InputButtonComponent m_CloseAction;
	protected SCR_SpinBoxComponent m_DestinationSpinBox;

	//-----------------------------------------------------------------------
	// HOOKS - the closed list. A consumer implementing these writes no widget code.
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Fills the list model with everything browsable in a mode.
	//! \param[in] mode The mode id currently selected.
	//! \param[in] model The model to fill. Already cleared; sorted by the base afterwards.
	void BuildEntries(int mode, OVT_TransferListModel model)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Declares the consumer's modes. Fewer than two hides the mode toggle and its keybinds.
	//! \param[out] modes Receives the mode ids, in button order.
	//! \param[out] labelKeys Receives one #OVT- label key per mode. The first one also titles the screen.
	void BuildModes(out array<int> modes, out array<string> labelKeys)
	{
		modes.Clear();
		labelKeys.Clear();

		modes.Insert(0);
		labelKeys.Insert("");
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] categoryId A consumer-defined category id. Never CATEGORY_ALL.
	//! \return The #OVT- label key for that category's tab.
	string GetCategoryLabelKey(int categoryId)
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Declares where a transfer may be sent. Zero or one hides the picker.
	//! \param[out] dests Receives the destinations, in picker order.
	void BuildDestinations(out array<ref OVT_TransferDestination> dests)
	{
		dests.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Fills the details panel for the selected row.
	//! \param[in] entry The selected row.
	//! \param[out] name Heading line.
	//! \param[out] value Value line.
	//! \param[out] body Free text under it. Empty falls back to the entry's disabled reason.
	void FillDetails(OVT_TransferEntry entry, out string name, out string value, out string body)
	{
		name = entry.m_sDisplayName;
		value = FormatValue(entry.m_iValue, entry.m_eValueKind);
		body = "";
	}

	//------------------------------------------------------------------------------------------------
	//! Commits the order. One existing request per line - this feature adds no RPC.
	//! \param[in] lines The cart's live line array. The base clears the cart immediately afterwards,
	//! so a consumer must not keep the array.
	//! \param[in] dest The chosen destination, or null when the consumer offers none.
	void OnAccept(array<ref OVT_TransferCartLine> lines, OVT_TransferDestination dest)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] mode The mode id currently selected.
	//! \return False to retire the "Add all" button (and, with it, its keybind) in that mode.
	bool IsAddAllAllowed(int mode)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] lines The cart's lines.
	//! \param[in] dest The chosen destination, or null.
	//! \return "" when the cart may be committed, otherwise an #OVT- key naming why it may not.
	string ValidateCart(array<ref OVT_TransferCartLine> lines, OVT_TransferDestination dest)
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The running total under the cart. Not a hook - a virtual with a working default that neither
	//! first consumer overrides. Empty on an empty cart: CartEmptyLabel is the only empty-state message.
	//! \return Already-resolved text, or an #OVT- key the widget resolves at draw time.
	string GetSummaryText()
	{
		int lines = m_Cart.Count();
		if(lines == 0) return "";

		if(GetCartValueKind() == EOVT_TransferValueKind.PRICE)
		{
			return WidgetManager.Translate("#OVT-Transfer_SummaryPrice",
				OVT_MoneyFormat.FormatMoney(m_Cart.TotalValue()), lines.ToString());
		}

		return WidgetManager.Translate("#OVT-Transfer_SummaryQuantity",
			m_Cart.TotalQuantity().ToString(), lines.ToString());
	}

	//-----------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	override void PostInit()
	{
		if(SCR_Global.IsEditMode()) return;
		if(m_Economy) m_Economy.m_OnPlayerMoneyChanged.Insert(OnPlayerMoneyChanged);
	}

	//------------------------------------------------------------------------------------------------
	override void OnShow()
	{
		m_iTab = OVT_TransferListModel.CATEGORY_ALL;
		m_iDestination = 0;
		m_ePane = EOVT_TransferPane.LIST;
		m_iListIndex = 0;
		m_iCartIndex = 0;
		m_bMessagePersistent = false;
		m_sMessageKey = "";
		m_iDestinationItems = 0;
		m_Cart.Clear();

		ResolveModes();

		WireWidgets();
		AddMenuNavListeners();

		Refresh();

		// Refresh() only restores focus that was already in a pane. On arrival it is still on the
		// vehicle menu's button - ShowContext() runs before that menu's CloseLayout() - so without
		// this a pad opens the screen with nothing focused.
		RestoreFocus();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes exactly what OnShow inserted. The row, cart-line and tab widgets die with the layout,
	//! but their invoker subscriptions and queued activations are torn down explicitly so a rebuild
	//! and a close behave identically.
	override void OnClose()
	{
		GetGame().GetCallqueue().Remove(HideMessage);
		GetGame().GetCallqueue().Remove(Refresh);

		ResetMessage();

		RemoveMenuNavListeners();

		ClearRows();
		ClearCartLines();
		ClearTabs();

		if(m_Mode1Action) m_Mode1Action.m_OnActivated.Remove(Mode1);
		if(m_Mode2Action) m_Mode2Action.m_OnActivated.Remove(Mode2);
		if(m_PrevCategoryAction) m_PrevCategoryAction.m_OnActivated.Remove(PreviousCategory);
		if(m_NextCategoryAction) m_NextCategoryAction.m_OnActivated.Remove(NextCategory);
		if(m_Qty1Action) m_Qty1Action.m_OnActivated.Remove(QtyOne);
		if(m_Qty10Action) m_Qty10Action.m_OnActivated.Remove(QtyTen);
		if(m_QtyAllAction) m_QtyAllAction.m_OnActivated.Remove(QtyAll);
		if(m_AcceptAction)
		{
			m_AcceptAction.m_OnActivated.Remove(Accept);
			m_AcceptAction.m_OnFocus.Remove(OnAcceptFocus);
			m_AcceptAction.m_OnFocusLost.Remove(OnAcceptFocusLost);
		}

		if(m_CloseAction)
		{
			m_CloseAction.m_OnActivated.Remove(CloseLayout);
			m_CloseAction.m_OnFocus.Remove(OnCloseFocus);
			m_CloseAction.m_OnFocusLost.Remove(OnCloseFocusLost);
		}

		if(m_DestinationSpinBox && m_DestinationSpinBox.m_OnChanged)
			m_DestinationSpinBox.m_OnChanged.Remove(OnDestinationChanged);

		m_Mode1Action = null;
		m_Mode2Action = null;
		m_PrevCategoryAction = null;
		m_NextCategoryAction = null;
		m_Qty1Action = null;
		m_Qty10Action = null;
		m_QtyAllAction = null;
		m_AcceptAction = null;
		m_CloseAction = null;
		m_DestinationSpinBox = null;

		m_wHeaderRow = null;
		m_wTabs = null;
		m_wMode1Button = null;
		m_wMode2Button = null;
		m_wPrevCategoryButton = null;
		m_wNextCategoryButton = null;
		m_wListRows = null;
		m_wCartLines = null;
		m_wCartEmptyLabel = null;
		m_wDestinationSpin = null;
		m_wQty1Button = null;
		m_wQty10Button = null;
		m_wQtyAllButton = null;
		m_wAcceptButton = null;
		m_wCloseButton = null;

		m_aDestinations.Clear();
		m_Model.Clear();
		m_Cart.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Finds and wires every button in the layout. Every lookup is null-guarded so a stale layout
	//! degrades to a missing button rather than a script error.
	protected void WireWidgets()
	{
		if(!m_wRoot) return;

		m_wHeaderRow = m_wRoot.FindAnyWidget("HeaderRow");
		m_wTabs = m_wRoot.FindAnyWidget("Tabs");
		m_wListRows = m_wRoot.FindAnyWidget("ListRows");
		m_wCartLines = m_wRoot.FindAnyWidget("CartLines");
		m_wCartEmptyLabel = m_wRoot.FindAnyWidget("CartEmptyLabel");

		m_wMode1Button = m_wRoot.FindAnyWidget("Mode1Button");
		if(m_wMode1Button)
		{
			m_Mode1Action = SCR_InputButtonComponent.Cast(m_wMode1Button.FindHandler(SCR_InputButtonComponent));
			if(m_Mode1Action) m_Mode1Action.m_OnActivated.Insert(Mode1);
		}

		m_wMode2Button = m_wRoot.FindAnyWidget("Mode2Button");
		if(m_wMode2Button)
		{
			m_Mode2Action = SCR_InputButtonComponent.Cast(m_wMode2Button.FindHandler(SCR_InputButtonComponent));
			if(m_Mode2Action) m_Mode2Action.m_OnActivated.Insert(Mode2);
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

		m_wQty1Button = m_wRoot.FindAnyWidget("Qty1Button");
		if(m_wQty1Button)
		{
			m_Qty1Action = SCR_InputButtonComponent.Cast(m_wQty1Button.FindHandler(SCR_InputButtonComponent));
			if(m_Qty1Action) m_Qty1Action.m_OnActivated.Insert(QtyOne);
		}

		m_wQty10Button = m_wRoot.FindAnyWidget("Qty10Button");
		if(m_wQty10Button)
		{
			m_Qty10Action = SCR_InputButtonComponent.Cast(m_wQty10Button.FindHandler(SCR_InputButtonComponent));
			if(m_Qty10Action) m_Qty10Action.m_OnActivated.Insert(QtyTen);
		}

		m_wQtyAllButton = m_wRoot.FindAnyWidget("QtyAllButton");
		if(m_wQtyAllButton)
		{
			m_QtyAllAction = SCR_InputButtonComponent.Cast(m_wQtyAllButton.FindHandler(SCR_InputButtonComponent));
			if(m_QtyAllAction) m_QtyAllAction.m_OnActivated.Insert(QtyAll);
		}

		// Accept has its own action (OverthrowTransferAccept), so it cannot collide with MenuSelect -
		// OnInput does not check focus, which is why it must never be bound to MenuSelect itself.
		// The component registers that action's listener; do NOT add a second one here.
		m_wAcceptButton = m_wRoot.FindAnyWidget("AcceptButton");
		if(m_wAcceptButton)
		{
			m_AcceptAction = SCR_InputButtonComponent.Cast(m_wAcceptButton.FindHandler(SCR_InputButtonComponent));
			if(m_AcceptAction)
			{
				m_AcceptAction.m_OnActivated.Insert(Accept);
				m_AcceptAction.m_OnFocus.Insert(OnAcceptFocus);
				m_AcceptAction.m_OnFocusLost.Insert(OnAcceptFocusLost);
			}
		}

		m_wCloseButton = m_wRoot.FindAnyWidget("CloseButton");
		if(m_wCloseButton)
		{
			m_CloseAction = SCR_InputButtonComponent.Cast(m_wCloseButton.FindHandler(SCR_InputButtonComponent));
			if(m_CloseAction)
			{
				m_CloseAction.m_OnActivated.Insert(CloseLayout);
				m_CloseAction.m_OnFocus.Insert(OnCloseFocus);
				m_CloseAction.m_OnFocusLost.Insert(OnCloseFocusLost);
			}
		}

		m_wDestinationSpin = m_wRoot.FindAnyWidget("DestinationSpin");
		if(m_wDestinationSpin)
		{
			m_DestinationSpinBox = SCR_SpinBoxComponent.Cast(m_wDestinationSpin.FindHandler(SCR_SpinBoxComponent));
			if(m_DestinationSpinBox && m_DestinationSpinBox.m_OnChanged)
				m_DestinationSpinBox.m_OnChanged.Insert(OnDestinationChanged);
		}

		ResetMessage();
	}

	//-----------------------------------------------------------------------
	// REFRESH
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Rebuilds everything from current state. Safe to call from an invoker.
	void Refresh()
	{
		if(!m_bIsActive) return;
		if(!m_wRoot) return;

		// Decided BEFORE the rebuild: a player parked on the picker or on Accept must keep that focus,
		// while a player whose row is about to be destroyed must get it back (B1).
		bool restoreFocus = FocusIsInPanes();

		// Destroying the focused row fires stray focus events; the player's column is not up for grabs.
		EOVT_TransferPane pane = m_ePane;

		BuildList();

		RefreshTitle();
		RefreshMoney();
		RefreshHeader();
		RefreshList();
		RefreshDestinations();
		RefreshCart();

		m_ePane = pane;
		NormalizePane();
		PaintSelection();
		RefreshDetails();
		RefreshActionButtons();
		RefreshCheckout();

		if(restoreFocus) RestoreFocus();
	}

	//------------------------------------------------------------------------------------------------
	//! Queues one coalesced redraw, for a consumer with no change invoker of its own.
	protected void ScheduleRefresh()
	{
		GetGame().GetCallqueue().Remove(Refresh);
		GetGame().GetCallqueue().CallLater(Refresh, TRANSACTION_RECHECK_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the row set and reconciles the cart against it.
	protected void BuildList()
	{
		m_Model.Clear();
		BuildEntries(m_iMode, m_Model);
		m_Model.SortByDisplayName();

		// Someone else may have emptied the source since the cart was built.
		m_Cart.Reconcile(m_Model);

		// A tab the new row set no longer populates would leave the player on an empty list.
		if(m_iTab != OVT_TransferListModel.CATEGORY_ALL && !m_Model.HasCategory(m_iTab))
			m_iTab = OVT_TransferListModel.CATEGORY_ALL;
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshTitle()
	{
		TextWidget title = TextWidget.Cast(m_wRoot.FindAnyWidget("Title"));
		if(!title) return;

		string key = GetModeLabel(m_iMode);
		if(key == "") key = "#OVT-Transfer_Title";

		title.SetText(key);
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshMoney()
	{
		TextWidget money = TextWidget.Cast(m_wRoot.FindAnyWidget("PlayerMoney"));
		if(!money) return;
		if(!m_Economy) return;

		money.SetText(OVT_MoneyFormat.FormatMoney(m_Economy.GetPlayerMoney(m_sPlayerID)));
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the mode toggle and the tab row, and hides the header entirely when it would be empty.
	protected void RefreshHeader()
	{
		bool showModes = m_aModes.Count() >= 2;

		if(m_wMode1Button) m_wMode1Button.SetVisible(showModes);
		if(m_wMode2Button) m_wMode2Button.SetVisible(showModes);

		if(showModes)
		{
			if(m_Mode1Action && m_aModeLabels[0] != "") m_Mode1Action.SetLabel(m_aModeLabels[0]);
			if(m_Mode2Action && m_aModeLabels[1] != "") m_Mode2Action.SetLabel(m_aModeLabels[1]);

			ApplyToggleVisual(m_wMode1Button, m_Mode1Action, m_iMode == m_aModes[0]);
			ApplyToggleVisual(m_wMode2Button, m_Mode2Action, m_iMode == m_aModes[1]);
		}

		bool showTabs = RefreshTabs();

		// The steppers are meaningless without a tab row, and a hidden SCR_InputButtonComponent also
		// refuses its keybind - so hiding them retires the shortcut too.
		if(m_wPrevCategoryButton) m_wPrevCategoryButton.SetVisible(showTabs);
		if(m_wNextCategoryButton) m_wNextCategoryButton.SetVisible(showTabs);

		if(m_wHeaderRow) m_wHeaderRow.SetVisible(showModes || showTabs);
	}

	//------------------------------------------------------------------------------------------------
	//! Draws the list column from the current tab's slice of the model.
	protected void RefreshList()
	{
		ClearRows();

		if(!m_wListRows) return;

		array<ref OVT_TransferEntry> entries = new array<ref OVT_TransferEntry>();
		m_Model.FilterByCategory(m_iTab, entries);

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if(!workspace || m_RowLayout.IsEmpty()) return;

		int index = 0;

		foreach(OVT_TransferEntry entry : entries)
		{
			Widget w = workspace.CreateWidgets(m_RowLayout, m_wListRows);
			if(!w) continue;

			OVT_TransferRowComponent row = OVT_TransferRowComponent.Cast(w.FindHandler(OVT_TransferRowComponent));
			if(!row)
			{
				w.RemoveFromHierarchy();
				continue;
			}

			row.Init(entry, index, this, index == m_iListIndex);
			m_aRowWidgets.Insert(w);
			index++;
		}

		// Clamped against the widgets that exist, not the entries: every downstream consumer indexes
		// m_aRowWidgets, which is shorter whenever a row failed to instantiate.
		if(m_iListIndex >= m_aRowWidgets.Count()) m_iListIndex = m_aRowWidgets.Count() - 1;
		if(m_iListIndex < 0) m_iListIndex = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Draws the cart column.
	protected void RefreshCart()
	{
		ClearCartLines();

		array<ref OVT_TransferCartLine> lines = m_Cart.GetLines();

		if(m_iCartIndex >= lines.Count()) m_iCartIndex = lines.Count() - 1;
		if(m_iCartIndex < 0) m_iCartIndex = 0;

		if(m_wCartEmptyLabel) m_wCartEmptyLabel.SetVisible(lines.IsEmpty());

		if(!m_wCartLines) return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if(!workspace || m_CartLineLayout.IsEmpty()) return;

		int index = 0;

		foreach(OVT_TransferCartLine line : lines)
		{
			Widget w = workspace.CreateWidgets(m_CartLineLayout, m_wCartLines);
			if(!w) continue;

			OVT_TransferCartLineComponent widget = OVT_TransferCartLineComponent.Cast(w.FindHandler(OVT_TransferCartLineComponent));
			if(!widget)
			{
				w.RemoveFromHierarchy();
				continue;
			}

			widget.Init(line, index, this, index == m_iCartIndex);
			m_aCartWidgets.Insert(w);
			index++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the destination picker. Hidden below two destinations, which is both first consumers.
	protected void RefreshDestinations()
	{
		array<ref OVT_TransferDestination> dests = new array<ref OVT_TransferDestination>();
		BuildDestinations(dests);

		m_aDestinations.Clear();
		foreach(OVT_TransferDestination dest : dests)
		{
			m_aDestinations.Insert(dest);
		}

		if(m_iDestination >= m_aDestinations.Count()) m_iDestination = m_aDestinations.Count() - 1;
		if(m_iDestination < 0) m_iDestination = 0;

		bool show = m_aDestinations.Count() > 1;
		if(m_wDestinationSpin) m_wDestinationSpin.SetVisible(show);

		if(!m_DestinationSpinBox) return;
		if(!show && m_iDestinationItems == 0) return;

		// ClearAll / AddItem both re-run SetInitialState, which fires m_OnChanged; the guard keeps
		// that from re-entering the refresh that is building the picker.
		m_bRebuildingDestinations = true;

		m_DestinationSpinBox.ClearAll();
		m_iDestinationItems = 0;

		if(show)
		{
			foreach(OVT_TransferDestination dest : m_aDestinations)
			{
				m_DestinationSpinBox.AddItem(dest.m_sLabel);
				m_iDestinationItems++;
			}

			m_DestinationSpinBox.SetCurrentItem(m_iDestination, false, false, false);
		}

		m_bRebuildingDestinations = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Draws the details panel for whatever is selected, in either pane.
	protected void RefreshDetails()
	{
		if(!m_wRoot) return;

		TextWidget nameWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("DetailsName"));
		TextWidget valueWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("DetailsValue"));
		TextWidget bodyWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("DetailsText"));
		ItemPreviewWidget preview = ItemPreviewWidget.Cast(m_wRoot.FindAnyWidget("DetailsPreview"));
		ImageWidget image = ImageWidget.Cast(m_wRoot.FindAnyWidget("DetailsImage"));

		OVT_TransferEntry entry = GetSelectedEntry();

		if(!entry)
		{
			if(nameWidget) nameWidget.SetText("");
			if(valueWidget) valueWidget.SetText("");
			if(bodyWidget) bodyWidget.SetText("");
			if(preview) preview.SetVisible(false);
			if(image) image.SetVisible(false);
			return;
		}

		string name;
		string value;
		string body;
		FillDetails(entry, name, value, body);

		if(body == "" && !entry.m_bEnabled) body = entry.m_sDisabledReasonKey;

		if(nameWidget) nameWidget.SetText(name);
		if(valueWidget) valueWidget.SetText(value);
		if(bodyWidget) bodyWidget.SetText(body);

		// Exactly one image widget per entry kind - the whole "items and resources" story.
		bool isPrefab = entry.m_eImageKind == EOVT_TransferImageKind.PREFAB;

		if(preview) preview.SetVisible(isPrefab);
		if(image) image.SetVisible(!isPrefab);

		if(entry.m_sImage.IsEmpty()) return;

		if(isPrefab)
		{
			if(!preview) return;

			ChimeraWorld world = GetGame().GetWorld();
			if(!world) return;

			ItemPreviewManagerEntity manager = world.GetItemPreviewManager();
			if(!manager) return;

			preview.SetResolutionScale(1, 1);
			manager.SetPreviewItemFromPrefab(preview, entry.m_sImage);
			return;
		}

		if(!image) return;

		if(image.LoadImageTexture(0, entry.m_sImage))
			image.SetImage(0);
	}

	//------------------------------------------------------------------------------------------------
	//! Three widgets, three actions, two label sets. "Add all" is retired with SetVisible(false),
	//! which also refuses its keybind; "Remove all" stays visible in every mode because removing a
	//! cart line is always meaningful.
	protected void RefreshActionButtons()
	{
		bool listEmpty = m_aRowWidgets.IsEmpty();
		bool cartEmpty = m_aCartWidgets.IsEmpty();

		if(listEmpty && cartEmpty)
		{
			if(m_wQty1Button) m_wQty1Button.SetVisible(false);
			if(m_wQty10Button) m_wQty10Button.SetVisible(false);
			if(m_wQtyAllButton) m_wQtyAllButton.SetVisible(false);
			return;
		}

		bool removing = m_ePane == EOVT_TransferPane.CART;

		if(m_wQty1Button) m_wQty1Button.SetVisible(true);
		if(m_wQty10Button) m_wQty10Button.SetVisible(true);

		if(removing)
		{
			if(m_Qty1Action) m_Qty1Action.SetLabel("#OVT-Transfer_Remove1");
			if(m_Qty10Action) m_Qty10Action.SetLabel("#OVT-Transfer_Remove10");
			if(m_QtyAllAction) m_QtyAllAction.SetLabel("#OVT-Transfer_RemoveAll");

			if(m_wQtyAllButton) m_wQtyAllButton.SetVisible(true);

			SetQuantityButtonsEnabled(true);
			return;
		}

		if(m_Qty1Action) m_Qty1Action.SetLabel("#OVT-Transfer_Add1");
		if(m_Qty10Action) m_Qty10Action.SetLabel("#OVT-Transfer_Add10");
		if(m_QtyAllAction) m_QtyAllAction.SetLabel("#OVT-Transfer_AddAll");

		if(m_wQtyAllButton) m_wQtyAllButton.SetVisible(IsAddAllAllowed(m_iMode));

		OVT_TransferEntry entry = GetListEntry(m_iListIndex);

		bool canAdd = false;
		if(entry && entry.m_bEnabled && entry.m_iMaxQuantity > 0) canAdd = true;

		SetQuantityButtonsEnabled(canAdd);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] enabled False greys the three quantity buttons, which also refuses their keybinds.
	protected void SetQuantityButtonsEnabled(bool enabled)
	{
		if(m_Qty1Action) m_Qty1Action.SetEnabled(enabled);
		if(m_Qty10Action) m_Qty10Action.SetEnabled(enabled);
		if(m_QtyAllAction) m_QtyAllAction.SetEnabled(enabled);
	}

	//------------------------------------------------------------------------------------------------
	//! Running total, Accept's availability, and the standing reason when the cart cannot be committed.
	protected void RefreshCheckout()
	{
		if(!m_wRoot) return;

		TextWidget summary = TextWidget.Cast(m_wRoot.FindAnyWidget("SummaryText"));
		if(summary) summary.SetText(GetSummaryText());

		string reason = "";
		if(m_Cart.Count() > 0)
			reason = ValidateCart(m_Cart.GetLines(), GetSelectedDestination());

		bool canAccept = m_Cart.Count() > 0 && reason == "";
		if(m_AcceptAction) m_AcceptAction.SetEnabled(canAccept);

		if(reason != "")
		{
			ShowPersistentMessage(reason);
			return;
		}

		if(m_bMessagePersistent) ResetMessage();
	}

	//-----------------------------------------------------------------------
	// TABS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \return True when a tab row is shown.
	protected bool RefreshTabs()
	{
		if(!m_wTabs) return false;

		array<int> tabs = new array<int>();
		BuildTabOrder(tabs);

		if(tabs.Count() < 2)
		{
			ClearTabs();
			m_wTabs.SetVisible(false);
			return false;
		}

		m_wTabs.SetVisible(true);

		// THE TAB WIDGETS ARE ONLY REBUILT WHEN THE TAB SET ITSELF CHANGES. Picking a tab changes
		// which tab is selected, never which tabs exist, so the widgets survive - which is what keeps
		// gamepad focus where the player put it.
		if(!TabOrderMatches(tabs)) RebuildTabs(tabs);

		UpdateTabSelection();

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The tabs the current model earns: ALL followed by every populated category, or nothing at all
	//! below two populated categories.
	//! \param[out] tabs Receives the tab order. Cleared first; empty means "no tab row".
	protected void BuildTabOrder(out array<int> tabs)
	{
		if(!tabs) return;

		tabs.Clear();

		array<int> categories = new array<int>();
		m_Model.GetPopulatedCategories(categories);

		if(categories.Count() < 2) return;

		tabs.Insert(OVT_TransferListModel.CATEGORY_ALL);

		foreach(int category : categories)
		{
			tabs.Insert(category);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] tabs The tab order the model now wants.
	//! \return True when the instantiated tabs are already exactly that.
	protected bool TabOrderMatches(array<int> tabs)
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
	protected void ClearTabs()
	{
		m_aTabOrder.Clear();

		if(!m_wTabs) return;

		while(m_wTabs.GetChildren())
			m_wTabs.RemoveChild(m_wTabs.GetChildren());
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] tabs The tab order to build.
	protected void RebuildTabs(array<int> tabs)
	{
		ClearTabs();

		if(!tabs) return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if(!workspace || m_TabLayout.IsEmpty()) return;

		foreach(int tabId : tabs)
		{
			if(CreateTab(workspace, tabId)) m_aTabOrder.Insert(tabId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Instantiates one tab into the tab row and wires its click.
	//! \param[in] workspace The widget workspace.
	//! \param[in] tabId The tab's id. CATEGORY_ALL is the always-present "no filter" tab.
	//! \return True when the tab was created and wired.
	protected bool CreateTab(WorkspaceWidget workspace, int tabId)
	{
		if(!workspace || !m_wTabs) return false;

		Widget w = workspace.CreateWidgets(m_TabLayout, m_wTabs);
		if(!w) return false;

		// The shared tab component owns the click wiring, its one-tick deferral and the selected look.
		OVT_ShopMenuTabComponent tab = OVT_ShopMenuTabComponent.Cast(w.FindHandler(OVT_ShopMenuTabComponent));
		if(!tab)
		{
			w.RemoveFromHierarchy();
			return false;
		}

		tab.Init(tabId, GetTabLabelKey(tabId), this, tabId == m_iTab);

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
			if(tab) tab.SetSelected(tab.GetTabId() == m_iTab);

			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] tabId A tab id, possibly CATEGORY_ALL.
	//! \return The label key to draw on that tab.
	protected string GetTabLabelKey(int tabId)
	{
		if(tabId == OVT_TransferListModel.CATEGORY_ALL) return "#OVT-Transfer_CategoryAll";

		return GetCategoryLabelKey(tabId);
	}

	//------------------------------------------------------------------------------------------------
	//! Switches the browsed category.
	//! \param[in] tabId The tab that was picked.
	override void SelectTabId(int tabId)
	{
		if(m_iTab == tabId) return;

		m_iTab = tabId;
		m_iListIndex = 0;
		m_ePane = EOVT_TransferPane.LIST;

		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] tabId A tab id.
	//! \return True when it is the active tab.
	override bool IsTabIdActive(int tabId)
	{
		return m_iTab == tabId;
	}

	//------------------------------------------------------------------------------------------------
	//! Steps through the visible tabs, wrapping at both ends, so a controller never has to point at
	//! an individual tab.
	//! \param[in] delta -1 for the previous tab, +1 for the next.
	void CycleTab(int delta)
	{
		array<int> tabs = new array<int>();
		BuildTabOrder(tabs);

		if(tabs.Count() < 2) return;

		int index = tabs.Find(m_iTab);
		if(index < 0) index = 0;

		index = index + delta;
		if(index < 0) index = tabs.Count() - 1;
		if(index >= tabs.Count()) index = 0;

		SelectTabId(tabs[index]);
	}

	//-----------------------------------------------------------------------
	// MODES
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Reads the consumer's modes once per show and guarantees at least one.
	protected void ResolveModes()
	{
		array<int> modes = new array<int>();
		array<string> labels = new array<string>();
		BuildModes(modes, labels);

		m_aModes.Clear();
		m_aModeLabels.Clear();

		foreach(int mode : modes)
		{
			m_aModes.Insert(mode);
		}

		if(m_aModes.IsEmpty()) m_aModes.Insert(0);

		for(int i = 0; i < m_aModes.Count(); i++)
		{
			if(i < labels.Count())
			{
				m_aModeLabels.Insert(labels[i]);
			}else{
				m_aModeLabels.Insert("");
			}
		}

		m_iMode = m_aModes[0];
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] mode A mode id.
	//! \return That mode's label key, or "" when it has none.
	protected string GetModeLabel(int mode)
	{
		int index = m_aModes.Find(mode);
		if(index < 0) return "";
		if(index >= m_aModeLabels.Count()) return "";

		return m_aModeLabels[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Switches the browsed mode. The cart is not cleared: Reconcile drops any line the new mode's
	//! row set no longer offers.
	//! \param[in] mode The mode to switch to.
	void SetMode(int mode)
	{
		if(m_iMode == mode) return;
		if(m_aModes.Find(mode) < 0) return;

		m_iMode = mode;
		m_iTab = OVT_TransferListModel.CATEGORY_ALL;
		m_iListIndex = 0;
		m_ePane = EOVT_TransferPane.LIST;

		Refresh();
	}

	//-----------------------------------------------------------------------
	// SELECTION AND FOCUS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! A list row took focus or was clicked.
	//! \param[in] index The row's index in the drawn list.
	void SelectListIndex(int index)
	{
		if(!m_bIsActive || !m_wRoot) return;
		if(index < 0) return;
		if(index >= m_aRowWidgets.Count()) return;
		if(m_ePane == EOVT_TransferPane.LIST && m_iListIndex == index) return;

		m_ePane = EOVT_TransferPane.LIST;
		m_iListIndex = index;

		PaintSelection();
		RefreshDetails();
		RefreshActionButtons();
	}

	//------------------------------------------------------------------------------------------------
	//! A cart line took focus or was clicked. This is what flips the buttons to Remove.
	//! \param[in] index The line's index in the cart.
	void SelectCartIndex(int index)
	{
		if(!m_bIsActive || !m_wRoot) return;
		if(index < 0) return;
		if(index >= m_aCartWidgets.Count()) return;
		if(m_ePane == EOVT_TransferPane.CART && m_iCartIndex == index) return;

		m_ePane = EOVT_TransferPane.CART;
		m_iCartIndex = index;

		PaintSelection();
		RefreshDetails();
		RefreshActionButtons();
	}

	//------------------------------------------------------------------------------------------------
	//! Repaints the selected look on both columns.
	protected void PaintSelection()
	{
		for(int i = 0; i < m_aRowWidgets.Count(); i++)
		{
			OVT_TransferRowComponent row = GetRowComponent(i);
			if(row) row.SetSelected(i == m_iListIndex);
		}

		for(int i = 0; i < m_aCartWidgets.Count(); i++)
		{
			OVT_TransferCartLineComponent line = GetCartComponent(i);
			if(line) line.SetSelected(i == m_iCartIndex);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Forces the remembered pane onto one that actually has rows, so the label set on the quantity
	//! buttons can never disagree with what a press would do.
	protected void NormalizePane()
	{
		if(m_ePane == EOVT_TransferPane.CART && m_aCartWidgets.IsEmpty())
			m_ePane = EOVT_TransferPane.LIST;

		if(m_ePane == EOVT_TransferPane.LIST && m_aRowWidgets.IsEmpty() && !m_aCartWidgets.IsEmpty())
			m_ePane = EOVT_TransferPane.CART;
	}

	//------------------------------------------------------------------------------------------------
	//! MenuLeft / MenuRight listeners, added on show and removed on close. Both scopings are
	//! required: MenuLeft is listed by a dozen contexts, so a character-lifetime listener would fire
	//! under some other menu.
	protected void AddMenuNavListeners()
	{
		if(m_bMenuNavListening) return;
		if(!m_InputManager) return;

		m_InputManager.AddActionListener("MenuLeft", EActionTrigger.DOWN, OnMenuLeft);
		m_InputManager.AddActionListener("MenuRight", EActionTrigger.DOWN, OnMenuRight);

		m_bMenuNavListening = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveMenuNavListeners()
	{
		if(!m_bMenuNavListening) return;
		if(!m_InputManager)
		{
			m_bMenuNavListening = false;
			return;
		}

		m_InputManager.RemoveActionListener("MenuLeft", EActionTrigger.DOWN, OnMenuLeft);
		m_InputManager.RemoveActionListener("MenuRight", EActionTrigger.DOWN, OnMenuRight);

		m_bMenuNavListening = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMenuLeft()
	{
		SwapPane(EOVT_TransferPane.LIST);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMenuRight()
	{
		SwapPane(EOVT_TransferPane.CART);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] pane The column to move to.
	protected void SwapPane(EOVT_TransferPane pane)
	{
		if(!m_bIsActive) return;
		if(!m_wRoot) return;

		// SCR_SpinBoxComponent installs its OWN MenuLeft/MenuRight listeners on focus and gates them
		// on focus; this one does not. Without this early return a single d-pad press would change the
		// destination AND jump the focus column.
		if(IsPickerFocused()) return;

		// Only a player already inside one of the two columns is swapping columns. From the header or
		// the footer, left/right belongs to the engine's own directional search.
		if(!FocusIsInPanes()) return;

		if(m_ePane == pane) return;

		if(pane == EOVT_TransferPane.CART && m_aCartWidgets.IsEmpty()) return;
		if(pane == EOVT_TransferPane.LIST && m_aRowWidgets.IsEmpty()) return;

		m_ePane = pane;

		PaintSelection();
		RefreshDetails();
		RefreshActionButtons();
		FocusCurrentPane();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the destination picker, or anything inside it, holds focus.
	protected bool IsPickerFocused()
	{
		if(!m_wDestinationSpin) return false;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if(!workspace) return false;

		Widget focused = workspace.GetFocusedWidget();
		if(!focused) return false;

		return SCR_WidgetTools.InHierarchy(focused, m_wDestinationSpin);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when focus is inside one of the two columns, or nowhere at all.
	protected bool FocusIsInPanes()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if(!workspace) return false;

		Widget focused = workspace.GetFocusedWidget();
		if(!focused) return true;

		if(m_wListRows && SCR_WidgetTools.InHierarchy(focused, m_wListRows)) return true;
		if(m_wCartLines && SCR_WidgetTools.InHierarchy(focused, m_wCartLines)) return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts focus on the remembered index of the remembered pane.
	protected void FocusCurrentPane()
	{
		Widget target = GetPaneFocusTarget();
		if(!target) return;

		GetGame().GetWorkspace().SetFocusedWidget(target);
	}

	//------------------------------------------------------------------------------------------------
	//! Focus is never lost: the remembered index in the remembered pane, else the other pane, else
	//! the checkout buttons.
	protected void RestoreFocus()
	{
		EOVT_TransferPane pane = m_ePane;

		Widget target = GetPaneFocusTarget();

		if(!target)
		{
			if(m_ePane == EOVT_TransferPane.CART)
			{
				m_ePane = EOVT_TransferPane.LIST;
			}else{
				m_ePane = EOVT_TransferPane.CART;
			}

			target = GetPaneFocusTarget();
		}

		if(!target)
		{
			m_ePane = EOVT_TransferPane.LIST;

			// A disabled Accept cannot hold focus, so the close button is the last resort.
			if(m_wAcceptButton && m_wAcceptButton.IsEnabledInHierarchy())
			{
				target = m_wAcceptButton;
			}else{
				target = m_wCloseButton;
			}
		}

		if(m_ePane != pane)
		{
			PaintSelection();
			RefreshDetails();
			RefreshActionButtons();
		}

		if(!target) return;

		GetGame().GetWorkspace().SetFocusedWidget(target);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The widget the current pane's remembered index points at, or null when it is empty.
	protected Widget GetPaneFocusTarget()
	{
		if(m_ePane == EOVT_TransferPane.CART)
		{
			if(m_aCartWidgets.IsEmpty()) return null;
			if(m_iCartIndex < 0 || m_iCartIndex >= m_aCartWidgets.Count()) return m_aCartWidgets[0];

			return m_aCartWidgets[m_iCartIndex];
		}

		if(m_aRowWidgets.IsEmpty()) return null;
		if(m_iListIndex < 0 || m_iListIndex >= m_aRowWidgets.Count()) return m_aRowWidgets[0];

		return m_aRowWidgets[m_iListIndex];
	}

	//-----------------------------------------------------------------------
	// ACTIONS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Mode1Button / OverthrowTransferMode1.
	void Mode1(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(m_aModes.IsEmpty()) return;

		SetMode(m_aModes[0]);
	}

	//------------------------------------------------------------------------------------------------
	//! Mode2Button / OverthrowTransferMode2.
	void Mode2(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(m_aModes.Count() < 2) return;

		SetMode(m_aModes[1]);
	}

	//------------------------------------------------------------------------------------------------
	//! PrevCategoryButton / OverthrowTransferPrevCategory.
	void PreviousCategory(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		CycleTab(-1);
	}

	//------------------------------------------------------------------------------------------------
	//! NextCategoryButton / OverthrowTransferNextCategory.
	void NextCategory(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		CycleTab(1);
	}

	//------------------------------------------------------------------------------------------------
	//! Qty1Button / OverthrowTransferQtyOne.
	void QtyOne(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		ChangeQuantity(QTY_SMALL);
	}

	//------------------------------------------------------------------------------------------------
	//! Qty10Button / OverthrowTransferQtyTen.
	void QtyTen(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		ChangeQuantity(QTY_LARGE);
	}

	//------------------------------------------------------------------------------------------------
	//! QtyAllButton / OverthrowTransferQtyAll. Adds a whole stack in the list, drops a whole line in
	//! the cart.
	void QtyAll(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(!m_bIsActive || !m_wRoot) return;

		if(m_ePane == EOVT_TransferPane.CART)
		{
			OVT_TransferCartLine line = GetCartLine(m_iCartIndex);
			if(!line) return;

			m_Cart.RemoveAll(line.m_sId);
			AfterCartChanged();
			return;
		}

		if(!IsAddAllAllowed(m_iMode)) return;

		OVT_TransferEntry entry = GetListEntry(m_iListIndex);
		if(!entry || !entry.m_bEnabled) return;

		m_Cart.AddAll(entry);
		AfterCartChanged();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] amount How many to add in the list column, or remove in the cart column.
	protected void ChangeQuantity(int amount)
	{
		if(!m_bIsActive || !m_wRoot) return;
		if(amount <= 0) return;

		if(m_ePane == EOVT_TransferPane.CART)
		{
			OVT_TransferCartLine line = GetCartLine(m_iCartIndex);
			if(!line) return;

			m_Cart.Remove(line.m_sId, amount);
			AfterCartChanged();
			return;
		}

		OVT_TransferEntry entry = GetListEntry(m_iListIndex);
		if(!entry || !entry.m_bEnabled) return;

		m_Cart.Add(entry, amount);
		AfterCartChanged();
	}

	//------------------------------------------------------------------------------------------------
	//! Redraws only what a cart change can affect, keeping focus on the column the player is using.
	protected void AfterCartChanged()
	{
		EOVT_TransferPane pane = m_ePane;

		RefreshCart();

		m_ePane = pane;
		NormalizePane();
		PaintSelection();
		RefreshDetails();
		RefreshActionButtons();
		RefreshCheckout();

		if(FocusIsInPanes()) FocusCurrentPane();
	}

	//------------------------------------------------------------------------------------------------
	//! AcceptButton. Commits the cart and leaves the menu open, because the common case is several
	//! trips through the list.
	void Accept(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(!m_bIsActive || !m_wRoot) return;
		if(m_Cart.Count() == 0) return;

		// m_aDestinations is only rebuilt by Refresh(), and a destination entity destroyed since then
		// does not null its handle - ValidateCart's !m_Entity check cannot see it. Re-ask the consumer
		// first. Safe for the picker: the rebuild is guarded and restores the same index without
		// firing m_OnChanged.
		RefreshDestinations();

		OVT_TransferDestination dest = GetSelectedDestination();

		string invalid = ValidateCart(m_Cart.GetLines(), dest);
		if(invalid != "")
		{
			ShowPersistentMessage(invalid);
			return;
		}

		int lineCount = m_Cart.Count();
		int itemCount = m_Cart.TotalQuantity();

		OnAccept(m_Cart.GetLines(), dest);

		m_Cart.Clear();
		m_iCartIndex = 0;
		m_ePane = EOVT_TransferPane.LIST;

		Refresh();

		// Focus is on AcceptButton, which is not a pane, so Refresh() left it there - and
		// RefreshCheckout() has just disabled it over the now-empty cart. Without this a pad player
		// is stranded on a dead widget.
		RestoreFocus();

		ShowMessage("#OVT-Transfer_Accepted", itemCount, lineCount.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! The picker moved. Guarded against the rebuild that fills it.
	//! \param[in] spinbox The picker.
	//! \param[in] index The newly selected destination index.
	protected void OnDestinationChanged(SCR_SpinBoxComponent spinbox, int index)
	{
		if(m_bRebuildingDestinations) return;
		if(!m_bIsActive || !m_wRoot) return;

		m_iDestination = index;

		RefreshCheckout();
	}

	//-----------------------------------------------------------------------
	// STATE ACCESSORS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \param[in] index A row index in the drawn list.
	//! \return That row's entry, or null.
	protected OVT_TransferEntry GetListEntry(int index)
	{
		OVT_TransferRowComponent row = GetRowComponent(index);
		if(!row) return null;

		return row.GetEntry();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] index A row index in the drawn list.
	//! \return That row's component, or null.
	protected OVT_TransferRowComponent GetRowComponent(int index)
	{
		if(index < 0 || index >= m_aRowWidgets.Count()) return null;

		Widget w = m_aRowWidgets[index];
		if(!w) return null;

		return OVT_TransferRowComponent.Cast(w.FindHandler(OVT_TransferRowComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] index A line index in the cart.
	//! \return That line's component, or null.
	protected OVT_TransferCartLineComponent GetCartComponent(int index)
	{
		if(index < 0 || index >= m_aCartWidgets.Count()) return null;

		Widget w = m_aCartWidgets[index];
		if(!w) return null;

		return OVT_TransferCartLineComponent.Cast(w.FindHandler(OVT_TransferCartLineComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] index A line index in the cart.
	//! \return That cart line, or null.
	protected OVT_TransferCartLine GetCartLine(int index)
	{
		array<ref OVT_TransferCartLine> lines = m_Cart.GetLines();
		if(index < 0 || index >= lines.Count()) return null;

		return lines[index];
	}

	//------------------------------------------------------------------------------------------------
	//! The entry the details panel describes: the focused row, or the entry a focused cart line
	//! orders.
	//! \return The selected entry, or null.
	protected OVT_TransferEntry GetSelectedEntry()
	{
		if(m_ePane == EOVT_TransferPane.CART)
		{
			OVT_TransferCartLine line = GetCartLine(m_iCartIndex);
			if(!line) return null;

			return m_Model.FindById(line.m_sId);
		}

		return GetListEntry(m_iListIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The chosen destination, or null when the consumer offers none.
	protected OVT_TransferDestination GetSelectedDestination()
	{
		if(m_aDestinations.IsEmpty()) return null;

		int index = m_iDestination;
		if(m_DestinationSpinBox && m_aDestinations.Count() > 1)
			index = m_DestinationSpinBox.GetCurrentIndex();

		if(index < 0 || index >= m_aDestinations.Count()) index = 0;

		return m_aDestinations[index];
	}

	//------------------------------------------------------------------------------------------------
	//! \return What the cart's value column means, taken from its first line.
	protected EOVT_TransferValueKind GetCartValueKind()
	{
		array<ref OVT_TransferCartLine> lines = m_Cart.GetLines();
		if(lines.IsEmpty()) return EOVT_TransferValueKind.QUANTITY;

		return lines[0].m_eValueKind;
	}

	//------------------------------------------------------------------------------------------------
	//! One formatting rule for both the rows and the cart.
	//! \param[in] value The number to draw.
	//! \param[in] kind What it means.
	//! \return Formatted money, or a plain count.
	static string FormatValue(int value, EOVT_TransferValueKind kind)
	{
		if(kind == EOVT_TransferValueKind.PRICE) return OVT_MoneyFormat.FormatMoney(value);

		return value.ToString();
	}

	//-----------------------------------------------------------------------
	// TEARDOWN HELPERS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Unsubscribes every row and destroys its widget. The array is emptied first, so nothing can
	//! reach a widget that is already gone.
	protected void ClearRows()
	{
		for(int i = 0; i < m_aRowWidgets.Count(); i++)
		{
			OVT_TransferRowComponent row = GetRowComponent(i);
			if(row) row.Cleanup();
		}

		m_aRowWidgets.Clear();

		if(!m_wListRows) return;

		while(m_wListRows.GetChildren())
			m_wListRows.RemoveChild(m_wListRows.GetChildren());
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearCartLines()
	{
		for(int i = 0; i < m_aCartWidgets.Count(); i++)
		{
			OVT_TransferCartLineComponent line = GetCartComponent(i);
			if(line) line.Cleanup();
		}

		m_aCartWidgets.Clear();

		if(!m_wCartLines) return;

		while(m_wCartLines.GetChildren())
			m_wCartLines.RemoveChild(m_wCartLines.GetChildren());
	}

	//-----------------------------------------------------------------------
	// MESSAGES
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Shows a transient localized message in the checkout area.
	//! \param[in] key #OVT- key, with %1 / %2 placeholders.
	//! \param[in] param1 First placeholder value.
	//! \param[in] param2 Second placeholder value.
	protected void ShowMessage(string key, int param1, string param2)
	{
		TextWidget message = PrepareMessage();
		if(!message) return;

		message.SetTextFormat(key, param1, param2);

		m_bMessagePersistent = false;
		m_sMessageKey = key;

		ArmMessageTimeout();
	}

	//------------------------------------------------------------------------------------------------
	//! Shows a standing reason the cart cannot be committed. It does not fade, because the condition
	//! does not go away on a timer, and it is not restarted while it says the same thing.
	//! \param[in] key #OVT- key naming the problem.
	protected void ShowPersistentMessage(string key)
	{
		if(m_bMessagePersistent && m_sMessageKey == key) return;

		TextWidget message = PrepareMessage();
		if(!message) return;

		GetGame().GetCallqueue().Remove(HideMessage);

		message.SetText(key);

		m_bMessagePersistent = true;
		m_sMessageKey = key;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The message widget at full opacity, or null when the layout has none.
	protected TextWidget PrepareMessage()
	{
		if(!m_wRoot) return null;

		TextWidget message = TextWidget.Cast(m_wRoot.FindAnyWidget("MessageText"));
		if(!message) return null;

		// A new message mid-fade must not inherit the fade, or it shows up half-transparent and then
		// vanishes on the old animation's schedule.
		AnimateWidget.StopAnimation(message, WidgetAnimationOpacity);

		message.SetOpacity(1);
		message.SetVisible(true);

		return message;
	}

	//------------------------------------------------------------------------------------------------
	protected void ArmMessageTimeout()
	{
		GetGame().GetCallqueue().Remove(HideMessage);
		GetGame().GetCallqueue().CallLater(HideMessage, MESSAGE_TIMEOUT_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Hard-clears the message with no animation. Used at show time and at close time, where an
	//! animation must never outlive the widget it animates.
	protected void ResetMessage()
	{
		m_bMessagePersistent = false;
		m_sMessageKey = "";

		if(!m_wRoot) return;

		TextWidget message = TextWidget.Cast(m_wRoot.FindAnyWidget("MessageText"));
		if(!message) return;

		AnimateWidget.StopAnimation(message, WidgetAnimationOpacity);

		message.SetText("");
		message.SetOpacity(0);
		message.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void HideMessage()
	{
		m_bMessagePersistent = false;
		m_sMessageKey = "";

		if(!m_wRoot) return;

		TextWidget message = TextWidget.Cast(m_wRoot.FindAnyWidget("MessageText"));
		if(!message) return;

		AnimateWidget.Opacity(message, 0, MESSAGE_FADE_SPEED, true);
	}

	//-----------------------------------------------------------------------
	// HELPERS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! AcceptButton and CloseButton carry "no focus" 0 so a pad can reach them; WLib_NavigationButton
	//! has no Background/Border widget for SCR_ButtonBaseComponent to tint, so the focus highlight is
	//! drawn here instead. Without it a pad player cannot tell that `a` would Accept.
	protected void OnAcceptFocus(Widget w)
	{
		ApplyFocusVisual(m_AcceptAction, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnAcceptFocusLost(Widget w)
	{
		ApplyFocusVisual(m_AcceptAction, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCloseFocus(Widget w)
	{
		ApplyFocusVisual(m_CloseAction, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCloseFocusLost(Widget w)
	{
		ApplyFocusVisual(m_CloseAction, false);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] action The footer button's input component. May be null.
	//! \param[in] focused Whether it currently holds focus.
	protected void ApplyFocusVisual(SCR_InputButtonComponent action, bool focused)
	{
		if(!action) return;

		if(focused)
		{
			action.SetLabelColor(Color.FromInt(COLOR_SELECTED));
		}else{
			action.SetLabelColor(Color.White);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Selected / unselected look for a mode-toggle button. The inactive one is never disabled - a
	//! greyed button reads as "you cannot do this".
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
			action.SetLabelColor(Color.FromInt(COLOR_SELECTED));
		}else{
			action.SetLabelColor(Color.White);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] playerId The player whose balance changed.
	//! \param[in] amount The new balance.
	protected void OnPlayerMoneyChanged(string playerId, int amount)
	{
		if(playerId != m_sPlayerID) return;
		if(!m_bIsActive || !m_wRoot) return;

		TextWidget money = TextWidget.Cast(m_wRoot.FindAnyWidget("PlayerMoney"));
		if(money) money.SetText(OVT_MoneyFormat.FormatMoney(amount));
	}
}
