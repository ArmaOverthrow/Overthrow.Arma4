//------------------------------------------------------------------------------------------------
//! TIER A cases - which of an owner's parked recruits could be hosting an inactive group nearby.
//!
//! WHY THESE FOUR CASES AND NOTHING ELSE FROM THIS FEATURE. Inactive recruits are almost entirely a
//! live-world story: spawning an AI group, moving agents between groups, a defend waypoint, and
//! vanilla destroying the group under a held pointer the moment it empties. None of that is reachable
//! from the automated spine. The one part that IS pure decision-making - "given these records and
//! this spot, which recruits are worth asking?" - was deliberately extracted into
//! OVT_RecruitInactiveGrouping, a static-only class with no world, no manager and no engine access,
//! so that this tier could pin it. If a future change makes that helper reach for a live component,
//! these cases die with it and the clustering rules lose their only automated coverage.
//!
//! WHAT A CANDIDATE LIST IS, AND IS NOT. It is a list of PLACES TO LOOK, in the order to look. The
//! caller resolves each id to a body, asks that body's parent group whether it carries the marker
//! component, and takes the first that does. So the helper is never claiming a group exists - it is
//! claiming that these records, and only these, could be standing in one.
//!
//! THE FOUR RULES BEING PINNED, and what breaks if any of them slips:
//!   1. THE RECRUIT BEING PLACED IS NOT ITS OWN HOST. Returning it would make the caller resolve the
//!      body it is currently holding, find whatever group it is in - possibly the owner's squad, on
//!      the deactivate path - and "join" it. The recruit would then never leave the squad, and the
//!      player would see a hold-action that reports success and does nothing.
//!   2. ONLY PARKED RECRUITS HOST. An active recruit's body is in its owner's slave group; putting a
//!      recruit being parked into THAT is the exact opposite of what the player asked for, and it is
//!      the failure mode a naive "find my nearest recruit" implementation lands on.
//!   3. ONLY RECRUITS WITH BODIES HOST. No body means no agent and therefore no parent group, so the
//!      id could never resolve to anything to join - it is a wasted lookup at best and, in a caller
//!      that is less careful than the current one, a null dereference.
//!   4. DISTANCE COMES FROM THE RECORD'S LAST KNOWN POSITION. That is what keeps this function pure.
//!      It is honest because the manager refreshes that field for a body at the moment it parks it,
//!      before the next recruit asks this question.
//!
//! ⚠️ THE RADIUS BOUNDARY IS DELIBERATELY NOT ASSERTED EXACTLY. vector.Distance is not correctly
//! rounded - it is off by 1 ULP at 1000 m and at 2000 m - so a case that pinned "exactly 50.0 m is
//! inside" would be pinning a floating-point accident rather than a rule. Every distance below is
//! well clear of the boundary in one direction or the other, and the case says which.
//!
//! PROVEN ABLE TO FAIL (recruit-ux Phase 2) by deliberate fault injection into
//! OVT_RecruitInactiveGrouping.SelectClusterCandidates() plus tools/compile-check.sh - running a
//! suite is the orchestrator's job, not an implementation agent's (.claude/test-policy.md).
//!
//! WHAT THE PROOF CONSISTS OF, EXACTLY, so nobody has to guess later. Each fault below was applied to
//! the shipped helper and the whole tree recompiled; EVERY ONE COMPILED CLEAN (exit 0), which is the
//! load-bearing half - none of these mistakes is a syntax error, so no gate but a case can catch
//! them. The other half is stated per fault: which assertion becomes false BY CONSTRUCTION, named by
//! the message it prints, and ordered so that it is the FIRST assertion the case reaches. The faults
//! were applied in three batches (a-d, e-f, g-h) because each batch touches one contiguous piece of
//! that function; a batch compiling clean proves each statement-level edit in it compiles clean.
//! Every fault was reverted and the tree recompiled clean afterwards.
//!   a. Self-exclusion (`recruit.m_sRecruitId == excludeRecruitId`) DELETED -> ExclusionRules fails on
//!      "The recruit being placed appeared in its own candidate list".
//!   b. Parked filter (`!recruit.m_bInactive`) DELETED -> ExclusionRules fails on "An ACTIVE recruit
//!      was offered as a cluster host".
//!   c. Body filter (`!recruit.m_bIsOnline`) DELETED -> ExclusionRules fails on "A recruit with no
//!      body was offered as a cluster host".
//!   d. Distance comparison inverted (`> radius` -> `< radius`) -> the near and far sets swap, so
//!      RadiusFiltersByLastKnownPosition fails on "3 recruits within the radius produced 2
//!      candidates, expected 3".
//!   e. Hole handling changed from `continue` to inserting an empty id -> EmptyRosterAndNullHoles
//!      fails on "A roster of 3 records and 2 holes produced 3 candidates, expected 1".
//!   f. `if (ownedRecruits.IsEmpty()) return null;` added at the top -> EmptyRosterAndNullHoles fails
//!      on its very first assertion, "SelectClusterCandidates() returned null for an owner with no
//!      recruits".
//!   g. The loop walked backwards (`for (int i = Count() - 1; i >= 0; i--)`) -> TableOrderStability
//!      fails on "Candidate order broke at index 0: got bravo, expected alpha".
//!   h. The result array hoisted to a static and cleared-and-reused between calls -> TableOrderStability
//!      fails on "Two calls shared one list: appending to the first changed the second to 5 entries".
//!   No maxAttempts anywhere: these are pure functions over hand-built records and cannot flake.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Hand-built recruit records for the clustering cases. No manager, no world - see the tier rule in
//! OVT_TEST_LogicSuite.c.
//!
//! Separate from OVT_TEST_GroupRecruitsFixture on purpose: these cases care about a field that one
//! does not read at all (the position), and a shared factory that set every field either suite might
//! ever want would stop reading as "arrange one value, assert one claim".
//------------------------------------------------------------------------------------------------
class OVT_TEST_RecruitClusteringFixture
{
	//! Where every case measures from. A named constant so a reader can see at a glance that the
	//! distances below are distances from HERE.
	static const vector CLUSTER_ORIGIN = "0 0 0";

