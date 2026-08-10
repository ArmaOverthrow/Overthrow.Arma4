//------------------------------------------------------------------------------------------------
//! Which travel verb a request is for. Sent as an int over the wire (RPC payloads stay ints and
//! RplIds), exactly as OVT_ShopSellResult is.
//------------------------------------------------------------------------------------------------
enum OVT_TravelVerb
{
	FAST_TRAVEL,	//!< Fast travel from any travel-capable map location.
	BUS				//!< Bus travel, bus stop to bus stop.
}

//------------------------------------------------------------------------------------------------
//! Outcome of a travel request. One vocabulary for both machines: the client uses it to label a
//! disabled button, the server uses it to refuse and to tell the requesting client why.
//! Sent as an int over the wire.
//------------------------------------------------------------------------------------------------
enum OVT_TravelResult
{
	OK,					//!< The request is allowed.
	NO_ACTOR,			//!< The acting player has no controlled entity (dead, unspawned, unresolvable).
	TOO_CLOSE,			//!< Destination is nearer than m_Difficulty.minFastTravelDistance.
	WANTED,				//!< The player's wanted level is above zero.
	QRF_ACTIVE,			//!< A QRF is running and QRFFastTravelMode is DISABLED.
	QRF_TOO_CLOSE,		//!< A QRF is running and the destination is inside QRF_RANGE of it.
	NOT_ALLOWED,		//!< Refused by a rule with no more specific code (partial state, missing manager).
	CANNOT_AFFORD,		//!< The full fare, recruits included, exceeds the player's money.
	MUST_BE_DRIVER,		//!< In a vehicle as a passenger; only the driver may take the vehicle along.
	MUST_EXIT_VEHICLE,	//!< In a vehicle at all - buses do not carry vehicles.
	NOT_AT_BUS_STOP,	//!< The player is not standing at a bus stop (BUS origin check).
	BAD_DESTINATION,	//!< The named destination is not a valid target for this verb.
	TELEPORT_FAILED		//!< The engine refused the teleport (out of bounds, unresolvable player).
}

//------------------------------------------------------------------------------------------------
//! Centralized travel rules and fare model for Overthrow. Extracted from OVT_MapContext.
//!
//! THE ONE RULE SET, run by BOTH machines. The client calls it to decide whether the travel button
//! is enabled and what fare to show; the server calls the same code to decide whether anything
//! actually happens. Two copies would drift, and "displayed availability matches enforced
//! availability" is only achievable while there is one.
//!
//! THE INVARIANT THAT MAKES THAT POSSIBLE: no function reachable from the server may resolve the
//! acting player from the MACHINE - SCR_PlayerController.GetLocalControlledEntity() is null on a
//! dedicated server and is the HOST's character on a listen server, which would validate the host's
//! wanted level and position against another player's request. The actor is therefore always a
//! parameter. The only machine-scoped lookup in this file lives in GetLocalActor(), under the
//! CLIENT-ONLY section at the bottom, and nothing above that section calls it.
//------------------------------------------------------------------------------------------------
class OVT_FastTravelService
{
	//! Radius around the player within which recruits travel with them. Carried over unchanged from the
	//! legacy OVT_MapContext fast-travel branch (removed in map/legacy-retirement).
	static const float RECRUIT_TRAVEL_RADIUS = 50.0;

	//! How near a bus stop counts as "at" it. The number Overthrow has always used for that question,
	//! carried over from the legacy OVT_MapContext bus-travel branch (removed in map/legacy-retirement).
	//! Server-side radius must never be smaller than the client's, or the server refuses a trip the
	//! panel offered.
	static const float BUS_STOP_RADIUS = 15.0;

	//------------------------------------------------------------------------------------------------
	// THE AUTHORITATIVE RULE SET
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Decides whether one player may make one trip. Runs identically on client and server.
	//!
	//! Fast travel applies the minimum distance, wanted and QRF rules; buses apply none of them
	//! (legacy bus travel never did, and adding them would be a new travel mechanic). Both verbs end
	//! on affordability at the FULL fare, recruits included.
	//! \param[in] verb OVT_TravelVerb.FAST_TRAVEL or .BUS
	//! \param[in] targetPos Destination in world space.
	//! \param[in] playerId Runtime player id of the ACTING player - used for money, never for entity resolution.
	//! \param[in] actor The acting player's controlled entity, supplied by the caller. Never resolved here.
	//! \param[in] recruitCount How many recruits are travelling too, for the fare only.
	//! \return An OVT_TravelResult code. OVT_TravelResult.OK means allowed.
	static int ValidateTravel(int verb, vector targetPos, int playerId, IEntity actor, int recruitCount)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return OVT_TravelResult.NOT_ALLOWED;

