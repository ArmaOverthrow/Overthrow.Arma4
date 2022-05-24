modded class SCR_ChimeraAIAgent : ChimeraAIAgent
{
	override Faction GetFaction(IEntity entity)
	{
		if (!entity)
			return null;
		
		Vehicle vehicle = Vehicle.Cast(entity);
		if (vehicle)
			return vehicle.GetFaction();
		
		OVT_PlayerWantedComponent wanted = OVT_PlayerWantedComponent.Cast(entity.FindComponent(OVT_PlayerWantedComponent));
		if(wanted && wanted.GetWantedLevel() < 2){
			return null;
		}
		
		FactionAffiliationComponent factionAffiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (factionAffiliation)
			return factionAffiliation.GetAffiliatedFaction();
		
		return null;
	}
}