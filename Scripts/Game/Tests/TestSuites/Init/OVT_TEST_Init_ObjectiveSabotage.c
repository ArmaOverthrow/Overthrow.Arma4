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
//!   4. THE TARGET FILTER, IN TWO HALVES. WHAT KIND of thing may be demolished (buildables, never
//!      placeables) and WHOSE it has to be (three exclusions, each the difference between demolishing
//!      this base's buildings and demolishing a nearby camp's, a different base's, or the occupying
//!      faction's own).
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
//! order is free. The names are still prefixed A/C/D/F/G/J for readability of a run log.
//!
//! No polling, no waiting, no maxAttempts: every subject is a synchronous read or a hand-built object.
//------------------------------------------------------------------------------------------------

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
//!       interruption must RESET the clock to the whole interval, not pause it".
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

		Print("Objective sabotage: the demolition clock runs only while the base is held and unopposed, RESETS to the whole interval on an interruption, fires repeatedly rather than once, and stops dead once the mission has reported");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckDecision()
	{
		OVT_BaseSabotageBehaviorDeploymentModule sabotage = new OVT_BaseSabotageBehaviorDeploymentModule();

		const int FULL = 3;
		int ticks = FULL;

		// --- Nobody there yet: the team is still on the road.
		if (sabotage.EvaluateDemolition(0, false, FULL, ticks))
			return "an empty base must not complete a demolition interval";

		if (ticks != FULL)
			return string.Format("an empty base must leave the clock at the full interval: %1 tick(s) left, expected %2", ticks.ToString(), FULL.ToString());

		// --- Defended: the team is there, so is a defender. NOTHING MAY BE DESTROYED.
		if (sabotage.EvaluateDemolition(4, true, FULL, ticks))
			return "a defended base must not complete a demolition interval";

		if (ticks != FULL)
			return string.Format("a defended base must leave the clock at the full interval: %1 tick(s) left, expected %2", ticks.ToString(), FULL.ToString());

		// --- Held and unopposed: one tick.
		if (sabotage.EvaluateDemolition(4, false, FULL, ticks))
			return "an interval with three ticks left must not complete on the first of them";

		if (ticks != 2)
			return string.Format("a held, unopposed tick must advance the clock by exactly one: %1 tick(s) left, expected 2", ticks.ToString());

		// --- 🔴 INTERRUPTED MID-INTERVAL. THE CLOCK MUST RESET, NOT PAUSE (author, 2026-08-25).
		// Progress banked before an interruption is exactly what let a team that never held the ground
		// finish anyway.
		if (sabotage.EvaluateDemolition(0, false, FULL, ticks))
			return "the team being wiped mid-interval must not complete it";

		if (ticks != FULL)
			return string.Format("an interruption must RESET the clock to the whole interval, not pause it: %1 tick(s) left, expected %2", ticks.ToString(), FULL.ToString());

		// --- And the whole interval must now be served from the top.
		if (sabotage.EvaluateDemolition(4, false, FULL, ticks))
			return "a reset interval must not complete on its first tick";

		if (sabotage.EvaluateDemolition(4, false, FULL, ticks))
			return "a reset interval must not complete on its second tick";

		if (!sabotage.EvaluateDemolition(4, false, FULL, ticks))
			return "an interval whose last tick was served must complete";

		// --- ⚠ AND IT MUST BE ABLE TO FIRE AGAIN. A mission takes several structures; the caller
		// re-arms the clock and the decision has no per-firing latch to spend.
		if (sabotage.HasMissionReported())
			return "one completed interval must not report the whole mission - a mission takes objectiveSabotageStructuresPerMission structures, not one";

		ticks = 1;
		if (!sabotage.EvaluateDemolition(4, false, 1, ticks))
			return "a re-armed interval must be able to complete again - a per-firing latch would cap every mission at one structure";

		// --- Once the MISSION is over, nothing more comes down.
		//
		// ⚠ THROUGH AbortMission(), NOT CompleteMission(). CompleteMission calls the LIVE director's
		// the progress reporter, which in a shared initialisation world would bank a sabotage success
		// against whatever objective happens to be running - a case that mutates campaign state it did
		// not create. AbortMission sets the same latch and nothing else, which is exactly the state
		// under test.
		sabotage.AbortMission();

		if (!sabotage.HasMissionReported())
			return "the fixture could not put the module into its stopped state, so the next claim would pass vacuously";

		ticks = 1;
		if (sabotage.EvaluateDemolition(4, false, 1, ticks))
			return "a reported mission must never demolish again";

		if (ticks != 1)
			return string.Format("a reported mission must not even advance its clock: %1 tick(s) left, expected 1", ticks.ToString());

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! The target filter excludes everything that is not this base's player-built property.
//!
//! ⚠ THIS IS THE OWNERSHIP HALF OF THE FILTER ONLY - "whose is it". The other half, "what KIND of
//! thing is it", is OVT_BaseSabotageBehaviorDeploymentModule.IsCandidateStructure and is asserted by
//! the G case below. A structure has to pass BOTH.
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
//! ONLY BUILT STRUCTURES ARE CANDIDATES AT ALL. A PLACEABLE IS NOT A TARGET.
//!
//! 🔴 THIS EXPECTATION IS THE EXACT INVERSE OF WHAT THE MODULE DID UNTIL 2026-08-19, and the inversion
//! is the reason the case exists rather than a tightening of an old one. The filter used to accept
//! EITHER ownership component; the user's decision from a play-test was explicit - "placeables dont
//! actually make any sense to sabotage, they are just sandbags, furniture, lights, etc".
//!
//! ⚠ IT IS NOT A COSMETIC NARROWING, IT IS WHAT DECIDES WHAT THE COST SORT PICKS. Placeables are
//! priced 5-250 and the cheapest buildable is a 750 bunker, so under the old filter "cheapest first"
//! meant a base's clutter came down FIRST, every time, and a mission's two-structure quota was
//! routinely spent on a sign and a sandbag while the recruitment tent stood untouched. Re-admitting
//! placeables here would silently restore that - no error, no warning, just a demolition ladder that
//! never reaches anything the player built. That is what this case is here to catch.
//!
//! ⚠ THE INPUTS ARE BOOLEANS AND NOT ENTITIES, ON PURPOSE. The rule is asserted at the same seam
//! IsSabotageTarget is - every input passed in - because the alternative in an initialisation-tier
//! world is spawning a real buildable and a real placeable into the shared map to look at their
//! components, which is the one thing the header of this suite promises no case does.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   G1. `return hasBuildable;` changed back to `return hasPlaceable || hasBuildable;` - the exact
//!       pre-2026-08-19 rule. Fails on "a placeable is not a sabotage target".
//!   G2. `return hasBuildable;` changed to `return !hasPlaceable && hasBuildable;`, the plausible
//!       over-correction. Fails on "a structure carrying both components is still a built structure".
//!   G3. `return hasBuildable;` changed to `return false;`. Fails on "a buildable IS a sabotage
//!       target", the row asserted first so a filter that refuses everything cannot pass by accident.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveSabotage_GCandidateFilterTakesBuildablesOnly : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- The one row that must be accepted, asserted first.
		if (!Expect(false, true, true, "a buildable IS a sabotage target - the tents, the tower, the garage and the fuel depot are the whole point of the mission"))
			return true;

		// --- 🔴 THE INVERTED ROW. Sandbags, signs, lamps, chairs, cots, hedgehogs, posters.
		if (!Expect(true, false, false, "a placeable is not a sabotage target - it is clutter, and because every placeable is cheaper than every buildable, accepting one means it is demolished FIRST and the built structures never come down at all"))
			return true;

		// --- Both. Nothing shipped is authored this way; a thing that can be BUILT is a built structure.
		if (!Expect(true, true, true, "a structure carrying both components is still a built structure - letting a placeable component veto a buildable one would be a way to make a structure permanently immune"))
			return true;

		// --- Neither: the rest of a 500 m sphere query. Houses, trees, vehicles, people.
		if (!Expect(false, false, false, "an entity that is neither a buildable nor a placeable is not player property and is never a target"))
			return true;

		Print("Objective sabotage: only BUILT structures are candidates - placeables are excluded, so the cheapest-first ladder starts at the 750 bunker instead of at a 20-resource sandbag");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one candidate row, naming it in the failure message.
	//! \param[in] hasPlaceable Whether the entity carries OVT_PlaceableComponent.
	//! \param[in] hasBuildable Whether the entity carries OVT_BuildableComponent.
	//! \param[in] expected Whether it must be considered as a target.
	//! \param[in] label Human description of the row, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool Expect(bool hasPlaceable, bool hasBuildable, bool expected, string label)
	{
		bool actual = OVT_BaseSabotageBehaviorDeploymentModule.IsCandidateStructure(hasPlaceable, hasBuildable);

		if (actual == expected)
			return true;

		SetFailure("%1: got %2, expected %3", label, actual.ToString(), expected.ToString());

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! The prefab-to-price join is unambiguous: every buildable authors a positive cost, no prefab
//! appears in two entries across both configs, and UNKNOWN_STRUCTURE_COST sorts after every price.
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
//! A CONSTRUCTION SITE IS NOT A SABOTAGE TARGET.
//!
//! A site carries OVT_BuildableComponent so the build pipeline and the Buildable persistence config
//! can claim it, which also made it a sabotage candidate: reported from a live server, a saboteur team
//! demolished a site the player had already paid for in cash and in hauled resources, leaving a ruin
//! that can never be finished.
//!
//! The case asserts BOTH halves, because the first is what makes the second necessary: the site does
//! reach the candidate list, and IsConstructionSite is what takes it back out.
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveSabotage_KConstructionSitesAreNotTargets : SCR_AutotestCaseBase
{
	static const ResourceName SITE = "{E91657A942F4C8DC}Prefabs/Sites/Site_Barracks.et";

	protected IEntity m_Site;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (OVT_BaseSabotageBehaviorDeploymentModule.IsConstructionSite(null))
		{
			SetFailure("A null candidate was called a construction site");
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.IsEmpty())
		{
			SetFailure("No town is registered, so there is nowhere sensible to put a test site");
			return true;
		}

		m_Site = OVT_Global.SpawnEntityPrefab(SITE, towns.m_Towns[0].location + "780 0 660");
		if (!m_Site)
		{
			SetFailure("SpawnEntityPrefab() produced no entity from %1", SITE);
			return true;
		}

		// The half that makes the guard necessary rather than decorative.
		if (!m_Site.FindComponent(OVT_BuildableComponent))
		{
			SetFailure("A construction site no longer carries OVT_BuildableComponent, so it can no longer reach the sabotage candidate list and this case is asserting nothing");
			return FinishAndCleanUp();
		}

		if (!OVT_BaseSabotageBehaviorDeploymentModule.IsConstructionSite(m_Site))
		{
			SetFailure("A construction site was offered as a sabotage target. The mission would demolish a build the player has already paid for in cash and in hauled resources, and leave a ruin that can never be finished.");
			return FinishAndCleanUp();
		}

		Print("Objective sabotage: a construction site is a candidate by component and is refused by name");
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		if (m_Site)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_Site);
			m_Site = null;
		}

		return true;
	}
}
