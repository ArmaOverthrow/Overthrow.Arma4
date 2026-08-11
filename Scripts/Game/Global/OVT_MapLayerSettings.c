//------------------------------------------------------------------------------------------------
//! One hidden map layer, as it is stored in the player's profile.
//!
//! THE TYPE NAME CARRIES THE MEANING: presence in the list IS "hidden". There is deliberately no
//! m_bVisible companion - a key->bool record can hold "visible" for a key that also appears in a
//! hidden list, and a set of hidden keys cannot disagree with itself. It also means a location type
//! or a canvas layer added in a later version is absent from every existing record and therefore
//! defaults to visible, with no migration and no schema bump.
//!
//! A nested [BaseContainerProps()] struct rather than a bare string in a top-level array, and that
//! is deliberate. `ref array<string>` is PROVEN to survive this settings store, but only one level
//! down, inside a [BaseContainerProps()] class held in a settings module's object array -
//! SCR_FilterSetStorage does exactly that and is round-tripped on every server-browser filter save.
//! NO base-game settings module has a top-level array<string>. Adopting the proven shape removes the
//! risk instead of testing it, and it leaves room for per-entry metadata later without a schema
//! break. The tutorial store settled this; it is inherited here rather than re-derived.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_MapHiddenLayerEntry
{
	//! The namespaced preference key, exactly as OVT_MapLayerPrefsStore.TypeKey or .LayerKey built
	//! it ("type:<ClassName>" or "layer:<layerId>"). The prefix is what keeps the two id spaces from
	//! colliding.
	[Attribute()]
	string m_sKey;
}

//------------------------------------------------------------------------------------------------
//! The per-machine, per-profile record of which map layers this player has hidden.
//!
//! Declaring the class IS the entire registration contract: nothing references this type from a
//! config, and UserSettings.GetModule(string className) resolves off the script type registry.
//! Storage lands in the profile's settings block, which is per PROFILE (so two client profiles on
//! one machine hold independent filters) and works on console.
//!
//! DO NOT read or write this class directly - go through OVT_MapLayerSettingsAccessor, which owns
//! the null guards, the version handling, the console/headless early-out and the disk flush.
//!
//! Deliberately NOT parallel arrays. SCR_HintSettings stores ids and repeat counts in two arrays
//! that have to be length-checked on every load and NUKED when they disagree; the same mistake here
//! would cost a player their whole filter record on any partial write.
//------------------------------------------------------------------------------------------------
class OVT_MapLayerSettings : ModuleGameSettings
{
	//! Schema version of the stored key list. Must match OVT_MapLayerPrefsStore.CURRENT_VERSION; any
	//! other value means the keys no longer mean what they meant and the list is discarded rather
	//! than half-trusted.
	[Attribute("1")]
	int m_iVersion;

	//! Every preference key this profile has hidden. May legitimately come back NULL from the loader
	//! when the member has never been written; every reader must allocate before use.
	[Attribute()]
	ref array<ref OVT_MapHiddenLayerEntry> m_aHidden;
}
