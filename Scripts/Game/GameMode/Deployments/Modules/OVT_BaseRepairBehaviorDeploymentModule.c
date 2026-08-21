//------------------------------------------------------------------------------------------------
//! BASE REPAIR: a detail sent to a base the occupying faction HOLDS, to put ruined structures back -
//! one per interval, cheapest first, for free.
//!
//! OVT_BaseSabotageBehaviorDeploymentModule's shape, inverted in three places and only three: the
//! base must be OURS, the targets must be RUINED, and there is no objective director anywhere in the
//! file (plan D16 - it is evaluator-selectable maintenance, not a director operation, so it must not
//! depend on the director's API). It also raises no notification: the occupying faction tidying up
//! its own base is not an event aimed at the player.
//!
//! ⚠ AUTHOR THIS MODULE BEFORE THE REINFORCEMENT MODULE in a config's m_aModules, or a mission that
//! completes has its detail rebought in the same pass that ended it.
//!
//! ⚠ THE CLOCK PAUSES, IT NEVER RESETS. A player wandering past should DELAY the next repair, not
//! stop it forever.
//!
//! EvaluateRepair(), IsRepairTarget(), IntervalTicksFrom() and StructuresPerMissionFrom() take their
//! inputs as arguments so the decision half is answerable with no deployment, no base and no manager.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_BaseRepairBehaviorDeploymentModule : OVT_BaseBehaviorDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;

	[Attribute(defvalue: "150", desc: "How close to the base centre the detail has to be to count as holding it. Also the circle a player has to be inside to pause the work")]
	float m_fClearRadius;

	[Attribute(defvalue: "500", desc: "Radius searched for structures around the base centre. Must match the sabotage module's own figure - a ruin sabotage could reach and repair could not would be permanent")]
	float m_fSearchRadius;

	[Attribute(defvalue: "300", desc: "Maximum distance from the deployment to the base it may repair. A base deployment is created AT the base's own position, so this only has to survive a spawn nudge")]
	float m_fMaxBaseDistance;

	[Attribute(defvalue: "120", desc: "Seconds between repairs. FALLBACK ONLY - the campaign's objectiveSabotageHoldSeconds wins whenever difficulty settings are loaded")]
	int m_iHoldSeconds;

	[Attribute(defvalue: "2", desc: "Structures put back before the detail stands down. FALLBACK ONLY - the campaign's objectiveSabotageStructuresPerMission wins whenever difficulty settings are loaded")]
	int m_iStructuresPerMission;

	//! One deployment update, in seconds.
	static const int UPDATE_SECONDS = 10;

	//! Fired-once latch, per MISSION. Not an attribute, not persisted, not cloned.
	protected bool m_bMissionReported;

	protected int m_iRepaired;
	protected int m_iTicksLeft;

	//! The interval is armed lazily, because difficulty settings are not guaranteed loaded at clone time.
	protected bool m_bArmed;

	//! Rebuilt from scratch on every repair, so a handle can never outlive the entity it names.
	protected ref array<IEntity> m_aTargets;
	protected ref array<int> m_aTargetCosts;

	//! Query state for the collect callback. The manager is cached here rather than resolved per
	//! entity: a sphere query over a base can offer hundreds.
	protected string m_sTargetBaseId;
	protected int m_iTargetBaseFaction;
	protected int m_iMyFaction;
	protected OVT_ResistanceFactionManager m_QueryResistance;

	//------------------------------------------------------------------------------------------------
	//! One observation of the base.
	//! \param[in] deltaTime Milliseconds nominally elapsed. Unused - the interval is counted in
	//!            updates, not wall time.
	override void OnUpdate(int deltaTime)
	{
		// ⚠ SUPER FIRST: the base body re-applies the behaviour to groups that spawned since the last
		// update.
		super.OnUpdate(deltaTime);

		if (m_bMissionReported || !m_ParentDeployment)
			return;

		OVT_BaseData base = ResolveTargetBase();
		if (!base)
			return;

		if (!m_bArmed)
		{
			m_iTicksLeft = ResolveIntervalTicks();
			m_bArmed = true;
		}

		int aliveInside = CountAliveRegisteredMembersWithin(base.location, m_fClearRadius);
				// ⚠ THE RESISTANCE, NOT JUST PLAYERS (author, 2026-08-21). Recruits and - when they arrive -
		// high command groups contest this place exactly as a player does. See DefenderWithin().
		bool enemyPresent = DefenderWithin(base.location, m_fClearRadius);

		if (!EvaluateRepair(aliveInside, enemyPresent, m_iTicksLeft))
			return;

		RepairNextStructure(base);
	}

	//------------------------------------------------------------------------------------------------
	//! The repair decision. No per-firing latch: a detail puts back several structures and the caller
	//! re-arms the clock, so the fired-once latch belongs to the MISSION.
	//! \param[in] aliveInside Living members of this deployment's force inside the clear radius.
	//! \param[in] enemyPresent Whether a player is standing inside the same circle.
	//! \param[inout] ticksLeft Updates still owed. PAUSED, never reset, on an interrupted tick.
	//! \return True when THIS call completed an interval and one structure may now be repaired.
	bool EvaluateRepair(int aliveInside, bool enemyPresent, inout int ticksLeft)
	{
		if (m_bMissionReported)
			return false;

		if (aliveInside < 1)
			return false;

		if (enemyPresent)
			return false;

		// ⚠ A NON-POSITIVE INTERVAL MUST STILL COST ONE TICK, so a misauthored zero cannot put a
		// structure back on the update the detail is registered.
		if (ticksLeft > 0)
			ticksLeft = ticksLeft - 1;

		if (ticksLeft > 0)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether one structure is a legitimate target. The first two rows are the inversions of
	//! IsSabotageTarget: nothing intact, and nothing at a base the resistance now holds.
	//! \param[in] associatedBaseId The structure's recorded association.
	//! \param[in] associatedType The structure's recorded association type.
	//! \param[in] targetBaseId The base being repaired.
	//! \param[in] targetBaseFaction Which faction currently holds that base.
	//! \param[in] myFaction This deployment's controlling faction.
	//! \param[in] isRuined Whether the structure is currently a ruin.
	//! \return True when the structure may be repaired by this mission.
	static bool IsRepairTarget(string associatedBaseId, EOVTBaseType associatedType, string targetBaseId, int targetBaseFaction, int myFaction, bool isRuined)
	{
		if (!isRuined)
			return false;

		if (targetBaseFaction != myFaction)
			return false;

		// An empty base id must match nothing rather than everything unassociated.
		if (targetBaseId == "")
			return false;

		// Ids are per-type, so a camp and a base can share one.
		if (associatedType != EOVTBaseType.BASE)
			return false;

		return associatedBaseId == targetBaseId;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts exactly one structure back, and decides whether the mission is over.
	//! \param[in] base The base being repaired.
	protected void RepairNextStructure(notnull OVT_BaseData base)
	{
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance)
			return;

		// Rebuilt every time: minutes pass between repairs, and the base can change hands.
		CollectTargets(base, resistance);

		int index = OVT_ObjectiveSelection.NextTargetIndex(m_aTargetCosts, null);
		if (index == OVT_ObjectiveSelection.NOTHING_TO_SELECT)
		{
			// On shipped data this is the COMMON outcome - see the trigger-surface note in the
			// feature's context.md. Standing down hands the still-intact groups back to the pool.
			CompleteMission("there was nothing left to repair");
			return;
		}

		IEntity target = m_aTargets[index];
		if (!target)
		{
			m_iTicksLeft = ResolveIntervalTicks();
			return;
		}

		Print(string.Format("[Overthrow] Base repair: restoring a structure worth %1 at %2",
			m_aTargetCosts[index], target.GetOrigin().ToString()), LogLevel.NORMAL);

		// -1 = server-initiated and free, the convention BuildItem() and ChargeForGarrison() use.
		if (!resistance.RepairStructure(target, -1))
		{
			// Charge a refusal a full interval, or a structure that cannot be put back spins the
			// mission on every update for its whole life.
			m_iTicksLeft = ResolveIntervalTicks();
			return;
		}

		m_iRepaired = m_iRepaired + 1;

		if (m_iRepaired >= ResolveStructuresPerMission())
		{
			CompleteMission("the detail met its quota");
			return;
		}

		m_iTicksLeft = ResolveIntervalTicks();
	}

	//------------------------------------------------------------------------------------------------
	//! Finds every ruined structure this mission may put back. A sphere query is the only enumerator
	//! there is - nothing in the tree keeps a registry of placed structures.
	//! \param[in] base The base being repaired.
	//! \param[in] resistance The manager that prices a structure, cached for the callback.
	protected void CollectTargets(notnull OVT_BaseData base, notnull OVT_ResistanceFactionManager resistance)
	{
		if (!m_aTargets)
		{
			m_aTargets = new array<IEntity>();
			m_aTargetCosts = new array<int>();
		}

		m_aTargets.Clear();
		m_aTargetCosts.Clear();

		m_sTargetBaseId = base.id.ToString();
		m_iTargetBaseFaction = base.faction;
		m_iMyFaction = m_ParentDeployment.GetControllingFaction();
		m_QueryResistance = resistance;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		world.QueryEntitiesBySphere(
			base.location,
			m_fSearchRadius,
			CollectTargetCallback,
			FilterStructureCallback,
			EQueryEntitiesFlags.ALL);

		m_QueryResistance = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Filter callback - buildables only. No placeable prefab carries a destruction component, so the
	//! isRuined gate would refuse them anyway; this just keeps the query cheap.
	//! \param[in] entity The entity being offered.
	//! \return True to pass it to the collect callback.
	protected bool FilterStructureCallback(IEntity entity)
	{
		if (!entity)
			return false;

		return OVT_BuildableComponent.Cast(entity.FindComponent(OVT_BuildableComponent)) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Collect callback - keeps the structures IsRepairTarget accepts, with their authored costs.
	//! \param[in] entity The entity that passed the filter.
	//! \return Always true, to keep searching.
	protected bool CollectTargetCallback(IEntity entity)
	{
		if (!entity)
			return true;

		// Re-resolved rather than trusted from the filter: the two callbacks share no channel.
		OVT_BuildableComponent buildable = OVT_BuildableComponent.Cast(entity.FindComponent(OVT_BuildableComponent));
		if (!buildable)
			return true;

		if (!IsRepairTarget(buildable.GetAssociatedBaseId(), buildable.GetBaseType(), m_sTargetBaseId,
			m_iTargetBaseFaction, m_iMyFaction, OVT_StructureDamage.IsRuined(entity)))
			return true;

		if (!m_QueryResistance)
			return true;

		m_aTargets.Insert(entity);
		m_aTargetCosts.Insert(m_QueryResistance.GetStructureCost(entity));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Stands the mission down. There is no separate "succeeded" form - nothing is reported to
	//! anybody, so ending early and ending at the quota are the same statement.
	void AbortMission()
	{
		m_bMissionReported = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Ends the mission and asks for the deployment.
	//! \param[in] reason Why the mission ended, for the log.
	protected void CompleteMission(string reason)
	{
		// Latch first, so a re-entrant path cannot find it down and repair one more.
		AbortMission();

		Print(string.Format("[Overthrow] Base repair detail finished after %1 structure(s): %2",
			m_iRepaired, reason), LogLevel.NORMAL);

		// Collected NEXT FRAME - an inline delete from a non-last behaviour module walks off the end
		// of the module list it is being iterated in.
		RequestDeploymentCollection("a base repair detail finished");
	}

	//------------------------------------------------------------------------------------------------
	//! The base this deployment is working on, or null when there is nothing valid under it.
	//!
	//! ⚠ THE RANGE TEST IS NOT OPTIONAL. GetNearestBase() answers at ANY distance, so without it a
	//! detail that ended up somewhere unexpected would start repairing the nearest base on the map.
	//! \return The base, or null.
	protected OVT_BaseData ResolveTargetBase()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return null;

		vector deploymentPos = m_ParentDeployment.GetPosition();

		OVT_BaseData base = occupying.GetNearestBase(deploymentPos);
		if (!base)
			return null;

		if (vector.Distance(deploymentPos, base.location) > m_fMaxBaseDistance)
			return null;

		// Inverted from sabotage: the occupying faction only rebuilds ground it holds.
		if (base.faction != m_ParentDeployment.GetControllingFaction())
			return null;

		return base;
	}

	//------------------------------------------------------------------------------------------------
	//! The precedence rule as a pure function, so the world-free test tier can reach it. A
	//! non-positive campaign figure means "no campaign, or the preset authored nothing".
	//!
	//! ⚠ THE FLOOR IS LOAD-BEARING: integer division of a sub-update interval answers zero, and a zero
	//! interval puts a structure back on the update the detail is registered.
	//! \param[in] fallbackSeconds The module's authored attribute.
	//! \param[in] difficultySeconds The campaign's figure, or non-positive for "not loaded".
	//! \return At least one update.
	static int IntervalTicksFrom(int fallbackSeconds, int difficultySeconds)
	{
		int seconds = fallbackSeconds;

		if (difficultySeconds > 0)
			seconds = difficultySeconds;

		if (seconds < UPDATE_SECONDS)
			return 1;

		return seconds / UPDATE_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	//! The same precedence rule for the quota.
	//! \param[in] fallbackCount The module's authored attribute.
	//! \param[in] difficultyCount The campaign's figure, or non-positive for "not loaded".
	//! \return At least one.
	static int StructuresPerMissionFrom(int fallbackCount, int difficultyCount)
	{
		int count = fallbackCount;

		if (difficultyCount > 0)
			count = difficultyCount;

		if (count < 1)
			return 1;

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! ⚠ IT READS THE SABOTAGE DIFFICULTY FIELDS DELIBERATELY (build decision BD23): repair holds "the
	//! same interval" per plan §3.7, and the shipped ladder already moves the right way.
	//!
	//! Public so an initialisation-tier case can assert the precedence against the campaign's real
	//! values without standing up a deployment.
	//! \return At least one update.
	int ResolveIntervalTicks()
	{
		int difficultySeconds = 0;

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (difficulty)
			difficultySeconds = difficulty.objectiveSabotageHoldSeconds;

		return IntervalTicksFrom(m_iHoldSeconds, difficultySeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! \return At least one. See ResolveIntervalTicks for why the field is sabotage's.
	int ResolveStructuresPerMission()
	{
		int difficultyCount = 0;

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (difficulty)
			difficultyCount = difficulty.objectiveSabotageStructuresPerMission;

		return StructuresPerMissionFrom(m_iStructuresPerMission, difficultyCount);
	}

	//------------------------------------------------------------------------------------------------
	//! ⚠ HAND-COPIED AND UNCHAINED. A dropped line does not warn, does not log and does not fail to
	//! parse - it ships the class default on every deployment forever. The latches, the counters and
	//! the armed timer must NOT be copied: a clone has repaired nothing.
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_BaseRepairBehaviorDeploymentModule clone = new OVT_BaseRepairBehaviorDeploymentModule();

		clone.m_sModuleName = m_sModuleName;
		clone.m_fClearRadius = m_fClearRadius;
		clone.m_fSearchRadius = m_fSearchRadius;
		clone.m_fMaxBaseDistance = m_fMaxBaseDistance;
		clone.m_iHoldSeconds = m_iHoldSeconds;
		clone.m_iStructuresPerMission = m_iStructuresPerMission;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether this mission has already stood down.
	bool HasMissionReported()
	{
		return m_bMissionReported;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many structures this mission has put back.
	int GetRepairedCount()
	{
		return m_iRepaired;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Updates still owed on the current interval, or 0 before the first update arms it.
	int GetIntervalTicksLeft()
	{
		return m_iTicksLeft;
	}

	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Base Repair Behavior Module: %1", m_sModuleName));
		Print(string.Format("  Clear Radius: %1m  Search Radius: %2m", m_fClearRadius, m_fSearchRadius));
		Print(string.Format("  Interval: %1 update(s) left  Repaired: %2", m_iTicksLeft, m_iRepaired));
		string reported = "No";
		if (m_bMissionReported)
			reported = "Yes";
		Print(string.Format("  Mission Reported: %1", reported));
	}
}
