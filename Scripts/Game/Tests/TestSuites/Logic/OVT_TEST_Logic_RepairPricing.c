//------------------------------------------------------------------------------------------------
//! TIER A cases - OVT_RepairPricing, the whole of what a repair costs.
//!
//! This is the only part of the repair feature that can be asserted without a world. The price is
//! computed on TWO machines from the same expression - the client draws it in the action label and
//! greys the action out with it, the server re-derives it and takes the money - so a drift here is
//! not a cosmetic difference, it is the gate disagreeing with the authority (implementation.md D12).
//!
//! WHAT BREAKS IF IT DRIFTS, and why none of it is loud:
//!   - round twice instead of once (D11) and the label and the charge disagree by a dollar at some
//!     costs and not others, which reads as a bug in the economy rather than in the arithmetic;
//!   - drop the UNKNOWN_STRUCTURE_COST guard and a structure no config entry claims quotes a
//!     MILLION-dollar repair - the sentinel is deliberately huge, not zero, precisely so it sorts
//!     last for sabotage, and anything that prices it instead of refusing it inherits that number;
//!   - let repairCostMultiplier exceed 1 at any preset and repairing becomes dearer than rebuilding,
//!     which no player would ever choose and nothing in the code would complain about.
//!
//! THE DIFFICULTY NUMBERS ARE HARD-CODED HERE ON PURPOSE. The Logic tier has no config, and copying
//! them is what makes the ladder an assertion about the SHIPPED presets rather than a tautology over
//! whatever is authored. If Configs/Difficulty/*.conf changes, these cases must be updated with it -
//! that is the point (buildableCostMultiplier / repairCostMultiplier: Easy .8/.5, Normal 1/.5,
//! Hard 1.5/.75, Extreme 3/1, Insane 4/1). The eight costs are the shipped
//! Configs/Resistance/buildables.conf m_iCost values.
//!
//! COMPILE PROOF: this file names OVT_RepairPricing, which nothing else in the test tree names;
//! tools/compile-check.sh resolves it, so these cases are genuinely in the compile set. No retry
//! attribute is used anywhere - this quality bar bans it outright.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! THE HEADLINE RULE: on Normal a repair costs exactly HALF what the structure cost to build, and at
//! the top of the ladder it costs exactly the full build price (Definition of Done F4).
//!
//! Also pins the two degenerate inputs. A zero cost must price at zero rather than at some floor, and
//! a NEGATIVE multiplier - a mis-authored preset, or a client that failed to read the config stream
//! and left the float at whatever it was - must clamp to free, never to a refund.
//!
//! PROVEN ABLE TO FAIL (fail proof recorded, execution belongs to the phase's suite run): change the
//! half-price assertion to expect the full price and the case goes red at the first rung.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RepairPricing_HalfAndFull : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// Normal: buildableCostMultiplier 1, repairCostMultiplier 0.5.
		int cost = OVT_RepairPricing.RepairCost(1000, 1.0, 0.5);
		if (cost != 500)
		{
			SetFailure("A $1000 structure repaired at 1.0/0.5 cost %1, expected 500 - on Normal a repair must be exactly half the build price (F4)", cost.ToString());
			return true;
		}

		// Insane: the repair multiplier is 1, so a repair costs what a rebuild costs at that preset.
		cost = OVT_RepairPricing.RepairCost(1000, 1.0, 1.0);
		if (cost != 1000)
		{
			SetFailure("A $1000 structure repaired at 1.0/1.0 cost %1, expected 1000 - a repair multiplier of 1 must charge the full build price (F4)", cost.ToString());
			return true;
		}

		// A free structure is free to repair.
		cost = OVT_RepairPricing.RepairCost(0, 1.5, 0.75);
		if (cost != 0)
		{
			SetFailure("A $0 structure priced at %1, expected 0 - nothing was paid to build it and nothing is owed to repair it", cost.ToString());
			return true;
		}

		// A negative build multiplier clamps to free rather than paying the player.
		cost = OVT_RepairPricing.RepairCost(1000, -1.0, 0.5);
		if (cost != 0)
		{
			SetFailure("A $1000 structure at a NEGATIVE build multiplier priced at %1, expected 0 - a mis-authored preset must land on free, never on the repair paying the player", cost.ToString());
			return true;
		}

		// Same for a negative repair multiplier.
		cost = OVT_RepairPricing.RepairCost(1000, 1.0, -0.5);
		if (cost != 0)
		{
			SetFailure("A $1000 structure at a NEGATIVE repair multiplier priced at %1, expected 0", cost.ToString());
			return true;
		}

		Print("Repair pricing: half the build price on Normal, the full price at Insane, free at zero cost, clamped at negative multipliers");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! THE FULL SHIPPED LADDER: every one of the eight buildables, at every one of the five presets.
