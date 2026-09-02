//------------------------------------------------------------------------------------------------
//! TIER B - the vehicle-rearm seam.
//!
//! WHY THIS FILE EXISTS. Vehicle rearm is built out of pieces that each fail SILENTLY: a prefab-source
//! read that answers "visible" for a hidden item, a job step whose refusal was never wired, a plan
//! that resolves no ammunition prefab. None of them produces a compile error, a runtime error or a
//! log line at the point of use - the player just loses a box of 25 mm.
//!
//! CASES IN THIS FILE (A-B land in Phase 2, C-E in Phase 4, F in Phase 5):
//!   A. the hidden-item predicate reads the CONCRETE M242 box as hidden, an ordinary rifle magazine
//!      as visible, and an unreadable name as visible;
//!   B. a hidden ledger line survives a TO_INVENTORY job: nothing spawns, nothing is debited, and the
//!      whole quantity comes back as shortfall;
//!   C. the rearm plan lines every deficient weapon and its total is the sum of those lines;
//!   D. a quote is free when the vehicle's own ledger covers the plan, and full price when it is empty;
//!   E. a half-covered quote lands strictly between free and full;
//!   F. the quote RPC pair resolves on the local player's OVT_ShopTransactionComponent and answers
//!      the asker exactly once, for the vehicle they named.
//!
//! C-E ARRANGE BY DRAINING A REAL VEHICLE. A freshly spawned armed vehicle is FULL, so its plan is
//! empty and every coverage claim would be vacuously true; the fixture below sets each magazine to
//! zero first and rejects any catalogue candidate that still yields no plan.
//!
//! ⚠ CASE A MUST USE THE CONCRETE VARIANT. BaseContainer.Get reads one ancestry level, and the
//! Box_25x137_M242_* family authors m_bVisible on its _Base prefabs while the concrete variants are
//! empty deltas. A case written against the _Base would pass while the shipped path - which is only
//! ever handed the concrete prefab a ledger line names - failed.
//!
//! Cases run alphabetically by class name; every subject a case spawns is deleted before it reports.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Prefabs and world helpers shared by the cases below.
//------------------------------------------------------------------------------------------------
class OVT_TEST_VehicleRearmSeamSubject
{
	//! The LAV-25's HEIT belt, as its turret names it (LAV25_turret_base.et:714). Its own file
	//! authors nothing at all; m_bVisible 0 lives one level up on
	//! Box_25x137_M242_150rnd_HEIT_Base.et:33.
	static const ResourceName HIDDEN_PREFAB = "{8E0429589CD8A49A}Prefabs/Weapons/Magazines/Magazine_M242/Box_25x137_M242_150rnd_HEIT.et";

	//! An ordinary AK magazine - the same empty-delta SHAPE as the M242 box, so the two cases differ
	//! only in what their chains author. Nothing in its three-link chain authors m_bVisible.
	static const ResourceName VISIBLE_PREFAB = "{0A84AA5A3884176F}Prefabs/Weapons/Magazines/Magazine_545x39_AK_30rnd_Last_5Tracer.et";

	//! A well-formed ResourceName no addon carries - what a ledger line looks like after the mod that
	//! shipped the item is removed.
	static const ResourceName MISSING_PREFAB = "{6BA1C4E0FFFFFFFF}Prefabs/Overthrow/NoSuchPrefabExists.et";

	//! A holder with a ledger, an inventory manager and no owner lock: everything MayUseHolder and the
	//! TO_INVENTORY checkout gate ask for.
	static const ResourceName HOLDER_PREFAB = "{0AAFD134C3BEE963}Prefabs/Props/Military/AmmoBoxes/OVT_AmmoBox_Placed.et";

	//------------------------------------------------------------------------------------------------
	//! \param[out] failure Why the case cannot say anything; "" when this machine is the authority.
	//! \return True on the replication authority.
	static bool RequireAuthority(out string failure)
	{
		failure = "";

		if (Replication.IsServer())
			return true;

		failure = "This test machine is not the replication authority, so the storage job engine would refuse at its Replication.IsServer() guard and the case would pass by doing nothing.";
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Somewhere to put a spawned subject, clear of every other case's.
	//! \param[in] offset Per-case separation from the town anchor.
	//! \param[out] position Where to spawn.
	//! \return True when a position was resolved.
	static bool ResolveSpawnPosition(vector offset, out vector position)
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.IsEmpty())
			return false;

