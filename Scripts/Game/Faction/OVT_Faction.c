class OVT_Faction : SCR_Faction
{
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction groups (all)", params: "et", category: "Faction Groups")]
	ref array<ResourceName> m_aGroupPrefabSlots;
	
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction groups (Light Infantry)", params: "et", category: "Faction Groups")]
	ref array<ResourceName> m_aGroupInfantryPrefabSlots;
	
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction groups (Heavy Infantry)", params: "et", category: "Faction Groups")]
	ref array<ResourceName> m_aHeavyInfantryPrefabSlots;
	
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction groups (AT)", params: "et", category: "Faction Groups")]
	ref array<ResourceName> m_aGroupATPrefabSlots;
	
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction groups (Special Forces)", params: "et", category: "Faction Groups")]
	ref array<ResourceName> m_aGroupSpecialPrefabSlots;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Sniper or Sharpshooter Prefab", params: "et", category: "Faction Soldiers")]
	ResourceName m_aSniperPrefab;
	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Spotter Prefab", params: "et", category: "Faction Soldiers")]
	ResourceName m_aSpotterPrefab;
	
}