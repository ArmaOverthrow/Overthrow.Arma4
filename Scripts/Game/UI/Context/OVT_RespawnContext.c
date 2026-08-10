//------------------------------------------------------------------------------------------------
//! The respawn screen: a vanilla SPAWNSCREEN map hosted inside an Overthrow workspace layout.
//!
//! WHY A SPAWNSCREEN MAP AND NOT THE FULLSCREEN ONE. SCR_MapEntity.OpenMap installs the
//! life-state-changed and player-deleted hooks ONLY for EMapEntityMode.FULLSCREEN, so a SPAWNSCREEN
//! map is not torn down by the engine when the player dies or their entity is deleted - which is
//! exactly the state this screen exists to sit in. It also needs no map gadget and no living
//! character, and a dead player cannot raise a gadget.
//!
//! WHY MapContext IS ACTIVATED BY HAND. Vanilla never opens a map outside a ChimeraMenuBase, and the
//! MapContext input context is supplied by the MENU PRESET (chimeraMenus.conf's MapMenu preset
//! carries ActionContext "MapContext"), not by SCR_MapEntity. A workspace-hosted layout has no
//! preset, so nothing would activate MapContext and the map would render without pan, zoom, cursor
//! or selection. SCR_DeployMenuBase.OnMenuUpdate does the same thing for the same reason.
//!
//! WHO DRIVES THIS. Nothing here runs on its own. OVT_RespawnScreenHandlerComponent, on the player
//! CONTROLLER, calls EOnFrame every frame while the screen is up; the context is configured on
//! OVT_OverthrowGameMode as m_RespawnUIContext. It is deliberately NOT in the player character's
//! OVT_UIManagerComponent.m_aContexts - that component closes every context it owns from its own
//! player-death handler, which is the exact moment this screen has to appear.
//!
//! THE SCREEN HAS NO DISMISS. m_sCloseAction stays EMPTY and there is no close button. It closes on
//! exactly two things, both of which mean the player has a character again: the server's OK result,
//! and the belt below.
//------------------------------------------------------------------------------------------------
class OVT_RespawnContext : OVT_UIContext
{
	//! The SPAWNSCREEN map config this screen opens. Overthrow has exactly ONE SPAWNSCREEN config,
	//! and that is load-bearing: SCR_MapEntity.SetupMapConfig early-returns the currently active
	//! config whenever the requested mode equals the last mode opened, swapping only the root
	//! widget. Two respawns in a row therefore reuse the cached config object. Adding a second
	//! SPAWNSCREEN config to the mod would silently hand one screen the other's config.
	static const ResourceName MAP_RESPAWN_CONF = "{6A83D5A000000002}Configs/Map/MapRespawn.conf";

	//! Vanilla's map input context. Supplied by a menu preset for every vanilla map; this screen is
	//! not a menu, so it must be activated every frame for as long as the map is up.
	static const string MAP_INPUT_CONTEXT = "MapContext";

	protected Widget m_wRespawnHomeButton;
	protected SCR_InputButtonComponent m_RespawnHomeAction;

	//! True while this context owns the open map, so OnClose only ever closes a map it opened.
	protected bool m_bMapOpen;

	//! Arms the living-character belt. Set on the first frame this screen sees the local player
	//! WITHOUT a living character, so the belt can only ever fire on a transition INTO one.
	protected bool m_bSeenNoLivingCharacter;

	//! Set when a map re-open failed, so a hard config fault costs one log line rather than one per
	//! frame on a screen the player cannot leave.
	protected bool m_bMapReopenFailed;

	//------------------------------------------------------------------------------------------------
	override void OnShow()
	{
		m_bSeenNoLivingCharacter = false;
		m_bMapReopenFailed = false;

		// THE FEATURE'S MOST LIKELY SILENT FAILURE, made loud. Every ownership check on this map is
		// keyed off the local persistent id: the house type refuses to populate on an empty one by
		// design, the camp type filters every private camp out, and the request component cannot be
		// resolved - an empty map with dead buttons and no error anywhere. Resolved without the
		// controlled entity, because a dead player may not have one.
		Print("[Overthrow] Respawn screen opening. Local persistent id: '" + OVT_Global.GetLocalPersistentId() + "'");

		WireRespawnHomeButton();
		SetStatusText("#OVT-Respawn_Hint");
		OpenRespawnMap();
	}

	//------------------------------------------------------------------------------------------------
	//! Closes the map BEFORE the base class detaches the layout. SCR_MapEntity's close path runs every
	//! map UI component's OnMapClose, and those still reference widgets that live inside this layout -
	//! the icons container they clear, the info panel they destroy, the root widget they hooked their
	//! event handler to. Tearing the map down first keeps that teardown running against a tree that is
	//! still parented. OnClose calls the same helper again; it is idempotent.
	override void CloseLayout()
	{
		if (m_bIsActive && m_wRoot)
			CloseRespawnMap();

		super.CloseLayout();
	}

