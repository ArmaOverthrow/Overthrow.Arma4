//------------------------------------------------------------------------------------------------
//! TIER B cases - THE QRF's MOUNTED ECHELON, on a real battle controller in a real world.
//!
//! occupying/vehicles Phase 4 puts one new call inside SendWave() and SpendWholeBudgetInOnePass(), on
//! the component that resolves every battle in the game. Three things about that call are worth a live
//! fixture, and they are the three these cases pin:
//!
//!   1. 🔴 THE ACCOUNTING. A wave debits the occupying faction's RESERVE (m_iResources) exactly once,
//!      outside the mode branch - BUG-027's shape - and the deployment framework's own teardowns
//!      credit a DIFFERENT ledger, the deployment POOL. An echelon that joined the wrong one, or that
//!      added a second debit, would move money between two ledgers and create it. The fourth case
//!      below measures both ledgers across a whole wave.
//!   2. THE GATES. Every refusal is silent by design (a campaign with no roads at a base must not spam
//!      the log), so nothing about them is visible at play-test. The first three cases drive them.
//!   3. THE SCORING QUESTION (T4.7). An echelon is only worth sending if the men riding it COUNT when
//!      they get there. The last case asks the two questions CheckUpdatePoints actually asks - the
//!      faction key and IsFightingFit - of a character that is genuinely sitting in a vehicle.
//!
//! FIXTURE DISCIPLINE, inherited from OVT_TEST_Init_QRFSiege.c and unchanged:
//!   * every controller is spawned through the occupying faction manager's own SpawnQRFController and
//!     deleted with DeleteEntityAndChildren, which is how the campaign disposes of a finished battle;
//!   * every manager field a case writes is read into a local BEFORE anything is spawned and restored
//!     BEFORE the case can leave through a failure;
//!   * ⚠ every deployment a case causes to exist is SetSpawnedUnitsEliminated(true) on the deployment
//!     AND on every one of its spawning modules before anything can tick, and is then torn down in the
//!     same frame. A deployment's first convergence is a whole 8-12 s update interval after it is
//!     created, and no test step spans one, so none of these ever converges - but the autotest camera
//!     IS an observer and the belt is cheap.
//!
//! ⚠ NO maxAttempts, NO polling, NO retry loop. Two cases take exactly one unconditional frame hop, in
//! the shape OVT_TEST_Init_QRFSiege.c established, so that an entity is world-registered before its id
//! is used and an AI agent has been offered to the AI world before it is looked for.
//!
//! ⚠ WHAT A DIAGNOSTIC MEANS HERE. Three of these cases need the WORLD to offer something - a base with
//! a road near it, two of them, a compartment a character can be put in. Where the world cannot, the
//! case says so in the log and asserts only the half it could reach, rather than passing silently or
//! reddening on the map instead of on the code. Each one names which half it lost.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! THE REACHABILITY GATE REFUSES A LAND-ISOLATED SOURCE (occupying/vehicles D10).
//!
//! There is no A-to-B land-reachability query anywhere in the engine or in this campaign, and the
//! feature invents none: it asks the two statements that DO exist - the map author's landIsolated flag
//! on the base record, and whether there is a road near the source. This case drives the first.
//!
//! TWO HALVES, AND ONLY ONE OF THEM IS UNCONDITIONAL:
//!   * UNCONDITIONAL: with the flag set, no echelon is sent and none is created. If this ever goes
//!     green-to-red the gate has been removed and the faction will drive armour off an island.
//!   * DIFFERENTIAL, when the world allows it: with the flag cleared and NOTHING ELSE CHANGED, the
//!     same call succeeds. That is what proves the flag was the deciding input rather than one of the
//!     five other reasons this method legitimately answers zero.
//!
//! PROVEN ABLE TO FAIL: the `&& nearest.landIsolated` term was deleted from IsEchelonSourceReachable,
//! which is exactly what a "tidy up this condition" edit produces. The tree recompiled CLEAN (exit 0),
//! so nothing in the toolchain would have stopped it shipping, and the case then reports "a
//! land-isolated source must send no echelon". Term restored, tree recompiled clean.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_QRFMountedEchelon_TheReachabilityGateRefusesALandIsolatedSource : SCR_AutotestCaseBase
{
	//! A slice far larger than any rung, so that nothing but the gate under test can refuse.
	static const int UNBOUNDED_SLICE = 100000;

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

		vector target;
		if (!OVT_TEST_EchelonFixture.ResolveTargetPosition(occupying, target))
		{
			Print("[OVT_TEST] QRF mounted echelon: the world produced neither a town nor a base to hang a fixture battle on - the reachability gate was not exercised");
			return true;
		}

		OVT_BaseData source = OVT_TEST_EchelonFixture.ResolveDrivableSource(occupying, target);
		if (!source)
		{
			Print("[OVT_TEST] QRF mounted echelon: no occupying base with a road near it and clear of the fixture objective - the reachability gate was not exercised");
			return true;
		}

		OVT_QRFControllerComponent battle = occupying.SpawnQRFController(target);
		if (!battle)
		{
			SetFailure("The occupying faction manager could not spawn a battle controller");
			return true;
		}

		bool savedIsolated = source.landIsolated;

		// --- HALF ONE: isolated. Nothing may be sent and nothing may be created.
		source.landIsolated = true;
		int isolatedCost = battle.SendMountedEchelon(source.location, target, UNBOUNDED_SLICE);
		int isolatedSent = battle.GetEchelonsSent();

		// --- HALF TWO: the same call with the one flag cleared.
		source.landIsolated = false;
		int reachableCost = battle.SendMountedEchelon(source.location, target, UNBOUNDED_SLICE);
		int reachableSent = battle.GetEchelonsSent();

		// --- RESTORE, then tear down, then judge.
		source.landIsolated = savedIsolated;
		OVT_TEST_EchelonFixture.RetireBattle(battle);

		if (isolatedCost != 0)
		{
			SetFailure("a land-isolated source must send no echelon: it committed %1 resources", isolatedCost.ToString());
			return true;
		}

		if (isolatedSent != 0)
		{
			SetFailure("a land-isolated source must CREATE no echelon: %1 were stood up", isolatedSent.ToString());
			return true;
		}

		if (reachableCost <= 0 || reachableSent != 1)
		{
			Print("[OVT_TEST] QRF mounted echelon: clearing landIsolated did not by itself make this source usable (cost " + reachableCost.ToString() + ") - the refusal above is proven, the DIFFERENTIAL is not");
			return true;
		}

		Print("[OVT_TEST] QRF mounted echelon: landIsolated is the deciding input - the same source refused while isolated and committed " + reachableCost.ToString() + " resources once cleared");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! THE BUDGET GATE, and with it the shape of the shipped config.
//!
//! Three claims, none of which needs the world to offer anything:
//!   1. The deployment registry really carries "QRF Mounted Echelon" and it really authors a mounted
//!      force module with a ladder role and a positive vehicle budget. Nothing else in the toolchain
//!      reads a .conf - compile-check.sh does not parse them - so this is the only mechanical check
//!      that the config Phase 4 shipped is the config Phase 4 wrote.
//!   2. A slice under the CHEAPEST RUNG sends nothing.
//!   3. 🔴 A slice under the config's VEHICLE BUDGET sends nothing either, even though it would cover
//!      the cheapest rung. That is the stricter gate and it is deliberate: the module resolves the
//!      ladder AGAIN when it spawns, against m_iTruckCostOverride and nothing else, so a wave that
//!      priced a rung against a smaller number would charge for a UAZ and field a BRDM - the reserve
//!      short by the difference, silently, on every echelon.
//!
//! PROVEN ABLE TO FAIL: `if(budget < vehicleBudget)` was relaxed to `if(budget <= 0)`, the plan's own
//! wording. The tree recompiled CLEAN (exit 0) and the case then reports the third claim.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_QRFMountedEchelon_TheBudgetGateRefusesASliceUnderTheVehicleBudget : SCR_AutotestCaseBase
{
	//! Under every rung in both shipped registries (the cheapest is 25).
	static const int SLICE_UNDER_EVERY_RUNG = 10;

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

		// --- CLAIM 1: the config exists, is registered, and authors what the QRF reads off it.
		OVT_DeploymentConfig config = deployments.FindConfigByName(OVT_QRFControllerComponent.ECHELON_CONFIG_NAME);
		if (!config)
		{
			SetFailure("the deployment registry holds no config named '%1' - the echelon can never be created", OVT_QRFControllerComponent.ECHELON_CONFIG_NAME);
			return true;
		}

		OVT_MountedForceSpawningDeploymentModule mounted = OVT_TEST_EchelonFixture.FindMountedTemplate(config);
		if (!mounted)
		{
			SetFailure("'%1' authors no OVT_MountedForceSpawningDeploymentModule, so nothing about it drives anywhere", OVT_QRFControllerComponent.ECHELON_CONFIG_NAME);
			return true;
		}

		int vehicleBudget = mounted.m_iTruckCostOverride;
		if (vehicleBudget <= 0)
		{
			SetFailure("'%1' authors a vehicle budget of %2, and a non-positive budget can never resolve a rung", OVT_QRFControllerComponent.ECHELON_CONFIG_NAME, vehicleBudget.ToString());
			return true;
		}

		if (mounted.m_sVehicleRole.IsEmpty())
		{
			SetFailure("'%1' authors no ladder role, so its echelon would field the named truck rather than an armed vehicle", OVT_QRFControllerComponent.ECHELON_CONFIG_NAME);
			return true;
		}

		if (vehicleBudget <= SLICE_UNDER_EVERY_RUNG)
		{
			SetFailure("the fixture's under-every-rung slice of %1 is not under the authored vehicle budget of %2, so claims 2 and 3 would be the same claim", SLICE_UNDER_EVERY_RUNG.ToString(), vehicleBudget.ToString());
			return true;
		}

		vector target;
		if (!OVT_TEST_EchelonFixture.ResolveTargetPosition(occupying, target))
		{
			Print("[OVT_TEST] QRF mounted echelon: the world produced nowhere to hang a fixture battle - the budget gate was not exercised");
			return true;
		}

		OVT_BaseData source = OVT_TEST_EchelonFixture.ResolveDrivableSource(occupying, target);
		if (!source)
		{
			Print("[OVT_TEST] QRF mounted echelon: no drivable source - the budget gate was not exercised");
			return true;
		}

		OVT_QRFControllerComponent battle = occupying.SpawnQRFController(target);
		if (!battle)
		{
			SetFailure("The occupying faction manager could not spawn a battle controller");
			return true;
		}

		bool savedIsolated = source.landIsolated;
		source.landIsolated = false;

		int underEveryRung = battle.SendMountedEchelon(source.location, target, SLICE_UNDER_EVERY_RUNG);
		int underTheBudget = battle.SendMountedEchelon(source.location, target, vehicleBudget - 1);
		int atTheBudget = battle.SendMountedEchelon(source.location, target, vehicleBudget);
		int sent = battle.GetEchelonsSent();

		source.landIsolated = savedIsolated;
		OVT_TEST_EchelonFixture.RetireBattle(battle);

		if (underEveryRung != 0)
		{
			SetFailure("a slice of %1, under every rung in the registry, must send no echelon: it committed %2", SLICE_UNDER_EVERY_RUNG.ToString(), underEveryRung.ToString());
			return true;
		}

		if (underTheBudget != 0)
		{
			SetFailure("a slice of %1, one under the config's vehicle budget, must send no echelon - or the wave prices a cheaper rung than the module will field", (vehicleBudget - 1).ToString());
			return true;
		}

		if (atTheBudget > 0 && sent != 1)
		{
			SetFailure("one echelon was paid for (%1) but %2 were recorded for teardown", atTheBudget.ToString(), sent.ToString());
			return true;
		}

		if (atTheBudget <= 0)
		{
			Print("[OVT_TEST] QRF mounted echelon: a slice of exactly the vehicle budget was still refused in this world (no road at the standoff, or the ladder is above the budget) - both refusals are proven, the acceptance is not");
			return true;
		}

		Print("[OVT_TEST] QRF mounted echelon: the budget gate is the config's vehicle ceiling of " + vehicleBudget.ToString() + " - refused at " + (vehicleBudget - 1).ToString() + ", accepted at " + vehicleBudget.ToString());

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! THE PER-BATTLE CAP, and the identity between what is CHARGED and what will be FIELDED.
//!
//! Two claims on one fixture, because both need real creations and a real creation is the expensive
//! part:
//!   1. THE COST IS THE RUNG COST. What SendMountedEchelon returns - and therefore what reaches the
//!      wave's `spent` and the faction's reserve - is exactly the m_iCost of the rung the ladder picks
//!      for this faction, at this threat, at this difficulty scale, inside the config's own vehicle
//!      budget. The expected figure is resolved here through the SAME public resolver the module will
//!      use, not restated as a number a registry edit would silently break.
//!   2. 🔴 THE THIRD ECHELON IS REFUSED. The cap is per BATTLE and not per source, because the siege
//!      spend walks its source list again and again until the budget is gone - a per-source rule would
//!      put an armed vehicle on the road on every pass, and there is no other bound on the count.
//!
//! PROVEN ABLE TO FAIL: the cap test was changed from `>=` to `>`, the classic off-by-one, which lets
//! a battle send three. The tree recompiled CLEAN (exit 0) and the case then reports the second claim.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_QRFMountedEchelon_TheCapBoundsABattleAndTheCostIsTheRungCost : SCR_AutotestCaseBase
{
	static const int UNBOUNDED_SLICE = 100000;

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

		OVT_DeploymentConfig config = deployments.FindConfigByName(OVT_QRFControllerComponent.ECHELON_CONFIG_NAME);
		if (!config)
		{
			SetFailure("the deployment registry holds no config named '%1'", OVT_QRFControllerComponent.ECHELON_CONFIG_NAME);
			return true;
		}

		OVT_MountedForceSpawningDeploymentModule mounted = OVT_TEST_EchelonFixture.FindMountedTemplate(config);
		if (!mounted)
		{
			SetFailure("'%1' authors no mounted force module", OVT_QRFControllerComponent.ECHELON_CONFIG_NAME);
			return true;
		}

		int expectedCost;
		if (!OVT_TEST_EchelonFixture.ResolveExpectedRungCost(occupying, mounted, expectedCost))
		{
			Print("[OVT_TEST] QRF mounted echelon: the ladder answers no rung for this faction at this threat inside the config's vehicle budget - the cap and the cost identity were not exercised");
			return true;
		}

		vector target;
		if (!OVT_TEST_EchelonFixture.ResolveTargetPosition(occupying, target))
		{
			Print("[OVT_TEST] QRF mounted echelon: the world produced nowhere to hang a fixture battle - the cap was not exercised");
			return true;
		}

		OVT_BaseData source = OVT_TEST_EchelonFixture.ResolveDrivableSource(occupying, target);
		if (!source)
		{
			Print("[OVT_TEST] QRF mounted echelon: no drivable source - the cap was not exercised");
			return true;
		}

		OVT_QRFControllerComponent battle = occupying.SpawnQRFController(target);
		if (!battle)
		{
			SetFailure("The occupying faction manager could not spawn a battle controller");
			return true;
		}

		bool savedIsolated = source.landIsolated;
		source.landIsolated = false;

		int first = battle.SendMountedEchelon(source.location, target, UNBOUNDED_SLICE);
		int second = battle.SendMountedEchelon(source.location, target, UNBOUNDED_SLICE);
		int third = battle.SendMountedEchelon(source.location, target, UNBOUNDED_SLICE);
		int sent = battle.GetEchelonsSent();

		source.landIsolated = savedIsolated;
		OVT_TEST_EchelonFixture.RetireBattle(battle);

		if (first <= 0)
		{
			Print("[OVT_TEST] QRF mounted echelon: the first echelon was refused in this world (no road at the standoff) - neither the cap nor the cost identity was exercised");
			return true;
		}

		if (first != expectedCost)
		{
			SetFailure("what a wave is charged must be the rung it will field: charged %1, the ladder answers %2", first.ToString(), expectedCost.ToString());
			return true;
		}

		if (second <= 0)
		{
			Print("[OVT_TEST] QRF mounted echelon: the second echelon was refused in this world, so the CAP was not reached - the cost identity is proven, the cap is not");
			return true;
		}

		if (second != expectedCost)
		{
			SetFailure("the second echelon was charged %1 where the ladder answers %2", second.ToString(), expectedCost.ToString());
			return true;
		}

		if (third != 0)
		{
			SetFailure("a battle may send exactly %1 echelons: the third committed %2", OVT_QRFControllerComponent.ECHELON_CAP_PER_BATTLE.ToString(), third.ToString());
			return true;
		}

		if (sent != OVT_QRFControllerComponent.ECHELON_CAP_PER_BATTLE)
		{
			SetFailure("the battle recorded %1 echelons where the cap is %2", sent.ToString(), OVT_QRFControllerComponent.ECHELON_CAP_PER_BATTLE.ToString());
			return true;
		}

		Print("[OVT_TEST] QRF mounted echelon: two echelons at " + expectedCost.ToString() + " each, and the third refused by the per-battle cap");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! 🔴 THE CONSERVED TOTAL. A WHOLE WAVE DEBITS THE RESERVE ONCE, BY EXACTLY WHAT IT COMMITTED, AND
//! NEVER TOUCHES THE DEPLOYMENT POOL.
//!
//! ==================================================================================================
//! WHY THIS IS THE MOST IMPORTANT CASE IN THE PHASE.
//! ==================================================================================================
//! There are TWO ledgers and they are not the same one:
//!
//!   * m_OccupyingFaction.m_iResources - the RESERVE - is what a QRF wave spends. SendWave() debits it
//!     exactly once, outside the mode branch. Running that debit twice charges the faction twice for
//!     one force, which is BUG-027's shape; skipping it makes battles free.
//!   * the deployment manager's per-faction pool - what AddFactionResources credits - is what buys
//!     deployments. An echelon collected or recalled rather than DELETED would pay its refund into
//!     this ledger out of money the other one spent, so the faction would end every battle richer.
//!
//! Neither fault has any symptom a player or a play-test could see: the numbers are internal and the
//! drift is a few tens of resources per battle. This case is the only thing that would ever notice.
//!
//! It drives Start() rather than SendWave() deliberately - Start() is the campaign's own entry point,
//! and it is what decides the source list, the budget and therefore the number of echelons.
//!
//! ⚠ THE SPAWN QUEUE IS EMPTIED AND THE CONTROLLER DELETED IN THE SAME FRAME. Start() fills the queue
//! with real group prefabs and the controller's own 1 000 ms timer is what pops them; a test step is
//! one frame, so no group is ever spawned. OnDelete() also cancels the follow-up wave Start() may have
//! scheduled 4-8 minutes out, which would otherwise be a call queued against a deleted component.
//!
//! PROVEN ABLE TO FAIL: `m_OccupyingFaction.m_iResources -= cost;` was added inside SendMountedEchelon
//! - the second debit, written the way a second debit is habitually written, right beside the thing it
//! pays for. The tree recompiled CLEAN (exit 0) and the case then reports "the reserve fell by N where
//! the wave committed M". Removed, tree recompiled clean.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_QRFMountedEchelon_AWaveDebitsTheReserveOnceAndNeverThePool : SCR_AutotestCaseBase
{
	//! Far above any maxQRF in any shipped preset, so the reserve can never hit SendWave's zero clamp
	//! and the identity below is an equality rather than an inequality.
	static const int PLANTED_RESERVE = 500000;

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

		vector target;
		if (!OVT_TEST_EchelonFixture.ResolveTargetPosition(occupying, target))
		{
			Print("[OVT_TEST] QRF mounted echelon: the world produced nowhere to hang a fixture battle - the ledger identity was not exercised");
			return true;
		}

		int factionIndex = OVT_Global.GetConfig().GetOccupyingFactionIndex();

		// --- SAVE.
		OVT_QRFControllerComponent savedBattle = occupying.m_CurrentQRF;
		int savedReserve = occupying.m_iResources;
		int savedTown = occupying.m_iCurrentQRFTown;
		int savedBase = occupying.m_iCurrentQRFBase;
		int savedPool = deployments.GetFactionResources(factionIndex);

		// Both objective indices to -1 for the same reason OVT_TEST_Init_QRFSiege forces them: at -1
		// nothing this fixture does publishes a notification or a civilian suppression for a place
		// that is not really under attack.
		occupying.m_iCurrentQRFTown = -1;
		occupying.m_iCurrentQRFBase = -1;
		occupying.m_iResources = PLANTED_RESERVE;

		OVT_QRFControllerComponent battle = occupying.SpawnQRFController(target);
		if (!battle)
		{
			occupying.m_iResources = savedReserve;
			occupying.m_iCurrentQRFTown = savedTown;
			occupying.m_iCurrentQRFBase = savedBase;
			SetFailure("The occupying faction manager could not spawn a battle controller");
			return true;
		}

		// --- ONE WHOLE WAVE, through the campaign's own entry point.
		battle.Start();

		int committed = battle.m_iUsedResources;
		int reserveAfter = occupying.m_iResources;
		int poolAfter = deployments.GetFactionResources(factionIndex);
		int echelonsSent = battle.GetEchelonsSent();
		int liveBeforeTeardown = battle.GetLiveEchelonCount();

		// --- The teardown under test, then the fixture's own.
		OVT_TEST_EchelonFixture.MarkEchelonsEliminated(battle);
		battle.TearDownEchelons();
		int liveAfterTeardown = battle.GetLiveEchelonCount();
		int poolAfterTeardown = deployments.GetFactionResources(factionIndex);

		OVT_TEST_EchelonFixture.RetireBattle(battle);

		// --- RESTORE before asserting.
		occupying.m_CurrentQRF = savedBattle;
		occupying.m_iResources = savedReserve;
		occupying.m_iCurrentQRFTown = savedTown;
		occupying.m_iCurrentQRFBase = savedBase;

		// --- ASSERT.
		if (committed <= 0)
		{
			SetFailure("a wave that committed nothing at all cannot measure the debit - the fixture reserve of %1 bought no troops", PLANTED_RESERVE.ToString());
			return true;
		}

		if (reserveAfter != PLANTED_RESERVE - committed)
		{
			SetFailure("the reserve must fall by exactly what the wave committed, once: %1 -> %2 against a commitment of %3", PLANTED_RESERVE.ToString(), reserveAfter.ToString(), committed.ToString());
			return true;
		}

		if (poolAfter != savedPool)
		{
			SetFailure("a QRF wave must never touch the deployment POOL: %1 -> %2", savedPool.ToString(), poolAfter.ToString());
			return true;
		}

		if (poolAfterTeardown != savedPool)
		{
			SetFailure("tearing an echelon down must credit nothing - a collection would pay the POOL out of what the RESERVE spent: %1 -> %2", savedPool.ToString(), poolAfterTeardown.ToString());
			return true;
		}

		if (liveAfterTeardown != 0)
		{
			SetFailure("every echelon must be gone when the battle ends: %1 still recorded", liveAfterTeardown.ToString());
			return true;
		}

		if (echelonsSent == 0)
		{
			Print("[OVT_TEST] QRF mounted echelon: the wave sent no echelon in this world, so the ledger identity is proven for the INFANTRY spend only - committed " + committed.ToString());
			return true;
		}

		Print("[OVT_TEST] QRF mounted echelon: a wave with " + echelonsSent.ToString() + " echelon(s) committed " + committed.ToString() + ", the reserve fell by exactly that once, the pool never moved, and " + liveBeforeTeardown.ToString() + " deployment(s) were deleted rather than collected");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! T4.7 - WHAT ZONE SCORING ACTUALLY ASKS OF A MAN SITTING IN A VEHICLE.
//!
//! An echelon is only worth sending if its crew COUNTS when it gets there. CheckUpdatePoints asks
//! three questions of every AI agent in the world, and this case asks the same three of a character
//! that is genuinely seated in a compartment:
//!
//!   1. it surfaces in GetGame().GetAIWorld().GetAIAgents() with a SCR_ChimeraCharacter under it;
//!   2. its GetFactionKey() is the occupying faction's key;
//!   3. IsFightingFit() is true - the predicate the scoring loop uses to refuse dead and downed men.
//!
//! and one the loop depends on without saying so:
//!
//!   4. 🔴 ITS GetOrigin() IS A WORLD POSITION WHILE IT IS INSIDE A VEHICLE. The loop measures
//!      vector.Distance(character.GetOrigin(), objective). If a seated character answered a position
//!      LOCAL to the vehicle it is parented to, every mounted crew in the game would measure as being
//!      thousands of metres away and no echelon would ever hold any ground, with nothing in the log.
//!      Vanilla's own capture-zone logic reads the same value the same way and only excludes seated
//!      characters for being too high (SCR_SeizingComponent.c:302), which is the reading this pins.
//!
//! ⚠ THIS CASE MAY NOT CHANGE ANYTHING IT FINDS. Its whole purpose is a verdict: if a seated occupant
//! were excluded, the fix belongs to the `qrf` feature and not to `occupying/vehicles`. The one edit it
//! justified was making IsFightingFit() public, exactly as CheckUpdatePoints and CheckUpdateTimer
//! already are, so that the answer is measured rather than asserted in prose.
//!
//! ⚠ THREE UNCONDITIONAL FRAME HOPS, and not one of them is a retry: spawn, then seat, then settle,
//! then judge. The middle one exists because MoveInVehicle() addresses an RPC to the character's owner
//! and the teleport happens when that lands, which is not the frame it was sent on. There is no
//! attempt counter and the judging pass asserts unconditionally.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_QRFMountedEchelon_ZoneScoringCountsASeatedCrew : SCR_AutotestCaseBase
{
	//! How far from the fixture objective the vehicle is put, in metres on each of two axes. Well
	//! inside OVT_QRFControllerComponent.QRF_POINT_RANGE once the diagonal is taken.
	static const float FIXTURE_OFFSET_M = 40;

	//! Which unconditional pass this is. FOUR of them, and every hop is fixed rather than polled:
	//!   0 spawn the vehicle and the man;
	//!   1 the entities are world-registered, so put the man in the driver's seat;
	//!   2 nothing - MoveInVehicle() addresses an RPC to the character's owner and the teleport happens
	//!     when it lands, which is not the frame it was sent on;
	//!   3 take every reading and judge.
	//!
	//! ⚠ NOT A RETRY. There is no attempt counter and pass 3 asserts unconditionally; if the man is not
	//! in the seat by then the case says so and measures nothing, rather than looping until he is.
	protected int m_iPass;

	protected SCR_ChimeraCharacter m_Character;
	protected IEntity m_VehicleEntity;

	//! Whether MoveInVehicle() accepted the request at all, as distinct from whether it landed.
	protected bool m_bSeatRequested;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPass == 0)
		{
			m_iPass = 1;

			string problem = Arrange();
			if (problem != "")
			{
				Cleanup();
				Print("[OVT_TEST] QRF mounted echelon, seated crew: " + problem + " - the T4.7 verdict rests on the code reading alone in this world");
				return true;
			}

			return false;
		}

		if (m_iPass == 1)
		{
			m_iPass = 2;

			string problem = Seat();
			if (problem != "")
			{
				Cleanup();
				Print("[OVT_TEST] QRF mounted echelon, seated crew: " + problem + " - nothing about a SEATED crew was measured");
				return true;
			}

			return false;
		}

		if (m_iPass == 2)
		{
			m_iPass = 3;

			return false;
		}

		return Judge();
	}

	//------------------------------------------------------------------------------------------------
	//! Second pass: ask for the driver's seat.
	//! \return An empty string when the request was accepted, or why it was not.
	protected string Seat()
	{
		if (!m_Character || !m_VehicleEntity)
			return "the fixture did not survive the first frame hop";

		Vehicle vehicle = Vehicle.Cast(m_VehicleEntity);
		if (!vehicle)
			return "the ladder's bottom rung is not a Vehicle";

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(m_Character.FindComponent(SCR_CompartmentAccessComponent));
		if (!access)
			return "the character has no compartment access component";

		m_bSeatRequested = access.MoveInVehicle(vehicle, ECompartmentType.PILOT);
		if (!m_bSeatRequested)
			return "the driver's seat was refused";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when the fixture stands, or why the world could not build it.
	protected string Arrange()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return "the occupying faction manager did not resolve";

		OVT_Faction faction = OVT_Global.GetConfig().GetOccupyingFaction();
		if (!faction)
			return "the occupying faction did not resolve";

		vector target;
		if (!OVT_TEST_EchelonFixture.ResolveTargetPosition(occupying, target))
			return "the world produced nowhere to hang a fixture";

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return "there is no world";

		faction.InitializeVehicleRegistry();

		OVT_FactionVehicleEntry entry;
		if (!faction.ResolveVehicleForRole("armed", 0, 1, -1, entry) || entry.m_sVehiclePrefab.IsEmpty())
			return "the occupying faction has no armed vehicle to seat anybody in";

		ResourceName characterPrefab = faction.GetCharacterPrefab("Officer");
		if (characterPrefab.IsEmpty())
			return "the occupying faction authors no named character to spawn";

		vector spot = target + Vector(FIXTURE_OFFSET_M, 0, FIXTURE_OFFSET_M);
		spot[1] = world.GetSurfaceY(spot[0], spot[2]);

		m_VehicleEntity = OVT_WorldUtils.SpawnEntityPrefab(entry.m_sVehiclePrefab, spot);
		if (!m_VehicleEntity)
			return "the armed vehicle could not be spawned";

		vector beside = spot + Vector(3, 0, 0);
		beside[1] = world.GetSurfaceY(beside[0], beside[2]);

		m_Character = OVT_WorldUtils.SpawnCharacterEntity(characterPrefab, beside);
		if (!m_Character)
			return "the character could not be spawned";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Second pass: seat the man, ask the four questions, put everything back.
	//! \return Always true.
	protected bool Judge()
	{
		if (!m_Character || !m_VehicleEntity)
		{
			Cleanup();
			Print("[OVT_TEST] QRF mounted echelon, seated crew: the fixture did not survive the frame hops");
			return true;
		}

		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(m_Character.FindComponent(CompartmentAccessComponent));
		if (!access || !access.IsInCompartment())
		{
			Cleanup();
			Print("[OVT_TEST] QRF mounted echelon, seated crew: the seat request was accepted (" + m_bSeatRequested.ToString() + ") but the man was not in the compartment two frames later, so nothing about a SEATED crew was measured");
			return true;
		}

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		vector target;
		if (!occupying || !OVT_TEST_EchelonFixture.ResolveTargetPosition(occupying, target))
		{
			Cleanup();
			Print("[OVT_TEST] QRF mounted echelon, seated crew: the fixture objective moved");
			return true;
		}

		OVT_QRFControllerComponent battle = occupying.SpawnQRFController(target);
		if (!battle)
		{
			Cleanup();
			SetFailure("The occupying faction manager could not spawn a battle controller");
			return true;
		}

		// --- The four readings, taken while the man is in the seat.
		string factionKey = m_Character.GetFactionKey();
		bool fightingFit = battle.IsFightingFit(m_Character);
		float seatedSeparation = vector.Distance(m_Character.GetOrigin(), m_VehicleEntity.GetOrigin());
		float scoringDistance = vector.Distance(m_Character.GetOrigin(), battle.GetOwner().GetOrigin());
		bool surfacedAsAgent = SurfacesAsAnAgent();

		string occupyingKey = OVT_Global.GetConfig().m_sOccupyingFaction;

		OVT_TEST_EchelonFixture.RetireBattle(battle);
		Cleanup();

		// --- ASSERT.
		if (factionKey != occupyingKey)
		{
			SetFailure("a mounted crew must register under the occupying faction key: read '%1', expected '%2'", factionKey, occupyingKey);
			return true;
		}

		if (!fightingFit)
		{
			SetFailure("🔴 A SEATED OCCUPANT IS EXCLUDED BY IsFightingFit - a mounted crew holds no ground and the whole echelon scores nothing. The fix belongs to the qrf feature, NOT to occupying/vehicles");
			return true;
		}

		if (seatedSeparation > 20)
		{
			SetFailure("🔴 A SEATED CHARACTER'S GetOrigin() IS NOT A WORLD POSITION: it reads %1 m from the vehicle it is sitting in, so every mounted crew measures as being somewhere else and no echelon ever holds ground", seatedSeparation.ToString());
			return true;
		}

		if (scoringDistance >= OVT_QRFControllerComponent.QRF_POINT_RANGE)
		{
			SetFailure("the fixture crew sat %1 m from the objective, outside the %2 m scoring ring, so the reading proves nothing about zone control", scoringDistance.ToString(), OVT_QRFControllerComponent.QRF_POINT_RANGE.ToString());
			return true;
		}

		if (!surfacedAsAgent)
		{
			SetFailure("🔴 A SEATED CHARACTER DOES NOT SURFACE IN GetAIAgents() - CheckUpdatePoints walks that list and nothing else, so a mounted crew is invisible to zone scoring. The fix belongs to the qrf feature, NOT to occupying/vehicles");
			return true;
		}

		Print("[OVT_TEST] QRF mounted echelon, seated crew: an occupying-faction man in a driver's seat is an agent, is fighting fit, carries the occupying key and measures " + scoringDistance.ToString() + " m from the objective - zone scoring counts him");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the fixture character is in the AI world's agent list with itself under it,
	//! which is the ONLY way CheckUpdatePoints can ever see anybody.
	protected bool SurfacesAsAnAgent()
	{
		AIWorld aiWorld = GetGame().GetAIWorld();
		if (!aiWorld)
			return false;

		array<AIAgent> agents = {};
		aiWorld.GetAIAgents(agents);

		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			if (agent.GetControlledEntity() == m_Character)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the man on the ground before either entity is deleted, so nothing is deleted twice through
	//! the parent-child link a compartment makes.
	protected void Cleanup()
	{
		if (m_Character)
		{
			CompartmentAccessComponent access = CompartmentAccessComponent.Cast(m_Character.FindComponent(CompartmentAccessComponent));
			if (access && access.IsInCompartment())
				access.GetOutVehicle(EGetOutType.TELEPORT, 0, ECloseDoorAfterActions.LEAVE_OPEN, true);

			SCR_EntityHelper.DeleteEntityAndChildren(m_Character);
			m_Character = null;
		}

		if (m_VehicleEntity)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_VehicleEntity);
			m_VehicleEntity = null;
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Shared fixture arithmetic for the cases above. Nothing here asserts; everything here either answers
//! or says it could not.
//------------------------------------------------------------------------------------------------
class OVT_TEST_EchelonFixture
{
	//! How far a source has to be from the fixture objective before the standoff geometry has a line to
	//! work with at all. LineEchelonAnchor refuses a source standing on the objective.
	static const float MIN_SOURCE_SEPARATION_M = 250;

	//------------------------------------------------------------------------------------------------
	//! A place in the world to hang a fixture battle on. Prefers a town, falls back to a base - the
	//! same order OVT_TEST_Init_QRFSiege.ResolveFixturePosition uses.
	//! \param[in] occupying The occupying faction manager.
	//! \param[out] position Where to put the fixture.
	//! \return True when the world offered somewhere.
	static bool ResolveTargetPosition(notnull OVT_OccupyingFactionManager occupying, out vector position)
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (towns && towns.m_Towns && !towns.m_Towns.IsEmpty())
		{
			position = towns.m_Towns[0].location;
			return true;
		}

		if (occupying.m_Bases && !occupying.m_Bases.IsEmpty())
		{
			position = occupying.m_Bases[0].location;
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A base an echelon could plausibly drive out of: clear of the objective, and with a road inside
	//! the same radius the controller's own gate uses.
	//!
	//! ⚠ IT DOES NOT LOOK AT landIsolated. The cases that use this SET that flag, in both directions;
	//! filtering on it here would hide the very input they are measuring.
	//! \param[in] occupying The occupying faction manager.
	//! \param[in] target The fixture objective.
	//! \return A base record, or null when the world offers none.
	static OVT_BaseData ResolveDrivableSource(notnull OVT_OccupyingFactionManager occupying, vector target)
	{
		if (!occupying.m_Bases)
			return null;

		foreach (OVT_BaseData base : occupying.m_Bases)
		{
			if (!base)
				continue;

			if (vector.Distance(base.location, target) < MIN_SOURCE_SEPARATION_M)
				continue;

			// ⚠ THE GATE UNDER TEST READS GetNearestBase(source), NOT THIS RECORD. Two records within
			// ECHELON_SOURCE_MATCH_M of each other would let a case flip a flag the controller never
			// looks at and then read the refusal as a pass - or, worse, as a false red. Only a base
			// that IS its own nearest base can be used to drive that gate.
			if (occupying.GetNearestBase(base.location) != base)
				continue;

			vector roadPosition;
			vector roadAngles;
			if (!OVT_WorldUtils.FindNearestRoadSpawn(base.location, OVT_QRFControllerComponent.ECHELON_ROAD_SEARCH_M, roadPosition, roadAngles))
				continue;

			return base;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] config A deployment config.
	//! \return Its mounted force module template, or null.
	static OVT_MountedForceSpawningDeploymentModule FindMountedTemplate(notnull OVT_DeploymentConfig config)
	{
		if (!config.m_aModules)
			return null;

		foreach (OVT_BaseDeploymentModule module : config.m_aModules)
		{
			OVT_MountedForceSpawningDeploymentModule mounted = OVT_MountedForceSpawningDeploymentModule.Cast(module);
			if (mounted)
				return mounted;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! What the ladder answers for the echelon config, asked through the same public resolver the
	//! module will use when it spawns, at the live threat and the live difficulty scale.
	//! \param[in] occupying The occupying faction manager, for the threat.
	//! \param[in] mounted The config's mounted module template, for the role and the ceiling.
	//! \param[out] cost The rung's cost.
	//! \return True when the ladder answered a rung.
	static bool ResolveExpectedRungCost(notnull OVT_OccupyingFactionManager occupying, notnull OVT_MountedForceSpawningDeploymentModule mounted, out int cost)
	{
		OVT_Faction faction = OVT_Global.GetConfig().GetOccupyingFaction();
		if (!faction)
			return false;

		faction.InitializeVehicleRegistry();

		float scale = 1;
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (difficulty)
			scale = difficulty.vehicleThresholdScale;

		OVT_FactionVehicleEntry entry;
		if (!faction.ResolveVehicleForRole(mounted.m_sVehicleRole, occupying.GetThreatFloat(), scale, mounted.m_iTruckCostOverride, entry))
			return false;

		cost = entry.m_iCost;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! MARKS EVERY ECHELON A FIXTURE STOOD UP AS ALREADY WIPED OUT, on the deployment AND on every one
	//! of its spawning modules. The tier's standing rule, and it must run BEFORE the teardown that
	//! empties the list - after it there is nothing left to mark.
	//!
	//! None of these has ever ticked: a deployment's first convergence is a whole 8-12 s update
	//! interval after it is created and no test step spans one. The autotest camera is an observer all
	//! the same, and this costs nothing.
	//! \param[in] battle The fixture battle.
	static void MarkEchelonsEliminated(notnull OVT_QRFControllerComponent battle)
	{
		int live = battle.GetLiveEchelonCount();
		for (int i = 0; i < live; i++)
		{
			OVT_DeploymentComponent echelon = battle.GetEchelon(i);
			if (!echelon)
				continue;

			echelon.SetSpawnedUnitsEliminated(true);

			array<OVT_BaseSpawningDeploymentModule> spawningModules = echelon.GetSpawningModules();
			foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
			{
				if (spawningModule)
					spawningModule.SetSpawnedUnitsEliminated(true);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Takes a fixture battle down the way the campaign does, and everything it stood up with it.
	//!
	//! ⚠ THE SPAWN QUEUE IS EMPTIED FIRST. A case that drove Start() has a queue of real group prefabs
	//! on the controller; the 1 000 ms timer that pops them cannot fire inside one test step, and
	//! OnDelete removes it, but leaving the queue full is one more thing that has to stay true.
	//! ⚠ MARKS EVERY ECHELON'S SPAWNING MODULES ELIMINATED BEFORE TEARING THEM DOWN. None of them has
	//! ever ticked - activation is a whole update interval after creation - but the autotest camera is
	//! an observer and this is the tier's standing rule.
	//! \param[in] battle The fixture battle.
	static void RetireBattle(OVT_QRFControllerComponent battle)
	{
		if (!battle)
			return;

		MarkEchelonsEliminated(battle);

		battle.TearDownEchelons();

		if (battle.m_aSpawnQueue) battle.m_aSpawnQueue.Clear();
		if (battle.m_aSpawnPositions) battle.m_aSpawnPositions.Clear();
		if (battle.m_aSpawnTargets) battle.m_aSpawnTargets.Clear();
		if (battle.m_aSpawnRingSlots) battle.m_aSpawnRingSlots.Clear();

		SCR_EntityHelper.DeleteEntityAndChildren(battle.GetOwner());
	}
}
