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
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
		{
			SetFailure("OVT_Global.GetEconomy() is null");
			return true;
		}

		string diagnostic;
		string persId = OVT_TEST_PersistenceSubject.ResolveLocalPersistentId(diagnostic);
		if (persId == "")
		{
			SetFailure("Cannot resolve the persistent player ID: %1", diagnostic);
			return true;
		}

		int playerId = OVT_TEST_PersistenceSubject.ResolveLocalPlayerId(diagnostic);
		if (playerId < 1)
		{
			SetFailure("Cannot resolve the runtime player ID: %1", diagnostic);
			return true;
		}

		int before = economy.GetPlayerMoney(persId);

		// Write through the runtime-ID mutator, read back through the persistent-ID accessor.
		economy.AddPlayerMoney(playerId, ADD_AMOUNT);
		int afterAdd = economy.GetPlayerMoney(persId);
		if (afterAdd != before + ADD_AMOUNT)
		{
			SetFailure("AddPlayerMoney(%1) did not stick: money was %2, is now %3",
				ADD_AMOUNT.ToString(), before.ToString(), afterAdd.ToString());
			return true;
		}

		if (!economy.PlayerHasMoney(persId, afterAdd))
		{
			SetFailure("PlayerHasMoney() disagrees with GetPlayerMoney() at %1", afterAdd.ToString());
			return true;
		}

		economy.TakePlayerMoney(playerId, TAKE_AMOUNT);
		int afterTake = economy.GetPlayerMoney(persId);
		if (afterTake != afterAdd - TAKE_AMOUNT)
		{
			SetFailure("TakePlayerMoney(%1) did not stick: money was %2, is now %3",
				TAKE_AMOUNT.ToString(), afterAdd.ToString(), afterTake.ToString());
			return true;
		}

		// Restore, and let the restoration double as an independent claim about the arithmetic.
		economy.TakePlayerMoney(playerId, ADD_AMOUNT - TAKE_AMOUNT);
		int restored = economy.GetPlayerMoney(persId);
		if (restored != before)
		{
			SetFailure("Money did not return to its starting value: expected %1, got %2",
				before.ToString(), restored.ToString());
			return true;
		}

		PrintFormat("Player money round-trip: %1 -> %2 -> %3", before.ToString(), afterAdd.ToString(), restored.ToString());
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
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_SkillManagerComponent skills = OVT_Global.GetSkills();
		if (!skills)
		{
			SetFailure("OVT_Global.GetSkills() is null");
			return true;
		}

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
		{
			SetFailure("OVT_Global.GetPlayers() is null");
			return true;
		}

		string diagnostic;
		string persId = OVT_TEST_PersistenceSubject.ResolveLocalPersistentId(diagnostic);
		if (persId == "")
		{
			SetFailure("Cannot resolve the persistent player ID: %1", diagnostic);
			return true;
		}

		int playerId = OVT_TEST_PersistenceSubject.ResolveLocalPlayerId(diagnostic);
		if (playerId < 1)
		{
			SetFailure("Cannot resolve the runtime player ID: %1", diagnostic);
			return true;
		}

		OVT_PlayerData player = players.GetPlayer(persId);
		if (!player)
		{
			SetFailure("The player manager has no record for persistent ID %1", persId);
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
			SetFailure("The player manager lost the record for persistent ID %1 after GiveXP()", persId);
			return true;
		}

		if (afterGive.xp != xpBefore + XP_AMOUNT)
		{
			SetFailure("GiveXP(%1) did not stick: xp was %2, is now %3",
				XP_AMOUNT.ToString(), xpBefore.ToString(), afterGive.xp.ToString());
			return true;
		}

		int levelAfter = afterGive.GetLevel();
		if (levelAfter < levelBefore)
		{
			SetFailure("Level went backwards after gaining XP: was %1, is now %2",
				levelBefore.ToString(), levelAfter.ToString());
			return true;
		}

		skills.TakeXP(playerId, XP_AMOUNT);

		OVT_PlayerData afterTake = players.GetPlayer(persId);
		if (!afterTake)
		{
			SetFailure("The player manager lost the record for persistent ID %1 after TakeXP()", persId);
			return true;
		}

		int xpRestored = afterTake.xp;
		if (xpRestored != xpBefore)
		{
			SetFailure("TakeXP(%1) did not return xp to its starting value: expected %2, got %3",
				XP_AMOUNT.ToString(), xpBefore.ToString(), xpRestored.ToString());
			return true;
		}

		// --- Skill levels ---
		string skillKey = OVT_TEST_PersistenceSubject.ResolveFirstSkillKey(diagnostic);
		if (skillKey == "")
		{
			SetFailure("Cannot resolve a skill to exercise: %1", diagnostic);
			return true;
		}

		OVT_PlayerData beforeSkill = players.GetPlayer(persId);
		if (!beforeSkill)
		{
			SetFailure("The player manager lost the record for persistent ID %1 before the skill mutation", persId);
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
			SetFailure("AddSkillLevel('%1') did not stick: level was %2, is now %3",
				skillKey, skillBefore.ToString(), skillAfter.ToString());
			return true;
		}

		OVT_PlayerData afterSkill = players.GetPlayer(persId);
		if (!afterSkill)
		{
			SetFailure("The player manager lost the record for persistent ID %1 after AddSkillLevel()", persId);
			return true;
		}

		int countAfter = afterSkill.CountSkills();
		if (countAfter != countBefore + 1)
		{
			SetFailure("CountSkills() did not follow the added level: was %1, is now %2",
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
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if (!realEstate)
		{
			SetFailure("OVT_Global.GetRealEstate() is null");
			return true;
		}

		string diagnostic;
		string persId = OVT_TEST_PersistenceSubject.ResolveLocalPersistentId(diagnostic);
		if (persId == "")
		{
			SetFailure("Cannot resolve the persistent player ID: %1", diagnostic);
			return true;
		}

		IEntity building = OVT_TEST_PersistenceSubject.ResolveUnownedBuilding(diagnostic);
		if (!building)
		{
			SetFailure("Cannot resolve a building to own: %1", diagnostic);
			return true;
		}

		if (realEstate.GetOwnerID(building) != "")
		{
			SetFailure("The building picked as an unowned subject is already owned by '%1'",
				realEstate.GetOwnerID(building));
			return true;
		}

		realEstate.SetOwnerPersistentId(persId, building);

		string ownerAfterSet = realEstate.GetOwnerID(building);
		if (ownerAfterSet != persId)
		{
			SetFailure("SetOwnerPersistentId() did not stick: GetOwnerID() returned '%1', expected '%2'",
				ownerAfterSet, persId);
			return true;
		}

		if (!realEstate.IsOwner(persId, building.GetID()))
		{
			SetFailure("IsOwner() says the player does not own the building GetOwnerID() says they own");
			return true;
		}

		if (!realEstate.IsOwned(building.GetID()))
		{
			SetFailure("IsOwned() says the building is unowned immediately after it was given an owner");
			return true;
		}

		// -1 is the documented "remove ownership" form of the same mutator.
		realEstate.SetOwner(-1, building);

		string ownerAfterRemove = realEstate.GetOwnerID(building);
		if (ownerAfterRemove != "")
		{
			SetFailure("Removing the owner did not stick: GetOwnerID() still returns '%1'", ownerAfterRemove);
			return true;
		}

		if (realEstate.IsOwner(persId, building.GetID()))
		{
			SetFailure("IsOwner() still reports ownership after the owner was removed");
			return true;
		}

		PrintFormat("Real estate ownership round-trip at %1 for '%2'",
			building.GetOrigin().ToString(), persId);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! BUG-003 - transferring a building to a new owner must CLEAR the previous owner.
//!
//! The defect: DoSetOwnerPersistentId() overwrites the position->owner map but only ever APPENDS to
//! the new owner's owned-position list, so after a transfer the OLD owner's list still contains the
//! building: GetOwnerID() names the new owner while IsOwner(oldOwner) stays true, and a later
//! ownership removal leaves IsOwned() scanning a stale claim. The persistence suite's other real
//! estate case deliberately routes around this by only ever owning an UNOWNED building - this case
//! is the transfer coverage that note promised.
//!
//! SYNTHETIC OWNERS, ON PURPOSE. Both owners are made-up persistent IDs, not the local player, so
//! the case never disturbs the player's real holdings (the campaign start deeds them a home) and
//! cleanup is total: the one building touched is returned to unowned at the end through the same
//! public mutator, and the case FAILS if that cleanup leaves any trace - which is itself the
//! stale-list half of the bug.
//!
//! PROVEN ABLE TO FAIL: authored red against the live BUG-003 defect (IsOwner(previous owner)
//! returned true after the transfer); flipped green with the fix in
//! OVT_OwnerManagerComponent.DoSetOwnerPersistentId().
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceSuite, timeoutS: 30)]
class OVT_TEST_Persistence_RealEstateTransfer_ClearsPreviousOwner : SCR_AutotestCaseBase
{
	//! First owner. Synthetic, never a real player's persistent ID.
	static const string OWNER_A = "AUTOTEST-BUG003-OWNER-A";

