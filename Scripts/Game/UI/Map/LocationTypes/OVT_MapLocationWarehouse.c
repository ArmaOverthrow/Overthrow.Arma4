//! Warehouse location type for the Overthrow map system
//!
//! PUBLIC BY DESIGN. Unlike houses, every player's warehouse is drawn on every player's map. That is
//! not an oversight: the legacy OVT_MapIcons layer (deleted in map/legacy-retirement) did the same
//! under a literal "//Public Owned Warehouses" comment. Do not "fix" the asymmetry with
//! OVT_MapLocationHouse for consistency.
//! It is also why the renter's NAME earns a row here and does not on a house: a rented warehouse can
//! belong to somebody else, and the panel shell only ever renders an "owner" key.
[BaseContainerProps(), OVT_MapLocationTypeTitle()]
class OVT_MapLocationWarehouse : OVT_MapLocationType
{
	[Attribute(defvalue: "0 1 0 1", UIWidgets.ColorPicker, desc: "Color for owned warehouses (green)", category: "Owned Warehouses")]
	protected ref Color m_OwnedWarehouseColor;

	[Attribute(defvalue: "1 1 0 1", UIWidgets.ColorPicker, desc: "Color for rented warehouses (yellow)", category: "Rented Warehouses")]
	protected ref Color m_RentedWarehouseColor;

