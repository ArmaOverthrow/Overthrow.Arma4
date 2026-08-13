//------------------------------------------------------------------------------------------------
//! TIER A cases - the respawn eligibility predicates.
//!
//! OVT_RespawnService is deliberately split into a pure half and a manager-reading half, and this
//! file is the reason the split exists: the predicates below take only values and return only
//! values, so they are the ONLY part of map/respawn that any automated tier can see. The death
//! path needs players, the screen needs a UI tier and the enumeration needs live managers - none of
//! which exist here. Everything else about this feature is gated by play-testing, so these ten
//! cases are worth writing precisely, not broadly.
//!
//! WHAT THEY PIN:
//!  - a base held by the occupying faction is refused and any other base is offered, which is the
//!    single rule that stops the respawn screen listing the enemy's own bases;
//!  - a private camp belongs to its owner and a public one to everybody, in all three combinations;
//!  - a house is respawnable by its owner OR its renter, and by nobody else;
//!  - THE EMPTY-ID CASE. An unresolved persistent id is "" and an unowned record's owner field is
//!    also "", so a naive equality test would hand every house in the world to a player whose id
//!    failed to resolve. That failure is silent in the world - it looks like a working screen - so
//!    it is pinned here instead;
//!  - QRF exclusion is inert while no QRF runs, at any distance, and is a hard radius when one does;
//!  - the position match is a tolerance, not an equality, and a small one: it is the whole
//!    anti-arbitrary-position rule, so its boundary is asserted on both sides;
//!  - every non-OK result code has something to say to the player, and OK says nothing.
//!
//! FLOAT DISCIPLINE. Both radius cases are asserted a whole metre either side of the boundary
//! rather than exactly on it. A metre of QRF radius or two centimetres of match tolerance is not a
//! behaviour anybody can observe, and asserting the exact edge would pin one compiler's float unit
//! in the last place rather than the design. The radii themselves are read from the constants the
//! production code reads, so a change to either is a change to these cases and not a silent drift.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Case 1 - a base is respawnable exactly when the occupying faction does not hold it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RespawnRules_Base : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (OVT_RespawnService.IsBaseEligible(true))
		{
			SetFailure("A base held by the occupying faction was offered as a respawn point");
			return true;
		}

		if (!OVT_RespawnService.IsBaseEligible(false))
		{
			SetFailure("A base NOT held by the occupying faction was refused as a respawn point");
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 2 - a public camp is respawnable by anybody, including somebody who is not its owner.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RespawnRules_CampPublic : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!OVT_RespawnService.IsCampEligible(false, "someone-else", "me"))
		{
			SetFailure("A PUBLIC camp owned by another player was refused; public camps are shared");
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 3 - a private camp is not respawnable by anybody but its owner.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RespawnRules_CampPrivateForeign : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (OVT_RespawnService.IsCampEligible(true, "someone-else", "me"))
		{
			SetFailure("A PRIVATE camp owned by another player was offered as a respawn point");
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 4 - a private camp IS respawnable by its own owner.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RespawnRules_CampPrivateOwn : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!OVT_RespawnService.IsCampEligible(true, "me", "me"))
		{
			SetFailure("A player's OWN private camp was refused as a respawn point");
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 5 - a house is respawnable by its owner or its renter, and by nobody else.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RespawnRules_HouseTenure : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// Owned, not rented. This is the shape the map's house records carry: the field that does not
		// apply is empty, which is exactly why the empty-id case next door matters.
		if (!OVT_RespawnService.IsHouseEligible("me", "", "me"))
		{
			SetFailure("A house the player OWNS was refused as a respawn point");
			return true;
		}

		// Rented, not owned.
		if (!OVT_RespawnService.IsHouseEligible("", "me", "me"))
		{
			SetFailure("A house the player RENTS was refused as a respawn point");
			return true;
		}

		// Somebody else's, both ways.
		if (OVT_RespawnService.IsHouseEligible("them", "them", "me"))
		{
			SetFailure("A house owned and rented by another player was offered as a respawn point");
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 6 - THE EMPTY-ID CASE. An unresolved player id must not match an unowned record.
//!
//! This is the one case in the file that pins a failure mode rather than a rule. When the local
//! persistent id cannot be resolved it is "", and an unowned, unrented house record also carries ""
//! in both fields. A plain equality test therefore returns TRUE for every such house, and the
//! symptom is not an error: it is a respawn screen full of strangers' property, or - with the
//! filter the other way round - an empty screen with working-looking buttons. Neither prints
//! anything. Only an assertion can hold this.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RespawnRules_HouseEmptyIds : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (OVT_RespawnService.IsHouseEligible("", "", ""))
		{
			SetFailure("An UNRESOLVED player id matched an UNOWNED house - every house in the world would be offered as a respawn point");
			return true;
		}

		// And the same trap one field at a time: an unresolved id must not inherit an owner's or a
		// renter's entitlement just because the other field happens to be empty.
		if (OVT_RespawnService.IsHouseEligible("them", "", ""))
		{
			SetFailure("An unresolved player id matched a house owned by somebody else with no renter");
			return true;
		}

		if (OVT_RespawnService.IsHouseEligible("", "them", ""))
		{
			SetFailure("An unresolved player id matched a house rented by somebody else with no owner");
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 7 - with no QRF running, nothing is inside one, at any distance.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RespawnRules_QrfInactive : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// Exactly on top of the recorded QRF centre - the most tempting position to exclude, and still
		// not excluded, because no QRF is running.
		if (OVT_RespawnService.IsInsideQrf(false, vector.Zero, vector.Zero))
		{
			SetFailure("A position on a STALE QRF centre was excluded while no QRF was running");
			return true;
		}

		// And far away, so the case reads as "the flag decides", not "the distance happened to save us".
		vector distant = Vector(100000, 0, 100000);
		if (OVT_RespawnService.IsInsideQrf(false, vector.Zero, distant))
		{
			SetFailure("A distant position was excluded while no QRF was running");
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 8 - a running QRF excludes everything inside its radius and nothing outside it.
//! The radius is read from the constant the production code reads, not written out here.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RespawnRules_QrfRadius : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		float range = OVT_QRFControllerComponent.QRF_RANGE;

		vector justInside = Vector(range - 1, 0, 0);
		if (!OVT_RespawnService.IsInsideQrf(true, vector.Zero, justInside))
		{
			SetFailure("A position one metre INSIDE the QRF radius was not excluded");
			return true;
		}

		vector justOutside = Vector(range + 1, 0, 0);
		if (OVT_RespawnService.IsInsideQrf(true, vector.Zero, justOutside))
		{
			SetFailure("A position one metre OUTSIDE the QRF radius was excluded");
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 9 - the position match is a small tolerance, asserted on both sides.
//!
//! This comparison is the anti-arbitrary-position rule: it is the only thing standing between a
//! crafted vector and a spawn at that vector. It must be forgiving enough to survive a float's
//! round trip through a map record and the wire, and tight enough that "near enough" can only ever
//! mean the same place.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RespawnRules_PositionMatch : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		vector origin = Vector(1000, 20, 2000);

		if (!OVT_RespawnService.PositionsMatch(origin, origin))
		{
			SetFailure("A position did not match ITSELF");
			return true;
		}

		float tolerance = OVT_RespawnService.MATCH_TOLERANCE;

		vector justInside = origin + Vector(tolerance - 0.1, 0, 0);
		if (!OVT_RespawnService.PositionsMatch(origin, justInside))
		{
			SetFailure("A position just INSIDE the match tolerance did not match - a legitimate pick would fall back to home");
			return true;
		}

		vector justOutside = origin + Vector(tolerance + 0.1, 0, 0);
		if (OVT_RespawnService.PositionsMatch(origin, justOutside))
		{
			SetFailure("A position just OUTSIDE the match tolerance matched - the tolerance is not bounding anything");
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 10 - every result code the player can be shown has something to say, and OK says nothing.
//! Silence on a refusal is indistinguishable from a request that never arrived, which is the
//! failure this vocabulary exists to prevent.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RespawnRules_ReasonKeys : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string okKey = OVT_RespawnService.ReasonKeyFor(OVT_RespawnResult.OK);
		if (!okKey.IsEmpty())
		{
			SetFailure("A successful respawn produced the refusal text '%1'; success must say nothing", okKey);
			return true;
		}

		if (!ExpectSpoken(OVT_RespawnResult.OK_FELL_BACK_HOME, "OK_FELL_BACK_HOME"))
			return true;

		if (!ExpectSpoken(OVT_RespawnResult.NO_PLAYER, "NO_PLAYER"))
			return true;

		if (!ExpectSpoken(OVT_RespawnResult.NOT_ELIGIBLE, "NOT_ELIGIBLE"))
			return true;

		if (!ExpectSpoken(OVT_RespawnResult.SPAWN_FAILED, "SPAWN_FAILED"))
			return true;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts that one non-OK result code produces a non-empty localization key.
	//! \param[in] result The OVT_RespawnResult code to look up.
	//! \param[in] name The code's name, for the failure message.
	//! \return True when the code has a key; false after recording a failure.
	protected bool ExpectSpoken(int result, string name)
	{
		string key = OVT_RespawnService.ReasonKeyFor(result);
		if (key.IsEmpty())
		{
			SetFailure("Result code %1 produced no text at all; a refused respawn must always say something", name);
			return false;
		}

		return true;
	}
}
