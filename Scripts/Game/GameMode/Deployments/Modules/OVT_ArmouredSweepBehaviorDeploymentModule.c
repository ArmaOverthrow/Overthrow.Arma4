//------------------------------------------------------------------------------------------------
//! A HUNTER-KILLER SWEEP: an armed vehicle sent to loiter around wherever the occupying faction most
//! recently lost one of its own, on a bounded clock.
//!
//! SHIPPED BY: Deployment_HunterKillerSweep.conf (occupying/vehicles Phase 6), alongside
//! OVT_MountedForceSpawningDeploymentModule. Nothing ever picks this config through the ordinary
//! evaluator - it is m_bDirectorOnly and its position is a hotspot, not a named location - so the
//! dispatcher that stands it up is OVT_OccupyingFactionManager.TickHunterKiller() (C5: the deployment
//! evaluator's own CollectSeedCandidates/FindDeploymentCandidates only ever produce named-location
//! positions - town / base / port / airfield / radio tower / checkpoint).
//!
//! ==========================================================================================
//! 🔴 ONE CLOCK, NOT TWO (Phase 2's own note, resolved here).
//! ==========================================================================================
//! OVT_MountedForceSpawningDeploymentModule.m_iHoldTicks already latches a bounded hold - but
//! RequestDeploymentCollection() is protected on OVT_BaseBehaviorDeploymentModule and unreachable from a
//! SPAWNING module (Phase 2's own finding, context.md -> Phase 2). Phase 2 left this module a choice:
//! poll IsHoldExpired() from here, or drop m_iHoldTicks from the config and run one clock in this file.
//!
//! THIS FILE RUNS ITS OWN CLOCK. Deployment_HunterKillerSweep.conf authors m_iHoldTicks as 0 - the
//! mounted module's documented "0 = indefinite" default, so its hold never expires on its own - and
//! m_iSweepMinutes below is the only deadline that exists. The alternative (poll IsHoldExpired(), keep
//! m_iHoldTicks authored to match m_iSweepMinutes in the config) was rejected: it would put the SAME
//! duration in two places a tuner could edit independently, converted through two different pieces of
//! arithmetic (the mounted module's raw tick count vs. this file's minutes-to-ticks), with nothing
//! anywhere to notice the two had drifted apart. One clock, authored once, in the file whose job is
//! deciding when the sweep is over.
//!
//! WALKS THE VEHICLE ON PATROL/DEFEND WAYPOINTS, never through BuildVirtualPlan() - the same answer and
//! the same reason OVT_MobileCheckpointBehaviorDeploymentModule gives in its own header
//! (virtualization/core api.md: "a plan is registration-time input only", built once at registration and
//! owned by core from then on). A sweep MOVES, so its orders go on the crew directly, exactly as the
//! checkpoint's IssueDrive() does - just with SpawnPatrolWaypoint()/SpawnDefendWaypoint() in place of
//! SpawnMoveWaypoint(). A point search that finds a road issues a PATROL waypoint there; one that finds
//! none issues a DEFEND waypoint at the hotspot itself, so the vehicle always has an order rather than
//! going idle with nothing to do.
//!
//! ⚠ THE PASSENGERS' REGISTRATION RING IS LEFT ALONE, for D8's reason and no other: a mounted deployment
//! is exempt from OVT_DeploymentManagerComponent.SuppressForcesAroundBattle only while its handles
//! resolve to a ring strictly wider than the global one, and this module never dismounts anybody to
//! change that.
//!
//! ⚠ IT DOES NOTHING AT ALL UNTIL THE FORCE IS HOLDING, polled every update rather than latched once -
//! the same rule and the same reason as the mobile checkpoint's own header: every road to the march ends
//! somewhere other than HOLDING, and a sweep with no vehicle is not a degradation this module can
//! improve on.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_ArmouredSweepBehaviorDeploymentModule : OVT_BaseBehaviorDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;

	[Attribute(defvalue: "400", desc: "How far from the hotspot the vehicle roams while sweeping, in metres")]
	float m_fSweepRadius;

	[Attribute(defvalue: "12", desc: "Minutes the sweep loiters before it reports itself finished and asks to be collected. THE ONLY CLOCK - see the class header")]
	int m_iSweepMinutes;

	[Attribute(defvalue: "90", desc: "Seconds on one patrol point before the vehicle moves to a new one within the sweep radius")]
	float m_fWaypointInterval;

	//------------------------------------------------------------------------------------------------
	// CONSTANTS
	//------------------------------------------------------------------------------------------------

	//! How many bearings around the hotspot are sampled for a road before falling back to a DEFEND
	//! waypoint at the hotspot itself. Coarser than the mobile checkpoint's twelve - a sweep is
	//! deliberately loose, not a road-blocking checkpoint.
	static const int SWEEP_SAMPLES = 8;

	//! How far from a sampled point a road still counts, in metres.
	static const float ROAD_SEARCH_RADIUS_M = 150;

	//! One deployment update, in seconds - mirrors OVT_MobileCheckpointBehaviorDeploymentModule's own
	//! UPDATE_SECONDS. The real interval is staggered 0.8-1.2x, so both clocks here are accurate to
	//! about a fifth of their length.
	static const int UPDATE_SECONDS = 10;

	//! Seconds in a minute, so the sweep clock's conversion is written down once.
	static const int SECONDS_PER_MINUTE = 60;

	//------------------------------------------------------------------------------------------------
	// RUNTIME STATE - none of it authored, none of it persisted, none of it cloned (D9: a restored
	// mounted deployment walks; there is no live sweep to resume).
	//------------------------------------------------------------------------------------------------

	protected bool m_bSweeping;
	protected int m_iTicksSinceWaypoint;
	protected int m_iTicksSweeping;

	//! Every waypoint this module spawned. AIGroup.AddWaypoint() does NOT take ownership - the same rule
	//! and the same trap as the mobile checkpoint's own IssueDrive().
	protected ref array<AIWaypoint> m_aOwnedWaypoints;

	//! Latches the one line explaining that no road could be found around this hotspot, so a sweep stuck
	//! holding at the centre does not print a line every ninety seconds for the rest of its life.
	protected bool m_bNoRoadLogged;

	//------------------------------------------------------------------------------------------------
	void OVT_ArmouredSweepBehaviorDeploymentModule()
	{
		m_aOwnedWaypoints = new array<AIWaypoint>();
	}

	//------------------------------------------------------------------------------------------------
	// The tick
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! One observation of a sweep.
	//! \param[in] deltaTime Milliseconds since the last update of this module. Unused - every clock here
	//!            is counted in updates, not in wall time; see UPDATE_SECONDS.
	override void OnUpdate(int deltaTime)
	{
		super.OnUpdate(deltaTime);

		if (!m_ParentDeployment)
			return;

		OVT_MountedForceSpawningDeploymentModule mounted = ResolveMountedForce();
		if (!mounted)
			return;

		// ⚠ POLLED, NEVER LATCHED. A force that fell back to the march after losing its vehicle has no
		// vehicle to sweep with and no crew to order; leaving a stale patrol waypoint on a group that is
		// now walking is how men end up driving nowhere.
		if (mounted.GetInsertionState() != OVT_EInsertionState.HOLDING)
		{
			if (m_bSweeping)
				StandDownSweep("its force is no longer riding a vehicle");

			return;
		}

		Vehicle vehicle = mounted.GetMountedVehicle();
		if (!vehicle)
			return;

		if (!m_bSweeping)
			BeginSweep();

		TickWaypointClock(mounted);
		TickSweepClock();
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the loiter. The waypoint clock is armed already expired, so the FIRST patrol point is
	//! issued on the very tick the sweep begins rather than after a whole waypoint interval of standing
	//! still.
	protected void BeginSweep()
	{
		m_bSweeping = true;
		m_iTicksSinceWaypoint = WaypointTicks();
		m_iTicksSweeping = 0;
		m_bNoRoadLogged = false;

		OVT_DeploymentLog.Debug(string.Format("[Overthrow] Armoured sweep '%1' is loitering around %2 for %3 minute(s)",
			DescribeSelf(), SweepCentre().ToString(), m_iSweepMinutes.ToString()));
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] mounted The force holding the vehicle.
	protected void TickWaypointClock(notnull OVT_MountedForceSpawningDeploymentModule mounted)
	{
		m_iTicksSinceWaypoint = m_iTicksSinceWaypoint + 1;

		if (m_iTicksSinceWaypoint < WaypointTicks())
			return;

		m_iTicksSinceWaypoint = 0;

		IssueSweepPoint(mounted);
	}

	//------------------------------------------------------------------------------------------------
	//! THE ONLY CLOCK THIS DEPLOYMENT EVER RUNS DOWN. Counts out, then asks OVT_BaseBehaviorDeploymentModule
	//! to collect - which is itself a poll with its own exfiltration hold (nobody may watch a squad
	//! evaporate), so this only ever ARMS the request.
	protected void TickSweepClock()
	{
		// ⚠ 0 IS "SWEEP FOREVER", not "collect immediately" - parity with the mobile checkpoint's own
		// m_iRelocateMinutes zero rule, kept for the same reason: an authored 0 is a deliberate opt-out,
		// never an accident this method should punish by returning the deployment on its first tick.
		if (m_iSweepMinutes <= 0)
			return;

		m_iTicksSweeping = m_iTicksSweeping + 1;

		if (m_iTicksSweeping < SweepTicks())
			return;

		RequestDeploymentCollection("its sweep is over");
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many updates one waypoint interval is worth. At least one.
	protected int WaypointTicks()
	{
		int ticks = Math.Round(m_fWaypointInterval / UPDATE_SECONDS);

		if (ticks < 1)
			return 1;

		return ticks;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many updates the whole sweep is worth. At least one.
	protected int SweepTicks()
	{
		int ticks = (m_iSweepMinutes * SECONDS_PER_MINUTE) / UPDATE_SECONDS;

		if (ticks < 1)
			return 1;

		return ticks;
	}

	//------------------------------------------------------------------------------------------------
	// Waypoints
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Sends the vehicle to a new point within the sweep radius, by giving its CREW a move order.
	//!
	//! ⚠ WHATEVER THE CREW WAS DOING IS DETACHED FIRST, AND DETACHED IS NOT DELETED - the same rule and
	//! the same reason as the mobile checkpoint's own IssueDrive(): the insertion module's landing-zone
	//! order may still be on the queue, and it is not this module's to delete.
	//! \param[in] mounted The force holding the vehicle.
	protected void IssueSweepPoint(notnull OVT_MountedForceSpawningDeploymentModule mounted)
	{
		SCR_AIGroup crew = ResolveCrewGroup(mounted);
		if (!crew)
			return;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return;

		ClearOwnedWaypoints(crew);
		DetachForeignWaypoints(crew);

		vector roadPoint;
		if (ChooseSweepPoint(roadPoint))
		{
			AIWaypoint waypoint = config.SpawnPatrolWaypoint(roadPoint);
			if (!waypoint)
				return;

			m_aOwnedWaypoints.Insert(waypoint);
			crew.AddWaypoint(waypoint);
			return;
		}

		LogNoRoad();

		AIWaypoint defend = config.SpawnDefendWaypoint(SweepCentre());
		if (!defend)
			return;

		m_aOwnedWaypoints.Insert(defend);
		crew.AddWaypoint(defend);
	}

	//------------------------------------------------------------------------------------------------
	//! Samples bearings around the hotspot, starting at a random one, for a road inside
	//! ROAD_SEARCH_RADIUS_M at a random distance within m_fSweepRadius.
	//! ⚠ RandInt is MAX-EXCLUSIVE.
	//! \param[out] position The road point to send the vehicle to. Untouched on a false return.
	//! \return True when a road point was found.
	protected bool ChooseSweepPoint(out vector position)
	{
		position = vector.Zero;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		vector centre = SweepCentre();

		int startSample = s_AIRandomGenerator.RandInt(0, SWEEP_SAMPLES);

		for (int i = 0; i < SWEEP_SAMPLES; i++)
		{
			int sample = (startSample + i) % SWEEP_SAMPLES;
			float bearing = (360.0 / SWEEP_SAMPLES) * sample;
			float distance = s_AIRandomGenerator.RandFloatXY(0, m_fSweepRadius);

			float radians = bearing * Math.DEG2RAD;
			vector probe = centre + Vector(Math.Sin(radians) * distance, 0, Math.Cos(radians) * distance);
			probe[1] = world.GetSurfaceY(probe[0], probe[2]);

			vector roadPosition;
			vector roadAngles;
			if (OVT_WorldUtils.FindNearestRoadSpawn(probe, ROAD_SEARCH_RADIUS_M, roadPosition, roadAngles))
			{
				position = roadPosition;
				return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Where the sweep is centred - the deployment's own position, which is where
	//! TickHunterKiller() created it: the hotspot itself.
	protected vector SweepCentre()
	{
		if (!m_ParentDeployment)
			return vector.Zero;

		return m_ParentDeployment.GetPosition();
	}

	//------------------------------------------------------------------------------------------------
	//! Says ONCE that this hotspot offers no road to sweep from, and then stops saying it.
	protected void LogNoRoad()
	{
		if (m_bNoRoadLogged)
			return;

		m_bNoRoadLogged = true;

		OVT_DeploymentLog.Debug(string.Format("[Overthrow] Armoured sweep '%1': no road within %2 m of any sampled point around %3 - holding at the hotspot instead",
			DescribeSelf(), ROAD_SEARCH_RADIUS_M.ToString(), SweepCentre().ToString()));
	}

	//------------------------------------------------------------------------------------------------
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
	//! Takes everything ELSE off the crew's queue, without deleting any of it.
	//! \param[in] crew The crew group.
	protected void DetachForeignWaypoints(notnull SCR_AIGroup crew)
	{
		array<AIWaypoint> waypoints = {};
		crew.GetWaypoints(waypoints);

		foreach (AIWaypoint waypoint : waypoints)
		{
			if (waypoint)
				crew.RemoveWaypoint(waypoint);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Resolving the force
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \return The mounted force of this deployment, or null when its config authors none. Resolved on
	//! every ask rather than cached - the same rule the mobile checkpoint's own header states.
	protected OVT_MountedForceSpawningDeploymentModule ResolveMountedForce()
	{
		if (!m_ParentDeployment)
			return null;

		array<OVT_BaseSpawningDeploymentModule> spawningModules = m_ParentDeployment.GetSpawningModules();

		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			OVT_MountedForceSpawningDeploymentModule mounted = OVT_MountedForceSpawningDeploymentModule.Cast(spawningModule);
			if (mounted)
				return mounted;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! THE CREW, IDENTIFIED STRUCTURALLY AND NOT BY A FLAG - the same seam the mobile checkpoint's own
	//! ResolveCrewGroup() uses: the mounted module reports its force AND its crew through
	//! GetSpawnedEntities(), and reports ONLY its force's handles through CollectRegisteredHandles(), so
	//! the crew is the one group it reports that is not one of its passengers.
	//! \param[in] mounted The force holding the vehicle.
	//! \return The crew group, or null.
	protected SCR_AIGroup ResolveCrewGroup(notnull OVT_MountedForceSpawningDeploymentModule mounted)
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return null;

		array<int> handles = {};
		mounted.CollectRegisteredHandles(handles);

		array<SCR_AIGroup> passengers = {};
		foreach (int handle : handles)
		{
			SCR_AIGroup passenger = virtualization.GetGroup(handle);
			if (passenger)
				passengers.Insert(passenger);
		}

		array<IEntity> reported = mounted.GetSpawnedEntities();
		foreach (IEntity entity : reported)
		{
			SCR_AIGroup group = SCR_AIGroup.Cast(entity);
			if (!group)
				continue;

			if (passengers.Contains(group))
				continue;

			return group;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return This module's name for a log line, or a stand-in when none was authored.
	protected string DescribeSelf()
	{
		if (!m_sModuleName.IsEmpty())
			return m_sModuleName;

		return "armoured sweep";
	}

	//------------------------------------------------------------------------------------------------
	// Teardown
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Withdraws this module's orders and forgets the sweep.
	//! \param[in] reason What ended it, for the log line. Empty logs nothing.
	protected void StandDownSweep(string reason)
	{
		OVT_MountedForceSpawningDeploymentModule mounted = ResolveMountedForce();

		SCR_AIGroup crew;
		if (mounted)
			crew = ResolveCrewGroup(mounted);

		ClearOwnedWaypoints(crew);

		m_bSweeping = false;
		m_iTicksSinceWaypoint = 0;
		m_iTicksSweeping = 0;
		m_bNoRoadLogged = false;

		if (reason != "")
			OVT_DeploymentLog.Debug(string.Format("[Overthrow] Armoured sweep '%1' stood down: %2", DescribeSelf(), reason));
	}

	//------------------------------------------------------------------------------------------------
	override void OnDeactivate()
	{
		super.OnDeactivate();

		StandDownSweep("");
	}

	//------------------------------------------------------------------------------------------------
	//! ⚠ THE WAYPOINTS HAVE TO GO HERE TOO - the same rule the mobile checkpoint's own OnCleanup()
	//! states: a deployment torn down by anything else (the hunter-killer dispatcher never revisiting
	//! this one, a Game Master, a campaign reset) runs Cleanup() on every module, and a waypoint entity
	//! left behind is a leaked entity per relocation, forever.
	override protected void OnCleanup()
	{
		ClearOwnedWaypoints(null);

		super.OnCleanup();
	}

	//------------------------------------------------------------------------------------------------
	// Cloning
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! EVERY attribute has to appear here, and no runtime state may: a clone belongs to a different
	//! deployment and has swept nothing.
	//!
	//! What a dropped line would cost: drop m_fSweepRadius and every sweep loiters at radius 0, i.e. sits
	//! on top of the hotspot; drop m_iSweepMinutes and the sweep never reports itself finished, so it is
	//! never collected; drop m_fWaypointInterval and the vehicle re-issues itself a new order every
	//! update, one WaypointTicks() short of never moving at all (the clamp to 1 tick masks it, badly).
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_ArmouredSweepBehaviorDeploymentModule clone = new OVT_ArmouredSweepBehaviorDeploymentModule();

		clone.m_sModuleName = m_sModuleName;
		clone.m_fSweepRadius = m_fSweepRadius;
		clone.m_iSweepMinutes = m_iSweepMinutes;
		clone.m_fWaypointInterval = m_fWaypointInterval;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	void PrintSweepDebugInfo()
	{
		Print(string.Format("Armoured Sweep Module: %1", DescribeSelf()));
		Print(string.Format("  Radius: %1 m  Sweep: %2 min  Waypoint interval: %3 s",
			m_fSweepRadius.ToString(), m_iSweepMinutes.ToString(), m_fWaypointInterval.ToString()));
		Print(string.Format("  Sweeping: %1  ticks %2/%3 (waypoint) %4/%5 (sweep)",
			m_bSweeping.ToString(), m_iTicksSinceWaypoint.ToString(), WaypointTicks().ToString(),
			m_iTicksSweeping.ToString(), SweepTicks().ToString()));
	}
}
