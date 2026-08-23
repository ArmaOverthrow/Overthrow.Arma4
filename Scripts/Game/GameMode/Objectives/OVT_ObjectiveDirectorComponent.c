[EntityEditorProps(category: "Overthrow/Managers", description: "Owns the occupying faction's current objective and the three-phase campaign to take it")]
class OVT_ObjectiveDirectorComponentClass : OVT_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! THE OCCUPYING FACTION'S ONE PIECE OF LONG-TERM INTENT.
//!
//! Server-only game-mode component. At most ONE objective exists at any moment - a resistance-held
//! base, town or city the occupying faction has decided to take back - and it is in exactly one
//! phase. Once per in-game minute it selects one when it has none, runs the phase machine, and
//! biases the deployment evaluator toward it.
//!
//! ⚠ IT IS NOT A MANAGER, AND THE NAME IS DELIBERATE (D1). Every OVT_*ManagerComponent owns a
//! registry and answers questions about it. This one owns no registry and answers no questions; it
//! makes a DECISION every minute. Nothing else in the campaign may start an offensive operation -
//! that single-decision-maker property is goal G1 and is checked by grep.
//!
//! EVERYTHING IT BUILDS IS A DEPLOYMENT (G4): it never spawns a group itself. It never holds money
//! either (G5) - every resource it spends leaves the one faction pool at the moment the deployment
//! is created, and the forward base's "budget" is a spend CEILING against that pool, not a wallet.
//!
//! The arithmetic is NOT in here. Selection scoring, the phase gates, the difficulty clamps and the
//! starvation predicate live in OVT_ObjectiveSelection and OVT_ObjectivePhaseRules as pure statics,
//! so the whole machine is assertable in the cheapest test tier.
//!
//! ⚠ EVERY TIMER IS A TICK COUNTER, NEVER A WALL-CLOCK DEADLINE (D4). Nothing here stores "at what
//! time"; it stores "in how many ticks". That is what makes the QRF freeze correct by construction
//! rather than a rule someone has to remember to apply to each timer - see DirectorTick().
//------------------------------------------------------------------------------------------------
class OVT_ObjectiveDirectorComponent : OVT_Component
{
	//------------------------------------------------------------------------------------------------
	// ATTRIBUTES
	//------------------------------------------------------------------------------------------------

	[Attribute(defvalue: "5000", desc: "Distance (m) at which the selection proximity term reaches zero. Beyond it a candidate is scored as unreachable rather than as merely distant", category: "Objective Director")]
	protected float m_fMaxUsefulDistance;

	//! ⚠ AN IDLE CLOCK, NOT A PHASE BUDGET. It used to measure in-game minutes since the phase was
	//! ENTERED, which made poverty and a long walk both look like a wedge: a play-test spent 31 real
	//! minutes unable to afford a single operation, created one, and had it deleted mid-walk 9 minutes
	//! later because the phase clock - not the operation - had run out. It now measures in-game minutes
	//! since the objective last made PROGRESS. See TickObjectiveIdleClock().
	[Attribute(defvalue: "240", desc: "In-game minutes an objective may go WITHOUT PROGRESS before it is abandoned as wedged. An IDLE clock, not a phase budget: every operation created, every operation reported complete and every phase entry re-arms it, and it is held entirely while an operation is still in flight or the faction pool cannot afford the next one. A backstop, not a pacing knob", category: "Objective Director")]
	protected int m_iPhaseTimeoutTicks;

	[Attribute(defvalue: "1", desc: "Selection rounds a failed objective sits out before it may be chosen again", category: "Objective Director")]
	protected int m_iBlacklistRounds;

	[Attribute(defvalue: "25", desc: "The most the objective may add to a routine deployment candidate's SORT KEY, at the objective itself, falling off linearly to nothing at the phase's anchor radius. It biases which suitable work is bought first and never which work is suitable", category: "Objective Director")]
	protected float m_fObjectiveAnchorWeight;

	//! THE AUTHORED SURFACE. Every plan the occupying faction may commit to, and every phase of each
	//! plan, as data - see OVT_ObjectiveRegistry. Wired on the game-mode prefab beside the deployment
	//! registry, in the same inherit-and-delta form.
	//!
	//! 🔴 It may legitimately be NULL, and that means the occupying faction STOPS ATTACKING. A world
	//! whose prefab predates this attribute, a mod that clears it, or a .conf that failed to load
	//! leaves the registry unresolved - and every rule the faction follows is in it. There is no second
	//! implementation, so selection picks nothing and says so once, and an objective committed from
	//! outside selection does nothing and says so once. Both are ERROR lines, deliberately: a quiet
	//! faction is the one failure mode with no symptom a player could report.
	[Attribute(desc: "The objective plan registry - every doctrine the occupying faction may commit to. Authored as a .conf, exactly like the deployment registry beside it", category: "Objective Director")]
	protected ref OVT_ObjectiveRegistry m_Registry;

	[Attribute(defvalue: "1", desc: "How many objectives may run at once. The campaign is designed for N and shipped and tested at 1; raising it is not tuned and every cadence, cap and reserve figure was chosen for a single objective", category: "Objective Director")]
	protected int m_iMaxConcurrentObjectives;

	//------------------------------------------------------------------------------------------------
	// CONSTANTS
	//------------------------------------------------------------------------------------------------

	//! Log prefix, matching the campaign's other server-side systems.
	static const string LOG = "[Overthrow.ObjectiveDirector] ";

	//! Tick period before the time multiplier is applied, in milliseconds.
	//!
	//! ⚠ Deliberately the same number as the occupying faction manager's own update period: the phase
	//! machine and the resource tick have to advance in step and freeze together, and a "45-minute"
	//! harassment cadence has to mean 45 IN-GAME minutes at any time multiplier. Re-declared rather
	//! than read across because that constant is not static on its own class.
	static const int DIRECTOR_UPDATE_FREQUENCY = 60000;

	//! Fallback day-length multiplier when the world carries no time handler, matching the occupying
	//! faction manager's own fallback so both tick at the same rate in a world without one.
	static const float DEFAULT_TIME_MULTIPLIER = 6;

	//! How far from its recorded position the forward base's deployment may be found again after a
	//! load. Generous: the marker is created at the recorded position, so anything inside this is the
	//! same deployment and anything outside it is a different one.
	static const float FOB_RELINK_RADIUS = 150;

	//! Ticks the re-link is allowed to come up empty before the objective is torn down. The deployment
	//! entities are restored by their own tracked records, and load order between those and this
	//! component's payload is NEVER assumed - so a first tick that finds nothing is given a couple of
	//! chances before it is believed.
	static const int FOB_RELINK_ATTEMPTS = 3;

	//! Search radius used when taking one of the director's own deployments back down. Matches the
	//! framework's own name-scoped dedup radius, so a lookup finds the deployment this director
	//! created and not one that happens to share its config elsewhere.
	static const float TEARDOWN_LOOKUP_RADIUS = 250;

	//! How far the objective bias reaches for a phase that authors no radius of its own, in metres.
	//!
	//! 🔴 The per-phase numbers are authored data now and this is only the fallback - deliberately the
	//! TIGHT one, because a bias nobody chose the reach of should lean on as little of the map as
	//! possible. Both shipped plans author their own:
	//!   600 m, HARASSMENT - the only thing worth leaning on is the objective itself, and harassment
	//!     is the one phase that may still re-select.
	//!   1200 m, FORWARD BASE AND BATTLE - the objective is LOCKED, and the forward base stands
	//!     BETWEEN its nearest held base and the objective, so the band that needs garrisoning is the
	//!     whole approach. The ground being fought over does not shrink when the fighting starts.
	static const float DEFAULT_ANCHOR_RADIUS = 600;

	//! THE HARASSMENT GROUP LADDER, one registered deployment config per rung, in ascending order.
	//!
	//! ⚠ The ramp is carried by CONFIGS, not by a per-create group override. The first four rungs are
	//! thin registry variants of one file, Deployment_ObjectiveHarassment.conf, inherited with a
	//! different m_sGroupType, cost and name; the fifth is its own file because it swaps the spawning
	//! module itself, which a registry delta cannot do. A director-side override applied after the
	//! create would be a second way of deciding what a deployment contains, invisible to everything
	//! that reads a config - the evaluator's cost model, the Game Master panel, the reinforcement
	//! rebuy and the save.
	//!
	//! ⚠ The names are the keys and are matched BY STRING three times each: FindConfigByName() resolves
	//! the rung, GetDeploymentNearPosition() and the concurrency count match live deployments back to
	//! it, and the teardown ledger stores it. A name changed in overthrowDeployments.conf and not here
	//! does not fail to parse and does not warn - the ramp simply stops sending anything, with one
	//! WARNING per in-game minute as its only symptom. An Init-tier case resolves every entry here.
	//!
	//! ⚠ ORDER IS THE RAMP. HarassmentLadderIndex indexes straight into this, saturating at the top
	//! rung; reordering it re-tunes the whole escalation.
	//!
	//! ⚠ THE TOP RUNG SATURATES, SO IT IS THE ONE THAT RUNS FOREVER, and that is why the mounted rung
	//! is last and why it still authors a town harassment behaviour of its own. A top rung that did
	//! not push support down would stall the ramp at the phase gate the moment it was reached: the
	//! forward-base phase opens on the town's support falling, and after four successes every
	//! subsequent operation is this one.
	//!
	//! ⚠ THE FIFTH RUNG ESCALATES BY VEHICLE, NOT BY GROUP. It fields the same light fireteam as rung
	//! two, in an armed vehicle picked off the faction's threat ladder, so "each rung fields a bigger
	//! group than the last" stops being true here on purpose.
	static const ref array<string> HARASSMENT_LADDER = {
		"Objective Harassment (Patrol)",
		"Objective Harassment (Fireteam)",
		"Objective Harassment (Rifle Squad)",
		"Objective Harassment (Heavy)",
		"Objective Harassment (Mounted)"
	};

	//! The one recapture config, by name. Same string-matching hazard as the ladder above.
	static const string TOWER_RECAPTURE_CONFIG = "Objective Tower Recapture";

	//! The one sabotage config, by name. Same string-matching hazard as the ladder above.
	//!
	//! ⚠ It has NO rungs, deliberately. The harassment ramp escalates because its effect is a stacking
	//! debuff that needs bigger groups to keep landing; a sabotage mission's effect is a structure that
	//! is gone, which does not get more gone with a bigger team. What escalates on a base objective is
	//! the number of missions the counter-attack gate demands.
	static const string SABOTAGE_CONFIG = "Objective Sabotage";

	//! How far from the objective a deployment counts towards the harassment concurrency cap. Wider
	//! than the town centre because an operation's marker sits where the director put it and a town is
	//! not a point.
	static const float HARASSMENT_OP_RADIUS = 800;

	//! How close to a tower an existing recapture deployment has to be to count as "already sent".
	//! Matches the 300 m ring that classifies a position as RADIO_TOWER everywhere else.
	static const float TOWER_OP_DEDUP_RADIUS = 300;

	//! How far from a base objective a deployment counts towards the sabotage concurrency cap, and how
	//! close an existing one has to be to count as "already sent". Matches the sabotage module's own
	//! m_fMaxBaseDistance, so a mission and the count that limits it agree about which base is which.
	static const float SABOTAGE_OP_RADIUS = 300;

	//! The support modifier a completed harassment operation stacks onto its town, and the CAUSAL HALF
	//! OF THE FORWARD-BASE GATE: the objective may not advance out of harassment unless the town is
	//! actually carrying it. Must match the name authored on the harassment behaviour module and, at
	//! the end of Configs/Modifiers/supportModifiers.conf, on the modifier entry itself.
	static const string HARASSMENT_MODIFIER = "ObjectiveHarassment";

	//! The deployment that carries the forward operating base's structure and its free garrison. Same
	//! three-way string contract as the harassment ladder: it resolves a config, it matches a live
	//! deployment back, and it is the re-link key written into the save.
	static const string FOB_CONFIG = "Objective Forward Base";

	//! The extra garrison sent to a forward base once it is standing, sourced FROM it.
	static const string FOB_GARRISON_CONFIG = "Objective Forward Base Garrison";

	//! The forward base's key in the asset map. THE ONE PLACE THE LITERAL IS WRITTEN: every consumer
	//! asks IsAssetUp(OVT_ObjectiveDirectorComponent.ASSET_FOB), so a mistyped key is a compile error
	//! rather than a silently absent asset.
	static const string ASSET_FOB = "fob";

	//------------------------------------------------------------------------------------------------
	// ⚠ THE RUNNER NAMES NO PLAN AND NO PHASE, AND THE CONSTANTS THAT USED TO SIT HERE ARE GONE.
	//
	// Selection is plan-driven, so the round that picks a plan hands it to CommitObjective(), and a
	// commit from outside a selection round asks the registry which plan can DESCRIBE the kind. The
	// phase names existed only to map the old phase enum onto the two shipped plans' phase order; a
	// phase is now an index into the running plan plus an authored m_sPhaseName, everywhere.
	//
	// 🔴 Nothing in this component may name a shipped plan or a shipped phase again. A constant here
	// is a doctrine this build knows about, and the whole point of the authored registry is that it
	// does not know about any of them.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// WHY A CREATE WAS REFUSED
	//
	// 🔴 Every way out of CreateObjectiveDeployment() that is not a create has a reason here, and the
	// reason IS the latch key. A play-test watched the forward-base phase re-site the same point every
	// ten seconds for five minutes with not one line to say why: the create was refused for want of
	// resources, and the only affordability line was latched ONCE PER OBJECTIVE and had been spent
	// twelve minutes earlier on a sabotage operation in the previous phase.
	//
	// ⚠ The strings ARE the keys. LogOperationRefusal() latches on the PAIR (config, reason), so
	// reusing a constant for two different situations silently merges them.
	//------------------------------------------------------------------------------------------------

	//! The name resolved nothing in the deployment registry - a config renamed in overthrowDeployments.conf.
	static const string REFUSAL_UNREGISTERED = "no deployment config by that name is registered";

	//! The one refusal that is not the objective's fault. See CreateObjectiveDeployment().
	static const string REFUSAL_POOL_SHORT = "the occupying faction's deployment pool cannot cover it";

	//! The forward base has had its share. A decision the director made about itself.
	static const string REFUSAL_FOB_CEILING = "the forward base's spend ceiling is already spent";

	//! The framework declined the create - an invalid config, a missing prefab, a spawn that failed.
	static const string REFUSAL_FRAMEWORK_DECLINED = "the deployment framework declined to create it";

	//! There is no base to run a supply line from, so there is nowhere to site a forward base between.
	static const string REFUSAL_NO_SOURCE_BASE = "the occupying faction holds no base to supply it from";

	//! In-game minutes between repeats of the "still cannot afford it" line while the idle clock is held.
	//!
	//! ⚠ A heartbeat, not a one-shot, and not per tick either. The affordability hold is DESIGNED to
	//! persist, so a one-shot line leaves a reader unable to tell "held, still broke" from "wedged, no
	//! idea". Sixty in-game minutes is one line per real ten minutes at the shipped 6x day multiplier.
	static const int AFFORDABILITY_HEARTBEAT_TICKS = 60;

	//! ResolveOperationCadence(): this phase's interval cannot be resolved at all, so nothing may be
	//! bought. Deliberately negative rather than zero - zero is a legal authored cadence meaning "every
	//! in-game minute", and answering it for an unresolvable phase would turn a broken campaign into an
	//! unbounded spender instead of a quiet one.
	static const int NO_CADENCE = -1;

	//------------------------------------------------------------------------------------------------
	// STATE
	//------------------------------------------------------------------------------------------------

	//! The current objective. Never null after Init(); an absent objective is kind NONE, phase IDLE.
	protected ref OVT_ObjectiveRecord m_Objective;

	//! The forward operating base for the current objective. Never null after Init(); an absent one
	//! has up == false.
	protected ref OVT_ObjectiveFOBRecord m_FOB;

	//! Every asset this director has standing, by key. THE RECORDS ARE SHARED, NOT COPIED: the "fob"
	//! entry IS m_FOB, so the keyed API and the FOB paths can never disagree about whether the base is
	//! up or where it is. Built once in OnPostInit() and never re-keyed.
	//! ⚠ None of it replicates (G12). On a remote client the records exist but were never written, so
	//! IsAssetUp() is false and GetAssetPosition() is the zero vector.
	protected ref map<string, ref OVT_ObjectiveAssetRecord> m_mAssets;

	//! Objectives serving a cooldown after a failure.
	//! THE OBJECTIVE IN FLIGHT, and everything the current phase's modules have accumulated.
	//!
	//! ⚠ Allocated once and kept for the component's lifetime, rather than made on commit and dropped
	//! on reset. It owns the record above - m_Objective IS m_Instance.GetRecord(), one object, not a
	//! copy - so every line of runner that reads m_Objective is reading the instance's own state. A
	//! per-commit instance would make every one of those lines a null check for no gain.
	protected ref OVT_ObjectiveInstance m_Instance;

	//! The objectives actually running. EMPTY while idle, one entry while an objective is live.
	//!
	//! ⚠ IT HOLDS m_Instance, IT DOES NOT REPLACE IT. Membership of this list is what "there is an
	//! objective" means to the tick and to the selection cadence; the instance object itself outlives
	//! every objective it runs. At m_iMaxConcurrentObjectives = 1 this is exactly today's single-slot
	//! machine, expressed so that N > 1 is a tuning question rather than a rewrite.
	protected ref array<ref OVT_ObjectiveInstance> m_aInstances;

	//! Whether the "this plan is not in the registry" line has already been said for the current
	//! objective. Once per objective, not once per in-game minute.
	protected bool m_bMissingPlanLogged;

	protected ref array<ref OVT_ObjectiveBlacklistEntry> m_aBlacklist;

	//! Deployments this director created for the current objective, so the reset path can take exactly
	//! those back down. Runtime-only and cleared on every reset.
	protected ref array<ref OVT_ObjectiveDeploymentRef> m_aCreatedDeployments;

	//! Set by both control-change handlers, consumed at the top of the next tick. NEVER acted on
	//! inline - see OnBaseControlChanged() for why.
	protected bool m_bReselectPending;

	//! In-game minutes left before an IDLE tick may run another selection round (D6). Re-armed by
	//! SelectObjective() itself and served by IsSelectionDue(), so every path that runs a round pays
	//! the same cooldown and no path can run two rounds in one minute.
	//! ⚠ AT THE SHIPPED CADENCE OF 1 IT NEVER LEAVES ZERO, which is what makes the whole mechanism a
	//! no-op by default and the pre-plan selection behaviour byte-identical.
	protected int m_iSelectionCooldown;

	//! True while a restored payload still has to be re-linked to the live campaign on a tick.
	protected bool m_bRestorePending;

	//! Re-link attempts already spent, against FOB_RELINK_ATTEMPTS.
	protected int m_iRelinkAttempts;

	//! True once the "nothing worth taking" line has been logged, so an early campaign does not print
	//! it once per in-game minute forever. Cleared the moment anything is selectable again.
	protected bool m_bIdleLogged;

	//! THE MODULE THAT OWNS EACH STANDING ASSET, by asset key.
	//!
	//! 🔴 It is what makes the ONE teardown path reach the doctrine that built the asset. An asset
	//! outlives the PHASE that built it: it stands through the counter-attack and comes down when the
	//! OBJECTIVE ends, on a tick where its own module is not in the running phase's set at all. A
	//! module registers itself here on entry and this map holds it - strongly - until the objective
	//! record is cleared.
	//!
	//! ⚠ The director never names a concrete module through it. It asks the base class three questions
	//! - is your ceiling armed, what is it, take yourself down - so the runner stays doctrine-free.
	//!
	//! ⚠ Runtime-only and deliberately NOT persisted. A restored objective rebuilds its phase's module
	//! set from the plan, and the module that comes back adopts a base already standing on its entry.
	protected ref map<string, ref OVT_BaseObjectiveAssetModule> m_mAssetModules;

	//! True once the "everything it can do is done and it is waiting" line has been said for the phase
	//! the objective is currently in. Cleared on every phase entry and with the objective record, so a
	//! later phase - and the next objective - each say it again.
	protected bool m_bIdleHoldLogged;

	//! One-shot, per operation walk: an operation module that did NOT act has claimed the interval
	//! anyway, so nothing later in the authored order may spend it.
	//!
	//! 🔴 Two play-tests put it here and both were livelocks. The forward base refused FOR MONEY has
	//! first claim on the pool: without the claim the faction buys a cheaper ramp operation every time
	//! the pool passes ITS price and never reaches the base's, and the reserve floor names the wrong
	//! operation because the last refusal in the walk overwrites the first. And a tick spent CLEARING
	//! THE WAY for a base must not hand its interval to a sabotage mission.
	protected bool m_bOperationIntervalClaimed;

	//! Refusals already said out loud, config half. Parallel to m_aRefusalReasons and never re-ordered:
	//! index i of one is the same refusal as index i of the other.
	//!
	//! ⚠ TWO PARALLEL ARRAYS RATHER THAN ONE COMPOSITE KEY, deliberately. A successful create clears its
	//! own config's entries and nothing else's (ForgetOperationRefusals), which with a composite key
	//! would be prefix surgery on a string; with a pair it is an equality test.
	protected ref array<string> m_aRefusalConfigs;

	//! Refusals already said out loud, reason half. One of the REFUSAL_* constants.
	protected ref array<string> m_aRefusalReasons;

	//! Consecutive in-game minutes the idle clock has been HELD because the pool was short.
	//!
	//! ⚠ A counter, not a latch. It was a bool: one line per objective, then silence for as long as
	//! the poverty lasted, so a reader could not tell "held, still broke" from "wedged, nobody knows".
	//! The line now repeats every AFFORDABILITY_HEARTBEAT_TICKS carrying the elapsed count and the
	//! operation being waited for. Zeroed by any progress and cleared with the objective record.
	protected int m_iAffordabilityHeldTicks;

	//! What the last refused create was trying to buy, and what it cost. Written beside
	//! m_bBlockedOnAffordability so the heartbeat can name the operation the phase is stuck on rather
	//! than saying only that something, somewhere, was too dear.
	protected string m_sBlockedOnConfig;
	protected int m_iBlockedOnCost;

	//! Set during a tick on which a create was refused because the faction pool was short, and consumed
	//! by that same tick's idle clock.
	//!
	//! ⚠ Per tick, NOT persistent state: cleared at the top of every phase handler that can spend and
	//! read exactly once at the bottom of it, so it can never freeze a clock on the strength of a
	//! refusal from some earlier minute. Deliberately NOT set by the forward base's spend CEILING - a
	//! spent ceiling is a decision the director made about itself, and a phase that can no longer buy
	//! anything for that reason SHOULD time out.
	protected bool m_bBlockedOnAffordability;

