//------------------------------------------------------------------------------------------------
//! ONE RULE SET, TWO MACHINES. The campaign's spatial-influence predicates, extracted from the code
//! that already applied them so that the server which decides a modifier and the map layer which
//! draws it cannot drift apart.
//!
//! PURE BY CONSTRUCTION: no engine call, no world, no manager, no config lookup, no BaseContainer,
//! no widget. Every input is handed in - the caller reads the difficulty ranges and the faction
//! indices itself and passes them here. That is the whole reason this file exists as a separate
//! class rather than as methods on the town manager: it makes the rules assertable in the world-free
//! test tier, which is the same split OVT_TerritorySolver uses for the same reason.
//!
//! THE TWO RANGE TESTS DISAGREE ABOUT THEIR BOUNDARY, AND THAT IS DELIBERATE. IsProximitySource is
//! STRICTLY less-than; IsMomentumSource is INCLUSIVE. Each reproduces the comparison the campaign
//! already shipped, and a Logic case pins each boundary so that neither can be quietly "tidied" into
//! the other by a later reader who notices they look almost the same.
//!
//! THE POLICY IS NOT HERE. Every function below returns an OUTCOME - a polarity, a name, a yes/no -
//! and the caller decides what to do with it. OVT_TownManagerComponent.CheckUpdateModifiers keeps
//! its own add/remove sequencing unchanged, because an extraction that also moved the policy would
//! not be an extraction.
//!
//! ModifierNameFor IS THE SINGLE HOME OF THE MODIFIER ID STRINGS. They are positional-index keys
//! into supportModifiers.conf resolved by name, so a rename that lands in one place and not the
//! other is silent: the campaign stops applying a modifier and the overlay starts asserting one that
//! does not exist. One function, one file, one Logic case over all five.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Which way a source pushes the support of the town it reaches.
//!
//! NONE is a real answer, not an error: a town with no source of a given kind in range is the
//! ordinary case, and the caller reads it as "remove whatever was there".
//------------------------------------------------------------------------------------------------
enum OVT_InfluencePolarity
{
	NONE,
	POSITIVE,
	NEGATIVE
}

//------------------------------------------------------------------------------------------------
//! The three kinds of location that project influence onto a town.
//!
//! These are the only sources in the modifier config that have a second LOCATION. Everything else is
//! event-sourced - a kill, a transaction, a roll, a low-stability tick - and has no source to relate
//! a town to.
//------------------------------------------------------------------------------------------------
enum OVT_InfluenceSourceKind
{
	RADIO_TOWER,
	MILITARY_BASE,
	MOMENTUM_TOWN
}

//------------------------------------------------------------------------------------------------
//! Whether a relation that exists geometrically is one the campaign is currently applying.
//!
//! CONSUMERS MUST SWITCH ON THIS WITH A default: BRANCH, never treat it as a boolean. It is a small
//! enum today and the temptation to write `if (state == ACTIVE) ... else ...` is obvious, but that
//! shape has to be rewritten the day a third state exists, whereas a switch with a default does not.
//------------------------------------------------------------------------------------------------
enum OVT_InfluenceEdgeState
{
	ACTIVE,
	SUPPRESSED
}

//------------------------------------------------------------------------------------------------
//! Static, world-free predicates. Never instantiated.
//------------------------------------------------------------------------------------------------
class OVT_InfluenceRules
{
	//! How far a player-held town lends momentum to its neighbours, in metres. Moved here from a
	//! protected const on the momentum modifier so that both the server that applies momentum and any
	//! reader that describes it are quoting the same number.
	static const float MOMENTUM_RANGE = 2000.0;

	//! Support modifier applied to a town within reach of an OCCUPYING radio tower that is on the air.
	static const string MODIFIER_TOWER_NEGATIVE = "NearbyRadioTowerNegative";

	//! Support modifier applied to a town within reach of a NON-occupying radio tower that is on the air.
	static const string MODIFIER_TOWER_POSITIVE = "NearbyRadioTowerPositive";

	//! Support modifier applied to a town within reach of an OCCUPYING military base.
	static const string MODIFIER_BASE_NEGATIVE = "NearbyBaseNegative";

	//! Support modifier applied to a town within reach of a NON-occupying military base.
	static const string MODIFIER_BASE_POSITIVE = "NearbyBasePositive";

	//! Support modifier applied to a town with a player-held town within MOMENTUM_RANGE.
	static const string MODIFIER_MOMENTUM = "RevolutionaryMomentum";

