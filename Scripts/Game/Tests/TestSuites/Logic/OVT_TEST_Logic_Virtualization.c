//------------------------------------------------------------------------------------------------
//! TIER A cases - OVT_VirtualizationMath, the world-free arithmetic behind AI-group virtualization.
//!
//! Everything asserted here is a pure static: distance/importance resolution, waypoint-plan
//! integrity, the per-slot survivor mask, the refill selector and the ambient round-robin. The
//! subject class is deliberately free of every world, entity and registry reference, which is what
//! lets these claims be asserted in the cheapest tier in the tree.
//!
//! WHAT IS NOT HERE. Registration itself, record bookkeeping and the group-entity wiring all need
//! live systems; those are asserted in the Init tier (registration refusals) and, from Phase 5, in
//! the persistence round trip. Nothing in this file reaches for anything the tier rule forbids.
//!
//! RNG. RollCountSafe() is the one RNG-touching static. Its DETERMINISTIC branches (min == max,
//! inverted pair) are asserted exactly; the random branch is asserted only for containment in
//! [min, max], which is a claim that holds for every draw. No retry attribute is used anywhere -
//! this quality bar bans it outright.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Spawn/despawn distance resolution: the override precedence and the anti-thrash band.
//!
//! The three interesting override values are all legitimate and all mean different things - -1
//! ("use the server's global"), 0 ("never materialise by proximity") and a huge value ("always
//! spawned", which is the ask in issue #100). A resolver that treated 0 as "unset" would silently
//! turn a deliberately-virtual agent into a normally-spawning one, and that mistake would be
//! invisible until someone counted AI in a town.
//!
//! The despawn half asserts the ONE invariant the engine's hysteresis depends on: despawn distance
//! is never below spawn distance. If it ever were, a group would sit permanently inside its own
//! despawn ring and thrash once per lifecycle tick.
//!
//! FAIL PROOF (edit recorded; execution belongs to the phase's suite run): change
//! ResolveSpawnDistance's guard from `if (distanceOverride < 0)` to `if (distanceOverride <= 0)`
//! and the "override 0 means never" assertion goes red. Clamp removal in ResolveDespawnDistance
//! (returning Math.Round(spawn * hysteresis) unconditionally) turns the 0.5-hysteresis assertion
//! red.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Virtualization_DistanceResolution : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// -1 defers to the configured global.
		int resolved = OVT_VirtualizationMath.ResolveSpawnDistance(-1, 1750);
		if (resolved != 1750)
		{
			SetFailure("An override of -1 resolved to %1, expected the global 1750", resolved.ToString());
			return true;
		}

		// 0 is a real answer: "this group never materialises by proximity".
		resolved = OVT_VirtualizationMath.ResolveSpawnDistance(0, 1750);
		if (resolved != 0)
		{
			SetFailure("An override of 0 resolved to %1, expected 0 - 0 means 'never spawn', not 'unset'", resolved.ToString());
			return true;
		}

		// A huge override is the "keep it spawned forever" case.
		resolved = OVT_VirtualizationMath.ResolveSpawnDistance(100000, 1750);
		if (resolved != 100000)
		{
			SetFailure("An override of 100000 resolved to %1, expected 100000", resolved.ToString());
			return true;
		}

		// A per-registration override always beats the global, in both directions.
		resolved = OVT_VirtualizationMath.ResolveSpawnDistance(300, 1750);
		if (resolved != 300)
		{
			SetFailure("An override of 300 resolved to %1, expected the override to win over the global", resolved.ToString());
			return true;
		}

		// A negative global with a deferring override cannot produce a negative ring.
		resolved = OVT_VirtualizationMath.ResolveSpawnDistance(-1, -1);
		if (resolved != 0)
		{
			SetFailure("An unset override over an unset global resolved to %1, expected 0", resolved.ToString());
			return true;
		}

		// Hysteresis widens the despawn ring.
		int despawn = OVT_VirtualizationMath.ResolveDespawnDistance(1000, 1.15);
		if (despawn != 1150)
		{
			SetFailure("A 1000 m spawn ring with 1.15 hysteresis gave a despawn ring of %1, expected 1150", despawn.ToString());
			return true;
		}

		// A hysteresis below 1 is clamped, never honoured - the band may not invert.
		despawn = OVT_VirtualizationMath.ResolveDespawnDistance(1000, 0.5);
		if (despawn < 1000)
		{
			SetFailure("A hysteresis of 0.5 gave a despawn ring of %1, which is INSIDE the 1000 m spawn ring - the group would thrash every lifecycle tick", despawn.ToString());
			return true;
		}

		// "Never spawn" stays "never": widening 0 would hand the group a ring it can never be inside.
		despawn = OVT_VirtualizationMath.ResolveDespawnDistance(0, 1.15);
		if (despawn != 0)
		{
			SetFailure("A 0 m spawn ring gave a despawn ring of %1, expected 0", despawn.ToString());
			return true;
		}

		Print("Spawn/despawn distance resolution holds for -1 / 0 / huge overrides and clamps the hysteresis band");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Importance defaulting: a registration can never fall through to vanilla's LOW tier by accident.
