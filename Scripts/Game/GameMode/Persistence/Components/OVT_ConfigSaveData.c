[EPF_ComponentSaveDataType(OVT_OverthrowConfigComponent), BaseContainerProps()]
class OVT_ConfigSaveDataClass : EPF_ComponentSaveDataClass
{
};

[EDF_DbName.Automatic()]
class OVT_ConfigSaveData : EPF_ComponentSaveData
{	
	ref OVT_DifficultySettings m_Difficulty;
	
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{		
		OVT_OverthrowConfigComponent config = OVT_OverthrowConfigComponent.Cast(component);
		
		m_Difficulty = config.m_Difficulty;
		
		return EPF_EReadResult.OK;
	}
	
	override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_OverthrowConfigComponent config = OVT_OverthrowConfigComponent.Cast(component);
		
		config.m_Difficulty = m_Difficulty;
		
		return EPF_EApplyResult.OK;
	}
}