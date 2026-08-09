//! Camp location type for the new map system
//! Handles camps with ownership-based fast travel
[BaseContainerProps(), OVT_MapLocationTypeTitle()]
class OVT_MapLocationCamp : OVT_MapLocationType
{	
	//! Populate camp locations from the resistance faction manager
	override void PopulateLocations(array<ref OVT_MapLocationData> locations)
	{
		if (!m_Resistance)
			return;
		
		// Get current player ID for filtering
		string currentPlayerID = GetCurrentPlayerID();
		
		// Iterate through all camps
		foreach (OVT_CampData camp : m_Resistance.m_Camps)
		{
			if (!camp)
				continue;
			
			// Only show public camps or camps owned by the current player
			if (camp.isPrivate && camp.owner != currentPlayerID)
				continue;
			
			// Create location data for this camp
			string campName = camp.name;
			if (campName.IsEmpty())
				campName = "Camp";
				
			OVT_MapLocationData locationData = new OVT_MapLocationData(camp.location, campName, ClassName());
			
			// Store camp-specific data
			locationData.SetDataString("owner", camp.owner);
			locationData.SetDataBool("isPrivate", camp.isPrivate);
			locationData.SetDataString("persistentId", camp.persistentId);
			locationData.SetDataInt("garrisonCount", camp.garrison.Count());
			
			locations.Insert(locationData);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Shared info panel: who may use this camp, and garrison ONLY when it is non-zero.
	//!
	//! Access is the row that matters here: a public camp is a fast-travel destination for everyone,
	//! a private one only for its owner, and CanFastTravel below enforces exactly that. Garrison is
	//! gated on > 0 because OVT_ResistanceFactionManager does not replicate camp garrisons - the count
	//! reads 0 on every remote client, and a row that always says zero is worse than no row.
	//! \param[in] location The record being described
	//! \param[in] rowsContainer The shared panel's rows container
	override protected void BuildInfoRows(OVT_MapLocationData location, Widget rowsContainer)
	{
		if (!location || !rowsContainer)
			return;

		if (location.GetDataBool("isPrivate", false))
			AddInfoRow(rowsContainer, "#OVT-Map_Row_Access", "#OVT-Map_Row_Private");
		else
			AddInfoRow(rowsContainer, "#OVT-Map_Row_Access", "#OVT-Map_Row_Public");

		int garrison = location.GetDataInt(OVT_MapDataKeys.GARRISON_COUNT, 0);
		if (garrison > 0)
			AddInfoRow(rowsContainer, "#OVT-Garrison", garrison.ToString());
	}

	//! Camps allow fast travel if owned by player or if public
	override bool CanFastTravel(OVT_MapLocationData location, string playerID, out string reason)
	{
		if (!location)
		{
			reason = "#OVT-CannotFastTravelThere";
			return false;
		}
		
		// Check if it's the player's own camp or if it's public
		string owner = location.GetDataString("owner", "");
		bool isPrivate = location.GetDataBool("isPrivate", false);
		
		if (isPrivate && owner != playerID)
		{
			reason = "#OVT-CannotFastTravelPrivateCamp";
			return false;
		}
		
		// Check global fast travel restrictions
		return OVT_FastTravelService.CanGlobalFastTravel(location.m_vPosition, playerID, reason);
	}
}