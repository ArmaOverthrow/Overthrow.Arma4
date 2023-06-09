class OVT_SkillManagerComponentClass: OVT_ComponentClass
{
};

class OVT_SkillManagerComponent: OVT_Component
{	
	[Attribute()]
	ref OVT_SkillsConfig m_Skills;
	
	static OVT_SkillManagerComponent s_Instance;
	
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
	
	ref ScriptInvoker m_OnPlayerSkill = new ref ScriptInvoker();
	
	void Init(IEntity owner)
	{
		if(!Replication.IsServer()) return;
		OVT_Global.GetPlayers().m_OnPlayerDataLoaded.Insert(OnPlayerDataLoaded);
	}
	
	void PostGameStart()
	{	
		if(!Replication.IsServer()) return;	
		OVT_OverthrowGameMode game = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());		
		game.GetOnPlayerKilled().Insert(OnPlayerDeath);
		
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		economy.m_OnPlayerBuy.Insert(OnPlayerBuy);
		economy.m_OnPlayerSell.Insert(OnPlayerSell);
		
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		of.m_OnAIKilled.Insert(OnAIKilled);
	}
	
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
	
	void OnPlayerDeath(int playerId)
	{
		TakeXP(playerId, 1);
	}
	
	void OnPlayerBuy(int playerId, int amount)
	{
		int xp = 1 + Math.Floor(amount * 0.01);
		GiveXP(playerId, xp);
	}
	
	void OnPlayerSell(int playerId, int amount)
	{
		int xp = 1 + Math.Floor(amount * 0.01);
		GiveXP(playerId, xp);
	}
	
	void OnAIKilled(IEntity killed, IEntity instigator)
	{
		if(!instigator) return;
		FactionAffiliationComponent fac = EPF_Component<FactionAffiliationComponent>.Find(killed);
		if(!fac) return;
		
		if(fac.GetAffiliatedFaction().GetFactionKey() == m_Config.m_sOccupyingFaction)
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
	
	void TakeXP(int playerId, int num)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		OVT_PlayerData player = OVT_PlayerData.Get(persId);
		player.xp = player.xp - num;
		if(player.xp < 0) player.xp = 0;
		StreamPlayerXP(playerId, player.xp);
	}
	
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
	
	OVT_SkillConfig GetSkill(string key)
	{
		foreach(OVT_SkillConfig skill : m_Skills.m_aSkills)
		{
			if(skill.m_sKey == key) return skill;
		}
		return null;
	}
	
	void StreamPlayerKills(int playerId, int kills)
	{
		Rpc(RpcDo_SetPlayerKills, playerId, kills);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetPlayerKills(int playerId, int kills)
	{
		OVT_PlayerData player = OVT_PlayerData.Get(playerId);
		player.kills = kills;
	}
	
	void StreamPlayerXP(int playerId, int xp)
	{
		Rpc(RpcDo_SetPlayerXP, playerId, xp);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetPlayerXP(int playerId, int xp)
	{
		OVT_PlayerData player = OVT_PlayerData.Get(playerId);
		player.xp = xp;
	}
	
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