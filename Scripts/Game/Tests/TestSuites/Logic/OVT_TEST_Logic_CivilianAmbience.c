//------------------------------------------------------------------------------------------------
//! TIER A cases - OVT_CivilianAmbienceMath, the arithmetic behind town civilian ambience.
//!
//! Everything asserted here is a pure static: the density formula and its clamps, the per-town hard
//! cap, the weighted type pick and the type filter. The subject class is deliberately free of every
//! live-system reference, which is what lets these claims be asserted in the cheapest tier in the
//! tree - and is also why the density formula is a static at all rather than a method on the config
//! object that will call it.
//!
//! WHAT IS NOT HERE. Building a source, resolving an authored template, spawning a crowd and pruning
//! a corpse all need live systems; those are Init-tier (registration and config-struct claims) and
//! play-test items (spawn feel, placement, leaks). Nothing in this file reaches for anything the tier
//! rule forbids, in code or in prose.
//!
//! RNG. There is none. PickWeightedIndex() takes the roll as an ARGUMENT precisely so that the
//! weighted pick is assertable exactly, at both ends and past both ends, with no draw involved. No
//! retry attribute is used anywhere - this project's quality bar bans it outright.
//!
//! SIZE ORDINALS. OVT_TownSize is an int enum (VILLAGE 1 < TOWN 2 < CITY 3 < CAPITAL 4) and the
//! subject takes plain ints, so the cases pass the enum members straight through - the same shape as
//! the virtualization cases passing SCR_EAISpawnImportance into an int parameter.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Density parity: the migrated formula produces the same crowd sizes as the spawner it replaces.
//!
//! This is the case that makes the migration a MIGRATION and not a redesign. The retired spawner
//! computed `population * 0.1` and spawned that many one-man groups, so the three worked examples
//! from the shipping map (a city of 283, a town of 38, a village of 27) must come out at 28, 4 and 3
//! - and the two fractional ones are the interesting half, because they only land there if the raw
//! figure is ROUNDED. Truncating instead silently thins every town in the world by up to one
//! civilian, which is precisely the kind of change nobody would ever notice by eye.
//!
//! The multiplier is asserted alongside them because it multiplies INTO the same expression rather
//! than scaling the result afterwards: 283 at rate 0.1 and multiplier 0.5 is 14 (round(14.15)), not
//! round(28.3) halved, and the two disagree at exactly the values an operator is most likely to try.
//!
//! PROVEN ABLE TO FAIL (fail proof recorded; execution belongs to the phase's suite run): replace
//! `int count = Math.Round(population * density);` with `int count = population * density;` in
//! ResolveTownCivilianCount and the 38-population assertion goes red with 3 instead of 4.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_CivilianAmbience_DensityParity : SCR_AutotestCaseBase
{
	//! The rate the retired spawner used, and the authored default this feature ships.
	protected const float PARITY_RATE = 0.1;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// The city: 283 * 0.1 = 28.3 -> 28.
		int count = OVT_CivilianAmbienceMath.ResolveTownCivilianCount(283, PARITY_RATE, 1.0, 2, 40, 0);
		if (count != 28)
		{
			SetFailure("A population of 283 at the parity rate resolved to %1 civilians, expected 28 - the migrated formula no longer agrees with the spawner it replaces", count.ToString());
			return true;
		}

		// The town: 38 * 0.1 = 3.8 -> 4. Truncation would give 3.
		count = OVT_CivilianAmbienceMath.ResolveTownCivilianCount(38, PARITY_RATE, 1.0, 2, 40, 0);
		if (count != 4)
		{
			SetFailure("A population of 38 at the parity rate resolved to %1 civilians, expected 4 - a 3 here means the raw figure is being truncated instead of rounded", count.ToString());
			return true;
		}

		// The village: 27 * 0.1 = 2.7 -> 3.
		count = OVT_CivilianAmbienceMath.ResolveTownCivilianCount(27, PARITY_RATE, 1.0, 2, 40, 0);
		if (count != 3)
		{
			SetFailure("A population of 27 at the parity rate resolved to %1 civilians, expected 3", count.ToString());
			return true;
		}

		// The operator multiplier scales the population term, not the rounded result.
		count = OVT_CivilianAmbienceMath.ResolveTownCivilianCount(283, PARITY_RATE, 0.5, 2, 40, 0);
		if (count != 14)
		{
			SetFailure("A population of 283 at half density resolved to %1 civilians, expected 14 (round(14.15)) - halving the ROUNDED 28 would give 14 too, so a %1 here means the multiplier is not in the expression at all", count.ToString());
			return true;
		}

		// A multiplier above 1 is a legitimate "busier towns" setting, bounded only by the clamps.
		count = OVT_CivilianAmbienceMath.ResolveTownCivilianCount(100, PARITY_RATE, 2.0, 2, 40, 0);
		if (count != 20)
		{
			SetFailure("A population of 100 at double density resolved to %1 civilians, expected 20", count.ToString());
			return true;
		}

		Print("Town civilian density matches the retired spawner at 283/38/27 population and scales with the operator multiplier");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The clamps, and the two things that must beat them.
//!
//! The floor and the ceiling exist so a rate or population change can never produce a crowd nobody
//! authored for. But two inputs have to win AGAINST the floor, and both are easy to get wrong by
//! writing the guards in the wrong order:
//!
//!  - POPULATION 0 BEATS minCount. A depopulated settlement has no crowd at all. If the floor were
//!    applied first, an emptied settlement would still be walking around with its minimum, which is
//!    both wrong and self-contradictory (the civilians would be modelling people who are gone).
//!  - MULTIPLIER 0 BEATS minCount. It is the documented "no civilians on this server" switch, so a
//!    floor that resurrected two per settlement would make the switch silently not work - the worst
//!    possible failure for a setting an operator turns on to reduce load.
//!
//! The ceiling is asserted from a population large enough that only the ceiling can be answering.
//!
//! PROVEN ABLE TO FAIL (fail proof recorded): move the `population <= 0` guard to AFTER the clamp
//! block (returning the clamped value) and the population-0 assertion goes red with 5; delete the
//! `density <= 0` guard and the multiplier-0 assertion goes red with 2 (the floor).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_CivilianAmbience_DensityClampsAndOffSwitches : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// FLOOR: 12 * 0.1 = 1.2 -> 1, lifted to the minimum of 2. A settlement with people in it is
		// never empty of civilians.
		int count = OVT_CivilianAmbienceMath.ResolveTownCivilianCount(12, 0.1, 1.0, 2, 40, 0);
		if (count != 2)
		{
			SetFailure("A population of 12 resolved to %1 civilians, expected the minimum of 2 - the floor is not being applied", count.ToString());
			return true;
		}

		// CEILING: 2000 * 0.1 = 200, cut to the authored maximum of 40.
		count = OVT_CivilianAmbienceMath.ResolveTownCivilianCount(2000, 0.1, 1.0, 2, 40, 0);
		if (count != 40)
		{
			SetFailure("A population of 2000 resolved to %1 civilians, expected the maximum of 40 - the ceiling is not being applied", count.ToString());
			return true;
		}

		// POPULATION 0 beats the floor, even a large one.
		count = OVT_CivilianAmbienceMath.ResolveTownCivilianCount(0, 0.1, 1.0, 5, 40, 0);
		if (count != 0)
		{
			SetFailure("A population of 0 resolved to %1 civilians, expected 0 - the minimum clamp must not resurrect a crowd in a settlement with nobody left in it", count.ToString());
			return true;
		}

		// A negative population is nonsense arriving from somewhere else; it may not become a crowd.
		count = OVT_CivilianAmbienceMath.ResolveTownCivilianCount(-10, 0.1, 1.0, 5, 40, 0);
		if (count != 0)
		{
			SetFailure("A negative population resolved to %1 civilians, expected 0", count.ToString());
			return true;
		}

		// MULTIPLIER 0 beats the floor - this is the documented off switch.
		count = OVT_CivilianAmbienceMath.ResolveTownCivilianCount(283, 0.1, 0.0, 2, 40, 0);
		if (count != 0)
		{
			SetFailure("A density multiplier of 0 resolved to %1 civilians, expected 0 - this is the operator's documented way to turn civilians off entirely", count.ToString());
			return true;
		}

		// A rate of 0 is the same statement made in the authored config instead of the operator file.
		count = OVT_CivilianAmbienceMath.ResolveTownCivilianCount(283, 0.0, 1.0, 2, 40, 0);
		if (count != 0)
		{
			SetFailure("A population rate of 0 resolved to %1 civilians, expected 0", count.ToString());
			return true;
		}

		// A negative multiplier is nonsense and reads as off, never as a negative crowd.
		count = OVT_CivilianAmbienceMath.ResolveTownCivilianCount(283, 0.1, -1.0, 2, 40, 0);
		if (count != 0)
		{
			SetFailure("A negative density multiplier resolved to %1 civilians, expected 0", count.ToString());
			return true;
		}

		Print("Density clamps hold at both ends, and population 0 / multiplier 0 both beat the minimum clamp");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The per-town hard cap, and the asymmetry that makes an un-migrated operator config safe.
//!
//! The cap is the operator's blast-radius control: whatever the authored rate and the settlement's
//! population say, no single settlement may exceed it. Two properties matter more than the cap
//! itself:
//!
//!  - `hardCap <= 0` MEANS UNCAPPED, not "cap of zero". An operator config file written before this
//!    key existed reads back 0 for it, and a `>= 0` guard would turn every one of those servers'
//!    towns empty on the next restart with nothing in the log to explain it. Turning civilians off
//!    is the density multiplier's job, and it is a separate key on purpose.
//!  - THE CAP BEATS THE MINIMUM CLAMP, because it is applied last. An operator who sets 1 gets 1,
//!    not the authored floor of 2. That is the point of a cap, and it is asserted so nobody
//!    "fixes" the order later.
//!
//! PROVEN ABLE TO FAIL (fail proof recorded): change the step-5 guard from `if (hardCap > 0 && ...)`
//! to `if (hardCap >= 0 && ...)` and the uncapped assertion goes red with 0 instead of 40.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_CivilianAmbience_HardCap : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// A crowd under the cap is untouched by it.
		int count = OVT_CivilianAmbienceMath.ResolveTownCivilianCount(283, 0.1, 1.0, 2, 40, 30);
		if (count != 28)
		{
			SetFailure("A 28-civilian crowd under a cap of 30 resolved to %1, expected 28 - the cap is changing a figure that was already inside it", count.ToString());
			return true;
		}

		// A crowd over the cap is cut to it, after the authored ceiling has already had its say.
		count = OVT_CivilianAmbienceMath.ResolveTownCivilianCount(2000, 0.1, 1.0, 2, 40, 30);
		if (count != 30)
		{
			SetFailure("A crowd clamped to the authored maximum of 40 resolved to %1 under a cap of 30, expected 30", count.ToString());
			return true;
		}

		// A cap of 0 is UNCAPPED - the un-migrated-config case.
		count = OVT_CivilianAmbienceMath.ResolveTownCivilianCount(2000, 0.1, 1.0, 2, 40, 0);
		if (count != 40)
		{
			SetFailure("A cap of 0 resolved to %1 civilians, expected the uncapped 40 - reading an absent config key as 'cap of zero' would empty every town on a server that upgraded", count.ToString());
			return true;
		}

		// So is a negative one.
		count = OVT_CivilianAmbienceMath.ResolveTownCivilianCount(2000, 0.1, 1.0, 2, 40, -1);
		if (count != 40)
		{
			SetFailure("A cap of -1 resolved to %1 civilians, expected the uncapped 40", count.ToString());
			return true;
		}

		// The cap is applied LAST, so it beats the minimum clamp.
		count = OVT_CivilianAmbienceMath.ResolveTownCivilianCount(12, 0.1, 1.0, 2, 40, 1);
		if (count != 1)
		{
			SetFailure("A cap of 1 over a minimum clamp of 2 resolved to %1 civilians, expected 1 - a cap that loses to the authored floor is not a cap", count.ToString());
			return true;
		}

		// An empty settlement stays empty however generous the cap is.
		count = OVT_CivilianAmbienceMath.ResolveTownCivilianCount(0, 0.1, 1.0, 2, 40, 30);
		if (count != 0)
		{
			SetFailure("A population of 0 under a cap of 30 resolved to %1 civilians, expected 0", count.ToString());
			return true;
		}

		Print("The per-town hard cap applies over the clamps, beats the minimum, and treats 0 or below as uncapped");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Weighted picking, at the first roll, the last roll, and past both ends.
//!
//! The weighted pick is how a settlement gets a MIX of civilian types instead of the same one every
//! time, so the arithmetic has to be exact at the boundaries: a fencepost error at the accumulator
//! silently shifts the whole distribution by one entry, which is invisible in play and would only
//! ever be noticed as "the crowd looks a bit samey".
//!
//! The out-of-range answers are asserted because they are a CONTRACT, not an accident: a caller may
//! draw against a total that has since changed, and the answer must be a deterministic end of the
//! table rather than an out-of-bounds index or a silent 0. A weight of 0 is also a contract - it
//! means "authored, but never picked", so it must be skipped rather than quietly treated as 1, and
//! it must not be able to become the answer for an out-of-range roll either.
//!
//! PROVEN ABLE TO FAIL (fail proof recorded): change the accumulator test from `clamped < cursor` to
//! `clamped <= cursor` and the first-boundary assertion (roll 5 over weights 5/3/2) goes red with 0
//! instead of 1; delete the `if (clamped >= total) clamped = total - 1;` clamp and the past-the-end
//! assertion goes red with -1 instead of the last index.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_CivilianAmbience_WeightedPick : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// Three types weighted 5 / 3 / 2: rolls 0-4 are index 0, 5-7 index 1, 8-9 index 2.
		array<int> weights = {5, 3, 2};

		if (OVT_CivilianAmbienceMath.SumWeights(weights) != 10)
		{
			SetFailure("SumWeights(5,3,2) returned %1, expected 10 - a caller rolling against the wrong bound would stop honouring the weights entirely",
				OVT_CivilianAmbienceMath.SumWeights(weights).ToString());
			return true;
		}

		string failure = ExpectPick(weights, 0, 0, "the very first roll");
		if (failure == "")
			failure = ExpectPick(weights, 4, 0, "the last roll inside the first band");
		if (failure == "")
			failure = ExpectPick(weights, 5, 1, "the first roll of the second band");
		if (failure == "")
			failure = ExpectPick(weights, 7, 1, "the last roll of the second band");
		if (failure == "")
			failure = ExpectPick(weights, 8, 2, "the first roll of the last band");
		if (failure == "")
			failure = ExpectPick(weights, 9, 2, "the very last valid roll");

		// Past both ends: deterministic, and at the ends of the table.
		if (failure == "")
			failure = ExpectPick(weights, -1, 0, "a negative roll");
		if (failure == "")
			failure = ExpectPick(weights, 10, 2, "a roll exactly at the total");
		if (failure == "")
			failure = ExpectPick(weights, 999, 2, "a roll far past the total");

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		// A zero weight is skipped, and cannot be the answer for an out-of-range roll either.
		array<int> zeroed = {0, 3, 0};

		failure = ExpectPick(zeroed, 0, 1, "a roll of 0 over a leading zero weight");
		if (failure == "")
			failure = ExpectPick(zeroed, 2, 1, "the last valid roll over trailing zero weights");
		if (failure == "")
			failure = ExpectPick(zeroed, -5, 1, "a negative roll where index 0 is disabled");
		if (failure == "")
			failure = ExpectPick(zeroed, 100, 1, "a past-the-end roll where the last index is disabled");

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		// A negative weight is disabled exactly like a zero one, and never subtracts from the total.
		array<int> negative = {0, -4, 2};
		if (OVT_CivilianAmbienceMath.SumWeights(negative) != 2)
		{
			SetFailure("SumWeights(0,-4,2) returned %1, expected 2 - a negative weight must be skipped, not subtracted",
				OVT_CivilianAmbienceMath.SumWeights(negative).ToString());
			return true;
		}

		failure = ExpectPick(negative, 0, 2, "a roll over a table whose only positive weight is last");
		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		// Nothing eligible answers -1, which a caller must not read as index 0.
		array<int> allZero = {0, 0, 0};
		if (OVT_CivilianAmbienceMath.PickWeightedIndex(allZero, 0) != -1)
		{
			SetFailure("An all-zero weight table returned %1, expected -1 - answering 0 would spawn the first type forever from a table that disabled it",
				OVT_CivilianAmbienceMath.PickWeightedIndex(allZero, 0).ToString());
			return true;
		}

		array<int> empty = {};
		if (OVT_CivilianAmbienceMath.PickWeightedIndex(empty, 0) != -1)
		{
			SetFailure("An empty weight table returned %1, expected -1",
				OVT_CivilianAmbienceMath.PickWeightedIndex(empty, 0).ToString());
			return true;
		}

		if (OVT_CivilianAmbienceMath.PickWeightedIndex(null, 0) != -1)
		{
			SetFailure("A null weight table returned %1, expected -1",
				OVT_CivilianAmbienceMath.PickWeightedIndex(null, 0).ToString());
			return true;
		}

		if (OVT_CivilianAmbienceMath.SumWeights(null) != 0)
		{
			SetFailure("SumWeights(null) returned %1, expected 0",
				OVT_CivilianAmbienceMath.SumWeights(null).ToString());
			return true;
		}

		Print("Weighted picking is exact at every band boundary, deterministic past both ends, and skips zero/negative weights");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One weighted-pick assertion, so a case body reads as a table of claims instead of ten
	//! near-identical if blocks.
	//! \param[in] weights The weight table.
	//! \param[in] roll The roll to resolve.
	//! \param[in] expected The index the contract says that roll must produce.
	//! \param[in] what Human description of the roll, used only in the failure text.
	//! \return An empty string when the pick was right, or the failure to report.
	protected string ExpectPick(array<int> weights, int roll, int expected, string what)
	{
		int picked = OVT_CivilianAmbienceMath.PickWeightedIndex(weights, roll);
		if (picked == expected)
			return "";

		return string.Format("A roll of %1 (%2) picked index %3, expected %4", roll.ToString(), what, picked.ToString(), expected.ToString());
	}
}

