//! Marks a curated sniper position. OVT_SniperMarkerPlacementProvider
//! (Deployment_BaseSniperPositions.conf) finds these within the deployment's search radius and mans
//! each one with a two-man sniper team facing the marker's own forward direction - which is why the
//! marker's ROTATION matters as much as its position.
//!
//! m_iMinimumThreat is a PER-MARKER escalation gate and the provider is the only thing that reads it:
//! a marker is skipped while the occupying faction's threat is below its own threshold, so a designer
//! can author exposed forward positions that only get manned once the campaign is hot.
[ComponentEditorProps(category: "Overthrow", description: "Marks a curated sniper position for base defense")]
class OVT_SniperPositionComponentClass : ScriptComponentClass
{
}

class OVT_SniperPositionComponent : ScriptComponent
{
	[Attribute("0", UIWidgets.EditBox, "Minimum occupying-faction threat level before this position is manned (0 = always)")]
	int m_iMinimumThreat;
}
