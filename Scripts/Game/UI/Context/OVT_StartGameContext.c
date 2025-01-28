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
		SCR_SpinBoxComponent ofSpin = SCR_SpinBoxComponent.Cast(of.FindHandler(SCR_SpinBoxComponent));
		ofSpin.m_OnChanged.Insert(OnSpinOccupyingFaction);
		
		Widget sf = m_wRoot.FindAnyWidget("SupportingFactionSpinner");
		SCR_SpinBoxComponent sfSpin = SCR_SpinBoxComponent.Cast(sf.FindHandler(SCR_SpinBoxComponent));
		sfSpin.m_OnChanged.Insert(OnSpinSupportingFaction);
				
		int selectedOccupyingFaction = 0;
		int selectedSupportingFaction = 0;
		
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		
		foreach(Faction fac : factions)
		{
			OVT_Faction faction = OVT_Global.GetFactions().GetOverthrowFactionByKey(fac.GetFactionKey());
			if(!faction) continue;
			if(faction.IsPlayable()) continue;
			if(faction.GetFactionKey() == "CIV") continue;
			
			ofSpin.AddItem(fac.GetUIInfo().GetName(),false,fac);
			sfSpin.AddItem(fac.GetUIInfo().GetName(),false,fac);
						
			if(faction.GetFactionKey() == config.m_sDefaultOccupyingFaction) selectedOccupyingFaction = i;
			if(faction.GetFactionKey() == config.m_sDefaultSupportingFaction) selectedSupportingFaction = i;
			
			i++;
		}
		ofSpin.SetCurrentItem(selectedOccupyingFaction);
		sfSpin.SetCurrentItem(selectedSupportingFaction);
		
		Widget diff = m_wRoot.FindAnyWidget("DifficultySpinner");
		SCR_SpinBoxComponent spin = SCR_SpinBoxComponent.Cast(diff.FindHandler(SCR_SpinBoxComponent));
		spin.m_OnChanged.Insert(OnSpinDifficulty);
		
		foreach(OVT_DifficultySettings preset : config.m_aDifficultyPresets)
		{
			spin.AddItem(preset.name, false, preset);			
		}
		
		Widget description = m_wRoot.FindAnyWidget("DifficultyDescription");
		TextWidget text = TextWidget.Cast(description);
		
		if (RplSession.Mode() == RplMode.None)
		{
			//Default to "Easy" in single player
			spin.SetCurrentItem(0);
			OVT_DifficultySettings preset = config.m_aDifficultyPresets.Get(0);
			text.SetText(preset.description);
			config.m_Difficulty = preset;
		}else{
			spin.SetCurrentItem(1);
			OVT_DifficultySettings preset = config.m_aDifficultyPresets.Get(1);
			text.SetText(preset.description);
			config.m_Difficulty = preset;
		}
		
		
		
	}
	
	protected void OnSpinOccupyingFaction(SCR_SpinBoxComponent spinner, int index)
	{
		Faction data = Faction.Cast(spinner.GetItemData(index));
		OVT_Global.GetConfig().SetOccupyingFaction(data.GetFactionKey());	
	}
	
	protected void OnSpinSupportingFaction(SCR_SpinBoxComponent spinner, int index)
	{
		Faction data = Faction.Cast(spinner.GetItemData(index));
		OVT_Global.GetConfig().SetSupportingFaction(data.GetFactionKey());	
	}
	
	protected void OnSpinDifficulty(SCR_SpinBoxComponent spinner, int index)
	{
		OVT_DifficultySettings preset = OVT_DifficultySettings.Cast(spinner.GetItemData(index));
		OVT_Global.GetConfig().m_Difficulty = preset;
		
		Print(OVT_Global.GetConfig().m_Difficulty.name);
		
		Widget description = m_wRoot.FindAnyWidget("DifficultyDescription");
		TextWidget text = TextWidget.Cast(description);
		text.SetText(preset.description);
	}
	
	protected void StartGame()
	{
		CloseLayout();
		
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		
		mode.DoStartNewGame();
		mode.DoStartGame();
	}
}