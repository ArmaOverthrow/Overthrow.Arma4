//------------------------------------------------------------------------------------------------
//! TIER A cases - the campaign's spatial-influence rule set.
//!
//! OVT_InfluenceRules was extracted from the code that already applied these rules so that the
//! server which decides a support modifier and the map layer which draws the relation cannot drift
//! apart. It fetches nothing, so every case below is plain values and static calls - no subject to
//! build, no state to arrange, nothing that could reach a manager or the world, and per this
//! directory's tier rule no comment here names the accessors that would.
//!
//! These ten cases are the entire automated surface of the influence overlay. Everything else about
//! that feature - lines, dashes, colours, alpha, the range ring, frame cost, and every multiplayer
//! behaviour - is play-tested, which is exactly why the rules were split out into something that
//! is not.
//!
//! WHAT THEY PIN, and why each one is worth a case of its own:
//!  - THE TWO RANGE BOUNDARIES, WHICH DISAGREE ON PURPOSE. Proximity is strictly less-than and
//!    momentum is inclusive. They look near-identical and a later reader will be tempted to make
//!    them match; a case sits on each boundary so that doing so fails loudly instead of quietly
//!    moving modifiers on and off towns in live saves.
//!  - THAT THE PROXIMITY TEST IS THREE-DIMENSIONAL. A plan-view distance is the single most
//!    plausible alternative implementation - the engine ships one, one call away by name - and on
//!    hilly ground it silently widens the reach of every tower and base.
//!  - ENEMY WINS OVER FRIENDLY. The rule the whole overlay exists to make legible, and the one
//!    outcome a player can observe today only as an unexplained negative modifier.
//!  - THE FIVE MODIFIER ID STRINGS, CHARACTER FOR CHARACTER. They are resolved by name against the
//!    support modifier config, so a rename in one place and not the other does not error: the
//!    campaign simply stops applying a modifier. The names are asserted here as LITERALS rather
//!    than against the constants they came from, because a case that compares a constant to itself
//!    cannot see a typo.
//!  - THE MOMENTUM REACH, which moved out of a protected const on the modifier class and is now
//!    shared. Two cases are sensitive to its value, deliberately.
//!
//! FLOAT DISCIPLINE, AND ONE MEASURED ENGINE FACT THE BOUNDARY CASES ARE BUILT AROUND. The one
//! float compared for equality here is compared with an epsilon. More importantly: the engine's
//! distance is NOT a correctly-rounded square root. Measured on this build it returns the exact
//! value for separations of 1, 5, 100, 1024, 1500 and 2048 m, and one unit in the last place HIGH
//! for 1000 m and 2000 m. A boundary case built on a separation it rounds up is decided by the
//! arithmetic before the comparison is reached, and would pass with either operator while appearing
//! to pin one - so the proximity boundary is asserted at 1500 m, and the momentum boundary, whose
//! reach is a constant 2000 and cannot be moved, is asserted through the comparison itself.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Readable renderings for the failure messages, so a wrong answer names itself instead of printing
//! an integer the reader has to decode.
//------------------------------------------------------------------------------------------------
class OVT_TEST_Logic_InfluenceFixture
{
	//------------------------------------------------------------------------------------------------
	//! \param[in] polarity The polarity to name.
	//! \return "NEGATIVE", "POSITIVE", "NONE", or "<unknown>" for a value added later.
	static string PolarityName(OVT_InfluencePolarity polarity)
	{
		if (polarity == OVT_InfluencePolarity.NEGATIVE)
			return "NEGATIVE";

		if (polarity == OVT_InfluencePolarity.POSITIVE)
			return "POSITIVE";

		if (polarity == OVT_InfluencePolarity.NONE)
			return "NONE";

		return "<unknown>";
	}
}

