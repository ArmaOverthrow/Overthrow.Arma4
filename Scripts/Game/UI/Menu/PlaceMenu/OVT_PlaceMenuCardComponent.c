class OVT_PlaceMenuCardComponent : SCR_ScriptedWidgetComponent
{
	protected OVT_Placeable m_Placeable;
	protected OVT_PlaceContext m_Context;
	
	void Init(OVT_Placeable placeable, OVT_PlaceContext context)
	{
		m_Placeable = placeable;
		m_Context = context;
		
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		
		TextWidget text = TextWidget.Cast(m_wRoot.FindAnyWidget("EntityName"));
		text.SetText(placeable.name);
		
		TextWidget cost = TextWidget.Cast(m_wRoot.FindAnyWidget("Cost"));
		cost.SetText("$" + config.GetPlaceableCost(placeable));
		
		ImageWidget img = ImageWidget.Cast(m_wRoot.FindAnyWidget("Image"));
		img.LoadImageTexture(0, placeable.m_tPreview);
	}
	
	override bool OnClick(Widget w, int x, int y, int button)
	{
		super.OnClick(w, x, y, button);
		if (button != 0)
			return false;
		
		m_Context.StartPlace(m_Placeable);
		
		return true;
	}
}