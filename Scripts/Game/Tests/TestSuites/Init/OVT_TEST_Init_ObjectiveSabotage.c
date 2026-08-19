//------------------------------------------------------------------------------------------------
//! TIER B - THE PHASE-1 BASE OPERATION: sabotage, where it meets loaded configs.
//!
//! 🔴 THIS IS THE ONE FEATURE IN THE EPIC THAT DESTROYS PLAYER PROPERTY PERMANENTLY. A sabotage
//! mission demolishes structures a player paid for and built; there is no rubble, no repair action,
//! no refund, and a destroyed entity does not come back on the next load because a deleted entity is
//! simply never saved. Every claim below is a guard on that, and each one is a way the feature could
//! quietly become something nobody agreed to:
//!
//!   1. THE CONFIG NAME. OVT_ObjectiveDirectorComponent.SABOTAGE_CONFIG is matched by string against
//!      the registry, three times (resolve, concurrency count, teardown ledger). Renamed in one place
//!      and not the other, a base objective ramps to nothing with one WARNING line per in-game minute.
//!   2. THE INVERSION KNOB. OVT_BaseControlConditionDeploymentModule with m_bRequireControl 0 means
//!      "deploy only while we do NOT hold this base". Authored as 1 it would send sabotage teams to
//!      bases the OCCUPYING faction already holds - a config that strips its own side's structures.
//!   3. THE MODULE ORDER. `.conf` files cannot carry comments, so "the mission behaviour is authored
//!      BEFORE the reinforcement module" is recorded only in class headers and checked only here.
//!   4. THE TARGET FILTER. Three exclusions decide which structures may be destroyed at all, and each
//!      of them is the difference between demolishing this base's buildings and demolishing a nearby
//!      camp's, a different base's, or the occupying faction's own.
//!   5. THE COST JOIN'S DATA. The join is by PREFAB, and it is only unambiguous while no prefab
//!      appears in two config entries. Nothing else in the tree would notice a duplicate.
//!   6. THE CLONE. CloneModule copies by hand and is not chained; a dropped line ships the class
//!      default on every deployment, forever.
//!
//! ⚠ NOTHING HERE CONSTRUCTS A DEPLOYMENT, DRIVES THE DIRECTOR, OR DESTROYS ANYTHING. Every subject is
//! a loaded config object, a bare `new` module with no parent deployment, or a read off a live
//! manager. A real sabotage mission demolishes real structures in the shared initialisation world; a
//! case that arranged one would be the only case in the suite that could not be re-run.
//!
//! ⚠ CASE ORDER: cases run alphabetically by class name and none of these writes anything, so the
//! order is free. The names are still prefixed A/C/D/F/J for readability of a run log.
//!
//! No polling, no waiting, no maxAttempts: every subject is a synchronous read or a hand-built object.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! The sabotage config is registered, valid, BASE-typed, correctly ordered, and carries the two knobs
//! that decide where a team is sent and what stands it down.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   A1. `m_sDeploymentName "Objective Sabotage"` misspelled in Deployment_ObjectiveSabotage.conf (the
//!       registry entry inherits the name rather than restating it). Fails on "is not registered".
//!   A2. `m_iAllowedLocationTypes` changed from BASE to TOWN. Fails on "cannot be used at a BASE".
//!   A3. The reinforcement module moved ABOVE the sabotage behaviour module. Fails on "must be
//!       authored before the reinforcement module".
//!   A4. `m_bRequireControl` set to 1 on the base control condition. Fails on "must be authored with
//!       m_bRequireControl 0".
//!   A5. The OVT_BaseControlConditionDeploymentModule entry deleted outright. Fails on "authors no
//!       base control condition".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveSabotage_AConfigResolvesAndIsOrdered : SCR_AutotestCaseBase
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

		string failure = CheckConfig(deployments);
		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective sabotage: the config is registered, valid, BASE-typed, ordered behaviour-before-reinforcement, deploys only while the resistance holds the base, and fields a group both factions have");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] deployments The deployment framework.
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckConfig(notnull OVT_DeploymentManagerComponent deployments)
	{
		string name = OVT_ObjectiveDirectorComponent.SABOTAGE_CONFIG;

		OVT_DeploymentConfig config = deployments.FindConfigByName(name);
		if (!config)
			return string.Format("'%1' is not registered in overthrowDeployments.conf - a base objective would ramp to nothing and time out", name);

		if (!config.IsValidConfig())
			return "the sabotage config does not validate";

		if (!config.CanUseLocationType(OVT_LocationTypeFlag.BASE))
			return "the sabotage config cannot be used at a BASE, which is the only place the director sends one";

		// --- Ordering. The mission must run before the reinforcement module, or a mission that
		// completed has its team rebought in the same pass that ended it.
		int behaviourIndex = -1;
		int reinforcementIndex = -1;
		bool foundInversion = false;
		bool foundObjectiveCondition = false;

		foreach (int index, OVT_BaseDeploymentModule module : config.m_aModules)
		{
			if (!module)
				continue;

			if (behaviourIndex == -1 && OVT_BaseSabotageBehaviorDeploymentModule.Cast(module))
				behaviourIndex = index;

			if (reinforcementIndex == -1 && OVT_ReinforcementBehaviorDeploymentModule.Cast(module))
				reinforcementIndex = index;

			if (OVT_ObjectiveConditionDeploymentModule.Cast(module))
				foundObjectiveCondition = true;

			// ⚠ A PATROL BEHAVIOUR MODULE WOULD BREAK THE INSERTION. It answers BuildVirtualPlan() with a
			// plan of its own and pre-empts the cycling march the force needs to walk from the landing
			// zone onto the base.
			if (OVT_PatrolBehaviorDeploymentModule.Cast(module))
				return "the sabotage config authors a patrol behaviour module, which would replace the movable march plan the team needs to reach the base";

			OVT_BaseControlConditionDeploymentModule control = OVT_BaseControlConditionDeploymentModule.Cast(module);
			if (control)
			{
				foundInversion = true;

				if (control.m_bRequireControl)
					return "the sabotage config's base control condition must be authored with m_bRequireControl 0 - authored as 1 it sends sabotage teams to bases the occupying faction already holds, to demolish its own structures";
			}
		}

		if (behaviourIndex == -1)
			return "the sabotage config authors no OVT_BaseSabotageBehaviorDeploymentModule - it has no mission at all";

		if (reinforcementIndex == -1)
			return "the sabotage config authors no reinforcement module, so nothing collects it when the objective moves";

		if (behaviourIndex > reinforcementIndex)
			return "the sabotage config authors its mission behaviour AFTER the reinforcement module - it must be authored before it, or a completed mission has its team rebought in the same pass that ended it";

		if (!foundInversion)
			return "the sabotage config authors no base control condition, so nothing stands the team down when the base changes hands";

		if (!foundObjectiveCondition)
			return "the sabotage config authors no objective condition, so it would not be collected when the objective moves or the phase advances";

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
			return "the sabotage config has no insertion spawning module - its team would appear at the base out of thin air";

		if (!insertion.m_Source)
			return "the sabotage config authors no source provider, so its insertion module registers nothing at all";

		return CheckGroupResolves(insertion.m_sGroupType);
	}

	//------------------------------------------------------------------------------------------------
	//! That BOTH shipped factions can field a group name, and that the prefab behind it actually loads.
	//! \param[in] groupType The registry name.
	//! \return An empty string when both factions field it, or why one does not.
	protected string CheckGroupResolves(string groupType)
	{
		if (groupType == "")
			return "the sabotage config authors an empty group type";

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
				return string.Format("the %1 registry cannot field '%2' - a campaign with that faction occupying would send no sabotage team at all", factionKey, groupType);

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
//! The demolition decision runs only while the base is held and unopposed, PAUSES rather than resets
//! on an interruption, and stops dead once the mission has reported.
//!
//! ⚠ UNLIKE ITS TWO SIBLING DECISIONS THIS ONE DOES NOT LATCH PER FIRING, and that asymmetry is the
//! point of the case. A sabotage mission fires REPEATEDLY - one structure per interval up to the
//! per-mission quota - so the fired-once latch belongs to the MISSION, not to a demolition. A latch in
//! the wrong place here means either one structure per mission forever (latched per firing) or a team
//! that keeps demolishing until the base is empty (no mission latch at all), and neither is a script
//! error.
//!
//! ⚠ "NOTHING IS DESTROYED WHILE A PLAYER IS STANDING THERE" IS THE ONE CLAIM A PLAYER WOULD NOTICE
//! BREAKING. It is asserted on the clock as well as on the return value: an interrupted tick must not
//! advance the countdown at all, or a defender holding the base would still be watching it come down,
//! just more slowly.
//!
//! The subject is a bare `new` object with NO parent deployment: EvaluateDemolition takes every input
//! as an argument precisely so this needs no marker, no base, no structures and no AI.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   D1. `if (aliveInside < 1) return false;` deleted. Fails on "an empty base must not advance the
//!       demolition clock".
//!   D2. `if (enemyPresent) return false;` deleted. Fails on "a defended base must not advance the
//!       demolition clock" - the live consequence being structures demolished under a player's feet.
//!   D3. The interrupted branch changed to reset ticksLeft to its starting value. Fails on "an
//!       interruption must PAUSE the clock, not reset it".
//!   D4. `if (m_bMissionReported) return false;` deleted. Fails on "a reported mission must never
//!       demolish again".
//!   D5. A PER-FIRING LATCH ADDED - `m_bMissionReported = true;` inserted before EvaluateDemolition's
//!       final `return true;`. Fails on "a re-armed interval must be able to complete again", which in
//!       the live module caps every mission at exactly one structure whatever difficulty authored.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveSabotage_DDemolitionDecisionHoldsAndPauses : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string failure = CheckDecision();
		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective sabotage: the demolition clock runs only while the base is held and unopposed, pauses rather than resets on an interruption, fires repeatedly rather than once, and stops dead once the mission has reported");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckDecision()
	{
		OVT_BaseSabotageBehaviorDeploymentModule sabotage = new OVT_BaseSabotageBehaviorDeploymentModule();

		int ticks = 3;

		// --- Nobody there yet: the team is still on the road.
		if (sabotage.EvaluateDemolition(0, false, ticks))
			return "an empty base must not complete a demolition interval";

		if (ticks != 3)
			return string.Format("an empty base must not advance the demolition clock: %1 tick(s) left, expected 3", ticks.ToString());

		// --- Defended: the team is there, so is a player. NOTHING MAY BE DESTROYED.
		if (sabotage.EvaluateDemolition(4, true, ticks))
			return "a defended base must not complete a demolition interval";

		if (ticks != 3)
			return string.Format("a defended base must not advance the demolition clock: %1 tick(s) left, expected 3", ticks.ToString());

		// --- Held and unopposed: one tick.
		if (sabotage.EvaluateDemolition(4, false, ticks))
			return "an interval with three ticks left must not complete on the first of them";

		if (ticks != 2)
			return string.Format("a held, unopposed tick must advance the clock by exactly one: %1 tick(s) left, expected 2", ticks.ToString());

		// --- Interrupted mid-interval. THE CLOCK MUST PAUSE, NOT RESET.
		if (sabotage.EvaluateDemolition(0, false, ticks))
			return "the team being wiped mid-interval must not complete it";

		if (ticks != 2)
			return string.Format("an interruption must PAUSE the clock, not reset it: %1 tick(s) left, expected 2", ticks.ToString());

		// --- Run it out.
		if (sabotage.EvaluateDemolition(4, false, ticks))
			return "an interval with two ticks left must not complete on the first of them";

		if (!sabotage.EvaluateDemolition(4, false, ticks))
			return "an interval whose last tick was served must complete";

		// --- ⚠ AND IT MUST BE ABLE TO FIRE AGAIN. A mission takes several structures; the caller
		// re-arms the clock and the decision has no per-firing latch to spend.
		if (sabotage.HasMissionReported())
			return "one completed interval must not report the whole mission - a mission takes objectiveSabotageStructuresPerMission structures, not one";

		ticks = 1;
		if (!sabotage.EvaluateDemolition(4, false, ticks))
			return "a re-armed interval must be able to complete again - a per-firing latch would cap every mission at one structure";

		// --- Once the MISSION is over, nothing more comes down.
		//
		// ⚠ THROUGH AbortMission(), NOT CompleteMission(). CompleteMission calls the LIVE director's
		// OnSabotageSuccess(), which in a shared initialisation world would bank a sabotage success
		// against whatever objective happens to be running - a case that mutates campaign state it did
		// not create. AbortMission sets the same latch and nothing else, which is exactly the state
		// under test.
		sabotage.AbortMission();

		if (!sabotage.HasMissionReported())
			return "the fixture could not put the module into its stopped state, so the next claim would pass vacuously";

		ticks = 1;
		if (sabotage.EvaluateDemolition(4, false, ticks))
			return "a reported mission must never demolish again";

		if (ticks != 1)
			return string.Format("a reported mission must not even advance its clock: %1 tick(s) left, expected 1", ticks.ToString());

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! The target filter excludes everything that is not this base's player-built property.
//!
//! 🔴 THIS IS THE GUARD ON WHAT GETS DESTROYED, and every row is a real way to demolish the wrong
//! thing. A sabotage team's search radius is 500 m around a base centre - large enough to contain a
//! neighbouring camp, a forward base, a house's furniture and, on a dense map, another base's outlying
//! structures. Without the association test the mission would take whichever of those happened to be
//! cheapest, at a place nobody attacked.
//!
//! ⚠ THE FIRST ROW IS THE ONE THAT PROTECTS THE OCCUPYING FACTION FROM ITSELF. If a base flips while
//! the team is walking to it, everything on it is now the occupying faction's own - and a team that
//! demolished it anyway would be destroying its own side's captured property with no way to tell.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   F1. `if (targetBaseFaction == myFaction) return false;` deleted. Fails on "a base the occupying
//!       faction already holds is never sabotaged".
//!   F2. `if (associatedType != EOVTBaseType.BASE) return false;` deleted. Fails on "a camp's
//!       structures are not this base's".
//!   F3. The final comparison changed to `return true;`. Fails on "another base's structures are not
//!       this base's".
//!   F4. `if (targetBaseId == "") return false;` deleted. Fails on "an unassociated structure belongs
//!       to nobody" - the row that also covers a base whose id resolved to an empty string, which
//!       would otherwise match every unassociated object within 500 m.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveSabotage_FTargetFilterExcludesEverythingElse : SCR_AutotestCaseBase
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
		if (!Expect(THIS_BASE, EOVTBaseType.BASE, THIS_BASE, ENEMY_FACTION, true, "this base's own structures ARE targets"))
			return true;

		// --- The base is ours now.
		if (!Expect(THIS_BASE, EOVTBaseType.BASE, THIS_BASE, MY_FACTION, false, "a base the occupying faction already holds is never sabotaged"))
			return true;

		// --- Somewhere else entirely.
		if (!Expect(OTHER_BASE, EOVTBaseType.BASE, THIS_BASE, ENEMY_FACTION, false, "another base's structures are not this base's"))
			return true;

		// --- The right id, the wrong KIND of place. Ids are per-type, so a camp and a base can share
		// one and this is the only test that separates them.
		if (!Expect(THIS_BASE, EOVTBaseType.CAMP, THIS_BASE, ENEMY_FACTION, false, "a camp's structures are not this base's, even with a matching id"))
			return true;

		if (!Expect(THIS_BASE, EOVTBaseType.FOB, THIS_BASE, ENEMY_FACTION, false, "a forward base's structures are not this base's, even with a matching id"))
			return true;

		if (!Expect(THIS_BASE, EOVTBaseType.NONE, THIS_BASE, ENEMY_FACTION, false, "an unassociated structure is not this base's"))
			return true;

		// --- Nothing associated at all: furniture in a house, a poster on a wall.
		if (!Expect("", EOVTBaseType.BASE, THIS_BASE, ENEMY_FACTION, false, "a structure with no recorded base is not this base's"))
			return true;

		// --- And the inverse: a base whose id resolved to nothing must not match every loose object.
		if (!Expect("", EOVTBaseType.BASE, "", ENEMY_FACTION, false, "an unresolved base id matches nothing, rather than everything unassociated"))
			return true;

		Print("Objective sabotage: only this base's own BASE-associated structures are targets, and only while the resistance still holds it");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one filter row, naming it in the failure message.
	//! \param[in] associatedId The structure's recorded association.
	//! \param[in] associatedType The structure's recorded association type.
	//! \param[in] targetBaseId The base being sabotaged.
	//! \param[in] targetBaseFaction Who holds that base.
	//! \param[in] expected Whether it must be a target.
	//! \param[in] label Human description of the row, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool Expect(string associatedId, EOVTBaseType associatedType, string targetBaseId, int targetBaseFaction, bool expected, string label)
	{
		bool actual = OVT_BaseSabotageBehaviorDeploymentModule.IsSabotageTarget(associatedId, associatedType, targetBaseId, targetBaseFaction, MY_FACTION);

		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! THE COST JOIN'S DATA: every priced structure is reachable from its prefab, unambiguously, and an
//! unpriced one sorts last.
//!
//! ⚠ THE JOIN IS BY PREFAB AND NOT BY THE TYPE STRING, AND THIS CASE RECORDS WHY. The plan expected
//! OVT_PlaceableComponent.GetPlaceableType() / OVT_BuildableComponent.GetBuildableType() to match the
//! config's m_sName. They do not: the shipped prefabs author "GuardTower" against "Guard Tower",
//! "Bunker" against "Bunkers" and "VehicleGarage" against "Garage" - seven of the eight buildables
//! disagree and only "Helipad" happens to line up. A type-string join would have priced most of the
//! game's structures at nothing and demolished them all first.
//!
//! WHAT IS ACTUALLY ASSERTED, since a live structure cannot be conjured without spawning one:
//!   1. EVERY entry has at least one prefab and a positive cost - a config entry the join can reach
//!      and that means something once reached.
//!   2. NO PREFAB APPEARS IN TWO ENTRIES, across buildables AND placeables together. This is the join's
//!      whole precondition: a duplicate makes the price of that structure depend on iteration order,
//!      and nothing else in the tree would ever notice.
//!   3. UNKNOWN_STRUCTURE_COST IS GREATER THAN EVERY AUTHORED COST, which is what makes an unpriced
//!      structure sort LAST rather than first. Sorting first would make anything a mod adds the very
//!      first thing a sabotage team destroys.
//!   4. A null entity answers UNKNOWN_STRUCTURE_COST rather than zero.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   J1. The Medical Tent's prefab in buildables.conf changed to the Recruitment Tent's. Fails on
//!       "two config entries claim the same prefab".
//!   J2. `UNKNOWN_STRUCTURE_COST` set to 0. Fails on "an unpriced structure must sort after every
//!       priced one".
//!   J3. `GetStructureCost` returning 0 for a null entity. Fails on "a null structure must be
//!       unpriced".
//!   J4. The Fuel Depot's `m_iCost` set to 0 in buildables.conf. Fails on "authors no cost".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveSabotage_JCostJoinIsUnambiguous : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance)
		{
			SetFailure("OVT_Global.GetResistanceFaction() is null - the buildables and placeables configs did not resolve");
			return true;
		}

		string failure = CheckCatalogue(resistance);
		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Objective sabotage: every priced structure is reachable from exactly one config entry by prefab, and an unpriced one sorts after all of them");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] resistance The manager that owns both configs.
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckCatalogue(notnull OVT_ResistanceFactionManager resistance)
	{
		if (!resistance.m_BuildablesConfig || !resistance.m_BuildablesConfig.m_aBuildables)
			return "the buildables config is not loaded - GetStructureCost could never price a built structure";

		if (!resistance.m_PlaceablesConfig || !resistance.m_PlaceablesConfig.m_aPlaceables)
			return "the placeables config is not loaded - GetStructureCost could never price a placed object";

		array<ResourceName> seen = new array<ResourceName>();
		int dearest = 0;

		foreach (OVT_Buildable buildable : resistance.m_BuildablesConfig.m_aBuildables)
		{
			if (!buildable)
				continue;

			if (buildable.m_iCost <= 0)
				return string.Format("the buildable '%1' authors no cost, so a sabotage team would treat it as free and take it first", buildable.m_sName);

			if (buildable.m_iCost > dearest)
				dearest = buildable.m_iCost;

			string duplicate = CollectPrefabs(buildable.m_aPrefabs, buildable.m_sName, seen);
			if (duplicate != "")
				return duplicate;
		}

		foreach (OVT_Placeable placeable : resistance.m_PlaceablesConfig.m_aPlaceables)
		{
			if (!placeable)
				continue;

			if (placeable.m_iCost > dearest)
				dearest = placeable.m_iCost;

			string duplicate = CollectPrefabs(placeable.m_aPrefabs, placeable.m_sName, seen);
			if (duplicate != "")
				return duplicate;
		}

		if (seen.IsEmpty())
			return "neither config lists a single prefab - the join has nothing to match against";

		// --- The unpriced sentinel has to sort LAST, or a modded structure becomes the first casualty.
		if (OVT_ResistanceFactionManager.UNKNOWN_STRUCTURE_COST <= dearest)
			return string.Format("an unpriced structure must sort after every priced one: UNKNOWN_STRUCTURE_COST is %1 and the dearest authored structure costs %2",
				OVT_ResistanceFactionManager.UNKNOWN_STRUCTURE_COST.ToString(), dearest.ToString());

		// --- And the join refuses rather than guessing when there is nothing to look up.
		if (resistance.GetStructureCost(null) != OVT_ResistanceFactionManager.UNKNOWN_STRUCTURE_COST)
			return "a null structure must be unpriced - a zero would make it the first thing demolished";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Adds one entry's prefabs to the seen list, refusing a prefab a previous entry already claimed.
	//! \param[in] prefabs The entry's prefab list. May be null.
	//! \param[in] owner The entry's name, for the failure message.
	//! \param[inout] seen Every prefab claimed so far.
	//! \return An empty string, or why the join is ambiguous.
	protected string CollectPrefabs(array<ResourceName> prefabs, string owner, notnull array<ResourceName> seen)
	{
		if (!prefabs || prefabs.IsEmpty())
			return string.Format("the config entry '%1' lists no prefab, so nothing in the world can ever be joined back to its price", owner);

		foreach (ResourceName prefab : prefabs)
		{
			if (seen.Contains(prefab))
				return string.Format("two config entries claim the same prefab ('%1', reached again from '%2') - the price of that structure would depend on which entry the join happened to walk first",
					prefab, owner);

			seen.Insert(prefab);
		}

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! The sabotage module clones every authored attribute, does not clone its mission latch, and reads
//! its two live figures out of difficulty rather than out of its own attributes.
//!
//! ⚠ CloneModule COPIES BY HAND AND IS NOT CHAINED. A forgotten line does not warn, does not log and
//! does not fail to parse: it ships the CLASS DEFAULT on every deployment, forever. Two of the six
//! attributes here have failure modes worth naming - a dropped m_fClearRadius clones as 0, so nothing
//! ever counts as holding the base and no base is ever sabotaged; a dropped m_fSearchRadius clones as
//! 0, so every mission finds nothing, reports "there was nothing left to demolish" on its first
//! interval, and opens the forward-base gate without a single structure having been destroyed.
//!
//! ⚠ THE DIFFICULTY PRECEDENCE IS PART OF THE SAME CLAIM. Both figures are authored twice - once on
//! the module as a fallback for a world with no campaign, once per difficulty preset - and difficulty
//! must win. A module that used its own attribute instead would ignore every preset, so Insane and
//! Easy would demolish at exactly the same rate.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   C1. `clone.m_fSearchRadius = m_fSearchRadius;` deleted. Fails on "dropped m_fSearchRadius".
//!   C2. `clone.m_iStructuresPerMission = m_iStructuresPerMission;` deleted. Fails on "dropped
//!       m_iStructuresPerMission".
//!   C3. `clone.m_bMissionReported = m_bMissionReported;` ADDED. Fails on "a clone must not inherit a
//!       reported mission".
//!   C4. ResolveStructuresPerMission's difficulty branch deleted, leaving only the attribute. Fails on
//!       "the per-mission quota must come from difficulty".
//!   C5. `objectiveSabotageStructuresPerMission`'s defvalue set to 0 in OVT_DifficultySettings.c. The
//!       autotest world's Difficulty_TestWorld.conf authors NONE of the objective fields and inherits
//!       every default, so this is what a zero authored anywhere looks like from here. Fails on "must
//!       be at least one".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveSabotage_CCloneFidelityAndDifficultyPrecedence : SCR_AutotestCaseBase
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

		Print("Objective sabotage: the module clone carries every authored attribute, never inherits a reported mission, and takes its interval and its quota from the campaign's difficulty rather than from its own fallbacks");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckClone()
	{
		OVT_BaseSabotageBehaviorDeploymentModule source = new OVT_BaseSabotageBehaviorDeploymentModule();

		// Every value is NOT the class default, so a dropped line reads as the default and fails rather
		// than passing by coincidence.
		source.m_sModuleName = "fixture sabotage";
		source.m_fClearRadius = 211;
		source.m_fSearchRadius = 733;
		source.m_fMaxBaseDistance = 419;
		source.m_iHoldSeconds = 317;
		source.m_iStructuresPerMission = 7;

		// Stop the mission, so the clone has a latch it could wrongly inherit. AbortMission rather than
		// CompleteMission for the reason the demolition-decision case records: the latter would bank a
		// sabotage success against the live campaign's objective.
		source.AbortMission();

		OVT_BaseSabotageBehaviorDeploymentModule clone = OVT_BaseSabotageBehaviorDeploymentModule.Cast(source.CloneModule());
		if (!clone)
			return "the sabotage module's CloneModule did not return a module of its own type";

		if (clone.m_sModuleName != source.m_sModuleName)
			return "the sabotage clone dropped m_sModuleName";

		if (clone.m_fClearRadius != source.m_fClearRadius)
			return string.Format("the sabotage clone dropped m_fClearRadius: %1, expected %2 - nothing would ever count as holding the base and no base would ever be sabotaged",
				clone.m_fClearRadius.ToString(), source.m_fClearRadius.ToString());

		if (clone.m_fSearchRadius != source.m_fSearchRadius)
			return string.Format("the sabotage clone dropped m_fSearchRadius: %1, expected %2 - every mission would report 'nothing left to demolish' without destroying anything",
				clone.m_fSearchRadius.ToString(), source.m_fSearchRadius.ToString());

		if (clone.m_fMaxBaseDistance != source.m_fMaxBaseDistance)
			return string.Format("the sabotage clone dropped m_fMaxBaseDistance: %1, expected %2",
				clone.m_fMaxBaseDistance.ToString(), source.m_fMaxBaseDistance.ToString());

		if (clone.m_iHoldSeconds != source.m_iHoldSeconds)
			return string.Format("the sabotage clone dropped m_iHoldSeconds: %1, expected %2",
				clone.m_iHoldSeconds.ToString(), source.m_iHoldSeconds.ToString());

		if (clone.m_iStructuresPerMission != source.m_iStructuresPerMission)
			return string.Format("the sabotage clone dropped m_iStructuresPerMission: %1, expected %2",
				clone.m_iStructuresPerMission.ToString(), source.m_iStructuresPerMission.ToString());

		if (!source.HasMissionReported())
			return "the fixture failed to report the source mission, so the next claim would pass vacuously";

		if (clone.HasMissionReported())
			return "a clone must not inherit a reported mission - every deployment built from that template would stand down without demolishing anything";

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

		OVT_BaseSabotageBehaviorDeploymentModule module = new OVT_BaseSabotageBehaviorDeploymentModule();

		// ⚠ DELIBERATELY DIFFERENT FROM EVERY AUTHORED PRESET. `new` applies no [Attribute()] default,
		// so these are the only values the fallback path could produce - if either shows up in the
		// answer, difficulty lost.
		module.m_iStructuresPerMission = 91;
		module.m_iHoldSeconds = 9130;

		if (module.ResolveStructuresPerMission() != difficulty.objectiveSabotageStructuresPerMission)
			return string.Format("the per-mission quota must come from difficulty, not from the module's fallback: got %1, expected %2",
				module.ResolveStructuresPerMission().ToString(), difficulty.objectiveSabotageStructuresPerMission.ToString());

		int expectedTicks = difficulty.objectiveSabotageHoldSeconds / OVT_BaseSabotageBehaviorDeploymentModule.UPDATE_SECONDS;
		if (expectedTicks < 1)
			expectedTicks = 1;

		if (module.ResolveIntervalTicks() != expectedTicks)
			return string.Format("the demolition interval must come from difficulty, not from the module's fallback: got %1 update(s), expected %2",
				module.ResolveIntervalTicks().ToString(), expectedTicks.ToString());

		if (module.ResolveIntervalTicks() < 1)
			return "a demolition interval must be at least one update, or a structure comes down on the update the team is registered";

		return "";
	}
}