		position = towns.m_Towns[0].location + offset;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the predicate with the prefab's resource PROVABLY in memory.
	//!
	//! IsItemHiddenInInventory fails open on an unreadable prefab source, and a resource nothing has
	//! spawned reads back as a source with zero components (see the header of
	//! OVT_PrefabUtils.ReadAuthoredVisibility). Spawning first is what makes the answer the CHAIN's and
	//! not the resource loader's - and reading before deleting the sample keeps it that way.
	//! \param[in] prefab The prefab to read.
	//! \param[in] position Where to put the throwaway sample.
	//! \param[out] hidden The predicate's answer.
	//! \return False when the sample could not be spawned, so nothing was read.
	static bool ReadWithResourceLoaded(ResourceName prefab, vector position, out bool hidden)
	{
		hidden = false;

		IEntity sample = OVT_Global.SpawnEntityPrefab(prefab, position);
		if (!sample)
			return false;

		hidden = OVT_PrefabUtils.IsItemHiddenInInventory(prefab);

		delete sample;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every registered ARMED wheeled vehicle the economy knows, in catalogue order.
	//!
	//! Read from the economy rather than hardcoded, for the reason OVT_TEST_Init_StorageSeam's vehicle
	//! cases give: retuning what Overthrow sells must change which vehicle is exercised, not turn a case
	//! red for a reason that has nothing to do with rearm. "Illegal" is Overthrow's word for armed.
	//! \param[out] prefabs Receives the candidates. Allocated if null, cleared otherwise.
	//! \return How many candidates were found.
	static int CollectArmedVehiclePrefabs(out array<ResourceName> prefabs)
	{
		if (!prefabs)
			prefabs = new array<ResourceName>();

		prefabs.Clear();

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
			return 0;

		array<ResourceName> all = new array<ResourceName>();
		economy.FindVehicles("", all);

		foreach (ResourceName candidate : all)
		{
			if (candidate.IndexOf("/Wheeled/") == -1)
				continue;

			if (!economy.IsRegisteredResource(candidate))
				continue;

			if (economy.IsLegalVehicle(economy.GetInventoryId(candidate)))
				continue;

			prefabs.Insert(candidate);
		}

		return prefabs.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Empties every gun magazine on a vehicle, so the rearm plan has something to plan for.
	//! SERVER ONLY - SetAmmoCount may only be called on the master.
	//! \param[in] vehicle The subject.
	//! \return How many magazines were drained.
	static int DepleteWeapons(IEntity vehicle)
	{
		array<BaseMagazineComponent> magazines;
		array<BaseMuzzleComponent> muzzles;
		array<IEntity> rocketWeapons;
		OVT_VehicleRearmUtils.GetRearmableWeapons(vehicle, magazines, muzzles, rocketWeapons);

		int drained = 0;

		foreach (BaseMagazineComponent magazine : magazines)
		{
			if (magazine.GetMaxAmmoCount() <= 0)
				continue;

			magazine.SetAmmoCount(0);
			drained += 1;
		}

		return drained;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether every line of a plan names an ammunition prefab - the precondition for any claim about
	//! full coverage, since a line with no prefab can never be covered by any ledger.
	//! \param[in] plan Lines from OVT_VehicleRearmUtils.BuildPlan.
	//! \return True when no line is unresolvable.
	static bool PlanNamesEveryPrefab(notnull array<ref OVT_RearmUnit> plan)
	{
		foreach (OVT_RearmUnit unit : plan)
		{
			if (!unit || unit.m_sRes == "")
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A spot beside the caller, snapped to the surface.
	//!
	//! The quote handler is gated at OVT_ShopTransactionComponent.VEHICLE_MAX_DISTANCE (15 m) from the
	//! ASKING PLAYER'S character, exactly as the rearm is, so a case that wants an answer has to put
	//! its subject where the player is standing rather than in the empty quarter cases C-E use.
	//! \param[in] body The caller's character.
	//! \param[in] offset Horizontal separation from it.
	//! \param[out] position Where to spawn.
	//! \return True when a position was resolved.
	static bool ResolveSpawnBeside(IEntity body, vector offset, out vector position)
	{
		if (!body)
			return false;

		position = body.GetOrigin() + offset;

		BaseWorld world = GetGame().GetWorld();
		if (world)
			position[1] = world.GetSurfaceY(position[0], position[2]);

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Spawns catalogue candidates until one yields a rearm plan the calling case can work with.
//!
//! ONE INSTANCE PER CASE. It walks the economy's armed-vehicle list one prefab per frame, drains the
//! candidate's magazines, builds a plan, and discards anything that cannot serve the case's minimum -
//! an armed jeep with one machine gun is a fine subject for case C and useless to case E, which needs
//! two units to have a half of.
//------------------------------------------------------------------------------------------------
class OVT_TEST_VehicleRearmVehicleFixture : Managed
{
	//! Still looking; the case should poll again.
	static const int STEP_PENDING = 0;

	//! A subject is spawned, drained, planned and its storage capacity resolved.
	static const int STEP_READY = 1;

	//! No candidate can serve; GetWhy() says which way it failed.
	static const int STEP_EXHAUSTED = 2;

	//! Most catalogue entries one case will spawn looking for a subject.
	static const int MAX_CANDIDATES = 8;

	protected ref array<ResourceName> m_aCandidates;
	protected int m_iCandidate;
	protected int m_iDrained;
	protected IEntity m_Vehicle;
	protected ResourceName m_sPrefab;
	protected string m_sWhy;

	//------------------------------------------------------------------------------------------------
	//! Advances one step. Call every frame until it answers something other than STEP_PENDING.
	//! \param[in] position Where to put the subject.
	//! \param[in] minUnits Fewest plan units the case needs.
	//! \param[in] requireResolvable True when every line must name a prefab (a coverage case).
	//! \param[out] plan The subject's rearm plan; only meaningful on STEP_READY.
	//! \param[out] totalUnits The plan's total; only meaningful on STEP_READY.
	//! \return STEP_PENDING, STEP_READY or STEP_EXHAUSTED.
	int Advance(vector position, int minUnits, bool requireResolvable, out array<ref OVT_RearmUnit> plan, out int totalUnits)
	{
		totalUnits = 0;

		if (!m_aCandidates)
		{
			m_aCandidates = new array<ResourceName>();
			OVT_TEST_VehicleRearmSeamSubject.CollectArmedVehiclePrefabs(m_aCandidates);

			if (m_aCandidates.IsEmpty())
			{
				m_sWhy = "The economy knows no registered ARMED wheeled vehicle at all, so this case has no subject. Check that Configs/Pricing/vehiclePrices.conf still registers the LAV25, BTR70 and BRDM2 families (illegal is Overthrow's word for armed).";
				return STEP_EXHAUSTED;
			}
		}

		if (!m_Vehicle)
		{
			if (m_iCandidate >= m_aCandidates.Count() || m_iCandidate >= MAX_CANDIDATES)
			{
				m_sWhy = "None of the first " + m_iCandidate.ToString() + " armed vehicles in the economy catalogue produced a rearm plan of at least " + minUnits.ToString() + " unit(s) after every magazine was set to zero. Either GetRearmableWeapons stopped finding turret magazines, or BuildPlan stopped lining deficient ones.";
				return STEP_EXHAUSTED;
			}

			m_sPrefab = m_aCandidates[m_iCandidate];
			m_Vehicle = OVT_Global.SpawnEntityPrefab(m_sPrefab, position);

			if (!m_Vehicle)
				m_iCandidate += 1;

			// A frame either way - a just-spawned vehicle's turret slots are read on the next call.
			return STEP_PENDING;
		}

		m_iDrained = OVT_TEST_VehicleRearmSeamSubject.DepleteWeapons(m_Vehicle);
		OVT_VehicleRearmUtils.BuildPlan(m_Vehicle, plan, totalUnits);

		if (totalUnits < minUnits || (requireResolvable && !OVT_TEST_VehicleRearmSeamSubject.PlanNamesEveryPrefab(plan)))
		{
			Discard();
			return STEP_PENDING;
		}

		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(m_Vehicle);
		if (!storage)
		{
			m_sWhy = "The armed vehicle " + m_sPrefab + " carries no OVT_StorageComponent, so it has no ledger a rearm could ever draw from. Prefabs/Vehicles/Core/Wheeled_Base.et has lost its entry.";
			return STEP_EXHAUSTED;
		}

		// The ledger clamps every Add against the resolved capacity, so an unresolved holder would
		// silently refuse the credit and every coverage claim would read as "nothing was covered".
		if (!storage.IsCapacityResolved())
			return STEP_PENDING;

		return STEP_READY;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The spawned subject, or null.
	IEntity GetVehicle()
	{
		return m_Vehicle;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The subject's prefab.
	ResourceName GetPrefab()
	{
		return m_sPrefab;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many magazines the last drain emptied.
	int GetDrained()
	{
		return m_iDrained;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Why STEP_EXHAUSTED was answered.
	string GetWhy()
	{
		return m_sWhy;
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes the subject without advancing - the case is over.
	void CleanUp()
	{
		if (m_Vehicle)
		{
			delete m_Vehicle;
			m_Vehicle = null;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes an unusable subject and moves to the next candidate.
	protected void Discard()
	{
		CleanUp();
		m_iCandidate += 1;
	}
}

//------------------------------------------------------------------------------------------------
//! The hidden-item predicate answers correctly for the three shapes that matter.
//!
//! THREE CLAIMS, PLUS ONE ABOUT THE CACHE:
//!   1. the CONCRETE Box_25x137_M242_150rnd_HEIT reads HIDDEN - the walk climbs to the _Base that
//!      authors the flag;
//!   2. an ordinary rifle magazine reads VISIBLE - the walk runs its whole chain to the end and finds
//!      nothing, rather than answering "hidden" for want of a read;
//!   3. an unreadable ResourceName reads VISIBLE, twice - the predicate fails OPEN, because making an
//!      arbitrary item permanently un-takeable off one bad prefab read is the worse failure (D7);
//!   4. the cold read of the M242 box does not poison the cache: whatever it answered before anything
//!      loaded that resource, the read with the resource in memory still says HIDDEN.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_VehicleRearmSeam_AHiddenPrefabIsDetected : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// COLD, before any of these prefabs has been spawned in this world. The answer is not asserted
		// here - it is compared against the warm one below.
		bool hiddenCold = OVT_PrefabUtils.IsItemHiddenInInventory(OVT_TEST_VehicleRearmSeamSubject.HIDDEN_PREFAB);

		// Claim 3 - fail open, and still fail open on the second ask.
		if (OVT_PrefabUtils.IsItemHiddenInInventory(OVT_TEST_VehicleRearmSeamSubject.MISSING_PREFAB))
		{
			SetFailure("IsItemHiddenInInventory() called a ResourceName no addon carries HIDDEN. The predicate must fail OPEN: a prefab source that cannot be read says nothing about m_bVisible, and answering 'hidden' would make every item behind a bad read permanently un-takeable from a ledger.");
			return true;
		}

		if (OVT_PrefabUtils.IsItemHiddenInInventory(OVT_TEST_VehicleRearmSeamSubject.MISSING_PREFAB))
		{
			SetFailure("IsItemHiddenInInventory() answered VISIBLE for an unreadable prefab and HIDDEN for the same prefab one call later, so the cache is being written from a read that reached no conclusion.");
			return true;
		}

		vector position;
		if (!OVT_TEST_VehicleRearmSeamSubject.ResolveSpawnPosition("0 300 0", position))
		{
			SetFailure("No town is registered, so there is nowhere to put the throwaway samples this case reads through.");
			return true;
		}

		// Claim 1 (and 4) - the concrete variant, with its resource provably loaded.
		bool hiddenWarm;
		if (!OVT_TEST_VehicleRearmSeamSubject.ReadWithResourceLoaded(OVT_TEST_VehicleRearmSeamSubject.HIDDEN_PREFAB, position, hiddenWarm))
		{
			SetFailure("SpawnEntityPrefab() produced nothing from %1, so the hidden-item claim could not be read with the resource in memory.", OVT_TEST_VehicleRearmSeamSubject.HIDDEN_PREFAB);
			return true;
		}

		if (!hiddenWarm)
		{
			SetFailure("IsItemHiddenInInventory() called the CONCRETE %1 visible. That file authors nothing of its own - m_bVisible 0 is one level up on Box_25x137_M242_150rnd_HEIT_Base.et:33 - so either the ancestry walk is not climbing, or a cold read that reached no conclusion was cached (it answered %2 before the resource was loaded).",
				OVT_TEST_VehicleRearmSeamSubject.HIDDEN_PREFAB,
				hiddenCold.ToString());
			return true;
		}

		// Claim 2 - the same empty-delta shape, a chain that authors nothing.
		bool visibleWarm;
		if (!OVT_TEST_VehicleRearmSeamSubject.ReadWithResourceLoaded(OVT_TEST_VehicleRearmSeamSubject.VISIBLE_PREFAB, position, visibleWarm))
		{
			SetFailure("SpawnEntityPrefab() produced nothing from %1, so the visible-item claim could not be read with the resource in memory.", OVT_TEST_VehicleRearmSeamSubject.VISIBLE_PREFAB);
			return true;
		}

		if (visibleWarm)
		{
			SetFailure("IsItemHiddenInInventory() called %1 HIDDEN. Nothing in its chain authors m_bVisible, so the predicate is answering 'hidden' for an UNAUTHORED flag - which would strand ordinary ammunition in every ledger in the game.", OVT_TEST_VehicleRearmSeamSubject.VISIBLE_PREFAB);
			return true;
		}

		PrintFormat("Vehicle rearm seam: the concrete M242 box reads hidden (cold read said %1), a rifle magazine reads visible, an unreadable name fails open", hiddenCold.ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A hidden ledger line survives a TO_INVENTORY job intact.
//!
//! This is R3 end to end, through the shipped checkout seam rather than a copy of it: Begin, one cart
//! line, Commit, and then the job engine's own chunk. THREE CLAIMS:
//!   1. nothing moved - the spawn was never attempted;
//!   2. the whole requested quantity came back as SHORTFALL, so the player is told;
//!   3. the ledger still holds every one of them - the refusal happens BEFORE ledger.Take, which is
//!      what keeps the ammunition where a rearm can still eat it.
//!
//! Claim 3 is the one that matters. A guard placed after the debit would satisfy 1 and 2 and still
//! destroy the stock, which is the exact bug this feature exists to close.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_VehicleRearmSeam_BHiddenLineSurvivesTakeToInventory : SCR_AutotestCaseBase
{
	//! Frame polls allowed for the local player, the controller seam and the deferred capacity resolve.
	static const int MAX_POLLS = 900;

	//! Frame polls allowed for the job engine to run its one chunk (50 ms between chunks by default).
	static const int MAX_RESULT_POLLS = 600;

	//! More than one, so "shortfall == wanted" is a real equality: a guard that drops the line but
	//! reports one unit of shortfall would answer 1 here and pass at a quantity of 1.
	static const int CREDITED_QUANTITY = 4;

	protected int m_iStage;
	protected int m_iPolls;
	protected int m_iResultPolls;
	protected int m_iPlayerId;

	protected IEntity m_Holder;
	protected OVT_StorageRequestComponent m_Requests;

	protected bool m_bResultSeen;
	protected int m_iMoved;
	protected int m_iShortfall;

	protected bool m_bErrorSeen;
	protected string m_sLastErrorKey;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string failure;
		if (!OVT_TEST_VehicleRearmSeamSubject.RequireAuthority(failure))
		{
			SetFailure(failure);
			return true;
		}

		if (m_iStage == 0)
			return Arrange();

		if (m_iStage == 1)
			return StartCheckout();

		return Judge();
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the caller and the seam, loads the hidden prefab's resource, and puts a holder at the
	//! caller's feet - MayUseHolder refuses anything beyond its 30 m use radius.
	//! \return True when the case is finished, false to poll again.
	protected bool Arrange()
	{
		m_iPlayerId = SCR_PlayerController.GetLocalPlayerId();
		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(m_iPlayerId);
		m_Requests = OVT_ControllerComponent<OVT_StorageRequestComponent>.Get();

		if (!body || !m_Requests || m_Requests.IsBusy())
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("After %1 frames the local player (id %2) had no character, or OVT_StorageRequestComponent never resolved off its controller, or a storage job from another case was still running - so this case has no seam to send a checkout through.",
					m_iPolls.ToString(),
					m_iPlayerId.ToString());
				return true;
			}

			return false;
		}

		// The predicate fails open on a prefab source it cannot read, so the resource is put in memory
		// before the job asks about it. Without this the case could go green on an empty ledger read
		// rather than on the guard.
		vector overhead = body.GetOrigin();
		overhead[1] = overhead[1] + 200;

		bool hidden;
		if (!OVT_TEST_VehicleRearmSeamSubject.ReadWithResourceLoaded(OVT_TEST_VehicleRearmSeamSubject.HIDDEN_PREFAB, overhead, hidden))
		{
			SetFailure("SpawnEntityPrefab() produced nothing from %1, so the hidden prefab could not be loaded and the guard would have nothing to refuse.", OVT_TEST_VehicleRearmSeamSubject.HIDDEN_PREFAB);
			return true;
		}

		if (!hidden)
		{
			SetFailure("IsItemHiddenInInventory() calls %1 visible, so this case cannot say anything about the TO_INVENTORY guard. Case A owns that claim - fix it there first.", OVT_TEST_VehicleRearmSeamSubject.HIDDEN_PREFAB);
			return true;
		}

		vector beside = body.GetOrigin();
		beside[0] = beside[0] + 3;
		beside[2] = beside[2] + 3;

		m_Holder = OVT_Global.SpawnEntityPrefab(OVT_TEST_VehicleRearmSeamSubject.HOLDER_PREFAB, beside);
		if (!m_Holder)
		{
			SetFailure("SpawnEntityPrefab() produced no holder from %1", OVT_TEST_VehicleRearmSeamSubject.HOLDER_PREFAB);
			return true;
		}

		m_iStage = 1;
		m_iPolls = 0;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for the deferred capacity resolve, credits the ledger and streams the checkout.
	//! \return True when the case is finished, false to poll again.
	protected bool StartCheckout()
	{
		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(m_Holder);
		if (!storage)
		{
			SetFailure("The spawned holder carries no OVT_StorageComponent, so there is no ledger to credit.");
			return FinishAndCleanUp();
		}

		if (!storage.IsCapacityResolved())
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("The holder never resolved a capacity in %1 frames, so MayUseHolder would refuse the checkout for want of capacity rather than the guard being tested.", m_iPolls.ToString());
				return FinishAndCleanUp();
			}

			return false;
		}

		OVT_StorageLedger ledger = storage.GetLedger();
		if (!ledger)
		{
			SetFailure("The holder's storage built no ledger.");
			return FinishAndCleanUp();
		}

		ledger.Add(OVT_TEST_VehicleRearmSeamSubject.HIDDEN_PREFAB, CREDITED_QUANTITY, storage.GetCapacity());
		storage.PublishCount();

		if (ledger.Count(OVT_TEST_VehicleRearmSeamSubject.HIDDEN_PREFAB) != CREDITED_QUANTITY)
		{
			SetFailure("The ledger holds %1 of the hidden prefab after being credited %2, so 'the line survives' would be asserted against a line that was never there.",
				ledger.Count(OVT_TEST_VehicleRearmSeamSubject.HIDDEN_PREFAB).ToString(),
				CREDITED_QUANTITY.ToString());
			return FinishAndCleanUp();
		}

		RplId holderId = OVT_StorageUtils.GetHolderId(m_Holder);
		if (!holderId.IsValid())
		{
			SetFailure("The holder's RplId is invalid, so no checkout could ever name it.");
			return FinishAndCleanUp();
		}

		m_Requests.GetOnBatchResult().Insert(OnBatchResult);
		m_Requests.GetOnStorageError().Insert(OnStorageError);

		int seq = m_Requests.RequestBatchBegin(holderId, holderId, EOVT_StorageOp.TO_INVENTORY, 1);
		if (seq == OVT_StorageRequestComponent.SEQ_NONE)
		{
			SetFailure("RequestBatchBegin() returned SEQ_NONE, so this machine does not own the controller the seam sits on and no checkout was ever sent.");
			return FinishAndCleanUp();
		}

		m_Requests.RequestBatchLine(seq, 0, OVT_TEST_VehicleRearmSeamSubject.HIDDEN_PREFAB, CREDITED_QUANTITY);
		m_Requests.RequestBatchCommit(seq, 1);

		m_iStage = 2;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for the checkout to report, then judges the three claims.
	//! \return True when the case is finished, false to poll again.
	protected bool Judge()
	{
		if (!m_bResultSeen)
		{
			m_iResultPolls += 1;
			if (m_iResultPolls > MAX_RESULT_POLLS)
			{
				SetFailure("The checkout never reported in %1 frames. The last refusal seen was '%2' - a refusal means the checkout was rejected at the gate (distance, lock, capacity) and the guard was never reached, while no refusal at all means the job engine did not run.",
					m_iResultPolls.ToString(),
					m_sLastErrorKey);
				return FinishAndCleanUp();
			}

			return false;
		}

		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(m_Holder);
		OVT_StorageLedger ledger;
		if (storage)
			ledger = storage.GetLedger();

		if (!ledger)
		{
			SetFailure("The holder or its ledger went away while the checkout was running.");
			return FinishAndCleanUp();
		}

		int held = ledger.Count(OVT_TEST_VehicleRearmSeamSubject.HIDDEN_PREFAB);

		// Claim 3 first: it is the one that costs the player their ammunition.
		if (held != CREDITED_QUANTITY)
		{
			SetFailure("The ledger holds %1 of the hidden prefab after a TO_INVENTORY take, expected %2 - the line was DEBITED. StepToInventory's hidden-item guard must sit before ledger.Take, or the item is destroyed exactly as it was before this feature.",
				held.ToString(),
				CREDITED_QUANTITY.ToString());
			return FinishAndCleanUp();
		}

		if (m_iMoved != 0)
		{
			SetFailure("The checkout reported %1 items moved for a hidden prefab, expected 0. Something spawned - which is the one-way trip this guard exists to prevent.", m_iMoved.ToString());
			return FinishAndCleanUp();
		}

		if (m_iShortfall != CREDITED_QUANTITY)
		{
			SetFailure("The checkout reported a shortfall of %1, expected the whole requested %2. The guard must count the WHOLE line, or the player is told the take succeeded for items that never arrived.",
				m_iShortfall.ToString(),
				CREDITED_QUANTITY.ToString());
			return FinishAndCleanUp();
		}

		PrintFormat("Vehicle rearm seam: a hidden line survived a TO_INVENTORY take intact (%1 held, %2 shortfall)", held.ToString(), m_iShortfall.ToString());
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] moved Items that reached the inventory.
	//! \param[in] shortfall Items that did not.
	//! \param[in] earned Money paid out; always 0 for a take.
	protected void OnBatchResult(int moved, int shortfall, int earned)
	{
		m_bResultSeen = true;
		m_iMoved = moved;
		m_iShortfall = shortfall;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] messageKey The refusal key - kept only to diagnose a checkout that never ran.
	protected void OnStorageError(string messageKey)
	{
		m_bErrorSeen = true;
		m_sLastErrorKey = messageKey;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		if (m_Requests)
		{
			m_Requests.GetOnBatchResult().Remove(OnBatchResult);
			m_Requests.GetOnStorageError().Remove(OnStorageError);
		}

		if (m_Holder)
		{
			delete m_Holder;
			m_Holder = null;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The rearm plan lines every deficient weapon, and its total is the sum of those lines.
//!
//! FOUR CLAIMS:
//!   1. a line exists for every magazine the arrangement drained - a plan that deduped by prefab, or
//!      skipped a weapon it could not price, would under-count a two-gun turret;
//!   2. every line carries at least one unit;
//!   3. totalUnits is the sum of the lines' units, not the line count - the pro-rata price divides by
//!      it, so a total that disagrees with the plan misprices every partially-covered rearm;
//!   4. at least one line names an ammunition prefab, i.e. the muzzle's own default resolved (D2). A
//!      plan of nothing-but-empty prefabs is well-formed and can never be covered by any ledger.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 120)]
class OVT_TEST_Init_VehicleRearmSeam_CPlanCountsUnitsPerWeapon : SCR_AutotestCaseBase
{
	//! Frames allowed for the catalogue walk: one spawn plus one read per candidate, plus the
	//! deferred capacity resolve (the 900 OVT_TEST_Init_StorageSeam's vehicle cases allow for it).
	static const int MAX_POLLS = 900;

	//! Well clear of every other case's subject, and of the 25 m holder radius.
	static const vector SPAWN_OFFSET = "1200 0 900";

	protected int m_iPolls;
	protected ref OVT_TEST_VehicleRearmVehicleFixture m_Fixture = new OVT_TEST_VehicleRearmVehicleFixture();

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string failure;
		if (!OVT_TEST_VehicleRearmSeamSubject.RequireAuthority(failure))
		{
			SetFailure(failure);
			return true;
		}

		vector position;
		if (!OVT_TEST_VehicleRearmSeamSubject.ResolveSpawnPosition(SPAWN_OFFSET, position))
		{
			SetFailure("No town is registered, so there is nowhere to put a test vehicle.");
			return true;
		}

		array<ref OVT_RearmUnit> plan;
		int totalUnits;
		int step = m_Fixture.Advance(position, 1, false, plan, totalUnits);

		if (step == OVT_TEST_VehicleRearmVehicleFixture.STEP_EXHAUSTED)
		{
			SetFailure(m_Fixture.GetWhy());
			return FinishAndCleanUp();
		}

		if (step == OVT_TEST_VehicleRearmVehicleFixture.STEP_PENDING)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("After %1 frames no armed vehicle had been spawned, drained and planned. The catalogue walk is stuck: either SpawnEntityPrefab is failing for every candidate, or the vehicle's storage capacity never resolves.", m_iPolls.ToString());
				return FinishAndCleanUp();
			}

			return false;
		}

		int drained = m_Fixture.GetDrained();
		if (drained < 1)
		{
			SetFailure("The arrangement drained no magazine on %1, so 'a line per deficient weapon' has nothing to be true about.", m_Fixture.GetPrefab());
			return FinishAndCleanUp();
		}

		// Claim 1 - one line per drained magazine, plus whatever rocket barrels add.
		if (plan.Count() < drained)
		{
			SetFailure("BuildPlan produced %1 lines for %2 emptied magazines on %3. A rearm buys one item per deficient magazine, so a plan shorter than the deficiency under-charges and under-draws for every extra gun on the turret.",
				plan.Count().ToString(),
				drained.ToString(),
				m_Fixture.GetPrefab());
			return FinishAndCleanUp();
		}

		int summed = 0;
		int named = 0;

		foreach (OVT_RearmUnit unit : plan)
		{
			if (!unit)
			{
				SetFailure("BuildPlan produced a null line for %1.", m_Fixture.GetPrefab());
				return FinishAndCleanUp();
			}

			// Claim 2.
			if (unit.m_iUnits < 1)
			{
				SetFailure("BuildPlan produced a line of %1 units for prefab '%2' on %3. A line that needs nothing must not be in the plan at all - it dilutes the pro-rata denominator and makes an uncoverable weapon look free.",
					unit.m_iUnits.ToString(),
					unit.m_sRes,
					m_Fixture.GetPrefab());
				return FinishAndCleanUp();
			}

			summed += unit.m_iUnits;

			if (unit.m_sRes != "")
				named += 1;
		}

		// Claim 3.
		if (summed != totalUnits)
		{
			SetFailure("BuildPlan reported a total of %1 units while its lines sum to %2 on %3. OVT_VehicleRearmRules.ProratedCost divides by that total, so the two disagreeing misprices every partially covered rearm.",
				totalUnits.ToString(),
				summed.ToString(),
				m_Fixture.GetPrefab());
			return FinishAndCleanUp();
		}

		// Claim 4.
		if (named < 1)
		{
			SetFailure("Not one of %1's %2 plan lines names an ammunition prefab. BaseMuzzleComponent.GetDefaultMagazineOrProjectileName() is answering empty for every gun, so no ledger can ever cover a rearm and the feature is money-only again.",
				m_Fixture.GetPrefab(),
				plan.Count().ToString());
			return FinishAndCleanUp();
		}

		PrintFormat("Vehicle rearm seam: %1 planned %2 lines for %3 emptied magazines", m_Fixture.GetPrefab(), plan.Count().ToString() + " / " + totalUnits.ToString() + " units", drained.ToString());
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		m_Fixture.CleanUp();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A quote is free when the vehicle's own ledger covers the plan, and full price when it is empty.
//!
//! THE TWO ENDS OF R1-AS-AMENDED, read back to back off the same vehicle so nothing but the ledger
//! differs between them. FOUR CLAIMS:
//!   1. with every planned item credited, coverage equals the plan's total;
//!   2. and the price is exactly 0 - a crew that hauled the ammunition pays nothing and needs no
//!      supply point;
//!   3. with the ledger emptied, coverage is 0 - the quote is reading the ledger, not remembering it;
//!   4. and the price is exactly today's GetRearmCost(), so a wholly-bought rearm did not change price.
//!
//! Claim 2 is the one that matters: QuoteRearm is also the handler's authority, so a quote that never
//! reaches 0 means a fully-covered rearm still demands money and a helipad.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 120)]
class OVT_TEST_Init_VehicleRearmSeam_DQuoteIsFreeWhenCovered : SCR_AutotestCaseBase
{
	static const int MAX_POLLS = 900;

	//! 100 m from case C's and case E's subjects - four times the holder radius, so no case's ledger
	//! can cover another's rearm.
	static const vector SPAWN_OFFSET = "1200 0 1000";

	protected int m_iPolls;
	protected ref OVT_TEST_VehicleRearmVehicleFixture m_Fixture = new OVT_TEST_VehicleRearmVehicleFixture();

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string failure;
		if (!OVT_TEST_VehicleRearmSeamSubject.RequireAuthority(failure))
		{
			SetFailure(failure);
			return true;
		}

		int playerId = SCR_PlayerController.GetLocalPlayerId();
		if (playerId <= 0)
		{
			SetFailure("The local player id is %1, so CollectStores would refuse every nearby holder and this case would assert coverage the permission clauses never approved.", playerId.ToString());
			return true;
		}

		vector position;
		if (!OVT_TEST_VehicleRearmSeamSubject.ResolveSpawnPosition(SPAWN_OFFSET, position))
		{
			SetFailure("No town is registered, so there is nowhere to put a test vehicle.");
			return true;
		}

		array<ref OVT_RearmUnit> plan;
		int totalUnits;
		int step = m_Fixture.Advance(position, 1, true, plan, totalUnits);

		if (step == OVT_TEST_VehicleRearmVehicleFixture.STEP_EXHAUSTED)
		{
			SetFailure(m_Fixture.GetWhy());
			return FinishAndCleanUp();
		}

		if (step == OVT_TEST_VehicleRearmVehicleFixture.STEP_PENDING)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("After %1 frames no armed vehicle had a fully resolvable rearm plan and a resolved storage capacity, so no coverage claim could be arranged.", m_iPolls.ToString());
				return FinishAndCleanUp();
			}

			return false;
		}

		IEntity vehicle = m_Fixture.GetVehicle();
		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(vehicle);
		OVT_StorageLedger ledger = storage.GetLedger();
		if (!ledger)
		{
			SetFailure("%1's storage built no ledger, so there is nothing to credit.", m_Fixture.GetPrefab());
			return FinishAndCleanUp();
		}

		ledger.Clear();

		foreach (OVT_RearmUnit unit : plan)
		{
			ledger.Add(unit.m_sRes, unit.m_iUnits, storage.GetCapacity());
		}

		storage.PublishCount();

		int quotedTotal, covered, cost;
		OVT_VehicleRearmUtils.QuoteRearm(vehicle, playerId, quotedTotal, covered, cost);

		// Claim 1.
		if (covered != totalUnits)
		{
			SetFailure("With every planned item in %1's OWN ledger the quote covered %2 of %3 units. The vehicle's own storage is prepended to the collected stores, so a shortfall here means either CollectRearmStores lost it or the plan's prefab is not the one the ledger was credited with.",
				m_Fixture.GetPrefab(),
				covered.ToString(),
				totalUnits.ToString());
			return FinishAndCleanUp();
		}

		// Claim 2.
		if (cost != 0)
		{
			SetFailure("A fully covered rearm of %1 was quoted at $%2, expected $0. R1 as amended makes a covered rearm free and performable anywhere; a non-zero quote here re-imposes both the price and the supply-point gate on a crew that hauled its own ammunition.",
				m_Fixture.GetPrefab(),
				cost.ToString());
			return FinishAndCleanUp();
		}

		ledger.Clear();
		storage.PublishCount();

		OVT_VehicleRearmUtils.QuoteRearm(vehicle, playerId, quotedTotal, covered, cost);

		// Claim 3.
		if (covered != 0)
		{
			SetFailure("With %1's ledger emptied the quote still covered %2 units. Either the quote is caching coverage rather than reading the ledger, or another holder within the 25 m radius is stocking this case's ammunition.",
				m_Fixture.GetPrefab(),
				covered.ToString());
			return FinishAndCleanUp();
		}

		// Claim 4.
		int full = OVT_VehicleRearmUtils.GetRearmCost();
		if (cost != full)
		{
			SetFailure("An uncovered rearm of %1 was quoted at $%2, expected today's full price $%3. A wholly bought rearm must cost exactly what it cost before this feature.",
				m_Fixture.GetPrefab(),
				cost.ToString(),
				full.ToString());
			return FinishAndCleanUp();
		}

		PrintFormat("Vehicle rearm seam: %1 quoted $0 covered and $%2 empty over %3 units", m_Fixture.GetPrefab(), full.ToString(), totalUnits.ToString());
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		m_Fixture.CleanUp();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A half-covered quote lands strictly between free and full.
//!
//! WHY "STRICTLY" IS THE WHOLE CASE. Both degenerate implementations pass case D: one that charges
//! full price unless coverage is total, and one that charges nothing as soon as anything is covered.
//! Only a partial arrangement can tell them apart, which is why this case insists on a subject with at
//! least two units and asserts both bounds.
//!
//! THREE CLAIMS: coverage is exactly the credited half; the price is above 0; the price is below full.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 120)]
class OVT_TEST_Init_VehicleRearmSeam_EQuoteIsProRata : SCR_AutotestCaseBase
{
	static const int MAX_POLLS = 900;

	//! 100 m from cases C and D.
	static const vector SPAWN_OFFSET = "1200 0 1100";

	//! Two units is the smallest plan that has a half at all.
	static const int MIN_UNITS = 2;

	protected int m_iPolls;
	protected ref OVT_TEST_VehicleRearmVehicleFixture m_Fixture = new OVT_TEST_VehicleRearmVehicleFixture();

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string failure;
		if (!OVT_TEST_VehicleRearmSeamSubject.RequireAuthority(failure))
		{
			SetFailure(failure);
			return true;
		}

		int playerId = SCR_PlayerController.GetLocalPlayerId();
		if (playerId <= 0)
		{
			SetFailure("The local player id is %1, so CollectStores would refuse every nearby holder and this case would assert coverage the permission clauses never approved.", playerId.ToString());
			return true;
		}

		int full = OVT_VehicleRearmUtils.GetRearmCost();
		if (full < 2)
		{
			SetFailure("A full rearm costs $%1 at the current difficulty, so there is no value strictly between $0 and it and the pro-rata claim cannot be stated. OVT_DifficultySettings.vehiclePriceMultiplier has been turned down past the point where REARM_BASE_COST means anything.", full.ToString());
			return true;
		}

		vector position;
		if (!OVT_TEST_VehicleRearmSeamSubject.ResolveSpawnPosition(SPAWN_OFFSET, position))
		{
			SetFailure("No town is registered, so there is nowhere to put a test vehicle.");
			return true;
		}

		array<ref OVT_RearmUnit> plan;
		int totalUnits;
		int step = m_Fixture.Advance(position, MIN_UNITS, true, plan, totalUnits);

		if (step == OVT_TEST_VehicleRearmVehicleFixture.STEP_EXHAUSTED)
		{
			SetFailure(m_Fixture.GetWhy());
			return FinishAndCleanUp();
		}

		if (step == OVT_TEST_VehicleRearmVehicleFixture.STEP_PENDING)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("After %1 frames no armed vehicle had a fully resolvable rearm plan of at least %2 units, so a half-covered arrangement could not be built.", m_iPolls.ToString(), MIN_UNITS.ToString());
				return FinishAndCleanUp();
			}

			return false;
		}

		IEntity vehicle = m_Fixture.GetVehicle();
		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(vehicle);
		OVT_StorageLedger ledger = storage.GetLedger();
		if (!ledger)
		{
			SetFailure("%1's storage built no ledger, so there is nothing to credit.", m_Fixture.GetPrefab());
			return FinishAndCleanUp();
		}

		ledger.Clear();

		int half = totalUnits / 2;
		int remaining = half;

		foreach (OVT_RearmUnit unit : plan)
		{
			if (remaining <= 0)
				break;

			int give = unit.m_iUnits;
			if (give > remaining)
				give = remaining;

			ledger.Add(unit.m_sRes, give, storage.GetCapacity());
			remaining -= give;
		}

		storage.PublishCount();

		int quotedTotal, covered, cost;
		OVT_VehicleRearmUtils.QuoteRearm(vehicle, playerId, quotedTotal, covered, cost);

		if (covered != half)
		{
			SetFailure("%1 was credited %2 of its planned units and the quote covered %3. Coverage that disagrees with the ledger makes the price a guess - and coverage ABOVE the credit means two plan lines naming the same prefab were each allowed to claim the whole stock.",
				m_Fixture.GetPrefab(),
				half.ToString() + " of " + totalUnits.ToString(),
				covered.ToString());
			return FinishAndCleanUp();
		}

		if (cost <= 0)
		{
			SetFailure("%1 was quoted $%2 with only %3 units covered. Any uncovered unit must cost something, or a player buys a full restock for the price of one magazine.",
				m_Fixture.GetPrefab(),
				cost.ToString(),
				covered.ToString() + " of " + totalUnits.ToString());
			return FinishAndCleanUp();
		}

		if (cost >= full)
		{
			SetFailure("%1 was quoted the full $%2 with %3 units already covered. The price is not prorating: the ammunition the crew brought is being charged for as well.",
				m_Fixture.GetPrefab(),
				full.ToString(),
				covered.ToString() + " of " + totalUnits.ToString());
			return FinishAndCleanUp();
		}

		PrintFormat("Vehicle rearm seam: %1 covered %2 units and was quoted %3", m_Fixture.GetPrefab(), covered.ToString() + " of " + totalUnits.ToString(), "$" + cost.ToString() + " against a full $" + full.ToString());
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		m_Fixture.CleanUp();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The quote RPC pair resolves on the local player's controller and answers the asker, once.
//!
//! WHY THIS IS A SEAM CASE AND NOT A UNIT ONE. Every piece of the quote wire fails silently: an
//! RPC pair whose component is not on OVT_OverthrowController.et is never reachable at all; a
//! ResolveOwningPlayerId() that answers -1 drops the ask with no log line; and an Owner-targeted
//! reply a listen host sends to its OWN controller is never delivered (the BUG-090 family), which is
//! the single most likely deployment in this project. None of the three produces an error - the
//! action just never gets a price, and silently falls back to charging full.
//!
//! FIVE CLAIMS:
//!   1. OVT_ShopTransactionComponent resolves off THIS player's controller entity, and the players
//!      manager has that same controller registered for this player id - the two halves of the
//!      identity rule the server applies to the ask;
//!   2. one RequestRearmQuote produces at least one answer - the listen-host/SP short circuit is
//!      taken and the reply is not dropped on the wire;
//!   3. exactly one answer - a handler that both called directly AND sent the Rpc would quote twice;
//!   4. the answer names the vehicle that was asked about, so an action instance can tell its own
//!      quote from the one for the APC parked alongside;
//!   5. the payload carries the server's real figures: a total of at least the planned units, a
//!      coverage inside [0, total] and a non-negative price. A wire that dropped its arguments
//!      answers all zeros, which would otherwise read as a perfectly good "free re-arm" quote.
//!
//! ARRANGES BESIDE THE CALLER, unlike cases C-E: the ask is distance-gated from the asking player's
//! character (see ResolveSpawnBeside).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 120)]
class OVT_TEST_Init_VehicleRearmSeam_FQuoteRpcPairAnswersTheAsker : SCR_AutotestCaseBase
{
	//! Frames allowed for the caller and the seam to resolve, and again for the catalogue walk.
	static const int MAX_POLLS = 900;

	//! Frames waited after the first quote, looking for a second one.
	static const int SETTLE_POLLS = 10;

	//! Comfortably inside the seam's 15 m gate and clear of the caller's own body.
	static const vector SPAWN_OFFSET = "8 0 0";

	protected int m_iStage;
	protected int m_iPolls;
	protected int m_iSettlePolls;
	protected int m_iPlayerId;
	protected vector m_vPosition;
	protected int m_iPlannedUnits;

	protected OVT_ShopTransactionComponent m_Transactions;
	protected ref OVT_TEST_VehicleRearmVehicleFixture m_Fixture = new OVT_TEST_VehicleRearmVehicleFixture();

	//! What the invoker delivered.
	protected int m_iQuotes;
	protected RplId m_QuotedVehicleId;
	protected int m_iQuotedTotal;
	protected int m_iQuotedCovered;
	protected int m_iQuotedCost;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string failure;
		if (!OVT_TEST_VehicleRearmSeamSubject.RequireAuthority(failure))
		{
			SetFailure(failure);
			return true;
		}

		if (m_iStage == 0)
			return Arrange();

		if (m_iStage == 1)
			return FindSubject();

		return Settle();
	}

	//------------------------------------------------------------------------------------------------
	//! Claim 1, plus somewhere inside the gate to put the subject.
	//! \return True when the case is finished, false to poll again.
	protected bool Arrange()
	{
		m_iPlayerId = SCR_PlayerController.GetLocalPlayerId();
		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(m_iPlayerId);
		m_Transactions = OVT_ControllerComponent<OVT_ShopTransactionComponent>.Get();

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		OVT_OverthrowController registered;
		if (players && m_iPlayerId > 0)
			registered = players.GetController(m_iPlayerId);

		if (!body || !m_Transactions || !registered)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("After %1 frames the local player (id %2) had no character, or OVT_ControllerComponent<OVT_ShopTransactionComponent>.Get() never resolved, or OVT_PlayerManagerComponent had no controller registered for that id. The first two mean Prefabs/GameMode/OVT_OverthrowController.et is not carrying the transaction component; the third means the server's ResolveOwningPlayerId() would answer -1 and drop every quote ask in silence.",
					m_iPolls.ToString(),
					m_iPlayerId.ToString());
				return true;
			}

			return false;
		}

		// Claim 1 - the seam the client sends through and the controller the server resolves the
		// caller from must be the same entity, or the ask is attributed to somebody else.
		OVT_ShopTransactionComponent onRegistered = OVT_ShopTransactionComponent.Cast(registered.FindComponent(OVT_ShopTransactionComponent));
		if (onRegistered != m_Transactions)
		{
			SetFailure("The OVT_ShopTransactionComponent the local accessor returns is not the one on the controller OVT_PlayerManagerComponent has registered for player %1. Every quote ask would be sent on another player's seam, and the server would resolve the caller as that player.", m_iPlayerId.ToString());
			return true;
		}

		if (!OVT_TEST_VehicleRearmSeamSubject.ResolveSpawnBeside(body, SPAWN_OFFSET, m_vPosition))
		{
			SetFailure("The local player's character has no position, so there is nowhere inside the quote's distance gate to put a subject.");
			return true;
		}

		m_iStage = 1;
		m_iPolls = 0;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns and drains an armed vehicle beside the caller, then asks once.
	//! \return True when the case is finished, false to poll again.
	protected bool FindSubject()
	{
		array<ref OVT_RearmUnit> plan;
		int totalUnits;
		int step = m_Fixture.Advance(m_vPosition, 1, false, plan, totalUnits);

		if (step == OVT_TEST_VehicleRearmVehicleFixture.STEP_EXHAUSTED)
		{
			SetFailure(m_Fixture.GetWhy());
			return FinishAndCleanUp();
		}

		if (step == OVT_TEST_VehicleRearmVehicleFixture.STEP_PENDING)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("After %1 frames no armed vehicle had been spawned and drained beside the caller, so there was nothing to ask a quote about.", m_iPolls.ToString());
				return FinishAndCleanUp();
			}

			return false;
		}

		IEntity vehicle = m_Fixture.GetVehicle();
		m_iPlannedUnits = totalUnits;

		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (!rpl)
		{
			SetFailure("%1 carries no RplComponent, so it cannot be named across the network at all and no quote could ever be asked for it.", m_Fixture.GetPrefab());
			return FinishAndCleanUp();
		}

		// Kept for the whole settle window: a second delivery is exactly what claim 3 is looking for.
		m_Transactions.m_OnRearmQuote.Insert(OnRearmQuote);
		m_Transactions.RequestRearmQuote(vehicle);

		m_iStage = 2;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits out the settle window, then judges claims 2-5.
	//! \return True when the case is finished, false to poll again.
	protected bool Settle()
	{
		m_iSettlePolls += 1;
		if (m_iSettlePolls <= SETTLE_POLLS)
			return false;

		if (m_Transactions && m_Transactions.m_OnRearmQuote)
			m_Transactions.m_OnRearmQuote.Remove(OnRearmQuote);

		IEntity vehicle = m_Fixture.GetVehicle();
		if (!vehicle)
		{
			SetFailure("The subject vehicle was deleted before the quote could be judged, so nothing can be said about the answer.");
			return FinishAndCleanUp();
		}

		RplId vehicleId = RplId.Invalid();
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (rpl)
			vehicleId = rpl.Id();

		// Claim 2.
		if (m_iQuotes < 1)
		{
			SetFailure("RequestRearmQuote() on %1 produced no answer in %2 frames. On this machine the asker IS the authority and the local player, so both halves short-circuit to a direct call: either RpcAsk_RearmQuote refused (identity, the 15 m distance gate, or an unresolvable RplId) or SendRearmQuote took the wire branch, where an Owner-targeted RPC a host sends to itself is never delivered.",
				m_Fixture.GetPrefab(),
				SETTLE_POLLS.ToString());
			return FinishAndCleanUp();
		}

		// Claim 3.
		if (m_iQuotes != 1)
		{
			SetFailure("One RequestRearmQuote() on %1 produced %2 answers. The authority must take exactly one of the two branches - a handler that calls directly AND sends the Rpc quotes twice, and every listener downstream sees a duplicate.",
				m_Fixture.GetPrefab(),
				m_iQuotes.ToString());
			return FinishAndCleanUp();
		}

		// Claim 4.
		if (m_QuotedVehicleId != vehicleId)
		{
			SetFailure("The quote for %1 came back naming a different RplId than the vehicle asked about. The action instance uses that id to tell its own quote from the one for the vehicle parked alongside, so a wrong or dropped id makes two neighbouring vehicles share one price.", m_Fixture.GetPrefab());
			return FinishAndCleanUp();
		}

		// Claim 5 - a wire that dropped its arguments answers all zeros, which reads as "free".
		if (m_iQuotedTotal < m_iPlannedUnits)
		{
			SetFailure("The quote for %1 reported %2 units where the plan had %3. A short total means the payload did not survive the call - and a total of 0 with a cost of 0 is indistinguishable from a fully covered, free, go-anywhere re-arm.",
				m_Fixture.GetPrefab(),
				m_iQuotedTotal.ToString(),
				m_iPlannedUnits.ToString());
			return FinishAndCleanUp();
		}

		if (m_iQuotedCovered < 0 || m_iQuotedCovered > m_iQuotedTotal)
		{
			SetFailure("The quote for %1 covered %2 of %3 units. Coverage outside [0, total] cannot be prorated into a price at all.",
				m_Fixture.GetPrefab(),
				m_iQuotedCovered.ToString(),
				m_iQuotedTotal.ToString());
			return FinishAndCleanUp();
		}

		if (m_iQuotedCost < 0)
		{
			SetFailure("The quote for %1 priced the re-arm at $%2. A negative price would pay the player to re-arm.",
				m_Fixture.GetPrefab(),
				m_iQuotedCost.ToString());
			return FinishAndCleanUp();
		}

		PrintFormat("Vehicle rearm seam: one quote for %1 answered the asker with %2", m_Fixture.GetPrefab(), m_iQuotedCovered.ToString() + " of " + m_iQuotedTotal.ToString() + " units covered at $" + m_iQuotedCost.ToString());
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! The invoker's payload. Matches OVT_ShopTransactionComponent.m_OnRearmQuote's argument list -
	//! Invoke() is untyped, so a mismatch here fails at run time and never at compile time.
	//! \param[in] vehicleId The vehicle quoted.
	//! \param[in] totalUnits Units the re-arm needs.
	//! \param[in] coveredUnits Units the reachable ledgers hold.
	//! \param[in] cost Money owed for the remainder.
	protected void OnRearmQuote(RplId vehicleId, int totalUnits, int coveredUnits, int cost)
	{
		m_iQuotes += 1;
		m_QuotedVehicleId = vehicleId;
		m_iQuotedTotal = totalUnits;
		m_iQuotedCovered = coveredUnits;
		m_iQuotedCost = cost;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		if (m_Transactions && m_Transactions.m_OnRearmQuote)
			m_Transactions.m_OnRearmQuote.Remove(OnRearmQuote);

		m_Fixture.CleanUp();
		return true;
	}
}
