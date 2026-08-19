[EntityEditorProps(category: "Overthrow/Managers", description: "Owns the occupying faction's current objective and the three-phase campaign to take it")]
class OVT_ObjectiveDirectorComponentClass : OVT_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! THE OCCUPYING FACTION'S ONE PIECE OF LONG-TERM INTENT.
//!
//! Server-only game-mode component. At most ONE objective exists at any moment - a resistance-held
//! base, town or city that the occupying faction has decided to take back - and it is in exactly one
//! phase. Once per in-game minute this component selects one when it has none, runs the phase
//! machine, and (from Phase 3 of the build onward) biases the deployment evaluator toward it.
//!
//! WHAT REPLACED WHAT. Until occupying/counter-attacks Phase 1 the occupying faction attacked by
//! dice: an hourly 10 % roll dropped a battle on a uniformly random base, and a second trigger
//! dropped one on any town whose support had collapsed while a player happened to be standing near
//! it. Both are deleted. Neither had a build-up, a warning or a reason, and this class exists so the
//! resistance can READ the ramp - see the intent it announces in its own log lines.
//!
//! ⚠ IT IS NOT A MANAGER, AND THE NAME IS DELIBERATE (D1). Every OVT_*ManagerComponent in this tree
//! owns a registry and answers questions about it. This one owns no registry and answers no
//! questions; it makes a DECISION every minute. Nothing else in the campaign may start an offensive
//! operation - that single-decision-maker property is goal G1 and is checked by grep.
//!
//! EVERYTHING IT BUILDS IS A DEPLOYMENT (G4). It never spawns a group itself; it creates deployments
//! through the deployment framework and lets that framework register them with the virtualization
//! core. It never holds money either (G5): every resource it spends leaves the one faction pool at
//! the moment the deployment is created, and the forward base's "budget" is a spend CEILING counted
//! against that pool, not a wallet.
//!
//! THE ARITHMETIC IS NOT IN HERE. Selection scoring, the phase gates, the difficulty clamps and the
//! starvation predicate all live in OVT_ObjectiveSelection and OVT_ObjectivePhaseRules, as pure
//! statics with no world behind them, so the whole machine is assertable in the cheapest test tier.
//! This class does the looking-up and hands them numbers.
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

	//! ⚠ THIS IS AN IDLE CLOCK, NOT A PHASE BUDGET, AND THE desc: SAYS SO BECAUSE THE MEANING CHANGED
	//! (2026-08-19). It used to measure in-game minutes since the phase was ENTERED, which made poverty
	//! and a long walk both look like a wedge: a play-test spent 31 real minutes unable to afford a single
	//! operation, created one, and had it deleted mid-walk 9 minutes later because the phase clock - not
	//! the operation - had run out. It now measures in-game minutes since the objective last made
	//! PROGRESS. See TickObjectiveIdleClock() for what counts as progress and what holds the clock.
	[Attribute(defvalue: "240", desc: "In-game minutes an objective may go WITHOUT PROGRESS before it is abandoned as wedged. An IDLE clock, not a phase budget: every operation created, every operation reported complete and every phase entry re-arms it, and it is held entirely while an operation is still in flight or the faction pool cannot afford the next one. A backstop, not a pacing knob", category: "Objective Director")]
	protected int m_iPhaseTimeoutTicks;

	[Attribute(defvalue: "1", desc: "Selection rounds a failed objective sits out before it may be chosen again", category: "Objective Director")]
	protected int m_iBlacklistRounds;

	[Attribute(defvalue: "25", desc: "The most the objective may add to a routine deployment candidate's SORT KEY, at the objective itself, falling off linearly to nothing at the phase's anchor radius. It biases which suitable work is bought first and never which work is suitable", category: "Objective Director")]
	protected float m_fObjectiveAnchorWeight;

	//------------------------------------------------------------------------------------------------
	// CONSTANTS
	//------------------------------------------------------------------------------------------------

	//! Log prefix, matching the campaign's other server-side systems.
	static const string LOG = "[Overthrow.ObjectiveDirector] ";

	//! Tick period before the time multiplier is applied, in milliseconds.
	//!
	//! ⚠ DELIBERATELY THE SAME NUMBER AS THE OCCUPYING FACTION MANAGER'S OWN UPDATE PERIOD, and the
	//! duplication is the point rather than an oversight: the phase machine and the resource tick have
	//! to advance in step and freeze together, and a "45-minute" harassment cadence has to mean 45
	//! IN-GAME minutes at any time multiplier. It is re-declared rather than read across because that
	//! constant is not static on its own class.
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

	//! How far the objective bias reaches during harassment, in metres.
	//!
	//! TIGHT ON PURPOSE. In this phase the only thing worth leaning on is the objective itself, and a
	//! wide radius here would pull routine garrisoning off the whole surrounding map for a target the
	//! machine has not committed to yet - harassment is the one phase that may still re-select.
	static const float HARASSMENT_ANCHOR_RADIUS = 600;

	//! How far the objective bias reaches from the forward-base phase onward, in metres.
	//!
	//! DELIBERATELY WIDER THAN THE HARASSMENT RADIUS. From here the objective is LOCKED and the
	//! occupying faction is committed, and the forward base stands somewhere BETWEEN its nearest held
	//! base and the objective - so the band that needs garrisoning is the whole approach, not just the
	//! target. The same figure serves the counter-attack phase: the ground being fought over does not
	//! shrink when the battle starts.
	static const float FORWARD_ANCHOR_RADIUS = 1200;

	//! THE HARASSMENT GROUP LADDER, one registered deployment config per rung, in ascending order.
	//!
	//! ⚠ THE RAMP IS CARRIED BY FOUR THIN REGISTRY VARIANTS OF ONE CONFIG, NOT BY A PER-CREATE GROUP
	//! OVERRIDE. Every rung is the same Deployment_ObjectiveHarassment.conf inherited in
	//! overthrowDeployments.conf with a different m_sGroupType, a different cost and a different name -
	//! the Deployment_BaseHeavyPatrol precedent. The alternative, one config plus a director-side
	//! override applied after the create, would be a second way of deciding what a deployment contains,
	//! parallel to the config system and invisible to everything that reads a config: the evaluator's
	//! cost model, the Game Master panel, the reinforcement rebuy and the save all read the CONFIG.
	//!
	//! ⚠ THE NAMES ARE THE KEYS AND THEY ARE MATCHED BY STRING, three times each: FindConfigByName()
	//! resolves the rung, GetDeploymentNearPosition() and the concurrency count match live deployments
	//! back to it, and the teardown ledger stores it. A name changed in overthrowDeployments.conf and
	//! not here does not fail to parse and does not warn - the ramp simply stops sending anything, with
	//! one WARNING line per in-game minute as its only symptom. An initialisation-tier case resolves
	//! every entry here against the live registry for exactly that reason.
	//!
	//! ⚠ ORDER IS THE RAMP. OVT_ObjectivePhaseRules.HarassmentLadderIndex indexes straight into this,
	//! saturating at the top rung; reordering it re-tunes the whole escalation.
	static const ref array<string> HARASSMENT_LADDER = {
		"Objective Harassment (Patrol)",
		"Objective Harassment (Fireteam)",
		"Objective Harassment (Rifle Squad)",
		"Objective Harassment (Heavy)"
	};

	//! The one recapture config, by name. Same string-matching hazard as the ladder above.
	static const string TOWER_RECAPTURE_CONFIG = "Objective Tower Recapture";

	//! The one sabotage config, by name. Same string-matching hazard as the ladder above.
	//!
	//! ⚠ IT HAS NO RUNGS, DELIBERATELY. The harassment ramp escalates because its EFFECT is a stacking
	//! debuff that needs bigger groups to keep landing; a sabotage mission's effect is a structure that
	//! is gone, which does not get more gone with a bigger team. What escalates on a base objective is
	//! the number of missions the counter-attack gate demands, which difficulty already scales - and
	//! inverted, so an easier campaign gets more warnings before the base is attacked, not fewer.
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

	//! How close to a real base a BASE objective's position has to be before sabotage will be sent
	//! there at all.
	//!
	//! ⚠ NOT DEFENSIVE PADDING. A base objective's position IS the base's own location (selection
	//! copies base.location verbatim), so in a live campaign this always passes by metres. What it stops
	//! is a director whose objective was set to a position with no base under it - a restored payload
	//! naming a base that has since gone, or a fixture arranging a state - from creating a real
	//! deployment, spending real resources and eventually demolishing the structures of whichever base
	//! happened to be nearest on the map.
	static const float BASE_OP_RESOLVE_RADIUS = 100;

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

	//------------------------------------------------------------------------------------------------
	// THE COUNTER-ATTACK WINDOW (D17)
	//------------------------------------------------------------------------------------------------

	//! First hour of the day a counter-attack may BEGIN, inclusive.
	//!
	//! ⚠ CONSTS HERE, NOT DIFFICULTY FIELDS, AND DELIBERATELY. The user asked for a fixed window; this
	//! feature already grows OVT_DifficultySettings by eleven fields; and promoting two consts to
	//! authored fields later is a two-line change, while un-shipping two authored fields is a
	//! save-format conversation. The predicate they are handed to is pure and takes both bounds as
	//! arguments precisely so that promotion needs no other edit.
	static const int COUNTER_ATTACK_HOUR_START = 5;

	//! First hour of the day a counter-attack may no longer begin, EXCLUSIVE - so the last minute one
	//! may start is 14:59.
	//!
	//! Ending at 15:00 rather than at dusk is what puts the whole battle in daylight without a second
	//! check anywhere: a counter-attack that starts inside this window has its build-up, its muster and
	//! its fight before dark.
	static const int COUNTER_ATTACK_HOUR_END = 15;

	//! EvaluateCounterAttackGate(): the ramp is not finished. The forward-base phase carries on as it
	//! did, clocks and all.
	static const int COUNTER_ATTACK_NOT_READY = 0;

	//! EvaluateCounterAttackGate(): everything the ramp can control is done and only the clock is in the
	//! way.
	//!
	//! ⚠ THE WAIT IS NOT A FAILURE OF THE OBJECTIVE - it does not spend the phase timeout, does not
	//! re-select and does not blacklist. It is NOT a suspension of the forward-base phase: the garrison
	//! sender still runs and the starvation rule is still evaluated, so the resistance can starve the
	//! base down or dismantle it at 22:00 exactly as it can at midday. See TickFOB() for the full
	//! argument and for the D17 correction it records.
	static const int COUNTER_ATTACK_WAIT_FOR_DAYLIGHT = 1;

	//! EvaluateCounterAttackGate(): start the battle.
	static const int COUNTER_ATTACK_FIRE = 2;

	//------------------------------------------------------------------------------------------------
	// FORWARD-BASE SITING
	//
	// ⚠ THE GENERATED PATH IS THE PRIMARY PATH AND IS TUNED AS IF THE AUTHORED ONE WILL NEVER EXIST
	// (R17). No OVT_FOBPosition marker is placed in any shipped world today - putting them there is a
	// Workbench world-editing job nobody has done - so every constant below is what actually decides
	// where forward bases land. Authored markers are an optimisation a map author may apply on top,
	// and they win when they exist.
	//------------------------------------------------------------------------------------------------

	//! Nearest the forward base may stand to the objective, as a fraction of the way back to the base
	//! supplying it. A third of the way out: any closer and it is inside the objective's own defended
	//! ground, where it would be found in the first minute and could not be supplied.
	static const float FOB_BAND_MIN_FRACTION = 0.35;

	//! Furthest the forward base may stand from the objective, as the same fraction. Three quarters of
	//! the way out, so the base is meaningfully FORWARD of the rear rather than a second flag beside
	//! the one the faction already has - which is the whole point of the middle phase.
	static const float FOB_BAND_MAX_FRACTION = 0.75;

	//! Absolute floor on the standoff from the objective, in metres, whatever the fractions work out
	//! to. A short supply line would otherwise put a forward base 80 m outside a town.
	static const float FOB_MIN_STANDOFF = 350;

	//! Absolute ceiling on the standoff, in metres. Past this the base is not supporting the objective
	//! in any sense a player could read, and the counter-attack's waves would come from nowhere near it.
	static const float FOB_MAX_STANDOFF = 2500;

	//! Steps sampled along the supply line. Eight spreads the samples about 40 m apart over a typical
	//! band, which is finer than the clearance box the trace tests with.
	static const int FOB_SITING_STEPS = 8;

	//! Lateral lanes sampled at each step: on the line, then one spread either side of it. Three,
	//! because a site straight down the supply road is the shortest resupply and should be tried first,
	//! and the two offsets are what find the field behind the treeline when it is not clear.
	static const int FOB_SITING_LANES = 3;

	//! ⚠ THE ATTEMPT BOUND, AND IT IS THE PRODUCT OF THE TWO ABOVE RATHER THAN A NUMBER SOMEBODY LIKED.
	//! Twenty-four is FOB_SITING_STEPS x FOB_SITING_LANES: the sampler walks a deterministic lattice and
	//! this is simply how many points are in it. It is a BOUND, not a budget - the search returns on the
	//! first candidate that passes every hard test, which on open terrain is usually the first or
	//! second. It is small on purpose: each attempt costs an ocean read, a TraceBox and five surface
	//! samples, and this runs on the director's once-a-minute tick, where a few hundred attempts would
	//! be a visible hitch on a dedicated server for a decision that is made once per objective.
	static const int FOB_SITING_ATTEMPTS = FOB_SITING_STEPS * FOB_SITING_LANES;

	//! How far each lateral lane pushes a sample off the supply line, in metres.
	static const float FOB_LATERAL_SPREAD = 250;

	//! Radius of the four ground probes that judge how level a candidate is, in metres. Slightly wider
	//! than the structure's own footprint so a site straddling the lip of a ditch is caught.
	static const float FOB_FLATNESS_PROBE_RADIUS = 8;

	//! Height spread across those probes at which a candidate is rejected outright, in metres. Two and
	//! a half metres over a sixteen-metre span is about a 1-in-6 slope; a structure on more than that
	//! floats at one corner and sinks at another, which is the one siting failure everybody can see.
	static const float FOB_FLATNESS_TOLERANCE = 2.5;

	//! Height advantage over the objective at which the elevation preference saturates, in metres.
	static const float FOB_ELEVATION_USEFUL_GAIN = 30;

	//! Half-width of the clearance box traced at each candidate, in metres.
	static const float FOB_CLEAR_BOX_HALF = 6;

	//! Height of that box, in metres.
	static const float FOB_CLEAR_BOX_HEIGHT = 8;

	//! Nothing may be built this close to a base of ANY faction, in metres. Matches the radius the
	//! placement limit already treats as "belonging to a base" (OVT_ItemLimitChecker's own figure for
	//! EOVTBaseType.BASE), so a forward base is never inside ground another system considers spoken for.
	static const float FOB_CLEARANCE_BASE = 500;

	//! Nothing may be built this close to a resistance forward base or camp, in metres. Tighter than a
	//! military base because a camp is a small thing, and generous enough that the occupying faction
	//! never plants a flag inside a player's own site.
	static const float FOB_CLEARANCE_RESISTANCE_SITE = 300;

	//! Added to a resistance-held town's own range to get its exclusion radius, in metres. The town
	//! range is where the campaign already stops counting a place as "in the town"; the margin keeps a
	//! forward base out of the fields immediately outside it, where it would be as visible as inside.
	static const float FOB_CLEARANCE_TOWN_MARGIN = 150;

	//! How far from the recorded forward-base position a deployment or a structure counts as belonging
	//! to it. Used by the garrison cap, the starvation count and the teardown sweep, so all three agree
	//! about what "at the forward base" means.
	static const float FOB_AREA_RADIUS = 250;

	//! How far from the recorded position the teardown looks for the structure itself, in metres.
	//! Tight: the structure is spawned AT that position, so anything further away is something else.
	static const float FOB_STRUCTURE_SEARCH_RADIUS = 60;

	//! How close a player has to be to the flag to dismantle a forward base, in metres. The action is
	//! performed on the flag from interaction range; this is the SERVER's re-derivation of that, and it
	//! is deliberately generous because the flag's origin is not where the player is standing.
	static const float FOB_DISMANTLE_RANGE = 30;

	//! No occupying-faction soldier may be alive within this many metres of the flag while a player
	//! dismantles it. The forward base has to be CLEARED, not merely reached - otherwise the fifteen
	//! second hold is something a player does while being shot at, which reads as a bug rather than a
	//! challenge.
	static const float FOB_DEFENDER_CLEAR_RADIUS = 150;

	//------------------------------------------------------------------------------------------------
	// STATE
	//------------------------------------------------------------------------------------------------

	//! The current objective. Never null after Init(); an absent objective is kind NONE, phase IDLE.
	protected ref OVT_ObjectiveRecord m_Objective;

	//! The forward operating base for the current objective. Never null after Init(); an absent one
	//! has up == false.
	protected ref OVT_ObjectiveFOBRecord m_FOB;

	//! Objectives serving a cooldown after a failure.
	protected ref array<ref OVT_ObjectiveBlacklistEntry> m_aBlacklist;

	//! Deployments this director created for the current objective, so the reset path can take exactly
	//! those back down. Runtime-only and cleared on every reset.
	protected ref array<ref OVT_ObjectiveDeploymentRef> m_aCreatedDeployments;

	//! Set by both control-change handlers, consumed at the top of the next tick. NEVER acted on
	//! inline - see OnBaseControlChanged() for why.
	protected bool m_bReselectPending;

	//! True while a restored payload still has to be re-linked to the live campaign on a tick.
	protected bool m_bRestorePending;

	//! Re-link attempts already spent, against FOB_RELINK_ATTEMPTS.
	protected int m_iRelinkAttempts;

	//! True once the "nothing worth taking" line has been logged, so an early campaign does not print
	//! it once per in-game minute forever. Cleared the moment anything is selectable again.
	protected bool m_bIdleLogged;

	//! True once the forward base's own deployment has been created for this objective, whether or not
	//! the structure is standing yet.
	//!
	//! ⚠ RUNTIME-ONLY AND DELIBERATELY NOT PERSISTED. A save taken while the supply truck was still on
	//! the road comes back with no truck, no convoy and a deployment that may raise nothing (D11 gates
	//! the raise on a restored deployment), so the honest restored state is "no forward base was sent"
	//! and the phase sends another one - after collecting the stranded marker, which is what
	//! FindLiveFOBDeployment() is for.
	protected bool m_bFOBDeploymentSent;

	//! Where the forward base's deployment was created. Held separately from m_FOB.position, which is
	//! only written once the structure is actually standing, so the teardown can still find a marker
	//! whose raise never completed.
	protected vector m_vFOBSite;

	//! True while the forward base is currently reported as cut off, so the two transitions are each
	//! logged once rather than once per in-game minute.
	protected bool m_bStarvationLogged;

	//! True once the "the ceiling is spent" line has been logged for the current forward base. Cleared
	//! whenever the forward-base record is.
	protected bool m_bCeilingLogged;

	//! True once the "waiting for daylight" line has been logged for the current objective.
	//!
	//! ⚠ ONCE PER OBJECTIVE, NOT ONCE PER TICK (D17). This is a once-a-minute tick and the wait can last
	//! most of an in-game day, so an unlatched line would put several hundred entries in the log for one
	//! entirely normal state. Cleared with the objective record, so the next objective says it again.
	protected bool m_bDaylightWaitLogged;

	//! True once a "the gate passed but the battle could not be started" line has been logged for the
	//! current objective. Same reason as the latch above: the gate is re-asked every in-game minute and
	//! every one of its refusals persists until the objective ends.
	protected bool m_bCounterAttackRefusalLogged;

	//! True once the "it cannot afford its next operation" line has been logged for the current
	//! objective.
	//!
	//! ⚠ ONCE PER OBJECTIVE, NOT ONCE PER TICK, and it is MANDATORY rather than a nicety. A refused
	//! create leaves the cadence at zero so the next tick asks again a minute later, which means poverty
	//! is retried every in-game minute for as long as it lasts - unlatched this would be hundreds of
	//! identical lines. Latched away entirely it would be what the play-test actually saw: 31 real minutes
	//! of a director doing nothing with not one line in the log to say why. Cleared with the objective
	//! record. The RESUME marker is the ordinary "Sent '<config>' ... for N resources" line that follows.
	protected bool m_bAffordabilityBlockLogged;

	//! Set during a tick on which a create was refused because the faction pool was short, and consumed
	//! by that same tick's idle clock.
	//!
	//! ⚠ PER TICK, NOT PERSISTENT STATE. It is cleared at the top of every phase handler that can spend
	//! and read exactly once at the bottom of it, so it can never freeze a clock on the strength of a
	//! refusal from some earlier minute. It is deliberately NOT set by the forward base's spend CEILING:
	//! a ceiling that is spent is a decision the director made about itself, and a phase that can no
	//! longer buy anything for that reason SHOULD time out.
	protected bool m_bBlockedOnAffordability;

	//! What m_Objective.harassmentSuccesses read the last time the idle clock was re-armed.
	//!
	//! ⚠ THE SUCCESS SIGNAL IS OBSERVED ON THE TICK, NEVER PUSHED FROM THE COUNTER. OnHarassmentSuccess()
	//! and OnSabotageSuccess() are public, are called from a deployment's own update, from a restore and
	//! from test fixtures, and are pinned by two cases as "IT COUNTS, IT DOES NOT DECIDE" - re-arming a
	//! timer from either of them would break D4 ("only a tick may move a timer") in exactly the way Phase
	//! 5 already paid for twice. So the tick compares the counters against these marks instead.
	protected int m_iProgressHarassmentMark;

	//! What m_Objective.sabotageSuccesses read the last time the idle clock was re-armed. Same rule.
	protected int m_iProgressSabotageMark;

	// ⚠ THE CLOCK HANDLE IS INHERITED, NOT DECLARED HERE. OVT_Component already carries m_Time and fills
	// it in its own OnPostInit, and the occupying faction manager reads the world's hour off exactly that
	// field. Re-declaring it would shadow the base class's copy with one nothing else fills in - see
	// ResolveWorldHour(), which re-resolves it lazily for the one case OnPostInit skips.

	//! Curated forward-base markers found by the in-flight siting query. Rebuilt at the start of every
	//! search and dropped at the end of it, in the OVT_SniperMarkerPlacementProvider shape: a marker can
	//! be deleted with whatever it was placed on, so nothing here is cached across calls.
	protected ref array<IEntity> m_aFoundFOBMarkers;

	//! Forward-base structures found by the in-flight teardown query. Same rule.
	protected ref array<IEntity> m_aFoundFOBStructures;

	//! What the teardown query is matching on. Held in a member because the engine's filter callback
	//! takes only the candidate entity, so a per-candidate config resolve is the alternative.
	protected ResourceName m_sFOBStructurePrefab;

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
	//! ⚠ ALLOCATED ON CLIENTS TOO, even though every DECISION here is server-only. The records are a
	//! few dozen bytes and the getters are public: a client that resolves the director through
	//! OVT_Global and asks it a question must get "no objective" rather than a null dereference.
	//! Behaviour is gated at the entry points - the tick, the initialisation and the subscriptions -
	//! not by leaving the state half-built.
	//!
	//! ⚠ THIS RUNS IN THE WORLD EDITOR TOO, AND NOTHING HERE MAY ASK A MANAGER ANYTHING. Loading a
	//! world in the editor constructs the game mode entity's components without starting a game, so
	//! GetGame().GetFactionManager() is null, the deployment framework is null and the campaign does
	//! not exist. Allocating records is all this method is allowed to do. It used to call
	//! ClearObjectiveRecord(), whose whole job is dropping the deployment bias, and the resolve chain
	//! behind that drop took the editor down with a NULL faction manager (2026-08-19). Construction has
	//! no anchor to drop - the component has never pushed one - so it initialises the record fields
	//! directly and leaves the drop to the runtime paths that genuinely end an objective.
	//! \param owner The game mode entity.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_Objective = new OVT_ObjectiveRecord();
		m_FOB = new OVT_ObjectiveFOBRecord();
		m_aBlacklist = new array<ref OVT_ObjectiveBlacklistEntry>();
		m_aCreatedDeployments = new array<ref OVT_ObjectiveDeploymentRef>();

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
	//! THREE EARLY RETURNS, IN THIS ORDER, AND THE ORDER IS THE CONTRACT:
	//!  1. NOT THE SERVER. Nothing here has a client half.
	//!  2. NOBODY ONLINE. Parity with the occupying faction manager's own tick: an empty server does
	//!     not run a war. Without it a dedicated server left overnight would ramp an objective to a
	//!     counter-attack that nobody was there to see start.
	//!  3. A BATTLE IS LIVE. ⚠ THE FREEZE. m_CurrentQRF is the campaign's single-battle contract -
	//!     the occupying faction manager refuses to start a second one while it is set, and clears it
	//!     only when the battle resolves. While it is set the whole objective machine stops: no phase
	//!     advances, no operation is sent, and because every timer is a TICK COUNT rather than a
	//!     deadline (D4), not one of them moves either. There is no catching up to do afterwards
	//!     because nothing fell behind.
	void DirectorTick()
	{
		if (!Replication.IsServer())
			return;

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

		switch (m_Objective.phase)
		{
			case OVT_EObjectivePhase.IDLE:
			{
				// ⚠ NOT A SECOND TIME IN ONE TICK. A re-selection request that found no candidate leaves
				// the machine idle, and running selection again here would serve a SECOND round off every
				// blacklist entry in the same in-game minute - so a place meant to sit out one round
				// would quietly sit out none.
				if (!alreadySelected)
					SelectObjective();

				break;
			}

			case OVT_EObjectivePhase.HARASSMENT:
			{
				TickHarassment();
				break;
			}

			case OVT_EObjectivePhase.FOB:
			{
				TickFOB();
				break;
			}

			case OVT_EObjectivePhase.COUNTER_QRF:
			{
				TickCounterQRF();
				break;
			}
		}
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

		if (m_Objective.phase != OVT_EObjectivePhase.IDLE && m_Objective.phase != OVT_EObjectivePhase.HARASSMENT)
			return false;

		SelectObjective();

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Phase 1. Sends operations at the objective and watches for the gate into the forward-base phase.
	//!
	//! FOUR THINGS, IN THIS ORDER, AND THE ORDER IS THE CONTRACT:
	//!  1. THE CADENCE. The countdown to the next operation serves one tick, behind all three of the
	//!     tick's early returns.
	//!  2. THE GATE OUT, before any spending. A town already below the threshold - because the last
	//!     operation landed, or because the resistance lost support to something else entirely - should
	//!     advance rather than buy one more squad it no longer needs.
	//!  3. AT MOST ONE OPERATION, and only once the cadence has elapsed.
	//!  4. THE IDLE CLOCK, LAST, because it is the only thing here that has to know whether the tick
	//!     accomplished anything. See TickObjectiveIdleClock().
	//!
	//! ⚠ THE IDLE CLOCK MOVED FROM THE TOP OF THIS METHOD TO THE BOTTOM (2026-08-19) AND THE ORDER IS
	//! PART OF THE FIX. Served first, it could only ever mean "minutes since the phase began", and it
	//! abandoned an objective on a tick that was about to advance the phase or about to send the very
	//! operation the phase was waiting for. Served last, the same budget means "minutes since anything
	//! happened", and the tick that finally sends something re-arms rather than gives up.
	//!
	//! ⚠ EVERY SPEND IS BEHIND THE CADENCE, INCLUDING TOWER RECAPTURE. The first cut of this method
	//! sent recapture teams ahead of the cadence check, on the reasoning that a tower is a discrete
	//! job that should not queue behind a harassment interval. That was wrong twice over: it is an
	//! UNBOUNDED PER-TICK SPENDER - an objective covered by three resistance-held towers drops three
	//! deployments in one in-game minute, which is the unpaced lurch this feature exists to replace -
	//! and it makes the only rule the pool has ("the director spends once per cadence interval") depend
	//! on which kind of operation it happened to be. The phase has 240 in-game minutes and the cadence
	//! is 45 on Normal, so there is room for five operations; towers in range of one objective are
	//! typically none or one.
	//!
	//! ⚠ SABOTAGE SHARES THE CADENCE, IT DOES NOT RUN BESIDE IT. A base objective's Phase 1 work is
	//! sabotage and a town's is harassment; both go through SendNextOperation(), which sends AT MOST ONE
	//! operation per interval whichever kind it is. Giving base operations a spender of their own would
	//! reopen exactly the unbounded-per-tick hole tower recapture was moved out of.
	protected void TickHarassment()
	{
		m_bBlockedOnAffordability = false;

		AdvanceOperationCadence();

		if (CheckHarassmentGate())
			return;

		bool created = false;
		if (m_Objective.nextOpTicks == 0)
			created = SendNextOperation();

		if (TickObjectiveIdleClock(created))
			ResetObjective("harassment did nothing at all for " + m_iPhaseTimeoutTicks.ToString() + " in-game minutes and never reached the forward-base gate", true);
	}

	//------------------------------------------------------------------------------------------------
	//! Sends ONE operation at the objective, and re-arms the cadence only if one was actually sent.
	//!
	//! TOWER RECAPTURE FIRST, because it is the most urgent and the most bounded of the three: a tower
	//! is a discrete thing that is either being worked on or is not, the sender deduplicates against
	//! the live deployment, and a tower left in resistance hands keeps the objective easier for the
	//! resistance to hold. The other two have no such ceiling and will still be there next interval.
	//!
	//! ⚠ HARASSMENT AND SABOTAGE ARE MUTUALLY EXCLUSIVE BY OBJECTIVE KIND, not by priority. A town
	//! objective refuses sabotage and a base objective refuses harassment - each sender's first line -
	//! so the order they are asked in here is arbitrary and only one of them can ever answer. They are
	//! chained rather than branched so that adding a third kind of Phase 1 work needs no new plumbing.
	//!
	//! ⚠ THE COUNTDOWN IS ONLY RE-ARMED ON A SUCCESSFUL CREATE. Every refusal - nothing to recapture,
	//! nothing to sabotage, the concurrency cap is full, the pool is short, a config is missing - leaves
	//! nextOpTicks at zero so the next tick asks again a minute later, instead of waiting out another
	//! whole interval for a condition that may have cleared immediately. That retry is also what makes
	//! the affordability hold cover a whole poverty spell rather than one tick in forty-five: every
	//! minute of it reaches the spender and is refused again.
	//! \return True when an operation was created and paid for - which is PROGRESS, and re-arms the idle
	//!         clock.
	protected bool SendNextOperation()
	{
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (!difficulty)
			return false;

		if (!SendTowerRecaptureOperation() && !SendHarassmentOperation() && !SendSabotageOperation())
			return false;

		SetOperationCountdown(difficulty.objectiveHarassmentIntervalMinutes);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Sends ONE harassment group at a town objective, if the cap and the pool allow.
	//! \return True when a deployment was created and paid for.
	protected bool SendHarassmentOperation()
	{
		// Towns and cities only. A base objective's Phase 1 work is sabotage - SendSabotageOperation().
		if (m_Objective.kind != OVT_EObjectiveKind.TOWN)
			return false;

		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();

		if (!deployments || !config || !difficulty)
			return false;

		int occupyingIndex = config.GetOccupyingFactionIndex();

		if (CountLiveHarassmentOperations(deployments, occupyingIndex) >= difficulty.objectiveHarassmentMaxConcurrent)
			return false;

		int rung = OVT_ObjectivePhaseRules.HarassmentLadderIndex(m_Objective.harassmentSuccesses, HARASSMENT_LADDER.Count());
		if (rung == OVT_ObjectivePhaseRules.NO_LADDER_RUNG)
			return false;

		return CreateObjectiveDeployment(deployments, HARASSMENT_LADDER[rung], m_Objective.position, occupyingIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! Sends ONE recapture team, at the first radio tower the resistance holds that still covers the
	//! objective and does not already have a team on the way.
	//!
	//! ⚠ ONE PER CALL, NOT ONE PER TOWER. See SendNextOperation() for why every spend is behind the
	//! cadence. A second contested tower is picked up on the next interval.
	//!
	//! ⚠ DEDUPLICATED AGAINST THE LIVE DEPLOYMENT, not against the director's teardown ledger: that
	//! ledger only grows until a reset, so a team that was wiped out or collected is still in it and
	//! would block its tower from ever being worked on again.
	//!
	//! ⚠ A SABOTAGED TOWER IS NOT A TARGET, because GetRadioTowersAffecting() skips towers that are off
	//! the air. That is correct rather than an oversight: a tower broadcasting nothing is not helping
	//! the resistance hold the objective, and it becomes a target again on its own when it recovers.
	//! \return True when a deployment was created and paid for.
	protected bool SendTowerRecaptureOperation()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!occupying || !deployments || !config)
			return false;

		int occupyingIndex = config.GetOccupyingFactionIndex();

		array<OVT_RadioTowerData> towers = occupying.GetRadioTowersAffecting(m_Objective.position);
		if (!towers)
			return false;

		foreach (OVT_RadioTowerData tower : towers)
		{
			if (!tower)
				continue;

			if (tower.faction == occupyingIndex)
				continue;

			if (deployments.GetDeploymentNearPosition(TOWER_RECAPTURE_CONFIG, tower.location, TOWER_OP_DEDUP_RADIUS))
				continue;

			return CreateObjectiveDeployment(deployments, TOWER_RECAPTURE_CONFIG, tower.location, occupyingIndex);
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Sends ONE sabotage team at a base objective, if the cap and the pool allow.
	//!
	//! 🔴 THIS IS THE ONE OPERATION THAT DESTROYS PLAYER PROPERTY PERMANENTLY. What it buys is a team
	//! that walks into a base the resistance holds and demolishes the structures built there, cheapest
	//! first, with no refund, no rubble and nothing to repair. Everything the module does to make that
	//! survivable - the hold, the nobody-nearby rule, the per-mission cap, the one notification - is
	//! documented on OVT_BaseSabotageBehaviorDeploymentModule; what is decided HERE is only how often
	//! one may be sent, which is the same cadence and the same cap the town ramp uses.
	//!
	//! ⚠ THE OBJECTIVE POSITION MUST STILL BE A RESISTANCE-HELD BASE. Selection copies base.location
	//! verbatim, so in a live campaign this resolves by metres - but a restored payload naming a base
	//! that has since been retaken, or an objective committed at an arbitrary position, must not buy a
	//! team that then strips whichever base was nearest. See BASE_OP_RESOLVE_RADIUS.
	//!
	//! ⚠ NO RUNGS AND NO ESCALATION. See SABOTAGE_CONFIG.
	//! \return True when a deployment was created and paid for.
	protected bool SendSabotageOperation()
	{
		// Bases only. A town objective's Phase 1 work is harassment - SendHarassmentOperation().
		if (m_Objective.kind != OVT_EObjectiveKind.BASE)
			return false;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();

		if (!occupying || !deployments || !config || !difficulty)
			return false;

		int occupyingIndex = config.GetOccupyingFactionIndex();

		OVT_BaseData base = occupying.GetNearestBase(m_Objective.position);
		if (!base)
			return false;

		if (vector.Distance(base.location, m_Objective.position) > BASE_OP_RESOLVE_RADIUS)
			return false;

		// Somebody took it back while the ramp was running. The objective is over on the next
		// control-change reselect; sending a team to strip our own base in the meantime is not.
		if (base.faction == occupyingIndex)
			return false;

		// ⚠ THE SAME CONCURRENCY KNOB AS THE TOWN RAMP, DELIBERATELY. objectiveHarassmentMaxConcurrent
		// is authored as "how many objective operations may be alive at once", not as a town-only
		// figure, and the twelve difficulty fields Phase 1 authored contain no separate sabotage cap. A
		// thirteenth field would have to be added to five presets to say the same number twice.
		if (CountLiveSabotageOperations(deployments, occupyingIndex) >= difficulty.objectiveHarassmentMaxConcurrent)
			return false;

		return CreateObjectiveDeployment(deployments, SABOTAGE_CONFIG, base.location, occupyingIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! THE ONE PLACE THE DIRECTOR SPENDS ANYTHING, and the one place it may ever be done.
	//!
	//! ⚠ ForceCreateDeployment DOES NOT DEBIT THE POOL. It forwards to CreateDeployment, which only
	//! STAMPS the cost onto the deployment so a refund and the Game Master snapshot read a real number.
	//! The evaluation path debits separately, right after its own create; a forcing caller has to do the
	//! same or the faction spends money it still has. This method is the whole of the director's side of
	//! goal G5 - "every resource leaves the pool exactly once, through SubtractFactionResources, at the
	//! moment the deployment is created" - and there is deliberately no second spending path anywhere in
	//! this component.
	//!
	//! ⚠ DEBIT ONLY AFTER A SUCCESSFUL CREATE. A create can be refused (an unregistered config name, a
	//! missing deployment prefab, a spawn that failed), and debiting a refusal would burn the faction's
	//! reserve on nothing, every tick, silently.
	//! \param[in] deployments The deployment framework.
	//! \param[in] configName The registered config to run.
	//! \param[in] position Where to put it.
	//! \param[in] factionIndex The occupying faction.
	//! \return True when a deployment was created and the pool debited for it.
	protected bool CreateObjectiveDeployment(notnull OVT_DeploymentManagerComponent deployments, string configName, vector position, int factionIndex)
	{
		OVT_DeploymentConfig config = deployments.FindConfigByName(configName);
		if (!config)
		{
			Print(LOG + "No deployment config named '" + configName + "' is registered - the operation cannot be sent", LogLevel.WARNING);
			return false;
		}

		int cost = config.GetTotalResourceCost();

		// ⚠ THE ONE REFUSAL THAT IS NOT THE OBJECTIVE'S FAULT, AND THE ONLY ONE THAT RAISES THE FLAG.
		// Being broke is a fact about the FACTION, and abandoning this objective for it would find the
		// next one exactly as unaffordable - which is the treadmill a play-test walked for 31 real
		// minutes. The flag holds this tick's idle clock; see TickObjectiveIdleClock(). Every other
		// refusal below (a missing config, the forward base's own spend ceiling, a create the framework
		// declined) is either a fault to be fixed or a decision the director made about itself, and a
		// phase that can only ever hit one of those SHOULD time out.
		if (deployments.GetFactionResources(factionIndex) < cost)
		{
			m_bBlockedOnAffordability = true;
			return false;
		}

		// ⚠ TWO TESTS, NOT ONE, AND THEY ASK DIFFERENT QUESTIONS. The pool test is "can the faction
		// afford this at all"; the ceiling test is "has this forward base already had its share". The
		// second is inactive during harassment, so Phase 1 behaves exactly as it did before the forward
		// base existed. See WithinFOBBudget() for why neither of them moves any money.
		if (!WithinFOBBudget(cost))
			return false;

		OVT_DeploymentComponent created = deployments.ForceCreateDeployment(config, position, factionIndex, cost, 0);
		if (!created)
			return false;

		// ⚠ THE LINE THE WHOLE ACCOUNTING IDENTITY RESTS ON. See the header.
		deployments.SubtractFactionResources(factionIndex, cost);

		// AFTER the pool, never instead of it: this is a counter recording what already left.
		CountFOBSpend(cost);

		TrackObjectiveDeployment(configName, position);

		Print(LOG + "Sent '" + configName + "' at " + position.ToString() + " for " + cost.ToString() + " resources", LogLevel.NORMAL);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! How many harassment operations are alive at the objective right now.
	//!
	//! COUNTED FROM THE LIVE DEPLOYMENT LIST, NOT FROM m_aCreatedDeployments. The tracked list is a
	//! teardown ledger that only grows until a reset; an operation that completed its hold, was wiped
	//! out or was collected by its own condition module is still in it and must not hold a concurrency
	//! slot. Every rung counts towards the same cap, because a rung is the same operation with bigger
	//! men in it.
	//! \param[in] deployments The deployment framework.
	//! \param[in] factionIndex The occupying faction.
	//! \return The count.
	protected int CountLiveHarassmentOperations(notnull OVT_DeploymentManagerComponent deployments, int factionIndex)
	{
		array<OVT_DeploymentComponent> nearby = deployments.GetDeploymentsInRadius(m_Objective.position, HARASSMENT_OP_RADIUS);
		if (!nearby)
			return 0;

		int count = 0;
		foreach (OVT_DeploymentComponent deployment : nearby)
		{
			if (!deployment)
				continue;

			if (deployment.GetControllingFaction() != factionIndex)
				continue;

			if (HARASSMENT_LADDER.Find(deployment.GetDeploymentName()) == -1)
				continue;

			count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! How many sabotage missions are alive at the objective right now.
	//!
	//! COUNTED FROM THE LIVE DEPLOYMENT LIST, NOT FROM m_aCreatedDeployments, for the reason
	//! CountLiveHarassmentOperations() gives: the tracked list is a teardown ledger that only grows
	//! until a reset, so a mission that reported, was wiped out or was collected is still in it.
	//! \param[in] deployments The deployment framework.
	//! \param[in] factionIndex The occupying faction.
	//! \return The count.
	protected int CountLiveSabotageOperations(notnull OVT_DeploymentManagerComponent deployments, int factionIndex)
	{
		array<OVT_DeploymentComponent> nearby = deployments.GetDeploymentsInRadius(m_Objective.position, SABOTAGE_OP_RADIUS);
		if (!nearby)
			return 0;

		int count = 0;
		foreach (OVT_DeploymentComponent deployment : nearby)
		{
			if (!deployment)
				continue;

			if (deployment.GetControllingFaction() != factionIndex)
				continue;

			if (deployment.GetDeploymentName() != SABOTAGE_CONFIG)
				continue;

			count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the objective has been softened enough to raise the forward operating base, and if so,
	//! advances to it.
	//!
	//! ⚠ CALLED FROM THE HARASSMENT TICK AND NOWHERE ELSE. It is the only phase transition Phase 1 can
	//! make, so it lives where every other one does - behind DirectorTick()'s three early returns. See
	//! OnHarassmentSuccess() for what happened when it was also called from there.
	//!
	//! ⚠ TWO CONJUNCTS, AND THE SECOND IS NOT IN THE PLAN'S DIAGRAM. §3.2 draws the transition as
	//! "town: support < 50 %" alone. Wired that literally, the gate fires on the FIRST tick of the
	//! phase for any town already under the threshold, and the whole harassment phase is skipped:
	//!
	//!   - IT BREAKS G3, the reason the feature exists. "Between the first harassment group arriving
	//!     and the counter-QRF there are tens of in-game minutes of visible activity" cannot be true of
	//!     a ramp that can advance before it has sent anything. A collapsed town is already rewarded by
	//!     SELECTION, which scores low support heavily (§3.4) - the prize is being CHOSEN, not being
	//!     allowed to skip the phase.
	//!   - IT MAKES PROGRESS UNATTRIBUTABLE. "Why did the occupying faction move to this phase" must
	//!     always have an answer, and "the town was already like that" is not one.
	//!   - IT IS WHAT MADE THE PHASE TIMEOUT LOOK LIKE IT RE-ARMED. A gate that fires on the entry tick
	//!     calls EnterPhase(), which legitimately re-arms the timeout - so a planted countdown read back
	//!     as a fresh one and the tick looked like it had failed to decrement. The re-arm was never the
	//!     bug; firing on that tick at all was.
	//!
	//! ⚠ THE SECOND CONJUNCT IS THE TOWN CARRYING THIS FEATURE'S OWN DEBUFF - NOT THE SUCCESS COUNTER.
	//! Both were tried and the counter is not enough. m_iHarassmentSuccesses is a plain integer that
	//! OnHarassmentSuccess() will raise for anybody who asks, so it records that operations were
	//! REPORTED, not that anything happened in the world; a caller arranging a mid-ramp state by
	//! bumping it three times satisfies a counter test while the town it names has never been touched.
	//! The modifier is the causal link itself: the town is below the threshold BECAUSE this ramp put a
	//! stack of ObjectiveHarassment on it. Nothing else in the campaign applies that modifier, so
	//! nothing else can open this gate.
	//!
	//! IT IS ALSO THE RIGHT ANSWER FOR A RESTORE, which the counter is not: town modifiers are
	//! persisted, so a campaign loaded mid-ramp still carries the debuff and still qualifies, with no
	//! session-local "have I sent one yet" state to rebuild.
	//!
	//! NOT A WEDGE RISK. If the debuff has expired the gate refuses, the ramp sends another operation,
	//! and that operation re-applies it - and if the ramp cannot make progress at all, the phase
	//! timeout gives up and blacklists, loudly. There is no path here that stops without a log line.
	//!
	//! ⚠ THE CONJUNCT IS HERE, NOT IN OVT_ObjectivePhaseRules.TownPhase2Gate. That static answers one
	//! question - "is this town soft enough" - and its signature is pinned by Phase 2 Logic cases on
	//! both sides of the threshold. "Did this ramp do it" is the DIRECTOR's question, and folding it
	//! into the static would change a Phase 2 contract to fix a Phase 5 mistake.
	//! \return True when the phase advanced, so the caller stops working on this phase.
	protected bool CheckHarassmentGate()
	{
		if (m_Objective.kind == OVT_EObjectiveKind.BASE)
			return CheckBaseHarassmentGate();

		if (m_Objective.kind != OVT_EObjectiveKind.TOWN)
			return false;

		// THE RAMP HAS TO HAVE DONE IT. See the header.
		if (!ObjectiveTownCarriesHarassmentDebuff())
			return false;

		int support = ResolveObjectiveSupportPercentage();
		if (support < 0)
			return false;

		if (!OVT_ObjectivePhaseRules.TownPhase2Gate(support))
			return false;

		Print(LOG + "Objective '" + m_Objective.name + "' is down to " + support.ToString() + "% support - raising a forward operating base", LogLevel.NORMAL);

		EnterPhase(OVT_EObjectivePhase.FOB);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The BASE half of the forward-base gate: one completed sabotage mission.
	//!
	//! ⚠ CALLED FROM CheckHarassmentGate() AND THEREFORE FROM THE HARASSMENT TICK AND NOWHERE ELSE, for
	//! the same reason everything else in this machine is: a phase entry re-arms the phase timeout, so a
	//! transition reachable from anything but a tick silently overwrites planted timers and can save a
	//! phase nobody asked for. ⚠ In particular OnSabotageSuccess() MUST NOT call this - see its header,
	//! and see OnHarassmentSuccess() for the two red cases the equivalent line cost in Phase 5.
	//!
	//! ⚠ WHY THE COUNTER IS ENOUGH HERE WHEN IT IS NOT ENOUGH FOR A TOWN. The town gate needs a second,
	//! causal conjunct because a town can already be under its support threshold when it is CHOSEN, so
	//! "the town is soft" is not evidence that "this ramp softened it" and the gate could fire on the
	//! phase's entry tick. sabotageSuccesses cannot do that: CommitObjective zeroes it, nothing in the
	//! campaign but a completed sabotage mission raises it, and one has to have been sent, driven to the
	//! base, held it unopposed and demolished something to raise it once. The counter IS the world fact
	//! here - which is also why it round-trips a save correctly with no session-local state to rebuild.
	//! \return True when the phase advanced, so the caller stops working on this phase.
	protected bool CheckBaseHarassmentGate()
	{
		if (!OVT_ObjectivePhaseRules.BasePhase2Gate(m_Objective.sabotageSuccesses))
			return false;

		Print(LOG + "Objective '" + m_Objective.name + "' has taken " + m_Objective.sabotageSuccesses.ToString() + " sabotage mission(s) - raising a forward operating base", LogLevel.NORMAL);

		EnterPhase(OVT_EObjectivePhase.FOB);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the objective town is carrying the debuff THIS ramp applies.
	//!
	//! The causal half of the forward-base gate - see CheckHarassmentGate() for why the success counter
	//! cannot do this job. Read the same way OVT_MapInfluenceLayer reads it: resolve the name to an
	//! index through the modifier system (the index is what the replicated per-town list carries) and
	//! look for that index on the town.
	//! \return True when the objective is a town currently carrying at least one stack.
	protected bool ObjectiveTownCarriesHarassmentDebuff()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
			return false;

		OVT_TownData town = towns.GetNearestTown(m_Objective.position);
		if (!town || !town.supportModifiers)
			return false;

		OVT_TownModifierSystem system = towns.GetModifierSystem(OVT_TownSupportModifierSystem);
		if (!system)
			return false;

		int index = system.GetModifierIndexByName(HARASSMENT_MODIFIER);
		if (index < 0)
			return false;

		foreach (OVT_TownModifierData modifier : town.supportModifiers)
		{
			if (modifier && modifier.id == index)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The objective town's current support for the resistance.
	//! \return 0-100, or -1 when there is no town to ask (a base objective, or no town manager).
	protected int ResolveObjectiveSupportPercentage()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
			return -1;

		OVT_TownData town = towns.GetNearestTown(m_Objective.position);
		if (!town)
			return -1;

		return town.SupportPercentage();
	}

	//------------------------------------------------------------------------------------------------
	//! Phase 2. Raises the forward operating base, garrisons it, and watches its supply line.
	//!
	//! FOUR THINGS, IN THIS ORDER, AND THE ORDER IS THE CONTRACT:
	//!  1. THE CLOCKS, behind all three of the tick's early returns, exactly as in harassment.
	//!  2. THE PHASE TIMEOUT. A forward-base phase that has run its whole budget of in-game minutes
	//!     without getting a base up has failed, and failing loudly beats wedging quietly.
	//!  3. STARVATION, before any spending. A base that is already cut off should not buy another
	//!     garrison for it on the way out.
	//!  4. AT MOST ONE OPERATION, and only once the cadence has elapsed - the same discipline the
	//!     harassment phase runs on, through the same countdown, for the same reason: one spender, one
	//!     interval, no unbounded per-tick lurch.
	//!
	//! ⚠ THE COUNTER-ATTACK GATE IS ASKED FIRST, BEFORE THE CLOCKS, AND THAT ORDERING IS D17. Every
	//! other check in this method is part of "the ramp is still working"; the gate answers "the ramp is
	//! finished". Its three answers:
	//!
	//!   FIRE                - the battle starts and the phase advances, and the tick ends there.
	//!                         Serving a round off the forward-base phase's timeout on the way out
	//!                         would be bookkeeping for a phase that no longer exists.
	//!   WAIT_FOR_DAYLIGHT   - everything the occupying faction controls is done and only the world
	//!                         clock is in the way. ⚠ EXACTLY ONE THING IS HELD: THE PHASE TIMEOUT.
	//!                         See the block below - this is narrower than it first looks, and getting
	//!                         it wrong the other way is the more damaging mistake.
	//!   NOT_READY           - ordinary forward-base work; the whole method runs.
	//!
	//! 🔴 WHAT THE DAYLIGHT WAIT DOES AND DOES NOT HOLD, AND WHY THE DIFFERENCE MATTERS.
	//! D17's original wording said the wait ticks "no starvation or timeout counter". That was CORRECTED
	//! (2026-08-19, by the author of the decision) to mean only what it was for: waiting for morning must
	//! not count as the objective FAILING. The two clocks are not the same kind of thing:
	//!
	//!   - THE PHASE TIMEOUT is a clock the director runs against ITSELF. Left running, a gate met at
	//!     16:00 would spend the phase's remaining budget waiting out the night and the objective would
	//!     be ABANDONED for being dark. It is held, and it is not even tested during the wait.
	//!   - STARVATION IS THE PLAYER'S COUNTERPLAY. It is a response to facts about the WORLD - the
	//!     supplying base taken or emptied, a strong resistance presence - and those facts do not stop
	//!     being true after sunset. Freezing it would mean a player who empties the supplying garrison at
	//!     22:00 watches the forward base stand for hours and then launch a counter-attack anyway, which
	//!     contradicts F7 outright and punishes a correct play. It RUNS, and it may take the objective
	//!     down mid-wait exactly as it would at midday, blacklist and all.
	//!   - THE OPERATION CADENCE runs too, because the garrison sender is what the starvation rule is
	//!     measured against and because a frozen cadence would either never fire or fire every tick.
	//!
	//! FOUR THINGS AFTER THE GATE, IN THIS ORDER, AND THE ORDER IS THE CONTRACT:
	//!  1. THE CADENCE, behind all three of the tick's early returns, exactly as in harassment.
	//!  2. STARVATION, before any spending. A base that is already cut off should not buy another
	//!     garrison for it on the way out.
	//!  3. AT MOST ONE OPERATION, and only once the cadence has elapsed - the same discipline the
	//!     harassment phase runs on, through the same countdown, for the same reason: one spender, one
	//!     interval, no unbounded per-tick lurch.
	//!  4. THE IDLE CLOCK, LAST and only when the daylight wait is off, because it is the only thing here
	//!     that has to know whether the tick accomplished anything. See TickObjectiveIdleClock(), and see
	//!     TickHarassment() for why it moved to the bottom.
	//!
	//! ⚠ THE DAYLIGHT WAIT STILL HOLDS THE IDLE CLOCK AND STILL DOES NOT TEST IT, unchanged by the
	//! 2026-08-19 idle-clock rework. It is the same argument one layer up: a clock the director runs
	//! against itself must not be spent on a block that is not the objective's fault. The affordability
	//! hold below is that same argument applied to an empty pool.
	//!
	//! ⚠ PHASE 1 OPERATIONS DO NOT CONTINUE INTO THIS PHASE, and that is a deliberate deferral rather
	//! than an oversight. §3.2's diagram has the forward base becoming the insertion source for further
	//! harassment; every Phase 1 config authors OVT_ObjectiveConditionDeploymentModule with
	//! m_iRequiredPhase 1, so those deployments are collected the moment the ramp advances, and changing
	//! that is a Phase 5 contract with initialisation cases pinned to it. What DOES launch from the
	//! forward base is its own garrison, through OVT_ObjectiveAnchorSourceProvider - which is the seam
	//! any later phase needs to make the rest of it true.
	protected void TickFOB()
	{
		int gate = EvaluateCounterAttackGate();

		if (gate == COUNTER_ATTACK_FIRE)
		{
			FireCounterAttack();
			return;
		}

		bool waitingForDaylight = gate == COUNTER_ATTACK_WAIT_FOR_DAYLIGHT;
		if (waitingForDaylight)
			LogDaylightWait();

		m_bBlockedOnAffordability = false;

		AdvanceOperationCadence();

		// ⚠ RUNS DURING THE WAIT, AND MAY END THE OBJECTIVE DURING IT. That is the point: starvation is
		// the resistance's answer to a forward base, and a base cut off at 22:00 must come down at 22:00.
		if (TickFOBStarvation())
			return;

		bool created = false;
		if (m_Objective.nextOpTicks == 0)
			created = SendNextFOBOperation();

		// ⚠ THE ONE THING THE WAIT HOLDS, AND IT IS NOT EVEN TESTED DURING IT. Nothing would have
		// decremented it, so it cannot have reached zero on this tick - but testing it anyway would
		// abandon an objective whose patience happened to run out at the exact moment the sun went down,
		// which is the one outcome the hold exists to prevent. The per-tick affordability flag is dropped
		// with it, so a refusal seen during a wait cannot leak into the next tick's decision.
		if (waitingForDaylight)
		{
			m_bBlockedOnAffordability = false;
			return;
		}

		if (TickObjectiveIdleClock(created))
			ResetObjective("the forward-base phase did nothing at all for " + m_iPhaseTimeoutTicks.ToString() + " in-game minutes and never reached the counter-attack gate", true);
	}

	//------------------------------------------------------------------------------------------------
	// PHASE 3 - THE COUNTER-ATTACK
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether the ramp has earned its counter-attack, and if so whether the clock will allow it yet.
	//!
	//! THREE ANSWERS, NOT TWO, because "not yet" and "not now" are different states with different
	//! costs - see TickFOB() for what each one does to the machine's timers.
	//!
	//! THE MATERIAL GATE IS ASKED FIRST AND THE CLOCK SECOND, and the order matters: an objective whose
	//! support has not collapsed yet is NOT waiting for daylight, it is still being worked on, and
	//! reporting it as a daylight wait would freeze the whole forward-base phase - no starvation, no
	//! timeout, no further garrisons - for a ramp that has not finished.
	//!
	//! ⚠ IT DECIDES NOTHING AND CHANGES NOTHING. It is reachable from a public reader
	//! (IsCounterAttackReady()) and must stay side-effect free; the transition is TickFOB()'s, behind
	//! DirectorTick()'s three early returns, for the reason every other transition in this machine gives.
	//! \return COUNTER_ATTACK_NOT_READY, COUNTER_ATTACK_WAIT_FOR_DAYLIGHT or COUNTER_ATTACK_FIRE.
	int EvaluateCounterAttackGate()
	{
		if (m_Objective.phase != OVT_EObjectivePhase.FOB)
			return COUNTER_ATTACK_NOT_READY;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();

		if (!occupying || !difficulty)
			return COUNTER_ATTACK_NOT_READY;

		if (!MeetsCounterAttackRamp(occupying, difficulty))
			return COUNTER_ATTACK_NOT_READY;

		if (!IsCounterAttackDaylight())
			return COUNTER_ATTACK_WAIT_FOR_DAYLIGHT;

		return COUNTER_ATTACK_FIRE;
	}

	//------------------------------------------------------------------------------------------------
	//! The half of the counter-attack gate that is about the campaign rather than about the clock.
	//!
	//! Both kinds share the same three conjuncts in OVT_ObjectivePhaseRules - the forward base standing,
	//! the ramp's own measure of progress, and the reserve covering the battle - and differ only in what
	//! "progress" means: collapsed support for a town, completed sabotage missions for a base.
	//! \param[in] occupying The occupying faction manager, for the reserve.
	//! \param[in] difficulty The difficulty settings, for the two authored gates.
	//! \return True when everything the occupying faction controls is done.
	protected bool MeetsCounterAttackRamp(notnull OVT_OccupyingFactionManager occupying, notnull OVT_DifficultySettings difficulty)
	{
		if (m_Objective.kind == OVT_EObjectiveKind.BASE)
		{
			int required = OVT_ObjectivePhaseRules.RequiredSabotageMissions(difficulty.objectiveSabotageMissionsRequired, OVT_ObjectivePhaseRules.DEFAULT_SABOTAGE_MISSIONS);

			return OVT_ObjectivePhaseRules.BasePhase3Gate(m_Objective.sabotageSuccesses, required, m_FOB.up, occupying.m_iResources, difficulty.objectiveQRFResourceGate);
		}

		if (m_Objective.kind != OVT_EObjectiveKind.TOWN)
			return false;

		int support = ResolveObjectiveSupportPercentage();
		if (support < 0)
			return false;

		return OVT_ObjectivePhaseRules.TownPhase3Gate(support, m_FOB.up, occupying.m_iResources, difficulty.objectiveQRFResourceGate);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the world clock is inside the window a counter-attack may begin in.
	//!
	//! ⚠ AN UNREADABLE CLOCK ALLOWS THE BATTLE (returns true), and that is a decision rather than a
	//! fallthrough. A world with no time-and-weather manager has no night to protect anyone from, and
	//! failing the other way would mean the occupying faction never counter-attacked at all in such a
	//! world - which is exactly the silent, undiagnosable passivity this feature exists to end.
	//! \return True when a counter-attack may begin now.
	bool IsCounterAttackDaylight()
	{
		int hour = ResolveWorldHour();
		if (hour < 0)
			return true;

		return OVT_ObjectivePhaseRules.IsCounterAttackWindow(hour, COUNTER_ATTACK_HOUR_START, COUNTER_ATTACK_HOUR_END);
	}

	//------------------------------------------------------------------------------------------------
	//! The world clock's hour.
	//!
	//! Resolved the way OVT_OccupyingFactionManager.CheckUpdate() resolves it: the handle is the one
	//! OVT_Component already fills in, re-resolved lazily here because that fill is skipped in edit mode
	//! and a component constructed before the world's managers exist would otherwise cache a null.
	//! \return 0-23, or -1 when there is no clock to read.
	protected int ResolveWorldHour()
	{
		if (!m_Time)
		{
			ChimeraWorld world = GetOwner().GetWorld();
			if (world)
				m_Time = world.GetTimeAndWeatherManager();
		}

		if (!m_Time)
			return -1;

		TimeContainer time = m_Time.GetTime();
		if (!time)
			return -1;

		return time.m_iHours;
	}

	//------------------------------------------------------------------------------------------------
	//! Says once, per objective, that the ramp is finished and only the clock is holding it.
	//!
	//! ⚠ ONCE, NOT ONCE PER TICK (D17). This runs once per in-game minute and the wait can last most of
	//! an in-game day; unlatched it would put hundreds of identical lines in the log for a state that is
	//! entirely normal. The latch is cleared with the objective record, so the next objective says it.
	//!
	//! ⚠ IT DOES NOT SAY THE PHASE HAS STOPPED, because it has not: only the phase timeout is held. If
	//! the objective disappears during the wait, the reason will be in the log from the starvation rule
	//! or the dismantle path, and it will be a true one.
	protected void LogDaylightWait()
	{
		if (m_bDaylightWaitLogged)
			return;

		m_bDaylightWaitLogged = true;

		Print(LOG + "Objective '" + m_Objective.name + "' is ready to be counter-attacked and is waiting for daylight (" + COUNTER_ATTACK_HOUR_START.ToString() + ":00 to " + COUNTER_ATTACK_HOUR_END.ToString() + ":00). Its phase timeout is held while it waits; its forward base can still be starved out or pulled down", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Says once, per objective, that the gate passed but the battle could not be started.
	//!
	//! ⚠ ONCE, FOR THE SAME REASON THE DAYLIGHT WAIT IS ONCE. Every refusal below is a state that lasts
	//! until the objective ends - a base retaken by the occupying faction, a marker that no longer
	//! resolves, an objective position with nothing under it - and the gate is re-asked once per in-game
	//! minute, so an unlatched line would be several hundred identical warnings before the phase timeout
	//! gives up. The timeout IS the exit here; this line is the explanation for it.
	//! \param[in] reason Why no battle could be started. Named in the log.
	protected void LogCounterAttackRefusal(string reason)
	{
		if (m_bCounterAttackRefusalLogged)
			return;

		m_bCounterAttackRefusalLogged = true;

		Print(LOG + "Objective '" + m_Objective.name + "' is ready to be counter-attacked but " + reason + ". It will sit out the rest of the forward-base phase and be abandoned when that times out", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the counter-attack and moves the objective into its last phase.
	//!
	//! ⚠ THE PHASE ADVANCES ONLY IF A BATTLE ACTUALLY STARTED. Every one of the starters below refuses
	//! silently when a battle is already running, and StartBaseQRF additionally needs a live base marker
	//! to hang a controller on. Advancing regardless would put the machine in COUNTER_QRF with no battle
	//! to wait for, and TickCounterQRF() would end the objective on the very next tick - a whole ramp
	//! thrown away with nothing in the log. So the battle handle is checked afterwards, and a refusal
	//! leaves the objective in the forward-base phase to try again next minute.
	//!
	//! ⚠ IT IS THE ONLY PLACE THIS FEATURE STARTS A BATTLE. StartBaseQRF/StartTownQRF keep exactly two
	//! callers each after this feature: the player-initiated one and this (G2).
	//! \return True when a battle was started and the phase advanced.
	protected bool FireCounterAttack()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return false;

		// The freeze would have returned before this method was reached, so a battle here means one
		// started outside the tick. Refusing is the same answer the starters would give.
		if (occupying.m_CurrentQRF)
			return false;

		bool started = false;

		if (m_Objective.kind == OVT_EObjectiveKind.BASE)
			started = StartCounterAttackOnBase(occupying);
		else if (m_Objective.kind == OVT_EObjectiveKind.TOWN)
			started = StartCounterAttackOnTown(occupying);

		if (!started)
			return false;

		Print(LOG + "Objective '" + m_Objective.name + "': the counter-attack has begun", LogLevel.NORMAL);

		EnterPhase(OVT_EObjectivePhase.COUNTER_QRF);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the counter-attack against a BASE objective.
	//!
	//! ⚠ THE OBJECTIVE POSITION MUST STILL BE A RESISTANCE-HELD BASE, the same precondition sabotage
	//! makes and for the same reasons: selection copies base.location verbatim so this resolves by metres
	//! in a live campaign, but a restored payload naming a base that has since gone must not drop a
	//! battle on whichever base happened to be nearest, and a base the occupying faction has retaken by
	//! some other route must not be attacked by its own side. The forward-base phase is LOCKED against
	//! re-selection, so a base recaptured mid-ramp sits until the phase timeout - loudly, with this line
	//! in the log every interval rather than a battle.
	//! \param[in] occupying The occupying faction manager.
	//! \return True when a battle was started.
	protected bool StartCounterAttackOnBase(notnull OVT_OccupyingFactionManager occupying)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return false;

		OVT_BaseData data = occupying.GetNearestBase(m_Objective.position);
		if (!data)
			return false;

		if (vector.Distance(data.location, m_Objective.position) > BASE_OP_RESOLVE_RADIUS)
		{
			LogCounterAttackRefusal("there is no base within " + BASE_OP_RESOLVE_RADIUS.ToString() + " m of its recorded position");
			return false;
		}

		if (data.faction == config.GetOccupyingFactionIndex())
		{
			LogCounterAttackRefusal("the occupying faction already holds it");
			return false;
		}

		OVT_BaseControllerComponent controller = occupying.GetBase(data.entId);
		if (!controller)
		{
			LogCounterAttackRefusal("its base marker could not be resolved");
			return false;
		}

		// ⚠ COUNTER_ATTACK, NOT THE DEFAULT. This is the only place in the tree that asks for a silent
		// siege; a standard battle is what every player-initiated caller gets.
		occupying.StartBaseQRF(controller, OVT_EQRFMode.COUNTER_ATTACK);

		return occupying.m_CurrentQRF != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the counter-attack against a TOWN or CITY objective.
	//! \param[in] occupying The occupying faction manager.
	//! \return True when a battle was started.
	protected bool StartCounterAttackOnTown(notnull OVT_OccupyingFactionManager occupying)
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
			return false;

		OVT_TownData town = towns.GetNearestTown(m_Objective.position);
		if (!town)
		{
			LogCounterAttackRefusal("no town could be resolved at its recorded position");
			return false;
		}

		// ⚠ COUNTER_ATTACK, NOT THE DEFAULT. See StartCounterAttackOnBase.
		occupying.StartTownQRF(town, OVT_EQRFMode.COUNTER_ATTACK);

		return occupying.m_CurrentQRF != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Sends ONE forward-base operation, and re-arms the cadence only if one was actually sent.
	//!
	//! THE BASE BEFORE ITS GARRISON, because there is nothing to garrison until the flag is up and the
	//! garrison's own source provider resolves to the forward base only once it is standing. Both
	//! refuse on their first line when it is not their turn, so the order they are asked in is the only
	//! sequencing this phase needs.
	//!
	//! ⚠ THE COUNTDOWN IS ONLY RE-ARMED ON A SUCCESSFUL CREATE, exactly as in harassment. Every refusal
	//! - no site, the pool is short, the ceiling is spent, the garrison is full - leaves nextOpTicks at
	//! zero so the next tick asks again a minute later instead of waiting out a whole interval for a
	//! condition that may have cleared immediately.
	//! \return True when an operation was created and paid for - which is PROGRESS, and re-arms the idle
	//!         clock.
	protected bool SendNextFOBOperation()
	{
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (!difficulty)
			return false;

		if (!SendFOBOperation() && !SendFOBGarrisonOperation())
			return false;

		SetOperationCountdown(difficulty.objectiveHarassmentIntervalMinutes);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Sends the supply truck that raises the forward operating base, once per objective.
	//!
	//! ⚠ IT ARMS THE CEILING BEFORE THE CREATE AND DISARMS IT AGAIN ON FAILURE. m_bFOBDeploymentSent is
	//! what makes IsFOBBudgetActive() true, and the ceiling has to already be active when the forward
	//! base's OWN cost is checked and counted - §3.7 is explicit that the budget covers "the structure
	//! itself". A create that is refused must leave the flag down, or the phase would believe a base was
	//! on its way and never send another.
	//! \return True when a deployment was created and paid for.
	protected bool SendFOBOperation()
	{
		if (m_FOB.up)
			return false;

		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!deployments || !config)
			return false;

		int occupyingIndex = config.GetOccupyingFactionIndex();

		OVT_DeploymentComponent existing = FindLiveFOBDeployment(deployments);

		if (m_bFOBDeploymentSent)
		{
			// A supply party is still on the road. Nothing more to send until it arrives.
			if (existing)
				return false;

			// ⚠ IT WAS SENT AND IT IS GONE, AND THIS IS THE PHASE'S ONE WEDGE RISK CLOSED. A deployment
			// can be collected by its own condition module, wiped out, or deleted by a Game Master
			// between the send and the raise; with the flag latched and nothing watching for that, the
			// phase would sit and do nothing until its timeout with no line in the log to explain it.
			// The flag is dropped and the next interval sites again - the resources already spent stay
			// counted against the ceiling, because they really were spent.
			Print(LOG + "The supply party sent to raise a forward base for objective '" + m_Objective.name + "' is gone before it could build - siting another", LogLevel.WARNING);
			m_bFOBDeploymentSent = false;
			m_vFOBSite = vector.Zero;

			return false;
		}

		// A marker left over from a save taken mid-drive, or from an objective whose teardown could not
		// reach it. It can never raise anything (D11 gates a restored deployment's raise), so leaving it
		// standing would cost this objective its whole phase.
		if (existing)
		{
			Print(LOG + "A forward-base deployment from a previous session is standing near objective '" + m_Objective.name + "' and can never raise anything - collecting it and re-siting", LogLevel.WARNING);
			deployments.DeleteDeployment(existing);
			return false;
		}

		vector source;
		if (!ResolveFOBSourceBase(occupyingIndex, source))
		{
			Print(LOG + "Objective '" + m_Objective.name + "' cannot raise a forward base: the occupying faction holds no base to supply one from", LogLevel.WARNING);
			return false;
		}

		vector site;
		if (!ResolveFOBSite(source, m_Objective.position, site))
		{
			// ⚠ NOT A RETRY. The band, the exclusions and the terrain do not change from one in-game
			// minute to the next, so an objective with nowhere to put a forward base has nowhere to put
			// one for as long as it is the objective. It sits out a selection round and something else
			// gets picked - T7.4.
			Print(LOG + "Objective '" + m_Objective.name + "' has nowhere to put a forward base: " + FOB_SITING_ATTEMPTS.ToString() + " generated candidate(s) and every authored site in the band were rejected. Abandoning it for one selection round", LogLevel.WARNING);
			ResetObjective("no forward-base site could be found anywhere in its band", true);
			return false;
		}

		m_bFOBDeploymentSent = true;

		if (!CreateObjectiveDeployment(deployments, FOB_CONFIG, site, occupyingIndex))
		{
			m_bFOBDeploymentSent = false;
			return false;
		}

		m_vFOBSite = site;
		m_FOB.sourceBasePosition = source;

		Print(LOG + "Objective '" + m_Objective.name + "': a supply party is on its way to raise a forward base at " + site.ToString(), LogLevel.NORMAL);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Sends one more garrison group to a forward base that is standing and short of men.
	//!
	//! ⚠ THE CAP IS COUNTED FROM THE LIVE DEPLOYMENT LIST, NEVER FROM m_aCreatedDeployments, for the
	//! reason every other count in this component gives: the tracked list is a teardown ledger that only
	//! grows until a reset, so a garrison that was wiped out or collected would hold a slot forever and
	//! a forward base that lost its men could never be reinforced.
	//! \return True when a deployment was created and paid for.
	protected bool SendFOBGarrisonOperation()
	{
		if (!m_FOB.up)
			return false;

		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();

		if (!deployments || !config || !difficulty)
			return false;

		int occupyingIndex = config.GetOccupyingFactionIndex();

		if (CountLiveFOBGarrisons(deployments, occupyingIndex) >= difficulty.objectiveFOBGarrisonMax)
			return false;

		return CreateObjectiveDeployment(deployments, FOB_GARRISON_CONFIG, m_FOB.position, occupyingIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! How many garrison deployments are alive at the forward base right now.
	//! \param[in] deployments The deployment framework.
	//! \param[in] factionIndex The occupying faction.
	//! \return The count.
	protected int CountLiveFOBGarrisons(notnull OVT_DeploymentManagerComponent deployments, int factionIndex)
	{
		array<OVT_DeploymentComponent> nearby = deployments.GetDeploymentsInRadius(m_FOB.position, FOB_AREA_RADIUS);
		if (!nearby)
			return 0;

		int count = 0;
		foreach (OVT_DeploymentComponent deployment : nearby)
		{
			if (!deployment)
				continue;

			if (deployment.GetControllingFaction() != factionIndex)
				continue;

			if (deployment.GetDeploymentName() != FOB_GARRISON_CONFIG)
				continue;

			count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! The forward base's own deployment, wherever this objective might have put one.
	//!
	//! SEARCHED FROM THE OBJECTIVE RATHER THAN FROM A REMEMBERED SITE, because the one caller that
	//! needs it most - a restored campaign looking for a marker it has no runtime record of - has no
	//! remembered site to search from. FOB_MAX_STANDOFF is the furthest the sampler could ever have put
	//! one, so this covers the whole band and nothing beyond it.
	//! \param[in] deployments The deployment framework.
	//! \return The deployment, or null when there is none.
	protected OVT_DeploymentComponent FindLiveFOBDeployment(notnull OVT_DeploymentManagerComponent deployments)
	{
		return deployments.GetDeploymentNearPosition(FOB_CONFIG, m_Objective.position, FOB_MAX_STANDOFF);
	}

	//------------------------------------------------------------------------------------------------
	//! The base the forward operating base will be supplied from: the nearest one the occupying faction
	//! holds to the objective.
	//!
	//! ⚠ THE SAME QUESTION THE INSERTION MODULE'S DEFAULT PROVIDER ASKS, AND DELIBERATELY THE SAME
	//! ANSWER. The director needs it here to know which way the supply line runs before any deployment
	//! exists to ask a provider on its behalf; the truck that actually drives resolves its own origin
	//! through the provider, and both walk the faction's own base list so a contested map degrades to a
	//! longer drive rather than to no forward base.
	//! \param[in] factionIndex The occupying faction.
	//! \param[out] source The supplying base's position.
	//! \return False when the faction holds no base at all.
	protected bool ResolveFOBSourceBase(int factionIndex, out vector source)
	{
		source = vector.Zero;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return false;

		array<OVT_BaseData> controlled = occupying.GetBasesControlledBy(factionIndex);
		if (!controlled || controlled.IsEmpty())
			return false;

		bool found = false;
		float best = 0;

		foreach (OVT_BaseData base : controlled)
		{
			if (!base)
				continue;

			float distance = vector.Distance(base.location, m_Objective.position);

			if (found && distance >= best)
				continue;

			found = true;
			best = distance;
			source = base.location;
		}

		if (!found)
			source = vector.Zero;

		return found;
	}

	//------------------------------------------------------------------------------------------------
	// WHERE THE FORWARD BASE GOES
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Picks the site for the forward operating base, or answers that there is nowhere to put one.
	//!
	//! TWO SOURCES OF CANDIDATES, AND THE AUTHORED ONE WINS WHENEVER IT PRODUCES ANYTHING:
	//!  1. GENERATED - a bounded, deterministic lattice of FOB_SITING_ATTEMPTS points on the supply
	//!     line and either side of it, each ocean-rejected, clearance-traced, flatness-probed and
	//!     scored. THIS IS THE PRIMARY PATH and it is built and tuned as if the other will never exist,
	//!     because today it does not: no OVT_FOBPosition marker is placed in any shipped world (R17).
	//!  2. AUTHORED - OVT_FOBPositionComponent markers inside the band, subject to the same exclusions
	//!     and the same clearance test. A map author who has walked the terrain knows better than a
	//!     lattice, so ANY qualifying marker beats the best generated point.
	//!
	//! ⚠ IT IS DETERMINISTIC, WITH NO RANDOMNESS AT ALL, and that is the same design constraint
	//! objective selection carries. The whole feature exists so the resistance can READ the occupying
	//! faction's intent; a forward base that lands somewhere different every time the same campaign
	//! reaches the same state is the unpredictability being retired. It also means a bad placement is a
	//! reproducible tuning question rather than a roll.
	//! \param[in] source Where the supply line starts - the nearest base the faction holds.
	//! \param[in] objective Where it is going.
	//! \param[out] site The chosen position, written only when this returns true.
	//! \return False when nothing in the band qualifies.
	protected bool ResolveFOBSite(vector source, vector objective, out vector site)
	{
		site = vector.Zero;

		array<vector> exclusions = new array<vector>();
		array<float> radii = new array<float>();
		CollectFOBExclusions(exclusions, radii);

		float bestScore = 0;
		bool found = SampleGeneratedFOBSite(source, objective, exclusions, radii, site, bestScore);

		vector authored;
		float authoredScore = 0;
		if (FindAuthoredFOBSite(source, objective, exclusions, radii, authored, authoredScore))
		{
			site = authored;

			Print(LOG + "Forward base for objective '" + m_Objective.name + "' will use an authored site at " + authored.ToString(), LogLevel.NORMAL);

			return true;
		}

		if (found)
			Print(LOG + "Forward base for objective '" + m_Objective.name + "' sited at " + site.ToString() + " (generated, score " + bestScore.ToString() + ")", LogLevel.NORMAL);

		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Every place a forward base may not be built, with how far away it has to stay.
	//!
	//! THREE KINDS, AND EACH IS A DIFFERENT WAY OF BEING SOMEBODY ELSE'S GROUND:
	//!  - EVERY BASE, WHOEVER HOLDS IT. Not just the resistance's: a forward base beside one the
	//!    occupying faction already owns is a second flag in the same field, which is not what the
	//!    middle phase is for.
	//!  - THE RESISTANCE'S FORWARD BASES AND CAMPS. These are player-built and player-owned, and an
	//!    enemy base materialising inside one is the single worst thing this feature could do.
	//!  - RESISTANCE-HELD TOWNS AND VILLAGES, at their own range plus a margin. Villages are included
	//!    here even though they are excluded from being objectives: they fall as collateral, but they
	//!    are still somewhere people live.
	//!
	//! ⚠ THE TWO LISTS ARE PARALLEL AND MUST STAY THE SAME LENGTH. OVT_FOBSiting.IsClearOfExclusions()
	//! refuses a ragged pair outright rather than guessing, so every Insert here is made in a pair.
	//! \param[out] exclusions Positions to stay away from.
	//! \param[out] radii How far, in the same order.
	protected void CollectFOBExclusions(notnull array<vector> exclusions, notnull array<float> radii)
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (occupying && occupying.m_Bases)
		{
			foreach (OVT_BaseData base : occupying.m_Bases)
			{
				if (!base)
					continue;

				exclusions.Insert(base.location);
				radii.Insert(FOB_CLEARANCE_BASE);
			}
		}

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (resistance)
		{
			if (resistance.m_FOBs)
			{
				foreach (OVT_FOBData fob : resistance.m_FOBs)
				{
					if (!fob)
						continue;

					exclusions.Insert(fob.location);
					radii.Insert(FOB_CLEARANCE_RESISTANCE_SITE);
				}
			}

			if (resistance.m_Camps)
			{
				foreach (OVT_CampData camp : resistance.m_Camps)
				{
					if (!camp)
						continue;

					exclusions.Insert(camp.location);
					radii.Insert(FOB_CLEARANCE_RESISTANCE_SITE);
				}
			}
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (towns && towns.m_Towns && config)
		{
			int occupyingIndex = config.GetOccupyingFactionIndex();

			foreach (OVT_TownData town : towns.m_Towns)
			{
				if (!town)
					continue;

				// A town the occupying faction already holds is not excluded: putting a forward base
				// beside one it owns is fine, and on a map where the resistance holds almost everything
				// excluding them all would leave nowhere at all.
				if (town.faction == occupyingIndex)
					continue;

				exclusions.Insert(town.location);
				radii.Insert(towns.GetTownRange(town) + FOB_CLEARANCE_TOWN_MARGIN);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Walks the deterministic lattice and keeps the best-scoring point that passes every hard test.
	//!
	//! ⚠ IT DOES NOT RETURN ON THE FIRST HIT. An early return would make the sampler prefer whatever
	//! happens to be nearest the supply base regardless of how bad it is, and the score exists precisely
	//! to order the survivors. The bound is what keeps the cost fixed - see FOB_SITING_ATTEMPTS.
	//! \param[in] source Where the supply line starts.
	//! \param[in] objective Where it is going.
	//! \param[in] exclusions Places to stay clear of.
	//! \param[in] radii How far from each, same order.
	//! \param[out] best The best candidate found.
	//! \param[out] bestScore Its score.
	//! \return False when no candidate passed.
	protected bool SampleGeneratedFOBSite(vector source, vector objective, notnull array<vector> exclusions, notnull array<float> radii, out vector best, out float bestScore)
	{
		best = vector.Zero;
		bestScore = 0;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		float separation = vector.Distance(source, objective);
		if (separation <= 0)
			return false;

		float bandMin = separation * FOB_BAND_MIN_FRACTION;
		float bandMax = separation * FOB_BAND_MAX_FRACTION;

		if (bandMin < FOB_MIN_STANDOFF)
			bandMin = FOB_MIN_STANDOFF;

		if (bandMax > FOB_MAX_STANDOFF)
			bandMax = FOB_MAX_STANDOFF;

		// A degenerate band - the supplying base is almost on top of the objective, or the floor has
		// swallowed the whole span - refuses everything, by OVT_FOBSiting.IsInBand's own rule.
		vector toObjective = objective - source;
		toObjective[1] = 0;

		if (toObjective.Length() <= 0)
			return false;

		vector along = toObjective.Normalized();
		vector lateral = Vector(-along[2], 0, along[0]);

		bool found = false;

		for (int attempt = 0; attempt < FOB_SITING_ATTEMPTS; attempt++)
		{
			int step = attempt / FOB_SITING_LANES;
			int lane = attempt % FOB_SITING_LANES;

			// Measured back from the OBJECTIVE, so the fraction is "how far out from the target", which
			// is what the band is expressed in.
			float standoff = bandMax - (bandMax - bandMin) * OVT_FOBSiting.BandFraction(step, FOB_SITING_STEPS);

			vector candidate = objective - along * standoff;
			candidate = candidate + lateral * (OVT_FOBSiting.LateralOffset(lane, FOB_SITING_LANES) * FOB_LATERAL_SPREAD);

			vector accepted;
			float score;
			if (!EvaluateFOBCandidate(world, candidate, objective, bandMin, bandMax, exclusions, radii, accepted, score))
				continue;

			if (found && score <= bestScore)
				continue;

			found = true;
			bestScore = score;
			best = accepted;
		}

		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Every curated marker in the band that still qualifies, best first.
	//!
	//! ⚠ A MARKER IS NOT EXEMPT FROM THE HARD TESTS. It still has to be in the band, out of the ocean,
	//! clear of every exclusion and clear of obstructions - the world moves under a marker placed months
	//! ago in the editor, and the resistance can build a camp on top of one. What a marker buys is
	//! PRIORITY over anything generated, not a bypass.
	//! \param[in] source Where the supply line starts.
	//! \param[in] objective Where it is going.
	//! \param[in] exclusions Places to stay clear of.
	//! \param[in] radii How far from each, same order.
	//! \param[out] best The best marker found.
	//! \param[out] bestScore Its score.
	//! \return False when no marker qualifies.
	protected bool FindAuthoredFOBSite(vector source, vector objective, notnull array<vector> exclusions, notnull array<float> radii, out vector best, out float bestScore)
	{
		best = vector.Zero;
		bestScore = 0;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		float separation = vector.Distance(source, objective);
		if (separation <= 0)
			return false;

		float bandMin = separation * FOB_BAND_MIN_FRACTION;
		float bandMax = separation * FOB_BAND_MAX_FRACTION;

		if (bandMin < FOB_MIN_STANDOFF)
			bandMin = FOB_MIN_STANDOFF;

		if (bandMax > FOB_MAX_STANDOFF)
			bandMax = FOB_MAX_STANDOFF;

		m_aFoundFOBMarkers = new array<IEntity>();
		world.QueryEntitiesBySphere(objective, bandMax, AddFOBMarker, FilterFOBMarker, EQueryEntitiesFlags.ALL);

		bool found = false;

		foreach (IEntity marker : m_aFoundFOBMarkers)
		{
			if (!marker)
				continue;

			OVT_FOBPositionComponent authored = OVT_FOBPositionComponent.Cast(marker.FindComponent(OVT_FOBPositionComponent));
			if (!authored || !authored.m_bEnabled)
				continue;

			vector accepted;
			float score;
			if (!EvaluateFOBCandidate(world, marker.GetOrigin(), objective, bandMin, bandMax, exclusions, radii, accepted, score))
				continue;

			if (found && score <= bestScore)
				continue;

			found = true;
			bestScore = score;
			best = accepted;
		}

		m_aFoundFOBMarkers = null;

		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Judges one candidate against every hard test, and scores it if it survives.
	//!
	//! THE HARD TESTS, IN THE ORDER OF WHAT THEY COST: the band (arithmetic), the ocean (one surface
	//! read), the exclusions (a list walk), the flatness probes (five surface reads), and the clearance
	//! trace (one TraceBox, the most expensive thing here). Reordering them makes the sampler slower and
	//! nothing else.
	//! \param[in] world The world to read the terrain from.
	//! \param[in] candidate The position being judged.
	//! \param[in] objective Where the objective is.
	//! \param[in] bandMin Nearest the base may stand to it.
	//! \param[in] bandMax Furthest it may stand from it.
	//! \param[in] exclusions Places to stay clear of.
	//! \param[in] radii How far from each, same order.
	//! \param[out] accepted The candidate with its height clamped to the ground.
	//! \param[out] score How good it is.
	//! \return False when it failed any hard test.
	protected bool EvaluateFOBCandidate(notnull BaseWorld world, vector candidate, vector objective, float bandMin, float bandMax, notnull array<vector> exclusions, notnull array<float> radii, out vector accepted, out float score)
	{
		accepted = candidate;
		score = 0;

		if (!OVT_FOBSiting.IsInBand(vector.Distance(candidate, objective), bandMin, bandMax))
			return false;

		if (OVT_WorldUtils.IsOceanAtPosition(candidate))
			return false;

		if (!OVT_FOBSiting.IsClearOfExclusions(candidate, exclusions, radii))
			return false;

		// The lattice interpolates in the horizontal plane and knows nothing about the ground, so every
		// candidate is clamped to the surface before anything measures a height against it.
		accepted = candidate;
		accepted[1] = world.GetSurfaceY(candidate[0], candidate[2]);

		float spread = MeasureGroundSpread(world, accepted);
		if (spread >= FOB_FLATNESS_TOLERANCE)
			return false;

		if (!IsSiteClearOfObstructions(accepted))
			return false;

		float flatness = OVT_FOBSiting.FlatnessScore(spread, FOB_FLATNESS_TOLERANCE);
		float elevation = OVT_FOBSiting.ElevationScore(accepted[1] - world.GetSurfaceY(objective[0], objective[2]), FOB_ELEVATION_USEFUL_GAIN);

		score = OVT_FOBSiting.ScoreSite(flatness, elevation, MeasureRoadDistance(accepted));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! How much the ground rises and falls across a candidate's footprint.
	//! \param[in] world The world to read the terrain from.
	//! \param[in] centre The candidate, already clamped to the surface.
	//! \return Metres between the highest and lowest of five probes.
	protected float MeasureGroundSpread(notnull BaseWorld world, vector centre)
	{
		float lowest = centre[1];
		float highest = centre[1];

		for (int i = 0; i < 4; i++)
		{
			float offsetX = 0;
			float offsetZ = 0;

			if (i == 0)
				offsetX = FOB_FLATNESS_PROBE_RADIUS;
			else if (i == 1)
				offsetX = -FOB_FLATNESS_PROBE_RADIUS;
			else if (i == 2)
				offsetZ = FOB_FLATNESS_PROBE_RADIUS;
			else
				offsetZ = -FOB_FLATNESS_PROBE_RADIUS;

			float height = world.GetSurfaceY(centre[0] + offsetX, centre[2] + offsetZ);

			if (height < lowest)
				lowest = height;

			if (height > highest)
				highest = height;
		}

		return highest - lowest;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether there is room to put a structure down.
	//!
	//! ⚠ THE TEST IS `result > 0 && !trace.TraceEnt`, AND BOTH HALVES ARE LOAD-BEARING. TracePosition
	//! returns a NEGATIVE value when the box starts inside something and sets TraceEnt to whatever it
	//! hit; the landing-zone code this is copied from used to accept `>= 0` and therefore accepted every
	//! candidate on the map, which was BUG-031. Getting this wrong here means forward bases inside
	//! buildings and inside each other.
	//! \param[in] position The candidate, already clamped to the surface.
	//! \return True when the clearance box is empty.
	protected bool IsSiteClearOfObstructions(vector position)
	{
		autoptr TraceBox trace = new TraceBox();
		trace.Flags = TraceFlags.ENTS;
		trace.Start = position;
		trace.Mins = Vector(-FOB_CLEAR_BOX_HALF, 0, -FOB_CLEAR_BOX_HALF);
		trace.Maxs = Vector(FOB_CLEAR_BOX_HALF, FOB_CLEAR_BOX_HEIGHT, FOB_CLEAR_BOX_HALF);
		trace.Exclude = GetOwner();

		float result = GetOwner().GetWorld().TracePosition(trace, null);

		return result > 0 && !trace.TraceEnt;
	}

	//------------------------------------------------------------------------------------------------
	//! How far a candidate is from the nearest road a vehicle could be put on.
	//! \param[in] position The candidate.
	//! \return Metres, or -1 when there is no road within the search bound - which
	//!         OVT_FOBSiting.RoadScore() reads as "no road", not as "on one".
	protected float MeasureRoadDistance(vector position)
	{
		vector roadPosition;
		vector roadAngles;
		if (!OVT_WorldUtils.FindNearestRoadSpawn(position, OVT_WorldUtils.ROAD_SPAWN_MAX_DISTANCE, roadPosition, roadAngles))
			return -1;

		return vector.Distance(position, roadPosition);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entity Candidate from the marker query.
	//! \return True when it is a curated forward-base marker.
	protected bool FilterFOBMarker(IEntity entity)
	{
		if (!entity)
			return false;

		return entity.FindComponent(OVT_FOBPositionComponent) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entity A marker that passed the filter.
	//! \return True, to keep the query running.
	protected bool AddFOBMarker(IEntity entity)
	{
		m_aFoundFOBMarkers.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// THE FORWARD BASE ONCE IT IS STANDING
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The forward operating base is up. Called by OVT_FOBRaiseSpawningDeploymentModule, once.
	//!
	//! ⚠ IT RECORDS. IT DOES NOT DECIDE - the same rule OnHarassmentSuccess() and OnSabotageSuccess()
	//! carry, and for the same reason: this is public, it is called from a deployment's own update, and
	//! a phase entry re-arms the phase timeout. Every transition in this machine happens on
	//! DirectorTick(), behind its three early returns.
	//!
	//! ⚠ NO NOTIFICATION IS SENT (D12). The forward base is the one thing in the ramp the resistance is
	//! meant to discover rather than be told about. This is an explicit requirement, not an oversight.
	//! \param[in] position Where the structure stands.
	//! \param[in] sourceBasePosition Where its supply line starts. Zero keeps whatever was recorded when
	//!            the deployment was sent, which is the better answer for a walking insertion that never
	//!            resolved a source of its own.
	//! \param[in] deploymentName The config carrying it - the re-link key written into the save.
	void OnFOBRaised(vector position, vector sourceBasePosition, string deploymentName)
	{
		if (m_Objective.kind == OVT_EObjectiveKind.NONE)
			return;

		vector source = sourceBasePosition;
		if (source == vector.Zero)
			source = m_FOB.sourceBasePosition;

		int spent = m_FOB.spent;

		RecordFOB(position, source, deploymentName);

		// RecordFOB() writes a fresh record; what has already been spent getting the base up is part of
		// its budget and must survive that (§3.7 - the ceiling covers "the structure itself").
		m_FOB.spent = spent;

		m_vFOBSite = position;
		m_bFOBDeploymentSent = true;
		m_bStarvationLogged = false;

		Print(LOG + "Objective '" + m_Objective.name + "': the forward operating base is standing at " + position.ToString(), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! One tick of the starvation rule.
	//!
	//! THREE INPUTS, ANY ONE OF WHICH CUTS THE BASE OFF, and the arithmetic is
	//! OVT_ObjectivePhaseRules.IsFOBStarved(): the base supplying it has been taken, its own garrison is
	//! dead, or the resistance is standing on it. RECOVERY ZEROES THE COUNT rather than pausing it - a
	//! forward base that was cut off for ten minutes and then relieved is not half-dead, it is supplied.
	//!
	//! ⚠ BOTH TRANSITIONS ARE LOGGED, ONCE EACH. "The occupying faction's forward base disappeared" has
	//! to have an explanation in the log, and so does "it survived": a latch rather than a line per
	//! in-game minute, because this runs for as long as the phase does.
	//! \return True when the objective was torn down, so the caller stops working on this phase.
	protected bool TickFOBStarvation()
	{
		if (!m_FOB.up)
			return false;

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (!difficulty)
			return false;

		bool starved = OVT_ObjectivePhaseRules.IsFOBStarved(IsFOBSourceBaseHeld(), CountAliveFOBGroups(), IsPlayerAtFOB());

		if (!starved)
		{
			if (m_bStarvationLogged)
			{
				m_bStarvationLogged = false;
				Print(LOG + "The forward base for objective '" + m_Objective.name + "' is supplied again after " + m_FOB.starvationTicks.ToString() + " in-game minute(s) cut off", LogLevel.NORMAL);
			}

			SetFOBStarvationTicks(0);

			return false;
		}

		SetFOBStarvationTicks(m_FOB.starvationTicks + 1);

		if (!m_bStarvationLogged)
		{
			m_bStarvationLogged = true;
			Print(LOG + "The forward base for objective '" + m_Objective.name + "' is cut off - it has " + difficulty.objectiveStarvationMinutes.ToString() + " in-game minute(s) before it is abandoned", LogLevel.NORMAL);
		}

		if (difficulty.objectiveStarvationMinutes <= 0)
			return false;

		if (m_FOB.starvationTicks < difficulty.objectiveStarvationMinutes)
			return false;

		// ⚠ NO PENALTY. Starvation is the occupying faction giving up on a supply line it could not
		// hold, not the resistance taking the base off it - only the player-initiated exit is paid for.
		ResetObjective("its forward base was cut off for " + m_FOB.starvationTicks.ToString() + " in-game minutes and has been abandoned", true);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the base supplying the forward operating base is still the occupying faction's.
	//! \return True when it is.
	protected bool IsFOBSourceBaseHeld()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!occupying || !config)
			return false;

		// A forward base with no recorded source is one raised by an insertion that walked in without
		// resolving an origin. It is not starving for want of a base it never had.
		if (m_FOB.sourceBasePosition == vector.Zero)
			return true;

		OVT_BaseData base = occupying.GetNearestBase(m_FOB.sourceBasePosition);
		if (!base)
			return false;

		if (vector.Distance(base.location, m_FOB.sourceBasePosition) > BASE_OP_RESOLVE_RADIUS)
			return false;

		return base.faction == config.GetOccupyingFactionIndex();
	}

	//------------------------------------------------------------------------------------------------
	//! How many of the occupying faction's registered groups are alive at the forward base.
	//!
	//! ⚠ COUNTED THROUGH HANDLES AND THE SURVIVOR MASK, NEVER THROUGH SPAWNED ENTITIES. A dormant group
	//! reports zero agents while being perfectly alive, so a count of bodies would starve every forward
	//! base on the map the moment the last player drove away - which is exactly when one is supposed to
	//! be quietly doing its job.
	//! \return The number of alive registered groups.
	protected int CountAliveFOBGroups()
	{
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!deployments || !virtualization || !config)
			return 0;

		array<OVT_DeploymentComponent> nearby = deployments.GetDeploymentsInRadius(m_FOB.position, FOB_AREA_RADIUS);
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
	//! Whether a player is standing on the forward base.
	//!
	//! ⚠ PLAYERS ONLY, and the difficulty's own baseCloseRange, which is the distance the rest of the
	//! campaign already means by "at a base". Asking it of every AI agent would make a resistance patrol
	//! walking past count as an interdiction.
	//! \return True when somebody is there.
	protected bool IsPlayerAtFOB()
	{
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (!difficulty)
			return false;

		return OVT_WorldUtils.PlayerInRange(m_FOB.position, difficulty.baseCloseRange);
	}

	//------------------------------------------------------------------------------------------------
	// THE ONE TEARDOWN, AND THE ONE PENALTY
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Takes the forward operating base down: its deployments, its structure, and its budget.
	//!
	//! ⚠ ONE PATH, THREE EXITS, AND ONLY ONE OF THEM IS PAID FOR:
	//!   STARVATION           - the supply line failed. No penalty: nobody took it off them.
	//!   PLAYER DISMANTLE     - the resistance cleared the site and pulled the flag down. THE PENALTY
	//!                          APPLIES, and it is applied by the caller (OnFOBDismantledByPlayer)
	//!                          rather than here, because "what happened" is the caller's knowledge and
	//!                          this method's job is only to make the base stop existing.
	//!   COUNTER-QRF RESOLVED - the battle is over whatever the outcome. No penalty.
	//! Everything else that ends an objective - the phase timeout, a re-selection, a restore that could
	//! not re-link - funnels through ResetObjective() as well, so they are covered by construction.
	//!
	//! ⚠ THE INSERTION RESERVATION IS RELEASED BY DELETING THE DEPLOYMENTS, not by anything here.
	//! DestroyDeployment() runs every module's Cleanup(), and the insertion module's releases the convoy
	//! slot, deletes its waypoints and disposes of its truck. A slot lost to a convoy that ended quietly
	//! is a slot the faction never gets back, which is why the deployments are deleted rather than
	//! merely forgotten.
	//!
	//! IDEMPOTENT AND SAFE WITH NO FORWARD BASE. It is reached from every reset, including the ones for
	//! objectives that never got past harassment.
	protected void TearDownFOB()
	{
		vector site = m_FOB.position;
		if (site == vector.Zero)
			site = m_vFOBSite;

		// NOTHING WAS EVER SENT. This method is reached from every reset, including the many for
		// objectives that never got out of harassment, so the common case gets no queries at all.
		if (!m_FOB.up && !m_bFOBDeploymentSent && site == vector.Zero)
		{
			ClearFOBRuntimeState();
			return;
		}

		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (deployments)
		{
			// The tracked ledger has already taken down what this session created; these two sweeps are
			// for what it could not know about - a marker restored from a save, or one whose position
			// moved between the send and the raise.
			if (m_Objective.kind != OVT_EObjectiveKind.NONE)
			{
				OVT_DeploymentComponent carrier = FindLiveFOBDeployment(deployments);
				if (carrier)
					deployments.DeleteDeployment(carrier);
			}

			if (site != vector.Zero)
			{
				array<OVT_DeploymentComponent> nearby = deployments.GetDeploymentsInRadius(site, FOB_AREA_RADIUS);
				if (nearby)
				{
					foreach (OVT_DeploymentComponent deployment : nearby)
					{
						if (!deployment)
							continue;

						string name = deployment.GetDeploymentName();
						if (name != FOB_CONFIG && name != FOB_GARRISON_CONFIG)
							continue;

						deployments.DeleteDeployment(deployment);
					}
				}
			}
		}

		if (site != vector.Zero)
			RemoveFOBStructure(site);

		// The bias is dropped again here so this method is complete on its own. ClearObjectiveRecord()
		// drops it too, on every path that ends an objective - both are idempotent, and a teardown that
		// silently depended on its caller to finish the job is how the anchor was left pointing at an
		// abandoned objective in the first place (see ResetObjective).
		DropObjectiveAnchor();

		ClearFOBRuntimeState();
	}

	//------------------------------------------------------------------------------------------------
	//! Puts back the forward-base state that is NOT in the save payload.
	//!
	//! Separate from ClearFOBRecord() so the teardown can finish its own job without depending on a
	//! caller to clear the record afterwards - the two are called in sequence by ResetObjective(), and
	//! each is idempotent on its own.
	protected void ClearFOBRuntimeState()
	{
		m_bFOBDeploymentSent = false;
		m_vFOBSite = vector.Zero;
		m_bStarvationLogged = false;
		m_bCeilingLogged = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes the forward base's structure, wherever the campaign left it.
	//!
	//! ⚠ THROUGH OVT_ResistanceFactionManager.DestroyPlacedItem() AND NOWHERE ELSE. That method is the
	//! navmesh-queue-then-delete pair: OVT_NavmeshRebuild.Queue() measures the entity's bounds at CALL
	//! time and rebuilds a second later, so the capture happens while the structure still stands and the
	//! rebuild happens once it is gone. A raw delete leaves the carve in the navmesh forever and the AI
	//! refuses to cross ground that is now empty, with no symptom anybody would trace back to a flagpole.
	//!
	//! ⚠ FOUND BY QUERY, NOT BY A REMEMBERED HANDLE. The raise module holds an EntityID, but that link is
	//! runtime-only - a campaign that has been loaded since has a structure standing with nothing
	//! pointing at it. The join is by PREFAB RESOURCE NAME, which is the same join Phase 6 established
	//! for structure costs and for the same reason: it survives a restore, because persistence respawns
	//! from the same prefab.
	//! \param[in] site Where the structure stands.
	protected void RemoveFOBStructure(vector site)
	{
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance)
			return;

		ResourceName prefab = ResolveFOBStructurePrefab();
		if (prefab.IsEmpty())
			return;

		m_sFOBStructurePrefab = prefab;
		m_aFoundFOBStructures = new array<IEntity>();

		BaseWorld world = GetGame().GetWorld();
		if (world)
			world.QueryEntitiesBySphere(site, FOB_STRUCTURE_SEARCH_RADIUS, AddFOBStructure, FilterFOBStructure, EQueryEntitiesFlags.ALL);

		foreach (IEntity structure : m_aFoundFOBStructures)
		{
			if (!structure)
				continue;

			resistance.DestroyPlacedItem(structure);
		}

		m_aFoundFOBStructures = null;
		m_sFOBStructurePrefab = "";
	}

	//------------------------------------------------------------------------------------------------
	//! The structure prefab the forward-base config is authored to raise.
	//!
	//! READ OFF THE CONFIG rather than duplicated as a constant here, so a modded config that raises a
	//! different structure is still torn down correctly and there is exactly one authored answer.
	//! \return The prefab, or an empty ResourceName when the config does not resolve.
	protected ResourceName ResolveFOBStructurePrefab()
	{
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (!deployments)
			return "";

		OVT_DeploymentConfig config = deployments.FindConfigByName(FOB_CONFIG);
		if (!config || !config.m_aModules)
			return "";

		foreach (OVT_BaseDeploymentModule module : config.m_aModules)
		{
			OVT_FOBRaiseSpawningDeploymentModule raise = OVT_FOBRaiseSpawningDeploymentModule.Cast(module);
			if (raise)
				return raise.GetFOBPrefab();
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entity Candidate from the structure query.
	//! \return True when it was spawned from the forward-base prefab.
	protected bool FilterFOBStructure(IEntity entity)
	{
		if (!entity)
			return false;

		return OVT_PrefabUtils.GetPrefabName(entity) == m_sFOBStructurePrefab;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entity A structure that passed the filter.
	//! \return True, to keep the query running.
	protected bool AddFOBStructure(IEntity entity)
	{
		m_aFoundFOBStructures.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// THE PLAYER-INITIATED EXIT
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether the forward base can be dismantled from a given position right now. THE SERVER'S ANSWER.
	//!
	//! ⚠ THIS OVERLOAD READS THE DIRECTOR'S OWN RECORD AND IS THEREFORE SERVER-ONLY IN PRACTICE. None of
	//! the forward-base state replicates - deliberately, G12 - so on a remote client m_FOB is empty and
	//! this would refuse everything. The user action asks CanDismantleFOBAt() instead, about the flag it
	//! is attached to, which is a real entity on every machine.
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
	//! The dismantle rule itself, asked about an arbitrary forward-base position.
	//!
	//! ⚠ ONE IMPLEMENTATION, ASKED TWICE, AND THE SERVER'S ANSWER IS THE ONLY ONE THAT COUNTS. The user
	//! action asks this on the client, about the flag it is attached to, so the prompt can say why it is
	//! refusing; the request handler asks CanDismantleFOB() on the server, about the position the
	//! DIRECTOR believes the base is at, before anything happens. That is not redundancy - the client's
	//! copy is a courtesy and the server never trusts it, which is this epic's standing debt (BUG-025)
	//! and the reason this feature adds no third unvalidated verb. Sharing the body is what stops the
	//! text a player reads and the rule the server enforces from drifting apart, which is how "the action
	//! was available and did nothing" happens.
	//!
	//! ⚠ IT IS SAFE ON A CLIENT. Everything it reads - a position, the campaign's faction key, and the
	//! AI agents standing in the world - exists on every machine.
	//! \param[in] callerPosition Where the caller is standing.
	//! \param[in] fobPosition Where the forward base is.
	//! \param[out] refusal A localization key naming why, written only when this returns false.
	//! \return True when a dismantle would be allowed.
	bool CanDismantleFOBAt(vector callerPosition, vector fobPosition, out string refusal)
	{
		refusal = "";

		if (vector.Distance(callerPosition, fobPosition) > FOB_DISMANTLE_RANGE)
		{
			refusal = "#OVT-DismantleEnemyFOB_TooFar";
			return false;
		}

		if (CountOccupyingDefendersNear(fobPosition, FOB_DEFENDER_CLEAR_RADIUS) > 0)
		{
			refusal = "#OVT-DismantleEnemyFOB_Defended";
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! How many occupying-faction soldiers are still on their feet near a position.
	//!
	//! ⚠ AGENTS, NOT REGISTERED HANDLES, AND THAT IS THE OPPOSITE OF THE STARVATION COUNT ABOVE - on
	//! purpose. Starvation asks "does this base still have a garrison at all", which is true of a
	//! perfectly alive dormant group and must be answered off the survivor mask. This asks "is anybody
	//! SHOOTING AT ME right now", which is a question about materialised bodies: a player is standing at
	//! the flag, so everything nearby is spawned, and a dormant group two hundred metres away is not
	//! what stops a dismantle.
	//!
	//! ⚠ IT WALKS EVERY AI AGENT IN THE WORLD, which is the same thing the battle scorer does every ten
	//! seconds - fine at that rate and not fine per frame. The user action caches its answer for a
	//! second; anything else that starts asking this often should do the same.
	//! \param[in] position The place to count around.
	//! \param[in] radius How far, in metres.
	//! \return The count.
	int CountOccupyingDefendersNear(vector position, float radius)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return 0;

		AIWorld aiWorld = GetGame().GetAIWorld();
		if (!aiWorld)
			return 0;

		string occupyingKey = config.m_sOccupyingFaction;

		array<AIAgent> agents = new array<AIAgent>();
		aiWorld.GetAIAgents(agents);

		int defenders = 0;

		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity entity = agent.GetControlledEntity();
			if (!entity)
				continue;

			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
			if (!character)
				continue;

			if (character.GetFactionKey() != occupyingKey)
				continue;

			CharacterControllerComponent controller = character.GetCharacterController();
			if (controller && controller.IsDead())
				continue;

			if (vector.Distance(character.GetOrigin(), position) > radius)
				continue;

			defenders++;
		}

		return defenders;
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: a player has pulled the forward base's flag down.
	//!
	//! ⚠ THE ONE EXIT THAT COSTS THE OCCUPYING FACTION RESOURCES. The penalty is objectiveFOBCost taken
	//! back out of the deployment pool - what the base cost to raise - so clearing one is worth doing
	//! rather than merely satisfying. SubtractFactionResources floors the pool at zero, so a faction
	//! that has already spent everything simply pays what it has.
	//!
	//! ⚠ IT SUBTRACTS. IT NEVER ADDS. Grepping this directory for the deployment manager's credit method
	//! finds nothing, and must go on finding nothing - the name is deliberately not written out even in
	//! a comment, because that grep is an acceptance criterion and a quotation would defeat it. This
	//! component's whole accounting claim (G5) is that resources only ever leave the one pool through it.
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
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();

		if (deployments && config && difficulty && difficulty.objectiveFOBCost > 0)
			deployments.SubtractFactionResources(config.GetOccupyingFactionIndex(), difficulty.objectiveFOBCost);

		ResetObjective("the resistance cleared its forward base and dismantled it", true);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	// THE SPEND CEILING
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether the forward base's ceiling governs the director's spending right now.
	//!
	//! ⚠ THE CEILING IS NOT A WALLET AND THE DIRECTOR HOLDS NO MONEY. m_FOB.spent counts what has
	//! ALREADY left the one deployment pool on this forward base and everything sourced from it; nothing
	//! is reserved, held or moved by any of it. If you are reading this because it looks like a budget
	//! that should be topped up, refunded or carried over - it is not one, and turning it into one
	//! breaks the conserved-total identity the base-defense migration established (G5).
	//!
	//! It is inactive during harassment, so Phase 1 spends against the pool alone exactly as it did
	//! before this phase existed, and it arms the moment the forward base's own deployment is sent so
	//! that the structure's own cost is inside the budget (§3.7).
	//! \return True while spending is counted against the ceiling.
	protected bool IsFOBBudgetActive()
	{
		return m_FOB.up || m_bFOBDeploymentSent;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether one more spend of this size is still inside the forward base's ceiling.
	//!
	//! ⚠ THE PROSPECTIVE SPEND IS ADDED BEFORE THE TEST, because OVT_ObjectivePhaseRules.WithinFOBCeiling
	//! asks "would this total take me past the ceiling" and is inclusive at it. Its two-argument
	//! signature is pinned by Phase 2's logic cases and is deliberately not widened here.
	//! \param[in] cost What is about to be spent.
	//! \return True when the spend is permitted.
	protected bool WithinFOBBudget(int cost)
	{
		if (!IsFOBBudgetActive())
			return true;

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (!difficulty)
			return true;

		int ceiling = OVT_ObjectivePhaseRules.FOBBudgetCeiling(difficulty.objectiveFOBCost);

		if (OVT_ObjectivePhaseRules.WithinFOBCeiling(m_FOB.spent + cost, ceiling))
			return true;

		if (!m_bCeilingLogged)
		{
			m_bCeilingLogged = true;
			Print(LOG + "The forward base for objective '" + m_Objective.name + "' has spent its ceiling (" + m_FOB.spent.ToString() + " of " + ceiling.ToString() + ") and will buy nothing more", LogLevel.NORMAL);
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Counts a spend that has ALREADY left the pool against the forward base's ceiling.
	//! \param[in] cost What was spent.
	protected void CountFOBSpend(int cost)
	{
		if (!IsFOBBudgetActive())
			return;

		AddFOBSpend(cost);
	}

	//------------------------------------------------------------------------------------------------
	//! Phase 3. Waits for the battle the director asked for.
	//!
	//! THERE IS ALMOST NOTHING TO DO HERE BY DESIGN. While the battle is live the freeze above has
	//! already returned; the only tick that reaches this method is one where no battle is running,
	//! which means either the battle has finished or it never started. Both are terminal: the
	//! objective resets whatever the outcome was, exactly as §3.2 says.
	//!
	//! ⚠ THE BATTLE'S END IS POLLED, NEVER SUBSCRIBED (D8). Both of the occupying faction manager's
	//! own finish handlers delete the battle controller's entity from inside the invoker's own
	//! dispatch, so a second subscriber ordered after them would run against a deleted entity. One
	//! null check per in-game minute cannot be got wrong. The poll IS DirectorTick()'s third early
	//! return: while m_CurrentQRF is set the tick never reaches this method, and the first tick on which
	//! it is null again is the first tick that does.
	//!
	//! ⚠ A WIN AND A LOSS TAKE THE SAME PATH, DELIBERATELY (T8.5). The occupying faction has spent its
	//! ramp; whether it took the place or not, this objective is finished. ResetObjective() is the one
	//! path, so the forward base is torn down, the objective's deployments are collected, the bias is
	//! dropped and the machine goes idle - and the IDLE branch of the NEXT tick chooses again, one
	//! in-game minute later, which is the same one-tick gap every other reset in this machine has.
	//!
	//! ⚠ IT DOES NOT BLACKLIST. A resolved battle is not a failure of the objective: the place gets
	//! re-evaluated on its merits, and if the resistance held it, it is very likely worth attacking
	//! again.
	protected void TickCounterQRF()
	{
		AdvanceObjectiveTimers();

		ResetObjective("the counter-attack has resolved", false);
	}

	//------------------------------------------------------------------------------------------------
	//! Serves one tick of every countdown the current objective owns.
	//!
	//! ⚠ CALLED FROM THE PHASE HANDLERS, NEVER FROM THE TICK ITSELF. That is what puts every counter
	//! behind all three early returns at once: a frozen tick does not reach a phase handler, so it
	//! cannot decrement anything, and there is no separate rule to remember per timer.
	//!
	//! ⚠ ONE CALLER LEFT: TickCounterQRF(), which resets the objective on the same call and is therefore
	//! the only handler with no interest in whether the tick accomplished anything. The two ramp phases
	//! serve the cadence directly and put their idle clock through TickObjectiveIdleClock() instead.
	protected void AdvanceObjectiveTimers()
	{
		AdvancePhaseTimeout();
		AdvanceOperationCadence();
	}

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
	//! 🔴 THE BACKSTOP, AND THE WHOLE OF WHAT "THE MACHINE NEVER WEDGES" (R1) RESTS ON. Called LAST by
	//! each of the two ramp phase handlers, once per tick, with the one thing it cannot work out for
	//! itself: whether this tick created an operation.
	//!
	//! IT USED TO BE A PHASE BUDGET AND IT IS NOW AN IDLE CLOCK (2026-08-19), because a play-test proved
	//! the budget measured the wrong thing. The log, in real time at 6x:
	//!
	//!   13:03  objective selected
	//!   13:34  the FIRST operation is created - 31 real minutes later, because the pool could not
	//!          afford 100 resources after a battle drained it
	//!   13:37  the team's transport stops 1561 m short and they start walking
	//!   13:43  the phase "ran out of time", the objective is abandoned, and the walking team is deleted
	//!          with its truck five minutes into a fifteen-minute walk
	//!   13:43  the same base is selected again, and the loop starts over
	//!
	//! Nothing in that sequence is a wedge. The director was broke, then it was working. A budget counted
	//! from the phase entry cannot tell any of that apart from a machine that has genuinely stopped, so
	//! the same number now counts from the last thing that HAPPENED.
	//!
	//! FOUR ANSWERS, IN THIS ORDER, AND THE ORDER IS THE CONTRACT:
	//!  1. NO OBJECTIVE - nothing to time out. A sender may have reset the objective on this very tick
	//!     (SendFOBOperation() does exactly that when there is nowhere to put a forward base), and
	//!     running the clock against a cleared record would call ResetObjective() a second time.
	//!  2. PROGRESS - an operation was created this tick, or a completed one has reported since the last
	//!     time we looked. The clock goes back to full.
	//!  3. AN OPERATION IN FLIGHT - men this director sent are alive and on their way. That IS the
	//!     objective working, and it is the headline half of this fix: deleting a team five minutes short
	//!     of its target is wrong however the clock came to run out.
	//!  4. BLOCKED ONLY BY THE POOL - held, and said out loud once. See LogAffordabilityBlock().
	//! Only a tick that is none of those four serves a round, and only such a tick can end an objective.
	//!
	//! ⚠ WHAT HAPPENS TO AN OBJECTIVE THAT CAN NEVER BE AFFORDED, WRITTEN DOWN BECAUSE IT IS THE ONE
	//! STATE THIS METHOD DELIBERATELY LETS PERSIST. It SITS: the clock is held, so it is never abandoned;
	//! and it is never abandoned because the next objective would be exactly as unaffordable, so churning
	//! through targets would achieve nothing but noise. While it sits, NOTHING ACCUMULATES AND NOTHING
	//! LEAKS - no deployment is created, no resource moves in either direction, the teardown ledger does
	//! not grow, the cadence stays at zero so the next tick simply asks again, and the log line is latched
	//! to exactly one per objective. The moment the pool can cover an operation the very next tick creates
	//! one, that create is progress, the clock is re-armed and the ramp carries on from where it stopped.
	//! It is diagnosable from the log alone: one WARNING naming the objective, then silence, then the
	//! ordinary "Sent '<config>' ... for N resources" line that marks the recovery.
	//!
	//! ⚠ IT DECIDES NOTHING BY ITSELF. It returns a verdict and the CALLER resets, so every ending stays
	//! on DirectorTick() behind its three early returns like every other transition in this machine.
	//! \param[in] created Whether this tick created and paid for an operation.
	//! \return True when the objective has been idle for its whole budget and must be abandoned.
	protected bool TickObjectiveIdleClock(bool created)
	{
		bool blocked = m_bBlockedOnAffordability;
		m_bBlockedOnAffordability = false;

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
	//! ⚠ A PULL, NOT A PUSH, AND THAT IS D4. OnHarassmentSuccess() and OnSabotageSuccess() only count -
	//! they are public, are called from a deployment's own update, from a restore and from fixtures
	//! arranging a state, and two initialisation cases pin them as unable to move a timer. Comparing the
	//! counters here puts the observation on the tick, where every other decision in this machine lives.
	//!
	//! ⚠ IT COMPARES RATHER THAN SUBTRACTS, so a counter that went DOWN (a fresh commit zeroes both) also
	//! reads as news and re-syncs, instead of leaving a mark permanently above the counter it guards.
	//! \return True when an operation has reported since the last re-arm.
	protected bool ConsumeReportedOperations()
	{
		if (m_Objective.harassmentSuccesses == m_iProgressHarassmentMark && m_Objective.sabotageSuccesses == m_iProgressSabotageMark)
			return false;

		SyncProgressMarks();

		return true;
	}

	//! Puts the idle clock back to its full budget and re-baselines the success marks with it.
	protected void RearmObjectiveIdleClock()
	{
		SetPhaseTimeout(m_iPhaseTimeoutTicks);
	}

	//! Records the success counters as "already seen", so nothing already banked reads as fresh progress.
	protected void SyncProgressMarks()
	{
		m_iProgressHarassmentMark = m_Objective.harassmentSuccesses;
		m_iProgressSabotageMark = m_Objective.sabotageSuccesses;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether an operation THIS DIRECTOR sent is still alive and still on its way.
	//!
	//! 🔴 THE HEADLINE HALF OF THE 2026-08-19 FIX. A sabotage team with 1561 m left to walk was deleted,
	//! with its truck, because a clock the team had nothing to do with ran out. Men who are walking to a
	//! target are the objective WORKING; a machine that cannot tell that from a wedge will always
	//! eventually throw away the operation it was waiting for.
	//!
	//! ⚠ WHAT COUNTS IS SCOPED BY IsObjectiveOperationConfig(), AND THE SCOPE IS LOAD-BEARING. If a
	//! standing forward base or its garrison counted, the forward-base phase could never time out at all
	//! and R1 would be gone - a base that is up with a full garrison and a counter-attack gate that will
	//! never open is EXACTLY the wedge the backstop exists to catch.
	//!
	//! ⚠ A FORCE THAT WAS WIPED OUT IS NOT IN FLIGHT. Its marker can outlive it (the condition module
	//! collects it a frame or a minute later), and reading a dead team as work in progress would hold the
	//! clock open forever on a corpse. Same predicate the refund uses, deliberately - see
	//! TearDownObjectiveDeployments().
	//!
	//! ⚠ READ FROM THE LIVE DEPLOYMENT LIST THROUGH THE TEARDOWN LEDGER, which is the one count in this
	//! component that legitimately walks m_aCreatedDeployments: the question is not "is anything of this
	//! kind nearby" but "is something WE sent still out there", and only the ledger knows the difference.
	//! An entry whose deployment has gone simply does not resolve, which is not an error.
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
	//! ⚠ ONE PREDICATE, TWO USES, AND THE FACT THAT THEY ARE THE SAME QUESTION IS THE POINT. "Is this
	//! operation still in flight" and "is tearing this down a RECALL rather than a write-off" are the
	//! same statement about the same deployment, so they read the same answer from here and can never
	//! disagree - a director that held its clock for a team and then refused to refund it, or refunded a
	//! standing base it had already got the value of, would be incoherent in a way nothing would catch.
	//!
	//! ⚠ THE SUPPLY PARTY IS AN OPERATION ONLY UNTIL THE FLAG IS UP. Before the raise it is a truck on a
	//! road, indistinguishable from a sabotage team; after it, it is the standing installation the whole
	//! phase is built around, its money has bought exactly what it was for, and it must neither hold the
	//! clock nor pay anything back. m_FOB.up is read live for that reason, and ResetObjective() tears the
	//! ledger down BEFORE ClearFOBRecord() so this still reads the truth during a teardown.
	//!
	//! ⚠ THE GARRISON IS NEVER ONE. It is bought FOR a base that is already standing and it stays there;
	//! there is no journey to be interrupted and nothing to recall it from.
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
	//! Says once, per objective, that the ramp is working but the faction cannot pay for it.
	//!
	//! ⚠ MANDATORY, AND ONCE. The play-test this whole change came out of spent 31 real minutes watching
	//! a director that had selected a target and then appeared to do nothing, with not one line in the log
	//! to explain it - because a refused create is silent and the retry is silent too. This is that line.
	//! It is latched per objective for the reason every other latch in this file is: a refused create
	//! leaves the cadence at zero, so poverty is re-tested every single in-game minute.
	protected void LogAffordabilityBlock()
	{
		if (m_bAffordabilityBlockLogged)
			return;

		m_bAffordabilityBlockLogged = true;

		Print(LOG + "Objective '" + m_Objective.name + "' cannot afford its next operation: the occupying faction's deployment pool is short of what the operation costs. Its idle clock is HELD while it waits - being broke is not a failure of the objective, and abandoning it would only find the next objective just as unaffordable. It resumes on the first tick the pool can cover one; the next \"Sent ...\" line is that moment", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	// SELECTION
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Chooses the objective, out of every resistance-held base, town and city.
	//!
	//! DELIBERATELY OMNISCIENT AND DELIBERATELY PREDICTABLE. There is no fog of war here and no
	//! randomness at all: an experienced player is supposed to be able to guess the target from the
	//! map. The score is a plain weighted sum in OVT_ObjectiveSelection, ties break on discovery
	//! order, and the winner AND the runner-up are both logged with their scores so a tuner can check
	//! the ordering without instrumenting anything.
	//!
	//! VILLAGES ARE EXCLUDED, and so are forward bases and radio towers: the requirements are explicit
	//! that towers are handled WITHIN an objective and that villages fall as collateral.
	void SelectObjective()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!occupying || !towns || !config)
			return;

		int resistanceIndex = config.GetPlayerFactionIndex();

		array<int> kinds = new array<int>();
		array<vector> positions = new array<vector>();
		array<string> names = new array<string>();
		array<float> scores = new array<float>();
		array<bool> blacklisted = new array<bool>();

		array<string> blacklistKeys = new array<string>();
		array<int> blacklistRounds = new array<int>();
		ReadBlacklist(blacklistKeys, blacklistRounds);

		CollectTownCandidates(occupying, towns, resistanceIndex, kinds, positions, names, scores);
		CollectBaseCandidates(occupying, resistanceIndex, kinds, positions, names, scores);

		foreach (vector candidate : positions)
		{
			blacklisted.Insert(OVT_ObjectiveSelection.IsBlacklisted(blacklistKeys, blacklistRounds, OVT_ObjectiveSelection.PositionKey(candidate)));
		}

		int best = OVT_ObjectiveSelection.SelectBestIndex(scores, blacklisted);

		// ONE ROUND SERVED PER SELECTION ROUND, and it is served AFTER the pick, not before: an
		// objective blacklisted for one round has to actually miss this round.
		ServeBlacklistRound();

		if (best == OVT_ObjectiveSelection.NOTHING_TO_SELECT)
		{
			EnterIdle();
			return;
		}

		OVT_EObjectiveKind kind = OVT_EObjectiveKind.TOWN;
		if (kinds[best] == OVT_EObjectiveKind.BASE)
			kind = OVT_EObjectiveKind.BASE;

		LogSelection(best, kinds, names, scores, blacklisted);

		CommitObjective(kind, positions[best], names[best]);
	}

	//------------------------------------------------------------------------------------------------
	//! Adds every resistance-held town and city to the candidate lists.
	//! \param[in] occupying The occupying faction manager, for the geometry inputs.
	//! \param[in] towns The town manager.
	//! \param[in] resistanceIndex The resistance faction's index.
	//! \param[inout] kinds Candidate kinds, appended to.
	//! \param[inout] positions Candidate positions, appended to.
	//! \param[inout] names Candidate names, appended to.
	//! \param[inout] scores Candidate scores, appended to.
	protected void CollectTownCandidates(notnull OVT_OccupyingFactionManager occupying, notnull OVT_TownManagerComponent towns, int resistanceIndex, notnull array<int> kinds, notnull array<vector> positions, notnull array<string> names, notnull array<float> scores)
	{
		if (!towns.m_Towns)
			return;

		foreach (OVT_TownData town : towns.m_Towns)
		{
			if (!town)
				continue;

			if (town.faction != resistanceIndex)
				continue;

			// Villages fall as collateral - they are never worth a campaign of their own.
			if (town.size == OVT_TownSize.VILLAGE)
				continue;

			float distance = DistanceToNearestHeldBase(occupying, town.location);
			bool covered = HasOccupyingTowerCoverage(occupying, town.location);

			kinds.Insert(OVT_EObjectiveKind.TOWN);
			positions.Insert(town.location);
			names.Insert(ResolveTownName(towns, town));
			scores.Insert(OVT_ObjectiveSelection.ScoreTown(town.population, town.SupportPercentage(), distance, m_fMaxUsefulDistance, covered));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Adds every resistance-held base to the candidate lists.
	//! \param[in] occupying The occupying faction manager.
	//! \param[in] resistanceIndex The resistance faction's index.
	//! \param[inout] kinds Candidate kinds, appended to.
	//! \param[inout] positions Candidate positions, appended to.
	//! \param[inout] names Candidate names, appended to.
	//! \param[inout] scores Candidate scores, appended to.
	protected void CollectBaseCandidates(notnull OVT_OccupyingFactionManager occupying, int resistanceIndex, notnull array<int> kinds, notnull array<vector> positions, notnull array<string> names, notnull array<float> scores)
	{
		array<OVT_BaseData> bases = occupying.GetBasesControlledBy(resistanceIndex);
		if (!bases)
			return;

		foreach (OVT_BaseData base : bases)
		{
			if (!base)
				continue;

			float distance = DistanceToNearestHeldBase(occupying, base.location);
			bool covered = HasOccupyingTowerCoverage(occupying, base.location);

			kinds.Insert(OVT_EObjectiveKind.BASE);
			positions.Insert(base.location);
			names.Insert(ResolveBaseName(occupying, base));
			scores.Insert(OVT_ObjectiveSelection.ScoreBase(occupying.GetThreatByLocation(base.location), distance, m_fMaxUsefulDistance, covered));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! How far a candidate is from the nearest base the occupying faction still holds.
	//! \param[in] occupying The occupying faction manager.
	//! \param[in] position The candidate's position.
	//! \return The distance, or the maximum useful distance when the faction holds no base at all -
	//! which scores the proximity term at zero rather than pretending the candidate is next door.
	protected float DistanceToNearestHeldBase(notnull OVT_OccupyingFactionManager occupying, vector position)
	{
		OVT_BaseData nearest = occupying.GetNearestOccupiedBase(position);
		if (!nearest)
			return m_fMaxUsefulDistance;

		return vector.Distance(nearest.location, position);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether an occupying-held radio tower still broadcasts over a candidate.
	//! \param[in] occupying The occupying faction manager.
	//! \param[in] position The candidate's position.
	//! \return True when at least one tower in range is on the air and occupying-held.
	protected bool HasOccupyingTowerCoverage(notnull OVT_OccupyingFactionManager occupying, vector position)
	{
		array<OVT_RadioTowerData> towers = occupying.GetRadioTowersAffecting(position);
		if (!towers)
			return false;

		foreach (OVT_RadioTowerData tower : towers)
		{
			if (tower && tower.IsOccupyingFaction())
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A base's display name, falling back to the nearest town's when the marker carries none.
	//! \param[in] occupying The occupying faction manager.
	//! \param[in] base The base record.
	//! \return A name for logs and for the GM panel. Never empty.
	protected string ResolveBaseName(notnull OVT_OccupyingFactionManager occupying, notnull OVT_BaseData base)
	{
		// Resolved here rather than through the manager's own GetBase(), which dereferences the marker
		// without checking it is still there - the same hazard its serializer documents avoiding.
		IEntity marker = GetGame().GetWorld().FindEntityByID(base.entId);
		if (marker)
		{
			OVT_BaseControllerComponent controller = OVT_BaseControllerComponent.Cast(marker.FindComponent(OVT_BaseControllerComponent));
			if (controller && controller.m_sName != "")
				return controller.m_sName;
		}

		return ResolveTownNameAt(OVT_Global.GetTowns(), base.location);
	}

	//------------------------------------------------------------------------------------------------
	//! A town's display name, resolved SAFELY.
	//!
	//! ⚠ THE GUARD IS NOT DEFENSIVE PROGRAMMING FOR ITS OWN SAKE. GetTownName() dereferences the town's
	//! map marker's item without checking the marker is there, and it is called here once per candidate
	//! per selection - which is every in-game minute, forever, on the server. A town with no marker
	//! within five metres (a hand-defined town, a world layer that moved one) would take the whole
	//! campaign down. The marker is therefore checked first, and only for names that are not already
	//! cached; an unresolvable name is empty, which costs a log line its label and nothing else.
	//! \param[in] towns The town manager. Null yields an empty name.
	//! \param[in] town The town. Null yields an empty name.
	//! \return The name, or an empty string.
	protected string ResolveTownName(OVT_TownManagerComponent towns, OVT_TownData town)
	{
		if (!towns || !town)
			return "";

		int townId = towns.GetTownID(town);
		if (townId < 0)
			return "";

		if (towns.m_TownNames && townId < towns.m_TownNames.Count() && towns.m_TownNames[townId] != "")
			return towns.m_TownNames[townId];

		if (!towns.GetNearestTownMarker(town.location))
			return "";

		return towns.GetTownName(townId);
	}

	//------------------------------------------------------------------------------------------------
	//! The name of the town nearest a position, resolved safely.
	//! \param[in] towns The town manager. Null yields an empty name.
	//! \param[in] position The position to resolve near.
	//! \return The name, or an empty string.
	protected string ResolveTownNameAt(OVT_TownManagerComponent towns, vector position)
	{
		if (!towns)
			return "";

		return ResolveTownName(towns, towns.GetNearestTown(position));
	}

	//------------------------------------------------------------------------------------------------
	//! Commits to a candidate and enters the harassment phase.
	//!
	//! Public because it is also how a restored payload and a scripted scenario put the machine into a
	//! known state - the whole point of every mutation going through one method is that there is only
	//! one place that decides what "a fresh objective" means.
	//! \param[in] kind What kind of place it is.
	//! \param[in] position Where it is.
	//! \param[in] name Display name for logs and the GM panel.
	void CommitObjective(OVT_EObjectiveKind kind, vector position, string name)
	{
		if (kind == OVT_EObjectiveKind.NONE)
		{
			EnterIdle();
			return;
		}

		m_Objective.kind = kind;
		m_Objective.position = position;
		m_Objective.name = name;
		m_Objective.harassmentSuccesses = 0;
		m_Objective.sabotageSuccesses = 0;

		// ⚠ TORN DOWN, NOT MERELY FORGOTTEN. A forward base only exists in the phase that is locked
		// against re-selection, so in the live machine there is never one standing here - but
		// ClearFOBRecord() alone would leave a structure and a garrison in the world with nothing
		// pointing at them, and "the record was cleared" is not the same statement as "the base is
		// gone". Cheap: the teardown returns immediately when nothing was ever sent.
		TearDownFOB();
		ClearFOBRecord();
		m_aCreatedDeployments.Clear();

		m_bIdleLogged = false;

		// A pending re-selection request is ANSWERED by committing: the request means "the map changed,
		// re-evaluate the target", and this is the re-evaluation. Leaving it set would fire a second,
		// immediate selection on the next tick against a map nothing had changed in since.
		m_bReselectPending = false;

		EnterPhase(OVT_EObjectivePhase.HARASSMENT);
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
	//! Logs the winner and the runner-up, both with their scores.
	//!
	//! THE RUNNER-UP IS NOT DECORATION. Predictability is a stated requirement (G1), and the only way
	//! a tuner can check that the weights order candidates the way the design intends is to see what
	//! came second and by how much. It is found by re-running the same pure selection with the winner
	//! masked out, so the log can never disagree with the pick.
	//! \param[in] best Index of the chosen candidate.
	//! \param[in] kinds Candidate kinds.
	//! \param[in] names Candidate names.
	//! \param[in] scores Candidate scores.
	//! \param[in] blacklisted Which candidates were sitting out.
	protected void LogSelection(int best, notnull array<int> kinds, notnull array<string> names, notnull array<float> scores, notnull array<bool> blacklisted)
	{
		string kindLabel = "town";
		if (kinds[best] == OVT_EObjectiveKind.BASE)
			kindLabel = "base";

		string line = LOG + "Objective: " + kindLabel + " '" + names[best] + "' at score " + scores[best].ToString();

		array<bool> masked = new array<bool>();
		foreach (bool flag : blacklisted)
		{
			masked.Insert(flag);
		}
		masked[best] = true;

		int runnerUp = OVT_ObjectiveSelection.SelectBestIndex(scores, masked);
		if (runnerUp != OVT_ObjectiveSelection.NOTHING_TO_SELECT)
			line = line + ", ahead of '" + names[runnerUp] + "' at " + scores[runnerUp].ToString();
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
	//! ⚠ EVERY PHASE ENTRY GOES THROUGH HERE, so every phase gets an exit for free. A phase that could
	//! be entered without arming the timeout is a phase the machine can wedge in, and "the occupying
	//! faction stopped doing anything" is the one failure mode with no symptom a player could report.
	//!
	//! ⚠ A TRANSITION IS PROGRESS, WHICH IS WHY THE ARM GOES THROUGH SetPhaseTimeout() RATHER THAN
	//! WRITING THE FIELD. The idle clock's progress marks have to be re-baselined with it, or the
	//! successes that OPENED this gate would read as fresh news on the first tick of the new phase.
	//! \param[in] phase The phase to enter.
	void EnterPhase(OVT_EObjectivePhase phase)
	{
		if (phase == OVT_EObjectivePhase.IDLE)
		{
			EnterIdle();
			return;
		}

		m_Objective.phase = phase;
		SetPhaseTimeout(m_iPhaseTimeoutTicks);
		m_Objective.nextOpTicks = 0;

		// The bias widens with the phase, so it is re-pushed on every entry rather than only on the
		// first. Committing to an objective enters HARASSMENT through here too, which is why this is
		// the only push site the live machine needs.
		PushObjectiveAnchor();
	}

	//------------------------------------------------------------------------------------------------
	// THE OBJECTIVE ANCHOR - HOW THE REST OF THE CAMPAIGN LEARNS WHERE THE OBJECTIVE IS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Points the occupying faction's ROUTINE deployment spending at the current objective.
	//!
	//! ⚠ THE DEPENDENCY POINTS ONE WAY, AND THAT IS THE WHOLE DESIGN (D5). This component pushes a
	//! position, a radius and a weight into the deployment framework; the framework has never heard of
	//! this component, holds no reference to it and asks it nothing. So the evaluator stays testable
	//! with no director present, its hottest loop gains no null-check on a component that may not
	//! exist, and a faction nobody biases behaves exactly as it did before this feature.
	//!
	//! ⚠ IT BIASES ORDERING, NEVER ELIGIBILITY. The weight is added to a candidate's SORT KEY only. It
	//! cannot make an unsuitable config suitable, cannot raise the per-faction deployment ceiling,
	//! cannot raise the per-pass cap and cannot conjure resources. A saturated map still buys nothing;
	//! it buys near the objective first.
	//!
	//! ⚠ THIS IS NOT HOW THE DIRECTOR'S OWN OPERATIONS ARE CREATED. Harassment, sabotage, tower
	//! recapture and the forward base are created explicitly, because their cadence and their ramp are
	//! this component's to own and the evaluator's threat sort would make both unpredictable. The
	//! anchor only leans on the ROUTINE spending the evaluator was doing anyway.
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

		deployments.SetObjectiveAnchor(occupyingIndex, m_Objective.position, AnchorRadiusForPhase(m_Objective.phase), m_fObjectiveAnchorWeight);
	}

	//------------------------------------------------------------------------------------------------
	//! Stops the occupying faction leaning on a place it is no longer working toward.
	//!
	//! Idempotent and safe with no objective, no deployment framework and no campaign - it is called
	//! from every path that goes idle and from every reset.
	//!
	//! ⚠ A NON-NULL CONFIG IS NOT ENOUGH, WHICH IS WHAT MADE THAT CLAIM A LIE FOR ONE CONTEXT
	//! (2026-08-19). GetOccupyingFactionIndex() resolves through the FACTION MANAGER, which is a
	//! different object with a different lifetime, and it answers -1 for "cannot tell yet" rather than
	//! guessing. Two things follow and both are asserted here: the index is read into a local and
	//! checked before it can reach the store, because a negative number is a perfectly valid map key
	//! and would file a bias against a faction that does not exist; and there is nothing to drop
	//! anyway, because a campaign with no resolvable occupying faction has never pushed one.
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
	//! How far the bias reaches in a given phase.
	//! \param[in] phase The phase the objective is in.
	//! \return A radius in metres, or zero for a phase that carries no bias at all.
	float AnchorRadiusForPhase(OVT_EObjectivePhase phase)
	{
		if (phase == OVT_EObjectivePhase.HARASSMENT)
			return HARASSMENT_ANCHOR_RADIUS;

		if (phase == OVT_EObjectivePhase.FOB)
			return FORWARD_ANCHOR_RADIUS;

		if (phase == OVT_EObjectivePhase.COUNTER_QRF)
			return FORWARD_ANCHOR_RADIUS;

		// IDLE, and anything a later build appends without deciding what it means. Zero is refused by
		// SetObjectiveAnchor(), which clears rather than storing a dead anchor - so an unhandled phase
		// fails safe to "no bias" instead of to a bias of unknown reach.
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! THE ONE RESET PATH - every failure and every ending comes through here.
	//!
	//! In order: the objective's own deployments are taken back down, the forward-base record is
	//! cleared, the objective is optionally sent to the back of the queue for a round, and the machine
	//! goes idle. The reason is ALWAYS logged at warning level: "the occupying faction never attacks
	//! town X" has to have an explanation in a log rather than a repro.
	//!
	//! ⚠ THE BIAS IS DROPPED ONE LEVEL DOWN, IN ClearObjectiveRecord(), NOT HERE. Phase 3 of the build
	//! reserved this spot for it and then found a second way to end up with no objective: a
	//! re-selection that runs during harassment and finds nothing selectable goes EnterIdle() ->
	//! ClearObjectiveRecord() without ever reaching this method, and would have left the evaluator
	//! leaning on a place the machine had already given up on. ClearObjectiveRecord() is the single
	//! method both paths funnel through, so the drop lives there and still lives in exactly one place.
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
		// insertion reservations and the trucks; this sweep is for what the ledger could not know about
		// (a restored marker) and for the structure itself, and it needs m_FOB.position to find either.
		TearDownFOB();

		ClearFOBRecord();
		ClearObjectiveRecord();

		m_bIdleLogged = false;
		m_bRestorePending = false;
		m_iRelinkAttempts = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes every deployment the director created for the current objective.
	//!
	//! TRACKED BY CONFIG NAME PLUS POSITION rather than by entity id, because ids do not survive a
	//! session and because that pair is exactly what the deployment framework's own lookup takes. A
	//! deployment that has already gone (wiped out, collected by its own condition module) simply is
	//! not found, which is not an error.
	//!
	//! 🔴 AN UNFINISHED OPERATION IS RECALLED, NOT WRITTEN OFF (2026-08-19). Before this, a director that
	//! abandoned an objective deleted the sabotage team that was still walking to it AND the 100 resources
	//! that had bought them: the pool was debited at the create and nothing ever gave it back. Calling men
	//! home is not the same as losing them, and the money for a job that was never done has to return to
	//! the pool - otherwise every abandoned objective is a permanent tax on a faction that is usually
	//! abandoning it because it was already poor.
	//!
	//! ⚠ THE REFUND IS THE FRAMEWORK'S, NOT THE DIRECTOR'S, AND THAT IS AN ACCEPTANCE CRITERION RATHER
	//! THAN A STYLE CHOICE. Q6 checks this feature by grepping Scripts/Game/GameMode/Objectives/ for the
	//! deployment framework's pool-credit method and requiring NO hits - the director subtracts and never
	//! credits, not even in a comment, which is why this paragraph does not spell the method's name.
	//! RecallDeployment() puts the credit in OVT_DeploymentManagerComponent, beside the framework's other
	//! refund, where the deployment being refunded for lives and where its "cannot double-credit"
	//! property can be structural rather than a rule. Read its header before moving it.
	//!
	//! ⚠ ONLY OPERATIONS, AND ONLY LIVE ONES. IsObjectiveOperationConfig() decides the first (a standing
	//! forward base got exactly what its money bought and pays nothing back) and RecallDeployment()
	//! decides the second (a team the resistance killed is a loss, not a recall). The two rules together
	//! are why "every resource leaves the pool exactly once" (G5) survives this: a refund is only ever
	//! paid for work that was bought and never delivered, at most once per deployment, at the single
	//! moment its deployment is destroyed.
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
	//! One harassment operation completed at the objective. Drives the group ladder.
	//!
	//! ⚠ IT COUNTS. IT DOES NOT DECIDE. This method may never change phase, and the reason is worth
	//! stating because T5.8 asked for the opposite ("increments the counter and re-checks the Phase 2
	//! gate") and the build tried it:
	//!
	//!   A COUNTER INCREMENT IS NOT A TICK. Everything that moves this machine moves on DirectorTick(),
	//!   behind its three early returns - not the server, nobody online, a battle is live. This method
	//!   is PUBLIC and is called from a deployment's own update, from a restore, and from test
	//!   fixtures arranging a known state. Transitioning from here means any of those silently
	//!   advances the ramp: a fixture that bumps the counter to arrange "three operations completed"
	//!   found itself saving a FOB-phase objective it never asked for, and a phase entry re-arms the
	//!   phase timeout, so the same call also overwrote a planted countdown. Both are contract
	//!   breaches (D4 - a timer moves only by a tick; G6 - the objective survives a save unchanged)
	//!   and neither is visible at the call site.
	//!
	//! The cost of counting only: the phase advances on the next director tick rather than in the same
	//! frame as the last debuff - at most one in-game minute later, on a ramp whose cadence is measured
	//! in tens of them.
	void OnHarassmentSuccess()
	{
		if (m_Objective.kind == OVT_EObjectiveKind.NONE)
			return;

		m_Objective.harassmentSuccesses = m_Objective.harassmentSuccesses + 1;
	}

	//------------------------------------------------------------------------------------------------
	//! One sabotage mission completed at the objective. Drives the base forward-base and counter-attack
	//! gates.
	//!
	//! ⚠ IT COUNTS. IT DOES NOT DECIDE - and T6.6 asked for the opposite ("increments the counter and
	//! re-checks the Phase 2 gate"), exactly as T5.8 did for harassment. It is the same refusal for the
	//! same reason, and Phase 5 already paid for the lesson with two red cases in two suites: this
	//! method is PUBLIC and is called from a deployment's own update, from a restore and from test
	//! fixtures arranging a known state, none of which is a tick. Every phase transition happens on
	//! DirectorTick(), behind its three early returns - not the server, nobody online, a battle is live -
	//! and a phase entry re-arms the phase timeout, so a transition from here would also silently
	//! overwrite planted countdowns and could save a phase nobody asked for. The base gate lives in
	//! CheckBaseHarassmentGate(), reached only from TickHarassment().
	//!
	//! The cost of counting only: the phase advances on the next director tick rather than in the same
	//! frame as the last demolition - at most one in-game minute later.
	void OnSabotageSuccess()
	{
		if (m_Objective.kind == OVT_EObjectiveKind.NONE)
			return;

		m_Objective.sabotageSuccesses = m_Objective.sabotageSuccesses + 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Records that the forward operating base is standing.
	//! \param[in] position Where the structure stands.
	//! \param[in] sourceBasePosition The base supplying it.
	//! \param[in] deploymentName Config name of the deployment carrying it - the re-link key.
	void RecordFOB(vector position, vector sourceBasePosition, string deploymentName)
	{
		m_FOB.up = true;
		m_FOB.position = position;
		m_FOB.sourceBasePosition = sourceBasePosition;
		m_FOB.deploymentName = deploymentName;
		m_FOB.starvationTicks = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Counts resources already spent from the deployment pool against the forward base's ceiling.
	//!
	//! ⚠ THIS MOVES NO MONEY. It is called AFTER the pool has been debited, and its only job is to
	//! stop the ramp spending past the ceiling. See the record's own header.
	//! \param[in] amount What was spent. Non-positive is ignored.
	void AddFOBSpend(int amount)
	{
		if (amount <= 0)
			return;

		m_FOB.spent = m_FOB.spent + amount;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets how many consecutive ticks the forward base has been cut off.
	//! \param[in] ticks The count. Negative reads as zero.
	void SetFOBStarvationTicks(int ticks)
	{
		if (ticks < 0)
			m_FOB.starvationTicks = 0;
		else
			m_FOB.starvationTicks = ticks;
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
	//! ⚠ IT ALSO RE-BASELINES THE PROGRESS MARKS, AND THAT SECOND HALF IS WHY EVERY PLANT MUST COME
	//! THROUGH HERE. The clock is re-armed when an operation REPORTS, which the tick detects by comparing
	//! the success counters against a mark. A caller that plants a clock is declaring the state of the
	//! world as of that moment - so operations already banked (a fixture arranging "three completed", a
	//! restore adopting a saved count, a phase entry starting fresh) must not read as news on the very
	//! next tick and immediately overwrite the value that was just planted.
	//!
	//! THREE CALLERS AND THEY ALL MEAN THE SAME THING: EnterPhase() (a transition IS progress),
	//! RearmObjectiveIdleClock() (progress observed on a tick), and whatever plants a known state - a test
	//! fixture or ApplyPersistedObjective(), which assigns the record's fields directly and then calls
	//! SyncProgressMarks() itself because its counters are written AFTER its clock.
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
	//! ⚠ REMOVE BEFORE INSERT, ALWAYS. A ScriptInvoker has no Contains() and does not de-duplicate, so
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
	//! ⚠ THIS HANDLER MAY NOT READ OWNERSHIP, AND THAT IS NOT A STYLE PREFERENCE (D3). The invoker
	//! fires from OVT_BaseControllerComponent.SetControllingFaction() BEFORE the affiliation is
	//! written, so anything asking who owns the base from in here gets the OLD owner - and a director
	//! that re-selected inline could pick as its next target the base the player has just taken.
	//! Setting a flag and returning is the whole handler.
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
	//! ⚠ IT TOUCHES NO POOL AND NO DEPLOYMENT, AND THAT IS A LOAD-ORDER RULE, NOT TIDINESS. The
	//! deployment manager's own restore CLEARS AND REFILLS the faction resource pool and runs after
	//! the game-mode component serializers, so anything this did with money would be overwritten
	//! moments later; and the deployment entities are separate tracked instances whose restore order
	//! relative to this payload is not defined. Everything that needs either is deferred to the first
	//! tick - the same pattern the legacy upgrade refund uses.
	//!
	//! IDEMPOTENT. Re-applying a save to a live session runs this again, and every line of it is an
	//! assignment or a clear-and-rebuild.
	//! \param[in] kind The persisted objective kind.
	//! \param[in] position Where it was.
	//! \param[in] phase Which phase it was in.
	//! \param[in] phaseTicks Ticks left on the phase timeout.
	//! \param[in] nextOpTicks Ticks left until the next operation.
	//! \param[in] harassmentSuccesses Harassment operations completed there.
	//! \param[in] sabotageSuccesses Sabotage missions completed there.
	//! \param[in] blacklistPositions Blacklisted places.
	//! \param[in] blacklistRounds Rounds each of them still owes, same order, same length.
	//! \param[in] fob The forward-base record, or null when none was saved.
	void ApplyPersistedObjective(int kind, vector position, int phase, int phaseTicks, int nextOpTicks, int harassmentSuccesses, int sabotageSuccesses, array<vector> blacklistPositions, array<int> blacklistRounds, OVT_ObjectiveFOBRecord fob)
	{
		m_Objective.kind = KindFromInt(kind);
		m_Objective.position = position;
		m_Objective.phase = PhaseFromInt(phase);
		m_Objective.phaseTicks = phaseTicks;
		m_Objective.nextOpTicks = nextOpTicks;
		m_Objective.harassmentSuccesses = harassmentSuccesses;
		m_Objective.sabotageSuccesses = sabotageSuccesses;

		// ⚠ AFTER BOTH COUNTERS, NEVER BEFORE. The idle clock detects a completed operation by comparing
		// these counters against a mark; a restored count is history, not news, and a mark left at zero
		// would make the first tick after a load treat the whole saved ramp as progress and overwrite the
		// clock that was just restored with a fresh full budget. This is the one place the marks are
		// synced by hand, because the payload writes the clock BEFORE the counters and SetPhaseTimeout()
		// is not on this path.
		SyncProgressMarks();

		// The name is a LABEL, not an identifier, and is not in the payload. It is re-resolved from
		// the live campaign on the first tick, which is also the only moment the town and base
		// registries are guaranteed to be populated.
		m_Objective.name = "";

		m_aBlacklist.Clear();
		if (blacklistPositions && blacklistRounds && blacklistPositions.Count() == blacklistRounds.Count())
		{
			int count = blacklistPositions.Count();
			for (int i = 0; i < count; i++)
			{
				if (blacklistRounds[i] <= 0)
					continue;

				OVT_ObjectiveBlacklistEntry entry = new OVT_ObjectiveBlacklistEntry();
				entry.position = blacklistPositions[i];
				entry.roundsLeft = blacklistRounds[i];

				m_aBlacklist.Insert(entry);
			}
		}

		ClearFOBRecord();
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

		m_bRestorePending = true;
		m_iRelinkAttempts = 0;
		m_bIdleLogged = false;
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

	//------------------------------------------------------------------------------------------------
	//! Reads a persisted phase, refusing anything the running build does not recognise.
	//! \param[in] value The persisted integer.
	//! \return A phase this build understands. Anything unknown reads as IDLE.
	protected OVT_EObjectivePhase PhaseFromInt(int value)
	{
		if (value == OVT_EObjectivePhase.HARASSMENT)
			return OVT_EObjectivePhase.HARASSMENT;

		if (value == OVT_EObjectivePhase.FOB)
			return OVT_EObjectivePhase.FOB;

		if (value == OVT_EObjectivePhase.COUNTER_QRF)
			return OVT_EObjectivePhase.COUNTER_QRF;

		return OVT_EObjectivePhase.IDLE;
	}

	//------------------------------------------------------------------------------------------------
	//! Re-links a restored objective to the live campaign, on the first tick after a load.
	//!
	//! Three jobs, none of which a deserialization is allowed to do:
	//!  1. A BATTLE THAT WAS LIVE WHEN THE SAVE WAS TAKEN IS GONE. Nothing about the battle controller
	//!     is persisted, so an objective restored mid-counter-attack has no battle to wait for and
	//!     resets - the same roll-back the campaign has always done.
	//!  2. THE DISPLAY NAME is re-resolved from the town and base registries.
	//!  3. THE FORWARD BASE'S DEPLOYMENT is found again by name plus position. If it is not there
	//!     after FOB_RELINK_ATTEMPTS ticks the marker went away while the campaign was saved, and the
	//!     objective is torn down with the reason in the log rather than left pointing at nothing.
	protected void ResolveRestoredObjective()
	{
		if (m_Objective.kind == OVT_EObjectiveKind.NONE)
		{
			m_bRestorePending = false;
			return;
		}

		if (m_Objective.phase == OVT_EObjectivePhase.COUNTER_QRF)
		{
			m_bRestorePending = false;
			ResetObjective("the counter-attack it was saved in the middle of did not survive the load", false);
			return;
		}

		if (m_Objective.name == "")
			m_Objective.name = ResolveObjectiveName();

		// THE RESTORED OBJECTIVE RE-ACQUIRES ITS BIAS HERE, and this is the only path that needs its
		// own push. A restore writes the phase straight into the record rather than entering it, so
		// EnterPhase() - the live machine's one push site - never runs. Doing it on this first tick
		// rather than during deserialization is the same load-order rule the rest of the restore
		// follows: the deployment framework rebuilds its own per-faction state AFTER the game mode's
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

			return ResolveBaseName(occupying, base);
		}

		return ResolveTownNameAt(OVT_Global.GetTowns(), m_Objective.position);
	}

	//------------------------------------------------------------------------------------------------
	// STATE HELPERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Puts the objective record back to "no objective".
	//!
	//! ⚠ THE ONE PLACE THE DEPLOYMENT BIAS IS DROPPED, and it is here rather than in ResetObjective()
	//! because this is the method every RUNTIME "there is no objective now" path funnels through - the
	//! reset path and the idle path a failed re-selection takes. A bias left pointing at an abandoned
	//! objective would keep pulling the occupying faction's routine spending toward a place nothing is
	//! working on, silently and for the rest of the campaign. That funnel is intact and every runtime
	//! path still has to come through here.
	//!
	//! ⚠ CONSTRUCTION NO LONGER COMES THROUGH HERE, AND MUST NOT (2026-08-19). OnPostInit() calls
	//! ClearObjectiveRecordFields() directly instead, for two reasons that both point the same way: at
	//! construction there is definitionally no anchor to drop, because this component has only just
	//! been made and has never pushed one - and OnPostInit() ALSO RUNS IN THE WORLD EDITOR, where a
	//! world has been loaded but no game has started, so none of the managers the drop resolves exist.
	//! It took the editor down with a NULL faction manager. Construction was never a "there is no
	//! objective now" EVENT; it is the absence of one, which is a different statement.
	protected void ClearObjectiveRecord()
	{
		DropObjectiveAnchor();

		ClearObjectiveRecordFields();
	}

	//------------------------------------------------------------------------------------------------
	//! The record half of ClearObjectiveRecord(): every field back to "no objective", and no anchor
	//! work of any kind.
	//!
	//! ⚠ SPLIT OUT FOR EXACTLY ONE CALLER - OnPostInit(), for the reason in the note above - on the
	//! precedent ClearFOBRuntimeState() set. Nothing at RUNTIME may call it: a live path that cleared
	//! the record without dropping the bias is precisely the failure the funnel exists to prevent.
	protected void ClearObjectiveRecordFields()
	{
		m_Objective.kind = OVT_EObjectiveKind.NONE;
		m_Objective.position = vector.Zero;
		m_Objective.name = "";
		m_Objective.phase = OVT_EObjectivePhase.IDLE;
		m_Objective.phaseTicks = 0;
		m_Objective.nextOpTicks = 0;
		m_Objective.harassmentSuccesses = 0;
		m_Objective.sabotageSuccesses = 0;

		// ⚠ EVERY PER-OBJECTIVE LATCH IS DROPPED HERE rather than in ResetObjective(): every "there is no
		// objective now" path reaches this body through ClearObjectiveRecord(), and a latch left set would
		// silence its line for the NEXT objective as well.
		m_bDaylightWaitLogged = false;
		m_bCounterAttackRefusalLogged = false;
		m_bAffordabilityBlockLogged = false;

		// The idle clock's own state. The per-tick flag is dropped with the rest so a refusal seen on the
		// tick that ended an objective cannot hold the NEXT objective's first clock.
		m_bBlockedOnAffordability = false;
		m_iProgressHarassmentMark = 0;
		m_iProgressSabotageMark = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the forward-base record back to "no forward base", including the runtime state that is not
	//! in the save payload.
	//!
	//! ⚠ THE RUNTIME HALF MATTERS AS MUCH AS THE RECORD. m_bFOBDeploymentSent is what arms the spend
	//! ceiling and what stops a second supply party being sent, so a record cleared without it leaves
	//! the next objective's very first spend measured against a forward base that no longer exists.
	protected void ClearFOBRecord()
	{
		m_FOB.up = false;
		m_FOB.position = vector.Zero;
		m_FOB.sourceBasePosition = vector.Zero;
		m_FOB.spent = 0;
		m_FOB.starvationTicks = 0;
		m_FOB.deploymentName = "";

		ClearFOBRuntimeState();
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
	//! ⚠ IT COMPUTES NOTHING AND CHANGES NOTHING, and that is a hard requirement of its caller rather
	//! than a style preference (T8.8). OVT_GMSnapshotBuilder and the fan around it are strictly
	//! read-only: the trap that rule exists for is a "predict the next value" call that also APPLIES it,
	//! which would fire every time a Game Master opened the editor. Anything here that needed arithmetic
	//! would belong in a pure static, never in a getter.
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

	//! \return Which phase of the ramp the objective is in.
	OVT_EObjectivePhase GetPhase() { return m_Objective.phase; }

	//------------------------------------------------------------------------------------------------
	//! Whether everything the occupying faction controls is done and the counter-attack is only waiting
	//! on the world clock. Read-only; the decision is TickFOB()'s.
	//! \return True when the gate answers WAIT_FOR_DAYLIGHT.
	bool IsWaitingForCounterAttackDaylight()
	{
		return EvaluateCounterAttackGate() == COUNTER_ATTACK_WAIT_FOR_DAYLIGHT;
	}

	//! \return True when the counter-attack would be started by the next tick of this phase.
	bool IsCounterAttackReady()
	{
		return EvaluateCounterAttackGate() == COUNTER_ATTACK_FIRE;
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

	//! \return Harassment operations completed at the objective.
	int GetHarassmentSuccesses() { return m_Objective.harassmentSuccesses; }

	//! \return Sabotage missions completed at the objective.
	int GetSabotageSuccesses() { return m_Objective.sabotageSuccesses; }

	//! \return True when the forward operating base is standing.
	bool IsFOBUp() { return m_FOB.up; }

	//! \return Where the forward operating base stands.
	vector GetFOBPosition() { return m_FOB.position; }

	//! \return The base supplying the forward operating base.
	vector GetFOBSourceBasePosition() { return m_FOB.sourceBasePosition; }

	//! \return What has been spent from the pool at the forward operating base.
	int GetFOBSpent() { return m_FOB.spent; }

	//! \return Consecutive ticks the forward operating base has been cut off.
	int GetFOBStarvationTicks() { return m_FOB.starvationTicks; }

	//! \return Config name of the deployment carrying the forward operating base.
	string GetFOBDeploymentName() { return m_FOB.deploymentName; }

	//! \return Where the forward base's deployment was sent, whether or not the structure went up yet.
	//! The zero vector when none has been sent. Runtime-only and deliberately NOT persisted.
	vector GetFOBSite() { return m_vFOBSite; }

	//! \return Whether a supply party has been sent to raise the forward base. This is also what arms
	//! the spend ceiling - see IsFOBBudgetActive().
	bool IsFOBDeploymentSent() { return m_bFOBDeploymentSent; }

	//! \return Whether the forward base is currently reported as cut off.
	bool IsFOBStarving() { return m_bStarvationLogged; }

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
}
