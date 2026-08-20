//------------------------------------------------------------------------------------------------
//! One ordered line in a transfer screen's cart column.
//!
//! Same wiring shape as OVT_TransferRowComponent: m_OnClicked for activation, m_OnFocus for
//! tracking, a guarded OnClick fallback, and a one-tick deferral so a rebuild triggered by the click
//! cannot destroy this widget inside its own handler. Focusing a line is what flips the three
//! quantity buttons from Add to Remove.
//------------------------------------------------------------------------------------------------
class OVT_TransferCartLineComponent : SCR_ScriptedWidgetComponent
{
	//! Accent orange (0.761 0.392 0.08), the selected colour used across Overthrow menus.
	protected const int COLOR_SELECTED = 0xFFC26414;

	protected OVT_TransferCartLine m_Line;
	protected OVT_TransferContext m_Context;
	protected SCR_ButtonComponent m_Button;
	protected int m_iIndex = -1;
	protected bool m_bSelected;
	protected bool m_bWiredToButton;

	//------------------------------------------------------------------------------------------------
	//! Fills in the line, wires its click and focus, and remembers who to notify.
	//! \param[in] line The cart line this widget draws.
	//! \param[in] index Position in the cart, which is what the context remembers.
	//! \param[in] context The transfer screen that owns this line.
	//! \param[in] selected Whether this line is the currently selected one.
	void Init(OVT_TransferCartLine line, int index, OVT_TransferContext context, bool selected = false)
	{
		m_Line = line;
		m_iIndex = index;
		m_Context = context;

		Draw();
		WireButton();
		SetSelected(selected);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The cart line this widget draws.
	OVT_TransferCartLine GetLine()
	{
		return m_Line;
	}

	//------------------------------------------------------------------------------------------------
	//! \return This line's index in the cart the context last drew.
	int GetIndex()
	{
		return m_iIndex;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the selected / unselected look.
	//! \param[in] selected True when this line is the active one.
	void SetSelected(bool selected)
	{
		m_bSelected = selected;

		if(!m_wRoot) return;

		TextWidget name = TextWidget.Cast(m_wRoot.FindAnyWidget("LineName"));
		if(!name) return;

		if(selected)
		{
			name.SetColor(Color.FromInt(COLOR_SELECTED));
		}else{
			name.SetColor(Color.White);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Removes everything Init inserted.
	void Cleanup()
	{
		GetGame().GetCallqueue().Remove(Activate);

		if(m_Button)
		{
			m_Button.m_OnClicked.Remove(OnLineClicked);
			m_Button.m_OnFocus.Remove(OnLineFocused);
		}

		m_Button = null;
		m_Context = null;
		m_Line = null;
		m_bWiredToButton = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Draws the line: name, ordered quantity, and the line's contribution to the running total.
	protected void Draw()
	{
		if(!m_wRoot) return;

		TextWidget name = TextWidget.Cast(m_wRoot.FindAnyWidget("LineName"));
		TextWidget quantity = TextWidget.Cast(m_wRoot.FindAnyWidget("LineQuantity"));
		TextWidget value = TextWidget.Cast(m_wRoot.FindAnyWidget("LineValue"));

		if(!m_Line)
		{
			if(name) name.SetText("");
			if(quantity) quantity.SetText("");
			if(value) value.SetText("");
			return;
		}

		if(name) name.SetText(m_Line.m_sDisplayName);
		if(quantity) quantity.SetText("x" + m_Line.m_iQuantity.ToString());

		if(!value) return;

		// A QUANTITY line's value column would just repeat the quantity, so it is left blank there.
		if(m_Line.m_eValueKind == EOVT_TransferValueKind.PRICE)
		{
			value.SetVisible(true);
			value.SetText(OVT_TransferContext.FormatValue(m_Line.m_iQuantity * m_Line.m_iUnitValue, m_Line.m_eValueKind));
		}else{
			value.SetVisible(false);
			value.SetText("");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Subscribes to the sibling button's invokers exactly once.
	protected void WireButton()
	{
		if(m_bWiredToButton) return;
		if(!m_wRoot) return;

		m_Button = SCR_ButtonComponent.Cast(m_wRoot.FindHandler(SCR_ButtonComponent));
		if(!m_Button) return;

		m_Button.m_OnClicked.Insert(OnLineClicked);
		m_Button.m_OnFocus.Insert(OnLineFocused);
		m_bWiredToButton = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Click handler, from either input device. Deferred by one call-queue tick because selecting a
	//! line may redraw the cart and destroy this widget while the click event is still on the stack.
	protected void OnLineClicked(SCR_ButtonBaseComponent button)
	{
		GetGame().GetCallqueue().Remove(Activate);
		GetGame().GetCallqueue().CallLater(Activate, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Focus handler - this is what flips the quantity buttons to Remove.
	protected void OnLineFocused(Widget w)
	{
		if(!m_Context) return;

		m_Context.SelectCartIndex(m_iIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! Tells the screen this line was picked.
	void Activate()
	{
		if(!m_Context) return;

		m_Context.SelectCartIndex(m_iIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! Fallback only: with a button component present the invoker above already handled this click.
	override bool OnClick(Widget w, int x, int y, int button)
	{
		super.OnClick(w, x, y, button);
		if (button != 0)
			return false;

		if(m_bWiredToButton)
			return false;

		if(!m_Context)
			return false;

		Activate();

		return true;
	}
}