//!
//! Vanilla defaults every group to LOW, which is capped at 50% of the AI budget and evicted first,
//! and stamps player groups CRITICAL - so an unstamped hostile is starved exactly when a player's
//! recruit squads are saturating the budget. The resolver's job is that every path out of it is a
//! deliberate tier: an explicit request wins, an unset request takes the configured default, and a
//! nonsense configured default falls back to NORMAL rather than to 0 (which IS LOW).
//!
//! FAIL PROOF (edit recorded): make ResolveImportance return `requested` when it is negative (drop
//! the IsValidImportance guard) and the "-1 takes the configured default" assertion goes red; make
//! the final fallback `return SCR_EAISpawnImportance.LOW;` and the nonsense-default assertion goes
//! red.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Virtualization_ImportanceDefaulting : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// An explicit tier is honoured, including LOW - "filler" is a legitimate request.
		int resolved = OVT_VirtualizationMath.ResolveImportance(SCR_EAISpawnImportance.HIGH, SCR_EAISpawnImportance.NORMAL);
		if (resolved != SCR_EAISpawnImportance.HIGH)
		{
			SetFailure("An explicit HIGH resolved to %1", resolved.ToString());
			return true;
		}

		resolved = OVT_VirtualizationMath.ResolveImportance(SCR_EAISpawnImportance.LOW, SCR_EAISpawnImportance.NORMAL);
		if (resolved != SCR_EAISpawnImportance.LOW)
		{
			SetFailure("An explicit LOW resolved to %1 - an explicitly requested filler tier must be honoured", resolved.ToString());
			return true;
		}

		// -1 is the documented "unset" sentinel and takes the configured default.
		resolved = OVT_VirtualizationMath.ResolveImportance(-1, SCR_EAISpawnImportance.HIGH);
		if (resolved != SCR_EAISpawnImportance.HIGH)
		{
			SetFailure("An unset request over a HIGH default resolved to %1, expected HIGH", resolved.ToString());
			return true;
		}

		// A tier above the enum is nonsense and must not be stamped as-is.
		resolved = OVT_VirtualizationMath.ResolveImportance(99, SCR_EAISpawnImportance.NORMAL);
		if (resolved != SCR_EAISpawnImportance.NORMAL)
		{
			SetFailure("A request of 99 resolved to %1, expected the NORMAL default", resolved.ToString());
			return true;
		}

		// Both unset: NORMAL, never LOW.
		resolved = OVT_VirtualizationMath.ResolveImportance(-1, -1);
		if (resolved != SCR_EAISpawnImportance.NORMAL)
		{
			SetFailure("An unset request over an unset default resolved to %1, expected NORMAL - falling through to LOW would budget-starve campaign AI", resolved.ToString());
			return true;
		}

		Print("Importance defaulting resolves to a deliberate tier on every path");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Waypoint-plan integrity: a ragged plan is refused, an empty plan is accepted.
