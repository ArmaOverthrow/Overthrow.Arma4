//! House location type for the Overthrow map system
//!
//! LOCAL PLAYER ONLY. Records are emitted exclusively for houses the machine's own player owns or
//! rents, filtered at POPULATE time. This restores legacy behaviour (the legacy OVT_MapIcons layer,
//! deleted in map/legacy-retirement, called realEstate.GetOwned(persId) then GetRented(persId)) and
//! fixes a live privacy regression on this branch, where every player's property was drawn on every
//! player's map (implementation.md N1).
//! Anything that widens this filter is a privacy regression, not a feature.
//!
//! Warehouses are deliberately the opposite: OVT_MapLocationWarehouse iterates every owner because
//! legacy did too, under a literal "//Public Owned Warehouses" comment (N2). That asymmetry is
//! intentional - do not "fix" it for consistency.
[BaseContainerProps(), OVT_MapLocationTypeTitle()]
class OVT_MapLocationHouse : OVT_MapLocationType
{
	[Attribute(defvalue: "3", desc: "Visibility zoom level for unowned houses (0=always visible, higher=visible at closer zoom)", category: "Unowned Houses")]
	protected float m_fUnownedVisibilityZoom;

	[Attribute(defvalue: "0 0 0 1", UIWidgets.ColorPicker, desc: "Color for owned houses (green)", category: "Owned Houses")]
	protected ref Color m_OwnedHouseColor;

	[Attribute(defvalue: "0 0 0 1", UIWidgets.ColorPicker, desc: "Color for unowned houses (gray)", category: "Unowned Houses")]
	protected ref Color m_UnownedHouseColor;

	[Attribute(defvalue: "0 0 0 1", UIWidgets.ColorPicker, desc: "Color for rented houses (yellow)", category: "Rented Houses")]
	protected ref Color m_RentedHouseColor;

	[Attribute(defvalue: "0 0 0 1", UIWidgets.ColorPicker, desc: "Color for the player's home / respawn point", category: "Owned Houses")]
	protected ref Color m_HomeHouseColor;

