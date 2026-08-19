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

	//! Authored as the integer of OVT_EObjectivePhase, and it is an integer rather than the enum for
	//! the same reason the save payload holds one: the members are a wire format that may never be
	//! renumbered, and an out-of-range value here has to read as "a phase this build does not have"
	//! rather than silently landing on whichever member is nearest.
	//!
	//! THE FIRST PHASE OF THE RANGE. m_iThroughPhase is the last.
	[Attribute(defvalue: "1", uiwidget: UIWidgets.ComboBox, desc: "The FIRST objective phase this deployment belongs to. 1 = HARASSMENT, 2 = FOB, 3 = COUNTER_QRF. 0 (IDLE) matches nothing, because there is no objective to be at", enums: ParamEnumArray.FromEnum(OVT_EObjectivePhase))]
	int m_iRequiredPhase;

	//! THE LAST PHASE OF THE RANGE, INCLUSIVE.
	//!
	//! ⚠ 0 - WHICH IS BOTH THE ATTRIBUTE DEFAULT AND THE VALUE AN UNAUTHORED FIELD HOLDS - MEANS "THE
	//! REQUIRED PHASE ONLY", i.e. exactly the equality test this module used before the range existed.
	//! The two agreeing is deliberate: it does not matter whether a .conf that omits this line is given
	//! the attribute's defvalue or the script member's zero, because both say the same thing.
	//!
	//! ⚠ AUTHORING 3 (COUNTER_QRF) ON A RAMP OPERATION IS A MISTAKE, and an initialisation case refuses
	//! it for every shipped objective config. Harassment and sabotage teams walking in while the battle
	//! they were building up to is being fought are noise. It is only ever the SECOND line of defence:
	//! DirectorTick() early-returns for the whole of a live battle, so no operation can be created in
	//! that phase whatever a config says - what this bound decides is whether one already in the world
	//! is collected when the battle starts, and it must be.
	[Attribute(defvalue: "0", uiwidget: UIWidgets.ComboBox, desc: "The LAST objective phase this deployment may keep working in, inclusive. 0 (the default) means the required phase only - the pre-range behaviour. Author 2 on a Phase 1 operation so the ramp continues through the forward-base phase", enums: ParamEnumArray.FromEnum(OVT_EObjectivePhase))]
	int m_iThroughPhase;

	[Attribute(defvalue: "600", desc: "How far from the current objective this deployment may sit. Generous by design: a tower or a landing zone belonging to an objective is not AT it")]
	float m_fMaxDistanceFromObjective;

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

		if (!OVT_ObjectivePhaseRules.PhaseInRange(director.GetPhase(), m_iRequiredPhase, m_iThroughPhase))
			return false;

		return vector.Distance(position, director.GetObjectivePosition()) <= m_fMaxDistanceFromObjective;
	}

	//------------------------------------------------------------------------------------------------
	//! THE LAST PHASE THIS CONFIG ACTUALLY SPANS, with the "unauthored means the required phase only"
	//! collapse already applied.
	//!
	//! ⚠ IT EXISTS SO THE AUTHORED FIELD AND THE EFFECTIVE SPAN CANNOT BE CONFUSED, and the caller that
	//! matters is the initialisation case that pins every shipped objective config's span. Asserting the
	//! raw field would pass a config authored `m_iThroughPhase 0` on the strength of the number being
	//! there; asserting this refuses it, because 0 collapses the span to one phase and that IS the
	//! deadlock. The collapse rule itself is NOT restated here - it is
	//! OVT_ObjectivePhaseRules.EffectiveLastPhase(), the same one PhaseInRange() applies, so a reader
	//! and the live predicate can never disagree about what a config spans.
	//! \return The inclusive last phase of the range.
	int ResolveThroughPhase()
	{
		return OVT_ObjectivePhaseRules.EffectiveLastPhase(m_iRequiredPhase, m_iThroughPhase);
	}

	//------------------------------------------------------------------------------------------------
	//! EVERY attribute has to appear here. A module is cloned out of its config template for each
	//! deployment and CloneModule copies by hand, so a forgotten attribute silently ships the class
	//! default instead of the authored value - that is how m_fMaxCruiseSpeed was lost on the vehicle
	//! module for a whole release.
	//!
	//! What a dropped line would cost here: drop m_iRequiredPhase and every clone starts at 0, which
	//! PhaseInRange() reads as "authored for IDLE" and refuses outright - so every objective deployment
	//! in the campaign is collected on its first reinforcement check, one update after it was bought;
	//! drop m_iThroughPhase and every range collapses back to a single
	//! phase, which is the 2026-08-19 deadlock restored one clone at a time - the ramp's operations would
	//! be collected on the promotion tick and the counter-attack would be unreachable again; drop
	//! m_fMaxDistanceFromObjective and it clones as 0, which refuses every position but the objective's
	//! exact centre and collects every operation one update after it is created.
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_ObjectiveConditionDeploymentModule clone = new OVT_ObjectiveConditionDeploymentModule();

		clone.m_sModuleName = m_sModuleName;
		clone.m_iRequiredPhase = m_iRequiredPhase;
		clone.m_iThroughPhase = m_iThroughPhase;
		clone.m_fMaxDistanceFromObjective = m_fMaxDistanceFromObjective;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Objective Condition Module: %1", m_sModuleName));
		Print(string.Format("  First Phase: %1", typename.EnumToString(OVT_EObjectivePhase, m_iRequiredPhase)));
		Print(string.Format("  Through Phase: %1", typename.EnumToString(OVT_EObjectivePhase, ResolveThroughPhase())));
		Print(string.Format("  Max Distance From Objective: %1m", m_fMaxDistanceFromObjective));
	}
}
