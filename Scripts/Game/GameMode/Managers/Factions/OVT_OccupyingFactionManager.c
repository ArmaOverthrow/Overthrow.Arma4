class OVT_OccupyingFactionManagerClass: OVT_ComponentClass
{
}

class OVT_BaseUpgradeData : Managed
{
	string type;
	int resources;
	ref array<ref OVT_BaseUpgradeGroupData> groups = {};
	string tag = "";
	vector pos;
}

class OVT_BaseUpgradeGroupData : Managed
{
	string prefab;
	vector position;
}

class OVT_BaseData : Managed
{
	[NonSerialized()]
	int id;

	int faction;

	vector location;
	ref array<vector> slotsFilled = {};

	ref array<ref OVT_BaseUpgradeData> upgrades = {};

	[NonSerialized()]
	EntityID entId;

	[SortAttribute(),NonSerialized()]
	int sortBy;

	//! No land route reaches this base - see OVT_BaseControllerComponent.m_bLandIsolated.
	//!
	//! NOT SERIALIZED, because it is WORLD-DERIVED like the location it is read beside: the map author
	//! states it on the controller and discovery copies it here on every Init. Persisting it would let
	//! a save outvote a map that had since been corrected.
	[NonSerialized()]
	bool landIsolated;

	bool IsOccupyingFaction()
	{
		return faction == OVT_Global.GetConfig().GetOccupyingFactionIndex();
	}

	static OVT_BaseData Get(vector pos)
	{
		return OVT_Global.GetOccupyingFaction().GetNearestBase(pos);
	}
}

class OVT_RadioTowerData : Managed
{
	[NonSerialized()]
	int id;

	int faction;
	vector location;

	//! Seconds of sabotage downtime left. Counted down by the server in CheckRadioTowers;
	//! clients only ever receive snapshots (on sabotage, on expiry, and via JIP)
	float disabledRemaining;

	//! Client side only. World time (ms) at which the snapshot above arrived, so a client can count
	//! the timer down locally instead of displaying a frozen number between snapshots. Never sent,
	//! never persisted - it is meaningless on any machine other than the one that stamped it.
	[NonSerialized()]
	float disabledStamp;

	//! THERE IS NO GARRISON LIST ON A TOWER ANY MORE. A tower's garrison is an ordinary deployment
	//! (Configs/Deployment/Deployment_TowerGarrison.conf) whose groups the virtualization core owns.
	//! Nothing here may rebuild one: the old list held whatever was materialised right now, and
	//! "the list is empty this tick" is precisely what used to make walking away capture a tower.

	bool IsOccupyingFaction()
	{
		return faction == OVT_Global.GetConfig().GetOccupyingFactionIndex();
	}

	//! A sabotaged tower broadcasts nothing for either side until the timer runs out
	bool IsDisabled()
	{
		return disabledRemaining > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes a fresh downtime snapshot and stamps when it landed. Use this rather than assigning
	//! disabledRemaining directly, or a client's countdown will run from the wrong instant.
	//! \param[in] seconds Seconds of downtime left as of now
	void SetDisabledRemaining(float seconds)
	{
		disabledRemaining = seconds;
		disabledStamp = GetGame().GetWorld().GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds of downtime left, live on both sides. The server owns the countdown and ticks
	//! disabledRemaining itself in CheckRadioTowers; a client only gets snapshots, so it extrapolates
	//! from the last one it received.
	//! \return Seconds remaining, never negative.
	float GetDisabledRemaining()
	{
		if(disabledRemaining <= 0) return 0;
		if(Replication.IsServer()) return disabledRemaining;

		float elapsed = (GetGame().GetWorld().GetWorldTime() - disabledStamp) / 1000;
		float remaining = disabledRemaining - elapsed;
		if(remaining < 0) return 0;

		return remaining;
	}
}

enum OVT_TargetType
{
	BASE,
	BROADCAST_TOWER,
	FOB,
	WAREHOUSE,
	CAMP
}

enum OVT_OrderType
{
	ATTACK,
	DEFEND,
	DESTROY
}

class OVT_TargetData
{
	OVT_TargetType type;
	vector location;
	int assignedBase;
	bool completed;
	OVT_OrderType order;
}

class OVT_OccupyingFactionManager: OVT_Component
{
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "QRF Controller Prefab", params: "et", category: "Controllers")]
	ResourceName m_pQRFControllerPrefab;


	bool m_bDistributeInitial = true;

	//! Resources a legacy save conversion owes the occupying faction's deployment pool but has not
	//! handed over yet. NEVER PERSISTED and never replicated - it exists only between a deserialization
	//! and the first safe moment to credit it. See QueueLegacyUpgradeRefund() for why it cannot be
	//! credited on the spot.
	protected int m_iPendingLegacyRefund;

	int m_iResources;
	float m_iThreat;
	ref array<ref OVT_BaseData> m_Bases = new array<ref OVT_BaseData>;
	ref array<ref OVT_RadioTowerData> m_RadioTowers = new array<ref OVT_RadioTowerData>;

	ref array<ref OVT_TargetData> m_aKnownTargets = new array<ref OVT_TargetData>;

	//! THE HUNTER-KILLER SWEEP (occupying/vehicles T6.4). ONE LIVE SWEEP AT A TIME - a single-slot field,
	//! never a list - paired with a bool for the same reason Phase 5's m_bHasSortie is paired with its
	//! own id: EntityID has no meaningful "unset" test from script. Public, not protected, matching every
	//! other piece of QRF/target bookkeeping on this class (m_CurrentQRF, m_bQRFActive) rather than the
	//! newer Deployments-module convention of protected fields with getters - the Init tier drives this
	//! dispatcher directly, the same way it drives SendMountedEchelon and OnBattleStarted.
	EntityID m_HunterKillerDeployment;
	bool m_bHunterKillerActive;

	protected int m_iOccupyingFactionIndex;
	protected int m_iPlayerFactionIndex;

	OVT_QRFControllerComponent m_CurrentQRF;
	protected OVT_BaseControllerComponent m_CurrentQRFBase;
	protected OVT_TownData m_CurrentQRFTown;

	bool m_bQRFActive = false;

	//------------------------------------------------------------------------------------------------
	// THREE FLAGS, BECAUSE THERE ARE THREE QUESTIONS (occupying/counter-attacks D15)
	//
	// Before the counter-attack siege there was one question with three answers, and it worked because
	// a battle became all three at once. A siege breaks that: it EXISTS for up to 31 minutes before it
	// is FOUGHT, and it is fought in secret for up to a minute before anyone is TOLD.
	//
	//   m_bQRFActive   - "may a new battle start? may this player capture, or rise up?"
	//                    Set for the whole siege, from the moment the first truck rolls. UNCHANGED.
	//   m_bQRFRevealed - "does the client know?"  HUD panel, map circle, fast travel, respawn.
	//   IsQRFEngaged() - "is the shooting on?"    Economy tick, deployments, the town's civilians.
	//
	// ⚠ EVERY ONE OF THEM IS SET SO THAT A **STANDARD** BATTLE BEHAVES EXACTLY AS IT DOES TODAY:
	// revealed at creation, engaged at creation. A player-initiated battle must be incapable of taking
	// a new code path.
	//------------------------------------------------------------------------------------------------

	//! Whether the resistance has been told about the current battle. TRUE FROM CREATION for a standard
	//! battle; true at the MUSTER transition for a counter-attack siege (see RevealQRF).
	//!
	//! Replicated the same way m_bQRFActive is - broadcast RPC on change, plus the JIP payload.
	bool m_bQRFRevealed = false;

	vector m_vQRFLocation = "0 0 0";
	int m_iCurrentQRFBase = -1;
	int m_iCurrentQRFTown = -1;
	int m_iQRFPoints = 0;
	int m_iQRFTimer = 0;

	//------------------------------------------------------------------------------------------------
	// TICK LATCHES (2026-08-19 review fix)
	//
	// WHAT THEY ARE FOR. CheckUpdate's two payload gates are minute-exact - "hour is 0/6/12/18 AND
	// minute is 0" for the resource gain, "minute is 0/15/30/45" for the threat decay - and until now
	// they had NO latch at all, which is the one thing OVT_EconomyManagerComponent's equivalent gates
	// have always had (m_iHourPaidIncome / m_iHourPaidStock / m_iHourPaidRent). Without one, ANY second
	// tick landing inside the same in-game minute runs the payload again, and after a sleep time skip
	// the resumed tick re-runs the boundary the replay has just paid for (implementation.md Q1/F7).
	//
	// THEY MIRROR m_iHourPaidIncome EXACTLY, including its lack of an else-branch reset: consecutive
	// boundaries are always DIFFERENT numbers (0 -> 6 -> 12 -> 18, and 0 -> 15 -> 30 -> 45), so a latch
	// holding the last boundary can never suppress the next one, and any value that is not a boundary
	// at all - which is what HandleTimeSkip leaves behind when a skip lands off the grid - is simply
	// inert.
	//
	// THEY ARE INITIALISED TO -1, THE ARMED STATE. Every boundary value is >= 0, so the first boundary
	// a fresh or freshly loaded campaign reaches always fires. Normal play is therefore provably
	// unchanged: the gate is only reachable at an exact boundary minute, and at each such minute the
	// latch cannot already hold that value unless the payload has already run inside that very minute.
	//
	// THIS APPLIES ON DEDICATED SERVERS TOO, not only to the single-player sleep path, because the
	// latch lives on the LIVE gate. That is deliberate and is a strict de-duplication: it can only ever
	// remove a repeat of a payload that has already run in the same in-game minute.
	//
	// NOT PERSISTED, on purpose. Same reasoning as BUG-183's fix on the economy manager: a latch
	// restored from a save would be about a clock that no longer applies. The armed -1 costs at most
	// one extra gain in the minute a campaign happens to load in, which is the pre-existing behaviour.
	//------------------------------------------------------------------------------------------------

	//! Hour of day the six-hour resource gain last ran in, or -1 when it has not run yet.
	protected int m_iHourGainedResources = -1;

	//! Minute of hour the quarter-hourly threat decay last ran in, or -1 when it has not run yet.
	protected int m_iMinuteDecayedThreat = -1;

	//! Hour of day the hourly defense-share drip last ran in, or -1 when it has not run yet. Same
	//! latch contract, and NOT PERSISTED for the same reason as the two above.
	protected int m_iHourDripped = -1;

	//------------------------------------------------------------------------------------------------
	// THE DEFENSE-SHARE DRIP (2026-08-22)
	//
	// WHAT CHANGED AND WHAT DELIBERATELY DID NOT. Income is untouched: the reserve is still paid in a
	// lump on the four six-hour boundaries, still latched by m_iHourGainedResources, and the sleep
	// replay still walks the same grid. What moved is the TRANSFER. The share no longer leaves for the
	// deployment pool all at once - it is ARMED as a debt the reserve owes the pool, and paid off one
	// slice an hour.
	//
	// WHY THE TRANSFER AND NOT THE INCOME. resistance/sleep replays income through the very same
	// methods the live tick uses, walking a skipped window in quarter-hour steps; change the income
	// cadence and the replay's granularity has to change with it, which is BUG-183's family (an
	// unpersisted hour latch paying a sweep twice on load is a repeatable money exploit). The pool is
	// the CONTENDED resource - nothing spends directly out of the reserve on defense - so smoothing
	// its arrival lands exactly where the burst behaviour is, and the half of the system carrying the
	// save-format and time-skip hazards does not move at all.
	//
	// THE MONEY STAYS IN THE RESERVE WHILE IT IS OWED (author decision 2026-08-22, over an escrow
	// bucket). One consequence is deliberate and is not a bug: the reserve now sits ~80 % fatter for
	// most of each six-hour window, and the reserve is what OVT_ObjectiveDirectorComponent reads for
	// objectiveQRFResourceGate - so a counter-attack clears its funding gate EARLIER than it did.
	//
	// NOTHING CAN BE STRANDED. ArmDefenseShareDrip() flushes whatever the previous window still owed
	// before arming the new one, so a window that lost drips to a QRF freeze, a missed tick or a load
	// pays in full at the next payday rather than silently evaporating.
	//------------------------------------------------------------------------------------------------

	//! What the deployment pool is still owed from the current window, still sitting in m_iResources.
	protected int m_iPendingDefenseTransfer = 0;

	//! How many drips are left to pay it off with, including the next one.
	protected int m_iDefenseDripsRemaining = 0;

	//! Minute of hour the next drip fires at - rolled fresh after every drip so the six transfers do
	//! not land on the exact hour in lockstep.
	protected int m_iDripMinute = 0;

	const int OF_UPDATE_FREQUENCY = 60000;
	const int RADIO_TOWER_CHECK_FREQUENCY = 9000;

	//! Registered name of the hunter-killer's deployment config (occupying/vehicles T6.4). Read the same
	//! way OVT_QRFControllerComponent.ECHELON_CONFIG_NAME is - a caller resolves it once, through
	//! FindConfigByName, rather than restating the module's own attributes as a second set of constants.
	static const string HUNTER_KILLER_CONFIG_NAME = "Hunter Killer Sweep";

	//! The score floor below which a hotspot is not worth a bounded armoured sweep. Under
	//! THREAT_WEIGHT_ENEMY_CAMP (below) - which is the weight a single reported vehicle loss is inserted
	//! at (T6.1/T6.6) - so a fresh loss clears it on its own at its own exact position, while an empty
	//! spot with nothing known nearby (score 0) does not.
	static const int HUNTER_KILLER_THREAT_FLOOR = 15;

	//! Town support percentage required before players can start an uprising at the town flag
	static const int UPRISING_SUPPORT_THRESHOLD = 75;

	ref ScriptInvoker<IEntity> m_OnAIKilled = new ScriptInvoker<IEntity>;
	ref ScriptInvoker<OVT_BaseControllerComponent> m_OnBaseControlChanged = new ScriptInvoker<OVT_BaseControllerComponent>;
	ref ScriptInvoker<IEntity> m_OnPlayerLoot = new ScriptInvoker<IEntity>;

	//! Which TOWN is currently under QRF attack, published the moment it changes: the town's id when a
	//! town QRF starts, -1 when it finishes.
	//!
	//! Exists so town-local systems (the ambient civilian crowd) can suppress themselves for ONE town
	//! instead of polling m_iCurrentQRFTown, and so the old "any QRF anywhere despawns every town's
	//! civilians" shortcut never has to come back.
	//!
	//! ⚠ BASE QRFs DELIBERATELY DO NOT PUBLISH HERE (decision D6). A base QRF happens at a base, not in
	//! a town, and has no town id to name; m_vQRFLocation is what a future distance-based consumer
	//! would use. Server-side only - m_bQRFActive / m_iCurrentQRFTown remain the replicated truth and
	//! are unchanged by this invoker.
	ref ScriptInvoker<int> m_OnQRFTownChanged = new ScriptInvoker<int>;

