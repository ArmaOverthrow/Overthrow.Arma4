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
