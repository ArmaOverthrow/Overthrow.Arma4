//------------------------------------------------------------------------------------------------
//! TIER A cases - which of an owner's recruits may follow them into another group, and the
//! slave-group AI budget arithmetic.
//!
//! WHY THESE FIVE CASES AND NOTHING ELSE FROM THIS FEATURE. Player groups are almost entirely a
//! multiplayer, cross-process story: joining, approving, commanding somebody else's AI, leaving,
//! reconnecting. None of that is reachable from the automated spine. The one part that is pure
//! decision-making - "given these records and this owner, which recruits move?" - was deliberately
//! extracted into OVT_GroupRecruitTransfer, a static-only class with no world, no manager and no
//! engine access, so that this tier could pin it. If a future change makes that helper reach for a
//! live component, these cases die with it and the feature loses its only automated coverage.
//!
//! THE TWO RULES BEING PINNED, and what breaks if either slips:
//!   1. An OFFLINE RECRUIT has no body in the world, so there is no agent to place in anybody's slave
//!      group. Handing its id out anyway makes the caller dereference a null entity on every group
//!      change - and it would only ever be seen on a server where somebody had been away long enough
//!      for their squad to despawn.
//!   2. An OFFLINE OWNER transfers NOTHING, even when their recruits still have bodies. A recruit is
//!      commanded by the leader of whatever group its owner is in, so leaving a departed player's
//!      squad inside somebody else's group hands that player's AI to a stranger. That is a grief
//!      vector, and it is invisible in solo play.
//!
//! LIVENESS IS AN ARGUMENT, NOT A LOOKUP. SelectTransferable() takes ownerOnline as a parameter
//! because the caller resolves it from the engine's player manager, never from the Overthrow player
//! record's own liveness accessor (decision D9). That is also what keeps the function pure enough for
//! this tier: a case can state "the owner is gone" without a session existing.
//!
//! PROVEN ABLE TO FAIL 2026-08-06, all five cases, both directions. Three separate deliberate faults
//! were needed, which is itself the evidence that the five cases are not restating one claim. Every
//! run below is `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast, 38 cases):
//!   1. SelectTransferable's recruit filter INVERTED (!m_bIsOnline -> m_bIsOnline, so only bodyless
//!      recruits came back) AND its offline-owner early return disabled -> exit 1, 3 of 38 failing:
//!      MixedOnlineOfflineRecruits ("...transferred 3, expected 2"),
//!      OfflineOwnerTransfersNothing ("Control: with the owner present, 0 of 3 online recruits were
//!      transferable, expected 3") and OnlineRecruitsFollowInTableOrder ("...produced 0 transferable
//!      ids, expected 4"). The other two survived it - an empty roster returns an empty list whichever
//!      way the filter points, and the budget arithmetic is untouched by it - so they were proven
//!      separately:
//!   2. ExceedsAiBudget's disabled test flipped from `budget <= 0` to `budget < 0`, making a budget of
//!      0 enforce -> AiBudgetArithmetic failed on "ExceedsAiBudget(96, 0) returned true".
//!   3. EmptyOwnerTransfersNothing's own assertion changed to Count() + 1 != 0 -> it failed on "An
//!      owner with no recruits transferred 0 recruits, expected 0".
//!   Faults 2 and 3 were run together (exit 1, 2 of 38). Every edit was reverted and the same command
//!   exited 0 with all 38 green. No retries, no maxAttempts anywhere: these are pure functions over
//!   hand-built records and cannot flake.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Hand-built recruit records for the cases below. No manager, no world - see the tier rule in
//! OVT_TEST_LogicSuite.c.
//------------------------------------------------------------------------------------------------
class OVT_TEST_GroupRecruitsFixture
{
	//------------------------------------------------------------------------------------------------
	//! One recruit record with only the two fields SelectTransferable() reads set explicitly.
	//! `new` applies no [Attribute()] defvalues, so nothing here may rely on a declared default.
	//! \param[in] recruitId The id the helper is expected to hand back.
	//! \param[in] isOnline Whether this recruit has a body in the world.
	//! \return The record.
	static OVT_RecruitData MakeRecruit(string recruitId, bool isOnline)
	{
		OVT_RecruitData recruit = new OVT_RecruitData();
		recruit.m_sRecruitId = recruitId;
		recruit.m_sOwnerPersistentId = "OVT_TEST_OWNER";
		recruit.m_sName = recruitId;
		recruit.m_bIsOnline = isOnline;
		return recruit;
	}

