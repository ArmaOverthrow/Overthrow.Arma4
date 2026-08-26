//------------------------------------------------------------------------------------------------
//! Stateless world-space helpers: spawning entities, finding safe/valid positions, querying nearby
//! entities and players, and road lookups.
//!
//! Split out of OVT_Global so the locator holds manager accessors and client seams only. Every method
//! here moved verbatim - behaviour is unchanged. OVT_Global keeps one-line forwarders for the two
//! highest-traffic entry points (SpawnEntityPrefab, PlayerInRange); everything else is called on this
//! class directly.
class OVT_WorldUtils : Managed
{
	static bool PlayerInRange(vector pos, int range)
	{		
		array<int> players = new array<int>;
		PlayerManager mgr = GetGame().GetPlayerManager();
		int numplayers = mgr.GetPlayers(players);
		
		if(numplayers > 0)
		{
			IEntity player;
			DamageManagerComponent dmg;
			foreach(int playerID : players)
			{
				player = mgr.GetPlayerControlledEntity(playerID);				
				if(!player) continue;
				dmg = DamageManagerComponent.Cast(player.FindComponent(DamageManagerComponent));
				if(dmg && dmg.IsDestroyed())
				{
					//Is dead, ignore
					continue;
				}
				float distance = vector.Distance(player.GetOrigin(), pos);
				if(distance < range)
				{
					return true;
				}
			}
		}
		
		return false;
	}
	// Static array for spawn point search results
	static ref array<IEntity> s_SpawnPointSearchResults;

	//! How far above the standing surface a resolved spawn position is placed, so the character drops
	//! onto the floor instead of starting fractionally inside it.
	static const float SPAWN_GROUND_CLEARANCE = 0.5;

	//! Ring radii used when the close-in probes are all inside whatever the caller is trying to get out
	//! of. Deliberately short: a spawn that walks the player across the street is a nuisance, a spawn
	//! that walks them across the town is a bug of its own.
	static const ref array<float> SPAWN_RING_RADII = {4.0, 6.0, 8.0, 12.0};

	//------------------------------------------------------------------------------------------------
	//! The height a character standing at this XZ should have.
	//!
	//! GetSurfaceY answers for the TERRAIN ONLY, so any point sitting on something the terrain does not
	//! know about - a building floor, a raised foundation, a concrete pad, a jetty - is dropped into
	//! whatever is underneath it, which is the inside of the structure. Trace down through terrain AND
	//! entities from just above the point instead, and keep GetSurfaceY as the answer when the trace
	//! finds nothing (a point over a cliff edge, or one whose owner is floating).
	//! \param[in] pos The position whose XZ is being resolved; its Y is the vertical search anchor.
	//! \param[in] searchUp How far above pos to start the downward trace.
	//! \param[in] searchDown How far below pos to give up.
	//! \return The Y of the first surface found, else the terrain height.
	static float ResolveGroundY(vector pos, float searchUp = 1.0, float searchDown = 3.0)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return pos[1];

		TraceParam param = new TraceParam();
		param.Start = pos + Vector(0, searchUp, 0);
		param.End = pos - Vector(0, searchDown, 0);
		param.Flags = TraceFlags.WORLD | TraceFlags.ENTS;

		float travelled = world.TraceMove(param, null);
		if (travelled < 1.0)
			return param.Start[1] - ((searchUp + searchDown) * travelled);

		return world.GetSurfaceY(pos[0], pos[2]);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a body-sized box at this position is free of geometry.
	//!
	//! ENTS only, matching every other clearance test here: the box is placed clear of the surface, so
	//! including terrain would make the ground itself read as an obstruction.
	//! \param[in] pos The position to test.
	//! \param[in] mins Box minimum corner, relative to pos.
	//! \param[in] maxs Box maximum corner, relative to pos.
	//! \param[in] exclude An entity the test should ignore - normally the thing the position belongs to.
	//! \return True when nothing is in the way.
	static bool IsPositionClear(vector pos, vector mins = "-0.5 0 -0.5", vector maxs = "0.5 2 0.5", IEntity exclude = null)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return true;

		TraceBox trace = new TraceBox;
		trace.Flags = TraceFlags.ENTS;
		trace.Start = pos;
		trace.Mins = mins;
		trace.Maxs = maxs;
		trace.Exclude = exclude;

