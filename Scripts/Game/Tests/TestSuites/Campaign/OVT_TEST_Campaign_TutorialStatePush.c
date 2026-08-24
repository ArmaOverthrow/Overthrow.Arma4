//------------------------------------------------------------------------------------------------
//! The server's per-player tutorial-state push reaches the owning client's mirror.
//!
//! WHAT THIS PINS, AND WHY IT EXISTS. A client decides for itself whether to show a tip whose
//! trigger is client-local (MENU_OPENED, MAP_OPENED, PLAYER_SPAWNED - ten of the shipped entries),
//! and it decides using an in-memory mirror that starts EMPTY every session. The campaign's durable
//! record lives on the server, so the ONLY thing that stops those tips repeating on every login is
//! OVT_OverthrowGameMode.PushTutorialStateToPlayer() landing on that client. Until 2026-08-24 the
//! push rode SetPlayerSpawnContext(), which runs only on the branches that hand out a home - so
//! every returning player was pushed nothing and read all ten tips again on every single login.
//!
//! WHAT IT DOES NOT PIN: that FinalizePlayerPreparation still CALLS the push. Both call sites are
//! on paths this tier cannot re-enter without teleporting and respawning the autotest player. This
//! case covers the half that is observable - resolve the controller, take the local-direct branch
//! (an RPC is never looped back to its sender), merge into the mirror - and the call sites are
//! covered by the MP play-test protocol.
//!
//! The sentinel id is deliberately not an authored entry: nothing may act on it, and it must not
//! collide with a real dismissal made by another case in this suite.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 30)]
class OVT_TEST_Campaign_Tutorial_StatePushReachesClient : SCR_AutotestCaseBase
{
	protected static const string SENTINEL_ID = "test-state-push-sentinel";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OverthrowGameMode mode = OVT_Global.GetOverthrow();
		if (!mode)
		{
			SetFailure("OVT_Global.GetOverthrow() is null in a campaign-tier case");
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
			SetFailure("No player is connected - the autotest client normally spawns a local player, so there is no client mirror to push to and this case cannot measure anything");
			return true;
		}

		int playerId = connected[0];

		OVT_PlayerData player = OVT_PlayerData.Get(playerId);
		if (!player)
		{
			SetFailure("No OVT_PlayerData for playerId %1, so SetupPlayer never ran and the campaign has no record to push", playerId.ToString());
			return true;
		}

		OVT_OverthrowController controller = players.GetController(playerId);
		if (!controller)
		{
			SetFailure("Player %1 has no registered OVT_OverthrowController, so nothing on this machine can carry the push. This is the async owner-assignment race - see OVT_TEST_Init_ControllerSeam.", playerId.ToString());
			return true;
		}

		OVT_TutorialComponent tutorials = OVT_TutorialComponent.Cast(controller.FindComponent(OVT_TutorialComponent));
		if (!tutorials)
		{
			SetFailure("The player's OVT_OverthrowController carries no OVT_TutorialComponent. It is registered on Prefabs/GameMode/OVT_OverthrowController.et; without it no tutorial state can reach any client and no tip can ever be shown.");
			return true;
		}

		if (tutorials.HasSeenLocally(SENTINEL_ID))
		{
			SetFailure("The client mirror already holds the sentinel '%1' before this case seeded anything, so a pass below would prove nothing. Another case in this suite is using the same id.", SENTINEL_ID);
			return true;
		}

		// Seed the SERVER's record, which is what a dismissal writes and what the save carries.
		if (!player.m_aSeenTutorials)
			player.m_aSeenTutorials = new array<string>();

		if (!player.m_aSeenTutorials.Contains(SENTINEL_ID))
			player.m_aSeenTutorials.Insert(SENTINEL_ID);

		mode.PushTutorialStateToPlayer(playerId);

		bool landed = tutorials.HasSeenLocally(SENTINEL_ID);

		// Leave the record as it was found, whatever the verdict: this runs against a live campaign
		// that later cases and the save both read.
		player.m_aSeenTutorials.RemoveItem(SENTINEL_ID);

		if (!landed)
		{
			SetFailure("PushTutorialStateToPlayer(%1) did not reach the owning client's mirror: the sentinel '%2' was on the campaign record and is not in the client's seen store. On a listen host or in single player the push must take PushTutorialState()'s local-direct branch, because the engine never loops an RPC back to the machine that sent it. With this broken, every client-local tip (the map, the home menu, the skills screen, the welcome) re-fires on every login even though the campaign save remembers them.",
				playerId.ToString(), SENTINEL_ID);
			return true;
		}

		PrintFormat("The server's tutorial-state push reached the local client's mirror (sentinel '%1'), so a returning player's dismissed tips stay dismissed", SENTINEL_ID);
		return true;
	}
}
