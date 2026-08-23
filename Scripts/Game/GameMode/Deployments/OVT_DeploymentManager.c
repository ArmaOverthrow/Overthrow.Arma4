// Helper class for sorting candidate positions by threat level
//
//! ⚠ The two floats are NOT interchangeable, and the split is load-bearing. The constructor seeds
//! both from the same number, so a candidate nobody biases behaves as it always has - but they answer
//! two different questions and only ONE may ever be biased:
//!
//!   sortBy      ORDERING ONLY. The [SortAttribute()] field the evaluator's Sort(true) reads, and
//!               nothing else reads it at all.
//!   threatLevel ELIGIBILITY AND THE RECORD. Passed to FindBestDeploymentConfig(), where
//!               m_iMinimumThreatLevel is a hard gate three shipped configs author above zero, and
//!               then stamped onto the created deployment and persisted. Raising it would make
//!               configs BUYABLE that are not.
//!
//! See ApplyObjectiveAnchorBias() for the one place that touches either of them after construction.
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

//------------------------------------------------------------------------------------------------
//! One faction's objective bias: a place that faction wants routine work bought NEAR, and how hard.
//!
//! PUSHED IN, NEVER PULLED OUT. Whatever owns a faction's long-term intent sets this and clears it;
//! this manager never asks anybody whether an objective exists. That keeps the dependency pointing
//! one way, keeps the hottest loop free of a null-check on a component that may not be present, and
//! keeps the manager testable with nothing else running.
//!
//! Absent is the default and means "as before". An entry only ever exists while a radius and a weight
//! are both positive - see SetObjectiveAnchor(), which refuses to store a dead one.
//------------------------------------------------------------------------------------------------
class OVT_DeploymentObjectiveAnchor : Managed
{
	//! Where the objective is.
	vector position;

	//! How far the bias reaches. It falls off linearly to nothing here.
	float radius;

	//! The most the bias may add to a candidate's sort key, at the anchor itself.
	float weight;
}

//------------------------------------------------------------------------------------------------
//! One faction's objective RESERVE FLOOR: how much of its pool the routine evaluator may not touch,
//! and what that money is being kept for.
//!
//! 🔴 THE PRIORITY INVERSION (D18). The occupying faction's pool is credited in a LUMP every six
//! in-game hours and two spenders race for it: the routine evaluator runs every 30 real seconds and
//! may create MAX_DEPLOYMENTS_PER_EVALUATION deployments in ONE pass, while the objective director
//! takes at most one operation per cadence interval. With nothing earmarked, routine garrisoning
//! drained each credit before the pool ever reached the price of the strategic operation.
//!
//! ⚠ It GATES spending. It moves NO money. There is still exactly one pool, one credit path and one
//! debit per create; a reserve is a CEILING ON ONE SPENDER for as long as somebody holds it up.
//!
//! PUSHED IN, NEVER PULLED OUT, exactly as OVT_DeploymentObjectiveAnchor is.
//!
//! ⚠ One operation deep, and it must not outlive the intent. The pusher re-pushes on every one of its
//! own ticks for as long as it is genuinely short and drops it the moment it is not, so an entry here
//! is at most one tick old. It is not a growing war chest and nothing accumulates in it.
//------------------------------------------------------------------------------------------------
class OVT_DeploymentObjectiveReserve : Managed
{
	//! What the money is being kept for. DIAGNOSTICS ONLY - nothing branches on it. It is in the
	//! record because "the evaluator was held off" is useless in a log without "for what".
	string operation;

	//! How much is withheld from the routine evaluator. Always positive: SetObjectiveReserve() clears
	//! rather than storing a dead one, so an entry in the store is always a live floor.
	int cost;

	//! True once this exact (operation, cost) earmark has been said out loud. The latch that keeps a
	//! floor re-pushed every ten real seconds from being a log line every ten real seconds; see
	//! OVT_DeploymentSelection.IsSameReserve() for why the key is the pair.
	bool announced;
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

	//! How many live insertion convoys ONE faction may have on the roads at once.
	//!
	//! ⚠ A cost CEILING, not a feature gate. A refused reservation never means "no reinforcement": the
	//! insertion module walks its force in instead. So 0 is a legitimate authored value - "this server
	//! never wants a live convoy" - and it degrades to everybody walking rather than nobody arriving.
	//!
	//! The authored default is the class default; PostGameStart() overwrites it from the difficulty
	//! preset, which is where the value that actually runs comes from.
	[Attribute(defvalue: "2", desc: "Maximum live insertion convoys per faction. Overwritten from the difficulty preset at game start. 0 disables live insertion - every force then walks in")]
	int m_iMaxConcurrentInsertions;

	//! THE DAYLIGHT WINDOW. Forces do not muster in the dark: outside these hours the 30 s evaluator
	//! buys nothing, and the pool it would have spent simply accumulates until morning.
	//!
	//! ⚠ WHAT IS **NOT** GATED, and none of it by omission:
	//!   - CAMPAIGN-START SEEDING (SeedFreeConfigs). A campaign that opens at 22:00 must not open on an
	//!     empty map, and baseline presence is not something the faction "decides" to do at all;
	//!   - THE OBJECTIVE DIRECTOR'S OPERATIONS. They are ForceCreateDeployment callers and never touch
	//!     the evaluator; the counter-attack already carries its own, narrower daylight condition;
	//!   - REINFORCEMENT REBUYS. A garrison that loses men at 02:00 still tops itself up, because that
	//!     is an existing force reacting rather than a new one being raised.
	//!
	//! The bounds are read by OVT_ObjectivePhaseRules.IsCounterAttackWindow(), which is reused rather
	//! than reimplemented: it already honours a window that crosses midnight and already treats an
	//! out-of-range bound or a zero-width window as NO RESTRICTION rather than as "never".
	[Attribute(defvalue: "5", desc: "First hour (0-23) of the day the evaluator may buy deployments, INCLUSIVE. A bound outside 0-23, or a start equal to the end, means no restriction at all")]
	int m_iDaylightStartHour;

	[Attribute(defvalue: "17", desc: "First hour (0-23) the evaluator may no longer buy deployments, EXCLUSIVE - so 17 means the last purchase of the day is at 16:59. Set equal to the start hour to switch the window off")]
	int m_iDaylightEndHour;

	//! The world clock, resolved on first use. Cached because this is asked once per evaluator pass.
	protected TimeAndWeatherManagerEntity m_DaylightClock;

	//! Latches the "the faction is not raising anything until morning" line to once per nightfall.
	protected bool m_bNightHoldLogged;

	//! "<factionIndex>|<config name>" -> the in-game minute the last one was created at. See
	//! ResolveInGameMinute() for why the stamp is an INT of minutes and not a float of hours.
	protected ref map<string, int> m_mConfigCooldownStamp;

	protected ref array<ref EntityID> m_aActiveDeployments;
	protected ref map<int, ref array<ref EntityID>> m_mFactionDeployments; // factionIndex -> deployments
	protected ref map<int, int> m_mFactionResources; // factionIndex -> available resources
	protected ref array<vector> m_aAvailableSlots; // Cached slot positions
	protected bool m_bInitialized;

	//! factionIndex -> that faction's objective bias. An absent entry is the normal state and means
	//! "not biased".
	//!
	//! ⚠ Runtime-only and deliberately not persisted. The bias is a restatement of an intent that lives
	//! somewhere else and is itself saved; re-reading it here would give two owners to one fact.
	//! Whoever set it pushes it again after a load, which is what makes the store safe to empty
	//! wholesale on a restore.
	protected ref map<int, ref OVT_DeploymentObjectiveAnchor> m_mObjectiveAnchors;

	//! factionIndex -> that faction's objective reserve floor. An absent entry is the normal state and
	//! means "nothing is earmarked".
	//!
	//! ⚠ Runtime-only, for a stronger reason than the anchor's. A reserve is a statement about ONE
	//! TICK - "the thing that owns this faction's intent asked to buy something on its last tick and
	//! could not pay for it". A saved one would be a floor asserted by a director that has not run yet,
	//! held against an operation a load may abandon on its first tick, and nothing would clear it.
	//! Re-earning it costs at most one unfloored evaluation pass.
	protected ref map<int, ref OVT_DeploymentObjectiveReserve> m_mObjectiveReserves;

	//! factionIndex -> how many live insertion convoys that faction currently holds.
	//!
	//! ⚠ Runtime-only, and A LEAK HERE PERMANENTLY STARVES INSERTION. Nothing in this map survives a
	//! save because nothing it counts survives one - a convoy is a truck, a crew and a set of
	//! waypoints, none of which is persisted. A count left behind by a convoy that ended without
	//! releasing is a reservation nobody can ever hand back, and once the count reaches the cap that
	//! faction never drives anywhere again for the rest of the campaign.
	protected ref map<int, int> m_mInsertionReservations;

	//! True once this manager has subscribed to the virtualization registry's events. Guards against
	//! a second subscription: a ScriptInvoker has no Contains(), so an Insert() that ran twice would
	//! fan every restore and every wipe out over the whole deployment list twice.
	protected bool m_bVirtualizationHooked;

	//! Handles this manager has parked on the engine's Manual lifecycle policy for the duration of a
	//! battle - see TickBattleSuppression() and OVT_DeploymentBattleSuppression.
	//!
	//! ⚠ Runtime-only, for the same reason no QRF state is persisted. A pin is a live SetLifecyclePolicy
	//! call on a group ENTITY and nothing else - it never touches m_iSpawnDistanceOverride, which IS in
	//! the save payload - so a campaign saved mid-battle comes back with every group on the ring its
	//! config authored.
	//!
	//! ⚠ An entry left here that is never released is a garrison that never materialises again for the
	//! rest of the campaign, with no log line and no symptom but empty ground. That is why the tick
	//! reconciles this list unconditionally on every pass instead of doing its release work from a
	//! battle-ended callback.
	protected ref array<int> m_aBattleSuppressedHandles;

	static const float THREAT_EVALUATION_RADIUS = 2000; // 2km
	static const int MAX_DEPLOYMENTS_PER_EVALUATION = 10; // Maximum deployments per evaluation cycle

	//! THE LOCAL-THREAT FLOOR FOR ROUTINE SPENDING: below this, a place is not worth fortifying and the
	//! faction keeps its money instead.
	//!
	//! ⚠ On the SPATIAL scale, which is roughly 0-60 - see CalculateThreatLevel(). A number that looks
	//! tiny beside the old ~420 candidate scores is a meaningful fraction of the new ones.
	//!
	//! 5 is deliberately low: it excludes ground with NOTHING near it (0 to 4, because there is no
	//! known enemy target within a kilometre and no town within three times its range) while leaving
	//! anywhere with a town or a known target able to develop. A higher floor would start refusing to
	//! garrison real places early in a campaign, when every score is naturally small - a much worse
	//! failure, because an undefended map is not recoverable by waiting.
	//!
	//! ⚠ Raise only against play-test evidence, and expect the effect to be non-linear: the town terms
	//! scale with town SIZE, so raising the floor prunes villages long before it prunes cities.
	static const float MIN_LOCAL_THREAT_TO_DEPLOY = 5;

