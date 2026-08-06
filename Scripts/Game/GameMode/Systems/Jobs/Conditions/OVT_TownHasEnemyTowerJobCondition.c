//------------------------------------------------------------------------------------------------
//! Passes while an occupying-faction radio tower is ON THE AIR within broadcast range of the town.
//!
//! This is the same test OVT_TownManagerComponent.CheckUpdate() uses to decide whether a town is
//! under enemy broadcast (the "NearbyRadioTowerNegative" support modifier): a tower counts only if
//! it is the occupier's AND not currently sabotaged. So the job is offered exactly when there is
//! something for the player to switch off, and stops being offered the moment someone does it.
//------------------------------------------------------------------------------------------------
class OVT_TownHasEnemyTowerJobCondition : OVT_JobCondition
{
	[Attribute("0", UIWidgets.EditBox, "Search radius override in meters. 0 = the difficulty's radioTowerRange, which is what actually decides whether a tower affects a town")]
	float m_fRange;

	override bool ShouldStart(OVT_TownData town, OVT_BaseData base, int playerId)
	{
		if(!town) return false; //base-only jobs have no town context

		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		if(!of) return false;

		float range = m_fRange;
		if(range <= 0) range = OVT_Global.GetConfig().m_Difficulty.radioTowerRange;

		foreach(OVT_RadioTowerData tower : of.m_RadioTowers)
		{
			if(!tower.IsOccupyingFaction()) continue;
			if(tower.IsDisabled()) continue; //already off the air - nothing to sabotage
			if(vector.Distance(town.location, tower.location) > range) continue;

			return true;
		}

		return false;
	}
}
