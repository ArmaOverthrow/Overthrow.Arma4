[ComponentEditorProps(category: "Persistence", description: "Must be attached to the gamemode entity to setup the persistence system.")]
class OVT_PersistenceManagerComponentClass : EPF_PersistenceManagerComponentClass
{
}

class OVT_PersistenceManagerComponent : EPF_PersistenceManagerComponent
{
	const string DB_BASE_DIR = "$profile:/.db/Overthrow";
	protected World m_World;
	
	override event void OnWorldPostProcess(World world)
	{
		//We want to control when the game loads from save, so we do nothing here
		m_World = world;
	}
	
	override event void OnGameEnd()
	{
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(mode)
			mode.PreShutdownPersist();
		
		super.OnGameEnd();
		if (m_pPersistenceManager)
			m_pPersistenceManager.OnGameEnd();
	}
	
	void LoadGame()
	{
		if (m_pPersistenceManager)
			m_pPersistenceManager.OnWorldPostProcess(m_World);
	}
	
	void StartNewGame()
	{
		//To-Do: wipe database to start a new game?
		if (m_pPersistenceManager)
			m_pPersistenceManager.OnWorldPostProcess(m_World);
	}
	
	bool HasSaveGame()
	{
		return FileIO.FileExist(DB_BASE_DIR + "/RootEntityCollections");
	}
	
	void WipeSave()
	{
		System.FindFiles(DeleteFileCallback, DB_BASE_DIR, ".json");
		System.FindFiles(DeleteFileCallback, DB_BASE_DIR, ".bin");
		System.FindFiles(DeleteFileCallback, DB_BASE_DIR, string.Empty);
		FileIO.DeleteFile(DB_BASE_DIR);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void DeleteFileCallback(string path, FileAttribute attributes)
	{
		FileIO.DeleteFile(path);
	}
}