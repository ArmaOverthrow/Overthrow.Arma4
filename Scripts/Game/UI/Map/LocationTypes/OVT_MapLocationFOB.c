//! FOB location type for the new map system
//! Handles Forward Operating Bases with priority-based visibility and icons
[BaseContainerProps(), OVT_MapLocationTypeTitle()]
class OVT_MapLocationFOB : OVT_MapLocationType
{	
	[Attribute(defvalue: "fob", desc: "Icon name for regular FOBs")]
	protected string m_sRegularFOBIcon;
	
	[Attribute(defvalue: "fob_priority", desc: "Icon name for priority FOBs")]
	protected string m_sPriorityFOBIcon;
	
	//! Populate FOB locations from the resistance faction manager
	override void PopulateLocations(array<ref OVT_MapLocationData> locations)
	{
		if (!m_Resistance)
			return;
		
		// Iterate through all FOBs
		foreach (OVT_FOBData fob : m_Resistance.m_FOBs)
		{
			if (!fob)
				continue;
			
			// Create location data for this FOB
			string fobName = fob.name;
			if (fobName.IsEmpty())
				fobName = "FOB";
				
			OVT_MapLocationData locationData = new OVT_MapLocationData(fob.location, fobName, ClassName());
			
			// Store FOB-specific data
			locationData.SetDataString(OVT_MapDataKeys.OWNER, fob.owner);
			locationData.SetDataBool("isPriority", fob.isPriority);
			locationData.SetDataString(OVT_MapDataKeys.PERSISTENT_ID, fob.persistentId);
			locationData.SetDataInt(OVT_MapDataKeys.GARRISON_COUNT, fob.garrison.Count());

			// A priority FOB is ALWAYS visible: 0 overrides the type's m_fVisibilityZoom for this record
			// only (BUG-138 made OVT_MapLocationElement read the key). An ordinary FOB writes nothing and
			// keeps the type threshold - writing the type value back would be a no-op by construction.
			if (fob.isPriority)
				locationData.SetDataFloat(OVT_MapDataKeys.VISIBILITY_ZOOM, 0);

			locations.Insert(locationData);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Shared info panel: priority flag, and garrison ONLY when it is non-zero.
	//!
	//! GARRISON IS NOT REPLICATED. OVT_ResistanceFactionManager's RplSave/RplLoad carries
	//! persistentId/name/location/owner/isPriority only, and the RpcDo_RegisterFOB broadcast carries no
	//! garrison either, so fob.garrison.Count() is 0 on every machine except the one that owns the
	//! record. A row reading "Garrison: 0" on every client is worse than no row, hence the > 0 test.
	//! The replication gap is filed against resistance/fob and deliberately not fixed here.
	//! \param[in] location The record being described
	//! \param[in] rowsContainer The shared panel's rows container
	override protected void BuildInfoRows(OVT_MapLocationData location, Widget rowsContainer)
	{
		if (!location || !rowsContainer)
			return;

		if (location.GetDataBool("isPriority", false))
			AddInfoRow(rowsContainer, "#OVT-Map_Row_Priority", "#OVT-Map_Row_Yes");
		else
			AddInfoRow(rowsContainer, "#OVT-Map_Row_Priority", "#OVT-Map_Row_No");

		int garrison = location.GetDataInt(OVT_MapDataKeys.GARRISON_COUNT, 0);
		if (garrison > 0)
			AddInfoRow(rowsContainer, "#OVT-Garrison", garrison.ToString());
	}

	//! Get icon name based on FOB priority
	override string GetIconName(OVT_MapLocationData location)
	{
		if (!location)
			return m_sRegularFOBIcon;
		
		bool isPriority = location.GetDataBool("isPriority", false);
		if (isPriority)
			return m_sPriorityFOBIcon;
		
		return m_sRegularFOBIcon;
	}
	
	//! FOBs always allow fast travel
	override bool CanFastTravel(OVT_MapLocationData location, string playerID, out string reason)
	{
		if (!location)
		{
			reason = "#OVT-CannotFastTravelThere";
			return false;
		}
		
		// Check global fast travel restrictions
		return OVT_FastTravelService.CanGlobalFastTravel(location.m_vPosition, playerID, reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Every FOB is respawnable. A FOB only exists because the resistance built it and Overthrow has
	//! no per-FOB access rule to consult. No global fast-travel tail: its rules are measured from a
	//! living player's position.
	//!
	//! NOT QRF-FILTERED BY DEFAULT (unlike every other respawnable type): a deployed FOB is the
	//! resistance's forward spawn for exactly the battle a QRF represents, and players routinely die
	//! defending one they forgot to set as home - dropping it from the respawn screen at that moment
	//! is when it is needed most. The allowFOBDuringQRF difficulty setting (JIP-streamed) lets a
	//! server owner restore the filter. The server-side enumeration
	//! (OVT_RespawnService.CollectEligiblePositions) reads the same setting through the same
	//! predicate; the two must stay in agreement or the marker and the spawn will disagree.
	//! \param[in] location The record being tested
	//! \param[in] playerID Persistent id of the local player
	//! \param[out] reason Localization key explaining a refusal
	//! \return True when this player may respawn at this FOB
	override bool CanRespawn(OVT_MapLocationData location, string playerID, out string reason)
	{
		if (!location)
		{
			reason = "#OVT-Respawn_NotEligible";
			return false;
		}

		if (!OVT_RespawnService.IsFobEligible())
		{
			reason = "#OVT-Respawn_NotEligible";
			return false;
		}

		if (!OVT_RespawnService.AllowFobDuringQrf() && OVT_RespawnService.IsPositionInActiveQRF(location.m_vPosition))
		{
			reason = "#OVT-Respawn_QRF";
			return false;
		}

		return true;
	}
}