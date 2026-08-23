//------------------------------------------------------------------------------------------------
//! TIER B - PHASE 2 OF THE RAMP: the occupying faction's forward operating base, where it meets
//! loaded configs and a live deployment framework.
//!
//! 🔴 THE HEADLINE CLAIM IS THAT A CAMPAIGN DOES NOT GROW A FORWARD BASE PER LOAD. The structure is a
//! persistence-tracked world entity: vanilla persistence puts it back before any deployment ticks, and
//! the director's own serializer brings back the record of it. A restored deployment that raised one
//! anyway would leave a second flagpole in a field on every single load, in a slightly different place
//! each time because the site is re-sampled, every one of them persisted, findable and dismantleable.
//! That is R2 from virtualization/base-defense-migration restated for a far more visible object, and
//! it is why OVT_FOBRaiseSpawningDeploymentModule.DecideRaise() is a pure static rather than four ifs
//! inside a method that spawns things.
//!
//! WHAT ELSE FAILS SILENTLY HERE, and each one is a case below:
//!   1. THE CONFIG NAMES. OVT_ObjectiveDirectorComponent.FOB_CONFIG and FOB_GARRISON_CONFIG are matched
//!      by string against the registry - to resolve a config, to count live garrisons, to sweep the
//!      teardown and to re-link a restored save. Renamed in one place and not the other, the whole
//!      middle phase does nothing and times out with one WARNING line per in-game minute.
//!   2. THE PHASE SPAN. Both configs author OVT_ObjectiveConditionDeploymentModule scoped to phase 2
//!      AND to phase 2 only. Authored as 1 they are collected the instant the ramp advances into the
//!      phase they belong to - the forward base would be raised and then immediately taken away.
//!      Authored to span 3 as well, the base's garrison would still be standing while the
//!      counter-attack controller spawns its own siege waves at the same place. ⚠ The upper bound
//!      matters here in the OPPOSITE direction to the Phase 1 ramp, whose configs must span 1 -> 2 for
//!      the counter-attack to be reachable at all (the 2026-08-19 deadlock; pinned by
//!      OVT_TEST_Init_ObjectiveOperations' phase-range case).
//!   3. THE SOURCE PROVIDER. The garrison is what makes the forward base a real supply source rather
//!      than scenery, and it does that through OVT_ObjectiveAnchorSourceProvider. Authored with the
//!      default provider instead, every garrison truck still drives from the rear and the base changes
//!      nothing about how the faction fights.
//!   4. THE TEARDOWN. Three exits share one path; a deployment left standing at a torn-down base is a
//!      force with no orders and a marker nothing will ever collect.
//!   5. THE CLONE. CloneModule is not chained and copies by hand - twenty-six lines here, thirteen
//!      inherited from the infantry module, eleven from the insertion module and two of its own. A
//!      dropped line ships the class default on every deployment, forever.
//!
//! ⚠ NOTHING HERE RAISES A STRUCTURE, AND NOTHING MAY. A real raise puts a persisted flagpole into the
//! shared initialisation world - the one thing in this suite that would still be there on the next run.
//! The raise DECISION is driven through its pure static; the two cases that need real deployments
//! create them inert (SetSpawnedUnitsEliminated on the deployment AND every spawning module,
//! immediately) and delete them on every path, including the red ones.
//!
//! ⚠ CASE ORDER: cases run alphabetically by class name. The two that mutate the live director
//! (D and F) both restore it by resetting the objective, which is the machine's own choke point, so
//! the order between them is still free. The names are prefixed A/C/D/F/J for readability of a run log.
//!
//! No polling, no waiting, no maxAttempts.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Both forward-base configs are registered, valid, correctly ordered, scoped to the right phase, and
//! field groups both shipped factions have.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   A1. The registry entry for Deployment_ObjectiveFOB.conf deleted from overthrowDeployments.conf.
//!       Fails on "'Objective Forward Base' is not registered".
//!   A2. m_sFromPhase changed from "ForwardBase" to "Harassment" on the forward-base config's objective
//!       condition. Fails on "must be scoped to phase 'ForwardBase'".
//!   A6. m_sThroughPhase changed from "ForwardBase" to "CounterAttack" on the garrison config's
//!       objective condition. Fails on "must span up to phase 'ForwardBase' and spans up to
//!       'CounterAttack'".
//!   A3. The reinforcement module moved ABOVE the raise module in Deployment_ObjectiveFOB.conf. Fails
//!       on "authors its spawning module AFTER the reinforcement module".
//!   A4. m_Source on the garrison config swapped from OVT_ObjectiveAnchorSourceProvider to
//!       OVT_NearestControlledBaseSourceProvider. Fails on "must be sourced from the forward base".
//!   A5. m_rFOBPrefab emptied on the raise module. Fails on "authors no structure prefab".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFOB_AConfigsResolveAndAreScoped : SCR_AutotestCaseBase
{
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

		string failure = CheckForwardBase(deployments);
		if (failure == "")
			failure = CheckGarrison(deployments);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective forward base: both configs are registered, valid, scoped to the forward-base phase, ordered spawning-before-reinforcement, and field groups both shipped factions have");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] deployments The deployment framework.
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckForwardBase(notnull OVT_DeploymentManagerComponent deployments)
	{
		string name = OVT_ObjectiveDirectorComponent.FOB_CONFIG;

		OVT_DeploymentConfig config = deployments.FindConfigByName(name);
		if (!config)
			return string.Format("'%1' is not registered in overthrowDeployments.conf - the forward-base phase would spend nothing, build nothing and time out", name);

		if (!config.IsValidConfig())
			return "the forward-base config does not validate";

		string ordering = CheckSpawningBeforeReinforcement(config);
		if (ordering != "")
			return ordering;

		string phase = CheckObjectivePhase(config, "ForwardBase", "ForwardBase");
		if (phase != "")
			return phase;

		OVT_FOBRaiseSpawningDeploymentModule raise = FindRaiseModule(config);
		if (!raise)
			return "the forward-base config has no OVT_FOBRaiseSpawningDeploymentModule - it would drive a truck out and build nothing";

		if (raise.GetFOBPrefab().IsEmpty())
			return "the forward-base config's raise module authors no structure prefab, so the phase spends resources and puts nothing in the world";

		if (!Resource.Load(raise.GetFOBPrefab()))
			return string.Format("the forward-base config's raise module names a structure prefab that does not load: %1", raise.GetFOBPrefab());

		if (raise.m_fRaiseOnFootRadius <= 0)
			return "the raise module's on-foot radius is not positive - the four insertion failures that divert onto foot would each leave the phase to time out with nothing built";

		if (!raise.m_Source)
			return "the forward-base config's raise module authors no source provider, so it registers nothing at all";

		return CheckGroupResolves(raise.m_sGroupType, name);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] deployments The deployment framework.
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckGarrison(notnull OVT_DeploymentManagerComponent deployments)
	{
		string name = OVT_ObjectiveDirectorComponent.FOB_GARRISON_CONFIG;

		OVT_DeploymentConfig config = deployments.FindConfigByName(name);
		if (!config)
			return string.Format("'%1' is not registered in overthrowDeployments.conf - a forward base would stand with only the party that raised it", name);

		if (!config.IsValidConfig())
			return "the forward-base garrison config does not validate";

		string ordering = CheckSpawningBeforeReinforcement(config);
		if (ordering != "")
			return ordering;

		string phase = CheckObjectivePhase(config, "ForwardBase", "ForwardBase");
		if (phase != "")
			return phase;

		OVT_InsertionSpawningDeploymentModule insertion = FindInsertionModule(config);
		if (!insertion)
			return "the garrison config has no insertion spawning module - its force would appear at the forward base out of thin air";

		// 🔴 THE ONE LINE THAT MAKES THE FORWARD BASE A SUPPLY SOURCE RATHER THAN SCENERY.
		if (!OVT_ObjectiveAnchorSourceProvider.Cast(insertion.m_Source))
			return "the garrison config must be sourced from the forward base (OVT_ObjectiveAnchorSourceProvider) - with any other provider every garrison truck still drives from the rear and raising a forward base changes nothing about how the faction fights";

		// ⚠ A PATROL BEHAVIOUR MODULE WOULD STRAND THE GARRISON. DEFEND anchors on where a group is
		// REGISTERED, and an insertion registers its force at the SOURCE - so a DEFEND plan would park
		// the forward base's garrison at the base it set out from, permanently. The insertion module's
		// own fallback march is what walks them onto the forward base and holds them there.
		foreach (OVT_BaseDeploymentModule module : config.m_aModules)
		{
			if (OVT_PatrolBehaviorDeploymentModule.Cast(module))
				return "the garrison config authors a patrol behaviour module - DEFEND anchors on where a group is registered, which for an insertion is the source base, so the garrison would never leave it";
		}

		return CheckGroupResolves(insertion.m_sGroupType, name);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] config The config to walk.
	//! \return An empty string when the spawning module comes first, or why it does not.
	protected string CheckSpawningBeforeReinforcement(notnull OVT_DeploymentConfig config)
	{
		int spawningIndex = -1;
		int reinforcementIndex = -1;

		foreach (int index, OVT_BaseDeploymentModule module : config.m_aModules)
		{
			if (!module)
				continue;

			if (spawningIndex == -1 && OVT_BaseSpawningDeploymentModule.Cast(module))
				spawningIndex = index;

			if (reinforcementIndex == -1 && OVT_ReinforcementBehaviorDeploymentModule.Cast(module))
				reinforcementIndex = index;
		}

		if (spawningIndex == -1)
			return string.Format("'%1' authors no spawning module at all", config.m_sDeploymentName);

		if (reinforcementIndex == -1)
			return string.Format("'%1' authors no reinforcement module, so nothing ever collects it when the objective ends", config.m_sDeploymentName);

		if (spawningIndex > reinforcementIndex)
			return string.Format("'%1' authors its spawning module AFTER the reinforcement module - update order is authored order, and the rebuy must never be asked before the force it is rebuying exists",
				config.m_sDeploymentName);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! ⚠ BOTH ENDS OF THE SPAN, since the objective condition became a RANGE on 2026-08-19. The two
	//! forward-base configs are the only objective configs that must NOT span more than one phase: the
	//! Phase 1 ramp now continues through this phase (that is the deadlock fix), while the base and its
	//! garrison belong to it and to nothing else. Asserting only the first phase would let a
	//! `m_sThroughPhase "CounterAttack"` slip through and keep the garrison standing into the battle the
	//! counter-attack controller is already spawning its own waves for.
	//!
	//! ⚠ THE SPAN IS TWO AUTHORED PHASE NAMES NOW, NOT TWO ENUM INTEGERS (build phase 4). The names are
	//! compared as literals against what the shipped plans author, deliberately: a constant naming a
	//! constant passes even when both sides are renamed together, which is the exact change that abandons
	//! every save on disk and silently unscopes every objective deployment.
	//!
	//! ⚠ THE UPPER BOUND IS READ THROUGH EffectiveThroughPhase(), NOT OFF THE FIELD, so an unauthored
	//! empty string is judged as the span it actually means rather than as the text that is written down.
	//! \param[in] config The config to walk.
	//! \param[in] required The first phase it must be scoped to, by name.
	//! \param[in] through The last phase it may span, inclusive, by name.
	//! \return An empty string when it is, or why it is not.
	protected string CheckObjectivePhase(notnull OVT_DeploymentConfig config, string required, string through)
	{
		foreach (OVT_BaseDeploymentModule module : config.m_aModules)
		{
			OVT_ObjectiveConditionDeploymentModule condition = OVT_ObjectiveConditionDeploymentModule.Cast(module);
			if (!condition)
				continue;

			if (condition.m_sFromPhase != required)
				return string.Format("'%1' must be scoped to phase '%2' and is scoped to '%3' - a config scoped to the wrong phase is collected on the tick the ramp reaches the phase it belongs to",
					config.m_sDeploymentName, required, condition.m_sFromPhase);

			if (condition.EffectiveThroughPhase() != through)
				return string.Format("'%1' must span up to phase '%2' and spans up to '%3' - the forward base and its garrison belong to their own phase alone, and a wider span keeps them standing into the counter-attack while a narrower one is a span the ramp's own fix would have to be undone to produce",
					config.m_sDeploymentName, through, condition.EffectiveThroughPhase());

			if (condition.m_fMaxDistanceFromObjective <= 0)
				return string.Format("'%1' authors no working radius around the objective, which refuses every position but its exact centre", config.m_sDeploymentName);

			return "";
		}

		return string.Format("'%1' authors no objective condition - the evaluator could create it anywhere, and nothing collects it when the objective ends", config.m_sDeploymentName);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] config The config to walk.
	//! \return Its raise module, or null.
	protected OVT_FOBRaiseSpawningDeploymentModule FindRaiseModule(notnull OVT_DeploymentConfig config)
	{
		foreach (OVT_BaseDeploymentModule module : config.m_aModules)
		{
			OVT_FOBRaiseSpawningDeploymentModule raise = OVT_FOBRaiseSpawningDeploymentModule.Cast(module);
			if (raise)
				return raise;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] config The config to walk.
	//! \return Its insertion spawning module, or null.
	protected OVT_InsertionSpawningDeploymentModule FindInsertionModule(notnull OVT_DeploymentConfig config)
	{
		foreach (OVT_BaseDeploymentModule module : config.m_aModules)
		{
			OVT_InsertionSpawningDeploymentModule insertion = OVT_InsertionSpawningDeploymentModule.Cast(module);
			if (insertion)
				return insertion;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! That BOTH shipped factions can field a group name, and that the prefab behind it loads.
	//!
	//! ⚠ BOTH FACTIONS, ALWAYS. Which faction occupies is a campaign setting, so a name authored for
	//! only one of them is a config that works on half the servers.
	//! \param[in] groupType The registry name.
	//! \param[in] configName Which config named it, for the failure text.
	//! \return An empty string when both factions field it, or why one does not.
	protected string CheckGroupResolves(string groupType, string configName)
	{
		if (groupType == "")
			return string.Format("'%1' authors an empty group type", configName);

		OVT_OverthrowFactionManager factions = OVT_Global.GetFactions();
		if (!factions)
			return "OVT_Global.GetFactions() is null - the faction registries are not loaded";

		array<string> factionKeys = {"US", "USSR"};

		foreach (string factionKey : factionKeys)
		{
			OVT_Faction faction = factions.GetOverthrowFactionByKey(factionKey);
			if (!faction)
				return string.Format("there is no Overthrow faction config for '%1', so '%2' could not be checked against it", factionKey, groupType);

			faction.InitializeGroupRegistry();

			if (!faction.HasGroupType(groupType))
				return string.Format("the %1 registry cannot field '%2' - a forward-base config names a group that faction has no entry for", factionKey, groupType);

			ResourceName prefab = faction.GetGroupPrefabByName(groupType);
			if (prefab.IsEmpty())
				return string.Format("the %1 registry entry '%2' names no prefab at all", factionKey, groupType);

			if (!Resource.Load(prefab))
				return string.Format("the %1 entry for '%2' names a prefab that does not load: %3", factionKey, groupType, prefab);
		}

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! 🔴 THE RAISE DECISION: a restored deployment raises NOTHING, a fresh one raises exactly ONCE, and
//! driving it again raises nothing more.
//!
//! THIS IS THE PHASE'S HIGHEST-CONSEQUENCE CLAIM AND THE REASON DecideRaise() IS A PURE STATIC. Every
//! other way of asserting it needs a real save, a real truck and a real persisted flagpole in the
//! shared world - and the failure it guards against only shows up on the SECOND load of a campaign,
//! which no automated tier ever reaches.
//!
//! THE FOUR GATES ARE FOUR DIFFERENT QUESTIONS AND THE CASE DRIVES THEM SEPARATELY:
//!   - no deployment: refuse, but do NOT latch, because a config template is later cloned onto a real
//!     deployment and must still be able to build;
//!   - already attempted: refuse. This is the "driven twice raises nothing more" half, and it is what
//!     stops a reinforcement rebuy - which CLEARS the eliminated flags and re-runs the convergence -
//!     from putting a second structure beside the first;
//!   - restored from a save: refuse. D11;
//!   - eliminated: refuse, but do NOT latch, because the rebuy path clears the flag and the base is
//!     then genuinely owed.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0):
//!   C1. `if (restoredFromSave) return false;` deleted from DecideRaise. Fails on "a deployment
//!       restored from a save must raise NOTHING".
//!   C2. `if (alreadyAttempted) return false;` deleted. Fails on "a module that has already had its
//!       attempt must raise nothing more".
//!   C3. `if (!hasDeployment) return false;` deleted, so a bare config template answers true. Fails on
//!       "a module with no deployment behind it must not raise".
//!   C4. The eliminated test inverted to `return eliminated;`. Fails on "a wiped-out party raises
//!       nothing".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFOB_CRaiseIsOnceAndNeverOnRestore : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// A FRESH DEPLOYMENT RAISES. Everything else is a refusal of this row.
		if (!ExpectDecision(true, false, false, false, true, "a fresh deployment whose party has arrived raises the forward base"))
			return true;

		// 🔴 D11 / R2. The whole reason this case exists.
		if (!ExpectDecision(true, false, true, false, false, "a deployment restored from a save must raise NOTHING - the structure is persistence-tracked and is already standing, so raising again grows one more forward base per load, forever"))
			return true;

		// THE "DRIVEN TWICE" HALF. The latch, not the gate.
		if (!ExpectDecision(true, true, false, false, false, "a module that has already had its attempt must raise nothing more, or a reinforcement rebuy puts a second structure beside the first"))
			return true;

		if (!ExpectDecision(true, true, true, false, false, "a restored deployment that has also already attempted stays refused"))
			return true;

		if (!ExpectDecision(false, false, false, false, false, "a module with no deployment behind it must not raise - that is a config template, and the initialisation tier holds several"))
			return true;

		if (!ExpectDecision(true, false, false, true, false, "a wiped-out party raises nothing"))
			return true;

		// THE ELIMINATED REFUSAL IS NOT A LATCH. The rebuy path clears the flag, and the structure is
		// then owed - a latch here would leave a paid-for forward base permanently unbuilt.
		if (!ExpectDecision(true, false, false, false, true, "clearing the eliminated flag lets the raise happen after all, so the refusal was not a latch"))
			return true;

		// A BARE MODULE HAS NOTHING AND CLAIMS NOTHING. ⚠ `new` does not apply [Attribute()] defvalues,
		// so this asserts only the runtime state, which is the half that is set in the constructor.
		OVT_FOBRaiseSpawningDeploymentModule bare = new OVT_FOBRaiseSpawningDeploymentModule();

		if (bare.HasAttemptedRaise())
		{
			SetFailure("a freshly constructed raise module already reports an attempt - the latch would refuse the one raise the deployment is entitled to");
			return true;
		}

		if (bare.GetStructure())
		{
			SetFailure("a freshly constructed raise module already reports a structure - the teardown would then be asked to remove something that was never built");
			return true;
		}

		Print("Objective forward base: the raise decision refuses a restored deployment outright, refuses a second attempt, refuses a template with no deployment, and refuses a wiped-out party WITHOUT latching so a rebuy can still build");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one row of the raise decision.
	//! \param[in] hasDeployment Whether there is a deployment behind the module.
	//! \param[in] alreadyAttempted Whether the one attempt has been spent.
	//! \param[in] restoredFromSave Whether the deployment came back from a save.
	//! \param[in] eliminated Whether the party is wiped out.
	//! \param[in] expected Whether a structure must go up.
	//! \param[in] label Human description of the row.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectDecision(bool hasDeployment, bool alreadyAttempted, bool restoredFromSave, bool eliminated, bool expected, string label)
	{
		bool actual = OVT_FOBRaiseSpawningDeploymentModule.DecideRaise(hasDeployment, alreadyAttempted, restoredFromSave, eliminated);

		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! The garrison's source provider prefers the forward base over the rear, and falls through rather
//! than failing when there is no forward base.
//!
//! WHY BOTH HALVES MATTER. The preference is the entire mechanical payoff of raising a forward base -
//! without it the phase costs resources, puts a flag in a field and changes nothing about where
//! trucks come from. The fall-through is what stops a garrison deployment that outlived its base by
//! one update from stranding its force: OVT_InsertionSpawningDeploymentModule refuses to register
//! anything at all when its provider answers false, and "the men never arrived" has no log line
//! anybody would connect to a forward base that came down a second earlier.
//!
//! ⚠ THIS CASE MUTATES THE LIVE DIRECTOR AND PUTS IT BACK. It records a forward base at a synthetic
//! position, asks, and then resets the objective - the machine's own choke point, which clears the
//! record, the runtime flags and the deployment bias together. The position is deliberately far from
//! anything, so the teardown's structure query finds nothing to remove.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0):
//!   D1. `if (!director.IsAssetUp(ASSET_FOB)) return false;` deleted from ResolveForwardBase, so a
//!       director with no forward base answers the zero vector. Fails on "with no forward base the
//!       provider must fall through to the nearest controlled base".
//!   D2. ResolveSource's forward-base branch removed, leaving only the fallback. Fails on "a standing
//!       forward base must be preferred over the rear".
//!   D3. The fallback removed, so the provider answers false with no forward base. Fails on "with no
//!       forward base the provider must still find an origin".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFOB_DAnchorProviderPrefersTheForwardBase : SCR_AutotestCaseBase
{
	//! Somewhere no campaign feature is, so recording a forward base here disturbs nothing and the
	//! teardown's structure query comes back empty.
	static const vector PROBE = "12000 40 12000";

	//! The pretend supply base, far enough from the probe that the two answers cannot be confused.
	static const vector SOURCE = "11000 40 11000";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		if (!director)
		{
			SetFailure("OVT_Global.GetObjectiveDirector() is null - the objective director is not on the game-mode prefab, so nothing in this phase runs at all");
			return true;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("OVT_Global.GetConfig() is null - the occupying faction index cannot be resolved");
			return true;
		}

		int occupyingIndex = config.GetOccupyingFactionIndex();

		// ⚠ `new` DOES NOT APPLY [Attribute()] DEFVALUES, so the one authored field is set explicitly.
		OVT_ObjectiveAnchorSourceProvider provider = new OVT_ObjectiveAnchorSourceProvider();
		provider.m_fMaxForwardDistance = 0;

		// ⚠ THE MANNED TEST IS OFF FOR THE PREFERENCE CLAIMS AND ON FOR ITS OWN. This fixture reports an
		// asset raised WITHOUT creating a garrison deployment at it, so with the wiped-base refusal live
		// every "the forward base wins" claim below would fall through and this case would assert nothing
		// about preference. Check() turns it back on for the one leg that is about it.
		provider.m_fGarrisonRadius = 0;

		string failure = Check(director, provider, occupyingIndex);

		// ALWAYS, INCLUDING ON THE RED PATHS. Leaving a forward base recorded on the live director would
		// change what every later case in this suite sees.
		director.ResetObjective("OVT_TEST_Init_ObjectiveFOB_D finished", false);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective forward base: the garrison's source provider answers the forward base while one is standing and manned, falls through to the nearest controlled base when none is standing, and refuses a standing base whose garrison has been wiped");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] director The live director.
	//! \param[in] provider The provider under test.
	//! \param[in] occupyingIndex The occupying faction.
	//! \return An empty string when every claim held, or the first that did not.
	protected string Check(notnull OVT_ObjectiveDirectorComponent director, notnull OVT_ObjectiveAnchorSourceProvider provider, int occupyingIndex)
	{
		if (director.IsAssetUp(OVT_ObjectiveDirectorComponent.ASSET_FOB))
			return "the director already reports a forward base before this case recorded one - some earlier case left one standing, and neither half of this claim can be trusted";

		// --- No forward base: fall through to the rear.
		vector fallback;
		bool answered = provider.ResolveSource(PROBE, occupyingIndex, fallback);

		if (!answered)
			return "with no forward base the provider must still find an origin - the occupying faction holds bases at initialisation, and a provider that answers false makes the insertion module register nothing at all";

		if (fallback == PROBE)
			return "with no forward base the provider answered the deployment's own position, which is not an origin at all";

		// --- Forward base standing: it wins.
		//
		// ⚠ AN OBJECTIVE IS COMMITTED FIRST, AND THAT IS BUILD PHASE 5'S RE-POINT. The keyed reporter
		// refuses to record an asset for a director that has no objective - a supply party can outlive
		// the objective that sent it, and a base recorded onto an idle machine would make the anchor
		// provider prefer a phantom for the rest of the campaign. So the fixture arranges the state the
		// production caller reports from.
		director.CommitObjective(OVT_EObjectiveKind.BASE, PROBE, "OVT_TEST_Init_ObjectiveFOB_D fixture");
		director.ReportAssetRaised(OVT_ObjectiveDirectorComponent.ASSET_FOB, PROBE, SOURCE, OVT_ObjectiveDirectorComponent.FOB_CONFIG);

		if (!director.IsAssetUp(OVT_ObjectiveDirectorComponent.ASSET_FOB))
			return "the keyed asset reporter did not put the director's forward base up, so the preference cannot be asked about";

		vector preferred;
		if (!provider.ResolveSource(SOURCE, occupyingIndex, preferred))
			return "with a forward base standing the provider answered nothing at all";

		if (preferred != PROBE)
			return string.Format("a standing forward base must be preferred over the rear: the provider answered %1, expected the forward base at %2. Without this the phase raises a flag in a field and every truck still drives from the same place it always did",
				preferred.ToString(), PROBE.ToString());

		// --- The per-provider distance limit still applies to the forward base itself.
		provider.m_fMaxForwardDistance = 10;

		vector limited;
		if (!provider.ResolveSource(SOURCE, occupyingIndex, limited))
			return "a forward base outside the provider's own distance limit must fall through to the rear, not answer nothing";

		if (limited == PROBE)
			return "a forward base outside the provider's own distance limit was still preferred - the limit does nothing";

		provider.m_fMaxForwardDistance = 0;

		// ==========================================================================================
		// 🔴 A WIPED FORWARD BASE IS NOT AN ORIGIN (author, 2026-08-21: *"Should a wiped FOB be an
		// insertion origin? No."*).
		// ==========================================================================================
		// The fixture is already exactly the state this rule is about: an asset the director reports as
		// UP, with no occupying deployment standing at it - a base whose garrison has been wiped. Turning
		// the manned test on must make the provider refuse it and fall through to the rear.
		//
		// ⚠ THE ASSET IS STILL UP THROUGHOUT, which is the whole point of the claim. asset.up says the
		// STRUCTURE is standing and it stays true when the men die, so a rule written against it would
		// pass this leg while doing nothing - that is the mistake this leg exists to catch.
		provider.m_fGarrisonRadius = 250;

		vector unmanned;
		if (!provider.ResolveSource(SOURCE, occupyingIndex, unmanned))
			return "a forward base with no living garrison must fall through to the rear, not answer nothing - refusing to send at all would strand every operation the objective owns";

		if (unmanned == PROBE)
			return "a forward base with NO LIVING GARRISON was still used as the origin. The only distance a force sourced there has to cover is zero, so it materialises exactly where the player who just cleared the base is standing - which is what the author watched happen on 2026-08-21";

		provider.m_fGarrisonRadius = 0;

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! The teardown leaves NO deployment of either forward-base config standing in the area.
//!
//! WHY THIS IS ASSERTED WITH REAL DEPLOYMENTS. The teardown's whole job is to find things the
//! director's own ledger does not know about - a marker restored from a save, one whose position moved
//! between the send and the raise, a garrison bought on an interval the reset interrupted. A case that
//! only checked the ledger would assert the half that was never in doubt.
//!
//! ⚠ THE FIXTURE DEPLOYMENTS ARE MADE INERT THE INSTANT THEY EXIST - SetSpawnedUnitsEliminated(true)
//! on the deployment AND on every spawning module - and are deleted on every path including the red
//! ones. Without that the raise module would drive a truck across the initialisation world and put a
//! persisted flagpole in it, which is the one thing in this suite that would survive to the next run.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0):
//!   F1. The area sweep removed from the raise module's teardown, leaving only the name-scoped carrier lookup. Fails
//!       on "the garrison deployment is still standing after the teardown".
//!   F2. The FOB_GARRISON_CONFIG arm of the sweep's name test removed. Fails on the same claim.
//!   F3. The asset-teardown call removed from ResetObjective(). Fails on "the forward-base deployment
//!       is still standing after the teardown".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFOB_FTeardownLeavesNothingStanding : SCR_AutotestCaseBase
{
	//! Far from every town, base and tower in the initialisation world, so the fixture deployments
	//! cannot be confused with anything the campaign put there.
	static const vector PROBE = "13000 40 13000";

	//! How far the assertions look for a deployment that should be gone. Wider than the teardown's own
	//! area radius would be pointless; this matches it.
	static const float LOOKUP_RADIUS = 250;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!deployments || !director || !config)
		{
			SetFailure("the deployment framework, the objective director or the campaign config did not resolve, so the teardown cannot be exercised");
			return true;
		}

		array<OVT_DeploymentComponent> created = new array<OVT_DeploymentComponent>();

		string failure = Check(deployments, director, config.GetOccupyingFactionIndex(), created);

		// ALWAYS, INCLUDING ON THE RED PATHS. Anything this case created and the teardown did not take
		// away is a deployment left in the shared world for every later case to trip over.
		Cleanup(deployments, created);
		director.ResetObjective("OVT_TEST_Init_ObjectiveFOB_F finished", false);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective forward base: one teardown takes down both the forward base's own deployment and its garrison, whether or not the director's ledger knew about them");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] deployments The deployment framework.
	//! \param[in] director The live director.
	//! \param[in] occupyingIndex The occupying faction.
	//! \param[in] created Every fixture deployment, filled BEFORE any assertion that could return.
	//! \return An empty string when every claim held, or the first that did not.
	protected string Check(notnull OVT_DeploymentManagerComponent deployments, notnull OVT_ObjectiveDirectorComponent director, int occupyingIndex, notnull array<OVT_DeploymentComponent> created)
	{
		OVT_DeploymentConfig fobConfig = deployments.FindConfigByName(OVT_ObjectiveDirectorComponent.FOB_CONFIG);
		OVT_DeploymentConfig garrisonConfig = deployments.FindConfigByName(OVT_ObjectiveDirectorComponent.FOB_GARRISON_CONFIG);

		if (!fobConfig || !garrisonConfig)
			return "one of the two forward-base configs is not registered, so the teardown has nothing to be asked about";

		OVT_DeploymentComponent fob = deployments.CreateDeployment(fobConfig, PROBE, occupyingIndex, 0, 0);
		if (fob)
		{
			created.Insert(fob);
			MakeInert(fob);
		}
		else
		{
			return "the forward-base deployment could not be created at the probe, so the teardown cannot be exercised";
		}

		OVT_DeploymentComponent garrison = deployments.CreateDeployment(garrisonConfig, PROBE, occupyingIndex, 0, 0);
		if (garrison)
		{
			created.Insert(garrison);
			MakeInert(garrison);
		}
		else
		{
			return "the garrison deployment could not be created at the probe, so the teardown cannot be exercised";
		}

		// Both are standing where the director will look for them.
		if (!deployments.GetDeploymentNearPosition(OVT_ObjectiveDirectorComponent.FOB_CONFIG, PROBE, LOOKUP_RADIUS))
			return "the forward-base fixture deployment cannot be found at the probe it was created at, so this case cannot say anything about the teardown";

		if (!deployments.GetDeploymentNearPosition(OVT_ObjectiveDirectorComponent.FOB_GARRISON_CONFIG, PROBE, LOOKUP_RADIUS))
			return "the garrison fixture deployment cannot be found at the probe it was created at";

		// The director is given an objective, put into the phase that OWNS the forward base - which is
		// what registers the asset's module, and therefore what the one teardown path reaches through -
		// told a forward base is standing there, and then put through the machine's ONE reset path.
		//
		// 🔴 ENTERING THE PHASE IS NOT SETUP DRESSING, IT IS THE CLAIM (build phase 5). The teardown is
		// the raise module's, and the raise module is only reachable because it registered itself with
		// the director on entry. A fixture that recorded a forward base onto an idle director would
		// leave nothing registered, so the reset would sweep nothing at all and this case would be
		// asserting about a machine that cannot happen.
		director.CommitObjective(OVT_EObjectiveKind.BASE, PROBE, "OVT_TEST_Init_ObjectiveFOB_F fixture");
		director.EnterPhase("ForwardBase");
		director.ReportAssetRaised(OVT_ObjectiveDirectorComponent.ASSET_FOB, PROBE, PROBE, OVT_ObjectiveDirectorComponent.FOB_CONFIG);
		director.ResetObjective("OVT_TEST_Init_ObjectiveFOB_F is exercising the teardown", false);

		if (deployments.GetDeploymentNearPosition(OVT_ObjectiveDirectorComponent.FOB_CONFIG, PROBE, LOOKUP_RADIUS))
			return "the forward-base deployment is still standing after the teardown - a marker nothing will collect and a force with no orders, for the rest of the campaign";

		if (deployments.GetDeploymentNearPosition(OVT_ObjectiveDirectorComponent.FOB_GARRISON_CONFIG, PROBE, LOOKUP_RADIUS))
			return "the garrison deployment is still standing after the teardown - the sweep must take down BOTH configs, not just the one carrying the structure";

		if (director.IsAssetUp(OVT_ObjectiveDirectorComponent.ASSET_FOB))
			return "the director still reports a forward base after the reset";

		if (director.IsFOBDeploymentSent())
			return "the director still believes a supply party is on its way after the reset, so the very next objective's first spend would be measured against a forward base that no longer exists";

		if (director.GetFOBSpent() != 0)
			return string.Format("the forward base's spend counter survived the reset at %1 - the next forward base would start its ceiling part-spent", director.GetFOBSpent().ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Makes one fixture deployment unable to register or build anything.
	//! \param[in] deployment The deployment to disarm.
	protected void MakeInert(notnull OVT_DeploymentComponent deployment)
	{
		deployment.SetSpawnedUnitsEliminated(true);

		array<OVT_BaseSpawningDeploymentModule> modules = deployment.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule module : modules)
		{
			if (module)
				module.SetSpawnedUnitsEliminated(true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes anything the teardown did not.
	//! \param[in] deployments The deployment framework.
	//! \param[in] created Every fixture deployment this case created.
	protected void Cleanup(notnull OVT_DeploymentManagerComponent deployments, notnull array<OVT_DeploymentComponent> created)
	{
		foreach (OVT_DeploymentComponent deployment : created)
		{
			if (deployment)
				MakeInert(deployment);
		}

		foreach (OVT_DeploymentComponent deployment : created)
		{
			if (deployment)
				deployments.DeleteDeployment(deployment);
		}

		created.Clear();
	}
}

//------------------------------------------------------------------------------------------------
//! THE DISMANTLE VALIDATION LADDER: what the server checks before it lets a player pull the flag
//! down, and what it says when it refuses.
//!
//! ⚠ THE SERVER AND THE PROMPT ASK THE SAME METHOD, AND THAT IS THE POINT OF ASSERTING IT HERE.
//! OVT_DismantleEnemyFOBAction.CanBePerformedScript() calls CanDismantleFOB() on the client so the
//! prompt can explain itself; OVT_CampaignRequestComponent.RpcAsk_DismantleEnemyFOB reaches
//! OnFOBDismantledByPlayer(), which calls the SAME method on the server before anything happens. A
//! rule that lived in two places would eventually disagree, and the symptom of that is "the action was
//! available and did nothing", which has no log line on the client at all.
//!
//! ⚠ EVERY REFUSAL ANSWERS A LOCALIZATION KEY, NOT AN EMPTY STRING. SetCannotPerformReason() with an
//! empty key shows the player nothing, so a refusal with no key is a held action that greys out for no
//! stated reason - the one failure mode a player reports as "the game is broken".
//!
//! ⚠ THIS CASE NEVER CALLS OnFOBDismantledByPlayer(). That method SUBTRACTS the removal penalty from
//! the live occupying pool and resets the objective; driving it here would take resources out of the
//! shared initialisation world for a forward base that was never built.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0):
//!   G1. The `if (!m_FOB.up)` guard deleted from CanDismantleFOB. Fails on "with no forward base
//!       standing the dismantle must be refused".
//!   G2. The distance test deleted. Fails on "a caller a kilometre away must be refused".
//!   G3. `refusal = "#OVT-DismantleEnemyFOB_TooFar";` blanked. Fails on "every refusal must answer a
//!       localization key".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFOB_GDismantleRefusalsAreStated : SCR_AutotestCaseBase
{
	//! Somewhere with no campaign feature and no occupying soldier near it, so the only conditions in
	//! play are the ones this case sets.
	static const vector PROBE = "14000 40 14000";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		if (!director)
		{
			SetFailure("OVT_Global.GetObjectiveDirector() is null - the dismantle rules have nothing to be asked of");
			return true;
		}

		string failure = Check(director);

		// ALWAYS, INCLUDING ON THE RED PATHS.
		director.ResetObjective("OVT_TEST_Init_ObjectiveFOB_G finished", false);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective forward base: the dismantle is refused with a stated reason when no forward base stands and when the caller is not at it, and is allowed at a cleared site");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] director The live director.
	//! \return An empty string when every claim held, or the first that did not.
	protected string Check(notnull OVT_ObjectiveDirectorComponent director)
	{
		if (director.IsAssetUp(OVT_ObjectiveDirectorComponent.ASSET_FOB))
			return "the director already reports a forward base before this case recorded one - an earlier case left one standing and none of these rows can be trusted";

		// --- ROW 1: nothing to dismantle.
		string refusal;
		if (director.CanDismantleFOB(PROBE, refusal))
			return "with no forward base standing the dismantle must be refused - the structure is a persisted world entity and can briefly outlive the campaign's record of it, which is exactly when a player would be standing at a flag that means nothing";

		if (refusal == "")
			return "the no-forward-base refusal answered no localization key, so the held action would grey out with nothing said";

		// --- ROW 2: the caller is nowhere near it.
		//
		// ⚠ AN OBJECTIVE IS COMMITTED FIRST - see the same note in the anchor-provider case. The keyed
		// reporter refuses to record an asset for a director that has no objective.
		director.CommitObjective(OVT_EObjectiveKind.BASE, PROBE, "OVT_TEST_Init_ObjectiveFOB_G fixture");
		director.ReportAssetRaised(OVT_ObjectiveDirectorComponent.ASSET_FOB, PROBE, PROBE, OVT_ObjectiveDirectorComponent.FOB_CONFIG);

		if (!director.IsAssetUp(OVT_ObjectiveDirectorComponent.ASSET_FOB))
			return "the keyed asset reporter did not put the director's forward base up, so the remaining rows cannot be asked";

		vector faraway = PROBE + Vector(1000, 0, 0);
		if (director.CanDismantleFOB(faraway, refusal))
			return "a caller a kilometre away must be refused - without the distance test the server would accept a dismantle from anywhere on the map, which is the shape of BUG-025";

		if (refusal == "")
			return "every refusal must answer a localization key, and the too-far one answered none";

		// --- ROW 3: standing at a cleared site. The probe is far from every base, town and patrol in
		// the initialisation world, so nothing of the occupying faction is alive within the clear
		// radius and the only remaining condition is satisfied.
		if (!director.CanDismantleFOB(PROBE, refusal))
			return string.Format("a player standing at a forward base with no enemy alive around it must be allowed to dismantle it - refused with '%1'. If this row is the one that broke, the defender count is finding occupying-faction agents at a probe chosen to be empty", refusal);

		if (refusal != "")
			return string.Format("an allowed dismantle still answered a refusal key ('%1'), which the action would then show as a reason not to do the thing it just did", refusal);

		// --- ROW 4: THE CLIENT'S ENTRY POINT AND THE SERVER'S AGREE. The user action cannot read the
		// director's record - none of it replicates - so it asks CanDismantleFOBAt() about the flag it
		// is attached to instead. If the two ever disagreed, the prompt and the server would disagree,
		// and the symptom is a held action that completes and does nothing, with nothing said on the
		// client at all.
		string sharedRefusal;
		if (!director.CanDismantleFOBAt(PROBE, PROBE, sharedRefusal))
			return string.Format("the rule the user action asks refused a case the server's own rule allowed ('%1') - the prompt and the server have drifted apart", sharedRefusal);

		if (director.CanDismantleFOBAt(faraway, PROBE, sharedRefusal))
			return "the rule the user action asks allowed a caller a kilometre from the flag, which the server's own rule refuses";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! The raise module's clone carries every one of its twenty-six attributes.
//!
//! ⚠ CloneModule IS NOT CHAINED. Each subclass rebuilds the whole list BY HAND, so this module repeats
//! thirteen lines from OVT_InfantrySpawningDeploymentModule, eleven from
//! OVT_InsertionSpawningDeploymentModule and adds two of its own. A dropped line does not warn, does
//! not log and does not fail to parse - it ships the CLASS DEFAULT on every deployment for the rest of
//! the campaign, which is exactly how m_fMaxCruiseSpeed was lost on the vehicle module for a release.
//!
//! WHAT A DROPPED LINE COSTS HERE: drop m_rFOBPrefab and the middle phase drives a truck across the
//! map and builds nothing; drop m_Source and the module registers nothing at all, silently; drop
//! m_fRaiseOnFootRadius and it clones as 0, disabling the walking path so that each of the four
//! insertion failures that divert onto foot becomes an objective that times out.
//!
//! ⚠ THE LATCH IS FIRED BEFORE THE CLONE so "a clone must not inherit a fired latch" cannot pass
//! vacuously - and it is fired through the ONE path that sets it, on a module with no deployment, so
//! nothing is spawned.
//!
//! ⚠ `new` DOES NOT APPLY [Attribute()] DEFVALUES, so every field is set explicitly to a value that is
//! not the class default. A case that cloned zeros would pass whatever CloneModule forgot.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0):
//!   J1. `clone.m_rFOBPrefab = m_rFOBPrefab;` deleted. Fails on "m_rFOBPrefab".
//!   J2. `clone.m_fRaiseOnFootRadius = m_fRaiseOnFootRadius;` deleted. Fails on "m_fRaiseOnFootRadius".
//!   J3. `clone.m_Source = m_Source;` deleted. Fails on "m_Source".
//!   J4. `clone.m_fLZStandoffDistance = m_fLZStandoffDistance;` deleted - one of the ten inherited
//!       insertion lines, to prove the inherited half is covered too. Fails on "m_fLZStandoffDistance".
//!   J5. `clone.m_eImportance = m_eImportance;` deleted - one of the thirteen inherited infantry lines.
//!       Fails on "m_eImportance".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFOB_JCloneCarriesEveryAttribute : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_FOBRaiseSpawningDeploymentModule subject = BuildSubject();

		// FIRE THE LATCH FIRST, through the one path that spends the attempt without building anything.
		// A clone assertion made against a module that had never attempted could not tell an inherited
		// latch from an absent one.
		subject.AbandonRaise("OVT_TEST_Init_ObjectiveFOB_J is firing the latch before cloning");

		OVT_FOBRaiseSpawningDeploymentModule clone = OVT_FOBRaiseSpawningDeploymentModule.Cast(subject.CloneModule());
		if (!clone)
		{
			SetFailure("CloneModule() did not return an OVT_FOBRaiseSpawningDeploymentModule");
			return true;
		}

		string failure = Compare(subject, clone);
		if (failure != "")
		{
			SetFailure("the raise module's clone dropped %1 - a dropped clone line ships the class default on every deployment, forever, and nothing warns", failure);
			return true;
		}

		if (clone.HasAttemptedRaise())
		{
			SetFailure("the raise module's clone inherited a FIRED latch - every deployment made from a template that had once attempted would refuse to build, and the middle phase would never produce a forward base again in that session");
			return true;
		}

		Print("Objective forward base: the raise module's clone carries all twenty-six attributes - thirteen inherited from the infantry module, eleven from the insertion module, two of its own - and does not inherit a fired latch");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A module with every field set to something that is not the class default.
	//! \return The subject.
	protected OVT_FOBRaiseSpawningDeploymentModule BuildSubject()
	{
		OVT_FOBRaiseSpawningDeploymentModule subject = new OVT_FOBRaiseSpawningDeploymentModule();

		subject.m_sModuleName = "OVT_TEST raise";
		subject.m_sGroupType = "OVT_TEST group";
		subject.m_iMinGroupCount = 3;
		subject.m_iMaxGroupCount = 7;
		subject.m_bScaleByTownSize = true;
		subject.m_fSpawnRadius = 37;
		subject.m_iCostPerGroup = 91;
		subject.m_bAllowReinforcement = true;
		subject.m_iReinforcementCost = 83;
		subject.m_bSpawnAtNearestBase = true;
		subject.m_bReinforceFromNearestBase = true;
		subject.m_eImportance = SCR_EAISpawnImportance.CRITICAL;
		subject.m_bSnapToRoad = true;

		subject.m_Source = new OVT_ObjectiveAnchorSourceProvider();
		subject.m_fWalkThresholdDistance = 411;
		subject.m_sTruckVehicleType = "OVT_TEST truck";
		subject.m_sTruckCrewGroup = "OVT_TEST crew";
		subject.m_fLZStandoffDistance = 271;
		subject.m_fStuckSpeedThreshold = 2.5;
		subject.m_iStuckTicks = 13;
		subject.m_fArrivalRadius = 47;
		subject.m_iTruckCostOverride = 61;
		subject.m_bWalkWhenInsertionRefused = true;
		subject.m_bTransportIsObserver = true;

		subject.m_rFOBPrefab = "{6B8C3F5D0000009A}Prefabs/Bases/OVT_OccupyingFOB.et";
		subject.m_fRaiseOnFootRadius = 93;

		return subject;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] subject The module that was cloned.
	//! \param[in] clone What came back.
	//! \return An empty string when every attribute matched, or the name of the first that did not.
	protected string Compare(notnull OVT_FOBRaiseSpawningDeploymentModule subject, notnull OVT_FOBRaiseSpawningDeploymentModule clone)
	{
		if (clone.m_sModuleName != subject.m_sModuleName) return "m_sModuleName";
		if (clone.m_sGroupType != subject.m_sGroupType) return "m_sGroupType";
		if (clone.m_iMinGroupCount != subject.m_iMinGroupCount) return "m_iMinGroupCount";
		if (clone.m_iMaxGroupCount != subject.m_iMaxGroupCount) return "m_iMaxGroupCount";
		if (clone.m_bScaleByTownSize != subject.m_bScaleByTownSize) return "m_bScaleByTownSize";
		if (clone.m_fSpawnRadius != subject.m_fSpawnRadius) return "m_fSpawnRadius";
		if (clone.m_iCostPerGroup != subject.m_iCostPerGroup) return "m_iCostPerGroup";
		if (clone.m_bAllowReinforcement != subject.m_bAllowReinforcement) return "m_bAllowReinforcement";
		if (clone.m_iReinforcementCost != subject.m_iReinforcementCost) return "m_iReinforcementCost";
		if (clone.m_bSpawnAtNearestBase != subject.m_bSpawnAtNearestBase) return "m_bSpawnAtNearestBase";
		if (clone.m_bReinforceFromNearestBase != subject.m_bReinforceFromNearestBase) return "m_bReinforceFromNearestBase";
		if (clone.m_eImportance != subject.m_eImportance) return "m_eImportance";
		if (clone.m_bSnapToRoad != subject.m_bSnapToRoad) return "m_bSnapToRoad";

		if (clone.m_Source != subject.m_Source) return "m_Source";
		if (clone.m_fWalkThresholdDistance != subject.m_fWalkThresholdDistance) return "m_fWalkThresholdDistance";
		if (clone.m_sTruckVehicleType != subject.m_sTruckVehicleType) return "m_sTruckVehicleType";
		if (clone.m_sTruckCrewGroup != subject.m_sTruckCrewGroup) return "m_sTruckCrewGroup";
		if (clone.m_fLZStandoffDistance != subject.m_fLZStandoffDistance) return "m_fLZStandoffDistance";
		if (clone.m_fStuckSpeedThreshold != subject.m_fStuckSpeedThreshold) return "m_fStuckSpeedThreshold";
		if (clone.m_iStuckTicks != subject.m_iStuckTicks) return "m_iStuckTicks";
		if (clone.m_fArrivalRadius != subject.m_fArrivalRadius) return "m_fArrivalRadius";
		if (clone.m_iTruckCostOverride != subject.m_iTruckCostOverride) return "m_iTruckCostOverride";
		if (clone.m_bWalkWhenInsertionRefused != subject.m_bWalkWhenInsertionRefused) return "m_bWalkWhenInsertionRefused";
		if (clone.m_bTransportIsObserver != subject.m_bTransportIsObserver) return "m_bTransportIsObserver";

		if (clone.m_rFOBPrefab != subject.m_rFOBPrefab) return "m_rFOBPrefab";
		if (clone.m_fRaiseOnFootRadius != subject.m_fRaiseOnFootRadius) return "m_fRaiseOnFootRadius";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! 🔴 THE FORWARD BASE'S SPEND CEILING CAN ALWAYS BUY THE FORWARD BASE. Two numbers authored in two
//! different files have to agree, and nothing but this case looks at both:
//!
//!   the ceiling  = objectiveFOBCost x FOB_CEILING_MULTIPLIER   Configs/Difficulty/Difficulty_*.conf
//!   the price    = Deployment_ObjectiveFOB's total resource cost   Configs/Deployment/*.conf
//!
//! WHAT HAPPENS IF THEY DISAGREE, AND WHY NOTHING ELSE WOULD CATCH IT. The raise module arms the
//! ceiling BEFORE it asks whether it may buy the base, deliberately - §3.7 requires the structure's own
//! cost to be inside its own budget. So a ceiling smaller than the price refuses the phase's very first
//! spend, and refuses it identically on every subsequent tick, forever: the objective is locked against
//! re-selection from this phase onward, so it can neither advance nor be replaced. It is a permanent
//! deadlock produced entirely by arithmetic, with no syntax error, no failed lookup and nothing in a
//! play-test to point at except a phase that does nothing.
//!
//! THE SECOND CLAIM: THE CEILING MUST LEAVE ROOM FOR AT LEAST ONE GARRISON after the base is paid for,
//! or objectiveFOBGarrisonMax is a dead knob on that preset - the phase would raise a flag it can never
//! reinforce, on a preset whose difficulty file says it may have up to N garrisons.
//!
//! ⚠ IT DOES NOT ASSERT THAT THE WHOLE AUTHORED PLAN FITS (base + max garrisons), and must not. The
//! garrison cap is a CONCURRENCY cap, not a purchase cap: garrisons are wiped and rebought, so cumulative
//! spend legitimately exceeds cap x price and the ceiling is SUPPOSED to bite eventually. That is the
//! whole point of a ceiling.
//!
//! PRESETS ARE FOUND BY NAME, NEVER BY INDEX, and Test World is excluded - both for the reasons
//! OVT_TEST_Init_DifficultyObjectiveSabotageInversion records.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; each exited tools/compile-check.sh
//! 0, and the subject was restored and re-compiled clean):
//!   C1. Difficulty_Easy.conf's objectiveFOBCost set to 30 (ceiling 90, base 120). Fails on "the forward
//!       base's own deployment costs more than the whole ceiling".
//!   C2. Deployment_ObjectiveFOB.conf's m_iBaseCost raised to 1200. Fails the same way on every preset.
//!   C3. FOB_CEILING_MULTIPLIER set to 1 with objectiveFOBCost at 130. Fails on the garrison headroom
//!       claim - the base fits and nothing else ever can.
//! No polling, no world state touched, no maxAttempts: every value is read off a loaded config.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFOB_CeilingCanCoverTheForwardBase : SCR_AutotestCaseBase
{
	//! The shipped ramp. Difficulty_TestWorld.conf authors none of the objective fields and is not part
	//! of it.
	protected static const ref array<string> SHIPPED_PRESETS = {"Easy", "Normal", "Hard", "Extreme", "Insane"};

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();

		if (!config || !deployments)
		{
			SetFailure("The campaign config or the deployment framework did not resolve");
			return true;
		}

		if (!config.m_aDifficultyPresets || config.m_aDifficultyPresets.IsEmpty())
		{
			SetFailure("The config component carries no difficulty presets, so no ceiling can be computed");
			return true;
		}

		OVT_DeploymentConfig fob = deployments.FindConfigByName(OVT_ObjectiveDirectorComponent.FOB_CONFIG);
		if (!fob)
		{
			SetFailure("'%1' is not registered in overthrowDeployments.conf - no forward base can ever be raised", OVT_ObjectiveDirectorComponent.FOB_CONFIG);
			return true;
		}

		OVT_DeploymentConfig garrison = deployments.FindConfigByName(OVT_ObjectiveDirectorComponent.FOB_GARRISON_CONFIG);
		if (!garrison)
		{
			SetFailure("'%1' is not registered in overthrowDeployments.conf - a forward base could never be reinforced", OVT_ObjectiveDirectorComponent.FOB_GARRISON_CONFIG);
			return true;
		}

		int fobPrice = fob.GetTotalResourceCost();
		int garrisonPrice = garrison.GetTotalResourceCost();

		// A free forward base would make every claim below vacuously true.
		if (fobPrice <= 0)
		{
			SetFailure("the forward base's deployment costs %1 - a free operation makes the whole spend ceiling meaningless", fobPrice.ToString());
			return true;
		}

		string failure = CheckEveryPreset(config, fobPrice, garrisonPrice);
		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective FOB: every shipped preset's spend ceiling covers the forward base itself (" + fobPrice.ToString() + ") with room for at least one garrison (" + garrisonPrice.ToString() + ")");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] config The config component holding the presets.
	//! \param[in] fobPrice What the forward base's own deployment costs.
	//! \param[in] garrisonPrice What one garrison costs.
	//! \return An empty string when every preset held, or the first that did not.
	protected string CheckEveryPreset(notnull OVT_OverthrowConfigComponent config, int fobPrice, int garrisonPrice)
	{
		foreach (string presetName : SHIPPED_PRESETS)
		{
			OVT_DifficultySettings preset = FindPreset(config, presetName);
			if (!preset)
				return string.Format("No shipped difficulty preset named '%1' is loaded", presetName);

			int ceiling = OVT_ObjectivePhaseRules.FOBBudgetCeiling(preset.objectiveFOBCost);

			// --- THE DEADLOCK. See the header: this refusal repeats forever and ends nothing.
			if (ceiling < fobPrice)
				return string.Format("preset '%1': the forward base's own deployment costs %2 but its whole spend ceiling is %3 (objectiveFOBCost %4) - the forward-base phase could never make its first spend and the objective would be locked in it permanently",
					presetName, fobPrice.ToString(), ceiling.ToString(), preset.objectiveFOBCost.ToString());

			// --- AND THE GARRISON KNOB HAS TO MEAN SOMETHING.
			if (preset.objectiveFOBGarrisonMax > 0 && ceiling < fobPrice + garrisonPrice)
				return string.Format("preset '%1' allows %2 garrison(s) at a forward base, but its ceiling of %3 has only %4 left after the base itself and one garrison costs %5 - the knob is dead on this preset",
					presetName, preset.objectiveFOBGarrisonMax.ToString(), ceiling.ToString(), (ceiling - fobPrice).ToString(), garrisonPrice.ToString());
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Finds a loaded difficulty preset by its configured name.
	//! \param[in] config The Overthrow config component holding the preset list.
	//! \param[in] presetName Name to match exactly.
	//! \return The matching preset, or null.
	protected OVT_DifficultySettings FindPreset(notnull OVT_OverthrowConfigComponent config, string presetName)
	{
		foreach (OVT_DifficultySettings preset : config.m_aDifficultyPresets)
		{
			if (preset && preset.name == presetName)
				return preset;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! 🔴 A PHASE 1 OPERATION IS NOT COLLECTED WHEN THE OBJECTIVE IS PROMOTED INTO THE FORWARD-BASE PHASE,
//! AND IS COLLECTED WHEN THE BATTLE STARTS. The behavioural half of the 2026-08-19 deadlock fix, driven
//! against the LIVE director and the LOADED configs rather than against hand-built modules.
//!
//! WHAT THIS ASSERTS THAT THE CONFIG CASES CANNOT. OVT_TEST_Init_ObjectiveOperations' phase-range case
//! reads the authored numbers; this one asks the shipped module the question the game asks it, in each
//! phase in turn, with a real objective committed on the real director. The distinction matters because
//! the collection path is not a number: OVT_ReinforcementBehaviorDeploymentModule with
//! m_bDeleteOnConditionFail 1 walks every condition module once per check interval and DELETES the
//! deployment on the first refusal. A sabotage team that answers false on the promotion tick is a team
//! deleted mid-mission - which this feature has already done once, to a team with 1561 m left to walk.
//!
//! ⚠ THE PREDICATE IS THE SAME ONE BOTH SIDES ASK, WHICH IS WHY ONE CASE COVERS BOTH DIRECTIONS.
//! EvaluateCondition() (runtime, the collection gate) and EvaluateStaticCondition() (creation) both call
//! IsAtCurrentObjective(), which is what is driven here. "A new sabotage operation may be sent in phase
//! 2" and "a live one is kept in phase 2" are one statement about one predicate; they cannot disagree.
//!
//! ⚠ THE GARRISON IS THE NEGATIVE CONTROL AND IT IS NOT PADDING. If the range had been made too loose -
//! a minimum-phase semantic, say - every row about the ramp would still pass. The garrison's 2 -> 2 span
//! is the row that fails if "belongs to phase N" ever quietly became "phase N or later".
//!
//! ⚠ NOTHING IS CREATED, SPENT OR SPAWNED. The case commits an objective record, plants the operation
//! cadence HIGH so no tick that lands afterwards can reach the spend path, drives EnterPhase() directly,
//! and resets the objective on every path including the red ones. It never calls DirectorTick() and
//! never touches the deployment pool.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   B1. `m_sThroughPhase "ForwardBase"` deleted from Deployment_ObjectiveSabotage.conf - the pre-fix
//!       authoring.
//!       Fails on "a sabotage team must survive the promotion into the forward-base phase". THIS ROW IS
//!       THE DEADLOCK, observed from the collection side.
//!   B2. IsAtCurrentObjective() reverted to an equality test on the first phase. Fails on the
//!       same row, from the code side rather than the data side.
//!   B3. PhaseIndexInRange() relaxed to `return index >= fromIndex;` (a pure minimum). Fails on "a sabotage
//!       team must be collected once the counter-attack has begun".
//!   B4. The distance test dropped from IsAtCurrentObjective(). Fails on "a position 5 km from the
//!       objective is not AT it".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFOB_BRampSurvivesThePromotion : SCR_AutotestCaseBase
{
	//! Where the fixture objective sits. Arbitrary and world-independent: the predicate under test only
	//! measures distance from it, and nothing here resolves a base, a town or a structure.
	static const vector OBJECTIVE = "13000 40 13000";

	//! Far enough outside every authored working radius (600 m for sabotage, 2500 m for the garrison)
	//! that no config could legitimately accept it.
	static const float FAR_AWAY = 5000;

	//! Planted operation cadence. HIGH ON PURPOSE - see the class header and
	//! OVT_TEST_Init_ObjectiveOperations' gate case. Nothing may be spent.
	static const int PLANTED_OP_TICKS = 44;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();

		if (!director || !deployments)
		{
			SetFailure("The objective director or the deployment framework did not resolve");
			return true;
		}

		OVT_ObjectiveConditionDeploymentModule ramp = FindCondition(deployments, OVT_ObjectiveDirectorComponent.SABOTAGE_CONFIG);
		OVT_ObjectiveConditionDeploymentModule garrison = FindCondition(deployments, OVT_ObjectiveDirectorComponent.FOB_GARRISON_CONFIG);

		if (!ramp || !garrison)
		{
			SetFailure("The sabotage config or the forward-base garrison config is not registered, or authors no objective condition - there is nothing to ask");
			return true;
		}

		director.CommitObjective(OVT_EObjectiveKind.BASE, OBJECTIVE, "phase range fixture");
		director.SetOperationCountdown(PLANTED_OP_TICKS);

		string failure = Check(director, ramp, garrison);

		// ALWAYS, INCLUDING ON THE RED PATHS. A committed objective left on the live director would
		// change what every later case sees, and would leave an anchor biasing the evaluator.
		director.ResetObjective("OVT_TEST_Init_ObjectiveFOB_B finished", false);

		if (failure == "")
			failure = CheckNoObjectiveRefuses(ramp);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective forward base: a Phase 1 operation is kept through the promotion into the forward-base phase and collected when the counter-attack begins, while a forward-base config is refused during harassment - the ramp continues, the battle is not walked into, and the counter-attack is reachable");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] director The live director, with the fixture objective committed.
	//! \param[in] ramp The sabotage config's objective condition, as authored.
	//! \param[in] garrison The garrison config's objective condition, as authored.
	//! \return An empty string when every claim held, or the first that did not.
	protected string Check(notnull OVT_ObjectiveDirectorComponent director, notnull OVT_ObjectiveConditionDeploymentModule ramp, notnull OVT_ObjectiveConditionDeploymentModule garrison)
	{
		// --- HARASSMENT. The ramp works; the forward base's garrison does not exist yet.
		if (director.GetObjectivePhaseName() != "Harassment")
			return string.Format("committing an objective must enter the harassment phase, and the director is in phase %1", director.GetObjectivePhaseName());

		if (!ramp.IsAtCurrentObjective(OBJECTIVE))
			return "a sabotage team must be sendable and keepable during harassment - that is the phase the ramp runs in";

		if (garrison.IsAtCurrentObjective(OBJECTIVE))
			return "a forward-base garrison must NOT be accepted during harassment - there is no forward base to garrison, and a config that answers yes here has a span that has stopped meaning anything";

		// --- THE PROMOTION. The row this whole case exists for.
		director.EnterPhase("ForwardBase");

		if (!ramp.IsAtCurrentObjective(OBJECTIVE))
			return "a sabotage team must survive the promotion into the forward-base phase. THIS IS THE 2026-08-19 DEADLOCK: one sabotage success promotes a base objective out of harassment, the reinforcement module's m_bDeleteOnConditionFail collects every team that answers false, and no further mission can ever be sent - so the six the counter-attack demands on Easy can never be reached and the objective sits until its idle clock runs out";

		if (!garrison.IsAtCurrentObjective(OBJECTIVE))
			return "a forward-base garrison must be accepted in the phase it belongs to - the forward base would stand with only the party that raised it";

		if (ramp.IsAtCurrentObjective(OBJECTIVE + Vector(FAR_AWAY, 0, 0)))
			return "a position 5 km from the objective is not AT it, in any phase - the working radius is what stops an objective config being accepted anywhere on the map";

		// --- THE BATTLE. Both are collected: teams walking in to soften a place already being stormed
		//     are noise, and the garrison's own phase is over.
		director.EnterPhase("CounterAttack");

		if (ramp.IsAtCurrentObjective(OBJECTIVE))
			return "a sabotage team must be collected once the counter-attack has begun - a span that reaches the battle is a minimum-phase semantic wearing a range's clothes";

		if (garrison.IsAtCurrentObjective(OBJECTIVE))
			return "a forward-base garrison must be collected once the counter-attack has begun - the counter-attack controller spawns its own siege force at the same place";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Run AFTER the objective has been reset, so it is a statement about the reset rather than about a
	//! phase: no objective is a refusal, never a pass.
	//! \param[in] ramp The sabotage config's objective condition.
	//! \return An empty string when it refused, or why that matters.
	protected string CheckNoObjectiveRefuses(notnull OVT_ObjectiveConditionDeploymentModule ramp)
	{
		if (ramp.IsAtCurrentObjective(OBJECTIVE))
			return "with no objective committed the condition must refuse - the evaluator considers every registered config at every candidate position it generates, and an objective config that answers yes with no objective would be created by it, anywhere, outside the director's accounting entirely";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The objective condition module a registered config authors, as loaded.
	//! \param[in] deployments The deployment framework.
	//! \param[in] name The config's registered name.
	//! \return The module, or null when the config is missing or authors none.
	protected OVT_ObjectiveConditionDeploymentModule FindCondition(notnull OVT_DeploymentManagerComponent deployments, string name)
	{
		OVT_DeploymentConfig config = deployments.FindConfigByName(name);
		if (!config)
			return null;

		foreach (OVT_BaseDeploymentModule module : config.m_aModules)
		{
			OVT_ObjectiveConditionDeploymentModule condition = OVT_ObjectiveConditionDeploymentModule.Cast(module);
			if (condition)
				return condition;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! THE RAISE OPERATION CLONES EVERY ATTRIBUTE IT AND ITS PARENTS DECLARE.
//!
//! 🔴 IT HAS TWELVE OF THEM AND EVERY ONE FAILS SILENTLY. CloneModule() copies by hand, is not
//! chained, and ships the class default for whatever it forgot - which for this module is a zero, and
//! a zero in any of the siting fields collapses the band, the lattice or the corridor into "there is
//! nowhere to put a forward base" on every map in the campaign. Nothing errors and nothing warns; the
//! only symptom is a forward-base phase that abandons its objective every time.
//!
//! ⚠ `new` DOES NOT APPLY [Attribute()] DEFVALUES, so every field is set to a DISTINCT non-default
//! value here. A field left unset would read as zero on both sides and the comparison would pass
//! vacuously - which is exactly the failure being tested for.
//!
//! ⚠ THE RUNTIME STATE IS DELIBERATELY NOT COPIED and this case pins that too: a clone is a fresh
//! module for a fresh phase entry, so it has sent nothing and sited nothing.
//!
//! CAN-FAIL, BY CONSTRUCTION: delete any one of the twelve assignments in CloneModule() and the field
//! it copied reads as zero or empty on the clone.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFOB_KRaiseOperationCloneCarriesEveryAttribute : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_RaiseForwardBaseObjectiveOperation template = new OVT_RaiseForwardBaseObjectiveOperation();

		template.m_sModuleName = "Raise Clone Fixture";
		template.m_sAssetKey = "fixtureasset";
		template.m_sDeploymentConfigName = "fixture carrier config";
		template.m_sGarrisonConfigName = "fixture garrison config";
		template.m_iBudgetCost = 137;
		template.m_fBandMinFraction = 0.11;
		template.m_fBandMaxFraction = 0.91;
		template.m_fMinStandoff = 271;
		template.m_fMaxStandoff = 3371;
		template.m_iSitingSteps = 3;
		template.m_iSitingLanes = 7;
		template.m_fLateralSpread = 613;

		OVT_RaiseForwardBaseObjectiveOperation clone = OVT_RaiseForwardBaseObjectiveOperation.Cast(template.CloneModule());
		if (!clone)
		{
			SetFailure("OVT_RaiseForwardBaseObjectiveOperation.CloneModule() did not return an instance of its own class");
			return true;
		}

		string failure = Compare(template, clone);
		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		if (clone.IsDeploymentSent())
		{
			SetFailure("the clone believes a supply party has already been sent - a fresh phase entry would then never send one, and the ceiling would be armed for a forward base nobody bought");
			return true;
		}

		if (clone.GetSite() != vector.Zero)
		{
			SetFailure("the clone inherited the template's site, so the first teardown would sweep a place this objective never used");
			return true;
		}

		Print("Objective forward base: the raise operation's clone carries all twelve authored attributes and none of the template's runtime state");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] template The hand-built subject.
	//! \param[in] clone Its clone.
	//! \return An empty string when every field survived, or which one did not and what it costs.
	protected string Compare(notnull OVT_RaiseForwardBaseObjectiveOperation template, notnull OVT_RaiseForwardBaseObjectiveOperation clone)
	{
		if (clone.m_sModuleName != template.m_sModuleName)
			return "CloneModule() dropped m_sModuleName - every log line about this operation loses its label";

		if (clone.m_sAssetKey != template.m_sAssetKey)
			return string.Format("CloneModule() dropped m_sAssetKey - expected '%1', got '%2'. A clone with the wrong key owns no record at all, so the raise writes nothing, IsAssetUp() is false forever and the phase can never advance",
				template.m_sAssetKey, clone.m_sAssetKey);

		if (clone.m_sDeploymentConfigName != template.m_sDeploymentConfigName)
			return string.Format("CloneModule() dropped m_sDeploymentConfigName - expected '%1', got '%2'. Nothing can be bought and nothing can be found again, so the phase sends a base every interval and never notices one standing",
				template.m_sDeploymentConfigName, clone.m_sDeploymentConfigName);

		if (clone.m_sGarrisonConfigName != template.m_sGarrisonConfigName)
			return string.Format("CloneModule() dropped m_sGarrisonConfigName - expected '%1', got '%2'. The teardown then leaves the garrison standing in the field with no objective behind it",
				template.m_sGarrisonConfigName, clone.m_sGarrisonConfigName);

		if (clone.m_iBudgetCost != template.m_iBudgetCost)
			return string.Format("CloneModule() dropped m_iBudgetCost - expected %1, got %2. A clone reading 0 has a ZERO ceiling, which refuses every spend, so the forward-base phase can never buy anything at all",
				template.m_iBudgetCost.ToString(), clone.m_iBudgetCost.ToString());

		if (clone.m_fBandMinFraction != template.m_fBandMinFraction)
			return string.Format("CloneModule() dropped m_fBandMinFraction - expected %1, got %2",
				template.m_fBandMinFraction.ToString(), clone.m_fBandMinFraction.ToString());

		if (clone.m_fBandMaxFraction != template.m_fBandMaxFraction)
			return string.Format("CloneModule() dropped m_fBandMaxFraction - expected %1, got %2. A clone reading 0 has a band that ends before it begins, so every candidate is rejected and the objective is abandoned for having nowhere to build",
				template.m_fBandMaxFraction.ToString(), clone.m_fBandMaxFraction.ToString());

		if (clone.m_fMinStandoff != template.m_fMinStandoff)
			return string.Format("CloneModule() dropped m_fMinStandoff - expected %1, got %2. A clone reading 0 loses the absolute floor and a short supply line puts a forward base metres outside the town it is besieging",
				template.m_fMinStandoff.ToString(), clone.m_fMinStandoff.ToString());

		if (clone.m_fMaxStandoff != template.m_fMaxStandoff)
			return string.Format("CloneModule() dropped m_fMaxStandoff - expected %1, got %2. It is ALSO the radius the teardown searches for the carrier deployment in, so a zero means the teardown finds nothing and the marker outlives the objective",
				template.m_fMaxStandoff.ToString(), clone.m_fMaxStandoff.ToString());

		if (clone.m_iSitingSteps != template.m_iSitingSteps)
			return string.Format("CloneModule() dropped m_iSitingSteps - expected %1, got %2",
				template.m_iSitingSteps.ToString(), clone.m_iSitingSteps.ToString());

		if (clone.m_iSitingLanes != template.m_iSitingLanes)
			return string.Format("CloneModule() dropped m_iSitingLanes - expected %1, got %2. Lanes are the knob that costs candidates; a clone reading 0 is floored to one lane, so the sampler only ever looks straight down the supply road",
				template.m_iSitingLanes.ToString(), clone.m_iSitingLanes.ToString());

		if (clone.m_fLateralSpread != template.m_fLateralSpread)
			return string.Format("CloneModule() dropped m_fLateralSpread - expected %1, got %2. Every lane collapses onto the supply line and the authored-marker corridor closes to nothing",
				template.m_fLateralSpread.ToString(), clone.m_fLateralSpread.ToString());

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! The asset-up condition clones both of its attributes.
//!
//! ⚠ A DROPPED m_sAssetKey IS THE EXPENSIVE ONE: the clone asks about the empty key, no asset is ever
//! registered under it, the conjunct is false forever and the objective runs its idle clock down
//! rather than ever reaching its battle - with nothing in the log to say which conjunct refused.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFOB_LAssetUpCloneCarriesEveryAttribute : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_AssetUpObjectiveCondition template = new OVT_AssetUpObjectiveCondition();

		template.m_sModuleName = "Asset Up Clone Fixture";
		template.m_sAssetKey = "fixtureasset";
		template.m_bInverted = true;

		OVT_AssetUpObjectiveCondition clone = OVT_AssetUpObjectiveCondition.Cast(template.CloneModule());
		if (!clone)
		{
			SetFailure("OVT_AssetUpObjectiveCondition.CloneModule() did not return an instance of its own class");
			return true;
		}

		if (clone.m_sModuleName != template.m_sModuleName)
		{
			SetFailure("CloneModule() dropped m_sModuleName");
			return true;
		}

		if (clone.m_sAssetKey != template.m_sAssetKey)
		{
			SetFailure(string.Format("CloneModule() dropped m_sAssetKey - expected '%1', got '%2'. The clone asks about a key no asset is ever registered under, so the conjunct is false forever and the plan can never advance past this phase",
				template.m_sAssetKey, clone.m_sAssetKey));
			return true;
		}

		if (clone.m_bInverted != template.m_bInverted)
		{
			SetFailure("CloneModule() dropped m_bInverted - an inverted authoring silently becomes a plain one, which passes exactly when the author meant it to fail");
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The reserve-at-least condition clones its one attribute.
//!
//! ⚠ A CLONE READING 0 GATES NOTHING - MeetsResourceGate() treats a non-positive gate as "no
//! restriction" - so the battle fires the moment the rest of the ramp is done, with an empty reserve
//! behind it and no waves to follow, on every difficulty.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFOB_MReserveAtLeastCloneCarriesEveryAttribute : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ReserveAtLeastObjectiveCondition template = new OVT_ReserveAtLeastObjectiveCondition();

		template.m_sModuleName = "Reserve Clone Fixture";
		template.m_iGate = 4321;

		OVT_ReserveAtLeastObjectiveCondition clone = OVT_ReserveAtLeastObjectiveCondition.Cast(template.CloneModule());
		if (!clone)
		{
			SetFailure("OVT_ReserveAtLeastObjectiveCondition.CloneModule() did not return an instance of its own class");
			return true;
		}

		if (clone.m_sModuleName != template.m_sModuleName)
		{
			SetFailure("CloneModule() dropped m_sModuleName");
			return true;
		}

		if (clone.m_iGate != template.m_iGate)
		{
			SetFailure(string.Format("CloneModule() dropped m_iGate - expected %1, got %2. A clone reading 0 gates nothing at all, so the counter-attack starts against an empty reserve with no waves behind it",
				template.m_iGate.ToString(), clone.m_iGate.ToString()));
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The daylight-window condition clones both hours, and does NOT clone its log latch or its cached
//! clock handle.
//!
//! 🔴 A DROPPED HOUR HAS NO SYMPTOM AT ALL AND THAT IS WHY THIS CASE EXISTS. Either hour reading zero
//! makes the window 0-0, which IsCounterAttackWindow() reads as "no restriction" rather than as
//! "never" - so the daylight gate silently leaves the campaign and counter-attacks start at night
//! again. Nothing errors, nothing warns, and the only way to see it is to play until dark.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFOB_NDaylightWindowCloneCarriesEveryAttribute : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_DaylightWindowObjectiveCondition template = new OVT_DaylightWindowObjectiveCondition();

		template.m_sModuleName = "Daylight Clone Fixture";
		template.m_iStartHour = 7;
		template.m_iEndHour = 19;

		OVT_DaylightWindowObjectiveCondition clone = OVT_DaylightWindowObjectiveCondition.Cast(template.CloneModule());
		if (!clone)
		{
			SetFailure("OVT_DaylightWindowObjectiveCondition.CloneModule() did not return an instance of its own class");
			return true;
		}

		if (clone.m_sModuleName != template.m_sModuleName)
		{
			SetFailure("CloneModule() dropped m_sModuleName");
			return true;
		}

		if (clone.m_iStartHour != template.m_iStartHour)
		{
			SetFailure(string.Format("CloneModule() dropped m_iStartHour - expected %1, got %2. A window of 0 to N is not the authored one, and a window of 0 to 0 is no window at all",
				template.m_iStartHour.ToString(), clone.m_iStartHour.ToString()));
			return true;
		}

		if (clone.m_iEndHour != template.m_iEndHour)
		{
			SetFailure(string.Format("CloneModule() dropped m_iEndHour - expected %1, got %2. A zero-width window is read as NO RESTRICTION, so the daylight gate leaves the campaign silently and battles start at night again",
				template.m_iEndHour.ToString(), clone.m_iEndHour.ToString()));
			return true;
		}

		// ⚠ THE ONE RULE THE MODULE OWES ITS ROLE: a false daylight answer must not cost the objective a
		// round off the clock it runs against itself. It is the whole of the D17 correction and it is a
		// one-line override that a reader could "tidy" away without any other symptom.
		if (!clone.HoldsIdleClock())
		{
			SetFailure("the daylight condition does not hold the idle clock. A gate met at 16:00 would then spend the objective's whole remaining patience waiting out the night, and the objective would be abandoned FOR BEING DARK");
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The starvation abort clones all three of its attributes.
//!
//! 🔴 EVERY ONE OF THEM FAILS TOWARDS A CAMPAIGN THAT LOOKS FINE. A dropped m_sAssetKey or a dropped
//! m_iStarvationMinutes removes the resistance's entire counterplay - a forward base can then never be
//! starved out, however thoroughly it is cut off. A dropped m_fAreaRadius does the opposite and is
//! worse: no deployment is ever inside a zero radius, so the garrison always counts as dead and EVERY
//! forward base in the campaign starves out on schedule whatever the player does.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFOB_PAssetStarvedCloneCarriesEveryAttribute : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_AssetStarvedObjectiveAbort template = new OVT_AssetStarvedObjectiveAbort();

		template.m_sModuleName = "Starvation Clone Fixture";
		template.m_sAssetKey = "fixtureasset";
		template.m_iStarvationMinutes = 83;
		template.m_fAreaRadius = 417;

		OVT_AssetStarvedObjectiveAbort clone = OVT_AssetStarvedObjectiveAbort.Cast(template.CloneModule());
		if (!clone)
		{
			SetFailure("OVT_AssetStarvedObjectiveAbort.CloneModule() did not return an instance of its own class");
			return true;
		}

		if (clone.m_sModuleName != template.m_sModuleName)
		{
			SetFailure("CloneModule() dropped m_sModuleName");
			return true;
		}

		if (clone.m_sAssetKey != template.m_sAssetKey)
		{
			SetFailure(string.Format("CloneModule() dropped m_sAssetKey - expected '%1', got '%2'. The clone asks about a key no asset is registered under, so the forward base can never be starved and the resistance's whole counterplay is gone with no symptom",
				template.m_sAssetKey, clone.m_sAssetKey));
			return true;
		}

		if (clone.m_iStarvationMinutes != template.m_iStarvationMinutes)
		{
			SetFailure(string.Format("CloneModule() dropped m_iStarvationMinutes - expected %1, got %2. A clone reading 0 DISABLES the abort, so a base cut off for an in-game week still launches its counter-attack",
				template.m_iStarvationMinutes.ToString(), clone.m_iStarvationMinutes.ToString()));
			return true;
		}

		if (clone.m_fAreaRadius != template.m_fAreaRadius)
		{
			SetFailure(string.Format("CloneModule() dropped m_fAreaRadius - expected %1, got %2. Nothing is inside a zero radius, so the garrison always counts as dead and EVERY forward base starves out on schedule whatever the player does",
				template.m_fAreaRadius.ToString(), clone.m_fAreaRadius.ToString()));
			return true;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! 🔴 THE FORWARD-BASE PHASE'S AUTHORED CHAIN, PINNED IN BOTH SHIPPED PLANS.
//!
//! Everything this case asserts is authored in a .conf that no compiler reads, and every one of the
//! faults it catches is SILENT in play:
//!
//!   1. THE ORDER IS THE CONTRACT. The five operations are the hard-coded forward-base spender's
//!      five-way chain term for term - raise, garrison, tower recapture, harassment, sabotage - and
//!      the first module that acts consumes the interval, so re-ordering them re-tunes the phase.
//!      Nothing else in the tree would notice.
//!   2. 🔴 THE RAMP MUST BE REPEATED HERE AT ALL. Omitting the last three restores the 2026-08-19
//!      DEADLOCK: a base objective is promoted on its FIRST completed sabotage mission and the
//!      counter-attack gate demands up to six, so a promotion that stopped the ramp made the remaining
//!      five unsendable and the battle unreachable. Towns deadlock identically - the stacking debuff
//!      that drives support under 25 % is applied by harassment operations. THE FIX HAS TWO HALVES and
//!      this case pins BOTH: the operations authored here, AND each ramp deployment's own condition
//!      module spanning Harassment through ForwardBase.
//!   3. THE NAMES ARE THE KEYS. The raise module names two deployment configs by string; the director
//!      names the same two as constants, because IsObjectiveOperationConfig() still classifies a
//!      tracked deployment by them. Two lists of the same names, and drift between them is silent.
//!   4. EVERY PHASE MUST BE ABLE TO END. Since the doctrine became authored data, a phase with no idle
//!      abort cannot time out AT ALL - the clock runs to zero and nothing answers. The validator
//!      deliberately does not catch it (it cannot tell a terminal phase from a forgotten one), so the
//!      shipped plans are pinned here instead.
//!   5. THE GATE OUT IS THE COUNTER-ATTACK GATE, DECOMPOSED. Asset-up, the reserve gate, the daylight
//!      window and each doctrine's own ramp measure. Drop any one and the battle either never starts or
//!      starts on an unearned ramp.
//!
//! ⚠ IT READS ONLY. No deployment is created, no resource is spent and the live objective is not
//! touched.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFOB_QForwardBasePhaseAuthorsTheShippedChain : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();

		if (!director || !deployments)
		{
			SetFailure("The objective director or the deployment framework did not resolve");
			return true;
		}

		OVT_ObjectiveRegistry registry = director.GetRegistry();
		if (!registry)
		{
			SetFailure("The objective registry did not resolve, so the shipped plans' forward-base phases cannot be read. The campaign would run on the strangler fallback, which no longer has a forward-base tick to fall back ON");
			return true;
		}

		if (registry.GetConfigCount() < 2)
		{
			SetFailure(string.Format("the registry carries %1 plan(s); both shipped doctrines are needed here", registry.GetConfigCount().ToString()));
			return true;
		}

		for (int i = 0; i < registry.GetConfigCount(); i++)
		{
			OVT_ObjectiveConfig plan = registry.GetConfig(i);
			if (!plan)
			{
				SetFailure(string.Format("registry entry %1 is empty", i.ToString()));
				return true;
			}

			string failure = CheckPlan(plan, deployments);
			if (failure != "")
			{
				SetFailure(failure);
				return true;
			}
		}

		string spans = CheckRampSpansIntoTheForwardBasePhase(deployments);
		if (spans != "")
		{
			SetFailure(spans);
			return true;
		}

		Print("Objective forward base: both shipped plans author raise, garrison and the whole ramp repeated, in that order; the gate out is the counter-attack gate decomposed into four conjuncts; and every ramp deployment still spans Harassment through ForwardBase");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] plan The plan to walk.
	//! \param[in] deployments The deployment framework, for the name-resolution claims.
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckPlan(notnull OVT_ObjectiveConfig plan, notnull OVT_DeploymentManagerComponent deployments)
	{
		int index = plan.IndexOfPhase("ForwardBase");
		if (index < 0)
			return string.Format("plan '%1' carries no phase called 'ForwardBase' - the name is the persistence key AND the name every objective deployment's condition module spans to, so a rename here silently collects every ramp deployment on its next reinforcement check", plan.m_sObjectiveName);

		OVT_ObjectivePhase phase = plan.GetPhase(index);
		if (!phase || !phase.m_aModules)
			return string.Format("plan '%1' has an empty forward-base phase", plan.m_sObjectiveName);

		array<OVT_BaseObjectiveOperationModule> operations = new array<OVT_BaseObjectiveOperationModule>();

		foreach (OVT_BaseObjectiveModule module : phase.m_aModules)
		{
			OVT_BaseObjectiveOperationModule operation = OVT_BaseObjectiveOperationModule.Cast(module);
			if (operation)
				operations.Insert(operation);
		}

		// ⚠ DOCTRINE-SPECIFIC SINCE 2026-08-21. A base objective no longer chases radio towers - see
		// DoctrineChasesTowers() - so its repeated ramp is the ladder and sabotage, two operations, and
		// a town's is still three. The DEADLOCK claim is untouched: whatever the ramp is for a doctrine,
		// all of it has to be repeated here or the objective's own gate becomes unreachable.
		int expected = 4;
		if (DoctrineChasesTowers(plan.m_sObjectiveName))
			expected = 5;

		if (operations.Count() != expected)
			return string.Format("plan '%1' authors %2 operation(s) in its forward-base phase; this doctrine's shipped chain is exactly %3 - raise the base, garrison it, then its own harassment ramp repeated. Fewer is the 2026-08-19 deadlock: the ramp stops the moment the objective is promoted, and the counter-attack it is ramping towards becomes unreachable",
				plan.m_sObjectiveName, operations.Count().ToString(), expected.ToString());

		// --- 1. THE FORWARD BASE ITSELF IS FIRST. Nothing else in this phase means anything until the
		//        flag is up: the garrison's own source provider resolves to the base only once it stands.
		OVT_RaiseForwardBaseObjectiveOperation raise = OVT_RaiseForwardBaseObjectiveOperation.Cast(operations[0]);
		if (!raise)
			return string.Format("plan '%1' does not raise the forward base FIRST in its forward-base phase. Every spend is behind one cadence, so whichever operation is asked first is the one that gets the interval - and everything else here is for a base that is not there yet",
				plan.m_sObjectiveName);

		if (raise.m_sAssetKey != OVT_ObjectiveDirectorComponent.ASSET_FOB)
			return string.Format("plan '%1' raises its forward base under the asset key '%2' and every consumer asks about '%3'. The dismantle action, the anchor source provider, the QRF layer and the save payload all read that key",
				plan.m_sObjectiveName, raise.m_sAssetKey, OVT_ObjectiveDirectorComponent.ASSET_FOB);

		if (raise.m_sDeploymentConfigName != OVT_ObjectiveDirectorComponent.FOB_CONFIG)
			return string.Format("plan '%1' names the forward-base carrier '%2' and the director still classifies a tracked deployment by '%3'. A name the director does not recognise buys men whose walk does not hold the idle clock and whose recall pays nothing back",
				plan.m_sObjectiveName, raise.m_sDeploymentConfigName, OVT_ObjectiveDirectorComponent.FOB_CONFIG);

		if (raise.m_sGarrisonConfigName != OVT_ObjectiveDirectorComponent.FOB_GARRISON_CONFIG)
			return string.Format("plan '%1' names the forward-base garrison '%2' and the director knows it as '%3'. The teardown sweeps for BOTH names, so a mismatch leaves the garrison standing in a field with no objective behind it",
				plan.m_sObjectiveName, raise.m_sGarrisonConfigName, OVT_ObjectiveDirectorComponent.FOB_GARRISON_CONFIG);

		if (!deployments.FindConfigByName(raise.m_sDeploymentConfigName))
			return string.Format("plan '%1' names the forward-base config '%2', which the deployment registry does not carry", plan.m_sObjectiveName, raise.m_sDeploymentConfigName);

		if (!deployments.FindConfigByName(raise.m_sGarrisonConfigName))
			return string.Format("plan '%1' names the garrison config '%2', which the deployment registry does not carry", plan.m_sObjectiveName, raise.m_sGarrisonConfigName);

		// --- 2. THE GARRISON IS SECOND, AND IS CAPPED AT THE BASE RATHER THAN AT THE OBJECTIVE.
		OVT_SendDeploymentObjectiveOperation garrison = OVT_SendDeploymentObjectiveOperation.Cast(operations[1]);
		if (!garrison)
			return string.Format("plan '%1' does not send the forward-base garrison SECOND", plan.m_sObjectiveName);

		if (garrison.m_sConfigName != OVT_ObjectiveDirectorComponent.FOB_GARRISON_CONFIG)
			return string.Format("plan '%1' sends '%2' as its second forward-base operation; the garrison config is '%3'",
				plan.m_sObjectiveName, garrison.m_sConfigName, OVT_ObjectiveDirectorComponent.FOB_GARRISON_CONFIG);

		if (!OVT_ForwardBaseTargetResolver.Cast(garrison.m_Resolver))
			return string.Format("plan '%1' does not send its garrison through the forward-base resolver, so it would be sent at the OBJECTIVE - the place the resistance holds - rather than at the base it is supposed to hold",
				plan.m_sObjectiveName);

		if (!garrison.m_bConcurrencyAtResolvedPosition)
			return string.Format("plan '%1' counts the garrison cap around the OBJECTIVE rather than around the forward base, where the garrison actually is. The cap then counts nothing and the base is reinforced without limit",
				plan.m_sObjectiveName);

		if (garrison.m_iMaxConcurrentDifficulty != OVT_EObjectiveConcurrencyLimit.FORWARD_BASE_GARRISON)
			return string.Format("plan '%1' defers the garrison cap to the wrong difficulty field. objectiveFOBGarrisonMax runs 1 to 6 across the presets and objectiveHarassmentMaxConcurrent runs 1 to 4; they are different numbers on every one of them",
				plan.m_sObjectiveName);

		if (garrison.m_fConcurrencyRadius <= 0)
			return string.Format("plan '%1' authors no concurrency radius on the garrison, so nothing is ever inside it and the cap never binds", plan.m_sObjectiveName);

		// --- 3-5. THE RAMP, REPEATED. See the class header for the deadlock that omitting it restores.
		string ramp = CheckRampIsRepeated(plan, operations);
		if (ramp != "")
			return ramp;

		return CheckGate(plan, phase);
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 THE RAMP CONTINUES INTO THIS PHASE. Half one of the deadlock fix.
	//! \param[in] plan The plan being walked.
	//! \param[in] operations Its forward-base phase's operations, in authored order.
	//! \return An empty string when every claim held, or the first that did not.
	//------------------------------------------------------------------------------------------------
	//! 🔴 DOES THIS DOCTRINE CHASE RADIO TOWERS? (author, 2026-08-21.)
	//!
	//! *"This is a base, radio towers don't matter to a base and there are non-objective deployments
	//! built to handle radio towers that don't matter to the current objective."* A town objective still
	//! repeats tower recapture - unrest at a tower is what its harassment ramp is fighting - and a base
	//! objective no longer does.
	//! \param[in] planName The plan being walked.
	//! \return True when this doctrine repeats tower recapture in its ramp.
	protected bool DoctrineChasesTowers(string planName)
	{
		return planName != "Base Offensive";
	}

	//------------------------------------------------------------------------------------------------
	protected string CheckRampIsRepeated(notnull OVT_ObjectiveConfig plan, notnull array<OVT_BaseObjectiveOperationModule> operations)
	{
		int ladderAt = 2;
		int sabotageAt = 3;

		if (DoctrineChasesTowers(plan.m_sObjectiveName))
		{
			ladderAt = 3;
			sabotageAt = 4;

			OVT_SendDeploymentObjectiveOperation tower = OVT_SendDeploymentObjectiveOperation.Cast(operations[2]);
			if (!tower || tower.m_sConfigName != OVT_ObjectiveDirectorComponent.TOWER_RECAPTURE_CONFIG)
				return string.Format("plan '%1' does not repeat tower recapture in its forward-base phase. A tower left in resistance hands keeps the objective easier for them to hold right through the build-up",
					plan.m_sObjectiveName);
		}
		else
		{
			// 🔴 THE BASE DOCTRINE'S POSITIVE CLAIM. The tower operation was not merely dropped from the
			// assertions when it was dropped from the plan - its ABSENCE is now the requirement, so
			// re-authoring one fails here rather than passing silently.
			foreach (OVT_BaseObjectiveOperationModule operation : operations)
			{
				OVT_SendDeploymentObjectiveOperation send = OVT_SendDeploymentObjectiveOperation.Cast(operation);
				if (send && send.m_sConfigName == OVT_ObjectiveDirectorComponent.TOWER_RECAPTURE_CONFIG)
					return string.Format("plan '%1' sends a tower recapture in its forward-base phase. Radio towers are nothing to do with a base objective and the standing non-objective tower deployments already handle them (author, 2026-08-21)",
						plan.m_sObjectiveName);
			}
		}

		OVT_SendDeploymentObjectiveOperation harassment = OVT_SendDeploymentObjectiveOperation.Cast(operations[ladderAt]);
		if (!harassment || !harassment.m_aLadder || harassment.m_aLadder.IsEmpty())
			return string.Format("plan '%1' does not repeat the harassment ladder in its forward-base phase. The stacking support debuff that drives a town under a quarter support is applied BY harassment operations, so a town objective could never reach its own counter-attack gate",
				plan.m_sObjectiveName);

		if (harassment.m_aLadder.Count() != OVT_ObjectiveDirectorComponent.HARASSMENT_LADDER.Count())
			return string.Format("plan '%1' repeats %2 ladder rung(s) in its forward-base phase and the director recognises %3",
				plan.m_sObjectiveName, harassment.m_aLadder.Count().ToString(), OVT_ObjectiveDirectorComponent.HARASSMENT_LADDER.Count().ToString());

		for (int rung = 0; rung < harassment.m_aLadder.Count(); rung++)
		{
			if (harassment.m_aLadder[rung] != OVT_ObjectiveDirectorComponent.HARASSMENT_LADDER[rung])
				return string.Format("plan '%1' forward-base rung %2 is '%3' and the director's rung %2 is '%4'",
					plan.m_sObjectiveName, rung.ToString(), harassment.m_aLadder[rung], OVT_ObjectiveDirectorComponent.HARASSMENT_LADDER[rung]);
		}

		OVT_SendDeploymentObjectiveOperation sabotage = OVT_SendDeploymentObjectiveOperation.Cast(operations[sabotageAt]);
		if (!sabotage || sabotage.m_sConfigName != OVT_ObjectiveDirectorComponent.SABOTAGE_CONFIG)
			return string.Format("plan '%1' does not repeat sabotage in its forward-base phase. A base objective is promoted on its FIRST completed mission and its counter-attack gate demands up to six, so the remaining five would be unsendable and the battle unreachable - this is the 2026-08-19 deadlock exactly",
				plan.m_sObjectiveName);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The gate out: the counter-attack gate, decomposed into authored conjuncts, plus the two aborts.
	//! \param[in] plan The plan being walked.
	//! \param[in] phase Its forward-base phase.
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckGate(notnull OVT_ObjectiveConfig plan, notnull OVT_ObjectivePhase phase)
	{
		bool hasAssetUp = false;
		bool hasReserve = false;
		bool hasDaylight = false;
		bool hasRampMeasure = false;
		bool hasIdleAbort = false;
		bool hasStarvationAbort = false;

		foreach (OVT_BaseObjectiveModule module : phase.m_aModules)
		{
			OVT_AssetUpObjectiveCondition assetUp = OVT_AssetUpObjectiveCondition.Cast(module);
			if (assetUp && assetUp.m_sAssetKey == OVT_ObjectiveDirectorComponent.ASSET_FOB && !assetUp.m_bInverted)
				hasAssetUp = true;

			if (OVT_ReserveAtLeastObjectiveCondition.Cast(module))
				hasReserve = true;

			if (OVT_DaylightWindowObjectiveCondition.Cast(module))
				hasDaylight = true;

			if (OVT_SupportBelowObjectiveCondition.Cast(module) || OVT_ProgressAtLeastObjectiveCondition.Cast(module))
				hasRampMeasure = true;

			if (OVT_IdleForObjectiveAbort.Cast(module))
				hasIdleAbort = true;

			OVT_AssetStarvedObjectiveAbort starved = OVT_AssetStarvedObjectiveAbort.Cast(module);
			if (starved && starved.m_sAssetKey == OVT_ObjectiveDirectorComponent.ASSET_FOB)
				hasStarvationAbort = true;
		}

		if (!hasAssetUp)
			return string.Format("plan '%1' does not require its forward base to be STANDING before it advances. The battle would then be mounted out of thin air, which is the dice-roll attack this whole feature replaced",
				plan.m_sObjectiveName);

		if (!hasReserve)
			return string.Format("plan '%1' does not gate its battle on the faction's reserve, so a counter-attack would start with nothing behind it and no waves to follow",
				plan.m_sObjectiveName);

		if (!hasDaylight)
			return string.Format("plan '%1' does not carry the daylight window, so counter-attacks would begin at night again - and the wait for morning would no longer hold the objective's idle clock either",
				plan.m_sObjectiveName);

		if (!hasRampMeasure)
			return string.Format("plan '%1' does not measure its own ramp before advancing, so the battle would fire on the phase's entry tick with none of the build-up the resistance is meant to be able to read",
				plan.m_sObjectiveName);

		if (!hasStarvationAbort)
			return string.Format("plan '%1' authors no starvation abort in its forward-base phase - the resistance's entire counterplay is gone and a forward base could be cut off indefinitely and still launch its battle",
				plan.m_sObjectiveName);

		if (!hasIdleAbort)
			return string.Format("plan '%1' authors no idle abort in its forward-base phase. Since the doctrine became authored data a phase with none CANNOT TIME OUT AT ALL: the clock runs to zero and nothing answers, so a wedged objective sits forever. The registry's validator deliberately does not catch this - it cannot tell a terminal phase from a forgotten one",
				plan.m_sObjectiveName);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 HALF TWO OF THE DEADLOCK FIX, IN A DIFFERENT FILE. The ramp's operations being authored in
	//! this phase is not enough on its own: each ramp deployment carries its own condition module, and
	//! a deployment whose span does not REACH the forward-base phase is collected on its next
	//! reinforcement check the moment the objective is promoted. Both halves or the deadlock returns.
	//! \param[in] deployments The deployment framework.
	//! \return An empty string when every ramp config spans into this phase, or the first that does not.
	protected string CheckRampSpansIntoTheForwardBasePhase(notnull OVT_DeploymentManagerComponent deployments)
	{
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
				return string.Format("the deployment registry does not carry '%1'", name);

			bool spans = false;

			foreach (OVT_BaseDeploymentModule module : config.m_aModules)
			{
				OVT_ObjectiveConditionDeploymentModule condition = OVT_ObjectiveConditionDeploymentModule.Cast(module);
				if (!condition)
					continue;

				if (condition.m_sThroughPhase == "ForwardBase")
					spans = true;
			}

			if (!spans)
				return string.Format("deployment config '%1' does not span through to the forward-base phase. Its m_sThroughPhase has to name '%2' or every one of its deployments is collected on its next reinforcement check the moment the objective is promoted - which is the 2026-08-19 deadlock, restored from the other side",
					name, "ForwardBase");
		}

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! 🔴 THE SPEND CEILING ARMS WITH THE ASSET AND DISARMS WITH THE OBJECTIVE, AND THE RUNTIME MODULE SET
//! IS CLONES.
//!
//! TWO CLAIMS, DRIVEN ON THE LIVE DIRECTOR, NEITHER OF WHICH SPENDS A RESOURCE.
//!
//!   1. THE CEILING IS A LATCH WITH TWO EDGES AND NEITHER HAS ANY OTHER SYMPTOM. It is INACTIVE during
//!      harassment, so the ramp spends against the pool alone exactly as it did before the forward base
//!      existed; it ARMS the moment the base is standing (and, in the live machine, the moment its own
//!      deployment is SENT, so the structure's own cost is inside the budget); and it DISARMS when the
//!      objective's record is cleared. A ceiling that failed to arm would let the forward-base phase
//!      spend the whole faction pool; one that failed to disarm would measure the NEXT objective's very
//!      first spend against a base that no longer exists.
//!
//!   2. 🔴 THE RUNTIME SET IS CLONES, NEVER THE CONFIG'S OWN TEMPLATE OBJECTS. A phase entered twice -
//!      by two objectives, or by the same objective in two campaigns in one session - would otherwise
//!      share one module object and one set of latches, and the second entry would inherit the first's
//!      state: a raise module that already believes it sent a supply party sends nothing, forever. This
//!      phase rebuilds a module set, so it repeats the assertion build phase 4 added for the harassment
//!      phase rather than trusting it.
//!
//! ⚠ NO DEPLOYMENT IS CREATED AND NO RESOURCE MOVES. The asset is reported standing through the same
//! public reporter the deployment-side raise module uses, which records and decides nothing.
//!
//! ⚠ IT PUTS THE DIRECTOR BACK on every path, including the red ones, because the initialisation world
//! is shared.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFOB_RCeilingArmsWithTheAssetAndTheSetIsClones : SCR_AutotestCaseBase
{
	//! Somewhere no campaign feature is, so nothing here disturbs anything and the teardown's structure
	//! query comes back empty.
	static const vector PROBE = "13000 40 13000";

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

		string failure = Check(director);

		// ALWAYS, INCLUDING ON THE RED PATHS.
		director.ResetObjective("OVT_TEST_Init_ObjectiveFOB_R finished", false);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective forward base: the spend ceiling is inactive during harassment, arms with the standing asset and disarms with the objective - and the phase's runtime modules are clones of the plan's templates, never the templates themselves");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] director The live director.
	//! \return An empty string when every claim held, or the first that did not.
	protected string Check(notnull OVT_ObjectiveDirectorComponent director)
	{
		if (director.HasObjective())
			return "the director already has an objective before this case committed one - an earlier case left one running and neither claim here can be trusted";

		// --- HARASSMENT: nothing is armed, because the raise module is not in this phase's set at all.
		director.CommitObjective(OVT_EObjectiveKind.BASE, PROBE, "OVT_TEST_Init_ObjectiveFOB_R fixture");

		if (director.IsAssetCeilingArmed())
			return "the forward base's spend ceiling is armed during the HARASSMENT phase. The ramp would then be measured against a budget for a base nobody has bought, and the phase would stop spending long before it should";

		if (director.GetAssetModule(OVT_ObjectiveDirectorComponent.ASSET_FOB))
			return "an asset module is registered during the harassment phase, which does not author one - a module registered by the wrong phase would keep a ceiling and a teardown alive for an asset that does not exist";

		// --- FORWARD BASE: the module registers on entry, and the ceiling is still down until an asset
		//     actually exists.
		director.EnterPhase("ForwardBase");

		OVT_BaseObjectiveAssetModule owner = director.GetAssetModule(OVT_ObjectiveDirectorComponent.ASSET_FOB);
		if (!owner)
			return "entering the forward-base phase registered no asset module. The director would then have nothing to ask about the spend ceiling and nothing to tell to take the base down when the objective ends - a structure and a garrison left standing in the world for the rest of the campaign";

		if (director.IsAssetCeilingArmed())
			return "the spend ceiling armed on the phase ENTRY, before anything was sent. It arms when the forward base's own deployment goes out, so that the structure's own cost is inside the budget - not before";

		string clones = CheckRuntimeSetIsClones(director, owner);
		if (clones != "")
			return clones;

		// --- THE ASSET IS STANDING: armed.
		director.ReportAssetRaised(OVT_ObjectiveDirectorComponent.ASSET_FOB, PROBE, PROBE, OVT_ObjectiveDirectorComponent.FOB_CONFIG);

		if (!director.IsAssetUp(OVT_ObjectiveDirectorComponent.ASSET_FOB))
			return "the keyed asset reporter did not put the forward base up";

		if (!director.IsAssetCeilingArmed())
			return "the spend ceiling is NOT armed with the forward base standing. Every ramp operation continuing into this phase would then spend against the faction pool alone, with no ceiling at all, which is the unbounded spender the budget exists to prevent";

		// ⚠ ONLY WHEN THERE IS A DIFFICULTY PRESET TO RESOLVE AGAINST. The shipped module authors the -1
		// sentinel, so in a world with no campaign settings loaded a zero ceiling is the honest answer
		// rather than a defect - and a case that went red for its environment would say nothing about
		// the product.
		if (OVT_Global.GetDifficulty() && owner.GetCeiling() <= 0)
			return "the registered asset module reports a ceiling of zero or less, which WithinFOBCeiling() refuses everything against - the phase could then never buy anything at all";

		// --- AND IT DISARMS WITH THE OBJECTIVE.
		director.ResetObjective("OVT_TEST_Init_ObjectiveFOB_R is exercising the disarm", false);

		if (director.IsAssetCeilingArmed())
			return "the spend ceiling is still armed after the objective ended. The NEXT objective's very first spend would be measured against a forward base that no longer exists, and the ramp would refuse operations it can afford";

		if (director.GetAssetModule(OVT_ObjectiveDirectorComponent.ASSET_FOB))
			return "the asset owner is still registered after the objective ended, so the next teardown would be handed a module whose record has already been zeroed";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 The runtime set is CLONES. See the class header.
	//! \param[in] director The live director.
	//! \param[in] owner The registered asset module, which must be one of the runtime clones.
	//! \return An empty string when the claim held, or why it did not.
	protected string CheckRuntimeSetIsClones(notnull OVT_ObjectiveDirectorComponent director, notnull OVT_BaseObjectiveAssetModule owner)
	{
		OVT_ObjectiveInstance instance = director.GetObjectiveInstance(0);
		if (!instance)
			return "the director reports no running objective instance, so the runtime module set cannot be inspected";

		OVT_ObjectiveConfig plan = instance.GetConfig();
		if (!plan)
			return "the running objective has no plan behind it, so there are no templates to compare the runtime set against";

		OVT_ObjectivePhase phase = plan.GetPhase(instance.GetPhaseIndex());
		if (!phase || !phase.m_aModules || phase.m_aModules.IsEmpty())
			return "the running phase authors no modules at all";

		int runtimeCount = instance.GetRuntimeModuleCount();
		if (runtimeCount != phase.m_aModules.Count())
			return string.Format("the phase authors %1 module(s) and the runtime set holds %2. A module that failed to clone is a rule that silently stopped applying",
				phase.m_aModules.Count().ToString(), runtimeCount.ToString());

		for (int i = 0; i < runtimeCount; i++)
		{
			OVT_BaseObjectiveModule runtime = instance.GetRuntimeModule(i);
			if (!runtime)
				return string.Format("runtime module %1 is null", i.ToString());

			foreach (OVT_BaseObjectiveModule template : phase.m_aModules)
			{
				if (runtime == template)
					return string.Format("runtime module %1 IS the plan's own template object rather than a clone of it. Two objectives running this plan would share one module and one set of latches, so the second would inherit the first's state - a raise module that already believes it sent a supply party sends nothing, forever",
						i.ToString());
			}
		}

		// The registered owner has to be one of those clones, not something built on the side.
		bool ownerIsRuntime = false;
		for (int j = 0; j < runtimeCount; j++)
		{
			if (instance.GetRuntimeModule(j) == owner)
				ownerIsRuntime = true;
		}

		if (!ownerIsRuntime)
			return "the registered asset owner is not one of the phase's runtime modules, so the director is holding a module nothing is ticking";

		return "";
	}
}
