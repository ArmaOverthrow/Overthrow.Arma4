//! Owned-vehicle location type for the Overthrow map system
//!
//! LOCAL PLAYER ONLY. This type emits records exclusively for vehicles owned by the machine's own
//! player, filtered at POPULATE time and never at display time. That is both legacy parity (the legacy
//! OVT_MapIcons layer, deleted in map/legacy-retirement, called vehicles.GetOwned(persId)) and a hard
//! multiplayer-privacy requirement: another player must never learn where your vehicles are parked.
//! Anything that widens this filter is a privacy regression, not a feature.
//!
//! POSITION AND HEADING ARE SAMPLED ONCE, AT MAP OPEN. Both are read here in PopulateLocations and
//! stored on the record; nothing refreshes while the map is up. This is parity, not a shortcut -
//! legacy also inserted vehicle positions into m_Centers once at build time and its Update() only
//! re-projected the stored world position. Live refresh is an explicit non-goal of this feature.
//! Storing the yaw also keeps entity lookups out of OnSetupIconWidget, which the element re-runs on
//! every zoom change.
//!
//! Uses the inherited m_Vehicles manager cache from OVT_MapLocationType rather than resolving
//! OVT_Global per call - this is the only consumer of that cache and closes tech debt T2.
[BaseContainerProps(), OVT_MapLocationTypeTitle()]
class OVT_MapLocationVehicle : OVT_MapLocationType
{
	//! Vehicle heading in degrees, sampled at map open and applied to the icon image.
	protected const string DATA_KEY_YAW = "yaw";

	//! Prefab resource name -> vehicle display name. Resolution loads and scans a prefab container,
	//! which is far too expensive to repeat every time a panel opens; names never change. Created
	//! lazily because this class is instantiated from a config container, not by script.
	protected ref map<string, string> m_mVehicleNames;

	//------------------------------------------------------------------------------------------------
	//! Emits one record per vehicle owned by the LOCAL player.
	//! Returns cleanly when the vehicle manager has not replicated yet or the local player's
	//! persistent ID cannot be resolved - an empty map beats a script error (Q-3).
	//! \param[out] locations Array to append this type's records to
	override void PopulateLocations(array<ref OVT_MapLocationData> locations)
	{
		if (!m_Vehicles)
			return;

		// No local persistent ID means we cannot tell whose vehicles these are. Showing nothing is the
		// only safe answer: showing everything would leak every player's vehicles to every client.
		string persId = GetCurrentPlayerID();
		if (persId.IsEmpty())
			return;

		set<EntityID> ownedVehicles = m_Vehicles.GetOwned(persId);
		if (!ownedVehicles)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		foreach (EntityID id : ownedVehicles)
		{
			IEntity vehicle = world.FindEntityByID(id);
			if (!vehicle)
				continue;

			OVT_MapLocationData locationData = new OVT_MapLocationData(vehicle.GetOrigin(), GetDisplayName(), ClassName());
			locationData.m_EntityID = id;
			locationData.SetDataString(OVT_MapDataKeys.OWNER, persId);
			locationData.SetDataFloat(DATA_KEY_YAW, vehicle.GetYawPitchRoll()[0]);

			locations.Insert(locationData);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Shared info panel: which vehicle this actually is.
	//!
	//! The marker itself is a generic "Vehicle" quad and the panel header shows the type's display
	//! name, so without this row a player with four cars parked around a town cannot tell which marker
	//! is the truck. Resolved at panel-open time rather than at populate time on purpose: prefab
	//! container loading is expensive and doing it per owned vehicle on every map open would make
	//! opening the map slower for exactly the players who own the most vehicles.
	//! \param[in] location The record being described
	//! \param[in] rowsContainer The shared panel's rows container
	override protected void BuildInfoRows(OVT_MapLocationData location, Widget rowsContainer)
	{
		if (!location || !rowsContainer)
			return;

		// The vehicle may have been despawned or driven out of the streaming radius since the map was
		// opened. No entity means no name - a missing row beats a script error.
		IEntity vehicle = location.GetEntity();
		if (!vehicle)
			return;

		EntityPrefabData prefabData = vehicle.GetPrefabData();
		if (!prefabData)
			return;

		ResourceName prefab = prefabData.GetPrefabName();
		if (prefab.IsEmpty())
			return;

		string vehicleName = ResolveVehicleName(prefab);
		if (vehicleName.IsEmpty())
			return;

		AddInfoRow(rowsContainer, "#OVT-Map_Vehicle", vehicleName);
	}

	//------------------------------------------------------------------------------------------------
	//! Display name for a vehicle prefab, memoised for the life of this location type.
	//! Returned raw (usually a "#AR-..." key) because TextWidget.SetText resolves keys at draw time -
	//! translating here would only matter if the value were being sorted or compared.
	//! \param[in] prefab Vehicle prefab resource name
	//! \return A display name or localization key, or "" when the prefab has no vehicle UIInfo
	protected string ResolveVehicleName(ResourceName prefab)
	{
		if (!m_mVehicleNames)
			m_mVehicleNames = new map<string, string>();

		string cached;
		if (m_mVehicleNames.Find(prefab, cached))
			return cached;

		string name = "";

		SCR_EditableVehicleUIInfo info = OVT_PrefabUtils.GetVehicleUIInfo(prefab);
		if (info)
			name = info.GetName();

		m_mVehicleNames.Set(prefab, name);
		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! Rotates the icon to the vehicle's heading, matching the legacy OVT_MapIcons layer (deleted in
	//! map/legacy-retirement), which read ent.GetYawPitchRoll()[0] and passed it straight to
	//! ImageWidget.SetRotation - degrees, same sign. The yaw was sampled in PopulateLocations, so
	//! nothing is resolved from the world here.
	//! \param[in] iconWidget The icon widget being set up
	//! \param[in] location The record being drawn
	//! \param[in] isSmall True when the map is zoomed out
	override protected void OnSetupIconWidget(Widget iconWidget, OVT_MapLocationData location, bool isSmall)
	{
		if (!iconWidget || !location)
			return;

		ImageWidget image = ImageWidget.Cast(iconWidget.FindAnyWidget("Icon"));
		if (!image)
			return;

		image.SetRotation(location.GetDataFloat(DATA_KEY_YAW, 0));
	}
}
