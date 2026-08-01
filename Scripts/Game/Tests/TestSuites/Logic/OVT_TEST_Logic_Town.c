//------------------------------------------------------------------------------------------------
//! TIER A cases - town record maths and town modifier recalculation.
//!
//! Every subject here is built with `new` and fed by hand; nothing in this file resolves a manager,
//! a controller or a world. See OVT_TEST_LogicSuite.c for the tier rule and the house rules.
//!
//! NO CASE IN THIS FILE PINS A BUG, WHICH IS ITSELF A FINDING. The plan expected
//! OVT_TownData.SupportPercentage() to be broken by integer division (Decision 10 / risk R7) and
//! budgeted a pinning case for it. Measured on 1.7.0.54, it is not: EnforceScript does not truncate
//! a division between two ints in these expressions, so the method returns a real percentage. The
//! boundary table below asserts the correct behaviour instead, and the correction is recorded in
//! findings.md under "Differs from assumptions".
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! OVT_TownData.SupportPercentage() across its whole boundary table.
//!
//! Rows: no population at all, partial support, support equal to population, and support above
//! population. The zero-population guard is the load-bearing one - without it the method divides by
//! zero on every town that has not been populated yet.
//!
//! The last row documents that there is NO upper clamp: the method can return more than 100, which
//! every caller comparing it against a percentage threshold inherits.
//!
//! THE PARTIAL-SUPPORT ROW WAS EXPECTED TO PIN A BUG AND DOES NOT. The plan predicted that
//! `support / population` between two ints would truncate to 0 and make this method a step
//! function. It measured otherwise on 1.7.0.54: EnforceScript does not truncate this division, and
//! 25 supporters of 50 civilians correctly reads as 50 (findings.md, Phase 5). The row is kept as a
//! plain correctness assertion - and it is the row that would go red if the language, or the
//! method, ever started truncating.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Town_SupportPercentage_Boundaries : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		// Population 0, support 0 - the guard returns before the division.
		OVT_TownData emptyTown = OVT_TEST_LogicFixture.MakeTown(0, 0);
		int emptyResult = emptyTown.SupportPercentage();
		if (emptyResult != 0)
		{
			SetResultFailure("SupportPercentage() with population 0 and support 0 returned %1, expected 0", emptyResult.ToString());
			return true;
		}

		// Population 0 with supporters - still guarded, still 0 rather than a divide by zero.
		OVT_TownData ghostTown = OVT_TEST_LogicFixture.MakeTown(0, 10);
		int ghostResult = ghostTown.SupportPercentage();
		if (ghostResult != 0)
		{
			SetResultFailure("SupportPercentage() with population 0 and support 10 returned %1, expected 0", ghostResult.ToString());
			return true;
		}

		// Support below population: a real percentage, not a truncated zero.
		OVT_TownData halfTown = OVT_TEST_LogicFixture.MakeTown(50, 25);
		int halfResult = halfTown.SupportPercentage();
		if (halfResult != 50)
		{
			SetResultFailure("SupportPercentage() with support 25 of population 50 returned %1, expected 50", halfResult.ToString());
			return true;
		}

		// One supporter short of the whole town, and rounded rather than truncated.
		OVT_TownData nearlyFullTown = OVT_TEST_LogicFixture.MakeTown(50, 49);
		int nearlyFullResult = nearlyFullTown.SupportPercentage();
		if (nearlyFullResult != 98)
		{
			SetResultFailure("SupportPercentage() with support 49 of population 50 returned %1, expected 98", nearlyFullResult.ToString());
			return true;
		}

		// Every civilian supports: exactly 100.
		OVT_TownData fullTown = OVT_TEST_LogicFixture.MakeTown(50, 50);
		int fullResult = fullTown.SupportPercentage();
		if (fullResult != 100)
		{
			SetResultFailure("SupportPercentage() with support == population returned %1, expected 100", fullResult.ToString());
			return true;
		}

		// More supporters than civilians: NOT clamped to 100.
		OVT_TownData overTown = OVT_TEST_LogicFixture.MakeTown(50, 150);
		int overResult = overTown.SupportPercentage();
		if (overResult != 300)
		{
			SetResultFailure("SupportPercentage() with support 150 of population 50 returned %1, expected 300 (there is no upper clamp)", overResult.ToString());
			return true;
		}

		PrintFormat("SupportPercentage boundaries: empty %1, half %2, full %3",
			emptyResult.ToString(), halfResult.ToString(), fullResult.ToString());
		PrintFormat("SupportPercentage boundaries: 49 of 50 -> %1, 150 of 50 -> %2 (unclamped)",
			nearlyFullResult.ToString(), overResult.ToString());

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_TownModifierSystem.Recalculate() - the generic (stability-shaped) recalculation.
//!
//! Covers, on a hand-built m_Config with Init() never called (Init() would reach for the town
//! manager, which does not exist in this tier):
//!   - empty modifier list is the identity: the base value comes back unchanged;
//!   - modifier effects are SUMMED onto the base value;
//!   - null entries in the list are skipped rather than dereferenced;
//!   - the result is clamped at max and at min;
//!   - the result is ROUNDED, not truncated, so fractional effects do not silently vanish.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Town_Modifiers_RecalculateSumsAndClamps : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		// id 0 = -5, id 1 = -10, id 2 = +50.
		OVT_TownModifierSystem system = new OVT_TownModifierSystem();
		system.m_Config = OVT_TEST_LogicFixture.MakeModifiersConfig(-5, -10, 50);

		// Identity: no modifiers, no change.
		array<ref OVT_TownModifierData> noModifiers = OVT_TEST_LogicFixture.MakeModifierList();
		int identity = system.Recalculate(noModifiers, 100, 0, 100);
		if (identity != 100)
		{
			SetResultFailure("Recalculate() with no modifiers returned %1, expected the base value 100", identity.ToString());
			return true;
		}

		// One modifier: base + effect.
		array<ref OVT_TownModifierData> oneModifier = OVT_TEST_LogicFixture.MakeModifierList(0);
		int single = system.Recalculate(oneModifier, 100, 0, 100);
		if (single != 95)
		{
			SetResultFailure("Recalculate() with one -5 modifier returned %1, expected 95", single.ToString());
			return true;
		}

		// A null entry in the list must be skipped, not dereferenced.
		oneModifier.Insert(null);
		int withNull = system.Recalculate(oneModifier, 100, 0, 100);
		if (withNull != 95)
		{
			SetResultFailure("Recalculate() with a null entry alongside one -5 modifier returned %1, expected 95", withNull.ToString());
			return true;
		}

		// Two modifiers sum, and the base value is honoured rather than assumed to be 100.
		array<ref OVT_TownModifierData> twoModifiers = OVT_TEST_LogicFixture.MakeModifierList(0, 1);
		int summed = system.Recalculate(twoModifiers, 50, 0, 100);
		if (summed != 35)
		{
			SetResultFailure("Recalculate() with -5 and -10 on a base of 50 returned %1, expected 35", summed.ToString());
			return true;
		}

		// Clamp at max: 100 + 50 is capped.
		array<ref OVT_TownModifierData> boostModifier = OVT_TEST_LogicFixture.MakeModifierList(2);
		int clampedHigh = system.Recalculate(boostModifier, 100, 0, 100);
		if (clampedHigh != 100)
		{
			SetResultFailure("Recalculate() with a +50 modifier on a base of 100 returned %1, expected the max 100", clampedHigh.ToString());
			return true;
		}

		// Clamp at min: three -10 modifiers on a base of 10 would be -20.
		array<ref OVT_TownModifierData> drainModifiers = OVT_TEST_LogicFixture.MakeModifierList(1, 1, 1);
		int clampedLow = system.Recalculate(drainModifiers, 10, 0, 100);
		if (clampedLow != 0)
		{
			SetResultFailure("Recalculate() with three -10 modifiers on a base of 10 returned %1, expected the min 0", clampedLow.ToString());
			return true;
		}

		// Non-default min: the clamp uses the caller's floor, not zero.
		int clampedToFloor = system.Recalculate(drainModifiers, 10, 5, 100);
		if (clampedToFloor != 5)
		{
			SetResultFailure("Recalculate() with a min of 5 returned %1, expected 5", clampedToFloor.ToString());
			return true;
		}

		// Rounding, not truncation: a -0.4 effect on a base of 100 is 99.6 -> 100, and a +0.4
		// effect on a base of 50 is 50.4 -> 50. Both would be 99 and 50 under truncation, so the
		// first assertion is the one that distinguishes them.
		OVT_TownModifierSystem fractionalSystem = new OVT_TownModifierSystem();
		fractionalSystem.m_Config = OVT_TEST_LogicFixture.MakeModifiersConfig(-0.4, 0.4, 0);

		array<ref OVT_TownModifierData> smallDrain = OVT_TEST_LogicFixture.MakeModifierList(0);
		int roundedUp = fractionalSystem.Recalculate(smallDrain, 100, 0, 100);
		if (roundedUp != 100)
		{
			SetResultFailure("Recalculate() with a -0.4 effect on a base of 100 returned %1, expected 100 (rounded, not truncated)", roundedUp.ToString());
			return true;
		}

		array<ref OVT_TownModifierData> smallBoost = OVT_TEST_LogicFixture.MakeModifierList(1);
		int roundedDown = fractionalSystem.Recalculate(smallBoost, 50, 0, 100);
		if (roundedDown != 50)
		{
			SetResultFailure("Recalculate() with a +0.4 effect on a base of 50 returned %1, expected 50", roundedDown.ToString());
			return true;
		}

		PrintFormat("Recalculate: identity %1, sum %2, clamped %3", identity.ToString(), summed.ToString(), clampedHigh.ToString());

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_TownSupportModifierSystem.Recalculate() - DETERMINISTIC BRANCHES ONLY.
//!
//! This override converts a summed modifier effect into a supporter count rather than a percentage,
//! and four of its paths are deterministic:
//!   - summed effect  >  75  ->  exactly one supporter gained;
//!   - summed effect  < -75  ->  exactly one supporter lost;
//!   - summed effect == 0    ->  no change (only the clamps apply);
//!   - max == 0              ->  early return of the base value, guarding a division by zero.
//! Plus the final clamps into [min, max].
//!
//! THE RNG-GATED BRANCHES ARE DELIBERATELY NOT TESTED.
//! When the summed effect is between -75 and 75 and non-zero, the method rolls
//! s_AIRandomGenerator.RandFloatXY(0, 100) and gains or loses a supporter based on the result.
//! There is no seam to seed or inject that generator, so a case over those branches could only be
//! made green by retrying until it passed - and the framework's per-case retry attribute is banned
//! by this feature's quality bar, precisely because it converts "this test is not deterministic"
//! into "this test is slow". Covering them needs a seam in gameplay code, which this feature is
//! not allowed to add.
//!
//! Note also that the summed effect itself is clamped to [-100, 100] before the branch, which the
//! "two large effects" assertion below exercises.
//!
//! EXPECTED CONSOLE NOISE: the max == 0 assertion drives a path that Print()s an ERROR line plus a
//! diagnostic dump from gameplay code. That is the guard working as designed, not a test failure -
//! verdicts come from junit.xml, never from console error counts.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Town_SupportModifiers_DeterministicBranches : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		// id 0 = +80 (over the +75 threshold), id 1 = -80 (under the -75 threshold), id 2 = +40.
		OVT_TownSupportModifierSystem system = new OVT_TownSupportModifierSystem();
		system.m_Config = OVT_TEST_LogicFixture.MakeModifiersConfig(80, -80, 40);

		// No modifiers: the summed effect is 0, no branch fires, the base value survives.
		array<ref OVT_TownModifierData> noModifiers = OVT_TEST_LogicFixture.MakeModifierList();
		int unchanged = system.Recalculate(noModifiers, 25, 0, 50);
		if (unchanged != 25)
		{
			SetResultFailure("Support Recalculate() with no modifiers returned %1, expected the base value 25", unchanged.ToString());
			return true;
		}

		// Summed effect above +75: exactly one supporter is gained, never a proportional jump.
		array<ref OVT_TownModifierData> strongPositive = OVT_TEST_LogicFixture.MakeModifierList(0);
		int gained = system.Recalculate(strongPositive, 25, 0, 50);
		if (gained != 26)
		{
			SetResultFailure("Support Recalculate() with a +80 effect on a base of 25 returned %1, expected 26 (one supporter gained)", gained.ToString());
			return true;
		}

		// Summed effect below -75: exactly one supporter is lost.
		array<ref OVT_TownModifierData> strongNegative = OVT_TEST_LogicFixture.MakeModifierList(1);
		int lost = system.Recalculate(strongNegative, 25, 0, 50);
		if (lost != 24)
		{
			SetResultFailure("Support Recalculate() with a -80 effect on a base of 25 returned %1, expected 24 (one supporter lost)", lost.ToString());
			return true;
		}

		// Two positive effects sum to 120 and are clamped to 100, which is still over +75, so the
		// outcome is the same single supporter - the clamp changes the number, not the branch.
		array<ref OVT_TownModifierData> twoPositives = OVT_TEST_LogicFixture.MakeModifierList(0, 2);
		int clampedEffect = system.Recalculate(twoPositives, 25, 0, 50);
		if (clampedEffect != 26)
		{
			SetResultFailure("Support Recalculate() with +80 and +40 (summed effect clamped to 100) returned %1, expected 26", clampedEffect.ToString());
			return true;
		}

		// The gained supporter is clamped at max.
		int clampedHigh = system.Recalculate(strongPositive, 50, 0, 50);
		if (clampedHigh != 50)
		{
			SetResultFailure("Support Recalculate() gaining a supporter at the max of 50 returned %1, expected 50", clampedHigh.ToString());
			return true;
		}

		// The lost supporter is clamped at min.
		int clampedLow = system.Recalculate(strongNegative, 0, 0, 50);
		if (clampedLow != 0)
		{
			SetResultFailure("Support Recalculate() losing a supporter at the min of 0 returned %1, expected 0", clampedLow.ToString());
			return true;
		}

		// max == 0 returns the base value early, before any branch and before the clamps. Without
		// that guard the method would divide by zero computing its support percentage.
		int guarded = system.Recalculate(strongPositive, 25, 0, 0);
		if (guarded != 25)
		{
			SetResultFailure("Support Recalculate() with max 0 returned %1, expected the base value 25 unchanged", guarded.ToString());
			return true;
		}

		PrintFormat("Support Recalculate: unchanged %1, gained %2, lost %3", unchanged.ToString(), gained.ToString(), lost.ToString());
		PrintFormat("Support Recalculate: clamped high %1, clamped low %2, max-zero guard %3",
			clampedHigh.ToString(), clampedLow.ToString(), guarded.ToString());

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_TownData.IsWithinTownBounds() - a fixed 500 m radius around the town location.
//!
//! The radius is hardcoded in the method and is NOT the town controller's m_iTownRange, which is
//! what a reader would expect and is 80 m in the test world. Anything that decides "is the player
//! in this town" through this method is therefore using a radius no map can configure. Pinned here
//! as documented behaviour rather than logged as a bug, because several callers depend on the
//! generous radius.
//!
//! Also covers the two edges that matter: the comparison is strictly less-than, and the distance is
//! three-dimensional, so altitude counts.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Town_IsWithinTownBounds_FixedRadius : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TownData town = OVT_TEST_LogicFixture.MakeTown(50, 0);
		town.location = "1000 0 1000";

		if (!town.IsWithinTownBounds(town.location))
		{
			SetResultFailure("IsWithinTownBounds() rejected the town's own location");
			return true;
		}

		if (!town.IsWithinTownBounds("1499 0 1000"))
		{
			SetResultFailure("IsWithinTownBounds() rejected a position 499 m away, inside the 500 m radius");
			return true;
		}

		// Strictly less than: exactly 500 m is OUT.
		if (town.IsWithinTownBounds("1500 0 1000"))
		{
			SetResultFailure("IsWithinTownBounds() accepted a position exactly 500 m away - the comparison is supposed to be strictly less than the radius");
			return true;
		}

		if (town.IsWithinTownBounds("2000 0 1000"))
		{
			SetResultFailure("IsWithinTownBounds() accepted a position 1000 m away");
			return true;
		}

		// The distance is 3D, so a position directly overhead can be outside the bounds.
		if (town.IsWithinTownBounds("1000 600 1000"))
		{
			SetResultFailure("IsWithinTownBounds() accepted a position 600 m directly above the town - the distance is supposed to be three-dimensional");
			return true;
		}

		Print("IsWithinTownBounds: 499 m in, 500 m out, 600 m of altitude out");

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_TownData.SetAreaHeat() / GetAreaHeat() - the undercover system's per-town heat value.
//!
//! SetAreaHeat clamps NEGATIVE values to zero and has no upper bound. Both halves are asserted:
//! the lower clamp is the documented intent ("Prevent negative heat"), and the absence of an upper
//! clamp is pinned as current behaviour so that adding a ceiling later is a visible change rather
//! than a silent one.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Town_AreaHeat_ClampsNegativeToZero : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TownData town = OVT_TEST_LogicFixture.MakeTown(50, 0);

		if (!OVT_TEST_LogicFixture.FloatEquals(town.GetAreaHeat(), 0))
		{
			SetResultFailure("A fresh town's area heat is %1, expected 0", town.GetAreaHeat().ToString());
			return true;
		}

		town.SetAreaHeat(12.5);
		if (!OVT_TEST_LogicFixture.FloatEquals(town.GetAreaHeat(), 12.5))
		{
			SetResultFailure("SetAreaHeat(12.5) then GetAreaHeat() returned %1", town.GetAreaHeat().ToString());
			return true;
		}

		// Negative heat is clamped to zero, not stored and not ignored.
		town.SetAreaHeat(-5);
		if (!OVT_TEST_LogicFixture.FloatEquals(town.GetAreaHeat(), 0))
		{
			SetResultFailure("SetAreaHeat(-5) left the area heat at %1, expected it clamped to 0", town.GetAreaHeat().ToString());
			return true;
		}

		// No upper clamp exists today. Pinned so that introducing one is a deliberate change.
		town.SetAreaHeat(1000);
		if (!OVT_TEST_LogicFixture.FloatEquals(town.GetAreaHeat(), 1000))
		{
			SetResultFailure("SetAreaHeat(1000) stored %1 - an upper clamp appears to have been added, which this case pinned as absent", town.GetAreaHeat().ToString());
			return true;
		}

		Print("Area heat: 12.5 stored, -5 clamped to 0, 1000 stored unclamped");

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_TownData.CopyFrom() - which fields a town record copies, and which it deliberately does not.
//!
//! CopyFrom is the seam the persistence restore path uses to pour a loaded town record into the
//! town the world already created, which is why it copies the mutable campaign state (population,
//! stability, support, faction, modifier lists, gun dealer position, area heat) and NOT the
//! identity of the destination town: `location` and `size` are left alone, because the live world's
//! own town placement is authoritative and a restored location would move the town.
//!
//! Both halves are asserted. The "not copied" half is the one that silently breaks if someone adds
//! `location = town.location;` to CopyFrom while chasing an unrelated bug.
//!
//! The modifier arrays are copied BY REFERENCE - the two records end up sharing one array object.
//! That is harmless in the one place CopyFrom is used (the source record is discarded immediately
//! afterwards) and is asserted here so the aliasing is on record rather than assumed away.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Town_CopyFrom_CopiesRecordButNotLocation : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TownData destination = OVT_TEST_LogicFixture.MakeTown(50, 0);
		destination.location = "100 0 100";
		destination.size = OVT_TownSize.VILLAGE;

		OVT_TownData source = OVT_TEST_LogicFixture.MakeTown(1234, 77, 42);
		source.location = "900 0 900";
		source.size = OVT_TownSize.CITY;
		source.targetPopulation = 4321;
		source.faction = 3;
		source.gunDealerPosition = "111 0 222";
		source.SetAreaHeat(9.5);
		source.stabilityModifiers.Insert(OVT_TEST_LogicFixture.MakeModifierData(1));
		source.supportModifiers.Insert(OVT_TEST_LogicFixture.MakeModifierData(2));

		destination.CopyFrom(source);

		if (destination.population != 1234)
		{
			SetResultFailure("CopyFrom() left population at %1, expected 1234", destination.population.ToString());
			return true;
		}

		if (destination.targetPopulation != 4321)
		{
			SetResultFailure("CopyFrom() left targetPopulation at %1, expected 4321", destination.targetPopulation.ToString());
			return true;
		}

		if (destination.stability != 42)
		{
			SetResultFailure("CopyFrom() left stability at %1, expected 42", destination.stability.ToString());
			return true;
		}

		if (destination.support != 77)
		{
			SetResultFailure("CopyFrom() left support at %1, expected 77", destination.support.ToString());
			return true;
		}

		if (destination.faction != 3)
		{
			SetResultFailure("CopyFrom() left faction at %1, expected 3", destination.faction.ToString());
			return true;
		}

		if (destination.gunDealerPosition != source.gunDealerPosition)
		{
			SetResultFailure("CopyFrom() left gunDealerPosition at %1, expected %2",
				destination.gunDealerPosition.ToString(), source.gunDealerPosition.ToString());
			return true;
		}

		if (!OVT_TEST_LogicFixture.FloatEquals(destination.GetAreaHeat(), 9.5))
		{
			SetResultFailure("CopyFrom() left area heat at %1, expected 9.5", destination.GetAreaHeat().ToString());
			return true;
		}

		if (destination.stabilityModifiers.Count() != 1 || destination.supportModifiers.Count() != 1)
		{
			SetResultFailure("CopyFrom() left %1 stability and %2 support modifiers, expected 1 of each",
				destination.stabilityModifiers.Count().ToString(), destination.supportModifiers.Count().ToString());
			return true;
		}

		// The modifier arrays are shared, not deep-copied: mutating the source is visible through
		// the destination.
		source.stabilityModifiers.Insert(OVT_TEST_LogicFixture.MakeModifierData(0));
		if (destination.stabilityModifiers.Count() != 2)
		{
			SetResultFailure("CopyFrom() appears to deep-copy the modifier arrays now (destination has %1 entries after the source grew to 2) - this case pinned them as shared references", destination.stabilityModifiers.Count().ToString());
			return true;
		}

		// Deliberately NOT copied: the destination keeps its own identity.
		if (destination.location != "100 0 100")
		{
			SetResultFailure("CopyFrom() overwrote location with %1 - it is supposed to leave the destination's own location alone", destination.location.ToString());
			return true;
		}

		if (destination.size != OVT_TownSize.VILLAGE)
		{
			SetResultFailure("CopyFrom() overwrote size - it is supposed to leave the destination's own size alone");
			return true;
		}

		Print("CopyFrom: campaign state copied, location and size preserved, modifier arrays shared");

		SetResultSuccess();
		return true;
	}
}
