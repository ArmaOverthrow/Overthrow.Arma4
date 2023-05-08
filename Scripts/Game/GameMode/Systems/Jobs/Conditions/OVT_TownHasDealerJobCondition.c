class OVT_TownHasDealerJobCondition : OVT_JobCondition
{			
	override bool ShouldStart(OVT_TownData town, OVT_BaseData base, int playerId)
	{
		if(town.gunDealerPosition && town.gunDealerPosition[0] != 0) return true;
		return false;
	}
}