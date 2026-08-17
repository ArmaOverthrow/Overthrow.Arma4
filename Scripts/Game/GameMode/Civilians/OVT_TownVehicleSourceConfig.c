//------------------------------------------------------------------------------------------------
//! One ambient parked vehicle's bookkeeping (implementation.md Phase 5).
//!
//! Deliberately tiny. A parked car owns nothing - no waypoints, no group husk, no member characters -
//! so the record exists for exactly two reasons: to make "every creation site is paired with a
//! deletion site" auditable in one place (DiscardRecord), and to carry the one bit that the prune
//! hook cannot re-derive, m_bClaimed.
//------------------------------------------------------------------------------------------------
class OVT_AmbientVehicleRecord : Managed
{
	//! The vehicle this record belongs to.
	EntityID m_VehicleId;

	//! Set by IsEntityDead() when a player took this car, read by OnEntityPruned() immediately after.
	//!
	//! ⚠ WITHOUT IT THE PRUNE HOOK CANNOT TELL A CLAIMED CAR FROM A WRECK. Both leave the source's
	//! ownership through the same predicate, and they want opposite treatment: a claimed car is
	//! re-tracked for persistence and handed to the vehicle manager, a wreck is left alone to rust
	//! and to disappear with the session.
	bool m_bClaimed;
}

//------------------------------------------------------------------------------------------------
//! THE AUTHORED TEMPLATE for a town's parked civilian vehicles (implementation.md Phase 5, T5.2).
//!
//! Authored once in Configs/Civilians/CivilianAmbience.conf beside `town_civilians`, and bound to a
//! town by OVT_CivilianAmbienceManagerComponent.ActivateTown() exactly as the civilian template is.
//!
//! THE VEHICLE POOL IS THE BASE CLASS'S m_aPrefabs - no new plumbing, and no entity catalog: the
//! game mode's m_CivilianVehicleEntityCatalog is bound to a vanilla conf with zero entries and stays
//! out of this feature's way.
//!
//! WHY THE COUNT IS AUTHORED AS THREE PAIRS instead of the base class's single min/max: a hamlet with
//! four parked cars looks like a car park and a city with one looks abandoned, and the population
//! figure the civilian source scales against is the wrong input for kerbside parking (a fishing
//! village has people but no traffic). Settlement size is the honest input, and three authored pairs
//! keep it a .conf edit rather than a formula nobody can re-tune.
//!
//! ⚠ THE BASE CLASS'S m_iMinCount / m_iMaxCount ARE UNUSED BY THIS SOURCE. They are authored in the
//! .conf for readability only; RollCount() reads the size pairs below and nothing else. Do not "fix"
//! them expecting an effect.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sSourceName")]
class OVT_TownVehicleAmbienceConfig : OVT_AmbientSpawnSourceConfig
{
	[Attribute(defvalue: "0", desc: "VILLAGE: fewest parked cars per activation")]
	int m_iVillageMin;

	[Attribute(defvalue: "1", desc: "VILLAGE: most parked cars per activation")]
	int m_iVillageMax;

	[Attribute(defvalue: "2", desc: "TOWN: fewest parked cars per activation")]
	int m_iTownMin;

	[Attribute(defvalue: "4", desc: "TOWN: most parked cars per activation")]
	int m_iTownMax;

	[Attribute(defvalue: "4", desc: "CITY and CAPITAL: fewest parked cars per activation")]
	int m_iCityMin;

	[Attribute(defvalue: "8", desc: "CITY and CAPITAL: most parked cars per activation")]
	int m_iCityMax;

	[Attribute(defvalue: "35", desc: "How far from a rolled point to look for a kerb or pavement piece to park against. The production idiom searches 20-30 m")]
	float m_fKerbSearchRange;

	[Attribute(defvalue: "120", desc: "How far from a rolled point to look for a road when no kerb was found. Past this the road belongs to somewhere else")]
	float m_fRoadSearchRange;

