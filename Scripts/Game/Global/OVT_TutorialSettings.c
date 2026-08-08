//------------------------------------------------------------------------------------------------
//! One remembered tutorial id, as it is stored in the player's profile.
//!
//! A nested [BaseContainerProps()] struct rather than a bare string in a top-level array, and that
//! is deliberate (plan decision D7). `ref array<string>` is PROVEN to survive this settings store,
//! but only one level down, inside a [BaseContainerProps()] class held in a settings module's object
//! array - SCR_FilterSetStorage does exactly that and is round-tripped on every server-browser
//! filter save. NO base-game settings module has a top-level array<string>. Adopting the proven
//! shape removes the risk instead of testing it, and it leaves room for per-entry metadata (a seen
//! count, a timestamp) later without a schema break.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_SeenTutorialEntry
{
	//! The tutorial entry id, exactly as authored on the entry config. Lowercase kebab, immutable,
	//! never reused - see the entry-id scheme in the plan's contract section.
	[Attribute()]
	string m_sId;
}

//------------------------------------------------------------------------------------------------
//! The per-machine, per-profile record of which tutorials this player has already been shown.
//!
//! Declaring the class IS the entire registration contract: nothing references this type from a
//! config, and UserSettings.GetModule(string className) resolves off the script type registry.
//! Storage lands in $profile:.save/settings/ReforgerGameSettings.conf, which is per PROFILE (so two
//! client profiles on one machine have independent seen state) and works on console.
//!
//! DO NOT read or write this class directly - go through OVT_TutorialSettingsAccessor, which owns
//! the null guards, the version handling, the console/headless early-out and the disk flush.
//!
//! Deliberately NOT parallel arrays. SCR_HintSettings stores ids and repeat counts in two arrays
//! that have to be length-checked on every load and NUKED when they disagree; the same mistake here
//! would cost a player their whole seen record on any partial write.
//------------------------------------------------------------------------------------------------
class OVT_TutorialSettings : ModuleGameSettings
{
	//! Schema version of the stored id list. Must match OVT_TutorialSeenStore.CURRENT_VERSION; any
	//! other value means the ids no longer mean what they meant and the list is discarded rather
	//! than half-trusted.
	[Attribute("1")]
	int m_iVersion;

	//! The player's "Don't show tips again" choice. Survives a version bump on purpose - it is a
	//! preference, not a record of ids, and silently re-enabling tips would look like a bug.
	[Attribute("false")]
	bool m_bTipsDisabled;

	//! Every tutorial id this profile has already been shown. May legitimately come back NULL from
	//! the loader when the member has never been written; every reader must allocate before use.
	[Attribute()]
	ref array<ref OVT_SeenTutorialEntry> m_aSeen;
}
