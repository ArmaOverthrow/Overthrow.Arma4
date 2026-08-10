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
	//! Warehouse record index stored on the map record at populate time, so the panel does not have to
	//! re-derive it from a position. -1 means "no warehouse record was matched".
	protected const string DATA_KEY_WAREHOUSE_INDEX = "warehouseIndex";

	//! How many stock lines the contents summary shows. The panel is 260px wide and every name costs a
	//! prefab container load to resolve, so this stays small deliberately.
	protected const int MAX_CONTENT_ROWS = 3;

	//! Radius used to match a map record back to its OVT_WarehouseData. 10m is the same tolerance the
	//! manager's own RpcDo_SetWarehouseOwner uses when it matches a location to a warehouse record.
	protected const int WAREHOUSE_MATCH_RANGE = 10;

	[Attribute(defvalue: "0 1 0 1", UIWidgets.ColorPicker, desc: "Color for owned warehouses (green)", category: "Owned Warehouses")]
	protected ref Color m_OwnedWarehouseColor;

	[Attribute(defvalue: "1 1 0 1", UIWidgets.ColorPicker, desc: "Color for rented warehouses (yellow)", category: "Rented Warehouses")]
	protected ref Color m_RentedWarehouseColor;

	//! Resource name -> localized display name. UIInfo resolution loads and scans a prefab container,
	//! which is far too expensive to repeat per panel open; item names never change. Created lazily
	//! because this class is instantiated from a config container, not by script.
	protected ref map<string, string> m_mItemNames;

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
			locationData.SetDataInt(DATA_KEY_WAREHOUSE_INDEX, FindWarehouseIndex(pos));

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
			locationData.SetDataInt(DATA_KEY_WAREHOUSE_INDEX, FindWarehouseIndex(pos));

			locations.Insert(locationData);
			processedWarehouses.Insert(warehouseID);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Matches a warehouse position back to its record index in the manager's array.
	//! The position handed in is the m_mOwners / m_mRenters key, which is byte-for-byte the vector the
	//! manager stored as OVT_WarehouseData.location, so this is an exact match in practice.
	//! \param[in] pos Registered warehouse position
	//! \return Index into OVT_RealEstateManagerComponent.m_aWarehouses, or -1 when none matched
	protected int FindWarehouseIndex(vector pos)
	{
		if (!m_RealEstate)
			return -1;

		OVT_WarehouseData warehouse = m_RealEstate.GetNearestWarehouse(pos, WAREHOUSE_MATCH_RANGE);
		if (!warehouse)
			return -1;

		return warehouse.id;
	}

	//------------------------------------------------------------------------------------------------
	//! Shared info panel: ownership status, the renter's name when somebody is renting it, and a short
	//! summary of what is stored inside.
	//!
	//! CONTENTS ARE GENUINELY CLIENT-READABLE. OVT_WarehouseData.inventory (resource name -> quantity)
	//! is JIP-replicated through the real-estate manager's RplSave/RplLoad and kept current by the
	//! broadcast RpcDo_SetWarehouseInventory, so this needs no new replication and no RPC.
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
	//! Appends a "Contents" heading plus up to MAX_CONTENT_ROWS of the most numerous stock lines, or a
	//! single "Empty" row when the warehouse is holding nothing.
	//! Items whose display name cannot be resolved are skipped rather than printed as a raw prefab
	//! path, which would be unreadable in a 260px panel.
	//! \param[in] rowsContainer The shared panel's rows container
	//! \param[in] location The record being described
	protected void AddContentsRows(Widget rowsContainer, OVT_MapLocationData location)
	{
		if (!m_RealEstate || !m_RealEstate.m_aWarehouses)
			return;

		int index = location.GetDataInt(DATA_KEY_WAREHOUSE_INDEX, -1);
		if (index < 0 || index >= m_RealEstate.m_aWarehouses.Count())
			return;

		OVT_WarehouseData warehouse = m_RealEstate.m_aWarehouses[index];
		if (!warehouse || !warehouse.inventory)
			return;

		map<string, int> inventory = m_RealEstate.GetWarehouseInventory(warehouse);
		if (!inventory)
			return;

		array<string> resources = new array<string>();
		array<int> quantities = new array<int>();

		for (int i = 0; i < inventory.Count(); i++)
		{
			int qty = inventory.GetElement(i);
			if (qty <= 0)
				continue;

			resources.Insert(inventory.GetKey(i));
			quantities.Insert(qty);
		}

		if (resources.IsEmpty())
		{
			AddInfoRow(rowsContainer, "#OVT-Map_Row_Contents", "#OVT-Map_Row_Empty");
			return;
		}

		AddInfoRow(rowsContainer, "#OVT-Map_Row_Contents", "");

		// Selection sort for the top few only - cheaper and simpler than sorting the whole inventory,
		// and the cap is 3. Attempts are bounded separately so a warehouse full of items with no
		// resolvable UIInfo cannot turn one panel open into dozens of prefab container loads.
		int shown = 0;
		int attempts = 0;
		while (shown < MAX_CONTENT_ROWS && attempts < MAX_CONTENT_ROWS * 2 && !resources.IsEmpty())
		{
			attempts++;

			int best = 0;
			for (int i = 1; i < quantities.Count(); i++)
			{
				if (quantities[i] > quantities[best])
					best = i;
			}

			string res = resources[best];
			int qty = quantities[best];
			resources.Remove(best);
			quantities.Remove(best);

			string itemName = ResolveItemName(res);
			if (itemName.IsEmpty())
				continue;

			AddInfoRow(rowsContainer, itemName, "x" + qty.ToString());
			shown++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Localized display name for a stored item, memoised for the life of this location type.
	//! \param[in] res Prefab resource name used as the inventory key
	//! \return A display name or localization key, or "" when the prefab has no item UIInfo
	protected string ResolveItemName(string res)
	{
		if (!m_mItemNames)
			m_mItemNames = new map<string, string>();

		string cached;
		if (m_mItemNames.Find(res, cached))
			return cached;

		string name = "";

		UIInfo info = OVT_Global.GetItemUIInfo(res);
		if (info)
			name = info.GetName();

		m_mItemNames.Set(res, name);
		return name;
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