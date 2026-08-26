class OVT_RespawnScreenHandlerComponentClass : ScriptComponentClass {}

//------------------------------------------------------------------------------------------------
//! Drives the respawn screen on the machine of the player who has to choose.
//!
//! Modelled on OVT_PlayerStartMenuHandlerComponent, and for the same reason: an OVT_UIContext is not
//! self-driving. Its input context is only activated from OVT_UIContext.EOnFrame, and nothing calls
//! that unless something with a FRAME event mask does. On the living character that job belongs to
//! OVT_UIManagerComponent - which is exactly why the respawn screen cannot live there. That component
//! sits on the CHARACTER, and OVT_UIManagerComponent.OnPlayerDeath closes every context it owns the
//! moment the player dies. A screen hosted on the character is torn down by the event that is
//! supposed to open it.
//!
//! The player CONTROLLER survives death, the corpse and the new character, so it is the only entity
//! on the client that can hold a screen across a respawn. The context itself is configured on
//! OVT_OverthrowGameMode (m_RespawnUIContext) so there is exactly one instance per machine.
//!
//! WHY THE MAP RE-OPEN GUARD IS A POLL. This screen must never be left without its map, but no
//! Overthrow context may subscribe to the static map-open / map-close invokers on SCR_MapEntity:
//! those outlive every layout and every context, and a subscription that survives its subscriber is
//! the defect map/legacy-retirement closed structurally (BUG-069 part 4). A poll on a frame event
//! that is already running costs a pointer compare and cannot leak. That rule is enforced by a grep
//! over Scripts/, so this comment deliberately does not spell the accessor out.
//!
//! ONE HANDLER PER MACHINE, NOT ONE PER PLAYER. On a server every connected player has a player
//! controller entity and therefore a copy of this component, but
//! OVT_ControllerComponent<OVT_RespawnRequestComponent>.Get() resolves the LOCAL machine's
//! controller, so on a listen server every copy would subscribe to the host's invokers and N screens
//! would open for one host death. The owning-player check below is what
//! stops that; it is the client-side twin of the reason OVT_SpawnLogic resolves the request component
//! per player rather than through OVT_Global.
//------------------------------------------------------------------------------------------------
class OVT_RespawnScreenHandlerComponent : ScriptComponent
{
	//! The screen. Owned by the game mode as a configured attribute; this is a borrowed pointer.
	protected OVT_RespawnContext m_RespawnContext;

	//! The local player's request component, once it exists. Kept so OnDelete can unsubscribe from
	//! invokers that live on an entity with a different lifetime to this one.
	protected OVT_RespawnRequestComponent m_RespawnRequests;

	//! True once both invokers are subscribed. Stops the poll.
	protected bool m_bSubscribed;

	//! True while this component believes the screen is up and is driving its frames.
	protected bool m_bScreenShown;

	//! Stops a missing context from printing an error on every frame of a session.
	protected bool m_bReportedMissingContext;

