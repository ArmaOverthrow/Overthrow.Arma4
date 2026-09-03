//------------------------------------------------------------------------------------------------
//! TIER A cases - the pure High Command rules: cap arithmetic, stance validity, arrival, the
//! supporter draw-down and id minting. World-free by construction (every subject is `new`-built or
//! a static call); see the suite header for the tier rule.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! An exact fit is admitted; one member over is refused.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_HighCommandRules_CapAdmitsExactFit : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!OVT_HighCommandRules.FitsUnderCap(40, 8, 48))
		{
			SetFailure("A purchase that lands EXACTLY on the cap (40 + 8 = 48) was refused - an exact fit must be admitted");
			return true;
		}

		if (OVT_HighCommandRules.FitsUnderCap(40, 9, 48))
		{
			SetFailure("A purchase that would land ONE OVER the cap (40 + 9 = 49 against 48) was admitted");
			return true;
		}

		Print("High Command cap: an exact fit is admitted, one member over is refused");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! cap <= 0 admits anything - the operator's "unlimited" switch.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_HighCommandRules_CapZeroIsUnlimited : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!OVT_HighCommandRules.FitsUnderCap(1000, 1000, 0))
		{
			SetFailure("A cap of 0 refused a huge purchase - 0 must mean unlimited, not 'no room at all'");
			return true;
		}

		if (!OVT_HighCommandRules.FitsUnderCap(1000, 1000, -5))
		{
			SetFailure("A negative cap refused a huge purchase - any cap <= 0 must mean unlimited");
			return true;
		}

		Print("High Command cap: cap <= 0 admits any purchase");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! An over-cap owner reports 0 remaining, never a negative number.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_HighCommandRules_RemainingCapacityNeverNegative : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (OVT_HighCommandRules.RemainingCapacity(60, 48) != 0)
		{
			SetFailure("An owner with 60 members against a 48 cap reported %1 remaining, expected 0 - a floor over the cap must never go negative", OVT_HighCommandRules.RemainingCapacity(60, 48).ToString());
			return true;
		}

		if (OVT_HighCommandRules.RemainingCapacity(40, 48) != 8)
		{
			SetFailure("An owner with 40 members against a 48 cap reported %1 remaining, expected 8", OVT_HighCommandRules.RemainingCapacity(40, 48).ToString());
			return true;
		}

		if (OVT_HighCommandRules.RemainingCapacity(1000000, 0) != int.MAX)
		{
			SetFailure("An unlimited cap (0) reported %1 remaining, expected int.MAX", OVT_HighCommandRules.RemainingCapacity(1000000, 0).ToString());
			return true;
		}

		Print("High Command cap: remaining capacity floors at 0 over the cap and reports int.MAX when unlimited");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Only the three real stances validate; one below and one above the range are refused.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_HighCommandRules_StanceValidationRejectsOutOfRange : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (OVT_HighCommandRules.IsStanceValid(-1))
		{
			SetFailure("Stance -1 validated - one below DEFEND must be refused");
			return true;
		}

		if (OVT_HighCommandRules.IsStanceValid(OVT_EHighCommandStance.ATTACK + 1))
		{
			SetFailure("Stance ATTACK+1 validated - one above the last real stance must be refused");
			return true;
		}

		if (!OVT_HighCommandRules.IsStanceValid(OVT_EHighCommandStance.DEFEND))
		{
			SetFailure("DEFEND did not validate");
			return true;
		}

		if (!OVT_HighCommandRules.IsStanceValid(OVT_EHighCommandStance.PATROL))
		{
			SetFailure("PATROL did not validate");
			return true;
		}

		if (!OVT_HighCommandRules.IsStanceValid(OVT_EHighCommandStance.ATTACK))
		{
			SetFailure("ATTACK did not validate");
			return true;
		}

		Print("High Command stance: DEFEND/PATROL/ATTACK validate, -1 and ATTACK+1 are refused");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! "Moving" is decided by SQUARED distance against the arrival radius, asserted with a comfortable
