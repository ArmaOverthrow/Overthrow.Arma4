//------------------------------------------------------------------------------------------------
//! "ONLY AT THE CURRENT OBJECTIVE, AND ONLY WHILE THE RAMP IS IN A PHASE THIS CONFIG BELONGS TO."
//!
//! The gate every deployment the objective director creates carries. It answers one question -
//! is this position inside the current objective's working radius, with the ramp inside the phase
//! RANGE this config belongs to - and it answers it identically at creation time and at runtime.
//!
//! 🔴 THE RANGE REPLACED AN EQUALITY TEST ON 2026-08-19, AND THE EQUALITY TEST WAS A DEADLOCK. A base
//! objective is promoted out of harassment by its FIRST completed sabotage mission and needs six of them
//! (on Easy) to earn its counter-attack; with every Phase 1 config scoped to phase 1 alone, the promotion
//! itself made the remaining five unsendable and unreachable. Towns deadlocked the same way one step
//! later. The whole of the fix is that a config now states a SPAN - see
//! OVT_ObjectivePhaseRules.PhaseInRange(), which owns the arithmetic and the reasoning.
//!
//! ================== THE SYMMETRY IS DELIBERATE, AND IT IS THE OPPOSITE OF ==================
//! ================== OVT_NoPlayersNearbyConditionDeploymentModule ===========================
//! That module's whole design is an ASYMMETRY: its creation gate tests player distance and its
//! runtime gate returns true unconditionally, because deleting a base's garrison the moment a player
//! walks in would evaporate the fight on approach. The opposite is true here. An objective operation
//! SHOULD be collected when the objective moves: the men were sent to soften a specific place for a
//! specific span of the ramp, and once the director has given that place up - or advanced past the LAST
//! phase that wanted them - they are a force with no orders standing in a town nobody is attacking any
//! more. Paired with m_bDeleteOnConditionFail on the reinforcement module, this is the whole of the
//! "objective operations clean themselves up" path. There is no code anywhere that watches for an
//! objective ending and goes looking for its deployments; the reset path takes down what it tracked
//! and this condition collects the rest.
//!
//! 🔴 AND THE SYMMETRY IS ALSO THE PROOF THAT A LIVE OPERATION SURVIVES A PROMOTION. "May a new one be
//! created in phase 2" and "may a live one keep working in phase 2" are the SAME CALL to the same
//! predicate on the same authored range, so they cannot disagree. A sabotage team created in harassment
//! is not collected when the objective enters the forward-base phase for exactly the reason a new one
//! may be sent there: the config says it belongs to both. Getting that wrong in the other direction is
//! what deleted a walking team five minutes short of its target once already; there is no second rule
//! here to keep in step with the first, because there is no second rule.
//! ==========================================================================================
//!
//! ⚠ IT ALSO MAKES THE OBJECTIVE CONFIGS SAFE IN THE EVALUATOR. Those configs sit in the same
//! registry as every routine one, so the 30 s evaluation pass will consider them at any candidate
//! position it generates. With this module authored, a pass that reaches one at a place that is not
//! the objective - or with no objective at all - simply finds it unsuitable, which is why the
//! director being the only creator of these deployments is a property rather than a hope.
//!
//! NO DIRECTOR AT ALL IS A REFUSAL, not a pass. A world with no objective director (an initialisation
//! test world, a mod that removed the component) has no objective, so nothing may be created "at" one.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_ObjectiveConditionDeploymentModule : OVT_BaseConditionDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;

	//! THE FIRST PHASE OF THE RANGE, BY ITS AUTHORED NAME. m_sThroughPhase is the last.
	//!
	//! ⚠ IT IS A NAME AND NOT AN INDEX, AND THE NAME IS THE ONE THING A PLAN AND A DEPLOYMENT CONFIG CAN
	//! AGREE ON. An index would re-point silently the moment a plan grew a phase in the middle, and an
	//! enum integer only ever described the three phases the hard-coded machine happened to have. The
	//! name is also the save payload's key, so a rename costs TWO edits - the plan and every config that
	//! names the phase - and an unresolved one is reported by name rather than silently landing on
	//! whichever index sits there now.
	[Attribute(defvalue: "Harassment", desc: "The FIRST objective phase this deployment belongs to, by the m_sPhaseName authored in the plan. The shipped plans author 'Harassment', 'ForwardBase' and 'CounterAttack'. A name the running plan does not carry matches NOTHING - it is not silently ignored")]
	string m_sFromPhase;

	//! THE LAST PHASE OF THE RANGE, INCLUSIVE.
	//!
	//! ⚠ AN EMPTY STRING - WHICH IS BOTH THE ATTRIBUTE DEFAULT AND THE VALUE AN UNAUTHORED FIELD HOLDS -
	//! MEANS "THE FIRST PHASE ONLY", i.e. exactly the equality test this module used before the range
	//! existed. The two agreeing is deliberate: it does not matter whether a .conf that omits this line
	//! is given the attribute's defvalue or the script member's empty string, because both say the same
	//! thing. This is the string-space form of OVT_ObjectivePhaseRules.EffectiveLastPhase()'s collapse
	//! and it is applied ONCE, in EffectiveThroughPhase(), so a reader and the live predicate can never
	//! disagree about what a config spans.
	//!
	//! ⚠ AUTHORING THE BATTLE PHASE ON A RAMP OPERATION IS A MISTAKE, and an initialisation case refuses
	//! it for every shipped objective config. Harassment and sabotage teams walking in while the battle
	//! they were building up to is being fought are noise. It is only ever the SECOND line of defence:
	//! DirectorTick() early-returns for the whole of a live battle, so no operation can be created in
	//! that phase whatever a config says - what this bound decides is whether one already in the world
	//! is collected when the battle starts, and it must be.
	[Attribute(defvalue: "", desc: "The LAST objective phase this deployment may keep working in, INCLUSIVE, by its authored name. EMPTY (the default) means the first phase only - the pre-range behaviour. Author 'ForwardBase' on a ramp operation so it continues through the forward-base phase, which is what makes the counter-attack reachable")]
	string m_sThroughPhase;

	[Attribute(defvalue: "600", desc: "How far from the current objective this deployment may sit. Generous by design: a tower or a landing zone belonging to an objective is not AT it")]
	float m_fMaxDistanceFromObjective;

	//! Whether the unresolved-span error has already been said. Once per module, not once per evaluation
	//! pass - this is asked about every candidate position the evaluator generates, and a line per
	//! candidate is how a log stops being read. NOT an attribute and NOT copied by CloneModule().
	protected bool m_bUnresolvedSpanLogged;

	//------------------------------------------------------------------------------------------------
	//! Runtime gate: is this deployment still working on the objective it was created for?
	//! \return True while the objective is live, in the required phase, and within range.
	override bool EvaluateCondition()
	{
		if (!m_ParentDeployment)
			return false;

		return IsAtCurrentObjective(m_ParentDeployment.GetPosition());
	}

	//------------------------------------------------------------------------------------------------
	//! Creation gate: asked by the evaluator about a candidate position before any deployment exists,
	//! and by the director itself through CheckDeploymentConditions on the way to a forced create.
	//! \param[in] position The candidate position.
	//! \param[in] factionIndex The faction the evaluator is deploying for. Unused - the objective
	//!            belongs to the occupying faction and the config's own faction flags say so.
	//! \param[in] threatLevel The candidate's scored threat. Unused - an objective is worth working on
	//!            whatever has happened near it lately.
	//! \return True when the position is at the current objective, in the required phase.
	override bool EvaluateStaticCondition(vector position, int factionIndex, float threatLevel)
	{
		return IsAtCurrentObjective(position);
	}

	//------------------------------------------------------------------------------------------------
	//! THE ONE QUESTION, asked by both evaluations. See the class header for why they are the same.
	//! \param[in] position The position being judged.
	//! \return True when there is an objective, it is inside this config's authored phase range, and the
	//!         position is within m_fMaxDistanceFromObjective of it.
	bool IsAtCurrentObjective(vector position)
	{
		OVT_ObjectiveDirectorComponent director = OVT_ObjectiveDirectorComponent.GetInstance();
		if (!director)
			return false;

		if (!director.HasObjective())
			return false;

		if (!IsInAuthoredPhaseRange(director))
			return false;

		return vector.Distance(position, director.GetObjectivePosition()) <= m_fMaxDistanceFromObjective;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the running objective's phase is inside this config's authored span.
	//!
	//! 🔴 AN UNRESOLVABLE SPAN REFUSES, AND IT SAYS SO ONCE. Three things can make it unresolvable: the
	//! objective is running with no plan behind it (the registry did not load), the plan does not carry
	//! the phase this config names (a rename that was made in one place and not the other), or the
	//! objective has not entered a phase yet. All three mean this deployment cannot say whether it
	//! belongs here, and a condition that cannot answer must refuse - which is the same answer the
	//! enum-era predicate gave a config scoped to a phase that did not exist.
	//!
	//! ⚠ IT IS FAILING CLOSED ON PURPOSE, AND THE COST IS REAL: every deployment of this config is
	//! collected on its next reinforcement check. That is why the refusal is LOGGED, once per module,
	//! naming the plan and the missing phase - the failure is loud rather than a campaign that quietly
	//! stops garrisoning its objective. The latch is the ONLY side effect either evaluation has.
	//! \param[in] director The objective director.
	//! \return True when the objective is inside the authored span.
	protected bool IsInAuthoredPhaseRange(notnull OVT_ObjectiveDirectorComponent director)
	{
		int fromIndex = director.IndexOfObjectivePhase(m_sFromPhase);
		int throughIndex = director.IndexOfObjectivePhase(EffectiveThroughPhase());

		if (fromIndex < 0 || throughIndex < 0)
		{
			LogUnresolvedSpan(director);
			return false;
		}

		return OVT_ObjectivePlanRules.PhaseIndexInRange(director.GetObjectivePhaseIndex(), fromIndex, throughIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! Says, once per module, that this config names a phase the running plan does not have.
	//! \param[in] director The objective director, for the plan's name.
	protected void LogUnresolvedSpan(notnull OVT_ObjectiveDirectorComponent director)
	{
		if (m_bUnresolvedSpanLogged)
			return;

		m_bUnresolvedSpanLogged = true;

		string plan = director.GetObjectiveConfigName();
		if (plan == "")
			plan = "<no plan - the objective registry did not resolve>";

		Print(string.Format("[Overthrow.ObjectiveCondition] '%1' spans phases '%2'..'%3', which the running plan '%4' does not carry - every deployment of this config will be refused and collected. Check the m_sPhaseName values in Configs/Objective against this config",
			m_sModuleName, m_sFromPhase, EffectiveThroughPhase(), plan), LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	//! THE LAST PHASE THIS CONFIG ACTUALLY SPANS, with the "unauthored means the required phase only"
	//! collapse already applied.
	//!
	//! ⚠ IT EXISTS SO THE AUTHORED FIELD AND THE EFFECTIVE SPAN CANNOT BE CONFUSED, and the caller that
	//! matters is the initialisation case that pins every shipped objective config's span. Asserting the
	//! raw field would pass a config that authored an EMPTY m_sThroughPhase on the strength of the line
	//! being there; asserting this refuses it, because an empty upper bound collapses the span to one
	//! phase and that IS the deadlock. The collapse is stated HERE and nowhere else, and the live
	//! predicate reads it from here too, so a reader and the machine can never disagree.
	//! \return The name of the inclusive last phase of the range.
	string EffectiveThroughPhase()
	{
		if (m_sThroughPhase == "")
			return m_sFromPhase;

		return m_sThroughPhase;
	}

	//------------------------------------------------------------------------------------------------
	//! EVERY attribute has to appear here. A module is cloned out of its config template for each
	//! deployment and CloneModule copies by hand, so a forgotten attribute silently ships the class
	//! default instead of the authored value - that is how m_fMaxCruiseSpeed was lost on the vehicle
	//! module for a whole release.
	//!
	//! What a dropped line would cost here: drop m_sFromPhase and every clone starts at the empty string,
	//! which no plan carries, so the span refuses outright - every objective deployment in the campaign
	//! is collected on its first reinforcement check, one update after it was bought; drop
	//! m_sThroughPhase and every range collapses back to a single phase, which is the 2026-08-19
	//! deadlock restored one clone at a time - the ramp's operations would be collected on the promotion
	//! tick and the counter-attack would be unreachable again; drop m_fMaxDistanceFromObjective and it
	//! clones as 0, which refuses every position but the objective's exact centre and collects every
	//! operation one update after it is created.
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_ObjectiveConditionDeploymentModule clone = new OVT_ObjectiveConditionDeploymentModule();

		clone.m_sModuleName = m_sModuleName;
		clone.m_sFromPhase = m_sFromPhase;
		clone.m_sThroughPhase = m_sThroughPhase;
		clone.m_fMaxDistanceFromObjective = m_fMaxDistanceFromObjective;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Objective Condition Module: %1", m_sModuleName));
		Print(string.Format("  First Phase: %1", m_sFromPhase));
		Print(string.Format("  Through Phase: %1", EffectiveThroughPhase()));
		Print(string.Format("  Max Distance From Objective: %1m", m_fMaxDistanceFromObjective));
	}
}