//!
//! The plan's three arrays are consumed BY INDEX when the real waypoint entities are built, so a
//! short parameter array would silently give the last waypoint whatever the builder's default
//! happens to be - a patrol with a zero radius, or a wait with someone else's duration. An empty
//! plan, on the other hand, is a perfectly ordinary garrison and must not be refused.
//!
//! FAIL PROOF (edit recorded): delete the `waypointParams.Count() != positions.Count()` check and
//! the "short parameter array" assertion goes red; make the empty-array case return false (e.g.
//! by adding `if (positions.IsEmpty()) return false;`) and the empty-plan assertion goes red.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Virtualization_WaypointPlanValidation : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<vector> positions = {"0 0 0", "10 0 10"};
		array<int> types = {OVT_EVirtualWaypointType.MOVE, OVT_EVirtualWaypointType.PATROL};
		array<float> waypointParams = {0, 50};

		if (!OVT_VirtualizationMath.ValidateWaypointPlan(positions, types, waypointParams))
		{
			SetFailure("A well-formed two-waypoint plan was rejected");
			return true;
		}

		// An empty plan is a group with no waypoints - legitimate.
		array<vector> noPositions = {};
		array<int> noTypes = {};
		array<float> noParams = {};
		if (!OVT_VirtualizationMath.ValidateWaypointPlan(noPositions, noTypes, noParams))
		{
			SetFailure("An empty plan was rejected - a garrison with no waypoints is legitimate");
			return true;
		}

		// One type short.
		array<int> shortTypes = {OVT_EVirtualWaypointType.MOVE};
		if (OVT_VirtualizationMath.ValidateWaypointPlan(positions, shortTypes, waypointParams))
		{
			SetFailure("A plan with 2 positions and 1 type was accepted - the arrays are consumed by index");
			return true;
		}

		// One parameter short.
		array<float> shortParams = {0};
		if (OVT_VirtualizationMath.ValidateWaypointPlan(positions, types, shortParams))
		{
			SetFailure("A plan with 2 positions and 1 parameter was accepted - the last waypoint would silently take a default radius/duration");
			return true;
		}

		// A type outside the enum would fall through to whatever the builder's default branch is.
		array<int> badTypes = {OVT_EVirtualWaypointType.MOVE, 99};
		if (OVT_VirtualizationMath.ValidateWaypointPlan(positions, badTypes, waypointParams))
		{
			SetFailure("A plan containing waypoint type 99 was accepted");
			return true;
		}

		// A null array is a caller bug, not a plan.
		if (OVT_VirtualizationMath.ValidateWaypointPlan(positions, null, waypointParams))
		{
			SetFailure("A plan with a null type array was accepted");
			return true;
		}

		Print("Waypoint-plan validation accepts empty and well-formed plans and refuses ragged ones");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The survivor mask: counting the living, and what does and does not count as a wipe.
