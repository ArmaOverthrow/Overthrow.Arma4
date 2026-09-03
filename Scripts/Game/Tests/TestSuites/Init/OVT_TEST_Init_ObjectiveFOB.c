//------------------------------------------------------------------------------------------------
//! TIER B - PHASE 2 OF THE RAMP: the occupying faction's forward operating base, where it meets
//! loaded configs and a live deployment framework.
//!
//! 🔴 THE HEADLINE CLAIM IS THAT A CAMPAIGN DOES NOT GROW A FORWARD BASE PER LOAD. The structure is a
//! persistence-tracked world entity: persistence puts it back before any deployment ticks, and
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
//! For every loaded difficulty preset the forward base's spend ceiling covers the forward base's
//! own deployment price, plus one garrison where the preset allows garrisons.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFOB_CeilingCanCoverTheForwardBase : SCR_AutotestCaseBase
{
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

		Print("Objective FOB: every loaded preset's spend ceiling covers the forward base itself (" + fobPrice.ToString() + ") with room for at least one garrison (" + garrisonPrice.ToString() + ")");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] config The config component holding the presets.
	//! \param[in] fobPrice What the forward base's own deployment costs.
	//! \param[in] garrisonPrice What one garrison costs.
	//! \return An empty string when every preset held, or the first that did not.
	protected string CheckEveryPreset(notnull OVT_OverthrowConfigComponent config, int fobPrice, int garrisonPrice)
	{
		foreach (OVT_DifficultySettings preset : config.m_aDifficultyPresets)
		{
			if (!preset)
				continue;

			string presetName = preset.name;

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

//------------------------------------------------------------------------------------------------
//! 🔴 THE FORWARD BASE COMES BACK ON LOAD. Play-test 2026-08-23: it did not, and the garrison stood
//! around a flagpole that was no longer there.
//!
//! The raise module tracks the structure (OVT_PersistenceTracking.Track) and its header claimed that
//! was enough. It is not. An entity only RETURNS when the PersistenceConfig it MATCHES has SelfSpawn
//! set, and the four rules in Configs/Systems/Persistence/Overthrow.conf match on component class -
//! OVT_PlaceableComponent, OVT_BuildableComponent, OVT_DeploymentComponent, OVT_ResourcePileComponent.
//! The FOB prefab carried none of them, so it was tracked, written and never spawned back, while the
//! garrison deployment (OVT_DeploymentComponent) returned every time. The fix is a fifth rule on
//! OVT_OccupyingFlagComponent; this case is what stops the prefab, the component or the rule from
//! drifting apart again.
//!
//! ⚠ WHY THIS CASE MAY SPAWN THE STRUCTURE WHEN THE REST OF THE FILE MAY NOT. The file's rule is that
//! no case RAISES a forward base - a raise leaves a persisted flagpole in the shared initialisation
//! world. This one stands the prefab up as a PROBE and takes it back down on every exit path,
//! Untrack(keepData: false) BEFORE the delete, so no record is left to spawn on the next run.
//!
//! PROVEN ABLE TO FAIL: the OVT_OccupyingFlagComponent configuration removed from Overthrow.conf -
//! fails on "matched persistence configuration does NOT self-spawn".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveFOB_MStructureConfigSelfSpawns : SCR_AutotestCaseBase
{
	//! How far from the fixture town to stand the probe.
	static const float PROBE_OFFSET = 180;

	//! Frames allowed for a town to appear and for tracking to be resolved. Registration is LAZY, so
	//! IsTracked can legitimately read false on the frame the entity was spawned.
	static const int SETTLE_FRAMES = 30;

	protected int m_iPhase;
	protected int m_iFrames;
	protected IEntity m_Structure;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == 0)
			return Arrange();

		return AwaitTracking();
	}

	//------------------------------------------------------------------------------------------------
	//! Stands the AUTHORED structure prefab near the fixture town - read off the config, not a constant
	//! here, so a modded prefab is the thing tested.
	//! \return True when the case is already over.
	protected bool Arrange()
	{
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (!deployments)
		{
			SetFailure("OVT_Global.GetDeploymentManager() is null - the forward-base config cannot be resolved in this world");
			return true;
		}

		OVT_DeploymentConfig config = deployments.FindConfigByName(OVT_ObjectiveDirectorComponent.FOB_CONFIG);
		if (!config || !config.m_aModules)
		{
			SetFailure("'%1' is not registered, so there is no authored structure prefab to test", OVT_ObjectiveDirectorComponent.FOB_CONFIG);
			return true;
		}

		ResourceName prefab;
		foreach (OVT_BaseDeploymentModule module : config.m_aModules)
		{
			OVT_FOBRaiseSpawningDeploymentModule raise = OVT_FOBRaiseSpawningDeploymentModule.Cast(module);
			if (raise)
				prefab = raise.GetFOBPrefab();
		}

		if (prefab.IsEmpty())
		{
			SetFailure("'%1' authors no structure prefab", OVT_ObjectiveDirectorComponent.FOB_CONFIG);
			return true;
		}

		vector anchor = OVT_TEST_VirtualizationFixture.PickPosition();
		if (anchor == vector.Zero)
		{
			m_iFrames++;
			if (m_iFrames <= SETTLE_FRAMES)
				return false;

			SetFailure("No town is registered after %1 frames, so there is nowhere to stand the probe structure", m_iFrames.ToString());
			return true;
		}

		m_Structure = OVT_Global.SpawnEntityPrefab(prefab, anchor + Vector(PROBE_OFFSET, 0, PROBE_OFFSET));
		if (!m_Structure)
		{
			SetFailure("SpawnEntityPrefab() produced nothing from the authored forward-base prefab %1", prefab);
			return true;
		}

		if (!OVT_ComponentFinder<OVT_OccupyingFlagComponent>.Find(m_Structure))
		{
			CleanUp();
			SetFailure("The authored forward-base prefab carries NO OVT_OccupyingFlagComponent. That component is what the persistence rule matches on, so the structure is saved and never spawned back: on the next load the garrison stands around a base that is not there and the dismantle action goes with it");
			return true;
		}

		// The raise path's own line, so this probe is registered exactly as a real forward base is.
		OVT_PersistenceTracking.Track(m_Structure);

		m_iPhase = 1;
		m_iFrames = 0;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits out lazy registration, then asserts the matched configuration self-spawns.
	//! \return True when the case is finished on this frame.
	protected bool AwaitTracking()
	{
		m_iFrames++;

		if (!m_Structure)
		{
			SetFailure("The probe structure disappeared before its persistence configuration could be read");
			return true;
		}

		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetByEntityWorld(m_Structure);
		if (!persistence)
		{
			CleanUp();
			SetFailure("This world has no persistence system, so nothing about save behaviour can be asserted in it");
			return true;
		}

		if (!persistence.IsTracked(m_Structure))
		{
			if (m_iFrames < SETTLE_FRAMES)
				return false;

			CleanUp();
			SetFailure("The forward-base structure is still not tracked after %1 frames - OVT_PersistenceTracking.Track() no longer takes on it, so it is never written to a save point at all", m_iFrames.ToString());
			return true;
		}

		EntityPersistenceConfig config = EntityPersistenceConfig.Cast(persistence.GetConfig(m_Structure));
		if (!config)
		{
			CleanUp();
			SetFailure("GetConfig() handed back no entity configuration for the tracked forward-base structure");
			return true;
		}

		if (!config.m_bSelfSpawn)
		{
			CleanUp();
			SetFailure("The forward-base structure's matched persistence configuration does NOT self-spawn. Tracking alone only WRITES the record: on the next load the flagpole is gone, its garrison deployment (which does self-spawn) is still standing on the site, the director's asset record still says the base is up, and the dismantle action that ends the objective went with the entity. Add the OVT_OccupyingFlagComponent configuration back to Configs/Systems/Persistence/Overthrow.conf");
			return true;
		}

		CleanUp();

		Print("Forward base: the authored structure prefab carries OVT_OccupyingFlagComponent and matches a self-spawning persistence configuration, so it comes back on load");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the probe's record and deletes it. Called on every exit path.
	//!
	//! ⚠ UNTRACK BEFORE DELETE, AND keepData FALSE. A tracked structure left behind by this case would
	//! be spawned back into the shared initialisation world on the next run, which is the one thing
	//! this file's header forbids.
	protected void CleanUp()
	{
		if (!m_Structure)
			return;

		OVT_PersistenceTracking.Untrack(m_Structure, false);

		SCR_EntityHelper.DeleteEntityAndChildren(m_Structure);
		m_Structure = null;
	}
}
