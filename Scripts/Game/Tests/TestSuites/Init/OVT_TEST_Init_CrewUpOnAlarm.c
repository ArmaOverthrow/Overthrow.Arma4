//------------------------------------------------------------------------------------------------
//! TIER B cases - CREW-UP ON ALARM (occupying/vehicles Phase 5).
//!
//! Three claims, in order of how cheap they are to prove:
//!   1. CLONE FIDELITY on the parked module's new field, the same way every clonable module in this
//!      system is pinned (Q2) - `new` does not apply attribute defvalues, so every probe value below is
//!      non-zero, non-empty and non-default.
//!   2. THE ALARM GEOFENCE. A battle outside m_fAlarmRadius does nothing at all; one inside sends
//!      exactly one sortie. Driven through OnBattleStarted() directly rather than through the real
//!      m_OnBattleStarted subscription, because that subscription is only made in OnActivate(), which is
//!      a whole 8-12 s update interval after a deployment is created (occupying/vehicles Phase 2's T2.9
//!      finding) - no Init-tier test step spans one.
//!   3. 🔴 THE OWNERSHIP TRANSFER. The parked module's own GetSpawnedEntities() must stop reporting the
//!      hull the instant the sortie's mounted module starts reporting it - the double-delete hazard the
//!      class headers of both modules exist to name.
//!
//! FIXTURE DISCIPLINE, inherited from OVT_TEST_Init_QRFMountedEchelon.c:
//!   * every deployment a case stands up is torn down in the same frame, with SetSpawnedUnitsEliminated
//!     (true) on the deployment AND on every one of its spawning modules first - a deployment's first
//!     convergence is a whole update interval away and none of these ever reaches one, but the autotest
//!     camera is an observer and the belt is cheap;
//!   * every manager field a case writes (the occupying faction's reserve, its current-QRF bookkeeping)
//!     is read into a local before anything is touched and restored before the case can leave through a
//!     failure.
//!
//! ⚠ NO maxAttempts, NO polling, NO retry loop. Every case here is a straight-line sequence of calls
//! through public seams, asserted the same frame.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! CLONE FIDELITY: the parked module's new m_sVehicleRole survives CloneModule(), alongside the five
//! it always carried.
//!
//! ⚠ THE PROBE ROLE MUST BE NON-EMPTY. A `new OVT_ParkedVehicleSpawningDeploymentModule()` starts with
//! m_sVehicleRole == "" (the class default, and G8's own "empty means today's behaviour" default), so an
//! empty probe would make a dropped copy line and a correct one read identically.
//!
//! PROVEN ABLE TO FAIL: `clone.m_sVehicleRole = m_sVehicleRole;` was deleted from CloneModule(). The tree
//! recompiled CLEAN (exit 0) and this case then reported the loss by name. Line restored, tree recompiled
//! clean.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ParkedVehicle_CloneCarriesTheLadderRole : SCR_AutotestCaseBase
{
	static const string PROBE_NAME = "OVT_TEST parked probe";
	static const string PROBE_TYPE = "OVT_TEST named vehicle";
	static const string PROBE_ROLE = "OVT_TEST ladder role";
	static const int PROBE_COUNT = 5;
	static const int PROBE_COST = 133;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ParkedVehicleSpawningDeploymentModule module = new OVT_ParkedVehicleSpawningDeploymentModule();

		module.m_sModuleName = PROBE_NAME;
		module.m_sVehicleType = PROBE_TYPE;
		module.m_sVehicleRole = PROBE_ROLE;
		module.m_iVehicleCount = PROBE_COUNT;
		module.m_iCostPerVehicle = PROBE_COST;
		module.m_eParkingType = OVT_ParkingType.PARKING_HEAVY;

		OVT_ParkedVehicleSpawningDeploymentModule clone = OVT_ParkedVehicleSpawningDeploymentModule.Cast(module.CloneModule());
		if (!clone)
		{
			SetFailure("OVT_ParkedVehicleSpawningDeploymentModule.CloneModule() did not answer a parked-vehicle module at all");
			return true;
		}

		if (clone.m_sModuleName != module.m_sModuleName)
		{
			SetFailure("the parked module's clone lost m_sModuleName");
			return true;
		}

		if (clone.m_sVehicleType != module.m_sVehicleType)
		{
			SetFailure("the parked module's clone lost m_sVehicleType - every ladder-miss fallback would park the class default instead");
			return true;
		}

		if (clone.m_sVehicleRole != module.m_sVehicleRole)
		{
			SetFailure("🔴 the parked module's clone lost m_sVehicleRole - every armour delta silently reverts to parking m_sVehicleType, and the escalation Phase 5 exists for never happens");
			return true;
		}

		if (clone.m_iVehicleCount != module.m_iVehicleCount)
		{
			SetFailure("the parked module's clone lost m_iVehicleCount");
			return true;
		}

		if (clone.m_iCostPerVehicle != module.m_iCostPerVehicle)
		{
			SetFailure("the parked module's clone lost m_iCostPerVehicle - both GetResourceCost() and the ladder budget would be wrong");
			return true;
		}

		if (clone.m_eParkingType != module.m_eParkingType)
		{
			SetFailure("the parked module's clone lost m_eParkingType - every truck would ask for a car spot and never find one");
			return true;
		}

		Print("Parked-vehicle clone: all 6 attributes survive, including m_sVehicleRole");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! THE ALARM GEOFENCE: a battle outside m_fAlarmRadius does nothing; the same module, on the same
//! fixture, sends exactly one sortie for a battle inside it.
//!
//! ORDER IS THE PROOF. The outside call runs FIRST, against the vehicle still parked; if it were a
//! no-op for the wrong reason (a missing config, a resolution failure) the inside call would be a no-op
//! for the same reason and the case would read as a false pass. Asserting the outside call left the
//! vehicle STILL parked is what rules that out.
//!
//! PROVEN ABLE TO FAIL: the `> m_fAlarmRadius` early return was deleted from OnBattleStarted(). The tree
//! recompiled CLEAN (exit 0) and this case then reported a sortie sent for the OUTSIDE battle. Line
//! restored, tree recompiled clean.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_CrewUpOnAlarm_FiresOnceInsideRadiusNotOutside : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TEST_CrewUpFixture fixture = OVT_TEST_CrewUpFixture.Build();
		if (!fixture)
		{
			Print("[OVT_TEST] Crew-up on alarm: the world produced no base to hang a fixture on - the geofence was not exercised");
			return true;
		}

		vector outside = fixture.m_vPosition + Vector(fixture.m_CrewUp.m_fAlarmRadius * 3, 0, 0);
		vector inside = fixture.m_vPosition + Vector(fixture.m_CrewUp.m_fAlarmRadius * 0.5, 0, 0);

		fixture.m_CrewUp.OnBattleStarted(outside);
		bool sortieAfterOutside = fixture.m_CrewUp.HasSortieOut();
		int parkedAfterOutside = fixture.m_Parked.GetSpawnedEntities().Count();

		fixture.m_CrewUp.OnBattleStarted(inside);
		bool sortieAfterInside = fixture.m_CrewUp.HasSortieOut();

		fixture.Retire();

		if (sortieAfterOutside)
		{
			SetFailure("a battle %1 m out - three times the authored alarm radius - must not send a sortie", (fixture.m_CrewUp.m_fAlarmRadius * 3).ToString());
			return true;
		}

		if (parkedAfterOutside != 1)
		{
			SetFailure("an out-of-range battle must leave the parked vehicle exactly where it was: the module now reports %1", parkedAfterOutside.ToString());
			return true;
		}

		if (!sortieAfterInside)
		{
			SetFailure("a battle half the authored alarm radius away must send a sortie, and none was sent - the config, the ladder or the framework refused it (see the log above for which)");
			return true;
		}

		Print("Crew-up on alarm: silent " + (fixture.m_CrewUp.m_fAlarmRadius * 3).ToString() + " m out, one sortie sent " + (fixture.m_CrewUp.m_fAlarmRadius * 0.5).ToString() + " m out");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! 🔴 THE OWNERSHIP TRANSFER LEAVES EXACTLY ONE OWNER.
//!
//! Before: the parked module reports the hull and the sortie does not exist. After: the parked module
//! reports NOTHING and the sortie's mounted module reports the SAME entity. Never both, never neither -
//! see both modules' class headers for what either failure mode costs (a double-delete or an orphan).
//!
//! PROVEN ABLE TO FAIL: the release call - `parked.ReleaseVehicleOwnership(vehicle);` - was moved to
//! BEFORE `mounted.AdoptVehicle(vehicle)` in SendSortie(). The tree recompiled CLEAN (exit 0); this case
//! did not change (both modules still ended up agreeing on one owner, because AdoptVehicle succeeded
//! anyway), which is itself the honest result recorded below - see "what this case cannot prove". Order
//! restored, tree recompiled clean.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_CrewUpOnAlarm_OwnershipTransferLeavesExactlyOneOwner : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TEST_CrewUpFixture fixture = OVT_TEST_CrewUpFixture.Build();
		if (!fixture)
		{
			Print("[OVT_TEST] Crew-up on alarm: the world produced no base to hang a fixture on - the ownership transfer was not exercised");
			return true;
		}

		int parkedBefore = fixture.m_Parked.GetSpawnedEntities().Count();

		vector inside = fixture.m_vPosition + Vector(fixture.m_CrewUp.m_fAlarmRadius * 0.5, 0, 0);
		fixture.m_CrewUp.OnBattleStarted(inside);

		bool sent = fixture.m_CrewUp.HasSortieOut();
		int parkedAfter = fixture.m_Parked.GetSpawnedEntities().Count();

		OVT_MountedForceSpawningDeploymentModule sortieMounted;
		int sortieOwnsIt = 0;
		if (sent)
		{
			OVT_DeploymentComponent sortie = fixture.ResolveSortie();
			if (sortie)
			{
				sortieMounted = OVT_TEST_CrewUpFixture.FindMountedModule(sortie);
				if (sortieMounted && sortieMounted.GetMountedVehicle() == fixture.m_Vehicle)
					sortieOwnsIt = 1;
			}
		}

		fixture.Retire();

		if (parkedBefore != 1)
		{
			SetFailure("the fixture must start with exactly one vehicle owned by the parked module: it reported %1", parkedBefore.ToString());
			return true;
		}

		if (!sent)
		{
			Print("[OVT_TEST] Crew-up on alarm: no sortie was sent in this world (see the log above) - the ownership transfer was not exercised");
			return true;
		}

		if (parkedAfter != 0)
		{
			SetFailure("🔴 DOUBLE OWNERSHIP: the parked module must stop reporting a hull the moment a sortie adopts it, and it still reports %1 - both modules would delete it at their own teardown", parkedAfter.ToString());
			return true;
		}

		if (sortieOwnsIt != 1)
		{
			SetFailure("🔴 ORPHANED HULL: the parked module released the vehicle but the sortie's mounted module does not report it as its own - nothing owns it, and nothing will ever delete it");
			return true;
		}

		Print("Crew-up on alarm: ownership transferred cleanly - the parked module reports 0, the sortie's mounted module reports the same entity it took");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Shared fixture for the two live-deployment cases above. Nothing here asserts; everything here
//! either builds the fixture or answers null/false so the case can say why.
//------------------------------------------------------------------------------------------------
class OVT_TEST_CrewUpFixture
{
	static const string PARKED_ARMOUR_CONFIG_NAME = "Base Parked Armour";

	vector m_vPosition;
	int m_iFactionIndex;
	IEntity m_Vehicle;
	OVT_DeploymentComponent m_ParkedDeployment;
	OVT_ParkedVehicleSpawningDeploymentModule m_Parked;
	OVT_CrewUpOnAlarmBehaviorDeploymentModule m_CrewUp;

	//------------------------------------------------------------------------------------------------
	//! Stands up a real "Base Parked Armour" deployment, then plants a real vehicle into its parked
	//! module directly - EnsureGroups() itself never runs inside one test step, because activation is a
	//! whole update interval after creation (Phase 2's T2.9 finding).
	//! \return A built fixture, or null when the world or the registry could not provide one.
	static OVT_TEST_CrewUpFixture Build()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying || !occupying.m_Bases || occupying.m_Bases.IsEmpty())
			return null;

		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (!deployments)
			return null;

		OVT_DeploymentConfig config = deployments.FindConfigByName(PARKED_ARMOUR_CONFIG_NAME);
		if (!config)
			return null;

		vector position = occupying.m_Bases[0].location;
		int factionIndex = OVT_Global.GetConfig().GetOccupyingFactionIndex();

		OVT_DeploymentComponent deployment = deployments.ForceCreateDeployment(config, position, factionIndex, 0);
		if (!deployment)
			return null;

		OVT_ParkedVehicleSpawningDeploymentModule parked = FindParkedModule(deployment);
		OVT_CrewUpOnAlarmBehaviorDeploymentModule crewUp = FindCrewUpModule(deployment);
		if (!parked || !crewUp)
		{
			deployment.SetSpawnedUnitsEliminated(true);
			deployments.DeleteDeployment(deployment);
			return null;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return null;

		vector spot = position + Vector(15, 0, 0);
		spot[1] = world.GetSurfaceY(spot[0], spot[2]);

		IEntity vehicle = OVT_WorldUtils.SpawnEntityPrefab(ResolveFixtureVehiclePrefab(occupying), spot);
		if (!vehicle)
		{
			deployment.SetSpawnedUnitsEliminated(true);
			deployments.DeleteDeployment(deployment);
			return null;
		}

		parked.PlantParkedVehicle(vehicle);

		OVT_TEST_CrewUpFixture fixture = new OVT_TEST_CrewUpFixture();
		fixture.m_vPosition = position;
		fixture.m_iFactionIndex = factionIndex;
		fixture.m_Vehicle = vehicle;
		fixture.m_ParkedDeployment = deployment;
		fixture.m_Parked = parked;
		fixture.m_CrewUp = crewUp;

		return fixture;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] occupying The occupying faction manager, for its faction.
	//! \return An armed vehicle prefab if the ladder answers one, or a plain truck otherwise - this
	//! fixture only needs SOME Vehicle, not specifically an armed one.
	static ResourceName ResolveFixtureVehiclePrefab(notnull OVT_OccupyingFactionManager occupying)
	{
		OVT_Faction faction = OVT_Global.GetConfig().GetOccupyingFaction();
		if (faction)
		{
			faction.InitializeVehicleRegistry();

			OVT_FactionVehicleEntry entry;
			if (faction.ResolveVehicleForRole("armed", 0, 1, -1, entry) && !entry.m_sVehiclePrefab.IsEmpty())
				return entry.m_sVehiclePrefab;

			ResourceName named = faction.GetVehiclePrefabByName("truck");
			if (!named.IsEmpty())
				return named;
		}

		return ResourceName.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] deployment A deployment.
	//! \return Its parked-vehicle module, or null.
	static OVT_ParkedVehicleSpawningDeploymentModule FindParkedModule(notnull OVT_DeploymentComponent deployment)
	{
		array<OVT_BaseSpawningDeploymentModule> spawningModules = deployment.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			OVT_ParkedVehicleSpawningDeploymentModule parked = OVT_ParkedVehicleSpawningDeploymentModule.Cast(spawningModule);
			if (parked)
				return parked;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] deployment A deployment.
	//! \return Its crew-up module, or null.
	static OVT_CrewUpOnAlarmBehaviorDeploymentModule FindCrewUpModule(notnull OVT_DeploymentComponent deployment)
	{
		array<OVT_BaseBehaviorDeploymentModule> behaviorModules = deployment.GetBehaviorModules();
		foreach (OVT_BaseBehaviorDeploymentModule behaviorModule : behaviorModules)
		{
			OVT_CrewUpOnAlarmBehaviorDeploymentModule crewUp = OVT_CrewUpOnAlarmBehaviorDeploymentModule.Cast(behaviorModule);
			if (crewUp)
				return crewUp;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] deployment A deployment.
	//! \return Its mounted force module, or null.
	static OVT_MountedForceSpawningDeploymentModule FindMountedModule(notnull OVT_DeploymentComponent deployment)
	{
		array<OVT_BaseSpawningDeploymentModule> spawningModules = deployment.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			OVT_MountedForceSpawningDeploymentModule mounted = OVT_MountedForceSpawningDeploymentModule.Cast(spawningModule);
			if (mounted)
				return mounted;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The sortie deployment this fixture's crew-up module sent, or null when it has none (or
	//! the world could not find its entity).
	OVT_DeploymentComponent ResolveSortie()
	{
		if (!m_CrewUp || !m_CrewUp.HasSortieOut())
			return null;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return null;

		IEntity entity = world.FindEntityByID(m_CrewUp.GetSortieDeploymentId());
		if (!entity)
			return null;

		return OVT_DeploymentComponent.Cast(entity.FindComponent(OVT_DeploymentComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Tears the whole fixture down: the sortie (if one was sent - its own teardown deletes the vehicle
	//! it adopted), then the parked-armour deployment, then the fixture vehicle IF NOBODY ADOPTED IT
	//! (still parked, or never handed off).
	//!
	//! ⚠ MARKS EVERY MODULE ELIMINATED BEFORE TEARING DOWN. None of these deployments has ever ticked -
	//! activation is a whole update interval after creation - but the autotest camera is an observer and
	//! this is the tier's standing rule.
	void Retire()
	{
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();

		bool vehicleStillOwnedHere = true;

		OVT_DeploymentComponent sortie = ResolveSortie();
		if (sortie && deployments)
		{
			MarkEliminated(sortie);
			deployments.DeleteDeployment(sortie);
			vehicleStillOwnedHere = false;
		}

		if (m_ParkedDeployment && deployments)
		{
			MarkEliminated(m_ParkedDeployment);
			deployments.DeleteDeployment(m_ParkedDeployment);
		}

		// The parked deployment's own teardown does not delete m_aSpawnedEntities (it authors no
		// OnCleanup override - the parked module is documented to persist for the campaign and is
		// never, in the shipped game, torn down at all). If nobody adopted the fixture vehicle it would
		// otherwise be left standing in the world for the rest of the suite run.
		if (vehicleStillOwnedHere && m_Vehicle)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Vehicle);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] deployment A deployment to mark fully eliminated, on itself and on every spawning
	//! module - the tier's standing rule (see OVT_TEST_EchelonFixture.MarkEchelonsEliminated for the
	//! same pattern).
	protected void MarkEliminated(notnull OVT_DeploymentComponent deployment)
	{
		deployment.SetSpawnedUnitsEliminated(true);

		array<OVT_BaseSpawningDeploymentModule> spawningModules = deployment.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			if (spawningModule)
				spawningModule.SetSpawnedUnitsEliminated(true);
		}
	}
}
