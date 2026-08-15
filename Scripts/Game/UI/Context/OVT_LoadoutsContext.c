class OVT_LoadoutsContext : OVT_UIContext
{
	[Attribute("{5D5C558A6E391692}UI/Layouts/Menu/LoadoutsMenu/LoadoutListItem.layout", uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout for loadout list items", params: "layout")]
	ResourceName m_LoadoutItemLayout;
	[Attribute("{AA94A214B16CB2C8}UI/Layouts/Menu/RecruitsMenu/RecruitListItem.layout", uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout for recruit list items", params: "layout")]
	ResourceName m_RecruitItemLayout;
	
	protected OVT_LoadoutManagerComponent m_LoadoutManager;
	protected ref array<string> m_aPlayerLoadoutNames;
	protected string m_SelectedLoadoutName;
	protected Widget m_wSelectedWidget;
	protected int m_iSelectedIndex = -1;
	protected IEntity m_EquipmentBox;
	//! The recruitment tent this screen was opened from, when it was opened to BUY a recruit rather
	//! than to apply a loadout out of a box. Mutually exclusive with m_EquipmentBox - see
	//! SetRecruitmentTent().
	protected IEntity m_RecruitmentTent;
	protected ref array<IEntity> m_aNearbyRecruits;
	protected IEntity m_SelectedRecruit;
	protected Widget m_wSelectedRecruitWidget;
	protected int m_iSelectedRecruitIndex = -1;

	//! PURCHASE MODE ONLY. The controller component that quotes and buys; null on a machine whose
	//! controller has not arrived yet, which is why every use of it is guarded rather than cached at
	//! PostInit.
	protected OVT_RecruitCommandComponent m_RecruitCommands;

	//! The purchase button's action component, held so the label and the enabled state can be driven
	//! from the quote. Cleared in OnClose with the layout it belongs to.
	protected SCR_InputButtonComponent m_PurchaseAction;

	//! The loadout a quote has been asked for and not yet answered, or empty when nothing is in
	//! flight. Also the "checking price..." condition.
	protected string m_sQuotePendingFor;

	//! True between pressing Buy and the server's answer. Stops a second press buying a second
	//! recruit while the first request is still on the wire - the ONLY optimistic state this screen
	//! keeps, and it disables a button rather than pretending anything happened.
	protected bool m_bPurchaseInFlight;

	override void PostInit()
	{		
		m_LoadoutManager = OVT_Global.GetLoadouts();
		
		// Initialize arrays
		m_aNearbyRecruits = new array<IEntity>();
		
		// Set the main layout if not already set through attributes
		if (!m_Layout)
		{
			m_Layout = "{5D5C558A6E391692}UI/Layouts/Menu/LoadoutsMenu.layout";
		}
	}
	
	override void OnShow()
	{		
		Widget closeButton = m_wRoot.FindAnyWidget("CloseButton");
		if (closeButton)
		{
			SCR_InputButtonComponent action = SCR_InputButtonComponent.Cast(closeButton.FindHandler(SCR_InputButtonComponent));
			if (action)
				action.m_OnActivated.Insert(CloseLayout);
		}
		
		// Set up button click handlers
		Widget applyButton = m_wRoot.FindAnyWidget("ApplyButton");
		if (applyButton)
		{
			SCR_InputButtonComponent action = SCR_InputButtonComponent.Cast(applyButton.FindHandler(SCR_InputButtonComponent));
			if (action)
				action.m_OnActivated.Insert(ApplyLoadout);
		}
		
		Widget deleteButton = m_wRoot.FindAnyWidget("DeleteButton");
		if (deleteButton)
		{
			SCR_InputButtonComponent action = SCR_InputButtonComponent.Cast(deleteButton.FindHandler(SCR_InputButtonComponent));
			if (action)
				action.m_OnActivated.Insert(DeleteLoadout);
		}
		
		Widget applyToSelectedRecruitButton = m_wRoot.FindAnyWidget("ApplyToSelectedRecruitButton");
		if (applyToSelectedRecruitButton)
		{
			SCR_InputButtonComponent action = SCR_InputButtonComponent.Cast(applyToSelectedRecruitButton.FindHandler(SCR_InputButtonComponent));
			if (action)
				action.m_OnActivated.Insert(ApplyLoadoutToSelectedRecruit);
		}
		
		Widget applyToAllRecruitsButton = m_wRoot.FindAnyWidget("ApplyToAllRecruitsButton");
		if (applyToAllRecruitsButton)
		{
			SCR_InputButtonComponent action = SCR_InputButtonComponent.Cast(applyToAllRecruitsButton.FindHandler(SCR_InputButtonComponent));
			if (action)
				action.m_OnActivated.Insert(ApplyLoadoutToAllRecruits);
		}
		
		// PURCHASE MODE. The button exists in every copy of this layout but is authored hidden, so a
		// box-mode screen is unchanged even if none of the code below runs.
		m_bPurchaseInFlight = false;
		m_sQuotePendingFor = "";
		m_PurchaseAction = null;

		Widget purchaseButton = m_wRoot.FindAnyWidget("PurchaseButton");
		if (purchaseButton)
		{
			m_PurchaseAction = SCR_InputButtonComponent.Cast(purchaseButton.FindHandler(SCR_InputButtonComponent));
			if (m_PurchaseAction)
				m_PurchaseAction.m_OnActivated.Insert(BuyEquippedRecruit);
		}

		if (IsPurchaseMode())
		{
			// Subscribed only in purchase mode, and removed unconditionally in OnClose: these invokers
			// live on the controller component, which outlives every layout this screen ever builds.
			m_RecruitCommands = OVT_Global.GetRecruitCommands();
			if (m_RecruitCommands)
			{
				m_RecruitCommands.m_OnRecruitQuote.Insert(OnRecruitQuote);
				m_RecruitCommands.m_OnEquippedRecruitResult.Insert(OnPurchaseResult);
			}
		}

		ApplyPurchaseModeVisibility();

		// Use the owner entity for applying loadouts

		Refresh();
		RefreshRecruits();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes everything OnShow inserted.
	//!
	//! The two quote/result invokers sit on OVT_RecruitCommandComponent, which is part of the player's
	//! controller and survives this screen - a subscription left behind would fire N times on the Nth
	//! open, against widgets that no longer exist.
	override void OnClose()
	{
		if (m_RecruitCommands)
		{
			m_RecruitCommands.m_OnRecruitQuote.Remove(OnRecruitQuote);
			m_RecruitCommands.m_OnEquippedRecruitResult.Remove(OnPurchaseResult);
		}

		if (m_PurchaseAction)
			m_PurchaseAction.m_OnActivated.Remove(BuyEquippedRecruit);

		// A close queued from a successful purchase must not outlive the close that already happened.
		GetGame().GetCallqueue().Remove(CloseLayout);

		m_RecruitCommands = null;
		m_PurchaseAction = null;
		m_sQuotePendingFor = "";
		m_bPurchaseInFlight = false;
	}

	override void OnActiveFrame(float timeSlice)
	{
		super.OnActiveFrame(timeSlice);
		
		// Handle loadout list navigation
		if (m_InputManager.GetActionTriggered("MenuUp"))
		{
			SelectPreviousLoadout();
		}
		
		if (m_InputManager.GetActionTriggered("MenuDown"))
		{
			SelectNextLoadout();
		}
		
		// MenuSelect (Enter / pad A) applies the loadout out of the box. It deliberately does NOT buy in
		// purchase mode: A is the pad's "confirm the thing under me" button and a player moving through
		// the list should not be able to spend money with it. Buying has its own labelled action, whose
		// glyph is drawn on the button.
		if (m_InputManager.GetActionTriggered("MenuSelect") && !IsPurchaseMode())
		{
			ApplyLoadout();
		}
	}
	
	protected void Refresh()
	{
		if (!m_LoadoutManager)
			return;
			
		// Get player loadouts
		m_aPlayerLoadoutNames = m_LoadoutManager.GetAvailableLoadouts(m_sPlayerID);
		
		// Clear the list
		Widget container = m_wRoot.FindAnyWidget("LoadoutsList");
		if (!container)
			return;
			
		Widget child = container.GetChildren();
		while(child)
		{
			container.RemoveChild(child);
			child = container.GetChildren();
		}
		
		// Update help text visibility
		Widget helpText = m_wRoot.FindAnyWidget("NoLoadoutsHelp");
		Widget loadoutInfo = m_wRoot.FindAnyWidget("LoadoutInfo");
		
		if (m_aPlayerLoadoutNames.IsEmpty())
		{
			// Show help text when no loadouts
			if (helpText)
				helpText.SetVisible(true);
			if (loadoutInfo)
				loadoutInfo.SetVisible(false);
				
			// Hide action buttons
			SetButtonsVisible(false);
			
			m_SelectedLoadoutName = "";
			return;
		}
		else
		{
			// Hide help text when has loadouts
			if (helpText)
				helpText.SetVisible(false);
			if (loadoutInfo)
				loadoutInfo.SetVisible(true);
		}
		
		// Populate loadout list
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		
		foreach (int i, string loadoutName : m_aPlayerLoadoutNames)
		{
			Widget itemWidget = workspace.CreateWidgets(m_LoadoutItemLayout, container);
			if (!itemWidget)
				continue;
				
			// Get the handler and populate it
			OVT_LoadoutListEntryHandler handler = OVT_LoadoutListEntryHandler.Cast(itemWidget.FindHandler(OVT_LoadoutListEntryHandler));
			if (handler)
			{
				handler.Populate(loadoutName, i);
				handler.m_OnClicked.Insert(OnLoadoutItemClicked);
			}
			
			// Select first loadout by default
			if (i == 0)
			{
				m_iSelectedIndex = 0;
				SelectLoadout(loadoutName, itemWidget);
			}
		}
	}
	
	protected void OnLoadoutItemClicked(SCR_ButtonBaseComponent button)
	{
		OVT_LoadoutListEntryHandler handler = OVT_LoadoutListEntryHandler.Cast(button);
		if (!handler)
			return;
			
		SelectLoadoutByIndex(handler.m_iIndex);
	}
	
	void SelectLoadout(string loadoutName, Widget widget)
	{
		// Update selection highlight
		if (m_wSelectedWidget)
		{
			ImageWidget bg = ImageWidget.Cast(m_wSelectedWidget.FindAnyWidget("Background"));
			if (bg)
				bg.SetOpacity(0);
		}
		
		m_SelectedLoadoutName = loadoutName;
		m_wSelectedWidget = widget;
		
		if (widget)
		{
			ImageWidget bg = ImageWidget.Cast(widget.FindAnyWidget("Background"));
			if (bg)
				bg.SetOpacity(0.3);
		}
		
		// Update loadout details
		UpdateLoadoutDetails();
	}
	
	protected void UpdateLoadoutDetails()
	{
		if (m_SelectedLoadoutName.IsEmpty())
		{
			SetButtonsVisible(false);
			return;
		}
		
		SetButtonsVisible(true);
		
		// Update name
		TextWidget nameText = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedName"));
		if (nameText)
			nameText.SetText(m_SelectedLoadoutName);
		
		if (IsPurchaseMode())
		{
			// A tent selection is worth a price, not a status. RequestQuote drives both the details
			// line and the button from here on.
			RequestQuote(m_SelectedLoadoutName);
			return;
		}

		// TODO: Add more details like item count, last used, etc.
		TextWidget statusText = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedStatus"));
		if (statusText)
			statusText.SetText("Ready to apply");

	// Update recruit button visibility when loadout selection changes
	UpdateRecruitButtonVisibility();
	}

	//------------------------------------------------------------------------------------------------
	//! Ask the server what a recruit wearing this loadout would cost at this tent.
	//!
	//! Fired on every selection change, so it must stay cheap and must never assume the previous answer
	//! is still relevant. The button is DISABLED for the whole round trip: a pad user holding down the
	//! d-pad and mashing the buy button can otherwise fire against a price that belongs to a different
	//! row.
	//! \param[in] loadoutName The row now highlighted.
	protected void RequestQuote(string loadoutName)
	{
		m_sQuotePendingFor = "";

		if (loadoutName.IsEmpty())
		{
			ShowPurchaseState("", "Buy Recruit", false);
			return;
		}

		if (!m_RecruitCommands)
			m_RecruitCommands = OVT_Global.GetRecruitCommands();

		RplId tentId = GetTentRplId();

		if (!m_RecruitCommands || !tentId.IsValid())
		{
			// TODO localize: #OVT-Recruit_NotAtTent
			ShowPurchaseState("This tent cannot be reached", "Buy Recruit", false);
			return;
		}

		m_sQuotePendingFor = loadoutName;

		// TODO localize: #OVT-Recruit_QuoteChecking
		ShowPurchaseState("Checking price...", "Checking price...", false);

		m_RecruitCommands.RequestRecruitLoadoutQuote(tentId, loadoutName);
	}

	//------------------------------------------------------------------------------------------------
	//! The replication id of the tent this screen was opened from.
	//! \return The tent's RplId, or an invalid id when there is no tent or it carries no RplComponent.
	protected RplId GetTentRplId()
	{
		if (!m_RecruitmentTent)
			return RplId.Invalid();

		RplComponent rpl = RplComponent.Cast(m_RecruitmentTent.FindComponent(RplComponent));
		if (!rpl)
			return RplId.Invalid();

		return rpl.Id();
	}

	//------------------------------------------------------------------------------------------------
	//! A quote arrived. It may not be for the row the player is looking at.
	//!
	//! ! STALE REPLIES ARE DROPPED BY NAME. Quotes are asked per-selection and answered a round trip
	//! later, so on a fast scroll an older answer can arrive after a newer one. The reply carries the
	//! loadout it is for precisely so this comparison is possible; without it the screen would show one
	//! row's price against another row's name.
	//! \param[in] loadoutName The loadout this quote is for.
	//! \param[in] price The whole quoted price.
	//! \param[in] itemCount How many items were priced.
	//! \param[in] refusalCode RESULT_OK, or why it cannot be bought.
	//! \param[in] unpriceableResource The item with no price, when that is the refusal.
	protected void OnRecruitQuote(string loadoutName, int price, int itemCount, int refusalCode, string unpriceableResource)
	{
		if (!IsPurchaseMode()) return;
		if (loadoutName != m_SelectedLoadoutName) return;

		m_sQuotePendingFor = "";

		if (refusalCode == OVT_RecruitCommandComponent.RESULT_OK)
		{
			// TODO localize: #OVT-Recruit_QuotePrice / #OVT-Recruit_BuyButton
			ShowPurchaseState(string.Format("%1 items - $%2 equipped", itemCount, price),
				string.Format("Buy Recruit ($%1)", price), true);
			return;
		}

		// A price still exists for this one, and it is the number the player needs in order to know how
		// short they are - so it is shown beside the reason rather than instead of it.
		if (refusalCode == OVT_RecruitCommandComponent.RESULT_CANNOT_AFFORD)
		{
			// TODO localize: #OVT-CannotAfford
			ShowPurchaseState(string.Format("$%1 - you cannot afford this", price),
				string.Format("Buy Recruit ($%1)", price), false);
			return;
		}

		if (refusalCode == OVT_RecruitCommandComponent.RESULT_UNPRICEABLE)
		{
			// TODO localize: #OVT-Recruit_LoadoutUnpriceable
			ShowPurchaseState(string.Format("No price for %1 - this loadout cannot be bought",
				OVT_RecruitCommandComponent.ShortResourceName(unpriceableResource)), "Buy Recruit", false);
			return;
		}

		ShowPurchaseState(OVT_RecruitCommandComponent.ReasonKeyFor(refusalCode), "Buy Recruit", false);
	}

	//------------------------------------------------------------------------------------------------
	//! Write the details line and the buy button in one place.
	//!
	//! One method rather than three call sites, because "the line says a price but the button is still
	//! disabled" is the failure this screen most easily produces.
	//! \param[in] status The details line, a sentence or a localization key.
	//! \param[in] buttonLabel What the buy button reads.
	//! \param[in] enabled Whether the purchase may be fired.
	protected void ShowPurchaseState(string status, string buttonLabel, bool enabled)
	{
		TextWidget statusText = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedStatus"));
		if (statusText)
			statusText.SetText(status);

		if (!m_PurchaseAction) return;

		m_PurchaseAction.SetLabel(buttonLabel);

		// A purchase already on the wire keeps the button dead whatever the quote says.
		if (m_bPurchaseInFlight)
		{
			m_PurchaseAction.SetEnabled(false);
			return;
		}

		m_PurchaseAction.SetEnabled(enabled);
	}

	//------------------------------------------------------------------------------------------------
	//! Buy a recruit wearing the selected loadout.
	//!
	//! Names a tent and a loadout and nothing else - no price, no body, no player id. Everything is
	//! re-derived and re-validated server-side, so this refusing wrongly costs a player one press and
	//! this permitting wrongly costs them nothing.
	protected void BuyEquippedRecruit()
	{
		if (!IsPurchaseMode()) return;
		if (m_bPurchaseInFlight) return;
		if (m_SelectedLoadoutName.IsEmpty()) return;

		// A quote still in flight means no price has been seen for this row.
		if (!m_sQuotePendingFor.IsEmpty()) return;

		if (!m_RecruitCommands) return;

		// The cached quote must be THIS row's and must have been a yes. Both are re-checked on the
		// server; this only stops the screen from asking for something it was told would be refused.
		if (m_RecruitCommands.GetLastQuoteLoadout() != m_SelectedLoadoutName) return;
		if (m_RecruitCommands.GetLastQuoteRefusal() != OVT_RecruitCommandComponent.RESULT_OK) return;

		RplId tentId = GetTentRplId();
		if (!tentId.IsValid()) return;

		m_bPurchaseInFlight = true;

		// TODO localize: #OVT-Recruit_QuoteChecking
		ShowPurchaseState("Buying...", "Buying...", false);

		m_RecruitCommands.RequestBuyEquippedRecruit(tentId, m_SelectedLoadoutName);
	}

	//------------------------------------------------------------------------------------------------
	//! The purchase was answered. The component has already shown the player the outcome hint.
	//!
	//! A bought recruit closes the screen - the player is standing at the tent and the thing they came
	//! for is now walking around. Anything else re-quotes, because a refusal means the world moved
	//! (money spent elsewhere, a supporter taken, the cap reached) and the old price is no longer an
	//! answer to anything.
	//! \param[in] resultCode One of the RESULT_ codes.
	//! \param[in] charged What was actually taken.
	//! \param[in] applied Top-level items that landed on the recruit.
	//! \param[in] total Top-level items the loadout recorded.
	protected void OnPurchaseResult(int resultCode, int charged, int applied, int total)
	{
		m_bPurchaseInFlight = false;

		if (resultCode == OVT_RecruitCommandComponent.RESULT_BUY_OK || resultCode == OVT_RecruitCommandComponent.RESULT_BUY_PARTIAL)
		{
			// Deferred by one frame: this runs inside the component's invoker, and closing here would
			// unsubscribe from that invoker while it is still walking its own subscriber list.
			GetGame().GetCallqueue().CallLater(CloseLayout, 0, false);
			return;
		}

		RequestQuote(m_SelectedLoadoutName);
	}
	
	protected void SelectPreviousLoadout()
	{
		if (m_aPlayerLoadoutNames.IsEmpty())
			return;
			
		int newIndex = m_iSelectedIndex - 1;
		if (newIndex < 0)
			newIndex = m_aPlayerLoadoutNames.Count() - 1;
			
		SelectLoadoutByIndex(newIndex);
	}
	
	protected void SelectNextLoadout()
	{
		if (m_aPlayerLoadoutNames.IsEmpty())
			return;
			
		int newIndex = m_iSelectedIndex + 1;
		if (newIndex >= m_aPlayerLoadoutNames.Count())
			newIndex = 0;
			
		SelectLoadoutByIndex(newIndex);
	}
	
	protected void SelectLoadoutByIndex(int index)
	{
		if (index < 0 || index >= m_aPlayerLoadoutNames.Count())
			return;
			
		m_iSelectedIndex = index;
		string loadoutName = m_aPlayerLoadoutNames[index];
		
		// Find the widget for this loadout
		Widget container = m_wRoot.FindAnyWidget("LoadoutsList");
		if (!container)
			return;
			
		Widget child = container.GetChildren();
		int i = 0;
		while (child && i <= index)
		{
			if (i == index)
			{
				SelectLoadout(loadoutName, child);
				break;
			}
			child = child.GetSibling();
			i++;
		}
	}
	
	protected void SetButtonsVisible(bool visible)
	{
		bool purchase = IsPurchaseMode();

		array<string> buttonNames = {
			"ApplyButton",
			"DeleteButton"
		};

		foreach (string name : buttonNames)
		{
			Widget button = m_wRoot.FindAnyWidget(name);
			if (!button) continue;

			// The box actions are never shown at a tent: applying or deleting a loadout is not what the
			// player came here to do, and Apply cannot work at all without a box.
			if (purchase)
			{
				button.SetVisible(false);
			}
			else
			{
				button.SetVisible(visible);
			}
		}

		Widget purchaseButton = m_wRoot.FindAnyWidget("PurchaseButton");
		if (purchaseButton)
		{
			if (purchase)
			{
				purchaseButton.SetVisible(visible);
			}
			else
			{
				purchaseButton.SetVisible(false);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Show the widgets this mode uses and hide the ones it does not.
	//!
	//! Called once per open, before the list is built, so a purchase-mode screen never flashes the box
	//! actions. Everything it hides is hidden again by SetButtonsVisible/SetRecruitButtonsVisible on
	//! every selection change - this method is the "before there is a selection" pass.
	protected void ApplyPurchaseModeVisibility()
	{
		bool purchase = IsPurchaseMode();

		// The recruit half of this screen applies a loadout OUT OF A BOX to a recruit standing next to
		// it. At a tent there is no box, so none of it can do anything.
		array<string> recruitWidgets = {
			"RecruitSection",
			"RecruitButtons",
			"SelectedRecruitName"
		};

		foreach (string name : recruitWidgets)
		{
			Widget widget = m_wRoot.FindAnyWidget(name);
			if (!widget) continue;

			if (purchase)
			{
				widget.SetVisible(false);
			}
			else
			{
				widget.SetVisible(true);
			}
		}

		SetButtonsVisible(!m_SelectedLoadoutName.IsEmpty());
	}

	protected void ApplyLoadout()
	{
		Print(string.Format("[OVT_LoadoutsContext] ApplyLoadout called - PlayerID: %1, LoadoutName: %2", m_sPlayerID, m_SelectedLoadoutName));
		
		if (m_SelectedLoadoutName.IsEmpty() || !m_Owner || !m_EquipmentBox)
		{
			Print(string.Format("[OVT_LoadoutsContext] ApplyLoadout failed - Empty name: %1, No owner: %2, No equipment box: %3", 
				m_SelectedLoadoutName.IsEmpty(), !m_Owner, !m_EquipmentBox));
			return;
		}
			
		// Apply the selected loadout via server RPC
		OVT_PlayerCommsComponent comms = OVT_Global.GetServer();
		if (comms)
		{
			Print(string.Format("[OVT_LoadoutsContext] Sending RPC for loadout: %1", m_SelectedLoadoutName));
			comms.LoadLoadoutFromBox(m_sPlayerID, m_SelectedLoadoutName, m_EquipmentBox, m_Owner);
			CloseLayout();
			// Notification will be sent by the loadout manager after processing
		}
		else
		{
			Print("[OVT_LoadoutsContext] No server comms component found");
		}
	}
	
	//! Set the equipment box entity to use for loadout operations
	//!
	//! Clears the recruitment tent: this screen is opened from two different actions and serves two
	//! different purposes, and "which mode am I in" has to be answerable from the two fields alone.
	//! Leaving a stale tent behind would put a Buy button on a box screen.
	void SetEquipmentBox(IEntity equipmentBox)
	{
		m_EquipmentBox = equipmentBox;
		m_RecruitmentTent = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Open this screen in PURCHASE mode, against a recruitment tent.
	//!
	//! Purchase mode is "a tent was set and a box was not". In it the screen lists the same loadouts
	//! with the same selection model and the same pad bindings, but the box-only actions (apply,
	//! delete, apply-to-recruit) are not what the player came for - they came to buy a recruit already
	//! wearing one of these.
	//!
	//! ! THE TENT MUST BE THE ROOT ENTITY. It is the root that carries the RplComponent the purchase
	//! request names it by, and the OVT_BuildableComponent the server checks it against; the actions
	//! are authored on the tent's table child, so an action must walk up before calling this.
	//! \param[in] tent The tent's root entity.
	void SetRecruitmentTent(IEntity tent)
	{
		m_RecruitmentTent = tent;
		m_EquipmentBox = null;
	}

	//------------------------------------------------------------------------------------------------
	//! The recruitment tent this screen was opened from, or null.
	IEntity GetRecruitmentTent()
	{
		return m_RecruitmentTent;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this screen is buying a recruit rather than applying a loadout from a box.
	bool IsPurchaseMode()
	{
		if(!m_RecruitmentTent) return false;

		return m_EquipmentBox == null;
	}
	
	protected void DeleteLoadout()
	{
		if (m_SelectedLoadoutName.IsEmpty())
			return;

		// Belt and braces: the Delete button is hidden in purchase mode, and SCR_InputButtonComponent
		// refuses to fire for a hidden widget - but "the key that deletes a loadout" is not a thing to
		// leave depending on one mechanism.
		if (IsPurchaseMode())
			return;
			
		// Delete the selected loadout via server RPC
		OVT_PlayerCommsComponent comms = OVT_Global.GetServer();
		if (comms)
		{
			comms.DeleteLoadout(m_sPlayerID, m_SelectedLoadoutName, false); // Assume personal loadout for now
			// Notification will be sent by the loadout manager after processing
			
			// Refresh the loadout list
			Refresh();
		}
		else
		{
			Print("[OVT_LoadoutsContext] Failed to delete loadout - no server communication");
		}
	}
	
	protected void RefreshRecruits()
	{
		if (!m_Owner)
			return;
		
		// Ensure array is initialized
		if (!m_aNearbyRecruits)
			m_aNearbyRecruits = new array<IEntity>();
		
		m_aNearbyRecruits.Clear();
		
		DiscoverNearbyRecruits();
		
		Widget recruitContainer = m_wRoot.FindAnyWidget("RecruitsList");
		if (!recruitContainer)
			return;
			
		Widget child = recruitContainer.GetChildren();
		while(child)
		{
			recruitContainer.RemoveChild(child);
			child = recruitContainer.GetChildren();
		}
		
		Widget noRecruitsText = m_wRoot.FindAnyWidget("NoRecruitsText");
		Widget recruitsTitle = m_wRoot.FindAnyWidget("RecruitsTitle");
		
		if (m_aNearbyRecruits.IsEmpty())
		{
			if (noRecruitsText)
				noRecruitsText.SetVisible(true);
			if (recruitsTitle)
				recruitsTitle.SetVisible(false);
				
			SetRecruitButtonsVisible(false);
			
			m_SelectedRecruit = null;
			m_iSelectedRecruitIndex = -1;
			UpdateSelectedRecruitDisplay();
			return;
		}
		else
		{
			if (noRecruitsText)
				noRecruitsText.SetVisible(false);
			if (recruitsTitle)
				recruitsTitle.SetVisible(true);
		}
		
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		
		foreach (int i, IEntity recruitEntity : m_aNearbyRecruits)
		{
			Widget itemWidget = workspace.CreateWidgets(m_RecruitItemLayout, recruitContainer);
			if (!itemWidget)
				continue;
				
			string recruitName = GetCharacterName(recruitEntity);
			
			OVT_RecruitListEntryHandler handler = OVT_RecruitListEntryHandler.Cast(itemWidget.FindHandler(OVT_RecruitListEntryHandler));
			if (handler)
			{
				handler.PopulateFromEntity(recruitEntity, recruitName, i);
				handler.m_OnClicked.Insert(OnRecruitItemClicked);
			}
			
			if (i == 0)
			{
				m_iSelectedRecruitIndex = 0;
				SelectRecruit(recruitEntity, itemWidget);
			}
		}
		
		UpdateRecruitButtonVisibility();
	}
	
	//! How far from the equipment box a recruit may stand and still be offered in the list.
	//!
	//! MEASURED FROM THE BOX, NOT THE PLAYER, because that is what the server checks:
	//! OVT_PlayerCommsComponent.RpcAsk_LoadLoadoutFromBox refuses any target further than its own
	//! LOADOUT_BOX_MAX_DISTANCE (20 m) from the box, and it refuses SILENTLY - no notification, no
	//! result. Listing a recruit the server will then ignore is worse than not listing them, so this
	//! stays under that ceiling, with the remaining metres left as slack for a recruit wandering
	//! during the time the menu is open.
	protected const float RECRUIT_SEARCH_RADIUS = 15.0;

	protected void DiscoverNearbyRecruits()
	{
		if (!m_Owner)
			return;

		// The player is themselves within interaction range of the box, so their position is a sound
		// fallback for the frame before SetEquipmentBox has run.
		vector searchCenter = m_Owner.GetOrigin();
		if (m_EquipmentBox)
			searchCenter = m_EquipmentBox.GetOrigin();

		GetGame().GetWorld().QueryEntitiesBySphere(searchCenter, RECRUIT_SEARCH_RADIUS, null, FilterRecruitEntities, EQueryEntitiesFlags.ALL);
	}
	
	protected bool FilterRecruitEntities(IEntity entity)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!character || character == m_Owner)
			return false;
			
		OVT_PlayerOwnerComponent ownerComp = OVT_PlayerOwnerComponent.Cast(entity.FindComponent(OVT_PlayerOwnerComponent));
		if (!ownerComp)
			return false;
			
		string ownerUID = ownerComp.GetPlayerOwnerUid();
		if (ownerUID == m_sPlayerID)
		{
			m_aNearbyRecruits.Insert(entity);
		}
		
		return false;
	}
	
	protected string GetCharacterName(IEntity character)
	{
		// First try to get name from recruit manager
		OVT_RecruitManagerComponent recruitManager = OVT_Global.GetRecruits();
		if (recruitManager)
		{
			OVT_RecruitData recruitData = recruitManager.GetRecruitFromEntity(character);
			if (recruitData)
			{
				string recruitName = recruitData.GetName();
				if (!recruitName.IsEmpty())
					return recruitName;
			}
		}
		
		// Fallback to editable character component
		SCR_EditableCharacterComponent editableChar = SCR_EditableCharacterComponent.Cast(character.FindComponent(SCR_EditableCharacterComponent));
		if (editableChar)
		{
			string displayName = editableChar.GetDisplayName();
			if (!displayName.IsEmpty())
				return displayName;
		}
		
		return "Recruit";
	}
	
	protected void OnRecruitItemClicked(SCR_ButtonBaseComponent button)
	{
		OVT_RecruitListEntryHandler handler = OVT_RecruitListEntryHandler.Cast(button);
		if (!handler)
			return;
			
		SelectRecruitByIndex(handler.GetIndex());
	}
	
	void SelectRecruit(IEntity recruitEntity, Widget widget)
	{
		if (m_wSelectedRecruitWidget)
		{
			ImageWidget bg = ImageWidget.Cast(m_wSelectedRecruitWidget.FindAnyWidget("Background"));
			if (bg)
				bg.SetOpacity(0);
		}
		
		m_SelectedRecruit = recruitEntity;
		m_wSelectedRecruitWidget = widget;
		
		if (widget)
		{
			ImageWidget bg = ImageWidget.Cast(widget.FindAnyWidget("Background"));
			if (bg)
				bg.SetOpacity(0.3);
		}
		
		UpdateSelectedRecruitDisplay();
		UpdateRecruitButtonVisibility();
	}
	
	protected void SelectRecruitByIndex(int index)
	{
		if (!m_aNearbyRecruits || index < 0 || index >= m_aNearbyRecruits.Count())
			return;
			
		m_iSelectedRecruitIndex = index;
		IEntity recruitEntity = m_aNearbyRecruits[index];
		
		Widget container = m_wRoot.FindAnyWidget("RecruitsList");
		if (!container)
			return;
			
		Widget child = container.GetChildren();
		int i = 0;
		while (child && i <= index)
		{
			if (i == index)
			{
				SelectRecruit(recruitEntity, child);
				break;
			}
			child = child.GetSibling();
			i++;
		}
	}
	
	protected void UpdateSelectedRecruitDisplay()
	{
		TextWidget selectedRecruitText = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedRecruitName"));
		if (!selectedRecruitText)
			return;
			
		if (m_SelectedRecruit)
		{
			string recruitName = GetCharacterName(m_SelectedRecruit);
			selectedRecruitText.SetTextFormat("#OVT-Loadouts_SelectedRecruit", recruitName);
		}
		else
		{
			selectedRecruitText.SetText("#OVT-Loadouts_NoRecruitSelected");
		}
	}

	protected void UpdateRecruitButtonVisibility()
	{
		bool hasLoadout = !m_SelectedLoadoutName.IsEmpty();
		bool hasRecruits = m_aNearbyRecruits && !m_aNearbyRecruits.IsEmpty();
		bool hasSelectedRecruit = m_SelectedRecruit != null;
		
		SetRecruitButtonsVisible(hasLoadout && hasRecruits);
		
		Widget applyToSelectedButton = m_wRoot.FindAnyWidget("ApplyToSelectedRecruitButton");
		if (applyToSelectedButton)
			applyToSelectedButton.SetEnabled(hasLoadout && hasSelectedRecruit);
			
		Widget applyToAllButton = m_wRoot.FindAnyWidget("ApplyToAllRecruitsButton");
		if (applyToAllButton)
			applyToAllButton.SetEnabled(hasLoadout && hasRecruits);
	}
	
	protected void SetRecruitButtonsVisible(bool visible)
	{
		bool show = visible;

		// No box, no box-apply: at a tent these two buttons stay hidden whatever the recruit list says.
		if (IsPurchaseMode())
			show = false;

		array<string> buttonNames = {
			"ApplyToSelectedRecruitButton",
			"ApplyToAllRecruitsButton"
		};

		foreach (string name : buttonNames)
		{
			Widget button = m_wRoot.FindAnyWidget(name);
			if (button)
				button.SetVisible(show);
		}
	}
	
	protected void ApplyLoadoutToSelectedRecruit()
	{
		if (m_SelectedLoadoutName.IsEmpty() || !m_SelectedRecruit)
		{
			Print("[OVT_LoadoutsContext] No recruit selected for loadout application");
			return;
		}
		
		if (!m_EquipmentBox)
		{
			Print("[OVT_LoadoutsContext] No equipment box available for loadout application");
			return;
		}
		
		ApplyLoadoutToRecruit(m_SelectedRecruit);
		CloseLayout();
		// Notification will be sent by the loadout manager after processing
	}
	
	protected void ApplyLoadoutToAllRecruits()
	{
		if (m_SelectedLoadoutName.IsEmpty() || !m_aNearbyRecruits || m_aNearbyRecruits.IsEmpty())
		{
			Print("[OVT_LoadoutsContext] No recruits nearby for loadout application");
			return;
		}
		
		if (!m_EquipmentBox)
		{
			Print("[OVT_LoadoutsContext] No equipment box available for loadout application");
			return;
		}
		
		// Apply loadout to all recruits - each will send its own notification
		foreach (IEntity recruit : m_aNearbyRecruits)
		{
			ApplyLoadoutToRecruit(recruit);
		}
		CloseLayout();
		// Individual notifications will be sent by the loadout manager for each recruit
	}
	
	protected bool ApplyLoadoutToRecruit(IEntity recruitEntity)
	{
		if (!recruitEntity || m_SelectedLoadoutName.IsEmpty() || !m_EquipmentBox)
			return false;
		
		// Apply loadout via server RPC
		OVT_PlayerCommsComponent comms = OVT_Global.GetServer();
		if (comms)
		{
			Print(string.Format("[OVT_LoadoutsContext] Sending RPC to apply loadout to recruit: %1", m_SelectedLoadoutName));
			comms.LoadLoadoutFromBox(m_sPlayerID, m_SelectedLoadoutName, m_EquipmentBox, recruitEntity);
			return true;
		}
		else
		{
			Print("[OVT_LoadoutsContext] No server comms component found");
			return false;
		}
	}
}