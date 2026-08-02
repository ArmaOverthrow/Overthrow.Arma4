class OVT_PlayerStartMenuHandlerComponentClass : ScriptComponentClass {}

//! Component responsible for showing the start game menu to players before spawning
class OVT_PlayerStartMenuHandlerComponent : ScriptComponent
{
	protected OVT_StartGameContext m_StartGameContext;
	protected bool m_bMenuShown = false;
	protected bool m_bCheckedForMenu = false;

	//! Set when this handler triggered a continue (LoadLatestSave) instead of showing the menu.
	//! While it holds, EOnFrame watches the load: success replaces the world (and this component);
	//! failure falls back to showing the start menu rather than leaving the start camera up forever.
	protected bool m_bContinueRequested = false;

	//! The game mode's persistence manager, kept for the continue watch above.
	protected OVT_PersistenceManagerComponent m_Persistence;

	//------------------------------------------------------------------------------------------------
	protected override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		Print("[Overthrow] OVT_PlayerStartMenuHandlerComponent initialized for player controller");

		// Enable frame updates to check when to show menu
		SetEventMask(owner, EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	protected override void EOnFrame(IEntity owner, float timeSlice)
	{
		// If menu is active, activate its input context
		if (m_bMenuShown && m_StartGameContext)
		{
			m_StartGameContext.EOnFrame(owner, timeSlice);

			// Check if user closed the menu (clicked "Start Game")
			if (!m_StartGameContext.IsActive())
			{
				m_bMenuShown = false;
				ClearEventMask(owner, EntityEvent.FRAME);
				return;
			}
		}

		// A continue was requested instead of the menu. A SUCCESSFUL load replaces the world (and
		// this component with it), and IsLoadInProgress() stays true right up to that transition -
		// so still being here with the load no longer in flight means it failed, and the player
		// must get the start menu rather than a start camera that never ends.
		if (m_bContinueRequested)
		{
			if (m_Persistence && m_Persistence.IsLoadInProgress())
				return;

			string diagnostic = "";
			if (m_Persistence)
				diagnostic = m_Persistence.GetLastLoadDiagnostic();

			Print("[Overthrow] Continue failed (" + diagnostic + ") - showing the start menu instead", LogLevel.WARNING);
			m_bContinueRequested = false;
			ShowStartMenu();
			return;
		}

		// Only check once for showing menu
		if (m_bCheckedForMenu)
			return;

		// Wait for game mode to exist
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if (!mode)
			return;

		// Wait for persistence manager to be initialized so we can check for saves
		OVT_PersistenceManagerComponent persistence = OVT_PersistenceManagerComponent.Cast(mode.FindComponent(OVT_PersistenceManagerComponent));
		if (!persistence)
			return;

		// Get the start game context from the game mode (it's configured there in the prefab)
		m_StartGameContext = mode.GetStartGameContext();
		if (!m_StartGameContext)
		{
			Print("[Overthrow] ERROR: Game mode has no start game context configured!", LogLevel.ERROR);
			ClearEventMask(owner, EntityEvent.FRAME);
			return;
		}

		// Initialize the context with the player controller as owner (so it has a viewport)
		m_StartGameContext.Init(owner, null);

		bool isDedicatedServer = (RplSession.Mode() == RplMode.Dedicated);
		bool isClientOnServer = (RplSession.Mode() == RplMode.Client);
		bool isSinglePlayer = (RplSession.Mode() == RplMode.None);
		bool isListenServerHost = (RplSession.Mode() == RplMode.Listen && Replication.IsServer());

		// The menu-or-continue decision below branches on HasSaveGame(), which is served from an
		// ASYNC cache (GetSaves has no synchronous form). Deciding on a stale false would offer a
		// new campaign over an existing one, so the authority waits for the scan to answer first.
		// Only SP and the listen host branch on it - clients and dedicated decide immediately (and
		// a client's persistence manager never seeds, so waiting there would hang forever).
		if ((isSinglePlayer || isListenServerHost) && !mode.HasGameStarted()
			&& !persistence.IsPlayingLoadedSave() && !persistence.IsSaveCacheSeeded())
			return;

		Print("[Overthrow] Game mode and persistence ready, checking if we should show start menu");
		m_bCheckedForMenu = true;
		m_Persistence = persistence;

		bool hasSave = persistence.HasSaveGame();

		Print("[Overthrow] Game started: " + mode.HasGameStarted() + ", Has save: " + hasSave + ", Mode: " + RplSession.Mode() + ", IsServer: " + Replication.IsServer());

		// The authority's decision when the campaign has not started:
		// - no save -> show the start menu (new campaign)
		// - a save exists -> CONTINUE it. This session was not launched from the save point
		//   (IsPlayingLoadedSave() is false - Workbench play, or "Play" instead of "Continue"),
		//   and nothing else in the boot flow loads one: the game mode is waiting for a start
		//   menu that must not show, so without this call the start camera never ends.
		// Never decide here for:
		// - Dedicated servers (OVT_OverthrowGameMode.EOnInit owns that decision)
		// - Clients connecting to servers (wait for server to start game)
		// - Listen server clients (wait for host to start game)
		// - A session launched from a save point (RestoreStartedCampaign drives the start)
		if (!mode.HasGameStarted() && (isSinglePlayer || isListenServerHost) && !persistence.IsPlayingLoadedSave())
		{
			if (hasSave)
			{
				Print("[Overthrow] A save exists for this mission - continuing the campaign from its latest save point");
				m_bContinueRequested = true;
				persistence.LoadLatestSave();
				// Frame updates stay on: the watch at the top of EOnFrame falls back to the start
				// menu if the load fails.
			}
			else
			{
				string menuType = "single player";
				if (isListenServerHost)
					menuType = "listen server host";
				Print("[Overthrow] Showing start menu for " + menuType);
				ShowStartMenu();
				// Keep frame updates running to activate input context
			}
		}
		else
		{
			Print("[Overthrow] Not showing start menu (multiplayer client, dedicated server, or game already started/loaded)");
			ClearEventMask(owner, EntityEvent.FRAME);
		}

		// Overthrow's UX owns the screen from here (start menu, or a spawn handled by
		// OVT_SpawnLogic). Vanilla's SCR_RespawnSystemComponent parked a full-screen loading
		// placeholder ("LoadingScreen" widget, z 100000, spinner + black background, SFX muted)
		// over the workspace at join, and only ITS OWN spawn/deploy flows ever destroy it -
		// Overthrow's custom spawn flow never runs those, so without this call the placeholder
		// covers the game forever (the 2026-08-02 Workbench "endless spinner"; also the likely
		// mechanism behind GitHub #143's "controls stuck, screen black, can hear the world").
		SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.GetInstance();
		if (respawnSystem)
			respawnSystem.DestroyLoadingPlaceholder();
	}

	//------------------------------------------------------------------------------------------------
	void ShowStartMenu()
	{
		if (m_bMenuShown)
			return;

		if (!m_StartGameContext)
		{
			Print("[Overthrow] ERROR: Start game context is null", LogLevel.ERROR);
			return;
		}

		Print("[Overthrow] Calling ShowLayout on start game context");
		m_StartGameContext.ShowLayout();
		m_bMenuShown = true;
	}

	//------------------------------------------------------------------------------------------------
	void CloseStartMenu()
	{
		if (!m_StartGameContext)
			return;

		m_StartGameContext.CloseLayout();
		m_bMenuShown = false;

		// Stop frame updates when menu is closed
		IEntity owner = GetOwner();
		if (owner)
			ClearEventMask(owner, EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	bool IsMenuShown()
	{
		return m_bMenuShown;
	}

	//------------------------------------------------------------------------------------------------
	OVT_StartGameContext GetStartGameContext()
	{
		return m_StartGameContext;
	}
}
