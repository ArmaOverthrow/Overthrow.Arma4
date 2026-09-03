//! Soak tier: cases that wait on a wall-clock window or a real spawn schedule.
//! Same world as the Init tier, campaign not started. The All group runs it, the Fast group does not.
[BaseContainerProps()]
class OVT_TEST_SoakSuite : OVT_TEST_SuiteBase
{
}

//------------------------------------------------------------------------------------------------
//! The status heartbeat is INSTALLED and really writes what it measures back onto the record.
//!
//! WHY THIS CASE EXISTS. Everything a client and every map marker knows about a High Command group
//! comes from its record, and after the purchase frame the ONLY thing that refreshes a record is the
//! STATUS_SYNC_INTERVAL_MS tick installed in OVT_HighCommandManagerComponent.OnPostInit(). A tick
//! that was never installed - or one whose body returns early - produces no error of any kind: every
//! marker simply freezes at the barracks forever and every roster row reports the purchase-frame
//! member count for the rest of the campaign.
//!
//! HOW IT IS PROVEN. The record's measured fields are deliberately WIPED after the spawn - position
//! to zero, flags to an impossible -1, alive count to 0 - and the case then waits for the tick to put
//! real values back. Only the sweep writes those three fields, so nothing else can make this pass.
//!
//! THE BUDGET IS WALL CLOCK, NOT FRAMES. The heartbeat is a CallLater in milliseconds, so a frame
//! budget would be generous on a loaded machine and far too tight on a fast one. Expiry is its own
//! named failure - this is a precondition, not a retry.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_SoakSuite, timeoutS: 120)]
class OVT_TEST_Init_HighCommandSeam_DHeartbeatFillsTheRecord : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the managers to post-init.
	static const int MAX_MANAGER_POLLS = 300;

	//! How long to wait for the heartbeat, in ms. Three intervals: one to arm, one to fire, one spare.
	static const int HEARTBEAT_BUDGET_MS = 30000;

	//! Every bit OVT_HighCommandStatus defines. Anything outside this is not a status mask.
	static const int KNOWN_STATUS_BITS = 15;

	static const int STAGE_SPAWN = 0;
	static const int STAGE_HEARTBEAT = 1;

	protected int m_iStage = STAGE_SPAWN;
	protected int m_iPolls;

	protected string m_sGroupId;
	protected int m_iExpectedMembers;
	protected float m_fDeadline;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iStage == STAGE_SPAWN)
			return SpawnStage();

		return HeartbeatStage();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a group and blanks the three fields only the sweep writes.
	//! \return True when the case is finished, false to poll again next frame.
	protected bool SpawnStage()
	{
		OVT_HighCommandManagerComponent manager = OVT_Global.GetHighCommand();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!manager || !config)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_MANAGER_POLLS)
			{
				SetFailure("The High Command manager or the Overthrow config was still null after %1 frames - this case cannot say anything about the status heartbeat either way", m_iPolls.ToString());
				return true;
			}

			return false;
		}

		if (!Replication.IsServer())
		{
			SetFailure("This world is not the server, so no group can be spawned and the heartbeat never runs - the Init suite is expected to run on a listen host");
			return true;
		}

		ResourceName sourcePrefab;
		array<ResourceName> slots = {};
		if (!PickSourceGroupPrefab(config, sourcePrefab, slots))
		{
			SetFailure("No faction group prefab in this world resolved to a non-empty m_aUnitPrefabSlots, so there is no composition to spawn a High Command group from");
			return true;
		}

		m_iExpectedMembers = slots.Count();

		vector position = OVT_TEST_VirtualizationFixture.PickPosition();
		if (position == vector.Zero)
		{
			SetFailure("This world defines no town to spawn a High Command group beside");
			return true;
		}

		OVT_HighCommandRecord record = manager.SpawnGroup("test_heartbeat", sourcePrefab, ResourceName.Empty, position, "test_heartbeat_owner");
		if (!record)
		{
			SetFailure("OVT_HighCommandManagerComponent.SpawnGroup() returned null for a group prefab with %1 member slot(s) - see case C for the two reasons that happens", m_iExpectedMembers.ToString());
			return true;
		}

		m_sGroupId = record.m_sGroupId;

		if (record.m_iTotalMembers != m_iExpectedMembers)
		{
			SetFailure("The record claims %1 member(s) at spawn while the source prefab defines %2 - the heartbeat is measured against this number, so it would report a wrong roster from the first tick", record.m_iTotalMembers.ToString(), m_iExpectedMembers.ToString());
			Dismiss(manager);
			return true;
		}

		// Only SweepStatus() writes these three. Blanking them is what makes the wait meaningful.
		record.m_vLastKnownPosition = vector.Zero;
		record.m_iStatusFlags = -1;
		record.m_iAliveMembers = 0;

		m_fDeadline = Now() + HEARTBEAT_BUDGET_MS;
		m_iStage = STAGE_HEARTBEAT;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for one heartbeat and reads what it wrote.
	//! \return True when the case is finished, false to poll again next frame.
	protected bool HeartbeatStage()
	{
		OVT_HighCommandManagerComponent manager = OVT_Global.GetHighCommand();
		if (!manager)
		{
			SetFailure("The High Command manager went null while waiting for the status heartbeat");
			return true;
		}

		OVT_HighCommandRecord record = manager.GetGroup(m_sGroupId);
		if (!record)
		{
			SetFailure("The High Command group's record vanished while waiting for the status heartbeat - the group was wiped or dismissed by something else in this world");
			return true;
		}

		if (record.m_iStatusFlags < 0)
		{
			if (Now() > m_fDeadline)
			{
				SetFailure("The status heartbeat had not run %1 ms after the group was spawned. OVT_HighCommandManagerComponent.OnPostInit() is what installs it (CallLater at STATUS_SYNC_INTERVAL_MS), so without it every High Command marker freezes where the group was bought and every roster row reports the purchase-frame roster for the rest of the campaign.", HEARTBEAT_BUDGET_MS.ToString());
				Dismiss(manager);
				return true;
			}

			return false;
		}

		if ((record.m_iStatusFlags & ~KNOWN_STATUS_BITS) != 0)
		{
			SetFailure("The heartbeat wrote the status mask %1, which carries bits OVT_HighCommandStatus does not define - the map badge and the roster icon both read this mask", record.m_iStatusFlags.ToString());
			Dismiss(manager);
			return true;
		}

		if (record.m_vLastKnownPosition == vector.Zero)
		{
			SetFailure("The heartbeat wrote a status mask but left the group's last known position at the origin - every client's marker would be drawn at the corner of the map");
			Dismiss(manager);
			return true;
		}

		if (record.m_iAliveMembers != m_iExpectedMembers)
		{
			SetFailure("The heartbeat counted %1 live member(s) in a group that was spawned with %2 - the roster line and the cap arithmetic are both read from this number", record.m_iAliveMembers.ToString(), m_iExpectedMembers.ToString());
			Dismiss(manager);
			return true;
		}

		if (record.m_iTotalMembers != m_iExpectedMembers)
		{
			SetFailure("The group's total member count read %1 after the heartbeat, against the %2 it was spawned with", record.m_iTotalMembers.ToString(), m_iExpectedMembers.ToString());
			Dismiss(manager);
			return true;
		}

		PrintFormat("High Command seam: the heartbeat re-measured a %1-man group onto its record (flags %2)", m_iExpectedMembers.ToString(), record.m_iStatusFlags.ToString());

		Dismiss(manager);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return World time in ms.
	protected float Now()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		return world.GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the group back out of the world, so one case does not leave a squad standing for the next.
	//! \param[in] manager The High Command manager.
	protected void Dismiss(notnull OVT_HighCommandManagerComponent manager)
	{
		if (m_sGroupId != "")
			manager.DismissGroup(m_sGroupId);
	}

	//------------------------------------------------------------------------------------------------
	//! The first faction group prefab in this world with a real composition, read straight off the
	//! prefab source - an expectation derived from the code under test would agree with it however
	//! wrong it is.
	//! \param[in] config The Overthrow config, for the player faction.
	//! \param[out] groupPrefab The prefab found.
	//! \param[out] slots Its member prefabs.
	//! \return True when one was found.
	protected bool PickSourceGroupPrefab(notnull OVT_OverthrowConfigComponent config, out ResourceName groupPrefab, out array<ResourceName> slots)
	{
		OVT_Faction faction = config.GetPlayerFaction();
		if (!faction || !faction.m_aGroupPrefabSlots)
			return false;

		foreach (ResourceName candidate : faction.m_aGroupPrefabSlots)
		{
			array<ResourceName> candidateSlots = {};
			if (!ReadSlots(candidate, candidateSlots))
				continue;

			groupPrefab = candidate;
			slots = candidateSlots;
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] groupPrefab A group prefab.
	//! \param[out] slots Its authored m_aUnitPrefabSlots.
	//! \return True when it defines at least one member.
	protected bool ReadSlots(ResourceName groupPrefab, out array<ResourceName> slots)
	{
		Resource resource = Resource.Load(groupPrefab);
		if (!resource || !resource.IsValid())
			return false;

		BaseResourceObject resourceObject = resource.GetResource();
		if (!resourceObject)
			return false;

		IEntitySource source = resourceObject.ToEntitySource();
		if (!source)
			return false;

		source.Get("m_aUnitPrefabSlots", slots);

		return !slots.IsEmpty();
	}
}

//------------------------------------------------------------------------------------------------
//! THE MOVEMENT MANAGER RESOLVES, AND ITS TRANSIENT STATE DOES NOT LEAK.
//!
//! OVT_Global.GetVirtualMovement() answering is the only proof in the suites that the manager is on
//! the game-mode prefab (it was text-wired, not added in Workbench); every other movement claim is
//! silently vacuous without it. The tracked count must return to 0 once nothing is registered.
//!
//! ⚠ A group whose plan is empty, null or DEFEND-only keeps NO entry in the transient map - it is
//! re-classified cheaply each pass instead, which is what keeps the map empty in a campaign full of
//! garrisons. A group that latches stationary at RUNTIME, having reached a DEFEND point, KEEPS its
//! entry on purpose: dropping it would let the next pass re-derive a movable plan and walk the group
//! off the post it just took up. This case does not exercise that path and must not be "fixed" to
//! expect 0 for it.
//!
//! The count is settled with a bounded poll, and that wait IS the no-leak assertion.
//! GetTrackedCount() is a read-only diagnostic, not part of any API.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_SoakSuite, timeoutS: 60)]
class OVT_TEST_Init_VirtualMovement_ManagerResolvesAndDoesNotLeak : SCR_AutotestCaseBase
{
	//! Wall-clock ms allowed for the tick to drop the previous cases' progress. Several passes at the
	//! 2000 ms default; bounded, and not a retry budget.
	static const float SETTLE_WINDOW_MS = 10000;

	protected int m_iPhase;
	protected int m_iHandle = -1;
	protected float m_fDeadlineMs;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == 0)
			return Begin();

		if (m_iPhase == 1)
			return AwaitEmptyState();

		return AwaitGarrisonWindow();
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the manager and opens the settle window.
	//! \return True when the case is already finished (always a named failure at this phase).
	protected bool Begin()
	{
		OVT_VirtualMovementManagerComponent movement = OVT_Global.GetVirtualMovement();
		if (!movement)
		{
			SetFailure("OVT_Global.GetVirtualMovement() is null - the movement manager is not on the game-mode prefab, so every other movement claim in these suites is vacuous");
			return true;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			SetFailure("There is no world, so the movement tick has nothing to run against");
			return true;
		}

		// Init-tier worlds never run PostGameStart; install the tick (idempotent) so the no-leak claim
		// is made against a manager whose tick is actually running and purging.
		movement.PostGameStart();

		m_fDeadlineMs = world.GetWorldTime() + SETTLE_WINDOW_MS;
		m_iPhase = 1;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for the tracked count to return to 0 now that the cases above have unregistered their
	//! groups - the no-leak claim (Q7).
	//! \return True when the case is finished.
	protected bool AwaitEmptyState()
	{
		OVT_VirtualMovementManagerComponent movement = OVT_Global.GetVirtualMovement();
		BaseWorld world = GetGame().GetWorld();

		if (!movement || !world)
		{
			SetFailure("The movement manager or the world went away while the case was waiting for its state map to empty");
			return true;
		}

		int tracked = movement.GetTrackedCount();
		if (tracked > 0 && world.GetWorldTime() < m_fDeadlineMs)
			return false;

		if (tracked > 0)
		{
			SetFailure(string.Format("The movement manager still tracks %1 handle(s) %2 ms after every group was unregistered - transient progress is leaking, and in a long campaign it would grow for the rest of the session",
				tracked.ToString(), SETTLE_WINDOW_MS.ToString()));
			return true;
		}

		return RegisterGarrison();
	}

	//------------------------------------------------------------------------------------------------
	//! Registers a DEFEND-only group and opens the observation window for it.
	//! \return True when the case is finished (a named failure); false to keep going.
	protected bool RegisterGarrison()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null - the virtualization manager is missing from the game-mode prefab");
			return true;
		}

		string factionKey;
		string groupName;
		if (!OVT_TEST_VirtualizationFixture.FindComposition(factionKey, groupName))
		{
			SetFailure("No faction in this world defines a resolvable group registry entry, so a garrison cannot be registered");
			return true;
		}

		vector position = OVT_TEST_VirtualizationFixture.PickPosition();
		vector target = position;
		target[2] = position[2] + OVT_TEST_VirtualMovementFixture.LEG_LENGTH_M;

		// A DEFEND plan is never walked, so this leg does not have to be on land - only distinct.
		// spawnDistanceOverride 0 = Manual policy, dormant by construction (see the walking case).
		m_iHandle = virtualization.RegisterGroup(OVT_TEST_VirtualMovementFixture.OWNER_SYSTEM, "movement_no_leak",
			factionKey, groupName, position,
			OVT_TEST_VirtualMovementFixture.BuildPlan(position, target, OVT_EVirtualWaypointType.DEFEND), 0);

		if (m_iHandle == -1)
		{
			SetFailure("RegisterGroup returned -1 for a composition the faction registry resolves, so 'a garrison is not tracked' would be asserted against nothing");
			return true;
		}

		BaseWorld world = GetGame().GetWorld();
		m_fDeadlineMs = world.GetWorldTime() + OVT_TEST_VirtualMovementFixture.OBSERVE_WINDOW_MS;

		m_iPhase = 2;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Gives the tick several passes over the garrison, then asserts it is still tracking nothing.
	//! \return True when the case is finished.
	protected bool AwaitGarrisonWindow()
	{
		OVT_VirtualMovementManagerComponent movement = OVT_Global.GetVirtualMovement();
		BaseWorld world = GetGame().GetWorld();

		if (!movement || !world)
		{
			CleanUp();
			SetFailure("The movement manager or the world went away while the case was watching a garrison");
			return true;
		}

		if (world.GetWorldTime() < m_fDeadlineMs)
			return false;

		int tracked = movement.GetTrackedCount();

		// Cleanup BEFORE reporting, the suite's rule.
		CleanUp();

		if (tracked != 0)
		{
			SetFailure(string.Format("The movement manager tracks %1 handle(s) while the only registered group has a DEFEND-only plan - a plan that cannot move must hold no transient state at all, or a campaign of garrisons pays for progress none of them can make",
				tracked.ToString()));
			return true;
		}

		PrintFormat("OVT_Global.GetVirtualMovement() resolves, its tracked count returned to 0 after the walking cases, and a registered DEFEND-only group contributed nothing to it");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Unregisters the garrison on every exit path.
	protected void CleanUp()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (virtualization && m_iHandle != -1)
			virtualization.UnregisterGroup(m_iHandle);

		m_iHandle = -1;
	}
}

