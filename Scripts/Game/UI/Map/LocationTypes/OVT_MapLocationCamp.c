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
			
			// Only show public camps or camps owned by the current player. An empty owner or an
			// unresolved local id means the identity is UNKNOWN on this machine (replication
			// timing), not "someone else" - show the camp rather than hide it; the server enforces
			// real eligibility against its own records for travel and respawn (BUG-173)
			if (camp.isPrivate && !camp.owner.IsEmpty() && !currentPlayerID.IsEmpty() && camp.owner != currentPlayerID)
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
		
		// Check if it's the player's own camp or if it's public. Either id being empty means the
		// identity is unknown on this machine, not a mismatch - allow, and let the server's own
		// destination resolution refuse a camp this player genuinely may not use (BUG-173)
		string owner = location.GetDataString("owner", "");
		bool isPrivate = location.GetDataBool("isPrivate", false);

		if (isPrivate && !owner.IsEmpty() && !playerID.IsEmpty() && owner != playerID)
		{
			reason = "#OVT-CannotFastTravelPrivateCamp";
			return false;
		}
		
		// Check global fast travel restrictions
		return OVT_FastTravelService.CanGlobalFastTravel(location.m_vPosition, playerID, reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Public camps are shared; a private camp is respawnable only by its owner.
	//!
	//! Reads the same "owner" and "isPrivate" keys CanFastTravel does, minus the global fast-travel
	//! tail. An unresolved player id refuses, which is the predicate's job rather than this method's.
	//! \param[in] location The record being tested
	//! \param[in] playerID Persistent id of the local player
	//! \param[out] reason Localization key explaining a refusal
	//! \return True when this player may respawn at this camp
	override bool CanRespawn(OVT_MapLocationData location, string playerID, out string reason)
	{
		if (!location)
		{
			reason = "#OVT-Respawn_NotEligible";
			return false;
		}

		string owner = location.GetDataString("owner", "");
		bool isPrivate = location.GetDataBool("isPrivate", false);

		if (!OVT_RespawnService.IsCampEligible(isPrivate, owner, playerID))
		{
			reason = "#OVT-Respawn_PrivateCamp";
			return false;
		}

		if (OVT_RespawnService.IsPositionInActiveQRF(location.m_vPosition))
		{
			reason = "#OVT-Respawn_QRF";
			return false;
		}

		return true;
	}
}