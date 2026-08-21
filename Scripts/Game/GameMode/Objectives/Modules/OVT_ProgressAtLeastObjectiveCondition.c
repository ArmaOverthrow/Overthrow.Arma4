//------------------------------------------------------------------------------------------------
//! "THE RAMP HAS BANKED ENOUGH COMPLETED WORK." One bag counter against a required count.
//!
//! The port of BasePhase2Gate() - one completed sabotage mission raises the forward base - and the
//! sabotage half of BasePhase3Gate() one phase later, over a bag key instead of a named field.
//!
//! ⚠ WHY THE COUNTER IS ENOUGH HERE WHEN IT IS NOT ENOUGH FOR A TOWN. The town gate needs a second,
//! causal conjunct (see OVT_SupportBelowObjectiveCondition) because a town can ALREADY be under its
//! support threshold at the moment it is chosen, so "the town is soft" is not evidence that "this ramp
//! softened it" and the gate could fire on the phase's own entry tick. A sabotage counter cannot do
//! that: committing an objective zeroes the bag, nothing in the campaign but a completed sabotage
//! mission raises it, and one has to have been sent, driven to the base, held it unopposed and
//! demolished something to raise it once. THE COUNTER IS THE WORLD FACT HERE - which is also why it
//! round-trips a save correctly with no session-local state to rebuild.
//!
//! 🔴 THE SIGNAL IS PULLED, NEVER PUSHED. The behaviour module that completes a mission calls
//! objective.Report(key, +1) and does nothing else - it does not advance, does not reset and does not
//! re-arm a timer. This condition is asked on the tick, like every other decision in the machine.
//! Reversing that cost two red cases in two suites once already.
//!
//! ⚠ Evaluate() IS SIDE-EFFECT FREE: it reads the bag and writes nothing.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_ProgressAtLeastObjectiveCondition : OVT_BaseObjectiveConditionModule
{
	[Attribute(defvalue: "sabotage.successes", desc: "Which bag counter to read. The shipped keys are 'sabotage.successes' and 'harassment.successes', both written by the deployment-side behaviour module that completes an operation")]
	string m_sBagKey;

	[Attribute(defvalue: "1", desc: "How many completed operations the gate demands, AT LEAST. 1 is the shipped forward-base gate. -1 = the campaign's objectiveSabotageMissionsRequired difficulty setting, which is INVERTED across the presets on purpose (Easy demands six, Insane two, so a new player gets more warning) and is what the counter-attack gate one phase later uses")]
	int m_iRequired;

	//------------------------------------------------------------------------------------------------
	//! \return True when the counter has reached the required count.
	override bool Evaluate()
	{
		OVT_ObjectiveInstance objective = GetObjective();
		if (!objective || !objective.IsLive())
			return false;

		return MeetsRequirement(objective.Get(m_sBagKey));
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a banked count clears the authored (or difficulty-derived) requirement.
	//! \param[in] banked What the bag counter reads.
	//! \return True when the gate is satisfied.
	bool MeetsRequirement(int banked)
	{
		return banked >= ResolveRequired();
	}

	//------------------------------------------------------------------------------------------------
	//! The required count, authored or from difficulty.
	//!
	//! ⚠ THE CLAMP IS RequiredSabotageMissions()'s, NOT A SECOND ONE. That static already refuses a zero,
	//! a negative and an absurd value (a mis-keyed 40 for 4 would take an in-game week and reads in play
	//! as "the occupying faction never counter-attacks"), so the difficulty path goes through it and the
	//! authored path is honoured verbatim - "absurd" is domain knowledge an authored override may have
	//! and this module does not.
	//! \return At least one.
	int ResolveRequired()
	{
		if (m_iRequired > OVT_ObjectivePlanRules.USE_DIFFICULTY)
		{
			if (m_iRequired < 1)
				return 1;

			return m_iRequired;
		}

		int authored = -1;

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (difficulty)
			authored = difficulty.objectiveSabotageMissionsRequired;

		return OVT_ObjectivePhaseRules.RequiredSabotageMissions(authored, OVT_ObjectivePhaseRules.DEFAULT_SABOTAGE_MISSIONS);
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty prefix: the condition READS a counter another module owns and writes nothing.
	override string GetBagPrefix()
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 EVERY ATTRIBUTE, BY HAND, INCLUDING THE PARENT'S. Not chained, silently drops what it forgets.
	//!
	//! What a dropped line costs here: drop m_sBagKey and every clone reads the empty key, which the bag
	//! answers zero for forever - the gate never opens and the objective runs out its idle clock; drop
	//! m_iRequired and the clone reads 0, which is floored to 1 rather than to "no gate at all", so the
	//! phase advances on the FIRST completed mission whatever the author demanded.
	//! \return An independent copy.
	override OVT_BaseObjectiveModule CloneModule()
	{
		OVT_ProgressAtLeastObjectiveCondition clone = new OVT_ProgressAtLeastObjectiveCondition();

		clone.m_sModuleName = m_sModuleName;
		clone.m_sBagKey = m_sBagKey;
		clone.m_iRequired = m_iRequired;

		return clone;
	}
}
