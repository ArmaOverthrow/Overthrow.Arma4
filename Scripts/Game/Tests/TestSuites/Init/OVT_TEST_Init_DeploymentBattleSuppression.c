//------------------------------------------------------------------------------------------------
//! TIER B case - the WIRING between a live battle and the deployment framework's suppression rule.
//!
//! WHAT IS PINNED HERE AND WHAT IS NOT. The rule itself - engaged, occupying faction, inside the ring -
//! is pure and is pinned in OVT_TEST_Logic_DeploymentBattleSuppression.c. What cannot be pinned there
//! is that OVT_DeploymentManagerComponent.IsBattleSuppressedAt() asks the RIGHT QUESTIONS OF THE RIGHT
//! OBJECTS: that it reads engagement off the battle controller currently in the occupying faction
//! manager's slot rather than off the mere existence of one, that it measures against that manager's
//! own m_vQRFLocation, and that it converts a deployment's faction INDEX to the config's occupying
//! faction correctly. Every one of those is a wiring mistake a pure case cannot see.
//!
//! 🔴 THE CLAIM THIS CASE EXISTS FOR IS THE SILENT ONE. A counter-attack siege is a battle object for
//! up to 31 minutes before a shot is fired, and if suppression started when the OBJECT appeared rather
//! than when the SHOOTING did, the objective town would empty of its garrison while the siege was still
//! supposed to be a secret - the exact tell the silent stages exist to avoid. Claims 3 and 4 below are
//! the same siege at two stages and are the whole reason this case is Tier B rather than Tier A.
//!
//! FIXTURE DISCIPLINE, copied deliberately from OVT_TEST_Init_QRFSiege.c because it is proven:
//!   - the battle controller is spawned through the occupying faction manager's own
//!     SpawnQRFController and disposed of with DeleteEntityAndChildren, which is exactly how the
//!     campaign disposes of a finished battle;
//!   - Start() is NEVER called, so no troops are queued and no resources are spent;
//!   - both objective indices are forced to -1, so no transition can announce a place this fixture is
//!     not really besieging or suppress a town's civilians;
//!   - every manager field this case writes is read into a local BEFORE anything is spawned and
//!     restored BEFORE the first assertion, so a red leaves the world as it found it.
//!
//! ⚠ NOTHING IN THIS CASE TOUCHES A DEPLOYMENT, A GROUP OR THE SUPPRESSION LIST. It asks one read-only
//! predicate about hand-supplied positions, so a live deployment wave running in this world cannot
//! influence it and it cannot influence the wave. The manager's 1 s reconciliation tick is installed in
//! PostGameStart(), which an Init-tier world never reaches, so it is not running here either - and
//! could not interleave anyway, because the whole pass is one frame.
//!
//! ⚠ THE RADIUS IS READ FROM OVT_QRFControllerComponent.QRF_RANGE, never written as a number, and the
//! boundary rows sit a metre either side of it: vector.Distance is not correctly rounded (measured a
//! full ULP high at 1 000 m), so a row planted exactly ON the ring would be asserting a rounding mode.
//!
//! CAN-FAIL PROOF. Running a suite is the orchestrator's job, not an implementation agent's
//! (.claude/test-policy.md), so the proof below is a fault injected into the subject and compiled: it
//! exited tools/compile-check.sh with 0, which is the point - it is not a script error and nothing else
//! in the tree would stop it shipping. The subject was restored and recompiled clean afterwards.
//!
//! No maxAttempts anywhere.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! IsBattleSuppressedAt() follows the SHOOTING, the FACTION and the RING - not the battle's existence.
//!
//! Six claims, in the order the machine reaches them:
//!   1. NO BATTLE AT ALL SUPPRESSES NOTHING, at the coordinates a battle would be at.
//!   2. A STANDARD, PLAYER-INITIATED BATTLE SUPPRESSES THE OCCUPYING FACTION AT ITS OWN LOCATION and
//!      one metre inside the ring. A standard battle is engaged from creation, so this is also the
//!      regression guard saying the everyday case still works.
//!   3. ...AND SUPPRESSES NOTHING one metre outside the ring. Local, never map-wide.
//!   4. THE RESISTANCE IS NEVER SUPPRESSED by that same standard battle, standing on top of it.
//!   5. 🔴 A COUNTER-ATTACK IN SILENT_DEPLOY SUPPRESSES NOTHING, at the battle's own coordinates, for
//!      the occupying faction's own force. The object exists; the shooting has not started.
//!   6. THE SAME SIEGE, DRIVEN TO BATTLE, SUPPRESSES THE SAME POSITION. Claims 5 and 6 differ in
//!      nothing but the stage, which is what proves the gate is engagement and not mode.
//!
//! CAN-FAIL, one fault:
//!   B1. ASK THE WRONG QUESTION - IsBattleSuppressedAt()'s engagement argument changed from
//!       `occupying.IsQRFEngaged()` to `occupying.m_bQRFActive`, i.e. "a battle exists" instead of "the
//!       shooting has started", which is precisely the mistake this whole design turns on. Compiled
//!       clean (exit 0): both are public bools on the same manager. Claims 1-4 all still pass, because
//!       a standard battle sets both, and the case fails on claim 5 with "a counter-attack in
//!       SILENT_DEPLOY must suppress NOTHING - the world keeps living while the encirclement forms, or
//!       the empty objective is the tell". Restored and recompiled clean.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_DeploymentBattleSuppression_SuppressionFollowsTheShootingNotTheBattlesExistence : SCR_AutotestCaseBase
{
	//! Planted remaining time, in ms. One tick of the machine takes it to zero, which is the
	//! MUSTER -> BATTLE transition. Deliberately not a value the machine itself ever produces.
	static const int PLANTED_LAST_SECOND = 1000;

	//! How far either side of the ring the boundary rows sit. Far larger than any floating-point error
	//! in the distance, far smaller than anything a player could perceive.
	static const float BOUNDARY_MARGIN_M = 1;

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

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("The Overthrow config component did not resolve");
			return true;
		}

		int occupyingIndex = config.GetOccupyingFactionIndex();
		int resistanceIndex = config.GetPlayerFactionIndex();

		if (occupyingIndex < 0 || resistanceIndex < 0)
		{
			SetFailure("The world did not resolve both faction indices: occupying %1, resistance %2",
				occupyingIndex.ToString(), resistanceIndex.ToString());
			return true;
		}

		// Without two DIFFERENT indices the faction half of the rule is untestable, and a case that
		// quietly asserted nothing would be worse than one that says so.
		if (occupyingIndex == resistanceIndex)
		{
			SetFailure("The occupying and resistance factions resolve to the same index (%1), so this case cannot tell them apart",
				occupyingIndex.ToString());
			return true;
		}

		vector fixturePosition;
		if (!ResolveFixturePosition(occupying, fixturePosition))
		{
			SetFailure("The world produced neither a town nor a base to hang a fixture battle on");
			return true;
		}

		float ring = OVT_QRFControllerComponent.QRF_RANGE;
		vector justInside = EastOf(fixturePosition, ring - BOUNDARY_MARGIN_M);
		vector justOutside = EastOf(fixturePosition, ring + BOUNDARY_MARGIN_M);

		// --- SAVE every manager field this case can touch, before anything is spawned.
		OVT_QRFControllerComponent savedBattle = occupying.m_CurrentQRF;
		bool savedRevealed = occupying.m_bQRFRevealed;
		int savedTimer = occupying.m_iQRFTimer;
		int savedTown = occupying.m_iCurrentQRFTown;
		int savedBase = occupying.m_iCurrentQRFBase;
		vector savedLocation = occupying.m_vQRFLocation;

		// Both objective indices to -1 for the same reason OVT_TEST_Init_QRFSiege.c forces them: at -1
		// no transition publishes a civilian suppression or names a place this fixture is not besieging.
		occupying.m_iCurrentQRFTown = -1;
		occupying.m_iCurrentQRFBase = -1;

		// --- CLAIM 1: no battle at all.
		occupying.m_CurrentQRF = null;
		occupying.m_vQRFLocation = fixturePosition;
		bool suppressedWithNoBattle = deployments.IsBattleSuppressedAt(fixturePosition, occupyingIndex);

		// --- CLAIMS 2-4: a standard, player-initiated battle. Engaged from creation.
		OVT_QRFControllerComponent standard = occupying.SpawnQRFController(fixturePosition);
		if (!standard)
		{
			occupying.m_CurrentQRF = savedBattle;
			occupying.m_vQRFLocation = savedLocation;
			occupying.m_iCurrentQRFTown = savedTown;
			occupying.m_iCurrentQRFBase = savedBase;
			SetFailure("The occupying faction manager could not spawn a battle controller");
			return true;
		}

		occupying.m_CurrentQRF = standard;

		bool suppressedOnStandard = deployments.IsBattleSuppressedAt(fixturePosition, occupyingIndex);
		bool suppressedJustInside = deployments.IsBattleSuppressedAt(justInside, occupyingIndex);
		bool suppressedJustOutside = deployments.IsBattleSuppressedAt(justOutside, occupyingIndex);
		bool suppressedResistance = deployments.IsBattleSuppressedAt(fixturePosition, resistanceIndex);

		occupying.m_CurrentQRF = null;
		SCR_EntityHelper.DeleteEntityAndChildren(standard.GetOwner());

		// --- CLAIMS 5-6: a counter-attack siege, silent and then shooting.
		OVT_QRFControllerComponent siege = occupying.SpawnQRFController(fixturePosition);
		if (!siege)
		{
			occupying.m_CurrentQRF = savedBattle;
			occupying.m_vQRFLocation = savedLocation;
			occupying.m_bQRFRevealed = savedRevealed;
			occupying.m_iCurrentQRFTown = savedTown;
			occupying.m_iCurrentQRFBase = savedBase;
			SetFailure("The occupying faction manager could not spawn a second battle controller");
			return true;
		}

		// ⚠ THE MODE IS SET BEFORE Start() WOULD BE CALLED, which is the one order this component
		// supports. Start() is deliberately never called: the fixture wants the stage machine, not a
		// spawn pass.
		siege.m_eMode = OVT_EQRFMode.COUNTER_ATTACK;
		occupying.m_CurrentQRF = siege;
		occupying.m_bQRFRevealed = false;

		OVT_EQRFStage stageWhenSilent = siege.GetStage();
		bool suppressedWhenSilent = deployments.IsBattleSuppressedAt(fixturePosition, occupyingIndex);

		// One tick on an empty spawn queue completes the encirclement (SILENT_DEPLOY -> MUSTER); the
		// tick that runs the muster clock out starts the assault (MUSTER -> BATTLE).
		siege.CheckUpdateTimer();
		siege.m_iTimer = PLANTED_LAST_SECOND;
		siege.CheckUpdateTimer();

		OVT_EQRFStage stageWhenShooting = siege.GetStage();
		bool suppressedWhenShooting = deployments.IsBattleSuppressedAt(fixturePosition, occupyingIndex);

		// --- RESTORE before asserting, so a red case leaves the world as it found it.
		occupying.m_CurrentQRF = savedBattle;
		occupying.m_vQRFLocation = savedLocation;
		occupying.m_bQRFRevealed = savedRevealed;
		occupying.m_iQRFTimer = savedTimer;
		occupying.m_iCurrentQRFTown = savedTown;
		occupying.m_iCurrentQRFBase = savedBase;
		SCR_EntityHelper.DeleteEntityAndChildren(siege.GetOwner());

		// --- ASSERT.
		if (suppressedWithNoBattle)
		{
			SetFailure("with no battle at all nothing may be suppressed anywhere");
			return true;
		}

		if (!suppressedOnStandard)
		{
			SetFailure("a standard, player-initiated battle must suppress the occupying faction at its own location - this is the everyday case the play-test found broken");
			return true;
		}

		if (!suppressedJustInside)
		{
			SetFailure("a position one metre inside the %1 m ring must be suppressed", ring.ToString());
			return true;
		}

		if (suppressedJustOutside)
		{
			SetFailure("a position one metre outside the %1 m ring must NOT be suppressed - suppression is local to the battle, never map-wide", ring.ToString());
			return true;
		}

		if (suppressedResistance)
		{
			SetFailure("the resistance (faction index %1) must never be suppressed, even standing on the battle", resistanceIndex.ToString());
			return true;
		}

		if (stageWhenSilent != OVT_EQRFStage.SILENT_DEPLOY)
		{
			SetFailure("the fixture siege was expected in SILENT_DEPLOY: read back stage %1", stageWhenSilent.ToString());
			return true;
		}

		if (suppressedWhenSilent)
		{
			SetFailure("a counter-attack in SILENT_DEPLOY must suppress NOTHING - the world keeps living while the encirclement forms, or the empty objective is the tell");
			return true;
		}

		if (stageWhenShooting != OVT_EQRFStage.BATTLE)
		{
			SetFailure("two driven ticks were expected to reach BATTLE: read back stage %1", stageWhenShooting.ToString());
			return true;
		}

		if (!suppressedWhenShooting)
		{
			SetFailure("the same siege, now in BATTLE at the same position, must suppress - the gate is the shooting, not the mode");
			return true;
		}

		Print("DeploymentBattleSuppression: no battle suppresses nothing; a standard battle suppresses the occupying faction inside its ring only and never the resistance; a siege suppresses nothing until it is actually being fought");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A place in the world to hang a fixture battle on. Prefers a town, falls back to a base.
	//! \param[in] occupying The occupying faction manager.
	//! \param[out] position Where to put the fixture.
	//! \return True when the world offered somewhere.
	protected bool ResolveFixturePosition(notnull OVT_OccupyingFactionManager occupying, out vector position)
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
	//! A point `metres` due east of `origin`, at the same altitude. Axis-aligned so the offset IS the
	//! distance and a failure message quotes a number the reader can check against the ring by eye.
	//! \param[in] origin The point to offset from.
	//! \param[in] metres How far east.
	//! \return The offset position.
	protected vector EastOf(vector origin, float metres)
	{
		return Vector(origin[0] + metres, origin[1], origin[2]);
	}
}

