class OVT_RandomStabilityModifier : OVT_StabilityModifier
{
	[Attribute()]
	float m_fChance;
	
	[Attribute("1")]
	float m_fLowPopulationFactor;
	
	[Attribute("1")]
	float m_fLowStabilityFactor;
	
	[Attribute("1")]
	float m_fLowSupportFactor;
	
	[Attribute("10")]
	float m_fGameStartFactor;
	
	override void OnTick(OVT_TownData town)
	{
		DoRandom(town);
	}
	
	override void OnStart(OVT_TownData town)
	{
		DoRandom(town,m_fGameStartFactor);
	}
	
	protected void DoRandom(OVT_TownData town, float mod = 1)
	{
		float chance = m_fChance * mod;
		if(town.population < 50) chance *= m_fLowPopulationFactor;
		if(town.stability < 50) chance *= m_fLowStabilityFactor;
		if(town.SupportPercentage() < 50) chance *= m_fLowSupportFactor;
		
		if(s_AIRandomGenerator.RandFloatXY(0,100) < chance)
		{
			m_Towns.TryAddStabilityModifier(town.id, m_iIndex);
		}
	}
}