modded class ChimeraMenuBase
{
	override static void ReloadCurrentWorld()
	{		
		//Wipe the save, but only if we are playing overthrow, and not on a dedicated server
		
		OVT_OverthrowGameMode gameMode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(gameMode && RplSession.Mode() != RplMode.Dedicated)		
		{
			OVT_PersistenceManagerComponent persistence = OVT_PersistenceManagerComponent.Cast(gameMode.FindComponent(OVT_PersistenceManagerComponent));
			if(persistence)
			{
				persistence.WipeSave();
			}
		}
		
		MissionHeader header = GetGame().GetMissionHeader();
		if (header)
		{
			GameStateTransitions.RequestMissionChangeTransition(header);
		}
		else
		{
			// In case of running the world from WB and entering the game menu (F10 ATM) there is no mission header to reload
			GameStateTransitions.RequestWorldChangeTransition(GetGame().GetWorldFile());
		}
	}
}