//------------------------------------------------------------------------------------------------
//! 🔴 THE RIDING RING IS WHAT EXEMPTS A MOUNTED FORCE FROM BATTLE SUPPRESSION, AND NOTHING ELSE IS.
//!
//! ==========================================================================================
//! WHY THIS CASE EXISTS: THERE IS NO WHITELIST, SO THERE IS NOTHING ELSE TO PROTECT (D8).
//! ==========================================================================================
//! SuppressForcesAroundBattle() pins every not-yet-materialised occupying-faction group inside the
//! battle circle, and skips a handle for exactly one reason that concerns a mounted force: its resolved
//! spawn distance is STRICTLY WIDER than the world's global ring (OVT_DeploymentManager.c:567, written
//! for *"the insertion module's riding passengers and crew, which exist precisely so they can be seated
//! in a truck driving through empty country. Pinning those arrives the convoy empty"*).
//!
//! Riding crews and riding passengers register at RIDING_SPAWN_DISTANCE, 100 km, so they are already
//! exempt with no list, no flag and no per-module opt-in anywhere. THE WHOLE PROTECTION IS THEREFORE A
//! PROHIBITION: a mounted force must not be put back on the ordinary proximity ring while it is aboard.
//! One line doing so - at arrival, in a convergence sweep, in a future refactor of either - and the
//! QRF's mounted echelon is pinned dormant the moment it reaches the battle it was sent to, materialises
//! nobody, and delivers an empty vehicle. Nothing warns. Nothing logs. The deployment still exists and
//! is still paid for.
//!
//! ⚠ WHAT THIS CASE ASSERTS, STATED HONESTLY. SuppressForcesAroundBattle() and the 1 Hz tick that calls
//! it are protected, and that tick is installed in PostGameStart() which an Init-tier world never
//! reaches - so nothing here can run the pass. What it CAN do is establish every input the pass reads,
//! against live objects, and quote the manager's own skip condition back at them:
//!   1. The rule DOES reach this place and this faction - IsBattleSuppressedAt() answers true, so
//!      neither group is being spared by distance, by faction or by there being no battle.
//!   2. An ORDINARY deployment group at that spot resolves to a ring that is NOT strictly wider than the
//!      global one, i.e. it is a suppression candidate. Without this row the case would pass in a world
//!      where the skip condition matched everything.
//!   3. A group registered at RIDING_SPAWN_DISTANCE resolves to a ring that IS strictly wider, i.e. the
//!      pass skips it.
//! Rows 2 and 3 differ in one argument to RegisterGroup and give opposite answers, which is the guard:
//! anything that makes a riding registration resolve to the global ring turns row 3 red.
//!
//! FIXTURE DISCIPLINE, the same as the case above: the battle controller is spawned through the
//! occupying faction manager's own SpawnQRFController and disposed of with DeleteEntityAndChildren,
//! Start() is never called, both objective indices are forced to -1, every manager field is saved before
//! anything is spawned and restored before the first assertion, and both groups are unregistered before
//! the first assertion too.
//!
//! ⚠ BOTH GROUPS ARE REGISTERED AND UNREGISTERED INSIDE ONE SYNCHRONOUS FRAME. A group on a 100 km ring
//! is permanently inside its own ring and the autotest camera is an observer, so one left behind would
//! materialise men into the shared initialisation world for every case that runs after it. The engine's
//! own 1 Hz lifecycle tick cannot run between the register and the unregister.
//!
//! CAN-FAIL PROOF. Running a suite is the orchestrator's job (.claude/test-policy.md), so the proof is a
//! fault injected into the subject and compiled: the riding registration's spawnDistanceOverride
//! argument changed from OVT_InsertionSpawningDeploymentModule.RIDING_SPAWN_DISTANCE to
//! OVT_BaseSpawningDeploymentModule.SPAWN_DISTANCE_GLOBAL - which is precisely what a stray restore of
//! the ordinary ring does to a live mounted force. It exited tools/compile-check.sh 0 (both are public
//! int constants) and the case fails on row 3, "a mounted force's riding registration must resolve to a
//! ring STRICTLY WIDER than the world's global one". Restored and recompiled clean.
//!
//! No maxAttempts, no polling, no assertion about live AI.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_DeploymentBattleSuppression_TheRidingRingExemptsAMountedForce : SCR_AutotestCaseBase
{
	//! Owner system tag for this case's two registrations, so a leaked record is obvious in a log.
	static const string FIXTURE_OWNER_SYSTEM = "test_battle_suppression_riding";

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

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("The virtualization core did not resolve");
			return true;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("The Overthrow config component did not resolve");
			return true;
		}

		int occupyingIndex = config.GetOccupyingFactionIndex();
		string occupyingKey = config.m_sOccupyingFaction;
		if (occupyingIndex < 0 || occupyingKey == "")
		{
			SetFailure("The world did not resolve an occupying faction: index %1, key '%2'",
				occupyingIndex.ToString(), occupyingKey);
			return true;
		}

		string groupName = ResolveOccupyingGroupName(occupyingKey);
		if (groupName == "")
		{
			SetFailure("The occupying faction '%1' offers no group this case can register, so neither ring can be measured",
				occupyingKey);
			return true;
		}

		vector fixturePosition;
		if (!ResolveFixturePosition(occupying, fixturePosition))
		{
			SetFailure("The world produced neither a town nor a base to hang a fixture battle on");
			return true;
		}

		// --- SAVE every manager field this case can touch, before anything is spawned.
		OVT_QRFControllerComponent savedBattle = occupying.m_CurrentQRF;
		int savedTown = occupying.m_iCurrentQRFTown;
		int savedBase = occupying.m_iCurrentQRFBase;
		vector savedLocation = occupying.m_vQRFLocation;

		occupying.m_iCurrentQRFTown = -1;
		occupying.m_iCurrentQRFBase = -1;
		occupying.m_vQRFLocation = fixturePosition;

		OVT_QRFControllerComponent battle = occupying.SpawnQRFController(fixturePosition);
		if (!battle)
		{
			occupying.m_CurrentQRF = savedBattle;
			occupying.m_vQRFLocation = savedLocation;
			occupying.m_iCurrentQRFTown = savedTown;
			occupying.m_iCurrentQRFBase = savedBase;
			SetFailure("The occupying faction manager could not spawn a battle controller");
			return true;
		}

		occupying.m_CurrentQRF = battle;

		// --- Two occupying-faction groups standing in the same battle, differing only in their ring.
		int garrisonHandle = virtualization.RegisterGroup(FIXTURE_OWNER_SYSTEM, "suppression_garrison",
			occupyingKey, groupName, fixturePosition, null,
			OVT_BaseSpawningDeploymentModule.SPAWN_DISTANCE_GLOBAL);

		int ridingHandle = virtualization.RegisterGroup(FIXTURE_OWNER_SYSTEM, "suppression_riding",
			occupyingKey, groupName, fixturePosition, null,
			OVT_InsertionSpawningDeploymentModule.RIDING_SPAWN_DISTANCE);

		bool ruleReachesThisPlace = deployments.IsBattleSuppressedAt(fixturePosition, occupyingIndex);
		int globalRing = virtualization.GetGlobalSpawnDistance();
		int garrisonRing = virtualization.GetSpawnDistance(garrisonHandle);
		int ridingRing = virtualization.GetSpawnDistance(ridingHandle);

		// --- RESTORE before asserting, so a red case leaves the world as it found it.
		virtualization.UnregisterGroup(garrisonHandle);
		virtualization.UnregisterGroup(ridingHandle);

		occupying.m_CurrentQRF = savedBattle;
		occupying.m_vQRFLocation = savedLocation;
		occupying.m_iCurrentQRFTown = savedTown;
		occupying.m_iCurrentQRFBase = savedBase;
		SCR_EntityHelper.DeleteEntityAndChildren(battle.GetOwner());

		// --- ASSERT.
		if (garrisonHandle == -1 || ridingHandle == -1)
		{
			SetFailure("This world could not register the two fixture groups (handles %1 and %2), so neither ring could be measured",
				garrisonHandle.ToString(), ridingHandle.ToString());
			return true;
		}

		if (!ruleReachesThisPlace)
		{
			SetFailure("the suppression rule must reach the fixture position for the occupying faction - if it does not, this case proves nothing about the exemption, because nothing would have been pinned anyway");
			return true;
		}

		// The manager's own skip condition, quoted: `spawnDistance <= 0 || spawnDistance > globalRing`.
		if (garrisonRing <= 0 || garrisonRing > globalRing)
		{
			SetFailure("an ORDINARY deployment group must be a suppression candidate: read back %1 m against a global ring of %2 m, which the manager's own skip condition would let through",
				garrisonRing.ToString(), globalRing.ToString());
			return true;
		}

		if (ridingRing <= globalRing)
		{
			SetFailure("a mounted force's riding registration must resolve to a ring STRICTLY WIDER than the world's global one: read back %1 m against %2 m. At or below it, the battle-suppression pass pins the force dormant inside the battle it was sent to, and the echelon arrives with nobody in it",
				ridingRing.ToString(), globalRing.ToString());
			return true;
		}

		Print("DeploymentBattleSuppression: an ordinary deployment group in a battle is a suppression candidate, and a group registered at the riding ring is exempt by the manager's own strictly-wider rule - which is the only thing exempting a mounted force");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A group registry name on the occupying faction that resolves to a real prefab in this world.
	//! \param[in] factionKey The occupying faction's key.
	//! \return The name, or an empty string when that faction offers none.
	protected string ResolveOccupyingGroupName(string factionKey)
	{
		OVT_OverthrowFactionManager factions = OVT_Global.GetFactions();
		if (!factions)
			return "";

		OVT_Faction faction = factions.GetOverthrowFactionByKey(factionKey);
		if (!faction)
			return "";

		array<string> names = faction.GetAvailableGroupNames();
		if (!names)
			return "";

		foreach (string name : names)
		{
			if (faction.GetGroupPrefabByName(name) != ResourceName.Empty)
				return name;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! A place in the world to hang a fixture battle on. Prefers a town, falls back to a base.
	//! \param[in] occupying The occupying faction manager.
	//! \param[out] position Where to put the fixture.
	//! \return True when the world offered somewhere.
	protected bool ResolveFixturePosition(notnull OVT_OccupyingFactionManager occupying, out vector position)
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
}
