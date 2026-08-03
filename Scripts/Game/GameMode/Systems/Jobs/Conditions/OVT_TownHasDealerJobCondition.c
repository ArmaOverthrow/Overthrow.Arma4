class OVT_TownHasDealerJobCondition : OVT_JobCondition
{			
	override bool ShouldStart(OVT_TownData town, OVT_BaseData base, int playerId)
	{
		if(!town) return false; //base-only jobs have no town context
		if(town.gunDealerPosition != vector.Zero) return true;
		return false;
	}
}