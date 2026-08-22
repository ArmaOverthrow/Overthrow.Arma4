//------------------------------------------------------------------------------------------------
//! "THE FACTION CAN AFFORD WHAT COMES NEXT." The occupying faction's reserve against an authored gate.
//!
//! The port of OVT_ObjectivePhaseRules.MeetsResourceGate(), the third conjunct of both halves of the
//! counter-attack gate. A battle costs the faction its reserve, and a battle started by a faction that
//! cannot pay for the waves behind it is the ramp throwing itself away.
//!
//! ⚠ AT THE GATE PASSES. The gate is what the battle is expected to cost, and demanding a margin on top
//! of it would make the authored number mean something other than what it says. That inclusive rule is
//! the pure static's and is not restated here.
//!
//! ⚠ IT READS THE FACTION'S RESERVE, NOT THE DEPLOYMENT POOL, and the two are different pots. The
//! reserve is what the occupying faction manager accumulates for battles; the deployment pool is what
//! the framework spends on groups. This condition asks about the first and the director's create-then-
//! debit choke point asks about the second, and neither is a substitute for the other.
//!
//! ⚠ Evaluate() IS SIDE-EFFECT FREE, and in particular it does NOT push the reserve floor. The floor is
//! the director's answer to an operation it wanted to buy and could not; a gate that is merely not met
//! yet has asked for nothing.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_ReserveAtLeastObjectiveCondition : OVT_BaseObjectiveConditionModule
{
	[Attribute(defvalue: "-1", desc: "How much reserve the occupying faction must hold before this conjunct is satisfied. -1 = the campaign's objectiveQRFResourceGate difficulty setting (Easy 750 up to Insane 3000). 0 or below gates nothing")]
	int m_iGate;

	//------------------------------------------------------------------------------------------------
	//! \return True when the reserve covers the gate.
	override bool Evaluate()
	{
		OVT_ObjectiveInstance objective = GetObjective();
		if (!objective || !objective.IsLive())
			return false;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return false;

		return OVT_ObjectivePhaseRules.MeetsResourceGate(occupying.m_iResources, ResolveGate());
	}

	//------------------------------------------------------------------------------------------------
	//! The gate, authored or from difficulty.
	//! \return The reserve demanded. Zero or below means no gate at all.
	int ResolveGate()
	{
		int difficultyValue = -1;

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (difficulty)
			difficultyValue = difficulty.objectiveQRFResourceGate;

		return OVT_ObjectivePlanRules.ResolveWithDifficulty(m_iGate, difficultyValue);
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty prefix: the condition reads the faction's reserve and writes nothing.
	override string GetBagPrefix()
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 EVERY ATTRIBUTE, BY HAND, INCLUDING THE PARENT'S. Not chained, silently drops what it forgets.
	//!
	//! What a dropped line costs here: drop m_iGate and the clone reads 0, which MeetsResourceGate()
	//! treats as "no gate" - so the battle fires the moment the rest of the ramp is done, with an empty
	//! reserve behind it and no waves to follow, on every difficulty.
	//! \return An independent copy.
	override OVT_BaseObjectiveModule CloneModule()
	{
		OVT_ReserveAtLeastObjectiveCondition clone = new OVT_ReserveAtLeastObjectiveCondition();

		clone.m_sModuleName = m_sModuleName;
		clone.m_iGate = m_iGate;

		return clone;
	}
}
