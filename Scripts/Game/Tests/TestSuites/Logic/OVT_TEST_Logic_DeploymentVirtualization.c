//------------------------------------------------------------------------------------------------
//! TIER A cases - OVT_DeploymentVirtualKey and OVT_VirtualPlanFactory, the two pure statics behind
//! virtualized deployments.
//!
//! These two subjects carry the whole of the reclaim contract's arithmetic. A deployment does not
//! persist the handles of the groups it registered; it re-derives an owner key after a load and
//! asks the registry what is already tagged with it. If a key derivation ever drifted - a float
//! rounding differently, a name gaining a space, two deployments colliding onto one string - the
//! campaign would either resurrect a force it had already lost or orphan one it still had, and
//! neither failure is visible from the outside until somebody counts soldiers. That is exactly the
//! kind of claim that belongs in the cheapest tier in the tree, which is why both subjects were
//! written free of every system reference in the first place.
//!
//! The plan half is the same argument in geometry. A plan is registration-time input AND the only
//! opt-in there is for being walked while dormant: a DEFEND-only plan is a garrison that holds its
//! post, a patrol plan keeps patrolling with nobody watching. The three arrays are consumed by
//! index and a ragged plan is refused outright, so "the arrays are the same length" is asserted on
//! every builder rather than assumed once.
//!
//! WHAT IS NOT HERE. Registration itself, the reclaim round trip, the serializer's version-2 key
//! field and the fan-out from the restore/wipe events all need live systems and are asserted in the
//! Init and Persistence tiers. Nothing in this file reaches for anything the tier rule forbids.
//!
//! ⚠ vector.Distance is +1 ULP off at exactly 1000 m and 2000 m (project memory). Every distance
//! here is taken at a small, safe magnitude - hundreds of metres - and every float claim carries a
//! tolerance.
//!
//! COMPILE PROOF (recorded 2026-08-17): an unknown type was substituted for the subject class in
//! this file and tools/compile-check.sh reported an error against this exact path, so these cases
//! are genuinely in the compile set and their assertions are genuinely executed. No retry attribute
//! is used anywhere - this quality bar bans it outright.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Key derivation: the same deployment must derive the same string, run after run and load after
//! load.
//!
//! Rounding to whole metres is what makes that true. A key built from a raw float would differ in
//! its last digit between the process that wrote it and the process that re-derives it, and the
//! reclaim would come back empty - a deployment would then register its whole force a second time
//! on top of the one it already had. The two sub-metre positions below round to the same key on
//! purpose: that IS the reclaim guarantee, stated as an assertion.
//!
//! Sanitisation is the other half. '@' and '#' are structural in a composed key, and an authored
//! name is a human label that can gain or lose a space between releases; either would silently
//! orphan every group registered under the old spelling.
//!
//! ⚠ No assertion below sits on a .5 boundary: the engine's rounding mode at exactly .5 is not
//! something this file has verified, and pinning an unverified engine behaviour would be a claim
//! about the engine rather than about the subject.
//!
//! FAIL PROOF (edit recorded; execution belongs to the phase's suite run): drop the Math.Round()
//! calls in DeriveKey and pass the floats straight through, and the sub-metre assertion goes red
//! (two positions 0.2 m apart produce two different keys). Delete the ASCII_SPACE branch in
//! Sanitise and the whitespace assertion goes red.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_DeploymentVirtualization_KeyDerivation : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// Determinism: the same inputs answer the same string, every time.
		string first = OVT_DeploymentVirtualKey.DeriveKey("TownPatrol", 4821.2, 7093.4);
		string second = OVT_DeploymentVirtualKey.DeriveKey("TownPatrol", 4821.2, 7093.4);

		if (first != second)
		{
			SetFailure("The same inputs derived two different keys, %1 and %2", first, second);
			return true;
		}

		if (first != "TownPatrol@4821_7093")
		{
			SetFailure("A key derived as %1, expected TownPatrol@4821_7093", first);
			return true;
		}

		// Sub-metre jitter rounds to the SAME key. This is the reclaim guarantee: the position a key
		// is re-derived from after a load is never bit-identical to the one that wrote it.
		string jittered = OVT_DeploymentVirtualKey.DeriveKey("TownPatrol", 4821.4, 7093.2);
		if (jittered != first)
		{
			SetFailure("A 0.2 m difference derived %1 instead of %2 - a reclaim would find nothing", jittered, first);
			return true;
		}

		// A whole metre of difference rounds differently and IS a different deployment.
		string moved = OVT_DeploymentVirtualKey.DeriveKey("TownPatrol", 4822.4, 7093.2);
		if (moved == first)
		{
			SetFailure("A 1 m difference derived the same key %1 - two deployments would share one force", moved);
			return true;
		}

		// Negative coordinates are ordinary positions, not an error, and do not collide with positive
		// ones.
		string negative = OVT_DeploymentVirtualKey.DeriveKey("TownPatrol", -4821.2, 7093.4);
		if (negative == first)
		{
			SetFailure("A position mirrored across the origin derived the same key %1", negative);
			return true;
		}

		// SANITISATION. '@' and '#' are structural and whitespace is editorial; all three come out.
		string sanitised = OVT_DeploymentVirtualKey.DeriveKey("A@B#C D", 0, 0);
		if (sanitised != "ABCD@0_0")
		{
			SetFailure("A name carrying @, # and a space derived %1, expected ABCD@0_0", sanitised);
			return true;
		}

		// The same name with the spacing an editor might have left behind is the SAME owner.
		string respaced = OVT_DeploymentVirtualKey.DeriveKey("Tower Garrison", 100.2, 200.2);
		string tightened = OVT_DeploymentVirtualKey.DeriveKey("TowerGarrison", 100.2, 200.2);
		if (respaced != tightened)
		{
			SetFailure("Respacing a name derived %1 instead of %2 - every group under the old spelling would be orphaned", respaced, tightened);
			return true;
		}

		// An empty name still yields a usable key rather than a bare "@0_0".
		string unnamed = OVT_DeploymentVirtualKey.DeriveKey("", 100.2, 200.2);
		if (unnamed != "unnamed@100_200")
		{
			SetFailure("An empty name derived %1, expected unnamed@100_200", unnamed);
			return true;
		}

		// A name made ENTIRELY of stripped characters is the same case, and must not fall through to
		// an empty leading part.
		string blank = OVT_DeploymentVirtualKey.DeriveKey("  # @ ", 100.2, 200.2);
		if (blank != unnamed)
		{
			SetFailure("A name of nothing but stripped characters derived %1, expected %2", blank, unnamed);
			return true;
		}

		Print("Key derivation is deterministic, rounds sub-metre jitter onto one key, separates a 1 m move, and strips @, # and whitespace out of the name");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Key composition: disambiguation, module tags and the owner key that carries them.
