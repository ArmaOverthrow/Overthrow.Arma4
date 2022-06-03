class OVT_NearestRoad
{
	protected static vector checkPos;
	protected static int nearestDistance;
	protected static vector nearest;
	
	static vector Find(vector pos, int range = 2000)
	{
		checkPos = pos;
		nearestDistance = range+1;
		GetGame().GetWorld().QueryEntitiesBySphere(pos, range, CheckRoad, FilterRoadEntities, EQueryEntitiesFlags.ALL);
		
		if(nearestDistance < range)
		{
			return nearest;
		}else{
			return pos;
		}
	}
	
	static bool CheckRoad(IEntity entity)
	{
		float dist = vector.Distance(entity.GetOrigin(), checkPos);
		if(dist < nearestDistance)
		{
			nearestDistance = dist;
			nearest = entity.GetOrigin();
		}
		
		return true;
	}
	
	static bool FilterRoadEntities(IEntity entity)
	{
		if(entity.ClassName() == "RoadEntity") return true;
		return false;
	}
}