//------------------------------------------------------------------------------------------------
//! Which of a base controller's slot lists a composition module draws from.
//!
//! One value per array on OVT_BaseControllerComponent. The FLAT sizes come from the vanilla editable
//! entity labels SLOT_FLAT_SMALL/MEDIUM/LARGE, the ROAD ones from SLOT_ROAD_SMALL/MEDIUM/LARGE, and
//! the controller sorts every slot it discovers into them at InitializeBase (:272-277).
//------------------------------------------------------------------------------------------------
enum OVT_EDeploymentSlotType
{
	SMALL,
	MEDIUM,
	LARGE,
	ROAD_SMALL,
	ROAD_MEDIUM,
	ROAD_LARGE
}

//------------------------------------------------------------------------------------------------
//! What a composition module should do about its structure on one convergence pass.
//!
//! THREE ANSWERS, NOT TWO, AND THE THIRD IS THE ONE THAT MATTERS. "Do not build" splits into "not
//! now" and "not ever", and getting them the wrong way round is invisible in both directions:
//!   - SKIP latched as NEVER makes a base that happened to converge one tick before its controller
//!     finished discovering slots permanently unfortifiable, silently;
//!   - NEVER treated as SKIP makes a RESTORED deployment build a second bunker on the next pass,
//!     which is D7's whole failure mode.
//------------------------------------------------------------------------------------------------
enum OVT_ECompositionBuildDecision
{
	//! Build the structure now.
	BUILD,

	//! Do nothing this pass and leave the latch alone - the next convergence is a free retry.
	SKIP,

	//! Do nothing, now or ever. The caller latches m_bCompositionAttempted.
	NEVER
}