	//! The owner the building is transferred to.
	static const string OWNER_B = "AUTOTEST-BUG003-OWNER-B";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if (!realEstate)
		{
			SetFailure("OVT_Global.GetRealEstate() is null");
			return true;
		}

		string diagnostic;
		IEntity building = OVT_TEST_PersistenceSubject.ResolveUnownedBuilding(diagnostic);
		if (!building)
		{
			SetFailure("Cannot resolve a building to transfer: %1", diagnostic);
			return true;
		}

		realEstate.SetOwnerPersistentId(OWNER_A, building);
		if (!realEstate.IsOwner(OWNER_A, building.GetID()))
		{
			SetFailure("Precondition failed: the first owner does not own the building it was just given");
			return true;
		}

		// The transfer under test: same mutator, different owner, no removal in between - exactly
		// what any sale/hand-over path does.
		realEstate.SetOwnerPersistentId(OWNER_B, building);

		string ownerAfterTransfer = realEstate.GetOwnerID(building);
		if (ownerAfterTransfer != OWNER_B)
		{
			SetFailure("The transfer did not stick: GetOwnerID() returned '%1', expected '%2'",
				ownerAfterTransfer, OWNER_B);
			return true;
		}

		if (!realEstate.IsOwner(OWNER_B, building.GetID()))
		{
			SetFailure("IsOwner() denies the new owner the building GetOwnerID() says they own");
			return true;
		}

		if (realEstate.IsOwner(OWNER_A, building.GetID()))
		{
			SetFailure("BUG-003: IsOwner() still reports the PREVIOUS owner after a transfer - the old owner's owned-position list was never cleaned");
			return true;
		}

		// Cleanup doubles as the stale-list assertion: removing the CURRENT owner must leave the
		// building fully unowned; a leaked previous-owner entry keeps IsOwned() true here.
		realEstate.SetOwner(-1, building);

		if (realEstate.IsOwned(building.GetID()))
		{
			SetFailure("BUG-003: IsOwned() still finds an owner after the current owner was removed - a stale list entry from before the transfer survives");
			return true;
		}

