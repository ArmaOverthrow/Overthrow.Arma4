//! Widget handler for one row of the map layer-filter panel.
//!
//! ============================================================================================
//! LAYOUT <-> CODE NAME CONTRACT - UI/Layouts/Map/Core/OVT_MapLayerRow.layout
//! ============================================================================================
//!   "RowHighlight" PanelWidgetClass   Row-wide focus/hover wash. Authored Opacity 0.
//!   "RowIcon"      ImageWidgetClass   Leading glyph. Authored "Is Visible" 0 and only shown when
//!                                     Init() is given BOTH an imageset and a quad name - the
//!                                     canvas-overlay and player rows have neither.
//!   "RowLabel"     TextWidgetClass    The plural, localized category or layer name.
//!   "RowCheckbox"  ButtonWidgetClass  Inherits WLib_Checkbox.layout. Its SCR_CheckboxComponent
//!                                     override MUST keep the base layout's component GUID, or a
//!                                     second unconfigured component is created and the checkbox
//!                                     goes dead.
//!
//! Renaming any of the four in the layout without changing this file produces a row that renders
//! its authored placeholder and never updates - FindAnyWidget returning null is a silent no-op the
//! compiler cannot see, and it is how map/core's IconLayout and CloseButton both shipped dead.
//! ============================================================================================
//!
//! A ROW NEVER MUTATES STATE ITSELF. It reports the key and the new value to the owning
//! OVT_MapLayersUI and does nothing else, so there is exactly one place that knows how a key maps
//! onto a location type, a canvas layer or the player markers. A row that applied its own change
//! would also have to know how to ask the map to re-sweep, which is the owner's job.
class OVT_MapLayerRowComponent : SCR_ScriptedWidgetComponent
{
	//! Widget names, kept as constants so the contract above and the lookups below cannot drift.
	protected const string WIDGET_HIGHLIGHT = "RowHighlight";
	protected const string WIDGET_ICON = "RowIcon";
	protected const string WIDGET_LABEL = "RowLabel";
	protected const string WIDGET_CHECKBOX = "RowCheckbox";

	//! Opacity of the row-wide wash while the row is focused or hovered.
	protected const float HIGHLIGHT_ACTIVE = 0.35;

	//! Opacity of the row-wide wash otherwise.
	protected const float HIGHLIGHT_IDLE = 0;

	protected string m_sKey;
	protected OVT_MapLayersUI m_Owner;

	protected Widget m_wHighlight;
	protected ImageWidget m_wIcon;
	protected TextWidget m_wLabel;
	protected SCR_CheckboxComponent m_Checkbox;

	//! Whether m_OnChanged has already been subscribed. The panel rebuilds its rows on every open,
	//! but a row widget can outlive one Init() call in a rebuild that reuses it, and a second
	//! subscription would report every toggle twice - once applied, once immediately re-applied.
	protected bool m_bWired;

	//------------------------------------------------------------------------------------------------
	//! Fills in the row and wires its checkbox.
	//!
	//! THE WIRING LIVES HERE AND NOT IN HandlerAttached(). HandlerAttached runs before the owner has
	//! passed anything in, and its ordering against the sibling handlers on this widget tree - in
	//! particular the SCR_CheckboxComponent this method subscribes to - is not guaranteed.
	//!
	//! The initial checkbox value is written BEFORE m_OnChanged is subscribed, deliberately.
	//! SCR_CheckboxComponent.SetChecked routes through SetCurrentItem, which toggles an element,
	//! which raises m_OnChanged - so wiring first and then seeding the value would report a
	//! player-driven toggle for every row that starts hidden, and the panel would switch the map
	//! back on the moment it opened.
	//!
	//! \param[in] key Namespaced preference key from OVT_MapLayerPrefsStore.TypeKey / LayerKey
	//! \param[in] label Localization key (or literal) for the row's name
	//! \param[in] imageset Imageset for the leading glyph, or "" for a row with no icon source
	//! \param[in] icon Quad name within imageset, or "" for a row with no icon source
	//! \param[in] visible The LIVE state read from the object this row controls, never from a store
	//! \param[in] owner The panel this row reports back to
	void Init(string key, string label, ResourceName imageset, string icon, bool visible, OVT_MapLayersUI owner)
	{
		m_sKey = key;
		m_Owner = owner;

		if (!m_wRoot)
		{
			Print("[Overthrow] OVT_MapLayerRowComponent: no root widget for row '" + key + "', the row cannot be filled in", LogLevel.ERROR);
			return;
		}

		ResolveWidgets();

		if (m_wLabel)
			m_wLabel.SetText(label);

		ApplyIcon(imageset, icon);
		SetHighlight(false);

		if (!m_Checkbox)
			return;

		// Seed first, subscribe second - see the note above.
		m_Checkbox.SetChecked(visible, false, false);

		WireCheckbox();
	}

