//------------------------------------------------------------------------------------------------
//! TIER B - THE OCCUPYING FACTION REPAIRING ITS OWN GROUND, where it meets loaded configs.
//!
//! Five claims, none of which produces a script error when it is wrong: the control INVERSION
//! (m_bRequireControl 1, where sabotage authors 0), the evaluator flag (m_bDirectorOnly 0, plan D16),
//! the module ORDER (which only a `.conf` carries and only this can check), the target filter, and
//! the hand-copied clone.
//!
//! ⚠ NOTHING HERE CONSTRUCTS A DEPLOYMENT, RUINS ANYTHING OR REPAIRS ANYTHING. Every subject is a
//! loaded config object, a bare `new` module with no parent deployment, or a read off a live manager.
//!
//! ⚠ CASE C OVERLAPS OVT_TEST_Logic_ObjectiveRepair on one claim, deliberately: the plan's Phase 7
//! acceptance names the pause-not-reset rule as an initialisation-tier claim.
//!
//! No polling, no waiting, no maxAttempts: every subject is a synchronous read or a hand-built object.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! The repair config is registered, valid, BASE-typed, evaluator-selectable, correctly ordered, and
//! carries the control inversion that decides whose base a detail is sent to.
//!
//! ⚠ A `.conf` FAULT IS INVISIBLE TO tools/compile-check.sh, which compiles script and not configs.
//! Every injection below therefore proves the fault is SILENT, and this case is what catches it.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   A1. `m_sDeploymentName "Base Repair Detail"` misspelled in Deployment_ObjectiveRepair.conf.
//!       Fails on "is not registered".
//!   A2. `m_iAllowedLocationTypes` changed from BASE to TOWN. Fails on "cannot be used at a BASE".
//!   A3. The reinforcement module moved ABOVE the repair behaviour module. Fails on "must be authored
//!       before the reinforcement module".
//!   A4. `m_bRequireControl` set to 0 on the base control condition - the sabotage value. Fails on
//!       "must be authored with m_bRequireControl 1".
//!   A5. The OVT_BaseControlConditionDeploymentModule entry deleted outright. Fails on "authors no
//!       base control condition".
//!   A6. `m_bDirectorOnly 1` authored. Fails on "is marked director-only".
//!   A7. The registry entry for the config removed from overthrowDeployments.conf. Fails on "is not
//!       registered".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveRepair_AConfigResolvesAndIsOrdered : SCR_AutotestCaseBase
{
	//! ⚠ MATCHED BY STRING AND BY NOTHING ELSE, exactly like the base-defense cases. There is no
	//! director constant to read it from, because a repair detail is not a director operation (D16).
	static const string REPAIR_CONFIG = "Base Repair Detail";

	//! The last base-defense config's priority (Base Parked Vehicles). Repair must sort strictly after
	//! it, or a base can buy a repair detail before it has finished defending itself.
	static const int LAST_BASE_DEFENSE_PRIORITY = 10;

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

		string failure = CheckConfig(deployments);
		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective repair: the config is registered, valid, BASE-typed, evaluator-selectable, sorted behind every base-defense config, ordered behaviour-before-reinforcement, deploys only while the base IS ours, and fields a detail both factions have");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] deployments The deployment framework.
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckConfig(notnull OVT_DeploymentManagerComponent deployments)
	{
		OVT_DeploymentConfig config = deployments.FindConfigByName(REPAIR_CONFIG);
		if (!config)
			return string.Format("'%1' is not registered in overthrowDeployments.conf - the occupying faction would never repair anything", REPAIR_CONFIG);

		if (!config.IsValidConfig())
			return "the repair config does not validate";

		if (!config.CanUseLocationType(OVT_LocationTypeFlag.BASE))
			return "the repair config cannot be used at a BASE, which is the only place a repair detail is ever sent";

		// --- D16. It is MAINTENANCE, picked by the ordinary evaluator, not an operation the objective
		// director sends. Marked director-only it would simply never be created by anything.
		if (!config.IsSelectableByEvaluator())
			return "the repair config is marked director-only - the objective director never sends one, so it would be registered, valid and created by nothing at all";

		// --- Priced to sort behind the base-defense ladder, so it can never starve a defense.
		if (config.m_iPriority <= LAST_BASE_DEFENSE_PRIORITY)
			return string.Format("the repair config authors priority %1, which is not behind the last base-defense config at %2 - priority is the order of acquisition at one place, so a base would buy a repair detail before it had finished defending itself",
				config.m_iPriority.ToString(), LAST_BASE_DEFENSE_PRIORITY.ToString());

		// --- Ordering, and the two module claims that only a config file can carry.
		int behaviourIndex = -1;
		int reinforcementIndex = -1;
		bool foundControl = false;

		foreach (int index, OVT_BaseDeploymentModule module : config.m_aModules)
		{
			if (!module)
				continue;

			if (behaviourIndex == -1 && OVT_BaseRepairBehaviorDeploymentModule.Cast(module))
				behaviourIndex = index;

			if (reinforcementIndex == -1 && OVT_ReinforcementBehaviorDeploymentModule.Cast(module))
				reinforcementIndex = index;

			// ⚠ AN OBJECTIVE CONDITION WOULD RE-COUPLE IT TO THE DIRECTOR. A repair detail must be able
			// to go to ANY held base, not only to wherever the campaign's current intent points.
			if (OVT_ObjectiveConditionDeploymentModule.Cast(module))
				return "the repair config authors an objective condition module - repairing your own ground is not tied to an objective (D16), and one would confine the detail to whichever place the director happens to be working on";

			// ⚠ A PATROL BEHAVIOUR MODULE WOULD BREAK THE INSERTION. It answers BuildVirtualPlan() with
			// a plan of its own and pre-empts the march the detail needs to reach the base.
			if (OVT_PatrolBehaviorDeploymentModule.Cast(module))
				return "the repair config authors a patrol behaviour module, which would replace the movable march plan the detail needs to reach the base";

			// ⚠ A COMPOSITION MODULE WOULD PULL IN A SLOT GATE THIS CONFIG DOES NOT AUTHOR. A repair
			// detail builds nothing; it puts existing structures back.
			if (OVT_CompositionSpawningDeploymentModule.Cast(module))
				return "the repair config authors a composition spawning module - it builds nothing, and one would make it fail the shared composition slot gate";

			OVT_BaseControlConditionDeploymentModule control = OVT_BaseControlConditionDeploymentModule.Cast(module);
			if (control)
			{
				foundControl = true;

				if (!control.m_bRequireControl)
					return "the repair config's base control condition must be authored with m_bRequireControl 1 - authored as 0 (the SABOTAGE value) it sends repair details to bases the resistance holds, to rebuild structures for the side that took them";
			}
		}

		if (behaviourIndex == -1)
			return "the repair config authors no OVT_BaseRepairBehaviorDeploymentModule - it has no mission at all";

		if (reinforcementIndex == -1)
			return "the repair config authors no reinforcement module, so nothing collects it when the base changes hands";

		if (behaviourIndex > reinforcementIndex)
			return "the repair config authors its mission behaviour AFTER the reinforcement module - it must be authored before it, or a completed mission has its detail rebought in the same pass that ended it";

		if (!foundControl)
			return "the repair config authors no base control condition, so nothing stands the detail down when the base changes hands";

		// --- The insertion module, and a group both factions can field.
		OVT_InsertionSpawningDeploymentModule insertion;
		foreach (OVT_BaseDeploymentModule module : config.m_aModules)
		{
			OVT_InsertionSpawningDeploymentModule candidate = OVT_InsertionSpawningDeploymentModule.Cast(module);
			if (candidate)
			{
				insertion = candidate;
				break;
			}
		}

		if (!insertion)
			return "the repair config has no insertion spawning module - its detail would appear at the base out of thin air";

		if (!insertion.m_Source)
			return "the repair config authors no source provider, so its insertion module registers nothing at all";

		// ⚠ THE SOURCE MUST NOT BE THE DIRECTOR'S ANCHOR. An evaluator-selectable config can be created
		// at a base with no objective anywhere near it; an anchor provider would refuse and the detail
		// would never be registered.
		if (OVT_ObjectiveAnchorSourceProvider.Cast(insertion.m_Source))
			return "the repair config sources its detail from the objective anchor - it is evaluator-selectable and can be created with no objective running at all, so the insertion would resolve nothing and register nobody";

		return CheckGroupResolves(insertion.m_sGroupType);
	}

	//------------------------------------------------------------------------------------------------
	//! That BOTH shipped factions can field a group name, and that the prefab behind it actually loads.
	//! \param[in] groupType The registry name.
	//! \return An empty string when both factions field it, or why one does not.
	protected string CheckGroupResolves(string groupType)
	{
		if (groupType == "")
			return "the repair config authors an empty group type";

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
				return string.Format("the %1 registry cannot field '%2' - a campaign with that faction occupying would send no repair detail at all", factionKey, groupType);

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
//! The target filter takes ONLY this base's own ruined structures, while this base is ours.
//!
//! 🔴 ROW ONE IS WHAT MAKES THE MISSION MEAN ANYTHING. Admit an INTACT structure and a detail spends
//! its quota putting back things that were never broken, and a base with no ruins never reports that
//! it has nothing to do - so it holds the single instance slot for the rest of the campaign.
//!
//! ⚠ ROW TWO IS SABOTAGE'S FIRST ROW, INVERTED: sabotage refuses a base we already hold, repair
//! refuses one we do NOT.
//!
//! Inputs are strings, enums and booleans rather than entities, the same seam the sabotage suite uses.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   B1. `if (!isRuined) return false;` deleted. Fails on "an intact structure is never repaired".
//!   B2. `if (targetBaseFaction != myFaction) return false;` deleted. Fails on "a base the resistance
//!       holds is never repaired".
//!   B3. The final comparison changed to `return true;`. Fails on "another base's structures are not
//!       this base's".
//!   B4. `if (associatedType != EOVTBaseType.BASE) return false;` deleted. Fails on "a camp's
//!       structures are not this base's".
//!   B5. `if (targetBaseId == "") return false;` deleted. Fails on "an unresolved base id matches
//!       nothing".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveRepair_BTargetFilterTakesRuinsAtOurOwnBase : SCR_AutotestCaseBase
{
	//! Any two different integers make the claim; real faction indices would only couple the case to
	//! campaign setup.
	static const int MY_FACTION = 7;
	static const int ENEMY_FACTION = 8;

	static const string THIS_BASE = "4";
	static const string OTHER_BASE = "9";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- The one row that must be accepted. Asserted first, so a filter that refuses everything
		// cannot pass the whole case by accident.
		if (!Expect(THIS_BASE, EOVTBaseType.BASE, THIS_BASE, MY_FACTION, true, true, "a ruin at our own base IS a repair target"))
			return true;

		// --- 🔴 THE ROW THAT DEFINES THE MISSION. Nothing broken here.
		if (!Expect(THIS_BASE, EOVTBaseType.BASE, THIS_BASE, MY_FACTION, false, false, "an intact structure is never repaired - a detail that took one would spend its quota rebuilding things that were never broken, and a base with no ruins would never report that it had nothing to do"))
			return true;

		// --- The base is theirs now. Sabotage's first row, inverted.
		if (!Expect(THIS_BASE, EOVTBaseType.BASE, THIS_BASE, ENEMY_FACTION, true, false, "a base the resistance holds is never repaired - the occupying faction does not rebuild what it has just lost"))
			return true;

		// --- Somewhere else entirely.
		if (!Expect(OTHER_BASE, EOVTBaseType.BASE, THIS_BASE, MY_FACTION, true, false, "another base's structures are not this base's"))
			return true;

		// --- The right id, the wrong KIND of place. Ids are per-type, so a camp and a base can share
		// one and this is the only test that separates them.
		if (!Expect(THIS_BASE, EOVTBaseType.CAMP, THIS_BASE, MY_FACTION, true, false, "a camp's structures are not this base's, even with a matching id"))
			return true;

		if (!Expect(THIS_BASE, EOVTBaseType.FOB, THIS_BASE, MY_FACTION, true, false, "a forward base's structures are not this base's, even with a matching id"))
			return true;

		if (!Expect(THIS_BASE, EOVTBaseType.NONE, THIS_BASE, MY_FACTION, true, false, "an unassociated structure is not this base's"))
			return true;

		// --- Nothing associated at all: furniture in a house, a poster on a wall.
		if (!Expect("", EOVTBaseType.BASE, THIS_BASE, MY_FACTION, true, false, "a structure with no recorded base is not this base's"))
			return true;

		// --- And the inverse: a base whose id resolved to nothing must not match every loose object.
		if (!Expect("", EOVTBaseType.BASE, "", MY_FACTION, true, false, "an unresolved base id matches nothing, rather than everything unassociated"))
			return true;

		Print("Objective repair: only RUINED structures associated with this base are targets, and only while the occupying faction still holds it");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one filter row, naming it in the failure message.
	//! \param[in] associatedId The structure's recorded association.
	//! \param[in] associatedType The structure's recorded association type.
	//! \param[in] targetBaseId The base being repaired.
	//! \param[in] targetBaseFaction Who holds that base.
	//! \param[in] isRuined Whether the structure is currently a ruin.
	//! \param[in] expected Whether it must be a target.
	//! \param[in] label Human description of the row, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool Expect(string associatedId, EOVTBaseType associatedType, string targetBaseId, int targetBaseFaction, bool isRuined, bool expected, string label)
	{
		bool actual = OVT_BaseRepairBehaviorDeploymentModule.IsRepairTarget(associatedId, associatedType, targetBaseId, targetBaseFaction, MY_FACTION, isRuined);

		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! A PLAYER STANDING AT THE BASE PAUSES THE WORK - and the clock is never rolled back.
//!
//! ⚠ PAUSING AND RESETTING ARE INDISTINGUISHABLE ON ANY SINGLE TICK: both refuse the repair. They
//! differ only in what happens next, and a reset lets a defender who walks past every couple of
//! minutes immunise the base forever.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   C1. The interrupted branch changed to reset ticksLeft to its starting value. Fails on "an
//!       interruption must PAUSE the clock, not reset it".
//!   C2. `if (enemyPresent) return false;` deleted. Fails on "a defended base must not advance the
//!       repair clock".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveRepair_CHoldIntervalPausesRatherThanResetting : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_BaseRepairBehaviorDeploymentModule repair = new OVT_BaseRepairBehaviorDeploymentModule();

		int ticks = 3;

		// --- Held and unopposed: one tick spent.
		if (repair.EvaluateRepair(4, false, ticks))
		{
			SetFailure("an interval with three ticks left must not complete on the first of them");
			return true;
		}

		if (ticks != 2)
		{
			SetFailure("a held, unopposed tick must advance the clock by exactly one: %1 tick(s) left, expected 2", ticks.ToString());
			return true;
		}

		// --- 🔴 A PLAYER WALKS IN. The work stops, and the two ticks already served are KEPT.
		if (repair.EvaluateRepair(4, true, ticks))
		{
			SetFailure("a defended base must not complete a repair interval");
			return true;
		}

		if (ticks != 2)
		{
			SetFailure("an interruption must PAUSE the clock, not reset it: %1 tick(s) left, expected 2 - a reset lets a defender who walks past every couple of minutes stop the work forever", ticks.ToString());
			return true;
		}

		// --- They leave. The work resumes from where it stopped (two owed) rather than from the start (three).
		if (repair.EvaluateRepair(4, false, ticks))
		{
			SetFailure("an interval resuming with two ticks owed must not complete on the first of them");
			return true;
		}

		if (ticks != 1)
		{
			SetFailure("the resumed tick must leave exactly one owed: %1 tick(s) left, expected 1", ticks.ToString());
			return true;
		}

		if (!repair.EvaluateRepair(4, false, ticks))
		{
			SetFailure("the paused interval must complete on its last owed tick - a reset would still have two left here, a pause has none");
			return true;
		}

		Print("Objective repair: a player inside the clear radius pauses the repair interval and never resets it");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The repair module clones every authored attribute, does not clone its mission latch, and reads its
//! two live figures out of the campaign's difficulty rather than out of its own attributes.
//!
//! ⚠ CloneModule IS HAND-COPIED AND UNCHAINED: a forgotten line ships the class default on every
//! deployment forever, silently. A dropped m_fClearRadius means nothing ever counts as holding the
//! base; a dropped m_fSearchRadius means every detail finds nothing.
//!
//! ⚠ THE LIVE HALF OF THE PRECEDENCE CLAIM. The pure rule is asserted in the Logic tier; only here
//! can "the module reads the CAMPAIGN'S numbers" be asserted. It also pins BD23's join to the
//! SABOTAGE difficulty fields - split them later and this names the second place to change.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   D1. `clone.m_fSearchRadius = m_fSearchRadius;` deleted. Fails on "dropped m_fSearchRadius".
//!   D2. `clone.m_iStructuresPerMission = m_iStructuresPerMission;` deleted. Fails on "dropped
//!       m_iStructuresPerMission".
//!   D3. `clone.m_bMissionReported = m_bMissionReported;` ADDED. Fails on "a clone must not inherit a
//!       finished mission".
//!   D4. ResolveIntervalTicks' difficulty lookup deleted, leaving only the attribute. Fails on "the
//!       repair interval must come from difficulty".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveRepair_DCloneFidelityAndDifficultyPrecedence : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string failure = CheckClone();
		if (failure == "")
			failure = CheckDifficultyPrecedence();

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective repair: the module clone carries every authored attribute, never inherits a finished mission, and takes its interval and its quota from the campaign's difficulty rather than from its own fallbacks");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckClone()
	{
		OVT_BaseRepairBehaviorDeploymentModule source = new OVT_BaseRepairBehaviorDeploymentModule();

		// Every value is NOT the class default, so a dropped line reads as the default and fails rather
		// than passing by coincidence.
		source.m_sModuleName = "fixture repair";
		source.m_fClearRadius = 211;
		source.m_fSearchRadius = 733;
		source.m_fMaxBaseDistance = 419;
		source.m_iHoldSeconds = 317;
		source.m_iStructuresPerMission = 7;

		// Stop the mission, so the clone has a latch it could wrongly inherit.
		source.AbortMission();

		OVT_BaseRepairBehaviorDeploymentModule clone = OVT_BaseRepairBehaviorDeploymentModule.Cast(source.CloneModule());
		if (!clone)
			return "the repair module's CloneModule did not return a module of its own type";

		if (clone.m_sModuleName != source.m_sModuleName)
			return "the repair clone dropped m_sModuleName";

		if (clone.m_fClearRadius != source.m_fClearRadius)
			return string.Format("the repair clone dropped m_fClearRadius: %1, expected %2 - nothing would ever count as holding the base and nothing would ever be repaired",
				clone.m_fClearRadius.ToString(), source.m_fClearRadius.ToString());

		if (clone.m_fSearchRadius != source.m_fSearchRadius)
			return string.Format("the repair clone dropped m_fSearchRadius: %1, expected %2 - every detail would find nothing and stand down beside the ruin it was sent to fix",
				clone.m_fSearchRadius.ToString(), source.m_fSearchRadius.ToString());

		if (clone.m_fMaxBaseDistance != source.m_fMaxBaseDistance)
			return string.Format("the repair clone dropped m_fMaxBaseDistance: %1, expected %2",
				clone.m_fMaxBaseDistance.ToString(), source.m_fMaxBaseDistance.ToString());

		if (clone.m_iHoldSeconds != source.m_iHoldSeconds)
			return string.Format("the repair clone dropped m_iHoldSeconds: %1, expected %2",
				clone.m_iHoldSeconds.ToString(), source.m_iHoldSeconds.ToString());

		if (clone.m_iStructuresPerMission != source.m_iStructuresPerMission)
			return string.Format("the repair clone dropped m_iStructuresPerMission: %1, expected %2",
				clone.m_iStructuresPerMission.ToString(), source.m_iStructuresPerMission.ToString());

		if (!source.HasMissionReported())
			return "the fixture failed to stop the source mission, so the next claim would pass vacuously";

		if (clone.HasMissionReported())
			return "a clone must not inherit a finished mission - every deployment built from that template would stand down without repairing anything";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckDifficultyPrecedence()
	{
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (!difficulty)
			return "OVT_Global.GetDifficulty() is null - the campaign's difficulty settings are not loaded";

		if (difficulty.objectiveSabotageStructuresPerMission < 1)
			return string.Format("the campaign's objectiveSabotageStructuresPerMission must be at least one: it is %1",
				difficulty.objectiveSabotageStructuresPerMission.ToString());

		if (difficulty.objectiveSabotageHoldSeconds < 1)
			return string.Format("the campaign's objectiveSabotageHoldSeconds must be at least one: it is %1",
				difficulty.objectiveSabotageHoldSeconds.ToString());

		OVT_BaseRepairBehaviorDeploymentModule module = new OVT_BaseRepairBehaviorDeploymentModule();

		// ⚠ DELIBERATELY DIFFERENT FROM EVERY AUTHORED PRESET. `new` applies no [Attribute()] default,
		// so these are the only values the fallback path could produce - if either shows up in the
		// answer, difficulty lost.
		module.m_iStructuresPerMission = 91;
		module.m_iHoldSeconds = 9130;

		if (module.ResolveStructuresPerMission() != difficulty.objectiveSabotageStructuresPerMission)
			return string.Format("the per-mission quota must come from difficulty, not from the module's fallback: got %1, expected %2",
				module.ResolveStructuresPerMission().ToString(), difficulty.objectiveSabotageStructuresPerMission.ToString());

		int expectedTicks = difficulty.objectiveSabotageHoldSeconds / OVT_BaseRepairBehaviorDeploymentModule.UPDATE_SECONDS;
		if (expectedTicks < 1)
			expectedTicks = 1;

		if (module.ResolveIntervalTicks() != expectedTicks)
			return string.Format("the repair interval must come from difficulty, not from the module's fallback: got %1 update(s), expected %2",
				module.ResolveIntervalTicks().ToString(), expectedTicks.ToString());

		if (module.ResolveIntervalTicks() < 1)
			return "a repair interval must be at least one update, or a structure comes back on the update the detail is registered";

		return "";
	}
}
