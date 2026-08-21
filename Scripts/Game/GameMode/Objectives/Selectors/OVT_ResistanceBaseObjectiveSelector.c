//------------------------------------------------------------------------------------------------
//! THE BASE DOCTRINE'S TARGET: every resistance-held military base, scored on being a base at all, on
//! how much fighting is already going on around it, on how close it is to ground the occupying
//! faction still holds, and on whether a broadcast still reaches it.
//!
//! ⚠ THIS IS THE SHIPPED SCORER, LIFTED FROM CONSTANTS TO ATTRIBUTES, AND THE DEFAULTS ARE THE
//! CONSTANTS (D10). 45 / 25 / 40 / 25 / 10 are the values OVT_ObjectiveSelection has carried since
//! the counter-attacks build, and the statics are NOT edited - this class reproduces the same sum,
//! in the same order, from the same shared helpers.
//!
//! ⚠ THE PRIZE TERM IS WHY A BASE HAS NO POPULATION TERM. A base has no inhabitants, so
//! m_fBasePrizeWeight stands in for the size term and is set just ABOVE a mid-size town on purpose:
//! the occupying faction would rather retake its own barracks than lean on a village. The two
//! ceilings - prize + threat + proximity + coverage here, population + collapse + proximity +
//! coverage for a town - are deliberately equal, because towns and bases still compete for ONE
//! objective and a ceiling that drifted would quietly stop one kind ever being picked.
//!
//! ⚠ FORWARD BASES AND RADIO TOWERS ARE NOT CANDIDATES, and this class never sees one: the candidate
//! collection asks the occupying faction manager for the bases the RESISTANCE controls, which is
//! neither of those things.
//!
//! ⚠ IT IS STATELESS AND IS NEVER CLONED - see the town selector's header for why that matters and
//! why it therefore carries no clone-fidelity case.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_ResistanceBaseObjectiveSelector : OVT_ObjectiveTargetSelector
{
	[Attribute(defvalue: "45", desc: "What a base is worth for being a base at all. Ships at 45 - just above a mid-size town, so the occupying faction would rather retake its own barracks than lean on a village. It stands in for the population term a base has no equivalent of")]
	protected float m_fBasePrizeWeight;

	[Attribute(defvalue: "25", desc: "What the resistance being ACTIVE around a base is worth. Where the fighting already is, is where the counter-attack goes. Ships at 25")]
	protected float m_fThreatWeight;

	[Attribute(defvalue: "40", desc: "Campaign threat that counts as a full-value hotspot. Above it the threat term saturates. Ships at 40. Zero or less makes any threat at all count as full value")]
	protected float m_fThreatReference;

	[Attribute(defvalue: "25", desc: "What being right next door to ground the occupying faction still holds is worth. Falls off linearly to zero at the maximum useful distance. Ships at 25")]
	protected float m_fProximityWeight;

	[Attribute(defvalue: "10", desc: "What still being able to broadcast over the base is worth. Small on purpose - it breaks ties between otherwise comparable targets rather than deciding the campaign. Ships at 10")]
	protected float m_fTowerCoverageWeight;

	[Attribute(defvalue: "-1", desc: "Distance (m) at which this doctrine's proximity term reaches zero. -1 = the director's own m_fMaxUsefulDistance, which is what every shipped plan authors and what the single-list machine used")]
	protected float m_fMaxUsefulDistance;

	//------------------------------------------------------------------------------------------------
	//! Assigns the shipped weights explicitly, for a selector built in code rather than loaded from a
	//! .conf. See the town selector's equivalent for why `new` needs this at all and why the numbers
	//! are read off the pure statics rather than repeated.
	void ApplyShippedWeights()
	{
		m_fBasePrizeWeight = OVT_ObjectiveSelection.BASE_PRIZE_WEIGHT;
		m_fThreatWeight = OVT_ObjectiveSelection.BASE_THREAT_WEIGHT;
		m_fThreatReference = OVT_ObjectiveSelection.THREAT_REFERENCE;
		m_fProximityWeight = OVT_ObjectiveSelection.PROXIMITY_WEIGHT;
		m_fTowerCoverageWeight = OVT_ObjectiveSelection.TOWER_COVERAGE_WEIGHT;
		m_fMaxUsefulDistance = -1;
	}

	//------------------------------------------------------------------------------------------------
	override int GetCandidateSources()
	{
		return OVT_EObjectiveCandidateSource.RESISTANCE_BASES;
	}

	//------------------------------------------------------------------------------------------------
	override string GetSelectorName()
	{
		return "resistance bases";
	}

	//------------------------------------------------------------------------------------------------
	//! Scores every base in the round and writes a zero for everything else.
	//! \param[in] candidates The round's shared candidate set.
	//! \param[out] scores One score per candidate, in the set's order.
	//! \return True when at least one base was in the set.
	override bool ScoreCandidates(notnull OVT_ObjectiveCandidateSet candidates, notnull array<float> scores)
	{
		scores.Clear();

		float maxUseful = ResolveMaxUsefulDistance(m_fMaxUsefulDistance, candidates);

		bool claimed = false;

		int count = candidates.Count();
		for (int i = 0; i < count; i++)
		{
			if (candidates.GetKind(i) != OVT_EObjectiveKind.BASE)
			{
				// A zero, not a skip - see the base class's contract, point 2.
				scores.Insert(0);
				continue;
			}

			scores.Insert(ScoreBase(candidates.GetThreat(i), candidates.GetReach(i), maxUseful, candidates.HasTowerCoverage(i)));
			claimed = true;
		}

		return claimed;
	}

	//------------------------------------------------------------------------------------------------
	//! What one resistance-held base is worth as the next objective.
	//!
	//! ⚠ THE PRIZE TERM IS SUMMED FIRST AND THE ORDER IS PART OF THE PARITY CLAIM - see the town
	//! selector's equivalent. OVT_ObjectiveSelection.ScoreBase() returns prize + threat + reach +
	//! coverage in that sequence and so does this.
	//! \param[in] threat Campaign threat at the base. Saturates at the threat reference.
	//! \param[in] reach Distance to the nearest base the occupying faction still holds.
	//! \param[in] maxUsefulDistance Distance at which the proximity term reaches zero.
	//! \param[in] hasTowerCoverage Whether an occupying broadcast still reaches the base.
	//! \return A score on the shared scale. Never negative for non-negative weights.
	protected float ScoreBase(int threat, float reach, float maxUsefulDistance, bool hasTowerCoverage)
	{
		float threatTerm = m_fThreatWeight * SaturatingFraction(threat, m_fThreatReference);

		float reachTerm = m_fProximityWeight * OVT_ObjectiveSelection.ProximityScore(reach, maxUsefulDistance);

		float coverageTerm = 0;
		if (hasTowerCoverage)
			coverageTerm = m_fTowerCoverageWeight;

		return m_fBasePrizeWeight + threatTerm + reachTerm + coverageTerm;
	}

	//------------------------------------------------------------------------------------------------
	//! A value as a fraction of a saturating reference, with a non-positive reference meaning
	//! "everything saturates". See the town selector's equivalent for why the branch exists at all.
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
