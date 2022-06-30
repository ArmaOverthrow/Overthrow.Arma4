[BaseContainerProps()]
class OVT_OverthrowSaveStruct : SCR_MissionStruct
{
	[Attribute()]
	protected ref OVT_EconomyStruct m_EconomyStruct;
	
	[Attribute()]
	protected ref OVT_RealEstateStruct m_RealEstateStruct;
	
	[Attribute()]
	protected ref OVT_VehiclesStruct m_VehiclesStruct;
	
	[Attribute()]
	protected ref OVT_OccupyingFactionStruct m_OccupyingStruct;
	
	protected string m_sOccupyingFaction;
	
	override bool Serialize()
	{
		if (m_EconomyStruct && !m_EconomyStruct.Serialize())
			return false;
		
		if (m_RealEstateStruct && !m_RealEstateStruct.Serialize())
			return false;
		
		if (m_VehiclesStruct && !m_VehiclesStruct.Serialize())
			return false;
		
		if (m_OccupyingStruct && !m_OccupyingStruct.Serialize())
			return false;
		
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		m_sOccupyingFaction = config.m_sOccupyingFaction;
		
		return true;
	}
	override bool Deserialize()
	{
		if (m_EconomyStruct && !m_EconomyStruct.Deserialize())
			return false;
		
		if (m_RealEstateStruct && !m_RealEstateStruct.Deserialize())
			return false;
		
		if (m_VehiclesStruct && !m_VehiclesStruct.Deserialize())
			return false;
		
		if (m_OccupyingStruct && !m_OccupyingStruct.Deserialize())
			return false;
		
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		config.SetOccupyingFaction(m_sOccupyingFaction);
		
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		mode.DoStartGame();
		
		return true;
	}
	
	void OVT_OverthrowSaveStruct()
	{
		RegV("m_EconomyStruct");
		RegV("m_RealEstateStruct");
		RegV("m_VehiclesStruct");
		RegV("m_OccupyingStruct");
		RegV("m_sOccupyingFaction");
	}
}