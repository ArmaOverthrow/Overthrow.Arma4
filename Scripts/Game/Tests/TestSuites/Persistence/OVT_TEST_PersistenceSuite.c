//------------------------------------------------------------------------------------------------
//! TIER D - persistence, same-session behaviour. GREEN, and part of the All group.
//!
//! ===========================================================================================
//! ASSERTION RULE, NON-NEGOTIABLE (implementation.md Phase 4, Decision 4).
//!
//! Quoted below with the three type-name tokens replaced by descriptions - DELIBERATELY, because
//! the rule is enforced by grepping this whole tree for exactly those tokens, and a comment that
//! quoted them would trip the very check it is describing. The verbatim wording, and the exact
//! grep, are in implementation.md under Decision 4.
//!
//!   "no persistence-framework type, no vanilla persistence type, and no Overthrow save-data
//!    class may appear anywhere in these files except the single documented save-trigger call.
//!    Every assertion reads state back through the same public manager API that wrote it. A
//!    reviewer must be able to grep this tree for those type names and find at most the one
//!    annotated trigger line."
//!
//! That rule is the whole point of this tier. A test that names a storage type reports the
//! EPF -> vanilla migration as a regression *by construction*, which inverts its purpose. The
//! contract these cases encode is the real one: the migration may change HOW state is stored and
//! may not change WHAT survives. Nothing in THIS file triggers a save at all - the one permitted
//! trigger lives in OVT_TEST_PersistenceRoundTripSuite.c and is annotated there.
//! ===========================================================================================
//!
//! What this suite proves TODAY: state written through Overthrow's public manager API is readable
//! back through Overthrow's public manager API, in the same session. That is not a consolation
//! prize for the missing save path - it independently catches the "setting town control doesn't
//! stick", "money mutator writes the wrong record", "removing an owner leaves it owned" class of
//! regression, which is a real class and is currently uncovered.
//!
//! What it does NOT prove: that anything survives a save and a reload. There is no save path on
//! this branch in either persistence system (findings.md 1.7), so that half lives in the
//! quarantined OVT_TEST_PersistenceRoundTripSuite, whose exit code is the migration's acceptance
//! criterion. Do not add save/reload assertions here.
//!
//! ---------------------------------------------------------------------------------------------
//! STATE KINDS COVERED (one case each; alphabetical, which is also the execution order)
//!   player money            AddPlayerMoney / TakePlayerMoney      -> GetPlayerMoney / PlayerHasMoney
//!   player skills and XP    GiveXP / TakeXP / AddSkillLevel       -> player record via the player manager
//!   real estate ownership   SetOwnerPersistentId / SetOwner(-1)   -> GetOwnerID / IsOwner / IsOwned
//!   recruits                AddRecruit / AddRecruitXP / Remove    -> GetRecruit / GetRecruitCount
//!   town control            ChangeTownControl                     -> GetTown().faction / IsOccupyingFaction
//!   town population         TakeSupportersFromNearestTown         -> GetTown().population
//!   town stability          TryAdd/RemoveStabilityModifier        -> GetTown().stability / GetTowns()
//!   town support            AddSupport / ResetSupport             -> GetTown().support
//!
//! STATE KINDS DELIBERATELY DEFERRED, with cause (task 4.3). These are the growth path, not this
//! feature's spine, and each is deferred for a reason that is visible in the source:
//!   - VEHICLES and PLACED STRUCTURES persist through per-entity components on spawned prefabs
//!     rather than through a manager, so a behaviour-level round-trip would have to spawn and
//!     despawn entities in a world with no navmesh. Out of scope here.
//!   - CONTAINER INVENTORIES are not saved at all: the inventory save layer's read/apply methods
//!     are empty stubs on this branch. There is nothing to round-trip and asserting on it would
//!     produce a test that cannot fail.
//!   - CHARACTER HELD ITEMS are deliberately disabled in the character controller's save layer,
//!     pending an upstream bug. Covering them would pin a switched-off feature.
//!   - LOADOUTS live in a separate scripted-state path whose repository methods are unimplemented
//!     stubs, so the manager has nothing to hand back.
//!   - GARRISONS are never populated in the test world (findings.md 1.4) - assertions on them
//!     would be red on arrival and would say nothing about Overthrow.
//!
//! ---------------------------------------------------------------------------------------------
//! RUN RECIPE
//!   .scripts/reset_save.sh --profile OverthrowCI      # fresh campaign (see tools/README.md)
//!   tools/run-tests.sh OVT_TEST_PersistenceSuite      # expect exit 0
//! The reset is inert today (there is no save DB to delete) and becomes load-bearing the moment
//! persistence works, which is exactly why it is wired now rather than retrofitted.
//!
//! ---------------------------------------------------------------------------------------------
//! HOUSE RULES FOR THIS TIER
//!  - Cases run alphabetically by class name and MUST be independent: no case may depend on
//!    another having run, and every case restores what it can through the same public API.
//!  - Every case must finish promptly. The town manager's modifier tick (MODIFIER_FREQUENCY,
//!    10 s) starts at campaign start and recalculates support and population growth; a case that
//!    polled for longer than that would race it. Nothing here polls - every mutator used is
//!    synchronous on the server.
//!  - Deltas, never absolutes: read the current value, mutate by a known amount, assert the
//!    delta. Hardcoding "money is 100000" would couple this suite to the difficulty config.
//!  - [BaseContainerProps()] is MANDATORY on a concrete suite class, or a group config silently
//!    instantiates nothing (findings.md 1.10).
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_TEST_PersistenceSuite : OVT_TEST_SuiteBase
{
	//------------------------------------------------------------------------------------------------
	//! Tier D needs a running campaign: player records, town control and real estate ownership are
	//! all campaign-start products. The start sequence itself lives in OVT_TEST_SuiteBase.
	//! \return Always true for this suite.
	override bool RequiresStartedCampaign()
	{
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Player money survives a write and a read through the economy manager.
//!
//! Covers the seam a persisted player wallet actually goes through: AddPlayerMoney takes a RUNTIME
//! player ID and GetPlayerMoney takes a PERSISTENT ID, so this case also proves the two identifier
//! spaces still resolve to the same record - the single most likely thing to break in a migration
//! that changes how player records are keyed.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceSuite, timeoutS: 30)]
class OVT_TEST_Persistence_PlayerMoney_RoundTrips : SCR_AutotestCaseBase
{
	//! Deliberately not a round number a stray income tick could coincidentally produce.
	static const int ADD_AMOUNT = 12345;

	//! Taken back out again, smaller than ADD_AMOUNT so the balance never clamps at zero.
	static const int TAKE_AMOUNT = 5000;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
		{
			SetResultFailure("OVT_Global.GetEconomy() is null");
			return true;
		}

		string diagnostic;
		string persId = OVT_TEST_PersistenceSubject.ResolveLocalPersistentId(diagnostic);
		if (persId == "")
		{
			SetResultFailure("Cannot resolve the persistent player ID: %1", diagnostic);
			return true;
		}

		int playerId = OVT_TEST_PersistenceSubject.ResolveLocalPlayerId(diagnostic);
		if (playerId < 1)
		{
			SetResultFailure("Cannot resolve the runtime player ID: %1", diagnostic);
			return true;
		}

		int before = economy.GetPlayerMoney(persId);

		// Write through the runtime-ID mutator, read back through the persistent-ID accessor.
		economy.AddPlayerMoney(playerId, ADD_AMOUNT);
		int afterAdd = economy.GetPlayerMoney(persId);
		if (afterAdd != before + ADD_AMOUNT)
		{
			SetResultFailure("AddPlayerMoney(%1) did not stick: money was %2, is now %3",
				ADD_AMOUNT.ToString(), before.ToString(), afterAdd.ToString());
			return true;
		}

		if (!economy.PlayerHasMoney(persId, afterAdd))
		{
			SetResultFailure("PlayerHasMoney() disagrees with GetPlayerMoney() at %1", afterAdd.ToString());
			return true;
		}

		economy.TakePlayerMoney(playerId, TAKE_AMOUNT);
		int afterTake = economy.GetPlayerMoney(persId);
		if (afterTake != afterAdd - TAKE_AMOUNT)
		{
			SetResultFailure("TakePlayerMoney(%1) did not stick: money was %2, is now %3",
				TAKE_AMOUNT.ToString(), afterAdd.ToString(), afterTake.ToString());
			return true;
		}

		// Restore, and let the restoration double as an independent claim about the arithmetic.
		economy.TakePlayerMoney(playerId, ADD_AMOUNT - TAKE_AMOUNT);
		int restored = economy.GetPlayerMoney(persId);
		if (restored != before)
		{
			SetResultFailure("Money did not return to its starting value: expected %1, got %2",
				before.ToString(), restored.ToString());
			return true;
		}

		PrintFormat("Player money round-trip: %1 -> %2 -> %3", before.ToString(), afterAdd.ToString(), restored.ToString());
		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Player XP and skill levels survive a write and a read through the skill manager.
//!
//! XP and skill levels are two separate persisted fields on the same record and they are written by
//! two different seams, so both are exercised. The skill key comes from the skill config, not from
//! a literal, so renaming a skill changes which one is tested instead of turning this red.
//!
//! Deliberately NOT asserted: the skill EFFECT fields (price/stealth multipliers, permissions).
//! They are recomputed from the skill levels when a player record is loaded, are marked as not
//! serialized, and therefore are not persisted state. Asserting them here would test Tier A logic
//! from the most expensive tier.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceSuite, timeoutS: 30)]
class OVT_TEST_Persistence_PlayerSkills_RoundTrip : SCR_AutotestCaseBase
{
	//! Enough XP to move the level, small enough to stay inside the first few levels.
	static const int XP_AMOUNT = 400;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_SkillManagerComponent skills = OVT_Global.GetSkills();
		if (!skills)
		{
			SetResultFailure("OVT_Global.GetSkills() is null");
			return true;
		}

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
		{
			SetResultFailure("OVT_Global.GetPlayers() is null");
			return true;
		}

		string diagnostic;
		string persId = OVT_TEST_PersistenceSubject.ResolveLocalPersistentId(diagnostic);
		if (persId == "")
		{
			SetResultFailure("Cannot resolve the persistent player ID: %1", diagnostic);
			return true;
		}

		int playerId = OVT_TEST_PersistenceSubject.ResolveLocalPlayerId(diagnostic);
		if (playerId < 1)
		{
			SetResultFailure("Cannot resolve the runtime player ID: %1", diagnostic);
			return true;
		}

		OVT_PlayerData player = players.GetPlayer(persId);
		if (!player)
		{
			SetResultFailure("The player manager has no record for persistent ID %1", persId);
			return true;
		}

		// --- XP ---
		int xpBefore = player.xp;
		int levelBefore = player.GetLevel();

		skills.GiveXP(playerId, XP_AMOUNT);

		// Re-read through the manager rather than reusing the reference: this is what a persistence
		// round-trip does, and it proves the manager is still handing out the record that was written.
		OVT_PlayerData afterGive = players.GetPlayer(persId);
		if (!afterGive)
		{
			SetResultFailure("The player manager lost the record for persistent ID %1 after GiveXP()", persId);
			return true;
		}

		if (afterGive.xp != xpBefore + XP_AMOUNT)
		{
			SetResultFailure("GiveXP(%1) did not stick: xp was %2, is now %3",
				XP_AMOUNT.ToString(), xpBefore.ToString(), afterGive.xp.ToString());
			return true;
		}

		int levelAfter = afterGive.GetLevel();
		if (levelAfter < levelBefore)
		{
			SetResultFailure("Level went backwards after gaining XP: was %1, is now %2",
				levelBefore.ToString(), levelAfter.ToString());
			return true;
		}

		skills.TakeXP(playerId, XP_AMOUNT);

		OVT_PlayerData afterTake = players.GetPlayer(persId);
		if (!afterTake)
		{
			SetResultFailure("The player manager lost the record for persistent ID %1 after TakeXP()", persId);
			return true;
		}

		int xpRestored = afterTake.xp;
		if (xpRestored != xpBefore)
		{
			SetResultFailure("TakeXP(%1) did not return xp to its starting value: expected %2, got %3",
				XP_AMOUNT.ToString(), xpBefore.ToString(), xpRestored.ToString());
			return true;
		}

		// --- Skill levels ---
		string skillKey = OVT_TEST_PersistenceSubject.ResolveFirstSkillKey(diagnostic);
		if (skillKey == "")
		{
			SetResultFailure("Cannot resolve a skill to exercise: %1", diagnostic);
			return true;
		}

		OVT_PlayerData beforeSkill = players.GetPlayer(persId);
		if (!beforeSkill)
		{
			SetResultFailure("The player manager lost the record for persistent ID %1 before the skill mutation", persId);
			return true;
		}

		int skillBefore = OVT_TEST_PersistenceSubject.GetPlayerSkillLevel(persId, skillKey);
		int countBefore = beforeSkill.CountSkills();

		// AddSkillLevel validates affordability server-side (BUG-032), so the record must
		// hold an unspent skill point before the purchase. Earn one if needed and hand the
		// XP back afterwards - the round-trip suite asserts this record's exact xp value.
		int earnedXP = 0;
		int pointsAvailable = (beforeSkill.GetLevel() - 1) - countBefore;
		if (pointsAvailable < 1)
		{
			// +1 covers float truncation in the GetLevelXP threshold maths
			earnedXP = beforeSkill.GetLevelXP(countBefore + 1) - beforeSkill.xp + 1;
			skills.GiveXP(playerId, earnedXP);
		}

		skills.AddSkillLevel(playerId, skillKey);

		int skillAfter = OVT_TEST_PersistenceSubject.GetPlayerSkillLevel(persId, skillKey);
		if (skillAfter != skillBefore + 1)
		{
			SetResultFailure("AddSkillLevel('%1') did not stick: level was %2, is now %3",
				skillKey, skillBefore.ToString(), skillAfter.ToString());
			return true;
		}

		OVT_PlayerData afterSkill = players.GetPlayer(persId);
		if (!afterSkill)
		{
			SetResultFailure("The player manager lost the record for persistent ID %1 after AddSkillLevel()", persId);
			return true;
		}

		int countAfter = afterSkill.CountSkills();
		if (countAfter != countBefore + 1)
		{
			SetResultFailure("CountSkills() did not follow the added level: was %1, is now %2",
				countBefore.ToString(), countAfter.ToString());
			return true;
		}

		if (earnedXP > 0)
			skills.TakeXP(playerId, earnedXP);

		// The skill manager exposes no level-removal seam, so this one mutation is left in place;
		// no other case reads skills and the session ends seconds later.
		// PrintFormat on a case takes at most THREE string params - the fourth positional is a
		// LogLevel (findings.md, Phase 3 observations). Split the message rather than widen the call.
		PrintFormat("Player skills round-trip: xp %1 (+%2 and back)", xpRestored.ToString(), XP_AMOUNT.ToString());
		PrintFormat("Player skills round-trip: skill '%1' level %2", skillKey, skillAfter.ToString());
		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Building ownership survives a write and a read through the real estate manager.
//!
//! Ownership is stored against the building's POSITION and keyed by the owner's persistent ID, and
//! it is read back through three different accessors that each derive from a different map
//! (GetOwnerID from the position map, IsOwner from the per-owner list, IsOwned by scanning). All
//! three must agree, before and after removal - a migration that rebuilt one map and not the others
//! would pass a single-accessor test and fail this one.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceSuite, timeoutS: 30)]
class OVT_TEST_Persistence_RealEstateOwnership_RoundTrips : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if (!realEstate)
		{
			SetResultFailure("OVT_Global.GetRealEstate() is null");
			return true;
		}

		string diagnostic;
		string persId = OVT_TEST_PersistenceSubject.ResolveLocalPersistentId(diagnostic);
		if (persId == "")
		{
			SetResultFailure("Cannot resolve the persistent player ID: %1", diagnostic);
			return true;
		}

		IEntity building = OVT_TEST_PersistenceSubject.ResolveUnownedBuilding(diagnostic);
		if (!building)
		{
			SetResultFailure("Cannot resolve a building to own: %1", diagnostic);
			return true;
		}

		if (realEstate.GetOwnerID(building) != "")
		{
			SetResultFailure("The building picked as an unowned subject is already owned by '%1'",
				realEstate.GetOwnerID(building));
			return true;
		}

		realEstate.SetOwnerPersistentId(persId, building);

		string ownerAfterSet = realEstate.GetOwnerID(building);
		if (ownerAfterSet != persId)
		{
			SetResultFailure("SetOwnerPersistentId() did not stick: GetOwnerID() returned '%1', expected '%2'",
				ownerAfterSet, persId);
			return true;
		}

		if (!realEstate.IsOwner(persId, building.GetID()))
		{
			SetResultFailure("IsOwner() says the player does not own the building GetOwnerID() says they own");
			return true;
		}

		if (!realEstate.IsOwned(building.GetID()))
		{
			SetResultFailure("IsOwned() says the building is unowned immediately after it was given an owner");
			return true;
		}

		// -1 is the documented "remove ownership" form of the same mutator.
		realEstate.SetOwner(-1, building);

		string ownerAfterRemove = realEstate.GetOwnerID(building);
		if (ownerAfterRemove != "")
		{
			SetResultFailure("Removing the owner did not stick: GetOwnerID() still returns '%1'", ownerAfterRemove);
			return true;
		}

		if (realEstate.IsOwner(persId, building.GetID()))
		{
			SetResultFailure("IsOwner() still reports ownership after the owner was removed");
			return true;
		}

		PrintFormat("Real estate ownership round-trip at %1 for '%2'",
			building.GetOrigin().ToString(), persId);
		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Recruit records survive creation, an XP award and removal through the recruit manager.
//!
//! Recruits are the one persisted state kind that is keyed by its own generated ID rather than by a
//! player or a position, so this case checks that the generated ID resolves back to the record, that
//! the record is attributed to the right owner, and that removal is complete in both the by-ID and
//! by-owner views.
//!
//! The recruit is created against the town controller entity - see
//! OVT_TEST_PersistenceSubject.ResolveRecruitSubjectEntity() for why a test must not spawn a
//! character here. The manager logs a warning about the entity lacking a persistence component; that
//! warning comes from gameplay code, is expected, and is not a failure.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceSuite, timeoutS: 30)]
class OVT_TEST_Persistence_Recruits_RoundTrip : SCR_AutotestCaseBase
{
	//! Explicit name, so the case does not depend on the random name generator.
	static const string RECRUIT_NAME = "Autotest Recruit";

	//! XP award used to prove the recruit's own progression is stored on the record.
	static const int RECRUIT_XP = 400;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if (!recruits)
		{
			SetResultFailure("OVT_Global.GetRecruits() is null");
			return true;
		}

		string diagnostic;
		string persId = OVT_TEST_PersistenceSubject.ResolveLocalPersistentId(diagnostic);
		if (persId == "")
		{
			SetResultFailure("Cannot resolve the persistent player ID: %1", diagnostic);
			return true;
		}

		IEntity subject = OVT_TEST_PersistenceSubject.ResolveRecruitSubjectEntity(diagnostic);
		if (!subject)
		{
			SetResultFailure("Cannot resolve an entity to attach a recruit to: %1", diagnostic);
			return true;
		}

		int countBefore = recruits.GetRecruitCount(persId);
		if (!recruits.CanRecruit(persId))
		{
			SetResultFailure("CanRecruit() is false with only %1 recruit(s) owned", countBefore.ToString());
			return true;
		}

		string recruitId = recruits.AddRecruit(persId, subject, RECRUIT_NAME);
		if (recruitId == "")
		{
			SetResultFailure("AddRecruit() returned no recruit ID");
			return true;
		}

		if (recruits.GetRecruitCount(persId) != countBefore + 1)
		{
			SetResultFailure("Recruit count did not follow AddRecruit(): was %1, is now %2",
				countBefore.ToString(), recruits.GetRecruitCount(persId).ToString());
			return true;
		}

		OVT_RecruitData recruit = recruits.GetRecruit(recruitId);
		if (!recruit)
		{
			SetResultFailure("GetRecruit('%1') returned nothing for the ID AddRecruit() just handed out", recruitId);
			return true;
		}

		if (recruit.m_sOwnerPersistentId != persId)
		{
			SetResultFailure("The recruit is attributed to '%1', expected '%2'",
				recruit.m_sOwnerPersistentId, persId);
			return true;
		}

		if (recruit.GetName() != RECRUIT_NAME)
		{
			SetResultFailure("The recruit's name is '%1', expected '%2'", recruit.GetName(), RECRUIT_NAME);
			return true;
		}

		array<ref OVT_RecruitData> ownedRecruits = recruits.GetPlayerRecruits(persId);
		if (!ownedRecruits || ownedRecruits.Count() != countBefore + 1)
		{
			SetResultFailure("GetPlayerRecruits() disagrees with GetRecruitCount() after AddRecruit()");
			return true;
		}

		int xpBefore = recruit.m_iXP;
		recruits.AddRecruitXP(recruitId, RECRUIT_XP);

		OVT_RecruitData afterXp = recruits.GetRecruit(recruitId);
		if (!afterXp)
		{
			SetResultFailure("GetRecruit('%1') returned nothing after AddRecruitXP()", recruitId);
			return true;
		}

		if (afterXp.m_iXP != xpBefore + RECRUIT_XP)
		{
			SetResultFailure("AddRecruitXP(%1) did not stick: xp was %2, is now %3",
				RECRUIT_XP.ToString(), xpBefore.ToString(), afterXp.m_iXP.ToString());
			return true;
		}

		if (afterXp.m_iLevel != afterXp.GetLevel())
		{
			SetResultFailure("The recruit's cached level %1 disagrees with its computed level %2",
				afterXp.m_iLevel.ToString(), afterXp.GetLevel().ToString());
			return true;
		}

		recruits.RemoveRecruit(recruitId);

		if (recruits.GetRecruit(recruitId))
		{
			SetResultFailure("GetRecruit('%1') still returns a record after RemoveRecruit()", recruitId);
			return true;
		}

		if (recruits.GetRecruitCount(persId) != countBefore)
		{
			SetResultFailure("Recruit count did not return to %1 after RemoveRecruit(): it is %2",
				countBefore.ToString(), recruits.GetRecruitCount(persId).ToString());
			return true;
		}

		PrintFormat("Recruit round-trip: '%1' xp %2, removed cleanly", recruitId, afterXp.m_iXP.ToString());
		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Town control survives a write and a read through the town manager.
//!
//! ChangeTownControl() is the only public mutator for a town's controlling faction and it is the
//! one the whole campaign hangs off. Faction indices are read from the config rather than
//! hardcoded, because they are resolved from the faction manager at runtime.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceSuite, timeoutS: 30)]
class OVT_TEST_Persistence_TownControl_RoundTrips : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetResultFailure("OVT_Global.GetTowns() is null");
			return true;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetResultFailure("OVT_Global.GetConfig() is null");
			return true;
		}

		int townId;
		string diagnostic;
		OVT_TownData town = OVT_TEST_PersistenceSubject.ResolveFirstTown(townId, diagnostic);
		if (!town)
		{
			SetResultFailure("Cannot resolve a town: %1", diagnostic);
			return true;
		}

		int playerFaction = config.GetPlayerFactionIndex();
		int occupyingFaction = config.GetOccupyingFactionIndex();
		if (playerFaction < 0 || occupyingFaction < 0)
		{
			SetResultFailure("The config could not resolve both faction indices (player %1, occupying %2)",
				playerFaction.ToString(), occupyingFaction.ToString());
			return true;
		}

		int factionBefore = town.faction;

		towns.ChangeTownControl(town, playerFaction);
		if (towns.GetTown(townId).faction != playerFaction)
		{
			SetResultFailure("ChangeTownControl(player) did not stick: town faction is %1, expected %2",
				towns.GetTown(townId).faction.ToString(), playerFaction.ToString());
			return true;
		}

		if (towns.GetTown(townId).IsOccupyingFaction())
		{
			SetResultFailure("IsOccupyingFaction() is true for a town just given to the player faction");
			return true;
		}

		towns.ChangeTownControl(town, occupyingFaction);
		if (towns.GetTown(townId).faction != occupyingFaction)
		{
			SetResultFailure("ChangeTownControl(occupying) did not stick: town faction is %1, expected %2",
				towns.GetTown(townId).faction.ToString(), occupyingFaction.ToString());
			return true;
		}

		if (!towns.GetTown(townId).IsOccupyingFaction())
		{
			SetResultFailure("IsOccupyingFaction() is false for a town just given to the occupying faction");
			return true;
		}

		// Restore whatever the campaign start left the town on.
		towns.ChangeTownControl(town, factionBefore);
		if (towns.GetTown(townId).faction != factionBefore)
		{
			SetResultFailure("Town control did not return to its starting faction %1: it is %2",
				factionBefore.ToString(), towns.GetTown(townId).faction.ToString());
			return true;
		}

		PrintFormat("Town control round-trip on town %1: %2 -> player -> occupying -> %3",
			townId.ToString(), factionBefore.ToString(), towns.GetTown(townId).faction.ToString());
		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Town population survives a write and a read through the town manager.
//!
//! CLOSEST PUBLIC SEAM, documented choice (task 4.2). The town manager exposes no public
//! "set population" mutator at all: population is grown by a protected, RNG-driven routine on the
//! modifier tick, and the only public method that moves it is
//! TakeSupportersFromNearestTown(), which removes supporters AND population together. That is the
//! seam this case uses, and support has to be raised first because the method refuses to act when
//! the town has fewer supporters than the amount asked for.
//!
//! Consequence, stated plainly: this case can lower population and cannot raise it, so it restores
//! support and leaves population five lower than it found it. Nothing else in this suite reads
//! population, and the session ends seconds later.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceSuite, timeoutS: 30)]
class OVT_TEST_Persistence_TownPopulation_RoundTrips : SCR_AutotestCaseBase
{
	//! Small enough to be well inside the test world town's population of ~50.
	static const int DELTA = 5;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetResultFailure("OVT_Global.GetTowns() is null");
			return true;
		}

		int townId;
		string diagnostic;
		OVT_TownData town = OVT_TEST_PersistenceSubject.ResolveFirstTown(townId, diagnostic);
		if (!town)
		{
			SetResultFailure("Cannot resolve a town: %1", diagnostic);
			return true;
		}

		int populationBefore = town.population;
		int supportBefore = town.support;

		if (populationBefore <= DELTA)
		{
			SetResultFailure("Town %1 has only %2 population - too few to exercise the supporter seam",
				townId.ToString(), populationBefore.ToString());
			return true;
		}

		// Raise support first: the population seam refuses to act without enough supporters.
		towns.AddSupport(town.location, DELTA);
		if (towns.GetTown(townId).support != supportBefore + DELTA)
		{
			SetResultFailure("AddSupport(%1) did not stick: support was %2, is now %3",
				DELTA.ToString(), supportBefore.ToString(), towns.GetTown(townId).support.ToString());
			return true;
		}

		if (!towns.NearestTownHasSupporters(town.location, DELTA))
		{
			SetResultFailure("NearestTownHasSupporters(%1) is false right after AddSupport(%1)", DELTA.ToString());
			return true;
		}

		towns.TakeSupportersFromNearestTown(town.location, DELTA);

		int populationAfter = towns.GetTown(townId).population;
		if (populationAfter != populationBefore - DELTA)
		{
			SetResultFailure("TakeSupportersFromNearestTown(%1) did not move population: was %2, is now %3",
				DELTA.ToString(), populationBefore.ToString(), populationAfter.ToString());
			return true;
		}

		int supportAfter = towns.GetTown(townId).support;
		if (supportAfter != supportBefore)
		{
			SetResultFailure("Taking supporters did not return support to %1: it is %2",
				supportBefore.ToString(), supportAfter.ToString());
			return true;
		}

		PrintFormat("Town population round-trip on town %1: %2 -> %3",
			townId.ToString(), populationBefore.ToString(), populationAfter.ToString());
		PrintFormat("Town population round-trip: support restored to %1", supportAfter.ToString());
		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Town stability survives a write and a read through the town manager.
//!
//! Stability is never set directly in Overthrow: every path that moves it goes
//! modifier -> RecalculateStability -> stored value. TryAddStabilityModifier() and
//! RemoveStabilityModifier() are the two public ends of that path, and both act synchronously on
//! the server - the request RPC they issue is delivered to the local authority in the same frame
//! (measured during this phase; see findings.md "Phase 4 - RPC self-delivery"). So this case
//! exercises the real seam rather than writing the field.
//!
//! The expected value is DERIVED, not hardcoded: it is what the modifier system itself computes
//! from the town's current modifier list. That makes this an invariant a persistence layer must
//! preserve - the stored stability equals the recalculation of the stored modifiers - rather than a
//! restatement of one config number. The round trip is the second claim: removing the modifier
//! returns stability to exactly where it started.
//!
//! The modifier is chosen as the first one with a NEGATIVE base effect. Stability starts at the
//! configured maximum, so a positive modifier would clamp and be unobservable - the case would then
//! "pass" while proving nothing.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceSuite, timeoutS: 30)]
class OVT_TEST_Persistence_TownStability_RoundTrips : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetResultFailure("OVT_Global.GetTowns() is null");
			return true;
		}

		int townId;
		string diagnostic;
		OVT_TownData town = OVT_TEST_PersistenceSubject.ResolveFirstTown(townId, diagnostic);
		if (!town)
		{
			SetResultFailure("Cannot resolve a town: %1", diagnostic);
			return true;
		}

		OVT_TownModifierSystem system = towns.GetModifierSystem(OVT_TownStabilityModifierSystem);
		if (!system || !system.m_Config || !system.m_Config.m_aModifiers)
		{
			SetResultFailure("The town manager has no stability modifier system with a loaded config");
			return true;
		}

		int modifierIndex = FindNegativeModifierIndex(system);
		if (modifierIndex < 0)
		{
			SetResultFailure("No stability modifier has a negative base effect - nothing that can move stability down from its maximum");
			return true;
		}

		int stabilityBefore = town.stability;
		int modifiersBefore = town.stabilityModifiers.Count();

		if (!towns.TryAddStabilityModifier(townId, modifierIndex))
		{
			SetResultFailure("TryAddStabilityModifier(%1) refused to add a modifier to a town that has %2",
				modifierIndex.ToString(), modifiersBefore.ToString());
			return true;
		}

		if (town.stabilityModifiers.Count() != modifiersBefore + 1)
		{
			SetResultFailure("The modifier did not stick: town had %1 stability modifiers, now has %2",
				modifiersBefore.ToString(), town.stabilityModifiers.Count().ToString());
			return true;
		}

		// Read the stored value back through the manager's accessor, and check it against what the
		// modifier system computes from the list that is actually stored.
		int afterAdd = towns.GetTown(townId).stability;
		int expected = system.Recalculate(towns.GetTown(townId).stabilityModifiers);
		if (afterAdd != expected)
		{
			SetResultFailure("Stored stability %1 disagrees with the modifier system's recalculation %2",
				afterAdd.ToString(), expected.ToString());
			return true;
		}

		if (afterAdd >= stabilityBefore)
		{
			SetResultFailure("A negative stability modifier did not lower stability: was %1, is now %2",
				stabilityBefore.ToString(), afterAdd.ToString());
			return true;
		}

		// Both public accessors must resolve to the SAME live record. Stated honestly, because it is
		// weaker than it looks: GetTowns()[townId] IS the object GetTown(townId) hands out, so what
		// this pins is the indexing contract - a town's ID is its index into GetTowns(), and the list
		// is long enough to contain it - not agreement between two independent storage paths. The
		// manager exposes no second, independent stability accessor to compare against; if a
		// migration ever adds one, this is the line to strengthen.
		array<ref OVT_TownData> townList = towns.GetTowns();
		if (!townList || townList.Count() <= townId || townList[townId].stability != afterAdd)
		{
			SetResultFailure("GetTowns() does not show the stability GetTown() reports (%1)", afterAdd.ToString());
			return true;
		}

		towns.RemoveStabilityModifier(townId, modifierIndex);

		if (town.stabilityModifiers.Count() != modifiersBefore)
		{
			SetResultFailure("Removing the modifier did not stick: town has %1 stability modifiers, expected %2",
				town.stabilityModifiers.Count().ToString(), modifiersBefore.ToString());
			return true;
		}

		int afterRemove = towns.GetTown(townId).stability;
		if (afterRemove != stabilityBefore)
		{
			SetResultFailure("Stability did not return to its starting value: was %1, is now %2",
				stabilityBefore.ToString(), afterRemove.ToString());
			return true;
		}

		PrintFormat("Town stability round-trip on town %1: %2 -> %3 -> %2",
			townId.ToString(), stabilityBefore.ToString(), afterAdd.ToString());
		SetResultSuccess();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Finds the first configured stability modifier whose base effect lowers stability.
	//! \param[in] system The stability modifier system holding the config.
	//! \return The modifier's index, or -1 when every configured modifier is non-negative.
	protected int FindNegativeModifierIndex(OVT_TownModifierSystem system)
	{
		foreach (int i, OVT_ModifierConfig config : system.m_Config.m_aModifiers)
		{
			if (!config)
				continue;

			if (config.baseEffect < 0)
				return i;
		}

		return -1;
	}
}

//------------------------------------------------------------------------------------------------
//! Town support survives a write and a read through the town manager.
//!
//! Support has two public mutators that both act synchronously on the server - AddSupport(), which
//! resolves the town from a world position, and ResetSupport(), which takes the record - so this
//! case exercises both and proves they agree on the same town.
//!
//! Deliberately NOT asserted: SupportPercentage(). Its integer division is a suspected defect and
//! belongs in the pure-logic tier's boundary table (Phase 5), where it can be pinned honestly
//! rather than smuggled into a persistence assertion.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceSuite, timeoutS: 30)]
class OVT_TEST_Persistence_TownSupport_RoundTrips : SCR_AutotestCaseBase
{
	//! Deliberately not a round number.
	static const int DELTA = 7;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetResultFailure("OVT_Global.GetTowns() is null");
			return true;
		}

		int townId;
		string diagnostic;
		OVT_TownData town = OVT_TEST_PersistenceSubject.ResolveFirstTown(townId, diagnostic);
		if (!town)
		{
			SetResultFailure("Cannot resolve a town: %1", diagnostic);
			return true;
		}

		int supportBefore = town.support;

		towns.AddSupport(town.location, DELTA);
		int afterAdd = towns.GetTown(townId).support;
		if (afterAdd != supportBefore + DELTA)
		{
			SetResultFailure("AddSupport(%1) did not stick: support was %2, is now %3",
				DELTA.ToString(), supportBefore.ToString(), afterAdd.ToString());
			return true;
		}

		towns.ResetSupport(town);
		int afterReset = towns.GetTown(townId).support;
		if (afterReset != 0)
		{
			SetResultFailure("ResetSupport() left support at %1, expected 0", afterReset.ToString());
			return true;
		}

		// Restore what the campaign start left behind.
		towns.AddSupport(town.location, supportBefore);
		int restored = towns.GetTown(townId).support;
		if (restored != supportBefore)
		{
			SetResultFailure("Support did not return to its starting value %1: it is %2",
				supportBefore.ToString(), restored.ToString());
			return true;
		}

		PrintFormat("Town support round-trip on town %1: %2 -> %3 -> 0 -> %2",
			townId.ToString(), supportBefore.ToString(), afterAdd.ToString());
		SetResultSuccess();
		return true;
	}
}
