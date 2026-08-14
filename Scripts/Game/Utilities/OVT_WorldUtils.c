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
	static vector FindSafeSpawnPosition(vector pos, vector mins = "-0.5 0 -0.5", vector maxs = "0.5 2 0.5", bool skipSpawnPointSearch = false)
	{
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
					OVT_SpawnPointComponent spawnComp = OVT_SpawnPointComponent.Cast(closestEntity.FindComponent(OVT_SpawnPointComponent));
					if (spawnComp)
					{
						return spawnComp.GetSpawnPoint();
					}
				}
			}
		}
		
		//a crude and brute-force way to find a spawn position, try to improve this later
		vector foundpos = pos;
		int i = 0;
		
		BaseWorld world = GetGame().GetWorld();
		float ground = world.GetSurfaceY(pos[0],pos[2]);
		
		vector checkpos;
		TraceBox trace;
		while(i < 30)
		{
			i++;
			
			//Get a random vector in a 2m radius sphere centered on pos and above the ground
			checkpos = s_AIRandomGenerator.GenerateRandomPointInRadius(0,2,pos,false);
			checkpos[1] = pos[1] + s_AIRandomGenerator.RandFloatXY(0, 2);
						
			//check if a box on that position collides with anything
			trace = new TraceBox;
			trace.Flags = TraceFlags.ENTS;
			trace.Start = checkpos;
			trace.Mins = mins;
			trace.Maxs = maxs;
			
			float result = world.TracePosition(trace, null);
				
			if (result < 0)
			{
				//collision, try again
				continue;
			}else{
				//no collision, this pos is safe
				foundpos = checkpos;
				break;
			}
		}
		
		return foundpos;
	}
	//! Find safe vehicle spawn position with rotation
	static bool FindSafeVehicleSpawnPosition(vector pos, out vector position, out vector angles, bool skipSpawnPointSearch = false)
	{
		// First check for nearby entities with parking or vehicle spawn point components (unless skipped for performance)
		if (!skipSpawnPointSearch)
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
}
