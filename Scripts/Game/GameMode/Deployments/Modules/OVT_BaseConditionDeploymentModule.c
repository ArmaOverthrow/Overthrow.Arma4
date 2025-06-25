[BaseContainerProps(configRoot: true)]
class OVT_BaseConditionDeploymentModule : OVT_BaseDeploymentModule
{
	//------------------------------------------------------------------------------------------------
	// Static condition evaluation for deployment creation
	//------------------------------------------------------------------------------------------------
	bool EvaluateStaticCondition(vector position, int factionIndex, float threatLevel)
	{
		// Override in derived classes to implement specific condition logic
		// This is called when deciding whether to create a deployment
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	// Utility methods for condition evaluation
	//------------------------------------------------------------------------------------------------
	protected bool IsTimeInRange(int startHour, int endHour)
	{
		ChimeraWorld world = GetGame().GetWorld();
		TimeAndWeatherManagerEntity timeManager = world.GetTimeAndWeatherManager();
		if (!timeManager)
			return true;
			
		int currentHour, currentMinute, currentSecond;
		timeManager.GetHoursMinutesSeconds(currentHour, currentMinute, currentSecond);
		
		if (startHour <= endHour)
		{
			// Normal range (e.g., 6 to 18)
			return currentHour >= startHour && currentHour <= endHour;
		}
		else
		{
			// Overnight range (e.g., 20 to 6)
			return currentHour >= startHour || currentHour <= endHour;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool IsWeekday()
	{
		ChimeraWorld world = GetGame().GetWorld();
		TimeAndWeatherManagerEntity timeManager = world.GetTimeAndWeatherManager();
		if (!timeManager)
			return true;
			
		int dayOfWeek = timeManager.GetWeekDay();
		return dayOfWeek >= 1 && dayOfWeek <= 5; // Monday to Friday
	}
	
	//------------------------------------------------------------------------------------------------
	protected float GetDistanceToNearestBase(vector position, int factionIndex)
	{
		OVT_OccupyingFactionManager ofManager = OVT_Global.GetOccupyingFaction();
		if (!ofManager)
			return float.MAX;
			
		// Find nearest base controlled by the same faction
		float nearestDistance = float.MAX;
		
		array<ref OVT_BaseData> bases = ofManager.m_Bases;
		foreach (OVT_BaseData baseData : bases)
		{
			if (baseData.faction == factionIndex)
			{
				float distance = vector.Distance(position, baseData.location);
				if (distance < nearestDistance)
					nearestDistance = distance;
			}
		}
		
		return nearestDistance;
	}
	
	//------------------------------------------------------------------------------------------------
	protected float GetDistanceToNearestTown(vector position)
	{
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		if (!townManager)
			return float.MAX;
			
		OVT_TownData nearestTown = townManager.GetNearestTown(position);
     	if (!nearestTown)
    			return float.MAX;
    
    		return vector.Distance(position, nearestTown.location);
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool IsNearRoad(vector position, float maxDistance = 100)
	{
		// Simple road detection - could be improved with actual road network queries
		// For now, use terrain analysis or predefined road positions
		
		// Trace in multiple directions to find roads
		for (int i = 0; i < 8; i++)
		{
			float angle = (i / 8.0) * Math.PI2;
			vector direction = Vector(Math.Cos(angle), 0, Math.Sin(angle));
			vector testPos = position + direction * maxDistance;
			
			// This is a simplified implementation
			// In a real scenario, you'd query the road network or use terrain data
			TraceParam param = new TraceParam();
			param.Start = testPos + Vector(0, 10, 0);
			param.End = testPos + Vector(0, -10, 0);
			param.Flags = TraceFlags.WORLD;
			
			float result = GetGame().GetWorld().TraceMove(param, null);
			if (result < 1.0)
			{
				// Check surface material or other road indicators
				// This is simplified - in practice you'd check for road materials
				return true;
			}
		}
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool IsPositionSuitable(vector position, string requirements = "")
	{
		// Check if position is suitable based on various criteria
		
		// Basic terrain check
		TraceParam param = new TraceParam();
		param.Start = position + Vector(0, 100, 0);
		param.End = position + Vector(0, -100, 0);
		param.Flags = TraceFlags.WORLD;
		
		float result = GetGame().GetWorld().TraceMove(param, null);
		if (result >= 1.0)
			return false; // No ground found
			
		// Check slope (simplified)
		vector groundPos = vector.Lerp(param.Start, param.End, result);
		float heightDiff = Math.AbsFloat(groundPos[1] - position[1]);
		if (heightDiff > 50) // Too steep or too far from desired height
			return false;
			
		// Additional checks could include:
		// - Water detection
		// - Building proximity
		// - Player base proximity
		// - Restricted area checks
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	protected int GetCurrentHour()
	{
		ChimeraWorld world = GetGame().GetWorld();
		TimeAndWeatherManagerEntity timeManager = world.GetTimeAndWeatherManager();
		if (!timeManager)
			return 12; // Default to noon
			
		int currentHour, currentMinute, currentSecond;
		timeManager.GetHoursMinutesSeconds(currentHour, currentMinute, currentSecond);
		return currentHour;
	}
	
	//------------------------------------------------------------------------------------------------
	protected float GetPlayerProximity(vector position)
	{
		float nearestDistance = float.MAX;
		
		array<int> players = new array<int>;
		GetGame().GetPlayerManager().GetPlayers(players);
		
		foreach (int playerId : players)
		{
			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!player)
				continue;
				
			float distance = vector.Distance(player.GetOrigin(), position);
			if (distance < nearestDistance)
				nearestDistance = distance;
		}
		
		return nearestDistance;
	}
}