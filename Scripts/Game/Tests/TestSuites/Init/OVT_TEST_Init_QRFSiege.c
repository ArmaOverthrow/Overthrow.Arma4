//------------------------------------------------------------------------------------------------
//! TIER B cases - the counter-attack siege's STAGE MACHINE, on a real battle controller.
//!
//! WHAT NEEDS LIVE MANAGERS AND WHAT DOES NOT. The siege's arithmetic - ring bearings, the publish
//! cadence, the minutes form, and both halves of the neutralised test - is pure and is pinned in
//! OVT_TEST_Logic_QRFSiege.c. What cannot be pinned there is the WIRING: that a battle controller
//! spawned in COUNTER_ATTACK mode really starts un-engaged, that a driven tick really advances the
//! stage and really arms a thirty-minute clock, that the reveal really reaches the occupying faction
//! manager, and that the early-end check really refuses to fire on a live group with no agents in it.
//!
//! ==================================================================================================
//! 🔴 WHY THESE CASES EXIST AT ALL: THE STANDARD BATTLE IS THE THING THAT MUST NOT BREAK.
//! ==================================================================================================
//! occupying/counter-attacks Phase 9 put a second mode on the ONE component every battle in the game
//! runs through - a player capturing a base, a player starting an uprising, and now the occupying
//! faction taking a place back. The player-initiated path is the most-played event in the campaign and
//! the one nobody exercises while this feature is being built, so the first claim of the first case
//! below is deliberately the boring one: A STANDARD CONTROLLER IS ENGAGED THE MOMENT IT EXISTS. Every
//! world-suppression gate in the game hangs off that answer, and if it ever came back false a player's
//! battle would silently stop freezing the economy, the deployments and the town's civilians.
//!
//! ==================================================================================================
//! 🔴 THE SECOND CASE HERE CAUGHT A REAL D16 DEFECT ON ITS FIRST RUN. READ ITS HEADER.
//! ==================================================================================================
//! CheckSiegeWipedOut was reading "FindEntityByID gave me nothing" as "the group is dead". That is the
//! same class of unreliable reading as "zero agents", and it fired the early end against a force that
//! had not been fought. The fix is the m_bSiegeForceSeenAlive latch on the controller; the THIRD case
//! below is what covers it, because the second case's own fixture arms the latch and therefore cannot.
//!
//! FIXTURE DISCIPLINE. All three cases spawn a real battle controller through the occupying faction
//! manager's own SpawnQRFController and delete it with DeleteEntityAndChildren, which is exactly how
//! the campaign disposes of a finished battle (OnQRFFinishedBase/OnQRFFinishedTown), so the teardown
//! is a proven-safe one. Every manager field any case writes - the battle handle, both battle flags,
//! the published timer and points, and both objective indices - is read back into a local BEFORE
//! anything is asserted and restored BEFORE the case can leave through a failure.
//!
//! ⚠ NO CASE CALLS Start(), so no case spawns troops: the spawn queue stays empty and the stage machine
//! is driven by hand. The groups the second and third cases need are spawned from the occupying
//! faction's own group prefab under SCR_AIGroup.IgnoreSpawning(true) - the engine's "create the group,
//! create no members" seam - through the SAME SpawnEntityPrefab call SpawnFromQueue uses. Building one
//! any other way is what produced the first cut's false red.
//!
//! ⚠ THE SECOND AND THIRD CASES TAKE ONE FRAME HOP between creating their group and using its id,
//! because an entity that is not world-registered yet answers GetID() with a value every unregistered
//! entity shares (OVT_InactiveRecruitGroupComponent.c:76-83). It is a single unconditional pass, not a
//! poll and not a retry: there is no attempt counter and the second pass asserts unconditionally.
//!
//! ⚠ NO CALLQUEUE TICK CAN INTERLEAVE WITH A JUDGEMENT. Each judging pass runs inside a single frame,
//! and the controller's own 1 000 ms and 10 000 ms repeating calls cannot fire inside one - and every
//! controller here is created and destroyed within that one pass. The cases own the clock completely.
//!
//! No maxAttempts anywhere.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Engagement and the reveal follow the MODE and the STAGE, and a standard battle is engaged at once.
//!
//! Five claims, in the order the machine reaches them:
//!   1. 🔴 A STANDARD CONTROLLER IS ENGAGED THE MOMENT IT IS SPAWNED, and so is the manager's
//!      IsQRFEngaged() with one in the slot. This is the regression guard for every player-initiated
//!      battle in the game.
//!   2. NO BATTLE AT ALL IS NOT ENGAGED - the accessor must not answer true off a stale controller.
//!   3. A COUNTER_ATTACK CONTROLLER STARTS IN SILENT_DEPLOY AND IS **NOT** ENGAGED. This is the whole
//!      mechanic: the world goes on living while the encirclement forms.
//!   4. ONE DRIVEN TICK ON AN EMPTY SPAWN QUEUE ADVANCES SILENT_DEPLOY -> MUSTER, arms the clock to
//!      exactly OVT_QRFSiege.MUSTER_TIME_MS, tells the manager (m_bQRFRevealed), and is STILL NOT
//!      ENGAGED - being told is not the same as being shot at.
//!   5. THE TICK THAT RUNS THE CLOCK OUT ADVANCES MUSTER -> BATTLE, zeroes the clock, and IS engaged.
//!
//! ⚠ THE MUSTER CLOCK IS ASSERTED AGAINST THE CONSTANT, not against 1 800 000. A case that hard-codes
//! the number would go green against a constant somebody had halved.
//!
//! PROVEN ABLE TO FAIL: IsEngaged() was changed to `return m_eStage == OVT_EQRFStage.BATTLE;` - i.e.
//! the mode short-circuit that keeps STANDARD on today's behaviour was deleted. The tree recompiled
//! CLEAN (exit 0), because a missing short-circuit is not a script error and nothing else in the tree
//! would stop it shipping, and the case then reports "a STANDARD battle must be engaged the moment it
//! is spawned - every world-suppression gate in the game reads this". Short-circuit restored, tree
//! recompiled clean.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_QRFSiege_EngagementAndRevealFollowTheModeAndStage : SCR_AutotestCaseBase
{
	//! Planted remaining time, in ms. One tick of the machine takes it to zero, which is the MUSTER ->
	//! BATTLE transition. Deliberately not a value the machine itself ever produces.
	static const int PLANTED_LAST_SECOND = 1000;

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

		vector fixturePosition;
		if (!ResolveFixturePosition(occupying, fixturePosition))
		{
			SetFailure("The world produced neither a town nor a base to hang a fixture battle on");
			return true;
		}

		// --- SAVE every manager field this case can touch, before anything is spawned.
		OVT_QRFControllerComponent savedBattle = occupying.m_CurrentQRF;
		bool savedRevealed = occupying.m_bQRFRevealed;
		int savedTimer = occupying.m_iQRFTimer;
		int savedTown = occupying.m_iCurrentQRFTown;
		int savedBase = occupying.m_iCurrentQRFBase;

		// BOTH OBJECTIVE INDICES ARE FORCED TO -1, and both for the same reason: they are what the
		// manager reads to decide WHICH place a battle is for. At -1 the BATTLE transition publishes no
		// civilian suppression for a town that is not under attack, and the MUSTER transition sends no
		// counter-attack notification naming a place this fixture is not really besieging. It is what
		// an unstarted campaign holds anyway; forced so the case does not depend on that.
		occupying.m_iCurrentQRFTown = -1;
		occupying.m_iCurrentQRFBase = -1;

		// --- CLAIM 1: a standard battle, engaged at once.
		OVT_QRFControllerComponent standard = occupying.SpawnQRFController(fixturePosition);
		if (!standard)
		{
			occupying.m_iCurrentQRFTown = savedTown;
			occupying.m_iCurrentQRFBase = savedBase;
			SetFailure("The occupying faction manager could not spawn a battle controller");
			return true;
		}

		bool standardEngaged = standard.IsEngaged();

		occupying.m_CurrentQRF = standard;
		bool managerEngagedForStandard = occupying.IsQRFEngaged();

		occupying.m_CurrentQRF = null;
		SCR_EntityHelper.DeleteEntityAndChildren(standard.GetOwner());

		// --- CLAIM 2: no battle at all.
		bool managerEngagedWithNoBattle = occupying.IsQRFEngaged();

		// --- CLAIM 3: a siege, not engaged, in SILENT_DEPLOY.
		OVT_QRFControllerComponent siege = occupying.SpawnQRFController(fixturePosition);
		if (!siege)
		{
			occupying.m_CurrentQRF = savedBattle;
			occupying.m_iCurrentQRFTown = savedTown;
			occupying.m_iCurrentQRFBase = savedBase;
			SetFailure("The occupying faction manager could not spawn a second battle controller");
			return true;
		}

		// ⚠ BEFORE Start() WOULD BE CALLED, which is the order the manager's own starters use and the
		// only order this component supports. Start() is deliberately never called here: the fixture
		// wants the stage machine, not a spawn pass.
		siege.m_eMode = OVT_EQRFMode.COUNTER_ATTACK;
		occupying.m_CurrentQRF = siege;
		occupying.m_bQRFRevealed = false;

		OVT_EQRFStage stageAtBirth = siege.GetStage();
		bool siegeEngagedAtBirth = siege.IsEngaged();
		bool managerEngagedAtBirth = occupying.IsQRFEngaged();

		// --- CLAIM 4: one tick on an empty queue completes the encirclement.
		siege.CheckUpdateTimer();

		OVT_EQRFStage stageAfterDeploy = siege.GetStage();
		int clockAfterDeploy = siege.m_iTimer;
		bool siegeEngagedInMuster = siege.IsEngaged();
		bool managerEngagedInMuster = occupying.IsQRFEngaged();
		bool revealedInMuster = occupying.m_bQRFRevealed;

		// --- CLAIM 5: the tick that runs the clock out starts the assault.
		siege.m_iTimer = PLANTED_LAST_SECOND;
		siege.CheckUpdateTimer();

		OVT_EQRFStage stageAfterMuster = siege.GetStage();
		int clockAfterMuster = siege.m_iTimer;
		bool siegeEngagedInBattle = siege.IsEngaged();
		bool managerEngagedInBattle = occupying.IsQRFEngaged();

		// --- RESTORE before asserting, so a red case leaves the world as it found it.
		occupying.m_CurrentQRF = savedBattle;
		occupying.m_bQRFRevealed = savedRevealed;
		occupying.m_iQRFTimer = savedTimer;
		occupying.m_iCurrentQRFTown = savedTown;
		occupying.m_iCurrentQRFBase = savedBase;
		SCR_EntityHelper.DeleteEntityAndChildren(siege.GetOwner());

		// --- ASSERT.
		if (!standardEngaged)
		{
			SetFailure("a STANDARD battle must be engaged the moment it is spawned - every world-suppression gate in the game reads this");
			return true;
		}

		if (!managerEngagedForStandard)
		{
			SetFailure("IsQRFEngaged() must be true with a standard battle in the slot");
			return true;
		}

		if (managerEngagedWithNoBattle)
		{
			SetFailure("IsQRFEngaged() must be false with no battle at all");
			return true;
		}

		if (stageAtBirth != OVT_EQRFStage.SILENT_DEPLOY)
		{
			SetFailure("a counter-attack starts in SILENT_DEPLOY: read back stage %1", stageAtBirth.ToString());
			return true;
		}

		if (siegeEngagedAtBirth || managerEngagedAtBirth)
		{
			SetFailure("a counter-attack in SILENT_DEPLOY is NOT engaged - the world keeps living while the encirclement forms (controller %1, manager %2)",
				siegeEngagedAtBirth.ToString(), managerEngagedAtBirth.ToString());
			return true;
		}

		if (stageAfterDeploy != OVT_EQRFStage.MUSTER)
		{
			SetFailure("one tick with an empty spawn queue must complete the encirclement: read back stage %1", stageAfterDeploy.ToString());
			return true;
		}

		if (clockAfterDeploy != OVT_QRFSiege.MUSTER_TIME_MS)
		{
			SetFailure("entering MUSTER must arm the muster window: got %1 ms, expected %2 ms",
				clockAfterDeploy.ToString(), OVT_QRFSiege.MUSTER_TIME_MS.ToString());
			return true;
		}

		if (!revealedInMuster)
		{
			SetFailure("entering MUSTER must reveal the battle to the resistance");
			return true;
		}

		if (siegeEngagedInMuster || managerEngagedInMuster)
		{
			SetFailure("a counter-attack in MUSTER is still NOT engaged - being told is not being shot at (controller %1, manager %2)",
				siegeEngagedInMuster.ToString(), managerEngagedInMuster.ToString());
			return true;
		}

		if (stageAfterMuster != OVT_EQRFStage.BATTLE)
		{
			SetFailure("the tick that runs the muster clock out must start the assault: read back stage %1", stageAfterMuster.ToString());
			return true;
		}

		if (clockAfterMuster != 0)
		{
			SetFailure("entering BATTLE must zero the clock, because scoring is gated on it: got %1 ms", clockAfterMuster.ToString());
			return true;
		}

		if (!siegeEngagedInBattle || !managerEngagedInBattle)
		{
			SetFailure("a counter-attack in BATTLE IS engaged (controller %1, manager %2)",
				siegeEngagedInBattle.ToString(), managerEngagedInBattle.ToString());
			return true;
		}

		Print("QRFSiege: a standard battle is engaged at birth; a counter-attack is not engaged in SILENT_DEPLOY, is revealed but still not engaged in MUSTER, and is engaged in BATTLE with the clock at zero");

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
}