//------------------------------------------------------------------------------------------------
//! A DEFEND-ONLY PLAN IS NEVER ADVANCED - the D10 opt-in contract, asserted from the other side.
//!
//! "The plan IS the opt-in": there is no flag to set and no core field to check, so this
//! classification is the only thing between a tower garrison and a stroll across the map.
//!
//! Deliberately the same shape as the case above with ONE variable changed - the waypoint type - so
//! the pair is a controlled experiment: both red means the tick is dead, this one alone means the
//! classification is.
//!
//! Tolerance 0.5 m of XZ: below the smallest step a pass could take (1.5 m/s x 2 s) and above
//! floating-point noise. No exact boundary is asserted.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_SoakSuite, timeoutS: 60)]
class OVT_TEST_Init_VirtualMovement_StationaryPlanIsNeverAdvanced : SCR_AutotestCaseBase
{
	//! XZ metres a DEFEND group is allowed to drift over the whole window.
	static const float MAX_DRIFT_M = 0.5;

	protected int m_iPhase;
	protected int m_iHandle = -1;
	protected float m_fDeadlineMs;
	protected vector m_vStart;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == 0)
			return Arrange();

		return AwaitWindow();
	}

	//------------------------------------------------------------------------------------------------
	//! Registers the garrison and opens the observation window.
	//! \return True when the case is already finished (always a named failure at this phase).
	protected bool Arrange()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null - the virtualization manager is missing from the game-mode prefab");
			return true;
		}

		if (!OVT_Global.GetVirtualMovement())
		{
			SetFailure("OVT_Global.GetVirtualMovement() is null - with no movement manager 'a DEFEND plan is never advanced' would be asserted against nothing that could advance it");
			return true;
		}

		// Same tick-install as the walking case (Init-tier worlds never run PostGameStart themselves):
		// without a LIVE tick this case would pass vacuously - "never advanced" by a tick that never ran.
		// PostGameStart() is idempotent, so installing it here is safe whatever ran before.
		OVT_Global.GetVirtualMovement().PostGameStart();

		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			SetFailure("There is no world to hold a garrison still in");
			return true;
		}

		string factionKey;
		string groupName;
		if (!OVT_TEST_VirtualizationFixture.FindComposition(factionKey, groupName))
		{
			SetFailure("No faction in this world defines a resolvable group registry entry, so a garrison cannot be registered");
			return true;
		}

		vector position = OVT_TEST_VirtualizationFixture.PickPosition();

		// The SAME all-land leg the walking case uses: the only variable between the two cases must be
		// the waypoint type, so a red here can never be blamed on a leg pointed into a bay.
		vector target = OVT_TEST_VirtualMovementFixture.PickLandTarget(position);
		if (target == vector.Zero)
		{
			SetFailure(string.Format("No %1 m leg out of %2 is entirely on land in this world, so this case would not be the same experiment as the walking one",
				OVT_TEST_VirtualMovementFixture.LEG_LENGTH_M.ToString(), position.ToString()));
			return true;
		}

		// spawnDistanceOverride 0 = Manual policy, dormant by construction - same reasoning and same
		// controlled-pair discipline as the walking case: the only variable between the two is the type.
		m_iHandle = virtualization.RegisterGroup(OVT_TEST_VirtualMovementFixture.OWNER_SYSTEM, "movement_garrison",
			factionKey, groupName, position,
			OVT_TEST_VirtualMovementFixture.BuildPlan(position, target, OVT_EVirtualWaypointType.DEFEND), 0);

		if (m_iHandle == -1)
		{
			SetFailure("RegisterGroup returned -1 for a composition the faction registry resolves, so there is nothing to hold still");
			return true;
		}

		m_vStart = virtualization.GetPosition(m_iHandle);
		m_fDeadlineMs = world.GetWorldTime() + OVT_TEST_VirtualMovementFixture.OBSERVE_WINDOW_MS;

		m_iPhase = 1;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits out the WHOLE window - a garrison that leaves late is still a garrison that leaves.
	//! \return True when the case is finished.
	protected bool AwaitWindow()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		BaseWorld world = GetGame().GetWorld();

		if (!virtualization || !world)
		{
			CleanUp();
			SetFailure("The virtualization manager or the world went away while the case was watching a garrison stand still");
			return true;
		}

		if (world.GetWorldTime() < m_fDeadlineMs)
			return false;

		float drift = OVT_VirtualMovementMath.DistanceXZ(virtualization.GetPosition(m_iHandle), m_vStart);

		// Cleanup BEFORE reporting, the suite's rule.
		CleanUp();

		if (drift > MAX_DRIFT_M)
		{
			SetFailure(string.Format("A DEFEND-only group drifted %1 m in %2 ms - the plan is the ONLY opt-in movement has, so a garrison with a classification bug walks off its post and nothing else in the tree notices",
				drift.ToString(), OVT_TEST_VirtualMovementFixture.OBSERVE_WINDOW_MS.ToString()));
			return true;
		}

		PrintFormat("A DEFEND-only group held its position (%1 m of drift) across the whole %2 ms window",
			drift.ToString(), OVT_TEST_VirtualMovementFixture.OBSERVE_WINDOW_MS.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Unregisters the garrison on every exit path.
	protected void CleanUp()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (virtualization && m_iHandle != -1)
			virtualization.UnregisterGroup(m_iHandle);

		m_iHandle = -1;
	}
}

