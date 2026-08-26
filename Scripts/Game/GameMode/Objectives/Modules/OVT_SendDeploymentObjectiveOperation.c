//------------------------------------------------------------------------------------------------
//! WHICH DIFFICULTY FIELD A SEND'S CONCURRENCY CAP DEFERS TO WHEN IT AUTHORS -1.
//!
//! ⚠ AN ENUM RATHER THAN A FIELD NAME STRING, because §3.10 of the plan is explicit that a module's
//! difficulty fallback is "resolved per module type in code, no string lookups": a mistyped field name
//! would resolve to nothing at runtime and read as "no cap", which is an unbounded spender.
//!
//! ⚠ TWO MEMBERS, AND THE SECOND IS WHAT LETS THE FORWARD BASE'S GARRISON BE THIS MODULE. The four
//! hard-coded senders shared one cap field; the garrison sender had its own, measured around the
//! forward base rather than around the objective. Without this the garrison could not be authored
//! without either duplicating the module or changing what its cap means on every difficulty preset.
//------------------------------------------------------------------------------------------------
enum OVT_EObjectiveConcurrencyLimit
{
	//! objectiveHarassmentMaxConcurrent - "how many objective operations may be alive at once".
	OBJECTIVE_OPERATIONS = 0,

	//! objectiveFOBGarrisonMax - how many garrison groups one forward base may hold.
	FORWARD_BASE_GARRISON = 1
}