		PrintFormat("Ownership transfer at %1: previous owner cleared, new owner sole claimant, removal total",
			building.GetOrigin().ToString());
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
//!
//! THE INACTIVE HALF (recruit-ux Phase 1, T1.9). A recruit can be ACTIVE or INACTIVE - owned but out
//! of its owner's group, holding position - and that state is a campaign fact this tier is
//! responsible for. Three claims are added here, all through the manager's public API:
//!   1. A freshly created recruit is ACTIVE. The state has to have a defined starting value, and
//!      "every recruit you hire is inactive" would be an invisible, total feature failure.
//!   2. Setting it sticks and IsRecruitInactive() agrees with the record.
//!   3. GetPlayerRecruitsByState() splits the roster - the inactive recruit appears in the inactive
//!      half and NOT in the active half. Both directions are asserted, because a filter that ignored
//!      its argument would satisfy either one alone.
//!
//! PROVEN ABLE TO FAIL (recruit-ux Phase 1, by deliberate fault + compile-check, since this tier is
//! the orchestrator's to run):
//!   a. OVT_RecruitManagerComponent.IsRecruitInactive() forced to `return false;` -> claim 2 fails on
//!      "IsRecruitInactive() disagrees with the record it reads".
//!   b. GetPlayerRecruitsByState() with its `recruit.m_bInactive == inactive` filter deleted (every
//!      recruit inserted) -> claim 3 fails on the active-half assertion, which then contains the
//!      recruit that was just deactivated.
//!   Both faults were injected and the whole tree recompiled: it compiled CLEAN, which is the point -
//!   neither fault is catchable by anything but this case. Both were reverted and the tree recompiled
//!   clean again. No maxAttempts - the case is deterministic state through one manager.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceSuite, timeoutS: 30)]
class OVT_TEST_Persistence_Recruits_RoundTrip : SCR_AutotestCaseBase
{
	//! Explicit name, so the case does not depend on the random name generator.
	static const string RECRUIT_NAME = "Autotest Recruit";

	//! XP award used to prove the recruit's own progression is stored on the record.
	static const int RECRUIT_XP = 400;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if (!recruits)
		{
			SetFailure("OVT_Global.GetRecruits() is null");
			return true;
		}

		string diagnostic;
		string persId = OVT_TEST_PersistenceSubject.ResolveLocalPersistentId(diagnostic);
		if (persId == "")
		{
			SetFailure("Cannot resolve the persistent player ID: %1", diagnostic);
			return true;
		}

		IEntity subject = OVT_TEST_PersistenceSubject.ResolveRecruitSubjectEntity(diagnostic);
		if (!subject)
		{
			SetFailure("Cannot resolve an entity to attach a recruit to: %1", diagnostic);
			return true;
		}

		int countBefore = recruits.GetRecruitCount(persId);
		if (!recruits.CanRecruit(persId))
		{
			SetFailure("CanRecruit() is false with only %1 recruit(s) owned", countBefore.ToString());
			return true;
		}

		string recruitId = recruits.AddRecruit(persId, subject, RECRUIT_NAME);
		if (recruitId == "")
		{
			SetFailure("AddRecruit() returned no recruit ID");
			return true;
		}

		if (recruits.GetRecruitCount(persId) != countBefore + 1)
		{
			SetFailure("Recruit count did not follow AddRecruit(): was %1, is now %2",
				countBefore.ToString(), recruits.GetRecruitCount(persId).ToString());
			return true;
		}

		OVT_RecruitData recruit = recruits.GetRecruit(recruitId);
		if (!recruit)
		{
			SetFailure("GetRecruit('%1') returned nothing for the ID AddRecruit() just handed out", recruitId);
			return true;
		}

		if (recruit.m_sOwnerPersistentId != persId)
		{
			SetFailure("The recruit is attributed to '%1', expected '%2'",
				recruit.m_sOwnerPersistentId, persId);
			return true;
		}

		if (recruit.GetName() != RECRUIT_NAME)
		{
			SetFailure("The recruit's name is '%1', expected '%2'", recruit.GetName(), RECRUIT_NAME);
			return true;
		}

		array<ref OVT_RecruitData> ownedRecruits = recruits.GetPlayerRecruits(persId);
		if (!ownedRecruits || ownedRecruits.Count() != countBefore + 1)
		{
			SetFailure("GetPlayerRecruits() disagrees with GetRecruitCount() after AddRecruit()");
			return true;
		}

		int xpBefore = recruit.m_iXP;
		recruits.AddRecruitXP(recruitId, RECRUIT_XP);

		OVT_RecruitData afterXp = recruits.GetRecruit(recruitId);
		if (!afterXp)
		{
			SetFailure("GetRecruit('%1') returned nothing after AddRecruitXP()", recruitId);
			return true;
		}

		if (afterXp.m_iXP != xpBefore + RECRUIT_XP)
		{
			SetFailure("AddRecruitXP(%1) did not stick: xp was %2, is now %3",
				RECRUIT_XP.ToString(), xpBefore.ToString(), afterXp.m_iXP.ToString());
			return true;
		}

		if (afterXp.m_iLevel != afterXp.GetLevel())
		{
			SetFailure("The recruit's cached level %1 disagrees with its computed level %2",
				afterXp.m_iLevel.ToString(), afterXp.GetLevel().ToString());
			return true;
		}

		// --- Active/inactive state, through the manager's public API.
		if (recruit.m_bInactive)
		{
			SetFailure("A freshly created recruit is INACTIVE - a new hire must join its owner's group");
			return true;
		}

		if (recruits.IsRecruitInactive(recruitId))
		{
			SetFailure("IsRecruitInactive('%1') is true for a freshly created recruit", recruitId);
			return true;
		}

		int activeBefore = recruits.GetPlayerRecruitsByState(persId, false).Count();
		int inactiveBefore = recruits.GetPlayerRecruitsByState(persId, true).Count();

