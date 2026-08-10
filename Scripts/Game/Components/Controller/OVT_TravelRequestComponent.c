[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative fast travel and bus travel for one player")]
class OVT_TravelRequestComponentClass : OVT_ComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative travel, on the per-player OVT_OverthrowController entity.
//!
//! Replaces OVT_FastTravelService.ExecuteFastTravel, which teleported on the CALLING machine and
//! debited money on the CLIENT (findings F1/F2), and the legacy
//! OVT_PlayerCommsComponent.RpcAsk_RequestFastTravel, which was ResolveSenderPlayerId + TeleportPlayer
//! with no eligibility check, no distance check and no payment - any client could teleport itself
//! anywhere, for free (finding N2). Project rule (overthrow-controller.md): no new client->server RPCs
//! go on OVT_PlayerCommsComponent.
//!
//! Extends plain OVT_Component rather than OVT_BaseServerProgressComponent (implementation plan §5
//! Phase 2): a trip completes inside one frame, so a progress dialog would only flash, and the result
//! carries MONEY, which the progress base's (transferred, skipped) pair cannot express.
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
class OVT_TravelRequestComponent : OVT_Component
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
	//! fare. The server takes its own recruit list; this flag only says whether to take one at all.
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

		// 5 - ONE list, for both the fare and the teleport
		array<IEntity> recruits = ResolveTravellingRecruits(playerId, originPos, bringRecruits);
		int recruitCount = recruits.Count();

		// 6
		int result = OVT_FastTravelService.ValidateTravel(verb, targetPos, playerId, actor, recruitCount);
		if(result != OVT_TravelResult.OK)
		{
			SendTravelResult(playerId, result, 0);
			return;
		}

		// 7
		vector dest = ResolveDestination(actor, targetPos);

		// 8 - while the actor is still standing at originPos. Measured to targetPos, not to dest, so
		// the charged fare is the one the panel displayed rather than one nudged by the safe-spawn
		// search.
		int cost = OVT_FastTravelService.CalculateTravelCost(verb, targetPos, actor, recruitCount);

		// 9
		if(!SCR_Global.TeleportPlayer(playerId, dest))
		{
			SendTravelResult(playerId, OVT_TravelResult.TELEPORT_FAILED, 0);
			return;
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
	//! \param[in] playerId Runtime id of the travelling player.
	//! \param[in] originPos The player's position BEFORE the teleport.
	//! \param[in] bringRecruits The player's opt-out choice.
	//! \return The recruit entities within OVT_FastTravelService.RECRUIT_TRAVEL_RADIUS, or an empty
	//! array when the player opted out or the recruit manager is unavailable.
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
					array<IEntity> nearby = recruitManager.GetPlayerRecruitEntitiesInRadius(persId, originPos, OVT_FastTravelService.RECRUIT_TRAVEL_RADIUS);
					if(nearby) return nearby;
				}
			}
		}

		return new array<IEntity>;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the player actually arrives.
	//!
	//! A driver takes the vehicle along - vanilla's TeleportPlayer teleports the VEHICLE when the
	//! player is in one (Functions.c:1657-1663) - so a driver needs a vehicle-sized clear spot. The
	//! returned angles are deliberately discarded, exactly as the legacy path discarded them
	//! (OVT_FastTravelService.ExecuteFastTravel pre-Phase 2): TeleportPlayer sets position only.
	//!
	//! FindSafeVehicleSpawnPosition's bool return reports only whether a parking spot was found; the
	//! out position is filled either way (OVT_Global.c:450-453), which is why it is not checked.
	//!
	//! Passengers never reach here - ValidateTravel already refused them with MUST_BE_DRIVER, and a bus
	//! passenger with MUST_EXIT_VEHICLE - but the pilot test is re-derived rather than inferred from
	//! that, so this stays correct if the rule order ever changes.
	//! \param[in] actor The travelling player's controlled entity.
	//! \param[in] targetPos The requested destination.
	//! \return A safe arrival position.
	protected vector ResolveDestination(IEntity actor, vector targetPos)
	{
		if(IsVehicleDriver(actor))
		{
			vector vehiclePos;
			vector vehicleAngles;
			OVT_Global.FindSafeVehicleSpawnPosition(targetPos, vehiclePos, vehicleAngles);
			return vehiclePos;
		}

		return OVT_Global.FindSafeSpawnPosition(targetPos);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the actor is in a vehicle's pilot seat. Same three steps, same null guards, as
	//! OVT_FastTravelService.ValidateTravel's in-vehicle rule.
	//! \param[in] actor The travelling player's controlled entity.
	//! \return True only for a driver; false on foot and false for a passenger.
	protected bool IsVehicleDriver(IEntity actor)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(actor);
		if(!character || !character.IsInVehicle()) return false;

		CompartmentAccessComponent compartmentAccess = character.GetCompartmentAccessComponent();
		if(!compartmentAccess) return false;

		BaseCompartmentSlot slot = compartmentAccess.GetCompartment();
		if(!slot) return false;

		return slot.GetType() == ECompartmentType.PILOT;
	}

	//------------------------------------------------------------------------------------------------
	//! Places the travelling recruits in a ring around the arrival point.
	//!
	//! Lifted verbatim from OVT_PlayerCommsComponent.RpcAsk_RequestFastTravelWithRecruits (:1529-1547),
	//! which is the only implementation of this that has ever run: angle i x 360/count, radius 3 m
	//! growing 0.5 m per recruit, and FindSafeSpawnPosition with skipSpawnPointSearch = true (the
	//! spawn-point query is a sphere query per recruit, and the ring is already spread out on purpose).
	//! SetOrigin rather than TeleportPlayer because recruits are AI, not players.
	//! \param[in] recruits The entities from step 5. Not re-queried - that is the point.
	//! \param[in] destPos The player's arrival point.
	protected void TeleportRecruits(array<IEntity> recruits, vector destPos)
	{
		if(!recruits || recruits.IsEmpty()) return;

		int recruitIndex = 0;
		foreach (IEntity recruitEntity : recruits)
		{
			if (!recruitEntity)
				continue;

			// Calculate offset position in a circle around the player destination
			float angle = (recruitIndex * 360.0 / recruits.Count()) * Math.DEG2RAD;
			float radius = 3.0 + (recruitIndex * 0.5); // Start at 3m and expand outward
			vector offset = Vector(Math.Sin(angle) * radius, 0, Math.Cos(angle) * radius);
			vector recruitPos = destPos + offset;

			// Find a safe position near the calculated spot (skip spawn point search for performance with multiple recruits)
			recruitPos = OVT_Global.FindSafeSpawnPosition(recruitPos, "-0.5 0 -0.5", "0.5 2 0.5", true);

			// Teleport the recruit
			recruitEntity.SetOrigin(recruitPos);
			recruitIndex++;
		}
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
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return false;

		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if(persId == "") return false;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return false;

		economy.TakePlayerMoneyPersistentId(persId, cost);
		return true;
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

	//------------------------------------------------------------------------------------------------
	//! Which player this controller belongs to, resolved on the SERVER from the controller entity
	//! this component sits on.
	//!
	//! This is the whole anti-spoofing story: a remote client can only reach this handler through the
	//! controller entity it owns, so the identity comes from the entity, never from the payload (the
	//! legacy comms component solves the same problem by living on the player's character). The scan
	//! is over connected players only, which is single digits.
	//!
	//! Copied from OVT_ShopTransactionComponent (:657-678) and OVT_TowerSabotageComponent - now three
	//! controller components deep, and a candidate for a shared base class.
	//! \return Runtime player id, or -1.
	protected int ResolveOwningPlayerId()
	{
		OVT_OverthrowController owner = OVT_OverthrowController.Cast(GetOwner());
		if(!owner) return -1;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return -1;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if(!playerManager) return -1;

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);

		foreach(int playerId : playerIds)
		{
			OVT_OverthrowController controller = players.GetController(playerId);
			if(controller == owner) return playerId;
		}

		return -1;
	}
}