	//------------------------------------------------------------------------------------------------
	//! Emits one record per house the LOCAL player owns or rents.
	//! Returns cleanly when real estate has not replicated yet or the local player's persistent ID
	//! cannot be resolved - an empty map beats a script error (Q-3).
	//! \param[out] locations Array to append this type's records to
	override void PopulateLocations(array<ref OVT_MapLocationData> locations)
	{
		if (!m_RealEstate)
			return;

		// No local persistent ID means we cannot tell which houses are ours. Showing nothing is the
		// only safe answer - showing everything is the N1 regression this type used to have.
		string persId = GetCurrentPlayerID();
		if (persId.IsEmpty())
			return;

		// Track processed houses so a house that is both owned and rented is only emitted once
		set<EntityID> processedHouses = new set<EntityID>();

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		set<EntityID> ownedHouses = m_RealEstate.GetOwned(persId);
		if (ownedHouses)
		{
			foreach (EntityID houseID : ownedHouses)
			{
				IEntity houseEntity = world.FindEntityByID(houseID);
				if (!houseEntity)
					continue;

				if (processedHouses.Contains(houseID))
					continue;

				OVT_RealEstateConfig bdgConfig = m_RealEstate.GetConfig(houseEntity);
				if (!bdgConfig || bdgConfig.m_IsWarehouse)
					continue;

				OVT_MapLocationData locationData = new OVT_MapLocationData(houseEntity.GetOrigin(), "#OVT-House", ClassName());
				locationData.m_bVisible = true;
				locationData.m_EntityID = houseID;

				locationData.SetDataString("houseID", houseID.ToString());
				locationData.SetDataBool(OVT_MapDataKeys.IS_OWNED, true);
				locationData.SetDataBool(OVT_MapDataKeys.IS_RENTED, false);
				locationData.SetDataBool(OVT_MapDataKeys.IS_HOME, m_RealEstate.IsHome(persId, houseID));
				locationData.SetDataString(OVT_MapDataKeys.OWNER, persId);

				locations.Insert(locationData);
				processedHouses.Insert(houseID);
			}
		}

		set<EntityID> rentedHouses = m_RealEstate.GetRented(persId);
		if (rentedHouses)
		{
			foreach (EntityID houseID : rentedHouses)
			{
				IEntity houseEntity = world.FindEntityByID(houseID);
				if (!houseEntity)
					continue;

				if (processedHouses.Contains(houseID))
					continue; // Skip if already processed as owned

				OVT_RealEstateConfig bdgConfig = m_RealEstate.GetConfig(houseEntity);
				if (!bdgConfig || bdgConfig.m_IsWarehouse)
					continue;

				OVT_MapLocationData locationData = new OVT_MapLocationData(houseEntity.GetOrigin(), "#OVT-House", ClassName());
				locationData.m_bVisible = true;
				locationData.m_EntityID = houseID;

				locationData.SetDataString("houseID", houseID.ToString());
				locationData.SetDataBool(OVT_MapDataKeys.IS_OWNED, false);
				locationData.SetDataBool(OVT_MapDataKeys.IS_RENTED, true);
				locationData.SetDataBool(OVT_MapDataKeys.IS_HOME, false);
				locationData.SetDataString(OVT_MapDataKeys.RENTER, persId);

				locations.Insert(locationData);
				processedHouses.Insert(houseID);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Home is checked before ownership so it reads distinctly from a second owned house, matching the
	//! legacy OVT_MapIcons layer's special-casing of the home marker (deleted in map/legacy-retirement).
	//! \param[in] location The record being drawn
	//! \return Icon tint for this record
	override Color GetIconColor(OVT_MapLocationData location)
	{
		bool isHome = location.GetDataBool(OVT_MapDataKeys.IS_HOME, false);
		bool isOwned = location.GetDataBool(OVT_MapDataKeys.IS_OWNED, false);
		bool isRented = location.GetDataBool(OVT_MapDataKeys.IS_RENTED, false);

		if (isHome)
			return m_HomeHouseColor;
		else if (isOwned)
			return m_OwnedHouseColor;
		else if (isRented)
			return m_RentedHouseColor;
		else
			return m_UnownedHouseColor;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] location The record being described
	//! \return Localised ownership status
	override string GetLocationDescription(OVT_MapLocationData location)
	{
		bool isHome = location.GetDataBool(OVT_MapDataKeys.IS_HOME, false);
		bool isOwned = location.GetDataBool(OVT_MapDataKeys.IS_OWNED, false);
		bool isRented = location.GetDataBool(OVT_MapDataKeys.IS_RENTED, false);

		if (isHome)
			return "#OVT-IsHome";
		else if (isOwned)
			return "#OVT-Owned";
		else if (isRented)
			return "#OVT-Rented";
		else
			return "#OVT-Unowned";
	}

	//------------------------------------------------------------------------------------------------
	//! Shared info panel: ownership status, plus the rent when this is a rented house.
	//!
	//! NO RENTER ROW. The plan's row table proposed one, on the grounds that the panel shell only
	//! special-cases the "owner" key and so shows nothing for a rented building. That reasoning predates
	//! the N1 privacy fix: this type now emits records ONLY for the local player's own property, so the
	//! renter is always the machine's own player and the row would read "Renter: <your own name>" on
	//! every rented house - noise, immediately above a Status row that already says "Rented".
	//! The renter name is genuinely informative on WAREHOUSES, which are public, and it is rendered
	//! there instead.
	//!
	//! RENT IS A PURE CLIENT-SIDE COMPUTATION over OVT_RealEstateConfig plus the replicated population
	//! and stability of the nearest town, so it is safe to call from a panel. It is guarded on the town
	//! list having arrived, because GetRentPrice dereferences GetNearestTown's result without checking
	//! it and would otherwise throw on a client that opens the map before towns replicate (Q-3).
	//! \param[in] location The record being described
	//! \param[in] rowsContainer The shared panel's rows container
	override protected void BuildInfoRows(OVT_MapLocationData location, Widget rowsContainer)
	{
		if (!location || !rowsContainer)
			return;

		bool isHome = location.GetDataBool(OVT_MapDataKeys.IS_HOME, false);
		bool isOwned = location.GetDataBool(OVT_MapDataKeys.IS_OWNED, false);
		bool isRented = location.GetDataBool(OVT_MapDataKeys.IS_RENTED, false);

		string status;
		if (isHome)
			status = "#OVT-Map_House_Home";
		else if (isOwned)
			status = "#OVT-Owned";
		else if (isRented)
			status = "#OVT-Rented";
		else
			status = "#OVT-Unowned";

		AddInfoRow(rowsContainer, "#OVT-Map_Row_Status", status);

		if (!isRented)
			return;

		AddRentRow(rowsContainer, location);
	}

	//------------------------------------------------------------------------------------------------
	//! Appends the rent row, or nothing at all if any input is missing.
	//! \param[in] rowsContainer The shared panel's rows container
	//! \param[in] location The record being described
	protected void AddRentRow(Widget rowsContainer, OVT_MapLocationData location)
	{
		if (!m_RealEstate || !m_TownManager)
			return;

		// GetRentPrice calls GetNearestTown(...).population with no null guard, so an unpopulated town
		// list is a script error waiting to happen rather than a zero price.
		if (!m_TownManager.m_Towns || m_TownManager.m_Towns.IsEmpty())
			return;

		IEntity house = location.GetEntity();
		if (!house)
			return;

		int rent = m_RealEstate.GetRentPrice(house);
		if (rent <= 0)
			return;

		AddInfoRow(rowsContainer, "#OVT-Rent", OVT_MoneyFormat.FormatMoney(rent));
	}

	//------------------------------------------------------------------------------------------------
	//! HOT PATH - runs per element on every zoom change. Keep it to map lookups and comparisons.
	//! \param[in] location The record being tested
	//! \param[in] playerID Persistent ID of the local player
	//! \param[out] reason Refusal reason when false
	//! \return True when the player may fast travel here
	override bool CanFastTravel(OVT_MapLocationData location, string playerID, out string reason)
	{
		if (!m_bCanFastTravel)
			return false;

		// Check if player owns or rents this house
		string ownerID = location.GetDataString(OVT_MapDataKeys.OWNER, "");
		string renterID = location.GetDataString(OVT_MapDataKeys.RENTER, "");

		if (ownerID != playerID && renterID != playerID)
		{
			reason = "#OVT-CannotFastTravelNotYourHouse";
			return false;
		}

		// Use global fast travel checks
		return OVT_FastTravelService.CanGlobalFastTravel(location.m_vPosition, playerID, reason);
	}
}