		recruit.m_bInactive = true;

		if (!recruits.IsRecruitInactive(recruitId))
		{
			SetFailure("IsRecruitInactive('%1') disagrees with the record it reads: the record says inactive", recruitId);
			return true;
		}

		array<ref OVT_RecruitData> inactiveHalf = recruits.GetPlayerRecruitsByState(persId, true);
		if (inactiveHalf.Count() != inactiveBefore + 1)
		{
			SetFailure("The inactive half of the roster holds %1 recruits, expected %2",
				inactiveHalf.Count().ToString(), (inactiveBefore + 1).ToString());
			return true;
		}

		bool foundInInactive = false;
		foreach (OVT_RecruitData listed : inactiveHalf)
		{
			if (listed.m_sRecruitId == recruitId)
				foundInInactive = true;
		}

		if (!foundInInactive)
		{
			SetFailure("The deactivated recruit '%1' is missing from the inactive half of the roster", recruitId);
			return true;
		}

		array<ref OVT_RecruitData> activeHalf = recruits.GetPlayerRecruitsByState(persId, false);
		if (activeHalf.Count() != activeBefore - 1)
		{
			SetFailure("The active half of the roster holds %1 recruits, expected %2",
				activeHalf.Count().ToString(), (activeBefore - 1).ToString());
			return true;
		}

		foreach (OVT_RecruitData stillActive : activeHalf)
		{
			if (stillActive.m_sRecruitId == recruitId)
			{
				SetFailure("The deactivated recruit '%1' is STILL in the active half of the roster", recruitId);
				return true;
			}
		}

		recruits.RemoveRecruit(recruitId);

		if (recruits.GetRecruit(recruitId))
		{
			SetFailure("GetRecruit('%1') still returns a record after RemoveRecruit()", recruitId);
			return true;
		}

		if (recruits.GetRecruitCount(persId) != countBefore)
		{
			SetFailure("Recruit count did not return to %1 after RemoveRecruit(): it is %2",
				countBefore.ToString(), recruits.GetRecruitCount(persId).ToString());
			return true;
		}

		PrintFormat("Recruit round-trip: '%1' xp %2, removed cleanly", recruitId, afterXp.m_iXP.ToString());
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
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetFailure("OVT_Global.GetTowns() is null");
			return true;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("OVT_Global.GetConfig() is null");
			return true;
		}

		int townId;
		string diagnostic;
		OVT_TownData town = OVT_TEST_PersistenceSubject.ResolveFirstTown(townId, diagnostic);
		if (!town)
		{
			SetFailure("Cannot resolve a town: %1", diagnostic);
			return true;
		}

		int playerFaction = config.GetPlayerFactionIndex();
		int occupyingFaction = config.GetOccupyingFactionIndex();
		if (playerFaction < 0 || occupyingFaction < 0)
		{
			SetFailure("The config could not resolve both faction indices (player %1, occupying %2)",
				playerFaction.ToString(), occupyingFaction.ToString());
			return true;
		}

		int factionBefore = town.faction;

		towns.ChangeTownControl(town, playerFaction);
		if (towns.GetTown(townId).faction != playerFaction)
		{
			SetFailure("ChangeTownControl(player) did not stick: town faction is %1, expected %2",
				towns.GetTown(townId).faction.ToString(), playerFaction.ToString());
			return true;
		}

		if (towns.GetTown(townId).IsOccupyingFaction())
		{
			SetFailure("IsOccupyingFaction() is true for a town just given to the player faction");
			return true;
		}

		towns.ChangeTownControl(town, occupyingFaction);
		if (towns.GetTown(townId).faction != occupyingFaction)
		{
			SetFailure("ChangeTownControl(occupying) did not stick: town faction is %1, expected %2",
				towns.GetTown(townId).faction.ToString(), occupyingFaction.ToString());
			return true;
		}

		if (!towns.GetTown(townId).IsOccupyingFaction())
		{
			SetFailure("IsOccupyingFaction() is false for a town just given to the occupying faction");
			return true;
		}

		// Restore whatever the campaign start left the town on.
		towns.ChangeTownControl(town, factionBefore);
		if (towns.GetTown(townId).faction != factionBefore)
		{
			SetFailure("Town control did not return to its starting faction %1: it is %2",
				factionBefore.ToString(), towns.GetTown(townId).faction.ToString());
			return true;
		}