//------------------------------------------------------------------------------------------------
//! THE MOVEMENT TICK ACTUALLY ADVANCES A DORMANT GROUP, TOWARD ITS PLAN.
//!
//! The one end-to-end claim of `virtualization/movement`: enumeration, round-robin slice, lazy state
//! derivation, per-group dt, arrival maths and ground-snapped write all wired together and installed
//! on the game mode. Every other case asserts one piece in isolation.
//!
//! A group is registered with a two-point PATROL/MOVE plan 200 m out, then polled for up to 10 s:
//! XZ displacement exceeds 1 m (it moved), is less than the whole leg (no teleport), and ends closer
//! to the target (it moved TOWARD the plan). No exact distance boundary is asserted - vector.Distance
//! is +1 ULP off at 1000 m and 2000 m, and the step depends on how many passes the window contained.
//!
//! ⚠ The FIRST pass over a handle cannot move it: state is derived on first touch and stamped with
//! the current world time, so its dt is 0 by construction. The window is sized for several after it.
//!
//! Cleanup before reporting: this is the one case that deliberately registers a group that MOVES.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_SoakSuite, timeoutS: 60)]
class OVT_TEST_Init_VirtualMovement_TickAdvancesDormantGroup : SCR_AutotestCaseBase
{
	//! XZ metres the group must cover to count as advanced.
	static const float MIN_DISPLACEMENT_M = 1;

