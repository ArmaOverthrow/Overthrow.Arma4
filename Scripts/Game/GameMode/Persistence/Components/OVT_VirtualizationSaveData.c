[EPF_ComponentSaveDataType(OVT_VirtualizationManagerComponent), BaseContainerProps()]
class OVT_VirtualizationSaveDataClass : EPF_ComponentSaveDataClass
{
};

[EDF_DbName.Automatic()]
class OVT_VirtualizationSaveData : EPF_ComponentSaveData
{
	ref array<ref OVT_VirtualizedGroupData> m_aGroups;
	
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{		
		OVT_VirtualizationManagerComponent virt = OVT_VirtualizationManagerComponent.Cast(component);
		
		m_aGroups = new array<ref OVT_VirtualizedGroupData>;
		virt.GetGroupsArray(m_aGroups);
		
		return EPF_EReadResult.OK;
	}
	
	override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_VirtualizationManagerComponent virt = OVT_VirtualizationManagerComponent.Cast(component);
			
		virt.LoadGroupsArray(m_aGroups);		
				
		return EPF_EApplyResult.OK;
	}
}