class OVT_StartGameContext : OVT_UIContext
{
	protected FactionManager m_Factions;
	
	protected ref array<FactionKey> m_FactionKeys;
	protected ref array<OVT_KeyButtonComponent> m_FactionCards;
	
	override void OnShow()
	{		
		Widget startButton = m_wRoot.FindAnyWidget("StartButton");
		SCR_NavigationButtonComponent action = SCR_NavigationButtonComponent.Cast(startButton.FindHandler(SCR_NavigationButtonComponent));
		
		if(action)
			action.m_OnActivated.Insert(StartGame);
					
		m_Factions = GetGame().GetFactionManager();
		int i = 0;
		
		m_FactionKeys = new array<FactionKey>;
		m_FactionCards = new array<OVT_KeyButtonComponent>;
		
		array<Faction> factions = new array<Faction>;
		m_Factions.GetFactionsList(factions);
		
		foreach(Faction fac : factions)
		{
			OVT_Faction faction = OVT_Faction.Cast(fac);
			if(!faction) continue;
			if(faction.IsPlayable()) continue;
			m_FactionKeys.Insert(faction.GetFactionKey());
			
			ButtonWidget card = ButtonWidget.Cast(m_wRoot.FindAnyWidget("OccupyingFaction_Card" + i));
			if(card)
			{				
				OVT_KeyButtonComponent button = OVT_KeyButtonComponent.Cast(card.FindHandler(OVT_KeyButtonComponent));
				if(button)
				{
					m_FactionCards.Insert(button);
					string key = faction.GetFactionKey();
					button.SetData(key);
					button.m_OnClicked.Insert(OnSetFaction);
					if(m_Config.m_sOccupyingFaction == key)
					{
						button.SetToggled(true);
					}else{
						button.SetToggled(false);
					}
				}
				ImageWidget image =  ImageWidget.Cast(card.FindAnyWidget("Image"));
				if(image)
				{
					image.LoadImageTexture(0, faction.GetUIInfo().GetIconPath());
				}
			}
			
			i++;
			if(i > 1) break;
		}
	}
	
	protected void OnSetFaction(SCR_ButtonBaseComponent button)
	{
		foreach(OVT_KeyButtonComponent b : m_FactionCards)
		{
			if(b != button) b.SetToggled(false);
		}
		button.SetToggled(true);
		OVT_KeyButtonComponent btn = OVT_KeyButtonComponent.Cast(button);
		m_Config.m_sOccupyingFaction = btn.GetData();		
	}
	
	protected void StartGame()
	{
		CloseLayout();
		
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		
		if(mode)
		{
			mode.StartGame();
		}
	}
}