	//! The radius every case passes in. Deliberately the same number as the production default, but
	//! passed EXPLICITLY - a case must not be able to pass because of a constant it did not state.
	static const float TEST_RADIUS = 50.0;

	//------------------------------------------------------------------------------------------------
	//! One recruit record with every field SelectClusterCandidates() reads set explicitly.
	//! `new` applies no [Attribute()] defvalues, so nothing here may rely on a declared default.
	//! \param[in] recruitId The id the helper is expected to hand back.
	//! \param[in] inactive Whether this recruit is parked out of its owner's squad.
	//! \param[in] isOnline Whether this recruit has a body in the world.
	//! \param[in] lastKnownPosition Where the record says the body is standing.
	//! \return The record.
	static OVT_RecruitData MakeRecruit(string recruitId, bool inactive, bool isOnline, vector lastKnownPosition)
	{
		OVT_RecruitData recruit = new OVT_RecruitData();
		recruit.m_sRecruitId = recruitId;
		recruit.m_sOwnerPersistentId = "OVT_TEST_OWNER";
		recruit.m_sName = recruitId;
		recruit.m_bInactive = inactive;
		recruit.m_bIsOnline = isOnline;
		recruit.m_vLastKnownPosition = lastKnownPosition;
		return recruit;
	}

	//------------------------------------------------------------------------------------------------
	//! A parked, embodied recruit standing `metres` east of the origin - the ordinary candidate.
	//! \param[in] recruitId The id.
	//! \param[in] metres Distance east of CLUSTER_ORIGIN. Pure X keeps the arithmetic visible.
	//! \return The record.
	static OVT_RecruitData MakeParkedAt(string recruitId, float metres)
	{
		return MakeRecruit(recruitId, true, true, Vector(metres, 0, 0));
	}

