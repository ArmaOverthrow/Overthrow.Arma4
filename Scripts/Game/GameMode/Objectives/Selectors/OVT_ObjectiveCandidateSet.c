//------------------------------------------------------------------------------------------------
//! WHICH SOURCES OF CANDIDATES A SELECTOR WANTS LOOKED AT.
//!
//! A FLAG SET, AND THAT IS THE WHOLE POINT (D6). Selection collects candidates ONCE per round and
//! hands the same set to every plan, so the cost that would otherwise multiply with the number of
//! plans - walking the town list, walking the base list, measuring reach and tower coverage per
//! candidate - is paid once. Each selector declares the sources it can score; the union of every
//! eligible plan's flags is what actually gets collected, so a registry with no base plan in it never
//! walks the base list at all, and a future "raid a discovered camp" doctrine adds a SOURCE rather
//! than a second collection pass.
//!
//! ⚠ THE FLAG IS ALSO THE MASK. A selector may only ever win a candidate whose kind is in its flag
//! set - the runner builds the per-plan selection mask from these flags and the blacklist together,
//! so a town selector cannot accidentally commit to a base by scoring it zero. Scoring zero is a
//! legal score; not being a candidate is a different statement, and the two are kept apart here.
//!
//! ⚠ APPEND ONLY, AND POWERS OF TWO. These travel in an authored .conf as a flag field, exactly as
//! OVT_FactionTypeFlag does, so renumbering a member silently re-points every authored selector at
//! the wrong source.
//------------------------------------------------------------------------------------------------
enum OVT_EObjectiveCandidateSource
{
	//! Every resistance-held town and city. VILLAGES ARE NEVER INCLUDED - they fall as collateral and
	//! are not worth a campaign of their own, which is the requirements' own wording.
	RESISTANCE_TOWNS = 1,

	//! Every resistance-held military base. Forward bases and radio towers are NOT bases here: a
	//! forward base is the occupying faction's own, and a tower is handled WITHIN an objective.
	RESISTANCE_BASES = 2
}

//------------------------------------------------------------------------------------------------
//! EVERY PLACE THAT COULD BE THE NEXT OBJECTIVE THIS ROUND, WITH EVERY NUMBER A SELECTOR NEEDS
//! ALREADY RESOLVED.
//!
//! ==========================================================================================
//! ⚠ THE WORLD QUERIES HAPPEN HERE AND NOWHERE ELSE (D6, T3.2).
//! ==========================================================================================
//! This is the ONLY class in the objective machine that walks the town registry, walks the base
//! registry, asks the occupying faction manager for threat, measures reach from held ground or asks
//! which radio towers still broadcast. A selector receives numbers and does arithmetic; the pure
//! statics in OVT_ObjectiveSelection receive numbers and do arithmetic. That layering is what makes
//! N plans cost ONE pass over the world instead of N, and it is why the seven collection and
//! name-resolution methods that used to live on the director moved HERE rather than into the two
//! selectors: a town's name is resolved the same way whether a town plan or a base plan is asking
//! (a base with no marker name falls back to the nearest town's), so duplicating it per selector
//! would have been two copies of one guard, drifting apart.
//!
//! PARALLEL ARRAYS, ONE ENTRY PER CANDIDATE, AND THEY ARE NEVER RAGGED. Everything is appended
//! through Add(), which writes every array or none, so an index that is valid for one is valid for
//! all of them. The pure statics refuse ragged input outright and this is what guarantees they never
//! see any.
//!
//! ⚠ THE COLLECTION ORDER IS PART OF THE CONTRACT: EVERY TOWN, THEN EVERY BASE, each in its
//! registry's own order. Ties in every scorer break on the caller's input order, so this ordering IS
//! the tie-break of the whole selection - and it is the ordering the single-list selection used
//! before plans existed (towns were collected first there too). Changing it would change which of
//! two equally attractive places the occupying faction attacks, silently.
//!
//! ⚠ NOTHING IS CACHED ACROSS ROUNDS. A set is built, read and dropped inside one selection round.
//! Towns change hands, bases are lost and towers go off the air between one in-game minute and the
//! next, and a stale candidate would be committed to as though it were still there.
//------------------------------------------------------------------------------------------------
class OVT_ObjectiveCandidateSet : Managed
{
	//! Every kind in this set, as OVT_EObjectiveKind integers.
	protected ref array<int> m_aKinds;

