//! Point-of-interest location type for the Overthrow map system
//!
//! Reads OVT_MapMarkerManagerComponent's POI registry, which is built from a client-safe world scan
//! plus component self-registration (so a player-built maintenance ramp appears on the next map open
//! without a restart). Nothing here is replicated or persisted.
//!
//! PER-LOCATION ICONS. POIs deliberately do NOT share one imageset: the garages draw from
//! overthrow_mapicons and the vehicle maintenance ramp from icons_wrapperUI-32. m_IconImageset is a
//! type-level attribute, so each record carries its own imageset/quad in string data and
//! OnSetupIconWidget loads it after the base class has run. The conf entry therefore leaves
//! m_IconImageset empty - the base class skips its own load and only applies colour.
//!
//! LEGACY GATING IS PRESERVED IN FULL. m_bMustOwnBase markers appear only when the nearest base
//! exists, is within 220m and is NOT held by the occupying faction. Legacy split those two halves
//! across registration and display; both are evaluated here at populate time, which is cheap (base
//! locations are static and populate runs once per map open) and keeps them off the map's hot path.
[BaseContainerProps(), OVT_MapLocationTypeTitle()]
class OVT_MapLocationPOI : OVT_MapLocationType
{
	//! String data keys carrying this record's own icon, since the imageset is authored per prefab.
	protected const string DATA_KEY_IMAGESET = "iconImageset";
	protected const string DATA_KEY_ICON = "iconName";

	//! The marker's authored description, copied onto the record at populate time so the panel does not
	//! have to go back to the registry to find it.
	protected const string DATA_KEY_DESCRIPTION = "description";

	//! Legacy proximity rule: a "must own base" POI only shows within this range of a base.
	protected const float MUST_OWN_BASE_RANGE = 220;

	[Attribute(defvalue: "0 0 0 1", UIWidgets.ColorPicker, desc: "Icon tint for POI markers. The map's icon atlases are white silhouettes designed to be tinted, and every other Overthrow map type tints dark, so a light tint renders POIs near-invisible against the map background.", category: "Appearance")]
	protected ref Color m_PoiIconColor;

	//------------------------------------------------------------------------------------------------
	//! Emits one record per registered POI marker that passes the base-ownership gate.
	//! Returns cleanly when the registry is not up yet - an empty map beats a script error.
	//! \param[out] locations Array to append this type's records to
	override void PopulateLocations(array<ref OVT_MapLocationData> locations)
	{
		OVT_MapMarkerManagerComponent markers = OVT_Global.GetMapMarkers();
		if (!markers)
			return;

		array<OVT_MapMarkerComponent> pois = markers.GetMarkers(OVT_MapMarkerCategory.POI);
		if (!pois)
			return;

		foreach (OVT_MapMarkerComponent marker : pois)
		{
			if (!marker)
				continue;

			IEntity poiEntity = marker.GetOwner();
			if (!poiEntity || !poiEntity.GetWorld())
				continue;

			SCR_UIInfo info = marker.GetUIInfo();
			if (!info)
				continue;

			vector position = poiEntity.GetOrigin();

			if (marker.MustOwnBase() && !IsNearResistanceHeldBase(position))
				continue;

			string name = info.GetName();
			if (name.IsEmpty())
				name = GetDisplayName();

			OVT_MapLocationData locationData = new OVT_MapLocationData(position, name, ClassName());
			locationData.m_EntityID = poiEntity.GetID();

			ResourceName imageset = info.GetIcon();
			locationData.SetDataString(DATA_KEY_IMAGESET, imageset);
			locationData.SetDataString(DATA_KEY_ICON, info.GetIconSetName());
			locationData.SetDataString(DATA_KEY_DESCRIPTION, info.GetDescription());

			locations.Insert(locationData);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Shared info panel: the marker's own authored description, and nothing else.
	//!
	//! NOTHING IS INVENTED HERE. A POI is whatever a prefab author attached the marker to, so the only
	//! honest sentence the map can print is the one that author wrote. None of the three shipped POI
	//! prefabs (Garage_E_02, GarageMilitary_E_01_base, OVT_VehicleMaintenanceRamp) currently sets
	//! Description on its OVT_MapMarkerComponent m_UiInfo, so today this adds no rows and the panel
	//! renders exactly its header - which UpdateInfoPanel handles by removing the empty container
	//! again. Adding a Description line to those three prefabs is authoring work, not code, and lights
	//! this row up with no further change here.
	//! \param[in] location The record being described
	//! \param[in] rowsContainer The shared panel's rows container
	override protected void BuildInfoRows(OVT_MapLocationData location, Widget rowsContainer)
	{
		if (!location || !rowsContainer)
			return;

		string description = location.GetDataString(DATA_KEY_DESCRIPTION, "");
		if (description.IsEmpty())
			return;

		AddInfoRow(rowsContainer, "", description);
	}

	//------------------------------------------------------------------------------------------------
	//! Legacy POI gating, both halves together.
	//! Registration required a base within 220m (OVT_MainMenuContextOverrideComponent, removed in this
	//! phase); display additionally required that base not to be occupying-faction held.
	//! \param[in] position World position of the marker
	//! \return True when the marker should be shown
	protected bool IsNearResistanceHeldBase(vector position)
	{
		if (!m_OccupyingFaction)
			return false;

		OVT_BaseData base = m_OccupyingFaction.GetNearestBase(position);
		if (!base)
			return false;

		if (vector.Distance(base.location, position) > MUST_OWN_BASE_RANGE)
			return false;

		if (base.IsOccupyingFaction())
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Configurable tint, defaulting dark to match every other type on this map.
	//!
	//! This was briefly hardcoded to white on the reasoning that POI art is "authored" and legacy drew
	//! it untinted. Both halves of that were wrong here: UI/Textures/Map/overthrow_mapicons_atlas.png
	//! is white glyphs on transparent (the garages' "vehicles" quad included), so a white tint paints a
	//! white icon onto a light topographic map; and .conf CAN express a Color - OVT_MapLocationHouse
	//! sets three of them (OverthrowMap.conf:77-79). Making it an attribute means the tint is a one-line
	//! conf change rather than a code edit, which matters because the ramp's icons_wrapperUI-32 art may
	//! warrant a different value from the garages' once seen on screen.
	//! \param[in] location The record being drawn
	//! \return Configured POI tint
	override Color GetIconColor(OVT_MapLocationData location)
	{
		return m_PoiIconColor;
	}

	//------------------------------------------------------------------------------------------------
	//! Loads this record's own imageset/quad, which the base class cannot do because m_IconImageset is
	//! type-level. Runs after the base class, so it overwrites nothing that matters and keeps the
	//! colour the base class applied.
	//! \param[in] iconWidget The icon widget being set up
	//! \param[in] location The record being drawn
	//! \param[in] isSmall True when the map is zoomed out
	override protected void OnSetupIconWidget(Widget iconWidget, OVT_MapLocationData location, bool isSmall)
	{
		if (!iconWidget || !location)
			return;

		ResourceName imageset = location.GetDataString(DATA_KEY_IMAGESET, "");
		string iconName = location.GetDataString(DATA_KEY_ICON, "");
		if (imageset.IsEmpty() || iconName.IsEmpty())
			return;

		ImageWidget image = ImageWidget.Cast(iconWidget.FindAnyWidget("Icon"));
		if (!image)
			return;

		image.LoadImageFromSet(0, imageset, iconName);
		image.SetColor(GetIconColor(location));
	}
}
