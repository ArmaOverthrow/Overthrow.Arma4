//------------------------------------------------------------------------------------------------
//! "THE TOWN HAS BEEN SOFTENED, AND THIS RAMP IS WHAT SOFTENED IT."
//!
//! The port of the hard-coded forward-base gate's town half, and it is TWO CONJUNCTS, not one.
//!
//! =========================================================================================
//! 🔴 THE WORLD-FACT CONJUNCT, AND WHY IT IS IN THIS MODULE AND NOT IN THE PURE STATIC
//! =========================================================================================
//! The plan's diagram draws this transition as "town: support < 50 %" alone. Wired literally, the gate
//! fires on the FIRST tick of the phase for any town already under the threshold, and the whole
//! harassment phase is skipped:
//!
//!   - IT BREAKS THE REASON THE FEATURE EXISTS. "Between the first harassment group arriving and the
//!     counter-QRF there are tens of in-game minutes of visible activity" cannot be true of a ramp that
//!     can advance before it has sent anything. A collapsed town is already rewarded by SELECTION,
//!     which scores low support heavily - the prize is being CHOSEN, not being allowed to skip a phase.
//!   - IT MAKES PROGRESS UNATTRIBUTABLE. "Why did the occupying faction move to this phase" must always
//!     have an answer, and "the town was already like that" is not one.
//!   - IT IS WHAT MADE THE PHASE TIMEOUT LOOK LIKE IT RE-ARMED. A gate that fires on the entry tick
//!     advances the phase, which legitimately re-arms the idle clock - so a planted countdown read back
//!     as a fresh one and the tick looked like it had failed to decrement. The re-arm was never the bug;
//!     firing on that tick at all was.
//!
//! ⚠ AND THE SECOND CONJUNCT IS THE TOWN CARRYING THIS RAMP'S OWN DEBUFF - NOT THE SUCCESS COUNTER.
//! Both were tried and the counter is not enough. A success counter is a plain integer that any caller
//! may raise, so it records that operations were REPORTED, not that anything happened in the world; a
//! fixture arranging a mid-ramp state by bumping it three times satisfies a counter test while the town
//! it names has never been touched. The MODIFIER is the causal link itself: the town is below the
//! threshold BECAUSE this ramp put a stack of it there. Nothing else in the campaign applies that
//! modifier, so nothing else can open this gate. It is also the right answer for a RESTORE, which the
//! counter is not - town modifiers are persisted, so a campaign loaded mid-ramp still carries the debuff
//! and still qualifies, with no session-local "have I sent one yet" state to rebuild.
//!
//! ⚠ THE CONJUNCT IS HERE, NOT IN OVT_ObjectivePhaseRules.TownPhase2Gate. That static answers ONE
//! question - "is this town soft enough" - it is a function of its arguments and nothing else, and its
//! signature is pinned by Logic cases on both sides of the threshold. "Did this ramp do it" is a
//! question about the WORLD, which the pure tier may not ask, so folding it in would change a settled
//! contract to fix a different layer's mistake.
//!
//! NOT A WEDGE RISK. If the debuff has expired the gate refuses, the ramp sends another operation, and
//! that operation re-applies it - and if the ramp cannot make progress at all, the phase's idle abort
//! gives up and blacklists, loudly. There is no path here that stops without a log line.
//! =========================================================================================
//!
//! ⚠ Evaluate() IS SIDE-EFFECT FREE. It reads the town manager and the modifier system and writes
//! nothing; the transition is the runner's, behind the tick's three early returns.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_SupportBelowObjectiveCondition : OVT_BaseObjectiveConditionModule
{
	[Attribute(defvalue: "-1", desc: "Advance once the town's support for the resistance is STRICTLY BELOW this percentage. -1 = the ported forward-base threshold, OVT_ObjectivePhaseRules.TownPhase2Gate (50 %). The counter-attack gate one phase later is 25")]
	int m_iSupportThreshold;

	[Attribute(defvalue: "ObjectiveHarassment", desc: "THE WORLD-FACT CONJUNCT: the town must currently be carrying at least one stack of this support modifier, i.e. THIS ramp is what pushed it down. Must match the name on the harassment behaviour module and on the entry in Configs/Modifiers/supportModifiers.conf. Empty disables the conjunct, which lets the gate fire on its own entry tick - see the class header before authoring that")]
	string m_sRequiredTownModifier;

	//------------------------------------------------------------------------------------------------
	//! \return True when both conjuncts hold. FALSE when there is no town to ask, which is what a base
	//!         objective and a world with no town manager both are.
	override bool Evaluate()
	{
		OVT_ObjectiveInstance objective = GetObjective();
		if (!objective || !objective.IsLive())
			return false;

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
			return false;

		OVT_TownData town = towns.GetNearestTown(objective.GetTargetPosition());
		if (!town)
			return false;

		// THE RAMP HAS TO HAVE DONE IT. See the class header.
		if (!CarriesRequiredModifier(towns, town))
			return false;

		return MeetsThreshold(town.SupportPercentage());
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a support percentage clears the authored (or ported) threshold.
	//!
	//! STRICTLY BELOW, so a town sitting exactly on the threshold has not qualified yet. The gate is
	//! crossed by the ramp pushing support DOWN, so the strict test means the last debuff has to actually
	//! land rather than merely reach the line.
	//! \param[in] supportPercentage The town's current support for the resistance, 0-100.
	//! \return True when the support half of the gate is satisfied.
	bool MeetsThreshold(int supportPercentage)
	{
		if (m_iSupportThreshold <= OVT_ObjectivePlanRules.USE_DIFFICULTY)
			return OVT_ObjectivePhaseRules.TownPhase2Gate(supportPercentage);

		return supportPercentage < m_iSupportThreshold;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the objective town is carrying the debuff THIS ramp applies.
	//!
	//! Read the same way OVT_MapInfluenceLayer reads it: resolve the name to an index through the
	//! modifier system (the index is what the replicated per-town list carries) and look for that index
	//! on the town.
	//! \param[in] towns The town manager.
	//! \param[in] town The objective town.
	//! \return True when the town carries at least one stack, or when no modifier is required.
	protected bool CarriesRequiredModifier(notnull OVT_TownManagerComponent towns, notnull OVT_TownData town)
	{
		if (m_sRequiredTownModifier == "")
			return true;

		if (!town.supportModifiers)
			return false;

		OVT_TownModifierSystem system = towns.GetModifierSystem(OVT_TownSupportModifierSystem);
		if (!system)
			return false;

		int index = system.GetModifierIndexByName(m_sRequiredTownModifier);
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
	//! \return An empty prefix: the condition keeps no state, it reads the world.
	override string GetBagPrefix()
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 EVERY ATTRIBUTE, BY HAND, INCLUDING THE PARENT'S. Not chained, silently drops what it forgets.
	//!
	//! What a dropped line costs here: drop m_iSupportThreshold and every clone reads 0, which the
	//! sentinel test below treats as an authored threshold of zero - a gate no town can ever cross, so
	//! the ramp runs until its idle clock abandons it; drop m_sRequiredTownModifier and the clone's
	//! conjunct is disabled, which is the entry-tick advance this whole class exists to prevent.
	//! \return An independent copy.
	override OVT_BaseObjectiveModule CloneModule()
	{
		OVT_SupportBelowObjectiveCondition clone = new OVT_SupportBelowObjectiveCondition();

		clone.m_sModuleName = m_sModuleName;
		clone.m_iSupportThreshold = m_iSupportThreshold;
		clone.m_sRequiredTownModifier = m_sRequiredTownModifier;

		return clone;
	}
}
