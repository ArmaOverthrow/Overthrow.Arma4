class OVT_RandomJobCondition : OVT_JobCondition
{
	[Attribute("0.01")]
	float m_fChance;
	
	[Attribute("1")]
	float m_fLowPopulationFactor;
	
	[Attribute("1")]
	float m_fLowStabilityFactor;
	
	[Attribute("1")]
	float m_fLowSupportFactor;
		
	override bool ShouldStart(OVT_TownData town)
	{
		float chance = m_fChance;
		if(town.population < 50) chance *= m_fLowPopulationFactor;
		if(town.stability < 50) chance *= m_fLowStabilityFactor;
		if(town.SupportPercentage() < 50) chance *= m_fLowSupportFactor;
		
		if(s_AIRandomGenerator.RandFloatXY(0,100) < chance)
		{
			return true;
		}
		return false;
	}
}