//------------------------------------------------------------------------------------------------
//! TIER B - LIVE INSERTION where it meets live components: the two registry entries it depends on,
//! the origin seam, the concurrency cap, the observer that keeps the transport simulated, and the
//! clone that carries all of it.
//!
//! WHAT THIS TIER CAN SEE THAT THE CHEAP ONE CANNOT. The geometry that routes a force between "by
//! truck" and "on foot" is a pure function of numbers and is pinned in
//! OVT_TEST_Logic_ObjectiveInsertion.c. Five things about the insertion module are NOT arithmetic and
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
//!   5. THAT THE TRANSPORT'S OWN OBSERVER IS PARKED AND, ABOVE ALL, TAKEN OFF AGAIN. The engine does
//!      not simulate distant entities, so without it a convoy far from every player never moves; and
//!      a leaked observer holds everything registered around it materialised for the rest of the
//!      session. Both halves are core bookkeeping and neither is arithmetic.
//!
//! ⚠ NOTHING HERE CONSTRUCTS A DEPLOYMENT, AND THAT IS DELIBERATE - see the fixture note below.
//! Every subject is a bare `new` object with no parent deployment, a loaded faction config, one
//! integer on the deployment manager that this file sets and puts back, or - in the observer case
//! alone - one throwaway transport vehicle, spawned near the fixture town and deleted before that
//! case reports.
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
//! (S...) cases only read; the clone case (C...lone) and the observer case (O...) touch nothing that
//! outlives them.
//!
//! No polling and no maxAttempts. The observer case is the only one with a clock at all, and it is
//! not a retry budget: it waits a bounded number of frames for its transport to become world-
//! registered, asserts once, and waits a fixed number more before deleting it.
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
//! attributes and all eleven of its own.
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
//!   - m_bTransportIsObserver -> false leaves the transport UNSIMULATED once it is about a kilometre
//!                              from every observer, so every convoy in the campaign stands at 0 m/s
//!                              with a perfectly healthy crew and falls back to the march.
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

		// This module's own eleven.
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
		module.m_bTransportIsObserver = true;

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

		Print("Every one of the insertion module's own eleven attributes and all thirteen it inherits survive CloneModule");

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
	//! The eleven attributes this module declares.
	//! \param[in] module The authored module.
	//! \param[in] clone Its clone.
	//! \return An empty string when all eleven survived, or the first that did not.
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

		if (clone.m_bTransportIsObserver != module.m_bTransportIsObserver)
			return "the insertion module's clone lost m_bTransportIsObserver - it clones as FALSE, so no engine observer is parked on the transport, the ENGINE STOPS SIMULATING THE HULL once the convoy is about a kilometre from every observer, and it stands at 0 m/s with a perfectly healthy crew until the stall test walks the force in";

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