		if (config.m_bDebugMode)
			return OVT_TravelResult.OK;

		if (!actor)
			return OVT_TravelResult.NO_ACTOR;

		ChimeraCharacter character = ChimeraCharacter.Cast(actor);
		vector originPos = actor.GetOrigin();

		if (verb == OVT_TravelVerb.FAST_TRAVEL)
		{
			// Minimum distance
			float dist = vector.Distance(targetPos, originPos);
			if (dist < config.m_Difficulty.minFastTravelDistance)
				return OVT_TravelResult.TOO_CLOSE;

			// Wanted level
			if (character)
			{
				OVT_PlayerWantedComponent wantedComp = OVT_PlayerWantedComponent.Cast(character.FindComponent(OVT_PlayerWantedComponent));
				if (wantedComp && wantedComp.GetWantedLevel() > 0)
					return OVT_TravelResult.WANTED;
			}

			// QRF restrictions
			OVT_OccupyingFactionManager occupyingFaction = OVT_Global.GetOccupyingFaction();
			if (occupyingFaction && occupyingFaction.m_bQRFActive &&
				config.m_Difficulty.QRFFastTravelMode != OVT_QRFFastTravelMode.FREE)
			{
				if (config.m_Difficulty.QRFFastTravelMode == OVT_QRFFastTravelMode.DISABLED)
					return OVT_TravelResult.QRF_ACTIVE;

				float qrfDist = vector.Distance(occupyingFaction.m_vQRFLocation, targetPos);
				if (qrfDist < OVT_QRFControllerComponent.QRF_RANGE)
					return OVT_TravelResult.QRF_TOO_CLOSE;
			}
		}

		// Vehicles. Fast travel takes the vehicle along, but only for the driver (vanilla's
		// TeleportPlayer moves the vehicle when the player is in one). A bus does not carry vehicles.
		if (character && character.IsInVehicle())
		{
			if (verb == OVT_TravelVerb.BUS)
				return OVT_TravelResult.MUST_EXIT_VEHICLE;

			CompartmentAccessComponent compartmentAccess = character.GetCompartmentAccessComponent();
			if (!compartmentAccess)
				return OVT_TravelResult.MUST_BE_DRIVER;

			BaseCompartmentSlot slot = compartmentAccess.GetCompartment();
			if (!slot || slot.GetType() != ECompartmentType.PILOT)
				return OVT_TravelResult.MUST_BE_DRIVER;
		}

		if (verb == OVT_TravelVerb.BUS)
		{
			// The origin is re-derived from the actor's own position - the client is not trusted about
			// where it is standing - and the destination must itself be a stop.
			if (!IsAtBusStop(originPos))
				return OVT_TravelResult.NOT_AT_BUS_STOP;

			if (!IsAtBusStop(targetPos))
				return OVT_TravelResult.BAD_DESTINATION;
		}

		// Affordability, at the full fare
		int cost = CalculateTravelCost(verb, targetPos, actor, recruitCount);
		if (cost > 0)
		{
			OVT_PlayerManagerComponent playerManager = OVT_Global.GetPlayers();
			if (!playerManager)
				return OVT_TravelResult.NOT_ALLOWED;

			string persId = playerManager.GetPersistentIDFromPlayerID(playerId);
			if (persId == "")
				return OVT_TravelResult.NOT_ALLOWED;

			OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
			if (!economy || !economy.PlayerHasMoney(persId, cost))
				return OVT_TravelResult.CANNOT_AFFORD;
		}

		return OVT_TravelResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! The localized refusal text for a result code, so the panel and the server's after-the-fact
	//! hint speak one vocabulary.
	//! \param[in] result An OVT_TravelResult code.
	//! \return A localization key, or "" for OVT_TravelResult.OK.
	static string ReasonKeyFor(int result)
	{
		switch (result)
		{
			case OVT_TravelResult.OK:					return "";
			case OVT_TravelResult.TOO_CLOSE:			return "#OVT-CannotFastTravelDistance";
			case OVT_TravelResult.WANTED:				return "#OVT-CannotFastTravelWanted";
			case OVT_TravelResult.QRF_ACTIVE:			return "#OVT-CannotFastTravelDuringQRF";
			case OVT_TravelResult.QRF_TOO_CLOSE:		return "#OVT-CannotFastTravelToQRF";
			case OVT_TravelResult.CANNOT_AFFORD:		return "#OVT-CannotAfford";
			case OVT_TravelResult.MUST_BE_DRIVER:		return "#OVT-MustBeDriver";
			case OVT_TravelResult.MUST_EXIT_VEHICLE:	return "#OVT-MustExitVehicle";
			case OVT_TravelResult.NOT_AT_BUS_STOP:		return "#OVT-NotAtBusStop";
		}

		// NO_ACTOR, NOT_ALLOWED, BAD_DESTINATION and TELEPORT_FAILED all mean "not there, not now" to
		// a player; the generic key is the one that has always been shown for them.
		return "#OVT-CannotFastTravelThere";
	}

