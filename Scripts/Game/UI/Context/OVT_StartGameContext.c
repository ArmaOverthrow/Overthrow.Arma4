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
		
		if (RplSession.Mode() == RplMode.None)
		{
			//Default to "Easy" in single player
			spin.SetCurrentItem(0);
			OVT_DifficultySettings preset = config.m_aDifficultyPresets.Get(0);
			config.m_Difficulty = preset;
		}else{
			spin.SetCurrentItem(1);
			OVT_DifficultySettings preset = config.m_aDifficultyPresets.Get(1);
			config.m_Difficulty = preset;
		}

		// One redraw for all four lines, at the end of both branches. The two branches above used to
		// carry a SetText each, which is exactly the shape that drifts when only one is edited.
		RefreshDescriptions();
	}

	//------------------------------------------------------------------------------------------------
	//! Redraws all four description lines from the CURRENT config selections. Every hook that can
	//! change a selection ends here, and nothing else writes these widgets.
	//!
	//! The config, not the spinner, is the source of truth: m_sOccupyingFaction / m_sSupportingFaction
	//! are what the campaign actually starts with, and both spin handlers have finished writing them
	//! by the time this runs. SCR_SpinBoxComponent.SetCurrentItem invokes m_OnChanged synchronously
	//! (SCR_SpinBoxComponent.c:142-165), so the conflict swap resolves depth-first and the outermost
	//! call is always the last one to redraw.
	protected void RefreshDescriptions()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(!config)
			return;

		SetDescriptionText("OccupyingFactionDescription", ResolveOccupyingFactionDescription(config.m_sOccupyingFaction));
		SetDescriptionText("SupportingFactionDescription", ResolveSupportingFactionDescription(config.m_sSupportingFaction));

		OVT_DifficultySettings preset = config.m_Difficulty;
		if(!preset)
		{
			SetDescriptionText("DifficultyDescription", "");
			SetDescriptionText("DifficultyNumbers", "");
			return;
		}

		// The preset's 'description' field holds a #OVT- stringtable key rather than English:
		// TextWidget.SetText translates #-prefixed entries, so the five shipped difficulty configs
		// carry keys and this read site needs no change. Difficulty_TestWorld.conf has no
		// description at all and correctly renders as an empty line.
		SetDescriptionText("DifficultyDescription", preset.description);

		TextWidget numbers = FindDescriptionWidget("DifficultyNumbers");
		if(numbers)
		{
			// SetTextFormat, never concatenation - no English is ever built in script.
			// #OVT-Difficulty_Numbers takes exactly 2 substitutions, so this call takes 3 arguments.
			// %1 = startingCash (read at OVT_OverthrowGameMode.c:1080), %2 = fastTravelCost (read at
			// OVT_MapContext.c:384). Both are ints on the selected OVT_DifficultySettings.
			numbers.SetTextFormat("#OVT-Difficulty_Numbers", preset.startingCash, preset.fastTravelCost);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] factionKey The faction key currently in the OCCUPYING slot.
	//! \return Its description key, or "" when no description ships for that faction.
	//!
	//! Keyed by FACTION KEY, never by spinner index: the spinner is built by filtering the live
	//! faction list (see OnShow), so its contents and order are data a mod can change, and an
	//! index-keyed string would silently label one faction with another's description.
	//!
	//! The whitelist is written out rather than composed ("#OVT-Faction_" + factionKey + "_Occupying")
	//! on purpose. A composed key for a faction with no string item does not fall back to nothing -
	//! Enfusion draws an unresolved key as its own raw text, so a modded faction would put a literal
	//! "#OVT-Faction_XYZ_Occupying" on the first screen a player ever sees. An empty description is
	//! the correct degradation; a wrong or raw one is not.
	protected string ResolveOccupyingFactionDescription(string factionKey)
	{
		if(factionKey == "US")
			return "#OVT-Faction_US_Occupying";

		if(factionKey == "USSR")
			return "#OVT-Faction_USSR_Occupying";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] factionKey The faction key currently in the SUPPORTING slot.
	//! \return Its description key, or "" when no description ships for that faction.
	//!
	//! Separate from the occupying keys because the same faction means a different thing in the two
	//! slots: as occupier it is who holds the island and whose uniform is the disguise that works,
	//! as supporter it is a sympathetic foreign power whose uniform gets you shot at instead.
	protected string ResolveSupportingFactionDescription(string factionKey)
	{
		if(factionKey == "US")
			return "#OVT-Faction_US_Supporting";

		if(factionKey == "USSR")
			return "#OVT-Faction_USSR_Supporting";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] name The widget name in StartGameMenu.layout.
	//! \return The named TextWidget, or null when the layout on screen does not carry it - the
	//!         chooser screen carries none of them, and a stale layout must degrade to a missing
	//!         line rather than to a script error.
	protected TextWidget FindDescriptionWidget(string name)
	{
		if(!m_wRoot)
			return null;

		return TextWidget.Cast(m_wRoot.FindAnyWidget(name));
	}

	//------------------------------------------------------------------------------------------------
	//! Sets one description line. An empty string clears it, which is what an unknown faction key
	//! resolves to: an empty description rather than a wrong one.
	//! \param[in] name The widget name in StartGameMenu.layout.
	//! \param[in] text A #OVT- stringtable key, or "" to clear the line.
	protected void SetDescriptionText(string name, string text)
	{
		TextWidget widget = FindDescriptionWidget(name);
		if(!widget)
			return;

		widget.SetText(text);
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

		// BOTH faction lines are refreshed, not just the one whose spinner moved. The conflict swap
		// above walks the OTHER spinner and reassigns it, so refreshing only this one would leave a
		// stale sentence describing a faction that is no longer selected. (The swap's re-entrant
		// OnSpinSupportingFaction has already refreshed once by the time we get here; this second
		// pass is the one with both keys settled, and a redundant SetText costs nothing.)
		RefreshDescriptions();
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

		// Mirror image of OnSpinOccupyingFaction: both faction lines, for the same reason.
		RefreshDescriptions();
	}

	protected void OnSpinDifficulty(SCR_SpinBoxComponent spinner, int index)
	{
		OVT_DifficultySettings preset = OVT_DifficultySettings.Cast(spinner.GetItemData(index));
		OVT_Global.GetConfig().m_Difficulty = preset;

		// Description AND numbers. The bare Print that used to sit here fired on every spinner move
		// and dereferenced the preset without a guard; the preset's name is on the spinner anyway.
		RefreshDescriptions();
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