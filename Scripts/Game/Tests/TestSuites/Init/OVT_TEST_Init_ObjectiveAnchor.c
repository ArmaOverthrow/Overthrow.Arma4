//------------------------------------------------------------------------------------------------
//! TIER B - the objective ANCHOR where it meets live components: absent by default, written into the
//! candidate's sort key and never into its threat, and pushed by the director with the phase's reach.
//!
//! WHAT THIS TIER CAN SEE THAT THE CHEAP ONE CANNOT. The bias arithmetic is a pure function of four
//! floats and is pinned in OVT_TEST_Logic_ObjectiveAnchorAndBearing.c. Three things about it are NOT
//! arithmetic and can only be asserted against real components:
//!
//!   1. THAT NO FACTION IN A RUNNING WORLD IS BIASED unless something deliberately biases it. This is
//!      the whole "no anchor is byte-identical to today" claim reduced to something a case can state:
//!      the evaluator's candidate loop resolves an anchor per faction per pass, and if every one of
//!      those lookups answers null then every faction's candidates take exactly the path they took
//!      before this feature existed.
//!   2. WHICH FIELD THE BIAS IS WRITTEN INTO. OVT_CandidatePosition carries two floats seeded from the
//!      same number, and only one of them may ever be biased - sortBy orders the pass, threatLevel
//!      decides what a position may BUY (it is handed to FindBestDeploymentConfig and so to
//!      CheckDeploymentConditions' m_iMinimumThreatLevel gate, which three shipped configs author
//!      above zero) and is then stamped onto the created deployment and persisted. Biasing the wrong
//!      one compiles, sorts identically, and quietly lets an objective-adjacent position buy configs
//!      its real threat cannot afford. There is no cheaper tier that can tell the two apart.
//!   3. THAT THE DIRECTOR AND THE FRAMEWORK AGREE. The director pushes at the occupying faction's
//!      index and the evaluator reads at the faction index it is evaluating; a mismatch there is
//!      silent, permanent, and looks exactly like a feature that does nothing.
//!
//! ⚠ THE EVALUATOR IS NOT DRIVEN, AND THAT IS DELIBERATE. EvaluateFactionDeployments() creates real
//! deployments, debits a real pool and is gated on a live player count; driving it to assert one
//! assignment would mutate the shared initialisation world for every case that follows and could not
//! be made deterministic. The two seams it uses are asserted directly instead - which is also why
//! ApplyObjectiveAnchorBias() is public, on the precedent FindBestDeploymentConfig() set in the same
//! file.
//!
//! ⚠ NOTHING HERE IS SCOPED TO A GLOBAL COUNT. The initialisation and persistence worlds run a live
//! deployment wave; every assertion below is about this file's own fixture positions, its own hand-
//! built candidates, and one faction index it sets and clears itself.
//!
//! ⚠ CASE ORDER MATTERS AND THE NAMES ARE CHOSEN FOR IT. Cases run alphabetically by class name.
//! `...AbsentForEveryFactionByDefault` asserts a pristine store, so it must run before every case that
//! writes to one - A sorts before B, D and N. The writing cases restore anyway, so the ordering is
//! belt and braces rather than either alone.
//!
//! No polling, no waiting, no maxAttempts: nothing here has a clock, and every subject is either a
//! hand-built object or a single synchronous call.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! No faction in this world is biased, so the evaluator takes the path it took before the anchor
//! existed - and the store round-trips when something does bias one.
//!
//! THE FIRST HALF IS THE INVARIANT, STATED THE ONLY WAY IT CAN BE. "The evaluator is byte-identical
//! with no anchor" is not directly observable, but it decomposes into two things that are: the bias
//! function is the identity when it has no anchor (pinned in the cheap tier, with an exact `==`), and
//! every faction lookup in a world nobody has biased answers null (pinned here). Together those two
//! say the candidate loop never touches a candidate, which is the claim.
//!
//! THE SECOND HALF EXISTS SO THE FIRST CANNOT PASS VACUOUSLY. A store that had been renamed, never
//! allocated, or wired to always answer null would satisfy the first half perfectly. So the case then
//! sets an anchor, reads it back, replaces it, clears it, and clears a faction that was never biased
//! - and puts the store back the way it found it.
//!
//! ⚠ THE FIXTURE FACTION INDEX IS ONE NO FACTION HOLDS, deliberately. The evaluator only ever looks up
//! indices it got from the faction manager, so a synthetic index cannot reach the live wave even for
//! the few microseconds this case holds it. The one claim that genuinely needs the REAL occupying
//! index is made in ...DirectorPushesPerPhaseAndDropsOnEveryExit, where the director is driven and
//! then reset.
//!
//! PROVEN ABLE TO FAIL: SetObjectiveAnchor() was changed to store the anchor under a fixed index of 0
//! instead of the caller's factionIndex. The tree recompiled CLEAN (tools/compile-check.sh exit 0) -
//! a bias filed against the wrong faction is not a script error - and the case then reports
//! "the store must hand back the anchor it was given for the faction it was given it for, but faction
//! <n> reads back as unbiased". Line restored, tree recompiled clean.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveAnchor_AbsentForEveryFactionByDefault : SCR_AutotestCaseBase
{
	//! Tolerance for a position that has been round-tripped through the record, in metres.
	//! vector.Distance is not correctly rounded at campaign ranges, so positions are never compared
	//! with ==.
	static const float POSITION_TOLERANCE = 1.0;

	//! Tolerance for the two scalars.
	static const float EPSILON = 0.01;

	//! Somewhere no candidate in any test world sits, so even a bias that escaped this case could not
	//! reach anything. It is never traced, walked to or spawned at - it is four floats in a record.
	static const vector FIXTURE_POSITION = "12000 0 12000";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (!deployments)
		{
			SetFailure("OVT_Global.GetDeploymentManager() is null - the deployment framework did not resolve");
			return true;
		}

		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
		{
			SetFailure("The faction manager did not resolve, so 'no faction is biased' could not be asked of anybody");
			return true;
		}

		array<Faction> factions = new array<Faction>();
		factionManager.GetFactionsList(factions);

		if (factions.IsEmpty())
		{
			SetFailure("The world has no factions at all, so the absent-by-default claim would pass vacuously");
			return true;
		}

		// --- HALF ONE: nobody is biased. Every index the evaluator could ever look up answers null.
		int highestIndex = -1;
		foreach (Faction faction : factions)
		{
			int factionIndex = factionManager.GetFactionIndex(faction);
			if (factionIndex > highestIndex)
				highestIndex = factionIndex;

			if (deployments.GetObjectiveAnchor(factionIndex))
			{
				SetFailure("faction %1 is biased in a world where nothing has set a bias - the evaluator would not be taking the path it took before the objective anchor existed",
					factionIndex.ToString());
				return true;
			}
		}

		// --- HALF TWO: the store works, so half one is a fact about this world and not about a store
		//     that never answers anything. An index no faction holds, so the live wave cannot see it.
		int fixtureIndex = highestIndex + 100;

		deployments.SetObjectiveAnchor(fixtureIndex, FIXTURE_POSITION, 600, 25);

		OVT_DeploymentObjectiveAnchor stored = deployments.GetObjectiveAnchor(fixtureIndex);
		if (!stored)
		{
			SetFailure("the store must hand back the anchor it was given for the faction it was given it for, but faction %1 reads back as unbiased",
				fixtureIndex.ToString());
			return true;
		}

		if (vector.Distance(stored.position, FIXTURE_POSITION) > POSITION_TOLERANCE)
		{
			SetFailure("the stored anchor is at %1, but it was set at %2",
				stored.position.ToString(), FIXTURE_POSITION.ToString());
			deployments.ClearObjectiveAnchor(fixtureIndex);
			return true;
		}

		if (Math.AbsFloat(stored.radius - 600) > EPSILON || Math.AbsFloat(stored.weight - 25) > EPSILON)
		{
			SetFailure("the stored anchor reads radius %1 and weight %2, set as 600 and 25",
				stored.radius.ToString(), stored.weight.ToString());
			deployments.ClearObjectiveAnchor(fixtureIndex);
			return true;
		}

		// --- A faction has ONE objective, so setting again replaces rather than stacking.
		deployments.SetObjectiveAnchor(fixtureIndex, FIXTURE_POSITION, 1200, 40);

		stored = deployments.GetObjectiveAnchor(fixtureIndex);
		if (!stored || Math.AbsFloat(stored.radius - 1200) > EPSILON)
		{
			SetFailure("setting an anchor twice must REPLACE the first, but the store still reports radius %1 rather than 1200",
				stored.radius.ToString());
			deployments.ClearObjectiveAnchor(fixtureIndex);
			return true;
		}

		// --- A dead anchor is refused rather than stored, so "is there an anchor?" is the whole test
		//     at the call site.
		deployments.SetObjectiveAnchor(fixtureIndex, FIXTURE_POSITION, 0, 40);
		if (deployments.GetObjectiveAnchor(fixtureIndex))
		{
			SetFailure("an anchor with no radius must be refused rather than stored, or the evaluator would carry a bias that reaches nowhere");
			deployments.ClearObjectiveAnchor(fixtureIndex);
			return true;
		}

		deployments.SetObjectiveAnchor(fixtureIndex, FIXTURE_POSITION, 1200, 0);
		if (deployments.GetObjectiveAnchor(fixtureIndex))
		{
			SetFailure("an anchor with no weight must be refused rather than stored");
			deployments.ClearObjectiveAnchor(fixtureIndex);
			return true;
		}

		// --- And clearing is safe on a faction nobody biased, which every teardown path relies on.
		deployments.SetObjectiveAnchor(fixtureIndex, FIXTURE_POSITION, 1200, 40);
		deployments.ClearObjectiveAnchor(fixtureIndex);
		deployments.ClearObjectiveAnchor(fixtureIndex);

		if (deployments.GetObjectiveAnchor(fixtureIndex))
		{
			SetFailure("clearing an anchor must remove it - faction %1 is still biased after being cleared twice",
				fixtureIndex.ToString());
			return true;
		}

		Print("Objective anchor: no faction in this world is biased, so every candidate the evaluator scores takes the unbiased path - and the store sets, replaces, refuses a dead anchor and clears");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The bias is written into the candidate's SORT KEY and never into its THREAT - and the sort the
//! evaluator actually runs puts the objective-adjacent candidate first.
//!
//! ⚠ THIS IS THE HIGHEST-VALUE ASSERTION IN THE WHOLE FEATURE, AND IT IS AN ASSIGNMENT. The two floats
//! on OVT_CandidatePosition are seeded from the same number by its constructor, so a bias written into
//! the wrong one produces an identical sort order and passes every ordering test that could be
//! written. What it also does is raise the number the evaluator hands to FindBestDeploymentConfig(),
//! which hands it to CheckDeploymentConditions(), where m_iMinimumThreatLevel is a hard gate - and
//! three shipped configs author it above zero. An anchor would then make configs BUYABLE at a
//! position whose real threat cannot afford them, and the inflated figure would be stamped onto the
//! deployment by SetThreatLevel() and written into the save. "Ordering, not eligibility" is the
//! binding invariant of this change, and this is where it is checked.
//!
//! THE SORT IS THE REAL ONE. The candidates are the real class, carrying the real [SortAttribute()],
//! sorted with the same array.Sort(true) the evaluator calls - so a change to which field carries the
//! attribute, or to the sort direction, is caught here as well.
//!
//! THE NO-ANCHOR CONTROL IS ASSERTED WITH ==, not with the epsilon this tier otherwise uses. "The
//! evaluator sorts on the same float it sorted on before" is only true if the candidate is handed
//! back untouched, and an epsilon would accept an implementation that recomputed it.
//!
//! PROVEN ABLE TO FAIL: the assignment in ApplyObjectiveAnchorBias() was changed to write
//! candidate.threatLevel instead of candidate.sortBy - the exact mistake §3.5's own illustrative
//! snippet would have produced. The tree recompiled CLEAN (tools/compile-check.sh exit 0) and the case
//! then reports "the bias must NEVER touch threatLevel - that field decides what a position may buy
//! and is written into the save: got 125, expected 100". Line restored, tree recompiled clean.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveAnchor_BiasWritesTheSortKeyAndNeverTheThreat : SCR_AutotestCaseBase
{
	//! Tolerance for the biased scalar.
	static const float EPSILON = 0.01;

	//! Where the fixture objective is. Never traced, walked to or spawned at.
	static const vector ANCHOR_POSITION = "12000 0 12000";

	//! Reach and strength of the fixture anchor.
	static const float ANCHOR_RADIUS = 600.0;
	static const float ANCHOR_WEIGHT = 25.0;

	//! The quieter candidate, sitting on the objective.
	static const float NEAR_THREAT = 100.0;

	//! The busier candidate, well outside the band - busier by less than the weight, so the bias is
	//! what decides the order and the case is not measuring an inequality that was already true.
	static const float FAR_THREAT = 110.0;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (!deployments)
		{
			SetFailure("OVT_Global.GetDeploymentManager() is null - the deployment framework did not resolve");
			return true;
		}

		OVT_DeploymentObjectiveAnchor anchor = new OVT_DeploymentObjectiveAnchor();
		anchor.position = ANCHOR_POSITION;
		anchor.radius = ANCHOR_RADIUS;
		anchor.weight = ANCHOR_WEIGHT;

		OVT_CandidatePosition near = new OVT_CandidatePosition(ANCHOR_POSITION, NEAR_THREAT);
		OVT_CandidatePosition far = new OVT_CandidatePosition("0 0 0", FAR_THREAT);

		deployments.ApplyObjectiveAnchorBias(near, anchor);
		deployments.ApplyObjectiveAnchorBias(far, anchor);

		// --- THE INVARIANT. Neither candidate's threat may have moved by so much as a bit, biased or
		//     not: that field is eligibility and the save record, and it is not this feature's to touch.
		if (near.threatLevel != NEAR_THREAT)
		{
			SetFailure("the bias must NEVER touch threatLevel - that field decides what a position may buy and is written into the save: got %1, expected %2",
				near.threatLevel.ToString(), NEAR_THREAT.ToString());
			return true;
		}

		if (far.threatLevel != FAR_THREAT)
		{
			SetFailure("an out-of-band candidate's threatLevel must be untouched: got %1, expected %2",
				far.threatLevel.ToString(), FAR_THREAT.ToString());
			return true;
		}

		// --- The sort key, on the other hand, carries the bias - by exactly the weight at the anchor.
		if (Math.AbsFloat(near.sortBy - (NEAR_THREAT + ANCHOR_WEIGHT)) > EPSILON)
		{
			SetFailure("a candidate ON the objective must sort as its threat plus the full weight: got %1, expected %2",
				near.sortBy.ToString(), (NEAR_THREAT + ANCHOR_WEIGHT).ToString());
			return true;
		}

		if (far.sortBy != FAR_THREAT)
		{
			SetFailure("a candidate outside the band must sort on exactly the number it was constructed with: got %1, expected %2",
				far.sortBy.ToString(), FAR_THREAT.ToString());
			return true;
		}

		// --- THE ORDERING, through the sort the evaluator really runs. Inserted busiest-first so a
		//     sort that did nothing at all would leave the wrong candidate in front.
		array<ref OVT_CandidatePosition> candidates = new array<ref OVT_CandidatePosition>();
		candidates.Insert(far);
		candidates.Insert(near);
		candidates.Sort(true);

		if (candidates[0] != near)
		{
			SetFailure("after the evaluator's own sort the objective-adjacent candidate must lead: it sorted %1 ahead of %2",
				candidates[0].sortBy.ToString(), near.sortBy.ToString());
			return true;
		}

		// --- THE CONTROL: with no anchor the same two candidates keep their own order, untouched.
		OVT_DeploymentObjectiveAnchor noAnchor = null;

		OVT_CandidatePosition unbiasedNear = new OVT_CandidatePosition(ANCHOR_POSITION, NEAR_THREAT);
		OVT_CandidatePosition unbiasedFar = new OVT_CandidatePosition("0 0 0", FAR_THREAT);

		deployments.ApplyObjectiveAnchorBias(unbiasedNear, noAnchor);
		deployments.ApplyObjectiveAnchorBias(unbiasedFar, noAnchor);

		if (unbiasedNear.sortBy != NEAR_THREAT || unbiasedNear.threatLevel != NEAR_THREAT)
		{
			SetFailure("with no anchor a candidate must be handed back exactly as constructed: sortBy %1, threatLevel %2",
				unbiasedNear.sortBy.ToString(), unbiasedNear.threatLevel.ToString());
			return true;
		}

		array<ref OVT_CandidatePosition> unbiased = new array<ref OVT_CandidatePosition>();
		unbiased.Insert(unbiasedFar);
		unbiased.Insert(unbiasedNear);
		unbiased.Sort(true);

		if (unbiased[0] != unbiasedFar)
		{
			SetFailure("with no anchor the busier candidate must still lead, or the ordering above is an artefact of the sort rather than of the bias");
			return true;
		}

		Print("Objective anchor: the bias is written into the candidate's sort key and never into its threat, so it reorders the pass without changing what any position may buy - and with no anchor both fields come back untouched");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The director pushes an anchor at the occupying faction, widens it with the phase, and drops it on
//! BOTH ways of ending up with no objective.
//!
//! WHY THE INDEX IS A CLAIM. The director pushes at the occupying faction's index and the evaluator
//! reads at whichever index it is evaluating. Nothing connects the two but the number, nothing checks
//! it, and a mismatch produces a feature that compiles, ticks, logs, persists and does nothing at all.
//!
//! WHY BOTH DROP PATHS ARE ASSERTED. The reset path is the obvious one. The other is not: a
//! re-selection that runs during harassment and finds nothing selectable goes idle WITHOUT passing
//! through the reset method, and an implementation that dropped the anchor only in the reset would
//! leave the occupying faction leaning on a place it had already given up on - silently, and for the
//! rest of the campaign. Both paths are driven here.
//!
//! ⚠ THE RADII ARE ASSERTED AGAINST THE DIRECTOR'S OWN CONSTANTS, NOT AGAINST 600 AND 1200. A case
//! that hard-coded the numbers would fail the day somebody tuned them, which is not a defect. What is
//! a defect is the two phases sharing one reach, or the forward phase being the NARROWER of the two -
//! so those are the relationships asserted, plus the fact that a phase with no objective carries no
//! reach at all.
//!
//! ⚠ EVERYTHING IS PUT BACK. The initialisation world is shared, and the director is left with no
//! objective and no anchor whatever this case does - including on every failure path, which is why
//! the teardown runs before the assertions rather than after them.
//!
//! PROVEN ABLE TO FAIL: the PushObjectiveAnchor() call was removed from EnterPhase(). The tree
//! recompiled CLEAN (tools/compile-check.sh exit 0) - a bias nobody pushes is not a script error, and
//! nothing in the campaign logs its absence - and the case then reports "committing to an objective
//! must bias the occupying faction toward it, but faction <n> reads back as unbiased". Line restored,
//! tree recompiled clean.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveAnchor_DirectorPushesPerPhaseAndDropsOnEveryExit : SCR_AutotestCaseBase
{
	//! Tolerance for a position that has been round-tripped through the record, in metres.
	static const float POSITION_TOLERANCE = 1.0;

	//! Tolerance for the two scalars.
	static const float EPSILON = 0.01;

	//! Where the fixture objective is. Deliberately nowhere near anything: the anchor is only ever
	//! read as three numbers, and this case holds it for one synchronous call at a time.
	static const vector FIXTURE_POSITION = "12000 0 12000";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!director || !deployments || !config)
		{
			SetFailure("The director, the deployment framework or the config did not resolve");
			return true;
		}

		int occupyingIndex = config.GetOccupyingFactionIndex();

		// --- ARRANGE: commit to an objective. This enters the harassment phase, which is the only
		//     push site the live machine uses.
		director.CommitObjective(OVT_EObjectiveKind.TOWN, FIXTURE_POSITION, "anchor fixture");

		OVT_DeploymentObjectiveAnchor harassment = CopyAnchor(deployments.GetObjectiveAnchor(occupyingIndex));

		// --- ACT: walk the phases the objective really goes through.
		director.EnterPhase(OVT_EObjectivePhase.FOB);
		OVT_DeploymentObjectiveAnchor forward = CopyAnchor(deployments.GetObjectiveAnchor(occupyingIndex));

		director.EnterPhase(OVT_EObjectivePhase.COUNTER_QRF);
		OVT_DeploymentObjectiveAnchor battle = CopyAnchor(deployments.GetObjectiveAnchor(occupyingIndex));

		// --- DROP PATH ONE: the reset.
		director.ResetObjective("initialisation-tier anchor fixture torn down", false);
		bool clearedByReset = deployments.GetObjectiveAnchor(occupyingIndex) == null;

		// --- DROP PATH TWO: going idle without passing through the reset, which is what a re-selection
		//     that finds nothing selectable does.
		director.CommitObjective(OVT_EObjectiveKind.BASE, FIXTURE_POSITION, "anchor fixture");
		bool pushedForABase = deployments.GetObjectiveAnchor(occupyingIndex) != null;

		director.EnterPhase(OVT_EObjectivePhase.IDLE);
		bool clearedByIdle = deployments.GetObjectiveAnchor(occupyingIndex) == null;

		// --- RESTORE, BEFORE ASSERTING. The world is shared and a SetFailure returns immediately.
		director.ResetObjective("initialisation-tier anchor fixture torn down", false);
		deployments.ClearObjectiveAnchor(occupyingIndex);

		// --- ASSERT: the push happened, at the right index, at the right place.
		if (!harassment)
		{
			SetFailure("committing to an objective must bias the occupying faction toward it, but faction %1 reads back as unbiased",
				occupyingIndex.ToString());
			return true;
		}

		if (vector.Distance(harassment.position, FIXTURE_POSITION) > POSITION_TOLERANCE)
		{
			SetFailure("the bias must point at the objective: it is at %1, the objective is at %2",
				harassment.position.ToString(), FIXTURE_POSITION.ToString());
			return true;
		}

		if (Math.AbsFloat(harassment.weight - director.GetObjectiveAnchorWeight()) > EPSILON)
		{
			SetFailure("the pushed weight must be the director's own: pushed %1, the director holds %2",
				harassment.weight.ToString(), director.GetObjectiveAnchorWeight().ToString());
			return true;
		}

		// --- ASSERT: the reach follows the phase.
		if (!ExpectRadius(harassment, director.AnchorRadiusForPhase(OVT_EObjectivePhase.HARASSMENT), "harassment"))
			return true;

		if (!forward)
		{
			SetFailure("entering the forward-base phase must leave the occupying faction biased, but the anchor is gone");
			return true;
		}

		if (!ExpectRadius(forward, director.AnchorRadiusForPhase(OVT_EObjectivePhase.FOB), "the forward base"))
			return true;

		if (!battle)
		{
			SetFailure("entering the counter-attack phase must leave the occupying faction biased, but the anchor is gone");
			return true;
		}

		if (!ExpectRadius(battle, director.AnchorRadiusForPhase(OVT_EObjectivePhase.COUNTER_QRF), "the counter-attack"))
			return true;

		// --- ...and the relationship between the two reaches, which is the part a tuner must not break.
		if (forward.radius <= harassment.radius)
		{
			SetFailure("the forward phases must reach FURTHER than harassment - the approach and the forward base both need garrisoning, not just the target: harassment %1, forward %2",
				harassment.radius.ToString(), forward.radius.ToString());
			return true;
		}

		// --- A phase with no objective carries no reach at all, so an unhandled phase fails safe.
		if (director.AnchorRadiusForPhase(OVT_EObjectivePhase.IDLE) > 0)
		{
			SetFailure("an idle objective must carry no reach, but the director reports %1",
				director.AnchorRadiusForPhase(OVT_EObjectivePhase.IDLE).ToString());
			return true;
		}

		// --- ASSERT: both ways of ending up with no objective drop the bias.
		if (!pushedForABase)
		{
			SetFailure("a base objective must be biased toward exactly as a town one is");
			return true;
		}

		if (!clearedByReset)
		{
			SetFailure("resetting the objective must drop the bias, or the occupying faction keeps leaning on a place nothing is working toward");
			return true;
		}

		if (!clearedByIdle)
		{
			SetFailure("going idle WITHOUT passing through the reset must drop the bias too - that is the path a re-selection takes when it finds nothing selectable");
			return true;
		}

		Print("Objective anchor: the director biases the occupying faction toward its objective, widens the reach from harassment to the forward phases, and drops the bias on both ways of ending up with none");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Snapshots an anchor so it can be asserted after the world has been put back.
	//! \param[in] source The live anchor, or null.
	//! \return A detached copy, or null when there was nothing to copy.
	protected OVT_DeploymentObjectiveAnchor CopyAnchor(OVT_DeploymentObjectiveAnchor source)
	{
		if (!source)
			return null;

		OVT_DeploymentObjectiveAnchor copy = new OVT_DeploymentObjectiveAnchor();
		copy.position = source.position;
		copy.radius = source.radius;
		copy.weight = source.weight;

		return copy;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one pushed reach against what the director says that phase is worth.
	//! \param[in] anchor The anchor that was pushed.
	//! \param[in] expected The reach the director declares for that phase.
	//! \param[in] label Human name of the phase.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectRadius(notnull OVT_DeploymentObjectiveAnchor anchor, float expected, string label)
	{
		if (Math.AbsFloat(anchor.radius - expected) <= EPSILON)
			return true;

		SetFailure("the reach pushed for %1 must be the one the director declares for that phase: pushed %2, declared %3",
			label, anchor.radius.ToString(), expected.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! A faction index the config could NOT resolve never reaches the anchor store.
//!
//! WHY THIS IS A CLAIM AND NOT PARANOIA. OVT_OverthrowConfigComponent resolves the occupying faction's
//! index through the FACTION MANAGER, which is a separate object with a separate lifetime, and it
//! answers -1 for "cannot tell yet" rather than guessing. The anchor store is a map<int, ...>, so a
//! negative index is a perfectly valid KEY: an unguarded push files a bias against a faction that does
//! not exist, where nothing will ever read it, nothing will ever clear it, and no symptom points back
//! at the objective machine.
//!
//! ⚠ THE FIXTURE INDEX IS -2, NOT -1, AND -1 WOULD ASSERT NOTHING. -1 is the config's NOT-YET-RESOLVED
//! sentinel, so planting it sends the accessor off to resolve the REAL index and the drive below would
//! push a perfectly good anchor. -2 comes straight back out of the cache, which is the same shape of
//! answer a config that cannot see a faction manager gives.
//!
//! ⚠ THIS DOES NOT REPRODUCE THE WORLD-EDITOR CRASH IT CAME FROM, AND DOES NOT PRETEND TO. That crash
//! is a component's OnPostInit() running in the editor, where GetGame().GetFactionManager() is null;
//! no autotest world can be made to lack a faction manager and no case can re-run an OnPostInit(). The
//! reachable half of that fix is the invariant above - a negative index is refused rather than filed -
//! and that is what is pinned here.
//!
//! ⚠ EVERYTHING IS PUT BACK BEFORE ANYTHING IS ASSERTED, on the precedent the case above sets: the
//! real index goes back into the config, the objective is reset, and BOTH the fixture index and the
//! real one are cleared from the store - so even a tree in which the guard has been deleted leaves the
//! shared initialisation world exactly as it found it.
//!
//! PROVEN ABLE TO FAIL 2026-08-19, by the orchestrator, on the All group. The mutation is one line: delete
//!     if (occupyingIndex < 0)
//!         return;
//! from OVT_ObjectiveDirectorComponent.PushObjectiveAnchor(). The tree recompiles CLEAN (a bias filed
//! under an impossible faction is not a script error) and this case then reports "an unresolvable
//! faction index must never reach the anchor store, but faction -2 reads back as biased".
//! Observed exactly that: All FAILED (1 of 361), this case and no other. Guard restored, All 361/361.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveAnchor_NegativeFactionIndexNeverReachesTheStore : SCR_AutotestCaseBase
{
	//! Where the fixture objective is. Deliberately nowhere near anything - it is three numbers in a
	//! record, held for a handful of synchronous calls.
	static const vector FIXTURE_POSITION = "12000 0 12000";

	//! What the config hands back when it cannot resolve the occupying faction. NOT -1 - see the header.
	static const int UNRESOLVABLE_INDEX = -2;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!director || !deployments || !config)
		{
			SetFailure("The director, the deployment framework or the config did not resolve");
			return true;
		}

		int realIndex = config.GetOccupyingFactionIndex();
		if (realIndex < 0)
		{
			SetFailure("This world cannot resolve its own occupying faction index, so a refused push and a world that never pushes would look identical");
			return true;
		}

		if (deployments.GetObjectiveAnchor(realIndex))
		{
			SetFailure("faction %1 is already biased before this case has done anything - a case that ran earlier did not put the store back, and the assertions below would be about its anchor rather than this one's",
				realIndex.ToString());
			return true;
		}

		// --- ARRANGE: the config can no longer tell which faction is occupying.
		config.m_iOccupyingFactionIndex = UNRESOLVABLE_INDEX;

		// --- ACT: drive the one push site the live machine uses.
		director.CommitObjective(OVT_EObjectiveKind.TOWN, FIXTURE_POSITION, "unresolvable-index fixture");

		bool filedUnderTheImpossibleIndex = deployments.GetObjectiveAnchor(UNRESOLVABLE_INDEX) != null;
		bool filedUnderTheRealIndex = deployments.GetObjectiveAnchor(realIndex) != null;

		// --- Put the index back and drive the SAME path again, so the half above cannot pass because
		//     the drive never reached the push site at all.
		config.m_iOccupyingFactionIndex = realIndex;

		director.ResetObjective("initialisation-tier unresolvable-index fixture torn down", false);
		director.CommitObjective(OVT_EObjectiveKind.TOWN, FIXTURE_POSITION, "unresolvable-index fixture");

		bool pushedOnceResolvable = deployments.GetObjectiveAnchor(realIndex) != null;

		// --- RESTORE, BEFORE ASSERTING. The world is shared and a SetFailure returns immediately.
		director.ResetObjective("initialisation-tier unresolvable-index fixture torn down", false);
		deployments.ClearObjectiveAnchor(UNRESOLVABLE_INDEX);
		deployments.ClearObjectiveAnchor(realIndex);

		// --- ASSERT.
		if (filedUnderTheImpossibleIndex)
		{
			SetFailure("an unresolvable faction index must never reach the anchor store, but faction %1 reads back as biased",
				UNRESOLVABLE_INDEX.ToString());
			return true;
		}

		if (filedUnderTheRealIndex)
		{
			SetFailure("a push made while the occupying faction could not be resolved must file NOTHING, but faction %1 came out biased",
				realIndex.ToString());
			return true;
		}

		if (!pushedOnceResolvable)
		{
			SetFailure("the same drive must bias faction %1 the moment the index resolves again - otherwise the refusal above is just a drive that never reached the push site",
				realIndex.ToString());
			return true;
		}

		Print("Objective anchor: a faction index the config cannot resolve is refused rather than filed under an impossible key, and the same push lands the moment it resolves again");

		return true;
	}
}