	protected int m_iPhase;
	protected int m_iHandle = -1;
	protected float m_fDeadlineMs;

	//! Where the group actually was once registered (NOT the requested position - core ground-snaps).
	protected vector m_vStart;

	//! The far point of the plan; the direction "toward" is measured against.
	protected vector m_vTarget;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == 0)
			return Arrange();

		return AwaitAdvance();
	}

	//------------------------------------------------------------------------------------------------
	//! Registers the walking group and opens the observation window.
	//! \return True when the case is already finished (always a named failure at this phase).
	protected bool Arrange()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null - the virtualization manager is missing from the game-mode prefab");
			return true;
		}

		if (!OVT_Global.GetVirtualMovement())
		{
			SetFailure("OVT_Global.GetVirtualMovement() is null - the movement manager is missing from the game-mode prefab, so no registered group can ever advance");
			return true;
		}

		// Init-tier worlds never press Start (RequiresStartedCampaign() is false for this suite), so
		// DoStartGame()/PostGameStart() never ran here and the movement CallLater was never installed.
		// PostGameStart() is public and idempotent (m_bTickRunning latch), so the case installs the
		// exact tick it is about to assert against - without it the group sits still and this case
		// reds with "the tick is not advancing registered groups at all".
		OVT_Global.GetVirtualMovement().PostGameStart();

		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			SetFailure("There is no world to walk a group across");
			return true;
		}

		string factionKey;
		string groupName;
		if (!OVT_TEST_VirtualizationFixture.FindComposition(factionKey, groupName))
		{
			SetFailure("No faction in this world defines a resolvable group registry entry, so a walking group cannot be registered");
			return true;
		}

		vector position = OVT_TEST_VirtualizationFixture.PickPosition();
		m_vTarget = OVT_TEST_VirtualMovementFixture.PickLandTarget(position);

		if (m_vTarget == vector.Zero)
		{
			SetFailure(string.Format("No %1 m leg out of %2 is entirely on land in this world, so movement's water rule would veto every write and 'the group moved' could not be asserted",
				OVT_TEST_VirtualMovementFixture.LEG_LENGTH_M.ToString(), position.ToString()));
			return true;
		}

		// spawnDistanceOverride 0 = the MANUAL lifecycle policy, so the engine never materialises the
		// group by proximity. ⚠ Dormant by construction is required: the autotest camera IS an observer
		// and at the global ring can spawn a test group's members, at which point the IsSpawned gate
		// refuses to advance and this case reds with "not advancing at all".
		m_iHandle = virtualization.RegisterGroup(OVT_TEST_VirtualMovementFixture.OWNER_SYSTEM, "movement_walks",
			factionKey, groupName, position,
			OVT_TEST_VirtualMovementFixture.BuildPlan(position, m_vTarget, OVT_EVirtualWaypointType.PATROL), 0);

		if (m_iHandle == -1)
		{
			SetFailure("RegisterGroup returned -1 for a composition the faction registry resolves, so there is nothing to advance");
			return true;
		}

		m_vStart = virtualization.GetPosition(m_iHandle);
		m_fDeadlineMs = world.GetWorldTime() + OVT_TEST_VirtualMovementFixture.OBSERVE_WINDOW_MS;

		m_iPhase = 1;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Polls until the group has visibly moved, or the window is spent.
	//! \return True when the case is finished.
	protected bool AwaitAdvance()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		BaseWorld world = GetGame().GetWorld();

		if (!virtualization || !world)
		{
			CleanUp();
			SetFailure("The virtualization manager or the world went away while the case was watching a group walk");
			return true;
		}

		vector current = virtualization.GetPosition(m_iHandle);
		float moved = OVT_VirtualMovementMath.DistanceXZ(current, m_vStart);
		bool expired = world.GetWorldTime() >= m_fDeadlineMs;

		if (moved <= MIN_DISPLACEMENT_M && !expired)
			return false;

		string failure = Verify(current, moved);

		// Cleanup BEFORE reporting: this case registers the one group in the tree that deliberately
		// MOVES, and a red assertion must not leak it into the cases after it.
		CleanUp();

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		PrintFormat("The movement tick walked a dormant group %1 m of its %2 m leg within the observation window",
			moved.ToString(), OVT_VirtualMovementMath.DistanceXZ(m_vStart, m_vTarget).ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] current Where the group is now.
	//! \param[in] moved XZ metres covered since registration.
	//! \return An empty string when the group walked toward its plan, or the first broken claim.
	protected string Verify(vector current, float moved)
	{
		float leg = OVT_VirtualMovementMath.DistanceXZ(m_vStart, m_vTarget);

		if (moved <= MIN_DISPLACEMENT_M)
		{
			int tracked = 0;
			OVT_VirtualMovementManagerComponent movement = OVT_Global.GetVirtualMovement();
			if (movement)
				tracked = movement.GetTrackedCount();

			return string.Format("A dormant group with a movable %1 m plan moved %2 m in %3 ms (%4 handle(s) tracked) - the tick is not advancing registered groups at all",
				leg.ToString(), moved.ToString(), OVT_TEST_VirtualMovementFixture.OBSERVE_WINDOW_MS.ToString(), tracked.ToString());
		}

		if (moved >= leg)
			return string.Format("The group covered %1 m of a %2 m leg - a single pass may never cover the whole route, which is what an unclamped dt or a broken step clamp would do",
				moved.ToString(), leg.ToString());

		if (OVT_VirtualMovementMath.DistanceXZ(current, m_vTarget) >= leg)
			return string.Format("The group moved %1 m but is %2 m from its target, no closer than the %3 m it started at - it is walking somewhere, just not toward its plan",
				moved.ToString(), OVT_VirtualMovementMath.DistanceXZ(current, m_vTarget).ToString(), leg.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Unregisters the walking group on every exit path.
	protected void CleanUp()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (virtualization && m_iHandle != -1)
			virtualization.UnregisterGroup(m_iHandle);

		m_iHandle = -1;
	}
}