//! margin on each side of the boundary rather than an exact equality - vector.Distance is not
//! correctly rounded, so an exact-boundary decision would be a coin flip.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_HighCommandRules_MovingIsSquaredDistance : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// Off-origin on purpose, at a scale where vector.Distance's own rounding would matter if the
		// implementation compared unsquared distance against a squared radius.
		vector destination = Vector(1000, 0, 500);
		vector justInside = destination + Vector(24.9, 0, 0);
		vector justOutside = destination + Vector(25.1, 0, 0);

		if (OVT_HighCommandRules.IsMoving(justInside, destination))
		{
			SetFailure("A position 24.9 m from a 25 m arrival radius counted as MOVING");
			return true;
		}

		if (!OVT_HighCommandRules.IsMoving(justOutside, destination))
		{
			SetFailure("A position 25.1 m from a 25 m arrival radius did NOT count as moving");
			return true;
		}

		if (OVT_HighCommandRules.IsMoving(destination, destination))
		{
			SetFailure("A group standing exactly on its destination counted as moving");
			return true;
		}

		Print("High Command arrival: inside the radius is not moving, outside it is");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Supporters scale linearly with member count; a non-positive rate never goes negative.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_HighCommandRules_SupportersScaleWithMembers : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (OVT_HighCommandRules.SupportersRequired(7, 2) != 14)
		{
			SetFailure("7 members at 2 supporters each required %1, expected 14", OVT_HighCommandRules.SupportersRequired(7, 2).ToString());
			return true;
		}

		if (OVT_HighCommandRules.SupportersRequired(7, 0) != 0)
		{
			SetFailure("A supporter rate of 0 required %1, expected 0", OVT_HighCommandRules.SupportersRequired(7, 0).ToString());
			return true;
		}

		if (OVT_HighCommandRules.SupportersRequired(7, -3) != 0)
		{
			SetFailure("A negative supporter rate required %1, expected 0 - never negative", OVT_HighCommandRules.SupportersRequired(7, -3).ToString());
			return true;
		}

		if (OVT_HighCommandRules.SupportersRequired(0, 5) != 0)
		{
			SetFailure("Zero members required %1 supporters, expected 0", OVT_HighCommandRules.SupportersRequired(0, 5).ToString());
			return true;
		}

		Print("High Command supporters: scale linearly with members and never go negative");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A minted id embeds the owner, and two different salts never collide.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_HighCommandRules_GroupIdIsStableAndUnique : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string ownerId = "steam-uid-778899";
		int unixTime = 1700000000;

		string idOne = OVT_HighCommandRules.MintGroupId(ownerId, unixTime, 1);
		string idTwo = OVT_HighCommandRules.MintGroupId(ownerId, unixTime, 2);

		if (idOne.IndexOf(ownerId) == -1)
		{
			SetFailure("Minted id '%1' does not embed the owner id '%2'", idOne, ownerId);
			return true;
		}

		if (idOne == idTwo)
		{
			SetFailure("Two different salts (1 and 2) minted the identical id '%1' - a real purchase and a converted group at the same second would collide", idOne);
			return true;
		}

		Print(string.Format("High Command group id: '%1' embeds the owner and differs by salt from '%2'", idOne, idTwo));
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A destination must be a real, finite, non-zero position.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_HighCommandRules_DestinationLegality : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (OVT_HighCommandRules.IsDestinationLegal(vector.Zero))
		{
			SetFailure("The zero vector validated as a legal order destination");
			return true;
		}

		// A normalised zero-length vector is NaN in this engine (the OVT_TEST_Logic_ObjectiveInsertion
		// degenerate-line precedent) - reachable in play whenever a caller derives a destination from a
		// direction rather than authoring one directly.
		vector nanDestination = vector.Zero.Normalized();
		if (OVT_HighCommandRules.IsDestinationLegal(nanDestination))
		{
			SetFailure("A NaN destination validated as legal");
			return true;
		}

		vector infiniteDestination = Vector(float.INFINITY, 0, 0);
		if (OVT_HighCommandRules.IsDestinationLegal(infiniteDestination))
		{
			SetFailure("An infinite destination validated as legal");
			return true;
		}

		if (!OVT_HighCommandRules.IsDestinationLegal(Vector(1500, 0, 2200)))
		{
			SetFailure("An ordinary, finite, non-zero position was refused as an order destination");
			return true;
		}

		Print("High Command destination legality: zero, NaN and infinite positions refused; an ordinary position accepted");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! R7: the vehicle is charged, at full price, on top of the crew - a technical must never cost the
