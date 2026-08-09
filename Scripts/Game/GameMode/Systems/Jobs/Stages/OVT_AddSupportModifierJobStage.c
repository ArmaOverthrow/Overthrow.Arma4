//------------------------------------------------------------------------------------------------
//! Grants a named support modifier to every town the job's location affects. The payoff half of a
//! job whose point is that the neighbourhood notices what you did.
//!
//! For a base job that is every town within range of the base - deliberately the same set the base's
//! own NearbyBaseNegative modifier punishes, so a reward authored against it actually pushes back on
//! the towns that were suffering. For a town job it is just that town.
//!
//! CHECKS SPACE BEFORE ADDING, and does not trust TryAddByName to say no. For a NON-stackable
//! modifier TryAddByName treats a second add as a request to refresh the existing timer and reports
//! success - the bug that once let a town collect any number of pirate radios. Same authoritative
//! check OVT_PlaceableSupportModHandler makes.
//!
//! Side-effecting stage: does its work in OnStart and returns false to advance immediately, which is
//! what keeps the no-replay persistence restore correct. It matters here more than most - this stage
//! hands out a reward, and a stage that could be re-run on load would hand it out again every time
//! the campaign was continued. See core/persistence.
//------------------------------------------------------------------------------------------------
class OVT_AddSupportModifierJobStage : OVT_JobStage
{
	[Attribute("", UIWidgets.EditBox, "Support modifier name, as authored in supportModifiers.conf")]
	string m_sModifierName;

	[Attribute("0", UIWidgets.EditBox, "Range in meters from the job location to the towns it affects. 0 = the difficulty's baseSupportRange")]
	float m_fRange;

	override bool OnStart(OVT_Job job)
	{
		if(m_sModifierName.IsEmpty()) return false;

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if(!towns) return false;

		OVT_TownModifierSystem system = towns.GetModifierSystem(OVT_TownSupportModifierSystem);
		if(!system) return false;

		OVT_TownData jobTown = job.GetTown();
		if(jobTown)
		{
			Grant(towns, system, jobTown);
			return false;
		}

		float range = m_fRange;
		if(range <= 0) range = OVT_Global.GetConfig().m_Difficulty.baseSupportRange;

		int granted = 0;
		foreach(OVT_TownData town : towns.m_Towns)
		{
			if(vector.Distance(town.location, job.location) > range) continue;
			if(Grant(towns, system, town)) granted++;
		}

		if(granted == 0)
			Print("[Overthrow] OVT_AddSupportModifierJobStage: no town in range of the job took '" + m_sModifierName + "'", LogLevel.NORMAL);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds one stack to a town if it has room.
	//! \return True when a stack was actually added.
	protected bool Grant(OVT_TownManagerComponent towns, OVT_TownModifierSystem system, OVT_TownData town)
	{
		int townId = towns.GetTownID(town);

		if(system.GetModifierSpaceByName(townId, m_sModifierName) <= 0) return false;

		return system.TryAddByName(townId, m_sModifierName);
	}
}