//!
//! Forty expected numbers, written out rather than recomputed, because a case that derives its
//! expectation from the same expression it is testing asserts nothing. They are the arithmetic a
//! player will see on the action label, and re-deriving them by hand is the only way this file can
//! catch an implementation that is internally consistent and wrong.
//!
//! ONE RUNG IS A GENUINE TIE and is treated as one: the Vehicle Maintenance Ramp and the Helipad both
//! cost 1500, and 1500 x 1.5 x 0.75 is exactly 1687.5. Math.Round's documented examples do not say
//! which way it breaks a tie, so both 1687 and 1688 are accepted THERE AND ONLY THERE - every other
//! rung is asserted exactly.
//!
//! PROVEN ABLE TO FAIL (fail proof recorded, execution belongs to the phase's suite run): change the
//! Garage's Insane expectation from 32000 to 32001 and the case goes red naming that rung.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RepairPricing_ShippedLadder : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// Configs/Resistance/buildables.conf, in file order.
		array<int> costs = {1200, 1000, 1000, 1500, 750, 8000, 1500, 2000};
		array<string> names = {"Guard Tower", "Recruitment Tent", "Medical Tent", "Vehicle Maintenance Ramp", "Bunkers", "Garage", "Helipad", "Fuel Depot"};

		// Configs/Difficulty/*.conf, in ladder order.
		array<string> presets = {"Easy", "Normal", "Hard", "Extreme", "Insane"};
		array<float> builds = {0.8, 1.0, 1.5, 3.0, 4.0};
		array<float> repairs = {0.5, 0.5, 0.75, 1.0, 1.0};

		// Costs x presets, row-major.
		array<int> expected = {
			480,  600,  1350, 3600,  4800,
			400,  500,  1125, 3000,  4000,
			400,  500,  1125, 3000,  4000,
			600,  750,  1687, 4500,  6000,
			300,  375,  844,  2250,  3000,
			3200, 4000, 9000, 24000, 32000,
			600,  750,  1687, 4500,  6000,
			800,  1000, 2250, 6000,  8000
		};

		for (int i = 0; i < costs.Count(); i++)
		{
			for (int j = 0; j < presets.Count(); j++)
			{
				int want = expected[(i * presets.Count()) + j];
				int got = OVT_RepairPricing.RepairCost(costs[i], builds[j], repairs[j]);

				// The 1500 x 1.5 x 0.75 = 1687.5 tie: the engine's rounding is not documented at .5.
				bool tie = (costs[i] == 1500 && j == 2);
				if (tie && (got == want || got == want + 1)) continue;

				if (got != want)
				{
					SetFailure(string.Format("%1 on %2 priced at %3, expected %4 - the shipped ladder is what a player is charged and what the action label promises",
						names[i], presets[j], got.ToString(), want.ToString()));
					return true;
				}
			}
		}

		Print("Repair pricing: all eight shipped buildables price correctly at all five difficulty presets");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! ROUNDING: the result is the nearest whole dollar, not a truncation, and a .5 boundary lands on one
