//------------------------------------------------------------------------------------------------
//! Manages player skills, experience points (XP), and skill progression within the game mode.
//! Handles awarding XP for various actions, applying skill effects, and synchronizing skill data across clients.
class OVT_SkillManagerComponentClass: OVT_ComponentClass
{
};

class OVT_SkillManagerComponent: OVT_Component
{	
	[Attribute()]
	ref OVT_SkillsConfig m_Skills;
	
	static OVT_SkillManagerComponent s_Instance;
	
	//------------------------------------------------------------------------------------------------
	//! Gets the singleton instance of the OVT_SkillManagerComponent.
	//! Creates the instance if it doesn't exist by finding it on the GameMode entity.
	//! \return The singleton instance of OVT_SkillManagerComponent.
	static OVT_SkillManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_SkillManagerComponent.Cast(pGameMode.FindComponent(OVT_SkillManagerComponent));
		}

		return s_Instance;
	}
	
	ref ScriptInvoker m_OnPlayerSkill = new ScriptInvoker();
	
	//------------------------------------------------------------------------------------------------
	//! Initializes the component on the server.
	//! Registers a callback for when player data is loaded.
	//! \param owner The entity owning this component.
	void Init(IEntity owner)
	{
		if(!Replication.IsServer()) return;
		OVT_Global.GetPlayers().m_OnPlayerDataLoaded.Insert(OnPlayerDataLoaded);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Performs post-initialization tasks after the game has started, only on the server.
	//! Registers callbacks for various game events like player kills, buying/selling, AI kills, building, and placing.
	void PostGameStart()
	{	
		if(!Replication.IsServer()) return;	
		OVT_OverthrowGameMode game = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());		
		game.GetOnPlayerKilled().Insert(OnPlayerKilled);
		
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		economy.m_OnPlayerBuy.Insert(OnPlayerBuy);
		economy.m_OnPlayerSell.Insert(OnPlayerSell);
		
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		of.m_OnAIKilled.Insert(OnAIKilled);
		
		OVT_ResistanceFactionManager rf = OVT_Global.GetResistanceFaction();
		rf.m_OnBuild.Insert(OnBuild);
		rf.m_OnPlace.Insert(OnPlace);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Callback executed when player data is loaded. Applies existing skill effects based on the loaded skill levels.
	//! \param player The OVT_PlayerData object for the loaded player.
	//! \param persId The persistent ID of the player.
	void OnPlayerDataLoaded(OVT_PlayerData player, string persId)
	{
		for(int t=0; t<player.skills.Count(); t++)
		{
			string key = player.skills.GetKey(t);
			if(key == "") continue;
			OVT_SkillConfig skill = GetSkill(key);
			int level = player.skills.GetElement(t);
			for(int i = 0; i < level && i < skill.m_aLevels.Count(); i++)
			{
				OVT_SkillLevelConfig levelCfg = skill.m_aLevels[i];
				foreach(OVT_SkillEffect effect : levelCfg.m_aEffects)
				{
					effect.OnPlayerData(player);
				}
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Adds a level to a specific skill for a player.
	//! Updates the player's data, invokes skill effects, and replicates the change to clients.
	//! \param playerId The ID of the player gaining the skill level.
	//! \param key The string identifier of the skill.
	void AddSkillLevel(int playerId, string key)
	{
		OVT_PlayerData player = OVT_PlayerData.Get(playerId);
		if(!player.skills.Contains(key)) player.skills.Set(key, 0);
		int newlevel = player.skills[key]+1;
		player.skills.Set(key, newlevel);
		m_OnPlayerSkill.Invoke();
		
		DoInvokeSkillData(playerId,key,newlevel);
		
		if(SCR_PlayerController.GetLocalPlayerId() == playerId)
		{
			DoInvokeSkillSpawn(playerId,key,newlevel);
		}else{				
			Rpc(RpcDo_SetPlayerSkill, playerId, key, newlevel);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Invokes the data-related effects of a skill level for a specific player.
	//! \param playerId The ID of the player.
	//! \param key The string identifier of the skill.
	//! \param newlevel The new level of the skill.
	protected void DoInvokeSkillData(int playerId, string key, int newlevel)
	{
		OVT_PlayerData player = OVT_PlayerData.Get(playerId);
		OVT_SkillConfig skill = GetSkill(key);		
		OVT_SkillLevelConfig levelCfg = skill.m_aLevels[newlevel-1];
		if(!levelCfg) return;
		foreach(OVT_SkillEffect effect : levelCfg.m_aEffects)
		{
			effect.OnPlayerData(player);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Invokes the spawn-related effects of a skill level for a specific player's character.
	//! \param playerId The ID of the player.
	//! \param key The string identifier of the skill.
	//! \param newlevel The new level of the skill.
	protected void DoInvokeSkillSpawn(int playerId, string key, int newlevel)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
		if(!character) return;
		OVT_SkillConfig skill = GetSkill(key);		
		OVT_SkillLevelConfig levelCfg = skill.m_aLevels[newlevel-1];
		if(!levelCfg) return;
		foreach(OVT_SkillEffect effect : levelCfg.m_aEffects)
		{
			effect.OnPlayerSpawn(character);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Callback executed when a player places a placeable entity. Awards XP to the player.
	//! \param entity The placed entity.
	//! \param placeable The OVT_Placeable component of the entity.
	//! \param playerId The ID of the player who placed the entity.
	void OnPlace(IEntity entity, OVT_Placeable placeable, int playerId)
	{
		GiveXP(playerId, placeable.m_iRewardXP);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Callback executed when a player builds a buildable entity. Awards XP to the player.
	//! \param entity The built entity.
	//! \param buildable The OVT_Buildable component of the entity.
	//! \param playerId The ID of the player who built the entity.
	void OnBuild(IEntity entity, OVT_Buildable buildable, int playerId)
	{
		GiveXP(playerId, buildable.m_iRewardXP);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Callback executed when a player is killed. Takes away XP from the victim.
	//! \param instigatorContextData Context data about the kill event.
	void OnPlayerKilled(notnull SCR_InstigatorContextData instigatorContextData)
	{
		TakeXP(instigatorContextData.GetVictimPlayerID(), 1);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Callback executed when a player buys items. Awards a small amount of XP.
	//! \param playerId The ID of the player who bought items.
	//! \param amount The total amount spent.
	void OnPlayerBuy(int playerId, int amount)
	{
		GiveXP(playerId, 1);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Callback executed when a player sells items. Awards XP based on the sell amount.
	//! \param playerId The ID of the player who sold items.
	//! \param amount The total amount received.
	void OnPlayerSell(int playerId, int amount)
	{
		int xp = 1 + Math.Floor(amount * 0.01);
		GiveXP(playerId, xp);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Callback executed when an AI unit is killed. Awards XP and increments kill count if killed by a player and belongs to the occupying faction.
	//! \param killed The AI entity that was killed.
	//! \param instigator The entity that killed the AI.
	void OnAIKilled(IEntity killed, IEntity instigator)
	{
		if(!instigator) return;
		FactionAffiliationComponent fac = EPF_Component<FactionAffiliationComponent>.Find(killed);
		if(!fac) return;
		
		if(fac.GetAffiliatedFaction().GetFactionKey() == OVT_Global.GetConfig().m_sOccupyingFaction)
		{
			int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(instigator);
			if(playerId > -1)
			{
				string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
				OVT_PlayerData player = OVT_PlayerData.Get(persId);
				player.kills = player.kills + 1;
				StreamPlayerKills(playerId, player.kills);
				GiveXP(playerId, 5);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Reduces the XP of a player. Ensures XP does not go below zero.
	//! \param playerId The ID of the player losing XP.
	//! \param num The amount of XP to take away.
	void TakeXP(int playerId, int num)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		OVT_PlayerData player = OVT_PlayerData.Get(persId);
		player.xp = player.xp - num;
		if(player.xp < 0) player.xp = 0;
		StreamPlayerXP(playerId, player.xp);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Awards XP to a player and handles level-up notifications.
	//! \param playerId The ID of the player gaining XP.
	//! \param num The amount of XP to give.
	void GiveXP(int playerId, int num)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		OVT_PlayerData player = OVT_PlayerData.Get(persId);
		player.xp = player.xp + num;
		StreamPlayerXP(playerId, player.xp);
		int level = player.GetLevel();
		if(player.levelNotified < level)
		{
			player.levelNotified = level;
			OVT_Global.GetNotify().SendTextNotification("LevelUp",playerId,level.ToString());
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Retrieves the skill configuration for a given skill key.
	//! \param key The string identifier of the skill.
	//! \return The OVT_SkillConfig object, or null if not found.
	OVT_SkillConfig GetSkill(string key)
	{
		foreach(OVT_SkillConfig skill : m_Skills.m_aSkills)
		{
			if(skill.m_sKey == key) return skill;
		}
		return null;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Sends an RPC to update the kill count for a player on all clients.
	//! \param playerId The ID of the player.
	//! \param kills The new kill count.
	void StreamPlayerKills(int playerId, int kills)
	{
		Rpc(RpcDo_SetPlayerKills, playerId, kills);
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC method to set the player's kill count on clients.
	//! \param playerId The ID of the player.
	//! \param kills The kill count to set.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetPlayerKills(int playerId, int kills)
	{
		OVT_PlayerData player = OVT_PlayerData.Get(playerId);
		player.kills = kills;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Sends an RPC to update the XP for a player on all clients.
	//! \param playerId The ID of the player.
	//! \param xp The new XP value.
	void StreamPlayerXP(int playerId, int xp)
	{
		Rpc(RpcDo_SetPlayerXP, playerId, xp);
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC method to set the player's XP on clients.
	//! \param playerId The ID of the player.
	//! \param xp The XP value to set.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetPlayerXP(int playerId, int xp)
	{
		OVT_PlayerData player = OVT_PlayerData.Get(playerId);
		player.xp = xp;
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC method to set a player's skill level on clients.
	//! Invokes local skill effects if the RPC is received by the owning client.
	//! \param playerId The ID of the player.
	//! \param key The string identifier of the skill.
	//! \param level The new level of the skill.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetPlayerSkill(int playerId, string key, int level)
	{
		OVT_PlayerData player = OVT_PlayerData.Get(playerId);
		player.skills[key] = level;
		if(SCR_PlayerController.GetLocalPlayerId() == playerId)
		{
			m_OnPlayerSkill.Invoke();
			DoInvokeSkillData(playerId, key, level);
			DoInvokeSkillSpawn(playerId, key, level);
		}
	}
	
	void ~OVT_SkillManagerComponent()
	{
		
	}
}