//------------------------------------------------------------------------------------------------
//! THE ONE OPERATION MODULE THE SHIPPED DOCTRINE IS BUILT FROM: buy one deployment, somewhere a
//! resolver names, if the caps and the pool allow.
//!
//! It unifies four hard-coded senders that were four methods with four inline destination rules:
//!   the harassment sender     the objective itself, a four-rung ladder, a concurrency cap
//!   the recapture sender      every resistance-held tower affecting the objective, deduped
//!   the sabotage sender       the objective's base, one config, the same concurrency cap
//!   the garrison sender       the forward base's own position, a cap of its own      (build phase 5)
//! What differed between them was WHERE it goes, WHICH config, HOW MANY may be alive and WHETHER a rung
//! is chosen - and every one of those four is now an attribute or a resolver.
//!
//! ================================ THE ORDER IS THE CONTRACT ================================
//! ⚠ THE AUTHORED ORDER OF A PHASE'S OPERATION MODULES IS THE EVALUATION ORDER, and .conf files cannot
//! carry comments, so the shipped order is written down HERE and in OVT_ObjectivePhase's header and
//! nowhere else. Both shipped plans author their harassment phase as:
//!
//!       1. tower recapture      2. harassment ladder      3. sabotage
//!
//! ⚠ THAT IS THE TOWN'S CHAIN. A BASE OBJECTIVE DROPPED TOWER RECAPTURE ON 2026-08-21 (author): *"This
//! is a base, radio towers don't matter to a base and there are non-objective deployments built to
//! handle radio towers that don't matter to the current objective."* Objective_BaseOffensive.conf now
//! authors the ladder then sabotage in both of its phases; Objective_TownOffensive.conf is unchanged,
//! because unrest at a tower is exactly what a town's ramp is fighting. A .conf cannot carry a comment,
//! which is why the reasoning lives here and the shape is pinned by the Init suites.
//!
//! ⚠ AND THE ORDER IS NO LONGER THE EVALUATION ORDER. The director draws a fresh random order every
//! cadence so its next move cannot be learned - see OVT_BaseObjectiveOperationModule.ShufflesFreely().
//! The authored order still decides which operations are PINNED ahead of that draw.
//!
//! which is the hard-coded ramp spender's `recapture || harassment || sabotage` chain, term for term
//! and in the same order. TOWER RECAPTURE IS FIRST because it is the most
//! urgent and the most bounded of the three: a tower is a discrete thing that is either being worked on
//! or is not, this module deduplicates against the live deployment, and a tower left in resistance
//! hands keeps the objective easier for the resistance to hold. The other two have no such ceiling and
//! will still be there next interval.
//!
//! ⚠ HARASSMENT AND SABOTAGE ARE MUTUALLY EXCLUSIVE BY TARGET KIND, NOT BY PRIORITY - which is why
//! their relative order is arbitrary and why authoring both in one phase is safe. m_iRequiredTargetKind
//! is the verbatim port of each sender's first line ("towns only" / "bases only"), so at most one of
//! them can ever answer for a given objective.
//! ===========================================================================================
//!
//! 🔴 THE FIRST MODULE THAT ACTS CONSUMES THE CADENCE, AND EVERY REFUSAL LEAVES IT AT ZERO. Nothing to
//! recapture, nothing to sabotage, the cap full, the pool short, a config missing - each of those leaves
//! the countdown where it was so the NEXT tick asks again a minute later, instead of waiting out another
//! whole interval for a condition that may have cleared immediately. That retry is also what makes an
//! affordability hold cover a whole poverty spell rather than one tick in forty-five: every minute of it
//! reaches the spender and is refused again, and each refusal re-asserts the reserve floor.
//!
//! 🔴 IT SPENDS NOTHING ITSELF (G5). It asks the director to create a deployment; the director creates
//! it and debits the ONE faction pool for it, in one place, once. This module never touches the resource
//! map, never credits anything and never holds money.
//!
//! ⚠ THE DEDUP-THEN-NEXT-CANDIDATE WALK IS INSIDE THIS MODULE, NOT AROUND IT. The resolver answers many
//! positions in preference order; this walks them, skips any that already carries a live instance of the
//! config it is about to send within m_fDedupRadius, and creates at the first free one. Lifting that
//! walk outside would mean an objective covered by two towers sends nothing once the first one has a
//! team on it.
//!
//! ⚠ DEDUPLICATED AGAINST THE LIVE DEPLOYMENT LIST, NEVER AGAINST THE DIRECTOR'S TEARDOWN LEDGER. That
//! ledger only grows until a reset, so a team that was wiped out or collected is still in it and would
//! block its tower from ever being worked on again.
//!
//! ⚠ THE LADDER NAMES REGISTRY ENTRIES, IT DOES NOT BUILD THEM. The first four harassment rungs are thin
//! inherit-and-delta variants of one .conf in overthrowDeployments.conf, each with its own group type,
//! cost and name. A per-create group override would be a second way of deciding what a deployment
//! contains, parallel to the config system and invisible to the evaluator's cost model, the Game Master
//! panel, the reinforcement rebuy and the save - all of which read the CONFIG.
//!
//! ⚠ THE NAMES ARE MATCHED BY STRING, THREE TIMES EACH: the registry resolves the config, the dedup and
//! the concurrency count match live deployments back to it, and the director's teardown ledger stores
//! it. A name changed in overthrowDeployments.conf and not here does not fail to parse and does not
//! warn - the ramp simply stops sending anything, with one ERROR line per in-game minute as its only
//! symptom. An initialisation case resolves every authored name against the live registry, and another
//! pins the authored ladder against the director's own constant, for exactly that reason.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_SendDeploymentObjectiveOperation : OVT_BaseObjectiveOperationModule
{
	[Attribute(desc: "The registered deployment config to send, by its m_sDeploymentName. Leave empty when m_aLadder is authored - the rung IS the config. Matched by string against overthrowDeployments.conf")]
	string m_sConfigName;

	[Attribute(desc: "THE ESCALATION LADDER: one registered config name per rung, in ASCENDING order. Authored instead of m_sConfigName. Which rung is bought is decided by the bag key below; the ramp SATURATES at the top rung rather than wrapping. Order IS the ramp - reordering it re-tunes the whole escalation")]
	ref array<string> m_aLadder;

	[Attribute(defvalue: "harassment.successes", desc: "Which bag counter drives the ladder rung: one completed operation buys one rung. Only read when m_aLadder is authored")]
	string m_sLadderProgressKey;

	[Attribute(desc: "WHERE THIS OPERATION GOES. Ship OVT_ObjectiveSelfTargetResolver (the objective itself), OVT_EnemyTowersAffectingTargetResolver (every resistance-held tower covering it), OVT_ForwardBaseTargetResolver (the standing forward base) or OVT_NearestControlledBaseTargetResolver. REQUIRED - a module with none sends nothing")]
	ref OVT_ObjectiveTargetResolver m_Resolver;

	[Attribute(defvalue: "-1", desc: "How many live deployments of this operation may exist near the objective at once. -1 = the campaign's objectiveHarassmentMaxConcurrent difficulty setting (Easy 1 up to Insane 4), which is authored as 'how many objective operations may be alive at once' rather than as a town-only figure. 0 = no cap at all, which is what tower recapture wants - it bounds itself by deduplicating per tower")]
	int m_iMaxConcurrent;

	[Attribute(defvalue: "800", desc: "How far from the OBJECTIVE a live deployment counts towards the concurrency cap. Wider than a town centre because an operation's marker sits where the director put it and a town is not a point. Ignored when the cap is 0")]
	float m_fConcurrencyRadius;

	[Attribute(defvalue: "0", desc: "How close to a RESOLVED POSITION a live deployment of this config has to be to count as 'already sent there', so the walk moves on to the next candidate. 300 matches the ring that classifies a position as a radio tower. 0 = no per-position dedup, which is right when the resolver answers a single place the cap already governs")]
	float m_fDedupRadius;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.ComboBox, desc: "Only act when the objective is this kind of place. 0 (NONE) = any kind. The verbatim port of each hard-coded sender's first line: harassment is towns only, sabotage is bases only", enums: ParamEnumArray.FromEnum(OVT_EObjectiveKind))]
	int m_iRequiredTargetKind;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.ComboBox, desc: "Which difficulty field m_iMaxConcurrent -1 defers to. OBJECTIVE_OPERATIONS is objectiveHarassmentMaxConcurrent, which is what the ramp's three senders share; FORWARD_BASE_GARRISON is objectiveFOBGarrisonMax. Ignored when m_iMaxConcurrent authors a number", enums: ParamEnumArray.FromEnum(OVT_EObjectiveConcurrencyLimit))]
	int m_iMaxConcurrentDifficulty;

	[Attribute(defvalue: "0", desc: "Measure the concurrency cap around the RESOLVED destination instead of around the objective. 1 for the forward-base garrison, whose cap is 'how many groups are at the BASE' and whose base is not where the objective is. 0 for every ramp operation, which is capped around the objective it is harassing")]
	bool m_bConcurrencyAtResolvedPosition;

	//! Scratch for the resolver, allocated once per module rather than once per tick. Never read across
	//! calls - the resolver clears it - so it carries no state and is not copied by CloneModule().
	protected ref array<vector> m_aResolved;

	//------------------------------------------------------------------------------------------------
	void OVT_SendDeploymentObjectiveOperation()
	{
		if (!m_aLadder)
			m_aLadder = new array<string>();

		m_aResolved = new array<vector>();
	}

	//------------------------------------------------------------------------------------------------
	//! Buy one deployment, if there is anywhere to put it and it can be afforded.
	//! \return True when a deployment was created AND paid for - which is PROGRESS, re-arms the idle
	//!         clock, consumes the cadence and stops later operation modules being asked this tick.
	override bool TryAct()
	{
		OVT_ObjectiveInstance objective = GetObjective();
		if (!objective || !objective.IsLive())
			return false;

		// The port of "towns only" / "bases only". See the class header.
		if (m_iRequiredTargetKind != OVT_EObjectiveKind.NONE && objective.GetTargetKind() != m_iRequiredTargetKind)
			return false;

		OVT_ObjectiveDirectorComponent director = GetDirector();
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!director || !deployments || !config || !m_Resolver)
			return false;

		int factionIndex = config.GetOccupyingFactionIndex();
		if (factionIndex < 0)
			return false;

		string configName = ResolveConfigName(objective);
		if (configName == "")
			return false;

		// ⚠ THE CAP IS ASKED BEFORE THE RESOLVER WHEN IT IS MEASURED AROUND THE OBJECTIVE, because it
		// does not depend on where the operation would go and the resolver is the more expensive
		// question. A cap measured at the DESTINATION cannot be asked until there is one, so it moves
		// inside the walk below - which is also the only shape in which two candidates can be capped
		// independently.
		if (!m_bConcurrencyAtResolvedPosition && !WithinConcurrencyCap(objective.GetTargetPosition(), deployments, configName, factionIndex))
			return false;

		if (!m_aResolved)
			m_aResolved = new array<vector>();

		if (!m_Resolver.Resolve(objective, factionIndex, m_aResolved))
			return false;

		foreach (vector position : m_aResolved)
		{
			// ⚠ ALREADY SERVED HERE - move on to the NEXT candidate rather than giving up. This is the
			// shipped tower walk and it is why the resolver answers many.
			if (m_fDedupRadius > 0 && deployments.GetDeploymentNearPosition(configName, position, m_fDedupRadius))
				continue;

			if (m_bConcurrencyAtResolvedPosition && !WithinConcurrencyCap(position, deployments, configName, factionIndex))
				continue;

			return director.CreateObjectiveDeployment(deployments, configName, position, factionIndex);
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Which config this send would buy right now: the single authored one, or the ladder rung the
	//! objective has earned.
	//!
	//! ⚠ THE RUNG SATURATES AT THE TOP RATHER THAN WRAPPING OR RUNNING OFF THE END, so a long-running
	//! objective keeps sending the biggest group it has instead of starting over with a two-man patrol.
	//! The arithmetic is OVT_ObjectivePhaseRules.HarassmentLadderIndex() and is not restated here.
	//! \param[in] objective The objective, for its ladder progress counter.
	//! \return A registered config name, or an empty string when this module has nothing to send.
	string ResolveConfigName(notnull OVT_ObjectiveInstance objective)
	{
		if (!m_aLadder || m_aLadder.IsEmpty())
			return m_sConfigName;

		int rung = OVT_ObjectivePhaseRules.HarassmentLadderIndex(objective.Get(m_sLadderProgressKey), m_aLadder.Count());
		if (rung == OVT_ObjectivePhaseRules.NO_LADDER_RUNG)
			return "";

		return m_aLadder[rung];
	}

	//------------------------------------------------------------------------------------------------
	//! Whether another one of these may be alive at the objective right now.
	//!
	//! ⚠ COUNTED FROM THE LIVE DEPLOYMENT LIST, NOT FROM THE DIRECTOR'S TEARDOWN LEDGER, for the reason
	//! the class header gives: the ledger only grows until a reset, so an operation that completed its
	//! hold, was wiped out or was collected by its own condition module is still in it and must not hold
	//! a concurrency slot.
	//!
	//! ⚠ EVERY RUNG COUNTS TOWARDS THE SAME CAP, because a rung is the same operation with bigger men in
	//! it. A ladder module therefore matches ALL of its rung names, never just the one it would buy next.
	//! \param[in] centre What the cap is measured around - the objective, or the resolved destination
	//!            when m_bConcurrencyAtResolvedPosition is authored.
	//! \param[in] deployments The deployment framework.
	//! \param[in] configName The config this send would buy, for a module with no ladder.
	//! \param[in] factionIndex The faction whose deployments count.
	//! \return True when there is room for another.
	protected bool WithinConcurrencyCap(vector centre, notnull OVT_DeploymentManagerComponent deployments, string configName, int factionIndex)
	{
		int cap = ResolveMaxConcurrent();
		if (cap <= 0)
			return true;

		array<OVT_DeploymentComponent> nearby = deployments.GetDeploymentsInRadius(centre, m_fConcurrencyRadius);
		if (!nearby)
			return true;

		int count = 0;
		foreach (OVT_DeploymentComponent deployment : nearby)
		{
			if (!deployment)
				continue;

			if (deployment.GetControllingFaction() != factionIndex)
				continue;

			if (!CountsTowardsCap(deployment.GetDeploymentName(), configName))
				continue;

			count++;
		}

		return count < cap;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a live deployment's config name is one of this module's own.
	//! \param[in] liveName The live deployment's config name.
	//! \param[in] configName The config this send would buy, for a module with no ladder.
	//! \return True when it occupies one of this module's concurrency slots.
	protected bool CountsTowardsCap(string liveName, string configName)
	{
		if (m_aLadder && !m_aLadder.IsEmpty())
			return m_aLadder.Find(liveName) != -1;

		return liveName == configName;
	}

	//------------------------------------------------------------------------------------------------
	//! The concurrency cap, authored or from difficulty.
	//!
	//! ⚠ THE SAME KNOB THE TOWN RAMP AND THE SABOTAGE RAMP BOTH USE, DELIBERATELY.
	//! objectiveHarassmentMaxConcurrent is authored as "how many objective operations may be alive at
	//! once", not as a town-only figure, and the twelve difficulty fields contain no separate sabotage
	//! cap. A thirteenth field would have to be added to five presets to say the same number twice.
	//! \return The cap, or zero or below for "no cap".
	int ResolveMaxConcurrent()
	{
		int difficultyValue = -1;

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (difficulty)
		{
			if (m_iMaxConcurrentDifficulty == OVT_EObjectiveConcurrencyLimit.FORWARD_BASE_GARRISON)
				difficultyValue = difficulty.objectiveFOBGarrisonMax;
			else
				difficultyValue = difficulty.objectiveHarassmentMaxConcurrent;
		}

		return OVT_ObjectivePlanRules.ResolveWithDifficulty(m_iMaxConcurrent, difficultyValue);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The bag prefix this module reads. It WRITES nothing - the ladder counter is reported by
	//!         the deployment-side behaviour module that completes an operation, not by the sender.
	override string GetBagPrefix()
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 EVERY ATTRIBUTE, BY HAND, INCLUDING THE PARENT'S. Not chained, silently drops what it forgets.
	//!
	//! What a dropped line costs here: drop m_sConfigName or m_aLadder and the module resolves nothing
	//! and refuses every tick, silently; drop m_Resolver and it has nowhere to send and does the same;
	//! drop m_iMaxConcurrent and every clone reads 0, which is "no cap" - an unbounded spender, which is
	//! the exact unpaced lurch the cadence exists to prevent; drop m_fConcurrencyRadius and the cap
	//! counts nothing, same result; drop m_fDedupRadius and a second tower is never picked up because the
	//! first one's team no longer blocks its own position; drop m_iRequiredTargetKind and a town ramp
	//! sends sabotage teams at towns; drop m_iMaxConcurrentDifficulty and the
	//! forward-base garrison's cap silently becomes the ramp's, which is a different number on every
	//! preset; drop m_bConcurrencyAtResolvedPosition and that same cap is counted around the OBJECTIVE,
	//! where the garrison is not, so it counts nothing and the base is reinforced without limit.
	//!
	//! ⚠ THE LADDER IS COPIED ENTRY BY ENTRY INTO A NEW ARRAY, not by handing over the reference. A
	//! shared array is safe only for as long as nobody writes to it, and "nobody writes to it" is not a
	//! property a clone can guarantee about a template it does not own.
	//!
	//! ⚠ THE RESOLVER IS SHARED BY REFERENCE, DELIBERATELY, and it is the one thing here that is. A
	//! resolver is arithmetic over its arguments with nothing cached across calls (its seam's contract
	//! point 3), so one instance answering for every clone is the same decision the plan's selectors
	//! already make. Cloning it would mean a CloneModule() on every resolver and a dropped-line hazard
	//! per resolver attribute, for no behaviour.
	//! \return An independent copy.
	override OVT_BaseObjectiveModule CloneModule()
	{
		OVT_SendDeploymentObjectiveOperation clone = new OVT_SendDeploymentObjectiveOperation();

		clone.m_sModuleName = m_sModuleName;
		clone.m_sConfigName = m_sConfigName;
		clone.m_sLadderProgressKey = m_sLadderProgressKey;
		clone.m_Resolver = m_Resolver;
		clone.m_iMaxConcurrent = m_iMaxConcurrent;
		clone.m_fConcurrencyRadius = m_fConcurrencyRadius;
		clone.m_fDedupRadius = m_fDedupRadius;
		clone.m_iRequiredTargetKind = m_iRequiredTargetKind;
		clone.m_iMaxConcurrentDifficulty = m_iMaxConcurrentDifficulty;
		clone.m_bConcurrencyAtResolvedPosition = m_bConcurrencyAtResolvedPosition;

		clone.m_aLadder = new array<string>();
		if (m_aLadder)
		{
			foreach (string rung : m_aLadder)
			{
				clone.m_aLadder.Insert(rung);
			}
		}

		return clone;
	}
}
