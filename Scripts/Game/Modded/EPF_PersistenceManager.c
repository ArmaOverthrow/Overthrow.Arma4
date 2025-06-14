modded class EPF_PersistenceManager
{
	override static bool IsPersistenceMaster()
	{
#ifdef PLATFORM_CONSOLE
		return false;
#endif
		
		if (!Replication.IsServer())
			return false;

		ArmaReforgerScripted game = GetGame();
		return game && game.InPlayMode();
	}
	
	protected override bool CheckLoaded()
	{
#ifdef PLATFORM_CONSOLE
		return false;
#endif
		
		if (m_eState < EPF_EPersistenceManagerState.SETUP)
		{
			Debug.Error("Attempted to call persistence operation before setup phase. Await setup/completion using GetOnStateChangeEvent/GetOnActiveEvent.");
			return false;
		}

		return true;
	}
}