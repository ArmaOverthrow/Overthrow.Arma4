[BaseContainerProps(configRoot : true)]
class OVT_PlaceablesConfig
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_Placeable> m_aPlaceables;		
}

[BaseContainerProps(), SCR_BaseContainerLocalizedTitleField("m_sName")]
class OVT_Placeable
{
	[Attribute()]
	string m_sName;
	
	[Attribute()]
	string m_sTitle;

	[Attribute()]
	string m_sDescription;
		
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Object Prefabs", params: "et")]
	ref array<ResourceName> m_aPrefabs;
	
	[Attribute("", UIWidgets.ResourceNamePicker, "", "edds")]
	ResourceName m_tPreview;
	
	[Attribute(defvalue: "100", desc: "Cost (multiplied by difficulty)")]
	int m_iCost;
	
	[Attribute(defvalue: "1", desc: "XP gain when placed")]
	int m_iRewardXP;
		
	[Attribute(defvalue: "0", desc: "Place on walls")]
	bool m_bPlaceOnWall;
	
	[Attribute(defvalue: "0", desc: "Can place it anywhere")]
	bool m_bIgnoreLocation;
	
	[Attribute(defvalue: "0", desc: "Cannot place near towns or bases")]
	bool m_bAwayFromTownsBases;

	[Attribute(defvalue: "0", desc: "Cannot place near bases")]
	bool m_bAwayFromBases;
	
	[Attribute(defvalue: "0", desc: "Must be placed near a town")]
	bool m_bNearTown;
	
	[Attribute("", UIWidgets.Object)]
	ref OVT_PlaceableHandler handler;
}