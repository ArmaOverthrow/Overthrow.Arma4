class OVT_StartGameContext : OVT_UIContext
{
	protected FactionManager m_Factions;
	
	protected ref array<FactionKey> m_FactionKeys;
	protected ref array<OVT_KeyButtonComponent> m_FactionCards;
	
	override void OnShow()
	{		
		IEntity mode = GetGame().GetGameMode();
		SCR_SaveLoadComponent saveload = SCR_SaveLoadComponent.Cast(mode.FindComponent(SCR_SaveLoadComponent));
		
		Widget startButton = m_wRoot.FindAnyWidget("StartButton");
		ButtonActionComponent action = ButtonActionComponent.Cast(startButton.FindHandler(ButtonActionComponent));
		
		if(action)
			action.GetOnAction().Insert(StartGame);
											
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
			OVT_Faction faction = OVT_Global.GetFactions().GetOverthrowFactionByKey(fac.GetFactionKey());
			if(!faction) continue;
			if(faction.IsPlayable()) continue;
			
			spin.AddItem(fac.GetUIInfo().GetName(),fac);
						
			if(faction.GetFactionKey() == m_Config.m_sDefaultOccupyingFaction) selectedFaction = i;
			
			i++;
		}
		spin.SetCurrentItem(selectedFaction);
	}
	
	protected void OnSpinFaction(SCR_SpinBoxComponent spinner, int index)
	{
		Faction data = Faction.Cast(spinner.GetItemData(index));
		m_Config.SetOccupyingFaction(data.GetFactionKey());	
	}
	
	protected void StartGame()
	{
		CloseLayout();
		
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		
		mode.DoStartNewGame();
		mode.DoStartGame();
	}
}