//------------------------------------------------------------------------------------------------
//! Exposes the protected seams the observer case has to drive.
//!
//! A SUBCLASS RATHER THAN A WIDENED PRODUCTION API - the same answer OVT_TEST_MountedForceProbe
//! gives, and for the same reason: the convoy state machine is driven by the module's own update and
//! by nothing else, so turning any of this public "so a test can get at it" would be the widening
//! this repair is under instruction not to do.
//------------------------------------------------------------------------------------------------
class OVT_TEST_InsertionObserverProbe : OVT_InsertionSpawningDeploymentModule
{
	//------------------------------------------------------------------------------------------------
	//! Plants the transport, exactly as a successful SpawnTruck() would have left it.
	//! \param[in] truck The fixture vehicle.
	void ProbeSetTruck(Vehicle truck)
	{
		m_Truck = truck;
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the third stamp - the one EnsureConvoy() runs the moment there is a transport.
	void ProbeHoldTruckSimulated()
	{
		HoldTruckSimulated();
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the module's single audited teardown, LEAVING THE TRANSPORT STANDING - see the case
	//! header for why an assertion made against a deleted one would prove nothing.
	//! \param[in] reason What ended the convoy, for the log line.
	void ProbeReleaseConvoy(string reason)
	{
		ReleaseConvoy(reason, false);
	}
}

//------------------------------------------------------------------------------------------------
//! THE THIRD STAMP: an engine observer is parked on the transport while the convoy is driving, is
//! GONE after the single audited teardown, and is never parked at all with the off-switch off.
//!
//! WHAT IT IS ABOUT. The engine does not simulate distant entities, so a transport more than about a
//! kilometre from every observer stands at 0 m/s however healthy its crew is - measured 2026-08-23,
//! two of two alive, two materialised, two AI-active, worst LOD 9 of 10, nearest player 1931 m.
//! HoldTruckSimulated() parks core's entity observer on the hull (BI's own lever:
//! ObserversSystem.InsertObserverSP, "Temporary observers can keep distant entities simulated, so be
//! mindful of their lifetime") and ReleaseConvoy() takes it off.
//!
//! 🔴 THE REMOVAL IS THE HALF THAT MATTERS. A leaked observer holds every registered group inside
//! its ring materialised, with their AI running, FOR THE REST OF THE SESSION - a server-cost
//! regression strictly worse than the stall it was parked to fix, and completely invisible. Core's
//! 2 s stale sweep only covers an observer whose ENTITY has been deleted; one standing on a live
//! truck this module has walked away from is nobody's but this module's.
//!
//! ⚠ THE TRANSPORT IS DELIBERATELY LEFT STANDING BY THE TEARDOWN (ReleaseConvoy with deleteTruck
//! false, which is the real stall path). Against a DELETED truck the handle nulls itself and
//! HasEntityObserver(null) is false for free - the assertion would pass whether or not the removal
//! was ever written. The count is asserted alongside it, which is the leak check that does not
//! depend on the entity at all.
//!
//! ⚠ OBSERVER PRESENCE IS READ FROM CORE'S OWN MAP (HasEntityObserver / GetEntityObserverCount) and
//! never from the engine: engine application is deferred by one frame in BOTH directions, so an
//! engine query taken right after the add reads a false negative and one taken right after the
//! removal reads a false leak. Nothing here queries the engine, so nothing needs a settling budget.
//!
//! ⚠ ALL THREE CLAIMS RUN IN ONE SYNCHRONOUS FRAME, which is also what makes the case safe in the
//! shared initialisation world: the observer is added and removed inside a single frame, so the
//! engine never applies it and no registered group anywhere near the fixture is pulled awake.
//!
//! ⚠ NO DEPLOYMENT IS BUILT - see the file header's fixture note. Both probes are bare `new` modules
//! with no parent, and neither is ever ticked.
//!
//! WHAT IT CANNOT PROVE, stated plainly: that a real convoy drives. Everything above is core's
//! bookkeeping. That the engine then simulates the hull, that the truck covers ground with no player
//! near it, and that the observer's cost along the route is acceptable are all play-test questions.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ObjectiveInsertion_ObserverParkedWhileDrivingAndGoneAfterTeardown : SCR_AutotestCaseBase
{
	//! A real transport, because m_Truck is Vehicle-typed and core keys observers on a world-
	//! registered EntityID. The same prefab the resource seam's truck cases use.
	static const ResourceName TRUCK_PREFAB = "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et";

	//! How far from the fixture town to stand it. Small on purpose: an entity spawned outside the
	//! world's bounds may never be world-registered, and an unregistered entity has no EntityID.
	static const float TRUCK_OFFSET = 150;

	//! Frames allowed for a town to appear and for the transport to become world-registered.
	static const int SETTLE_FRAMES = 30;

	//! Frames between the last observer removal and deleting the transport, so no engine observer is
	//! ever left following a deleted entity. A FIXED DELAY - nothing is asserted while it counts.
	static const int DELETE_DELAY_FRAMES = 5;

	protected int m_iPhase;
	protected int m_iFrames;
	protected IEntity m_Truck;
	protected string m_sFailure;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == 0)
			return Arrange();

		if (m_iPhase == 1)
			return AwaitTruckId();

		return AwaitDeletion();
	}

