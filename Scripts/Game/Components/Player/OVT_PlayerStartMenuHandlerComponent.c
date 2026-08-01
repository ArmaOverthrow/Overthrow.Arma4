class OVT_PlayerStartMenuHandlerComponentClass : ScriptComponentClass {}

//! Component responsible for showing the start game menu to players before spawning
class OVT_PlayerStartMenuHandlerComponent : ScriptComponent
{
	protected OVT_StartGameContext m_StartGameContext;
	protected bool m_bMenuShown = false;
	protected bool m_bCheckedForMenu = false;

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

		Print("[Overthrow] Game mode and persistence ready, checking if we should show start menu");
		m_bCheckedForMenu = true;

		// Only show start menu if game hasn't started and no save exists
		bool hasSave = persistence.HasSaveGame();
		bool isDedicatedServer = (RplSession.Mode() == RplMode.Dedicated);
		bool isClientOnServer = (RplSession.Mode() == RplMode.Client);
		bool isSinglePlayer = (RplSession.Mode() == RplMode.None);
		bool isListenServerHost = (RplSession.Mode() == RplMode.Listen && Replication.IsServer());

		Print("[Overthrow] Game started: " + mode.HasGameStarted() + ", Has save: " + hasSave + ", Mode: " + RplSession.Mode() + ", IsServer: " + Replication.IsServer());

		// Show start menu for:
		// - Single player (RplMode.None) with no save
		// - Listen server HOST with no save (host needs to configure and start the game)
		// Never show for:
		// - Dedicated servers (handled by config file)
		// - Clients connecting to servers (wait for server to start game)
		// - Listen server clients (wait for host to start game)
		if (!mode.HasGameStarted() && !hasSave && (isSinglePlayer || isListenServerHost))
		{
			string menuType = "single player";
			if (isListenServerHost)
				menuType = "listen server host";
			Print("[Overthrow] Showing start menu for " + menuType);
			ShowStartMenu();
			// Keep frame updates running to activate input context
		}
		else
		{
			Print("[Overthrow] Not showing start menu (multiplayer client, dedicated server, or game already started/saved)");
			ClearEventMask(owner, EntityEvent.FRAME);
		}
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