	override void PopulateLocations(array<ref OVT_MapLocationData> locations)
	{
		if (!m_RealEstate)
			return;
		
		// Track processed warehouses to avoid duplicates
		set<EntityID> processedWarehouses = new set<EntityID>();
		
		// Iterate through all owners and their owned warehouses
		for (int i = 0; i < m_RealEstate.m_mOwners.Count(); i++)
		{
			vector pos = m_RealEstate.m_mOwners.GetKey(i).ToVector();
			string ownerID = m_RealEstate.m_mOwners.GetElement(i);
			
			IEntity warehouseEntity = m_RealEstate.GetNearestBuilding(pos);
			if (!warehouseEntity)
				continue;
				
			EntityID warehouseID = warehouseEntity.GetID();
			if (processedWarehouses.Contains(warehouseID))
				continue;
				
			OVT_RealEstateConfig bdgConfig = m_RealEstate.GetConfig(warehouseEntity);
			if (!bdgConfig || !bdgConfig.m_IsWarehouse)
				continue;
				
			// Create location data for this owned warehouse
			ref OVT_MapLocationData locationData = new OVT_MapLocationData(warehouseEntity.GetOrigin(), "#OVT-Warehouse", ClassName());
			
			// Store warehouse data
			locationData.SetDataString("warehouseID", warehouseID.ToString());
			locationData.SetDataBool("isOwned", true);
			locationData.SetDataBool("isRented", false);
			locationData.SetDataString("owner", ownerID);

			locations.Insert(locationData);
			processedWarehouses.Insert(warehouseID);
		}
		
		// Iterate through all renters and their rented warehouses
		for (int i = 0; i < m_RealEstate.m_mRenters.Count(); i++)
		{
			vector pos = m_RealEstate.m_mRenters.GetKey(i).ToVector();
			string renterID = m_RealEstate.m_mRenters.GetElement(i);
			
			IEntity warehouseEntity = m_RealEstate.GetNearestBuilding(pos);
			if (!warehouseEntity)
				continue;
				
			EntityID warehouseID = warehouseEntity.GetID();
			if (processedWarehouses.Contains(warehouseID))
				continue; // Skip if already processed as owned
				
			OVT_RealEstateConfig bdgConfig = m_RealEstate.GetConfig(warehouseEntity);
			if (!bdgConfig || !bdgConfig.m_IsWarehouse)
				continue;
				
			// Create location data for this rented warehouse
			ref OVT_MapLocationData locationData = new OVT_MapLocationData(warehouseEntity.GetOrigin(), "#OVT-Warehouse", ClassName());
			
			// Store warehouse data
			locationData.SetDataString("warehouseID", warehouseID.ToString());
			locationData.SetDataBool("isOwned", false);
			locationData.SetDataBool("isRented", true);
			locationData.SetDataString("renter", renterID);

			locations.Insert(locationData);
			processedWarehouses.Insert(warehouseID);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Shared info panel: ownership status, the renter's name when somebody is renting it, and one row
	//! naming the storage and how much is in it.
	//!
	//! CONTENTS ARE NOT CLIENT-READABLE, AND THAT IS THE POINT. A holder's items never leave the server
	//! except to the one player who opened it (logistics/storage §3.12). What a client does have is the
	//! holder's replicated COUNT and NAME - two RplProps on its OVT_StorageComponent - so the panel
	//! shows those and nothing else. Listing what is inside would need an RPC per panel open.
	//! \param[in] location The record being described
	//! \param[in] rowsContainer The shared panel's rows container
	override protected void BuildInfoRows(OVT_MapLocationData location, Widget rowsContainer)
	{
		if (!location || !rowsContainer)
			return;

		bool isOwned = location.GetDataBool(OVT_MapDataKeys.IS_OWNED, false);
		bool isRented = location.GetDataBool(OVT_MapDataKeys.IS_RENTED, false);

		string status;
		if (isOwned)
			status = "#OVT-Owned";
		else if (isRented)
			status = "#OVT-Rented";
		else
			status = "#OVT-Unowned";

		AddInfoRow(rowsContainer, "#OVT-Map_Row_Status", status);

		AddRenterRow(rowsContainer, location);
		AddContentsRows(rowsContainer, location);
	}

	//------------------------------------------------------------------------------------------------
	//! Appends the renter's name, or nothing when nobody rents this warehouse or the name has not
	//! replicated yet. The panel shell renders an "Owner" line for the "owner" key only, so without
	//! this a rented warehouse would show no person at all.
	//! \param[in] rowsContainer The shared panel's rows container
	//! \param[in] location The record being described
	protected void AddRenterRow(Widget rowsContainer, OVT_MapLocationData location)
	{
		string renterID = location.GetDataString(OVT_MapDataKeys.RENTER, "");
		if (renterID.IsEmpty())
			return;

		if (!m_Players)
			return;

		string renterName = m_Players.GetPlayerName(renterID);
		if (renterName.IsEmpty())
			return;

		AddInfoRow(rowsContainer, "#OVT-Map_Row_Renter", renterName);
	}

	//------------------------------------------------------------------------------------------------
	//! Appends ONE row: the storage's display name against how many items it holds, or the shared
	//! "Contents / Empty" row when it holds nothing.
	//!
	//! Both values come off the building's own OVT_StorageComponent, whose count and name are ordinary
	//! RplProps - free for a joining client and correct within one replication tick.
	//! \param[in] rowsContainer The shared panel's rows container
	//! \param[in] location The record being described
	protected void AddContentsRows(Widget rowsContainer, OVT_MapLocationData location)
	{
		if (!m_RealEstate || !location)
			return;

		IEntity building = m_RealEstate.GetNearestBuilding(location.m_vPosition, OVT_RealEstateManagerComponent.WAREHOUSE_MATCH_RANGE);
		if (!building)
			return;

		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(building);
		if (!storage)
			return;

		int count = storage.GetTotalCount();
		if (count <= 0)
		{
			AddInfoRow(rowsContainer, "#OVT-Map_Row_Contents", "#OVT-Map_Row_Empty");
			return;
		}

		AddInfoRow(rowsContainer, storage.GetDisplayName(), "x" + count.ToString());
	}

	override Color GetIconColor(OVT_MapLocationData location)
	{
		bool isOwned = location.GetDataBool("isOwned", false);
		bool isRented = location.GetDataBool("isRented", false);
		
		if (isOwned)
			return m_OwnedWarehouseColor;
		else if (isRented)
			return m_RentedWarehouseColor;
		else
			return Color.Black; // Default color for unowned (shouldn't happen)
	}
	
	override string GetLocationDescription(OVT_MapLocationData location)
	{
		bool isOwned = location.GetDataBool("isOwned", false);
		bool isRented = location.GetDataBool("isRented", false);
		
		if (isOwned)
			return "#OVT-Owned";
		else if (isRented)
			return "#OVT-Rented";
		else
			return "#OVT-Unowned";
	}
}