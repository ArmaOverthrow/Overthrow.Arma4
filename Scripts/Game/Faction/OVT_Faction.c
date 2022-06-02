class OVT_Faction : SCR_Faction
{
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Faction groups", params: "et", category: "Faction Groups")]
	ref array<ResourceName> m_aGroupPrefabSlots;
}