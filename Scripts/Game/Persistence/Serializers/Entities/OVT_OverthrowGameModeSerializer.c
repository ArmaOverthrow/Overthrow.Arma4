//------------------------------------------------------------------------------------------------
//! Persists Overthrow's campaign lifecycle state on the game mode entity.
//!
//! BINDING. Configs/Systems/Persistence/Overthrow.conf overrides the EntitySerializer of the shared
//! vanilla game-mode configuration ({65ACD95F40F6C669}, rule EntityClass "SCR_BaseGameMode") with
//! this class, reusing vanilla's serializer GUID {691481C816B7FF22}. That is exactly how vanilla's
//! own Conflict.conf swaps in SCR_GameModeCampaignSerializer, and it is why there is only ever ONE
//! configuration matching the game mode entity - no second rule, no priority tie to break.
//!
//! VANILLA PAYLOAD IS NOT DROPPED. Replacing the serializer entry would otherwise silently stop
//! persisting SCR_BaseGameMode's disabled resource types, so the base payload is nested under a
//! "base" sub-object and delegated to super, copying SCR_GameModeCampaignSerializer verbatim
//! (SCR_GameModeCampaignSerializer.c:47-51 / :70-78). Overthrow never mutates those types today, so
//! the base serializer normally returns DEFAULT and writes nothing - the wrapper costs an empty
//! object and keeps the behaviour honest if that ever changes.
//!
//! WHAT THIS SERIALIZER ADDS, AND WHY IT IS THE WHOLE OF THE GAME-MODE WORK.
//! Overthrow has two distinct start paths (OVT_OverthrowGameMode.c):
//!   DoStartNewGame() - GENERATES a fresh campaign (base/town ownership, occupying faction seed);
//!   DoStartGame()    - flips m_bGameStarted, runs every manager's PostGameStart(), then flips
//!                      m_bGameInitialized.
//! Continuing a saved campaign must run the SECOND and never the first, or the restored world state
//! is immediately overwritten by freshly generated ownership. The old EPF path expressed that by
//! setting m_bRequestStartOnPostProcess and letting OnWorldPostProcess call DoStartGame(); this
//! serializer restores the same flag through OVT_OverthrowGameMode.RestoreStartedCampaign(), so the
//! restore runs the shipped start sequence rather than a serializer-private shortcut.
//!
//! DESERIALIZE RUNS IN TWO SITUATIONS, AND MUST BE SAFE IN BOTH.
//!  1. A session started from a savepoint - the campaign has not started, so the restore does its
//!     job.
//!  2. Saved data re-applied to a LIVE session
//!     (OVT_PersistenceManagerComponent.ReapplyLatestSaveData), where the campaign is already
//!     running. RestoreStartedCampaign() returns immediately when HasGameStarted() is already true,
//!     so nothing here re-enters the non-re-entrant DoStartGame() sequence.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order, and WriteValue()'s
//! name is only meaningful in JSON. Version is hand-rolled, first field, as in every vanilla
//! serializer.
//------------------------------------------------------------------------------------------------
class OVT_OverthrowGameModeSerializer : SCR_GameModeSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The game mode class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_OverthrowGameMode;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the campaign lifecycle state, after the inherited vanilla payload.
	//! \param[in] entity The game mode entity being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the base serializer failed or the entity is not an Overthrow mode.
	override protected ESerializeResult Serialize(notnull IEntity entity, notnull SaveContext context)
	{
		const OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(entity);
		if (!mode)
			return ESerializeResult.ERROR;

		context.StartObject("base");
		const ESerializeResult baseResult = super.Serialize(entity, context);
		context.EndObject();
		if (baseResult == ESerializeResult.ERROR)
			return baseResult;

		context.WriteValue("version", 1);

		const bool gameStarted = mode.HasGameStarted();
		context.Write(gameStarted);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the campaign lifecycle state back and asks the game mode to continue the campaign.
	//! \param[in] entity The game mode entity being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity entity, notnull LoadContext context)
	{
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(entity);
		if (!mode)
			return false;

		if (context.DoesObjectExist("base"))
		{
			if (!context.StartObject("base") ||
				!super.Deserialize(entity, context) ||
				!context.EndObject())
			{
				return false;
			}
		}

		int version;
		context.Read(version);

		bool gameStarted;
		if (context.Read(gameStarted) && gameStarted)
		{
			Print("[Overthrow] Restoring a started campaign from the save point", LogLevel.NORMAL);
			mode.RestoreStartedCampaign();
		}

		return true;
	}
}
