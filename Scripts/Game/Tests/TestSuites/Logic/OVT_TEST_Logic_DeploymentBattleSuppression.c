//------------------------------------------------------------------------------------------------
//! TIER A cases - the rule that decides whether a deployment's force may materialise while a battle
//! is being fought (OVT_DeploymentBattleSuppression).
//!
//! Every subject here is a static function of plain values. Nothing in this file resolves a manager,
//! a controller or a world, and nothing needs to. See OVT_TEST_LogicSuite.c for the tier rule and the
//! house rules - including the reviewer grep over this directory, which does not distinguish code from
//! comments, so neither banned identifier appears anywhere below, prose included.
//!
//! WHY THIS FILE EXISTS. Two shipped features stated opposite intentions about the same ground: the
//! battle system says the contested place loses its defenders by design, and the deployment framework
//! says an existing force keeps being maintained wherever it is. A play-test found the seam - guards
//! materialising at a contested base mid-battle - and the resolution was a narrow one: suppress only
//! inside the battle's own circle. Every part of that resolution that can be decided without a world
//! lives in the subject below, because each half of it fails SILENTLY when it is wrong:
//!
//!  - SUPPRESSING WHEN NOTHING IS ENGAGED empties the objective of its garrison during the silent
//!    stages of a counter-attack siege. That is not an error, it is a TELL: the one thing the silent
//!    stages exist to avoid, visible to the player as a place that mysteriously emptied.
//!  - SUPPRESSING THE WRONG FACTION nerfs the forces the player paid for and committed, mid-battle,
//!    in a battle whose scoring deliberately counts them. Nothing logs it and nothing looks broken.
//!  - GETTING THE RADIUS WRONG IN THE WIDE DIRECTION freezes deployments across the map for the length
//!    of a battle, which is exactly the whole-map behaviour this change was written NOT to reintroduce.
//!  - AN UNRESOLVABLE FACTION KEY MATCHING ANOTHER EMPTY ONE would suppress a nameless force on the
//!    strength of two blanks being equal.
//!
//! ⚠ THE EXACT RING IS DELIBERATELY NOT ASSERTED, AND THAT IS A MEASUREMENT DECISION, NOT AN OVERSIGHT.
//! vector.Distance is not correctly rounded - it has been measured a full ULP high at 1 000 m and at
//! 2 000 m - so a case that planted a point at EXACTLY the radius would be asserting which side of a
//! floating-point rounding error the engine happens to land on that build, not which side of the rule
//! the position is on. The boundary is pinned one metre either side instead, which is the claim that
//! actually matters ("just inside is in, just outside is out") and cannot flake. Whoever probes
//! vector.Distance at 750 m may tighten this.
//!
//! ⚠ THE MEASURE IS 3D, on purpose: it is the same vector.Distance the battle's own scoring, the
//! fast-travel veto and the respawn veto all use against the same constant, and a 2D copy here would
//! give the player a suppression circle that did not line up with the circle on their map. At terrain
//! scale the altitude term is immaterial at 750 m; every row below is horizontal.
//!
//! CAN-FAIL PROOFS. Running a suite is the orchestrator's job, not an implementation agent's
//! (.claude/test-policy.md), so each proof below is a fault that was injected into the subject one at
//! a time and compiled - every one exited tools/compile-check.sh with 0, which is the point: none of
//! them is a syntax error and nothing else in the tree would stop it reaching players. The subject was
//! restored and recompiled clean afterwards. The resulting failure text is recorded per case.
//!
//! No maxAttempts anywhere: the subject is a pure function with no clock, no randomness and no world,
//! and cannot flake.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! OVT_DeploymentBattleSuppression - three conjuncts, and each one of them is load-bearing on its own.
//!
//! Six claims, in the order a reader of the rule meets them:
//!   1. 🔴 NOTHING IS SUPPRESSED WHILE NOTHING IS ENGAGED, not even the occupying faction's own force
//!      standing on the battle's exact coordinates. This is the regression guard for the silent stages
//!      of a counter-attack: a battle OBJECT can exist for half an hour before a shot is fired, and the
//!      world has to keep living for all of it.
//!   2. AN ENGAGED BATTLE SUPPRESSES THE OCCUPYING FACTION'S FORCE INSIDE THE CIRCLE - on the battle
//!      itself and one metre inside the ring.
//!   3. IT SUPPRESSES NOTHING OUTSIDE THE CIRCLE - one metre outside the ring, and far away. The
//!      "far away" row is the one that would catch a rule that had quietly become global again.
//!   4. THE RESISTANCE IS NEVER SUPPRESSED, standing directly on an engaged battle. The battle's own
//!      scoring counts those men for the resistance; holding them back would be scoring the player
//!      down for forces the rule had just prevented from arriving.
//!   5. A THIRD FACTION IS NEVER SUPPRESSED EITHER - the test is "is this the occupying faction", not
//!      "is this anyone but the resistance".
//!   6. AN EMPTY OCCUPYING KEY SUPPRESSES NOTHING, including against an empty force key. "We could not
//!      work out who the occupying faction is" must never be read as "this nameless force is theirs".
//!
//! ⚠ THE RADIUS IS READ FROM THE BATTLE COMPONENT'S OWN CONSTANT, never written as 750 here. A case
//! that hard-coded the number would go green against a constant somebody had halved.
//!
//! CAN-FAIL, four faults, injected and compiled separately:
//!   S1. DROP THE ENGAGEMENT GUARD - remove `if (!battleEngaged) return false;` from
//!       SuppressesMaterialisation. Compiled clean (exit 0), because a missing early return is not a
//!       script error. The case fails on its first row: "an UNENGAGED battle must suppress nothing -
//!       a counter-attack's silent stages would empty the objective and give themselves away".
//!   S2. INVERT THE FACTION TEST - `if (forceFactionKey == occupyingFactionKey) return false;`.
//!       Compiled clean (exit 0). Row 2 is the first to reach it, so the case fails on "an engaged
//!       battle must suppress the occupying faction's own force standing on it".
//!   S3. DROP THE RANGE TEST - make WithinBattleRange `return true;`. Compiled clean (exit 0). Rows 1
//!       and 2 still pass (they are inside anyway), so the case fails on the first outside row: "a
//!       force one metre outside the ring must not be suppressed".
//!   S4. DROP THE EMPTY-KEY GUARD - remove `if (occupyingFactionKey.IsEmpty()) return false;`.
//!       Compiled clean (exit 0). Every earlier row passes and the case fails on the last one: "an
//!       unresolvable occupying faction key must suppress nothing, even against an empty force key".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_DeploymentBattleSuppression_TheRuleIsEngagedAndOccupyingAndInsideTheCircle : SCR_AutotestCaseBase
{
	//! Stand-in faction keys. Real ones as shipped, so a reader recognises them, but the rule compares
	//! strings and knows nothing about either.
	static const string OCCUPYING_KEY = "USSR";
	static const string RESISTANCE_KEY = "FIA";
	static const string THIRD_KEY = "CIV";

	//! How far either side of the ring the boundary rows sit. One metre is far larger than any
	//! floating-point error in the distance and far smaller than anything a player could perceive.
	static const float BOUNDARY_MARGIN_M = 1;

	//! A position that is unambiguously nowhere near the battle - several rings away, so a rule that
	//! had gone global again cannot pass this row by accident.
	static const float FAR_AWAY_M = 5000;

	//! Where every row's battle is. Deliberately not the origin: a rule that compared against
	//! vector.Zero by mistake would still be wrong everywhere else, and this makes it wrong HERE.
	static const vector BATTLE = "1200 40 -3400";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		float ring = OVT_QRFControllerComponent.QRF_RANGE;
		float insideDistance = ring - BOUNDARY_MARGIN_M;
		float outsideDistance = ring + BOUNDARY_MARGIN_M;

		vector onTheBattle = BATTLE;
		vector justInside = EastOf(BATTLE, insideDistance);
		vector justOutside = EastOf(BATTLE, outsideDistance);
		vector farAway = EastOf(BATTLE, FAR_AWAY_M);

		// --- CLAIM 1: nothing engaged, nothing suppressed.
		if (OVT_DeploymentBattleSuppression.SuppressesMaterialisation(false, OCCUPYING_KEY, OCCUPYING_KEY, BATTLE, onTheBattle))
		{
			SetFailure("an UNENGAGED battle must suppress nothing - a counter-attack's silent stages would empty the objective and give themselves away");
			return true;
		}

		if (OVT_DeploymentBattleSuppression.SuppressesMaterialisation(false, OCCUPYING_KEY, OCCUPYING_KEY, BATTLE, justInside))
		{
			SetFailure("an UNENGAGED battle must suppress nothing anywhere inside its circle either");
			return true;
		}

		// --- CLAIM 2: engaged, occupying, inside.
		if (!OVT_DeploymentBattleSuppression.SuppressesMaterialisation(true, OCCUPYING_KEY, OCCUPYING_KEY, BATTLE, onTheBattle))
		{
			SetFailure("an engaged battle must suppress the occupying faction's own force standing on it");
			return true;
		}

		if (!OVT_DeploymentBattleSuppression.SuppressesMaterialisation(true, OCCUPYING_KEY, OCCUPYING_KEY, BATTLE, justInside))
		{
			SetFailure("a force %1 m from an engaged battle, one metre inside the %2 m ring, must be suppressed",
				insideDistance.ToString(), ring.ToString());
			return true;
		}

		// --- CLAIM 3: engaged, occupying, outside.
		if (OVT_DeploymentBattleSuppression.SuppressesMaterialisation(true, OCCUPYING_KEY, OCCUPYING_KEY, BATTLE, justOutside))
		{
			SetFailure("a force one metre outside the ring must not be suppressed: %1 m against a %2 m ring",
				outsideDistance.ToString(), ring.ToString());
			return true;
		}

		if (OVT_DeploymentBattleSuppression.SuppressesMaterialisation(true, OCCUPYING_KEY, OCCUPYING_KEY, BATTLE, farAway))
		{
			SetFailure("a force %1 m away must not be suppressed - suppression is LOCAL to the battle, never map-wide", FAR_AWAY_M.ToString());
			return true;
		}

		// --- CLAIM 4: the resistance, standing on the battle.
		if (OVT_DeploymentBattleSuppression.SuppressesMaterialisation(true, OCCUPYING_KEY, RESISTANCE_KEY, BATTLE, onTheBattle))
		{
			SetFailure("the resistance must NEVER be suppressed - the battle's scoring counts those men, so holding them back would score the player down for it");
			return true;
		}

		// --- CLAIM 5: a third faction, standing on the battle.
		if (OVT_DeploymentBattleSuppression.SuppressesMaterialisation(true, OCCUPYING_KEY, THIRD_KEY, BATTLE, onTheBattle))
		{
			SetFailure("faction '%1' is not the occupying faction and must not be suppressed", THIRD_KEY);
			return true;
		}

		// --- CLAIM 6: an unresolvable occupying key.
		if (OVT_DeploymentBattleSuppression.SuppressesMaterialisation(true, string.Empty, OCCUPYING_KEY, BATTLE, onTheBattle))
		{
			SetFailure("an unresolvable occupying faction key must suppress nothing");
			return true;
		}

		if (OVT_DeploymentBattleSuppression.SuppressesMaterialisation(true, string.Empty, string.Empty, BATTLE, onTheBattle))
		{
			SetFailure("an unresolvable occupying faction key must suppress nothing, even against an empty force key");
			return true;
		}

		Print("DeploymentBattleSuppression: nothing is suppressed unengaged; an engaged battle suppresses only the occupying faction, only inside its own ring, and never the resistance");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A point `metres` due east of `origin`, at the same altitude.
	//!
	//! Axis-aligned on purpose: the offset IS the distance, so a failure message quotes a number the
	//! reader can check against the ring by eye rather than a diagonal they would have to work out.
	//! \param[in] origin The point to offset from.
	//! \param[in] metres How far east.
	//! \return The offset position.
	protected vector EastOf(vector origin, float metres)
	{
		return Vector(origin[0] + metres, origin[1], origin[2]);
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_DeploymentBattleSuppression.WithinBattleRange - the geometry on its own, against the battle
//! component's own constant.
//!
//! Split out from the rule above so that a change to the RADIUS and a change to the RULE fail
//! separately and say different things. Three claims:
//!   1. THE RING IS THE BATTLE'S OWN QRF_RANGE, not a copy. Read off OVT_QRFControllerComponent and
//!      used to build every row, so halving the constant moves this case's own inputs with it and the
//!      case stays green - which is the point. What is asserted is the RELATIONSHIP to the constant.
//!   2. IT IS SYMMETRIC IN ALL FOUR HORIZONTAL DIRECTIONS. A ring built out of a signed component
//!      instead of a distance passes north and east and fails south and west.
//!   3. ZERO DISTANCE IS INSIDE - the degenerate row, and the one a strict inequality on the wrong
//!      side of the comparison would break.
//!
//! CAN-FAIL, one fault:
//!   W1. COMPARE THE WRONG WAY ROUND - `vector.Distance(...) > OVT_QRFControllerComponent.QRF_RANGE`.
//!       Compiled clean (exit 0). The case fails on its first row: "a point ON the battle must be
//!       inside the ring".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_DeploymentBattleSuppression_TheRingIsTheBattlesOwnRadiusInEveryDirection : SCR_AutotestCaseBase
{
	static const float BOUNDARY_MARGIN_M = 1;
	static const vector BATTLE = "-820 12 640";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		float ring = OVT_QRFControllerComponent.QRF_RANGE;

		if (ring <= 0)
		{
			SetFailure("the battle radius must be a positive distance: read back %1", ring.ToString());
			return true;
		}

		if (!OVT_DeploymentBattleSuppression.WithinBattleRange(BATTLE, BATTLE))
		{
			SetFailure("a point ON the battle must be inside the ring");
			return true;
		}

		float inside = ring - BOUNDARY_MARGIN_M;
		float outside = ring + BOUNDARY_MARGIN_M;

		if (!CheckAxis("east", Vector(inside, 0, 0), Vector(outside, 0, 0)))
			return true;

		if (!CheckAxis("west", Vector(-inside, 0, 0), Vector(-outside, 0, 0)))
			return true;

		if (!CheckAxis("north", Vector(0, 0, -inside), Vector(0, 0, -outside)))
			return true;

		if (!CheckAxis("south", Vector(0, 0, inside), Vector(0, 0, outside)))
			return true;

		Print("DeploymentBattleSuppression: the ring is the battle's own radius and closes symmetrically on all four horizontal axes");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts that one direction's just-inside point is in and its just-outside point is out.
	//! \param[in] label The direction, for the failure text.
	//! \param[in] insideOffset Offset from the battle to a point one metre inside the ring.
	//! \param[in] outsideOffset Offset from the battle to a point one metre outside it.
	//! \return True when both rows held; false after SetFailure.
	protected bool CheckAxis(string label, vector insideOffset, vector outsideOffset)
	{
		if (!OVT_DeploymentBattleSuppression.WithinBattleRange(BATTLE, BATTLE + insideOffset))
		{
			SetFailure("a point one metre inside the ring to the %1 must be inside it", label);
			return false;
		}

		if (OVT_DeploymentBattleSuppression.WithinBattleRange(BATTLE, BATTLE + outsideOffset))
		{
			SetFailure("a point one metre outside the ring to the %1 must be outside it", label);
			return false;
		}

		return true;
	}
}