	//! How far apart two BASELINE deployments of the same config must be, in metres.
	//!
	//! ⚠ Wider than the 250 m per-concern dedup on purpose, and only applied to m_bFreeAtGameStart
	//! configs. 250 m is right for escalation - a base fortifies by acquiring several DIFFERENT configs
	//! within a few hundred metres. It is wrong for baseline presence, which is supposed to exist
	//! exactly ONCE per place: a base position inside a town's bounds classifies as BASE|TOWN and
	//! satisfies Town Patrol's TOWN mask while standing far enough from the town centre that a 250 m
	//! check cannot see the patrol already there.
	//!
	//! 1000 m is HasDeploymentNearPosition()'s own default radius rather than a new number.
	static const float BASELINE_DEDUP_RADIUS = 1000;

	//! ⚠ There is no longer a blanket minimum distance between deployments. The 100 m veto that used to
	//! live here made a place that already held one deployment invisible to every other config, and a
	//! base fortifies by acquiring several DIFFERENT deployments at one place. The one anti-stack rule
	//! left is the name-scoped 250 m dedup in HasExistingDeploymentOfType().

	//! How close a position has to be to a radio tower to count as being AT one. Read by both halves of
	//! the location classification, which is the point of it being a constant: two copies of 300 that
	//! drifted apart would classify the same spot two different ways depending on which was asked.
	static const float RADIO_TOWER_RADIUS = 300;

	//! How close a position has to be to a base centre for the BASE bit to be OR-ed into its
	//! classification (see GetLocationTypeAtPosition).
	//!
	//! ⚠ Deliberately EQUAL to HasExistingDeploymentOfType()'s default 250 m dedup radius, and that
	//! equality is the whole safety argument for OR-ing the bit in at all. Any position that gains the
	//! BASE bit is within 250 m of the base centre, so the name-scoped dedup already sees the base's own
	//! copy of every base config and refuses a second one - force doubling is geometrically impossible
	//! rather than merely unlikely. If this ever grows past the dedup radius, a base and a nearby
	//! position could each buy their own copy of every base-defense config.
	static const float BASE_CLASSIFICATION_RADIUS = 250;

	//! How often the battle-suppression reconciliation runs, in milliseconds.
	//!
	//! 1 000 ms MATCHES THE ENGINE'S OWN LIFECYCLE CADENCE (SCR_AIGroup.m_fLifecycleTickSeconds drives
	//! LifecycleTick from EOnFrame at 1 Hz) and the battle controller's own tick, so a group cannot
	//! come up more than one lifecycle tick before this sees it. It is not a hot loop: the tick returns
	//! on its first line whenever there is no battle and nothing is pinned, which is almost always.
	static const int BATTLE_SUPPRESSION_INTERVAL_MS = 1000;

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

		// Allocated with the rest of the per-faction state, and therefore EMPTY for every campaign
		// this component is built for. A second campaign in one client session gets a second
		// component and so a second, empty store.
		m_mObjectiveAnchors = new map<int, ref OVT_DeploymentObjectiveAnchor>;

		// Same reasoning, and empty is the only safe starting state: an entry here holds the routine
		// evaluator off, and a fresh component has never been asked to hold anybody off by anybody.
		m_mObjectiveReserves = new map<int, ref OVT_DeploymentObjectiveReserve>;

		// Same reasoning, and the same lifetime: a second campaign in one client session gets a second
		// component and so an empty reservation count, which is the only thing that could otherwise
		// carry a dead convoy's claim across a restart.
		m_mInsertionReservations = new map<int, int>;

		// Empty for every campaign, and empty is the only safe starting state: an entry here means
		// "this manager owes that group a SetLifecyclePolicy(ProximityDriven)", which a fresh component
		// owes nobody.
		m_aBattleSuppressedHandles = new array<int>();

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

		// The difficulty preset is not readable at OnPostInit - the config component may not have been
		// initialised yet - and it never changes mid-campaign, so once here is exactly right.
		ApplyDifficultyInsertionCap();

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