//!
//! Two deployments of the same config CAN land on the same rounded spot - the creation path's 250 m
//! same-name dedup makes it rare rather than impossible, and a save restored on top of a live
//! session can produce it outright. The ordinal is what keeps their forces apart; ordinal 0 leaves
//! the base key untouched so the ordinary case stays readable in a log.
//!
//! The owner key is the ambiguity claim, and it is not academic. A deployment key legitimately ends
//! in "#2" and a module tag is free text, so a naive join would let ("a#b", "c") and ("a", "b#c")
//! compose to one string - two different modules reclaiming each other's groups, each then
//! registering to make up its own shortfall. Sanitising the tag at composition time is what stops
//! it, and it is asserted here rather than trusted.
//!
//! FAIL PROOF (edit recorded): make OwnerKey return `deploymentKey + PART_MARK + moduleTag` without
//! the Sanitise call and the non-ambiguity assertion goes red (both sides compose to a#b#c). Change
//! Disambiguate's guard to `if (ordinal < 0)` and the ordinal-0 assertion goes red (it answers
//! "key#0").
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_DeploymentVirtualization_KeyComposition : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string baseKey = "TownPatrol@4821_7093";

		// Ordinal 0 is the first holder and changes nothing.
		string firstHolder = OVT_DeploymentVirtualKey.Disambiguate(baseKey, 0);
		if (firstHolder != baseKey)
		{
			SetFailure("Ordinal 0 produced %1, expected the base key %2 unchanged", firstHolder, baseKey);
			return true;
		}

		// Any later holder is a different string, and a readable one.
		string secondHolder = OVT_DeploymentVirtualKey.Disambiguate(baseKey, 2);
		if (secondHolder == baseKey)
		{
			SetFailure("Ordinal 2 produced the base key %1 - two deployments on one spot would share one force", secondHolder);
			return true;
		}

		if (secondHolder != "TownPatrol@4821_7093#2")
		{
			SetFailure("Ordinal 2 produced %1, expected TownPatrol@4821_7093#2", secondHolder);
			return true;
		}

		string thirdHolder = OVT_DeploymentVirtualKey.Disambiguate(baseKey, 3);
		if (thirdHolder == secondHolder)
		{
			SetFailure("Ordinals 2 and 3 both produced %1", thirdHolder);
			return true;
		}

		// A negative ordinal is not a third shape: it falls back to the base key.
		string negativeOrdinal = OVT_DeploymentVirtualKey.Disambiguate(baseKey, -1);
		if (negativeOrdinal != baseKey)
		{
			SetFailure("A negative ordinal produced %1, expected the base key %2", negativeOrdinal, baseKey);
			return true;
		}

		// MODULE TAGS. An authored name wins, because it survives the module being re-ordered.
		string namedTag = OVT_DeploymentVirtualKey.ModuleTag("Spawn Infantry", 0);
		if (namedTag != "Spawn Infantry")
		{
			SetFailure("A named module tagged as %1, expected its authored name", namedTag);
			return true;
		}

		// Both shipped vehicle configs leave the name blank, so the positional fallback is the live
		// path and not a theoretical one.
		string fallbackTag = OVT_DeploymentVirtualKey.ModuleTag("", 2);
		if (fallbackTag != "m2")
		{
			SetFailure("An unnamed module at index 2 tagged as %1, expected m2", fallbackTag);
			return true;
		}

		string firstFallback = OVT_DeploymentVirtualKey.ModuleTag("", 0);
		if (firstFallback == fallbackTag)
		{
			SetFailure("Unnamed modules at index 0 and index 2 both tagged as %1", firstFallback);
			return true;
		}

		// OWNER KEYS. The deployment key survives verbatim, so a key stays readable end to end.
		string ownerKey = OVT_DeploymentVirtualKey.OwnerKey(baseKey, "Spawn Infantry");
		if (!ownerKey.StartsWith(baseKey))
		{
			SetFailure("An owner key %1 does not start with its deployment key %2", ownerKey, baseKey);
			return true;
		}

		if (ownerKey != "TownPatrol@4821_7093#SpawnInfantry")
		{
			SetFailure("An owner key composed as %1, expected TownPatrol@4821_7093#SpawnInfantry", ownerKey);
			return true;
		}

		// A disambiguated deployment key still composes, and does not collide with the first holder's.
		string secondOwnerKey = OVT_DeploymentVirtualKey.OwnerKey(secondHolder, "Spawn Infantry");
		if (secondOwnerKey == ownerKey)
		{
			SetFailure("Two deployments on one spot composed the same owner key %1", ownerKey);
			return true;
		}

		// THE AMBIGUITY CLAIM: the '#' between the two parts must not be forgeable from either side.
		string leftHeavy = OVT_DeploymentVirtualKey.OwnerKey("a#b", "c");
		string rightHeavy = OVT_DeploymentVirtualKey.OwnerKey("a", "b#c");
		if (leftHeavy == rightHeavy)
		{
			SetFailure("(a#b, c) and (a, b#c) both composed to %1 - two modules would reclaim each other's groups", leftHeavy);
			return true;
		}

		// Composition is deterministic, which is the whole point of re-deriving it after a load.
		string again = OVT_DeploymentVirtualKey.OwnerKey(baseKey, "Spawn Infantry");
		if (again != ownerKey)
		{
			SetFailure("The same parts composed %1 and then %2", ownerKey, again);
			return true;
		}

		Print("Key composition: ordinal 0 is the bare key, later ordinals separate, module tags prefer the authored name and fall back to the index, and owner keys cannot be forged from either side");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Missing count: how many more groups to register, and never a negative answer.
//!
//! Holding MORE than the wanted count is a normal state - a count re-rolled lower on a restore, a
//! reinforcement already paid for, a reclaim that found groups a previous session registered - and
//! it means "register nothing". A negative answer would be read as a loop bound somewhere and would
//! either do nothing by luck or unregister a force nobody asked to lose.
//!
//! FAIL PROOF (edit recorded): make MissingCount return `wanted - held` unconditionally and the
//! over-held assertion goes red (-2 instead of 0).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_DeploymentVirtualization_MissingCount : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// The ordinary case: nothing held yet.
		int missing = OVT_DeploymentVirtualKey.MissingCount(4, 0);
		if (missing != 4)
		{
			SetFailure("Wanting 4 and holding 0 answered %1, expected 4", missing.ToString());
			return true;
		}

		// Partly reclaimed.
		missing = OVT_DeploymentVirtualKey.MissingCount(4, 3);
		if (missing != 1)
		{
			SetFailure("Wanting 4 and holding 3 answered %1, expected 1", missing.ToString());
			return true;
		}

		// Exactly satisfied.
		missing = OVT_DeploymentVirtualKey.MissingCount(4, 4);
		if (missing != 0)
		{
			SetFailure("Wanting 4 and holding 4 answered %1, expected 0", missing.ToString());
			return true;
		}

		// Over-held: register nothing, and never a negative.
		missing = OVT_DeploymentVirtualKey.MissingCount(4, 6);
		if (missing != 0)
		{
			SetFailure("Wanting 4 and holding 6 answered %1, expected 0 - a negative would be read as a loop bound", missing.ToString());
			return true;
		}

		// A wanted count of 0 is a legitimate configuration, not an unset value.
		missing = OVT_DeploymentVirtualKey.MissingCount(0, 0);
		if (missing != 0)
		{
			SetFailure("Wanting 0 and holding 0 answered %1, expected 0", missing.ToString());
			return true;
		}

		// A negative wanted count cannot ask for anything.
		missing = OVT_DeploymentVirtualKey.MissingCount(-2, 0);
		if (missing != 0)
		{
			SetFailure("Wanting -2 answered %1, expected 0", missing.ToString());
			return true;
		}

		// A negative held count is nonsense arriving from somewhere else; it does not inflate the ask
		// beyond what was wanted.
		missing = OVT_DeploymentVirtualKey.MissingCount(3, -1);
		if (missing != 3)
		{
			SetFailure("Wanting 3 while holding -1 answered %1, expected 3", missing.ToString());
			return true;
		}

		Print("Missing count answers the shortfall, floors at 0 when held meets or beats wanted, and never returns a negative");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The defend plan: one point, nothing movable, and three arrays of the same length.
