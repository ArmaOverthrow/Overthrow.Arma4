//------------------------------------------------------------------------------------------------
//! TIER B - the objective PLAN FRAMEWORK: the registry is really on the prefab and really loads, the
//! validator names and skips what is broken, every clonable module carries its whole attribute list,
//! and every phase entry keeps the instance in step with the machine driving it.
//!
//! WHAT THIS TIER CAN SEE THAT THE CHEAP ONE CANNOT, and it is the whole reason this file exists:
//! ⚠ NO COMPILER READS A .conf. The plan registry is authored data referenced by GUID from the
//! game-mode prefab, so a mistyped path, a class name that does not exist, an attribute renamed in
//! script but not in the config, or a prefab line that never got saved all produce a tree that
//! compiles perfectly and a campaign in which the occupying faction quietly runs the hard-coded
//! fallback forever. tools/compile-check.sh cannot see any of it. Case A is what does.
//!
//! ⚠ CASE ORDER MATTERS AND THE NAMES ARE CHOSEN FOR IT. Cases run alphabetically by class name.
//! A and B assert on state nothing has driven, so they sort before every case that drives the
//! director. Every driving case also restores the machine to idle before it returns, so the ordering
//! is belt AND braces rather than either alone.
//!
//! ⚠ NOTHING HERE TICKS THE DIRECTOR. A tick can reach the spender, and the spender buys real
//! deployments with real resources in a live campaign; the phase-entry and commit paths are driven
//! directly instead, which is exactly what every other objective case in this tier does. No polling,
//! no waiting, no maxAttempts anywhere in this file.
//!
//! CAN-FAIL PROOFS. Running a suite is the orchestrator's job, not an implementation agent's
//! (.claude/test-policy.md), so each proof named below is a fault that was injected one at a time and
//! compiled - each exited tools/compile-check.sh with 0, which is the point: none of them are syntax
//! errors and nothing else in the tree would stop them reaching players.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! The plan registry is wired on the game-mode prefab, its .conf loads, both shipped plans are in it,
//! and every plan passes the validator.
//!
//! 🔴 THIS IS THE ONE CASE THAT CAN SEE A BROKEN .conf. Everything the objective framework does is
//! driven by authored data, and authored data has no compiler. If m_Registry is null the campaign
//! still plays - the strangler fallback runs the hard-coded phase machine - so the ONLY symptom of a
//! registry that failed to load is an ERROR line in a log nobody is reading, and the whole feature
//! silently does nothing. That is precisely the failure mode this case exists to convert into a red
//! test.
//!
//! ⚠ THE PLAN AND PHASE NAMES ARE ASSERTED LITERALLY, AND NOTHING IN THE DIRECTOR NAMES THEM ANY MORE.
//! Asserting a .conf against a production constant would pass even if BOTH sides were renamed together,
//! which is exactly the change that breaks every save on disk; a literal is the only assertion that can
//! catch a rename. The plan names were read off the director's own constants until plan-driven selection
//! deleted them (build phase 3), and the three phase names until the phase enum they mapped went in
//! build phase 7. Literals here are what they should always have been.
//!
//! CAN-FAIL: the m_Registry line was removed from Prefabs/GameMode/OVT_OverthrowGameMode.et. The tree
//! recompiled CLEAN (exit 0) - a prefab attribute nobody authors is not a script error - and the case
//! then reports "the objective director has NO plan registry: Configs/Objective/overthrowObjectives.conf
//! is not wired on the game-mode prefab, so every objective runs the hard-coded fallback". Line
//! restored, tree recompiled clean.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFramework_ARegistryResolvesAndValidates : SCR_AutotestCaseBase
{
	//! The two shipped plans' m_sObjectiveName values, AS LITERALS.
	//!
	//! ⚠ THE SAME RULE THE PHASE NAMES ARE ASSERTED UNDER, now that it is available for the plan names
	//! too. Until plan-driven selection landed the director carried the two plan names as constants and
	//! this case read them, which would have passed even if BOTH sides were renamed together - the
	//! exact change that abandons every objective in every save on disk. Phase 3 deleted those
	//! constants along with the kind-to-plan lookup that needed them, so the literal is now the only
	//! form available, which is also the only form that can catch a rename.
	static const string TOWN_PLAN = "Town Offensive";

	//! As above, for the base doctrine.
	static const string BASE_PLAN = "Base Offensive";

	//------------------------------------------------------------------------------------------------
	//! 🔴 DOES THIS DOCTRINE CHASE RADIO TOWERS? (author, 2026-08-21.)
	//!
	//! *"This is a base, radio towers don't matter to a base and there are non-objective deployments
	//! built to handle radio towers that don't matter to the current objective."*
	//!
	//! A tower recapture was authored into BOTH phases of BOTH plans. For a town it is doctrine - a
	//! tower in resistance hands feeds the unrest that the harassment ramp is trying to reverse - but a
	//! base objective is about the base, and the standing non-objective tower deployments already answer
	//! for towers whether or not an objective is running. Carrying it in the base plan spent a whole
	//! cadence on something irrelevant to the target.
	//!
	//! ⚠ THE ASSERTIONS BELOW DID NOT SIMPLY GET LOOSER. The town claims are unchanged, term for term,
	//! and the base plan gains the OPPOSITE claim - that it authors no tower recapture at all - so the
	//! removal is pinned rather than merely permitted. See AssertNoTowerRecapture().
	//! \param[in] planName The plan being walked.
	//! \return True when this doctrine repeats tower recapture in its operation chains.
	protected bool DoctrineChasesTowers(string planName)
	{
		return planName != BASE_PLAN;
	}

	//------------------------------------------------------------------------------------------------
	//! The base doctrine's positive claim: no phase of it may send a tower recapture.
	//! \param[in] planName The plan being walked, for the failure message.
	//! \param[in] phaseName Which phase is being checked, for the failure message.
	//! \param[in] operations That phase's operations.
	//! \return An empty string when no tower recapture is authored, otherwise the failure.
	protected string AssertNoTowerRecapture(string planName, string phaseName, notnull array<OVT_BaseObjectiveOperationModule> operations)
	{
		foreach (OVT_BaseObjectiveOperationModule operation : operations)
		{
			OVT_SendDeploymentObjectiveOperation send = OVT_SendDeploymentObjectiveOperation.Cast(operation);
			if (!send)
				continue;

			if (send.m_sConfigName == OVT_ObjectiveDirectorComponent.TOWER_RECAPTURE_CONFIG || OVT_EnemyTowersAffectingTargetResolver.Cast(send.m_Resolver))
				return "Plan '" + planName + "' phase '" + phaseName + "' sends a TOWER RECAPTURE. Radio towers are nothing to do with a base objective and the standing non-objective tower deployments already handle them; carrying one here spends a whole cadence on the wrong target (author, 2026-08-21)";
		}

		return "";
	}

	//! How many phases each shipped plan authors.
	static const int EXPECTED_PHASES = 3;

	//! The cadence sentinel both ramp phases author: "use the campaign's difficulty interval".
	static const int USE_DIFFICULTY_CADENCE = -1;

	//! 🔴 THE BATTLE PHASE AUTHORS A CADENCE OF ZERO - "act every in-game minute" - AND IT IS NOT A
	//! TYPO. Its one operation spends nothing: it starts the battle and then POLLS for the end of it,
	//! and the poll is the runner reaching the module again once the campaign's battle slot is empty. A
	//! battle phase left on the difficulty interval would be asked again up to a whole cadence after its
	//! battle ended - sixty in-game minutes on Normal - and the finished objective would hold the
	//! machine's one objective slot and the deployment bias for all of it.
	static const int POLLING_CADENCE = 0;

	//! Tolerance for comparing two authored reaches, in metres.
	//!
	//! ⚠ THE REACHES ARE ASSERTED AS A SHAPE, NOT AS 600 AND 1200. A case that hard-coded the numbers
	//! would fail the day somebody tuned them, which is not a defect. What IS a defect, and what
	//! AssertAnchorReachWidens() below states, is a phase that authors no reach at all - build phase 6
	//! moved the per-phase radii out of the director and into the plans, so a sentinel left behind
	//! silently takes one flat default wherever the ramp happens to be - or committed phases that reach
	//! no further than the phase which may still re-select.
	static const float RADIUS_EPSILON = 0.01;

	//! ⚠ THERE IS NO MODULE-COUNT CONSTANT ANY MORE AND THERE MUST NEVER BE ONE AGAIN. A count in this
	//! case went stale THREE separate times - once when Harassment became real doctrine, once when
	//! ForwardBase did, and once when the counter-attack gate was decomposed onto ForwardBase - and each
	//! time the red said nothing about the product, because a number can neither tell two phases apart
	//! nor say which rule went missing. Every phase is asserted by TYPE and POSITION below.

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		if (!director)
		{
			SetFailure("OVT_Global.GetObjectiveDirector() is null - the component is not declared on the game mode prefab");
			return true;
		}

		OVT_ObjectiveRegistry registry = director.GetRegistry();
		if (!registry)
		{
			SetFailure("The objective director has NO plan registry: Configs/Objective/overthrowObjectives.conf is not wired on the game-mode prefab, so every objective runs the hard-coded fallback");
			return true;
		}

		if (registry.GetConfigCount() != 2)
		{
			SetFailure("The objective registry carries %1 plan(s), expected the two shipped ones - a plan entry did not resolve", registry.GetConfigCount().ToString());
			return true;
		}

		string failure = AssertPlan(registry, TOWN_PLAN);
		if (failure == "")
			failure = AssertPlan(registry, BASE_PLAN);

		// 🔴 HALF TWO OF THE DEADLOCK FIX, AND IT IS IN A DIFFERENT FILE FROM HALF ONE. Asserted once
		// rather than per plan, because it is a fact about the DEPLOYMENT configs and a per-plan failure
		// message would name the wrong thing. See AssertForwardBasePhase() for half one.
		if (failure == "")
			failure = AssertRampSpansIntoTheForwardBase();

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		// ⚠ THE VALIDATOR IS CALLED DIRECTLY, because an initialisation-tier world never runs
		// PostGameStart() and therefore never reaches its real call site. What the tier can assert is
		// that the shipped registry PASSES; that the call site exists at all is a code-review grep.
		if (!director.ValidateObjectiveRegistry())
		{
			SetFailure("The shipped objective registry FAILED validation - see the ERROR line above naming the plan and the fault");
			return true;
		}

		if (registry.GetSkippedCount() != 0)
		{
			SetFailure("The validator skipped %1 shipped plan(s); every shipped plan must pass", registry.GetSkippedCount().ToString());
			return true;
		}

		Print("Objective framework: the plan registry is on the prefab, both shipped plans load with their selector and their three named phases and their three authored anchor radii, the harassment phase carries its three-operation chain and the forward-base phase its five-operation chain in the shipped order with both halves of the ramp-continuation fix in place, the counter-attack phase carries the terminal battle operation and the abort that is its only other way out, and every plan passes validation");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one shipped plan's whole shape.
	//! \param[in] registry The loaded registry.
	//! \param[in] planName The plan's persistence key.
	//! \return An empty string when the plan is as authored, otherwise the failure.
	protected string AssertPlan(notnull OVT_ObjectiveRegistry registry, string planName)
	{
		OVT_ObjectiveConfig plan = registry.FindConfigByName(planName);
		if (!plan)
			return "The objective registry has no plan named '" + planName + "' - the director resolves its plans by that exact string, so nothing would ever be committed to";

		if (plan.GetPhaseCount() != EXPECTED_PHASES)
			return "Plan '" + planName + "' authors " + plan.GetPhaseCount().ToString() + " phase(s), expected " + EXPECTED_PHASES.ToString();

		// ⚠ THE SELECTOR IS A .conf LINE, WHICH IS EXACTLY THE KIND OF FAULT NOTHING ELSE CAN SEE. A
		// plan with no m_Selector loads perfectly, validates as skipped, and the doctrine then never
		// runs - so the occupying faction quietly stops attacking half the map.
		if (!plan.m_Selector)
			return "Plan '" + planName + "' authors NO m_Selector, so it can never be ranked and will never be committed to";

		int expectedSource = OVT_EObjectiveCandidateSource.RESISTANCE_TOWNS;
		if (planName == BASE_PLAN)
			expectedSource = OVT_EObjectiveCandidateSource.RESISTANCE_BASES;

		if (plan.GetCandidateSources() != expectedSource)
			return "Plan '" + planName + "' declares candidate sources " + plan.GetCandidateSources().ToString() + ", expected " + expectedSource.ToString() + " - a doctrine that claims the wrong source competes for targets it has no phases for";

		// 🔴 ALL THREE PHASES ARE REAL DOCTRINE NOW, and this is where that finished. The strangler moved
		// one phase at a time - build phase 4 made Harassment real, build phase 5 made ForwardBase real,
		// build phase 6 made CounterAttack real - and each of them moved ONE line here from the shim
		// assert to a real one. The last of them deleted the shim assert outright, along with both shim
		// classes, so there is nothing left for a phase to BE except authored doctrine.
		//
		// 🔴 MOVING THAT LINE WAS PART OF THE BUILD PHASE, NOT A TIDY-UP AFTERWARDS. It was missed once
		// (build phase 5's first gate run) and the symptom was this case reporting the REAL module set
		// as a broken shim pair - a red that says nothing about the product.
		string faultA = AssertPhaseHeader(plan, planName, 0, "Harassment", USE_DIFFICULTY_CADENCE);
		if (faultA == "")
			faultA = AssertHarassmentPhase(plan, planName);
		if (faultA != "")
			return faultA;

		string faultB = AssertPhaseHeader(plan, planName, 1, "ForwardBase", USE_DIFFICULTY_CADENCE);
		if (faultB == "")
			faultB = AssertForwardBasePhase(plan, planName);
		if (faultB != "")
			return faultB;

		string faultC = AssertPhaseHeader(plan, planName, 2, "CounterAttack", POLLING_CADENCE);
		if (faultC == "")
			faultC = AssertCounterAttackPhase(plan, planName);
		if (faultC != "")
			return faultC;

		return AssertAnchorReachWidens(plan, planName);
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 THE DEPLOYMENT BIAS WIDENS WHEN THE DOCTRINE COMMITS, AND THE PLAN IS WHERE THAT NOW LIVES.
	//!
	//! Build phase 6 deleted the director's hard-coded per-phase radius lookup - it was keyed by the
	//! legacy phase ENUM, which is the coupling that phase existed to remove - and both plans author
	//! their three reaches instead. The two numbers carried a reason each, and the reasons are the
	//! assertions:
	//!   TIGHT WHILE STILL CHOOSING. In the first phase the only thing worth leaning on is the objective
	//!     itself; a wide reach would pull routine garrisoning off the whole surrounding map for a target
	//!     the machine has not committed to yet, and that is the one phase which may still re-select.
	//!   WIDER ONCE COMMITTED. From the forward-base phase the objective is LOCKED, and the forward base
	//!     stands somewhere BETWEEN the nearest held base and the objective - so the band that needs
	//!     garrisoning is the whole approach, not just the target. The battle shares that figure,
	//!     because the ground being fought over does not shrink when the fighting starts.
	//!
	//! ⚠ IT ASSERTS THE SHAPE AND NOT THE NUMBERS, so a tuner may move both without reddening a case -
	//! see RADIUS_EPSILON. Whether the pushed reach really IS the authored one is
	//! OVT_TEST_Init_ObjectiveAnchor_DirectorPushesPerPhaseAndDropsOnEveryExit's, which drives the push.
	//! \param[in] plan The plan.
	//! \param[in] planName The plan's name, for the failure message.
	//! \return An empty string when the three reaches are shaped as the doctrine needs them.
	protected string AssertAnchorReachWidens(notnull OVT_ObjectiveConfig plan, string planName)
	{
		float ramp = plan.GetPhase(0).m_fAnchorRadius;
		float forward = plan.GetPhase(1).m_fAnchorRadius;
		float battle = plan.GetPhase(2).m_fAnchorRadius;

		if (forward <= ramp)
			return "Plan '" + planName + "' reaches no further once it is COMMITTED than while it is still choosing: harassment " + ramp.ToString() + " m, forward base " + forward.ToString() + " m. From the forward-base phase the objective is locked and the base stands between the nearest held base and the target, so the whole approach needs garrisoning rather than just the target";

		if (Math.AbsFloat(battle - forward) > RADIUS_EPSILON)
			return "Plan '" + planName + "' authors a different reach for its battle (" + battle.ToString() + " m) than for its forward base (" + forward.ToString() + " m). The ground being fought over does not shrink when the fighting starts, and the two phases shared one figure in every build before the reach became authored data";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one phase's name, its position and its two tuning fields - everything that is true of
	//! EVERY shipped phase whatever doctrine it carries.
	//! \param[in] plan The plan.
	//! \param[in] planName The plan's name, for the failure message.
	//! \param[in] index Where the phase must sit.
	//! \param[in] expectedName The authored name, written as a LITERAL - see the case header.
	//! \param[in] expectedCadence The cadence this phase must author: the difficulty sentinel for a
	//!            spending phase, zero for the polling battle phase.
	//! \return An empty string when the phase is as authored, otherwise the failure.
	protected string AssertPhaseHeader(notnull OVT_ObjectiveConfig plan, string planName, int index, string expectedName, int expectedCadence)
	{
		OVT_ObjectivePhase phase = plan.GetPhase(index);
		if (!phase)
			return "Plan '" + planName + "' has no phase at index " + index.ToString();

		if (phase.m_sPhaseName != expectedName)
			return "Plan '" + planName + "' phase " + index.ToString() + " is named '" + phase.m_sPhaseName + "', expected '" + expectedName + "'. The name is the PERSISTENCE KEY: renaming it abandons every save carrying it - and from build phase 4 the deployment-side phase condition names it too, so a rename costs TWO edits";

		if (plan.IndexOfPhase(expectedName) != index)
			return "Plan '" + planName + "' resolves phase '" + expectedName + "' to index " + plan.IndexOfPhase(expectedName).ToString() + ", expected " + index.ToString();

		// ⚠ THE TWO RAMP PHASES AUTHOR THE DIFFICULTY SENTINEL AND THE BATTLE PHASE AUTHORS ZERO, and
		// the difference is a contract rather than a tuning preference - see POLLING_CADENCE. An
		// authored number on a ramp phase would silently change the ramp's pacing away from the
		// campaign's difficulty preset; the sentinel on the battle phase would leave a finished
		// objective standing for a whole cadence after its battle ended.
		if (phase.m_iOperationCadence != expectedCadence)
			return "Plan '" + planName + "' phase '" + expectedName + "' authors a cadence of " + phase.m_iOperationCadence.ToString() + ", expected " + expectedCadence.ToString() + ". -1 is the campaign's difficulty interval and is what a SPENDING phase wants; 0 is 'every in-game minute' and is what the battle phase's POLL wants, because the end of a battle is only ever noticed by the runner reaching that module again";

		// 🔴 THE REACH IS AUTHORED DATA SINCE BUILD PHASE 6, so the sentinel is a fault here rather than
		// the shipped state. The director's per-phase lookup was deleted with the enum mapping it needed,
		// and -1 now means one flat default - so a phase that leaves it behind quietly loses the widening
		// that every committed phase is supposed to get.
		if (phase.m_fAnchorRadius <= 0)
			return "Plan '" + planName + "' phase '" + expectedName + "' authors an anchor radius of " + phase.m_fAnchorRadius.ToString() + " m. The per-phase reach is authored data since build phase 6 and -1 is now ONE FLAT DEFAULT for every phase, so a sentinel left behind costs a committed phase the wider band its approach and its forward base need garrisoned - silently, because a narrower bias is not an error anywhere";

		if (!phase.m_aModules || phase.m_aModules.IsEmpty())
			return "Plan '" + planName + "' phase '" + expectedName + "' carries NO modules at all, so an objective that reaches it can neither act, advance nor end";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 THE HARASSMENT PHASE'S AUTHORED CHAIN, IN ORDER. Build phase 4 replaced this phase's shim
	//! pair with six real modules, and the ORDER of the three operations among them is the contract:
	//!
	//!     tower recapture  ->  the harassment ladder  ->  sabotage
	//!
	//! `.conf` module order IS evaluation order and the FIRST operation that acts consumes the cadence,
	//! so reordering the file changes which operation gets the interval on a tick when more than one
	//! could act. A `.conf` cannot carry a comment saying so, which is why the contract is enforced here
	//! rather than written down beside the data.
	//!
	//! ⚠ IT ASSERTS THE SHAPE, NOT THE TUNING. Which config each operation names, which resolver it
	//! sends through, its caps and its radii are
	//! OVT_TEST_Init_ObjectiveModules_HarassmentPhaseAuthorsTheShippedChain's, which also compares the
	//! authored ladder against the director's own constant rung for rung. What is here is the part that
	//! belongs to the PLAN FRAMEWORK: the phase is real doctrine, it carries the three-operation chain in
	//! the shipped order, it can advance, and it can give up.
	//!
	//! ⚠ AND IT REFUSES A SHIM. A harassment phase that still carried one would mean the .conf had been
	//! reverted to its build-phase-2 authoring while the director methods that shim calls no longer
	//! exist - the switch has had no harassment case since build phase 4, so the phase would do nothing
	//! at all, forever, behind one ERROR line.
	//! \param[in] plan The plan.
	//! \param[in] planName The plan's name, for the failure message.
	//! \return An empty string when the phase is as authored, otherwise the failure.
	protected string AssertHarassmentPhase(notnull OVT_ObjectiveConfig plan, string planName)
	{
		OVT_ObjectivePhase phase = plan.GetPhase(0);

		// ⚠ THE CHAIN IS DOCTRINE-SPECIFIC SINCE 2026-08-21 - see DoctrineChasesTowers(). A town runs
		// tower recapture, the ladder, then sabotage; a base runs the ladder then sabotage, and is
		// separately asserted to carry no tower recapture at all.
		int expectedOperations = 2;
		int ladderPosition = 1;
		if (DoctrineChasesTowers(planName))
		{
			expectedOperations = 3;
			ladderPosition = 2;
		}

		array<OVT_BaseObjectiveOperationModule> harassmentOperations = new array<OVT_BaseObjectiveOperationModule>();

		int operationsSeen = 0;
		bool hasCondition = false;
		bool hasAbort = false;

		foreach (OVT_BaseObjectiveModule module : phase.m_aModules)
		{
			if (OVT_BaseObjectiveConditionModule.Cast(module))
				hasCondition = true;

			if (OVT_BaseObjectiveAbortModule.Cast(module))
				hasAbort = true;

			if (!OVT_BaseObjectiveOperationModule.Cast(module))
				continue;

			operationsSeen++;
			harassmentOperations.Insert(OVT_BaseObjectiveOperationModule.Cast(module));

			OVT_SendDeploymentObjectiveOperation send = OVT_SendDeploymentObjectiveOperation.Cast(module);
			if (!send)
				return "Plan '" + planName + "' phase 'Harassment' carries an operation that is not a send-deployment operation; the shipped chain is " + expectedOperations.ToString() + " of them";

			// 🔴 THE ORDER IS THE CONTRACT. Position 1 is the ladder (and only the ladder authors one);
			// positions 0 and 2 are the two single-config sends, tower recapture then sabotage.
			bool isLadder = send.m_aLadder && !send.m_aLadder.IsEmpty();

			if (operationsSeen == ladderPosition)
			{
				if (!isLadder)
					return "Plan '" + planName + "' phase 'Harassment' does not author the harassment LADDER at operation " + ladderPosition.ToString() + ". The authored order still decides which operations are PINNED ahead of the director's per-cadence shuffle and how those are ordered, so the chain is still a contract";
			}
			else
			{
				if (isLadder)
					return "Plan '" + planName + "' phase 'Harassment' authors the harassment ladder at operation " + operationsSeen.ToString() + ", not " + ladderPosition.ToString();
			}
		}

		if (operationsSeen != expectedOperations)
			return "Plan '" + planName + "' phase 'Harassment' authors " + operationsSeen.ToString() + " operation(s); this doctrine's shipped chain is exactly " + expectedOperations.ToString() + " - see DoctrineChasesTowers() for why a base runs one fewer than a town";

		// 🔴 THE BASE DOCTRINE'S OWN CLAIM, not merely the absence of the town's. See AssertNoTowerRecapture().
		if (!DoctrineChasesTowers(planName))
		{
			string noTower = AssertNoTowerRecapture(planName, "Harassment", harassmentOperations);
			if (noTower != "")
				return noTower;
		}

		if (!hasCondition)
			return "Plan '" + planName + "' phase 'Harassment' authors no condition module, so it can never advance to the forward base";

		if (!hasAbort)
			return "Plan '" + planName + "' phase 'Harassment' authors no abort module, so an objective that stalls in it is never given up and the doctrine sticks on one place for the rest of the campaign";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 THE FORWARD-BASE PHASE'S AUTHORED CHAIN, IN ORDER. Build phase 5 replaced this phase's shim
	//! pair with the whole forward base as authored data:
	//!
	//!     raise the base  ->  garrison it  ->  tower recapture  ->  the harassment ladder  ->  sabotage
	//!
	//! `.conf` module order IS evaluation order and the FIRST operation that acts consumes the cadence,
	//! so reordering the file changes which operation gets the interval on a tick when more than one
	//! could act. THE RAISE IS FIRST because nothing else in this phase means anything until the flag is
	//! up - the garrison's own source provider resolves to the forward base only once it is standing. A
	//! `.conf` cannot carry a comment saying so, which is why the contract is enforced here.
	//!
	//! 🔴 AND OPERATIONS 3 TO 5 ARE THE RAMP, REPEATED, WHICH IS HALF ONE OF THE 2026-08-19 DEADLOCK
	//! FIX. A base objective is promoted to this phase on its FIRST completed sabotage mission and its
	//! counter-attack gate demands up to SIX, so a promotion that stopped the ramp made the remaining
	//! five unsendable and the battle unreachable. Towns deadlock identically: the stacking support
	//! debuff that drives support under 25 % is applied BY harassment operations. Half two is the
	//! deployment-side phase span - see AssertRampSpansIntoTheForwardBase(). EITHER HALF ALONE LEAVES
	//! THE RAMP DEAD, which is why both are asserted in this one case.
	//!
	//! ⚠ IT ASSERTS THE SHAPE, NOT THE TUNING. Which config each operation names, which resolver it
	//! sends through, the garrison's cap and its difficulty field, and the ladder rung for rung against
	//! the director's own constant are
	//! OVT_TEST_Init_ObjectiveFOB_QForwardBasePhaseAuthorsTheShippedChain's. What is here is the part
	//! that belongs to the PLAN FRAMEWORK: the phase is real doctrine, it carries the five-operation
	//! chain in the shipped order, its gate is the counter-attack gate decomposed into conjuncts, and it
	//! can be given up in both of the two ways this phase can fail.
	//!
	//! 🔴 THE COUNT IS NOT THE CLAIM AND MUST NEVER BE. A module count is exactly what made this case
	//! stale three times over - Harassment becoming doctrine, ForwardBase becoming doctrine, and this
	//! phase growing the decomposed counter-attack gate - because a number can neither tell two phases
	//! apart nor say which rule went missing. Every assertion below names a TYPE and a POSITION.
	//! \param[in] plan The plan.
	//! \param[in] planName The plan's name, for the failure message.
	//! \return An empty string when the phase is as authored, otherwise the failure.
	protected string AssertForwardBasePhase(notnull OVT_ObjectiveConfig plan, string planName)
	{
		OVT_ObjectivePhase phase = plan.GetPhase(1);

		array<OVT_BaseObjectiveOperationModule> operations = new array<OVT_BaseObjectiveOperationModule>();

		bool hasAssetUp = false;
		bool hasReserveGate = false;
		bool hasDaylight = false;
		bool hasRampMeasure = false;
		int idleAbortAt = -1;
		int starvationAbortAt = -1;
		int abortsSeen = 0;

		foreach (OVT_BaseObjectiveModule module : phase.m_aModules)
		{
			OVT_BaseObjectiveOperationModule operation = OVT_BaseObjectiveOperationModule.Cast(module);
			if (operation)
			{
				operations.Insert(operation);
				continue;
			}

			if (OVT_AssetUpObjectiveCondition.Cast(module))
				hasAssetUp = true;

			if (OVT_ReserveAtLeastObjectiveCondition.Cast(module))
				hasReserveGate = true;

			if (OVT_DaylightWindowObjectiveCondition.Cast(module))
				hasDaylight = true;

			// EACH DOCTRINE MEASURES ITS OWN RAMP: a town by its collapsed support, a base by its
			// completed demolition missions. Both are asserted by name below.
			if (OVT_SupportBelowObjectiveCondition.Cast(module) || OVT_ProgressAtLeastObjectiveCondition.Cast(module))
				hasRampMeasure = true;

			if (!OVT_BaseObjectiveAbortModule.Cast(module))
				continue;

			if (OVT_AssetStarvedObjectiveAbort.Cast(module) && starvationAbortAt < 0)
				starvationAbortAt = abortsSeen;

			if (OVT_IdleForObjectiveAbort.Cast(module) && idleAbortAt < 0)
				idleAbortAt = abortsSeen;

			abortsSeen++;
		}

		string chain = AssertForwardBaseChain(planName, operations);
		if (chain != "")
			return chain;

		// --- THE GATE OUT: the hard-coded counter-attack gate, decomposed into authored conjuncts.
		if (!hasAssetUp)
			return "Plan '" + planName + "' phase 'ForwardBase' does not require its forward base to be STANDING before it advances, so the battle would be mounted out of thin air - which is the dice-roll attack this whole feature replaced";

		if (!hasRampMeasure)
			return "Plan '" + planName + "' phase 'ForwardBase' does not measure its own ramp before advancing, so the battle would fire on the phase's ENTRY TICK with none of the build-up the resistance is meant to be able to read";

		if (!hasReserveGate)
			return "Plan '" + planName + "' phase 'ForwardBase' does not gate its battle on the faction's reserve, so a counter-attack would start with nothing behind it and no waves to follow";

		if (!hasDaylight)
			return "Plan '" + planName + "' phase 'ForwardBase' does not carry the daylight window, so counter-attacks would begin at night again - and the wait for morning would no longer hold the objective's idle clock either";

		// --- AND THE TWO WAYS OUT WHEN IT DOES NOT WORK.
		if (starvationAbortAt < 0)
			return "Plan '" + planName + "' phase 'ForwardBase' authors no starvation abort. The resistance's entire counterplay is gone: a forward base could be cut off indefinitely and still launch its battle";

		if (idleAbortAt < 0)
			return "Plan '" + planName + "' phase 'ForwardBase' authors no OVT_IdleForObjectiveAbort. Since the doctrine became authored data a phase with none CANNOT TIME OUT AT ALL - the idle clock runs to zero and nothing answers, so a wedged objective sits on one place forever. The registry's validator deliberately does not catch this, because it cannot tell a terminal phase from a forgotten one";

		// 🔴 THE ABORT ORDER IS A CONTRACT TOO. The abort fold takes its REASON and its blacklist flag
		// from the FIRST module that fires, and a forward base that has been cut off for its whole
		// budget has almost certainly also stopped making progress - so both can fire on the same tick.
		// Starvation first is what puts "its forward base was cut off" in the campaign log instead of
		// "the forward-base phase did nothing at all", which is the difference between a player being
		// told their counterplay worked and being told nothing happened.
		if (starvationAbortAt > idleAbortAt)
			return "Plan '" + planName + "' phase 'ForwardBase' authors the idle abort BEFORE the starvation abort. Both can fire on the same tick and the first one wins the log line, so the resistance would be told the occupying faction lost interest rather than that its own counterplay cut the base off";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The forward-base phase's five operations, by type and by POSITION.
	//! \param[in] planName The plan's name, for the failure message.
	//! \param[in] operations The phase's operations, in authored order.
	//! \return An empty string when the chain is as authored, otherwise the failure.
	protected string AssertForwardBaseChain(string planName, notnull array<OVT_BaseObjectiveOperationModule> operations)
	{
		// ⚠ DOCTRINE-SPECIFIC SINCE 2026-08-21 - see DoctrineChasesTowers(). A town repeats the whole
		// three-operation ramp (tower, ladder, sabotage); a base repeats the two that concern a base.
		// THE DEADLOCK CLAIM IS UNCHANGED AND IS THE REASON THE RAMP MUST BE REPEATED AT ALL: a base
		// objective is promoted on its FIRST completed mission and its counter-attack gate demands up to
		// six, so a promotion that stopped the ramp would make the rest unsendable.
		int expected = 4;
		int ladderAt = 2;
		int sabotageAt = 3;
		if (DoctrineChasesTowers(planName))
		{
			expected = 5;
			ladderAt = 3;
			sabotageAt = 4;
		}

		if (operations.Count() != expected)
			return "Plan '" + planName + "' phase 'ForwardBase' authors " + operations.Count().ToString() + " operation(s); this doctrine's shipped chain is exactly " + expected.ToString() + " - raise the base, garrison it, then its own harassment ramp repeated. FEWER IS THE 2026-08-19 DEADLOCK: the ramp stops the moment the objective is promoted, and the counter-attack it is ramping towards becomes unreachable";

		if (!OVT_RaiseForwardBaseObjectiveOperation.Cast(operations[0]))
			return "Plan '" + planName + "' phase 'ForwardBase' does not RAISE THE FORWARD BASE first. Every spend is behind one cadence, so whichever operation is asked first is the one that gets the interval - and everything else in this phase is for a base that is not there yet";

		OVT_SendDeploymentObjectiveOperation garrison = OVT_SendDeploymentObjectiveOperation.Cast(operations[1]);
		if (!garrison)
			return "Plan '" + planName + "' phase 'ForwardBase' does not send the forward base's GARRISON second";

		if (!OVT_ForwardBaseTargetResolver.Cast(garrison.m_Resolver))
			return "Plan '" + planName + "' phase 'ForwardBase' does not send its garrison through the forward-base resolver, so it would be sent at the OBJECTIVE - the place the resistance holds - rather than at the base it is supposed to be holding";

		// --- THE RAMP, REPEATED, FROM POSITION 3 ON. Half one of the deadlock fix.
		for (int i = 2; i < expected; i++)
		{
			if (!OVT_SendDeploymentObjectiveOperation.Cast(operations[i]))
				return "Plan '" + planName + "' phase 'ForwardBase' operation " + (i + 1).ToString() + " is not a send-deployment operation; every position after the garrison is this doctrine's harassment ramp repeated";
		}

		OVT_SendDeploymentObjectiveOperation ladder = OVT_SendDeploymentObjectiveOperation.Cast(operations[ladderAt]);
		OVT_SendDeploymentObjectiveOperation sabotage = OVT_SendDeploymentObjectiveOperation.Cast(operations[sabotageAt]);

		if (DoctrineChasesTowers(planName))
		{
			OVT_SendDeploymentObjectiveOperation tower = OVT_SendDeploymentObjectiveOperation.Cast(operations[2]);

			if (!OVT_EnemyTowersAffectingTargetResolver.Cast(tower.m_Resolver))
				return "Plan '" + planName + "' phase 'ForwardBase' does not repeat TOWER RECAPTURE as its third operation. A tower left in resistance hands keeps the objective easier for them to hold right through the build-up, and the ramp is supposed to carry on into this phase";
		}
		else
		{
			// The base doctrine's own claim, not merely the absence of the town's.
			string noTower = AssertNoTowerRecapture(planName, "ForwardBase", operations);
			if (noTower != "")
				return noTower;
		}

		if (!ladder.m_aLadder || ladder.m_aLadder.IsEmpty())
			return "Plan '" + planName + "' phase 'ForwardBase' does not repeat the HARASSMENT LADDER at operation " + (ladderAt + 1).ToString() + ". The stacking support debuff that drives a town under a quarter support is applied BY harassment operations, so a town objective could never reach its own counter-attack gate";

		if (sabotage.m_aLadder && !sabotage.m_aLadder.IsEmpty())
			return "Plan '" + planName + "' phase 'ForwardBase' authors the harassment ladder AFTER sabotage. The repeated chain ends ladder then sabotage - the same order the harassment phase uses, for the same reason";

		if (!OVT_ObjectiveSelfTargetResolver.Cast(sabotage.m_Resolver))
			return "Plan '" + planName + "' phase 'ForwardBase' does not repeat SABOTAGE as its last operation. A base objective is promoted on its FIRST completed mission and its counter-attack gate demands up to six, so the remaining five would be unsendable and the battle unreachable - the 2026-08-19 deadlock exactly";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 HALF TWO OF THE DEADLOCK FIX, IN A DIFFERENT FILE FROM HALF ONE.
	//!
	//! Authoring the ramp's operations in the forward-base phase is not enough on its own: every ramp
	//! deployment carries its own OVT_ObjectiveConditionDeploymentModule, and one whose span does not
	//! REACH the forward-base phase stops belonging the moment the objective is promoted - so its
	//! reinforcement module collects it on the next check. The ramp would then send teams that are
	//! deleted a few seconds later, which is worse than not sending them: it spends the pool and
	//! returns nothing.
	//!
	//! ⚠ THE SPAN IS TWO NAMES AND NO COMPILER READS EITHER. m_sFromPhase "Harassment" through
	//! m_sThroughPhase "ForwardBase", matched by string against the running plan's phase names. A phase
	//! renamed in Configs/Objective and not here does not fail to parse and does not warn beyond one
	//! ERROR line per module.
	//! \return An empty string when every ramp config spans into the forward-base phase, or the first
	//!         that does not.
	protected string AssertRampSpansIntoTheForwardBase()
	{
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (!deployments)
			return "The deployment framework did not resolve, so the deployment-side half of the ramp-continuation fix could not be checked at all - and a half-checked deadlock fix is not a checked one";

		array<string> configs = new array<string>();
		configs.Insert(OVT_ObjectiveDirectorComponent.TOWER_RECAPTURE_CONFIG);
		configs.Insert(OVT_ObjectiveDirectorComponent.SABOTAGE_CONFIG);

		foreach (string rung : OVT_ObjectiveDirectorComponent.HARASSMENT_LADDER)
		{
			configs.Insert(rung);
		}

		foreach (string name : configs)
		{
			OVT_DeploymentConfig config = deployments.FindConfigByName(name);
			if (!config || !config.m_aModules)
				return "The deployment registry does not carry '" + name + "', which both shipped plans send in two phases";

			bool spansFromHarassment = false;
			bool spansThroughForwardBase = false;

			foreach (OVT_BaseDeploymentModule module : config.m_aModules)
			{
				OVT_ObjectiveConditionDeploymentModule condition = OVT_ObjectiveConditionDeploymentModule.Cast(module);
				if (!condition)
					continue;

				if (condition.m_sFromPhase == "Harassment")
					spansFromHarassment = true;

				if (condition.m_sThroughPhase == "ForwardBase")
					spansThroughForwardBase = true;
			}

			if (!spansFromHarassment)
				return "Deployment config '" + name + "' does not begin its phase span at 'Harassment', so the ramp phase that sends it would collect it on its own first reinforcement check";

			if (!spansThroughForwardBase)
				return "Deployment config '" + name + "' does not span THROUGH to 'ForwardBase'. An EMPTY m_sThroughPhase means 'this phase only', so every one of its deployments is collected on its next reinforcement check the moment the objective is promoted - which is the 2026-08-19 deadlock restored from the other side, with the plan's own module bag looking perfectly correct";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 THE BATTLE PHASE'S AUTHORED MODULE SET. Build phase 6 replaced this phase's shim pair with the
	//! two modules that end a plan:
	//!
	//!     start the battle, then poll for it  +  give the place up if no battle could be started
	//!
	//! ⚠ IT ASSERTS BY TYPE AND POSITION AND NEVER BY COUNT. A module count in THIS case has gone stale
	//! three separate times - Harassment becoming doctrine, ForwardBase becoming doctrine, and the
	//! counter-attack gate being decomposed onto ForwardBase - and every one of those reds said nothing
	//! about the product, because a number can neither tell two phases apart (ForwardBase and
	//! CounterAttack were both "2" while both were shims) nor say which rule went missing.
	//!
	//! 🔴 THE OPERATION MUST ANSWER IsTerminal(), AND THAT IS NOT A DETAIL. This phase authors NO advance
	//! condition, by design - there is nothing after it to advance to - so the registry's wedge rule
	//! accepts it only because a terminal operation can END the objective by acting. An operation that
	//! answered false here would make the LAST PHASE OF BOTH SHIPPED PLANS look like a wedge, and the
	//! validator would skip both plans at world start: the occupying faction would stop attacking
	//! entirely, behind two ERROR lines.
	//!
	//! 🔴 AND IT MUST CARRY NO CONDITION MODULE AT ALL, which is the one rule a modder is most likely to
	//! break. A condition on the LAST phase does not gate the battle - conditions ADVANCE, and advancing
	//! off the end of a plan ENDS the objective without blacklisting it. Authoring the daylight window
	//! here, which reads like the obvious way to say "fight in daylight", would end the objective at
	//! 05:00 with no battle at all and no failure anywhere. The daylight window and the other three
	//! conjuncts of the battle's gate belong on the phase BEFORE this one, where they admit the objective
	//! to it - see AssertForwardBasePhase(), which asserts all four and is where a missing one is
	//! reported, so that one fault produces one message.
	//!
	//! ⚠ AND THE IDLE ABORT IS THE PHASE'S OTHER HALF, not a nicety. A refused start - the base retaken
	//! by some other route, a marker that no longer resolves - is a state that lasts until the objective
	//! ends, and since the doctrine became authored data a phase with no abort module CANNOT TIME OUT AT
	//! ALL. Without it a doctrine that cannot start its battle sits on that place for the rest of the
	//! campaign. It blacklists, because that IS the failure arm of this phase; the resolution arm does
	//! not, and the two are asserted together in
	//! OVT_TEST_Init_ObjectiveDirector_TerminalPhaseEndsTheObjectiveOnOnePath.
	//! \param[in] plan The plan.
	//! \param[in] planName The plan's name, for the failure message.
	//! \return An empty string when the phase is as authored, otherwise the failure.
	protected string AssertCounterAttackPhase(notnull OVT_ObjectiveConfig plan, string planName)
	{
		OVT_ObjectivePhase phase = plan.GetPhase(2);

		array<OVT_BaseObjectiveOperationModule> operations = new array<OVT_BaseObjectiveOperationModule>();

		OVT_IdleForObjectiveAbort idleAbort;

		foreach (OVT_BaseObjectiveModule module : phase.m_aModules)
		{
			OVT_BaseObjectiveOperationModule operation = OVT_BaseObjectiveOperationModule.Cast(module);
			if (operation)
			{
				operations.Insert(operation);
				continue;
			}

			if (OVT_BaseObjectiveConditionModule.Cast(module))
				return "Plan '" + planName + "' phase 'CounterAttack' carries a CONDITION module ('" + module.m_sModuleName + "'). Conditions ADVANCE a phase, and there is no phase after this one - so advancing ENDS the objective, with no battle and no failure recorded anywhere. Everything that gates the battle belongs on the phase before this one, where it admits the objective to this one";

			if (!OVT_BaseObjectiveAbortModule.Cast(module))
				continue;

			if (!idleAbort)
				idleAbort = OVT_IdleForObjectiveAbort.Cast(module);
		}

		// --- THE TERMINAL OPERATION.
		if (operations.Count() != 1)
			return "Plan '" + planName + "' phase 'CounterAttack' authors " + operations.Count().ToString() + " operation(s); the shipped phase is exactly one - start the battle, then poll for the end of it. A second operation would be tried on the same interval as the battle poll, on an objective that is already finished";

		OVT_StartBattleObjectiveOperation battle = OVT_StartBattleObjectiveOperation.Cast(operations[0]);
		if (!battle)
			return "Plan '" + planName + "' phase 'CounterAttack' does not author OVT_StartBattleObjectiveOperation as its operation, so the whole ramp - the harassment, the forward base, the demolition quota - ends in a phase that never starts a battle and is abandoned when its clock runs out";

		if (!battle.IsTerminal())
			return "Plan '" + planName + "' phase 'CounterAttack' authors an operation that does not declare itself TERMINAL. This phase carries no advance condition, so the registry's wedge rule would skip the whole plan at world start and the doctrine would never run at all";

		if (battle.m_eMode != OVT_EQRFMode.COUNTER_ATTACK)
			return "Plan '" + planName + "' phase 'CounterAttack' authors battle mode " + battle.m_eMode.ToString() + ", expected COUNTER_ATTACK. STANDARD is the player-facing battle a captured base raises - announced at once, with a 120-second countdown - and using it here would replace the silent encirclement this whole doctrine exists to mount with a siren";

		if (battle.m_fBaseResolveRadius <= 0)
			return "Plan '" + planName + "' phase 'CounterAttack' authors a base resolve radius of " + battle.m_fBaseResolveRadius.ToString() + " m, so NO base is ever near enough its own recorded position and a base doctrine would refuse its own battle every time";

		// --- AND THE ONE WAY OUT WHEN NO BATTLE CAN BE STARTED.
		if (!idleAbort)
			return "Plan '" + planName + "' phase 'CounterAttack' authors no OVT_IdleForObjectiveAbort. Since the doctrine became authored data a phase with none CANNOT TIME OUT AT ALL - so a battle that cannot be started (the base retaken by some other route, a marker that no longer resolves) leaves the occupying faction sitting on that place for the rest of the campaign. The registry's validator deliberately does not catch this: it cannot tell a terminal phase that wants no timeout from one that forgot it";

		if (!idleAbort.m_bBlacklist)
			return "Plan '" + planName + "' phase 'CounterAttack' authors an idle abort that does NOT blacklist. This is the FAILURE arm of the battle phase - nothing could be attacked at all - and a place that just failed must sit out a selection round, or the machine picks it again immediately and fails the same way. The RESOLUTION arm is the one that must not blacklist, and it is the battle module's, not this one's";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! A broken plan is named in an ERROR line and skipped, and the rest of the registry still runs.
//!
//! ⚠ "SKIPPED" AND "NAMED" ARE TWO SEPARATE REQUIREMENTS AND BOTH FAIL SILENTLY ON THEIR OWN. A
//! validator that only skipped would give a mod author a plan that never runs and no way to find out
//! why; one that only logged would let a plan with a duplicate persistence key restore another plan's
//! objectives. This case asserts the skip; the naming is the Print(ERROR) beside it, whose text is
//! checked by the failure messages below quoting what a reader would need.
//!
//! ⚠ A DUPLICATE NAME POISONS BOTH COPIES, DELIBERATELY. The name is the persistence key, so two plans
//! sharing one means a saved objective cannot be resolved to the plan that created it. Skipping only
//! the second would leave the ambiguity in the save format; skipping both makes it visible immediately
//! and costs a mod author one rename.
//!
//! ⚠ AN UNNAMED PLAN IS REPORTED BUT CANNOT BE ON THE SKIPPED LIST, because the list is keyed by name
//! and it has none. It is unselectable anyway for exactly the same reason, which is why the case
//! asserts the count rather than looking for an empty key.
//!
//! ⚠ EVERY SUBJECT IS BUILT WITH `new`, WHICH APPLIES NO [Attribute()] DEFAULTS. Every field these
//! rules read is therefore set explicitly, including the ones whose declared default is what the case
//! wants - a hand-built plan starts with m_fPriority at 0, not at 1.
//!
//! ⚠ THE RULES GROW WITH THE FEATURE AND SO DOES THIS CASE. The structural rules landed with the
//! framework; plan-driven selection added the two that describe a doctrine which can never be
//! ranked - no selector at all, and a selector that declares no candidate source; and the harassment
//! phase's build added four more about a phase's MODULE BAG. Every one of them is a silent fault of
//! exactly the shape the validator exists for: the campaign plays on, the plan simply never runs, and
//! nothing in any log says why.
//!
//! 🔴 AND THE VALID CONTROL PLAN HAD TO BE RE-AUTHORED WHEN THE WEDGE RULE LANDED, WHICH IS WORTH
//! RECORDING RATHER THAN QUIETLY FIXING. Until then MakePhase() built a phase with an EMPTY module bag
//! and "Alpha" - the one plan in this fixture that is supposed to pass - was built from it. A phase
//! with no modules can neither advance (nothing gates it) nor end (nothing terminal acts), which is
//! precisely the wedge the new rule names, so Alpha stopped being valid and the case went red on "the
//! one wholly valid plan was skipped". THE RULE WAS RIGHT AND THE FIXTURE WAS UNDER-AUTHORED: an empty
//! phase really is a plan that can be committed to and then never do anything. MakePhase() now builds
//! the minimum legal bag - one advance condition and one idle abort - which is also the smallest thing
//! a modder could honestly ship.
//!
//! CAN-FAIL, three faults, injected into ValidateConfig() separately:
//!   V1. THE DUPLICATE-PHASE-NAME RULE was removed. The tree recompiled clean (exit 0) and the case
//!       then reports "a plan authoring the same phase name twice must be skipped: a restored
//!       objective would resolve to whichever copy came first, whichever one it was actually in".
//!   V2. THE NO-SELECTOR RULE was removed. Compiled clean (exit 0). The case then reports "a plan
//!       with NO SELECTOR must be skipped: it cannot say what it attacks, so it would score nothing
//!       on every round for the rest of the campaign and never be committed to", and the skipped
//!       count falls to 5.
//!   V3. THE NO-CANDIDATE-SOURCES RULE was removed. Compiled clean (exit 0). The case then reports
//!       "a plan whose selector declares NO CANDIDATE SOURCES must be skipped: selection collects
//!       the union of every plan's sources, so a selector that declares none is handed an empty set
//!       and scores nothing - forever, and silently".
//!   V4. THE WEDGE RULE was removed. Compiled clean (exit 0). The case then reports "a phase that can
//!       neither ADVANCE nor END must be skipped".
//!   V5. THE NO-RESOLVER RULE was removed. Compiled clean (exit 0). The case then reports "an
//!       operation with NO RESOLVER must be skipped".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFramework_BValidatorNamesAndSkipsABrokenPlan : SCR_AutotestCaseBase
{
	//! A deployment config every campaign world really carries, for the fixture plans that are supposed
	//! to pass the name-resolution rule. Read off the director's own constant so the fixture cannot
	//! drift away from the registry.
	static const string REAL_CONFIG_NAME = OVT_ObjectiveDirectorComponent.SABOTAGE_CONFIG;

	//! A name no registry carries, for the fixture plan that is supposed to FAIL that rule. Deliberately
	//! absurd rather than plausible: a plausible one could be added to the registry one day and this
	//! fixture would then quietly stop testing anything.
	static const string UNREGISTERED_CONFIG_NAME = "framework fixture - no such deployment config";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveRegistry registry = new OVT_ObjectiveRegistry();
		registry.m_sRegistryName = "framework validator fixture";
		registry.m_aObjectiveConfigs = new array<ref OVT_ObjectiveConfig>();

		registry.m_aObjectiveConfigs.Insert(MakePlan("Alpha", 0, "One", "", MakeShippedSelector()));
		registry.m_aObjectiveConfigs.Insert(MakePlan("", 0, "One", "", MakeShippedSelector()));
		registry.m_aObjectiveConfigs.Insert(MakePlan("Delta", 0, "One", "", MakeShippedSelector()));
		registry.m_aObjectiveConfigs.Insert(MakePlan("Delta", 0, "One", "", MakeShippedSelector()));
		registry.m_aObjectiveConfigs.Insert(MakePlan("Beta", 0, "", "", MakeShippedSelector()));
		registry.m_aObjectiveConfigs.Insert(MakePlan("Gamma", 0, "One", "One", MakeShippedSelector()));
		registry.m_aObjectiveConfigs.Insert(MakePlan("Epsilon", -1, "One", "", MakeShippedSelector()));

		// The two plan-driven-selection rules. Both describe a doctrine that would sit in the registry
		// forever competing for objectives it can never describe - the exact shape of fault that has no
		// symptom, because the campaign plays on and nothing says why the plan never runs.
		registry.m_aObjectiveConfigs.Insert(MakePlan("Zeta", 0, "One", "", null));
		registry.m_aObjectiveConfigs.Insert(MakePlan("Eta", 0, "One", "", new OVT_ObjectiveTargetSelector()));

		// The four MODULE-BAG rules the harassment phase's build added. Each describes a phase that
		// loads perfectly, validates as authored data, and then silently does nothing - or does
		// something the author cannot see is missing.
		registry.m_aObjectiveConfigs.Insert(MakeModulePlan("Theta", MakeWedgePhase()));
		registry.m_aObjectiveConfigs.Insert(MakeModulePlan("Iota", MakeSendPhase(null, REAL_CONFIG_NAME)));
		registry.m_aObjectiveConfigs.Insert(MakeModulePlan("Kappa", MakeSendPhase(new OVT_ObjectiveSelfTargetResolver(), "")));
		registry.m_aObjectiveConfigs.Insert(MakeModulePlan("Lambda", MakeSendPhase(new OVT_ObjectiveSelfTargetResolver(), UNREGISTERED_CONFIG_NAME)));

		// ⚠ THE NAME-RESOLUTION RULE IS SKIPPED WHEN THERE IS NO DEPLOYMENT FRAMEWORK TO ASK, and that
		// is deliberate: a validator that turned "I could not check" into "this plan is broken" would
		// skip both shipped doctrines in any world without one. So the expectation for 'Lambda' - and
		// only for Lambda - depends on whether the framework resolved here too.
		bool nameRulesRan = OVT_Global.GetDeploymentManager() != null;

		if (registry.ValidateAllConfigs())
		{
			SetFailure("A registry carrying an unnamed plan, a duplicated name, a plan with no phases, a plan with a duplicated phase name, a negative priority, a plan with no selector, a selector that claims no candidate source, a phase that can neither advance nor end, an operation with no resolver and an operation with nothing to send PASSED validation - a broken plan would reach the campaign silently");
			return true;
		}

		if (!registry.WasValidated())
		{
			SetFailure("The registry does not report having been validated, so nothing downstream can tell a checked registry from an unchecked one");
			return true;
		}

		if (registry.IsSkipped("Alpha"))
		{
			SetFailure("The one wholly valid plan was skipped: one broken plan in a mod must never stop the rest of the registry running");
			return true;
		}

		if (!registry.IsSkipped("Beta"))
		{
			SetFailure("a plan with an EMPTY phase list must be skipped: it can be committed to and can then never act, advance or end, which is the one failure mode with no symptom a player could report");
			return true;
		}

		if (!registry.IsSkipped("Gamma"))
		{
			SetFailure("a plan authoring the same phase name twice must be skipped: a restored objective would resolve to whichever copy came first, whichever one it was actually in");
			return true;
		}

		if (!registry.IsSkipped("Delta"))
		{
			SetFailure("a DUPLICATED plan name must be skipped: the name is the persistence key, so two plans sharing one cannot be told apart by a save");
			return true;
		}

		if (!registry.IsSkipped("Epsilon"))
		{
			SetFailure("a plan with a NEGATIVE priority multiplier must be skipped: higher wins, so a negative one would let a poor candidate out-rank a good one");
			return true;
		}

		if (!registry.IsSkipped("Zeta"))
		{
			SetFailure("a plan with NO SELECTOR must be skipped: it cannot say what it attacks, so it would score nothing on every round for the rest of the campaign and never be committed to");
			return true;
		}

		if (!registry.IsSkipped("Eta"))
		{
			SetFailure("a plan whose selector declares NO CANDIDATE SOURCES must be skipped: selection collects the union of every plan's sources, so a selector that declares none is handed an empty set and scores nothing - forever, and silently");
			return true;
		}

		// 🔴 THE WEDGE. A phase carrying neither an advance condition nor a terminal operation runs until
		// its idle clock abandons the objective, every single time, and the only symptom is an occupying
		// faction that never gets past that phase.
		if (!registry.IsSkipped("Theta"))
		{
			SetFailure("a phase that can neither ADVANCE nor END must be skipped: it carries no condition to satisfy and no terminal operation, so every objective that reaches it runs its idle clock down and is abandoned, for the rest of the campaign, with nothing in any log naming the phase");
			return true;
		}

		if (!registry.IsSkipped("Iota"))
		{
			SetFailure("an operation with NO RESOLVER must be skipped: it has nowhere to send anything, so it refuses on every cadence interval - silently, forever - and the phase it sits in does nothing");
			return true;
		}

		if (!registry.IsSkipped("Kappa"))
		{
			SetFailure("an operation authoring NEITHER a config name NOR a ladder must be skipped: there is no deployment for it to buy");
			return true;
		}

		if (nameRulesRan && !registry.IsSkipped("Lambda"))
		{
			SetFailure("an operation naming a deployment config the registry does not carry must be skipped: no compiler reads a .conf, so a config renamed in overthrowDeployments.conf and not in the plan produces a ramp that stops sending with one refusal line per in-game minute as its only symptom");
			return true;
		}

		// Beta, Gamma, Delta, Epsilon, Zeta, Eta, Theta, Iota, Kappa - and Lambda only when there was a
		// deployment framework to resolve its config name against. The unnamed plan is reported but
		// cannot be listed by a key it does not have - see the case header.
		int expectedSkipped = 9;
		if (nameRulesRan)
			expectedSkipped = 10;

		if (registry.GetSkippedCount() != expectedSkipped)
		{
			SetFailure("The validator skipped %1 plan(s) by name, expected %2 - an unnamed plan is reported rather than listed, and every other fault must produce exactly one entry", registry.GetSkippedCount().ToString(), expectedSkipped.ToString());
			return true;
		}

		Print("Objective framework: the validator skips an unnamed plan, a duplicated name, an empty phase list, a duplicated phase name, a negative priority, a plan with no selector, a selector that claims nothing, a phase that can neither advance nor end, an operation with no resolver and an operation with nothing to send - names each one, and leaves the valid plan running");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds one plan by hand. Every field these rules read is set explicitly - `new` applies no
	//! attribute defaults.
	//! \param[in] name The plan's persistence key. An empty string is the unnamed-plan fault.
	//! \param[in] priority The plan's multiplier. Negative is the mis-key fault.
	//! \param[in] phaseA First phase name, or an empty string for a plan with no phases at all.
	//! \param[in] phaseB Second phase name, or an empty string for a single-phase plan.
	//! \param[in] selector What the plan attacks. Null is the no-selector fault; a bare
	//!            OVT_ObjectiveTargetSelector is the declares-no-sources fault.
	//! \return The plan.
	protected OVT_ObjectiveConfig MakePlan(string name, float priority, string phaseA, string phaseB, OVT_ObjectiveTargetSelector selector)
	{
		OVT_ObjectiveConfig plan = new OVT_ObjectiveConfig();
		plan.m_sObjectiveName = name;
		plan.m_iAllowedFactionTypes = OVT_FactionTypeFlag.OCCUPYING_FACTION;
		plan.m_fPriority = priority;
		plan.m_fChance = 100;
		plan.m_iMaxInstances = 1;
		plan.m_Selector = selector;
		plan.m_aPhases = new array<ref OVT_ObjectivePhase>();

		if (phaseA != "")
			plan.m_aPhases.Insert(MakePhase(phaseA));

		if (phaseB != "")
			plan.m_aPhases.Insert(MakePhase(phaseB));

		return plan;
	}

	//------------------------------------------------------------------------------------------------
	//! A shipped town selector at its shipped weights, for the plans in this fixture that are supposed
	//! to be VALID.
	//!
	//! ⚠ `new` APPLIES NO [Attribute()] DEFAULTS, so the weights are assigned explicitly through the
	//! selector's own ApplyShippedWeights() - which reads them off the pure statics' constants, so this
	//! fixture cannot drift away from the shipped values without the constants themselves moving.
	//! \return A selector that claims resistance towns.
	protected OVT_ObjectiveTargetSelector MakeShippedSelector()
	{
		OVT_ResistanceTownObjectiveSelector selector = new OVT_ResistanceTownObjectiveSelector();
		selector.ApplyShippedWeights();

		return selector;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds one VALID phase by hand: a name, the three tuning sentinels, and the minimum legal module
	//! bag.
	//!
	//! 🔴 THE MODULE BAG IS NOT PADDING - IT IS WHAT MAKES THE PHASE LEGAL. Until the wedge rule landed
	//! this built an EMPTY bag, and the plan built from it was this fixture's "wholly valid" control. An
	//! empty bag is a phase that can neither advance (nothing gates it) nor end (nothing terminal acts),
	//! which is the exact fault the wedge rule names, so the control plan stopped being valid. One
	//! advance condition and one idle abort is the smallest bag a modder could honestly ship, and it is
	//! what a valid control has to carry now.
	//! \param[in] name The phase's persistence key.
	//! \return The phase.
	protected OVT_ObjectivePhase MakePhase(string name)
	{
		OVT_ObjectivePhase phase = MakeBarePhase(name);

		OVT_TargetKindIsObjectiveCondition condition = new OVT_TargetKindIsObjectiveCondition();
		condition.m_sModuleName = "fixture advance condition";
		condition.m_iRequiredKind = OVT_EObjectiveKind.TOWN;
		phase.m_aModules.Insert(condition);

		OVT_IdleForObjectiveAbort abort = new OVT_IdleForObjectiveAbort();
		abort.m_sModuleName = "fixture idle abort";
		abort.m_sPhaseWork = "the fixture phase";
		abort.m_sGoalNotReached = "the fixture goal";
		abort.m_bBlacklist = true;
		phase.m_aModules.Insert(abort);

		return phase;
	}

	//------------------------------------------------------------------------------------------------
	//! A phase with its header fields set and an EMPTY module bag, for the fixtures that are supposed to
	//! be broken in a module-bag way.
	//! \param[in] name The phase's persistence key.
	//! \return The phase.
	protected OVT_ObjectivePhase MakeBarePhase(string name)
	{
		OVT_ObjectivePhase phase = new OVT_ObjectivePhase();
		phase.m_sPhaseName = name;
		phase.m_iOperationCadence = OVT_ObjectivePlanRules.USE_DIFFICULTY;
		phase.m_fAnchorRadius = OVT_ObjectivePlanRules.USE_DIFFICULTY;
		phase.m_iIdleTimeoutTicks = OVT_ObjectivePlanRules.USE_DIFFICULTY;
		phase.m_aModules = new array<ref OVT_BaseObjectiveModule>();

		return phase;
	}

	//------------------------------------------------------------------------------------------------
	//! THE WEDGE: a phase with modules but with neither an advance condition nor a terminal operation.
	//!
	//! ⚠ ITS OPERATION IS OTHERWISE PERFECTLY VALID - a resolver and a config name the registry really
	//! carries - so the wedge rule is the ONLY rule that can fire on it. A fixture that also tripped a
	//! send rule would pass this case for the wrong reason.
	//! \return The phase.
	protected OVT_ObjectivePhase MakeWedgePhase()
	{
		OVT_ObjectivePhase phase = MakeBarePhase("One");
		phase.m_aModules.Insert(MakeSend(new OVT_ObjectiveSelfTargetResolver(), REAL_CONFIG_NAME));

		return phase;
	}

	//------------------------------------------------------------------------------------------------
	//! A phase whose ONLY fault is in its send-deployment operation.
	//!
	//! ⚠ IT CARRIES AN ADVANCE CONDITION, so the wedge rule cannot fire on it and the send rule is the
	//! only one left.
	//! \param[in] resolver Where the operation sends. Null is the no-resolver fault.
	//! \param[in] configName What it buys. An empty string is the nothing-to-send fault; an unregistered
	//!            name is the does-not-resolve fault.
	//! \return The phase.
	protected OVT_ObjectivePhase MakeSendPhase(OVT_ObjectiveTargetResolver resolver, string configName)
	{
		OVT_ObjectivePhase phase = MakeBarePhase("One");

		OVT_TargetKindIsObjectiveCondition condition = new OVT_TargetKindIsObjectiveCondition();
		condition.m_sModuleName = "fixture advance condition";
		condition.m_iRequiredKind = OVT_EObjectiveKind.TOWN;
		phase.m_aModules.Insert(condition);

		phase.m_aModules.Insert(MakeSend(resolver, configName));

		return phase;
	}

	//------------------------------------------------------------------------------------------------
	//! One send-deployment operation. `new` applies no attribute defaults, so every field a validator
	//! rule reads is set explicitly.
	//! \param[in] resolver Where it sends, or null.
	//! \param[in] configName What it buys, or an empty string.
	//! \return The module.
	protected OVT_SendDeploymentObjectiveOperation MakeSend(OVT_ObjectiveTargetResolver resolver, string configName)
	{
		OVT_SendDeploymentObjectiveOperation send = new OVT_SendDeploymentObjectiveOperation();
		send.m_sModuleName = "fixture send";
		send.m_sConfigName = configName;
		send.m_aLadder = new array<string>();
		send.m_sLadderProgressKey = "";
		send.m_Resolver = resolver;
		send.m_iMaxConcurrent = OVT_ObjectivePlanRules.USE_DIFFICULTY;
		send.m_fConcurrencyRadius = 800;
		send.m_fDedupRadius = 0;
		send.m_iRequiredTargetKind = OVT_EObjectiveKind.NONE;

		return send;
	}

	//------------------------------------------------------------------------------------------------
	//! A plan whose only possible fault is in the one phase it is handed. Every other field is valid.
	//! \param[in] name The plan's persistence key.
	//! \param[in] phase The phase to hang on it.
	//! \return The plan.
	protected OVT_ObjectiveConfig MakeModulePlan(string name, OVT_ObjectivePhase phase)
	{
		OVT_ObjectiveConfig plan = new OVT_ObjectiveConfig();
		plan.m_sObjectiveName = name;
		plan.m_iAllowedFactionTypes = OVT_FactionTypeFlag.OCCUPYING_FACTION;
		plan.m_fPriority = 0;
		plan.m_fChance = 100;
		plan.m_iMaxInstances = 1;
		plan.m_Selector = MakeShippedSelector();
		plan.m_aPhases = new array<ref OVT_ObjectivePhase>();
		plan.m_aPhases.Insert(phase);

		return plan;
	}
}

//------------------------------------------------------------------------------------------------
//! The TERMINAL BATTLE OPERATION clones every attribute it and its parents declare.
//!
//! 🔴 THE DROPPED-LINE TRAP. CloneModule() copies by hand, is not chained, and silently drops what it
//! forgets - and the config's own module objects are TEMPLATES that are never run, so what actually
//! executes in a campaign is always a clone. Neither dropped line here is a compile error, a runtime
//! error, or visible in any log:
//!   m_eMode reads 0 = STANDARD, so the silent encirclement the director exists to mount becomes the
//!     player-facing battle a captured base raises - announced at once, with a 120-second countdown.
//!     The battle still happens; it is simply the wrong one, in the wrong doctrine, every time.
//!   m_fBaseResolveRadius reads 0, so NO base is ever within it: every base doctrine refuses its own
//!     battle, sits out its battle phase and is abandoned with the blacklist when the clock runs out.
//!
//! ⚠ EVERY FIELD IS SET TO A DISTINCT NON-DEFAULT VALUE AND ASSERTED INDIVIDUALLY, with its own
//! failure message naming the field. A single "the clone differs" assertion would pass this case's
//! purpose by: the whole point is to say WHICH line was dropped.
//!
//! ⚠ THE TWO RUNTIME LATCHES ARE DELIBERATELY NOT COPIED and this case does not ask for them. "A battle
//! has been started" and "the refusal has been said" are per phase ENTRY, and a clone is a fresh module
//! for a fresh entry: copying them would give a restored objective a module that believes it already
//! started a battle nothing can find.
//!
//! ⚠ IT REPLACED THE TWO STRANGLER-SHIM CLONE CASES THAT SORTED HERE. Build phase 6 deleted both shim
//! classes with the last of the hard-coded doctrine, and the rule they were covering - one dedicated
//! clone-fidelity case per clonable module - moved to the module that replaced them.
//!
//! CAN-FAIL: the `clone.m_eMode = m_eMode;` line was removed from CloneModule(). The tree recompiled
//! clean (tools/compile-check.sh exit 0) and the case then reports "OVT_StartBattleObjectiveOperation
//! .CloneModule() dropped m_eMode: expected 0, got 1". Line restored, tree recompiled clean.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFramework_CBattleOperationClonesEveryAttribute : SCR_AutotestCaseBase
{
	//! Deliberately NOT the shipped COUNTER_ATTACK: the attribute's declared default is COUNTER_ATTACK,
	//! so a dropped copy that happened to leave the default in place would be invisible against it.
	static const OVT_EQRFMode MODE = OVT_EQRFMode.STANDARD;

	//! Deliberately not 100, which is the declared default and what both plans author.
	static const float RESOLVE_RADIUS = 137.5;

	//! Deliberately not empty, so a dropped name is visible.
	static const string MODULE_NAME = "clone fidelity fixture";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_StartBattleObjectiveOperation template = new OVT_StartBattleObjectiveOperation();
		template.m_sModuleName = MODULE_NAME;
		template.m_eMode = MODE;
		template.m_fBaseResolveRadius = RESOLVE_RADIUS;

		OVT_StartBattleObjectiveOperation clone = OVT_StartBattleObjectiveOperation.Cast(template.CloneModule());
		if (!clone)
		{
			SetFailure("OVT_StartBattleObjectiveOperation.CloneModule() answered null or the wrong type - the battle phase would enter with no operation module at all and the whole ramp would end in a phase that does nothing");
			return true;
		}

		if (clone == template)
		{
			SetFailure("OVT_StartBattleObjectiveOperation.CloneModule() answered the TEMPLATE itself - two objectives reaching the battle phase would share one module and one 'a battle has been started' latch, so the second would believe a battle it never started had already resolved and end itself on its first tick");
			return true;
		}

		if (clone.m_sModuleName != MODULE_NAME)
		{
			SetFailure("OVT_StartBattleObjectiveOperation.CloneModule() dropped m_sModuleName: expected '%1', got '%2' - the parent's attributes are NOT copied for it", MODULE_NAME, clone.m_sModuleName);
			return true;
		}

		if (clone.m_eMode != MODE)
		{
			SetFailure("OVT_StartBattleObjectiveOperation.CloneModule() dropped m_eMode: expected %1, got %2 - every counter-attack would be fought as the mode the clone defaulted to, and nothing anywhere logs the difference", MODE.ToString(), clone.m_eMode.ToString());
			return true;
		}

		if (clone.m_fBaseResolveRadius != RESOLVE_RADIUS)
		{
			SetFailure("OVT_StartBattleObjectiveOperation.CloneModule() dropped m_fBaseResolveRadius: expected %1, got %2 - at zero no base is ever near enough its own recorded position, so every base doctrine refuses its own battle and is abandoned when the phase times out", RESOLVE_RADIUS.ToString(), clone.m_fBaseResolveRadius.ToString());
			return true;
		}

		// 🔴 THE LATCHES ARE NOT ATTRIBUTES AND MUST NOT TRAVEL. A clone is a fresh module for a fresh
		// phase entry: it has started nothing and said nothing. There is no getter for either - they are
		// protected runtime state - so what is asserted is the observable consequence: a clone that
		// believed it had already started a battle would END the objective on its first tick instead of
		// starting one, and IsInitialized() is the nearest thing to a state read this seam offers.
		if (clone.IsInitialized())
		{
			SetFailure("OVT_StartBattleObjectiveOperation.CloneModule() answered a module that is already INITIALISED - a clone is bound to its objective by the phase entry that made it, and one that arrives pre-bound is holding a reference to whatever objective the template was last used for");
			return true;
		}

		Print("Objective framework: the terminal battle operation clones its mode, its base resolve radius and its inherited name, and carries none of its runtime latches across");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFramework_ECommitEntersPhaseZeroAndEveryEntrySyncsTheInstance : SCR_AutotestCaseBase
{
	//! Somewhere no fixture in this world sits, so nothing the case does can collide with another
	//! case's objective. Nothing is spawned here - the objective is a record and a bias.
	static const vector FIXTURE_POSITION = "1500 0 1500";

	//! The two shipped plans' m_sObjectiveName values, AS LITERALS - see case A for why a literal is
	//! the only assertion that can catch a rename.
	//!
	//! ⚠ THIS CASE ALSO PINS ResolvePlanForKind(). A three-argument CommitObjective() no longer looks
	//! two plan names up by kind; it asks the registry which plan's SELECTOR declares it can score that
	//! kind. The town commit below therefore proves the town plan's selector claims towns and the base
	//! commit proves the base plan's claims bases - which is a stronger statement than the name table
	//! it replaced, and it goes red if a shipped selector's m_Selector line is dropped from a .conf.
	static const string TOWN_PLAN = "Town Offensive";

	//! As above, for the base doctrine.
	static const string BASE_PLAN = "Base Offensive";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		if (!director)
		{
			SetFailure("OVT_Global.GetObjectiveDirector() is null");
			return true;
		}

		if (!director.GetRegistry())
		{
			SetFailure("The objective director has no plan registry, so a committed objective has no plan to bind and this case cannot say whether the seam is in step");
			return true;
		}

		director.CommitObjective(OVT_EObjectiveKind.TOWN, FIXTURE_POSITION, "objective framework fixture");

		string failure = AssertState(director, 1, TOWN_PLAN, 0, "Harassment", "committing an objective");

		if (failure == "")
		{
			director.EnterPhase("ForwardBase");
			failure = AssertState(director, 1, TOWN_PLAN, 1, "ForwardBase", "entering the forward-base phase");
		}

		if (failure == "")
		{
			director.EnterPhase("CounterAttack");
			failure = AssertState(director, 1, TOWN_PLAN, 2, "CounterAttack", "entering the counter-attack phase");
		}

		// A BASE objective runs the OTHER plan. Committing over the top of a live objective is what the
		// selection path does every time an objective ends, so it is the same call.
		if (failure == "")
		{
			director.CommitObjective(OVT_EObjectiveKind.BASE, FIXTURE_POSITION, "objective framework base fixture");
			failure = AssertState(director, 1, BASE_PLAN, 0, "Harassment", "committing a base objective");
		}

		// --- The reset path drops everything: the instance leaves the live list, the phase is
		//     forgotten and the module set is emptied. A module left initialised would carry a latch
		//     into the next objective.
		director.ResetObjective("objective framework fixture torn down", false);

		if (failure == "")
			failure = AssertState(director, 0, "", -1, "", "resetting the objective");

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective framework: a commit binds the plan and enters phase zero by name, every entry re-syncs the index, the name and the module set, and a reset drops all of it");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts the whole seam in one place.
	//! \param[in] director The director.
	//! \param[in] instances How many objectives must be live.
	//! \param[in] planName The plan the objective must be bound to.
	//! \param[in] phaseIndex Which phase of it.
	//! \param[in] phaseName The authored name of that phase.
	//! \param[in] what What the case had just done, for the failure message.
	//! \return An empty string when everything is in step, otherwise the failure.
	protected string AssertState(notnull OVT_ObjectiveDirectorComponent director, int instances, string planName, int phaseIndex, string phaseName, string what)
	{
		if (director.GetInstanceCount() != instances)
			return what + " left " + director.GetInstanceCount().ToString() + " live objective(s), expected " + instances.ToString();

		if (director.GetObjectiveConfigName() != planName)
			return what + " bound the plan '" + director.GetObjectiveConfigName() + "', expected '" + planName + "'";

		if (director.GetObjectivePhaseIndex() != phaseIndex)
			return what + " left the objective at plan phase index " + director.GetObjectivePhaseIndex().ToString() + ", expected " + phaseIndex.ToString();

		if (director.GetObjectivePhaseName() != phaseName)
			return what + " left the objective in phase '" + director.GetObjectivePhaseName() + "', expected '" + phaseName + "'. That name is what the save payload carries";

		// An IDLE machine must be carrying no modules at all - see the reset step.
		if (instances == 0)
		{
			if (director.GetRuntimeModuleCount() != 0)
				return what + " left " + director.GetRuntimeModuleCount().ToString() + " runtime module(s) behind. A module left initialised carries its latches into the next objective";

			return "";
		}

		return AssertRuntimeSetMatchesTheAuthoredPhase(director, what);
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 THE RUNTIME MODULE SET IS THE PHASE THAT WAS JUST ENTERED - MODULE BY MODULE, IN ORDER,
	//! CLONED, AND INITIALISED. Read this case's header before replacing any of it with a count.
	//!
	//! The expectation is DERIVED from the plan the objective is running, so re-authoring a doctrine
	//! cannot make this case stale, and two phases of the same size cannot hide a failed swap from it.
	//! \param[in] director The director.
	//! \param[in] what What the case had just done, for the failure message.
	//! \return An empty string when the set is exactly the authored phase's, otherwise the failure.
	protected string AssertRuntimeSetMatchesTheAuthoredPhase(notnull OVT_ObjectiveDirectorComponent director, string what)
	{
		OVT_ObjectiveInstance instance = director.GetObjectiveInstance(0);
		if (!instance)
			return what + " left no objective instance to read a module set from";

		OVT_ObjectiveConfig plan = instance.GetConfig();
		if (!plan)
			return what + " left the instance with no plan bound, so there is nothing to compare its module set against";

		OVT_ObjectivePhase authored = plan.GetPhase(instance.GetPhaseIndex());
		if (!authored || !authored.m_aModules)
			return what + " left the instance in a phase the plan does not carry, so its module set cannot be checked";

		int expected = authored.m_aModules.Count();

		if (instance.GetRuntimeModuleCount() != expected)
			return what + " left " + instance.GetRuntimeModuleCount().ToString() + " runtime module(s) and the phase it entered authors " + expected.ToString() + ". A phase entry that does not swap the set runs the PREVIOUS phase's work under the new phase's name";

		for (int i = 0; i < expected; i++)
		{
			OVT_BaseObjectiveModule live = instance.GetRuntimeModule(i);
			OVT_BaseObjectiveModule template = authored.m_aModules[i];

			if (!live || !template)
				return what + " left a null module at position " + i.ToString() + " of the runtime set";

			if (live.Type() != template.Type())
				return what + " put a " + live.Type().ToString() + " at position " + i.ToString() + " of the runtime set and the phase authors a " + template.Type().ToString() + " there. ORDER IS EVALUATION ORDER - the shipped chain is tower recapture, then the harassment ladder, then sabotage, and the first module that acts consumes the cadence";

			if (live.m_sModuleName != template.m_sModuleName)
				return what + " put '" + live.m_sModuleName + "' at position " + i.ToString() + " of the runtime set and the phase authors '" + template.m_sModuleName + "' there";

			// 🔴 A CLONE, NEVER THE CONFIG'S OWN OBJECT. Two objectives entering one phase would
			// otherwise share one module and one set of latches, and the second would inherit the
			// first's state.
			if (live == template)
				return what + " put the CONFIG'S OWN TEMPLATE object into the runtime set at position " + i.ToString() + " rather than a clone of it. Two objectives running that phase would share one module and one set of latches";

			if (!live.IsInitialized())
				return what + " left the module at position " + i.ToString() + " un-Initialize()d, so it has no objective bound and would refuse everything it is asked";
		}

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! The two success counters live in the instance's bag now, one reader and one writer see the same
//! number, and neither of them can move the machine.
//!
//! 🔴 "A PUBLIC COUNTER MAY NEVER CHANGE PHASE" IS A RULE THIS FEATURE INHERITED THE HARD WAY - it
//! cost two red cases in two suites when the opposite was tried. ReportObjectiveProgress() is public and
//! is called from a deployment's own update, from a restore and from test fixtures arranging a known
//! state, none of which is a director tick; a transition from any of them silently advances the ramp
//! AND re-arms the idle clock, overwriting whatever a caller had just planted. The counters moving
//! into a generic bag is exactly the kind of refactor that quietly re-introduces it, so the rule is
//! re-asserted at the new call site rather than assumed to have survived.
//!
//! ⚠ THE "ONE STORAGE" HALF IS THE REFACTOR'S OWN CLAIM. GetHarassmentSuccesses() kept its name and
//! its meaning while its storage moved from a record field to a bag key; if the two ever disagreed
//! there would be two sources of truth for the group ladder and for the base gate, and the save would
//! carry whichever one the serializer happened to read.
//!
//! CAN-FAIL: GetHarassmentSuccesses() was pointed at BAG_SABOTAGE_SUCCESSES - the exact shape of a
//! copy-paste slip between two adjacent one-line getters. The tree recompiled clean (exit 0) and the
//! case then reports "the counter and the bag key disagree: GetHarassmentSuccesses() says 2 and the
//! bag says 1 - there are two sources of truth for the group ladder".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFramework_FCountersAreBagKeysAndNeverChangeThePhase : SCR_AutotestCaseBase
{
	static const vector FIXTURE_POSITION = "1600 0 1600";

	//! Deliberately not a value any phase entry or any other case produces.
	static const int PLANTED_PHASE_TICKS = 133;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		if (!director)
		{
			SetFailure("OVT_Global.GetObjectiveDirector() is null");
			return true;
		}

		director.CommitObjective(OVT_EObjectiveKind.TOWN, FIXTURE_POSITION, "objective bag fixture");
		director.SetPhaseTimeout(PLANTED_PHASE_TICKS);

		string phaseBefore = director.GetObjectivePhaseName();

		string failure = "";

		// --- Claim 1: a commit starts every counter at nothing. A key carried over from the objective
		//     before would be a counter nobody earned.
		if (director.GetHarassmentSuccesses() != 0 || director.GetSabotageSuccesses() != 0)
			failure = "a freshly committed objective starts with " + director.GetHarassmentSuccesses().ToString() + " harassment and " + director.GetSabotageSuccesses().ToString() + " sabotage success(es), expected none - the bag was not emptied on commit";

		// --- Claim 2: one writer, one reader, one number.
		if (failure == "")
		{
			director.ReportObjectiveProgress(OVT_ObjectiveInstance.BAG_HARASSMENT_SUCCESSES, 1);
			director.ReportObjectiveProgress(OVT_ObjectiveInstance.BAG_SABOTAGE_SUCCESSES, 1);
			director.ReportObjectiveProgress(OVT_ObjectiveInstance.BAG_SABOTAGE_SUCCESSES, 1);

			if (director.GetObjectiveBagValue(OVT_ObjectiveInstance.BAG_HARASSMENT_SUCCESSES) != director.GetHarassmentSuccesses())
				failure = "the counter and the bag key disagree: GetHarassmentSuccesses() says " + director.GetHarassmentSuccesses().ToString() + " and the bag says " + director.GetObjectiveBagValue(OVT_ObjectiveInstance.BAG_HARASSMENT_SUCCESSES).ToString() + " - there are two sources of truth for the group ladder";
		}

		if (failure == "" && director.GetHarassmentSuccesses() != 1)
			failure = "one reported harassment operation read back as " + director.GetHarassmentSuccesses().ToString() + ", expected 1";

		if (failure == "" && director.GetSabotageSuccesses() != 2)
			failure = "two reported sabotage missions read back as " + director.GetSabotageSuccesses().ToString() + ", expected 2";

		if (failure == "" && director.GetObjectiveBagValue(OVT_ObjectiveInstance.BAG_SABOTAGE_SUCCESSES) != 2)
			failure = "the sabotage bag key read back as " + director.GetObjectiveBagValue(OVT_ObjectiveInstance.BAG_SABOTAGE_SUCCESSES).ToString() + ", expected 2";

		// --- Claim 3: 🔴 NOTHING ABOVE MOVED THE MACHINE. Not the phase, and not the idle clock a
		//     phase entry would have re-armed on its way past.
		if (failure == "" && director.GetObjectivePhaseName() != phaseBefore)
			failure = "reporting a completed operation CHANGED THE PHASE, from " + phaseBefore + " to " + director.GetObjectivePhaseName() + " - only the director's tick may move the machine";

		if (failure == "" && director.GetPhaseTicks() != PLANTED_PHASE_TICKS)
			failure = "reporting a completed operation re-armed the idle clock, from " + PLANTED_PHASE_TICKS.ToString() + " to " + director.GetPhaseTicks().ToString() + " - a counter that re-arms a timer overwrites whatever a caller had just planted";

		// --- Claim 4: an arbitrary module key round-trips through the same bag, which is what makes the
		//     save format enumerable rather than a list somebody maintains.
		if (failure == "")
		{
			director.SetObjectiveBagValue("framework.probe", 7);
			director.SetObjectiveBagPosition("framework.probe", FIXTURE_POSITION);

			if (director.GetObjectiveBagValue("framework.probe") != 7)
				failure = "a module's own bag key did not read back: expected 7, got " + director.GetObjectiveBagValue("framework.probe").ToString();
			else if (vector.Distance(director.GetObjectiveBagPosition("framework.probe"), FIXTURE_POSITION) > 1)
				failure = "a module's own vector bag key did not read back";
		}

		// --- Claim 5: committing again empties the bag, including keys no shipped module owns.
		if (failure == "")
		{
			director.CommitObjective(OVT_EObjectiveKind.TOWN, FIXTURE_POSITION, "objective bag fixture, second objective");

			if (director.GetObjectiveBagValue("framework.probe") != 0)
				failure = "a bag key survived a commit: a new objective must never inherit the last one's counters";
		}

		director.ResetObjective("objective bag fixture torn down", false);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective framework: both success counters are bag keys with one storage, an arbitrary module key round-trips, a commit empties the bag, and no counter moves the phase or the clock");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The phase-advance arithmetic and the module lifecycle, on a plan built entirely by hand.
//!
//! WHY BY HAND. The two shipped plans carry legacy shims whose conditions never fire, so the RUNNER's
//! advance path cannot be driven end-to-end until a real condition module exists (it arrives with the
//! harassment doctrine). What can be pinned now is the machinery that path is made of, and it is
//! pinned here so that the phase which replaces the first shim is adding a caller to tested code
//! rather than testing three things at once.
//!
//! ⚠ "THE LAST PHASE ADVANCES TO NOTHING" IS THE HALF THAT MATTERS. A plan that ran off the end of
//! its own phase list would either index past the end of an authored array or silently loop back to
//! its first phase - and a plan that loops is a campaign that never finishes an objective.
//!
//! ⚠ THE MODULES ARE CLONES, NOT THE TEMPLATES. Asserting that is asserting that two objectives
//! entering the same phase cannot share one module's latches - which is the same claim the two clone
//! cases make about one module, made here about the collection.
//!
//! ⚠ `new` APPLIES NO ATTRIBUTE DEFAULTS, so every field is set explicitly below.
//!
//! CAN-FAIL: the `if (next < 0 || next >= m_Config.GetPhaseCount())` bound was changed to `if
//! (next < 0)`. The tree recompiled clean (exit 0) and the case then reports "the LAST phase of a plan
//! advanced to index 3 instead of answering -1 - a plan that runs off its own end either reads past an
//! authored array or loops forever".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFramework_GPhaseAdvanceArithmeticAndModuleLifecycle : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		if (!director)
		{
			SetFailure("OVT_Global.GetObjectiveDirector() is null");
			return true;
		}

		OVT_ObjectiveConfig plan = new OVT_ObjectiveConfig();
		plan.m_sObjectiveName = "framework lifecycle fixture";
		plan.m_iAllowedFactionTypes = OVT_FactionTypeFlag.OCCUPYING_FACTION;
		plan.m_fPriority = 1;
		plan.m_fChance = 100;
		plan.m_iMaxInstances = 1;
		plan.m_aPhases = new array<ref OVT_ObjectivePhase>();
		plan.m_aPhases.Insert(MakePhase("First"));
		plan.m_aPhases.Insert(MakePhase("Second"));
		plan.m_aPhases.Insert(MakePhase("Third"));

		// --- Claim 1: a fresh instance is in no phase at all, and knows it.
		OVT_ObjectiveInstance instance = new OVT_ObjectiveInstance(director);
		instance.SetConfig(plan);

		if (instance.GetPhaseIndex() != -1 || instance.GetPhaseName() != "")
		{
			SetFailure("A fresh objective instance reports phase %1 named '%2', expected no phase at all", instance.GetPhaseIndex().ToString(), instance.GetPhaseName());
			return true;
		}

		// --- Claim 2: the advance walks the authored order and STOPS at the end.
		instance.RecordPhase(0, "First");
		if (instance.GetNextPhaseIndex() != 1)
		{
			SetFailure("The first phase of a plan advanced to index %1, expected 1", instance.GetNextPhaseIndex().ToString());
			return true;
		}

		instance.RecordPhase(1, "Second");
		if (instance.GetNextPhaseIndex() != 2)
		{
			SetFailure("The middle phase of a plan advanced to index %1, expected 2", instance.GetNextPhaseIndex().ToString());
			return true;
		}

		instance.RecordPhase(2, "Third");
		if (instance.GetNextPhaseIndex() != -1)
		{
			SetFailure("the LAST phase of a plan advanced to index %1 instead of answering -1 - a plan that runs off its own end either reads past an authored array or loops forever", instance.GetNextPhaseIndex().ToString());
			return true;
		}

		// --- Claim 3: entering a phase clones its modules and initialises each of them.
		OVT_ObjectivePhase first = plan.GetPhase(0);
		instance.EnterRuntimePhase(first);

		if (instance.GetRuntimeModuleCount() != 1)
		{
			SetFailure("Entering a phase with one module produced %1 runtime module(s)", instance.GetRuntimeModuleCount().ToString());
			return true;
		}

		OVT_BaseObjectiveModule runtime = instance.GetRuntimeModule(0);
		if (!runtime || !runtime.IsInitialized())
		{
			SetFailure("The cloned module was not initialised on phase entry, so nothing would ever have told it which objective it belongs to");
			return true;
		}

		if (runtime == first.m_aModules[0])
		{
			SetFailure("The runtime module IS the config's template object - two objectives in this phase would share one module and one set of latches");
			return true;
		}

		// --- Claim 4: entering the NEXT phase exits the outgoing set and replaces it.
		OVT_BaseObjectiveModule outgoing = runtime;
		instance.EnterRuntimePhase(plan.GetPhase(1));

		if (outgoing.IsInitialized())
		{
			SetFailure("A module from the phase that just ended is still initialised - a latch it set would be carried into the phase that replaced it");
			return true;
		}

		if (instance.GetRuntimeModuleCount() != 1 || instance.GetRuntimeModule(0) == outgoing)
		{
			SetFailure("Entering the next phase did not replace the runtime module set");
			return true;
		}

		// --- Claim 5: exiting leaves nothing behind.
		instance.ExitRuntimePhase();

		if (instance.GetRuntimeModuleCount() != 0)
		{
			SetFailure("Exiting a phase left %1 runtime module(s) behind", instance.GetRuntimeModuleCount().ToString());
			return true;
		}

		Print("Objective framework: a plan's phases advance in authored order and stop at the end, entry clones and initialises, and every entry and exit tells the outgoing modules their phase is over");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds one phase carrying a single module, by hand.
	//!
	//! ⚠ THE MODULE IS NEVER ASKED ANYTHING. This case drives the phase LIFECYCLE - clone, initialise,
	//! exit, replace - so what it needs is a concrete clonable module and not a particular behaviour. It
	//! carried a strangler shim until build phase 6 deleted both shim classes; the idle abort replaces it
	//! because it is the simplest shipped module that owns attributes of its own, and its ShouldAbort()
	//! is never called here.
	//! \param[in] name The phase's persistence key.
	//! \return The phase.
	protected OVT_ObjectivePhase MakePhase(string name)
	{
		OVT_IdleForObjectiveAbort module = new OVT_IdleForObjectiveAbort();
		module.m_sModuleName = "lifecycle fixture " + name;
		module.m_sPhaseWork = name;
		module.m_sGoalNotReached = "the next phase";
		module.m_bBlacklist = true;

		OVT_ObjectivePhase phase = new OVT_ObjectivePhase();
		phase.m_sPhaseName = name;
		phase.m_iOperationCadence = OVT_ObjectivePlanRules.USE_DIFFICULTY;
		phase.m_fAnchorRadius = OVT_ObjectivePlanRules.USE_DIFFICULTY;
		phase.m_iIdleTimeoutTicks = OVT_ObjectivePlanRules.USE_DIFFICULTY;
		phase.m_aModules = new array<ref OVT_BaseObjectiveModule>();
		phase.m_aModules.Insert(module);

		return phase;
	}
}

//------------------------------------------------------------------------------------------------
//! A PLAN THAT FAILED VALIDATION IS NEVER SELECTED, AND THE REST OF THE REGISTRY STILL RUNS.
//!
//! 🔴 WHY THE SKIP LIST NEEDS ITS OWN CASE AT THIS TIER. The validator's own case
//! (..._BValidatorNamesAndSkipsABrokenPlan) proves each rule FIRES and names the plan; it says nothing
//! about the skip having any consequence. The consequence is the whole point: a broken plan that is
//! reported and then selected anyway would commit an objective to a doctrine that can neither advance
//! nor end, and the campaign would look exactly as it does when everything is fine - one objective,
//! one target, one log line - right up until it silently never progressed.
//!
//! ⚠ THE FIXTURE PLAN IS AUTHORED TO WIN. Priority 1000 against both shipped plans' 1, a shipped town
//! selector at its shipped weights, and it claims the same source the town doctrine does - so if the
//! skip list were not consulted it would out-rank every real plan on every candidate, and the failure
//! would be total rather than intermittent. That is deliberate: a fixture that only MIGHT have won
//! could pass this case by luck.
//!
//! ⚠ ITS ONLY FAULT IS THE WEDGE RULE, so the plan is structurally perfect in every other respect -
//! named, priced, selectable, with a phase carrying a real operation that names a real deployment
//! config. Nothing but validation stands between it and being committed to.
//!
//! ⚠ IT MUTATES THE LIVE REGISTRY AND PUTS IT BACK BEFORE ASSERTING, on the precedent every world-
//! rearranging case in this tier follows. The plan is removed and the registry re-validated inside the
//! same step that added it, so a red case cannot leave a wedged doctrine in the registry for whatever
//! case runs next.
//!
//! ⚠ THE INITIALISATION TIER NEVER RUNS PostGameStart(), so nothing has validated the live registry
//! before this case does - which is exactly why the case validates it explicitly rather than assuming
//! a skip list exists.
//!
//! CAN-FAIL: remove the `if (m_Registry.IsSkipped(plan.m_sObjectiveName)) continue;` line from
//! CollectEligiblePlans(). The tree compiles clean - a missing guard is not a script error - and the
//! case then reports "the campaign committed to a plan the validator had SKIPPED".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ObjectiveFramework_HASkippedPlanIsNeverSelected : SCR_AutotestCaseBase
{
	//! The fixture doctrine's persistence key. Long and unmistakable: it appears in a log line if this
	//! ever goes wrong in a real session.
	static const string FIXTURE_PLAN = "Init fixture - wedged doctrine that must never be selected";

	//! Priority high enough that this plan out-ranks every shipped one on every candidate.
	static const float FIXTURE_PRIORITY = 1000;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!director || !occupying || !towns || !config)
		{
			SetFailure("The director, the occupying faction manager, the town manager or the config did not resolve");
			return true;
		}

		OVT_ObjectiveRegistry registry = director.GetRegistry();
		if (!registry || !registry.m_aObjectiveConfigs)
		{
			SetFailure("The director carries no objective registry, so there is nothing for a skipped plan to be skipped OUT of. Configs/Objective/overthrowObjectives.conf is not wired on the game-mode prefab.");
			return true;
		}

		if (!towns.m_Towns || towns.m_Towns.IsEmpty())
		{
			SetFailure("The world produced no towns, so there is nothing to select");
			return true;
		}

		int occupyingIndex = config.GetOccupyingFactionIndex();
		int resistanceIndex = config.GetPlayerFactionIndex();

		if (occupyingIndex == resistanceIndex)
		{
			SetFailure("The occupying and resistance faction indices are the same (%1), so 'resistance-held' cannot be expressed", occupyingIndex.ToString());
			return true;
		}

		// --- ARRANGE the map: exactly one resistance-held town, nothing else selectable.
		array<int> townFactions = new array<int>();
		array<OVT_TownSize> townSizes = new array<OVT_TownSize>();
		foreach (OVT_TownData town : towns.m_Towns)
		{
			townFactions.Insert(town.faction);
			townSizes.Insert(town.size);
			town.faction = occupyingIndex;
		}

		array<int> baseFactions = new array<int>();
		foreach (OVT_BaseData base : occupying.m_Bases)
		{
			baseFactions.Insert(base.faction);
			base.faction = occupyingIndex;
		}

		OVT_TownData fixture = towns.m_Towns[0];
		fixture.faction = resistanceIndex;
		fixture.size = OVT_TownSize.TOWN;

		// --- ARRANGE the registry: one wedged doctrine that would out-rank everything.
		registry.m_aObjectiveConfigs.Insert(MakeWedgedPlan());

		bool everythingValidated = registry.ValidateAllConfigs();
		bool fixtureSkipped = registry.IsSkipped(FIXTURE_PLAN);

		// --- ACT.
		director.SelectObjective();

		string committedPlan = director.GetObjectiveConfigName();
		OVT_EObjectiveKind committedKind = director.GetObjectiveKind();
		string committedName = director.GetObjectiveName();

		// --- RESTORE, all of it, before a single assertion runs.
		director.ResetObjective("initialisation-tier skipped-plan fixture torn down", false);

		for (int i = 0; i < towns.m_Towns.Count(); i++)
		{
			towns.m_Towns[i].faction = townFactions[i];
			towns.m_Towns[i].size = townSizes[i];
		}

		for (int b = 0; b < occupying.m_Bases.Count(); b++)
		{
			occupying.m_Bases[b].faction = baseFactions[b];
		}

		RemoveFixturePlan(registry);
		registry.ValidateAllConfigs();

		// --- ASSERT.
		if (everythingValidated)
		{
			SetFailure("The registry PASSED validation with a phase in it that carries neither an advance condition nor a terminal operation - the fixture is not being validated at all, so nothing below means anything");
			return true;
		}

		if (!fixtureSkipped)
		{
			SetFailure("The wedged fixture plan was not on the skipped list, so this case cannot say whether a skipped plan is selectable. The wedge rule in OVT_ObjectiveRegistry.ValidatePhase() is what should have caught it.");
			return true;
		}

		if (committedPlan == FIXTURE_PLAN)
		{
			SetFailure("the campaign committed to a plan the validator had SKIPPED ('%1', on target '%2') - a doctrine that can neither advance nor end would hold the objective slot until its idle clock abandoned it, over and over, with nothing in the log to say the plan was known to be broken", committedPlan, committedName);
			return true;
		}

		// The other half of the claim, and the reason the rule is "skip", not "stop": one broken plan in
		// a mod must not stop the registry running.
		if (committedKind != OVT_EObjectiveKind.TOWN)
		{
			int kindValue = committedKind;
			SetFailure("with one resistance-held town standing and one broken plan in the registry, selection committed to nothing (kind %1) - a single invalid plan must be skipped, never stop the rest of the registry running", kindValue.ToString());
			return true;
		}

		if (committedPlan == "")
		{
			SetFailure("selection committed to a town but names NO plan for it - the objective would run with nothing driving it, which is the state the registry exists to make impossible");
			return true;
		}

		Print("Objective framework: a plan that fails validation is skipped by selection even when it is authored to out-rank every shipped doctrine, and the valid plans still run - the campaign committed to '" + committedPlan + "'");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the fixture plan back out of the live registry, by name.
	//! \param[in] registry The live registry.
	protected void RemoveFixturePlan(notnull OVT_ObjectiveRegistry registry)
	{
		if (!registry.m_aObjectiveConfigs)
			return;

		for (int i = registry.m_aObjectiveConfigs.Count() - 1; i >= 0; i--)
		{
			OVT_ObjectiveConfig plan = registry.m_aObjectiveConfigs[i];
			if (plan && plan.m_sObjectiveName == FIXTURE_PLAN)
				registry.m_aObjectiveConfigs.RemoveOrdered(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The fixture doctrine: perfect in every respect except that its one phase can neither advance nor
	//! end. `new` applies no [Attribute()] defaults, so every field any rule reads is set explicitly.
	//! \return The plan.
	protected OVT_ObjectiveConfig MakeWedgedPlan()
	{
		OVT_ObjectivePhase phase = new OVT_ObjectivePhase();
		phase.m_sPhaseName = "One";
		phase.m_iOperationCadence = OVT_ObjectivePlanRules.USE_DIFFICULTY;
		phase.m_fAnchorRadius = OVT_ObjectivePlanRules.USE_DIFFICULTY;
		phase.m_iIdleTimeoutTicks = OVT_ObjectivePlanRules.USE_DIFFICULTY;
		phase.m_aModules = new array<ref OVT_BaseObjectiveModule>();

		// A real operation naming a real deployment config: every send rule passes, so the WEDGE rule is
		// the only one that can fire on this plan.
		OVT_SendDeploymentObjectiveOperation send = new OVT_SendDeploymentObjectiveOperation();
		send.m_sModuleName = "fixture send";
		send.m_sConfigName = OVT_ObjectiveDirectorComponent.SABOTAGE_CONFIG;
		send.m_aLadder = new array<string>();
		send.m_sLadderProgressKey = "";
		send.m_Resolver = new OVT_ObjectiveSelfTargetResolver();
		send.m_iMaxConcurrent = OVT_ObjectivePlanRules.USE_DIFFICULTY;
		send.m_fConcurrencyRadius = 800;
		send.m_fDedupRadius = 0;
		send.m_iRequiredTargetKind = OVT_EObjectiveKind.NONE;
		phase.m_aModules.Insert(send);

		OVT_ResistanceTownObjectiveSelector selector = new OVT_ResistanceTownObjectiveSelector();
		selector.ApplyShippedWeights();

		OVT_ObjectiveConfig plan = new OVT_ObjectiveConfig();
		plan.m_sObjectiveName = FIXTURE_PLAN;
		plan.m_iAllowedFactionTypes = OVT_FactionTypeFlag.OCCUPYING_FACTION;
		plan.m_fPriority = FIXTURE_PRIORITY;
		plan.m_fChance = 100;
		plan.m_iMaxInstances = 1;
		plan.m_Selector = selector;
		plan.m_aPhases = new array<ref OVT_ObjectivePhase>();
		plan.m_aPhases.Insert(phase);

		return plan;
	}
}
