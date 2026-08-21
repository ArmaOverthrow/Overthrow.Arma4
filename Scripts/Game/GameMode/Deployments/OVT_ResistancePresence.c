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
//! the DYNAMIC layer only, it stops at the first hit rather than collecting, and the modules that ask
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
		if (radius <= 0)
			return false;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return false;

		s_SearchFaction = config.GetPlayerFactionData();
		if (!s_SearchFaction)
			return false;

		s_bFound = false;

		// DYNAMIC only: characters are dynamic entities, and asking for ALL would walk every tree, wall
		// and rock inside the radius to find them.
		world.QueryEntitiesBySphere(position, radius, null, FilterLivingResistance, EQueryEntitiesFlags.DYNAMIC);

		s_SearchFaction = null;

		return s_bFound;
	}

	//------------------------------------------------------------------------------------------------
	//! The query filter. Returns FALSE once a hit is found, which is how BaseWorld's query is told to
	//! stop walking - this is a "does one exist" question and there is nothing to gain from counting
	//! the rest.
	//! \param[in] entity One candidate inside the sphere.
	//! \return Whether the query should keep going.
	protected static bool FilterLivingResistance(IEntity entity)
	{
		if (s_bFound)
			return false;

		if (!entity)
			return true;

		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return true;

		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return true;

		if (affiliation.GetAffiliatedFaction() != s_SearchFaction)
			return true;

		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(character.FindComponent(SCR_DamageManagerComponent));
		if (damage && damage.IsDestroyed())
			return true;

		s_bFound = true;

		return false;
	}
}