	//------------------------------------------------------------------------------------------------
	//! An owner's recruit table, in the order given. Holes are the caller's to insert.
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
//! An empty roster, and a roster full of holes, both answer cleanly.
//!
//! THE EMPTY ANSWER IS THE COMMON ONE. The first recruit a player ever parks has no other parked
//! recruit to join, and so does every recruit parked on its own somewhere - so this path runs at
//! least as often as the clustering path does. It must be an EMPTY list and never a null one: the
//! caller iterates the result immediately, and a null would take down the deactivate action rather
//! than fall through to "create a group", which is the correct behaviour it is standing in for.
//!
//! A HOLE IS NOT A CANDIDATE AND IS NOT AN ERROR. The owner's recruit table is assembled from a map
//! lookup per id, so a record that has just been dismissed can leave a null behind for a frame.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RecruitClustering_EmptyRosterAndNullHoles : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<ref OVT_RecruitData> emptyRoster = new array<ref OVT_RecruitData>();

		array<string> fromEmpty = OVT_RecruitInactiveGrouping.SelectClusterCandidates(
			emptyRoster,
			"recruit-being-parked",
			OVT_TEST_RecruitClusteringFixture.CLUSTER_ORIGIN,
			OVT_TEST_RecruitClusteringFixture.TEST_RADIUS);

		if (!fromEmpty)
		{
			SetFailure("SelectClusterCandidates() returned null for an owner with no recruits - it must return an empty list, the caller iterates it directly");
			return true;
		}

		if (fromEmpty.Count() != 0)
		{
			SetFailure("An owner with no recruits produced %1 cluster candidates, expected 0", fromEmpty.Count().ToString());
			return true;
		}

		// Holes interleaved with real records, so a helper that stopped at the first hole - or counted
		// it - fails here. Only ONE of the three real records is an eligible host.
		array<ref OVT_RecruitData> holed = new array<ref OVT_RecruitData>();
		holed.Insert(null);
		holed.Insert(OVT_TEST_RecruitClusteringFixture.MakeParkedAt("recruit-parked-near", 12));
		holed.Insert(null);
		holed.Insert(OVT_TEST_RecruitClusteringFixture.MakeRecruit("recruit-active-near", false, true, "8 0 0"));
		holed.Insert(OVT_TEST_RecruitClusteringFixture.MakeRecruit("recruit-parked-bodyless", true, false, "9 0 0"));

		array<string> fromHoled = OVT_RecruitInactiveGrouping.SelectClusterCandidates(
			holed,
			"recruit-being-parked",
			OVT_TEST_RecruitClusteringFixture.CLUSTER_ORIGIN,
			OVT_TEST_RecruitClusteringFixture.TEST_RADIUS);

		if (fromHoled.Count() != 1)
		{
			SetFailure("A roster of 3 records and 2 holes produced %1 candidates, expected 1", fromHoled.Count().ToString());
			return true;
		}

		if (fromHoled[0] != "recruit-parked-near")
		{
			SetFailure("The candidate from a holed roster was '%1', expected recruit-parked-near", fromHoled[0]);
			return true;
		}

		// A roster that is NOTHING but holes is the empty answer again, by a different route.
		array<ref OVT_RecruitData> allHoles = new array<ref OVT_RecruitData>();
		allHoles.Insert(null);
		allHoles.Insert(null);

		array<string> fromAllHoles = OVT_RecruitInactiveGrouping.SelectClusterCandidates(
			allHoles,
			"recruit-being-parked",
			OVT_TEST_RecruitClusteringFixture.CLUSTER_ORIGIN,
			OVT_TEST_RecruitClusteringFixture.TEST_RADIUS);

		if (!fromAllHoles || fromAllHoles.Count() != 0)
		{
			SetFailure("A roster of nothing but holes produced %1 candidates, expected 0", fromAllHoles.Count().ToString());
			return true;
		}

