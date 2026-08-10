[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server->owning-client tutorial delivery and the whole client-side tutorial pipeline")]
class OVT_TutorialComponentClass : OVT_ComponentClass {};

//------------------------------------------------------------------------------------------------
//! The client half of the tutorial framework, on the per-player OVT_OverthrowController entity.
//!
//! Two jobs, and the split between them is the whole point of the design:
//!
//!  1. DELIVERY. RplRcver.Owner RPCs, one per player. The server resolves the acting player's
//!     controller and calls Notify() with an entry id; nobody else's client hears it. This is
//!     deliberately NOT the OVT_NotificationManagerComponent pattern of broadcasting to everyone and
//!     filtering client-side, and it is the exact failure mode (per-player delivery on a dedicated
//!     server) that killed the starter jobs in BUG-037. SetSpawnContext() is the second such RPC and
//!     carries a FACT rather than a decision: what that one player's spawn actually gave them, which
//!     the client provably cannot work out for itself and which the PLAYER_SPAWNED trigger filters on.
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

	//! The entry the Tips re-read screen falls back to when the player has seen nothing yet. Preferred
	//! over its houseless twin because it is the entry the overwhelming majority of players get.
	static const string REVIEW_FALLBACK_ID = "welcome-intro";

	//! The second fallback, used only when REVIEW_FALLBACK_ID does not resolve on this machine. Both
	//! welcomes are four-page MODAL entries, so either renders correctly in the popup. NOT chosen by
	//! spawn context: the Tips screen is a re-read surface, not a spawn event, and matching a filter
	//! there would make the button's behaviour depend on a fact that has nothing to do with reading.
	static const string REVIEW_FALLBACK_ID_NOHOME = "welcome-nohome";

	//! The two values a spawn context can take. These are the exact strings the two welcome entries'
	//! PLAYER_SPAWNED triggers are authored against in Configs/Tutorials/, so changing either of them
	//! is a config change as much as a code change.
	static const string SPAWN_CONTEXT_HOUSE = "house";
	static const string SPAWN_CONTEXT_NOHOUSE = "nohouse";

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

	//! The entry handed to the UI, held for exactly as long as m_bShowing is true.
	//!
	//! Read by OVT_UIManagerComponent every frame to decide whether the HUD tip's input context should
	//! be active: the Learn more prompt on the non-modal overlay must be live while a tip is on screen
	//! and dead the rest of the time, and "is a non-modal tip up?" is a question only this side of the
	//! pipeline can answer without reaching into a HUD display.
	protected OVT_TutorialEntryConfig m_ShowingEntry;

	//! True while the pump timer is registered on the call queue.
	protected bool m_bPumpRunning;

	//! THE CLIENT'S STORED ANSWER TO "WHAT DID MY SPAWN ACTUALLY GIVE ME?", pushed here by the server
	//! through SetSpawnContext() and read into every PLAYER_SPAWNED context this machine fires.
	//!
	//! The client provably cannot derive this for itself (plan decision D4): the fallback spawn calls
	//! SetHomePos() too, so a home position exists on BOTH branches of FinalizePlayerPreparation, and
	//! the player-manager snapshot a joining client receives is sent before finalization has run.
	//!
	//! DEFAULTS TO "house" AND NEVER TO "" - THAT ASYMMETRY IS LOAD-BEARING, NOT UNTIDINESS.
	//! OVT_TutorialTrigger.Matches:116 treats "" on the TRIGGER as "no filter", but its test is
	//! `m_sFilter != "" && ctx.m_sFilter != m_sFilter`, so a "" on the CONTEXT matches no FILTERED
	//! trigger at all. An empty default would therefore suppress BOTH welcomes rather than degrading
	//! to one of them. "house" degrades to exactly the behaviour that shipped before the spawn context
	//! existed: everybody reads the house page, which is right for everybody who has a house.
	//! Do not "tidy" this to "".
	protected string m_sSpawnContextFilter = OVT_TutorialComponent.SPAWN_CONTEXT_HOUSE;

	//! True once the server's answer has actually ARRIVED on this machine, as opposed to the default
	//! above standing in for it. NotifyPlayerSpawnedLocal() reports "not delivered" while this is
	//! false, which is what makes the game mode's existing bounded retry wait for the fact rather than
	//! fire PLAYER_SPAWNED with a guess (plan decision D7).
	protected bool m_bSpawnContextReceived;

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

	//------------------------------------------------------------------------------------------------
	//! SERVER: tells one player's client what their spawn actually gave them.
	//!
	//! Mirrors Notify()'s local-direct-call branch, and for the same reason: the engine never loops an
	//! RPC back to the machine that sent it, so without the direct call a listen host and a single
	//! player would never receive their own context and would silently sit on the default forever.
	//!
	//! THE OWNERSHIP TEST IS A PLAYER-ID COMPARISON, NOT Notify()'s IsOwnedByLocalPlayer(). That
	//! helper dereferences SCR_PlayerController.GetLocalControlledEntity(), and this method's one
	//! caller (OVT_OverthrowGameMode.FinalizePlayerPreparation) runs BEFORE the character exists on
	//! the load-a-save path - OVT_SpawnLogic.DoSpawn_S:150-160 finalizes first and only then calls
	//! SpawnDeferredPlayer. An entity-dereferencing test would answer "not mine" there and push the
	//! host's own context out over the wire, where it would be dropped.
	//! \param[in] playerId Runtime id of the player this controller belongs to.
	//! \param[in] filter SPAWN_CONTEXT_HOUSE or SPAWN_CONTEXT_NOHOUSE. An empty filter is refused.
	void SetSpawnContext(int playerId, string filter)
	{
		if (filter == "")
			return;

		if (!Replication.IsServer())
			return;

		int localPlayerId = SCR_PlayerController.GetLocalPlayerId();
		if (localPlayerId >= FIRST_VALID_PLAYER_ID && localPlayerId == playerId)
		{
			RpcDo_SetSpawnContext(filter);
			return;
		}

		// ARITY, PROVEN BY INSPECTION (BUG-090). RpcDo_SetSpawnContext declares exactly ONE parameter
		// (string filter), so this call carries TWO arguments in total: the method plus that one
		// parameter. Rpc() is an untyped variadic proto - a wrong argument count compiles clean,
		// passes every automated test, and then dies silently at the wire with no log line anywhere.
		// The only observation that can catch it is a two-client multiplayer play-test.
		Rpc(RpcDo_SetSpawnContext, filter);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: the server's answer to what this player's spawn gave them.
	//!
	//! An empty filter is refused here as well as at the sender: the invariant this class relies on is
	//! that m_sSpawnContextFilter is never "", because a "" context matches no filtered trigger and
	//! would suppress every welcome rather than degrade to one (see the field's own comment).
	//! \param[in] filter SPAWN_CONTEXT_HOUSE or SPAWN_CONTEXT_NOHOUSE.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RpcDo_SetSpawnContext(string filter)
	{
		if (filter == "")
			return;

		m_sSpawnContextFilter = filter;
		m_bSpawnContextReceived = true;
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
	//! THE RETURN VALUE IS A DELIVERY CONTRACT, AND IT NOW ANSWERS TWO QUESTIONS RATHER THAN ONE.
	//! It used to mean only "was there anyone to tell?" - this is the one local trigger that races the
	//! async controller assignment, so the component legitimately may not exist yet. It now ALSO means
	//! "has the server's spawn context arrived?", because the welcome entries are FILTERED on that
	//! context and firing PLAYER_SPAWNED before it lands shows a houseless player the house page.
	//!
	//! Both answers are false-means-try-again, which is exactly what the caller's existing bounded
	//! retry already does with a false (OVT_OverthrowGameMode.PushSpawnedTutorialTrigger). On its
	//! FINAL attempt that caller passes acceptDefaultContext, which drops the second question and
	//! fires with whatever the context field holds - so a context that never arrives costs one page of
	//! accuracy and NEVER the whole welcome (plan decision D7, "degrade, never disappear").
	//! \param[in] acceptDefaultContext True to fire even when the server's context never arrived, so
	//! that the default "house" is used rather than the trigger being dropped altogether.
	//! \return True when a local tutorial component existed AND (the context has arrived OR
	//! acceptDefaultContext is true). False is a "not delivered, ask again" and never an error.
	static bool NotifyPlayerSpawnedLocal(bool acceptDefaultContext = false)
	{
		return FireLocalEventOnLocalPlayer(OVT_TutorialEvent.PLAYER_SPAWNED, 0, "", acceptDefaultContext);
	}

	//------------------------------------------------------------------------------------------------
	//! Builds a context and fires it at the local player's tutorial component, if there is one.
	//!
	//! EVERY local trigger goes through here, and every one of them is allowed to be DROPPED. The
	//! controller is registered by an async RpcDo_NotifyOwnerAssignment, so a trigger fired in the
	//! seconds after a spawn or a join legitimately finds no component yet. A missed tip is a
	//! non-event; a script error on the client's first ten seconds is not (plan quality item Q7).
	//!
	//! PLAYER_SPAWNED IS THE ONE EVENT WHOSE FILTER IS NOT THE CALLER'S TO SUPPLY. Its filter is the
	//! server-authored spawn context held on this component, so it is substituted here rather than
	//! passed in - there is no caller that could know it, and a second source for it would be a second
	//! thing to keep in step. Every other event still carries whatever its own call site passed.
	//! \param[in] evt The event that occurred.
	//! \param[in] value The event's numeric payload.
	//! \param[in] filter The event's string payload. IGNORED for PLAYER_SPAWNED, which substitutes the
	//! stored spawn context.
	//! \param[in] acceptDefaultContext PLAYER_SPAWNED only: fire even when the server's spawn context
	//! has not arrived, using the default. Ignored by every other event.
	//! \return True when a local tutorial component existed and was given the event; false when the
	//! controller is not assigned yet, or when PLAYER_SPAWNED is still waiting on its context. Both
	//! are a "not yet" for the caller to retry, and neither is an error.
	protected static bool FireLocalEventOnLocalPlayer(OVT_TutorialEvent evt, int value, string filter, bool acceptDefaultContext = false)
	{
		OVT_TutorialComponent tutorials = OVT_Global.GetTutorials();
		if (!tutorials)
			return false;

		string eventFilter = filter;

		if (evt == OVT_TutorialEvent.PLAYER_SPAWNED)
		{
			if (!tutorials.m_bSpawnContextReceived && !acceptDefaultContext)
				return false;

			eventFilter = tutorials.m_sSpawnContextFilter;
		}

		OVT_TutorialEventContext ctx = new OVT_TutorialEventContext();
		ctx.m_eEvent = evt;
		ctx.m_iPlayerId = SCR_PlayerController.GetLocalPlayerId();
		ctx.m_iValue = value;
		ctx.m_sFilter = eventFilter;

		tutorials.FireLocalEvent(ctx);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Binds the process-wide client-local hooks, exactly once per map-entity lifetime.
	//!
	//! Only the map hook is bound here; the other two client-local events have no invoker to bind to
	//! and are pushed from their one call site (OVT_UIContext.ShowLayout and
	//! OVT_OverthrowGameMode.OnPlayerSpawnedLocal) through the static Notify* methods above.
	//!
	//! REMOVE-THEN-INSERT, NOT A ONE-SHOT STATIC BOOL. The obvious guard - a `static bool` set on the
	//! first bind - is wrong here, and silently so. SCR_MapEntity's invoker is a process-lifetime
	//! static that its DESTRUCTOR empties (~SCR_MapEntity calls s_OnMapOpen.Clear()), so a world
	//! teardown drops this subscription while leaving any such flag set. The map tip would then work
	//! in the first campaign of a process and never again in that process - and this project reloads
	//! the world several times per process (see OVT_PersistenceManagerComponent.OnDelete's header).
	//! Remove() on a handler that is not subscribed is a no-op, so this binds exactly once per
	//! invoker however many components call it - which is what the flag was there to guarantee: a
	//! listen-server host holds one OVT_TutorialComponent per CONNECTED PLAYER, and a second
	//! subscription would fire the local trigger once per player. Same shape as
	//! OVT_TutorialContext.TrySubscribe().
	//!
	//! Harmless on a dedicated server: no map is ever opened there, and the handler resolves the
	//! local player's component, of which a dedicated server has none.
	protected static void BindLocalHooksOnce()
	{
		ScriptInvokerBase<MapConfigurationInvoker> onMapOpen = SCR_MapEntity.GetOnMapOpen();
		if (!onMapOpen)
			return;

		onMapOpen.Remove(OnMapOpened);
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

		// PEEK, not dequeue: the gate's blocking-UI veto is now the pending ENTRY's to waive, so the
		// pump has to know which entry it is about to show before it may decide. A dequeue here would
		// have to put a refused entry back, which reorders the queue on every tick.
		string pendingId;
		if (!m_Queue.TryPeek(pendingId))
		{
			StopPump();
			return;
		}

		if (!OVT_TutorialGate.CanShowNow(m_bTipsDisabled, m_bShowing, IsBlockingUiOpen(), IsLocalPlayerAlive(), ShowsOverUi(FindEntry(pendingId))))
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
		m_ShowingEntry = entry;

		m_OnShowTutorial.Invoke(entry);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this entry is one that belongs ON TOP of the screen that triggered it.
	//!
	//! THE ONE COPY OF THE "MODAL CANNOT" RULE. m_bShowOverUI is honoured only for entries the HUD
	//! overlay will actually present: the modal surface is an OVT_UIContext, so a modal drawn over
	//! another Overthrow menu would leave two contexts alive with both binding MenuBack. Everything
	//! that asks this question - the gate here and the overlay's auto-dismiss - asks it through this
	//! method, so the two can never disagree about a given entry.
	//! \param[in] entry The entry in question. Null shows over nothing.
	//! \return True when the entry declares m_bShowOverUI and is not modal.
	bool ShowsOverUi(OVT_TutorialEntryConfig entry)
	{
		if (!entry)
			return false;

		if (!entry.m_bShowOverUI)
			return false;

		return !OVT_TutorialContext.IsModal(entry);
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
		m_ShowingEntry = null;

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
	//! Whether the NON-MODAL overlay specifically is presenting something right now.
	//!
	//! The activation condition for OverthrowTutorialHudContext, which carries the overlay's Learn
	//! more binding. Deliberately excludes the modal: that surface is an OVT_UIContext and activates
	//! its own OverthrowTutorialMenuContext, and both contexts being live at once would put two
	//! actions on the same intent.
	//! \return True while an entry the HUD overlay owns is on screen.
	bool IsShowingNonModal()
	{
		if (!m_bShowing)
			return false;

		if (!m_ShowingEntry)
			return false;

		return !OVT_TutorialContext.IsModal(m_ShowingEntry);
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
	//! Clears the player's tutorial progress: every seen id, plus the "don't show tips again" flag.
	//!
	//! THE ONLY SUPPORTED WAY TO RESET FROM THE UI. OVT_TutorialSettingsAccessor.Reset() writes an
	//! empty record straight to the profile and leaves this component's in-memory store and flag
	//! untouched - so the running session still believes every id is seen, and the next MarkSeen or
	//! SetTipsDisabled writes them all straight back. This component is documented as the ONLY writer
	//! (see GetSeenStore), and that invariant is what a direct accessor call would break.
	//!
	//! Ordering is the same caution SetTipsDisabled carries: GetSeenStore() is what LOADS the profile
	//! on first access, so it has to run BEFORE the flag is set. Clearing through its return value
	//! rather than after a bare call makes that ordering impossible to get wrong by re-editing.
	//!
	//! Deliberately does NOT touch the pending queue. Re-enabling tips is exactly the SetTipsDisabled
	//! (false) case, which also leaves the queue alone: anything still queued was matched under the
	//! old setting and is legitimately still due.
	void ResetSeen()
	{
		OVT_TutorialSeenStore store = GetSeenStore();

		store.Clear();

		m_bTipsDisabled = false;

		PersistSettings();
	}

	//------------------------------------------------------------------------------------------------
	//! Picks an entry for the Tips re-read screen when nothing has been shown this session.
	//!
	//! The fallback chain the Tips menu entry relies on, in order:
	//!   1. an entry the player has already seen (resolved here);
	//!   2. the welcome, when they have seen nothing at all;
	//!   3. null, when this install has no resolvable entries - the caller must then refuse to open,
	//!      which OVT_TutorialContext.CanShowLayout() already does.
	//!
	//! A SEEN ID CAN OUTLIVE ITS .CONF. Entries get renamed, removed, or differ between a server and
	//! a client, so every candidate is resolved through FindEntry AND checked for pages: an id that
	//! no longer resolves must be skipped, not handed to the popup as a blank modal.
	//!
	//! "MOST RECENT" IS AN APPROXIMATION AND IS DOCUMENTED AS ONE. The walk runs newest-first over
	//! the store's own order, but OVT_TutorialSeenStore is backed by a sorted set<string> and
	//! WriteTo() says so explicitly - the order is canonical, not chronological, and a profile round
	//! trip normalises it. Nothing here depends on getting the genuinely-latest tip: every candidate
	//! is something the player has read, and any of them is a valid thing to re-read.
	//! \return An entry with at least one page, or null when nothing at all resolves.
	OVT_TutorialEntryConfig FindReviewEntry()
	{
		array<string> seenIds = new array<string>();
		GetSeenStore().WriteTo(seenIds);

		for (int i = seenIds.Count() - 1; i >= 0; i--)
		{
			OVT_TutorialEntryConfig seen = FindEntry(seenIds[i]);
			if (IsReadable(seen))
				return seen;
		}

		OVT_TutorialEntryConfig welcome = FindEntry(REVIEW_FALLBACK_ID);
		if (IsReadable(welcome))
			return welcome;

		welcome = FindEntry(REVIEW_FALLBACK_ID_NOHOME);
		if (IsReadable(welcome))
			return welcome;

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entry The candidate. Null is not readable.
	//! \return True when this entry has something the popup can actually draw.
	protected bool IsReadable(OVT_TutorialEntryConfig entry)
	{
		if (!entry)
			return false;

		if (!entry.m_aPages)
			return false;

		return !entry.m_aPages.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many entries are waiting for the gate to open.
	int GetPendingCount()
	{
		return m_Queue.Count();
	}
}
