[EPF_ComponentSaveDataType(SCR_CharacterControllerComponent), BaseContainerProps()]
class OVT_CharacterControllerComponentSaveDataClass : EPF_CharacterControllerComponentSaveDataClass
{
	
};

[EDF_DbName.Automatic()]
class OVT_CharacterControllerComponentSaveData : EPF_CharacterControllerComponentSaveData
{
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		SCR_CharacterControllerComponent characterController = SCR_CharacterControllerComponent.Cast(component);

		m_eStance = characterController.GetStance();
		
		//Ignore what character is holding in their hands for now, until EPF bug is fixed

		return EPF_EReadResult.OK;
	}
}