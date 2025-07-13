//! Port location type for the Overthrow map system
[BaseContainerProps(), OVT_MapLocationTypeTitle()]
class OVT_MapLocationPort : OVT_MapLocationType
{
	override void PopulateLocations(array<ref OVT_MapLocationData> locations)
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
			return;
		
		// Get all ports from economy manager
		array<RplId> allPorts = economy.GetAllPorts();
		if (!allPorts)
			return;
		
		foreach (RplId portId : allPorts)
		{
			// Get the port entity from replication
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(portId));
			if (!rpl)
				continue;
			
			IEntity portEntity = rpl.GetEntity();
			if (!portEntity || !portEntity.GetWorld())
				continue;
			
			// Create location data for this port
			OVT_MapLocationData locationData = new OVT_MapLocationData(portEntity.GetOrigin(), "#OVT-Port", ClassName());
			locationData.m_EntityID = portEntity.GetID();
			locationData.m_RplID = portId;
			
			locations.Insert(locationData);
		}
	}
}