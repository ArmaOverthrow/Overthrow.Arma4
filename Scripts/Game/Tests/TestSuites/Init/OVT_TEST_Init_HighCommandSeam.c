//------------------------------------------------------------------------------------------------
//! TIER B - the High Command seam reaches both hosts it needs: the manager on the game mode, and
//! the request component on the local player's own controller entity (OVT_TEST_Init_VehicleRequestSeam.c
//! is the template).
//!
//! Case C covers the Phase 2 group entity: spawn, composition, faction affiliation, the AI observer
//! and the dismissal teardown. Case D covers the Phase 6 heartbeat: the installed tick really does
//! re-measure a live group back onto its record.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! The manager resolves off the game mode.
//!
//! WHY THIS CASE EXISTS. The manager reaches the game through ONE text edit with no compile-time
//! link: a block on Prefabs/GameMode/OVT_OverthrowGameMode.et. Without it the accessor answers null,
//! every caller null-guards by contract and returns, and every High Command feature - purchase,
//! ordering, persistence - silently never happens with no compile error and no runtime error.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_HighCommandSeam_AManagerResolves : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the game mode's components to post-init.
	static const int MAX_POLLS = 300;

	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_HighCommandManagerComponent manager = OVT_Global.GetHighCommand();
		if (!manager)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("OVT_Global.GetHighCommand() is still null after %1 frames. Prefabs/GameMode/OVT_OverthrowGameMode.et has lost its OVT_HighCommandManagerComponent block, so no High Command feature can ever work.", m_iPolls.ToString());
				return true;
			}

			return false;
		}

		if (manager.GetMemberCap() <= 0)
		{
			SetFailure("The High Command manager resolved a member cap of %1 - the default is 48, and 0 or below silently means unlimited, which must be an explicit choice, not the unresolved-config fallback", manager.GetMemberCap().ToString());
			return true;
		}

		PrintFormat("High Command seam: the manager resolves with a member cap of %1 (found after %2 poll(s))", manager.GetMemberCap().ToString(), m_iPolls.ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_HighCommandRequestComponent is actually on the local player's own controller entity.
//!
//! TWO CLAIMS, ONE PRECONDITION:
//!   1. the component resolves through the epic-level accessor at all;
//!   2. what comes back is the instance carried by THIS player's own controller entity - not merely
//!      "a component of that type from somewhere", which would mean every High Command request is
//!      sent through another player's seam and resolved by the server as that player.
//!
//! THE POLL IS A PRECONDITION, NOT A RETRY (no maxAttempts anywhere in this project).
//! OVT_PlayerManagerComponent.SetupPlayer() spawns the controller when the player enters the world,
//! which is not instantaneous at world load. Expiry is itself a named failure carrying the
//! diagnosis, so nothing here can pass by being asked at a luckier moment.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_HighCommandSeam_BRequestComponentResolves : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the local player's controller to be spawned and registered.
	static const int MAX_POLLS = 300;

	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OverthrowController controller = OVT_Global.GetController();
		if (!controller)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("OVT_Global.GetController() was still null after %1 polls (local player id %2). Nothing on the controller seam is reachable from this machine, so this case cannot say anything about OVT_HighCommandRequestComponent either way.",
					m_iPolls.ToString(),
					SCR_PlayerController.GetLocalPlayerId().ToString());
				return true;
			}

			return false;
		}

		// Claim 1 - it is wired.
		OVT_HighCommandRequestComponent viaAccessor = OVT_ControllerComponent<OVT_HighCommandRequestComponent>.Get();
		if (!viaAccessor)
		{
			SetFailure("OVT_ControllerComponent<OVT_HighCommandRequestComponent>.Get() returned null while a controller entity exists. Prefabs/GameMode/OVT_OverthrowController.et is not carrying the component, so quoting, purchasing, ordering, converting and dismissing a High Command group would all silently never reach the server.");
			return true;
		}

		// Claim 2 - it is OUR controller's instance, not somebody else's.
		OVT_HighCommandRequestComponent onEntity = OVT_HighCommandRequestComponent.Cast(controller.FindComponent(OVT_HighCommandRequestComponent));
		if (viaAccessor != onEntity)
		{
			SetFailure("OVT_ControllerComponent<OVT_HighCommandRequestComponent>.Get() did not return the instance on the local player's own controller entity. Every High Command request would then be sent through another player's seam, and the server would resolve the caller - and therefore the ownership - as that player.");
			return true;
		}

		PrintFormat("High Command seam: OVT_HighCommandRequestComponent resolves off the local controller (found after %1 poll(s))", m_iPolls.ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A High Command group really is a live, resistance-affiliated squad carrying an AI observer, and
//! dismissing it takes all three back.
//!
//! FOUR CLAIMS, EACH ONE A SILENT FAILURE OF ITS OWN:
//!   a) the group entity exists and holds exactly the source prefab's composition - a member that
//!      spawned but never joined leaves a loose armed civilian and a group one man short;
//!   b) every member is affiliated with the configured player faction - CIV is friendly to everyone,
//!      so a mis-stamped group never classifies anybody as an enemy and never fights (BUG-146);
//!   c) the group is an AI observer one frame after it was created - without it a group parked in a
//!      town stands next to dormant patrols that never wake, which is the whole point of D2;
//!   d) dismissing it removes the observer and the record - a leaked observer holds everything near
//!      it materialised for the rest of the session, and its only symptom is a slow server.
//!
//! ⚠ OBSERVER PRESENCE IS READ FROM CORE'S OWN MAP (HasEntityObserver / GetEntityObserverCount),
//! never from ObserversSystem: engine application is deferred a frame in BOTH directions, so an
//! engine query reads a false negative right after an add and a false leak right after a remove
//! (virtualization/core api.md §3).
//!
//! THE POLLS ARE PRECONDITIONS, NOT RETRIES. Each expiry is its own named failure.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 90)]
class OVT_TEST_Init_HighCommandSeam_CGroupSpawnsWithObserver : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the managers to post-init.
	static const int MAX_MANAGER_POLLS = 300;

	//! Frame polls allowed for the deferred observer install to run. It is one call-queue hop.
	static const int MAX_OBSERVER_POLLS = 30;

	static const int STAGE_SPAWN = 0;
	static const int STAGE_OBSERVER = 1;
	static const int STAGE_DISMISS = 2;

	protected int m_iStage = STAGE_SPAWN;
	protected int m_iPolls;

	protected string m_sGroupId;
	protected EntityID m_GroupEntityId;
	protected int m_iObserversBefore;
	protected int m_iExpectedMembers;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iStage == STAGE_SPAWN)
			return SpawnStage();

		if (m_iStage == STAGE_OBSERVER)
			return ObserverStage();

		return DismissStage();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the group and proves claims (a) and (b).
	//! \return True when the case is finished, false to poll again next frame.
	protected bool SpawnStage()
	{
		OVT_HighCommandManagerComponent manager = OVT_Global.GetHighCommand();
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!manager || !virtualization || !config)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_MANAGER_POLLS)
			{
				SetFailure("The High Command manager, the virtualization manager or the Overthrow config was still null after %1 frames - this case cannot say anything about High Command groups either way", m_iPolls.ToString());
				return true;
			}

			return false;
		}

		if (!Replication.IsServer())
		{
			SetFailure("This world is not the server, so no group can be spawned - the Init suite is expected to run on a listen host");
			return true;
		}

		ResourceName sourcePrefab;
		array<ResourceName> slots = {};
		if (!PickSourceGroupPrefab(config, sourcePrefab, slots))
		{
			SetFailure("No faction group prefab in this world resolved to a non-empty m_aUnitPrefabSlots, so there is no composition to buy a High Command group from");
			return true;
		}

		m_iExpectedMembers = slots.Count();

		vector position = OVT_TEST_VirtualizationFixture.PickPosition();
		if (position == vector.Zero)
		{
			SetFailure("This world defines no town to spawn a High Command group beside");
			return true;
		}

		m_iObserversBefore = virtualization.GetEntityObserverCount();

		OVT_HighCommandRecord record = manager.SpawnGroup("test_entry", sourcePrefab, ResourceName.Empty, position, "test_owner");
		if (!record)
		{
			SetFailure("OVT_HighCommandManagerComponent.SpawnGroup() returned null for a group prefab with %1 member slot(s). Either the manager's m_sGroupPrefab is unset on Prefabs/GameMode/OVT_OverthrowGameMode.et, or that prefab is not carrying OVT_HighCommandGroupComponent - in both cases no group can ever be bought.", m_iExpectedMembers.ToString());
			return true;
		}

		m_sGroupId = record.m_sGroupId;
		m_GroupEntityId = record.m_GroupEntityId;

		SCR_AIGroup group = manager.GetGroupEntity(record);
		if (!group)
		{
			SetFailure("SpawnGroup() produced a record whose group entity does not resolve - the record and the world disagree from the first frame");
			Dismiss(manager);
			return true;
		}

		// (a) the composition arrived intact.
		if (group.GetAgentsCount() != m_iExpectedMembers)
		{
			SetFailure("The High Command group holds %1 agent(s) but its source prefab defines %2 member slot(s) - a member that spawned without joining is a loose armed character and a group short of what the player paid for", group.GetAgentsCount().ToString(), m_iExpectedMembers.ToString());
			Dismiss(manager);
			return true;
		}

		if (record.m_iTotalMembers != m_iExpectedMembers)
		{
			SetFailure("The record claims %1 member(s) while the source prefab defines %2 - the member cap is counted from the record, so it would let a player buy past it", record.m_iTotalMembers.ToString(), m_iExpectedMembers.ToString());
			Dismiss(manager);
			return true;
		}

		// (b) every member fights for the resistance.
		string wrongFaction = FindMemberWithWrongFaction(group, config.m_sPlayerFaction);
		if (wrongFaction != "")
		{
			SetFailure("A High Command member is affiliated '%1' instead of the configured player faction '%2' - anything but the player faction is friendly to the occupiers and never opens fire (BUG-146)", wrongFaction, config.m_sPlayerFaction);
			Dismiss(manager);
			return true;
		}

		m_iStage = STAGE_OBSERVER;
		m_iPolls = 0;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Proves claim (c) - the deferred install landed.
	//! \return True when the case is finished, false to poll again next frame.
	protected bool ObserverStage()
	{
		OVT_HighCommandManagerComponent manager = OVT_Global.GetHighCommand();
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!manager || !virtualization)
		{
			SetFailure("A manager went null between spawning the High Command group and checking its observer");
			return true;
		}

		IEntity groupEntity = GetGame().GetWorld().FindEntityByID(m_GroupEntityId);
		if (!groupEntity)
		{
			SetFailure("The High Command group entity vanished within a frame of being spawned - m_bDeleteWhenEmpty deletes a group whose last member LEAVES, so this means the members never really joined");
			Dismiss(manager);
			return true;
		}

		if (!virtualization.HasEntityObserver(groupEntity))
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_OBSERVER_POLLS)
			{
				SetFailure("The High Command group is still not an AI observer %1 frames after it was spawned. The install is one CallLater(.., 0) hop, so either OVT_HighCommandGroupComponent is missing from the group prefab, or the m_bHighCommandGroupsAreObservers gate is off - a group that wakes nothing stands beside dormant patrols in a town that never comes alive.", m_iPolls.ToString());
				Dismiss(manager);
				return true;
			}

			return false;
		}

		m_iStage = STAGE_DISMISS;
		m_iPolls = 0;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Proves claim (d) - dismissal takes the observer, the entity and the record with it.
	//! \return True - the case always ends here.
	protected bool DismissStage()
	{
		OVT_HighCommandManagerComponent manager = OVT_Global.GetHighCommand();
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!manager || !virtualization)
		{
			SetFailure("A manager went null before the High Command group could be dismissed - the group is still standing in this world");
			return true;
		}

		if (!manager.DismissGroup(m_sGroupId))
		{
			SetFailure("DismissGroup() refused a group it had just spawned");
			return true;
		}

		// Re-resolved by ID, never through the pointer captured before the delete.
		IEntity groupEntity = GetGame().GetWorld().FindEntityByID(m_GroupEntityId);
		if (groupEntity)
		{
			SetFailure("The High Command group entity still resolves after DismissGroup() - its members, its waypoints and its observer are all still in the world");
			return true;
		}

		if (virtualization.HasEntityObserver(groupEntity))
		{
			SetFailure("HasEntityObserver() still answers true for the dismissed group");
			return true;
		}

		int observersAfter = virtualization.GetEntityObserverCount();
		if (observersAfter > m_iObserversBefore)
		{
			SetFailure("Core is holding %1 entity observer(s) after the dismissal, up from %2 before the group was spawned - a leaked observer keeps every registered group near that spot materialised for the rest of the session, and nobody can name the key that would remove it", observersAfter.ToString(), m_iObserversBefore.ToString());
			return true;
		}

		if (manager.GetGroup(m_sGroupId))
		{
			SetFailure("The dismissed group's record is still in the table - it would count against its owner's member cap and draw a marker on every map for a group that no longer exists");
			return true;
		}

		PrintFormat("High Command seam: a %1-man group spawned, observed, and dismissed cleanly (observers %2 -> %3)", m_iExpectedMembers.ToString(), m_iObserversBefore.ToString(), observersAfter.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the group back out of the world on a failure exit, so one red case does not leave an
	//! armed squad standing for every case after it.
	//! \param[in] manager The High Command manager.
	protected void Dismiss(notnull OVT_HighCommandManagerComponent manager)
	{
		if (m_sGroupId != "")
			manager.DismissGroup(m_sGroupId);
	}

	//------------------------------------------------------------------------------------------------
	//! The first faction group prefab in this world with a real composition.
	//!
	//! Read straight off the prefab source, INDEPENDENTLY of the manager's own composition reader -
	//! an expectation derived from the code under test would agree with it however wrong it is.
	//! \param[in] config The Overthrow config, for the player faction key.
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

	//------------------------------------------------------------------------------------------------
	//! \param[in] group The spawned group.
	//! \param[in] expectedFactionKey The configured player faction key.
	//! \return The first wrong faction key found (or "none" for a member with no affiliation at all),
	//! or an empty string when every member is correct.
	protected string FindMemberWithWrongFaction(notnull SCR_AIGroup group, string expectedFactionKey)
	{
		array<AIAgent> agents = {};
		group.GetAgents(agents);

		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity character = agent.GetControlledEntity();
			if (!character)
				continue;

			SCR_CharacterFactionAffiliationComponent affiliation = SCR_CharacterFactionAffiliationComponent.Cast(
				character.FindComponent(SCR_CharacterFactionAffiliationComponent)
			);

			if (!affiliation)
				return "none";

			Faction affiliated = affiliation.GetAffiliatedFaction();
			if (!affiliated)
				return "none";

			if (affiliated.GetFactionKey() != expectedFactionKey)
				return affiliated.GetFactionKey();
		}

		return "";
	}
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
[Test(suite: OVT_TEST_InitSuite, timeoutS: 120)]
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
//! QRF VERIFICATION, NO QRF CODE (T12.2). OVT_QRFControllerComponent.CheckUpdatePoints() counts a
//! resistance-faction agent toward zone control on exactly two conditions: its faction key equals
//! m_Config.m_sPlayerFaction, and it is alive and conscious (IsFightingFit(): GetLifeState() ==
//! ALIVE and !IsUnconscious()). This case proves a freshly spawned High Command member answers both
//! the same way QRF would ask them, without touching OVT_QRFControllerComponent.c at all.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 90)]
class OVT_TEST_Init_HighCommandSeam_FMembersCountForQRF : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the managers to post-init.
	static const int MAX_MANAGER_POLLS = 300;

	protected string m_sGroupId;
	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_HighCommandManagerComponent manager = OVT_Global.GetHighCommand();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!manager || !config)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_MANAGER_POLLS)
			{
				SetFailure("The High Command manager or the Overthrow config was still null after %1 frames - this case cannot say anything about QRF zone counting either way", m_iPolls.ToString());
				return true;
			}

			return false;
		}

		if (!Replication.IsServer())
		{
			SetFailure("This world is not the server, so no group can be spawned - the Init suite is expected to run on a listen host");
			return true;
		}

		ResourceName sourcePrefab;
		array<ResourceName> slots = {};
		if (!PickSourceGroupPrefab(config, sourcePrefab, slots))
		{
			SetFailure("No faction group prefab in this world resolved to a non-empty m_aUnitPrefabSlots, so there is no composition to spawn a High Command group from");
			return true;
		}

		vector position = OVT_TEST_VirtualizationFixture.PickPosition();
		if (position == vector.Zero)
		{
			SetFailure("This world defines no town to spawn a High Command group beside");
			return true;
		}

		OVT_HighCommandRecord record = manager.SpawnGroup("test_qrf", sourcePrefab, ResourceName.Empty, position, "test_qrf_owner");
		if (!record)
		{
			SetFailure("OVT_HighCommandManagerComponent.SpawnGroup() returned null for a group prefab with %1 member slot(s)", slots.Count().ToString());
			return true;
		}

		m_sGroupId = record.m_sGroupId;

		SCR_AIGroup group = manager.GetGroupEntity(record);
		if (!group)
		{
			SetFailure("SpawnGroup() produced a record whose group entity does not resolve");
			return CleanUp(manager);
		}

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		if (agents.IsEmpty())
		{
			SetFailure("The spawned High Command group has no members - there is nothing here for QRF to count");
			return CleanUp(manager);
		}

		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity member = agent.GetControlledEntity();
			if (!member)
			{
				SetFailure("A High Command group member's AI agent has no controlled entity");
				return CleanUp(manager);
			}

			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(member);
			if (!character)
			{
				SetFailure("A High Command group member is not a SCR_ChimeraCharacter, so CheckUpdatePoints()'s own SCR_ChimeraCharacter.Cast() would skip it and it would never count toward the zone");
				return CleanUp(manager);
			}

			// Condition 1 of 2: the faction key CheckUpdatePoints compares against m_Config.m_sPlayerFaction.
			if (character.GetFactionKey() != config.m_sPlayerFaction)
			{
				SetFailure("A High Command member's faction key is '%1' but the configured player faction is '%2' - QRF would never count this member toward resistance zone control", character.GetFactionKey(), config.m_sPlayerFaction);
				return CleanUp(manager);
			}

			// Condition 2 of 2: IsFightingFit()'s own test, re-implemented here rather than called, so
			// this case proves something rather than exercising the QRF file it is forbidden to touch.
			CharacterControllerComponent controller = CharacterControllerComponent.Cast(member.FindComponent(CharacterControllerComponent));
			if (!controller)
			{
				SetFailure("A High Command member has no CharacterControllerComponent - QRF's IsFightingFit() would answer false and never count it");
				return CleanUp(manager);
			}

			if (controller.GetLifeState() != ECharacterLifeState.ALIVE)
			{
				SetFailure("A freshly spawned High Command member's life state is '%1', not ALIVE - QRF's IsFightingFit() would never count it", typename.EnumToString(ECharacterLifeState, controller.GetLifeState()));
				return CleanUp(manager);
			}

			if (controller.IsUnconscious())
			{
				SetFailure("A freshly spawned High Command member is unconscious - QRF's IsFightingFit() would never count it");
				return CleanUp(manager);
			}
		}

		PrintFormat("High Command seam: %1 member(s) of a freshly spawned group each answer QRF's own two conditions (faction '%2', alive and conscious)", agents.Count().ToString(), config.m_sPlayerFaction);

		return CleanUp(manager);
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the group back out of the world so one case does not leave an armed squad standing.
	//! \param[in] manager The High Command manager.
	//! \return Always true - the case is over.
	protected bool CleanUp(notnull OVT_HighCommandManagerComponent manager)
	{
		if (m_sGroupId != "")
			manager.DismissGroup(m_sGroupId);

		return true;
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
//! Converting a parked recruit squad produces ONE observed High Command group and leaves nothing
//! behind (Phase 9, T9.1-T9.3).
//!
//! SIX CLAIMS, EACH ONE A SILENT FAILURE OF ITS OWN:
//!   a) the recruits' OWN bodies are what the new group holds - a conversion that spawned fresh men
//!      would look identical on the map and would have thrown away every recruit's gear;
//!   b) the recruit records are gone, so the men left the recruit cap in the same operation;
//!   c) the record names an entry the catalog still answers for, or the restore walk drops the whole
//!      group on the next load with nothing but an ERROR in the log;
//!   d) the new group is an AI observer, and the emptied recruit group and its observer are gone -
//!      EXACTLY ONE entity observer more than before the fixture existed. Two would mean the old
//!      group's observer was orphaned; none would mean the new group wakes nothing;
//!   e) dismissing the converted group puts the observer count back where it started;
//!   f) every converted body is persistence-TRACKED, which is the whole gear promise: an untracked
//!      body has no persistence id, so the saved roster names nothing for that man and he reloads as
//!      a fresh prefab in civilian clothes (D8/T6.7).
//!
//! ⚠ OBSERVER PRESENCE IS READ FROM CORE'S OWN MAP (HasEntityObserver / GetEntityObserverCount),
//! never from ObserversSystem: engine application is deferred a frame in BOTH directions, so an
//! engine query reads a false negative right after an add (virtualization/core api.md §3).
//!
//! The +1 claim is gate-independent: the baseline is taken BEFORE the recruits are parked, so it
//! holds whether or not m_bRecruitGroupsAreObservers made the inactive group an observer too.
//!
//! THE POLLS ARE PRECONDITIONS, NOT RETRIES. Each expiry is its own named failure.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 120)]
class OVT_TEST_Init_HighCommandSeam_EConvertedRecruitsBecomeOneObservedGroup : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the managers to post-init.
	static const int MAX_MANAGER_POLLS = 300;

	//! Frame polls allowed for a freshly spawned body to be given its AI agent.
	static const int MAX_AGENT_POLLS = 120;

	//! Frame polls allowed for vanilla's delete-when-empty (1 frame) and the deferred observer install
	//! (1 call-queue hop) to both land.
	static const int MAX_SETTLE_POLLS = 60;

	//! Frame polls allowed for persistence registration to land on a promoted body. The same budget
	//! OVT_TEST_Init_Persistence_RecruitedTransientCharacterIsRetracked uses, for the same mechanism.
	static const int MAX_TRACK_POLLS = 900;

	//! Collides with no real player; every record it holds is removed again on cleanup.
	static const string TEST_OWNER_UID = "OVT_TEST_HC_CONVERT_OWNER";

	//! Two, because one man could not tell "the squad moved" apart from "a group was created".
	static const int MEMBER_COUNT = 2;

	static const int STAGE_SPAWN = 0;
	static const int STAGE_PARK = 1;
	static const int STAGE_CONVERT = 2;
	static const int STAGE_TRACKED = 3;
	static const int STAGE_SETTLE = 4;

	protected int m_iStage = STAGE_SPAWN;
	protected int m_iPolls;

	protected int m_iObserversBefore;

	//! Ids, never pointers: an entity re-resolved by id is null after a deletion instead of dangling.
	protected ref array<EntityID> m_aBodyIds = {};
	protected ref array<string> m_aRecruitIds = {};

	protected EntityID m_InactiveGroupEntityId;
	protected EntityID m_GroupEntityId;
	protected string m_sGroupId;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iStage == STAGE_SPAWN)
			return SpawnStage();

		if (m_iStage == STAGE_PARK)
			return ParkStage();

		if (m_iStage == STAGE_CONVERT)
			return ConvertStage();

		if (m_iStage == STAGE_TRACKED)
			return TrackedStage();

		return SettleStage();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the recruit bodies the fixture converts.
	//! \return True when the case is finished, false to poll again next frame.
	protected bool SpawnStage()
	{
		OVT_HighCommandManagerComponent manager = OVT_Global.GetHighCommand();
		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();

		if (!manager || !recruits || !virtualization)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_MANAGER_POLLS)
			{
				SetFailure("The High Command manager, the recruit manager or the virtualization manager was still null after %1 frames - this case cannot say anything about converting recruits either way", m_iPolls.ToString());
				return true;
			}

			return false;
		}

		if (!Replication.IsServer())
		{
			SetFailure("This world is not the server, so nothing can be converted - the Init suite is expected to run on a listen host");
			return true;
		}

		if (recruits.m_sRecruitPrefab.IsEmpty())
		{
			SetFailure("The recruit manager has no character prefab, so this case has nothing to recruit and park");
			return true;
		}

		vector position = OVT_TEST_VirtualizationFixture.PickPosition();
		if (position == vector.Zero)
		{
			SetFailure("This world defines no town to park a recruit squad beside");
			return true;
		}

		m_iObserversBefore = virtualization.GetEntityObserverCount();

		for (int i = 0; i < MEMBER_COUNT; i++)
		{
			// Well inside OVT_RecruitInactiveGrouping.DEFAULT_CLUSTER_RADIUS, so both park in ONE group.
			vector bodyPosition = position;
			bodyPosition[0] = bodyPosition[0] + (i * 3);

			IEntity body = recruits.SpawnRecruit(bodyPosition);
			if (!body)
			{
				SetFailure("The recruit manager could not spawn body %1 of %2 from its own civilian prefab", (i + 1).ToString(), MEMBER_COUNT.ToString());
				return CleanUp();
			}

			m_aBodyIds.Insert(body.GetID());
		}

		m_iStage = STAGE_PARK;
		m_iPolls = 0;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Recruits the bodies and parks them, which is what builds the inactive group to convert.
	//! \return True when the case is finished, false to poll again next frame.
	protected bool ParkStage()
	{
		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		OVT_HighCommandManagerComponent manager = OVT_Global.GetHighCommand();
		if (!recruits || !manager)
		{
			SetFailure("A manager went null between spawning the recruit bodies and parking them");
			return CleanUp();
		}

		// An agent is what AddAIEntityToGroup needs, and it is not there in the spawn frame.
		if (!AllBodiesHaveAgents())
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_AGENT_POLLS)
			{
				SetFailure("A freshly spawned recruit body still had no AI agent after %1 frames, so it could not be put into any group - the fixture never existed", m_iPolls.ToString());
				return CleanUp();
			}

			return false;
		}

		foreach (EntityID bodyId : m_aBodyIds)
		{
			IEntity body = GetGame().GetWorld().FindEntityByID(bodyId);
			if (!body)
			{
				SetFailure("A recruit body vanished between being spawned and being recruited");
				return CleanUp();
			}

			string recruitId = recruits.AddRecruit(TEST_OWNER_UID, body);
			if (recruitId == "")
			{
				SetFailure("AddRecruit() returned no id for a spawned body - is the test owner already at the recruit cap?");
				return CleanUp();
			}

			m_aRecruitIds.Insert(recruitId);

			if (!recruits.SetRecruitInactive(recruitId, true))
			{
				SetFailure("SetRecruitInactive() refused to park recruit '%1', so there is no inactive group to convert", recruitId);
				return CleanUp();
			}
		}

		SCR_AIGroup hostGroup = manager.FindInactiveRecruitGroup(m_aRecruitIds[0]);
		if (!hostGroup)
		{
			SetFailure("The parked recruits are not in a group carrying OVT_InactiveRecruitGroupComponent, so the conversion seam has nothing to find");
			return CleanUp();
		}

		foreach (string recruitId : m_aRecruitIds)
		{
			if (manager.FindInactiveRecruitGroup(recruitId) == hostGroup)
				continue;

			SetFailure("Recruit '%1' was parked 3 m from the others but did not share their inactive group - this case needs one multi-man squad to tell 'the squad moved' apart from 'a group was created'", recruitId);
			return CleanUp();
		}

		m_InactiveGroupEntityId = hostGroup.GetID();

		m_iStage = STAGE_CONVERT;
		m_iPolls = 0;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Converts the squad and proves claims (a), (b) and (c).
	//! \return True when the case is finished, false to poll again next frame.
	protected bool ConvertStage()
	{
		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		OVT_HighCommandManagerComponent manager = OVT_Global.GetHighCommand();
		if (!recruits || !manager)
		{
			SetFailure("A manager went null between parking the recruits and converting them");
			return CleanUp();
		}

		SCR_AIGroup hostGroup = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(m_InactiveGroupEntityId));
		if (!hostGroup)
		{
			SetFailure("The inactive recruit group vanished before it could be converted");
			return CleanUp();
		}

		array<string> recruitIds = {};
		array<IEntity> bodies = {};
		int collected = manager.CollectInactiveGroupRecruits(TEST_OWNER_UID, hostGroup, recruitIds, bodies);
		if (collected != MEMBER_COUNT)
		{
			SetFailure("CollectInactiveGroupRecruits() found %1 recruit(s) in a group holding %2 - the conversion would take only part of the squad and leave the rest parked", collected.ToString(), MEMBER_COUNT.ToString());
			return CleanUp();
		}

		OVT_HighCommandRecord record = manager.ConvertRecruitGroup(TEST_OWNER_UID, recruitIds, bodies);
		if (!record)
		{
			SetFailure("ConvertRecruitGroup() returned null for a %1-man parked squad - the recruits are still recruits and nothing was converted", MEMBER_COUNT.ToString());
			return CleanUp();
		}

		m_sGroupId = record.m_sGroupId;
		m_GroupEntityId = record.m_GroupEntityId;

		if (record.m_iTotalMembers != MEMBER_COUNT)
		{
			SetFailure("The converted group's record claims %1 member(s) against the %2 that were parked - the member cap is counted from the record, so it would be wrong from the first frame", record.m_iTotalMembers.ToString(), MEMBER_COUNT.ToString());
			return CleanUp();
		}

		// (a) THE SAME BODIES. A conversion that spawned fresh men would pass every count above.
		foreach (EntityID bodyId : m_aBodyIds)
		{
			IEntity body = GetGame().GetWorld().FindEntityByID(bodyId);
			if (!body)
			{
				SetFailure("A converted recruit's body is gone from the world - the conversion destroyed the very bodies whose gear it exists to keep");
				return CleanUp();
			}

			if (manager.GetGroupIdFromMemberEntity(body) != m_sGroupId)
			{
				SetFailure("A converted recruit's body is not registered as a member of High Command group '%1' - the modded SCR_AIGroup hooks untrack it the moment it joins, and an untracked body loses the persistence id that IS its gear", m_sGroupId);
				return CleanUp();
			}

			if (ParentGroupOf(body) != m_GroupEntityId)
			{
				SetFailure("A converted recruit's body did not actually join the High Command group entity - it is standing in some other group, or in none");
				return CleanUp();
			}
		}

		// (b) THEY LEFT THE RECRUIT CAP.
		foreach (string recruitId : m_aRecruitIds)
		{
			if (recruits.GetRecruit(recruitId))
			{
				SetFailure("Recruit '%1' still has a record after being converted - it counts against its owner's recruit cap and still draws a roster row for a man who is now under High Command", recruitId);
				return CleanUp();
			}
		}

		if (recruits.GetRecruitCount(TEST_OWNER_UID) != 0)
		{
			SetFailure("The test owner still holds %1 recruit(s) after converting all of them", recruits.GetRecruitCount(TEST_OWNER_UID).ToString());
			return CleanUp();
		}

		// (c) IT WILL SURVIVE THE NEXT LOAD. RestoreGroup drops any record whose entry key the catalog
		// no longer answers for, and a converted group names no purchasable entry.
		if (record.m_sEntryKey != OVT_HighCommandRules.CONVERTED_ENTRY_KEY)
		{
			SetFailure("A converted group's record names the entry '%1' instead of the reserved converted key", record.m_sEntryKey);
			return CleanUp();
		}

		if (!manager.GetEntryByKey(record.m_sEntryKey))
		{
			SetFailure("Nothing answers GetEntryByKey('%1'), so the restore walk would drop this whole group on the next load with an ERROR and no other symptom", record.m_sEntryKey);
			return CleanUp();
		}

		m_iStage = STAGE_TRACKED;
		m_iPolls = 0;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Proves the gear claim: every converted body is under persistence tracking.
	//!
	//! WHY IT IS ITS OWN STAGE. This is what "converted recruits keep their gear" actually rests on -
	//! an untracked body has no persistence id, so the save writes nothing for it and the reload puts a
	//! fresh prefab stand-in in its place wearing civilian clothes (D8/T6.7). Registration is LAZY and
	//! lands frames after a promotion, so it is polled.
	//!
	//! Skipped, with a line in the log, in a world with no persistence system - IsTracked() answers
	//! false for every entity there, and a claim nothing can satisfy is not a claim.
	//! \return True when the case is finished, false to poll again next frame.
	protected bool TrackedStage()
	{
		if (!SCR_PersistenceSystem.GetScriptedInstance())
		{
			Print("High Command seam: this world has no persistence system, so the converted bodies' tracking claim was skipped");
			m_iStage = STAGE_SETTLE;
			m_iPolls = 0;
			return false;
		}

		if (!AllBodiesAreTracked())
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_TRACK_POLLS)
			{
				SetFailure("A converted recruit's body is still not persistence-tracked %1 frames after the conversion. An untracked body has no persistence id, so the group's saved roster names nothing for that man and he reloads as a fresh prefab in civilian clothes - which is the whole gear promise of the conversion, failing silently.", m_iPolls.ToString());
				return CleanUp();
			}

			return false;
		}

		m_iStage = STAGE_SETTLE;
		m_iPolls = 0;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Proves claims (d) and (e) once the frame-deferred halves have landed.
	//! \return True when the case is finished, false to poll again next frame.
	protected bool SettleStage()
	{
		OVT_HighCommandManagerComponent manager = OVT_Global.GetHighCommand();
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!manager || !virtualization)
		{
			SetFailure("A manager went null before the converted group's observer could be checked");
			return CleanUp();
		}

		IEntity groupEntity = GetGame().GetWorld().FindEntityByID(m_GroupEntityId);
		if (!groupEntity)
		{
			SetFailure("The converted High Command group entity vanished within a frame - m_bDeleteWhenEmpty deletes a group whose last member LEAVES, so this means the bodies never really joined it");
			return CleanUp();
		}

		IEntity oldGroupEntity = GetGame().GetWorld().FindEntityByID(m_InactiveGroupEntityId);

		if (oldGroupEntity || !virtualization.HasEntityObserver(groupEntity))
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_SETTLE_POLLS)
			{
				if (oldGroupEntity)
					SetFailure("The emptied recruit group still stands %1 frames after the conversion took its last member - its hold waypoints and its AI observer are still in the world, and nothing else will ever remove them", m_iPolls.ToString());
				else
					SetFailure("The converted group is still not an AI observer %1 frames after it was created. The install is one CallLater(.., 0) hop, so either OVT_HighCommandGroupComponent is missing from the group prefab or the m_bHighCommandGroupsAreObservers gate is off - a group that wakes nothing stands beside dormant patrols in a town that never comes alive.", m_iPolls.ToString());

				return CleanUp();
			}

			return false;
		}

		// (d) EXACTLY ONE MORE THAN THE FIXTURE STARTED WITH.
		int observersAfter = virtualization.GetEntityObserverCount();
		if (observersAfter != m_iObserversBefore + 1)
		{
			SetFailure("Core holds %1 entity observer(s) after the conversion, against the %2 it held before the recruits were parked. Exactly one more is correct - one converted group, one observer. More means the emptied recruit group's observer was orphaned and keeps everything near that spot materialised for the rest of the session.", observersAfter.ToString(), (m_iObserversBefore + 1).ToString());
			return CleanUp();
		}

		// (e) AND IT ALL COMES BACK OUT AGAIN.
		if (!manager.DismissGroup(m_sGroupId))
		{
			SetFailure("DismissGroup() refused the group it had just converted");
			return CleanUp();
		}

		m_sGroupId = "";

		int observersFinal = virtualization.GetEntityObserverCount();
		if (observersFinal != m_iObserversBefore)
		{
			SetFailure("Core holds %1 entity observer(s) after the converted group was dismissed, against the %2 it held before the fixture existed", observersFinal.ToString(), m_iObserversBefore.ToString());
			return CleanUp();
		}

		// PrintFormat takes at most three string parameters, so the observer trace is one of them.
		string observerTrace = m_iObserversBefore.ToString() + " -> " + observersAfter.ToString() + " -> " + observersFinal.ToString();
		PrintFormat("High Command seam: a %1-man parked recruit squad converted into one observed High Command group (observers %2)",
			MEMBER_COUNT.ToString(),
			observerTrace);

		return CleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when every converted body is under persistence tracking.
	protected bool AllBodiesAreTracked()
	{
		foreach (EntityID bodyId : m_aBodyIds)
		{
			IEntity body = GetGame().GetWorld().FindEntityByID(bodyId);
			if (!body)
				return false;

			if (!OVT_PersistenceTracking.IsTracked(body))
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when every spawned body has an AI agent.
	protected bool AllBodiesHaveAgents()
	{
		foreach (EntityID bodyId : m_aBodyIds)
		{
			IEntity body = GetGame().GetWorld().FindEntityByID(bodyId);
			if (!body)
				return false;

			AIControlComponent control = AIControlComponent.Cast(body.FindComponent(AIControlComponent));
			if (!control)
				return false;

			if (!control.GetAIAgent())
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] body A member body.
	//! \return The entity id of the group its agent belongs to, or EntityID.INVALID.
	protected EntityID ParentGroupOf(notnull IEntity body)
	{
		AIControlComponent control = AIControlComponent.Cast(body.FindComponent(AIControlComponent));
		if (!control)
			return EntityID.INVALID;

		AIAgent agent = control.GetAIAgent();
		if (!agent)
			return EntityID.INVALID;

		SCR_AIGroup group = SCR_AIGroup.Cast(agent.GetParentGroup());
		if (!group)
			return EntityID.INVALID;

		return group.GetID();
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the whole fixture back out of the world, whichever verdict it was: one red case must not
	//! leave an armed squad and a set of recruit records standing for every case after it.
	//! \return Always true - the case is over.
	protected bool CleanUp()
	{
		OVT_HighCommandManagerComponent manager = OVT_Global.GetHighCommand();
		if (manager && m_sGroupId != "")
			manager.DismissGroup(m_sGroupId);

		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		if (recruits)
		{
			foreach (string recruitId : m_aRecruitIds)
			{
				recruits.RemoveRecruit(recruitId);
			}
		}

		foreach (EntityID bodyId : m_aBodyIds)
		{
			IEntity body = GetGame().GetWorld().FindEntityByID(bodyId);
			if (body)
				SCR_EntityHelper.DeleteEntityAndChildren(body);
		}

		IEntity oldGroupEntity = GetGame().GetWorld().FindEntityByID(m_InactiveGroupEntityId);
		if (oldGroupEntity)
			SCR_EntityHelper.DeleteEntityAndChildren(oldGroupEntity);

		m_aRecruitIds.Clear();
		m_aBodyIds.Clear();

		return true;
	}
}
