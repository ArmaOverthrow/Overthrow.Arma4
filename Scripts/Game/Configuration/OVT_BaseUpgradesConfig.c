//! Config holding the set of base upgrades a base controller runs.
//! Referenced by OVT_BaseControllerComponent.m_BaseUpgradesConfig so the default
//! upgrade set lives in Configs/BaseUpgrades/ instead of on the prefab, and can be
//! overridden per-base or replaced wholesale by mods.
[BaseContainerProps(configRoot: true)]
class OVT_BaseUpgradesConfig : ScriptAndConfig
{
	[Attribute("", UIWidgets.Object, desc: "Upgrades this base will spend resources on, in priority order")]
	ref array<ref OVT_BaseUpgrade> m_aBaseUpgrades;
}
