//------------------------------------------------------------------------------------------------
//! Shared subject resolution for the two persistence tiers (Tier D green + Tier D' quarantined).
//!
//! A persistence case needs a *subject* before it can round-trip anything: which player, which
//! town, which building. Resolving those is identical work in both suites and is deliberately kept
//! out of the cases so that a case body reads as "mutate -> read back -> assert" and nothing else.
//!
//! THE ASSERTION RULE APPLIES HERE TOO (implementation.md Phase 4, Decision 4):
//! nothing in this file may name a persistence implementation type of any flavour. Subjects are
//! resolved exclusively through Overthrow's public manager API, so this file survives the
//! vanilla-persistence migration unchanged.
//!
//! Every resolver follows the same contract: it returns the resolved subject, and writes a
//! human-readable reason into `diagnostic` when it could not resolve one. A case that gets an empty
//! subject fails with that reason verbatim - never with a null dereference, which would show up in
//! junit.xml as nothing at all.
//!
//! No resolver polls. Ordering is settled empirically: the local player is registered with the
//! player manager ~18 ms BEFORE the suite's campaign-start Setup step runs (Phase 3 run log,
//! `Setting up player: <uid> with playerId: 1` at 01:42:53.577 vs the start step at 01:42:53.595),
//! so by the time any Main step executes the mappings exist. If that ever stops being true the
//! failure is a named diagnostic, not a flake.
//------------------------------------------------------------------------------------------------
class OVT_TEST_PersistenceSubject
{
	//------------------------------------------------------------------------------------------------
	//! Resolves the runtime player ID of the client running the tests.
	//!
	//! The autotest client spawns a real local player (findings.md 1.4: `DoSpawn_S called for
	//! playerId: 1`), which is what makes player-scoped persistence testable at all. The connected
	//! player list is used as a fallback so this does not depend on the local-controller path.
	//! \param[out] diagnostic Reason the ID could not be resolved; untouched on success.
	//! \return The runtime player ID, or -1 when there is no player.
	static int ResolveLocalPlayerId(out string diagnostic)
	{
		int playerId = SCR_PlayerController.GetLocalPlayerId();
		if (playerId > 0)
			return playerId;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
		{
			diagnostic = "No PlayerManager in the running game - persistence cases need a player to own state";
			return -1;
		}

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		if (playerIds.IsEmpty())
		{
			diagnostic = "No player is connected - the autotest client normally spawns a local player (playerId 1)";
			return -1;
		}

		return playerIds[0];
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the persistent ID of the local player through the player manager.
	//!
	//! The persistent ID - not the runtime ID - is the key every persisted player record is stored
	//! under, so it is the identifier a persistence assertion must use.
	//! \param[out] diagnostic Reason the ID could not be resolved; untouched on success.
	//! \return The persistent ID, or an empty string when it could not be resolved.
	static string ResolveLocalPersistentId(out string diagnostic)
	{
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
		{
			diagnostic = "OVT_Global.GetPlayers() is null - no player manager on the game mode";
			return "";
		}

		int playerId = ResolveLocalPlayerId(diagnostic);
		if (playerId < 1)
			return "";

		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if (persId == "")
		{
			diagnostic = "The player manager has no persistent ID for playerId " + playerId.ToString() + " - the player was never set up";
			return "";
		}

		return persId;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the first registered town and its ID.
	//!
	//! The test world defines exactly ONE town, so "first" is unambiguous here; the ID is read back
	//! through GetTownID() rather than assumed to be 0, because the ID is the key the manager's own
	//! mutators take.
	//! \param[out] townId The town's ID as the manager reports it; -1 when unresolved.
	//! \param[out] diagnostic Reason the town could not be resolved; untouched on success.
	//! \return The town record, or null.
	static OVT_TownData ResolveFirstTown(out int townId, out string diagnostic)
	{
		townId = -1;

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			diagnostic = "OVT_Global.GetTowns() is null - no town manager on the game mode";
			return null;
		}

		array<ref OVT_TownData> townList = towns.GetTowns();
		if (!townList || townList.Count() < 1)
		{
			diagnostic = "No towns are registered - the town manager found none during Init";
			return null;
		}

		OVT_TownData town = townList[0];
		if (!town)
		{
			diagnostic = "Town 0 is null";
			return null;
		}

		townId = towns.GetTownID(town);
		if (townId < 0)
		{
			diagnostic = "GetTownID() does not recognise the town it just handed out";
			return null;
		}

		return town;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves a world entity to hang a recruit record on.
	//!
	//! OVT_RecruitManagerComponent.AddRecruit() takes an IEntity because in the game a recruit is
	//! created from a civilian standing in front of the player. A test must not spawn AI - the test
	//! world has no navmesh (findings.md, inherited caveat) - and must not hijack the local player's
	//! own character, so the registered town controller entity is used instead: AddRecruit only
	//! reads GetOrigin() / GetID() / FindComponent() off it, and the town controller sits at the
	//! town centre, which also gives the recruit a real hometown to resolve.
	//! \param[out] diagnostic Reason no entity could be resolved; untouched on success.
	//! \return The entity, or null.
	static IEntity ResolveRecruitSubjectEntity(out string diagnostic)
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			diagnostic = "OVT_Global.GetTowns() is null - no town manager on the game mode";
			return null;
		}

		if (towns.m_TownControllers.Count() < 1)
		{
			diagnostic = "No town controllers are registered - nothing to attach a recruit record to";
			return null;
		}

		IEntity entity = GetGame().GetWorld().FindEntityByID(towns.m_TownControllers[0]);
		if (!entity)
		{
			diagnostic = "Town controller 0 is registered but its entity ID no longer resolves in the world";
			return null;
		}

		return entity;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves an ownable building that nobody currently owns.
	//!
	//! Uses the town manager's own unowned-house query, which is what the game uses when it needs a
	//! house to hand out. Picking an *unowned* one matters: the campaign start already gives the
	//! local player a starting home, and which house that is, is randomised - a test that grabbed the
	//! nearest building to the town centre would collide with it on some runs and not others.
	//! \param[out] diagnostic Reason no building could be resolved; untouched on success.
	//! \return An unowned ownable building, or null.
	static IEntity ResolveUnownedBuilding(out string diagnostic)
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			diagnostic = "OVT_Global.GetTowns() is null - no town manager on the game mode";
			return null;
		}

