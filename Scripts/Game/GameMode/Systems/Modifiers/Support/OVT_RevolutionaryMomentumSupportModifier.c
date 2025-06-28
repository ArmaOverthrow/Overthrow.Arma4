class OVT_RevolutionaryMomentumSupportModifier : OVT_SupportModifier
{
	protected const float MOMENTUM_RANGE = 2000.0; // Towns within 2km of resistance-controlled towns
	
	override void OnPostInit()
	{
		// Listen for town control changes
		if (m_Towns)
		{
			m_Towns.m_OnTownControlChange.Insert(OnTownControlChange);
		}
	}
	
	override void OnDestroy()
	{
		// Stop listening for town control changes
		if (m_Towns)
		{
			m_Towns.m_OnTownControlChange.Remove(OnTownControlChange);
		}
	}
	
	protected void OnTownControlChange(IEntity townEntity)
	{
		// Update momentum modifiers for all towns when any town control changes
		UpdateAllTownMomentum();
	}
	
	protected void UpdateAllTownMomentum()
	{
		if (!m_Towns)
			return;
			
		int playerFactionIndex = OVT_Global.GetConfig().GetPlayerFactionIndex();
		
		// Check each town for momentum
		foreach (OVT_TownData town : m_Towns.m_Towns)
		{
			if (!town)
				continue;
				
			// Skip if this town is already resistance-controlled
			if (town.faction == playerFactionIndex)
				continue;
				
			// Check if there are any nearby resistance-controlled towns
			bool hasNearbyResistanceTown = false;
			
			foreach (OVT_TownData otherTown : m_Towns.m_Towns)
			{
				if (!otherTown || otherTown == town)
					continue;
					
				// Check if this other town is resistance-controlled
				if (otherTown.faction == playerFactionIndex)
				{
					// Check distance
					float distance = vector.Distance(town.location, otherTown.location);
					if (distance <= MOMENTUM_RANGE)
					{
						hasNearbyResistanceTown = true;
						break;
					}
				}
			}
			
			int townID = m_Towns.GetTownID(town);
			
			if (hasNearbyResistanceTown)
			{
				// Add modifier if not already active
				m_Towns.TryAddSupportModifier(townID, m_iIndex);
			}
			else
			{
				// Remove modifier if no nearby resistance towns
				m_Towns.RemoveSupportModifier(townID, m_iIndex);
			}
		}
	}
}