//!
//! The mask is the ONLY roster truth in this feature - the engine's dormant alive/dead counts are
//! scratch values that provably corrupt themselves when a despawn lands mid-refill. Wipe detection
//! is the destructive half of that truth: it removes the record and fires the wipe event, so the
//! "unknown roster" case matters as much as the "everyone died" case. A record whose roster has
//! not been captured yet has an EMPTY mask, and reporting that as a wipe would delete records that
//! never had a member in the first place.
//!
//! FAIL PROOF (edit recorded): make IsWiped() return `CountAlive(mask) == 0` without the
//! null/empty guard and the "an empty mask is not a wipe" assertion goes red.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Virtualization_MaskCountingAndWipe : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<int> full = {1, 1, 1, 1, 1, 1};
		if (OVT_VirtualizationMath.CountAlive(full) != 6)
		{
			SetFailure("A six-slot all-alive mask counted %1 alive", OVT_VirtualizationMath.CountAlive(full).ToString());
			return true;
		}

		if (OVT_VirtualizationMath.IsWiped(full))
		{
			SetFailure("An all-alive mask was reported as wiped");
			return true;
		}

		// Three of six killed - the F4 shape.
		array<int> partial = {1, 0, 1, 0, 1, 0};
		if (OVT_VirtualizationMath.CountAlive(partial) != 3)
		{
			SetFailure("A mask with three dead slots counted %1 alive, expected 3", OVT_VirtualizationMath.CountAlive(partial).ToString());
			return true;
		}

		if (OVT_VirtualizationMath.IsWiped(partial))
		{
			SetFailure("A partially-killed mask was reported as wiped");
			return true;
		}

		array<int> wiped = {0, 0, 0};
		if (OVT_VirtualizationMath.CountAlive(wiped) != 0)
		{
			SetFailure("An all-dead mask counted %1 alive", OVT_VirtualizationMath.CountAlive(wiped).ToString());
			return true;
		}

		if (!OVT_VirtualizationMath.IsWiped(wiped))
		{
			SetFailure("An all-dead mask was NOT reported as wiped");
			return true;
		}

		// "Roster unknown" is not "everyone died".
		array<int> empty = {};
		if (OVT_VirtualizationMath.IsWiped(empty))
		{
			SetFailure("An EMPTY mask was reported as wiped - a record whose roster has not been captured yet would be deleted");
			return true;
		}

		if (OVT_VirtualizationMath.IsWiped(null))
		{
			SetFailure("A null mask was reported as wiped");
			return true;
		}

		if (OVT_VirtualizationMath.CountAlive(null) != 0)
		{
			SetFailure("A null mask counted %1 alive", OVT_VirtualizationMath.CountAlive(null).ToString());
			return true;
		}

		Print("Mask counting and wipe detection hold, including the empty 'roster unknown' mask");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The refill selector: which roster slot the next queued member spawns as.
//!
//! This is the whole of slot-accurate survivor truth in one function. Vanilla always spawns
//! slot index == current agent count, a first-N refill that structurally destroys identity: a
//! group that lost its slot-1 machinegunner comes back at the right STRENGTH with the MG alive and
//! a tail rifleman missing instead. The selector must skip dead slots, skip slots already
//! materialised this activation (the queue dispatches one member at a time, so it is asked
//! repeatedly), and report exhaustion rather than looping.
//!
//! FAIL PROOF (edit recorded): drop the `spawnedSlots.Contains(i)` skip and the "already
//! materialised" assertion goes red (it returns 0 forever); drop the `mask[i] == 0` skip and the
//! first assertion returns the dead slot 1.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Virtualization_NextSlotToSpawn : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// Slot 1 (say, the machinegunner) is dead; nothing has spawned yet.
		array<int> mask = {1, 0, 1, 1};
		array<int> spawned = {};

		int next = OVT_VirtualizationMath.NextSlotToSpawn(mask, spawned);
		if (next != 0)
		{
			SetFailure("The first refill slot was %1, expected 0", next.ToString());
			return true;
		}

		// Slot 0 is now in the world, so the dead slot 1 must be skipped in favour of slot 2.
		spawned.Insert(0);
		next = OVT_VirtualizationMath.NextSlotToSpawn(mask, spawned);
		if (next != 2)
		{
			SetFailure("With slot 0 materialised and slot 1 dead, the selector returned %1, expected 2", next.ToString());
			return true;
		}

		spawned.Insert(2);
		next = OVT_VirtualizationMath.NextSlotToSpawn(mask, spawned);
		if (next != 3)
		{
			SetFailure("With slots 0 and 2 materialised, the selector returned %1, expected 3", next.ToString());
			return true;
		}

		// Every surviving slot is in the world: exhausted, not looping.
		spawned.Insert(3);
		next = OVT_VirtualizationMath.NextSlotToSpawn(mask, spawned);
		if (next != -1)
		{
			SetFailure("With every surviving slot materialised, the selector returned %1, expected -1", next.ToString());
			return true;
		}

		// A null "already spawned" list means nothing has spawned yet.
		next = OVT_VirtualizationMath.NextSlotToSpawn(mask, null);
		if (next != 0)
		{
			SetFailure("With a null spawned-slot list the selector returned %1, expected 0", next.ToString());
			return true;
		}

		// A wiped mask has nothing to refill.
		array<int> wiped = {0, 0};
		next = OVT_VirtualizationMath.NextSlotToSpawn(wiped, null);
		if (next != -1)
		{
			SetFailure("A wiped mask offered slot %1 to refill, expected -1", next.ToString());
			return true;
		}

		if (OVT_VirtualizationMath.NextSlotToSpawn(null, null) != -1)
		{
			SetFailure("A null mask offered a slot to refill");
			return true;
		}

		Print("The refill selector skips dead and already-materialised slots and reports exhaustion");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Ambient round-robin arithmetic: the slice wraps, never repeats within a tick, and survives the
