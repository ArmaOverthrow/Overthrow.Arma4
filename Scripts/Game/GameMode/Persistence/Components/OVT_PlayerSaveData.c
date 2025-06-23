[EPF_ComponentSaveDataType(OVT_PlayerManagerComponent), BaseContainerProps()]
class OVT_PlayerSaveDataClass : EPF_ComponentSaveDataClass
{
};

[EDF_DbName.Automatic()]
class OVT_PlayerSaveData : EPF_ComponentSaveData
{
	ref map<string, ref OVT_PlayerData> m_mPlayers;
	
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{		
		Print("[Overthrow] PlayerSaveData.ReadFrom called - component: " + component);
		OVT_PlayerManagerComponent players = OVT_PlayerManagerComponent.Cast(component);
		Print("[Overthrow] PlayerSaveData.ReadFrom - cast result: " + players);
		
		m_mPlayers = new map<string, ref OVT_PlayerData>;
		
		if(!players)
		{
			Print("[Overthrow] Error saving players - component is null");
			m_mPlayers = new map<string, ref OVT_PlayerData>;
			return EPF_EReadResult.OK;
		}
		
		Print("[Overthrow] PlayerSaveData.ReadFrom - players.m_mPlayers: " + players.m_mPlayers);
		if(!players.m_mPlayers)
		{
			Print("[Overthrow] Error saving players - players map is null");
			m_mPlayers = new map<string, ref OVT_PlayerData>;
			return EPF_EReadResult.OK;
		}
		
		for(int t=0; t< players.m_mPlayers.Count(); t++)
		{
			string persId = players.m_mPlayers.GetKey(t);
			OVT_PlayerData player = players.m_mPlayers.GetElement(t);
			
			// Validate player data before saving
			if(!persId || persId.IsEmpty())
			{
				Print("[Overthrow] WARNING: Skipping player with empty/null ID");
				continue;
			}
			
			if(!player)
			{
				Print("[Overthrow] WARNING: Skipping null player data for ID: " + persId);
				continue;
			}
			
			// Additional validation - ensure the player has been properly initialized
			if(player.name.IsEmpty() && !player.initialized && player.money == 0)
			{
				Print("[Overthrow] WARNING: Skipping uninitialized player with ID: " + persId);
				continue;
			}
			
			Print("[Overthrow] Saving player " + persId + " (" + player.name + ")");
			m_mPlayers[persId] = player;
		}
		
		return EPF_EReadResult.OK;
	}
	
	override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_PlayerManagerComponent players = OVT_PlayerManagerComponent.Cast(component);
		
		if(!m_mPlayers)
		{
			Print("[Overthrow] No player save data found");
			return EPF_EApplyResult.OK;
		}
		if(!players.m_mPlayers)
		{
			players.m_mPlayers = new map<string, ref OVT_PlayerData>();
		}
		
		for(int t=0; t< m_mPlayers.Count(); t++)
		{
			string persId = m_mPlayers.GetKey(t);
			OVT_PlayerData player = m_mPlayers.GetElement(t);
			
			// Validate player data before loading
			if(!persId || persId.IsEmpty())
			{
				Print("[Overthrow] WARNING: Skipping loading player with empty/null ID");
				continue;
			}
			
			if(!player)
			{
				Print("[Overthrow] WARNING: Skipping loading null player data for ID: " + persId);
				continue;
			}
			
			players.m_mPlayers[persId] = player;
			players.m_OnPlayerDataLoaded.Invoke(player, persId);
			Print("[Overthrow] Loading player " + persId + " (" + player.name + ")");
		}
				
		return EPF_EApplyResult.OK;
	}
}