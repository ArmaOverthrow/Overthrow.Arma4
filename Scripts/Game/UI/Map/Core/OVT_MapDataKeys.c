//! Shared data-key constants for OVT_MapLocationData's typed maps (tech debt T3).
//!
//! OVT_MapLocationData stores per-record information in map<string, ...> collections, so every key is
//! an UNREGISTERED string: a typo does not fail to compile, it silently yields the default value and
//! the panel renders a blank row. Several key sets are duplicated by convention between location
//! types - House/Warehouse share an ownership shape, FOB/Camp share owner/persistentId - which is
//! exactly where a typo goes unnoticed longest.
//!
//! These constants are the single spelling for those shared keys. They are deliberately NOT applied
//! to every shipped type: only types the map/location-types feature already edits use them, so this
//! stays a debt reduction rather than a repo-wide refactor.
//!
//! Type-private keys (Town's modifiers, POI's per-record imageset, Shop's type info) stay local to
//! their own class - nothing is gained by centralising a key with exactly one writer and one reader.
class OVT_MapDataKeys
{
	//! Persistent ID of the player who owns this location. The panel shell special-cases this key to
	//! render its "Owner" row (OVT_OverthrowMapUI.c:419-445), so a record that stores only RENTER
	//! shows no owner line.
	static const string OWNER = "owner";

	//! Persistent ID of the player renting this location.
	static const string RENTER = "renter";

	//! True when the local player owns this location.
	static const string IS_OWNED = "isOwned";

	//! True when the local player rents this location.
	static const string IS_RENTED = "isRented";

	//! True when this building is the local player's registered home / respawn point.
	static const string IS_HOME = "isHome";

	//! Persistent ID of the record itself (FOB/Camp), not of a player. Distinct from OWNER.
	static const string PERSISTENT_ID = "persistentId";

	//! Per-record OVERRIDE of the type's m_fVisibilityZoom (BUG-138). Optional float: when present and
	//! >= 0 it replaces OVT_MapLocationType.GetVisibilityZoom() for this record alone, in BOTH
	//! OVT_MapLocationElement.ShouldUseSmallIcon and SetVisible. Absent (or negative) = use the type
	//! value, which is what every record that does not write this key does.
	//! `0` means "always visible", so the reader must use a NEGATIVE sentinel, not 0, to detect absence.
	static const string VISIBILITY_ZOOM = "visibilityZoom";
}
