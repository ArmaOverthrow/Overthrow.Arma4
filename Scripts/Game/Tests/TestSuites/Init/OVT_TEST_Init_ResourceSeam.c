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
