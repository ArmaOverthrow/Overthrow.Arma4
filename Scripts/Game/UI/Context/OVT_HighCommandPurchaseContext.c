//------------------------------------------------------------------------------------------------
//! The barracks purchase screen. Lists the FIA's authored High Command groups (name, member count,
//! icon - composition read client-side from the prefab source, no spawn) and shows a SERVER quote
//! for whichever one is selected: total, coverage from nearby warehouse stock, supporters required.
//!
//! THIS SCREEN NEVER COMPUTES A PRICE. Warehouse stock is not replicated (logistics/storage), so
//! every number here came from OVT_HighCommandManagerComponent.QuoteEntry on the server and is
//! labelled as an estimate - the purchase re-derives and charges its own number (D19).
//!
//! The quote is asked for on selection change, DEBOUNCED: OVT_HighCommandManagerComponent.QuoteEntry
//! runs a 150 m sphere query per call, and a pad holding MenuDown must not fire one per frame.
//------------------------------------------------------------------------------------------------
class OVT_HighCommandPurchaseContext : OVT_UIContext
{
	[Attribute("{6B1C3D0000002000}UI/Layouts/Menu/HighCommandMenu/HighCommandGroupRow.layout", uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout for one purchasable group row", params: "layout")]
	ResourceName m_GroupRowLayout;

	//! A selection must sit still this long before the server is asked for a quote.
	protected const int QUOTE_DEBOUNCE_MS = 250;

	protected OVT_HighCommandManagerComponent m_Manager;

	//! Resolved lazily in OnShow, guarded everywhere it is used: the controller can still be absent
	//! the first frame after possession.
	protected OVT_HighCommandRequestComponent m_Requests;

	protected SCR_InputButtonComponent m_BuyAction;
	protected SCR_InputButtonComponent m_CloseAction;

	protected int m_iEntryCount;
	protected int m_iSelectedIndex = -1;
	protected Widget m_wSelectedWidget;

	//! The entry index the last OK quote was for, or -1 when nothing bought-able has arrived.
	//! Buy() refuses unless this equals m_iSelectedIndex - the GetLastQuoteLoadout() re-check shape.
	protected int m_iQuotedEntryIndex = -1;

	//! True from a selection change until its debounced quote is answered (or superseded).
	protected bool m_bQuotePending;

	//! True between pressing Buy and the server's answer - the only optimistic state this screen
	//! keeps, and it only ever disables a button.
	protected bool m_bPurchaseInFlight;

	//------------------------------------------------------------------------------------------------
	override void PostInit()
	{
		if (!m_Layout)
			m_Layout = "{6B1C3D0000000090}UI/Layouts/Menu/HighCommandMenu.layout";
	}

	//------------------------------------------------------------------------------------------------
	override void OnShow()
	{
		m_Manager = OVT_Global.GetHighCommand();

		m_Requests = OVT_ControllerComponent<OVT_HighCommandRequestComponent>.Get();
		if (m_Requests)
		{
			m_Requests.m_OnHCQuote.Insert(OnQuote);
			m_Requests.m_OnHCResult.Insert(OnResult);
		}

		Widget closeButton = m_wRoot.FindAnyWidget("CloseButton");
		if (closeButton)
		{
			m_CloseAction = SCR_InputButtonComponent.Cast(closeButton.FindHandler(SCR_InputButtonComponent));
			if (m_CloseAction)
				m_CloseAction.m_OnActivated.Insert(CloseLayout);
		}

		Widget buyButton = m_wRoot.FindAnyWidget("BuyButton");
		if (buyButton)
		{
			m_BuyAction = SCR_InputButtonComponent.Cast(buyButton.FindHandler(SCR_InputButtonComponent));
			if (m_BuyAction)
				m_BuyAction.m_OnActivated.Insert(Buy);
		}

		m_iSelectedIndex = -1;
		m_iQuotedEntryIndex = -1;
		m_bQuotePending = false;
		m_bPurchaseInFlight = false;

		RefreshList();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes everything OnShow inserted - the two invokers live on the controller's request
	//! component, which outlives this screen and would otherwise fire once per past opening.
	override void OnClose()
	{
		GetGame().GetCallqueue().Remove(SendQuoteRequest);
		GetGame().GetCallqueue().Remove(CloseLayout);

		if (m_Requests)
		{
			m_Requests.m_OnHCQuote.Remove(OnQuote);
			m_Requests.m_OnHCResult.Remove(OnResult);
		}

		if (m_BuyAction)
			m_BuyAction.m_OnActivated.Remove(Buy);

		if (m_CloseAction)
			m_CloseAction.m_OnActivated.Remove(CloseLayout);

		m_Requests = null;
		m_Manager = null;
		m_BuyAction = null;
		m_CloseAction = null;
		m_wSelectedWidget = null;
		m_iSelectedIndex = -1;
		m_iQuotedEntryIndex = -1;
		m_iEntryCount = 0;
		m_bQuotePending = false;
		m_bPurchaseInFlight = false;
	}

	//------------------------------------------------------------------------------------------------
	//! d-pad / left stick move the selection. MenuSelect is deliberately NOT wired to Buy - it is the
	//! pad's "confirm the thing under me" button, and a player moving through the list should not be
	//! able to spend money with it (the OVT_LoadoutsContext purchase-mode rule).
	override void OnActiveFrame(float timeSlice)
	{
		super.OnActiveFrame(timeSlice);

		if (m_InputManager.GetActionTriggered("MenuUp"))
			SelectPrevious();

		if (m_InputManager.GetActionTriggered("MenuDown"))
			SelectNext();
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the group list from the authored catalog and selects the first entry.
	protected void RefreshList()
	{
		if (!m_wRoot) return;

		Widget container = m_wRoot.FindAnyWidget("HighCommandGroupsList");
		if (!container) return;

		Widget child = container.GetChildren();
		while (child)
		{
			container.RemoveChild(child);
			child = container.GetChildren();
		}

		m_wSelectedWidget = null;
		m_iEntryCount = 0;
		if (m_Manager)
			m_iEntryCount = m_Manager.GetEntryCount();

		if (m_iEntryCount <= 0)
		{
			ShowDetails(-1);
			return;
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace) return;

		for (int i = 0; i < m_iEntryCount; i++)
		{
			OVT_HighCommandGroupEntry entry = m_Manager.GetEntryByIndex(i);
			if (!entry) continue;

			Widget itemWidget = workspace.CreateWidgets(m_GroupRowLayout, container);
			if (!itemWidget) continue;

			OVT_HighCommandGroupRowHandler handler = OVT_HighCommandGroupRowHandler.Cast(itemWidget.FindHandler(OVT_HighCommandGroupRowHandler));
			if (handler)
			{
				handler.Populate(i, entry, m_Manager.GetEntryMemberCount(entry));
				handler.m_OnClicked.Insert(OnRowClicked);
			}

			if (i == 0)
				SelectEntry(0, itemWidget);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRowClicked(SCR_ButtonBaseComponent button)
	{
		OVT_HighCommandGroupRowHandler handler = OVT_HighCommandGroupRowHandler.Cast(button);
		if (!handler) return;

		SelectEntryByIndex(handler.m_iIndex);
	}

	//------------------------------------------------------------------------------------------------
	protected void SelectPrevious()
	{
		if (m_iEntryCount <= 0) return;

		int index = m_iSelectedIndex - 1;
		if (index < 0) index = m_iEntryCount - 1;

		SelectEntryByIndex(index);
	}

	//------------------------------------------------------------------------------------------------
	protected void SelectNext()
	{
		if (m_iEntryCount <= 0) return;

		int index = m_iSelectedIndex + 1;
		if (index >= m_iEntryCount) index = 0;

		SelectEntryByIndex(index);
	}

	//------------------------------------------------------------------------------------------------
	//! Re-finds the row widget for an index and selects it - the OVT_LoadoutsContext
	//! SelectLoadoutByIndex shape, so selection never depends on which widget happened to be clicked.
	//! \param[in] index Index into the catalog.
	protected void SelectEntryByIndex(int index)
	{
		if (index < 0 || index >= m_iEntryCount) return;

		Widget container = m_wRoot.FindAnyWidget("HighCommandGroupsList");
		if (!container) return;

		Widget child = container.GetChildren();
		int i = 0;
		while (child)
		{
			if (i == index)
			{
				SelectEntry(index, child);
				return;
			}
			child = child.GetSibling();
			i++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] index Index into the catalog.
	//! \param[in] widget The row widget for that index.
	protected void SelectEntry(int index, Widget widget)
	{
		if (m_wSelectedWidget)
		{
			ImageWidget bg = ImageWidget.Cast(m_wSelectedWidget.FindAnyWidget("Background"));
			if (bg) bg.SetOpacity(0);
		}

		m_iSelectedIndex = index;
		m_wSelectedWidget = widget;

		if (widget)
		{
			ImageWidget bg = ImageWidget.Cast(widget.FindAnyWidget("Background"));
			if (bg) bg.SetOpacity(0.3);
		}

		ShowDetails(index);
	}

	//------------------------------------------------------------------------------------------------
	//! Redraws the details panel for a selection and kicks off its debounced quote.
	//! \param[in] index Index into the catalog, or -1 for "nothing selected".
	protected void ShowDetails(int index)
	{
		TextWidget nameText = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedName"));
		TextWidget descText = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDescription"));
		TextWidget membersText = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedMembers"));

		if (index < 0 || !m_Manager)
		{
			if (nameText) nameText.SetText("");
			if (descText) descText.SetText("#OVT-HC_NoSelectionHelp");
			if (membersText) membersText.SetText("");
			ClearQuoteLines();
			ShowQuoteState("", "#OVT-HC_BuyButtonPlain", false);
			return;
		}

		OVT_HighCommandGroupEntry entry = m_Manager.GetEntryByIndex(index);
		if (!entry)
		{
			ClearQuoteLines();
			ShowQuoteState("#OVT-HC_QuoteError", "#OVT-HC_BuyButtonPlain", false);
			return;
		}

		if (nameText) nameText.SetText(entry.m_sTitle);
		if (descText) descText.SetText(entry.m_sDescription);

		if (membersText)
			membersText.SetTextFormat("#OVT-HC_MemberCountFormat", m_Manager.GetEntryMemberCount(entry).ToString());

		RequestQuoteDebounced(index);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] index The row to price a moment from now, once the selection stops moving.
	protected void RequestQuoteDebounced(int index)
	{
		GetGame().GetCallqueue().Remove(SendQuoteRequest);

		m_iQuotedEntryIndex = -1;
		m_bQuotePending = true;

		ClearQuoteLines();
		ShowQuoteState("#OVT-HC_QuoteChecking", "#OVT-HC_QuoteChecking", false);

		GetGame().GetCallqueue().CallLater(SendQuoteRequest, QUOTE_DEBOUNCE_MS, false, index);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] index The row this debounced call was scheduled for.
	protected void SendQuoteRequest(int index)
	{
		// The selection moved again during the debounce window; a stale ask would only race its
		// own successor's reply.
		if (index != m_iSelectedIndex) return;
		if (!m_Requests) return;

		m_Requests.RequestQuoteGroup(index);
	}

	//------------------------------------------------------------------------------------------------
	//! A quote arrived. It may not be for the row the player is looking at any more.
	//! \param[in] entryIndex The entry this quote is for.
	//! \param[in] total The quoted price.
	//! \param[in] coveredValue What nearby warehouse stock covers, at buy price.
	//! \param[in] memberCount How many members the entry buys.
	//! \param[in] supportersNeeded How many town supporters it draws down.
	//! \param[in] refusalCode RESULT_OK, or why it cannot be bought here.
	protected void OnQuote(int entryIndex, int total, int coveredValue, int memberCount, int supportersNeeded, int refusalCode)
	{
		if (entryIndex != m_iSelectedIndex) return;

		m_bQuotePending = false;

		// AT_CAP and NO_SUPPORTERS still carry a real price (the player is told what they cannot
		// quite afford or fit); every other refusal answers zeros and has nothing worth drawing.
		bool showNumbers = refusalCode == OVT_HighCommandRequestComponent.RESULT_OK
			|| refusalCode == OVT_HighCommandRequestComponent.RESULT_AT_CAP
			|| refusalCode == OVT_HighCommandRequestComponent.RESULT_NO_SUPPORTERS;

		if (showNumbers)
			ShowQuoteLines(total, coveredValue, supportersNeeded);
		else
			ClearQuoteLines();

		if (refusalCode == OVT_HighCommandRequestComponent.RESULT_OK)
		{
			m_iQuotedEntryIndex = entryIndex;
			ShowQuoteState("", WidgetManager.Translate("#OVT-HC_BuyButton", total.ToString()), true);
			return;
		}

		m_iQuotedEntryIndex = -1;
		ShowQuoteState(ReasonTextFor(refusalCode, memberCount, supportersNeeded, total), "#OVT-HC_BuyButtonPlain", false);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] total The quoted price.
	//! \param[in] coveredValue What nearby warehouse stock covers, at buy price.
	//! \param[in] supportersRequired How many town supporters the purchase draws down.
	protected void ShowQuoteLines(int total, int coveredValue, int supportersRequired)
	{
		TextWidget totalText = TextWidget.Cast(m_wRoot.FindAnyWidget("QuoteTotal"));
		if (totalText)
			totalText.SetTextFormat("#OVT-HC_QuoteTotal", total.ToString());

		TextWidget coveredText = TextWidget.Cast(m_wRoot.FindAnyWidget("QuoteCovered"));
		if (coveredText)
		{
			if (coveredValue > 0)
				coveredText.SetTextFormat("#OVT-HC_QuoteCovered", coveredValue.ToString());
			else
				coveredText.SetText("#OVT-HC_QuoteNoCoverage");
		}

		TextWidget supportersText = TextWidget.Cast(m_wRoot.FindAnyWidget("QuoteSupporters"));
		if (supportersText)
		{
			if (supportersRequired > 0)
				supportersText.SetTextFormat("#OVT-HC_QuoteSupporters", supportersRequired.ToString());
			else
				supportersText.SetText("#OVT-HC_QuoteNoSupporters");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearQuoteLines()
	{
		TextWidget totalText = TextWidget.Cast(m_wRoot.FindAnyWidget("QuoteTotal"));
		if (totalText) totalText.SetText("");

		TextWidget coveredText = TextWidget.Cast(m_wRoot.FindAnyWidget("QuoteCovered"));
		if (coveredText) coveredText.SetText("");

		TextWidget supportersText = TextWidget.Cast(m_wRoot.FindAnyWidget("QuoteSupporters"));
		if (supportersText) supportersText.SetText("");
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the status line and the Buy button in one place, so "the line says a price but the
	//! button is still disabled" cannot happen from one call site and not another.
	//! \param[in] status The status line, a sentence or a localization key.
	//! \param[in] buttonLabel What the Buy button reads.
	//! \param[in] enabled Whether the purchase may be fired.
	protected void ShowQuoteState(string status, string buttonLabel, bool enabled)
	{
		TextWidget statusText = TextWidget.Cast(m_wRoot.FindAnyWidget("QuoteStatus"));
		if (statusText) statusText.SetText(status);

		if (!m_BuyAction) return;

		m_BuyAction.SetLabel(buttonLabel);

		// A purchase already on the wire keeps the button dead whatever the quote says.
		if (m_bPurchaseInFlight)
		{
			m_BuyAction.SetEnabled(false);
			return;
		}

		m_BuyAction.SetEnabled(enabled);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] refusalCode One of OVT_HighCommandRequestComponent's RESULT_* constants.
	//! \return A localization key describing it. Reuses the purchase notification sentences rather
	//! than authoring a second copy of the same reasons.
	protected string ReasonKeyFor(int refusalCode)
	{
		if (refusalCode == OVT_HighCommandRequestComponent.RESULT_NOT_AT_BARRACKS)
			return "#OVT-Msg-HCPurchaseNotAtBarracks";

		if (refusalCode == OVT_HighCommandRequestComponent.RESULT_BARRACKS_RUINED)
			return "#OVT-Msg-HCPurchaseBarracksRuined";

		if (refusalCode == OVT_HighCommandRequestComponent.RESULT_BASE_NOT_FRIENDLY)
			return "#OVT-Msg-HCPurchaseBaseNotFriendly";

		if (refusalCode == OVT_HighCommandRequestComponent.RESULT_BAD_ENTRY)
			return "#OVT-Msg-HCPurchaseBadEntry";

		if (refusalCode == OVT_HighCommandRequestComponent.RESULT_AT_CAP)
			return "#OVT-Msg-HCPurchaseAtCap";

		if (refusalCode == OVT_HighCommandRequestComponent.RESULT_NO_SUPPORTERS)
			return "#OVT-Msg-HCPurchaseNoSupporters";

		if (refusalCode == OVT_HighCommandRequestComponent.RESULT_CANNOT_AFFORD)
			return "#OVT-Msg-HCPurchaseCannotAfford";

		if (refusalCode == OVT_HighCommandRequestComponent.RESULT_SPAWN_FAILED)
			return "#OVT-Msg-HCPurchaseSpawnFailed";

		return "#OVT-HC_QuoteError";
	}

	//------------------------------------------------------------------------------------------------
	//! The refusal sentence, with its number already in it.
	//!
	//! ⚠ THREE OF THESE KEYS CARRY A %1 and must be formatted HERE. SetText() on a key with a
	//! placeholder renders the literal "%1" - which is exactly what the barracks screen was showing
	//! for "the nearest town cannot spare %1 supporters" (R5). The notification path was never wrong:
	//! OVT_HighCommandRequestComponent.ReplyPurchase() has always passed the number.
	//! \param[in] refusalCode One of OVT_HighCommandRequestComponent's RESULT_* constants.
	//! \param[in] memberCount How many members the refused entry would have added.
	//! \param[in] supportersNeeded How many supporters it would have drawn down.
	//! \param[in] total The quoted price.
	//! \return A sentence, translated when it needed a parameter, a bare key when it did not.
	protected string ReasonTextFor(int refusalCode, int memberCount, int supportersNeeded, int total)
	{
		string key = ReasonKeyFor(refusalCode);

		if (refusalCode == OVT_HighCommandRequestComponent.RESULT_AT_CAP)
			return WidgetManager.Translate(key, memberCount.ToString());

		if (refusalCode == OVT_HighCommandRequestComponent.RESULT_NO_SUPPORTERS)
			return WidgetManager.Translate(key, supportersNeeded.ToString());

		if (refusalCode == OVT_HighCommandRequestComponent.RESULT_CANNOT_AFFORD)
			return WidgetManager.Translate(key, total.ToString());

		return key;
	}

	//------------------------------------------------------------------------------------------------
	//! Buy the selected entry. Names only the entry index - price, stock and supporters are all
	//! re-derived and re-charged server-side (D19); this only stops the screen asking for something
	//! it was just told would be refused.
	protected void Buy()
	{
		if (m_bPurchaseInFlight) return;
		if (m_iSelectedIndex < 0) return;
		if (m_bQuotePending) return;
		if (m_iQuotedEntryIndex != m_iSelectedIndex) return;
		if (!m_Requests) return;

		m_bPurchaseInFlight = true;
		ShowQuoteState("#OVT-HC_Buying", "#OVT-HC_Buying", false);

		m_Requests.RequestPurchaseGroup(m_iSelectedIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! The purchase was answered. Quote-only refusals also route through RpcDo_HCResult
	//! (OVT_HighCommandRequestComponent.ReplyQuote), but those never set m_bPurchaseInFlight, so they
	//! are naturally ignored here.
	//! \param[in] resultCode One of OVT_HighCommandRequestComponent's RESULT_* constants.
	//! \param[in] charged What was actually taken.
	//! \param[in] groupId The new group's id, or "" when none was created.
	protected void OnResult(int resultCode, int charged, string groupId)
	{
		if (!m_bPurchaseInFlight) return;

		m_bPurchaseInFlight = false;

		if (resultCode == OVT_HighCommandRequestComponent.RESULT_OK)
		{
			// Deferred by one frame: this runs inside the component's invoker, and closing here
			// would unsubscribe from that invoker while it is still walking its subscriber list.
			GetGame().GetCallqueue().CallLater(CloseLayout, 0, false);
			return;
		}

		// The world moved (cap, funds, stock) since the quote was shown - it is no longer an answer
		// to anything. Ask again.
		if (m_iSelectedIndex >= 0)
			RequestQuoteDebounced(m_iSelectedIndex);
	}
}
