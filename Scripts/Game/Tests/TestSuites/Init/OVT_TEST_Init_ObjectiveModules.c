//------------------------------------------------------------------------------------------------
//! THE HARASSMENT PHASE, NOW THAT IT IS AUTHORED DATA: the five objective modules that replaced the
//! director's hard-coded senders and gates, the two resolvers they send through, and the .conf that
//! wires them together.
//!
//! WHAT THIS FILE IS FOR, AND WHY THE INITIALISATION TIER IS THE ONLY PLACE IT CAN LIVE. Nothing here
//! is arithmetic - that is the Logic tier's, and OVT_ObjectivePhaseRules' ladder and gate maths are
//! already pinned there. What is asserted here is the class of fault NO COMPILER CAN SEE:
//!
//!   1. A CLONE THAT DROPPED A LINE. CloneModule() copies by hand, is not chained, and silently ships
//!      the class default for whatever it forgot. Every one of the five new modules gets its own case,
//!      in the OVT_TEST_Init_TowerUnrestRecapture.c:214-259 shape: a hand-built template with DISTINCT
//!      NON-DEFAULT values on every field, and a per-field SetFailure naming what a zero would cost.
//!      A single "the clone differs" assertion would not do, because the whole point is to name the
//!      line that went missing.
//!   2. A NAME THAT NO LONGER RESOLVES. The plan's operations name deployment configs by string, three
//!      times each: the registry resolves the config, the dedup and the concurrency count match live
//!      deployments back to it, and the director's teardown ledger stores it. A rung renamed in
//!      overthrowDeployments.conf and not in Configs/Objective does not fail to parse.
//!   3. A .conf THAT SILENTLY LOST ITS AUTHORING. The harassment ladder is the first array-of-strings
//!      authored anywhere in this mod's own configs, and the whole ramp is four names inside it.
//!   4. THE WORLD-FACT CONJUNCT ON THE TOWN GATE, which is the one rule in this phase whose absence
//!      does not error, does not warn and simply skips the entire harassment phase.
//!
//! ⚠ NOTHING HERE SPENDS THE CAMPAIGN'S RESOURCES OR CREATES A DEPLOYMENT. Every case is either
//! world-free (the clone cases, built with `new`) or drives a READ - a resolver, a config lookup, a
//! condition. The one case that ticks the director (the refusal-log case) empties the pool first
//! precisely so that nothing can be bought, and puts it back on every path.
//!
//! ⚠ `new` DOES NOT APPLY [Attribute()] DEFVALUES. Every hand-built subject in this file therefore
//! sets every field it cares about explicitly, and the clone cases depend on that: a field left unset
//! reads as zero on BOTH sides and the comparison passes vacuously.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! The send-deployment operation clones every authored attribute, including its parent's and
//! including every rung of its ladder.
//!
//! ⚠ THE LADDER IS COPIED ENTRY BY ENTRY, NOT BY HANDING THE REFERENCE ACROSS, and this case asserts
//! both halves: the contents match, AND the arrays are not the same object. A shared array is safe
//! only for as long as nobody writes to it, and "nobody writes to it" is not a property a clone can
//! guarantee about a template it does not own.
//!
//! ⚠ THE RESOLVER IS DELIBERATELY SHARED BY REFERENCE and this case pins that too, so a later reader
//! does not "fix" it into a deep copy. A resolver is arithmetic over its arguments with nothing cached
//! across calls (its seam's contract, point 3), so one instance answering for every clone is the same
//! decision the plan's selectors already make - and cloning it would need a CloneModule() on every
//! resolver plus a dropped-line hazard per resolver attribute, for no behaviour at all.
//!
//! CAN-FAIL, BY CONSTRUCTION: delete any one of the ten assignments in CloneModule() and the field
//! it copied reads as zero or empty on the clone, which no other case in the tree would notice.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveModules_SendDeploymentCloneCarriesEveryAttribute : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_SendDeploymentObjectiveOperation template = new OVT_SendDeploymentObjectiveOperation();

		template.m_sModuleName = "Send Clone Fixture";
		template.m_sConfigName = "fixture config name";
		template.m_sLadderProgressKey = "fixture.progress";
		template.m_iMaxConcurrent = 7;
		template.m_fConcurrencyRadius = 613;
		template.m_fDedupRadius = 271;
		template.m_iRequiredTargetKind = OVT_EObjectiveKind.BASE;
		template.m_iMaxConcurrentDifficulty = OVT_EObjectiveConcurrencyLimit.FORWARD_BASE_GARRISON;
		template.m_bConcurrencyAtResolvedPosition = true;

		template.m_aLadder = new array<string>();
		template.m_aLadder.Insert("fixture rung one");
		template.m_aLadder.Insert("fixture rung two");
		template.m_aLadder.Insert("fixture rung three");

		OVT_ObjectiveSelfTargetResolver resolver = new OVT_ObjectiveSelfTargetResolver();
		resolver.m_fRequireEnemyHeldBaseWithin = 133;
		template.m_Resolver = resolver;

		OVT_SendDeploymentObjectiveOperation clone = OVT_SendDeploymentObjectiveOperation.Cast(template.CloneModule());
		if (!clone)
		{
			SetFailure("OVT_SendDeploymentObjectiveOperation.CloneModule() did not return an instance of its own class");
			return true;
		}

		if (clone.m_sModuleName != template.m_sModuleName)
		{
			SetFailure("CloneModule() dropped m_sModuleName - every log line about this operation loses its label");
			return true;
		}

		if (clone.m_sConfigName != template.m_sConfigName)
		{
			SetFailure(string.Format("CloneModule() dropped m_sConfigName - expected '%1', got '%2'. A clone with an empty config name resolves nothing and refuses on every cadence interval, silently",
				template.m_sConfigName, clone.m_sConfigName));
			return true;
		}

		if (clone.m_sLadderProgressKey != template.m_sLadderProgressKey)
		{
			SetFailure(string.Format("CloneModule() dropped m_sLadderProgressKey - expected '%1', got '%2'. A clone reading the empty key sees zero successes forever and never leaves the bottom rung",
				template.m_sLadderProgressKey, clone.m_sLadderProgressKey));
			return true;
		}

		if (clone.m_iMaxConcurrent != template.m_iMaxConcurrent)
		{
			SetFailure(string.Format("CloneModule() dropped m_iMaxConcurrent - expected %1, got %2. A clone reading 0 has NO concurrency cap at all, which is the unbounded per-tick spender the cadence exists to prevent",
				template.m_iMaxConcurrent.ToString(), clone.m_iMaxConcurrent.ToString()));
			return true;
		}

		if (clone.m_fConcurrencyRadius != template.m_fConcurrencyRadius)
		{
			SetFailure(string.Format("CloneModule() dropped m_fConcurrencyRadius - expected %1, got %2. A clone reading 0 counts nothing towards its own cap, so the cap never binds",
				template.m_fConcurrencyRadius.ToString(), clone.m_fConcurrencyRadius.ToString()));
			return true;
		}

		if (clone.m_fDedupRadius != template.m_fDedupRadius)
		{
			SetFailure(string.Format("CloneModule() dropped m_fDedupRadius - expected %1, got %2. A clone reading 0 never skips a position it has already served, so a second radio tower is never picked up",
				template.m_fDedupRadius.ToString(), clone.m_fDedupRadius.ToString()));
			return true;
		}

		if (clone.m_iRequiredTargetKind != template.m_iRequiredTargetKind)
		{
			SetFailure(string.Format("CloneModule() dropped m_iRequiredTargetKind - expected %1, got %2. A clone reading 0 acts for ANY kind of objective, so a town ramp sends sabotage teams at towns",
				template.m_iRequiredTargetKind.ToString(), clone.m_iRequiredTargetKind.ToString()));
			return true;
		}

		if (clone.m_iMaxConcurrentDifficulty != template.m_iMaxConcurrentDifficulty)
		{
			SetFailure(string.Format("CloneModule() dropped m_iMaxConcurrentDifficulty - expected %1, got %2. A clone reading 0 defers to objectiveHarassmentMaxConcurrent, so the forward base's garrison cap silently becomes the ramp's - a different number on every difficulty preset",
				template.m_iMaxConcurrentDifficulty.ToString(), clone.m_iMaxConcurrentDifficulty.ToString()));
			return true;
		}

		if (clone.m_bConcurrencyAtResolvedPosition != template.m_bConcurrencyAtResolvedPosition)
		{
			SetFailure("CloneModule() dropped m_bConcurrencyAtResolvedPosition. A clone reading false counts the forward-base garrison's cap around the OBJECTIVE, where the garrison is not, so it counts nothing and the base is reinforced without limit");
			return true;
		}

		if (!clone.m_aLadder)
		{
			SetFailure("CloneModule() left m_aLadder null - the ladder module resolves no config at all and the whole town ramp stops sending");
			return true;
		}

		if (clone.m_aLadder.Count() != template.m_aLadder.Count())
		{
			SetFailure(string.Format("CloneModule() dropped ladder rungs - expected %1, got %2. A short ladder saturates early and the escalation stops at whichever rung survived",
				template.m_aLadder.Count().ToString(), clone.m_aLadder.Count().ToString()));
			return true;
		}

		for (int i = 0; i < template.m_aLadder.Count(); i++)
		{
			if (clone.m_aLadder[i] != template.m_aLadder[i])
			{
				SetFailure(string.Format("CloneModule() copied ladder rung %1 wrongly - expected '%2', got '%3'",
					i.ToString(), template.m_aLadder[i], clone.m_aLadder[i]));
				return true;
			}
		}

		if (clone.m_aLadder == template.m_aLadder)
		{
			SetFailure("CloneModule() handed the TEMPLATE's ladder array across rather than copying it. Two objectives running the same phase would then share one array, and anything that ever wrote to one would rewrite the doctrine of the other");
			return true;
		}

		if (clone.m_Resolver != template.m_Resolver)
		{
			SetFailure("CloneModule() copied the resolver instead of sharing it. Sharing is deliberate - a resolver caches nothing across calls, so one instance answers for every clone, and deep-copying it would need a CloneModule() on every resolver with a dropped-line hazard per attribute");
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The support-below condition clones both of its attributes.
//!
//! 🔴 THE SECOND ONE IS THE WORLD-FACT CONJUNCT AND ITS ABSENCE IS INVISIBLE IN PLAY. A clone whose
//! m_sRequiredTownModifier came back empty evaluates the support threshold ALONE - and a town that is
//! already under half when it is chosen then advances on the phase's own entry tick, skipping the
//! whole harassment ramp. Nothing errors, nothing warns, and the only symptom is a forward base going
//! up before a single harassment group has been sent.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveModules_SupportBelowCloneCarriesEveryAttribute : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_SupportBelowObjectiveCondition template = new OVT_SupportBelowObjectiveCondition();

		template.m_sModuleName = "Support Clone Fixture";
		template.m_iSupportThreshold = 37;
		template.m_sRequiredTownModifier = "FixtureModifierName";

		OVT_SupportBelowObjectiveCondition clone = OVT_SupportBelowObjectiveCondition.Cast(template.CloneModule());
		if (!clone)
		{
			SetFailure("OVT_SupportBelowObjectiveCondition.CloneModule() did not return an instance of its own class");
			return true;
		}

		if (clone.m_sModuleName != template.m_sModuleName)
		{
			SetFailure("CloneModule() dropped m_sModuleName");
			return true;
		}

		if (clone.m_iSupportThreshold != template.m_iSupportThreshold)
		{
			SetFailure(string.Format("CloneModule() dropped m_iSupportThreshold - expected %1, got %2. A clone reading 0 demands support strictly below zero, which no town can ever reach, so the ramp runs until its idle clock abandons it",
				template.m_iSupportThreshold.ToString(), clone.m_iSupportThreshold.ToString()));
			return true;
		}

		if (clone.m_sRequiredTownModifier != template.m_sRequiredTownModifier)
		{
			SetFailure(string.Format("CloneModule() dropped m_sRequiredTownModifier - expected '%1', got '%2'. An empty modifier DISABLES the world-fact conjunct, so the gate fires on its own entry tick for any town already under the threshold and the entire harassment phase is skipped",
				template.m_sRequiredTownModifier, clone.m_sRequiredTownModifier));
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The progress-at-least condition clones both of its attributes.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveModules_ProgressAtLeastCloneCarriesEveryAttribute : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ProgressAtLeastObjectiveCondition template = new OVT_ProgressAtLeastObjectiveCondition();

		template.m_sModuleName = "Progress Clone Fixture";
		template.m_sBagKey = "fixture.bagkey";
		template.m_iRequired = 9;

		OVT_ProgressAtLeastObjectiveCondition clone = OVT_ProgressAtLeastObjectiveCondition.Cast(template.CloneModule());
		if (!clone)
		{
			SetFailure("OVT_ProgressAtLeastObjectiveCondition.CloneModule() did not return an instance of its own class");
			return true;
		}

		if (clone.m_sModuleName != template.m_sModuleName)
		{
			SetFailure("CloneModule() dropped m_sModuleName");
			return true;
		}

		if (clone.m_sBagKey != template.m_sBagKey)
		{
			SetFailure(string.Format("CloneModule() dropped m_sBagKey - expected '%1', got '%2'. The bag answers zero for the empty key forever, so the gate never opens and the objective is abandoned every time",
				template.m_sBagKey, clone.m_sBagKey));
			return true;
		}

		if (clone.m_iRequired != template.m_iRequired)
		{
			SetFailure(string.Format("CloneModule() dropped m_iRequired - expected %1, got %2. A clone reading 0 is floored to ONE, so the phase advances on the first completed mission whatever the author demanded",
				template.m_iRequired.ToString(), clone.m_iRequired.ToString()));
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The target-kind condition clones its one attribute.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveModules_TargetKindIsCloneCarriesEveryAttribute : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TargetKindIsObjectiveCondition template = new OVT_TargetKindIsObjectiveCondition();

		template.m_sModuleName = "Kind Clone Fixture";
		template.m_iRequiredKind = OVT_EObjectiveKind.BASE;

		OVT_TargetKindIsObjectiveCondition clone = OVT_TargetKindIsObjectiveCondition.Cast(template.CloneModule());
		if (!clone)
		{
			SetFailure("OVT_TargetKindIsObjectiveCondition.CloneModule() did not return an instance of its own class");
			return true;
		}

		if (clone.m_sModuleName != template.m_sModuleName)
		{
			SetFailure("CloneModule() dropped m_sModuleName");
			return true;
		}

		if (clone.m_iRequiredKind != template.m_iRequiredKind)
		{
			SetFailure(string.Format("CloneModule() dropped m_iRequiredKind - expected %1, got %2. A clone reading 0 (NONE) matches nothing, so the phase it gates can never advance and every objective that reaches it is abandoned and blacklisted",
				template.m_iRequiredKind.ToString(), clone.m_iRequiredKind.ToString()));
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The idle abort clones all three of its attributes, including the blacklist decision.
//!
//! ⚠ THE BLACKLIST FLAG IS THE ONE WITH THE CAMPAIGN-LEVEL CONSEQUENCE. A clone that came back false
//! makes a place the machine has just failed at immediately eligible again, so selection picks it,
//! fails the same way, and the campaign churns on one town for the rest of the session.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveModules_IdleForAbortCloneCarriesEveryAttribute : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_IdleForObjectiveAbort template = new OVT_IdleForObjectiveAbort();

		template.m_sModuleName = "Idle Clone Fixture";
		template.m_sPhaseWork = "fixture work";
		template.m_sGoalNotReached = "the fixture goal";
		template.m_bBlacklist = true;

		OVT_IdleForObjectiveAbort clone = OVT_IdleForObjectiveAbort.Cast(template.CloneModule());
		if (!clone)
		{
			SetFailure("OVT_IdleForObjectiveAbort.CloneModule() did not return an instance of its own class");
			return true;
		}

		if (clone.m_sModuleName != template.m_sModuleName)
		{
			SetFailure("CloneModule() dropped m_sModuleName");
			return true;
		}

		if (clone.m_sPhaseWork != template.m_sPhaseWork)
		{
			SetFailure(string.Format("CloneModule() dropped m_sPhaseWork - expected '%1', got '%2'. The campaign log gets a half-written sentence for the one event a server owner most needs explained",
				template.m_sPhaseWork, clone.m_sPhaseWork));
			return true;
		}

		if (clone.m_sGoalNotReached != template.m_sGoalNotReached)
		{
			SetFailure(string.Format("CloneModule() dropped m_sGoalNotReached - expected '%1', got '%2'",
				template.m_sGoalNotReached, clone.m_sGoalNotReached));
			return true;
		}

		if (clone.m_bBlacklist != template.m_bBlacklist)
		{
			SetFailure("CloneModule() dropped m_bBlacklist. A place the machine has just failed at is immediately eligible again, so selection picks it, fails the same way, and the campaign churns on one place");
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! THE LADDER RUNG IS THE BAG COUNTER, AND IT SATURATES AT THE TOP RATHER THAN RUNNING OFF THE END.
//!
//! The arithmetic is OVT_ObjectivePhaseRules.HarassmentLadderIndex()'s and is pinned in the Logic
//! tier; what is asserted here is the WIRING - that the module reads the authored progress key out of
//! the objective's bag, indexes its own authored ladder with it, and answers a config name.
//!
//! ⚠ IT DRIVES A HAND-BUILT MODULE BOUND TO A LIVE OBJECTIVE, NOT THE SHIPPED ONE, so it cannot
//! disturb the campaign's own doctrine and cannot pass by reading a value the .conf happened to
//! author. The rung names are fixtures for the same reason.
//!
//! ⚠ IT NEVER CALLS TryAct(). That would create a real deployment and debit the occupying faction's
//! real pool in the shared initialisation world.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveModules_LadderRungFollowsTheBagAndSaturates : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();

		if (!director || !towns)
		{
			SetFailure("The objective director or the town manager did not resolve");
			return true;
		}

		if (!towns.m_Towns || towns.m_Towns.IsEmpty())
		{
			SetFailure("The world produced no town to hang a fixture objective on");
			return true;
		}

		director.CommitObjective(OVT_EObjectiveKind.TOWN, towns.m_Towns[0].location, "ladder fixture");

		// ⚠ HIGH ON PURPOSE. CommitObjective() enters a phase, which arms the cadence to ZERO, so a tick
		// taken from here would reach the spender and buy a real deployment in the shared world. This
		// case takes no tick at all, and the plant is the second line of defence.
		director.SetOperationCountdown(9999);

		OVT_ObjectiveInstance instance = director.GetObjectiveInstance(0);
		if (!instance)
		{
			director.ResetObjective("initialisation-tier ladder fixture torn down", false);
			SetFailure("committing an objective produced no instance to bind a module to");
			return true;
		}

		OVT_SendDeploymentObjectiveOperation module = new OVT_SendDeploymentObjectiveOperation();
		module.m_sModuleName = "ladder fixture";
		module.m_sConfigName = "";
		module.m_sLadderProgressKey = "fixture.ladder";
		module.m_aLadder = new array<string>();
		module.m_aLadder.Insert("rung zero");
		module.m_aLadder.Insert("rung one");
		module.m_aLadder.Insert("rung two");

		module.Initialize(instance);

		string failure = CheckRungs(director, module);

		module.Exit();
		director.ResetObjective("initialisation-tier ladder fixture torn down", false);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective harassment ladder: the rung follows the authored bag counter one for one and saturates at the top rung rather than wrapping or running off the end");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] director The director, for the bag.
	//! \param[in] module The bound fixture module.
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckRungs(notnull OVT_ObjectiveDirectorComponent director, notnull OVT_SendDeploymentObjectiveOperation module)
	{
		director.SetObjectiveBagValue("fixture.ladder", 0);
		if (module.ResolveConfigName(director.GetObjectiveInstance(0)) != "rung zero")
			return string.Format("with no completed operations the ladder must buy its bottom rung: got '%1'", module.ResolveConfigName(director.GetObjectiveInstance(0)));

		director.SetObjectiveBagValue("fixture.ladder", 1);
		if (module.ResolveConfigName(director.GetObjectiveInstance(0)) != "rung one")
			return string.Format("one completed operation must buy one rung: got '%1'", module.ResolveConfigName(director.GetObjectiveInstance(0)));

		director.SetObjectiveBagValue("fixture.ladder", 2);
		if (module.ResolveConfigName(director.GetObjectiveInstance(0)) != "rung two")
			return string.Format("two completed operations must buy the top rung of a three-rung ladder: got '%1'", module.ResolveConfigName(director.GetObjectiveInstance(0)));

		// 🔴 THE SATURATION. Wrapping would restart a long-running objective with a two-man patrol;
		// running off the end would index past the authored list.
		director.SetObjectiveBagValue("fixture.ladder", 47);
		if (module.ResolveConfigName(director.GetObjectiveInstance(0)) != "rung two")
			return string.Format("the ladder must SATURATE at its top rung rather than wrapping or running off the end: 47 completed operations answered '%1'", module.ResolveConfigName(director.GetObjectiveInstance(0)));

		// A module with no ladder falls back to its single authored config, which is what tower
		// recapture and sabotage are.
		OVT_SendDeploymentObjectiveOperation single = new OVT_SendDeploymentObjectiveOperation();
		single.m_sConfigName = "the one config";
		single.m_aLadder = new array<string>();

		if (single.ResolveConfigName(director.GetObjectiveInstance(0)) != "the one config")
			return string.Format("a module with no ladder must send its single authored config: got '%1'", single.ResolveConfigName(director.GetObjectiveInstance(0)));

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! THE TOWER RESOLVER SKIPS TOWERS THE ASKING FACTION ALREADY HOLDS, AND OFFERS THE REST IN ORDER.
//!
//! ⚠ THE SKIP IS ASSERTED BY ASKING TWICE, FROM BOTH SIDES, WHICH IS THE ONLY WAY TO DO IT WITHOUT
//! TAKING A TOWER OFF SOMEBODY IN THE SHARED WORLD. Resolving for the occupying faction must answer
//! every tower the occupying faction does NOT hold; resolving for the resistance must answer every
//! tower the resistance does not hold; the two answers must be DISJOINT and together must account for
//! every tower affecting the objective. A resolver that had lost its skip would answer the same full
//! list both times, and the disjointness claim catches it.
//!
//! ⚠ AND "MANY, IN ORDER" IS THE CLAIM THE SEAM EXISTS FOR. The caller walks the answers and creates
//! at the first that does not already carry a team, so a resolver that answered only its best tower
//! would send NOTHING at an objective covered by two towers once the first had one. The order claim is
//! asserted against the campaign's own list order.
//!
//! ⚠ IT RESOLVES ONLY. Resolve() creates nothing, spends nothing and writes nothing.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveModules_TowerResolverSkipsOursAndOffersTheRest : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();

		if (!director || !occupying || !config || !towns)
		{
			SetFailure("The director, the occupying faction manager, the campaign config or the town manager did not resolve");
			return true;
		}

		if (!towns.m_Towns || towns.m_Towns.IsEmpty())
		{
			SetFailure("The world produced no town to hang a fixture objective on");
			return true;
		}

		vector fixturePosition = towns.m_Towns[0].location;

		array<OVT_RadioTowerData> affecting = occupying.GetRadioTowersAffecting(fixturePosition);
		if (!affecting || affecting.IsEmpty())
		{
			SetFailure("no radio tower affects the fixture town, so the resolver has nothing to skip and nothing to offer - this world cannot exercise the claim");
			return true;
		}

		director.CommitObjective(OVT_EObjectiveKind.TOWN, fixturePosition, "tower resolver fixture");
		director.SetOperationCountdown(9999);

		OVT_ObjectiveInstance instance = director.GetObjectiveInstance(0);
		if (!instance)
		{
			director.ResetObjective("initialisation-tier tower resolver fixture torn down", false);
			SetFailure("committing an objective produced no instance to resolve against");
			return true;
		}

		OVT_EnemyTowersAffectingTargetResolver resolver = new OVT_EnemyTowersAffectingTargetResolver();

		array<vector> forOccupying = new array<vector>();
		array<vector> forResistance = new array<vector>();

		resolver.Resolve(instance, config.GetOccupyingFactionIndex(), forOccupying);
		resolver.Resolve(instance, config.GetPlayerFactionIndex(), forResistance);

		string failure = CheckAnswers(affecting, forOccupying, forResistance);

		director.ResetObjective("initialisation-tier tower resolver fixture torn down", false);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective tower resolver: it answers every tower the asking faction does NOT hold, in the campaign's own list order, and the two factions' answers partition the towers affecting the objective");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] affecting Every tower affecting the objective, in the campaign's list order.
	//! \param[in] forOccupying What the resolver answered the occupying faction.
	//! \param[in] forResistance What it answered the resistance.
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckAnswers(notnull array<OVT_RadioTowerData> affecting, notnull array<vector> forOccupying, notnull array<vector> forResistance)
	{
		// --- THE SKIP. Neither answer may contain a position the other one does.
		foreach (vector mine : forOccupying)
		{
			if (forResistance.Find(mine) != -1)
				return string.Format("the resolver offered the tower at %1 to BOTH factions, so it is not skipping the towers the asking faction already holds - a recapture team would be sent to take a tower we already own",
					mine.ToString());
		}

		// --- NOTHING INVENTED, AND NOTHING LOST. Together the two answers must be exactly the set of
		//     towers affecting the objective.
		if (forOccupying.Count() + forResistance.Count() != affecting.Count())
			return string.Format("the two answers account for %1 tower(s) and %2 affect the objective. The resolver must skip only the asking faction's own towers - it may not invent one and it may not drop one",
				(forOccupying.Count() + forResistance.Count()).ToString(), affecting.Count().ToString());

		// --- THE ORDER. The campaign's own list order is the preference order the caller walks.
		int expected = 0;
		foreach (OVT_RadioTowerData tower : affecting)
		{
			if (!tower)
				continue;

			if (expected >= forOccupying.Count())
				break;

			if (forOccupying.Find(tower.location) == -1)
				continue;

			if (forOccupying[expected] != tower.location)
				return string.Format("the resolver answered tower %1 out of order: expected %2, got %3. The caller walks these in preference order and creates at the first that is free, so the order IS which tower gets worked on",
					expected.ToString(), tower.location.ToString(), forOccupying[expected].ToString());

			expected++;
		}

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! 🔴 THE TOWN GATE REFUSES WITHOUT THIS RAMP'S OWN DEBUFF AND PASSES WITH IT.
//!
//! THE SINGLE MOST IMPORTANT ASSERTION IN THIS PHASE. The hard-coded forward-base gate's town half had TWO
//! conjuncts and the plan's diagram only drew one; wired to the diagram, the gate fires on the FIRST
//! tick of the phase for any town already under its support threshold and the entire harassment ramp
//! is skipped. Nothing errors and nothing warns - the only symptom is a forward base going up before a
//! single harassment group has been sent.
//!
//! HOW IT IS DRIVEN. Both halves are arranged on a real town and both are put back:
//!   - the town's SUPPORT is driven below the threshold, so the support half is unambiguously
//!     satisfied and cannot be what refuses;
//!   - the modifier is ABSENT first (the gate must refuse) and PRESENT second (it must pass).
//! Arranging support and the modifier list directly is the same gesture the counter-attack gate case
//! already makes with a base's faction, and every field touched is restored before anything is
//! asserted.
//!
//! ⚠ THE FIXTURE MODIFIER IS INSERTED AND REMOVED BY THIS CASE. It is a server-side record on a shared
//! world, so the removal is unconditional and happens before the first SetFailure can return.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveModules_TownGateNeedsTheRampsOwnDebuff : SCR_AutotestCaseBase
{
	//! Support to plant on the fixture town: comfortably under the ported 50 % forward-base threshold.
	static const int PLANTED_SUPPORT_PERCENT = 10;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();

		if (!director || !towns)
		{
			SetFailure("The director or the town manager did not resolve");
			return true;
		}

		if (!towns.m_Towns || towns.m_Towns.IsEmpty())
		{
			SetFailure("The world produced no town to hang a fixture objective on");
			return true;
		}

		OVT_TownModifierSystem system = towns.GetModifierSystem(OVT_TownSupportModifierSystem);
		if (!system)
		{
			SetFailure("The town support modifier system did not resolve, so the causal half of the gate cannot be arranged");
			return true;
		}

		int modifierIndex = system.GetModifierIndexByName(OVT_ObjectiveDirectorComponent.HARASSMENT_MODIFIER);
		if (modifierIndex < 0)
		{
			SetFailure(string.Format("'%1' does not resolve to a modifier index, so nothing in the campaign could ever open this gate. Check Configs/Modifiers/supportModifiers.conf",
				OVT_ObjectiveDirectorComponent.HARASSMENT_MODIFIER));
			return true;
		}

		OVT_TownData town = towns.m_Towns[0];
		if (!town.supportModifiers)
		{
			SetFailure("the fixture town carries no support-modifier list at all");
			return true;
		}

		int savedSupport = town.support;
		int savedPopulation = town.population;

		// A town with no population answers 0 % whatever its support is, which would make the support
		// half true for a reason this case is not about.
		if (town.population < 10)
			town.population = 100;

		town.support = (town.population * PLANTED_SUPPORT_PERCENT) / 100;

		director.CommitObjective(OVT_EObjectiveKind.TOWN, town.location, "town gate fixture");
		director.SetOperationCountdown(9999);

		string failure = RunHalves(director, town, modifierIndex);

		// --- RESTORE, BEFORE ASSERTING, ON EVERY PATH.
		StripFixtureModifier(town, modifierIndex);
		town.support = savedSupport;
		town.population = savedPopulation;
		director.ResetObjective("initialisation-tier town gate fixture torn down", false);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective town gate: with support driven under the threshold it still REFUSES until the town is carrying this ramp's own debuff, and passes the moment it is - being already unpopular is not the same as having been harassed");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] director The director, for the live instance.
	//! \param[in] town The fixture town, with its support already planted.
	//! \param[in] modifierIndex The harassment modifier's index.
	//! \return An empty string when every claim held, or the first that did not.
	protected string RunHalves(notnull OVT_ObjectiveDirectorComponent director, notnull OVT_TownData town, int modifierIndex)
	{
		OVT_ObjectiveInstance instance = director.GetObjectiveInstance(0);
		if (!instance)
			return "committing an objective produced no instance to bind the condition to";

		OVT_SupportBelowObjectiveCondition condition = new OVT_SupportBelowObjectiveCondition();
		condition.m_sModuleName = "town gate fixture";
		condition.m_iSupportThreshold = OVT_ObjectivePlanRules.USE_DIFFICULTY;
		condition.m_sRequiredTownModifier = OVT_ObjectiveDirectorComponent.HARASSMENT_MODIFIER;
		condition.Initialize(instance);

		// --- PRECONDITION: the support half must be unambiguously satisfied, or "it refused" below
		//     would prove nothing about the conjunct.
		if (!condition.MeetsThreshold(town.SupportPercentage()))
			return string.Format("the fixture town reads %1 %% support, which does not clear the forward-base threshold - the refusal below would be the SUPPORT half rather than the conjunct this case is about",
				town.SupportPercentage().ToString());

		// --- PRECONDITION: and the ramp must not already have debuffed it.
		if (CarriesFixtureModifier(town, modifierIndex))
			return "the fixture town is already carrying this ramp's debuff before the case has done anything, so the refusal half cannot be arranged";

		// --- HALF ONE: soft, but nobody softened it. The gate must REFUSE.
		if (condition.Evaluate())
			return "a town that is already under the threshold, carrying NONE of this ramp's own debuff, must NOT open the forward-base gate. Without the causal conjunct the gate fires on the phase's own entry tick and the entire harassment ramp is skipped - the prize for a collapsed town is being CHOSEN, not being allowed to skip a phase";

		// --- HALF TWO: this ramp did it. The gate must PASS.
		OVT_TownModifierData planted = new OVT_TownModifierData();
		planted.id = modifierIndex;
		planted.timer = 999;
		town.supportModifiers.Insert(planted);

		bool passedWithTheDebuff = condition.Evaluate();

		condition.Exit();

		if (!passedWithTheDebuff)
			return "a town under the threshold that IS carrying this ramp's debuff must open the forward-base gate. Both conjuncts are satisfied and refusing here would wedge the town doctrine at its first phase forever";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] town The fixture town.
	//! \param[in] modifierIndex The harassment modifier's index.
	//! \return True when the town carries at least one stack.
	protected bool CarriesFixtureModifier(notnull OVT_TownData town, int modifierIndex)
	{
		foreach (OVT_TownModifierData modifier : town.supportModifiers)
		{
			if (modifier && modifier.id == modifierIndex)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes every stack of the fixture modifier back off the town.
	//!
	//! ⚠ DOWNWARD, because removing shifts every later entry, and unconditional, because a red half
	//! must not leave a debuff on a shared world's town for every case that follows.
	//! \param[in] town The fixture town.
	//! \param[in] modifierIndex The harassment modifier's index.
	protected void StripFixtureModifier(notnull OVT_TownData town, int modifierIndex)
	{
		if (!town.supportModifiers)
			return;

		for (int i = town.supportModifiers.Count() - 1; i >= 0; i--)
		{
			OVT_TownModifierData modifier = town.supportModifiers[i];
			if (modifier && modifier.id == modifierIndex)
				town.supportModifiers.RemoveOrdered(i);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! A REFUSAL IS SAID ONCE, NOT ONCE PER IN-GAME MINUTE.
//!
//! The phase machine retries a refused operation every tick forever - that retry is what makes an
//! affordability hold cover a whole poverty spell rather than one tick in forty-five - so a refusal
//! that spoke every time would print a line per in-game minute for as long as the faction stayed
//! broke, and a log that says the same thing sixty times an hour is a log nobody reads. The dedup is
//! keyed on the PAIR (config, reason), so a different operation refused for the same reason still
//! speaks and the same operation refused for a NEW reason speaks again.
//!
//! ⚠ IT IS ASSERTED AS A DELTA, NEVER AS AN ABSOLUTE. The refusal ledger is not cleared when an
//! objective ends - only a successful create forgets that config's entries - so an earlier case in
//! this shared world may legitimately have latched the very refusal this one provokes. What is
//! asserted is that the SECOND broke tick adds nothing to whatever was there after the first.
//!
//! ⚠ AND THE FIRST TICK IS PROVED TO HAVE REACHED THE SPENDER, or the delta claim would be vacuous.
//! The reserve floor is pushed by exactly one thing - an affordability refusal inside
//! CanSendObjectiveDeployment() - so a floor standing after the first tick is proof that the operation
//! module asked, was refused on cost, and latched.
//!
//! ⚠ THE POOL IS EMPTIED SO THAT NOTHING CAN BE BOUGHT, and put back on every path before anything is
//! asserted. The live resource map is written directly rather than through the credit and debit
//! methods, which are the campaign's own accounting and are not a fixture's to call - the same gesture
//! OVT_TEST_Init_ObjectiveReserve makes.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveModules_RefusalIsLoggedOnceNotEveryTick : SCR_AutotestCaseBase
{
	//! Planted idle clock. High, so nothing here can time the fixture out.
	static const int PLANTED_PHASE_TICKS = 500;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();

		if (!director || !deployments || !config || !towns)
		{
			SetFailure("The director, the deployment framework, the campaign config or the town manager did not resolve");
			return true;
		}

		if (!towns.m_Towns || towns.m_Towns.IsEmpty())
		{
			SetFailure("The world produced no town to hang a fixture objective on");
			return true;
		}

		int occupyingIndex = config.GetOccupyingFactionIndex();
		if (occupyingIndex < 0)
		{
			SetFailure("The occupying faction does not resolve to a faction index, so its pool cannot be emptied");
			return true;
		}

		// PRECONDITION: the ramp has to be ALLOWED an operation, or the spender refuses on the
		// concurrency cap before it ever reaches the pool and the "it asked and was refused on cost"
		// claim below would be an accident. Every shipped preset and the attribute default author 1 or
		// more. (The same precondition OVT_TEST_Init_ObjectiveDirector's affordability case makes.)
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (!difficulty || difficulty.objectiveHarassmentMaxConcurrent < 1)
		{
			SetFailure("the difficulty preset allows fewer than one concurrent objective operation, so the ramp refuses before it reaches the pool and no refusal for want of resources could be provoked");
			return true;
		}

		// PRECONDITION: and it has to COST something, or an empty pool would not refuse it.
		OVT_DeploymentConfig rung = deployments.FindConfigByName(OVT_ObjectiveDirectorComponent.HARASSMENT_LADDER[0]);
		if (!rung || rung.GetTotalResourceCost() <= 0)
		{
			SetFailure(string.Format("'%1' is unregistered or free, so an empty pool would not refuse it and this case would prove nothing", OVT_ObjectiveDirectorComponent.HARASSMENT_LADDER[0]));
			return true;
		}

		map<int, int> pools = deployments.GetAllFactionResources();
		if (!pools)
		{
			SetFailure("The deployment framework has no resource map, so the pool could not be emptied and nothing would be refused");
			return true;
		}

		int savedPool = pools.Get(occupyingIndex);
		pools.Set(occupyingIndex, 0);

		director.CommitObjective(OVT_EObjectiveKind.TOWN, towns.m_Towns[0].location, "refusal fixture");
		director.SetPhaseTimeout(PLANTED_PHASE_TICKS);
		director.SetOperationCountdown(0);

		director.DirectorTick();

		bool reachedTheSpender = deployments.GetObjectiveReserve(occupyingIndex) != null;
		int afterFirstTick = director.GetLoggedRefusalCount();

		director.SetOperationCountdown(0);
		director.DirectorTick();

		int afterSecondTick = director.GetLoggedRefusalCount();

		// --- RESTORE, BEFORE ASSERTING, ON EVERY PATH.
		pools.Set(occupyingIndex, savedPool);
		director.ResetObjective("initialisation-tier refusal fixture torn down", false);
		deployments.ClearObjectiveReserve(occupyingIndex);

		if (!reachedTheSpender)
		{
			SetFailure("a broke tick with the cadence elapsed must reach the spender and be refused on cost - no reserve floor was pushed, so the delta claim below would be about a tick that never asked for anything");
			return true;
		}

		if (afterFirstTick < 1)
		{
			SetFailure("the refusal ledger is empty after a tick that was demonstrably refused on cost - a refused create that says nothing is a director that appears to have stopped, which is the whole reason this ledger exists");
			return true;
		}

		if (afterSecondTick != afterFirstTick)
		{
			SetFailure(string.Format("a second identical refusal must be SILENT: the ledger held %1 entries after the first broke tick and %2 after the second. The machine retries every in-game minute forever, so a refusal that spoke each time would print a line a minute for as long as the faction stayed broke",
				afterFirstTick.ToString(), afterSecondTick.ToString()));
			return true;
		}

		Print("Objective refusal log: a refused operation says why once, and the identical refusal on the next in-game minute is silent");

		return true;
	}
}