		return world.TracePosition(trace, null) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether something solid is directly overhead.
	//!
	//! THE ONLY TEST THAT TELLS "inside a building" APART FROM "in the open". A clearance box cannot:
	//! an empty room passes it exactly as a field does, which is why a gun dealer standing in somebody's
	//! living room looked like a perfectly good placement to every check Overthrow had. Used to judge a
	//! position that was MEANT to be outdoors - never to reject one that was deliberately put indoors.
	//! \param[in] pos The standing position to test.
	//! \param[in] height How far up to look.
	//! \return True when the position is roofed.
	static bool HasGeometryOverhead(vector pos, float height = 5.0)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		TraceParam param = new TraceParam();
		param.Start = pos + Vector(0, 0.5, 0);
		param.End = pos + Vector(0, height, 0);
		param.Flags = TraceFlags.ENTS;

		return world.TraceMove(param, null) < 1.0;
	}

	//------------------------------------------------------------------------------------------------
	//! A clear, grounded standing position just OUTSIDE a building's footprint.
	//!
	//! TryFindSafeSpawnPosition samples a 2 m sphere centred on what it is given, so anchoring it on a
	//! building ORIGIN - a point inside the shell - answers with another point inside the shell, and an
	//! open room passes the clearance test perfectly well. That is what puts gun dealers in living
	//! rooms. This starts outside the entity's world bounds and rings outward instead.
	//! \param[in] building The building to stand next to.
	//! \param[out] foundPos A clear position outside it, or its origin when there is none.
	//! \param[in] mins Clearance box minimum corner.
	//! \param[in] maxs Clearance box maximum corner.
	//! \return True when a clear position outside the footprint was found.
	static bool FindSpawnPositionOutside(notnull IEntity building, out vector foundPos, vector mins = "-0.5 0 -0.5", vector maxs = "0.5 2 0.5")
	{
		foundPos = building.GetOrigin();

		vector boundsMin, boundsMax;
		building.GetWorldBounds(boundsMin, boundsMax);

		vector centre = (boundsMin + boundsMax) * 0.5;
		centre[1] = foundPos[1];

		float footprint = Math.Max(boundsMax[0] - boundsMin[0], boundsMax[2] - boundsMin[2]) * 0.5;

		vector firstClear;
		bool haveClear = false;

		for (int step = 0; step < 4; step++)
		{
			float radius = footprint + 2.0 + (step * 3.0);
			for (int bearing = 0; bearing < 12; bearing++)
			{
				float angle = bearing * 30.0 * Math.DEG2RAD;
				vector candidate = centre + Vector(Math.Sin(angle) * radius, 0, Math.Cos(angle) * radius);
				candidate[1] = ResolveGroundY(Vector(candidate[0], centre[1], candidate[2])) + SPAWN_GROUND_CLEARANCE;

				if (!IsPositionClear(candidate, mins, maxs))
					continue;

				if (!haveClear)
				{
					firstClear = candidate;
					haveClear = true;
				}

				// OPEN SKY IS THE POINT, not merely a gap. A bounding box is quantized and can end short
				// of a wing, an awning or a porch, so a "clear" candidate can still be under the very
				// roof this is escaping - and a caller that re-judges its own recorded position would
				// then move it again every session. Take a roofed spot only if nothing better exists.
				if (!HasGeometryOverhead(candidate))
				{
					foundPos = candidate;
					return true;
				}
			}
		}

		if (haveClear)
		{
			foundPos = firstClear;
			return true;
		}

		return false;
	}

	static vector FindSafeSpawnPosition(vector pos, vector mins = "-0.5 0 -0.5", vector maxs = "0.5 2 0.5", bool skipSpawnPointSearch = false)
	{
		vector foundPos;
		TryFindSafeSpawnPosition(pos, foundPos, mins, maxs, skipSpawnPointSearch);
		return foundPos;
	}

	//! As FindSafeSpawnPosition, but the failure is REPORTED instead of hidden: when neither an
	//! authored spawn point nor the random search finds a clear spot, foundPos is the input position
	//! unchanged and the return is false, so the caller chooses its own fallback rather than
	//! inheriting a position that is known to collide. FindSafeSpawnPosition's silent return of the
	//! original position is what embedded fast-travelled recruits inside starter-house walls - the
	//! 30 probes all collide inside a wall cavity and the colliding input came back looking like an
	//! answer.
	static bool TryFindSafeSpawnPosition(vector pos, out vector foundPos, vector mins = "-0.5 0 -0.5", vector maxs = "0.5 2 0.5", bool skipSpawnPointSearch = false)
	{
		foundPos = pos;

		// First check for nearby entities with spawn point components (unless skipped for performance)
		if (!skipSpawnPointSearch)
		{
			if (!s_SpawnPointSearchResults)
				s_SpawnPointSearchResults = {};
			
			s_SpawnPointSearchResults.Clear();
			GetGame().GetWorld().QueryEntitiesBySphere(pos, 15, null, FilterSpawnPointEntities, EQueryEntitiesFlags.ALL);
			
			// If we found spawn point components, use the closest one exactly (no ground adjustment)
			if (s_SpawnPointSearchResults.Count() > 0)
			{
				IEntity closestEntity = null;
				float closestDistance = 999999;
				
				// Find the closest spawn point entity
				foreach (IEntity entity : s_SpawnPointSearchResults)
				{
					float distance = vector.Distance(entity.GetOrigin(), pos);
					if (distance < closestDistance)
					{
						closestDistance = distance;
						closestEntity = entity;
					}
				}
				
				if (closestEntity)
				{
					// HasSpawnPoints() first: GetSpawnPoint() falls back to the HOLDER'S OWN ORIGIN when
					// nothing is authored, and for a building that is a point inside it - the exact
					// answer this method exists to avoid. The clearance re-test is for the other half:
					// an authored point can be blocked by something that was not there when it was
					// authored (a placeable, a wreck, a neighbour's fence), and falling through to the
					// probe below beats handing back a point we can see is occupied.
					OVT_SpawnPointComponent spawnComp = OVT_SpawnPointComponent.Cast(closestEntity.FindComponent(OVT_SpawnPointComponent));
					if (spawnComp && spawnComp.HasSpawnPoints())
					{
						vector authored = spawnComp.GetSpawnPoint();
						if (IsPositionClear(authored, mins, maxs))
						{
							foundPos = authored;
							return true;
						}
					}
				}
			}
		}

		// A crude random search close in, then rings outward. Every candidate is put ON the surface
		// under it: the old code lifted the probe by a random 0-2 m from the INPUT's Y and traced ENTS
		// only, so a candidate buried in a hillside or hanging two metres over it both read as clear
		// (terrain is TraceFlags.WORLD, which the box test does not ask for) - that is a spawn in a
		// wall wearing the disguise of a successful probe.
		vector checkpos;
		for (int i = 0; i < 30; i++)
		{
			//Get a random vector in a 2m radius sphere centered on pos, standing on whatever is under it
			checkpos = s_AIRandomGenerator.GenerateRandomPointInRadius(0, 2, pos, false);
			checkpos[1] = ResolveGroundY(Vector(checkpos[0], pos[1], checkpos[2])) + SPAWN_GROUND_CLEARANCE;

			if (IsPositionClear(checkpos, mins, maxs))
			{
				foundPos = checkpos;
				return true;
			}
		}

		// Still nothing: the 2 m sphere is inside the thing we are trying to get out of. Ring outward
		// rather than hand back the colliding input - the same escape the vehicle path already has in
		// FindVehicleSpawnNear, which the character path never got.
		foreach (float radius : SPAWN_RING_RADII)
		{
			for (int bearing = 0; bearing < 8; bearing++)
			{
				float angle = bearing * 45.0 * Math.DEG2RAD;
				checkpos = pos + Vector(Math.Sin(angle) * radius, 0, Math.Cos(angle) * radius);
				checkpos[1] = ResolveGroundY(Vector(checkpos[0], pos[1], checkpos[2])) + SPAWN_GROUND_CLEARANCE;

				if (IsPositionClear(checkpos, mins, maxs))
				{
					foundPos = checkpos;
					return true;
				}
			}
		}

		// Every probe collided - foundPos is still the (colliding) input position
		return false;
	}
	//! Find safe vehicle spawn position with rotation
	//! skipAuthoredSpots skips ONLY the parking/vehicle-point query and keeps the road search: the
	//! 15 m sphere answers "whose parking is nearest", not "whose parking is this", so a destination
	//! that merely sits NEAR somebody's authored parking (a player-placed camp beside a house) would
	//! inherit a spot that belongs to the neighbour. Callers who know the destination authors no
	//! vehicle arrival pass true and go straight to the nearest road.
	static bool FindSafeVehicleSpawnPosition(vector pos, out vector position, out vector angles, bool skipSpawnPointSearch = false, bool skipAuthoredSpots = false)
	{
		// First check for nearby entities with parking or vehicle spawn point components (unless skipped for performance)
		if (!skipSpawnPointSearch && !skipAuthoredSpots)
		{
			if (!s_SpawnPointSearchResults)
				s_SpawnPointSearchResults = {};
			
			s_SpawnPointSearchResults.Clear();
			GetGame().GetWorld().QueryEntitiesBySphere(pos, 15, null, FilterVehicleSpawnEntities, EQueryEntitiesFlags.ALL);
			
			// First priority: Find the closest parking component
			if (s_SpawnPointSearchResults.Count() > 0)
			{
				IEntity closestParkingEntity = null;
				IEntity closestSpawnEntity = null;
				float closestParkingDistance = 999999;
				float closestSpawnDistance = 999999;
				
				foreach (IEntity entity : s_SpawnPointSearchResults)
				{
					float distance = vector.Distance(entity.GetOrigin(), pos);
					
					// Check for parking component first (priority)
					OVT_ParkingComponent parkingComp = OVT_ParkingComponent.Cast(entity.FindComponent(OVT_ParkingComponent));
					if (parkingComp && distance < closestParkingDistance)
					{
						closestParkingDistance = distance;
						closestParkingEntity = entity;
					}
					
					// Also check for spawn point component with vehicle spawn points (fallback)
					OVT_SpawnPointComponent spawnComp = OVT_SpawnPointComponent.Cast(entity.FindComponent(OVT_SpawnPointComponent));
					if (spawnComp && spawnComp.HasVehicleSpawnPoints() && distance < closestSpawnDistance)
					{
						closestSpawnDistance = distance;
						closestSpawnEntity = entity;
					}
				}
				
				// Use parking component if available (higher priority)
				if (closestParkingEntity)
				{
					OVT_ParkingComponent parkingComp = OVT_ParkingComponent.Cast(closestParkingEntity.FindComponent(OVT_ParkingComponent));
					if (parkingComp)
					{
						vector parkingMat[4];
						if (parkingComp.GetParkingSpot(parkingMat, OVT_ParkingType.PARKING_CAR, true)) // Skip obstruction check for fast travel
						{
							position = parkingMat[3];
							angles = Math3D.MatrixToAngles(parkingMat);
							return true;
						}
					}
				}
				
				// Fallback to spawn point component with vehicle spawn points
				if (closestSpawnEntity)
				{
					OVT_SpawnPointComponent spawnComp = OVT_SpawnPointComponent.Cast(closestSpawnEntity.FindComponent(OVT_SpawnPointComponent));
					if (spawnComp && spawnComp.GetVehicleSpawnPoint(position, angles))
					{
						return true;
					}
				}
			}
		}
		
		// Nothing authored here. Put the vehicle on the nearest road, facing the way traffic runs, so it
		// arrives somewhere it can be driven away from (BUG-165). Skipped along with the other spatial
		// queries when the caller asked for the cheap path.
		if (!skipSpawnPointSearch)
		{
			if (FindNearestRoadSpawn(pos, ROAD_SPAWN_MAX_DISTANCE, position, angles))
				return true;

			// No road worth walking back from. Ring out around the destination instead - near it, but
			// deliberately NOT on it.
			if (FindVehicleSpawnNear(pos, position, angles))
				return true;
		}

		// Final fallback. skipSpawnPointSearch is forced TRUE here, and that is the BUG-165 fix rather
		// than a tidy-up: passing the caller's flag through let this land on a CHARACTER spawn point.
		// FindSafeSpawnPosition's spawn-point branch returns the closest OVT_SpawnPointComponent's
		// pedestrian point and never looks at the box it was handed, so a camp - which authors four
		// character points and no vehicle points - parked the car 2 m from the tent centre, on top of it.
		position = FindSafeSpawnPosition(pos, "-1.5 0 -3", "1.5 2.5 3", true);
		angles = "0 0 0";
		return false; // Indicates fallback was used
	}

	//! How far Overthrow will reach for a road to put a fast-travelled vehicle on.
	//!
	//! A vehicle wants to arrive somewhere it can drive away from, but it must still arrive at the place
	//! the player asked for. Past this the road belongs to somewhere else and the trip would end in a long
	//! walk back, which is worse than an awkward park.
	static const float ROAD_SPAWN_MAX_DISTANCE = 200.0;

	//------------------------------------------------------------------------------------------------
	//! The nearest road position to a point, and the direction traffic runs along it.
	//!
	//! Unlike FindNearestRoad, which answers a position only, this also derives the road's HEADING from
	//! the polyline segment nearest the point, so a vehicle can be set down pointing along the road
	//! instead of across it.
	//!
	//! Fails closed on everything: no AI world, no road manager, a negative query result, a road with
	//! fewer than two points, or a degenerate segment all return false and leave the caller on its own
	//! fallback. None of this is play-tested engine API with a vanilla call site to copy, so it is written
	//! to give up rather than to guess.
	//! \param[in] center Position to search from.
	//! \param[in] maxDistance Give up beyond this many metres.
	//! \param[out] position A point ON the road, at ground height.
	//! \param[out] angles Yaw/pitch/roll pointing along the direction of travel.
	//! \return True when a road was found within maxDistance.
	static bool FindNearestRoadSpawn(vector center, float maxDistance, out vector position, out vector angles)
	{
		position = center;
		angles = "0 0 0";

		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return false;

		RoadNetworkManager roadManager = aiWorld.GetRoadNetworkManager();
		if (!roadManager)
			return false;

		BaseRoad foundRoad;
		float distance;
		if (roadManager.GetClosestRoad(center, foundRoad, distance, false) < 0)
			return false;

		if (!foundRoad || distance > maxDistance)
			return false;

		array<vector> points = {};
		foundRoad.GetPoints(points);

		// A direction needs a segment, not a vertex.
		if (points.Count() < 2)
			return false;

		int closestIndex = 0;
		float closestDistance = 999999;
		for (int i = 0; i < points.Count(); i++)
		{
			float pointDistance = vector.Distance(points[i], center);
			if (pointDistance < closestDistance)
			{
				closestDistance = pointDistance;
				closestIndex = i;
			}
		}

		// The segment leaving the closest vertex, or the one arriving at it when that vertex is the end.
		vector segStart = points[closestIndex];
		vector segEnd;
		if (closestIndex + 1 < points.Count())
		{
			segEnd = points[closestIndex + 1];
		}
		else
		{
			segStart = points[closestIndex - 1];
			segEnd = points[closestIndex];
		}

		// Flattened: a vehicle's heading is a compass bearing, and the road's slope is the terrain's job.
		vector direction = segEnd - segStart;
		direction[1] = 0;
		if (direction.Length() < 0.01)
			return false;

		direction.Normalize();

		// Project the destination onto the segment so the vehicle lands at the point of the road NEAREST
		// the place the player asked for, rather than at whichever polyline vertex happened to be closest.
		float segmentLength = vector.Distance(segStart, segEnd);
		float along = vector.Dot(center - segStart, direction);
		if (along < 0)
			along = 0;

		if (along > segmentLength)
			along = segmentLength;

		position = segStart + (direction * along);
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);

		vector roadMat[4];
		Math3D.DirectionAndUpMatrix(direction, vector.Up, roadMat);
		angles = Math3D.MatrixToAngles(roadMat);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A vehicle-sized clear spot in a ring AROUND a position - near it, deliberately not on it.
	//!
	//! The last resort before the crude random search, and the reason it exists: FindSafeSpawnPosition
	//! only ever samples a 2 m sphere centred on the destination, which for a camp or any other placed
	//! structure means "inside the thing you travelled to". This starts outside the footprint and works
	//! outward, testing a vehicle-sized box at each candidate.
	//!
	//! The vehicle faces AWAY from the centre, so it is pointing at open ground rather than at whatever
	//! it just arrived next to.
	//! \param[in] center The position travelled to.
	//! \param[out] position A clear vehicle-sized spot, at ground height.
	//! \param[out] angles Yaw/pitch/roll facing away from centre.
	//! \return True when a clear spot was found.
	static bool FindVehicleSpawnNear(vector center, out vector position, out vector angles)
	{
		position = center;
		angles = "0 0 0";

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		// Start clear of a camp's footprint and widen. Eight bearings is enough to find a gap without
		// turning an arrival into a spatial query storm.
		array<float> radii = {8.0, 12.0, 16.0, 22.0};

		foreach (float radius : radii)
		{
			for (int i = 0; i < 8; i++)
			{
				float angle = i * 45.0 * Math.DEG2RAD;
				vector offset = Vector(Math.Sin(angle) * radius, 0, Math.Cos(angle) * radius);

				vector candidate = center + offset;
				candidate[1] = world.GetSurfaceY(candidate[0], candidate[2]);

				TraceBox trace = new TraceBox;
				trace.Flags = TraceFlags.ENTS;
				trace.Start = candidate;
				trace.Mins = "-1.5 0 -3";
				trace.Maxs = "1.5 2.5 3";

				if (world.TracePosition(trace, null) < 0)
					continue; // collision, try the next bearing

				position = candidate;

				vector facing = offset;
				facing[1] = 0;
				facing.Normalize();

				vector faceMat[4];
				Math3D.DirectionAndUpMatrix(facing, vector.Up, faceMat);
				angles = Math3D.MatrixToAngles(faceMat);

				return true;
			}
		}

		return false;
	}

	//! Filter function to find entities with parking or vehicle spawn point components
	static bool FilterVehicleSpawnEntities(IEntity entity)
	{
		// Check for parking component (priority)
		OVT_ParkingComponent parkingComp = OVT_ParkingComponent.Cast(entity.FindComponent(OVT_ParkingComponent));
		if (parkingComp)
		{
			s_SpawnPointSearchResults.Insert(entity);
			return false;
		}
		
		// Check for spawn point component with vehicle spawn points
		OVT_SpawnPointComponent spawnComp = OVT_SpawnPointComponent.Cast(entity.FindComponent(OVT_SpawnPointComponent));
		if (spawnComp && spawnComp.HasVehicleSpawnPoints())
		{
			s_SpawnPointSearchResults.Insert(entity);
		}
		return false;
	}
	
	//! Filter function to find entities with spawn point components
	static bool FilterSpawnPointEntities(IEntity entity)
	{
		OVT_SpawnPointComponent spawnComp = OVT_SpawnPointComponent.Cast(entity.FindComponent(OVT_SpawnPointComponent));
		if (spawnComp)
		{
			s_SpawnPointSearchResults.Insert(entity);
		}
		return false;
	}
	static IEntity SpawnEntityPrefab(ResourceName prefab, vector origin, vector orientation = "0 0 0", bool global = true)
	{
		EntitySpawnParams spawnParams();

		spawnParams.TransformMode = ETransformMode.WORLD;

		Math3D.AnglesToMatrix(orientation, spawnParams.Transform);
		spawnParams.Transform[3] = origin;

		IEntity entity;
		if (global)
			entity = GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), spawnParams);
		else
			entity = GetGame().SpawnEntityPrefabLocal(Resource.Load(prefab), GetGame().GetWorld(), spawnParams);

		// Waypoints are session-scoped by construction - every one Overthrow spawns is rebuilt with
		// its group on the next boot, so a persistence record for one is a permanent orphan (BUG-118).
		if (AIWaypoint.Cast(entity))
			OVT_PersistenceManagerComponent.UntrackTransient(entity);

		return entity;
	}

	static IEntity SpawnEntityPrefabMatrix(ResourceName prefab, vector mat[4], bool global = true)
	{
		EntitySpawnParams spawnParams();

		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform = mat;

		IEntity entity;
		if (global)
			entity = GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), spawnParams);
		else
			entity = GetGame().SpawnEntityPrefabLocal(Resource.Load(prefab), GetGame().GetWorld(), spawnParams);

		// Same waypoint rule as SpawnEntityPrefab() above (BUG-118).
		if (AIWaypoint.Cast(entity))
			OVT_PersistenceManagerComponent.UntrackTransient(entity);

		return entity;
	}
	//! Clear the aiming state a player leaves on a character after possessing it (BUG-147).
	//! Releasing a possessed AI never resets the player-driven aim, which pins the character's
	//! body yaw forever - the AI's only aim-stop lives in LookAction.bt and never fires for an
	//! aim it did not start, so the recruit walks backwards staring at wherever the player last
	//! looked. Server-side, call after SetPossessedEntity(null) with the ex-possessed entity.
	static void ResetAIAimState(IEntity character)
	{
		if (!character)
			return;

		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.FindComponent(SCR_CharacterControllerComponent));
		if (controller)
		{
			controller.SetWeaponRaised(false);
			controller.SetForcedFreeLook(false);
			controller.ResetPersistentStates(true, true);

			AimingComponent aiming = controller.GetAimingComponent();
			if (aiming)
			{
				aiming.SetAimingRotationWanted(vector.Zero);
				aiming.SetAimingRotation(vector.Zero);
			}
		}

		// Reset the AI-side look bookkeeping too, so the next look request starts clean
		AIControlComponent aiControl = AIControlComponent.Cast(character.FindComponent(AIControlComponent));
		if (aiControl)
		{
			AIAgent agent = aiControl.GetAIAgent();
			if (agent)
			{
				SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
				if (utility && utility.m_LookAction)
				{
					utility.m_LookAction.Cancel();
					utility.m_LookAction.Complete();
				}
			}
		}
	}

	//! Spawn a character entity directly without creating a group
	static SCR_ChimeraCharacter SpawnCharacterEntity(ResourceName prefab, vector origin, vector orientation = "0 0 0")
	{
		EntitySpawnParams spawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		
		Math3D.AnglesToMatrix(orientation, spawnParams.Transform);
		spawnParams.Transform[3] = origin;
		
		// Load the prefab resource
		Resource resource = Resource.Load(prefab);
		if (!resource)
		{
			Print("[Overthrow] Error: Could not load prefab resource: " + prefab);
			return null;
		}
		
		// Spawn the entity directly
		IEntity spawnedEntity = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
		if (!spawnedEntity)
		{
			Print("[Overthrow] Error: Failed to spawn entity from prefab: " + prefab);
			return null;
		}
		
		// Cast to character and return
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(spawnedEntity);
		if (!character)
		{
			Print("[Overthrow] Error: Spawned entity is not a SCR_ChimeraCharacter: " + spawnedEntity);
			// Clean up the spawned entity since it's not what we expected
			SCR_EntityHelper.DeleteEntityAndChildren(spawnedEntity);
			return null;
		}
		
		return character;
	}
	
	static bool IsOceanAtPosition(vector checkpos)
	{		
		World world = GetGame().GetWorld();
		return 1 > world.GetSurfaceY(checkpos[0],checkpos[2]);
		return world.GetOceanBaseHeight() > world.GetSurfaceY(checkpos[0],checkpos[2]);
	}
	
	static vector GetRandomNonOceanPositionNear(vector pos, float range)
	{
		int i = 0;
		vector checkpos;
		BaseWorld world = GetGame().GetWorld();
		vector worldMin, worldMax;
		world.GetBoundBox(worldMin, worldMax);

		while(i < 30)
		{
			i++;

			checkpos = s_AIRandomGenerator.GenerateRandomPointInRadius(0,range,pos,false);

			// Keep the point on the map, or AI given a waypoint here can never reach it
			if(checkpos[0] < worldMin[0]) checkpos[0] = worldMin[0];
			if(checkpos[0] > worldMax[0]) checkpos[0] = worldMax[0];
			if(checkpos[2] < worldMin[2]) checkpos[2] = worldMin[2];
			if(checkpos[2] > worldMax[2]) checkpos[2] = worldMax[2];

			if(!OVT_WorldUtils.IsOceanAtPosition(checkpos))
			{
				checkpos[1] = world.GetSurfaceY(checkpos[0],checkpos[2]) + 1;
				return checkpos;
			}
		}

		return pos;
	}
	
	static bool GetNearbyBodiesAndWeapons(vector pos, int range, out array<IEntity> entities)
	{
		m_Bodies = new array<IEntity>;
		GetGame().GetWorld().QueryEntitiesBySphere(pos, range, FilterDeadBodiesAndWeapons);
		entities.InsertAll(m_Bodies);
		
		return true;
	}
	
	protected static ref array<IEntity> m_Bodies;
	
	protected static bool FilterDeadBodiesAndWeapons(IEntity ent)
	{		
		DamageManagerComponent dmg = OVT_ComponentFinder<DamageManagerComponent>.Find(ent);
		if(dmg && dmg.IsDestroyed())
		{
			m_Bodies.Insert(ent);
			return true;
		}
		
		WeaponComponent weapon = OVT_ComponentFinder<WeaponComponent>.Find(ent);
		if(weapon) m_Bodies.Insert(ent);
				
		return true;
	}
	//------------------------------------------------------------------------------------------------
	//! Find the nearest road position to a given location
	//! @param center Starting position to search from
	//! @param searchRadius Maximum distance to search for roads
	//! @return Position on nearest road, or original position if no road found
	static vector FindNearestRoad(vector center)
	{
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return center;
			
		RoadNetworkManager roadManager = aiWorld.GetRoadNetworkManager();
		if (!roadManager)
			return center;
			
		BaseRoad foundRoad;
		float distance;
		int result = roadManager.GetClosestRoad(center, foundRoad, distance, false);
				
		if (result >= 0 && foundRoad && foundRoad.GetWidth() > 0)
		{
			// Try to get a reachable waypoint on the road
			vector roadPos;
			if (roadManager.GetReachableWaypointInRoad(center, center, 500, roadPos))
				return roadPos;
		}
		
		// If no road found within range, return original position
		return center;
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes an entity AND the prefab-authored hierarchy children it was spawned with.
	//!
	//! ⚠ SCR_EntityHelper.DeleteEntityAndChildren IS A MISNOMER. Its whole body is
	//! RplComponent.DeleteRplEntity(entity, false) (SCR_EntityHelper.c:177), which takes the ROOT out of
	//! replication and out of the world and leaves prefab-authored hierarchy children standing. On a
	//! vehicle those children are the SupplyStorage_NN slots every vanilla car and van carries, so a
	//! despawned ambient car left its supply crates hanging in the air where it had been parked.
	//!
	//! Direct children only, each deleted through its own subtree - collecting the whole tree and
	//! deleting deepest-first would hand back handles an ancestor's delete already freed.
	//! \param[in] root The entity to remove. Null is a no-op.
	//------------------------------------------------------------------------------------------------
	//! An entity's position in WORLD space, whatever it is attached to.
	//!
	//! 🔴 GetOrigin() IS PARENT SPACE, AND A MOUNTED CHARACTER IS PARENTED TO ITS VEHICLE. So
	//! `player.GetOrigin()` on somebody sitting in a truck is a vehicle-local coordinate a few metres
	//! from the vehicle's own origin - not a place on the map. Every distance test against it is then
	//! wrong by however far that vehicle is from the map origin, and wrong in the PERMISSIVE direction:
	//! the answer comes out enormous, so a proximity gate reads "nobody is near".
	//!
	//! Found twice in one day (2026-08-24/25): OVT_PlayerWantedComponent could not see a player in a
	//! vehicle, and OVT_BaseConditionDeploymentModule.GetPlayerProximity - the gate whose whole job is
	//! "do not spawn a force in front of a player" - could not either.
	//! \param[in] entity The entity to locate. Null answers vector.Zero.
	//! \return A world-space position.
	static vector GetWorldOrigin(IEntity entity)
	{
		if (!entity)
			return vector.Zero;

		IEntity root = entity;
		int guard = 0;
		while (root.GetParent() && guard < 4)
		{
			root = root.GetParent();
			guard++;
		}

		return root.GetOrigin();
	}

	static void DeleteEntityTree(IEntity root)
	{
		if (!root)
			return;

		DeleteSubtree(root, 0);
	}

	//! Depth guard. A prefab hierarchy is a handful of levels; anything deeper is a cycle, and a cycle
	//! here would recurse until the stack gives out.
	protected static const int MAX_TREE_DEPTH = 16;

	//------------------------------------------------------------------------------------------------
	//! One node and everything under it, DEEPEST FIRST.
	//!
	//! ⚠ IT USED TO BE ONE LEVEL DEEP DESPITE THE NAME (fixed 2026-08-24, author: an enemy FOB was
	//! pulled down and its props stayed standing). The old body walked the root's DIRECT children and
	//! handed each to SCR_EntityHelper.DeleteEntityAndChildren - which is a misnomer whose whole body
	//! is RplComponent.DeleteRplEntity(entity, false), so it removes the entity it is given and leaves
	//! that entity's own children in the world. Anything two levels down survived.
	//!
	//! ⚠ CHILDREN ARE RE-READ AT EACH LEVEL rather than the whole tree being collected up front. A
	//! pre-collected list would hand back handles an ancestor's delete had already freed.
	//! \param[in] node The subtree root.
	//! \param[in] depth Recursion depth, against MAX_TREE_DEPTH.
	protected static void DeleteSubtree(IEntity node, int depth)
	{
		if (!node || depth > MAX_TREE_DEPTH)
			return;

		array<IEntity> children = new array<IEntity>();

		IEntity child = node.GetChildren();
		while (child)
		{
			children.Insert(child);
			child = child.GetSibling();
		}

		foreach (IEntity c : children)
		{
			if (!c)
				continue;

			// Never a player, a recruit or a civilian riding in the thing being deleted. Asked at every
			// level now, not just the first.
			if (ChimeraCharacter.Cast(c))
				continue;

			DeleteSubtree(c, depth + 1);
		}

		// A replicated entity must leave through replication; a plain prop has no RplComponent at all,
		// and DeleteRplEntity does nothing for it.
		if (RplComponent.Cast(node.FindComponent(RplComponent)))
		{
			SCR_EntityHelper.DeleteEntityAndChildren(node);
			return;
		}

		delete node;
	}
}