		Print("Clustering: empty roster and holed roster both answer with an empty, non-null list");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Self, ACTIVE recruits and bodyless recruits are all excluded - each for its own reason.
//!
//! Three exclusions in one case because they are the same shape of claim and the case is only
//! meaningful if all three hold at once: a roster where the ONLY eligible record is the fourth one
//! proves that each of the first three was rejected for its own reason rather than by an accident of
//! ordering.
//!
//! SELF-EXCLUSION IS BY ID, NOT BY THE CALLER PRE-FILTERING. At the moment the manager asks this
//! question, the record being parked may or may not already carry m_bInactive - the order of "write
//! the flag" and "move the body" is the manager's to choose, and this helper has to answer the same
//! way either way. So the case deliberately includes the recruit being placed in its own roster,
//! marked parked and embodied and standing exactly at the origin: everything an eligible host looks
//! like, disqualified only by being itself.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RecruitClustering_ExclusionRules : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// All four are within the radius. Only the last one is eligible.
		array<ref OVT_RecruitData> roster = OVT_TEST_RecruitClusteringFixture.MakeRoster({
			OVT_TEST_RecruitClusteringFixture.MakeRecruit("recruit-being-parked", true, true, "0 0 0"),
			OVT_TEST_RecruitClusteringFixture.MakeRecruit("recruit-active", false, true, "5 0 0"),
			OVT_TEST_RecruitClusteringFixture.MakeRecruit("recruit-bodyless", true, false, "6 0 0"),
			OVT_TEST_RecruitClusteringFixture.MakeParkedAt("recruit-eligible-host", 7)
		});

		array<string> candidates = OVT_RecruitInactiveGrouping.SelectClusterCandidates(
			roster,
			"recruit-being-parked",
			OVT_TEST_RecruitClusteringFixture.CLUSTER_ORIGIN,
			OVT_TEST_RecruitClusteringFixture.TEST_RADIUS);

		// Each exclusion is named FIRST and separately, deliberately: these run before the count check
		// so that a failure says WHICH rule slipped instead of "the count is wrong", and so that the
		// count check below is the one that catches anything none of them anticipated.
		if (candidates.Find("recruit-being-parked") != -1)
		{
			SetFailure("The recruit being placed appeared in its own candidate list - it would be asked to join whatever group it is already in");
			return true;
		}

		if (candidates.Find("recruit-active") != -1)
		{
			SetFailure("An ACTIVE recruit was offered as a cluster host - joining its group would put the parked recruit back under its owner's command");
			return true;
		}

		if (candidates.Find("recruit-bodyless") != -1)
		{
			SetFailure("A recruit with no body was offered as a cluster host - it has no agent and therefore no group to join");
			return true;
		}

		if (candidates.Count() != 1)
		{
			SetFailure("A roster of self + 1 active + 1 bodyless + 1 eligible produced %1 candidates, expected 1", candidates.Count().ToString());
			return true;
		}

		if (candidates[0] != "recruit-eligible-host")
		{
			SetFailure("The single candidate was '%1', expected recruit-eligible-host", candidates[0]);
			return true;
		}

		// Excluding NOTHING is a legal request, and it is what makes the self-exclusion a real rule
		// rather than a side effect of some other filter: the same roster, asked with an empty
		// exclusion, hands back the self record too.
		array<string> withoutExclusion = OVT_RecruitInactiveGrouping.SelectClusterCandidates(
			roster,
			"",
			OVT_TEST_RecruitClusteringFixture.CLUSTER_ORIGIN,
			OVT_TEST_RecruitClusteringFixture.TEST_RADIUS);

		if (withoutExclusion.Count() != 2)
		{
			SetFailure("Control: asked with no exclusion, the same roster produced %1 candidates, expected 2 (the parked self record and the eligible host)", withoutExclusion.Count().ToString());
			return true;
		}

		if (withoutExclusion[0] != "recruit-being-parked" || withoutExclusion[1] != "recruit-eligible-host")
		{
			SetFailure("Control: with no exclusion the candidates were %1, %2 - expected recruit-being-parked, recruit-eligible-host",
				withoutExclusion[0], withoutExclusion[1]);
			return true;
		}

