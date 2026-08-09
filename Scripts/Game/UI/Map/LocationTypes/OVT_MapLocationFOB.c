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

			// NOTE: no per-location "visibilityZoom" is written. OVT_MapLocationElement reads only the
			// TYPE-level GetVisibilityZoom() (:315, :498), so a per-record key is inert - priority FOBs
			// are NOT always visible today. The element-side fix belongs to map/core and is filed
			// separately (implementation.md N4 / K9); the dead writes are removed here so nobody reads
			// them as working behaviour.

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
	
}