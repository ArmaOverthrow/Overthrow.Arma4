class OVT_PlayerInRangeJobCondition : OVT_JobCondition
{
	[Attribute("1500")]
	float m_iRange;
			
	override bool ShouldStart(OVT_TownData town, OVT_BaseData base)
	{
		if(base)
		{
			return OVT_Global.PlayerInRange(base.location, m_iRange);
		}
		if(town)
		{
			return OVT_Global.PlayerInRange(town.location, m_iRange);
		}
		return false;
	}
}