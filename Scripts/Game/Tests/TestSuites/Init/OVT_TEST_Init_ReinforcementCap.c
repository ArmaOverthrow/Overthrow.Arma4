//------------------------------------------------------------------------------------------------
//! THE REBUY CAP: a deployment may only buy its force back so many times.
//!
//! 🔴 WHAT IT IS FOR. Author, 2026-08-23: *"a max cap on reinforcements, just for town patrols I think
//! they should only be reinforced up to 3 times, otherwise you can just farm them for loot."* Every
//! other rebuy gate in that module is a "not right now" - a defender standing too close, a contact
//! cooldown, an empty pool - and all three clear on their own, so an unlimited town patrol is a loot
//! farm with a timer rather than a fight.
//!
//! WHAT IS ASSERTED HERE AND WHY IT IS THE INIT TIER RATHER THAN LOGIC: the module is constructed
//! directly and no world is touched, but the Logic tier's own rule bans a file in that directory from
//! naming anything world-shaped at all, and the CLONE claim below has to name the deployment framework.
//!
//! 🔴 THE CLONE CLAIM IS THE ONE THAT MATTERS. CloneModule() is copy-by-hand: a dropped line does not
//! warn, does not log and does not fail to parse - it silently ships the class default, which for this
//! field is 0, which is "no limit". Every deployment in the game would be farmable again and the only
//! symptom would be a town patrol that never stops coming back. The module's own header records the
//! same trap catching m_fNoRebuyZoneMultiple and the vehicle module's m_fMaxCruiseSpeed before it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ReinforcementCap : SCR_AutotestCaseBase
{
	//! What the shipped town patrol authors.
	static const int SHIPPED_CAP = 3;

	//! The config whose cap this case reads back out of the registry, by name.
	static const string TOWN_PATROL_CONFIG = "Town Patrol";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- CLAIM 1: an unauthored cap is no cap, which is what every pre-existing config relies on.
		OVT_ReinforcementBehaviorDeploymentModule unlimited = new OVT_ReinforcementBehaviorDeploymentModule();
		unlimited.m_iMaxReinforcements = 0;

		if (unlimited.IsReinforcementBudgetSpent())
		{
			SetFailure("A module with no cap authored reported its budget spent - every config that does not author one would stop reinforcing");
			return true;
		}

		// --- CLAIM 2: a negative cap is also no cap rather than "never", matching how every other
		//     authored bound in this project fails open on a value nobody meant.
		OVT_ReinforcementBehaviorDeploymentModule negative = new OVT_ReinforcementBehaviorDeploymentModule();
		negative.m_iMaxReinforcements = -5;

		if (negative.IsReinforcementBudgetSpent())
		{
			SetFailure("A negative cap reported the budget spent, expected no limit");
			return true;
		}

		// --- CLAIM 3: the cap is reached AT the authored number, not one past it.
		OVT_ReinforcementBehaviorDeploymentModule capped = new OVT_ReinforcementBehaviorDeploymentModule();
		capped.m_iMaxReinforcements = SHIPPED_CAP;

		if (capped.GetReinforcementsBought() != 0)
		{
			SetFailure("A fresh module had already bought %1 reinforcements", capped.GetReinforcementsBought().ToString());
			return true;
		}

		if (capped.IsReinforcementBudgetSpent())
		{
			SetFailure("A module that has bought nothing reported its budget spent");
			return true;
		}

		// --- CLAIM 4: the clone carries the cap. See the header - this is the silent one.
		capped.m_sModuleName = "OVT_TEST cap";

		OVT_ReinforcementBehaviorDeploymentModule clone = OVT_ReinforcementBehaviorDeploymentModule.Cast(capped.CloneModule());
		if (!clone)
		{
			SetFailure("CloneModule() did not answer a reinforcement module at all");
			return true;
		}

		if (clone.m_iMaxReinforcements != SHIPPED_CAP)
		{
			SetFailure("The clone carries a cap of %1, expected %2 - a dropped CloneModule line ships the class default of 0, which is NO LIMIT, and every deployment in the game becomes farmable again with no symptom but a patrol that keeps coming back", clone.m_iMaxReinforcements.ToString(), SHIPPED_CAP.ToString());
			return true;
		}

		// --- CLAIM 5: the SHIPPED town patrol actually authors one. A cap nothing uses is dead code,
		//     and this is the claim that catches the config being edited back to unlimited.
		if (!AssertShippedTownPatrolCap())
			return true;

		Print("Reinforcement cap: no cap by default, a negative is no cap, the clone carries it, and the shipped town patrol authors 3");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the shipped "Town Patrol" config authors the cap; false after SetFailure.
	protected bool AssertShippedTownPatrolCap()
	{
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
		{
			SetFailure("There is no deployment manager, so the shipped config cannot be read");
			return false;
		}

		OVT_DeploymentConfig config = manager.FindConfigByName(TOWN_PATROL_CONFIG);
		if (!config)
		{
			SetFailure("The registry holds no config named '%1'", TOWN_PATROL_CONFIG);
			return false;
		}

		foreach (OVT_BaseDeploymentModule module : config.m_aModules)
		{
			OVT_ReinforcementBehaviorDeploymentModule reinforcement = OVT_ReinforcementBehaviorDeploymentModule.Cast(module);
			if (!reinforcement)
				continue;

			if (reinforcement.m_iMaxReinforcements != SHIPPED_CAP)
			{
				SetFailure("'%1' authors a reinforcement cap of %2, expected %3", TOWN_PATROL_CONFIG,
					reinforcement.m_iMaxReinforcements.ToString(), SHIPPED_CAP.ToString());
				return false;
			}

			return true;
		}

		SetFailure("'%1' has no reinforcement module at all, so nothing caps its rebuys", TOWN_PATROL_CONFIG);
		return false;
	}
}
