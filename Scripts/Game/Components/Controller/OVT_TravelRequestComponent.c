[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative fast travel and bus travel for one player")]
class OVT_TravelRequestComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative travel, on the per-player OVT_OverthrowController entity.
//!
//! Replaces OVT_FastTravelService.ExecuteFastTravel, which teleported on the CALLING machine and
//! debited money on the CLIENT (findings F1/F2), and the legacy comms monolith's
//! RpcAsk_RequestFastTravel, which was ResolveSenderPlayerId + TeleportPlayer
//! with no eligibility check, no distance check and no payment - any client could teleport itself
//! anywhere, for free (finding N2). Project rule (overthrow-controller.md): every client->server RPC
//! lives on a controller component like this one.
//!
//! Extends OVT_ControllerRequestComponent (the shared caller-identity base) rather than
//! OVT_BaseServerProgressComponent (implementation plan §5 Phase 2): a trip completes inside one
//! frame, so a progress dialog would only flash, and the result carries MONEY, which the progress
//! base's (transferred, skipped) pair cannot express.
//!
//! ONE RPC pair serves both travel verbs (K10). Rpc()'s prototype is untyped variadic, so a wrong
//! argument count compiles clean and dies silently at the wire (BUG-090); the practical defence is to
//! have as few signatures as possible and read each one by hand.
//!
//! The rules and the fare live in OVT_FastTravelService and are the SAME code the client runs to
//! decide whether the travel button is enabled and what price to show. The client check is advisory;
//! this one is authoritative. Nothing on this server path may call anything in that service's
//! CLIENT-ONLY section - those functions resolve the actor from the MACHINE, which on a listen server
//! is the host's character and would validate the host against another player's request.
//------------------------------------------------------------------------------------------------
class OVT_TravelRequestComponent : OVT_ControllerRequestComponent
{
	//! Fired on the requesting client only. Args: (int OVT_TravelResult, int amountCharged).
	//! Display only - see RpcDo_TravelResult. Money is never mutated from an invoker.
	ref ScriptInvoker m_OnTravelResult = new ScriptInvoker();

	//------------------------------------------------------------------------------------------------
	//! Ask the server to move the local player.
	//!
	//! The client names a verb, a destination and whether its recruits are coming. It does NOT name
	//! itself: identity is resolved server-side from the controller entity this component sits on, so
	//! there is no player id in the payload to spoof (S-4).
	//! \param[in] verb An OVT_TravelVerb value. Anything else is refused by the server.
	//! \param[in] targetPos Destination in world space. Advisory - the server re-derives the actual
	//! arrival point and re-validates the trip against its own view of the world.
	//! \param[in] bringRecruits True to bring the player's nearby recruits, which also multiplies the
	//! fare. The server takes its own recruit list; this flag only says whether to take one at all - and
	//! it is ignored entirely when the player is driving, because the vehicle carries its occupants for
	//! free and gathering anyone would break them (BUG-163).
	void RequestTravel(int verb, vector targetPos, bool bringRecruits)
	{
		if(Replication.IsServer())
		{
			RpcAsk_Travel(verb, targetPos, bringRecruits);
		}else{
			Rpc(RpcAsk_Travel, verb, targetPos, bringRecruits);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: validate, move, charge, and report - in that order.
	//!
	//! The step order is the whole point of this component (K8/R10) and matches implementation plan
	//! §4.3 exactly:
	//!  1. we are the server
	//!  1b. the request shape is sane - an unknown verb is refused outright, because ValidateTravel
	//!      would otherwise skip BOTH the fast-travel rules (distance/wanted/QRF) and the bus rules
	//!  2. the requesting player is resolved from THIS controller entity, never from the payload
	//!  3. that player has a controlled entity
	//!  4. the origin is captured BEFORE anything moves - TeleportPlayer moves the entity
	//!     synchronously and the fare is origin-dependent (comms :1514-1516)
	//!  5. the recruit entity list is taken ONCE and used for BOTH the fare and the teleport, so it is
	//!     structurally impossible to pay solo and arrive with a squad (S-5)
	//!  6. the shared rule set decides; a refusal reports and stops, having charged nothing
	//!  6b. the DESTINATION is matched against the server's own enumeration of places this player may
	//!      travel to, and targetPos is replaced by the server's vector for it. Step 6 decides whether
	//!      the player may travel; this decides whether the place they named exists
	//!  7. the real arrival point is derived server-side (vehicle spawn for a driver, safe spawn on foot)
	//!  8. the fare is computed while the actor is still at the origin
	//!  9. the teleport runs and its RETURN VALUE is checked - it fails on an out-of-bounds destination
	//!     or an unresolvable player (Functions.c:1640-1645)
	//! 10. money is taken only AFTER a successful teleport, so a refused trip cannot charge (N4/Q-1)
	//! 11. the recruits from step 5 are placed in a ring around the arrival point
	//! 12. the outcome, including the amount actually charged, goes back to the requesting client
	//!
	//! A crash between 9 and 10 loses the fare in the PLAYER's favour, which is the fail-safe
	//! direction. There is deliberately no refund path.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Travel(int verb, vector targetPos, bool bringRecruits)
	{
		// 1
		if(!Replication.IsServer()) return;

		// R2/BUG-090 diagnostic: this line firing proves the request left the client, reached this
		// machine and was decoded with the right arity. If travel does nothing and this never prints,
		// the component is missing from the controller prefab or the Rpc() arity is wrong; if it
		// prints and nothing happens, a rule refused - see the matching result print below.
		Print("[Overthrow] Travel request received: verb=" + verb.ToString() + " recruits=" + bringRecruits.ToString(), LogLevel.NORMAL);

		// 1b - request shape. An out-of-range verb is not a harmless default: ValidateTravel would
		// match neither the FAST_TRAVEL branch nor the BUS branch and would wave the trip through with
		// no minimum distance, no wanted check and no QRF check.
		if(verb != OVT_TravelVerb.FAST_TRAVEL && verb != OVT_TravelVerb.BUS)
		{
			Print("[Overthrow] Travel request refused: unknown verb " + verb.ToString(), LogLevel.WARNING);
			return;
		}

		// 2
		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0)
		{
			Print("[Overthrow] Travel request refused: could not resolve the requesting player", LogLevel.WARNING);
			return;
		}

		// 3
		IEntity actor = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!actor)
		{
			SendTravelResult(playerId, OVT_TravelResult.NO_ACTOR, 0);
			return;
		}

		// 4 - captured before anything moves
		vector originPos = actor.GetOrigin();

		// 5 - ONE list, for both the fare and the teleport.
		//
		// A driver's trip moves the VEHICLE (vanilla TeleportPlayer, Functions.c:1649-1656), which brings
		// everyone sitting in it at no cost and with nothing of ours involved - so it gathers no recruits
		// at all (BUG-163). Gathering them meant the ring placement below called SetOrigin on recruits who
		// were still occupying a compartment, which threw them off the map, and charged a per-recruit fare
		// for seats they were already in. The client's panel suppresses the toggle through the same
		// predicate, so the fare it displays is the fare charged here.
		bool gatherRecruits = bringRecruits && !OVT_FastTravelService.VehicleCarriesOccupants(verb, actor);

		array<IEntity> recruits = ResolveTravellingRecruits(playerId, originPos, gatherRecruits);
		int recruitCount = recruits.Count();

		// 6
		int result = OVT_FastTravelService.ValidateTravel(verb, targetPos, playerId, actor, recruitCount);
		if(result != OVT_TravelResult.OK)
		{
			SendTravelResult(playerId, result, 0);
			return;
		}

		// 6b - THE DESTINATION ITSELF. Step 6 asks whether this player may make a trip; it does not ask
		// whether the place they named is one Overthrow offers. That rule - owned house, your camp, a
		// FOB, a base we hold - lived only in the per-type CanFastTravel overrides, which are CLIENT
		// code and which OVT_FastTravelService labels advisory, so before this step a crafted request
		// could name any coordinate on the map and be teleported to it for the fare.
		//
		// ! targetPos IS REASSIGNED, and that is the point rather than a shortcut. Everything below -
		// the fare, the arrival point, the recruit ring - reads targetPos, so overwriting it with the
		// SERVER's own recorded vector makes it structurally impossible for a later edit to price or
		// place anything from the client's number. Same property respawn gets by never letting the
		// client's vector past CompleteRespawn.
		//
		// FAST_TRAVEL only: the BUS verb's destination rule is already inside ValidateTravel
		// (IsAtBusStop refuses anything not within BUS_STOP_RADIUS of a real marker), so a bus request
		// cannot name an arbitrary coordinate either and re-resolving it here would buy nothing.
		bool destIsCamp = false;
		if(verb == OVT_TravelVerb.FAST_TRAVEL)
		{
			vector authorisedPos;
			int destResult = OVT_FastTravelService.ResolveFastTravelDestination(ResolvePersistentId(playerId), targetPos, authorisedPos);
			if(destResult != OVT_TravelResult.OK)
			{
				Print("[Overthrow] Travel request refused: player " + playerId.ToString() + " named a destination the server does not offer", LogLevel.WARNING);
				SendTravelResult(playerId, destResult, 0);
				return;
			}

			targetPos = authorisedPos;

			// Camps author character spawn points and no vehicle arrival at all, but they are
			// player-placed and can sit within the authored-spot query's 15 m of somebody else's
			// parking - which would park the travelled vehicle in the neighbour's spot. A camp
			// arrival therefore skips the authored query and takes the nearest road.
			destIsCamp = OVT_FastTravelService.IsCampPosition(targetPos);
		}

		// 7
		vector destAngles;
		bool destOriented;
		vector dest = ResolveDestination(actor, targetPos, destAngles, destOriented, destIsCamp);

		// 8 - while the actor is still standing at originPos. Measured to targetPos, not to dest, so
		// the charged fare is the one the panel displayed rather than one nudged by the safe-spawn
		// search. After 6b targetPos is the server's own vector for the named location, which is within
		// OVT_RespawnService.MATCH_TOLERANCE of the one the panel priced - far below one fare unit.
		int cost = OVT_FastTravelService.CalculateTravelCost(verb, targetPos, actor, recruitCount);

		// 9 - THE TELEPORT.
		//
		// On a dedicated server, player characters are CLIENT-AUTHORITATIVE for movement: the client runs
		// its own physics locally and the server corrects drift. Calling TeleportPlayer server-side on an
		// on-foot character sets the server's position but the client's local physics immediately overrides
		// it, so the player never moves. Vanilla's own editor teleport (SCR_PlayersManagerEditorComponent.
		// RPC_TeleportPlayerToPositionServer) handles this exact split: on-foot → Owner RPC to the client;
		// in-vehicle → server call (vehicles are server-authoritative). Overthrow must do the same.
		//
		// For vehicles, TeleportPlayer is still the right server-side call (it moves the vehicle entity,
		// which is authoritative, and all clients see the change). For characters, TeleportCharacter sends
		// the resolved position to the owning client, which applies it from its own physics context.
		if(OVT_FastTravelService.IsVehicleDriver(actor))
		{
			if(!SCR_Global.TeleportPlayer(playerId, dest))
			{
				SendTravelResult(playerId, OVT_TravelResult.TELEPORT_FAILED, 0);
				return;
			}

			// 9b - TeleportPlayer sets POSITION only: it copies the vehicle's existing world transform and
			// overwrites the translation (Functions.c:1658-1661), so a car that arrives on a road arrives
			// pointing whichever way it was pointing when the player opened the map. Turning it to face the
			// way the arrival spot expects has to be a second, separate step (BUG-165).
			if(destOriented)
				OrientVehicle(actor, destAngles);
		}
		else
		{
			TeleportCharacter(playerId, dest);
		}

		// 10 - only now, and only what was actually taken is reported. The debit is deliberately NOT
		// folded into the condition of an && : a side effect inside a boolean expression is the kind of
		// line that gets "simplified" later into one that charges before the teleport is checked.
		int charged = 0;
		if(cost > 0)
		{
			if(ChargeFare(playerId, cost))
				charged = cost;
		}

		// 11
		TeleportRecruits(recruits, dest);

		// 12
		SendTravelResult(playerId, OVT_TravelResult.OK, charged);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the outcome of a travel request, for display only.
	//!
	//! This RPC must never mutate money or move anything. The authoritative money arrives through the
	//! economy manager's player-money broadcast and the position through the engine's own replication,
	//! so a lost packet here costs a hint, not a transaction.
	//!
	//! Server authority means a click can be refused AFTER the map has closed, so every non-OK result
	//! produces a specific on-screen reason rather than silence (Q-2/R11). EVERY outcome hints, including
	//! a successful FREE trip (debug mode, or a bus fare that rounds to zero): silence is
	//! indistinguishable from a request that never left the client, which is precisely the failure this
	//! component's diagnostics exist to tell apart.
	//! \param[in] result An OVT_TravelResult value.
	//! \param[in] amountCharged Money actually taken, server-computed. 0 on any refusal, and 0 in
	//! debug mode where travel is free.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_TravelResult(int result, int amountCharged)
	{
		if(result != OVT_TravelResult.OK)
		{
			OVT_Global.ShowHint(OVT_FastTravelService.ReasonKeyFor(result));
		}else if(amountCharged > 0)
		{
			// The server's recruit count can differ from the one the panel priced (a recruit wandered
			// out of the 50 m radius between the click and the RPC), so the amount ACTUALLY taken is
			// reported rather than assumed - drift becomes visible instead of silent (§4.5/R4).
			//
			// Resolved here rather than concatenated onto the "#OVT-Travelled" key: ShowCustom takes a
			// plain string and SCR_HintUIInfo.SetDescriptionTo ends at TextWidget.SetText, so nothing
			// downstream substitutes anything. Same two-placeholder format the travel button uses.
			OVT_Global.ShowHint(WidgetManager.Translate(OVT_FastTravelService.FARE_LABEL_FORMAT,
				WidgetManager.Translate("#OVT-Travelled"), amountCharged.ToString()));
		}else{
			// Free, and successful. No fare to name, so the bare key - which is why "#OVT-Travelled" is
			// deliberately NOT parameterised and the price lives in FARE_LABEL_FORMAT instead.
			OVT_Global.ShowHint("#OVT-Travelled");
		}

		if(m_OnTravelResult)
			m_OnTravelResult.Invoke(result, amountCharged);
	}

	//------------------------------------------------------------------------------------------------
	//! The recruits that are travelling, resolved SERVER-side. Never null.
	//!
	//! This one list is handed to both the fare (as a count) and the teleport (as entities), which is
	//! what makes "pay solo, arrive with a squad" impossible by construction rather than by keeping two
	//! paths in sync (K4/S-5). No client-supplied count reaches the fare - bringRecruits only says
	//! whether to look at all.
	//!
	//! INACTIVE RECRUITS ARE EXCLUDED, AND THEREFORE NOT CHARGED FOR. A recruit is made inactive
	//! precisely so that it stays where it is and holds that spot; taking a parked garrison along
	//! because the player happened to fast-travel from beside it would undo the order they gave it.
	//! Because the same list feeds the fare, a player standing in their own garrison is not billed for
	//! passengers that are not coming.
	//! \param[in] playerId Runtime id of the travelling player.
	//! \param[in] originPos The player's position BEFORE the teleport.
	//! \param[in] bringRecruits The player's opt-out choice.
	//! \return The ACTIVE recruit entities within OVT_FastTravelService.RECRUIT_TRAVEL_RADIUS, or an
	//! empty array when the player opted out or the recruit manager is unavailable.
	protected array<IEntity> ResolveTravellingRecruits(int playerId, vector originPos, bool bringRecruits)
	{
		if(bringRecruits)
		{
			OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
			OVT_RecruitManagerComponent recruitManager = OVT_Global.GetRecruits();

			if(players && recruitManager)
			{
				string persId = players.GetPersistentIDFromPlayerID(playerId);
				if(persId != "")
				{
					array<IEntity> nearby = recruitManager.GetPlayerRecruitEntitiesInRadius(persId, originPos, OVT_FastTravelService.RECRUIT_TRAVEL_RADIUS, true);
					if(nearby) return nearby;
				}
			}
		}

		return new array<IEntity>;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the player actually arrives, and which way they should be pointing when they get there.
	//!
	//! A driver takes the vehicle along - vanilla's TeleportPlayer teleports the VEHICLE when the
	//! player is in one (Functions.c:1657-1663) - so a driver needs a vehicle-sized clear spot.
	//!
	//! THE ANGLES ARE NO LONGER DISCARDED (BUG-165). They used to be, on the reasoning that
	//! TeleportPlayer sets position only - which is true, and is exactly why throwing them away left a
	//! car that fast-travelled onto a road sitting across it. FindSafeVehicleSpawnPosition's bool return
	//! now means \"the arrival spot has an opinion about facing\": a parking spot, an authored vehicle
	//! point, a road, or a ring position all say true, and only the crude random fallback says false.
	//!
	//! Passengers never reach here - ValidateTravel already refused them with MUST_BE_DRIVER, and a bus
	//! passenger with MUST_EXIT_VEHICLE - but the pilot test is re-derived rather than inferred from
	//! that, so this stays correct if the rule order ever changes.
	//! \param[in] actor The travelling player's controlled entity.
	//! \param[in] targetPos The requested destination.
	//! \param[out] angles Yaw/pitch/roll the vehicle should end up at. Meaningless unless oriented is true.
	//! \param[out] oriented True when the arrival spot named a facing worth applying.
	//! \param[in] destIsCamp True when the destination is a camp: the vehicle skips the authored
	//! parking/vehicle-point query (which would grab a neighbour's spot) and takes the nearest road.
	//! \return A safe arrival position.
	protected vector ResolveDestination(IEntity actor, vector targetPos, out vector angles, out bool oriented, bool destIsCamp = false)
	{
		angles = "0 0 0";
		oriented = false;

		if(OVT_FastTravelService.IsVehicleDriver(actor))
		{
			vector vehiclePos;
			vector vehicleAngles;
			oriented = OVT_WorldUtils.FindSafeVehicleSpawnPosition(targetPos, vehiclePos, vehicleAngles, false, destIsCamp);
			angles = vehicleAngles;
			return vehiclePos;
		}

		// On foot. The safe-search has two ways to hand back a position inside something at a built-up
		// destination: total failure returns the input unchanged (a deployed FOB's record vector IS the
		// truck's origin - the player materialised inside the truck bed), and the authored-spawn-point
		// branch returns its point UNTRACED, so a point an obstruction has since covered comes back
		// looking clear. Verify whichever answer arrives, and fall back to the nearest road - the same
		// answer the recruits and a travelled vehicle already use. Only when there is no road within
		// reach does the original position stand, as the least-bad last resort.
		vector dest;
		bool clear = OVT_WorldUtils.TryFindSafeSpawnPosition(targetPos, dest);
		if(clear && !IsPositionClear(dest))
		{
			// A blocked AUTHORED point usually has clear floor right beside it (something was built or
			// parked over it since it was authored) - a scatter around the point keeps the arrival in
			// the same room rather than on the street.
			vector scattered;
			clear = OVT_WorldUtils.TryFindSafeSpawnPosition(dest, scattered, "-0.5 0 -0.5", "0.5 2 0.5", true);
			if(clear)
				dest = scattered;
		}

		if(!clear)
		{
			vector roadPos;
			vector roadAngles;
			if(OVT_WorldUtils.FindNearestRoadSpawn(targetPos, ROAD_FALLBACK_MAX_DISTANCE, roadPos, roadAngles))
				dest = roadPos;
		}

		return dest;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a person-sized box at pos is free of geometry - the same test the spawn search's own
	//! probes apply, for positions that arrive from branches that never traced them.
	protected bool IsPositionClear(vector pos)
	{
		BaseWorld world = GetGame().GetWorld();
		if(!world)
			return true;

		TraceBox trace = new TraceBox;
		trace.Flags = TraceFlags.ENTS;
		trace.Start = pos;
		trace.Mins = "-0.5 0 -0.5";
		trace.Maxs = "0.5 2 0.5";

		return world.TracePosition(trace, null) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Turn the travelled vehicle to the facing its arrival spot asked for. SERVER-side, after the move.
	//!
	//! Same tail as vanilla's TeleportPlayer (Functions.c:1658-1670): build the transform, prefer
	//! BaseGameEntity.Teleport over SetWorldTransform so the engine's own teleport bookkeeping runs, and
	//! keep the position the teleport already established - only the rotation is being changed here.
	//!
	//! Silently does nothing when the actor turns out not to be in a vehicle after all, which is the right
	//! answer rather than an error: the trip has already happened and succeeded.
	//! \param[in] actor The travelling player's controlled entity.
	//! \param[in] angles Yaw/pitch/roll to face.
	protected void OrientVehicle(IEntity actor, vector angles)
	{
		SCR_CompartmentAccessComponent compartmentAccess = SCR_CompartmentAccessComponent.Cast(actor.FindComponent(SCR_CompartmentAccessComponent));
		if(!compartmentAccess) return;

		IEntity vehicle = compartmentAccess.GetVehicle();
		if(!vehicle) return;

		vector transform[4];
		Math3D.AnglesToMatrix(angles, transform);
		transform[3] = vehicle.GetOrigin();

		BaseGameEntity baseGameEntity = BaseGameEntity.Cast(vehicle);
		if(baseGameEntity)
		{
			baseGameEntity.Teleport(transform);
		}else{
			vehicle.SetWorldTransform(transform);
		}
	}

	//! How far to reach for a road when an on-foot arrival point cannot be cleared - both for the
	//! recruit line-up and for the player's own arrival fallback. Tighter than the vehicle's
	//! ROAD_SPAWN_MAX_DISTANCE (200 m): a car on a road 200 m away can be driven back, a person 200 m
	//! away has to walk. Past this, the local fallbacks run - and anywhere WITHOUT a road within
	//! 100 m is open ground, where the local search rarely fails in the first place.
	static const float ROAD_FALLBACK_MAX_DISTANCE = 100.0;

	//! Gap between recruits lined up along the road.
	static const float RECRUIT_ROAD_SPACING = 1.5;

	//------------------------------------------------------------------------------------------------
	//! Places the travelling recruits on the nearest road to the arrival point.
	//!
	//! ON THE ROAD, NOT IN A RING - the ring the legacy comms RPC used (3 m + 0.5 m per recruit around
	//! the player) put recruits inside walls and furniture whenever the player's own arrival point was
	//! an authored INDOOR spawn point: a house interior has no clear spot 3 m out, the 2 m-sphere
	//! random search then fails on every probe, and FindSafeSpawnPosition handed back the colliding
	//! ring position as if it were an answer. A road is open by construction, and there is one near
	//! almost every destination - so the squad waits on the street while the player arrives inside.
	//!
	//! The ring survives only as the no-road fallback (wilderness camps and FOBs, where the ground is
	//! open and the ring was never the problem), and a ring position that cannot be cleared now falls
	//! back to the player's own arrival point - overlapping entities shove apart on open floor;
	//! a wall does not.
	//!
	//! SetOrigin rather than TeleportPlayer because recruits are AI, not players.
	//! \param[in] recruits The entities from step 5. Not re-queried - that is the point.
	//! \param[in] destPos The player's arrival point.
	protected void TeleportRecruits(array<IEntity> recruits, vector destPos)
	{
		if(!recruits || recruits.IsEmpty()) return;

		// One road query for the whole squad, and the direction it runs so the line follows it.
		vector roadPos, roadAngles;
		bool haveRoad = OVT_WorldUtils.FindNearestRoadSpawn(destPos, ROAD_FALLBACK_MAX_DISTANCE, roadPos, roadAngles);
		vector roadDir = vector.Zero;
		if (haveRoad)
		{
			vector roadMat[4];
			Math3D.AnglesToMatrix(roadAngles, roadMat);
			roadDir = roadMat[2];
		}

		BaseWorld world = GetGame().GetWorld();

		int recruitIndex = 0;
		foreach (IEntity recruitEntity : recruits)
		{
			if (!recruitEntity)
				continue;

			// NEVER SetOrigin a recruit who is sitting in a vehicle (BUG-163). A compartment occupant is
			// attached to the vehicle, so writing a world position onto it does not move it to that world
			// position - it throws the recruit somewhere unrecoverable while the compartment still believes
			// it is seated, which is what made them vanish and put their "Show on Map" marker in the ocean.
			// A driver's own trip no longer reaches here at all (step 5 gathers nobody), so this only fires
			// for a recruit parked in some OTHER vehicle during an on-foot trip: they keep their seat and are
			// left behind, which is recoverable, rather than being deleted in place, which is not.
			if (IsInCompartment(recruitEntity))
				continue;

			vector recruitPos;
			vector clearPos;
			if (haveRoad)
			{
				// Alternate sides of the road point: 0, +1.5, -1.5, +3, -3 ... metres along the road.
				int stepIndex = (recruitIndex + 1) / 2;
				float step = stepIndex * RECRUIT_ROAD_SPACING;
				if (recruitIndex % 2 == 0)
					step = -step;

				recruitPos = roadPos + (roadDir * step);
				recruitPos[1] = world.GetSurfaceY(recruitPos[0], recruitPos[2]);

				// Declutter (a parked car, a fence). On failure keep the unadjusted road point: it is
				// on a road at ground height, which is never inside a building - unlike the ring, the
				// raw position is an acceptable answer here.
				if (OVT_WorldUtils.TryFindSafeSpawnPosition(recruitPos, clearPos, "-0.5 0 -0.5", "0.5 2 0.5", true))
					recruitPos = clearPos;
			}
			else
			{
				// No road near - open ground. The legacy ring: angle i x 360/count, radius 3 m growing
				// 0.5 m per recruit.
				float angle = (recruitIndex * 360.0 / recruits.Count()) * Math.DEG2RAD;
				float radius = 3.0 + (recruitIndex * 0.5);
				vector offset = Vector(Math.Sin(angle) * radius, 0, Math.Cos(angle) * radius);
				recruitPos = destPos + offset;

				// A ring position that cannot be cleared is a KNOWN collision - fall back to the
				// player's own arrival point rather than embed the recruit where the probe failed.
				if (OVT_WorldUtils.TryFindSafeSpawnPosition(recruitPos, clearPos, "-0.5 0 -0.5", "0.5 2 0.5", true))
					recruitPos = clearPos;
				else
					recruitPos = destPos;
			}

			// Teleport the recruit
			recruitEntity.SetOrigin(recruitPos);
			recruitIndex++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Whether an entity is currently occupying a vehicle compartment - any seat, not just the pilot's.
	//!
	//! Distinct from OVT_FastTravelService.IsVehicleDriver, which asks whether the TRAVELLING PLAYER takes
	//! their vehicle along. This asks whether an entity may be moved by writing its origin, and any seat
	//! at all is disqualifying.
	//! \param[in] entity The entity to test.
	//! \return True when the entity is seated in a vehicle.
	protected bool IsInCompartment(IEntity entity)
	{
		CompartmentAccessComponent compartmentAccess = CompartmentAccessComponent.Cast(entity.FindComponent(CompartmentAccessComponent));
		if(!compartmentAccess) return false;

		return compartmentAccess.IsInCompartment();
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the fare, on the SERVER.
	//!
	//! TakePlayerMoneyPersistentId mutates the authoritative record and streams the new balance to the
	//! owning client (OVT_EconomyManagerComponent.c:1168-1181), which is why calling it from a client -
	//! as ExecuteFastTravel used to (F2) - looked correct locally and was then overwritten by the
	//! server's next authoritative value.
	//!
	//! Reports whether the debit happened so the client is told the amount actually taken and never a
	//! figure that only the log believes. ValidateTravel has already proven the managers resolve and
	//! the player can afford this, so a false here means the world changed underneath the request.
	//! \param[in] playerId Runtime id of the paying player.
	//! \param[in] cost The fare. Always > 0 at the call site.
	//! \return True when the money was taken.
	protected bool ChargeFare(int playerId, int cost)
	{
		string persId = ResolvePersistentId(playerId);
		if(persId == "") return false;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return false;

		economy.TakePlayerMoneyPersistentId(persId, cost);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One player's persistent id, on the SERVER, from a runtime id this component already resolved
	//! from its own controller entity.
	//!
	//! Never from the machine: OVT_Global.GetLocalPersistentId() answers the HOST's id on a listen
	//! server, which would price and authorise one player's trip against another player's property.
	//! \param[in] playerId Runtime id of the acting player.
	//! \return The persistent id, or "" when it cannot be resolved.
	protected string ResolvePersistentId(int playerId)
	{
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return "";

		return players.GetPersistentIDFromPlayerID(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Sends the resolved arrival position to the travelling player so they can apply it locally.
	//!
	//! Player characters are CLIENT-AUTHORITATIVE for movement on dedicated servers: the server cannot
	//! write a new world position onto a character and expect the client to accept it, because the
	//! client's own physics loop will override the server's position on the very next frame. The
	//! solution — and the one vanilla uses in SCR_PlayersManagerEditorComponent.
	//! RPC_TeleportPlayerToPositionServer — is to send an Owner-targeted RPC so the client calls
	//! TeleportPlayer from WITHIN its own physics context, where the position change is authoritative.
	//! The same listen-server guard pattern used by SendTravelResult prevents the
	//! "authority-sends-owner-RPC-to-itself → dropped packet" failure.
	//! \param[in] playerId Runtime id of the travelling player.
	//! \param[in] position The server's resolved arrival position.
	protected void TeleportCharacter(int playerId, vector position)
	{
		if(playerId > 0 && playerId == SCR_PlayerController.GetLocalPlayerId())
		{
			RpcDo_TeleportCharacter(position);
			return;
		}

		Rpc(RpcDo_TeleportCharacter, position);
	}

	//------------------------------------------------------------------------------------------------
	//! Owner: apply the server-authorised arrival position to the local character.
	//!
	//! Runs on the travelling player's machine only. The destination was chosen and validated by the
	//! server — this handler cannot change it. It intentionally does nothing on failure (out-of-bounds
	//! position or no controlled entity): the fare has already been charged and the server has already
	//! moved the recruits, so there is no clean rollback; silence is the same outcome TeleportPlayer
	//! produces when it returns false on the server path.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_TeleportCharacter(vector position)
	{
		SCR_Global.TeleportPlayer(SCR_PlayerController.GetLocalPlayerId(), position);
	}

	//------------------------------------------------------------------------------------------------
	//! Delivers the outcome to the requesting client.
	//!
	//! On a listen server the requester IS this machine's local player, and an Owner-targeted RPC to
	//! ourselves is at best redundant - so the handler is called directly and nothing is sent. On a
	//! dedicated server GetLocalPlayerId() is 0 and this always goes over the wire. Copied from
	//! OVT_ShopTransactionComponent.SendSellResult (:629-638), which exists because of this project's
	//! known "listen-server host never receives its own owner-targeted RPCs" bug class.
	//! \param[in] playerId Runtime id of the requesting player.
	//! \param[in] result An OVT_TravelResult value.
	//! \param[in] amountCharged Money actually taken.
	protected void SendTravelResult(int playerId, int result, int amountCharged)
	{
		// R2/BUG-090 diagnostic: pairs with the arrival print above, so a play-test can tell a refusal
		// (this prints a non-zero result) from a request that never arrived (neither prints).
		Print("[Overthrow] Travel result " + result.ToString() + " charged " + amountCharged.ToString() + " for player " + playerId.ToString(), LogLevel.NORMAL);

		if(playerId > 0 && playerId == SCR_PlayerController.GetLocalPlayerId())
		{
			RpcDo_TravelResult(result, amountCharged);
			return;
		}

		Rpc(RpcDo_TravelResult, result, amountCharged);
	}

}