//! of the two adjacent dollars rather than anywhere else.
//!
//! WHY THE TIE IS ASSERTED AS A RANGE. Math.Round's own documentation (Core/generated/Math/Math.c:30-40)
//! shows only 5.3 and 5.8, so which way it breaks an exact .5 is undefined behaviour as far as this
//! project can prove. Pinning it to one answer would make this case a claim about the engine that no
//! evidence supports; pinning it to a RANGE still catches the failures that matter - a truncation, a
//! double rounding, or a price that lands nowhere near the input.
//!
//! PROVEN ABLE TO FAIL (fail proof recorded, execution belongs to the phase's suite run): assert that
//! 2.7 prices at 2 (i.e. that the implementation truncates) and the case goes red.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RepairPricing_Rounding : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// 2.7 must round UP. A truncating implementation answers 2 here and would be a dollar cheap on
		// roughly half of all prices.
		int cost = OVT_RepairPricing.RepairCost(9, 1.0, 0.3);
		if (cost != 3)
		{
			SetFailure("9 x 1.0 x 0.3 = 2.7 priced at %1, expected 3 - the price is rounded, not truncated", cost.ToString());
			return true;
		}

		// 2.4 must round DOWN, so this is rounding and not a ceiling either.
		cost = OVT_RepairPricing.RepairCost(10, 1.0, 0.24);
		if (cost != 2)
		{
			SetFailure("10 x 1.0 x 0.24 = 2.4 priced at %1, expected 2 - the price is rounded, not raised to the next dollar", cost.ToString());
			return true;
		}

		// The .5 boundary: 1001 x 1.0 x 0.5 = 500.5 exactly.
		cost = OVT_RepairPricing.RepairCost(1001, 1.0, 0.5);
		if (cost != 500 && cost != 501)
		{
			SetFailure("1001 x 1.0 x 0.5 = 500.5 priced at %1, expected 500 or 501 - a price at a .5 boundary must land on an adjacent dollar", cost.ToString());
			return true;
		}

		// Immediately either side of that boundary there is no ambiguity at all.
		cost = OVT_RepairPricing.RepairCost(1000, 1.0, 0.5);
		if (cost != 500)
		{
			SetFailure("1000 x 1.0 x 0.5 = 500 priced at %1, expected 500", cost.ToString());
			return true;
		}

		cost = OVT_RepairPricing.RepairCost(1002, 1.0, 0.5);
		if (cost != 501)
		{
			SetFailure("1002 x 1.0 x 0.5 = 501 priced at %1, expected 501", cost.ToString());
			return true;
		}

		Print("Repair pricing: rounds to the nearest dollar in both directions and stays adjacent at a .5 boundary");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! REPAIRABILITY, and the invariant that makes repair worth choosing.
//!
//! Two claims:
//!   1. UNKNOWN_STRUCTURE_COST is not a price. It is the sentinel GetStructureCost() answers for a
//!      structure no config entry claims, deliberately huge so sabotage sorts it last - so anything
//!      that treats it as money quotes a million dollars.
//!   2. A repair NEVER costs more than a rebuild, at any authored preset and any authored cost. That
//!      is the whole economic proposition of the mechanic, and it holds only while every preset's
//!      repairCostMultiplier stays at or below 1.
//!
//! PROVEN ABLE TO FAIL (fail proof recorded, execution belongs to the phase's suite run): raise the
//! Insane repair multiplier in this case's local ladder to 1.1 and the invariant assertion goes red
//! naming that preset.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RepairPricing_RepairableAndNeverDearer : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (OVT_RepairPricing.IsRepairable(OVT_ResistanceFactionManager.UNKNOWN_STRUCTURE_COST))
		{
			SetFailure("IsRepairable(UNKNOWN_STRUCTURE_COST) answered true - the unpriced-structure sentinel is %1, so a structure no config claims would quote that as a repair bill",
				OVT_ResistanceFactionManager.UNKNOWN_STRUCTURE_COST.ToString());
			return true;
		}

		if (OVT_RepairPricing.IsRepairable(0))
		{
			SetFailure("IsRepairable(0) answered true - a structure with no authored cost has no repair price to quote");
			return true;
		}

		if (!OVT_RepairPricing.IsRepairable(750))
		{
			SetFailure("IsRepairable(750) answered false - the Bunkers is the cheapest shipped buildable and must be repairable");
			return true;
		}

		if (!OVT_RepairPricing.IsRepairable(8000))
		{
			SetFailure("IsRepairable(8000) answered false - the Garage is the dearest shipped buildable and must be repairable");
			return true;
		}

		// The invariant, over the same shipped numbers as the ladder case.
		array<int> costs = {1200, 1000, 1000, 1500, 750, 8000, 1500, 2000};
		array<string> presets = {"Easy", "Normal", "Hard", "Extreme", "Insane"};
		array<float> builds = {0.8, 1.0, 1.5, 3.0, 4.0};
		array<float> repairs = {0.5, 0.5, 0.75, 1.0, 1.0};

		for (int i = 0; i < costs.Count(); i++)
		{
			for (int j = 0; j < presets.Count(); j++)
			{
				int repair = OVT_RepairPricing.RepairCost(costs[i], builds[j], repairs[j]);

				// What OVT_OverthrowConfigComponent.GetBuildableCost() answers for the same structure.
				int build = Math.Round(costs[i] * builds[j]);

				if (repair > build)
				{
					SetFailure(string.Format("A $%1 structure on %2 costs %3 to repair but only %4 to build - a repair must never be dearer than a rebuild",
						costs[i].ToString(), presets[j], repair.ToString(), build.ToString()));
					return true;
				}
			}
		}

		Print("Repair pricing: the unpriced sentinel is refused, and no shipped structure is ever dearer to repair than to rebuild");
		return true;
	}
}
