//------------------------------------------------------------------------------------------------
//! TIER B - the repair seam where it meets real data: the controller verb, and the prefab->price join.
//!
//! The Logic tier already pins the arithmetic (OVT_TEST_Logic_RepairPricing). What it cannot reach is
//! everything between a live structure and that arithmetic: whether the seam the client sends through
//! exists at all, whether the difficulty the price is scaled by actually arrived, and whether
//! OVT_ResistanceFactionManager.FindBuildableForEntity() can get from a SPAWNED prefab back to the
//! config entry it came from. That join is the load-bearing half - it is on the prefab resource name
//! and never on the buildable type string, because seven of the eight type strings do not match the
//! config's m_sName, and a join that silently missed would price every repair at "unrepairable".
//!
//! ⚠ NOTHING HERE REPAIRS ANYTHING. The RPC is not invoked and no money moves: charging is a
//! dedicated-server play-test item (Phase 5's "Needs human verification"). These cases spawn their
//! own subjects far from anything, price them, and delete them again.
//!
//! Cases run alphabetically by class name and neither writes shared state; the A/B prefixes are for
//! the run log. No maxAttempts anywhere - the one poll below is a named precondition, not a retry.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Every buildable prefab spawns, joins back to its config entry, and prices to a repair cost that
//! is positive and no more than its build cost at this difficulty.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 120)]
class OVT_TEST_Init_RepairSeam_BEveryBuildablePrices : SCR_AutotestCaseBase
{
	//! Stepped so two subjects never share a spot even though each is deleted before the next spawns.
	static const int SUBJECT_STEP_M = 25;

	protected IEntity m_Subject;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance || !resistance.m_BuildablesConfig || !resistance.m_BuildablesConfig.m_aBuildables)
		{
			SetFailure("The buildables config is not loaded, so there is nothing to price");
			return true;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config || !config.m_Difficulty)
		{
			SetFailure("No difficulty settings on this machine - GetRepairCost() could never answer");
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.Count() < 1)
		{
			SetFailure("No towns are registered - nowhere sensible to spawn the subjects");
			return true;
		}

		array<ref OVT_Buildable> buildables = resistance.m_BuildablesConfig.m_aBuildables;
		vector anchor = towns.m_Towns[0].location + Vector(700, 0, 700);
		int index = 0;

		foreach (OVT_Buildable buildable : buildables)
		{
			if (!buildable || !buildable.m_aPrefabs)
			{
				SetFailure("A buildables config entry carries no prefab list");
				return CleanUp();
			}

			foreach (ResourceName prefab : buildable.m_aPrefabs)
			{
				string failure = CheckOne(resistance, config, buildable, prefab, anchor + Vector(0, 0, index * SUBJECT_STEP_M));
				index++;

				if (failure != "")
				{
					SetFailure(failure);
					return CleanUp();
				}
			}
		}

		Print(string.Format("Repair seam: all %1 buildable prefabs price to a positive repair cost through the live prefab join", index.ToString()));

		return CleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns one prefab, prices it, and removes it again whatever the answer was.
	//! \param[in] resistance The resistance manager.
	//! \param[in] config The config component, for the build price to compare against.
	//! \param[in] buildable The config entry this prefab came from.
	//! \param[in] prefab The prefab to spawn.
	//! \param[in] origin Where to put it.
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckOne(notnull OVT_ResistanceFactionManager resistance, notnull OVT_OverthrowConfigComponent config, notnull OVT_Buildable buildable, ResourceName prefab, vector origin)
	{
		if (prefab == ResourceName.Empty)
			return string.Format("The buildable '%1' lists an empty prefab", buildable.m_sName);

		m_Subject = OVT_Global.SpawnEntityPrefab(prefab, origin);
		if (!m_Subject)
			return string.Format("SpawnEntityPrefab() produced no entity for '%1' from %2", buildable.m_sName, prefab);

		string failure = CheckPrice(resistance, config, buildable, m_Subject);

		if (m_Subject && !m_Subject.IsDeleted())
			delete m_Subject;

		m_Subject = null;

		return failure;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] resistance The resistance manager.
	//! \param[in] config The config component.
	//! \param[in] buildable The config entry this subject came from.
	//! \param[in] subject The spawned structure.
	//! \return An empty string when every claim held, or the first that did not.
	protected string CheckPrice(notnull OVT_ResistanceFactionManager resistance, notnull OVT_OverthrowConfigComponent config, notnull OVT_Buildable buildable, notnull IEntity subject)
	{
		OVT_Buildable joined = resistance.FindBuildableForEntity(subject);
		if (!joined)
			return string.Format("'%1' spawned but FindBuildableForEntity() did not find its config entry. The join is on the prefab resource name; a live structure it cannot place has no price and can never be repaired", buildable.m_sName);

		if (joined.m_iCost != buildable.m_iCost)
			return string.Format("'%1' joined to a config entry costing %2 instead of its own %3 - two entries share a prefab, or the wrong one matched first",
				buildable.m_sName, joined.m_iCost.ToString(), buildable.m_iCost.ToString());

		int repair = resistance.GetRepairCost(subject);
		if (repair <= 0)
			return string.Format("'%1' priced at %2 to repair. Anything at or below zero means the join missed or the cost is unauthored, and the held action would refuse to appear on its ruin",
				buildable.m_sName, repair.ToString());

		int build = config.GetBuildableCost(buildable);
		if (repair > build)
			return string.Format("'%1' costs %2 to repair but only %3 to build at this difficulty - a repair must never be dearer than a rebuild",
				buildable.m_sName, repair.ToString(), build.ToString());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool CleanUp()
	{
		if (m_Subject && !m_Subject.IsDeleted())
			delete m_Subject;

		m_Subject = null;

		return true;
	}
}
