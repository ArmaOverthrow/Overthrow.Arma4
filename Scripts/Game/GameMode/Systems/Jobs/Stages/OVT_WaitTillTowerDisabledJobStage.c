//------------------------------------------------------------------------------------------------
//! Waits until the radio tower at job.location has been taken off the air.
//!
//! Pair it with OVT_GetRadioTowerLocationJobStage, which puts the tower's own position in
//! job.location first. It does not care WHO sabotaged it or how - only that the tower stopped
//! broadcasting - so any of the usual routes (OVT_TowerSabotageComponent's action, capturing it)
//! completes the job.
//!
//! RESTORE-SAFE WITHOUT A DROP RULE. Waiting stage: OnTick only, no OnStart, and it holds no live
//! session state - it re-finds the tower each tick from a persisted vector rather than caching an
//! RplId the way OVT_WaitTillDeadJobStage does. That is exactly why this one needs no entry in
//! OVT_JobManagerComponent.FindRestorableJobConfig() - see core/persistence.
//------------------------------------------------------------------------------------------------
class OVT_WaitTillTowerDisabledJobStage : OVT_JobStage
{
	//! How far job.location may be from the tower it refers to. Generous: it only has to beat the
	//! spacing between towers, and job.location was set to a tower position in the first place
	[Attribute("100", UIWidgets.EditBox, "How close a tower must be to the job location to count as the target")]
	float m_fMatchRadius;

	override bool OnTick(OVT_Job job)
	{
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		if(!of) return true;

		OVT_RadioTowerData tower = of.GetNearestRadioTower(job.location);

		//! No tower there any more (removed from the world, or the job location was never set
		//! because none was on the air). Advancing rather than hanging follows
		//! OVT_WaitTillDeadJobStage, which also advances when its target cannot be resolved: a job
		//! that can never be finished would otherwise hold its town's slot for the rest of the campaign
		if(!tower) return false;
		if(vector.Distance(tower.location, job.location) > m_fMatchRadius) return false;

		//! Someone else getting there first still counts - the tower is off the air either way
		if(tower.IsDisabled()) return false;

		//! Captured from the occupier: the town is no longer under its broadcast, so the job is done
		if(!tower.IsOccupyingFaction()) return false;

		return true;
	}
}
