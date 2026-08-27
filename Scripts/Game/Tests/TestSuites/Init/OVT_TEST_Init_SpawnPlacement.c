//------------------------------------------------------------------------------------------------
//! TIER B - the spawn-placement contract (BUG-195).
//!
//! WHY THIS FILE EXISTS. Every defect BUG-195 records was a silent one: a blind
//! GetRandomElement(), a GetSurfaceY() that ignored the floor the point sat on, a probe that traced
//! ENTS while the thing burying it was terrain. None of them fails a compile, none of them logs,
//! and the only symptom is a body in a wall that a player has to report.
//!
//! FOUR CLAIMS, one per case:
//!   1. ResolveGroundY() answers a GROUND height even when its trace starts inside geometry;
//!   2. FindSpawnPositionOutside() only returns positions IsPositionClear accepts, and only positions
//!      outside the building's footprint - the producer/acceptor agreement
//!      OVT_TownController.SpawnGunDealer depends on to stop rerolling the dealer's house;
//!   3. GetVehicleSpawnPoint() refuses rather than handing back a spot it can see is blocked;
//!   4. GetSpawnPoint() applies the authored offset and validates it, instead of answering with the
//!      owner's own origin.
//!
//! NOTE ON INDOORS. An authored point INSIDE a building is correct, not a defect - houses are bought
//! and fast-travelled to, and the civilian placement config ranks interior spots as its best
//! candidates. Nothing here asserts a position is outdoors, and nothing should.
//!
//! ⚠ WHAT THESE CANNOT CATCH. Cases 2 and 3 assert a CONDITIONAL: "when the search succeeds, its
//! answer is fit". In the test world - one town, one base, open ground - the searches essentially
//! always succeed on the first ring, so the blocked branches are not exercised here. Case 1's
//! fallback is likewise only reached if the engine reports a zero-fraction TraceMove from inside
//! terrain; if it reports no hit at all instead, the case passes on the pre-fix code too. Both
//! limits are the price of asserting against a live world rather than a fixture.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! ResolveGroundY() never answers with the trace's own start height.
//!
//! The hit-point formula is `Start[1] - (length * fraction)`. At fraction 0 - a trace that begins
//! INSIDE geometry - that evaluates to Start[1], which is searchUp ABOVE the input and is not a
//! ground height at all. The caller then adds SPAWN_GROUND_CLEARANCE on top of it. Sampled from
//! deep under the terrain so the trace is unambiguously starting inside something solid.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_SpawnPlacement_GroundResolvesFromInsideGeometry : SCR_AutotestCaseBase
{
	//! How far under the terrain the sample sits. Must exceed ResolveGroundY's default searchDown.
	static const float BURIAL_DEPTH = 50.0;

	//! The terrain is not flat under an arbitrary XZ, and GetSurfaceY is the reference answer, so
	//! this only needs to separate "the surface" from "49 m underground".
	static const float TOLERANCE = 1.0;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.IsEmpty())
		{
			SetFailure("No town is registered, so there is no known-good patch of terrain to sample");
			return true;
		}

		BaseWorld world = GetGame().GetWorld();
		vector sample = towns.m_Towns[0].location;
		float surface = world.GetSurfaceY(sample[0], sample[2]);

		sample[1] = surface - BURIAL_DEPTH;

		float resolved = OVT_WorldUtils.ResolveGroundY(sample);

		if (Math.AbsFloat(resolved - surface) > TOLERANCE)
		{
			SetFailure("ResolveGroundY() answered %1 for a point %2 m under a surface at %3. A downward ground trace that starts inside geometry must fall back to the terrain height, not to its own start point.",
				resolved.ToString(), BURIAL_DEPTH.ToString(), surface.ToString());
			return true;
		}

		Print("ResolveGroundY: buried sample resolved to " + resolved.ToString() + ", surface " + surface.ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! FindSpawnPositionOutside() emits only what IsPositionClear accepts, outside the footprint.
//!
//! THIS IS THE PRODUCER/ACCEPTOR AGREEMENT, and it is load-bearing rather than cosmetic:
//! SpawnGunDealer PERSISTS what this returns and re-judges it with IsPositionClear on the next load.
//! If the two can ever disagree, the dealer is relocated to a different house - and his map marker
//! with him - every single session.
//! Also asserts the answer is outside the building's world bounds, which is the whole reason the
//! method exists: a 2 m probe anchored on a building ORIGIN stays in the living room.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_SpawnPlacement_OutsideSearchIsClearAndOutside : SCR_AutotestCaseBase
{
	static const ResourceName SUBJECT = "{F634370733F16BC0}Prefabs/Structures/Military/FOB/OVT_MedicalTent.et";

	protected IEntity m_Subject;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.IsEmpty())
		{
			SetFailure("No town is registered, so there is nowhere sensible to put a test building");
			return true;
		}

		vector position = towns.m_Towns[0].location + "780 0 660";
		m_Subject = OVT_Global.SpawnEntityPrefab(SUBJECT, position);
		if (!m_Subject)
		{
			SetFailure("SpawnEntityPrefab() produced no entity from %1 - there is no building to search around", SUBJECT);
			return true;
		}

		vector found;
		if (!OVT_WorldUtils.FindSpawnPositionOutside(m_Subject, found))
		{
			// Not a failure: a refusal is the honest answer when nothing clear exists, and it is
			// exactly what keeps a bad position from being persisted. See the file header's ⚠.
			Print("FindSpawnPositionOutside refused - nothing clear near the test building");
			return FinishAndCleanUp();
		}

		if (!OVT_WorldUtils.IsPositionClear(found))
		{
			SetFailure("FindSpawnPositionOutside() returned %1, which IsPositionClear rejects. A position this path emits and SpawnGunDealer stores must survive being re-judged on the next load, or the dealer rerolls his house every session.",
				found.ToString());
			return FinishAndCleanUp();
		}

		vector boundsMin, boundsMax;
		m_Subject.GetWorldBounds(boundsMin, boundsMax);

		bool insideX = found[0] > boundsMin[0] && found[0] < boundsMax[0];
		bool insideZ = found[2] > boundsMin[2] && found[2] < boundsMax[2];

		if (insideX && insideZ)
		{
			SetFailure("FindSpawnPositionOutside() returned %1, which is inside the building's own world bounds. Standing things INSIDE the shell is the defect this method exists to avoid.",
				found.ToString());
			return FinishAndCleanUp();
		}

		Print("FindSpawnPositionOutside: " + found.ToString() + " is clear and outside the footprint");
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool FinishAndCleanUp()
	{
		if (m_Subject)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_Subject);
			m_Subject = null;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! GetVehicleSpawnPoint() reports true only for a spot it has proven clear.
//!
//! Both callers read true as "placed" and spawn a vehicle there, so a true carrying a blocked spot
//! lands the vehicle on top of whatever is parked in it. False is a legitimate answer - HighCommand
//! and FindVehicleSpawnNear both fall through to a ring search on it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_SpawnPlacement_VehiclePointsAreProvenClear : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		IEntity owner = OVT_TEST_SpawnPointSubject.FindBaseController();
		if (!owner)
		{
			SetFailure("No base controller in the world, so there is no entity authoring vehicle spawn points. This is OVT_TEST_Init_Controllers_AreRegistered's territory.");
			return true;
		}

		OVT_SpawnPointComponent points = OVT_SpawnPointComponent.Cast(owner.FindComponent(OVT_SpawnPointComponent));
		if (!points || !points.HasVehicleSpawnPoints())
		{
			SetFailure("The base controller carries no authored vehicle spawn points - OVT_BaseController.et's m_aVehiclePoints block has been dropped");
			return true;
		}

		vector position, angles;
		if (!points.GetVehicleSpawnPoint(position, angles))
		{
			// A refusal is the contract, not a defect. See the file header's ⚠.
			Print("GetVehicleSpawnPoint refused - every authored vehicle point is blocked");
			return true;
		}

		if (!OVT_WorldUtils.IsPositionClear(position, OVT_SpawnPointComponent.VEHICLE_MINS, OVT_SpawnPointComponent.VEHICLE_MAXS, owner))
		{
			SetFailure("GetVehicleSpawnPoint() reported true for %1, but a vehicle-sized box there collides. Its callers treat true as 'placed' and will drop a vehicle on whatever is already in that spot.",
				position.ToString());
			return true;
		}

		Print("GetVehicleSpawnPoint: " + position.ToString() + " is clear at the vehicle box");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! GetSpawnPoint() applies the authored offset and validates the result.
//!
//! Two ways this used to be wrong at once: the owner's ORIGIN came back whenever the offset was not
//! applied, and the chosen point was handed out without ever being traced. The base controller
//! authors four points 2 m out on each side, so an answer within a metre of the origin means the
//! offset was lost.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_SpawnPlacement_AuthoredPointsAreOffsetAndValidated : SCR_AutotestCaseBase
{
	//! Under the smallest authored offset (2 m) and clear of float noise.
	static const float MIN_OFFSET = 1.0;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		IEntity owner = OVT_TEST_SpawnPointSubject.FindBaseController();
		if (!owner)
		{
			SetFailure("No base controller in the world, so there is no entity authoring spawn points. This is OVT_TEST_Init_Controllers_AreRegistered's territory.");
			return true;
		}

		OVT_SpawnPointComponent points = OVT_SpawnPointComponent.Cast(owner.FindComponent(OVT_SpawnPointComponent));
		if (!points || !points.HasSpawnPoints())
		{
			SetFailure("The base controller carries no authored spawn points - OVT_BaseController.et's m_aPoints block has been dropped");
			return true;
		}

		vector origin = owner.GetOrigin();
		vector spawn = points.GetSpawnPoint();

		float offset = vector.Distance(Vector(spawn[0], 0, spawn[2]), Vector(origin[0], 0, origin[2]));
		if (offset < MIN_OFFSET)
		{
			SetFailure("GetSpawnPoint() answered %1 for an owner at %2 - only %3 m apart. The authored offset was not applied, so this is the owner's own origin, which for a building is a point inside its shell.",
				spawn.ToString(), origin.ToString(), offset.ToString());
			return true;
		}

		if (!OVT_WorldUtils.IsPositionClear(spawn, OVT_SpawnPointComponent.PERSON_MINS, OVT_SpawnPointComponent.PERSON_MAXS, owner))
		{
			SetFailure("GetSpawnPoint() handed back %1, where a person-sized box collides, while other authored points were available. The point is being chosen without being traced.",
				spawn.ToString());
			return true;
		}

		Print("GetSpawnPoint: " + spawn.ToString() + " is " + offset.ToString() + " m from the owner origin and clear");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Locates the test world's spawn-point-authoring subject.
//!
//! By registry rather than by prefab path: the base controller is the one entity in the test world
//! that authors BOTH m_aPoints and m_aVehiclePoints, and the occupying manager already holds it.
//------------------------------------------------------------------------------------------------
class OVT_TEST_SpawnPointSubject : Managed
{
	//------------------------------------------------------------------------------------------------
	//! \return The first registered base controller entity, or null when none is registered.
	static IEntity FindBaseController()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying || occupying.m_Bases.IsEmpty())
			return null;

		return GetGame().GetWorld().FindEntityByID(occupying.m_Bases[0].entId);
	}
}
