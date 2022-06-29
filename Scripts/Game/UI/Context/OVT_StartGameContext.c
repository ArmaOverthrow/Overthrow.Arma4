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
				
		array<Faction> factions = new array<Faction>;
		m_Factions.GetFactionsList(factions);
		
		Widget of = m_wRoot.FindAnyWidget("OccupyingFactionSpinner");
		SCR_SpinBoxComponent spin = SCR_SpinBoxComponent.Cast(of.FindHandler(SCR_SpinBoxComponent));
		spin.m_OnChanged.Insert(OnSpinFaction);
				
		int selectedFaction = 0;
		
		foreach(Faction fac : factions)
		{
			OVT_Faction faction = OVT_Faction.Cast(fac);
			if(!faction) continue;
			if(faction.IsPlayable()) continue;
			
			spin.AddItem(faction.GetUIInfo().GetName(),faction);
						
			if(faction.GetFactionKey() == m_Config.m_sDefaultOccupyingFaction) selectedFaction = i;
			
			i++;
			if(i > 1) break;
		}
		Print(selectedFaction);
		spin.SetCurrentItem(selectedFaction);
	}
	
	protected void OnSpinFaction(SCR_SpinBoxComponent spinner, int index)
	{
		OVT_Faction data = OVT_Faction.Cast(spinner.GetItemData(index));
		m_Config.SetOccupyingFaction(data.GetFactionKey());	
	}
	
	protected void StartGame()
	{
		CloseLayout();
		
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		
		if(mode)
		{
			mode.StartGame();
		}else{
			Print("Game mode error");
		}
	}
}