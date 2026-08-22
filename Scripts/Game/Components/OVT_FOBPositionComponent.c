//! Marks a curated site for the occupying faction's forward operating base. The objective director
//! finds these inside the siting band between the base supplying an operation and the objective it is
//! working on, and PREFERS them over anything its own sampler generates.
//!
//! THE COMPONENT IS THE QUERYABLE THING, not the entity, exactly as OVT_SniperPositionComponent works:
//! a map author can drop the OVT_FOBPosition marker prefab, or attach this component to any existing
//! entity that happens to stand where a forward base belongs. Discovery is a QueryEntitiesBySphere
//! plus a FindComponent filter, which is the shape OVT_SniperMarkerPlacementProvider already proves.
//!
//! ⚠ NO INSTANCES OF THIS EXIST IN ANY SHIPPED WORLD, AND THE FEATURE DOES NOT DEPEND ON ANY. Authored
//! sites live in world layers and placing them is a Workbench world-editing job; the director's
//! GENERATED path is the primary one and is tuned to ship standing alone. This component is the
//! optimisation a map author can apply on top, not a prerequisite. If you are reading this because
//! forward bases are landing somewhere silly, the fix is either a marker here or the sampler's
//! constants in OVT_ObjectiveDirectorComponent - and the log line names the attempt count either way.
//!
//! ⚠ A MARKER IS A PREFERENCE, NOT AN EXEMPTION. The director still applies every hard test to it: in
//! the band between the supplying base and the objective, out of the ocean, clear of every base, camp,
//! resistance forward base and resistance-held town, level enough, and with room to put a structure
//! down. The world moves under a marker placed months ago in the editor - a player can build a camp on
//! top of one - so a marker that no longer qualifies is simply skipped and the generated sampler
//! answers instead.
//!
//! ⚠ ONE ATTRIBUTE, AND DELIBERATELY ONLY ONE. Anything a marker could be given here that the director
//! did not actually read would be a knob that lies to map authors; m_bEnabled is read on every siting
//! pass. A per-marker clearance override was considered and cut for exactly that reason - the
//! exclusions each carry their own radius already.
[ComponentEditorProps(category: "Overthrow", description: "Marks a curated site for an occupying-faction forward operating base")]
class OVT_FOBPositionComponentClass : ScriptComponentClass
{
}

class OVT_FOBPositionComponent : ScriptComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Whether this site may be used at all. Uncheck to retire a marker without deleting it from the world layer")]
	bool m_bEnabled;
}
