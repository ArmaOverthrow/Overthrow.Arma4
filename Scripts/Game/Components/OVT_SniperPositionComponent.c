//! Marks a curated sniper position. The occupying faction's sniper-position base upgrade
//! (OVT_BaseUpgradeSniperPosition) finds these within base range and mans each one with a
//! two-man sniper team facing the owner entity's forward direction.
[ComponentEditorProps(category: "Overthrow", description: "Marks a curated sniper position for base defense")]
class OVT_SniperPositionComponentClass : ScriptComponentClass
{
}

class OVT_SniperPositionComponent : ScriptComponent
{
	[Attribute("0", UIWidgets.EditBox, "Minimum occupying-faction threat level before this position is manned (0 = always)")]
	int m_iMinimumThreat;
}
