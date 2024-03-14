class OVT_StartGameContext : OVT_UIContext
{
	protected FactionManager m_Factions;
	
	protected ref array<FactionKey> m_FactionKeys;
	protected ref array<OVT_KeyButtonComponent> m_FactionCards;
	
	override void OnShow()
	{		
#ifdef PLATFORM_XBOX		
		Widget xbox = m_wRoot.FindAnyWidget("XBOXWarning");
		xbox.SetVisible(true);
#endif
		
		IEntity mode = GetGame().GetGameMode();
		SCR_SaveLoadComponent saveload = SCR_SaveLoadComponent.Cast(mode.FindComponent(SCR_SaveLoadComponent));
		
		Widget startButton = m_wRoot.FindAnyWidget("StartButton");
		SCR_InputButtonComponent action = SCR_InputButtonComponent.Cast(startButton.FindHandler(SCR_InputButtonComponent));
		
		if(action)
			action.m_OnActivated.Insert(StartGame);
											
		m_Factions = GetGame().GetFactionManager();
		int i = 0;
				
		autoptr array<Faction> factions = new array<Faction>;
		m_Factions.GetFactionsList(factions);
		
		Widget of = m_wRoot.FindAnyWidget("OccupyingFactionSpinner");
		SCR_SpinBoxComponent spin = SCR_SpinBoxComponent.Cast(of.FindHandler(SCR_SpinBoxComponent));
		spin.m_OnChanged.Insert(OnSpinFaction);
				
		int selectedFaction = 0;
		
		foreach(Faction fac : factions)
		{
			OVT_Faction faction = OVT_Global.GetFactions().GetOverthrowFactionByKey(fac.GetFactionKey());
			if(!faction) continue;
			if(faction.IsPlayable()) continue;
			
			spin.AddItem(fac.GetUIInfo().GetName(),false,fac);
						
			if(faction.GetFactionKey() == OVT_Global.GetConfig().m_sDefaultOccupyingFaction) selectedFaction = i;
			
			i++;
		}
		spin.SetCurrentItem(selectedFaction);
	}
	
	protected void OnSpinFaction(SCR_SpinBoxComponent spinner, int index)
	{
		Faction data = Faction.Cast(spinner.GetItemData(index));
		OVT_Global.GetConfig().SetOccupyingFaction(data.GetFactionKey());	
	}
	
	protected void StartGame()
	{
		CloseLayout();
		
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		
		mode.DoStartNewGame();
		mode.DoStartGame();
	}
}