//------------------------------------------------------------------------------------------------
//! Builds ONE slotted composition - a bunker, an ammo cache, an MG nest, a road checkpoint - and
//! optionally garrisons it. Replaces the legacy slotted-composition and checkpoint base upgrades,
//! which were the same mechanism written twice; both classes are gone and their reasons are preserved
//! here and in implementation.md 3.3 of the base-defense-migration feature.
//!
//! SHIPPED BY: Deployment_BaseCheckpoints.conf (LargeCheckpoint on ROAD_LARGE + MediumCheckpoint on
//! ROAD_MEDIUM, each with a light_patrol guard) and Deployment_BaseFortifications.conf (SmallBunker
//! with a bunker_team guard, AmmoCache and MGNest unguarded at m_iMaxGroupCount 0).
//!
//! ⚠ THE OWNER KEY IS SCOPED BY m_sModuleName (GetOwnerKey -> BuildOwnerKey(m_sModuleName)). Two
//! composition modules on ONE config MUST therefore carry DIFFERENT module names, or each would
//! reclaim the other's guards on every convergence and one of them would end up holding them all.
//!
//! ================== IT SUBCLASSES THE INFANTRY MODULE ON PURPOSE (D6) ===================
//! The guard group anchored on a composition is an ordinary registered group and wants the whole
//! inherited lifecycle - convergence, reclaim, wipe accounting, and above all REINFORCEMENT, which
//! OVT_ReinforcementBehaviorDeploymentModule can only see through an
//! OVT_InfantrySpawningDeploymentModule cast. A config that wants an unguarded composition (the ammo
//! cache, the MG nest) authors m_iMaxGroupCount 0 and gets exactly the structure.
//! =======================================================================================
//!
//! ================== A RESTORED DEPLOYMENT BUILDS NOTHING (D7) ===========================
//! The composition is a world ENTITY. OVT_PersistenceTracking.Track() puts it in the save point and
//! vanilla persistence brings it back before any deployment ticks; its slot claim comes back with the
//! base controller's m_aSlotsFilled, which OVT_OccupyingFactionManager restores verbatim. So a
//! deployment that came from a save is forbidden from building - without that gate a base would grow
//! one more bunker per load, in a different slot each time, forever. This is exactly what the legacy
//! composition upgrade's Deserialize already did: restore the position, re-man the turrets, rebuild
//! nothing.
//!
//! WHAT IS TRACKED AND WHAT IS NOT, stated once because it is the whole persistence claim:
//!   TRACKED   the composition ENTITY (OVT_PersistenceTracking.Track below) - vanilla persistence
//!             saves and restores it, so the structure is standing before any deployment ticks;
//!   TRACKED   its SLOT CLAIM, through the base controller's m_aSlotsFilled, which the occupying
//!             faction serializer writes and InitBaseControllers restores;
//!   NOT       the LINK between this module and the structure it built. m_CompositionId is runtime
//!             only, so on a restored deployment GetComposition() is null and ResolveSpawnPosition
//!             falls back to the deployment anchor. Invisible for the restored guards themselves
//!             (core re-creates them at their persisted positions and ReclaimHandles finds them);
//!             visible only for a reinforcement bought FOR a restored composition deployment, which
//!             anchors on the marker instead of on the structure. Costed and deliberately not built
//!             (D7 / D10 - it would need a serializer version bump).
//!
//! The known gap, inherited from legacy and accepted: a composition DESTROYED while the campaign was
//! saved is never rebuilt.
//! =======================================================================================
//!
//! ⚠ THE SLOT IS CLAIMED ONLY AFTER A SUCCESSFUL SPAWN. Carried verbatim from the legacy checkpoint
//! upgrade - "Only pay and fill the slot after a successful spawn - a failed spawn must not burn
//! resources or permanently block the slot". A slot inserted into
//! m_aSlotsFilled is claimed for the rest of the campaign AND across every save, so claiming one for
//! a composition that never appeared takes it out of the game permanently.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_CompositionSpawningDeploymentModule : OVT_InfantrySpawningDeploymentModule
{
	//! ⚠ ONLY m_aPrefabs AND m_iCost ARE READ off the resolved OVT_FactionComposition. Its
	//! m_aGroupPrefabs array is deliberately IGNORED: a guard here is a registered, virtualized group
	//! and comes from this module's inherited m_sGroupType against the faction GROUP REGISTRY, which is
	//! the only resolution path core accepts (api.md §3 - core resolves (factionKey, groupName), never
	//! a raw prefab). The legacy SmallBunker composition's group prefab is preserved as the registry
	//! entry `bunker_team`.
	[Attribute(desc: "Composition tag from the faction's composition config, e.g. SmallBunker, AmmoCache, MGNest, MediumCheckpoint, LargeCheckpoint")]
	string m_sCompositionTag;

	[Attribute(defvalue: "0", UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(OVT_EDeploymentSlotType), desc: "Which of the base's slot lists to build in")]
	OVT_EDeploymentSlotType m_eSlotType;

	[Attribute(defvalue: "0", desc: "Fill every ammo box attached to the composition with occupying-faction items")]
	bool m_bFillAmmoBoxes;

	//! How far around the composition to look for things with turrets to man, by slot size. The legacy
	//! composition upgrade used 7/15/23 m for the three flat sizes; the road sizes take the same
	//! ladder. ⚠ Legacy checkpoints did NOT man their turrets (the checkpoint upgrade never implemented
	//! the composition upgrade's Setup() hook); doing it here is a deliberate, small improvement - a
	//! checkpoint with a crewed gun is what a checkpoint is for.
	protected const float OCCUPANT_RANGE_SMALL = 7;
	protected const float OCCUPANT_RANGE_MEDIUM = 15;
	protected const float OCCUPANT_RANGE_LARGE = 23;

	//! How many times to roll for a free slot before giving up. The legacy slotted upgrade's number - a
	//! random roll rather than a scan, so two bases fortifying in the same pass do not both take the
	//! first free slot in their list.
	protected const int SLOT_TRIES = 30;

	//! Item count range poured into each attached ammo box. Legacy's numbers.
	protected const int AMMO_ITEMS_MIN = 15;
	protected const int AMMO_ITEMS_MAX = 40;

	//! The composition this module built, once. EntityID rather than IEntity: the structure is a world
	//! entity that can be destroyed under us, and a stale pointer would be a crash where a stale id is
	//! just a null lookup. Paired with a flag rather than compared against a sentinel, because
	//! EntityID is a raw 64-bit handle with no meaningful "unset" test from script.
	protected EntityID m_CompositionId;

	//! Whether m_CompositionId holds anything.
	protected bool m_bHasComposition;

	//! Whether this module has already had its one attempt at building. Kept separately from
	//! m_CompositionId so a DESTROYED composition is not silently rebuilt on the next convergence -
	//! legacy did not rebuild either, and rebuilding would need a second slot.
	protected bool m_bCompositionAttempted;

	//------------------------------------------------------------------------------------------------
	//! EVERY inherited attribute plus this module's own - CloneModule is not chained. See the note on
	//! OVT_InfantrySpawningDeploymentModule.CloneModule().
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_CompositionSpawningDeploymentModule clone = new OVT_CompositionSpawningDeploymentModule();

		// --- inherited from OVT_InfantrySpawningDeploymentModule (all twelve, plus m_bSnapToRoad) ---
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

		// --- this module's own ---
		clone.m_sCompositionTag = m_sCompositionTag;
		clone.m_eSlotType = m_eSlotType;
		clone.m_bFillAmmoBoxes = m_bFillAmmoBoxes;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	//! What this module costs the pool: its guard groups plus the composition's own authored cost.
	//!
	//! ⚠ ASKED OF THE CONFIG TEMPLATE, with no deployment behind it - OVT_DeploymentConfig
	//! .GetTotalResourceCost() walks the authored modules, not a live deployment's. So the composition
	//! is resolved against the occupying faction when there is no deployment to ask, which is what the
	//! legacy upgrades did unconditionally.
	//! \return The total cost.
	override int GetResourceCost()
	{
		int cost = super.GetResourceCost();

		OVT_FactionComposition composition = ResolveComposition();
		if (composition)
			cost += composition.m_iCost;

		return cost;
	}

	//------------------------------------------------------------------------------------------------
	//! THE SAME PRICE, MINUS ANYTHING THIS BASE CANNOT ACTUALLY TAKE.
	//!
	//! ⚠ THE COMPOSITION'S PRICE IS THE ONLY PART THAT IS CONDITIONAL. Everything else this module is
	//! charged for - its groups, the base cost - is bought and delivered whatever the slots look like,
	//! so only the structure's own m_iCost is dropped. A config with two composition modules and one
	//! free slot therefore pays for one of them, which is exactly what the author asked for: not a
	//! refusal, not a full charge, a bill for what can land.
	//! \param[in] position Where the deployment would be created.
	//! \return The cost this module should be charged at that position.
	override int GetResourceCostAt(vector position)
	{
		int cost = super.GetResourceCostAt(position);

		OVT_FactionComposition composition = ResolveComposition();
		if (composition && HasFreeSlotAt(position, m_eSlotType))
			cost += composition.m_iCost;

		return cost;
	}

	//------------------------------------------------------------------------------------------------
	//! WHAT THIS MODULE'S STRUCTURE COSTS - the conditional part of its price, on its own.
	//!
	//! ⚠ PUBLIC BECAUSE THE INVARIANT IS, and ResolveComposition() stays protected. The rule
	//! OVT_TEST_Init_CompositionSlotGate_NothingIsChargedForAnUnplaceableComposition pins is that the
	//! difference between this module's template price and its priced-for-here price is either zero or
	//! EXACTLY this number - all or nothing. Asserting that needs the number, not the composition, so
	//! this is the narrowest thing that can be exposed rather than opening the resolver.
	//! \return The composition's m_iCost, or 0 when the tag resolves to nothing for this faction.
	int GetCompositionCost()
	{
		OVT_FactionComposition composition = ResolveComposition();
		if (!composition)
			return 0;

		return composition.m_iCost;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True - this module is the one that needs a slot.
	override bool WantsCompositionSlot()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] position Where the deployment would be created.
	//! \return True when this module's composition could be placed there.
	override bool CanPlaceCompositionAt(vector position)
	{
		if (!ResolveComposition())
			return false;

		return HasFreeSlotAt(position, m_eSlotType);
	}

	//------------------------------------------------------------------------------------------------
	//! Build the composition (once, ever), then converge the guard group on it.
	//!
	//! ORDER IS LOAD-BEARING: the structure goes in first so the guards can be anchored on it, and
	//! super.EnsureGroups() is called on EVERY path - including the ones where no composition could be
	//! built - because a guard group that already exists must still be reclaimed or it would be
	//! orphaned in the registry.
	override void EnsureGroups()
	{
		TryBuildComposition();

		super.EnsureGroups();
	}

	//------------------------------------------------------------------------------------------------
	//! Guards stand on the composition, not on a ring around the deployment and never on a road.
	//! \param[in] anchor The batch anchor. Used when there is no composition (a restored deployment
	//!            whose structure the save brought back, or one that never found a slot).
	//! \param[in] index Position within this batch. Unused.
	//------------------------------------------------------------------------------------------------
	//! TRUE: this module's groups stand on the structure it just built - a bunker, an MG nest, a road
	//! checkpoint - and a checkpoint guard who leaves the checkpoint is not guarding anything. See the
	//! base class for what the false answer protects.
	//! \return True, always.
	override bool StationsGroupsDeliberately()
	{
		return true;
	}

	//! \return Where to register.
	override protected vector ResolveSpawnPosition(vector anchor, int index)
	{
		IEntity composition = GetComposition();
		if (!composition)
			return anchor;

		return composition.GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The composition entity this module built, or null if it never built one / it is gone.
	IEntity GetComposition()
	{
		if (!m_bHasComposition)
			return null;

		return GetGame().GetWorld().FindEntityByID(m_CompositionId);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether this module has already had its one build attempt.
	bool HasAttemptedComposition()
	{
		return m_bCompositionAttempted;
	}

	//------------------------------------------------------------------------------------------------
	// Composition building
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether a composition module in this state should put a structure in the world.
	//!
	//! A PURE FUNCTION OF ITS ARGUMENTS, WITH NO WORLD, NO SLOT AND NO DEPLOYMENT BEHIND IT, because
	//! the no-double-build claim is otherwise only assertable by driving a real save through a real
	//! base with a real free slot. TryBuildComposition() is routed through it - this is the decision,
	//! not a second copy of it. (Same shape and the same reason as
	//! OVT_PlacedInfantrySpawningDeploymentModule's placement statics.)
	//!
	//! FOUR GATES, IN ORDER, AND EACH IS A DIFFERENT QUESTION:
	//!   1. no deployment          - nothing to build around (a config template, the Init tier). SKIP,
	//!                               because a template that is later cloned onto a real deployment
	//!                               must still be able to build;
	//!   2. already attempted      - one composition per module, for the life of the deployment. NEVER;
	//!   3. restored from a save   - D7: vanilla persistence already put the structure back and
	//!                               m_aSlotsFilled already holds its slot. NEVER, and the latch is
	//!                               what stops a later reinforcement (which clears the eliminated
	//!                               flags) from building a second one;
	//!   4. eliminated             - a deployment whose force was wiped out does not quietly grow a new
	//!                               bunker. SKIP, not NEVER: the rebuy path clears the flag first,
	//!                               exactly as it does for groups, and the structure is then owed.
	//! \param[in] hasDeployment Whether the module is attached to a live deployment.
	//! \param[in] alreadyAttempted Whether this module has already had its one attempt.
	//! \param[in] restoredFromSave Whether the parent deployment came back from a save point.
	//! \param[in] eliminated Whether this module or its deployment is flagged wiped out.
	//! \return BUILD, SKIP (retry later) or NEVER (latch it).
	static OVT_ECompositionBuildDecision DecideBuild(bool hasDeployment, bool alreadyAttempted, bool restoredFromSave, bool eliminated)
	{
		if (!hasDeployment)
			return OVT_ECompositionBuildDecision.SKIP;

		if (alreadyAttempted)
			return OVT_ECompositionBuildDecision.NEVER;

		if (restoredFromSave)
			return OVT_ECompositionBuildDecision.NEVER;

		if (eliminated)
			return OVT_ECompositionBuildDecision.SKIP;

		return OVT_ECompositionBuildDecision.BUILD;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies a decision's latch and answers whether the caller may go on to build.
	//!
	//! Separate from DecideBuild() so the decision stays pure while the ONE state change it implies
	//! still has exactly one home. A decision computed and never latched is the silent half of the
	//! double-build failure.
	//! \param[in] decision What DecideBuild() answered.
	//! \return True only for BUILD.
	protected bool ApplyBuildDecision(OVT_ECompositionBuildDecision decision)
	{
		if (decision == OVT_ECompositionBuildDecision.NEVER)
			m_bCompositionAttempted = true;

		return decision == OVT_ECompositionBuildDecision.BUILD;
	}

	//------------------------------------------------------------------------------------------------
	//! The one build attempt, guarded by every reason not to make it. See DecideBuild().
	protected void TryBuildComposition()
	{
		bool hasDeployment = false;
		bool restoredFromSave = false;
		bool eliminated = m_bSpawnedUnitsEliminated;

		if (m_ParentDeployment)
		{
			hasDeployment = true;
			restoredFromSave = m_ParentDeployment.WasRestoredFromSave();
			if (m_ParentDeployment.GetSpawnedUnitsEliminated())
				eliminated = true;
		}

		if (!ApplyBuildDecision(DecideBuild(hasDeployment, m_bCompositionAttempted, restoredFromSave, eliminated)))
			return;

		OVT_FactionComposition composition = ResolveComposition();
		if (!composition)
		{
			Print(string.Format("[Overthrow] Deployment '%1': composition tag '%2' resolves to nothing for this faction - no structure will be built",
				m_ParentDeployment.GetDeploymentName(), m_sCompositionTag), LogLevel.WARNING);
			m_bCompositionAttempted = true;
			return;
		}

		if (!composition.m_aPrefabs || composition.m_aPrefabs.IsEmpty())
		{
			Print(string.Format("[Overthrow] Deployment '%1': composition '%2' has no prefabs authored",
				m_ParentDeployment.GetDeploymentName(), m_sCompositionTag), LogLevel.WARNING);
			m_bCompositionAttempted = true;
			return;
		}

		OVT_BaseControllerComponent controller = FindNearestBaseController();
		if (!controller)
		{
			// NOT marked attempted: the base controller may simply not have initialised yet, and the
			// next convergence is a free retry.
			Print(string.Format("[Overthrow] Deployment '%1': no base controller near the deployment, cannot place composition '%2'",
				m_ParentDeployment.GetDeploymentName(), m_sCompositionTag), LogLevel.WARNING);
			return;
		}

		IEntity slot = FindFreeSlot(controller);
		if (!slot)
		{
			// A base with every slot of this size already filled is an ordinary, permanent state, so
			// this IS marked attempted - retrying every 10 s forever would log the same warning for the
			// rest of the campaign.
			//
			// ⚠ "THE BASE IS FULL" USED TO BE SAID FOR TWO DIFFERENT FAILURES AND SENT A READER THE WRONG
			// WAY (2026-08-20). A base with no slot of this KIND at all - which is the common case for
			// the ROAD_* sizes, since road slots only exist where a road actually runs through the base -
			// reported exactly the same sentence as a base whose slots are genuinely all taken. The
			// author read it, checked the base's SMALL slots, found them free, and reasonably concluded
			// the module was broken: "a checkpoint was purchased but not spawned. levie base is free, it
			// has small slots afaik, I think the composition module is broken." A checkpoint needs
			// ROAD_LARGE / ROAD_MEDIUM, and SMALL slots have nothing to do with it.
			array<ref EntityID> ofThisKind = GetSlotList(controller);
			int slotCount = 0;
			if (ofThisKind)
				slotCount = ofThisKind.Count();

			string reason;
			if (slotCount == 0)
				reason = "this base has NO slot of that kind at all - it is not a shortage, nothing of this type can ever be built here";
			else
				reason = string.Format("all %1 slot(s) of that kind at this base are already taken", slotCount.ToString());

			// ⚠ NORMAL, NOT WARNING, SINCE 2026-08-22, AND THE DOWNGRADE IS THE POINT OF THE WHOLE CHANGE.
			// This used to be a fault: the faction had PAID for a structure and got nothing. It is not a
			// fault any more - the price is taken per position now, so an unplaceable composition is not
			// charged for (OVT_BaseDeploymentModule.GetResourceCostAt), and a base with no free slot of
			// this kind is an ordinary property of the terrain the author called out by name: *"some
			// bases simply don't have the slots or anywhere to put them"*. A WARNING on every base
			// startup for a normal condition is noise that hides real ones.
			OVT_DeploymentLog.Debug(string.Format("[Overthrow] Deployment '%1' did not place composition '%2': it needs a %3 slot and %4 - nothing was charged for it",
				m_ParentDeployment.GetDeploymentName(), m_sCompositionTag, typename.EnumToString(OVT_EDeploymentSlotType, m_eSlotType), reason));

			m_bCompositionAttempted = true;
			return;
		}

		m_bCompositionAttempted = true;

		vector mat[4];
		slot.GetTransform(mat);

		IEntity structure = OVT_WorldUtils.SpawnEntityPrefabMatrix(composition.m_aPrefabs.GetRandomElement(), mat);
		if (!structure)
		{
			Print(string.Format("[Overthrow] Deployment '%1': composition '%2' failed to spawn - the slot is left FREE",
				m_ParentDeployment.GetDeploymentName(), m_sCompositionTag), LogLevel.WARNING);
			return;
		}

		// Tracking is what puts the structure in save points. The composition prefabs all descend from
		// CompositionBase.et, which vanilla's own Composition.conf already configures; tracking is the
		// only missing half, and without it the structure vanishes on load while its slot stays marked
		// occupied - blocking rebuild forever.
		OVT_PersistenceTracking.Track(structure);

		// Guarded helper rather than an inline cast: a null AI world here would VM-error out before the
		// slot was claimed, leaving the composition standing in a slot the base still thinks is free.
		OVT_NavmeshRebuild.RebuildNow(structure);

		m_CompositionId = structure.GetID();
		m_bHasComposition = true;

		// THE CLAIM COMES LAST. See the class header.
		ClaimSlot(controller.m_aSlotsFilled, slot.GetID());

		ManTurrets(structure.GetOrigin());

		if (m_bFillAmmoBoxes)
			FillAmmoBoxes(structure);

		OVT_DeploymentLog.Debug(string.Format("[Overthrow] Deployment '%1' built composition '%2' at %3",
			m_ParentDeployment.GetDeploymentName(), m_sCompositionTag, structure.GetOrigin().ToString()));
	}

	//------------------------------------------------------------------------------------------------
	//! A free slot of this module's authored size, by random roll.
	//!
	//! RANDOM ROLLS RATHER THAN A SCAN, SLOT_TRIES times, straight from the legacy slotted upgrade's
	//! slot finder: a scan would make every base put its first bunker in the same slot, and two modules
	//! fortifying in one pass would queue on the same one.
	//! \param[in] controller The base controller that owns the slots.
	//! \return A slot entity nothing has claimed, or null.
	protected IEntity FindFreeSlot(notnull OVT_BaseControllerComponent controller)
	{
		array<ref EntityID> slots = GetSlotList(controller);
		if (!slots || slots.IsEmpty())
			return null;

		if (!controller.m_aSlotsFilled)
			return null;

		for (int i = 0; i < SLOT_TRIES; i++)
		{
			int index = RollFreeSlotIndex(slots, controller.m_aSlotsFilled);
			if (index < 0)
				continue;

			IEntity slot = GetGame().GetWorld().FindEntityByID(slots[index]);
			if (slot)
				return slot;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! ONE roll of the slot lottery.
	//!
	//! THE WHOLE "TWO COMPOSITIONS NEVER SHARE A SLOT" RULE LIVES HERE, and it is pure so it can be
	//! asserted without a base, a slot entity or a world - the Init tier never runs InitBaseControllers,
	//! so a live slot list does not exist there at all.
	//!
	//! ⚠ `filled` IS NOT THIS DEPLOYMENT'S BOOKKEEPING - it is the BASE's, restored verbatim from the
	//! save by OVT_OccupyingFactionManager.InitBaseControllers(). That is precisely what lets a legacy
	//! campaign's compositions and a new deployment's compositions coexist: the old structures' slots
	//! come back in this list, and this roll refuses every one of them.
	//!
	//! A ROLL, NOT A SCAN (the legacy slotted upgrade's shape): a scan would put every base's first
	//! bunker in the same slot and would make two modules fortifying in one pass queue on the same one.
	//! One roll per call so the caller keeps its own try budget and its own dead-entity retry.
	//! \param[in] slots Every slot of the wanted size at this base.
	//! \param[in] filled Every slot at this base that anything has already claimed.
	//! \return The index into `slots` of an unclaimed slot, or -1 when the roll landed on a claimed one
	//!         (or there was nothing to roll).
	static int RollFreeSlotIndex(array<ref EntityID> slots, array<ref EntityID> filled)
	{
		if (!slots || slots.IsEmpty())
			return -1;

		if (!filled)
			return -1;

		// RandInt is max-EXCLUSIVE, and RandInt(0, 0) is an engine error - the emptiness guard above is
		// what keeps it out of reach.
		int index = s_AIRandomGenerator.RandInt(0, slots.Count());

		if (filled.Contains(slots[index]))
			return -1;

		return index;
	}

	//------------------------------------------------------------------------------------------------
	//! Marks a slot as taken for the rest of the campaign AND across every save.
	//!
	//! One line, but it has its own method for two reasons: it is the state change RollFreeSlotIndex()
	//! reads, so a test can assert the pair round-trips; and it is irreversible - nothing anywhere
	//! removes an entry from m_aSlotsFilled, so a slot claimed for a composition that never appeared is
	//! out of the game permanently. That is why the production path calls it only after a successful
	//! spawn.
	//! \param[in] filled The base controller's claim list.
	//! \param[in] slotId The slot to claim.
	//! \return True when the claim was recorded, false when it was already there or there is no list.
	static bool ClaimSlot(array<ref EntityID> filled, EntityID slotId)
	{
		if (!filled)
			return false;

		if (filled.Contains(slotId))
			return false;

		filled.Insert(slotId);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] controller The base controller.
	//! \return The slot array this module's authored size names.
	protected array<ref EntityID> GetSlotList(notnull OVT_BaseControllerComponent controller)
	{
		return GetSlotListFor(controller, m_eSlotType);
	}

	//------------------------------------------------------------------------------------------------
	//! THE ONE PLACE A SLOT TYPE IS TURNED INTO A LIST, shared by this module and by the condition that
	//! stops a deployment being bought for a slot type the base does not have.
	//!
	//! ⚠ STATIC BECAUSE THE CONDITION HAS NO INSTANCE TO ASK. It runs at CREATION time, before any
	//! deployment or module clone exists, so it cannot call the instance method above - and a second
	//! copy of this switch is exactly how the two would drift into disagreeing about which list a
	//! ROAD_MEDIUM composition looks at.
	//! \param[in] controller The base whose slots to read.
	//! \param[in] slotType Which size and kind.
	//! \return That base's list for the type, or null for an unrecognised type.
	//------------------------------------------------------------------------------------------------
	//! THE BASE CONTROLLER A DEPLOYMENT AT THIS POSITION WOULD BUILD INTO.
	//!
	//! The static twin of FindNearestBaseController(), which asks the same question of a LIVE module's
	//! own position. This one takes the position because the pricing gate has to ask it BEFORE any
	//! deployment exists - see GetResourceCostAt().
	//! \param[in] position Where the deployment would be created.
	//! \return The controller, or null when there is no base there or its marker is not in the world.
	static OVT_BaseControllerComponent FindBaseControllerAt(vector position)
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return null;

		OVT_BaseData nearest = occupying.GetNearestBase(position);
		if (!nearest)
			return null;

		IEntity marker = GetGame().GetWorld().FindEntityByID(nearest.entId);
		if (!marker)
			return null;

		return OVT_BaseControllerComponent.Cast(marker.FindComponent(OVT_BaseControllerComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! COULD A COMPOSITION OF THIS SLOT KIND BE PLACED AT THIS POSITION AT ALL?
	//!
	//! ==========================================================================================
	//! 🔴 "They shouldn't have to pay for unplaceable positions, some bases simply don't have the
	//! slots or anywhere to put them." (author, 2026-08-22.)
	//! ==========================================================================================
	//! A deployment's price is taken from the CONFIG TEMPLATE before the deployment exists, and the
	//! composition is placed much later, from TryBuildComposition(). Nothing joined the two, so a base
	//! with no free slot of the right kind was charged in full for a structure that could never appear -
	//! silently, because a refusal to place is not an error. The author's own log has it happening at
	//! three separate bases inside one second of startup:
	//!
	//!     Deployment 'Base Fortifications' built composition 'SmallBunker' at <3839.09, ...>
	//!     Deployment 'Base Fortifications' could not place composition 'MGNest': it needs a SMALL slot
	//!     and all 1 slot(s) of that kind at this base are already taken
	//!
	//! ⚠ A DETERMINISTIC SCAN, DELIBERATELY, WHERE FindFreeSlot() ROLLS. That one rolls SLOT_TRIES times
	//! because picking a random free slot is what it is FOR; this one has to answer "is there one" and a
	//! roll that misses would price a placeable composition as unplaceable and hand the faction a
	//! discount it had not earned. Both read the SAME two lists - GetSlotListFor() and the controller's
	//! m_aSlotsFilled - so the price and the placement can never disagree about what "free" means.
	//! \param[in] position Where the deployment would be created.
	//! \param[in] slotType The kind of slot the composition needs.
	//! \return True when at least one slot of that kind is free at the base there.
	static bool HasFreeSlotAt(vector position, OVT_EDeploymentSlotType slotType)
	{
		OVT_BaseControllerComponent controller = FindBaseControllerAt(position);
		if (!controller)
			return false;

		// ⚠ THE SCAN ITSELF IS HasFreeSlot(), NOT A SECOND COPY OF IT. That static already exists, is
		// already asserted by OVT_TEST_Init_CompositionSlotGate_FreeSlotScanIsExhaustive - including the
		// asymmetry that a missing CLAIM list means "no free slot" rather than "nothing is claimed" - and
		// duplicating it here is exactly the drift this file keeps warning about. All this method adds is
		// resolving the two lists from a position.
		return HasFreeSlot(GetSlotListFor(controller, slotType), controller.m_aSlotsFilled);
	}

	static array<ref EntityID> GetSlotListFor(notnull OVT_BaseControllerComponent controller, OVT_EDeploymentSlotType slotType)
	{
		switch (slotType)
		{
			case OVT_EDeploymentSlotType.SMALL:
				return controller.m_SmallSlots;
			case OVT_EDeploymentSlotType.MEDIUM:
				return controller.m_MediumSlots;
			case OVT_EDeploymentSlotType.LARGE:
				return controller.m_LargeSlots;
			case OVT_EDeploymentSlotType.ROAD_SMALL:
				return controller.m_SmallRoadSlots;
			case OVT_EDeploymentSlotType.ROAD_MEDIUM:
				return controller.m_MediumRoadSlots;
			case OVT_EDeploymentSlotType.ROAD_LARGE:
				return controller.m_LargeRoadSlots;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether any slot in a list is unclaimed.
	//!
	//! ⚠ A SCAN, DELIBERATELY, WHERE FindFreeSlot() ROLLS. The roll is right for PICKING - it stops
	//! every base putting its first bunker in the same slot - but it is wrong for ASKING, because a roll
	//! that lands on a taken slot is indistinguishable from there being none free. A creation gate that
	//! answered "no" on a bad roll would refuse a perfectly buildable deployment at random.
	//!
	//! PURE OVER THE TWO ARRAYS, so the Init tier can assert it without a base, a slot entity or a
	//! world - the same reason RollFreeSlotIndex is written this way.
	//! \param[in] slots Every slot of one kind at a base. Null or empty answers false.
	//! \param[in] filled Every slot at that base anything has already claimed. Null answers false,
	//!            because "the claim list does not exist" is not the same as "nothing is claimed".
	//! \return True when at least one entry of `slots` is absent from `filled`.
	static bool HasFreeSlot(array<ref EntityID> slots, array<ref EntityID> filled)
	{
		if (!slots || slots.IsEmpty())
			return false;

		if (!filled)
			return false;

		foreach (EntityID slot : slots)
		{
			if (!filled.Contains(slot))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How far around a composition of this size to look for crewable turrets.
	protected float GetOccupantRange()
	{
		switch (m_eSlotType)
		{
			case OVT_EDeploymentSlotType.MEDIUM:
				return OCCUPANT_RANGE_MEDIUM;
			case OVT_EDeploymentSlotType.ROAD_MEDIUM:
				return OCCUPANT_RANGE_MEDIUM;
			case OVT_EDeploymentSlotType.LARGE:
				return OCCUPANT_RANGE_LARGE;
			case OVT_EDeploymentSlotType.ROAD_LARGE:
				return OCCUPANT_RANGE_LARGE;
		}

		return OCCUPANT_RANGE_SMALL;
	}

	//------------------------------------------------------------------------------------------------
	//! Mans every turret in the composition, which is what makes an MG nest or a bunker fight back.
	//!
	//! TURRET ONLY, deliberately: the legacy upgrade asked for {ECompartmentType.TURRET} and nothing
	//! else, so a composition with a parked truck in it does not conjure a driver.
	//! \param[in] centre The composition's origin.
	protected void ManTurrets(vector centre)
	{
		GetGame().GetWorld().QueryEntitiesBySphere(centre, GetOccupantRange(), FillCompartments, null, EQueryEntitiesFlags.ALL);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entity Candidate from the sphere query.
	//! \return True, to keep the query running.
	protected bool FillCompartments(IEntity entity)
	{
		SCR_BaseCompartmentManagerComponent compartments = OVT_ComponentFinder<SCR_BaseCompartmentManagerComponent>.Find(entity);
		if (!compartments)
			return true;

		compartments.SpawnDefaultOccupants({ ECompartmentType.TURRET });

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Pours occupying-faction items into every box attached to the composition. Port of the legacy
	//! composition upgrade's FillAmmoboxes.
	//! \param[in] entity The composition.
	protected void FillAmmoBoxes(notnull IEntity entity)
	{
		SlotManagerComponent slots = OVT_ComponentFinder<SlotManagerComponent>.Find(entity);
		if (!slots)
			return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
			return;

		array<ResourceName> prefabs = {};
		economy.GetAllNonClothingOccupyingFactionItems(prefabs);

		// An empty catalog would make the index roll a VM error - RandInt(0,0) errors outright.
		if (prefabs.IsEmpty())
			return;

		array<EntitySlotInfo> slotInfos = {};
		slots.GetSlotInfos(slotInfos);

		foreach (EntitySlotInfo slot : slotInfos)
		{
			IEntity box = slot.GetAttachedEntity();
			if (!box)
				continue;

			SCR_InventoryStorageManagerComponent storage = SCR_InventoryStorageManagerComponent.Cast(box.FindComponent(SCR_InventoryStorageManagerComponent));
			if (!storage)
				continue;

			int numItems = s_AIRandomGenerator.RandInt(AMMO_ITEMS_MIN, AMMO_ITEMS_MAX);
			for (int i = 0; i < numItems; i++)
			{
				storage.TrySpawnPrefabToStorage(prefabs[s_AIRandomGenerator.RandInt(0, prefabs.Count())]);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	// Resolution helpers
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! This module's composition entry, resolved against the deployment's own faction when there is a
	//! deployment and against the occupying faction when there is not (the config-template case that
	//! GetResourceCost hits).
	//! \return The composition entry, or null.
	protected OVT_FactionComposition ResolveComposition()
	{
		if (m_sCompositionTag.IsEmpty())
			return null;

		OVT_Faction faction = ResolveOverthrowFaction();
		if (!faction)
			return null;

		return faction.GetCompositionConfig(m_sCompositionTag);
	}

	//------------------------------------------------------------------------------------------------
	//! The Overthrow faction data this module builds for.
	//!
	//! Deliberately NOT through OVT_OverthrowFactionManager.GetOverthrowFactionByIndex(), which
	//! dereferences GetFactionByIndex(index) unguarded and would VME on a stale index - the same
	//! reasoning as ResolveFactionKey() on the base class, which this reuses.
	//! \return The faction, or null.
	protected OVT_Faction ResolveOverthrowFaction()
	{
		if (m_ParentDeployment)
		{
			string key = ResolveFactionKey(m_ParentDeployment.GetControllingFaction());
			if (!key.IsEmpty())
			{
				OVT_OverthrowFactionManager factions = OVT_Global.GetFactions();
				if (factions)
				{
					OVT_Faction faction = factions.GetOverthrowFactionByKey(key);
					if (faction)
						return faction;
				}
			}
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return null;

		return config.GetOccupyingFaction();
	}

	//------------------------------------------------------------------------------------------------
	//! The base controller whose slots this module may build in.
	//! \return The controller, or null when there is no base near the deployment.
	protected OVT_BaseControllerComponent FindNearestBaseController()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return null;

		OVT_BaseData nearest = occupying.GetNearestBase(GetDeploymentPosition());
		if (!nearest)
			return null;

		IEntity marker = GetGame().GetWorld().FindEntityByID(nearest.entId);
		if (!marker)
			return null;

		return OVT_BaseControllerComponent.Cast(marker.FindComponent(OVT_BaseControllerComponent));
	}
}
