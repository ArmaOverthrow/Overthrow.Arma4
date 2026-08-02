class OVT_StartGameContext : OVT_UIContext
{
	[Attribute(defvalue: "{6B0F11B0AA01C001}UI/Layouts/Menu/ContinueGameMenu.layout", uiwidget: UIWidgets.ResourceNamePicker, desc: "Continue/new-game chooser, shown instead of the start menu when a save exists", params: "layout")]
	ResourceName m_ChooserLayout;

	//! The start-menu layout as configured in the prefab, captured on first ShowLayout() so the two
	//! screens can swap underneath the base class (which always creates m_Layout).
	protected ResourceName m_StartMenuLayout;

	//! True while the widgets on screen are the chooser's, not the start menu's.
	protected bool m_bChooserMode;

	//! Set when the player picked "Start Game" on the chooser - the chooser must not come back for
	//! the rest of this session, or it would reappear over the campaign setup it just handed over to.
	protected bool m_bNewGameChosen;

	//------------------------------------------------------------------------------------------------
	//! TWO SCREENS, ONE CONTEXT. When a save exists for this mission the player first gets a small
	//! chooser - Continue Save / Start Game - on its own screen; the full campaign-setup menu only
	//! shows for a genuinely new campaign (no save, or the player explicitly chose to start over).
	//! Both screens share this context's registration, its input context (both actions live in
	//! OverthrowStartContext) and the handler that drives EOnFrame.
	override void ShowLayout()
	{
		if(m_StartMenuLayout == "")
			m_StartMenuLayout = m_Layout;

		m_bChooserMode = !m_bNewGameChosen && m_ChooserLayout != "" && HasCampaignSave();

		if(m_bChooserMode)
			m_Layout = m_ChooserLayout;
		else
			m_Layout = m_StartMenuLayout;

		super.ShowLayout();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when a campaign save exists for this mission (the server-side cached answer).
	protected bool HasCampaignSave()
	{
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(!mode)
			return false;

		OVT_PersistenceManagerComponent persistence = mode.GetPersistence();
		if(!persistence)
			return false;

		return persistence.HasSaveGame();
	}

	protected FactionManager m_Factions;
	
	protected ref array<FactionKey> m_FactionKeys;
	
	override void OnShow()
	{		
		// The chooser is its own screen with its own widgets - none of the start menu's exist here.
		if(m_bChooserMode)
		{
			ShowChooser();
			return;
		}

#ifdef PLATFORM_CONSOLE	
		Widget xbox = m_wRoot.FindAnyWidget("XBOXWarning");
		xbox.SetVisible(true);
#endif
				
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
		
		config.SetOccupyingFaction(config.m_sDefaultOccupyingFaction);
		
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
		
		// Check if this conflicts with supporting faction
		string currentSupporting = OVT_Global.GetConfig().m_sSupportingFaction;
		if(data.GetFactionKey() == currentSupporting)
		{
			// Find supporting faction spinner and change it to a different faction
			Widget sf = m_wRoot.FindAnyWidget("SupportingFactionSpinner");
			SCR_SpinBoxComponent sfSpin = SCR_SpinBoxComponent.Cast(sf.FindHandler(SCR_SpinBoxComponent));
			
			// Find a different faction (not the one we just selected)
			for(int i = 0; i < sfSpin.GetNumItems(); i++)
			{
				Faction altFaction = Faction.Cast(sfSpin.GetItemData(i));
				if(altFaction.GetFactionKey() != data.GetFactionKey())
				{
					sfSpin.SetCurrentItem(i);
					OVT_Global.GetConfig().SetSupportingFaction(altFaction.GetFactionKey());
					break;
				}
			}
		}
		
		OVT_Global.GetConfig().SetOccupyingFaction(data.GetFactionKey());
	}
	
	protected void OnSpinSupportingFaction(SCR_SpinBoxComponent spinner, int index)
	{
		Faction data = Faction.Cast(spinner.GetItemData(index));
		
		// Check if this conflicts with occupying faction
		string currentOccupying = OVT_Global.GetConfig().m_sOccupyingFaction;
		if(data.GetFactionKey() == currentOccupying)
		{
			// Find occupying faction spinner and change it to a different faction
			Widget of = m_wRoot.FindAnyWidget("OccupyingFactionSpinner");
			SCR_SpinBoxComponent ofSpin = SCR_SpinBoxComponent.Cast(of.FindHandler(SCR_SpinBoxComponent));
			
			// Find a different faction (not the one we just selected)
			for(int i = 0; i < ofSpin.GetNumItems(); i++)
			{
				Faction altFaction = Faction.Cast(ofSpin.GetItemData(i));
				if(altFaction.GetFactionKey() != data.GetFactionKey())
				{
					ofSpin.SetCurrentItem(i);
					OVT_Global.GetConfig().SetOccupyingFaction(altFaction.GetFactionKey());
					break;
				}
			}
		}
		
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
	
	//------------------------------------------------------------------------------------------------
	//! Wires the chooser screen: Continue Save resumes the campaign, Start New Game moves on to the
	//! campaign-setup menu. The buttons are PauseMenuButton-style (the same style as the in-game
	//! main menu), wired through SCR_ButtonTextComponent like every other Overthrow menu.
	protected void ShowChooser()
	{
		SCR_ButtonTextComponent continueButton = SCR_ButtonTextComponent.GetButtonText("ContinueButton", m_wRoot);
		if(continueButton)
			continueButton.m_OnClicked.Insert(ContinueGame);

		SCR_ButtonTextComponent newGameButton = SCR_ButtonTextComponent.GetButtonText("NewGameButton", m_wRoot);
		if(newGameButton)
			newGameButton.m_OnClicked.Insert(NewGame);
	}

	//------------------------------------------------------------------------------------------------
	//! Swaps the chooser for the campaign-setup menu, within the same frame - the handler polling
	//! IsActive() never sees the context inactive.
	protected void NewGame()
	{
		Print("[Overthrow] Start Game chosen on the continue screen - showing campaign setup");
		m_bNewGameChosen = true;
		CloseLayout();
		ShowLayout();
	}

	protected void ContinueGame()
	{
		Print("[Overthrow] Continue Save clicked - loading the latest save point");

		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(!mode)
			return;

		OVT_PersistenceManagerComponent persistence = mode.GetPersistence();
		if(!persistence)
			return;

		// Close immediately - the load is asynchronous and the chooser must not sit over the
		// screen while the engine works. A player is still never left stranded: the watch below
		// brings the chooser back if the load fails.
		CloseLayout();

		persistence.LoadLatestSave();
		GetGame().GetCallqueue().CallLater(CheckContinueOutcome, 1000, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Watches a continue that was handed to the engine. A successful load replaces the world (and
	//! this context with it), and IsLoadInProgress() stays true right up to that transition - so
	//! still being here with the load no longer in flight means it failed, and the chooser comes
	//! back instead of leaving a bare start camera. Mouse-driven buttons stay clickable on the
	//! re-shown chooser even though the handler has stopped driving frame updates by now.
	protected void CheckContinueOutcome()
	{
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(!mode)
			return;

		OVT_PersistenceManagerComponent persistence = mode.GetPersistence();
		if(!persistence)
			return;

		if(persistence.IsLoadInProgress())
		{
			GetGame().GetCallqueue().CallLater(CheckContinueOutcome, 1000, false);
			return;
		}

		Print("[Overthrow] Continue failed (" + persistence.GetLastLoadDiagnostic() + ") - showing the chooser again", LogLevel.WARNING);
		ShowLayout();
	}

	protected void StartGame()
	{
		Print("[Overthrow] StartGame button clicked - closing menu and starting game");
		CloseLayout();

		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());

		Print("[Overthrow] Calling DoStartNewGame()");
		mode.DoStartNewGame();

		Print("[Overthrow] Calling DoStartGame()");
		mode.DoStartGame();

		Print("[Overthrow] Game start complete");
	}
}