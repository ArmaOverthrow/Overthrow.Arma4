//------------------------------------------------------------------------------------------------
//! IS THE RESISTANCE HOLDING THIS GROUND? - the one question every contest test in the campaign asks,
//! and the one place it is answered.
//!
//! ==========================================================================================
//! 🔴 "There should never be any 'players-only' contest test, it's resistance always (including
//! high command groups that will come later)." (author, 2026-08-21.)
//! ==========================================================================================
//!
//! WHAT THIS REPLACED AND WHY IT KEPT BREAKING. Every "is this place contested" test in the
//! deployments framework used to measure PLAYER-CONTROLLED ENTITIES ONLY - a walk over
//! PlayerManager.GetPlayers(). That answer was wrong in a way that got worse with every feature the
//! campaign grew:
//!   - the author watched a base structure demolished while his RECRUITS fought over it, because he
//!     personally stood a few metres outside the circle (2026-08-21);
//!   - it was fixed by adding "or a recruit" to one module, which was correct and immediately became
//!     the wrong shape - four other modules asked the identical question and still answered
//!     players-only;
//!   - and HIGH COMMAND GROUPS ARE COMING. A fourth `||` in five call sites, again, is not a design.
//!
//! ==========================================================================================
//! THE EXTENSION POINT IS THE FACTION, NOT A LIST OF KINDS, AND THAT IS THE WHOLE IDEA.
//! ==========================================================================================
//! This does not enumerate players, then recruits, then high command, then whatever comes after. It
//! asks the world for LIVING CHARACTERS OF THE RESISTANCE FACTION. A player body, a recruit, a high
//! command group's rifleman and anything a future feature invents are all resistance-faction
//! characters standing in the world, so a new kind of resistance force costs ZERO edits here rather
//! than one - which is a stronger guarantee than the "one edit in one place" that was asked for.
//!
//! ⚠ THE FACTION IS THE REGISTRY. Overthrow has no single component that owns "every force that
//! belongs to the resistance" - the recruit manager knows recruits, the player manager knows players,
//! and nothing knows both - so there is no registry to ask. FactionAffiliationComponent IS the
//! shared property all of them already carry, and it is what the rest of the engine uses to decide
//! who is on whose side. If a future feature ever does build that registry, this is the one method
//! that has to learn about it.
//!
//! ==========================================================================================
//! ⚠ CONTESTING GROUND IS NOT WATCHING SOMETHING HAPPEN. DO NOT USE THIS FOR VISIBILITY.
//! ==========================================================================================
//! Two rules in this framework deliberately stay PLAYERS-ONLY and must not be converted:
//!   the exfiltration rule    (OVT_BaseBehaviorDeploymentModule.IsPlayerWatchingDeployment,
//!                             difficulty.baseCloseRange) - a finished mission team may not evaporate
//!                             in front of somebody;
//!   the abandoned transport  (OVT_InsertionSpawningDeploymentModule.TickAbandonedTruck,
//!                             ABANDONED_TRUCK_PLAYER_RADIUS_M) - a truck may not vanish in front of
//!                             somebody.
//! Both ask "would a HUMAN watch this happen", because both exist to stop a player seeing something
//! that reads as a bug. A recruit is not embarrassed by a truck despawning and a high command group
//! does not file a review. Those two are the only players-only tests left in the framework and this
//! comment is the reason.
//!
//! ⚠ ON COST, because the players-only helper defended itself on exactly this ground and the
//! objection deserves an answer rather than a shrug. It said an agent sweep "would cost a sphere
//! query on every behavior module of every deployment on the map every ten seconds". That is true and
//! it is affordable: the query is bounded by the CONTEST RADIUS (150-320 m, not the map), it runs on
//! a bounded sphere rather than the map, and the modules that ask
//! are a handful of live objective operations plus a reinforcement check that runs once a minute. The
//! alternative - four registries OR-ed together at five call sites, one of which does not exist yet -
//! costs more in defects than this costs in queries.
//------------------------------------------------------------------------------------------------
class OVT_ResistancePresence
{
	//! Set by the query filter while a search is running. A static because
	//! BaseWorld.QueryEntitiesBySphere takes a plain function rather than a closure, which is the same
	//! constraint (and the same answer) as OVT_WorldUtils' own filters.
	protected static bool s_bFound;

