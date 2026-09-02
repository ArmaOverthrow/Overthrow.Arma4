//------------------------------------------------------------------------------------------------
//! TIER A - vehicle rearm's pure decisions (OVT_VehicleRearmRules). Every subject is a static
//! function of plain numbers and strings, per the tier rule in OVT_TEST_LogicSuite.c - including
//! the reviewer grep, which does not distinguish code from comments, so no banned identifier
//! appears anywhere below, prose included.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! ResolveAmmoPrefab: the muzzle-default, then the loaded prefab, then the rocket prefab, first
//! non-empty wins - never "most specific" or "last wins".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_VehicleRearm_ResolveAmmoPrefab : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// Only the muzzle default is set: it wins.
		string result = OVT_VehicleRearmRules.ResolveAmmoPrefab("MuzzleAmmo", "", "");
		if (result != "MuzzleAmmo")
		{
			SetFailure(string.Format("Only a muzzle default set answered '%1', expected 'MuzzleAmmo'", result));
			return true;
		}

		// All three set: the muzzle default STILL wins - the order is fixed, not "most specific".
		result = OVT_VehicleRearmRules.ResolveAmmoPrefab("MuzzleAmmo", "LoadedAmmo", "RocketAmmo");
		if (result != "MuzzleAmmo")
		{
			SetFailure(string.Format("All three set answered '%1', expected the muzzle default 'MuzzleAmmo' to win", result));
			return true;
		}

		// No muzzle default: the loaded prefab is next, even with a rocket prefab present.
		result = OVT_VehicleRearmRules.ResolveAmmoPrefab("", "LoadedAmmo", "RocketAmmo");
		if (result != "LoadedAmmo")
		{
			SetFailure(string.Format("No muzzle default answered '%1', expected the loaded prefab 'LoadedAmmo'", result));
			return true;
		}

		// Neither the muzzle default nor the loaded prefab: the rocket prefab is last resort.
		result = OVT_VehicleRearmRules.ResolveAmmoPrefab("", "", "RocketAmmo");
		if (result != "RocketAmmo")
		{
			SetFailure(string.Format("Only a rocket prefab set answered '%1', expected 'RocketAmmo'", result));
			return true;
		}

		// All three empty: an unresolvable weapon, correctly reported as "", never a placeholder.
		result = OVT_VehicleRearmRules.ResolveAmmoPrefab("", "", "");
		if (result != "")
		{
			SetFailure(string.Format("All three empty answered '%1', expected \"\" - an unresolvable weapon must never invent a prefab", result));
			return true;
		}

		Print("Vehicle rearm ammo prefab: muzzle default beats loaded beats rocket beats nothing, in that fixed order");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! ProratedCost: free when fully covered, full price when nothing is covered, and pro rata in
//! between - asserted through an explicit int/float boundary so an int/int truncation cannot pass.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_VehicleRearm_ProratedCost : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- 0% uncovered (fully covered by the ledgers): free.
		int cost = OVT_VehicleRearmRules.ProratedCost(1000, 0, 10);
		if (cost != 0)
		{
			SetFailure(string.Format("Zero uncovered units cost %1, expected 0 (fully covered rearm is free)", cost.ToString()));
			return true;
		}

		// A defensive negative reading of "uncovered" must also be free, not negative money.
		cost = OVT_VehicleRearmRules.ProratedCost(1000, -1, 10);
		if (cost != 0)
		{
			SetFailure(string.Format("A negative uncovered count cost %1, expected 0", cost.ToString()));
			return true;
		}

		// --- total <= 0: always free, whatever uncovered says - a weapon-less vehicle costs nothing.
		cost = OVT_VehicleRearmRules.ProratedCost(1000, 5, 0);
		if (cost != 0)
		{
			SetFailure(string.Format("A total of zero units cost %1, expected 0", cost.ToString()));
			return true;
		}

		cost = OVT_VehicleRearmRules.ProratedCost(1000, 5, -1);
		if (cost != 0)
		{
			SetFailure(string.Format("A negative total cost %1, expected 0", cost.ToString()));
			return true;
		}

		// --- 100% uncovered: the full, unscaled price - today's behaviour, exactly.
		cost = OVT_VehicleRearmRules.ProratedCost(1000, 10, 10);
		if (cost != 1000)
		{
			SetFailure(string.Format("Every unit uncovered cost %1, expected the full 1000", cost.ToString()));
			return true;
		}

		// Uncovered reported greater than total (corrupt input) must still cap at the full price,
		// never scale past it.
		cost = OVT_VehicleRearmRules.ProratedCost(1000, 15, 10);
		if (cost != 1000)
		{
			SetFailure(string.Format("More uncovered than total cost %1, expected the full 1000, never scaled past it", cost.ToString()));
			return true;
		}

		// --- 50% uncovered: exactly half.
		cost = OVT_VehicleRearmRules.ProratedCost(1000, 5, 10);
		if (cost != 500)
		{
			SetFailure(string.Format("Half the units uncovered cost %1, expected 500", cost.ToString()));
			return true;
		}

		// --- THE INT/FLOAT BOUNDARY. 100 * 2 = 200 (int), and 200 / 3 truncates to 66 in a pure int
		// expression, but 200 / 3.0 rounds to 67. An int/int implementation of this rule would silently
		// undercharge by a dollar here; only an explicit float divisor answers 67.
		cost = OVT_VehicleRearmRules.ProratedCost(100, 2, 3);
		if (cost != 67)
		{
			SetFailure(string.Format("Two of three units uncovered on a 100 price cost %1, expected 67 - an int/int division here would truncate to 66 and undercharge", cost.ToString()));
			return true;
		}

		// --- THE ROUND-TO-ZERO CLAMP. 1 * 1 / 1000.0 rounds to 0, but a partially uncovered rearm
		// must never be free - the minimum charge is 1.
		cost = OVT_VehicleRearmRules.ProratedCost(1, 1, 1000);
		if (cost != 1)
		{
			SetFailure(string.Format("A rearm that rounds to zero cost %1, expected the floor of 1 - a partial rearm must never be free", cost.ToString()));
			return true;
		}

		Print("Vehicle rearm prorated cost: free when fully covered, full price when fully uncovered, pro rata between with an explicit float divisor, and floored at 1");

		return true;
	}
}
