//------------------------------------------------------------------------------------------------
//! WHICH of an owner's recruits may follow them into another group, and the slave-group AI budget
//! arithmetic - expressed as PURE FUNCTIONS with no world, no manager and no engine access.
//!
//! WHY THIS IS A SEPARATE, STATIC-ONLY CLASS. Everything else in the recruits-follow-their-owner path
//! needs a live world: entities to move, a slave group to move them into, a replicated membership
//! broadcast. None of that is reachable from the automated spine (JIP/multiplayer is uncovered), so
//! the one part that IS decidable - the mapping from "these records, this owner" onto "these recruits
//! move" - is pulled out here where Tier A (OVT_TEST_LogicSuite) can pin it. Nothing in this file may
//! ever grow a manager lookup or an entity dereference, or that coverage silently dies.
//!
//! THE TWO RULES IT ENCODES:
//!   1. An OFFLINE RECRUIT has no body in the world, so there is no agent to put in anybody's slave
//!      group. It is skipped and counted, never silently dropped.
//!   2. An OFFLINE OWNER transfers NOTHING - not even recruits that still have bodies. A recruit is
//!      commanded by the leader of whatever group its owner is in; leaving a departed player's squad
//!      inside somebody else's group hands that player's AI to a stranger, which is a grief vector
//!      (risk R9) as well as being wrong under "recruits are owned by a player, not by a group".
//------------------------------------------------------------------------------------------------
class OVT_GroupRecruitTransfer
{
	//------------------------------------------------------------------------------------------------
	//! Which of an owner's recruits can physically move to another group's slave group.
	//!
	//! OWNER LIVENESS IS AN ARGUMENT, NOT A LOOKUP - AND IT MUST NOT COME FROM OVT_PlayerData
	//! (decision D9). The player record's own liveness accessor answers from the record's runtime id
	//! field, which is written by the player manager's connect/disconnect bookkeeping; this feature
	//! deliberately does not consume it, because a recruit's commandability is a question about the
	//! ENGINE's session state, not about Overthrow's record of it, and the two have been out of step
	//! before. The caller resolves it once, from
	//! GetGame().GetPlayerManager().GetPlayerController(playerId) != null, and passes the answer in.
	//! That also keeps this function pure and therefore Tier-A testable.
	//!
	//! ORDER IS TABLE ORDER. The ids come back in the order the owner's recruit table holds them, so a
	//! caller iterating the result places recruits in the same order every time - which is what makes a
	//! failing play-test reproducible.
	//!
	//! \param[in] ownedRecruits Every record the owner holds (as returned by GetPlayerRecruits).
	//! \param[in] ownerOnline False when the owner has no live PlayerController.
	//! \param[out] outSkippedOffline How many records were skipped for having no body in the world.
	//!             ZERO when the owner is offline: nothing is examined per-recruit in that case, so
	//!             nothing was skipped "for being offline" - the whole transfer was refused instead.
	//! \return Recruit ids to move, in table order. Never null; empty is a normal answer.
	static array<string> SelectTransferable(notnull array<ref OVT_RecruitData> ownedRecruits, bool ownerOnline, out int outSkippedOffline)
	{
		outSkippedOffline = 0;

		array<string> transferable = new array<string>();

		// An offline owner transfers nothing - see rule 2 in the class header. Deliberately BEFORE the
		// loop, so outSkippedOffline stays a statement about recruits rather than about the owner.
		if (!ownerOnline)
			return transferable;

		foreach (OVT_RecruitData recruit : ownedRecruits)
		{
			// A hole in the table is not an offline recruit; it is not counted as one.
			if (!recruit)
				continue;

			if (!recruit.m_bIsOnline)
			{
				outSkippedOffline++;
				continue;
			}

			transferable.Insert(recruit.m_sRecruitId);
		}

		return transferable;
	}

	//------------------------------------------------------------------------------------------------
	//! Projected slave-group AI count after a transfer.
	//!
	//! A count is never negative, so a caller that hands in a sentinel (an unresolved group, a -1 from
	//! a failed lookup) gets 0 for that term rather than a projection that reads as FEWER AI than are
	//! already there - which would make ExceedsAiBudget() answer false exactly when it matters.
	//!
	//! \param[in] currentSlaveAiCount AI already in the target slave group.
	//! \param[in] incomingCount AI about to be added.
	//! \return The projected total, never negative.
	static int ProjectSlaveAiCount(int currentSlaveAiCount, int incomingCount)
	{
		int current = currentSlaveAiCount;
		if (current < 0)
			current = 0;

		int incoming = incomingCount;
		if (incoming < 0)
			incoming = 0;

		return current + incoming;
	}

	//------------------------------------------------------------------------------------------------
	//! Is a projected slave-group AI count over the configured budget?
	//!
	//! BUDGET <= 0 MEANS "NO BUDGET CONFIGURED" AND ALWAYS RETURNS FALSE. The ~96-AI-in-one-group
	//! scenario (6 players x 16 recruits) is to be MEASURED before it is enforced (goal G11, task
	//! T6.2), so the knob ships disabled and this predicate is the one-line seam that turns it on once
	//! a real number exists. Sitting exactly ON the budget is not exceeding it.
	//!
	//! \param[in] projectedCount Count from ProjectSlaveAiCount().
	//! \param[in] budget Configured maximum, or <= 0 for "disabled".
	//! \return True only when a budget is configured AND the projection is strictly above it.
	static bool ExceedsAiBudget(int projectedCount, int budget)
	{
		if (budget <= 0)
			return false;

		return projectedCount > budget;
	}
}
