class OVT_PatrolHarassmentStabilityModifier : OVT_StabilityModifier
{
	protected int m_iLastCheckedHour = -1; // Track when we last checked to avoid spam
	
	override void OnTick(OVT_TownData town)
	{
		if (!town)
			return;
			
		// Only check once per in-game hour
		ChimeraWorld world = GetGame().GetWorld();
		TimeContainer time = world.GetTimeAndWeatherManager().GetTime();
		
		if (m_iLastCheckedHour == time.m_iHours)
			return;
			
		m_iLastCheckedHour = time.m_iHours;
		
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