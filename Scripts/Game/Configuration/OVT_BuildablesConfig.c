[BaseContainerProps(configRoot : true)]
class OVT_BuildablesConfig
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_Buildable> m_aBuildables;		
}

[BaseContainerProps(), SCR_BaseContainerLocalizedTitleField("m_sName")]
class OVT_Buildable
{
	[Attribute()]
	string m_sName;
	
	[Attribute()]
	string m_sTitle;
	
	[Attribute()]
	string m_sDescription;
		
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Structure Prefabs", params: "et")]
	ref array<ResourceName> m_aPrefabs;
	
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Furniture Prefabs", params: "et")]
	ref array<ResourceName> m_aFurniturePrefabs;
	
	[Attribute("", UIWidgets.ResourceNamePicker, "", "edds")]
	ResourceName m_tPreview;
	
	[Attribute(defvalue: "100", desc: "Cost (multiplied by difficulty)")]
	int m_iCost;	
	
	[Attribute(defvalue: "25", desc: "XP gain when built")]
	int m_iRewardXP;
	
	[Attribute(defvalue: "0", desc: "Can build at a base")]
	bool m_bBuildAtBase;
	
	[Attribute(defvalue: "0", desc: "Can build in a town")]
	bool m_bBuildInTown;
	
	[Attribute(defvalue: "0", desc: "Can build in a village")]
	bool m_bBuildInVillage;
	
	[Attribute(defvalue: "0", desc: "Can build at an FOB")]
	bool m_bBuildAtFOB;
	
	[Attribute(defvalue: "0", desc: "Can build at a Camp")]
	bool m_bBuildAtCamp;
	
	[Attribute("", UIWidgets.Object)]
	ref OVT_PlaceableHandler handler;

	//! APPENDED AFTER handler ON PURPOSE - a .conf entry that authors neither field is unchanged, and
	//! an empty list means money-only and instant, exactly as before this existed.
	[Attribute("", UIWidgets.Object, desc: "Resource requirements (empty = money only, instant)")]
	ref array<ref OVT_BuildableResourceRequirement> m_aResourceRequirements;

	[Attribute("", UIWidgets.ResourceNamePicker, "", "et", desc: "Construction site prefab (empty = generic)")]
	ResourceName m_SitePrefab;
}

//------------------------------------------------------------------------------------------------
//! One authored resource requirement of a buildable, at the DEFAULT difficulty.
//!
//! The quantity here is never the quantity charged: OVT_ResourceRequirements.ScaleForDifficulty()
//! puts every entry through OVT_OverthrowConfigComponent.GetBuildableResourceCost(), which is the
//! single call behind both the figure shown on the build card and the figure the site consumes.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(), SCR_BaseContainerLocalizedTitleField("m_sResourceId")]
class OVT_BuildableResourceRequirement
{
	[Attribute(desc: "Stable resource id from resources.conf, e.g. 'timber'")]
	string m_sResourceId;

	[Attribute(defvalue: "0", desc: "Units required at the default difficulty")]
	int m_iQuantity;
}