	//! Localization key for "<something> ($<fare>)" - a two-placeholder format, NOT a concatenation.
	//!
	//! Shared by the map panel's travel button ("Fast Travel ($300)") and the post-trip hint
	//! ("Travelled ($300)") so the two read the same way and a translator moves the price once.
	//! Resolve it with WidgetManager.Translate(FARE_LABEL_FORMAT, label, fare) - the sinks on both sides
	//! (SCR_InputButtonComponent.SetLabel, SCR_HintManagerComponent.ShowCustom) take a finished string and
	//! do no substitution of their own.
	static const string FARE_LABEL_FORMAT = "#OVT-TravelWithFare";

	//------------------------------------------------------------------------------------------------
	// FARES
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The fare arithmetic, and nothing else. No world access at all, which is what lets it be
	//! asserted in the world-free test tier.
	//!
	//! Fast travel applies a 1 km floor so a short hop still costs something; buses do not, which is
	//! the one structural difference between the two fares. Rounding happens BEFORE the recruit
	//! multiplier, so N recruits cost exactly N extra solo fares and never a rounding drift.
	//! \param[in] distMeters Distance from the player to the destination, in metres. Negative is clamped to 0.
	//! \param[in] unitPrice Price per kilometre (m_Difficulty.fastTravelCost or .busTicketPrice).
	//! \param[in] applyKmFloor True to charge at least one kilometre.
	//! \param[in] recruitCount Recruits travelling with the player. Each pays a full fare.
	//! \return The total fare.
	static int ComputeFare(float distMeters, int unitPrice, bool applyKmFloor, int recruitCount)
	{
		if (unitPrice <= 0)
			return 0;

		if (distMeters < 0)
			distMeters = 0;

		if (recruitCount < 0)
			recruitCount = 0;

		float distKm = distMeters / 1000;
		if (applyKmFloor)
			distKm = Math.Max(1.0, distKm);

		int fare = Math.Round(distKm * unitPrice);

		return fare * (1 + recruitCount);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a verb's fare is charged at a minimum of one kilometre.
	//!
	//! THE SINGLE PLACE THIS MAPPING LIVES. Fast travel has the floor so a short hop still costs
	//! something; a bus is billed strictly pro rata and a 0 m bus ride is free. Swapping the two used to
	//! be a silent change - ComputeFare takes the floor as a bare bool parameter, so passing the wrong
	//! one made buses cost a kilometre minimum and fast travel free over short hops while every fare
	//! assertion stayed green, because they all exercised ComputeFare directly and never the verb.
	//! Pinned world-free by OVT_TEST_Logic_TravelFares.
	//! \param[in] verb OVT_TravelVerb.FAST_TRAVEL or .BUS
	//! \return True when the fare is floored at one kilometre.
	static bool VerbUsesKmFloor(int verb)
	{
		if (verb == OVT_TravelVerb.BUS)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The fare for one trip, measured from the actor's current position. Free in debug mode.
	//! \param[in] verb OVT_TravelVerb.FAST_TRAVEL or .BUS
	//! \param[in] targetPos Destination in world space.
	//! \param[in] actor The acting player's controlled entity, supplied by the caller.
	//! \param[in] recruitCount Recruits travelling with the player.
	//! \return The total fare, or 0 when it cannot be computed.
	static int CalculateTravelCost(int verb, vector targetPos, IEntity actor, int recruitCount)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config || config.m_bDebugMode)
			return 0;

		if (!actor)
			return 0;

		float dist = vector.Distance(targetPos, actor.GetOrigin());

		int unitPrice = config.m_Difficulty.fastTravelCost;
		if (verb == OVT_TravelVerb.BUS)
			unitPrice = config.m_Difficulty.busTicketPrice;

		return ComputeFare(dist, unitPrice, VerbUsesKmFloor(verb), recruitCount);
	}

