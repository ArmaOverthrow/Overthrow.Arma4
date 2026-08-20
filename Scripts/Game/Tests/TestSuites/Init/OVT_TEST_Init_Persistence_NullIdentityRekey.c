//------------------------------------------------------------------------------------------------
//! TIER B - the NULL-UUID identity corruption class: the tripwire refuses it, the rekey moves it.
//!
//! THE BUG. The backend can transiently answer a player-identity lookup with the NULL UUID
//! ("00000000-...") instead of an empty string - observed in a Workbench session 2026-08-20, where
//! the engine's own "### Updating player" line already carried the zero id. The zero form is
//! non-empty, so it defeated vanilla's name-hash fallback (fires only on EMPTY) and Overthrow's
//! own empty-uid guards, and the whole campaign - player record, house, vehicle, stored body - was
//! keyed to an identity no session can ever present again. The next (healthy) session prepared the
//! player as brand new, and the reservation sweep hid their real body forever as "offline".
//!
//! WHAT IS ASSERTED HERE, and what is not. Three of the fix's four pieces are deterministic and
//! synchronous, so they are asserted directly:
//!   1. SetupPlayer() REFUSES the NULL UUID - neither the record map nor the id maps learn it;
//!   2. the per-manager RekeyPlayerPersistentId() moves zero-keyed data to a real id (player
//!      record, ownership maps, vehicle registry rows, recruits, camps) and leaves nothing behind;
//!   3. rekey never clobbers: a record already sitting under the new id stays untouched.
//! NOT asserted: OVT_Global.GetPlayerUID()'s recovery chain (it reads the live backend, which a
//! test cannot make answer the zero id) and TryAdoptNullIdentityRecords' local-player guard chain
//! (the autotest world's observer is not a connected local player) - both are play-test territory
//! and say so in the fix's verification notes.
//!
//! WHY THIS TIER. Every manager exists after world load without a started campaign, and the case
//! only writes its own synthetic keys, which it removes again - the shared Init world keeps no
//! trace of it either way.
//!
//! PROVEN ABLE TO FAIL 2026-08-20: with SetupPlayer's tripwire disabled (`if (false && ...)`) the
//! case went red on "SetupPlayer(4242, NULL_UUID) created a player record keyed to the zero id -
//! the tripwire is gone" (Init suite 1 of 26 failing); tripwire restored, suite green again.
//! No polling, no timing, no async step anywhere in the case - no maxAttempts.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_Persistence_NullIdentityRekey : SCR_AutotestCaseBase
{
	//! The zero id exactly as the backend hands it out and exactly as it sat in the decoded save.
	protected static const string NULL_ID = "00000000-0000-0000-0000-000000000000";

	//! Synthetic adopter id. Not UUID-shaped on purpose - it can never collide with real data.
	protected static const string TEST_ID = "OVT_TEST_NULL_REKEY_TARGET";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
		{
			SetFailure("OVT_Global.GetPlayers() is null");
			return true;
		}

		// --- 1. The tripwire: SetupPlayer must refuse to key anything to the NULL UUID ---------------
		players.SetupPlayer(4242, NULL_ID);

		if (players.m_mPlayers.Contains(NULL_ID))
		{
			players.m_mPlayers.Remove(NULL_ID);
			SetFailure("SetupPlayer(4242, NULL_UUID) created a player record keyed to the zero id - the tripwire is gone");
			return true;
		}

		if (players.GetPersistentIDFromPlayerID(4242) != "")
		{
			SetFailure("SetupPlayer(4242, NULL_UUID) mapped runtime id 4242 to '%1' - the tripwire is gone",
				players.GetPersistentIDFromPlayerID(4242));
			return true;
		}

		// --- 2. Player manager rekey moves the record and leaves nothing behind ----------------------
		OVT_PlayerData orphan = new OVT_PlayerData();
		orphan.money = 424242;
		players.m_mPlayers[NULL_ID] = orphan;

		players.RekeyPlayerPersistentId(NULL_ID, TEST_ID);

		bool moved = players.m_mPlayers.Contains(TEST_ID) && players.m_mPlayers[TEST_ID].money == 424242;
		bool cleaned = !players.m_mPlayers.Contains(NULL_ID);
		players.m_mPlayers.Remove(TEST_ID);
		players.m_mPlayers.Remove(NULL_ID);

		if (!moved)
		{
			SetFailure("Player-manager rekey did not move the zero-keyed record to the new id (or lost its contents)");
			return true;
		}
		if (!cleaned)
		{
			SetFailure("Player-manager rekey left the record under the zero id as well - the orphan would be adopted twice");
			return true;
		}

		// --- 3. Rekey never clobbers an existing record under the new id -----------------------------
		OVT_PlayerData existing = new OVT_PlayerData();
		existing.money = 111;
		OVT_PlayerData lateOrphan = new OVT_PlayerData();
		lateOrphan.money = 222;
		players.m_mPlayers[TEST_ID] = existing;
		players.m_mPlayers[NULL_ID] = lateOrphan;

		players.RekeyPlayerPersistentId(NULL_ID, TEST_ID);

		bool kept = players.m_mPlayers.Contains(TEST_ID) && players.m_mPlayers[TEST_ID].money == 111;
		players.m_mPlayers.Remove(TEST_ID);
		players.m_mPlayers.Remove(NULL_ID);

		if (!kept)
		{
			SetFailure("Rekey overwrote a record that already existed under the new id - adoption must never merge silently");
			return true;
		}

		// --- 4. Real estate: key map and reverse map both follow -------------------------------------
		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if (!realEstate)
		{
			SetFailure("OVT_Global.GetRealEstate() is null");
			return true;
		}

		array<string> positions = {"<1234 0 5678>"};
		realEstate.m_mOwned[NULL_ID] = positions;
		realEstate.m_mOwners["<1234 0 5678>"] = NULL_ID;

		realEstate.RekeyPlayerPersistentId(NULL_ID, TEST_ID);

		bool ownedMoved = realEstate.m_mOwned.Contains(TEST_ID) && !realEstate.m_mOwned.Contains(NULL_ID);
		bool ownerMoved = realEstate.m_mOwners["<1234 0 5678>"] == TEST_ID;
		realEstate.m_mOwned.Remove(TEST_ID);
		realEstate.m_mOwned.Remove(NULL_ID);
		realEstate.m_mOwners.Remove("<1234 0 5678>");

		if (!ownedMoved)
		{
			SetFailure("Real-estate rekey did not move the owned-buildings list off the zero id");
			return true;
		}
		if (!ownerMoved)
		{
			SetFailure("Real-estate rekey did not update the position->owner reverse map - the house would still answer to the zero id");
			return true;
		}

		// --- 5. Vehicle registry: the persisted row's owner field follows ----------------------------
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		if (!vehicles)
		{
			SetFailure("OVT_Global.GetVehicles() is null");
			return true;
		}

		OVT_PersistedPlayerVehicle row = new OVT_PersistedPlayerVehicle();
		row.persistentId = "OVT_TEST_NULL_REKEY_VEHICLE";
		row.ownerUid = NULL_ID;
		vehicles.GetVehicleRecords()["OVT_TEST_NULL_REKEY_VEHICLE"] = row;

		vehicles.RekeyPlayerPersistentId(NULL_ID, TEST_ID);

		bool rowMoved = vehicles.GetVehicleRecords()["OVT_TEST_NULL_REKEY_VEHICLE"].ownerUid == TEST_ID;
		vehicles.GetVehicleRecords().Remove("OVT_TEST_NULL_REKEY_VEHICLE");

		if (!rowMoved)
		{
			SetFailure("Vehicle-registry rekey did not update the persisted row's ownerUid - the rebuilt car would belong to the zero id");
			return true;
		}

		// --- 6. Recruits: owner index and the recruit's own owner field follow -----------------------
		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if (!recruits)
		{
			SetFailure("OVT_Global.GetRecruits() is null");
			return true;
		}

		OVT_RecruitData recruit = new OVT_RecruitData();
		recruit.m_sRecruitId = "OVT_TEST_NULL_REKEY_RECRUIT";
		recruit.m_sOwnerPersistentId = NULL_ID;
		recruits.m_mRecruits["OVT_TEST_NULL_REKEY_RECRUIT"] = recruit;
		array<string> recruitIds = {"OVT_TEST_NULL_REKEY_RECRUIT"};
		recruits.m_mRecruitsByOwner[NULL_ID] = recruitIds;

		recruits.RekeyPlayerPersistentId(NULL_ID, TEST_ID);

		bool indexMoved = recruits.m_mRecruitsByOwner.Contains(TEST_ID) && !recruits.m_mRecruitsByOwner.Contains(NULL_ID);
		bool fieldMoved = recruits.m_mRecruits["OVT_TEST_NULL_REKEY_RECRUIT"].m_sOwnerPersistentId == TEST_ID;
		recruits.m_mRecruits.Remove("OVT_TEST_NULL_REKEY_RECRUIT");
		recruits.m_mRecruitsByOwner.Remove(TEST_ID);
		recruits.m_mRecruitsByOwner.Remove(NULL_ID);

		if (!indexMoved)
		{
			SetFailure("Recruit rekey did not move the by-owner index off the zero id");
			return true;
		}
		if (!fieldMoved)
		{
			SetFailure("Recruit rekey did not update the recruit's own owner field");
			return true;
		}

		// --- 7. Camps follow too ----------------------------------------------------------------------
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance)
		{
			SetFailure("OVT_Global.GetResistanceFaction() is null");
			return true;
		}

		OVT_CampData camp = new OVT_CampData();
		camp.persistentId = "OVT_TEST_NULL_REKEY_CAMP";
		camp.owner = NULL_ID;
		resistance.m_Camps.Insert(camp);

		resistance.RekeyPlayerPersistentId(NULL_ID, TEST_ID);

		bool campMoved = camp.owner == TEST_ID;
		resistance.m_Camps.RemoveItem(camp);

		if (!campMoved)
		{
			SetFailure("Camp rekey did not update the camp's owner - a private camp would stay locked to the zero id");
			return true;
		}

		Print("Null-identity guards: SetupPlayer refuses the NULL UUID, and rekey moves player/real-estate/vehicle/recruit/camp data cleanly");
		return true;
	}
}