		// Battle suppression. A SECOND, MUCH FASTER TICK ON PURPOSE: the 30 s evaluation is a decision
		// about spending, and this is a reconciliation against a fact that changes in seconds (a battle
		// starting, a battle ending, a dormant group being walked into range by the movement tick). It
		// cannot ride on the evaluation, which also returns early during a battle - which is exactly
		// when this has the most to do.
		GetGame().GetCallqueue().CallLater(TickBattleSuppression, BATTLE_SUPPRESSION_INTERVAL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	// Virtualization
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Subscribes THIS manager - and nothing else in the deployments framework - to the virtualization
	//! registry's restore and wipe events.
	//!
	//! ⚠ The manager subscribes; the modules never do (D7). Deployment modules are cloned config
	//! objects with no stable identity, destroyed whenever their deployment is deleted - and one of
	//! them deletes its own deployment from inside its own update. A ScriptInvoker holding a method
	//! pointer into one of those is a dangling call waiting to happen.
	//!
	//! IDEMPOTENT, because a second campaign in the same session must not end up double-subscribed.
	//! Safe to run before the virtualization manager's own PostGameStart(): the invokers are created on
	//! first use by their getters and are never replaced.
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
		// ⚠ The battle-suppression list is DROPPED, not released. A restore re-creates the group ENTITIES
		// from core's own payload, so every entity this manager had parked on the Manual policy is gone
		// and its replacement was freshly stamped ProximityDriven. KEEPING the handles would be actively
		// wrong, because the tick treats a listed handle as "already pinned" and would never pin the
		// replacement.
		if (m_aBattleSuppressedHandles)
			m_aBattleSuppressedHandles.Clear();

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
	// Battle suppression
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Keeps this framework's dormant forces out of a battle that is already being fought, and lets
	//! them back in when it is over. See OVT_DeploymentBattleSuppression for the rule.
	//!
	//! It SUPPRESSES MATERIALISATION of registered groups that are NOT UP. It does NOT despawn groups
	//! already standing: SetLifecyclePolicy(Manual) changes who drives the group's dormant/active
	//! transitions and despawns nothing, so a garrison that was on the ground when the battle started
	//! fights the battle it is in. The contested base is emptied by the player, as it always was; it
	//! simply stops being refilled.
	//!
	//! Why the policy and not ForceDespawn(): force is a nudge and not a pin - the engine's spawn queue
	//! re-validates proximity at dispatch and the group's own 1 Hz tick undoes the decision a second
	//! later - so a despawn-on-sight loop would flicker for the whole battle.
	//!
	//! Two independent layers, both already in the engine/core, and the second is what makes this safe:
	//!   1. Manual policy clears EntityEvent.FRAME, so LifecycleTick() - the only thing that turns
	//!      observer proximity into a RequestSpawn - stops being called at all;
	//!   2. a spawn request that was ALREADY QUEUED when the policy flipped is refused at DISPATCH by
	//!      Modded/SCR_AIGroup.RefusesUnrequestedManualSpawn(), which books the request complete so the
	//!      dispatcher drops it instead of retrying forever.
	//!
	//! ⚠ No distances are ever passed to SetLifecyclePolicy, in either direction, and that is the whole
	//! restore strategy. Negative/omitted distances leave the group's bands untouched, so the release
	//! call puts back EXACTLY what core stamped at registration. Passing numbers here would mean owning
	//! a copy of the core's despawn hysteresis, which is protected with no getter.
	//!
	//! ⚠ The release pass runs FIRST and UNCONDITIONALLY - not behind "was there a battle", not behind a
	//! latch, not behind a battle-ended event. One missed release is a force that never comes back for
	//! the rest of the campaign.
	protected void TickBattleSuppression()
	{
		if (!m_bInitialized || !Replication.IsServer())
			return;

		if (!m_aBattleSuppressedHandles)
			return;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		// Resolve the battle ONCE per pass rather than per handle. All three reads are server-side
		// truth; a client never gets here because of the guard above.
		bool engaged = false;
		vector battleLocation = vector.Zero;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (occupying)
		{
			engaged = occupying.IsQRFEngaged();
			battleLocation = occupying.m_vQRFLocation;
		}

		string occupyingFactionKey;
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (config)
			occupyingFactionKey = config.m_sOccupyingFaction;

		// THE CHEAP EXIT, and the reason a 1 Hz tick costs nothing. No battle and nothing owed means
		// there is provably no work: the release pass has nothing to iterate and the suppress pass is
		// gated on `engaged`.
		if (!engaged && m_aBattleSuppressedHandles.IsEmpty())
			return;

		ReleaseFinishedBattleSuppression(virtualization, engaged, occupyingFactionKey, battleLocation);

		if (!engaged)
			return;

		SuppressForcesAroundBattle(virtualization, occupyingFactionKey, battleLocation);
	}

	//------------------------------------------------------------------------------------------------
	//! Puts back every group this manager pinned that no longer qualifies for suppression.
	//!
	//! A handle stops qualifying when the battle ends, when the battle moves (a second battle in a
	//! different place), when the group is walked out of range, when the record is gone, or when the
	//! occupying faction key can no longer be resolved. All of those are the same answer - release -
	//! which is why this asks the SAME predicate the suppress pass asks rather than a mirror of it.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] engaged Whether a battle is being fought right now.
	//! \param[in] occupyingFactionKey The occupying faction's key; may be empty.
	//! \param[in] battleLocation Where the battle is; meaningless when engaged is false.
	protected void ReleaseFinishedBattleSuppression(notnull OVT_VirtualizationManagerComponent virtualization, bool engaged, string occupyingFactionKey, vector battleLocation)
	{
		for (int i = m_aBattleSuppressedHandles.Count() - 1; i >= 0; i--)
		{
			int handle = m_aBattleSuppressedHandles[i];

			if (virtualization.IsRegistered(handle) && StillSuppressed(virtualization, handle, engaged, occupyingFactionKey, battleLocation))
				continue;

			ReleaseBattleSuppression(virtualization, handle);
			m_aBattleSuppressedHandles.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Does a handle this manager has already pinned still qualify?
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] handle A registered handle.
	//! \param[in] engaged Whether a battle is being fought right now.
	//! \param[in] occupyingFactionKey The occupying faction's key; may be empty.
	//! \param[in] battleLocation Where the battle is.
	//! \return True while the pin must stay on.
	protected bool StillSuppressed(notnull OVT_VirtualizationManagerComponent virtualization, int handle, bool engaged, string occupyingFactionKey, vector battleLocation)
	{
		OVT_VirtualGroupRecord record = virtualization.GetRecord(handle);
		if (!record)
			return false;

		return OVT_DeploymentBattleSuppression.SuppressesMaterialisation(engaged, occupyingFactionKey, record.m_sFactionKey,
			battleLocation, virtualization.GetPosition(handle));
	}

	//------------------------------------------------------------------------------------------------
	//! Pins every not-yet-materialised occupying-faction deployment group inside the battle circle.
	//!
	//! ⚠ Scoped by OWNER SYSTEM, not by walking the deployment list, and that is a correctness choice.
	//! CollectRegisteredHandles() is overridden by the infantry module ONLY - the vehicle module's crews
	//! are registered under the same owner system and would be invisible to a deployment walk, so a
	//! base's armour crews would keep materialising mid-battle while its garrison did not.
	//! FindGroupsBySystem(OWNER_SYSTEM) sees everything this framework has ever registered.
	//!
	//! ⚠ It also cannot see the battle's own troops, which is the point. A QRF's groups are spawned
	//! straight from a prefab and are never registered with the virtualization core at all, so they are
	//! structurally unreachable from here.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] occupyingFactionKey The occupying faction's key; may be empty.
	//! \param[in] battleLocation Where the battle is.
	protected void SuppressForcesAroundBattle(notnull OVT_VirtualizationManagerComponent virtualization, string occupyingFactionKey, vector battleLocation)
	{
		if (occupyingFactionKey.IsEmpty())
			return;

		array<int> handles = virtualization.FindGroupsBySystem(OVT_BaseSpawningDeploymentModule.OWNER_SYSTEM);
		foreach (int handle : handles)
		{
			if (m_aBattleSuppressedHandles.Contains(handle))
				continue;

			if (!StillSuppressed(virtualization, handle, true, occupyingFactionKey, battleLocation))
				continue;

			// ALREADY STANDING MEANS LEFT ALONE. This is the scope line of the whole change: a group
			// with members in the world was there before the battle reached it and is fighting it, and
			// taking it away would be a despawn with survivor-mask implications rather than a
			// suppression. Note the tick re-asks every second, so a group that later despawns of its
			// own accord IS pinned from then on - "not up" is a live reading, not a snapshot.
			if (virtualization.IsSpawned(handle))
				continue;

			// ⚠ Two registrations this must never touch, both identified by their resolved ring:
			//   0 or less - the record already means "never materialise by proximity", so it is not ours
			//     and releasing it later would hand it a proximity lifecycle it was registered without;
			//   STRICTLY WIDER than the world's ring - the insertion module's riding passengers and crew,
			//     which exist precisely so they can be seated in a truck driving through empty country.
			//     Pinning those arrives the convoy empty.
			// ⚠ STRICTLY wider, not "at least as wide": every ordinary deployment group registers with
			// SPAWN_DISTANCE_GLOBAL, which RESOLVES TO the global ring, so `>=` would suppress nothing.
			int spawnDistance = virtualization.GetSpawnDistance(handle);
			if (spawnDistance <= 0 || spawnDistance > virtualization.GetGlobalSpawnDistance())
				continue;

			if (!ApplyBattleSuppression(virtualization, handle))
				continue;

			m_aBattleSuppressedHandles.Insert(handle);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Parks one group on the engine's Manual lifecycle policy.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] handle The group's handle.
	//! \return True when the pin was applied and is this manager's to release.
	protected bool ApplyBattleSuppression(notnull OVT_VirtualizationManagerComponent virtualization, int handle)
	{
		SCR_AIGroup group = virtualization.GetGroup(handle);
		if (!group)
			return false;

		// Somebody else already has it on Manual - the record's own spawnDistanceOverride of 0, or a
		// module mid-transition. Not ours to pin and therefore not ours to release.
		if (group.GetLifecyclePolicy() == SCR_EAIGroupLifecyclePolicy.Manual)
			return false;

		group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Hands one group back to the engine's proximity lifecycle.
	//!
	//! The transition back re-seeds the group's own anti-pop-in state (any transition INTO
	//! ProximityDriven samples HasObserverInRange), so a garrison released while a player is standing
	//! on top of it is treated as a gradual approach rather than popping in beside them.
	//!
	//! ⚠ Which means a released garrison does NOT come back while the player is still on it - vanilla's
	//! m_fVeryNearBlockDistance is 150 m. That is the desired behaviour, but it means "the garrison is
	//! still missing" is only evidence of a missed release when observed from more than 150 m away.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] handle The group's handle. A handle whose group is gone is a no-op.
	protected void ReleaseBattleSuppression(notnull OVT_VirtualizationManagerComponent virtualization, int handle)
	{
		SCR_AIGroup group = virtualization.GetGroup(handle);
		if (!group)
			return;

		if (group.GetLifecyclePolicy() != SCR_EAIGroupLifecyclePolicy.Manual)
			return;

		group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.ProximityDriven);
	}

	//------------------------------------------------------------------------------------------------
	//! Would a force of `factionIndex` at `position` be suppressed by a battle right now?
	//!
	//! The public, one-shot form of the rule the tick reconciles with, for callers about to CREATE a
	//! force rather than maintain one - the reinforcement rebuy is the only one today. A rebuy near a
	//! live battle is refused before it spends anything, instead of buying a force this manager then
	//! immediately pins: a purchase that materialises nobody is BUG-027's shape.
	//!
	//! ⚠ Takes a faction INDEX because a deployment holds one. The rule itself is stated on keys, so the
	//! conversion happens here, once, against the config's own occupying key.
	//! \param[in] position Where the force would be.
	//! \param[in] factionIndex The faction that would own it.
	//! \return True when nothing new may be put on the ground there yet.
	bool IsBattleSuppressedAt(vector position, int factionIndex)
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return false;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return false;

		int occupyingIndex = config.GetOccupyingFactionIndex();
		if (occupyingIndex < 0 || factionIndex != occupyingIndex)
			return false;

		// Both halves of the faction test have now been answered by the index compare above, so the key
		// is handed in on both sides deliberately: the ONE rule stays in the ONE place, and this method
		// never grows a second copy of the range or engagement logic.
		return OVT_DeploymentBattleSuppression.SuppressesMaterialisation(occupying.IsQRFEngaged(),
			config.m_sOccupyingFaction, config.m_sOccupyingFaction, occupying.m_vQRFLocation, position);
	}

	//------------------------------------------------------------------------------------------------
	//! The ordinal a deployment should disambiguate its base key with, so that two deployments on one
	//! rounded spot never share one force.
	//!
	//! ⚠ Probed, not counted. A counter would have to survive a save: a restored deployment carries its
	//! persisted key without ever asking for an ordinal, so a session-local counter would hand a NEW
	//! deployment on that spot the key the restored one is already using. Probing is self-healing
	//! across loads, deletions and re-applies, and a derivation happens once per deployment.
	//!
	//! Counting starts at 2 so the suffix reads as "the second one here"; ordinal 0 is the first holder
	//! and carries no suffix at all.
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
	//! Radio tower garrisons and town patrols became ordinary deployments bought out of the faction
	//! resource pool (D17), and on Easy that pool starts at 150 per tick while a Tower Garrison costs
	//! 50 - so a play-test found most towers ungarrisoned. A garrison is not an opportunistic purchase,
	//! it is the baseline state of the world.
	//!
	//! ⚠ No player-count guard and no QRF guard, deliberately, and this is the one place in the
	//! framework where that is true. Those early returns are DECISIONS about spending a budget on new
	//! forces; this is the world's opening state, and it must be on the ground before the first player
	//! joins a dedicated server. Seeding also charges nothing, so a QRF's claim on the pool is
	//! untouched.
	//!
	//! ⚠ Runs AFTER persistence restore, always. A save is deserialized synchronously during load, long
	//! before this fires at +9 s, so every restored deployment is already visible to the dedup below.
	//! That turns this from "seed the world" into "fill in what is genuinely missing".
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

			// BELT AND BRACES, and deliberately not relying on the two flags never being authored
			// together. "A director-only config is created by the director and by nothing else" is an
			// invariant of this framework, not a property of today's .conf files.
			if (!config.IsSelectableByEvaluator())
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
	//! Deliberately NOT consulted, because baseline presence is the point:
	//!   - the faction's resource pool - nothing is charged, and the deployment is stamped with 0
	//!     invested resources so collecting it later refunds nothing;
	//!   - m_fChance - a garrison that exists 70% of the time is not a baseline;
	//!   - m_iMinimumThreatLevel - threat measures what has already happened, and at t0 nothing has.
	//!     The config's own condition MODULES are still asked, because those answer "does this place
	//!     belong to this faction";
	//!   - MAX_DEPLOYMENTS_PER_EVALUATION - that cap paces an ongoing spend, and there is nothing here
	//!     to pace.
	//!
	//! What IS: the same-name 250 m dedup, m_iMaxInstances, and the manager's per-faction ceiling.
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

			// resourcesInvested 0 IS LOAD-BEARING: a deployment nobody paid for must refund nothing when
			// it is collected, and must not show up in the GM panel as money spent.
			//
			// ⚠ And so is the seededAtGameStart flag, which cannot be inferred from anything else here.
			// It is NOT config.m_bFreeAtGameStart - that is a property of the CONFIG, and the same config
			// is bought again by the evaluator later in the campaign when the force SHOULD come from a
			// base. Nor is it "resourcesInvested == 0" - Town Patrol authors m_iCostPerGroup 0.
			OVT_DeploymentComponent deployment = CreateDeployment(config, position, factionIndex, 0, threatLevel, 0, true);
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
	//! right for the opportunistic evaluator, wrong here: it would offer a tower position to the town
	//! patrol config (a tower inside a town's bounds classifies as both), seeding a second patrol on
	//! top of the tower.
	//!
	//! ⚠ A config that authors NO location types seeds nothing. CanUseLocationType() reads that as "no
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
	//! The 30 s evaluation.
	//!
	//! Both early returns below are deliberately kept. This framework migrated group LIFECYCLE, not
	//! deployment decision-making: with nobody connected there is no one for a new deployment to be
	//! created for, and during a QRF the occupying faction's resources belong to the QRF. Existing
	//! deployments' groups live entirely on the engine's lifecycle and are unaffected by either.
	//!
	//! ⚠ True everywhere EXCEPT at the battle itself. Inside QRF_RANGE of an ENGAGED battle, this
	//! framework's occupying-faction groups are held dormant until it is over - that is
	//! TickBattleSuppression()'s job, not this method's. The guard below still only ever blocks
	//! CREATION.
	void EvaluateDeployments()
	{
		if (!m_bInitialized || !Replication.IsServer())
			return;

		//Don't create deployments if all players are offline
		PlayerManager mgr = GetGame().GetPlayerManager();
		if(mgr.GetPlayerCount() == 0)
			return;

		//Don't create deployments during a QRF
		//
		// ⚠ ENGAGED, not merely existing (D15). A counter-attack siege is a battle object for up to 31
		// minutes before a shot is fired; freezing every deployment on the map for half an hour before
		// the resistance is told anything would be a dead world and a tell besides. A standard,
		// player-initiated battle is engaged from creation.
		if(OVT_Global.GetOccupyingFaction().IsQRFEngaged())
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
	//! ONE faction's routine spending for one pass.
	//!
	//! 🔴 The one spender the objective reserve floor governs, and the only one (D18). Everything else
	//! that takes money out of the pool goes past this method and is deliberately unaffected: the
	//! director's own operations, the forward base's dismantle penalty, an existing deployment's
	//! reinforcement, and the free-at-game-start seeding pass, which charges nothing at all. Routine
	//! garrisoning outbidding the campaign's strategic intent is the whole of the defect.
	protected void EvaluateFactionDeployments(int factionIndex)
	{
		array<ref EntityID> factionDeployments = EnsureFactionDeploymentList(factionIndex);

		// Skip if faction has reached deployment limit
		if (factionDeployments.Count() >= m_iMaxDeploymentsPerFaction)
			return;

		// ⚠ TWO NUMBERS NOW, AND THEY ARE NOT INTERCHANGEABLE. `pool` is what the faction HOLDS and is
		// the only thing ever written back to m_mFactionResources; `availableResources` is what this
		// pass may SPEND. They are seeded from the same integer and are decremented in lockstep, so a
		// faction nobody earmarks for behaves exactly as it always has - see SpendableResources(),
		// which is the identity on a zero reserve and performs no arithmetic at all.
		int pool = m_mFactionResources.Get(factionIndex);

		int reserve = GetObjectiveReserveCost(factionIndex);
		int availableResources = OVT_DeploymentSelection.SpendableResources(pool, reserve);

		// Said once per (operation, price), never per pass. Nothing branches on it.
		AnnounceObjectiveReserve(factionIndex, pool, availableResources);

		// Find potential deployment locations
		array<vector> candidatePositions = FindDeploymentCandidates(factionIndex);

		// This faction's objective bias, resolved ONCE for the whole pass rather than per candidate:
		// one map lookup, and null for every faction that has no objective - which is every faction in
		// every world that never starts one. See ApplyObjectiveAnchorBias() for the two invariants.
		OVT_DeploymentObjectiveAnchor objectiveAnchor = GetObjectiveAnchor(factionIndex);

		// Calculate threat levels for all candidates and add randomness
		array<ref OVT_CandidatePosition> candidatesWithThreat = new array<ref OVT_CandidatePosition>;
		foreach (vector position : candidatePositions)
		{
			float baseThreatLevel = CalculateThreatLevel(position, factionIndex);
			// Add randomness: ±20% of base threat level
			float randomModifier = s_AIRandomGenerator.RandFloatXY(-0.2, 0.2);
			float finalThreatLevel = baseThreatLevel * (1.0 + randomModifier);

			OVT_CandidatePosition candidate = new OVT_CandidatePosition(position, finalThreatLevel);
			ApplyObjectiveAnchorBias(candidate, objectiveAnchor);
			candidatesWithThreat.Insert(candidate);
		}

		// Sort candidates by threat level (highest first). Reads OVT_CandidatePosition.sortBy, which is
		// the jittered threat unless this faction is biased toward an objective - and everything below
		// still reads candidate.threatLevel, which is never biased.
		candidatesWithThreat.Sort(true);
		
		int numDeployments = 0;

		// Evaluate each candidate position in order of threat level
		foreach (OVT_CandidatePosition candidate : candidatesWithThreat)
		{
			// Find suitable deployment config for this position and threat level
			OVT_DeploymentConfig bestConfig = FindBestDeploymentConfig(candidate.position, factionIndex, candidate.threatLevel, availableResources);
			if (bestConfig)
			{
				// 🔴 THE SPEND FLOOR: a quiet place is not worth escalating at, and saving the money is a
				// better answer than spending it badly.
				//
				// 🔴 But it EXEMPTS baseline presence, and that exemption is a bug fix. A config marked
				// m_bFreeAtGameStart is not escalation - it is what a place this faction holds is supposed
				// to look like, and SeedFreeConfig() already refuses to gate it on threat.
				//
				// Without the exemption the two paths disagreed and the gap was permanent. Seeding is a
				// ONE-SHOT that legitimately skips places (a base a player is standing in is refused by
				// Base Garrison Patrol's own condition module). Before the floor, the evaluator quietly
				// repaired those gaps on a later pass; after it, a quiet base scored under 5 and the
				// evaluator refused too, so a garrison missed once was missed FOREVER - the play-test had
				// ZERO Base Garrison Patrols, 26 of 39 candidates floored every pass, and 6871 idle
				// resources.
				//
				// ⚠ The floor is therefore asked AFTER the config is chosen, not before: "is this worth
				// spending on" now depends on WHAT would be bought.
				//
				// ⚠ It reads candidate.threatLevel, the UNBIASED score, and that is load-bearing. The
				// objective anchor writes only to sortBy (D5: it biases ORDERING, never ELIGIBILITY), so an
				// objective cannot make a dead corner of the map worth garrisoning.
				if (!bestConfig.m_bFreeAtGameStart && candidate.threatLevel < MIN_LOCAL_THREAT_TO_DEPLOY)
				{
					continue;
				}

				// 🔴 And a baseline exemption REPAIRS what is missing, never DUPLICATES what is there. The
				// exemption's first play-test showed the other half of the coin: the evaluator's first
				// pass, one second after seeding, bought four more Town Patrols, two more Tower Garrisons
				// and two more Base Garrison Patrols on top of the twenty just placed - including a THIRD
				// patrol for one town.
				//
				// ⚠ The standard 250 m dedup had already said yes, and for a town-scoped config it can be
				// wrong by construction: a BASE position inside a town's bounds classifies as BASE|TOWN,
				// satisfies Town Patrol's TOWN mask, and sits far enough from the town centre that a 250 m
				// radius cannot see the patrol already there. Widening the radius for THIS path only
				// closes that gap without loosening the per-concern dedup the escalation model depends on.
				//
				// ⚠ Deliberately asymmetric: escalation still uses the 250 m rule; only baseline presence,
				// which is supposed to exist exactly once per place, is held to the wider one.
				if (bestConfig.m_bFreeAtGameStart && HasExistingDeploymentOfType(candidate.position, factionIndex, bestConfig.m_sDeploymentName, BASELINE_DEDUP_RADIUS))
					continue;

				// Redundant since the escalation change, and kept on purpose. FindBestDeploymentConfig now
				// applies this same dedup per config before it picks, so this is a cheap belt-and-braces
				// guard on the one invariant the whole framework rests on - never two of the same config
				// in one place.
				// ⚠ It must NOT go back to being a `continue` on the POSITION rather than on the config:
				// that is precisely what stopped a place ever acquiring more than its single best config.
				if (HasExistingDeploymentOfType(candidate.position, factionIndex, bestConfig.m_sDeploymentName))
				{
					continue; // Skip this position, deployment already exists nearby
				}

				// ⚠ PRICED FOR THIS PLACE, NOT FROM THE TEMPLATE. A composition the base has no free slot
				// for is dropped from the bill here rather than bought and then silently not built - see
				// OVT_BaseDeploymentModule.GetResourceCostAt(). The figure handed to CreateDeployment is
				// the one actually debited, so the deployment's own record of what it cost stays honest.
				int deploymentCost = bestConfig.GetTotalResourceCost(1, candidate.position);
				if (availableResources >= deploymentCost)
				{
					CreateDeployment(bestConfig, candidate.position, factionIndex, deploymentCost, candidate.threatLevel);
					availableResources -= deploymentCost;

					// ⚠ The POOL is what is debited and the POOL is what is written back. Writing the
					// SPENDABLE figure here would silently DESTROY the reserve on the first purchase of the
					// pass - the faction would lose the earmarked resources outright, which is a hole in the
					// "every resource leaves the pool exactly once" identity (G5). With no reserve the two
					// decrement in lockstep from the same starting integer.
					pool -= deploymentCost;
					m_mFactionResources.Set(factionIndex, pool);
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
			// ⚠ A DIRECTOR-ONLY CONFIG MUST NOT EVEN WIDEN THE CANDIDATE SEARCH. Its location mask is a
			// statement about where the DIRECTOR sends it, not a request for this pass to go and find
			// places of that kind. See OVT_DeploymentConfig.IsSelectableByEvaluator().
			if (!config.IsSelectableByEvaluator())
				continue;

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
			if (!baseData)
				continue;

			// 🔴 A base this faction does not hold is not a place for it to build.
			// IsPositionRelevantToFaction() answers TRUE unconditionally for the occupying faction, so
			// without this line every base on the map - including every one the resistance has captured -
			// was offered to the evaluator as somewhere to buy a garrison.
			//
			// ⚠ It was latent until the threat rescale. While every candidate scored the global threat
			// plus noise, an enemy base was no more attractive than anywhere else and rarely won the sort;
			// now a captured base is worth THREAT_WEIGHT_ENEMY_BASE to the spatial score, so it is
			// SYSTEMATICALLY among the top candidates.
			//
			// ⚠ The director is unaffected and must be: its operations against enemy bases are created
			// through ForceCreateDeployment(), which never asks for a candidate list.
			if (baseData.faction != factionIndex)
				continue;

			if (!IsPositionRelevantToFaction(baseData.location, factionIndex))
				continue;

			positions.Insert(baseData.location);
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
	//! Whether a candidate position can carry a deployment at all.
	//!
	//! A GROUND CHECK and nothing else, deliberately. It used to veto any position within 100 m of ANY
	//! existing deployment, which made a place that already had one deployment permanently invisible to
	//! every other config: a base could buy its highest-priority config and nothing else, ever.
	//!
	//! What prevents stacking now is the NAME-SCOPED 250 m dedup, HasExistingDeploymentOfType(). Two
	//! deployments of DIFFERENT configs may share a position; two of the SAME config still may not.
	//! FindBestDeploymentConfig() applies that dedup per config BEFORE it compares priorities, which is
	//! what turns "one config per place" into "the next config this place is still missing".
	//! \param[in] position The candidate position.
	//! \param[in] factionIndex The faction being deployed for. Unused: suitability is a property of
	//!            the ground, and every faction-scoped rule lives in the config filter instead.
	//! \return True when there is ground under the position.
	protected bool IsPositionSuitableForDeployment(vector position, int factionIndex)
	{
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
	//! HOW MUCH THIS PARTICULAR PLACE NEEDS DEFENDING. Spatial, and ONLY spatial.
	//!
	//! 🔴 It used to ADD the global campaign threat to this, and that made the evaluator pick places at
	//! random. The arithmetic, with figures from the campaign that exposed it:
	//!   - GetThreatLevel() is the GLOBAL escalation scalar - the same number at every position on the
	//!     map. It was ~420.
	//!   - GetThreatByLocation() is the only term that knows where you are, and it is SMALL by
	//!     construction: an enemy base within 1 km is worth 10, a tower or FOB 5, a warehouse 1, a town
	//!     up to 5 x its size per component. Realistic spread: 0-60.
	//!   - The caller then jitters the TOTAL by +/-20%, which at 420 is +/-84.
	//! So the ordering was a constant, plus noise nearly twice the size of the entire signal, plus the
	//! signal.
	//!
	//! It also silently killed every authored m_iMinimumThreatLevel: with 420 added to every position,
	//! "only deploy where local threat exceeds 50" passed everywhere, permanently.
	//!
	//! ⚠ The global scalar is not lost, it is back where it belongs - PredictResourceGain() multiplies
	//! income by it, which is what an escalation scalar should do. Deciding WHERE to spend is a
	//! different question and it was never qualified to answer it.
	//! \param[in] position The candidate position.
	//! \param[in] factionIndex The faction being deployed for. Unused: threat is a property of the
	//!            place, and the occupying faction is the only one that scores places.
	//! \return The spatial threat score, 0 for a position with nothing of interest near it.
	protected float CalculateThreatLevel(vector position, int factionIndex)
	{
		OVT_OccupyingFactionManager ofManager = OVT_Global.GetOccupyingFaction();
		if (!ofManager)
			return 0;

		return ofManager.GetThreatByLocation(position);
	}
	
	//------------------------------------------------------------------------------------------------
	//! The config this position should buy NEXT, or null when there is nothing left for it to buy.
	//!
	//! THE ESCALATION CONTRACT. m_iPriority is no longer only a tie-break - it is the ORDER OF
	//! ACQUISITION AT ONE PLACE. Every evaluation pass asks again, and it answers with the
	//! LOWEST-PRIORITY config the position is still MISSING, so a place fortifies one concern at a
	//! time over successive passes:
	//!
	//!     pass 1 at a base:  missing everything          -> Garrison Patrol   (priority 1)
	//!     pass 2 at a base:  Garrison Patrol is here     -> Defense Positions (priority 2)
	//!     pass 3 at a base:  both are here               -> Tower Guards      (priority 2, order)
	//!     ...                                            -> Parked Vehicles   (priority 10)
	//!     when nothing is missing                        -> null
	//!
	//! ...each still gated by cost, m_iMinimumThreatLevel, m_iMaxInstances, m_fChance and the config's
	//! own condition modules, and paced map-wide by MAX_DEPLOYMENTS_PER_EVALUATION. Ties resolve to
	//! REGISTRY ORDER and the loser is picked up on the next pass rather than being shut out forever.
	//!
	//! ⚠ "Missing" means the name-scoped 250 m dedup, applied here PER CONFIG BEFORE the priority
	//! comparison instead of once after it. Doing it after was the whole reason a place could only ever
	//! hold its single best config: the caller got one answer, found it already present, and skipped
	//! the position entirely.
	//!
	//! The pick itself is delegated to OVT_DeploymentSelection, pure arithmetic over names and
	//! priorities with no world in it, so the contract is assertable in the cheapest test tier.
	//!
	//! ⚠ PUBLIC so the Init tier can assert the contract as a live claim rather than as an inspection
	//! of this diff. Nothing outside the evaluator and that case calls it.
	//! \param[in] position The candidate position.
	//! \param[in] factionIndex The faction being deployed for.
	//! \param[in] threatLevel The candidate's scored threat.
	//! \param[in] availableResources That faction's deployment pool.
	//! \return The next config to create here, or null.
	//------------------------------------------------------------------------------------------------
	//! THE PER-CONFIG COOLDOWN. \return True when this config was created too recently to create again.
	//!
	//! 🔴 WHAT IT IS FOR. Play-test, 2026-08-23: *"the OF was relentlessly throwing TowerRecaptureUnrest
	//! specops at a single tower which was pretty annoying."* `m_iMaxInstances 1` only stops TWO existing
	//! at once - the moment the last specops team dies the deployment is deleted and the evaluator buys
	//! another on its next pass, which reads as spamming rather than responding.
	//!
	//! ⚠ PER FACTION AS WELL AS PER CONFIG. Two factions running the same config are two different
	//! stories and neither should silence the other.
	//! \param[in] config The config being considered.
	//! \param[in] factionIndex The faction that would own it.
	//! \return True when it must wait.
	bool IsConfigOnCooldown(notnull OVT_DeploymentConfig config, int factionIndex)
	{
		if (config.m_fCooldownHours <= 0)
			return false;

		if (!m_mConfigCooldownStamp)
			return false;

		int last;
		if (!m_mConfigCooldownStamp.Find(CooldownKey(config, factionIndex), last))
			return false;

		int now = ResolveInGameMinute();
		if (now < 0)
			return false;

		// A stamp in the future means the clock went backwards (a load into an earlier save). Treat it as
		// expired rather than locking the config out for the rest of the campaign.
		if (now < last)
			return false;

		return (now - last) < Math.Round(config.m_fCooldownHours * 60);
	}

	//------------------------------------------------------------------------------------------------
	//! Records that one was just created, so the next is held off.
	//! \param[in] config The config that was created.
	//! \param[in] factionIndex The faction that owns it.
	protected void StampConfigCooldown(notnull OVT_DeploymentConfig config, int factionIndex)
	{
		if (config.m_fCooldownHours <= 0)
			return;

		if (!m_mConfigCooldownStamp)
			m_mConfigCooldownStamp = new map<string, int>();

		int now = ResolveInGameMinute();
		if (now < 0)
			return;

		m_mConfigCooldownStamp.Set(CooldownKey(config, factionIndex), now);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] config The config.
	//! \param[in] factionIndex The faction.
	//! \return The map key for this (faction, config) pair.
	protected string CooldownKey(notnull OVT_DeploymentConfig config, int factionIndex)
	{
		return factionIndex.ToString() + "|" + config.m_sDeploymentName;
	}

	//------------------------------------------------------------------------------------------------
	//! THE CAMPAIGN CLOCK AS A MONOTONIC INTEGER NUMBER OF MINUTES.
	//!
	//! ⚠ AN INT OF MINUTES, NEVER A FLOAT OF HOURS, AND THAT IS NOT STYLE. EnforceScript `float` is IEEE
	//! binary32: an absolute time measured from year zero has a ~2-hour quantum by the present day, so a
	//! six-hour cooldown expressed as a float difference of two absolute stamps is unusable - a delta of
	//! twelve minutes reads as 0.0. Minutes as an int stay exact: the present day lands near 1.1e9, well
	//! inside int range.
	//!
	//! ⚠ THE DAY INDEX IS APPROXIMATE AND ONLY HAS TO BE MONOTONIC. 31 days per month and 12 months per
	//! year over-counts, which shifts every stamp by the same amount and therefore cancels in the
	//! subtraction. It must never go BACKWARDS, which is why month and year are included at all.
	//! \return Minutes, or -1 when there is no clock to read.
	protected int ResolveInGameMinute()
	{
		if (!m_DaylightClock)
		{
			ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
			if (world)
				m_DaylightClock = world.GetTimeAndWeatherManager();
		}

		if (!m_DaylightClock)
			return -1;

		TimeContainer time = m_DaylightClock.GetTime();
		if (!time)
			return -1;

		int year, month, day;
		m_DaylightClock.GetDate(year, month, day);

		int dayIndex = ((year * 12) + month) * 31 + day;

		return ((dayIndex * 24) + time.m_iHours) * 60 + time.m_iMinutes;
	}

	//------------------------------------------------------------------------------------------------
	//! IS THE EVALUATOR ALLOWED TO RAISE ANYTHING RIGHT NOW?
	//!
	//! ⚠ INCOME IS NOT GATED, ONLY SPENDING. The deployment pool keeps filling all night - the author's
	//! own framing, 2026-08-23: *"its ok if they accrue deployment pool etc in this time"* - so a night
	//! costs the faction nothing but timing, and dawn is when it spends what it banked.
	//!
	//! ⚠ FAILS OPEN ON A MISSING CLOCK, exactly as OVT_DaylightWindowObjectiveCondition does. A world
	//! with no time manager is a test world, and failing closed there would silently stop every
	//! evaluator purchase in the whole Init tier with nothing logged.
	//! \return True when deployments may be bought.
	bool IsInDaylightWindow()
	{
		if (!m_DaylightClock)
		{
			ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
			if (world)
				m_DaylightClock = world.GetTimeAndWeatherManager();
		}

		if (!m_DaylightClock)
			return true;

		TimeContainer time = m_DaylightClock.GetTime();
		if (!time)
			return true;

		bool open = OVT_ObjectivePhaseRules.IsCounterAttackWindow(time.m_iHours, m_iDaylightStartHour, m_iDaylightEndHour);

		if (open)
		{
			m_bNightHoldLogged = false;
			return true;
		}

		if (!m_bNightHoldLogged)
		{
			m_bNightHoldLogged = true;

			Print(string.Format("[Overthrow] Deployments: it is %1:00, outside the %2:00-%3:00 window - nothing new is raised until morning, and the pool keeps filling meanwhile",
				time.m_iHours.ToString(), m_iDaylightStartHour.ToString(), m_iDaylightEndHour.ToString()), LogLevel.NORMAL);
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	OVT_DeploymentConfig FindBestDeploymentConfig(vector position, int factionIndex, float threatLevel, int availableResources)
	{
		array<OVT_DeploymentConfig> suitableConfigs = new array<OVT_DeploymentConfig>;

		// Read ONCE for the whole pass rather than per config: it is a clock read and every config in
		// the registry would otherwise ask the same question the same number of times.
		bool inDaylight = IsInDaylightWindow();

		// Parallel to suitableConfigs, for the world-free selection below.
		array<string> suitableNames = new array<string>;
		array<int> suitablePriorities = new array<int>;

		// Which of those this position ALREADY holds. Collected rather than `continue`d over so the
		// selection stays one pure function of what was found, and so a log line can tell "nothing
		// suitable here" apart from "everything suitable is already here".
		array<string> alreadyHere = new array<string>;

		// Filter configs by conditions and resources
		foreach (OVT_DeploymentConfig config : m_DeploymentRegistry.m_aDeploymentConfigs)
		{
			if (!config.IsValidConfig())
				continue;

			// 🔴 The objective director's own operations are NOT candidates. They are registered here so
			// everything which reads a CONFIG keeps working, not so this pass can buy them: a play-test
			// found the evaluator sending an "Objective Harassment (Patrol)" of its own accord at a base
			// objective, outside the director's accounting and with no objective behind it. The director
			// reaches them through FindConfigByName() + ForceCreateDeployment(), which ignore this flag.
			if (!config.IsSelectableByEvaluator())
				continue;

			// Forces are not raised in the dark. Asked per config rather than as one early return on the
			// whole pass, so a config can author its way out of the window - see m_bIgnoreDaylightWindow.
			if (!inDaylight && !config.m_bIgnoreDaylightWindow)
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
			
			// ⚠ A BASE THAT CANNOT TAKE ANY OF THIS CONFIG'S STRUCTURES IS NOT A CANDIDATE FOR IT. This
			// sits beside the location-type test above because it is the same kind of question - "is this
			// config suitable HERE" - and it is deliberately silent: some bases simply have no slots, and
			// that is ordinary rather than a fault. See OVT_DeploymentConfig.CanPlaceCompositionsAt().
			if (!config.CanPlaceCompositionsAt(position))
			{
				continue;
			}

			// Check resource cost, priced for THIS position so an unplaceable composition is never part
			// of the affordability question either.
			int cost = config.GetTotalResourceCost(1, position);
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
					suitableNames.Insert(config.m_sDeploymentName);
					suitablePriorities.Insert(config.m_iPriority);

					// THE ESCALATION FILTER. Asked per config, before anything compares priorities.
					if (HasExistingDeploymentOfType(position, factionIndex, config.m_sDeploymentName))
						alreadyHere.Insert(config.m_sDeploymentName);
				}
			}
		}

		// Lowest priority among the ones this position does NOT already hold; ties to registry order.
		int pick = OVT_DeploymentSelection.SelectNextConfigIndex(suitableNames, suitablePriorities, alreadyHere);
		if (pick == OVT_DeploymentSelection.NOTHING_TO_BUY)
			return null;

		return suitableConfigs[pick];
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
	//! ONE FLAG, PLUS RADIO_TOWER AND/OR BASE OR-ED IN. The classification is a precedence chain
	//! returning the FIRST kind it matches - town, base, port, airfield, tower, checkpoint - so a radio
	//! tower inside a town's bounds classified as TOWN and could never be offered to a tower-only
	//! config. Every radio tower on Eden is near something, so Tower Garrison would never have fired.
	//!
	//! The fix is the smallest one that works: compute the single value EXACTLY as before, then OR the
	//! bit in on top. Nothing moves in the precedence, so no existing config's acceptance changes.
	//!
	//! The BASE bit is OR-ed for the same class of reason: towns are tested before bases against a
	//! hardcoded 500 m radius, so a base whose centre falls inside a town's bounds could be offered NO
	//! base-only config - 4 of Eden's 10 bases plus the Init test world's only base. The system this
	//! replaces never asked what kind of place a base was, so leaving it would have been a regression
	//! on ~40 % of the map's bases.
	//!
	//! ⚠ The 250 m radius is the SAFETY ARGUMENT, not a tuning knob: it is deliberately equal to
	//! HasExistingDeploymentOfType()'s dedup radius, so any position that gains the BASE bit is within
	//! the dedup's reach of the base's own copy of every base config. OR-ing at the precedence chain's
	//! own 500 m was rejected because at Levie the town centre is 460 m from the base, far enough for
	//! the dedup to miss and for the base and the town to each buy a full set of base defense.
	//! \param[in] position The candidate position.
	//! \return The location kind, with RADIO_TOWER and/or BASE OR-ed in when one is within range.
	OVT_LocationTypeFlag GetLocationTypeAtPosition(vector position)
	{
		OVT_LocationTypeFlag locationType = GetPrimaryLocationTypeAtPosition(position);

		if (IsNearRadioTower(position))
			locationType = locationType | OVT_LocationTypeFlag.RADIO_TOWER;

		if (IsNearBaseCentre(position))
			locationType = locationType | OVT_LocationTypeFlag.BASE;

		return locationType;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a tracked base centre stands within BASE_CLASSIFICATION_RADIUS of a position.
	//!
	//! Deliberately NOT the 500 m the precedence chain's own base branch uses: this one feeds the
	//! OR-in, where the radius must stay inside the deployment dedup radius to stop a base and a
	//! neighbouring candidate each buying their own defense. The two numbers answer two different
	//! questions and are meant to differ.
	//! \param[in] position The position to test.
	//! \return True when the nearest base centre is inside BASE_CLASSIFICATION_RADIUS.
	protected bool IsNearBaseCentre(vector position)
	{
		OVT_OccupyingFactionManager ofManager = OVT_Global.GetOccupyingFaction();
		if (!ofManager)
			return false;

		OVT_BaseData baseData = ofManager.GetNearestBase(position);
		if (!baseData)
			return false;

		return vector.Distance(position, baseData.location) < BASE_CLASSIFICATION_RADIUS;
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
	//!
	//! ⚠ The marker carries a HEADING as well as a position, and it is unrotated by default. Almost
	//! every deployment is a point on the map with no front. The one caller that passes a yaw is the
	//! objective director's forward-base operation, because the thing that deployment raises is a
	//! STRUCTURE with a front, and the marker's transform is the only channel between "the director
	//! chose a facing" and "a spawning module builds something" that cannot go stale.
	//!
	//! ⚠ Not persisted separately, and it does not need to be: the marker entity itself is tracked, so
	//! vanilla persistence saves the transform it was spawned with.
	//! \param[in] config Deployment config to run.
	//! \param[in] position World position for the marker entity.
	//! \param[in] factionIndex Owning faction.
	//! \param[in] resourcesInvested Resources spent creating it (stamped so RecoverResources and the GM snapshot are non-zero).
	//! \param[in] threatLevel Threat level the candidate position scored at.
	//! \param[in] yaw Heading for the marker, in degrees. 0 leaves it unrotated.
	//! \param[in] seededAtGameStart True only for the free-at-game-start pass. See
	//!            OVT_DeploymentComponent.WasSeededAtGameStart() for what reads it and why.
	OVT_DeploymentComponent CreateDeployment(OVT_DeploymentConfig config, vector position, int factionIndex, int resourcesInvested = 0, float threatLevel = 0, float yaw = 0, bool seededAtGameStart = false)
	{
		if (config)
			Print(string.Format("[Overthrow] Creating deployment '%1' for faction %2", config.m_sDeploymentName, factionIndex), LogLevel.NORMAL);
		
		if (!config || !config.IsValidConfig())
			return null;

		// ⚠ HERE RATHER THAN IN THE EVALUATOR, so a deliberate ForceCreateDeployment caller is held to
		// the same tempo as the 30 s pass. A config that authors no cooldown never reaches the map
		// lookup. ⚠ EXEMPT AT CAMPAIGN START: the free-at-game-start pass seeds many instances of the
		// same config in one go, and a cooldown would let through the first and silently drop the rest.
		if (!seededAtGameStart && IsConfigOnCooldown(config, factionIndex))
		{
			Print(string.Format("[Overthrow] Deployment '%1' is on cooldown for faction %2 (%3 in-game hours between them) - not creating another yet",
				config.m_sDeploymentName, factionIndex.ToString(), config.m_fCooldownHours.ToString()), LogLevel.NORMAL);

			return null;
		}

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
		
		// Create transform matrix. The heading goes in HERE, in the spawn transform, and is never applied
		// afterwards - OVT_BaseSpawningDeploymentModule.SpawnEntity records why in full, and
		// GetUprightSpawnRotation() is the one place the (yaw, pitch, roll) conversion lives. A yaw of 0
		// produces the identity this used to build unconditionally.
		vector mat[4];
		Math3D.AnglesToMatrix(OVT_BaseSpawningDeploymentModule.GetUprightSpawnRotation(yaw), mat);
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

		// ⚠ SET BEFORE InitializeDeployment, NOT AFTER, and the ordering is the whole reason this is a
		// parameter rather than a call the seeding pass makes on the returned component. Initialisation
		// is what builds and activates the modules, and a spawning module reads this on its very first
		// convergence; stamping it afterwards would be one pass too late for exactly the deployments it
		// exists to describe.
		deployment.SetSeededAtGameStart(seededAtGameStart);

		deployment.InitializeDeployment(config, factionIndex);

		// Stamp what this deployment cost and the threat it answered. InitializeDeployment does not
		// write either, so without this RecoverResources() refunds 0 and the GM snapshot reads 0.
		deployment.SetResourcesInvested(resourcesInvested);
		deployment.SetThreatLevel(threatLevel);

		// ⚠ ON SUCCESS ONLY, AND AT THE END. Stamping beside the refusal check would start the clock on
		// a creation that then failed for one of the six reasons above, and the faction would sit out a
		// whole cooldown for a deployment that never existed. Seeded instances are exempt both ways -
		// they never stamp, so the first evaluator purchase after game start is not held off by them.
		if (!seededAtGameStart)
			StampConfigCooldown(config, factionIndex);

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

		// THE FACTION-LIST TEARDOWN. The one place the whole per-faction picture is emptied and rebuilt,
		// so the one place a bias belonging to the campaign that just ended could otherwise survive into
		// the campaign being loaded. Whoever owns the intent pushes its anchor again on its own first
		// tick after a load, so at most one evaluation pass runs unbiased in between.
		ClearAllObjectiveAnchors();

		// And the same teardown for the RESERVE FLOOR, for a stronger reason than the anchor's: a stale
		// anchor biases spending at a place that no longer matters, a stale floor STOPS spending. Nothing
		// here needs its own re-push either - a floor is a restatement of "the owner asked and could not
		// pay ON ITS LAST TICK", and the first tick after a load either re-asks and re-pushes or does not
		// want it any more.
		ClearAllObjectiveReserves();

		// AND THE SAME TEARDOWN FOR THE CONVOY COUNT, for a stronger reason than the anchor's. An
		// anchor left behind biases spending at a place that no longer matters; a reservation left
		// behind is a permanent debit against a cap, and enough of them stop the faction driving
		// anywhere ever again. Nothing a reservation counts survives a load - no truck, no crew, no
		// waypoints - so there is nothing here to preserve and everything to lose by trying.
		ResetInsertionReservations();
	}

	//------------------------------------------------------------------------------------------------
	// Live insertion convoys - a per-faction concurrency cap
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Takes the difficulty preset's convoy cap, replacing the authored class default.
	//!
	//! Fails soft in both directions: no config and no preset leave the authored value alone, and a
	//! preset that authors nothing (Difficulty_TestWorld authors none of the objective fields) reads
	//! the attribute's own defvalue from OVT_DifficultySettings rather than a zero.
	protected void ApplyDifficultyInsertionCap()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config || !config.m_Difficulty)
			return;

		int cap = config.m_Difficulty.objectiveMaxConcurrentInsertions;
		if (cap < 0)
			cap = 0;

		m_iMaxConcurrentInsertions = cap;
	}

	//------------------------------------------------------------------------------------------------
	//! Claims one of a faction's live-convoy slots.
	//!
	//! ⚠ A refusal is not a failure and the caller must not treat it as one. It means "not by truck",
	//! and the insertion module's answer is to walk the force in. Nothing about this cap may ever cause
	//! a force not to exist.
	//!
	//! Every successful call must be paired with exactly one ReleaseInsertion(), on every path out of
	//! the convoy including cleanup and abandonment.
	//! \param[in] factionIndex The faction that wants to drive.
	//! \return True when a slot was claimed. False when the cap is reached, or is zero, or there is no
	//!         store (a client).
	bool TryReserveInsertion(int factionIndex)
	{
		// Server-only component: the collections are not allocated on clients.
		if (!m_mInsertionReservations)
			return false;

		if (m_iMaxConcurrentInsertions <= 0)
			return false;

		int held = m_mInsertionReservations.Get(factionIndex);
		if (held < 0)
			held = 0;

		if (held >= m_iMaxConcurrentInsertions)
			return false;

		m_mInsertionReservations.Set(factionIndex, held + 1);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Hands one live-convoy slot back.
	//!
	//! IDEMPOTENT DOWNWARDS AND FLOORED AT ZERO. A release from a path that never reserved must be
	//! harmless, because the alternative is a caller that has to remember whether it reserved. ⚠ Going
	//! negative would be worse than leaking: it would silently RAISE the cap for the next convoy.
	//! \param[in] factionIndex The faction whose convoy has ended.
	void ReleaseInsertion(int factionIndex)
	{
		if (!m_mInsertionReservations)
			return;

		int held = m_mInsertionReservations.Get(factionIndex);
		if (held <= 1)
		{
			m_mInsertionReservations.Remove(factionIndex);
			return;
		}

		m_mInsertionReservations.Set(factionIndex, held - 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Forgets every faction's convoy count. See the call in ApplyPersistedFactionResources().
	void ResetInsertionReservations()
	{
		if (!m_mInsertionReservations)
			return;

		m_mInsertionReservations.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] factionIndex The faction to look up.
	//! \return How many live convoys that faction currently holds; 0 when it holds none.
	int GetInsertionReservations(int factionIndex)
	{
		if (!m_mInsertionReservations)
			return 0;

		return m_mInsertionReservations.Get(factionIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The per-faction live-convoy cap in force right now.
	int GetMaxConcurrentInsertions()
	{
		return m_iMaxConcurrentInsertions;
	}

	//------------------------------------------------------------------------------------------------
	//! Overrides the cap.
	//!
	//! ⚠ FOR TESTS AND FOR AN OPERATOR TOOL, NOT FOR GAMEPLAY. The value that runs comes from the
	//! difficulty preset at game start; anything that changes it mid-campaign is changing a tuning knob
	//! under a running system, and a cap lowered below the count already held simply refuses new
	//! convoys until the live ones finish rather than cancelling any of them.
	//! \param[in] cap The new cap. Negative is clamped to 0.
	void SetMaxConcurrentInsertions(int cap)
	{
		if (cap < 0)
			cap = 0;

		m_iMaxConcurrentInsertions = cap;
	}

	//------------------------------------------------------------------------------------------------
	// The objective anchor - a pushed, per-faction ORDERING bias
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Points a faction's routine deployment spending at a place.
	//!
	//! A non-positive radius or weight CLEARS instead of storing, so an entry in the store is always a
	//! live bias and "is there an anchor?" is the whole test at the call site.
	//!
	//! IDEMPOTENT. Setting the same faction twice replaces the anchor rather than stacking a second
	//! one: a faction has at most one objective, so it has at most one bias.
	//! \param[in] factionIndex The faction whose spending is being biased.
	//! \param[in] position Where the objective is.
	//! \param[in] radius How far the bias reaches. Non-positive clears.
	//! \param[in] weight The most it may add to a candidate's sort key. Non-positive clears.
	void SetObjectiveAnchor(int factionIndex, vector position, float radius, float weight)
	{
		// Server-only component: the collections are not allocated on clients.
		if (!m_mObjectiveAnchors)
			return;

		if (radius <= 0 || weight <= 0)
		{
			ClearObjectiveAnchor(factionIndex);
			return;
		}

		OVT_DeploymentObjectiveAnchor anchor = new OVT_DeploymentObjectiveAnchor();
		anchor.position = position;
		anchor.radius = radius;
		anchor.weight = weight;

		m_mObjectiveAnchors.Set(factionIndex, anchor);
	}

	//------------------------------------------------------------------------------------------------
	//! Stops biasing a faction's routine deployment spending.
	//!
	//! Safe on a faction that was never biased. Every path that ends an objective has to reach this,
	//! or the evaluator would keep leaning on a place nobody is working toward any more.
	//! \param[in] factionIndex The faction to stop biasing.
	void ClearObjectiveAnchor(int factionIndex)
	{
		if (!m_mObjectiveAnchors)
			return;

		m_mObjectiveAnchors.Remove(factionIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops every faction's bias at once. See the call in ApplyPersistedFactionResources().
	void ClearAllObjectiveAnchors()
	{
		if (!m_mObjectiveAnchors)
			return;

		m_mObjectiveAnchors.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! A faction's objective bias.
	//!
	//! ⚠ PUBLIC so the initialisation tier can assert the absent default as a live claim - "no faction
	//! in this world is biased, so the evaluator is taking the path it took before the bias existed"
	//! is a statement a case can make, and an inspection of a diff is not.
	//! \param[in] factionIndex The faction to look up.
	//! \return The anchor, or null when that faction is not biased - which is the normal state.
	OVT_DeploymentObjectiveAnchor GetObjectiveAnchor(int factionIndex)
	{
		if (!m_mObjectiveAnchors)
			return null;

		return m_mObjectiveAnchors.Get(factionIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! Folds a faction's objective bias into ONE candidate's sort key.
	//!
	//! THE TWO INVARIANTS:
	//!  1. NO ANCHOR IS BYTE-IDENTICAL TO TODAY. A null anchor returns before touching the candidate,
	//!     so every faction with no objective sorts on exactly the number the constructor put in
	//!     sortBy - not a re-derived one, not a rounded one, the same float. No arithmetic is
	//!     performed, so there is no floating-point difference to argue about.
	//!  2. THE ANCHOR BIASES ORDERING, NEVER ELIGIBILITY. It writes sortBy and ONLY sortBy. It never
	//!     touches threatLevel, which is the field handed to FindBestDeploymentConfig() and therefore
	//!     to the m_iMinimumThreatLevel gate, and which is then stamped onto the deployment and saved.
	//!     Biasing that instead would make an objective-adjacent position able to BUY configs a
	//!     position of its real threat cannot, and would write an inflated threat into the save. A
	//!     saturated map still buys nothing; it just buys near the objective first.
	//!
	//! The arithmetic itself is a pure function of four floats in OVT_ObjectiveSelection, pinned in the
	//! cheapest test tier: bounded above by score + weight, never below score, linear falloff.
	//!
	//! ⚠ PUBLIC, because invariant 2 is a claim about which FIELD gets written and the initialisation
	//! tier can only make that claim as a live one. Nothing outside the evaluator and that case calls
	//! it.
	//! \param[in] candidate The candidate to bias.
	//! \param[in] anchor The faction's bias, or null when it has none.
	void ApplyObjectiveAnchorBias(notnull OVT_CandidatePosition candidate, OVT_DeploymentObjectiveAnchor anchor)
	{
		if (!anchor)
			return;

		candidate.sortBy = OVT_ObjectiveSelection.ApplyAnchorBias(candidate.sortBy, vector.Distance(candidate.position, anchor.position), anchor.radius, anchor.weight);
	}

	//------------------------------------------------------------------------------------------------
	// The objective reserve floor - a pushed, per-faction CEILING on the routine evaluator
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Keeps part of a faction's pool out of the ROUTINE evaluator's reach, for one named operation.
	//!
	//! A non-positive cost CLEARS instead of storing, exactly as SetObjectiveAnchor() does with a dead
	//! radius, so an entry in the store is always a live floor.
	//!
	//! ⚠ An unchanged re-push KEEPS the existing record, and that is load-bearing. The owner re-pushes
	//! on EVERY one of its ticks - once every ten real seconds at 6x - because a floor that is not
	//! continuously re-earned is a floor that outlives the intent. Replacing the record each time would
	//! reset the `announced` latch and turn the diagnostic below into one WARNING every ten seconds.
	//! Keyed on the pair through IsSameReserve(), so a different operation or a changed price is heard
	//! immediately and an identical repeat is silent.
	//!
	//! IDEMPOTENT. A faction has at most one objective, so it has at most one floor.
	//! \param[in] factionIndex The faction whose routine spending is being floored.
	//! \param[in] operation What the money is being kept for. Diagnostics only; may be empty.
	//! \param[in] cost How much to withhold. Non-positive clears.
	void SetObjectiveReserve(int factionIndex, string operation, int cost)
	{
		// Server-only component: the collections are not allocated on clients.
		if (!m_mObjectiveReserves)
			return;

		if (cost <= OVT_DeploymentSelection.NO_RESERVE)
		{
			ClearObjectiveReserve(factionIndex);
			return;
		}

		OVT_DeploymentObjectiveReserve existing = m_mObjectiveReserves.Get(factionIndex);
		if (existing && OVT_DeploymentSelection.IsSameReserve(existing.operation, existing.cost, operation, cost))
			return;

		OVT_DeploymentObjectiveReserve reserve = new OVT_DeploymentObjectiveReserve();
		reserve.operation = operation;
		reserve.cost = cost;
		reserve.announced = false;

		m_mObjectiveReserves.Set(factionIndex, reserve);
	}

	//------------------------------------------------------------------------------------------------
	//! Gives a faction's whole pool back to the routine evaluator.
	//!
	//! Safe on a faction that never had a floor, which every teardown path relies on. Every path that
	//! ends an objective, and every tick on which its owner is NOT short of money, has to reach this -
	//! or routine spending would be held off for an operation nobody intends to send any more.
	//! \param[in] factionIndex The faction to stop flooring.
	void ClearObjectiveReserve(int factionIndex)
	{
		if (!m_mObjectiveReserves)
			return;

		m_mObjectiveReserves.Remove(factionIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops every faction's floor at once. See the call in ApplyPersistedFactionResources().
	void ClearAllObjectiveReserves()
	{
		if (!m_mObjectiveReserves)
			return;

		m_mObjectiveReserves.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! A faction's objective reserve floor.
	//!
	//! ⚠ PUBLIC for the same reason GetObjectiveAnchor() is: "no faction in this world is floored, so
	//! the evaluator is spending exactly what it spent before the floor existed" is a claim the
	//! initialisation tier can make as a live one, and an inspection of a diff is not.
	//! \param[in] factionIndex The faction to look up.
	//! \return The reserve, or null when that faction is not floored - which is the normal state.
	OVT_DeploymentObjectiveReserve GetObjectiveReserve(int factionIndex)
	{
		if (!m_mObjectiveReserves)
			return null;

		return m_mObjectiveReserves.Get(factionIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! How much of a faction's pool is earmarked, as the plain integer the arithmetic wants.
	//!
	//! ZERO IS THE DEFAULT AND ZERO IS THE IDENTITY. Every faction with no entry - which is every
	//! faction in every campaign that never starts an objective - answers
	//! OVT_DeploymentSelection.NO_RESERVE, and SpendableResources() hands the pool straight back.
	//! \param[in] factionIndex The faction to look up.
	//! \return The reserved amount, or NO_RESERVE.
	int GetObjectiveReserveCost(int factionIndex)
	{
		OVT_DeploymentObjectiveReserve reserve = GetObjectiveReserve(factionIndex);
		if (!reserve)
			return OVT_DeploymentSelection.NO_RESERVE;

		return reserve.cost;
	}

	//------------------------------------------------------------------------------------------------
	//! Says, ONCE per (operation, price), that this pass is spending less than the faction holds and
	//! what the difference is being kept for.
	//!
	//! ⚠ Mandatory. A floor is invisible from the outside: nothing errors, no deployment fails, the
	//! faction simply buys less than it could and a reader has no way to tell that from a quiet
	//! campaign.
	//!
	//! ⚠ Latched per (operation, price), not per pass and not per faction. Per pass would be a line
	//! every 30 seconds for hours; per faction would hide the second earmark behind the first. The
	//! latch lives on the record, so it is dropped for free by every path that clears or re-prices it.
	//! \param[in] factionIndex The faction being evaluated.
	//! \param[in] pool What that faction holds.
	//! \param[in] spendable What this pass may spend out of it.
	protected void AnnounceObjectiveReserve(int factionIndex, int pool, int spendable)
	{
		OVT_DeploymentObjectiveReserve reserve = GetObjectiveReserve(factionIndex);
		if (!reserve || reserve.announced)
			return;

		reserve.announced = true;

		string what = "its next objective operation";
		if (reserve.operation != "")
			what = "'" + reserve.operation + "'";

		string line = "[Overthrow] Deployment pool for faction " + factionIndex.ToString() + ": routine spending is held to " + spendable.ToString() + " of " + pool.ToString() + " because " + reserve.cost.ToString() + " is reserved for " + what;

		if (spendable <= 0)
			line = line + ". This pass will buy nothing at all - the faction is saving up, and routine garrisoning resumes the moment the operation is sent or abandoned";
		else
			line = line + ". Routine garrisoning continues out of the remainder";

		Print(line + ". This line repeats only if the operation or its price changes", LogLevel.NORMAL);
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
	//! Creates a deployment outside the evaluation pass, for a caller that has already decided WHERE
	//! and WHY - the objective director's operations.
	//!
	//! ⚠ It does NOT debit the pool, and it never will. resourcesInvested is STAMPED onto the
	//! deployment (so a refund and the GM snapshot read a real number) and nothing more; the evaluation
	//! path debits separately, right after its own create. A forcing caller owns its own accounting and
	//! must call SubtractFactionResources itself, immediately, or the faction spends money it still has.
	//!
	//! \param[in] config The config to run.
	//! \param[in] position Where the marker goes.
	//! \param[in] factionIndex The owning faction.
	//! \param[in] resourcesInvested What the caller is about to debit. Stamped, not moved.
	//! \param[in] threatLevel Threat to stamp on the deployment.
	//! \param[in] yaw Heading for the marker. Only the forward-base operation passes one.
	//! \return The created deployment, or null when it could not be built. RETURNED so a forcing
	//!         caller can tell "created, now debit" from "refused, debit nothing".
	OVT_DeploymentComponent ForceCreateDeployment(OVT_DeploymentConfig config, vector position, int factionIndex, int resourcesInvested = 0, float threatLevel = 0, float yaw = 0)
	{
		return CreateDeployment(config, position, factionIndex, resourcesInvested, threatLevel, yaw);
	}

	//------------------------------------------------------------------------------------------------
	//! ForceCreateDeployment, plus "and this force sets out from HERE".
	//!
	//! WHY A RUNTIME SOURCE RATHER THAN A PROVIDER (D7). A mounted force's origin is sometimes a
	//! RUNTIME fact - the base a QRF wave is drawing from, the compound whose parked armour has just
	//! been crewed - and OVT_DeploymentSourceProvider is authored. Rather than invent a mutable
	//! provider, the mounted module carries an override its own source resolution prefers, and this is
	//! the one place that applies it.
	//!
	//! ⚠ IT MUST LAND BEFORE THE FIRST CONVERGENCE, and it does, with a whole update interval to spare.
	//! Creation and activation are NOT the same call stack: CreateDeployment() -> InitializeDeployment()
	//! clones the modules, calls Initialize() on each (which reaches only the empty OnInitialize hook -
	//! nothing in the tree overrides it) and arms a repeating 8-12 s UpdateDeployment; ActivateDeployment()
	//! - which is what calls OnActivate() and therefore EnsureGroups() - runs at the END of the FIRST of
	//! those ticks. So a source applied here is in place a full update interval before anything asks for
	//! it, and the override does not have to be threaded through CreateDeployment itself.
	//!
	//! ⚠ It does NOT debit the pool, exactly as ForceCreateDeployment does not. A forcing caller owns
	//! its own accounting.
	//! \param[in] config The config to run.
	//! \param[in] position Where the marker goes.
	//! \param[in] factionIndex The owning faction.
	//! \param[in] sourcePosition Where the force sets out from. vector.Zero applies no override, which
	//!            leaves every module on its authored provider.
	//! \param[in] resourcesInvested What the caller is about to debit. Stamped, not moved.
	//! \param[in] threatLevel Threat to stamp on the deployment.
	//! \param[in] yaw Heading for the marker.
	//! \return The created deployment, or null when it could not be built.
	OVT_DeploymentComponent ForceCreateDeploymentFrom(OVT_DeploymentConfig config, vector position, int factionIndex, vector sourcePosition, int resourcesInvested = 0, float threatLevel = 0, float yaw = 0)
	{
		OVT_DeploymentComponent deployment = CreateDeployment(config, position, factionIndex, resourcesInvested, threatLevel, yaw);
		if (!deployment)
			return null;

		if (sourcePosition == vector.Zero)
			return deployment;

		ApplyMountedSourceOverride(deployment, sourcePosition);

		return deployment;
	}

	//------------------------------------------------------------------------------------------------
	//! Tells every mounted module on a freshly created deployment where its force comes from.
	//!
	//! ⚠ THE RUNTIME MODULES, NOT THE CONFIG'S. A deployment holds CLONES; writing to the config's own
	//! module objects would set the source on the template and therefore on every deployment that
	//! config ever creates afterwards.
	//! \param[in] deployment The created deployment.
	//! \param[in] sourcePosition Where its mounted forces set out from.
	protected void ApplyMountedSourceOverride(notnull OVT_DeploymentComponent deployment, vector sourcePosition)
	{
		array<OVT_BaseSpawningDeploymentModule> spawningModules = deployment.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			OVT_MountedForceSpawningDeploymentModule mounted = OVT_MountedForceSpawningDeploymentModule.Cast(spawningModule);
			if (mounted)
				mounted.SetSourceOverride(sourcePosition);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One registered config, by the name it was authored under.
	//!
	//! EXISTS SO NOBODY WALKS m_aDeploymentConfigs FROM OUTSIDE THIS FILE. A caller that creates a
	//! specific config by name - the objective director, whose ramp names one config per rung - would
	//! otherwise reach into the registry array, and the registry's internals would be leaking out of
	//! it the way the modifier config's did before GetModifierIndexByName.
	//! \param[in] deploymentName The config's m_sDeploymentName.
	//! \return The config, or null when the registry holds none by that name.
	OVT_DeploymentConfig FindConfigByName(string deploymentName)
	{
		if (deploymentName == "")
			return null;

		if (!m_DeploymentRegistry || !m_DeploymentRegistry.m_aDeploymentConfigs)
			return null;

		foreach (OVT_DeploymentConfig config : m_DeploymentRegistry.m_aDeploymentConfigs)
		{
			if (config && config.m_sDeploymentName == deploymentName)
				return config;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	void DeleteDeployment(OVT_DeploymentComponent deployment)
	{
		if (deployment)
			deployment.DestroyDeployment();
	}

	//------------------------------------------------------------------------------------------------
	//! TAKES A DEPLOYMENT DOWN THAT HAS FINISHED ITS JOB, AND PAYS BACK FOR THE MEN WHO SURVIVED IT.
	//!
	//! The third of three teardowns, and the difference is whose decision it was and how it went:
	//!   - DeleteDeployment()  "this force no longer exists"    - pays nothing;
	//!   - RecallDeployment()  "we changed our mind, come back" - pays back the whole investment;
	//!   - CollectDeployment() "the job is done, come home"     - pays back the men, not the mission.
	//! A completed operation is not a recall: the money that bought the transport, the composition and
	//! the overhead was genuinely spent. What was not spent is the men, if they are all still standing.
	//!
	//! ⚠ The refund is asked of the MODULES, not computed here. Each spawning module knows its own
	//! price per group and its own groups; the manager only sums and credits. That keeps "what is
	//! refundable" a property of the thing that was bought.
	//!
	//! ⚠ It credits through AddFactionResources like the recall does: "resource accounting is closed"
	//! is checked by grepping for callers of that method, and the answer it is allowed to give is the
	//! occupying faction manager's single credit point plus the deployment framework's own refunds.
	//!
	//! ⚠ It cannot double-pay. The refund is read from live group records, and DeleteDeployment()
	//! unregisters every one of them on the same call stack, so a second call finds nothing to refund.
	//! There is no stamp to zero because there is no stored figure.
	//! \param[in] deployment The deployment to collect. Null is a no-op.
	//! \return What was credited back to the controlling faction's pool.
	int CollectDeployment(OVT_DeploymentComponent deployment)
	{
		if (!deployment)
			return 0;

		int refund = 0;

		array<OVT_BaseSpawningDeploymentModule> spawningModules = deployment.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule module : spawningModules)
		{
			if (!module)
				continue;

			refund += module.GetIntactGroupRefund();
		}

		if (refund > 0)
		{
			int factionIndex = deployment.GetControllingFaction();
			AddFactionResources(factionIndex, refund);

			Print(string.Format("[Overthrow] Collected deployment '%1' after it finished its job - %2 resources returned to faction %3's pool for the groups that came through it intact",
				deployment.GetDeploymentName(), refund.ToString(), factionIndex.ToString()), LogLevel.NORMAL);
		}

		DeleteDeployment(deployment);

		return refund;
	}

	//------------------------------------------------------------------------------------------------
	//! TAKES A DEPLOYMENT DOWN AND HANDS BACK WHAT WAS INVESTED IN IT - a RECALL rather than a delete.
	//!
	//! DeleteDeployment is "this force no longer exists"; a recall is "we changed our mind and called
	//! them back", and the money for men who were still walking has not been spent on anything. The
	//! objective director is the caller this exists for: before this method, the operations it tore
	//! down when abandoning an objective simply evaporated their resources.
	//!
	//! ⚠ The refund lives here, in the framework, not at the call site. "Resource accounting is closed"
	//! is checked by grepping the tree for callers of the credit method, and the answer that grep is
	//! allowed to give is the occupying faction manager's single credit point plus the deployment
	//! framework's own refunds.
	//!
	//! ⚠ A force that was WIPED OUT is a loss, not a recall, and pays nothing: refunding for it would
	//! pay the occupying faction for losing a fight.
	//!
	//! ⚠ It cannot double-credit, structurally rather than by a flag: the stamp is zeroed BEFORE the
	//! credit, so a second call reads 0 and pays nothing. The delete then removes it from the world.
	//! \param[in] deployment The deployment to recall. Null is a no-op.
	//! \return What was credited back to the controlling faction's pool. Zero when there was nothing to
	//!         refund, when the force was eliminated, or when there was no deployment.
	int RecallDeployment(OVT_DeploymentComponent deployment)
	{
		if (!deployment)
			return 0;

		int refund = 0;
		if (!deployment.GetSpawnedUnitsEliminated())
			refund = deployment.GetResourcesInvested();

		if (refund > 0)
		{
			// ZEROED FIRST. See the header - this is the whole of the "exactly once" property.
			deployment.SetResourcesInvested(0);

			int factionIndex = deployment.GetControllingFaction();
			AddFactionResources(factionIndex, refund);

			Print(string.Format("[Overthrow] Recalled deployment '%1' before it finished - %2 resources returned to faction %3's pool",
				deployment.GetDeploymentName(), refund.ToString(), factionIndex.ToString()), LogLevel.NORMAL);
		}

		DeleteDeployment(deployment);

		return refund;
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