class OVT_PatrolHarassmentStabilityModifier : OVT_StabilityModifier
{
	// One shared handler instance ticks every town, so the hour gate must be keyed per town
	protected ref map<int, int> m_mLastCheckedHour = new map<int, int>;

	override void OnTick(OVT_TownData town)
	{
		if (!town)
			return;

		// Only check once per in-game hour
		ChimeraWorld world = GetGame().GetWorld();
		TimeContainer time = world.GetTimeAndWeatherManager().GetTime();

		int gateTownID = m_Towns.GetTownID(town);
		int lastCheckedHour;
		if (m_mLastCheckedHour.Find(gateTownID, lastCheckedHour) && lastCheckedHour == time.m_iHours)
			return;

		m_mLastCheckedHour.Set(gateTownID, time.m_iHours);
		
		// Check if there's an active "Town Patrol" deployment in this town
		OVT_DeploymentManagerComponent deploymentManager = OVT_DeploymentManagerComponent.GetInstance();
		if (!deploymentManager)
			return;
			
		// Get the actual deployment and check if it's still active
		OVT_DeploymentComponent deploymentComp = deploymentManager.GetDeploymentNearPosition("Town Patrol", town.location, 500);
		
		bool hasActivePatrol = false;
		if (deploymentComp)
		{
			// Check if the patrol is still active (not eliminated)
			hasActivePatrol = !deploymentComp.GetSpawnedUnitsEliminated();
		}
		
		int townID = m_Towns.GetTownID(town);
		
		if (hasActivePatrol)
		{
			// Add harassment modifier
			m_Towns.TryAddStabilityModifier(townID, m_iIndex);
		}
		else
		{
			// Remove harassment modifier if no active patrol present
			m_Towns.RemoveStabilityModifier(townID, m_iIndex);
		}
	}
}