	[Attribute(defvalue: "4.5", desc: "How far to step sideways from a road CENTRELINE, in metres. Too small parks the car in the driving lane; too large puts it in the hedge. Eden's two-lane roads are ~6 m wide")]
	float m_fRoadLateralOffset;

	[Attribute(defvalue: "6", desc: "How many points to try before giving up on one car. Each attempt is one small sphere query, so this bounds the cost of a town with no roadside at all")]
	int m_iPlacementAttempts;
}

//------------------------------------------------------------------------------------------------
//! THE RUNTIME INSTANCE: one town's parked civilian vehicles (implementation.md Phase 5).
//!
//! Registered on core's ambient seam beside that town's civilian source, by the same manager, at the
//! same moment, and taken down by the same paths (deactivation, and the opt-in QRF suppression).
//!
//! ─────────────────────────────────────────────────────────────────────────────────────────────────
//! THE THREE RULES THIS CLASS EXISTS TO ENFORCE
//! ─────────────────────────────────────────────────────────────────────────────────────────────────
//!
//! 1. AN AMBIENT VEHICLE IS NEVER IN A SAVE (decision D9's one exception, risk R6). Vehicles carry
//!    native persistence components, so - unlike a civilian, which the modded SCR_AIGroup chokepoint
//!    untracks for free - every parked car would write its own save record and come back DUPLICATED
//!    on load, one copy from the record and one from the next ambient roll (the BUG-118 shape).
//!    OnEntitySpawned calls OVT_PersistenceManagerComponent.UntrackTransient() on the very next line
//!    after the spawn, exactly as the town controller's gun dealer does. THAT LINE IS THE PHASE.
//!
//! 2. A CAR A PLAYER TOOK STOPS BEING AMBIENT, AND STARTS BEING PERSISTED. The moment it is claimed
//!    it leaves this source's ownership (so no despawn can ever delete it out from under its driver),
//!    is re-tracked for persistence (CancelUntrackTransient + Track - the recruit-body precedent,
//!    BUG-131) and is handed to OVT_VehicleManagerComponent so the rest of Overthrow can see it.
//!
//! 3. NOTHING IS EVER PARKED BLIND. Every candidate spot is trace-tested with the vehicle-sized box
//!    before core is allowed to spawn on it, and a car that could not be given a spot is released and
//!    deleted rather than dropped into a wall.
//!
//! ⚠ SPOTS COME FROM ROADSIDE GEOMETRY, NEVER FROM OVT_ParkingComponent (decision D2). Eleven building
//! prefabs in the whole tree carry that component; town-wide ambient parking on authored spots would
//! mean authoring thousands of points. Kerb geometry first (OVT_VehicleManagerComponent's own
//! Pavement_/Kerb_ static-model match, which already yaws a car along the kerb), the road network
//! second (OVT_WorldUtils.FindNearestRoadSpawn, which already returns road-aligned angles).
//------------------------------------------------------------------------------------------------
class OVT_TownVehicleSourceConfig : OVT_AmbientSpawnSourceConfig
{
	//------------------------------------------------------------------------------------------------
	// MEMBER VARIABLES
	//------------------------------------------------------------------------------------------------

	//! The authored declaration, read THROUGH rather than copied field by field (decision D5), so a
	//! template that gains an attribute later cannot go missing from a bound instance.
	protected OVT_TownVehicleAmbienceConfig m_Template;

	//! Index into the town manager's town list, or -1 when unbound.
	protected int m_iTownId;

	//! The settlement's size - the whole density input for parked cars.
	protected OVT_TownSize m_eTownSize;

	//! Where this source is (and is re-) registered.
	protected vector m_vTownLocation;

	//! vehicleId -> its record. The one place a car this source owns is written down.
	protected ref map<EntityID, ref OVT_AmbientVehicleRecord> m_mVehicles;

