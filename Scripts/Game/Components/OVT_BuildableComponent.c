//! Component for tracking built structures and their association with bases/camps/FOBs
[ComponentEditorProps(category: "Overthrow", description: "Tracks built structures and their ownership")]
class OVT_BuildableComponentClass : ScriptComponentClass
{
}

class OVT_BuildableComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, "Type of buildable structure")]
	protected string m_sBuildableType;
	
	//! Replicated so client-side removal mode can check ownership (set server-side only)
	[RplProp()]
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
		Replication.BumpMe();
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