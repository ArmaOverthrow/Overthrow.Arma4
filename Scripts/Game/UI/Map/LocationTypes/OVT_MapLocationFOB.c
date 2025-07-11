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
			locationData.SetDataString("owner", fob.owner);
			locationData.SetDataBool("isPriority", fob.isPriority);
			locationData.SetDataString("persistentId", fob.persistentId);
			locationData.SetDataInt("garrisonCount", fob.garrison.Count());
			
			// Set visibility zoom based on priority
			if (fob.isPriority)
				locationData.SetDataFloat("visibilityZoom", 0); // Always visible
			else
				locationData.SetDataFloat("visibilityZoom", m_fVisibilityZoom); // Use configured zoom
			
			locations.Insert(locationData);
		}
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