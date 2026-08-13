//------------------------------------------------------------------------------------------------
//! A player who was already in the world when the campaign started still gets PLAYER_SPAWNED.
//!
//! THE BUG THIS EXISTS FOR. OnPlayerSpawnedLocal() is the only push site for the client-local
//! PLAYER_SPAWNED tutorial trigger, and it used to open with `if (!m_bGameStarted) return;`. On a
//! new campaign the local character is possessed SECONDS BEFORE the campaign starts - OVT_SpawnLogic
//! .DoSpawn_S always creates it, because possession is the only signal that dismisses the engine
//! loading screen behind the start menu - and nothing possesses the player again afterwards. So the
//! one push per session always landed on the wrong side of that guard and was silently dropped, and
//! the welcome-intro popup never appeared for anybody on the new-game path.
//!
//! WHY THIS TIER, AND WHY THIS IS NOT A RESTATEMENT OF THE IMPLEMENTATION. The autotest world
//! reproduces the failing ORDER for free: the harness spawns and possesses its local player while
//! the world loads, and OVT_TEST_SuiteBase's Setup step starts the campaign afterwards - the exact
//! sequence from the user's log (possession at 17:00:44, Start Game at 17:00:49). The case asserts
//! the property that ordering violated - the trigger was delivered - and nothing about HOW. Any
//! future refactor that reintroduces a start-state guard on the spawn path turns this red again.
//!
//! It cannot live in the Init tier: the campaign is never started there, so the second half of the
//! ordering does not exist and the trigger is legitimately still owed.
//!
//! PROVEN ABLE TO FAIL: with the TryPushSpawnedTutorialTrigger() call removed from the end of
//! OVT_OverthrowGameMode.DoStartGame(), this case fails on the poll backstop reporting
//! HasDeliveredSpawnTutorialTrigger() == false - which is the pre-fix behaviour exactly.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 30)]
class OVT_TEST_Campaign_Tutorial_SpawnTriggerSurvivesCampaignStart : SCR_AutotestCaseBase
{
	//! Poll budget for the delivery. The push itself is synchronous when the local controller is
	//! already assigned; the bounded retry behind it can take TUTORIAL_SPAWN_PUSH_ATTEMPTS x
	//! TUTORIAL_SPAWN_PUSH_RETRY_MS (~5 s) when it is not, so this is comfortably past that and is a
	//! diagnostic backstop rather than a retry.
	protected static const int MAX_POLLS = 600;

	//! Polls spent waiting for the delivery.
	protected int m_iPolls;

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

		// Precondition, not the assertion: there has to BE a local player for a client-local trigger
		// to be owed to. The harness spawns one (playerId 1) while the world loads; without it this
		// case would pass vacuously forever.
		if (!HasLocalPlayerCharacter())
		{
			SetFailure("No local player character is possessed in the test world, so the client-local PLAYER_SPAWNED trigger is not owed to anybody and this case cannot measure anything");
			return true;
		}

		if (!OVT_Global.GetTutorials())
		{
			SetFailure("The local player has no OVT_TutorialComponent - OVT_Global.GetTutorials() is null, so there is nothing on this machine that could receive a client-local trigger. Check Prefabs/GameMode/OVT_OverthrowController.et still carries the component");
			return true;
		}

		if (mode.HasDeliveredSpawnTutorialTrigger())
		{
			PrintFormat("PLAYER_SPAWNED was delivered to the local tutorial pipeline after %1 poll(s), although the local player was possessed before the campaign started", m_iPolls.ToString());
			return true;
		}

		m_iPolls += 1;
		if (m_iPolls > MAX_POLLS)
		{
			SetFailure("The local player was possessed BEFORE the campaign started (the new-campaign order: OVT_SpawnLogic.DoSpawn_S possesses, Start Game follows) and the PLAYER_SPAWNED tutorial trigger was still never delivered after %1 polls. welcome-intro - and every other PLAYER_SPAWNED entry - can never appear for a player who started the campaign. The push is owed from OnPlayerSpawnedLocal and must be re-tried once the campaign is actually running.",
				m_iPolls.ToString());
			return true;
		}

		return false; // keep polling
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this machine has a local player controlling a character.
	//! \return True when the local player controller has a controlled entity.
	protected bool HasLocalPlayerCharacter()
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
			return false;

		return controller.GetControlledEntity() != null;
	}
}