//! same as the two-man team it is built from.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_HighCommandRules_VehicleIsChargedOnTopOfTheCrew : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		int footTotal = OVT_HighCommandRules.PurchaseTotal(900, 0);
		if (footTotal != 900)
		{
			SetFailure("A FOOT group (no vehicle) was priced at %1, expected the crew total of 900 unchanged", footTotal.ToString());
			return true;
		}

		int technicalTotal = OVT_HighCommandRules.PurchaseTotal(900, 27500);
		if (technicalTotal != 28400)
		{
			SetFailure("The same crew WITH a 27500 vehicle was priced at %1, expected 28400 - the vehicle must be added to the total at its full shop buy price, not folded into the gear fee and not dropped", technicalTotal.ToString());
			return true;
		}

		if (technicalTotal <= footTotal)
		{
			SetFailure("A vehicle group (%1) did not cost more than the same crew on foot (%2) - m_sVehiclePrefab is contributing nothing to the price", technicalTotal.ToString(), footTotal.ToString());
			return true;
		}

		if (OVT_HighCommandRules.PurchaseTotal(900, -500) != 900)
		{
			SetFailure("A negative vehicle price DISCOUNTED the group to %1 - an unpriceable vehicle must cost 0, never a refund", OVT_HighCommandRules.PurchaseTotal(900, -500).ToString());
			return true;
		}

		Print("High Command pricing: the vehicle is added to the crew total at full price, a foot group is unchanged, and a bad price never discounts");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! R4: the status heartbeat is adaptive - a travelling group is read on every sweep tick, a parked
//! one only on the slow cadence.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_HighCommandRules_StatusReadCadenceIsAdaptive : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		vector here = "0 0 0";
		vector faraway = "400 0 0";

		// Travelling: due on every one of the five ticks in an idle period.
		for (int tick = 1; tick <= 5; tick++)
		{
			if (!OVT_HighCommandRules.IsStatusReadDue(here, faraway, tick, 5))
			{
				SetFailure("A group 400 m from its destination was NOT due a status read on sweep tick %1 - a travelling group must be read on every tick or its map marker crawls", tick.ToString());
				return true;
			}
		}

		// Parked: silent for four ticks, read on the fifth.
		for (int idle = 1; idle <= 4; idle++)
		{
			if (OVT_HighCommandRules.IsStatusReadDue(here, here, idle, 5))
			{
				SetFailure("A PARKED group was read on sweep tick %1 of 5 - a group standing still must stay on the cheap cadence, which is what keeps a settled campaign silent", idle.ToString());
				return true;
			}
		}

		if (!OVT_HighCommandRules.IsStatusReadDue(here, here, 5, 5))
		{
			SetFailure("A parked group was never read at all - it must still be measured once every idle period, or a member killed beside it is never noticed");
			return true;
		}

		if (!OVT_HighCommandRules.IsStatusReadDue(here, here, 3, 1))
		{
			SetFailure("An idle period of 1 tick did not read a parked group every tick - the adaptive gate must degrade to 'always' rather than to 'never'");
			return true;
		}

		Print("High Command heartbeat: a travelling group is read every sweep tick, a parked one every fifth");
		return true;
	}
}
