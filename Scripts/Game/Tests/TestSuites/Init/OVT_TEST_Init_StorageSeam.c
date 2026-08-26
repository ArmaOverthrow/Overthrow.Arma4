//------------------------------------------------------------------------------------------------
//! TIER B - the storage holder seam: OVT_StorageComponent is actually ON the prefabs that need one,
//! and each resolves the capacity its holder class is supposed to get.
//!
//! WHY THIS FILE EXISTS. The component reaches every truck, car, box and warehouse through FOUR
//! prefab edits and nothing else - there is no runtime component creation in EnforceScript, so a
//! dropped prefab block is the whole failure. It is also completely silent: a holder without the
//! component simply has no storage actions, and a holder whose AUTO resolve answered 0 looks exactly
//! the same. Neither produces a compile error, a runtime error or a log line at the point of use.
//!
//! ONE CLAIM PER HOLDER CLASS:
//!   A. a legal PARKING_TRUCK wheeled vehicle resolves UNLIMITED (-1)  - Wheeled_Base.et, AUTO
//!   B. a legal non-truck wheeled vehicle resolves the authored cap (300) - Wheeled_Base.et, AUTO + D4
//!   C. a placed ammo box resolves UNLIMITED (-1)                      - OVT_AmmoBox_Base.et
//!   D. the test world's warehouse building resolves UNLIMITED (-1)    - the same-GUID delta of
//!      vanilla Warehouse_01_Base.et {E35EA41864A3B0ED}, which is the one edit here that a typo in a
//!      GUID or a parent path would silently turn into a file the engine never loads (R4).
//!   E. an illegal/armed wheeled vehicle resolves NONE (0) and the radius query leaves it out.
//!
//! Every case also asserts the authored capacity MODE. Without that, a dropped m_eCapacityMode
//! attribute would still produce the right number on a box or a building - AUTO answers -1 through the
//! not-a-vehicle branch - and the case would pass for a reason that has nothing to do with the prefab.
//!
//! THE POLLS ARE PRECONDITIONS, NOT RETRIES (no maxAttempts anywhere in this project). Capacity
//! resolution is deferred out of OnPostInit on purpose - the economy's vehicle catalogue is built on a
//! call-queue hop of its own - so a case that read GetCapacity() in the spawn frame would be asserting
//! on "not decided yet". Expiry is itself a named failure carrying the diagnosis.
//!
//! Cases run alphabetically by class name; none writes shared state, and every subject a case spawns
//! is deleted again before the case reports.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Subject resolution for the four cases below, kept out of the case bodies so each reads as
//! "spawn -> wait for the resolve -> assert the number".
//------------------------------------------------------------------------------------------------
class OVT_TEST_StorageSeamSubject
{
	//! What a placed ammo box is. The player-placeable variant rather than the bare base, because that
	//! is the one a stockpile is ever made of.
	static const ResourceName AMMO_BOX_PREFAB = "{0AAFD134C3BEE963}Prefabs/Props/Military/AmmoBoxes/OVT_AmmoBox_Placed.et";

	//! Prefab path fragment every warehouse variant shares, and the fragment the real-estate config
	//! filters on (OVT_OverthrowGameMode.et, OVT_RealEstateConfig "Warehouse").
	static const string WAREHOUSE_PREFAB_FRAGMENT = "Warehouse_01";

	//! The one wheeled prefab that overrides the AUTO capacity mode, and therefore the one the vehicle
	//! cases must not pick as their subject.
	static const string MODE_OVERRIDE_FRAGMENT = "OverthrowMobileFOB";

	//------------------------------------------------------------------------------------------------
	//! Resolves a wheeled vehicle prefab of a given parking class from the economy's OWN catalogue.
	//!
	//! Read from the economy rather than hardcoded for the same reason the shipped vehicle cases read
	//! the manager's starting-car list: retuning which vehicles Overthrow sells must change which
	//! vehicle is exercised, not turn a case red for a reason that has nothing to do with storage.
	//! Registered and legal are required because those are the two inputs AUTO resolution reads.
	//! \param[in] wantTruck True for a PARKING_TRUCK vehicle, false for anything else.
	//! \param[in] pathMustContain Extra path fragment the candidate must carry; "" accepts any.
	//! \param[out] prefab The prefab to spawn; untouched when nothing matched.
	//! \return True when a prefab was resolved.
	static bool FindWheeledVehicle(bool wantTruck, string pathMustContain, out ResourceName prefab)
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
			return false;

		array<ResourceName> all = new array<ResourceName>();
		economy.FindVehicles("", all);

