//------------------------------------------------------------------------------------------------
//! One LOCAL option's value, as it is stored in the player's profile.
//!
//! A nested `[BaseContainerProps()]` record inside an object array, never a top-level `array<string>`
//! (D10). `OVT_MapHiddenLayerEntry` is the proven shape this one copies.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_LocalOptionEntry
{
	//! The option id, exactly as registered.
	[Attribute()]
	string m_sId;

	//! The value, in the option's own string encoding.
	[Attribute()]
	string m_sValue;
}

//------------------------------------------------------------------------------------------------
//! The per-machine, per-profile record of every LOCAL option value.
//!
//! Declaring the class is the whole registration contract (D10): nothing references this type from
//! a config, and `UserSettings.GetModule(string className)` resolves off the script type registry.
//! Read or write it only through `OVT_OptionsLocalSettingsAccessor`.
//------------------------------------------------------------------------------------------------
class OVT_OptionsLocalSettings : ModuleGameSettings
{
	//! Schema version of the stored entries. Must match `OVT_OptionsLocalStore.CURRENT_VERSION`.
	[Attribute("1")]
	int m_iVersion;

	//! Every stored LOCAL value. May legitimately come back null from the loader when the member has
	//! never been written; every reader must allocate before use.
	[Attribute()]
	ref array<ref OVT_LocalOptionEntry> m_aEntries;
}
