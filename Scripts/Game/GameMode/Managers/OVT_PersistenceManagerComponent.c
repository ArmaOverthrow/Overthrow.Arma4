[ComponentEditorProps(category: "Persistence", description: "Must be attached to the gamemode entity to setup the persistence system.")]
class OVT_PersistenceManagerComponentClass : EPF_PersistenceManagerComponentClass
{
}

class OVT_PersistenceManagerComponent : EPF_PersistenceManagerComponent
{
	const string DB_BASE_DIR = "$profile:/.db/Overthrow";
	
	protected World m_World;
	
	override event void OnGameEnd()
	{
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(mode)
			mode.PreShutdownPersist();
		
		super.OnGameEnd();
	}
	
	void SaveGame()
	{
		if (m_pPersistenceManager){
			m_pPersistenceManager.AutoSave();
		}
	}
	
	bool HasSaveGame()
	{
#ifdef PLATFORM_XBOX
		return false;
#endif
		return FileIO.FileExists(DB_BASE_DIR + "/RootEntityCollections");
	}
	
	void WipeSave()
	{
#ifdef PLATFORM_XBOX
		return;
#endif
		FileIO.FindFiles(DeleteFileCallback, DB_BASE_DIR, ".json");
		FileIO.FindFiles(DeleteFileCallback, DB_BASE_DIR, ".bin");
		FileIO.FindFiles(DeleteFileCallback, DB_BASE_DIR, string.Empty);
		FileIO.DeleteFile(DB_BASE_DIR);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void DeleteFileCallback(string path, FileAttribute attributes)
	{
		FileIO.DeleteFile(path);
	}
	
	override event void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (m_pPersistenceManager){
			m_pPersistenceManager.OnAutoSaveCompleteEvent().Insert(OnAutoSaveComplete);
		}		
	}
	
	protected void OnAutoSaveComplete()
	{
		Print("[Overthrow] autosave completed");
	}
}