//------------------------------------------------------------------------------------------------
//! Opens the vanilla support-station registry for reading, so Overthrow can ask "what fuel sources
//! exist, and where are they?" without keeping a parallel list of its own.
//!
//! WHY THIS ONE METHOD EXISTS. Every range-bearing support station registers itself with this
//! manager on init and removes itself when it is destroyed or repaired
//! (SCR_BaseSupportStationComponent.OnDamageStateChanged), which makes the registry the only list of
//! fuel sources that is correct for free. The accessor that reads it - GetArrayOfSupportStationType
//! - is protected, and a modded class is the cheapest legal way to reach it. That is the whole
//! purpose of this file: it adds no behaviour, hooks nothing, and overrides nothing.
//!
//! WHY IT COPIES. GetArrayOfSupportStationType hands back the manager's OWN array by reference (its
//! out parameter is rebound to the internal handle), so a caller that held onto the result would be
//! holding a live view of the registry: it would see stations appear and vanish mid-iteration, and a
//! caller that "tidied up" its own results would be editing the manager's state. This method walks
//! the internal array and inserts into the caller's, so the returned list is a snapshot that belongs
//! to the caller. Never return or store the internal array itself.
//!
//! Held jerrycans (FuelSupportStation_Gadget.ct authors m_fRange -1) are never registered here,
//! which is correct - they are not a place you can drive to. See OVT_FuelUtils for the fuel-specific
//! queries built on top of this.
//------------------------------------------------------------------------------------------------
modded class SCR_SupportStationManagerComponent
{
	//------------------------------------------------------------------------------------------------
	//! Copies (never aliases) the internal per-type array of registered support stations.
	//!
	//! Null entries in the registry are skipped, so the caller never has to guard the results. An
	//! unknown type simply yields an empty list; unlike the vanilla accessor this does not insert a
	//! new bucket into the map for a type nobody registered.
	//! \param[in] type Support station type to list, e.g. ESupportStationType.FUEL.
	//! \param[out] stations Cleared, then filled with a snapshot of the registered stations.
	//! \return How many stations were written to stations.
	int OVT_GetSupportStationsOfType(ESupportStationType type, notnull out array<SCR_BaseSupportStationComponent> stations)
	{
		stations.Clear();

		if (!m_mSupportStations)
			return 0;

		array<SCR_BaseSupportStationComponent> registered;
		if (!m_mSupportStations.Find(type, registered))
			return 0;

		if (!registered)
			return 0;

		foreach (SCR_BaseSupportStationComponent station : registered)
		{
			if (!station)
				continue;

			stations.Insert(station);
		}

		return stations.Count();
	}
}
