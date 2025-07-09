class OVT_PlaceMenuCardComponent : SCR_ScriptedWidgetComponent
{
	protected OVT_Placeable m_Placeable;
	protected OVT_PlaceContext m_Context;
	protected bool m_bCanPlace;
	protected string m_sRestrictionReason;
	
	void Init(OVT_Placeable placeable, OVT_PlaceContext context, bool canPlace = true, string reason = "")
	{
		m_Placeable = placeable;
		m_Context = context;
		m_bCanPlace = canPlace;
		m_sRestrictionReason = reason;
		
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		
		TextWidget text = TextWidget.Cast(m_wRoot.FindAnyWidget("EntityName"));
		text.SetText(placeable.m_sTitle);
		
		TextWidget cost = TextWidget.Cast(m_wRoot.FindAnyWidget("Cost"));
		cost.SetText("$" + config.GetPlaceableCost(placeable));
		
		ImageWidget img = ImageWidget.Cast(m_wRoot.FindAnyWidget("Image"));
		img.LoadImageTexture(0, placeable.m_tPreview);
		
		TextWidget desc = TextWidget.Cast(m_wRoot.FindAnyWidget("EntityDescription"));
		if (!m_bCanPlace && m_sRestrictionReason != "")
		{
			// Show restriction reason in description
			desc.SetText(placeable.m_sDescription + "\n\n" + m_sRestrictionReason);
		}
		else
		{
			desc.SetText(placeable.m_sDescription);
		}
		
		// Visual feedback for non-placeable items
		if (!m_bCanPlace)
		{
			m_wRoot.SetOpacity(0.5);
			m_wRoot.SetColor(Color.FromInt(0xFFFF4444)); // Red tint
		}
		else
		{
			m_wRoot.SetOpacity(1.0);
			m_wRoot.SetColor(Color.White);
		}
	}
	
	void InitRemoveCard(OVT_PlaceContext context, ResourceName removeIcon = "")
	{
		m_Context = context;
		m_bCanPlace = true;
		m_sRestrictionReason = "";
		
		TextWidget text = TextWidget.Cast(m_wRoot.FindAnyWidget("EntityName"));
		text.SetText("#OVT-Remove");
		
		TextWidget cost = TextWidget.Cast(m_wRoot.FindAnyWidget("Cost"));
		cost.SetText(""); // No cost for removal
		
		ImageWidget img = ImageWidget.Cast(m_wRoot.FindAnyWidget("Image"));
		if (removeIcon != "")
		{
			img.LoadImageTexture(0, removeIcon);
		}
		else
		{
			img.LoadImageTexture(0, "");
		}
		
		TextWidget desc = TextWidget.Cast(m_wRoot.FindAnyWidget("EntityDescription"));
		desc.SetText("#OVT-RemoveDescription");
		
		// Use normal styling (no special coloring)
		m_wRoot.SetOpacity(1.0);
		m_wRoot.SetColor(Color.White);
	}
	
	override bool OnClick(Widget w, int x, int y, int button)
	{
		super.OnClick(w, x, y, button);
		if (button != 0)
			return false;
		
		if(!m_Context)
			return false;
		
		// Check if this is the remove card (no placeable set)
		if (!m_Placeable)
		{
			m_Context.StartRemovalMode();
			return true;
		}
		
		if (!m_bCanPlace)
		{
			// Show restriction reason instead of attempting to place
			m_Context.ShowHint(m_sRestrictionReason);
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ERROR);
			return true;
		}
		
		m_Context.StartPlace(m_Placeable);
		
		return true;
	}
}