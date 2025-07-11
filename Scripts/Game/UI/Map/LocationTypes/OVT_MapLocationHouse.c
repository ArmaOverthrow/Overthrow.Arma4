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
	
	override void PopulateLocations(array<ref OVT_MapLocationData> locations)
	{
		if (!m_RealEstate)
			return;
		
		// Track processed houses to avoid duplicates
		set<EntityID> processedHouses = new set<EntityID>();
		
		// Iterate through all owners and their owned houses
		foreach (string playerId, ref array<string> ownedPositions : m_RealEstate.m_mOwned)
		{
			foreach (string posString : ownedPositions)
			{
				vector pos = posString.ToVector();
				IEntity houseEntity = m_RealEstate.GetNearestBuilding(pos);
				if (!houseEntity)
					continue;
					
				EntityID houseID = houseEntity.GetID();
				if (processedHouses.Contains(houseID))
					continue;
					
				OVT_RealEstateConfig bdgConfig = m_RealEstate.GetConfig(houseEntity);
				if (!bdgConfig || bdgConfig.m_IsWarehouse)
					continue;
					
				// Create location data for this owned house
				ref OVT_MapLocationData locationData = new OVT_MapLocationData(houseEntity.GetOrigin(), "#OVT-House", ClassName());
				locationData.m_bVisible = true;
				locationData.SetDataFloat("visibilityZoom", m_fVisibilityZoom);
				
				// Store house data
				locationData.SetDataString("houseID", houseID.ToString());
				locationData.SetDataBool("isOwned", true);
				locationData.SetDataBool("isRented", false);
				locationData.SetDataString("owner", playerId);
				
				locations.Insert(locationData);
				processedHouses.Insert(houseID);
			}
		}
		
		// Iterate through all renters and their rented houses
		foreach (string playerId, ref array<string> rentedPositions : m_RealEstate.m_mRented)
		{
			foreach (string posString : rentedPositions)
			{
				vector pos = posString.ToVector();
				IEntity houseEntity = m_RealEstate.GetNearestBuilding(pos);
				if (!houseEntity)
					continue;
					
				EntityID houseID = houseEntity.GetID();
				if (processedHouses.Contains(houseID))
					continue; // Skip if already processed as owned
					
				OVT_RealEstateConfig bdgConfig = m_RealEstate.GetConfig(houseEntity);
				if (!bdgConfig || bdgConfig.m_IsWarehouse)
					continue;
					
				// Create location data for this rented house
				ref OVT_MapLocationData locationData = new OVT_MapLocationData(houseEntity.GetOrigin(), "#OVT-House", ClassName());
				locationData.m_bVisible = true;
				locationData.SetDataFloat("visibilityZoom", m_fVisibilityZoom);
				
				// Store house data
				locationData.SetDataString("houseID", houseID.ToString());
				locationData.SetDataBool("isOwned", false);
				locationData.SetDataBool("isRented", true);
				locationData.SetDataString("renter", playerId);
				
				locations.Insert(locationData);
				processedHouses.Insert(houseID);
			}
		}
	}
	
	override Color GetIconColor(OVT_MapLocationData location)
	{
		bool isOwned = location.GetDataBool("isOwned", false);
		bool isRented = location.GetDataBool("isRented", false);
		
		if (isOwned)
			return m_OwnedHouseColor;
		else if (isRented)
			return m_RentedHouseColor;
		else
			return m_UnownedHouseColor;
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
	
	override bool CanFastTravel(OVT_MapLocationData location, string playerID, out string reason)
	{
		if (!m_bCanFastTravel)
			return false;
		
		// Check if player owns or rents this house
		string ownerID = location.GetDataString("owner", "");
		string renterID = location.GetDataString("renter", "");
		
		if (ownerID != playerID && renterID != playerID)
		{
			reason = "#OVT-CannotFastTravelNotYourHouse";
			return false;
		}
		
		// Use global fast travel checks
		return OVT_FastTravelService.CanGlobalFastTravel(location.m_vPosition, playerID, reason);
	}
}