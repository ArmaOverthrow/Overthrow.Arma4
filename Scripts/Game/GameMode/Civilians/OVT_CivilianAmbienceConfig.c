//------------------------------------------------------------------------------------------------
//! How a civilian moves around its town (implementation.md §3.3).
//!
//! PINGPONG is the parity behaviour - byte-for-byte what the retired town-controller spawner did (two
//! points, patrol/wait at each, cycled forever). WANDER and LOITER were added alongside it so a town
//! reads as a mix of errands, strolls and people standing about rather than one repeated animation.
//! An archetype value nothing knows how to build falls back to PINGPONG rather than standing still,
//! which is a visible-but-harmless downgrade instead of a broken civilian.
//------------------------------------------------------------------------------------------------
enum OVT_ECivilianArchetype
{
	//! Two points, patrol-and-wait at each, cycled forever. Parity with the retired spawner.
	PINGPONG = 0,

	//! N scattered points around the town, short pauses, cycled.
	WANDER = 1,

	//! One spot, one long pause, cycled: somebody who is not going anywhere.
	LOITER = 2
}

//------------------------------------------------------------------------------------------------
//! One KIND of civilian a town crowd may contain (implementation.md §3.3).
//!
//! Authored in Configs/Civilians/CivilianAmbience.conf. Phase 2 ships exactly one - `generic`, the
//! stock Group_CIV.et one-man group - because parity is the phase's whole job; Phase 4 adds the
//! Overthrow-authored variants and their per-type clothing.
//!
//! ⚠ THE PREFAB MUST BE A ONE-MAN GROUP, not a character. The source's tracked entity is an
//! SCR_AIGroup: waypoints attach to groups and GetOnAgentAdded() is the only hook that can dress a
//! member the engine's spawn queue has not created yet (decision D3). A character prefab authored
//! here would spawn, never be given a route, and never be pruned when it died.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sTypeName")]
class OVT_CivilianTypeConfig
{
	[Attribute(desc: "Type name - 'generic', 'businessman', 'dockworker'. Matched EXACTLY and case-sensitively by a town's allow-list")]
	string m_sTypeName;

	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "ONE-MAN GROUP prefab (Group_CIV*.et), never a bare character - the ambient source tracks groups", params: "et")]
	ResourceName m_rGroupPrefab;

	[Attribute(defvalue: "100", desc: "Roll weight within the set a town allows. 0 or below is never picked - that is how a modder disables a type without deleting it")]
	int m_iWeight;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.ComboBox, desc: "Smallest settlement this type may appear in. VILLAGE (the default) means anywhere; CITY keeps a type out of villages and towns", enums: ParamEnumArray.FromEnum(OVT_TownSize))]
	OVT_TownSize m_eMinTownSize;

	[Attribute(uiwidget: UIWidgets.Object, desc: "OPTIONAL per-type clothing. Left empty, the type is dressed from the global CivilianClothes.conf (Phase 4 authors the per-type files)")]
	ref OVT_LoadoutConfig m_Loadout;
}

//------------------------------------------------------------------------------------------------
//! One BEHAVIOUR a civilian may be given (implementation.md §3.3).
//!
//! The weights are rolled per civilian, so one crowd mixes archetypes.
//!
//! ⚠ THE WAIT BOUNDS ONLY STARTED MEANING ANYTHING IN PHASE 3. SpawnWaitWaypoint silently dropped its
//! duration (defect F-C) until then, so every pause in the game was the wait prefab's authored 60 s.
//! Re-authoring these numbers now actually changes what a player sees.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sArchetypeName")]
class OVT_CivilianArchetypeConfig
{
	[Attribute(desc: "Archetype name, for the editor and the logs")]
	string m_sArchetypeName;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.ComboBox, desc: "Which behaviour to build: PINGPONG (two points, there and back), WANDER (m_iPointCount scattered points) or LOITER (one spot, long pause)", enums: ParamEnumArray.FromEnum(OVT_ECivilianArchetype))]
	OVT_ECivilianArchetype m_eArchetype;

	[Attribute(defvalue: "100", desc: "Roll weight. 0 or below is never picked")]
	int m_iWeight;

	[Attribute(defvalue: "2", desc: "WANDER only: how many route points to scatter. Clamped to 2..6 at build time - fewer is not a route, more is one civilian owning a town's worth of waypoint entities")]
	int m_iPointCount;

	[Attribute(defvalue: "15", desc: "Minimum seconds spent at each stop")]
	float m_fWaitMin;

	[Attribute(defvalue: "50", desc: "Maximum seconds spent at each stop")]
	float m_fWaitMax;
}

//------------------------------------------------------------------------------------------------
//! THE AUTHORED TEMPLATE for a town crowd (implementation.md §3.3, §3.7).
//!
//! WHAT IT IS. The declarative half of the feature: what a town crowd is made of (prefab pool by
//! civilian type, behaviour archetypes) and how dense it is (population rate, and the base class's
//! m_iMinCount / m_iMaxCount reused as the density floor and ceiling). It is authored ONCE in
//! Configs/Civilians/CivilianAmbience.conf and is never bound to a town.
//!
//! WHAT BINDS IT TO A TOWN is OVT_TownCivilianSourceConfig, one instance per activated town, built by
//! OVT_CivilianAmbienceManagerComponent.ActivateTown(). That instance holds THIS object by reference
//! and reads everything below THROUGH it - there is deliberately no field-copy step, so a template
//! that gains an attribute later cannot go missing from the per-town instances (decision D5).
//!
//! ⚠ THIS CLASS IS NEVER REGISTERED DIRECTLY. Registering the template itself would produce a source
//! with no town id, no live population and no radius - it would roll 0 civilians forever. The manager
//! is the only thing that reads it, and it only ever registers an instance built from it.
//!
//! THE MODDER SEAM. Add a civilian type, re-weight the archetypes or change the density by editing
//! the .conf; add a whole new ambient source by adding another entry to the same registry. No script
//! change either way - that is the whole point of the shape, and `integration` and
//! `base-defense-migration` are expected to copy it.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sSourceName")]
class OVT_CivilianAmbienceConfig : OVT_AmbientSpawnSourceConfig
{
	[Attribute(desc: "The kinds of civilian this crowd may contain. A town rolls only the ones its size (and, from Phase 4, its own allow-list) permits")]
	ref array<ref OVT_CivilianTypeConfig> m_aTypes;

	[Attribute(desc: "The behaviours a civilian may be given, rolled per civilian by weight")]
	ref array<ref OVT_CivilianArchetypeConfig> m_aArchetypes;

	[Attribute(defvalue: "0.1", desc: "Civilians per head of town population. 0.1 is parity with the retired spawn rate: a 283-population city gets 28, a 38-population village gets 4")]
	float m_fPopulationRate;

	[Attribute(defvalue: "0", desc: "Share of spawns (0..1) placed at a doorway or POI instead of a road point. 0 places everyone on a road (the retired spawner's behaviour, and no world query is ever run); 1 places everyone at a doorway. 0.4 is the authored mix")]
	float m_fBuildingPlacementChance;

	//------------------------------------------------------------------------------------------------
	void OVT_CivilianAmbienceConfig()
	{
		if (!m_aTypes)
			m_aTypes = new array<ref OVT_CivilianTypeConfig>();

		if (!m_aArchetypes)
			m_aArchetypes = new array<ref OVT_CivilianArchetypeConfig>();
	}
}