		PrintFormat("Town control round-trip on town %1: %2 -> player -> occupying -> %3",
			townId.ToString(), factionBefore.ToString(), towns.GetTown(townId).faction.ToString());
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
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetFailure("OVT_Global.GetTowns() is null");
			return true;
		}

		int townId;
		string diagnostic;
		OVT_TownData town = OVT_TEST_PersistenceSubject.ResolveFirstTown(townId, diagnostic);
		if (!town)
		{
			SetFailure("Cannot resolve a town: %1", diagnostic);
			return true;
		}

		int populationBefore = town.population;
		int supportBefore = town.support;

		if (populationBefore <= DELTA)
		{
			SetFailure("Town %1 has only %2 population - too few to exercise the supporter seam",
				townId.ToString(), populationBefore.ToString());
			return true;
		}

		// Raise support first: the population seam refuses to act without enough supporters.
		towns.AddSupport(town.location, DELTA);
		if (towns.GetTown(townId).support != supportBefore + DELTA)
		{
			SetFailure("AddSupport(%1) did not stick: support was %2, is now %3",
				DELTA.ToString(), supportBefore.ToString(), towns.GetTown(townId).support.ToString());
			return true;
		}

		if (!towns.NearestTownHasSupporters(town.location, DELTA))
		{
			SetFailure("NearestTownHasSupporters(%1) is false right after AddSupport(%1)", DELTA.ToString());
			return true;
		}

		towns.TakeSupportersFromNearestTown(town.location, DELTA);

		int populationAfter = towns.GetTown(townId).population;
		if (populationAfter != populationBefore - DELTA)
		{
			SetFailure("TakeSupportersFromNearestTown(%1) did not move population: was %2, is now %3",
				DELTA.ToString(), populationBefore.ToString(), populationAfter.ToString());
			return true;
		}

		int supportAfter = towns.GetTown(townId).support;
		if (supportAfter != supportBefore)
		{
			SetFailure("Taking supporters did not return support to %1: it is %2",
				supportBefore.ToString(), supportAfter.ToString());
			return true;
		}

		PrintFormat("Town population round-trip on town %1: %2 -> %3",
			townId.ToString(), populationBefore.ToString(), populationAfter.ToString());
		PrintFormat("Town population round-trip: support restored to %1", supportAfter.ToString());
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
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetFailure("OVT_Global.GetTowns() is null");
			return true;
		}

		int townId;
		string diagnostic;
		OVT_TownData town = OVT_TEST_PersistenceSubject.ResolveFirstTown(townId, diagnostic);
		if (!town)
		{
			SetFailure("Cannot resolve a town: %1", diagnostic);
			return true;
		}

		OVT_TownModifierSystem system = towns.GetModifierSystem(OVT_TownStabilityModifierSystem);
		if (!system || !system.m_Config || !system.m_Config.m_aModifiers)
		{
			SetFailure("The town manager has no stability modifier system with a loaded config");
			return true;
		}

		int modifierIndex = FindNegativeModifierIndex(system);
		if (modifierIndex < 0)
		{
			SetFailure("No stability modifier has a negative base effect - nothing that can move stability down from its maximum");
			return true;
		}

		int stabilityBefore = town.stability;
		int modifiersBefore = town.stabilityModifiers.Count();

		if (!towns.TryAddStabilityModifier(townId, modifierIndex))
		{
			SetFailure("TryAddStabilityModifier(%1) refused to add a modifier to a town that has %2",
				modifierIndex.ToString(), modifiersBefore.ToString());
			return true;
		}

		if (town.stabilityModifiers.Count() != modifiersBefore + 1)
		{
			SetFailure("The modifier did not stick: town had %1 stability modifiers, now has %2",
				modifiersBefore.ToString(), town.stabilityModifiers.Count().ToString());
			return true;
		}

		// Read the stored value back through the manager's accessor, and check it against what the
		// modifier system computes from the list that is actually stored.
		int afterAdd = towns.GetTown(townId).stability;
		int expected = system.Recalculate(towns.GetTown(townId).stabilityModifiers);
		if (afterAdd != expected)
		{
			SetFailure("Stored stability %1 disagrees with the modifier system's recalculation %2",
				afterAdd.ToString(), expected.ToString());
			return true;
		}

		if (afterAdd >= stabilityBefore)
		{
			SetFailure("A negative stability modifier did not lower stability: was %1, is now %2",
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
			SetFailure("GetTowns() does not show the stability GetTown() reports (%1)", afterAdd.ToString());
			return true;
		}

		towns.RemoveStabilityModifier(townId, modifierIndex);

		if (town.stabilityModifiers.Count() != modifiersBefore)
		{
			SetFailure("Removing the modifier did not stick: town has %1 stability modifiers, expected %2",
				town.stabilityModifiers.Count().ToString(), modifiersBefore.ToString());
			return true;
		}

		int afterRemove = towns.GetTown(townId).stability;
		if (afterRemove != stabilityBefore)
		{
			SetFailure("Stability did not return to its starting value: was %1, is now %2",
				stabilityBefore.ToString(), afterRemove.ToString());
			return true;
		}

		PrintFormat("Town stability round-trip on town %1: %2 -> %3 -> %2",
			townId.ToString(), stabilityBefore.ToString(), afterAdd.ToString());
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
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
		{
			SetFailure("OVT_Global.GetTowns() is null");
			return true;
		}

		int townId;
		string diagnostic;
		OVT_TownData town = OVT_TEST_PersistenceSubject.ResolveFirstTown(townId, diagnostic);
		if (!town)
		{
			SetFailure("Cannot resolve a town: %1", diagnostic);
			return true;
		}

		int supportBefore = town.support;

		towns.AddSupport(town.location, DELTA);
		int afterAdd = towns.GetTown(townId).support;
		if (afterAdd != supportBefore + DELTA)
		{
			SetFailure("AddSupport(%1) did not stick: support was %2, is now %3",
				DELTA.ToString(), supportBefore.ToString(), afterAdd.ToString());
			return true;
		}

		towns.ResetSupport(town);
		int afterReset = towns.GetTown(townId).support;
		if (afterReset != 0)
		{
			SetFailure("ResetSupport() left support at %1, expected 0", afterReset.ToString());
			return true;
		}

		// Restore what the campaign start left behind.
		towns.AddSupport(town.location, supportBefore);
		int restored = towns.GetTown(townId).support;
		if (restored != supportBefore)
		{
			SetFailure("Support did not return to its starting value %1: it is %2",
				supportBefore.ToString(), restored.ToString());
			return true;
		}

		PrintFormat("Town support round-trip on town %1: %2 -> %3 -> 0 -> %2",
			townId.ToString(), supportBefore.ToString(), afterAdd.ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! BUG-004 - removing a recruit must also remove its entity and replication lookups.
//!
//! The defect: RemoveRecruit() cleans m_mRecruits and m_mRecruitsByOwner but never
//! m_mEntityToRecruit / m_mRplIdToRecruit, so the removed record's ID keeps being handed out for as
//! long as the mapped entity lives - GetRecruitFromEntity() resolves the dead ID and GetRecruit()
//! then answers null for it - and the maps grow one stale entry per removal for the session's
//! lifetime.
//!
//! The public read API cannot see the leak (a dead ID resolves to a null record either way), so the
//! two assertions scan the lookup maps themselves for any entry still pointing at the removed
//! record - by VALUE, not by key, so they also catch entries under an id this case never learned.
//! That is the same direct-member style the town cases use for m_Towns, and it is the only
//! observation that can distinguish "removed cleanly" from "leaked".
//!
//! PROVEN ABLE TO FAIL: authored red against the live BUG-004 defect (the entity map still carried
//! the removed recruit's ID); flipped green with the cleanup added to RemoveRecruit().
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceSuite, timeoutS: 30)]
class OVT_TEST_Persistence_RecruitRemoval_ClearsEntityLookups : SCR_AutotestCaseBase
{
	//! Explicit name, so the case does not depend on the random name generator.
	static const string RECRUIT_NAME = "Autotest Lookup Leak Recruit";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if (!recruits)
		{
			SetFailure("OVT_Global.GetRecruits() is null");
			return true;
		}

