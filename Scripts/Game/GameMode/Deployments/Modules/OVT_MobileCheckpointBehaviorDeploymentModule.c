//------------------------------------------------------------------------------------------------
//! A MOBILE CHECKPOINT: the armed vehicle a mounted force arrived in is parked on a road INTO the
//! objective, its gun stays manned, its infantry gets out, and every few minutes the vehicle moves to
//! a different approach.
//!
//! It is the first behaviour module in this tree that owns a VEHICLE rather than a squad, and it owns
//! it entirely through OVT_MountedForceSpawningDeploymentModule's public seams - GetMountedVehicle(),
//! GetInsertionState(), GetSpawnedEntities(), CollectRegisteredHandles(). Nothing here reaches into
//! that module and nothing here was added to it.
//!
//! ⚠ IT DOES NOTHING AT ALL UNTIL THE FORCE IS HOLDING. Every one of the mounted module's roads to the
//! march ends somewhere other than HOLDING, and in all of them this module must stand aside: the
//! men are on foot with a plan that already points at the objective, and a checkpoint with no vehicle
//! is not a degradation this module can improve on. A force that LEAVES the hold - its vehicle
//! destroyed under it - has its orders withdrawn here and is left to walk. That is F3/F8, and it is
//! why this module polls the state on every update rather than latching once on arrival.
//!
//! ==========================================================================================
//! 🔴 IT DOES NOT ANSWER BuildVirtualPlan(), AND THAT IS A DECISION, NOT AN OMISSION.
//! ==========================================================================================
//! The virtualization core builds a group's waypoints from its plan AT REGISTRATION and owns them for
//! the group's life - "a plan is registration-time input only" (virtualization/core api.md). A plan is
//! therefore asked for at the SOURCE, before this checkpoint exists, and could never be moved
//! afterwards, while the whole point of this module is that the checkpoint MOVES.
//!
//! So the force keeps the insertion module's own fallback: a cycling march onto the objective with a
//! long hold. That is not a compromise, it is what a harassment operation is FOR - and the tree already
//! says so: OVT_TEST_Init_ObjectiveOperations' ladder case fails any rung that authors a patrol
//! behaviour, on the stated grounds that a plan of its own would "pre-empt the insertion module's
//! cycling march onto the deployment position". The infantry pushes into the town; the VEHICLE and its
//! crew are the checkpoint.
//!
//! ⚠ THE CONSEQUENCE, STATED: a relocation moves the vehicle and its crew, not the dismounted
//! infantry. Men already on the ground hold the plan they were registered with. Changing that would
//! mean re-planning a registered group, which the frozen core API forbids.
//!
//! ==========================================================================================
//! 🔴 THE CREW IS THE ONLY GROUP IN THIS FRAMEWORK THIS MODULE MAY GIVE A WAYPOINT TO.
//! ==========================================================================================
//! OVT_InsertionSpawningDeploymentModule.EnsureCrew() registers the transport crew with a NULL plan,
//! deliberately ("the crew is not the force, and a harassment plan handed to the crew would send the
//! truck into the town centre"). Core therefore owns no waypoints on that group, which is the only
//! reason its own module can hand it move orders and the only reason this one can. EVERY OTHER GROUP
//! OF THIS DEPLOYMENT IS PLAN-DRIVEN AND MUST NOT BE TOUCHED: adding or removing a waypoint on one
//! corrupts the record core persists (OVT_BaseBehaviorDeploymentModule's own header).
//!
//! ⚠ WAYPOINTS ARE NOT OWNED BY THE GROUP THEY ARE GIVEN TO. AIGroup.AddWaypoint() takes no ownership,
//! so every waypoint spawned here is remembered and deleted by hand on every exit - the same rule, and
//! the same trap, as the insertion module's own move orders.
//!
//! ⚠ THE PASSENGERS' REGISTRATION RING IS DELIBERATELY LEFT ALONE WHEN THEY GET OUT. The insertion
//! module pairs its disembark with a drop back to the ordinary proximity ring; this one must not, and
//! the reason is D8: a mounted deployment is exempt from OVT_DeploymentManagerComponent
//! .SuppressForcesAroundBattle only while its handles resolve to a ring STRICTLY WIDER than the global
//! one. Phase 4 authors this same module on the QRF's mounted echelon, at a live battle, where a drop
//! to the global ring would pin the force dormant and deliver an empty vehicle - silently. The price
//! is that the checkpoint's men stay materialised for the deployment's life, and it is already being
//! paid: D11 keeps an engine observer on the transport for exactly that long anyway.
//!
//! ⚠ THE VEHICLE IS NEVER MOVED BY TRANSFORM, only driven. Rotating or teleporting an occupied,
//! physics-simulated hull desynchronises the rigid body from the entity node (see
//! OVT_BaseSpawningDeploymentModule.SpawnEntity's own note) and throws its occupants. "Facing along
//! the road" is therefore delivered by ARRIVING along it: the checkpoint point is a road point, the
//! crew drives to it on the road, and a vehicle that drove there is pointing the way it came.
//! m_vCheckpointAngles is the road's heading, used to lay the men out along the road and to explain
//! the choice in the log.
//!
//! ⚠ AUTHOR THIS BEFORE THE REINFORCEMENT MODULE, like every other behaviour module: behaviour
//! modules update in authored order and a reinforcement pass that ran first would rebuy a force in the
//! same pass another module decided its mission was over.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_MobileCheckpointBehaviorDeploymentModule : OVT_BaseBehaviorDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;

	[Attribute(defvalue: "150", desc: "Nearest the objective a checkpoint may be set up, in metres")]
	float m_fApproachMinDistance;

	[Attribute(defvalue: "300", desc: "Furthest from the objective a checkpoint may be set up, in metres")]
	float m_fApproachMaxDistance;

	[Attribute(defvalue: "120", desc: "How far from a sampled approach point a road still counts as THAT approach, in metres. A bearing with no road inside this is not an approach and is refused")]
	float m_fRoadSearchRadius;

	[Attribute(defvalue: "8", desc: "Minutes on one approach before the vehicle moves to a different one. 0 parks it once and never moves it, which is what a standoff or a defensive posture wants")]
	int m_iRelocateMinutes;

	//! ⚠ CONSUMED AS A PLACEMENT, NOT AS ORDERS. The men are put down in a ring of this radius when
	//! they get out; where they go afterwards belongs to the plan they were registered with. See the
	//! class header's BuildVirtualPlan note.
	[Attribute(defvalue: "25", desc: "How far from the parked vehicle the dismounted infantry is placed, in metres")]
	float m_fCheckpointSpread;

	//------------------------------------------------------------------------------------------------
	// CONSTANTS
	//------------------------------------------------------------------------------------------------

	//! How many bearings around the objective are sampled for a road. Twelve is one every thirty
	//! degrees - fine enough that a town with two roads in offers both, coarse enough that the whole
	//! sweep is twelve road queries once every few minutes.
	static const int APPROACH_SAMPLES = 12;

	//! How far apart two approaches have to be to count as different, in degrees. Thirty degrees would
	//! let two samples of the SAME road out of town both qualify; sixty is two clear sectors apart.
	static const float MIN_BEARING_SEPARATION_DEG = 60;

	//! How close to the chosen point the vehicle has to stop before the checkpoint counts as set, in
	//! metres. Generous: the point is road-snapped and a driver parks where the road lets him, not on
	//! the waypoint.
	static const float PARKED_RADIUS_M = 45;

	//! How many updates the crew is given to reach a checkpoint before the checkpoint is declared to be
	//! wherever the vehicle actually is.
	//!
	//! ⚠ IT MUST TERMINATE. A drive that never arrives - blocked road, water in the way, a crew the LOD
	//! system has stopped simulating - would otherwise leave the infantry aboard forever, which is the
	//! one outcome worse than a checkpoint in a slightly wrong place. Twelve updates is about two
	//! minutes.
	static const int PARK_PATIENCE_TICKS = 12;

	//! How many updates the checkpoint waits for somebody to be aboard before deciding nobody is.
	//!
	//! A force that walked in, or one that has been wiped, is indistinguishable from one still filling
	//! through the AI spawn queue, so this has to be a budget rather than a single look. Six updates is
	//! about a minute.
	static const int DISMOUNT_PATIENCE_TICKS = 6;

	//! One deployment update, in seconds. Mirrors OVT_DeploymentComponent.UPDATE_FREQUENCY, which is
	//! what Update() is actually called with; the real interval is staggered by 0.8-1.2x, so the
	//! relocate clock is accurate to about a fifth of its length.
	static const int UPDATE_SECONDS = 10;

	//! Seconds in a minute, so the relocate clock's conversion is written down once.
	static const int SECONDS_PER_MINUTE = 60;

	//------------------------------------------------------------------------------------------------
	// RUNTIME STATE - none of it authored, none of it persisted, none of it cloned.
	//------------------------------------------------------------------------------------------------

	//! Where the checkpoint is, once one has been chosen. Zero until then.
	protected vector m_vCheckpoint;

	//! The heading of the road at m_vCheckpoint, in Math3D.AnglesToMatrix order.
	protected vector m_vCheckpointAngles;

	//! The bearing from the objective to the current checkpoint, or NO_PREVIOUS_BEARING before the
	//! first one. Set in the constructor, because `new` does not apply attribute defaults and an unset
	//! float starts at zero - which is due north, a real bearing this module would then never pick.
	protected float m_fCheckpointBearing;

	protected bool m_bCheckpointChosen;

	//! True once the vehicle has settled at (or been given up on near) the checkpoint.
	protected bool m_bParked;

	//! 0 nobody has been told to get out, 1 the order has been given, 2 the men have been placed.
	protected int m_iDismountStage;

	//! Updates spent at stage 0 with nobody aboard to let out, for DISMOUNT_PATIENCE_TICKS.
	protected int m_iDismountWaitTicks;

	//! Updates since the crew was last given a destination, for PARK_PATIENCE_TICKS.
	protected int m_iTicksSinceOrder;

	//! Updates since the checkpoint was last set, for the relocate clock.
	protected int m_iTicksSinceRelocate;

	//! Every waypoint this module spawned. AIGroup.AddWaypoint() does NOT take ownership.
	protected ref array<AIWaypoint> m_aOwnedWaypoints;

	//! Latches the one line explaining that no approach could be found, so a checkpoint at an objective
	//! with no roads at all does not print a line every ten seconds for the rest of its life.
	protected bool m_bNoApproachLogged;

	//------------------------------------------------------------------------------------------------
	void OVT_MobileCheckpointBehaviorDeploymentModule()
	{
		m_aOwnedWaypoints = new array<AIWaypoint>();
		m_fCheckpointBearing = OVT_CheckpointApproachRules.NO_PREVIOUS_BEARING;
	}

	//------------------------------------------------------------------------------------------------
	// The tick
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! One observation of a checkpoint.
	//! \param[in] deltaTime Milliseconds since the last update of this module. Unused - every clock
	//!            here is counted in updates, not in wall time; see UPDATE_SECONDS.
	override void OnUpdate(int deltaTime)
	{
		super.OnUpdate(deltaTime);

		if (!m_ParentDeployment)
			return;

		OVT_MountedForceSpawningDeploymentModule mounted = ResolveMountedForce();
		if (!mounted)
			return;

		// ⚠ POLLED, NEVER LATCHED. A force that fell back to the march after the checkpoint was set has
		// no vehicle to park and no crew to order; leaving a stale move waypoint on a group that is now
		// walking is how men end up driving nowhere.
		if (mounted.GetInsertionState() != OVT_EInsertionState.HOLDING)
		{
			if (m_bCheckpointChosen)
				StandDownCheckpoint("its force is no longer riding a vehicle");

			return;
		}

		Vehicle vehicle = mounted.GetMountedVehicle();
		if (!vehicle)
			return;

		if (!m_bCheckpointChosen)
		{
			if (!EstablishCheckpoint(mounted))
				return;
		}

		TickPark(mounted, vehicle);
	}

	//------------------------------------------------------------------------------------------------
	//! Chooses the first approach and sends the vehicle to it.
	//! \param[in] mounted The force holding the vehicle.
	//! \return True when there is a checkpoint to drive to.
	protected bool EstablishCheckpoint(notnull OVT_MountedForceSpawningDeploymentModule mounted)
	{
		vector position;
		vector angles;
		float bearing;

		if (!ChooseApproach(position, angles, bearing))
		{
			LogNoApproach();
			return false;
		}

		AdoptCheckpoint(position, angles, bearing);

		IssueDrive(mounted, position);

		Print(string.Format("[Overthrow] Mobile checkpoint '%1': setting up on the approach at bearing %2 from the objective, %3 m out",
			DescribeSelf(), Math.Round(bearing).ToString(), Math.Round(vector.Distance(position, ObjectivePosition())).ToString()), LogLevel.NORMAL);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Watches the vehicle onto its checkpoint, opens the doors once it is there, and runs the relocate
	//! clock.
	//! \param[in] mounted The force holding the vehicle.
	//! \param[in] vehicle The vehicle it is holding.
	protected void TickPark(notnull OVT_MountedForceSpawningDeploymentModule mounted, notnull Vehicle vehicle)
	{
		if (!m_bParked)
		{
			m_iTicksSinceOrder = m_iTicksSinceOrder + 1;

			// ⚠ THE ORDER IS RE-OFFERED UNTIL IT LANDS, ONCE. A crew fills progressively through the AI
			// spawn queue, so the drive order issued the moment the checkpoint was chosen can be refused
			// because there is no group to give it to yet - and a checkpoint that never issued one would
			// spend its whole patience budget waiting for a drive nobody was ever told to make. Gated on
			// this module owning no waypoint at all, so it is a retry and never a re-order: a waypoint
			// the group has already completed is still held here, which is what stops this thrashing.
			if (m_aOwnedWaypoints.IsEmpty())
				IssueDrive(mounted, m_vCheckpoint);

			if (vector.Distance(vehicle.GetOrigin(), m_vCheckpoint) <= PARKED_RADIUS_M)
			{
				SettleCheckpoint(vehicle, "it reached the approach it was sent to");
			}
			else if (m_iTicksSinceOrder >= PARK_PATIENCE_TICKS)
			{
				// The drive did not finish. The checkpoint is wherever the gun ended up, which is still
				// a manned vehicle on the ground - and it lets the infantry out, which is the thing that
				// must not wait on a drive that may never complete.
				m_vCheckpoint = vehicle.GetOrigin();
				SettleCheckpoint(vehicle, "it stopped making progress towards the approach and holds where it is");
			}
			else
			{
				return;
			}
		}

		TickDismount(mounted, vehicle);

		TickRelocateClock(mounted, vehicle);
	}

	//------------------------------------------------------------------------------------------------
	//! The checkpoint is now where the vehicle is standing.
	//! \param[in] vehicle The parked vehicle.
	//! \param[in] reason What settled it, for the log.
	protected void SettleCheckpoint(notnull Vehicle vehicle, string reason)
	{
		m_bParked = true;
		m_iTicksSinceRelocate = 0;

		Print(string.Format("[Overthrow] Mobile checkpoint '%1' is set at %2 - %3",
			DescribeSelf(), m_vCheckpoint.ToString(), reason), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	// The dismount
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! OPENS THE DOORS FOR THE INFANTRY AND LEAVES THE CREW ABOARD, over two updates.
	//!
	//! ⚠ THE TWO STEPS ARE ON SEPARATE UPDATES DELIBERATELY. Getting out and being put down are two
	//! different operations on the same man, and the second one is only meaningful once the first has
	//! taken effect - a placement applied in the same pass as the exit would be teleporting somebody the
	//! engine still has in a compartment.
	//!
	//! ⚠ THE CREW IS NEVER TOUCHED. It is identified structurally rather than by a flag - see
	//! ResolveCrewGroup() - and only the mounted module's own PASSENGER handles are opened, so a gunner
	//! is never walked out of his turret.
	//! \param[in] mounted The force holding the vehicle.
	//! \param[in] vehicle The parked vehicle.
	protected void TickDismount(notnull OVT_MountedForceSpawningDeploymentModule mounted, notnull Vehicle vehicle)
	{
		if (m_iDismountStage == 0)
		{
			int opened = DisembarkForce(mounted);
			if (opened > 0)
			{
				Print(string.Format("[Overthrow] Mobile checkpoint '%1': %2 man/men dismounted; the crew stays aboard so the gun stays manned",
					DescribeSelf(), opened.ToString()), LogLevel.NORMAL);

				m_iDismountStage = 1;
				return;
			}

			// ⚠ NOBODY ABOARD IS NOT THE SAME AS "THE DOORS ARE OPEN", AND THE STAGE MUST NOT ADVANCE ON
			// IT. A group fills progressively through the AI spawn queue, so an empty answer on the first
			// pass after parking is ordinary; advancing anyway would leave men who arrived a tick later
			// riding a parked vehicle for the rest of the operation. It is still BOUNDED, because "nobody
			// aboard" is also what a force that walked in, or one that has been wiped, looks like.
			m_iDismountWaitTicks = m_iDismountWaitTicks + 1;

			if (m_iDismountWaitTicks < DISMOUNT_PATIENCE_TICKS)
				return;

			m_iDismountStage = 2;

			Print(string.Format("[Overthrow] Mobile checkpoint '%1': nobody was aboard to dismount after %2 update(s) - its force either walked in or is gone, and the vehicle holds the approach on its own",
				DescribeSelf(), m_iDismountWaitTicks.ToString()), LogLevel.NORMAL);

			return;
		}

		if (m_iDismountStage == 1)
		{
			PlaceForce(mounted, vehicle);
			m_iDismountStage = 2;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every member of the FORCE gets out. The crew does not.
	//! \param[in] mounted The force holding the vehicle.
	//! \return How many men were actually taken out of a compartment.
	protected int DisembarkForce(notnull OVT_MountedForceSpawningDeploymentModule mounted)
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return 0;

		array<int> handles = {};
		mounted.CollectRegisteredHandles(handles);

		int opened = 0;

		foreach (int handle : handles)
		{
			SCR_AIGroup group = virtualization.GetGroup(handle);
			if (!group)
				continue;

			array<AIAgent> agents = {};
			group.GetAgents(agents);

			foreach (AIAgent agent : agents)
			{
				if (DisembarkAgent(agent))
					opened = opened + 1;
			}
		}

		return opened;
	}

	//------------------------------------------------------------------------------------------------
	//! Gets one man out.
	//!
	//! TELEPORT RATHER THAN THE ANIMATED EXIT, the same call the insertion module's own disembark makes
	//! and for the same reason: an animated dismount is interruptible and takes seconds per man.
	//! \param[in] agent The man to put on the ground.
	//! \return True when he was in a compartment and has been taken out of it.
	protected bool DisembarkAgent(AIAgent agent)
	{
		if (!agent)
			return false;

		IEntity character = agent.GetControlledEntity();
		if (!character)
			return false;

		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
		if (!access)
			return false;

		if (!access.IsInCompartment())
			return false;

		access.GetOutVehicle(EGetOutType.TELEPORT, 0, ECloseDoorAfterActions.LEAVE_OPEN, true);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Lays the dismounted men out around the vehicle, once, so the checkpoint reads as one.
	//!
	//! ⚠ IT IS A PLACEMENT AND NOT AN ORDER, and it happens exactly once. Where the men go next is
	//! their registered plan's business - see the class header. Repeating it every update would fight
	//! that plan and pin a squad to a spot the core does not know about.
	//!
	//! Teleport rather than SetOrigin: a character moved by SetOrigin keeps a navmesh position it is no
	//! longer standing on, which is the trap OVT_PlacedInfantrySpawningDeploymentModule records.
	//! \param[in] mounted The force holding the vehicle.
	//! \param[in] vehicle The parked vehicle.
	protected void PlaceForce(notnull OVT_MountedForceSpawningDeploymentModule mounted, notnull Vehicle vehicle)
	{
		if (m_fCheckpointSpread <= 0)
			return;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		array<int> handles = {};
		mounted.CollectRegisteredHandles(handles);

		array<AIAgent> placeable = {};

		foreach (int handle : handles)
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

				// Anybody the exit did not actually free stays where he is rather than being teleported
				// out of a seat by the back door.
				CompartmentAccessComponent access = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
				if (access && access.IsInCompartment())
					continue;

				placeable.Insert(agent);
			}
		}

		vector centre = vehicle.GetOrigin();

		for (int i = 0; i < placeable.Count(); i++)
		{
			BaseGameEntity character = BaseGameEntity.Cast(placeable[i].GetControlledEntity());
			if (!character)
				continue;

			vector post = PerimeterPost(centre, i, placeable.Count(), world);

			vector mat[4];
			Math3D.AnglesToMatrix(OVT_BaseSpawningDeploymentModule.GetUprightSpawnRotation(PostFacing(centre, post)), mat);
			mat[3] = post;

			character.Teleport(mat);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Where the index-th of count men stands around the checkpoint.
	//!
	//! Laid out along the ROAD rather than in a circle: the men spread up and down the approach the
	//! vehicle is covering, which is what a checkpoint looks like from a driver's seat. The road's
	//! heading is m_vCheckpointAngles' yaw.
	//! \param[in] centre The parked vehicle.
	//! \param[in] index Which man.
	//! \param[in] count How many there are.
	//! \param[in] world The world to take a surface height from.
	//! \return A ground position.
	protected vector PerimeterPost(vector centre, int index, int count, notnull BaseWorld world)
	{
		float bearing = m_vCheckpointAngles[0];

		// Alternate up and down the road, stepping out by a share of the spread each pair, so two men
		// are a short way either side and four are a short way and a long way either side.
		int step = (index / 2) + 1;
		float along = m_fCheckpointSpread * (step / (float)((count / 2) + 1));

		if (index % 2 == 1)
			along = -along;

		// A quarter of the spread off the carriageway, so nobody is standing in the road the vehicle
		// is meant to be blocking.
		float across = m_fCheckpointSpread * 0.25;
		if (index % 4 >= 2)
			across = -across;

		float radians = bearing * Math.DEG2RAD;
		vector forward = Vector(Math.Sin(radians), 0, Math.Cos(radians));
		vector side = Vector(forward[2], 0, -forward[0]);

		vector post = centre + (forward * along) + (side * across);
		post[1] = world.GetSurfaceY(post[0], post[2]);

		return post;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] centre The parked vehicle.
	//! \param[in] post Where the man is standing.
	//! \return The yaw that has him looking away from the vehicle, in degrees.
	protected float PostFacing(vector centre, vector post)
	{
		vector outward = post - centre;
		outward[1] = 0;

		if (outward.Length() < 0.01)
			return m_vCheckpointAngles[0];

		outward.Normalize();

		return Math.Atan2(outward[0], outward[2]) * Math.RAD2DEG;
	}

	//------------------------------------------------------------------------------------------------
	// The relocation
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Counts the clock out and moves the vehicle when it runs down.
	//! \param[in] mounted The force holding the vehicle.
	//! \param[in] vehicle The parked vehicle.
	protected void TickRelocateClock(notnull OVT_MountedForceSpawningDeploymentModule mounted, notnull Vehicle vehicle)
	{
		// ⚠ 0 IS "PARK AND STAY PARKED", NOT "MOVE EVERY TICK". The QRF echelon and the base armour
		// sortie both author it: a force holding a standoff ring is not patrolling.
		if (m_iRelocateMinutes <= 0)
			return;

		m_iTicksSinceRelocate = m_iTicksSinceRelocate + 1;

		if (m_iTicksSinceRelocate < RelocateTicks())
			return;

		Relocate(mounted, vehicle);
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many updates one relocation interval is worth. At least one.
	protected int RelocateTicks()
	{
		int ticks = (m_iRelocateMinutes * SECONDS_PER_MINUTE) / UPDATE_SECONDS;

		if (ticks < 1)
			return 1;

		return ticks;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the checkpoint to a DIFFERENT approach.
	//!
	//! ⚠ A REFUSAL RESTARTS THE CLOCK AND CHANGES NOTHING ELSE. An objective with one usable road has
	//! no second approach to move to, and announcing a relocation that did not happen - or relocating
	//! onto the spot already occupied - is worse than staying put.
	//! \param[in] mounted The force holding the vehicle.
	//! \param[in] vehicle The parked vehicle.
	protected void Relocate(notnull OVT_MountedForceSpawningDeploymentModule mounted, notnull Vehicle vehicle)
	{
		m_iTicksSinceRelocate = 0;

		vector position;
		vector angles;
		float bearing;

		if (!ChooseApproach(position, angles, bearing))
		{
			LogNoApproach();
			return;
		}

		float previous = m_fCheckpointBearing;

		AdoptCheckpoint(position, angles, bearing);

		m_bParked = false;

		IssueDrive(mounted, position);

		Print(string.Format("[Overthrow] Mobile checkpoint '%1' is moving from the approach at bearing %2 to the one at bearing %3",
			DescribeSelf(), Math.Round(previous).ToString(), Math.Round(bearing).ToString()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	// Choosing an approach
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! SAMPLES THE BEARINGS AROUND THE OBJECTIVE, KEEPS THE ONES WITH A ROAD, AND TAKES ONE AT RANDOM
	//! THAT IS NOT THE ONE JUST LEFT.
	//!
	//! The arithmetic - the bearings, the band, the exclusion and the pick - is
	//! OVT_CheckpointApproachRules and is not restated here. What is here is the only part that needs
	//! the map: whether a sampled bearing has a road on it.
	//!
	//! ⚠ THE ROAD POINT IS WHERE THE CHECKPOINT GOES, not the sampled point, so a checkpoint can sit up
	//! to m_fRoadSearchRadius outside the authored band. That is deliberate: a checkpoint belongs on the
	//! road, and the band is where to look for one.
	//! \param[out] position The road point to park on.
	//! \param[out] angles The road's heading there, in Math3D.AnglesToMatrix order.
	//! \param[out] bearing The bearing from the objective to that approach.
	//! \return True when an approach was found.
	protected bool ChooseApproach(out vector position, out vector angles, out float bearing)
	{
		position = vector.Zero;
		angles = vector.Zero;
		bearing = OVT_CheckpointApproachRules.NO_PREVIOUS_BEARING;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		vector objective = ObjectivePosition();

		float band = OVT_CheckpointApproachRules.BandDistance(m_fApproachMinDistance, m_fApproachMaxDistance,
			s_AIRandomGenerator.RandFloat01());

		array<float> bearings = {};
		array<vector> positions = {};
		array<vector> headings = {};

		for (int sample = 0; sample < APPROACH_SAMPLES; sample++)
		{
			float candidate = OVT_CheckpointApproachRules.BearingForSample(sample, APPROACH_SAMPLES);
			if (candidate < 0)
				continue;

			vector probe = OffsetByBearing(objective, candidate, band);
			probe[1] = world.GetSurfaceY(probe[0], probe[2]);

			vector roadPosition;
			vector roadAngles;
			if (!OVT_WorldUtils.FindNearestRoadSpawn(probe, m_fRoadSearchRadius, roadPosition, roadAngles))
				continue;

			bearings.Insert(candidate);
			positions.Insert(roadPosition);
			headings.Insert(roadAngles);
		}

		int picked = OVT_CheckpointApproachRules.ChooseBearingIndex(bearings, m_fCheckpointBearing,
			MIN_BEARING_SEPARATION_DEG, s_AIRandomGenerator.RandFloat01());

		if (picked == -1)
			return false;

		position = positions[picked];
		angles = headings[picked];
		bearing = bearings[picked];

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] origin Where to measure from.
	//! \param[in] bearing Compass bearing in degrees, zero being the world's +Z.
	//! \param[in] distance How far along it, in metres.
	//! \return The point that far out on that bearing, at the origin's own height.
	protected vector OffsetByBearing(vector origin, float bearing, float distance)
	{
		float radians = bearing * Math.DEG2RAD;

		return origin + Vector(Math.Sin(radians) * distance, 0, Math.Cos(radians) * distance);
	}

	//------------------------------------------------------------------------------------------------
	//! Takes a chosen approach as the current one.
	//! \param[in] position The road point.
	//! \param[in] angles The road's heading.
	//! \param[in] bearing The bearing from the objective.
	protected void AdoptCheckpoint(vector position, vector angles, float bearing)
	{
		m_vCheckpoint = position;
		m_vCheckpointAngles = angles;
		m_fCheckpointBearing = bearing;
		m_bCheckpointChosen = true;
		m_iTicksSinceOrder = 0;
		m_bNoApproachLogged = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Says ONCE that this objective offers nowhere to set up, and then stops saying it.
	protected void LogNoApproach()
	{
		if (m_bNoApproachLogged)
			return;

		m_bNoApproachLogged = true;

		Print(string.Format("[Overthrow] Mobile checkpoint '%1': no approach to the objective has a road within %2 m of the %3-%4 m band, so there is nowhere to set up",
			DescribeSelf(), Math.Round(m_fRoadSearchRadius).ToString(), Math.Round(m_fApproachMinDistance).ToString(),
			Math.Round(m_fApproachMaxDistance).ToString()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	// Orders
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Sends the vehicle somewhere, by giving its CREW a move order.
	//!
	//! ⚠ WHATEVER THE CREW WAS DOING IS DETACHED FIRST, AND DETACHED IS NOT DELETED. The insertion
	//! module's own landing-zone order is still on the queue when a mounted force arrives - arrival is
	//! judged by that module's radius test, not by the waypoint completing - so an order merely appended
	//! behind it might never run. It is taken OFF the group and left alone: the waypoint entity belongs
	//! to the insertion module, which deletes it on its own teardown.
	//! \param[in] mounted The force holding the vehicle.
	//! \param[in] destination Where to send it.
	//! \return True when the crew has the order.
	protected bool IssueDrive(notnull OVT_MountedForceSpawningDeploymentModule mounted, vector destination)
	{
		SCR_AIGroup crew = ResolveCrewGroup(mounted);
		if (!crew)
			return false;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return false;

		ClearOwnedWaypoints(crew);
		DetachForeignWaypoints(crew);

		AIWaypoint waypoint = config.SpawnMoveWaypoint(destination);
		if (!waypoint)
		{
			Print(string.Format("[Overthrow] Mobile checkpoint '%1': a move waypoint could not be spawned, so the vehicle has no orders",
				DescribeSelf()), LogLevel.WARNING);
			return false;
		}

		m_aOwnedWaypoints.Insert(waypoint);
		crew.AddWaypoint(waypoint);

		return true;
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
	//! every ask rather than cached: a module reference outliving the module list it came from is a
	//! worse failure than a five-element walk once every ten seconds.
	protected OVT_MountedForceSpawningDeploymentModule ResolveMountedForce()
	{
		// ⚠ GUARDED HERE AND NOT ONLY AT THE CALL SITES. OnDeactivate() reaches this through the stand-down
		// path, and a deployment being torn down by a Game Master or by the director's reset can have
		// dropped its parent already.
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
	//! THE CREW, IDENTIFIED STRUCTURALLY AND NOT BY A FLAG.
	//!
	//! The mounted module reports its force, its vehicle and its crew through GetSpawnedEntities(), and
	//! reports ONLY its force's handles through CollectRegisteredHandles() - the crew is registered
	//! under a separate owner key precisely so the two never mix (CREW_KEY_SUFFIX). So the crew is the
	//! one group it reports that is not one of its passengers, and that holds however many passenger
	//! groups the config authors.
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
	//! \return Where the checkpoint's objective is - the deployment's own position.
	protected vector ObjectivePosition()
	{
		if (!m_ParentDeployment)
			return vector.Zero;

		return m_ParentDeployment.GetPosition();
	}

	//------------------------------------------------------------------------------------------------
	//! \return This module's name for a log line, or a stand-in when none was authored.
	protected string DescribeSelf()
	{
		if (!m_sModuleName.IsEmpty())
			return m_sModuleName;

		return "mobile checkpoint";
	}

	//------------------------------------------------------------------------------------------------
	// Teardown
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Withdraws this module's orders and forgets the checkpoint.
	//! \param[in] reason What ended it, for the log.
	protected void StandDownCheckpoint(string reason)
	{
		OVT_MountedForceSpawningDeploymentModule mounted = ResolveMountedForce();

		SCR_AIGroup crew;
		if (mounted)
			crew = ResolveCrewGroup(mounted);

		ClearOwnedWaypoints(crew);

		m_bCheckpointChosen = false;
		m_bParked = false;
		m_iDismountStage = 0;
		m_iDismountWaitTicks = 0;
		m_iTicksSinceOrder = 0;
		m_iTicksSinceRelocate = 0;

		if (reason != "")
			Print(string.Format("[Overthrow] Mobile checkpoint '%1' stood down: %2", DescribeSelf(), reason), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDeactivate()
	{
		super.OnDeactivate();

		StandDownCheckpoint("");
	}

	//------------------------------------------------------------------------------------------------
	//! ⚠ THE WAYPOINTS HAVE TO GO HERE TOO. A deployment torn down by anything else - the objective
	//! director's reset, the reinforcement module's condition check, a Game Master - runs Cleanup() on
	//! every module, and a waypoint entity left behind is a leaked entity per relocation, forever.
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
	//! deployment and has parked nothing.
	//!
	//! What a dropped line would cost: drop m_fApproachMinDistance or m_fApproachMaxDistance and the
	//! band collapses to zero, so every checkpoint is set up ON the objective instead of on the road
	//! into it; drop m_fRoadSearchRadius and no bearing ever has a road inside it, so no checkpoint is
	//! ever established and the vehicle sits wherever it stopped with its infantry still aboard; drop
	//! m_iRelocateMinutes and a checkpoint authored to roam parks once and never moves - or, for the
	//! configs that authored 0, a standoff force starts wandering off its ring; drop m_fCheckpointSpread
	//! and the dismounted men are all put down on the same spot.
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_MobileCheckpointBehaviorDeploymentModule clone = new OVT_MobileCheckpointBehaviorDeploymentModule();

		clone.m_sModuleName = m_sModuleName;
		clone.m_fApproachMinDistance = m_fApproachMinDistance;
		clone.m_fApproachMaxDistance = m_fApproachMaxDistance;
		clone.m_fRoadSearchRadius = m_fRoadSearchRadius;
		clone.m_iRelocateMinutes = m_iRelocateMinutes;
		clone.m_fCheckpointSpread = m_fCheckpointSpread;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	// Queries
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \return Where the checkpoint is, or vector.Zero before one has been chosen.
	vector GetCheckpointPosition()
	{
		return m_vCheckpoint;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The bearing from the objective to the current checkpoint, or
	//! OVT_CheckpointApproachRules.NO_PREVIOUS_BEARING before one has been chosen.
	float GetCheckpointBearing()
	{
		return m_fCheckpointBearing;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether the vehicle has settled at its checkpoint.
	bool IsCheckpointSet()
	{
		return m_bCheckpointChosen && m_bParked;
	}

	//------------------------------------------------------------------------------------------------
	void PrintCheckpointDebugInfo()
	{
		Print(string.Format("Mobile Checkpoint Module: %1", DescribeSelf()));
		Print(string.Format("  Band: %1-%2 m, road search %3 m, spread %4 m",
			m_fApproachMinDistance.ToString(), m_fApproachMaxDistance.ToString(),
			m_fRoadSearchRadius.ToString(), m_fCheckpointSpread.ToString()));
		Print(string.Format("  Checkpoint: %1  bearing %2", m_vCheckpoint.ToString(), m_fCheckpointBearing.ToString()));
		Print(string.Format("  Parked: %1  dismount stage %2  relocate %3 of %4 tick(s)",
			m_bParked.ToString(), m_iDismountStage.ToString(), m_iTicksSinceRelocate.ToString(), RelocateTicks().ToString()));
	}
}
