class OVT_PlaceableFOBHandler : OVT_PlaceableHandler
{	
	override void OnPlace(IEntity entity, int playerId)
	{
		OVT_Global.GetResistanceFaction().RegisterFOB(entity, playerId);
	}
}