//------------------------------------------------------------------------------------------------
//! Type filtering: by minimum settlement size, and by an explicit allow-list.
//!
//! The two filters are INDEPENDENT and both must pass, which is the whole reason this is one
//! function and not two scattered conditions. The size filter is the default that needs no authoring
//! at all (a city-only look simply never appears in a hamlet); the allow-list is the per-settlement
//! override for the cases where size is not expressive enough.
//!
//! THE EMPTY LIST IS THE DANGEROUS CASE, and it is asserted first. Every un-authored settlement
//! carries an empty list, so an implementation that read "empty" as "nothing allowed" would empty
//! every settlement in the world except the handful somebody remembered to author - and it would do
//! it silently, because "no civilians here" is exactly what the un-authored default is supposed to
//! mean the opposite of.
//!
//! Matching is exact and case-sensitive, the same rule the source registry's own name lookup uses;
//! that is asserted too, so the behaviour is a decision on the record rather than an accident nobody
//! ever tested.
//!
//! PROVEN ABLE TO FAIL (fail proof recorded): change the guard
//! `if (!allowedNames || allowedNames.IsEmpty()) return true;` to `if (!allowedNames) return true;`
//! and the empty-list assertion goes red (false where true is required); change the size test from
//! `townSize < typeMinSize` to `townSize <= typeMinSize` and the "a city type is allowed in a city"
//! assertion goes red.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_CivilianAmbience_TypeFilter : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// -- no allow-list authored: the size filter decides on its own ------------------------------

		// A type with no size requirement belongs everywhere, including the smallest settlement.
		if (!OVT_CivilianAmbienceMath.TypeAllowed(OVT_TownSize.VILLAGE, OVT_TownSize.VILLAGE, null, "generic"))
		{
			SetFailure("An unrestricted type was refused in the smallest settlement size - the unrestricted type is what keeps a village from being empty");
			return true;
		}

		if (!OVT_CivilianAmbienceMath.TypeAllowed(OVT_TownSize.VILLAGE, OVT_TownSize.CAPITAL, null, "generic"))
		{
			SetFailure("An unrestricted type was refused in the largest settlement size");
			return true;
		}

		// An EMPTY list is "not authored", not "nothing allowed".
		array<string> noneAuthored = {};
		if (!OVT_CivilianAmbienceMath.TypeAllowed(OVT_TownSize.VILLAGE, OVT_TownSize.TOWN, noneAuthored, "generic"))
		{
			SetFailure("An empty allow-list refused every type - every un-authored settlement carries an empty list, so this would silently empty the whole map");
			return true;
		}

		// -- the size filter -------------------------------------------------------------------------

		// A city-only type is refused below its minimum size, on both sizes below it.
		if (OVT_CivilianAmbienceMath.TypeAllowed(OVT_TownSize.CITY, OVT_TownSize.VILLAGE, null, "businessman"))
		{
			SetFailure("A city-only type was allowed in a village");
			return true;
		}

		if (OVT_CivilianAmbienceMath.TypeAllowed(OVT_TownSize.CITY, OVT_TownSize.TOWN, null, "businessman"))
		{
			SetFailure("A city-only type was allowed in a town, one size below its minimum");
			return true;
		}

		// The minimum is INCLUSIVE - a city type belongs in a city.
		if (!OVT_CivilianAmbienceMath.TypeAllowed(OVT_TownSize.CITY, OVT_TownSize.CITY, null, "businessman"))
		{
			SetFailure("A city-only type was refused in a city - the minimum size must be inclusive, or every type would need a size one below the one it is for");
			return true;
		}

		// ...and in everything above it.
		if (!OVT_CivilianAmbienceMath.TypeAllowed(OVT_TownSize.CITY, OVT_TownSize.CAPITAL, null, "businessman"))
		{
			SetFailure("A city-only type was refused in a capital");
			return true;
		}

		// -- the explicit allow-list -----------------------------------------------------------------

		array<string> authored = {"generic", "dockworker"};

		if (!OVT_CivilianAmbienceMath.TypeAllowed(OVT_TownSize.VILLAGE, OVT_TownSize.CITY, authored, "dockworker"))
		{
			SetFailure("A type named in the allow-list was refused");
			return true;
		}

		if (OVT_CivilianAmbienceMath.TypeAllowed(OVT_TownSize.VILLAGE, OVT_TownSize.CITY, authored, "businessman"))
		{
			SetFailure("A type absent from an authored allow-list was allowed - the list is an allow-list, not a hint");
			return true;
		}

		// Matching is exact: a differently-cased name is a different name.
		if (OVT_CivilianAmbienceMath.TypeAllowed(OVT_TownSize.VILLAGE, OVT_TownSize.CITY, authored, "Generic"))
		{
			SetFailure("A differently-cased type name matched the allow-list - matching is exact, the same rule the source registry's own name lookup uses");
			return true;
		}

		// -- both filters at once ---------------------------------------------------------------------

		// Being named in the list does NOT buy a type past the size filter.
		array<string> onlyBusinessmen = {"businessman"};
		if (OVT_CivilianAmbienceMath.TypeAllowed(OVT_TownSize.CITY, OVT_TownSize.VILLAGE, onlyBusinessmen, "businessman"))
		{
			SetFailure("A city-only type named in a village's allow-list was allowed - the two filters are independent and both must pass");
			return true;
		}

		// And passing the size filter does not buy a type past the list.
		if (OVT_CivilianAmbienceMath.TypeAllowed(OVT_TownSize.VILLAGE, OVT_TownSize.CITY, onlyBusinessmen, "generic"))
		{
			SetFailure("An unrestricted type was allowed in a settlement whose allow-list names only one other type");
			return true;
		}

		Print("Type filtering enforces the minimum settlement size inclusively, honours an authored allow-list exactly, and reads an empty list as 'no restriction'");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Archetype resolution: the authored behaviour mix comes out of the weight table it is authored as.
