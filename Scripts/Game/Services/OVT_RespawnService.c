//------------------------------------------------------------------------------------------------
//! Where a respawning player asked to be put. Sent as an int over the wire, exactly as
//! OVT_TravelVerb is.
//!
//! HOME is not a location record and carries no vector: it routes through the untouched
//! CreateFreshCharacter path, which is what makes "respawn at home always works, even inside an
//! active QRF" true by construction instead of by a special case.
//------------------------------------------------------------------------------------------------
enum OVT_RespawnDestination
{
	HOME,		//!< The player's home, resolved entirely by the server's existing home chain.
	LOCATION	//!< A map location the player picked. Named, never supplied as a coordinate.
}

//------------------------------------------------------------------------------------------------
//! Outcome of a respawn request. One vocabulary for both machines: the server uses it to report
//! what it did, the client uses it to decide whether to close the screen and what to say.
//! Sent as an int over the wire.
//------------------------------------------------------------------------------------------------
enum OVT_RespawnResult
{
	OK,					//!< Spawned where the player asked.
	OK_FELL_BACK_HOME,	//!< The chosen location was no longer eligible; spawned at home instead. Alive either way.
	NO_PLAYER,			//!< The acting player could not be resolved (no persistent id, gone).
	NOT_ELIGIBLE,		//!< The request named nothing the server recognises as an eligible position.
	SPAWN_FAILED		//!< The spawn itself failed. The player is still awaiting and must be re-asked.
}

//------------------------------------------------------------------------------------------------
//! Centralized respawn eligibility rules for Overthrow. Modelled on OVT_FastTravelService.
//!
//! THE ONE RULE SET, run by BOTH machines. The client calls the pure predicates to decide which
//! markers to draw on the respawn screen; the server calls the same predicates while re-deriving
//! the eligible set from its own managers. Two copies would drift, and "displayed availability
//! matches enforced availability" is only achievable while there is one.
//!
//! THE INVARIANT THAT MAKES THAT POSSIBLE: no function in this file resolves the acting player from
//! the MACHINE - SCR_PlayerController.GetLocalControlledEntity() is null on a dedicated server and
//! is the HOST's character on a listen server, which would enumerate the host's property against
//! another player's request. The persistent id is therefore always a parameter. Unlike
//! OVT_FastTravelService there is no CLIENT-ONLY section at all: nothing here needs one, and the
//! fence is kept by the absence rather than by a comment.
//!
//! THE PURE / IMPURE SPLIT IS LOAD-BEARING, not aesthetic. Everything under PURE PREDICATES takes
//! only values and returns only values - no world, no manager, no game mode - which is the only
//! reason any of this feature can be asserted in the world-free Logic test tier. Anything that
//! reads a manager lives below that section and is asserted by the manual gate, not by a test.
//!
//! ! NOTHING IN THIS FILE CALLS OVT_FastTravelService.CanGlobalFastTravel, deliberately. That
//! check's minimum-distance rule, its wanted rule and its entire fare model are all measured from a
//! LIVING player's current position. A respawning player has no current position to measure from -
//! their character does not exist yet - so every one of those rules is meaningless here, and
//! applying them would refuse a respawn for being "too close" to a corpse.
//------------------------------------------------------------------------------------------------
class OVT_RespawnService
{
	//! How near the client's named vector must be to the server's own recorded vector to count as
	//! the same place.
	//!
	//! NOT ZERO because the vector makes a round trip through the map record and the wire, and a
	//! float that survives that is not guaranteed to be bit-identical to the one the manager holds.
	//! SMALL because this comparison IS the anti-arbitrary-position rule: the client names a place,
	//! it does not supply a coordinate, and the spawn always uses the SERVER's vector. A generous
	//! tolerance would let a crafted request drift the accepted point away from anything the server
	//! actually offered. Two metres is smaller than any two distinct spawnable locations in
	//! Overthrow and larger than any transport error.
	static const float MATCH_TOLERANCE = 2.0;

	//! How close a base record has to be to the broadcast QRF point to be read as the battle's target.
	//! Generous, because it only ever makes the rule STRICTER. A TOWN QRF has no base near its centre,
	//! which is why the distance is checked at all - GetNearestBase answers at any range.
	static const float QRF_TARGET_RESOLVE_M = 50.0;

