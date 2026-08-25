//------------------------------------------------------------------------------------------------
//! Infantry spawning module for deployments: registers this deployment's foot groups with the
//! virtualization core and holds their handles.
//!
//! WHAT CHANGED. This module used to spawn real soldiers whenever a player came within 1750 m and
//! delete every one of them again when the last player left, so a patrol shot down to one man came
//! back at full strength as soon as you walked away and returned. It now REGISTERS its groups; the
//! engine materialises and despawns them by observer proximity, and the core's survivor mask
//! remembers which men died - across despawn AND across save/load.
//!
//! EnsureGroups() IS THE WHOLE MODULE. It is written as CONVERGE TO WANTED, never SPAWN WANTED:
//! reclaim from the registry first (FindGroupsByOwner, keyed on the deployment's persisted virtual
//! key), then register only the shortfall. That is what makes it safe from activation, from the
//! manager's records-restored fan-out and from the rebuy path, in any order and any number of times -
//! and it is what stops a continued campaign registering a second copy of every patrol on top of the
//! one the core has already restored.
//!
//! HANDLES ARE NEVER PERSISTED - the owner key is, and the handles are re-found from it.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_InfantrySpawningDeploymentModule : OVT_BaseSpawningDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;

	[Attribute(defvalue: "light_patrol", desc: "Group type name from faction registry")]
	string m_sGroupType;

	[Attribute(defvalue: "1", desc: "Minimum number of groups to spawn")]
	int m_iMinGroupCount;

	[Attribute(defvalue: "3", desc: "Maximum number of groups to spawn")]
	int m_iMaxGroupCount;

	[Attribute(defvalue: "false", desc: "Scale group count by nearest town size")]
	bool m_bScaleByTownSize;

	protected int m_iActualGroupCount; // Calculated group count based on difficulty and town size

	[Attribute(defvalue: "50", desc: "Spawn radius around deployment position")]
	float m_fSpawnRadius;

	//! ⚠ DEFAULT TRUE = TODAY'S BEHAVIOUR, and it must stay that way: the four shipped configs author
	//! nothing here and have to keep registering exactly where they always did.
	//!
	//! WHY THE OPT-OUT EXISTS. The road snap is not bounded by m_fSpawnRadius. The ring roll picks a
	//! point 10..m_fSpawnRadius m from the anchor and OVT_WorldUtils.FindNearestRoad then searches
	//! 500 m for a road waypoint, so m_fSpawnRadius bounds the ROLL and not the RESULT: a garrison
	//! authored at 15 m can end up hundreds of metres away on the access road, and with a DEFEND or
	//! null plan it then holds there rather than walking back. That is right for a patrol meant to
	//! use roads and wrong for anything garrisoning a PLACE.
	[Attribute(defvalue: "1", desc: "Snap each registration to the nearest road. TRUE is the shipped behaviour. Set FALSE for anything that garrisons a PLACE - the snap searches up to 500 m and is NOT bounded by the spawn radius")]
	bool m_bSnapToRoad;

	[Attribute(defvalue: "30", desc: "Resource cost per group")]
	int m_iCostPerGroup;

	[Attribute(defvalue: "true", desc: "Allow reinforcement when groups are destroyed")]
	bool m_bAllowReinforcement;

	[Attribute(defvalue: "15", desc: "Reinforcement cost per group")]
	int m_iReinforcementCost;

	[Attribute(defvalue: "false", desc: "Spawn initial groups at nearest faction-controlled base instead of deployment location")]
	bool m_bSpawnAtNearestBase;

	[Attribute(defvalue: "true", desc: "Spawn reinforcement groups at nearest faction-controlled base instead of deployment location")]
	bool m_bReinforceFromNearestBase;

	//! NEVER leave a registration unstamped: an unstamped group inherits vanilla's LOW tier, is capped
	//! at half the AI budget and is evicted first - exactly how a garrison silently fails to appear.
	[Attribute(defvalue: "1", UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(SCR_EAISpawnImportance), desc: "AI spawn-budget tier for this module's groups (LOW 0.50 of budget, NORMAL 0.70, HIGH 0.90, CRITICAL 1.00). Garrisons the player travelled to fight should be HIGH")]
	SCR_EAISpawnImportance m_eImportance;

	//! OWNER_SYSTEM and SPAWN_DISTANCE_GLOBAL live on OVT_BaseSpawningDeploymentModule - the vehicle
	//! module registers under the same owner system, and two spawning modules disagreeing about that
	//! string would each be unable to reclaim the other's groups.

	//! Registry handles this module currently holds. NOT persisted and NOT authoritative: the registry
	//! is, and this list is re-derived from it by every convergence pass.
	protected ref array<int> m_aHandles;

	//! High-water mark of how many groups this module has ever held, so an empty handle list can be
	//! told apart from "never had any". Monotonic - a wipe never lowers it.
	protected int m_iSpawnedEver;

	//------------------------------------------------------------------------------------------------
	void OVT_InfantrySpawningDeploymentModule()
	{
		m_aHandles = new array<int>();
		m_iSpawnedEver = 0;
	}

	//------------------------------------------------------------------------------------------------
	override int GetResourceCost()
	{
		// Use max group count for resource cost calculation to ensure we have enough resources
		return m_iMaxGroupCount * m_iCostPerGroup;
	}

	//------------------------------------------------------------------------------------------------
	//! THE PER-GROUP PRICE OF EVERY GROUP THAT IS STILL AT FULL STRENGTH.
	//!
	//! Author's rule, 2026-08-20: *"A fully alive group with all members should get a refund if they were
	//! successful... they still lose the insertion cost of the truck if there was one, and the base cost
	//! (admin costs), but the per-group cost goes back into the deployment pool."*
	//!
	//! ⚠ ALL-OR-NOTHING PER GROUP, NOT PRO-RATA PER MAN, and that is the author's rule rather than a
	//! simplification of it. A group that took casualties has been fought, and the faction does not get
	//! its money back for a fight it had; a group that walked in and out untouched is the one that
	//! genuinely encountered no resistance. Refunding four fifths of a squad that lost a man would pay
	//! the occupying faction for losing him, which is the same principle
	//! OVT_DeploymentManagerComponent.RecallDeployment() applies when it refuses to refund a force that
	//! was eliminated.
	//!
	//! ⚠ WHAT IS DELIBERATELY NOT REFUNDED, because it is not in m_iCostPerGroup and must not be added:
	//! the transport (m_iTruckCostOverride, on the insertion subclass - the truck was spent getting them
	//! there and is often abandoned at the far end) and the config's m_iBaseCost, which is the operation's
	//! overhead. Both fall out of using the per-group figure alone; neither needs a subtraction.
	//!
	//! ⚠ THE ROSTER SIZE MUST BE NON-ZERO FOR A GROUP TO COUNT AS INTACT. GetMemberCount() answers 0
	//! until the group entity has existed and core has captured its roster, so a group registered but
	//! never materialised reads 0 alive of 0 - and `0 == 0` would refund a full price for men who were
	//! never there. The explicit `> 0` is that guard.
	//!
	//! ⚠ IT READS CORE, WHICH IS THE AUTHORITATIVE SURVIVOR TRUTH. GetAliveMemberCount() consults the
	//! per-slot survivor mask before it consults any engine count, precisely because the engine's dormant
	//! counts corrupt themselves when a despawn lands mid-refill. Counting agents here instead would pay
	//! out on whatever happened to be materialised at the moment the mission finished.
	//! \return Resources to return: intact groups x m_iCostPerGroup.
	override int GetIntactGroupRefund()
	{
		if (m_iCostPerGroup <= 0)
			return 0;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return 0;

		int intact = 0;

		foreach (int handle : m_aHandles)
		{
			int roster = virtualization.GetMemberCount(handle);
			if (roster <= 0)
				continue;

			if (virtualization.GetAliveMemberCount(handle) < roster)
				continue;

			intact++;
		}

		return intact * m_iCostPerGroup;
	}

	//------------------------------------------------------------------------------------------------
	override void OnActivate()
	{
		super.OnActivate();

		// No eliminated check here any more - the convergence owns that gate, so every caller gets the
		// same answer whether it arrives through activation, a restore or a rebuy.
		EnsureGroups();
	}

	//------------------------------------------------------------------------------------------------
	//! DELIBERATELY A NO-OP. Deactivation used to delete every soldier this module had spawned - the ad
	//! hoc virtualization the core replaced - and doing it now would throw away the survivor mask this
	//! whole feature exists to keep. Deactivate() means "stop ticking"; only Cleanup() means "over".
	override void OnDeactivate()
	{
		super.OnDeactivate();
	}

	//------------------------------------------------------------------------------------------------
	//! The deployment is over: release every group back to the core. UnregisterGroup respects held
	//! members, so a group somebody is fighting is retired in place rather than deleted out from under
	//! them, and it takes the group's owned waypoints with it.
	//!
	//! THERE IS NO OnDelete COUNTERPART HERE OR ON THE DEPLOYMENT. One would also fire at
	//! quit-to-menu and would erase the very records persistence exists to keep.
	override protected void OnCleanup()
	{
		super.OnCleanup();

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (virtualization)
		{
			// Reclaim before releasing. A deployment deleted before this module ever converged - a
			// restored one whose condition failed on its first evaluation, say - holds an empty list
			// while the registry still holds its groups, and those would be orphaned for the rest of
			// the campaign with no owner left to find them.
			string ownerKey = GetOwnerKey();
			if (!ownerKey.IsEmpty())
				ReclaimHandles(virtualization, ownerKey);

			foreach (int handle : m_aHandles)
			{
				virtualization.UnregisterGroup(handle);
			}
		}

		m_aHandles.Clear();
	}

	//------------------------------------------------------------------------------------------------
	override array<IEntity> GetSpawnedEntities()
	{
		array<IEntity> entities = new array<IEntity>;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return entities;

		foreach (int handle : m_aHandles)
		{
			// "Record exists, entity does not" is a legitimate runtime state, so null is skipped.
			SCR_AIGroup group = virtualization.GetGroup(handle);
			if (group)
				entities.Insert(group);
		}

		return entities;
	}

	//------------------------------------------------------------------------------------------------
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_InfantrySpawningDeploymentModule clone = new OVT_InfantrySpawningDeploymentModule();

		// EVERY attribute has to appear here - a forgotten one silently ships the class default instead
		// of the authored value (that is how m_fMaxCruiseSpeed was lost on the vehicle module). Forget
		// m_eImportance and a HIGH-tier garrison ships at NORMAL and quietly loses the AI budget race.
		//
		// ⚠ EVERY SUBCLASS MUST COPY ALL OF THESE TOO. CloneModule is not chained - a subclass builds
		// its own instance and copies by hand - so the list below has to be reproduced in full by
		// OVT_PlacedInfantrySpawningDeploymentModule and OVT_CompositionSpawningDeploymentModule
		// before they add their own. Anything appended here has to be appended there as well.
		clone.m_sModuleName = m_sModuleName;
		clone.m_sGroupType = m_sGroupType;
		clone.m_iMinGroupCount = m_iMinGroupCount;
		clone.m_iMaxGroupCount = m_iMaxGroupCount;
		clone.m_bScaleByTownSize = m_bScaleByTownSize;
		clone.m_fSpawnRadius = m_fSpawnRadius;
		clone.m_iCostPerGroup = m_iCostPerGroup;
		clone.m_bAllowReinforcement = m_bAllowReinforcement;
		clone.m_iReinforcementCost = m_iReinforcementCost;
		clone.m_bSpawnAtNearestBase = m_bSpawnAtNearestBase;
		clone.m_bReinforceFromNearestBase = m_bReinforceFromNearestBase;
		clone.m_eImportance = m_eImportance;
		clone.m_bSnapToRoad = m_bSnapToRoad;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	// Virtualization
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Brings this module's registered groups up to the count it wants. ALWAYS SAFE TO CALL.
	//! ⚠ A GAME-START SEEDING IGNORES m_bSpawnAtNearestBase, ON PURPOSE. Author's rule, 2026-08-20:
	//! "they shouldnt spawn at nearest base at game start (if free at game start = true they should
	//! spawn in the town)". A campaign that opens with every town's patrol walking in from a base opens
	//! with every town empty, which is not what a map the occupying faction has held for years should
	//! look like. A patrol BOUGHT later is reinforcement arriving from somewhere real and should travel;
	//! the founding garrison is simply already there.
	//!
	//! ⚠ THE QUESTION IS ASKED OF THE DEPLOYMENT, NOT OF THE CONFIG. config.m_bFreeAtGameStart would be
	//! the wrong test - the same config is bought again by the evaluator mid-campaign, and answering it
	//! would ground every one of those purchases too. See OVT_DeploymentComponent.WasSeededAtGameStart().
	override void EnsureGroups()
	{
		bool fromNearestBase = m_bSpawnAtNearestBase;

		if (fromNearestBase && m_ParentDeployment && m_ParentDeployment.WasSeededAtGameStart())
			fromNearestBase = false;

		ConvergeGroups(fromNearestBase);
	}

	//------------------------------------------------------------------------------------------------
	//! Reclaim, then register the shortfall. The whole of this module's registration behaviour, and the
	//! one path both activation and the paid-for rebuy go through.
	//!
	//! Every step is load-bearing: RECLAIM first, so a restore, a re-apply and an activation converge on
	//! the SAME groups instead of stacking a second force on the first; STOP IF WIPED, because a
	//! deployment whose force was killed does not get a new one on any path except a rebuy; and take the
	//! wanted count from GetMaxGroupCount(), which rolls ONCE and remembers, so a re-roll cannot quietly
	//! grow or shrink a force that already exists.
	//! \param[in] fromNearestBase Anchor new groups on the nearest controlled base rather than on the
	//!            deployment. Initial registrations and rebuys author this separately.
	//! \return How many groups this pass newly registered; 0 when it only reclaimed.
	protected int ConvergeGroups(bool fromNearestBase)
	{
		if (!m_ParentDeployment)
			return 0;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return 0;

		string ownerKey = GetOwnerKey();
		if (ownerKey.IsEmpty())
			return 0;

		ReclaimHandles(virtualization, ownerKey);

		if (m_bSpawnedUnitsEliminated || m_ParentDeployment.GetSpawnedUnitsEliminated())
			return 0;

		int missing = OVT_DeploymentVirtualKey.MissingCount(GetMaxGroupCount(), m_aHandles.Count());
		if (missing <= 0)
			return 0;

		// 🔴 THE LINE THAT NAMES THE PATH when men appear next to somebody (author, 2026-08-25: "3
		// snipers spawning meters from me on a roof"). Convergence is the ONLY route to a fresh group
		// that carries no proximity gate of its own - the creation gate and the rebuy gate both sit
		// upstream of it - so if this fires with the resistance a few metres away, this is where they
		// came from and the eliminated flags above are the thing that failed. Costs a sphere query, and
		// only on a pass that is actually about to register.
		OVT_DeploymentLog.Debug(string.Format("[Overthrow] '%1' converging: registering %2 group(s); nearest resistance %3 m; module eliminated=%4 deployment eliminated=%5",
			GetOwnerKey(), missing.ToString(),
			Math.Round(OVT_ResistancePresence.DistanceToNearest(GetDeploymentPosition(), 1000)).ToString(),
			m_bSpawnedUnitsEliminated.ToString(), m_ParentDeployment.GetSpawnedUnitsEliminated().ToString()));

		return RegisterGroups(virtualization, ownerKey, missing, fromNearestBase);
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the handle list from the registry and re-tags every group it finds.
	//!
	//! THE REGISTRY IS THE TRUTH: the list is cleared and re-derived rather than merged into, so a
	//! handle that stopped being ours cannot survive in it. Re-tagging is not optional - the core
	//! re-creates a group's ENTITY on load with a fresh EntityID and the Game Master registry is keyed
	//! on EntityID, so a reclaimed group loses its provenance unless it is tagged again.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] ownerKey This module's owner key.
	protected void ReclaimHandles(notnull OVT_VirtualizationManagerComponent virtualization, string ownerKey)
	{
		m_aHandles.Clear();

		array<int> found = virtualization.FindGroupsByOwner(OWNER_SYSTEM, ownerKey);
		foreach (int foundHandle : found)
		{
			if (!virtualization.IsRegistered(foundHandle))
				continue;

			m_aHandles.Insert(foundHandle);
			TagForGameMaster(virtualization.GetGroup(foundHandle));
			OnGroupReclaimed(foundHandle);
		}

		if (m_aHandles.Count() > m_iSpawnedEver)
			m_iSpawnedEver = m_aHandles.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Registers `count` more groups under this module's owner key.
	//!
	//! Position and plan are resolved per group: the ring-and-road-snap spread is the one the
	//! hand-spawned version used, and the plan comes from whichever behaviour module has an opinion -
	//! asked BEFORE the group exists, because the core builds the waypoints at registration and owns
	//! them from then on.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] ownerKey This module's owner key.
	//! \param[in] count How many groups to add.
	//! \param[in] fromNearestBase Anchor the batch on the nearest controlled base.
	//! \return How many were actually registered.
	protected int RegisterGroups(notnull OVT_VirtualizationManagerComponent virtualization, string ownerKey, int count, bool fromNearestBase)
	{
		int factionIndex = m_ParentDeployment.GetControllingFaction();

		string factionKey = ResolveFactionKey(factionIndex);
		if (factionKey.IsEmpty())
		{
			Print(string.Format("[Overthrow] Deployment '%1': faction index %2 resolves to no faction key, cannot register infantry",
				m_ParentDeployment.GetDeploymentName(), factionIndex), LogLevel.WARNING);
			return 0;
		}

		if (m_sGroupType.IsEmpty())
		{
			Print(string.Format("[Overthrow] Deployment '%1': infantry module has no group type authored",
				m_ParentDeployment.GetDeploymentName()), LogLevel.WARNING);
			return 0;
		}

		vector baseSpawnPos;
		if (!ResolveSpawnAnchor(factionIndex, fromNearestBase, baseSpawnPos))
			return 0;

		int registered = 0;

		for (int i = 0; i < count; i++)
		{
			vector spawnPos = ResolveSpawnPosition(baseSpawnPos, i);
			OVT_VirtualWaypointPlan plan = ResolveVirtualPlan(spawnPos);

			int handle = virtualization.RegisterGroup(OWNER_SYSTEM, ownerKey, factionKey, m_sGroupType,
				spawnPos, plan, ResolveRegistrationSpawnDistance(), m_eImportance);

			if (handle == -1)
			{
				// A refusal is structural - an unresolvable composition, a malformed plan - so every
				// remaining iteration would fail identically. Stop; the next convergence retries.
				Print(string.Format("[Overthrow] Deployment '%1': registration of '%2' (%3) was refused, stopping at %4/%5 groups",
					m_ParentDeployment.GetDeploymentName(), m_sGroupType, factionKey, registered, count), LogLevel.WARNING);
				break;
			}

			m_aHandles.Insert(handle);
			registered++;

			TagForGameMaster(virtualization.GetGroup(handle));
			OnGroupRegistered(handle, spawnPos);
		}

		if (m_aHandles.Count() > m_iSpawnedEver)
			m_iSpawnedEver = m_aHandles.Count();

		return registered;
	}

	//------------------------------------------------------------------------------------------------
	//! One registered group has been wiped out - every slot on its roster is dead.
	//!
	//! FIRED ONLY ON A REAL WIPE. The core raises this off the survivor mask, never off an agent count,
	//! so a group that merely went dormant can no longer be mistaken for a dead one - which is what the
	//! deleted agent-count poll did every time nobody was nearby.
	//! \param[in] handle The wiped group's registry handle.
	override void OnVirtualGroupWiped(int handle)
	{
		int index = m_aHandles.Find(handle);
		if (index == -1)
			return;

		m_aHandles.Remove(index);

		if (!m_aHandles.IsEmpty())
			return;

		// An empty list only means elimination if this module ever held anything.
		if (m_iSpawnedEver <= 0 || m_bSpawnedUnitsEliminated)
			return;

		m_bSpawnedUnitsEliminated = true;

		if (m_ParentDeployment)
			m_ParentDeployment.CheckAllSpawningModulesEliminated();
	}

	//------------------------------------------------------------------------------------------------
	//! The owner key this module's groups are registered under. Composed by the base class from the
	//! deployment's persisted virtual key and this module's authored name.
	//! \return The owner key, or an empty string when there is no deployment to key against.
	protected string GetOwnerKey()
	{
		return BuildOwnerKey(m_sModuleName);
	}

	//------------------------------------------------------------------------------------------------
	//! Where a batch of groups should be registered around.
	//! \param[in] factionIndex The deployment's controlling faction.
	//! \param[in] fromNearestBase Whether the batch comes out of the nearest controlled base.
	//! \param[out] anchor The resolved anchor position.
	//! \return False when a base was asked for and there is no controlled one to use.
	protected bool ResolveSpawnAnchor(int factionIndex, bool fromNearestBase, out vector anchor)
	{
		anchor = m_ParentDeployment.GetPosition();

		if (!fromNearestBase)
			return true;

		vector nearestBasePos = GetNearestControlledBasePosition(factionIndex);
		if (nearestBasePos == vector.Zero)
		{
			Print("No controlled base found, aborting infantry registration", LogLevel.WARNING);
			return false;
		}

		anchor = nearestBasePos;
		OVT_DeploymentLog.Debug(string.Format("Infantry will be registered at nearest base: %1", anchor.ToString()));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! WHERE ONE GROUP OF THIS BATCH GOES. The single seam a subclass overrides to place its groups
	//! somewhere it chose rather than on the shared ring.
	//!
	//! The default is the ring roll (plus the optional road snap), which is what every shipped config
	//! wants. OVT_PlacedInfantrySpawningDeploymentModule overrides it to answer an exact post and
	//! ignores both the ring and the snap.
	//!
	//! ⚠ CALLED ONCE PER GROUP, IN REGISTRATION ORDER, AND ONLY FOR THE SHORTFALL. `index` counts this
	//! batch (0..count-1), NOT this module's groups: a convergence that reclaimed two groups and
	//! registers a third calls this with index 0. A subclass that needs "which group am I placing"
	//! reads m_aHandles.Count(), which is the index the handle about to be created will occupy.
	//! \param[in] anchor The batch anchor - the deployment position, or the nearest controlled base.
	//! \param[in] index Position within THIS batch.
	//! \return The world position to register at.
	protected vector ResolveSpawnPosition(vector anchor, int index)
	{
		return GetRandomSpawnPosition(anchor);
	}

	//------------------------------------------------------------------------------------------------
	//! THE RING THIS MODULE'S GROUPS ARE REGISTERED AT. The single seam a subclass overrides when its
	//! groups need to be materialised somewhere other than the global proximity rule puts them.
	//!
	//! SPAWN_DISTANCE_GLOBAL IS THE SHIPPED ANSWER AND MUST STAY THE DEFAULT: every config that existed
	//! before this seam did registers exactly where and when it always did, because this returns the
	//! literal that used to be written inline at the one call site.
	//!
	//! The one override that exists is the insertion module, whose passengers have to be physically in
	//! a truck that may be driving through country with no player anywhere near it - a group at the
	//! global ring would simply not exist to be seated. It is a heavy thing to ask for (an
	//! always-materialised group costs AI budget for as long as it is registered), which is why it is a
	//! deliberate per-subclass answer and not an attribute anybody can turn on by accident.
	//! \return The spawnDistanceOverride to register with. -1 uses the global virtualization distance.
	protected int ResolveRegistrationSpawnDistance()
	{
		return SPAWN_DISTANCE_GLOBAL;
	}

	//------------------------------------------------------------------------------------------------
	//! One group has just been registered under this module's owner key.
	//!
	//! Empty by design. It is the hook the placement subclass uses to remember a group's post and
	//! subscribe its placement applier; nothing in the base class needs it.
	//! \param[in] handle The new group's registry handle. Already present in m_aHandles.
	//! \param[in] position Where it was registered.
	protected void OnGroupRegistered(int handle, vector position)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! One group has just been RE-FOUND in the registry rather than created.
	//!
	//! Empty by design, and the counterpart to OnGroupRegistered for everything that survived a
	//! despawn or a load. ⚠ A reclaimed group is a NEW ENTITY after a load (core re-creates it from
	//! its own payload with a fresh EntityID), so anything a subclass subscribed to on the old entity
	//! is gone and has to be re-established here - the same reason the base class re-tags for the Game
	//! Master at this exact point.
	//! \param[in] handle The reclaimed group's registry handle. Already present in m_aHandles.
	protected void OnGroupReclaimed(int handle)
	{
	}

	//------------------------------------------------------------------------------------------------
	protected vector GetRandomSpawnPosition(vector center)
	{
		float angle = Math.RandomFloat01() * Math.PI2;
		float distance = Math.RandomFloat(10, m_fSpawnRadius);

		vector offset = Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);
		vector spawnPos = center + offset;

		// The snap is an OPT-OUT, not an opt-in: every config that shipped before m_bSnapToRoad
		// existed authors nothing and must keep snapping. See the attribute's header for why a
		// garrison wants it off - the search is 500 m wide and ignores m_fSpawnRadius entirely.
		if (!m_bSnapToRoad)
			return spawnPos;

		//Find nearest road
		vector roadPos = OVT_WorldUtils.FindNearestRoad(spawnPos);

		return roadPos;
	}

	//------------------------------------------------------------------------------------------------
	protected int CalculateGroupCount(vector position)
	{
		// Get difficulty configuration
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			Print("Failed to get config, using default group count", LogLevel.WARNING);
			return m_iMinGroupCount;
		}

		// Base randomized group count from difficulty settings
		int numGroups = s_AIRandomGenerator.RandInt(Math.Ceil((float)config.m_Difficulty.patrolGroupsMin * 0.5), Math.Ceil((float)config.m_Difficulty.patrolGroupsMax * 0.5));

		// Scale by town size if enabled
		if (m_bScaleByTownSize)
		{
			OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
			if (townManager)
			{
				OVT_TownData nearestTown = townManager.GetNearestTown(position);
				if (nearestTown)
				{
					// Scale by town size: multiply by town size (1-4)
					numGroups = numGroups * nearestTown.size;
					OVT_DeploymentLog.Debug(string.Format("Scaling groups by town size %1: %2 groups", nearestTown.size, numGroups));
				}
			}
		}

		// Clamp to min/max bounds
		numGroups = Math.Clamp(numGroups, m_iMinGroupCount, m_iMaxGroupCount);

		OVT_DeploymentLog.Debug(string.Format("Calculated %1 groups for deployment (min: %2, max: %3)", numGroups, m_iMinGroupCount, m_iMaxGroupCount));

		return numGroups;
	}

	//------------------------------------------------------------------------------------------------
	bool CanReinforce(int groupsNeeded)
	{
		if (!m_bAllowReinforcement)
			return false;

		if (!m_ParentDeployment)
			return false;

		// Check if deployment manager has resources for reinforcement
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
			return false;

		int factionIndex = m_ParentDeployment.GetControllingFaction();

		// ⚠ NOT WHILE A BATTLE IS BEING FOUGHT HERE (occupying/counter-attacks, 2026-08-19). A rebuy is
		// the one path in this framework that puts a NEW force on the ground during a battle, and it is
		// the reason a base can grow fresh tower guards while the player is still fighting for it: the
		// player wipes a module, the reinforcement behaviour notices 60 s later, and the replacement
		// materialises on the spot because the player is standing right there.
		//
		// Refused HERE rather than in the behaviour module because this is the choke point - Reinforce()
		// consults it itself, so every caller present and future is covered by one guard - and refusing
		// before anything is charged is what keeps the pool honest: the manager's suppression tick would
		// pin the new groups dormant anyway, so buying them would be resources leaving the pool with
		// nothing to show for them (BUG-027's shape).
		//
		// SCOPED TO THE BATTLE CIRCLE, so a deployment on the other side of the island rebuys normally.
		// The whole map is NOT frozen - that is the rule the base-defense migration deliberately
		// replaced; see OVT_DeploymentBattleSuppression for why local-only is the decision.
		if (manager.IsBattleSuppressedAt(m_ParentDeployment.GetPosition(), factionIndex))
			return false;

		int availableResources = manager.GetFactionResources(factionIndex);
		int totalCost = groupsNeeded * m_iReinforcementCost;

		return availableResources >= totalCost;
	}

	//------------------------------------------------------------------------------------------------
	//! Buys `groupsNeeded` replacement groups and registers them through the same convergence
	//! everything else uses.
	//!
	//! THE FLAGS HAVE TO COME OFF FIRST - BOTH OF THEM. The convergence refuses to register for a module
	//! that is eliminated OR whose deployment is, which is the guarantee that a wiped force never
	//! resurrects itself; a rebuy is the one path allowed through it, so it clears both, converges, and
	//! then lets CheckAllSpawningModulesEliminated() recompute the deployment-wide truth from whatever
	//! the modules actually ended up holding. Clearing only this module's flag would leave a deployment
	//! with a second, still-eliminated spawning module blocking its own rebuy.
	//!
	//! Resources are charged up front and not refunded on a failed registration, which is what this path
	//! has always done.
	//! \param[in] groupsNeeded How many groups to buy.
	//! \return True when at least one group was registered.
	bool Reinforce(int groupsNeeded)
	{
		if (!m_ParentDeployment || groupsNeeded <= 0)
			return false;

		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
			return false;

		// Check if we can afford the reinforcement
		if (!CanReinforce(groupsNeeded))
			return false;

		int factionIndex = m_ParentDeployment.GetControllingFaction();
		int totalCost = groupsNeeded * m_iReinforcementCost;

		// Deduct resources and register reinforcements
		manager.SubtractFactionResources(factionIndex, totalCost);

		// Raise the count this module wants to hold; the convergence reads it back out of
		// GetMaxGroupCount() and registers exactly the difference.
		m_iActualGroupCount = m_aHandles.Count() + groupsNeeded;

		bool wasEliminated = m_bSpawnedUnitsEliminated;

		// ⚠ THE DEPLOYMENT'S PREVIOUS STATE IS CAPTURED TOO, AND IT DID NOT USED TO BE. Not restoring it
		// on the failure path defeated CheckAllSpawningModulesEliminated()'s edge trigger: that method
		// only logs when the flag CHANGES, so clearing it here and letting the recompute set it straight
		// back re-announced "All spawned units for deployment 'X' have been eliminated" on EVERY failed
		// rebuy. A wiped deployment whose condition can never be met again therefore printed the same
		// line every check interval for the rest of the campaign - once every ~66 s in the play-test that
		// found it (2026-08-20) - which reads as an event recurring rather than a state that has not
		// changed since the first time it was announced.
		bool deploymentWasEliminated = m_ParentDeployment.GetSpawnedUnitsEliminated();

		m_bSpawnedUnitsEliminated = false;
		m_ParentDeployment.SetSpawnedUnitsEliminated(false);

		int successfulSpawns = ConvergeGroups(m_bReinforceFromNearestBase);

		if (successfulSpawns <= 0)
		{
			m_bSpawnedUnitsEliminated = wasEliminated;
			m_ParentDeployment.SetSpawnedUnitsEliminated(deploymentWasEliminated);
		}

		// Recompute the deployment-wide flag from what the modules now actually hold, on both paths.
		m_ParentDeployment.CheckAllSpawningModulesEliminated();

		// 🔴 PAY BACK WHAT WAS NOT DELIVERED. The charge above is taken up front, before anything is
		// registered, because the convergence needs the money to already be gone - but a group that
		// failed to register is a group the faction did not get, and keeping its price is a straight
		// leak. It used to keep it: this method's header said "resources are charged up front and not
		// refunded on a failed registration, which is what this path has always done", and a play-test
		// showed what that costs. A town patrol whose town had changed hands could never register again,
		// retried every check interval, and burned 100 resources a time - THIRTEEN failed rebuys and
		// 1300 resources in twenty minutes, for zero groups, with no successful reinforcement anywhere in
		// the session. That money is a large part of the "spending that doesn't make sense" the same
		// play-test reported.
		//
		// ⚠ PRO-RATA, NOT ALL-OR-NOTHING, because a partial success is a real outcome: buying 2 and
		// registering 1 should cost one group, not two and not none.
		//
		// ⚠ THROUGH THE MANAGER'S OWN CREDIT METHOD, on the precedent OVT_MultiTownPatrolBehaviorDeployment
		// Module's patrol recovery already set - "resource accounting is closed" is checked by grepping
		// AddFactionResources for callers, and the framework's own refunds are an allowed answer.
		int undelivered = groupsNeeded - successfulSpawns;
		if (undelivered > 0)
		{
			int refund = undelivered * m_iReinforcementCost;
			manager.AddFactionResources(factionIndex, refund);

			if (successfulSpawns <= 0)
			{
				Print(string.Format("Reinforcement bought 0/%1 groups - the %2 resources it was charged have been refunded, because nothing was delivered", groupsNeeded, refund), LogLevel.WARNING);
				return false;
			}

			OVT_DeploymentLog.Debug(string.Format("Reinforced with %1/%2 groups, cost: %3 resources (%4 refunded for the %5 that could not be registered)", successfulSpawns, groupsNeeded, totalCost - refund, refund, undelivered));
			return true;
		}

		OVT_DeploymentLog.Debug(string.Format("Reinforced with %1/%2 groups, cost: %3 resources", successfulSpawns, groupsNeeded, totalCost));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! THE NEAREST BASE THIS FACTION ACTUALLY CONTROLS.
	//!
	//! 🔴 IT USED TO ASK FOR THE NEAREST BASE AND THEN CHECK WHETHER THAT ONE HAPPENED TO BE FRIENDLY,
	//! which is a different and much weaker question - and it answered vector.Zero whenever the closest
	//! base belonged to somebody else. On a contested map that is exactly backwards: the moment the
	//! resistance takes the base beside a town, every deployment there loses its source and falls back to
	//! materialising in place, however many friendly bases are a kilometre further on. The author hit it
	//! at Levie on 2026-08-20 - "a town patrol was bought for Levie and has spawned at Levie base (I
	//! control Levie base). they need to come from the closest controlled base".
	//!
	//! ⚠ IT DELEGATES RATHER THAN RE-IMPLEMENTING, and that is the point of the fix rather than an
	//! incidental tidy-up. OVT_NearestControlledBaseSourceProvider already walks the faction's OWN base
	//! list and picks the nearest of those - its class header describes this exact defect as the reason
	//! it exists - so the insertion module has had the correct answer available all along while this one
	//! kept the broken copy. One implementation, one behaviour.
	//! \param[in] factionIndex The faction that must control the base.
	//! \return The nearest controlled base's position, or vector.Zero when the faction holds none.
	protected vector GetNearestControlledBasePosition(int factionIndex)
	{
		if (!m_ParentDeployment)
			return vector.Zero;

		if (!m_NearestBaseSource)
		{
			// ⚠ `new` DOES NOT APPLY [Attribute()] DEFVALUES - the same trap the anchor provider's
			// constructor documents - so the limit is set explicitly. 0 = no limit, which is what this
			// caller wants: a distant friendly base is still better than no source at all.
			m_NearestBaseSource = new OVT_NearestControlledBaseSourceProvider();
			m_NearestBaseSource.m_fMaxSourceDistance = 0;
		}

		vector source;
		if (!m_NearestBaseSource.ResolveSource(m_ParentDeployment.GetPosition(), factionIndex, source))
			return vector.Zero;

		return source;
	}

	//! Held rather than constructed per call: a provider is a stateless answerer, so one instance can
	//! serve every lookup this module makes for its whole life.
	protected ref OVT_NearestControlledBaseSourceProvider m_NearestBaseSource;

	//------------------------------------------------------------------------------------------------
	// Status methods for behavior modules
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \return How many registered groups this module currently holds.
	int GetGroupCount()
	{
		return m_aHandles.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The framework-wide read of the same number, so a consumer that holds only an
	//! OVT_BaseSpawningDeploymentModule (the Game Master snapshot builder) does not have to cast.
	//!
	//! GUARDED WHERE GetGroupCount() IS NOT, and deliberately. Every module reached through AddModule()
	//! has been Initialize()d, so m_aHandles is never null on a live deployment - but this one is read
	//! from a diagnostic walk over EVERY deployment on the map, on a Game Master's poll, and that path
	//! must be incapable of throwing whatever state a deployment is in.
	//! \return How many registered groups this module currently holds.
	override int GetRegisteredGroupCount()
	{
		if (!m_aHandles)
			return 0;

		return GetGroupCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Appends every handle this module holds. Guarded for the same reason the count above is: the
	//! caller may be walking a deployment in any state.
	//! \param[inout] handles The caller's list, appended to. Never cleared.
	override void CollectRegisteredHandles(notnull array<int> handles)
	{
		if (!m_aHandles)
			return;

		foreach (int handle : m_aHandles)
		{
			handles.Insert(handle);
		}
	}

	//------------------------------------------------------------------------------------------------
	int GetMissingGroupCount()
	{
		return OVT_DeploymentVirtualKey.MissingCount(GetMaxGroupCount(), m_aHandles.Count());
	}

	//------------------------------------------------------------------------------------------------
	//! The count this module wants to hold, rolled ONCE and remembered - a re-roll on a later pass
	//! would quietly grow or shrink a force that already exists.
	//! \return The wanted group count.
	int GetMaxGroupCount()
	{
		if (m_iActualGroupCount > 0)
			return m_iActualGroupCount;

		if (!m_ParentDeployment)
			return m_iMaxGroupCount;

		m_iActualGroupCount = CalculateGroupCount(m_ParentDeployment.GetPosition());

		return m_iActualGroupCount;
	}

	//------------------------------------------------------------------------------------------------
	int GetReinforcementCost()
	{
		return m_iReinforcementCost;
	}

	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Infantry Module: %1", m_sModuleName));
		Print(string.Format("  Groups: %1/%2 registered (ever held: %3, min: %4, max: %5)", m_aHandles.Count(), GetMaxGroupCount(), m_iSpawnedEver, m_iMinGroupCount, m_iMaxGroupCount));
		Print(string.Format("  Group Type: %1", m_sGroupType));
		Print(string.Format("  Owner Key: %1", GetOwnerKey()));
		Print(string.Format("  Importance: %1", typename.EnumToString(SCR_EAISpawnImportance, m_eImportance)));
		string townScaling = "No";
		if (m_bScaleByTownSize)
			townScaling = "Yes";
		Print(string.Format("  Town Size Scaling: %1", townScaling));
		string reinforcementStatus = "Disabled";
		if (m_bAllowReinforcement)
			reinforcementStatus = "Enabled";
		Print(string.Format("  Reinforcement: %1", reinforcementStatus));

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		foreach (int handle : m_aHandles)
		{
			Print(string.Format("    Handle %1: %2/%3 alive at %4", handle,
				virtualization.GetAliveMemberCount(handle), virtualization.GetMemberCount(handle),
				virtualization.GetPosition(handle).ToString()));
		}
	}
}