//------------------------------------------------------------------------------------------------
//! 🔴 The early end refuses to fire on a LIVE GROUP WITH NO AGENTS IN IT.
//!
//! ==================================================================================================
//! WHY THIS CASE IS WORTH A LIVE FIXTURE WHEN THE PREDICATE IS ALREADY PINNED IN THE CHEAP TIER.
//! ==================================================================================================
//! OVT_QRFSiege.GroupNeutralised is asserted row by row in OVT_TEST_Logic_QRFSiege.c, including the
//! zero-agent row. What that cannot check is the CALLER: whether the controller's own loop really
//! reads a live group's agent count and routes it through the predicate, rather than concluding death
//! from something that is not evidence of death. The engine's zero-agent state is a known-unreliable
//! reading, and so - as this case proved on its first run - is a failed EntityID lookup. A false
//! positive from either does not cost accuracy: it ends the battle mid-muster and hands the resistance
//! the objective for free, silently.
//!
//! ==================================================================================================
//! 🔴 THIS CASE CAUGHT A REAL DEFECT ON ITS FIRST RUN, AND THE FIXTURE IT CAUGHT IT WITH WAS WRONG TOO.
//! ==================================================================================================
//! The first cut built its empty group with GetGame().SpawnEntity(SCR_AIGroup, ...) and inserted
//! GetID() in the same frame. That id did not resolve, the controller read "cannot find it" as "it is
//! dead", the early end fired and the siege jumped to BATTLE - the exact D16 catastrophe. TWO things
//! were wrong and BOTH were fixed:
//!
//!  1. THE PRODUCT. CheckSiegeWipedOut now refuses to declare anything dead until it has resolved at
//!     least one tracked group ALIVE (m_bSiegeForceSeenAlive). An id that never resolved is not
//!     evidence of a wipe - it is what a not-yet-world-registered entity looks like, and every
//!     unregistered entity shares one EntityID value
//!     (OVT_InactiveRecruitGroupComponent.c:76-83 records the same trap costing core its observer map).
//!  2. THE FIXTURE. It now spawns a REAL group through Resource.Load + SpawnEntityPrefab - the same
//!     call SpawnFromQueue uses - under SCR_AIGroup.IgnoreSpawning(true), which is the engine's own
//!     "create the group, populate nothing" seam. That yields a properly world-registered group entity
//!     with genuinely zero agents, which is the reading this case is about. It then waits ONE frame
//!     before using the id, for the reason the recruit component records.
//!
//! ⚠ THE FRAME HOP IS NOT A RETRY. It is a single, unconditional pass - there is no attempt counter and
//! no polling loop, and the second pass asserts unconditionally. A red here is real.
//!
//! ⚠ THE CASE PROVES THE CHECK IS LIVE BEFORE IT PROVES THE CHECK IS QUIET. A stage that did not
//! advance is only evidence if the same check DOES advance it when the group really is gone; otherwise
//! a controller whose early end had been deleted outright - or one whose new latch never arms - would
//! pass. So the empty group is asserted first, then deleted from the world, and the second driven tick
//! must reach BATTLE. That second half is also the only thing that proves the fix did not simply make
//! the early end unreachable.
//!
//! ⚠ THE RESOLVABILITY PRECONDITION IS ASSERTED EXPLICITLY. "A live group with zero agents counts as
//! alive" says nothing at all if the group was never resolvable, so the case checks that it is - and
//! fails with a fixture-flavoured message if not, which is deliberately distinct from the 🔴 product
//! message below it.
//!
//! ⚠ IgnoreSpawning IS A STATIC FLAG THAT THE NEXT GROUP TO INITIALISE CONSUMES. It is cleared again
//! immediately after the spawn call returns, on every path, so a spawn that failed before EOnInit
//! cannot leave it set and silently unpopulate somebody else's group later in the run.
//!
//! PROVEN ABLE TO FAIL: `if(agents.Count() == 0) { neutralised++; continue; }` was added to
//! CheckSiegeWipedOut's loop - the "0 agents = dead" prune, written the way it is habitually written.
//! The tree recompiled CLEAN (exit 0) and the case then reports "🔴 A LIVE GROUP WITH ZERO AGENTS MUST
//! COUNT AS ALIVE: the siege entered BATTLE against a force that had not been fought". Prune removed,
//! tree recompiled clean.
//!
//! ⚠ THIS CASE DOES **NOT** COVER THE m_bSiegeForceSeenAlive LATCH, and saying so is the honest thing:
//! its fixture group resolves, so the latch arms and deleting the guard changes nothing here. The latch
//! has its own case below (…_EarlyEndNeverFiresOnAGroupItNeverSawAlive), which is the one that would go
//! red if the guard were removed.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_QRFSiege_EarlyEndTreatsAResolvedEmptyGroupAsAlive : SCR_AutotestCaseBase
{
	//! 0 = the group has not been created yet; 1 = it was created last frame and is now usable.
	protected int m_iPass;

	//! The fixture group, held across the one frame hop. Deleted on every exit of the second pass.
	protected SCR_AIGroup m_EmptyGroup;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPass == 0)
		{
			m_iPass = 1;

			string problem = CreateEmptyGroup();
			if (problem != "")
			{
				SetFailure("%1", problem);
				return true;
			}

			// ONE frame, so the entity is unambiguously world-registered before its id is used.
			return false;
		}

		return Judge();
	}

	//------------------------------------------------------------------------------------------------
	//! Second pass: build the battle, bring it into MUSTER, and ask the early end twice.
	//! \return Always true - this pass never waits for anything.
	protected bool Judge()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
		{
			DeleteFixtureGroup();
			SetFailure("The occupying faction manager did not resolve");
			return true;
		}

		if (!m_EmptyGroup)
		{
			SetFailure("The fixture group did not survive the frame hop, so the zero-agent reading could not be manufactured");
			return true;
		}

		vector fixturePosition;
		if (!ResolveFixturePosition(occupying, fixturePosition))
		{
			DeleteFixtureGroup();
			SetFailure("The world produced neither a town nor a base to hang a fixture battle on");
			return true;
		}

		// --- SAVE.
		bool savedRevealed = occupying.m_bQRFRevealed;
		int savedTimer = occupying.m_iQRFTimer;
		int savedPoints = occupying.m_iQRFPoints;
		int savedTown = occupying.m_iCurrentQRFTown;
		int savedBase = occupying.m_iCurrentQRFBase;

		// Both indices at -1: no civilian suppression and no counter-attack notification for a place
		// this fixture is not really besieging. See the sibling case for the full reasoning.
		occupying.m_iCurrentQRFTown = -1;
		occupying.m_iCurrentQRFBase = -1;

		OVT_QRFControllerComponent siege = occupying.SpawnQRFController(fixturePosition);
		if (!siege)
		{
			occupying.m_iCurrentQRFTown = savedTown;
			occupying.m_iCurrentQRFBase = savedBase;
			DeleteFixtureGroup();
			SetFailure("The occupying faction manager could not spawn a battle controller");
			return true;
		}

		siege.m_eMode = OVT_EQRFMode.COUNTER_ATTACK;

		// One tick on an empty spawn queue takes the machine into MUSTER, which is the only stage the
		// early end is asked in.
		siege.CheckUpdateTimer();

		if (siege.GetStage() != OVT_EQRFStage.MUSTER)
		{
			occupying.m_bQRFRevealed = savedRevealed;
			occupying.m_iQRFTimer = savedTimer;
			occupying.m_iCurrentQRFTown = savedTown;
			occupying.m_iCurrentQRFBase = savedBase;
			SCR_EntityHelper.DeleteEntityAndChildren(siege.GetOwner());
			DeleteFixtureGroup();
			SetFailure("The fixture could not be brought into MUSTER, so the early end was never asked");
			return true;
		}

		// --- ARRANGE: read back the two facts this case's claim depends on, through the SAME lookup
		//     the controller uses, so a broken fixture reports itself instead of libelling the product.
		int agentsInFixture = m_EmptyGroup.GetAgentsCount();
		EntityID fixtureId = m_EmptyGroup.GetID();
		bool fixtureResolves = GetGame().GetWorld().FindEntityByID(fixtureId) != null;

		siege.m_Groups.Insert(fixtureId);

		// --- ACT, half one: the group is alive and empty.
		siege.CheckUpdatePoints();

		OVT_EQRFStage stageWithEmptyGroup = siege.GetStage();

		// --- ACT, half two: the group is gone from the world entirely, which IS death.
		DeleteFixtureGroup();

		siege.CheckUpdatePoints();

		OVT_EQRFStage stageWithGoneGroup = siege.GetStage();

		// --- RESTORE before asserting.
		occupying.m_bQRFRevealed = savedRevealed;
		occupying.m_iQRFTimer = savedTimer;
		occupying.m_iQRFPoints = savedPoints;
		occupying.m_iCurrentQRFTown = savedTown;
		occupying.m_iCurrentQRFBase = savedBase;
		SCR_EntityHelper.DeleteEntityAndChildren(siege.GetOwner());

		// --- ASSERT the fixture really was the reading the case is about. Without these two the quiet
		//     half below would pass on a group that had men in it, or on one the controller could never
		//     see in the first place.
		if (agentsInFixture != 0)
		{
			SetFailure("the fixture group was supposed to be empty, which is the whole reading this case is about: it reported %1 agents", agentsInFixture.ToString());
			return true;
		}

		if (!fixtureResolves)
		{
			SetFailure("the fixture group is not world-resolvable, so 'a LIVE group with zero agents' was never actually presented to the controller. This is a fault in the fixture, not in the early end");
			return true;
		}

		if (stageWithEmptyGroup != OVT_EQRFStage.MUSTER)
		{
			SetFailure("🔴 A LIVE GROUP WITH ZERO AGENTS MUST COUNT AS ALIVE: the siege entered BATTLE against a force that had not been fought (stage %1)", stageWithEmptyGroup.ToString());
			return true;
		}

		if (stageWithGoneGroup != OVT_EQRFStage.BATTLE)
		{
			SetFailure("a siege whose only group has been deleted from the world HAS been wiped out and must start scoring - an early end that can never fire is a bug too: read back stage %1", stageWithGoneGroup.ToString());
			return true;
		}

		Print("QRFSiege: the early end leaves a siege in MUSTER while its one group is alive and empty, and advances it to BATTLE once that group is really gone");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a real occupying-faction group prefab with its members deliberately not created.
	//!
	//! ⚠ THE SAME SPAWN CALL SpawnFromQueue USES. A group built any other way is not the thing the
	//! controller will meet in a real battle, and the first cut of this case proved that the difference
	//! matters. SCR_AIGroup.IgnoreSpawning(true) is the engine's own seam for "create the group, create
	//! no members"; it is consumed by the next group to initialise and is cleared again here on every
	//! path so it can never leak into an unrelated spawn.
	//! \return "" on success, or the reason the fixture could not be built.
	protected string CreateEmptyGroup()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return "The Overthrow config did not resolve, so no group prefab could be found";

		OVT_Faction faction = config.GetOccupyingFaction();
		if (!faction || !faction.m_aGroupPrefabSlots || faction.m_aGroupPrefabSlots.IsEmpty())
			return "The occupying faction authors no group prefabs, so an empty group could not be built";

		// Index 0 rather than a random element: this fixture must not vary between runs.
		Resource groupResource = Resource.Load(faction.m_aGroupPrefabSlots[0]);
		if (!groupResource || !groupResource.IsValid())
			return "The occupying faction's first group prefab could not be loaded";

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		vector position;
		if (!occupying || !ResolveFixturePosition(occupying, position))
			return "The world produced neither a town nor a base to put the fixture group at";

		EntitySpawnParams spawnParams = new EntitySpawnParams;
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform[3] = position;

		SCR_AIGroup.IgnoreSpawning(true);
		IEntity spawned = GetGame().SpawnEntityPrefab(groupResource, GetGame().GetWorld(), spawnParams);
		SCR_AIGroup.IgnoreSpawning(false);

		m_EmptyGroup = SCR_AIGroup.Cast(spawned);
		if (!m_EmptyGroup)
		{
			if (spawned)
				SCR_EntityHelper.DeleteEntityAndChildren(spawned);

			return "The occupying faction's first group prefab did not spawn an SCR_AIGroup";
		}

		// Defensive: a group created empty never fires OnEmpty, but a fixture whose entire point is
		// "this group is empty and must stay alive" should not depend on that.
		m_EmptyGroup.SetDeleteWhenEmpty(false);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the fixture group from the world, once, whatever path the case leaves by.
	protected void DeleteFixtureGroup()
	{
		if (!m_EmptyGroup)
			return;

		SCR_EntityHelper.DeleteEntityAndChildren(m_EmptyGroup);
		m_EmptyGroup = null;
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

//------------------------------------------------------------------------------------------------
//! 🔴 The early end never fires on a group it never saw alive.
//!
//! ==================================================================================================
//! THIS IS THE CASE THAT COVERS THE FIX FOR THE DEFECT THE SIBLING CASE ABOVE FOUND.
//! ==================================================================================================
//! CheckSiegeWipedOut resolves each tracked group with FindEntityByID, and a null answer is AMBIGUOUS:
//! the group was deleted (death), or the id never resolved at all. The second is a reading this
//! component can genuinely take - an entity that is not world-registered answers GetID() with
//! EntityID.INVALID, every unregistered entity shares that one value, and SpawnFromQueue reads
//! group.GetID() in the same frame as the spawn (OVT_InactiveRecruitGroupComponent.c:76-83 records the
//! same trap costing core its observer map).
//!
//! Read that second case as death and the FIRST early-end tick of a healthy siege declares the whole
//! force wiped out, jumps to BATTLE against men nobody fought, and hands the resistance the objective
//! for free with nothing in the log. So the controller now refuses to declare anything dead until it
//! has resolved at least one tracked group ALIVE.
//!
//! THE FIXTURE IS THE PRODUCTION READING, NOT A SYNTHETIC ONE. A real group prefab is spawned and then
//! DELETED BEFORE THE BATTLE EVER EXISTS, so the id handed to the controller is one that has never
//! resolved for it - which is precisely what an unregistered id looks like from inside the check.
//!
//! ⚠ WHAT THIS CASE DELIBERATELY PINS IS A TRADE-OFF, AND IT IS THE CHEAP HALF OF IT. A force that was
//! genuinely wiped out before any early-end tick ever saw it standing will now wait out its muster
//! clock instead of ending early. That is D16's own stated preference - late, never early - and the
//! sibling case above proves the early end still fires normally for a force that WAS seen alive, so the
//! fix has not made it unreachable.
//!
//! ⚠ ENTITY IDs ARE RECYCLED BY THE ENGINE, so in principle the deleted group's id could be reissued
//! before the second pass. Nothing spawns in this world between the two passes, and a recycled id would
//! have to land on an SCR_AIGroup specifically to change the answer.
//!
//! CAN-FAIL, two faults, injected and compiled separately, both reaching the same 🔴 assertion because
//! they are the two independent ways the guard can be defeated:
//!   L1. REVERT THE FIX - delete `if(!m_bSiegeForceSeenAlive) return;` from CheckSiegeWipedOut, which is
//!       exactly the shape that produced the original red. Compiled clean (exit 0). The case then
//!       reports "🔴 AN ID THAT NEVER RESOLVED IS NOT A DEAD GROUP: the siege entered BATTLE (stage 2)
//!       on a group the check had never once seen alive".
//!   L2. ARM THE LATCH UNCONDITIONALLY - `m_bSiegeForceSeenAlive = true;` in place of the
//!       `if(resolvedNow > 0)` that guards it. Compiled clean (exit 0). Same failure text, and it is
//!       worth recording separately: the guard and the thing that arms it are two claims, and a future
//!       edit that "simplifies" the arming is the likelier of the two mistakes.
//! Both reverted, tree recompiled clean.
//!
//! ⚠ THE ZERO-AGENT PRUNE (`if(agents.Count() == 0) neutralised++`) WAS RE-INJECTED AFTER THIS CASE WAS
//! ADDED and still compiles clean; it is caught by the sibling case above, not by this one. The two
//! cases guard the two different unreliable readings and neither substitutes for the other.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_QRFSiege_EarlyEndNeverFiresOnAGroupItNeverSawAlive : SCR_AutotestCaseBase
{
	//! 0 = the id has not been minted yet; 1 = it was minted and its entity destroyed last frame.
	protected int m_iPass;

	//! An EntityID that belonged to a real group and no longer resolves to anything.
	protected EntityID m_StaleId;

	//! Whether the first pass managed to mint one.
	protected bool m_bHaveStaleId;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPass == 0)
		{
			m_iPass = 1;

			string problem = MintStaleId();
			if (problem != "")
			{
				SetFailure("%1", problem);
				return true;
			}

			// ONE frame, so the deletion has unambiguously taken effect before the id is used.
			return false;
		}

		return Judge();
	}

	//------------------------------------------------------------------------------------------------
	//! Second pass: hand the stale id to a fresh siege and require that it changes nothing.
	//! \return Always true.
	protected bool Judge()
	{
		if (!m_bHaveStaleId)
		{
			SetFailure("No stale group id was minted, so the never-seen-alive reading could not be presented");
			return true;
		}

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
		{
			SetFailure("The occupying faction manager did not resolve");
			return true;
		}

		vector fixturePosition;
		if (!ResolveFixturePosition(occupying, fixturePosition))
		{
			SetFailure("The world produced neither a town nor a base to hang a fixture battle on");
			return true;
		}

		// The precondition the whole case rests on: this id really does resolve to nothing.
		bool staleResolves = GetGame().GetWorld().FindEntityByID(m_StaleId) != null;

		// --- SAVE.
		bool savedRevealed = occupying.m_bQRFRevealed;
		int savedTimer = occupying.m_iQRFTimer;
		int savedPoints = occupying.m_iQRFPoints;
		int savedTown = occupying.m_iCurrentQRFTown;
		int savedBase = occupying.m_iCurrentQRFBase;

		occupying.m_iCurrentQRFTown = -1;
		occupying.m_iCurrentQRFBase = -1;

		OVT_QRFControllerComponent siege = occupying.SpawnQRFController(fixturePosition);
		if (!siege)
		{
			occupying.m_iCurrentQRFTown = savedTown;
			occupying.m_iCurrentQRFBase = savedBase;
			SetFailure("The occupying faction manager could not spawn a battle controller");
			return true;
		}

		siege.m_eMode = OVT_EQRFMode.COUNTER_ATTACK;
		siege.CheckUpdateTimer();

		OVT_EQRFStage stageAfterDeploy = siege.GetStage();

		// The battle has never once resolved this id, because its entity was gone before the battle
		// existed. Ask the early end three times, so a latch that arms on some later tick is caught too.
		siege.m_Groups.Insert(m_StaleId);

		siege.CheckUpdatePoints();
		siege.CheckUpdatePoints();
		siege.CheckUpdatePoints();

		OVT_EQRFStage stageAfterChecks = siege.GetStage();

		// --- RESTORE before asserting.
		occupying.m_bQRFRevealed = savedRevealed;
		occupying.m_iQRFTimer = savedTimer;
		occupying.m_iQRFPoints = savedPoints;
		occupying.m_iCurrentQRFTown = savedTown;
		occupying.m_iCurrentQRFBase = savedBase;
		SCR_EntityHelper.DeleteEntityAndChildren(siege.GetOwner());

		// --- ASSERT.
		if (staleResolves)
		{
			SetFailure("the stale id still resolves to an entity, so the never-seen-alive reading was never presented. This is a fault in the fixture, not in the early end");
			return true;
		}

		if (stageAfterDeploy != OVT_EQRFStage.MUSTER)
		{
			SetFailure("The fixture could not be brought into MUSTER, so the early end was never asked: stage %1", stageAfterDeploy.ToString());
			return true;
		}

		if (stageAfterChecks == OVT_EQRFStage.BATTLE)
		{
			SetFailure("🔴 AN ID THAT NEVER RESOLVED IS NOT A DEAD GROUP: the siege entered BATTLE (stage %1) on a group the check had never once seen alive", stageAfterChecks.ToString());
			return true;
		}

		// Asserted as MUSTER specifically, not merely "not BATTLE": a machine that had fallen back to
		// SILENT_DEPLOY would also satisfy the weaker claim while being thoroughly broken.
		if (stageAfterChecks != OVT_EQRFStage.MUSTER)
		{
			SetFailure("the siege left MUSTER for something that is not BATTLE either: stage %1", stageAfterChecks.ToString());
			return true;
		}

		Print("QRFSiege: an id the battle never once resolved does not end it - three early-end checks left the siege in MUSTER, exactly as a force that has never been seen alive must");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a real group prefab and immediately destroys it, keeping only its id.
	//!
	//! ⚠ IgnoreSpawning KEEPS IT UNPOPULATED so nothing is left behind when it goes, and it is cleared
	//! again on every path so the static cannot leak into an unrelated spawn later in the run.
	//! \return "" on success, or the reason the fixture could not be built.
	protected string MintStaleId()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return "The Overthrow config did not resolve, so no group prefab could be found";

		OVT_Faction faction = config.GetOccupyingFaction();
		if (!faction || !faction.m_aGroupPrefabSlots || faction.m_aGroupPrefabSlots.IsEmpty())
			return "The occupying faction authors no group prefabs, so a stale group id could not be minted";

		Resource groupResource = Resource.Load(faction.m_aGroupPrefabSlots[0]);
		if (!groupResource || !groupResource.IsValid())
			return "The occupying faction's first group prefab could not be loaded";

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		vector position;
		if (!occupying || !ResolveFixturePosition(occupying, position))
			return "The world produced neither a town nor a base to put the fixture group at";

		EntitySpawnParams spawnParams = new EntitySpawnParams;
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform[3] = position;

		SCR_AIGroup.IgnoreSpawning(true);
		IEntity spawned = GetGame().SpawnEntityPrefab(groupResource, GetGame().GetWorld(), spawnParams);
		SCR_AIGroup.IgnoreSpawning(false);

		if (!spawned)
			return "The occupying faction's first group prefab did not spawn";

		m_StaleId = spawned.GetID();
		m_bHaveStaleId = true;

		SCR_EntityHelper.DeleteEntityAndChildren(spawned);

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
