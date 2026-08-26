//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Overthrow/Components", description: "A building ordered but not yet paid for in resources")]
class OVT_ConstructionSiteComponentClass : OVT_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! What a construction site remembers: which buildable was ordered, which of its prefabs, and the
//! orientation it will stand at.
//!
//! The indices are the same ones the build menu sent, so finishing a site is literally re-entering
//! OVT_ResistanceFactionManager.FinishBuild() with them - there is no second spawn path and no second
//! ordering (D13).
//!
//! ONE REPLICATED FIELD. m_sBuildableName exists so a client's action label can name the building
//! without racing the buildables config through an index lookup; the indices themselves are server
//! and save state and never ride the wire. The name is NOT persisted - it is re-derived from the
//! config when the payload is applied, so renaming a buildable renames every standing site.
//!
//! NO PERSISTENCE RULE OF ITS OWN (D16). The site carries OVT_BuildableComponent, so the Overthrow
//! Buildable EntityPersistenceConfig already claims it; its serializer is listed there.
//------------------------------------------------------------------------------------------------
class OVT_ConstructionSiteComponent : OVT_Component
{
	//-----------------------------------------------------------------------------------------------
	// MEMBER VARIABLES
	//-----------------------------------------------------------------------------------------------

	//! Index into the buildables config. -1 until the site is initialised or loaded.
	protected int m_iBuildableIndex = -1;

	//! Index into that buildable's prefab list. -1 until the site is initialised or loaded.
	protected int m_iPrefabIndex = -1;

	//! The orientation the finished building will stand at, as the build menu sent it.
	protected vector m_vAngles;

	//! The whole replicated surface: the building's config title, so a client can label the action.
	[RplProp()]
	protected string m_sBuildableName;

	//-----------------------------------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \param[in] owner The site entity.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		// A throwaway ItemPreview icon: no site, no RplComponent complaint.
		if (IsPreviewInstance(owner))
			return;

		// Without an RplComponent the site has no RplId, so no client can name it in a build request
		// and its name never replicates (BUG-193 is exactly this found late).
		if (!GetRpl())
			Print(string.Format("[Overthrow] OVT_ConstructionSiteComponent on '%1' has no RplComponent. No client can ask the server to finish this site.", OVT_PrefabUtils.GetPrefabName(owner)), LogLevel.ERROR);
	}

	//-----------------------------------------------------------------------------------------------
	// SERVER API
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Stamps a freshly placed site with what was ordered. Server only.
	//! \param[in] buildableIndex Index into the buildables config.
	//! \param[in] prefabIndex Index into that buildable's prefab list.
	//! \param[in] angles The orientation the finished building will stand at.
	//! \param[in] buildableName The buildable's config title, for the client's action label.
	void Initialize(int buildableIndex, int prefabIndex, vector angles, string buildableName)
	{
		m_iBuildableIndex = buildableIndex;
		m_iPrefabIndex = prefabIndex;
		m_vAngles = angles;
		m_sBuildableName = buildableName;

		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Restores a site from its save payload and re-derives the replicated name from the config.
	//! \param[in] buildableIndex Index into the buildables config.
	//! \param[in] prefabIndex Index into that buildable's prefab list.
	//! \param[in] angles The orientation the finished building will stand at.
	void ApplyPersisted(int buildableIndex, int prefabIndex, vector angles)
	{
		m_iBuildableIndex = buildableIndex;
		m_iPrefabIndex = prefabIndex;
		m_vAngles = angles;
		m_sBuildableName = ResolveBuildableName(buildableIndex);

		Replication.BumpMe();
	}

	//-----------------------------------------------------------------------------------------------
	// CLIENT-SAFE GETTERS
	//-----------------------------------------------------------------------------------------------

	//! \return Index into the buildables config, or -1 on a site that was never initialised.
	int GetBuildableIndex()
	{
		return m_iBuildableIndex;
	}

	//! \return Index into that buildable's prefab list, or -1.
	int GetPrefabIndex()
	{
		return m_iPrefabIndex;
	}

	//! \return The orientation the finished building will stand at.
	vector GetAngles()
	{
		return m_vAngles;
	}

	//! \return The building's config title. Replicated, so it is right on every machine.
	string GetBuildableName()
	{
		return m_sBuildableName;
	}

	//-----------------------------------------------------------------------------------------------
	// PROTECTED
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \param[in] buildableIndex Index into the buildables config.
	//! \return That buildable's title, or "" when the index no longer names one.
	protected string ResolveBuildableName(int buildableIndex)
	{
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance)
			return "";

		OVT_Buildable buildable = resistance.GetBuildableAt(buildableIndex);
		if (!buildable)
			return "";

		return buildable.m_sTitle;
	}
}
