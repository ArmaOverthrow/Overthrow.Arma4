//------------------------------------------------------------------------------------------------
//! One tab in a tab host's tab row.
//!
//! Instantiated per tab at runtime by an OVT_TabHostContext through
//! workspace.CreateWidgets(ShopMenu_Tab.layout, tabsContainer) - the same dynamic-list pattern the
//! transfer screen uses for its rows. The tab id is the host's own, opaque here.
//!
//! CLICKS ARRIVE THROUGH THE BUTTON'S m_OnClicked INVOKER, NOT THROUGH OnClick. Both input paths
//! funnel into that one invoker - the mouse through SCR_ButtonBaseComponent.OnClick and a gamepad
//! through OnMenuSelect on the focused widget - so subscribing to it once is the only wiring that
//! fires exactly once on either device. OnClick is kept as a fallback for the case where the layout
//! has no button component to subscribe to, so a tab can never end up a dead wire.
//!
//! Selection is drawn rather than toggled: SCR_ButtonBaseComponent's toggle state flips itself on
//! every click and would then have to be corrected from script, so the selected look is applied
//! explicitly by the context after every tab rebuild (the same treatment
//! OVT_PlaceMenuCardComponent uses for its can't-do-this state).
//------------------------------------------------------------------------------------------------
class OVT_ShopMenuTabComponent : SCR_ScriptedWidgetComponent
{
	//! Accent orange (0.761 0.392 0.08), the selected colour used across Overthrow menus.
	protected const int COLOR_SELECTED = 0xFFC26414;

	protected const float OPACITY_SELECTED = 1.0;
	protected const float OPACITY_UNSELECTED = 0.6;

	protected int m_iTabId;
	protected string m_sLabelKey;
	protected OVT_TabHostContext m_Host;
	protected bool m_bSelected;
	protected bool m_bWiredToButton;

	//------------------------------------------------------------------------------------------------
	//! Fills in the tab, wires its click and remembers who to notify.
	//! \param[in] tabId The host's id for the tab this widget selects.
	//! \param[in] labelKey The #OVT- key to draw on the tab.
	//! \param[in] host The menu that owns this tab.
	//! \param[in] selected Whether this tab is the currently active one.
	void Init(int tabId, string labelKey, OVT_TabHostContext host, bool selected = false)
	{
		m_iTabId = tabId;
		m_sLabelKey = labelKey;
		m_Host = host;

		if(m_wRoot)
		{
			TextWidget label = TextWidget.Cast(m_wRoot.FindAnyWidget("TabLabel"));
			if(label) label.SetText(labelKey);
		}

		WireButton();
		SetSelected(selected);
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the selected / unselected look.
	//! \param[in] selected True when this tab is the active one.
	void SetSelected(bool selected)
	{
		m_bSelected = selected;

		if(!m_wRoot) return;

		if(selected)
		{
			m_wRoot.SetOpacity(OPACITY_SELECTED);
		}else{
			m_wRoot.SetOpacity(OPACITY_UNSELECTED);
		}

		TextWidget label = TextWidget.Cast(m_wRoot.FindAnyWidget("TabLabel"));
		if(!label) return;

		if(selected)
		{
			label.SetColor(Color.FromInt(COLOR_SELECTED));
		}else{
			label.SetColor(Color.White);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return The host's id for the tab this widget selects.
	int GetTabId()
	{
		return m_iTabId;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when this tab is drawn as the active one.
	bool IsSelected()
	{
		return m_bSelected;
	}

	//------------------------------------------------------------------------------------------------
	//! Subscribes to the sibling button's click invoker exactly once. Called from Init rather than
	//! HandlerAttached so it does not depend on the order the engine attaches the two handlers.
	protected void WireButton()
	{
		if(m_bWiredToButton) return;
		if(!m_wRoot) return;

		SCR_ButtonComponent button = SCR_ButtonComponent.Cast(m_wRoot.FindHandler(SCR_ButtonComponent));
		if(!button) return;

		button.m_OnClicked.Insert(OnTabClicked);
		m_bWiredToButton = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Click handler, from either input device.
	//!
	//! DEFERRED BY ONE CALL-QUEUE TICK ON PURPOSE. Selecting a tab refreshes the menu, and a refresh
	//! may rebuild the tab row - destroying this very widget, and with it this handler, while the
	//! click event is still on the stack. The call queue holds its own reference, so the work happens
	//! safely after the event has returned. (Vanilla uses the same CallLater(..., 0) trick in
	//! SCR_ButtonBaseComponent.HandlerAttached for an init-order problem of the same shape.)
	protected void OnTabClicked(SCR_ButtonBaseComponent button)
	{
		GetGame().GetCallqueue().Remove(Activate);
		GetGame().GetCallqueue().CallLater(Activate, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Tells the menu this tab was picked.
	void Activate()
	{
		if(!m_Host) return;

		m_Host.SelectTabId(m_iTabId);
	}

	//------------------------------------------------------------------------------------------------
	//! Fallback only: with a button component present the invoker above already handled this click,
	//! and handling it twice would rebuild the grid twice per click.
	override bool OnClick(Widget w, int x, int y, int button)
	{
		super.OnClick(w, x, y, button);
		if (button != 0)
			return false;

		if(m_bWiredToButton)
			return false;

		if(!m_Host)
			return false;

		Activate();

		return true;
	}
}
