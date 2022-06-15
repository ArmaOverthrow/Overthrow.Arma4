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
	
	override void OnTick(OVT_TownData town)
	{
		DoRandom(town);
	}
	
	override void OnStart(OVT_TownData town)
	{
		DoRandom(town);
	}
	
	protected void DoRandom(OVT_TownData town)
	{
		float chance = m_fChance;
		if(town.population < 50) chance *= m_fLowPopulationFactor;
		if(town.stability < 50) chance *= m_fLowStabilityFactor;
		if(town.support < 50) chance *= m_fLowSupportFactor;
		
		if(s_AIRandomGenerator.RandFloat01() < chance)
		{
			m_Towns.TryAddStabilityModifier(town.id, m_iIndex);
		}
	}
}