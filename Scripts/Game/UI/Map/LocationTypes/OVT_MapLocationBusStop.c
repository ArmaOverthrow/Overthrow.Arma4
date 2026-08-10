//! Bus stop location type for the Overthrow map system
//!
//! Reads OVT_MapMarkerManagerComponent's BUS_STOP registry, which is built from a client-safe world
//! scan plus component self-registration. Nothing here is replicated or persisted, so this runs
//! identically on a host, a dedicated-server client and a late joiner.
[BaseContainerProps(), OVT_MapLocationTypeTitle()]
class OVT_MapLocationBusStop : OVT_MapLocationType
{
	//------------------------------------------------------------------------------------------------
	//! Emits one record per registered bus-stop marker.
	//! Returns cleanly when the registry is not up yet - an empty map beats a script error.
	//! \param[out] locations Array to append this type's records to
	override void PopulateLocations(array<ref OVT_MapLocationData> locations)
	{
		OVT_MapMarkerManagerComponent markers = OVT_Global.GetMapMarkers();
		if (!markers)
			return;

		array<OVT_MapMarkerComponent> busStops = markers.GetMarkers(OVT_MapMarkerCategory.BUS_STOP);
		if (!busStops)
			return;

		foreach (OVT_MapMarkerComponent marker : busStops)
		{
			if (!marker)
				continue;

			IEntity stopEntity = marker.GetOwner();
			if (!stopEntity || !stopEntity.GetWorld())
				continue;

			OVT_MapLocationData locationData = new OVT_MapLocationData(stopEntity.GetOrigin(), GetDisplayName(), ClassName());
			locationData.m_EntityID = stopEntity.GetID();

			locations.Insert(locationData);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Shared info panel: one line explaining what a bus stop is for.
	//!
	//! Grounded in OVT_CatchBusAction (the interact action on the sign, which opens the map and arms
	//! nothing) and in OVT_FastTravelService.ComputeFare, where the bus fare is
	//! round(distance_km * m_Difficulty.busTicketPrice) and every recruit travelling with you costs the
	//! same again. That service replaced the legacy OVT_MapContext bus-travel branch, removed in
	//! map/legacy-retirement. The exact fare is deliberately not quoted: it depends on where the player
	//! is standing, which the panel does not know at the moment it is drawn.
	//! \param[in] location The record being described
	//! \param[in] rowsContainer The shared panel's rows container
	override protected void BuildInfoRows(OVT_MapLocationData location, Widget rowsContainer)
	{
		if (!location || !rowsContainer)
			return;

		AddInfoRow(rowsContainer, "", "#OVT-Map_BusStop_Desc");
	}
}