	//! A battle has started, ANYWHERE - base or town, STANDARD or COUNTER_ATTACK - published once,
	//! at m_vQRFLocation, from the tail of StartBaseQRF and the tail of StartTownQRF, after the mode is
	//! configured and Start() has run (occupying/vehicles T5.1).
	//!
	//! ⚠ WHY THIS IS NOT m_OnQRFTownChanged (C9). That invoker is deliberately narrower on TWO axes: it
	//! is TOWN-ONLY (a base QRF never touches it - D6 of counter-attacks) and it is STANDARD-ONLY (a
	//! siege stays silent on it until BATTLE, so the resistance is not tipped off by the same signal
	//! that would arm a garrison). m_OnBattleStarted answers a different question - "is there a fight
	//! near THIS position a mounted force should react to" - which a garrison sitting on a base's own
	//! parked armour needs answered for base battles too, and needs answered the instant the QRF is
	//! stood up rather than delayed until a siege reveals itself. Firing it for COUNTER_ATTACK as well
	//! is deliberate: a garrison crewing up in secret is exactly what a silent siege should provoke.
	//!
	//! Server-side only, like every QRF invoker in this file - a client's m_CurrentQRF is never set, so
	//! StartBaseQRF/StartTownQRF never run there and this never fires there either.
	ref ScriptInvoker<vector> m_OnBattleStarted = new ScriptInvoker<vector>;

	static OVT_OccupyingFactionManager s_Instance;

