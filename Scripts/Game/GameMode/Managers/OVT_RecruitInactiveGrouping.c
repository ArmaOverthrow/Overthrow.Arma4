//------------------------------------------------------------------------------------------------
//! WHICH of an owner's parked recruits a newly-parked one could share an AI group with - expressed
//! as PURE FUNCTIONS with no world, no manager and no engine access.
//!
//! WHY THIS IS A SEPARATE, STATIC-ONLY CLASS. Everything else about inactive recruits needs a live
//! world: an AI group to spawn, agents to move between groups, a defend waypoint, vanilla's
//! delete-when-empty destroying the group under a held pointer. None of that is reachable from the
//! automated spine (group behaviour and MP are play-test-only), so the one part that IS decidable -
//! the mapping from "these records, this spot" onto "these recruits are close enough to join" - is
//! pulled out here where Tier A (OVT_TEST_LogicSuite) can pin it. NOTHING IN THIS FILE MAY EVER
//! GROW A MANAGER LOOKUP OR AN ENTITY DEREFERENCE, or that coverage silently dies.
//!
//! The division of labour with the manager is exact, and it is the reason this stays pure:
//!   - HERE: which recruit RECORDS are eligible to host, and in what order.
//!   - THERE: resolving each of those ids to a body, asking that body's parent group whether it
//!     carries OVT_InactiveRecruitGroupComponent, and taking the first that does.
//! A candidate list is therefore a list of PLACES TO LOOK, never a decision that a group exists.
//!
//! THE FOUR RULES IT ENCODES:
//!   1. THE RECRUIT BEING PLACED IS NOT ITS OWN HOST. It is excluded by id rather than by trusting
//!      the caller to have removed it from the list, because at the moment this runs the record's
//!      own m_bInactive may already have been written - the caller decides the order of those two
//!      operations, and this function must answer the same way whichever it picks.
//!   2. ONLY INACTIVE RECRUITS HOST. An ACTIVE recruit's body is in its owner's slave group; joining
//!      that would put a parked recruit back under command, which is the exact opposite of what the
//!      player asked for.
//!   3. ONLY ONLINE RECRUITS HOST. A recruit with no body in the world has no agent and therefore no
//!      parent group, so its id could never resolve to anything to join.
//!   4. DISTANCE IS MEASURED FROM THE RECORD'S LAST KNOWN POSITION, not from an entity. That is what
//!      keeps this function pure, and it is honest as long as the caller keeps that field current
//!      for a body it has just parked - which the recruit manager does, in
//!      PlaceRecruitInInactiveGroup(), before it asks this question for the next recruit.
//------------------------------------------------------------------------------------------------
class OVT_RecruitInactiveGrouping
{
	//! How close two parked recruits must be to share one AI group, in metres.
	//!
	//! 50 m matches the fast-travel recruit radius already in the codebase (decision D10), so "the
	//! squad standing around me" means the same distance in both features. A garrison of five spread
	//! over a compound therefore becomes one group rather than five one-man formations, and a recruit
	//! parked at the other end of town gets its own.
	static const float DEFAULT_CLUSTER_RADIUS = 50.0;

	//------------------------------------------------------------------------------------------------
	//! Which of an owner's recruits could be hosting an inactive group near a given spot.
	//!
	//! ORDER IS TABLE ORDER, and it is a real claim: the caller takes the FIRST candidate whose body
	//! turns out to be in a marked group (decision D10, "first suitable host wins"), so a stable
	//! order is what makes a failing play-test reproducible instead of "sometimes they cluster".
	//!
	//! A HOLE IN THE TABLE IS SKIPPED SILENTLY and is not a candidate. Unlike the transfer path there
	//! is no count to keep honest here: this answers "where might a group be", and a record that does
	//! not exist is not a place.
	//!
	//! \param[in] ownedRecruits Every record the owner holds (as returned by GetPlayerRecruits).
	//! \param[in] excludeRecruitId The recruit being placed - never its own host. Pass "" to exclude
	//!            nothing.
	//! \param[in] origin Where the recruit being placed is standing.
	//! \param[in] radius How far away a host may be, in metres.
	//! \return Recruit ids to try as hosts, in table order. Never null; empty is a normal answer and
	//!         means the caller must create a group.
	static array<string> SelectClusterCandidates(notnull array<ref OVT_RecruitData> ownedRecruits, string excludeRecruitId, vector origin, float radius)
	{
		array<string> candidates = new array<string>();

		foreach (OVT_RecruitData recruit : ownedRecruits)
		{
			// A hole in the table is not a candidate, and there is nothing to count it as.
			if (!recruit)
				continue;

			// Rule 1: the recruit being placed is not its own host.
			if (recruit.m_sRecruitId == excludeRecruitId)
				continue;

			// Rule 2: an active recruit is in its owner's slave group, which is not somewhere a
			// parked recruit may be put.
			if (!recruit.m_bInactive)
				continue;

			// Rule 3: no body in the world means no agent and therefore no group to join.
			if (!recruit.m_bIsOnline)
				continue;

			// Rule 4: the record's own idea of where it is standing.
			if (vector.Distance(origin, recruit.m_vLastKnownPosition) > radius)
				continue;

			candidates.Insert(recruit.m_sRecruitId);
		}

		return candidates;
	}
}
