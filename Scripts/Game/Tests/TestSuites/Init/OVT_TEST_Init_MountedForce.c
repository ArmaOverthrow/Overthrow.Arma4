//------------------------------------------------------------------------------------------------
//! TIER B - THE MOUNTED FORCE where it meets live components: the escalation ladder resolved against
//! two real faction registries, the arrival that does NOT open the doors, the fallback that still
//! walks, the runtime source seam, and the clone that carries all twenty-eight attributes.
//!
//! WHAT THIS TIER CAN SEE THAT A PURE ONE CANNOT.
//!   1. THAT THE LADDER RESOLVES AGAINST SHIPPED CONFIGS. OVT_VehicleLadderRules is pure and is pinned
//!      in OVT_TEST_Logic_VehicleLadder.c. What is NOT pure is that the module hands it the registry of
//!      the right faction, filters on the authored role string, and uses its own vehicle price as the
//!      BUDGET - the decision that makes escalation cost money (D4).
//!   2. THAT ARRIVAL LEAVES THE FORCE ON THE RIDING RING. This is the whole of D8, and its failure mode
//!      is silent: a force put back on the ordinary proximity ring is no longer exempt from
//!      OVT_DeploymentManagerComponent.SuppressForcesAroundBattle, which pins it dormant inside the
//!      battle it was sent to and delivers an empty vehicle. Nothing else in the tree would notice.
//!   3. THAT A LOST VEHICLE STILL PRODUCES A MARCH. The inherited fallback is the spine of the parent
//!      module; a new terminal state that forgot to reach it would strand a force in a wreck.
//!   4. THAT THE CLONE CARRIES EVERY ATTRIBUTE. CloneModule copies by hand and is NOT chained, so this
//!      module repeats thirteen inherited lines, eleven more inherited lines and four of its own.
//!
//! ================== THE FIXTURE HAZARD THIS FILE STAYS AWAY FROM ==========================
//!
//! OVT_DeploymentComponent.InitializeDeployment() arms a repeating 8-12 s UpdateDeployment whose first
//! tick converges every spawning module - which for THIS module would resolve an origin, claim a convoy
//! slot, put a real armed vehicle on a real road and register real groups at a 100 000 m ring, with the
//! autotest camera permanently inside it. NO CASE HERE BUILDS A DEPLOYMENT. Every subject is a bare
//! `new` module with no parent, driven through a test-local subclass that exposes the protected seams.
//! (The rule, if a later phase ever does need one: SetSpawnedUnitsEliminated(true) on the DEPLOYMENT and
//! on EVERY spawning module, in the frame it is created, before anything can tick.)
//!
//! ⚠ TWO CASES REGISTER A REAL VIRTUAL GROUP, because "is this force still on the riding ring" is a
//! question about a core record and cannot be asked of nothing. They are registered and unregistered
//! inside ONE synchronous frame - the engine's own 1 Hz lifecycle tick cannot run in between - and the
//! unregister happens BEFORE the first assertion, so a red case leaves no record behind.
//!
//! ⚠ [Attribute] DEFVALUES DO NOT APPLY TO `new`. A hand-built module starts at 0 / "" / false, which
//! is exactly what makes the clone case honest and what every other case has to set up explicitly.
//!
//! ⚠ CASE ORDER: cases run alphabetically by class name. Nothing here writes to a manager, a config or
//! a shared record that outlives its own case, so the order does not matter - but the names are still
//! chosen to read as a sequence: Arrival, Clone, Holding, Ladder, RuntimeSource.
//!
//! No polling, no waiting, no maxAttempts, no assertion about live AI reaching anywhere.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Exposes the protected seams of the module under test.
//!
//! A SUBCLASS RATHER THAN A WIDENED PRODUCTION API, deliberately. Everything below is protected
//! because no production caller may reach it - the state machine is driven by the module's own update
//! and by nothing else - and turning any of it public "so a test can get at it" would be exactly the
//! kind of widening this phase is under instruction not to do. A subclass is the language's own answer.
//------------------------------------------------------------------------------------------------
class OVT_TEST_MountedForceProbe : OVT_MountedForceSpawningDeploymentModule
{
	//------------------------------------------------------------------------------------------------
	//! \param[in] state The insertion state to plant.
	void ProbeSetState(OVT_EInsertionState state)
	{
		m_eState = state;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] reserved Whether this module should believe it holds a convoy slot.
	void ProbeSetReserved(bool reserved)
	{
		m_bReserved = reserved;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] lz Where the vehicle stopped.
	void ProbeSetLandingZone(vector lz)
	{
		m_vLZ = lz;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds one already-registered group to this module's force.
	//! \param[in] handle A virtualization handle.
	void ProbeAdoptHandle(int handle)
	{
		m_aHandles.Insert(handle);
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the arrival path.
	void ProbeCompleteInsertion()
	{
		CompleteInsertion();
	}

	//------------------------------------------------------------------------------------------------
	//! Runs one module update.
	//! \param[in] deltaTime Milliseconds since the last update.
	void ProbeUpdate(int deltaTime)
	{
		OnUpdate(deltaTime);
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the source resolution.
	//! \return Whether there is an origin to work from.
	bool ProbeEnsureSourceResolved()
	{
		return EnsureSourceResolved();
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the ladder query at a PLANTED threat, so no case has to write to the live threat figure.
	//! \param[in] factionIndex The faction whose registry to ask.
	//! \param[in] threat The threat to resolve against.
	//! \param[out] entry The picked rung.
	//! \return True when a rung was picked.
	bool ProbeResolveLadderEntry(int factionIndex, float threat, out OVT_FactionVehicleEntry entry)
	{
		return ResolveLadderEntry(factionIndex, threat, entry);
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the vehicle-prefab resolution the parent's SpawnTruck() would run.
	//! \param[in] factionIndex The faction whose registry to ask.
	//! \return The prefab, or an empty ResourceName.
	ResourceName ProbeGetVehiclePrefab(int factionIndex)
	{
		return GetVehiclePrefabFromFaction(factionIndex);
	}
}

//------------------------------------------------------------------------------------------------
//! Shared helpers, so five cases do not each grow their own copy of "find a group I can register".
//------------------------------------------------------------------------------------------------
class OVT_TEST_MountedForceFixture
{
	//! Owner system tag used by every registration here, so a leaked record is obvious in a log.
	static const string OWNER_SYSTEM = "test_mounted_force";

	//------------------------------------------------------------------------------------------------
	//! Registers one group at the riding ring, exactly as a mounted force's passengers are registered.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] ownerKey A key unique to the calling case.
	//! \return The handle, or -1 when this world offers no resolvable composition.
	static int RegisterRidingGroup(notnull OVT_VirtualizationManagerComponent virtualization, string ownerKey)
	{
		string factionKey;
		string groupName;
		if (!OVT_TEST_VirtualizationFixture.FindComposition(factionKey, groupName))
			return -1;

		return virtualization.RegisterGroup(OWNER_SYSTEM, ownerKey, factionKey, groupName, vector.Zero, null,
			OVT_InsertionSpawningDeploymentModule.RIDING_SPAWN_DISTANCE);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The occupying faction's index, or -1 when this world cannot resolve one.
	static int ResolveOccupyingFactionIndex()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return -1;

		return config.GetOccupyingFactionIndex();
	}
}

//------------------------------------------------------------------------------------------------
//! 🔴 ARRIVAL LEAVES THE FORCE ABOARD, ON THE RIDING RING, AND HANDS THE CONVOY SLOT BACK.
//!
//! Three claims, and the middle one is the reason this file exists:
//!   1. THE STATE BECOMES HOLDING, not RETURNING. RETURNING is the parent's "the men are down, send the
//!      empty vehicle home" - the exact opposite of what a mounted force is for.
//!   2. THE FORCE IS STILL ON THE RIDING RING. A registration at RIDING_SPAWN_DISTANCE is STRICTLY
//!      WIDER than the world's global ring, and that - and only that - is what makes
//!      SuppressForcesAroundBattle skip it (OVT_DeploymentManager.c:567). Put the force back on the
//!      global ring at arrival and the QRF's mounted echelon is pinned dormant the moment it reaches
//!      the battle it was sent to, and materialises nobody. Nothing logs it, nothing warns, and the
//!      deployment still exists and is still paid for.
//!   3. THE CONVOY SLOT IS RELEASED. A mounted force holds its vehicle for the rest of the deployment's
//!      life; a slot held that long is a slot the faction never gets back, and enough of them stop the
//!      faction driving anywhere for the rest of the campaign.
//!
//! AND THE CONTRAST THAT PROVES THE CASE IS MEASURING SOMETHING: the same arrival on a module with
//! m_bDismountOnArrival set reproduces the parent exactly - RETURNING, and the force back on the global
//! ring. Two modules, one difference, opposite answers on both readings.
//!
//! CAN-FAIL PROOF (faults injected one at a time and compiled; both exited tools/compile-check.sh 0 -
//! neither is a script error and nothing else in the tree would stop either shipping; the subject was
//! restored and recompiled clean afterwards):
//!   A1. `m_eState = OVT_EInsertionState.HOLDING;` in CompleteInsertion() changed to RETURNING. Fails on
//!       "arrival must leave a mounted force HOLDING".
//!   A2. `ReleaseReservation();` deleted from CompleteInsertion(). Fails on "arrival must hand the
//!       convoy slot back".
//! The ring claim's fault is the one that cannot be injected without writing the prohibited call, so it
//! is proven the other way round: the m_bDismountOnArrival arm of this very case takes the parent's path
//! and reads back the global ring, so the two readings differ inside one run.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_MountedForce_ArrivalHoldsTheForceAboard : SCR_AutotestCaseBase
{
	//! Somewhere to call a landing zone. Never vector.Zero, which several seams read as "unset".
	static const vector PROBE_LZ = "1000 0 1000";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null - the virtualization core did not resolve");
			return true;
		}

		int mountedHandle = OVT_TEST_MountedForceFixture.RegisterRidingGroup(virtualization, "mounted_arrival_holds");
		int liftHandle = OVT_TEST_MountedForceFixture.RegisterRidingGroup(virtualization, "mounted_arrival_lift");

		if (mountedHandle == -1 || liftHandle == -1)
		{
			virtualization.UnregisterGroup(mountedHandle);
			virtualization.UnregisterGroup(liftHandle);
			SetFailure("this world could not register two virtual groups, so the riding-ring claim cannot be asked at all");
			return true;
		}

		int globalRing = virtualization.GetGlobalSpawnDistance();

		// --- THE MOUNTED ARRIVAL.
		OVT_TEST_MountedForceProbe mounted = new OVT_TEST_MountedForceProbe();
		mounted.m_sModuleName = "OVT_TEST mounted arrival";
		mounted.ProbeSetState(OVT_EInsertionState.DRIVING);
		mounted.ProbeSetLandingZone(PROBE_LZ);
		mounted.ProbeSetReserved(true);
		mounted.ProbeAdoptHandle(mountedHandle);
		mounted.ProbeCompleteInsertion();

		OVT_EInsertionState mountedState = mounted.GetInsertionState();
		bool mountedStillReserved = mounted.HoldsInsertionReservation();
		int mountedRing = virtualization.GetSpawnDistance(mountedHandle);

		// --- THE SAME ARRIVAL ON A CONFIG THAT ASKED FOR A LIFT.
		OVT_TEST_MountedForceProbe lift = new OVT_TEST_MountedForceProbe();
		lift.m_sModuleName = "OVT_TEST lift arrival";
		lift.m_bDismountOnArrival = true;
		lift.ProbeSetState(OVT_EInsertionState.DRIVING);
		lift.ProbeSetLandingZone(PROBE_LZ);
		lift.ProbeSetReserved(true);
		lift.ProbeAdoptHandle(liftHandle);
		lift.ProbeCompleteInsertion();

		OVT_EInsertionState liftState = lift.GetInsertionState();
		int liftRing = virtualization.GetSpawnDistance(liftHandle);

		// --- TEAR DOWN BEFORE ASSERTING, so a red case leaves no record behind.
		virtualization.UnregisterGroup(mountedHandle);
		virtualization.UnregisterGroup(liftHandle);

		// --- ASSERT.
		if (mountedState != OVT_EInsertionState.HOLDING)
		{
			SetFailure("arrival must leave a mounted force HOLDING, read back state %1 - RETURNING would send the vehicle home and leave a mounted deployment with nothing mounted",
				mountedState.ToString());
			return true;
		}

		if (mountedStillReserved)
		{
			SetFailure("arrival must hand the convoy slot back - a mounted force holds its vehicle for the rest of the deployment's life, and a slot held that long is one the faction never drives on again");
			return true;
		}

		if (mountedRing <= globalRing)
		{
			SetFailure("a mounted force must still be on the riding ring after arrival: read back %1 m against a global ring of %2 m. Anything not STRICTLY wider is suppressed dormant inside its own battle and delivers an empty vehicle, silently",
				mountedRing.ToString(), globalRing.ToString());
			return true;
		}

		if (liftState != OVT_EInsertionState.RETURNING)
		{
			SetFailure("m_bDismountOnArrival must reproduce the plain insertion exactly - expected RETURNING, read back %1",
				liftState.ToString());
			return true;
		}

		if (liftRing != globalRing)
		{
			SetFailure("a dropped force must go back on the ordinary proximity ring: read back %1 m against a global ring of %2 m. If this reads the same as the mounted arm, this case is measuring nothing",
				liftRing.ToString(), globalRing.ToString());
			return true;
		}

		Print("MountedForce arrival: the force stays aboard on the riding ring, the convoy slot is released, and m_bDismountOnArrival still drops it to the global ring exactly as the insertion module does");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! THE STANDING CloneModule TRAP, on a module that repeats TWENTY-SEVEN lines by hand.
//!
//! Every deployment gets a CLONE of its config's modules (OVT_DeploymentComponent.InitializeDeployment)
//! and CloneModule copies attribute by attribute BY HAND and is NOT CHAINED - a subclass repeats its
//! whole ancestry's copy list. A forgotten line does not warn, does not log and does not fail to parse:
//! it ships the CLASS DEFAULT instead of the authored value, on every deployment, forever.
//!
//! WHAT A DROPPED LINE COSTS AMONG THIS MODULE'S OWN FOUR:
//!   - m_sVehicleRole          -> every mounted force in the campaign silently reverts to a soft-skinned
//!                                truck and the escalation the whole feature exists for never happens.
//!   - m_bDismountOnArrival    -> a config that wanted a lift gets a force that sits in its vehicle at
//!                                the drop point forever.
//!   - m_iHoldTicks            -> a bounded hold becomes an indefinite one, so a sweep never reports
//!                                itself finished and its deployment is never collected.
//!   - m_bAdoptExistingVehicle -> the base armour sortie spawns a SECOND vehicle beside the hull the
//!                                player watched it crew.
//!
//! EVERY PROBE VALUE IS NON-ZERO, NON-EMPTY AND NON-FALSE, which is the point: a `new` instance starts
//! at 0 / "" / false / enum 0, so a probe value of `false` or `0` would be indistinguishable from a
//! dropped copy and the assertion would pass while the bug shipped.
//!
//! PROVEN ABLE TO FAIL: delete any single `clone.X = X;` line from
//! OVT_MountedForceSpawningDeploymentModule.CloneModule() and this case goes red naming that field.
//! Faults were injected on m_sVehicleRole (one of the four), m_sTruckVehicleType (one of the parent's
//! ten) and m_eImportance (one of the grandparent's thirteen), to prove all three tiers are really
//! being checked; all three exited tools/compile-check.sh 0 and the subject was restored and recompiled
//! clean.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_MountedForce_CloneCarriesEveryAttribute : SCR_AutotestCaseBase
{
	//! Distinctive probe values. None may be 0, "" or false - see the class header.
	static const string PROBE_NAME = "OVT_TEST mounted probe";
	static const string PROBE_GROUP = "OVT_TEST group type";
	static const string PROBE_TRUCK = "OVT_TEST vehicle type";
	static const string PROBE_CREW = "OVT_TEST crew type";
	static const string PROBE_ROLE = "OVT_TEST ladder role";
	static const int PROBE_MIN_GROUPS = 2;
	static const int PROBE_MAX_GROUPS = 6;
	static const float PROBE_SPAWN_RADIUS = 87.5;
	static const int PROBE_COST = 43;
	static const int PROBE_REINFORCE_COST = 19;
	static const float PROBE_WALK_THRESHOLD = 512.25;
	static const float PROBE_STANDOFF = 275.75;
	static const float PROBE_STUCK_SPEED = 2.5;
	static const int PROBE_STUCK_TICKS = 7;
	static const float PROBE_ARRIVAL_RADIUS = 33.5;
	static const int PROBE_VEHICLE_COST = 61;
	static const int PROBE_HOLD_TICKS = 23;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_MountedForceSpawningDeploymentModule module = new OVT_MountedForceSpawningDeploymentModule();

		// --- Inherited from OVT_InfantrySpawningDeploymentModule - all thirteen.
		module.m_sModuleName = PROBE_NAME;
		module.m_sGroupType = PROBE_GROUP;
		module.m_iMinGroupCount = PROBE_MIN_GROUPS;
		module.m_iMaxGroupCount = PROBE_MAX_GROUPS;
		module.m_bScaleByTownSize = true;
		module.m_fSpawnRadius = PROBE_SPAWN_RADIUS;
		module.m_iCostPerGroup = PROBE_COST;
		module.m_bAllowReinforcement = true;
		module.m_iReinforcementCost = PROBE_REINFORCE_COST;
		module.m_bSpawnAtNearestBase = true;
		module.m_bReinforceFromNearestBase = true;
		module.m_eImportance = SCR_EAISpawnImportance.CRITICAL;
		module.m_bSnapToRoad = true;

		// --- Inherited from OVT_InsertionSpawningDeploymentModule - all eleven.
		module.m_Source = new OVT_NearestControlledBaseSourceProvider();
		module.m_fWalkThresholdDistance = PROBE_WALK_THRESHOLD;
		module.m_sTruckVehicleType = PROBE_TRUCK;
		module.m_sTruckCrewGroup = PROBE_CREW;
		module.m_fLZStandoffDistance = PROBE_STANDOFF;
		module.m_fStuckSpeedThreshold = PROBE_STUCK_SPEED;
		module.m_iStuckTicks = PROBE_STUCK_TICKS;
		module.m_fArrivalRadius = PROBE_ARRIVAL_RADIUS;
		module.m_iTruckCostOverride = PROBE_VEHICLE_COST;
		module.m_bWalkWhenInsertionRefused = true;
		module.m_bTransportIsObserver = true;

		// --- This module's own four.
		module.m_sVehicleRole = PROBE_ROLE;
		module.m_bDismountOnArrival = true;
		module.m_iHoldTicks = PROBE_HOLD_TICKS;
		module.m_bAdoptExistingVehicle = true;

		OVT_MountedForceSpawningDeploymentModule clone = OVT_MountedForceSpawningDeploymentModule.Cast(module.CloneModule());
		if (!clone)
		{
			SetFailure("OVT_MountedForceSpawningDeploymentModule.CloneModule() did not answer a mounted force module at all - every deployment running this config would get the wrong class");
			return true;
		}

		string failure = CompareGrandparent(module, clone);
		if (failure == "")
			failure = CompareParent(module, clone);
		if (failure == "")
			failure = CompareOwn(module, clone);

		if (failure != "")
		{
			SetFailure(failure);
			return true;
		}

		Print("MountedForce clone: all 27 attributes survive - 13 from the infantry module, 10 from the insertion module and the mounted module's own 4");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The thirteen OVT_InfantrySpawningDeploymentModule declares.
	//! \param[in] module The authored module.
	//! \param[in] clone Its clone.
	//! \return An empty string when all thirteen survived, or the first that did not.
	protected string CompareGrandparent(notnull OVT_MountedForceSpawningDeploymentModule module,
		notnull OVT_MountedForceSpawningDeploymentModule clone)
	{
		if (clone.m_sModuleName != module.m_sModuleName)
			return "the mounted module's clone lost m_sModuleName - its OWNER KEY is derived from it, so the force and the vehicle crew would be registered under keys nothing ever reclaims";

		if (clone.m_sGroupType != module.m_sGroupType)
			return "the mounted module's clone lost m_sGroupType - the core resolves (factionKey, groupName) and would refuse every registration, so the vehicle would drive out empty";

		if (clone.m_iMinGroupCount != module.m_iMinGroupCount)
			return "the mounted module's clone lost m_iMinGroupCount";

		if (clone.m_iMaxGroupCount != module.m_iMaxGroupCount)
			return "the mounted module's clone lost m_iMaxGroupCount - the force size would be the class default on every deployment";

		if (clone.m_bScaleByTownSize != module.m_bScaleByTownSize)
			return "the mounted module's clone lost m_bScaleByTownSize";

		if (clone.m_fSpawnRadius != module.m_fSpawnRadius)
			return "the mounted module's clone lost m_fSpawnRadius";

		if (clone.m_iCostPerGroup != module.m_iCostPerGroup)
			return "the mounted module's clone lost m_iCostPerGroup - the deployment's price would be wrong and the pool would stop balancing";

		if (clone.m_bAllowReinforcement != module.m_bAllowReinforcement)
			return "the mounted module's clone lost m_bAllowReinforcement - a wiped force would never be rebought";

		if (clone.m_iReinforcementCost != module.m_iReinforcementCost)
			return "the mounted module's clone lost m_iReinforcementCost";

		if (clone.m_bSpawnAtNearestBase != module.m_bSpawnAtNearestBase)
			return "the mounted module's clone lost m_bSpawnAtNearestBase";

		if (clone.m_bReinforceFromNearestBase != module.m_bReinforceFromNearestBase)
			return "the mounted module's clone lost m_bReinforceFromNearestBase";

		if (clone.m_eImportance != module.m_eImportance)
			return "the mounted module's clone lost m_eImportance - every group would register at the class default tier and lose the AI spawn-budget race, which for a crew means a vehicle with nobody in it";

		if (clone.m_bSnapToRoad != module.m_bSnapToRoad)
			return "the mounted module's clone lost m_bSnapToRoad";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The eleven OVT_InsertionSpawningDeploymentModule declares.
	//! \param[in] module The authored module.
	//! \param[in] clone Its clone.
	//! \return An empty string when all eleven survived, or the first that did not.
	protected string CompareParent(notnull OVT_MountedForceSpawningDeploymentModule module,
		notnull OVT_MountedForceSpawningDeploymentModule clone)
	{
		if (!clone.m_Source)
			return "the mounted module's clone carries NO source provider - it would refuse to put a force in the world at all, register nothing, and cost the faction the deployment's whole price for an empty marker";

		if (clone.m_fWalkThresholdDistance != module.m_fWalkThresholdDistance)
			return "the mounted module's clone lost m_fWalkThresholdDistance - a force whose objective is eighty metres away would board a vehicle to drive there";

		if (clone.m_sTruckVehicleType != module.m_sTruckVehicleType)
			return "the mounted module's clone lost m_sTruckVehicleType - it is BOTH the ladder's fallback AND the parent's non-empty gate, so an empty one means no vehicle is ever put on the road and every mounted force walks";

		if (clone.m_sTruckCrewGroup != module.m_sTruckCrewGroup)
			return "the mounted module's clone lost m_sTruckCrewGroup - an armed vehicle would be spawned with nobody driving it and nobody on its gun";

		if (clone.m_fLZStandoffDistance != module.m_fLZStandoffDistance)
			return "the mounted module's clone lost m_fLZStandoffDistance - the force would drive all the way ONTO the objective it was meant to stop short of";

		if (clone.m_fStuckSpeedThreshold != module.m_fStuckSpeedThreshold)
			return "the mounted module's clone lost m_fStuckSpeedThreshold";

		if (clone.m_iStuckTicks != module.m_iStuckTicks)
			return "the mounted module's clone lost m_iStuckTicks - a zero disables the stall test, so a wedged vehicle holds its force inside it until the deployment is torn down";

		if (clone.m_fArrivalRadius != module.m_fArrivalRadius)
			return "the mounted module's clone lost m_fArrivalRadius - the force would only arrive by stopping on the exact metre and would drive past its holding point";

		if (clone.m_iTruckCostOverride != module.m_iTruckCostOverride)
			return "the mounted module's clone lost m_iTruckCostOverride - it is the LADDER BUDGET as well as the price, so the deployment would be under-priced AND would refuse every rung";

		if (clone.m_bWalkWhenInsertionRefused != module.m_bWalkWhenInsertionRefused)
			return "the mounted module's clone lost m_bWalkWhenInsertionRefused - a refused convoy slot would wait instead of walking, so a saturated faction would stop inserting at all";

		if (clone.m_bTransportIsObserver != module.m_bTransportIsObserver)
			return "the mounted module's clone lost m_bTransportIsObserver - it clones as FALSE, so no engine observer is parked on the transport, the ENGINE STOPS SIMULATING THE HULL once the convoy is about a kilometre from every observer, and it stands at 0 m/s with a perfectly healthy crew until the stall test walks the force in";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The four this module declares.
	//! \param[in] module The authored module.
	//! \param[in] clone Its clone.
	//! \return An empty string when all four survived, or the first that did not.
	protected string CompareOwn(notnull OVT_MountedForceSpawningDeploymentModule module,
		notnull OVT_MountedForceSpawningDeploymentModule clone)
	{
		if (clone.m_sVehicleRole != module.m_sVehicleRole)
			return "the mounted module's clone lost m_sVehicleRole - every mounted force in the campaign would silently revert to a soft-skinned truck and the escalation this whole feature exists for would never happen";

		if (clone.m_bDismountOnArrival != module.m_bDismountOnArrival)
			return "the mounted module's clone lost m_bDismountOnArrival - a config that asked for a lift would get a force that sits in its vehicle at the drop point forever";

		if (clone.m_iHoldTicks != module.m_iHoldTicks)
			return "the mounted module's clone lost m_iHoldTicks - a bounded hold would become an indefinite one, so a sweep would never report itself finished and its deployment would never be collected";

		if (clone.m_bAdoptExistingVehicle != module.m_bAdoptExistingVehicle)
			return "the mounted module's clone lost m_bAdoptExistingVehicle - the base armour sortie would spawn a SECOND vehicle beside the hull the player watched it crew";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! 🔴 A HOLD THAT LOSES ITS VEHICLE STILL PRODUCES A MARCH.
//!
//! The walk fallback is the SPINE of the module this one subclasses, not its error handling: the force
//! is registered first, holding a plan that already points at the objective, and every way of losing the
//! vehicle ends with men on the ground walking that plan. HOLDING is a new terminal state and it is the
//! one place a new way to strand a force could have been introduced - a state whose tick forgot to
//! reach the fallback would leave a squad sitting inside a wreck for the rest of the campaign.
//!
//! ⚠ THE FALLBACK IS ALSO WHERE THE RIDING RING IS CORRECTLY GIVEN UP. The men are on the ground now,
//! so the exemption is no longer wanted: this case asserts the handle comes back to the global ring,
//! which is the mirror image of the arrival case and stops "never restore the ring" being read as an
//! absolute rather than as "not while they are aboard".
//!
//! Two claims:
//!   1. A HOLDING module with no vehicle reaches WALKING on its next update.
//!   2. Its force is back on the ordinary proximity ring, so it is virtualized like any other marching
//!      squad rather than being permanently materialised at 100 km for the rest of the campaign.
//!
//! CAN-FAIL PROOF: `DismountAndWalk("its vehicle was destroyed");` in TickHold() replaced with `return;`
//! - the state stays HOLDING and the case fails on "a mounted force that has lost its vehicle must fall
//! back to the march". Compiled clean (exit 0); subject restored and recompiled clean.
//!
//! No maxAttempts: ONE synchronous update, and the state is read immediately afterwards.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_MountedForce_HoldingFallsBackToTheMarch : SCR_AutotestCaseBase
{
	//! One deployment update, in milliseconds. The nominal figure the framework passes.
	static const int ONE_UPDATE_MS = 10000;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
		{
			SetFailure("OVT_Global.GetVirtualization() is null - the virtualization core did not resolve");
			return true;
		}

		int handle = OVT_TEST_MountedForceFixture.RegisterRidingGroup(virtualization, "mounted_holding_walks");
		if (handle == -1)
		{
			SetFailure("this world could not register a virtual group, so the fallback's ring claim cannot be asked at all");
			return true;
		}

		int globalRing = virtualization.GetGlobalSpawnDistance();

		// A holding force whose vehicle is gone. m_Truck is left null, which is what a destroyed and
		// deleted vehicle looks like from here - IsTruckOperational() answers the same for a wreck.
		OVT_TEST_MountedForceProbe module = new OVT_TEST_MountedForceProbe();
		module.m_sModuleName = "OVT_TEST holding force";
		module.ProbeSetState(OVT_EInsertionState.HOLDING);
		module.ProbeAdoptHandle(handle);

		module.ProbeUpdate(ONE_UPDATE_MS);

		OVT_EInsertionState state = module.GetInsertionState();
		int ring = virtualization.GetSpawnDistance(handle);

		virtualization.UnregisterGroup(handle);

		if (state != OVT_EInsertionState.WALKING)
		{
			SetFailure("a mounted force that has lost its vehicle must fall back to the march: read back state %1. Anything else leaves a squad sitting inside a wreck holding a plan it will never execute",
				state.ToString());
			return true;
		}

		if (ring != globalRing)
		{
			SetFailure("a force put back on the ground must go back on the ordinary proximity ring: read back %1 m against a global ring of %2 m. Left at the riding ring it is permanently materialised for the rest of the campaign, one squad of the AI budget per insertion",
				ring.ToString(), globalRing.ToString());
			return true;
		}

		Print("MountedForce fallback: a hold that loses its vehicle reaches WALKING and puts its force back on the ordinary proximity ring");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! THE LADDER, resolved against the SHIPPED faction registries, and the budget that bounds it (D4).
//!
//! Four claims:
//!   1. AT A HIGH THREAT WITH AN UNBOUNDED BUDGET the occupying faction's role 'armed' resolves to a
//!      rung. This is the wiring claim: the right registry, the right role string, the right resolver.
//!   2. THE BUDGET IS A CEILING, NOT A SUGGESTION. The same role at the same threat with a budget of
//!      exactly the cheapest rung's price resolves to something no dearer than that budget. This is
//!      what makes escalation cost money: a config that wants a BTR-70 has to author what one costs.
//!   3. A BUDGET UNDER EVERY RUNG ANSWERS FALSE rather than picking one anyway.
//!   4. AN UNAUTHORED ROLE FALLS BACK TO THE NAMED VEHICLE TYPE rather than answering nothing. The
//!      drive-vs-walk decision has already been made by the time the module asks for a prefab, so a
//!      ladder that cannot answer must never be able to turn into a force that never leaves.
//!
//! ⚠ NOTHING HERE WRITES THE LIVE THREAT FIGURE. The threat is handed straight to the resolver through
//! the probe, so this case is deterministic in a world whose campaign threat is whatever it is, and it
//! cannot disturb a threat-driven system running in the same world.
//!
//! ⚠ NO PREFAB PATH OR VEHICLE NAME IS WRITTEN AS A LITERAL. The rung table is authored in
//! Configs/Factions/*.conf and OVT_TEST_Init_VehicleLadderResolution.c already pins that both shipped
//! registries answer three DISTINCT rungs; repeating the names here would make this case a second copy
//! of a config file.
//!
//! CAN-FAIL PROOF (compiled clean, exit 0, subject restored):
//!   L1. The budget argument in ResolveLadderEntry() changed from m_iTruckCostOverride to -1. Fails on
//!       claim 3, "a budget under every rung must answer no rung at all".
//!   L2. `if (m_sVehicleRole.IsEmpty()) return super.GetVehiclePrefabFromFaction(factionIndex);` deleted
//!       from the override. Fails on claim 4 with an empty prefab, which downstream is one of the roads
//!       to walking - i.e. a silent loss of every mounted force in the campaign.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_MountedForce_LadderPicksARungInsideItsBudget : SCR_AutotestCaseBase
{
	//! The role every shipped rung is authored under. One string, matched by string at runtime, named
	//! in four deployment configs across four later phases - so a typo is worth catching here.
	static const string LADDER_ROLE = "armed";

	//! Far above every authored threshold at every difficulty scale, so claim 1 is about the wiring and
	//! never about whether the campaign has escalated.
	static const float UNLOCK_EVERYTHING_THREAT = 100000;

	//! A budget nothing can fit inside. OVT_VehicleLadderRules.RungAffordable treats a NEGATIVE budget
	//! as unbounded and a zero one as "only something free would do", so zero is the smallest bounded
	//! purse there is - and it is also a real authored value: an operator who wants no vehicle budget.
	static const int ZERO_BUDGET = 0;

	//! Unbounded, per OVT_VehicleLadderRules.RungAffordable.
	static const int UNBOUNDED_BUDGET = -1;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		int factionIndex = OVT_TEST_MountedForceFixture.ResolveOccupyingFactionIndex();
		if (factionIndex < 0)
		{
			SetFailure("this world did not resolve an occupying faction index, so no registry can be asked");
			return true;
		}

		// --- CLAIM 1: unbounded, at a threat above everything.
		OVT_TEST_MountedForceProbe unbounded = new OVT_TEST_MountedForceProbe();
		unbounded.m_sModuleName = "OVT_TEST ladder unbounded";
		unbounded.m_sVehicleRole = LADDER_ROLE;
		unbounded.m_iTruckCostOverride = UNBOUNDED_BUDGET;

		OVT_FactionVehicleEntry top;
		if (!unbounded.ProbeResolveLadderEntry(factionIndex, UNLOCK_EVERYTHING_THREAT, top))
		{
			SetFailure("the occupying faction's role '%1' resolved no rung at all with an unbounded budget - either the registries are unauthored or the module is asking the wrong faction",
				LADDER_ROLE);
			return true;
		}

		if (top.m_sVehiclePrefab.IsEmpty())
		{
			SetFailure("the top rung of role '%1' resolved to an entry with no prefab, which downstream is one of the roads to walking", LADDER_ROLE);
			return true;
		}

		// --- CLAIM 2: the same pick, bounded to the price of the rung it just answered MINUS one, must
		//     come back cheaper. Expressed against the resolved price rather than a literal, so this
		//     case does not carry a copy of the rung table.
		int tightBudget = top.m_iCost - 1;
		if (tightBudget < 0)
			tightBudget = 0;

		OVT_TEST_MountedForceProbe bounded = new OVT_TEST_MountedForceProbe();
		bounded.m_sModuleName = "OVT_TEST ladder bounded";
		bounded.m_sVehicleRole = LADDER_ROLE;
		bounded.m_iTruckCostOverride = tightBudget;

		OVT_FactionVehicleEntry cheaper;
		if (bounded.ProbeResolveLadderEntry(factionIndex, UNLOCK_EVERYTHING_THREAT, cheaper) && cheaper.m_iCost > tightBudget)
		{
			SetFailure("the ladder answered a rung costing %1 inside a budget of %2 - the vehicle price is a CEILING, and a ladder that ignores it makes escalation free",
				cheaper.m_iCost.ToString(), tightBudget.ToString());
			return true;
		}

		// --- CLAIM 3: a budget under every rung answers nothing rather than picking one anyway.
		OVT_TEST_MountedForceProbe broke = new OVT_TEST_MountedForceProbe();
		broke.m_sModuleName = "OVT_TEST ladder broke";
		broke.m_sVehicleRole = LADDER_ROLE;
		broke.m_iTruckCostOverride = ZERO_BUDGET;

		OVT_FactionVehicleEntry unaffordable;
		if (broke.ProbeResolveLadderEntry(factionIndex, UNLOCK_EVERYTHING_THREAT, unaffordable))
		{
			SetFailure("a budget of %1 must answer no rung at all - it answered '%2'",
				ZERO_BUDGET.ToString(), unaffordable.m_sVehicleName);
			return true;
		}

		// --- CLAIM 4: an unauthored role falls back to the named vehicle type.
		OVT_TEST_MountedForceProbe unauthored = new OVT_TEST_MountedForceProbe();
		unauthored.m_sModuleName = "OVT_TEST ladder unauthored";
		unauthored.m_sTruckVehicleType = ResolveAnyVehicleName(factionIndex);

		if (unauthored.m_sTruckVehicleType == "")
		{
			SetFailure("the occupying faction's vehicle registry is empty, so the fallback claim cannot be asked");
			return true;
		}

		ResourceName fallbackPrefab = unauthored.ProbeGetVehiclePrefab(factionIndex);
		if (fallbackPrefab.IsEmpty())
		{
			SetFailure("with no ladder role authored, the module must fall back to the named vehicle type '%1' - it resolved nothing, which downstream means every mounted force walks and nothing says why",
				unauthored.m_sTruckVehicleType);
			return true;
		}

		Print("MountedForce ladder: role 'armed' resolves against the shipped registry, the authored vehicle price bounds the pick, an impossible budget answers no rung, and an unauthored role still resolves a vehicle by name");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Any vehicle name the faction's registry actually holds, so the fallback claim names a real one.
	//! \param[in] factionIndex The faction to ask.
	//! \return A registry name, or an empty string.
	protected string ResolveAnyVehicleName(int factionIndex)
	{
		OVT_OverthrowFactionManager factions = OVT_Global.GetFactions();
		if (!factions)
			return "";

		OVT_Faction faction = factions.GetOverthrowFactionByIndex(factionIndex);
		if (!faction)
			return "";

		faction.InitializeVehicleRegistry();

		array<string> names = faction.GetAvailableVehicleNames();
		if (!names || names.IsEmpty())
			return "";

		return names[0];
	}
}

//------------------------------------------------------------------------------------------------
//! A RUNTIME SOURCE BEATS THE AUTHORED PROVIDER, AND ONLY BEFORE ANYTHING HAS BEEN RESOLVED (D7).
//!
//! The QRF's mounted echelon and the base armour sortie both need "come from THIS base", which is a
//! runtime fact no authored provider knows. Rather than invent a mutable provider, the module carries
//! an override its own source resolution prefers - and the ordering rule is the load-bearing half: the
//! source decides both the anchor the force is registered at and the ring it is registered on, and
//! neither can be changed afterwards without throwing away the survivor mask.
//!
//! Three claims:
//!   1. THE OVERRIDE ANSWERS WHERE THE PROVIDER COULD NOT. Both subjects carry a real authored provider
//!      and NEITHER has a parent deployment, so the provider path can only answer false - which is what
//!      makes the difference between the two attributable to the override and nothing else.
//!   2. WITHOUT ONE, NOTHING IS INVENTED. The unset module resolves false and reports vector.Zero, never
//!      the world origin. "There is nowhere for this force to come from" and "it comes from 0 0 0" are
//!      the same value and completely different facts; getting it wrong drives a vehicle to the
//!      south-west corner of the map.
//!   3. A LATE OVERRIDE IS REFUSED. Once a source is resolved, a second one is ignored rather than
//!      silently moving an origin the force is already registered against.
//!
//! CAN-FAIL PROOF (compiled clean, exit 0, subject restored):
//!   S1. The override branch deleted from EnsureSourceResolved(). Fails on claim 1, "a runtime source
//!       must be preferred over the authored provider".
//!   S2. The late-offer guard in SetSourceOverride() neutralised (its condition replaced with `false`).
//!       Fails on claim 3, "a source handed in after one has been resolved must be ignored".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_MountedForce_RuntimeSourceBeatsTheAuthoredProvider : SCR_AutotestCaseBase
{
	//! A planted origin. Never vector.Zero, which every seam here reads as "unset".
	static const vector PLANTED_SOURCE = "2500 0 3500";

	//! A second, different origin, offered too late.
	static const vector LATE_SOURCE = "4500 0 1500";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- CLAIM 1: with an override.
		OVT_TEST_MountedForceProbe overridden = new OVT_TEST_MountedForceProbe();
		overridden.m_sModuleName = "OVT_TEST runtime source";
		overridden.m_Source = new OVT_NearestControlledBaseSourceProvider();
		overridden.SetSourceOverride(PLANTED_SOURCE);

		bool overriddenResolved = overridden.ProbeEnsureSourceResolved();
		vector overriddenSource = overridden.GetInsertionSource();

		// --- CLAIM 2: without one, and otherwise identical.
		OVT_TEST_MountedForceProbe authored = new OVT_TEST_MountedForceProbe();
		authored.m_sModuleName = "OVT_TEST authored source";
		authored.m_Source = new OVT_NearestControlledBaseSourceProvider();

		bool authoredResolved = authored.ProbeEnsureSourceResolved();
		vector authoredSource = authored.GetInsertionSource();

		// --- CLAIM 3: a late override on the module that has already resolved one.
		overridden.SetSourceOverride(LATE_SOURCE);
		vector sourceAfterLateOffer = overridden.GetInsertionSource();

		if (!overriddenResolved)
		{
			SetFailure("a runtime source must be preferred over the authored provider - the module refused to resolve one at all, so the QRF echelon and the armour sortie could never say which base they set out from");
			return true;
		}

		if (overriddenSource != PLANTED_SOURCE)
		{
			SetFailure("the runtime source must be the one resolved: planted %1, read back %2",
				PLANTED_SOURCE.ToString(), overriddenSource.ToString());
			return true;
		}

		if (authoredResolved)
		{
			SetFailure("a module with no parent deployment and no override must not resolve a source at all - if it can, this case is not measuring the override");
			return true;
		}

		if (authoredSource != vector.Zero)
		{
			SetFailure("a module that could not resolve an origin must report vector.Zero, not a place: read back %1. 'Nowhere to come from' and 'comes from the world origin' are the same value and completely different facts",
				authoredSource.ToString());
			return true;
		}

		if (sourceAfterLateOffer != PLANTED_SOURCE)
		{
			SetFailure("a source handed in after one has been resolved must be ignored: the origin moved from %1 to %2, and the force is already registered against the first",
				PLANTED_SOURCE.ToString(), sourceAfterLateOffer.ToString());
			return true;
		}

		Print("MountedForce runtime source: an override beats the authored provider, an unset module invents nothing, and a late override is refused");

		return true;
	}
}
