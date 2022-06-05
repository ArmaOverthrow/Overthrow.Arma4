class OVT_PlaceMenu
{
	OVT_PlaceMenuWidgets m_Widgets;
	OVT_UIManagerComponent m_UIManager;
	Widget m_Root;
	
	void OnUpdate()
	{
		OVT_OverthrowConfigComponent config = OVT_OverthrowConfigComponent.GetInstance();
		
		int done = 0;
		foreach(int i, OVT_Placeable placeable : config.m_aPlaceables)
		{
			Widget card = m_Widgets.m_BrowserGrid.FindWidget("PlaceMenu_Card" + i);
			done++;
			
			TextWidget text = TextWidget.Cast(card.FindAnyWidget("EntityName"));
			text.SetText(placeable.name);
			
			TextWidget cost = TextWidget.Cast(card.FindAnyWidget("Cost"));
			cost.SetText("$" + config.GetPlaceableCost(placeable));
			
			ImageWidget img = ImageWidget.Cast(card.FindAnyWidget("Image"));
			img.LoadImageTexture(0, placeable.m_tPreview);
		}
		
		for(int i=done; i < 15; i++)
		{
			Widget card = m_Widgets.m_BrowserGrid.FindWidget("PlaceMenu_Card" + i);
			card.SetVisible(false);
		}
	}
	
	void OVT_PlaceMenu(OVT_PlaceMenuWidgets widgets, Widget root, OVT_UIManagerComponent uimanager)
	{		
		m_Widgets = widgets;
		m_UIManager = uimanager;
		m_Root = root;
	}	
	
}