	//------------------------------------------------------------------------------------------------
	//! An owner's recruit table, in the order given.
	//! \param[in] recruits Records to put in the table, in table order.
	//! \return The table.
	static array<ref OVT_RecruitData> MakeRoster(notnull array<ref OVT_RecruitData> recruits)
	{
		array<ref OVT_RecruitData> roster = new array<ref OVT_RecruitData>();
		foreach (OVT_RecruitData recruit : recruits)
		{
			roster.Insert(recruit);
		}

		return roster;
	}
}

//------------------------------------------------------------------------------------------------
//! ProjectSlaveAiCount() and ExceedsAiBudget() - the slave-group AI budget arithmetic.
//!
//! The budget knob ships DISABLED (goal G11 / task T6.2 say measure the ~96-AI case before enforcing
//! anything), so the most important claim here is the boring one: budget <= 0 never trips, no matter
//! how large the projection. A "disabled" budget that quietly enforced at 0 would refuse every single
//! recruit transfer in the game, and the symptom would look like the reactor being broken rather than
//! like a comparison being wrong.
//!
//! Sitting exactly ON the budget is not exceeding it - pinned explicitly, because that boundary is
//! the one an off-by-one lands on.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_GroupRecruits_AiBudgetArithmetic : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		// --- ProjectSlaveAiCount is a sum.
		if (OVT_GroupRecruitTransfer.ProjectSlaveAiCount(0, 0) != 0)
		{
			SetResultFailure("ProjectSlaveAiCount(0, 0) returned %1, expected 0",
				OVT_GroupRecruitTransfer.ProjectSlaveAiCount(0, 0).ToString());
			return true;
		}

		if (OVT_GroupRecruitTransfer.ProjectSlaveAiCount(3, 2) != 5)
		{
			SetResultFailure("ProjectSlaveAiCount(3, 2) returned %1, expected 5",
				OVT_GroupRecruitTransfer.ProjectSlaveAiCount(3, 2).ToString());
			return true;
		}

		if (OVT_GroupRecruitTransfer.ProjectSlaveAiCount(16, 0) != 16)
		{
			SetResultFailure("ProjectSlaveAiCount(16, 0) returned %1, expected 16",
				OVT_GroupRecruitTransfer.ProjectSlaveAiCount(16, 0).ToString());
			return true;
		}

		// A count is never negative: a sentinel handed in for either term contributes 0 rather than
		// dragging the projection below what is already in the group.
		if (OVT_GroupRecruitTransfer.ProjectSlaveAiCount(-1, 4) != 4)
		{
			SetResultFailure("ProjectSlaveAiCount(-1, 4) returned %1, expected 4 (a negative current count contributes 0)",
				OVT_GroupRecruitTransfer.ProjectSlaveAiCount(-1, 4).ToString());
			return true;
		}

		if (OVT_GroupRecruitTransfer.ProjectSlaveAiCount(4, -1) != 4)
		{
			SetResultFailure("ProjectSlaveAiCount(4, -1) returned %1, expected 4 (a negative incoming count contributes 0)",
				OVT_GroupRecruitTransfer.ProjectSlaveAiCount(4, -1).ToString());
			return true;
		}

		// --- budget <= 0 means "no budget configured" and never trips.
		if (OVT_GroupRecruitTransfer.ExceedsAiBudget(96, 0))
		{
			SetResultFailure("ExceedsAiBudget(96, 0) returned true - a budget of 0 must mean DISABLED, not 'refuse everything'");
			return true;
		}

		if (OVT_GroupRecruitTransfer.ExceedsAiBudget(96, -1))
		{
			SetResultFailure("ExceedsAiBudget(96, -1) returned true - a negative budget must mean DISABLED");
			return true;
		}

		if (OVT_GroupRecruitTransfer.ExceedsAiBudget(0, 0))
		{
			SetResultFailure("ExceedsAiBudget(0, 0) returned true - a disabled budget never trips");
			return true;
		}

		// --- With a budget configured, the boundary is strictly above.
		if (OVT_GroupRecruitTransfer.ExceedsAiBudget(9, 10))
		{
			SetResultFailure("ExceedsAiBudget(9, 10) returned true - 9 is under the budget");
			return true;
		}

		if (OVT_GroupRecruitTransfer.ExceedsAiBudget(10, 10))
		{
			SetResultFailure("ExceedsAiBudget(10, 10) returned true - sitting exactly ON the budget is not exceeding it");
			return true;
		}

		if (!OVT_GroupRecruitTransfer.ExceedsAiBudget(11, 10))
		{
			SetResultFailure("ExceedsAiBudget(11, 10) returned false - 11 is over the budget");
			return true;
		}

		// The projection and the predicate compose, which is how the caller will actually use them.
		int projected = OVT_GroupRecruitTransfer.ProjectSlaveAiCount(80, 16);
		if (projected != 96)
		{
			SetResultFailure("ProjectSlaveAiCount(80, 16) returned %1, expected 96", projected.ToString());
			return true;
		}

		if (!OVT_GroupRecruitTransfer.ExceedsAiBudget(projected, 64))
		{
			SetResultFailure("A projection of 96 did not exceed a budget of 64");
			return true;
		}

		if (OVT_GroupRecruitTransfer.ExceedsAiBudget(projected, 0))
		{
			SetResultFailure("A projection of 96 tripped a DISABLED budget");
			return true;
		}

		Print("AI budget: projection is a clamped sum, budget <= 0 is disabled, the boundary is strictly above");

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! An owner with no recruits transfers nothing, and nothing was skipped.
//!
//! This is the overwhelmingly common case - most players in most sessions own no recruits at all, and
//! it runs on EVERY membership change on the server. It must be an empty answer and not a null one:
//! the caller iterates the result immediately, so a null return would take down the group-membership
//! handler for every player in the session, not just for this one.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_GroupRecruits_EmptyOwnerTransfersNothing : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		array<ref OVT_RecruitData> roster = new array<ref OVT_RecruitData>();

		// Poison value: the helper is required to WRITE this, not merely leave it alone.
		int skippedOffline = -999;
		array<string> transferable = OVT_GroupRecruitTransfer.SelectTransferable(roster, true, skippedOffline);

		if (!transferable)
		{
			SetResultFailure("SelectTransferable() returned null for an owner with no recruits - it must return an empty list, the caller iterates it directly");
			return true;
		}

		if (transferable.Count() != 0)
		{
			SetResultFailure("An owner with no recruits transferred %1 recruits, expected 0", transferable.Count().ToString());
			return true;
		}

		if (skippedOffline != 0)
		{
			SetResultFailure("An owner with no recruits reported %1 skipped-offline recruits, expected 0", skippedOffline.ToString());
			return true;
		}

		Print("Empty owner: empty transfer list, 0 skipped");

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Only recruits that have a body in the world move, and the skipped count is exact.
//!
//! A recruit whose body has been despawned (its owner was away long enough, or it is mid-respawn) has
//! no agent to put in a slave group. The count is a separate claim from the filtering because it is
//! what the server log reports - "3 followed, 2 have no body" is how a play-test tells "the transfer
//! worked" apart from "half the squad silently vanished".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_GroupRecruits_MixedOnlineOfflineRecruits : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		// Deliberately interleaved, so a filter that returned a contiguous slice would fail.
		array<ref OVT_RecruitData> roster = OVT_TEST_GroupRecruitsFixture.MakeRoster({
			OVT_TEST_GroupRecruitsFixture.MakeRecruit("recruit-online-a", true),
			OVT_TEST_GroupRecruitsFixture.MakeRecruit("recruit-offline-a", false),
			OVT_TEST_GroupRecruitsFixture.MakeRecruit("recruit-online-b", true),
			OVT_TEST_GroupRecruitsFixture.MakeRecruit("recruit-offline-b", false),
			OVT_TEST_GroupRecruitsFixture.MakeRecruit("recruit-offline-c", false)
		});

		int skippedOffline = -999;
		array<string> transferable = OVT_GroupRecruitTransfer.SelectTransferable(roster, true, skippedOffline);

		if (transferable.Count() != 2)
		{
			SetResultFailure("A roster of 2 online and 3 offline recruits transferred %1, expected 2", transferable.Count().ToString());
			return true;
		}

		if (transferable[0] != "recruit-online-a" || transferable[1] != "recruit-online-b")
		{
			SetResultFailure("The transferred ids were %1, %2 - expected recruit-online-a, recruit-online-b",
				transferable[0], transferable[1]);
			return true;
		}

		if (skippedOffline != 3)
		{
			SetResultFailure("A roster with 3 bodyless recruits reported %1 skipped, expected 3", skippedOffline.ToString());
			return true;
		}

		// A roster where NOTHING has a body is still a well-formed empty answer with an exact count.
		array<ref OVT_RecruitData> allOffline = OVT_TEST_GroupRecruitsFixture.MakeRoster({
			OVT_TEST_GroupRecruitsFixture.MakeRecruit("recruit-gone-a", false),
			OVT_TEST_GroupRecruitsFixture.MakeRecruit("recruit-gone-b", false)
		});

		int allOfflineSkipped = -999;
		array<string> nothing = OVT_GroupRecruitTransfer.SelectTransferable(allOffline, true, allOfflineSkipped);

		if (nothing.Count() != 0)
		{
			SetResultFailure("A roster with no bodies transferred %1 recruits, expected 0", nothing.Count().ToString());
			return true;
		}

		if (allOfflineSkipped != 2)
		{
			SetResultFailure("A roster of 2 bodyless recruits reported %1 skipped, expected 2", allOfflineSkipped.ToString());
			return true;
		}

		PrintFormat("Mixed roster: %1 of 5 recruits follow, %2 skipped for having no body", transferable.Count().ToString(), skippedOffline.ToString());

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! An OFFLINE OWNER transfers nothing, even when every one of their recruits still has a body.
//!
//! THIS IS THE GRIEF GUARD. A recruit is commanded by the leader of whatever group its owner is in;
//! if a departed player's squad were left eligible, the group they happened to be visiting would keep
//! command of a disconnected player's AI. It is also the case a naive implementation gets wrong,
//! because the recruits themselves are perfectly online - the disqualification comes entirely from
//! the owner.
//!
//! The second claim is the counter's meaning: skipped-offline counts RECRUITS skipped for having no
//! body, so an offline owner reports 0 rather than "all of them". Nothing was examined per-recruit;
//! the whole transfer was refused.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_GroupRecruits_OfflineOwnerTransfersNothing : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		array<ref OVT_RecruitData> roster = OVT_TEST_GroupRecruitsFixture.MakeRoster({
			OVT_TEST_GroupRecruitsFixture.MakeRecruit("recruit-online-a", true),
			OVT_TEST_GroupRecruitsFixture.MakeRecruit("recruit-online-b", true),
			OVT_TEST_GroupRecruitsFixture.MakeRecruit("recruit-online-c", true)
		});

		// Same roster, owner present: the control. If this half ever returns anything but 3, the case
		// below would be passing for the wrong reason.
		int onlineSkipped = -999;
		array<string> withOwner = OVT_GroupRecruitTransfer.SelectTransferable(roster, true, onlineSkipped);
		if (withOwner.Count() != 3)
		{
			SetResultFailure("Control: with the owner present, %1 of 3 online recruits were transferable, expected 3", withOwner.Count().ToString());
			return true;
		}

		// Owner gone. Every recruit still has a body, and none of them may move.
		int skippedOffline = -999;
		array<string> transferable = OVT_GroupRecruitTransfer.SelectTransferable(roster, false, skippedOffline);

		if (!transferable)
		{
			SetResultFailure("SelectTransferable() returned null for an offline owner - it must return an empty list");
			return true;
		}

		if (transferable.Count() != 0)
		{
			SetResultFailure("An OFFLINE owner transferred %1 recruits, expected 0 - a departed player's AI must not be commandable by anybody", transferable.Count().ToString());
			return true;
		}

		if (skippedOffline != 0)
		{
			SetResultFailure("An offline owner reported %1 skipped-OFFLINE recruits, expected 0 - the counter describes recruits without bodies, and none was examined", skippedOffline.ToString());
			return true;
		}

		// An offline owner whose recruits ALSO have no bodies is the same answer, by the same route.
		array<ref OVT_RecruitData> mixedRoster = OVT_TEST_GroupRecruitsFixture.MakeRoster({
			OVT_TEST_GroupRecruitsFixture.MakeRecruit("recruit-online-a", true),
			OVT_TEST_GroupRecruitsFixture.MakeRecruit("recruit-offline-a", false)
		});

		int mixedSkipped = -999;
		array<string> mixed = OVT_GroupRecruitTransfer.SelectTransferable(mixedRoster, false, mixedSkipped);

		if (mixed.Count() != 0 || mixedSkipped != 0)
		{
			SetResultFailure("An offline owner with a mixed roster transferred %1 recruits and reported %2 skipped, expected 0 and 0",
				mixed.Count().ToString(), mixedSkipped.ToString());
			return true;
		}

		Print("Offline owner: 3 recruits with bodies, 0 transferable, 0 counted as skipped-offline");

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! An online owner whose recruits all have bodies moves EVERY one of them, in table order.
//!
//! Order is a real claim, not decoration: the caller places recruits into the slave group in the
//! order it receives them, so a stable order is what makes a failing two-client play-test
//! reproducible instead of "sometimes the third one does not follow".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_GroupRecruits_OnlineRecruitsFollowInTableOrder : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		// Ids chosen so that alphabetical order is NOT table order - a helper that sorted, or that
		// walked the table backwards, fails here.
		array<ref OVT_RecruitData> roster = OVT_TEST_GroupRecruitsFixture.MakeRoster({
			OVT_TEST_GroupRecruitsFixture.MakeRecruit("zulu", true),
			OVT_TEST_GroupRecruitsFixture.MakeRecruit("alpha", true),
			OVT_TEST_GroupRecruitsFixture.MakeRecruit("mike", true),
			OVT_TEST_GroupRecruitsFixture.MakeRecruit("bravo", true)
		});

		int skippedOffline = -999;
		array<string> transferable = OVT_GroupRecruitTransfer.SelectTransferable(roster, true, skippedOffline);

		if (transferable.Count() != 4)
		{
			SetResultFailure("4 online recruits with an online owner produced %1 transferable ids, expected 4", transferable.Count().ToString());
			return true;
		}

		array<string> expected = {"zulu", "alpha", "mike", "bravo"};
		for (int i = 0; i < expected.Count(); i++)
		{
			if (transferable[i] != expected[i])
			{
				SetResultFailure("Transfer order broke at index %1: got %2, expected %3", i.ToString(), transferable[i], expected[i]);
				return true;
			}
		}

		if (skippedOffline != 0)
		{
			SetResultFailure("A roster where every recruit has a body reported %1 skipped, expected 0", skippedOffline.ToString());
			return true;
		}

		// The returned list is the caller's own snapshot, not a view onto the roster: the caller
		// iterates it while the manager it came from prunes stale entity mappings underneath.
		transferable.Clear();
		if (roster.Count() != 4)
		{
			SetResultFailure("Clearing the returned id list changed the owner's roster to %1 records - SelectTransferable must return a fresh list", roster.Count().ToString());
			return true;
		}

		Print("Online owner, 4 online recruits: all 4 follow, in table order, as a fresh list");

		SetResultSuccess();
		return true;
	}
}