	static OVT_OccupyingFactionManager GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_OccupyingFactionManager.Cast(pGameMode.FindComponent(OVT_OccupyingFactionManager));
		}

		return s_Instance;
	}

	void Init(IEntity owner)
	{
		if(!Replication.IsServer()) 
		{
			// On clients, set up base faction affiliations after JIP data is loaded
			GetGame().GetCallqueue().CallLater(SetClientBaseFactions, 1000);
			return;
		}

		Faction playerFaction = GetGame().GetFactionManager().GetFactionByKey(m_Config.m_sPlayerFaction);
		m_iPlayerFactionIndex = GetGame().GetFactionManager().GetFactionIndex(playerFaction);

		Faction occupyingFaction = GetGame().GetFactionManager().GetFactionByKey(m_Config.m_sOccupyingFaction);
		m_iOccupyingFactionIndex = GetGame().GetFactionManager().GetFactionIndex(occupyingFaction);

		OVT_Global.GetTowns().m_OnTownControlChange.Insert(OnTownControlChanged);

		InitializeBases();
	}
	
	void SetClientBaseFactions()
	{
		if (Replication.IsServer()) return;
		
		// Iterate through all bases and set their faction affiliations on the client
		foreach (OVT_BaseData base : m_Bases)
		{
			BaseWorld world = GetOwner().GetWorld();
			world.QueryEntitiesBySphere(base.location, 5, CheckBaseAndSetFaction, null, EQueryEntitiesFlags.STATIC);
		}
	}
	
	bool CheckBaseAndSetFaction(IEntity entity)
	{
		OVT_BaseControllerComponent baseController = OVT_BaseControllerComponent.Cast(entity.FindComponent(OVT_BaseControllerComponent));
		if (!baseController) return true;
		
		//Initialize the base controller for the client
		baseController.InitBaseClient();		
		
		// Find the base data for this location
		OVT_BaseData baseData = GetNearestBase(entity.GetOrigin());
		if (!baseData) return true;

		// entId is [NonSerialized] and otherwise only assigned by the server's world query -
		// without this a client record never resolves its entity (BUG-176: no base name on the map)
		baseData.entId = entity.GetID();

		// Set the faction affiliation
		SCR_FactionAffiliationComponent affiliation = OVT_ComponentFinder<SCR_FactionAffiliationComponent>.Find(entity);
		if (affiliation)
		{
			FactionManager factionManager = GetGame().GetFactionManager();
			Faction faction = factionManager.GetFactionByIndex(baseData.faction);
			if (faction)
			{
				affiliation.SetAffiliatedFaction(faction);
			}
		}
		
		return true;
	}

	void NewGameStart()
	{
		OVT_Global.GetConfig().m_iOccupyingFactionIndex = -1;
		m_iThreat = m_Config.m_Difficulty.baseThreat;
		m_iResources = m_Config.m_Difficulty.maxQRF;

		int factionIndex = OVT_Global.GetConfig().GetOccupyingFactionIndex();

		Print(string.Format("[Overthrow] NewGameStart: Setting %1 bases to occupying faction index %2", m_Bases.Count(), factionIndex));

		// Set all bases to occupying faction
		foreach(OVT_BaseData data : m_Bases)
		{
			data.faction = factionIndex;
		}

		// Set all radio towers to occupying faction
		foreach(OVT_RadioTowerData tower : m_RadioTowers)
		{
			tower.faction = factionIndex;
		}
		Print(string.Format("[Overthrow] NewGameStart: Set %1 radio towers to occupying faction", m_RadioTowers.Count()));

		// Set all towns to occupying faction
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		if(townManager)
		{
			foreach(OVT_TownData town : townManager.m_Towns)
			{
				town.faction = factionIndex;
			}
			Print(string.Format("[Overthrow] NewGameStart: Set %1 towns to occupying faction", townManager.m_Towns.Count()));
		}

		// THE OCCUPYING FACTION'S OPENING DEFENSE BUDGET - ONE CREDIT, AND THE ONLY ONE.
		//
		// It replaces TWO opening paths. The free baseResourcesPerTick that used to be credited here,
		// and a +5 s per-base distribution that handed every base
		// (startingResources * m_fStartingResourcesMultiplier) and had it SPEND that immediately through
		// the base controller's legacy spender - money that never passed through m_iResources, never
		// appeared in any pool, and was therefore invisible to every accounting the campaign had. Both
		// are now one number in the deployment pool, which the evaluator spends on the nine
		// Deployment_Base*.conf configs, concern by concern, as threat and cost allow.
		//
		// IT RUNS HERE AND NOT ON A TIMER. The deleted distribution was deferred 5 s because SPENDING
		// needed each base controller's discovered slots (InitBase() runs out of PostGameStart). A
		// CREDIT needs only m_fStartingResourcesMultiplier, which is an [Attribute] readable the instant
		// the controller entity exists - and InitializeBases() has already found every one of them
		// during Init(), which is the same reason the loops above can walk m_Bases.
		if(m_bDistributeInitial)
		{
			SeedOpeningDeploymentResources();

			// A campaign gets its opening allocation exactly once. ApplyPersistedOccupyingFaction()
			// clears the same flag when a save is restored, so a continued campaign never re-seeds -
			// which is what this flag has always meant.
			m_bDistributeInitial = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Credits the occupying faction's deployment pool with its opening defense budget.
	//!
	//! PUBLIC, AND DELIBERATELY SEPARATE FROM THE NUMBER IT CREDITS. The phase's headline claim - that
	//! the opening seed lands in the DEPLOYMENT POOL and not in m_iResources - is only assertable as a
	//! live claim by driving this, and only checkable against CalculateOpeningDeploymentSeed() because
	//! the two are apart. Nothing but NewGameStart() calls it in production.
	//!
	//! NOT GUARDED HERE. The "once per campaign" rule is m_bDistributeInitial's, and its owner is
	//! NewGameStart(), which is the only path that can run on a campaign that has not been restored.
	void SeedOpeningDeploymentResources()
	{
		int seed = CalculateOpeningDeploymentSeed();
		AllocateDeploymentResources(seed);

		Print(string.Format("[Overthrow.OccupyingFactionManager] Opening defense budget: %1 resources into the deployment pool across %2 base(s)",
			seed.ToString(), m_Bases.Count().ToString()));
	}

	//------------------------------------------------------------------------------------------------
	//! The opening defense budget: one tick of base income plus every base's authored opening
	//! allocation.
	//!
	//! READ-ONLY - it computes, it never credits. Both halves are the legacy numbers unchanged:
	//! baseResourcesPerTick is what NewGameStart() used to credit for free, and
	//! (startingResources * m_fStartingResourcesMultiplier) per base is exactly what the deleted
	//! per-base distribution used to conjure and spend. What changes is where the money goes, not how
	//! much of it there is.
	//! \return The seed in deployment-pool resources. Never negative.
	int CalculateOpeningDeploymentSeed()
	{
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if(!difficulty) return 0;

		int seed = difficulty.baseResourcesPerTick;

		foreach(OVT_BaseData data : m_Bases)
		{
			if(!data) continue;

			OVT_BaseControllerComponent base = GetBase(data.entId);
			if(!base) continue;

			seed += Math.Floor(difficulty.startingResources * base.m_fStartingResourcesMultiplier);
		}

		if(seed < 0) seed = 0;

		return seed;
	}

	void PostGameStart()
	{
		float timeMul = 6;
		OVT_TimeAndWeatherHandlerComponent tw = OVT_TimeAndWeatherHandlerComponent.Cast(GetGame().GetGameMode().FindComponent(OVT_TimeAndWeatherHandlerComponent));

		if(tw) timeMul = tw.GetDayTimeMultiplier();
		
		UpdateKnownTargets();

		GetGame().GetCallqueue().CallLater(InitBaseControllers, 0);

		GetGame().GetCallqueue().CallLater(CheckUpdate, OF_UPDATE_FREQUENCY / timeMul, true, GetOwner());

		GetGame().GetCallqueue().CallLater(CheckRadioTowers, RADIO_TOWER_CHECK_FREQUENCY, true, GetOwner());

		// THE +5 s INITIAL-RESOURCE DISTRIBUTION THAT STOOD HERE IS GONE AND MUST NOT COME BACK.
		// The occupying faction's opening defense budget is now ONE credit to the deployment pool, made
		// by NewGameStart() - see SeedOpeningDeploymentResources(). Scheduling anything here would run
		// on a CONTINUED campaign too, which is precisely the double-build m_bDistributeInitial existed
		// to prevent.

		// THE LEGACY REFUND'S DELIVERY POINT ON THE LOAD PATH, and the reason it is here rather than in
		// the deserialization that computed it: the deployment manager's restore CLEARS the resource
		// pool, and it runs after this manager's inside the same load. This runs after the whole load.
		// A no-op on any campaign that was not loaded from a pre-migration save.
		CreditPendingLegacyRefund();
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the persisted occupying-faction war state.
	//!
	//! Called from OVT_OccupyingFactionManagerSerializer.Deserialize().
	//!
	//! SUPPRESSES THE OPENING RESOURCE ALLOCATION. m_bDistributeInitial is cleared because a saved
	//! campaign has already had its opening allocation; the flag is what NewGameStart() checks before
	//! seeding the deployment pool, and a restored campaign must never be seeded a second time on top
	//! of what it saved. EPF cleared the same flag for the same reason.
	//!
	//! QUEUES THE LEGACY BASE-UPGRADE REFUND, ONCE, AFTER EVERY BASE RECORD HAS BEEN READ. A save
	//! written before the base-defense migration carries per-base upgrade records; those upgrades no
	//! longer exist, so each base's record is converted to a VALUE (ApplyPersistedBaseUpgrades), the sum
	//! is accumulated across the whole loop, and it is handed to QueueLegacyUpgradeRefund() ONCE below.
	//! The evaluator re-establishes defense from it by threat - there is no per-base re-establishment
	//! code anywhere, deliberately. The sum is accumulated to after the loop because it is one
	//! campaign-level refund, not eleven, and because the occupying faction key it is credited to is
	//! only settled at the top of this method.
	//!
	//! 🔴 IT IS QUEUED, NOT CREDITED. The deployment manager's own restore CLEARS the per-faction
	//! resource pool and runs after this one in the same load - read QueueLegacyUpgradeRefund()'s header
	//! before moving the credit back inline.
	//!
	//! SPAWNS NOTHING. Filled slots are data; InitBaseControllers() replays them during PostGameStart,
	//! which is the same path a fresh campaign takes.
	//!
	//! NO RPC. Clients receive base and tower control through the normal replication paths.
	//!
	//! IDEMPOTENT: assignments and clear-and-rebuilds only, safe to run again on a live session. The
	//! refund is idempotent STRUCTURALLY rather than by a flag - see ApplyPersistedBaseUpgrades().
	//! \param[in] occupyingFactionKey Faction key the campaign was started against, may be empty.
	//! \param[in] resources Occupying faction resource pool.
	//! \param[in] threat Occupying faction threat level.
	//! \param[in] bases Persisted base records, matched to live bases by location. May be null.
	//! \param[in] towers Persisted radio tower records, matched by location. May be null.
	//------------------------------------------------------------------------------------------------
	//! Restores the defense-share drip's outstanding debt from a save.
	//!
	//! SEPARATE FROM ApplyPersistedOccupyingFaction() ON PURPOSE - that method's five arguments are the
	//! war state a campaign cannot start without, and the Persistence tier drives it directly; the drip
	//! is a schedule laid over it and a version 1-3 save legitimately has none.
	//!
	//! DEFENSIVE, BECAUSE A RESTORED DEBT IS SPENDABLE MONEY. A corrupt or hand-edited payload claiming
	//! a debt with no drips left to pay it would be settled in full by the next flush, so the two are
	//! reconciled here instead: no debt means no drips, and a debt with no schedule gets a full one.
	//! \param[in] pendingDefenseTransfer What the pool was still owed when the save was written.
	//! \param[in] defenseDripsRemaining How many drips were left to pay it.
	//! \param[in] dripMinute The minute of the hour the next drip was due at.
	void ApplyPersistedDefenseDrip(int pendingDefenseTransfer, int defenseDripsRemaining, int dripMinute)
	{
		if (pendingDefenseTransfer < 0)
			pendingDefenseTransfer = 0;

		m_iPendingDefenseTransfer = pendingDefenseTransfer;

		if (m_iPendingDefenseTransfer == 0)
			m_iDefenseDripsRemaining = 0;
		else if (defenseDripsRemaining <= 0 || defenseDripsRemaining > OVT_BaseDefenseConversion.DRIP_STEPS)
			m_iDefenseDripsRemaining = OVT_BaseDefenseConversion.DRIP_STEPS;
		else
			m_iDefenseDripsRemaining = defenseDripsRemaining;

		if (dripMinute < 0 || dripMinute >= OVT_BaseDefenseConversion.DRIP_MINUTE_SPREAD)
			RollDripMinute();
		else
			m_iDripMinute = dripMinute;
	}

	//------------------------------------------------------------------------------------------------
	//! What the deployment pool is still owed from the current drip window. Read-only, for tests and
	//! the GM snapshot.
	int GetPendingDefenseTransfer()
	{
		return m_iPendingDefenseTransfer;
	}

	//------------------------------------------------------------------------------------------------
	//! How many drips are left to pay that debt off with. Read-only.
	int GetDefenseDripsRemaining()
	{
		return m_iDefenseDripsRemaining;
	}

	//------------------------------------------------------------------------------------------------
	//! The minute of the hour the next drip is due at. Read-only.
	int GetDripMinute()
	{
		return m_iDripMinute;
	}

	void ApplyPersistedOccupyingFaction(string occupyingFactionKey, int resources, float threat, array<ref OVT_PersistedBase> bases, array<ref OVT_PersistedRadioTower> towers)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		// Applied FIRST: every faction index below is relative to this choice.
		if (config && occupyingFactionKey != "" && config.m_sOccupyingFaction != occupyingFactionKey)
			config.SetOccupyingFaction(occupyingFactionKey);

		m_iResources = resources;
		m_iThreat = threat;
		m_bDistributeInitial = false;

		int legacyUpgradeValue = 0;
		int baseRecordCount = 0;

		array<ref OVT_BaseData> restoredBases = new array<ref OVT_BaseData>();
		if (bases)
		{
			foreach (OVT_PersistedBase baseRecord : bases)
			{
				if (!baseRecord)
					continue;

				OVT_BaseData base = GetNearestBase(baseRecord.location);
				if (!base)
					continue;

				int faction = baseRecord.faction;
				if (faction < 0)
				{
					Print(string.Format("[Overthrow] Saved base at %1 has no faction - handing it to the occupying faction", baseRecord.location.ToString()), LogLevel.WARNING);
					if (config)
						faction = config.GetOccupyingFactionIndex();
				}

				base.faction = faction;

				legacyUpgradeValue += ApplyPersistedBaseUpgrades(base, baseRecord);
				baseRecordCount += 1;

				ApplyPersistedBaseSlots(base, baseRecord);

				restoredBases.Insert(base);
			}
		}

		// ONE refund, after every base record has been read. A post-migration payload sums to zero and
		// nothing is queued at all - which is the whole of the idempotence argument, and why there is no
		// flag here.
		if (legacyUpgradeValue > 0)
		{
			Print(string.Format("[Overthrow] Legacy base upgrades converted: %1 resources owed to the occupying faction's deployment pool from %2 base record(s)",
				legacyUpgradeValue.ToString(), baseRecordCount.ToString()));

			QueueLegacyUpgradeRefund(legacyUpgradeValue);
		}

		// A base with no save record was added to the map after the save was written. NewGameStart
		// never runs on a continue, and discovery stamped its faction before the campaign's real
		// occupying faction key was applied above - so it is handed to the occupying faction here,
		// or it would never garrison and the deployment evaluator would never fortify it.
		if (config)
		{
			int occupyingBaseFaction = config.GetOccupyingFactionIndex();
			foreach (OVT_BaseData base : m_Bases)
			{
				if (!base || restoredBases.Contains(base))
					continue;

				Print(string.Format("[Overthrow] Base at %1 is not in the save - handing it to the occupying faction", base.location.ToString()));
				base.faction = occupyingBaseFaction;
			}
		}

		array<ref OVT_RadioTowerData> restoredTowers = new array<ref OVT_RadioTowerData>();
		if (towers)
		{
			foreach (OVT_PersistedRadioTower towerRecord : towers)
			{
				if (!towerRecord)
					continue;

				OVT_RadioTowerData tower = GetNearestRadioTower(towerRecord.location);
				if (!tower)
					continue;

				int towerFaction = towerRecord.faction;
				if (towerFaction < 0)
				{
					Print(string.Format("[Overthrow] Saved radio tower at %1 has no faction - handing it to the occupying faction", towerRecord.location.ToString()), LogLevel.WARNING);
					if (config)
						towerFaction = config.GetOccupyingFactionIndex();
				}

				tower.faction = towerFaction;

				// A tower that was sabotaged when the game was saved comes back still off the air,
				// with the time it had left. Version 1 payloads carry 0 here, which restores the
				// pre-sabotage behaviour of every tower being up on load.
				tower.SetDisabledRemaining(towerRecord.disabledRemaining);

				restoredTowers.Insert(tower);
			}
		}

		// A tower with no save record was added to the map after the save was written. NewGameStart
		// never runs on a continue, and discovery stamped its faction before the campaign's real
		// occupying faction key was applied above - so it is handed to the occupying faction here.
		// THE REASON IS NOT WHAT IT ORIGINALLY WAS, and the behaviour is still required. Nothing on
		// this manager garrisons a tower any more; a tower is garrisoned by Deployment_TowerGarrison
		// .conf, whose base-control condition module only offers a tower the deployment evaluator can
		// see as OCCUPIED. A tower left on a stale faction index is therefore invisible to it forever.
		if (config)
		{
			int occupyingFactionIndex = config.GetOccupyingFactionIndex();
			foreach (OVT_RadioTowerData tower : m_RadioTowers)
			{
				if (!tower || restoredTowers.Contains(tower))
					continue;

				Print(string.Format("[Overthrow] Radio tower at %1 is not in the save - handing it to the occupying faction", tower.location.ToString()));
				tower.faction = occupyingFactionIndex;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Queues resources a legacy save conversion owes the occupying faction's deployment pool.
	//!
	//! 🔴 THE REFUND IS DEFERRED, AND IT HAS TO BE. It cannot be credited from inside
	//! ApplyPersistedOccupyingFaction(), because the deployment manager's own restore runs LATER IN THE
	//! SAME LOAD and its first act is to CLEAR the per-faction resource map before refilling it from the
	//! save (OVT_DeploymentManagerComponent.ApplyPersistedFactionResources). The serializer order is
	//! authored in Configs/Systems/Persistence/Overthrow.conf, where this manager sits several entries
	//! ABOVE the deployment manager - so an inline credit would be wiped microseconds later, silently,
	//! and every legacy campaign would load with its whole investment gone.
	//!
	//! TWO DELIVERY POINTS, AND WHICH ONE IS ARMED DEPENDS ON WHETHER A CAMPAIGN IS ALREADY RUNNING:
	//!   - LOADING A SAVE POINT (campaign not started yet): the refund waits for PostGameStart(), which
	//!     the game mode schedules only after the whole load has been applied. Provably after the
	//!     deployment restore, with no assumption about how many frames a load takes.
	//!   - RE-APPLYING TO A LIVE SESSION (campaign already running, so PostGameStart will never run
	//!     again): a next-frame callback, which is after the one synchronous re-application that
	//!     triggered it.
	//! \param[in] value Resources owed. Accumulated, never overwritten.
	protected void QueueLegacyUpgradeRefund(int value)
	{
		if (value <= 0)
			return;

		bool alreadyQueued = m_iPendingLegacyRefund > 0;
		m_iPendingLegacyRefund += value;

		if (alreadyQueued)
			return;

		OVT_OverthrowGameMode mode = OVT_Global.GetOverthrow();
		if (mode && mode.HasGameStarted())
			GetGame().GetCallqueue().CallLater(CreditPendingLegacyRefund, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Hands whatever a legacy save conversion owed to the deployment pool, once.
	//!
	//! IDEMPOTENT AND SELF-CLEARING: the pending amount is zeroed before the credit, so whichever of the
	//! two delivery points above reaches it first wins and the other becomes a no-op. That is what makes
	//! it safe to arm both.
	//!
	//! PUBLIC so the persistence-tier case can drive it in the frame it made the claim in, rather than
	//! asserting against a callback it would have to wait for.
	void CreditPendingLegacyRefund()
	{
		if (m_iPendingLegacyRefund <= 0)
			return;

		int value = m_iPendingLegacyRefund;
		m_iPendingLegacyRefund = 0;

		AllocateDeploymentResources(value);

		Print(string.Format("[Overthrow] Legacy base-upgrade refund credited: %1 resources into the occupying faction's deployment pool", value.ToString()));
	}

	//------------------------------------------------------------------------------------------------
	//! Resources a legacy save conversion owes the deployment pool but has not handed over yet.
	//! \return The pending amount, 0 when nothing is owed.
	int GetPendingLegacyRefund()
	{
		return m_iPendingLegacyRefund;
	}

	//------------------------------------------------------------------------------------------------
	//! CONVERTS one base's LEGACY upgrade records into a resource value, and leaves the base's upgrade
	//! list EMPTY.
	//!
	//! THIS USED TO COPY THE RECORDS ONTO THE BASE so InitBaseControllers() could replay them into the
	//! live upgrade objects a base controller ran. Those objects no longer exist: base defense is nine
	//! Deployment_Base*.conf configs the deployment evaluator establishes and the virtualization core
	//! owns. So a pre-migration payload is read for its VALUE and nothing else - value-parity, not
	//! entity-identity - and the caller credits the sum to the deployment pool once.
	//!
	//! THE PAYLOAD CLASSES STAY. OVT_PersistedBaseUpgrade / OVT_PersistedBaseUpgradeGroup are still
	//! declared and still READ here, because OVT_PersistedBase.upgrades sits at field 4 of 5 and a
	//! binary save context is positional: removing it would shift `garrison` and break every existing
	//! save. What changed is that the write path now stores an empty array in it.
	//!
	//! WHAT CONVERTS TO ZERO, AND WHY, is documented on OVT_BaseDefenseConversion.ConvertedValue().
	//! Two of them matter here: a composition/checkpoint record refunds nothing because the structure
	//! itself is a tracked world entity that comes back on its own (and its slot claim comes back in
	//! ApplyPersistedBaseSlots), and an ALREADY-CONVERTED payload refunds nothing because it is empty.
	//!
	//! IDEMPOTENCE IS STRUCTURAL, NOT GUARDED. After the first load the write path stores an empty
	//! upgrade array, so every subsequent save/load of that campaign converts zero. The one exposure
	//! this leaves, stated so nobody mistakes it for a bug: re-applying the SAME pre-migration save
	//! twice in one session (OVT_PersistenceManagerComponent.ReapplyLatestSaveData) credits twice,
	//! because nothing has rewritten the payload yet. Taking a single save after loading a legacy
	//! campaign closes it permanently.
	//!
	//! A NON-OCCUPYING BASE NEEDS NO FILTER: the legacy write path only ever populated `upgrades` for a
	//! base the occupying faction held, so a resistance-held record converts to zero on its own.
	//! \param[in] base The live base data.
	//! \param[in] record The saved record.
	//! \return The value this base's legacy investment converts to, in deployment-pool resources.
	//!         0 for a post-migration record.
	protected int ApplyPersistedBaseUpgrades(notnull OVT_BaseData base, notnull OVT_PersistedBase record)
	{
		if (!base.upgrades)
			base.upgrades = new array<ref OVT_BaseUpgradeData>();

		// Left EMPTY on every path. Nothing replays these any more, and a populated list would be a
		// standing invitation for something to try.
		base.upgrades.Clear();

		if (!record.upgrades)
			return 0;

		array<int> bankedResources = {};
		array<int> groupCounts = {};

		foreach (OVT_PersistedBaseUpgrade upgradeRecord : record.upgrades)
		{
			if (!upgradeRecord)
				continue;

			bankedResources.Insert(upgradeRecord.resources);

			int groups = 0;
			if (upgradeRecord.groups)
				groups = upgradeRecord.groups.Count();

			groupCounts.Insert(groups);
		}

		return OVT_BaseDefenseConversion.ConvertedValue(bankedResources, groupCounts, GetLegacyGroupValue());
	}

	//------------------------------------------------------------------------------------------------
	//! What one group recorded in a legacy base-upgrade payload is worth today.
	//!
	//! LEGACY_GROUP_VALUE = OVT_BaseDefenseConversion.LEGACY_GROUP_SIZE * difficulty.baseResourceCost,
	//! which is the same per-man price the deleted valuation used (a live group was worth
	//! agents * baseResourceCost). It is an APPROXIMATION BY DESIGN - value-parity, not
	//! entity-identity - and the constant carries that sentence.
	//!
	//! NULL-SAFE ON DIFFICULTY: deserialization can land before the campaign's difficulty settings are
	//! resolved, and a refund of 0 is a far better failure than a VME during a load.
	//! \return The per-group refund value, never negative.
	protected int GetLegacyGroupValue()
	{
		int baseResourceCost = 0;

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (difficulty)
			baseResourceCost = difficulty.baseResourceCost;

		return OVT_BaseDefenseConversion.LegacyGroupValue(baseResourceCost);
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds one base's filled-slot list from its save record.
	//! \param[in] base The live base data.
	//! \param[in] record The saved record.
	protected void ApplyPersistedBaseSlots(notnull OVT_BaseData base, notnull OVT_PersistedBase record)
	{
		if (!base.slotsFilled)
			base.slotsFilled = new array<vector>();

		base.slotsFilled.Clear();

		if (!record.slotsFilled)
			return;

		foreach (vector slot : record.slotsFilled)
		{
			base.slotsFilled.Insert(slot);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ticks every radio tower's sabotage countdown on the 9 s timer installed by PostGameStart, and
	//! puts a tower back on the air when its downtime runs out.
	//!
	//! GARRISONS ARE NOT HERE ANY MORE, AND MUST NOT COME BACK. This loop used to fuse four unrelated
	//! concerns into one foreach: the sabotage countdown below, spawning a tower's defence groups,
	//! deleting them again, and capturing the tower when the list of them happened to be empty. All
	//! three garrison halves are gone - a radio tower garrison is now an ordinary DEPLOYMENT
	//! (Configs/Deployment/Deployment_TowerGarrison.conf), so:
	//!   - the groups are registered with the virtualization core and the ENGINE decides when to put
	//!     men on the ground, remembering which of them died across every despawn and every save;
	//!   - capture is driven by that deployment's eliminated flag, which is set only by a real wipe,
	//!     so driving away from a tower can no longer take it;
	//!   - the map-wide "delete every tower garrison for the whole duration of any QRF anywhere" side
	//!     effect went with the despawn branch that carried it.
	//!
	//! What is left is the one concern this method was ever named for. Re-adding a spawn here would
	//! double every garrison, because the deployment config is already producing one.
	void CheckRadioTowers()
	{
		foreach(OVT_RadioTowerData tower : m_RadioTowers)
		{
			if(tower.disabledRemaining > 0)
			{
				tower.disabledRemaining -= RADIO_TOWER_CHECK_FREQUENCY / 1000;
				if(tower.disabledRemaining <= 0)
				{
					tower.disabledRemaining = 0;
					Rpc(RpcDo_SetRadioTowerDisabled, tower.location, 0);
					string townName = OVT_Global.GetTowns().GetTownName(tower.location);
					OVT_Global.GetNotify().SendTextNotification("RadioTowerRepaired", -1, townName);
				}
			}
		}
	}

	void ChangeRadioTowerControl(OVT_RadioTowerData tower, int faction)
	{
		if(faction == tower.faction) return;
		tower.faction = faction;
		Rpc(RpcDo_SetRadioTowerFaction, tower.location, faction);

		string townName = OVT_Global.GetTowns().GetTownName(tower.location);

		if(faction == OVT_Global.GetConfig().GetOccupyingFactionIndex())
		{
			OVT_Global.GetNotify().SendTextNotification("RadioTowerControlledOccupying",-1,townName);
			OVT_Global.GetNotify().SendExternalNotifications("RadioTowerControlledOccupying",townName);
		}else{
			OVT_Global.GetNotify().SendTextNotification("RadioTowerControlledResistance",-1,townName);
			OVT_Global.GetNotify().SendExternalNotifications("RadioTowerControlledResistance",townName);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: take a radio tower off the air for a duration (sabotage) and tell everyone.
	//! \param tower The radio tower to disable
	//! \param seconds How long the tower stays disabled
	void SetRadioTowerDisabled(OVT_RadioTowerData tower, float seconds)
	{
		tower.SetDisabledRemaining(seconds);
		Rpc(RpcDo_SetRadioTowerDisabled, tower.location, seconds);

		string townName = OVT_Global.GetTowns().GetTownName(tower.location);
		OVT_Global.GetNotify().SendTextNotification("RadioTowerSabotaged", -1, townName);
		OVT_Global.GetNotify().SendExternalNotifications("RadioTowerSabotaged", townName);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetRadioTowerDisabled(vector pos, float seconds)
	{
		OVT_RadioTowerData tower = GetNearestRadioTower(pos);
		if(tower) tower.SetDisabledRemaining(seconds);
	}

	void OnTownControlChanged(OVT_TownData town)
	{
		if(town.faction == m_iPlayerFactionIndex)
		{
			m_iThreat += town.size * 150;
		}
	}

	OVT_BaseData GetNearestBase(vector pos)
	{
		OVT_BaseData nearestBase;
		float nearest = -1;
		foreach(OVT_BaseData data : m_Bases)
		{
			float distance = vector.Distance(data.location, pos);
			if(nearest == -1 || distance < nearest){
				nearest = distance;
				nearestBase = data;
			}
		}
		if(!nearestBase) return null;
		return nearestBase;
	}

	OVT_RadioTowerData GetNearestRadioTower(vector pos)
	{
		OVT_RadioTowerData nearestBase;
		float nearest = -1;
		foreach(OVT_RadioTowerData data : m_RadioTowers)
		{
			float distance = vector.Distance(data.location, pos);
			if(nearest == -1 || distance < nearest){
				nearest = distance;
				nearestBase = data;
			}
		}
		if(!nearestBase) return null;
		return nearestBase;
	}

	OVT_BaseData GetNearestOccupiedBase(vector pos)
	{
		OVT_BaseData nearestBase;
		float nearest = -1;
		foreach(OVT_BaseData data : m_Bases)
		{
			if(!data.IsOccupyingFaction()) continue;
			float distance = vector.Distance(data.location, pos);
			if(nearest == -1 || distance < nearest){
				nearest = distance;
				nearestBase = data;
			}
		}
		if(!nearestBase) return null;
		return nearestBase;
	}

	void GetBasesWithinDistance(vector pos, float maxDistance, out array<OVT_BaseData> bases)
	{
		foreach(OVT_BaseData base : m_Bases)
		{
			float distance = vector.Distance(base.location, pos);
			if(distance < maxDistance){
				bases.Insert(base);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every base a given faction currently holds.
	//!
	//! A PURE READ - it allocates a list and copies references into it, and changes nothing. Added for
	//! the objective director, which enumerates resistance-held bases as candidates and occupying-held
	//! bases as supply sources, and which had no way to ask that question: GetNearestOccupiedBase()
	//! answers about ONE base and hard-codes the occupying faction.
	//!
	//! ⚠ THE RETURNED LIST BORROWS. The elements are the manager's own live records, not copies, so a
	//! caller reads through them and never holds them past the current frame.
	//! \param[in] factionIndex The faction to filter by.
	//! \return A new list, empty when the faction holds nothing. Never null.
	array<OVT_BaseData> GetBasesControlledBy(int factionIndex)
	{
		array<OVT_BaseData> controlled = new array<OVT_BaseData>();

		foreach(OVT_BaseData base : m_Bases)
		{
			if(!base) continue;
			if(base.faction != factionIndex) continue;

			controlled.Insert(base);
		}

		return controlled;
	}

	//------------------------------------------------------------------------------------------------
	//! Every radio tower whose broadcast still reaches a position, whoever holds it.
	//!
	//! A PURE READ. Two rules are baked in, and both are the rules the campaign already used:
	//!  - RANGE is OVT_InfluenceRules.IsProximitySource() against the difficulty's radioTowerRange, the
	//!    same predicate the town support tick has always used, so "in range" means one thing;
	//!  - A SABOTAGED TOWER IS NOT IN RANGE OF ANYTHING. A tower that is off the air broadcasts nothing
	//!    for either side, so it is skipped outright rather than returned for the caller to remember to
	//!    filter.
	//!
	//! THIS DE-DUPLICATES THE TOWN SUPPORT TICK'S OWN INLINE LOOP, which is now its first caller. The
	//! objective director is the second: an objective the occupying faction can still broadcast over is
	//! easier for it to hold, which is one of the selection inputs.
	//! \param[in] position The position to test coverage at.
	//! \return A new list of towers on the air within range. Empty, never null.
	array<OVT_RadioTowerData> GetRadioTowersAffecting(vector position)
	{
		array<OVT_RadioTowerData> affecting = new array<OVT_RadioTowerData>();

		float range = 0;
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if(difficulty) range = difficulty.radioTowerRange;
		if(range <= 0) return affecting;

		foreach(OVT_RadioTowerData tower : m_RadioTowers)
		{
			if(!tower) continue;
			if(tower.IsDisabled()) continue;
			if(!OVT_InfluenceRules.IsProximitySource(position, tower.location, range)) continue;

			affecting.Insert(tower);
		}

		return affecting;
	}

	void InitializeBases()
	{
		#ifdef OVERTHROW_DEBUG
		Print("Finding bases");
		#endif

		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, CheckBaseAdd, FilterBaseEntities, EQueryEntitiesFlags.STATIC);
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, CheckTransmitterTowerAdd, FilterTransmitterTowerEntities, EQueryEntitiesFlags.STATIC);
	}

	protected void InitBaseControllers()
	{
		Print(string.Format("[Overthrow] InitBaseControllers: Initializing %1 bases", m_Bases.Count()));

		foreach(int index, OVT_BaseData data : m_Bases)
		{
			OVT_BaseControllerComponent base = GetBase(data.entId);
			if(!base)
			{
				Print(string.Format("[Overthrow] WARNING: Could not find base controller for entity at %1", data.location.ToString()), LogLevel.WARNING);
				continue;
			}

			base.InitBase();
			base.SetControllingFaction(data.faction, true);
			base.UpdateFlagMaterial(data.faction);

			Print(string.Format("[Overthrow] Initialized base %1 at %2 with faction %3", index, data.location.ToString(), data.faction));

			if(base.IsOccupyingFaction())
			{
				// THE UPGRADE REPLAY THAT STOOD HERE IS GONE. There is nothing to replay into: a legacy
				// save's upgrade value was converted to a single deployment-pool credit during
				// ApplyPersistedBaseUpgrades(), and base defense is nine Deployment_Base*.conf configs the
				// evaluator re-establishes on its own.
				//
				// THE SLOT CLAIMS BELOW ARE KEPT VERBATIM AND ARE NOW LOAD-BEARING FOR A DIFFERENT
				// REASON. They are what stops OVT_CompositionSpawningDeploymentModule choosing a slot a
				// legacy composition is still standing in - the structure itself comes back from vanilla
				// persistence as an ordinary tracked entity, and this list is the only record that its
				// slot is taken.
				if(data.slotsFilled)
				{
					foreach(vector slotPos : data.slotsFilled)
					{
						IEntity slot = base.GetNearestSlot(slotPos);
						if(slot) base.m_aSlotsFilled.Insert(slot.GetID());
					}
				}
			}
		}

		Print(string.Format("[Overthrow] InitBaseControllers complete"));
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves a base's controller component from its marker entity id.
	//!
	//! NULL-SAFE ON THE MARKER. It used to dereference the result of FindEntityByID() directly and
	//! throw when a marker had gone away - which is why the occupying-faction serializer carries its own
	//! FindBaseController() and says so in its header. Every caller here already null-checks the
	//! RESULT; this is what makes those checks reachable instead of decorative.
	//! \param[in] id The base marker's entity id.
	//! \return The controller, or null.
	OVT_BaseControllerComponent GetBase(EntityID id)
	{
		IEntity marker = GetGame().GetWorld().FindEntityByID(id);
		if(!marker) return null;

		return OVT_BaseControllerComponent.Cast(marker.FindComponent(OVT_BaseControllerComponent));
	}

	OVT_BaseControllerComponent GetBaseByIndex(int index)
	{
		return GetBase(m_Bases[index].entId);
	}

	int GetBaseIndex(OVT_BaseData base)
	{
		return m_Bases.Find(base);
	}

	OVT_TargetData GetNearestKnownTarget(vector pos)
	{
		OVT_TargetData nearestTarget;
		float nearest = -1;
		foreach(OVT_TargetData data : m_aKnownTargets)
		{
			float distance = vector.Distance(data.location, pos);
			if(nearest == -1 || distance < nearest){
				nearest = distance;
				nearestTarget = data;
			}
		}
		if(!nearestTarget) return null;
		return nearestTarget;
	}

	void UpdateQRFTimer(int timer)
	{
		m_iQRFTimer = timer;
		Rpc(RpcDo_SetQRFTimer, timer);
	}

	void UpdateQRFPoints(int points)
	{
		m_iQRFPoints = points;
		Rpc(RpcDo_SetQRFPoints, points);
	}

	//------------------------------------------------------------------------------------------------
	//! Starts a battle for a base.
	//!
	//! ⚠ THE MODE IS CONFIGURED BEFORE Start(), following the SpawnQRFController -> configure -> Start()
	//! order the landing-zone fields already use. Start() takes no arguments on purpose (D14).
	//! \param[in] base The base being fought over.
	//! \param[in] mode STANDARD for a player-initiated battle - the default, and the only value any
	//! shipped caller but the objective director passes. COUNTER_ATTACK makes it a silent siege.
	void StartBaseQRF(OVT_BaseControllerComponent base, OVT_EQRFMode mode = OVT_EQRFMode.STANDARD)
	{
		if(m_CurrentQRF) return;

		OVT_BaseData data = GetNearestBase(base.GetOwner().GetOrigin());

		m_CurrentQRF = SpawnQRFController(base.GetOwner().GetOrigin());
		m_CurrentQRF.m_eMode = mode;
		m_CurrentQRF.m_iLZMin = base.m_iAttackDistanceMin;
		m_CurrentQRF.m_iLZMax = base.m_iAttackDistanceMax;
		m_CurrentQRF.m_iPreferredDirection = base.m_iAttackPreferredDirection;
		m_CurrentQRF.m_iDirectionVariance = base.m_iAttackDirectionVariance;

		if(base.m_iAttackPreferredDirection > -1)
			Print("[Overthrow] QRF starting from preferred direction: " + base.m_iAttackPreferredDirection.ToString() + " +/- " + base.m_iAttackDirectionVariance.ToString());

		m_CurrentQRF.Start();

		RplComponent rpl = RplComponent.Cast(m_CurrentQRF.GetOwner().FindComponent(RplComponent));

		m_CurrentQRF.m_OnFinished.Insert(OnQRFFinishedBase);
		m_CurrentQRFBase = base;

		m_bQRFActive = true;
		m_vQRFLocation = base.GetOwner().GetOrigin();
		m_iCurrentQRFBase= GetBaseIndex(data);

		// A standard battle is announced the instant it starts, exactly as it always has been. A siege
		// says nothing until its encirclement is complete - RevealQRF() sends the notification then.
		m_bQRFRevealed = false;
		if(mode == OVT_EQRFMode.STANDARD)
		{
			m_bQRFRevealed = true;

			OVT_Global.GetNotify().SendTextNotification("BaseBattle", -1, base.m_sName);
			OVT_Global.GetNotify().SendExternalNotifications("BaseBattle", base.m_sName);
		}

		Rpc(RpcDo_SetQRFBase, m_iCurrentQRFBase);
		Rpc(RpcDo_SetQRFActive, m_vQRFLocation);
		Rpc(RpcDo_SetQRFRevealed, m_bQRFRevealed);

		// ⚠ AFTER the mode is configured and Start() has run (occupying/vehicles T5.1/C9) - a base QRF
		// publishes here even though m_OnQRFTownChanged never does for a base.
		m_OnBattleStarted.Invoke(m_vQRFLocation);
	}

	//------------------------------------------------------------------------------------------------
	//! Starts a battle for a town or city. See StartBaseQRF for the mode argument.
	//! \param[in] town The town being fought over.
	//! \param[in] mode STANDARD for a player-initiated uprising; COUNTER_ATTACK for a silent siege.
	void StartTownQRF(OVT_TownData town, OVT_EQRFMode mode = OVT_EQRFMode.STANDARD)
	{
		if(m_CurrentQRF) return;

		int townID = OVT_Global.GetTowns().GetTownID(town);

		m_CurrentQRF = SpawnQRFController(town.location);
		m_CurrentQRF.m_eMode = mode;

		// Find the town controller to get QRF parameters
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		EntityID townControllerID;
		if(townID >= 0 && townID < townManager.m_TownControllers.Count())
			townControllerID = townManager.m_TownControllers.Get(townID);
		if(townControllerID)
		{
			IEntity townEntity = GetGame().GetWorld().FindEntityByID(townControllerID);
			if(townEntity)
			{
				OVT_TownControllerComponent townController = OVT_TownControllerComponent.Cast(townEntity.FindComponent(OVT_TownControllerComponent));
				if(townController)
				{
					m_CurrentQRF.m_iLZMin = townController.m_iAttackDistanceMin;
					m_CurrentQRF.m_iLZMax = townController.m_iAttackDistanceMax;
					m_CurrentQRF.m_iPreferredDirection = townController.m_iAttackPreferredDirection;
					m_CurrentQRF.m_iDirectionVariance = townController.m_iAttackDirectionVariance;
					
					if(townController.m_iAttackPreferredDirection > -1)
						Print("[Overthrow] Town QRF starting from preferred direction: " + townController.m_iAttackPreferredDirection.ToString() + " +/- " + townController.m_iAttackDirectionVariance.ToString());
				}
			}
		}
		
		RplComponent rpl = RplComponent.Cast(m_CurrentQRF.GetOwner().FindComponent(RplComponent));
		
		m_CurrentQRF.Start();

		m_CurrentQRF.m_OnFinished.Insert(OnQRFFinishedTown);
		m_CurrentQRFTown = town;

		m_bQRFActive = true;
		m_vQRFLocation = town.location;
		m_iCurrentQRFTown = townID;

		// A standard battle is announced the instant it starts. A siege says nothing until RevealQRF().
		m_bQRFRevealed = false;
		if(mode == OVT_EQRFMode.STANDARD)
		{
			m_bQRFRevealed = true;

			string type = "Village";
			if(town.size == 2) type = "Town";
			if(town.size == 3) type = "City";
			OVT_Global.GetNotify().SendTextNotification(type + "Battle", -1, OVT_Global.GetTowns().GetTownName(townID));
			OVT_Global.GetNotify().SendExternalNotifications(type + "Battle", OVT_Global.GetTowns().GetTownName(townID));
		}

		Rpc(RpcDo_SetQRFTown, m_iCurrentQRFTown);
		Rpc(RpcDo_SetQRFActive, m_vQRFLocation);
		Rpc(RpcDo_SetQRFRevealed, m_bQRFRevealed);

		// Town-local suppression (D6): only THIS town's ambient crowd goes away.
		//
		// ⚠ THIS IS THE FIRST HALF OF A PAIRED TRANSITION - OnQRFFinishedTown fires the matching -1.
		// In counter-attack mode it moves to the BATTLE transition (OnQRFEngaged), because emptying the
		// target town of civilians half an hour before the resistance is told anything is the loudest
		// possible tell. The pairing survives BY CONSTRUCTION: a siege cannot resolve without passing
		// through BATTLE (see OVT_QRFControllerComponent.EnterBattle), and nothing may ever introduce a
		// path that lets it.
		if(mode == OVT_EQRFMode.STANDARD)
			m_OnQRFTownChanged.Invoke(m_iCurrentQRFTown);

		// ⚠ UNLIKE m_OnQRFTownChanged above, THIS FIRES FOR BOTH MODES (C9) - after the mode is
		// configured and Start() has run.
		m_OnBattleStarted.Invoke(m_vQRFLocation);
	}

	//------------------------------------------------------------------------------------------------
	//! Is the current battle actually being FOUGHT, as opposed to merely existing? (D15)
	//!
	//! The three server-side world-suppression gates - the occupying economy tick, deployment
	//! evaluation and the objective town's civilian crowd - all ask this rather than `m_CurrentQRF`,
	//! so that a silent siege leaves the world running until its assault begins.
	//!
	//! ⚠ SERVER-ONLY. m_CurrentQRF is never set on a client; a client asking this always gets false,
	//! which is why the client-facing rules are on m_bQRFRevealed instead.
	//!
	//! ⚠ TRUE FROM CREATION FOR A STANDARD BATTLE, so nothing about a player-initiated battle changes.
	//! \return True while shots are being fired over the objective.
	bool IsQRFEngaged()
	{
		if(!m_CurrentQRF) return false;

		return m_CurrentQRF.IsEngaged();
	}

	//------------------------------------------------------------------------------------------------
	//! Tells the resistance that a counter-attack has surrounded one of their places.
	//!
	//! Called by the battle controller at the SILENT_DEPLOY -> MUSTER transition, once, when the last
	//! group is on the ground. Flips m_bQRFRevealed - which is what turns the HUD panel, the map circle
	//! and the travel/respawn restrictions on - and sends the notification the siege has been holding.
	//!
	//! ⚠ IDEMPOTENT AND SELF-GUARDING. A standard battle is already revealed and this is a no-op for
	//! it, so a future caller cannot accidentally send a counter-attack notification for a player's own
	//! battle.
	void RevealQRF()
	{
		if(!m_CurrentQRF) return;
		if(m_bQRFRevealed) return;

		m_bQRFRevealed = true;
		Rpc(RpcDo_SetQRFRevealed, true);

		// ⚠ WHICH KIND OF BATTLE THIS IS COMES OFF THE **INDICES**, NOT OFF m_CurrentQRFBase /
		// m_CurrentQRFTown. Those two object handles are set by the starters and are NEVER CLEARED -
		// neither finish handler touches them, only the indices beside them are reset to -1. Reading
		// the handles would make a base siege announce the name of whatever town was fought over last,
		// which is a confident lie about where the enemy is.
		//
		// Cities use the town tag; villages are never counter-attack objectives (the objective director
		// only ever selects a town or a base), so there is no size branch here.
		if(m_iCurrentQRFTown > -1)
		{
			string townName = OVT_Global.GetTowns().GetTownName(m_iCurrentQRFTown);
			OVT_Global.GetNotify().SendTextNotification("CounterAttackTown", -1, townName);
			OVT_Global.GetNotify().SendExternalNotifications("CounterAttackTown", townName);

			Print("[Overthrow] Counter-attack revealed at town " + townName);
			return;
		}

		// The handle is still what carries the base's NAME, so it is read - but only behind the index,
		// and only when it is actually there.
		if(m_iCurrentQRFBase > -1 && m_CurrentQRFBase)
		{
			OVT_Global.GetNotify().SendTextNotification("CounterAttackBase", -1, m_CurrentQRFBase.m_sName);
			OVT_Global.GetNotify().SendExternalNotifications("CounterAttackBase", m_CurrentQRFBase.m_sName);

			Print("[Overthrow] Counter-attack revealed at base " + m_CurrentQRFBase.m_sName);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The world stops living around the objective: the battle controller calls this at the MUSTER ->
	//! BATTLE transition of a counter-attack siege.
	//!
	//! The economy tick and the deployment evaluator need nothing - they poll IsQRFEngaged() and pick
	//! the change up on their own next tick. The civilian crowd is a TRANSITION rather than a poll, so
	//! it has to be pushed, and this is the first half of the pair StartTownQRF fires for a standard
	//! battle.
	//!
	//! ⚠ COUNTER-ATTACK ONLY. A standard battle has already invoked this in StartTownQRF and must never
	//! reach here, or a town would be announced twice.
	void OnQRFEngaged()
	{
		if(m_iCurrentQRFTown > -1)
			m_OnQRFTownChanged.Invoke(m_iCurrentQRFTown);
	}

	void OnQRFFinishedBase()
	{
		if(m_CurrentQRF.m_iWinningFaction != m_CurrentQRFBase.GetControllingFaction())
		{
			ChangeBaseControl(m_CurrentQRFBase, m_CurrentQRF.m_iWinningFaction);
		}

		SCR_EntityHelper.DeleteEntityAndChildren(m_CurrentQRF.GetOwner());
		m_CurrentQRF = null;

		m_bQRFActive = false;
		m_bQRFRevealed = false;
		m_iCurrentQRFBase = -1;
		m_iCurrentQRFTown = -1;

		Rpc(RpcDo_SetQRFInactive);
	}

	void ChangeBaseControl(OVT_BaseControllerComponent base, int newFactionIndex)
	{
		string townName = OVT_Global.GetTowns().GetTownName(base.GetOwner().GetOrigin());
		if(base.IsOccupyingFaction())
		{
			m_iThreat += 250;
			OVT_Global.GetNotify().SendTextNotification("BaseControlledResistance",-1,townName);
			OVT_Global.GetNotify().SendExternalNotifications("BaseControlledResistance",townName);
		}else{
			m_iThreat -= 250;
			OVT_Global.GetNotify().SendTextNotification("BaseControlledOccupying",-1,townName);
			OVT_Global.GetNotify().SendExternalNotifications("BaseControlledOccupying",townName);
		}
		
		OVT_BaseData baseData = GetNearestBase(base.GetOwner().GetOrigin());
		int baseIndex = GetBaseIndex(baseData);
		
		m_Bases[baseIndex].faction = newFactionIndex;
		base.SetControllingFaction(newFactionIndex);
		Rpc(RpcDo_SetBaseFaction, baseIndex, newFactionIndex);
	}

	void OnQRFFinishedTown()
	{
		int townID = OVT_Global.GetTowns().GetTownID(m_CurrentQRFTown);
		if(m_CurrentQRF.m_iWinningFaction != m_CurrentQRFTown.faction)
		{
			//This town has changed control
			string type = "Town";
			if(m_CurrentQRFTown.size > 2) type = "City";
			if(m_CurrentQRFTown.IsOccupyingFaction())
			{
				m_iThreat += 250;
				OVT_Global.GetTowns().TryAddSupportModifierByName(townID, "RecentBattlePositive");
			}else{
				m_iThreat -= 250;
				OVT_Global.GetTowns().TryAddSupportModifierByName(townID, "RecentBattleNegative");
				//All supporters in this town abandon the resistance (and avoids the battle looping)
				OVT_Global.GetTowns().ResetSupport(m_CurrentQRFTown);
			}
			OVT_Global.GetTowns().ChangeTownControl(m_CurrentQRFTown, m_CurrentQRF.m_iWinningFaction);

		}else{
			//This town has NOT changed control, but we still need to add modifiers
			if(m_CurrentQRFTown.IsOccupyingFaction())
			{
				OVT_Global.GetTowns().TryAddSupportModifierByName(townID, "RecentBattleNegative");
				//All supporters in this town abandon the resistance (and avoids the battle looping)
				OVT_Global.GetTowns().ResetSupport(m_CurrentQRFTown);
			}else{
				m_iThreat += 250;
				OVT_Global.GetTowns().TryAddSupportModifierByName(townID, "RecentBattlePositive");
			}
		}

		OVT_Global.GetTowns().TryAddStabilityModifierByName(townID, "RecentBattle");

		SCR_EntityHelper.DeleteEntityAndChildren(m_CurrentQRF.GetOwner());
		m_CurrentQRF = null;

		m_bQRFActive = false;
		m_bQRFRevealed = false;
		m_iCurrentQRFBase = -1;
		m_iCurrentQRFTown = -1;

		Rpc(RpcDo_SetQRFInactive);

		// No town is under attack any more - whatever suppressed itself for this battle comes back.
		// The SECOND HALF of the paired transition; the first is in StartTownQRF for a standard battle
		// and in OnQRFEngaged for a counter-attack siege.
		m_OnQRFTownChanged.Invoke(m_iCurrentQRFTown);
	}

	void WinBattle()
	{
		if(!m_CurrentQRF) return;
		m_CurrentQRF.KillAll();
	}

	bool CheckBaseAdd(IEntity ent)
	{
		#ifdef OVERTHROW_DEBUG
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		OVT_TownData closestTown = townManager.GetNearestTown(ent.GetOrigin());
		Print("Adding base near " + closestTown.name);
		#endif

		int occupyingFactionIndex = m_Config.GetOccupyingFactionIndex();

		OVT_BaseData data = new OVT_BaseData();
		data.entId = ent.GetID();
		data.id = m_Bases.Count();
		data.location = ent.GetOrigin();
		data.faction = m_Config.GetOccupyingFactionIndex();

		// COPIED AT DISCOVERY, not looked up on demand. The objective candidate set is rebuilt every
		// director tick and reads this once per base per round; resolving the marker entity each time
		// would put a FindEntityByID in that loop for a value that cannot change while the world is
		// loaded. FilterBaseEntities has already proven the component is there.
		OVT_BaseControllerComponent controller = OVT_BaseControllerComponent.Cast(ent.FindComponent(OVT_BaseControllerComponent));
		if(controller)
			data.landIsolated = controller.m_bLandIsolated;

		m_Bases.Insert(data);
		return true;
	}

	OVT_QRFControllerComponent SpawnQRFController(vector loc)
	{
		IEntity qrf = OVT_Global.SpawnEntityPrefab(m_pQRFControllerPrefab, loc);
		return OVT_QRFControllerComponent.Cast(qrf.FindComponent(OVT_QRFControllerComponent));
	}

	bool FilterBaseEntities(IEntity entity)
	{
		OVT_BaseControllerComponent controller = OVT_BaseControllerComponent.Cast(entity.FindComponent(OVT_BaseControllerComponent));
		if(controller) return true;

		return false;
	}

	bool CheckTransmitterTowerAdd(IEntity ent)
	{
		int occupyingFactionIndex = OVT_Global.GetConfig().GetOccupyingFactionIndex();

		OVT_RadioTowerData data = new OVT_RadioTowerData;
		data.id = m_RadioTowers.Count();
		data.location = ent.GetOrigin();
		data.faction = occupyingFactionIndex;

		m_RadioTowers.Insert(data);

		return true;
	}

	bool FilterTransmitterTowerEntities(IEntity entity)
	{
		OVT_TowerControllerComponent controller = OVT_TowerControllerComponent.Cast(entity.FindComponent(OVT_TowerControllerComponent));
		if(controller) return true;

		return false;
	}

	//! HOW FAR A KNOWN ENEMY TARGET PROJECTS THREAT, in metres.
	//!
	//! ⚠ WIDENED 1000 -> 2500 ON 2026-08-20, and the reason is that this score became load-bearing that
	//! day. It used to be added to the global campaign threat (~420) before anything read it, so its
	//! range and its weights barely mattered - the sum was dominated by the constant. Now it IS the
	//! deployment evaluator's entire notion of where danger is, and at 1 km a base the resistance had
	//! taken projected nothing at all onto the next base along, or onto the towns between them. The
	//! occupying faction could not see its own frontline.
	static const float KNOWN_TARGET_THREAT_RANGE = 2500;

	//! WHAT EACH KIND OF ENEMY HOLDING IS WORTH at zero distance, falling off linearly to nothing at
	//! KNOWN_TARGET_THREAT_RANGE.
	//!
	//! ⚠ THESE WERE 10 / 5 / 5 / 1 AND ARE NOW MUCH LARGER, deliberately and for the same reason as the
	//! range. Against a ~420 constant they were rounding error; against nothing but each other and the
	//! town terms they are the signal. The author's own list of what should count - "support, radio
	//! towers and captured bases" - is exactly what these encode, with support already handled by the
	//! town loop below.
	//!
	//! THE ORDER IS THE POINT, not the absolute numbers: a base the resistance has TAKEN is the most
	//! threatening thing on the map to the faction that lost it, a radio tower it cannot broadcast from
	//! is next, then a camp, then a warehouse. Tune the ratios before tuning the magnitudes.
	static const float THREAT_WEIGHT_ENEMY_BASE = 40;
	static const float THREAT_WEIGHT_ENEMY_TOWER = 25;
	static const float THREAT_WEIGHT_ENEMY_FOB = 20;
	static const float THREAT_WEIGHT_ENEMY_CAMP = 20;
	static const float THREAT_WEIGHT_ENEMY_WAREHOUSE = 5;

	//! How close two vehicle losses have to be to count as the SAME incident, in metres (T6.1). Loose
	//! enough that a convoy wiped out along one stretch of road becomes one known target, not one wreck
	//! at a time - deduplicated against GetNearestKnownTarget(), never against an exact position match.
	static const float VEHICLE_LOSS_DEDUP_RADIUS_M = 200;

	int GetThreatByLocation(vector pos)
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		int villageRange = towns.m_iVillageRange;
		int townRange = towns.m_iTownRange;
		int cityRange = towns.m_iCityRange;
		
		
		int score = 0;
		foreach(OVT_TargetData target : m_aKnownTargets)
		{
			float distance = vector.Distance(target.location, pos);
			if(distance < KNOWN_TARGET_THREAT_RANGE)
			{
				float distanceFactor = 1.0 - (distance / KNOWN_TARGET_THREAT_RANGE);
				if(target.type == OVT_TargetType.BASE)
				{
					score += (int)Math.Round(THREAT_WEIGHT_ENEMY_BASE * distanceFactor);
				}
				if(target.type == OVT_TargetType.BROADCAST_TOWER)
				{
					score += (int)Math.Round(THREAT_WEIGHT_ENEMY_TOWER * distanceFactor);
				}
				if(target.type == OVT_TargetType.FOB)
				{
					score += (int)Math.Round(THREAT_WEIGHT_ENEMY_FOB * distanceFactor);
				}
				// ⚠ CAMP WAS SCORED AT NOTHING UNTIL 2026-08-20, and it is not a new target type -
				// UpdateKnownTargets() has always inserted one for every resistance camp it knows about,
				// and this loop simply had no branch for it. A resistance camp contributed exactly zero
				// to how dangerous the occupying faction considered the ground around it.
				if(target.type == OVT_TargetType.CAMP)
				{
					score += (int)Math.Round(THREAT_WEIGHT_ENEMY_CAMP * distanceFactor);
				}
				if(target.type == OVT_TargetType.WAREHOUSE)
				{
					score += (int)Math.Round(THREAT_WEIGHT_ENEMY_WAREHOUSE * distanceFactor);
				}
			}
		}
		foreach(OVT_TownData town : OVT_Global.GetTowns().m_Towns)
		{
			int range = villageRange;
			if(town.size == 2) range = townRange;
			if(town.size == 3) range = cityRange;
			
			float distance = vector.Distance(town.location, pos);
			if(distance > range * 3) continue;
			
			float distanceFactor = 1.0 - (distance / ((float)range * 3));
			
			if(town.IsOccupyingFaction())
			{
				int supportScore = (int)Math.Round(((float)town.SupportPercentage() / 100) * distanceFactor * 5 * town.size);
				int stabilityScore = (int)Math.Round((1 - ((float)town.stability / 100)) * distanceFactor * 5 * town.size);				
				score += supportScore + stabilityScore;
			}else{
				int townScore = (int)Math.Round(5 * distanceFactor * town.size);
				score += townScore;
			}	
		}
		return score;
	}

	int GetThreatLevel() {return m_iThreat;}

	//------------------------------------------------------------------------------------------------
	//! Current campaign threat at full precision.
	//! m_iThreat is a float; GetThreatLevel() declares int and therefore TRUNCATES, showing 3 where
	//! the campaign actually holds 3.87. Use this accessor anywhere the fractional part matters.
	//! \return The threat value, unrounded.
	float GetThreatFloat() {return m_iThreat;}

	//------------------------------------------------------------------------------------------------
	//! CAN THIS FACTION PUT A LADDER VEHICLE ON THE ROAD AT ALL, at the threat it has right now?
	//!
	//! Every mounted doctrine carries its crew and NOTHING ELSE, so a dispatch made below the bottom
	//! rung does not degrade into a smaller convoy - it marches three crewmen across the map on foot.
	//! Asked with an UNBOUNDED budget: this is "has the campaign escalated far enough for the ladder to
	//! answer anything", not "can this config afford what it answered", which each module still asks
	//! for itself against its own m_iTruckCostOverride.
	//! \param[in] role The ladder role, as authored on the faction's vehicle registry.
	//! \return True when at least one rung is unlocked.
	bool CanFieldLadderVehicle(string role = "armed")
	{
		OVT_OverthrowConfigComponent gameConfig = OVT_Global.GetConfig();
		if (!gameConfig)
			return false;

		OVT_Faction faction = gameConfig.GetOccupyingFaction();
		if (!faction)
			return false;

		faction.InitializeVehicleRegistry();

		float scale = 1;
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (difficulty)
			scale = difficulty.vehicleThresholdScale;

		OVT_FactionVehicleEntry entry;
		return faction.ResolveVehicleForRole(role, GetThreatFloat(), scale, -1, entry);
	}

	int GetBaseThreat(OVT_BaseData base)
	{
		return GetThreatByLocation(base.location);
	}

	void CheckUpdate()
	{
		if(!m_Time)
		{
			ChimeraWorld world = GetOwner().GetWorld();
			m_Time = world.GetTimeAndWeatherManager();
		}

		PlayerManager mgr = GetGame().GetPlayerManager();
		if(mgr.GetPlayerCount() == 0)
		{
			//Clear dead bodies when no players are online
			SCR_AIWorld aiworld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
			autoptr array<AIAgent> agents = new array<AIAgent>;
			aiworld.GetAIAgents(agents);
			foreach(AIAgent agent : agents)
			{
				DamageManagerComponent dmg = DamageManagerComponent.Cast(agent.FindComponent(DamageManagerComponent));
				if(dmg && dmg.IsDestroyed())
				{
					//Is dead, remove body
					SCR_EntityHelper.DeleteEntityAndChildren(agent);
				}
			}
			return;
		}

		TimeContainer time = m_Time.GetTime();

		//We dont get/spend resources or reduce threat during a QRF
		//
		// ⚠ ENGAGED, NOT MERELY EXISTING (D15). A counter-attack siege exists for up to 31 minutes
		// before it fights, and stalling the faction's whole income for half an hour before the
		// resistance has been told anything would be both a dead world and a tell.
		if(IsQRFEngaged()) return;

		//Every 6 hrs get more resources
		if((time.m_iHours == 0
			|| time.m_iHours == 6
			|| time.m_iHours == 12
			|| time.m_iHours == 18)
			 &&
			time.m_iMinutes == 0
			 &&
			m_iHourGainedResources != time.m_iHours)
		{
			m_iHourGainedResources = time.m_iHours;

			GainAndSpendResources();
		}

		//Every hour, pay one slice of what the pool is still owed. The minute is jittered, so the gate
		//is ">= the rolled minute" rather than "== it": a tick lost to a QRF freeze or a long frame
		//must delay the drip inside its hour, never cancel it.
		if(m_iPendingDefenseTransfer > 0
			&&
			m_iHourDripped != time.m_iHours
			&&
			time.m_iMinutes >= m_iDripMinute)
		{
			m_iHourDripped = time.m_iHours;

			DripDefenseShare();
		}
		//Every 15 mins reduce threat
		if(time.m_iMinutes == 0
			|| time.m_iMinutes == 15
			|| time.m_iMinutes == 30
			|| time.m_iMinutes == 45)
		{
			//THE LATCH IS ON THE DECAY: the payload is owed exactly once per quarter-hour boundary,
			//and a second visit to the same boundary (a longer tick, a sleep replay) must not pay twice.
			if(m_iMinuteDecayedThreat != time.m_iMinutes)
			{
				m_iMinuteDecayedThreat = time.m_iMinutes;

				DecayThreatStep();
			}
		}

		// Every tick: the hunter-killer dispatcher (T6.4). No latch of its own - its four gates are
		// idempotent and self-healing (see the method header), so asking every 60 s is the whole design.
		TickHunterKiller();
	}

	//------------------------------------------------------------------------------------------------
	//! One six-hour resource boundary: gain, move the defense share into the deployment pool, report.
	//!
	//! Lifted out of CheckUpdate (resistance/sleep) because there are now THREE callers - the live
	//! tick, the sleep replay's chronological loop, and the replay's start-boundary flush - and a
	//! second implementation of a payout is a defect by construction.
	//!
	//! WHAT THIS IS NOT ANY MORE. On main this method also called SpendResourcesOnBases() and
	//! UpdateSpecops(). Both were deleted by virtualization/base-defense-migration: the per-base
	//! spend became TransferDefenseShareToPool below, and specops left with the legacy spender. The
	//! sleep replay therefore replays exactly what the live tick now does - which is the whole point
	//! of routing both through this one method.
	//!
	//! THE LATCH IS NOT SET HERE. Each caller owns its own idea of "which boundary was that", so each
	//! caller writes m_iHourGainedResources itself; burying the write in here would leave the replay
	//! loop - which runs this many times for many different boundaries - writing a meaningless value.
	protected void GainAndSpendResources()
	{
		int newResources = GainResources();

		// KEPT, AND NOT A SPECOPS LEFTOVER. m_aKnownTargets feeds GetThreatByLocation(), which is
		// what the DEPLOYMENT EVALUATOR sorts its candidates by and what the player's map threat
		// overlay reads. Dropping this call would freeze both at whatever PostGameStart() computed.
		UpdateKnownTargets();

		// THE ONE DEFENSE SPEND PATH. 80 % of every tick moves into the deployment pool, which is
		// where base defense, town patrols, tower garrisons and vehicle patrols are ALL bought from.
		//
		// WHAT STOOD HERE WAS A SECOND SPENDER: bases sorted by threat, an EVEN per-base split, a
		// "skip a base a player is standing near" rule, and the base controller's legacy spender
		// turning each budget into men through the base-upgrade classes. Every one of those concerns
		// now belongs to the deployment framework, which already does them better:
		//   - threat ordering    -> EvaluateFactionDeployments() sorts candidates by threat, with
		//                           jitter, instead of an even split that only decided serving order;
		//   - the proximity skip -> OVT_NoPlayersNearbyConditionDeploymentModule, as a CREATION gate;
		//   - the 1..19 priority sweep -> each config's m_iPriority and the escalation selection.
		//
		// The other 20 % stays in m_iResources, which remains the QRF sizing reserve. That reserve is
		// now spent deliberately rather than by dice: OVT_ObjectiveDirectorComponent gates its
		// counter-attack on objectiveQRFResourceGate (occupying/counter-attacks Phase 1 retired the
		// hourly random roll that used to draw on it).
		// ARMED, NOT PAID (2026-08-22). This used to be TransferDefenseShareToPool(newResources) and
		// the whole 80 % crossed here in one statement. It is now a debt the reserve owes the pool,
		// paid one slice an hour by DripDefenseShare(); see the drip block's header for why the
		// TRANSFER moved and the income did not.
		ArmDefenseShareDrip(newResources);

		Print("[Overthrow.OccupyingFactionManager] Reserve Resources: " + m_iResources.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! One quarter-hour step of threat decay.
	//!
	//! Lifted out of CheckUpdate's fifteen-minute branch so the sleep time-skip replay can run the
	//! same decay the live tick runs. ONLY the decay moved: the town-uprising scan that follows it in
	//! CheckUpdate stayed behind deliberately (see HandleTimeSkip).
	protected void DecayThreatStep()
	{
		// OVT_Global.GetDifficulty() is null-guarded now (it returns null instead of a VME before
		// the config exists), so the dereference has to be guarded here too. No difficulty means no
		// threat reduction this step - fail closed rather than crash the campaign tick.
		int threatReduce = 0;
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if(difficulty)
			threatReduce = Math.Ceil((float)m_iThreat * difficulty.threatReductionFactor);

		m_iThreat -= threatReduce;
		if(m_iThreat < 0) m_iThreat = 0;

		Print("[Overthrow.OccupyingFactionManager] Reduced Threat to: " + m_iThreat.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! Replays the occupying faction's resource gain, spend and threat decay for a skipped window.
	//!
	//! CONTRACT (implementation.md 3.3), shared word for word with OVT_EconomyManagerComponent's
	//! HandleTimeSkip: called on the AUTHORITY, BEFORE the world clock is advanced. The current
	//! in-game time is the START of the skipped window, and the window is (start, start + hours] -
	//! HALF-OPEN at the start, CLOSED at the end. Anything owed exactly at the start is the live
	//! tick's job, not ours.
	//!
	//! BOTH EDGES ARE DEFENDED, AND THEY ARE NOT THE SAME DEFENCE (2026-08-19 review fix). The economy
	//! manager protects the open start with a flush CheckUpdate() and the closed end with
	//! AssertHourLatches(landingHour); this one had neither, and its live gates had no latch to assert.
	//! So: step 1 below flushes the two boundaries that can fall exactly ON the start instant, each
	//! behind its own latch, and step 3 leaves both latches asserted at the landing instant. Without
	//! step 3 a skip landing exactly on a boundary hour produces a THIRD "Gaining Resources" pair when
	//! the live tick resumes inside the same in-game minute (Q1/F7); without step 1 a skip beginning
	//! exactly on one loses that payday altogether (Q2).
	//!
	//! CHRONOLOGICAL, NOT BATCHED (D6). The loop walks the window in quarter-hour steps and does the
	//! resource work on the steps that are also six-hour boundaries, exactly as the live tick does.
	//! Batching it as "two gains, then thirty-two decays" would produce different numbers, because
	//! GainResources scales with m_iThreat and m_iThreat decays between the gains. The loop IS the
	//! simulation, at fifteen-minute resolution.
	//!
	//! ===========================================================================================
	//! TWO THINGS ARE EXCLUDED ON PURPOSE. DO NOT "FIX" THEM (D7).
	//!
	//!  1. THE COUNTER-ATTACK ROLL. It is a random surplus check, not accounting. Replaying it would
	//!     hand a sleeping player thirty-two rolls at a base QRF launching on top of them.
	//!  2. THE TOWN-UPRISING SCAN. It is gated on PlayerInRange(town, 300) and is world-side. A
	//!     sleeping player is by definition within 300 m of the town they are sleeping in, so thirty-
	//!     two replays could start a town QRF onto a screen that is fading to black.
	//!
	//! Neither is a payout, so neither is owed. Anything the player would have missed by BEING there
	//! is not something the skip is obliged to manufacture.
	//! ===========================================================================================
	//! \param[in] hours Length of the skip in whole in-game hours. Non-positive is ignored.
	void HandleTimeSkip(int hours)
	{
		if(!Replication.IsServer()) return;
		if(hours <= 0) return;

		if(!m_Time)
		{
			ChimeraWorld world = GetOwner().GetWorld();
			if(!world) return;
			m_Time = world.GetTimeAndWeatherManager();
		}

		if(!m_Time) return;

		TimeContainer time = m_Time.GetTime();
		if(!time) return;

		//Read into plain ints before the replay: GetTime() hands back a snapshot, not a live view.
		int startHour = time.m_iHours;
		int startMinute = time.m_iMinutes;

		int startAbsoluteMinute = (startHour * OVT_SleepSchedule.MINUTES_PER_HOUR) + startMinute;

		//1. THE OPEN START, CLOSED. The economy manager settles this by calling its own CheckUpdate()
		//   once (implementation.md D3), which is not available here - this CheckUpdate returns early
		//   on a live QRF, which the replay is not allowed to do. So the two boundaries that CAN be
		//   owed exactly at the start are
		//   flushed explicitly instead, each behind its own latch so a live tick that already took it
		//   is not paid twice. Without this a sleep beginning exactly at 12:00 loses the 12:00 payday
		//   the live tick still owed (Q2), and one beginning on a quarter hour loses a decay step.
		if(OVT_SleepSchedule.IsIntervalBoundary(startAbsoluteMinute, OVT_SleepSchedule.INCOME_INTERVAL_HOURS)
			&& m_iHourGainedResources != startHour)
		{
			m_iHourGainedResources = startHour;

			GainAndSpendResources();
		}

		if(OVT_SleepSchedule.IsStepBoundary(startAbsoluteMinute, OVT_SleepSchedule.THREAT_STEP_MINUTES)
			&& m_iMinuteDecayedThreat != startMinute)
		{
			m_iMinuteDecayedThreat = startMinute;

			DecayThreatStep();
		}

		//2. The window itself, chronologically.
		int steps = OVT_SleepSchedule.CountStepCrossings(startHour, startMinute, hours, OVT_SleepSchedule.THREAT_STEP_MINUTES);

		for(int i = 0; i < steps; i++)
		{
			int stepMinute = OVT_SleepSchedule.StepMinuteAt(startHour, startMinute, OVT_SleepSchedule.THREAT_STEP_MINUTES, i);

			//Same order as the live tick: gain and ARM the defense share, drip whatever this hour
			//owes, then the decay for that same step.
			if(OVT_SleepSchedule.IsIntervalBoundary(stepMinute, OVT_SleepSchedule.INCOME_INTERVAL_HOURS))
			{
				GainAndSpendResources();
			}

			//THE DRIP IS REPLAYED ON THE HOUR, WITHOUT ITS JITTER. The rolled minute decides WHEN
			//inside an hour a live drip lands, which is a feel choice with no bearing on the totals; a
			//replay that is fast-forwarding whole hours has no "inside the hour" to land in. What the
			//replay owes is the same NUMBER of drips a played window would have paid, which is one per
			//hour boundary crossed.
			if(OVT_SleepSchedule.IsIntervalBoundary(stepMinute, OVT_BaseDefenseConversion.DRIP_INTERVAL_HOURS))
			{
				DripDefenseShare();
			}

			DecayThreatStep();
		}

		//3. THE CLOSED END, LATCHED - the exact analogue of the economy manager's
		//   AssertHourLatches(LandingHour(...)) (D4). The window is closed at the end, so the replay has
		//   just paid for the landing instant; the tick that resumes a fraction of a second later still
		//   reads that same in-game minute, and without these two writes it would gain and decay a
		//   SECOND time for it. That is the third "Gaining Resources"/"Reserve Resources" pair Q1/F7
		//   forbid, and the thirty-third decay step.
		//
		//   The landing MINUTE of hour is startMinute, because the skip is a whole number of hours -
		//   OVT_SleepService.AdvanceClock preserves minutes and seconds. A landing hour that is not on
		//   the six-hour grid (or a start minute that is not on the quarter-hour grid) leaves an inert
		//   value behind, exactly as AssertHourLatches leaves the stock latch at hour 14.
		m_iHourGainedResources = OVT_SleepSchedule.LandingHour(startHour, hours);
		m_iMinuteDecayedThreat = startMinute;
		m_iHourDripped = OVT_SleepSchedule.LandingHour(startHour, hours);
	}

	//------------------------------------------------------------------------------------------------
	//! Moves this tick's defense share out of the occupying faction's reserve and into its deployment
	//! resource pool.
	//!
	//! THE CONSERVED-TOTAL IDENTITY, AND IT IS UNCONDITIONAL: the pool rises by EXACTLY what the reserve
	//! falls by. Nothing is created, nothing is destroyed, and after this phase there is exactly one
	//! code path that credits the pool (AllocateDeploymentResources) and exactly one that spends it on
	//! defense (the deployment evaluator).
	//!
	//! THE CLAMP TO THE RESERVE IS PARITY, NOT CAUTION. The per-base loop this replaced clamped every
	//! base's budget the same way (`if(budget > m_iResources) budget = m_iResources;`). It is what makes
	//! the identity hold in every state, including the degenerate ones a test can construct: the reserve
	//! can never go negative and the pool can never be handed money that did not exist.
	//!
	//! PUBLIC, because the identity is only assertable as a live claim by driving it.
	//! GainAndSpendResources() is its only production caller - which since resistance/sleep means the
	//! live tick AND the sleep replay reach it through the same one line.
	//! \param[in] newResources The resources gained this tick, as GainResources() reported them.
	void TransferDefenseShareToPool(int newResources)
	{
		TransferToPool(OVT_BaseDefenseConversion.DefenseShare(newResources));
	}

	//------------------------------------------------------------------------------------------------
	//! Moves an EXACT amount out of the reserve and into the deployment pool, clamped to the reserve.
	//!
	//! The conserved-total identity described above lives here now - both the six-hour share and every
	//! hourly drip reach the pool through this one statement pair, so there is still exactly one place
	//! where the reserve falls and the pool rises, and it is still by the same number.
	//! \param[in] amount The amount to move. Non-positive, or a reserve of zero, moves nothing.
	//! \return What actually moved, which is less than amount only when the reserve was short.
	int TransferToPool(int amount)
	{
		if(amount > m_iResources) amount = m_iResources;
		if(amount <= 0) return 0;

		AllocateDeploymentResources(amount);
		m_iResources -= amount;

		return amount;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens a new drip window: settles whatever the last one still owed, then arms this tick's share.
	//!
	//! THE FLUSH IS FIRST AND IS UNCONDITIONAL. A window can lose drips - CheckUpdate returns early
	//! for the whole of an engaged QRF, a campaign can be loaded mid-window with the hour latch armed,
	//! and a sleep replay only drips on the boundaries it walks. Paying the remainder here is what
	//! makes every one of those cases cost TIMING and never MONEY: over any two paydays the pool has
	//! received exactly the defense share of everything the reserve was paid, which is the same
	//! invariant the single transfer guaranteed instant-by-instant.
	//! \param[in] newResources The resources gained this tick, as GainResources() reported them.
	void ArmDefenseShareDrip(int newResources)
	{
		FlushDefenseShareDrip();

		m_iPendingDefenseTransfer = OVT_BaseDefenseConversion.DefenseShare(newResources);
		m_iDefenseDripsRemaining = OVT_BaseDefenseConversion.DRIP_STEPS;

		RollDripMinute();
	}

	//------------------------------------------------------------------------------------------------
	//! Pays one slice of the current window's debt into the deployment pool.
	//!
	//! The drip COUNTER is decremented even when the reserve was too short to pay the slice in full.
	//! That is what stops a starved campaign accumulating an unpayable debt across windows: the flush
	//! at the next payday takes one more attempt at whatever is left, and then the arm overwrites it.
	//! \return What actually reached the pool.
	int DripDefenseShare()
	{
		if(m_iPendingDefenseTransfer <= 0)
			return 0;

		int amount = OVT_BaseDefenseConversion.DripAmount(m_iPendingDefenseTransfer, m_iDefenseDripsRemaining);
		int moved = TransferToPool(amount);

		m_iPendingDefenseTransfer -= moved;
		if(m_iPendingDefenseTransfer < 0) m_iPendingDefenseTransfer = 0;

		m_iDefenseDripsRemaining--;
		if(m_iDefenseDripsRemaining < 0) m_iDefenseDripsRemaining = 0;

		RollDripMinute();

		return moved;
	}

	//------------------------------------------------------------------------------------------------
	//! Pays out everything the current window still owes, in one move, and closes the window.
	//! \return What actually reached the pool.
	int FlushDefenseShareDrip()
	{
		if(m_iPendingDefenseTransfer <= 0)
		{
			m_iPendingDefenseTransfer = 0;
			m_iDefenseDripsRemaining = 0;
			return 0;
		}

		int moved = TransferToPool(m_iPendingDefenseTransfer);

		m_iPendingDefenseTransfer = 0;
		m_iDefenseDripsRemaining = 0;

		return moved;
	}

	//------------------------------------------------------------------------------------------------
	//! Rolls the minute of the hour the next drip fires at. RandInt is max-EXCLUSIVE, so the spread is
	//! the count of legal minutes and not the last one.
	protected void RollDripMinute()
	{
		m_iDripMinute = Math.RandomInt(0, OVT_BaseDefenseConversion.DRIP_MINUTE_SPREAD);
	}

	void UpdateKnownTargets()
	{
		//To-Do: target discovery not by magic
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		
		foreach(OVT_CampData fob : resistance.m_Camps)
		{
			if(!IsKnownTarget(fob.location))
			{
				OVT_TargetData target = new OVT_TargetData();
				target.location = fob.location;
				target.type = OVT_TargetType.CAMP;
				target.order = OVT_OrderType.ATTACK;
				m_aKnownTargets.Insert(target);
			}
		}
		
		foreach(OVT_BaseData data : m_Bases)
		{
			if(data.IsOccupyingFaction()){
				if(IsKnownTarget(data.location))
				{
					m_aKnownTargets.RemoveItem(GetNearestKnownTarget(data.location));
				}
				continue;
			}
			if(!IsKnownTarget(data.location))
			{
				OVT_TargetData target = new OVT_TargetData();
				target.location = data.location;
				target.type = OVT_TargetType.BASE;
				target.order = OVT_OrderType.ATTACK;
				m_aKnownTargets.Insert(target);
			}
		}
		
		foreach(OVT_RadioTowerData data : m_RadioTowers)
		{
			if(data.IsOccupyingFaction()){
				if(IsKnownTarget(data.location))
				{
					m_aKnownTargets.RemoveItem(GetNearestKnownTarget(data.location));
				}
				continue;
			}
			if(!IsKnownTarget(data.location))
			{
				OVT_TargetData target = new OVT_TargetData();
				target.location = data.location;
				target.type = OVT_TargetType.BROADCAST_TOWER;
				target.order = OVT_OrderType.ATTACK;
				m_aKnownTargets.Insert(target);
			}
		}

		foreach(OVT_FOBData fob : resistance.m_FOBs)
		{
			if(!IsKnownTarget(fob.location))
			{
				OVT_TargetData target = new OVT_TargetData();
				target.location = fob.location;
				target.type = OVT_TargetType.FOB;
				target.order = OVT_OrderType.ATTACK;
				m_aKnownTargets.Insert(target);
			}
		}
		
		foreach(OVT_CampData fob : resistance.m_Camps)
		{
			if(!IsKnownTarget(fob.location))
			{
				OVT_TargetData target = new OVT_TargetData();
				target.location = fob.location;
				target.type = OVT_TargetType.CAMP;
				target.order = OVT_OrderType.ATTACK;
				m_aKnownTargets.Insert(target);
			}
		}

		foreach(OVT_BaseData data : m_Bases)
		{
			if(data.IsOccupyingFaction()){
				if(IsKnownTarget(data.location))
				{
					m_aKnownTargets.RemoveItem(GetNearestKnownTarget(data.location));
				}
				continue;
			}
			if(!IsKnownTarget(data.location))
			{
				OVT_TargetData target = new OVT_TargetData();
				target.location = data.location;
				target.type = OVT_TargetType.BASE;
				target.order = OVT_OrderType.ATTACK;
				m_aKnownTargets.Insert(target);
			}
		}
		
	}

	bool IsKnownTarget(vector pos)
	{
		foreach(OVT_TargetData target : m_aKnownTargets)
		{
			if(vector.Distance(target.location, pos) < 1)
			{
				return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A vehicle was lost at `position` (occupying/vehicles T6.1) - a mounted force's own hull, or any
	//! other insertion's transport. Inserted through the SAME PATH every other known-target insert above
	//! uses (new OVT_TargetData, three fields set, m_aKnownTargets.Insert), so GetThreatByLocation() and
	//! TickHunterKiller() see it exactly like any other enemy holding.
	//!
	//! ⚠ NOT CALLED FROM A DEPLOYMENT ENDING NORMALLY. The one caller
	//! (OVT_InsertionSpawningDeploymentModule.ReleaseConvoy()) only reaches here when the transport
	//! itself failed IsTruckOperational() - a deployment collected with an intact vehicle has lost
	//! nothing and never calls this, even though its own teardown also runs ReleaseConvoy().
	//!
	//! TYPED AS CAMP, not a new enum member: OVT_TargetType has no "a vehicle died here" value and this
	//! feature does not add one (context.md explains why CAMP was the closest existing fit rather than
	//! widening the enum for one caller).
	//!
	//! DEDUPLICATED AGAINST THE NEAREST EXISTING TARGET, not an exact match: two losses along the same
	//! stretch of road, or a wreck already standing where a base's own known-target insert already
	//! covers, count as one incident rather than stacking the score at that spot.
	//! \param[in] position Where the vehicle was lost.
	void ReportVehicleLoss(vector position)
	{
		OVT_TargetData nearest = GetNearestKnownTarget(position);
		if (nearest && vector.Distance(nearest.location, position) < VEHICLE_LOSS_DEDUP_RADIUS_M)
			return;

		OVT_TargetData target = new OVT_TargetData();
		target.location = position;
		target.type = OVT_TargetType.CAMP;
		target.order = OVT_OrderType.ATTACK;
		m_aKnownTargets.Insert(target);

		Print(string.Format("[Overthrow] A vehicle was lost at %1 - the location is now a known target", position.ToString()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! THE HUNTER-KILLER DISPATCHER (T6.4). The deployment evaluator cannot be pointed at a hotspot at
	//! all (C5 - CollectSeedCandidates and FindDeploymentCandidates only ever produce NAMED-LOCATION
	//! positions: town / base / port / airfield / radio tower / checkpoint), so this is a deliberate
	//! caller, exactly like the QRF echelon (T4.3) and the base armour sortie (T5.4/SendSortie) before
	//! it - CREATE, THEN DEBIT, at this one call site, matching
	//! OVT_ObjectiveDirectorComponent.CreateObjectiveDeployment (:1225) and SendSortie's own choke point.
	//!
	//! FOUR REFUSAL GATES, cheapest first, mirroring CanSendObjectiveDeployment's own ordering rule:
	//!   1. ONE LIVE SWEEP AT A TIME - m_bHunterKillerActive, self-healing: a sweep that has already been
	//!      collected or deleted (the entity no longer resolves) clears the flag and this tick may still
	//!      send a new one.
	//!   2. A BATTLE IS ENGAGED (IsQRFEngaged()) - the same "we don't spend or drive while shots are
	//!      being fired over the objective" rule CheckUpdate()'s own early return already applies to
	//!      everything else this tick does.
	//!   3. THE BEST KNOWN TARGET'S SCORE IS UNDER THE FLOOR - GetThreatByLocation() asked of the
	//!      highest-scoring entry in m_aKnownTargets, never a fabricated "average" or a raw count.
	//!   4. THE DEPLOYMENT POOL CANNOT AFFORD THE CONFIG'S OWN TOTAL COST - read once, through
	//!      GetTotalResourceCost(), and reused as both the affordability question and the amount debited,
	//!      so the two numbers can never drift apart.
	//!
	//! ⚠ PUBLIC ONLY SO THAT THE INITIALISATION TIER CAN DRIVE ONE - the same note IsFightingFit and
	//! SendMountedEchelon already carry on the QRF controller. Nothing in the campaign calls this
	//! directly; production reaches it only from CheckUpdate()'s own 60 s tick.
	void TickHunterKiller()
	{
		if (m_bHunterKillerActive && IsHunterKillerStillLive())
			return;

		m_bHunterKillerActive = false;

		if (IsQRFEngaged())
			return;

		// GATE 2b: the sweep IS its vehicle. Below the bottom rung there is nothing to send.
		if (!CanFieldLadderVehicle())
			return;

		OVT_TargetData best = PickHunterKillerTarget();
		if (!best)
			return;

		int score = GetThreatByLocation(best.location);
		if (score < HUNTER_KILLER_THREAT_FLOOR)
			return;

		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (!deployments)
			return;

		OVT_DeploymentConfig config = deployments.FindConfigByName(HUNTER_KILLER_CONFIG_NAME);
		if (!config)
			return;

		OVT_OverthrowConfigComponent gameConfig = OVT_Global.GetConfig();
		if (!gameConfig)
			return;

		int factionIndex = gameConfig.GetOccupyingFactionIndex();
		int cost = config.GetTotalResourceCost();

		if (deployments.GetFactionResources(factionIndex) < cost)
			return;

		OVT_DeploymentComponent created = deployments.ForceCreateDeployment(config, best.location, factionIndex, cost);
		if (!created)
			return;

		// ⚠ THE LINE THE WHOLE ACCOUNTING IDENTITY RESTS ON (G5/Q5) - see the header. Debited only AFTER
		// a successful create, never before, so a refusal never burns the pool on nothing.
		deployments.SubtractFactionResources(factionIndex, cost);

		m_HunterKillerDeployment = created.GetOwner().GetID();
		m_bHunterKillerActive = true;

		Print(string.Format("[Overthrow] Hunter-killer sweep sent to %1 (score %2) for %3 resources",
			best.location.ToString(), score.ToString(), cost.ToString()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether the sweep this manager thinks is out there still resolves to a real entity - the
	//! self-healing half of gate 1. A sweep collected by its own behaviour module (T6.2) or torn down by
	//! anything else leaves nothing for FindEntityByID to find.
	protected bool IsHunterKillerStillLive()
	{
		if (!m_bHunterKillerActive)
			return false;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		return world.FindEntityByID(m_HunterKillerDeployment) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The known target with the highest GetThreatByLocation() score, or null when there are
	//! none. Ties break to the first found - m_aKnownTargets carries no authored order to break them by.
	protected OVT_TargetData PickHunterKillerTarget()
	{
		OVT_TargetData best;
		int bestScore = -1;

		foreach (OVT_TargetData target : m_aKnownTargets)
		{
			if (!target)
				continue;

			// A radio tower is not worth armour. It is undefended by design, the faction already has a
			// recapture path for it in the objective director, and sending a fighting vehicle at one
			// reads as nonsense to the player who owns it.
			if (target.type == OVT_TargetType.BROADCAST_TOWER)
				continue;

			int score = GetThreatByLocation(target.location);
			if (score > bestScore)
			{
				bestScore = score;
				best = target;
			}
		}

		return best;
	}

	void OnBaseControlChange(OVT_BaseControllerComponent base)
	{
		if(m_OnBaseControlChanged) m_OnBaseControlChanged.Invoke(base);
	}

	int GainResources()
	{
		Print("[Overthrow.OccupyingFactionManager] Gaining Resources");
		Print("[Overthrow.OccupyingFactionManager] Current Threat: " + m_iThreat.ToString());
		int numPlayersOnline = GetGame().GetPlayerManager().GetPlayerCount();

		// The arithmetic lives in OVT_GMSchedule so that a read-only consumer (the Game Master state
		// seam) can predict this tick's amount without calling this method, which accumulates it.
		int newResources = OVT_GMSchedule.PredictResourceGain(
			m_Config.m_Difficulty.baseResourcesPerTick,
			m_Config.m_Difficulty.resourcesPerTick,
			m_iThreat,
			numPlayersOnline);

		m_iResources += newResources;

		Print ("[Overthrow.OccupyingFactionManager] Gained Resources: " + newResources.ToString());

		// NOTHING IS ALLOCATED HERE ANY MORE. A conditional drip used to run from this line, topping the
		// deployment pool up only when it was under 500 AND the reserve was over 1000. That condition
		// existed for exactly one reason: to ARBITRATE between two spenders, the pool and the per-base
		// loop that bought base defense directly. There is one spender now, so the arbitration is gone
		// and CheckUpdate() transfers the defense share unconditionally.

		return newResources;
	}

	//------------------------------------------------------------------------------------------------
	//! DEBUG ENTRY POINT - the "/give-resources" admin chat command and nothing else.
	//!
	//! IT CREDITS THE RESERVE, NOT THE POOL. This is deliberately NOT a pool credit path: it adds to
	//! m_iResources exactly the way GainResources() does, and the resources reach the deployment pool
	//! by the only route they ever take - TransferDefenseShareToPool() on the next resource tick, which
	//! moves 80 % of the reserve across. AllocateDeploymentResources() still has three callers and must
	//! not gain a fourth (see its header); calling it from here would break the "resource accounting is
	//! closed" grep that the deployments feature is checked against.
	//!
	//! Server-only by contract - the caller (OVT_AdminCommandsComponent.RpcAsk_GiveResources) is the
	//! admin gate and the authority check; this method performs neither.
	//! \param[in] amount Resources to add to the reserve. A non-positive amount is a no-op.
	//! \return The reserve total after the credit.
	int DebugCreditReserve(int amount)
	{
		if (amount <= 0)
			return m_iResources;

		m_iResources += amount;

		Print(string.Format("[Overthrow.OccupyingFactionManager] DEBUG: credited %1 to the reserve, reserve is now %2 (reaches the deployment pool on the next resource tick)", amount, m_iResources), LogLevel.NORMAL);

		return m_iResources;
	}

	//------------------------------------------------------------------------------------------------
	//! THE SINGLE POINT AT WHICH THE OCCUPYING FACTION'S DEPLOYMENT POOL IS CREDITED. Three callers,
	//! and there must never be a fourth without a reason written down:
	//!   - SeedOpeningDeploymentResources()  the opening budget, once per new campaign;
	//!   - TransferDefenseShareToPool()      80 % of every resource tick;
	//!   - CreditPendingLegacyRefund()       the legacy base-upgrade refund, once per legacy save.
	//!
	//! Keeping it in one place is what makes "resource accounting is closed" checkable at all: a grep
	//! for AddFactionResources across the tree answers this method and the deployment framework's own
	//! refund path, and nothing else.
	//! \param[in] amount Resources to credit. A non-positive amount is a no-op at the manager.
	protected void AllocateDeploymentResources(int amount)
	{
		OVT_DeploymentManagerComponent deploymentManager = OVT_Global.GetDeploymentManager();
		if (!deploymentManager)
			return;

		int occupyingFactionIndex = OVT_Global.GetConfig().GetOccupyingFactionIndex();
		deploymentManager.AddFactionResources(occupyingFactionIndex, amount);

		Print(string.Format("[Overthrow.OccupyingFactionManager] Allocated %1 resources to deployment manager", amount));
	}

	void OnAIKilled(IEntity ai, IEntity instigator)
	{
		if(!Replication.IsServer()) return;

		m_iThreat += 5;

		m_OnAIKilled.Invoke(ai, instigator);
	}

	//RPC Methods

	override bool RplSave(ScriptBitWriter writer)
	{
		//Send JIP factions
		writer.WriteString(m_Config.m_sOccupyingFaction);
		writer.WriteString(m_Config.m_sPlayerFaction);

		//Send JIP bases
		writer.WriteInt(m_Bases.Count());
		for(int i=0; i<m_Bases.Count(); i++)
		{
			OVT_BaseData data = m_Bases[i];
			writer.WriteVector(data.location);
			writer.WriteInt(data.faction);
		}

		//Send JIP radio towers
		writer.WriteInt(m_RadioTowers.Count());
		for(int i=0; i<m_RadioTowers.Count(); i++)
		{
			OVT_RadioTowerData data = m_RadioTowers[i];
			writer.WriteVector(data.location);
			writer.WriteInt(data.faction);
			writer.WriteFloat(data.disabledRemaining);
		}

		writer.WriteVector(m_vQRFLocation);
		writer.WriteInt(m_iQRFPoints);
		writer.WriteInt(m_iQRFTimer);
		writer.WriteBool(m_bQRFActive);

		// ⚠ APPENDED, AND POSITIONAL LIKE EVERYTHING ELSE HERE. RplLoad reads these in the same order;
		// the two halves must be edited together or a joining client mis-parses the whole tail.
		//
		// ⚠ m_iCurrentQRFBase AND m_iCurrentQRFTown ARE **ALREADY** MISSING FROM THIS PAYLOAD, and they
		// stay missing: a client that joins mid-battle gets m_bQRFActive and m_vQRFLocation but neither
		// index, so the map's "don't draw the objective base's own restricted circle" rule reads -1
		// until the next RpcDo_SetQRFBase/Town, which never comes. That is a PRE-EXISTING defect, not
		// this feature's, and widening the payload contract beyond the one flag it needs is exactly the
		// sort of drive-by that makes a wire format unreviewable. Recorded rather than fixed.
		writer.WriteBool(m_bQRFRevealed);

		return true;
	}

	override bool RplLoad(ScriptBitReader reader)
	{
		int length;
		RplId id;

		if(!reader.ReadString(m_Config.m_sOccupyingFaction)) return false;
		if(!reader.ReadString(m_Config.m_sPlayerFaction)) return false;

		FactionManager fm = GetGame().GetFactionManager();
		m_Config.m_iOccupyingFactionIndex = fm.GetFactionIndex(fm.GetFactionByKey(m_Config.m_sOccupyingFaction));
		m_Config.m_iPlayerFactionIndex = fm.GetFactionIndex(fm.GetFactionByKey(m_Config.m_sPlayerFaction));

		//Recieve JIP bases
		if (!reader.ReadInt(length)) return false;
		for(int i=0; i<length; i++)
		{
			OVT_BaseData base = new OVT_BaseData();

			if (!reader.ReadVector(base.location)) return false;
			if (!reader.ReadInt(base.faction)) return false;

			base.id = i;
			m_Bases.Insert(base);
		}

		//Recieve JIP radio towers
		if (!reader.ReadInt(length)) return false;
		for(int i=0; i<length; i++)
		{
			OVT_RadioTowerData base = new OVT_RadioTowerData();

			if (!reader.ReadVector(base.location)) return false;
			if (!reader.ReadInt(base.faction)) return false;

			float disabledRemaining;
			if (!reader.ReadFloat(disabledRemaining)) return false;
			base.SetDisabledRemaining(disabledRemaining);

			base.id = i;
			m_RadioTowers.Insert(base);
		}
		if (!reader.ReadVector(m_vQRFLocation)) return false;
		if (!reader.ReadInt(m_iQRFPoints)) return false;
		if (!reader.ReadInt(m_iQRFTimer)) return false;
		if (!reader.ReadBool(m_bQRFActive)) return false;
		// Appended with the matching write in RplSave - see the note there.
		if (!reader.ReadBool(m_bQRFRevealed)) return false;

		return true;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetQRFActive(vector pos)
	{
		m_vQRFLocation = pos;
		m_bQRFActive = true;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetQRFBase(int base)
	{
		m_iCurrentQRFBase = base;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetQRFTown(int townId)
	{
		m_iCurrentQRFTown = townId;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetQRFPoints(int points)
	{
		m_iQRFPoints = points;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetBaseFaction(int index, int faction)
	{
		m_Bases[index].faction = faction;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetRadioTowerFaction(vector pos, int faction)
	{
		SetRadioTowerFaction(pos, faction);
	}

	void SetRadioTowerFaction(vector pos, int faction)
	{
		OVT_RadioTowerData tower = GetNearestRadioTower(pos);
		if(!tower) return;

		tower.faction = faction;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetQRFTimer(int timer)
	{
		m_iQRFTimer = timer;
	}

	//------------------------------------------------------------------------------------------------
	//! Publishes m_bQRFRevealed - whether the resistance has been told about the current battle.
	//!
	//! ⚠ A NEW PAIR, NOT A WIDENED ONE, and its arity was DIFFED BY EYE against RpcDo_SetQRFTimer
	//! immediately above. Rpc() is an untyped variadic prototype: a wrong argument count compiles
	//! perfectly cleanly and then dies silently at the wire (BUG-090), so the only check that exists is
	//! this one. `Rpc(RpcDo_SetQRFRevealed, revealed)` is ONE payload argument and
	//! `RpcDo_SetQRFRevealed(bool)` takes ONE. There are three send sites: both battle starters and
	//! RevealQRF.
	//! \param[in] revealed Whether the client should show the battle.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetQRFRevealed(bool revealed)
	{
		m_bQRFRevealed = revealed;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetQRFInactive()
	{
		m_bQRFActive = false;
		m_bQRFRevealed = false;
		m_iCurrentQRFBase = -1;
		m_iCurrentQRFTown = -1;
	}

}