		IEntity building = towns.GetRandomUnownedHouse();
		if (!building)
		{
			diagnostic = "GetRandomUnownedHouse() found no unowned ownable building in any town";
			return null;
		}

		return building;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the key of a skill that has at least one configured level.
	//!
	//! Read from the skill config rather than hardcoded, so renaming or reordering skills changes
	//! which skill is exercised instead of turning the case red for the wrong reason.
	//! AddSkillLevel() indexes m_aLevels[newLevel - 1], so a skill with no levels would be an engine
	//! exception rather than a test failure - hence the level count check.
	//! \param[out] diagnostic Reason no usable skill was found; untouched on success.
	//! \return The skill key, or an empty string.
	static string ResolveFirstSkillKey(out string diagnostic)
	{
		OVT_SkillManagerComponent skills = OVT_Global.GetSkills();
		if (!skills)
		{
			diagnostic = "OVT_Global.GetSkills() is null - no skill manager on the game mode";
			return "";
		}

		if (!skills.m_Skills || !skills.m_Skills.m_aSkills)
		{
			diagnostic = "The skill manager has no skill config";
			return "";
		}

		foreach (OVT_SkillConfig skill : skills.m_Skills.m_aSkills)
		{
			if (!skill)
				continue;

			if (skill.m_sKey == "")
				continue;

			if (!skill.m_aLevels || skill.m_aLevels.Count() < 1)
				continue;

			return skill.m_sKey;
		}

		diagnostic = "No configured skill has at least one level";
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Reads a player's current skill level through the player manager.
	//! \param[in] persId Persistent ID of the player.
	//! \param[in] skillKey Skill to read.
	//! \return The level, or 0 when the player has never taken that skill.
	static int GetPlayerSkillLevel(string persId, string skillKey)
	{
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
			return 0;

		OVT_PlayerData player = players.GetPlayer(persId);
		if (!player || !player.skills)
			return 0;

		if (!player.skills.Contains(skillKey))
			return 0;

		return player.skills[skillKey];
	}
}
