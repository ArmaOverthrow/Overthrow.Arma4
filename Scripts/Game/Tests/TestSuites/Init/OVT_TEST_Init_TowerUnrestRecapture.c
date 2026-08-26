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
//! The config is registered, valid, EVALUATOR-SELECTABLE, tower-scoped, and authored with both of the
//! gates that give it its meaning.
//!
//! PROVEN ABLE TO FAIL (2026-08-20): m_bDirectorOnly was flipped to 1 in
//! Deployment_TowerRecaptureUnrest.conf. The tree recompiled CLEAN (tools/compile-check.sh exit 0) -
//! a config no caller selects is not a script error, which is exactly why this case has to exist - and
//! the case then reported "'Tower Recapture (Unrest)' is authored m_bDirectorOnly 1, so the evaluator
//! will never buy it and nothing else ever creates it - the operation can never happen". Value
//! restored, tree recompiled clean, case green.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_TowerUnrestRecapture_ConfigIsWiredForTheEvaluator : SCR_AutotestCaseBase
{
	//! The registry key. FindConfigByName() matches on m_sDeploymentName, and that string is also the
	//! persistence key and what m_iMaxInstances counts, so it is a rename nobody may make casually.
	static const string CONFIG_NAME = "Tower Recapture (Unrest)";

	//! OVT_TownSize.TOWN. Anything below this is a village, which the design excludes outright.
	static const int SMALLEST_ALLOWED_TOWN_SIZE = 2;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (!deployments)
		{
			SetFailure("OVT_Global.GetDeploymentManager() is null - the deployment framework did not resolve");
			return true;
		}

		OVT_DeploymentConfig config = deployments.FindConfigByName(CONFIG_NAME);
		if (!config)
		{
			SetFailure(string.Format("'%1' is not registered in overthrowDeployments.conf - the occupying faction will never respond to a town turning against it", CONFIG_NAME));
			return true;
		}

		if (!config.IsValidConfig())
		{
			SetFailure(string.Format("'%1' does not validate", CONFIG_NAME));
			return true;
		}

		// 🔴 THE ONE THAT MATTERS. See the file header.
		if (!config.IsSelectableByEvaluator())
		{
			SetFailure(string.Format("'%1' is authored m_bDirectorOnly 1, so the evaluator will never buy it and nothing else ever creates it - the operation can never happen. This is the value its objective twin carries and the one a copy inherits", CONFIG_NAME));
			return true;
		}

		if (!config.CanUseLocationType(OVT_LocationTypeFlag.RADIO_TOWER))
		{
			SetFailure(string.Format("'%1' cannot be used at a RADIO_TOWER, so no candidate position it is ever offered will match", CONFIG_NAME));
			return true;
		}

		string gates = CheckGates(config);
		if (gates != "")
		{
			SetFailure(gates);
			return true;
		}

		string ordering = CheckRecaptureBeforeReinforcement(config);
		if (ordering != "")
		{
			SetFailure(ordering);
			return true;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Both condition modules, and the authored values that give each one its meaning.
	//! \param[in] config The config under test.
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckGates(notnull OVT_DeploymentConfig config)
	{
		bool foundControl = false;
		bool foundUnrest = false;

		foreach (OVT_BaseDeploymentModule module : config.m_aModules)
		{
			OVT_RadioTowerControlConditionDeploymentModule control = OVT_RadioTowerControlConditionDeploymentModule.Cast(module);
			if (control)
			{
				foundControl = true;

				// The inversion. Authored 1 this sends recapture teams only to towers already held.
				if (control.m_bRequireControl)
					return "the unrest recapture config's radio tower control condition must be authored m_bRequireControl 0 - authored 1 it deploys only to towers the occupying faction already holds, and nothing ever collects the team";

				continue;
			}

			OVT_TownUnrestConditionDeploymentModule unrest = OVT_TownUnrestConditionDeploymentModule.Cast(module);
			if (!unrest)
				continue;

			foundUnrest = true;

			if (unrest.m_iMinTownSize < SMALLEST_ALLOWED_TOWN_SIZE)
				return string.Format("the unrest condition is authored m_iMinTownSize %1, which lets VILLAGES trigger a specops recapture - the design is a town or city", unrest.m_iMinTownSize.ToString());

			// Zero or negative would make every town in range qualify, including contented ones, which
			// turns "respond to unrest" into "garrison every tower forever".
			if (unrest.m_iMinSupportPercent <= 0)
				return string.Format("the unrest condition is authored m_iMinSupportPercent %1, so every town in range qualifies and the operation fires whatever the population thinks", unrest.m_iMinSupportPercent.ToString());

			if (unrest.m_fMaxDistance <= 0)
				return "the unrest condition is authored a non-positive m_fMaxDistance, so it can never resolve the tower its candidate position sits on";
		}

		if (!foundControl)
			return "the unrest recapture config authors no radio tower control condition, so nothing collects the team once the tower flips and the deployment stands for the rest of the campaign";

		if (!foundUnrest)
			return "the unrest recapture config authors no OVT_TownUnrestConditionDeploymentModule, so it is an unconditional tower recapture - it would fire at every enemy-held tower on the map regardless of what any town thinks";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The recapture behaviour must be updated BEFORE the reinforcement module.
	//!
	//! Module order is update order, and .conf files cannot carry comments to say so. Ordered after
	//! reinforcement, the tower flip and the condition failure it causes land in the same pass, and the
	//! deployment can be collected on the very tick it succeeds - the team is deleted mid-capture.
	//! \param[in] config The config under test.
	//! \return An empty string when the order held, or why it did not.
	protected string CheckRecaptureBeforeReinforcement(notnull OVT_DeploymentConfig config)
	{
		int recaptureIndex = -1;
		int reinforcementIndex = -1;

		for (int i = 0; i < config.m_aModules.Count(); i++)
		{
			OVT_BaseDeploymentModule module = config.m_aModules[i];

			if (recaptureIndex == -1 && OVT_TowerRecaptureBehaviorDeploymentModule.Cast(module))
				recaptureIndex = i;

			if (reinforcementIndex == -1 && OVT_ReinforcementBehaviorDeploymentModule.Cast(module))
				reinforcementIndex = i;
		}

		if (recaptureIndex == -1)
			return "the unrest recapture config authors no OVT_TowerRecaptureBehaviorDeploymentModule, so its team holds the mast and never takes it";

		if (reinforcementIndex == -1)
			return "";

		if (recaptureIndex > reinforcementIndex)
			return "the unrest recapture config updates its reinforcement module BEFORE its recapture behaviour - module order is update order, so the deployment can be collected on the same pass it succeeds";

		return "";
	}
}

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