		string diagnostic;
		string persId = OVT_TEST_PersistenceSubject.ResolveLocalPersistentId(diagnostic);
		if (persId == "")
		{
			SetFailure("Cannot resolve the persistent player ID: %1", diagnostic);
			return true;
		}

		IEntity subject = OVT_TEST_PersistenceSubject.ResolveRecruitSubjectEntity(diagnostic);
		if (!subject)
		{
			SetFailure("Cannot resolve an entity to attach a recruit to: %1", diagnostic);
			return true;
		}

		string recruitId = recruits.AddRecruit(persId, subject, RECRUIT_NAME);
		if (recruitId == "")
		{
			SetFailure("AddRecruit() returned no recruit ID");
			return true;
		}

		// Sanity: the mapping this case is about actually exists before the removal.
		OVT_RecruitData mapped = recruits.GetRecruitFromEntity(subject);
		if (!mapped || mapped.m_sRecruitId != recruitId)
		{
			SetFailure("GetRecruitFromEntity() does not resolve the recruit that was just attached to the entity");
			return true;
		}

		recruits.RemoveRecruit(recruitId);

		if (recruits.GetRecruit(recruitId))
		{
			SetFailure("GetRecruit() still returns a record for a removed recruit");
			return true;
		}

		foreach (EntityID entityId, string mappedRecruitId : recruits.m_mEntityToRecruit)
		{
			if (mappedRecruitId == recruitId)
			{
				SetFailure("BUG-004: m_mEntityToRecruit still maps an entity to removed recruit '%1' - RemoveRecruit() leaked the entity lookup", recruitId);
				return true;
			}
		}

		foreach (RplId rplId, string mappedRplRecruitId : recruits.m_mRplIdToRecruit)
		{
			if (mappedRplRecruitId == recruitId)
			{
				SetFailure("BUG-004: m_mRplIdToRecruit still maps a replication id to removed recruit '%1' - RemoveRecruit() leaked the replication lookup", recruitId);
				return true;
			}
		}

		PrintFormat("Recruit removal left no stale lookups for '%1'", recruitId);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! BUG-130: despawning an offline owner's recruit must RESERVE the body - alive, tracked, hidden
//! in place - never save-and-release it.
//!
//! The old despawn wrote the character to storage, released tracking keeping the record, and
//! DELETED the entity. BUG-086 measured that a record released this way dies within minutes of
//! its entity, no restart required, so a returning owner's recruits answered NOT_FOUND and came
//! back as fresh prefabs in civilian clothes - the live-server report this bug is. Under the
//! reservation model the body must survive the despawn as the same live, tracked entity, because
//! it IS the durable store.
//!
//! WHAT IT MEASURES: a real character is spawned from the recruit prefab, recruited through the
//! public AddRecruit() API, and - once lazy registration has landed, so the id capture inside the
//! despawn is meaningful - despawned through the public DespawnPlayerRecruits(). The case then
//! asserts the body entity still exists, is still persistence-tracked, is reserved (hidden), the
//! record holds a non-empty body id, and the recruit is marked offline.
//!
//! PROVEN ABLE TO FAIL (2026-08-09): with ReserveRecruitBody() temporarily reverted to the old
//! Save()/Untrack(keepData)/delete sequence, the case reports "the body entity was destroyed".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceSuite, timeoutS: 60)]
class OVT_TEST_Persistence_RecruitDespawnReservesBody : SCR_AutotestCaseBase
{
	//! Same registration budget the Init-tier reservation and untrack cases use.
	static const int MAX_POLLS = 900;

	//! An owner uid that collides with no real player, so DespawnPlayerRecruits() touches only
	//! this case's recruit. The record is removed again on cleanup.
	static const string TEST_OWNER_UID = "OVT_TEST_BUG130_OWNER";

	protected int m_iPhase;
	protected int m_iPolls;
	protected IEntity m_Body;
	protected string m_sRecruitId;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (m_iPhase == 0)
			return SpawnAndRecruit();

		return AwaitTrackedThenDespawn();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a character from the recruit prefab and recruits it for the test owner.
	//! \return True when the case is finished, which at this phase always means a named failure.
	protected bool SpawnAndRecruit()
	{
		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		if (!recruits || recruits.m_sRecruitPrefab.IsEmpty())
		{
			SetFailure("The recruit manager has no character prefab to spawn a body from");
			return true;
		}

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.Count() < 1)
		{
			SetFailure("No towns are registered - nowhere sensible to spawn the body");
			return true;
		}

