//! Dropped resource crate piles on the Overthrow map
//!
//! NO REGISTRY IS INVENTED (D11). Piles are found through OVT_MapMarkerComponent, which self-registers
//! runtime-spawned entities from OnPostInit on every machine and unregisters in OnDelete. A pile
//! dropped ten seconds ago is therefore on the next map open with no replication, no JIP half and no
//! manager-held array.
//!
//! CONTENTS COME FROM THE PILE ITSELF. The info rows read the pile entity's own
//! OVT_ResourceStoreComponent, whose contents are replicated in one RplProp, so every machine reads
//! them locally and correctly. Nothing about the pile's stock is copied onto the marker or onto the
//! location record - the marker registry must not become a second source of truth for non-map state
//! (OVT_MapMarkerComponent.c:22-24).
[BaseContainerProps(), OVT_MapLocationTypeTitle()]
class OVT_MapLocationResourcePile : OVT_MapLocationType
{
	//------------------------------------------------------------------------------------------------
	//! Emits one record per registered crate pile marker.
	//! Returns cleanly when the registry is not up yet - an empty map beats a script error.
	//! \param[out] locations Array to append this type's records to
	override void PopulateLocations(array<ref OVT_MapLocationData> locations)
	{
		OVT_MapMarkerManagerComponent markers = OVT_Global.GetMapMarkers();
		if (!markers)
			return;

		array<OVT_MapMarkerComponent> piles = markers.GetMarkers(OVT_MapMarkerCategory.RESOURCE_PILE);
		if (!piles)
			return;

		foreach (OVT_MapMarkerComponent marker : piles)
		{
			if (!marker)
				continue;

			IEntity pile = marker.GetOwner();
			if (!pile || !pile.GetWorld())
				continue;

			OVT_MapLocationData locationData = new OVT_MapLocationData(pile.GetOrigin(), GetDisplayName(), ClassName());
			locationData.m_EntityID = pile.GetID();

			locations.Insert(locationData);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Shared info panel: what this pile is holding, resolved at panel-open time from the pile's own
	//! replicated store. Resolving here rather than at populate time keeps the ledger read off the
	//! map's populate path and means the rows cannot be stale relative to the entity.
	//! \param[in] location The record being described
	//! \param[in] rowsContainer The shared panel's rows container
	override protected void BuildInfoRows(OVT_MapLocationData location, Widget rowsContainer)
	{
		if (!location || !rowsContainer)
			return;

		// The pile may have been consumed or streamed out since the map was opened
		IEntity pile = location.GetEntity();
		if (!pile)
			return;

		OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(pile);
		if (!store)
			return;

		OVT_ResourceLedger ledger = store.GetLedger();
		if (!ledger)
			return;

		array<string> ids = new array<string>();
		array<int> quantities = new array<int>();
		ledger.GetLines(ids, quantities);

		int drawn = 0;
		for (int i = 0; i < ids.Count(); i++)
		{
			if (quantities[i] <= 0)
				continue;

			AddInfoRow(rowsContainer, ResolveName(ids[i]), quantities[i].ToString());
			drawn++;
		}

		if (drawn == 0)
		{
			AddInfoRow(rowsContainer, "", "#OVT-Map_PileEmpty");
			return;
		}

		float cubicMetres = store.GetUsedLitres() / 1000.0;
		AddInfoRow(rowsContainer, "#OVT-Map_PileVolume",
			WidgetManager.Translate("#OVT-Map_PileVolumeValue", cubicMetres.ToString(-1, 1)));
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id A bare resource id
	//! \return The catalogue title, falling back to the id so a row is never blank
	protected string ResolveName(string id)
	{
		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
			return id;

		OVT_ResourcesConfig config = resources.GetResourcesConfig();
		if (!config || !config.m_aResources)
			return id;

		foreach (OVT_Resource res : config.m_aResources)
		{
			if (!res || res.m_sId != id)
				continue;

			if (res.m_sTitle == "")
				return id;

			return res.m_sTitle;
		}

		return id;
	}
}
