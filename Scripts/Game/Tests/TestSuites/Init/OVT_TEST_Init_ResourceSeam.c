//------------------------------------------------------------------------------------------------
//! TIER B - the resource seam: the manager is actually ON the game mode prefab, it actually loaded
//! resources.conf, and the catalogue it built is usable by everything downstream.
//!
//! WHY THIS FILE EXISTS. The whole catalogue reaches the game through TWO text edits with no
//! compile-time link: an OVT_ResourceManagerComponent block on OVT_OverthrowGameMode.et, and a
//! ResourceName pointing at Configs/Resistance/resources.conf. A typo in either GUID produces no
//! compile error and no runtime error at the point of use - it produces an empty definition table,
//! which reads downstream as "no resource exists", i.e. an empty port category, empty requirement
//! readouts and stores that accept nothing. Nothing in the mod would say why.
//!
//! ONE CLAIM PER CASE:
//!   A. the manager resolves off the game mode and holds a definition table
//!   B. the catalogue holds exactly the MVP four, each with non-zero litres and a non-zero base price
//!   C. every definition id is non-empty, unique, and round-trips through IndexOf
//!   D. the live price starts at base, read through the manager and not off the definition table
//!
//! Later phases extend this file with the store/prefab claims; the shared resolution below is what
//! keeps those cases to "resolve -> assert".
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Shared subject resolution for the cases below.
//------------------------------------------------------------------------------------------------
class OVT_TEST_ResourceSeamSubject
{
	//! What §3.9 of the plan fixes the MVP catalogue at. A fifth resource is a deliberate act and
	//! should update this number with it.
	static const int EXPECTED_DEFINITION_COUNT = 4;

	//! Frame polls allowed for the game mode's components to post-init.
	static const int MAX_POLLS = 300;

