//------------------------------------------------------------------------------------------------
//! Passes while at least one affected town could still take another stack of a named support modifier.
//!
//! This is what makes a repeatable "do a thing, the town likes you for it" job self-limiting. Once
//! the towns are carrying as much of the reward modifier as the config allows, the job stops being
//! offered and only comes back when the oldest stack times out - no cooldown bookkeeping needed,
//! because the modifier's own stackLimit and timeout ARE the cooldown.
//!
//! Pair it with OVT_AddSupportModifierJobStage using the same modifier name and range, or the gate
//! and the payout will disagree about which towns are involved.
//!
//! For a base job the affected towns are every town within range of the BASE (the same shape as the
//! NearbyBaseNegative modifier the reward is meant to push back against). For a town job it is just
//! that town.
//------------------------------------------------------------------------------------------------
class OVT_SupportModifierSpaceJobCondition : OVT_JobCondition
{
	[Attribute("", UIWidgets.EditBox, "Support modifier name, as authored in supportModifiers.conf")]
	string m_sModifierName;

	[Attribute("0", UIWidgets.EditBox, "Range in meters from a base to the towns it affects. 0 = the difficulty's baseSupportRange. Ignored for town jobs")]
	float m_fRange;

	override bool ShouldStart(OVT_TownData town, OVT_BaseData base, int playerId)
	{
		if(m_sModifierName.IsEmpty()) return false;

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if(!towns) return false;

		OVT_TownModifierSystem system = towns.GetModifierSystem(OVT_TownSupportModifierSystem);
		if(!system) return false;

		if(town)
		{
			return HasSpace(towns, system, town);
		}

		if(!base) return false;

		float range = m_fRange;
		if(range <= 0) range = OVT_Global.GetConfig().m_Difficulty.baseSupportRange;

		foreach(OVT_TownData nearby : towns.m_Towns)
		{
			if(vector.Distance(nearby.location, base.location) > range) continue;
			if(HasSpace(towns, system, nearby)) return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether one town could take another stack.
	//! \return True when there is room. A modifier name this system does not know answers -1 ("no
	//! opinion"), which is treated as NO room: the job's payout would be a no-op, so offering it
	//! would be lying to the player.
	protected bool HasSpace(OVT_TownManagerComponent towns, OVT_TownModifierSystem system, OVT_TownData town)
	{
		int townId = towns.GetTownID(town);
		return system.GetModifierSpaceByName(townId, m_sModifierName) > 0;
	}
}