		m_Body = OVT_Global.SpawnEntityPrefab(recruits.m_sRecruitPrefab, towns.m_Towns[0].location);
		if (!m_Body)
		{
			SetFailure("SpawnEntityPrefab() produced no character from the recruit prefab");
			return true;
		}

		m_sRecruitId = recruits.AddRecruit(TEST_OWNER_UID, m_Body);
		if (m_sRecruitId.IsEmpty())
		{
			SetFailure("AddRecruit() returned no recruit ID for the spawned body");
			return FinishAndCleanUp();
		}

		m_iPhase = 1;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for lazy registration, despawns, and asserts the body survives reserved.
	//! \return True when the case is finished.
	protected bool AwaitTrackedThenDespawn()
	{
		m_iPolls += 1;

		// The despawn's id capture is a real storage write and needs the registration to have
		// landed - which in the game it always has, because recruits exist for minutes before the
		// offline timer fires. Skipping this wait would fail the id assertion for a timing reason
		// the shipped code does not have.
		if (!OVT_PersistenceTracking.IsTracked(m_Body))
		{
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("The recruit body was never persistence-tracked (%1 polls) - registration is not running, so the despawn cannot be asserted", m_iPolls.ToString());
				return FinishAndCleanUp();
			}
			return false;
		}

		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		recruits.DespawnPlayerRecruits(TEST_OWNER_UID);

		OVT_RecruitData recruit = recruits.GetRecruit(m_sRecruitId);
		if (!recruit)
		{
			SetFailure("The recruit record vanished across DespawnPlayerRecruits() - despawn must park the body, never drop the record");
			return FinishAndCleanUp();
		}

		IEntity bodyAfter = recruits.FindRecruitEntity(m_sRecruitId);
		if (!bodyAfter)
		{
			SetFailure("The body entity was destroyed (or its mapping dropped) by DespawnPlayerRecruits() - that is the save-and-release path BUG-086 proved loses the gear within minutes");
			return FinishAndCleanUp();
		}

		if (!OVT_PersistenceTracking.IsTracked(bodyAfter))
		{
			SetFailure("The despawned body is no longer persistence-tracked - an untracked body writes no record at the next save and the recruit's gear cannot survive a restart");
			return FinishAndCleanUp();
		}

		if (!OVT_PersistenceReservation.IsReserved(bodyAfter))
		{
			SetFailure("The despawned body is not reserved - an offline owner's recruit left visible and traceable is a free supply crate");
			return FinishAndCleanUp();
		}

		if (recruit.m_sBodyPersistenceId.IsEmpty())
		{
			SetFailure("The record holds no body persistence id after the despawn - the post-restart respawn would have nothing to ask for and would fall back to a fresh civilian body");
			return FinishAndCleanUp();
		}

		if (recruit.m_bIsOnline)
		{
			SetFailure("The recruit is still marked online after DespawnPlayerRecruits()");
			return FinishAndCleanUp();
		}

		PrintFormat("Recruit despawn reserved the body - alive, tracked, hidden, id %1 (settled after %2 poll(s))", recruit.m_sBodyPersistenceId, m_iPolls.ToString());
		return FinishAndCleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the test recruit record and the spawned body, whichever verdict it was.
	//! \return Always true - the case is over.
	protected bool FinishAndCleanUp()
	{
		if (!m_sRecruitId.IsEmpty())
		{
			OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
			if (recruits)
				recruits.RemoveRecruit(m_sRecruitId);
		}

		if (m_Body)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Body);

		m_Body = null;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A radio tower the save has no record for is handed to the occupying faction on load.
//!
//! WHY. Towers are world-derived: a map update can add towers that existing saves know nothing
//! about. NewGameStart() never runs on a continue, and discovery stamps tower factions before the
//! save's real occupying faction key is applied, so an unmatched tower used to keep whatever index
//! discovery guessed - and a tower that is not IsOccupyingFaction() never garrisons (CheckRadioTowers
//! skips it). This case drives ApplyPersistedOccupyingFaction(), the exact seam the serializer calls.
//!
//! TWO SCENARIOS, ONE CASE. First: the tower is dirtied to faction -1 and the apply is given an
//! EMPTY tower-record list (a save written before the tower existed) - the tower must come back
//! occupying. Second: the apply is given a record AT the tower's location carrying the player
//! faction (a legitimately captured tower) - the sweep must NOT trample it back to occupying.
//!
//! ANTI-VACUOUS: the dirty step sets a faction the campaign start never produces (-1), so an
//! occupying faction afterwards can only have come from the code under test. Live resources/threat
//! are read first and passed straight back through, so the apply mutates nothing else.
//!
//! CAN-FAIL: proven red 2026-08-13 against the pre-fix ApplyPersistedOccupyingFaction (unmatched
//! towers were simply never visited): "expected occupying faction ..., it is -1".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceSuite, timeoutS: 30)]
class OVT_TEST_Persistence_NewRadioTower_DefaultsToOccupyingFaction : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
		{
			SetFailure("OVT_Global.GetOccupyingFaction() is null");
			return true;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("OVT_Global.GetConfig() is null");
			return true;
		}

		int occupyingFaction = config.GetOccupyingFactionIndex();
		int playerFaction = config.GetPlayerFactionIndex();
		if (occupyingFaction < 0 || playerFaction < 0)
		{
			SetFailure("The config could not resolve both faction indices (player %1, occupying %2)",
				playerFaction.ToString(), occupyingFaction.ToString());
			return true;
		}

		OVT_RadioTowerData tower;
		foreach (OVT_RadioTowerData candidate : occupying.m_RadioTowers)
		{
			if (candidate)
			{
				tower = candidate;
				break;
			}
		}

		if (!tower)
		{
			SetFailure("The test world handed out no radio tower");
			return true;
		}

