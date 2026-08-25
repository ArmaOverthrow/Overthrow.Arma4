//------------------------------------------------------------------------------------------------
//! What stage of its journey an insertion is at.
enum OVT_EInsertionState
{
	//! Nothing decided yet. Re-entered on every convergence until an origin can be resolved, so a
	//! faction that currently holds no base is not a permanent failure - it is a retry.
	UNDECIDED,

	//! On foot. The terminal state of every path that is not a live convoy.
	WALKING,

	//! A truck is on the road with the force aboard.
	DRIVING,

	//! The force is down; the empty truck is going home.
	RETURNING,

	//! The convoy is over and everything it owned has been handed back.
	FINISHED,

	//! Arrived and STILL ABOARD. Never entered here - see OVT_MountedForceSpawningDeploymentModule.
	HOLDING
}

//------------------------------------------------------------------------------------------------
//! LIVE INSERTION: registers a deployment's force at a place it could plausibly have come FROM, puts
//! it in a truck, drives it to a landing zone short of the objective, drops it, and sends the truck
//! home.
//!
//! 🔴 The walk fallback is the spine of this file, not its error handling: the force is registered
//! first with a plan already pointing at the objective, so every way of losing the truck just means
//! it walks.
//!
//! ⚠ A live convoy needs three stamps - the crew EXISTS (RIDING_SPAWN_DISTANCE), its AI RUNS
//! (OVT_MountedGroupActivation) and the HULL is SIMULATED (HoldTruckSimulated). ReleaseConvoy hands
//! all three back, along with the truck, the crew registration, two waypoints and a convoy slot.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_InsertionSpawningDeploymentModule : OVT_InfantrySpawningDeploymentModule
{
	//! WHERE THE FORCE COMES FROM - the modder seam. Left unauthored, this module registers nothing.
	[Attribute(desc: "Where this deployment's force sets out from. Ship OVT_NearestControlledBaseSourceProvider unless a config has a better answer. REQUIRED - with none, this module registers nothing")]
	ref OVT_DeploymentSourceProvider m_Source;

	[Attribute(defvalue: "400", desc: "Below this separation between the source and the objective, no truck is spawned at all - the force simply walks. 0 disables the rule and always sends a truck")]
	float m_fWalkThresholdDistance;

	[Attribute(defvalue: "truck", desc: "Vehicle type name from the faction VEHICLE registry")]
	string m_sTruckVehicleType;

	[Attribute(defvalue: "truck_crew", desc: "Group type name from the faction GROUP registry, used to crew the transport")]
	string m_sTruckCrewGroup;

	[Attribute(defvalue: "300", desc: "How far short of the objective the transport stops, in metres. Clamped: a standoff longer than the whole journey drops the force at the source rather than behind it")]
	float m_fLZStandoffDistance;

	[Attribute(defvalue: "1", desc: "Below this ground speed in m/s the transport is not making progress")]
	float m_fStuckSpeedThreshold;

	//! ⚠ A TICK HERE IS ONE DEPLOYMENT UPDATE, i.e. roughly TEN SECONDS.
	[Attribute(defvalue: "6", desc: "Consecutive update ticks (about 10 s each) below the speed threshold before the force dismounts and walks. 0 disables the stuck test entirely")]
	int m_iStuckTicks;

	[Attribute(defvalue: "40", desc: "How close to the landing zone counts as arrived, in metres")]
	float m_fArrivalRadius;

	//! ⚠ A budgeted cost, not a receipt: charged whether or not a truck ever reaches a road.
	[Attribute(defvalue: "40", desc: "Added to this module's resource cost to cover the transport. Budgeted at creation time, so it is charged even on deployments that end up walking - EXCEPT where the source provider reports the origin has no vehicles at all, e.g. a forward operating base")]
	int m_iTruckCostOverride;

	//! What to do when the faction's convoy cap is spent. TRUE walks the force in now; FALSE leaves the
	//! insertion undecided and retries on the next convergence.
	[Attribute(defvalue: "1", desc: "When every convoy slot is taken: TRUE walks the force in immediately, FALSE waits for a slot and retries on the next update")]
	bool m_bWalkWhenInsertionRefused;

	//! THE THIRD STAMP'S OFF-SWITCH - see the class header.
	[Attribute(defvalue: "1", desc: "TRUE parks an engine observer on the transport so the ENGINE KEEPS SIMULATING IT with no player near. Without it a convoy far from every observer stands at 0 m/s with a perfectly healthy crew and falls back to the march. THE COST: every registered group inside the observer's ring materialises with its AI running as the transport passes, the whole length of its route")]
	bool m_bTransportIsObserver;

	//! HOW SLOW A TRANSPORT INSIDE THE ARRIVAL RADIUS HAS TO BE BEFORE ITS DOORS OPEN, in m/s.
	static const float ARRIVAL_SETTLE_SPEED_MS = 0.5;

	//! Where a crew is registered relative to its truck, in metres along X - the same offset the
	//! vehicle module uses, and for the same reason: close enough to board, not inside the geometry.
	static const float CREW_SPAWN_OFFSET_M = 5;

	//! Where passengers are registered relative to the truck. Further out than the crew so the two
	//! groups do not materialise inside one another.
	static const float PASSENGER_SPAWN_OFFSET_M = 9;

	//! AI spawn-budget tier for the transport crew. ⚠ Never leave a registration unstamped: an unstamped
	//! group inherits vanilla's LOW tier and is evicted first, which for a crew means an empty truck.
	static const SCR_EAISpawnImportance CREW_IMPORTANCE = SCR_EAISpawnImportance.HIGH;

	//! How many update ticks a crew is given to MATERIALISE before the insertion gives up and walks.
	static const int CREW_MATERIALISE_TICKS = 18;

	//! ALWAYS MATERIALISED, and not tunable: a dormant crew is walked away from its parked truck by the
	//! movement tick, and a dormant passenger group has nobody to seat.
	static const int RIDING_SPAWN_DISTANCE = 100000;

	//! Mirrors OVT_VirtualizationManagerComponent's own m_fDespawnHysteresis default, which is protected
	//! there with no getter.
	static const float DESPAWN_HYSTERESIS = 1.15;

	//! Appended to the module name to key the CREW's registration. ⚠ It must differ from the passengers'
	//! key, or the base class reclaims the crew as part of the force.
	static const string CREW_KEY_SUFFIX = "crew";

	//! Separates the deployment-scoped part of a crew key from the per-insertion serial appended to it.
	//! See GetCrewOwnerKey() for why the serial exists at all.
	static const string CREW_INSTANCE_MARK = "i";

	//! How many update ticks an empty truck gets to reach home before it is released where it stands.
	static const int RETURN_TIMEOUT_TICKS = 60;

	//! How many update ticks a truck this module ABANDONED is left standing before it is collected.
	static const int ABANDONED_TRUCK_TIMEOUT_TICKS = RETURN_TIMEOUT_TICKS * 2;

	//! How many update ticks a transport ABANDONED BY A FAILED DRIVE is left standing before it is
	//! collected, once nobody can see it go.
	static const int STUCK_TRUCK_TIMEOUT_TICKS = 1;

	//! How close a live player has to be for an abandoned truck to be left exactly where it is.
	static const int ABANDONED_TRUCK_PLAYER_RADIUS_M = 320;

	//! How far the source may be from a base marker for that base's authored vehicle spawns to count as
	//! ITS spawns. 250 m is OVT_DeploymentManagerComponent.BASE_CLASSIFICATION_RADIUS.
	static const float SOURCE_BASE_RADIUS_M = 250;

	//! HOW CLOSE TO ITS OWN SPAWN A STALLED TRANSPORT COUNTS AS "never left", in metres.
	static const float ABANDONED_AT_SPAWN_RADIUS_M = 30;

	//! How much room a transport needs at an authored marker before that marker counts as free. Wider
	//! than the vehicle manager's car-sized 3 m default; these markers are shared with vehicle patrols.
	static const float MARKER_CLEARANCE_M = 6;

	//! How long the fallback march plan pauses at the objective before walking to it again.
	static const float MARCH_HOLD_SECONDS = 3600;

	//------------------------------------------------------------------------------------------------
	// RUNTIME STATE - none of it persisted, all of it re-derived. See the class header.
	//------------------------------------------------------------------------------------------------

	protected OVT_EInsertionState m_eState;

	//! Where the force sets out from, resolved once through m_Source and then reused for the whole
	//! insertion so a base changing hands mid-drive cannot move the landing zone under the convoy.
	protected vector m_vSource;

	//! Where the truck is going. Decided before anything is registered, because the stuck fallback and
	//! the arrival test both measure against it.
	protected vector m_vLZ;

	//! WHERE THE TRANSPORT GOES HOME TO: the exact spot it was spawned on, not the base.
	protected vector m_vHome;

	protected Vehicle m_Truck;

	protected int m_iCrewHandle;

	//! How long this transport's driver has been holding the horn, in milliseconds. See
	//! OVT_VehicleHornWatchdog - the engine presses it and sometimes never writes it back.
	protected int m_iHornHeldMs;

	//! THIS INSERTION'S OWN crew owner key, minted once on first use and never recomputed. See
	//! GetCrewOwnerKey() - the whole point is that no other insertion, ever, can compose this string.
	protected string m_sCrewOwnerKey;

	//! Session-wide serial handed to crew keys, one per insertion that ever asks for one.
	static int s_iCrewKeySerial;

	protected bool m_bReserved;

	protected int m_iStuckTicksElapsed;

	//! Consecutive update ticks the transport has been INSIDE the arrival radius without having settled
	//! enough to open its doors. Reset the moment it is outside again.
	protected int m_iInsideRadiusTicks;

	//! Consecutive update ticks the transport has been standing on the road WITH NOBODY DRIVING IT.
	protected int m_iUncrewedTicksElapsed;

	//! Consecutive update ticks the transport's crew has had NO MEN IN IT AT ALL.
	protected int m_iUnmaterialisedTicksElapsed;

	protected vector m_vLastTruckPosition;

	//! False until the truck has been observed once, so the first tick does not measure a speed against
	//! a zero vector.
	protected bool m_bHaveLastTruckPosition;

	protected int m_iReturnTicksElapsed;

	//! Whether m_Truck is a transport this module walked away from and is only holding until it can be
	//! collected. Set at exactly one place - ReleaseConvoy() with deleteTruck false.
	protected bool m_bTruckAbandoned;

	//! Update ticks since the abandonment, counting up to STUCK_TRUCK_TIMEOUT_TICKS.
	protected int m_iAbandonedTicksElapsed;

	//! Latches the one VERBOSE line explaining that an overdue transport is being kept because a player
	//! is standing near it.
	protected bool m_bAbandonedHoldLogged;

	//! Every waypoint this module spawned. AIGroup.AddWaypoint() does NOT take ownership, so these are
	//! deleted by hand on every exit. Entity handles null themselves out when the engine deletes one.
	protected ref array<AIWaypoint> m_aOwnedWaypoints;

	//! Group ENTITY id -> whether that group is the CREW (true) or a PASSENGER (false). Keyed on the
	//! entity because that is all the engine's per-member callback gives us.
	protected ref map<ref EntityID, bool> m_mRiderIsCrew;

	//! Latches the "nowhere to come from" warning so a faction that has lost every base does not fill
	//! the log at one line per module per update.
	protected bool m_bSourceWarned;


	//------------------------------------------------------------------------------------------------
	void OVT_InsertionSpawningDeploymentModule()
	{
		m_eState = OVT_EInsertionState.UNDECIDED;
		m_iCrewHandle = -1;
		m_aOwnedWaypoints = new array<AIWaypoint>();
		m_mRiderIsCrew = new map<ref EntityID, bool>();
	}

	//------------------------------------------------------------------------------------------------
	//! The force, plus the transport it is budgeted to arrive in - UNLESS ITS ORIGIN HAS NO VEHICLES.
	override int GetResourceCost()
	{
		if (m_Source && !m_Source.MayProvideTransport())
			return super.GetResourceCost();

		return super.GetResourceCost() + m_iTruckCostOverride;
	}

	//------------------------------------------------------------------------------------------------
	// The convergence
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Brings this insertion up to what it wants: a decision, a transport if one is warranted, and the
	//! force itself. ALWAYS SAFE TO CALL, any number of times, in any order.
	override void EnsureGroups()
	{
		if (!m_ParentDeployment)
			return;

		if (m_eState == OVT_EInsertionState.UNDECIDED)
		{
			DecideInsertion();

			// ⚠ Still undecided means register NOTHING. Falling through would put the force on the ground AT
			// ITS SOURCE with a march plan - which is exactly the decision not yet made.
			if (m_eState == OVT_EInsertionState.UNDECIDED)
				return;
		}

		if (m_eState == OVT_EInsertionState.DRIVING)
			EnsureConvoy();

		super.EnsureGroups();

		if (m_eState == OVT_EInsertionState.DRIVING)
		{
			// ⚠ The only seating sweep left, and it is safe because this method is not periodic - see
			// BoardEveryone().
			BoardEveryone();

			// Every path that seats also pins, so the two can never drift apart. Reached on the
			// records-restored fan-out as well as on activation, and idempotent on both.
			HoldRidersActive();
			return;
		}

		// Everything that is not riding belongs on the ordinary proximity ring. Unconditional rather than
		// latched: one missed latch clear is a squad materialised for the rest of the campaign.
		DropPassengersToGlobalRing();
	}

	//------------------------------------------------------------------------------------------------
	//! Chooses between a march and a convoy, once, and commits to a source and a landing zone.
	protected void DecideInsertion()
	{
		if (!EnsureSourceResolved())
			return;

		// A convoy is state that lives only in this session's memory: a truck, a crew, two waypoints
		// and a reservation. None of it is in the save, so there is nothing to resume - see the class
		// header.
		if (m_ParentDeployment.WasRestoredFromSave())
		{
			EnterWalking("it came back from a save point, and a convoy is never resumed across a load");
			return;
		}

		vector target = m_ParentDeployment.GetPosition();
		float separation = vector.Distance(m_vSource, target);

		if (OVT_InsertionGeometry.ShouldWalk(separation, m_fWalkThresholdDistance))
		{
			int separationMetres = Math.Round(separation);
			int thresholdMetres = Math.Round(m_fWalkThresholdDistance);

			EnterWalking(string.Format("the objective is %1 m away, inside the %2 m walk threshold",
				separationMetres.ToString(), thresholdMetres.ToString()));
			return;
		}

		// ⚠ The sixth way to end up walking, and the only one about the PLACE. Asked AFTER the threshold and
		// BEFORE the convoy slot, so an origin with no vehicles never holds a reservation for a march.
		if (m_Source && !m_Source.SourceProvidesTransport(m_vSource, m_ParentDeployment.GetControllingFaction()))
		{
			EnterWalking(string.Format("its origin (%1) has no transport to give it", m_Source.GetProviderName()));
			return;
		}

		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
		{
			EnterWalking("the deployment framework could not be resolved, so no convoy slot could be claimed");
			return;
		}

		if (!manager.TryReserveInsertion(m_ParentDeployment.GetControllingFaction()))
		{
			if (m_bWalkWhenInsertionRefused)
			{
				EnterWalking(string.Format("all %1 of the faction's convoy slots are taken",
					manager.GetMaxConcurrentInsertions().ToString()));
				return;
			}

			// The config would rather wait for a truck than arrive on foot. Nothing is registered this
			// pass and the next convergence asks again.
			return;
		}

		m_bReserved = true;

		// Decided BEFORE anything is registered, so a config with an impossible standoff fails here
		// rather than three kilometres down a road, and so the stuck fallback has somewhere to measure
		// against from the very first tick.
		m_vLZ = ResolveLandingZone(m_vSource, target);

		m_eState = OVT_EInsertionState.DRIVING;
		m_iStuckTicksElapsed = 0;
		m_iUncrewedTicksElapsed = 0;
		m_iUnmaterialisedTicksElapsed = 0;
		m_iInsideRadiusTicks = 0;
		m_bHaveLastTruckPosition = false;
		m_iReturnTicksElapsed = 0;

		// ⚠ The distance quoted is the DRIVE, not the separation - the landing zone sits
		// m_fLZStandoffDistance short of the objective.
		int driveMetres = Math.Round(vector.Distance(m_vSource, m_vLZ));
		int standoffMetres = Math.Round(vector.Distance(m_vLZ, target));

		OVT_DeploymentLog.Debug(string.Format("[Overthrow] Insertion '%1': driving %2 m from %3 to a landing zone at %4, %5 m short of the objective",
			DescribeSelf(), driveMetres.ToString(), m_vSource.ToString(), m_vLZ.ToString(),
			standoffMetres.ToString()));
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves and caches the origin. Never overwrites one that is already set: an insertion that has
	//! begun keeps its source even if the base it came from changes hands mid-drive.
	//! \return True when there is an origin to work from.
	protected bool EnsureSourceResolved()
	{
		if (m_vSource != vector.Zero)
			return true;

		if (!m_ParentDeployment)
			return false;

		if (!m_Source)
		{
			WarnNoSource("no source provider is authored on it");
			return false;
		}

		vector resolved;
		if (!m_Source.ResolveSource(m_ParentDeployment.GetPosition(), m_ParentDeployment.GetControllingFaction(), resolved))
		{
			WarnNoSource(string.Format("its source provider (%1) found nowhere for the force to come from", m_Source.GetProviderName()));
			return false;
		}

		m_vSource = resolved;
		m_bSourceWarned = false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Says once, and then stops saying, that this insertion has nowhere to start from.
	//! \param[in] reason Why there is no origin.
	protected void WarnNoSource(string reason)
	{
		if (m_bSourceWarned)
			return;

		m_bSourceWarned = true;

		Print(string.Format("[Overthrow] Insertion '%1' will register nothing: %2. Nothing is spawned from thin air; this is retried on every update",
			DescribeSelf(), reason), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! The landing zone: a point on the source->objective line, snapped onto a road where there is one.
	//! \param[in] source Where the convoy starts.
	//! \param[in] target The objective.
	//! \return A landing zone at ground height.
	protected vector ResolveLandingZone(vector source, vector target)
	{
		vector point = OVT_InsertionGeometry.LZPointOnLine(source, target, m_fLZStandoffDistance);

		vector roadPosition;
		vector roadAngles;
		if (OVT_WorldUtils.FindNearestRoadSpawn(point, OVT_WorldUtils.ROAD_SPAWN_MAX_DISTANCE, roadPosition, roadAngles))
		{
			// ⚠ A ROAD IS NOT AUTOMATICALLY A LANDING ZONE. The search runs 200 m in every direction
			// and does not care which way it moves the point - see IsAcceptableLZ.
			if (OVT_InsertionGeometry.IsAcceptableLZ(roadPosition, target, m_fLZStandoffDistance))
				return roadPosition;

			OVT_DeploymentLog.Debug(string.Format("[Overthrow] Insertion '%1': the nearest road to its landing zone is %2 m from the objective, inside the %3 m standoff - dropping off the road instead",
				DescribeSelf(), Math.Round(vector.Distance(roadPosition, target)).ToString(),
				Math.Round(m_fLZStandoffDistance).ToString()));
		}

		// The geometry interpolates Y between two endpoints and knows nothing about the ground between
		// them; over 300 m of hillside that is metres out, and a landing zone under the terrain is one
		// a truck can never arrive at.
		BaseWorld world = GetGame().GetWorld();
		if (world)
			point[1] = world.GetSurfaceY(point[0], point[2]);

		return point;
	}

	//------------------------------------------------------------------------------------------------
	// The convoy
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Brings the transport up to what a drive needs: a truck, a crew, and an order to go.
	protected void EnsureConvoy()
	{
		if (m_bSpawnedUnitsEliminated || m_ParentDeployment.GetSpawnedUnitsEliminated())
		{
			// There is nobody left to carry. This is not a fallback - walking a force that does not
			// exist is meaningless - so the convoy is simply wound up.
			ReleaseConvoy("the force it was carrying has been wiped out", true);
			m_eState = OVT_EInsertionState.FINISHED;
			return;
		}

		if (!m_Truck && !SpawnTruck())
		{
			FallBackToWalking("no transport could be put on the road");
			return;
		}

		// ⚠ HERE rather than inside SpawnTruck(): OVT_MountedForceSpawningDeploymentModule overrides that
		// method and assigns m_Truck itself, so this is the one place both routes pass through.
		HoldTruckSimulated();

		if (m_iCrewHandle == -1 && !EnsureCrew())
		{
			FallBackToWalking("no crew could be registered for the transport");
			return;
		}

		IssueDriveOrder();
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the truck in the world at the source.
	//! \return True when there is a truck.
	protected bool SpawnTruck()
	{
		if (m_sTruckVehicleType.IsEmpty())
		{
			Print(string.Format("[Overthrow] Insertion '%1' has no transport type authored", DescribeSelf()), LogLevel.WARNING);
			return false;
		}

		ResourceName prefab = GetVehiclePrefabFromFaction(m_ParentDeployment.GetControllingFaction());
		if (prefab.IsEmpty())
			return false;

		vector spawnPosition;
		vector spawnAngles;
		ResolveTruckSpawn(spawnPosition, spawnAngles);

		m_Truck = Vehicle.Cast(SpawnEntity(prefab, spawnPosition, spawnAngles));
		if (!m_Truck)
		{
			Print(string.Format("[Overthrow] Insertion '%1': transport '%2' failed to spawn at %3",
				DescribeSelf(), m_sTruckVehicleType, spawnPosition.ToString()), LogLevel.WARNING);
			return false;
		}

		// Recorded HERE rather than re-derived when the truck turns for home: by then the marker it left
		// from may well be occupied, and ResolveAuthoredTruckSpawn() would hand back a different spot.
		m_vHome = spawnPosition;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! WHERE THE TRUCK APPEARS: an authored marker at the source base if there is a free one, and the
	//! nearest road if there is not.
	//! \param[out] position Where to put the truck.
	//! \param[out] angles How to point it.
	protected void ResolveTruckSpawn(out vector position, out vector angles)
	{
		if (ResolveAuthoredTruckSpawn(position, angles))
			return;

		position = m_vSource;
		angles = vector.Zero;

		vector roadPosition;
		vector roadAngles;
		if (OVT_WorldUtils.FindNearestRoadSpawn(m_vSource, OVT_WorldUtils.ROAD_SPAWN_MAX_DISTANCE, roadPosition, roadAngles))
		{
			position = roadPosition;
			angles = roadAngles;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The authored answer, when the source base has one that is free.
	//! \param[out] position The chosen marker's position.
	//! \param[out] angles The chosen marker's OWN facing - not the road's, not zero.
	//! \return True when a free authored marker was found and written out.
	protected bool ResolveAuthoredTruckSpawn(out vector position, out vector angles)
	{
		OVT_BaseControllerComponent baseController = OVT_BaseControllerComponent.FindNearestBaseControllerWithin(m_vSource, SOURCE_BASE_RADIUS_M);
		if (!baseController)
			return false;

		array<IEntity> markers = {};
		baseController.CollectVehiclePatrolSpawns(markers);

		if (markers.IsEmpty())
			return false;

		array<vector> positions = {};
		array<bool> blocked = {};

		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();

		foreach (IEntity marker : markers)
		{
			vector markerPosition = marker.GetOrigin();
			positions.Insert(markerPosition);

			// No vehicle manager is not a reason to refuse an authored spot - it is a reason not to know
			// whether it is taken. ChooseSpawnMarker treats an unanswered index as free.
			if (!vehicles)
				continue;

			blocked.Insert(vehicles.IsSpotBlockedByVehicle(markerPosition, MARKER_CLEARANCE_M));
		}

		int chosen = OVT_InsertionGeometry.ChooseSpawnMarker(positions, blocked, m_vSource);
		if (chosen == -1)
			return false;

		position = positions[chosen];

		// ⚠ YAW ONLY. SpawnEntity()'s rotation is in Math3D.AnglesToMatrix order "(yaw, pitch, roll)", NOT
		// the "(pitch, yaw, roll)" GetAngles() returns - pass it straight through and the truck spawns on
		// its nose.
		angles = GetUprightSpawnRotation(markers[chosen].GetYawPitchRoll()[0]);

		OVT_DeploymentLog.Debug(string.Format("[Overthrow] Insertion '%1': transport spawning on authored vehicle spawn %2 of %3 at %4",
			DescribeSelf(), (chosen + 1).ToString(), markers.Count().ToString(), position.ToString()));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Registers the transport crew under its OWN owner key, beside the truck, always materialised AND
	//! always running.
	//! \return True when there is a crew.
	protected bool EnsureCrew()
	{
		if (m_sTruckCrewGroup.IsEmpty())
		{
			Print(string.Format("[Overthrow] Insertion '%1' has no transport crew type authored", DescribeSelf()), LogLevel.WARNING);
			return false;
		}

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return false;

		string crewKey = GetCrewOwnerKey();
		if (crewKey.IsEmpty())
			return false;

		// Reclaim before registering, exactly as everything else in this framework does: a convergence
		// that ran twice, or one that follows a records-restored fan-out, must not put a second crew in
		// a truck that already has one.
		array<int> found = virtualization.FindGroupsByOwner(OWNER_SYSTEM, crewKey);
		foreach (int foundHandle : found)
		{
			if (!virtualization.IsRegistered(foundHandle))
				continue;

			// ⚠ Never adopt a crew that can never be repopulated: a group with no men whose refill seam reports
			// COMPLETE is a husk - the spawn queue books every request against it as satisfied and drops it.
			if (IsCrewHusk(virtualization, foundHandle))
			{
				Print(string.Format("[Overthrow] Insertion '%1': crew handle %2 has no men and cannot be refilled - unregistering it and crewing the transport fresh",
					DescribeSelf(), foundHandle.ToString()), LogLevel.WARNING);

				virtualization.UnregisterGroup(foundHandle);
				continue;
			}

			m_iCrewHandle = foundHandle;
			break;
		}

		if (m_iCrewHandle == -1)
		{
			string factionKey = ResolveFactionKey(m_ParentDeployment.GetControllingFaction());
			if (factionKey.IsEmpty())
			{
				Print(string.Format("[Overthrow] Insertion '%1': faction index %2 resolves to no faction key, cannot crew the transport",
					DescribeSelf(), m_ParentDeployment.GetControllingFaction().ToString()), LogLevel.WARNING);
				return false;
			}

			vector crewPosition = m_Truck.GetOrigin() + Vector(CREW_SPAWN_OFFSET_M, 0, 0);

			// 🔴 THE CREW GETS THE MARCH PLAN TOO, and it used to get null. While the crew is riding the
			// plan is inert - the movement tick skips a spawned group, and a crew on the riding ring is
			// always spawned - so this changes nothing about a drive. It matters in exactly one place:
			// AFTER A LOAD.
			//
			// A convoy is never resumed across a load (D9), and the documented consequence was "a
			// restored mounted deployment WALKS". That was only ever true because the force was
			// PASSENGERS, which are re-registered with this plan. Since the mounted configs became
			// crew-only (2026-08-24) there are no passengers, so a restored deployment had nothing that
			// could walk: the crew came back with a null plan and stood in the road beside an abandoned
			// truck, forever, blocking it (author play-test, same day).
			m_iCrewHandle = virtualization.RegisterGroup(OWNER_SYSTEM, crewKey, factionKey, m_sTruckCrewGroup,
				crewPosition, ResolveVirtualPlan(crewPosition), RIDING_SPAWN_DISTANCE, CREW_IMPORTANCE);

			if (m_iCrewHandle == -1)
			{
				Print(string.Format("[Overthrow] Insertion '%1': registration of transport crew '%2' (%3) was refused",
					DescribeSelf(), m_sTruckCrewGroup, factionKey), LogLevel.WARNING);
				return false;
			}
		}

		SCR_AIGroup crew = virtualization.GetGroup(m_iCrewHandle);
		if (crew)
		{
			TagForGameMaster(crew);
			PairRider(crew, true);

			// Whoever is already standing. Everyone who arrives after this is pinned by
			// OnRiderAgentAdded as the spawn queue produces him, and the whole crew is re-pinned on
			// every drive tick - a crew fills PROGRESSIVELY, so one pass over it is never enough.
			// No-op while the transport is an observer - see HoldRidersActive().
			if (!m_bTransportIsObserver)
				OVT_MountedGroupActivation.HoldGroupActive(crew);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! IS THIS REGISTERED CREW AN UNREPOPULATABLE CORPSE?
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] handle A registered crew handle.
	//! \return True when the handle names a group that will never have men in it again.
	protected bool IsCrewHusk(notnull OVT_VirtualizationManagerComponent virtualization, int handle)
	{
		SCR_AIGroup group = virtualization.GetGroup(handle);
		if (!group)
			return true;

		if (group.GetAgentsCount() > 0)
			return false;

		return group.IsExpandComplete();
	}

	//------------------------------------------------------------------------------------------------
	//! Gives the crew a MOVE order to the landing zone, once.
	protected void IssueDriveOrder()
	{
		if (!m_aOwnedWaypoints.IsEmpty())
			return;

		IssueCrewMove(m_vLZ);
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces whatever the crew was doing with a single MOVE.
	//! \param[in] destination Where the truck should go.
	protected void IssueCrewMove(vector destination)
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		SCR_AIGroup crew = virtualization.GetGroup(m_iCrewHandle);
		if (!crew)
			return;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return;

		ClearOwnedWaypoints(crew);

		AIWaypoint waypoint = config.SpawnMoveWaypoint(destination);
		if (!waypoint)
		{
			Print(string.Format("[Overthrow] Insertion '%1': a move waypoint could not be spawned, so the transport has no orders",
				DescribeSelf()), LogLevel.WARNING);
			return;
		}

		m_aOwnedWaypoints.Insert(waypoint);
		crew.AddWaypoint(waypoint);
	}

	//------------------------------------------------------------------------------------------------
	//! Takes this module's waypoints off a group and deletes them.
	//! \param[in] crew The group holding them; null is legal (the group may already be gone).
	protected void ClearOwnedWaypoints(SCR_AIGroup crew)
	{
		foreach (AIWaypoint waypoint : m_aOwnedWaypoints)
		{
			if (!waypoint)
				continue;

			if (crew)
				crew.RemoveWaypoint(waypoint);

			SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
		}

		m_aOwnedWaypoints.Clear();
	}

	//------------------------------------------------------------------------------------------------
	// The drive
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	override void OnUpdate(int deltaTime)
	{
		super.OnUpdate(deltaTime);

		// ⚠ BEFORE THE STATE DISPATCH, BECAUSE EVERY BRANCH BELOW RETURNS. A truck is only ever abandoned
		// in WALKING or FINISHED - states the dispatch does nothing for - so a sweep placed inside it
		// would never run at all. It is a no-op on every other path (the flag is false).
		TickAbandonedTruck();

		// ⚠ BEFORE THE STATE DISPATCH, FOR THE SAME REASON: every branch below returns, and a latched
		// horn is not confined to the drive. A mounted force that has arrived sits in HOLDING, which is
		// where one of the two recorded latches happened.
		m_iHornHeldMs = OVT_VehicleHornWatchdog.Tick(ResolveDriverCharacter(), deltaTime, m_iHornHeldMs);

		// ⚠ The retry, and the only place it can live: EnsureGroups() is called once, at activation, so an
		// insertion that could not resolve an origin on that pass would register nothing ever again.
		if (m_eState == OVT_EInsertionState.UNDECIDED)
		{
			EnsureGroups();
			return;
		}

		if (m_eState == OVT_EInsertionState.DRIVING)
		{
			TickDrive(deltaTime);
			return;
		}

		if (m_eState == OVT_EInsertionState.RETURNING)
			TickReturn(deltaTime);
	}

	//------------------------------------------------------------------------------------------------
	//! One observation of a convoy on its way out.
	//! \param[in] deltaTime Milliseconds since the last update of this module.
	protected void TickDrive(int deltaTime)
	{
		if (!m_Truck || !IsTruckOperational())
		{
			DismountAndWalk("its transport was destroyed");
			return;
		}

		if (!IsCrewAlive())
		{
			DismountAndWalk("its transport lost its crew");
			return;
		}

		// ⚠ BEFORE ANY TEST, EVERY TICK. The pin is a re-assert, and it has to land before the stall
		// accounting or a crewman who arrived this tick reads as "not driving".
		HoldRidersActive();

		// And the gate BEFORE the pin: men who do not exist cannot be held awake. A no-op on every tick
		// of every convoy that has a crew - see NudgeCrewMaterialisation for the pop-in clause it exists
		// to get past.
		NudgeCrewMaterialisation();

		vector truckPosition = m_Truck.GetOrigin();
		float distanceToLZ = vector.Distance(truckPosition, m_vLZ);

		// ⚠ The first observation is not a measurement and must not cost a stall: there is no previous
		// position to compare against, and with a stall limit as low as three a free stall on a boarding
		// tick is a quarter of the budget.
		float speed = 0;
		if (m_bHaveLastTruckPosition)
			speed = OVT_InsertionGeometry.SpeedFromTravel(m_vLastTruckPosition, truckPosition, deltaTime / 1000.0);

		// ⚠ Everything inside the radius is the arrival path's business. This branch RETURNS, so the stall
		// test below never sees a SETTLING truck; both ways out of here are CompleteInsertion().
		if (OVT_InsertionGeometry.IsInsideArrivalRadius(distanceToLZ, m_fArrivalRadius))
		{
			m_iInsideRadiusTicks = m_iInsideRadiusTicks + 1;

			// ⚠ The LIVE reading, not the tick average, and only here: `speed` averages the braking tick with
			// road speed. The stall test below keeps `speed` - an average is what spinning wheels cannot fool.
			if (OVT_InsertionGeometry.HasArrived(distanceToLZ, m_fArrivalRadius, TruckGroundSpeed(speed), ARRIVAL_SETTLE_SPEED_MS))
			{
				CompleteInsertion();
				return;
			}

			if (OVT_InsertionGeometry.IsSettleGraceExpired(m_iInsideRadiusTicks, m_iStuckTicks))
			{
				int settleTicks = m_iInsideRadiusTicks;

				OVT_DeploymentLog.Debug(string.Format("[Overthrow] Insertion '%1': its transport reached the landing zone but never came to a stop in %2 update(s); dropping the force anyway",
					DescribeSelf(), settleTicks.ToString()));

				CompleteInsertion();
				return;
			}

			// ⚠ Still braking - and what keeps everyone aboard is this `return`, not a seating sweep. Nothing
			// here takes anybody out until CompleteInsertion().
			m_vLastTruckPosition = truckPosition;
			m_bHaveLastTruckPosition = true;

			return;
		}

		m_iInsideRadiusTicks = 0;

		// ==========================================================================================
		// ⚠ THE STALL CLOCK ONLY RUNS WHILE THERE IS SOMEBODY TO STALL - see m_iUncrewedTicksElapsed.
		// ==========================================================================================
		if (!CrewIsAtTheWheel())
		{
			m_iStuckTicksElapsed = 0;

			// 🔴 Two clocks, and which one runs depends on whether there is anybody there at all: an empty
			// crew is the ordinary state of a new group, not a transport that failed to get a driver.
			if (CrewMaterialisedCount() == 0)
			{
				m_iUnmaterialisedTicksElapsed = m_iUnmaterialisedTicksElapsed + 1;

				// ⚠ One line per tick, on purpose: a single line at the end of the window cannot tell a slow
				// queue from a count that was flat at zero. Bounded by CREW_MATERIALISE_TICKS.
				OVT_DeploymentLog.Debug(string.Format("[Overthrow] Insertion '%1': waiting for its crew, update %2 of %3 - %4",
					DescribeSelf(), m_iUnmaterialisedTicksElapsed.ToString(), ResolveMaterialiseTicks().ToString(),
					DescribeCrewFill()));

				if (OVT_InsertionGeometry.IsUncrewedGraceExpired(m_iUnmaterialisedTicksElapsed, ResolveMaterialiseTicks()))
				{
					Print(string.Format("[Overthrow] Insertion '%1': its crew never materialised in %2 update(s) - %3",
						DescribeSelf(), m_iUnmaterialisedTicksElapsed.ToString(), DescribeCrewLiveness()), LogLevel.WARNING);

					DismountAndWalk("its transport's crew never materialised");
					return;
				}
			}
			else
			{
				m_iUncrewedTicksElapsed = m_iUncrewedTicksElapsed + 1;

				// Bounded by the same budget, so a crew that turns up and will not board is written off in
				// about a minute and walks. A disabled stall budget disables this too.
				if (OVT_InsertionGeometry.IsUncrewedGraceExpired(m_iUncrewedTicksElapsed, m_iStuckTicks))
				{
					OVT_DeploymentLog.Debug(string.Format("[Overthrow] Insertion '%1': its crew is on the ground but nobody took the wheel in %2 update(s) - %3",
						DescribeSelf(), m_iUncrewedTicksElapsed.ToString(), DescribeCrewLiveness()));

					DismountAndWalk("its transport never got a driver");
					return;
				}
			}

			// Keep the observation fresh so the tick the crew DOES board is measured against where the
			// truck is now, not wherever it was when it was last driven.
			m_vLastTruckPosition = truckPosition;
			m_bHaveLastTruckPosition = true;

			return;
		}

		m_iUncrewedTicksElapsed = 0;
		m_iUnmaterialisedTicksElapsed = 0;

		if (m_bHaveLastTruckPosition)
		{
			m_iStuckTicksElapsed = OVT_InsertionGeometry.AdvanceStuckTicks(speed, m_fStuckSpeedThreshold, m_iStuckTicksElapsed);


			if (OVT_InsertionGeometry.IsStuck(speed, m_fStuckSpeedThreshold, m_iStuckTicksElapsed, m_iStuckTicks, distanceToLZ, m_fArrivalRadius))
			{
				int shortfallMetres = Math.Round(distanceToLZ);

				// ⚠ The crew's state goes in the log on its own line, always - "never left its spawn point" is
				// equally true of an unmaterialised crew, a sleeping one and a truck wedged against a wall.
				OVT_DeploymentLog.Debug(string.Format("[Overthrow] Insertion '%1': its transport stalled at %2 - %3",
					DescribeSelf(), truckPosition.ToString(), DescribeCrewLiveness()));

				DismountAndWalk(string.Format("its transport stopped making progress %1 m short of the landing zone",
					shortfallMetres.ToString()));
				return;
			}
		}

		m_vLastTruckPosition = truckPosition;
		m_bHaveLastTruckPosition = true;

		// The only thing this module still polices on a moving truck: a member of the FORCE at the
		// wheel. Never anybody on foot - see EvictHijackers().
		EvictHijackers();
	}

	//------------------------------------------------------------------------------------------------
	//! HOW FAST THE TRANSPORT IS GOING RIGHT NOW, in m/s - a speedometer, not a tick average.
	//! \param[in] fallbackSpeed The tick-average speed to use when the transport has no physics.
	//! \return Metres per second.
	protected float TruckGroundSpeed(float fallbackSpeed)
	{
		if (!m_Truck)
			return fallbackSpeed;

		Physics physics = m_Truck.GetPhysics();
		if (!physics)
			return fallbackSpeed;

		return physics.GetVelocity().Length();
	}

	//------------------------------------------------------------------------------------------------
	// The three liveness stamps - see RIDING_SPAWN_DISTANCE, OVT_MountedGroupActivation and the hull
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Keeps the transport CREW out of max LOD, so its behaviour tree keeps running whatever the
	//! distance to the nearest observer.
	//!
	//! ⚠ SUPERSEDED BY THE OBSERVER, AND ONLY A FALLBACK NOW (2026-08-24). HoldTruckSimulated() parks an
	//! engine observer ON THE TRANSPORT, and the LOD system is driven by observer distance - so the crew
	//! of an observing transport is permanently at distance ~0 and can never reach max LOD on its own.
	//! Pinning on top of that bought nothing and yanked every crewman's LOD every tick, which is a prime
	//! suspect for the latched AI horn (the native driving code writes the horn input and nothing in
	//! script can write it back). The pin therefore now runs ONLY when an author has switched the
	//! observer off, which is the one configuration where it is still load-bearing.
	protected void HoldRidersActive()
	{
		if (m_bTransportIsObserver)
			return;

		if (m_iCrewHandle == -1)
			return;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		OVT_MountedGroupActivation.HoldGroupActive(virtualization.GetGroup(m_iCrewHandle));
	}

	//------------------------------------------------------------------------------------------------
	//! ASKS THE ENGINE, DIRECTLY, FOR A CREW THAT IS REGISTERED AND HAS NOBODY IN IT.
	protected void NudgeCrewMaterialisation()
	{
		if (m_iCrewHandle == -1)
			return;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		if (!virtualization.IsRegistered(m_iCrewHandle))
			return;

		SCR_AIGroup crew = virtualization.GetGroup(m_iCrewHandle);
		if (!crew || crew.GetAgentsCount() > 0)
			return;

		virtualization.ForceSpawn(m_iCrewHandle);
	}

	//------------------------------------------------------------------------------------------------
	//! Hands EVERY rider back to the LOD system - the crew and the force alike, whatever this module
	//! thought their roles were.
	protected void ReleaseRidersActive()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		if (m_iCrewHandle != -1)
			OVT_MountedGroupActivation.ReleaseGroupActive(virtualization.GetGroup(m_iCrewHandle));

		foreach (int handle : m_aHandles)
		{
			OVT_MountedGroupActivation.ReleaseGroupActive(virtualization.GetGroup(handle));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Keeps the TRANSPORT ITSELF simulated, so the throttle its driver is holding down does something.
	protected void HoldTruckSimulated()
	{
		if (!m_bTransportIsObserver)
			return;

		if (!m_Truck)
			return;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		virtualization.AddEntityObserver(m_Truck);
	}

	//------------------------------------------------------------------------------------------------
	//! IS ANYBODY DRIVING THIS THING: a crewman with his AI running, in THIS transport's PILOT seat.
	//! \return True when the transport has a working driver in its driver's seat.
	//------------------------------------------------------------------------------------------------
	//! \return The character in the transport's pilot seat, or null when nobody is driving it.
	protected IEntity ResolveDriverCharacter()
	{
		if (!m_Truck || m_iCrewHandle == -1)
			return null;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return null;

		SCR_AIGroup crew = virtualization.GetGroup(m_iCrewHandle);
		if (!crew)
			return null;

		array<AIAgent> agents = {};
		crew.GetAgents(agents);

		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity character = agent.GetControlledEntity();
			if (!character)
				continue;

			CompartmentAccessComponent access = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
			if (!access)
				continue;

			BaseCompartmentSlot slot = access.GetCompartment();
			if (!slot || !PilotCompartmentSlot.Cast(slot))
				continue;

			if (slot.GetVehicle() != m_Truck)
				continue;

			return character;
		}

		return null;
	}

	protected bool CrewIsAtTheWheel()
	{
		if (!m_Truck)
			return false;

		if (m_iCrewHandle == -1)
			return false;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return false;

		SCR_AIGroup crew = virtualization.GetGroup(m_iCrewHandle);
		if (!crew)
			return false;

		array<AIAgent> agents = {};
		crew.GetAgents(agents);

		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			if (!agent.IsAIActivated())
				continue;

			IEntity character = agent.GetControlledEntity();
			if (!character)
				continue;

			CompartmentAccessComponent access = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
			if (!access)
				continue;

			BaseCompartmentSlot slot = access.GetCompartment();
			if (!slot)
				continue;

			if (!PilotCompartmentSlot.Cast(slot))
				continue;

			if (slot.GetVehicle() != m_Truck)
				continue;

			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The materialisation deadline AS IT APPLIES TO THIS CONFIG.
	//! \return CREW_MATERIALISE_TICKS, or 0 when this config has disabled giving up altogether.
	protected int ResolveMaterialiseTicks()
	{
		if (m_iStuckTicks <= 0)
			return 0;

		return CREW_MATERIALISE_TICKS;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many of this crew's men are standing in the world right now.
	protected int CrewMaterialisedCount()
	{
		if (m_iCrewHandle == -1)
			return 0;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return 0;

		return OVT_MountedGroupActivation.MaterialisedCount(virtualization.GetGroup(m_iCrewHandle));
	}

	//------------------------------------------------------------------------------------------------
	//! THE PER-TICK TREND LINE: how full the cab is right now and what the engine's spawn queue has done
	//! about it, short enough to print once every ten seconds without burying the log.
	//! \return A compact fill state.
	protected string DescribeCrewFill()
	{
		if (m_iCrewHandle == -1)
			return "no crew is registered";

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return "the virtualization manager could not be resolved";

		SCR_AIGroup crew = virtualization.GetGroup(m_iCrewHandle);
		if (!crew)
			return string.Format("crew handle %1 has no group entity", m_iCrewHandle.ToString());

		return string.Format("crew handle %1: %2 of %3 materialised, %4 alive in the mask; %5",
			m_iCrewHandle.ToString(),
			OVT_MountedGroupActivation.MaterialisedCount(crew).ToString(),
			virtualization.GetMemberCount(m_iCrewHandle).ToString(),
			virtualization.GetAliveMemberCount(m_iCrewHandle).ToString(),
			crew.GetOVTSpawnQueueDiagnostic());
	}

	//------------------------------------------------------------------------------------------------
	//! ONE LINE THAT SAYS WHY A CONVOY IS NOT MOVING, covering both gates and the distance that drives
	//! the second one.
	//! \return A compact human-readable description of the crew's liveness.
	protected string DescribeCrewLiveness()
	{
		if (m_iCrewHandle == -1)
			return "no crew is registered";

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return "the virtualization manager could not be resolved";

		if (!virtualization.IsRegistered(m_iCrewHandle))
			return string.Format("crew handle %1 is no longer registered", m_iCrewHandle.ToString());

		SCR_AIGroup crew = virtualization.GetGroup(m_iCrewHandle);

		string policy = "no group entity";
		if (crew)
			policy = typename.EnumToString(SCR_EAIGroupLifecyclePolicy, crew.GetLifecyclePolicy());

		string nearest = "no live players";
		if (m_Truck)
		{
			float nearestDistance = OVT_MountedGroupActivation.NearestPlayerDistance(m_Truck.GetOrigin());
			if (nearestDistance >= 0)
			{
				int nearestMetres = Math.Round(nearestDistance);
				nearest = nearestMetres.ToString() + " m";
			}
		}

		int ring = virtualization.GetSpawnDistance(m_iCrewHandle);

		// ⚠ THE WHY-CLAUSE IS ONLY ADDED WHEN THERE IS NOTHING TO SEE, and that is the point: a crew with
		// men in it does not need three sentences about the spawn queue, and a crew with none is useless
		// without them. See OVT_MountedGroupActivation.DescribeSpawnState for how to read it.
		string why = "";
		if (OVT_MountedGroupActivation.MaterialisedCount(crew) == 0)
		{
			why = string.Format(" [%1; %2]",
				OVT_MountedGroupActivation.DescribeSpawnState(crew, ring),
				OVT_MountedGroupActivation.DescribeAiBudget(crew));
		}

		return string.Format("crew handle %1: %2 of %3 alive in the mask, %4%5; lifecycle %6 on a %7 m ring, nearest player %8",
			m_iCrewHandle.ToString(),
			virtualization.GetAliveMemberCount(m_iCrewHandle).ToString(),
			virtualization.GetMemberCount(m_iCrewHandle).ToString(),
			OVT_MountedGroupActivation.DescribeActivation(crew),
			why,
			policy,
			ring.ToString(),
			nearest);
	}

	//------------------------------------------------------------------------------------------------
	//! WHERE HOME IS for a transport that has delivered: the spot it was spawned on.
	//! \return The spawn spot when there is one, otherwise the source base.
	protected vector HomePosition()
	{
		if (m_vHome == vector.Zero)
			return m_vSource;

		return m_vHome;
	}

	//------------------------------------------------------------------------------------------------
	//! One observation of an empty truck on its way home.
	//! \param[in] deltaTime Milliseconds since the last update of this module.
	protected void TickReturn(int deltaTime)
	{
		if (!m_Truck || !IsTruckOperational())
		{
			ReleaseConvoy("its transport was destroyed on the way home", false);
			m_eState = OVT_EInsertionState.FINISHED;
			return;
		}

		// ⚠ THE RETURN LEG IS STILL A DRIVE: the crew needs the same pin it had on the way out, which is why
		// the pin follows the RIDE and not the DRIVING state.
		HoldRidersActive();
		NudgeCrewMaterialisation();

		// ⚠ THE RADIUS ALONE, NOT THE SPEED-AWARE ARRIVAL TEST. Nobody gets out here - the truck is empty
		// and is about to be deleted - so there is nothing to throw about, and a transport that rolls
		// through its own spawn at walking pace has got home. Bounded anyway by RETURN_TIMEOUT_TICKS below.
		if (OVT_InsertionGeometry.IsInsideArrivalRadius(vector.Distance(m_Truck.GetOrigin(), HomePosition()), m_fArrivalRadius))
		{
			ReleaseConvoy("its transport is home", true);
			m_eState = OVT_EInsertionState.FINISHED;
			return;
		}

		// 🔴 THE CREW CAN GET OUT AND WALK HOME, and it is vanilla doing it: SCR_AIVehicleCombatActivity
		// dismounts a group whose vehicle answers false to HasWeapon() when it perceives a threat, leaving
		// a truck in the road and a crew executing this module's MOVE order on foot.
		if (!CrewIsAtTheWheel())
		{
			m_iUncrewedTicksElapsed = m_iUncrewedTicksElapsed + 1;

			if (OVT_InsertionGeometry.IsUncrewedGraceExpired(m_iUncrewedTicksElapsed, m_iStuckTicks))
			{
				OVT_DeploymentLog.Debug(string.Format("[Overthrow] Insertion '%1': nobody is driving its empty transport home after %2 update(s) - %3",
					DescribeSelf(), m_iUncrewedTicksElapsed.ToString(), DescribeCrewLiveness()));

				ReleaseConvoy("nobody is driving its transport home - the crew left it", false);
				m_eState = OVT_EInsertionState.FINISHED;
				return;
			}
		}
		else
		{
			m_iUncrewedTicksElapsed = 0;
		}

		m_iReturnTicksElapsed++;
		if (m_iReturnTicksElapsed < RETURN_TIMEOUT_TICKS)
			return;

		// A truck that cannot find its way home is not worth watching for the rest of the campaign.
		// It is deleted where it stands, subject to the same player veto as any other teardown.
		ReleaseConvoy("its transport did not get home in time", true);
		m_eState = OVT_EInsertionState.FINISHED;
	}

	//------------------------------------------------------------------------------------------------
	//! ONE OBSERVATION OF A TRANSPORT THIS MODULE WALKED AWAY FROM.
	protected void TickAbandonedTruck()
	{
		if (!m_bTruckAbandoned)
			return;

		if (!m_Truck)
		{
			DisarmAbandonedTruck();
			return;
		}

		m_iAbandonedTicksElapsed = m_iAbandonedTicksElapsed + 1;

		// Asked every tick rather than only at the deadline. It is a handful of distance checks against
		// the connected players once per ten seconds per abandoned truck, and keeping the whole decision
		// in one pure call is worth more than saving them.
		bool playerNearby = OVT_WorldUtils.PlayerInRange(m_Truck.GetOrigin(), ABANDONED_TRUCK_PLAYER_RADIUS_M);

		if (!OVT_InsertionGeometry.IsAbandonedTruckCollectable(m_iAbandonedTicksElapsed, STUCK_TRUCK_TIMEOUT_TICKS, playerNearby))
		{
			LogAbandonedHold(playerNearby);
			return;
		}

		vector where = m_Truck.GetOrigin();

		// Read before the release, which nulls the handle. ReleaseTruck() disarms this countdown on both of
		// its branches, so this line must only claim a collection that actually happened.
		if (!ReleaseTruck())
			return;

		OVT_DeploymentLog.Debug(string.Format("[Overthrow] Insertion '%1': its abandoned transport at %2 was collected after %3 update(s) - nobody was within %4 m of it, and a transport left on this road is the next convoy's obstacle",
			DescribeSelf(), where.ToString(), m_iAbandonedTicksElapsed.ToString(),
			ABANDONED_TRUCK_PLAYER_RADIUS_M.ToString()));
	}

	//------------------------------------------------------------------------------------------------
	//! Arms the collection countdown on a transport that has just been left standing.
	//! \param[in] reason Why it was left, for the log line.
	protected void ArmAbandonedTruck(string reason)
	{
		if (!m_Truck)
			return;

		if (m_bTruckAbandoned)
			return;

		// 🔴 A transport that never left its own motor pool is an OBSTRUCTION: it holds an authored
		// OVT_VehiclePatrolSpawn out of service, and when the last free marker goes the next insertion
		// falls back to the road snap and strands itself somewhere worse.
		if (m_vHome != vector.Zero && vector.Distance(m_Truck.GetOrigin(), m_vHome) <= ABANDONED_AT_SPAWN_RADIUS_M)
		{
			OVT_DeploymentLog.Debug(string.Format("[Overthrow] Insertion '%1': its transport never left its spawn point - %2. Collecting it immediately, without even waiting for the coast to clear, because it is standing on a vehicle spawn the next insertion needs",
				DescribeSelf(), reason));

			ReleaseTruck();
			return;
		}

		m_bTruckAbandoned = true;
		m_iAbandonedTicksElapsed = 0;
		m_bAbandonedHoldLogged = false;

		// ⚠ NORMAL, not VERBOSE: a reader has to be able to tell "waiting for the coast to clear" apart
		// from "nothing is watching it" without a debugger.
		OVT_DeploymentLog.Debug(string.Format("[Overthrow] Insertion '%1': its transport is left standing at %2 - %3. It will be collected on the next update once nobody is within %4 m of it",
			DescribeSelf(), m_Truck.GetOrigin().ToString(), reason,
			ABANDONED_TRUCK_PLAYER_RADIUS_M.ToString()));
	}

	//------------------------------------------------------------------------------------------------
	//! Stops counting. Safe to call when nothing was ever armed.
	protected void DisarmAbandonedTruck()
	{
		m_bTruckAbandoned = false;
		m_iAbandonedTicksElapsed = 0;
		m_bAbandonedHoldLogged = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Says ONCE that an overdue transport is being kept because somebody is standing near it.
	//! \param[in] playerNearby Whether that is in fact why it is being kept.
	protected void LogAbandonedHold(bool playerNearby)
	{
		if (m_bAbandonedHoldLogged)
			return;

		if (!playerNearby)
			return;

		if (m_iAbandonedTicksElapsed < STUCK_TRUCK_TIMEOUT_TICKS)
			return;

		m_bAbandonedHoldLogged = true;

		// NORMAL for the same reason ArmAbandonedTruck's line is: this is the answer to "why is that
		// truck still standing there", it is latched to exactly one line per abandoned transport, and a
		// reader who cannot see it concludes the countdown is broken.
		OVT_DeploymentLog.Debug(string.Format("[Overthrow] Insertion '%1': its abandoned transport at %2 is overdue for collection but a player is within %3 m, so it stays until nobody is",
			DescribeSelf(), m_Truck.GetOrigin().ToString(), ABANDONED_TRUCK_PLAYER_RADIUS_M.ToString()));
	}

	//------------------------------------------------------------------------------------------------
	//! THE SUCCESS PATH: the force is down at the landing zone and the truck goes home.
	protected void CompleteInsertion()
	{
		int delivered = m_aHandles.Count();

		DisembarkPassengers();
		DropPassengersToGlobalRing();

		// The slot is about trucks driving TOWARDS an objective. The empty one going home does not hold
		// the next insertion up.
		ReleaseReservation();

		OnInsertionArrived(m_vLZ);

		m_eState = OVT_EInsertionState.RETURNING;
		m_iReturnTicksElapsed = 0;

		// The return leg has its own uncrewed test (see TickReturn) and must start from zero rather than
		// from whatever the outbound drive left behind.
		m_iUncrewedTicksElapsed = 0;
		m_iUnmaterialisedTicksElapsed = 0;

		IssueCrewMove(HomePosition());

		OVT_DeploymentLog.Debug(string.Format("[Overthrow] Insertion '%1' delivered %2 group(s) at %3; its transport is going home",
			DescribeSelf(), delivered.ToString(), m_vLZ.ToString()));
	}

	//------------------------------------------------------------------------------------------------
	//! THE HOOK A SUBCLASS OVERRIDES to do something at the drop point. Empty here on purpose: this
	//! module delivers a force and nothing else.
	//! \param[in] lzPosition Where the transport actually stopped - NOT the objective.
	protected void OnInsertionArrived(vector lzPosition)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! THE FALLBACK, taken when a drive that had already started cannot continue.
	//! \param[in] reason What ended the drive, for the log line.
	protected void DismountAndWalk(string reason)
	{
		DisembarkPassengers();

		// The truck is NOT deleted here and NOT left forever either: "released when the deployment ends" is
		// only true of a deployment that ENDS, and the forward base's stands as long as the base does.
		ReleaseConvoy(reason, false);

		EnterWalking(reason);

		// Immediately, rather than waiting for the next convergence: the men are on the ground and
		// there is no reason for them to be an always-materialised squad for another ten seconds.
		DropPassengersToGlobalRing();
	}

	//------------------------------------------------------------------------------------------------
	//! Diverts an insertion that has not yet started driving onto the march.
	//! \param[in] reason Why there is no convoy, for the log line.
	protected void FallBackToWalking(string reason)
	{
		ReleaseConvoy(reason, true);
		EnterWalking(reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Settles this insertion into "on foot".
	//! \param[in] reason Why, for the log line.
	protected void EnterWalking(string reason)
	{
		m_eState = OVT_EInsertionState.WALKING;

		OVT_DeploymentLog.Debug(string.Format("[Overthrow] Insertion '%1' is on foot: %2", DescribeSelf(), reason));
	}

	//------------------------------------------------------------------------------------------------
	// Teardown - see the class header's owned/borrowed split
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Hands back everything this module borrowed for a convoy. IDEMPOTENT and safe on a module that
	//! never had one.
	//! \param[in] reason What ended the convoy, for the log line. Empty logs nothing.
	//! \param[in] deleteTruck Whether the transport should be taken away as well.
	protected void ReleaseConvoy(string reason, bool deleteTruck)
	{
		// ⚠ m_Truck still resolves here - it is not nulled until later in this method - so this is the one
		// place every road to a destroyed transport funnels through. Gated on IsTruckOperational() rather
		// than `reason`, so an ordinary teardown with an intact vehicle reports nothing.
		if (m_Truck && !IsTruckOperational())
		{
			OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
			if (occupying)
				occupying.ReportVehicleLoss(m_Truck.GetOrigin());
		}

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();

		SCR_AIGroup crew;
		if (virtualization && m_iCrewHandle != -1)
			crew = virtualization.GetGroup(m_iCrewHandle);

		ClearOwnedWaypoints(crew);

		// ⚠ BEFORE THE UNREGISTER AND BEFORE UnsubscribeRiders(), because both of those take away the
		// handles this reads. It is the single release point for the activation pin - see
		// ReleaseRidersActive() for why there is exactly one and why it is wider than the pin.
		ReleaseRidersActive();

		// ⚠ AND THE THIRD STAMP, before ReleaseTruck() below nulls m_Truck - a leaked observer holds
		// everything around it materialised for the rest of the session. NOT gated on
		// m_bTransportIsObserver: an author who switches the parking off must still get back what is parked.
		if (virtualization && m_Truck)
			virtualization.RemoveEntityObserver(m_Truck);

		UnsubscribeRiders();

		if (virtualization && m_iCrewHandle != -1)
		{
			// ⚠ TAKE THE MEN OUT AND PUT THEM AWAY BEFORE HANDING THE REGISTRATION BACK. See
			// StandDownCrew() - without this the truck is deleted a few lines below with a live crew
			// inside it, and the registration is retired around men who never despawn.
			StandDownCrew(virtualization, m_iCrewHandle);

			// UnregisterGroup respects held members: a crewman still sitting in the truck retires the
			// group in place rather than having it deleted out from under him. StandDownCrew() has
			// already emptied the group, so the ordinary branch - despawn and delete - is the one taken.
			virtualization.UnregisterGroup(m_iCrewHandle);
		}

		m_iCrewHandle = -1;

		// ⚠ And then sweep the key. A crew key is unique to ONE insertion, so NOTHING WILL EVER LOOK UNDER
		// IT AGAIN once this module is gone - a lost record would be a permanently-materialised two-man
		// group with no owner for the rest of the campaign. Asking the registry costs one map lookup.
		if (virtualization && !m_sCrewOwnerKey.IsEmpty())
		{
			array<int> strays = virtualization.FindGroupsByOwner(OWNER_SYSTEM, m_sCrewOwnerKey);
			foreach (int strayHandle : strays)
			{
				if (!virtualization.IsRegistered(strayHandle))
					continue;

				Print(string.Format("[Overthrow] Insertion '%1': crew handle %2 was still registered under this insertion's key at teardown - releasing it",
					DescribeSelf(), strayHandle.ToString()), LogLevel.WARNING);

				StandDownCrew(virtualization, strayHandle);

				virtualization.UnregisterGroup(strayHandle);
			}
		}

		if (deleteTruck)
		{
			ReleaseTruck();
		}
		else
		{
			// ⚠ THE ONE PLACE THE COLLECTION COUNTDOWN IS ARMED, covering both a truck stranded mid-drive and
			// one destroyed on its way home.
			ArmAbandonedTruck(reason);
		}

		ReleaseReservation();

		m_bHaveLastTruckPosition = false;
		m_iStuckTicksElapsed = 0;
		m_iUncrewedTicksElapsed = 0;
		m_iUnmaterialisedTicksElapsed = 0;
		m_iInsideRadiusTicks = 0;

		if (!reason.IsEmpty())
			OVT_DeploymentLog.Debug(string.Format("[Overthrow] Insertion '%1': convoy stood down - %2", DescribeSelf(), reason));
	}

	//------------------------------------------------------------------------------------------------
	//! WINDS THE TRANSPORT CREW UP FOR GOOD: out of the truck, off the map, ready to be unregistered.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] handle The crew registration to wind up. Passed in rather than read off m_iCrewHandle
	//!            because the teardown sweep in ReleaseConvoy() stands down crews this module has
	//!            already stopped tracking.
	protected void StandDownCrew(notnull OVT_VirtualizationManagerComponent virtualization, int handle)
	{
		if (handle == -1)
			return;

		SCR_AIGroup crew = virtualization.GetGroup(handle);
		if (!crew)
			return;

		OVT_MountedGroupActivation.ReleaseGroupActive(crew);

		array<AIAgent> agents = {};
		crew.GetAgents(agents);

		foreach (AIAgent agent : agents)
		{
			DisembarkAgent(agent);
		}

		if (crew.GetAgentsCount() > 0)
			crew.DespawnMembers();
	}

	//------------------------------------------------------------------------------------------------
	//! Gives the faction's convoy slot back. Idempotent: only a claim that was actually made is
	//! released, and the manager floors its own counter at zero regardless.
	protected void ReleaseReservation()
	{
		if (!m_bReserved)
			return;

		m_bReserved = false;

		if (!m_ParentDeployment)
			return;

		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
			return;

		manager.ReleaseInsertion(m_ParentDeployment.GetControllingFaction());
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the truck away - unless somebody has made it theirs.
	//! \return True when the vehicle was actually deleted, false when a veto left it standing. Every
	//!         caller but the abandoned-transport sweep ignores it; that one needs it so its log line
	//!         cannot claim to have collected a truck a player had just claimed.
	protected bool ReleaseTruck()
	{
		if (!m_Truck)
			return false;

		string veto = TruckDeletionVeto(m_Truck);
		if (veto != "")
		{
			OVT_DeploymentLog.Debug(string.Format("[Overthrow] Insertion '%1': transport left standing - %2", DescribeSelf(), veto));
			m_Truck = null;
			DisarmAbandonedTruck();
			return false;
		}

		// ⚠ The last line of defence against deleting a vehicle over its occupants. TruckDeletionVeto()
		// answers a question about who the vehicle BELONGS to and says nothing about the men this
		// module put in the cab.
		EvacuateAiOccupants(m_Truck);

		OVT_WorldUtils.DeleteEntityTree(m_Truck);
		m_Truck = null;
		DisarmAbandonedTruck();

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts every AI still aboard on the ground, immediately before the vehicle stops existing.
	//! \param[in] vehicle The transport about to be deleted.
	protected void EvacuateAiOccupants(notnull Vehicle vehicle)
	{
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(BaseCompartmentManagerComponent)
		);

		if (!compartments)
			return;

		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);

		int evacuated = 0;

		foreach (BaseCompartmentSlot slot : slots)
		{
			if (!slot)
				continue;

			IEntity occupant = slot.GetOccupant();
			if (!occupant)
				continue;

			CompartmentAccessComponent access = CompartmentAccessComponent.Cast(
				occupant.FindComponent(CompartmentAccessComponent)
			);

			if (!access || !access.IsInCompartment())
				continue;

			access.GetOutVehicle(EGetOutType.TELEPORT, 0, ECloseDoorAfterActions.LEAVE_OPEN, true);
			evacuated = evacuated + 1;
		}

		if (evacuated > 0)
		{
			OVT_DeploymentLog.Debug(string.Format("[Overthrow] Insertion '%1': %2 AI occupant(s) put on the ground before their transport was removed",
				DescribeSelf(), evacuated.ToString()));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! ⚠ A test of OWNERSHIP, not of SAFETY: it says nothing about this module's own crew, which is why
	//! the vehicle is emptied first (see EvacuateAiOccupants) rather than vetoed here.
	//! \param[in] vehicle The transport to judge.
	//! \return An empty string when it is safe to delete, or the reason it is not.
	protected string TruckDeletionVeto(notnull Vehicle vehicle)
	{
		return OVT_VehicleClaim.DeletionVeto(vehicle);
	}

	//------------------------------------------------------------------------------------------------
	//! The deployment is over.
	override protected void OnCleanup()
	{
		ReleaseConvoy("the deployment is over", true);
		m_eState = OVT_EInsertionState.FINISHED;

		super.OnCleanup();
	}

	//------------------------------------------------------------------------------------------------
	//! One registered group has been wiped out.
	//! \param[in] handle The wiped group's registry handle.
	override void OnVirtualGroupWiped(int handle)
	{
		if (m_iCrewHandle != -1 && handle == m_iCrewHandle)
		{
			m_iCrewHandle = -1;

			if (m_eState == OVT_EInsertionState.DRIVING)
			{
				DismountAndWalk("its transport crew was killed");
				return;
			}

			if (m_eState == OVT_EInsertionState.RETURNING)
			{
				ReleaseConvoy("its transport crew was killed on the way home", true);
				m_eState = OVT_EInsertionState.FINISHED;
			}

			return;
		}

		super.OnVirtualGroupWiped(handle);
	}

	//------------------------------------------------------------------------------------------------
	// The registration seams
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! WHERE THE FORCE IS REGISTERED: at its source, always, whatever the config says about nearest
	//! bases.
	//! \param[in] factionIndex The deployment's controlling faction.
	//! \param[in] fromNearestBase Ignored; the provider is the authority here.
	//! \param[out] anchor The resolved origin.
	//! \return False when there is nowhere for the force to come from.
	override protected bool ResolveSpawnAnchor(int factionIndex, bool fromNearestBase, out vector anchor)
	{
		anchor = vector.Zero;

		if (!EnsureSourceResolved())
			return false;

		anchor = m_vSource;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! WHERE ONE GROUP GOES: beside the truck while there is one to board, and on the base class's ring
	//! roll otherwise.
	//! \param[in] anchor The batch anchor - the resolved source.
	//! \param[in] index Position within this batch.
	//! \return The world position to register at.
	override protected vector ResolveSpawnPosition(vector anchor, int index)
	{
		if (IsForceMounted())
			return m_Truck.GetOrigin() + Vector(PASSENGER_SPAWN_OFFSET_M, 0, 0);

		return super.ResolveSpawnPosition(anchor, index);
	}

	//------------------------------------------------------------------------------------------------
	//! IS THERE A VEHICLE UNDER THIS FORCE RIGHT NOW - the one question both registration seams and the
	//! ring sweep should be asking.
	//! \return True while the force is riding a transport that exists.
	protected bool IsForceMounted()
	{
		return m_eState == OVT_EInsertionState.DRIVING && m_Truck;
	}

	//------------------------------------------------------------------------------------------------
	//! THE RING: always-materialised while riding, ordinary otherwise. See RIDING_SPAWN_DISTANCE for
	//! why a dormant passenger cannot be seated - and for why a ring alone was never the whole answer.
	//! \return The spawnDistanceOverride to register with.
	override protected int ResolveRegistrationSpawnDistance()
	{
		if (IsForceMounted())
			return RIDING_SPAWN_DISTANCE;

		return super.ResolveRegistrationSpawnDistance();
	}

	//------------------------------------------------------------------------------------------------
	//! THE PLAN: whatever the behaviour modules want, and failing that, a march onto the objective.
	//! \param[in] groupPosition Where the group is about to be registered.
	//! \return The plan. Never null once there is a deployment to march towards.
	override protected OVT_VirtualWaypointPlan ResolveVirtualPlan(vector groupPosition)
	{
		OVT_VirtualWaypointPlan plan = super.ResolveVirtualPlan(groupPosition);
		if (plan)
			return plan;

		if (!m_ParentDeployment)
			return null;

		array<vector> stops = {};
		stops.Insert(m_ParentDeployment.GetPosition());

		return OVT_VirtualPlanFactory.BuildRoutePlan(stops, MARCH_HOLD_SECONDS, false, groupPosition);
	}

	//------------------------------------------------------------------------------------------------
	//! A newly registered group is a passenger. Subscribe it and seat it if the truck is waiting.
	//! \param[in] handle The new group's registry handle.
	//! \param[in] position Where it was registered.
	override protected void OnGroupRegistered(int handle, vector position)
	{
		super.OnGroupRegistered(handle, position);

		AdoptPassenger(handle);
	}

	//------------------------------------------------------------------------------------------------
	//! A group re-found in the registry is a passenger too.
	//! \param[in] handle The reclaimed group's registry handle.
	override protected void OnGroupReclaimed(int handle)
	{
		super.OnGroupReclaimed(handle);

		AdoptPassenger(handle);
	}

	//------------------------------------------------------------------------------------------------
	//! Binds one passenger group to the ride, if there is one.
	//! \param[in] handle The group's registry handle.
	protected void AdoptPassenger(int handle)
	{
		if (m_eState != OVT_EInsertionState.DRIVING)
			return;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		SCR_AIGroup group = virtualization.GetGroup(handle);
		if (!group)
			return;

		PairRider(group, false);

		if (m_Truck)
			SeatExistingRiders(group, false);
	}

	//------------------------------------------------------------------------------------------------
	// Seating
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Records what a group is riding as, and subscribes its per-member arrival callback.
	//! \param[in] group The rider.
	//! \param[in] isCrew True for the transport crew, false for a passenger.
	protected void PairRider(notnull SCR_AIGroup group, bool isCrew)
	{
		m_mRiderIsCrew.Set(group.GetID(), isCrew);

		group.GetOnAgentAdded().Remove(OnRiderAgentAdded);
		group.GetOnAgentAdded().Insert(OnRiderAgentAdded);
	}

	//------------------------------------------------------------------------------------------------
	//! Stops listening for every rider's members, so no invoker keeps a pointer into a module the
	//! deployment is about to throw away.
	protected void UnsubscribeRiders()
	{
		BaseWorld world = GetGame().GetWorld();

		for (int i = 0; i < m_mRiderIsCrew.Count(); i++)
		{
			if (!world)
				break;

			SCR_AIGroup group = SCR_AIGroup.Cast(world.FindEntityByID(m_mRiderIsCrew.GetKey(i)));
			if (group)
				group.GetOnAgentAdded().Remove(OnRiderAgentAdded);
		}

		m_mRiderIsCrew.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! One rider has arrived in the world. Put him in the truck.
	//! \param[in] agent The arriving member.
	protected void OnRiderAgentAdded(AIAgent agent)
	{
		if (!agent)
			return;

		// ⚠ RETURNING COUNTS NOW, AND IT DID NOT USED TO. A crewman who materialises on the way home is
		// as much a driver as one who materialises on the way out, and refusing him here left the empty
		// truck's replacement crew unpinned and unseated.
		if (m_eState != OVT_EInsertionState.DRIVING && m_eState != OVT_EInsertionState.RETURNING)
			return;

		AIGroup parent = agent.GetParentGroup();
		if (!parent)
			return;

		EntityID groupId = parent.GetID();
		if (!m_mRiderIsCrew.Contains(groupId))
			return;

		bool isCrew = m_mRiderIsCrew.Get(groupId);

		// THE PIN FIRST FOR A CREWMAN, before any test that can bail out. It is what makes him drive;
		// losing it to a transport that has just gone would leave a live crew asleep while ReleaseConvoy
		// is still a tick away. Releasing it again there is free. Skipped while the transport is an
		// observer, which keeps him off max LOD by itself - see HoldRidersActive().
		if (isCrew && !m_bTransportIsObserver)
			OVT_MountedGroupActivation.HoldAgentActive(agent);

		if (!m_Truck)
		{
			m_mRiderIsCrew.Remove(groupId);
			return;
		}

		// Passengers are only ever seated on the way OUT. The way home is the empty truck's leg, and
		// putting a member of a force that has already been delivered back into it would carry him away
		// from the objective he was dropped for.
		if (!isCrew && m_eState != OVT_EInsertionState.DRIVING)
			return;

		SeatRider(m_Truck, agent, isCrew);
	}

	//------------------------------------------------------------------------------------------------
	//! BOARDING: seats everybody who is already on their feet - the crew, then the force.
	protected void BoardEveryone()
	{
		if (!m_Truck)
			return;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		if (m_iCrewHandle != -1)
		{
			SCR_AIGroup crew = virtualization.GetGroup(m_iCrewHandle);
			if (crew)
				SeatExistingRiders(crew, true);
		}

		foreach (int handle : m_aHandles)
		{
			SCR_AIGroup group = virtualization.GetGroup(handle);
			if (group)
				SeatExistingRiders(group, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! THE ONE THING STILL POLICED ON A MOVING TRUCK: a member of the FORCE sitting in the driver's seat.
	protected void EvictHijackers()
	{
		if (!m_Truck)
			return;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		foreach (int handle : m_aHandles)
		{
			SCR_AIGroup group = virtualization.GetGroup(handle);
			if (!group)
				continue;

			array<AIAgent> agents = {};
			group.GetAgents(agents);

			foreach (AIAgent agent : agents)
			{
				if (!agent)
					continue;

				IEntity character = agent.GetControlledEntity();
				if (!character)
					continue;

				CompartmentAccessComponent access = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
				if (!access)
					continue;

				// ⚠ THE GUARD THAT MAKES THIS SAFE. A man on foot is somebody else's business - the
				// spawn hook's, or nobody's.
				if (!access.IsInCompartment())
					continue;

				EvictPassengerFromPilotSeat(m_Truck, agent, access);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Seats every member a group already has.
	//! \param[in] group The rider.
	//! \param[in] isCrew True for the transport crew.
	protected void SeatExistingRiders(notnull SCR_AIGroup group, bool isCrew)
	{
		if (!m_Truck)
			return;

		array<AIAgent> agents = {};
		group.GetAgents(agents);

		foreach (AIAgent agent : agents)
		{
			SeatRider(m_Truck, agent, isCrew);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Puts one man in a seat.
	//! \param[in] vehicle The transport.
	//! \param[in] agent The man.
	//! \param[in] isCrew True for the transport crew.
	//! \return True when he was seated.
	protected bool SeatRider(notnull Vehicle vehicle, AIAgent agent, bool isCrew)
	{
		if (!agent)
			return false;

		IEntity character = agent.GetControlledEntity();
		if (!character)
			return false;

		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
		if (!access)
			return false;

		// Already aboard something - this truck, or one he was ordered into. Leave him there.
		if (access.IsInCompartment())
		{
			if (isCrew)
				return false;

			return EvictPassengerFromPilotSeat(vehicle, agent, access);
		}

		SCR_BaseCompartmentManagerComponent compartmentManager = SCR_BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(SCR_BaseCompartmentManagerComponent)
		);

		if (!compartmentManager)
			return false;

		array<BaseCompartmentSlot> cargo = {};
		int cabCount = CollectCargoSlots(vehicle, cargo);

		if (isCrew)
		{
			if (FillCompartment(compartmentManager, agent, ECompartmentType.PILOT))
				return true;

			// ⚠ The gunner BEFORE the co-driver, on the author's instruction: "insertion teams in future
			// deployment configs may have a weapon... so yes we should force driver + gunner (fallback to
			// co-driver when no gunner position)".
			if (FillCompartment(compartmentManager, agent, ECompartmentType.TURRET))
				return true;

			// The co-driver's seat: the fallback on every transport that carries no weapon, which is
			// every one shipped today. He is still the man who gets out to open gates.
			if (cabCount > 0 && FillSlot(vehicle, agent, cargo[0]))
				return true;

			return FillCompartment(compartmentManager, agent, ECompartmentType.CARGO);
		}

		return SeatPassengerInCargo(vehicle, agent, compartmentManager, cargo, cabCount);
	}

	//------------------------------------------------------------------------------------------------
	//! WHERE A MEMBER OF THE FORCE GOES: the back of the truck, then the leftover cab, then anywhere.
	//! \param[in] vehicle The transport.
	//! \param[in] agent The man.
	//! \param[in] compartmentManager The transport's own compartment manager, for the last resort.
	//! \param[in] cargo Every cargo slot, cab first, from CollectCargoSlots().
	//! \param[in] cabCount How many leading entries of `cargo` are cab seats.
	//! \return True when he was seated.
	protected bool SeatPassengerInCargo(notnull Vehicle vehicle, AIAgent agent, SCR_BaseCompartmentManagerComponent compartmentManager, notnull array<BaseCompartmentSlot> cargo, int cabCount)
	{
		// THE BACK OF THE TRUCK FIRST: everything past the cab, in authored order. On a transport with a
		// bed this is where the whole force ends up, which is both what it should look like and what
		// keeps the cab free for the men whose job needs them able to get out of it.
		for (int i = cabCount; i < cargo.Count(); i++)
		{
			if (FillSlot(vehicle, agent, cargo[i]))
				return true;
		}

		// Then the cab, MINUS the co-driver's seat - the leftover middle seat on a Ural, or every seat on
		// a vehicle that has no bed at all, which is the ordinary case for a car and not a fallback.
		for (int i = 1; i < cabCount; i++)
		{
			if (FillSlot(vehicle, agent, cargo[i]))
				return true;
		}

		// ⚠ THE RESERVATION IS A PREFERENCE, NOT A RULE: by the time a passenger reaches this line the crew
		// that wanted the seat has been seated, or there is no crew at all.
		if (!compartmentManager)
			return false;

		return FillCompartment(compartmentManager, agent, ECompartmentType.CARGO);
	}

	//------------------------------------------------------------------------------------------------
	//! GETS A MEMBER OF THE FORCE OUT OF THE DRIVER'S SEAT OF OUR OWN TRUCK.
	//! \param[in] vehicle The transport.
	//! \param[in] agent The man who has taken the wheel.
	//! \param[in] access His compartment access component.
	//! \return True when he was moved.
	protected bool EvictPassengerFromPilotSeat(notnull Vehicle vehicle, AIAgent agent, notnull CompartmentAccessComponent access)
	{
		BaseCompartmentSlot slot = access.GetCompartment();
		if (!slot)
			return false;

		if (slot.GetType() != ECompartmentType.PILOT)
			return false;

		// Somebody else's vehicle. Not ours to police.
		if (slot.GetVehicle() != vehicle)
			return false;

		SCR_BaseCompartmentManagerComponent compartmentManager = SCR_BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(SCR_BaseCompartmentManagerComponent)
		);

		array<BaseCompartmentSlot> cargo = {};
		int cabCount = CollectCargoSlots(vehicle, cargo);

		if (SeatPassengerInCargo(vehicle, agent, compartmentManager, cargo, cabCount))
		{
			OVT_DeploymentLog.Debug(string.Format("[Overthrow] Insertion '%1': a member of the force had taken the driver's seat - moved to a passenger seat before it could drive the convoy to the objective instead of the landing zone",
				DescribeSelf()));

			return true;
		}

		Print(string.Format("[Overthrow] Insertion '%1': a member of the force is driving and there is NO free passenger seat to move him to - leaving him at the wheel, because putting a man on a moving road is worse. The convoy may drive to the objective rather than the landing zone",
			DescribeSelf()), LogLevel.WARNING);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! EVERY CARGO SEAT ON A TRANSPORT, IN AUTHORED ORDER, cab first - so index 0 is the co-driver.
	//! \param[in] vehicle The transport.
	//! \param[out] cargo Filled with its cargo compartments; cleared first.
	//! \return How many leading entries belong to the vehicle itself, i.e. the cab.
	protected int CollectCargoSlots(notnull Vehicle vehicle, notnull array<BaseCompartmentSlot> cargo)
	{
		cargo.Clear();

		AppendOwnCargoSlots(vehicle, cargo);

		int cabCount = cargo.Count();

		IEntity child = vehicle.GetChildren();
		while (child)
		{
			AppendCargoSlots(child, cargo);
			child = child.GetSibling();
		}

		return cabCount;
	}

	//------------------------------------------------------------------------------------------------
	//! One entity's own cargo compartments, then its children's - the same shape, and the same order, as
	//! vanilla's FindFreeAndAccessibleCompartment, which recurses by calling itself on each child.
	//! \param[in] entity The entity whose compartment manager to read.
	//! \param[out] cargo Appended to.
	protected void AppendCargoSlots(notnull IEntity entity, notnull array<BaseCompartmentSlot> cargo)
	{
		AppendOwnCargoSlots(entity, cargo);

		IEntity child = entity.GetChildren();
		while (child)
		{
			AppendCargoSlots(child, cargo);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The cargo compartments of ONE entity, ignoring its children.
	//! \param[in] entity The entity whose compartment manager to read.
	//! \param[out] cargo Appended to.
	protected void AppendOwnCargoSlots(notnull IEntity entity, notnull array<BaseCompartmentSlot> cargo)
	{
		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(
			entity.FindComponent(BaseCompartmentManagerComponent)
		);

		if (!manager)
			return;

		array<BaseCompartmentSlot> slots = {};
		manager.GetCompartments(slots);

		foreach (BaseCompartmentSlot slot : slots)
		{
			if (!slot)
				continue;

			if (slot.GetType() != ECompartmentType.CARGO)
				continue;

			cargo.Insert(slot);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Puts one man in ONE NAMED SEAT, or refuses.
	//! \param[in] vehicle The transport.
	//! \param[in] agent The man.
	//! \param[in] slot The seat.
	//! \return True when he got it.
	protected bool FillSlot(notnull Vehicle vehicle, AIAgent agent, BaseCompartmentSlot slot)
	{
		if (!agent || !slot)
			return false;

		IEntity character = agent.GetControlledEntity();
		if (!character)
			return false;

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(character.FindComponent(SCR_CompartmentAccessComponent));
		if (!access)
			return false;

		return access.MoveInVehicle(vehicle, ECompartmentType.CARGO, false, slot);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] compartmentManager The transport's compartments.
	//! \param[in] agent The man.
	//! \param[in] type Which kind of seat to try.
	//! \return True when he got one.
	protected bool FillCompartment(SCR_BaseCompartmentManagerComponent compartmentManager, AIAgent agent, ECompartmentType type)
	{
		if (!agent || !agent.GetControlledEntity())
			return false;

		IEntity character = agent.GetControlledEntity();

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(character.FindComponent(SCR_CompartmentAccessComponent));
		if (!access)
			return false;

		IEntity vehicle = compartmentManager.GetOwner();
		if (!vehicle)
			return false;

		return access.MoveInVehicle(vehicle, type);
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the doors: every member of the FORCE gets out of the truck, wherever the truck is.
	protected void DisembarkPassengers()
	{
		if (!m_Truck)
			return;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		foreach (int handle : m_aHandles)
		{
			SCR_AIGroup group = virtualization.GetGroup(handle);
			if (!group)
				continue;

			array<AIAgent> agents = {};
			group.GetAgents(agents);

			foreach (AIAgent agent : agents)
			{
				DisembarkAgent(agent);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Gets one man out.
	//! \param[in] agent The man to put on the ground.
	protected void DisembarkAgent(AIAgent agent)
	{
		if (!agent)
			return;

		IEntity character = agent.GetControlledEntity();
		if (!character)
			return;

		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
		if (!access)
			return;

		if (!access.IsInCompartment())
			return;

		access.GetOutVehicle(EGetOutType.TELEPORT, 0, ECloseDoorAfterActions.LEAVE_OPEN, true);
	}

	//------------------------------------------------------------------------------------------------
	// The riding ring
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Puts the whole force back on the ordinary proximity ring.
	protected void DropPassengersToGlobalRing()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		foreach (int handle : m_aHandles)
		{
			RestoreGlobalSpawnRing(virtualization, handle);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Puts ONE group back on the ordinary proximity ring, in the record and in the engine.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] handle The group to put back.
	protected void RestoreGlobalSpawnRing(notnull OVT_VirtualizationManagerComponent virtualization, int handle)
	{
		OVT_VirtualGroupRecord record = virtualization.GetRecord(handle);
		if (record)
		{
			if (record.m_iSpawnDistanceOverride == SPAWN_DISTANCE_GLOBAL)
				return;

			record.m_iSpawnDistanceOverride = SPAWN_DISTANCE_GLOBAL;
		}

		SCR_AIGroup group = virtualization.GetGroup(handle);
		if (!group)
			return;

		int spawnDistance = virtualization.GetSpawnDistance(handle);
		if (spawnDistance <= 0)
		{
			// core expresses "never materialise by proximity" as the Manual policy, because
			// SetLifecyclePolicy ignores non-positive distances and a ProximityDriven group stamped
			// with 0 would silently keep vanilla's own defaults.
			group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
			return;
		}

		int despawnDistance = OVT_VirtualizationMath.ResolveDespawnDistance(spawnDistance, DESPAWN_HYSTERESIS);

		group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.ProximityDriven, spawnDistance, despawnDistance, -1);
	}

	//------------------------------------------------------------------------------------------------
	// Queries
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The force, plus the transport and its crew while they exist.
	override array<IEntity> GetSpawnedEntities()
	{
		array<IEntity> entities = super.GetSpawnedEntities();

		if (m_Truck)
			entities.Insert(m_Truck);

		if (m_iCrewHandle != -1)
		{
			OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
			if (virtualization)
			{
				SCR_AIGroup crew = virtualization.GetGroup(m_iCrewHandle);
				if (crew)
					entities.Insert(crew);
			}
		}

		return entities;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether the transport is still a working vehicle.
	//! ⚠ It asks "is it a WRECK", not "can it drive": an immobilised truck passes, and is written off by
	//! the uncrewed test within about a minute of its crew walking out.
	protected bool IsTruckOperational()
	{
		if (!m_Truck)
			return false;

		SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.Cast(m_Truck.FindComponent(SCR_DamageManagerComponent));
		if (damageManager && damageManager.IsDestroyed())
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! ⚠ ASKED OF THE SURVIVOR MASK, NEVER OF AN AGENT COUNT - a dormant or spawn-queued group reports
	//! zero agents while being perfectly alive.
	//! \return Whether the transport still has a crew.
	protected bool IsCrewAlive()
	{
		if (m_iCrewHandle == -1)
			return false;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return false;

		if (!virtualization.IsRegistered(m_iCrewHandle))
			return false;

		return virtualization.GetAliveMemberCount(m_iCrewHandle) > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The owner key the CREW is registered under - UNIQUE TO THIS ONE INSERTION, not to its deployment
	//! and not to its config. See CREW_KEY_SUFFIX for why it must also differ from the passengers'.
	//! \return The key, or an empty string when there is no deployment to key against.
	protected string GetCrewOwnerKey()
	{
		if (!m_sCrewOwnerKey.IsEmpty())
			return m_sCrewOwnerKey;

		string deploymentScoped = BuildOwnerKey(m_sModuleName + CREW_KEY_SUFFIX);
		if (deploymentScoped.IsEmpty())
			return "";

		s_iCrewKeySerial = s_iCrewKeySerial + 1;

		m_sCrewOwnerKey = deploymentScoped + CREW_INSTANCE_MARK + s_iCrewKeySerial.ToString();

		return m_sCrewOwnerKey;
	}

	//------------------------------------------------------------------------------------------------
	//! The vehicle prefab for this module's transport type.
	//! \param[in] factionIndex The deployment's controlling faction.
	//! \return The prefab, or an empty ResourceName - which is one of the five roads to walking.
	protected ResourceName GetVehiclePrefabFromFaction(int factionIndex)
	{
		OVT_OverthrowFactionManager factions = OVT_Global.GetFactions();
		if (!factions)
			return "";

		OVT_Faction faction = factions.GetOverthrowFactionByIndex(factionIndex);
		if (!faction)
		{
			Print(string.Format("[Overthrow] Insertion '%1': faction index %2 resolves to no faction",
				DescribeSelf(), factionIndex.ToString()), LogLevel.WARNING);
			return "";
		}

		faction.InitializeVehicleRegistry();

		ResourceName prefab = faction.GetVehiclePrefabByName(m_sTruckVehicleType);
		if (prefab.IsEmpty())
		{
			Print(string.Format("[Overthrow] Insertion '%1': transport type '%2' is not in faction '%3's registry",
				DescribeSelf(), m_sTruckVehicleType, faction.GetFactionKey()), LogLevel.WARNING);
		}

		return prefab;
	}

	//------------------------------------------------------------------------------------------------
	//! How this insertion names itself in a log line: the deployment, then the module within it.
	//! \return "<deployment name>/<module name>".
	protected string DescribeSelf()
	{
		string deploymentName = "unknown deployment";
		if (m_ParentDeployment)
			deploymentName = m_ParentDeployment.GetDeploymentName();

		if (m_sModuleName.IsEmpty())
			return deploymentName;

		return deploymentName + "/" + m_sModuleName;
	}

	//------------------------------------------------------------------------------------------------
	//! \return What stage of its journey this insertion is at.
	OVT_EInsertionState GetInsertionState()
	{
		return m_eState;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Where the force set out from; vector.Zero before an origin has been resolved.
	vector GetInsertionSource()
	{
		return m_vSource;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Where the transport is headed; only meaningful once the state is DRIVING.
	vector GetLandingZone()
	{
		return m_vLZ;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether this insertion currently holds one of its faction's convoy slots.
	bool HoldsInsertionReservation()
	{
		return m_bReserved;
	}

	//------------------------------------------------------------------------------------------------
	// Cloning
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! EVERY inherited attribute plus this module's own ten.
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_InsertionSpawningDeploymentModule clone = new OVT_InsertionSpawningDeploymentModule();

		// --- Inherited from OVT_InfantrySpawningDeploymentModule, all thirteen.
		clone.m_sModuleName = m_sModuleName;
		clone.m_sGroupType = m_sGroupType;
		clone.m_iMinGroupCount = m_iMinGroupCount;
		clone.m_iMaxGroupCount = m_iMaxGroupCount;
		clone.m_bScaleByTownSize = m_bScaleByTownSize;
		clone.m_fSpawnRadius = m_fSpawnRadius;
		clone.m_iCostPerGroup = m_iCostPerGroup;
		clone.m_bAllowReinforcement = m_bAllowReinforcement;
		clone.m_iReinforcementCost = m_iReinforcementCost;
		clone.m_bSpawnAtNearestBase = m_bSpawnAtNearestBase;
		clone.m_bReinforceFromNearestBase = m_bReinforceFromNearestBase;
		clone.m_eImportance = m_eImportance;
		clone.m_bSnapToRoad = m_bSnapToRoad;

		// --- This module's own eleven. The provider is shared rather than deep-copied, exactly as the
		//     placed-infantry module shares its placement provider: a provider is a stateless answerer.
		clone.m_Source = m_Source;
		clone.m_fWalkThresholdDistance = m_fWalkThresholdDistance;
		clone.m_sTruckVehicleType = m_sTruckVehicleType;
		clone.m_sTruckCrewGroup = m_sTruckCrewGroup;
		clone.m_fLZStandoffDistance = m_fLZStandoffDistance;
		clone.m_fStuckSpeedThreshold = m_fStuckSpeedThreshold;
		clone.m_iStuckTicks = m_iStuckTicks;
		clone.m_fArrivalRadius = m_fArrivalRadius;
		clone.m_iTruckCostOverride = m_iTruckCostOverride;
		clone.m_bWalkWhenInsertionRefused = m_bWalkWhenInsertionRefused;
		clone.m_bTransportIsObserver = m_bTransportIsObserver;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	void PrintInsertionDebugInfo()
	{
		Print(string.Format("Insertion Module: %1", DescribeSelf()));
		Print(string.Format("  State: %1", typename.EnumToString(OVT_EInsertionState, m_eState)));
		Print(string.Format("  Source: %1  LZ: %2", m_vSource.ToString(), m_vLZ.ToString()));
		string reservation = "no";
		if (m_bReserved)
			reservation = "yes";
		Print(string.Format("  Holds a convoy slot: %1", reservation));
		Print(string.Format("  Crew handle: %1  Stuck ticks: %2  Return ticks: %3",
			m_iCrewHandle.ToString(), m_iStuckTicksElapsed.ToString(), m_iReturnTicksElapsed.ToString()));
		string crewKey = m_sCrewOwnerKey;
		if (crewKey.IsEmpty())
			crewKey = "not minted - this insertion has never asked for a crew";
		Print(string.Format("  Crew owner key: %1", crewKey));
		string abandoned = "no";
		if (m_bTruckAbandoned)
			abandoned = "yes";
		Print(string.Format("  Transport abandoned: %1  Ticks towards collection: %2 of %3",
			abandoned, m_iAbandonedTicksElapsed.ToString(), STUCK_TRUCK_TIMEOUT_TICKS.ToString()));
		Print(string.Format("  Owned waypoints: %1  Riders paired: %2",
			m_aOwnedWaypoints.Count().ToString(), m_mRiderIsCrew.Count().ToString()));
		Print(string.Format("  Uncrewed ticks: %1 of %2  At the wheel: %3",
			m_iUncrewedTicksElapsed.ToString(), m_iStuckTicks.ToString(), CrewIsAtTheWheel().ToString()));

		// The same line the stall path prints, on demand. See DescribeCrewLiveness() for how to read it.
		Print(string.Format("  Crew liveness: %1", DescribeCrewLiveness()));
	}
}
