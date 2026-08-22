//------------------------------------------------------------------------------------------------
//! A PLAN: one doctrine the occupying faction may commit to, expressed as authored data.
//!
//! The objectives framework's answer to OVT_DeploymentConfig one layer down. A deployment config says
//! "here is a thing that can exist"; a plan says "here is a campaign of work against a place, in
//! phases". Two ship - a town offensive and a base offensive - and a third built from shipped modules
//! needs no EnforceScript at all, which is goal G1 of this feature and the only reason the indirection
//! is worth paying for.
//!
//! ⚠ m_sObjectiveName IS THE PERSISTENCE KEY, exactly as m_sDeploymentName is for a deployment
//! (C8). The .conf FILE name is a hint the registry attaches to a GUID for readability; renaming the
//! file costs one line in overthrowObjectives.conf and orphans nothing, while renaming THIS abandons
//! every saved objective running the plan. An unknown plan name on load is logged by name, discarded
//! and re-selected.
//!
//! ⚠ m_fPriority IS A FLOAT MULTIPLIER WHERE HIGHER WINS, WHICH IS THE OPPOSITE OF A DEPLOYMENT
//! CONFIG'S m_iPriority (D8). Two conventions in one tree is a real cost, paid deliberately: silently
//! inverting a convention under the same field name produces a plan that never runs and a report
//! nobody can reproduce, so the type and the prefix differ to make the difference visible in the
//! .conf itself. See OVT_ObjectivePlanRules.ResolvePlanScore().
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sObjectiveName")]
class OVT_ObjectiveConfig : ScriptAndConfig
{
	[Attribute(desc: "Stable name of this plan. THE PERSISTENCE KEY - it travels in the save payload, so renaming it abandons every saved objective running it. The .conf file name is only a hint and is safe to change")]
	string m_sObjectiveName;

	[Attribute("1", UIWidgets.Flags, enums: ParamEnumArray.FromEnum(OVT_FactionTypeFlag), desc: "Which factions may run this plan. Only the occupying faction has a director today; the flag is authored the same way a deployment config authors it so a second director costs no format change")]
	OVT_FactionTypeFlag m_iAllowedFactionTypes;

	[Attribute(defvalue: "1", desc: "MULTIPLIER on this plan's selector score when plans are compared. HIGHER WINS - the opposite convention to a deployment config's m_iPriority, deliberately. 1 = score me on my selector alone; 2 = worth twice as much as an equally-scored rival; 0 = ship this doctrine but never run it")]
	float m_fPriority;

	[Attribute(defvalue: "100", desc: "Percent chance this plan is considered at all on a selection round (0-100). 100 = always considered. A roll below it does not pick the plan, it only lets it compete")]
	float m_fChance;

	[Attribute(defvalue: "1", desc: "How many objectives may run this plan at once. The campaign ships with one objective in total, so this is a headroom knob rather than a tuning one")]
	int m_iMaxInstances;

	//! WHAT THIS PLAN ATTACKS - the modder seam for target. ONE per plan, and a plan without one is
	//! named by the validator and skipped: a doctrine that cannot say what it is for cannot compete for
	//! an objective, and letting it sit in the registry scoring nothing would be a plan that silently
	//! never runs.
	//!
	//! ⚠ THE SELECTOR DECLARES WHICH CANDIDATE SOURCES IT READS, AND THAT DECLARATION IS WHAT MAKES
	//! N PLANS COST ONE PASS OVER THE WORLD (D6). Selection collects the union of every eligible plan's
	//! sources exactly once per round and hands the same set to every selector.
	[Attribute(desc: "What this plan attacks, and what it thinks each candidate is worth. Ship OVT_ResistanceTownObjectiveSelector or OVT_ResistanceBaseObjectiveSelector unless a doctrine needs a target the campaign has never attacked. REQUIRED - a plan with none is skipped by the validator")]
	ref OVT_ObjectiveTargetSelector m_Selector;

	[Attribute(desc: "Ordered phases. Index 0 is entered the moment the plan is committed to; each later one is reached by the phase before it passing ALL of its condition modules")]
	ref array<ref OVT_ObjectivePhase> m_aPhases;

	//------------------------------------------------------------------------------------------------
	void OVT_ObjectiveConfig()
	{
		if (!m_aPhases)
			m_aPhases = new array<ref OVT_ObjectivePhase>();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a faction may run this plan.
	//! \param[in] factionType The faction type to test.
	//! \return True when the plan's flags include it.
	bool CanFactionUse(OVT_FactionType factionType)
	{
		switch (factionType)
		{
			case OVT_FactionType.OCCUPYING_FACTION: return (m_iAllowedFactionTypes & OVT_FactionTypeFlag.OCCUPYING_FACTION) != 0;
			case OVT_FactionType.RESISTANCE_FACTION: return (m_iAllowedFactionTypes & OVT_FactionTypeFlag.RESISTANCE_FACTION) != 0;
			case OVT_FactionType.SUPPORTING_FACTION: return (m_iAllowedFactionTypes & OVT_FactionTypeFlag.SUPPORTING_FACTION) != 0;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Which candidate sources this plan's selector reads.
	//! \return OVT_EObjectiveCandidateSource flags, or 0 when the plan authors no selector.
	int GetCandidateSources()
	{
		if (!m_Selector)
			return 0;

		return m_Selector.GetCandidateSources();
	}

	//------------------------------------------------------------------------------------------------
	//! One phase by index.
	//! \param[in] index Index into m_aPhases.
	//! \return The phase, or null when the index is out of range.
	OVT_ObjectivePhase GetPhase(int index)
	{
		if (!m_aPhases || index < 0 || index >= m_aPhases.Count())
			return null;

		return m_aPhases[index];
	}

	//------------------------------------------------------------------------------------------------
	//! How many phases the plan has.
	//! \return The phase count.
	int GetPhaseCount()
	{
		if (!m_aPhases)
			return 0;

		return m_aPhases.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The plan's phase names, in authored order, for OVT_ObjectivePlanRules.PhaseIndexOf().
	//!
	//! ⚠ IT ANSWERS INTO A CALLER-OWNED ARRAY rather than allocating, because the restore path and the
	//! validator both call it and neither wants an allocation per phase per objective per load.
	//! \param[out] names Receives the names in order. Cleared first.
	void GetPhaseNames(notnull array<string> names)
	{
		names.Clear();

		if (!m_aPhases)
			return;

		foreach (OVT_ObjectivePhase phase : m_aPhases)
		{
			if (!phase)
			{
				// A null entry still occupies an index, so it contributes an unmatchable name rather
				// than shifting every phase after it one place to the left.
				names.Insert("");
				continue;
			}

			names.Insert(phase.m_sPhaseName);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Where a named phase sits in this plan.
	//! \param[in] name The phase name, as persisted.
	//! \return The index, or OVT_ObjectivePlanRules.NO_PHASE_INDEX when the plan has no such phase.
	int IndexOfPhase(string name)
	{
		array<string> names = new array<string>();
		GetPhaseNames(names);

		return OVT_ObjectivePlanRules.PhaseIndexOf(names, name);
	}
}
