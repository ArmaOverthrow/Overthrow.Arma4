[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server->owning-client tutorial delivery and the whole client-side tutorial pipeline")]
class OVT_TutorialComponentClass : OVT_ComponentClass {};

//------------------------------------------------------------------------------------------------
//! The client half of the tutorial framework, on the per-player OVT_OverthrowController entity.
//!
//! Two jobs, and the split between them is the whole point of the design:
//!
//!  1. DELIVERY. One RplRcver.Owner RPC carrying an entry id. The server resolves the acting
//!     player's controller and calls Notify(); nobody else's client hears it. This is deliberately
//!     NOT the OVT_NotificationManagerComponent pattern of broadcasting to everyone and filtering
//!     client-side, and it is the exact failure mode (per-player delivery on a dedicated server)
//!     that killed the starter jobs in BUG-037.
//!
//!  2. THE PIPELINE. Seen check, tips-disabled check, queue, a 1000 ms pump, the can-show-now gate,
//!     and finally one invoker the UI surfaces subscribe to. Every DECISION in that chain is a pure
//!     function in Scripts/Game/Data/ that the Logic tier already pins; what lives here is only the
//!     plumbing those functions cannot be given - timers, widgets, menus and the world.
//!
//! Client-local triggers (map opened, an Overthrow menu opened, the local player spawned) never
//! round-trip to the server. They call FireLocalEvent(), which runs the SAME matcher over the SAME
//! authored entries and lands in the SAME queue as the RPC path. There is exactly one pipeline.
//!
//! Never on OVT_PlayerCommsComponent: it is deprecated for new RPCs by project rule.
//------------------------------------------------------------------------------------------------
class OVT_TutorialComponent : OVT_Component
{
	//! How often the queue is drained, in milliseconds. Slow on purpose: a tip that a menu delayed
	//! should appear a beat after the menu closes, not the same frame the widget is torn down.
	static const int PUMP_INTERVAL_MS = 1000;

	//! Lowest runtime player id the engine ever issues.
	static const int FIRST_VALID_PLAYER_ID = 1;

	//! Fired on the owning client when an entry is cleared to be shown. Args: OVT_TutorialEntryConfig.
	//! The two UI surfaces (the HUD overlay in Phase 5 and the modal context in Phase 6) subscribe to
	//! this the way OVT_ProgressInfo subscribes to the controller's progress events.
	ref ScriptInvoker m_OnShowTutorial = new ScriptInvoker();

	//! Pending entries, priority-ordered. Pure; see OVT_TutorialQueue.
	protected ref OVT_TutorialQueue m_Queue;

	//! Per-machine "already shown" record. Pure; see OVT_TutorialSeenStore.
	//!
	//! Loaded from the player's profile on first use through OVT_TutorialSettingsAccessor and flushed
	//! back on every mutation. On a headless server (or if the engine has no settings module) the
	//! load is a no-op and this degrades to an in-memory store for the session - which is correct
	//! there, because a dedicated server has no profile to remember anything in.
	protected ref OVT_TutorialSeenStore m_SeenStore;

	//! Reusable match buffer for the client-local path. Cleared by the matcher on every call.
	protected ref array<string> m_aMatchBuffer;

	//! Lazy id -> entry lookup over the authored entries.
	//!
	//! Built HERE and not read off the manager on purpose: the manager's registry map is built in its
	//! server-only PostGameStart, so on a client it is empty forever. The authored array itself does
	//! replicate with the game-mode prefab, which is what makes a client-side lookup possible at all.
	protected ref map<string, ref OVT_TutorialEntryConfig> m_mEntriesById;

	//! The player's "Don't show tips again" setting. Read from the profile alongside the seen ids on
	//! the first store access, and flushed by SetTipsDisabled().
	protected bool m_bTipsDisabled;

	//! True from the moment an entry is handed to the UI until the UI reports it dismissed.
	protected bool m_bShowing;

	//! True while the pump timer is registered on the call queue.
	protected bool m_bPumpRunning;

	//! One-shot guard for the process-wide client-local hooks. Static because the hooks themselves
	//! are static: a listen-server host holds one OVT_TutorialComponent per CONNECTED PLAYER, and
	//! binding an instance handler from each of them would fire a local trigger once per player.
	protected static bool s_bLocalHooksBound;

	//-----------------------------------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Allocates the client-side collections so that every path is safe before the first delivery.
	void OVT_TutorialComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_Queue = new OVT_TutorialQueue();
		m_aMatchBuffer = new array<string>();
		m_mEntriesById = new map<string, ref OVT_TutorialEntryConfig>();
	}

	//------------------------------------------------------------------------------------------------
	//! \param owner The controller entity this component is attached to.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		BindLocalHooksOnce();
	}

	//------------------------------------------------------------------------------------------------
	//! Tears the pump timer down with the component. A CallLater left pointing at a deleted component
	//! is the classic Overthrow leak; this controller is deleted every time its player disconnects.
	//! \param owner The controller entity this component is attached to.
	override void OnDelete(IEntity owner)
	{
		StopPump();

		super.OnDelete(owner);
	}

	//-----------------------------------------------------------------------------------------------
	// DELIVERY - server side
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! SERVER: hands one entry id to this controller's owning client.
	//!
	//! The engine never loops an RPC back to the machine that sent it (the same fact that BUG-035 was
	//! about), so a listen-server host would never receive its own tips through Rpc(). When this
	//! controller is the host's own, the receive path is therefore called DIRECTLY instead - which is
	//! exactly what the remote client would have run, so the two cases cannot drift.
	//! \param[in] entryId Id of the entry to deliver. An empty id is refused.
	void Notify(string entryId)
	{
		if (entryId == "")
			return;

		if (!Replication.IsServer())
			return;

		if (IsOwnedByLocalPlayer())
		{
			RpcDo_ShowTutorial(entryId);
			return;
		}

		Rpc(RpcDo_ShowTutorial, entryId);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: one entry became due for this player, and only this player.
	//!
	//! A string id rather than an index into the entry array (plan decision D4): the array is
	//! append-only-by-convention, and a positional wire format is the exact fragility that
	//! starter-jobs-retirement is unpicking with jobIndex.
	//! \param[in] entryId Id of the entry that became due.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RpcDo_ShowTutorial(string entryId)
	{
		Receive(entryId);
	}

	//-----------------------------------------------------------------------------------------------
	// CLIENT-LOCAL TRIGGERS
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! CLIENT: matches a locally observed event against the authored entries and queues every hit.
	//!
	//! The client-local half of the pipeline. It shares the matcher, the seen check, the queue, the
	//! pump and the gate with the RPC path - the ONLY difference is where the event came from.
	//! \param[in] ctx The event occurrence. Null is a no-op.
	void FireLocalEvent(OVT_TutorialEventContext ctx)
	{
		if (!ctx)
			return;

		array<ref OVT_TutorialEntryConfig> entries = GetEntries();
		if (!entries)
			return;

		OVT_TutorialMatcher.FindMatches(entries, ctx, m_aMatchBuffer);

		foreach (string entryId : m_aMatchBuffer)
		{
			Receive(entryId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The local player opened the map. Static: routed to whichever component the LOCAL controller
	//! carries, which is the only one on this machine that may show anything.
	//! \return True when the local player's tutorial component received the event.
	static bool NotifyMapOpened()
	{
		return FireLocalEventOnLocalPlayer(OVT_TutorialEvent.MAP_OPENED, 0, "");
	}

	//------------------------------------------------------------------------------------------------
	//! The local player opened an Overthrow menu.
	//! \param[in] contextClassName Class name of the OVT_UIContext that opened; the trigger's filter.
	//! \return True when the local player's tutorial component received the event.
	static bool NotifyMenuOpened(string contextClassName)
	{
		return FireLocalEventOnLocalPlayer(OVT_TutorialEvent.MENU_OPENED, 0, contextClassName);
	}

	//------------------------------------------------------------------------------------------------
	//! The local player's character spawned and is under their control.
	//!
	//! The RETURN VALUE is what makes the caller's bounded retry possible: this is the one local
	//! trigger that races the async controller assignment (OVT_OverthrowGameMode's
	//! PushSpawnedTutorialTrigger), and "was there anyone to tell?" is a fact only this method has.
	//! \return True when the local player's tutorial component received the event.
	static bool NotifyPlayerSpawnedLocal()
	{
		return FireLocalEventOnLocalPlayer(OVT_TutorialEvent.PLAYER_SPAWNED, 0, "");
	}

	//------------------------------------------------------------------------------------------------
	//! Builds a context and fires it at the local player's tutorial component, if there is one.
	//!
	//! EVERY local trigger goes through here, and every one of them is allowed to be DROPPED. The
	//! controller is registered by an async RpcDo_NotifyOwnerAssignment, so a trigger fired in the
	//! seconds after a spawn or a join legitimately finds no component yet. A missed tip is a
	//! non-event; a script error on the client's first ten seconds is not (plan quality item Q7).
	//! \param[in] evt The event that occurred.
	//! \param[in] value The event's numeric payload.
	//! \param[in] filter The event's string payload.
	//! \return True when a local tutorial component existed and was given the event; false when the
	//! controller is not assigned yet, which is a drop and not an error.
	protected static bool FireLocalEventOnLocalPlayer(OVT_TutorialEvent evt, int value, string filter)
	{
		OVT_TutorialComponent tutorials = OVT_Global.GetTutorials();
		if (!tutorials)
			return false;

		OVT_TutorialEventContext ctx = new OVT_TutorialEventContext();
		ctx.m_eEvent = evt;
		ctx.m_iPlayerId = SCR_PlayerController.GetLocalPlayerId();
		ctx.m_iValue = value;
		ctx.m_sFilter = filter;

		tutorials.FireLocalEvent(ctx);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Binds the process-wide client-local hooks exactly once.
	//!
	//! Only the map hook is bound here; the other two client-local events have no invoker to bind to
	//! and are pushed from their one call site (OVT_UIContext.ShowLayout and
	//! OVT_OverthrowGameMode.OnPlayerSpawnedLocal) through the static Notify* methods above.
	//!
	//! Harmless on a dedicated server: no map is ever opened there, and the handler resolves the
	//! local player's component, of which a dedicated server has none.
	protected static void BindLocalHooksOnce()
	{
		if (s_bLocalHooksBound)
			return;

		s_bLocalHooksBound = true;

		ScriptInvokerBase<MapConfigurationInvoker> onMapOpen = SCR_MapEntity.GetOnMapOpen();
		if (onMapOpen)
			onMapOpen.Insert(OnMapOpened);
	}

	//------------------------------------------------------------------------------------------------
	//! SCR_MapEntity.GetOnMapOpen() handler.
	//! \param[in] config The map configuration. Unused - MAP_OPENED carries no payload.
	protected static void OnMapOpened(MapConfiguration config)
	{
		NotifyMapOpened();
	}

	//-----------------------------------------------------------------------------------------------
	// THE PIPELINE
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The single client receive path. Both the Owner RPC and every client-local trigger land here.
	//!
	//! Order is load-bearing:
	//!   1. SEEN -> drop. Checked once here rather than on every pump: an entry that has been shown
	//!      must never occupy a queue slot at all.
	//!   2. TIPS DISABLED -> drop, and specifically WITHOUT marking it seen (plan decision D6). A
	//!      player who turns tips back on has not "already seen" the ones they suppressed.
	//!   3. ENQUEUE, then start the pump. Whether it can be SHOWN is the gate's decision, made later
	//!      and repeatedly - which is what makes "the popup appears after the menu closes" true.
	//! \param[in] entryId Id of the entry that became due.
	protected void Receive(string entryId)
	{
		if (entryId == "")
			return;

		OVT_TutorialSeenStore store = GetSeenStore();
		if (store.HasSeen(entryId))
			return;

		if (m_bTipsDisabled)
			return;

		OVT_TutorialEntryConfig entry = FindEntry(entryId);
		if (!entry)
		{
			// The server sent an id this client cannot resolve: the two machines are running
			// different tutorial configs. Worth a line - it is silent otherwise, and unfixable
			// from the client.
			Print("[Overthrow.Tutorial] Received unknown entry id '" + entryId + "' - this client's authored entries do not contain it", LogLevel.WARNING);
			return;
		}

		if (!m_Queue.Enqueue(entryId, entry.m_iPriority))
			return;

		StartPump();
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the drain timer if it is not already running.
	protected void StartPump()
	{
		if (m_bPumpRunning)
			return;

		m_bPumpRunning = true;

		GetGame().GetCallqueue().CallLater(Pump, PUMP_INTERVAL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Stops the drain timer. Idempotent, and safe to call from OnDelete.
	protected void StopPump()
	{
		if (!m_bPumpRunning)
			return;

		m_bPumpRunning = false;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
			queue.Remove(Pump);
	}

	//------------------------------------------------------------------------------------------------
	//! One drain attempt: show the highest-priority pending entry if the gate allows it right now.
	//!
	//! The timer only runs while something is pending, so this is not a permanent per-second cost.
	//! A closed gate is NOT an error and NOT a drop - the entry stays queued and the next tick tries
	//! again, which is precisely the behaviour "not never and not on top" asks for.
	protected void Pump()
	{
		if (m_Queue.Count() == 0)
		{
			StopPump();
			return;
		}

		if (!OVT_TutorialGate.CanShowNow(m_bTipsDisabled, m_bShowing, IsBlockingUiOpen(), IsLocalPlayerAlive()))
			return;

		string entryId;
		if (!m_Queue.TryDequeue(entryId))
		{
			StopPump();
			return;
		}

		OVT_TutorialEntryConfig entry = FindEntry(entryId);
		if (!entry)
			return;

		// Set BEFORE the invoke: a subscriber that shows and immediately dismisses (or that throws)
		// must not leave the pipeline believing nothing is on screen.
		m_bShowing = true;

		m_OnShowTutorial.Invoke(entry);
	}

	//------------------------------------------------------------------------------------------------
	//! UI SURFACES: the entry currently on screen has gone away and must never come back.
	//!
	//! Called by both presentations, from every route that ends a popup (the auto-dismiss timer, the
	//! Dismiss button, the last page's Next). Idempotent, because the timer and an explicit dismiss
	//! can legitimately both fire.
	//! \param[in] entryId Id of the entry that was shown.
	void NotifyDismissed(string entryId)
	{
		m_bShowing = false;

		// The HasSeen test is not redundant with MarkSeen's idempotence: dismissing an already-seen
		// entry (the auto-dismiss timer and an explicit Dismiss can both fire) must not cost a second
		// disk flush for a byte-identical block.
		if (entryId != "")
		{
			OVT_TutorialSeenStore store = GetSeenStore();
			if (!store.HasSeen(entryId) && store.MarkSeen(entryId))
				PersistSettings();
		}

		if (m_Queue.Count() > 0)
			StartPump();
	}

	//-----------------------------------------------------------------------------------------------
	// GATE INPUTS - the three engine facts the pure gate cannot know
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether anything on screen must not be drawn over.
	//!
	//! Three separate facts, folded into the one bool OVT_TutorialGate takes: an Overthrow context
	//! owns the screen, a base-game menu is on top, or the map is open. Folding them HERE is what
	//! keeps the gate itself world-free and testable, and keeps "what counts as blocking" a question
	//! with one answer in one place.
	//!
	//! The first term asks IsAnyContextBlocking(), not IsAnyContextActive(): placement and building
	//! run with their menus CLOSED, so an active-contexts test would let a PLAYER_PLACE tip pop up
	//! in the middle of the very placement it describes.
	//! \return True while a popup would be intrusive. Every lookup is null-guarded; an unresolvable
	//! fact reads as "not blocking", because a missing menu manager is not an open menu.
	protected bool IsBlockingUiOpen()
	{
		OVT_UIManagerComponent ui = GetLocalUIManager();
		if (ui && ui.IsAnyContextBlocking())
			return true;

		MenuManager menus = GetGame().GetMenuManager();
		if (menus && menus.GetTopMenu())
			return true;

		// NOT named `map`: EnforceScript reserves that as a type name and rejects the local.
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity && mapEntity.IsOpen())
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while the local player has a controlled character and it is alive. No character
	//! at all (spawn screen, spectator, dedicated server) reads as not alive, which is correct: there
	//! is nobody to show a tip to.
	protected bool IsLocalPlayerAlive()
	{
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (!player)
			return false;

		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(player.FindComponent(SCR_CharacterControllerComponent));
		if (!controller)
			return false;

		return controller.GetLifeState() == ECharacterLifeState.ALIVE;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the local player's UI manager without going through OVT_Global.GetUI(), which
	//! dereferences the controlled entity unguarded and would throw on a dedicated server or between
	//! respawns - exactly the two moments this component runs in.
	//! \return The UI manager, or null.
	protected OVT_UIManagerComponent GetLocalUIManager()
	{
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (!player)
			return null;

		return OVT_UIManagerComponent.Cast(player.FindComponent(OVT_UIManagerComponent));
	}

	//-----------------------------------------------------------------------------------------------
	// HELPERS
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether this controller is the one the local player owns.
	//!
	//! Only ever true on a listen-server host (for the host's own controller) or on the client that
	//! owns it. On a dedicated server it is false for every controller, which is what makes Notify()
	//! always take the Rpc branch there.
	//! \return True when GetOwner() is the local player's controller.
	protected bool IsOwnedByLocalPlayer()
	{
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (!player)
			return false;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
			return false;

		int localPlayerId = SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(player);
		if (localPlayerId < FIRST_VALID_PLAYER_ID)
			return false;

		OVT_OverthrowController localController = players.GetController(localPlayerId);
		if (!localController)
			return false;

		return localController == GetOwner();
	}

	//------------------------------------------------------------------------------------------------
	//! The authored entry list, read off the tutorial manager on the game-mode entity.
	//! \return The entries, or null when the game mode has no tutorial manager (or none authored).
	protected array<ref OVT_TutorialEntryConfig> GetEntries()
	{
		OVT_TutorialManagerComponent manager = OVT_Global.GetTutorialManager();
		if (!manager)
			return null;

		return manager.GetEntries();
	}

	//------------------------------------------------------------------------------------------------
	//! Client-side id -> entry lookup, built lazily on first use and then cached.
	//!
	//! Rebuilt rather than shared with the manager's registry because that one is server-only, and it
	//! deliberately does NOT re-run the manager's duplicate validation: a client cannot fix a bad
	//! registry, the server already logged it by name, and refusing to resolve here would turn a
	//! server-side authoring mistake into a second, more confusing client-side symptom.
	//! \param[in] entryId The entry id. Exact and case-sensitive.
	//! \return The entry, or null when this client has no such id authored.
	protected OVT_TutorialEntryConfig FindEntry(string entryId)
	{
		if (entryId == "")
			return null;

		if (m_mEntriesById.Count() == 0)
			BuildEntryLookup();

		return m_mEntriesById.Get(entryId);
	}

	//------------------------------------------------------------------------------------------------
	//! Fills the client-side lookup from the authored entry array. First id wins on a duplicate.
	protected void BuildEntryLookup()
	{
		array<ref OVT_TutorialEntryConfig> entries = GetEntries();
		if (!entries)
			return;

		foreach (OVT_TutorialEntryConfig entry : entries)
		{
			if (!entry)
				continue;

			if (entry.m_sId == "")
				continue;

			if (m_mEntriesById.Contains(entry.m_sId))
				continue;

			m_mEntriesById.Insert(entry.m_sId, entry);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The per-machine seen store, allocated and LOADED FROM THE PROFILE on first use.
	//!
	//! Loaded lazily rather than in the constructor because the component is built on a controller
	//! entity that a dedicated server also owns, and the load is the one call in this class that
	//! touches the player's profile. Loading once is enough: this process is the only writer, so the
	//! in-memory store and the profile block cannot drift apart afterwards.
	//! \return The store. Never null.
	protected OVT_TutorialSeenStore GetSeenStore()
	{
		if (m_SeenStore)
			return m_SeenStore;

		m_SeenStore = new OVT_TutorialSeenStore();

		bool tipsDisabled;
		if (OVT_TutorialSettingsAccessor.Load(m_SeenStore, tipsDisabled))
			m_bTipsDisabled = tipsDisabled;

		return m_SeenStore;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the seen ids and the tips flag back to the player's profile and flushes to disk.
	//!
	//! Called from every mutation rather than only on exit (plan decision D8): the value of this
	//! store is entirely in a tip never repeating, and a crash or an alt-F4 is the common exit in
	//! this game. Silent when the profile is unavailable - the caller has nothing useful to do about
	//! it and the session still behaves correctly in memory.
	protected void PersistSettings()
	{
		OVT_TutorialSettingsAccessor.Save(GetSeenStore(), m_bTipsDisabled);
	}

	//-----------------------------------------------------------------------------------------------
	// GETTERS / SETTERS
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \return True while a tutorial popup of either presentation is on screen.
	bool IsShowing()
	{
		return m_bShowing;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The player's "Don't show tips again" setting.
	bool GetTipsDisabled()
	{
		return m_bTipsDisabled;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the "Don't show tips again" setting. Disabling also empties the pending queue: those tips
	//! were queued under the old setting and re-showing them the moment tips come back on would make
	//! the toggle look broken.
	//! The flag is flushed to the profile immediately, so it survives an unclean exit - a player who
	//! turned tips off and then crashed must not be shown tips again on the next launch.
	//! \param[in] disabled True to suppress every subsequent popup.
	void SetTipsDisabled(bool disabled)
	{
		// Read the store BEFORE mutating the flag: the first access loads the profile, and that load
		// would otherwise overwrite the value just set with the one still on disk.
		GetSeenStore();

		m_bTipsDisabled = disabled;

		PersistSettings();

		if (!disabled)
			return;

		m_Queue.Clear();
		StopPump();
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many entries are waiting for the gate to open.
	int GetPendingCount()
	{
		return m_Queue.Count();
	}
}
