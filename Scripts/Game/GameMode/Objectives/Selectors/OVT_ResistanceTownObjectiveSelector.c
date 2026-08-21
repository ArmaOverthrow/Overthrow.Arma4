//------------------------------------------------------------------------------------------------
//! THE TOWN DOCTRINE'S TARGET: every resistance-held town and city, scored on size, on how far its
//! support has collapsed, on how close it is to ground the occupying faction still holds, and on
//! whether a broadcast still reaches it.
//!
//! ⚠ THIS IS THE SHIPPED SCORER, LIFTED FROM CONSTANTS TO ATTRIBUTES, AND THE DEFAULTS ARE THE
//! CONSTANTS (D10). Every defvalue below is the value OVT_ObjectiveSelection has carried since the
//! counter-attacks build - 40 / 400 / 30 / 25 / 10 - so a registry that authors nothing behaves
//! exactly as the single-list machine did, term for term and in the same order of addition. The
//! statics keep their constants and are NOT edited; this class reproduces the same sum from the same
//! shared helpers (Clamp01, ProximityScore) so that a tuner can move one number in a .conf without a
//! script change, and so that the two scales stay comparable:
//!   a town saturates at  population + collapse + proximity + coverage
//!   a base saturates at  prize + threat + proximity + coverage
//! and those two totals are equal ON PURPOSE, because towns and bases still compete for one
//! objective. If a server owner raises one ceiling and not the other, the loser simply stops being
//! picked - which is worth knowing before turning a knob, and is why every desc: below says what the
//! number is worth rather than merely what it is.
//!
//! ⚠ VILLAGES ARE NOT CANDIDATES AND THIS CLASS NEVER SEES ONE. The exclusion lives in the candidate
//! collection (OVT_ObjectiveCandidateSet.AddResistanceTowns) because it is a statement about what the
//! world offers, not about what a doctrine values. Forward bases and radio towers are likewise never
//! candidates: a forward base is the occupying faction's own, and a tower is handled WITHIN an
//! objective.
//!
//! ⚠ IT IS STATELESS AND IS NEVER CLONED, WHICH IS WHY IT HAS NO CloneModule(). A phase's MODULES are
//! cloned per objective because they latch state; a selector is arithmetic over its arguments and the
//! plan's one authored instance is read by every round of every campaign in the session. That also
//! means it carries no dropped-line hazard and needs no clone-fidelity case - the trap that costs
//! every module class one.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_ResistanceTownObjectiveSelector : OVT_ObjectiveTargetSelector
{
	[Attribute(defvalue: "40", desc: "The biggest prize a town can be, at the population reference or more. Ships at 40 - together with the collapse, proximity and coverage weights it sets the town ceiling, which is deliberately equal to the base ceiling because towns and bases compete for ONE objective")]
	protected float m_fPopulationWeight;

	[Attribute(defvalue: "400", desc: "Population that counts as a full-value prize. Above it the size term saturates, so a capital does not outrank every other consideration put together. Ships at 400. Zero or less makes any inhabited town count as full value")]
	protected float m_fPopulationReference;

	[Attribute(defvalue: "30", desc: "What a fully collapsed town (0% support for the resistance) is worth ON TOP of its size. Ships at 30. This is where the retired town-suppression trigger went: a collapsed town is no longer an instant battle, it is a high-weight objective the ramp then works toward in the open")]
	protected float m_fSupportCollapseWeight;

	[Attribute(defvalue: "25", desc: "What being right next door to ground the occupying faction still holds is worth. Falls off linearly to zero at the maximum useful distance. Ships at 25")]
	protected float m_fProximityWeight;

	[Attribute(defvalue: "10", desc: "What still being able to broadcast over the town is worth. Small on purpose - it breaks ties between otherwise comparable targets rather than deciding the campaign. Ships at 10")]
	protected float m_fTowerCoverageWeight;

	[Attribute(defvalue: "-1", desc: "Distance (m) at which this doctrine's proximity term reaches zero. -1 = the director's own m_fMaxUsefulDistance, which is what every shipped plan authors and what the single-list machine used")]
	protected float m_fMaxUsefulDistance;

	//------------------------------------------------------------------------------------------------
	//! Assigns the shipped weights explicitly, for a selector built in code rather than loaded from a
	//! .conf.
	//!
	//! ⚠ `new` DOES NOT APPLY [Attribute()] DEFAULTS. Defvalues are an authored-data mechanism: a
	//! hand-built selector starts at zero on every field and would score every town identically, which
	//! is a silent wrong answer rather than a crash. The strangler fallback builds one of these, and so
	//! does any fixture that wants the shipped scorer without a registry behind it.
	//!
	//! ⚠ IT READS THE PURE STATICS' CONSTANTS RATHER THAN REPEATING THE NUMBERS, which is the ONE place
	//! in the tree that ties the authored defaults to the constants they were lifted from. Repeating
	//! them here would be a second copy to keep in step, and the drift would be invisible: the
	//! .conf-driven machine and the fallback would simply disagree about what a town is worth.
	void ApplyShippedWeights()
	{
		m_fPopulationWeight = OVT_ObjectiveSelection.TOWN_POPULATION_WEIGHT;
		m_fPopulationReference = OVT_ObjectiveSelection.POPULATION_REFERENCE;
		m_fSupportCollapseWeight = OVT_ObjectiveSelection.TOWN_SUPPORT_COLLAPSE_WEIGHT;
		m_fProximityWeight = OVT_ObjectiveSelection.PROXIMITY_WEIGHT;
		m_fTowerCoverageWeight = OVT_ObjectiveSelection.TOWER_COVERAGE_WEIGHT;
		m_fMaxUsefulDistance = -1;
	}

	//------------------------------------------------------------------------------------------------
	override int GetCandidateSources()
	{
		return OVT_EObjectiveCandidateSource.RESISTANCE_TOWNS;
	}

	//------------------------------------------------------------------------------------------------
	override string GetSelectorName()
	{
		return "resistance towns and cities";
	}

	//------------------------------------------------------------------------------------------------
	//! Scores every town in the round and writes a zero for everything else.
	//! \param[in] candidates The round's shared candidate set.
	//! \param[out] scores One score per candidate, in the set's order.
	//! \return True when at least one town was in the set.
	override bool ScoreCandidates(notnull OVT_ObjectiveCandidateSet candidates, notnull array<float> scores)
	{
		scores.Clear();

		float maxUseful = ResolveMaxUsefulDistance(m_fMaxUsefulDistance, candidates);

		bool claimed = false;

		int count = candidates.Count();
		for (int i = 0; i < count; i++)
		{
			if (candidates.GetKind(i) != OVT_EObjectiveKind.TOWN)
			{
				// A zero, not a skip: the arrays are parallel and the mask - not the score - is what
				// says "not mine". See the base class's contract, point 2.
				scores.Insert(0);
				continue;
			}

			scores.Insert(ScoreTown(candidates.GetPopulation(i), candidates.GetSupportPercentage(i), candidates.GetReach(i), maxUseful, candidates.HasTowerCoverage(i)));
			claimed = true;
		}

		return claimed;
	}

	//------------------------------------------------------------------------------------------------
	//! What one resistance-held town or city is worth as the next objective.
	//!
	//! ⚠ THE TERMS ARE SUMMED IN THIS ORDER AND THE ORDER IS PART OF THE PARITY CLAIM. Floating-point
	//! addition is not associative, so reproducing OVT_ObjectiveSelection.ScoreTown() bit for bit means
	//! reproducing size + collapse + reach + coverage in that sequence. The initialisation tier's
	//! parity case compares this against that static on a real fixture and would see any drift.
	//! \param[in] population Inhabitants. Saturates at the population reference; negative reads as none.
	//! \param[in] supportPercentage Support for the resistance, 0-100. Outside that band it is clamped.
	//! \param[in] reach Distance to the nearest base the occupying faction still holds.
	//! \param[in] maxUsefulDistance Distance at which the proximity term reaches zero.
	//! \param[in] hasTowerCoverage Whether an occupying broadcast still reaches the town.
	//! \return A score on the shared scale. Never negative for non-negative weights.
	protected float ScoreTown(int population, int supportPercentage, float reach, float maxUsefulDistance, bool hasTowerCoverage)
	{
		float sizeTerm = m_fPopulationWeight * SaturatingFraction(population, m_fPopulationReference);

		// COLLAPSED SUPPORT IS THE HIGH-VALUE SIGNAL, so the term is inverted: 0 % support pays the
		// full weight, 100 % pays nothing.
		float collapseTerm = m_fSupportCollapseWeight * (1.0 - OVT_ObjectiveSelection.Clamp01(supportPercentage / 100.0));

		float reachTerm = m_fProximityWeight * OVT_ObjectiveSelection.ProximityScore(reach, maxUsefulDistance);

		float coverageTerm = 0;
		if (hasTowerCoverage)
			coverageTerm = m_fTowerCoverageWeight;

		return sizeTerm + collapseTerm + reachTerm + coverageTerm;
	}

	//------------------------------------------------------------------------------------------------
	//! A value as a fraction of a saturating reference, with a non-positive reference meaning
	//! "everything saturates".
	//!
	//! ⚠ THE ZERO-REFERENCE BRANCH EXISTS BECAUSE THE REFERENCE IS NOW AUTHORED. As a compiled constant
	//! it could never be zero; as a .conf field a tuner can type one, and dividing by it would take the
	//! whole campaign down on the next selection round. Saturating is the honest reading of "there is no
	//! population above which this stops mattering".
	//! \param[in] value The measured value. Negative reads as none.
	//! \param[in] saturationPoint The saturation point. Non-positive saturates any positive value.
	//! \return A fraction in [0, 1].
	protected float SaturatingFraction(int value, float saturationPoint)
	{
		if (saturationPoint <= 0)
		{
			if (value > 0)
				return 1;

			return 0;
		}

		return OVT_ObjectiveSelection.Clamp01(value / saturationPoint);
	}
}
