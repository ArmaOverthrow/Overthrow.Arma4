//------------------------------------------------------------------------------------------------
//! TIER A - QRF ZONE CONTROL, as arithmetic.
//!
//! Two pure rules: what one fighter is WORTH at a given distance, and which way the score SWINGS for
//! two weighted sides.
//!
//! ⚠ WHY THIS TIER AND NOT A LIVE BATTLE. Everything below is a function of its arguments, and a live
//! QRF cannot assert any of it: the scoring tick runs every ten seconds against whatever bodies happen
//! to be standing in the world, so the one thing a battle can prove about the maths is that it did not
//! crash. The weighting was added on the author's brief - *"closer characters count for slightly more
//! points than distant ones"* (2026-08-25) - and "slightly" is a claim about numbers, which is exactly
//! what this tier is for.
//!
//! Every subject is a static call. No world, no manager, no campaign, no polling.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! A fighter is worth more the closer he stands, the ring's edge is worth EDGE_WEIGHT, and outside it
//! he is worth nothing at all.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   A1. `if (distance >= pointRange) return 0;` deleted. Fails on "a fighter outside the ring".
//!   A2. The interpolation's sign flipped (`1.0 + ...`). Fails on "the edge must be worth EDGE_WEIGHT".
//!   A3. `if (pointRange <= 0) return 0;` deleted. Fails on "a zero ring".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_QRFScoring_AFighterWeightFallsWithDistance : SCR_AutotestCaseBase
{
	protected const float RING = 220;
	protected const float EPSILON = 0.001;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string failure = CheckWeights();
		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("QRF scoring: a fighter is worth 1.0 on the objective, falls linearly to EDGE_WEIGHT at the ring's edge, and is worth nothing outside it");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckWeights()
	{
		// --- On the objective itself: a whole man.
		float centre = OVT_QRFScoring.FighterWeight(0, RING);
		if (Math.AbsFloat(centre - 1.0) > EPSILON)
			return string.Format("a fighter on the objective must be worth 1.0, was %1", centre.ToString());

		// --- At the very edge: EDGE_WEIGHT. Asked one metre inside, because the edge itself is outside.
		float edge = OVT_QRFScoring.FighterWeight(RING - 1, RING);
		if (Math.AbsFloat(edge - OVT_QRFScoring.EDGE_WEIGHT) > 0.01)
			return string.Format("the edge of the ring must be worth EDGE_WEIGHT (%1), was %2",
				OVT_QRFScoring.EDGE_WEIGHT.ToString(), edge.ToString());

		// --- 🔴 OUTSIDE THE RING IS WORTH NOTHING. The range test the caller used to make is folded
		// into this function, so a fighter beyond it must score zero rather than EDGE_WEIGHT.
		if (OVT_QRFScoring.FighterWeight(RING, RING) != 0)
			return "a fighter at exactly the ring's radius must be worth nothing - the boundary is exclusive, as the caller's old `dist < QRF_POINT_RANGE` test was";

		if (OVT_QRFScoring.FighterWeight(RING + 100, RING) != 0)
			return "a fighter outside the ring must be worth nothing";

		// --- Monotonic: closer is always worth at least as much.
		float near = OVT_QRFScoring.FighterWeight(RING * 0.25, RING);
		float far = OVT_QRFScoring.FighterWeight(RING * 0.75, RING);
		if (near <= far)
			return string.Format("a closer fighter must be worth more than a distant one: %1 m scored %2, %3 m scored %4",
				(RING * 0.25).ToString(), near.ToString(), (RING * 0.75).ToString(), far.ToString());

		// --- ⚠ "SLIGHTLY". The whole ring must stay inside [EDGE_WEIGHT, 1.0]: if the spread ever grows,
		// numbers stop beating position and every battle becomes a flag-camping contest.
		if (near > 1.0 || far < OVT_QRFScoring.EDGE_WEIGHT)
			return "every weight inside the ring must lie between EDGE_WEIGHT and 1.0 - a wider spread would let a smaller force win by hugging the objective";

		// --- A zero or negative ring divides by nothing.
		if (OVT_QRFScoring.FighterWeight(10, 0) != 0)
			return "a zero-radius ring must answer zero rather than divide by it";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! The swing: the uncontested fast push, the weighted advantage, and the stalemate pull towards zero.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   B1. The deadband comparison changed to `advantage > 0`. Fails on "a near-tie must be a stalemate".
//!   B2. `occupyingAnywhere == 0` changed to `occupyingWeight == 0`. Fails on "a distant survivor".
//!   B3. The stalemate branch's sign flipped. Fails on "a stalemate must pull towards zero".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_QRFScoring_BSwingFollowsWeightedStrength : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string failure = CheckSwing();
		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("QRF scoring: an uncontested ring pushes +5, a weighted advantage beyond the deadband pushes one step, and anything closer than that is a stalemate that decays towards zero");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckSwing()
	{
		// --- Uncontested: the fast push that ends a walkover quickly.
		if (OVT_QRFScoring.ResolveSwing(2.0, 0, 0, 0) != 5)
			return "the resistance holding the ring with no enemy left in the battle must push +5";

		// --- 🔴 AND THE FAST PUSH IS GATED ON A HEAD COUNT, NOT ON THE WEIGHT. A defender at the far
		// edge of the ring scores almost nothing but is still alive and still fighting; treating him as
		// absent would end a contested battle at five points a tick.
		if (OVT_QRFScoring.ResolveSwing(2.0, 0, 1, 0) == 5)
			return "an enemy still alive anywhere in the battle must stop the +5 fast push, however little he weighs in the ring";

		// --- A clear advantage each way.
		if (OVT_QRFScoring.ResolveSwing(3.0, 1.0, 2, 0) != 1)
			return "a clear resistance advantage must push +1";

		if (OVT_QRFScoring.ResolveSwing(1.0, 3.0, 2, 0) != -1)
			return "a clear occupying advantage must push -1";

		// --- 🔴 THE DEADBAND. Floats do not have ties: without it the "push towards zero" branch could
		// never run, because two sides are essentially never exactly equal once weights are involved.
		if (OVT_QRFScoring.ResolveSwing(3.0, 3.0, 2, 0) != 0)
			return "an exact tie at zero points must not move the score";

		if (OVT_QRFScoring.ResolveSwing(3.1, 3.0, 2, 0) != 0)
			return "a near-tie must read as a stalemate, not as an advantage - otherwise the weighting makes the stalemate branch unreachable";

		// --- A stalemate decays whatever the score is, and never overshoots zero.
		if (OVT_QRFScoring.ResolveSwing(3.0, 3.0, 2, 4) != -1)
			return "a stalemate with the resistance ahead must pull the score down towards zero";

		if (OVT_QRFScoring.ResolveSwing(3.0, 3.0, 2, -4) != 1)
			return "a stalemate with the occupying faction ahead must pull the score up towards zero";

		// --- An empty ring on both sides is a stalemate, not a resistance win.
		if (OVT_QRFScoring.ResolveSwing(0, 0, 0, 0) != 0)
			return "an empty battle must not push +5 - the fast push requires the resistance to actually hold the ring";

		return "";
	}
}
