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