	//------------------------------------------------------------------------------------------------
	//! Stands a transport near the fixture town. Asserts nothing about it yet - a freshly spawned
	//! entity may not be world-registered on this frame.
	//! \return True when the case is already over.
	protected bool Arrange()
	{
		if (!OVT_Global.GetVirtualization())
		{
			SetFailure("OVT_Global.GetVirtualization() is null - the entity observer API cannot be exercised in this world");
			return true;
		}

		vector anchor = OVT_TEST_VirtualizationFixture.PickPosition();
		if (anchor == vector.Zero)
		{
			m_iFrames++;
			if (m_iFrames <= SETTLE_FRAMES)
				return false;

			SetFailure("No town is registered after %1 frames, so there is nowhere to stand a fixture transport", m_iFrames.ToString());
			return true;
		}

		m_Truck = OVT_Global.SpawnEntityPrefab(TRUCK_PREFAB, anchor + Vector(TRUCK_OFFSET, 0, TRUCK_OFFSET));
		if (!m_Truck)
		{
			SetFailure("SpawnEntityPrefab() produced no transport from %1 - the prefab or its .meta GUID no longer resolves", TRUCK_PREFAB);
			return true;
		}

		m_iPhase = 1;
		m_iFrames = 0;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits until the transport is world-registered, runs every claim, then hands over to the
	//! deletion delay.
	//! \return True when the case is finished on this frame.
	protected bool AwaitTruckId()
	{
		m_iFrames++;

		if (!m_Truck)
		{
			SetFailure("The fixture transport disappeared while waiting for its EntityID to be assigned");
			return true;
		}

		if (m_Truck.GetID() == EntityID.INVALID)
		{
			if (m_iFrames < SETTLE_FRAMES)
				return false;

			CleanUp();
			SetFailure("After %1 frames the fixture transport still has no valid EntityID, so core cannot key an observer on it and nothing here can be asserted in this world", m_iFrames.ToString());
			return true;
		}

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			CleanUp();
			SetFailure("OVT_Global.GetVirtualization() went null between phases");
			return true;
		}

		m_sFailure = Verify(virtualization, Vehicle.Cast(m_Truck));

		// Belt, and it runs on the failing paths too: nothing may be left following the entity the
		// case is about to delete.
		virtualization.RemoveEntityObserver(m_Truck);

		m_iPhase = 2;
		m_iFrames = 0;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Every claim, in order, first failure wins.
	//!
	//! ⚠ THE OFF-SWITCH CLAIM IS LAST DELIBERATELY: the teardown above it has already taken the
	//! observer off, so "nothing was parked" is being asserted against a transport that is genuinely
	//! unobserved rather than one that never was.
	//! \param[in] virtualization The manager.
	//! \param[in] truck The fixture transport.
	//! \return The failure text, or "" when everything held.
	protected string Verify(notnull OVT_VirtualizationManagerComponent virtualization, Vehicle truck)
	{
		if (!truck)
			return "the fixture prefab did not spawn a Vehicle, so it cannot be planted as a transport at all: " + TRUCK_PREFAB;

		if (virtualization.HasEntityObserver(truck))
			return "the freshly spawned fixture transport already carries an entity observer - something else in this world parked one, and every count below would be measuring it";

		int before = virtualization.GetEntityObserverCount();

		// --- 1. THE CONVOY IS DRIVING: the hull is kept simulated.
		OVT_TEST_InsertionObserverProbe driving = new OVT_TEST_InsertionObserverProbe();
		driving.m_bTransportIsObserver = true;
		driving.ProbeSetTruck(truck);
		driving.ProbeHoldTruckSimulated();

		if (!virtualization.HasEntityObserver(truck))
			return "a driving transport carries NO entity observer - the engine stops simulating the hull about a kilometre from the nearest observer, so the convoy stands at 0 m/s with a perfectly healthy crew and every insertion far from a player quietly walks";

		int afterHold = virtualization.GetEntityObserverCount();
		if (afterHold != before + 1)
			return "parking the transport's observer moved core's observer count from " + before.ToString() + " to " + afterHold.ToString() + ", expected " + (before + 1).ToString();

		// --- 2. THE TEARDOWN: it comes off, and the count comes back.
		driving.ProbeReleaseConvoy("OVT_TEST insertion observer teardown");

		if (virtualization.HasEntityObserver(truck))
			return "the transport STILL carries an entity observer after ReleaseConvoy() - this file's single audited teardown has leaked one, and it will hold every registered group around that truck materialised with its AI running for the rest of the session";

		int afterRelease = virtualization.GetEntityObserverCount();
		if (afterRelease != before)
			return "core's observer count is " + afterRelease.ToString() + " after the teardown and was " + before.ToString() + " before the convoy - the removal did not give the key back, which is a leak nothing else in the tree would ever notice";

		// --- 3. THE OFF-SWITCH: nothing is parked at all.
		OVT_TEST_InsertionObserverProbe unwatched = new OVT_TEST_InsertionObserverProbe();
		unwatched.m_bTransportIsObserver = false;
		unwatched.ProbeSetTruck(truck);
		unwatched.ProbeHoldTruckSimulated();

		if (virtualization.HasEntityObserver(truck))
			return "an observer was parked on the transport with m_bTransportIsObserver FALSE - the off-switch does nothing, so an operator cannot decline the cost of everything the route wakes up";

		int afterOff = virtualization.GetEntityObserverCount();
		if (afterOff != before)
			return "core's observer count is " + afterOff.ToString() + " with the off-switch off, expected " + before.ToString();

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Counts out the fixed delay, deletes the transport and reports.
	//! \return True once the case is finished.
	protected bool AwaitDeletion()
	{
		m_iFrames++;
		if (m_iFrames < DELETE_DELAY_FRAMES)
			return false;

		CleanUp();

		if (m_sFailure != "")
		{
			SetFailure(m_sFailure);
			return true;
		}

		Print("Insertion transport: an entity observer is parked on the hull while the convoy drives, is gone after ReleaseConvoy, and is never parked with m_bTransportIsObserver off");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes any observer off the transport and deletes it. Called on every exit path.
	protected void CleanUp()
	{
		if (!m_Truck)
			return;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (virtualization)
			virtualization.RemoveEntityObserver(m_Truck);

		SCR_EntityHelper.DeleteEntityAndChildren(m_Truck);
		m_Truck = null;
	}
}
