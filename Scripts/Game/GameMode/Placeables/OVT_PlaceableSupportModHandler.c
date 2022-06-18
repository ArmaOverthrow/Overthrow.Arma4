class OVT_PlaceableSupportModHandler : OVT_PlaceableHandler
{
	[Attribute()]
	string m_sSupportModifierName;
	
	override void OnPlace(IEntity entity, int playerId)
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		OVT_TownData town = towns.GetNearestTown(entity.GetOrigin());
		OVT_TownModifierSystem system = towns.GetModifierSystem(OVT_TownSupportModifierSystem);
		system.TryAddByName(town.id, m_sSupportModifierName);		
	}
}