	//! The faction the running search is looking for. Resolved once per search rather than once per
	//! entity - the lookup walks the faction manager and the query may see hundreds of entities.
	protected static Faction s_SearchFaction;

	//! Where the search was centred, so a hit can report how far out it was.
	protected static vector s_vSearchCentre;

	//! WHAT CLOSED THE GATE, in words, from the last search that found something.
	//!
	//! 🔴 THIS EXISTS BECAUSE A SILENTLY-REFUSING GATE COSTS PLAY-TEST ROUNDS. A rule that answers only
	//! "yes, held" tells a reader nothing about WHY, and "why" is the whole question when a gate turns
	//! out to be permanently closed - the author lost reinforcement at four towns to exactly that and
	//! nothing in any log said which character was holding the ground or how far away it was. One string
	//! per search costs nothing and answers it outright.
	protected static string s_sLastHold;

	//------------------------------------------------------------------------------------------------
	//! IS ANY LIVING RESISTANCE FORCE STANDING INSIDE THIS CIRCLE?
	//!
	//! ⚠ LIVING IS ASKED OF THE BODY, NOT OF A RECORD. A corpse is a dynamic entity with a faction and
	//! it holds no ground; SCR_DamageManagerComponent.IsDestroyed() is what tells them apart, and it is
	//! the same test the rest of this tree uses for "is that man still a man".
	//!
	//! ⚠ AND IT DELIBERATELY DOES NOT CARE WHO OWNS THE FORCE. A co-op server's ground may be held by
	//! three different players' recruits; a contest test that asked "MY recruits" would answer no for
	//! all three.
	//! \param[in] position Centre of the circle.
	//! \param[in] radius Its radius in metres. Non-positive contains nobody.
	//! \return True when at least one living resistance character is inside it.
	static bool IsGroundHeld(vector position, float radius)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return false;

