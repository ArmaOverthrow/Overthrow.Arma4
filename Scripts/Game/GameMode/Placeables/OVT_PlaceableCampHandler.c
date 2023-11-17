class OVT_PlaceableCampHandler : OVT_PlaceableHandler
{	
	override bool OnPlace(IEntity entity, int playerId)
	{
		OVT_Global.GetResistanceFaction().RegisterCamp(entity, playerId);
		return true;
	}
}