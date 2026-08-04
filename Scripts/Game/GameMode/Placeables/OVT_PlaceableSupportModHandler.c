class OVT_PlaceableSupportModHandler : OVT_PlaceableHandler
{
	[Attribute()]
	string m_sSupportModifierName;
	
	override bool OnPlace(IEntity entity, int playerId)
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		OVT_TownData town = towns.GetNearestTown(entity.GetOrigin());
		if(!town) return false;

		int townID = towns.GetTownID(town);
		OVT_TownModifierSystem system = towns.GetModifierSystem(OVT_TownSupportModifierSystem);
		if(!system) return false;

		// Refuse outright when the town is already carrying as many of this modifier as it may.
		// TryAddByName cannot be trusted to say no here: for a NON-stackable modifier it treats a
		// second add as a request to refresh the existing timer and reports SUCCESS, which is what
		// let a town collect any number of pirate radios, each one silently re-arming the first.
		// This is the authoritative half - the placement UI runs the same check for the hint.
		if(system.GetModifierSpaceByName(townID, m_sSupportModifierName) == 0) return false;

		return system.TryAddByName(townID, m_sSupportModifierName);
	}
}