//------------------------------------------------------------------------------------------------
//! TIER A cases - OVT_FuelPricing and OVT_FuelChargeLedger, the pure arithmetic behind what a tank
//! of fuel costs.
//!
//! This is the only part of the fuel fee that can be asserted without a world. The engine fuel stack
//! - support stations, fuel nodes, flow capacities, the looped user action - is not reachable from
//! the Logic tier, and the money path that consumes these numbers runs inside a modded vanilla
//! component on a server. Which is exactly why every decision that could be a pure function was made
//! one: the claims below are the whole of the pricing contract - what a litre costs, how the
//! sub-dollar remainder carries, what the affordability gate demands, how the price is drawn, what
//! one press of the fast Fill action moves and charges - and everything above them is plumbing that
//! a play-test can see.
//!
//! WHAT BREAKS IF IT DRIFTS, and why none of it is loud:
//!   - lose the negative/zero clamp and fuelPricePerLitre 0 stops being the kill switch - a preset
//!     authored at -1 would start REFUNDING fuel, and the "disabled" setting would gate players out
//!     of a free pump;
//!   - lose the ledger's carry and every 0.5 s tick either drops its remainder (fuel is nearly free
//!     at low flow) or rounds up (fuel costs nearly double at low flow) - both look plausible on
//!     screen and neither raises anything;
//!   - lose the ceiling on the tick estimate and the affordability gate lets through a tick the
//!     charge cannot cover, which surfaces as an overdraw the take silently clamps to 0;
//!   - lose the trailing-zero trim and the action label reads "($1.00/L)" forever.
//!
//! WHAT IS NOT HERE. Whether a station is free, whether a player resolves from an action user,
//! whether money actually moves, and whether the depot dispenses - all live state, all play-test
//! gated (implementation.md § Testing Strategy).
//!
//! COMPILE PROOF: this file names OVT_FuelPricing and OVT_FuelChargeLedger, which nothing else in
//! the test tree names; tools/compile-check.sh resolves it, so these cases are genuinely in the
//! compile set. No retry attribute is used anywhere - this quality bar bans it outright.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! COST IS LITRES x PRICE, and a price that resolves to zero costs nothing.
//!
//! The zero half is the kill switch (implementation.md D9) and the negative half is its guard rail:
//! a mis-authored preset, or a client that failed to read the config stream, must land on "free",
//! never on "the pump pays you". The large-litre assertion pins that a full fuel-truck cargo tank
//! (5,000 L) still prices exactly, because a float that loses a dollar at that scale would do it
//! silently on the one transaction big enough to notice.
//!
//! PROVEN ABLE TO FAIL (fail proof recorded, execution belongs to the phase's suite run): invert the
//! negative-price assertion to expect a non-zero cost and the case goes red at the -1 rung.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Fuel_CostMath : SCR_AutotestCaseBase
{
	protected const float EPSILON = 0.001;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// The dominant case: one 0.5 s tick at a car's fill rate, at the default price.
		float cost = OVT_FuelPricing.ComputeCost(2.08, 1.0);
		if (Math.AbsFloat(cost - 2.08) > EPSILON)
		{
			SetFailure("2.08 L at $1/L cost %1, expected 2.08 - cost is litres x price and nothing else", cost.ToString());
			return true;
		}

		// A non-unit price multiplies, it does not round.
		cost = OVT_FuelPricing.ComputeCost(10.0, 2.5);
		if (Math.AbsFloat(cost - 25.0) > EPSILON)
		{
			SetFailure("10 L at $2.5/L cost %1, expected 25", cost.ToString());
			return true;
		}

		// A price of 0 is the documented kill switch: no charge anywhere, at any volume.
		cost = OVT_FuelPricing.ComputeCost(500.0, 0);
		if (cost != 0)
		{
			SetFailure("500 L at $0/L cost %1, expected 0 - fuelPricePerLitre 0 is the kill switch and must charge nothing", cost.ToString());
			return true;
		}

		// A negative price resolves to 0, never to a refund.
		float resolved = OVT_FuelPricing.ResolvePrice(-1.0);
		if (resolved != 0)
		{
			SetFailure("ResolvePrice(-1) answered %1, expected 0 - a negative price must clamp, not refund", resolved.ToString());
			return true;
		}

		cost = OVT_FuelPricing.ComputeCost(100.0, -1.0);
		if (cost != 0)
		{
			SetFailure("100 L at $-1/L cost %1, expected 0 - a mis-authored preset must land on free, never on the pump paying the player", cost.ToString());
			return true;
		}

		// A positive price is passed through untouched.
		resolved = OVT_FuelPricing.ResolvePrice(1.5);
		if (Math.AbsFloat(resolved - 1.5) > EPSILON)
		{
			SetFailure("ResolvePrice(1.5) answered %1, expected 1.5", resolved.ToString());
			return true;
		}

		// Nothing delivered costs nothing, even at a live price.
		cost = OVT_FuelPricing.ComputeCost(0, 1.0);
		if (cost != 0)
		{
			SetFailure("0 L at $1/L cost %1, expected 0 - a tick that delivered nothing must not be charged", cost.ToString());
			return true;
		}

		// A full fuel-truck cargo tank, the largest single delivery in the game.
		cost = OVT_FuelPricing.ComputeCost(5000.0, 1.5);
		if (Math.AbsFloat(cost - 7500.0) > 1.0)
		{
			SetFailure("5000 L at $1.5/L cost %1, expected 7500 - the biggest transaction in the game must still price exactly", cost.ToString());
			return true;
		}

		Print("Fuel cost is litres x price; zero and negative prices both resolve to free; large deliveries price within a dollar");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! THE LEDGER CARRIES THE REMAINDER instead of dropping or rounding it.
//!
//! Money is an int and a refuel tick costs a fraction of a dollar, so this is the arithmetic that
//! decides whether a tank of fuel bills at its price, at nothing, or at nearly double. Five 30-cent
//! accruals must charge exactly one dollar, on the fourth, and leave half a dollar waiting.
//!
//! THE TWO-KEY CLAIM IS THE MULTIPLAYER ONE. Two players refuelling at the same pump share one
//! ledger instance; a key collision or a shared accumulator would charge one for the other's fuel,
//! and Clear() on a broke player must not disturb the solvent one standing next to them.
//!
//! WHAT IS DELIBERATELY NOT ASSERTED: that the pending remainder is ever settled or persisted. It is
//! not, by design (implementation.md D8) - the leakage is capped at $0.99 per player per pump and
//! the alternative is a money path with no fuel behind it.
//!
//! PROVEN ABLE TO FAIL (fail proof recorded, execution belongs to the phase's suite run): invert the
//! fourth-accrual assertion to expect 0 charged and the case goes red on the accrual walk.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Fuel_ChargeLedger : SCR_AutotestCaseBase
{
	protected const float EPSILON = 0.001;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_FuelChargeLedger ledger = new OVT_FuelChargeLedger();

		// 0.3 litres at $1/L, five times: 0.3, 0.6, 0.9, 1.2 -> $1, 0.5.
		array<int> expected = {};
		expected.Insert(0);
		expected.Insert(0);
		expected.Insert(0);
		expected.Insert(1);
		expected.Insert(0);

		int totalCharged = 0;
		float totalAccrued = 0;

		for (int i = 0; i < expected.Count(); i++)
		{
			int charged = ledger.Accrue("player-a", 0.3, 1.0);
			totalCharged = totalCharged + charged;
			totalAccrued = totalAccrued + 0.3;

			if (charged != expected[i])
			{
				SetFailure("Accrual %1 charged $%2, expected $%3 - the remainder must carry, not be dropped or rounded up every tick", i.ToString(), charged.ToString(), expected[i].ToString());
				return true;
			}

			if (totalCharged > totalAccrued + EPSILON)
			{
				string detail = totalCharged.ToString() + " charged vs " + totalAccrued.ToString() + " accrued";
				SetFailure("After accrual %1 the ledger had taken more than it accrued (%2) - a player must never be charged for fuel that did not arrive", i.ToString(), detail);
				return true;
			}
		}

		float pending = ledger.GetPending("player-a");
		if (Math.AbsFloat(pending - 0.5) > EPSILON)
		{
			SetFailure("Five 30-cent accruals left %1 pending, expected 0.5 - the carried remainder is what makes the price come out right over a whole tank", pending.ToString());
			return true;
		}

		// A second payer at the same pump is accounted separately.
		int chargedB = ledger.Accrue("player-b", 0.4, 1.0);
		if (chargedB != 0)
		{
			SetFailure("A second payer's first 40-cent accrual charged $%1, expected $0 - two players at one pump must not share an accumulator", chargedB.ToString());
			return true;
		}

		if (Math.AbsFloat(ledger.GetPending("player-a") - 0.5) > EPSILON)
		{
			SetFailure("Accruing for player-b moved player-a's pending to %1, expected 0.5 - concurrent refuellers must never cross-charge", ledger.GetPending("player-a").ToString());
			return true;
		}

		if (ledger.GetTrackedCount() != 2)
		{
			SetFailure("The ledger tracks %1 payers, expected 2", ledger.GetTrackedCount().ToString());
			return true;
		}

		// Clear drops one payer's remainder and leaves the other's alone.
		ledger.Clear("player-a");

		if (ledger.GetPending("player-a") != 0)
		{
			SetFailure("After Clear, player-a still has %1 pending, expected 0 - a clamped charge must not carry a shortfall forward", ledger.GetPending("player-a").ToString());
			return true;
		}

		if (Math.AbsFloat(ledger.GetPending("player-b") - 0.4) > EPSILON)
		{
			SetFailure("Clearing player-a moved player-b's pending to %1, expected 0.4 - Clear must touch exactly one key", ledger.GetPending("player-b").ToString());
			return true;
		}

		if (ledger.GetTrackedCount() != 1)
		{
			SetFailure("After clearing one of two payers the ledger tracks %1, expected 1", ledger.GetTrackedCount().ToString());
			return true;
		}

		// An unknown payer has nothing pending and clearing one is not an error.
		if (ledger.GetPending("player-c") != 0)
		{
			SetFailure("An unknown payer reported %1 pending, expected 0", ledger.GetPending("player-c").ToString());
			return true;
		}

		ledger.Clear("player-c");

		Print("The charge ledger carries sub-dollar remainders, never over-charges, keeps payers independent and clears exactly one key");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! THE TICK ESTIMATE IS CONSERVATIVE: rounded up, floored at a dollar, and zero when fuel is free.
//!
//! This number is the affordability gate on both machines. It must never UNDER-state the next tick,
//! because the take that follows clamps at 0 and would hide the overdraw as a successful sale; the
//! cost of over-stating it is a player left holding a few unspendable dollars, which is documented
//! expected behaviour (implementation.md R10) and not a defect to "fix" by charging in advance.
//!
//! THE FLOOR MUST NOT APPLY AT PRICE ZERO. A floored estimate on a free pump would gate a player
//! with no money out of free fuel - the kill switch failing open into the exact behaviour it exists
//! to disable.
//!
//! PROVEN ABLE TO FAIL (fail proof recorded, execution belongs to the phase's suite run): invert the
//! zero-price assertion to expect the $1 floor and the case goes red on the free-pump rung.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Fuel_TickEstimate : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// Rounds UP, never down: 2.08 L at $1/L is $3 to be allowed, not $2.
		int estimate = OVT_FuelPricing.EstimateTickCost(2.08, 1.0);
		if (estimate != 3)
		{
			SetFailure("A 2.08 L tick at $1/L estimated $%1, expected $3 - the gate rounds up so the take can never overdraw", estimate.ToString());
			return true;
		}

		// An exact dollar amount is not inflated by the ceiling.
		estimate = OVT_FuelPricing.EstimateTickCost(4.0, 2.0);
		if (estimate != 8)
		{
			SetFailure("A 4 L tick at $2/L estimated $%1, expected $8 - an exact amount must not be pushed up a dollar", estimate.ToString());
			return true;
		}

		// A tiny delivery at a live price still costs the floor.
		estimate = OVT_FuelPricing.EstimateTickCost(0.01, 0.1);
		if (estimate != OVT_FuelPricing.MIN_TICK_COST)
		{
			SetFailure("A 0.01 L tick at $0.1/L estimated $%1, expected the $%2 floor - a charged pump must never be free in practice", estimate.ToString(), OVT_FuelPricing.MIN_TICK_COST.ToString());
			return true;
		}

		// A free pump gates nobody: 0, never the floor.
		estimate = OVT_FuelPricing.EstimateTickCost(2.08, 0);
		if (estimate != 0)
		{
			SetFailure("A 2.08 L tick at $0/L estimated $%1, expected $0 - the floor must not apply to free fuel or the kill switch gates players out of a free pump", estimate.ToString());
			return true;
		}

		// A negative price behaves as zero, for the same reason.
		estimate = OVT_FuelPricing.EstimateTickCost(2.08, -2.0);
		if (estimate != 0)
		{
			SetFailure("A 2.08 L tick at $-2/L estimated $%1, expected $0", estimate.ToString());
			return true;
		}

		// The estimate is an UPPER BOUND: whatever is actually delivered within the tick's flow
		// costs no more than the estimate said it would.
		float tickFlow = 2.08;
		float price = 1.5;
		int gate = OVT_FuelPricing.EstimateTickCost(tickFlow, price);

		array<float> deliveries = {};
		deliveries.Insert(0.0);
		deliveries.Insert(0.5);
		deliveries.Insert(1.9);
		deliveries.Insert(tickFlow);

		for (int i = 0; i < deliveries.Count(); i++)
		{
			float actual = OVT_FuelPricing.ComputeCost(deliveries[i], price);
			if (actual > gate)
			{
				string detail = actual.ToString() + " actual vs $" + gate.ToString() + " gated";
				SetFailure("Delivering %1 L cost more than the gate allowed (%2) - the affordability gate must bound the charge that follows it", deliveries[i].ToString(), detail);
				return true;
			}
		}

		Print("The tick estimate rounds up, floors at $1 while fuel costs anything, answers 0 at a free pump, and bounds the charge that follows");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! THE PRICE IN THE ACTION LABEL CARRIES NO TRAILING-ZERO NOISE.
//!
//! The suffix is appended to a label that already shows the fill percentage - "Refuel (37.5) ($1/L)"
//! - so every wasted character is drawn on a hovered action in the middle of the world. "1.00" and
//! "1.50" are the shapes a naive format produces and the ones this pins out.
//!
//! PROVEN ABLE TO FAIL (fail proof recorded, execution belongs to the phase's suite run): invert the
//! 1.0 assertion to expect "1.00" and the case goes red on the first rung.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Fuel_PriceFormat : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<float> prices = {};
		prices.Insert(1.0);
		prices.Insert(1.5);
		prices.Insert(2.25);
		prices.Insert(0);

		array<string> expected = {};
		expected.Insert("1");
		expected.Insert("1.5");
		expected.Insert("2.25");
		expected.Insert("0");

		for (int i = 0; i < prices.Count(); i++)
		{
			string formatted = OVT_FuelPricing.FormatPricePerLitre(prices[i]);
			if (formatted != expected[i])
			{
				string detail = "'" + formatted + "', expected '" + expected[i] + "'";
				SetFailure("Price %1 formatted as %2 - the label suffix must carry no trailing-zero noise", prices[i].ToString(), detail);
				return true;
			}
		}

		Print("The price suffix formats without trailing zeroes: 1, 1.5, 2.25, 0");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! THE FILL PLAN IS THE MINIMUM OF THREE LIMITS, AND ITS CHARGE NEVER EXCEEDS THE WALLET.
//!
//! ComputeFillPlan is the whole of the fast "Fill" action's arithmetic. It runs twice for every
//! fill: once on the CLIENT, purely to draw the real price into the action label before the player
//! commits, and once on the SERVER, where its answer is what actually moves fuel and takes money.
//! Both machines must reach the same shape of answer from the same inputs or the label lies.
//!
//! WHAT BREAKS IF IT DRIFTS, and why none of it is loud:
//!   - lose the source clamp and a 5,000 L truck fills a car from a depot that only had 40 L in it,
//!     conjuring fuel out of nothing and charging for it;
//!   - lose the money clamp and the charge overdraws - which is SILENT, because
//!     TakePlayerMoneyPersistentId floors the account at zero and reports success either way, so the
//!     player is simply given fuel nobody paid for;
//!   - let the money limit REFUSE instead of REDUCING and the amendment's whole premise is gone: a
//!     broke player is supposed to get the fuel their money buys, not a blocked action;
//!   - let a free source consult the balance and a penniless player can no longer empty a fuel truck
//!     into their own car, which is the free path this feature promised never to regress.
//!
//! THE ZERO CASE IS NOT A FORMALITY. A full tank is the single most common state to look at a
//! vehicle in, so "nothing needed" is the input this function sees most often; a plan that returned
//! a non-zero cost there would charge for a fill that moved nothing.
//!
//! WHAT IS NOT HERE. How many litres a real tank needs, what a real station has left, whether a
//! source is in range and whether the money actually moved - all live state, all play-test gated.
//!
//! PROVEN ABLE TO FAIL (fail proof recorded, execution belongs to the phase's suite run): invert the
//! source-clamp assertion to expect the full 100 L and the case goes red on the second rung.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Fuel_FillPlan : SCR_AutotestCaseBase
{
	protected const float EPSILON = 0.001;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// 1. Nothing binds: a car needing 40 L at a pump with plenty of fuel and a solvent player.
		OVT_FuelFillPlan plan = OVT_FuelPricing.ComputeFillPlan(40, 5000, 1000, 1.0);
		if (Math.AbsFloat(plan.m_fLitres - 40) > EPSILON || plan.m_iCost != 40)
		{
			SetFailure("An unconstrained fill of 40 L at $1/L planned %1 L for $%2, expected 40 L for $40 - with no limit biting the plan must be exactly what the tank needs", plan.m_fLitres.ToString(), plan.m_iCost.ToString());
			return true;
		}

		// 2. THE SOURCE binds: the tank wants 100 L, the source only has 25 L left.
		plan = OVT_FuelPricing.ComputeFillPlan(100, 25, 1000, 1.0);
		if (Math.AbsFloat(plan.m_fLitres - 25) > EPSILON)
		{
			SetFailure("A 100 L need drawn from a source holding 25 L planned %1 L, expected 25 - fuel that is not in the source cannot be delivered", plan.m_fLitres.ToString());
			return true;
		}

		if (plan.m_iCost != 25)
		{
			SetFailure("Delivering 25 L at $1/L planned a charge of $%1, expected $25 - the cost must be priced from the litres that actually move, not the litres that were wanted", plan.m_iCost.ToString());
			return true;
		}

		// 3. THE MONEY binds, and it REDUCES the fill rather than refusing it. $10 at $1/L buys 10 L.
		plan = OVT_FuelPricing.ComputeFillPlan(100, 5000, 10, 1.0);
		if (Math.AbsFloat(plan.m_fLitres - 10) > EPSILON || plan.m_iCost != 10)
		{
			SetFailure("A broke player's fill planned %1 L for $%2, expected 10 L for $10 - running short of money must buy less fuel, never no fuel", plan.m_fLitres.ToString(), plan.m_iCost.ToString());
			return true;
		}

		// A non-unit price still spends the whole balance and no more.
		plan = OVT_FuelPricing.ComputeFillPlan(100, 5000, 10, 1.5);
		if (plan.m_iCost != 10)
		{
			SetFailure("Spending a $10 balance at $1.5/L planned a charge of $%1, expected $10 - the clamped charge is the balance itself, never past it", plan.m_iCost.ToString());
			return true;
		}

		if (Math.AbsFloat(plan.m_fLitres - 6.6666) > 0.01)
		{
			SetFailure("Spending $10 at $1.5/L planned %1 L, expected about 6.67 - the litres bought must follow the money spent", plan.m_fLitres.ToString());
			return true;
		}

		// 4. THE CHARGE NEVER EXCEEDS THE BALANCE, at any combination of the three limits. An overdraw
		// here would be invisible in game: the take clamps at zero and reports success.
		array<float> needs = {};
		needs.Insert(0);
		needs.Insert(0.4);
		needs.Insert(37.5);
		needs.Insert(5000);

		array<int> balances = {};
		balances.Insert(0);
		balances.Insert(1);
		balances.Insert(37);
		balances.Insert(100000);

		array<float> prices = {};
		prices.Insert(0.5);
		prices.Insert(1.0);
		prices.Insert(4.0);

		for (int n = 0; n < needs.Count(); n++)
		{
			for (int b = 0; b < balances.Count(); b++)
			{
				for (int p = 0; p < prices.Count(); p++)
				{
					plan = OVT_FuelPricing.ComputeFillPlan(needs[n], 5000, balances[b], prices[p]);

					if (plan.m_iCost > balances[b])
					{
						string overdraw = "$" + plan.m_iCost.ToString() + " from a $" + balances[b].ToString() + " balance";
						SetFailure("A fill of %1 L at $%2/L planned to take %3 - the charge must never exceed the balance it was computed against, because the take that follows clamps at zero and would hide the overdraw", needs[n].ToString(), prices[p].ToString(), overdraw);
						return true;
					}

					if (plan.m_iCost < 0 || plan.m_fLitres < 0)
					{
						string negative = plan.m_fLitres.ToString() + " L for $" + plan.m_iCost.ToString();
						SetFailure("A fill of %1 L at $%2/L planned %3 - neither half of a plan may ever go negative", needs[n].ToString(), prices[p].ToString(), negative);
						return true;
					}

					if (plan.m_fLitres > needs[n] + EPSILON)
					{
						SetFailure("A fill of %1 L at $%2/L planned %3 L - a plan must never deliver more than the target can hold", needs[n].ToString(), prices[p].ToString(), plan.m_fLitres.ToString());
						return true;
					}
				}
			}
		}

		// 5. A FREE SOURCE COSTS NOTHING AND IS NEVER LIMITED BY MONEY. A penniless player emptying a
		// fuel truck into their car is the free path this feature promised not to regress.
		plan = OVT_FuelPricing.ComputeFillPlan(5000, 5000, 0, 0);
		if (Math.AbsFloat(plan.m_fLitres - 5000) > EPSILON || plan.m_iCost != 0)
		{
			SetFailure("A free 5000 L fill for a player with $0 planned %1 L for $%2, expected 5000 L for $0 - a free source must never consult the wallet", plan.m_fLitres.ToString(), plan.m_iCost.ToString());
			return true;
		}

		// A negative price is the same thing, for the same reason ResolvePrice clamps it.
		plan = OVT_FuelPricing.ComputeFillPlan(50, 5000, 0, -2.0);
		if (Math.AbsFloat(plan.m_fLitres - 50) > EPSILON || plan.m_iCost != 0)
		{
			SetFailure("A fill at a negative price planned %1 L for $%2, expected 50 L for $0 - a mis-authored price must land on free, never on the pump paying the player", plan.m_fLitres.ToString(), plan.m_iCost.ToString());
			return true;
		}

		// 6. NOTHING NEEDED IS NOTHING DONE - the state a full tank is looked at in all day.
		plan = OVT_FuelPricing.ComputeFillPlan(0, 5000, 1000, 1.0);
		if (plan.m_fLitres != 0 || plan.m_iCost != 0 || plan.HasFuel())
		{
			SetFailure("A fill of a full tank planned %1 L for $%2 - a fill that moves nothing must charge nothing", plan.m_fLitres.ToString(), plan.m_iCost.ToString());
			return true;
		}

		// An empty source is the same answer from the other side.
		plan = OVT_FuelPricing.ComputeFillPlan(100, 0, 1000, 1.0);
		if (plan.m_fLitres != 0 || plan.m_iCost != 0)
		{
			SetFailure("A fill from an empty source planned %1 L for $%2, expected nothing at all", plan.m_fLitres.ToString(), plan.m_iCost.ToString());
			return true;
		}

		// And so is a paid fill with an empty wallet: no fuel, no charge, no negative anything.
		plan = OVT_FuelPricing.ComputeFillPlan(100, 5000, 0, 1.0);
		if (plan.m_fLitres != 0 || plan.m_iCost != 0)
		{
			SetFailure("A paid fill for a player with $0 planned %1 L for $%2, expected nothing at all", plan.m_fLitres.ToString(), plan.m_iCost.ToString());
			return true;
		}

		Print("The fill plan takes the minimum of need, stock and wallet; its charge never exceeds the balance; free fuel ignores money; and nothing needed costs nothing");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! THE DISCOVERY QUERIES ANSWER "NOTHING HERE" BY EMPTYING THE CALLER'S ARRAY, NEVER BY LEAVING IT
//! ALONE.
//!
//! FindFuelSourcesCovering / FindFuelSourcesNear are the API resistance/high-command will call on a
//! tick, with an array it reuses every time. Every path that answers 0 - no support station manager
//! (no Overthrow game mode, a vanilla world, this world-free test tier), a non-positive radius, or
//! simply nothing in range - has to clear that array first, or the caller reads LAST tick's stations
//! as if they were this tick's and sends a vehicle to a pump that is not there. An early return that
//! forgets the Clear() is the easiest possible mistake here and it is completely silent: the
//! returned count and the array's contents disagree, and only the array is iterated.
//!
//! The position is deliberately 5 km underground so the claim holds even if a station registry
//! survives from an earlier suite in the same session - no support station's range reaches it.
//!
//! WHAT IS NOT HERE. That a pump 5 m away IS found, and that its price is non-zero while a depot's
//! is 0 - both need a loaded world with registered stations, and both are play-test gated
//! (implementation.md § Phase 4 acceptance, DoD I4).
//!
//! PROVEN ABLE TO FAIL (fail proof recorded): drop the stations.Clear() from either query's
//! nothing-found path and the matching rung goes red with the stale entry still present.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Fuel_SourceQueryEmptiesResults : SCR_AutotestCaseBase
{
	//! Far below any world's terrain, so no station's range can cover it.
	protected const vector NOWHERE = "0 -5000 0";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// A caller's array that already holds a result from a previous query.
		array<SCR_FuelSupportStationComponent> stations = {};
		stations.Insert(null);

		int found = OVT_FuelUtils.FindFuelSourcesCovering(NOWHERE, stations);
		if (found != 0 || stations.Count() != 0)
		{
			SetFailure("FindFuelSourcesCovering with nothing in range returned %1 and left %2 entries - it must report 0 AND empty the caller's array", found.ToString(), stations.Count().ToString());
			return true;
		}

		stations.Insert(null);

		found = OVT_FuelUtils.FindFuelSourcesNear(NOWHERE, 50, stations);
		if (found != 0 || stations.Count() != 0)
		{
			SetFailure("FindFuelSourcesNear with nothing in radius returned %1 and left %2 entries - stale results must never survive a query", found.ToString(), stations.Count().ToString());
			return true;
		}

		stations.Insert(null);

		found = OVT_FuelUtils.FindFuelSourcesNear(NOWHERE, 0, stations);
		if (found != 0 || stations.Count() != 0)
		{
			SetFailure("FindFuelSourcesNear with a zero radius returned %1 and left %2 entries - a search that cannot find anything still owes the caller an empty array", found.ToString(), stations.Count().ToString());
			return true;
		}

		Print("Both fuel-source queries report 0 and empty the caller's array when nothing is found");
		return true;
	}
}
