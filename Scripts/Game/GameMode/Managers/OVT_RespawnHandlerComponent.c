class OVT_RespawnHandlerComponentClass : SCR_RespawnHandlerComponentClass {}

class OVT_RespawnHandlerComponent : SCR_RespawnHandlerComponent
{
	protected OVT_RespawnSystemComponent m_pRespawnSystem;
	protected PlayerManager m_pPlayerManager;
	protected EPF_PersistenceManager m_pPersistenceManager;
	
	[Attribute("-1", uiwidget: UIWidgets.EditComboBox, category: "Respawn", desc: "Faction index to spawn player(s) with. Only applied when greater or equal to 0.")]
	protected int m_iForcedFaction;
	
	[Attribute("-1", uiwidget: UIWidgets.EditComboBox, category: "Respawn", desc: "Loadout index to spawn player(s) with. Only applied when greater or equal to 0.")]
	protected int m_iForcedLoadout;
	
	/*!
		Batch of players that are supposed to spawn.
		Used to prevent modifying collection we're iterating through.
	*/
	private ref array<int> m_aSpawningBatch = {};
	
	protected OVT_OverthrowGameMode m_Overthrow;
	
	void OVT_RespawnHandlerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_Overthrow = OVT_OverthrowGameMode.Cast(ent);
	}
	
	override protected void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_pRespawnSystem = OVT_RespawnSystemComponent.Cast(owner.FindComponent(OVT_RespawnSystemComponent));
		m_pPlayerManager = GetGame().GetPlayerManager();
		m_pPersistenceManager = EPF_PersistenceManager.GetInstance();
	}
	
	override bool CanPlayerSpawn(int playerId)
	{
		SCR_RespawnSystemComponent respawnSystem = m_pGameMode.GetRespawnSystemComponent();

		if (!respawnSystem.GetPlayerLoadout(playerId))
			return false;

		return true;
	}
	
	protected override bool RandomizePlayerSpawnPoint(int playerId)
	{
		if (!m_pGameMode.IsMaster())
			return false;
		
		return true;
	}

	/*!
		When player is enqueued, randomize their loadout.
	*/
	override void OnPlayerEnqueued(int playerId)
	{
		super.OnPlayerEnqueued(playerId);

		if (m_iForcedFaction >= 0)
		{
			if (OVT_RespawnSystemComponent.GetInstance().CanSetFaction(playerId, m_iForcedFaction))
				OVT_RespawnSystemComponent.GetInstance().DoSetPlayerFaction(playerId, m_iForcedFaction);
			else
				Print(string.Format("Cannot set faction %1 to player %2! Is faction index valid?", m_iForcedFaction, playerId), LogLevel.ERROR);
		}
		else 
			RandomizePlayerFaction(playerId);
		
		if (m_iForcedLoadout >= 0)
		{
			if (OVT_RespawnSystemComponent.GetInstance().CanSetLoadout(playerId, m_iForcedLoadout))
				OVT_RespawnSystemComponent.GetInstance().DoSetPlayerLoadout(playerId, m_iForcedLoadout);
			else
				Print(string.Format("Cannot set loadout %1 to player %2! Is loadout index valid?", m_iForcedLoadout, playerId), LogLevel.ERROR);
		}
		else
			RandomizePlayerLoadout(playerId);
		
		RandomizePlayerSpawnPoint(playerId);
	}

	/*!
		Ticks every frame. Handles automatic player respawn.
	*/
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		// Authority only
		if (!m_pGameMode.IsMaster())
			return;
		
		//Make sure game is started and initialized
		if(!m_Overthrow.IsInitialized())
			return;
		
		//Make sure persistence is active
		if(!m_pPersistenceManager.GetState() == EPF_EPersistenceManagerState.ACTIVE)
			return;
		
		// Clear batch
		m_aSpawningBatch.Clear();
		
		// Find players eligible for respawn
		foreach (int playerId : m_sEnqueuedPlayers)
		{			
			if (m_pGameMode.CanPlayerRespawn(playerId))
				m_aSpawningBatch.Insert(playerId);
		}

		// Respawn eligible players
		foreach (int playerId : m_aSpawningBatch)
		{
			Print("Respawning player " + playerId);
			PlayerController playerController = m_pPlayerManager.GetPlayerController(playerId);
			
			if(playerController)
				playerController.RequestRespawn();
		}

		super.EOnFrame(owner, timeSlice);
	}

	#ifdef WORKBENCH
	//! Possibility to get variable value choices dynamically
	override array<ref ParamEnum> _WB_GetUserEnums(string varName, IEntity owner, IEntityComponentSource src)
	{
		if (varName == "m_iForcedFaction")
		{
			FactionManager factionManager = GetGame().GetFactionManager();
			if (factionManager)
			{
				ref array<ref ParamEnum> factionEnums = new array<ref ParamEnum>();
				factionEnums.Insert(new ParamEnum("Disabled", "-1"));

				Faction faction;
				string name;
				int factionCount = factionManager.GetFactionsCount();
				for (int i = 0; i < factionCount; i++)
				{
					faction = factionManager.GetFactionByIndex(i);
					name = faction.GetFactionKey();
					factionEnums.Insert(new ParamEnum(name, i.ToString()));
				}

				return factionEnums;
			}
		}
		
		if (varName == "m_iForcedLoadout")
		{
			SCR_LoadoutManager loadoutManager = GetGame().GetLoadoutManager();
			if (loadoutManager)
			{
				ref array<ref ParamEnum> loadoutEnums = new array<ref ParamEnum>();
				loadoutEnums.Insert(new ParamEnum("Disabled", "-1"));
				
				array<SCR_BasePlayerLoadout> loadouts = {};
				for (int i = 0, count = loadoutManager.GetLoadoutCount(); i < count; i++)
					loadouts.Insert(loadoutManager.GetLoadoutByIndex(i));
				
				SCR_BasePlayerLoadout loadout;
				for (int i = 0, count = loadouts.Count(); i < count; i++)
				{
					loadout = loadouts[i];
					int loadoutIndex = loadoutManager.GetLoadoutIndex(loadout);
					loadoutEnums.Insert(new ParamEnum(loadout.GetLoadoutName(), loadoutIndex.ToString()));
				}

				return loadoutEnums;
			}
		}

		return super._WB_GetUserEnums(varName, owner, src);
	}
	#endif
}