	//------------------------------------------------------------------------------------------------
	//! Whether a radio tower or a military base standing at sourcePos reaches a town at townPos.
	//!
	//! THREE-DIMENSIONAL AND STRICTLY LESS-THAN, both reproducing the campaign exactly. The distance
	//! includes the height difference, which on hilly ground is a real difference from a plan-view
	//! test; and a source sitting exactly on the range boundary does NOT reach, so the range is an
	//! open interval. Both properties are pinned by Logic cases because both are the kind of detail a
	//! second implementation gets subtly wrong without ever erroring.
	//! \param[in] townPos World position of the town being influenced.
	//! \param[in] sourcePos World position of the tower or base.
	//! \param[in] range Reach in metres, read from difficulty by the caller.
	//! \return True when the source is inside the range.
	static bool IsProximitySource(vector townPos, vector sourcePos, float range)
	{
		return vector.Distance(townPos, sourcePos) < range;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a player-held town at otherTownPos lends momentum to a town at townPos.
	//!
	//! INCLUSIVE, unlike IsProximitySource. The asymmetry is inherited rather than designed, and it is
	//! reproduced rather than corrected: this predicate's job is to say what the campaign does, and
	//! changing where either boundary sits would move modifiers on and off towns on live saves.
	//! \param[in] townPos World position of the town receiving momentum.
	//! \param[in] otherTownPos World position of the player-held town projecting it.
	//! \return True when the two towns are within MOMENTUM_RANGE of each other.
	static bool IsMomentumSource(vector townPos, vector otherTownPos)
	{
		return IsWithinMomentumRange(vector.Distance(townPos, otherTownPos));
	}

	//------------------------------------------------------------------------------------------------
	//! The momentum reach comparison on its own, separated from the measurement.
	//!
	//! THIS SEAM EXISTS BECAUSE THE BOUNDARY IS OTHERWISE UNOBSERVABLE. The engine's distance is not a
	//! correctly-rounded square root: measured against two points separated by exactly 2000 m it
	//! answers one unit in the last place HIGH, and no arrangement of world points makes it answer
	//! exactly 2000. So through IsMomentumSource alone, this comparison being inclusive or strict is
	//! the same function, and a case built on world coordinates could never tell them apart. Splitting
	//! the comparison out is what lets the inclusiveness be pinned at all - and it is the reason the
	//! proximity test needs no equivalent, since its range is a parameter and a case can simply choose
	//! one the engine does measure exactly.
	//!
	//! The practical consequence for the campaign is worth knowing and is NOT a defect: two towns
	//! exactly 2000 m apart do not exchange momentum, because the measurement puts them just outside.
	//! \param[in] distance Distance between the two towns, in metres.
	//! \return True when the distance is inside the reach, boundary included.
	static bool IsWithinMomentumRange(float distance)
	{
		return distance <= MOMENTUM_RANGE;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a town can RECEIVE revolutionary momentum.
	//!
	//! A town the player already holds is not a momentum target - momentum models a neighbouring
	//! liberation pulling an occupied town along, so a town that has already come across has nothing
	//! left to be pulled toward.
	//! \param[in] townFaction Faction index currently controlling the town.
	//! \param[in] playerFactionIndex Faction index of the player/resistance faction.
	//! \return True when the town is a candidate target for momentum.
	static bool TownQualifiesForMomentum(int townFaction, int playerFactionIndex)
	{
		return townFaction != playerFactionIndex;
	}

	//------------------------------------------------------------------------------------------------
	//! Which way a single source of any kind pushes support.
	//! \param[in] isOccupyingFaction True when the source is held by the occupying faction.
	//! \return NEGATIVE for an occupier-held source, POSITIVE otherwise.
	static OVT_InfluencePolarity PolarityForSource(bool isOccupyingFaction)
	{
		if (isOccupyingFaction)
			return OVT_InfluencePolarity.NEGATIVE;

		return OVT_InfluencePolarity.POSITIVE;
	}

	//------------------------------------------------------------------------------------------------
	//! ENEMY WINS OVER FRIENDLY. Resolves a whole set of same-kind sources in range of one town into
	//! the single polarity the campaign applies.
	//!
	//! One enemy source in range beats any number of friendly ones of the same kind, which is why a
	//! town can sit inside a friendly tower's reach and still carry the negative modifier. That
	//! outcome is invisible in game today and is the single rule the influence overlay exists to make
	//! legible, so it is extracted and pinned even though the server is currently its only caller.
	//! \param[in] hasEnemy True when at least one occupier-held source of this kind is in range.
	//! \param[in] hasFriendly True when at least one non-occupier source of this kind is in range.
	//! \return NEGATIVE, POSITIVE, or NONE when nothing of this kind is in range.
	static OVT_InfluencePolarity ResolveProximity(bool hasEnemy, bool hasFriendly)
	{
		if (hasEnemy)
			return OVT_InfluencePolarity.NEGATIVE;

		if (hasFriendly)
			return OVT_InfluencePolarity.POSITIVE;

		return OVT_InfluencePolarity.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! The support modifier a (source kind, polarity) pair maps to.
	//!
	//! Returns an EMPTY STRING for a combination that has no modifier rather than inventing a name.
	//! Momentum has a positive form only, and NONE is not a modifier at all - it is the answer that
	//! tells a caller to remove one. An empty name is refused by the modifier lookup it would be
	//! handed to, so the failure mode of asking for a combination that does not exist is nothing
	//! happening, not a wrong modifier being applied.
	//! \param[in] kind Which kind of location is the source.
	//! \param[in] polarity Which way it pushes.
	//! \return The modifier name as authored in the support modifier config, or "" when the pair maps
	//! to no modifier.
	static string ModifierNameFor(OVT_InfluenceSourceKind kind, OVT_InfluencePolarity polarity)
	{
		switch (kind)
		{
			case OVT_InfluenceSourceKind.RADIO_TOWER:
			{
				if (polarity == OVT_InfluencePolarity.NEGATIVE)
					return MODIFIER_TOWER_NEGATIVE;

				if (polarity == OVT_InfluencePolarity.POSITIVE)
					return MODIFIER_TOWER_POSITIVE;

				return string.Empty;
			}

			case OVT_InfluenceSourceKind.MILITARY_BASE:
			{
				if (polarity == OVT_InfluencePolarity.NEGATIVE)
					return MODIFIER_BASE_NEGATIVE;

				if (polarity == OVT_InfluencePolarity.POSITIVE)
					return MODIFIER_BASE_POSITIVE;

				return string.Empty;
			}

			case OVT_InfluenceSourceKind.MOMENTUM_TOWN:
			{
				if (polarity == OVT_InfluencePolarity.POSITIVE)
					return MODIFIER_MOMENTUM;

				return string.Empty;
			}

			default:
			{
				return string.Empty;
			}
		}

		return string.Empty;
	}
}
