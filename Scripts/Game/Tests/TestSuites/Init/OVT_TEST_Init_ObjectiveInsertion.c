//------------------------------------------------------------------------------------------------
//! TIER B - LIVE INSERTION where it meets live components: the two registry entries it depends on,
//! the origin seam, the concurrency cap, and the clone that carries all of it.
//!
//! WHAT THIS TIER CAN SEE THAT THE CHEAP ONE CANNOT. The geometry that routes a force between "by
//! truck" and "on foot" is a pure function of numbers and is pinned in
//! OVT_TEST_Logic_ObjectiveInsertion.c. Four things about the insertion module are NOT arithmetic and
//! can only be asserted against loaded configs and real components:
//!
//!   1. THAT THE TWO NEW REGISTRY NAMES RESOLVE, FOR BOTH FACTIONS. `truck_crew` and `specops_team`
//!      are strings in a .conf, matched by string at runtime. A typo in either does not fail to
//!      parse, does not warn at load and does not appear anywhere until a live insertion refuses to
//!      register a crew somewhere in the middle of a campaign - and then only in a log line nobody is
//!      reading. Five configs across three later phases are going to name these two strings, so the
//!      cheapest possible time to catch a typo is now.
//!   2. THAT THE ORIGIN SEAM REFUSES RATHER THAN ANSWERING THE WORLD ORIGIN. "There is nowhere for
//!      this force to come from" and "the force comes from 0 0 0" are the same value and completely
//!      different facts. Getting it wrong drives a truck to the south-west corner of the map.
//!   3. THAT THE CONVOY CAP COUNTS. It is a per-faction integer with no other reader, so nothing else
//!      in the tree would notice if reserve, release or the refusal stopped working - and a release
//!      that stopped working is permanent: the faction never drives again for the rest of the
//!      campaign.
//!   4. THAT THE CLONE CARRIES EVERY ATTRIBUTE. CloneModule copies by hand and is not chained.
//!
//! ⚠ NOTHING HERE CONSTRUCTS A DEPLOYMENT, AND THAT IS DELIBERATE - see the fixture note below.
//! Every subject is either a bare `new` object with no parent deployment, a loaded faction config, or
//! one integer on the deployment manager that this file sets and puts back.
//!
//! ================== THE FIXTURE HAZARD THIS FILE DOES NOT GO NEAR ==========================
//!
//! OVT_DeploymentComponent.InitializeDeployment() arms a repeating 8-12 s UpdateDeployment, and the
//! FIRST of those ticks converges every spawning module. For this module in particular that would
//! resolve an origin, claim a convoy slot, put a real truck on a real road and register real groups
//! at a 100 000 m ring - with the autotest camera permanently inside it. A case that built one would
//! be spawning live AI into the shared initialisation world for every case that runs after it.
//!
//! The rule, if a later phase ever does need one: SetSpawnedUnitsEliminated(true) on the DEPLOYMENT
//! and on EVERY spawning module, in the same frame the deployment is created, before anything can
//! tick. That is what the base-defense seeding case and the persistence deployment fixtures do. This
//! file needs no deployment to make any of its four claims, so it builds none.
//!
//! ⚠ CASE ORDER MATTERS AND THE NAMES ARE CHOSEN FOR IT. Cases run alphabetically by class name. The
//! cap case (C...) writes to the deployment manager and restores; the registry (R...) and provider
//! (S...) cases only read; the clone case (C...lone) touches nothing outside itself.
//!
//! No polling, no waiting, no maxAttempts: nothing here has a clock, and every subject is either a
//! hand-built object or a single synchronous call.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! The per-faction convoy cap counts up on reserve, down on release, and refuses past the ceiling.
//!
//! ⚠ THE RELEASE HALF IS THE HALF THAT MATTERS, AND IT IS THE ONE NOTHING ELSE WOULD CATCH. The
//! counter is runtime-only: it is not persisted, nothing sweeps it, and no other system reads it. A
//! reservation that is claimed and never handed back is therefore not "stale state" - it is a slot
//! that has left the world. Enough of them and TryReserveInsertion() answers false forever, every
//! insertion in the campaign quietly walks, and the most visible mechanic the feature has simply
//! stops happening with nothing in the log to say so.
//!
//! ⚠ THE FIXTURE FACTION INDEX IS ONE NO FACTION HOLDS, deliberately, and the case puts the cap back
//! where it found it. Nothing in the tree authors an insertion module yet, so there is no live
//! consumer to disturb - but the initialisation world is shared with every case that follows and a
//! test that leaves a counter behind would be doing the exact thing this case exists to forbid.
//!
//! THE ZERO CAP IS A ROW BECAUSE IT IS A LEGITIMATE AUTHORED VALUE - an operator's "never put a live
//! convoy on my server" - and it must REFUSE rather than divide by nothing or allow one through.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; every one exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean):
//!   E1. `if (held >= m_iMaxConcurrentInsertions) return false;` deleted from TryReserveInsertion.
//!       Fails on "a third reservation past a cap of two must be refused".
//!   E2. ReleaseInsertion() made a no-op. Fails on "releasing one of two convoys must free a slot".
//!   E3. `if (m_iMaxConcurrentInsertions <= 0) return false;` deleted. Fails on "a cap of zero must
//!       refuse every convoy, so a server can turn live insertion off".
//!   E4. ReleaseInsertion() allowed to go negative (the `held <= 1` branch replaced by a plain
//!       decrement). Fails on "releasing more often than reserving must never lend the faction extra
//!       slots".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveInsertion_CapReservesReleasesAndRefuses : SCR_AutotestCaseBase
{
	//! A faction index no faction holds, so nothing that reads the store by a real index can see this
	//! case's bookkeeping.
	static const int FIXTURE_FACTION = 4242;

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

		int originalCap = deployments.GetMaxConcurrentInsertions();

		string failure = RunCapChecks(deployments);

		// --- RESTORE FIRST, ASSERT SECOND, so a red case leaves nothing behind.
		deployments.ReleaseInsertion(FIXTURE_FACTION);
		deployments.ReleaseInsertion(FIXTURE_FACTION);
		deployments.ReleaseInsertion(FIXTURE_FACTION);
		deployments.SetMaxConcurrentInsertions(originalCap);

		if (deployments.GetInsertionReservations(FIXTURE_FACTION) != 0)
		{
			SetFailure("the fixture faction still holds %1 convoy slot(s) after the teardown",
				deployments.GetInsertionReservations(FIXTURE_FACTION).ToString());
			return true;
		}

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Insertion convoy cap: reserve counts up, release counts down, the ceiling refuses, a cap of zero refuses everything and an over-release never lends a slot");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] deployments The deployment manager.
	//! \return An empty string when every claim held, or the first that did not.
	protected string RunCapChecks(notnull OVT_DeploymentManagerComponent deployments)
	{
		// --- A faction nobody has reserved for holds nothing.
		if (deployments.GetInsertionReservations(FIXTURE_FACTION) != 0)
			return "a faction that has never driven anywhere already holds a convoy slot";

		deployments.SetMaxConcurrentInsertions(2);

		// --- Up.
		if (!deployments.TryReserveInsertion(FIXTURE_FACTION))
			return "the first convoy under a cap of two was refused";

		if (deployments.GetInsertionReservations(FIXTURE_FACTION) != 1)
			return string.Format("after one reservation the faction holds %1 convoy slot(s), expected 1",
				deployments.GetInsertionReservations(FIXTURE_FACTION).ToString());

		if (!deployments.TryReserveInsertion(FIXTURE_FACTION))
			return "the second convoy under a cap of two was refused";

		if (deployments.GetInsertionReservations(FIXTURE_FACTION) != 2)
			return string.Format("after two reservations the faction holds %1 convoy slot(s), expected 2",
				deployments.GetInsertionReservations(FIXTURE_FACTION).ToString());

		// --- The ceiling.
		if (deployments.TryReserveInsertion(FIXTURE_FACTION))
			return "a third reservation past a cap of two must be refused";

		if (deployments.GetInsertionReservations(FIXTURE_FACTION) != 2)
			return string.Format("a refused reservation moved the count to %1, expected it to stay at 2",
				deployments.GetInsertionReservations(FIXTURE_FACTION).ToString());

		// --- Down, and back up again: a released slot is genuinely reusable, not merely uncounted.
		deployments.ReleaseInsertion(FIXTURE_FACTION);

		if (deployments.GetInsertionReservations(FIXTURE_FACTION) != 1)
			return string.Format("releasing one of two convoys must free a slot: the faction holds %1, expected 1",
				deployments.GetInsertionReservations(FIXTURE_FACTION).ToString());

		if (!deployments.TryReserveInsertion(FIXTURE_FACTION))
			return "a slot freed by a release could not be claimed again - a released reservation must be genuinely reusable";

		deployments.ReleaseInsertion(FIXTURE_FACTION);
		deployments.ReleaseInsertion(FIXTURE_FACTION);

		if (deployments.GetInsertionReservations(FIXTURE_FACTION) != 0)
			return string.Format("releasing every convoy must leave nothing behind: the faction holds %1, expected 0",
				deployments.GetInsertionReservations(FIXTURE_FACTION).ToString());

		// --- Over-release. A counter that went negative would silently RAISE the cap.
		deployments.ReleaseInsertion(FIXTURE_FACTION);
		deployments.ReleaseInsertion(FIXTURE_FACTION);

		if (deployments.GetInsertionReservations(FIXTURE_FACTION) != 0)
			return string.Format("releasing more often than reserving must never lend the faction extra slots: the count is %1, expected 0",
				deployments.GetInsertionReservations(FIXTURE_FACTION).ToString());

		deployments.SetMaxConcurrentInsertions(1);

		if (!deployments.TryReserveInsertion(FIXTURE_FACTION))
			return "after an over-release the faction could not claim its first slot under a cap of one - the counter went below zero and then had to climb back";

		if (deployments.TryReserveInsertion(FIXTURE_FACTION))
			return "after an over-release a cap of one allowed a second convoy - the counter went below zero";

		deployments.ReleaseInsertion(FIXTURE_FACTION);

		// --- The operator's off-switch.
		deployments.SetMaxConcurrentInsertions(0);

		if (deployments.TryReserveInsertion(FIXTURE_FACTION))
			return "a cap of zero must refuse every convoy, so a server can turn live insertion off";

		// --- And a negative cap is clamped rather than treated as unbounded.
		deployments.SetMaxConcurrentInsertions(-5);

		if (deployments.GetMaxConcurrentInsertions() != 0)
			return string.Format("a negative cap must clamp to zero, not be stored: the cap reads %1",
				deployments.GetMaxConcurrentInsertions().ToString());

		if (deployments.TryReserveInsertion(FIXTURE_FACTION))
			return "a negative cap must refuse every convoy";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! THE STANDING CloneModule TRAP, asserted on the module this phase ships - all thirteen inherited
//! attributes and all ten of its own.
//!
//! WHY THIS CASE EXISTS AT ALL. Every deployment gets a CLONE of its config's modules
//! (OVT_DeploymentComponent.InitializeDeployment), and CloneModule copies attribute by attribute BY
//! HAND and is NOT CHAINED - a subclass builds a fresh instance and repeats its parent's whole copy
//! list. A forgotten line does not warn, does not log and does not fail to parse: it ships the CLASS
//! DEFAULT instead of the authored value, on every deployment, forever. That is how
//! m_fMaxCruiseSpeed was lost on the vehicle module for a whole release.
//!
//! WHAT A DROPPED LINE WOULD COST ON THIS MODULE SPECIFICALLY:
//!   - m_Source              -> the module registers NOTHING AT ALL, in silence. It refuses to put a
//!                              force in the world without an origin, and a null provider is no
//!                              origin, so the deployment is bought, paid for and permanently empty.
//!   - m_sTruckVehicleType   -> no prefab resolves and every insertion in the campaign walks. The men
//!                              still arrive, which is precisely why nobody would ever notice.
//!   - m_sTruckCrewGroup     -> same, one step later: a truck on the road with nobody in it.
//!   - m_fWalkThresholdDistance -> 0 disables the short-hop rule, so a force whose objective is 80 m
//!                              away boards a truck to drive there.
//!   - m_fLZStandoffDistance -> 0 drives the convoy all the way ONTO the objective it was supposed to
//!                              stop short of, which is the difference between an insertion and an
//!                              assault.
//!   - m_fArrivalRadius      -> 0 means the convoy only arrives by stopping on the exact metre, so it
//!                              drives past its landing zone and keeps going.
//!   - m_iStuckTicks         -> 0 disables the stall test, so a truck wedged against a wall holds its
//!                              force inside it until the deployment is torn down.
//!   - m_iTruckCostOverride  -> the deployment is under-priced and the pool no longer balances.
//!   - m_bWalkWhenInsertionRefused -> false makes a refused slot wait instead of walk, so a saturated
//!                              faction stops inserting at all rather than walking.
//!
//! EVERY PROBE VALUE IS NON-ZERO, NON-EMPTY AND NON-FALSE, WHICH IS THE POINT. A `new` instance
//! starts at 0 / "" / false / enum 0, so a probe value of `false` or `0` would be indistinguishable
//! from a dropped copy and the assertion would pass while the bug shipped.
//!
//! ⚠ [Attribute] DEFVALUES DO NOT APPLY TO `new` - which is what makes the above true, and is worth
//! stating because it is counter-intuitive and bites every fixture in this framework.
//!
//! NOTHING IS REGISTERED, CREATED OR MUTATED: one bare `new` module with no parent deployment.
//! CloneModule is pure field copying.
//!
//! PROVEN ABLE TO FAIL: delete any single `clone.X = X;` line from
//! OVT_InsertionSpawningDeploymentModule.CloneModule() and this case goes red naming that exact
//! field. Faults were injected on m_Source, m_sTruckVehicleType and m_eImportance (one of the
//! inherited thirteen, to prove the inherited half is really being checked); all three exited
//! tools/compile-check.sh 0 and the subject was restored and re-compiled clean.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveInsertion_CloneCarriesEveryAttribute : SCR_AutotestCaseBase
{
	//! Distinctive probe values. None may be 0, "" or false - see the class header.
	static const string PROBE_NAME = "OVT_TEST insertion probe";
	static const string PROBE_GROUP = "OVT_TEST group type";
	static const string PROBE_TRUCK = "OVT_TEST truck type";
	static const string PROBE_CREW = "OVT_TEST crew type";
	static const int PROBE_MIN_GROUPS = 2;
	static const int PROBE_MAX_GROUPS = 6;
	static const float PROBE_SPAWN_RADIUS = 87.5;
	static const int PROBE_COST = 43;
	static const int PROBE_REINFORCE_COST = 19;
	static const float PROBE_WALK_THRESHOLD = 512.25;
	static const float PROBE_STANDOFF = 275.75;
	static const float PROBE_STUCK_SPEED = 2.5;
	static const int PROBE_STUCK_TICKS = 7;
	static const float PROBE_ARRIVAL_RADIUS = 33.5;
	static const int PROBE_TRUCK_COST = 61;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_InsertionSpawningDeploymentModule module = new OVT_InsertionSpawningDeploymentModule();

		// Inherited from OVT_InfantrySpawningDeploymentModule - all thirteen.
		module.m_sModuleName = PROBE_NAME;
		module.m_sGroupType = PROBE_GROUP;
		module.m_iMinGroupCount = PROBE_MIN_GROUPS;
		module.m_iMaxGroupCount = PROBE_MAX_GROUPS;
		module.m_bScaleByTownSize = true;
		module.m_fSpawnRadius = PROBE_SPAWN_RADIUS;
		module.m_iCostPerGroup = PROBE_COST;
		module.m_bAllowReinforcement = true;
		module.m_iReinforcementCost = PROBE_REINFORCE_COST;
		module.m_bSpawnAtNearestBase = true;
		module.m_bReinforceFromNearestBase = true;
		module.m_eImportance = SCR_EAISpawnImportance.CRITICAL;
		module.m_bSnapToRoad = true;

		// This module's own ten.
		module.m_Source = new OVT_NearestControlledBaseSourceProvider();
		module.m_fWalkThresholdDistance = PROBE_WALK_THRESHOLD;
		module.m_sTruckVehicleType = PROBE_TRUCK;
		module.m_sTruckCrewGroup = PROBE_CREW;
		module.m_fLZStandoffDistance = PROBE_STANDOFF;
		module.m_fStuckSpeedThreshold = PROBE_STUCK_SPEED;
		module.m_iStuckTicks = PROBE_STUCK_TICKS;
		module.m_fArrivalRadius = PROBE_ARRIVAL_RADIUS;
		module.m_iTruckCostOverride = PROBE_TRUCK_COST;
		module.m_bWalkWhenInsertionRefused = true;

		OVT_InsertionSpawningDeploymentModule clone = OVT_InsertionSpawningDeploymentModule.Cast(module.CloneModule());
		if (!clone)
		{
			SetFailure("OVT_InsertionSpawningDeploymentModule.CloneModule() did not answer an insertion module at all");
			return true;
		}

		string failure = CompareInherited(module, clone);
		if (failure == "")
			failure = CompareOwn(module, clone);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Every one of the insertion module's own ten attributes and all thirteen it inherits survive CloneModule");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The thirteen attributes OVT_InfantrySpawningDeploymentModule declares.
	//! \param[in] module The authored module.
	//! \param[in] clone Its clone.
	//! \return An empty string when all thirteen survived, or the first that did not.
	protected string CompareInherited(notnull OVT_InsertionSpawningDeploymentModule module,
		notnull OVT_InsertionSpawningDeploymentModule clone)
	{
		if (clone.m_sModuleName != module.m_sModuleName)
			return "the insertion module's clone lost m_sModuleName - its OWNER KEY is derived from it, so both the force and the transport crew would be registered under different keys and never reclaimed";

		if (clone.m_sGroupType != module.m_sGroupType)
			return "the insertion module's clone lost m_sGroupType - the core resolves (factionKey, groupName) and would refuse every registration";

		if (clone.m_iMinGroupCount != module.m_iMinGroupCount)
			return "the insertion module's clone lost m_iMinGroupCount";

		if (clone.m_iMaxGroupCount != module.m_iMaxGroupCount)
			return "the insertion module's clone lost m_iMaxGroupCount - the force size would be the class default on every deployment";

		if (clone.m_bScaleByTownSize != module.m_bScaleByTownSize)
			return "the insertion module's clone lost m_bScaleByTownSize";

		if (clone.m_fSpawnRadius != module.m_fSpawnRadius)
			return "the insertion module's clone lost m_fSpawnRadius";

		if (clone.m_iCostPerGroup != module.m_iCostPerGroup)
			return "the insertion module's clone lost m_iCostPerGroup - the deployment's resource cost would be wrong";

		if (clone.m_bAllowReinforcement != module.m_bAllowReinforcement)
			return "the insertion module's clone lost m_bAllowReinforcement - a wiped force would never be rebought";

		if (clone.m_iReinforcementCost != module.m_iReinforcementCost)
			return "the insertion module's clone lost m_iReinforcementCost";

		if (clone.m_bSpawnAtNearestBase != module.m_bSpawnAtNearestBase)
			return "the insertion module's clone lost m_bSpawnAtNearestBase";

		if (clone.m_bReinforceFromNearestBase != module.m_bReinforceFromNearestBase)
			return "the insertion module's clone lost m_bReinforceFromNearestBase";

		if (clone.m_eImportance != module.m_eImportance)
			return "the insertion module's clone lost m_eImportance - every group would register at the class default tier and lose the AI spawn-budget race";

		if (clone.m_bSnapToRoad != module.m_bSnapToRoad)
			return "the insertion module's clone lost m_bSnapToRoad";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The ten attributes this module declares.
	//! \param[in] module The authored module.
	//! \param[in] clone Its clone.
	//! \return An empty string when all ten survived, or the first that did not.
	protected string CompareOwn(notnull OVT_InsertionSpawningDeploymentModule module,
		notnull OVT_InsertionSpawningDeploymentModule clone)
	{
		if (!clone.m_Source)
			return "the insertion module's clone carries NO source provider - it would refuse to put a force in the world at all, register nothing, and cost the faction the deployment's whole price for an empty marker";

		if (clone.m_fWalkThresholdDistance != module.m_fWalkThresholdDistance)
			return "the insertion module's clone lost m_fWalkThresholdDistance - a force whose objective is eighty metres away would board a truck to drive there";

		if (clone.m_sTruckVehicleType != module.m_sTruckVehicleType)
			return "the insertion module's clone lost m_sTruckVehicleType - no prefab would resolve and every insertion in the campaign would walk, invisibly";

		if (clone.m_sTruckCrewGroup != module.m_sTruckCrewGroup)
			return "the insertion module's clone lost m_sTruckCrewGroup - a truck would be spawned with nobody in it";

		if (clone.m_fLZStandoffDistance != module.m_fLZStandoffDistance)
			return "the insertion module's clone lost m_fLZStandoffDistance - the convoy would drive all the way ONTO the objective it was meant to stop short of";

		if (clone.m_fStuckSpeedThreshold != module.m_fStuckSpeedThreshold)
			return "the insertion module's clone lost m_fStuckSpeedThreshold";

		if (clone.m_iStuckTicks != module.m_iStuckTicks)
			return "the insertion module's clone lost m_iStuckTicks - a zero disables the stall test, so a wedged truck holds its force inside it until the deployment is torn down";

		if (clone.m_fArrivalRadius != module.m_fArrivalRadius)
			return "the insertion module's clone lost m_fArrivalRadius - the convoy would only arrive by stopping on the exact metre and would drive past its landing zone";

		if (clone.m_iTruckCostOverride != module.m_iTruckCostOverride)
			return "the insertion module's clone lost m_iTruckCostOverride - the deployment would be under-priced and the resource pool would stop balancing";

		if (clone.m_bWalkWhenInsertionRefused != module.m_bWalkWhenInsertionRefused)
			return "the insertion module's clone lost m_bWalkWhenInsertionRefused - a faction that had spent its convoy slots would wait instead of walking, and stop inserting at all";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! `truck_crew` and `specops_team` resolve to real, loadable prefabs for BOTH shipped factions.
//!
//! ⚠ THE PREFAB IS ACTUALLY LOADED, NOT JUST LOOKED UP. Two different things can be wrong with a
//! registry entry and only one of them is caught by a string comparison: the NAME can be misspelled
//! (the lookup answers empty) or the GUID can be wrong or point at something that no longer ships
//! (the lookup answers a perfectly good-looking string that resolves to nothing). The second is the
//! more likely of the two here, because both entries were added by hand-editing a .conf with GUIDs
//! recovered from git history and from the vanilla tree. Resource.Load() is what tells the two apart.
//!
//! ⚠ BOTH FACTIONS, BECAUSE A CAMPAIGN CAN BE EITHER WAY ROUND. Overthrow's occupying faction is
//! authored per scenario; a registry entry added to USSR and forgotten on US produces a feature that
//! works perfectly for half the players and not at all for the other half, and nothing in a
//! single-faction test world would ever show it.
//!
//! THE EXISTING ENTRIES ARE SPOT-CHECKED TOO, so a failure can be read as "this entry is wrong"
//! rather than "the registry is not loaded in this world" - which are very different bugs and would
//! otherwise produce the same red.
//!
//! PROVEN ABLE TO FAIL: `m_sGroupName "truck_crew"` was misspelled to "truck_crews" in
//! Configs/Factions/US_OverthrowData.conf. The tree compiled clean (exit 0 - a config string is not
//! script), and the case then reports "faction 'US' has no group registry entry named 'truck_crew'".
//! The entry was restored.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveInsertion_RegistryNamesResolveForBothFactions : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OverthrowFactionManager factions = OVT_Global.GetFactions();
		if (!factions)
		{
			SetFailure("OVT_Global.GetFactions() is null - the faction manager did not resolve");
			return true;
		}

		string failure = VerifyFaction(factions, "USSR");
		if (failure == "")
			failure = VerifyFaction(factions, "US");

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("Both shipped factions carry loadable 'truck_crew' and 'specops_team' group registry entries");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] factions The faction manager.
	//! \param[in] factionKey The faction to check.
	//! \return An empty string when both new entries resolve, or the first failure.
	protected string VerifyFaction(notnull OVT_OverthrowFactionManager factions, string factionKey)
	{
		OVT_Faction faction = factions.GetOverthrowFactionByKey(factionKey);
		if (!faction)
			return string.Format("there is no Overthrow faction config for '%1'", factionKey);

		faction.InitializeGroupRegistry();

		// The spot-check: an entry that has shipped for a year. If THIS one fails, the registry is not
		// loaded and the two new entries are innocent.
		string failure = VerifyEntry(faction, factionKey, "light_patrol");
		if (failure != "")
			return failure + " - and that entry has shipped for a year, so the registry itself is not loaded in this world";

		failure = VerifyEntry(faction, factionKey, "truck_crew");
		if (failure != "")
			return failure;

		return VerifyEntry(faction, factionKey, "specops_team");
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] faction The faction config.
	//! \param[in] factionKey Its key, for the message.
	//! \param[in] groupName The registry name to resolve.
	//! \return An empty string when the name resolves to a loadable prefab, or the failure.
	protected string VerifyEntry(notnull OVT_Faction faction, string factionKey, string groupName)
	{
		if (!faction.HasGroupType(groupName))
			return string.Format("faction '%1' has no group registry entry named '%2'", factionKey, groupName);

		ResourceName prefab = faction.GetGroupPrefabByName(groupName);
		if (prefab.IsEmpty())
			return string.Format("faction '%1's registry entry '%2' names no prefab at all", factionKey, groupName);

		Resource resource = Resource.Load(prefab);
		if (!resource)
			return string.Format("faction '%1's registry entry '%2' names a prefab that will not load: %3",
				factionKey, groupName, prefab);

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! The origin seam REFUSES when there is nowhere to come from, and never answers the world origin.
//!
//! ⚠ THE TWO ANSWERS ARE THE SAME VALUE AND COMPLETELY DIFFERENT FACTS. `vector.Zero` is both "the
//! south-west corner of the map" and "I do not know", and a provider that conflated them would send a
//! truck driving to the corner of Eden with a squad in the back - or, worse, would let the module
//! register a force there, which is the one thing the whole module exists to prevent. Hence the
//! contract: return FALSE, never a zero vector.
//!
//! THE POSITIVE HALF IS ASSERTED FIRST so the refusal cannot pass vacuously. A provider that always
//! answered false would satisfy every "must refuse" row perfectly, and would also be a provider that
//! silently disables live insertion for the entire campaign.
//!
//! ⚠ THE REFUSING FACTION INDEX IS ONE NO FACTION HOLDS. "A position with no friendly base" cannot be
//! expressed as a position, because the provider searches the whole faction's base list and not a
//! radius - so the honest fixture is a faction that controls nothing, which is exactly the state a
//! faction reaches when the resistance has taken its last base. Nothing is mutated: no base changes
//! hands, no faction is touched.
//!
//! THE MAXIMUM SOURCE DISTANCE is exercised as its own refusal, because it is the other way a
//! provider legitimately answers "not from here" and it must refuse in the same shape.
//!
//! ⚠ THE BASE CLASS PROVIDER IS CHECKED TOO. OVT_DeploymentSourceProvider's own ResolveSource() is the
//! contract's reference implementation and the thing a modder subclasses; if IT ever started answering
//! true with a zero vector, every provider that forgot to override would inherit the bug.
//!
//! PROVEN ABLE TO FAIL (faults injected one at a time and compiled; both exited compile-check 0):
//!   F1. `return found;` replaced with `return true;` in OVT_NearestControlledBaseSourceProvider.
//!       Fails on "a faction that controls no base must be refused, not answered the world origin".
//!   F2. The nearest-base comparison inverted (`distance <= bestDistance` -> `distance >= bestDistance`).
//!       Fails on "the provider must answer the NEAREST controlled base" when the world holds more
//!       than one; on a single-base world it is a documented no-op, which is why the row states its
//!       own precondition rather than assuming one.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveInsertion_SourceProviderRefusesRatherThanAnsweringZero : SCR_AutotestCaseBase
{
	//! A faction index no faction holds, so it controls nothing and can never control anything.
	static const int FIXTURE_FACTION = 4242;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!occupying || !config)
		{
			SetFailure("The occupying faction manager or the campaign config did not resolve");
			return true;
		}

		OVT_NearestControlledBaseSourceProvider provider = new OVT_NearestControlledBaseSourceProvider();
		provider.m_fMaxSourceDistance = 0;

		int occupyingIndex = config.GetOccupyingFactionIndex();

		array<OVT_BaseData> held = occupying.GetBasesControlledBy(occupyingIndex);
		if (!held || held.IsEmpty())
		{
			SetFailure("The occupying faction controls no base in this world, so the provider's POSITIVE answer cannot be exercised and its refusal would pass vacuously");
			return true;
		}

		// --- THE POSITIVE HALF. Asked from a real base's own position, the provider answers it.
		vector probe = held[0].location;

		vector answered;
		if (!provider.ResolveSource(probe, occupyingIndex, answered))
		{
			SetFailure("the provider refused an origin for a faction that controls %1 base(s)", held.Count().ToString());
			return true;
		}

		if (answered == vector.Zero)
		{
			SetFailure("the provider answered TRUE with the world origin - the two must never be confused");
			return true;
		}

		if (vector.Distance(answered, probe) > NearestHeldDistance(held, probe) + 1.0)
		{
			SetFailure("the provider must answer the NEAREST controlled base: it answered one %1 m away, the nearest is %2 m away",
				vector.Distance(answered, probe).ToString(), NearestHeldDistance(held, probe).ToString());
			return true;
		}

		// --- THE REFUSAL. A faction that controls nothing.
		vector refused = Vector(1, 2, 3);
		if (provider.ResolveSource(probe, FIXTURE_FACTION, refused))
		{
			SetFailure("a faction that controls no base must be refused, not answered the world origin");
			return true;
		}

		if (refused != vector.Zero)
		{
			SetFailure("a refused origin must leave the out parameter at the zero vector rather than at whatever the caller had: it reads %1", refused.ToString());
			return true;
		}

		// --- THE OTHER REFUSAL: everything is out of range.
		provider.m_fMaxSourceDistance = 1;

		vector outOfRange = Vector(4, 5, 6);
		vector faraway = probe + Vector(50000, 0, 50000);
		if (provider.ResolveSource(faraway, occupyingIndex, outOfRange))
		{
			SetFailure("a maximum source distance of one metre must refuse a base fifty kilometres away");
			return true;
		}

		if (outOfRange != vector.Zero)
		{
			SetFailure("an out-of-range refusal must leave the out parameter at the zero vector: it reads %1", outOfRange.ToString());
			return true;
		}

		// --- THE CONTRACT'S REFERENCE IMPLEMENTATION, which every modder subclasses.
		OVT_DeploymentSourceProvider none = new OVT_DeploymentSourceProvider();

		vector baseAnswer = Vector(7, 8, 9);
		if (none.ResolveSource(probe, occupyingIndex, baseAnswer))
		{
			SetFailure("the base source provider must answer false - a subclass that forgets to override would otherwise inherit an origin it never chose");
			return true;
		}

		if (baseAnswer != vector.Zero)
		{
			SetFailure("the base source provider must zero its out parameter as well as answering false: it reads %1", baseAnswer.ToString());
			return true;
		}

		Print("The insertion origin seam answers the nearest controlled base, and refuses with a zero vector rather than pretending the world origin is a place a force came from");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] held Every base the faction controls.
	//! \param[in] probe Where the question was asked from.
	//! \return The distance to the nearest of them.
	protected float NearestHeldDistance(notnull array<OVT_BaseData> held, vector probe)
	{
		float best = -1;

		foreach (OVT_BaseData base : held)
		{
			if (!base)
				continue;

			float distance = vector.Distance(base.location, probe);
			if (best < 0 || distance < best)
				best = distance;
		}

		return best;
	}
}
