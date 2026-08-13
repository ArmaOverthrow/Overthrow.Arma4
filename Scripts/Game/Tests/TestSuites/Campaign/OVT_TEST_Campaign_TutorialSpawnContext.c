//------------------------------------------------------------------------------------------------
//! The server authored a spawn context for the player it finalized.
//!
//! WHAT THIS PINS. Which of the two welcomes a player reads is decided by one fact that ONLY the
//! server knows: whether player preparation gave them a house, a car and starting cash, or sent them
//! to a fallback spawn with none of it. The client cannot derive it - a home position is set on BOTH
//! branches, and the join-time player snapshot predates finalization entirely - so the fact is
//! captured in OVT_OverthrowGameMode.FinalizePlayerPreparation and cached per persistent id.
//!
//! Everything downstream of that cache is UI and network, and neither is assertable here. The cache
//! itself is, and it is the one link in the chain that a refactor can quietly cut: delete either
//! SetPlayerSpawnContext call and the code still compiles, the campaign still starts, the welcome
//! still fires, and every player silently reads the house page - including the one standing at a bus
//! stop with nothing. This case is what goes red on that day, and it is the reason the server-side
//! map is not YAGNI.
//!
//! WHY "ONE OF THE TWO", NEVER "house". Which branch a start takes is a property of the world's
//! available starting houses, not of this feature, and a RETURNING player legitimately gets neither
//! branch and an empty context. The assertion is therefore that the finalized new player got a
//! REAL context rather than nothing - the distinction between "the code ran" and "the code did not".
//! Hardcoding "house" would make this case a hostage to the test world's house count.
//!
//! It cannot live in the Init tier: the campaign is never started there, FinalizePlayerPreparation
//! never runs, and the context is legitimately still unwritten.
//!
//! PROVEN ABLE TO FAIL 2026-08-09: see the Proven-Red Table in
//! docs/features/new-player-experience/first-spawn/context.md.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 30)]
class OVT_TEST_Campaign_Tutorial_SpawnContextIsAuthored : SCR_AutotestCaseBase
{
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

		if (!mode.HasGameStarted())
		{
			SetFailure("HasGameStarted() is false in a campaign-tier case - see OVT_TEST_Campaign_GameMode_IsStartedAndInitialized");
			return true;
		}

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
		{
			SetFailure("OVT_Global.GetPlayers() is null - see OVT_TEST_Init_Globals_ManagersResolve");
			return true;
		}

		// Precondition, not the assertion: somebody has to have been prepared for a context to exist.
		array<int> connected = {};
		GetGame().GetPlayerManager().GetPlayers(connected);
		if (connected.IsEmpty())
		{
			SetFailure("No player is connected - the autotest client normally spawns a local player (playerId 1), so player preparation never ran for anybody and this case cannot measure anything");
			return true;
		}

		int playerId = connected[0];

		string persistentId = players.GetPersistentIDFromPlayerID(playerId);
		if (persistentId == "")
		{
			SetFailure("The player manager has no persistent ID for playerId %1, so SetupPlayer never ran and player preparation cannot have authored a spawn context", playerId.ToString());
			return true;
		}

		// Precondition: preparation must actually have completed. Without this, an empty context below
		// would be honest rather than a defect, and this case would be reporting the wrong failure.
		if (!mode.m_aInitializedPlayers || !mode.m_aInitializedPlayers.Contains(persistentId))
		{
			SetFailure("Player %1 is not in the game mode's finalized set, so FinalizePlayerPreparation never completed for them. The campaign start should have finalized every connected player - see OVT_TEST_Campaign_ContinuePlayerIdMapping.", persistentId);
			return true;
		}

		string context = mode.GetPlayerSpawnContext(persistentId);

		if (context == OVT_TutorialComponent.SPAWN_CONTEXT_HOUSE || context == OVT_TutorialComponent.SPAWN_CONTEXT_NOHOUSE)
		{
			PrintFormat("The campaign start authored a spawn context of '%1' for the finalized local player, so FinalizePlayerPreparation ran the branch that decides which welcome they read", context);
			return true;
		}

		if (context == "")
		{
			SetFailure("Player %1 was finalized by the campaign start but has NO spawn context ('' - unknown). FinalizePlayerPreparation took one of its two branches and neither recorded what the player was given, so nothing on the server can answer which welcome they should read, and every client falls back to the house page - including a player who spawned at a bus stop with no house and no car. Check that both branches of FinalizePlayerPreparation still call SetPlayerSpawnContext (the fallback branch with '%2', the home branch with '%3').",
				persistentId, OVT_TutorialComponent.SPAWN_CONTEXT_NOHOUSE, OVT_TutorialComponent.SPAWN_CONTEXT_HOUSE);
			return true;
		}

		string known = "'" + OVT_TutorialComponent.SPAWN_CONTEXT_HOUSE + "' nor '" + OVT_TutorialComponent.SPAWN_CONTEXT_NOHOUSE + "'";

		SetFailure("Player %1 has a spawn context of '%2', which is neither %3. The value is dispatched into the PLAYER_SPAWNED tutorial event as its filter, so an unrecognised context matches NO welcome entry at all and the player sees nothing.",
			persistentId, context, known);
		return true;
	}
}
