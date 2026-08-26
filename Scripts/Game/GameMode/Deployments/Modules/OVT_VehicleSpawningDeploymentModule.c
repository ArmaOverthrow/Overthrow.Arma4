//------------------------------------------------------------------------------------------------
//! Vehicle spawning module for deployments: spawns the trucks by hand, REGISTERS their crews with the
//! virtualization core, and seats each crewman as the engine hands him over.
//!
//! TWO DIFFERENT KINDS OF THING, ON PURPOSE. A vehicle is a plain world entity that this module owns
//! outright; a crew is a registered virtualization group that the CORE owns. That split is the whole
//! shape of this file:
//!   - vehicles are spawned, counted, checked for destruction and deleted here, exactly as before;
//!   - crews are registered, reclaimed by owner key and unregistered - never deleted, never counted by
//!     agents, and never given a waypoint by anything in this directory.
//!
//! CREWS ARE REGISTERED ALWAYS-MATERIALISED (D10). m_iSpawnDistanceOverride defaults to 100000 m, so a
//! crew never goes dormant, the movement tick skips it (it only walks groups that are NOT spawned) and
//! live AI drives real roads. That is not a luxury: a DORMANT crew holding a route plan would be walked
//! in a straight line across the map by the movement tick while its truck stayed parked at the base,
//! and would materialise kilometres from its own vehicle. It is also parity - both shipped vehicle
//! configs already ship m_bEnableProximityActivation 0, so their crews were never despawned - and it is
//! bounded by m_iMaxInstances 1 on both of them.
//!
//! ⚠ AND ALWAYS-MATERIALISED IS ONLY HALF OF IT (2026-08-21). A ring decides whether a crew's men
//! EXIST; the per-agent LOD system decides whether anything is RUNNING inside them, and it switches
//! their behaviour trees off outright at max LOD - roughly a kilometre from the nearest observer,
//! whatever ring they were registered on. A crew with only the ring is a materialised driver asleep at
//! the wheel: the vehicle sits at its spawn holding orders nobody executes. That is what stranded an
//! insertion convoy 2.4 km from the only player on the map, and a patrol vehicle has the identical
//! failure mode. HoldCrewsActive() stamps the second gate every update through
//! OVT_MountedGroupActivation, and UnsubscribeCrew() - the single point at which a crew leaves this
//! module's hands - hands it back.
//!
//! THE STOLEN-VEHICLE GUARANTEE (D11) NO LONGER NEEDS A DISTANCE RULE. Teardown used to delete every
//! vehicle and every crewman within 40 m of the deployment marker, and the 40 m was there purely so a
//! vehicle a player had driven away was spared. There is no periodic teardown any more - OnDeactivate
//! does nothing - so the only teardown left is OnCleanup, when the deployment is genuinely over, and it
//! asks the two questions that actually matter: is anybody's player or recruit sitting in this vehicle,
//! and does a player own it. Crews are handed back with UnregisterGroup, which RESPECTS HELD MEMBERS
//! (SCR_AIGroup.HasHeldMember - an AI-activated member, e.g. one driving) and retires such a group in
//! place rather than deleting it out from under whatever is using it.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_VehicleSpawningDeploymentModule : OVT_BaseSpawningDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;

	[Attribute(desc: "Vehicle type name from faction registry")]
	string m_sVehicleType;

	[Attribute(defvalue: "1", desc: "Number of vehicles to spawn")]
	int m_iVehicleCount;

	[Attribute(desc: "Infantry group type to use as crew/passengers")]
	string m_sCrewGroupType;

	// Vehicle crew assignment happens automatically based on spawned group members

	[Attribute(defvalue: "50", desc: "Spawn radius around deployment position")]
	float m_fSpawnRadius;

	[Attribute(defvalue: "50", desc: "Resource cost per vehicle")]
	int m_iCostPerVehicle;

	[Attribute(defvalue: "true", desc: "Allow reinforcement when vehicles are destroyed")]
	bool m_bAllowReinforcement;

	[Attribute(defvalue: "25", desc: "Reinforcement cost per vehicle")]
	int m_iReinforcementCost;

	[Attribute(defvalue: "50", uiwidget: UIWidgets.EditBox, desc: "Max cruise speed of vehicles in km/h.", params: "0 inf 0.5")]
	float m_fMaxCruiseSpeed;

	//! DEFAULT 100000 = ALWAYS MATERIALISED, and that is the point (D10). A vehicle crew must not go
	//! dormant: the movement tick walks dormant groups along their plans, and a crew walked away from a
	//! parked truck would materialise kilometres from it. -1 would use the global virtualization spawn
	//! distance and 0 would mean "never materialise by proximity"; both are wrong here and are only
	//! exposed so an operator can dial the ring down on a server that cannot afford live crews.
	[Attribute(defvalue: "100000", desc: "Spawn distance for this module's vehicle crews, in metres. 100000 keeps them permanently materialised, which is what a vehicle crew needs; -1 uses the global virtualization spawn distance and 0 means never materialise by proximity")]
	int m_iSpawnDistanceOverride;

	//! Where a crew is registered relative to its vehicle, in metres along X. Unchanged from the
	//! hand-spawned version - close enough to walk to the truck, not inside it.
	static const float CREW_SPAWN_OFFSET_M = 5;

	//! AI spawn-budget tier for vehicle crews. NEVER leave a registration unstamped: an unstamped group
	//! inherits vanilla's LOW tier, is capped at half the AI budget and is evicted first.
	static const SCR_EAISpawnImportance CREW_IMPORTANCE = SCR_EAISpawnImportance.NORMAL;

	protected ref array<Vehicle> m_aSpawnedVehicles;

	//! Registry handles for this module's crews. NOT persisted and NOT authoritative: the registry is,
	//! and this list is re-derived from it by every convergence pass.
	protected ref array<int> m_aCrewHandles;

	//! Crew group ENTITY id -> the vehicle that crew rides in. Keyed on the group entity because that is
	//! all the engine's per-member callback gives us, and rebuilt from scratch every session because a
	//! restored group's entity id is a new one.
	protected ref map<ref EntityID, ref EntityID> m_mCrewVehicles;

	//! How many vehicles this module has ever put in the world, so "no vehicles" can be told apart from
	//! "never had any". Monotonic within a session.
	protected int m_iSpawnedCount;

	//! Same idea for crews: an empty handle list only means elimination if there was ever anything in it.
	protected int m_iCrewsRegisteredEver;

	//------------------------------------------------------------------------------------------------
	void OVT_VehicleSpawningDeploymentModule()
	{
		m_aSpawnedVehicles = new array<Vehicle>;
		m_aCrewHandles = new array<int>();
		m_mCrewVehicles = new map<ref EntityID, ref EntityID>;
		m_iSpawnedCount = 0;
		m_iCrewsRegisteredEver = 0;
	}

	//------------------------------------------------------------------------------------------------
	override int GetResourceCost()
	{
		return m_iVehicleCount * m_iCostPerVehicle;
	}

	//------------------------------------------------------------------------------------------------
	override void OnActivate()
	{
		super.OnActivate();

		if (!m_ParentDeployment)
			return;

		// No eliminated check here any more - the convergence owns that gate, so every caller gets the
		// same answer whether it arrives through activation or through the records-restored fan-out.
		EnsureGroups();
	}

	//------------------------------------------------------------------------------------------------
	//! DELIBERATELY A NO-OP. This used to run every time the last player walked outside the activation
	//! ring: it deleted every vehicle and cleaned up every crew that was still within 40 m of the
	//! deployment marker, which is the ad hoc virtualization this framework was migrated off. Deleting
	//! a crew now would throw away the survivor mask the whole feature exists to keep, and deleting a
	//! vehicle on a proximity timer is what the 40 m rule was invented to paper over.
	//! Deactivate() means "stop ticking"; only Cleanup() means "over".
	override void OnDeactivate()
	{
		super.OnDeactivate();
	}

	//------------------------------------------------------------------------------------------------
	//! The deployment is over: hand every crew back to the core, then take the trucks away - but only
	//! the ones nobody has taken over.
	//!
	//! ORDER MATTERS. Crews are unregistered FIRST, so a group with a held member (someone driving) is
	//! retired in place by the core instead of being deleted out from under the vehicle. Only then is
	//! the vehicle itself judged, and it is judged on the two things that can make it somebody else's
	//! property: a player-controlled occupant, and a player owner.
	//!
	//! THERE IS NO OnDelete COUNTERPART HERE OR ON THE DEPLOYMENT. One would also fire at quit-to-menu
	//! and would erase the very records persistence exists to keep.
	override protected void OnCleanup()
	{
		super.OnCleanup();

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (virtualization)
		{
			// Reclaim before releasing. A deployment deleted before this module ever converged holds an
			// empty handle list while the registry still holds its crews, and those would be orphaned
			// for the rest of the campaign with no owner left to find them.
			string ownerKey = GetOwnerKey();
			if (!ownerKey.IsEmpty())
				ReclaimHandles(virtualization, ownerKey);

			foreach (int handle : m_aCrewHandles)
			{
				UnsubscribeCrew(virtualization.GetGroup(handle));
				virtualization.UnregisterGroup(handle);
			}
		}

		m_aCrewHandles.Clear();
		m_mCrewVehicles.Clear();

		ReleaseVehicles();
	}

	//------------------------------------------------------------------------------------------------
	override void OnUpdate(int deltaTime)
	{
		super.OnUpdate(deltaTime);

		// Check for destroyed vehicles
		CheckVehicleStatus();

		// A crew that exists but is not running is a vehicle that never moves. See the class header.
		HoldCrewsActive();

		// An armed patrol vehicle whose gun nobody is manning is not a patrol. See the method.
		EnsureTurretsManned();

	}


	//------------------------------------------------------------------------------------------------
	//! Keeps every crew this module owns out of max LOD, so their behaviour trees keep running however
	//! far away the nearest observer is.
	//!
	//! ⚠ A RE-ASSERT ON EVERY UPDATE, NOT A ONE-SHOT AT PAIRING TIME. A core-registered crew fills
	//! PROGRESSIVELY through the engine's AI spawn queue and can go dormant and back again over a
	//! campaign, so the man in the driver's seat two minutes from now may not have existed when the crew
	//! was paired. Both calls behind this are no-ops on an agent that is already held, which is what
	//! makes running it every tick free.
	protected void HoldCrewsActive()
	{
		if (m_aCrewHandles.IsEmpty())
			return;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		foreach (int handle : m_aCrewHandles)
		{
			OVT_MountedGroupActivation.HoldGroupActive(virtualization.GetGroup(handle));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! PUTS SOMEBODY BACK BEHIND THE GUN when an armed patrol vehicle is driving around with an empty
	//! turret.
	//!
	//! ==========================================================================================
	//! 🔴 WHY THIS IS A RE-ASSERT AND NOT A FIX TO THE SEATING. SeatAgent() already seats
	//! pilot -> turret -> cargo and already refuses to move a man who is aboard, so the turret IS manned
	//! correctly when the crew is first seated - the author confirmed it: "the gunner spawns in the
	//! gunner slot but then gets moved to co-driver". Something OUTSIDE this module moves him
	//! afterwards, and the two candidates are both vanilla:
	//!   - SCR_AIGetEmptyCompartment allocates a NON-LEADER driver > turret > cargo and the LEADER
	//!     turret > cargo, and it runs whenever the group boards a vehicle it owns. It only ever fills
	//!     compartments that are EMPTY, so it cannot evict on its own - but any member who leaves for
	//!     any reason is re-allocated by that priority rather than back to the seat he had.
	//!   - The crew despawns and respawns by proximity like everything else here, and members
	//!     materialise through the AI spawn queue in whatever order it produces. Nothing anywhere
	//!     records that THIS man was the gunner, so a fresh materialisation re-races the seats - the
	//!     same order-of-arrival race that put a passenger in the insertion truck's co-driver seat.
	//!
	//! Chasing either into vanilla would be a much larger job than the symptom warrants, and neither
	//! can be prevented from here. What CAN be guaranteed from here is the property that actually
	//! matters: if there is a gun and there is a spare crewman, the gun is manned. This runs on the
	//! ordinary deployment update, so it self-heals within one tick of whatever moved him.
	//! ==========================================================================================
	//!
	//! ⚠ IT ONLY EVER PROMOTES OUT OF CARGO, NEVER OUT OF THE PILOT SEAT. Taking the driver would strand
	//! a patrol that is mid-route, which is a worse failure than an unmanned gun and a far more visible
	//! one. A vehicle with a driver and no spare passenger therefore keeps its empty turret, correctly:
	//! there is nobody to put in it.
	//!
	//! ⚠ AND IT CANNOT FIGHT ITSELF. The move is only made when the turret is genuinely unoccupied, so
	//! once somebody is in it this method does nothing on every subsequent tick. If vanilla moves him
	//! straight back out again the two would alternate at the update interval - that would be visible in
	//! the log as a repeating "manned the turret" line, which is the signal to go and do the larger
	//! vanilla investigation after all.
	protected void EnsureTurretsManned()
	{
		foreach (Vehicle vehicle : m_aSpawnedVehicles)
		{
			if (!vehicle)
				continue;

			BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(
				vehicle.FindComponent(BaseCompartmentManagerComponent)
			);

			if (!manager)
				continue;

			array<BaseCompartmentSlot> slots = {};
			manager.GetCompartments(slots);

			BaseCompartmentSlot emptyTurret;
			IEntity cargoOccupant;

			foreach (BaseCompartmentSlot slot : slots)
			{
				if (!slot)
					continue;

				if (slot.GetType() == ECompartmentType.TURRET)
				{
					// A vehicle can carry more than one gun; the first unmanned one is the one to fill.
					if (!emptyTurret && !slot.GetOccupant())
						emptyTurret = slot;

					continue;
				}

				if (slot.GetType() != ECompartmentType.CARGO)
					continue;

				if (!cargoOccupant)
					cargoOccupant = slot.GetOccupant();
			}

			if (!emptyTurret || !cargoOccupant)
				continue;

			SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(
				cargoOccupant.FindComponent(SCR_CompartmentAccessComponent)
			);

			if (!access)
				continue;

			// ⚠ A PLAYER IN THE BACK IS NOT CREW AND IS NEVER MOVED. Teleporting somebody who climbed
			// into an occupying-faction vehicle into its turret would be the game playing for them.
			PlayerManager players = GetGame().GetPlayerManager();
			if (players && players.GetPlayerIdFromControlledEntity(cargoOccupant) > 0)
				continue;

			if (access.MoveInVehicle(vehicle, ECompartmentType.TURRET, false, emptyTurret))
			{
				OVT_DeploymentLog.Debug(string.Format("[Overthrow] Deployment '%1': its patrol vehicle was driving with an unmanned gun - moved a passenger into the turret",
					m_ParentDeployment.GetDeploymentName()));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	override array<IEntity> GetSpawnedEntities()
	{
		array<IEntity> entities = new array<IEntity>;

		foreach (Vehicle vehicle : m_aSpawnedVehicles)
		{
			if (vehicle)
				entities.Insert(vehicle);
		}

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return entities;

		foreach (int handle : m_aCrewHandles)
		{
			// "Record exists, entity does not" is a legitimate runtime state, so null is skipped.
			SCR_AIGroup group = virtualization.GetGroup(handle);
			if (group)
				entities.Insert(group);
		}

		return entities;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The registry handles of this module's crews. Read-only to callers.
	array<int> GetCrewHandles()
	{
		return m_aCrewHandles;
	}

	//------------------------------------------------------------------------------------------------
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_VehicleSpawningDeploymentModule clone = new OVT_VehicleSpawningDeploymentModule();

		// EVERY attribute has to appear here - a forgotten one silently ships the class default instead
		// of the authored value, which is exactly how m_fMaxCruiseSpeed was lost for a whole release.
		// Forget m_iSpawnDistanceOverride and every vehicle crew in the campaign registers at the class
		// default rather than the authored ring.
		clone.m_sModuleName = m_sModuleName;
		clone.m_sVehicleType = m_sVehicleType;
		clone.m_iVehicleCount = m_iVehicleCount;
		clone.m_sCrewGroupType = m_sCrewGroupType;
		clone.m_fSpawnRadius = m_fSpawnRadius;
		clone.m_iCostPerVehicle = m_iCostPerVehicle;
		clone.m_bAllowReinforcement = m_bAllowReinforcement;
		clone.m_iReinforcementCost = m_iReinforcementCost;
		clone.m_fMaxCruiseSpeed = m_fMaxCruiseSpeed;
		clone.m_iSpawnDistanceOverride = m_iSpawnDistanceOverride;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	// Virtualization
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Brings this module up to the vehicles and crews it wants. ALWAYS SAFE TO CALL.
	//!
	//! CONVERGE TO WANTED, NEVER SPAWN WANTED - the same contract the infantry module carries, and for
	//! the same reason: this runs from the deployment's activation AND from the manager's
	//! records-restored fan-out, in either order. Reclaiming first is what stops a continued campaign
	//! registering a second crew on top of the one the core has already restored.
	//!
	//! ⚠ A RESTORED CREW COMES BACK WITHOUT ITS TRUCK. Vehicles are not persisted, so after a load the
	//! crews are reclaimed (remembering exactly who died) and a fresh vehicle is spawned for them to
	//! ride in. Pairing is by availability, and a reclaimed crew that is still dormant is moved to its
	//! new vehicle rather than left where the campaign last had it.
	override void EnsureGroups()
	{
		if (!m_ParentDeployment || m_sVehicleType.IsEmpty())
			return;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		string ownerKey = GetOwnerKey();
		if (ownerKey.IsEmpty())
			return;

		ReclaimHandles(virtualization, ownerKey);

		if (m_bSpawnedUnitsEliminated || m_ParentDeployment.GetSpawnedUnitsEliminated())
			return;

		PruneDestroyedVehicles();

		int missing = m_iVehicleCount - m_aSpawnedVehicles.Count();
		if (missing > 0)
			SpawnVehicles(missing);

		PairCrewsToVehicles(virtualization, ownerKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the handle list from the registry, re-tags every crew it finds and drops pairings whose
	//! group entity is gone.
	//!
	//! THE REGISTRY IS THE TRUTH: the list is cleared and re-derived rather than merged into, so a
	//! handle that stopped being ours cannot survive in it.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] ownerKey This module's owner key.
	protected void ReclaimHandles(notnull OVT_VirtualizationManagerComponent virtualization, string ownerKey)
	{
		m_aCrewHandles.Clear();

		array<int> found = virtualization.FindGroupsByOwner(OWNER_SYSTEM, ownerKey);
		foreach (int foundHandle : found)
		{
			if (!virtualization.IsRegistered(foundHandle))
				continue;

			m_aCrewHandles.Insert(foundHandle);
			TagForGameMaster(virtualization.GetGroup(foundHandle));
		}

		if (m_aCrewHandles.Count() > m_iCrewsRegisteredEver)
			m_iCrewsRegisteredEver = m_aCrewHandles.Count();

		PruneCrewVehicleMap();
	}

	//------------------------------------------------------------------------------------------------
	//! Gives every vehicle without a crew one: an already-registered crew that is not riding anything,
	//! or a fresh registration.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] ownerKey This module's owner key.
	protected void PairCrewsToVehicles(notnull OVT_VirtualizationManagerComponent virtualization, string ownerKey)
	{
		if (m_sCrewGroupType.IsEmpty())
			return;

		foreach (Vehicle vehicle : m_aSpawnedVehicles)
		{
			if (!vehicle)
				continue;

			if (IsVehicleCrewed(vehicle))
				continue;

			// A crew that came back out of the registry is somewhere else on the map; a crew registered
			// here and now is already standing beside this truck. Only the first needs moving.
			int handle = TakeUnpairedCrewHandle(virtualization);
			bool reclaimed = handle != -1;

			if (!reclaimed)
				handle = RegisterCrew(virtualization, ownerKey, vehicle);

			if (handle == -1)
			{
				// A refusal is structural - an unresolvable composition, a malformed plan - so every
				// remaining vehicle would fail identically. Stop; the next convergence retries.
				break;
			}

			PairCrew(virtualization, handle, vehicle, reclaimed);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Registers one crew for one vehicle.
	//!
	//! The position is 5 m from the truck, unchanged from the hand-spawned version, and the plan comes
	//! from whichever behaviour module has an opinion - asked BEFORE the group exists, because the core
	//! builds the waypoints at registration and owns them from then on.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] ownerKey This module's owner key.
	//! \param[in] vehicle The vehicle this crew is for.
	//! \return The new handle, or -1 when the registration was refused.
	protected int RegisterCrew(notnull OVT_VirtualizationManagerComponent virtualization, string ownerKey, notnull Vehicle vehicle)
	{
		int factionIndex = m_ParentDeployment.GetControllingFaction();

		string factionKey = ResolveFactionKey(factionIndex);
		if (factionKey.IsEmpty())
		{
			Print(string.Format("[Overthrow] Deployment '%1': faction index %2 resolves to no faction key, cannot register a vehicle crew",
				m_ParentDeployment.GetDeploymentName(), factionIndex), LogLevel.WARNING);
			return -1;
		}

		vector spawnPos = vehicle.GetOrigin() + Vector(CREW_SPAWN_OFFSET_M, 0, 0);
		OVT_VirtualWaypointPlan plan = ResolveVirtualPlan(spawnPos);

		int handle = virtualization.RegisterGroup(OWNER_SYSTEM, ownerKey, factionKey, m_sCrewGroupType,
			spawnPos, plan, m_iSpawnDistanceOverride, CREW_IMPORTANCE);

		if (handle == -1)
		{
			Print(string.Format("[Overthrow] Deployment '%1': registration of crew '%2' (%3) was refused",
				m_ParentDeployment.GetDeploymentName(), m_sCrewGroupType, factionKey), LogLevel.WARNING);
			return -1;
		}

		m_aCrewHandles.Insert(handle);

		if (m_aCrewHandles.Count() > m_iCrewsRegisteredEver)
			m_iCrewsRegisteredEver = m_aCrewHandles.Count();

		TagForGameMaster(virtualization.GetGroup(handle));

		return handle;
	}

	//------------------------------------------------------------------------------------------------
	//! Binds one crew to one vehicle: records the pairing, subscribes the per-member seating callback,
	//! moves the crew to its truck if it is still dormant, and seats anybody already on their feet.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] handle The crew's registry handle.
	//! \param[in] vehicle The vehicle it rides in.
	//! \param[in] anchorToVehicle Move the crew to the vehicle first. True only for a crew reclaimed
	//!            from the registry, which is somewhere else on the map; a crew registered moments ago
	//!            is already beside this truck and moving it would only undo its own spawn position.
	//! \return True when the pairing was made.
	protected bool PairCrew(notnull OVT_VirtualizationManagerComponent virtualization, int handle, notnull Vehicle vehicle, bool anchorToVehicle)
	{
		SCR_AIGroup group = virtualization.GetGroup(handle);
		if (!group)
			return false;

		m_mCrewVehicles.Set(group.GetID(), vehicle.GetID());

		// ⚠ PER MEMBER, NOT PER GROUP. Crew seating used to hang off the group's whole-group init event,
		// which in 1.8 fires only when a group fills COMPLETELY - under AI budget pressure that may
		// never happen, and a core-registered group fills progressively through the engine's spawn
		// queue, so the old hook would frequently have seated nobody at all. (The identifier is left
		// out on purpose: the phase gate greps this directory for it.) GetOnAgentAdded fires once per
		// arriving member.
		//
		// Remove-then-insert because a convergence runs more than once per session and ScriptInvoker does
		// not de-duplicate; a plain Insert would try to seat every arriving crewman twice.
		group.GetOnAgentAdded().Remove(OnCrewAgentAdded);
		group.GetOnAgentAdded().Insert(OnCrewAgentAdded);

		if (anchorToVehicle)
			AnchorCrewToVehicle(virtualization, handle, vehicle);

		// GetOnAgentAdded only fires for members that arrive AFTER this point, and a reclaimed crew is
		// usually already standing, so anyone present is seated now - and woken now. Seating a man whose
		// AI is switched off puts a passenger in the driver's seat, not a driver.
		SeatExistingAgents(group, vehicle);
		OVT_MountedGroupActivation.HoldGroupActive(group);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves a still-dormant crew to its vehicle, so it materialises at the truck rather than wherever
	//! the campaign last recorded it.
	//!
	//! ONLY WHILE DORMANT. Moving a group entity whose members exist would leave the group origin and
	//! the men who belong to it in two different places.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] handle The crew's registry handle.
	//! \param[in] vehicle The vehicle it rides in.
	protected void AnchorCrewToVehicle(notnull OVT_VirtualizationManagerComponent virtualization, int handle, notnull Vehicle vehicle)
	{
		if (virtualization.IsSpawned(handle))
			return;

		vector target = vehicle.GetOrigin() + Vector(CREW_SPAWN_OFFSET_M, 0, 0);
		if (vector.Distance(virtualization.GetPosition(handle), target) < 1)
			return;

		virtualization.SetPosition(handle, target);
	}

	//------------------------------------------------------------------------------------------------
	//! One crew member has arrived in the world. Seat him.
	//!
	//! ⚠ ONE PARAMETER. SCR_AIGroup's own doc comment claims the invoker passes (group, agent); the
	//! actual invocation is Event_OnAgentAdded.Invoke(child) and passes the AGENT alone
	//! (SCR_AIGroup.c:2458-2461, Reforger 1.8.0.10). The group is recovered from the agent.
	//! \param[in] agent The arriving member.
	protected void OnCrewAgentAdded(AIAgent agent)
	{
		if (!agent)
			return;

		AIGroup parent = agent.GetParentGroup();
		if (!parent)
			return;

		EntityID groupId = parent.GetID();
		if (!m_mCrewVehicles.Contains(groupId))
			return;

		// BEFORE THE VEHICLE LOOKUP, because a crewman whose vehicle has gone still fights on foot and
		// the pairing is dropped below - losing the pin on that path would leave a live group asleep
		// until something else woke it. UnsubscribeCrew() hands it back either way.
		OVT_MountedGroupActivation.HoldAgentActive(agent);

		Vehicle vehicle = Vehicle.Cast(GetGame().GetWorld().FindEntityByID(m_mCrewVehicles.Get(groupId)));
		if (!vehicle)
		{
			// The truck is gone (destroyed, or deleted by a teardown). The crew fights on foot; drop the
			// pairing so nothing keeps looking for it.
			m_mCrewVehicles.Remove(groupId);
			return;
		}

		SeatAgent(vehicle, agent);
	}

	//------------------------------------------------------------------------------------------------
	//! Seats every member a group already has.
	//! \param[in] group The crew.
	//! \param[in] vehicle The vehicle it rides in.
	protected void SeatExistingAgents(notnull SCR_AIGroup group, notnull Vehicle vehicle)
	{
		array<AIAgent> agents = {};
		group.GetAgents(agents);

		foreach (AIAgent agent : agents)
		{
			SeatAgent(vehicle, agent);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Puts one crewman in the next free seat: driver's, then a turret, then cargo.
	//!
	//! The order is the one the group-wide version used, applied one man at a time: the first arrival
	//! finds the pilot compartment free and takes it, the second finds it taken and falls through to a
	//! turret, and so on. MoveInVehicle locks the slot it hands out for a frame, so several members
	//! arriving in the same frame cannot be given the same seat.
	//! \param[in] vehicle The vehicle to board.
	//! \param[in] agent The crewman.
	//! \return True when he was seated.
	protected bool SeatAgent(notnull Vehicle vehicle, AIAgent agent)
	{
		if (!agent)
			return false;

		IEntity character = agent.GetControlledEntity();
		if (!character)
			return false;

		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
		if (!access)
			return false;

		// Already aboard something - his own truck, or one he was ordered into. Leave him there.
		if (access.IsInCompartment())
			return false;

		SCR_BaseCompartmentManagerComponent compartmentManager = SCR_BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(SCR_BaseCompartmentManagerComponent)
		);

		if (!compartmentManager)
			return false;

		if (FillCompartment(compartmentManager, agent, ECompartmentType.PILOT))
			return true;

		if (FillCompartment(compartmentManager, agent, ECompartmentType.TURRET))
			return true;

		if (FillCompartment(compartmentManager, agent, ECompartmentType.CARGO))
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool FillCompartment(SCR_BaseCompartmentManagerComponent compartmentManager, AIAgent agent, ECompartmentType type)
	{
		if (!agent || !agent.GetControlledEntity())
			return false;

		IEntity character = agent.GetControlledEntity();

		// Get the character's compartment access component
		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(character.FindComponent(SCR_CompartmentAccessComponent));
		if (!access)
			return false;

		// Get the vehicle entity from the compartment manager
		IEntity vehicle = compartmentManager.GetOwner();
		if (!vehicle)
			return false;

		// Move the character into the vehicle with the specified compartment type
		return access.MoveInVehicle(vehicle, type);
	}

	//------------------------------------------------------------------------------------------------
	//! Stops listening for a crew's members. Called before the group leaves this module's hands, so the
	//! invoker cannot keep a pointer to a module the deployment is about to throw away.
	//! \param[in] group The crew. Null is a no-op.
	protected void UnsubscribeCrew(SCR_AIGroup group)
	{
		if (!group)
			return;

		group.GetOnAgentAdded().Remove(OnCrewAgentAdded);

		// ⚠ THE ONE PLACE THE ACTIVATION PIN IS HANDED BACK, and it is here for exactly the reason this
		// method exists: both teardown paths (OnCleanup and OnVirtualGroupWiped) already funnel through
		// it, so there is one release to keep correct rather than two. AllowMaxLOD on a man who was never
		// pinned is a no-op, so this is safe on any group. The ordinary proximity ring is the backstop
		// underneath it - dormancy deletes the member entities, and a pin cannot outlive its man.
		OVT_MountedGroupActivation.ReleaseGroupActive(group);
	}

	//------------------------------------------------------------------------------------------------
	//! One registered crew has been wiped out - every slot on its roster is dead.
	//!
	//! FIRED ONLY ON A REAL WIPE. The core raises this off the survivor mask, never off an agent count,
	//! so a crew that merely went dormant can no longer be mistaken for a dead one - which is what the
	//! deleted GetAgentsCount() == 0 poll did every time nobody was nearby.
	//! \param[in] handle The wiped crew's registry handle.
	override void OnVirtualGroupWiped(int handle)
	{
		int index = m_aCrewHandles.Find(handle);
		if (index == -1)
			return;

		m_aCrewHandles.Remove(index);

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (virtualization)
		{
			// The record is still there - the core fires this BEFORE it removes it - so the group entity
			// is still resolvable and the subscription can still be taken off it.
			SCR_AIGroup group = virtualization.GetGroup(handle);
			if (group)
			{
				UnsubscribeCrew(group);
				m_mCrewVehicles.Remove(group.GetID());
			}
		}

		if (!m_aCrewHandles.IsEmpty())
			return;

		if (m_iCrewsRegisteredEver <= 0 || m_bSpawnedUnitsEliminated)
			return;

		MarkEliminated("its crew has been wiped out");
	}

	//------------------------------------------------------------------------------------------------
	// Vehicles
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Puts `count` more vehicles in the world. Crews are paired separately.
	//! \param[in] count How many vehicles to add.
	protected void SpawnVehicles(int count)
	{
		if (!m_ParentDeployment || m_sVehicleType.IsEmpty() || count <= 0)
			return;

		vector deploymentPos = m_ParentDeployment.GetPosition();
		int factionIndex = m_ParentDeployment.GetControllingFaction();

		// Get vehicle prefab from faction registry
		ResourceName vehiclePrefab = GetVehiclePrefabFromFaction(factionIndex);
		if (vehiclePrefab.IsEmpty())
		{
			Print(string.Format("Failed to get vehicle prefab for type '%1' from faction %2", m_sVehicleType, factionIndex), LogLevel.ERROR);
			return;
		}

		for (int i = 0; i < count; i++)
		{
			float spawnYaw;
			vector spawnPos = GetRandomSpawnPosition(deploymentPos, spawnYaw);

			// NOTE: the marker entity is deliberately NOT moved to the vehicle. It is the persisted
			// record of this deployment and what GetDeploymentNearPosition measures - it must stay put.
			// (main carried a m_ParentDeployment.GetOwner().SetOrigin(spawnPos) here; v1.5 removed it
			// deliberately and that removal is kept through this merge.)

			// Spawn vehicle ALREADY FACING the right way. The orientation belongs in the spawn
			// transform: this used to spawn level and then call vehicle.SetAngles() on the result,
			// which rotates a live rigid body out from under the physics solver and is what left
			// patrol vehicles standing on their noses at base markers.
			Vehicle vehicle = Vehicle.Cast(SpawnEntity(vehiclePrefab, spawnPos, GetUprightSpawnRotation(spawnYaw)));
			if (!vehicle)
			{
				Print(string.Format("Failed to spawn vehicle %1 (%2)", i + 1, m_sVehicleType), LogLevel.ERROR);
				continue;
			}

			/** Disabling this for now, is bugged in Reforger
			2026-08-17, virtualization/integration T1.6: verified still unapplied by design. CloneModule
			now carries m_fMaxCruiseSpeed through so the config-authored value survives; only the apply
			below stays disabled. Do not re-enable without re-verifying the engine bug.
			AICarMovementComponent carMovementComp = AICarMovementComponent.Cast(vehicle.FindComponent(AICarMovementComponent));
			if(carMovementComp)
			{
				carMovementComp.SetCruiseSpeed(m_fMaxCruiseSpeed);
			}
			*/

			m_aSpawnedVehicles.Insert(vehicle);
			m_iSpawnedCount++;

			OVT_DeploymentLog.Debug(string.Format("Spawned vehicle %1 (%2) at %3", i + 1, m_sVehicleType, spawnPos.ToString()));
		}

		OVT_DeploymentLog.Debug(string.Format("[Overthrow] Vehicle spawning complete: %1/%2 vehicles for type '%3' near %4",
			m_aSpawnedVehicles.Count(), m_iVehicleCount, m_sVehicleType,
			OVT_Global.GetTowns().GetNearestTownName(deploymentPos)));
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] vehicle The vehicle the garbage system is about to book.
	//! \return True when it is one of this patrol's.
	override bool OwnsVehicle(IEntity vehicle)
	{
		if (!vehicle || !m_aSpawnedVehicles)
			return false;

		foreach (Vehicle mine : m_aSpawnedVehicles)
		{
			if (mine == vehicle)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes this module's vehicles at teardown - EXCEPT the ones that have become somebody else's.
	//!
	//! THE 40 m RULE IS GONE. It existed because deactivation used to delete everything on a proximity
	//! timer and a stolen vehicle had to be spared by distance, since distance was the only signal
	//! available at the time. There is no proximity teardown left, so the question is asked directly
	//! instead: is a player or a player's recruit sitting in it, and does a player own it.
	protected void ReleaseVehicles()
	{
		foreach (Vehicle vehicle : m_aSpawnedVehicles)
		{
			if (!vehicle)
				continue;

			string veto = VehicleDeletionVeto(vehicle);
			if (veto != "")
			{
				OVT_DeploymentLog.Debug(string.Format("[Overthrow] Deployment vehicle left standing at teardown: %1", veto));
				continue;
			}

			// ⚠ THE DELETE BRANCH USED TO SAY NOTHING while the veto branch spoke - so "my patrol vehicle
			// vanished" was indistinguishable from a despawn, a garbage collection and a fault (author,
			// 2026-08-26).
			OVT_DeploymentLog.Debug(string.Format("[Overthrow] Deployment vehicle at %1 deleted at teardown",
				vehicle.GetOrigin().ToString()));

			OVT_WorldUtils.DeleteEntityTree(vehicle);
		}

		m_aSpawnedVehicles.Clear();
		m_iSpawnedCount = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Why this vehicle must not be deleted, if it must not be.
	//!
	//! Two independent signals, either of which is enough, both of them the project's existing notions
	//! rather than new ones:
	//!   - A PLAYER OWNER. OVT_PlayerOwnerComponent's uid is stamped by the claim path the moment a
	//!     player sits in the driver's seat of an unowned vehicle, and it is the same test the modded
	//!     SCR_GarbageSystem uses to refuse to collect a car and the vehicle manager uses to find
	//!     player vehicles.
	//!   - AN OCCUPANT THAT BELONGS TO A PLAYER. A player-controlled character covers the passenger
	//!     seat, which the claim path never fires for; a character carrying a player owner uid covers
	//!     that player's RECRUITS, who are not player-controlled but are certainly not ours to delete.
	//! \param[in] vehicle The vehicle to judge.
	//! \return An empty string when it is safe to delete, or the reason it is not.
	protected string VehicleDeletionVeto(notnull Vehicle vehicle)
	{
		return OVT_VehicleClaim.DeletionVeto(vehicle);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops vehicles that no longer exist or are wrecks.
	protected void PruneDestroyedVehicles()
	{
		for (int i = m_aSpawnedVehicles.Count() - 1; i >= 0; i--)
		{
			Vehicle vehicle = m_aSpawnedVehicles[i];

			// 🔴 THE TWO CASES ARE NOT THE SAME EVENT AND THIS USED TO CONFLATE THEM (author,
			// 2026-08-26: a patrol UAZ and its mounted crew vanished together as a GM camera arrived,
			// with nothing in the log and the editor budgets nowhere near full).
			//
			// A WRECK is ordinary - somebody destroyed it. A reference that has gone NULL means the
			// entity was DELETED, and this module deletes its vehicles in exactly one place
			// (ReleaseVehicles, which now says so). A null here with no teardown line above it is
			// therefore somebody else's deletion - the vanilla garbage system, the editor, or the
			// engine - and this is the only place in Overthrow that can witness it.
			//
			// ⚠ A mounted crew is PARENTED to its vehicle, so whoever deletes the vehicle takes the men
			// in the cab with it. One event, not two.
			if (!vehicle)
			{
				OVT_DeploymentLog.Debug(string.Format("[Overthrow] '%1': a patrol vehicle entity has gone - it was DELETED by something, not destroyed. If no teardown line precedes this, it was not this module",
					GetOwnerKey()));

				m_aSpawnedVehicles.Remove(i);
				continue;
			}

			if (!IsVehicleOperational(vehicle))
			{
				OVT_DeploymentLog.Debug(string.Format("[Overthrow] '%1': a patrol vehicle at %2 is a wreck - dropping it",
					GetOwnerKey(), vehicle.GetOrigin().ToString()));

				m_aSpawnedVehicles.Remove(i);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Vehicle destruction accounting.
	//!
	//! KEPT, deliberately, where the group half of this check was deleted. A vehicle is not a group:
	//! the core does not own it, nothing reports its death, and counting wrecks is the only way to know
	//! it is gone. What went with the migration is the half that counted a GROUP's agents - a test 1.8
	//! makes outright wrong, because a dormant or spawn-queued group reports zero agents. Crew death
	//! arrives through OnVirtualGroupWiped() instead.
	protected void CheckVehicleStatus()
	{
		PruneDestroyedVehicles();

		if (m_bSpawnedUnitsEliminated)
			return;

		// "No vehicles" only means elimination if this module ever had any.
		if (m_iSpawnedCount <= 0)
			return;

		if (!m_aSpawnedVehicles.IsEmpty())
			return;

		MarkEliminated("every vehicle has been destroyed");
	}

	//------------------------------------------------------------------------------------------------
	//! Flags this module eliminated and lets the deployment recompute its own flag from all of its
	//! spawning modules.
	//! \param[in] reason What finished it, for the log line.
	protected void MarkEliminated(string reason)
	{
		m_bSpawnedUnitsEliminated = true;

		string deploymentName;
		if (m_ParentDeployment)
			deploymentName = m_ParentDeployment.GetDeploymentName();

		OVT_DeploymentLog.Debug(string.Format("[Overthrow] Vehicle patrol '%1' is finished: %2", deploymentName, reason));

		if (m_ParentDeployment)
			m_ParentDeployment.CheckAllSpawningModulesEliminated();
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsVehicleOperational(Vehicle vehicle)
	{
		if (!vehicle)
			return false;

		// Check if vehicle is destroyed
		SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.Cast(vehicle.FindComponent(SCR_DamageManagerComponent));
		if (damageManager && damageManager.IsDestroyed())
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] vehicle The vehicle to check.
	//! \return True when some crew is already assigned to it.
	protected bool IsVehicleCrewed(notnull Vehicle vehicle)
	{
		for (int i = 0; i < m_mCrewVehicles.Count(); i++)
		{
			// Compared as ENTITIES rather than as ids: an entity pointer comparison has one meaning and
			// needs no assumption about how EntityID compares.
			IEntity paired = GetGame().GetWorld().FindEntityByID(m_mCrewVehicles.GetElement(i));
			if (paired == vehicle)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A registered crew of this module that is not riding anything yet.
	//! \param[in] virtualization The virtualization manager.
	//! \return The handle, or -1 when every crew already has a vehicle.
	protected int TakeUnpairedCrewHandle(notnull OVT_VirtualizationManagerComponent virtualization)
	{
		foreach (int handle : m_aCrewHandles)
		{
			SCR_AIGroup group = virtualization.GetGroup(handle);
			if (!group)
				continue;

			if (m_mCrewVehicles.Contains(group.GetID()))
				continue;

			return handle;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops pairings whose crew group entity no longer exists - which is every one of them after a
	//! load, because the core re-creates a restored group's entity with a fresh id.
	protected void PruneCrewVehicleMap()
	{
		for (int i = m_mCrewVehicles.Count() - 1; i >= 0; i--)
		{
			if (!GetGame().GetWorld().FindEntityByID(m_mCrewVehicles.GetKey(i)))
				m_mCrewVehicles.RemoveElement(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Pick where a patrol vehicle starts, and which way it faces.
	//! \param[in] center Deployment position to search from.
	//! \param[out] spawnYaw Heading in degrees. Zero on the road fallback, which is what an
	//!             unoriented spawn has always produced.
	//! \return The spawn position.
	protected vector GetRandomSpawnPosition(vector center, out float spawnYaw)
	{
		// No heading unless a marker gives us one
		spawnYaw = 0;

		// Try to get a vehicle patrol spawn from the nearest base
		OVT_BaseData nearestBase = OVT_Global.GetOccupyingFaction().GetNearestBase(center);
		if (nearestBase && nearestBase.entId)
		{
			IEntity baseEntity = GetGame().GetWorld().FindEntityByID(nearestBase.entId);
			if (baseEntity)
			{
				OVT_BaseControllerComponent baseController = OVT_BaseControllerComponent.Cast(baseEntity.FindComponent(OVT_BaseControllerComponent));
				if (baseController)
				{
					vector spawnPos;
					if (baseController.GetRandomVehiclePatrolSpawn(spawnPos, spawnYaw))
					{
						return spawnPos;
					}
				}
			}
		}

		// Fall back to finding a random road position
		return OVT_WorldUtils.FindNearestRoad(center);
	}

	//------------------------------------------------------------------------------------------------
	//! The owner key this module's crews are registered under. Composed by the base class from the
	//! deployment's persisted virtual key and this module's authored name.
	//! \return The owner key, or an empty string when there is no deployment to key against.
	protected string GetOwnerKey()
	{
		return BuildOwnerKey(m_sModuleName);
	}

	//------------------------------------------------------------------------------------------------
	// Faction vehicle registry integration
	//------------------------------------------------------------------------------------------------
	protected ResourceName GetVehiclePrefabFromFaction(int factionIndex)
	{
		// Get the OVT_Faction directly by index
		OVT_Faction ovtFaction = OVT_Global.GetFactions().GetOverthrowFactionByIndex(factionIndex);
		if (!ovtFaction)
		{
			Print(string.Format("Failed to get OVT_Faction for index %1", factionIndex), LogLevel.ERROR);
			return "";
		}

		// Initialize vehicle registry if needed
		ovtFaction.InitializeVehicleRegistry();

		// Get the vehicle prefab from the registry
		ResourceName vehiclePrefab = ovtFaction.GetVehiclePrefabByName(m_sVehicleType);
		if (vehiclePrefab.IsEmpty())
		{
			Print(string.Format("Vehicle type '%1' not found in faction '%2' registry", m_sVehicleType, ovtFaction.GetFactionKey()), LogLevel.WARNING);
		}

		return vehiclePrefab;
	}

	//------------------------------------------------------------------------------------------------
	// NO GROUP PREFAB LOOKUP LIVES HERE ANY MORE. A crew's composition is resolved by the
	// virtualization core from (factionKey, groupName) - never from a raw prefab and never from a
	// faction INDEX, because indices are positional and move between saves.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	// Status methods for behavior modules
	//------------------------------------------------------------------------------------------------
	int GetOperationalVehicleCount()
	{
		int count = 0;
		foreach (Vehicle vehicle : m_aSpawnedVehicles)
		{
			if (vehicle && IsVehicleOperational(vehicle))
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	array<Vehicle> GetOperationalVehicles()
	{
		array<Vehicle> vehicles = new array<Vehicle>;
		foreach (Vehicle vehicle : m_aSpawnedVehicles)
		{
			if (vehicle && IsVehicleOperational(vehicle))
				vehicles.Insert(vehicle);
		}
		return vehicles;
	}

	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Vehicle Module: %1", m_sModuleName));
		Print(string.Format("  Vehicles: %1/%2 operational (ever spawned: %3)", GetOperationalVehicleCount(), m_iVehicleCount, m_iSpawnedCount));
		Print(string.Format("  Vehicle Type: %1", m_sVehicleType));
		Print(string.Format("  Crew Type: %1", m_sCrewGroupType));
		Print(string.Format("  Crews: %1 registered (ever held: %2)", m_aCrewHandles.Count(), m_iCrewsRegisteredEver));
		Print(string.Format("  Owner Key: %1", GetOwnerKey()));
		Print(string.Format("  Crew Spawn Distance: %1", m_iSpawnDistanceOverride));
		string reinforcementStatus = "Disabled";
		if (m_bAllowReinforcement)
			reinforcementStatus = "Enabled";
		Print(string.Format("  Reinforcement: %1", reinforcementStatus));

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		foreach (int handle : m_aCrewHandles)
		{
			Print(string.Format("    Handle %1: %2/%3 alive at %4", handle,
				virtualization.GetAliveMemberCount(handle), virtualization.GetMemberCount(handle),
				virtualization.GetPosition(handle).ToString()));
		}
	}
}
