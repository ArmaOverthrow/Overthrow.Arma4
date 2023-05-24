[EPF_ComponentSaveDataType(OVT_PlayerManagerComponent), BaseContainerProps()]
class OVT_PlayerSaveDataClass : EPF_ComponentSaveDataClass
{
};

[EDF_DbName.Automatic()]
class OVT_PlayerSaveData : EPF_ComponentSaveData
{
	map<string, ref OVT_PlayerData> m_mPlayers;
	
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{		
		OVT_PlayerManagerComponent players = OVT_PlayerManagerComponent.Cast(component);
		
		m_mPlayers = players.m_mPlayers;
		
		return EPF_EReadResult.OK;
	}
	
	override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_PlayerManagerComponent players = OVT_PlayerManagerComponent.Cast(component);
		
		players.m_mPlayers = m_mPlayers;
				
		return EPF_EApplyResult.OK;
	}
}