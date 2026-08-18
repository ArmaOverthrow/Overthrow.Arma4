//------------------------------------------------------------------------------------------------
//! "Sleep" - skip eight in-game hours on a bed the player is entitled to use, replaying everything
//! the skipped window contained (implementation.md 3.6).
//!
//! SINGLE PLAYER ONLY, AND THEREFORE NO NETWORKING (D12). In single player (RplMode.None) the
//! machine that clicks IS the authority, so this action calls the static OVT_SleepService directly:
//! no RPC, no controller component, no replicated state. The mode check lives in CanBeShownScript
//! (and again inside OVT_SleepService.CanSleep), so the action never appears on a listen host or a
//! dedicated client.
//!
//! DO NOT ADD AN RplComponent OWNERSHIP GATE. The pattern in OVT_SetHomeAction (RplComponent.IsOwner
//! on the owner entity) cannot work here: the seven vanilla bed prefabs this action is authored onto
//! carry no RplComponent at all, and in single player there is no replication to be the owner of.
//! The actor is resolved as SCR_PlayerController.GetLocalControlledEntity() instead - which is the
//! honest answer on the only machine this action ever runs on.
//!
//! WHAT IS HIDDEN AND WHAT IS DISABLED (D13). Hidden: not single player, and not a sleeping place
//! (an owned house, the player's own camp, a deployed FOB or a captured base). Everything else - an
//! active QRF, a wanted level, the twelve-hour cooldown - stays VISIBLE with a translated reason,
//! following OVT_SabotageTowerAction's rule that a relevant blocked action must not silently vanish.
//!
//! WHY THE VISIBILITY ANSWER IS CACHED. CanBeShownScript runs every frame while a player looks at a
//! bed, and the location gate walks four manager collections (real estate, camps, FOBs, bases). Same
//! caching shape as OVT_RearmVehicleAction: one answer per CHECK_TTL_MS, invalidated on a perform so
//! the label flips to the countdown immediately.
//------------------------------------------------------------------------------------------------
class OVT_SleepAction : ScriptedUserAction
{
	//! How long the cached visibility answer is reused, in milliseconds of world time.
	protected const float CHECK_TTL_MS = 1000;

	//! Cached visibility answer (single player AND a place this player may sleep in).
	protected bool m_bCachedVisible;

	//! World time (ms) at which the cached answer goes stale.
	protected float m_fCacheExpiresAt;

	//! False until the first RefreshCache(), and reset to false whenever the answer is known stale.
	protected bool m_bHasCache;

	//------------------------------------------------------------------------------------------------
	//! Sleeps. The service re-validates before touching anything, so a state that changed inside the
	//! cache window cannot get through.
	//! \param[in] pOwnerEntity The bed this action lives on.
	//! \param[in] pUserEntity The performing character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		OVT_SleepService.PerformSleep(pUserEntity);

		// The cooldown is about to be stamped, so both the gate answer and the label are stale.
		m_bHasCache = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Shown only in single player, and only on a bed inside a place this player is entitled to sleep
	//! in. Deliberately NOT hidden for a QRF, a wanted level or the cooldown - those are told why in
	//! CanBePerformedScript.
	//! \param[in] user The character looking at the action.
	//! \return True when the action is worth putting on screen.
	override bool CanBeShownScript(IEntity user)
	{
		RefreshCache();
		return m_bCachedVisible;
	}

	//------------------------------------------------------------------------------------------------
	//! Blocked, with a translated reason, during a QRF, while wanted, and until the cooldown expires.
	//!
	//! OVT_SleepService.CanSleep is the single rule set - the same call PerformSleep re-runs - and an
	//! EMPTY reason key from it means "there is nothing to tell the player" (not single player, no
	//! resolvable record, no clock). Those are states in which CanBeShownScript already hides the
	//! action, so this refuses without inventing a message for them.
	//! \param[in] user The character looking at the action.
	//! \return True when the sleep may proceed.
	override bool CanBePerformedScript(IEntity user)
	{
		string reasonKey;
		if(OVT_SleepService.CanSleep(ResolveActor(), reasonKey)) return true;

		if(reasonKey != "") SetCannotPerformReason(reasonKey);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! While the cooldown runs the action name carries the time left, so a player can see when it is
	//! worth coming back rather than finding an action that refuses without saying for how long.
	//!
	//! KEY-PLUS-LITERAL CONCATENATION (D14) is the project's established form here
	//! (OVT_SabotageTowerAction, OVT_RearmVehicleAction): the engine translates the leading #key and
	//! passes the remainder through untouched.
	//! \param[out] outName Replacement action name.
	//! \return True when this action supplied a name, false to use the one configured on the prefab.
	override bool GetActionNameScript(out string outName)
	{
		float remaining = OVT_SleepService.GetCooldownRemainingHours(ResolvePersistentId());
		if(remaining <= 0) return false;

		outName = "#OVT-Sleep (" + OVT_SleepSchedule.FormatRemaining(remaining) + ")";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Runs on the clicking machine only - which in single player is the authority. See the class
	//! header.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Recompute the cached visibility answer when it has gone stale.
	//!
	//! The location gate is only paid for in single player, so a multiplayer session costs one enum
	//! comparison per second per bed a player looks at and nothing else.
	protected void RefreshCache()
	{
		float now = GetWorldTimeMs();
		if(m_bHasCache && now < m_fCacheExpiresAt) return;

		m_fCacheExpiresAt = now + CHECK_TTL_MS;
		m_bHasCache = true;
		m_bCachedVisible = false;

		// Single player only (OVT_PlayerStartMenuHandlerComponent uses the same test).
		if(RplSession.Mode() != RplMode.None) return;

		// No Overthrow game mode (world editor, other scenarios): none of the managers exist.
		OVT_OverthrowGameMode gameMode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(!gameMode) return;

		IEntity bed = GetOwner();
		if(!bed) return;

		string persistentId = ResolvePersistentId();
		if(persistentId == "") return;

		m_bCachedVisible = OVT_SleepService.IsSleepLocation(bed.GetOrigin(), persistentId);
	}

	//------------------------------------------------------------------------------------------------
	//! The character this action is asked about. See the class header for why it is the LOCAL
	//! controlled entity rather than an ownership test on the bed.
	//! \return The local player's controlled entity, or null.
	protected IEntity ResolveActor()
	{
		return SCR_PlayerController.GetLocalControlledEntity();
	}

	//------------------------------------------------------------------------------------------------
	//! The local player's persistent id.
	//! \return The persistent id, or "" when it cannot be resolved (no player manager, no character).
	protected string ResolvePersistentId()
	{
		IEntity actor = ResolveActor();
		if(!actor) return "";

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return "";

		return players.GetPersistentIDFromControlledEntity(actor);
	}

	//------------------------------------------------------------------------------------------------
	//! World time in milliseconds, guarded so the action still answers in a world-less context.
	//! \return The current world time, or 0.
	protected float GetWorldTimeMs()
	{
		BaseWorld world = GetGame().GetWorld();
		if(!world) return 0;

		return world.GetWorldTime();
	}
}
