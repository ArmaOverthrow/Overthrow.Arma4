//------------------------------------------------------------------------------------------------
//! A CONTINUED CAMPAIGN MUST GIVE EVERY ALREADY-CONNECTED PLAYER A LIVE OVT_OverthrowController BACK,
//! NOT MERELY THEIR ID MAPPING.
//!
//! WHY THIS IS A SEPARATE CLAIM FROM ITS SIBLING. OVT_TEST_Campaign_ContinuePlayerIdMapping asserts
//! that PrepareConnectedPlayers() restores the runtime-id -> persistent-id mapping, and its failure
//! message merely MENTIONS the controller as a consequence. That is not the same claim: the mapping is
//! plain map state, while the controller is a spawned, replicated ENTITY whose ownership has to be
//! handed to a client. The controller is where every client->server request in the mod is heading
//! (core/controller-migration moves ~50 of them onto it), so "the seam exists after a Continue" is
//! about to be load-bearing for the entire mod rather than for one HUD number.
//!
//! CONTINUE IS NOT CONNECT, WHICH IS THE WHOLE HAZARD. SetupPlayer() - the only thing that spawns a
//! controller - is called from OVT_SpawnLogic.DoSpawn_S, which runs when a player CONNECTS. Loading a
//! save point replaces the world without anyone connecting: the player manager and every entity in it
//! are new, the DoSpawn_S that ran for this player belongs to the world that was thrown away, and
//! nothing re-runs it. The repair is the SetupPlayer() call inside PrepareConnectedPlayers()
//! (OVT_OverthrowGameMode.c) - and this case is the only automated thing that watches it.
//! A DEDICATED SERVER IS IMMUNE (it Continues at boot, before anyone connects), so a green MP round
//! proves nothing here; the listen-host play-test is still owed on top of this case.
//!
//! HOW THE STATE IS REPRODUCED, AND WHY NOT WITH A REAL LOAD. A genuine continue is a world
//! transition, and a world transition restarts the autotest harness mid-run (the same reason the
//! persistence tier round-trips in-session). Instead the case drives the two PUBLIC manager APIs the
//! disconnect path already uses - ClearPlayerIdMappings() and CleanupPlayerController() - to produce
//! the identical observable state: a still-connected player with no ID mapping and no controller
//! entity. What it does not reproduce is a brand-new manager instance; that rides on the same repair.
//!
//! THE SYMPTOM IS ASSERTED BEFORE THE REPAIR IS. Between the teardown and PrepareConnectedPlayers()
//! the case requires GetController() to actually answer null. Without that step, a repair that
//! silently never ran would still pass - the controller would simply never have been missing.
//!
//! IT DELIBERATELY DOES NOT COMPARE ENTITY HANDLES before and after. A freed entity's address can in
//! principle be reused, and a case that could flake on an allocator detail is worse than one that
//! asserts slightly less. Non-null AND not-deleted is the property every caller actually depends on.
//!
//! CAMPAIGN TIER, and the finalized-state precondition below is why: PrepareConnectedPlayers() runs
//! full player preparation (a home, a starting car, a cash grant) for anyone the game mode has not
//! finalized. In a started campaign the local player is already finalized, so the only thing this case
//! can provoke is the repair itself.
//!
//! PROVEN ABLE TO FAIL (2026-08-14): the SetupPlayer() repair inside PrepareConnectedPlayers()
//! (OVT_OverthrowGameMode.c, the `has no session ID mapping (continued campaign)` block) was
//! neutralised by short-circuiting its condition to `if (false && m_PlayerManager && ...)`, which
//! disables the call without touching the block. The case then failed with
//! "PrepareConnectedPlayers() did NOT give player 1 a controller back."; condition restored, it passes.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 30)]
class OVT_TEST_Campaign_ContinueControllerRespawn : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if (!mode)
		{
			SetFailure("The running game mode is not an OVT_OverthrowGameMode");
			return true;
		}

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
		{
			SetFailure("OVT_Global.GetPlayers() is null - see OVT_TEST_Init_Globals_ManagersResolve");
			return true;
		}

		array<int> connected = {};
		GetGame().GetPlayerManager().GetPlayers(connected);
		if (connected.IsEmpty())
		{
			SetFailure("No player is connected - this case is about a CONNECTED player keeping their controller across a continue");
			return true;
		}

		int playerId = connected[0];

		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if (persId == "")
		{
			SetFailure("The player manager has no persistent ID for playerId %1 before this case touched anything - SetupPlayer never ran for the connected player", playerId.ToString());
			return true;
		}

		// Same precondition as the sibling mapping case: without it the PrepareConnectedPlayers() call
		// below would assign a home, spawn a starting car and grant starting cash into a shared campaign.
		if (!mode.m_aInitializedPlayers || !mode.m_aInitializedPlayers.Contains(persId))
		{
			SetFailure("Player %1 is not in the game mode's finalized set, so calling PrepareConnectedPlayers() here would run full new-player preparation into a shared campaign. The campaign start should have finalized them.", persId);
			return true;
		}

		// Precondition: the seam works BEFORE the teardown. If it does not, a null afterwards proves
		// nothing about the repair.
		if (!players.GetController(playerId))
		{
			SetFailure("Player %1 has no OVT_OverthrowController even before this case tore anything down, so the repair below cannot be measured. SetupPlayer() either never spawned one or the controller prefab is unset on the game mode.", playerId.ToString());
			return true;
		}

		// Stand in for the world rebuild: a still-connected player with neither mapping nor controller.
		players.ClearPlayerIdMappings(playerId);
		players.CleanupPlayerController(playerId);

		// The symptom itself, asserted before the repair.
		if (players.GetController(playerId))
		{
			RepairWorld(players, playerId, persId);
			SetFailure("CleanupPlayerController() left a controller registered for playerId %1, so the continued-session state this case is about was never reproduced", playerId.ToString());
			return true;
		}

		if (players.GetPersistentIDFromPlayerID(playerId) != "")
		{
			RepairWorld(players, playerId, persId);
			SetFailure("ClearPlayerIdMappings() left a mapping in place for playerId %1 - PrepareConnectedPlayers() only repairs a player whose mapping is empty, so the repair path would not even be entered", playerId.ToString());
			return true;
		}

		mode.PrepareConnectedPlayers();

		OVT_OverthrowController controller = players.GetController(playerId);
		if (!controller)
		{
			RepairWorld(players, playerId, persId);
			SetFailure("PrepareConnectedPlayers() did NOT give player %1 a controller back. After a Continue that is the whole session: every client->server request rides the controller entity, so shop purchases, camp and recruit menus, fast travel and respawn requests are all inert for the rest of the session, with no error anywhere.", playerId.ToString());
			return true;
		}

		if (controller.IsDeleted())
		{
			RepairWorld(players, playerId, persId);
			SetFailure("PrepareConnectedPlayers() left player %1 mapped to a DELETED controller entity. A stale map entry answers non-null to a Contains() check while every component lookup on it fails, which is worse than having none.", playerId.ToString());
			return true;
		}

		// The client-side accessor, not just the server-side map. This is the call every migrated
		// request actually makes, and it has its own two-step fallback that must survive a Continue.
		if (playerId == SCR_PlayerController.GetLocalPlayerId() && !OVT_Global.GetController())
		{
			SetFailure("The player manager has a controller for playerId %1 but OVT_Global.GetController() answers null, so no client->server request can find it. The accessor's controlled-entity route and its local-player-id fallback both missed a controller that demonstrably exists.", playerId.ToString());
			return true;
		}

		PrintFormat("A connected player with no mapping and no controller got a live OVT_OverthrowController back from PrepareConnectedPlayers() (persistent id %1)", persId);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the player's mapping and controller back after a failure, so the cases that run after this
	//! one inherit a working world rather than this case's teardown.
	//! \param[in] players The player manager.
	//! \param[in] playerId The runtime player id.
	//! \param[in] persId The player's persistent id.
	protected void RepairWorld(notnull OVT_PlayerManagerComponent players, int playerId, string persId)
	{
		players.SetupPlayer(playerId, persId);
	}
}