	//! The heading RollPosition() derived for the spot it just returned, consumed by the
	//! OnEntitySpawned() that immediately follows it.
	//!
	//! ⚠ SAME COUPLING, SAME JUSTIFICATION AS THE CIVILIAN SOURCE'S m_sPendingTypeName: core's
	//! SpawnAmbientEntity() does roll -> position -> spawn -> hook synchronously, one entity at a time,
	//! so the value cannot be read by the wrong spawn. If core ever batches, this has to move.
	protected vector m_vPendingAngles;

	//! Set by RollPosition() when it could not find ANY clear roadside spot, read by OnEntitySpawned().
	//!
	//! WHY A FLAG AND NOT A REFUSAL. RollPosition has no way to say "don't spawn this one" - it must
	//! return a position and core spawns on it. So the refusal is carried forward one step: the car is
	//! spawned, immediately released from ambient ownership and deleted. That costs one spawn in the
	//! rare case, and it is the only shape that keeps the obstruction test (T5.5) genuinely BEFORE the
	//! car is allowed to stay.
	protected bool m_bPendingPlacementFailed;

	//------------------------------------------------------------------------------------------------
	void OVT_TownVehicleSourceConfig()
	{
		m_mVehicles = new map<EntityID, ref OVT_AmbientVehicleRecord>();
		m_iTownId = -1;
		m_vPendingAngles = "0 0 0";
	}

	//------------------------------------------------------------------------------------------------
	// BINDING
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Binds this instance to one town. Called exactly once, by the manager, before registration.
	//!
	//! ⚠ THE PREFAB POOL IS ALIASED, NOT COPIED. m_aPrefabs points at the TEMPLATE's array, so the base
	//! class's RollPrefab() works unchanged and a re-authored pool reaches every bound instance. This is
	//! the "no new plumbing" half of T5.2.
	//! \param[in] template The authored declaration to read through.
	//! \param[in] townId Index into the town manager's town list.
	//! \param[in] townSize The settlement's size, which is the density input.
	void Bind(notnull OVT_TownVehicleAmbienceConfig template, int townId, OVT_TownSize townSize)
	{
		m_Template = template;
		m_iTownId = townId;
		m_eTownSize = townSize;

		if (template.m_aPrefabs)
			m_aPrefabs = template.m_aPrefabs;
	}

	//! \return The authored template this source reads through; null only if Bind() was never called.
	OVT_TownVehicleAmbienceConfig GetTemplate()
	{
		return m_Template;
	}

	//! \return The town this source belongs to, or -1 when unbound.
	int GetTownId()
	{
		return m_iTownId;
	}

	//! \return The settlement size this source scales its count against.
	OVT_TownSize GetTownSize()
	{
		return m_eTownSize;
	}

	//! Where this source is (and is re-) registered. Set by the manager at activation.
	//! \param[in] location The town's location.
	void SetTownLocation(vector location)
	{
		m_vTownLocation = location;
	}

	//! \return The position this source was registered at.
	vector GetTownLocation()
	{
		return m_vTownLocation;
	}

	//! \return How many parked cars this source currently has bookkeeping for.
	int GetLiveVehicleCount()
	{
		if (!m_mVehicles)
			return 0;

		return m_mVehicles.Count();
	}

