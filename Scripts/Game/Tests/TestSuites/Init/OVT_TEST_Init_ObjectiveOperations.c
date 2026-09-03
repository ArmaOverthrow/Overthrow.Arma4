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
//! ⚠ NOTHING HERE DRIVES THE DIRECTOR EITHER. A harassment tick spends the occupying faction's real
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
//!       interruption must RESET the clock to the whole interval, not pause it".
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

		const int FULL = 3;
		int ticks = FULL;

		// --- Nobody there yet: the force is still on the road.
		if (harassment.EvaluateHold(0, false, FULL, ticks))
			return "an empty circle must not complete a hold";

		if (ticks != FULL)
			return string.Format("an empty circle must leave the hold at its full length: %1 tick(s) left, expected %2", ticks.ToString(), FULL.ToString());

		// --- Contested: men are there, so is a defender.
		if (harassment.EvaluateHold(4, true, FULL, ticks))
			return "a contested hold must not complete";

		if (ticks != FULL)
			return string.Format("a contested hold must leave the clock at its full length: %1 tick(s) left, expected %2", ticks.ToString(), FULL.ToString());

		// --- Held and unopposed: one tick.
		if (harassment.EvaluateHold(4, false, FULL, ticks))
			return "a hold with three ticks left must not complete on the first of them";

		if (ticks != 2)
			return string.Format("a held, unopposed tick must advance the clock by exactly one: %1 tick(s) left, expected 2", ticks.ToString());

		// --- 🔴 INTERRUPTED MID-HOLD. THE CLOCK MUST RESET, NOT PAUSE (author, 2026-08-25).
		if (harassment.EvaluateHold(0, false, FULL, ticks))
			return "the force being wiped mid-hold must not complete it";

		if (ticks != FULL)
			return string.Format("an interruption must RESET the hold to its full length, not pause it: %1 tick(s) left, expected %2", ticks.ToString(), FULL.ToString());

		// --- And the whole hold must now be served from the top.
		if (harassment.EvaluateHold(4, false, FULL, ticks))
			return "a reset hold must not complete on its first tick";

		if (harassment.EvaluateHold(4, false, FULL, ticks))
			return "a reset hold must not complete on its second tick";

		if (!harassment.EvaluateHold(4, false, FULL, ticks))
			return "a hold whose last tick was served must complete";

		if (!harassment.HasHoldFired())
			return "a completed hold must latch";

		// --- And never again, however the inputs read afterwards.
		if (harassment.EvaluateHold(4, false, FULL, ticks))
			return "a completed hold must not fire a second time - it would re-stack the town's support debuff every ten seconds";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckRecapture()
	{
		OVT_TowerRecaptureBehaviorDeploymentModule recapture = new OVT_TowerRecaptureBehaviorDeploymentModule();

		const int RFULL = 2;
		int ticks = RFULL;

		// --- Already ours. Nothing to take, and nothing to spend a latch on.
		if (recapture.EvaluateRecapture(true, false, MY_FACTION, MY_FACTION, RFULL, ticks))
			return "a tower this faction already holds must never be recaptured - the hold would run and the flip would be a no-op";

		if (recapture.EvaluateRecapture(false, false, ENEMY_FACTION, MY_FACTION, RFULL, ticks))
			return "a tower nobody of ours is standing at must not complete a hold";

		if (ticks != RFULL)
			return string.Format("an unheld tower must leave the clock at its full length: %1 tick(s) left, expected %2", ticks.ToString(), RFULL.ToString());

		if (recapture.EvaluateRecapture(true, true, ENEMY_FACTION, MY_FACTION, RFULL, ticks))
			return "a contested tower must not complete a hold";

		if (ticks != RFULL)
			return string.Format("a contested tower must leave the clock at its full length: %1 tick(s) left, expected %2", ticks.ToString(), RFULL.ToString());

		// --- Held and unopposed: one tick.
		if (recapture.EvaluateRecapture(true, false, ENEMY_FACTION, MY_FACTION, RFULL, ticks))
			return "a hold with two ticks left must not complete on the first of them";

		if (ticks != 1)
			return string.Format("a held, unopposed tick must advance the clock by exactly one: %1 tick(s) left, expected 1", ticks.ToString());

		// --- 🔴 INTERRUPTED. THE CLOCK MUST RESET, NOT PAUSE (author, 2026-08-25).
		if (recapture.EvaluateRecapture(true, true, ENEMY_FACTION, MY_FACTION, RFULL, ticks))
			return "an interrupted hold must not complete";

		if (ticks != RFULL)
			return string.Format("an interruption must RESET the hold to its full length, not pause it: %1 tick(s) left, expected %2", ticks.ToString(), RFULL.ToString());

		if (recapture.EvaluateRecapture(true, false, ENEMY_FACTION, MY_FACTION, RFULL, ticks))
			return "a reset hold must not complete on its first tick";

		if (!recapture.EvaluateRecapture(true, false, ENEMY_FACTION, MY_FACTION, RFULL, ticks))
			return "a hold whose last tick was served must complete";

		if (!recapture.HasCaptureFired())
			return "a completed recapture must latch";

				if (recapture.EvaluateRecapture(true, false, ENEMY_FACTION, MY_FACTION, RFULL, ticks))
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
//!   C2. `clone.m_sFromPhase = m_sFromPhase;` deleted from the objective condition clone.
//!   C5. `clone.m_sThroughPhase = m_sThroughPhase;` deleted from the objective condition clone. Fails
//!       on "dropped m_sThroughPhase" - every range would collapse to a single phase, which is the
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
		if (failure == "")
			failure = CheckCheckpointClone();

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective operation modules: the harassment, recapture, objective condition and mobile checkpoint clones all carry every authored attribute, and neither latch is copied");

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
		source.EvaluateHold(1, false, 1, ticks);

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
		source.EvaluateRecapture(true, false, 8, 7, 1, ticks);

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
		source.m_sFromPhase = "FixtureFirstPhase";

		// ⚠ DELIBERATELY DIFFERENT FROM m_sFromPhase, and deliberately not a name any shipped plan
		// authors. A clone that copied the first phase into both fields, or dropped the upper bound and
		// let it collapse back onto the first, would pass an assertion that used the same string twice.
		source.m_sThroughPhase = "FixtureLastPhase";
		source.m_fMaxDistanceFromObjective = 933;

		OVT_ObjectiveConditionDeploymentModule clone = OVT_ObjectiveConditionDeploymentModule.Cast(source.CloneModule());
		if (!clone)
			return "the objective condition module's CloneModule did not return a module of its own type";

		if (clone.m_sModuleName != source.m_sModuleName)
			return "the objective condition clone dropped m_sModuleName";

		if (clone.m_sFromPhase != source.m_sFromPhase)
			return string.Format("the objective condition clone dropped m_sFromPhase: '%1', expected '%2' - an empty first phase resolves to no phase at all, so every objective deployment in the campaign would be refused and collected on its first reinforcement check",
				clone.m_sFromPhase, source.m_sFromPhase);

		if (clone.m_sThroughPhase != source.m_sThroughPhase)
			return string.Format("the objective condition clone dropped m_sThroughPhase: '%1', expected '%2' - every range would collapse to a single phase, which is the 2026-08-19 deadlock restored one clone at a time: the ramp's operations would be collected on the promotion tick and the counter-attack would be unreachable again",
				clone.m_sThroughPhase, source.m_sThroughPhase);

		if (clone.EffectiveThroughPhase() != source.EffectiveThroughPhase())
			return string.Format("the objective condition clone reports a different span from its source: up to '%1', expected up to '%2'",
				clone.EffectiveThroughPhase(), source.EffectiveThroughPhase());

		if (clone.m_fMaxDistanceFromObjective != source.m_fMaxDistanceFromObjective)
			return string.Format("the objective condition clone dropped m_fMaxDistanceFromObjective: %1, expected %2 - it would refuse every position but the objective's exact centre",
				clone.m_fMaxDistanceFromObjective.ToString(), source.m_fMaxDistanceFromObjective.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The mobile checkpoint's six.
	//!
	//! ⚠ EVERY PROBE VALUE IS DIFFERENT FROM WHAT `new` PRODUCES, which for this module is 0 and "".
	//! [Attribute] defvalues do NOT apply to a hand-built instance, so a probe value that happened to
	//! match a fresh field would prove nothing at all - a dropped copy line and a correct one would read
	//! identically. That is why m_iRelocateMinutes is 7 rather than 0 and why the band is authored the
	//! wrong way round from the shipped one.
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckCheckpointClone()
	{
		OVT_MobileCheckpointBehaviorDeploymentModule source = new OVT_MobileCheckpointBehaviorDeploymentModule();

		source.m_sModuleName = "fixture checkpoint";
		source.m_fApproachMinDistance = 211;
		source.m_fApproachMaxDistance = 417;
		source.m_fRoadSearchRadius = 83;
		source.m_iRelocateMinutes = 7;
		source.m_fCheckpointSpread = 31;

		OVT_MobileCheckpointBehaviorDeploymentModule clone = OVT_MobileCheckpointBehaviorDeploymentModule.Cast(source.CloneModule());
		if (!clone)
			return "the mobile checkpoint module's CloneModule did not return a module of its own type - every deployment running that config would get the wrong class";

		if (clone.m_sModuleName != source.m_sModuleName)
			return "the mobile checkpoint clone dropped m_sModuleName";

		if (clone.m_fApproachMinDistance != source.m_fApproachMinDistance)
			return string.Format("the mobile checkpoint clone dropped m_fApproachMinDistance: %1, expected %2 - the band collapses towards zero and the checkpoint is set up ON the objective instead of on the road into it",
				clone.m_fApproachMinDistance.ToString(), source.m_fApproachMinDistance.ToString());

		if (clone.m_fApproachMaxDistance != source.m_fApproachMaxDistance)
			return string.Format("the mobile checkpoint clone dropped m_fApproachMaxDistance: %1, expected %2 - the same collapse from the other end",
				clone.m_fApproachMaxDistance.ToString(), source.m_fApproachMaxDistance.ToString());

		if (clone.m_fRoadSearchRadius != source.m_fRoadSearchRadius)
			return string.Format("the mobile checkpoint clone dropped m_fRoadSearchRadius: %1, expected %2 - a radius of zero means no bearing ever has a road inside it, so no checkpoint is ever established and the infantry rides until the patience clock lets it out",
				clone.m_fRoadSearchRadius.ToString(), source.m_fRoadSearchRadius.ToString());

		if (clone.m_iRelocateMinutes != source.m_iRelocateMinutes)
			return string.Format("the mobile checkpoint clone dropped m_iRelocateMinutes: %1, expected %2 - zero is the authored way of saying 'park and never roam', so a dropped line turns every roaming checkpoint into a static one and nothing warns",
				clone.m_iRelocateMinutes.ToString(), source.m_iRelocateMinutes.ToString());

		if (clone.m_fCheckpointSpread != source.m_fCheckpointSpread)
			return string.Format("the mobile checkpoint clone dropped m_fCheckpointSpread: %1, expected %2 - the whole dismounted section would be put down on one spot",
				clone.m_fCheckpointSpread.ToString(), source.m_fCheckpointSpread.ToString());

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! Every config a hand-creator sends by name is marked director-only, and no other registered
//! config is. The names come from the director's constants, the other hand-creators and the registry.
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

		array<string> handCreated = CollectDirectorOwnedNames();
		CollectOtherHandCreatedNames(deployments, handCreated);

		string failure = CheckOwnedAreExcluded(deployments, handCreated);
		if (failure == "")
			failure = CheckNothingElseIs(deployments, handCreated);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective operations: all " + handCreated.Count().ToString() + " hand-created configs are excluded from the evaluator's own candidate selection, and no other registered config is");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every config name the objective director creates by hand, read off the director's own constants
	//! rather than re-typed here - a rung renamed in one place and not the other is a different case's
	//! job (case A), and duplicating the list would hide it from both.
	//! \return Every director-owned name: the whole harassment ladder plus the four singletons.
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
	//! The director is no longer the only hand-creator. Three other callers stand a config up by name
	//! and each one has to be director-only for the same reason the director's are.
	//! \param[in] deployments The deployment framework, for the crew-up modules in its registry.
	//! \param[inout] names The list to append to.
	protected void CollectOtherHandCreatedNames(notnull OVT_DeploymentManagerComponent deployments, notnull array<string> names)
	{
		names.Insert(OVT_OccupyingFactionManager.HUNTER_KILLER_CONFIG_NAME);
		names.Insert(OVT_QRFControllerComponent.ECHELON_CONFIG_NAME);

		// The sortie's name is authored per crew-up module rather than held in a constant, so it is read
		// off the registry - a renamed sortie config stays covered without touching this case.
		foreach (OVT_DeploymentConfig config : deployments.m_DeploymentRegistry.m_aDeploymentConfigs)
		{
			if (!config)
				continue;

			foreach (OVT_BaseDeploymentModule module : config.m_aModules)
			{
				OVT_CrewUpOnAlarmBehaviorDeploymentModule crewUp = OVT_CrewUpOnAlarmBehaviorDeploymentModule.Cast(module);
				if (!crewUp || crewUp.m_sSortieConfigName == "")
					continue;

				if (!names.Contains(crewUp.m_sSortieConfigName))
					names.Insert(crewUp.m_sSortieConfigName);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] deployments The deployment framework.
	//! \param[in] directorOwned Every config the director sends.
	//! \return An empty string when every one is excluded, or the first that is not.
	protected string CheckOwnedAreExcluded(notnull OVT_DeploymentManagerComponent deployments, notnull array<string> directorOwned)
	{
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

			return string.Format("'%1' is marked director-only but no hand-creator sends it - the director, the hunter-killer dispatcher, the QRF echelon and the crew-up sortie are the whole list, so it would simply never appear in the world, silently", config.m_sDeploymentName);
		}

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! Exposes the mobile checkpoint's approach chooser, and gives it an objective without a deployment.
//!
//! A SUBCLASS RATHER THAN A WIDENED PRODUCTION API. ChooseApproach() is protected because nothing but
//! the module's own update may call it, and building a real deployment to reach it would arm a
//! repeating update that converges a mounted force - a real armed vehicle on a real road, with the
//! autotest camera inside its registration ring.
//------------------------------------------------------------------------------------------------
class OVT_TEST_MobileCheckpointProbe : OVT_MobileCheckpointBehaviorDeploymentModule
{
	protected vector m_vProbeObjective;

	//------------------------------------------------------------------------------------------------
	//! \param[in] objective Where this probe's objective is.
	void ProbeSetObjective(vector objective)
	{
		m_vProbeObjective = objective;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The planted objective, since this probe has no parent deployment to take one from.
	override protected vector ObjectivePosition()
	{
		return m_vProbeObjective;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] bearing The bearing to treat as the one just used.
	void ProbeSetPreviousBearing(float bearing)
	{
		m_fCheckpointBearing = bearing;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[out] position The road point chosen.
	//! \param[out] angles The road's heading there.
	//! \param[out] bearing The bearing from the objective.
	//! \return True when an approach was found.
	bool ProbeChooseApproach(out vector position, out vector angles, out float bearing)
	{
		return ChooseApproach(position, angles, bearing);
	}
}

//------------------------------------------------------------------------------------------------
//! THE APPROACH CHOOSER against a real map: a bearing with no road inside the search radius is
//! refused, and a relocation never lands back on the bearing it just left.
//!
//! WHAT THIS TIER CAN SEE THAT THE PURE ONE CANNOT. OVT_CheckpointApproachRules is proven function by
//! function in OVT_TEST_Logic_CheckpointApproach.c and knows nothing about roads. What is NOT pure is
//! the wiring: that the module samples bearings around ITS objective, that it keeps only the samples a
//! road answered for, that the point it returns is the ROAD point and not the sample, and that it
//! carries its own last bearing into the exclusion rule. A chooser that sampled correctly and then
//! forgot to pass the previous bearing would look right in every pure case and oscillate forever.
//!
//! ⚠ THE REFUSAL HALF IS DETERMINISTIC AND THE ROTATION HALF IS NOT. A search radius of zero refuses
//! every sample whatever the map looks like, so that claim is asserted outright. Whether this world's
//! town has TWO road approaches in the band is a property of the map, so the rotation half asserts a
//! conditional - if a second approach was found it must be a different one - and reports a diagnostic
//! when the map offered nothing to rotate to. A world with one road cannot make that claim false, and
//! saying so is better than a case that passes for the wrong reason without saying anything.
//!
//! ⚠ NOTHING HERE BUILDS A DEPLOYMENT AND NOTHING TICKS. The subject is a bare `new` module driven
//! through OVT_TEST_MobileCheckpointProbe; no group is registered, no vehicle is spawned and no order
//! is issued.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   M1. The `if (!OVT_WorldUtils.FindNearestRoadSpawn(...)) continue;` line in ChooseApproach()
//!       replaced with an unconditional insert of the sampled probe point. Fails on "a search radius
//!       of zero must refuse every approach".
//!   M2. m_fCheckpointBearing replaced with NO_PREVIOUS_BEARING in the ChooseBearingIndex() call.
//!       Fails on "a relocation came back to the bearing it just left".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveOperations_MountedCheckpointApproachChooser : SCR_AutotestCaseBase
{
	//! How many relocations are asked for. Three is enough to catch a chooser that alternates between
	//! two bearings without excluding either, and short enough to be a few dozen road queries.
	static const int ROUNDS = 3;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || !towns.m_Towns || towns.m_Towns.IsEmpty())
		{
			SetFailure("The world produced no town to sample approaches around");
			return true;
		}

		vector objective = towns.m_Towns[0].location;

		string failure = CheckRefusesWithoutARoad(objective);
		if (failure == "")
			failure = CheckRotatesAwayFromTheLastBearing(objective);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Mobile checkpoint: the approach chooser refuses every bearing with no road inside its search radius, and a relocation never comes back to the bearing it just left");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] objective Where the fixture objective is.
	//! \return An empty string when the claim held, or why it did not.
	protected string CheckRefusesWithoutARoad(vector objective)
	{
		OVT_TEST_MobileCheckpointProbe probe = BuildProbe(objective);

		// ⚠ ZERO, NOT NEGATIVE. FindNearestRoadSpawn refuses anything further than the radius, so a
		// radius of zero refuses every road that is not exactly underfoot - which no sampled point on a
		// ring 150-300 m out ever is. A negative radius would prove the same thing through a degenerate
		// input the module can never be authored with.
		probe.m_fRoadSearchRadius = 0;

		vector position;
		vector angles;
		float bearing;

		if (probe.ProbeChooseApproach(position, angles, bearing))
			return string.Format("a search radius of zero must refuse every approach, and the chooser answered the point %1 on bearing %2. The road test is what makes a checkpoint a ROADBLOCK rather than a vehicle parked in a field",
				position.ToString(), bearing.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] objective Where the fixture objective is.
	//! \return An empty string when the claim held, or why it did not.
	protected string CheckRotatesAwayFromTheLastBearing(vector objective)
	{
		OVT_TEST_MobileCheckpointProbe probe = BuildProbe(objective);

		vector position;
		vector angles;
		float bearing;

		if (!probe.ProbeChooseApproach(position, angles, bearing))
		{
			Print("[OVT_TEST] Approach diagnostic: this world's first town has no road within the checkpoint's search radius anywhere in the 150-300 m band, so the rotation claim could not be exercised here at all", LogLevel.NORMAL);
			return "";
		}

		float reach = probe.m_fApproachMaxDistance + probe.m_fRoadSearchRadius;
		float measured = vector.Distance(position, objective);
		if (measured > reach)
			return string.Format("the chosen approach is %1 m from the objective, further than the band plus the search radius (%2 m). A checkpoint outside its own band is not on an approach to anything",
				measured.ToString(), reach.ToString());

		int rotations = 0;

		for (int round = 0; round < ROUNDS; round++)
		{
			float previous = bearing;

			probe.ProbeSetPreviousBearing(previous);

			vector nextPosition;
			vector nextAngles;
			float nextBearing;

			// A refusal is a legal answer: an objective with one usable road has nowhere else to go, and
			// the module keeps the checkpoint where it is rather than announcing a move it did not make.
			if (!probe.ProbeChooseApproach(nextPosition, nextAngles, nextBearing))
				continue;

			float separation = OVT_CheckpointApproachRules.AngularSeparation(nextBearing, previous);
			if (separation < OVT_MobileCheckpointBehaviorDeploymentModule.MIN_BEARING_SEPARATION_DEG)
				return string.Format("a relocation came back to the bearing it just left: %1 degrees from %2, which is only %3 degrees apart. Two usable approaches and no exclusion is a checkpoint that oscillates between the same pair forever",
					nextBearing.ToString(), previous.ToString(), separation.ToString());

			rotations = rotations + 1;
			bearing = nextBearing;
		}

		if (rotations == 0)
			Print("[OVT_TEST] Approach diagnostic: this world's first town offered only ONE usable approach, so every relocation was refused and the exclusion rule was never exercised against a real second choice", LogLevel.NORMAL);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! ⚠ EVERY FIELD IS SET BY HAND. [Attribute] defvalues do not apply to `new`, so an unset band
	//! would be 0..0 and the whole case would sample the objective's own centre twelve times.
	//! \param[in] objective Where the fixture objective is.
	//! \return A probe authored like the shipped mounted rung.
	protected OVT_TEST_MobileCheckpointProbe BuildProbe(vector objective)
	{
		OVT_TEST_MobileCheckpointProbe probe = new OVT_TEST_MobileCheckpointProbe();

		probe.m_sModuleName = "approach fixture";
		probe.m_fApproachMinDistance = 150;
		probe.m_fApproachMaxDistance = 300;
		probe.m_fRoadSearchRadius = 120;
		probe.m_iRelocateMinutes = 4;
		probe.m_fCheckpointSpread = 25;

		probe.ProbeSetObjective(objective);
		probe.ProbeSetPreviousBearing(OVT_CheckpointApproachRules.NO_PREVIOUS_BEARING);

		return probe;
	}
}