	//------------------------------------------------------------------------------------------------
	// SHARED LOOKUPS - safe on both machines
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! How many of a player's recruits are close enough to travel with them.
	//! \param[in] persId Persistent id of the owning player.
	//! \param[in] originPos Position to measure from - the player's position BEFORE the teleport.
	//! \return The recruit count, or 0 when the recruit manager is unavailable.
	static int CountRecruitsInRadius(string persId, vector originPos)
	{
		OVT_RecruitManagerComponent recruitManager = OVT_Global.GetRecruits();
		if (!recruitManager)
			return 0;

		array<ref OVT_RecruitData> nearby = recruitManager.GetPlayerRecruitsInRadius(persId, originPos, RECRUIT_TRAVEL_RADIUS);
		if (!nearby)
			return 0;

		return nearby.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a position counts as being at a bus stop.
	//!
	//! Guarded rather than assumed: the marker registry is a world scan on the game mode, so it is
	//! null before the game mode exists and in a world with no markers. A null registry refuses,
	//! because "we cannot tell" must never charge somebody for a bus that is not there.
	//! \param[in] pos World position to test.
	//! \return True when a BUS_STOP marker is within BUS_STOP_RADIUS.
	static bool IsAtBusStop(vector pos)
	{
		OVT_MapMarkerManagerComponent markers = OVT_Global.GetMapMarkers();
		if (!markers)
			return false;

		return markers.GetNearestMarker(pos, OVT_MapMarkerCategory.BUS_STOP, BUS_STOP_RADIUS) != null;
	}

	//------------------------------------------------------------------------------------------------
	// CLIENT-ONLY WRAPPERS
	//
	// Everything below this line resolves the acting player from the MACHINE and is therefore valid
	// on a client and nowhere else. Server code calls ValidateTravel / CalculateTravelCost directly
	// with an actor it resolved itself.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The local player's controlled entity. CLIENT-ONLY - the single machine-scoped lookup in this
	//! service, deliberately kept in one place so the server-side rule set provably cannot reach it.
	//!
	//! Used in preference to PlayerManager.GetPlayerControlledEntity(localId), which this file used
	//! until Phase 1 of map/fast-travel: on a dedicated-server client that call returns null, so the
	//! displayed fare came out as $0 and the client-side affordability check silently passed. The
	//! local-controlled-entity form is correct on dedicated AND listen clients alike.
	//!
	//! Public since Phase 3 so the map UI can price CalculateTravelCost(verb, pos, actor, recruitCount)
	//! with a recruit count - the alternative was a second local-entity lookup in the UI, which is the
	//! duplication this section exists to prevent. It is still CLIENT-ONLY: never call it from anything
	//! the server can reach.
	//! \return The local controlled entity, or null.
	static IEntity GetLocalActor()
	{
		return SCR_PlayerController.GetLocalControlledEntity();
	}

	//------------------------------------------------------------------------------------------------
	//! Global travel checks for the LOCAL player. CLIENT-ONLY.
	//!
	//! Signature deliberately unchanged: the per-type CanFastTravel overrides in
	//! Scripts/Game/UI/Map/LocationTypes/ all call it. It is a thin wrapper that resolves the local
	//! actor and delegates - SERVER CALLERS MUST USE ValidateTravel and pass their own actor, or they
	//! will validate the wrong player's wanted level and position on a listen server.
	//!
	//! Advisory only: it drives button state. The server's ValidateTravel is what decides.
	//! \param[in] targetPos Destination in world space.
	//! \param[in] playerID Persistent id of the local player.
	//! \param[out] reason Localization key explaining a refusal.
	//! \return True when the local player may travel there.
	static bool CanGlobalFastTravel(vector targetPos, string playerID, out string reason)
	{
		reason = "#OVT-CannotFastTravelThere";

		OVT_PlayerManagerComponent playerManager = OVT_Global.GetPlayers();
		if (!playerManager)
			return false;

		int intPlayerID = playerManager.GetPlayerIDFromPersistentID(playerID);
		if (intPlayerID < 0)
			return false;

		// Recruit accompaniment is not wired to the fare yet (map/fast-travel Phase 3 adds the
		// player's opt-out toggle), so the advisory fare here is the solo fare, as it has always been.
		int result = ValidateTravel(OVT_TravelVerb.FAST_TRAVEL, targetPos, intPlayerID, GetLocalActor(), 0);
		if (result == OVT_TravelResult.OK)
			return true;

		reason = ReasonKeyFor(result);
		return false;
	}

	// DELETED in map/fast-travel Phase 3: CalculateFastTravelCost(targetPos, playerID). Its playerID
	// parameter was unused and it could not express a recruit count, so the map UI showed the SOLO fare
	// while the server charged for the recruits it found. The UI now calls
	// CalculateTravelCost(verb, targetPos, GetLocalActor(), effectiveRecruitCount) directly.

	// DELETED in map/fast-travel Phase 2: ExecuteFastTravel. It teleported on the calling machine and
	// debited money on the client (findings F1/F2). Execution, payment and the recruit ring now live in
	// OVT_TravelRequestComponent on OVT_OverthrowController, reached through
	// OVT_Global.GetTravelRequests().RequestTravel(verb, targetPos, bringRecruits). Nothing in this
	// service teleports or moves money any more; it decides and it prices, on both machines.
}
