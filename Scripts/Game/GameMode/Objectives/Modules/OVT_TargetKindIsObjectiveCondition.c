//------------------------------------------------------------------------------------------------
//! "THIS PLAN'S GATE ONLY APPLIES TO THIS KIND OF PLACE." The port of the kind fork that used to sit
//! inside the hard-coded forward-base gate: "a BASE objective takes the sabotage-counter branch;
//! anything that is not a TOWN refuses outright".
//!
//! ⚠ IT IS A CONDITION, SO IT GATES THE ADVANCE - NOT THE OPERATIONS. The other half of the shipped
//! fork lived in the SENDERS ("towns only" / "bases only"), and that half is
//! OVT_SendDeploymentObjectiveOperation.m_iRequiredTargetKind. Both halves are needed and they are not
//! the same question: one decides whether the phase may end, the other decides whether an operation may
//! be bought.
//!
//! ⚠ AND'D WITH THE PHASE'S OTHER CONDITIONS, so a plan authors it beside its real gate: the town
//! doctrine advances on "the town is soft AND it is a town", the base doctrine on "a mission landed AND
//! it is a base". On the shipped registry the kind always matches - each plan's selector claims a
//! disjoint candidate source - so this conjunct is always true in play. It is authored anyway because a
//! commit that did NOT come from a selection round (a restore, a scripted scenario, a fixture) can hand
//! a plan a kind it has no doctrine for, and refusing to advance is the honest answer to that.
//!
//! ⚠ Evaluate() IS SIDE-EFFECT FREE.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_TargetKindIsObjectiveCondition : OVT_BaseObjectiveConditionModule
{
	[Attribute(defvalue: "1", uiwidget: UIWidgets.ComboBox, desc: "The kind of place this plan's gate applies to. 1 = TOWN, 2 = BASE. 0 (NONE) matches nothing, because an objective in that state does not exist", enums: ParamEnumArray.FromEnum(OVT_EObjectiveKind))]
	int m_iRequiredKind;

	//------------------------------------------------------------------------------------------------
	//! \return True when the objective is the authored kind.
	override bool Evaluate()
	{
		OVT_ObjectiveInstance objective = GetObjective();
		if (!objective || !objective.IsLive())
			return false;

		if (m_iRequiredKind == OVT_EObjectiveKind.NONE)
			return false;

		return objective.GetTargetKind() == m_iRequiredKind;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty prefix: the condition keeps no state.
	override string GetBagPrefix()
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 EVERY ATTRIBUTE, BY HAND, INCLUDING THE PARENT'S. Not chained, silently drops what it forgets.
	//!
	//! What a dropped line costs here: drop m_iRequiredKind and every clone reads 0 = NONE, which matches
	//! nothing - so the phase this condition sits in can never advance and the objective runs out its
	//! idle clock and is blacklisted, every time, for every plan.
	//! \return An independent copy.
	override OVT_BaseObjectiveModule CloneModule()
	{
		OVT_TargetKindIsObjectiveCondition clone = new OVT_TargetKindIsObjectiveCondition();

		clone.m_sModuleName = m_sModuleName;
		clone.m_iRequiredKind = m_iRequiredKind;

		return clone;
	}
}
