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
		SaveRecruits();
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(mode)
			mode.PreShutdownPersist();
		
		super.OnGameEnd();
	}
	
	void SaveGame()
	{
		SaveRecruits();
		if (m_pPersistenceManager){
			m_pPersistenceManager.AutoSave();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Save all currently spawned recruits manually via EPF
	void SaveRecruits()
	{
#ifdef PLATFORM_CONSOLE
		return;
#endif
		OVT_RecruitManagerComponent recruitManager = OVT_Global.GetRecruits();
		if (!recruitManager)
			return;
			
		// Iterate through all currently spawned recruit entities and save them
		foreach (EntityID entityId, string recruitId : recruitManager.m_mEntityToRecruit)
		{
			IEntity recruitEntity = GetGame().GetWorld().FindEntityByID(entityId);
			if (!recruitEntity)
				continue;
				
			EPF_PersistenceComponent persistence = EPF_PersistenceComponent.Cast(
				recruitEntity.FindComponent(EPF_PersistenceComponent)
			);
			
			if (persistence)
			{
				EPF_EReadResult readResult;
				persistence.Save(readResult);
				Print("[Overthrow] Manually saved recruit: " + recruitId);
			}
		}
	}
	
	bool HasSaveGame()
	{
#ifdef PLATFORM_CONSOLE
		return false;
#endif
		return FileIO.FileExists(DB_BASE_DIR + "/RootEntityCollections");
	}
	
	void WipeSave()
	{
#ifdef PLATFORM_CONSOLE
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