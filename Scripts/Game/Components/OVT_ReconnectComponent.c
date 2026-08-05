[EntityEditorProps(category: "GameScripted/GameMode", description: "Keeps a disconnected player's character alive and hidden instead of letting it be deleted.")]
class OVT_ReconnectComponentClass : SCR_ReconnectComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Stops the engine from deleting a disconnecting player's character, and hides it instead (BUG-086).
//!
//! THIS COMPONENT EXISTS FOR ONE LINE OF VANILLA CODE. SCR_BaseGameMode.OnPlayerDisconnected ends with
//!
//!     m_pRespawnSystemComponent.OnPlayerEntityCleanup_S(character);   // Save + StopTracking(keepData)
//!     RplComponent.DeleteRplEntity(character, false);                 // and then it is gone
//!
//! and the ONLY way script can skip both is the branch just above it (SCR_BaseGameMode.c:963-975):
//!
//!     SCR_ReconnectComponent reconnect = SCR_ReconnectComponent.GetInstance();
//!     if (reconnect && reconnect.HandlePlayerDisconnect(playerId, cause))
//!         ... character = null;   // Avoid deleting it
//!
//! That branch is BI's own extension point for exactly this question - "should this body survive the
//! disconnect?" - and it is why Overthrow now carries a reconnect component at all. The alternative
//! was re-implementing OnPlayerDisconnected in a modded SCR_BaseGameMode, duplicating ~40 lines of
//! engine glue that would rot silently on the next game update.
//!
//! WHAT IT CHANGES ABOUT VANILLA'S ANSWER. Vanilla reserves a body only for REPLICATION-group kicks
//! (a connection drop) and only for m_iReconnectTime seconds, default 120 s or the dedicated server's
//! slotReservationTimeout. Overthrow reserves for EVERY disconnect cause, including a clean quit, and
//! forever - the user's requirement is 100% restoration after an absence of any length, across any
//! number of restarts (BUG-086), and a clean quit is the common case that requirement is about.
//!
//! IT DELIBERATELY STORES NOTHING IN m_mReconnectData. Vanilla's map is a per-player reservation with
//! an expiry timer and its own hand-back path (ApplyData -> SetInitialMainEntity + EmitPlayerEntityChange_S).
//! Overthrow already has a complete, better-informed hand-back path of its own - OVT_SpawnLogic's
//! FindById fast path adopts the live tracked body and runs Overthrow's guards (corpse, owner left,
//! already has a character) - so a second one competing with it would be a bug, not a feature. Leaving
//! the map empty also means:
//!
//!   - CheckExpiery() never schedules and nothing is ever reaped, which IS the no-expiry decision;
//!   - HandlePlayerReconnect() answers false, so SCR_SpawnLogic.ResolveReconnection() stays a no-op and
//!     Overthrow's own spawn flow runs exactly as before;
//!   - SCR_PlayerReconnectDataSerializer writes an empty `reconnect` list, so nothing new lands in the
//!     save.
//!
//! The related sweep in SCR_PlayerReconnectData - which deletes stored player characters nobody has
//! claimed 60 s after a load - is neutralised separately, in Scripts/Game/Modded/SCR_PlayerReconnectData.c.
//------------------------------------------------------------------------------------------------
class OVT_ReconnectComponent : SCR_ReconnectComponent
{
	//------------------------------------------------------------------------------------------------
	//! Claims the leaving player's character so the game mode does not delete it, and hides it.
	//!
	//! Called from SCR_BaseGameMode.OnPlayerDisconnected AFTER the game mode's own
	//! OVT_OverthrowGameMode.OnPlayerDisconnected has run, so the player's body id, transform and gear
	//! snapshot have already been captured off this body by the time it is hidden. That ordering is
	//! load-bearing and is the reason nothing needs to be captured here.
	//!
	//! A DEAD BODY IS NOT CLAIMED, matching vanilla's own IsDataRelevant(): a corpse is lootable remains
	//! whose id the death path has already cleared, so it takes the unchanged vanilla route.
	//! \param[in] playerId The player who is leaving.
	//! \param[in] cause Why they left. Deliberately ignored - see the class header.
	//! \return True when the body has been reserved and the caller must not delete it.
	override bool HandlePlayerDisconnect(int playerId, KickCauseCode cause)
	{
		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!character)
			return false;

		if (OVT_PlayerManagerComponent.IsCharacterDead(character))
			return false;

		if (!OVT_PersistenceReservation.Reserve(character))
			return false;

		PrintFormat("[Overthrow] Player %1's body is reserved - it stays alive, tracked and hidden until they return", playerId.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Never hands a body back through vanilla's path - Overthrow's spawn flow owns that.
	//!
	//! Would answer false anyway (nothing is ever put in m_mReconnectData), but saying so explicitly
	//! means a future change to the base class cannot quietly introduce a second hand-over racing
	//! OVT_SpawnLogic.RequestPersistedPlayerBody().
	//! \param[in] playerId The returning player.
	//! \return Always false.
	override bool HandlePlayerReconnect(int playerId)
	{
		return false;
	}
}
