//------------------------------------------------------------------------------------------------
//! A CONTINUED CAMPAIGN MUST RE-ESTABLISH THE RUNTIME-ID -> PERSISTENT-ID MAPPING OF EVERY PLAYER
//! WHO IS ALREADY CONNECTED.
//!
//! WHAT BROKE, AND WHY NOTHING CAUGHT IT. SetupPlayer() - which writes the session ID maps, sets the
//! record's runtime id and spawns the player's OVT_OverthrowController - is called from exactly one
//! place: OVT_SpawnLogic.DoSpawn_S. That runs when a player CONNECTS. Loading a save point does not
//! make anyone connect: it REPLACES THE WORLD, so the player manager and its maps are brand new while
//! the DoSpawn_S that mapped the player ran in the world instance that was just thrown away. The
//! player is still connected throughout, so nothing re-runs it, and PrepareConnectedPlayers() used to
//! assume otherwise ("SetupPlayer was already called in DoSpawn_S").
//!
//! THE SYMPTOM IS MONEY, AND IT IS THE ASSERTION THIS CASE MAKES. The HUD reads a balance by composing
//! exactly two calls - GetPersistentIDFromPlayerID(runtime id) then GetPlayerMoney(persistent id)
//! (OVT_EconomyInfo.c:66,318) - so an empty mapping makes a player with a six-figure balance read $0
//! while their record is perfectly intact in the save. Observed 2026-08-08, Workbench SP:
//! money 100,000 written to savepoint000, $0 on the screen after Continue. Every other playerId-keyed
//! lookup misses the same way, and the player gets no controller at all.
//!
//! WHY IT IS EXERCISED THROUGH ClearPlayerIdMappings() RATHER THAN A REAL LOAD. A genuine continue is
//! a world transition, and a world transition restarts the autotest harness mid-run - the reason the
//! persistence tier round-trips through an in-session re-apply instead. ClearPlayerIdMappings() is the
//! same public API the disconnect path uses and produces the identical observable state: a connected
//! player the manager cannot resolve. What it does NOT reproduce is the rest of a world rebuild
//! (a new manager instance, a lost controller); those ride on the same repair and stay play-test
//! territory.
//!
//! THE SYMPTOM IS ASSERTED BEFORE THE FIX IS. Between clearing the mapping and repairing it, the case
//! requires the read to actually come back 0. Without that step a mapping repair that silently never
//! ran would still pass, because the balance would never have been unreadable in the first place.
//!
//! CAMPAIGN TIER ON PURPOSE. PrepareConnectedPlayers() finalizes any player the game mode has not
//! finalized yet - assigning a home, spawning a starting car, granting starting cash. In a STARTED
//! campaign the local player is already in m_aInitializedPlayers, so the only thing this case can
//! provoke is the mapping repair itself. In the Init tier (campaign deliberately not started) the same
//! call would hand the world a house and a car, and every case after it would inherit them. The
//! precondition below asserts that finalized state rather than trusting it.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 30)]
class OVT_TEST_Campaign_ContinuePlayerIdMapping : SCR_AutotestCaseBase
{
	//! Distinctive balance written onto the live record for the duration of the case. Nothing in the
	//! campaign produces this number, so a read that returns it can only have gone through the mapping.
	static const int PROBE_MONEY = 987654;

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if (!mode)
		{
			SetResultFailure("The running game mode is not an OVT_OverthrowGameMode");
			return true;
		}

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!players || !economy)
		{
			SetResultFailure("OVT_Global.GetPlayers() or GetEconomy() is null - see OVT_TEST_Init_Globals_ManagersResolve");
			return true;
		}

		array<int> connected = {};
		GetGame().GetPlayerManager().GetPlayers(connected);
		if (connected.IsEmpty())
		{
			SetResultFailure("No player is connected - the autotest client normally spawns a local player (playerId 1), and this case is about a CONNECTED player surviving a continue");
			return true;
		}

		int playerId = connected[0];

		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if (persId == "")
		{
			SetResultFailure("The player manager already has no persistent ID for playerId %1 before this case touched anything - SetupPlayer never ran for the connected player", playerId.ToString());
			return true;
		}

		OVT_PlayerData record = players.GetPlayer(persId);
		if (!record)
		{
			SetResultFailure("No OVT_PlayerData record for persistent ID %1", persId);
			return true;
		}

		// The finalized-state precondition this case's header explains: without it, the
		// PrepareConnectedPlayers() call below would run full player preparation and leave the campaign
		// with a house, a car and a cash grant that later cases would inherit.
		if (!mode.m_aInitializedPlayers || !mode.m_aInitializedPlayers.Contains(persId))
		{
			SetResultFailure("Player %1 is not in the game mode's finalized set, so calling PrepareConnectedPlayers() here would assign a home, spawn a starting car and grant starting cash into a shared campaign. The campaign start should have finalized them.", persId);
			return true;
		}

		int originalMoney = record.money;
		record.money = PROBE_MONEY;

		// Precondition: the read path works WHILE the mapping is present. If this is not the probe, the
		// case cannot attribute a later 0 to the mapping.
		int mapped = MoneyThroughTheIdMapping(players, economy, playerId);
		if (mapped != PROBE_MONEY)
		{
			record.money = originalMoney;
			SetResultFailure("With the mapping intact, the balance read back as %1 instead of the probe %2 - this case cannot measure the mapping through a read path that is already broken", mapped.ToString(), PROBE_MONEY.ToString());
			return true;
		}

		// Stand in for the world rebuild: a connected player the manager can no longer resolve.
		players.ClearPlayerIdMappings(playerId);

		if (players.GetPersistentIDFromPlayerID(playerId) != "")
		{
			record.money = originalMoney;
			SetResultFailure("ClearPlayerIdMappings() left a mapping in place for playerId %1, so the continued-session state this case is about was never reproduced", playerId.ToString());
			return true;
		}

		// The symptom itself, asserted before the repair.
		int unmapped = MoneyThroughTheIdMapping(players, economy, playerId);
		if (unmapped != 0)
		{
			players.SetupPlayer(playerId, persId);
			record.money = originalMoney;
			SetResultFailure("An unmapped player's balance read back as %1 rather than 0, so this case would prove nothing about the repair below", unmapped.ToString());
			return true;
		}

		mode.PrepareConnectedPlayers();

		string remapped = players.GetPersistentIDFromPlayerID(playerId);
		if (remapped != persId)
		{
			// Leave the world usable for the cases that run after this one.
			players.SetupPlayer(playerId, persId);
			record.money = originalMoney;

			SetResultFailure("PrepareConnectedPlayers() did NOT restore the session ID mapping for playerId %1 (got '%2', expected '%3'). After a Continue this is the whole session's state: the HUD shows $0 for a player whose record holds their real balance, every playerId-keyed lookup misses, and no OVT_OverthrowController is ever spawned for them.",
				playerId.ToString(), remapped, persId);
			return true;
		}

		int repaired = MoneyThroughTheIdMapping(players, economy, playerId);
		int reverseId = players.GetPlayerIDFromPersistentID(persId);
		int moneyAfter = record.money;

		record.money = originalMoney;

		if (repaired != PROBE_MONEY)
		{
			SetResultFailure("The mapping came back but the balance read through it is %1, not the probe %2", repaired.ToString(), PROBE_MONEY.ToString());
			return true;
		}

		if (reverseId != playerId)
		{
			SetResultFailure("The reverse mapping was not restored: GetPlayerIDFromPersistentID('%1') answered %2, expected %3", persId, reverseId.ToString(), playerId.ToString());
			return true;
		}

		// A returning player must not be treated as a new one. FinalizePlayerPreparation grants starting
		// cash to a record whose initialized flag is false, and the repair must not open that path.
		if (moneyAfter != PROBE_MONEY)
		{
			SetResultFailure("The repair changed the player's balance from %1 to %2 - a continued campaign re-ran new-player preparation", PROBE_MONEY.ToString(), moneyAfter.ToString());
			return true;
		}

		PrintFormat("A connected player with no session ID mapping was remapped to %1 and their balance reads back through it", persId);
		SetResultSuccess();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads a player's balance the way the HUD does: runtime ID -> persistent ID -> record.
	//! \param[in] players The player manager holding the session ID maps.
	//! \param[in] economy The economy manager that reads the record.
	//! \param[in] playerId The runtime player ID.
	//! \return The balance, or 0 when the runtime ID cannot be resolved to a record.
	protected int MoneyThroughTheIdMapping(notnull OVT_PlayerManagerComponent players, notnull OVT_EconomyManagerComponent economy, int playerId)
	{
		return economy.GetPlayerMoney(players.GetPersistentIDFromPlayerID(playerId));
	}
}