		Print("Clustering exclusions: self, active and bodyless recruits all rejected, each for its own reason");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Distance is measured from the RECORD's last known position, and far recruits do not host.
//!
//! THIS IS THE RULE THE FEATURE IS FOR. Two recruits parked side by side must share one group, and a
//! recruit parked across town must get its own; that is the whole difference between "a garrison of
//! five" and "five one-man formations", which is what a player actually sees.
//!
//! ⚠️ NO ASSERTION SITS ON THE BOUNDARY. vector.Distance is not correctly rounded (it is off by 1 ULP
//! at 1000 m and 2000 m), so "exactly at the radius" is a question about floating point rather than
//! about this rule. Everything here is at 0-30 m (comfortably inside 50) or at 80-400 m (comfortably
//! outside), and the case asserts the SIDE, never the edge.
//!
//! The radius is passed in explicitly by every case, so a change to the shipped default cannot make
//! these pass or fail. The default is asserted once, separately and on purpose: 50 m is decision D10
//! and it matches the fast-travel recruit radius, so a silent change to it would change what "nearby"
//! means in two features at once.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RecruitClustering_RadiusFiltersByLastKnownPosition : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// Near and far interleaved, so a filter that returned a contiguous slice would fail.
		array<ref OVT_RecruitData> roster = OVT_TEST_RecruitClusteringFixture.MakeRoster({
			OVT_TEST_RecruitClusteringFixture.MakeParkedAt("recruit-at-origin", 0),
			OVT_TEST_RecruitClusteringFixture.MakeParkedAt("recruit-far-80m", 80),
			OVT_TEST_RecruitClusteringFixture.MakeParkedAt("recruit-near-10m", 10),
			OVT_TEST_RecruitClusteringFixture.MakeParkedAt("recruit-far-400m", 400),
			OVT_TEST_RecruitClusteringFixture.MakeParkedAt("recruit-near-30m", 30)
		});

		array<string> candidates = OVT_RecruitInactiveGrouping.SelectClusterCandidates(
			roster,
			"recruit-being-parked",
			OVT_TEST_RecruitClusteringFixture.CLUSTER_ORIGIN,
			OVT_TEST_RecruitClusteringFixture.TEST_RADIUS);

		if (candidates.Count() != 3)
		{
			SetFailure("3 recruits within the radius produced %1 candidates, expected 3", candidates.Count().ToString());
			return true;
		}

		if (candidates[0] != "recruit-at-origin" || candidates[1] != "recruit-near-10m" || candidates[2] != "recruit-near-30m")
		{
			SetFailure("The near candidates were %1, %2, %3 - expected recruit-at-origin, recruit-near-10m, recruit-near-30m",
				candidates[0], candidates[1], candidates[2]);
			return true;
		}

		if (candidates.Find("recruit-far-80m") != -1 || candidates.Find("recruit-far-400m") != -1)
		{
			SetFailure("A recruit parked well outside the radius was offered as a cluster host - a garrison across town is not the same garrison");
			return true;
		}

		// Distance is measured in three dimensions, not on the ground plane: a recruit 200 m straight
		// up is not nearby. Cheap to state and it pins the axis choice, which a 2D distance helper
		// would silently change.
		array<ref OVT_RecruitData> vertical = OVT_TEST_RecruitClusteringFixture.MakeRoster({
			OVT_TEST_RecruitClusteringFixture.MakeRecruit("recruit-overhead", true, true, "0 200 0")
		});

		array<string> verticalCandidates = OVT_RecruitInactiveGrouping.SelectClusterCandidates(
			vertical,
			"recruit-being-parked",
			OVT_TEST_RecruitClusteringFixture.CLUSTER_ORIGIN,
			OVT_TEST_RecruitClusteringFixture.TEST_RADIUS);

		if (verticalCandidates.Count() != 0)
		{
			SetFailure("A recruit 200 m above the origin was offered as a cluster host - distance is three-dimensional");
			return true;
		}

		// A radius wide enough to swallow everything takes everything, which is what proves the
		// rejections above came from the DISTANCE and not from some other property of those records.
		array<string> wide = OVT_RecruitInactiveGrouping.SelectClusterCandidates(
			roster,
			"recruit-being-parked",
			OVT_TEST_RecruitClusteringFixture.CLUSTER_ORIGIN,
			1000.0);

