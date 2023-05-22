class OVT_IsNearestJobCondition : OVT_JobCondition
{
		
	override bool ShouldStart(OVT_TownData town, OVT_BaseData base, int playerId)
	{		
		if(playerId == -1) return false; //only valid for player-allocated jobs
				
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);		
		if(!player) return false;
		
		vector pos = player.GetOrigin();
		
		if(base)
		{
			OVT_BaseData nearest = OVT_Global.GetOccupyingFaction().GetNearestBase(pos);
			if(nearest.id == base.id) return true;
		}
		if(town)
		{
			OVT_TownData nearest = OVT_Global.GetTowns().GetNearestTown(pos);
			if(nearest == town) return true;
		}
		return false;			
		
	}
}