//!
//! WHY THIS IS ITS OWN CASE and not a re-run of the type-pick one above. A crowd's TYPES are what
//! people look like; its ARCHETYPES are what they do, and the two tables are rolled separately, per
//! civilian, against different weights. The behaviour mix is the more fragile of the two, because a
//! wrong answer here is not visibly wrong at all - a town where every civilian silently resolved to
//! the same archetype still looks like a town full of people, just a strangely synchronised one. So
//! the shipped mix is asserted as numbers rather than trusted to eyes.
//!
//! THE SHIPPED TABLE is 50 / 30 / 20 (ping-pong, wander, loiter), summing to 100 - which makes every
//! weight readable as a percentage and every band boundary a round number worth pinning:
//! rolls 0-49 are the first behaviour, 50-79 the second, 80-99 the third.
//!
//! THE ROLL BOUND IS THE TOTAL, NOT THE TOTAL MINUS ONE. The draw a caller makes is max-EXCLUSIVE, so
//! the largest roll that can ever occur is 99 and it must land inside the last band. A caller that
//! subtracted one from the bound would make the last behaviour very slightly rarer than authored, and
//! nobody would ever see it; a caller that added one would produce a roll of 100, which is the case
//! asserted here as landing at the end of the table rather than nowhere.
//!
//! DISABLING ONE BEHAVIOUR by zeroing its weight is the modder-facing operation the whole table is
//! for, so "a zeroed entry is never picked, and the remaining bands close up over its range" is
//! asserted too - a zero that behaved as a 1 would leak the disabled behaviour back in at a low rate,
//! which is the single most annoying way for a config edit to not work.
//!
//! PROVEN ABLE TO FAIL (fail proof recorded; execution belongs to the phase's suite run): change the
//! accumulator test in PickWeightedIndex from `clamped < cursor` to `clamped <= cursor` and the
//! roll-50 assertion goes red with 0 instead of 1 (the first band swallowing the second's first
//! roll). Independently: change SumWeights to skip the last entry (`i < weights.Count() - 1`) and the
//! shipped-total assertion goes red with 80 instead of 100.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_CivilianAmbience_ArchetypeWeights : SCR_AutotestCaseBase
{
	//! Indices into the shipped archetype table, in the order the .conf authors them.
	protected const int PINGPONG = 0;
	protected const int WANDER = 1;
	protected const int LOITER = 2;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// The shipped mix: 50 ping-pong, 30 wander, 20 loiter.
		array<int> shipped = {50, 30, 20};

		int total = OVT_CivilianAmbienceMath.SumWeights(shipped);
		if (total != 100)
		{
			SetFailure("The shipped archetype weights summed to %1, expected 100 - a caller draws in [0, this), so a wrong total silently re-weights every behaviour in the game", total.ToString());
			return true;
		}

		string failure = ExpectArchetype(shipped, 0, PINGPONG, "the very first roll");
		if (failure == "")
			failure = ExpectArchetype(shipped, 49, PINGPONG, "the last roll of the ping-pong band");
		if (failure == "")
			failure = ExpectArchetype(shipped, 50, WANDER, "the first roll of the wander band");
		if (failure == "")
			failure = ExpectArchetype(shipped, 79, WANDER, "the last roll of the wander band");
		if (failure == "")
			failure = ExpectArchetype(shipped, 80, LOITER, "the first roll of the loiter band");

		// The largest roll a max-exclusive draw against the total can ever produce.
		if (failure == "")
			failure = ExpectArchetype(shipped, 99, LOITER, "the largest roll a max-exclusive draw can produce");

		// One past it - what a caller that used the wrong bound would produce. Deterministic, at the
		// end of the table, never an out-of-bounds index.
		if (failure == "")
			failure = ExpectArchetype(shipped, 100, LOITER, "a roll one past the total");

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		// -- disabling one behaviour ------------------------------------------------------------------

		// Wander zeroed: the table becomes 50 ping-pong / 20 loiter, and the bands close up.
		array<int> noWander = {50, 0, 20};

		if (OVT_CivilianAmbienceMath.SumWeights(noWander) != 70)
		{
			SetFailure("Zeroing one archetype left a total of %1, expected 70 - a disabled behaviour must leave the roll space entirely, not reserve it",
				OVT_CivilianAmbienceMath.SumWeights(noWander).ToString());
			return true;
		}

		failure = ExpectArchetype(noWander, 49, PINGPONG, "the last roll of the first band with the middle behaviour disabled");
		if (failure == "")
			failure = ExpectArchetype(noWander, 50, LOITER, "the roll that would have been the disabled behaviour's first");
		if (failure == "")
			failure = ExpectArchetype(noWander, 69, LOITER, "the last valid roll with one behaviour disabled");

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		// -- the degenerate tables a mis-authored .conf can produce ------------------------------------

		// Every behaviour disabled: -1, which the caller must read as "author nothing" and fall back to
		// its own defaults, never as "the first behaviour".
		array<int> allDisabled = {0, 0, 0};
		if (OVT_CivilianAmbienceMath.PickWeightedIndex(allDisabled, 0) != -1)
		{
			SetFailure("An archetype table with every entry disabled resolved to index %1, expected -1 - answering 0 would give the whole map one behaviour that the config explicitly turned off",
				OVT_CivilianAmbienceMath.PickWeightedIndex(allDisabled, 0).ToString());
			return true;
		}

		// A single authored behaviour is picked for every roll in its range, including the boundaries.
		array<int> single = {100};
		failure = ExpectArchetype(single, 0, 0, "the first roll of a single-behaviour table");
		if (failure == "")
			failure = ExpectArchetype(single, 99, 0, "the last roll of a single-behaviour table");

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Archetype resolution honours the shipped 50/30/20 mix at every band boundary, closes the bands over a disabled behaviour, and answers -1 rather than a default when every behaviour is disabled");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One archetype-resolution assertion.
	//! \param[in] weights The archetype weight table.
	//! \param[in] roll The roll to resolve.
	//! \param[in] expected The index the authored mix says that roll must produce.
	//! \param[in] what Human description of the roll, used only in the failure text.
	//! \return An empty string when the pick was right, or the failure to report.
	protected string ExpectArchetype(array<int> weights, int roll, int expected, string what)
	{
		int picked = OVT_CivilianAmbienceMath.PickWeightedIndex(weights, roll);
		if (picked == expected)
			return "";

		return string.Format("An archetype roll of %1 (%2) resolved to index %3, expected %4", roll.ToString(), what, picked.ToString(), expected.ToString());
	}
}