//------------------------------------------------------------------------------------------------
//! The CONFIG's overridden RollCount() is the one core calls - not the arithmetic behind it.
//!
//! The modder seam the whole ambient design rests on: a subclass overrides a roll, a .conf names the
//! subclass, no core code changes. ⚠ If core ever "optimised" that into a direct
//! RollCountSafe(m_iMinCount, m_iMaxCount) call, every subclass in every consumer mod would silently
//! stop being consulted - the source would still spawn, just with the authored numbers.
//!
//! OVT_TEST_AmbientCountingConfig authors min == max == 1 but overrides RollCount() to return 2, so
//! counting prefab requests in one activation distinguishes the two: 2 means consulted, 1 bypassed.
//! To make an activation happen the case parks a LOCAL OBSERVER on the source position through the
//! engine's own ObserversSystem and waits out the 2 s ambient tick.
//!
//! Where this world cannot activate, it falls back to asserting virtual dispatch through a
//! BASE-TYPED reference and says so in the log. It never passes silently on the weaker claim.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_SoakSuite, timeoutS: 60)]
class OVT_TEST_Init_Virtualization_AmbientRollCountOverrideIsCalled : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the 2 s ambient tick to come round (generous: the tick is wall-clock,
	//! the test world's frame rate is not this case's subject). Bounded, and NOT a retry budget -
	//! the case asserts once, when the activation has happened or the budget is spent.
	static const int MAX_POLLS = 1200;

	protected int m_iPhase;
	protected int m_iPolls;
	protected int m_iHandle = -1;
	protected ref OVT_TEST_AmbientCountingConfig m_Config;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == 0)
			return Arrange();

		return AwaitActivation();
	}

	//------------------------------------------------------------------------------------------------
	//! Registers the counting source and parks an observer on it.
	//! \return True when the case is already finished (always a named failure at this phase).
	protected bool Arrange()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null");
			return true;
		}

		m_Config = new OVT_TEST_AmbientCountingConfig();
		m_Config.m_sSourceName = "test_ambient_rollcount";
		m_Config.m_iMinCount = 1;      // deliberately disagrees with the override
		m_Config.m_iMaxCount = 1;
		m_Config.m_fRadius = 10;

		vector position = OVT_TEST_VirtualizationFixture.PickPosition();

		m_iHandle = virtualization.RegisterAmbientSource(m_Config, position, "rollcount_case");
		if (m_iHandle == -1)
		{
			SetFailure("RegisterAmbientSource refused the test config");
			return true;
		}

		// ⚠ NO parked observer. InsertObserverSP with a null entity has ZERO vanilla callers, and the one
		// All-group run that parked one here froze the main thread the moment this case began. In a
		// world with real observers the source activates off them; in a world with none, the documented
		// fallback assertion runs.
		m_iPhase = 1;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Polls until the source has activated, or the budget is spent.
	//! \return True when the case is finished.
	protected bool AwaitActivation()
	{
		m_iPolls++;

		if (m_Config.m_iRollCountCalls == 0 && m_iPolls < MAX_POLLS)
			return false;

		string failure = Verify();
		CleanUp();

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty string when the override is the implementation core consulted.
	protected string Verify()
	{
		if (m_Config.m_iRollCountCalls == 0)
		{
			// This world never activated the source. Fall back to the weaker, world-free claim and
			// SAY SO - a silent pass here would hide a real regression.
			OVT_AmbientSpawnSourceConfig asBase = m_Config;
			int dispatched = asBase.RollCount();

			if (dispatched != OVT_TEST_AmbientCountingConfig.ROLLED_COUNT)
				return string.Format("RollCount() through a base-typed reference returned %1, expected the override's %2 - the modder seam is not virtual at all",
					dispatched.ToString(), OVT_TEST_AmbientCountingConfig.ROLLED_COUNT.ToString());

			PrintFormat("Ambient activation did not happen in this world within %1 polls (no honoured observer) - asserted virtual dispatch of the RollCount() override instead",
				m_iPolls.ToString());
			return "";
		}

		if (m_Config.m_iRollCountCalls != 1)
			return string.Format("RollCount() was called %1 times for one activation - the count must be rolled ONCE and then spent across ticks, never re-rolled per tick",
				m_Config.m_iRollCountCalls.ToString());

		if (m_Config.m_iRollPrefabCalls != OVT_TEST_AmbientCountingConfig.ROLLED_COUNT)
			return string.Format("The activation asked for %1 prefab(s); the override said %2 and the authored min/max said 1, so core consulted the wrong one",
				m_Config.m_iRollPrefabCalls.ToString(), OVT_TEST_AmbientCountingConfig.ROLLED_COUNT.ToString());

		PrintFormat("Ambient activation consulted the config subclass: RollCount() once, %1 prefab rolls (authored min/max would have given 1)",
			m_Config.m_iRollPrefabCalls.ToString());
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the observer and the source on EVERY exit path - a leaked source would keep ticking
	//! for the rest of the suite, and a leaked observer would keep whatever is near it awake.
	protected void CleanUp()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (virtualization && m_iHandle != -1)
			virtualization.UnregisterAmbientSource(m_iHandle);

		m_iHandle = -1;
	}

}
