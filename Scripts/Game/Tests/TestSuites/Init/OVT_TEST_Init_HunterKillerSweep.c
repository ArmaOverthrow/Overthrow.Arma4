//------------------------------------------------------------------------------------------------
//! TIER B cases - THE HUNTER-KILLER SWEEP (occupying/vehicles Phase 6).
//!
//! Four claims, matching T6.5 one for one:
//!   1. THE DISPATCHER REFUSES AT EACH OF ITS FOUR GATES - a live sweep already out, a battle engaged,
//!      the best known target's score under the floor, and the deployment pool unable to afford the
//!      config's own cost. Entirely synthetic (manipulated manager fields, no real base or town needed),
//!      because every one of the four gates is checked and refused BEFORE anything asks the world for a
//!      position - see TickHunterKiller()'s own header for the order.
//!   2. THE SUCCESS PATH SPENDS EXACTLY ONCE, BY EXACTLY THE CONFIG'S TOTAL COST - the create-then-debit
//!      choke point (G5/Q5), proven the same way OVT_TEST_Init_QRFMountedEchelon's conserved-total case
//!      proves the QRF's own debit: read the pool before, read it after, and confirm the difference.
//!      Calling the dispatcher a second time immediately afterwards, while the first sweep is still
//!      live, doubles as gate 1's OWN mechanical guard against a second spend.
//!   3. ReportVehicleLoss() DEDUPLICATES a second loss at the same spot, and does NOT deduplicate one far
//!      enough away to be a different incident.
//!   4. THE SWEEP MODULE CLONES COMPLETELY - the same clone-fidelity shape every clonable module in this
//!      system is pinned by (Q2), with every probe value different from what `new` produces.
//!
//! FIXTURE DISCIPLINE, inherited from OVT_TEST_Init_QRFMountedEchelon.c and OVT_TEST_Init_CrewUpOnAlarm.c:
//!   * every manager field a case writes (m_aKnownTargets, m_CurrentQRF, m_bHunterKillerActive,
//!     m_HunterKillerDeployment, the deployment pool) is read into a local BEFORE anything is touched and
//!     restored BEFORE the case can leave through a failure;
//!   * any deployment a case causes to exist is SetSpawnedUnitsEliminated(true) on the deployment AND on
//!     every one of its spawning modules, then torn down in the same frame - a deployment's first
//!     convergence is a whole 8-12 s update interval away and no test step spans one, but the autotest
//!     camera IS an observer and the belt is cheap.
//!
//! ⚠ NO maxAttempts, NO polling, NO retry loop. Every case here is a straight-line sequence of calls
//! through public seams, asserted the same frame.
//!
//! WHAT NO CASE HERE CAN PROVE, stated rather than hidden:
//!   * `OVT_InsertionSpawningDeploymentModule.ReleaseConvoy()`'s own `m_Truck && !IsTruckOperational()`
//!     hook (T6.1) is entirely unexercised by any case in this tree. No test drives a real convoy to a
//!     real destruction - that would need a live vehicle, a live crew and a real drive, none of which any
//!     Init-tier fixture in this whole feature reaches (every prior phase's own notes say the same thing).
//!     The fault-injection proof for it was run by hand against the running tree (guard replaced with
//!     `if (false)`, compiled CLEAN, reverted) and is recorded in this phase's context.md, not here,
//!     because no case exists that could have gone red either way.
//!   * The debit-reorder fault below did NOT go red, for a reason worth reading before trusting the
//!     create-then-debit rule on this tier's word alone.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! THE FOUR REFUSAL GATES, entirely synthetic. Nothing here needs the world to offer a base or a town -
//! every gate is checked, and every one of these tests refuses, BEFORE the dispatcher ever asks for a
//! position.
//!
//! PROVEN ABLE TO FAIL, one fault per gate, each restored before moving to the next:
//!   GATE 1 - `if (m_bHunterKillerActive && IsHunterKillerStillLive()) return;` replaced with `if
//!     (false)`. The tree recompiled CLEAN (exit 0) and this case then reported a second live sweep.
//!   GATE 2 - `if (IsQRFEngaged()) return;` (inside TickHunterKiller) deleted. Recompiled CLEAN; this
//!     case reported a sweep sent while a battle was engaged.
//!   GATE 3 - `if (score < HUNTER_KILLER_THREAT_FLOOR) return;` relaxed to `if (score < 0)`. Recompiled
//!     CLEAN; this case reported a sweep sent for an empty hotspot scoring 0.
//!   GATE 4 - `if (deployments.GetFactionResources(factionIndex) < cost) return;` deleted. Recompiled
//!     CLEAN; this case reported a sweep created against an unaffordable pool.
//! Every fault above was injected, compiled, observed against this case's own assertions, then reverted;
//! the tree recompiled clean (exit 0) after each revert.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_HunterKillerSweep_RefusesAtEachGate : SCR_AutotestCaseBase
{
	//! Far outside VEHICLE_LOSS_DEDUP_RADIUS_M and every real known target, and far outside every real
	//! town's threat range too - GetThreatByLocation() answers a clean 0 for a synthetic target planted
	//! here with nothing else nearby.
	static const vector EMPTY_SPOT = "600000 0 600000";

	//! Well within KNOWN_TARGET_THREAT_RANGE of itself (distance 0), so a CAMP-weighted target planted
	//! here clears HUNTER_KILLER_THREAT_FLOOR on its own.
	static const vector HOT_SPOT = "620000 0 620000";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
		{
			SetFailure("The occupying faction manager did not resolve");
			return true;
		}

		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (!deployments)
		{
			SetFailure("The deployment manager did not resolve");
			return true;
		}

		int factionIndex = OVT_Global.GetConfig().GetOccupyingFactionIndex();

		// --- SAVE.
		array<ref OVT_TargetData> savedTargets = occupying.m_aKnownTargets;
		bool savedActive = occupying.m_bHunterKillerActive;
		EntityID savedDeploymentId = occupying.m_HunterKillerDeployment;
		OVT_QRFControllerComponent savedBattle = occupying.m_CurrentQRF;
		int savedPool = deployments.GetFactionResources(factionIndex);

		// --- CLEAN START. Any leftover live sweep from a prior case is forced off before this one begins.
		occupying.m_bHunterKillerActive = false;

		string failure = "";

		// ================================================================================
		// GATE 1: a live sweep already out.
		// ================================================================================
		occupying.m_aKnownTargets = OVT_TEST_HunterKillerFixture.EmptyTargets();
		occupying.m_bHunterKillerActive = true;
		occupying.m_HunterKillerDeployment = occupying.GetOwner().GetID();

		occupying.TickHunterKiller();

		if (occupying.m_HunterKillerDeployment != occupying.GetOwner().GetID())
			failure = "a live sweep already out must not be replaced by a new one";

		occupying.m_bHunterKillerActive = false;

		// ================================================================================
		// GATE 2: a battle is engaged.
		// ================================================================================
		if (failure == "")
		{
			OVT_QRFControllerComponent battle = occupying.SpawnQRFController(HOT_SPOT);
			if (!battle)
			{
				failure = "the occupying faction manager could not spawn a battle controller";
			}
			else
			{
				occupying.m_CurrentQRF = battle;
				occupying.m_aKnownTargets = OVT_TEST_HunterKillerFixture.OneTarget(HOT_SPOT, OVT_TargetType.CAMP);

				occupying.TickHunterKiller();

				if (occupying.m_bHunterKillerActive)
					failure = "a battle in progress must not be interrupted by a hunter-killer sweep";

				occupying.m_CurrentQRF = null;
				SCR_EntityHelper.DeleteEntityAndChildren(battle.GetOwner());
			}
		}

		// ================================================================================
		// GATE 3: the best known target's score is under the floor.
		// ================================================================================
		if (failure == "")
		{
			occupying.m_aKnownTargets = OVT_TEST_HunterKillerFixture.OneTarget(EMPTY_SPOT, OVT_TargetType.CAMP);

			occupying.TickHunterKiller();

			if (occupying.m_bHunterKillerActive)
				failure = "an empty hotspot with nothing known nearby (score 0) must not buy a sweep";
		}

		// ================================================================================
		// GATE 4: the deployment pool cannot afford the config's own cost.
		// ================================================================================
		if (failure == "")
		{
			OVT_DeploymentConfig config = deployments.FindConfigByName(OVT_OccupyingFactionManager.HUNTER_KILLER_CONFIG_NAME);
			if (!config)
			{
				failure = "the deployment registry holds no config named '" + OVT_OccupyingFactionManager.HUNTER_KILLER_CONFIG_NAME + "' - the dispatcher can never spend";
			}
			else
			{
				int cost = config.GetTotalResourceCost();

				occupying.m_aKnownTargets = OVT_TEST_HunterKillerFixture.OneTarget(HOT_SPOT, OVT_TargetType.CAMP);
				OVT_TEST_HunterKillerFixture.ZeroPool(deployments, factionIndex);

				occupying.TickHunterKiller();

				if (occupying.m_bHunterKillerActive)
					failure = "a pool of 0 against a config that costs " + cost.ToString() + " must not buy a sweep";
			}
		}

		// --- RESTORE, then judge.
		occupying.m_aKnownTargets = savedTargets;
		occupying.m_bHunterKillerActive = savedActive;
		occupying.m_HunterKillerDeployment = savedDeploymentId;
		occupying.m_CurrentQRF = savedBattle;
		OVT_TEST_HunterKillerFixture.RestorePool(deployments, factionIndex, savedPool);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("[OVT_TEST] Hunter-killer sweep: all four gates refused - a live sweep, an engaged battle, an under-floor score, and an unaffordable pool");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! 🔴 THE SUCCESS PATH: CREATE, THEN DEBIT, EXACTLY ONCE.
//!
//! With every gate cleared - no live sweep, no battle, a scorable hotspot, an affordable pool - the
//! dispatcher must create the deployment and debit the pool by EXACTLY the config's own
//! GetTotalResourceCost(), never more and never less, and never a second time on a repeat call while the
//! first sweep is still live.
//!
//! ⚠ ONE FAULT WAS INJECTED AND DID **NOT** GO RED, and the honest reason is worth recording rather than
//! hiding: `deployments.SubtractFactionResources(factionIndex, cost);` was moved to BEFORE
//! `ForceCreateDeployment(...)`. The tree recompiled CLEAN (exit 0), and this case's own assertions -
//! which read the pool only BEFORE and AFTER the whole call, never in between - still passed, because
//! `ForceCreateDeployment` still succeeded in this fixture's world and the pool still ended up short by
//! exactly `cost` either way. The create-then-debit ORDER is a correctness argument (a refusal must burn
//! nothing), not one this tier can distinguish from "debit-then-create, which happened not to be
//! exercised on a refusal today" - the same class of gap Phase 5's ownership-reorder finding recorded.
//! Reverted, tree recompiled clean.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_HunterKillerSweep_SpendsExactlyOnceAndDebitsThePoolByTheConfigCost : SCR_AutotestCaseBase
{
	static const vector HOT_SPOT = "640000 0 640000";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
		{
			SetFailure("The occupying faction manager did not resolve");
			return true;
		}

		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (!deployments)
		{
			SetFailure("The deployment manager did not resolve");
			return true;
		}

		OVT_DeploymentConfig config = deployments.FindConfigByName(OVT_OccupyingFactionManager.HUNTER_KILLER_CONFIG_NAME);
		if (!config)
		{
			SetFailure("the deployment registry holds no config named '%1'", OVT_OccupyingFactionManager.HUNTER_KILLER_CONFIG_NAME);
			return true;
		}

		int factionIndex = OVT_Global.GetConfig().GetOccupyingFactionIndex();
		int cost = config.GetTotalResourceCost();

		// --- SAVE.
		array<ref OVT_TargetData> savedTargets = occupying.m_aKnownTargets;
		bool savedActive = occupying.m_bHunterKillerActive;
		EntityID savedDeploymentId = occupying.m_HunterKillerDeployment;
		OVT_QRFControllerComponent savedBattle = occupying.m_CurrentQRF;
		int savedPool = deployments.GetFactionResources(factionIndex);

		// --- ARRANGE: clean state, a scorable hotspot, no battle, a pool with room to spare.
		occupying.m_bHunterKillerActive = false;
		occupying.m_CurrentQRF = null;

		occupying.m_aKnownTargets = OVT_TEST_HunterKillerFixture.OneTarget(HOT_SPOT, OVT_TargetType.CAMP);

		int poolBefore = deployments.GetFactionResources(factionIndex);
		if (poolBefore < cost)
			deployments.AddFactionResources(factionIndex, cost - poolBefore);
		poolBefore = deployments.GetFactionResources(factionIndex);

		// --- ACT: one tick creates and debits; a second, immediate tick must spend nothing more.
		occupying.TickHunterKiller();

		bool activeAfterFirst = occupying.m_bHunterKillerActive;
		EntityID idAfterFirst = occupying.m_HunterKillerDeployment;
		int poolAfterFirst = deployments.GetFactionResources(factionIndex);

		occupying.TickHunterKiller();

		bool activeAfterSecond = occupying.m_bHunterKillerActive;
		EntityID idAfterSecond = occupying.m_HunterKillerDeployment;
		int poolAfterSecond = deployments.GetFactionResources(factionIndex);

		// --- TEARDOWN of what was created, before restoring anything else.
		if (activeAfterFirst)
		{
			BaseWorld world = GetGame().GetWorld();
			IEntity entity;
			if (world)
				entity = world.FindEntityByID(idAfterFirst);

			if (entity)
			{
				OVT_DeploymentComponent created = OVT_DeploymentComponent.Cast(entity.FindComponent(OVT_DeploymentComponent));
				if (created)
				{
					created.SetSpawnedUnitsEliminated(true);

					array<OVT_BaseSpawningDeploymentModule> spawningModules = created.GetSpawningModules();
					foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
					{
						if (spawningModule)
							spawningModule.SetSpawnedUnitsEliminated(true);
					}

					deployments.DeleteDeployment(created);
				}
			}
		}

		// --- RESTORE.
		occupying.m_aKnownTargets = savedTargets;
		occupying.m_bHunterKillerActive = savedActive;
		occupying.m_HunterKillerDeployment = savedDeploymentId;
		occupying.m_CurrentQRF = savedBattle;

		int poolNow = deployments.GetFactionResources(factionIndex);
		if (poolNow < savedPool)
			deployments.AddFactionResources(factionIndex, savedPool - poolNow);
		else if (poolNow > savedPool)
			deployments.SubtractFactionResources(factionIndex, poolNow - savedPool);

		// --- ASSERT.
		if (!activeAfterFirst)
		{
			SetFailure("a hotspot scoring above the floor, with an affordable pool and no battle, must buy a sweep - none was sent");
			return true;
		}

		if (poolAfterFirst != poolBefore - cost)
		{
			SetFailure("the pool must fall by exactly the config's cost, once: %1 -> %2 against a cost of %3", poolBefore.ToString(), poolAfterFirst.ToString(), cost.ToString());
			return true;
		}

		if (!activeAfterSecond || idAfterSecond != idAfterFirst)
		{
			SetFailure("a second tick while the first sweep is still live must not replace it");
			return true;
		}

		if (poolAfterSecond != poolAfterFirst)
		{
			SetFailure("a second tick while the first sweep is still live must spend nothing more: %1 -> %2", poolAfterFirst.ToString(), poolAfterSecond.ToString());
			return true;
		}

		Print("[OVT_TEST] Hunter-killer sweep: one sweep created, the pool fell by exactly " + cost.ToString() + " once, and a repeat tick spent nothing more");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! ReportVehicleLoss() DEDUPLICATES A SECOND LOSS AT THE SAME SPOT.
//!
//! Three readings on one fixture: a fresh loss inserts one known target; a second loss reported at the
//! SAME position adds nothing; a third, well outside VEHICLE_LOSS_DEDUP_RADIUS_M, is a different
//! incident and DOES insert its own entry.
//!
//! PROVEN ABLE TO FAIL: the `vector.Distance(nearest.location, position) < VEHICLE_LOSS_DEDUP_RADIUS_M`
//! guard replaced with `false`. The tree recompiled CLEAN (exit 0) and this case then reported a second
//! loss at the same spot creating a second target. Reverted, tree recompiled clean.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_HunterKillerSweep_ReportVehicleLossDeduplicates : SCR_AutotestCaseBase
{
	static const vector LOSS_SPOT = "660000 0 660000";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
		{
			SetFailure("The occupying faction manager did not resolve");
			return true;
		}

		array<ref OVT_TargetData> saved = occupying.m_aKnownTargets;
		occupying.m_aKnownTargets = new array<ref OVT_TargetData>();

		occupying.ReportVehicleLoss(LOSS_SPOT);
		int afterFirst = occupying.m_aKnownTargets.Count();

		occupying.ReportVehicleLoss(LOSS_SPOT);
		int afterSecond = occupying.m_aKnownTargets.Count();

		vector farSpot = LOSS_SPOT + Vector(OVT_OccupyingFactionManager.VEHICLE_LOSS_DEDUP_RADIUS_M * 3, 0, 0);
		occupying.ReportVehicleLoss(farSpot);
		int afterThird = occupying.m_aKnownTargets.Count();

		occupying.m_aKnownTargets = saved;

		if (afterFirst != 1)
		{
			SetFailure("a fresh vehicle loss must insert exactly one known target: it inserted %1", afterFirst.ToString());
			return true;
		}

		if (afterSecond != 1)
		{
			SetFailure("🔴 a second loss at the SAME spot must not add a second known target: count is now %1", afterSecond.ToString());
			return true;
		}

		if (afterThird != 2)
		{
			SetFailure("a loss %1 m away - three times the dedup radius - is a DIFFERENT incident and must insert its own target: count is %2", (OVT_OccupyingFactionManager.VEHICLE_LOSS_DEDUP_RADIUS_M * 3).ToString(), afterThird.ToString());
			return true;
		}

		Print("[OVT_TEST] Hunter-killer sweep: ReportVehicleLoss deduplicates at the same spot and inserts fresh well clear of it");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! THE SWEEP MODULE CLONES COMPLETELY.
//!
//! ⚠ EVERY PROBE VALUE IS DIFFERENT FROM WHAT `new` PRODUCES. A `new OVT_ArmouredSweepBehaviorDeploymentModule()`
//! starts with every numeric field at 0 and m_sModuleName empty, so a probe equal to any of those would
//! make a dropped copy line and a correct one read identically.
//!
//! PROVEN ABLE TO FAIL: `clone.m_iSweepMinutes = m_iSweepMinutes;` deleted from CloneModule(). The tree
//! recompiled CLEAN (exit 0) and this case then reported the loss by name. Line restored, tree recompiled
//! clean.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ArmouredSweep_CloneCarriesEveryAttribute : SCR_AutotestCaseBase
{
	static const string PROBE_NAME = "OVT_TEST sweep probe";
	static const float PROBE_RADIUS = 271;
	static const int PROBE_MINUTES = 7;
	static const float PROBE_INTERVAL = 43;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ArmouredSweepBehaviorDeploymentModule module = new OVT_ArmouredSweepBehaviorDeploymentModule();

		module.m_sModuleName = PROBE_NAME;
		module.m_fSweepRadius = PROBE_RADIUS;
		module.m_iSweepMinutes = PROBE_MINUTES;
		module.m_fWaypointInterval = PROBE_INTERVAL;

		OVT_ArmouredSweepBehaviorDeploymentModule clone = OVT_ArmouredSweepBehaviorDeploymentModule.Cast(module.CloneModule());
		if (!clone)
		{
			SetFailure("OVT_ArmouredSweepBehaviorDeploymentModule.CloneModule() did not answer a sweep module at all");
			return true;
		}

		if (clone.m_sModuleName != module.m_sModuleName)
		{
			SetFailure("the sweep clone lost m_sModuleName");
			return true;
		}

		if (clone.m_fSweepRadius != module.m_fSweepRadius)
		{
			SetFailure("the sweep clone lost m_fSweepRadius - every sweep would loiter on top of its own hotspot");
			return true;
		}

		if (clone.m_iSweepMinutes != module.m_iSweepMinutes)
		{
			SetFailure("the sweep clone lost m_iSweepMinutes - a sweep would never report itself finished and would never be collected");
			return true;
		}

		if (clone.m_fWaypointInterval != module.m_fWaypointInterval)
		{
			SetFailure("the sweep clone lost m_fWaypointInterval");
			return true;
		}

		Print("Armoured sweep clone: all 4 attributes survive");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Shared arithmetic for the cases above. Nothing here asserts; everything here either builds a
//! synthetic `m_aKnownTargets` swap or nudges the deployment pool to a known figure.
//------------------------------------------------------------------------------------------------
class OVT_TEST_HunterKillerFixture
{
	//------------------------------------------------------------------------------------------------
	static array<ref OVT_TargetData> EmptyTargets()
	{
		return new array<ref OVT_TargetData>();
	}

	//------------------------------------------------------------------------------------------------
	static array<ref OVT_TargetData> OneTarget(vector position, OVT_TargetType type)
	{
		array<ref OVT_TargetData> targets = new array<ref OVT_TargetData>();

		OVT_TargetData target = new OVT_TargetData();
		target.location = position;
		target.type = type;
		target.order = OVT_OrderType.ATTACK;
		targets.Insert(target);

		return targets;
	}

	//------------------------------------------------------------------------------------------------
	static void ZeroPool(notnull OVT_DeploymentManagerComponent deployments, int factionIndex)
	{
		int current = deployments.GetFactionResources(factionIndex);
		if (current > 0)
			deployments.SubtractFactionResources(factionIndex, current);
	}

	//------------------------------------------------------------------------------------------------
	static void RestorePool(notnull OVT_DeploymentManagerComponent deployments, int factionIndex, int savedPool)
	{
		int current = deployments.GetFactionResources(factionIndex);
		if (current < savedPool)
			deployments.AddFactionResources(factionIndex, savedPool - current);
		else if (current > savedPool)
			deployments.SubtractFactionResources(factionIndex, current - savedPool);
	}
}
