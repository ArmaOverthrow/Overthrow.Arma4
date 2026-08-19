//------------------------------------------------------------------------------------------------
//! What stage of its journey an insertion is at.
//!
//! ⚠ WALKING IS AN OUTCOME, NOT AN ERROR STATE, and five different things lead to it: the hop was too
//! short to be worth a truck, no convoy slot was free, no transport could be put on the road, the
//! transport stopped making progress, and the transport was destroyed. Four of those are failures of
//! the drive and none of them is a failure of the INSERTION - in every one of them the force exists,
//! holds the plan it was registered with, and reaches the objective on foot.
//------------------------------------------------------------------------------------------------
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
	FINISHED
}

//------------------------------------------------------------------------------------------------
//! LIVE INSERTION: registers a deployment's force at a place it could plausibly have come FROM, puts
//! it in a truck, drives that truck down real roads to a landing zone short of the objective, drops
//! it, and sends the truck home.
//!
//! ==========================================================================================
//! THE WALK FALLBACK IS THE SPINE OF THIS FILE, NOT ITS ERROR HANDLING.
//! ==========================================================================================
//!
//! Read the class in this order and it makes sense; read it as "a convoy, with some error paths" and
//! it does not. The force is registered FIRST, with a plan that already points at the objective, and
//! is registered whether or not any of the driving works. Everything about the truck is an
//! OPTIMISATION laid on top of a march that would have happened anyway. Five things can take the
//! truck away - a hop too short to bother with, a convoy cap already spent, a missing vehicle prefab,
//! a truck that stopped making progress and a truck that was destroyed - and all five land in exactly
//! the same place: the men are on the ground, they hold the plan they were registered with, and they
//! walk. A path that leaves men in a dead truck is a defect, not a degradation.
//!
//! WHY IT SUBCLASSES THE INFANTRY MODULE rather than sitting beside it. OVT_ReinforcementBehaviorDeploymentModule
//! .GetMissingUnitsCount() casts to OVT_InfantrySpawningDeploymentModule and answers 0 for anything
//! else, so a sibling class would silently never be rebought after a wipe. The same reasoning
//! base-defense-migration recorded in its D6.
//!
//! IT HAS NO IDEA WHAT AN OBJECTIVE IS, AND THAT IS D7. Nothing here resolves, references or names the
//! component that owns a faction's long-term intent; the only place-specific question it asks -
//! "where does this force come from?" - is asked through OVT_DeploymentSourceProvider, and the
//! concurrency cap it respects lives on the deployment manager. A future config that wants a supply
//! run, a reinforcement convoy or a resistance-side insertion authors a provider and a .conf and
//! changes nothing else.
//!
//! WHAT IT OWNS AND WHAT IT BORROWS, because the split is the whole reason the teardown is careful:
//!   OWNED, and leaked if this module forgets them:
//!     - the truck (a plain world entity, deleted at teardown unless a player has made it theirs);
//!     - the crew's registration (a SEPARATE owner key from the passengers' - see EnsureCrew);
//!     - two AIWaypoint entities, one out and one home. ⚠ AIGroup.AddWaypoint() does NOT take
//!       ownership; a leaked waypoint per insertion compounds over a campaign;
//!     - one slot of the deployment manager's per-faction convoy cap. ⚠ A leaked reservation is
//!       PERMANENT: nothing can hand it back, and enough of them stop the faction driving for the
//!       rest of the campaign. Every exit path below releases.
//!   BORROWED from the base class, and never torn down here:
//!     - the passenger groups. They are the deployment's actual force, registered under the base
//!       class's owner key, reclaimed by it and released by its OnCleanup.
//!
//! ⚠ A CONVOY IS NEVER RESUMED ACROSS A LOAD. Vehicles are not persisted, so a save taken mid-drive
//! comes back with the force somewhere along a road and no truck under it. Re-spawning a truck at the
//! source and expecting men who are three kilometres away to board it is not a restore, it is a
//! second insertion. A restored deployment therefore walks, always - which is the fallback, working.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_InsertionSpawningDeploymentModule : OVT_InfantrySpawningDeploymentModule
{
	//! WHERE THE FORCE COMES FROM - the modder seam. Left unauthored, this module refuses to register
	//! anything at all, which is deliberate: "reinforcements arrive from somewhere real" is the rule it
	//! exists to enforce, and a null provider means nobody has said where that is.
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

	//! ⚠ A TICK HERE IS ONE DEPLOYMENT UPDATE, i.e. roughly TEN SECONDS - so the default is about a
	//! minute of complete immobility. It is deliberately generous, because the cost of calling a convoy
	//! stuck is a force dumped in open country, and because a truck legitimately stands still while its
	//! crew materialises through the AI spawn queue and boards.
	[Attribute(defvalue: "6", desc: "Consecutive update ticks (about 10 s each) below the speed threshold before the force dismounts and walks. 0 disables the stuck test entirely")]
	int m_iStuckTicks;

	[Attribute(defvalue: "40", desc: "How close to the landing zone counts as arrived, in metres")]
	float m_fArrivalRadius;

	//! ⚠ CHARGED WHETHER OR NOT A TRUCK EVER REACHES A ROAD, and it has to be. A deployment's price is
	//! computed from the CONFIG TEMPLATE before the deployment exists, months of campaign time before
	//! this module knows whether the hop is long enough, whether a slot is free or whether the faction
	//! even has a prefab for the vehicle. It is a budgeted cost, not a receipt.
	[Attribute(defvalue: "40", desc: "Added to this module's resource cost to cover the transport. Budgeted at creation time, so it is charged even on deployments that end up walking")]
	int m_iTruckCostOverride;

	//! What to do when the faction's convoy cap is already spent. TRUE walks the force in now; FALSE
	//! leaves the whole insertion undecided and tries again on the next convergence, which is right for
	//! a config where arriving by truck matters more than arriving soon.
	[Attribute(defvalue: "1", desc: "When every convoy slot is taken: TRUE walks the force in immediately, FALSE waits for a slot and retries on the next update")]
	bool m_bWalkWhenInsertionRefused;

	//! Where a crew is registered relative to its truck, in metres along X - the same offset the
	//! vehicle module uses, and for the same reason: close enough to board, not inside the geometry.
	static const float CREW_SPAWN_OFFSET_M = 5;

	//! Where passengers are registered relative to the truck. Further out than the crew so the two
	//! groups do not materialise inside one another.
	static const float PASSENGER_SPAWN_OFFSET_M = 9;

	//! AI spawn-budget tier for the transport crew. NEVER leave a registration unstamped: an unstamped
	//! group inherits vanilla's LOW tier, is capped at half the AI budget and is evicted first - which
	//! for a crew means a truck with nobody in it.
	static const SCR_EAISpawnImportance CREW_IMPORTANCE = SCR_EAISpawnImportance.NORMAL;

	//! ALWAYS MATERIALISED. Both the crew and, while they are riding, the passengers register at this
	//! ring. It is not a luxury and it is not tunable:
	//!   - a DORMANT crew holding a route plan is walked in a straight line across the map by the
	//!     movement tick while its truck stays parked, and materialises kilometres from its own
	//!     vehicle (OVT_VehicleSpawningDeploymentModule's header records the same finding);
	//!   - a DORMANT passenger group cannot be seated, because there is nobody to seat. A truck driving
	//!     through country with no player near it would arrive empty and the whole insertion would be
	//!     theatre.
	//! The passengers are dropped back to the global ring the moment they are on the ground - see
	//! RestoreGlobalSpawnRing(), which is the one place in this file that writes to a core record.
	static const int RIDING_SPAWN_DISTANCE = 100000;

	//! Mirrors OVT_VirtualizationManagerComponent's own m_fDespawnHysteresis default. It is protected
	//! there with no getter, and this module needs it only to re-stamp ONE transient policy change on
	//! groups it put on a truck; a server that dialled the core value elsewhere gets a slightly
	//! different anti-thrash band on those groups until they are next re-registered, which is
	//! invisible. Not worth widening a frozen API for.
	static const float DESPAWN_HYSTERESIS = 1.15;

	//! Appended to the module name to key the CREW's registration. ⚠ THE CREW MUST NOT SHARE THE
	//! PASSENGERS' OWNER KEY: the base class rebuilds its handle list from FindGroupsByOwner on every
	//! convergence, so a crew under the same key would be reclaimed as a passenger, counted towards the
	//! force size, driven off in its own truck and released by the wrong teardown.
	static const string CREW_KEY_SUFFIX = "crew";

	//! How many update ticks an empty truck gets to reach home before it is released where it stands.
	//! A const rather than an attribute: nobody tunes "how long before we stop caring about an empty
	//! truck", and one more authored field is one more thing CloneModule can drop.
	static const int RETURN_TIMEOUT_TICKS = 60;

	//! How far the source may be from a base marker for that base's authored vehicle spawns to be
	//! treated as ITS spawns. 250 m is OVT_DeploymentManagerComponent.BASE_CLASSIFICATION_RADIUS - the
	//! radius the rest of the framework uses to decide a position IS a base, and the same one
	//! OVT_RoadSlotOverwatchPlacementProvider asks the base controller at. The shipped provider answers
	//! a base marker's exact location, so this is only really a guard against a MODDED provider (a
	//! forward base, a port) borrowing the markers of a base it merely stands near.
	static const float SOURCE_BASE_RADIUS_M = 250;

	//! How much room a transport needs at an authored marker before that marker counts as free. Wider
	//! than the vehicle manager's car-sized 3 m default because the thing being placed is a truck, and
	//! because these markers are SHARED with the vehicle-patrol deployments - a patrol vehicle sitting
	//! on one is the normal case this exists to skip past.
	static const float MARKER_CLEARANCE_M = 6;

	//! How long the fallback march plan pauses at the objective before walking to it again. Long enough
	//! to be silent, short enough that a group displaced by a fight walks back. Mirrors the shape of
	//! the parked-recruit hold loop.
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

	protected Vehicle m_Truck;

	protected int m_iCrewHandle;

	protected bool m_bReserved;

	protected int m_iStuckTicksElapsed;

	protected vector m_vLastTruckPosition;

	//! False until the truck has been observed once. Without it the first tick would measure a speed
	//! against a zero vector - a "distance" of tens of thousands of metres, or, if it were clamped, a
	//! free stuck tick out of a limit that may only be three.
	protected bool m_bHaveLastTruckPosition;

	protected int m_iReturnTicksElapsed;

	//! Every waypoint this module spawned. AIGroup.AddWaypoint() does NOT take ownership, so these are
	//! deleted by hand on every exit. Entity handles null themselves out when the engine deletes one.
	protected ref array<AIWaypoint> m_aOwnedWaypoints;

	//! Group ENTITY id -> whether that group is the CREW (true) or a PASSENGER (false). Keyed on the
	//! entity because that is all the engine's per-member callback gives us, and rebuilt from scratch
	//! every session because a restored group's entity id is a new one.
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
	//! The force, plus the transport it is budgeted to arrive in. See m_iTruckCostOverride for why the
	//! transport is charged even on an insertion that ends up walking.
	override int GetResourceCost()
	{
		return super.GetResourceCost() + m_iTruckCostOverride;
	}

	//------------------------------------------------------------------------------------------------
	// The convergence
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Brings this insertion up to what it wants: a decision, a transport if one is warranted, and the
	//! force itself. ALWAYS SAFE TO CALL, any number of times, in any order.
	//!
	//! ⚠ THE ORDER OF THE THREE STEPS IS LOAD-BEARING.
	//!   1. DECIDE FIRST, because the decision picks both the anchor the force is registered AT and the
	//!      proximity ring it is registered ON, and neither can be changed after the fact without
	//!      re-registering - which would throw away the survivor mask.
	//!   2. THE TRANSPORT SECOND, before the force exists. Passengers are registered beside the truck
	//!      so they can be seated the instant they materialise; with no truck yet there is nothing to
	//!      register beside. This is also what makes the failure ordering right - a truck that cannot
	//!      be spawned diverts to WALKING while the force is still unregistered, so it is then
	//!      registered spread around its source on the global ring, exactly as a walking force should
	//!      be.
	//!   3. THE FORCE LAST, through the base class, which reclaims before it registers and therefore
	//!      converges rather than stacking a second force on a restored one.
	//!
	//! ⚠ THIS IS NOT A PERIODIC METHOD, AND KNOWING THAT IS LOAD-BEARING. Nothing in the framework ticks
	//! it: it is reached from the deployment's ONE activation and from the manager's records-restored
	//! fan-out, and that is all (grep `EnsureGroups()` - there is no third caller). Everything this
	//! module has to keep doing while a convoy runs - seating late arrivals, watching for arrival, the
	//! stall test - therefore lives in OnUpdate(), and the ONE case that needs the convergence itself to
	//! run again (an insertion with nowhere yet to come from) is re-driven from there explicitly. Writing
	//! a retry here and assuming something would call it again is the mistake this note exists to stop.
	override void EnsureGroups()
	{
		if (!m_ParentDeployment)
			return;

		if (m_eState == OVT_EInsertionState.UNDECIDED)
		{
			DecideInsertion();

			// ⚠ STILL UNDECIDED MEANS REGISTER NOTHING, AND IT HAS TO BE SAID EXPLICITLY. There are two
			// ways to get here and they look identical from below: there is nowhere for the force to
			// come from, or the config would rather wait for a convoy slot than walk. Falling through
			// would put the force on the ground AT ITS SOURCE with a march plan - i.e. it would walk -
			// which is exactly the decision that has not been made yet. (The first case happens to be
			// safe anyway, because ResolveSpawnAnchor refuses with no source; the second is not, and
			// relying on the accident would be a trap for whoever changes the anchor.)
			if (m_eState == OVT_EInsertionState.UNDECIDED)
				return;
		}

		if (m_eState == OVT_EInsertionState.DRIVING)
			EnsureConvoy();

		super.EnsureGroups();

		if (m_eState == OVT_EInsertionState.DRIVING)
		{
			SeatEveryone();
			return;
		}

		// EVERYTHING THAT IS NOT RIDING BELONGS ON THE ORDINARY PROXIMITY RING, and this is the sweep
		// that guarantees it however the state got here - including the force of a deployment restored
		// mid-drive, whose records came back out of the save still carrying the riding ring. Run
		// unconditionally rather than behind a latch on purpose: RestoreGlobalSpawnRing() answers
		// immediately for a group that is already on the global ring, which is every group on almost
		// every pass, and a latch here has to be cleared correctly on five different paths to be
		// right - one missed clear is a squad materialised for the rest of the campaign.
		DropPassengersToGlobalRing();
	}

	//------------------------------------------------------------------------------------------------
	//! Chooses between a march and a convoy, once, and commits to a source and a landing zone.
	//!
	//! ⚠ EVERY BRANCH THAT IS NOT "DRIVE" IS A BRANCH THAT STILL DELIVERS THE FORCE. The only outcome
	//! that registers nothing at all is "there is nowhere for this force to come from", and that one
	//! stays UNDECIDED rather than settling, so a faction that takes a base back later gets its
	//! insertion then.
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
		m_bHaveLastTruckPosition = false;
		m_iReturnTicksElapsed = 0;

		int driveMetres = Math.Round(separation);

		Print(string.Format("[Overthrow] Insertion '%1': driving %2 m from %3 to a landing zone at %4",
			DescribeSelf(), driveMetres.ToString(), m_vSource.ToString(), m_vLZ.ToString()), LogLevel.NORMAL);
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
	//!
	//! THIS IS THE ONE FAILURE THAT REGISTERS NOTHING, and it is the rule the module exists to enforce:
	//! a force that appears at its objective out of thin air is exactly what live insertion replaces.
	//! It is logged at WARNING and named, because "the enemy never reinforces town X" has to have an
	//! explanation in the log rather than a repro.
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
	//!
	//! THE SNAP IS A PREFERENCE, NOT A REQUIREMENT. A truck dropped on a road is a truck that can drive
	//! away again; a truck dropped on the raw line point may be in a field. But the raw point is always
	//! reachable-ish and the road search is bounded, so no road within range simply uses the line
	//! point, surface-clamped.
	//! \param[in] source Where the convoy starts.
	//! \param[in] target The objective.
	//! \return A landing zone at ground height.
	protected vector ResolveLandingZone(vector source, vector target)
	{
		vector point = OVT_InsertionGeometry.LZPointOnLine(source, target, m_fLZStandoffDistance);

		vector roadPosition;
		vector roadAngles;
		if (OVT_WorldUtils.FindNearestRoadSpawn(point, OVT_WorldUtils.ROAD_SPAWN_MAX_DISTANCE, roadPosition, roadAngles))
			return roadPosition;

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
	//!
	//! Each of the two things it can fail to build diverts to the march, and the divert happens BEFORE
	//! the force is registered (see EnsureGroups' step ordering), so the force is then registered as a
	//! walking force rather than as a stranded one.
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

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! WHERE THE TRUCK APPEARS: an authored marker at the source base if there is a free one, and the
	//! nearest road if there is not.
	//!
	//! ⚠ AN AUTHORED MARKER BEATS THE ROAD SNAP, ALWAYS, AND THAT IS THE WHOLE POINT OF THE MARKERS. The
	//! road snap answers "somewhere a vehicle could drive away from" with no knowledge of what is
	//! standing there: at a base it lands on the access road, which may be inside the wire, across a
	//! gate or nose-first into a wall. A designer who placed an OVT_VehiclePatrolSpawn has already
	//! answered the question properly, INCLUDING THE FACING - that is what the arrow in Workbench is.
	//!
	//! Everything below the marker branch is byte-for-byte the behaviour that shipped before markers
	//! were consulted, and it is reached on all four of: no base near the source, a base with no
	//! markers authored, every marker occupied, and no vehicle manager to ask about occupancy.
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
	//!
	//! ⚠ NO SECOND WORLD QUERY FOR THE MARKERS. The base controller already swept baseRange for them at
	//! base init and cached them; this reads that cache through CollectVehiclePatrolSpawns(). The only
	//! query it runs is the occupancy test, once per candidate.
	//!
	//! THE CHOICE IS DETERMINISTIC - nearest free marker to the source, ties to the lower index - and
	//! deliberately NOT the base controller's GetRandomVehiclePatrolSpawn(). See
	//! OVT_InsertionGeometry.ChooseSpawnMarker, which is where the decidable part of this lives so it
	//! can be asserted without a world.
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
		angles = markers[chosen].GetAngles();

		Print(string.Format("[Overthrow] Insertion '%1': transport spawning on authored vehicle spawn %2 of %3 at %4",
			DescribeSelf(), (chosen + 1).ToString(), markers.Count().ToString(), position.ToString()), LogLevel.VERBOSE);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Registers the transport crew under its OWN owner key, beside the truck, always materialised.
	//!
	//! ⚠ A NULL PLAN, DELIBERATELY, AND NOT THE BEHAVIOUR MODULES' ONE. ResolveVirtualPlan() asks the
	//! deployment's behaviour modules what a group of this deployment should do, and for the FORCE that
	//! is exactly right - but the crew is not the force. A harassment plan handed to the crew would
	//! send the truck into the town centre and hold it there. The crew's orders are issued by this
	//! module as ordinary waypoints it owns and deletes.
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

			m_iCrewHandle = virtualization.RegisterGroup(OWNER_SYSTEM, crewKey, factionKey, m_sTruckCrewGroup,
				crewPosition, null, RIDING_SPAWN_DISTANCE, CREW_IMPORTANCE);

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
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Gives the crew a MOVE order to the landing zone, once.
	//!
	//! Idempotent by construction: it does nothing while this module already owns a waypoint, so the
	//! convergence can run every ten seconds for the whole drive without stacking orders.
	protected void IssueDriveOrder()
	{
		if (!m_aOwnedWaypoints.IsEmpty())
			return;

		IssueCrewMove(m_vLZ);
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces whatever the crew was doing with a single MOVE.
	//!
	//! ⚠ EVERY WAYPOINT SPAWNED HERE IS REMEMBERED AND DELETED BY HAND. AIGroup.AddWaypoint() does not
	//! take ownership - a waypoint is an ordinary world entity - so the alternative is one leaked
	//! entity per leg per insertion, forever.
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

		// ⚠ THE RETRY, AND THE ONLY PLACE IT CAN LIVE. EnsureGroups() is called once, at activation - see
		// its header - so an insertion that could not resolve an origin on that one pass would otherwise
		// stay undecided and register nothing for the rest of the campaign. "A faction that has lost
		// every base is not a permanent failure, it is a retry" is only true because of this line, and so
		// is "the config would rather wait for a convoy slot than walk". Both states are transient and
		// rare; a settled insertion never reaches this.
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
	//!
	//! THE ORDER OF THE TESTS IS THE ORDER OF THEIR SEVERITY, and it matters. Losing the truck or the
	//! crew ends the drive wherever it happens to be; ARRIVING beats being stuck (a truck standing
	//! still on its own landing zone is finished, not stranded - see OVT_InsertionGeometry.IsStuck);
	//! and only then is progress judged.
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

		vector truckPosition = m_Truck.GetOrigin();
		float distanceToLZ = vector.Distance(truckPosition, m_vLZ);

		if (OVT_InsertionGeometry.HasArrived(distanceToLZ, m_fArrivalRadius))
		{
			CompleteInsertion();
			return;
		}

		// ⚠ THE FIRST OBSERVATION IS NOT A MEASUREMENT AND MUST NOT COST A STALL. There is no previous
		// position to compare against, so the honest answer is "no reading yet" rather than "no speed" -
		// and with a stall limit that may be as low as three, a free stall on the very tick the convoy is
		// still boarding is a quarter of the budget spent on nothing.
		//
		// ⚠ deltaTime IS THE NOMINAL 10 s, NOT THE REAL INTERVAL. The deployment's update timer is
		// deliberately staggered by 0.8-1.2x, and its caller passes the constant rather than the elapsed
		// time, so this speed carries up to 20% of error. That is fine for the only question being asked
		// of it - "did this thing move at all" - and would not be for anything finer.
		if (m_bHaveLastTruckPosition)
		{
			float speed = OVT_InsertionGeometry.SpeedFromTravel(m_vLastTruckPosition, truckPosition, deltaTime / 1000.0);

			m_iStuckTicksElapsed = OVT_InsertionGeometry.AdvanceStuckTicks(speed, m_fStuckSpeedThreshold, m_iStuckTicksElapsed);

			if (OVT_InsertionGeometry.IsStuck(speed, m_fStuckSpeedThreshold, m_iStuckTicksElapsed, m_iStuckTicks, distanceToLZ, m_fArrivalRadius))
			{
				int shortfallMetres = Math.Round(distanceToLZ);

				DismountAndWalk(string.Format("its transport stopped making progress %1 m short of the landing zone",
					shortfallMetres.ToString()));
				return;
			}
		}

		m_vLastTruckPosition = truckPosition;
		m_bHaveLastTruckPosition = true;

		// Anybody who materialised, fell out or was pulled out mid-drive gets back aboard. Cheap: the
		// seating call refuses anyone already in a compartment, which is almost everybody almost always.
		SeatEveryone();
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

		if (OVT_InsertionGeometry.HasArrived(vector.Distance(m_Truck.GetOrigin(), m_vSource), m_fArrivalRadius))
		{
			ReleaseConvoy("its transport is home", true);
			m_eState = OVT_EInsertionState.FINISHED;
			return;
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
	//! THE SUCCESS PATH: the force is down at the landing zone and the truck goes home.
	//!
	//! The force needs no new orders. It has held a plan pointing at the objective since the moment it
	//! was registered - the behaviour module's, or this module's fallback march - which is precisely
	//! why every failure path below can simply open the doors and walk away.
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

		IssueCrewMove(m_vSource);

		Print(string.Format("[Overthrow] Insertion '%1' delivered %2 group(s) at %3; its transport is going home",
			DescribeSelf(), delivered.ToString(), m_vLZ.ToString()), LogLevel.NORMAL);
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
	//!
	//! Everything the force needs it already has. It is registered, it is alive, it holds a plan that
	//! points at the objective, and the only thing between it and that plan is a door. So: open the
	//! door, put the men back on the ordinary proximity ring, hand every borrowed thing back, and let
	//! them walk. Logged at NORMAL rather than WARNING - this is the system working.
	//! \param[in] reason What ended the drive, for the log line.
	protected void DismountAndWalk(string reason)
	{
		DisembarkPassengers();

		// The truck is NOT deleted here. A stuck one is a landmark and a lootable, a destroyed one is
		// already a wreck, and either way it is released with everything else when the deployment ends.
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

		Print(string.Format("[Overthrow] Insertion '%1' is on foot: %2", DescribeSelf(), reason), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	// Teardown - see the class header's owned/borrowed split
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Hands back everything this module borrowed for a convoy. IDEMPOTENT and safe on a module that
	//! never had one.
	//!
	//! ⚠ THIS IS THE ONLY PLACE THE RESERVATION IS RELEASED OUTSIDE CompleteInsertion(), AND EVERY
	//! NON-ARRIVAL EXIT MUST REACH IT. The manager's counter cannot be recovered by anything else - it
	//! is not persisted, nothing sweeps it, and a slot lost to a convoy that ended quietly is a slot
	//! the faction never gets back.
	//! \param[in] reason What ended the convoy, for the log line. Empty logs nothing.
	//! \param[in] deleteTruck Whether the transport should be taken away as well.
	protected void ReleaseConvoy(string reason, bool deleteTruck)
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();

		SCR_AIGroup crew;
		if (virtualization && m_iCrewHandle != -1)
			crew = virtualization.GetGroup(m_iCrewHandle);

		ClearOwnedWaypoints(crew);

		UnsubscribeRiders();

		if (virtualization && m_iCrewHandle != -1)
		{
			// UnregisterGroup respects held members: a crewman still sitting in the truck retires the
			// group in place rather than having it deleted out from under him.
			virtualization.UnregisterGroup(m_iCrewHandle);
		}

		m_iCrewHandle = -1;

		if (deleteTruck)
			ReleaseTruck();

		ReleaseReservation();

		m_bHaveLastTruckPosition = false;
		m_iStuckTicksElapsed = 0;

		if (!reason.IsEmpty())
			Print(string.Format("[Overthrow] Insertion '%1': convoy stood down - %2", DescribeSelf(), reason), LogLevel.VERBOSE);
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
	//!
	//! The two vetoes are the ones OVT_VehicleSpawningDeploymentModule settled on, for the same
	//! reasons: a player owner uid is stamped the moment a player drives an unowned vehicle, and an
	//! occupant that is a player or a player's recruit covers the passenger seat the claim path never
	//! fires for.
	protected void ReleaseTruck()
	{
		if (!m_Truck)
			return;

		string veto = TruckDeletionVeto(m_Truck);
		if (veto != "")
		{
			Print(string.Format("[Overthrow] Insertion '%1': transport left standing - %2", DescribeSelf(), veto), LogLevel.NORMAL);
			m_Truck = null;
			return;
		}

		SCR_EntityHelper.DeleteEntityAndChildren(m_Truck);
		m_Truck = null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] vehicle The transport to judge.
	//! \return An empty string when it is safe to delete, or the reason it is not.
	protected string TruckDeletionVeto(notnull Vehicle vehicle)
	{
		OVT_PlayerOwnerComponent owner = OVT_PlayerOwnerComponent.Cast(vehicle.FindComponent(OVT_PlayerOwnerComponent));
		if (owner && !owner.GetPlayerOwnerUid().IsEmpty())
			return "a player owns it";

		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(BaseCompartmentManagerComponent)
		);

		if (!compartments)
			return "";

		PlayerManager players = GetGame().GetPlayerManager();

		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);

		foreach (BaseCompartmentSlot slot : slots)
		{
			if (!slot)
				continue;

			IEntity occupant = slot.GetOccupant();
			if (!occupant)
				continue;

			if (players && players.GetPlayerIdFromControlledEntity(occupant) > 0)
				return "a player is riding in it";

			OVT_PlayerOwnerComponent occupantOwner = OVT_PlayerOwnerComponent.Cast(
				occupant.FindComponent(OVT_PlayerOwnerComponent)
			);

			if (occupantOwner && !occupantOwner.GetPlayerOwnerUid().IsEmpty())
				return "a player's recruit is riding in it";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The deployment is over.
	//!
	//! ⚠ THE CONVOY IS WOUND UP BEFORE super, AND THE ORDER IS NOT COSMETIC. The base class releases
	//! the PASSENGERS - its own owner key - and knows nothing about the crew's separate registration,
	//! the truck, the waypoints or the reservation. Running it first would clear m_aHandles and leave
	//! this module tearing down against an empty list.
	override protected void OnCleanup()
	{
		ReleaseConvoy("the deployment is over", true);
		m_eState = OVT_EInsertionState.FINISHED;

		super.OnCleanup();
	}

	//------------------------------------------------------------------------------------------------
	//! One registered group has been wiped out.
	//!
	//! ⚠ THE CREW IS NOT PART OF THE FORCE. It is registered under a different owner key, is not in
	//! m_aHandles and must never count towards this module's elimination - a deployment whose truck
	//! crew was shot has not lost its infantry. So the crew's wipe is intercepted here and the base
	//! class never sees it; everything else is passed straight through.
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
	//!
	//! ⚠ RETURNING FALSE ABORTS THE REGISTRATION, AND THAT IS THE POINT. This is the one guarantee the
	//! module exists to make: a force with nowhere to have come from does not appear at its objective.
	//! The base class's own fromNearestBase flag is deliberately ignored - the provider is a strictly
	//! better answer to the same question (see OVT_NearestControlledBaseSourceProvider's header for why
	//! the base class's version goes blind on a contested map).
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
		if (m_eState == OVT_EInsertionState.DRIVING && m_Truck)
			return m_Truck.GetOrigin() + Vector(PASSENGER_SPAWN_OFFSET_M, 0, 0);

		return super.ResolveSpawnPosition(anchor, index);
	}

	//------------------------------------------------------------------------------------------------
	//! THE RING: always-materialised while riding, ordinary otherwise. See RIDING_SPAWN_DISTANCE for
	//! why a dormant passenger cannot be seated.
	//! \return The spawnDistanceOverride to register with.
	override protected int ResolveRegistrationSpawnDistance()
	{
		if (m_eState == OVT_EInsertionState.DRIVING)
			return RIDING_SPAWN_DISTANCE;

		return super.ResolveRegistrationSpawnDistance();
	}

	//------------------------------------------------------------------------------------------------
	//! THE PLAN: whatever the behaviour modules want, and failing that, a march onto the objective.
	//!
	//! ⚠ THE FALLBACK IS WHAT MAKES THE WALK FALLBACK REAL RATHER THAN NOMINAL. A group registered with
	//! no plan is a garrison, and a garrison registered at the SOURCE is a force that stands at the
	//! base it set out from for the rest of the campaign - which is exactly the "men left behind"
	//! outcome every other line in this file is written to prevent. A deployment with an opinionated
	//! behaviour module never reaches this; one without it still arrives.
	//!
	//! It CYCLES, with a long pause, for the same reason the parked-recruit hold loop does: the group
	//! walks to the objective, stands there quietly, and walks back to it if a fight displaced it.
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
	//!
	//! ⚠ A RECLAIMED GROUP IS A NEW ENTITY AFTER A LOAD - core re-creates it from its own payload with
	//! a fresh EntityID - so anything subscribed on the old one is gone and the pairing has to be made
	//! again here, which is the same reason the base class re-tags for the Game Master at this point.
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
	//!
	//! ⚠ PER MEMBER, NOT PER GROUP. A group's whole-group init event fires only when the group fills
	//! COMPLETELY, which under AI budget pressure may never happen, and a core-registered group fills
	//! progressively through the engine's spawn queue - so a whole-group hook would frequently seat
	//! nobody at all.
	//!
	//! ⚠ REMOVE THEN INSERT. ScriptInvoker does not de-duplicate and a convergence runs many times per
	//! session; a plain Insert would try to seat every arriving man once per pass that had ever run.
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
	//!
	//! ⚠ ONE PARAMETER. SCR_AIGroup's own doc comment claims the invoker passes (group, agent); the
	//! actual invocation passes the AGENT alone, and the group is recovered from it.
	//! \param[in] agent The arriving member.
	protected void OnRiderAgentAdded(AIAgent agent)
	{
		if (!agent)
			return;

		if (m_eState != OVT_EInsertionState.DRIVING)
			return;

		AIGroup parent = agent.GetParentGroup();
		if (!parent)
			return;

		EntityID groupId = parent.GetID();
		if (!m_mRiderIsCrew.Contains(groupId))
			return;

		if (!m_Truck)
		{
			m_mRiderIsCrew.Remove(groupId);
			return;
		}

		SeatRider(m_Truck, agent, m_mRiderIsCrew.Get(groupId));
	}

	//------------------------------------------------------------------------------------------------
	//! Seats everybody who is on their feet right now: the crew, then the force.
	protected void SeatEveryone()
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
	//!
	//! CREW TAKE THE DRIVER'S SEAT, THEN A TURRET, THEN CARGO - the order the vehicle module uses,
	//! applied one man at a time, so the first arrival finds the pilot compartment free and takes it.
	//! PASSENGERS ONLY EVER TAKE CARGO: a passenger in the driver's seat is a passenger who drives the
	//! truck somewhere nobody asked for, and a passenger in a turret is a passenger who starts a
	//! firefight the convoy exists to avoid.
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
			return false;

		SCR_BaseCompartmentManagerComponent compartmentManager = SCR_BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(SCR_BaseCompartmentManagerComponent)
		);

		if (!compartmentManager)
			return false;

		if (isCrew)
		{
			if (FillCompartment(compartmentManager, agent, ECompartmentType.PILOT))
				return true;

			if (FillCompartment(compartmentManager, agent, ECompartmentType.TURRET))
				return true;
		}

		return FillCompartment(compartmentManager, agent, ECompartmentType.CARGO);
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
	//!
	//! THE CREW IS DELIBERATELY LEFT ABOARD. It still has a truck to drive home, and on the paths where
	//! it does not - a wreck, a wipe - it is unregistered moments later and core retires it in place.
	//! Teleporting men out of a vehicle that is about to be released would only scatter them.
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
	//!
	//! TELEPORT RATHER THAN THE ANIMATED EXIT, which is the call vanilla's own AI makes when it needs a
	//! man out of a vehicle now: an animated dismount is interruptible, takes seconds per man and can
	//! leave a whole squad half-out when the vehicle is destroyed under them.
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
	//!
	//! ================== THE ONE PLACE THIS FILE WRITES TO A CORE RECORD ======================
	//!
	//! ⚠ AND IT HAS TO. A group is registered with its ring, the ring is stamped onto the engine
	//! lifecycle policy at registration, and the registry has no setter for it - the virtualization
	//! core's API is frozen for this feature and its own re-stamp is protected. Re-registering instead
	//! would throw away the survivor mask, which is the whole point of the registry.
	//!
	//! ⚠ AND BOTH HALVES ARE NEEDED. The engine policy is what decides whether these men despawn when
	//! the last player leaves; the RECORD is what a save writes and what a load re-stamps. Fixing only
	//! the policy delivers a force that behaves correctly this session and comes back permanently
	//! materialised, forever, on the next load - one squad of the AI budget spent on men nobody can
	//! see, per insertion, for the rest of the campaign.
	//!
	//! Everything about the values is taken from core rather than assumed: the ring comes back out of
	//! GetSpawnDistance() (which resolves the record we just corrected against the server's configured
	//! global) and the anti-thrash band from core's own arithmetic. Only the hysteresis multiplier is a
	//! local constant - see DESPAWN_HYSTERESIS.
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
	//! ⚠ ASKED OF THE SURVIVOR MASK, NEVER OF AN AGENT COUNT. A dormant or spawn-queued group reports
	//! zero agents while being perfectly alive, and a convoy that dismounted its passengers every time
	//! its crew went briefly dormant would never deliver anybody.
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
	//! The owner key the CREW is registered under. See CREW_KEY_SUFFIX for why it must differ from the
	//! passengers'.
	//! \return The key, or an empty string when there is no deployment to key against.
	protected string GetCrewOwnerKey()
	{
		return BuildOwnerKey(m_sModuleName + CREW_KEY_SUFFIX);
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
	//!
	//! ⚠ CloneModule IS NOT CHAINED - it builds a fresh instance and copies BY HAND - so the thirteen
	//! lines from OVT_InfantrySpawningDeploymentModule have to be repeated here verbatim, and anything
	//! appended there has to be appended here as well. A forgotten line does not warn, does not log and
	//! does not fail to parse: it ships the CLASS DEFAULT on every deployment, forever. That is how
	//! m_fMaxCruiseSpeed was lost on the vehicle module for a whole release.
	//!
	//! What a dropped line would cost here specifically: drop m_Source and the module registers NOTHING
	//! AT ALL, silently, because it refuses to spawn a force from thin air; drop m_sTruckVehicleType
	//! and every insertion in the campaign walks; drop m_fArrivalRadius and every convoy drives past
	//! its landing zone until the stuck test catches it, which it never will while the truck is moving.
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

		// --- This module's own ten. The provider is shared rather than deep-copied, exactly as the
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
		Print(string.Format("  Owned waypoints: %1  Riders paired: %2",
			m_aOwnedWaypoints.Count().ToString(), m_mRiderIsCrew.Count().ToString()));
	}
}