	//! True while this component believes it is holding a RESERVE FLOOR on the deployment pool.
	//!
	//! ⚠ A cache over the framework's store, kept only so the top-of-tick drop is free. DirectorTick()
	//! clears the floor on every tick before anything can re-push it, and the overwhelming majority of
	//! ticks have no floor to clear - so this bool turns "resolve two managers and remove from a map,
	//! six times a real minute, forever" into a bool test. This component is the only thing that ever
	//! pushes one.
	//!
	//! ⚠ Runtime-only and not persisted, like the floor it mirrors.
	protected bool m_bReserveHeld;

	//! What m_Objective.harassmentSuccesses read the last time the idle clock was re-armed.
	//!
	//! ⚠ The success signal is OBSERVED ON THE TICK, never pushed from the counter.
	//! ReportObjectiveProgress() is public and is called from a deployment's own update, from a restore
	//! and from test fixtures - "it counts, it does not decide". Re-arming a timer from either of them
	//! would break D4 ("only a tick may move a timer"), which Phase 5 already paid for twice.
	protected int m_iProgressHarassmentMark;

	//! What m_Objective.sabotageSuccesses read the last time the idle clock was re-armed. Same rule.
	protected int m_iProgressSabotageMark;

	// ⚠ This component no longer reads the world clock at all. The hour was read for one thing - the
	// counter-attack's daylight window - and that is an authored condition module now, which resolves
	// its own handle lazily because a MODULE is not a component and has no OnPostInit.
	//
	// If anything here ever needs the hour again: OVT_Component ALREADY carries m_Time and fills it in
	// its own OnPostInit. Re-declaring it here would shadow the base class's copy with one nothing ever
	// fills, and the symptom is a clock that reads as absent forever - which every reader in this
	// feature treats as "no restriction".

	//! True once the control-change invokers have been subscribed.
	protected bool m_bHooked;

	//! True once the repeating tick has been installed.
	protected bool m_bTickInstalled;

	//! The two invoker owners, cached at subscription time so the unsubscribe cannot depend on a
	//! static accessor that may already have been torn down.
	protected OVT_OccupyingFactionManager m_HookedOccupying;
	protected OVT_TownManagerComponent m_HookedTowns;

	static OVT_ObjectiveDirectorComponent s_Instance;

