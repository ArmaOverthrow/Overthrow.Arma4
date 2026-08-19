//------------------------------------------------------------------------------------------------
//! "ONLY AT THE CURRENT OBJECTIVE, AND ONLY WHILE IT IS IN PHASE N."
//!
//! The gate every deployment the objective director creates carries. It answers one question -
//! is this position inside the current objective's working radius, with the ramp in the phase this
//! config belongs to - and it answers it identically at creation time and at runtime.
//!
//! ================== THE SYMMETRY IS DELIBERATE, AND IT IS THE OPPOSITE OF ==================
//! ================== OVT_NoPlayersNearbyConditionDeploymentModule ===========================
//! That module's whole design is an ASYMMETRY: its creation gate tests player distance and its
//! runtime gate returns true unconditionally, because deleting a base's garrison the moment a player
//! walks in would evaporate the fight on approach. The opposite is true here. An objective operation
//! SHOULD be collected when the objective moves: the men were sent to soften a specific place for a
//! specific phase, and once the director has given that place up - or advanced past the phase that
//! wanted them - they are a force with no orders standing in a town nobody is attacking any more.
//! Paired with m_bDeleteOnConditionFail on the reinforcement module, this is the whole of the
//! "objective operations clean themselves up" path. There is no code anywhere that watches for an
//! objective ending and goes looking for its deployments; the reset path takes down what it tracked
//! and this condition collects the rest.
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
	[Attribute(defvalue: "1", uiwidget: UIWidgets.ComboBox, desc: "The objective phase this deployment belongs to. 1 = HARASSMENT, 2 = FOB, 3 = COUNTER_QRF. 0 (IDLE) matches nothing, because there is no objective to be at", enums: ParamEnumArray.FromEnum(OVT_EObjectivePhase))]
	int m_iRequiredPhase;

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
	//! \return True when there is an objective, it is in the required phase, and the position is
	//!         within m_fMaxDistanceFromObjective of it.
	bool IsAtCurrentObjective(vector position)
	{
		OVT_ObjectiveDirectorComponent director = OVT_ObjectiveDirectorComponent.GetInstance();
		if (!director)
			return false;

		if (!director.HasObjective())
			return false;

		int phase = director.GetPhase();
		if (phase != m_iRequiredPhase)
			return false;

		return vector.Distance(position, director.GetObjectivePosition()) <= m_fMaxDistanceFromObjective;
	}

	//------------------------------------------------------------------------------------------------
	//! EVERY attribute has to appear here. A module is cloned out of its config template for each
	//! deployment and CloneModule copies by hand, so a forgotten attribute silently ships the class
	//! default instead of the authored value - that is how m_fMaxCruiseSpeed was lost on the vehicle
	//! module for a whole release.
	//!
	//! What a dropped line would cost here: drop m_iRequiredPhase and every objective deployment
	//! believes it belongs to HARASSMENT, so the forward base's own garrison would be collected the
	//! moment the ramp advanced past it; drop m_fMaxDistanceFromObjective and it clones as 0, which
	//! refuses every position but the objective's exact centre and collects every operation one update
	//! after it is created.
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_ObjectiveConditionDeploymentModule clone = new OVT_ObjectiveConditionDeploymentModule();

		clone.m_sModuleName = m_sModuleName;
		clone.m_iRequiredPhase = m_iRequiredPhase;
		clone.m_fMaxDistanceFromObjective = m_fMaxDistanceFromObjective;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Objective Condition Module: %1", m_sModuleName));
		Print(string.Format("  Required Phase: %1", typename.EnumToString(OVT_EObjectivePhase, m_iRequiredPhase)));
		Print(string.Format("  Max Distance From Objective: %1m", m_fMaxDistanceFromObjective));
	}
}