	//------------------------------------------------------------------------------------------------
	//! Resolves the manager, or explains precisely which of the two text edits is missing.
	//! \param[out] manager The manager; untouched when it did not resolve.
	//! \param[out] failure A diagnosis when it did not; "" otherwise.
	//! \return True when the manager AND a populated definition table exist.
	static bool ResolveManager(out OVT_ResourceManagerComponent manager, out string failure)
	{
		failure = "";

		OVT_ResourceManagerComponent found = OVT_Global.GetResources();
		if (!found)
		{
			failure = "OVT_Global.GetResources() is null. Prefabs/GameMode/OVT_OverthrowGameMode.et has lost its OVT_ResourceManagerComponent block, so no resource exists anywhere in the mod.";
			return false;
		}

		OVT_ResourceDefs defs = found.GetDefs();
		if (!defs)
		{
			failure = "The resource manager holds no definition table at all - BuildDefs() did not run.";
			return false;
		}

		if (defs.Count() == 0)
		{
			failure = "The resource catalogue is EMPTY. Either m_rResourcesConfigFile on OVT_OverthrowGameMode.et does not point at Configs/Resistance/resources.conf, or that file's .meta GUID does not match the reference.";
			return false;
		}

		manager = found;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The manager resolves off the game mode and carries a definition table.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ResourceSeam_AManagerResolves : SCR_AutotestCaseBase
{
	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceManagerComponent manager;
		string failure;

		if (!OVT_TEST_ResourceSeamSubject.ResolveManager(manager, failure))
		{
			m_iPolls += 1;
			if (m_iPolls > OVT_TEST_ResourceSeamSubject.MAX_POLLS)
			{
				SetFailure("%1 (still true after %2 frames)", failure, m_iPolls.ToString());
				return true;
			}

			return false;
		}

		PrintFormat("Resource seam: the manager resolves with %1 definitions", manager.GetDefs().Count().ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The catalogue holds the MVP four, and every one of them is usable.
//!
//! Non-zero litres is the load-bearing half: a resource that occupies nothing would make every
//! capacity check trivially true, so a truck would hold an unbounded amount of it and the whole
//! logistics constraint would quietly not exist.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ResourceSeam_BCatalogueIsPopulated : SCR_AutotestCaseBase
{
	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceManagerComponent manager;
		string failure;

		if (!OVT_TEST_ResourceSeamSubject.ResolveManager(manager, failure))
		{
			m_iPolls += 1;
			if (m_iPolls > OVT_TEST_ResourceSeamSubject.MAX_POLLS)
			{
				SetFailure("%1 (still true after %2 frames)", failure, m_iPolls.ToString());
				return true;
			}

			return false;
		}

		OVT_ResourceDefs defs = manager.GetDefs();

		if (defs.Count() != OVT_TEST_ResourceSeamSubject.EXPECTED_DEFINITION_COUNT)
		{
			SetFailure("resources.conf produced %1 definitions, expected %2. An entry was dropped by BuildDefs() (empty or duplicate id) or the .conf gained one without this case being updated.",
				defs.Count().ToString(),
				OVT_TEST_ResourceSeamSubject.EXPECTED_DEFINITION_COUNT.ToString());
			return true;
		}

		for (int i = 0; i < defs.Count(); i++)
		{
			if (defs.LitresAt(i) <= 0)
			{
				SetFailure("Definition %1 ('%2') occupies %3 litres per unit. A zero-volume resource makes every capacity check trivially true, so no store would ever fill.",
					i.ToString(),
					defs.IdAt(i),
					defs.LitresAt(i).ToString());
				return true;
			}

			if (defs.BasePriceAt(i) <= 0)
			{
				SetFailure("Definition %1 ('%2') has base price %3. A free resource would break the drift band, which is derived from base.",
					i.ToString(),
					defs.IdAt(i),
					defs.BasePriceAt(i).ToString());
				return true;
			}
		}

		PrintFormat("Resource seam: all %1 definitions carry non-zero litres and a non-zero base price", defs.Count().ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Every definition id is non-empty, unique and resolvable.
//!
//! The id is the save key and the requirement-list key. Two entries sharing one id would make a
//! requirement resolve to whichever the table found first, and a save would silently merge them.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ResourceSeam_CIdsAreUnique : SCR_AutotestCaseBase
{
	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceManagerComponent manager;
		string failure;

		if (!OVT_TEST_ResourceSeamSubject.ResolveManager(manager, failure))
		{
			m_iPolls += 1;
			if (m_iPolls > OVT_TEST_ResourceSeamSubject.MAX_POLLS)
			{
				SetFailure("%1 (still true after %2 frames)", failure, m_iPolls.ToString());
				return true;
			}

			return false;
		}

		OVT_ResourceDefs defs = manager.GetDefs();
		array<string> seen = new array<string>();

		for (int i = 0; i < defs.Count(); i++)
		{
			string id = defs.IdAt(i);

			if (id == "")
			{
				SetFailure("Definition %1 has an empty id. It can never be saved, required or looked up.", i.ToString());
				return true;
			}

			if (seen.Find(id) > -1)
			{
				SetFailure("The id '%1' appears twice in resources.conf. Requirements and saves key on the id, so the duplicate would shadow the original.", id);
				return true;
			}

			if (defs.IndexOf(id) != i)
			{
				SetFailure("IndexOf('%1') answered %2, expected %3 - the id does not round-trip to its own index.",
					id,
					defs.IndexOf(id).ToString(),
					i.ToString());
				return true;
			}

			seen.Insert(id);
		}

		PrintFormat("Resource seam: %1 unique, resolvable definition ids", defs.Count().ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The live price starts at base and is read off the manager, not the definition table.
//!
//! This is the "prices are state" seam. If the price table were never filled, GetPrice() would answer
//! 0 and every resource would be free; if a call site read the base as the live price, drift would be
//! invisible. Asserting both together at startup is the cheapest place to catch either.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ResourceSeam_DPricesStartAtBase : SCR_AutotestCaseBase
{
	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceManagerComponent manager;
		string failure;

		if (!OVT_TEST_ResourceSeamSubject.ResolveManager(manager, failure))
		{
			m_iPolls += 1;
			if (m_iPolls > OVT_TEST_ResourceSeamSubject.MAX_POLLS)
			{
				SetFailure("%1 (still true after %2 frames)", failure, m_iPolls.ToString());
				return true;
			}

			return false;
		}

		OVT_ResourceDefs defs = manager.GetDefs();

		for (int i = 0; i < defs.Count(); i++)
		{
			if (manager.GetStoredPrice(i) != defs.BasePriceAt(i))
			{
				SetFailure("Definition '%1' stores price %2 but its config base is %3. The price table was not initialised from base.",
					defs.IdAt(i),
					manager.GetStoredPrice(i).ToString(),
					defs.BasePriceAt(i).ToString());
				return true;
			}

			if (manager.GetPrice(i) <= 0)
			{
				SetFailure("Definition '%1' quotes a live price of %2 - it would be free at the port.",
					defs.IdAt(i),
					manager.GetPrice(i).ToString());
				return true;
			}

			if (manager.GetSellPrice(i) <= 0 || manager.GetSellPrice(i) > manager.GetPrice(i))
			{
				SetFailure("Definition '%1' sells for %2 against a buy price of %3. The sell ratio is not applied, and a sell price at or above buy is a money printer.",
					defs.IdAt(i),
					manager.GetSellPrice(i).ToString(),
					manager.GetPrice(i).ToString());
				return true;
			}
		}

		PrintFormat("Resource seam: %1 live prices initialised to base", defs.Count().ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Subject resolution for the holder cases below (E onwards).
//!
//! WHY THESE CASES EXIST. The whole hauling constraint reaches the game through PREFAB edits and
//! nothing else - there is no runtime component creation in EnforceScript. A truck with no store has
//! no cargo action and nothing anywhere says why; a truck whose store resolved 0 litres looks exactly
//! the same; and the 20 m3 transport override sits on top of a 15 m3 base, so a dropped override is a
//! silent 25% capacity cut. None of it produces a compile error.
//------------------------------------------------------------------------------------------------
class OVT_TEST_ResourceStoreSubject
{
	//! The transport truck the plan gives the 20 m3 override to. Pinned deliberately: the claim is
	//! about THIS prefab's authored override, not about whatever the economy happens to sell.
	static const ResourceName TRANSPORT_TRUCK_PREFAB = "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et";

	//! The dropped crate pile.
	static const ResourceName PILE_PREFAB = "{6A8E2E0000000100}Prefabs/Props/Resources/OVT_ResourcePile.et";

	//! 20 m3 in litres - the transport override, not the 15 000 the truck base authors.
	static const int TRANSPORT_CAPACITY_LITRES = 20000;

	//! Prefab path fragment every warehouse variant shares.
	static const string WAREHOUSE_PREFAB_FRAGMENT = "Warehouse_01";

	//! The two vanilla families that descend from Wheeled_Truck_Base.et. A "car" subject carrying one
	//! of these in its path would be a truck, and the no-store claim would be asserting nothing.
	static const string TRUCK_FAMILY_M923A1 = "M923A1";
	static const string TRUCK_FAMILY_URAL = "Ural4320";

	//! Frame polls allowed for the economy catalogue and the world to be ready.
	static const int MAX_POLLS = 900;

	//------------------------------------------------------------------------------------------------
	//! A legal, registered wheeled vehicle that is NOT one of the two truck families - the subject for
	//! "cars cannot haul".
	//! \param[out] prefab The prefab to spawn; untouched when nothing matched.
	//! \return True when a prefab was resolved.
	static bool FindNonTruckVehicle(out ResourceName prefab)
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

			if (candidate.IndexOf(TRUCK_FAMILY_M923A1) > -1)
				continue;

			if (candidate.IndexOf(TRUCK_FAMILY_URAL) > -1)
				continue;

			if (!economy.IsRegisteredResource(candidate))
				continue;

			if (economy.GetParkingType(economy.GetInventoryId(candidate)) == OVT_ParkingType.PARKING_TRUCK)
				continue;

			prefab = candidate;
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Somewhere to put a spawned subject, clear of the town and of the storage seam's own subjects.
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
//! ONE INSTANCE PER CALL, accumulator on the instance - the same rule the production queries follow.
//! By prefab path rather than by position, so moving the building in
//! Worlds/MP/OVT_Campaign_Test_Layers/default.layer cannot turn the case green-by-absence.
//------------------------------------------------------------------------------------------------
class OVT_TEST_ResourceWarehouseFinder : Managed
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
		if (prefab.IndexOf(OVT_TEST_ResourceStoreSubject.WAREHOUSE_PREFAB_FRAGMENT) > -1)
			m_Found = e;

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! The transport truck resolves a 20 000 litre store, and it starts empty.
//!
//! Two prefab edits meet here: the same-GUID delta of vanilla Wheeled_Truck_Base.et {E03D5609EEA6E03D}
//! puts the component on every truck at 15 m3, and M923A1_transport.et restates the SAME component
//! GUID to raise it to 20. A minted GUID on the override would leave the truck with two stores and
//! FindComponent would answer whichever the engine ordered first; a dropped delta leaves no store at
//! all. Neither is visible to the compiler.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ResourceSeam_ETruckStoreIsCapped : SCR_AutotestCaseBase
{
	protected int m_iPolls;
	protected IEntity m_Vehicle;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!m_Vehicle)
		{
			vector position;
			if (!OVT_TEST_ResourceStoreSubject.ResolveSpawnPosition("600 0 900", position))
			{
				m_iPolls += 1;
				if (m_iPolls > OVT_TEST_ResourceStoreSubject.MAX_POLLS)
				{
					SetFailure("No town is registered after %1 frames, so there is nowhere to put a test truck", m_iPolls.ToString());
					return true;
				}

				return false;
			}

			m_Vehicle = OVT_Global.SpawnEntityPrefab(OVT_TEST_ResourceStoreSubject.TRANSPORT_TRUCK_PREFAB, position);
			if (!m_Vehicle)
			{
				SetFailure("SpawnEntityPrefab() produced no vehicle from %1 - the prefab GUID in this case no longer resolves", OVT_TEST_ResourceStoreSubject.TRANSPORT_TRUCK_PREFAB);
				return true;
			}
		}

		OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(m_Vehicle);
		if (!store)
		{
			SetFailure("The transport truck has no OVT_ResourceStoreComponent. Prefabs/Vehicles/Core/Wheeled_Truck_Base.et has lost its block or its .meta GUID no longer matches vanilla's {E03D5609EEA6E03D}, so NO truck in the game can haul and every cargo action is invisible mod-wide.");
			return FinishAndCleanUp();
		}

		if (store.GetCapacityLitres() != OVT_TEST_ResourceStoreSubject.TRANSPORT_CAPACITY_LITRES)
		{
			SetFailure("The transport truck holds %1 litres, expected %2. 15000 means M923A1_transport.et lost its override; -1 means it was authored as unlimited; 0 means the m3 attribute is missing.",
				store.GetCapacityLitres().ToString(),
				OVT_TEST_ResourceStoreSubject.TRANSPORT_CAPACITY_LITRES.ToString());
			return FinishAndCleanUp();
		}

		if (store.GetUsedLitres() != 0 || store.GetFreeLitres() != OVT_TEST_ResourceStoreSubject.TRANSPORT_CAPACITY_LITRES)
		{
			SetFailure("A freshly spawned truck reports %1 litres used and %2 free against a %3 litre cap - a new holder must start empty with the whole cap free.",
				store.GetUsedLitres().ToString(),
				store.GetFreeLitres().ToString(),
				OVT_TEST_ResourceStoreSubject.TRANSPORT_CAPACITY_LITRES.ToString());
			return FinishAndCleanUp();
		}

		PrintFormat("Resource seam: the transport truck resolves a %1 litre store", store.GetCapacityLitres().ToString());
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
//! The test world's warehouse resolves an unlimited resource store beside its item ledger.
//!
//! This is the one edit here that a typo in a GUID or a parent path turns into a file the engine never
//! loads, and it is also the two-ledgers wall in one entity: OVT_StorageComponent counts items,
//! OVT_ResourceStoreComponent counts litres, and both must be present at once.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ResourceSeam_FWarehouseStoreIsUnlimited : SCR_AutotestCaseBase
{
	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TEST_ResourceWarehouseFinder finder = new OVT_TEST_ResourceWarehouseFinder();
		IEntity warehouse = finder.Find();

		if (!warehouse)
		{
			m_iPolls += 1;
			if (m_iPolls > OVT_TEST_ResourceStoreSubject.MAX_POLLS)
			{
				SetFailure("No Warehouse_01 building exists in the test world after %1 frames. Worlds/MP/OVT_Campaign_Test_Layers/default.layer has lost the one the warehouse claims are asserted against.", m_iPolls.ToString());
				return true;
			}

			return false;
		}

		OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(warehouse);
		if (!store)
		{
			SetFailure("The warehouse has no OVT_ResourceStoreComponent. The same-GUID delta Prefabs/Structures/Industrial/Houses/Warehouse_01/Warehouse_01_Base.et {E35EA41864A3B0ED} has lost its block, so no warehouse anywhere can hold resources.");
			return true;
		}

		if (store.GetCapacityLitres() != OVT_ResourceStoreComponent.UNLIMITED_CAPACITY)
		{
			SetFailure("The warehouse holds %1 litres, expected -1 (unlimited). A capped warehouse would silently refuse deliveries once it filled.", store.GetCapacityLitres().ToString());
			return true;
		}

		if (!OVT_StorageUtils.GetStorage(warehouse))
		{
			SetFailure("The warehouse gained a resource store but lost its OVT_StorageComponent. The two ledgers are meant to sit side by side on one entity; the resource edit must not have displaced the item one.");
			return true;
		}

		PrintFormat("Resource seam: the warehouse resolves an unlimited resource store beside its item ledger");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A spawned crate pile resolves an unlimited store.
//!
//! Unlimited by construction, not by policy: a pile is a heap on the ground, so a cap could only
//! produce a drop that half-lands. int.MAX free litres is what the unlimited branch answers.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ResourceSeam_GPileStoreIsUnlimited : SCR_AutotestCaseBase
{
	protected int m_iPolls;
	protected IEntity m_Pile;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!m_Pile)
		{
			vector position;
			if (!OVT_TEST_ResourceStoreSubject.ResolveSpawnPosition("640 0 900", position))
			{
				m_iPolls += 1;
				if (m_iPolls > OVT_TEST_ResourceStoreSubject.MAX_POLLS)
				{
					SetFailure("No town is registered after %1 frames, so there is nowhere to put a test pile", m_iPolls.ToString());
					return true;
				}

				return false;
			}

			m_Pile = OVT_Global.SpawnEntityPrefab(OVT_TEST_ResourceStoreSubject.PILE_PREFAB, position);
			if (!m_Pile)
			{
				SetFailure("SpawnEntityPrefab() produced no pile from %1 - the prefab or its .meta GUID no longer resolves", OVT_TEST_ResourceStoreSubject.PILE_PREFAB);
				return true;
			}
		}

		OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(m_Pile);
		if (!store)
		{
			SetFailure("The crate pile has no OVT_ResourceStoreComponent, so a dropped load would have nowhere to land");
			return FinishAndCleanUp();
		}

		if (store.GetCapacityLitres() != OVT_ResourceStoreComponent.UNLIMITED_CAPACITY)
		{
			SetFailure("The crate pile holds %1 litres, expected -1 (unlimited)", store.GetCapacityLitres().ToString());
			return FinishAndCleanUp();
		}

		if (store.GetFreeLitres() != int.MAX)
		{
			SetFailure("An unlimited pile reports %1 free litres, expected int.MAX - the unlimited branch of FreeLitres() is not being taken", store.GetFreeLitres().ToString());
			return FinishAndCleanUp();
		}

		PrintFormat("Resource seam: the crate pile resolves an unlimited store");
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		if (m_Pile)
		{
			delete m_Pile;
			m_Pile = null;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The two radius queries answer different questions about the same pair of entities.
//!
//! A truck parked beside a construction site must never be drained by it, so the pile query filters on
//! OVT_ResourcePileComponent (D16's whole purpose) while the holder query filters on the store. This
//! is also the only case that runs either query object, so without it both ship untested until the
//! phase that first calls them.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ResourceSeam_HQueriesSeparatePilesFromHolders : SCR_AutotestCaseBase
{
	//! Comfortably larger than the 8 m separation below and far smaller than the distance to any other
	//! case's subject.
	static const float QUERY_RADIUS = 20;

	protected int m_iPolls;
	protected IEntity m_Pile;
	protected IEntity m_Vehicle;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!m_Pile)
		{
			vector position;
			if (!OVT_TEST_ResourceStoreSubject.ResolveSpawnPosition("680 0 900", position))
			{
				m_iPolls += 1;
				if (m_iPolls > OVT_TEST_ResourceStoreSubject.MAX_POLLS)
				{
					SetFailure("No town is registered after %1 frames, so the query case has nowhere to put its subjects", m_iPolls.ToString());
					return true;
				}

				return false;
			}

			m_Pile = OVT_Global.SpawnEntityPrefab(OVT_TEST_ResourceStoreSubject.PILE_PREFAB, position);
			m_Vehicle = OVT_Global.SpawnEntityPrefab(OVT_TEST_ResourceStoreSubject.TRANSPORT_TRUCK_PREFAB, position + "8 0 0");

			if (!m_Pile || !m_Vehicle)
			{
				SetFailure("The query case could not spawn both a pile and a truck, so it has nothing to separate");
				return FinishAndCleanUp();
			}
		}

		array<IEntity> holders = new array<IEntity>();
		OVT_ResourceHolderQuery holderQuery = new OVT_ResourceHolderQuery();
		holderQuery.Run(m_Pile.GetOrigin(), QUERY_RADIUS, holders);

		if (!holders.Contains(m_Pile) || !holders.Contains(m_Vehicle))
		{
			m_iPolls += 1;
			if (m_iPolls > OVT_TEST_ResourceStoreSubject.MAX_POLLS)
			{
				SetFailure("The holder query found %1 holders in %2 m after %3 frames; both the pile and the truck carry a store and both should be there.",
					holders.Count().ToString(),
					QUERY_RADIUS.ToString(),
					m_iPolls.ToString());
				return FinishAndCleanUp();
			}

			return false;
		}

		array<IEntity> piles = new array<IEntity>();
		OVT_ResourcePileQuery pileQuery = new OVT_ResourcePileQuery();
		pileQuery.Run(m_Pile.GetOrigin(), QUERY_RADIUS, piles);

		if (!piles.Contains(m_Pile))
		{
			SetFailure("The pile query missed the pile it was centred on, so a dropped load could never merge into an existing heap");
			return FinishAndCleanUp();
		}

		if (piles.Contains(m_Vehicle))
		{
			SetFailure("The pile query returned the parked truck. It must filter on OVT_ResourcePileComponent, not on the store, or a construction site would drain the nearest lorry.");
			return FinishAndCleanUp();
		}

		PrintFormat("Resource seam: the holder query found %1 holders and the pile query %2 pile(s)", holders.Count().ToString(), piles.Count().ToString());
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		if (m_Pile)
		{
			delete m_Pile;
			m_Pile = null;
		}

		if (m_Vehicle)
		{
			delete m_Vehicle;
			m_Vehicle = null;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A car resolves no store at all, so it cannot haul.
//!
//! Non-truck hauling is out of scope BY CONSTRUCTION: the component is authored on
//! Wheeled_Truck_Base.et, which only the M923A1 and Ural4320 families descend from, rather than on
//! Wheeled_Base.et behind a runtime capacity rule. Moving the block up one prefab would put a cargo
//! action on every civilian hatchback in the game, and nothing but this case would notice.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ResourceSeam_ICarHasNoStore : SCR_AutotestCaseBase
{
	protected int m_iPolls;
	protected IEntity m_Vehicle;
	protected ResourceName m_sPrefab;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!m_Vehicle)
		{
			vector position;
			if (!OVT_TEST_ResourceStoreSubject.FindNonTruckVehicle(m_sPrefab) || !OVT_TEST_ResourceStoreSubject.ResolveSpawnPosition("720 0 900", position))
			{
				m_iPolls += 1;
				if (m_iPolls > OVT_TEST_ResourceStoreSubject.MAX_POLLS)
				{
					SetFailure("After %1 frames the economy still knows no registered wheeled vehicle outside the two truck families, or no town is registered, so 'a car cannot haul' has no subject to assert against.", m_iPolls.ToString());
					return true;
				}

				return false;
			}

			m_Vehicle = OVT_Global.SpawnEntityPrefab(m_sPrefab, position);
			if (!m_Vehicle)
			{
				SetFailure("SpawnEntityPrefab() produced no vehicle from %1", m_sPrefab);
				return true;
			}
		}

		OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(m_Vehicle);
		if (store)
		{
			SetFailure("The car %1 carries a resource store of %2 litres. The component has escaped Wheeled_Truck_Base.et onto a shared vehicle base, so every car in the game can now haul.",
				m_sPrefab,
				store.GetCapacityLitres().ToString());
			return FinishAndCleanUp();
		}

		PrintFormat("Resource seam: the car %1 carries no resource store", m_sPrefab);
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
//! OVT_ResourceRequestComponent is actually ON the local player's OVT_OverthrowController.
//!
//! WHY THIS CASE EXISTS. The component reaches the game through ONE text edit with no compile-time
//! link: a block on Prefabs/GameMode/OVT_OverthrowController.et. Without it the accessor answers null,
//! every call site null-guards by contract and returns, and the whole resource wire - take, put, drop
//! to the ground, port import, port export and build-from-site - silently never happens. There is no
//! compile error, no runtime error and no log line, and there is no older path to fall back to
//! because this component IS the path.
//!
//! TWO CLAIMS, ONE PRECONDITION:
//!   1. the component resolves through the epic-level accessor at all;
//!   2. what comes back is the instance carried by THIS player's own controller entity - not merely
//!      "a component of that type from somewhere", which a Get() that searched the wrong entity would
//!      also satisfy, and which would mean every transfer was resolved by the server as coming from
//!      another player.
//!
//! THE POLL IS A PRECONDITION, NOT A RETRY (no maxAttempts anywhere in this project).
//! OVT_PlayerManagerComponent.SetupPlayer() spawns the controller when the player enters the world,
//! which is not instantaneous at world load. Expiry is itself a named failure carrying the diagnosis,
//! so nothing here can pass by being asked at a luckier moment.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ResourceSeam_JRequestComponentResolves : SCR_AutotestCaseBase
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
				SetFailure("OVT_Global.GetController() was still null after %1 polls (local player id %2). Nothing on the controller seam is reachable from this machine, so this case cannot say anything about OVT_ResourceRequestComponent either way.",
					m_iPolls.ToString(),
					SCR_PlayerController.GetLocalPlayerId().ToString());
				return true;
			}

			return false;
		}

		// Claim 1 - it is wired.
		OVT_ResourceRequestComponent viaAccessor = OVT_ControllerComponent<OVT_ResourceRequestComponent>.Get();
		if (!viaAccessor)
		{
			SetFailure("OVT_ControllerComponent<OVT_ResourceRequestComponent>.Get() returned null while a controller entity exists. Prefabs/GameMode/OVT_OverthrowController.et is not carrying the component, so every resource transfer - take, put, drop to the ground, port import, port export and build-from-site - silently never reaches the server.");
			return true;
		}

		// Claim 2 - it is OUR controller's instance, not somebody else's.
		OVT_ResourceRequestComponent onEntity = OVT_ResourceRequestComponent.Cast(controller.FindComponent(OVT_ResourceRequestComponent));
		if (viaAccessor != onEntity)
		{
			SetFailure("OVT_ControllerComponent<OVT_ResourceRequestComponent>.Get() did not return the instance on the local player's own controller entity. Every resource request would then be sent through another player's seam, and the server would resolve the caller - and debit the money - as that player.");
			return true;
		}

		PrintFormat("Resource seam: OVT_ResourceRequestComponent resolves off the local controller (found after %1 poll(s))", m_iPolls.ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! TIER B - §3.14's difficulty table actually reached the presets, and the three accessors read it.
//!
//! WHY THIS CASE EXISTS. The whole difficulty half of price drift and construction cost reaches the
//! game through FOUR text edits with no compile-time link - one line each in Difficulty_Easy.conf,
//! _Hard.conf, _Extreme.conf and _Insane.conf. A misspelt key in a .conf is not a parse error and
//! not a runtime error: the field simply keeps its attribute default of 1, and every preset then
//! prices resources identically. Nothing in the game would say so, and a play-test cannot tell 1.25
//! from 1 without a spreadsheet.
//!
//! THE FOUR SHIPPED OVERRIDES ARE ASSERTED BY VALUE, and Normal and Test World are asserted to author
//! NONE - a .conf overrides only where it differs from the attribute default, and an override
//! creeping into Normal would move the baseline every other preset is tuned against.
//!
//! PRESETS ARE FOUND BY NAME, NEVER BY INDEX (a world layer APPENDS to the prefab's list).
//!
//! PROVEN ABLE TO FAIL (fail proof recorded in context.md; execution belongs to the phase's suite
//! run): drop resourcePriceMultiplier from Difficulty_Hard.conf and the case goes red naming the
//! field, the preset, the value read and the value §3.14 fixes.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ResourceSeam_KDifficultyMultipliersArrive : SCR_AutotestCaseBase
{
	//! §3.14's table, one row per preset that authors an override.
	protected static const ref array<string> OVERRIDING_PRESETS = {"Easy", "Hard", "Extreme", "Insane"};
	protected static const ref array<float> EXPECTED_BUILD_COST = {0.8, 1.5, 3, 4};
	protected static const ref array<float> EXPECTED_PRICE = {0.8, 1.25, 1.5, 2};
	protected static const ref array<float> EXPECTED_VOLATILITY = {0.5, 1.5, 2, 2};

	//! Presets that deliberately author none of the three and must read the attribute default.
	protected static const ref array<string> DEFAULTING_PRESETS = {"Normal", "Test World"};

	//! Authored floats are compared with a tolerance, not by equality - EnforceScript float is binary32.
	protected static const float TOLERANCE = 0.0001;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("OVT_Global.GetConfig() is null, so nothing on this machine can price a resource or a requirement");
			return true;
		}

		if (!config.m_aDifficultyPresets || config.m_aDifficultyPresets.IsEmpty())
		{
			SetFailure("The config component carries no difficulty presets, so the resource ladder cannot be read");
			return true;
		}

		for (int i = 0; i < OVERRIDING_PRESETS.Count(); i++)
		{
			OVT_DifficultySettings preset = FindPreset(config, OVERRIDING_PRESETS[i]);
			if (!preset)
			{
				SetFailure("No shipped difficulty preset named '%1' is loaded - the resource ladder is incomplete", OVERRIDING_PRESETS[i]);
				return true;
			}

			string failure = CheckField(OVERRIDING_PRESETS[i], "buildableResourceCostMultiplier", preset.buildableResourceCostMultiplier, EXPECTED_BUILD_COST[i]);
			if (failure == "")
				failure = CheckField(OVERRIDING_PRESETS[i], "resourcePriceMultiplier", preset.resourcePriceMultiplier, EXPECTED_PRICE[i]);

			if (failure == "")
				failure = CheckField(OVERRIDING_PRESETS[i], "resourcePriceVolatility", preset.resourcePriceVolatility, EXPECTED_VOLATILITY[i]);

			if (failure != "")
			{
				SetFailure(failure);
				return true;
			}
		}

		foreach (string defaulting : DEFAULTING_PRESETS)
		{
			OVT_DifficultySettings preset = FindPreset(config, defaulting);
			if (!preset)
			{
				// "Test World" exists only in the test world's layer. "Normal" is shipped on the game
				// mode prefab and its absence would make this half of the case pass vacuously.
				if (defaulting != "Normal")
					continue;

				SetFailure("No difficulty preset named 'Normal' is loaded - that is the baseline every other preset is tuned against");
				return true;
			}

			string failure = CheckField(defaulting, "buildableResourceCostMultiplier", preset.buildableResourceCostMultiplier, 1);
			if (failure == "")
				failure = CheckField(defaulting, "resourcePriceMultiplier", preset.resourcePriceMultiplier, 1);

			if (failure == "")
				failure = CheckField(defaulting, "resourcePriceVolatility", preset.resourcePriceVolatility, 1);

			if (failure != "")
			{
				SetFailure("%1 - this preset authors none of the three on purpose, so all three must read the attribute default", failure);
				return true;
			}
		}

		// --- THE ACCESSORS. The live difficulty answers through them, and the requirement scaler keeps
		//     its floor - a requirement that scaled to zero would make a buildable free.
		if (config.GetResourcePriceMultiplier() <= 0)
		{
			SetFailure("GetResourcePriceMultiplier() is %1 - at or below zero every live price would collapse to the floor of 1", config.GetResourcePriceMultiplier().ToString());
			return true;
		}

		if (config.GetResourcePriceVolatility() < 0)
		{
			SetFailure("GetResourcePriceVolatility() is %1 - a negative volatility inverts every drift step", config.GetResourcePriceVolatility().ToString());
			return true;
		}

		if (config.GetBuildableResourceCost(1) < 1)
		{
			SetFailure("GetBuildableResourceCost(1) is %1 - a requirement of one unit must never scale away to nothing", config.GetBuildableResourceCost(1).ToString());
			return true;
		}

		if (config.GetBuildableResourceCost(0) != 0)
		{
			SetFailure("GetBuildableResourceCost(0) is %1 - an unauthored requirement must stay unauthored", config.GetBuildableResourceCost(0).ToString());
			return true;
		}

		PrintFormat("Resource difficulty: the four shipped overrides carry the planned values, and this machine reads a price multiplier of %1 and a volatility of %2",
			config.GetResourcePriceMultiplier().ToString(),
			config.GetResourcePriceVolatility().ToString());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] presetName Which preset is being read.
	//! \param[in] fieldName Which field, spelled as the .conf spells it.
	//! \param[in] actual What the loaded preset holds.
	//! \param[in] expected What §3.14 fixes it at.
	//! \return An empty string when they agree, or the diagnosis.
	protected string CheckField(string presetName, string fieldName, float actual, float expected)
	{
		if (Math.AbsFloat(actual - expected) <= TOLERANCE)
			return "";

		return string.Format("Preset '%1' reads %2 = %3, but the difficulty table fixes it at %4. A misspelt key in Configs/Difficulty/Difficulty_%1.conf reads back as the attribute default and is not a parse error",
			presetName, fieldName, actual.ToString(), expected.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] config The config component holding the preset list.
	//! \param[in] presetName Name to match exactly.
	//! \return The matching preset, or null.
	protected OVT_DifficultySettings FindPreset(notnull OVT_OverthrowConfigComponent config, string presetName)
	{
		foreach (OVT_DifficultySettings preset : config.m_aDifficultyPresets)
		{
			if (preset && preset.name == presetName)
				return preset;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! The construction site seam: the prefab is wired, it carries the three components the build path
//! needs, and every authored requirement names a resource the catalogue knows.
//!
//! WHY THIS EXISTS. Every one of these is a SILENT failure. An unwired m_rDefaultSitePrefab makes
//! PlaceConstructionSite() refuse, so a Guard Tower simply cannot be built and no compiler says why.
//! A site with no OVT_BuildableComponent gets no persistence configuration (D16) and no removal
//! flow. A requirement naming an id resources.conf does not carry is skipped by the readout and
//! demanded by IsSatisfied(), so the site can never be finished by anything.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ResourceSeam_LConstructionSiteSeam : SCR_AutotestCaseBase
{
	//! Distinct from every other seam subject's offset.
	static const vector SPAWN_OFFSET = "680 0 940";

	static const int SAVED_BUILDABLE_INDEX = 3;
	static const int SAVED_PREFAB_INDEX = 1;
	static const vector SAVED_ANGLES = "0 137 0";

	protected int m_iPolls;
	protected IEntity m_Site;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
		{
			SetFailure("OVT_Global.GetResources() is null, so there is no manager to hold a site prefab");
			return true;
		}

		if (resources.GetDefaultSitePrefab() == ResourceName.Empty)
		{
			SetFailure("OVT_ResourceManagerComponent.m_rDefaultSitePrefab is unwired on Prefabs/GameMode/OVT_OverthrowGameMode.et. Every buildable that costs resources then refuses to place a site, so the Guard Tower, the Helipad and the Garage cannot be built at all - and nothing in the log says which buildables they are.");
			return true;
		}

		if (!m_Site)
		{
			vector position;
			if (!OVT_TEST_ResourceStoreSubject.ResolveSpawnPosition(SPAWN_OFFSET, position))
			{
				m_iPolls += 1;
				if (m_iPolls > OVT_TEST_ResourceStoreSubject.MAX_POLLS)
				{
					SetFailure("No town is registered after %1 frames, so there is nowhere to put a test construction site", m_iPolls.ToString());
					return true;
				}

				return false;
			}

			m_Site = OVT_Global.SpawnEntityPrefab(resources.GetDefaultSitePrefab(), position);
			if (!m_Site)
			{
				SetFailure("SpawnEntityPrefab() produced no construction site from %1 - the wired prefab or its .meta GUID no longer resolves", resources.GetDefaultSitePrefab());
				return true;
			}
		}

		OVT_ConstructionSiteComponent site = OVT_ComponentFinder<OVT_ConstructionSiteComponent>.Find(m_Site);
		if (!site)
		{
			SetFailure("The construction site prefab carries no OVT_ConstructionSiteComponent, so it remembers nothing and no build action can ever finish it");
			return FinishAndCleanUp();
		}

		if (!OVT_ComponentFinder<OVT_BuildableComponent>.Find(m_Site))
		{
			SetFailure("The construction site prefab carries no OVT_BuildableComponent. It then matches no Overthrow EntityPersistenceConfig (D16), so it does not survive a reload, and OVT_BuildContext.DoRemove() cannot remove it either.");
			return FinishAndCleanUp();
		}

		if (!RplComponent.Cast(m_Site.FindComponent(RplComponent)))
		{
			SetFailure("The construction site prefab carries no RplComponent, so it has no RplId and no client can ever name it in a build request (BUG-193's shape)");
			return FinishAndCleanUp();
		}

		// What PlaceConstructionSite() stamps, read back through the getters CompleteSite() uses.
		site.Initialize(SAVED_BUILDABLE_INDEX, SAVED_PREFAB_INDEX, SAVED_ANGLES, "#OVT-Resource_ConstructionSite");

		if (site.GetBuildableIndex() != SAVED_BUILDABLE_INDEX || site.GetPrefabIndex() != SAVED_PREFAB_INDEX)
		{
			SetFailure("A stamped construction site reads back buildable %1 / prefab %2, expected %3 / 1", site.GetBuildableIndex().ToString(), site.GetPrefabIndex().ToString(), SAVED_BUILDABLE_INDEX.ToString());
			return FinishAndCleanUp();
		}

		if (site.GetAngles() != SAVED_ANGLES)
		{
			SetFailure("A stamped construction site reads back angles %1, expected %2 - the finished building would stand at the wrong angle", site.GetAngles().ToString(), SAVED_ANGLES.ToString());
			return FinishAndCleanUp();
		}

		string unknown = FindUnknownRequirementId();
		if (unknown != "")
		{
			SetFailure(string.Format("buildables.conf authors a resource requirement for '%1', which resources.conf does not carry. The readout silently drops the line while IsSatisfied() still demands it, so that buildable's construction site can never be finished by anything.", unknown));
			return FinishAndCleanUp();
		}

		int requirementBearing = CountRequirementBearingBuildables();
		if (requirementBearing != 4)
		{
			SetFailure(string.Format("%1 buildable(s) author resource requirements, expected 4 - D17's three (Guard Tower, Helipad, Garage) plus the Warehouse this feature adds. The untouched majority is the standing proof that an empty requirement list changes nothing.", requirementBearing.ToString()));
			return FinishAndCleanUp();
		}

		PrintFormat("Resource seam: the construction site prefab is wired and complete, and all 4 requirement-bearing buildables name known resources");
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The first authored requirement id the catalogue does not know, or "".
	protected string FindUnknownRequirementId()
	{
		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resources || !resistance || !resistance.m_BuildablesConfig || !resistance.m_BuildablesConfig.m_aBuildables)
			return "";

		OVT_ResourceDefs defs = resources.GetDefs();
		if (!defs)
			return "";

		foreach (OVT_Buildable buildable : resistance.m_BuildablesConfig.m_aBuildables)
		{
			if (!buildable || !buildable.m_aResourceRequirements)
				continue;

			foreach (OVT_BuildableResourceRequirement requirement : buildable.m_aResourceRequirements)
			{
				if (!requirement || requirement.m_sResourceId == "" || requirement.m_iQuantity <= 0)
					continue;

				if (!defs.Knows(requirement.m_sResourceId))
					return requirement.m_sResourceId;
			}
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many buildables author at least one usable resource requirement.
	protected int CountRequirementBearingBuildables()
	{
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance || !resistance.m_BuildablesConfig || !resistance.m_BuildablesConfig.m_aBuildables)
			return 0;

		int count = 0;
		foreach (OVT_Buildable buildable : resistance.m_BuildablesConfig.m_aBuildables)
		{
			if (resistance.HasResourceRequirements(buildable))
				count += 1;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		if (m_Site)
		{
			delete m_Site;
			m_Site = null;
		}

		return true;
	}
}