	//------------------------------------------------------------------------------------------------
	//! The director on the running game mode.
	//! \return The component, or null before the game mode exists.
	static OVT_ObjectiveDirectorComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode gameMode = GetGame().GetGameMode();
			if (gameMode)
				s_Instance = OVT_ObjectiveDirectorComponent.Cast(gameMode.FindComponent(OVT_ObjectiveDirectorComponent));
		}

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	// LIFECYCLE
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Allocates the director's own state.
	//!
	//! ⚠ Allocated on CLIENTS too, even though every decision here is server-only. The records are a
	//! few dozen bytes and the getters are public: a client that resolves the director through
	//! OVT_Global and asks it a question must get "no objective" rather than a null dereference.
	//! Behaviour is gated at the entry points, not by leaving the state half-built.
	//!
	//! ⚠ This runs in the WORLD EDITOR too, and nothing here may ask a manager anything. Loading a
	//! world in the editor constructs the game mode entity's components without starting a game, so
	//! GetGame().GetFactionManager() is null and the campaign does not exist. It used to call
	//! ClearObjectiveRecord(), whose whole job is dropping the deployment bias, and the resolve chain
	//! behind that drop took the editor down with a NULL faction manager. Construction has no anchor
	//! to drop, so it initialises the record fields directly.
	//! \param owner The game mode entity.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		// ⚠ The instance OWNS the record and the asset map; the two fields below are the SAME objects,
		// not copies. With one object there is no synchronisation step that can be forgotten and no
		// second source of truth to drift. The only way m_Objective can disagree with the instance is
		// if these two lines are deleted, which makes the whole thing vanish rather than go stale.
		m_Instance = new OVT_ObjectiveInstance(this);
		m_aInstances = new array<ref OVT_ObjectiveInstance>();

		m_Objective = m_Instance.GetRecord();

		m_FOB = new OVT_ObjectiveFOBRecord();

		m_mAssets = m_Instance.GetAssetMap();
		m_Instance.SetAsset(ASSET_FOB, m_FOB);
		m_aBlacklist = new array<ref OVT_ObjectiveBlacklistEntry>();
		m_aCreatedDeployments = new array<ref OVT_ObjectiveDeploymentRef>();
		m_aRefusalConfigs = new array<string>();
		m_aRefusalReasons = new array<string>();
		m_mAssetModules = new map<string, ref OVT_BaseObjectiveAssetModule>();

		ClearObjectiveRecordFields();
		ClearFOBRecord();
	}

	//------------------------------------------------------------------------------------------------
	//! Subscribes to the two control-change events that can invalidate a target choice.
	//!
	//! ⚠ CALLED LAST IN THE GAME MODE'S INIT CHAIN, deliberately: the director queries the town and
	//! occupying-faction managers, so both have to exist before it runs.
	//! \param owner The game mode entity.
	void Init(IEntity owner)
	{
		if (!Replication.IsServer())
			return;

		HookControlChanges();
	}

	//------------------------------------------------------------------------------------------------
	//! Installs the repeating tick.
	//!
	//! ⚠ NOT WHERE THE FIRST DECISION HAPPENS. The first tick lands one in-game minute from now and
	//! selects then, so every manager the selection reads has finished its own PostGameStart first.
	void PostGameStart()
	{
		if (!Replication.IsServer())
			return;

		// Belt and braces: a campaign restarted in the same session runs Init() again, and a second
		// subscription would fan every control change out twice.
		HookControlChanges();

		// ⚠ ONCE, HERE, AND NOWHERE ELSE (C6). The deployment registry's equivalent validator has been
		// dead code since it was written - nothing calls it - so "a broken config is named and skipped"
		// was decorative there. Validating from PostGameStart() is what makes it real: it runs after
		// the registry .conf has loaded, before the first tick can select anything, and exactly once
		// per campaign start.
		ValidateObjectiveRegistry();

		InstallTick();
	}

	//------------------------------------------------------------------------------------------------
	//! Installs the repeating tick if it is not already running. Idempotent, and public because an
	//! initialisation-tier test world never runs PostGameStart() and has to install it itself.
	void InstallTick()
	{
		if (m_bTickInstalled)
			return;

		float timeMultiplier = DEFAULT_TIME_MULTIPLIER;

		OVT_TimeAndWeatherHandlerComponent handler = OVT_TimeAndWeatherHandlerComponent.Cast(GetGame().GetGameMode().FindComponent(OVT_TimeAndWeatherHandlerComponent));
		if (handler)
			timeMultiplier = handler.GetDayTimeMultiplier();

		if (timeMultiplier <= 0)
			timeMultiplier = DEFAULT_TIME_MULTIPLIER;

		GetGame().GetCallqueue().CallLater(DirectorTick, DIRECTOR_UPDATE_FREQUENCY / timeMultiplier, true, GetOwner());

		m_bTickInstalled = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Tears the tick and the subscriptions down with the component.
	//! \param owner The game mode entity.
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(DirectorTick);
		m_bTickInstalled = false;

		UnhookControlChanges();

		if (s_Instance == this)
			s_Instance = null;

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	// THE TICK, AND THE FREEZE
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! ONE step of the objective machine. Called once per in-game minute by the repeating timer, and
	//! callable directly by anything that needs a deterministic single step.
	//!
	//! Three early returns, in this order, and the ORDER IS THE CONTRACT:
	//!  1. NOT THE SERVER. Nothing here has a client half.
	//!  2. NOBODY ONLINE. Parity with the occupying faction manager's own tick: an empty server does
	//!     not run a war, or a dedicated server left overnight would ramp to a counter-attack nobody
	//!     was there to see start.
	//!  3. A BATTLE IS LIVE. ⚠ THE FREEZE. m_CurrentQRF is the campaign's single-battle contract.
	//!     While it is set the whole objective machine stops: no phase advances, no operation is sent,
	//!     and because every timer is a TICK COUNT rather than a deadline (D4), not one of them moves.
	//!     There is no catching up to do afterwards because nothing fell behind.
	//!
	//! ⚠ And one thing that happens BEFORE all three (D18). The reserve floor is DROPPED on the first
	//! line and re-earned during the tick, so it is only ever what the most recent COMPLETED tick
	//! concluded - "the floor cannot outlive the intent". It costs nothing to reason about because
	//! callqueue callbacks do not interleave. Putting it above the early returns is deliberate: a
	//! battle starting, a server emptying, or an objective machine that stops being ticked at all must
	//! each give the pool straight back.
	void DirectorTick()
	{
		if (!Replication.IsServer())
			return;

		// See the note above. Free on every tick that is not holding one - see m_bReserveHeld.
		DropObjectiveReserve();

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players || players.GetPlayerCount() == 0)
			return;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return;

		if (occupying.m_CurrentQRF)
			return;

		if (m_bRestorePending)
		{
			// Re-linking a restored payload is a tick-time job and may itself abandon the objective,
			// so it runs before anything reads the phase.
			ResolveRestoredObjective();
		}

		bool alreadySelected = ConsumeReselectRequest();

		// ⚠ Measured BEFORE the loop - a parity rule, not a micro-optimisation. An objective that ENDS
		// during this tick must not be replaced in the same in-game minute. Reading the count after the
		// loop would select a fresh objective on the very tick the last one died, and its first phase
		// arms the operation countdown to ZERO - so the tick after that would buy a real deployment with
		// real resources, one minute early, every single time an objective ended.
		int liveAtTickStart = m_aInstances.Count();

		// ⚠ DOWNWARD, BECAUSE AN INSTANCE MAY REMOVE ITSELF FROM THIS LIST WHILE IT IS BEING RUN.
		// ResetObjective() is reachable from an abort module, from the idle clock and from the battle
		// resolving, and all three funnel through ClearObjectiveRecord(), which drops the instance out
		// of this array. A downward index loop is the only shape that survives that without skipping
		// an entry or reading past the end.
		for (int i = m_aInstances.Count() - 1; i >= 0; i--)
		{
			if (i >= m_aInstances.Count())
				continue;

			OVT_ObjectiveInstance instance = m_aInstances[i];
			if (!instance)
				continue;

			RunObjectivePhaseModules(instance);
		}

		// ⚠ Not a second time in one tick. A re-selection request that found no candidate leaves the
		// machine idle, and running selection again here would serve a SECOND round off every blacklist
		// entry in the same in-game minute.
		//
		// ⚠ And not more often than the authored cadence (D6). IsSelectionDue() is what the registry's
		// m_iSelectionCooldownTicks buys; at its shipped value of 1 it is always true. It is asked ONLY
		// on the free-slot path: a reselect request means the map changed under the objective and is
		// answered immediately, above.
		if (liveAtTickStart < MaxConcurrentObjectives() && !alreadySelected && IsSelectionDue())
			SelectObjective();
	}

	//------------------------------------------------------------------------------------------------
	//! How many objectives may run at once, floored at one.
	//!
	//! ⚠ THE FLOOR IS NOT DEFENSIVE PROGRAMMING. An unauthored or mis-authored zero would stop the
	//! occupying faction ever choosing an objective again, with nothing in the log and no symptom a
	//! player could report - which is the exact failure mode this whole machine is built around
	//! avoiding. "Turn the director off" is not an authoring gesture this feature supports.
	//! \return At least 1.
	protected int MaxConcurrentObjectives()
	{
		if (m_iMaxConcurrentObjectives < 1)
			return 1;

		return m_iMaxConcurrentObjectives;
	}

	//------------------------------------------------------------------------------------------------
	//! ONE step of ONE objective: its aborts, then its conditions, then its operations.
	//!
	//! The order is the contract:
	//!  1. ABORTS, OR'd. Giving up is decided before anything is spent, so an objective that is already
	//!     lost does not buy one more squad on its way out.
	//!  2. CONDITIONS, AND'd. The gate out comes before the work: a town already below the threshold
	//!     should advance rather than buy a squad it no longer needs.
	//!  3. OPERATIONS, in authored order, first-to-act-wins - and ONLY on a tick that did not advance.
	//!     Dropping that last clause would run the NEW phase's work on the transition tick.
	//!
	//! 🔴 The module set is SNAPSHOTTED into a strong-ref local before any of it runs, and both halves
	//! matter. A module can end its own phase from inside its own method and two shipped ones do: the
	//! terminal battle operation resets the objective when its battle resolves, and the forward-base
	//! raise resets it when there is nowhere to put a base. Either swaps the instance's runtime array
	//! out from under this loop. Iterating the live array would walk a mutating collection; iterating a
	//! WEAK copy would free the very object whose method is on the stack.
	//!
	//! ⚠ The cadence and the idle clock are the RUNNER'S, and they bracket the modules. The countdown
	//! to the next operation is served FIRST, so every timer sits behind DirectorTick()'s three early
	//! returns by construction rather than by a rule each module has to remember. The idle clock is
	//! served LAST, because it is the only thing here that has to know whether the tick accomplished
	//! anything.
	//!
	//! 🔴 The abort fold runs TWICE on a tick that reaches the end, and that is what keeps the timing
	//! exact. An abort module asked only at the top of the tick can only ever see the PREVIOUS tick's
	//! clock, so the objective would be abandoned one in-game minute late and would get one more
	//! operation attempt it never used to get. Asking again after the clock is served costs nothing,
	//! because ShouldAbort() is side-effect free by contract.
	//! \param[in] instance The objective to step.
	protected void RunObjectivePhaseModules(notnull OVT_ObjectiveInstance instance)
	{
		array<ref OVT_BaseObjectiveModule> modules = new array<ref OVT_BaseObjectiveModule>();

		int count = instance.GetRuntimeModuleCount();
		for (int i = 0; i < count; i++)
		{
			OVT_BaseObjectiveModule module = instance.GetRuntimeModule(i);
			if (module)
				modules.Insert(module);
		}

		if (modules.IsEmpty())
		{
			LogObjectiveWithNoPlan(instance);
			return;
		}

		// Re-earned during this tick by CanSendObjectiveDeployment(), so it is only ever what the
		// most recent completed tick concluded.
		m_bBlockedOnAffordability = false;

		AdvanceOperationCadence();

		foreach (OVT_BaseObjectiveModule ticked : modules)
		{
			ticked.Tick();
		}

		if (RunObjectiveAbortModules(instance, modules))
			return;

		bool holdsIdleClock;
		string holdReason;
		if (RunObjectiveConditionModules(instance, modules, holdsIdleClock, holdReason))
			return;

		// ⚠ AT MOST ONE OPERATION, AND ONLY ONCE THE CADENCE HAS ELAPSED. The first module that acts
		// consumes the interval, whichever kind of work it was - which is what stops an objective
		// covered by three towers dropping three deployments in one in-game minute.
		int cadence = ResolveOperationCadence(instance);

		bool created = false;
		if (cadence >= 0 && m_Objective.nextOpTicks == 0)
			created = RunObjectiveOperationModules(instance, modules);

		// ⚠ RE-ARMED ONLY ON A SUCCESSFUL CREATE. Every refusal leaves the countdown at zero so the next
		// tick asks again a minute later, instead of waiting out another whole interval for a condition
		// that may have cleared immediately. That retry is also what makes the affordability hold cover
		// a whole poverty spell rather than one tick in forty-five.
		if (created)
			SetOperationCountdown(cadence);

		// 🔴 A condition may hold the idle clock, and exactly one shipped condition does (D17). The
		// daylight window is the objective waiting for something the occupying faction cannot influence;
		// spending its patience on that would abandon an objective FOR BEING DARK. Everything else on
		// this tick has already happened, so the forward base can still be starved out at 22:00. THE
		// HOLD IS THE CLOCK AND ONLY THE CLOCK.
		//
		// ⚠ The per-tick affordability flag is dropped with it, so a refusal seen during a wait cannot
		// leak into the next tick's decision. It is normally consumed by the clock, which is not served.
		if (holdsIdleClock)
		{
			LogIdleClockHold(holdReason);

			m_bBlockedOnAffordability = false;
			return;
		}

		if (TickObjectiveIdleClock(created))
			RunObjectiveAbortModules(instance, modules);
	}

	//------------------------------------------------------------------------------------------------
	//! How many in-game minutes this phase puts between operations.
	//!
	//! ⚠ An unresolvable cadence STOPS the phase spending, and that is the shipped behaviour: a world
	//! with no difficulty settings loaded (the World Editor, a broken campaign) buys nothing at all
	//! rather than buying on every tick. A phase that authors its own cadence needs no difficulty.
	//!
	//! ⚠ An authored ZERO is a legal, if aggressive, authoring gesture - "one operation every in-game
	//! minute" - and is honoured. Only the SENTINEL with nothing behind it refuses.
	//! \param[in] instance The objective, for its running phase.
	//! \return The interval in in-game minutes, or NO_CADENCE when it cannot be resolved at all.
	protected int ResolveOperationCadence(notnull OVT_ObjectiveInstance instance)
	{
		int authored = OVT_ObjectivePlanRules.USE_DIFFICULTY;

		OVT_ObjectiveConfig config = instance.GetConfig();
		if (config)
		{
			OVT_ObjectivePhase phase = config.GetPhase(instance.GetPhaseIndex());
			if (phase)
				authored = phase.m_iOperationCadence;
		}

		if (authored > OVT_ObjectivePlanRules.USE_DIFFICULTY)
			return authored;

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (!difficulty)
			return NO_CADENCE;

		return difficulty.objectiveHarassmentIntervalMinutes;
	}

	//------------------------------------------------------------------------------------------------
	//! Gives a freshly committed objective its breathing room: the in-game minutes it waits before it
	//! sends anything at all.
	//!
	//! ⚠ COMMIT ONLY, NOT PHASE ENTRY. The grace is what a player gets when the faction picks a NEW
	//! target - time to settle, build and repair before the first team arrives. A phase TRANSITION is
	//! the same objective escalating and is deliberately not slowed: the ramp's own cadence governs it.
	//!
	//! ⚠ IT IS SPENT WHETHER OR NOT THE FACTION COULD HAVE AFFORDED ANYTHING, because it arms the same
	//! countdown a successful operation arms and the tick serves that countdown unconditionally. A
	//! faction that was broke for the whole grace does not get the grace back.
	//!
	//! ⚠ A RESTORED OBJECTIVE KEEPS ITS SAVED COUNTDOWN. ApplyPersistedObjective() writes nextOpTicks
	//! directly rather than committing, so a save taken mid-grace comes back mid-grace and a save taken
	//! after it does not serve it again.
	protected void ArmFirstOperationDelay()
	{
		int delay = ResolveFirstOperationDelay();
		if (delay <= 0)
			return;

		SetOperationCountdown(delay);

		Print(LOG + "Objective '" + m_Objective.name + "': holding fire for " + delay.ToString() + " in-game minute(s) before the first operation (objectiveFirstOperationDelayMinutes)", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! How long a newly committed objective holds fire.
	//!
	//! ⚠ NO DIFFICULTY MEANS NO GRACE, which is the pre-setting behaviour and the safe direction to
	//! fail in: a world with nothing loaded (the World Editor, a test world) behaves exactly as it did
	//! before this setting existed rather than freezing the director for an unauthored number.
	//! \return In-game minutes to wait, or 0 when there is no grace to serve.
	protected int ResolveFirstOperationDelay()
	{
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (!difficulty)
			return 0;

		if (difficulty.objectiveFirstOperationDelayMinutes < 0)
			return 0;

		return difficulty.objectiveFirstOperationDelayMinutes;
	}

	//------------------------------------------------------------------------------------------------
	//! Says ONCE, per objective, that a live objective has no plan behind it and is therefore doing
	//! nothing at all.
	//!
	//! 🔴 This line IS the whole symptom, which is why it is loud. The plan registry is a .conf and no
	//! compiler reads a .conf - a mistyped path, a prefab that predates the attribute, or a mod that
	//! clears it all leave it unresolved, and every rule the occupying faction follows lives in it.
	//! There is no second implementation to fall back on. Nothing else in the machine says so: the
	//! objective is committed, the tick runs, the module set is simply empty - and an empty set carries
	//! no abort module either, so the idle clock cannot end it.
	//!
	//! ⚠ Once, not once per in-game minute. The latch is cleared when anything is committed to and with
	//! the objective record, so the next objective says it again if it is still true.
	//! \param[in] instance The objective with no runtime modules.
	protected void LogObjectiveWithNoPlan(notnull OVT_ObjectiveInstance instance)
	{
		if (m_bMissingPlanLogged)
			return;

		m_bMissingPlanLogged = true;

		string planName = instance.GetConfigName();
		if (planName == "")
			planName = "<none>";

		Print(LOG + "Objective '" + m_Objective.name + "' is running with NO PLAN behind it (plan '" + planName + "', phase '" + instance.GetPhaseName() + "') - the objective registry did not resolve, so this objective can neither act, advance nor be given up", LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	//! Asks every abort module in the phase and resets the objective if any of them fired.
	//!
	//! ⚠ THE FIRST TRUE ANSWER SUPPLIES THE REASON AND THE BLACKLIST FLAG, and every later one is
	//! still ASKED. Short-circuiting would hide a second, unrelated failure from a module that wanted to
	//! latch a log line about it; the OR itself is decided by the pure static, so the fold stays where
	//! every other fold in this feature is.
	//! \param[in] instance The objective being stepped.
	//! \param[in] modules The phase's runtime modules, snapshotted.
	//! \return True when the objective was reset, so the caller stops working on it this tick.
	protected bool RunObjectiveAbortModules(notnull OVT_ObjectiveInstance instance, notnull array<ref OVT_BaseObjectiveModule> modules)
	{
		array<bool> results = new array<bool>();

		string firstReason = "";
		bool firstBlacklist = false;

		foreach (OVT_BaseObjectiveModule module : modules)
		{
			OVT_BaseObjectiveAbortModule abortModule = OVT_BaseObjectiveAbortModule.Cast(module);
			if (!abortModule)
				continue;

			string reason;
			bool blacklist;
			bool fired = abortModule.ShouldAbort(reason, blacklist);

			results.Insert(fired);

			if (fired && firstReason == "")
			{
				firstReason = reason;
				firstBlacklist = blacklist;
			}
		}

		if (!OVT_ObjectivePlanRules.AnyAbort(results))
			return false;

		// A module that aborts without saying why still gets a line, because "the occupying faction
		// stopped attacking this place" always has to have an explanation in the log.
		if (firstReason == "")
			firstReason = "an abort module fired without giving a reason";

		ResetObjective(firstReason, firstBlacklist);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asks every condition module in the phase and advances if they all agreed.
	//!
	//! ⚠ An EMPTY condition set does not advance, even though AllConditionsMet([]) is true, and both
	//! statements are correct. The pure static answers "does anything BLOCK the advance", and nothing
	//! blocks an advance nobody gated. The RUNNER additionally requires that something actually gated
	//! it: a phase with no conditions ends on a terminal operation or on the idle clock, and advancing
	//! it every tick would run a whole plan to its last phase in three in-game minutes.
	//! \param[in] instance The objective being stepped.
	//! \param[in] modules The phase's runtime modules, snapshotted.
	//! \return True when the phase advanced or the objective ended, so no operation runs this tick.
	protected bool RunObjectiveConditionModules(notnull OVT_ObjectiveInstance instance, notnull array<ref OVT_BaseObjectiveModule> modules, out bool holdsClock, out string holdName)
	{
		holdsClock = false;
		holdName = "";

		array<bool> results = new array<bool>();

		// 🔴 The hold is "the only thing in the way is something we cannot influence", NOT "one of the
		// things in the way is". Holding on the first holding condition alone would stop the clock every
		// night whatever else was unfinished - and the objective's whole 240-minute backstop would only
		// be spent between 05:00 and 15:00, roughly two and a half times longer before a wedged
		// objective is ever given up.
		bool anyBlocked = false;
		bool everyBlockerHolds = true;

		foreach (OVT_BaseObjectiveModule module : modules)
		{
			OVT_BaseObjectiveConditionModule condition = OVT_BaseObjectiveConditionModule.Cast(module);
			if (!condition)
				continue;

			bool met = condition.Evaluate();
			results.Insert(met);

			if (met)
				continue;

			anyBlocked = true;

			if (!condition.HoldsIdleClock())
			{
				everyBlockerHolds = false;
				continue;
			}

			if (holdName == "")
				holdName = condition.m_sModuleName;
		}

		holdsClock = anyBlocked && everyBlockerHolds;
		if (!holdsClock)
			holdName = "";

		if (results.IsEmpty())
			return false;

		if (!OVT_ObjectivePlanRules.AllConditionsMet(results))
			return false;

		// Nothing is held on a tick that ADVANCES: the phase this belonged to is over.
		holdsClock = false;
		holdName = "";

		return AdvanceObjectivePhase(instance);
	}

	//------------------------------------------------------------------------------------------------
	//! Tries the phase's operation modules, stopping at the first that acts.
	//!
	//! ⚠ THE ORDER IS PARTLY RANDOM AND THAT IS DELIBERATE - see BuildOperationOrder() and
	//! OVT_BaseObjectiveOperationModule.ShufflesFreely(). The authored order still decides which
	//! operations are pinned ahead of the shuffle and how those are ordered among themselves.
	//! \param[in] instance The objective being stepped.
	//! \param[in] modules The phase's runtime modules, snapshotted.
	//! \return True when an operation was created and paid for.
	protected bool RunObjectiveOperationModules(notnull OVT_ObjectiveInstance instance, notnull array<ref OVT_BaseObjectiveModule> modules)
	{
		m_bOperationIntervalClaimed = false;

		array<OVT_BaseObjectiveOperationModule> ordered = new array<OVT_BaseObjectiveOperationModule>();
		BuildOperationOrder(modules, ordered);

		int offered = ordered.Count();

		for (int position = 0; position < offered; position++)
		{
			OVT_BaseObjectiveOperationModule operation = ordered[position];

			if (operation.TryAct())
			{
				// 🔴 "WHY DID IT DO THAT" MUST BE ANSWERABLE FROM THE LOG, and it stopped being so the
				// moment the order became random: a reader who knows the authored chain can no longer
				// derive which operation was asked first. One line per successful operation - which is at
				// most one per cadence per objective - names the winner and how many it beat.
				Print(LOG + "Objective '" + m_Objective.name + "' phase '" + instance.GetPhaseName() + "': ran operation '" + operation.m_sModuleName + "', drawn " + (position + 1).ToString() + " of " + offered.ToString() + " offered this cadence", LogLevel.NORMAL);

				return true;
			}

			// ⚠ A refusal that CLAIMED the interval stops the walk, and it is the only way one can. It
			// answers false - nothing created, nothing paid for, the cadence not re-armed, the tick counts
			// as idle - but nothing later in THIS cadence's draw gets to spend the interval it was saving.
			if (m_bOperationIntervalClaimed)
				return false;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! HOW MANY LIVING OCCUPYING-FACTION GROUPS ARE STANDING AT THIS PLACE - the campaign's one answer
	//! to "is this position still manned".
	//!
	//! It was two answers: the starvation abort has asked this since the forward base shipped and the
	//! anchor source provider now asks it too. A second copy of a rule this specific - occupying
	//! faction only, registered handles rather than agents, the survivor mask rather than a body count
	//! - is a rule that drifts.
	//!
	//! ⚠ Handles and the MASK, never agents. A dormant group has no agents and is perfectly alive; this
	//! is the "0 agents = dead" mistake the whole virtualization migration exists to have removed.
	//!
	//! \param[in] position The place to count around.
	//! \param[in] radius How far from it a deployment's groups count as ITS garrison, in metres.
	//! \return The number of registered groups there with at least one survivor.
	static int CountAliveOccupyingGroupsAt(vector position, float radius)
	{
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!deployments || !virtualization || !config)
			return 0;

		array<OVT_DeploymentComponent> nearby = deployments.GetDeploymentsInRadius(position, radius);
		if (!nearby)
			return 0;

		int occupyingIndex = config.GetOccupyingFactionIndex();
		int alive = 0;

		foreach (OVT_DeploymentComponent deployment : nearby)
		{
			if (!deployment)
				continue;

			if (deployment.GetControllingFaction() != occupyingIndex)
				continue;

			array<int> handles = new array<int>();

			array<OVT_BaseSpawningDeploymentModule> spawningModules = deployment.GetSpawningModules();
			foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
			{
				if (spawningModule)
					spawningModule.CollectRegisteredHandles(handles);
			}

			foreach (int handle : handles)
			{
				if (!virtualization.IsRegistered(handle))
					continue;

				if (virtualization.GetAliveMemberCount(handle) < 1)
					continue;

				alive++;
			}
		}

		return alive;
	}

	//------------------------------------------------------------------------------------------------
	//! THE ORDER THE PHASE'S OPERATIONS ARE ASKED IN THIS CADENCE: the pinned ones first in authored
	//! order, then everything else shuffled.
	//!
	//! WHY THERE IS A SHUFFLE AT ALL. *"We want some unpredictability about what the director does next
	//! otherwise the player learns his patterns and just follows a script to counter it."* A strict
	//! authored order with first-wins is a script - and worse, it STARVES: a forward-base phase was
	//! logged where the garrison won every cadence for ten minutes and sabotage was never once asked.
	//! ⚠ This is not the "no jitter" rule being broken: that governs OBJECTIVE SELECTION, where
	//! predictability is wanted. See OVT_BaseObjectiveOperationModule.ShufflesFreely().
	//!
	//! 🔴 WHY THE HEAD IS PINNED, and why the raise's own latch does not make it moot. The forward base
	//! latches itself - `if (m_Asset.up) return false;` - but only AFTER the base is standing. Before
	//! it is, OVT_RaiseForwardBaseObjectiveOperation is the one operation in the tree that calls
	//! ClaimOperationInterval(), and that claim is POSITIONAL by construction: it stops the walk, so it
	//! protects only what has not been asked YET. Shuffle the raise to the back and the money livelock
	//! returns exactly as recorded - the base costs 120, the pool holds 100, sabotage costs 100, so
	//! sabotage acts first, spends the pool, and the base is never affordable on any cadence. Pinning
	//! the raise ahead of the draw is NECESSARY while the base is unraised and FREE once it is up.
	//!
	//! ⚠ So the fix for starved sabotage is the shuffle of the TAIL, not a reorder of the head.
	//!
	//! ⚠ FISHER-YATES, AND THE BOUND IS THE TRAP. Math.RandomInt is MAX-EXCLUSIVE, so the roll is
	//! RandomInt(0, i + 1) to cover 0..i inclusive; RandomInt(0, 0) is an engine error and the loop
	//! condition `i > 0` is what makes it unreachable. This project has been bitten by both.
	//! \param[in] modules The phase's runtime modules, snapshotted, in authored order.
	//! \param[inout] ordered The caller's list, cleared and filled with this cadence's draw.
	protected void BuildOperationOrder(notnull array<ref OVT_BaseObjectiveModule> modules, notnull array<OVT_BaseObjectiveOperationModule> ordered)
	{
		ordered.Clear();

		array<OVT_BaseObjectiveOperationModule> free = new array<OVT_BaseObjectiveOperationModule>();

		foreach (OVT_BaseObjectiveModule module : modules)
		{
			OVT_BaseObjectiveOperationModule operation = OVT_BaseObjectiveOperationModule.Cast(module);
			if (!operation)
				continue;

			// The pinned head keeps its authored order; everything else goes into the draw.
			if (operation.ShufflesFreely())
				free.Insert(operation);
			else
				ordered.Insert(operation);
		}

		for (int i = free.Count() - 1; i > 0; i--)
		{
			int j = Math.RandomInt(0, i + 1);

			OVT_BaseObjectiveOperationModule swap = free[i];
			free[i] = free[j];
			free[j] = swap;
		}

		foreach (OVT_BaseObjectiveOperationModule drawn : free)
		{
			ordered.Insert(drawn);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! THE INTERVAL IS SPOKEN FOR: an operation module that did not act, but that nothing later in the
	//! authored order may be allowed to overtake this tick.
	//!
	//! 🔴 Two play-tests put this here and both were livelocks of the forward-base phase.
	//!   MONEY - the forward base was refused at 120 against a pool of 56, the walk fell through to
	//!   sabotage at 100, and from then on the faction bought a sabotage mission every time the pool
	//!   passed 100 and NEVER reached 120. The reserve floor named the LAST refused operation rather
	//!   than the first, and the floor alone would not have been enough anyway because it deliberately
	//!   does not govern this component's own spending.
	//!   HOUSEKEEPING - two branches of the forward-base send CLEAR THE WAY for a base rather than
	//!   sending one. Both delete something and answer false, and false let the walk fall through to a
	//!   cheaper operation which then armed the whole interval, pushing the base a full cadence away.
	//!
	//! ⚠ It is not a refusal and it is not a success. The cadence is NOT re-armed (so the next tick
	//! asks again a minute later), the idle clock is served exactly as it would have been, and the
	//! affordability hold still holds the clock through the director's own flag. It says one thing:
	//! not this tick, not by anyone else.
	//!
	//! ⚠ Per-walk, not a latch - RunObjectiveOperationModules() clears it before it asks anybody.
	//!
	//! ⚠ Nothing in the harassment phase claims, which is why the ramp still falls through from one
	//! operation to the next. The claim is authored doctrine's, not the runner's.
	//!
	//! ⚠ It is POSITIONAL, and since the order became random that has a consequence: "stop the walk"
	//! only protects what has not been asked yet, so an operation that claims MUST be asked before the
	//! operations it is saving the interval from. A claimer drawn last claims nothing. Any future
	//! operation that calls this method must pin itself the same way.
	void ClaimOperationInterval()
	{
		m_bOperationIntervalClaimed = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves an objective into the next phase of its plan, or ends it when there is no next phase.
	//!
	//! ⚠ A plan that runs out of phases ends the objective WITHOUT blacklisting it. Reaching the end of
	//! a plan is a plan COMPLETING, not a place failing, so the place is re-evaluated on its merits
	//! next round rather than sitting out one.
	//!
	//! 🔴 It says so in the log, and that line is not decoration. "Why did the occupying faction move
	//! to this phase" must always have an answer a server owner can read - it is the whole reason this
	//! machine replaced a pair of dice. The condition modules themselves stay silent, because a phase
	//! can be gated by several and only the transition is news.
	//! \param[in] instance The objective to advance.
	//! \return True always: either the phase changed or the objective ended, and both mean "no operation
	//!         this tick".
	protected bool AdvanceObjectivePhase(notnull OVT_ObjectiveInstance instance)
	{
		int next = instance.GetNextPhaseIndex();
		if (next < 0)
		{
			ResetObjective("the plan '" + instance.GetConfigName() + "' has no phase after '" + instance.GetPhaseName() + "' - the objective is finished", false);
			return true;
		}

		string leaving = instance.GetPhaseName();

		EnterObjectivePhaseIndex(instance, next);

		Print(LOG + "Objective '" + m_Objective.name + "' has met every condition of phase '" + leaving + "' on plan '" + instance.GetConfigName() + "' - entering '" + instance.GetPhaseName() + "'", LogLevel.NORMAL);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Acts on a pending re-selection request, or discards it.
	//!
	//! ⚠ THE FLAG IS ALWAYS CLEARED, EVEN WHEN IT IS NOT ACTED ON. From the forward-base phase onward
	//! the objective is LOCKED (the requirements are explicit), so a control change during those
	//! phases changes nothing - and keeping the flag would fire a stale re-selection at whatever
	//! moment the objective happened to end, minutes or hours later.
	//! \return True when a selection actually ran, so the caller does not run a second one this tick.
	protected bool ConsumeReselectRequest()
	{
		if (!m_bReselectPending)
			return false;

		m_bReselectPending = false;

		// ⚠ THE LOCK IS "PAST THE FIRST PHASE", ASKED OF THE PLAN RATHER THAN OF AN ENUM. No objective at
		// all answers -1 and the first phase answers 0, so both re-evaluate; anything from the second
		// phase onward is committed - it has a forward base going up or a battle mustering - and a
		// control change may not move it. That is the same boundary the phase enum drew, expressed in
		// the only terms a plan-driven machine has.
		if (GetObjectivePhaseIndex() > 0)
			return false;

		SelectObjective();

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! THE ONE PLACE THE DIRECTOR SPENDS ANYTHING, and the one place it may ever be done.
	//!
	//! ⚠ ForceCreateDeployment does NOT debit the pool. It forwards to CreateDeployment, which only
	//! STAMPS the cost onto the deployment so a refund and the Game Master snapshot read a real number.
	//! The evaluation path debits separately, right after its own create; a forcing caller has to do
	//! the same or the faction spends money it still has. This method is the whole of the director's
	//! side of G5 - "every resource leaves the pool exactly once, through SubtractFactionResources, at
	//! the moment the deployment is created".
	//!
	//! ⚠ Debit only AFTER a successful create. A create can be refused (an unregistered config name, a
	//! missing prefab, a failed spawn), and debiting a refusal would burn the faction's reserve on
	//! nothing, every tick, silently.
	//!
	//! 🔴 Every way out of here that is not a create says WHY. The phase machine retries a refusal every
	//! in-game minute forever, so silence here is a director that appears to have stopped.
	//! \param[in] deployments The deployment framework.
	//! \param[in] configName The registered config to run.
	//! \param[in] position Where to put it.
	//! \param[in] factionIndex The occupying faction.
	//! \param[in] yaw Which way the deployment marker faces. Defaults to unrotated, which is what every
	//!            operation but the forward base wants - only the forward base builds something with a
	//!            front.
	//! \return True when a deployment was created and the pool debited for it.
	bool CreateObjectiveDeployment(notnull OVT_DeploymentManagerComponent deployments, string configName, vector position, int factionIndex, float yaw = 0)
	{
		int cost;
		OVT_DeploymentConfig config = CanSendObjectiveDeployment(deployments, configName, factionIndex, cost);
		if (!config)
			return false;

		OVT_DeploymentComponent created = deployments.ForceCreateDeployment(config, position, factionIndex, cost, 0, yaw);
		if (!created)
		{
			// A fault, not a decision: an invalid config, a missing deployment prefab, a spawn the world
			// refused. ERROR rather than WARNING because nothing about it will clear on its own.
			LogOperationRefusal(configName, REFUSAL_FRAMEWORK_DECLINED, "at " + position.ToString(), LogLevel.ERROR);
			return false;
		}

		// ⚠ THE LINE THE WHOLE ACCOUNTING IDENTITY RESTS ON. See the header.
		deployments.SubtractFactionResources(factionIndex, cost);

		// AFTER the pool, never instead of it: this is a counter recording what already left.
		CountAssetSpend(cost);

		TrackObjectiveDeployment(configName, position);

		// The RESUME marker. Whatever stopped this config being bought has cleared, so the ledger forgets
		// its refusals and a later one - the same reason or a different one - is heard again rather than
		// swallowed by a latch set an hour ago.
		ForgetOperationRefusals(configName);

		Print(LOG + "Sent '" + configName + "' at " + position.ToString() + " for " + cost.ToString() + " resources", LogLevel.NORMAL);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! EVERY REASON THE DIRECTOR MAY NOT BUY THIS OPERATION RIGHT NOW, ASKED IN ONE PLACE AND ANSWERED
	//! OUT LOUD. Lifted out of CreateObjectiveDeployment() so a caller with expensive preparation can
	//! ask BEFORE it does it - the forward base's raise module used to run its whole forty-point
	//! terrain search, log a site, and only then discover the faction was twenty resources short.
	//!
	//! ⚠ The three refusals are ordered cheapest-question-first and each is latched on its own (config,
	//! reason) pair, so the pool being short on a sabotage team in Phase 1 can never silence the pool
	//! being short on a forward base in Phase 2.
	//!
	//! ⚠ It moves no money and creates nothing - it reads the pool and the ceiling and writes only the
	//! per-tick affordability flag and the refusal ledger. Asking twice in one tick is harmless.
	//! \param[in] deployments The deployment framework.
	//! \param[in] configName The registered config the caller wants to run.
	//! \param[in] factionIndex The occupying faction.
	//! \param[out] cost What it would cost. Zero when this returns null.
	//! \return The config to hand to ForceCreateDeployment(), or null when it may not be bought now.
	protected OVT_DeploymentConfig CanSendObjectiveDeployment(notnull OVT_DeploymentManagerComponent deployments, string configName, int factionIndex, out int cost)
	{
		cost = 0;

		OVT_DeploymentConfig config = deployments.FindConfigByName(configName);
		if (!config)
		{
			// ⚠ A FAULT, AND THE RAMP STOPS DEAD ON IT. A rung renamed in overthrowDeployments.conf and
			// not in HARASSMENT_LADDER lands here every in-game minute for the rest of the campaign.
			LogOperationRefusal(configName, REFUSAL_UNREGISTERED, "check overthrowDeployments.conf against the director's config-name constants", LogLevel.ERROR);
			return null;
		}

		cost = config.GetTotalResourceCost();

		// ⚠ The one refusal that is not the objective's fault, and the only one that raises the flag.
		// Being broke is a fact about the FACTION, and abandoning this objective for it would find the
		// next one exactly as unaffordable - the treadmill a play-test walked for 31 real minutes. The
		// other two refusals are either a fault to be fixed or a decision the director made about
		// itself, and a phase that can only ever hit one of those SHOULD time out.
		int pool = deployments.GetFactionResources(factionIndex);
		if (pool < cost)
		{
			m_bBlockedOnAffordability = true;

			// Named and priced, so the heartbeat below can say WHAT the phase is waiting to buy.
			m_sBlockedOnConfig = configName;
			m_iBlockedOnCost = cost;

			// 🔴 And the reserve floor, which is what makes the wait FINITE (D18). Without it the routine
			// evaluator drains every six-hourly credit within 30 seconds of its landing and the pool never
			// reaches this price - the play-test sat at 20 against a 120-cost forward base indefinitely.
			// The floor is exactly this cost, is asserted only while this ask keeps being refused, and
			// governs nothing this component itself buys.
			PushObjectiveReserve(configName, cost);

			LogOperationRefusal(configName, REFUSAL_POOL_SHORT, "it costs " + cost.ToString() + " and the pool holds " + pool.ToString(), LogLevel.WARNING);

			cost = 0;
			return null;
		}

		// ⚠ Two tests, not one, asking different questions: the pool test is "can the faction afford
		// this at all", the ceiling test is "has this forward base already had its share".
		//
		// ⚠ The ceiling is keyed on the PHASE, never on which operation is asking. It is inactive for
		// the whole of harassment, so Phase 1 spends against the pool alone, and active for the whole of
		// the forward-base phase INCLUDING the Phase 1 ramp operations that continue into it.
		if (!WithinAssetCeilings(configName, cost))
		{
			cost = 0;
			return null;
		}

		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! WHETHER TWO REFUSALS ARE THE SAME ENTRY IN THE LEDGER, and therefore whether the second one is
	//! silenced by the first.
	//!
	//! 🔴 The whole correction is in this one predicate, which is why it is a pure static and not an
	//! inline `&&`. The latch it replaced was keyed on the OBJECTIVE, so "the pool is short" about a
	//! sabotage team in Phase 1 silenced the same sentence about a forward base in Phase 2, twelve
	//! in-game hours later. Keyed on the pair, only a genuinely identical repeat is silenced.
	//! \param[in] configA First refusal's operation.
	//! \param[in] reasonA First refusal's reason.
	//! \param[in] configB Second refusal's operation.
	//! \param[in] reasonB Second refusal's reason.
	//! \return True only when both halves match - so a different operation, or a different reason for the
	//!         same operation, is a new entry that gets its own line.
	static bool IsSameRefusal(string configA, string reasonA, string configB, string reasonB)
	{
		if (configA != configB)
			return false;

		return reasonA == reasonB;
	}

	//------------------------------------------------------------------------------------------------
	//! Says ONCE, per objective AND per (config, reason), why an operation could not be sent.
	//!
	//! 🔴 The key is the PAIR. A latch keyed on the objective does not quieten a log - it hides the
	//! second fault behind the first: a forward-base phase re-sited the same point every ten seconds
	//! for five real minutes with nothing in the log, because the only affordability line had been
	//! spent twelve minutes earlier, in the previous phase, on a sabotage operation.
	//!
	//! ⚠ Latched at all because every refusal is retried every in-game minute: a refused create leaves
	//! the cadence at zero on purpose, so an unlatched line is hundreds of identical entries. The
	//! ledger is cleared when the objective ends and when the same config is successfully bought.
	//! \param[in] configName The operation that could not be sent.
	//! \param[in] reason One of the REFUSAL_* constants, and the second half of the latch key.
	//! \param[in] detail Numbers or advice for a reader. May be empty.
	//! \param[in] level What kind of entry this is.
	protected void LogOperationRefusal(string configName, string reason, string detail, LogLevel level)
	{
		if (!m_aRefusalConfigs || !m_aRefusalReasons)
			return;

		for (int i = 0; i < m_aRefusalConfigs.Count(); i++)
		{
			if (IsSameRefusal(m_aRefusalConfigs[i], m_aRefusalReasons[i], configName, reason))
				return;
		}

		m_aRefusalConfigs.Insert(configName);
		m_aRefusalReasons.Insert(reason);

		string line = LOG + "Objective '" + m_Objective.name + "' could not send '" + configName + "': " + reason;

		if (detail != "")
			line = line + " (" + detail + ")";

		Print(line + ". It will keep asking every in-game minute; this line repeats only if the reason changes or the operation is bought and then refused again", level);
	}

	//------------------------------------------------------------------------------------------------
	//! Forgets every refusal recorded against one config, so the next one is heard.
	//!
	//! ⚠ WALKED BACKWARDS. Both arrays are index-parallel and Remove() shifts everything after the hole,
	//! so a forward loop would step over the entry that moved into the gap.
	//! \param[in] configName The config whose refusals are no longer true.
	protected void ForgetOperationRefusals(string configName)
	{
		if (!m_aRefusalConfigs || !m_aRefusalReasons)
			return;

		for (int i = m_aRefusalConfigs.Count() - 1; i >= 0; i--)
		{
			if (m_aRefusalConfigs[i] != configName)
				continue;

			m_aRefusalConfigs.Remove(i);
			m_aRefusalReasons.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! How many refusals are currently latched. Public so a running campaign can be interrogated about
	//! why its ramp is quiet without reading the log back; the KEYING itself is asserted through the pure
	//! IsSameRefusal() predicate rather than by driving this ledger.
	//! \return The ledger's size.
	int GetLoggedRefusalCount()
	{
		if (!m_aRefusalConfigs)
			return 0;

		return m_aRefusalConfigs.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether one particular refusal has already been said out loud for this objective. Public for the
	//! same reason the count is.
	//! \param[in] configName The operation.
	//! \param[in] reason One of the REFUSAL_* constants.
	//! \return True when that exact pair is latched.
	bool HasLoggedRefusal(string configName, string reason)
	{
		if (!m_aRefusalConfigs || !m_aRefusalReasons)
			return false;

		for (int i = 0; i < m_aRefusalConfigs.Count(); i++)
		{
			if (m_aRefusalConfigs[i] == configName && m_aRefusalReasons[i] == reason)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	// WHERE THE FORWARD BASE GOES
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// THE FORWARD BASE ONCE IT IS STANDING
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// THE ONE TEARDOWN, AND THE ONE PENALTY
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// THE PLAYER-INITIATED EXIT - A FACADE OVER THE ASSET'S OWN MODULE
	//
	// 🔴 These four methods stay on the director and forward, rather than moving with the rest of the
	// forward base. Their two callers are a USER ACTION on the flag and the SERVER'S request validator,
	// and neither should have to know which module owns an asset or how to reach it.
	//
	// ⚠ The "one body, two entry points" rule is unchanged. The client asks about the ENTITY it is
	// attached to (CanDismantleFOBAt); the server asks about the position the DIRECTOR's record names
	// (CanDismantleFOB). Sharing the body is what stops the text a player reads and the rule the server
	// enforces from drifting apart.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether the forward base can be dismantled from a given position right now. THE SERVER'S ANSWER.
	//!
	//! ⚠ This overload reads the director's own record and is therefore server-only in practice. None
	//! of the forward-base state replicates (G12), so on a remote client the record is empty and this
	//! would refuse everything. The user action asks CanDismantleFOBAt() instead.
	//! \param[in] position Where the caller is standing.
	//! \param[out] refusal A localization key naming why, written only when this returns false.
	//! \return True when a dismantle would be allowed.
	bool CanDismantleFOB(vector position, out string refusal)
	{
		refusal = "";

		if (!m_FOB.up)
		{
			refusal = "#OVT-DismantleEnemyFOB_None";
			return false;
		}

		return CanDismantleFOBAt(position, m_FOB.position, refusal);
	}

	//------------------------------------------------------------------------------------------------
	//! The dismantle rule, asked about an arbitrary forward-base position.
	//!
	//! ⚠ IT FORWARDS TO A STATIC ON THE ASSET'S OWN MODULE, and the rule lives there with the rest of the
	//! forward base's doctrine. A STATIC rather than an instance method because THIS IS ASKED ON A
	//! CLIENT, where there is no objective, no plan and no runtime module set - see that method's header
	//! for why its two radii are constants and not authored values.
	//! \param[in] callerPosition Where the caller is standing.
	//! \param[in] fobPosition Where the forward base is.
	//! \param[out] refusal A localization key naming why, written only when this returns false.
	//! \return True when a dismantle would be allowed.
	bool CanDismantleFOBAt(vector callerPosition, vector fobPosition, out string refusal)
	{
		return OVT_RaiseForwardBaseObjectiveOperation.CanDismantleAt(callerPosition, fobPosition, refusal);
	}

	//------------------------------------------------------------------------------------------------
	//! How many occupying-faction soldiers are still on their feet near a position.
	//!
	//! Kept as a facade for the same reason the two above are: it is public, it is what the dismantle
	//! prompt counts, and its one implementation lives with the forward base's doctrine.
	//! \param[in] position The place to count around.
	//! \param[in] radius How far, in metres.
	//! \return The count.
	int CountOccupyingDefendersNear(vector position, float radius)
	{
		return OVT_RaiseForwardBaseObjectiveOperation.CountOccupyingDefendersNear(position, radius);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: a player has pulled the forward base's flag down.
	//!
	//! ⚠ The one exit that COSTS the occupying faction resources. The penalty is what the base cost to
	//! raise, taken back out of the deployment pool, so clearing one is worth doing rather than merely
	//! satisfying. SubtractFactionResources floors the pool at zero.
	//!
	//! ⚠ The amount is the ASSET MODULE'S, not a second reading of the difficulty setting - a doctrine
	//! that authors a cheaper forward base must be refunded a cheaper one. With no module registered it
	//! falls back to the campaign's own figure, which is what the authored default resolves to anyway.
	//!
	//! ⚠ It SUBTRACTS. It never adds. Grepping this directory for the deployment manager's credit
	//! method finds nothing, and must go on finding nothing - the name is deliberately not written out
	//! even in a comment, because that grep is an acceptance criterion and a quotation would defeat it.
	//!
	//! \param[in] callerPosition Where the player asking is standing. Re-validated here; the client's own
	//!            check is a courtesy and is not trusted.
	//! \return An empty string when the base came down, or the localization key naming the refusal.
	string OnFOBDismantledByPlayer(vector callerPosition)
	{
		if (!Replication.IsServer())
			return "#OVT-DismantleEnemyFOB_None";

		string refusal;
		if (!CanDismantleFOB(callerPosition, refusal))
			return refusal;

		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		int penalty = 0;

		OVT_RaiseForwardBaseObjectiveOperation raise = OVT_RaiseForwardBaseObjectiveOperation.Cast(GetAssetModule(ASSET_FOB));
		if (raise)
		{
			penalty = raise.GetDismantlePenalty();
		}
		else
		{
			OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
			if (difficulty)
				penalty = difficulty.objectiveFOBCost;
		}

		if (deployments && config && penalty > 0)
			deployments.SubtractFactionResources(config.GetOccupyingFactionIndex(), penalty);

		ResetObjective("the resistance cleared its forward base and dismantled it", true);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	// THE ASSET REGISTRY - GENERIC, KEYED, AND IT NEVER NAMES A DOCTRINE
	//
	// Everything below answers a question about "whatever this objective has standing" by asking the
	// module that built it. The director knows three things about an asset: that its ceiling may be
	// armed, what that ceiling is, and how to tell it to take itself down. A checkpoint asset adds a
	// key and a module and nothing here changes.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Records which module owns a standing asset, so the one teardown path can reach it from any phase.
	//!
	//! ⚠ Called from the module's own entry, and the registration outlives the phase deliberately. It is
	//! dropped when the objective record is cleared, the one funnel every "there is no objective now"
	//! path comes through.
	//! \param[in] key The asset key.
	//! \param[in] module The module that owns it.
	void RegisterAssetModule(string key, OVT_BaseObjectiveAssetModule module)
	{
		if (key == "" || !module || !m_mAssetModules)
			return;

		m_mAssetModules.Set(key, module);
	}

	//------------------------------------------------------------------------------------------------
	//! The module that owns a standing asset.
	//! \param[in] key The asset key.
	//! \return The module, or null when nothing has claimed that key this objective.
	OVT_BaseObjectiveAssetModule GetAssetModule(string key)
	{
		OVT_BaseObjectiveAssetModule module;
		if (!m_mAssetModules || !m_mAssetModules.Find(key, module))
			return null;

		return module;
	}

	//------------------------------------------------------------------------------------------------
	//! Tells every asset this objective has standing to take itself out of the world.
	//!
	//! ⚠ THE ONE TEARDOWN, REACHED FROM ResetObjective() AND FROM CommitObjective(), and idempotent on
	//! both. An objective that never raised anything registered no module and this does nothing at all.
	protected void TearDownObjectiveAssets()
	{
		if (!m_mAssetModules)
			return;

		foreach (string key, OVT_BaseObjectiveAssetModule module : m_mAssetModules)
		{
			if (module)
				module.TearDownAsset();
		}

		// The bias is dropped here so this method is complete on its own. ClearObjectiveRecord() drops
		// it too, on every path that ends an objective - both are idempotent, and a teardown that
		// silently depended on its caller to finish the job is how the anchor was left pointing at an
		// abandoned objective in the first place (see ResetObjective).
		DropObjectiveAnchor();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether any standing asset's spend ceiling governs the director's spending right now.
	//!
	//! ⚠ PUBLIC BECAUSE IT IS THE ONE THING ABOUT THE CEILING THAT CAN BE ASSERTED WITHOUT SPENDING
	//! REAL RESOURCES: it arms when the asset's own deployment is SENT and disarms when the objective's
	//! record is cleared, and neither transition has any other symptom.
	//! \return True while spends are counted against a ceiling.
	bool IsAssetCeilingArmed()
	{
		if (!m_mAssetModules)
			return false;

		foreach (string key, OVT_BaseObjectiveAssetModule module : m_mAssetModules)
		{
			if (module && module.IsCeilingArmed())
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether one more spend of this size is still inside every armed asset ceiling.
	//!
	//! ⚠ The prospective spend is added BEFORE the test, because WithinFOBCeiling asks "would this
	//! total take me past the ceiling" and is inclusive at it. Its two-argument signature is pinned by
	//! the logic tier and is deliberately not widened here.
	//!
	//! ⚠ It moves no money - the ceiling is a COUNTER of what has already left the one pool.
	//!
	//! 🔴 A spent ceiling deliberately does NOT set m_bBlockedOnAffordability. Being broke is a fact
	//! about the FACTION and holds the idle clock; a spent ceiling is a decision the machine made about
	//! ITSELF, and a phase that can only ever hit that SHOULD run its clock down and be abandoned.
	//! \param[in] configName What is being bought, for the refusal ledger's latch key.
	//! \param[in] cost What is about to be spent.
	//! \return True when the spend is permitted.
	protected bool WithinAssetCeilings(string configName, int cost)
	{
		if (!m_mAssetModules)
			return true;

		foreach (string key, OVT_BaseObjectiveAssetModule module : m_mAssetModules)
		{
			if (!module || !module.IsCeilingArmed())
				continue;

			int ceiling = module.GetCeiling();
			int spent = module.GetSpent();

			if (OVT_ObjectivePhaseRules.WithinFOBCeiling(spent + cost, ceiling))
				continue;

			LogOperationRefusal(configName, REFUSAL_FOB_CEILING, spent.ToString() + " of " + ceiling.ToString() + " already spent, and this would add " + cost.ToString(), LogLevel.NORMAL);

			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Counts a spend that has ALREADY left the pool against every armed asset ceiling.
	//! \param[in] cost What was spent.
	protected void CountAssetSpend(int cost)
	{
		if (cost <= 0 || !m_mAssetModules)
			return;

		foreach (string key, OVT_BaseObjectiveAssetModule module : m_mAssetModules)
		{
			if (module && module.IsCeilingArmed())
				module.CountSpend(cost);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! An asset is standing. Called by the deployment-side module that built it, once.
	//!
	//! ⚠ It RECORDS, it does not DECIDE - the same rule ReportObjectiveProgress() carries. This is
	//! public and is called from a deployment's own update; every transition in this machine happens on
	//! DirectorTick(), behind its three early returns.
	//!
	//! ⚠ No notification is sent (D12). The forward base is the one thing in the ramp the resistance is
	//! meant to discover rather than be told about. An explicit requirement, not an oversight.
	//!
	//! ⚠ The spend counter SURVIVES. What has already been spent getting the asset up is part of its
	//! budget and this is not a fresh record.
	//! \param[in] key The asset key.
	//! \param[in] position Where the structure stands.
	//! \param[in] sourceBasePosition Where its supply line starts. Zero keeps whatever was recorded when
	//!            the deployment was sent, which is the better answer for a walking insertion that never
	//!            resolved a source of its own.
	//! \param[in] deploymentName The config carrying it - the re-link key written into the save.
	void ReportAssetRaised(string key, vector position, vector sourceBasePosition, string deploymentName)
	{
		if (m_Objective.kind == OVT_EObjectiveKind.NONE || !m_Instance)
			return;

		OVT_ObjectiveAssetRecord asset = m_Instance.GetAsset(key);
		if (!asset)
			return;

		vector source = sourceBasePosition;
		if (source == vector.Zero)
			source = asset.sourceBasePosition;

		asset.up = true;
		asset.position = position;
		asset.sourceBasePosition = source;
		asset.deploymentName = deploymentName;
		asset.starvationTicks = 0;

		OVT_BaseObjectiveAssetModule module = GetAssetModule(key);
		OVT_RaiseForwardBaseObjectiveOperation raise = OVT_RaiseForwardBaseObjectiveOperation.Cast(module);
		if (raise)
			raise.OnAssetRaised(position);

		Print(LOG + "Objective '" + m_Objective.name + "': the forward operating base is standing at " + position.ToString(), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Counts resources already spent from the deployment pool against an asset's ceiling.
	//!
	//! ⚠ THIS MOVES NO MONEY. It is called AFTER the pool has been debited, and its only job is to stop
	//! the ramp spending past the ceiling. See the record's own header.
	//! \param[in] key The asset key.
	//! \param[in] amount What was spent. Non-positive is ignored.
	void AddAssetSpend(string key, int amount)
	{
		if (amount <= 0 || !m_Instance)
			return;

		OVT_ObjectiveAssetRecord asset = m_Instance.GetAsset(key);
		if (!asset)
			return;

		asset.spent = asset.spent + amount;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets how many consecutive ticks an asset has been cut off.
	//! \param[in] key The asset key.
	//! \param[in] ticks The count. Negative reads as zero.
	void SetAssetStarvationTicks(string key, int ticks)
	{
		if (!m_Instance)
			return;

		OVT_ObjectiveAssetRecord asset = m_Instance.GetAsset(key);
		if (!asset)
			return;

		if (ticks < 0)
			asset.starvationTicks = 0;
		else
			asset.starvationTicks = ticks;
	}

	//------------------------------------------------------------------------------------------------
	//! The pre-flight an operation module with expensive preparation asks BEFORE it does any of it.
	//!
	//! ⚠ IT MOVES NO MONEY AND CREATES NOTHING, and it is the same three refusals, latched the same way,
	//! that the create itself would make. Asking twice in one tick is harmless: the second ask is silent
	//! because the latch is already set.
	//! \param[in] deployments The deployment framework.
	//! \param[in] configName The registered config the caller wants to run.
	//! \param[in] factionIndex The occupying faction.
	//! \return True when it could be bought right now.
	bool CanAffordObjectiveDeployment(notnull OVT_DeploymentManagerComponent deployments, string configName, int factionIndex)
	{
		int cost;

		return CanSendObjectiveDeployment(deployments, configName, factionIndex, cost) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Says ONCE, per objective AND per (config, reason), why an operation could not be sent.
	//!
	//! ⚠ THE PUBLIC DOOR ONTO THE REFUSAL LEDGER, for a module that refuses for a reason the create
	//! choke point never sees - the forward base having no supply line to site along is the shipped one.
	//! The dedup rule and its latch key are unchanged; see LogOperationRefusal().
	//! \param[in] configName The operation that was refused.
	//! \param[in] reason One of the REFUSAL_* constants.
	//! \param[in] detail Anything specific to this refusal, for the log line.
	//! \param[in] level How loudly to say it.
	void LogObjectiveRefusal(string configName, string reason, string detail, LogLevel level)
	{
		LogOperationRefusal(configName, reason, detail, level);
	}

	//! \return Whether the most recent create pre-flight was refused for want of resources. Read by an
	//!         operation module deciding whether its refusal claims the interval - see
	//!         ClaimOperationInterval().
	bool IsBlockedOnAffordability() { return m_bBlockedOnAffordability; }

	//------------------------------------------------------------------------------------------------
	// THE OBJECTIVE'S OWN COUNTDOWNS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Serves one tick of the objective's idle clock - the clock the DIRECTOR runs against itself.
	protected void AdvancePhaseTimeout()
	{
		m_Objective.phaseTicks = OVT_ObjectivePhaseRules.TickDown(m_Objective.phaseTicks);
	}

	//------------------------------------------------------------------------------------------------
	//! Serves one tick of the countdown to the next operation at the objective.
	protected void AdvanceOperationCadence()
	{
		m_Objective.nextOpTicks = OVT_ObjectivePhaseRules.TickDown(m_Objective.nextOpTicks);
	}

	//------------------------------------------------------------------------------------------------
	// THE IDLE CLOCK
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! 🔴 THE BACKSTOP, and the whole of what "the machine never wedges" (R1) rests on. Called LAST by
	//! each ramp phase handler, once per tick, with the one thing it cannot work out for itself:
	//! whether this tick created an operation.
	//!
	//! It used to be a phase BUDGET and is now an IDLE clock, because a play-test proved the budget
	//! measured the wrong thing: the objective was selected at 13:03, could not afford anything for 31
	//! real minutes, finally created an operation at 13:34 - and the phase "ran out of time" at 13:43,
	//! deleting a walking team five minutes into a fifteen-minute walk and re-selecting the same base.
	//! Nothing in that sequence is a wedge; the director was broke, then it was working.
	//!
	//! Four answers, in this order, and the order is the contract:
	//!  1. NO OBJECTIVE - nothing to time out. A sender may have reset the objective on this very tick,
	//!     and running the clock against a cleared record would call ResetObjective() a second time.
	//!  2. PROGRESS - an operation was created this tick, or a completed one has reported since we last
	//!     looked. The clock goes back to full.
	//!  3. AN OPERATION IN FLIGHT - men this director sent are alive and on their way. That IS the
	//!     objective working, and deleting a team five minutes short of its target is wrong however
	//!     the clock came to run out.
	//!  4. BLOCKED ONLY BY THE POOL - held, and said out loud on a heartbeat.
	//! Only a tick that is none of those four serves a round, and only such a tick can end an objective.
	//!
	//! ⚠ An objective that can NEVER be afforded is the one state this method deliberately lets
	//! persist. It SITS: the clock is held, and abandoning it would find the next one exactly as
	//! unaffordable. While it sits nothing accumulates and nothing leaks - no deployment is created, no
	//! resource moves, the teardown ledger does not grow, and the cadence stays at zero so the next
	//! tick simply asks again.
	//!
	//! ⚠ It decides nothing by itself: it returns a verdict and the CALLER resets, so every ending
	//! stays on DirectorTick() behind its three early returns.
	//! \param[in] created Whether this tick created and paid for an operation.
	//! \return True when the objective has been idle for its whole budget and must be abandoned.
	protected bool TickObjectiveIdleClock(bool created)
	{
		bool blocked = m_bBlockedOnAffordability;
		m_bBlockedOnAffordability = false;

		// ⚠ THE HEARTBEAT COUNTS CONSECUTIVE HELD TICKS, so a tick that was not blocked at all breaks the
		// run wherever it lands - before any of the early returns below, because the run has to be broken
		// even on a tick that returns for some other reason entirely.
		if (!blocked)
			m_iAffordabilityHeldTicks = 0;

		if (m_Objective.kind == OVT_EObjectiveKind.NONE)
			return false;

		// ⚠ CONSUMED UNCONDITIONALLY, NEVER SHORT-CIRCUITED PAST. The marks have to move even on a tick
		// that already counts as progress for another reason, or the NEXT tick would read the same
		// completed operation as fresh news.
		bool reported = ConsumeReportedOperations();

		if (created || reported)
		{
			RearmObjectiveIdleClock();
			return false;
		}

		if (HasOperationInFlight())
		{
			RearmObjectiveIdleClock();
			return false;
		}

		if (blocked)
		{
			LogAffordabilityBlock();
			return false;
		}

		AdvancePhaseTimeout();

		return m_Objective.phaseTicks == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether either success counter has moved since the idle clock was last re-armed, consuming the
	//! news either way.
	//!
	//! ⚠ A PULL, not a push, and that is D4. ReportObjectiveProgress() only counts - it is public and
	//! is called from a deployment's own update, from a restore and from fixtures. Comparing the
	//! counters here puts the observation on the tick, where every other decision in this machine lives.
	//!
	//! ⚠ It COMPARES rather than subtracts, so a counter that went DOWN (a fresh commit zeroes both)
	//! also reads as news and re-syncs, instead of leaving a mark permanently above the counter.
	//! \return True when an operation has reported since the last re-arm.
	protected bool ConsumeReportedOperations()
	{
		if (GetHarassmentSuccesses() == m_iProgressHarassmentMark && GetSabotageSuccesses() == m_iProgressSabotageMark)
			return false;

		SyncProgressMarks();

		return true;
	}

	//! Puts the idle clock back to its full budget and re-baselines the success marks with it.
	//!
	//! ⚠ AND BREAKS THE AFFORDABILITY HEARTBEAT'S RUN. A tick can be BOTH blocked and productive - the
	//! tower recapture sender can be refused on cost and the sabotage sender succeed in the same call
	//! chain - and a heartbeat that kept counting through that would report a hold that is not happening.
	protected void RearmObjectiveIdleClock()
	{
		m_iAffordabilityHeldTicks = 0;

		SetPhaseTimeout(m_iPhaseTimeoutTicks);
	}

	//! Records the success counters as "already seen", so nothing already banked reads as fresh progress.
	protected void SyncProgressMarks()
	{
		m_iProgressHarassmentMark = GetHarassmentSuccesses();
		m_iProgressSabotageMark = GetSabotageSuccesses();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether an operation THIS DIRECTOR sent is still alive and still on its way.
	//!
	//! 🔴 The headline half of the idle-clock fix. A sabotage team with 1561 m left to walk was deleted,
	//! with its truck, because a clock the team had nothing to do with ran out. Men who are walking to
	//! a target are the objective WORKING.
	//!
	//! ⚠ What counts is scoped by IsObjectiveOperationConfig(), and the scope is load-bearing. If a
	//! standing forward base or its garrison counted, the forward-base phase could never time out at
	//! all and R1 would be gone - a base that is up with a full garrison and a counter-attack gate that
	//! will never open is exactly the wedge the backstop exists to catch.
	//!
	//! ⚠ Ramp operations DO count in the forward-base phase and the phase can still time out, because
	//! an operation is TRANSIENT: it completes, it is wiped out, or its condition collects it. A
	//! forward base that has spent its whole ceiling creates nothing more and runs the clock down.
	//!
	//! ⚠ A force that was WIPED OUT is not in flight. Its marker can outlive it, and reading a dead
	//! team as work in progress would hold the clock open forever on a corpse.
	//!
	//! ⚠ Read from the live deployment list through the teardown LEDGER - the one count in this
	//! component that legitimately walks m_aCreatedDeployments, because the question is "is something
	//! WE sent still out there" and only the ledger knows the difference.
	//! \return True when at least one operation the director created is alive and unfinished.
	protected bool HasOperationInFlight()
	{
		if (!m_aCreatedDeployments || m_aCreatedDeployments.IsEmpty())
			return false;

		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (!deployments)
			return false;

		foreach (OVT_ObjectiveDeploymentRef created : m_aCreatedDeployments)
		{
			if (!created)
				continue;

			if (!IsObjectiveOperationConfig(created.configName))
				continue;

			OVT_DeploymentComponent deployment = deployments.GetDeploymentNearPosition(created.configName, created.position, TEARDOWN_LOOKUP_RADIUS);
			if (!deployment)
				continue;

			if (deployment.GetSpawnedUnitsEliminated())
				continue;

			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a config the director creates is an OPERATION - men sent somewhere to do a job and then
	//! finish - rather than a standing installation.
	//!
	//! ⚠ One predicate, two uses, and the fact that they are the same question is the point. "Is this
	//! operation still in flight" and "is tearing this down a RECALL rather than a write-off" are the
	//! same statement about the same deployment, so they can never disagree.
	//!
	//! ⚠ The supply party is an operation ONLY until the flag is up. Before the raise it is a truck on
	//! a road; after it, it is the standing installation the whole phase is built around and must
	//! neither hold the clock nor pay anything back. m_FOB.up is read live for that reason, and
	//! ResetObjective() tears the ledger down BEFORE ClearFOBRecord() so this still reads the truth.
	//!
	//! ⚠ The garrison is never one - it is bought FOR a base that is already standing.
	//! \param[in] configName The config name as the ledger recorded it.
	//! \return True for the harassment ladder, tower recapture, sabotage, and an unraised forward base.
	protected bool IsObjectiveOperationConfig(string configName)
	{
		if (configName == "")
			return false;

		if (HARASSMENT_LADDER.Find(configName) != -1)
			return true;

		if (configName == TOWER_RECAPTURE_CONFIG)
			return true;

		if (configName == SABOTAGE_CONFIG)
			return true;

		if (configName == FOB_CONFIG)
			return !m_FOB.up;

		// FOB_GARRISON_CONFIG, and anything a later phase appends without deciding what it means. A
		// config nobody has classified fails safe to "not an operation": it neither holds the backstop
		// open nor pays anything back, which are the two ways an unclassified entry could do harm.
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Says, on a heartbeat, that the ramp is working but the faction cannot pay for it.
	//!
	//! ⚠ Mandatory. The first play-test this came out of spent 31 real minutes watching a director that
	//! had selected a target and then appeared to do nothing, with not one line in the log to explain
	//! it.
	//!
	//! 🔴 A heartbeat rather than a one-shot, because once was not enough either: the second play-test
	//! had this line at 15:30 and a forward-base phase visibly broken from 15:43 onward, and the two
	//! were about different operations twelve in-game hours apart. The repeat carries the elapsed count
	//! and the operation being waited for, which makes "held, still broke" readable as a state rather
	//! than inferable from an absence.
	//!
	//! ⚠ It reports the HOLD, not the refusal. Which operation was refused and why is
	//! LogOperationRefusal()'s line; this one is about the CLOCK and would be wrong to key on a config.
	protected void LogAffordabilityBlock()
	{
		m_iAffordabilityHeldTicks = m_iAffordabilityHeldTicks + 1;

		// The first held tick speaks, then one in every AFFORDABILITY_HEARTBEAT_TICKS after it.
		if (m_iAffordabilityHeldTicks > 1 && (m_iAffordabilityHeldTicks - 1) % AFFORDABILITY_HEARTBEAT_TICKS != 0)
			return;

		string waiting = "its next operation";
		if (m_sBlockedOnConfig != "")
			waiting = "'" + m_sBlockedOnConfig + "' (" + m_iBlockedOnCost.ToString() + " resources)";

		Print(LOG + "Objective '" + m_Objective.name + "' has been unable to afford " + waiting + " for " + m_iAffordabilityHeldTicks.ToString() + " in-game minute(s). Its idle clock is HELD while it waits - being broke is not a failure of the objective, and abandoning it would only find the next objective just as unaffordable. It resumes on the first tick the pool can cover one; the next \"Sent ...\" line is that moment", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Says ONCE, per phase entry, that the objective is waiting on something it cannot influence.
	//!
	//! ⚠ The RUNNER's line and not the module's, deliberately. A condition module cannot see the
	//! others, so a condition that logged its own wait would say it on every tick it was false - a
	//! forward-base phase whose ramp is nowhere near done would announce "waiting for daylight" the
	//! first time night fell. The runner is the only thing that knows the hold ACTUALLY APPLIES.
	//!
	//! ⚠ Once per phase entry, not per tick (D17). This is a once-a-minute tick and a daylight wait can
	//! last most of an in-game day.
	//!
	//! ⚠ It does not say the phase has STOPPED, because it has not: only the idle clock is held. Every
	//! abort module is still asked and every operation still tried, so a forward base cut off during
	//! the wait still comes down during the wait.
	//! \param[in] moduleName The authored name of the condition holding the clock, for the log line.
	protected void LogIdleClockHold(string moduleName)
	{
		if (m_bIdleHoldLogged)
			return;

		m_bIdleHoldLogged = true;

		string what = moduleName;
		if (what == "")
			what = "a condition it cannot influence";

		Print(LOG + "Objective '" + m_Objective.name + "' has done everything it can and is waiting on '" + what + "'. Its idle clock is HELD while it waits - a wait nobody can shorten is not a failure of the objective - but nothing else is: its operations still run and whatever it has standing can still be taken off it", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	// SELECTION
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Chooses the objective: the best candidate of the best PLAN.
	//!
	//! Deliberately omniscient and deliberately PREDICTABLE. There is no fog of war and no randomness
	//! at all - an experienced player is supposed to be able to guess the target from the map. Every
	//! score is a plain weighted sum, every tie breaks on the authored order, and the winner AND the
	//! runner-up are both logged with their scores. The only roll anywhere in this path is a plan's own
	//! m_fChance, and a plan authoring 100 (both shipped ones do) never reaches the generator.
	//!
	//! HOW THIS REPRODUCES THE SINGLE LIST IT REPLACED, EXACTLY. Four properties, all load-bearing:
	//!   1. ONE CANDIDATE COLLECTION, IN THE SAME ORDER - towns then bases, each in its registry's own
	//!      order (OVT_ObjectiveCandidateSet is the only thing here that looks at the world).
	//!   2. THE SHIPPED SELECTORS ARE THE OLD SCORERS, term for term and in the same order of addition,
	//!      with the eight weights lifted to attributes whose defaults ARE the constants.
	//!   3. EQUAL PRIORITIES MULTIPLY BY ONE. Both shipped plans author m_fPriority 1, and score * 1.0
	//!      is exact in binary floating point, so a plan's rank IS its selector's score.
	//!   4. THE TIE-BREAKS AGREE - first candidate at a given score, first plan at a given rank, and
	//!      the town plan authored first exactly as towns were collected first.
	//! ⚠ The four shipped candidate kinds are DISJOINT, so "best plan by its best candidate" and "best
	//! candidate over one list" are the same argmax here. A registry whose plans OVERLAP is supported
	//! and is where the two forms could differ; the plan wins, because a doctrine's priority is meant
	//! to be able to out-rank a slightly better target it has no doctrine for.
	//!
	//! Villages, forward bases and radio towers are excluded in the candidate COLLECTION rather than in
	//! a selector - that is a statement about what the world offers, not about what a doctrine values.
	void SelectObjective()
	{
		int startTick = System.GetTickCount();

		// ⚠ RE-ARMED HERE RATHER THAN AT THE CALL SITE, so every path that runs a round - the idle
		// slot, the reselect flag, a test driving it directly - pays the same cooldown. Nothing else
		// writes this counter.
		m_iSelectionCooldown = SelectionCooldownTicks() - 1;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!occupying || !towns || !config)
			return;

		int resistanceIndex = config.GetPlayerFactionIndex();

		array<OVT_ObjectiveConfig> plans = new array<OVT_ObjectiveConfig>();
		CollectEligiblePlans(plans);

		// ⚠ ONE PASS OVER THE WORLD FOR EVERY PLAN (D6). The union of the eligible plans' declared
		// sources is collected once and every selector is handed the same set, so the cost that would
		// otherwise multiply with the plan count is paid once. A round with no eligible plan at all
		// still collects the sources the STRANGLER FALLBACK needs, because that path has to be able to
		// pick a target when the registry did not load.
		int sources = OVT_EObjectiveCandidateSource.RESISTANCE_TOWNS | OVT_EObjectiveCandidateSource.RESISTANCE_BASES;
		if (!plans.IsEmpty())
			sources = UnionOfCandidateSources(plans);

		OVT_ObjectiveCandidateSet candidates = new OVT_ObjectiveCandidateSet();
		candidates.Collect(occupying, towns, sources, resistanceIndex, m_fMaxUsefulDistance);

		array<string> blacklistKeys = new array<string>();
		array<int> blacklistRounds = new array<int>();
		ReadBlacklist(blacklistKeys, blacklistRounds);
		candidates.ApplyBlacklist(blacklistKeys, blacklistRounds);

		// THE BEST RANK ANY PLAN GAVE EACH CANDIDATE, and which plan gave it. Built alongside the
		// per-plan pick because it costs nothing there and it is what lets the log name the runner-up
		// across the WHOLE map rather than only within the winning doctrine - which is what the single
		// list's log meant and what a tuner is actually reading it for.
		array<float> candidateRanks = new array<float>();
		array<bool> unclaimed = new array<bool>();
		SeedCandidateRanks(candidates, candidateRanks, unclaimed);

		array<float> planScores = new array<float>();
		array<bool> planEligible = new array<bool>();
		array<int> planPick = new array<int>();

		foreach (OVT_ObjectiveConfig plan : plans)
		{
			array<float> scores = new array<float>();
			plan.m_Selector.ScoreCandidates(candidates, scores);

			array<bool> mask = new array<bool>();
			candidates.BuildSelectionMask(plan.GetCandidateSources(), mask);

			// ⚠ RAGGED INPUT IS THE ONE THING A SELECTOR CAN GET WRONG THAT NOTHING ELSE WOULD CATCH.
			// The pure static refuses a mis-aligned pair outright rather than picking through it, and
			// a modder's selector that returned the wrong number of scores would otherwise commit to a
			// candidate that was supposed to be masked out.
			int best = OVT_ObjectiveSelection.NOTHING_TO_SELECT;
			if (scores.Count() == mask.Count())
				best = OVT_ObjectiveSelection.SelectBestIndex(scores, mask);

			float rank = 0;
			if (best != OVT_ObjectiveSelection.NOTHING_TO_SELECT)
				rank = OVT_ObjectivePlanRules.ResolvePlanScore(scores[best], plan.m_fPriority);

			planScores.Insert(rank);
			planEligible.Insert(best != OVT_ObjectiveSelection.NOTHING_TO_SELECT);
			planPick.Insert(best);

			FoldCandidateRanks(candidates, plan, scores, mask, candidateRanks, unclaimed);
		}

		int winningPlan = OVT_ObjectivePlanRules.SelectBestPlanIndex(planScores, planEligible);

		int best = OVT_ObjectiveSelection.NOTHING_TO_SELECT;
		OVT_ObjectiveConfig winner;

		if (winningPlan != OVT_ObjectivePlanRules.NOTHING_TO_SELECT)
		{
			winner = plans[winningPlan];
			best = planPick[winningPlan];
		}
		// 🔴 And there is no other way to pick one. The doctrine is authored data and there is no second
		// implementation, so a registry that did not load selects NOTHING and says so through the round's
		// own log line. An occupying faction that has stopped choosing objectives is the one failure mode
		// this machine exists to make visible.

		// ONE ROUND SERVED PER SELECTION ROUND, and it is served AFTER the pick, not before: an
		// objective blacklisted for one round has to actually miss this round.
		ServeBlacklistRound();

		LogSelectionRound(config, candidates.Count(), plans.Count(), System.GetTickCount() - startTick);

		if (best == OVT_ObjectiveSelection.NOTHING_TO_SELECT)
		{
			EnterIdle();
			return;
		}

		OVT_EObjectiveKind kind = OVT_EObjectiveKind.TOWN;
		if (candidates.GetKind(best) == OVT_EObjectiveKind.BASE)
			kind = OVT_EObjectiveKind.BASE;

		LogSelection(winner, best, candidates, candidateRanks, unclaimed);

		CommitObjective(kind, candidates.GetPosition(best), candidates.GetName(best), winner);
	}

	//------------------------------------------------------------------------------------------------
	//! Every plan that may compete for an objective on this round.
	//!
	//! Four gates, cheapest-first: validation, faction, instance cap, then the chance roll. ⚠ The roll
	//! is last on purpose - a plan that was never going to be eligible must not consume a random draw,
	//! because the moment selection's answer depends on how many dice were thrown earlier the whole
	//! path stops being reproducible from the map.
	//!
	//! ⚠ A plan with no selector is not eligible even if the validator never ran: the initialisation
	//! tier's worlds never call PostGameStart(), so the skipped list may legitimately be empty in a
	//! world with a broken registry, and the null check is what keeps that from being a crash.
	//! \param[out] plans Receives the eligible plans, in registry order. Cleared first.
	protected void CollectEligiblePlans(notnull array<OVT_ObjectiveConfig> plans)
	{
		plans.Clear();

		if (!m_Registry)
			return;

		int count = m_Registry.GetConfigCount();
		for (int i = 0; i < count; i++)
		{
			OVT_ObjectiveConfig plan = m_Registry.GetConfig(i);
			if (!plan || !plan.m_Selector)
				continue;

			if (m_Registry.IsSkipped(plan.m_sObjectiveName))
				continue;

			if (!plan.CanFactionUse(OVT_FactionType.OCCUPYING_FACTION))
				continue;

			if (CountInstancesOfPlan(plan.m_sObjectiveName) >= plan.m_iMaxInstances)
				continue;

			// Mirrors the deployment framework's own roll (OVT_DeploymentManager.c:1778) exactly,
			// short-circuit included: a plan at 100 never touches the generator.
			if (plan.m_fChance < 100.0 && s_AIRandomGenerator.RandFloatXY(0, 100) > plan.m_fChance)
				continue;

			plans.Insert(plan);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! How many live objectives are already running a plan, NOT counting the one this round would
	//! replace.
	//!
	//! ⚠ The exclusion is not an optimisation, it is the difference between a cap and a DEADLOCK. A
	//! re-selection request runs a full round while an objective is still LIVE and commits over the top
	//! of m_Instance. Counting that instance would make a plan at its cap ineligible to be re-picked by
	//! the very round that was about to free the slot, so a town changing hands would silently stop the
	//! town doctrine ever being chosen again.
	//!
	//! At the shipped concurrency of one objective this always answers zero, which is the honest
	//! answer. The cap is headroom for N > 1.
	//! \param[in] planName The plan's persistence key.
	//! \return The count, which is compared against the plan's m_iMaxInstances.
	protected int CountInstancesOfPlan(string planName)
	{
		int running = 0;

		foreach (OVT_ObjectiveInstance instance : m_aInstances)
		{
			if (!instance || instance == m_Instance)
				continue;

			if (instance.GetConfigName() == planName)
				running = running + 1;
		}

		return running;
	}

	//------------------------------------------------------------------------------------------------
	//! The union of every eligible plan's declared candidate sources.
	//!
	//! ⚠ THIS IS THE ECONOMY OF D6 EXPRESSED IN ONE LINE PER PLAN. A registry with no base doctrine in
	//! it never walks the base registry; a registry with ten town doctrines walks the town registry
	//! once. The trade-off is stated in the decision record: one exotic plan widens the collection for
	//! everybody, which is acceptable because the flag set makes that cost legible in the .conf.
	//! \param[in] plans The eligible plans.
	//! \return OVT_EObjectiveCandidateSource flags.
	protected int UnionOfCandidateSources(notnull array<OVT_ObjectiveConfig> plans)
	{
		int sources = 0;

		foreach (OVT_ObjectiveConfig plan : plans)
		{
			sources = sources | plan.GetCandidateSources();
		}

		return sources;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the per-candidate rank table at "nothing has claimed this".
	//! \param[in] candidates The round's candidate set.
	//! \param[out] ranks One rank per candidate, all zero. Cleared first.
	//! \param[out] unclaimed One flag per candidate, all true. Cleared first.
	protected void SeedCandidateRanks(notnull OVT_ObjectiveCandidateSet candidates, notnull array<float> ranks, notnull array<bool> unclaimed)
	{
		ranks.Clear();
		unclaimed.Clear();

		int count = candidates.Count();
		for (int i = 0; i < count; i++)
		{
			ranks.Insert(0);
			unclaimed.Insert(true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Folds one plan's scores into the per-candidate rank table, keeping the best rank per candidate.
	//!
	//! ⚠ IT STORES THE PLAN-SCALED RANK, NOT THE RAW SELECTOR SCORE. The table is what the log's
	//! runner-up is read out of, and a runner-up quoted in raw score would disagree with the pick the
	//! moment two plans had different priorities - which is exactly when a tuner is reading the line.
	//! \param[in] candidates The round's candidate set.
	//! \param[in] plan The plan being folded in.
	//! \param[in] scores That plan's scores, one per candidate.
	//! \param[in] mask That plan's selection mask - a masked candidate contributes nothing.
	//! \param[inout] ranks The rank table.
	//! \param[inout] unclaimed The "nothing has claimed this" flags.
	protected void FoldCandidateRanks(notnull OVT_ObjectiveCandidateSet candidates, notnull OVT_ObjectiveConfig plan, notnull array<float> scores, notnull array<bool> mask, notnull array<float> ranks, notnull array<bool> unclaimed)
	{
		int count = candidates.Count();
		if (scores.Count() != count || mask.Count() != count || ranks.Count() != count)
			return;

		for (int i = 0; i < count; i++)
		{
			if (mask[i])
				continue;

			float rank = OVT_ObjectivePlanRules.ResolvePlanScore(scores[i], plan.m_fPriority);

			// Strictly greater-than, so the FIRST plan to claim a candidate at a given rank keeps it -
			// the same tie rule as everywhere else in this path.
			if (unclaimed[i] || rank > ranks[i])
				ranks[i] = rank;

			unclaimed[i] = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	// THE SELECTION CADENCE (D6)
	//------------------------------------------------------------------------------------------------

	//! \return In-game minutes between idle selection rounds. 1 - the shipped value - is every tick.
	protected int SelectionCooldownTicks()
	{
		if (!m_Registry)
			return 1;

		return m_Registry.GetSelectionCooldownTicks();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether an idle tick may run a selection round, serving one tick of the cooldown when it may not.
	//!
	//! ⚠ AT THE SHIPPED VALUE OF 1 THIS IS ALWAYS TRUE AND THE COUNTER NEVER LEAVES ZERO, which is what
	//! makes the cadence attribute a no-op by default and the pre-plan behaviour byte-identical. The
	//! counter is re-armed inside SelectObjective(), so the reselect flag - which calls it directly -
	//! also pays the cooldown for the ticks that follow, and a map change is never made to wait.
	//! \return True when a round is due.
	protected bool IsSelectionDue()
	{
		if (m_iSelectionCooldown > 0)
		{
			m_iSelectionCooldown = m_iSelectionCooldown - 1;
			return false;
		}

		return true;
	}

	//! \return Ticks left before an idle tick may run another selection round.
	int GetSelectionCooldown() { return m_iSelectionCooldown; }

	//------------------------------------------------------------------------------------------------
	//! One line per selection round, behind the campaign's existing debug flag: how much work the round
	//! actually did and how long it took.
	//!
	//! ⚠ Behind the debug flag and nowhere else. This runs once per in-game minute in every live
	//! campaign, and the whole argument for collecting candidates once is that the cost is invisible; a
	//! line per minute in a normal server's log would cost more than the thing it measures.
	protected void LogSelectionRound(OVT_OverthrowConfigComponent config, int candidateCount, int planCount, int elapsedMs)
	{
		if (!config || !config.m_bDebugMode)
			return;

		Print(LOG + "Selection round: " + candidateCount.ToString() + " candidate(s) x " + planCount.ToString() + " plan(s) in " + elapsedMs.ToString() + " ms", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Which plan an objective committed WITHOUT one runs - resolved from the registry by target kind.
	//!
	//! ⚠ It is NOT a kind-to-name table. The strangler's lookup this replaced held two plan NAMES as
	//! constants and chose between them by kind, a hard-coded assumption about the shipped registry.
	//! This asks the registry instead: the first eligible plan whose SELECTOR declares it can score
	//! that kind. On the shipped registry it answers exactly what the name lookup did.
	//!
	//! ⚠ Selection never uses this. A round that picked a plan passes that plan to CommitObjective()
	//! directly, because "the first plan that could score this kind" and "the plan that actually won"
	//! are different statements the moment two plans claim one source. This is only for a commit from
	//! outside selection - a test fixture, a scripted scenario, or a restore.
	//! \param[in] kind What kind of place the objective is.
	//! \return The plan, or null when no registry is wired or no plan can describe that kind.
	protected OVT_ObjectiveConfig ResolvePlanForKind(OVT_EObjectiveKind kind)
	{
		if (!m_Registry)
			return null;

		int source = OVT_ObjectiveCandidateSet.SourceForKind(kind);
		if (source == 0)
			return null;

		int count = m_Registry.GetConfigCount();
		for (int i = 0; i < count; i++)
		{
			OVT_ObjectiveConfig plan = m_Registry.GetConfig(i);
			if (!plan || !plan.m_Selector)
				continue;

			if (m_Registry.IsSkipped(plan.m_sObjectiveName))
				continue;

			if (!plan.CanFactionUse(OVT_FactionType.OCCUPYING_FACTION))
				continue;

			if ((plan.GetCandidateSources() & source) == 0)
				continue;

			return plan;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Commits to a candidate and enters the harassment phase.
	//!
	//! Public because it is also how a restored payload and a scripted scenario put the machine into a
	//! known state - the point of every mutation going through one method is that there is only one
	//! place that decides what "a fresh objective" means.
	//!
	//! ⚠ The plan comes IN, it is not looked up. A selection round has already decided which doctrine
	//! won; re-deriving one from the target's KIND would be a different answer the moment two plans
	//! claim one source, and would silently commit the objective to the wrong doctrine while the log
	//! named the right one. A caller that is not a selection round passes null and gets
	//! ResolvePlanForKind()'s answer.
	//! \param[in] kind What kind of place it is.
	//! \param[in] position Where it is.
	//! \param[in] name Display name for logs and the GM panel.
	//! \param[in] plan The doctrine to run. Null resolves one from the registry by kind.
	void CommitObjective(OVT_EObjectiveKind kind, vector position, string name, OVT_ObjectiveConfig plan = null)
	{
		if (kind == OVT_EObjectiveKind.NONE)
		{
			EnterIdle();
			return;
		}

		m_Objective.kind = kind;
		m_Objective.position = position;
		m_Objective.name = name;

		// ⚠ THE BAG IS EMPTIED BEFORE THE PLAN IS BOUND, NOT AFTER. Every key in it is a fact about the
		// objective that just ended - operations completed, a forward base's spend - and carrying one
		// into a new objective would be a counter nobody earned. This is what zeroing the two success
		// fields used to do, generalised: the two counters are bag keys now.
		if (m_Instance)
		{
			m_Instance.ClearBags();

			OVT_ObjectiveConfig committed = plan;
			if (!committed)
				committed = ResolvePlanForKind(kind);

			m_Instance.SetConfig(committed);
		}

		m_bMissingPlanLogged = false;

		// ⚠ THE LIVE LIST IS WHAT "THERE IS AN OBJECTIVE" MEANS TO THE TICK, so membership is granted
		// here, at the one commit funnel, and revoked in ClearObjectiveRecordFields(), at the one clear
		// funnel. The instance object itself is allocated once and outlives every objective it runs.
		if (m_aInstances && m_Instance && m_aInstances.Find(m_Instance) == -1)
			m_aInstances.Insert(m_Instance);

		// ⚠ Torn down, not merely forgotten. A forward base only exists in the phase that is locked
		// against re-selection, so in the live machine there is never one standing here - but clearing
		// the RECORD alone would leave a structure and a garrison in the world with nothing pointing at
		// them. Cheap: an asset module whose asset was never sent takes itself down with no queries.
		TearDownObjectiveAssets();
		ClearFOBRecord();

		// ⚠ AND THE OWNERS DROPPED WITH THE ASSETS THEY OWNED. A module registered by the objective that
		// just ended would otherwise arm a spend ceiling for a forward base this one has not built, and
		// hand the next teardown a module whose record has already been zeroed. EnterPhase() below
		// re-registers whatever the NEW objective's first phase owns.
		if (m_mAssetModules)
			m_mAssetModules.Clear();

		m_aCreatedDeployments.Clear();

		m_bIdleLogged = false;

		// A pending re-selection request is ANSWERED by committing: the request means "the map changed,
		// re-evaluate the target", and this is the re-evaluation. Leaving it set would fire a second,
		// immediate selection on the next tick against a map nothing had changed in since.
		m_bReselectPending = false;

		EnterObjectivePhaseIndex(m_Instance, 0);

		// ⚠ AFTER THE PHASE ENTRY, NEVER BEFORE. EnterObjectivePhase() zeroes the cadence on every entry,
		// so a grace armed ahead of it would be wiped by the entry it was armed for.
		ArmFirstOperationDelay();
	}

	//------------------------------------------------------------------------------------------------
	//! Drops back to having no objective, logging the fact exactly once.
	//!
	//! ONCE, NOT EVERY TICK. An early campaign in which the resistance holds nothing is the normal
	//! state, not a fault, and a line per in-game minute would bury everything else in the log. The
	//! latch clears the moment anything is committed to.
	protected void EnterIdle()
	{
		ClearObjectiveRecord();

		if (m_bIdleLogged)
			return;

		m_bIdleLogged = true;
		Print(LOG + "No objective: the resistance holds nothing worth taking back yet", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Logs the winning PLAN, the winning candidate and the runner-up, all with their scores.
	//!
	//! The runner-up is not decoration. Predictability is a stated requirement (G1), and the only way a
	//! tuner can check that the weights order candidates the way the design intends is to see what came
	//! second and by how much. It is found by re-running the same pure selection over the same rank
	//! table with the winner masked out, so the log can never disagree with the pick.
	//!
	//! ⚠ The runner-up is across the WHOLE MAP, not within the winning plan - the rank table holds the
	//! best plan-scaled rank any plan gave each candidate, so "ahead of" names the place that would
	//! actually have been attacked instead, including one belonging to a rival doctrine.
	//!
	//! ⚠ It names the plan, and the plan may be NULL. Nothing in the live machine commits without one,
	//! but a commit from outside a selection round still reaches this line, and saying "NO PLAN" is
	//! what makes that visible at the moment it first matters.
	//! \param[in] plan The winning plan, or null for a commit that did not come from a plan.
	//! \param[in] best Index of the chosen candidate.
	//! \param[in] candidates The round's candidate set.
	//! \param[in] ranks The per-candidate rank table.
	//! \param[in] unclaimed Which candidates no plan claimed, which masks them out of the runner-up.
	protected void LogSelection(OVT_ObjectiveConfig plan, int best, notnull OVT_ObjectiveCandidateSet candidates, notnull array<float> ranks, notnull array<bool> unclaimed)
	{
		if (!candidates.IsValidIndex(best) || ranks.Count() != candidates.Count() || unclaimed.Count() != candidates.Count())
			return;

		string kindLabel = "town";
		if (candidates.GetKind(best) == OVT_EObjectiveKind.BASE)
			kindLabel = "base";

		string planLabel = "NO PLAN (the registry did not load)";
		if (plan)
			planLabel = "'" + plan.m_sObjectiveName + "'";

		string line = LOG + "Objective: " + planLabel + " on " + kindLabel + " '" + candidates.GetName(best) + "' at score " + ranks[best].ToString();

		array<bool> masked = new array<bool>();
		foreach (bool flag : unclaimed)
		{
			masked.Insert(flag);
		}

		int count = candidates.Count();
		for (int i = 0; i < count; i++)
		{
			if (candidates.IsBlacklisted(i))
				masked[i] = true;
		}

		masked[best] = true;

		int runnerUp = OVT_ObjectiveSelection.SelectBestIndex(ranks, masked);
		if (runnerUp != OVT_ObjectiveSelection.NOTHING_TO_SELECT)
			line = line + ", ahead of '" + candidates.GetName(runnerUp) + "' at " + ranks[runnerUp].ToString();
		else
			line = line + " (the only candidate)";

		Print(line, LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	// PHASES
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Moves the objective into a phase and re-arms its timeout.
	//!
	//! ⚠ Every phase entry goes through here, so every phase gets an exit for free. A phase that could
	//! be entered without arming the timeout is a phase the machine can wedge in.
	//!
	//! ⚠ It is the ONE funnel, and that is what made the strangler safe: a second entry path would be a
	//! phase whose modules, clock and bias disagree with the phase the record says it is in.
	//!
	//! ⚠ It takes an AUTHORED phase name. The plan is now the only thing that knows what phases exist,
	//! which is what lets a mod ship a doctrine with four phases and still be driven, restored and
	//! displayed by the same runner. An EMPTY name means "no objective"; a name the running plan does
	//! not carry is REFUSED rather than silently entering phase 0, because phase 0 is a real phase.
	//! \param[in] phaseName The authored m_sPhaseName to enter, or "" to drop to no objective.
	void EnterPhase(string phaseName)
	{
		if (phaseName == "")
		{
			EnterIdle();
			return;
		}

		int index = IndexOfObjectivePhase(phaseName);
		if (index == OVT_ObjectivePlanRules.NO_PHASE_INDEX)
		{
			Print(LOG + "Refusing to enter phase '" + phaseName + "': the plan '" + GetObjectiveConfigName() + "' the current objective is running does not carry a phase by that name", LogLevel.ERROR);
			return;
		}

		EnterObjectivePhaseIndex(m_Instance, index);
	}

	//------------------------------------------------------------------------------------------------
	//! Moves an objective into one of its plan's phases, by index.
	//! \param[in] instance The objective.
	//! \param[in] index Index into the plan's m_aPhases.
	protected void EnterObjectivePhaseIndex(notnull OVT_ObjectiveInstance instance, int index)
	{
		EnterObjectivePhase(instance, index);
	}

	//------------------------------------------------------------------------------------------------
	//! THE ONE PHASE-ENTRY BODY. Re-arms the idle clock, zeroes the cadence, pushes the bias, and swaps
	//! the runtime module set.
	//!
	//! ⚠ A transition is PROGRESS, which is why the arm goes through SetPhaseTimeout() rather than
	//! writing the field: the idle clock's progress marks have to be re-baselined with it, or the
	//! successes that OPENED this gate would read as fresh news on the first tick of the new phase.
	//! That is the reason this method may not be inlined into its callers.
	//!
	//! ⚠ The module swap is LAST, after the timers and the anchor, so an incoming module's OnEnter()
	//! sees the phase it is actually in rather than the tail end of the phase it replaced.
	//! \param[in] instance The objective. Null is tolerated so a component with no instance yet still
	//!            behaves; nothing in the live machine passes one.
	//! \param[in] index Index into the plan's phases, or -1 when the plan has no phase for this entry.
	protected void EnterObjectivePhase(OVT_ObjectiveInstance instance, int index)
	{
		OVT_ObjectivePhase authored;
		string phaseName = "";

		if (instance)
		{
			OVT_ObjectiveConfig config = instance.GetConfig();
			if (config)
			{
				authored = config.GetPhase(index);
				if (authored)
					phaseName = authored.m_sPhaseName;
			}

			instance.RecordPhase(index, phaseName);
		}

		SetPhaseTimeout(ResolvePhaseIdleTimeout(authored));
		m_Objective.nextOpTicks = 0;

		// ⚠ THE "WAITING ON SOMETHING IT CANNOT INFLUENCE" LATCH IS PER PHASE, NOT PER OBJECTIVE. A
		// later phase has its own conditions and its own reasons to wait, and inheriting the previous
		// phase's silence would leave the one state a reader most needs explained unexplained.
		m_bIdleHoldLogged = false;

		// The bias widens with the phase, so it is re-pushed on every entry rather than only on the
		// first. Committing to an objective enters the first phase through here too, which is why this
		// is the only push site the live machine needs.
		PushObjectiveAnchor();

		if (instance)
			instance.EnterRuntimePhase(authored);
	}

	//------------------------------------------------------------------------------------------------
	//! How many in-game minutes of patience a phase gets, authored or inherited.
	//! \param[in] phase The authored phase, or null.
	//! \return The phase's own m_iIdleTimeoutTicks when it authors one, otherwise the director's
	//!         m_iPhaseTimeoutTicks - which is what every phase shared before plans existed.
	protected int ResolvePhaseIdleTimeout(OVT_ObjectivePhase phase)
	{
		if (!phase)
			return m_iPhaseTimeoutTicks;

		return OVT_ObjectivePlanRules.ResolveWithDifficulty(phase.m_iIdleTimeoutTicks, m_iPhaseTimeoutTicks);
	}

	//------------------------------------------------------------------------------------------------
	//! How far the deployment bias reaches right now: the running phase's authored radius, or the
	//! hard-coded one it inherits.
	//!
	//! ⚠ The authored value wins only when it IS authored. A phase that authors -1, and any phase of a
	//! plan with no radii at all, gets DEFAULT_ANCHOR_RADIUS.
	//! \return A radius in metres, or zero for a phase that carries no bias at all.
	protected float ResolveObjectiveAnchorRadius()
	{
		if (m_Instance)
		{
			OVT_ObjectiveConfig config = m_Instance.GetConfig();
			if (config)
			{
				OVT_ObjectivePhase authored = config.GetPhase(m_Instance.GetPhaseIndex());
				if (authored && authored.m_fAnchorRadius >= 0)
					return authored.m_fAnchorRadius;
			}
		}

		if (m_Objective.kind == OVT_EObjectiveKind.NONE)
			return 0;

		return DEFAULT_ANCHOR_RADIUS;
	}

	//------------------------------------------------------------------------------------------------
	// THE OBJECTIVE ANCHOR - HOW THE REST OF THE CAMPAIGN LEARNS WHERE THE OBJECTIVE IS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Points the occupying faction's ROUTINE deployment spending at the current objective.
	//!
	//! ⚠ The dependency points ONE WAY, and that is the whole design (D5). This component pushes a
	//! position, a radius and a weight into the deployment framework; the framework has never heard of
	//! this component and asks it nothing. So the evaluator stays testable with no director present,
	//! its hottest loop gains no null-check, and a faction nobody biases behaves as it did before.
	//!
	//! ⚠ It biases ORDERING, never ELIGIBILITY. The weight is added to a candidate's sort key only. It
	//! cannot make an unsuitable config suitable, raise the per-faction ceiling or the per-pass cap, or
	//! conjure resources. A saturated map still buys nothing; it buys near the objective first.
	//!
	//! ⚠ This is NOT how the director's own operations are created - those are created explicitly,
	//! because their cadence and their ramp are this component's to own. The anchor only leans on the
	//! ROUTINE spending the evaluator was doing anyway.
	protected void PushObjectiveAnchor()
	{
		if (!Replication.IsServer())
			return;

		if (m_Objective.kind == OVT_EObjectiveKind.NONE)
		{
			DropObjectiveAnchor();
			return;
		}

		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!deployments || !config)
			return;

		// A NEGATIVE INDEX IS NOT A FACTION. See DropObjectiveAnchor() for why the config can answer
		// "cannot tell yet"; storing an anchor under it would file a bias against a faction that does
		// not exist, where nothing would ever read it and nothing would ever clear it.
		int occupyingIndex = config.GetOccupyingFactionIndex();
		if (occupyingIndex < 0)
			return;

		deployments.SetObjectiveAnchor(occupyingIndex, m_Objective.position, ResolveObjectiveAnchorRadius(), m_fObjectiveAnchorWeight);
	}

	//------------------------------------------------------------------------------------------------
	//! Stops the occupying faction leaning on a place it is no longer working toward.
	//!
	//! Idempotent and safe with no objective, no deployment framework and no campaign.
	//!
	//! ⚠ A non-null config is not enough. GetOccupyingFactionIndex() resolves through the FACTION
	//! MANAGER, a different object with a different lifetime, and answers -1 for "cannot tell yet". The
	//! index is read into a local and checked before it can reach the store, because a negative number
	//! is a perfectly valid map key and would file a bias against a faction that does not exist.
	protected void DropObjectiveAnchor()
	{
		if (!Replication.IsServer())
			return;

		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!deployments || !config)
			return;

		int occupyingIndex = config.GetOccupyingFactionIndex();
		if (occupyingIndex < 0)
			return;

		deployments.ClearObjectiveAnchor(occupyingIndex);
	}

	//------------------------------------------------------------------------------------------------
	// THE RESERVE FLOOR - HOW THE OBJECTIVE STOPS ROUTINE SPENDING OUTBIDDING IT
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Keeps the price of the operation this director just tried and failed to buy out of the ROUTINE
	//! evaluator's reach, until it can buy it or stops wanting it.
	//!
	//! 🔴 THE PRIORITY INVERSION THIS CLOSES (D18). The pool is credited in a lump every six in-game
	//! hours and two spenders race for it. The routine evaluator may create ten deployments in one
	//! 30-second pass; this director takes one operation per cadence interval. With nothing earmarked,
	//! routine garrisoning drained each credit before the pool ever reached the price of the forward
	//! base - 20 in the pool against a 120-cost operation, indefinitely, with nothing failing anywhere.
	//!
	//! ⚠ The number is the `cost` local of the ask that was just refused. This component does not
	//! compute or predict "what would I like to buy next": it reserves for something it ALREADY ASKED
	//! FOR and was told it could not afford. A phase whose senders all decline for some other reason
	//! never reaches the ask, so it never reserves - which is correct.
	//!
	//! ⚠ One operation deep and RE-EARNED EVERY TICK, which is what makes a deadlock impossible.
	//! DirectorTick() DROPS the floor on its first line, so the only way one exists at the end of a
	//! tick is that this tick asked and was refused for money. Every way an objective ends, every tick
	//! that does not ask, every refusal that is not about money, and every early return therefore
	//! leaves the store empty. A floor can never outlive the intent by more than the one tick it is
	//! asserted on, and the re-push cadence is three times the evaluator's own 30-second pass.
	//!
	//! ⚠ It does NOT govern this component's own spending, and must not. The director buys through
	//! ForceCreateDeployment() + SubtractFactionResources(), neither of which consults the floor, and
	//! CanSendObjectiveDeployment() reads the RAW pool. Reserving against itself would be a deadlock by
	//! construction - the floor exists so the money is there when the director asks again.
	//! \param[in] configName What the money is being kept for. Diagnostics only, but always supplied.
	//! \param[in] cost The price of that operation.
	protected void PushObjectiveReserve(string configName, int cost)
	{
		if (!Replication.IsServer())
			return;

		if (cost <= 0)
		{
			DropObjectiveReserve();
			return;
		}

		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!deployments || !config)
			return;

		// A NEGATIVE INDEX IS NOT A FACTION - the same guard, for the same reason, as the anchor push.
		// A floor filed under an impossible key would never be read and never be cleared.
		int occupyingIndex = config.GetOccupyingFactionIndex();
		if (occupyingIndex < 0)
			return;

		deployments.SetObjectiveReserve(occupyingIndex, configName, cost);

		m_bReserveHeld = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Gives the whole pool back to routine spending.
	//!
	//! Idempotent and safe with no objective, no deployment framework and no campaign.
	//!
	//! ⚠ The cache is dropped WHATEVER happens, including on the paths that could not reach the store.
	//! Leaving it set after a failed resolve would make the NEXT drop skip itself, which is the one way
	//! a floor could survive a teardown. A spurious clear only costs one redundant Remove().
	protected void DropObjectiveReserve()
	{
		if (!Replication.IsServer())
			return;

		// Nothing has ever been pushed, so there is nothing to resolve two managers for. See
		// m_bReserveHeld - this is the branch that runs on almost every tick of almost every campaign.
		if (!m_bReserveHeld)
			return;

		m_bReserveHeld = false;

		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!deployments || !config)
			return;

		int occupyingIndex = config.GetOccupyingFactionIndex();
		if (occupyingIndex < 0)
			return;

		deployments.ClearObjectiveReserve(occupyingIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! THE ONE RESET PATH - every failure and every ending comes through here.
	//!
	//! In order: the objective's own deployments are taken back down, the forward-base record is
	//! cleared, the objective is optionally sent to the back of the queue for a round, and the machine
	//! goes idle. The reason is ALWAYS logged at warning level: "the occupying faction never attacks
	//! town X" has to have an explanation in a log rather than a repro.
	//!
	//! ⚠ The bias is dropped one level down, in ClearObjectiveRecord(), NOT here. There is a second way
	//! to end up with no objective: a re-selection that runs during harassment and finds nothing
	//! selectable goes EnterIdle() -> ClearObjectiveRecord() without ever reaching this method.
	//! \param[in] reason Why the objective ended. Named in the log.
	//! \param[in] blacklist Whether the place should sit out a selection round. A FAILURE blacklists;
	//! a resolved battle does not - the objective simply gets re-evaluated on its merits.
	void ResetObjective(string reason, bool blacklist)
	{
		if (m_Objective.kind != OVT_EObjectiveKind.NONE)
		{
			Print(LOG + "Objective '" + m_Objective.name + "' ended: " + reason, LogLevel.WARNING);

			if (blacklist)
				BlacklistPosition(m_Objective.position, m_iBlacklistRounds);
		}

		TearDownObjectiveDeployments();

		// ⚠ AFTER the ledger and BEFORE the record is cleared. The ledger's deletes are what release the
		// insertion reservations and the trucks; each asset module's own sweep is for what the ledger
		// could not know about (a restored marker) and for the structure itself, and it needs the asset
		// record's position to find either.
		TearDownObjectiveAssets();

		ClearFOBRecord();
		ClearObjectiveRecord();

		m_bIdleLogged = false;
		m_bRestorePending = false;
		m_iRelinkAttempts = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes every deployment the director created for the current objective.
	//!
	//! Tracked by CONFIG NAME PLUS POSITION rather than by entity id, because ids do not survive a
	//! session and because that pair is what the deployment framework's own lookup takes. A deployment
	//! that has already gone simply is not found, which is not an error.
	//!
	//! 🔴 An unfinished operation is RECALLED, not written off. Before this, a director that abandoned
	//! an objective deleted the sabotage team still walking to it AND the 100 resources that had bought
	//! them - otherwise every abandoned objective is a permanent tax on a faction that is usually
	//! abandoning it because it was already poor.
	//!
	//! ⚠ The refund is the FRAMEWORK's, not the director's, and that is an acceptance criterion: Q6
	//! greps Scripts/Game/GameMode/Objectives/ for the deployment framework's pool-credit method and
	//! requires NO hits - which is why this paragraph does not spell the method's name.
	//! RecallDeployment() puts the credit in OVT_DeploymentManagerComponent, where its
	//! "cannot double-credit" property can be structural rather than a rule.
	//!
	//! ⚠ Only OPERATIONS, and only LIVE ones. IsObjectiveOperationConfig() decides the first (a standing
	//! forward base got exactly what its money bought) and RecallDeployment() the second (a team the
	//! resistance killed is a loss, not a recall). Together they are why G5 survives this.
	protected void TearDownObjectiveDeployments()
	{
		if (!m_aCreatedDeployments || m_aCreatedDeployments.IsEmpty())
			return;

		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (deployments)
		{
			int refunded = 0;
			int recalled = 0;

			foreach (OVT_ObjectiveDeploymentRef created : m_aCreatedDeployments)
			{
				if (!created)
					continue;

				OVT_DeploymentComponent deployment = deployments.GetDeploymentNearPosition(created.configName, created.position, TEARDOWN_LOOKUP_RADIUS);
				if (!deployment)
					continue;

				if (!IsObjectiveOperationConfig(created.configName))
				{
					deployments.DeleteDeployment(deployment);
					continue;
				}

				int returned = deployments.RecallDeployment(deployment);
				if (returned > 0)
				{
					refunded = refunded + returned;
					recalled = recalled + 1;
				}
			}

			if (refunded > 0)
				Print(LOG + "Objective '" + m_Objective.name + "': " + recalled.ToString() + " operation(s) were recalled before they finished and " + refunded.ToString() + " resources returned to the pool", LogLevel.NORMAL);
		}

		// ⚠ CLEARED WHATEVER HAPPENED, and it is the outer half of the "cannot pay twice" guarantee: a
		// teardown path that somehow ran again would find an empty ledger and look nothing up. The inner
		// half is RecallDeployment() zeroing the stamp before it credits.
		m_aCreatedDeployments.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Records a deployment as belonging to the current objective, so the reset path takes it down.
	//!
	//! Called by whatever created it, immediately after the create. Later build phases (harassment,
	//! sabotage, tower recapture, the forward base) are the callers; the director tracks rather than
	//! guesses, so no list of config names has to be kept in sync with the configs.
	//! \param[in] configName The deployment config's name.
	//! \param[in] position Where it was created.
	void TrackObjectiveDeployment(string configName, vector position)
	{
		if (configName == "")
			return;

		OVT_ObjectiveDeploymentRef created = new OVT_ObjectiveDeploymentRef();
		created.configName = configName;
		created.position = position;

		m_aCreatedDeployments.Insert(created);
	}

	//------------------------------------------------------------------------------------------------
	// COUNTERS AND THE FORWARD BASE
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! ONE COMPLETED OPERATION, REPORTED. The public counter every deployment-side behaviour module
	//! calls when its work lands.
	//!
	//! 🔴 IT COUNTS. IT DOES NOT DECIDE. This method may never change phase, reset the objective or
	//! move a timer. A counter increment is not a TICK: everything that moves this machine moves on
	//! DirectorTick(), behind its three early returns, and this method is PUBLIC and is called from a
	//! deployment's own update, from a restore, and from test fixtures arranging a known state.
	//! Transitioning from here means any of those silently advances the ramp - a fixture that bumped a
	//! counter to arrange "three operations completed" found itself saving a forward-base-phase
	//! objective it never asked for, and the phase entry overwrote its planted countdown.
	//!
	//! ⚠ The signal is PULLED by the tick, never pushed from here. ConsumeReportedOperations() compares
	//! the counters against a mark once per tick; nothing here notifies anything.
	//!
	//! The cost of counting only: the phase advances on the next director tick rather than in the same
	//! frame as the last debuff - at most one in-game minute, on a ramp whose cadence is tens of them.
	//! \param[in] key The bag counter, e.g. OVT_ObjectiveInstance.BAG_HARASSMENT_SUCCESSES.
	//! \param[in] delta How much to add.
	void ReportObjectiveProgress(string key, int delta)
	{
		if (m_Objective.kind == OVT_EObjectiveKind.NONE)
			return;

		if (m_Instance)
			m_Instance.Report(key, delta);
	}

	//------------------------------------------------------------------------------------------------
	//! Arms the countdown to the next operation at the objective.
	//! \param[in] ticks In-game minutes until the next operation. Negative reads as zero.
	void SetOperationCountdown(int ticks)
	{
		if (ticks < 0)
			m_Objective.nextOpTicks = 0;
		else
			m_Objective.nextOpTicks = ticks;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the objective's idle clock: "this many in-game minutes of patience remain, AS OF NOW".
	//!
	//! ⚠ It ALSO re-baselines the progress marks, and that second half is why every plant must come
	//! through here. The clock is re-armed when an operation REPORTS, which the tick detects by
	//! comparing the success counters against a mark - so operations already banked (a fixture
	//! arranging "three completed", a restore adopting a saved count, a phase entry starting fresh)
	//! must not read as news on the very next tick and overwrite the value just planted.
	//!
	//! Three callers, all meaning the same thing: EnterPhase() (a transition IS progress),
	//! RearmObjectiveIdleClock() (progress observed on a tick), and whatever plants a known state -
	//! ApplyPersistedObjective() calls SyncProgressMarks() itself because its counters are written
	//! AFTER its clock.
	//! \param[in] ticks In-game minutes of patience remaining. Negative reads as zero.
	void SetPhaseTimeout(int ticks)
	{
		if (ticks < 0)
			m_Objective.phaseTicks = 0;
		else
			m_Objective.phaseTicks = ticks;

		SyncProgressMarks();
	}

	//------------------------------------------------------------------------------------------------
	// BLACKLIST
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Sends a place to the back of the queue for a number of selection rounds.
	//! \param[in] position The place.
	//! \param[in] rounds How many rounds it sits out. Non-positive is ignored.
	void BlacklistPosition(vector position, int rounds)
	{
		if (rounds <= 0)
			return;

		string key = OVT_ObjectiveSelection.PositionKey(position);

		foreach (OVT_ObjectiveBlacklistEntry entry : m_aBlacklist)
		{
			if (!entry)
				continue;

			if (OVT_ObjectiveSelection.PositionKey(entry.position) != key)
				continue;

			// Re-blacklisting a place that is already serving extends its cooldown rather than
			// stacking a second entry for the same spot.
			if (entry.roundsLeft < rounds)
				entry.roundsLeft = rounds;

			return;
		}

		OVT_ObjectiveBlacklistEntry added = new OVT_ObjectiveBlacklistEntry();
		added.position = position;
		added.roundsLeft = rounds;

		m_aBlacklist.Insert(added);
	}

	//------------------------------------------------------------------------------------------------
	//! Flattens the blacklist into the parallel arrays the pure selection statics take.
	//! \param[inout] keys Filled with one key per entry.
	//! \param[inout] roundsLeft Filled with the matching rounds.
	protected void ReadBlacklist(notnull array<string> keys, notnull array<int> roundsLeft)
	{
		foreach (OVT_ObjectiveBlacklistEntry entry : m_aBlacklist)
		{
			if (!entry)
				continue;

			keys.Insert(OVT_ObjectiveSelection.PositionKey(entry.position));
			roundsLeft.Insert(entry.roundsLeft);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Serves one round of every blacklist entry and drops the ones that have served their time.
	protected void ServeBlacklistRound()
	{
		array<int> rounds = new array<int>();
		foreach (OVT_ObjectiveBlacklistEntry entry : m_aBlacklist)
		{
			if (entry)
				rounds.Insert(entry.roundsLeft);
			else
				rounds.Insert(0);
		}

		OVT_ObjectiveSelection.DecayBlacklist(rounds);

		// BACKWARDS, and with the ORDERED removal: iterating down means an index below the current one
		// is never disturbed, so the parallel rounds list stays aligned with the entries; the ordered
		// variant then keeps the saved payload in a stable order, which costs nothing on a list this
		// size and makes two consecutive saves diffable.
		for (int i = m_aBlacklist.Count() - 1; i >= 0; i--)
		{
			if (!m_aBlacklist[i] || rounds[i] <= 0)
			{
				m_aBlacklist.RemoveOrdered(i);
				continue;
			}

			m_aBlacklist[i].roundsLeft = rounds[i];
		}
	}

	//------------------------------------------------------------------------------------------------
	// RE-SELECTION TRIGGERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Subscribes to both control-change invokers, exactly once.
	//!
	//! ⚠ Remove before insert, always. A ScriptInvoker has no Contains() and does not de-duplicate, so
	//! an Insert() that ran twice would fan every control change out twice - and this component's
	//! initialisation genuinely can run twice in one session, because starting a second campaign
	//! without leaving the session re-runs the whole chain.
	protected void HookControlChanges()
	{
		if (m_bHooked)
			return;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (occupying && occupying.m_OnBaseControlChanged)
		{
			occupying.m_OnBaseControlChanged.Remove(OnBaseControlChanged);
			occupying.m_OnBaseControlChanged.Insert(OnBaseControlChanged);
			m_HookedOccupying = occupying;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (towns && towns.m_OnTownControlChange)
		{
			towns.m_OnTownControlChange.Remove(OnTownControlChanged);
			towns.m_OnTownControlChange.Insert(OnTownControlChanged);
			m_HookedTowns = towns;
		}

		if (m_HookedOccupying || m_HookedTowns)
			m_bHooked = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Unsubscribes from both invokers, through the handles cached when they were subscribed.
	//!
	//! THE CACHED HANDLES ARE THE POINT: resolving the managers again during teardown can hand back a
	//! static instance whose entity has already gone.
	protected void UnhookControlChanges()
	{
		if (m_HookedOccupying && m_HookedOccupying.m_OnBaseControlChanged)
			m_HookedOccupying.m_OnBaseControlChanged.Remove(OnBaseControlChanged);

		if (m_HookedTowns && m_HookedTowns.m_OnTownControlChange)
			m_HookedTowns.m_OnTownControlChange.Remove(OnTownControlChanged);

		m_HookedOccupying = null;
		m_HookedTowns = null;
		m_bHooked = false;
	}

	//------------------------------------------------------------------------------------------------
	//! A base changed hands: ask for a re-selection on the next tick.
	//!
	//! ⚠ This handler may NOT read ownership (D3). The invoker fires from
	//! OVT_BaseControllerComponent.SetControllingFaction() BEFORE the affiliation is written, so
	//! anything asking who owns the base from in here gets the OLD owner - and a director that
	//! re-selected inline could pick as its next target the base the player has just taken.
	//! \param[in] baseController The base that changed hands. Deliberately unused.
	protected void OnBaseControlChanged(OVT_BaseControllerComponent baseController)
	{
		m_bReselectPending = true;
	}

	//------------------------------------------------------------------------------------------------
	//! A town changed hands: ask for a re-selection on the next tick.
	//!
	//! Flag-only for the same reason as the base handler, and for a second one: the town invoker fans
	//! out to several systems that mutate town state, so a re-selection running inside the dispatch
	//! would score a half-updated campaign.
	//! \param[in] town The town that changed hands. Deliberately unused.
	protected void OnTownControlChanged(OVT_TownData town)
	{
		m_bReselectPending = true;
	}

	//------------------------------------------------------------------------------------------------
	// PERSISTENCE
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Adopts a persisted objective. THE ONE SIDE-EFFECTING ENTRY POINT of the director's serializer.
	//!
	//! ⚠ It touches no pool and no deployment, and that is a LOAD-ORDER rule. The deployment manager's
	//! own restore CLEARS AND REFILLS the faction resource pool and runs after the game-mode component
	//! serializers, so anything this did with money would be overwritten moments later; and the
	//! deployment entities are separate tracked instances whose restore order relative to this payload
	//! is not defined. Everything that needs either is deferred to the first tick.
	//!
	//! IDEMPOTENT - re-applying a save to a live session runs this again, and every line of it is an
	//! assignment or a clear-and-rebuild.
	//!
	//! ⚠ An unrecognised plan or phase NAME is abandoned, loudly, and never guessed at (G6). The
	//! payload carries names rather than enum integers precisely so a plan removed by a mod, or a phase
	//! renamed between builds, is DETECTABLE rather than silently adopted as whatever index sits there
	//! now. It costs a player one in-game minute and nothing else.
	//! \param[in] configName The plan the objective was running. Unknown to the registry means abandon.
	//! \param[in] kind The persisted objective kind.
	//! \param[in] position Where it was.
	//! \param[in] phaseName Which phase it was in, by authored name. Unknown means abandon.
	//! \param[in] phaseTicks Ticks left on the idle clock.
	//! \param[in] nextOpTicks Ticks left until the next operation.
	//! \param[in] bagKeys The objective's int-bag keys.
	//! \param[in] bagValues The matching values, same order, same length.
	//! \param[in] bagVecKeys The objective's vector-bag keys.
	//! \param[in] bagVecValues The matching positions, same order, same length.
	//! \param[in] blacklistPositions Blacklisted places.
	//! \param[in] blacklistRounds Rounds each of them still owes, same order, same length.
	//! \param[in] fob The forward-base record, or null when none was saved.
	void ApplyPersistedObjective(string configName, int kind, vector position, string phaseName, int phaseTicks, int nextOpTicks, array<string> bagKeys, array<int> bagValues, array<string> bagVecKeys, array<vector> bagVecValues, array<vector> blacklistPositions, array<int> blacklistRounds, OVT_ObjectiveFOBRecord fob)
	{
		// ⚠ THE BLACKLIST IS RESTORED WHATEVER HAPPENS TO THE OBJECTIVE, AND BEFORE IT. It is a fact
		// about places, not about the objective that was running, so a payload whose plan has gone must
		// still come back with its cooldowns intact - otherwise abandoning one objective would also
		// forget every place the campaign had already decided to leave alone.
		ReadPersistedBlacklist(blacklistPositions, blacklistRounds);

		OVT_EObjectiveKind restoredKind = KindFromInt(kind);
		if (restoredKind == OVT_EObjectiveKind.NONE)
		{
			// No objective was saved. Put the machine into the state a fresh campaign is in, without a
			// log line - "nothing was running" is not a fault.
			//
			// ⚠ The forward-base record is cleared too, and it is NOT reached by ClearObjectiveRecord().
			// This path also runs when a save is re-applied to a LIVE session, where the machine may be
			// standing a forward base the payload knows nothing about; a record left set would leave the
			// next objective's very first spend measured against a base that is not there.
			// ⚠ No teardown, though - deleting the deployment behind it is a deployment operation, and
			// this method is a codec that must not touch one.
			ClearFOBRecord();
			ClearObjectiveRecord();

			m_aCreatedDeployments.Clear();
			m_bRestorePending = false;
			m_iRelinkAttempts = 0;
			m_bIdleLogged = false;

			return;
		}

		OVT_ObjectiveConfig plan;
		if (m_Registry)
			plan = m_Registry.FindConfigByName(configName);

		int phaseIndex = -1;
		if (plan)
			phaseIndex = plan.IndexOfPhase(phaseName);

		// ⚠ A MISSING REGISTRY IS NOT A MISSING PLAN. A world whose prefab has no registry wired has no
		// doctrine at all, and a save taken in it must still restore its target, its counters and its
		// blacklist - so the plan is only REQUIRED once a registry exists to require it of.
		if (m_Registry && !plan)
		{
			DiscardPersistedObjective("the saved plan '" + configName + "' is not in the objective registry");
			return;
		}

		if (plan && phaseIndex < 0)
		{
			DiscardPersistedObjective("the saved phase '" + phaseName + "' is not a phase of plan '" + configName + "'");
			return;
		}

		// ⚠ The same rule with no registry behind it, in the only form still available. With no registry
		// there is nothing to check a phase name AGAINST, so this is an emptiness test rather than a
		// lookup: an authored name is meaningful to whichever build wrote it, and refusing every name
		// this registry-less build has not shipped would abandon a modded campaign's objective on every
		// load. A save carrying NO phase name at all is still unrecoverable.
		if (!plan && phaseName == "")
		{
			DiscardPersistedObjective("the saved objective names no phase at all, and no plan registry loaded to resolve one against");
			return;
		}

		m_Objective.kind = restoredKind;
		m_Objective.position = position;
		m_Objective.phaseTicks = phaseTicks;
		m_Objective.nextOpTicks = nextOpTicks;

		// ⚠ THE BAG IS THE FORMAT (D9), so restoring it restores every module counter at once - the two
		// success counters included, because they are bag keys now. Clear-and-rebuild, which is what
		// makes a re-apply to a live session idempotent.
		if (m_Instance)
		{
			m_Instance.WriteBag(bagKeys, bagValues);
			m_Instance.WriteBagV(bagVecKeys, bagVecValues);
		}

		// ⚠ After the bag, never before. The idle clock detects a completed operation by comparing the
		// success counters against a mark; a restored count is history, not news, and a mark left at zero
		// would make the first tick after a load treat the whole saved ramp as progress and overwrite the
		// clock just restored. This is the one place the marks are synced by hand, because the payload
		// writes the clock BEFORE the counters and SetPhaseTimeout() is not on this path.
		SyncProgressMarks();

		// The name is a LABEL, not an identifier, and is not in the payload. It is re-resolved from
		// the live campaign on the first tick, which is also the only moment the town and base
		// registries are guaranteed to be populated.
		m_Objective.name = "";

		ClearFOBRecord();

		// ⚠ And the asset owners, because the record is being replaced wholesale. This method is also
		// reached by re-applying a save to a LIVE session, where a module registered by whatever the
		// session was doing is still holding a "a supply party is on its way" flag about a record that
		// has just been zeroed. AdoptPersistedPhase() re-registers whatever the RESTORED phase owns.
		// ⚠ No teardown. This is a codec and must not touch a deployment.
		if (m_mAssetModules)
			m_mAssetModules.Clear();

		if (fob && fob.up)
		{
			m_FOB.up = true;
			m_FOB.position = fob.position;
			m_FOB.sourceBasePosition = fob.sourceBasePosition;
			m_FOB.spent = fob.spent;
			m_FOB.starvationTicks = fob.starvationTicks;
			m_FOB.deploymentName = fob.deploymentName;
		}

		// Nothing the director created survives as a tracked reference across a load: only the forward
		// base is re-linked, and it is re-linked by name and position on the first tick.
		m_aCreatedDeployments.Clear();

		// ⚠ The phase is adopted LAST, and it is ADOPTED rather than ENTERED. Last, because adopting a
		// phase rebuilds its runtime module set and a module's OnEnter() must see the WHOLE restored
		// objective rather than the tail end of whatever the session was doing. Adopted rather than
		// entered, because EnterPhase() re-arms the idle clock and re-baselines the progress marks,
		// which would overwrite the two values this payload just restored: adopting a phase is not a
		// transition, it is a declaration that the machine was already in one.
		AdoptPersistedPhase(plan, phaseIndex, phaseName);

		m_bMissingPlanLogged = false;

		if (m_aInstances && m_Instance && m_aInstances.Find(m_Instance) == -1)
			m_aInstances.Insert(m_Instance);

		m_bRestorePending = true;
		m_iRelinkAttempts = 0;
		m_bIdleLogged = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the blacklist from a payload's two parallel arrays.
	//!
	//! ⚠ A SERVED ENTRY IS DROPPED RATHER THAN RESTORED AT ZERO. An entry with no rounds left has
	//! already done its time, and keeping it would leave a place in the list that nothing ever prunes.
	//! \param[in] positions Blacklisted places.
	//! \param[in] rounds Rounds each still owes, same order, same length. A mismatch restores nothing.
	protected void ReadPersistedBlacklist(array<vector> positions, array<int> rounds)
	{
		m_aBlacklist.Clear();

		if (!positions || !rounds || positions.Count() != rounds.Count())
			return;

		int count = positions.Count();
		for (int i = 0; i < count; i++)
		{
			if (rounds[i] <= 0)
				continue;

			OVT_ObjectiveBlacklistEntry entry = new OVT_ObjectiveBlacklistEntry();
			entry.position = positions[i];
			entry.roundsLeft = rounds[i];

			m_aBlacklist.Insert(entry);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a restored objective back into the phase it was saved in, WITHOUT entering it.
	//!
	//! ⚠ Adopting is not entering. An entry re-arms the idle clock, re-baselines the progress marks and
	//! zeroes the cadence - all three of which the payload has just restored to values that mean
	//! something. What an adoption DOES share with an entry is the runtime module set: a restored
	//! objective with no modules can neither act, advance nor be given up.
	//! \param[in] plan The plan the objective is running, or null when no registry resolved.
	//! \param[in] phaseIndex The plan phase index, or -1 when there is no plan.
	//! \param[in] phaseName The saved phase name, used when there is no plan to resolve it against.
	protected void AdoptPersistedPhase(OVT_ObjectiveConfig plan, int phaseIndex, string phaseName)
	{
		if (!m_Instance)
			return;

		m_Instance.SetConfig(plan);

		if (!plan || phaseIndex < 0)
		{
			// ⚠ THE NAME IS KEPT EVEN THOUGH NOTHING CAN RUN IT. No plan resolved, so there are no
			// modules to clone and the objective can neither act nor advance - but the name is what the
			// per-tick "running with NO PLAN behind it" line reports, and a restore that dropped it
			// would leave that line unable to say which phase the save thought it was in.
			m_Instance.RecordPhase(-1, phaseName);
			m_Instance.ExitRuntimePhase();

			return;
		}

		OVT_ObjectivePhase authored = plan.GetPhase(phaseIndex);
		m_Instance.RecordPhase(phaseIndex, phaseName);
		m_Instance.EnterRuntimePhase(authored);
	}

	//------------------------------------------------------------------------------------------------
	//! Throws a persisted objective away, loudly, and leaves the machine choosing again.
	//!
	//! ⚠ The only clean-abandon path, shared by three faults (D2/G6): an unrecognised payload VERSION,
	//! a plan the registry does not carry, and a phase the plan does not have. All three mean "this
	//! save describes a machine this build cannot run" and all three cost a player one in-game minute.
	//!
	//! ⚠ It is an ERROR, not a warning, and it names the missing thing. A discarded objective is
	//! invisible in play - the campaign simply picks a different target - so without a line naming the
	//! plan or the phase, a mod that renamed one would look like a mod that did nothing.
	//! \param[in] reason What was missing, phrased for a log line.
	void DiscardPersistedObjective(string reason)
	{
		Print(LOG + "DISCARDING the persisted objective: " + reason + ". The occupying faction will choose a new one on its next tick", LogLevel.ERROR);

		// ⚠ THE RECORD ONLY. No teardown, no refund, no deployment lookup: this is reached from a codec
		// and from a load, and both run before the deployment framework has rebuilt its own state.
		// Anything the discarded objective had standing is found and dealt with by the next objective's
		// own commit, which tears down a forward base before it raises one.
		ClearFOBRecord();
		ClearObjectiveRecord();

		m_aCreatedDeployments.Clear();

		m_bRestorePending = false;
		m_iRelinkAttempts = 0;
		m_bIdleLogged = false;
		m_bReselectPending = true;
	}

	//------------------------------------------------------------------------------------------------
	// THE PLAN REGISTRY
	//------------------------------------------------------------------------------------------------

	//! \return The authored plan registry, or null when the prefab wires none.
	OVT_ObjectiveRegistry GetRegistry() { return m_Registry; }

	//------------------------------------------------------------------------------------------------
	//! Runs the registry's validator once, at world start.
	//!
	//! ⚠ THIS IS THE CALL SITE THE DEPLOYMENT REGISTRY'S EQUIVALENT NEVER GOT (C6), and without it
	//! "a broken plan is named and skipped" would be decorative. It is server-only, it is idempotent,
	//! and it runs before the first tick can select anything.
	//! \return True when every plan passed, and true when there is no registry to check.
	bool ValidateObjectiveRegistry()
	{
		if (!Replication.IsServer())
			return true;

		if (!m_Registry)
		{
			Print(LOG + "No objective registry is wired on the game mode - the occupying faction is running the hard-coded phase machine", LogLevel.WARNING);
			return true;
		}

		return m_Registry.ValidateAllConfigs();
	}

	//------------------------------------------------------------------------------------------------
	//! Reads a persisted objective kind, refusing anything the running build does not recognise.
	//!
	//! AN UNKNOWN INTEGER READS AS "NO OBJECTIVE", never as whatever member happens to sit at that
	//! position. The payload is append-only and version-first, so a save written by a LATER build can
	//! legitimately carry a kind this one has never heard of; adopting it blindly would put the
	//! machine into a phase with no handler.
	//! \param[in] value The persisted integer.
	//! \return A kind this build understands.
	protected OVT_EObjectiveKind KindFromInt(int value)
	{
		if (value == OVT_EObjectiveKind.TOWN)
			return OVT_EObjectiveKind.TOWN;

		if (value == OVT_EObjectiveKind.BASE)
			return OVT_EObjectiveKind.BASE;

		return OVT_EObjectiveKind.NONE;
	}

	// ⚠ PhaseFromInt() was deleted with the version-1 record, and its absence is the point. It read a
	// persisted phase INTEGER and refused anything this build did not recognise - the whole mechanism
	// behind "never renumber the enum". The version-2 payload carries the phase NAME, so nothing reads
	// a phase integer off a save any more.

	//------------------------------------------------------------------------------------------------
	//! Whether the phase a restored objective came back in is one that ENDS the objective.
	//!
	//! ⚠ The one question the restore path asks about a phase, and it is asked of the phase's own
	//! MODULES rather than of its name or its index. A terminal operation (the shipped one starts a
	//! battle) is the only kind of phase with nothing to advance to, and nothing about what it started
	//! is persisted - so a save taken inside one describes a state the load cannot rebuild.
	//!
	//! A phase with NO modules answers false, which is right for both ways that happens: a plan that
	//! did not resolve (reported and abandoned elsewhere) and a phase authored empty (the validator
	//! names it at world start).
	//! \return True when the restored phase carries a terminal operation.
	protected bool RestoredPhaseIsTerminal()
	{
		if (!m_Instance)
			return false;

		int count = m_Instance.GetRuntimeModuleCount();
		for (int i = 0; i < count; i++)
		{
			OVT_BaseObjectiveOperationModule operation = OVT_BaseObjectiveOperationModule.Cast(m_Instance.GetRuntimeModule(i));
			if (operation && operation.IsTerminal())
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Re-links a restored objective to the live campaign, on the first tick after a load.
	//!
	//! Three jobs, none of which a deserialization is allowed to do:
	//!  1. A BATTLE THAT WAS LIVE WHEN THE SAVE WAS TAKEN IS GONE. Nothing about the battle controller
	//!     is persisted, so an objective restored mid-counter-attack has no battle to wait for.
	//!  2. THE DISPLAY NAME is re-resolved from the town and base registries.
	//!  3. THE FORWARD BASE'S DEPLOYMENT is found again by name plus position. If it is not there after
	//!     FOB_RELINK_ATTEMPTS ticks the marker went away while the campaign was saved, and the
	//!     objective is torn down with the reason in the log rather than left pointing at nothing.
	protected void ResolveRestoredObjective()
	{
		if (m_Objective.kind == OVT_EObjectiveKind.NONE)
		{
			m_bRestorePending = false;
			return;
		}

		// ⚠ Asked of the phase's MODULES, not of a phase number. "Was it saved mid-battle" used to be
		// "is the phase COUNTER_QRF"; the property that actually matters is that the restored phase ENDS
		// the objective rather than advancing off it, and nothing about a battle controller is
		// persisted. A plan whose terminal phase is its second, or its fifth, gets the same roll-back.
		if (RestoredPhaseIsTerminal())
		{
			m_bRestorePending = false;
			ResetObjective("the operation it was saved in the middle of ends the objective and nothing about it survived the load", false);
			return;
		}

		if (m_Objective.name == "")
			m_Objective.name = ResolveObjectiveName();

		// The restored objective RE-ACQUIRES its bias here, and this is the only path that needs its own
		// push: a restore writes the phase straight into the record rather than entering it, so
		// EnterPhase() - the live machine's one push site - never runs. Doing it on this first tick
		// rather than during deserialization is the same load-order rule the rest of the restore
		// follows: the deployment framework rebuilds its own per-faction state after the game mode's
		// component records are read, and empties the bias store when it does.
		PushObjectiveAnchor();

		if (!m_FOB.up)
		{
			m_bRestorePending = false;
			return;
		}

		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (deployments)
		{
			OVT_DeploymentComponent deployment = deployments.GetDeploymentNearPosition(m_FOB.deploymentName, m_FOB.position, FOB_RELINK_RADIUS);
			if (deployment)
			{
				m_bRestorePending = false;
				m_iRelinkAttempts = 0;
				return;
			}
		}

		m_iRelinkAttempts = m_iRelinkAttempts + 1;
		if (m_iRelinkAttempts < FOB_RELINK_ATTEMPTS)
			return;

		m_bRestorePending = false;
		ResetObjective("its forward operating base could not be found again after the load", false);
	}

	//------------------------------------------------------------------------------------------------
	//! Re-resolves the current objective's display name from the campaign.
	//! \return A name, or an empty string when nothing can be resolved yet.
	protected string ResolveObjectiveName()
	{
		if (m_Objective.kind == OVT_EObjectiveKind.BASE)
		{
			OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
			if (!occupying)
				return "";

			OVT_BaseData base = occupying.GetNearestBase(m_Objective.position);
			if (!base)
				return "";

			return OVT_ObjectiveCandidateSet.ResolveBaseName(occupying, base);
		}

		return OVT_ObjectiveCandidateSet.ResolveTownNameAt(OVT_Global.GetTowns(), m_Objective.position);
	}

	//------------------------------------------------------------------------------------------------
	// STATE HELPERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Puts the objective record back to "no objective".
	//!
	//! ⚠ The one place the deployment bias is dropped, and it is here rather than in ResetObjective()
	//! because this is the method every RUNTIME "there is no objective now" path funnels through - the
	//! reset path and the idle path a failed re-selection takes. A bias left pointing at an abandoned
	//! objective would keep pulling routine spending toward a place nothing is working on.
	//!
	//! ⚠ Construction no longer comes through here, and must not. OnPostInit() calls
	//! ClearObjectiveRecordFields() directly: at construction there is definitionally no anchor to
	//! drop, and OnPostInit() ALSO RUNS IN THE WORLD EDITOR, where none of the managers the drop
	//! resolves exist. It took the editor down with a NULL faction manager.
	protected void ClearObjectiveRecord()
	{
		DropObjectiveAnchor();

		// ⚠ And the reserve floor, through the same funnel and for a sharper version of the same reason
		// (D18). A stale anchor leans routine spending toward a place nothing is working on; a stale
		// floor STOPS routine spending outright, for an operation nobody intends to send. DirectorTick()
		// would clear it next pass anyway, but "next pass" is an in-game minute in which the evaluator
		// may run twice.
		DropObjectiveReserve();

		ClearObjectiveRecordFields();
	}

	//------------------------------------------------------------------------------------------------
	//! The record half of ClearObjectiveRecord(): every field back to "no objective", and no anchor
	//! work of any kind.
	//!
	//! ⚠ SPLIT OUT FOR EXACTLY ONE CALLER - OnPostInit(), for the reason in the note above - on the
	//! precedent the forward base's own runtime clear set. Nothing at RUNTIME may call it: a live path that cleared
	//! the record without dropping the bias is precisely the failure the funnel exists to prevent.
	protected void ClearObjectiveRecordFields()
	{
		m_Objective.kind = OVT_EObjectiveKind.NONE;
		m_Objective.position = vector.Zero;
		m_Objective.name = "";
		m_Objective.phaseTicks = 0;
		m_Objective.nextOpTicks = 0;

		// ⚠ The instance leaves the live list, its modules are told the phase ended, and its bag is
		// EMPTIED - all through this one funnel. A module left initialised would keep a latch alive
		// across objectives; a bag left populated would hand the next objective counters it never
		// earned; an instance left in the list would make the tick keep stepping a phase that no longer
		// exists. Clear() rather than a per-entry removal is correct while there is exactly ONE instance
		// object: this method means "there is no objective now".
		if (m_Instance)
		{
			m_Instance.ExitRuntimePhase();
			m_Instance.RecordPhase(-1, "");
			m_Instance.SetConfig(null);
			m_Instance.ClearBags();
		}

		if (m_aInstances)
			m_aInstances.Clear();

		// ⚠ AND THE ASSET OWNERS WITH THEM. A registered module is a fact about ONE objective's standing
		// asset; carried into the next objective it would arm a spend ceiling for a forward base that no
		// longer exists and hand the next teardown a module whose record has already been zeroed. Every
		// path that reaches here has already run TearDownObjectiveAssets(), so nothing is left standing
		// by dropping them - the two are called in sequence by ResetObjective() and by CommitObjective().
		if (m_mAssetModules)
			m_mAssetModules.Clear();

		m_bOperationIntervalClaimed = false;
		m_bIdleHoldLogged = false;

		m_bMissingPlanLogged = false;

		// ⚠ Every per-objective latch is dropped here rather than in ResetObjective(): every "there is no
		// objective now" path reaches this body through ClearObjectiveRecord(), and a latch left set
		// would silence its line for the NEXT objective as well.
		//
		// ⚠ The battle's own two latches are not among them and do not need to be. They live on
		// OVT_StartBattleObjectiveOperation, which is CLONED on every phase entry.
		m_iAffordabilityHeldTicks = 0;

		// ⚠ AND EVERY (CONFIG, REASON) REFUSAL LATCH WITH THEM. A refusal is a fact about ONE objective's
		// attempt to buy something; carried into the next objective it would silence the same refusal for
		// a target that has not yet said it once. Guarded because OnPostInit() reaches this body, and the
		// world editor constructs components in an order nothing here may assume.
		if (m_aRefusalConfigs)
			m_aRefusalConfigs.Clear();

		if (m_aRefusalReasons)
			m_aRefusalReasons.Clear();

		// The idle clock's own state. The per-tick flag is dropped with the rest so a refusal seen on the
		// tick that ended an objective cannot hold the NEXT objective's first clock.
		m_bBlockedOnAffordability = false;
		m_sBlockedOnConfig = "";
		m_iBlockedOnCost = 0;
		m_iProgressHarassmentMark = 0;
		m_iProgressSabotageMark = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the forward-base record back to "no forward base".
	//!
	//! ⚠ The runtime half is the MODULE's, not this method's. Whether a supply party has been SENT is
	//! what arms the spend ceiling and what stops a second one going out, and it lives on the raise
	//! module. It is dropped by TearDownObjectiveAssets() and by ClearObjectiveRecordFields() dropping
	//! the module registration itself, both of which run on every path that reaches here.
	protected void ClearFOBRecord()
	{
		m_FOB.up = false;
		m_FOB.position = vector.Zero;
		m_FOB.sourceBasePosition = vector.Zero;
		m_FOB.spent = 0;
		m_FOB.starvationTicks = 0;
		m_FOB.deploymentName = "";
	}

	//------------------------------------------------------------------------------------------------
	// GETTERS
	//------------------------------------------------------------------------------------------------

	//! \return True when the occupying faction currently has a target.
	bool HasObjective() { return m_Objective.kind != OVT_EObjectiveKind.NONE; }

	//! \return What kind of place the objective is, or NONE.
	OVT_EObjectiveKind GetObjectiveKind() { return m_Objective.kind; }

	//! \return Where the objective is. The zero vector when there is none.
	vector GetObjectivePosition() { return m_Objective.position; }

	//! \return The objective's display name, or an empty string.
	string GetObjectiveName() { return m_Objective.name; }

	//------------------------------------------------------------------------------------------------
	//! The objective's display name for a READ-ONLY SURFACE - today the Game Master snapshot.
	//!
	//! ⚠ It computes nothing and changes nothing, a hard requirement of its caller. OVT_GMSnapshotBuilder
	//! and the fan around it are strictly read-only: the trap that rule exists for is a "predict the
	//! next value" call that also APPLIES it, which would fire every time a Game Master opened the
	//! editor.
	//!
	//! It differs from GetObjectiveName() in one way: with no objective it answers an empty string even
	//! if a name were somehow left behind, so a panel can never label a campaign that has no target.
	//! \return The display name, or an empty string when there is no objective.
	string GetObjectiveDisplayName()
	{
		if (m_Objective.kind == OVT_EObjectiveKind.NONE)
			return "";

		return m_Objective.name;
	}

	//! \return Ticks left on the objective's idle clock before it is abandoned as wedged.
	int GetPhaseTicks() { return m_Objective.phaseTicks; }

	//! \return The authored idle-clock budget - what a re-arm puts the clock back to. Public so a case can
	//! assert "this was re-armed" against the real authored figure instead of a copy of it.
	int GetPhaseTimeoutTicks() { return m_iPhaseTimeoutTicks; }

	//! \return True while an operation this director created is alive and unfinished. Side-effect free,
	//! and public for the same reason IsCounterAttackReady() is: it is the reason the backstop is being
	//! held, and a state that changes an outcome has to be assertable. See HasOperationInFlight().
	bool IsOperationInFlight() { return HasOperationInFlight(); }

	//! \return Ticks left before the next operation is sent.
	int GetNextOpTicks() { return m_Objective.nextOpTicks; }

	//------------------------------------------------------------------------------------------------
	//! Harassment operations completed at the objective. Drives the group ladder.
	//!
	//! ⚠ IT READS THE BAG, WHICH IS WHERE THE COUNTER LIVES NOW. The name and the meaning are exactly
	//! what they were when it read a record field, which is why every case that asserts on it is
	//! unchanged - the storage moved, the contract did not.
	//! \return The count, or zero before the component has been initialised.
	int GetHarassmentSuccesses()
	{
		if (!m_Instance)
			return 0;

		return m_Instance.Get(OVT_ObjectiveInstance.BAG_HARASSMENT_SUCCESSES);
	}

	//------------------------------------------------------------------------------------------------
	//! Sabotage missions completed at the objective. Drives the base gates.
	//! \return The count, or zero before the component has been initialised.
	int GetSabotageSuccesses()
	{
		if (!m_Instance)
			return 0;

		return m_Instance.Get(OVT_ObjectiveInstance.BAG_SABOTAGE_SUCCESSES);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a named asset is standing.
	//!
	//! ⚠ SERVER-AUTHORITATIVE, AND FALSE IS THE CLIENT'S ANSWER. None of the director's state
	//! replicates (G12), so on a remote client every asset reads as absent - exactly as the FOB-only
	//! pair this replaced did. Client-side code must never gate on it.
	//! \param key The asset key, e.g. ASSET_FOB.
	//! \return True when the asset exists and is standing; false for an unknown key.
	bool IsAssetUp(string key)
	{
		if (!m_mAssets)
			return false;

		OVT_ObjectiveAssetRecord asset;
		if (!m_mAssets.Find(key, asset) || !asset)
			return false;

		return asset.up;
	}

	//------------------------------------------------------------------------------------------------
	//! Where a named asset stands.
	//!
	//! ⚠ SERVER-AUTHORITATIVE, AND THE ZERO VECTOR IS THE CLIENT'S ANSWER - see IsAssetUp().
	//! \param key The asset key, e.g. ASSET_FOB.
	//! \return The asset's position, or the zero vector for an unknown or absent asset.
	vector GetAssetPosition(string key)
	{
		if (!m_mAssets)
			return vector.Zero;

		OVT_ObjectiveAssetRecord asset;
		if (!m_mAssets.Find(key, asset) || !asset)
			return vector.Zero;

		return asset.position;
	}

	//! \return The base supplying the forward operating base.
	vector GetFOBSourceBasePosition() { return m_FOB.sourceBasePosition; }

	//! \return What has been spent from the pool at the forward operating base.
	int GetFOBSpent() { return m_FOB.spent; }

	//! \return Consecutive ticks the forward operating base has been cut off.
	int GetFOBStarvationTicks() { return m_FOB.starvationTicks; }

	//! \return Config name of the deployment carrying the forward operating base.
	string GetFOBDeploymentName() { return m_FOB.deploymentName; }

	//------------------------------------------------------------------------------------------------
	//! Where the forward base's deployment was sent, whether or not the structure went up yet.
	//!
	//! ⚠ IT IS THE ASSET MODULE'S RUNTIME STATE, NOT A RECORD FIELD, so it answers the zero vector on a
	//! client, before the forward-base phase has been entered, and after a load - all three of which are
	//! "no supply party of ours is out there", which is the truth in each case.
	//! \return The site, or the zero vector.
	vector GetFOBSite()
	{
		OVT_RaiseForwardBaseObjectiveOperation raise = OVT_RaiseForwardBaseObjectiveOperation.Cast(GetAssetModule(ASSET_FOB));
		if (!raise)
			return vector.Zero;

		return raise.GetSite();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a supply party has been sent to raise the forward base. This is also what arms the spend
	//! ceiling. Same lifetime note as GetFOBSite().
	//! \return True while one is on its way or standing.
	bool IsFOBDeploymentSent()
	{
		OVT_RaiseForwardBaseObjectiveOperation raise = OVT_RaiseForwardBaseObjectiveOperation.Cast(GetAssetModule(ASSET_FOB));
		if (!raise)
			return false;

		return raise.IsDeploymentSent();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the forward base is currently reported as cut off.
	//!
	//! ⚠ Derived from the counter rather than from a second latch, and the two are equivalent by
	//! construction: the starvation abort zeroes the counter on every tick the base is supplied and
	//! increments it on every tick it is not. A separate boolean would be a second source of truth for
	//! one bit, and the record is the half that survives a save.
	//! \return True when it is.
	bool IsFOBStarving() { return m_FOB.starvationTicks > 0; }

	//! \return How many places are currently sitting out a selection round.
	int GetBlacklistCount() { return m_aBlacklist.Count(); }

	//------------------------------------------------------------------------------------------------
	//! One blacklisted place.
	//! \param[in] index Index into the blacklist.
	//! \return The position, or the zero vector when the index is out of range.
	vector GetBlacklistPosition(int index)
	{
		if (index < 0 || index >= m_aBlacklist.Count())
			return vector.Zero;

		return m_aBlacklist[index].position;
	}

	//------------------------------------------------------------------------------------------------
	//! How long one blacklisted place still has to serve.
	//! \param[in] index Index into the blacklist.
	//! \return Rounds left, or 0 when the index is out of range.
	int GetBlacklistRounds(int index)
	{
		if (index < 0 || index >= m_aBlacklist.Count())
			return 0;

		return m_aBlacklist[index].roundsLeft;
	}

	//! \return Rounds a failed objective sits out. Read by the reset path and by the save round trip.
	int GetBlacklistRoundsSetting() { return m_iBlacklistRounds; }

	//! \return Distance at which the selection proximity term reaches zero.
	float GetMaxUsefulDistance() { return m_fMaxUsefulDistance; }

	//! The most the objective may add to a routine deployment candidate's sort key.
	float GetObjectiveAnchorWeight() { return m_fObjectiveAnchorWeight; }

	//! \return True when a control change is waiting to be acted on at the next tick.
	bool IsReselectPending() { return m_bReselectPending; }

	//------------------------------------------------------------------------------------------------
	// THE OBJECTIVE INSTANCE
	//------------------------------------------------------------------------------------------------

	//! \return How many objectives are running. 0 while idle, 1 while one is live.
	int GetInstanceCount()
	{
		if (!m_aInstances)
			return 0;

		return m_aInstances.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! One running objective.
	//! \param[in] index Index into the live list.
	//! \return The instance, or null when the index is out of range.
	OVT_ObjectiveInstance GetObjectiveInstance(int index)
	{
		if (!m_aInstances || index < 0 || index >= m_aInstances.Count())
			return null;

		return m_aInstances[index];
	}

	//! \return How many objectives may run at once. See MaxConcurrentObjectives() for the floor.
	int GetMaxConcurrentObjectives() { return MaxConcurrentObjectives(); }

	//! \return The plan the current objective is running, by its persistence key, or an empty string
	//! when there is no objective or no registry.
	string GetObjectiveConfigName()
	{
		if (!m_Instance)
			return "";

		return m_Instance.GetConfigName();
	}

	//! \return The authored name of the phase the current objective is in, or an empty string. THE
	//! PERSISTENCE KEY, and what the Game Master panel shows instead of an enum label.
	string GetObjectivePhaseName()
	{
		if (!m_Instance)
			return "";

		return m_Instance.GetPhaseName();
	}

	//! \return Which phase of its plan the current objective is in, or -1 when it has none.
	int GetObjectivePhaseIndex()
	{
		if (!m_Instance)
			return -1;

		return m_Instance.GetPhaseIndex();
	}

	//------------------------------------------------------------------------------------------------
	//! Where a named phase sits in the RUNNING objective's plan.
	//!
	//! The one place a phase NAME becomes an INDEX for a consumer outside this component: a deployment
	//! config states its span as two phase names (the name is the persistence key and the only thing a
	//! plan and a config can agree on) and needs them located in the plan actually running.
	//!
	//! ⚠ An unknown name, a plan that did not resolve and an objective with no instance all answer
	//! NO_PHASE_INDEX, and the caller must treat that as "this config belongs to no phase" rather than
	//! as index 0 - which is a real phase, and is the first one.
	//! \param[in] name The authored phase name.
	//! \return The plan-phase index, or OVT_ObjectivePlanRules.NO_PHASE_INDEX.
	int IndexOfObjectivePhase(string name)
	{
		if (!m_Instance)
			return OVT_ObjectivePlanRules.NO_PHASE_INDEX;

		OVT_ObjectiveConfig config = m_Instance.GetConfig();
		if (!config)
			return OVT_ObjectivePlanRules.NO_PHASE_INDEX;

		return config.IndexOfPhase(name);
	}

	//! \return How many modules the current phase is running. ZERO MEANS NOTHING IS DRIVING IT - the
	//! objective has no plan behind it and can neither act, advance nor be given up. See
	//! LogObjectiveWithNoPlan(), which is the only symptom.
	int GetRuntimeModuleCount()
	{
		if (!m_Instance)
			return 0;

		return m_Instance.GetRuntimeModuleCount();
	}

	//------------------------------------------------------------------------------------------------
	//! An integer any module has reported at the current objective.
	//! \param[in] key The bag key, e.g. OVT_ObjectiveInstance.BAG_HARASSMENT_SUCCESSES.
	//! \return The value, or zero when nobody has written that key.
	int GetObjectiveBagValue(string key)
	{
		if (!m_Instance)
			return 0;

		return m_Instance.Get(key);
	}

	//------------------------------------------------------------------------------------------------
	//! A position any module has handed forward at the current objective.
	//! \param[in] key The vector-bag key.
	//! \return The position, or the zero vector when nobody has written that key.
	vector GetObjectiveBagPosition(string key)
	{
		if (!m_Instance)
			return vector.Zero;

		return m_Instance.GetPos(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Writes an integer into the current objective's bag.
	//!
	//! ⚠ A PUBLIC MUTATOR MAY NEVER CHANGE PHASE, and this one does not: it writes a number and
	//! returns. Everything that moves this machine moves on DirectorTick(), behind its three early
	//! returns. See ReportObjectiveProgress() for the two red cases the opposite once cost.
	//! \param[in] key The bag key.
	//! \param[in] value The value.
	void SetObjectiveBagValue(string key, int value)
	{
		if (m_Instance)
			m_Instance.Set(key, value);
	}

	//------------------------------------------------------------------------------------------------
	//! Writes a position into the current objective's vector bag. The same rule as above.
	//! \param[in] key The vector-bag key.
	//! \param[in] value The position.
	void SetObjectiveBagPosition(string key, vector value)
	{
		if (m_Instance)
			m_Instance.SetPos(key, value);
	}

	//------------------------------------------------------------------------------------------------
	//! The whole int bag, for the save payload.
	//! \param[out] keys Receives the keys. Cleared first.
	//! \param[out] values Receives the values, same order.
	void ReadObjectiveBag(notnull array<string> keys, notnull array<int> values)
	{
		keys.Clear();
		values.Clear();

		if (m_Instance)
			m_Instance.ReadBag(keys, values);
	}

	//------------------------------------------------------------------------------------------------
	//! The whole vector bag, for the save payload.
	//! \param[out] keys Receives the keys. Cleared first.
	//! \param[out] values Receives the positions, same order.
	void ReadObjectiveBagV(notnull array<string> keys, notnull array<vector> values)
	{
		keys.Clear();
		values.Clear();

		if (m_Instance)
			m_Instance.ReadBagV(keys, values);
	}
}
