modded class SCR_SaveLoadComponent
{
	[Attribute()]
	protected bool m_bLoadSaveInWorkbench;
	
	override void OnWorldPostProcess(World world)
	{
		if (Replication.IsServer())
		{
			bool load = false;
			SCR_MissionHeader missionHeader = SCR_MissionHeader.Cast(GetGame().GetMissionHeader());		
			if (missionHeader)
			{
				m_sFileName = missionHeader.GetSaveFileName();
			
				//--- Saving is disabled, terminate
				if (!missionHeader.IsSavingEnabled())
					return;
				
				load = IsLoadOnStart(missionHeader);
			}
#ifdef WORKBENCH
			else
			{
				m_sFileName = "WB_" + FilePath.StripPath(FilePath.StripExtension(GetGame().GetWorldFile()));
			}
			load = m_bLoadSaveInWorkbench;
			if(world.IsEditMode()) load = false;
#endif
			
			if (m_sFileName)
			{			
				m_Callback = new SCR_DSSessionCallback(m_Struct);
				
				if (load)
					m_Callback.LoadSession(m_sFileName);
			}
		}
	}
	
	void LoadGame()
	{
		if (m_sFileName && Replication.IsServer())
		{			
			m_Callback = new SCR_DSSessionCallback(m_Struct);			
			m_Callback.LoadSession(m_sFileName);
		}
	}
}