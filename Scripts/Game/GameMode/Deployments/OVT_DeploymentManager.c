// Helper class for sorting candidate positions by threat level
class OVT_CandidatePosition
{
	vector position;
	float threatLevel;
	
	[SortAttribute(),NonSerialized()]
	float sortBy;
	
	void OVT_CandidatePosition(vector pos, float threat)
	{
		position = pos;
		threatLevel = threat;
		sortBy = threat;
	}
}

[EntityEditorProps(category: "Overthrow/Managers", description: "Manages all deployments across factions")]
class OVT_DeploymentManagerComponentClass : OVT_ComponentClass
{
}

class OVT_DeploymentManagerComponent : OVT_Component
{
	[Attribute(desc: "Deployment registry containing all available deployment configs")]
	ref OVT_DeploymentRegistry m_DeploymentRegistry;
	
	[Attribute(defvalue: "{53D8FEE526831693}Prefabs/GameMode/OVT_Deployment.et", desc: "Prefab to use for deployment entities")]
	ResourceName m_DeploymentPrefab;
	
	[Attribute(defvalue: "30000", desc: "Interval for deployment evaluation in milliseconds")]
	int m_iEvaluationInterval;
	
	[Attribute(defvalue: "100", desc: "Maximum deployments per faction")]
	int m_iMaxDeploymentsPerFaction;
	
	protected ref array<ref EntityID> m_aActiveDeployments;
	protected ref map<int, ref array<ref EntityID>> m_mFactionDeployments; // factionIndex -> deployments
	protected ref map<int, int> m_mFactionResources; // factionIndex -> available resources
	protected ref array<vector> m_aAvailableSlots; // Cached slot positions
	protected bool m_bInitialized;

	//! True once this manager has subscribed to the virtualization registry's events. Guards against
	//! a second subscription: a ScriptInvoker has no Contains(), so an Insert() that ran twice would
	//! fan every restore and every wipe out over the whole deployment list twice.
	protected bool m_bVirtualizationHooked;

	static const float THREAT_EVALUATION_RADIUS = 2000; // 2km
	static const int MIN_DEPLOYMENT_DISTANCE = 100; // 100m minimum between deployments
	static const int MAX_DEPLOYMENTS_PER_EVALUATION = 10; // Maximum deployments per evaluation cycle

	//! How close a position has to be to a radio tower to count as being AT one. Read by both halves
	//! of the location classification (the precedence chain and the OR-ed bit), which is the point of
	//! it being a constant: two copies of 300 that drifted apart would classify the same spot two
	//! different ways depending on which question was asked.
	static const float RADIO_TOWER_RADIUS = 300;

	//! Ceiling on the collision-ordinal search in NextKeyOrdinal(). Reaching it means something is
	//! generating deployments on one spot in a loop, which is a bug to see in a log rather than a
	//! reason to hang.
	static const int MAX_KEY_ORDINAL = 1000;
	
	static OVT_DeploymentManagerComponent s_Instance;
	
	private ref array<IEntity> m_aFoundSlots;
	
