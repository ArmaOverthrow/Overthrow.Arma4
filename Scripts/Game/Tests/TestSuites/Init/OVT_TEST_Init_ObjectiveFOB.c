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
//!   2. THE PHASE NUMBER. Both configs author OVT_ObjectiveConditionDeploymentModule with
//!      m_iRequiredPhase 2. Authored as 1 they are collected the instant the ramp advances into the
//!      phase they belong to - the forward base would be raised and then immediately taken away.
//!   3. THE SOURCE PROVIDER. The garrison is what makes the forward base a real supply source rather
//!      than scenery, and it does that through OVT_ObjectiveAnchorSourceProvider. Authored with the
//!      default provider instead, every garrison truck still drives from the rear and the base changes
//!      nothing about how the faction fights.
//!   4. THE TEARDOWN. Three exits share one path; a deployment left standing at a torn-down base is a
//!      force with no orders and a marker nothing will ever collect.
//!   5. THE CLONE. CloneModule is not chained and copies by hand - twenty-five lines here, thirteen
//!      inherited from the infantry module, ten from the insertion module and two of its own. A dropped
//!      line ships the class default on every deployment, forever.
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
//!   A2. m_iRequiredPhase changed from 2 to 1 on the forward-base config's objective condition. Fails
//!       on "must be scoped to phase 2".
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

		string phase = CheckObjectivePhase(config, OVT_EObjectivePhase.FOB);
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

		string phase = CheckObjectivePhase(config, OVT_EObjectivePhase.FOB);
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
	//! \param[in] config The config to walk.
	//! \param[in] required The phase it must be scoped to.
	//! \return An empty string when it is, or why it is not.
	protected string CheckObjectivePhase(notnull OVT_DeploymentConfig config, int required)
	{
		foreach (OVT_BaseDeploymentModule module : config.m_aModules)
		{
			OVT_ObjectiveConditionDeploymentModule condition = OVT_ObjectiveConditionDeploymentModule.Cast(module);
			if (!condition)
				continue;

			if (condition.m_iRequiredPhase != required)
				return string.Format("'%1' must be scoped to phase %2 and is scoped to %3 - a config scoped to the wrong phase is collected on the tick the ramp reaches the phase it belongs to",
					config.m_sDeploymentName, required.ToString(), condition.m_iRequiredPhase.ToString());

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
//!   D1. `if (!director.IsFOBUp()) return false;` deleted from ResolveForwardBase, so a director with
//!       no forward base answers the zero vector. Fails on "with no forward base the provider must
//!       fall through to the nearest controlled base".
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

		string failure = Check(director, provider, occupyingIndex);

		// ALWAYS, INCLUDING ON THE RED PATHS. Leaving a forward base recorded on the live director would
		// change what every later case in this suite sees.
		director.ResetObjective("OVT_TEST_Init_ObjectiveFOB_D finished", false);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective forward base: the garrison's source provider answers the forward base while one is standing and falls through to the nearest controlled base when none is");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] director The live director.
	//! \param[in] provider The provider under test.
	//! \param[in] occupyingIndex The occupying faction.
	//! \return An empty string when every claim held, or the first that did not.
	protected string Check(notnull OVT_ObjectiveDirectorComponent director, notnull OVT_ObjectiveAnchorSourceProvider provider, int occupyingIndex)
	{
		if (director.IsFOBUp())
			return "the director already reports a forward base before this case recorded one - some earlier case left one standing, and neither half of this claim can be trusted";

		// --- No forward base: fall through to the rear.
		vector fallback;
		bool answered = provider.ResolveSource(PROBE, occupyingIndex, fallback);

		if (!answered)
			return "with no forward base the provider must still find an origin - the occupying faction holds bases at initialisation, and a provider that answers false makes the insertion module register nothing at all";

		if (fallback == PROBE)
			return "with no forward base the provider answered the deployment's own position, which is not an origin at all";

		// --- Forward base standing: it wins.
		director.RecordFOB(PROBE, SOURCE, OVT_ObjectiveDirectorComponent.FOB_CONFIG);

		if (!director.IsFOBUp())
			return "RecordFOB() did not put the director's forward base up, so the preference cannot be asked about";

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
//!   F1. The area sweep removed from TearDownFOB(), leaving only the name-scoped carrier lookup. Fails
//!       on "the garrison deployment is still standing after the teardown".
//!   F2. The FOB_GARRISON_CONFIG arm of the sweep's name test removed. Fails on the same claim.
//!   F3. The TearDownFOB() call removed from ResetObjective(). Fails on "the forward-base deployment
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

		// The director is told a forward base is standing there - which is all the teardown needs to
		// find them - and then put through the machine's ONE reset path.
		director.RecordFOB(PROBE, PROBE, OVT_ObjectiveDirectorComponent.FOB_CONFIG);
		director.ResetObjective("OVT_TEST_Init_ObjectiveFOB_F is exercising the teardown", false);

		if (deployments.GetDeploymentNearPosition(OVT_ObjectiveDirectorComponent.FOB_CONFIG, PROBE, LOOKUP_RADIUS))
			return "the forward-base deployment is still standing after the teardown - a marker nothing will collect and a force with no orders, for the rest of the campaign";

		if (deployments.GetDeploymentNearPosition(OVT_ObjectiveDirectorComponent.FOB_GARRISON_CONFIG, PROBE, LOOKUP_RADIUS))
			return "the garrison deployment is still standing after the teardown - the sweep must take down BOTH configs, not just the one carrying the structure";

		if (director.IsFOBUp())
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
		if (director.IsFOBUp())
			return "the director already reports a forward base before this case recorded one - an earlier case left one standing and none of these rows can be trusted";

		// --- ROW 1: nothing to dismantle.
		string refusal;
		if (director.CanDismantleFOB(PROBE, refusal))
			return "with no forward base standing the dismantle must be refused - the structure is a persisted world entity and can briefly outlive the campaign's record of it, which is exactly when a player would be standing at a flag that means nothing";

		if (refusal == "")
			return "the no-forward-base refusal answered no localization key, so the held action would grey out with nothing said";

		// --- ROW 2: the caller is nowhere near it.
		director.RecordFOB(PROBE, PROBE, OVT_ObjectiveDirectorComponent.FOB_CONFIG);

		if (!director.IsFOBUp())
			return "RecordFOB() did not put the director's forward base up, so the remaining rows cannot be asked";

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
//! The raise module's clone carries every one of its twenty-five attributes.
//!
//! ⚠ CloneModule IS NOT CHAINED. Each subclass rebuilds the whole list BY HAND, so this module repeats
//! thirteen lines from OVT_InfantrySpawningDeploymentModule, ten from
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

		Print("Objective forward base: the raise module's clone carries all twenty-five attributes - thirteen inherited from the infantry module, ten from the insertion module, two of its own - and does not inherit a fired latch");

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
		subject.m_bAllowReinforcement = false;
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
		subject.m_bWalkWhenInsertionRefused = false;

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

		if (clone.m_rFOBPrefab != subject.m_rFOBPrefab) return "m_rFOBPrefab";
		if (clone.m_fRaiseOnFootRadius != subject.m_fRaiseOnFootRadius) return "m_fRaiseOnFootRadius";

		return "";
	}
}
