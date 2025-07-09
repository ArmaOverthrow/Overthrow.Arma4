//! Component for tracking placed objects and their association with bases/camps/FOBs
[ComponentEditorProps(category: "Overthrow", description: "Tracks placed objects and their ownership")]
class OVT_PlaceableComponentClass : ScriptComponentClass
{
}

class OVT_PlaceableComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, "Type of placeable object")]
	protected string m_sPlaceableType;
	
	protected string m_sOwnerPersistentId;
	protected string m_sAssociatedBaseId; // Base/Camp/FOB ID this belongs to
	protected EOVTBaseType m_eBaseType; // CAMP, FOB, or BASE
	
	//------------------------------------------------------------------------------------------------
	//! Get the placeable type
	string GetPlaceableType()
	{
		return m_sPlaceableType;
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
	//! Get the base type this object belongs to
	EOVTBaseType GetBaseType()
	{
		return m_eBaseType;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if this object belongs to a specific base/camp/FOB
	bool BelongsTo(string baseId, EOVTBaseType baseType)
	{
		return m_sAssociatedBaseId == baseId && m_eBaseType == baseType;
	}
}

[EPF_ComponentSaveDataType(OVT_PlaceableComponent), BaseContainerProps()]
class OVT_PlaceableDataClass : EPF_ComponentSaveDataClass
{
}

[EDF_DbName.Automatic()]
class OVT_PlaceableData : EPF_ComponentSaveData
{
	string m_sOwnerPersistentId;
	string m_sAssociatedBaseId;
	int m_eBaseType;
	
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_PlaceableComponent placeableComp = OVT_PlaceableComponent.Cast(component);
		
		m_sOwnerPersistentId = placeableComp.GetOwnerPersistentId();
		m_sAssociatedBaseId = placeableComp.GetAssociatedBaseId();
		m_eBaseType = placeableComp.GetBaseType();
		
		return EPF_EReadResult.OK;
	}
	
	override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_PlaceableComponent placeableComp = OVT_PlaceableComponent.Cast(component);
		
		placeableComp.SetOwnerPersistentId(m_sOwnerPersistentId);
		placeableComp.SetAssociatedBase(m_sAssociatedBaseId, m_eBaseType);
		
		return EPF_EApplyResult.OK;
	}
}