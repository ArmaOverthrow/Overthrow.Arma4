//------------------------------------------------------------------------------------------------
//! THE MODDER SEAM FOR DESTINATION. Answers "where does this operation actually GO?" for
//! OVT_SendDeploymentObjectiveOperation.
//!
//! Shaped on OVT_DeploymentSourceProvider (Scripts/Game/GameMode/Deployments/Modules/OVT_DeploymentSourceProvider.c:28-37),
//! deliberately: that seam answers "where does this force come FROM" one layer down, this one answers
//! "where does this operation GO" one layer up, and two seams in one epic that answer "where" should
//! not be two different shapes. A config authors ONE of these inside its send-deployment module.
//!
//! THE CONTRACT, and every implementation must hold to all five points:
//!   1. AN EMPTY ARRAY IS THE REFUSAL. NEVER a zero vector standing in for "nowhere". "There is
//!      nothing to send this at" is a normal answer - an objective with no resistance-held towers
//!      near it has no recapture target - and it must be distinguishable from "the target is the
//!      world origin". The operation module's whole reason for existing is that it refuses to send a
//!      force at nothing, and it can only refuse if it is told.
//!   2. ZERO, ONE OR MANY, IN PREFERENCE ORDER, AND THE MANY IS LOAD-BEARING. The caller walks the
//!      answers in order, skips any that already carries a live instance of its config within the
//!      module's dedup radius, and creates at the first free one. That dedup-then-next-candidate walk
//!      is the shipped tower-recapture behaviour; ⚠ THIS IS NOT "RETURN ONE AND ITERATE OUTSIDE",
//!      because an objective covered by two towers would then send nothing at all once the first one
//!      had a team on it.
//!   3. ANSWER FROM THE CAMPAIGN, EVERY TIME. This is asked again on every cadence interval and after
//!      every load, and the campaign moves under it: towers change hands, forward bases are torn down,
//!      bases are lost. NOTHING MAY BE CACHED ACROSS CALLS.
//!   4. BE CHEAP ENOUGH TO CALL EVERY INTERVAL. Walking a faction's base list or an objective's tower
//!      list is fine; anything that queries the whole map is not.
//!   5. BE SAFE WITH NO CAMPAIGN BEHIND IT. It is legal to call this on a config template with no
//!      objective running - that is what the initialisation tier does - so every manager lookup is
//!      guarded and an unresolvable one answers false rather than throwing.
//!
//! ⚠ IT RESOLVES A DESTINATION AND NOTHING ELSE. It never creates a deployment, never spends, never
//! writes the objective's bag and never changes a phase. The four shipped resolvers arrive with the
//! send-deployment operation in the phase that needs them; this file ships the seam alone so that the
//! module contract and the config format are settled before anything depends on them.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_ObjectiveTargetResolver
{
	//------------------------------------------------------------------------------------------------
	//! Where an operation at this objective should be sent, in preference order.
	//! \param[in] objective The objective the operation belongs to.
	//! \param[in] factionIndex The faction running the operation.
	//! \param[out] positions Receives the destinations, best first. Cleared by the implementation.
	//! \return True when at least one destination was written. FALSE, with an EMPTY array, when there
	//!         is nowhere to send anything.
	bool Resolve(notnull OVT_ObjectiveInstance objective, int factionIndex, notnull array<vector> positions)
	{
		positions.Clear();

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Human-readable name for warnings, debug output and the registry's validator.
	//! \return The resolver's name.
	string GetResolverName()
	{
		return "none";
	}
}
