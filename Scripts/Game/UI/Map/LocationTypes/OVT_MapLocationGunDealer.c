//! Gun dealer location type for the Overthrow map system
[BaseContainerProps(), OVT_MapLocationTypeTitle()]
class OVT_MapLocationGunDealer : OVT_MapLocationType
{
	override void PopulateLocations(array<ref OVT_MapLocationData> locations)
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
			return;
		
		// Get all gun dealers from economy manager
		array<RplId> gunDealers = economy.GetGunDealers();
		if (!gunDealers)
			return;
		
		foreach (RplId dealerId : gunDealers)
		{
			// Get the gun dealer entity from replication
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(dealerId));
			if (!rpl)
				continue;
			
			IEntity dealerEntity = rpl.GetEntity();
			if (!dealerEntity || !dealerEntity.GetWorld())
				continue;
			
			// Create location data for this gun dealer
			OVT_MapLocationData locationData = new OVT_MapLocationData(dealerEntity.GetOrigin(), "#OVT-GunDealer", ClassName());
			locationData.m_EntityID = dealerEntity.GetID();
			locationData.m_RplID = dealerId;
			
			locations.Insert(locationData);
		}
	}
}