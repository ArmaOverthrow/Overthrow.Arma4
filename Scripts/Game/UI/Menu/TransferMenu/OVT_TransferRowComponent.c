//------------------------------------------------------------------------------------------------
//! One browsable row in a transfer screen's list column.
//!
//! Clicks arrive through the button's m_OnClicked invoker, not through OnClick: both input paths
//! funnel into that one invoker (mouse through SCR_ButtonBaseComponent.OnClick, gamepad through
//! MenuSelect on the focused widget), so subscribing once fires exactly once on either device.
//! OnClick stays as a fallback for a layout with no button component, guarded by m_bWiredToButton so
//! it can never run a second time on top of the invoker.
//!
//! Focus is reported separately from clicks: SCR_ButtonComponent has m_bMouseOverToFocus, so a mouse
//! hover and a d-pad move both land here, which is what keeps the context's remembered pane/index
//! honest without the context polling anything.
//------------------------------------------------------------------------------------------------
class OVT_TransferRowComponent : SCR_ScriptedWidgetComponent
{
	//! Accent orange (0.761 0.392 0.08), the selected colour used across Overthrow menus.
	protected const int COLOR_SELECTED = 0xFFC26414;

	protected const float OPACITY_ENABLED = 1.0;
	protected const float OPACITY_DISABLED = 0.5;

	protected OVT_TransferEntry m_Entry;
	protected OVT_TransferContext m_Context;
	protected SCR_ButtonComponent m_Button;
	protected int m_iIndex = -1;
	protected bool m_bSelected;
	protected bool m_bWiredToButton;

	//------------------------------------------------------------------------------------------------
	//! Fills in the row, wires its click and focus, and remembers who to notify.
	//! \param[in] entry The row's data. Never held by the cart - the cart copies what it needs.
	//! \param[in] index Position in the currently drawn list, which is what the context remembers.
	//! \param[in] context The transfer screen that owns this row.
	//! \param[in] selected Whether this row is the currently selected one.
	void Init(OVT_TransferEntry entry, int index, OVT_TransferContext context, bool selected = false)
	{
		m_Entry = entry;
		m_iIndex = index;
		m_Context = context;

		Draw();
		WireButton();
		SetSelected(selected);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The entry this row draws.
	OVT_TransferEntry GetEntry()
	{
		return m_Entry;
	}

	//------------------------------------------------------------------------------------------------
	//! \return This row's index in the list the context last drew.
	int GetIndex()
	{
		return m_iIndex;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the selected / unselected look.
	//! \param[in] selected True when this row is the active one.
	void SetSelected(bool selected)
	{
		m_bSelected = selected;

		if(!m_wRoot) return;

		TextWidget name = TextWidget.Cast(m_wRoot.FindAnyWidget("RowName"));
		if(!name) return;

		if(selected)
		{
			name.SetColor(Color.FromInt(COLOR_SELECTED));
		}else{
			name.SetColor(Color.White);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Removes everything Init inserted. Called before the context destroys the row, so a rebuild
	//! mid-interaction cannot leave a queued Activate pointing at a dead widget.
	void Cleanup()
	{
		GetGame().GetCallqueue().Remove(Activate);

		if(m_Button)
		{
			m_Button.m_OnClicked.Remove(OnRowClicked);
			m_Button.m_OnFocus.Remove(OnRowFocused);
		}

		m_Button = null;
		m_Context = null;
		m_Entry = null;
		m_bWiredToButton = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Draws the row from its entry: one of the two image widgets, the name, and the value column.
	protected void Draw()
	{
		if(!m_wRoot) return;

		TextWidget name = TextWidget.Cast(m_wRoot.FindAnyWidget("RowName"));
		TextWidget value = TextWidget.Cast(m_wRoot.FindAnyWidget("RowValue"));

		if(!m_Entry)
		{
			if(name) name.SetText("");
			if(value) value.SetText("");
			return;
		}

		if(name) name.SetText(m_Entry.m_sDisplayName);
		if(value) value.SetText(OVT_TransferContext.FormatValue(m_Entry.m_iValue, m_Entry.m_eValueKind));

		if(m_Entry.m_bEnabled)
		{
			m_wRoot.SetOpacity(OPACITY_ENABLED);
		}else{
			m_wRoot.SetOpacity(OPACITY_DISABLED);
		}

		DrawImage();
	}

	//------------------------------------------------------------------------------------------------
	//! Shows exactly one of the two image widgets - the whole of the "items and resources" story.
	protected void DrawImage()
	{
		ItemPreviewWidget preview = ItemPreviewWidget.Cast(m_wRoot.FindAnyWidget("RowPreview"));
		ImageWidget image = ImageWidget.Cast(m_wRoot.FindAnyWidget("RowImage"));

		bool isPrefab = m_Entry.m_eImageKind == EOVT_TransferImageKind.PREFAB;

		if(preview) preview.SetVisible(isPrefab);
		if(image) image.SetVisible(!isPrefab);

		if(m_Entry.m_sImage.IsEmpty()) return;

		if(isPrefab)
		{
			if(!preview) return;

			ChimeraWorld world = GetGame().GetWorld();
			if(!world) return;

			ItemPreviewManagerEntity manager = world.GetItemPreviewManager();
			if(!manager) return;

			preview.SetResolutionScale(1, 1);
			manager.SetPreviewItemFromPrefab(preview, m_Entry.m_sImage);
			return;
		}

		if(!image) return;

		if(image.LoadImageTexture(0, m_Entry.m_sImage))
			image.SetImage(0);
	}

	//------------------------------------------------------------------------------------------------
	//! Subscribes to the sibling button's invokers exactly once. Called from Init rather than
	//! HandlerAttached so it does not depend on the order the engine attaches the two handlers.
	protected void WireButton()
	{
		if(m_bWiredToButton) return;
		if(!m_wRoot) return;

		m_Button = SCR_ButtonComponent.Cast(m_wRoot.FindHandler(SCR_ButtonComponent));
		if(!m_Button) return;

		m_Button.m_OnClicked.Insert(OnRowClicked);
		m_Button.m_OnFocus.Insert(OnRowFocused);
		m_bWiredToButton = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Click handler, from either input device. Deferred by one call-queue tick: selecting a row may
	//! redraw the list and destroy this widget while the click event is still on the stack.
	protected void OnRowClicked(SCR_ButtonBaseComponent button)
	{
		GetGame().GetCallqueue().Remove(Activate);
		GetGame().GetCallqueue().CallLater(Activate, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Focus handler. Runs on mouse-over as well as on a d-pad move, which is how the context knows
	//! which pane and index the player is on without polling.
	protected void OnRowFocused(Widget w)
	{
		if(!m_Context) return;

		m_Context.SelectListIndex(m_iIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! Tells the screen this row was picked. Selects it AND adds one to the cart - only the click
	//! invoker reaches here, never OnRowFocused, so a d-pad walk still just selects.
	void Activate()
	{
		if(!m_Context) return;

		m_Context.ActivateListIndex(m_iIndex);
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
