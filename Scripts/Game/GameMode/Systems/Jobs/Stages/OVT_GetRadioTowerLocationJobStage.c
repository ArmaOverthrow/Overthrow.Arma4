//------------------------------------------------------------------------------------------------
//! Points the job at the enemy radio tower nearest its town, so the player can find it on the map.
//!
//! Runs at ACCEPT time, not offer time (put it after OVT_WaitTillJobAcceptedJobStage), the same
//! shape as OVT_GetDealerLocationJobStage in findGunDealer.
//!
//! Side-effecting stage: does its work in OnStart and returns false to advance immediately, which
//! is what keeps the no-replay persistence restore correct - see core/persistence. It writes only
//! job.location, a persisted plain vector, so a job restored at the following stage still knows
//! where its tower is without re-running anything.
//------------------------------------------------------------------------------------------------
class OVT_GetRadioTowerLocationJobStage : OVT_JobStage
{
	[Attribute("0", UIWidgets.EditBox, "Search radius override in meters. 0 = the difficulty's radioTowerRange")]
	float m_fRange;

	override bool OnStart(OVT_Job job)
	{
		OVT_TownData town = job.GetTown();
		if(!town) return false;

		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		if(!of) return false;

		float range = m_fRange;
		if(range <= 0) range = OVT_Global.GetConfig().m_Difficulty.radioTowerRange;

		//! Deliberately NOT GetNearestRadioTower(): that returns the nearest tower of any faction in
		//! any state, and this job is only about one the occupier is still broadcasting from
		OVT_RadioTowerData nearest;
		float nearestDistance = -1;

		foreach(OVT_RadioTowerData tower : of.m_RadioTowers)
		{
			if(!tower.IsOccupyingFaction()) continue;
			if(tower.IsDisabled()) continue;

			float distance = vector.Distance(town.location, tower.location);
			if(distance > range) continue;

			if(nearestDistance == -1 || distance < nearestDistance)
			{
				nearestDistance = distance;
				nearest = tower;
			}
		}

		if(!nearest)
		{
			Print("[Overthrow] OVT_GetRadioTowerLocationJobStage: no enemy tower on the air near town " + job.townId.ToString(), LogLevel.WARNING);
			return false;
		}

		job.location = nearest.location;

		return false;
	}
}