	//------------------------------------------------------------------------------------------------
	override void OnClose()
	{
		CloseRespawnMap();

		if (m_RespawnHomeAction)
			m_RespawnHomeAction.m_OnActivated.Remove(OnRespawnHomeActivated);

		m_RespawnHomeAction = null;
		m_wRespawnHomeButton = null;
		m_bSeenNoLivingCharacter = false;
		m_bMapReopenFailed = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Keeps vanilla's map input alive alongside this screen's own context, and runs the belt.
	//! The base class has already activated m_sContextName by the time this runs.
	override void OnActiveFrame(float timeSlice)
	{
		if (UpdateLivingCharacterBelt())
			return;

		if (!m_bMapOpen)
			return;

		if (!m_InputManager)
			return;

		m_InputManager.ActivateContext(MAP_INPUT_CONTEXT);
	}

	//------------------------------------------------------------------------------------------------
	//! THE BELT. Closes the screen when the local player acquires a living character.
	//!
	//! The server's result RPC is the primary close, and this is the backup for it being lost, for a
	//! listen-server owner-RPC that never arrives, and for the ordinary race where the new character
	//! replicates before the result does. It fires often on the normal path and that is fine.
	//!
	//! WHY IT IS A TRANSITION AND NOT A STATE TEST. When this screen opens, the player still controls
	//! their CORPSE - the controller keeps referencing it for as long as the choice takes. A plain
	//! "do they have a character" test would therefore be racing the replication of the corpse's dead
	//! flag, and losing that race would close the screen on the frame it opened, leaving a dead player
	//! with nothing (the server's re-ask would eventually recover it, seconds later). Instead the belt
	//! must first SEE the player without a living character - a dead corpse, a deleted corpse, or no
	//! entity at all - and only then does a living one mean a spawn happened.
	//! \return True when the screen was closed by the belt.
	protected bool UpdateLivingCharacterBelt()
	{
		if (!HasLivingLocalCharacter())
		{
			m_bSeenNoLivingCharacter = true;
			return false;
		}

		if (!m_bSeenNoLivingCharacter)
			return false;

		Print("[Overthrow] Respawn screen closing: the local player has a living character");
		CloseLayout();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when this machine's player controls a character that can still play.
	protected bool HasLivingLocalCharacter()
	{
		// Cast rather than trusting IsCharacterDead's own null handling: that helper answers "not
		// dead" for anything that is not a ChimeraCharacter, which would read as living.
		ChimeraCharacter character = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!character)
			return false;

		return !OVT_PlayerManagerComponent.IsCharacterDead(character);
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the server's answer to a respawn request.
	//!
	//! A successful spawn - including one that fell back to home - is the only thing that closes this
	//! screen from a result. Everything else leaves it up with the reason on the status line, because
	//! a refused player still has no character and still has to choose. The hint that
	//! OVT_RespawnRequestComponent raises is transient and can be missed; this line is not.
	//! \param[in] result An OVT_RespawnResult value.
	void ApplyRespawnResult(int result)
	{
		// A result for a screen that is already gone. Reachable by mashing the button: the second
		// request finds its awaiting claim already spent and is refused, and by the time that answer
		// lands the first one has closed the screen. Nothing to close and nothing to say.
		if (!m_bIsActive)
			return;

		if (result == OVT_RespawnResult.OK || result == OVT_RespawnResult.OK_FELL_BACK_HOME)
		{
			CloseLayout();
			return;
		}

		string reason = OVT_RespawnService.ReasonKeyFor(result);
		if (reason.IsEmpty())
			reason = "#OVT-Respawn_Failed";

		SetStatusText(reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Re-opens the map if something else closed it while this screen is up.
	//!
	//! A POLL, DELIBERATELY. No Overthrow context may subscribe to the static map-open / map-close
	//! invokers on SCR_MapEntity - those outlive every layout, and a subscription that survives its
	//! subscriber is the defect map/legacy-retirement closed structurally. The rule is enforced by a
	//! grep over Scripts/, so this comment deliberately does not spell the accessor out. Called every
	//! frame by OVT_RespawnScreenHandlerComponent, off a frame event that is already running.
	//!
	//! One failure is logged and then the screen carries on without a map rather than re-attempting
	//! forever: a SetupMapConfig that fails once fails every time, and "Respawn at Home" still works
	//! from the workspace layout, so the degrade is a usable screen rather than a log flood.
	void EnsureMapOpen()
	{
		if (!m_bMapOpen)
			return;

		if (m_bMapReopenFailed)
			return;

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity)
			return;

		if (mapEntity.IsOpen())
			return;

		Print("[Overthrow] Respawn screen's map was closed by something else - reopening", LogLevel.WARNING);

		m_bMapOpen = false;
		OpenRespawnMap();

		if (!m_bMapOpen)
			m_bMapReopenFailed = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds the SPAWNSCREEN map configuration against this screen's root widget and opens it.
	//! The root widget must contain vanilla's Map.layout (it supplies MapFrame, MapWidget and
	//! UIIconsContainer); OpenMap resolves "MapWidget" out of it with FindAnyWidget.
	protected void OpenRespawnMap()
	{
		if (!m_wRoot)
			return;

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity)
		{
			Print("[Overthrow] Respawn screen has no SCR_MapEntity - the map cannot be shown", LogLevel.ERROR);
			return;
		}

		// A map opened by something else (the gadget map, via the vanilla MapMenu preset) would be
		// closed by OpenMap anyway, with a warning, leaving its menu on screen with no map under it.
		// Close it here so the log says who did it.
		if (mapEntity.IsOpen())
		{
			Print("[Overthrow] Respawn screen closing a map that was already open", LogLevel.WARNING);
			mapEntity.CloseMap();
		}

		MapConfiguration config = mapEntity.SetupMapConfig(EMapEntityMode.SPAWNSCREEN, MAP_RESPAWN_CONF, m_wRoot);
		if (!config)
		{
			Print("[Overthrow] Respawn screen could not load " + MAP_RESPAWN_CONF, LogLevel.ERROR);
			return;
		}

		mapEntity.OpenMap(config);
		m_bMapOpen = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void CloseRespawnMap()
	{
		if (!m_bMapOpen)
			return;

		m_bMapOpen = false;

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity)
			mapEntity.CloseMap();
	}

	//------------------------------------------------------------------------------------------------
	//! "Respawn at Home" - always present, always operable, nothing needs to be selected first.
	protected void WireRespawnHomeButton()
	{
		m_wRespawnHomeButton = m_wRoot.FindAnyWidget("RespawnHomeButton");
		if (!m_wRespawnHomeButton)
		{
			Print("[Overthrow] Respawn screen layout has no RespawnHomeButton", LogLevel.ERROR);
			return;
		}

		m_RespawnHomeAction = SCR_InputButtonComponent.Cast(m_wRespawnHomeButton.FindHandler(SCR_InputButtonComponent));
		if (!m_RespawnHomeAction)
		{
			Print("[Overthrow] RespawnHomeButton has no SCR_InputButtonComponent", LogLevel.ERROR);
			return;
		}

		// m_OnActivated, not m_OnClicked: it is the one invoker reached by BOTH the mouse click path
		// and the bound-action path, so mouse, keyboard and pad all arrive here once.
		m_RespawnHomeAction.m_OnActivated.Insert(OnRespawnHomeActivated);
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the server to spawn this player at home. THE GUARANTEED FLOOR of the whole feature: it
	//! needs no selection, no eligible location anywhere in the world, and no QRF exemption - home
	//! never goes through the eligibility rules at all, so it works even when a QRF is sitting on it.
	//!
	//! Takes no parameters on purpose. SCR_InputButtonComponent.m_OnActivated invokes with
	//! (component, actionName), which is not the (widget, value, trigger) shape of the click invoker,
	//! and every other Overthrow m_OnActivated subscriber is likewise a nullary method.
	//!
	//! It does NOT close the screen. The server's result does, or the belt does - a refusal must leave
	//! the player looking at a map they can press again, not at nothing.
	protected void OnRespawnHomeActivated()
	{
		OVT_RespawnRequestComponent respawn = OVT_Global.GetRespawnRequests();
		if (!respawn)
		{
			// A component that exists in script but not on OVT_OverthrowController.et is a silent
			// no-op - the request never leaves this machine, and on a screen the player cannot
			// dismiss that reads as a frozen game. Say so on screen as well as in the log.
			Print("[Overthrow] No OVT_RespawnRequestComponent on the local controller - respawn-at-home request dropped", LogLevel.ERROR);
			SetStatusText("#OVT-Respawn_Failed");
			return;
		}

		respawn.RequestRespawn(OVT_RespawnDestination.HOME, vector.Zero);
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the line under the title. Null-safe so a stale layout degrades to no status text rather
	//! than a script error on a screen the player cannot leave.
	//! \param[in] text Literal text or a localization key.
	void SetStatusText(string text)
	{
		if (!m_wRoot)
			return;

		TextWidget status = TextWidget.Cast(m_wRoot.FindAnyWidget("StatusText"));
		if (!status)
			return;

		status.SetText(text);
	}
}
