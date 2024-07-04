[BaseContainerProps(), SCR_BaseContainerCustomTitleResourceName("m_sName", true)]
class OVT_LoadoutSlot
{
	[Attribute("Name", desc: "")]
	string m_sName;

	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Available choices", params: "et")]
	ref array<ResourceName> m_aChoices;

	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Items to store", params: "et")]
	ref array<ResourceName> m_aStoredItems;
}

[BaseContainerProps(configRoot: true)]
class OVT_LoadoutConfig
{
	[Attribute( desc: "Loadout Areas" )]
	ref array<ref OVT_LoadoutSlot> m_aSlots;
}