		int factionBefore = tower.faction;
		int resources = occupying.m_iResources;
		float threat = occupying.m_iThreat;

		// Scenario 1: the save was written before this tower existed - no record for it at all.
		tower.faction = -1;
		array<ref OVT_PersistedRadioTower> noRecords = new array<ref OVT_PersistedRadioTower>();
		occupying.ApplyPersistedOccupyingFaction(config.m_sOccupyingFaction, resources, threat, null, noRecords);

		if (tower.faction != occupyingFaction)
		{
			SetFailure("A tower absent from the save was not handed to the occupying faction: expected occupying faction %1, it is %2",
				occupyingFaction.ToString(), tower.faction.ToString());
			tower.faction = factionBefore;
			return true;
		}

		// Scenario 2: the save DOES know the tower, as player-captured - the sweep must not trample it.
		array<ref OVT_PersistedRadioTower> capturedRecords = new array<ref OVT_PersistedRadioTower>();
		OVT_PersistedRadioTower record = new OVT_PersistedRadioTower();
		record.location = tower.location;
		record.faction = playerFaction;
		record.disabledRemaining = 0;
		capturedRecords.Insert(record);
		occupying.ApplyPersistedOccupyingFaction(config.m_sOccupyingFaction, resources, threat, null, capturedRecords);

		if (tower.faction != playerFaction)
		{
			SetFailure("A player-captured tower with a save record was trampled: expected player faction %1, it is %2",
				playerFaction.ToString(), tower.faction.ToString());
			tower.faction = factionBefore;
			return true;
		}

		// Restore whatever the campaign start left the tower on.
		tower.faction = factionBefore;

		PrintFormat("New-tower default round-trip: unmatched tower -> occupying %1, recorded capture kept player %2",
			occupyingFaction.ToString(), playerFaction.ToString());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A base the save has no record for is handed to the occupying faction on load.
//!
//! WHY. The exact same gap the towers case above guards, on the bases side: bases are world-derived
//! (flagpole prefabs in bases.layer), so a map update can add one that existing saves know nothing
//! about. NewGameStart() never runs on a continue, and discovery stamps base factions before the
//! save's real occupying faction key is applied, so an unmatched base used to keep whatever index
//! discovery guessed - and never garrisoned or spent resources as the occupying faction.
//!
//! TWO SCENARIOS, ONE CASE. First: the base is dirtied to faction -1 and the apply is given an
//! EMPTY base-record list (a save written before the base existed) - the base must come back
//! occupying. Second: the apply is given a record AT the base's location carrying the player
//! faction (a legitimately captured base) - the sweep must NOT trample it back to occupying.
//!
//! SIDE EFFECT, ACCEPTED. Applying the scenario-2 record clears the base's persisted-data lists
//! (upgrades/slots/garrison records). Those lists are rebuilt from the LIVE controller at save time
//! and are empty during a test-world session anyway (garrisons are never populated here), so
//! nothing downstream reads what this case clears.
//!
//! CAN-FAIL: proven red 2026-08-13 against the pre-fix ApplyPersistedOccupyingFaction (unmatched
//! bases were simply never visited): "expected occupying faction ..., it is -1".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_PersistenceSuite, timeoutS: 30)]
class OVT_TEST_Persistence_NewBase_DefaultsToOccupyingFaction : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
		{
			SetFailure("OVT_Global.GetOccupyingFaction() is null");
			return true;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("OVT_Global.GetConfig() is null");
			return true;
		}

		int occupyingFaction = config.GetOccupyingFactionIndex();
		int playerFaction = config.GetPlayerFactionIndex();
		if (occupyingFaction < 0 || playerFaction < 0)
		{
			SetFailure("The config could not resolve both faction indices (player %1, occupying %2)",
				playerFaction.ToString(), occupyingFaction.ToString());
			return true;
		}

		OVT_BaseData base;
		foreach (OVT_BaseData candidate : occupying.m_Bases)
		{
			if (candidate)
			{
				base = candidate;
				break;
			}
		}

		if (!base)
		{
			SetFailure("The test world handed out no base");
			return true;
		}

		int factionBefore = base.faction;
		int resources = occupying.m_iResources;
		float threat = occupying.m_iThreat;

		// Scenario 1: the save was written before this base existed - no record for it at all.
		base.faction = -1;
		array<ref OVT_PersistedBase> noRecords = new array<ref OVT_PersistedBase>();
		occupying.ApplyPersistedOccupyingFaction(config.m_sOccupyingFaction, resources, threat, noRecords, null);

		if (base.faction != occupyingFaction)
		{
			SetFailure("A base absent from the save was not handed to the occupying faction: expected occupying faction %1, it is %2",
				occupyingFaction.ToString(), base.faction.ToString());
			base.faction = factionBefore;
			return true;
		}

		// Scenario 2: the save DOES know the base, as player-captured - the sweep must not trample it.
		array<ref OVT_PersistedBase> capturedRecords = new array<ref OVT_PersistedBase>();
		OVT_PersistedBase record = new OVT_PersistedBase();
		record.location = base.location;
		record.faction = playerFaction;
		capturedRecords.Insert(record);
		occupying.ApplyPersistedOccupyingFaction(config.m_sOccupyingFaction, resources, threat, capturedRecords, null);

		if (base.faction != playerFaction)
		{
			SetFailure("A player-captured base with a save record was trampled: expected player faction %1, it is %2",
				playerFaction.ToString(), base.faction.ToString());
			base.faction = factionBefore;
			return true;
		}

		// Restore whatever the campaign start left the base on.
		base.faction = factionBefore;

		PrintFormat("New-base default round-trip: unmatched base -> occupying %1, recorded capture kept player %2",
			occupyingFaction.ToString(), playerFaction.ToString());
		return true;
	}
}