		return IsGroundHeldBy(config.GetPlayerFactionData(), position, radius);
	}

	//------------------------------------------------------------------------------------------------
	//! The same question asked of the OCCUPYING faction: is anybody of theirs standing here?
	//!
	//! ⚠ AN ENTITY QUERY, NOT A VIRTUALIZATION READ, AND THAT IS THE POINT (author, 2026-08-25: "im
	//! here, its owned by them, theres noone here (confirmed in GM)"). A registered group that has not
	//! materialised reports its RECORD position, and a recapture team is registered AT the tower it is
	//! walking towards - so the virtualization answer is "they are holding it" while the ground is
	//! visibly empty. Any test a player is standing in front of has to ask the world, because the
	//! player can see the world.
	//! \param[in] position Centre of the sphere.
	//! \param[in] radius Sphere radius in metres. Non-positive is never held.
	//! \return True when at least one living occupying-faction character is inside it.
	static bool IsGroundHeldByOccupying(vector position, float radius)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return false;

		return IsGroundHeldBy(config.GetOccupyingFactionData(), position, radius);
	}

	//------------------------------------------------------------------------------------------------
	//! The shared body. One faction, one sphere, one answer.
	//! \param[in] faction The faction being looked for.
	//! \param[in] position Centre of the sphere.
	//! \param[in] radius Sphere radius in metres.
	//! \return True when at least one living character of \a faction is inside it.
	protected static bool IsGroundHeldBy(Faction faction, vector position, float radius)
	{
		if (radius <= 0 || !faction)
			return false;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		s_SearchFaction = faction;

		s_bFound = false;
		s_sLastHold = "";
		s_vSearchCentre = position;

		// ⚠ ALL, NOT DYNAMIC, AND THE CHANGE WAS DELIBERATE (2026-08-22). DYNAMIC is documented as
		// "entities without TFL_STATIC" and a character does qualify, so it SHOULD have worked - but
		// every other character query in this mod (OVT_WorldUtils.GetNearbyBodiesAndWeapons, the base
		// controller's parking sweep) passes ALL, and while this gate was under suspicion for refusing
		// forever it was one variable that could be removed for free rather than argued about.
		world.QueryEntitiesBySphere(position, radius, null, FilterLivingResistance, EQueryEntitiesFlags.ALL);

		s_SearchFaction = null;

		return s_bFound;
	}

	//------------------------------------------------------------------------------------------------
	//! The one membership test both filters make: a living character of s_SearchFaction.
	//! \param[in] entity One candidate inside the sphere.
	//! \return True when it counts.
	protected static bool IsLivingSearchFactionCharacter(IEntity entity)
	{
		if (!entity)
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return false;

		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
		if (!affiliation || affiliation.GetAffiliatedFaction() != s_SearchFaction)
			return false;

		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(character.FindComponent(SCR_DamageManagerComponent));
		if (damage && damage.IsDestroyed())
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! How far the nearest living resistance character is, for DIAGNOSTICS.
	//!
	//! ⚠ NOT a gate. IsGroundHeld short-circuits on its first hit, which is what makes it cheap; this
	//! cannot, so it walks every match. Call it from a debug-gated log line, not from a tick.
	//! \param[in] position Centre of the search.
	//! \param[in] searchRadius How far to look, in metres.
	//! \return Metres to the nearest, or float.MAX when there is none inside \a searchRadius.
	static float DistanceToNearest(vector position, float searchRadius)
	{
		if (searchRadius <= 0)
			return float.MAX;

		BaseWorld world = GetGame().GetWorld();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!world || !config)
			return float.MAX;

		s_SearchFaction = config.GetPlayerFactionData();
		if (!s_SearchFaction)
			return float.MAX;

		s_fNearest = float.MAX;
		s_vSearchCentre = position;

		world.QueryEntitiesBySphere(position, searchRadius, null, FilterNearestResistance, EQueryEntitiesFlags.ALL);

		s_SearchFaction = null;

		return s_fNearest;
	}

	//! Accumulator for DistanceToNearest, on the same single-threaded contract as s_bFound.
	protected static float s_fNearest;

	//------------------------------------------------------------------------------------------------
	//! DistanceToNearest's filter: keeps the closest match rather than stopping at the first.
	//! \param[in] entity One candidate inside the sphere.
	//! \return Always false - the query runs to completion.
	protected static bool FilterNearestResistance(IEntity entity)
	{
		if (!IsLivingSearchFactionCharacter(entity))
			return false;

		float distance = vector.Distance(OVT_WorldUtils.GetWorldOrigin(entity), s_vSearchCentre);
		if (distance < s_fNearest)
			s_fNearest = distance;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The query filter.
	//!
	//! ⚠ THE RETURN VALUE IS NOT "keep walking", AND AN EARLIER VERSION OF THIS COMMENT CLAIMED IT WAS.
	//! BaseWorld's filter answers whether an entity is passed on to the addEntity callback, and this
	//! file's own model for it - OVT_WorldUtils.FilterSpawnPointEntities - returns false for EVERYTHING
	//! while still collecting every match, which is the proof that false does not terminate the walk.
	//! The s_bFound short-circuit at the top is therefore the only saving there is: the sphere is still
	//! walked in full and each remaining entity costs one bool test. Correctness was never affected; the
	//! "stops at the first hit" claim was simply wrong and is the kind of thing that gets believed.
	//! \param[in] entity One candidate inside the sphere.
	//! \return Whether the query should keep going.
	protected static bool FilterLivingResistance(IEntity entity)
	{
		if (s_bFound)
			return false;

		if (!entity)
			return true;

		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!IsLivingSearchFactionCharacter(entity))
			return true;

		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));

		s_bFound = true;

		// Named, not counted. "Something resistance-flavoured is nearby" is what the caller already knows;
		// what a reader needs is which entity, whose faction it thinks it is, and how far out - because
		// the three failure modes are a body nobody remembers, a faction key that matches more than it
		// should, and a radius that is simply too big.
		int metres = Math.Round(vector.Distance(OVT_WorldUtils.GetWorldOrigin(character), s_vSearchCentre));

		string factionKey = "no faction key";
		Faction held = affiliation.GetAffiliatedFaction();
		if (held)
			factionKey = held.GetFactionKey();

		s_sLastHold = string.Format("%1 (faction %2) at %3 m", character.ClassName(), factionKey, metres.ToString());

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return What closed the gate on the last search that found something, or an empty string.
	static string GetLastHold()
	{
		return s_sLastHold;
	}
}