	//------------------------------------------------------------------------------------------------
	static OVT_DeploymentManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode gameMode = GetGame().GetGameMode();
			if (gameMode)
				s_Instance = OVT_DeploymentManagerComponent.Cast(gameMode.FindComponent(OVT_DeploymentManagerComponent));
		}
		return s_Instance;
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		if (!Replication.IsServer())
			return;
			
		m_aActiveDeployments = new array<ref EntityID>;
		m_mFactionDeployments = new map<int, ref array<ref EntityID>>;
		m_mFactionResources = new map<int, int>;
		m_aAvailableSlots = new array<vector>;
		
		// Initialize deployment registry if empty
		if (!m_DeploymentRegistry)
		{
			m_DeploymentRegistry = new OVT_DeploymentRegistry();
			m_DeploymentRegistry.m_sRegistryName = "Default Registry";
		}
		
		m_bInitialized = true;
	}
	
	void PostGameStart()
	{
		if(!Replication.IsServer())
			return;

		// THE reclaim point. Subscribed here rather than at OnPostInit so it happens once the campaign
		// is actually running, and before the first evaluation below.
		HookVirtualization();

		// BASELINE FIRST, PAID-FOR SECOND. Configs marked m_bFreeAtGameStart are seeded at every
		// eligible location one second BEFORE the first evaluation, so the world the evaluator starts
		// bidding into already has its garrisons and town patrols standing. Run the other way round the
		// evaluator would spend the opening pool on the same places and the seed would then find them
		// deduped - the ordering is the whole point, not a detail.
		GetGame().GetCallqueue().CallLater(SeedFreeDeployments, 9000, false);

		//First evaluation sooner
		GetGame().GetCallqueue().CallLater(EvaluateDeployments, 10000, false);

		// Start evaluation timer
		GetGame().GetCallqueue().CallLater(EvaluateDeployments, m_iEvaluationInterval, true);
	}

	//------------------------------------------------------------------------------------------------
	// Virtualization
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Subscribes THIS manager - and nothing else in the deployments framework - to the virtualization
	//! registry's restore and wipe events.
	//!
	//! THE MANAGER SUBSCRIBES; THE MODULES NEVER DO (D7). Deployment modules are cloned config objects
	//! with no stable identity, destroyed whenever their deployment is deleted - and one of them
	//! deletes its own deployment from inside its own update. A ScriptInvoker holding a method pointer
	//! into one of those is a dangling call waiting to happen. This manager is a component with a
	//! campaign-long lifetime and already holds the deployment list, so it subscribes once and fans
	//! out.
	//!
	//! IDEMPOTENT, because a second campaign in the same session must not end up double-subscribed.
	//! Safe to run before the virtualization manager's own PostGameStart(): the invokers are created
	//! on first use by their getters and are never replaced.
	protected void HookVirtualization()
	{
		if (m_bVirtualizationHooked)
			return;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		virtualization.GetOnRecordsRestored().Insert(OnVirtualRecordsRestored);
		virtualization.GetOnGroupWiped().Insert(OnVirtualGroupWiped);

		m_bVirtualizationHooked = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Persisted group records have been restored: every deployment reclaims what it already owns.
	//!
	//! This is the reclaim point the virtualization API mandates - never a deployment's own
	//! deserialize, which races the restoration of the group entities themselves. It also fires on an
	//! in-session re-apply, which is why the fan-out has to be idempotent all the way down.
	protected void OnVirtualRecordsRestored()
	{
		array<OVT_DeploymentComponent> deployments = GetAllDeployments();
		foreach (OVT_DeploymentComponent deployment : deployments)
		{
			if (deployment)
				deployment.EnsureGroups();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A registered group has been wiped out: tell every deployment, and let the one that owns the
	//! handle do the bookkeeping.
	//!
	//! Fired BEFORE the record is removed, so a subscriber can still read it. Fired ONLY on a real
	//! wipe - the survivor mask says every slot is dead - which is what makes it safe to hang a
	//! deployment's eliminated flag off, where the old proximity despawn never could be.
	//! \param[in] handle The wiped group's registry handle.
	protected void OnVirtualGroupWiped(int handle)
	{
		array<OVT_DeploymentComponent> deployments = GetAllDeployments();
		foreach (OVT_DeploymentComponent deployment : deployments)
		{
			if (deployment)
				deployment.OnVirtualGroupWiped(handle);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The ordinal a deployment should disambiguate its base key with, so that two deployments on one
	//! rounded spot never share one force.
	//!
	//! PROBED, NOT COUNTED. The answer is derived by asking which candidate keys are already held by a
	//! live deployment, rather than by a counter, because a counter would have to survive a save: a
	//! restored deployment carries its persisted key without ever asking for an ordinal, so a
	//! session-local counter would hand a NEW deployment on that same spot the key the restored one is
	//! already using. Probing is self-healing across loads, deletions and re-applies, and a derivation
	//! happens once per deployment.
	//!
	//! Counting starts at 2 so the suffix reads as "the second one here"; ordinal 0 is the first
	//! holder and carries no suffix at all.
	//! \param[in] baseKey A base key from OVT_DeploymentVirtualKey.DeriveKey().
	//! \return 0 when the base key is free, otherwise the lowest free ordinal from 2 upwards.
	int NextKeyOrdinal(string baseKey)
	{
		if (baseKey.IsEmpty())
			return 0;

		int ordinal = 0;
		while (IsVirtualKeyTaken(OVT_DeploymentVirtualKey.Disambiguate(baseKey, ordinal)))
		{
			if (ordinal == 0)
				ordinal = 2;
			else
				ordinal++;

			if (ordinal > MAX_KEY_ORDINAL)
			{
				Print(string.Format("[Overthrow] Deployment key '%1' has run out of ordinals - something is creating deployments on one spot in a loop", baseKey), LogLevel.ERROR);
				break;
			}
		}

		return ordinal;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether any live deployment already holds this exact virtualization key.
	//!
	//! Reads the key AS IT STANDS rather than deriving one, so probing never triggers a derivation on
	//! somebody else's deployment - a deployment that has not needed a key yet holds none and cannot
	//! collide.
	//! \param[in] key The candidate key.
	//! \return True when it is already in use.
	protected bool IsVirtualKeyTaken(string key)
	{
		if (key.IsEmpty())
			return false;

		array<OVT_DeploymentComponent> deployments = GetAllDeployments();
		foreach (OVT_DeploymentComponent deployment : deployments)
		{
			if (!deployment)
				continue;

			if (deployment.GetVirtualKey() == key)
				return true;
		}

		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	void Init(IEntity owner)
	{	
		// Only initialize on server
		if(!Replication.IsServer())
			return;
			
		// Cache available slots from the world
		CacheAvailableSlots();
				
		Print("[Overthrow] DeploymentManager initialized with " + m_DeploymentRegistry.m_aDeploymentConfigs.Count() + " deployment configs", LogLevel.NORMAL);
	}
	
	
	//------------------------------------------------------------------------------------------------
	protected void CacheAvailableSlots()
	{
		m_aAvailableSlots.Clear();
		
		// Query for all slot entities in the world
		// This will depend on how slots are implemented in the base upgrade system
		m_aFoundSlots = new array<IEntity>;
		GetGame().GetWorld().QueryEntitiesBySphere(vector.Zero, 50000, FilterSlotEntities, null, EQueryEntitiesFlags.ALL);
		
		foreach (IEntity slotEntity : m_aFoundSlots)
		{
			m_aAvailableSlots.Insert(slotEntity.GetOrigin());
		}
		
		Print("[Overthrow] Cached " + m_aAvailableSlots.Count() + " available deployment slots", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool FilterSlotEntities(IEntity entity)
	{
		SCR_EditableEntityComponent editable = OVT_ComponentFinder<SCR_EditableEntityComponent>.Find(entity);
		if (editable && editable.GetEntityType() == EEditableEntityType.SLOT)
		{
			m_aFoundSlots.Insert(entity);
		}
		return true;
	}
		
	//------------------------------------------------------------------------------------------------
	// Free-at-game-start seeding
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Puts every config marked m_bFreeAtGameStart on the ground at every eligible location, free.
	//!
	//! WHY THIS EXISTS. Radio tower garrisons and town patrols became ordinary deployments bought out
	//! of the faction resource pool (D17), and on Easy that pool starts at 150 per tick while a Tower
	//! Garrison costs 50 - so a play-test found most towers simply ungarrisoned, with the evaluator's
	//! MAX_DEPLOYMENTS_PER_EVALUATION cap spreading what it could afford over minutes. A garrison is
	//! not an opportunistic purchase, it is the baseline state of the world: it is what the legacy
	//! tower spawning did unconditionally before the migration. This pass restores that, opt-in per
	//! config (user amendment, 2026-08-17).
	//!
	//! ⚠ NO PLAYER-COUNT GUARD AND NO QRF GUARD, DELIBERATELY - and this is the one place in the
	//! framework where that is true. EvaluateDeployments() returns early with nobody connected and
	//! during a QRF, and rightly so: those are DECISIONS about spending a budget on new forces. This
	//! is not a decision, it is the world's opening state, and it must be on the ground before the
	//! first player joins a dedicated server rather than a second after. Seeding also charges nothing,
	//! so a QRF's claim on the pool is untouched.
	//!
	//! ⚠ RUNS AFTER PERSISTENCE RESTORE, ALWAYS. A save is deserialized synchronously during load,
	//! long before this fires at +9 s, so every restored deployment is already registered and visible
	//! to the dedup below. That is what turns this from "seed the world" into "fill in what is
	//! genuinely missing": a continued campaign gets nothing at a location that already has its
	//! deployment, and gets one at a location whose garrison the evaluator never managed to afford.
	//!
	//! IDEMPOTENT for the same reason, so calling it twice creates nothing the second time.
	void SeedFreeDeployments()
	{
		if (!m_bInitialized || !Replication.IsServer())
			return;

		if (!m_DeploymentRegistry || !m_DeploymentRegistry.m_aDeploymentConfigs)
			return;

		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return;

		// Stale ids in the per-faction lists count towards m_iMaxDeploymentsPerFaction (BUG-028), and a
		// continued campaign can arrive here holding some.
		CleanupDestroyedDeployments();

		array<Faction> factions = new array<Faction>;
		factionManager.GetFactionsList(factions);

		int seeded = 0;
		foreach (Faction faction : factions)
		{
			seeded += SeedFactionFreeDeployments(factionManager.GetFactionIndex(faction));
		}

		if (seeded > 0)
			Print(string.Format("[Overthrow] Seeded %1 free-at-game-start deployment(s)", seeded), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Seeds one faction's free configs.
	//! \param[in] factionIndex The faction to seed for.
	//! \return How many deployments were created.
	protected int SeedFactionFreeDeployments(int factionIndex)
	{
		OVT_FactionTypeFlag factionType = GetFactionType(factionIndex);
		if (factionType == 0)
			return 0; // Civilians and anything else Overthrow does not classify

		array<ref EntityID> factionDeployments = EnsureFactionDeploymentList(factionIndex);

		int seeded = 0;
		foreach (OVT_DeploymentConfig config : m_DeploymentRegistry.m_aDeploymentConfigs)
		{
			if (!config || !config.m_bFreeAtGameStart)
				continue;

			if (!config.IsValidConfig() || !config.CanFactionUse(factionType))
				continue;

			seeded += SeedFreeConfig(config, factionIndex, factionDeployments);
		}

		return seeded;
	}

	//------------------------------------------------------------------------------------------------
	//! Seeds ONE free config at every location it is eligible for.
	//!
	//! WHAT IS DELIBERATELY NOT CONSULTED, because baseline presence is the point:
	//!   - the faction's resource pool. Nothing is charged and nothing is deducted, and the deployment
	//!     is stamped with 0 invested resources so collecting it later refunds nothing;
	//!   - m_fChance. A garrison that exists 70% of the time is not a baseline;
	//!   - m_iMinimumThreatLevel. Threat is a measure of what has already happened, and at t0 nothing
	//!     has. The config's own condition MODULES are still asked (see PassesSeedConditions) - those
	//!     answer "does this place belong to this faction", which is exactly the question that stops a
	//!     continued campaign re-garrisoning a tower the player has already taken;
	//!   - MAX_DEPLOYMENTS_PER_EVALUATION. That cap paces an ongoing spend over evaluation cycles;
	//!     there is nothing to pace here.
	//!
	//! WHAT IS: the same-name 250 m dedup, m_iMaxInstances, and the manager's per-faction ceiling.
	//! \param[in] config A config marked m_bFreeAtGameStart.
	//! \param[in] factionIndex The faction to seed for.
	//! \param[in] factionDeployments That faction's live deployment list, for the per-faction ceiling.
	//! \return How many deployments were created.
	protected int SeedFreeConfig(notnull OVT_DeploymentConfig config, int factionIndex, notnull array<ref EntityID> factionDeployments)
	{
		array<vector> candidates = CollectSeedCandidates(config.m_iAllowedLocationTypes, factionIndex);

		int seeded = 0;
		foreach (vector position : candidates)
		{
			// The per-faction ceiling is a real limit on a large map. Respected rather than bypassed -
			// but said out loud, because hitting it means some location is going without.
			if (factionDeployments.Count() >= m_iMaxDeploymentsPerFaction)
			{
				Print(string.Format("[Overthrow] Free-at-game-start seeding of '%1' stopped at the per-faction ceiling of %2 - some eligible locations will have no deployment", config.m_sDeploymentName, m_iMaxDeploymentsPerFaction), LogLevel.WARNING);
				break;
			}

			if (config.m_iMaxInstances > 0 && GetActiveInstancesOfType(config.m_sDeploymentName, factionIndex) >= config.m_iMaxInstances)
				break;

			// The same location match the evaluator makes (FindBestDeploymentConfig), asked of the
			// composite classification so a tower inside a town's bounds still reads as a tower.
			if (!config.CanUseLocationType(GetLocationTypeAtPosition(position)))
				continue;

			// THE IDEMPOTENCE, and the reason a continued campaign only gets what it is missing.
			if (HasExistingDeploymentOfType(position, factionIndex, config.m_sDeploymentName))
				continue;

			float threatLevel = CalculateThreatLevel(position, factionIndex);

			if (!PassesSeedConditions(config, position, factionIndex, threatLevel))
				continue;

			// resourcesInvested 0 IS LOAD-BEARING: a deployment nobody paid for must refund nothing
			// when it is collected, and must not show up in the GM panel as money spent.
			OVT_DeploymentComponent deployment = CreateDeployment(config, position, factionIndex, 0, threatLevel);
			if (!deployment)
				continue;

			seeded++;
		}

		return seeded;
	}

	//------------------------------------------------------------------------------------------------
	//! Every place of the kinds a config allows, for one faction.
	//!
	//! Asked PER LOCATION KIND rather than through FindDeploymentCandidates(), on purpose. That method
	//! unions the kinds wanted by every config the faction can use and then filters by suitability -
	//! which is right for the opportunistic evaluator, and wrong twice over here. It would offer a
	//! tower position to the town patrol config (a tower inside a town's bounds classifies as both),
	//! seeding a second patrol on top of the tower; and its MIN_DEPLOYMENT_DISTANCE filter exists to
	//! stop the evaluator stacking deployments, which would make a garrison unseedable purely because
	//! a town patrol happens to stand 90 m away. Seeding is per-LOCATION baseline presence, so it asks
	//! each kind of location for its own list and lets the dedup below do the deduplicating.
	//!
	//! A config that authors NO location types seeds nothing. CanUseLocationType() reads that as "no
	//! restrictions", but seeding enumerates places and "anywhere" is not a place - so marking an
	//! unrestricted config free is a no-op rather than a map-wide flood.
	//! \param[in] locationTypes The config's m_iAllowedLocationTypes.
	//! \param[in] factionIndex The faction to collect for.
	//! \return Candidate positions, possibly empty.
	protected array<vector> CollectSeedCandidates(OVT_LocationTypeFlag locationTypes, int factionIndex)
	{
		array<vector> candidates = new array<vector>;

		if (locationTypes & OVT_LocationTypeFlag.TOWN)
			candidates.InsertAll(GetTownPositions(factionIndex));

		if (locationTypes & OVT_LocationTypeFlag.BASE)
			candidates.InsertAll(GetBasePositions(factionIndex));

		if (locationTypes & OVT_LocationTypeFlag.PORT)
			candidates.InsertAll(GetPortPositions(factionIndex));

		if (locationTypes & OVT_LocationTypeFlag.AIRFIELD)
			candidates.InsertAll(GetAirfieldPositions(factionIndex));

		if (locationTypes & OVT_LocationTypeFlag.RADIO_TOWER)
			candidates.InsertAll(GetRadioTowerPositions(factionIndex));

		if (locationTypes & OVT_LocationTypeFlag.CHECKPOINT)
			candidates.InsertAll(GetCheckpointPositions(factionIndex));

		return candidates;
	}

	//------------------------------------------------------------------------------------------------
	//! The config's condition modules, asked the creation-time question.
	//!
	//! This is OVT_DeploymentComponent.CheckDeploymentConditions() WITHOUT its m_iMinimumThreatLevel
	//! floor - the modules, not the threat gate. Keeping the modules is what makes seeding safe on a
	//! continued campaign: the tower garrison's control condition refuses a tower the resistance
	//! already holds, so loading a save cannot re-garrison what the player has taken.
	//! \param[in] config The config being seeded.
	//! \param[in] position The candidate position.
	//! \param[in] factionIndex The faction being seeded for.
	//! \param[in] threatLevel The candidate's scored threat, passed through to the modules.
	//! \return True when every condition module accepts the position.
	protected bool PassesSeedConditions(notnull OVT_DeploymentConfig config, vector position, int factionIndex, float threatLevel)
	{
		foreach (OVT_BaseDeploymentModule moduleTemplate : config.m_aModules)
		{
			OVT_BaseConditionDeploymentModule conditionModule = OVT_BaseConditionDeploymentModule.Cast(moduleTemplate);
			if (conditionModule && !conditionModule.EvaluateStaticCondition(position, factionIndex, threatLevel))
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A faction's deployment list, created on first ask.
	//!
	//! MUST be called before creating a deployment for a faction that has none yet: RegisterDeployment()
	//! only inserts into this list when it already exists, so a deployment created for an unseen faction
	//! would land in m_aActiveDeployments and nowhere else - invisible to GetFactionDeployments() and
	//! uncounted by the per-faction ceiling.
	//! \param[in] factionIndex The faction to look up.
	//! \return The list, never null.
	protected array<ref EntityID> EnsureFactionDeploymentList(int factionIndex)
	{
		array<ref EntityID> factionDeployments = m_mFactionDeployments.Get(factionIndex);
		if (!factionDeployments)
		{
			factionDeployments = new array<ref EntityID>();
			m_mFactionDeployments.Set(factionIndex, factionDeployments);
		}

		return factionDeployments;
	}

	//------------------------------------------------------------------------------------------------
	//! The 30 s evaluation. UNCHANGED by the virtualization migration except that it is no longer
	//! silenced: deployments spawn their groups through the virtualization core now, so the epic's
	//! legacy-spawn kill switch has nothing left to silence here.
	//!
	//! Both early returns below are DELIBERATELY KEPT. This framework migrated group LIFECYCLE, not
	//! deployment decision-making: with nobody connected there is no one for a new deployment to be
	//! created for, and during a QRF the occupying faction's resources belong to the QRF. Existing
	//! deployments' groups live entirely on the engine's lifecycle and are unaffected by either, which
	//! is a strict improvement on the old behaviour where the whole force also stopped being maintained.
	void EvaluateDeployments()
	{
		if (!m_bInitialized || !Replication.IsServer())
			return;

		//Don't create deployments if all players are offline
		PlayerManager mgr = GetGame().GetPlayerManager();
		if(mgr.GetPlayerCount() == 0)
			return;

		//Don't create deployments during a QRF
		if(OVT_Global.GetOccupyingFaction().m_CurrentQRF)
			return;
		
		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return;
		
		// Clean up destroyed deployments first — the per-faction creation gate below
		// counts these lists, so stale IDs must not survive into the evaluation
		CleanupDestroyedDeployments();

		// Evaluate each faction's deployment needs
		array<Faction> factions = new array<Faction>;
		factionManager.GetFactionsList(factions);

		foreach (Faction faction : factions)
		{
			int factionIndex = factionManager.GetFactionIndex(faction);
			EvaluateFactionDeployments(factionIndex);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void EvaluateFactionDeployments(int factionIndex)
	{
		array<ref EntityID> factionDeployments = EnsureFactionDeploymentList(factionIndex);

		// Skip if faction has reached deployment limit
		if (factionDeployments.Count() >= m_iMaxDeploymentsPerFaction)
			return;
		
		int availableResources = m_mFactionResources.Get(factionIndex);
		
		// Find potential deployment locations
		array<vector> candidatePositions = FindDeploymentCandidates(factionIndex);

		// Calculate threat levels for all candidates and add randomness
		array<ref OVT_CandidatePosition> candidatesWithThreat = new array<ref OVT_CandidatePosition>;
		foreach (vector position : candidatePositions)
		{
			float baseThreatLevel = CalculateThreatLevel(position, factionIndex);
			// Add randomness: ±20% of base threat level
			float randomModifier = s_AIRandomGenerator.RandFloatXY(-0.2, 0.2);
			float finalThreatLevel = baseThreatLevel * (1.0 + randomModifier);
			
			OVT_CandidatePosition candidate = new OVT_CandidatePosition(position, finalThreatLevel);
			candidatesWithThreat.Insert(candidate);
		}
		
		// Sort candidates by threat level (highest first)
		candidatesWithThreat.Sort(true);
		
		int numDeployments = 0;
		
		// Evaluate each candidate position in order of threat level
		foreach (OVT_CandidatePosition candidate : candidatesWithThreat)
		{
			// Find suitable deployment config for this position and threat level
			OVT_DeploymentConfig bestConfig = FindBestDeploymentConfig(candidate.position, factionIndex, candidate.threatLevel, availableResources);
			if (bestConfig)
			{
				// Check if we already have this type of deployment nearby
				if (HasExistingDeploymentOfType(candidate.position, factionIndex, bestConfig.m_sDeploymentName))
				{
					continue; // Skip this position, deployment already exists nearby
				}
				
				int deploymentCost = bestConfig.GetTotalResourceCost();
				if (availableResources >= deploymentCost)
				{
					CreateDeployment(bestConfig, candidate.position, factionIndex, deploymentCost, candidate.threatLevel);
					availableResources -= deploymentCost;
					m_mFactionResources.Set(factionIndex, availableResources);
					numDeployments++;
				}
			}

			if (numDeployments >= MAX_DEPLOYMENTS_PER_EVALUATION)
				break;
		}
	}
	
	
	//------------------------------------------------------------------------------------------------
	protected array<vector> FindDeploymentCandidates(int factionIndex)
	{
		array<vector> candidates = new array<vector>;
		
		// Get faction type to determine which configs this faction can use
		OVT_FactionTypeFlag factionType = GetFactionType(factionIndex);
		
		// Collect all location types needed by available configs for this faction
		OVT_LocationTypeFlag neededLocationTypes = 0;
		foreach (OVT_DeploymentConfig config : m_DeploymentRegistry.m_aDeploymentConfigs)
		{
			if (config.IsValidConfig() && config.CanFactionUse(factionType))
			{
				neededLocationTypes = neededLocationTypes | config.m_iAllowedLocationTypes;
			}
		}
		
		// Generate candidates based on needed location types
		if (neededLocationTypes & OVT_LocationTypeFlag.TOWN)
		{
			candidates.InsertAll(GetTownPositions(factionIndex));
		}
		
		if (neededLocationTypes & OVT_LocationTypeFlag.BASE)
		{
			candidates.InsertAll(GetBasePositions(factionIndex));
		}
		
		if (neededLocationTypes & OVT_LocationTypeFlag.PORT)
		{
			candidates.InsertAll(GetPortPositions(factionIndex));
		}
		
		if (neededLocationTypes & OVT_LocationTypeFlag.AIRFIELD)
		{
			candidates.InsertAll(GetAirfieldPositions(factionIndex));
		}
		
		if (neededLocationTypes & OVT_LocationTypeFlag.RADIO_TOWER)
		{
			candidates.InsertAll(GetRadioTowerPositions(factionIndex));
		}
		
		if (neededLocationTypes & OVT_LocationTypeFlag.CHECKPOINT)
		{
			candidates.InsertAll(GetCheckpointPositions(factionIndex));
		}
		
		// Filter by suitability
		array<vector> suitableCandidates = new array<vector>;
		foreach (vector pos : candidates)
		{
			if (IsPositionSuitableForDeployment(pos, factionIndex))
				suitableCandidates.Insert(pos);
		}
		
		return suitableCandidates;
	}
	
	//------------------------------------------------------------------------------------------------
	protected array<vector> GetTownPositions(int factionIndex)
	{
		array<vector> positions = new array<vector>;
		
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		if (!townManager)
			return positions;
			
		foreach (OVT_TownData townData : townManager.m_Towns)
		{
			if (townData && IsPositionRelevantToFaction(townData.location, factionIndex))
			{
				positions.Insert(townData.location);
			}
		}
		
		return positions;
	}
	
	//------------------------------------------------------------------------------------------------
	protected array<vector> GetBasePositions(int factionIndex)
	{
		array<vector> positions = new array<vector>;
		
		OVT_OccupyingFactionManager ofManager = OVT_Global.GetOccupyingFaction();
		if (!ofManager)
			return positions;
			
		foreach (OVT_BaseData baseData : ofManager.m_Bases)
		{
			if (baseData && IsPositionRelevantToFaction(baseData.location, factionIndex))
			{
				positions.Insert(baseData.location);
			}
		}
		
		return positions;
	}
	
	//------------------------------------------------------------------------------------------------
	protected array<vector> GetPortPositions(int factionIndex)
	{
		array<vector> positions = new array<vector>;
		
		// TODO: Implement port detection based on actual port entities in the world
		
		return positions;
	}
	
	//------------------------------------------------------------------------------------------------
	protected array<vector> GetAirfieldPositions(int factionIndex)
	{
		array<vector> positions = new array<vector>;
		
		// TODO: Implement airfield detection based on actual airfield entities in the world
		
		return positions;
	}
	
	//------------------------------------------------------------------------------------------------
	protected array<vector> GetRadioTowerPositions(int factionIndex)
	{
		array<vector> positions = new array<vector>;
		
		OVT_OccupyingFactionManager ofManager = OVT_Global.GetOccupyingFaction();
		if (!ofManager)
			return positions;
			
		foreach (OVT_RadioTowerData towerData : ofManager.m_RadioTowers)
		{
			if (towerData && IsPositionRelevantToFaction(towerData.location, factionIndex))
			{
				positions.Insert(towerData.location);
			}
		}
		
		return positions;
	}
	
	//------------------------------------------------------------------------------------------------
	protected array<vector> GetCheckpointPositions(int factionIndex)
	{
		array<vector> positions = new array<vector>;
		
		// TODO: Implement checkpoint detection based on actual checkpoint entities or road intersections
		
		return positions;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool IsPositionNearWater(vector position)
	{
		// TODO: Implement proper water detection
		// For now, return false - this would need proper terrain analysis
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool IsPositionSuitableForAirfield(vector position)
	{
		// TODO: Implement terrain analysis for airfield suitability
		// For now, return false - this would need terrain slope and size analysis
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool IsPositionBetweenMajorLocations(vector position)
	{
		// Check if position is roughly between a town and a base
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		OVT_OccupyingFactionManager ofManager = OVT_Global.GetOccupyingFaction();
		
		if (!townManager || !ofManager)
			return false;
			
		foreach (OVT_TownData townData : townManager.m_Towns)
		{
			if (!townData)
				continue;
				
			foreach (OVT_BaseData baseData : ofManager.m_Bases)
			{
				if (!baseData)
					continue;
					
				float townDistance = vector.Distance(position, townData.location);
				float baseDistance = vector.Distance(position, baseData.location);
				float townToBaseDistance = vector.Distance(townData.location, baseData.location);
				
				// Check if position is roughly on the line between town and base
				if (townDistance > 500 && baseDistance > 500 && // Not too close to either
					townDistance + baseDistance < townToBaseDistance * 1.2) // Roughly on the path
				{
					return true;
				}
			}
		}
		
		return false;
	}
	
	protected OVT_DeploymentComponent GetDeploymentFromEntity(IEntity entity)
	{
		return OVT_DeploymentComponent.Cast(entity.FindComponent(OVT_DeploymentComponent));
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool IsPositionSuitableForDeployment(vector position, int factionIndex)
	{
		// Check minimum distance to existing deployments
		foreach (EntityID deploymentID : m_aActiveDeployments)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(deploymentID);
			if (!entity)
				continue;
				
			OVT_DeploymentComponent deployment = GetDeploymentFromEntity(entity);
			if (!deployment)
				continue;
				
			float distance = vector.Distance(position, deployment.GetPosition());
			if (distance < MIN_DEPLOYMENT_DISTANCE)
				return false;
		}
		
		// Check if position is in suitable terrain
		TraceParam param = new TraceParam();
		param.Start = position + Vector(0, 100, 0);
		param.End = position + Vector(0, -100, 0);
		param.Flags = TraceFlags.WORLD;
		
		float result = GetGame().GetWorld().TraceMove(param, null);
		if (result >= 1.0)
			return false; // No ground found
		
		// Additional checks could include:
		// - Not in restricted areas
		// - Not too close to player bases
		// - Suitable for faction type
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool HasExistingDeploymentOfType(vector position, int factionIndex, string deploymentName, float radius = 250)
	{
		foreach (EntityID deploymentID : m_aActiveDeployments)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(deploymentID);
			if (!entity)
				continue;
				
			OVT_DeploymentComponent deployment = GetDeploymentFromEntity(entity);
			if (!deployment)
				continue;
				
			if (deployment.GetControllingFaction() == factionIndex && 
				deployment.GetDeploymentName() == deploymentName)
			{
				float distance = vector.Distance(position, deployment.GetPosition());
				if (distance <= radius)
					return true;
			}
		}
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool IsPositionRelevantToFaction(vector position, int factionIndex)
	{
		// Determine if a position is strategically relevant to a faction
		// This is a simplified implementation
		
		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return false;
		
		Faction faction = factionManager.GetFactionByIndex(factionIndex);
		if (!faction)
			return false;
		
		string factionKey = faction.GetFactionKey();
		
		// Resistance is interested in populated areas
		if (factionKey == OVT_Global.GetConfig().GetPlayerFaction().GetFactionKey())
		{
			// Check proximity to towns
			OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
			if (townManager)
			{
				foreach (OVT_TownData townData : townManager.m_Towns)
				{
					if (townData && vector.Distance(position, townData.location) < 2000)
						return true;
				}
			}
		}
		
		// Occupying forces are interested in strategic control points
		if (factionKey == OVT_Global.GetConfig().GetOccupyingFaction().GetFactionKey())
		{
			// Always relevant for occupying forces
			return true;
		}
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	protected float CalculateThreatLevel(vector position, int factionIndex)
	{
		float threat = 0;
		
		// Base threat level
		OVT_OccupyingFactionManager ofManager = OVT_Global.GetOccupyingFaction();
		threat += ofManager.GetThreatLevel();
		
		threat += ofManager.GetThreatByLocation(position);
		
		return threat;
	}
	
	//------------------------------------------------------------------------------------------------
	protected OVT_DeploymentConfig FindBestDeploymentConfig(vector position, int factionIndex, float threatLevel, int availableResources)
	{
		array<OVT_DeploymentConfig> suitableConfigs = new array<OVT_DeploymentConfig>;
		
		// Filter configs by conditions and resources
		foreach (OVT_DeploymentConfig config : m_DeploymentRegistry.m_aDeploymentConfigs)
		{
			if (!config.IsValidConfig())
				continue;
			
			// Check if faction can use this config
			OVT_FactionTypeFlag factionType = GetFactionType(factionIndex);
			if (!config.CanFactionUse(factionType))
			{				
				continue;
			}
			
			// Check if location type is compatible
			OVT_LocationTypeFlag locationType = GetLocationTypeAtPosition(position);
			if (!config.CanUseLocationType(locationType))
			{
				continue;
			}
			
			// Check resource cost
			int cost = config.GetTotalResourceCost();
			if (cost > availableResources)
				continue;
			
			// Check deployment conditions
			if (OVT_DeploymentComponent.CheckDeploymentConditions(config, position, factionIndex, threatLevel))
			{
				// Check maximum instances limit
				if (config.m_iMaxInstances > 0 && GetActiveInstancesOfType(config.m_sDeploymentName, factionIndex) >= config.m_iMaxInstances)
				{
					continue; // Skip this config - max instances reached
				}
				
				// Check chance - if less than 1.0, roll for deployment creation
				if (config.m_fChance >= 100.0 || s_AIRandomGenerator.RandFloatXY(0,100) <= config.m_fChance)
				{
					suitableConfigs.Insert(config);
				}
			}
		}
		
		if (suitableConfigs.IsEmpty())
			return null;
		
		// Select best config based on priority and threat level
		OVT_DeploymentConfig bestConfig = null;
		int bestPriority = 999;
		
		foreach (OVT_DeploymentConfig config : suitableConfigs)
		{
			// Lower priority value = higher priority
			if (config.m_iPriority < bestPriority)
			{
				bestPriority = config.m_iPriority;
				bestConfig = config;
			}
		}
		
		return bestConfig;
	}
	
	//------------------------------------------------------------------------------------------------
	protected OVT_FactionTypeFlag GetFactionType(int factionIndex)
	{
		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return 0;
		
		Faction faction = factionManager.GetFactionByIndex(factionIndex);
		if (!faction)
			return 0;
		
		string factionKey = faction.GetFactionKey();
		
		string occupyingKey = OVT_Global.GetConfig().GetOccupyingFaction().GetFactionKey();
		string playerKey = OVT_Global.GetConfig().GetPlayerFaction().GetFactionKey();
		string supportingKey = OVT_Global.GetConfig().GetSupportingFaction().GetFactionKey();
		
		if (factionKey == occupyingKey)
			return OVT_FactionTypeFlag.OCCUPYING_FACTION;
		else if (factionKey == playerKey)
			return OVT_FactionTypeFlag.RESISTANCE_FACTION;
		else if (factionKey == supportingKey)
			return OVT_FactionTypeFlag.SUPPORTING_FACTION;
		
		return 0;
	}
	
	//------------------------------------------------------------------------------------------------
	//! What kind of place a position is, for matching against a config's m_iAllowedLocationTypes.
	//!
	//! ONE FLAG, PLUS RADIO_TOWER WHEN THERE IS A TOWER. The classification below is a precedence
	//! chain that returns the FIRST kind it matches - town, then base, then port, airfield, tower,
	//! checkpoint - so a radio tower standing inside a town's bounds or within 500 m of a base
	//! classified as TOWN or BASE and could never be offered to a tower-only config. Every radio
	//! tower on Eden is near something, so the Tower Garrison config would simply never have fired.
	//!
	//! The fix is deliberately the smallest one that works: compute the single value EXACTLY as
	//! before, then OR the RADIO_TOWER bit in on top. Nothing moves in the precedence, so no existing
	//! config's candidate acceptance changes - a town centre near a tower still reads as a TOWN to
	//! the Town Patrol config, because CanUseLocationType is a bitwise test and the TOWN bit is still
	//! there. Returning the full union of every matching kind was rejected for the opposite reason:
	//! it would newly make town centres within 500 m of a base acceptable to the BASE-only vehicle
	//! patrol configs, which is a behaviour change nobody asked for.
	//! \param[in] position The candidate position.
	//! \return The location kind, with RADIO_TOWER OR-ed in when a tower is within range.
	OVT_LocationTypeFlag GetLocationTypeAtPosition(vector position)
	{
		OVT_LocationTypeFlag locationType = GetPrimaryLocationTypeAtPosition(position);

		if (IsNearRadioTower(position))
			locationType = locationType | OVT_LocationTypeFlag.RADIO_TOWER;

		return locationType;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a radio tower stands within the classification radius of a position.
	//!
	//! ONE definition, used by both the precedence chain and the OR above, so the two can never drift
	//! apart and start disagreeing about what "at a radio tower" means.
	//! \param[in] position The position to test.
	//! \return True when at least one radio tower is inside RADIO_TOWER_RADIUS.
	protected bool IsNearRadioTower(vector position)
	{
		OVT_OccupyingFactionManager ofManager = OVT_Global.GetOccupyingFaction();
		if (!ofManager)
			return false;

		foreach (OVT_RadioTowerData towerData : ofManager.m_RadioTowers)
		{
			if (towerData && vector.Distance(position, towerData.location) < RADIO_TOWER_RADIUS)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The single, first-match location kind - the classification exactly as it has always been.
	//!
	//! ⚠ CALLERS WANT GetLocationTypeAtPosition() ABOVE, not this. It is split out so the RADIO_TOWER
	//! bit can be added without disturbing a line of the precedence, and it is reachable only so that
	//! "the bit was OR-ed in, nothing was replaced" is assertable as a live claim rather than an
	//! inspection of the diff. Matching a config against this value would silently lose every tower.
	//! \param[in] position The candidate position.
	//! \return The first location kind that matches, or OPEN_TERRAIN.
	OVT_LocationTypeFlag GetPrimaryLocationTypeAtPosition(vector position)
	{
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		if (!townManager)
			return OVT_LocationTypeFlag.OPEN_TERRAIN;
		
		// Check if position is in a town
		OVT_TownData townData = townManager.GetNearestTown(position);
		if (townData && townData.IsWithinTownBounds(position))
		{
			return OVT_LocationTypeFlag.TOWN;
		}
		
		// Check if position is near a base
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		if (of)
		{
			OVT_BaseData baseData = of.GetNearestBase(position);
			if (baseData && vector.Distance(position, baseData.location) < 500) // 500m radius for base
			{
				// Could differentiate between different base types here
				return OVT_LocationTypeFlag.BASE;
			}
		}
		
		// Check if position is near water (potential port)
		if (IsPositionNearWater(position))
		{
			return OVT_LocationTypeFlag.PORT;
		}
		
		// Check if position is suitable for airfield
		if (IsPositionSuitableForAirfield(position))
		{
			return OVT_LocationTypeFlag.AIRFIELD;
		}
		
		// Check if position is near an actual radio tower
		if (IsNearRadioTower(position))
		{
			return OVT_LocationTypeFlag.RADIO_TOWER;
		}

		// Check if position could be a checkpoint (between towns/bases)
		// TODO: Implement proper road intersection detection
		// For now, check if it's between major locations
		if (IsPositionBetweenMajorLocations(position))
		{
			return OVT_LocationTypeFlag.CHECKPOINT;
		}
		
		// Default to open terrain
		return OVT_LocationTypeFlag.OPEN_TERRAIN;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Create a deployment marker and initialize it from a config.
	//! \param[in] config Deployment config to run.
	//! \param[in] position World position for the marker entity.
	//! \param[in] factionIndex Owning faction.
	//! \param[in] resourcesInvested Resources spent creating it (stamped so RecoverResources and the GM snapshot are non-zero).
	//! \param[in] threatLevel Threat level the candidate position scored at.
	OVT_DeploymentComponent CreateDeployment(OVT_DeploymentConfig config, vector position, int factionIndex, int resourcesInvested = 0, float threatLevel = 0)
	{
		if (config)
			Print(string.Format("[Overthrow] Creating deployment '%1' for faction %2", config.m_sDeploymentName, factionIndex), LogLevel.NORMAL);
		
		if (!config || !config.IsValidConfig())
			return null;
		
		// Create deployment entity
		if (!m_DeploymentPrefab || m_DeploymentPrefab.IsEmpty())
		{
			Print("Deployment prefab not configured", LogLevel.ERROR);
			return null;
		}
		
		Resource deploymentPrefab = Resource.Load(m_DeploymentPrefab);
		if (!deploymentPrefab)
		{
			Print(string.Format("Failed to load deployment prefab: %1", m_DeploymentPrefab), LogLevel.ERROR);
			return null;
		}
		
		// Create transform matrix
		vector mat[4];
		Math3D.MatrixIdentity4(mat);
		mat[3] = position;
		
		// Spawn deployment entity
		IEntity deploymentEntity = OVT_WorldUtils.SpawnEntityPrefabMatrix(deploymentPrefab.GetResource().GetResourceName(), mat);
		if (!deploymentEntity)
		{
			Print("Failed to spawn deployment entity", LogLevel.ERROR);
			return null;
		}
		
		// Get deployment component and initialize
		OVT_DeploymentComponent deployment = OVT_DeploymentComponent.Cast(deploymentEntity.FindComponent(OVT_DeploymentComponent));
		if (!deployment)
		{
			Print("Deployment entity missing OVT_DeploymentComponent", LogLevel.ERROR);
			delete deploymentEntity;
			return null;
		}
		
		// Include the deployment marker in save points. Its units are deliberately NOT persisted -
		// the spawning modules create and delete them by player proximity - so this entity is the
		// only durable record that the faction has a force committed here.
		OVT_PersistenceTracking.Track(deploymentEntity);

		deployment.InitializeDeployment(config, factionIndex);

		// Stamp what this deployment cost and the threat it answered. InitializeDeployment does not
		// write either, so without this RecoverResources() refunds 0 and the GM snapshot reads 0.
		deployment.SetResourcesInvested(resourcesInvested);
		deployment.SetThreatLevel(threatLevel);

		string townName = OVT_Global.GetTowns().GetNearestTownName(position);
				
		Print(string.Format("[Overthrow] Created deployment '%1' for faction %2 near %3", config.m_sDeploymentName, factionIndex, townName), LogLevel.NORMAL);
		
		return deployment;
	}
	
	//------------------------------------------------------------------------------------------------
	void RegisterDeployment(OVT_DeploymentComponent deployment)
	{
		if (!deployment)
			return;
			
		EntityID deploymentID = deployment.GetOwner().GetID();
		if (m_aActiveDeployments.Contains(deploymentID))
			return;
		
		m_aActiveDeployments.Insert(deploymentID);
						
		// Add to faction-specific list
		int factionIndex = deployment.GetControllingFaction();
		array<ref EntityID> factionDeployments = m_mFactionDeployments.Get(factionIndex);
		if (factionDeployments && !factionDeployments.Contains(deploymentID))
		{
			factionDeployments.Insert(deploymentID);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void UnregisterDeployment(OVT_DeploymentComponent deployment)
	{
		if (!deployment)
			return;
		
		EntityID deploymentID = deployment.GetOwner().GetID();
		int index = m_aActiveDeployments.Find(deploymentID);
		if (index != -1)
			m_aActiveDeployments.Remove(index);
		
		// Remove from faction-specific list
		int factionIndex = deployment.GetControllingFaction();
		array<ref EntityID> factionDeployments = m_mFactionDeployments.Get(factionIndex);
		if (factionDeployments)
		{
			int deploymentIndex = factionDeployments.Find(deploymentID);
			if (deploymentIndex != -1)
				factionDeployments.Remove(deploymentIndex);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void CleanupDestroyedDeployments()
	{
		for (int i = m_aActiveDeployments.Count() - 1; i >= 0; i--)
		{
			EntityID deploymentID = m_aActiveDeployments[i];
			IEntity deployment = GetGame().GetWorld().FindEntityByID(deploymentID);
			if (!deployment)
			{
				m_aActiveDeployments.Remove(i);
			}
		}

		// The per-faction lists gate deployment creation (m_iMaxDeploymentsPerFaction) —
		// dead IDs left here accumulate until the cap and silently halt all deploying (BUG-028)
		foreach (int factionIndex, array<ref EntityID> factionDeployments : m_mFactionDeployments)
		{
			for (int i = factionDeployments.Count() - 1; i >= 0; i--)
			{
				IEntity deployment = GetGame().GetWorld().FindEntityByID(factionDeployments[i]);
				if (!deployment)
				{
					factionDeployments.Remove(i);
				}
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Resource management
	//------------------------------------------------------------------------------------------------
	void AddFactionResources(int factionIndex, int amount)
	{
		int current = m_mFactionResources.Get(factionIndex);
		m_mFactionResources.Set(factionIndex, current + amount);
	}
	
	//------------------------------------------------------------------------------------------------
	void SubtractFactionResources(int factionIndex, int amount)
	{
		int current = m_mFactionResources.Get(factionIndex);
		m_mFactionResources.Set(factionIndex, Math.Max(0, current - amount));
	}
	
	//------------------------------------------------------------------------------------------------
	int GetFactionResources(int factionIndex)
	{
		return m_mFactionResources.Get(factionIndex);
	}
	
	//------------------------------------------------------------------------------------------------
	map<int, int> GetAllFactionResources()
	{
		return m_mFactionResources;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetAllFactionResources(map<int, int> resources)
	{
		m_mFactionResources = resources;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies persisted per-faction resource pools.
	//!
	//! Called from OVT_DeploymentManagerSerializer.Deserialize().
	//!
	//! Refills the EXISTING map rather than replacing it (unlike SetAllFactionResources), so nothing
	//! already holding a reference to it is left pointing at the old one.
	//!
	//! IDEMPOTENT: a clear and a refill, safe to run again on a live session.
	//! \param[in] factionIndices Faction indices, index-aligned with resources.
	//! \param[in] resources Resource pool per faction.
	void ApplyPersistedFactionResources(array<int> factionIndices, array<int> resources)
	{
		// Server-only component: the collections are not allocated on clients.
		if (!m_mFactionResources)
			return;

		m_mFactionResources.Clear();

		if (!factionIndices || !resources)
			return;

		int count = factionIndices.Count();
		if (resources.Count() < count)
			count = resources.Count();

		for (int i = 0; i < count; i++)
		{
			m_mFactionResources.Set(factionIndices[i], resources[i]);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Utility methods
	//------------------------------------------------------------------------------------------------
	array<OVT_DeploymentComponent> GetDeploymentsInRadius(vector position, float radius)
	{
		array<OVT_DeploymentComponent> nearbyDeployments = new array<OVT_DeploymentComponent>;
		
		foreach (EntityID deploymentID : m_aActiveDeployments)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(deploymentID);
			if (!entity)
				continue;
				
			OVT_DeploymentComponent deployment = GetDeploymentFromEntity(entity);
			if (!deployment)
				continue;
				
			float distance = vector.Distance(position, deployment.GetPosition());
			if (distance <= radius)
				nearbyDeployments.Insert(deployment);
		}
		
		return nearbyDeployments;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get the number of active instances of a specific deployment type for a faction
	protected int GetActiveInstancesOfType(string deploymentName, int factionIndex)
	{
		int instanceCount = 0;
		
		foreach (EntityID deploymentID : m_aActiveDeployments)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(deploymentID);
			if (!entity)
				continue;
				
			OVT_DeploymentComponent deployment = GetDeploymentFromEntity(entity);
			if (!deployment)
				continue;
				
			if (deployment.GetControllingFaction() == factionIndex && deployment.GetDeploymentName() == deploymentName)
			{
				instanceCount++;
			}
		}
		
		return instanceCount;
	}
	
	//------------------------------------------------------------------------------------------------
	protected float GetNearestPlayerDistance(vector position)
	{
		float nearestDistance = float.MAX;
		
		array<int> players = new array<int>;
		GetGame().GetPlayerManager().GetPlayers(players);
		
		foreach (int playerId : players)
		{
			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!player)
				continue;
			
			float distance = vector.Distance(player.GetOrigin(), position);
			if (distance < nearestDistance)
				nearestDistance = distance;
		}
		
		return nearestDistance;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool HasRecentBattleNearby(vector position)
	{
		// TODO: Implement battle tracking system
		// For now, return false
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	// Public API
	//------------------------------------------------------------------------------------------------
	array<OVT_DeploymentComponent> GetAllDeployments()
	{
		array<OVT_DeploymentComponent> deployments = new array<OVT_DeploymentComponent>;
		foreach (EntityID deploymentID : m_aActiveDeployments)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(deploymentID);
			if (!entity)
				continue;
				
			OVT_DeploymentComponent deployment = GetDeploymentFromEntity(entity);
			if (deployment)
				deployments.Insert(deployment);
		}
		return deployments;
	}
	
	//------------------------------------------------------------------------------------------------
	array<OVT_DeploymentComponent> GetFactionDeployments(int factionIndex)
	{
		array<OVT_DeploymentComponent> deployments = new array<OVT_DeploymentComponent>;
		array<ref EntityID> factionDeploymentIDs = m_mFactionDeployments.Get(factionIndex);
		if (!factionDeploymentIDs)
			return deployments;
			
		foreach (EntityID deploymentID : factionDeploymentIDs)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(deploymentID);
			if (!entity)
				continue;
				
			OVT_DeploymentComponent deployment = GetDeploymentFromEntity(entity);
			if (deployment)
				deployments.Insert(deployment);
		}
		return deployments;
	}
	
	//------------------------------------------------------------------------------------------------
	void ForceCreateDeployment(OVT_DeploymentConfig config, vector position, int factionIndex, int resourcesInvested = 0, float threatLevel = 0)
	{
		CreateDeployment(config, position, factionIndex, resourcesInvested, threatLevel);
	}

	//------------------------------------------------------------------------------------------------
	void DeleteDeployment(OVT_DeploymentComponent deployment)
	{
		if (deployment)
			deployment.DestroyDeployment();
	}
	
	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print("=== Deployment Manager Debug Info ===");
		Print(string.Format("Total Deployments: %1", m_aActiveDeployments.Count()));
		Print(string.Format("Available Configs: %1", m_DeploymentRegistry.m_aDeploymentConfigs.Count()));
		Print(string.Format("Cached Slots: %1", m_aAvailableSlots.Count()));
		
		foreach (int factionIndex, array<ref EntityID> deploymentIDs : m_mFactionDeployments)
		{			
			int resources = m_mFactionResources.Get(factionIndex);
			Print(string.Format("Faction %1: %2 deployments, %3 resources", factionIndex, deploymentIDs.Count(), resources));
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if a deployment with the specified name exists near the given position
	//! \param deploymentName The name of the deployment to search for
	//! \param position The position to search around
	//! \param radius The search radius in meters
	//! \return True if a deployment with that name exists within the radius
	bool HasDeploymentNearPosition(string deploymentName, vector position, float radius = 1000)
	{
		if (!m_aActiveDeployments)
			return false;
			
		foreach (EntityID deploymentID : m_aActiveDeployments)
		{
			IEntity deploymentEntity = GetGame().GetWorld().FindEntityByID(deploymentID);
			if (!deploymentEntity)
				continue;
				
			OVT_DeploymentComponent deploymentComp = OVT_DeploymentComponent.Cast(deploymentEntity.FindComponent(OVT_DeploymentComponent));
			if (!deploymentComp)
				continue;
				
			// Check if deployment name matches
			OVT_DeploymentConfig config = deploymentComp.GetConfig();
			if (!config || config.m_sDeploymentName != deploymentName)
				continue;
				
			// Check if within radius
			float distance = vector.Distance(position, deploymentEntity.GetOrigin());
			if (distance <= radius)
				return true;
		}
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get the first active deployment with the specified name near the given position
	//! \param deploymentName The name of the deployment to search for
	//! \param position The position to search around
	//! \param radius The search radius in meters
	//! \return The deployment component if found, null otherwise
	OVT_DeploymentComponent GetDeploymentNearPosition(string deploymentName, vector position, float radius = 1000)
	{
		if (!m_aActiveDeployments)
			return null;
			
		foreach (EntityID deploymentID : m_aActiveDeployments)
		{
			IEntity deploymentEntity = GetGame().GetWorld().FindEntityByID(deploymentID);
			if (!deploymentEntity)
				continue;
				
			OVT_DeploymentComponent deploymentComp = OVT_DeploymentComponent.Cast(deploymentEntity.FindComponent(OVT_DeploymentComponent));
			if (!deploymentComp)
				continue;
				
			// Check if deployment name matches
			OVT_DeploymentConfig config = deploymentComp.GetConfig();
			if (!config || config.m_sDeploymentName != deploymentName)
				continue;
				
			// Check if within radius
			float distance = vector.Distance(position, deploymentEntity.GetOrigin());
			if (distance <= radius)
				return deploymentComp;
		}
		
		return null;
	}
}