	//------------------------------------------------------------------------------------------------
	// THE OVERRIDES
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! How many cars this town owes for ONE activation, from its size alone.
	//!
	//! Also the source's one housekeeping point, for the same reason the civilian source's RollCount is:
	//! core rolls it exactly once per activation, at the moment this source is provably owed nothing.
	//! \return A car count, never negative.
	override int RollCount()
	{
		SweepOrphanedRecords();

		if (!m_Template)
			return 0;

		int minCount = m_Template.m_iVillageMin;
		int maxCount = m_Template.m_iVillageMax;

		if (m_eTownSize == OVT_TownSize.TOWN)
		{
			minCount = m_Template.m_iTownMin;
			maxCount = m_Template.m_iTownMax;
		}
		else if (m_eTownSize == OVT_TownSize.CITY || m_eTownSize == OVT_TownSize.CAPITAL)
		{
			minCount = m_Template.m_iCityMin;
			maxCount = m_Template.m_iCityMax;
		}

		// RollCountSafe, not RandInt: RandInt is max-exclusive AND RandInt(n, n) raises an engine error,
		// so an authored village pair of 0/0 would error every activation without it.
		int rolled = OVT_VirtualizationMath.RollCountSafe(minCount, maxCount);
		if (rolled < 0)
			return 0;

		return rolled;
	}

	//------------------------------------------------------------------------------------------------
	//! Where ONE car parks: against a kerb if this town has one within reach, otherwise beside a road.
	//!
	//! THE DECISION THIS IMPLEMENTS (spike T5.1, recorded in context.md): kerb-first with a road-derived
	//! fallback. Kerb geometry is the better answer where it exists - the kerb piece's own transform
	//! gives both a spot clear of the carriageway and a heading parallel to the street - but it only
	//! exists where a world author placed pavement, which in Eden means the built-up cores of the larger
	//! settlements and almost nothing in a hamlet. The road network reaches everywhere, at the cost of
	//! needing a lateral step off the centreline that is a guess until somebody looks at it.
	//!
	//! EVERY CANDIDATE IS TRACE-TESTED (T5.5) with the vehicle-sized box, rejecting on
	//! TracePosition() < 0 - the OVT_ParkingComponent idiom, NOT the inverted >= 0 predicate BUG-031
	//! caught. FindSafeSpawnPosition is deliberately not used (a 2 m probe is a known trap for a
	//! vehicle-sized caller); the deployments framework's old shared position validator, which carried
	//! sticky state, was deleted with its file when vehicle patrols were migrated to virtualization.
	//! \param[in] origin The town's location.
	//! \param[in] radius The town's range.
	//! \return A clear roadside spot, or the origin with the failure flag set.
	override vector RollPosition(vector origin, float radius)
	{
		m_bPendingPlacementFailed = true;
		m_vPendingAngles = "0 0 0";

		if (!m_Template || radius <= 0)
			return origin;

		int attempts = m_Template.m_iPlacementAttempts;
		if (attempts < 1)
			attempts = 1;

		for (int i = 0; i < attempts; i++)
		{
			vector scattered = OVT_WorldUtils.GetRandomNonOceanPositionNear(origin, radius);

			vector spot;
			vector angles;

			if (!FindKerbSpot(scattered, spot, angles))
			{
				if (!FindRoadsideSpot(scattered, spot, angles))
					continue;
			}

			if (OVT_WorldUtils.IsOceanAtPosition(spot))
				continue;

			if (!IsSpotClear(spot))
				continue;

			m_vPendingAngles = angles;
			m_bPendingPlacementFailed = false;

			return spot;
		}

		return origin;
	}

