//------------------------------------------------------------------------------------------------
//! Which Overthrow map location type renders a marker.
//! Purely presentational - the category selects the OVT_MapLocationType that draws it, nothing else.
enum OVT_MapMarkerCategory
{
	BUS_STOP,
	POI,
	RESOURCE_PILE
}

//------------------------------------------------------------------------------------------------
class OVT_MapMarkerComponentClass: OVT_ComponentClass
{
};

//------------------------------------------------------------------------------------------------
//! Generic "this entity should appear on the Overthrow map" marker.
//!
//! Attachable to ANY entity via prefab composition. Replaces both the vanilla EMapDescriptorType
//! proximity query that used to find bus stops and the legacy map-icon layer's static POI registry
//! (a RegisterPOI call driven from a per-frame event on the prefab).
//!
//! PRESENTATION ONLY. The component carries a category, a UIInfo and one visibility rule - nothing
//! else. Any other state belongs to the system that owns the entity, not here: this registry must
//! not become a second source of truth for non-map state.
//!
//! DISCOVERY IS TWO-WAY AND BOTH WAYS ARE IDEMPOTENT:
//!  - World-placed statics are found by OVT_MapMarkerManagerComponent's one world scan at init.
//!  - Runtime-spawned placeables (which the scan has already missed) self-register from OnPostInit.
//! Registering twice is harmless, so both mechanisms run unconditionally.
//!
//! NOT REPLICATED. The manager scan runs on every machine over identical static world entities,
//! exactly as OVT_EconomyManagerComponent's port scan does. There is deliberately no RplSave/RplLoad.
class OVT_MapMarkerComponent: OVT_Component
{
	//------------------------------------------------------------------------------------------------
	// ATTRIBUTES
	//------------------------------------------------------------------------------------------------

	[Attribute("0", UIWidgets.ComboBox, "Which map location type draws this marker", "", ParamEnumArray.FromEnum(OVT_MapMarkerCategory))]
	protected OVT_MapMarkerCategory m_eCategory;

	//! Name + imageset + icon used when the marker is drawn. Consumed by the POI location type;
	//! bus stops take their name and icon from the location type's own config instead.
	[Attribute(desc: "Display name and icon for this marker (POI category)")]
	protected ref SCR_UIInfo m_UiInfo;

	//! Preserves the legacy POI gating: only show when a resistance-held base is within 220m.
	//! Evaluated at populate time by the POI location type, never on the map's hot path.
	[Attribute("0", desc: "Only show this marker when a resistance-held base is nearby (POI category)")]
	protected bool m_bMustOwnBase;

	//------------------------------------------------------------------------------------------------
	// LIFECYCLE
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Schedules self-registration one frame out, so runtime-spawned placeables reach the registry.
	//! \param[in] owner The entity this component is attached to
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		GetGame().GetCallqueue().CallLater(Register, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the marker from the registry so a destroyed entity stops drawing.
	//! \param[in] owner The entity this component is attached to
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Register);

		OVT_MapMarkerManagerComponent markers = OVT_MapMarkerManagerComponent.GetInstance();
		if (markers)
			markers.UnregisterMarker(this);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Adds this marker to the registry.
	//! Deliberately silent when the manager is not up yet: a world static that misses this window is
	//! still picked up by the manager's world scan, which runs after the game mode has initialised.
	protected void Register()
	{
		OVT_MapMarkerManagerComponent markers = OVT_MapMarkerManagerComponent.GetInstance();
		if (!markers)
			return;

		markers.RegisterMarker(this);
	}

	//------------------------------------------------------------------------------------------------
	// GETTERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \return The category that decides which location type draws this marker
	OVT_MapMarkerCategory GetCategory()
	{
		return m_eCategory;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The authored display name and icon, or null when none was configured
	SCR_UIInfo GetUIInfo()
	{
		return m_UiInfo;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when this marker should only show near a resistance-held base
	bool MustOwnBase()
	{
		return m_bMustOwnBase;
	}

	//------------------------------------------------------------------------------------------------
	//! World position of the marked entity.
	//! \return The owner's origin, or vector.Zero when the owner has gone away
	vector GetMarkerPosition()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return vector.Zero;

		return owner.GetOrigin();
	}
}