		if (wide.Count() != 5)
		{
			SetFailure("Control: with a 1000 m radius the same roster produced %1 candidates, expected all 5", wide.Count().ToString());
			return true;
		}

		// Decision D10: the shipped default is 50 m, the same as the fast-travel recruit radius.
		if (!OVT_TEST_LogicFixture.FloatEquals(OVT_RecruitInactiveGrouping.DEFAULT_CLUSTER_RADIUS, 50.0))
		{
			SetFailure("The default cluster radius is %1 m, expected 50 - it is meant to match the fast-travel recruit radius",
				OVT_RecruitInactiveGrouping.DEFAULT_CLUSTER_RADIUS.ToString());
			return true;
		}

		Print("Clustering radius: 3 of 5 parked recruits are near enough, in 3D, and a wide radius takes all 5");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Candidates come back in table order, and every call gets its own list.
//!
//! ORDER IS A REAL CLAIM. The caller takes the FIRST candidate whose body turns out to be in a marked
//! group (decision D10, "first suitable host wins"), so the order decides which group a recruit joins
//! when two are in range. A stable order is what makes a failing play-test reproducible instead of
//! "sometimes they cluster and sometimes they do not".
//!
//! A FRESH LIST PER CALL IS ALSO A REAL CLAIM, and it is about a hazard this feature actually has:
//! the caller iterates the returned list while resolving each id through the recruit manager, and
//! that resolution PRUNES stale entries from a manager map as it walks. A shared or cached list would
//! put a second caller's iteration inside the first one's.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RecruitClustering_TableOrderStability : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// Ids chosen so that alphabetical order is NOT table order - a helper that sorted, or that
		// walked the table backwards, fails here. Distances rise and fall so that ordering by
		// proximity would fail too: "first suitable" means first in the TABLE, not nearest.
		array<ref OVT_RecruitData> roster = OVT_TEST_RecruitClusteringFixture.MakeRoster({
			OVT_TEST_RecruitClusteringFixture.MakeParkedAt("alpha", 30),
			OVT_TEST_RecruitClusteringFixture.MakeParkedAt("zulu", 5),
			OVT_TEST_RecruitClusteringFixture.MakeParkedAt("mike", 20),
			OVT_TEST_RecruitClusteringFixture.MakeParkedAt("bravo", 1)
		});

		array<string> candidates = OVT_RecruitInactiveGrouping.SelectClusterCandidates(
			roster,
			"recruit-being-parked",
			OVT_TEST_RecruitClusteringFixture.CLUSTER_ORIGIN,
			OVT_TEST_RecruitClusteringFixture.TEST_RADIUS);

		if (candidates.Count() != 4)
		{
			SetFailure("4 parked recruits inside the radius produced %1 candidates, expected 4", candidates.Count().ToString());
			return true;
		}

		array<string> expected = {"alpha", "zulu", "mike", "bravo"};
		for (int i = 0; i < expected.Count(); i++)
		{
			if (candidates[i] != expected[i])
			{
				SetFailure("Candidate order broke at index %1: got %2, expected %3", i.ToString(), candidates[i], expected[i]);
				return true;
			}
		}

		// The same question asked twice gives two independent lists, not two handles on one.
		array<string> second = OVT_RecruitInactiveGrouping.SelectClusterCandidates(
			roster,
			"recruit-being-parked",
			OVT_TEST_RecruitClusteringFixture.CLUSTER_ORIGIN,
			OVT_TEST_RecruitClusteringFixture.TEST_RADIUS);

		candidates.Insert("appended-by-the-first-caller");

		if (second.Count() != 4)
		{
			SetFailure("Two calls shared one list: appending to the first changed the second to %1 entries", second.Count().ToString());
			return true;
		}

		// And neither list is a view onto the owner's roster.
		if (roster.Count() != 4)
		{
			SetFailure("Appending to a returned candidate list changed the owner's roster to %1 records", roster.Count().ToString());
			return true;
		}

		Print("Clustering order: candidates come back in table order, and each call owns its own list");

		return true;
	}
}