		foreach (ResourceName candidate : all)
		{
			if (candidate.IndexOf("/Wheeled/") == -1)
				continue;

			// The mobile FOB is the first registered PARKING_TRUCK in the catalogue AND the one wheeled
			// prefab that overrides the mode to UNLIMITED, so picking it would silently assert the
			// override instead of the AUTO truck branch.
			if (candidate.IndexOf(MODE_OVERRIDE_FRAGMENT) > -1)
				continue;

			if (pathMustContain != "" && candidate.IndexOf(pathMustContain) == -1)
				continue;

			if (!economy.IsRegisteredResource(candidate))
				continue;

			int id = economy.GetInventoryId(candidate);
			if (!economy.IsLegalVehicle(id))
				continue;

			bool isTruck = (economy.GetParkingType(id) == OVT_ParkingType.PARKING_TRUCK);
			if (isTruck != wantTruck)
				continue;

			prefab = candidate;
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A civilian car, preferring a genuinely civilian prefab and settling for any legal non-truck
	//! wheeled vehicle - both resolve through exactly the same AUTO branch.
	//! \param[out] prefab The prefab to spawn; untouched when nothing matched.
	//! \return True when a prefab was resolved.
	static bool FindCivilianCar(out ResourceName prefab)
	{
		if (FindWheeledVehicle(false, "_CIV", prefab))
			return true;

		return FindWheeledVehicle(false, "", prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! An illegal or armed wheeled vehicle - the AUTO branch that must answer "no storage at all".
	//! \param[out] prefab The prefab to spawn; untouched when nothing matched.
	//! \return True when a prefab was resolved.
	static bool FindIllegalVehicle(out ResourceName prefab)
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
			return false;

		array<ResourceName> all = new array<ResourceName>();
		economy.FindVehicles("", all);

		foreach (ResourceName candidate : all)
		{
			if (candidate.IndexOf("/Wheeled/") == -1)
				continue;

			if (candidate.IndexOf(MODE_OVERRIDE_FRAGMENT) > -1)
				continue;

			if (!economy.IsRegisteredResource(candidate))
				continue;

			if (economy.IsLegalVehicle(economy.GetInventoryId(candidate)))
				continue;

			prefab = candidate;
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Somewhere to put a spawned subject, well clear of the town and of the other cases' subjects.
	//! \param[in] offset Per-case separation from the anchor.
	//! \param[out] position Where to spawn; untouched on failure.
	//! \return True when a position was resolved.
	static bool ResolveSpawnPosition(vector offset, out vector position)
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.IsEmpty())
			return false;

		position = towns.m_Towns[0].location + offset;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Finds the test world's warehouse building.
//!
//! ONE INSTANCE PER CALL, accumulator on the instance - the same rule the production query follows.
//! The search is by prefab path rather than by position so that moving the building in
//! Worlds/MP/OVT_Campaign_Test_Layers/default.layer does not silently turn this case green-by-absence.
//------------------------------------------------------------------------------------------------
class OVT_TEST_StorageWarehouseFinder : Managed
{
	protected IEntity m_Found;

	//------------------------------------------------------------------------------------------------
	//! \return The first warehouse building in the world, or null when there is none.
	IEntity Find()
	{
		m_Found = null;

		BaseWorld world = GetGame().GetWorld();
		if (world)
			world.QueryEntitiesBySphere("0 0 0", 20000, null, FilterWarehouse, EQueryEntitiesFlags.STATIC);

		return m_Found;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] e The entity the query offered.
	//! \return Always false - there is no early-out with a null query callback.
	protected bool FilterWarehouse(IEntity e)
	{
		if (!e || m_Found)
			return false;

		ResourceName prefab = OVT_PrefabUtils.GetPrefabName(e);
		if (prefab.IndexOf(OVT_TEST_StorageSeamSubject.WAREHOUSE_PREFAB_FRAGMENT) > -1)
			m_Found = e;

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! A legal truck resolves UNLIMITED storage.
//!
//! This is the claim that proves Wheeled_Base.et carries the component at all, and that AUTO reads a
//! populated economy catalogue rather than an empty one - an empty catalogue would answer
//! "unregistered", i.e. capacity 0, i.e. no storage on any vehicle in the game.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_StorageSeam_ATruckIsUnlimited : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the deferred capacity resolve, generous enough to outlast its own
	//! ten-second retry budget.
	static const int MAX_POLLS = 900;

	protected int m_iPolls;
	protected IEntity m_Vehicle;
	protected ResourceName m_sPrefab;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!m_Vehicle)
		{
			if (!OVT_TEST_StorageSeamSubject.FindWheeledVehicle(true, "", m_sPrefab))
			{
				SetFailure("The economy knows no legal wheeled vehicle parked as PARKING_TRUCK, so 'a truck holds an unlimited ledger' has no subject to assert against. Check Configs/Pricing/vehiclePrices.conf and that BuildResourceDatabase() has run.");
				return true;
			}

			vector position;
			if (!OVT_TEST_StorageSeamSubject.ResolveSpawnPosition("600 0 600", position))
			{
				SetFailure("No town is registered, so there is nowhere sensible to put a test truck");
				return true;
			}

			m_Vehicle = OVT_Global.SpawnEntityPrefab(m_sPrefab, position);
			if (!m_Vehicle)
			{
				SetFailure("SpawnEntityPrefab() produced no vehicle from %1", m_sPrefab);
				return true;
			}
		}

		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(m_Vehicle);
		if (!storage)
		{
			SetFailure("The truck %1 has no OVT_StorageComponent. Prefabs/Vehicles/Core/Wheeled_Base.et has lost its entry, so NO wheeled vehicle spawned by any system has a ledger and every storage action is invisible mod-wide.", m_sPrefab);
			return FinishAndCleanUp();
		}

		if (storage.GetCapacityMode() != EOVT_StorageCapacityMode.AUTO)
		{
			SetFailure("The truck %1 does not use AUTO capacity, so this case would assert an override instead of the AUTO truck branch. Pick a different subject, or extend OVT_TEST_StorageSeamSubject.MODE_OVERRIDE_FRAGMENT.", m_sPrefab);
			return FinishAndCleanUp();
		}

		if (!storage.IsCapacityResolved())
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("The truck %1 never resolved a capacity in %2 frames. The deferred resolve is not running - either OnPostInit did not queue it, or it is still waiting on an economy catalogue that never got built.",
					m_sPrefab,
					m_iPolls.ToString());
				return FinishAndCleanUp();
			}

			return false;
		}

		if (storage.GetCapacity() != OVT_StorageComponent.UNLIMITED_CAPACITY)
		{
			SetFailure("The truck %1 resolved capacity %2, expected -1 (unlimited). A truck that is not unlimited cannot be the mod's bulk hauler, and a 0 here means the economy does not know this prefab.",
				m_sPrefab,
				storage.GetCapacity().ToString());
			return FinishAndCleanUp();
		}

		PrintFormat("Storage seam: the truck %1 resolves UNLIMITED capacity", m_sPrefab);
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		if (m_Vehicle)
		{
			delete m_Vehicle;
			m_Vehicle = null;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A legal non-truck car resolves the authored vehicle cap (D4: 300 items).
//!
//! The number is read back off the component's own attribute, not hardcoded here: D4 exists precisely
//! so the cap can be retuned in the Workbench, and a case that pinned 300 in script would turn a
//! deliberate retune into a red run. What is asserted is the BRANCH - a car gets the authored cap, not
//! unlimited and not nothing.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_StorageSeam_BCivilianCarIsCapped : SCR_AutotestCaseBase
{
	static const int MAX_POLLS = 900;

	//! What Wheeled_Base.et authors, and what decision 4 fixed the value at.
	static const int EXPECTED_CAPACITY = 300;

	protected int m_iPolls;
	protected IEntity m_Vehicle;
	protected ResourceName m_sPrefab;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!m_Vehicle)
		{
			if (!OVT_TEST_StorageSeamSubject.FindCivilianCar(m_sPrefab))
			{
				SetFailure("The economy knows no legal wheeled vehicle outside PARKING_TRUCK, so 'a civilian car is capped' has no subject to assert against.");
				return true;
			}

			vector position;
			if (!OVT_TEST_StorageSeamSubject.ResolveSpawnPosition("640 0 600", position))
			{
				SetFailure("No town is registered, so there is nowhere sensible to put a test car");
				return true;
			}

			m_Vehicle = OVT_Global.SpawnEntityPrefab(m_sPrefab, position);
			if (!m_Vehicle)
			{
				SetFailure("SpawnEntityPrefab() produced no vehicle from %1", m_sPrefab);
				return true;
			}
		}

		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(m_Vehicle);
		if (!storage)
		{
			SetFailure("The car %1 has no OVT_StorageComponent - Prefabs/Vehicles/Core/Wheeled_Base.et has lost its entry", m_sPrefab);
			return FinishAndCleanUp();
		}

		if (storage.GetCapacityMode() != EOVT_StorageCapacityMode.AUTO)
		{
			SetFailure("The car %1 does not use AUTO capacity, so this case would assert an override instead of the AUTO vehicle branch", m_sPrefab);
			return FinishAndCleanUp();
		}

		if (!storage.IsCapacityResolved())
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("The car %1 never resolved a capacity in %2 frames - the deferred resolve is not running, or is still waiting on the economy catalogue",
					m_sPrefab,
					m_iPolls.ToString());
				return FinishAndCleanUp();
			}

			return false;
		}

		if (storage.GetCapacity() != EXPECTED_CAPACITY)
		{
			SetFailure("The car %1 resolved capacity %2, expected %3. -1 would mean the truck branch is answering for cars; 0 would mean the economy calls it illegal or does not know it.",
				m_sPrefab,
				storage.GetCapacity().ToString(),
				EXPECTED_CAPACITY.ToString());
			return FinishAndCleanUp();
		}

		PrintFormat("Storage seam: the car %1 resolves the authored vehicle cap", m_sPrefab);
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		if (m_Vehicle)
		{
			delete m_Vehicle;
			m_Vehicle = null;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A placed ammo box resolves UNLIMITED storage.
//!
//! The box is the holder every other flow ends at - Unload, Transfer all, the officer clear - so it
//! being authored UNLIMITED rather than left on AUTO is load-bearing: AUTO would answer through the
//! not-a-vehicle branch, which is the same number today but for a reason that has nothing to do with
//! boxes.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_StorageSeam_CAmmoBoxIsUnlimited : SCR_AutotestCaseBase
{
	static const int MAX_POLLS = 900;

	protected int m_iPolls;
	protected IEntity m_Box;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!m_Box)
		{
			vector position;
			if (!OVT_TEST_StorageSeamSubject.ResolveSpawnPosition("680 0 600", position))
			{
				SetFailure("No town is registered, so there is nowhere sensible to put a test ammo box");
				return true;
			}

			m_Box = OVT_Global.SpawnEntityPrefab(OVT_TEST_StorageSeamSubject.AMMO_BOX_PREFAB, position);
			if (!m_Box)
			{
				SetFailure("SpawnEntityPrefab() produced no entity from %1", OVT_TEST_StorageSeamSubject.AMMO_BOX_PREFAB);
				return true;
			}
		}

		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(m_Box);
		if (!storage)
		{
			SetFailure("A placed ammo box has no OVT_StorageComponent - Prefabs/Props/Military/AmmoBoxes/OVT_AmmoBox_Base.et has lost its entry, so every player stockpile in the game is unreachable");
			return FinishAndCleanUp();
		}

		if (storage.GetCapacityMode() != EOVT_StorageCapacityMode.UNLIMITED)
		{
			SetFailure("A placed ammo box is not authored UNLIMITED. AUTO answers -1 for a box too, through the not-a-vehicle branch, so the number below would still be right for the wrong reason - the m_eCapacityMode attribute has gone missing from OVT_AmmoBox_Base.et.");
			return FinishAndCleanUp();
		}

		if (!storage.IsCapacityResolved())
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("A placed ammo box never resolved a capacity in %1 frames - the deferred resolve is not running at all", m_iPolls.ToString());
				return FinishAndCleanUp();
			}

			return false;
		}

