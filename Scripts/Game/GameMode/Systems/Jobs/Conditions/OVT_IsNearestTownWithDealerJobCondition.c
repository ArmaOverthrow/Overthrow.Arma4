class OVT_IsNearestTownWithDealerJobCondition : OVT_JobCondition
{
		
	override bool ShouldStart(OVT_TownData town, OVT_BaseData base, int playerId)
	{
		if(playerId == -1) return false; //only valid for player-allocated jobs
		if(!town.gunDealerPosition || town.gunDealerPosition[0] == 0) return false;
				
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);		
		if(!player) return false;
		
		vector pos = player.GetOrigin();
		
		OVT_TownData nearestTown;
		float nearest = 9999999;
		foreach(OVT_TownData t : OVT_Global.GetTowns().m_Towns)
		{
			if(!t.gunDealerPosition || t.gunDealerPosition[0] == 0) continue;
			float distance = vector.Distance(t.location, pos);
			if(distance < nearest){
				nearest = distance;
				nearestTown = t;
			}
		}
		if(nearestTown && nearestTown.id == town.id) return true;
		return false;			
		
	}
}