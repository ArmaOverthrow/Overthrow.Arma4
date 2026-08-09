//------------------------------------------------------------------------------------------------
//! Neutralises vanilla's post-load sweep that deletes unclaimed player characters (BUG-086).
//!
//! WHAT THE SWEEP DOES, AND WHY IT IS FATAL HERE. SCR_PlayerReconnectData is a conf-declared
//! PersistentState that Overthrow inherits from vanilla's Common.conf. Its serializer writes down the
//! character id of every player who was CONNECTED when the save was taken; on the next load,
//! HandleStateChange() schedules RemoveUnusedCharacters() for 60 s after persistence reaches ACTIVE,
//! and that deletes every one of those characters nobody has taken control of - the entity AND, by way
//! of OnPlayerEntityCleanup_S, any expectation of getting it back.
//!
//! That is correct for vanilla Conflict, where a body is worth 120 s and then the player respawns
//! fresh. It is exactly wrong for Overthrow, where a stopped server's players must find their bodies -
//! and their gear - whenever they next log in, days later if need be. It was MEASURED as the cause of
//! the slow-rejoin failures on the dedicated server (2026-08-05): a fast rejoin returned
//! "restored from their stored body, gear intact", the same test with several minutes' delay returned
//! "could not be given their stored body".
//!
//! NEUTRALISED BY NOT SCHEDULING IT, which is the smallest possible change: the state, its config
//! entry and its serializer all keep working, the ids are still written and read, and nothing in the
//! save format moves. Only the timer is gone.
//!
//! WHY NOT SCOPE THE STATE OUT IN Overthrow.conf INSTEAD. Removing a vanilla PersistentState by GUID
//! override would silently change what a save contains, and a mistake in a .conf fails quietly -
//! whereas this file is compiled, greppable and fails loudly. The same reasoning as the AI SelfSpawn
//! overrides, in the opposite direction: one bit is worth a conf edit, a whole behaviour is worth
//! script.
//!
//! WHAT REPLACES IT: OVT_PlayerManagerComponent.ReserveOfflinePlayerBodies(), which sweeps the same
//! bodies after a load and HIDES them instead of destroying them (OVT_PersistenceReservation).
//------------------------------------------------------------------------------------------------
modded class SCR_PlayerReconnectData
{
	//------------------------------------------------------------------------------------------------
	//! Vanilla schedules RemoveUnusedCharacters() here. Overthrow does not - see the file header.
	//! \param[in] oldState State being left.
	//! \param[in] newState State being entered.
	override void HandleStateChange(EPersistenceSystemState oldState, EPersistenceSystemState newState)
	{
		if (newState != EPersistenceSystemState.ACTIVE)
			return;

		PrintFormat("[Overthrow] Vanilla's unclaimed-player-character sweep is disabled - %1 restored body/bodies are Overthrow's to reserve",
			m_aPlayerCharacters.Count().ToString());
	}
}
