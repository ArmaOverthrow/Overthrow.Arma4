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
//! The repair verb is reachable, and the number it will be priced with actually arrived.
//!
//! TWO CLAIMS:
//!   1. OVT_ResistanceRequestComponent - which now carries RepairStructure()/RpcAsk_RepairStructure -
//!      resolves off the LOCAL player's own OVT_OverthrowController. Without it the held action's
//!      PerformAction returns silently and a player holds the ring for 20 seconds for nothing.
//!   2. repairCostMultiplier is present and sane on this machine's difficulty. It rides the hand-rolled
//!      config bitstream at CONFIG_STREAM_VERSION 5 (D12); a writer and reader that disagreed by one
//!      field would leave it reading whatever float sat next to it in the stream, and the client would
//!      draw - and gate on - a price the server never charges. A multiplier at or below 0 would make
//!      every repair free, and one above 1 would make repairing dearer than rebuilding.
//!
//! PROVEN ABLE TO FAIL (fail proof recorded, execution belongs to the phase's suite run): assert the
//! multiplier is strictly greater than 1 and the case goes red naming the shipped value.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_RepairSeam_AVerbAndMultiplierResolve : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the local player's controller to be spawned and registered.
	static const int MAX_POLLS = 300;

	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OverthrowController controller = OVT_Global.GetController();
		if (!controller)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("OVT_Global.GetController() was still null after %1 polls. Nothing on the controller seam is reachable from this machine, so this case cannot say anything about the repair verb either way.",
					m_iPolls.ToString());
				return true;
			}

			return false;
		}

		OVT_ResistanceRequestComponent viaAccessor = OVT_ControllerComponent<OVT_ResistanceRequestComponent>.Get();
		if (!viaAccessor)
		{
			SetFailure("OVT_ControllerComponent<OVT_ResistanceRequestComponent>.Get() returned null while a controller entity exists. OVT_RepairStructureAction.PerformAction sends through exactly this component, so every repair hold would complete and do nothing.");
			return true;
		}

		if (viaAccessor != OVT_ResistanceRequestComponent.Cast(controller.FindComponent(OVT_ResistanceRequestComponent)))
		{
			SetFailure("The resistance request component did not come from the local player's own controller entity - a repair would be attributed to, and charged to, another player server-side.");
			return true;
		}

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (!difficulty)
		{
			SetFailure("OVT_Global.GetDifficulty() is null, so nothing on this machine can price a repair at all");
			return true;
		}

		if (difficulty.repairCostMultiplier <= 0)
		{
			SetFailure("repairCostMultiplier is %1 - at or below zero every repair is free, which is not a setting any shipped preset authors",
				difficulty.repairCostMultiplier.ToString());
			return true;
		}

		if (difficulty.repairCostMultiplier > 1)
		{
			SetFailure("repairCostMultiplier is %1 - above 1 a repair costs more than a rebuild, and no player would ever choose it",
				difficulty.repairCostMultiplier.ToString());
			return true;
		}

		PrintFormat("Repair seam: the request component resolves off the local controller and repairCostMultiplier arrived as %1", difficulty.repairCostMultiplier.ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! EVERY buildable in the shipped config prices to a positive repair cost through the REAL join.
//!
//! It spawns each prefab and asks the manager to price the live entity, which is the only way to
//! exercise FindBuildableForEntity()'s prefab-name lookup - a hardcoded cost table would assert
//! nothing about the join, and the join is the part that can silently miss.
//!
//! IT READS THE LIVE CONFIG, not a list, so a ninth buildable added without a cost turns this red on
//! the first run rather than shipping with an unrepairable structure.
//!
//! Three claims per prefab: the buildable entry is found from the spawned entity, the price is
//! strictly positive (a free repair is not a price), and it never exceeds what the same structure
//! costs to build at this difficulty.
//!
//! PROVEN ABLE TO FAIL (fail proof recorded, execution belongs to the phase's suite run): make
//! FindBuildableForEntity() join on OVT_BuildableComponent.GetBuildableType() against m_sName instead
//! of on the prefab and the case goes red on the Guard Tower ("GuardTower" never equals "Guard
//! Tower") - the exact defect the join's header warns about.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 120)]
class OVT_TEST_Init_RepairSeam_BEveryBuildablePrices : SCR_AutotestCaseBase
{
	//! How many buildables the feature covers. A LOWER bound: a ninth is welcome and gets priced too.
	static const int PRICED_BUILDABLES = 8;

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
		if (buildables.Count() < PRICED_BUILDABLES)
		{
			SetFailure("The buildables config lists %1 structures, fewer than the %2 this feature prices - it did not load fully",
				buildables.Count().ToString(), PRICED_BUILDABLES.ToString());
			return true;
		}

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