	//------------------------------------------------------------------------------------------------
	protected override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		// The mask is never cleared. The poll below is a handful of null checks, and the frame event
		// is what drives the screen's input context for as long as it is up - a component that gave
		// the mask back would have to take it again from an invoker callback, which is more state
		// than the saving is worth on a screen the player cannot leave.
		SetEventMask(owner, EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	//! Unsubscribes from invokers on the OVT_OverthrowController entity, whose lifetime is not this
	//! component's. Without this a controller that outlives a re-created player controller would call
	//! into a dead handler.
	protected override void OnDelete(IEntity owner)
	{
		Unsubscribe();

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (m_bScreenShown)
			UpdateScreen(owner, timeSlice);

		if (m_bSubscribed)
			return;

		TrySubscribe(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Drives the open screen for one frame: its input contexts, its close conditions and its map.
	//! \param[in] owner The player controller entity this component sits on.
	//! \param[in] timeSlice Seconds since the previous frame.
	protected void UpdateScreen(IEntity owner, float timeSlice)
	{
		if (!m_RespawnContext)
		{
			m_bScreenShown = false;
			return;
		}

		// This is the whole reason the component exists. OVT_UIContext.EOnFrame activates
		// OverthrowRespawnContext, and OVT_RespawnContext.OnActiveFrame activates vanilla's
		// MapContext on top of it - without both, the screen renders and ignores every input.
		m_RespawnContext.EOnFrame(owner, timeSlice);

		// The context closes itself on a successful result and on the living-character belt.
		if (!m_RespawnContext.IsActive())
		{
			m_bScreenShown = false;
			return;
		}

		m_RespawnContext.EnsureMapOpen();
	}

	//------------------------------------------------------------------------------------------------
	//! Polls until the local player's request component exists, then subscribes once and stops.
	//!
	//! Everything here can legitimately be unavailable for a while: the controller entity is spawned
	//! and given to its owner asynchronously, and on a client the persistent-id mapping that resolves
	//! it arrives in an RPC. Nothing is logged for a miss, because a miss is the normal state for the
	//! first seconds of a session and for the whole session on a dedicated server.
	//! \param[in] owner The player controller entity this component sits on.
	protected void TrySubscribe(IEntity owner)
	{
		PlayerController controller = PlayerController.Cast(owner);
		if (!controller)
			return;

		// No local player on this machine - a dedicated server, or a client that has not finished
		// connecting. Deliberately not a give-up: a client that answers 0 now will answer its own id
		// shortly, and giving up here would leave that player with no screen for the whole session.
		int localPlayerId = SCR_PlayerController.GetLocalPlayerId();
		if (localPlayerId < 1)
			return;

		// Somebody else's player controller, held by a server. See the class header: without this a
		// listen-server host opens one screen per connected player when the HOST dies. Not cleared
		// from the frame mask, because GetPlayerId() can legitimately answer 0 before the controller
		// has been assigned and a permanent teardown from a transient value is not recoverable.
		if (controller.GetPlayerId() != localPlayerId)
			return;

		OVT_RespawnRequestComponent requests = OVT_ControllerComponent<OVT_RespawnRequestComponent>.Get();
		if (!requests)
			return;

		if (!ResolveContext(owner))
			return;

		m_RespawnRequests = requests;
		requests.m_OnShowRespawnScreen.Insert(OnShowRespawnScreen);
		requests.m_OnRespawnResult.Insert(OnRespawnResult);
		m_bSubscribed = true;

		Print("[Overthrow] Respawn screen handler ready for player " + localPlayerId.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! Finds the configured respawn context on the game mode and initialises it against this
	//! controller.
	//!
	//! Init() is what gives the context its InputManager, and without it EOnFrame cannot activate a
	//! single input context. The owner passed is the player controller, so the context's character
	//! fields stay null - the same deal OVT_StartGameContext gets, and this screen reads none of them.
	//! \param[in] owner The player controller entity this component sits on.
	//! \return True when the context is available.
	protected bool ResolveContext(IEntity owner)
	{
		if (m_RespawnContext)
			return true;

		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if (!mode)
			return false;

		OVT_RespawnContext context = mode.GetRespawnContext();
		if (!context)
		{
			if (!m_bReportedMissingContext)
			{
				m_bReportedMissingContext = true;
				Print("[Overthrow] Game mode has no m_RespawnUIContext configured - a dead player will never see the respawn screen", LogLevel.ERROR);
			}

			return false;
		}

		context.Init(owner, null);
		m_RespawnContext = context;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The server wants this player to choose where to respawn.
	//!
	//! IDEMPOTENT BY CONTRACT. The server re-asks every few seconds for as long as the player is still
	//! awaiting a choice, so this runs repeatedly for one death. Showing an already-shown screen must
	//! be a no-op - and note that it cannot simply call ShowLayout() and hope, because
	//! OVT_UIContext.ShowLayout CLOSES an active layout when m_bOpenActionCloses is set. The prefab
	//! registration sets that attribute to 0 and this guard makes the behaviour independent of it.
	protected void OnShowRespawnScreen()
	{
		ShowRespawnScreen();
	}

	//------------------------------------------------------------------------------------------------
	//! The server's answer to a respawn request. Display only - the character arrives by replication.
	//! \param[in] result An OVT_RespawnResult value.
	protected void OnRespawnResult(int result)
	{
		if (!m_RespawnContext)
			return;

		m_RespawnContext.ApplyRespawnResult(result);
		m_bScreenShown = m_RespawnContext.IsActive();
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the respawn screen up, or does nothing if it is already up.
	//! Public so the admin chat harness can reach the shipped path rather than a copy of it.
	void ShowRespawnScreen()
	{
		if (!ResolveContext(GetOwner()))
			return;

		if (m_RespawnContext.IsActive())
		{
			m_bScreenShown = true;
			return;
		}

		m_RespawnContext.ShowLayout();
		m_bScreenShown = m_RespawnContext.IsActive();
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the respawn screen down. Nothing in the shipped flow calls this - the context closes
	//! itself on a result or on the living-character belt - but the harness needs a way out of a
	//! screen that deliberately has no dismiss action.
	void CloseRespawnScreen()
	{
		if (!m_RespawnContext)
			return;

		m_RespawnContext.CloseLayout();
		m_bScreenShown = false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while the respawn screen is up on this machine.
	bool IsScreenShown()
	{
		if (!m_RespawnContext)
			return false;

		return m_RespawnContext.IsActive();
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the invoker subscriptions. Safe to call when nothing was subscribed.
	protected void Unsubscribe()
	{
		if (m_RespawnRequests)
		{
			m_RespawnRequests.m_OnShowRespawnScreen.Remove(OnShowRespawnScreen);
			m_RespawnRequests.m_OnRespawnResult.Remove(OnRespawnResult);
		}

		m_RespawnRequests = null;
		m_bSubscribed = false;
	}

	//------------------------------------------------------------------------------------------------
	//! The local machine's handler, or null. Used by the admin chat harness.
	//! \return The handler on the local player controller, or null.
	static OVT_RespawnScreenHandlerComponent GetLocalInstance()
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
			return null;

		return OVT_RespawnScreenHandlerComponent.Cast(controller.FindComponent(OVT_RespawnScreenHandlerComponent));
	}
}