	//------------------------------------------------------------------------------------------------
	// PURE PREDICATES - no world, no manager, no game mode.
	//
	// These are what both machines share and what the Logic tier asserts. Keep them that way: a
	// single manager lookup added here silently removes this feature's only automated coverage.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether a base may be respawned at.
	//! A base held by the occupying faction is theirs, not ours; everything else is a resistance or
	//! neutral base and is fair game.
	//! \param[in] isOccupying True when the base is held by the occupying faction.
	//! \return True when the base is eligible.
	static bool IsBaseEligible(bool isOccupying)
	{
		if (isOccupying)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a FOB may be respawned at.
	//! Always true. A FOB only exists because the resistance built it, and Overthrow has no per-FOB
	//! ownership or access rule to consult. Stated as a predicate anyway so the four types read
	//! alike and so a future access rule has exactly one place to land.
	//! \return True, always.
	static bool IsFobEligible()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether deployed FOBs are exempt from the active-QRF filters (respawn AND fast travel).
	//! Reads the allowFOBDuringQRF difficulty setting, which defaults on and is JIP-streamed, so
	//! client markers and the server's enumeration answer identically. Fails toward the DEFAULT
	//! (exempt) when the config is unavailable, matching the attribute default.
	//! \return True when a FOB inside an active QRF stays a valid destination.
	static bool AllowFobDuringQrf()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config || !config.m_Difficulty)
			return true;

