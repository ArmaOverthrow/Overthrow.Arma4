//! Component for tracking built structures and their association with bases/camps/FOBs
[ComponentEditorProps(category: "Overthrow", description: "Tracks built structures and their ownership")]
class OVT_BuildableComponentClass : ScriptComponentClass
{
}

class OVT_BuildableComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, "Type of buildable structure")]
	protected string m_sBuildableType;
	
	protected string m_sOwnerPersistentId;
	protected string m_sAssociatedBaseId; // Base/Camp/FOB ID this belongs to
	protected EOVTBaseType m_eBaseType; // CAMP, FOB, or BASE
	
	//------------------------------------------------------------------------------------------------
	//! Get the buildable type
	string GetBuildableType()
	{
		return m_sBuildableType;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Set the owner persistent ID
	void SetOwnerPersistentId(string ownerPersistentId)
	{
		m_sOwnerPersistentId = ownerPersistentId;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get the owner persistent ID
	string GetOwnerPersistentId()
	{
		return m_sOwnerPersistentId;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Set the associated base/camp/FOB
	void SetAssociatedBase(string baseId, EOVTBaseType baseType)
	{
		m_sAssociatedBaseId = baseId;
		m_eBaseType = baseType;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get the associated base/camp/FOB ID
	string GetAssociatedBaseId()
	{
		return m_sAssociatedBaseId;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get the base type this structure belongs to
	EOVTBaseType GetBaseType()
	{
		return m_eBaseType;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if this structure belongs to a specific base/camp/FOB
	bool BelongsTo(string baseId, EOVTBaseType baseType)
	{
		return m_sAssociatedBaseId == baseId && m_eBaseType == baseType;
	}
}

[EPF_ComponentSaveDataType(OVT_BuildableComponent), BaseContainerProps()]
class OVT_BuildableDataClass : EPF_ComponentSaveDataClass
{
}

[EDF_DbName.Automatic()]
class OVT_BuildableData : EPF_ComponentSaveData
{
	string m_sOwnerPersistentId;
	string m_sAssociatedBaseId;
	int m_eBaseType;
	
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_BuildableComponent buildableComp = OVT_BuildableComponent.Cast(component);
		
		m_sOwnerPersistentId = buildableComp.GetOwnerPersistentId();
		m_sAssociatedBaseId = buildableComp.GetAssociatedBaseId();
		m_eBaseType = buildableComp.GetBaseType();
		
		return EPF_EReadResult.OK;
	}
	
	override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_BuildableComponent buildableComp = OVT_BuildableComponent.Cast(component);
		
		buildableComp.SetOwnerPersistentId(m_sOwnerPersistentId);
		buildableComp.SetAssociatedBase(m_sAssociatedBaseId, m_eBaseType);
		
		return EPF_EApplyResult.OK;
	}
}