//!
//! This is the tower garrison's entire behaviour and the root of the "garrisons never wander"
//! claim. The plan IS the opt-in for being walked while dormant, so a garrison stays on its post
//! because its plan has nothing to advance along - not because of a flag somewhere.
//!
//! The equal-length claim looks like bookkeeping and is not: a ragged plan is refused outright at
//! registration, so a builder that appended to two arrays out of three would produce a garrison
//! that silently never registers at all.
//!
//! FAIL PROOF (edit recorded): drop the m_aParams.Insert() out of AddPoint and the equal-length
//! assertion goes red (1 position, 0 parameters). Change BuildDefendPlan's type to PATROL and the
//! type assertion goes red.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_DeploymentVirtualization_DefendPlan : SCR_AutotestCaseBase
{
	protected const float EPSILON = 0.01;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		vector post = Vector(4821, 12, 7093);

		OVT_VirtualWaypointPlan plan = OVT_VirtualPlanFactory.BuildDefendPlan(post, 25);
		if (!plan)
		{
			SetFailure("BuildDefendPlan answered nothing at all");
			return true;
		}

		if (plan.m_aPositions.Count() != 1)
		{
			SetFailure("A defend plan carried %1 positions, expected exactly 1", plan.m_aPositions.Count().ToString());
			return true;
		}

		// PARALLEL ARRAYS: a ragged plan is refused at registration, so the garrison would never exist.
		if (plan.m_aTypes.Count() != plan.m_aPositions.Count() || plan.m_aParams.Count() != plan.m_aPositions.Count())
		{
			string counts = plan.m_aPositions.Count().ToString() + "/" + plan.m_aTypes.Count().ToString() + "/" + plan.m_aParams.Count().ToString();
			SetFailure("A defend plan's three arrays are (positions/types/parameters) %1 - a ragged plan is refused outright", counts);
			return true;
		}

		if (plan.m_aTypes[0] != OVT_EVirtualWaypointType.DEFEND)
		{
			SetFailure("A defend plan's only point is type %1, expected DEFEND - anything movable would walk the garrison off its post", plan.m_aTypes[0].ToString());
			return true;
		}

		if (Math.AbsFloat(plan.m_aParams[0] - 25) > EPSILON)
		{
			SetFailure("A defend plan's parameter is %1, expected the requested radius of 25", plan.m_aParams[0].ToString());
			return true;
		}

		if (vector.Distance(plan.m_aPositions[0], post) > EPSILON)
		{
			SetFailure("A defend plan's point is at %1, expected the post at %2", plan.m_aPositions[0].ToString(), post.ToString());
			return true;
		}

		// NOT CYCLING. A cycling one-point plan would be a group re-arriving at its own feet forever.
		if (plan.m_bCycle)
		{
			SetFailure("A defend plan cycles - a garrison has nothing to loop around");
			return true;
		}

		Print("The defend plan is one DEFEND point at the post, non-cycling, with all three arrays the same length");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The perimeter plan: four corners 90 degrees apart, each with a pause on it, cycling forever.
//!
//! This is the shipped town patrol's geometry, and after the migration it is what a dormant patrol
//! actually walks - the player-visible half of the whole feature. Three properties have to hold or
//! the patrol is not a patrol: the corners are a real square around the centre at the requested
//! radius, the first one follows the group's OWN bearing (so two patrols on one town do not walk
//! the same line), and each corner carries its own pause so the group rests where it arrived.
//!
//! The cycle flag is the fourth: a perimeter patrol that stopped at its fourth corner would guard
//! one quarter of the town for the rest of the campaign.
//!
//! ⚠ Every distance below is 200-400 m - well away from the 1000 m and 2000 m magnitudes where
//! vector.Distance is a ULP out - and every comparison carries a tolerance.
//!
//! FAIL PROOF (edit recorded): drop the `+ (i * PERIMETER_STEP_DEGREES)` term and the corner-spacing
//! assertions go red (all four corners land on top of each other). Replace BearingBetween's result
//! with a constant 0 and the second-bearing assertion goes red (the first corner stops following the
//! group). Move the WAIT insert before the PATROL insert and the alternating-type assertion goes
//! red.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_DeploymentVirtualization_PerimeterPlan : SCR_AutotestCaseBase
{
	protected const float TOLERANCE_M = 0.5;
	protected const float EPSILON = 0.01;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		vector centre = Vector(500, 12, 500);
		vector fromPosition = Vector(500, 4, 300);	// due south of the centre: bearing 0
		float radius = 200;

		array<float> waits = {45, 50, 55, 60};

		OVT_VirtualWaypointPlan plan = OVT_VirtualPlanFactory.BuildPerimeterPlan(centre, fromPosition, radius, waits);
		if (!plan)
		{
			SetFailure("BuildPerimeterPlan answered nothing at all");
			return true;
		}

		// EIGHT entries: four corners, each followed by its own pause.
		if (plan.m_aPositions.Count() != 8)
		{
			SetFailure("A perimeter plan carried %1 positions, expected 8 (4 corners + 4 pauses)", plan.m_aPositions.Count().ToString());
			return true;
		}

		if (plan.m_aTypes.Count() != 8 || plan.m_aParams.Count() != 8)
		{
			string counts = plan.m_aPositions.Count().ToString() + "/" + plan.m_aTypes.Count().ToString() + "/" + plan.m_aParams.Count().ToString();
			SetFailure("A perimeter plan's three arrays are (positions/types/parameters) %1 - a ragged plan is refused outright", counts);
			return true;
		}

		// The four corners, in order, for a bearing of 0. Each is one radius from the centre and 90
		// degrees round from the last.
		array<vector> expected = {};
		expected.Insert(Vector(500, 12, 700));
		expected.Insert(Vector(700, 12, 500));
		expected.Insert(Vector(500, 12, 300));
		expected.Insert(Vector(300, 12, 500));

		for (int corner = 0; corner < 4; corner++)
		{
			int pointIndex = corner * 2;
			int waitIndex = pointIndex + 1;

			// TYPES ALTERNATE: corner, pause, corner, pause...
			if (plan.m_aTypes[pointIndex] != OVT_EVirtualWaypointType.PATROL)
			{
				SetFailure("Perimeter entry %1 is type %2, expected PATROL", pointIndex.ToString(), plan.m_aTypes[pointIndex].ToString());
				return true;
			}

			if (plan.m_aTypes[waitIndex] != OVT_EVirtualWaypointType.WAIT)
			{
				SetFailure("Perimeter entry %1 is type %2, expected WAIT", waitIndex.ToString(), plan.m_aTypes[waitIndex].ToString());
				return true;
			}

			// THE GEOMETRY: the requested radius, 90 degrees apart, starting on the group's bearing.
			float offBy = vector.Distance(plan.m_aPositions[pointIndex], expected[corner]);
			if (offBy > TOLERANCE_M)
			{
				string detail = plan.m_aPositions[pointIndex].ToString() + ", expected " + expected[corner].ToString();
				SetFailure("Perimeter corner %1 landed at %2 (out by %3 m)", corner.ToString(), detail, offBy.ToString());
				return true;
			}

			float fromCentre = vector.Distance(plan.m_aPositions[pointIndex], centre);
			if (Math.AbsFloat(fromCentre - radius) > TOLERANCE_M)
			{
				SetFailure("Perimeter corner %1 is %2 m from the centre, expected the requested 200 m", corner.ToString(), fromCentre.ToString());
				return true;
			}

			// THE PAUSE SITS ON THE CORNER, not somewhere between corners.
			float pauseOffset = vector.Distance(plan.m_aPositions[waitIndex], plan.m_aPositions[pointIndex]);
			if (pauseOffset > EPSILON)
			{
				SetFailure("The pause after corner %1 is %2 m away from it, expected the same position", corner.ToString(), pauseOffset.ToString());
				return true;
			}

			// ...and carries the duration it was given, in order.
			if (Math.AbsFloat(plan.m_aParams[waitIndex] - waits[corner]) > EPSILON)
			{
				string durations = plan.m_aParams[waitIndex].ToString() + ", expected " + waits[corner].ToString();
				SetFailure("The pause after corner %1 lasts %2 s", corner.ToString(), durations);
				return true;
			}

			// The corner itself carries no completion radius, matching the hand-authored waypoints.
			if (Math.AbsFloat(plan.m_aParams[pointIndex]) > EPSILON)
			{
				SetFailure("Perimeter corner %1 carries a parameter of %2, expected 0", corner.ToString(), plan.m_aParams[pointIndex].ToString());
				return true;
			}
		}

		// Opposite corners are a full diameter apart - the square is a square, not a fan.
		float across = vector.Distance(plan.m_aPositions[0], plan.m_aPositions[4]);
		if (Math.AbsFloat(across - (radius * 2)) > TOLERANCE_M)
		{
			SetFailure("Opposite corners are %1 m apart, expected a 400 m diameter", across.ToString());
			return true;
		}

		// IT CYCLES, or the patrol guards one quarter of the town forever.
		if (!plan.m_bCycle)
		{
			SetFailure("A perimeter plan does not cycle - the patrol would stop at its fourth corner");
			return true;
		}

		// THE BEARING FOLLOWS THE GROUP. Same centre, same radius, a group standing due WEST instead:
		// the first corner moves round with it rather than always pointing north.
		vector fromWest = Vector(300, 4, 500);
		OVT_VirtualWaypointPlan rotated = OVT_VirtualPlanFactory.BuildPerimeterPlan(centre, fromWest, radius, waits);

		float rotatedOffBy = vector.Distance(rotated.m_aPositions[0], Vector(700, 12, 500));
		if (rotatedOffBy > TOLERANCE_M)
		{
			string detail = rotated.m_aPositions[0].ToString() + ", expected (700, 12, 500)";
			SetFailure("A group approaching from the west started its patrol at %1 (out by %2 m)", detail, rotatedOffBy.ToString());
			return true;
		}

		// A degenerate call - the group standing exactly on the centre - still builds a legal plan.
		OVT_VirtualWaypointPlan degenerate = OVT_VirtualPlanFactory.BuildPerimeterPlan(centre, centre, radius, waits);
		if (!degenerate || degenerate.m_aPositions.Count() != 8 || degenerate.m_aTypes.Count() != 8 || degenerate.m_aParams.Count() != 8)
		{
			SetFailure("A group standing on its own patrol centre did not produce a legal 8-entry plan");
			return true;
		}

		// A missing duration list is not an error either: the corners just carry no pause.
		OVT_VirtualWaypointPlan unpaused = OVT_VirtualPlanFactory.BuildPerimeterPlan(centre, fromPosition, radius, null);
		if (!unpaused || unpaused.m_aParams.Count() != 8)
		{
			SetFailure("A perimeter plan with no durations supplied did not produce 8 parameters");
			return true;
		}

		if (Math.AbsFloat(unpaused.m_aParams[1]) > EPSILON)
		{
			SetFailure("A perimeter plan with no durations supplied paused for %1 s, expected 0", unpaused.m_aParams[1].ToString());
			return true;
		}

		Print("The perimeter plan is 4 corners at the requested radius, 90 degrees apart, starting on the group's own bearing, each followed by its own pause, and it cycles");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! THE AUTHORED SQUARE: four corners at an authored ROTATION rather than on the walker's own bearing.
//!
//! Added by amendment A1 (2026-08-18). The plan this builds is what a base garrison walks, and it is
//! the ONE thing about that garrison a designer controls directly - two numbers authored on the base,
//! a radius and a rotation. Everything that can go wrong with it is silent in play: a rotation that is
//! quietly ignored looks exactly like a rotation that was never authored, and corners that stop being
//! 90 degrees apart look like a patrol that is merely wandering.
//!
//! FIVE CLAIMS:
//!   1. THE SHAPE. Eight entries, PATROL and WAIT alternating, the pause sitting on its own corner and
//!      carrying its own duration, no completion radius on the corners, and it CYCLES - identical to
//!      the ring builder, because everything downstream has to treat the two the same.
//!   2. THE ROTATION IS OBEYED. At rotation 0 the corners are due north/east/south/west of the centre;
//!      at rotation 45 they are on the diagonals. Same centre, same radius, different points.
//!   3. THE SQUARE IS A SQUARE. Every corner is exactly `radius` from the centre, adjacent corners are
//!      radius*sqrt(2) apart and opposite corners a full diameter - which is what stops a broken
//!      rotation term collapsing four corners into a fan.
//!   4. WHICH CORNER COMES FIRST STILL FOLLOWS THE WALKER. The square does not move, but a group
//!      approaching from the west starts at the western corner rather than always at corner 0, so two
//!      garrisons on one square do not walk it in lockstep.
//!   5. A NEGATIVE ROTATION IS THE SAME AS ITS POSITIVE TWIN. -90 and 270 must answer the same four
//!      points, or an authored negative angle folds to a corner index off the end of the square.
//!
//! Every distance below is 200-400 m, well away from the 1000 m and 2000 m magnitudes where
//! vector.Distance is a ULP out, and every comparison carries a tolerance.
//!
//! FAIL PROOF (edits recorded, execution belongs to the phase's suite run): drop `rotationDeg` from
//! the corner expression in BuildSquarePerimeterPlan and claim 2 goes red (rotation 45 answers
//! rotation 0's points); drop the `* PERIMETER_STEP_DEGREES` term and claim 3 goes red (all four
//! corners land on top of each other); make StartCornerIndex return a constant 0 and claim 4 goes red;
//! delete the `if (wrapped < 0)` fold in NormalizeDegrees and claim 5 goes red.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_DeploymentVirtualization_SquarePerimeterPlan : SCR_AutotestCaseBase
{
	protected const float TOLERANCE_M = 0.5;
	protected const float EPSILON = 0.01;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		vector centre = Vector(500, 12, 500);
		vector fromSouth = Vector(500, 4, 300);	// bearing 0 towards the centre
		float radius = 200;

		array<float> waits = {45, 50, 55, 60};

		OVT_VirtualWaypointPlan plan = OVT_VirtualPlanFactory.BuildSquarePerimeterPlan(centre, fromSouth, radius, 0, waits);
		if (!plan)
		{
			SetFailure("BuildSquarePerimeterPlan answered nothing at all");
			return true;
		}

		// CLAIM 1 - the shape, and it must be the ring builder's shape exactly.
		if (plan.m_aPositions.Count() != 8 || plan.m_aTypes.Count() != 8 || plan.m_aParams.Count() != 8)
		{
			string counts = plan.m_aPositions.Count().ToString() + "/" + plan.m_aTypes.Count().ToString() + "/" + plan.m_aParams.Count().ToString();
			SetFailure("A square perimeter plan's three arrays are (positions/types/parameters) %1, expected 8/8/8 - a ragged plan is refused outright and 4 corners + 4 pauses is 8", counts);
			return true;
		}

		// CLAIM 2, first half - rotation 0 puts corner 0 due north of the centre, and the rest follow
		// clockwise, which is the same four points the bearing-anchored ring builds for this walker.
		array<vector> expected = {};
		expected.Insert(Vector(500, 12, 700));
		expected.Insert(Vector(700, 12, 500));
		expected.Insert(Vector(500, 12, 300));
		expected.Insert(Vector(300, 12, 500));

		string shapeFailure = VerifyCorners(plan, centre, radius, expected, waits, "rotation 0");
		if (shapeFailure != "")
		{
			SetFailure(shapeFailure);
			return true;
		}

		if (!plan.m_bCycle)
		{
			SetFailure("A square perimeter plan does not cycle - the garrison would stop at its fourth corner and guard that quarter of the base for the rest of the campaign");
			return true;
		}

		// CLAIM 2, second half - 45 degrees puts the same square on the diagonals. sin(45)*200 = 141.42.
		//
		// ⚠ THE WALKER IS MOVED ONTO THE DIAGONAL TOO, and that is not decoration. A walker whose bearing
		// falls exactly HALFWAY between two corners (which bearing 0 does against a square rotated 45) is
		// the one input where the start-corner rounding is on a knife edge, and pinning a walk ORDER
		// against it would be pinning a tie-break rather than the geometry. Approaching from the
		// south-west puts the bearing ON corner 0, which answers 0 whichever way the rounding leans.
		float diagonal = 141.42;
		vector fromSouthWest = Vector(500 - 212.13, 4, 500 - 212.13);	// bearing 45 towards the centre
		OVT_VirtualWaypointPlan turned = OVT_VirtualPlanFactory.BuildSquarePerimeterPlan(centre, fromSouthWest, radius, 45, waits);

		array<vector> expectedTurned = {};
		expectedTurned.Insert(Vector(500 + diagonal, 12, 500 + diagonal));
		expectedTurned.Insert(Vector(500 + diagonal, 12, 500 - diagonal));
		expectedTurned.Insert(Vector(500 - diagonal, 12, 500 - diagonal));
		expectedTurned.Insert(Vector(500 - diagonal, 12, 500 + diagonal));

		string turnedFailure = VerifyCorners(turned, centre, radius, expectedTurned, waits, "rotation 45");
		if (turnedFailure != "")
		{
			SetFailure(turnedFailure);
			return true;
		}

		// ...and the two really are different points, which is the assertion a silently-ignored rotation
		// argument would break while every other claim above still passed.
		float moved = vector.Distance(plan.m_aPositions[0], turned.m_aPositions[0]);
		if (moved < TOLERANCE_M)
		{
			SetFailure("Rotating the authored square by 45 degrees moved its first corner %1 m - the rotation argument is being ignored", moved.ToString());
			return true;
		}

		// CLAIM 4 - the square stays put; only the corner the walker starts at moves. A group standing
		// due west starts on the western corner (index 1 of the rotation-0 square), not on corner 0.
		vector fromWest = Vector(300, 4, 500);
		OVT_VirtualWaypointPlan westward = OVT_VirtualPlanFactory.BuildSquarePerimeterPlan(centre, fromWest, radius, 0, waits);

		float startOffBy = vector.Distance(westward.m_aPositions[0], Vector(700, 12, 500));
		if (startOffBy > TOLERANCE_M)
		{
			string detail = westward.m_aPositions[0].ToString() + ", expected (700, 12, 500)";
			SetFailure("A group approaching the authored square from the west started at %1 (out by %2 m) - the start corner no longer follows the walker", detail, startOffBy.ToString());
			return true;
		}

		// It walks the SAME four points, only beginning one corner along: its second point is the
		// rotation-0 square's third corner.
		float secondOffBy = vector.Distance(westward.m_aPositions[2], Vector(500, 12, 300));
		if (secondOffBy > TOLERANCE_M)
		{
			SetFailure("The westward walk's second corner is %1 m from the authored square's third corner - the square itself moved with the walker, which is exactly what an AUTHORED square must not do", secondOffBy.ToString());
			return true;
		}

		// CLAIM 5 - a negative authored rotation folds, rather than reading off the end of the square.
		OVT_VirtualWaypointPlan negative = OVT_VirtualPlanFactory.BuildSquarePerimeterPlan(centre, fromSouth, radius, -90, waits);
		OVT_VirtualWaypointPlan positive = OVT_VirtualPlanFactory.BuildSquarePerimeterPlan(centre, fromSouth, radius, 270, waits);

		for (int i = 0; i < 8; i++)
		{
			float foldOffBy = vector.Distance(negative.m_aPositions[i], positive.m_aPositions[i]);
			if (foldOffBy > TOLERANCE_M)
			{
				SetFailure("Point %1 of a square authored at -90 degrees is %2 m from the same point authored at 270 - a negative rotation is not folding into the square's four corners", i.ToString(), foldOffBy.ToString());
				return true;
			}
		}

		// Defensive inputs, both of which a live caller can produce: a walker standing exactly on the
		// centre has no bearing at all, and a caller that does not care about pausing passes nothing.
		OVT_VirtualWaypointPlan degenerate = OVT_VirtualPlanFactory.BuildSquarePerimeterPlan(centre, centre, radius, 30, waits);
		if (!degenerate || degenerate.m_aPositions.Count() != 8 || degenerate.m_aTypes.Count() != 8 || degenerate.m_aParams.Count() != 8)
		{
			SetFailure("A walker standing on its own square's centre did not produce a legal 8-entry plan");
			return true;
		}

		OVT_VirtualWaypointPlan unpaused = OVT_VirtualPlanFactory.BuildSquarePerimeterPlan(centre, fromSouth, radius, 0, null);
		if (!unpaused || unpaused.m_aParams.Count() != 8)
		{
			SetFailure("A square perimeter plan with no durations supplied did not produce 8 parameters");
			return true;
		}

		if (Math.AbsFloat(unpaused.m_aParams[1]) > EPSILON)
		{
			SetFailure("A square perimeter plan with no durations supplied paused for %1 s, expected 0", unpaused.m_aParams[1].ToString());
			return true;
		}

		Print("The authored square is 4 corners at the authored rotation and radius, 90 degrees apart, each followed by its own pause; the rotation is obeyed, negative rotations fold, and only the START corner follows the walker");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Claims 1 and 3 on one plan, plus the expected corner positions.
	//! \param[in] plan The plan to check.
	//! \param[in] centre What it circles.
	//! \param[in] radius The requested radius.
	//! \param[in] expected The four corner positions, in walk order.
	//! \param[in] waits The durations that were requested, in walk order.
	//! \param[in] label What to call this plan in a message.
	//! \return An empty string when every claim holds, or the first broken one.
	protected string VerifyCorners(notnull OVT_VirtualWaypointPlan plan, vector centre, float radius, notnull array<vector> expected, notnull array<float> waits, string label)
	{
		for (int corner = 0; corner < 4; corner++)
		{
			int pointIndex = corner * 2;
			int waitIndex = pointIndex + 1;

			if (plan.m_aTypes[pointIndex] != OVT_EVirtualWaypointType.PATROL)
				return string.Format("%1: entry %2 is type %3, expected PATROL", label, pointIndex.ToString(), plan.m_aTypes[pointIndex].ToString());

			if (plan.m_aTypes[waitIndex] != OVT_EVirtualWaypointType.WAIT)
				return string.Format("%1: entry %2 is type %3, expected WAIT", label, waitIndex.ToString(), plan.m_aTypes[waitIndex].ToString());

			float offBy = vector.Distance(plan.m_aPositions[pointIndex], expected[corner]);
			if (offBy > TOLERANCE_M)
			{
				string detail = plan.m_aPositions[pointIndex].ToString() + ", expected " + expected[corner].ToString() + ", out by " + offBy.ToString() + " m";
				return string.Format("%1: corner %2 landed at %3", label, corner.ToString(), detail);
			}

			float fromCentre = vector.Distance(plan.m_aPositions[pointIndex], centre);
			if (Math.AbsFloat(fromCentre - radius) > TOLERANCE_M)
				return string.Format("%1: corner %2 is %3 m from the centre, expected the authored radius", label, corner.ToString(), fromCentre.ToString());

			float pauseOffset = vector.Distance(plan.m_aPositions[waitIndex], plan.m_aPositions[pointIndex]);
			if (pauseOffset > EPSILON)
				return string.Format("%1: the pause after corner %2 is %3 m away from it, expected the same position", label, corner.ToString(), pauseOffset.ToString());

			if (Math.AbsFloat(plan.m_aParams[waitIndex] - waits[corner]) > EPSILON)
			{
				string durations = plan.m_aParams[waitIndex].ToString() + ", expected " + waits[corner].ToString();
				return string.Format("%1: the pause after corner %2 lasts %3 s", label, corner.ToString(), durations);
			}

			if (Math.AbsFloat(plan.m_aParams[pointIndex]) > EPSILON)
				return string.Format("%1: corner %2 carries a parameter of %3, expected 0 - a corner takes the waypoint prefab's own completion radius", label, corner.ToString(), plan.m_aParams[pointIndex].ToString());
		}

		// CLAIM 3 - adjacent corners are radius*sqrt(2) apart and opposite corners a full diameter, so a
		// broken rotation term cannot collapse the square into a fan and still pass.
		float adjacent = vector.Distance(plan.m_aPositions[0], plan.m_aPositions[2]);
		float expectedAdjacent = radius * Math.Sqrt(2);
		if (Math.AbsFloat(adjacent - expectedAdjacent) > TOLERANCE_M)
			return string.Format("%1: two adjacent corners are %2 m apart, expected %3 - the corners are not 90 degrees apart", label, adjacent.ToString(), expectedAdjacent.ToString());

		float across = vector.Distance(plan.m_aPositions[0], plan.m_aPositions[4]);
		if (Math.AbsFloat(across - (radius * 2)) > TOLERANCE_M)
			return string.Format("%1: opposite corners are %2 m apart, expected a full diameter of %3", label, across.ToString(), (radius * 2).ToString());

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! The route plan: a stop and a pause per town, and exactly one way to avoid ending in a dead stop.
//!
//! Returning to the start and cycling are mutually exclusive by construction, which is the claim
//! worth pinning: a route that comes home has a natural end and must not also loop back onto its
//! first stop, and a route that does not come home has to loop or it stops for good at its last
//! town. Both shipped vehicle configs return to the start, so the non-cycling branch is the live
//! one and the cycling branch is the one a new config would reach.
//!
//! The empty-route case is the defensive half. A caller with no stops must get an EMPTY plan - which
//! is legal, and registers a group with no waypoints - and not a lone closing point that would send
//! a crew back to a start it never left.
//!
//! FAIL PROOF (edit recorded): move the closing MOVE outside the `if (returnToStart)` and the
//! non-returning entry-count assertion goes red (5 instead of 4). Delete the empty-stops early
//! return and the empty-route assertion goes red (1 position instead of 0).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_DeploymentVirtualization_RoutePlan : SCR_AutotestCaseBase
{
	protected const float TOLERANCE_M = 0.5;
	protected const float EPSILON = 0.01;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		vector start = Vector(100, 5, 100);

		array<vector> stops = {};
		stops.Insert(Vector(400, 8, 100));
		stops.Insert(Vector(400, 9, 400));

		// RETURNING: MOVE, WAIT, MOVE, WAIT, MOVE-home. Five entries, no cycle.
		OVT_VirtualWaypointPlan returning = OVT_VirtualPlanFactory.BuildRoutePlan(stops, 60, true, start);
		if (!returning)
		{
			SetFailure("BuildRoutePlan answered nothing at all");
			return true;
		}

		if (returning.m_aPositions.Count() != 5)
		{
			SetFailure("A 2-stop returning route carried %1 positions, expected 5 (2 stops x 2 + 1 closing move)", returning.m_aPositions.Count().ToString());
			return true;
		}

		if (returning.m_aTypes.Count() != 5 || returning.m_aParams.Count() != 5)
		{
			string counts = returning.m_aPositions.Count().ToString() + "/" + returning.m_aTypes.Count().ToString() + "/" + returning.m_aParams.Count().ToString();
			SetFailure("A returning route's three arrays are (positions/types/parameters) %1 - a ragged plan is refused outright", counts);
			return true;
		}

		if (returning.m_aTypes[0] != OVT_EVirtualWaypointType.MOVE || returning.m_aTypes[2] != OVT_EVirtualWaypointType.MOVE)
		{
			string types = returning.m_aTypes[0].ToString() + " and " + returning.m_aTypes[2].ToString();
			SetFailure("A route's stops are types %1, expected MOVE", types);
			return true;
		}

		if (returning.m_aTypes[1] != OVT_EVirtualWaypointType.WAIT || returning.m_aTypes[3] != OVT_EVirtualWaypointType.WAIT)
		{
			string types = returning.m_aTypes[1].ToString() + " and " + returning.m_aTypes[3].ToString();
			SetFailure("A route's pauses are types %1, expected WAIT", types);
			return true;
		}

		// Each pause sits on the stop it belongs to and lasts as long as it was told to.
		if (vector.Distance(returning.m_aPositions[1], stops[0]) > EPSILON)
		{
			SetFailure("The pause at the first stop is at %1, expected the stop itself", returning.m_aPositions[1].ToString());
			return true;
		}

		if (Math.AbsFloat(returning.m_aParams[1] - 60) > EPSILON)
		{
			SetFailure("The pause at the first stop lasts %1 s, expected 60", returning.m_aParams[1].ToString());
			return true;
		}

		// The closing move goes home, and it is a MOVE rather than a pause.
		if (returning.m_aTypes[4] != OVT_EVirtualWaypointType.MOVE)
		{
			SetFailure("The closing entry is type %1, expected MOVE", returning.m_aTypes[4].ToString());
			return true;
		}

		if (vector.Distance(returning.m_aPositions[4], start) > TOLERANCE_M)
		{
			string detail = returning.m_aPositions[4].ToString() + ", expected " + start.ToString();
			SetFailure("The closing move goes to %1", detail);
			return true;
		}

		// A route that comes home must NOT also loop back onto its first stop.
		if (returning.m_bCycle)
		{
			SetFailure("A returning route cycles as well - it would loop from home back to its first stop forever");
			return true;
		}

		// NOT RETURNING: the same stops, no closing move, and it loops so it does not stop for good.
		OVT_VirtualWaypointPlan looping = OVT_VirtualPlanFactory.BuildRoutePlan(stops, 60, false, start);
		if (looping.m_aPositions.Count() != 4)
		{
			SetFailure("A 2-stop looping route carried %1 positions, expected 4 - nothing is appended when it does not come home", looping.m_aPositions.Count().ToString());
			return true;
		}

		if (looping.m_aTypes.Count() != 4 || looping.m_aParams.Count() != 4)
		{
			string counts = looping.m_aPositions.Count().ToString() + "/" + looping.m_aTypes.Count().ToString() + "/" + looping.m_aParams.Count().ToString();
			SetFailure("A looping route's three arrays are (positions/types/parameters) %1", counts);
			return true;
		}

		if (!looping.m_bCycle)
		{
			SetFailure("A route that does not come home does not cycle either - it would stop at its last town for good");
			return true;
		}

		// EMPTY ROUTE: an empty, legal plan - not a lone closing point back to a start never left.
		OVT_VirtualWaypointPlan nowhere = OVT_VirtualPlanFactory.BuildRoutePlan(null, 60, true, start);
		if (!nowhere)
		{
			SetFailure("A route with a null stop list answered nothing at all, expected an empty plan");
			return true;
		}

		if (!nowhere.m_aPositions.IsEmpty() || !nowhere.m_aTypes.IsEmpty() || !nowhere.m_aParams.IsEmpty())
		{
			string counts = nowhere.m_aPositions.Count().ToString() + "/" + nowhere.m_aTypes.Count().ToString() + "/" + nowhere.m_aParams.Count().ToString();
			SetFailure("A route with a null stop list carried (positions/types/parameters) %1, expected an empty plan", counts);
			return true;
		}

		array<vector> noStops = {};
		OVT_VirtualWaypointPlan alsoNowhere = OVT_VirtualPlanFactory.BuildRoutePlan(noStops, 60, false, start);
		if (!alsoNowhere.m_aPositions.IsEmpty() || !alsoNowhere.m_aTypes.IsEmpty() || !alsoNowhere.m_aParams.IsEmpty())
		{
			string counts = alsoNowhere.m_aPositions.Count().ToString() + "/" + alsoNowhere.m_aTypes.Count().ToString() + "/" + alsoNowhere.m_aParams.Count().ToString();
			SetFailure("A route with an empty stop list carried (positions/types/parameters) %1, expected an empty plan", counts);
			return true;
		}

		// One stop is the smallest real route, and still gets its pause.
		array<vector> oneStop = {};
		oneStop.Insert(Vector(400, 8, 100));

		OVT_VirtualWaypointPlan single = OVT_VirtualPlanFactory.BuildRoutePlan(oneStop, 30, true, start);
		if (single.m_aPositions.Count() != 3)
		{
			SetFailure("A 1-stop returning route carried %1 positions, expected 3", single.m_aPositions.Count().ToString());
			return true;
		}

		if (Math.AbsFloat(single.m_aParams[1] - 30) > EPSILON)
		{
			SetFailure("A 1-stop route's pause lasts %1 s, expected 30", single.m_aParams[1].ToString());
			return true;
		}

		Print("The route plan is MOVE+WAIT per stop, appends a closing MOVE only when it comes home, cycles only when it does not, and answers an empty legal plan for an empty route");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! BuildSearchPlan: one SEARCH entry per site, in the order given, each carrying its own hold, cycling.
//!
//! This is the town sweep's house route, and the four things that make it a sweep rather than a
//! garrison are pinned separately: the point count is the site count (no WAIT is appended after a
//! SEARCH - the hold IS the pause, a trailing WAIT would stand the men outside the house they just
//! searched); every entry is SEARCH, which core maps onto the Search & Destroy prefab and movement
//! treats as a WAIT; the parameters are the holds, by index; and the plan cycles, or the last house
//! becomes somebody's permanent doorstep guard. An empty site list answers an empty, LEGAL plan.
//!
//! FAIL PROOF (edit recorded): add a WAIT after each SEARCH in BuildSearchPlan and the count assertion
//! goes red (6 for 3). Insert PATROL instead of SEARCH and the type assertion goes red on entry 0.
//! Drop `plan.m_bCycle = true` and the cycle assertion goes red.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_DeploymentVirtualization_SearchPlan : SCR_AutotestCaseBase
{
	protected const float EPSILON = 0.01;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<vector> sites = {};
		sites.Insert(Vector(100, 5, 100));
		sites.Insert(Vector(160, 5, 120));
		sites.Insert(Vector(140, 5, 180));

		array<float> holds = {60, 90, 120};

		OVT_VirtualWaypointPlan plan = OVT_VirtualPlanFactory.BuildSearchPlan(sites, holds);
		if (!plan)
		{
			SetFailure("BuildSearchPlan answered nothing at all");
			return true;
		}

		if (plan.m_aPositions.Count() != 3 || plan.m_aTypes.Count() != 3 || plan.m_aParams.Count() != 3)
		{
			string counts = plan.m_aPositions.Count().ToString() + "/" + plan.m_aTypes.Count().ToString() + "/" + plan.m_aParams.Count().ToString();
			SetFailure("A 3-site search plan carried (positions/types/parameters) %1, expected 3/3/3 - one SEARCH per site and nothing else", counts);
			return true;
		}

		for (int i = 0; i < 3; i++)
		{
			if (plan.m_aTypes[i] != OVT_EVirtualWaypointType.SEARCH)
			{
				SetFailure("Search plan entry %1 is type %2, expected SEARCH", i.ToString(), plan.m_aTypes[i].ToString());
				return true;
			}

			if (vector.Distance(plan.m_aPositions[i], sites[i]) > EPSILON)
			{
				SetFailure("Search plan entry %1 sits at %2, expected the site it was given (%3) - the factory must not reorder, the caller owns the route", i.ToString(), plan.m_aPositions[i].ToString(), sites[i].ToString());
				return true;
			}

			if (Math.AbsFloat(plan.m_aParams[i] - holds[i]) > EPSILON)
			{
				SetFailure("Search plan entry %1 holds for %2 s, expected %3", i.ToString(), plan.m_aParams[i].ToString(), holds[i].ToString());
				return true;
			}
		}

		if (!plan.m_bCycle)
		{
			SetFailure("A search plan does not cycle - the patrol would search its last house and guard its doorstep for the rest of the campaign");
			return true;
		}

		// A short hold list holds for 0 on the missing entries rather than refusing the plan.
		array<float> oneHold = {45};
		OVT_VirtualWaypointPlan shortHolds = OVT_VirtualPlanFactory.BuildSearchPlan(sites, oneHold);
		if (shortHolds.m_aParams.Count() != 3 || Math.AbsFloat(shortHolds.m_aParams[0] - 45) > EPSILON || shortHolds.m_aParams[2] != 0)
		{
			SetFailure("A search plan with 3 sites and 1 hold did not pad the missing holds with 0");
			return true;
		}

		// No sites is a legal, empty plan - the caller falls back to the ring, the factory does not refuse.
		array<vector> noSites = {};
		OVT_VirtualWaypointPlan nowhere = OVT_VirtualPlanFactory.BuildSearchPlan(noSites, holds);
		if (!nowhere || !nowhere.m_aPositions.IsEmpty() || !nowhere.m_aTypes.IsEmpty() || !nowhere.m_aParams.IsEmpty())
		{
			SetFailure("A search plan with no sites was not an empty plan");
			return true;
		}

		OVT_VirtualWaypointPlan nullSites = OVT_VirtualPlanFactory.BuildSearchPlan(null, holds);
		if (!nullSites || !nullSites.m_aPositions.IsEmpty())
		{
			SetFailure("A search plan with a null site list was not an empty plan");
			return true;
		}

		Print("The search plan is one SEARCH per site in the order given, holds by index padded with 0, cycling, and empty for no sites");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! OrderNearestNeighbour: from the start, always the nearest unvisited site next; pure; empty for empty.
//!
//! The sites are laid out so that INPUT order and DISTANCE order disagree at every step, which is the
//! only way the assertion can tell a nearest-neighbour walk from "returned them as given": the start
//! is at the origin, the input lists the far site first, and the correct walk is far-input-last.
//! A second check proves the input array is untouched (it is COPIED, not consumed), and the
//! null/empty inputs answer an empty array rather than null.
//!
//! FAIL PROOF (edit recorded): return `sites` unchanged and the first ordering assertion goes red
//! (entry 0 is the 300 m site). Use `>` instead of `<` in the nearest comparison and it goes red the
//! other way round (farthest-first).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_DeploymentVirtualization_NearestNeighbourRoute : SCR_AutotestCaseBase
{
	protected const float EPSILON = 0.01;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		vector start = Vector(0, 0, 0);

		// Input order: 300 m, 100 m, 200 m along +X - deliberately not by distance.
		array<vector> sites = {};
		sites.Insert(Vector(300, 0, 0));
		sites.Insert(Vector(100, 0, 0));
		sites.Insert(Vector(200, 0, 0));

		array<vector> route = OVT_VirtualPlanFactory.OrderNearestNeighbour(start, sites);
		if (!route)
		{
			SetFailure("OrderNearestNeighbour answered null");
			return true;
		}

		if (route.Count() != 3)
		{
			SetFailure("A 3-site route came back with %1 entries", route.Count().ToString());
			return true;
		}

		array<vector> expected = {};
		expected.Insert(Vector(100, 0, 0));
		expected.Insert(Vector(200, 0, 0));
		expected.Insert(Vector(300, 0, 0));

		for (int i = 0; i < 3; i++)
		{
			if (vector.Distance(route[i], expected[i]) > EPSILON)
			{
				SetFailure("Route entry %1 is %2, expected %3 - the walk is not nearest-unvisited-next from the start", i.ToString(), route[i].ToString(), expected[i].ToString());
				return true;
			}
		}

		// The input is untouched.
		if (sites.Count() != 3 || vector.Distance(sites[0], Vector(300, 0, 0)) > EPSILON)
		{
			SetFailure("OrderNearestNeighbour mutated its input array");
			return true;
		}

		// A start on the FAR side flips the walk - the order follows the start, not the data.
		array<vector> reversed = OVT_VirtualPlanFactory.OrderNearestNeighbour(Vector(400, 0, 0), sites);
		if (reversed.Count() != 3 || vector.Distance(reversed[0], Vector(300, 0, 0)) > EPSILON || vector.Distance(reversed[2], Vector(100, 0, 0)) > EPSILON)
		{
			SetFailure("A route started from the far end did not walk back the other way");
			return true;
		}

		array<vector> none = {};
		array<vector> emptyRoute = OVT_VirtualPlanFactory.OrderNearestNeighbour(start, none);
		if (!emptyRoute || !emptyRoute.IsEmpty())
		{
			SetFailure("An empty site list did not answer an empty route");
			return true;
		}

		array<vector> nullRoute = OVT_VirtualPlanFactory.OrderNearestNeighbour(start, null);
		if (!nullRoute || !nullRoute.IsEmpty())
		{
			SetFailure("A null site list did not answer an empty route");
			return true;
		}

		Print("OrderNearestNeighbour walks nearest-unvisited-next from the start, follows the start not the input, leaves the input alone, and answers empty for empty");
		return true;
	}
}