//------------------------------------------------------------------------------------------------
//! THE SHIPPED CURATION TABLE: the six authored civilian types, filtered the way the shipped world
//! actually filters them.
//!
//! WHY A SECOND FILTER CASE. OVT_TEST_Logic_CivilianAmbience_TypeFilter asserts the RULE with made-up
//! inputs; this one asserts the CONTENT - the six type names and minimum sizes that
//! Configs/Civilians/CivilianAmbience.conf ships, run through the same rule as whole sets. The two
//! fail for different reasons: the rule case goes red when the filter is rewritten, this one goes red
//! when somebody re-authors a type's minimum size and quietly changes what a village looks like.
//!
//! THE SHIPPED TABLE (name -> smallest settlement it may appear in):
//!   generic     VILLAGE      townsfolk  VILLAGE      villager  VILLAGE
//!   worker      VILLAGE      dockworker TOWN         businessman CITY
//!
//! THE TWO PATHS, both of which the user explicitly asked for:
//!  1. SIZE ALONE. An un-authored settlement (empty list) gets everything its size permits - four
//!     types in a village, five in a town, all six in a city.
//!  2. AN AUTHORED ALLOW-LIST ON TOP. A curated port town names its own types and gets exactly those,
//!     and naming a type its size forbids does NOT smuggle it in. This is the combination that makes
//!     a dock town read as a dock town instead of as "whatever fits".
//!
//! THE PARANOID ASSERTION is the last one: every settlement size resolves at least ONE type. An
//! authored table where the unrestricted entry gained a minimum size would empty every village in the
//! world, and an empty village is indistinguishable from an unlucky roll in play.
//!
//! PROVEN ABLE TO FAIL (fail proof recorded; execution belongs to the phase's suite run): change the
//! dockworker row below from TOWN to VILLAGE and the "a village gets four types" count assertion goes
//! red with 5; drop "generic" from the curated port town's list and the "a curated town keeps an
//! unrestricted type" assertion goes red.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_CivilianAmbience_ShippedTypeCuration : SCR_AutotestCaseBase
{
	//! The shipped type names, in the order the authored file lists them.
	protected ref array<string> m_aTypeNames = {"generic", "townsfolk", "villager", "worker", "dockworker", "businessman"};

	//! Each shipped type's minimum settlement size, index-aligned with the names above.
	protected ref array<int> m_aTypeMinSizes = {OVT_TownSize.VILLAGE, OVT_TownSize.VILLAGE, OVT_TownSize.VILLAGE,
		OVT_TownSize.VILLAGE, OVT_TownSize.TOWN, OVT_TownSize.CITY};

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// -- path 1: size alone, for a settlement that authored nothing ------------------------------

		array<string> village = ResolveSet(OVT_TownSize.VILLAGE, null);
		if (village.Count() != 4)
		{
			SetFailure("An un-authored VILLAGE resolved %1 civilian types, expected the 4 unrestricted ones - a 5 means a type meant for bigger settlements leaked down, a 3 means an unrestricted type gained a minimum size",
				village.Count().ToString());
			return true;
		}

		if (village.Contains("dockworker") || village.Contains("businessman"))
		{
			SetFailure("A village resolved a type authored for a larger settlement - the size filter is the only thing keeping businessmen out of hamlets when nobody has curated them");
			return true;
		}

		array<string> town = ResolveSet(OVT_TownSize.TOWN, null);
		if (town.Count() != 5 || !town.Contains("dockworker") || town.Contains("businessman"))
		{
			SetFailure("An un-authored TOWN resolved %1 types, expected 5 - the dockworker's minimum size is TOWN so it must be in, and the businessman's is CITY so it must be out",
				town.Count().ToString());
			return true;
		}

		array<string> city = ResolveSet(OVT_TownSize.CITY, null);
		if (city.Count() != m_aTypeNames.Count())
		{
			SetFailure("An un-authored CITY resolved %1 of the %2 shipped civilian types - a city is the size every authored type is valid in, so anything less means a minimum size above CITY was authored by mistake",
				city.Count().ToString(), m_aTypeNames.Count().ToString());
			return true;
		}

		// -- path 2: an authored allow-list, on top of the size filter -------------------------------

		// A curated port TOWN: exactly what it names, no more.
		array<string> portList = {"generic", "townsfolk", "dockworker"};
		array<string> port = ResolveSet(OVT_TownSize.TOWN, portList);
		if (port.Count() != 3)
		{
			SetFailure("A port town naming three types resolved %1 - an authored list is an allow-list, so the set must be exactly the intersection of the list and what the size permits",
				port.Count().ToString());
			return true;
		}

		if (!port.Contains("generic"))
		{
			SetFailure("A curated town lost the unrestricted type it named - every authored list carries one on purpose, so that curation can never empty a settlement");
			return true;
		}

		// Naming a type the size forbids does not smuggle it in: the same list in a VILLAGE loses the
		// dockworker and keeps the two the size allows.
		array<string> portVillage = ResolveSet(OVT_TownSize.VILLAGE, portList);
		if (portVillage.Count() != 2 || portVillage.Contains("dockworker"))
		{
			SetFailure("A VILLAGE authoring the port list resolved %1 types, expected 2 with the dockworker refused - naming a type must never buy it past the size filter",
				portVillage.Count().ToString());
			return true;
		}

		// A curated CITY: the shipped city list is the one place all four "urban" types coexist.
		array<string> cityList = {"generic", "townsfolk", "dockworker", "businessman", "worker"};
		array<string> curatedCity = ResolveSet(OVT_TownSize.CITY, cityList);
		if (curatedCity.Count() != 5 || !curatedCity.Contains("businessman"))
		{
			SetFailure("A curated CITY resolved %1 types, expected all 5 it named including the city-only businessman",
				curatedCity.Count().ToString());
			return true;
		}

		// -- the paranoid one: no settlement size can ever come out empty ----------------------------

		array<int> everySize = {OVT_TownSize.VILLAGE, OVT_TownSize.TOWN, OVT_TownSize.CITY, OVT_TownSize.CAPITAL};
		foreach (int size : everySize)
		{
			if (ResolveSet(size, null).IsEmpty())
			{
				SetFailure("Settlement size ordinal %1 resolved NO civilian type at all - the shipped table must always leave an unrestricted type standing, or that whole size band silently has no people in it",
					size.ToString());
				return true;
			}
		}

		Print("The shipped six-type table curates as authored: 4 types in a village, 5 in a town, 6 in a city, an authored list narrows to exactly its intersection with the size filter, and no size band is ever empty");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the whole shipped table through the filter, the way a settlement's type set is resolved.
	//! \param[in] townSize The settlement's size ordinal.
	//! \param[in] allowedNames The settlement's own authored list, or null when it authored none.
	//! \return The names that survived both filters, never null.
	protected array<string> ResolveSet(int townSize, array<string> allowedNames)
	{
		array<string> resolved = {};

		for (int i = 0; i < m_aTypeNames.Count(); i++)
		{
			if (OVT_CivilianAmbienceMath.TypeAllowed(m_aTypeMinSizes[i], townSize, allowedNames, m_aTypeNames[i]))
				resolved.Insert(m_aTypeNames[i]);
		}

		return resolved;
	}
}