//! degenerate collection sizes.
//!
//! The ambient tick evaluates a SLICE of the sources each pass so that a large source list cannot
//! spike one frame. The two failure modes worth pinning are an index appearing twice in one slice
//! (a source would evaluate - and roll its spawn - twice in a single tick) and a modulo against a
//! zero count (no sources at all is the normal state of a fresh campaign).
//!
//! FAIL PROOF (edit recorded): remove the `if (take > count) take = count;` clamp and the
//! "slice larger than the collection" assertion goes red with a repeated index; remove the
//! `count <= 0` guard in AdvanceCursor and the empty-collection assertion becomes a division error.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Virtualization_SliceArithmetic : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// A plain slice from the start of the collection.
		array<int> expected = {0, 1, 2};
		array<int> slice = OVT_VirtualizationMath.SliceIndices(10, 3, 0);
		if (!SliceEquals(slice, expected))
		{
			SetFailure("SliceIndices(10, 3, 0) returned %1, expected 0,1,2", Describe(slice));
			return true;
		}

		// Wrapping past the end.
		expected = {4, 0, 1};
		slice = OVT_VirtualizationMath.SliceIndices(5, 3, 4);
		if (!SliceEquals(slice, expected))
		{
			SetFailure("SliceIndices(5, 3, 4) returned %1, expected 4,0,1", Describe(slice));
			return true;
		}

		// A slice bigger than the collection covers it exactly once.
		slice = OVT_VirtualizationMath.SliceIndices(3, 8, 1);
		if (slice.Count() != 3)
		{
			SetFailure("SliceIndices(3, 8, 1) returned %1 entries, expected 3 - a repeated index would evaluate a source twice in one tick", slice.Count().ToString());
			return true;
		}

		expected = {1, 2, 0};
		if (!SliceEquals(slice, expected))
		{
			SetFailure("SliceIndices(3, 8, 1) returned %1, expected 1,2,0", Describe(slice));
			return true;
		}

		// No sources: nothing to do, and no modulo against zero.
		slice = OVT_VirtualizationMath.SliceIndices(0, 4, 0);
		if (!slice.IsEmpty())
		{
			SetFailure("SliceIndices with a count of 0 returned %1 entries", slice.Count().ToString());
			return true;
		}

		slice = OVT_VirtualizationMath.SliceIndices(10, 0, 0);
		if (!slice.IsEmpty())
		{
			SetFailure("SliceIndices with a slice size of 0 returned %1 entries", slice.Count().ToString());
			return true;
		}

		// A cursor beyond the collection is normalised rather than skipping everything.
		expected = {1, 2};
		slice = OVT_VirtualizationMath.SliceIndices(4, 2, 9);
		if (!SliceEquals(slice, expected))
		{
			SetFailure("SliceIndices(4, 2, 9) returned %1, expected 1,2", Describe(slice));
			return true;
		}

		// Cursor advance wraps in step with the slice.
		int cursor = OVT_VirtualizationMath.AdvanceCursor(4, 3, 5);
		if (cursor != 2)
		{
			SetFailure("AdvanceCursor(4, 3, 5) returned %1, expected 2", cursor.ToString());
			return true;
		}

		cursor = OVT_VirtualizationMath.AdvanceCursor(0, 3, 0);
		if (cursor != 0)
		{
			SetFailure("AdvanceCursor over an empty collection returned %1, expected 0", cursor.ToString());
			return true;
		}

		Print("Ambient slice arithmetic wraps, clamps and survives empty collections");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] actual Slice under test.
	//! \param[in] expected Expected indices in order.
	//! \return True when both hold the same indices in the same order.
	protected bool SliceEquals(array<int> actual, array<int> expected)
	{
		if (!actual || actual.Count() != expected.Count())
			return false;

		for (int i = 0; i < expected.Count(); i++)
		{
			if (actual[i] != expected[i])
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] slice Slice to describe.
	//! \return A comma-separated rendering, for failure messages.
	protected string Describe(array<int> slice)
	{
		if (!slice || slice.IsEmpty())
			return "(empty)";

		string text = slice[0].ToString();
		for (int i = 1; i < slice.Count(); i++)
		{
			text = text + "," + slice[i].ToString();
		}

		return text;
	}
}

