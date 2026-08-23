//------------------------------------------------------------------------------------------------
//! TIER B - both shipped faction registries answer the vehicle escalation ladder correctly through
//! OVT_Faction.ResolveVehicleForRole, live rather than by construction.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_VehicleLadderResolution : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OverthrowFactionManager factions = OVT_Global.GetFactions();
		if (!factions)
		{
			SetFailure("OVT_Global.GetFactions() is null");
			return true;
		}

		if (!AssertThreeDistinctRungs(factions, "USSR")) return true;
		if (!AssertThreeDistinctRungs(factions, "US")) return true;

		OVT_Faction ussr = factions.GetOverthrowFactionByKey("USSR");
		if (!ussr)
		{
			SetFailure("USSR faction not registered");
			return true;
		}

		OVT_FactionVehicleEntry unknown;
		if (ussr.ResolveVehicleForRole("no_such_role", 100000, 1, -1, unknown))
		{
			SetFailure("an unauthored role must answer false rather than resolve to some entry");
			return true;
		}

		Print("Vehicle ladder resolution: both shipped registries resolve role 'armed' to three distinct rungs, and an unknown role answers false");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] factions The live faction manager.
	//! \param[in] key The faction key to check.
	//! \return True when role "armed" resolves to three distinct named rungs across a low, a mid
	//! and a high threat; false after calling SetFailure.
	protected bool AssertThreeDistinctRungs(OVT_OverthrowFactionManager factions, string key)
	{
		OVT_Faction faction = factions.GetOverthrowFactionByKey(key);
		if (!faction)
		{
			SetFailure(string.Format("%1 faction not registered", key));
			return false;
		}

		OVT_FactionVehicleEntry low;
		if (!faction.ResolveVehicleForRole("armed", 400, 1, -1, low))
		{
			SetFailure(string.Format("%1 role 'armed' at threat 400 must resolve to its bottom rung", key));
			return false;
		}

		OVT_FactionVehicleEntry mid;
		if (!faction.ResolveVehicleForRole("armed", 900, 1, -1, mid))
		{
			SetFailure(string.Format("%1 role 'armed' at threat 900 must resolve to its middle rung", key));
			return false;
		}

		OVT_FactionVehicleEntry high;
		if (!faction.ResolveVehicleForRole("armed", 1500, 1, -1, high))
		{
			SetFailure(string.Format("%1 role 'armed' at threat 1500 must resolve to its top rung", key));
			return false;
		}

		if (low.m_sVehicleName == mid.m_sVehicleName || mid.m_sVehicleName == high.m_sVehicleName || low.m_sVehicleName == high.m_sVehicleName)
		{
			SetFailure(string.Format("%1 role 'armed' must resolve to three distinct rungs, got %2 / %3 / %4",
				key, low.m_sVehicleName, mid.m_sVehicleName, high.m_sVehicleName));
			return false;
		}

		return true;
	}
}