		if (storage.GetCapacity() != OVT_StorageComponent.UNLIMITED_CAPACITY)
		{
			SetFailure("A placed ammo box resolved capacity %1, expected -1 (unlimited)", storage.GetCapacity().ToString());
			return FinishAndCleanUp();
		}

		Print("Storage seam: a placed ammo box resolves UNLIMITED capacity");
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		if (m_Box)
		{
			delete m_Box;
			m_Box = null;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The test world's warehouse building resolves UNLIMITED storage.
//!
//! THE ONLY CHECK THAT EXISTS ON THE SAME-GUID DELTA (R4). Overthrow's
//! Prefabs/Structures/Industrial/Houses/Warehouse_01/Warehouse_01_Base.et carries vanilla's own GUID
//! {E35EA41864A3B0ED} and vanilla's own parent line, which is what makes it a delta on the vanilla
//! prefab rather than an unrelated file the engine never loads. A typo in either is completely silent:
//! the warehouse simply has no ledger, and the warehouse is the one holder the whole feature was
//! extracted out of.
//!
//! ⚠ NOTHING IS SPAWNED OR DELETED HERE. The subject is the world's own building; the case reads it
//! and leaves it exactly as it found it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_StorageSeam_DWarehouseBuildingIsUnlimited : SCR_AutotestCaseBase
{
	static const int MAX_POLLS = 900;

	protected int m_iPolls;
	protected IEntity m_Warehouse;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!m_Warehouse)
		{
			OVT_TEST_StorageWarehouseFinder finder = new OVT_TEST_StorageWarehouseFinder();
			m_Warehouse = finder.Find();

			if (!m_Warehouse)
			{
				SetFailure("No Warehouse_01 building exists in this world. Worlds/MP/OVT_Campaign_Test_Layers/default.layer is supposed to place one, and the real-estate config filters on that same prefab fragment - without it neither this case nor the warehouse economy has a subject.");
				return true;
			}
		}

		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(m_Warehouse);
		if (!storage)
		{
			SetFailure("The world's warehouse building has no OVT_StorageComponent. The same-GUID delta of vanilla Warehouse_01_Base.et {E35EA41864A3B0ED} is not being applied - check that the .et.meta GUID matches vanilla byte-for-byte and that the header names Building_Base.et {A43A100E3C377DB2} exactly as vanilla does.");
			return true;
		}

		if (storage.GetCapacityMode() != EOVT_StorageCapacityMode.UNLIMITED)
		{
			SetFailure("The warehouse building is not authored UNLIMITED. AUTO answers -1 for a building too, so the number below would be right for the wrong reason - the m_eCapacityMode attribute has gone missing from the Warehouse_01_Base.et delta.");
			return true;
		}

		if (!storage.IsCapacityResolved())
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("The warehouse building never resolved a capacity in %1 frames - the deferred resolve is not running at all", m_iPolls.ToString());
				return true;
			}

