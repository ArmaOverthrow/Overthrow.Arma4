//------------------------------------------------------------------------------------------------
//! TIER B - THE PHASE-1 TOWN OPERATIONS where they meet loaded configs: the two new deployment
//! configs and the four registry rungs that carry the ramp, the two new behaviour decisions, the
//! three new clones, and the appended town modifier.
//!
//! WHAT THIS TIER CAN SEE THAT NOTHING ELSE CAN. Five of the things this phase ships are STRINGS
//! matched at runtime, and not one of them fails to parse when it is wrong:
//!
//!   1. FIVE DEPLOYMENT CONFIG NAMES. The director resolves a rung with FindConfigByName(), matches
//!      live deployments back to it by name for the concurrency cap, and stores the name in its
//!      teardown ledger. A name changed in overthrowDeployments.conf and not in HARASSMENT_LADDER
//!      stops the entire ramp with one WARNING line per in-game minute as its only symptom.
//!   2. FOUR REGISTRY GROUP NAMES, one per rung, resolved per faction at spawn time. `rifle_squad`
//!      in particular did not exist on the US side until this phase.
//!   3. THE MODULE ORDER INSIDE EACH CONFIG. `.conf` files cannot carry comments, so the constraint
//!      that a behaviour module completing a mission must be authored BEFORE the reinforcement module
//!      is recorded only in class headers and enforced only here. Authored the other way round, a
//!      completed hold has its force rebought in the same pass that ended the mission.
//!   4. ONE SUPPORT MODIFIER NAME, plus the rule that its config entry is the LAST one. m_iIndex is
//!      the POSITIONAL index in m_aModifiers and travels in the replicated per-town modifier lists,
//!      so an entry inserted anywhere but the end re-labels every modifier in every live save.
//!   5. THE INVERSION KNOB ON THE RECAPTURE CONFIG. m_bRequireControl 0 means "deploy only while we
//!      do NOT hold this tower"; authored as 1 by mistake, the recapture team would be sent only to
//!      towers the occupying faction already owns, which is a config that never does anything.
//!
//! ⚠ NOTHING HERE CONSTRUCTS A DEPLOYMENT. OVT_DeploymentComponent.InitializeDeployment() arms a
//! repeating 8-12 s UpdateDeployment whose first tick converges every spawning module - for these
//! configs that means resolving an origin, claiming a convoy slot, putting a real truck on a real road
//! and registering real groups at a 100 000 m ring with the autotest camera inside it. Every subject
//! below is a loaded config object, a bare `new` module with no parent deployment, or one read off a
//! live manager. (If a later phase does need one: SetSpawnedUnitsEliminated(true) on the DEPLOYMENT
//! and on EVERY spawning module, in the creating frame, before anything can tick.)
//!
//! ⚠ NOTHING HERE DRIVES THE DIRECTOR EITHER. TickHarassment() spends the occupying faction's real
//! pool and puts real deployments on the shared initialisation world's map. The parts of it worth
//! pinning are the rung table (case A, against the live registry) and the two behaviour decisions
//! (case E, driven directly); the spending path itself is a play-test item and says so in context.md.
//!
//! ⚠ CASE ORDER: cases run alphabetically by class name and none of these writes anything, so the
//! order is free. The names are still prefixed A/C/E/M for readability of a run log.
//!
//! No polling, no waiting, no maxAttempts: every subject is a synchronous read or a hand-built object.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Every rung of the ramp, and the recapture config, is registered, valid, correctly ordered and
//! names a group both factions can actually field.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   A1. `m_sDeploymentName "Objective Harassment (Rifle Squad)"` misspelled in
//!       overthrowDeployments.conf. Fails on "rung 2 ... is not registered".
//!   A2. The rifle-squad rung's `m_sGroupType` left as the inherited `light_patrol`. Fails on "two
//!       rungs of the ladder field the same group".
//!   A3. `rifle_squad` removed from US_OverthrowData.conf. Fails on "the US registry cannot field it".
//!   A4. The recapture config's reinforcement module moved ABOVE the recapture behaviour module.
//!       Fails on "must be authored before the reinforcement module".
//!   A5. The recapture config's `m_bRequireControl` set to 1. Fails on "must be authored with
//!       m_bRequireControl 0".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveOperations_ARampConfigsResolveAndAreOrdered : SCR_AutotestCaseBase
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

		string failure = CheckLadder(deployments);
		if (failure == "")
			failure = CheckRecapture(deployments);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective operations: four harassment rungs and the tower recapture config are registered, valid, ordered behaviour-before-reinforcement, and every rung fields a distinct group both factions have");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] deployments The deployment framework.
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckLadder(notnull OVT_DeploymentManagerComponent deployments)
	{
		array<string> groupTypes = new array<string>();

		int rungs = OVT_ObjectiveDirectorComponent.HARASSMENT_LADDER.Count();
		if (rungs < 2)
			return "the harassment ladder has fewer than two rungs - there is no ramp";

		for (int rung = 0; rung < rungs; rung++)
		{
			string name = OVT_ObjectiveDirectorComponent.HARASSMENT_LADDER[rung];

			OVT_DeploymentConfig config = deployments.FindConfigByName(name);
			if (!config)
				return string.Format("rung %1 of the harassment ladder, '%2', is not registered in overthrowDeployments.conf - the director would send nothing at that rung",
					rung.ToString(), name);

			if (!config.IsValidConfig())
				return string.Format("the harassment rung '%1' does not validate", name);

			if (!config.CanUseLocationType(OVT_LocationTypeFlag.TOWN))
				return string.Format("the harassment rung '%1' cannot be used at a TOWN, which is the only place the director sends one", name);

			string ordering = CheckBehaviourBeforeReinforcement(config, OVT_TownHarassmentBehaviorDeploymentModule);
			if (ordering != "")
				return ordering;

			// The force has to WALK TO THE TOWN CENTRE. A patrol behaviour module would answer
			// BuildVirtualPlan() with a plan of its own and pre-empt the insertion module's cycling
			// march onto the deployment position, and a DEFEND one would park the group where it
			// landed - which for an insertion is a road 300 m short of the town.
			foreach (OVT_BaseDeploymentModule module : config.m_aModules)
			{
				if (OVT_PatrolBehaviorDeploymentModule.Cast(module))
					return string.Format("the harassment rung '%1' authors a patrol behaviour module, which would replace the movable march plan the force needs to reach the town centre", name);
			}

			OVT_InsertionSpawningDeploymentModule insertion = FindInsertionModule(config);
			if (!insertion)
				return string.Format("the harassment rung '%1' has no insertion spawning module - its force would appear at the objective out of thin air", name);

			if (!insertion.m_Source)
				return string.Format("the harassment rung '%1' authors no source provider, so its insertion module registers nothing at all", name);

			if (groupTypes.Contains(insertion.m_sGroupType))
				return string.Format("two rungs of the ladder field the same group ('%1') - rung %2 is not an escalation",
					insertion.m_sGroupType, rung.ToString());

			groupTypes.Insert(insertion.m_sGroupType);

			string resolvable = CheckGroupResolves(insertion.m_sGroupType);
			if (resolvable != "")
				return resolvable;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] deployments The deployment framework.
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckRecapture(notnull OVT_DeploymentManagerComponent deployments)
	{
		OVT_DeploymentConfig config = deployments.FindConfigByName(OVT_ObjectiveDirectorComponent.TOWER_RECAPTURE_CONFIG);
		if (!config)
			return string.Format("'%1' is not registered in overthrowDeployments.conf - the occupying faction can never take a radio tower back",
				OVT_ObjectiveDirectorComponent.TOWER_RECAPTURE_CONFIG);

		if (!config.IsValidConfig())
			return "the tower recapture config does not validate";

		if (!config.CanUseLocationType(OVT_LocationTypeFlag.RADIO_TOWER))
			return "the tower recapture config cannot be used at a RADIO_TOWER";

		string ordering = CheckBehaviourBeforeReinforcement(config, OVT_TowerRecaptureBehaviorDeploymentModule);
		if (ordering != "")
			return ordering;

		OVT_InsertionSpawningDeploymentModule insertion = FindInsertionModule(config);
		if (!insertion)
			return "the tower recapture config has no insertion spawning module";

		string resolvable = CheckGroupResolves(insertion.m_sGroupType);
		if (resolvable != "")
			return resolvable;

		// THE INVERSION KNOB. m_bRequireControl 0 is what makes the existing tower-control condition
		// mean "deploy only while we do NOT hold this tower", and what collects the deployment the
		// moment the recapture behaviour flips it.
		bool foundInversion = false;
		foreach (OVT_BaseDeploymentModule module : config.m_aModules)
		{
			OVT_RadioTowerControlConditionDeploymentModule control = OVT_RadioTowerControlConditionDeploymentModule.Cast(module);
			if (!control)
				continue;

			foundInversion = true;

			if (control.m_bRequireControl)
				return "the tower recapture config's radio tower control condition must be authored with m_bRequireControl 0 - authored as 1 it sends recapture teams only to towers the occupying faction already holds";
		}

		if (!foundInversion)
			return "the tower recapture config authors no radio tower control condition, so nothing collects the team once the tower flips";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] config The config to walk.
	//! \param[in] behaviourType The mission behaviour that must come first.
	//! \return An empty string when the order is right, or why it is not.
	protected string CheckBehaviourBeforeReinforcement(notnull OVT_DeploymentConfig config, typename behaviourType)
	{
		int behaviourIndex = -1;
		int reinforcementIndex = -1;

		foreach (int index, OVT_BaseDeploymentModule module : config.m_aModules)
		{
			if (!module)
				continue;

			if (behaviourIndex == -1 && module.Type().IsInherited(behaviourType))
				behaviourIndex = index;

			if (reinforcementIndex == -1 && OVT_ReinforcementBehaviorDeploymentModule.Cast(module))
				reinforcementIndex = index;
		}

		if (behaviourIndex == -1)
			return string.Format("'%1' authors no %2 - it has no mission at all", config.m_sDeploymentName, behaviourType.ToString());

		if (reinforcementIndex == -1)
			return string.Format("'%1' authors no reinforcement module, so nothing ever collects it when the objective moves", config.m_sDeploymentName);

		if (behaviourIndex > reinforcementIndex)
			return string.Format("'%1' authors its mission behaviour AFTER the reinforcement module - it must be authored before it, or a completed mission has its force rebought in the same pass that ended it",
				config.m_sDeploymentName);

		return "";
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
	//! That BOTH shipped factions can field a group name, and that the prefab behind it actually loads.
	//!
	//! ⚠ BOTH FACTIONS, ALWAYS. Which faction occupies is a campaign setting, so a name authored for
	//! only one of them is a config that works on half the servers.
	//! \param[in] groupType The registry name.
	//! \return An empty string when both factions field it, or why one does not.
	protected string CheckGroupResolves(string groupType)
	{
		if (groupType == "")
			return "a rung of the ladder authors an empty group type";

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
				return string.Format("the %1 registry cannot field '%2' - a config names a group that faction has no entry for", factionKey, groupType);

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
//! Both new behaviour decisions reach their fire condition from a driven sequence, pause rather than
//! reset on an interruption, and fire exactly once.
//!
//! ⚠ THE "EXACTLY ONCE" HALF IS THE HALF NOTHING ELSE WOULD CATCH. Both modules are asked again on
//! every deployment update for as long as the deployment exists, so without the latch a completed
//! hold would re-stack the town's support debuff every ten seconds and a recaptured tower would
//! re-notify every ten seconds. Neither is a script error and neither shows up until a play-test.
//!
//! ⚠ THE "PAUSE, NEVER RESET" HALF IS A DESIGN CLAIM, NOT AN IMPLEMENTATION DETAIL. If an interruption
//! rolled the clock back to full, a defender walking past a town once every few minutes would make the
//! mechanic impossible to finish - which reads in play as "the occupying faction never does anything"
//! and has no other diagnosis.
//!
//! Both subjects are bare `new` objects with NO parent deployment: the decision methods take every
//! input as an argument precisely so this needs no marker, no town, no tower and no AI.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   E1. `if (m_bHoldFired) return false;` deleted from EvaluateHold. Fails on "a completed hold must
//!       not fire a second time".
//!   E2. `if (aliveInside < 1) return false;` deleted. Fails on "an empty circle must not advance the
//!       hold".
//!   E3. `if (enemyPresent) return false;` deleted. Fails on "a contested hold must not advance".
//!   E4. The interrupted branch changed to reset ticksLeft to its starting value. Fails on "an
//!       interruption must PAUSE the clock, not reset it".
//!   E5. `if (towerFaction == myFaction) return false;` deleted from EvaluateRecapture. Fails on "a
//!       tower the faction already holds must never be recaptured".
//!   E6. The decrement in EvaluateRecapture moved above the holding test. Fails on "an unheld tower
//!       must not advance the recapture clock".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveOperations_EHoldDecisionsFireOnceAndPause : SCR_AutotestCaseBase
{
	//! Occupying and resistance faction indices for the recapture rows. Any two different integers
	//! make the claim; real indices would only couple the case to campaign setup.
	static const int MY_FACTION = 7;
	static const int ENEMY_FACTION = 8;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string failure = CheckHold();
		if (failure == "")
			failure = CheckRecapture();

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective hold decisions: both clocks run only while the place is held and unopposed, pause rather than reset on an interruption, and fire exactly once");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckHold()
	{
		OVT_TownHarassmentBehaviorDeploymentModule harassment = new OVT_TownHarassmentBehaviorDeploymentModule();

		int ticks = 3;

		// --- Nobody there yet: the force is still on the road.
		if (harassment.EvaluateHold(0, false, ticks))
			return "an empty circle must not complete a hold";

		if (ticks != 3)
			return string.Format("an empty circle must not advance the hold: %1 tick(s) left, expected 3", ticks.ToString());

		// --- Contested: men are there, so is a player.
		if (harassment.EvaluateHold(4, true, ticks))
			return "a contested hold must not complete";

		if (ticks != 3)
			return string.Format("a contested hold must not advance: %1 tick(s) left, expected 3", ticks.ToString());

		// --- Held and unopposed: one tick.
		if (harassment.EvaluateHold(4, false, ticks))
			return "a hold with three ticks left must not complete on the first of them";

		if (ticks != 2)
			return string.Format("a held, unopposed tick must advance the clock by exactly one: %1 tick(s) left, expected 2", ticks.ToString());

		// --- Interrupted mid-hold. THE CLOCK MUST PAUSE, NOT RESET.
		if (harassment.EvaluateHold(0, false, ticks))
			return "the force being wiped mid-hold must not complete it";

		if (ticks != 2)
			return string.Format("an interruption must PAUSE the clock, not reset it: %1 tick(s) left, expected 2", ticks.ToString());

		// --- Run it out.
		if (harassment.EvaluateHold(4, false, ticks))
			return "a hold with two ticks left must not complete on the first of them";

		if (!harassment.EvaluateHold(4, false, ticks))
			return "a hold whose last tick was served must complete";

		if (!harassment.HasHoldFired())
			return "a completed hold must latch";

		// --- And never again, however the inputs read afterwards.
		if (harassment.EvaluateHold(4, false, ticks))
			return "a completed hold must not fire a second time - it would re-stack the town's support debuff every ten seconds";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckRecapture()
	{
		OVT_TowerRecaptureBehaviorDeploymentModule recapture = new OVT_TowerRecaptureBehaviorDeploymentModule();

		int ticks = 2;

		// --- Already ours. Nothing to take, and nothing to spend a latch on.
		if (recapture.EvaluateRecapture(true, false, MY_FACTION, MY_FACTION, ticks))
			return "a tower the faction already holds must never be recaptured";

		if (ticks != 2)
			return string.Format("a tower the faction already holds must not advance the clock: %1 tick(s) left, expected 2", ticks.ToString());

		// --- Nobody at the tower.
		if (recapture.EvaluateRecapture(false, false, ENEMY_FACTION, MY_FACTION, ticks))
			return "an unheld tower must not be recaptured";

		if (ticks != 2)
			return string.Format("an unheld tower must not advance the recapture clock: %1 tick(s) left, expected 2", ticks.ToString());

		// --- A player standing on it.
		if (recapture.EvaluateRecapture(true, true, ENEMY_FACTION, MY_FACTION, ticks))
			return "a contested tower must not be recaptured";

		if (ticks != 2)
			return string.Format("a contested tower must not advance the recapture clock: %1 tick(s) left, expected 2", ticks.ToString());

		// --- Held, unopposed, enemy-owned.
		if (recapture.EvaluateRecapture(true, false, ENEMY_FACTION, MY_FACTION, ticks))
			return "a recapture with two ticks left must not complete on the first of them";

		if (ticks != 1)
			return string.Format("a held, unopposed tick must advance the recapture clock by exactly one: %1 tick(s) left, expected 1", ticks.ToString());

		if (!recapture.EvaluateRecapture(true, false, ENEMY_FACTION, MY_FACTION, ticks))
			return "a recapture whose last tick was served must complete";

		if (!recapture.HasCaptureFired())
			return "a completed recapture must latch";

		if (recapture.EvaluateRecapture(true, false, ENEMY_FACTION, MY_FACTION, ticks))
			return "a completed recapture must not fire a second time - it would re-notify every ten seconds";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! All three new modules clone every authored attribute, and neither behaviour module clones its
//! fired-once latch.
//!
//! ⚠ CloneModule COPIES BY HAND AND IS NOT CHAINED. A forgotten line does not warn, does not log and
//! does not fail to parse: it ships the CLASS DEFAULT on every deployment, forever. That is how
//! m_fMaxCruiseSpeed was lost on the vehicle module for a whole release.
//!
//! ⚠ AND THE LATCH MUST NOT BE COPIED. A clone is a fresh deployment's module and has harassed
//! nobody and captured nothing; a copied latch would ship a template that had fired once and produce
//! operations that can never complete.
//!
//! Every field is set to a value that is NOT the class default, so a dropped line reads as the
//! default and fails rather than passing by coincidence.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   C1. `clone.m_fHoldRadius = m_fHoldRadius;` deleted from the harassment clone.
//!   C2. `clone.m_iRequiredPhase = m_iRequiredPhase;` deleted from the objective condition clone.
//!   C5. `clone.m_iThroughPhase = m_iThroughPhase;` deleted from the objective condition clone. Fails
//!       on "dropped m_iThroughPhase" - every range would collapse to a single phase, which is the
//!       2026-08-19 deadlock restored one clone at a time.
//!   C3. `clone.m_fMaxDistance = m_fMaxDistance;` deleted from the recapture clone.
//!   C4. `clone.m_bHoldFired = m_bHoldFired;` ADDED to the harassment clone. Fails on "a clone must
//!       not inherit a fired latch".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveOperations_CloneFidelity : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string failure = CheckHarassmentClone();
		if (failure == "")
			failure = CheckRecaptureClone();
		if (failure == "")
			failure = CheckConditionClone();

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective operation modules: the harassment, recapture and objective condition clones all carry every authored attribute, and neither latch is copied");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckHarassmentClone()
	{
		OVT_TownHarassmentBehaviorDeploymentModule source = new OVT_TownHarassmentBehaviorDeploymentModule();

		source.m_sModuleName = "fixture harassment";
		source.m_fHoldRadius = 137;
		source.m_iHoldSeconds = 271;
		source.m_sSupportModifier = "FixtureModifier";

		// Fire the latch, so the clone has something it could wrongly inherit.
		int ticks = 1;
		source.EvaluateHold(1, false, ticks);

		OVT_TownHarassmentBehaviorDeploymentModule clone = OVT_TownHarassmentBehaviorDeploymentModule.Cast(source.CloneModule());
		if (!clone)
			return "the harassment module's CloneModule did not return a module of its own type";

		if (clone.m_sModuleName != source.m_sModuleName)
			return "the harassment clone dropped m_sModuleName";

		if (clone.m_fHoldRadius != source.m_fHoldRadius)
			return string.Format("the harassment clone dropped m_fHoldRadius: %1, expected %2", clone.m_fHoldRadius.ToString(), source.m_fHoldRadius.ToString());

		if (clone.m_iHoldSeconds != source.m_iHoldSeconds)
			return string.Format("the harassment clone dropped m_iHoldSeconds: %1, expected %2", clone.m_iHoldSeconds.ToString(), source.m_iHoldSeconds.ToString());

		if (clone.m_sSupportModifier != source.m_sSupportModifier)
			return "the harassment clone dropped m_sSupportModifier - the hold would complete and the town's support would never move";

		if (!source.HasHoldFired())
			return "the fixture failed to fire the harassment latch, so the next claim would pass vacuously";

		if (clone.HasHoldFired())
			return "a clone must not inherit a fired latch - every deployment built from that template could never complete its hold";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckRecaptureClone()
	{
		OVT_TowerRecaptureBehaviorDeploymentModule source = new OVT_TowerRecaptureBehaviorDeploymentModule();

		source.m_sModuleName = "fixture recapture";
		source.m_fMaxDistance = 417;
		source.m_fHoldRadius = 63;
		source.m_iHoldSeconds = 519;

		int ticks = 1;
		source.EvaluateRecapture(true, false, 8, 7, ticks);

		OVT_TowerRecaptureBehaviorDeploymentModule clone = OVT_TowerRecaptureBehaviorDeploymentModule.Cast(source.CloneModule());
		if (!clone)
			return "the recapture module's CloneModule did not return a module of its own type";

		if (clone.m_sModuleName != source.m_sModuleName)
			return "the recapture clone dropped m_sModuleName";

		if (clone.m_fMaxDistance != source.m_fMaxDistance)
			return string.Format("the recapture clone dropped m_fMaxDistance: %1, expected %2", clone.m_fMaxDistance.ToString(), source.m_fMaxDistance.ToString());

		if (clone.m_fHoldRadius != source.m_fHoldRadius)
			return string.Format("the recapture clone dropped m_fHoldRadius: %1, expected %2", clone.m_fHoldRadius.ToString(), source.m_fHoldRadius.ToString());

		if (clone.m_iHoldSeconds != source.m_iHoldSeconds)
			return string.Format("the recapture clone dropped m_iHoldSeconds: %1, expected %2", clone.m_iHoldSeconds.ToString(), source.m_iHoldSeconds.ToString());

		if (!source.HasCaptureFired())
			return "the fixture failed to fire the recapture latch, so the next claim would pass vacuously";

		if (clone.HasCaptureFired())
			return "a clone must not inherit a fired latch - every recapture team built from that template could never take its tower";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckConditionClone()
	{
		OVT_ObjectiveConditionDeploymentModule source = new OVT_ObjectiveConditionDeploymentModule();

		source.m_sModuleName = "fixture condition";
		source.m_iRequiredPhase = OVT_EObjectivePhase.FOB;

		// ⚠ DELIBERATELY DIFFERENT FROM m_iRequiredPhase, and deliberately not a value any shipped config
		// authors. A clone that copied the first phase into both fields, or dropped the upper bound and
		// let it collapse back onto the first, would pass an assertion that used the same number twice.
		source.m_iThroughPhase = OVT_EObjectivePhase.COUNTER_QRF;
		source.m_fMaxDistanceFromObjective = 933;

		OVT_ObjectiveConditionDeploymentModule clone = OVT_ObjectiveConditionDeploymentModule.Cast(source.CloneModule());
		if (!clone)
			return "the objective condition module's CloneModule did not return a module of its own type";

		if (clone.m_sModuleName != source.m_sModuleName)
			return "the objective condition clone dropped m_sModuleName";

		if (clone.m_iRequiredPhase != source.m_iRequiredPhase)
			return string.Format("the objective condition clone dropped m_iRequiredPhase: %1, expected %2 - every objective deployment would believe it belonged to the harassment phase",
				clone.m_iRequiredPhase.ToString(), source.m_iRequiredPhase.ToString());

		if (clone.m_iThroughPhase != source.m_iThroughPhase)
			return string.Format("the objective condition clone dropped m_iThroughPhase: %1, expected %2 - every range would collapse to a single phase, which is the 2026-08-19 deadlock restored one clone at a time: the ramp's operations would be collected on the promotion tick and the counter-attack would be unreachable again",
				clone.m_iThroughPhase.ToString(), source.m_iThroughPhase.ToString());

		if (clone.ResolveThroughPhase() != source.ResolveThroughPhase())
			return string.Format("the objective condition clone reports a different span from its source: up to %1, expected up to %2",
				clone.ResolveThroughPhase().ToString(), source.ResolveThroughPhase().ToString());

		if (clone.m_fMaxDistanceFromObjective != source.m_fMaxDistanceFromObjective)
			return string.Format("the objective condition clone dropped m_fMaxDistanceFromObjective: %1, expected %2 - it would refuse every position but the objective's exact centre",
				clone.m_fMaxDistanceFromObjective.ToString(), source.m_fMaxDistanceFromObjective.ToString());

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! THE PHASE-1 REGRESSION GUARD. Counting a harassment success does not move the phase, and a tick
//! does not open the forward-base gate for a town this ramp never actually debuffed.
//!
//! ⚠ THIS CASE EXISTS BECAUSE THE FIRST CUT OF PHASE 5 BROKE TWO EARLIER PHASES' CONTRACTS AT ONCE,
//! with one mistake, and neither breakage was visible from the code that caused it:
//!
//!   THE MISTAKE. TickHarassment() gained a real Phase-2 gate check, and OnHarassmentSuccess() gained
//!   one too (T5.8 asked for it). The gate was the plan's diagram taken literally - "town: support
//!   < 50 %" and nothing else - so it fired on the FIRST tick of the phase for any town already under
//!   the threshold, and it fired from a public counter-bumping method that is not a tick at all.
//!
//!   WHAT IT LOOKED LIKE. Two failures in two different suites, neither of them near the new code:
//!     - Init "an unfrozen tick must serve exactly one round off the phase timeout: planted 50, read
//!       back 240" - the gate fired on the driven tick, EnterPhase() legitimately re-armed the phase
//!       timeout, and 240 is a fresh harassment timeout rather than a failed decrement. That is D4:
//!       every timer is a tick counter that only a tick may move.
//!     - Persistence "the restored objective is in phase 2, not the harassment phase it was saved in"
//!       - the fixture's three OnHarassmentSuccess() calls promoted the objective to FOB BEFORE the
//!       save, so FOB was what got saved. That is G6: the whole objective survives a save unchanged.
//!
//!   ⚠ THE SUCCESS COUNTER IS NOT A SUFFICIENT GUARD, WHICH IS WHY THIS CASE TESTS THE DEBUFF. The
//!   obvious fix - "the gate also needs at least one completed operation" - repairs the Init case and
//!   NOT the Persistence one, because that fixture bumps the counter to three itself. The counter
//!   records that operations were REPORTED; the modifier on the town records that one actually
//!   HAPPENED there. Only the second is a fact about the world.
//!
//! ⚠ THE OPERATION COUNTDOWN IS PLANTED HIGH ON PURPOSE, and any future case that drives the director
//! must do the same. CommitObjective() enters a phase, which arms nextOpTicks to ZERO - so a tick
//! taken straight afterwards reaches the spend path and puts a REAL deployment, a real truck and real
//! groups into the shared initialisation world, permanently debiting the occupying faction's pool.
//! Both pre-existing DirectorTick() callers plant a countdown; so does this one.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   G1. `CheckHarassmentGate()` re-added to OnHarassmentSuccess(). Fails on "counting a harassment
//!       success must not change the phase".
//!   G2. The `ObjectiveTownCarriesHarassmentDebuff()` conjunct deleted from CheckHarassmentGate().
//!       Fails on "a tick must not open the forward-base gate for a town this ramp never debuffed"
//!       (and, on a town whose support is already low, on the phase-timeout claim too).
//!
//! ⚠ ONE LINE WAS ADDED TO THIS FIXTURE ON 2026-08-19 AND IT IS NOT PADDING - see the comment at the
//! re-plant, below the counting half. The phase timeout became an IDLE clock that a tick re-arms when
//! it sees a success counter move, so a fixture that counts three successes and then drives a tick is
//! now arranging PROGRESS, not idleness. The clock is re-planted between the two halves to say "this is
//! the state as of now". Both original assertions survive unchanged and both keep their full strength:
//! counting still moves no timer, and a value above the planted one after the tick still means exactly
//! one thing.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveOperations_GateNeedsTheRampsOwnDebuff : SCR_AutotestCaseBase
{
	//! Planted phase timeout. Deliberately not a value any phase entry produces, so a re-arm is
	//! distinguishable from a decrement.
	static const int PLANTED_PHASE_TICKS = 61;

	//! Planted operation cadence. HIGH ON PURPOSE - see the class header. Nothing may be spent.
	static const int PLANTED_OP_TICKS = 44;

	static const int SUCCESSES_TO_COUNT = 3;

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

		vector fixturePosition = towns.m_Towns[0].location;

		director.CommitObjective(OVT_EObjectiveKind.TOWN, fixturePosition, "gate fixture");
		director.SetPhaseTimeout(PLANTED_PHASE_TICKS);
		director.SetOperationCountdown(PLANTED_OP_TICKS);

		// --- HALF ONE: counting is not deciding.
		for (int i = 0; i < SUCCESSES_TO_COUNT; i++)
		{
			director.OnHarassmentSuccess();
		}

		int phaseAfterCounting = director.GetPhase();
		int successes = director.GetHarassmentSuccesses();
		int phaseTicksAfterCounting = director.GetPhaseTicks();

		// ⚠ THE CLOCK IS RE-PLANTED BEFORE THE TICK, AND THIS LINE IS NOT PADDING (added 2026-08-19 with
		// the idle-clock rework). The phase timeout is now an IDLE clock: a tick that sees a success
		// counter move since the clock was last set treats it as an operation REPORTING and re-arms the
		// clock to its full authored budget, which is the correct product behaviour and would read here as
		// "240 instead of 60". The three successes above are FIXTURE ARRANGEMENT, not work the ramp did
		// this minute, and SetPhaseTimeout() is how a caller says so - it plants "this much patience
		// remains as of now" and re-baselines the progress marks with it.
		//
		// ⚠ IT DOES NOT WEAKEN THE ASSERTION BELOW; it is what keeps it sharp. That assertion is a canary
		// for an accidental EnterPhase(), and a canary that could also be tripped by legitimate progress
		// would say nothing. With the marks baselined, a value above the planted one still means exactly
		// one thing: something re-armed the clock that had no business doing so.
		director.SetPhaseTimeout(PLANTED_PHASE_TICKS);

		// --- HALF TWO: a tick does not open the gate either, because nothing debuffed this town.
		director.DirectorTick();

		int phaseAfterTick = director.GetPhase();
		int phaseTicksAfterTick = director.GetPhaseTicks();

		// --- RESTORE before asserting, so a red case leaves the campaign alone.
		director.ResetObjective("initialisation-tier gate fixture torn down", false);

		// --- ASSERT.
		if (successes != SUCCESSES_TO_COUNT)
		{
			SetFailure("counting %1 harassment successes read back %2 - the counter itself is broken, so the claims below would be vacuous",
				SUCCESSES_TO_COUNT.ToString(), successes.ToString());
			return true;
		}

		if (phaseAfterCounting != OVT_EObjectivePhase.HARASSMENT)
		{
			SetFailure("counting a harassment success must not change the phase: read back phase %1, expected HARASSMENT. A counter increment is not a tick, and this method is called from deployments, restores and fixtures",
				phaseAfterCounting.ToString());
			return true;
		}

		if (phaseTicksAfterCounting != PLANTED_PHASE_TICKS)
		{
			SetFailure("counting a harassment success moved the phase timeout: planted %1, read back %2. Only a tick may move a timer",
				PLANTED_PHASE_TICKS.ToString(), phaseTicksAfterCounting.ToString());
			return true;
		}

		if (phaseAfterTick != OVT_EObjectivePhase.HARASSMENT)
		{
			SetFailure("a tick must not open the forward-base gate for a town this ramp never debuffed: read back phase %1, expected HARASSMENT. Being already unpopular is not the same as having been harassed",
				phaseAfterTick.ToString());
			return true;
		}

		if (phaseTicksAfterTick != PLANTED_PHASE_TICKS - 1)
		{
			SetFailure("a harassment tick must serve exactly one round off the phase timeout: planted %1, read back %2. A value far ABOVE the planted one is a phase entry re-arming it, not a failed decrement",
				PLANTED_PHASE_TICKS.ToString(), phaseTicksAfterTick.ToString());
			return true;
		}

		Print("Objective gate: counting a harassment success moves no phase and no timer, and a harassment tick refuses the forward-base gate for a town carrying none of this ramp's own debuff");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The appended support modifier resolves by name, and its index is the LAST one in the config.
//!
//! ⚠ THIS IS THE SAVE-CORRUPTING ONE, AND IT IS WHY THE CASE ASSERTS A POSITION RATHER THAN JUST A
//! LOOKUP. OVT_Modifier.m_iIndex is the POSITIONAL index of an entry in m_aModifiers, assigned in
//! OVT_TownModifierSystem.PostInit(), and that index is what the replicated per-town modifier lists
//! carry. An entry inserted anywhere other than the END shifts every later index by one, so every
//! town in every live save comes back carrying different modifiers than it was saved with. A
//! well-meaning tidy-up that alphabetised the config would do it, and nothing else in the tree would
//! notice.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   M1. The "ObjectiveHarassment" entry moved one place up in supportModifiers.conf. Fails on "must
//!       be the LAST entry".
//!   M2. `name "ObjectiveHarassment"` misspelled. Fails on "does not resolve by name".
//!   M3. `flags 3` changed to `flags 1` (ACTIVE without STACKABLE). Fails on "must be STACKABLE".
//!   M4. `baseEffect -20` changed to a positive 20. Fails on "must push support DOWN".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveOperations_ModifierIsLastAndStacks : SCR_AutotestCaseBase
{
	//! The name the harassment behaviour module applies by, and its authored default.
	static const string MODIFIER_NAME = "ObjectiveHarassment";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetFailure("OVT_Global.GetTowns() is null - the town modifier systems did not resolve");
			return true;
		}

		OVT_TownModifierSystem system = towns.GetModifierSystem(OVT_TownSupportModifierSystem);
		if (!system || !system.m_Config || !system.m_Config.m_aModifiers)
		{
			SetFailure("the support modifier system has no loaded config");
			return true;
		}

		int index = system.GetModifierIndexByName(MODIFIER_NAME);
		if (index < 0)
		{
			SetFailure("'%1' does not resolve by name in the support modifier config - the harassment behaviour module applies it by exactly this string and would silently do nothing",
				MODIFIER_NAME);
			return true;
		}

		int last = system.m_Config.m_aModifiers.Count() - 1;
		if (index != last)
		{
			SetFailure("'%1' is at index %2 but must be the LAST entry (index %3): m_iIndex is positional and travels in every save's replicated per-town modifier lists, so an entry ahead of it shifts every later index by one",
				MODIFIER_NAME, index.ToString(), last.ToString());
			return true;
		}

		OVT_ModifierConfig modifier = system.m_Config.m_aModifiers[index];

		if (!(modifier.flags & OVT_ModifierFlags.STACKABLE))
		{
			SetFailure("'%1' must be STACKABLE - the whole mechanic is a debuff that deepens with each completed operation", MODIFIER_NAME);
			return true;
		}

		if (system.GetModifierLimit(index) < 2)
		{
			SetFailure("'%1' has a stack limit of %2 - a ramp that cannot stack twice cannot ramp",
				MODIFIER_NAME, system.GetModifierLimit(index).ToString());
			return true;
		}

		if (modifier.baseEffect >= 0)
		{
			SetFailure("'%1' has a baseEffect of %2 - harassment must push support DOWN",
				MODIFIER_NAME, modifier.baseEffect.ToString());
			return true;
		}

		if (modifier.title == "")
		{
			SetFailure("'%1' has no localized title, so it renders as a blank row in the town panel", MODIFIER_NAME);
			return true;
		}

		Print(string.Format("Objective harassment modifier: resolves by name at index %1, which is the last entry, stackable, negative and titled",
			index.ToString()));

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! 🔴 EVERY CONFIG THE OBJECTIVE DIRECTOR OWNS IS INVISIBLE TO THE ORDINARY EVALUATOR, AND NOTHING ELSE
//! IS.
//!
//! WHAT WENT WRONG WITHOUT IT (play-test, 2026-08-19). The director's operations are registered configs
//! like any other - they have to be, because the cost model, the Game Master panel, the reinforcement
//! rebuy and the save all read a CONFIG. That made them candidates for the 30 s evaluator's own
//! FindBestDeploymentConfig(), and it duly bought one: an "Objective Harassment (Patrol)" created at a
//! town beside a BASE objective, with no "[ObjectiveDirector] Sent ..." line above it because the
//! director had not sent it. The harassment force stood at the flag doing nothing - it was not the
//! director's operation and its objective condition had never made it one - and the faction pool was
//! charged for it outside the director's accounting, which is precisely the single-decision-maker
//! property (G1) the director exists to hold.
//!
//! WHY A FLAG AND NOT A MASK. Neither existing gate can express this. Every one of these configs is
//! legitimately an OCCUPYING_FACTION config sent to a TOWN, a BASE, a RADIO_TOWER or OPEN_TERRAIN -
//! that is where the director sends it - and m_iAllowedLocationTypes 0 means "no restrictions", not
//! "nowhere". See OVT_DeploymentConfig.IsSelectableByEvaluator().
//!
//! THREE CLAIMS:
//!   1. All EIGHT director-owned configs resolve. The four harassment rungs and the FOB pair are easy to
//!      forget when a new one is added.
//!   2. Every one of them is director-only. A rung added to overthrowDeployments.conf without the flag
//!      is bought by the evaluator on the next 30 s pass, at some place the director never chose.
//!   3. NOTHING ELSE is. The flag silently removes a config from the only path that creates it, so a
//!      base garrison or a town patrol marked by mistake simply stops existing in the world with no
//!      error anywhere - which is the failure mode this half exists to catch.
//!
//! ⚠ IT DOES NOT ASSERT THAT ForceCreateDeployment() STILL WORKS ON THEM, because the flag is not
//! consulted there by construction - the director's own send path is exercised by the play-test and by
//! OVT_TEST_Init_ObjectiveDirector's spender cases.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; each exited tools/compile-check.sh
//! 0, and the subject was restored and re-compiled clean):
//!   D1. `m_bDirectorOnly 1` removed from Deployment_ObjectiveHarassment.conf. Fails on "is not marked
//!       director-only" for all four rungs, which inherit it from that file.
//!   D2. `m_bDirectorOnly 1` added to Deployment_TownPatrol.conf. Fails on "'Town Patrol' is marked
//!       director-only but the objective director never sends it".
//!   D3. `IsSelectableByEvaluator()` returning `m_bDirectorOnly` rather than its negation. Fails both
//!       halves at once.
//! No polling, no waiting, no world state touched, no maxAttempts.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveOperations_DirectorConfigsAreNotEvaluatorCandidates : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (!deployments || !deployments.m_DeploymentRegistry || !deployments.m_DeploymentRegistry.m_aDeploymentConfigs)
		{
			SetFailure("The deployment framework or its registry did not resolve");
			return true;
		}

		array<string> directorOwned = CollectDirectorOwnedNames();

		string failure = CheckOwnedAreExcluded(deployments, directorOwned);
		if (failure == "")
			failure = CheckNothingElseIs(deployments, directorOwned);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective operations: all " + directorOwned.Count().ToString() + " director-owned configs are excluded from the evaluator's own candidate selection, and no other registered config is");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every config name the objective director creates by hand, read off the director's own constants
	//! rather than re-typed here - a rung renamed in one place and not the other is a different case's
	//! job (case A), and duplicating the list would hide it from both.
	//! \return The eight names.
	protected array<string> CollectDirectorOwnedNames()
	{
		array<string> directorOwned = new array<string>();

		foreach (string rung : OVT_ObjectiveDirectorComponent.HARASSMENT_LADDER)
		{
			directorOwned.Insert(rung);
		}

		directorOwned.Insert(OVT_ObjectiveDirectorComponent.TOWER_RECAPTURE_CONFIG);
		directorOwned.Insert(OVT_ObjectiveDirectorComponent.SABOTAGE_CONFIG);
		directorOwned.Insert(OVT_ObjectiveDirectorComponent.FOB_CONFIG);
		directorOwned.Insert(OVT_ObjectiveDirectorComponent.FOB_GARRISON_CONFIG);

		return directorOwned;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] deployments The deployment framework.
	//! \param[in] directorOwned Every config the director sends.
	//! \return An empty string when every one is excluded, or the first that is not.
	protected string CheckOwnedAreExcluded(notnull OVT_DeploymentManagerComponent deployments, notnull array<string> directorOwned)
	{
		if (directorOwned.Count() < 8)
			return string.Format("only %1 director-owned config names were collected - the ladder or the constants have shrunk and this case would pass without checking the missing ones", directorOwned.Count().ToString());

		foreach (string name : directorOwned)
		{
			OVT_DeploymentConfig config = deployments.FindConfigByName(name);
			if (!config)
				return string.Format("'%1' is not registered in overthrowDeployments.conf - the director cannot send it at all", name);

			if (config.IsSelectableByEvaluator())
				return string.Format("'%1' is not marked director-only, so the 30 s evaluator may buy one of its own accord at any place its location mask happens to match - outside the director's accounting and with no objective behind it", name);
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] deployments The deployment framework.
	//! \param[in] directorOwned Every config the director sends.
	//! \return An empty string when no other config carries the flag, or which one does.
	protected string CheckNothingElseIs(notnull OVT_DeploymentManagerComponent deployments, notnull array<string> directorOwned)
	{
		foreach (OVT_DeploymentConfig config : deployments.m_DeploymentRegistry.m_aDeploymentConfigs)
		{
			if (!config || config.IsSelectableByEvaluator())
				continue;

			if (directorOwned.Contains(config.m_sDeploymentName))
				continue;

			return string.Format("'%1' is marked director-only but the objective director never sends it - nothing else in the campaign creates configs by hand, so it would simply never appear in the world, silently", config.m_sDeploymentName);
		}

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! 🔴 EVERY PHASE 1 OPERATION SPANS HARASSMENT **AND** THE FORWARD-BASE PHASE, AND NONE OF THEM
//! REACHES THE COUNTER-ATTACK. This is the authored half of the 2026-08-19 deadlock fix, and it is the
//! half nothing else can see.
//!
//! WHAT WENT WRONG, AND WHY A CONFIG FILE IS THE ONLY PLACE IT SHOWS. All six Phase 1 configs - the
//! four harassment rungs, tower recapture and sabotage - authored
//! OVT_ObjectiveConditionDeploymentModule with `m_iRequiredPhase 1` and nothing else, and the module
//! compared with ==. BasePhase2Gate() promotes a base objective on its FIRST completed sabotage
//! mission, and BasePhase3Gate() demands six of them on Easy; so the promotion made the remaining five
//! unsendable, the counter froze at one, and the counter-attack - the headline promise of the whole
//! feature - could never fire. The town side deadlocked one step later, because the stacking support
//! debuff that has to drive support under 25 % is applied by harassment operations. Nothing errored.
//! Nothing warned. The play-test symptom was an objective that sat in the forward-base phase until its
//! idle clock ran out, over and over.
//!
//! ⚠ IT ASSERTS THE EFFECTIVE SPAN, NOT THE RAW FIELD, and that distinction is the case. Reading
//! m_iThroughPhase directly would accept a config that authored 0 on the strength of the number being
//! present; ResolveThroughPhase() applies the same collapse the live predicate applies, so a config
//! whose span the gate refuses is a config this case refuses.
//!
//! ⚠ AND THE UPPER BOUND IS ASSERTED AS A REFUSAL, not merely as a number. Authored 3, harassment and
//! sabotage teams would keep walking into a battle that is already being fought. It is the SECOND line
//! of defence rather than the first - DirectorTick() early-returns for the whole of a live battle, so
//! nothing can be created in that phase whatever a config says - but it decides whether teams already
//! in the world are collected when the battle starts, and they must be.
//!
//! WHY IT LIVES BESIDE THE RAMP RATHER THAN WITH THE FORWARD BASE. The two forward-base configs are
//! pinned at 2 -> 2 by OVT_TEST_Init_ObjectiveFOB's config case, which owns them; duplicating them here
//! would mean two places to update and two chances to update only one.
//!
//! 🔴 AND THE SECOND HALF OF THE SAME SENTENCE: THE RAMP LAUNCHES FROM THE FORWARD BASE. §3.2 reads
//! "the FOB becomes the insertion source for further Phase 1 operations, spending against a CEILING
//! inside the deployment pool". Continuing to send them is the first half; sourcing them from the
//! forward base is the second, and without it the base is a flag in a field with a garrison round it
//! while every truck still drives the whole way from the rear. OVT_ObjectiveAnchorSourceProvider is the
//! one line that makes it true, and it is asserted here rather than left to a reader.
//!
//! ⚠ THE PROVIDER SWAP IS A NO-OP UNTIL A FORWARD BASE STANDS, which is why it is safe on configs that
//! spend most of their life in harassment. Its fallback IS OVT_NearestControlledBaseSourceProvider with
//! no distance limit - exactly what these configs authored before - so with no director, no objective or
//! no forward base the answer is byte-identical to the old one. What it can never do is strand a force:
//! it falls through rather than failing.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   P1. `m_iThroughPhase 2` deleted from Deployment_ObjectiveSabotage.conf - the pre-fix authoring.
//!       Fails on "'Objective Sabotage' must span the forward-base phase".
//!   P2. `m_iThroughPhase 2` deleted from Deployment_ObjectiveHarassment.conf, which all four rungs
//!       inherit. Fails on the same claim for the first rung walked, 'Objective Harassment (Patrol)'.
//!   P3. `m_iThroughPhase 3` authored on Deployment_ObjectiveTowerRecapture.conf. Fails on "must NOT
//!       reach the counter-attack phase".
//!   P4. `m_iRequiredPhase 2` authored on Deployment_ObjectiveSabotage.conf. Fails on "must be
//!       creatable during harassment".
//!   P5. m_Source on Deployment_ObjectiveHarassment.conf reverted to
//!       OVT_NearestControlledBaseSourceProvider - the pre-fix authoring, inherited by all four rungs.
//!       Fails on "must be sourced from the forward base" for the first rung walked.
//! No polling, no waiting, no world state touched, no maxAttempts.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveOperations_RampSpansTheForwardBasePhaseAndLaunchesFromIt : SCR_AutotestCaseBase
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

		array<string> rampConfigs = CollectRampConfigNames();

		// The ladder is read off the director's own constant, so a rung deleted there would silently
		// shrink what this case checks. Six is four rungs plus recapture plus sabotage.
		if (rampConfigs.Count() < 6)
		{
			SetFailure("only %1 Phase 1 config names were collected - the harassment ladder or the director's constants have shrunk, and this case would pass without checking the missing ones",
				rampConfigs.Count().ToString());
			return true;
		}

		foreach (string name : rampConfigs)
		{
			string failure = CheckSpan(deployments, name);
			if (failure == "")
				failure = CheckSource(deployments, name);

			if (failure != "")
			{
				SetFailure(failure);
				return true;
			}
		}

		Print("Objective operations: all " + rampConfigs.Count().ToString() + " Phase 1 configs span harassment through the forward-base phase and are sourced from the forward base once one stands, so the ramp continues AND shortens the supply line - and not one of them reaches the counter-attack");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every config the director sends as Phase 1 work, read off the director's own constants rather
	//! than re-typed - a rung renamed in one place and not the other is case A's job, and duplicating
	//! the list would hide it from both.
	//! \return The six names.
	protected array<string> CollectRampConfigNames()
	{
		array<string> names = new array<string>();

		foreach (string rung : OVT_ObjectiveDirectorComponent.HARASSMENT_LADDER)
		{
			names.Insert(rung);
		}

		names.Insert(OVT_ObjectiveDirectorComponent.TOWER_RECAPTURE_CONFIG);
		names.Insert(OVT_ObjectiveDirectorComponent.SABOTAGE_CONFIG);

		return names;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] deployments The deployment framework.
	//! \param[in] name The config to check.
	//! \return An empty string when its span is exactly harassment through the forward-base phase, or
	//!         which half of that is wrong.
	protected string CheckSpan(notnull OVT_DeploymentManagerComponent deployments, string name)
	{
		OVT_DeploymentConfig config = deployments.FindConfigByName(name);
		if (!config)
			return string.Format("'%1' is not registered in overthrowDeployments.conf, so there is no span to check", name);

		OVT_ObjectiveConditionDeploymentModule condition;
		foreach (OVT_BaseDeploymentModule module : config.m_aModules)
		{
			condition = OVT_ObjectiveConditionDeploymentModule.Cast(module);
			if (condition)
				break;
		}

		if (!condition)
			return string.Format("'%1' authors no objective condition at all - the evaluator could create it anywhere and nothing would collect it when the objective ends", name);

		if (condition.m_iRequiredPhase != OVT_EObjectivePhase.HARASSMENT)
			return string.Format("'%1' must be creatable during harassment and its first phase is %2 - a ramp operation that cannot be sent in the phase the ramp runs in is a phase that does nothing",
				name, condition.m_iRequiredPhase.ToString());

		int through = condition.ResolveThroughPhase();

		if (through < OVT_EObjectivePhase.FOB)
			return string.Format("'%1' must span the forward-base phase and stops at %2. THIS IS THE 2026-08-19 DEADLOCK: one sabotage success promotes a base objective out of harassment, so a ramp scoped to harassment alone can never reach the six missions the counter-attack demands, and a town's support debuff stops stacking and times out",
				name, through.ToString());

		if (through > OVT_EObjectivePhase.FOB)
			return string.Format("'%1' must NOT reach the counter-attack phase and spans up to %2 - once the battle is on, harassment and sabotage teams walking in are noise, and a team already in the world must be collected rather than kept",
				name, through.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] deployments The deployment framework.
	//! \param[in] name The config to check.
	//! \return An empty string when it is sourced from the forward base, or why that matters.
	protected string CheckSource(notnull OVT_DeploymentManagerComponent deployments, string name)
	{
		OVT_DeploymentConfig config = deployments.FindConfigByName(name);
		if (!config)
			return string.Format("'%1' is not registered in overthrowDeployments.conf, so there is no source provider to check", name);

		OVT_InsertionSpawningDeploymentModule insertion;
		foreach (OVT_BaseDeploymentModule module : config.m_aModules)
		{
			insertion = OVT_InsertionSpawningDeploymentModule.Cast(module);
			if (insertion)
				break;
		}

		if (!insertion)
			return string.Format("'%1' has no insertion spawning module - its force would appear at the objective out of thin air", name);

		if (!insertion.m_Source)
			return string.Format("'%1' authors no source provider at all, so its insertion module registers nothing", name);

		// 🔴 THE LINE THAT MAKES A FORWARD BASE A SUPPLY SOURCE RATHER THAN SCENERY, for the RAMP and not
		// only for its own garrison. Its fallback is the provider this replaced, so nothing changes until
		// one is standing.
		if (!OVT_ObjectiveAnchorSourceProvider.Cast(insertion.m_Source))
			return string.Format("'%1' must be sourced from the forward base (OVT_ObjectiveAnchorSourceProvider) - with any other provider the ramp keeps driving the whole way from the rear even once a forward base is standing, which is half of §3.2 unimplemented and makes the middle phase cost the occupying faction resources for nothing but a garrison", name);

		return "";
	}
}
