//------------------------------------------------------------------------------------------------
//! TIER B - the UNREST-DRIVEN tower recapture: the one objective-shaped operation the DIRECTOR does
//! not send.
//!
//! WHAT MAKES THIS CONFIG DIFFERENT FROM ITS TWIN, and why it needs its own cases. It is a near-copy
//! of Deployment_ObjectiveTowerRecapture.conf - same specops team, same recapture behaviour, same
//! inverted tower-control gate - but it is bought by the ORDINARY EVALUATOR rather than forced by the
//! objective director. That single difference is carried by one authored integer, m_bDirectorOnly,
//! and getting it wrong is completely silent in both directions:
//!
//!   🔴 AUTHORED 1 (the value its twin carries, and the value anyone copying that file inherits) the
//!      evaluator never sees the config at all. No error, no warning, no failed deployment - the
//!      feature simply never happens, for the whole campaign. There is nothing in a log to grep for.
//!      This is the single highest-value assertion in the file.
//!   - AUTHORED 0 on an OBJECTIVE config, which is the same mistake in reverse, the evaluator buys
//!      the director's operations out from under it and charges the pool outside the director's
//!      accounting. That defect was found by play-testing on 2026-08-19, not by any suite, and
//!      m_bDirectorOnly is the flag that was added to fix it.
//!
//! ⚠ NOTHING HERE DRIVES THE EVALUATOR. Creating a real deployment would spend the real pool and
//! mutate the shared initialisation world for every case that follows. Every claim below is read
//! straight off the authored config template, which is exactly where these particular defects live -
//! they are authoring mistakes, not runtime ones.
//!
//! NOT ASSERTED HERE, DELIBERATELY: that "specops_team" resolves for both factions (already pinned by
//! the Phase 4 registry cases - re-asserting it here would just duplicate a failure message) and
//! anything about which towns are actually in unrest, which needs a live campaign rather than a
//! config and belongs to play-testing.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! The new condition module's clone carries every authored attribute.
//!
//! WHY THIS IS ITS OWN CASE AND NOT A LINE IN THE ONE ABOVE. Every module in this framework is cloned
//! out of its config template for each deployment, by a hand-written CloneModule() - and a forgotten
//! line there does not fail, it silently ships the CLASS DEFAULT instead of the authored value. That
//! is not hypothetical: m_fMaxCruiseSpeed was lost from the vehicle module that way for a whole
//! release. On this module a dropped m_iMinSupportPercent clones as 0 and every town in range
//! qualifies; a dropped m_iMinTownSize clones as 0 and villages come back.
//!
//! ⚠ IT ASSERTS AGAINST HAND-SET VALUES, NOT THE AUTHORED CONFIG. The case builds its own template so
//! it is testing the copy, not the authoring - and so it cannot pass vacuously by comparing a config
//! value with itself. The values are non-round and distinct for the same reason.
//!
//! PROVEN ABLE TO FAIL (2026-08-20): `clone.m_iMinTownSize = m_iMinTownSize;` was deleted from
//! CloneModule(). The tree recompiled CLEAN - a missing copy is not a script error, which is the whole
//! hazard - and the case then reported "CloneModule() dropped m_iMinTownSize - expected 3, got 0. A
//! clone reading 0 lets villages trigger the operation". Line restored, tree recompiled clean, case
//! green.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_TowerUnrestRecapture_CloneCarriesEveryAttribute : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TownUnrestConditionDeploymentModule template = new OVT_TownUnrestConditionDeploymentModule();

		template.m_sModuleName = "Unrest Clone Fixture";
		template.m_fMaxDistance = 271;
		template.m_iMinSupportPercent = 63;
		template.m_iMinTownSize = 3;

		OVT_TownUnrestConditionDeploymentModule clone = OVT_TownUnrestConditionDeploymentModule.Cast(template.CloneModule());
		if (!clone)
		{
			SetFailure("OVT_TownUnrestConditionDeploymentModule.CloneModule() did not return an instance of its own class");
			return true;
		}

		if (clone.m_sModuleName != template.m_sModuleName)
		{
			SetFailure("CloneModule() dropped m_sModuleName");
			return true;
		}

		if (clone.m_fMaxDistance != template.m_fMaxDistance)
		{
			SetFailure(string.Format("CloneModule() dropped m_fMaxDistance - expected %1, got %2. A clone reading 0 can never resolve the tower its position sits on", template.m_fMaxDistance, clone.m_fMaxDistance));
			return true;
		}

		if (clone.m_iMinSupportPercent != template.m_iMinSupportPercent)
		{
			SetFailure(string.Format("CloneModule() dropped m_iMinSupportPercent - expected %1, got %2. A clone reading 0 qualifies every town in range, contented ones included", template.m_iMinSupportPercent.ToString(), clone.m_iMinSupportPercent.ToString()));
			return true;
		}

		if (clone.m_iMinTownSize != template.m_iMinTownSize)
		{
			SetFailure(string.Format("CloneModule() dropped m_iMinTownSize - expected %1, got %2. A clone reading 0 lets villages trigger the operation", template.m_iMinTownSize.ToString(), clone.m_iMinTownSize.ToString()));
			return true;
		}

		return true;
	}
}
