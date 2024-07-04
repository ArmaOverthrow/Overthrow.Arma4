class OVT_PlaceableFOBHandler : OVT_PlaceableHandler
{	
	override bool OnPlace(IEntity entity, int playerId)
	{
		OVT_Global.GetResistanceFaction().RegisterFOB(entity, playerId);
		return true;
	}
}