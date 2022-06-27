class OVT_TownSupportJobCondition : OVT_JobCondition
{
	[Attribute("-1")]
	int m_iMinSupport;
	
	[Attribute("-1")]
	int m_iMaxSupport;
			
	override bool ShouldStart(OVT_TownData town, OVT_BaseControllerComponent base)
	{
		int support = town.SupportPercentage();
		if(m_iMinSupport > -1 && support < m_iMinSupport) return false;
		if(m_iMaxSupport > -1 && support > m_iMaxSupport) return false;
		
		return true;
	}
}