			return false;
		}

		if (storage.GetCapacity() != OVT_StorageComponent.UNLIMITED_CAPACITY)
		{
			SetFailure("The warehouse building resolved capacity %1, expected -1 (unlimited)", storage.GetCapacity().ToString());
			return true;
		}

		Print("Storage seam: the world's warehouse building resolves UNLIMITED capacity through the same-GUID vanilla delta");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! An illegal or armed wheeled vehicle resolves NO capacity, and is not offered as a destination.
//!
//! The inverse of case B, and the only automated check on the "capacity 0 means not a holder" rule
//! that every gate, action and picker downstream is built on. Two claims:
//!   1. AUTO answers 0 for an illegal vehicle - a stolen BTR is not a mobile warehouse;
//!   2. OVT_StorageHolderQuery leaves it out, which is the clause (GetCapacity() != 0) that keeps it
//!      off the destination picker and out of server-side validation.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_StorageSeam_EIllegalVehicleHasNoStorage : SCR_AutotestCaseBase
{
	static const int MAX_POLLS = 900;

	//! Destination-picker radius the query is exercised at; matches the plan's default holder radius.
	static const float QUERY_RADIUS = 25;

	protected int m_iPolls;
	protected IEntity m_Vehicle;
	protected ResourceName m_sPrefab;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!m_Vehicle)
		{
			if (!OVT_TEST_StorageSeamSubject.FindIllegalVehicle(m_sPrefab))
			{
				SetFailure("The economy knows no illegal wheeled vehicle, so 'an illegal vehicle has no storage' has no subject. Check that Configs/Pricing/vehiclePrices.conf still marks anything illegal.");
				return true;
			}

			vector position;
			if (!OVT_TEST_StorageSeamSubject.ResolveSpawnPosition("720 0 600", position))
			{
				SetFailure("No town is registered, so there is nowhere sensible to put a test vehicle");
				return true;
			}

			m_Vehicle = OVT_Global.SpawnEntityPrefab(m_sPrefab, position);
			if (!m_Vehicle)
			{
				SetFailure("SpawnEntityPrefab() produced no vehicle from %1", m_sPrefab);
				return true;
			}
		}

		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(m_Vehicle);
		if (!storage)
		{
			SetFailure("The illegal vehicle %1 has no OVT_StorageComponent - Prefabs/Vehicles/Core/Wheeled_Base.et has lost its entry", m_sPrefab);
			return FinishAndCleanUp();
		}

		if (storage.GetCapacityMode() != EOVT_StorageCapacityMode.AUTO)
		{
			SetFailure("The illegal vehicle %1 does not use AUTO capacity, so this case would assert an override instead of the AUTO illegal branch", m_sPrefab);
			return FinishAndCleanUp();
		}

		if (!storage.IsCapacityResolved())
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("The illegal vehicle %1 never resolved a capacity in %2 frames - the deferred resolve is not running, or is still waiting on the economy catalogue",
					m_sPrefab,
					m_iPolls.ToString());
				return FinishAndCleanUp();
			}

			return false;
		}

		if (storage.GetCapacity() != OVT_StorageComponent.NO_CAPACITY)
		{
			SetFailure("The illegal vehicle %1 resolved capacity %2, expected 0. Anything else makes an armed or illegal vehicle a storage holder, with actions on it and a slot in every destination picker.",
				m_sPrefab,
				storage.GetCapacity().ToString());
			return FinishAndCleanUp();
		}

		// Claim 2 - the radius query's capacity clause actually excludes it. Asserted as "not present"
		// rather than "found nothing", because what else stands near the spawn point is the world's
		// business, not this case's.
		array<IEntity> holders = new array<IEntity>();
		OVT_StorageHolderQuery query = new OVT_StorageHolderQuery();
		query.Run(m_Vehicle.GetOrigin(), QUERY_RADIUS, holders);

		if (holders.Contains(m_Vehicle))
		{
			SetFailure("OVT_StorageHolderQuery offered the illegal vehicle %1 as a holder even though its capacity is 0 - the FilterHolders capacity clause is gone, so it would appear in the destination picker", m_sPrefab);
			return FinishAndCleanUp();
		}

		PrintFormat("Storage seam: the illegal vehicle %1 resolves NO capacity and is not offered as a holder", m_sPrefab);
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		if (m_Vehicle)
		{
			delete m_Vehicle;
			m_Vehicle = null;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_StorageRequestComponent is actually ON the local player's OVT_OverthrowController.
//!
//! D11 requires one Init assertion per new controller component, added in the phase that creates it,
//! because the failure it catches is completely silent: a component that is not on
//! Prefabs/GameMode/OVT_OverthrowController.et resolves null through the accessor with no compile
//! error and no runtime error, and every caller null-checks by contract and returns. Here that would
//! mean the pull-on-open fan, every batch verb, Clear inventory and Rename all quietly never happen -
//! and this is the ONLY seam any of them has, so there is no older path to fall back on.
//!
//! TWO CLAIMS, ONE PRECONDITION:
//!   1. the component resolves through the epic-level accessor at all;
//!   2. what comes back is the instance carried by THIS player's own controller entity - a Get() that
//!      searched the wrong entity would also satisfy claim 1, and would mean every storage request
//!      was sent through another player's seam and resolved by the server as coming from them.
//!
//! THE POLL IS A PRECONDITION, NOT A RETRY (no maxAttempts anywhere in this project). The controller
//! is spawned by OVT_PlayerManagerComponent.SetupPlayer() when the player enters the world, which is
//! not instantaneous at world load; expiry is itself a named failure carrying the diagnosis.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_StorageSeam_FRequestComponentResolves : SCR_AutotestCaseBase
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
				SetFailure("OVT_Global.GetController() was still null after %1 polls (local player id %2). Nothing on the controller seam is reachable from this machine, so this case cannot say anything about OVT_StorageRequestComponent either way.",
					m_iPolls.ToString(),
					SCR_PlayerController.GetLocalPlayerId().ToString());
				return true;
			}

			return false;
		}

		// Claim 1 - it is wired.
		OVT_StorageRequestComponent viaAccessor = OVT_ControllerComponent<OVT_StorageRequestComponent>.Get();
		if (!viaAccessor)
		{
			SetFailure("OVT_ControllerComponent<OVT_StorageRequestComponent>.Get() returned null while a controller entity exists. Prefabs/GameMode/OVT_OverthrowController.et is not carrying the component, so opening a holder, every batch verb, Clear inventory and Rename all silently never happen - this component is the only seam any of them has.");
			return true;
		}

		// Claim 2 - it is OUR controller's instance, not somebody else's.
		OVT_StorageRequestComponent onEntity = OVT_StorageRequestComponent.Cast(controller.FindComponent(OVT_StorageRequestComponent));
		if (viaAccessor != onEntity)
		{
			SetFailure("OVT_ControllerComponent<OVT_StorageRequestComponent>.Get() did not return the instance on the local player's own controller entity. Every storage request would then be sent through another player's seam, and the server would resolve the caller as that player.");
			return true;
		}

		PrintFormat("Storage seam: OVT_StorageRequestComponent resolves off the local controller (found after %1 poll(s))", m_iPolls.ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The DEPLOYED mobile FOB resolves UNLIMITED storage.
//!
//! It is not in Configs/Pricing/vehiclePrices.conf, so AUTO would call it an unregistered vehicle and
//! answer 0 - and a FOB undeploy would then refuse to collect the deployed FOB's own cargo and lose
//! it with the entity the cleanup deletes. The explicit UNLIMITED override on
//! OverthrowMobileFOBDeployed.et is what stops that, and nothing else in the mod asserts it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_StorageSeam_GDeployedFOBIsUnlimited : SCR_AutotestCaseBase
{
	static const int MAX_POLLS = 900;

	//! The prefab OVT_ResistanceFactionManager.DeployFOB spawns.
	static const ResourceName DEPLOYED_FOB_PREFAB = "{ABF741C3184846E9}Prefabs/Vehicles/Wheeled/M923A1/OverthrowMobileFOBDeployed.et";

	protected int m_iPolls;
	protected IEntity m_Fob;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!m_Fob)
		{
			vector position;
			if (!OVT_TEST_StorageSeamSubject.ResolveSpawnPosition("900 0 900", position))
			{
				SetFailure("No town is registered, so there is nowhere sensible to put a test FOB");
				return true;
			}

			m_Fob = OVT_Global.SpawnEntityPrefab(DEPLOYED_FOB_PREFAB, position);
			if (!m_Fob)
			{
				SetFailure("SpawnEntityPrefab() produced no deployed FOB from %1", DEPLOYED_FOB_PREFAB);
				return true;
			}
		}

		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(m_Fob);
		if (!storage)
		{
			SetFailure("The deployed FOB has no OVT_StorageComponent. Undeploy collects the deployed FOB into the mobile one through that component, so its cargo would be deleted with the entity instead.");
			return FinishAndCleanUp();
		}

		if (storage.GetCapacityMode() != EOVT_StorageCapacityMode.UNLIMITED)
		{
			SetFailure("The deployed FOB does not author UNLIMITED capacity. It is not in the economy's vehicle catalogue, so AUTO resolves it to 0 - no storage, and an undeploy that silently drops whatever was in it.");
			return FinishAndCleanUp();
		}

		if (!storage.IsCapacityResolved())
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("The deployed FOB never resolved a capacity in %1 frames. The deferred resolve is not running.", m_iPolls.ToString());
				return FinishAndCleanUp();
			}

			return false;
		}

		if (storage.GetCapacity() != OVT_StorageComponent.UNLIMITED_CAPACITY)
		{
			SetFailure("The deployed FOB resolved capacity %1, expected -1 (unlimited).", storage.GetCapacity().ToString());
			return FinishAndCleanUp();
		}

		PrintFormat("Storage seam: the deployed mobile FOB resolves UNLIMITED capacity");
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		if (m_Fob)
		{
			// The deployed FOB is a composition: a plain delete would leave its flagpole, tent and
			// camo net standing in the test world for every case that runs after this one.
			SCR_EntityHelper.DeleteEntityAndChildren(m_Fob);
			m_Fob = null;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The two transport trucks carry vanilla's own cargo capacity, not Overthrow's old fifth of it.
//!
//! Overthrow's same-GUID deltas of M923A1_transport.et and Ural4320_transport.et used to override
//! MaxCumulativeVolume down to 200000 and m_fMaxWeight down to 1000 - a fifth of the volume and a
//! quarter of the weight vanilla gives the same truck. Battlefield loot goes to the LEDGER now and no
//! longer depends on these, but every hand-loaded crate and every Take-from-storage withdrawal still
//! spawns into the vanilla bed, so a throttled bed is still what makes a truck stop accepting cargo.
//!
//! The thresholds are vanilla's own figures read out of the extracted tree; asserting ">= vanilla"
//! rather than "== 1000000" leaves the numbers tunable upwards without turning this red.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_StorageSeam_HTransportTrucksKeepVanillaCargoCaps : SCR_AutotestCaseBase
{
	//! Vanilla M923A1_transport.et / Ural4320_transport.et.
	static const float MIN_VOLUME = 1000000;

	//! Vanilla M923A1.et is 4500 and Ural4320.et is 5000; the lower of the two is the shared floor.
	static const float MIN_LOAD = 4500;

	static const ResourceName M923A1_TRANSPORT = "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et";
	static const ResourceName URAL_TRANSPORT = "{16C1F16C9B053801}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport.et";

	protected IEntity m_Truck;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!CheckTruck(M923A1_TRANSPORT, "1200 0 1200"))
			return true;

		if (!CheckTruck(URAL_TRANSPORT, "1200 0 1500"))
			return true;

		PrintFormat("Storage seam: both transport trucks carry vanilla's cargo capacity");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns one truck, reads its cargo storage and deletes it again.
	//! \param[in] prefab The transport prefab.
	//! \param[in] offset Where to put it, clear of the other cases' subjects.
	//! \return True when the truck met both floors; false after SetFailure.
	protected bool CheckTruck(ResourceName prefab, vector offset)
	{
		vector position;
		if (!OVT_TEST_StorageSeamSubject.ResolveSpawnPosition(offset, position))
		{
			SetFailure("No town is registered, so there is nowhere sensible to put a test truck");
			return false;
		}

		m_Truck = OVT_Global.SpawnEntityPrefab(prefab, position);
		if (!m_Truck)
		{
			SetFailure("SpawnEntityPrefab() produced no truck from %1", prefab);
			return false;
		}

		SCR_UniversalInventoryStorageComponent cargo = SCR_UniversalInventoryStorageComponent.Cast(m_Truck.FindComponent(SCR_UniversalInventoryStorageComponent));
		if (!cargo)
		{
			SetFailure("%1 has no SCR_UniversalInventoryStorageComponent on its root, so it has no cargo bed to loot into.", prefab);
			return CleanUpAndFail();
		}

		float volume = cargo.GetMaxVolumeCapacity();
		if (volume < MIN_VOLUME)
		{
			SetFailure("%1 carries %2 m3 of cargo volume, below vanilla's own %3. Withdrawals from storage spawn into the VANILLA bed, so a throttled bed is what makes a Take stop half way.",
				prefab,
				volume.ToString(),
				MIN_VOLUME.ToString());
			return CleanUpAndFail();
		}

		float load = cargo.GetMaxLoad();
		if (load < MIN_LOAD)
		{
			SetFailure("%1 carries %2 kg of cargo weight, below vanilla's own %3.",
				prefab,
				load.ToString(),
				MIN_LOAD.ToString());
			return CleanUpAndFail();
		}

		CleanUp();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always false, so the caller reports the failure it has just set.
	protected bool CleanUpAndFail()
	{
		CleanUp();
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void CleanUp()
	{
		if (m_Truck)
		{
			delete m_Truck;
			m_Truck = null;
		}
	}
}

//------------------------------------------------------------------------------------------------
//! The widened loot query takes a loose item off the ground and NEVER takes a storage holder.
//!
//! ⚠ THIS IS THE GUARD ON A DESTRUCTIVE OP. A loot run DELETES every tree it prices, and the query
//! used to accept anything whose damage manager reported destroyed - which a ruined Overthrow
//! building and a wrecked truck both do. The holder exclusion in OVT_StorageLootQuery.FilterLootables
//! is the only thing standing between "Loot battlefield" and a permanently deleted warehouse, and it
//! is invisible to compile-check.
//!
//! The second half is the widening itself: loose gear on the ground is loot now, not just bodies and
//! weapons, so a dropped radio next to the truck must appear in the same result.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_StorageSeam_ILootQueryTakesItemsNotHolders : SCR_AutotestCaseBase
{
	//! Any prefab with an InventoryItemComponent and nothing else interesting about it.
	static const ResourceName LOOSE_ITEM_PREFAB = "{E1A5D4B878AA8980}Prefabs/Items/Equipment/Radios/Radio_R148.et";

	static const float RADIUS = 25;

	protected IEntity m_Box;
	protected IEntity m_Item;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		vector position;
		if (!OVT_TEST_StorageSeamSubject.ResolveSpawnPosition("760 0 640", position))
		{
			SetFailure("No town is registered, so there is nowhere sensible to put a test box");
			return true;
		}

		m_Box = OVT_Global.SpawnEntityPrefab(OVT_TEST_StorageSeamSubject.AMMO_BOX_PREFAB, position);
		if (!m_Box)
		{
			SetFailure("SpawnEntityPrefab() produced no entity from %1", OVT_TEST_StorageSeamSubject.AMMO_BOX_PREFAB);
			return FinishAndCleanUp();
		}

		m_Item = OVT_Global.SpawnEntityPrefab(LOOSE_ITEM_PREFAB, position + "2 0 0");
		if (!m_Item)
		{
			SetFailure("SpawnEntityPrefab() produced no entity from %1", LOOSE_ITEM_PREFAB);
			return FinishAndCleanUp();
		}

		array<IEntity> lootables = new array<IEntity>();
		OVT_StorageLootQuery query = new OVT_StorageLootQuery();
		query.Run(position, RADIUS, lootables);

		if (lootables.Contains(m_Box))
		{
			SetFailure("The loot query offered an ammo box as loot. A loot run DELETES what it prices, so the holder exclusion in OVT_StorageLootQuery.FilterLootables has been lost - every ruined building and wrecked truck in 25 m is now destroyable by one Loot battlefield.");
			return FinishAndCleanUp();
		}

		if (!lootables.Contains(m_Item))
		{
			SetFailure("The loot query did not offer a radio lying on the ground 2 m away. Loose items are loot since the ledger conversion, so the InventoryItemComponent branch has been lost and looting is back to bodies only.");
			return FinishAndCleanUp();
		}

		Print("Storage seam: the loot query takes loose items and leaves holders alone");
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		if (m_Item)
		{
			delete m_Item;
			m_Item = null;
		}

		if (m_Box)
		{
			delete m_Box;
			m_Box = null;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A part a prefab's own slot declares is never a ledger line, and IS detected at runtime.
//!
//! ⚠ THIS CASE EXISTS FOR ONE UNPROVABLE ASSUMPTION. OVT_PrefabPartUtils decides "this item came
//! with its holder's prefab" from InventoryStorageSlot.GetParentContainer() being the holder's
//! BaseLoadoutClothComponent or AttachmentSlotComponent. Nothing in the generated API documents what
//! that call returns for a cloth-declared slot, and if it answers anything else the whole guard is
//! INERT: harness pouches go back to being nameless ledger lines that duplicate on every withdrawal.
//! Compile-check cannot see it and no other case touches it.
//!
//! Vest_SovietHarness_AR is vanilla's own example of the pattern - it is the plain harness plus two
//! Pouch_Soviet_45rnd_RPK74 declared on its cloth slots. Rifle_SVD_PSO is the same trick on a weapon,
//! and plain Rifle_SVD is the control: an optic on THAT one is a player's and must still be credited.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_StorageSeam_JDeclaredPartsAreDetected : SCR_AutotestCaseBase
{
	static const ResourceName HARNESS_AR = "{4711A4CAF64C4CEE}Prefabs/Characters/Vests/Vest_SovietHarness/Variants/Vest_SovietHarness_AR.et";
	static const ResourceName HARNESS_POUCH = "{B6EEF03975F21E4E}Prefabs/Items/Equipment/Accessories/Pouch_Soviet_45rnd_RPK74/Pouch_Soviet_45rnd_RPK74.et";

	static const ResourceName SVD_SCOPED = "{6415B7923DE28C1B}Prefabs/Weapons/Rifles/SVD/Rifle_SVD_PSO.et";
	static const ResourceName SVD_PLAIN = "{3EB02CDAD5F23C82}Prefabs/Weapons/Rifles/SVD/Rifle_SVD.et";
	static const ResourceName OPTIC_PSO1 = "{C850A33226B8F9C1}Prefabs/Weapons/Attachments/Optics/Optic_PSO1/Optic_PSO1.et";

	protected IEntity m_Vest;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (OVT_PrefabPartUtils.GetDeclaredParts(HARNESS_AR).Find(HARNESS_POUCH) == -1)
		{
			SetFailure("Vest_SovietHarness_AR does not report its own declared pouch. The prefab read in OVT_PrefabPartUtils.GetDeclaredParts is not reaching BaseLoadoutClothComponent's Slots array, so every harness part is a ledger line again.");
			return true;
		}

		if (OVT_PrefabPartUtils.GetDeclaredParts(SVD_SCOPED).Find(OPTIC_PSO1) == -1)
		{
			SetFailure("Rifle_SVD_PSO does not report the optic its own prefab declares. AttachmentSlotComponent hangs off WeaponComponent, so the search has lost its child-component flag - a scoped rifle mints a spare optic on every withdrawal.");
			return true;
		}

		if (OVT_PrefabPartUtils.GetDeclaredParts(SVD_PLAIN).Find(OPTIC_PSO1) != -1)
		{
			SetFailure("Plain Rifle_SVD reports the PSO-1 as a declared part. The prefab read is resolving a SIBLING variant's slot, so an optic a player mounted themselves would be destroyed uncredited.");
			return true;
		}

		return CheckRuntimeDetection();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the harness and asserts the engine's own slot answers the way the guard assumes.
	//! \return Always true - the case is over either way.
	protected bool CheckRuntimeDetection()
	{
		vector position;
		if (!OVT_TEST_StorageSeamSubject.ResolveSpawnPosition("780 0 660", position))
		{
			SetFailure("No town is registered, so there is nowhere sensible to put a test vest");
			return true;
		}

		m_Vest = OVT_Global.SpawnEntityPrefab(HARNESS_AR, position);
		if (!m_Vest)
		{
			SetFailure("SpawnEntityPrefab() produced no entity from %1", HARNESS_AR);
			return FinishAndCleanUp();
		}

		array<IEntity> parts = new array<IEntity>();
		OVT_PrefabPartUtils.CollectAttachedParts(m_Vest, parts);

		if (parts.IsEmpty())
		{
			int children = 0;
			IEntity child = m_Vest.GetChildren();
			while (child)
			{
				children += 1;
				child = child.GetSibling();
			}

			SetFailure("A spawned Vest_SovietHarness_AR reports no declared parts among its %1 child entities. IsDeclaredPart's GetParentContainer() test does not hold for a cloth slot, so the whole guard is inert and harness pouches are ledger lines again.", children.ToString());
			return FinishAndCleanUp();
		}

		Print("Storage seam: slot-declared parts are detected on the prefab and at runtime");
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		if (m_Vest)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_Vest);
			m_Vest = null;
		}

		return true;
	}
}


//------------------------------------------------------------------------------------------------
//! The declared-part read must survive a variant delta AND an unloaded resource.
//!
//! WHY THIS FILE NEEDS A SECOND PART CASE. Case J proves the read on Vest_SovietHarness_AR, which
//! declares its pouches in its OWN file, and on prefabs whose resources a spawn had already loaded.
//! Two things it cannot see, both found by a player looting spec-ops:
//!
//!   1. An UNLOADED prefab resource answers with an entity source of ZERO components rather than
//!      with null, and GetDeclaredParts used to cache that as "declares nothing" for the rest of the
//!      session - permanently, silently, and for exactly the prefabs a first loot run meets first.
//!   2. Scabbard_Bayonet_6Kh4 declares its bayonet on an EquipmentStorageComponent InitialStorageSlot
//!      and on a BaseSlotComponent AttachType, neither of which the cloth/attachment read looked at.
//!
//! Vest_6B3.et is the inheritance control: an EMPTY delta over Vest_6B3_base.et, which declares the
//! scabbard on a LoadoutSlotInfo. A worn 6B3 reports Vest_6B3.et as its prefab.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_StorageSeam_LDeclaredPartsResolveInheritance : SCR_AutotestCaseBase
{
	static const ResourceName VEST_6B3 = "{4CBDC206FEF9897C}Prefabs/Characters/Vests/Vest_6B3/Vest_6B3.et";
	static const ResourceName SCABBARD = "{F759F0488730620F}Prefabs/Items/Equipment/Accessories/Scabbard_Bayonet_6Kh4/Scabbard_Bayonet_6Kh4.et";
	static const ResourceName BAYONET = "{98C79F5FAE12F9B6}Prefabs/Weapons/Attachments/Bayonets/Bayonet_6Kh4.et";

	protected IEntity m_Vest;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (OVT_PrefabPartUtils.GetDeclaredParts(VEST_6B3).Find(SCABBARD) == -1)
		{
			SetFailure("Vest_6B3.et does not report the scabbard its BASE declares. The prefab read stops at the variant's own delta, so every piece of vanilla gear that is a thin delta - which is nearly all of it - loses its declared-part guard.");
			return true;
		}

		return CheckSpawnedVest();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the vest, which is what loads the scabbard resource the second read needs.
	//! \return Always true - the case is over either way.
	protected bool CheckSpawnedVest()
	{
		vector position;
		if (!OVT_TEST_StorageSeamSubject.ResolveSpawnPosition("780 0 660", position))
		{
			SetFailure("No town is registered, so there is nowhere sensible to put a test vest");
			return true;
		}

		m_Vest = OVT_Global.SpawnEntityPrefab(VEST_6B3, position);
		if (!m_Vest)
		{
			SetFailure("SpawnEntityPrefab() produced no entity from %1", VEST_6B3);
			return FinishAndCleanUp();
		}

		array<IEntity> parts = new array<IEntity>();
		OVT_PrefabPartUtils.CollectAttachedParts(m_Vest, parts);

		if (parts.IsEmpty())
		{
			SetFailure("A spawned Vest_6B3 reports no declared parts. Its scabbard is then ordinary loot: a nameless raw-path ledger line that no cargo storage will take back, which is how this was found.");
			return FinishAndCleanUp();
		}

		// The scabbard's own resource is loaded now, so its slot reads can answer.
		if (OVT_PrefabPartUtils.GetDeclaredParts(SCABBARD).Find(BAYONET) == -1)
		{
			SetFailure("Scabbard_Bayonet_6Kh4 does not report its own bayonet even with its resource loaded. Neither the EquipmentStorageComponent InitialStorageSlots read nor the BaseSlotComponent AttachType read is reaching it, so a looted scabbard mints a spare bayonet on every withdrawal.");
			return FinishAndCleanUp();
		}

		Print("Storage seam: declared parts survive a variant delta and an unloaded resource");
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		if (m_Vest)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_Vest);
			m_Vest = null;
		}

		return true;
	}
}
