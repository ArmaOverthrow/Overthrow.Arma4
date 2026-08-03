class OVT_IsNearestTownWithDealerJobCondition : OVT_JobCondition
{
		
	override bool ShouldStart(OVT_TownData town, OVT_BaseData base, int playerId)
	{
		if(playerId == -1) return false; //only valid for player-allocated jobs
		if(!town) return false; //base-only jobs have no town context
		if(town.gunDealerPosition == vector.Zero) return false;
				
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);		
		if(!player) return false;
		
		vector pos = player.GetOrigin();
		
		OVT_TownData nearestTown;
		float nearest = -1;
		foreach(OVT_TownData t : OVT_Global.GetTowns().m_Towns)
		{
			if(t.gunDealerPosition == vector.Zero) continue;
			float distance = vector.Distance(t.location, pos);
			if(nearest == -1 || distance < nearest){
				nearest = distance;
				nearestTown = t;
			}
		}
		if(nearestTown && nearestTown == town) return true;
		return false;			
		
	}
}