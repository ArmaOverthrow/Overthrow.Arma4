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
//!   E. an illegal/armed wheeled vehicle resolves the small armed cap (100) and the radius query
//!      DOES offer it - inverted 2026-09-01 by logistics/vehicle-rearm R4/D8.
//!   K. an armed HELICOPTER carries a storage component at all and resolves the same 100 -
//!      Helicopter_Base.et is the only place that block exists.
//!   O. the civilian Mi-8 delta resolves UNLIMITED (-1) - the new same-GUID delta of vanilla
//!      Mi8MT_unarmed_civ_base.et {366EA0B41474A7F8}, re-declaring the storage GUID minted in K
//!      (logistics/vehicle-rearm R5). Letter O, not the plan's G - F and G are both taken.
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

	//! Prefab path fragment every helicopter variant shares, in both the US and USSR catalogues.
	static const string HELICOPTER_PREFAB_FRAGMENT = "/Helicopters/";

	//! Overthrow's own same-GUID delta of vanilla Mi8MT_unarmed_civ_base.et - the one civilian
	//! helicopter authored UNLIMITED storage (logistics/vehicle-rearm R5).
	static const ResourceName CIVILIAN_MI8_PREFAB = "{366EA0B41474A7F8}Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_unarmed_civ_base.et";

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
	//! An armed (illegal) helicopter from the economy's own catalogue.
	//!
	//! Parking type as well as the path fragment, so a mis-pathed ground vehicle cannot stand in for the
	//! prefab family this case exists to check.
	//! \param[out] prefab The prefab to spawn; untouched when nothing matched.
	//! \return True when a prefab was resolved.
	static bool FindArmedHelicopter(out ResourceName prefab)
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
			return false;

		array<ResourceName> all = new array<ResourceName>();
		economy.FindVehicles("", all);

		foreach (ResourceName candidate : all)
		{
			if (candidate.IndexOf(HELICOPTER_PREFAB_FRAGMENT) == -1)
				continue;

			if (!economy.IsRegisteredResource(candidate))
				continue;

			int id = economy.GetInventoryId(candidate);
			if (economy.IsLegalVehicle(id))
				continue;

			if (economy.GetParkingType(id) != OVT_ParkingType.PARKING_HELI)
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


//------------------------------------------------------------------------------------------------
//! A ledger line's display name must survive a variant delta.
//!
//! Vanilla's `*_Dirty` civilian clothing authors only a material override, so its own source declares
//! no InventoryItemComponent and therefore no ItemDisplayName. Overthrow's civilian wardrobe is full of
//! them, so looting a civilian used to fill the transfer screen with raw "{GUID}Prefabs/..." rows.
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_StorageSeam_MVariantDeltaKeepsItsDisplayName : SCR_AutotestCaseBase
{
	static const ResourceName DIRTY_JACKET = "{23A15812C40D34C2}Prefabs/Characters/Uniforms/Jacket_Denim_01/Jacket_Denim_01_strippedShirt_dirty.et";
	static const ResourceName CLEAN_JACKET = "{43D84EA05C66258C}Prefabs/Characters/Uniforms/Jacket_Denim_01/Jacket_Denim_01_base.et";

	protected IEntity m_Jacket;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		vector position;
		if (!OVT_TEST_StorageSeamSubject.ResolveSpawnPosition("780 0 660", position))
		{
			SetFailure("No town is registered, so there is nowhere sensible to put a test jacket");
			return true;
		}

		// Spawned first: an unloaded resource answers with a zero-component source, which would make
		// this case pass or fail on residency rather than on the inheritance it exists to prove.
		m_Jacket = OVT_Global.SpawnEntityPrefab(DIRTY_JACKET, position);
		if (!m_Jacket)
		{
			SetFailure("SpawnEntityPrefab() produced no entity from %1", DIRTY_JACKET);
			return true;
		}

		UIInfo baseInfo = OVT_PrefabUtils.GetItemUIInfo(CLEAN_JACKET);
		if (!baseInfo || baseInfo.GetName() == "")
		{
			SetFailure("The BASE denim jacket reports no display name, so this case cannot tell an inheritance fault from a missing one");
			return FinishAndCleanUp();
		}

		UIInfo dirtyInfo = OVT_PrefabUtils.GetItemUIInfo(DIRTY_JACKET);
		if (!dirtyInfo || dirtyInfo.GetName() == "")
		{
			SetFailure("The dirty denim jacket variant reports no display name. Its delta authors only a material, so the read must walk to the base - otherwise every looted civilian garment is a raw {GUID}path row in the transfer screen.");
			return FinishAndCleanUp();
		}

		if (dirtyInfo.GetName() != baseInfo.GetName())
		{
			SetFailure("The dirty variant resolved '%1' but its base declares '%2'", dirtyInfo.GetName(), baseInfo.GetName());
			return FinishAndCleanUp();
		}

		ResourceName cleaned = OVT_PrefabUtils.ResolveCleanVariant(DIRTY_JACKET);
		if (cleaned == DIRTY_JACKET)
		{
			SetFailure("ResolveCleanVariant() left the dirty denim jacket alone. Every looted civilian would then bank a second stack of the same garment.");
			return FinishAndCleanUp();
		}

		if (OVT_PrefabUtils.ResolveCleanVariant(cleaned) != cleaned)
		{
			SetFailure("ResolveCleanVariant() is not idempotent: it walked past the clean prefab %1", cleaned);
			return FinishAndCleanUp();
		}

		if (OVT_PrefabUtils.ResolveCleanVariant(CLEAN_JACKET) != CLEAN_JACKET)
		{
			SetFailure("ResolveCleanVariant() rewrote a prefab that is not a dirty variant");
			return FinishAndCleanUp();
		}

		Print("Storage seam: a variant delta inherits its base's display name and banks clean");
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		if (m_Jacket)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_Jacket);
			m_Jacket = null;
		}

		return true;
	}
}


