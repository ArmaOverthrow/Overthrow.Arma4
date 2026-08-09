//------------------------------------------------------------------------------------------------
//! Passes only while the job's base is still held by the occupying faction.
//!
//! OVT_JobManagerComponent.CheckUpdate() offers base-only jobs across EVERY entry in
//! OVT_OccupyingFactionManager.m_Bases without looking at who holds it, so a base the player has
//! captured keeps being offered enemy jobs. Any base job whose fiction depends on the enemy still
//! being there wants this condition.
//------------------------------------------------------------------------------------------------
class OVT_BaseIsOccupiedJobCondition : OVT_JobCondition
{
	override bool ShouldStart(OVT_TownData town, OVT_BaseData base, int playerId)
	{
		if(!base) return false; //town jobs have no base context

		return base.IsOccupyingFaction();
	}
}