//------------------------------------------------------------------------------------------------
//! Case 1 - a source INSIDE the range reaches the town, and one well outside it does not.
//!
//! The baseline the other proximity cases are read against. It cannot tell a strict boundary from an
//! inclusive one and it cannot tell a 3D distance from a plan-view one - that is what the next two
//! cases are for - but it is what catches a predicate that has been inverted, that ignores its range
//! argument, or that always answers the same way.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Influence_ProximityInRange : SCR_AutotestCaseBase
{
	//! The reach a tower or base is asked about. Any value works; a round one keeps the arithmetic
	//! in the failure messages readable.
	protected static const float RANGE = 1000;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		vector town = Vector(0, 0, 0);

		// One metre inside. Flat, so this case says nothing about the height component either way.
		if (!OVT_InfluenceRules.IsProximitySource(town, Vector(999, 0, 0), RANGE))
		{
			SetResultFailure("A source 999 m from a town did not reach it at a range of %1 m; every tower and base would stop influencing anything", RANGE.ToString());
			return true;
		}

		// Comfortably inside, so a fault at the boundary cannot be what turned this green.
		if (!OVT_InfluenceRules.IsProximitySource(town, Vector(500, 0, 0), RANGE))
		{
			SetResultFailure("A source halfway to the range boundary did not reach the town");
			return true;
		}

		// And the range argument is actually consulted rather than ignored.
		if (OVT_InfluenceRules.IsProximitySource(town, Vector(1500, 0, 0), RANGE))
		{
			SetResultFailure("A source 1500 m from a town reached it at a range of %1 m; the range argument is not bounding anything", RANGE.ToString());
			return true;
		}

		// The same pair at a wider range does reach, which is the other half of "the argument matters".
		if (!OVT_InfluenceRules.IsProximitySource(town, Vector(1500, 0, 0), 2000))
		{
			SetResultFailure("A source 1500 m from a town did not reach it at a range of 2000 m");
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 2 - a source sitting EXACTLY ON the range boundary does NOT reach the town.
//!
//! The proximity range is an OPEN interval, reproducing the comparison the campaign already shipped.
//! One metre of difference on one town is nothing; the reason it is pinned is that this boundary and
//! the momentum one below are the two halves of an asymmetry that looks like an oversight. Anyone
//! who "fixes" either to match the other moves modifiers on and off towns on every live save, and
//! this is the case that stops them.
//!
//! THE RANGE IS 1500 m FOR A MEASURED REASON, and it is not interchangeable with a rounder number.
//! The engine's distance is not a correctly-rounded square root: for two points separated by exactly
//! 1000 m or exactly 2000 m it answers one unit in the last place HIGH, so a boundary case built on
//! either of those separations is rejected by the arithmetic before the comparison is even reached -
//! it would pass whether the operator were strict or inclusive, and would pin nothing. 1500 m was
//! measured to come back exact (as do 1, 5, 100, 1024 and 2048), so here the operator alone decides
//! the answer. Change this number and check the same way before trusting the result.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Influence_ProximityAtRange : SCR_AutotestCaseBase
{
	//! A range whose boundary separation the engine measures EXACTLY - see the note above.
	protected static const float RANGE = 1500;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		vector town = Vector(0, 0, 0);

		if (OVT_InfluenceRules.IsProximitySource(town, Vector(1500, 0, 0), RANGE))
		{
			SetResultFailure("A source sitting exactly %1 m away reached a town at a range of %1 m; the proximity range is an open interval and the campaign's comparison is strict", RANGE.ToString());
			return true;
		}

		// The mirror: one metre closer and it does reach. Without this half, a predicate that always
		// answered false would pass the assertion above.
		if (!OVT_InfluenceRules.IsProximitySource(town, Vector(1499, 0, 0), RANGE))
		{
			SetResultFailure("A source one metre inside the boundary did not reach the town; the boundary has moved rather than being strict");
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 3 - THE PROXIMITY TEST IS THREE-DIMENSIONAL. A source that is in range across the ground but
//! out of range once its height is counted does NOT reach.
//!
//! THIS IS THE CASE THAT TELLS A 3D DISTANCE FROM A PLAN-VIEW ONE, and it exists because the engine
//! ships both under names one word apart. Radio towers in particular sit on high ground, so a
//! plan-view implementation would widen the effective reach of exactly the sources that matter most,
//! by an amount that varies with the terrain and therefore never looks like a constant to anyone
//! debugging it.
//!
//! The control half is what makes the assertion mean something: the SAME horizontal offset at ground
//! level does reach, so the only thing that rejected the raised source was its height.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Influence_ProximityIsThreeDimensional : SCR_AutotestCaseBase
{
	protected static const float RANGE = 1000;

	//! 900 m across the ground and 600 m up is 1081 m in three dimensions - comfortably outside a
	//! 1000 m range, and comfortably inside it if the height is dropped.
	protected static const float HORIZONTAL = 900;
	protected static const float HEIGHT = 600;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		vector town = Vector(0, 0, 0);

		// The control. Same horizontal offset, no height: in range, so the rejection below cannot be
		// blamed on the horizontal distance.
		if (!OVT_InfluenceRules.IsProximitySource(town, Vector(HORIZONTAL, 0, 0), RANGE))
		{
			SetResultFailure("A source %1 m away at ground level did not reach a town at a range of %2 m; the control for this case is broken and it can no longer say anything about height", HORIZONTAL.ToString(), RANGE.ToString());
			return true;
		}

		if (OVT_InfluenceRules.IsProximitySource(town, Vector(HORIZONTAL, HEIGHT, 0), RANGE))
		{
			SetResultFailure("A source %1 m away across the ground and %2 m above the town reached it at a range of %3 m; that is 1081 m in three dimensions, so the distance is being measured on the map rather than in the world", HORIZONTAL.ToString(), HEIGHT.ToString(), RANGE.ToString());
			return true;
		}

		// And height counts in the other direction too, so nothing can pass by treating a positive Y
		// as a special case.
		if (OVT_InfluenceRules.IsProximitySource(town, Vector(HORIZONTAL, -HEIGHT, 0), RANGE))
		{
			SetResultFailure("A source %1 m away across the ground and %2 m BELOW the town reached it at a range of %3 m; height must count whichever way it goes", HORIZONTAL.ToString(), HEIGHT.ToString(), RANGE.ToString());
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 4 - momentum reaches a town sitting EXACTLY at the momentum range. INCLUSIVE, unlike the
//! strict proximity test two cases up.
//!
//! THE BOUNDARY IS ASSERTED THROUGH THE COMPARISON, NOT THROUGH TWO POSITIONS, and that is a
//! measured necessity rather than a preference. The engine's distance answers one unit in the last
//! place HIGH for a separation of exactly 2000 m, and no arrangement of world points makes it answer
//! exactly 2000 - so through the two-position form this comparison being inclusive or strict is
//! literally the same function, and a case built that way would pin nothing while looking like it
//! pinned everything. The comparison is therefore exposed on its own and asserted directly. Case 2
//! needs no such treatment because its range is a PARAMETER and it can pick one the engine measures
//! exactly; this reach is a constant and 2000 is not such a value.
//!
//! The distances are written as LITERAL METRES rather than derived from the shared constant, so that
//! the reach moving is caught here as well as in the case that pins the constant on its own.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Influence_MomentumAtRange : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		// --- THE BOUNDARY ITSELF. Inclusive: a town sitting exactly at the reach is inside it.
		if (!OVT_InfluenceRules.IsWithinMomentumRange(2000))
		{
			SetResultFailure("A separation of exactly 2000 m was outside the momentum reach; the momentum comparison is INCLUSIVE, unlike the strict proximity one, and that asymmetry is what these two cases exist to hold apart");
			return true;
		}

		// A hair past it is past it. Without this half the comparison could answer true for
		// everything and still satisfy the assertion above.
		if (OVT_InfluenceRules.IsWithinMomentumRange(2000.5))
		{
			SetResultFailure("A separation of 2000.5 m was inside the momentum reach; the reach is 2000 m");
			return true;
		}

		// --- AND THE TWO-POSITION FORM AGREES, so the seam above is measuring the same rule the
		// campaign actually applies rather than a comparison nothing calls.
		vector town = Vector(0, 0, 0);

		if (!OVT_InfluenceRules.IsMomentumSource(town, Vector(1999, 0, 0)))
		{
			SetResultFailure("A player-held town 1999 m away lent no momentum");
			return true;
		}

		if (OVT_InfluenceRules.IsMomentumSource(town, Vector(2001, 0, 0)))
		{
			SetResultFailure("A player-held town 2001 m away lent momentum; the reach is 2000 m and nothing outside it may qualify");
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 5 - ENEMY WINS OVER FRIENDLY. With both an occupier-held and a resistance-held source of the
//! same kind in range of one town, the town gets the NEGATIVE modifier.
//!
//! THIS IS THE RULE THE INFLUENCE OVERLAY EXISTS TO MAKE LEGIBLE. Today a player can see the
//! negative modifier on the town panel and has no way to learn that a friendly source is in range
//! and being out-shouted; the overlay draws that friendly relation as a suppressed edge. If this
//! resolution ever flips, the campaign and the overlay would both flip with it and the picture would
//! remain internally consistent while being wrong - which is precisely the kind of fault no play-test
//! catches.
//!
//! The second assertion pins the rule as a COMPOSITION rather than as a coincidence: the outcome of
//! "enemy wins" is exactly the polarity that enemy source would produce on its own.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Influence_EnemyWinsOverFriendly : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_InfluencePolarity contested = OVT_InfluenceRules.ResolveProximity(true, true);

		if (contested != OVT_InfluencePolarity.NEGATIVE)
		{
			SetResultFailure("A town with BOTH an enemy and a friendly source of the same kind in range resolved to %1, expected NEGATIVE; an enemy source out-weighs any number of friendly ones", OVT_TEST_Logic_InfluenceFixture.PolarityName(contested));
			return true;
		}

		// An enemy alone resolves the same way, which is what makes the line above "the enemy won"
		// rather than "contested has its own answer".
		OVT_InfluencePolarity enemyOnly = OVT_InfluenceRules.ResolveProximity(true, false);

		if (enemyOnly != OVT_InfluencePolarity.NEGATIVE)
		{
			SetResultFailure("A town with only an enemy source in range resolved to %1, expected NEGATIVE", OVT_TEST_Logic_InfluenceFixture.PolarityName(enemyOnly));
			return true;
		}

		// And the contested outcome IS the winning source's own polarity, not a third answer that
		// happens to share its name.
		if (contested != OVT_InfluenceRules.PolarityForSource(true))
		{
			SetResultFailure("The contested outcome %1 is not the polarity an occupier-held source produces on its own (%2); 'the enemy wins' must mean the enemy's own answer is taken", OVT_TEST_Logic_InfluenceFixture.PolarityName(contested), OVT_TEST_Logic_InfluenceFixture.PolarityName(OVT_InfluenceRules.PolarityForSource(true)));
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 6 - the other two outcomes of the resolution: friendly alone is POSITIVE, and nothing in
//! range is NONE.
//!
//! NONE IS A REAL ANSWER, NOT AN ERROR. It is what the caller reads as "remove whatever modifier was
//! there", and it is the outcome a sabotaged radio tower produces - the tower is still standing and
//! still in range, but it broadcasts for nobody, so the town's proximity modifiers must go. Fold
//! NONE into POSITIVE and every town in reach of a dead tower starts gaining support.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Influence_ResolveFriendlyAndNone : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_InfluencePolarity friendly = OVT_InfluenceRules.ResolveProximity(false, true);

		if (friendly != OVT_InfluencePolarity.POSITIVE)
		{
			SetResultFailure("A town with only a friendly source in range resolved to %1, expected POSITIVE", OVT_TEST_Logic_InfluenceFixture.PolarityName(friendly));
			return true;
		}

		OVT_InfluencePolarity nothing = OVT_InfluenceRules.ResolveProximity(false, false);

		if (nothing != OVT_InfluencePolarity.NONE)
		{
			SetResultFailure("A town with no source of this kind in range resolved to %1, expected NONE; NONE is what tells the campaign to REMOVE the modifier, and a sabotaged tower produces it", OVT_TEST_Logic_InfluenceFixture.PolarityName(nothing));
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 7 - a single source's polarity: occupier-held is NEGATIVE, anything else is POSITIVE.
//!
//! The smallest rule in the set and the one every other answer is built out of. It is asserted on
//! its own so that a failure here reads as "the sign of influence is inverted" rather than surfacing
//! as four modifiers being applied the wrong way round in three different call sites.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Influence_PolarityForSource : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_InfluencePolarity occupier = OVT_InfluenceRules.PolarityForSource(true);

		if (occupier != OVT_InfluencePolarity.NEGATIVE)
		{
			SetResultFailure("An occupier-held source produced %1, expected NEGATIVE; occupation depresses a town's support", OVT_TEST_Logic_InfluenceFixture.PolarityName(occupier));
			return true;
		}

		OVT_InfluencePolarity resistance = OVT_InfluenceRules.PolarityForSource(false);

		if (resistance != OVT_InfluencePolarity.POSITIVE)
		{
			SetResultFailure("A source NOT held by the occupying faction produced %1, expected POSITIVE", OVT_TEST_Logic_InfluenceFixture.PolarityName(resistance));
			return true;
		}

		// The two answers must differ, which is the property that survives a later third polarity.
		if (occupier == resistance)
		{
			SetResultFailure("An occupier-held source and a resistance-held source produced the same polarity %1; the sign of influence would carry no information at all", OVT_TEST_Logic_InfluenceFixture.PolarityName(occupier));
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 8 - THE FIVE MODIFIER ID STRINGS, character for character, plus the combinations that map to
//! no modifier at all.
//!
//! These names are resolved against the support modifier config BY NAME at runtime. A rename that
//! lands in the config and not here - or here and not in the config - does not error and does not
//! log: the campaign simply stops applying that modifier, and the overlay starts asserting a
//! relation whose modifier can never be found. There is no gate other than this one.
//!
//! THE EXPECTED VALUES ARE LITERALS ON PURPOSE. Comparing the function's answer to the constant it
//! returns would assert nothing whatsoever; the strings below were read off the support modifier
//! config and are the only copy of them that is not the implementation.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Influence_ModifierNames : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		if (!Expect(OVT_InfluenceSourceKind.RADIO_TOWER, OVT_InfluencePolarity.NEGATIVE, "NearbyRadioTowerNegative", "a town in reach of an enemy radio tower"))
			return true;

		if (!Expect(OVT_InfluenceSourceKind.RADIO_TOWER, OVT_InfluencePolarity.POSITIVE, "NearbyRadioTowerPositive", "a town in reach of a friendly radio tower"))
			return true;

		if (!Expect(OVT_InfluenceSourceKind.MILITARY_BASE, OVT_InfluencePolarity.NEGATIVE, "NearbyBaseNegative", "a town in reach of an enemy base"))
			return true;

		if (!Expect(OVT_InfluenceSourceKind.MILITARY_BASE, OVT_InfluencePolarity.POSITIVE, "NearbyBasePositive", "a town in reach of a friendly base"))
			return true;

		if (!Expect(OVT_InfluenceSourceKind.MOMENTUM_TOWN, OVT_InfluencePolarity.POSITIVE, "RevolutionaryMomentum", "a town next to a liberated neighbour"))
			return true;

		// --- THE COMBINATIONS WITH NO MODIFIER return an empty name rather than inventing one.
		// Momentum has no negative form, and NONE is not a modifier at all - it is the answer that
		// tells a caller to remove one. An empty name is refused by the lookup it would be handed to,
		// so the failure mode of asking for a pair that does not exist is nothing happening.
		if (!ExpectEmpty(OVT_InfluenceSourceKind.MOMENTUM_TOWN, OVT_InfluencePolarity.NEGATIVE, "momentum has no negative form"))
			return true;

		if (!ExpectEmpty(OVT_InfluenceSourceKind.RADIO_TOWER, OVT_InfluencePolarity.NONE, "NONE is not a modifier"))
			return true;

		if (!ExpectEmpty(OVT_InfluenceSourceKind.MILITARY_BASE, OVT_InfluencePolarity.NONE, "NONE is not a modifier"))
			return true;

		if (!ExpectEmpty(OVT_InfluenceSourceKind.MOMENTUM_TOWN, OVT_InfluencePolarity.NONE, "NONE is not a modifier"))
			return true;

		// --- THE FIVE NAMES ARE FIVE DIFFERENT NAMES. A mapping that collapsed two pairs onto one
		// string would satisfy every assertion above only if the string were also wrong, but this is
		// cheap and it states the property directly.
		array<string> names = new array<string>();
		names.Insert(OVT_InfluenceRules.ModifierNameFor(OVT_InfluenceSourceKind.RADIO_TOWER, OVT_InfluencePolarity.NEGATIVE));
		names.Insert(OVT_InfluenceRules.ModifierNameFor(OVT_InfluenceSourceKind.RADIO_TOWER, OVT_InfluencePolarity.POSITIVE));
		names.Insert(OVT_InfluenceRules.ModifierNameFor(OVT_InfluenceSourceKind.MILITARY_BASE, OVT_InfluencePolarity.NEGATIVE));
		names.Insert(OVT_InfluenceRules.ModifierNameFor(OVT_InfluenceSourceKind.MILITARY_BASE, OVT_InfluencePolarity.POSITIVE));
		names.Insert(OVT_InfluenceRules.ModifierNameFor(OVT_InfluenceSourceKind.MOMENTUM_TOWN, OVT_InfluencePolarity.POSITIVE));

		for (int i = 0; i < names.Count(); i++)
		{
			for (int j = i + 1; j < names.Count(); j++)
			{
				if (names.Get(i) != names.Get(j))
					continue;

				SetResultFailure("Two different source/polarity pairs both map to '%1'; each of the five modifiers must be addressable on its own", names.Get(i));
				return true;
			}
		}

		SetResultSuccess();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one pair maps to one exact name.
	//! \param[in] kind The source kind.
	//! \param[in] polarity The polarity.
	//! \param[in] expected The name as authored in the support modifier config.
	//! \param[in] situation Plain-language description of the pair, for the failure message.
	//! \return True when the mapping is correct; false after recording the failure.
	protected bool Expect(OVT_InfluenceSourceKind kind, OVT_InfluencePolarity polarity, string expected, string situation)
	{
		string actual = OVT_InfluenceRules.ModifierNameFor(kind, polarity);

		if (actual == expected)
			return true;

		SetResultFailure("The modifier for %1 is '%2', expected '%3'. These names are resolved against the modifier config at runtime, so a mismatch does not error - the campaign silently stops applying it", situation, actual, expected);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one pair maps to no modifier at all.
	//! \param[in] kind The source kind.
	//! \param[in] polarity The polarity.
	//! \param[in] reason Why this pair has no modifier, for the failure message.
	//! \return True when the mapping is empty; false after recording the failure.
	protected bool ExpectEmpty(OVT_InfluenceSourceKind kind, OVT_InfluencePolarity polarity, string reason)
	{
		string actual = OVT_InfluenceRules.ModifierNameFor(kind, polarity);

		if (actual == string.Empty)
			return true;

		SetResultFailure("A source/polarity pair that has no modifier (%1) returned '%2'; inventing a name here would have the campaign apply a modifier nothing authored", reason, actual);
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 9 - a town the player ALREADY HOLDS is not a momentum target; a town held by anyone else is.
//!
//! Momentum models a neighbouring liberation pulling an occupied town along, so a town that has
//! already come across has nothing left to be pulled toward. Lose this and every liberated town
//! starts carrying a support modifier for being next to itself - the modifier would be applied to
//! towns that are already at the top of the scale and the campaign's own "skip if this town is
//! already resistance-controlled" step would have been quietly deleted.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Influence_MomentumTargetQualifies : SCR_AutotestCaseBase
{
	//! Stand-in faction indices. Their VALUES are arbitrary - only their equality matters - but they
	//! are written as two clearly different numbers so a failure message reads unambiguously.
	protected static const int PLAYER_FACTION = 2;
	protected static const int OCCUPIER_FACTION = 1;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		if (OVT_InfluenceRules.TownQualifiesForMomentum(PLAYER_FACTION, PLAYER_FACTION))
		{
			SetResultFailure("A town already held by the player qualified as a momentum TARGET; a town that has come across has nothing left to be pulled toward");
			return true;
		}

		if (!OVT_InfluenceRules.TownQualifiesForMomentum(OCCUPIER_FACTION, PLAYER_FACTION))
		{
			SetResultFailure("A town held by faction %1 did not qualify as a momentum target for player faction %2; momentum would never be applied to anything", OCCUPIER_FACTION.ToString(), PLAYER_FACTION.ToString());
			return true;
		}

		// A third faction is not the player either, so it qualifies. This is what keeps the rule
		// "not the player's" rather than "the occupier's".
		if (!OVT_InfluenceRules.TownQualifiesForMomentum(0, PLAYER_FACTION))
		{
			SetResultFailure("A town held by a third faction did not qualify as a momentum target; the rule is 'not the player's', not 'the occupier's'");
			return true;
		}

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Case 10 - the momentum reach is 2000 m.
//!
//! It moved out of a protected const on the momentum modifier class into the shared rule set, and a
//! value that has moved is a value that can be mistyped in the move. It is also the one range in the
//! feature that is NOT a difficulty setting - the two proximity ranges are configured per difficulty
//! and are read from config at the call site, so there is nothing to pin about them here, whereas
//! this one is a constant with exactly one home and this is the assertion that guards it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Influence_MomentumRangeConstant : SCR_AutotestCaseBase
{
	//! The reach the campaign has always applied, in metres.
	protected static const float EXPECTED_RANGE = 2000;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		if (!OVT_TEST_LogicFixture.FloatEquals(OVT_InfluenceRules.MOMENTUM_RANGE, EXPECTED_RANGE))
		{
			SetResultFailure("The momentum reach is %1 m, expected %2 m; it moved out of a per-class constant and every town pair within the difference would gain or lose a support modifier", OVT_InfluenceRules.MOMENTUM_RANGE.ToString(), EXPECTED_RANGE.ToString());
			return true;
		}

		SetResultSuccess();
		return true;
	}
}
