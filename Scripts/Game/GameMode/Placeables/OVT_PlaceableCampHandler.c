class OVT_PlaceableCampHandler : OVT_PlaceableHandler
{	
	override void OnPlace(IEntity entity, int playerId)
	{
		OVT_Global.GetResistanceFaction().RegisterCamp(entity, playerId);
	}
}