	//------------------------------------------------------------------------------------------------
	//! Turns ONE freshly spawned car into a parked one: aligned, untracked, recorded.
	//!
	//! THE ORDER MATTERS. The failed-placement bail-out runs FIRST (nothing else should touch a car that
	//! is about to be deleted), the untrack SECOND (rule 1 - a save taken in the next frame must not
	//! find this car), the alignment THIRD, and the record LAST.
	//!
	//! ⚠ CORE SPAWNS BY POSITION, NOT BY MATRIX, so a car arrives facing world north whatever the road
	//! does. The heading RollPosition() derived is applied here, which is the only hook that can. The
	//! car is empty and one frame old, so SetTransform is safe - the "SetOrigin on an occupant throws
	//! them off the map" trap needs an occupant, and this path can never have one.
	//! \param[in] entity The vehicle core just spawned.
	//! \param[in] sourcePosition The town's location - NOT the car's.
	override void OnEntitySpawned(IEntity entity, vector sourcePosition)
	{
		if (!entity)
			return;

		if (m_bPendingPlacementFailed)
		{
			DiscardUnplaceable(entity);
			return;
		}

		// RULE 1, AND THE MOST LOAD-BEARING LINE IN THE PHASE. A vehicle carries native persistence
		// components: left tracked, every parked car writes a save record and comes back duplicated on
		// load (BUG-118). Untracking is queued when the lazy registration has not landed yet, so this is
		// correct however early it runs.
		OVT_PersistenceManagerComponent.UntrackTransient(entity);

		AlignParkedVehicle(entity);

		// EntityIDs are recycled. A record still filed under this id belongs to a car that no longer
		// exists; closing it properly first is normally a no-op.
		DiscardRecord(entity.GetID());

		OVT_AmbientVehicleRecord record = new OVT_AmbientVehicleRecord();
		record.m_VehicleId = entity.GetID();
		m_mVehicles.Set(entity.GetID(), record);
	}

