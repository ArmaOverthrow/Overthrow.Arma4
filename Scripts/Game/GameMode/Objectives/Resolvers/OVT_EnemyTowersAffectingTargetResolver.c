//------------------------------------------------------------------------------------------------
//! EVERY RADIO TOWER STILL IN RESISTANCE HANDS THAT COVERS THE OBJECTIVE, in the campaign's own list
//! order.
//!
//! The verbatim port of the hard-coded tower-recapture sender's loop, including its skip: a tower the ASKING
//! faction already holds is not a target, and it is skipped rather than ending the walk, so a second
//! contested tower behind a friendly one is still found.
//!
//! 🔴 THE MANY-ANSWER IS LOAD-BEARING AND IS WHY THIS SEAM IS NOT "RETURN ONE". The caller walks these
//! in order and creates at the first that does not already carry a live recapture deployment inside its
//! dedup radius. Answering only the nearest tower would mean an objective covered by two towers sends
//! NOTHING once the first one has a team on it - which is the shipped dedup-then-next-candidate walk,
//! deleted.
//!
//! ⚠ A SABOTAGED TOWER IS NOT A TARGET, and that is correct rather than an oversight.
//! GetRadioTowersAffecting() skips towers that are off the air: a tower broadcasting nothing is not
//! helping the resistance hold the objective, and it becomes a target again on its own when it recovers.
//!
//! ⚠ NOTHING IS CACHED. Towers change hands, go off the air and come back; this is asked again on every
//! cadence interval and after every load, exactly as the seam's contract requires.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_EnemyTowersAffectingTargetResolver : OVT_ObjectiveTargetResolver
{
	//------------------------------------------------------------------------------------------------
	//! \param[in] objective The objective the operation belongs to.
	//! \param[in] factionIndex The faction running the operation. Towers it already holds are skipped.
	//! \param[out] positions Receives every qualifying tower, in the campaign's list order.
	//! \return True when at least one tower qualifies.
	override bool Resolve(notnull OVT_ObjectiveInstance objective, int factionIndex, notnull array<vector> positions)
	{
		positions.Clear();

		if (!objective.IsLive())
			return false;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return false;

		array<OVT_RadioTowerData> towers = occupying.GetRadioTowersAffecting(objective.GetTargetPosition());
		if (!towers)
			return false;

		foreach (OVT_RadioTowerData tower : towers)
		{
			if (!tower)
				continue;

			// Ours already. Skipped, not stopped on - see the class header.
			if (tower.faction == factionIndex)
				continue;

			positions.Insert(tower.location);
		}

		return !positions.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The resolver's name, for the registry's validator and for debug output.
	override string GetResolverName()
	{
		return "EnemyTowersAffecting";
	}
}