//------------------------------------------------------------------------------------------------
//! RollCountSafe: the inclusive count roll, guarded against two documented RandInt traps.
//!
//! RandInt is MAX-EXCLUSIVE, and RandInt(n, n) raises an ENGINE ERROR rather than returning n -
//! which is exactly what a config that wants "always spawn 3 civilians" produces. The equality
//! branch is therefore the important one, and it is fully deterministic. The random branch is
//! asserted for containment only, which is true of every draw.
//!
//! FAIL PROOF (edit recorded): drop the `if (lo == hi) return lo;` short-circuit and the min==max
//! assertion goes red with an engine error; drop the `+ 1` and the containment loop eventually
//! fails to ever produce the maximum (assert the observed maximum instead to see it red
//! immediately).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Virtualization_RollCountSafe : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// min == max is the engine-error case; it must short-circuit, and it must give back the value.
		int rolled = OVT_VirtualizationMath.RollCountSafe(3, 3);
		if (rolled != 3)
		{
			SetFailure("RollCountSafe(3, 3) returned %1, expected 3", rolled.ToString());
			return true;
		}

		rolled = OVT_VirtualizationMath.RollCountSafe(0, 0);
		if (rolled != 0)
		{
			SetFailure("RollCountSafe(0, 0) returned %1, expected 0", rolled.ToString());
			return true;
		}

		// The random branch stays inside its inclusive bounds, every draw.
		for (int i = 0; i < 50; i++)
		{
			rolled = OVT_VirtualizationMath.RollCountSafe(2, 5);
			if (rolled < 2 || rolled > 5)
			{
				SetFailure("RollCountSafe(2, 5) returned %1, outside the inclusive range", rolled.ToString());
				return true;
			}
		}

		// An inverted pair is a mis-authored config, not a reason to refuse to spawn.
		for (int j = 0; j < 50; j++)
		{
			rolled = OVT_VirtualizationMath.RollCountSafe(5, 2);
			if (rolled < 2 || rolled > 5)
			{
				SetFailure("RollCountSafe(5, 2) returned %1, expected a value inside [2, 5]", rolled.ToString());
				return true;
			}
		}

		Print("RollCountSafe short-circuits min == max and stays inside its inclusive bounds");
		return true;
	}
}
