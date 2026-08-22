[ComponentEditorProps(category: "Overthrow/Components", description: "Marks an entity as a resource production site: what it makes, how fast, and what buying it costs")]
class OVT_ResourceProductionComponentClass : OVT_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! One map-authored production site: a name, a resource, a rate and a purchase price.
//!
//! NOTHING HERE REPLICATES. Every field is authored in a world file that is byte-identical on every
//! machine, so a client reads them locally - the same reasoning that keeps the sibling store's
//! capacity off the wire. Everything mutable about a site (owner, privacy, the fractional carry)
//! lives in OVT_ResourceProductionManagerComponent's record, and the stock lives in the
//! OVT_ResourceStoreComponent beside this one.
//!
//! The manager finds these by a world query at Init, so a site is a world edit and nothing else.
//------------------------------------------------------------------------------------------------
class OVT_ResourceProductionComponent : OVT_Component
{
	//-----------------------------------------------------------------------------------------------
	// ATTRIBUTES
	//-----------------------------------------------------------------------------------------------

	[Attribute("", desc: "Localization key for this site's name, e.g. #OVT-ProdSite_Sawmill")]
	protected string m_sSiteName;

	[Attribute("", desc: "Which resource this site produces. Must match an OVT_Resource id in resources.conf")]
	protected string m_sResourceId;

	[Attribute(defvalue: "2", desc: "Units produced per in-game hour. Below 1 works - the manager carries the fraction")]
	protected float m_fUnitsPerHour;

	[Attribute(defvalue: "8000", desc: "Purchase price before the difficulty's real-estate cost multiplier")]
	protected int m_iBaseCost;

	//-----------------------------------------------------------------------------------------------
	// MEMBER VARIABLES
	//-----------------------------------------------------------------------------------------------

	//! Memoised store beside this component. Immutable for the life of the entity.
	protected OVT_ResourceStoreComponent m_Store;

	protected bool m_bStoreResolved;

	//-----------------------------------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Validates the two authored things that produce no compile error and no runtime error at the
	//! point of use: an empty resource id (the site would drip nothing forever) and a missing
	//! RplComponent (BUG-193 - the store's contents would never reach a client).
	//! \param[in] owner The site entity.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		// ItemPreviewManagerEntity spawns a throwaway worldless instance to render an icon. It is not
		// a site, so it gets neither validation nor a complaint.
		if (!owner || !owner.GetWorld())
			return;

		if (m_sResourceId == "")
			Print(string.Format("[Overthrow] OVT_ResourceProductionComponent on '%1' has no resource id. The site will never produce anything.", OVT_PrefabUtils.GetPrefabName(owner)), LogLevel.ERROR);

		if (!GetRpl())
			Print(string.Format("[Overthrow] OVT_ResourceProductionComponent on '%1' has no RplComponent. The site's stock can never reach a client.", OVT_PrefabUtils.GetPrefabName(owner)), LogLevel.ERROR);
	}

	//-----------------------------------------------------------------------------------------------
	// GETTERS
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \return The authored localization key for this site's name.
	string GetSiteName()
	{
		return m_sSiteName;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The stable resource id this site produces.
	string GetResourceId()
	{
		return m_sResourceId;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Units produced per in-game hour. May be below 1.
	float GetUnitsPerHour()
	{
		return m_fUnitsPerHour;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The pre-multiplier purchase price.
	int GetBaseCost()
	{
		return m_iBaseCost;
	}

	//------------------------------------------------------------------------------------------------
	//! The store the site drips into, memoised.
	//! \return The store beside this component, or null when the prefab carries none.
	OVT_ResourceStoreComponent GetStore()
	{
		if (!m_bStoreResolved)
		{
			m_bStoreResolved = true;
			m_Store = OVT_ResourceUtils.GetStore(GetOwner());
		}

		return m_Store;
	}
}