	//! Where each candidate is. THE KEY of everything downstream - the blacklist, the commit and the
	//! save payload are all keyed on the position, never on an index into this set.
	protected ref array<vector> m_aPositions;

	//! Each candidate's display name, for logs and the Game Master panel. May be empty; an
	//! unresolvable name costs a log line its label and nothing else.
	protected ref array<string> m_aNames;

	//! Inhabitants. Zero for anything that is not a town.
	protected ref array<int> m_aPopulations;

	//! Support for the resistance, 0-100. Zero for anything that is not a town.
	protected ref array<int> m_aSupport;

	//! Campaign threat measured at the candidate. Zero for anything that is not a base.
	protected ref array<int> m_aThreat;

	//! How far the candidate is from the nearest base the occupying faction still holds. When the
	//! faction holds none this is the round's maximum useful distance, which scores the proximity term
	//! at zero rather than pretending the candidate is next door.
	protected ref array<float> m_aReach;

	//! Whether an occupying-held radio tower still broadcasts over the candidate.
	protected ref array<bool> m_aTowerCoverage;

	//! Whether each candidate is sitting out its blacklist round. All false until ApplyBlacklist() has
	//! been called, which is the honest answer for a set nobody has masked.
	protected ref array<bool> m_aBlacklisted;

	//! Distance at which the proximity term reaches zero for this round, in metres. The director's own
	//! setting; a selector may override it for itself, and this is what it falls back to.
	protected float m_fMaxUsefulDistance;

	//------------------------------------------------------------------------------------------------
	void OVT_ObjectiveCandidateSet()
	{
		m_aKinds = new array<int>();
		m_aPositions = new array<vector>();
		m_aNames = new array<string>();
		m_aPopulations = new array<int>();
		m_aSupport = new array<int>();
		m_aThreat = new array<int>();
		m_aReach = new array<float>();
		m_aTowerCoverage = new array<bool>();
		m_aBlacklisted = new array<bool>();
	}

	//------------------------------------------------------------------------------------------------
	// COLLECTION - the one pass over the world
	//------------------------------------------------------------------------------------------------

