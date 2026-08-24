//------------------------------------------------------------------------------------------------
//! THE FORWARD OPERATING BASE, AS ONE AUTHORED MODULE: site it, send the supply party that raises it,
//! keep the ceiling it spends against, take it down again, and answer whether a player may pull it
//! down.
//!
//! It is the whole of the hard-coded forward-base block moved verbatim - the siting lattice, the
//! authored-marker path, the exclusion list, the clearance trace, the send with its two housekeeping
//! branches and its affordability first claim, the spend ceiling, the teardown and the player-facing
//! dismantle rule. Nothing about WHERE a forward base goes or WHAT it costs changed; what changed is
//! that a plan says when to raise one instead of a phase handler being the only place it can happen.
//!
//! ⚠ IT IS AN ASSET MODULE, NOT A PLAIN OPERATION, AND THAT IS THE LIFETIME ARGUMENT. The base it
//! raises outlives the phase that raised it: it stands through the counter-attack and comes down when
//! the OBJECTIVE ends, from a tick where this phase is long over. See OVT_BaseObjectiveAssetModule's
//! header for the registration that makes the director able to reach it from there.
//!
//! ============================== THE ORDER IS THE CONTRACT ==============================
//! ⚠ THE AUTHORED ORDER OF A PHASE'S OPERATION MODULES IS THE EVALUATION ORDER, and .conf files
//! cannot carry comments, so the shipped forward-base phase's order is written down HERE and in
//! OVT_ObjectivePhase's header and nowhere else. Both shipped plans author it as:
//!
//!   1. RAISE THE FORWARD BASE (this module)   2. its garrison   3. tower recapture (TOWNS ONLY since
//!      2026-08-21 - a base objective no longer chases radio towers; see
//!      OVT_SendDeploymentObjectiveOperation's header)
//!   4. the harassment ladder                  5. sabotage
//!
//! which is the hard-coded forward-base spender's five-way `&&` chain, term for term and in the same
//! order. NOTHING ELSE IN THIS PHASE MEANS ANYTHING UNTIL THE FLAG IS UP - the garrison's own source
//! provider resolves to the forward base only once it is standing - and 3 to 5 are the ramp
//! continuing, which is what makes the counter-attack reachable at all (see below).
//!
//! 🔴 THE RAMP OPERATIONS ARE AUTHORED IN THIS PHASE TOO, AND OMITTING THEM IS A DEADLOCK. A base
//! objective is promoted to this phase on its FIRST completed sabotage mission and the counter-attack
//! gate demands up to six of them, so a promotion that stopped the ramp made the remaining five
//! unsendable and the battle unreachable. Towns deadlock identically: the stacking support debuff that
//! drives support under 25 % is applied by harassment operations. THE FIX HAS TWO HALVES and both are
//! authored data now - this phase's module bag repeats the ramp's operations, AND each ramp
//! deployment's OVT_ObjectiveConditionDeploymentModule spans Harassment through ForwardBase. Either
//! half alone leaves the ramp dead.
//! =======================================================================================
//!
//! 🔴 THE FORWARD BASE HAS FIRST CLAIM ON THE POOL, AND WITHOUT IT THE PHASE LIVELOCKS. When the base
//! is refused FOR MONEY this module CLAIMS THE INTERVAL (see the director's ClaimOperationInterval)
//! rather than letting a cheaper ramp operation spend the pool the base is saving toward. A play-test
//! (2026-08-20) watched a forward base refused at 120 against a pool of 56 while sabotage was bought
//! at 100 every time the pool passed it - forever. Only an AFFORDABILITY refusal claims; every other
//! reason falls through to the ramp exactly as before.
//!
//! ⚠ IT SPENDS NOTHING ITSELF (G5). It asks the director to create a deployment; the director creates
//! it and debits the ONE faction pool for it, in one place, once. The ceiling below is a COUNTER of
//! what has already left that pool - it holds no money, refunds nothing and moves nothing.
//!
//! ⚠ THE SITING IS DETERMINISTIC, WITH NO RANDOMNESS AT ALL. The whole feature exists so the
//! resistance can READ the occupying faction's intent; a forward base that lands somewhere different
//! every time the same campaign reaches the same state is the unpredictability being retired. It also
//! means a bad placement is a reproducible tuning question rather than a roll.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_RaiseForwardBaseObjectiveOperation : OVT_BaseObjectiveAssetModule
{
	//------------------------------------------------------------------------------------------------
	// ATTRIBUTES
	//------------------------------------------------------------------------------------------------

	[Attribute(defvalue: "Objective Forward Base", desc: "The registered deployment config that carries the structure and its free garrison, by its m_sDeploymentName. THE RE-LINK KEY: it is written into the save and matched back by string, so a name changed in overthrowDeployments.conf and not here stops the phase silently")]
	string m_sDeploymentConfigName;

	[Attribute(defvalue: "Objective Forward Base Garrison", desc: "The registered deployment config for the extra garrison sourced FROM the forward base. Read only by the teardown, which has to collect the garrison with the base it was bought for - the garrison is SENT by its own OVT_SendDeploymentObjectiveOperation")]
	string m_sGarrisonConfigName;

	[Attribute(defvalue: "-1", desc: "What one forward base and everything sourced from it may spend, before the ceiling multiplier. -1 = the campaign's objectiveFOBCost difficulty setting (400 on every shipped preset). ⚠ IT IS A CEILING, NOT A WALLET: nothing is reserved or held, it only counts what has already left the pool")]
	int m_iBudgetCost;

	[Attribute(defvalue: "0.35", desc: "Nearest the forward base may stand to the objective, as a fraction of the way back to the base supplying it. A third of the way out: any closer and it is inside the objective's own defended ground, where it would be found in the first minute and could not be supplied")]
	float m_fBandMinFraction;

	[Attribute(defvalue: "0.75", desc: "Furthest the forward base may stand from the objective, as the same fraction. Three quarters of the way out, so the base is meaningfully FORWARD of the rear rather than a second flag beside the one the faction already has")]
	float m_fBandMaxFraction;

	[Attribute(defvalue: "350", desc: "Absolute floor on the standoff from the objective, in metres, whatever the fractions work out to. A short supply line would otherwise put a forward base 80 m outside a town")]
	float m_fMinStandoff;

	[Attribute(defvalue: "2500", desc: "Absolute ceiling on the standoff, in metres. Past this the base is not supporting the objective in any sense a player could read, and the counter-attack's waves would come from nowhere near it. ALSO the radius the teardown and the re-send search for an existing forward-base deployment in, so the two can never disagree about which one belongs to this objective")]
	float m_fMaxStandoff;

	[Attribute(defvalue: "8", desc: "Steps sampled along the supply line. Eight spreads the samples about 40 m apart over a typical band, which is finer than the clearance box the trace tests with. ⚠ THE LATTICE IS steps x lanes and every raise walks ALL of it")]
	int m_iSitingSteps;

	[Attribute(defvalue: "5", desc: "Lateral lanes sampled at each step: on the line, then evenly spaced out to the spread either side of it. A site straight down the supply road is the shortest resupply and is lane 0 so it is tried first. ⚠ THIS IS THE KNOB THAT COSTS CANDIDATES, and it costs one whole column of steps apiece. Raised from 3 to 5 by the author on 2026-08-19 - 'some more room to choose a spot'")]
	int m_iSitingLanes;

	[Attribute(defvalue: "400", desc: "How far each lateral lane pushes a sample off the supply line, in metres. A MAXIMUM, not a per-lane step: widening it relocates the outer lanes rather than sampling anywhere new. Widened from 250 by the author on 2026-08-19. ⚠ It also bounds the corridor an AUTHORED marker has to sit inside")]
	float m_fLateralSpread;

	//------------------------------------------------------------------------------------------------
	// CONSTANTS
	//
	// ⚠ THE GENERATED PATH IS THE PRIMARY PATH AND IS TUNED AS IF THE AUTHORED ONE WILL NEVER EXIST
	// (R17). No OVT_FOBPosition marker is placed in any shipped world today - putting them there is a
	// Workbench world-editing job nobody has done - so every constant below is what actually decides
	// where forward bases land. Authored markers are an optimisation a map author may apply on top,
	// and they win when they exist.
	//
	// ⚠ THESE ARE CONSTANTS AND NOT ATTRIBUTES, DELIBERATELY. They are the tuned output of a play-test
	// rather than a tuning surface, and the requirements put the siting maths out of scope.
	//------------------------------------------------------------------------------------------------

	//! Log prefix. Matches the director's, so the forward base's lines read as part of the same system.
	static const string LOG = "[Overthrow.ObjectiveDirector] ";

	//! Radius of the four ground probes that judge how level a candidate is, in metres. Slightly wider
	//! than the structure's own footprint so a site straddling the lip of a ditch is caught.
	static const float FLATNESS_PROBE_RADIUS = 8;

	//! Height spread across those probes at which a candidate is rejected outright, in metres. Two and
	//! a half metres over a sixteen-metre span is about a 1-in-6 slope; a structure on more than that
	//! floats at one corner and sinks at another, which is the one siting failure everybody can see.
	static const float FLATNESS_TOLERANCE = 2.5;

	//! Height advantage over the objective at which the elevation preference saturates, in metres.
	static const float ELEVATION_USEFUL_GAIN = 30;

	//! Half-width of the clearance box traced at each candidate, in metres.
	static const float CLEAR_BOX_HALF = 6;

	//! Height of that box, in metres.
	static const float CLEAR_BOX_HEIGHT = 8;

	//! Nothing may be built this close to a base of ANY faction, in metres. Matches the radius the
	//! placement limit already treats as "belonging to a base" (OVT_ItemLimitChecker's own figure for
	//! EOVTBaseType.BASE), so a forward base is never inside ground another system considers spoken for.
	static const float CLEARANCE_BASE = 500;

	//! Nothing may be built this close to a resistance forward base or camp, in metres. Tighter than a
	//! military base because a camp is a small thing, and generous enough that the occupying faction
	//! never plants a flag inside a player's own site.
	static const float CLEARANCE_RESISTANCE_SITE = 300;

	//! Added to a resistance-held town's own range to get its exclusion radius, in metres. The town
	//! range is where the campaign already stops counting a place as "in the town"; the margin keeps a
	//! forward base out of the fields immediately outside it, where it would be as visible as inside.
	static const float CLEARANCE_TOWN_MARGIN = 150;

	//! How close to a real base a BASE objective's position has to be before the record of who supplies
	//! the forward base is believed, in metres. Shares the director's own figure for the same question.
	static const float BASE_MATCH_RADIUS = 100;

	//! How far from the recorded forward-base position a deployment or a structure counts as belonging
	//! to it. Used by the teardown sweep, by the garrison cap authored in the plan and by the starvation
	//! count in OVT_AssetStarvedObjectiveAbort, so all three agree about what "at the forward base"
	//! means. ⚠ THE OTHER TWO AUTHOR OR DEFAULT TO THE SAME NUMBER RATHER THAN READING THIS ONE - a
	//! .conf cannot reference a constant - so changing it means changing three places.
	static const float AREA_RADIUS = 250;

	//! How far from the recorded position the teardown looks for the structure itself, in metres.
	//! Tight: the structure is spawned AT that position, so anything further away is something else.
	static const float STRUCTURE_SEARCH_RADIUS = 60;

	//! How close a player has to be to the flag to dismantle a forward base, in metres. The action is
	//! performed on the flag from interaction range; this is the SERVER's re-derivation of that, and it
	//! is deliberately generous because the flag's origin is not where the player is standing.
	//!
	//! 🔴 A CONSTANT AND NOT AN ATTRIBUTE, AND THAT IS LOAD-BEARING RATHER THAN LAZY. The dismantle rule
	//! is asked TWICE - by the user action on the CLIENT, where no objective, no plan and no module set
	//! exist, and by the request handler on the server. An authored value would be visible to one of
	//! them and not the other, so the prompt a player reads and the rule the server enforces would
	//! disagree, which is exactly the "the action was available and did nothing" failure the shared body
	//! exists to prevent.
	static const float DISMANTLE_RANGE = 30;

	//! No occupying-faction soldier may be alive within this many metres of the flag while a player
	//! dismantles it. The forward base has to be CLEARED, not merely reached - otherwise the fifteen
	//! second hold is something a player does while being shot at, which reads as a bug rather than a
	//! challenge. Same client/server argument as the range above: a constant on purpose.
	static const float DEFENDER_CLEAR_RADIUS = 150;

	//------------------------------------------------------------------------------------------------
	// RUNTIME STATE
	//
	// ⚠ NONE OF IT IS PERSISTED, AND THAT IS DELIBERATE. A save taken while the supply truck was still
	// on the road comes back with no truck, no convoy and a deployment that may raise nothing (a
	// restored deployment never raises - see OVT_FOBRaiseSpawningDeploymentModule.DecideRaise), so the
	// honest restored state is "no forward base was sent" and the phase sends another one after
	// collecting the stranded marker. A restored objective gets a FRESH clone of this module, which is
	// exactly that state.
	//------------------------------------------------------------------------------------------------

	//! True once the forward base's own deployment has been created for this objective, whether or not
	//! the structure is standing yet. IT IS ALSO WHAT ARMS THE SPEND CEILING.
	protected bool m_bDeploymentSent;

	//! Where the forward base's deployment was created. Held separately from the record's position,
	//! which is only written once the structure is actually standing, so the teardown can still find a
	//! marker whose raise never completed.
	protected vector m_vSite;

	//! Curated forward-base markers found by the in-flight siting query. Rebuilt at the start of every
	//! search and dropped at the end of it, in the OVT_SniperMarkerPlacementProvider shape: a marker can
	//! be deleted with whatever it was placed on, so nothing here is cached across calls.
	protected ref array<IEntity> m_aFoundMarkers;

	//! Forward-base structures found by the in-flight teardown query. Same rule.
	protected ref array<IEntity> m_aFoundStructures;

	//! What the teardown query is matching on. Held in a member because the engine's filter callback
	//! takes only the candidate entity, so a per-candidate config resolve is the alternative.
	protected ResourceName m_sStructurePrefab;

	//------------------------------------------------------------------------------------------------
	// LIFECYCLE
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Registers as the asset's owner, and adopts a base that is ALREADY standing.
	//!
	//! ⚠ THE ADOPTION IS THE RESTORE PATH AND IS NOT DEFENSIVE PADDING. A restored objective rebuilds
	//! its phase's module set from the plan, so the module that reaches this method may be looking at a
	//! forward base that a previous session raised: the record says up, the structure is standing, and
	//! the deployment is about to be re-linked by the director's first tick. Reading m_bDeploymentSent
	//! as false there would leave the ceiling disarmed and the teardown with nothing to take down.
	override protected void OnEnter()
	{
		RegisterAsAssetOwner();

		if (m_Asset && m_Asset.up)
		{
			m_bDeploymentSent = true;
			m_vSite = m_Asset.position;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! ⚠ IT DOES NOT TEAR THE BASE DOWN, AND MUST NOT. Leaving this phase for the counter-attack is not
	//! the end of the forward base - it is the moment it matters most. The teardown is TearDownAsset(),
	//! called only from the objective's one reset path.
	override protected void OnExit()
	{
	}

	//------------------------------------------------------------------------------------------------
	// THE SPEND CEILING
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether the forward base's ceiling governs the director's spending right now.
	//!
	//! ⚠ THE CEILING IS NOT A WALLET AND NOTHING HERE HOLDS MONEY. The record's spend counter records
	//! what has ALREADY left the one deployment pool on this forward base and everything sourced from
	//! it; nothing is reserved, held or moved by any of it. If you are reading this because it looks
	//! like a budget that should be topped up, refunded or carried over - it is not one, and turning it
	//! into one breaks the conserved-total identity the base-defense migration established (G5).
	//!
	//! It is inactive during harassment, because this module is not in that phase's module set, so the
	//! ramp spends against the pool alone exactly as it did before the forward base existed. It arms the
	//! moment the forward base's own deployment is SENT so that the structure's own cost is inside the
	//! budget, and it disarms when the objective's record is cleared.
	//! \return True while spending is counted against the ceiling.
	override bool IsCeilingArmed()
	{
		if (m_Asset && m_Asset.up)
			return true;

		return m_bDeploymentSent;
	}

	//------------------------------------------------------------------------------------------------
	//! ⚠ THE CEILING MUST BE ABLE TO COVER THE FORWARD BASE ITSELF, and that is an authored-data
	//! invariant rather than something this method can enforce: the ceiling is the budget cost times
	//! OVT_ObjectivePhaseRules.FOB_CEILING_MULTIPLIER while the base's own price is the deployment
	//! config's total resource cost, and the two are authored in different files. Misauthored the wrong
	//! way round, the very first spend of the phase is refused and the phase can never make progress -
	//! an initialisation case pins it across all five shipped presets for exactly that reason.
	//! \return The ceiling, from OVT_ObjectivePhaseRules.
	override int GetCeiling()
	{
		return OVT_ObjectivePhaseRules.FOBBudgetCeiling(ResolveBudgetCost());
	}

	//------------------------------------------------------------------------------------------------
	//! The budget cost, authored or from difficulty.
	//! \return The cost one forward base is allowed to be worth. Non-positive yields a zero ceiling,
	//!         which refuses everything.
	int ResolveBudgetCost()
	{
		int difficultyValue = -1;

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (difficulty)
			difficultyValue = difficulty.objectiveFOBCost;

		return OVT_ObjectivePlanRules.ResolveWithDifficulty(m_iBudgetCost, difficultyValue);
	}

	//------------------------------------------------------------------------------------------------
	// THE OPERATION
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Sends the supply truck that raises the forward operating base, once per objective.
	//!
	//! ⚠ IT ARMS THE CEILING BEFORE IT ASKS WHETHER IT MAY BUY ANYTHING, AND DISARMS IT AGAIN ON EVERY
	//! FAILURE EXIT. m_bDeploymentSent is what makes IsCeilingArmed() true, and the ceiling has to
	//! already be active when the forward base's OWN cost is checked and counted - the budget covers
	//! "the structure itself". A refusal must leave the flag down, or the phase would believe a base was
	//! on its way and never send another. There are FOUR such exits below (the pre-flight, no source
	//! base, no site, the create itself) and every one of them clears it.
	//!
	//! 🔴 THE PRE-FLIGHT COMES BEFORE THE SITING, AND THAT ORDER IS A BUG FIX (2026-08-19). It used to
	//! site first: a play-test with twenty resources in the pool ran the full siting lattice - an ocean
	//! read, a TraceBox and five surface samples each - resolved the same deterministic site, printed
	//! the same "sited at" line, and only then discovered it could not afford the base. Every ten
	//! seconds. Indefinitely, because the affordability hold means the phase never times out. Asking the
	//! cheap question first makes a poverty spell in this phase cost exactly what one in the harassment
	//! phase costs: one map lookup and nothing else.
	//!
	//! 🔴 TWO OF ITS REFUSALS CLAIM THE INTERVAL, AND BOTH CLAIMS ARE PLAY-TEST FIXES.
	//!   AFFORDABILITY - the forward base has first claim on the pool. Without it the faction buys a
	//!     cheaper ramp operation every time the pool passes ITS price and never reaches the base's, and
	//!     the reserve floor names the wrong operation because the last refusal in the walk overwrites
	//!     the first. Returning without letting the walk continue leaves the FIRST refusal's floor
	//!     standing and spends nothing below it.
	//!   HOUSEKEEPING - a tick spent CLEARING THE WAY for a base (collecting a stale marker, or noticing
	//!     the supply party vanished) must not hand its interval to a sabotage mission and push the base
	//!     a whole cadence away. It is a SINGLE tick of bookkeeping and the very next tick has a clear
	//!     field to site on, which is why it is a per-tick claim and not a latch.
	//! Every other reason this returns false - no source base, no site, a supply party already on the
	//! road, the base already standing - falls through to the garrison and the ramp exactly as before.
	//! \return True when a deployment was created and paid for.
	//------------------------------------------------------------------------------------------------
	//! 🔴 THE ONE OPERATION IN THE TREE THAT REFUSES TO BE SHUFFLED, AND IT IS ABOUT MONEY.
	//!
	//! The director draws a random order for a phase's operations every cadence, so that a player cannot
	//! learn the sequence (OVT_ObjectiveDirectorComponent.BuildOperationOrder). This module opts out,
	//! and the reason is the interval claim below rather than any notion of importance.
	//!
	//! ClaimOperationInterval() works by STOPPING THE WALK, so it can only ever protect operations that
	//! have not been asked yet. Drawn last, this module's claim protects nothing: the play-tested
	//! 2026-08-20 livelock is that a forward base costing 120 is never affordable because sabotage at
	//! 100 keeps being asked first and spending the pool. Pinning it ahead of the draw is what keeps the
	//! reserve it saves for itself.
	//!
	//! ⚠ AND IT COSTS THE UNPREDICTABILITY NOTHING. Once the base is standing, TryAct() answers false on
	//! its `m_Asset.up` line - so from then on the pinned head is a single cast per cadence and the
	//! garrison, harassment and sabotage behind it draw freely against one another, which is exactly the
	//! variety that was asked for.
	//! \return False - never shuffled.
	override bool ShufflesFreely()
	{
		return false;
	}

	override bool TryAct()
	{
		OVT_ObjectiveInstance objective = GetObjective();
		if (!objective || !objective.IsLive())
			return false;

		OVT_ObjectiveDirectorComponent director = GetDirector();
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!director || !deployments || !config || !m_Asset)
			return false;

		if (m_Asset.up)
			return false;

		int occupyingIndex = config.GetOccupyingFactionIndex();
		if (occupyingIndex < 0)
			return false;

		vector target = objective.GetTargetPosition();

		OVT_DeploymentComponent existing = FindLiveDeployment(deployments, target);

		if (m_bDeploymentSent)
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
			Print(LOG + "The supply party sent to raise a forward base for objective '" + objective.GetTargetName() + "' is gone before it could build - siting another", LogLevel.WARNING);
			m_bDeploymentSent = false;
			m_vSite = vector.Zero;

			director.ClaimOperationInterval();
			return false;
		}

		// A marker left over from a save taken mid-drive, or from an objective whose teardown could not
		// reach it. It can never raise anything (a restored deployment's raise is gated), so leaving it
		// standing would cost this objective its whole phase.
		if (existing)
		{
			Print(LOG + "A forward-base deployment from a previous session is standing near objective '" + objective.GetTargetName() + "' and can never raise anything - collecting it and re-siting", LogLevel.WARNING);
			deployments.DeleteDeployment(existing);

			director.ClaimOperationInterval();
			return false;
		}

		// THE CEILING IS ARMED FROM HERE TO THE END OF THE METHOD. See the header.
		m_bDeploymentSent = true;

		// 🔴 THE CHEAP QUESTION FIRST. Nothing below this line is worth doing if the faction cannot pay
		// for the base, and everything below it is expensive, noisy or both.
		if (!director.CanAffordObjectiveDeployment(deployments, m_sDeploymentConfigName, occupyingIndex))
		{
			m_bDeploymentSent = false;

			// ⚠ ONLY AN AFFORDABILITY REFUSAL CLAIMS THE INTERVAL. An unregistered config name is a
			// fault to be fixed and a spent ceiling is a decision the machine made about itself; neither
			// is a reason to stop the ramp, and a phase that can only ever hit one of those SHOULD run
			// its idle clock down and be abandoned.
			if (director.IsBlockedOnAffordability())
				director.ClaimOperationInterval();

			return false;
		}

		vector source;
		if (!ResolveSourceBase(target, occupyingIndex, source))
		{
			m_bDeploymentSent = false;

			// Latched with the rest: it is re-asked every in-game minute and stays true until the faction
			// takes a base back, which the campaign log will say on its own.
			director.LogObjectiveRefusal(m_sDeploymentConfigName, OVT_ObjectiveDirectorComponent.REFUSAL_NO_SOURCE_BASE, "there is no supply line to site one along", LogLevel.WARNING);
			return false;
		}

		vector site;
		float siteYaw;
		if (!ResolveSite(source, target, objective.GetTargetName(), site, siteYaw))
		{
			// ⚠ NOT A RETRY. The band, the exclusions and the terrain do not change from one in-game
			// minute to the next, so an objective with nowhere to put a forward base has nowhere to put
			// one for as long as it is the objective. It sits out a selection round and something else
			// gets picked.
			//
			// ⚠ THE FLAG IS DROPPED BEFORE THE RESET, not after: the teardown reads it to decide whether
			// there is anything to sweep, and nothing was ever sent.
			m_bDeploymentSent = false;

			Print(LOG + "Objective '" + objective.GetTargetName() + "' has nowhere to put a forward base: " + SitingAttempts().ToString() + " generated candidate(s) and every authored site in the band were rejected. Abandoning it for one selection round", LogLevel.WARNING);
			director.ResetObjective("no forward-base site could be found anywhere in its band", true);
			return false;
		}

		// ⚠ THE FACING GOES ON THE DEPLOYMENT MARKER, WHICH IS HOW IT REACHES THE RAISE. The raise module
		// already takes its position from its parent deployment; taking the heading from the same object
		// is the one arrangement in which the two can never disagree, and it needs no lookup back into
		// the director from inside a deployment module.
		if (!director.CreateObjectiveDeployment(deployments, m_sDeploymentConfigName, site, occupyingIndex, siteYaw))
		{
			m_bDeploymentSent = false;
			return false;
		}

		m_vSite = site;

		// 🔴 RECORDED FROM THE SITE, NOT FROM THE OBJECTIVE, SO THE RECORD MATCHES WHO ACTUALLY SUPPLIES
		// IT (2026-08-20).
		//
		// `source` above is the nearest controlled base TO THE OBJECTIVE, and it is the right input for
		// the corridor - it is what decides which way the supply line runs before any site exists. It is
		// the wrong thing to REMEMBER. What actually drives the supply party is the insertion module's
		// own provider, and that resolves the nearest controlled base to the DEPLOYMENT, i.e. to the
		// site. Those two are allowed to differ, and before the corridor gate they routinely did: a
		// play-test found a forward base whose record named a base 1.2 km further from it than the one
		// its convoy really came from.
		//
		// ⚠ WHY IT MATTERS RATHER THAN BEING BOOKKEEPING: the starvation rule reads this record to ask
		// "has the base supplying it been taken?". Named wrongly, the forward base starves when the
		// player takes a base that was supplying nothing, and survives when they take the one that
		// really was.
		vector supplyBase;
		if (ResolveSourceBase(site, occupyingIndex, supplyBase))
			m_Asset.sourceBasePosition = supplyBase;
		else
			m_Asset.sourceBasePosition = source;

		OVT_DeploymentLog.Debug(LOG + "Objective '" + objective.GetTargetName() + "': a supply party is on its way to raise a forward base at " + site.ToString());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The forward base's own deployment, wherever this objective might have put one.
	//!
	//! SEARCHED FROM THE OBJECTIVE RATHER THAN FROM A REMEMBERED SITE, because the one caller that
	//! needs it most - a restored campaign looking for a marker it has no runtime record of - has no
	//! remembered site to search from. The maximum standoff is the furthest the sampler could ever have
	//! put one, so this covers the whole band and nothing beyond it.
	//! \param[in] deployments The deployment framework.
	//! \param[in] target Where the objective is.
	//! \return The deployment, or null when there is none.
	protected OVT_DeploymentComponent FindLiveDeployment(notnull OVT_DeploymentManagerComponent deployments, vector target)
	{
		return deployments.GetDeploymentNearPosition(m_sDeploymentConfigName, target, m_fMaxStandoff);
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest base the faction holds to ANY position.
	//!
	//! ⚠ TWO CALLERS ASKING THE SAME QUESTION ABOUT DIFFERENT PLACES, which is exactly why it is one
	//! method. The siting asks it about the OBJECTIVE, to decide which way the supply line runs before a
	//! site exists; the record asks it again about the chosen SITE, to remember who will really be
	//! supplying the base once it is standing. Written twice, those two would be free to drift into
	//! answering subtly different questions - which is the class of defect that put a forward base's
	//! recorded supply base 1.2 km from its actual one.
	//!
	//! ⚠ IT IS THE SAME WALK OVT_NearestControlledBaseSourceProvider MAKES, deliberately, because the
	//! second caller's whole purpose is to predict what that provider will answer for the deployment it
	//! is about to create. The provider cannot be called directly here - it resolves against a live
	//! deployment position and none exists yet - so the agreement is kept by both walking the faction's
	//! OWN base list and taking the nearest, rather than by nearest-then-check-if-friendly, which is the
	//! subtly different question that broke the insertion module's copy of this.
	//! \param[in] position What "nearest" is measured to.
	//! \param[in] factionIndex The faction that must control the base.
	//! \param[out] source The nearest controlled base's position; zero when there is none.
	//! \return False when the faction holds no base at all.
	protected bool ResolveSourceBase(vector position, int factionIndex, out vector source)
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

			float distance = vector.Distance(base.location, position);

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

	//! ⚠ THE ATTEMPT COUNT IS THE PRODUCT OF THE TWO AUTHORED KNOBS RATHER THAN A NUMBER SOMEBODY LIKED.
	//! The sampler walks a deterministic lattice and this is simply how many points are in it.
	//!
	//! 🔴 IT IS A BUDGET, NOT A BOUND. The search does NOT return on the first candidate that passes: it
	//! evaluates EVERY point and keeps the highest-scoring one, which is the whole reason a site is
	//! chosen by flatness, elevation and road proximity rather than by lattice order - and the reason
	//! the log line quotes a score. Every raise pays for all of these, so the number is a real cost.
	//!
	//! WHAT ONE ATTEMPT ACTUALLY COSTS, cheapest test first, because most candidates never reach the
	//! expensive half: a band test (arithmetic), an ocean read, and one loop over the exclusion list -
	//! and only then, for a survivor, five surface probes, a TraceBox and a nearest-road query.
	//!
	//! ⚠ AND IT RUNS ONCE PER OBJECTIVE, WHICH IT DID NOT BEFORE. Until the 2026-08-19 affordability fix
	//! this whole lattice ran on EVERY tick of a forward-base phase that could not pay - six times a real
	//! minute, indefinitely. The pre-flight in TryAct() refuses before the search, so the points are
	//! walked once, on the tick that actually raises the base.
	//! \return Steps x lanes.
	int SitingAttempts()
	{
		int steps = m_iSitingSteps;
		if (steps < 1)
			steps = 1;

		int lanes = m_iSitingLanes;
		if (lanes < 1)
			lanes = 1;

		return steps * lanes;
	}

	//------------------------------------------------------------------------------------------------
	//! Picks the site for the forward operating base, or answers that there is nowhere to put one.
	//!
	//! TWO SOURCES OF CANDIDATES, AND THE AUTHORED ONE WINS WHENEVER IT PRODUCES ANYTHING:
	//!  1. GENERATED - a bounded, deterministic lattice of SitingAttempts() points on the supply line
	//!     and either side of it, each ocean-rejected, clearance-traced, flatness-probed and scored.
	//!     THIS IS THE PRIMARY PATH and it is built and tuned as if the other will never exist, because
	//!     today it does not: no OVT_FOBPosition marker is placed in any shipped world (R17).
	//!  2. AUTHORED - OVT_FOBPositionComponent markers inside the band, subject to the same exclusions
	//!     and the same clearance test. A map author who has walked the terrain knows better than a
	//!     lattice, so ANY qualifying marker beats the best generated point.
	//!
	//! ⚠ A SITE IS A POSITION AND A FACING, AND THE FACING TRAVELS WITH IT FROM HERE TO THE SPAWN
	//! TRANSFORM. It did not until 2026-08-19: nothing in this chain read a heading, the raise spawned
	//! with an identity rotation, and every forward base in the campaign - authored or generated - stood
	//! unrotated. A map author's OVT_FOBPosition arrow was simply not consulted. The two branches answer
	//! the heading differently and both answers are deliberate; see each of them.
	//! \param[in] source Where the supply line starts - the nearest base the faction holds.
	//! \param[in] objective Where it is going.
	//! \param[in] label The objective's display name, for the log line.
	//! \param[out] site The chosen position, written only when this returns true.
	//! \param[out] yaw Which way the structure faces there, in the Math3D.AnglesToMatrix frame.
	//! \return False when nothing in the band qualifies.
	protected bool ResolveSite(vector source, vector objective, string label, out vector site, out float yaw)
	{
		site = vector.Zero;
		yaw = OVT_FOBSiting.NO_FACING;

		array<vector> exclusions = new array<vector>();
		array<float> radii = new array<float>();
		CollectExclusions(exclusions, radii);

		float bestScore = 0;
		bool found = SampleGeneratedSite(source, objective, exclusions, radii, site, bestScore, yaw);

		vector authored;
		float authoredScore = 0;
		float authoredYaw = OVT_FOBSiting.NO_FACING;
		if (FindAuthoredSite(source, objective, exclusions, radii, authored, authoredScore, authoredYaw))
		{
			// ⚠ THE FACING IS TAKEN OVER WITH THE POSITION, IN THE SAME BREATH. Keeping the generated
			// heading here would point an authored base wherever the sampler's best guess happened to
			// look, which is the one thing a marker exists to override.
			site = authored;
			yaw = authoredYaw;

			// Rounded through an int the way every other bearing and distance in this feature is:
			// Math.Round answers a float, and a float's ToString puts six decimal places in the log line.
			int authoredFacing = Math.Round(yaw);

			OVT_DeploymentLog.Debug(LOG + "Forward base for objective '" + label + "' will use an authored site at " + authored.ToString() + " facing " + authoredFacing.ToString() + " deg (the marker's own)");

			return true;
		}

		if (found)
		{
			int generatedFacing = Math.Round(yaw);

			OVT_DeploymentLog.Debug(LOG + "Forward base for objective '" + label + "' sited at " + site.ToString() + " facing " + generatedFacing.ToString() + " deg towards the objective (generated, score " + bestScore.ToString() + ")");
		}

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
	protected void CollectExclusions(notnull array<vector> exclusions, notnull array<float> radii)
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (occupying && occupying.m_Bases)
		{
			foreach (OVT_BaseData base : occupying.m_Bases)
			{
				if (!base)
					continue;

				exclusions.Insert(base.location);
				radii.Insert(CLEARANCE_BASE);
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
					radii.Insert(CLEARANCE_RESISTANCE_SITE);
				}
			}

			if (resistance.m_Camps)
			{
				foreach (OVT_CampData camp : resistance.m_Camps)
				{
					if (!camp)
						continue;

					exclusions.Insert(camp.location);
					radii.Insert(CLEARANCE_RESISTANCE_SITE);
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
				radii.Insert(towns.GetTownRange(town) + CLEARANCE_TOWN_MARGIN);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Walks the deterministic lattice and keeps the best-scoring point that passes every hard test.
	//!
	//! ⚠ IT DOES NOT RETURN ON THE FIRST HIT. An early return would make the sampler prefer whatever
	//! happens to be nearest the supply base regardless of how bad it is, and the score exists precisely
	//! to order the survivors. The bound is what keeps the cost fixed - see SitingAttempts().
	//!
	//! ⚠ A GENERATED SITE FACES ITS OBJECTIVE, AND THAT IS A DECISION RATHER THAN A DEFAULT. There is no
	//! author to ask, so the only readable answer is the one the base exists for: a forward base looking
	//! at the thing it was sent to advance on puts the shipped prefab's hedgehogs and wire between the
	//! flag and the town, and a player who finds it can tell at a glance which way it is pointed. It is
	//! also DERIVED FROM TWO POSITIONS, so it is as deterministic as the site itself. The alternative
	//! that was NOT chosen is "face back along the supply line": the same axis in most bands, but
	//! reversed, and it would put the base's back to the fight in every one of them.
	//! \param[in] source Where the supply line starts.
	//! \param[in] objective Where it is going.
	//! \param[in] exclusions Places to stay clear of.
	//! \param[in] radii How far from each, same order.
	//! \param[out] best The best candidate found.
	//! \param[out] bestScore Its score.
	//! \param[out] bestYaw The heading that faces the objective from `best`.
	//! \return False when no candidate passed.
	protected bool SampleGeneratedSite(vector source, vector objective, notnull array<vector> exclusions, notnull array<float> radii, out vector best, out float bestScore, out float bestYaw)
	{
		best = vector.Zero;
		bestScore = 0;
		bestYaw = OVT_FOBSiting.NO_FACING;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		float bandMin;
		float bandMax;
		if (!ResolveBand(source, objective, bandMin, bandMax))
			return false;

		// A degenerate band - the supplying base is almost on top of the objective, or the floor has
		// swallowed the whole span - refuses everything, by OVT_FOBSiting.IsInBand's own rule.
		vector toObjective = objective - source;
		toObjective[1] = 0;

		if (toObjective.Length() <= 0)
			return false;

		vector along = toObjective.Normalized();
		vector lateral = Vector(-along[2], 0, along[0]);

		int lanes = m_iSitingLanes;
		if (lanes < 1)
			lanes = 1;

		int steps = m_iSitingSteps;
		if (steps < 1)
			steps = 1;

		bool found = false;
		int attempts = steps * lanes;

		for (int attempt = 0; attempt < attempts; attempt++)
		{
			int step = attempt / lanes;
			int lane = attempt % lanes;

			// Measured back from the OBJECTIVE, so the fraction is "how far out from the target", which
			// is what the band is expressed in.
			float standoff = bandMax - (bandMax - bandMin) * OVT_FOBSiting.BandFraction(step, steps);

			vector candidate = objective - along * standoff;
			candidate = candidate + lateral * (OVT_FOBSiting.LateralOffset(lane, lanes) * m_fLateralSpread);

			vector accepted;
			float score;
			if (!EvaluateCandidate(world, candidate, objective, bandMin, bandMax, exclusions, radii, accepted, score))
				continue;

			if (found && score <= bestScore)
				continue;

			found = true;
			bestScore = score;
			best = accepted;

			// Measured from the ACCEPTED point rather than from the raw lattice candidate. They differ
			// only in height today, which a flat heading discards - but the accepted point is the one the
			// structure is actually put on, and deriving the facing from anything else is how the two
			// would quietly drift apart if the clamp ever moved a candidate sideways.
			bestYaw = OVT_FOBSiting.FacingYaw(accepted, objective);
		}

		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! The band the forward base has to stand in, from the supply line's own length.
	//! \param[in] source Where the supply line starts.
	//! \param[in] objective Where it is going.
	//! \param[out] bandMin Nearest the base may stand to the objective.
	//! \param[out] bandMax Furthest it may stand from it.
	//! \return False when the two positions coincide, which is not a supply line.
	protected bool ResolveBand(vector source, vector objective, out float bandMin, out float bandMax)
	{
		bandMin = 0;
		bandMax = 0;

		float separation = vector.Distance(source, objective);
		if (separation <= 0)
			return false;

		bandMin = separation * m_fBandMinFraction;
		bandMax = separation * m_fBandMaxFraction;

		if (bandMin < m_fMinStandoff)
			bandMin = m_fMinStandoff;

		if (bandMax > m_fMaxStandoff)
			bandMax = m_fMaxStandoff;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One curated marker in the corridor that still qualifies, chosen AT RANDOM from all of them.
	//!
	//! ==========================================================================================
	//! 🔴 SCORE DOES NOT DECIDE BETWEEN AUTHORED SITES, AND FROM 2026-08-21 IT MUST NOT.
	//! ==========================================================================================
	//! This used to keep a running best and take the highest terrain score, which made the whole siting
	//! path strictly deterministic: the same source, the same objective and the same terrain always
	//! produced the same winner, so an area with three or four authored markers used exactly one of them
	//! for the entire campaign and the other markers were decoration (user observation, 2026-08-21).
	//!
	//! THE ASYMMETRY WITH THE GENERATED LATTICE IS DELIBERATE AND IS THE WHOLE IDEA. A generated
	//! candidate is a GUESS - the sampler invents points along the supply line and scores them because it
	//! has no other way to tell a good one from a bad one, so the best guess should win and
	//! SampleGeneratedSite() stays deterministic and score-driven. An authored marker is a HUMAN'S
	//! STATED INTENT: somebody stood in the Eden editor, looked at the ground, and said "a forward base
	//! belongs here, facing that way". Once such a marker has passed every hard test there is nothing
	//! left for a terrain score to add - all the surviving markers are, by construction, places the
	//! author already approved. Ranking them by flatness only picks the flattest of several equally
	//! intended answers, forever.
	//!
	//! ⚠ THE FILTERS ARE UNCHANGED AND EVERY ONE OF THEM IS STILL HARD. Randomness applies ONLY to the
	//! choice between survivors: the corridor test, the band, the ocean read, the exclusions and the
	//! clearance trace all still reject outright, and a marker that fails any of them is not in the
	//! draw. "Pick a random one" means a random ELIGIBLE one, never a random marker.
	//!
	//! ⚠ TWO PASSES, NOT A TWEAKED COMPARISON, AND IT CANNOT BE DONE ANY OTHER WAY. A uniform choice
	//! needs to know how many candidates there are, which a streaming running-best does not - reservoir
	//! sampling would work but is a much harder thing to read and to prove for no gain at these counts.
	//! Collecting first also keeps each accepted POSITION with its own marker's YAW, which the old loop
	//! only got right by accident of assigning both in the same breath; a two-pass version that kept one
	//! running yaw would hand the winning site the LAST marker's heading.
	//!
	//! Every curated marker in the band that still qualifies is eligible, and one is drawn.
	//!
	//! ⚠ A MARKER IS NOT EXEMPT FROM THE HARD TESTS. It still has to be in the band, out of the ocean,
	//! clear of every exclusion and clear of obstructions - the world moves under a marker placed months
	//! ago in the editor, and the resistance can build a camp on top of one. What a marker buys is
	//! PRIORITY over anything generated, not a bypass.
	//!
	//! ⚠ AN AUTHORED SITE USES THE MARKER'S OWN FACING, WHICH IS THE HALF OF A MARKER THAT WAS BEING
	//! THROWN AWAY. OVT_FOBPosition draws a Workbench arrow along its transform[2] precisely so an author
	//! can aim the base, and until 2026-08-19 nothing read it - the arrow was decoration. It is read as
	//! GetYawPitchRoll()[0] and NOT as GetAngles()[0]: the two engine angle APIs use different orders
	//! (see OVT_BaseSpawningDeploymentModule.GetUprightSpawnRotation), GetAngles() puts PITCH in slot 0,
	//! and the shipped Eden marker is authored "angles 0 44.43 0" - so the wrong read would answer 0 on
	//! it and look exactly like the bug being fixed. Pitch and roll are dropped by the shared helper at
	//! the spawn: a marker with a few degrees of terrain tilt must not lean the structure.
	//! \param[in] source Where the supply line starts.
	//! \param[in] objective Where it is going.
	//! \param[in] exclusions Places to stay clear of.
	//! \param[in] radii How far from each, same order.
	//! \param[out] best The marker that was drawn.
	//! \param[out] bestScore Its score. Reported for the log only - it no longer decides anything
	//!            between authored sites, and ResolveSite() has never read it.
	//! \param[out] bestYaw That marker's own heading - the drawn one's, not the last one evaluated.
	//! \return False when no marker qualifies.
	protected bool FindAuthoredSite(vector source, vector objective, notnull array<vector> exclusions, notnull array<float> radii, out vector best, out float bestScore, out float bestYaw)
	{
		best = vector.Zero;
		bestScore = 0;
		bestYaw = OVT_FOBSiting.NO_FACING;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		float bandMin;
		float bandMax;
		if (!ResolveBand(source, objective, bandMin, bandMax))
			return false;

		m_aFoundMarkers = new array<IEntity>();
		world.QueryEntitiesBySphere(objective, bandMax, AddMarker, FilterMarker, EQueryEntitiesFlags.ALL);

		// PASS ONE: every marker that survives every hard test, with the two things that have to travel
		// together - the position EvaluateCandidate accepted (ground-clamped, not the marker's raw
		// origin) and that marker's OWN heading. Parallel arrays rather than a small struct because
		// three of them beside one another read better here than a class nothing else would ever use.
		array<vector> eligible = new array<vector>();
		array<float> eligibleYaw = new array<float>();
		array<float> eligibleScore = new array<float>();

		foreach (IEntity marker : m_aFoundMarkers)
		{
			if (!marker)
				continue;

			OVT_FOBPositionComponent authored = OVT_FOBPositionComponent.Cast(marker.FindComponent(OVT_FOBPositionComponent));
			if (!authored || !authored.m_bEnabled)
				continue;

			// 🔴 THE SAME CORRIDOR THE GENERATED LATTICE OCCUPIES, AND WITHOUT IT THIS PATH WAS A RING.
			// The query above is a SPHERE around the objective and EvaluateCandidate's band test is a
			// DISTANCE - neither has a side - so before this line a marker directly behind the objective,
			// on the far side from every base the faction held, was as eligible as one on the supply
			// line and could win outright on terrain score. That is exactly what a play-test found
			// (2026-08-20). The generated sampler never had the problem because it constructs its
			// candidates along the line; this makes the authored path answer the same question rather
			// than a weaker one. See OVT_FOBSiting.IsInSupplyCorridor.
			if (!OVT_FOBSiting.IsInSupplyCorridor(marker.GetOrigin(), objective, source, bandMin, bandMax, m_fLateralSpread))
				continue;

			vector accepted;
			float score;
			if (!EvaluateCandidate(world, marker.GetOrigin(), objective, bandMin, bandMax, exclusions, radii, accepted, score))
				continue;

			eligible.Insert(accepted);
			eligibleYaw.Insert(marker.GetYawPitchRoll()[0]);
			eligibleScore.Insert(score);
		}

		m_aFoundMarkers = null;

		if (eligible.IsEmpty())
			return false;

		// PASS TWO: the draw.
		//
		// 🔴 Math.RandomInt IS MAX-EXCLUSIVE. The upper bound is Count(), NOT Count() - 1: writing the
		// latter is how the last authored marker in a list becomes unreachable, which is the same bug
		// this change exists to fix wearing a different hat. And RandomInt(0, 0) is an engine ERROR
		// rather than a zero, which is why the empty case is refused above and not clamped here.
		int chosen = Math.RandomInt(0, eligible.Count());

		best = eligible[chosen];
		bestYaw = eligibleYaw[chosen];
		bestScore = eligibleScore[chosen];

		// ⚠ THE POOL SIZE IS IN THE LINE, NOT JUST THE WINNER. "It always picks the same one" is an
		// observation about a DISTRIBUTION, and it cannot be confirmed or refuted from a log that only
		// ever names one site - a run that reports "1 of 1" is a siting question about the corridor and
		// the filters, while a run that reports "1 of 4" repeatedly is a randomness question. One number
		// tells the two apart without anybody reading this file again.
		OVT_DeploymentLog.Debug(LOG + "Forward base siting: drew authored site " + (chosen + 1).ToString() + " of " + eligible.Count().ToString() + " eligible in the corridor, at " + best.ToString());

		return true;
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
	protected bool EvaluateCandidate(notnull BaseWorld world, vector candidate, vector objective, float bandMin, float bandMax, notnull array<vector> exclusions, notnull array<float> radii, out vector accepted, out float score)
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
		if (spread >= FLATNESS_TOLERANCE)
			return false;

		if (!IsSiteClearOfObstructions(accepted))
			return false;

		float flatness = OVT_FOBSiting.FlatnessScore(spread, FLATNESS_TOLERANCE);
		float elevation = OVT_FOBSiting.ElevationScore(accepted[1] - world.GetSurfaceY(objective[0], objective[2]), ELEVATION_USEFUL_GAIN);

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
				offsetX = FLATNESS_PROBE_RADIUS;
			else if (i == 1)
				offsetX = -FLATNESS_PROBE_RADIUS;
			else if (i == 2)
				offsetZ = FLATNESS_PROBE_RADIUS;
			else
				offsetZ = -FLATNESS_PROBE_RADIUS;

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
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		autoptr TraceBox trace = new TraceBox();
		trace.Flags = TraceFlags.ENTS;
		trace.Start = position;
		trace.Mins = Vector(-CLEAR_BOX_HALF, 0, -CLEAR_BOX_HALF);
		trace.Maxs = Vector(CLEAR_BOX_HALF, CLEAR_BOX_HEIGHT, CLEAR_BOX_HALF);

		// The game-mode entity is excluded exactly as it was when this trace lived on the director: it
		// is a real entity in the world and a box traced through wherever it stands must not be rejected
		// by it.
		OVT_ObjectiveDirectorComponent director = GetDirector();
		if (director)
			trace.Exclude = director.GetOwner();

		float result = world.TracePosition(trace, null);

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
	protected bool FilterMarker(IEntity entity)
	{
		if (!entity)
			return false;

		return entity.FindComponent(OVT_FOBPositionComponent) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entity A marker that passed the filter.
	//! \return True, to keep the query running.
	protected bool AddMarker(IEntity entity)
	{
		m_aFoundMarkers.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// THE FORWARD BASE ONCE IT IS STANDING
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The structure is up. Called by the director when the deployment-side raise module reports it.
	//!
	//! ⚠ IT RECORDS. IT DOES NOT DECIDE. Every transition in this machine happens on DirectorTick(),
	//! behind its three early returns, and this is reached from a deployment's own update.
	//! \param[in] position Where the structure stands.
	void OnAssetRaised(vector position)
	{
		m_vSite = position;
		m_bDeploymentSent = true;
	}

	//! \return Where the forward base's deployment was sent, whether or not the structure went up yet.
	vector GetSite()
	{
		return m_vSite;
	}

	//! \return Whether a supply party has been sent. This is also what arms the spend ceiling.
	bool IsDeploymentSent()
	{
		return m_bDeploymentSent;
	}

	//------------------------------------------------------------------------------------------------
	// THE ONE TEARDOWN
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Takes the forward operating base down: its deployments, its structure, and its budget.
	//!
	//! ⚠ ONE PATH, THREE EXITS, AND ONLY ONE OF THEM IS PAID FOR:
	//!   STARVATION           - the supply line failed. No penalty: nobody took it off them.
	//!   PLAYER DISMANTLE     - the resistance cleared the site and pulled the flag down. THE PENALTY
	//!                          APPLIES, and it is applied by the caller rather than here, because
	//!                          "what happened" is the caller's knowledge and this method's job is only
	//!                          to make the base stop existing.
	//!   COUNTER-QRF RESOLVED - the battle is over whatever the outcome. No penalty.
	//! Everything else that ends an objective - the idle clock, a re-selection, a restore that could not
	//! re-link - funnels through ResetObjective() as well, so they are covered by construction.
	//!
	//! ⚠ THE INSERTION RESERVATION IS RELEASED BY DELETING THE DEPLOYMENTS, not by anything here.
	//! DestroyDeployment() runs every module's Cleanup(), and the insertion module's releases the convoy
	//! slot, deletes its waypoints and disposes of its truck. A slot lost to a convoy that ended quietly
	//! is a slot the faction never gets back, which is why the deployments are deleted rather than
	//! merely forgotten.
	//!
	//! IDEMPOTENT AND SAFE WITH NO FORWARD BASE. It is reached from every reset, including the ones for
	//! objectives that never got past harassment.
	//!
	//! ⚠ IT RUNS WITH THE PHASE ALREADY OVER, more often than not: the commonest teardown of a standing
	//! forward base is the counter-attack resolving, a phase later. Everything it reads is cached or
	//! resolved fresh - nothing goes through GetObjective().
	override void TearDownAsset()
	{
		vector site = vector.Zero;
		if (m_Asset)
			site = m_Asset.position;

		if (site == vector.Zero)
			site = m_vSite;

		// NOTHING WAS EVER SENT. This method is reached from every reset, including the many for
		// objectives that never got out of harassment, so the common case gets no queries at all.
		bool up = m_Asset && m_Asset.up;
		if (!up && !m_bDeploymentSent && site == vector.Zero)
		{
			ClearRuntimeState();
			return;
		}

		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (deployments)
		{
			// The tracked ledger has already taken down what this session created; these two sweeps are
			// for what it could not know about - a marker restored from a save, or one whose position
			// moved between the send and the raise.
			vector target = vector.Zero;
			if (m_OwnerDirector)
				target = m_OwnerDirector.GetObjectivePosition();

			if (target != vector.Zero)
			{
				OVT_DeploymentComponent carrier = FindLiveDeployment(deployments, target);
				if (carrier)
					deployments.DeleteDeployment(carrier);
			}

			if (site != vector.Zero)
			{
				array<OVT_DeploymentComponent> nearby = deployments.GetDeploymentsInRadius(site, AREA_RADIUS);
				if (nearby)
				{
					foreach (OVT_DeploymentComponent deployment : nearby)
					{
						if (!deployment)
							continue;

						string name = deployment.GetDeploymentName();
						if (name != m_sDeploymentConfigName && name != m_sGarrisonConfigName)
							continue;

						deployments.DeleteDeployment(deployment);
					}
				}
			}
		}

		if (site != vector.Zero)
			RemoveStructure(site);

		ClearRuntimeState();
	}

	//------------------------------------------------------------------------------------------------
	//! Puts back the forward-base state that is NOT in the save payload.
	//!
	//! Separate from the record clear so the teardown can finish its own job without depending on a
	//! caller to clear the record afterwards - the two are called in sequence by ResetObjective(), and
	//! each is idempotent on its own.
	protected void ClearRuntimeState()
	{
		m_bDeploymentSent = false;
		m_vSite = vector.Zero;
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
	//! ⚠ FOUND BY QUERY, NOT BY A REMEMBERED HANDLE. The deployment-side raise module holds an EntityID,
	//! but that link is runtime-only - a campaign that has been loaded since has a structure standing
	//! with nothing pointing at it. The join is by PREFAB RESOURCE NAME, which survives a restore
	//! because persistence respawns from the same prefab.
	//! \param[in] site Where the structure stands.
	protected void RemoveStructure(vector site)
	{
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance)
			return;

		ResourceName prefab = ResolveStructurePrefab();
		if (prefab.IsEmpty())
			return;

		m_sStructurePrefab = prefab;
		m_aFoundStructures = new array<IEntity>();

		BaseWorld world = GetGame().GetWorld();
		if (world)
			world.QueryEntitiesBySphere(site, STRUCTURE_SEARCH_RADIUS, AddStructure, FilterStructure, EQueryEntitiesFlags.ALL);

		foreach (IEntity structure : m_aFoundStructures)
		{
			if (!structure)
				continue;

			resistance.DestroyPlacedItem(structure);
		}

		m_aFoundStructures = null;
		m_sStructurePrefab = "";
	}

	//------------------------------------------------------------------------------------------------
	//! The structure prefab the forward-base config is authored to raise.
	//!
	//! READ OFF THE CONFIG rather than duplicated as a constant here, so a modded config that raises a
	//! different structure is still torn down correctly and there is exactly one authored answer.
	//! \return The prefab, or an empty ResourceName when the config does not resolve.
	protected ResourceName ResolveStructurePrefab()
	{
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (!deployments)
			return "";

		OVT_DeploymentConfig config = deployments.FindConfigByName(m_sDeploymentConfigName);
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
	protected bool FilterStructure(IEntity entity)
	{
		if (!entity)
			return false;

		return OVT_PrefabUtils.GetPrefabName(entity) == m_sStructurePrefab;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entity A structure that passed the filter.
	//! \return True, to keep the query running.
	protected bool AddStructure(IEntity entity)
	{
		m_aFoundStructures.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// THE PLAYER-INITIATED EXIT
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The dismantle rule itself, asked about an arbitrary forward-base position.
	//!
	//! ⚠ ONE IMPLEMENTATION, ASKED TWICE, AND THE SERVER'S ANSWER IS THE ONLY ONE THAT COUNTS. The user
	//! action asks this on the client, about the flag it is attached to, so the prompt can say why it is
	//! refusing; the request handler asks it on the server, about the position the DIRECTOR believes the
	//! base is at, before anything happens. That is not redundancy - the client's copy is a courtesy and
	//! the server never trusts it, which is this epic's standing debt (BUG-025). Sharing the body is what
	//! stops the text a player reads and the rule the server enforces from drifting apart, which is how
	//! "the action was available and did nothing" happens.
	//!
	//! 🔴 IT IS A STATIC, AND THE TWO RADII IT USES ARE CONSTANTS, PRECISELY BECAUSE OF THAT SECOND
	//! CALLER. On a client there is no objective, no plan and no runtime module set, so there is no
	//! authored value to read: a static over two constants is the only shape in which both machines
	//! provably ask the same question. See DISMANTLE_RANGE.
	//!
	//! ⚠ IT IS SAFE ON A CLIENT. Everything it reads - a position, the campaign's faction key, and the
	//! AI agents standing in the world - exists on every machine.
	//! \param[in] callerPosition Where the caller is standing.
	//! \param[in] assetPosition Where the forward base is.
	//! \param[out] refusal A localization key naming why, written only when this returns false.
	//! \return True when a dismantle would be allowed.
	static bool CanDismantleAt(vector callerPosition, vector assetPosition, out string refusal)
	{
		refusal = "";

		if (vector.Distance(callerPosition, assetPosition) > DISMANTLE_RANGE)
		{
			refusal = "#OVT-DismantleEnemyFOB_TooFar";
			return false;
		}

		if (CountOccupyingDefendersNear(assetPosition, DEFENDER_CLEAR_RADIUS) > 0)
		{
			refusal = "#OVT-DismantleEnemyFOB_Defended";
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! How many occupying-faction soldiers are still on their feet near a position.
	//!
	//! ⚠ AGENTS, NOT REGISTERED HANDLES, AND THAT IS THE OPPOSITE OF THE STARVATION COUNT - on purpose.
	//! Starvation asks "does this base still have a garrison at all", which is true of a perfectly alive
	//! dormant group and must be answered off the survivor mask. This asks "is anybody SHOOTING AT ME
	//! right now", which is a question about materialised bodies: a player is standing at the flag, so
	//! everything nearby is spawned, and a dormant group two hundred metres away is not what stops a
	//! dismantle.
	//!
	//! ⚠ IT WALKS EVERY AI AGENT IN THE WORLD, which is the same thing the battle scorer does every ten
	//! seconds - fine at that rate and not fine per frame. The user action caches its answer for a
	//! second; anything else that starts asking this often should do the same.
	//! \param[in] position The place to count around.
	//! \param[in] radius How far, in metres.
	//! \return The count.
	static int CountOccupyingDefendersNear(vector position, float radius)
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
	//! What a player pays the occupying faction for pulling the flag down.
	//!
	//! ⚠ IT IS THE ONE EXIT THAT COSTS THE OCCUPYING FACTION RESOURCES, and the amount is what the base
	//! cost to raise. Clearing one is worth doing rather than merely satisfying.
	//! \return The penalty, in pool resources. Non-positive means none.
	int GetDismantlePenalty()
	{
		return ResolveBudgetCost();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The bag prefix this module declares. Every value it keeps lives on the ASSET RECORD
	//!         under its key rather than in the generic bag, which is what lets one save format carry
	//!         a forward base, a checkpoint and anything after them.
	override string GetBagPrefix()
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 EVERY ATTRIBUTE, BY HAND, INCLUDING THE PARENTS'. Not chained, silently drops what it forgets.
	//!
	//! What a dropped line costs here: drop m_sAssetKey and the clone owns no record, so the raise
	//! writes nothing and IsAssetUp() is false forever; drop m_sDeploymentConfigName and nothing can be
	//! bought or found again, so the phase sends a base every interval and never notices one standing;
	//! drop m_sGarrisonConfigName and the teardown leaves the garrison in the field with no objective
	//! behind it; drop m_iBudgetCost and the clone reads 0, which FOBBudgetCeiling turns into a zero
	//! ceiling and WithinFOBCeiling refuses everything against - the phase can then never buy anything
	//! at all; drop any siting attribute and the clone reads 0, which collapses the band, the lattice
	//! or the corridor and produces "nowhere to put a forward base" on every map.
	//!
	//! ⚠ THE RUNTIME STATE IS DELIBERATELY NOT COPIED. A clone is a fresh module for a fresh phase
	//! entry: it has sent nothing, sited nothing and owns no query buffers.
	//! \return An independent copy.
	override OVT_BaseObjectiveModule CloneModule()
	{
		OVT_RaiseForwardBaseObjectiveOperation clone = new OVT_RaiseForwardBaseObjectiveOperation();

		clone.m_sModuleName = m_sModuleName;
		clone.m_sAssetKey = m_sAssetKey;
		clone.m_sDeploymentConfigName = m_sDeploymentConfigName;
		clone.m_sGarrisonConfigName = m_sGarrisonConfigName;
		clone.m_iBudgetCost = m_iBudgetCost;
		clone.m_fBandMinFraction = m_fBandMinFraction;
		clone.m_fBandMaxFraction = m_fBandMaxFraction;
		clone.m_fMinStandoff = m_fMinStandoff;
		clone.m_fMaxStandoff = m_fMaxStandoff;
		clone.m_iSitingSteps = m_iSitingSteps;
		clone.m_iSitingLanes = m_iSitingLanes;
		clone.m_fLateralSpread = m_fLateralSpread;

		return clone;
	}
}