		return config.m_Difficulty.allowFOBDuringQRF;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a camp may be respawned at.
	//! Public camps are shared; a private camp belongs to its owner alone. Mirrors the rule the map's
	//! camp type has always applied to fast travel.
	//! \param[in] isPrivate True when the camp is marked private.
	//! \param[in] ownerId Persistent id of the camp's owner.
	//! \param[in] playerId Persistent id of the acting player.
	//! \return True when the camp is eligible for that player.
	static bool IsCampEligible(bool isPrivate, string ownerId, string playerId)
	{
		if (!isPrivate)
			return true;

		if (playerId.IsEmpty())
			return false;

		return ownerId == playerId;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a house may be respawned at.
	//!
	//! ! THE EMPTY-ID CASE IS THE DANGEROUS ONE. An unresolved persistent id is "" and an unowned or
	//! unrented record also carries "" in the corresponding field, so a naive equality test would
	//! make EVERY house match for a player whose id failed to resolve. That is the failure this
	//! predicate exists to make impossible, and it is pinned by a Logic case.
	//! \param[in] ownerId Persistent id of the owner, or "" when nobody owns it.
	//! \param[in] renterId Persistent id of the renter, or "" when nobody rents it.
	//! \param[in] playerId Persistent id of the acting player.
	//! \return True when the player owns or rents the house.
	static bool IsHouseEligible(string ownerId, string renterId, string playerId)
	{
		if (playerId.IsEmpty())
			return false;

		if (ownerId == playerId)
			return true;

		return renterId == playerId;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a position is inside a running QRF.
	//! Takes the QRF state as values so it can be asserted world-free; the reading half is
	//! IsPositionInActiveQRF below.
	//! \param[in] qrfActive True when a QRF is currently running.
	//! \param[in] qrfLocation Centre of the running QRF. Ignored when qrfActive is false.
	//! \param[in] pos World position to test.
	//! \return True when a QRF is running and pos is within QRF_RANGE of it.
	static bool IsInsideQrf(bool qrfActive, vector qrfLocation, vector pos)
	{
		if (!qrfActive)
			return false;

		return vector.Distance(qrfLocation, pos) < OVT_QRFControllerComponent.QRF_RANGE;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether two positions name the same place, within MATCH_TOLERANCE.
	//! \param[in] a First position.
	//! \param[in] b Second position.
	//! \return True when the two are within MATCH_TOLERANCE of each other.
	static bool PositionsMatch(vector a, vector b)
	{
		return vector.Distance(a, b) <= MATCH_TOLERANCE;
	}

	//------------------------------------------------------------------------------------------------
	//! The localized text for a result code, so the server's after-the-fact hint and the client
	//! speak one vocabulary.
	//! \param[in] result An OVT_RespawnResult code.
	//! \return A localization key, or "" for OVT_RespawnResult.OK.
	static string ReasonKeyFor(int result)
	{
		switch (result)
		{
			case OVT_RespawnResult.OK:					return "";
			case OVT_RespawnResult.OK_FELL_BACK_HOME:	return "#OVT-Respawn_FellBackHome";
			case OVT_RespawnResult.NOT_ELIGIBLE:		return "#OVT-Respawn_NotEligible";
		}

		// NO_PLAYER and SPAWN_FAILED both mean "the server could not do it" to a player, and both
		// leave them on the screen to try again, so both get the generic failure line.
		return "#OVT-Respawn_Failed";
	}

	//------------------------------------------------------------------------------------------------
	// SHARED LOOKUPS - safe on both machines
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether a position sits inside a currently running QRF.
	//! Guarded rather than assumed: the occupying faction manager is null before the game mode
	//! exists. A missing manager means "we cannot tell", and here that resolves to NOT excluded -
	//! refusing every location on a partially replicated client would leave a dead player with an
	//! empty screen, and the server re-checks on arrival anyway.
	//!
	//! ⚠ ACTIVE **AND** REVEALED (occupying/counter-attacks D15). During a counter-attack's silent
	//! encirclement a player may still respawn at the objective, because nobody has told them not to -
	//! a refusal there would be a free reveal, delivered by the respawn screen before the notification.
	//! A player-initiated battle is revealed from creation, so this is unchanged for every battle a
	//! player starts.
	//! \param[in] pos World position to test.
	//! \return True when a REVEALED QRF is running and pos is within QRF_RANGE of it.
	static bool IsPositionInActiveQRF(vector pos)
	{
		OVT_OccupyingFactionManager occupyingFaction = OVT_Global.GetOccupyingFaction();
		if (!occupyingFaction)
			return false;

		return IsInsideQrf(occupyingFaction.m_bQRFActive && occupyingFaction.m_bQRFRevealed, occupyingFaction.m_vQRFLocation, pos);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a position is a base the RESISTANCE holds that is not itself the battle.
	//!
	//! 🔴 THE RULE (author, 2026-08-25): "you should be able to fast travel or respawn at a captured
	//! base inside a QRF as long as that base isn't the actual QRF being counter-attacked". A QRF ring
	//! is 1 km wide and used to blank out every destination inside it; a base the player already took,
	//! sitting inside that ring but not under attack, is not part of the fight and refusing it just
	//! strands people a kilometre away from where they are needed.
	//!
	//! ⚠ MATCHED ON POSITION AGAINST m_vQRFLocation, NEVER ON m_iCurrentQRFBase. The index is NOT in
	//! the JIP payload (a standing epic defect - it is why JIP clients draw the wrong map circle),
	//! while m_vQRFLocation is broadcast by RpcDo_SetQRFActive to every machine. This predicate runs on
	//! both the client (to enable the button) and the server (to allow the act), so it may only read
	//! state that both of them provably have.
	//!
	//! ⚠ IT DOES NOT ASK WHETHER THE QRF IS ACTIVE. Callers have already established that; this answers
	//! only "is this destination the thing being fought over".
	//! \param[in] pos The destination being tested.
	//! \return True when pos is a resistance-held base other than the QRF's target.
	static bool IsCapturedBaseAwayFromQrfTarget(vector pos)
	{
		OVT_OccupyingFactionManager occupyingFaction = OVT_Global.GetOccupyingFaction();
		if (!occupyingFaction || !occupyingFaction.m_Bases)
			return false;

		// The battle itself is never exempt, whichever base it is - and it is identified TWO ways on
		// purpose. m_vQRFLocation is base.GetOwner().GetOrigin() while the record carries
		// ent.GetOrigin() of that same entity, so the two agree today; a rule that INVERTS on a few
		// metres of drift (letting a player travel into the base being counter-attacked) is not one to
		// rest on a 2 m tolerance.
		if (PositionsMatch(occupyingFaction.m_vQRFLocation, pos))
			return false;

		OVT_BaseData target = occupyingFaction.GetNearestBase(occupyingFaction.m_vQRFLocation);
		if (target && vector.Distance(target.location, occupyingFaction.m_vQRFLocation) <= QRF_TARGET_RESOLVE_M
			&& PositionsMatch(target.location, pos))
			return false;

		foreach (OVT_BaseData base : occupyingFaction.m_Bases)
		{
			if (!base || !PositionsMatch(base.location, pos))
				continue;

			// Ours, and not the one under attack.
			return !base.IsOccupyingFaction();
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	// SERVER ENUMERATION
	//
	// The authority half. This does NOT re-run the map's checks - OVT_MapLocationType instances only
	// exist on a machine with a map open, so the server cannot see them. It is a second, independent
	// enumeration built straight from the managers, agreeing with the client only because both ends
	// call the same pure predicates above.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Every position one player is entitled to respawn at, derived from the server's own managers.
	//!
	//! Home is deliberately absent: it is not a location record, it needs no eligibility test, and it
	//! is reached by a destination code rather than by a vector.
	//!
	//! ! ALSO THE FAST-TRAVEL DESTINATION SET, and that is not a coincidence to be tidied away later.
	//! Exactly four location types carry m_bCanFastTravel 1 in Configs/Map/OverthrowMap.conf - Bases,
	//! FOBs, Camps and Houses - and their per-type CanFastTravel overrides apply the same four rules
	//! this function applies, from the same four managers, reading the same vectors (base.location,
	//! fob.location, camp.location, house.GetOrigin(), warehouses skipped). One enumeration serving
	//! both verbs is what stops "respawnable here" and "travellable here" drifting apart silently.
	//! See OVT_FastTravelService.ResolveFastTravelDestination.
	//! \param[in] persId Persistent id of the acting player. Always a parameter, never resolved from the machine.
	//! \param[out] positions Appended to, never cleared - the caller owns the array.
	//! \param[in] excludeActiveQrf True to drop positions inside a running QRF, which is the respawn
	//! rule. Fast travel passes false because it has its OWN QRF rules - m_Difficulty.QRFFastTravelMode
	//! can be FREE, which permits a trip into a QRF that this filter would have refused as an
	//! unrecognised destination.
	static void CollectEligiblePositions(string persId, notnull array<vector> positions, bool excludeActiveQrf = true)
	{
		if (persId.IsEmpty())
			return;

		// Bases held by anyone other than the occupying faction
		OVT_OccupyingFactionManager occupyingFaction = OVT_Global.GetOccupyingFaction();
		if (occupyingFaction && occupyingFaction.m_Bases)
		{
			foreach (OVT_BaseData base : occupyingFaction.m_Bases)
			{
				if (!base)
					continue;

				if (!IsBaseEligible(base.IsOccupyingFaction()))
					continue;

				// A base we already hold, inside the ring but not the battle, stays a destination.
				if (excludeActiveQrf && IsPositionInActiveQRF(base.location) && !IsCapturedBaseAwayFromQrfTarget(base.location))
					continue;

				positions.Insert(base.location);
			}
		}

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (resistance)
		{
			if (resistance.m_FOBs)
			{
				// FOBs are exempt (by default) from the excludeActiveQrf filter that drops every
				// other type: a deployed FOB is the forward spawn for exactly the battle a QRF
				// represents, and it is needed most by players who died defending it without having
				// set it as home. allowFOBDuringQRF (difficulty setting, JIP-streamed) lets a server
				// owner switch the exemption off. OVT_MapLocationFOB.CanRespawn carries the matching
				// client-side rule; the two must stay in agreement.
				bool fobQrfExempt = AllowFobDuringQrf();

				foreach (OVT_FOBData fob : resistance.m_FOBs)
				{
					if (!fob)
						continue;

					if (!IsFobEligible())
						continue;

					if (excludeActiveQrf && !fobQrfExempt && IsPositionInActiveQRF(fob.location))
						continue;

					positions.Insert(fob.location);
				}
			}

			if (resistance.m_Camps)
			{
				foreach (OVT_CampData camp : resistance.m_Camps)
				{
					if (!camp)
						continue;

					if (!IsCampEligible(camp.isPrivate, camp.owner, persId))
						continue;

					if (excludeActiveQrf && IsPositionInActiveQRF(camp.location))
						continue;

					positions.Insert(camp.location);
				}
			}
		}

		// Houses. Ownership is the manager's answer, so the eligibility predicate is satisfied by
		// construction - the sets ARE "owned by this player" and "rented by this player". The
		// predicate is still applied so the rule has exactly one statement, and so an id that failed
		// to resolve refuses here as well as on the client.
		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if (realEstate)
		{
			BaseWorld world = GetGame().GetWorld();
			if (world)
			{
				CollectHousePositions(world, realEstate.GetOwned(persId), persId, true, positions, excludeActiveQrf);
				CollectHousePositions(world, realEstate.GetRented(persId), persId, false, positions, excludeActiveQrf);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Appends the eligible positions of one set of house entity ids.
	//! Warehouses are skipped for the same reason the map's house type skips them: they are property,
	//! but they are not somewhere a player lives or spawns.
	//! \param[in] world World to resolve entity ids against.
	//! \param[in] houses Owned or rented set from the real estate manager. May be null.
	//! \param[in] persId Persistent id of the acting player.
	//! \param[in] asOwner True when houses is the owned set, false when it is the rented set.
	//! \param[out] positions Appended to.
	//! \param[in] excludeActiveQrf True to drop houses inside a running QRF. See CollectEligiblePositions.
	protected static void CollectHousePositions(BaseWorld world, set<EntityID> houses, string persId, bool asOwner, notnull array<vector> positions, bool excludeActiveQrf = true)
	{
		if (!houses)
			return;

		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if (!realEstate)
			return;

		string ownerId = "";
		string renterId = "";
		if (asOwner)
			ownerId = persId;
		else
			renterId = persId;

		if (!IsHouseEligible(ownerId, renterId, persId))
			return;

		foreach (EntityID houseId : houses)
		{
			IEntity house = world.FindEntityByID(houseId);
			if (!house)
				continue;

			OVT_RealEstateConfig houseConfig = realEstate.GetConfig(house);
			if (!houseConfig || houseConfig.m_IsWarehouse)
				continue;

			vector pos = house.GetOrigin();
			if (excludeActiveQrf && IsPositionInActiveQRF(pos))
				continue;

			positions.Insert(pos);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Turns a client's named destination into a position the server is willing to spawn at.
	//!
	//! ! THE RETURNED VECTOR ALWAYS COMES FROM CollectEligiblePositions, NEVER FROM requestedPos.
	//! requestedPos is used as a LOOKUP KEY and nothing else: the client names a place, it does not
	//! supply a coordinate. A client that sends a crafted vector matches nothing and gets home.
	//! \param[in] persId Persistent id of the acting player. Always a parameter.
	//! \param[in] destination An OVT_RespawnDestination code.
	//! \param[in] requestedPos The vector the client sent. A key, not a position.
	//! \param[out] resolvedPos The SERVER's own recorded vector for the matched location. vector.Zero when nothing matched.
	//! \return OVT_RespawnResult.OK on a match, NO_PLAYER on an unresolvable id, NOT_ELIGIBLE otherwise.
	static int ResolveRespawnPosition(string persId, int destination, vector requestedPos, out vector resolvedPos)
	{
		resolvedPos = vector.Zero;

		if (persId.IsEmpty())
			return OVT_RespawnResult.NO_PLAYER;

		// HOME carries no vector and never reaches this function's matching half. Anything that is
		// not LOCATION is a value this service does not understand and must not guess at.
		if (destination != OVT_RespawnDestination.LOCATION)
			return OVT_RespawnResult.NOT_ELIGIBLE;

		array<vector> eligible = new array<vector>();
		CollectEligiblePositions(persId, eligible);

		foreach (vector candidate : eligible)
		{
			if (!PositionsMatch(candidate, requestedPos))
				continue;

			resolvedPos = candidate;
			return OVT_RespawnResult.OK;
		}

		return OVT_RespawnResult.NOT_ELIGIBLE;
	}
}