	//------------------------------------------------------------------------------------------------
	//! Caches the four contracted widgets, ERROR-logging by name whichever one is missing.
	protected void ResolveWidgets()
	{
		m_wHighlight = m_wRoot.FindAnyWidget(WIDGET_HIGHLIGHT);
		if (!m_wHighlight)
			Print("[Overthrow] OVT_MapLayerRowComponent: widget '" + WIDGET_HIGHLIGHT + "' not found in the layer row layout, the row will show no focus highlight", LogLevel.ERROR);

		m_wIcon = ImageWidget.Cast(m_wRoot.FindAnyWidget(WIDGET_ICON));
		if (!m_wIcon)
			Print("[Overthrow] OVT_MapLayerRowComponent: widget '" + WIDGET_ICON + "' not found in the layer row layout, rows will show no icons", LogLevel.ERROR);

		m_wLabel = TextWidget.Cast(m_wRoot.FindAnyWidget(WIDGET_LABEL));
		if (!m_wLabel)
			Print("[Overthrow] OVT_MapLayerRowComponent: widget '" + WIDGET_LABEL + "' not found in the layer row layout, rows will show their authored placeholder text", LogLevel.ERROR);

		m_Checkbox = SCR_CheckboxComponent.GetCheckboxComponent(WIDGET_CHECKBOX, m_wRoot);
		if (!m_Checkbox)
			Print("[Overthrow] OVT_MapLayerRowComponent: no SCR_CheckboxComponent on widget '" + WIDGET_CHECKBOX + "' in the layer row layout, the row cannot be toggled", LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	//! Shows the leading glyph, or hides it when this row has no icon source.
	//! Canvas overlays and the player-marker row are named things rather than icon categories, so
	//! they legitimately arrive with nothing to draw and must not leave a blank square behind.
	//! \param[in] imageset Imageset holding the glyph
	//! \param[in] icon Quad name within imageset
	protected void ApplyIcon(ResourceName imageset, string icon)
	{
		if (!m_wIcon)
			return;

		if (imageset.IsEmpty() || icon.IsEmpty())
		{
			m_wIcon.SetVisible(false);
			return;
		}

		m_wIcon.LoadImageFromSet(0, imageset, icon);
		m_wIcon.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Subscribes to the checkbox exactly once.
	protected void WireCheckbox()
	{
		if (m_bWired)
			return;

		if (!m_Checkbox)
			return;

		m_Checkbox.m_OnChanged.Insert(OnCheckboxChanged);
		m_bWired = true;
	}

	//------------------------------------------------------------------------------------------------
	//! SCR_CheckboxComponent.m_OnChanged handler. Arguments are documented on
	//! SCR_ChangeableComponentBase.m_OnChanged: for a checkbox they are the component and the new
	//! checked state.
	//! \param[in] checkbox The checkbox that changed
	//! \param[in] state True when the row was switched on
	protected void OnCheckboxChanged(SCR_CheckboxComponent checkbox, bool state)
	{
		if (!m_Owner)
			return;

		m_Owner.OnRowToggled(m_sKey, state);
	}

	//------------------------------------------------------------------------------------------------
	//! Paints the row-wide focus wash.
	//! \param[in] active True while the row is focused or hovered
	protected void SetHighlight(bool active)
	{
		if (!m_wHighlight)
			return;

		if (active)
			m_wHighlight.SetOpacity(HIGHLIGHT_ACTIVE);
		else
			m_wHighlight.SetOpacity(HIGHLIGHT_IDLE);
	}

	//------------------------------------------------------------------------------------------------
	//! A gamepad shows which row it is on ONLY through this highlight, so it is driven from four
	//! events rather than two: focus is the pad's cursor, hover is the mouse's, and the two have to
	//! agree. Every widget-library handler in the chain returns false from these events, so a focus
	//! change on the child checkbox propagates up to this handler on the row root.
	//! \param[in] w The widget that gained focus
	//! \param[in] x Cursor x
	//! \param[in] y Cursor y
	//! \return False, so the event keeps propagating exactly as the widget library expects
	override bool OnFocus(Widget w, int x, int y)
	{
		SetHighlight(true);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] w The widget that lost focus
	//! \param[in] x Cursor x
	//! \param[in] y Cursor y
	//! \return False, so the event keeps propagating
	override bool OnFocusLost(Widget w, int x, int y)
	{
		SetHighlight(false);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] w The widget the cursor entered
	//! \param[in] x Cursor x
	//! \param[in] y Cursor y
	//! \return False, so the event keeps propagating
	override bool OnMouseEnter(Widget w, int x, int y)
	{
		SetHighlight(true);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Leaving for a child widget is not leaving the row - the cursor moving from the label onto the
	//! checkbox must not blink the highlight off.
	//! \param[in] w The widget the cursor left
	//! \param[in] enterW The widget the cursor entered
	//! \param[in] x Cursor x
	//! \param[in] y Cursor y
	//! \return False, so the event keeps propagating
	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		if (IsInsideRow(enterW))
			return false;

		SetHighlight(false);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a widget is this row or sits underneath it.
	//! Copied rather than inherited: SCR_WLibComponentBase has the same walk, but this handler
	//! extends SCR_ScriptedWidgetComponent and reparenting it onto the interactive-element base
	//! would drag that class's borders, backgrounds and sounds onto a plain layout row.
	//! \param[in] widget The widget to test
	//! \return True when widget is the row root or one of its descendants
	protected bool IsInsideRow(Widget widget)
	{
		if (!m_wRoot || !widget)
			return false;

		Widget current = widget;
		while (current)
		{
			if (current == m_wRoot)
				return true;

			current = current.GetParent();
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The widget a gamepad should be focused on to be "on" this row.
	//!
	//! The checkbox rather than the row root: it is the only focusable widget in the row, it carries
	//! the widget library's own focus visuals, and it is what the pad's left/right acts on. One
	//! focus stop per row is the point - a focusable row root as well would double the number of
	//! stops a player has to walk through.
	//! \return The checkbox widget, or the row root when the checkbox could not be resolved
	Widget GetFocusWidget()
	{
		if (m_Checkbox)
		{
			Widget checkboxRoot = m_Checkbox.GetRootWidget();
			if (checkboxRoot)
				return checkboxRoot;
		}

		return m_wRoot;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The namespaced preference key this row controls
	string GetKey()
	{
		return m_sKey;
	}
}
