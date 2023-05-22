modded class SCR_SaveLoadComponent
{		
	void LoadGame()
	{
		Load();
	}
	
	override static bool IsLoadOnStart(SCR_MissionHeader missionHeader)
	{
		string saveFileName = GetSaveFileName(missionHeader);
		if (!saveFileName)
			return false;
		
		if(RplSession.Mode() == RplMode.Dedicated && HasSaveFile(missionHeader)) return true;		
		
		//--- Load from CLI param
		string fileNameCLI;
		if (System.GetCLIParam("loadSaveFile", fileNameCLI))
		{
			array<string> fileNamesCLI = {};
			fileNameCLI.Split(",", fileNamesCLI, false);
			if (fileNamesCLI.Contains(saveFileName))
				return true;
		}
		
		string fileNameToLoad;
		return GameSessionStorage.s_Data.Find(GAME_SESSION_STORAGE_NAME, fileNameToLoad) && fileNameToLoad == saveFileName;
	}
}