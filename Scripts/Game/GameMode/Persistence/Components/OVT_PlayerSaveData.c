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
		OVT_PlayerManagerComponent players = OVT_PlayerManagerComponent.Cast(component);
		
		m_mPlayers = players.m_mPlayers;
		
		return EPF_EReadResult.OK;
	}
	
	override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_PlayerManagerComponent players = OVT_PlayerManagerComponent.Cast(component);
		
		if(!m_mPlayers) return;
		
		for(int t=0; t< m_mPlayers.Count(); t++)
		{
			string persId = m_mPlayers.GetKey(t);
			OVT_PlayerData player = m_mPlayers.GetElement(t);
			players.m_mPlayers[persId] = player;
			players.m_OnPlayerDataLoaded.Invoke(player, persId);
		}
				
		return EPF_EApplyResult.OK;
	}
}