	//! Which candidate source a target kind belongs to.
	//! \param[in] kind An OVT_EObjectiveKind value.
	//! \return The matching source flag, or 0 for a kind no source produces.
	static int SourceForKind(int kind)
	{
		if (kind == OVT_EObjectiveKind.TOWN)
			return OVT_EObjectiveCandidateSource.RESISTANCE_TOWNS;

		if (kind == OVT_EObjectiveKind.BASE)
			return OVT_EObjectiveCandidateSource.RESISTANCE_BASES;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Walks the world once and fills the set with every candidate the requested sources produce.
	//!
	//! ⚠ TOWNS FIRST, THEN BASES, and see the class header for why that ordering is a contract rather
	//! than an implementation detail.
	//!
	//! ⚠ AN UNREQUESTED SOURCE IS NOT WALKED AT ALL. That is the entire economy of D6: a round in which
	//! no eligible plan can score a base never touches the base registry.
	//! \param[in] occupying The occupying faction manager, for bases, threat, reach and tower coverage.
	//! \param[in] towns The town manager. May be null, which simply produces no town candidates.
	//! \param[in] sources OVT_EObjectiveCandidateSource flags - the union of every eligible plan's.
	//! \param[in] resistanceIndex The resistance faction's index; only places it holds are candidates.
	//! \param[in] maxUsefulDistance Distance at which the proximity term reaches zero, and the reach
	//!            reported for a candidate when the occupying faction holds no base to measure from.
	//! \return True when at least one candidate was collected.
	bool Collect(notnull OVT_OccupyingFactionManager occupying, OVT_TownManagerComponent towns, int sources, int resistanceIndex, float maxUsefulDistance)
	{
		Clear();

		m_fMaxUsefulDistance = maxUsefulDistance;

		if ((sources & OVT_EObjectiveCandidateSource.RESISTANCE_TOWNS) != 0)
			AddResistanceTowns(occupying, towns, resistanceIndex);

		if ((sources & OVT_EObjectiveCandidateSource.RESISTANCE_BASES) != 0)
			AddResistanceBases(occupying, resistanceIndex);

		return !m_aKinds.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Adds every resistance-held town and city.
	//! \param[in] occupying The occupying faction manager, for the geometry inputs.
	//! \param[in] towns The town manager. Null produces nothing.
	//! \param[in] resistanceIndex The resistance faction's index.
	protected void AddResistanceTowns(notnull OVT_OccupyingFactionManager occupying, OVT_TownManagerComponent towns, int resistanceIndex)
	{
		if (!towns || !towns.m_Towns)
			return;

		foreach (OVT_TownData town : towns.m_Towns)
		{
			if (!town)
				continue;

			if (town.faction != resistanceIndex)
				continue;

			// Villages fall as collateral - they are never worth a campaign of their own.
			if (town.size == OVT_TownSize.VILLAGE)
				continue;

			// NO LAND ROUTE, NO CAMPAIGN. See AddResistanceBases() for why the gate is here.
			if (town.landIsolated)
				continue;

			float reach = ReachFromHeldGround(occupying, town.location);
			bool covered = IsUnderOccupyingBroadcast(occupying, town.location);

			Add(OVT_EObjectiveKind.TOWN, town.location, ResolveTownName(towns, town), town.population, town.SupportPercentage(), 0, reach, covered);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Adds every resistance-held base.
	//! \param[in] occupying The occupying faction manager.
	//! \param[in] resistanceIndex The resistance faction's index.
	protected void AddResistanceBases(notnull OVT_OccupyingFactionManager occupying, int resistanceIndex)
	{
		array<OVT_BaseData> bases = occupying.GetBasesControlledBy(resistanceIndex);
		if (!bases)
			return;

		foreach (OVT_BaseData base : bases)
		{
			if (!base)
				continue;

			// NO LAND ROUTE, NO CAMPAIGN.
			//
			// THE GATE IS IN THE CANDIDATE SET AND NOT IN A SCORER, DELIBERATELY. A score of zero does
			// not exclude anything - the director picks the best candidate it has, so on a quiet map a
			// zero-scored island is still the best thing going. More importantly a scorer gate would
			// have to be repeated in every selector, including ones a modder writes: the whole point of
			// the objectives registry is that doctrines are authored data, and "this target cannot be
			// walked to" is a fact about the MAP that no doctrine should be able to opt out of.
			//
			// ⚠ ONLY OBJECTIVE TARGETING IS GATED. The base still defends itself (its deployments are
			// created and spawned at the base, never sent to it), still fights a QRF if the resistance
			// starts one, and can still be captured. What it cannot be is the thing the faction marches
			// on - because everything downstream of that decision assumes a land route: the harassment
			// insertion strands its truck at the coast and the stranded-truck path then opens the doors
			// and marches the passengers on the objective, and the forward base is sited between the
			// nearest holding and the target, which across a strait is open water.
			if (base.landIsolated)
				continue;

			float reach = ReachFromHeldGround(occupying, base.location);
			bool covered = IsUnderOccupyingBroadcast(occupying, base.location);

			Add(OVT_EObjectiveKind.BASE, base.location, ResolveBaseName(occupying, base), 0, 0, occupying.GetThreatByLocation(base.location), reach, covered);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! How far a candidate is from the nearest base the occupying faction still holds.
	//! \param[in] occupying The occupying faction manager.
	//! \param[in] position The candidate's position.
	//! \return The distance, or the round's maximum useful distance when the faction holds no base at
	//! all - which scores the proximity term at zero rather than pretending the candidate is next door.
	protected float ReachFromHeldGround(notnull OVT_OccupyingFactionManager occupying, vector position)
	{
		OVT_BaseData nearest = occupying.GetNearestOccupiedBase(position);
		if (!nearest)
			return m_fMaxUsefulDistance;

		return vector.Distance(nearest.location, position);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether an occupying-held radio tower still broadcasts over a candidate.
	//! \param[in] occupying The occupying faction manager.
	//! \param[in] position The candidate's position.
	//! \return True when at least one tower in range is on the air and occupying-held.
	protected bool IsUnderOccupyingBroadcast(notnull OVT_OccupyingFactionManager occupying, vector position)
	{
		array<OVT_RadioTowerData> towers = occupying.GetRadioTowersAffecting(position);
		if (!towers)
			return false;

		foreach (OVT_RadioTowerData tower : towers)
		{
			if (tower && tower.IsOccupyingFaction())
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	// NAMES - resolved once here, for every consumer
	//------------------------------------------------------------------------------------------------

	//! A base's display name, falling back to the nearest town's when the marker carries none.
	//! \param[in] occupying The occupying faction manager.
	//! \param[in] base The base record.
	//! \return A name for logs and for the Game Master panel. May be empty.
	static string ResolveBaseName(notnull OVT_OccupyingFactionManager occupying, notnull OVT_BaseData base)
	{
		// Resolved here rather than through the manager's own GetBase(), which dereferences the marker
		// without checking it is still there - the same hazard its serializer documents avoiding.
		IEntity marker = GetGame().GetWorld().FindEntityByID(base.entId);
		if (marker)
		{
			OVT_BaseControllerComponent controller = OVT_BaseControllerComponent.Cast(marker.FindComponent(OVT_BaseControllerComponent));
			if (controller && controller.m_sName != "")
				return controller.m_sName;
		}

		return ResolveTownNameAt(OVT_Global.GetTowns(), base.location);
	}

	//------------------------------------------------------------------------------------------------
	//! A town's display name, resolved SAFELY.
	//!
	//! ⚠ THE GUARD IS NOT DEFENSIVE PROGRAMMING FOR ITS OWN SAKE. GetTownName() dereferences the town's
	//! map marker's item without checking the marker is there, and it is called here once per candidate
	//! per selection - which is every in-game minute, forever, on the server. A town with no marker
	//! within five metres (a hand-defined town, a world layer that moved one) would take the whole
	//! campaign down. The marker is therefore checked first, and only for names that are not already
	//! cached; an unresolvable name is empty, which costs a log line its label and nothing else.
	//! \param[in] towns The town manager. Null yields an empty name.
	//! \param[in] town The town. Null yields an empty name.
	//! \return The name, or an empty string.
	static string ResolveTownName(OVT_TownManagerComponent towns, OVT_TownData town)
	{
		if (!towns || !town)
			return "";

		int townId = towns.GetTownID(town);
		if (townId < 0)
			return "";

		if (towns.m_TownNames && townId < towns.m_TownNames.Count() && towns.m_TownNames[townId] != "")
			return towns.m_TownNames[townId];

		if (!towns.GetNearestTownMarker(town.location))
			return "";

		return towns.GetTownName(townId);
	}

	//------------------------------------------------------------------------------------------------
	//! The name of the town nearest a position, resolved safely.
	//! \param[in] towns The town manager. Null yields an empty name.
	//! \param[in] position The position to resolve near.
	//! \return The name, or an empty string.
	static string ResolveTownNameAt(OVT_TownManagerComponent towns, vector position)
	{
		if (!towns)
			return "";

		return ResolveTownName(towns, towns.GetNearestTown(position));
	}

	//------------------------------------------------------------------------------------------------
	// THE BLACKLIST MASK
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Marks every candidate that is sitting out its blacklist round.
	//!
	//! ⚠ ONCE PER ROUND, FOR EVERY PLAN. The blacklist is a fact about PLACES, not about plans, so it
	//! is folded in here rather than per selector - which also means one plan cannot see a place a
	//! rival plan has been told to leave alone.
	//! \param[in] keys Blacklisted position keys.
	//! \param[in] roundsLeft Rounds remaining per key, same order, same length.
	void ApplyBlacklist(array<string> keys, array<int> roundsLeft)
	{
		m_aBlacklisted.Clear();

		int count = m_aPositions.Count();
		for (int i = 0; i < count; i++)
		{
			m_aBlacklisted.Insert(OVT_ObjectiveSelection.IsBlacklisted(keys, roundsLeft, OVT_ObjectiveSelection.PositionKey(m_aPositions[i])));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The mask one selector's pick must be taken through: everything blacklisted, plus everything
	//! whose kind that selector does not claim.
	//!
	//! ⚠ THIS IS WHAT KEEPS "SCORED ZERO" AND "NOT A CANDIDATE" APART. Every selector writes a score
	//! for every index so the arrays stay parallel, and zero is a perfectly legal score; without this
	//! mask a town selector's zero for a base would be selectable, and the town plan could commit to a
	//! base it has no doctrine for.
	//! \param[in] sources The selector's OVT_EObjectiveCandidateSource flags.
	//! \param[out] mask Receives one flag per candidate - true means "may not be picked". Cleared first.
	void BuildSelectionMask(int sources, notnull array<bool> mask)
	{
		mask.Clear();

		int count = m_aKinds.Count();
		for (int i = 0; i < count; i++)
		{
			bool blocked = false;

			if (i < m_aBlacklisted.Count() && m_aBlacklisted[i])
				blocked = true;

			if ((sources & SourceForKind(m_aKinds[i])) == 0)
				blocked = true;

			mask.Insert(blocked);
		}
	}

	//------------------------------------------------------------------------------------------------
	// READ AND WRITE
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Appends one candidate, writing every parallel array.
	//!
	//! ⚠ EVERY ARRAY OR NONE. This is the only writer, which is what makes "an index valid for one is
	//! valid for all of them" true by construction rather than by discipline.
	//! \param[in] kind An OVT_EObjectiveKind value.
	//! \param[in] position Where the candidate is.
	//! \param[in] name Its display name. May be empty.
	//! \param[in] population Inhabitants, or 0 for anything that is not a town.
	//! \param[in] supportPercentage Support for the resistance 0-100, or 0 for anything that is not a town.
	//! \param[in] threat Campaign threat, or 0 for anything that is not a base.
	//! \param[in] reach Distance to the nearest base the occupying faction still holds.
	//! \param[in] towerCoverage Whether an occupying broadcast still reaches it.
	void Add(int kind, vector position, string name, int population, int supportPercentage, int threat, float reach, bool towerCoverage)
	{
		m_aKinds.Insert(kind);
		m_aPositions.Insert(position);
		m_aNames.Insert(name);
		m_aPopulations.Insert(population);
		m_aSupport.Insert(supportPercentage);
		m_aThreat.Insert(threat);
		m_aReach.Insert(reach);
		m_aTowerCoverage.Insert(towerCoverage);
		m_aBlacklisted.Insert(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Empties every array. The set is built, read and dropped inside one selection round.
	void Clear()
	{
		m_aKinds.Clear();
		m_aPositions.Clear();
		m_aNames.Clear();
		m_aPopulations.Clear();
		m_aSupport.Clear();
		m_aThreat.Clear();
		m_aReach.Clear();
		m_aTowerCoverage.Clear();
		m_aBlacklisted.Clear();
	}

	//! \return How many candidates the round produced.
	int Count()
	{
		return m_aKinds.Count();
	}

	//! \return True when the index addresses a candidate.
	bool IsValidIndex(int index)
	{
		return index >= 0 && index < m_aKinds.Count();
	}

	//! \return One candidate's OVT_EObjectiveKind, or NONE when the index is out of range.
	int GetKind(int index)
	{
		if (!IsValidIndex(index))
			return OVT_EObjectiveKind.NONE;

		return m_aKinds[index];
	}

	//! \return Where a candidate is, or the zero vector when the index is out of range.
	vector GetPosition(int index)
	{
		if (!IsValidIndex(index))
			return vector.Zero;

		return m_aPositions[index];
	}

	//! \return A candidate's display name, or an empty string when the index is out of range.
	string GetName(int index)
	{
		if (!IsValidIndex(index))
			return "";

		return m_aNames[index];
	}

	//! \return A candidate's population, or 0 when the index is out of range.
	int GetPopulation(int index)
	{
		if (!IsValidIndex(index))
			return 0;

		return m_aPopulations[index];
	}

	//! \return A candidate's support percentage, or 0 when the index is out of range.
	int GetSupportPercentage(int index)
	{
		if (!IsValidIndex(index))
			return 0;

		return m_aSupport[index];
	}

	//! \return A candidate's threat, or 0 when the index is out of range.
	int GetThreat(int index)
	{
		if (!IsValidIndex(index))
			return 0;

		return m_aThreat[index];
	}

	//! \return A candidate's distance from the nearest base the occupying faction holds, or 0.
	float GetReach(int index)
	{
		if (!IsValidIndex(index))
			return 0;

		return m_aReach[index];
	}

	//! \return Whether an occupying broadcast reaches a candidate. False when the index is out of range.
	bool HasTowerCoverage(int index)
	{
		if (!IsValidIndex(index))
			return false;

		return m_aTowerCoverage[index];
	}

	//! \return Whether a candidate is sitting out its blacklist round. False before ApplyBlacklist().
	bool IsBlacklisted(int index)
	{
		if (index < 0 || index >= m_aBlacklisted.Count())
			return false;

		return m_aBlacklisted[index];
	}

	//! \return The round's maximum useful distance, which a selector that authors none falls back to.
	float GetMaxUsefulDistance()
	{
		return m_fMaxUsefulDistance;
	}

	//! Sets the round's maximum useful distance. Collect() does this; a hand-built fixture set needs it.
	//! \param[in] distance Distance at which the proximity term reaches zero, in metres.
	void SetMaxUsefulDistance(float distance)
	{
		m_fMaxUsefulDistance = distance;
	}
}
