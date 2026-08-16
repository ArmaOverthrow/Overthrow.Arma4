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
				// A deployed FOB stays travellable through EVERY QRF restriction, DISABLED included -
				// it is the resistance's forward spawn for exactly this battle, and refusing it
				// strands players who forgot to set it as home. allowFOBDuringQRF (default on, JIP-
				// streamed) lets a server owner switch the exemption off. Runs identically on both
				// machines (m_FOBs is replicated), so the panel's enable state and the server's
				// refusal cannot disagree.
				bool fobExempt = config.m_Difficulty.allowFOBDuringQRF && IsFobPosition(targetPos);
				if (!fobExempt)
				{
					if (config.m_Difficulty.QRFFastTravelMode == OVT_QRFFastTravelMode.DISABLED)
						return OVT_TravelResult.QRF_ACTIVE;

					float qrfDist = vector.Distance(occupyingFaction.m_vQRFLocation, targetPos);
					if (qrfDist < OVT_QRFControllerComponent.QRF_RANGE)
						return OVT_TravelResult.QRF_TOO_CLOSE;
				}
			}
		}

		// Vehicles. Fast travel takes the vehicle along, but only for the driver (vanilla's
		// TeleportPlayer moves the vehicle when the player is in one). A bus does not carry vehicles.
		if (character && character.IsInVehicle())
		{
			if (verb == OVT_TravelVerb.BUS)
				return OVT_TravelResult.MUST_EXIT_VEHICLE;

			if (!IsVehicleDriver(actor))
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
	// SERVER DESTINATION AUTHORITY
	//
	// WHY THIS IS NOT IN ValidateTravel. The rule set above runs on BOTH machines, and the client runs
	// it per map element on every zoom change (OVT_MapLocationType.CanFastTravel is documented as a hot
	// path). A manager walk in there would be O(locations x locations) per zoom. This half runs ONCE
	// per accepted request, on the server only, and is reached from OVT_TravelRequestComponent - the
	// same shape as respawn, where OVT_RespawnRequestComponent resolves the destination rather than
	// OVT_RespawnService deciding it inside a shared predicate.
	//
	// WHAT IT CLOSES. Before this existed the server checked distance, wanted level, QRF, vehicle seat
	// and affordability, but nothing about WHAT WAS AT targetPos: the "owned house / your camp / your
	// FOB / a controlled base" rule lived only in the per-type CanFastTravel overrides, which are
	// client-side and which the service itself labels advisory. A crafted RequestTravel could therefore
	// teleport a player to any coordinate at all, for the fare.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether any candidate names the same place as target. PURE - no world, no manager, so the
	//! matching rule itself is assertable in the world-free Logic tier while the enumeration below is
	//! not.
	//!
	//! Uses OVT_RespawnService.PositionsMatch, and must keep using it: the two verbs resolving a
	//! client-named place by different tolerances is exactly the drift this shares one enumeration to
	//! avoid.
	//! \param[in] candidates The server's own recorded positions.
	//! \param[in] target The vector the client sent.
	//! \param[out] matched The CANDIDATE that matched - the server's vector, never the client's.
	//! \return True when one of the candidates names the same place.
	static bool MatchesAnyPosition(notnull array<vector> candidates, vector target, out vector matched)
	{
		matched = vector.Zero;

		foreach (vector candidate : candidates)
		{
			if (!OVT_RespawnService.PositionsMatch(candidate, target))
				continue;

			matched = candidate;
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Turns a client's named fast-travel destination into a position the server is willing to move
	//! somebody to. SERVER-SIDE, once per request.
	//!
	//! ! THE RETURNED VECTOR ALWAYS COMES FROM THE SERVER'S ENUMERATION, NEVER FROM targetPos, exactly
	//! as OVT_RespawnService.ResolveRespawnPosition does. targetPos is a LOOKUP KEY: the client names a
	//! place, it does not supply a coordinate, so a crafted vector matches nothing and is refused
	//! rather than being nudged into range by the safe-spawn search downstream.
	//!
	//! DEBUG MODE IS FREE MOVEMENT, deliberately. ValidateTravel already returns OK unconditionally
	//! under m_bDebugMode, so refusing a destination here would leave debug mode able to pay nothing
	//! and go nowhere - a worse tool than the one it replaced. The bypass is stated in both places
	//! rather than inferred from the other.
	//!
	//! BUS IS NOT HANDLED HERE. Its destination rule is already server-authoritative and already in
	//! ValidateTravel: IsAtBusStop(targetPos) refuses anything that is not within BUS_STOP_RADIUS of a
	//! real BUS_STOP marker, so a bus request cannot name an arbitrary coordinate either.
	//! \param[in] persId Persistent id of the acting player. Always a parameter, never resolved from the machine.
	//! \param[in] targetPos The vector the client sent. A key, not a position.
	//! \param[out] resolvedPos The SERVER's own vector for the matched location, or targetPos in debug mode.
	//! \return OVT_TravelResult.OK on a match, NOT_ALLOWED on an unresolvable id, BAD_DESTINATION otherwise.
	static int ResolveFastTravelDestination(string persId, vector targetPos, out vector resolvedPos)
	{
		resolvedPos = targetPos;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (config && config.m_bDebugMode)
			return OVT_TravelResult.OK;

		if (persId.IsEmpty())
			return OVT_TravelResult.NOT_ALLOWED;

		// excludeActiveQrf false: fast travel has its own QRF rules and ValidateTravel has already
		// applied them. Letting the collector's respawn-shaped QRF filter run here would report a
		// destination inside a permitted QRF as one the server does not recognise at all.
		array<vector> eligible = new array<vector>();
		OVT_RespawnService.CollectEligiblePositions(persId, eligible, false);

		vector matched;
		if (!MatchesAnyPosition(eligible, targetPos, matched))
			return OVT_TravelResult.BAD_DESTINATION;

		resolvedPos = matched;
		return OVT_TravelResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	// SHARED LOOKUPS - safe on both machines
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether the actor is sitting in a vehicle's pilot seat.
	//!
	//! Load-bearing beyond the MUST_BE_DRIVER refusal: it is also the test for "the vehicle is coming
	//! along", because vanilla's TeleportPlayer swaps the character for its vehicle before applying the
	//! transform (Functions.c:1649-1656) and moves the whole thing with BaseGameEntity.Teleport.
	//! \param[in] actor The acting player's controlled entity, supplied by the caller.
	//! \return True only for a driver; false on foot and false for a passenger.
	static bool IsVehicleDriver(IEntity actor)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(actor);
		if (!character || !character.IsInVehicle())
			return false;

		CompartmentAccessComponent compartmentAccess = character.GetCompartmentAccessComponent();
		if (!compartmentAccess)
			return false;

		BaseCompartmentSlot slot = compartmentAccess.GetCompartment();
		if (!slot)
			return false;

		return slot.GetType() == ECompartmentType.PILOT;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this trip moves a vehicle, and therefore carries its own occupants.
	//!
	//! THE FIX FOR BUG-163, and the reason it is one shared predicate rather than a test on each side:
	//! the fare (client and server) and the recruit ring (server) must agree about whether recruits are
	//! being carried, or the panel prices a squad the server does not gather - the same client/server
	//! drift the whole service exists to prevent.
	//!
	//! WHY IT MATTERS. Teleporting a vehicle brings everyone sitting in it, for free, with no code of
	//! ours involved. Recruits gathered by radius include the ones IN that vehicle, and the ring
	//! placement then calls SetOrigin on entities that are still occupying a compartment - which is
	//! what made them vanish and left their map marker pointing at nonsense. Vehicle travel therefore
	//! gathers NO recruits at all: the occupants ride along, and anyone standing outside is left behind
	//! rather than teleported into the ring. Nobody is charged a per-recruit fare for a seat they were
	//! already sitting in.
	//! \param[in] verb OVT_TravelVerb.FAST_TRAVEL or .BUS. Buses never carry vehicles (MUST_EXIT_VEHICLE).
	//! \param[in] actor The acting player's controlled entity, supplied by the caller.
	//! \return True when the trip is a driver's fast travel.
	static bool VehicleCarriesOccupants(int verb, IEntity actor)
	{
		if (verb != OVT_TravelVerb.FAST_TRAVEL)
			return false;

		return IsVehicleDriver(actor);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a position is a camp's recorded location.
	//!
	//! SERVER-SIDE, and meaningful only AFTER ResolveFastTravelDestination has reassigned the request
	//! to the server's own vector - it compares against camp.location with the same tolerance the
	//! destination match used, so the answer agrees with the resolution by construction. Camps are
	//! player-placed and sit wherever the player stood, which is routinely within the 15 m authored-spot
	//! query of somebody's parking - the reason a camp arrival must NOT inherit whatever parking or
	//! vehicle point happens to be nearby (a camp authors character points and no vehicle points), and
	//! must go straight to the road search instead.
	//! \param[in] pos The position to test - after 6b, the server's vector for the destination.
	//! \return True when pos names a camp; false otherwise, or when the resistance manager is unavailable.
	static bool IsCampPosition(vector pos)
	{
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance || !resistance.m_Camps)
			return false;

		foreach (OVT_CampData camp : resistance.m_Camps)
		{
			if (!camp)
				continue;

			if (OVT_RespawnService.PositionsMatch(camp.location, pos))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a position is a deployed FOB's recorded location.
	//!
	//! SAFE ON BOTH MACHINES - m_FOBs reaches clients through the resistance manager's JIP stream and
	//! registration broadcasts - and it must stay that way: ValidateTravel consults it for the QRF
	//! exemption, and ValidateTravel is the shared rule table that keeps the panel's enable state and
	//! the server's refusal in agreement. Uses the same PositionsMatch tolerance as the destination
	//! resolution, so "is a FOB" and "resolved to this FOB" cannot drift apart.
	//! \param[in] pos The position to test.
	//! \return True when pos names a deployed FOB; false otherwise, or when the resistance manager is
	//! unavailable.
	static bool IsFobPosition(vector pos)
	{
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance || !resistance.m_FOBs)
			return false;

		foreach (OVT_FOBData fob : resistance.m_FOBs)
		{
			if (!fob)
				continue;

			if (OVT_RespawnService.PositionsMatch(fob.location, pos))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! How many of a player's recruits are close enough to travel with them.
	//!
	//! PARKED (INACTIVE) RECRUITS ARE NOT COUNTED, and the exclusion has to be here as well as on the
	//! server: this is what the map panel PRICES a trip at, and the server gathers the travellers with
	//! the same exclusion (OVT_TravelRequestComponent.ResolveTravellingRecruits). If only one side
	//! dropped them, a player standing in their own garrison would be quoted a fare for a squad that
	//! is not coming - the client/server drift this whole service exists to prevent.
	//!
	//! \param[in] persId Persistent id of the owning player.
	//! \param[in] originPos Position to measure from - the player's position BEFORE the teleport.
	//! \return The ACTIVE recruit count, or 0 when the recruit manager is unavailable.
	static int CountRecruitsInRadius(string persId, vector originPos)
	{
		OVT_RecruitManagerComponent recruitManager = OVT_Global.GetRecruits();
		if (!recruitManager)
			return 0;

		array<ref OVT_RecruitData> nearby = recruitManager.GetPlayerRecruitsInRadius(persId, originPos, RECRUIT_TRAVEL_RADIUS, true);
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
