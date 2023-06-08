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
	
	void Init(IEntity owner)
	{
		
	}
	
	void PostGameStart()
	{		
		OVT_OverthrowGameMode game = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());		
		game.GetOnPlayerKilled().Insert(OnPlayerDeath);
		
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		economy.m_OnPlayerBuy.Insert(OnPlayerBuy);
		economy.m_OnPlayerSell.Insert(OnPlayerSell);
		
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		of.m_OnAIKilled.Insert(OnAIKilled);
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
	
	protected void TakeXP(int playerId, int num)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		OVT_PlayerData player = OVT_PlayerData.Get(persId);
		player.xp = player.xp - num;
		if(player.xp < 0) player.xp = 0;
		StreamPlayerXP(playerId, player.xp);
	}
	
	protected void GiveXP(int playerId, int num)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		OVT_PlayerData player = OVT_PlayerData.Get(persId);
		player.xp = player.xp + num;
		StreamPlayerXP(playerId, player.xp);
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
	
	void ~OVT_SkillManagerComponent()
	{
		
	}
}