//------------------------------------------------------------------------------------------------
//! An ARMED mine is not loot.
//!
//! The loot query takes anything with an InventoryItemComponent and no parent slot, and a loot run
//! DELETES what it prices - so a truck parked on a minefield used to collect and disarm it. An unarmed
//! mine lying loose is still ordinary loot, and this case pins both halves.
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_StorageSeam_NArmedMinesAreNotLoot : SCR_AutotestCaseBase
{
	static const ResourceName MINE = "{D6EF54367CECE1D9}Prefabs/Weapons/Explosives/Mine_TM62M/Mine_TM62M.et";

	protected IEntity m_Mine;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		vector position;
		if (!OVT_TEST_StorageSeamSubject.ResolveSpawnPosition("780 0 660", position))
		{
			SetFailure("No town is registered, so there is nowhere sensible to put a test mine");
			return true;
		}

		m_Mine = OVT_Global.SpawnEntityPrefab(MINE, position);
		if (!m_Mine)
		{
			SetFailure("SpawnEntityPrefab() produced no entity from %1", MINE);
			return true;
		}

		SCR_BaseTriggerComponent trigger = SCR_BaseTriggerComponent.Cast(m_Mine.FindComponent(SCR_BaseTriggerComponent));
		if (!trigger)
		{
			SetFailure("Mine_TM62M carries no SCR_BaseTriggerComponent, so the armed test has nothing to read and this case cannot prove anything");
			return FinishAndCleanUp();
		}

		if (trigger.IsActivated())
		{
			SetFailure("A freshly spawned mine reports itself already armed, so the unarmed half of this case is untestable");
			return FinishAndCleanUp();
		}

		if (!LootableCountAt(position))
		{
			SetFailure("An UNARMED mine lying on the ground was not offered as loot. The guard is too wide - a dropped mine is ordinary litter.");
			return FinishAndCleanUp();
		}

		trigger.ActivateTrigger();

		if (!trigger.IsActivated())
		{
			SetFailure("ActivateTrigger() did not arm the mine, so the armed half of this case is untestable");
			return FinishAndCleanUp();
		}

		if (LootableCountAt(position))
		{
			SetFailure("An ARMED mine is still offered as loot. A loot run deletes what it prices, so a truck parked beside a minefield collects and silently disarms it.");
			return FinishAndCleanUp();
		}

		Print("Storage seam: an armed mine is left where it was laid");
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] position Centre of the query.
	//! \return True when the test mine is among the lootables there.
	protected bool LootableCountAt(vector position)
	{
		array<IEntity> found = new array<IEntity>();
		OVT_StorageLootQuery query = new OVT_StorageLootQuery();
		query.Run(position, 10, found);

		return found.Find(m_Mine) != -1;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		if (m_Mine)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_Mine);
			m_Mine = null;
		}

		return true;
	}
}
