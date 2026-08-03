class OVT_GrowingDiscontentSupportModifier : OVT_SupportModifier
{
	protected const float LOW_STABILITY_THRESHOLD = 30.0; // Apply when stability below 30%
	// One shared handler instance ticks every town, so the hour gate must be keyed per town
	protected ref map<int, int> m_mLastCheckedHour = new map<int, int>;

	override void OnTick(OVT_TownData town)
	{
		if (!town)
			return;

		// Only check once per in-game hour to allow stacking
		ChimeraWorld world = GetGame().GetWorld();
		TimeContainer time = world.GetTimeAndWeatherManager().GetTime();

		int gateTownID = m_Towns.GetTownID(town);
		int lastCheckedHour;
		if (m_mLastCheckedHour.Find(gateTownID, lastCheckedHour) && lastCheckedHour == time.m_iHours)
			return;

		m_mLastCheckedHour.Set(gateTownID, time.m_iHours);
		
		// Only apply to towns with low stability
		if (town.stability < LOW_STABILITY_THRESHOLD)
		{
			int townID = m_Towns.GetTownID(town);
			
			m_Towns.TryAddSupportModifier(townID, m_iIndex);
		}
		else
		{
			// Remove modifier if stability has improved
			int townID = m_Towns.GetTownID(town);
			m_Towns.RemoveSupportModifier(townID, m_iIndex);
		}
	}
}