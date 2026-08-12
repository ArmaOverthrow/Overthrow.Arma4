//! Widget handler for a single label/value row inside the shared map info panel.
//!
//! ============================================================================================
//! LAYOUT <-> CODE NAME CONTRACT - UI/Layouts/Map/Core/OVT_MapInfoRow.layout
//! ============================================================================================
//!   "RowLabel"  TextWidgetClass   Left-hand caption. Hidden when the row is a full-width line.
//!   "RowValue"  TextWidgetClass   The value or, for an explanatory line, the whole sentence.
//!   "RowIcon"   ImageWidgetClass  Optional leading glyph. Authored "Is Visible" 0 and only shown
//!                                 when Init() is given both an imageset and a quad name.
//!
//! Renaming any of the three in the layout without changing this file produces a row that renders
//! its authored placeholder text and never updates - FindAnyWidget returning null is a silent
//! no-op the compiler cannot see, and it is exactly how map/core's IconLayout (D1) and CloseButton
//! (D2) shipped dead. Q-8 in the feature plan requires this list to be audited name-by-name against
//! the layout on every change.
//! ============================================================================================
//!
//! Modelled on OVT_TownModifierWidgetHandler: cache in HandlerAttached, expose one Init(), and
//! tolerate Init() arriving either side of attachment (CreateWidgets attaches handlers during
//! creation, so in practice HandlerAttached runs first and m_bHasData is false at that point).
class OVT_MapInfoRowHandler : SCR_ScriptedWidgetComponent
{
	protected TextWidget m_wLabel;
	protected TextWidget m_wValue;
	protected ImageWidget m_wIcon;

	protected string m_sLabel;
	protected string m_sValue;
	protected ResourceName m_Imageset;
	protected string m_sIcon;

	//! Whether Init() has been called. Without it HandlerAttached would paint the authored
	//! placeholder text over a row that is about to be filled in.
	protected bool m_bHasData;

	//------------------------------------------------------------------------------------------------
	//! Fills in the row.
	//! \param[in] label Left-hand caption. Pass "" for a full-width explanatory line.
	//! \param[in] value Right-hand value, or the whole sentence when label is empty.
	//! \param[in] imageset Optional imageset for the leading glyph. Both this and icon must be set.
	//! \param[in] icon Optional quad name within imageset.
	void Init(string label, string value, ResourceName imageset = "", string icon = "")
	{
		m_sLabel = label;
		m_sValue = value;
		m_Imageset = imageset;
		m_sIcon = icon;
		m_bHasData = true;

		UpdateDisplay();
	}

	//------------------------------------------------------------------------------------------------
	//! Caches the three contracted widgets.
	//! \param[in] w The widget this handler is attached to
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		if (!w)
			return;

		m_wLabel = TextWidget.Cast(w.FindAnyWidget("RowLabel"));
		m_wValue = TextWidget.Cast(w.FindAnyWidget("RowValue"));
		m_wIcon = ImageWidget.Cast(w.FindAnyWidget("RowIcon"));

		if (m_bHasData)
			UpdateDisplay();
	}

	//------------------------------------------------------------------------------------------------
	//! Paints the cached data. Every widget lookup is null-guarded so a layout that has lost one of
	//! the three degrades to a missing element rather than a script error.
	protected void UpdateDisplay()
	{
		if (m_wLabel)
		{
			if (m_sLabel.IsEmpty())
			{
				m_wLabel.SetVisible(false);
			}
			else
			{
				m_wLabel.SetText(m_sLabel);
				m_wLabel.SetVisible(true);
			}
		}

		if (m_wValue)
		{
			if (m_sValue.IsEmpty())
			{
				m_wValue.SetVisible(false);
			}
			else
			{
				m_wValue.SetText(m_sValue);
				m_wValue.SetVisible(true);
			}
		}

		if (!m_wIcon)
			return;

		if (m_Imageset.IsEmpty() || m_sIcon.IsEmpty())
		{
			m_wIcon.SetVisible(false);
			return;
		}

		m_wIcon.LoadImageFromSet(0, m_Imageset, m_sIcon);
		m_wIcon.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The caption this row was initialised with
	string GetLabel()
	{
		return m_sLabel;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The value this row was initialised with
	string GetValue()
	{
		return m_sValue;
	}
}
