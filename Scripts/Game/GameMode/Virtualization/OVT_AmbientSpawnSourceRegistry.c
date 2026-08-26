//------------------------------------------------------------------------------------------------
//! A named collection of ambient spawn sources, authored in a .conf (implementation.md §3.4).
//!
//! Shaped exactly like OVT_DeploymentRegistry: a configRoot ScriptAndConfig holding a ref array of
//! configs, so a consumer feature ships its own .conf and points the manager attribute
//! (OVT_VirtualizationManagerComponent.m_AmbientRegistry) at it with no code change.
//!
//! ⚠ CORE SHIPS THIS EMPTY. There is deliberately no authored ambient content anywhere in core -
//! `civilians` writes the first one. The feature's Definition of Done greps Configs/ to prove it.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sRegistryName")]
class OVT_AmbientSpawnSourceRegistry : ScriptAndConfig
{
	[Attribute(desc: "Name of this ambient spawn source registry")]
	string m_sRegistryName;

	[Attribute(desc: "All ambient spawn source configurations")]
	ref array<ref OVT_AmbientSpawnSourceConfig> m_aSources;

	//------------------------------------------------------------------------------------------------
	void OVT_AmbientSpawnSourceRegistry()
	{
		if (!m_aSources)
			m_aSources = new array<ref OVT_AmbientSpawnSourceConfig>();
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves a source config by its authored name - the lookup a consumer uses when it knows what
	//! it wants ("town_civilian") but not which registry slot holds it.
	//! \param[in] name The m_sSourceName to find.
	//! \return The config, or null when nothing in this registry carries that name.
	OVT_AmbientSpawnSourceConfig FindByName(string name)
	{
		if (!m_aSources)
			return null;

		foreach (OVT_AmbientSpawnSourceConfig config : m_aSources)
		{
			if (config && config.m_sSourceName == name)
				return config;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Number of source configs authored in this registry; 0 for an empty (core-default) one.
	int GetSourceCount()
	{
		if (!m_aSources)
			return 0;

		return m_aSources.Count();
	}
}