	//------------------------------------------------------------------------------------------------
	//! Last chance before core deletes a parked car.
	//!
	//! THE OCCUPANCY HATCH (T5.4's belt and braces). A car with somebody in it is not ambient scenery
	//! any more, whatever this source still thinks: deleting it would delete a player (or a player's
	//! recruit) with it. Core documents releasing from inside this hook as the "save this one" escape
	//! hatch, and it is taken here - the car is claimed exactly as the prune path would have claimed it,
	//! and core then skips the deletion because the entity is no longer in its list.
	//!
	//! This is also the path that covers a player driving an ambient car clean out of the town's despawn
	//! ring before the next prune pass could see them.
	//! \param[in] entity The car about to be deleted.
	override void OnEntityDespawning(IEntity entity)
	{
		if (!entity)
			return;

		if (IsVehicleOccupied(entity))
		{
			ClaimVehicle(entity);
			return;
		}

		DiscardRecord(entity.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! Has this car stopped being part of the ambient scenery?
	//!
	//! TWO WAYS OUT, AND THEY SHARE THIS PREDICATE because core's ownership model has exactly one exit
	//! that leaves the entity in the world:
	//!   - IT WAS CLAIMED. Somebody is in it, or Overthrow's claim path has stamped an owner on it. The
	//!     record is marked so OnEntityPruned knows to hand it over rather than leave it.
	//!   - IT IS A WRECK. The base class's damage-state check, unchanged. A burnt-out car is left where
	//!     it died exactly as a civilian's body is, and dies with the session because it is untracked.
	//!
	//! WHY THIS IS ALSO THE "PLAYER GOT IN" TRIGGER, and the one deviation from T5.4's literal text:
	//! Reforger 1.8.0.10 ships NO vehicle-side compartment-entry invoker (SCR_BaseCompartmentManagerComponent
	//! publishes only GetOnDoneSpawningDefaultOccupants; GetOnCompartmentEntered lives on the entering
	//! CHARACTER's SCR_CompartmentAccessComponent). Subscribing per character would mean a global,
	//! per-player subscription with respawn and join-in-progress bookkeeping - exactly the dangling-handler
	//! risk the task wanted avoided - to learn something core's own prune pass asks this source anyway,
	//! every tick, for every car it owns. So the trigger is core's tick: latency is one ambient tick
	//! (~2 s), nothing can happen in that window (the despawn path above covers the only race), and
	//! there is no subscription to leak because there is no subscription.
	//! \param[in] entity The car to judge.
	//! \return True when this source should stop owning it.
	override bool IsEntityDead(IEntity entity)
	{
		if (!entity)
			return false;

		OVT_AmbientVehicleRecord record = m_mVehicles.Get(entity.GetID());

		if (IsVehicleClaimed(entity))
		{
			if (record)
				record.m_bClaimed = true;

			return true;
		}

		return super.IsEntityDead(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! The car has just stopped being ambient: hand it over, or leave it to rust.
	//!
	//! Core has already dropped it from the source's list and its reverse map, so it is nobody's - and
	//! ReleaseAmbientEntity() on it is a documented no-op from here on.
	//! \param[in] entity The car that has just stopped being ambient.
	override void OnEntityPruned(IEntity entity)
	{
		if (!entity)
			return;

		OVT_AmbientVehicleRecord record = m_mVehicles.Get(entity.GetID());

		bool claimed = false;
		if (record)
			claimed = record.m_bClaimed;

		DiscardRecord(entity.GetID());

		if (!claimed)
			return; // a wreck: left where it died, and untracked, so it dies with the session

		AdoptClaimedVehicle(entity);
	}

	//------------------------------------------------------------------------------------------------
	// CLAIMING
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Takes one car out of ambient ownership and gives it to the rest of Overthrow.
	//!
	//! Used by the despawn hatch. The prune path reaches the same end through OnEntityPruned, which core
	//! calls after doing the release itself.
	//! \param[in] entity The car to claim.
	//! \return True when the car was ambient and has been handed over.
	bool ClaimVehicle(IEntity entity)
	{
		if (!entity)
			return false;

		OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
		if (!virt)
			return false;

		if (!virt.ReleaseAmbientEntity(entity))
		{
			// Not ours (already released, or pruned a moment ago). The record still has to go.
			DiscardRecord(entity.GetID());
			return false;
		}

		DiscardRecord(entity.GetID());
		AdoptClaimedVehicle(entity);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! RULE 2, the other half of T5.3's untrack: a car somebody took must survive a save.
	//!
	//! THE RECRUIT-BODY PRECEDENT VERBATIM (BUG-131). The spawn-side UntrackTransient either already
	//! released this entity or still holds a QUEUED release for it, because the native persistence
	//! registration is lazy - so cancelling the queued release is not optional, and neither is the
	//! IsTracked() re-check before tracking again.
	//!
	//! The vehicle manager registration is separate and deliberately unconditional-but-idempotent: the
	//! claim user action stamps ownership and calls RegisterPlayerVehicle itself, but a car whose only
	//! occupant is a passenger has no owner to register under and still has to be visible to the rest of
	//! the game.
	//! \param[in] entity The claimed car.
	protected void AdoptClaimedVehicle(notnull IEntity entity)
	{
		OVT_PersistenceManagerComponent.CancelUntrackTransient(entity);
		if (!OVT_PersistenceTracking.IsTracked(entity))
			OVT_PersistenceTracking.Track(entity);

		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		if (vehicles && vehicles.m_aVehicles)
		{
			// Remove-then-insert, the manager's own idiom: a second claim of the same car cannot leave
			// two entries behind.
			vehicles.m_aVehicles.RemoveItem(entity.GetID());
			vehicles.m_aVehicles.Insert(entity.GetID());
		}

		Print(string.Format("[Overthrow] CivilianAmbience: an ambient parked vehicle in town %1 was claimed - it is now persisted and managed", m_iTownId.ToString()), LogLevel.VERBOSE);
	}

	//------------------------------------------------------------------------------------------------
	//! Has a player taken this car?
	//!
	//! Two independent signals, either of which is enough:
	//!   - AN OWNER UID. Overthrow's claim path (SCR_GetInUserAction -> OVT_VehicleRequestComponent)
	//!     stamps one the moment a player sits in the driver's seat of an unowned vehicle, server-side.
	//!   - A PLAYER-CONTROLLED OCCUPANT. Covers the passenger seat, which the claim path never fires for.
	//! \param[in] entity The car to test.
	//! \return True when the car belongs to a player now.
	protected bool IsVehicleClaimed(notnull IEntity entity)
	{
		OVT_PlayerOwnerComponent owner = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(entity);
		if (owner && owner.GetPlayerOwnerUid() != "")
			return true;

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return false;

		array<IEntity> occupants = GetVehicleOccupants(entity);
		foreach (IEntity occupant : occupants)
		{
			if (players.GetPlayerIdFromControlledEntity(occupant) > 0)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Is anybody at all inside - player, recruit or otherwise?
	//!
	//! Broader than IsVehicleClaimed on purpose: the despawn hatch must not delete a car with a player's
	//! RECRUIT in it either.
	//! \param[in] entity The car to test.
	//! \return True when at least one compartment is occupied.
	protected bool IsVehicleOccupied(notnull IEntity entity)
	{
		return !GetVehicleOccupants(entity).IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Every character currently sitting in a vehicle.
	//!
	//! Walks the compartments rather than asking Vehicle.IsOccupied(), which answers through the faction
	//! component and returns false outright when the vehicle has none.
	//! \param[in] entity The vehicle.
	//! \return The occupants; empty when there are none or the entity is not a vehicle.
	protected array<IEntity> GetVehicleOccupants(notnull IEntity entity)
	{
		array<IEntity> occupants = new array<IEntity>();

		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(entity.FindComponent(BaseCompartmentManagerComponent));
		if (!compartments)
			return occupants;

		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);

		foreach (BaseCompartmentSlot slot : slots)
		{
			if (!slot)
				continue;

			IEntity occupant = slot.GetOccupant();
			if (occupant)
				occupants.Insert(occupant);
		}

		return occupants;
	}

	//------------------------------------------------------------------------------------------------
	// PLACEMENT
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! A parking spot against the nearest kerb or pavement piece.
	//!
	//! Straight through OVT_VehicleManagerComponent.FindNearestKerbParking, which is production code
	//! (the starting-car path and every shop purchase use it): it matches Pavement_/Kerb_ static models,
	//! steps 3 m off the piece and yaws the car 90 degrees so it stands PARALLEL to the kerb rather than
	//! nose-in. Reusing it wholesale is the point - a second implementation of kerb geometry would be a
	//! second thing to get wrong.
	//! \param[in] near The rolled point to search around.
	//! \param[out] position The parking spot.
	//! \param[out] angles Yaw/pitch/roll along the kerb.
	//! \return True when a kerb was found in range.
	protected bool FindKerbSpot(vector near, out vector position, out vector angles)
	{
		position = near;
		angles = "0 0 0";

		if (!m_Template)
			return false;

		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		if (!vehicles)
			return false;

		vector mat[4];
		if (!vehicles.FindNearestKerbParking(near, m_Template.m_fKerbSearchRange, mat))
			return false;

		position = mat[3];
		angles = Math3D.MatrixToAngles(mat);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A parking spot beside the nearest road, when this part of town has no kerb.
	//!
	//! FindNearestRoadSpawn projects onto the road POLYLINE, so its answer is the middle of the
	//! carriageway - parking there would leave a car straddling the white line. The lateral step is the
	//! whole difference, and its size (m_fRoadLateralOffset) is the one number in this phase that only a
	//! play-test can settle: too small and the car sits in the lane, too large and it is in the hedge.
	//! The side is rolled so a street does not end up with every car on the same verge.
	//! \param[in] near The rolled point to search around.
	//! \param[out] position The parking spot, offset from the centreline.
	//! \param[out] angles Yaw/pitch/roll along the road.
	//! \return True when a road was found in range.
	protected bool FindRoadsideSpot(vector near, out vector position, out vector angles)
	{
		position = near;
		angles = "0 0 0";

		if (!m_Template)
			return false;

		vector roadPosition;
		vector roadAngles;
		if (!OVT_WorldUtils.FindNearestRoadSpawn(near, m_Template.m_fRoadSearchRange, roadPosition, roadAngles))
			return false;

		vector roadMat[4];
		Math3D.AnglesToMatrix(roadAngles, roadMat);

		// roadMat[0] is the RIGHT vector of a matrix built from the road's direction of travel, so this
		// steps squarely onto one verge or the other.
		float offset = m_Template.m_fRoadLateralOffset;
		if (s_AIRandomGenerator.RandInt(0, 2) == 0)
			offset = -offset;

		vector candidate = roadPosition + (roadMat[0] * offset);

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		candidate[1] = world.GetSurfaceY(candidate[0], candidate[2]);

		position = candidate;
		angles = roadAngles;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Is there room for a car here (T5.5)?
	//!
	//! The vehicle-sized box OVT_WorldUtils.FindVehicleSpawnNear uses, and the OVT_ParkingComponent
	//! predicate: TracePosition() < 0 means the box hit something, which is a REJECT. BUG-031 was the
	//! same test written the other way round, so the direction is spelled out here rather than left to
	//! a reader's memory.
	//! \param[in] position The candidate spot.
	//! \return True when a car fits.
	protected bool IsSpotClear(vector position)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		TraceBox trace = new TraceBox;
		trace.Flags = TraceFlags.ENTS;
		trace.Start = position;
		trace.Mins = "-1.5 0 -3";
		trace.Maxs = "1.5 2.5 3";

		return world.TracePosition(trace, null) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Points a freshly spawned car along the road or kerb it was placed against.
	//! \param[in] entity The car.
	protected void AlignParkedVehicle(notnull IEntity entity)
	{
		vector mat[4];
		Math3D.AnglesToMatrix(m_vPendingAngles, mat);
		mat[3] = entity.GetOrigin();

		entity.SetTransform(mat);

		// The tree's own settle idiom (OVT_FlipVehicleAction): a rigid body moved by SetTransform needs
		// a nudge to wake its physics, or it can hang a few centimetres proud of the ground.
		Physics physics = entity.GetPhysics();
		if (physics)
			physics.ApplyImpulse("0 -1 0");
	}

	//------------------------------------------------------------------------------------------------
	//! Gets rid of a car that could not be given a spot.
	//!
	//! Released FIRST: core records an entity before it runs the spawn hook, so deleting one without
	//! releasing would leave a dangling reference in its list until the next prune.
	//! \param[in] entity The unplaceable car.
	protected void DiscardUnplaceable(notnull IEntity entity)
	{
		OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
		if (virt)
			virt.ReleaseAmbientEntity(entity);

		SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}

	//------------------------------------------------------------------------------------------------
	// BOOKKEEPING
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! THE ONE DELETION SITE for a vehicle record - despawn, prune, claim and the orphan sweep all
	//! funnel through here, so the "every creation site is paired with a deletion site" audit is one
	//! function rather than four.
	//! \param[in] vehicleId The record to close.
	protected void DiscardRecord(EntityID vehicleId)
	{
		if (!m_mVehicles)
			return;

		m_mVehicles.Remove(vehicleId);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops records whose vehicle no longer exists.
	//!
	//! Core cannot hook the case where a car is destroyed by something else entirely (a GM deleting it,
	//! a world edit): the reference simply becomes null and core's prune drops it before any config hook
	//! runs. RollCount() is the one moment this source is provably owed nothing, so anything still in the
	//! map whose entity has gone is by definition an orphan.
	//!
	//! Cheap by construction - a town has single-digit cars.
	protected void SweepOrphanedRecords()
	{
		if (!m_mVehicles || m_mVehicles.IsEmpty())
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		array<EntityID> orphans = new array<EntityID>();
		for (int i = 0; i < m_mVehicles.Count(); i++)
		{
			EntityID vehicleId = m_mVehicles.GetKey(i);
			if (!world.FindEntityByID(vehicleId))
				orphans.Insert(vehicleId);
		}

		foreach (EntityID vehicleId : orphans)
		{
			DiscardRecord(vehicleId);
		}
	}
}
