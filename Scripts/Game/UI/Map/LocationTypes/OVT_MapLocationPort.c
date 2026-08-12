//! Port location type for the Overthrow map system
//!
//! The panel's two lines are the only two things a port actually does for a player, and both are
//! grounded in code rather than flavour text:
//!  - Bulk import. OVT_PortContext is the buy-in-bulk menu, and OVT_PlayerCommsComponent's import RPC
//!    rejects the purchase unless BOTH the player and the receiving vehicle are within
//!    IMPORT_MAX_PORT_DISTANCE (30m) of a port.
//!  - Cheaper goods nearby. OVT_EconomyManagerComponent.GetSellPriceAtOffset adds
//!    price * distanceToPort * 0.0001, so every shop's prices rise with its distance from the nearest
//!    port. No number is quoted here on purpose - the shop menu owns real prices.
[BaseContainerProps(), OVT_MapLocationTypeTitle()]
class OVT_MapLocationPort : OVT_MapLocationType
{
	//------------------------------------------------------------------------------------------------
	//! Shared info panel: two static explanatory lines.
	//! Ports carry no per-instance state worth showing - the record is a position and a name - so these
	//! lines are identical on every port and on every machine.
	//! \param[in] location The record being described
	//! \param[in] rowsContainer The shared panel's rows container
	override protected void BuildInfoRows(OVT_MapLocationData location, Widget rowsContainer)
	{
		if (!location || !rowsContainer)
			return;

		AddInfoRow(rowsContainer, "", "#OVT-Map_Port_Desc");
		AddInfoRow(rowsContainer, "", "#OVT-Map_Port_Prices");
	}

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