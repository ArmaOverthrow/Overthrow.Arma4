class OVT_RespawnHandlerComponentClass : SCR_RespawnHandlerComponentClass {}

class OVT_RespawnHandlerComponent : SCR_RespawnHandlerComponent
{
	protected OVT_RespawnSystemComponent m_pRespawnSystem;
	protected PlayerManager m_pPlayerManager;
	protected OVT_OverthrowGameMode m_Overthrow;
	protected ref array<ref Tuple2<int, string>> m_sEnqueuedCharacters = new array<ref Tuple2<int, string>>;

	//------------------------------------------------------------------------------------------------
	override void OnPlayerRegistered(int playerId)
	{
		// On dedicated servers we need to wait for OnPlayerAuditSuccess instead for the real UID to be available
		if (m_pGameMode.IsMaster() && (RplSession.Mode() != RplMode.Dedicated))
			OnUidAvailable(playerId);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerAuditSuccess(int playerId)
	{
		if (m_pGameMode.IsMaster())
			OnUidAvailable(playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnUidAvailable(int playerId)
	{
		string playerUid = EPF_Utils.GetPlayerUID(playerId);
		
#ifdef WORKBENCH
		//Force only two players in workbench to test reconnection
		if(playerId > 2)
		{
			playerUid = EPF_Utils.GetPlayerUID(2);
		}
#endif

		if (!playerUid)
			return;
		
		if(!m_Overthrow || !m_Overthrow.IsInitialized())
		{
			Tuple2<int, string> characterInfo(playerId, playerUid);
			m_sEnqueuedCharacters.Insert(characterInfo);
		}else{
			LoadPlayerData(playerId, playerUid);
		}
		GetGame().GetCallqueue().CallLater(OVT_Global.GetTowns().StreamTownModifiers, 5000, false, playerId);		
	}
	
	protected void LoadPlayerData(int playerId, string playerUid)
	{
#ifdef PLATFORM_XBOX
		m_pRespawnSystem.PrepareCharacter(playerId, playerUid, null);
		m_sEnqueuedPlayers.Insert(playerId);
		return;
#endif
		
		Tuple2<int, string> characterContext(playerId, playerUid);
		EDF_DbFindCallbackSingle<EPF_CharacterSaveData> characterDataCallback(this, "OnCharacterDataLoaded", characterContext);
		EPF_PersistenceEntityHelper<EPF_CharacterSaveData>.GetRepository().FindAsync(playerUid, characterDataCallback);
	}

	//------------------------------------------------------------------------------------------------
	//! Handles the character data found for the players account
	protected void OnCharacterDataLoaded(EDF_EDbOperationStatusCode statusCode, EPF_CharacterSaveData characterData, Managed context)
	{
		Tuple2<int, string> characterInfo = Tuple2<int, string>.Cast(context);

		if (characterData)
			PrintFormat("Loaded existing character '%1'.", characterInfo.param2);

		// Prepare spawn data buffer with last known player data (null for fresh accounts) and queue player for spawn
		m_pRespawnSystem.PrepareCharacter(characterInfo.param1, characterInfo.param2, characterData);
		m_sEnqueuedPlayers.Insert(characterInfo.param1);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
		m_sEnqueuedPlayers.RemoveItem(playerId);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerKilled(int playerId, IEntity player, IEntity killer)
	{
		if (!m_pGameMode.IsMaster())
			return;

		// Add the dead body root entity collection so it spawns back after restart for looting
		EPF_PersistenceComponent persistence = EPF_Component<EPF_PersistenceComponent>.Find(player);
		if (!persistence)
		{
			Print(string.Format("OnPlayerKilled(%1, %2, %3) -> Player killed that does not have persistence component?!? Something went terribly wrong!", playerId, player, killer), LogLevel.ERROR);
			return;
		}

		string newId = persistence.GetPersistentId();

		persistence.SetPersistentId(string.Empty); // Force generation of new id for dead body
		persistence.ForceSelfSpawn();

		// Prepare and execute fresh character spawn
		m_pRespawnSystem.PrepareCharacter(playerId, newId, null);
		m_sEnqueuedPlayers.Insert(playerId);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		if (!m_pGameMode.IsMaster())
			return;

		m_sEnqueuedPlayers.RemoveItem(playerId);

		IEntity player = m_pPlayerManager.GetPlayerController(playerId).GetControlledEntity();
		if (player)
		{
			EPF_PersistenceComponent persistence = EPF_Component<EPF_PersistenceComponent>.Find(player);
			persistence.PauseTracking();
			persistence.Save();
		}
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if(!m_Overthrow || !m_Overthrow.IsInitialized())
			return;
		
		if (!m_sEnqueuedCharacters.IsEmpty())
		{
			foreach (Tuple2<int, string> characterData : m_sEnqueuedCharacters)
			{
				LoadPlayerData(characterData.param1, characterData.param2);
			}
			m_sEnqueuedCharacters.Clear();
		}
		
		if (m_sEnqueuedPlayers.IsEmpty())
			return;

		set<int> iterCopy();
		iterCopy.Copy(m_sEnqueuedPlayers);
		foreach (int playerId : iterCopy)
		{
			if (m_pRespawnSystem.IsReadyForSpawn(playerId)){
				PlayerController controller = m_pPlayerManager.GetPlayerController(playerId);
				if(controller)
					controller.RequestRespawn();
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_pRespawnSystem = EPF_Component<OVT_RespawnSystemComponent>.Find(owner);
		m_pPlayerManager = GetGame().GetPlayerManager();
		m_Overthrow = OVT_OverthrowGameMode.Cast(owner);
	}
}