[EPF_ComponentSaveDataType(OVT_EconomyManagerComponent), BaseContainerProps()]
class OVT_EconomySaveDataClass : EPF_ComponentSaveDataClass
{
};

[EDF_DbName.Automatic()]
class OVT_EconomySaveData : EPF_ComponentSaveData
{	
	int m_iResistanceMoney = 0;
	float m_fResistanceTax = 0;
	
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{		
		OVT_EconomyManagerComponent economy = OVT_EconomyManagerComponent.Cast(component);
		
		m_iResistanceMoney = economy.GetResistanceMoney();
		m_fResistanceTax = economy.m_fResistanceTax;
		
		return EPF_EReadResult.OK;
	}
	
	override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_EconomyManagerComponent economy = OVT_EconomyManagerComponent.Cast(component);
		
		economy.m_iResistanceMoney = m_iResistanceMoney;
		economy.m_fResistanceTax = m_fResistanceTax;
		
		return EPF_EApplyResult.OK;
	}
}