[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative, GM-gated campaign state snapshots for one player")]
class OVT_GMRequestComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! What a GM client may ask this seam for. Versioned and extensible on purpose: gm-map adds
//! THREAT_GRID here later and gets its own record RPC under the same seq/version framing, without
//! changing a single existing signature.
//------------------------------------------------------------------------------------------------
enum OVT_EGMRequestType
{
	CAMPAIGN_SNAPSHOT,

	//! One AI group's waypoint route, asked for by RplId when a GM's selection changes. APPENDED, and
	//! every future value must be too: the ordinal is what crosses the wire, so inserting above an
	//! existing name would re-point every older client's request type at a different handler.
	GROUP_WAYPOINTS
}

//------------------------------------------------------------------------------------------------
//! The Game Master state seam, on the per-player OVT_OverthrowController entity.
//!
//! ! THE INVARIANT, AND IT IS NOT NEGOTIABLE: every current and future RpcAsk_* handler on this
//! component must call IsAuthorizedGM(ResolveOwningPlayerId()) at the top and return without sending
//! anything at all when it answers false, before reading any campaign state. This component exists
//! to hand out occupying-faction internals; an ungated handler here leaks the campaign's hidden
//! numbers to every player who can send an RPC.
//!
//! WHAT IT DOES. A GM client asks for a campaign snapshot when the Game Master editor opens and
//! re-asks every m_fPollIntervalMs while it stays open. The server answers with a FAN of small
//! owner-targeted RPCs framed Begin ... End, every one of them carrying the CLIENT-GENERATED
//! sequence id, so a client that has already superseded a request discards its late arrivals instead
//! of interleaving them with the current one. The client stages the fan and commits it in one step
//! into OVT_GMCampaignState, then fires GetOnSnapshotUpdated(). Siblings read the store; nothing
//! outside this file touches an RPC.
//!
//! IT IS STRICTLY READ-ONLY. No path here mutates campaign state - in particular the predicted next
//! distribution comes from OVT_GMSchedule.PredictResourceGain(), NOT from
//! OVT_OccupyingFactionManager.GainResources(), which computes the same number and then ADDS it.
//! Calling GainResources() from here would hand the occupying faction a free income tick every time
//! a GM opened the editor.
//!
//! NAMED FOR THE ROLE, NOT THE PAYLOAD. Phase 2 of the GM epic adds write actions (give resources,
//! adjust funds, spawn deployments) to this same component, gated by the same static. The STATE
//! lives in OVT_GMCampaignState rather than in fields here, which also honours the base class's rule
//! that a request component carries no domain state.
//!
//! THE SECOND FAN: GROUP WAYPOINT ROUTES. Waypoint entities carry no RplComponent at all, so they
//! exist only on the server and a GM client can only learn a route by being told. RequestGroupWaypoints()
//! asks for ONE group's route by RplId; the server answers with WaypointsBegin ... Waypoint x n ...
//! WaypointsEnd, and the client commits it into GetRoute() and fires GetOnRouteUpdated(). It is the
//! same framing as the snapshot with ONE deliberate difference: the route runs on its OWN sequence
//! counter (m_iRouteSeq / m_iRouteStagingSeq). Sharing the snapshot's m_iSeq would let the 8-second
//! poll silently invalidate a route fan that was still in flight.
//!
//! AN EMPTY ANSWER IS STILL AN ANSWER on that fan. A group with no waypoints, an RplId that resolves
//! to nothing and an RplId that resolves to something that is not a group all receive
//! Begin(count 0, currentIndex -1) + End(0). ONLY an authorization failure produces silence - the
//! consumer fetches once per selection, so a silent "found nothing" would leave the previous group's
//! route drawn forever.
//!
//! LISTEN-SERVER CORRECTNESS. Every RpcDo_* send site uses the ShouldRespondLocally short-circuit
//! (the BUG-090 family: the engine never loops an Rpc back to the sender, so a host's owner-targeted
//! response to its own controller is silently dropped) and the public request entry point branches on
//! Replication.IsServer(). Without both, the single most likely GM in the world - the host of a
//! self-hosted server - sees a permanently empty panel with nothing in any log.
//------------------------------------------------------------------------------------------------
class OVT_GMRequestComponent : OVT_ControllerRequestComponent
{
	//! Wire format version, echoed in every SnapshotBegin. A client that does not recognise the
	//! version REFUSES TO STAGE rather than mis-parsing a fan from a mismatched build.
	//!
	//! ⚠ BUMP IT WHENEVER THE SHAPE OF THE FAN CHANGES, not only when a field changes meaning.
	//!  1 -> 2  occupying/counter-attacks Phase 8 appended the CampaignObjective record. A 1-speaking
	//!          client would stage a fan whose End reports one more record than it ever received and
	//!          would show a permanently short m_iReportedRecordCount; refusing to stage at all is the
	//!          loud failure this field exists to produce.
	static const int WIRE_VERSION = 2;

	//! Minimum real seconds between two refusal log lines from this player. The component is
	//! per-player, so one field rate-limits per player and a spamming client cannot flood the log.
	static const float REFUSAL_LOG_INTERVAL = 10;

	//! How far the truncation warning re-walks a pathological group to name its TRUE waypoint count.
	//! Bounded rather than unlimited: the point is a useful number in a log line, not a complete walk
	//! of a route that is already known to be broken.
	static const int TRUNCATION_PROBE_CAP = 4096;

	//! Minimum real seconds between two route-truncation warnings from this player. Same throttle
	//! shape as REFUSAL_LOG_INTERVAL: a GM click-spamming a pathological group must not flood the log.
	static const float TRUNCATION_LOG_INTERVAL = 10;

	//! CLI switch that authorises everyone. Read SERVER-SIDE ONLY, so a client cannot set it.
	//! Follows the existing -ovtDevUid precedent (OVT_Global.c:4).
	static const string DEV_CLI_PARAM = "ovtGmDev";

	//! Campaign-wide records in every fan: CampaignResources, CampaignSchedule and CampaignObjective.
	//! They are outside m_iMaxRecordsPerSnapshot's budget on purpose - the cap exists to bound the
	//! PER-ENTITY fan, and a snapshot that dropped its campaign scalars to make room for a base record
	//! would be useless.
	//!
	//! ⚠ IT MUST EQUAL THE NUMBER OF SendCampaign* CALLS IN SendCampaignSnapshot(), AND NOTHING CHECKS
	//! THAT FOR YOU. SendSnapshotEnd reports CAMPAIGN_RECORD_COUNT + perEntityRecords as the total the
	//! server sent, and the client compares that against what it actually received; a count left behind
	//! makes every snapshot look truncated, and a count run ahead hides a record that really was lost.
	//! A record added here is also a WIRE_VERSION bump - see that field.
	static const int CAMPAIGN_RECORD_COUNT = 3;

	[Attribute(defvalue: "8000", desc: "How often a GM client re-requests the campaign snapshot while the editor is open, in milliseconds")]
	protected float m_fPollIntervalMs;

	[Attribute(defvalue: "400", desc: "Safety valve: most records one snapshot may carry (used by the per-entity fan)")]
	protected int m_iMaxRecordsPerSnapshot;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Print snapshot build info server-side and staging commits client-side")]
	protected bool m_bDebugSnapshotTiming;

	[Attribute(defvalue: "32", desc: "Safety valve: most waypoints one group's route fan may carry (real Overthrow routes are 9 or fewer)")]
	protected int m_iMaxWaypointsPerGroup;

	//! CLIENT: the store siblings read. Never null - a sibling may ask for it before the first
	//! snapshot lands and gets an empty store rather than a null check it will forget to write.
	protected ref OVT_GMCampaignState m_State = new OVT_GMCampaignState();

	//! CLIENT: the half-assembled snapshot. Committed into m_State in one step on SnapshotEnd, so no
	//! reader can ever observe a mixture of two snapshots.
	protected ref OVT_GMCampaignState m_Staging = new OVT_GMCampaignState();

	//! CLIENT: fired when a complete snapshot has been committed.
	protected ref ScriptInvoker m_OnSnapshotUpdated;

	//! CLIENT: fired when the editor closed and the store was emptied.
	protected ref ScriptInvoker m_OnStateCleared;

	//! CLIENT: the sequence id of the request currently in flight. Incremented per request; echoed
	//! by the server untouched. It cannot collide because this component is per-player.
	protected int m_iSeq;

	//! CLIENT: the sequence a Begin opened staging for. A record whose seq differs is dropped.
	protected int m_iStagingSeq = -1;

	//! CLIENT: whether a Begin has opened staging that an End has not yet committed.
	protected bool m_bStaging;

	//! CLIENT: one log line per session for a wire-version mismatch, not one per poll.
	protected bool m_bLoggedVersionMismatch;

	//! CLIENT: the editor manager this machine's player owns, delivered by the editor core.
	protected SCR_EditorManagerEntity m_EditorManager;

	//! CLIENT: the one live waypoint route - the GM's current selection and nothing else (plan 5 D10).
	//! Never null; a consumer that asks before any route lands gets an empty, incomplete route.
	protected ref OVT_GMWaypointRoute m_Route = new OVT_GMWaypointRoute();

	//! CLIENT: the half-assembled route. Committed into m_Route in one step on WaypointsEnd.
	protected ref OVT_GMWaypointRoute m_RouteStaging = new OVT_GMWaypointRoute();

	//! CLIENT: fired when a complete route - possibly an empty one - has been committed.
	protected ref ScriptInvoker m_OnRouteUpdated;

	//! CLIENT: the route request currently in flight. SEPARATE from m_iSeq on purpose: the snapshot
	//! poll and the route fan supersede only their own requests.
	protected int m_iRouteSeq;

	//! CLIENT: the sequence a WaypointsBegin opened route staging for. A record whose seq differs is
	//! dropped.
	protected int m_iRouteStagingSeq = -1;

	//! CLIENT: whether a WaypointsBegin has opened route staging that a WaypointsEnd has not committed.
	protected bool m_bRouteStaging;

	//! SERVER: world time (ms) of the last refusal log line for this player. See REFUSAL_LOG_INTERVAL.
	protected float m_fLastRefusalLog;

	//! SERVER: world time (ms) of the last truncation warning for this player, throttled exactly like
	//! the refusal line and for the same reason.
	protected float m_fLastTruncationLog;

	//! SERVER: the read-only manager walk. Created on the first authorized request and reused, so a
	//! poll every m_fPollIntervalMs does not churn four arrays per GM. It stays null forever on a
	//! machine that never serves a snapshot, which is every client.
	protected ref OVT_GMSnapshotBuilder m_Builder;

	//------------------------------------------------------------------------------------------------
	// THE GATE
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether a player may read campaign state. SERVER-SIDE ONLY - on a client the engine's role
	//! flags for another connection are not authoritative and this answer means nothing.
	//!
	//! GAME_MASTER **or** admin, deliberately both: the GAME_MASTER role is only held while a
	//! player's editor is unlimited (SCR_EditorManagerEntity.UpdateLimited()), and Overthrow's editor
	//! is not configured to grant that yet, so today it is admins who actually get an unlimited
	//! editor. HasPlayerRole is what keeps working once the editor IS configured. Neither alone is
	//! sufficient.
	//!
	//! Re-checked on EVERY handler rather than cached at connect: roles change mid-session (an admin
	//! logs in, a vote promotes someone, an editor mode is removed), and a cached authorization is a
	//! stale authorization. It is two proto calls - cheaper than the record it guards.
	//! \param[in] playerId Runtime id of the caller, as resolved from the entity the RPC arrived on.
	//! \return True when the player may be sent campaign state.
	static bool IsAuthorizedGM(int playerId)
	{
		if(playerId <= 0) return false;

#ifdef WORKBENCH
		// Workbench Play mode is a debug context (user decision 2026-08-15: the GM panel is a debug
		// tool and must light up there), and Play mode offers no admin login or role grant to satisfy
		// the gate. WORKBENCH is defined only by addon.gproj's workbench script configuration - never
		// in a shipping client or dedicated-server build - so this authorizes nobody in real MP.
		return true;
#endif

		// Local play-testing only, and server-side, so a client cannot grant itself anything.
		if(System.IsCLIParam(DEV_CLI_PARAM)) return true;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if(!playerManager) return false;

		if(playerManager.HasPlayerRole(playerId, EPlayerRole.GAME_MASTER)) return true;

		return SCR_Global.IsAdmin(playerId);
	}

	//------------------------------------------------------------------------------------------------
	// CLIENT-SIDE API - the sibling contract
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The client-side campaign store. Never null; check HasData() for whether a snapshot has landed.
	//! \return The store.
	OVT_GMCampaignState GetState()
	{
		return m_State;
	}

	//------------------------------------------------------------------------------------------------
	//! Fired after a complete snapshot has been committed into GetState(). No arguments: the store is
	//! the payload.
	//! \return The invoker.
	ScriptInvoker GetOnSnapshotUpdated()
	{
		if(!m_OnSnapshotUpdated) m_OnSnapshotUpdated = new ScriptInvoker();

		return m_OnSnapshotUpdated;
	}

	//------------------------------------------------------------------------------------------------
	//! Fired when the editor closed and the store was emptied. Consumers must hide whatever they drew.
	//! \return The invoker.
	ScriptInvoker GetOnStateCleared()
	{
		if(!m_OnStateCleared) m_OnStateCleared = new ScriptInvoker();

		return m_OnStateCleared;
	}

	//------------------------------------------------------------------------------------------------
	//! The client-side waypoint route of the group most recently asked about. Never null.
	//!
	//! Check m_bComplete (or HasRoute()) rather than the array count for "has an answer arrived": a
	//! COMPLETE ROUTE WITH ZERO WAYPOINTS is a real answer meaning "that group has none", and it is
	//! what clears a previous group's drawing.
	//! \return The route store.
	OVT_GMWaypointRoute GetRoute()
	{
		return m_Route;
	}

	//------------------------------------------------------------------------------------------------
	//! Fired after a complete route fan has been committed into GetRoute(), empty ones included. No
	//! arguments: the store is the payload.
	//! \return The invoker.
	ScriptInvoker GetOnRouteUpdated()
	{
		if(!m_OnRouteUpdated) m_OnRouteUpdated = new ScriptInvoker();

		return m_OnRouteUpdated;
	}

	//------------------------------------------------------------------------------------------------
	// CLIENT LIFECYCLE
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Subscribes to the editor core so this machine learns when ITS player's editor manager exists.
	//!
	//! Event_OnEditorManagerInitOwner only ever fires on the owner's own machine
	//! (SCR_EditorManagerEntity.c:2132), which is why no retry loop is needed here. A dedicated server
	//! never sees it fire, and every ownership question is answered again in IsLocalControllerOwner()
	//! before a single byte is sent - a client carries a replicated instance of this component for
	//! every connected player's controller, and only the local player's may ever poll.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if(SCR_Global.IsEditMode()) return;

		SCR_EditorManagerCore core = SCR_EditorManagerCore.Cast(SCR_EditorManagerCore.GetInstance(SCR_EditorManagerCore));
		if(!core) return;

		// Remove-then-Insert: a second init must never leave two subscriptions behind.
		core.Event_OnEditorManagerInitOwner.Remove(OnEditorManagerInitOwner);
		core.Event_OnEditorManagerInitOwner.Insert(OnEditorManagerInitOwner);

		// The owner's editor manager may ALREADY exist by the time this component initialises - the
		// init event fired before the subscription above and will not fire again. Observed 2026-08-15
		// as a permanent "Waiting for campaign data" (Workbench Play mode and dedicated server): the
		// open/close invokers were never wired, so polling never started. Catch up explicitly -
		// GetInstance() returns the LOCAL player's manager, which is the only one this machine may
		// poll for; remote controller instances that also run this attach the same local manager, and
		// OnEditorOpened()'s IsLocalControllerOwner() check keeps them silent as it always did.
		SCR_EditorManagerEntity existing = SCR_EditorManagerEntity.GetInstance();
		if (existing)
		{
			OnEditorManagerInitOwner(existing);

			// The editor can also already be OPEN (component re-init mid-session) - the opened
			// invoker we just wired has already fired for this session, so start polling directly.
			if (existing.IsOpened())
				OnEditorOpened();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The local player's editor manager has been initialised - subscribe to its open/close events.
	//! \param[in] editorManager The manager the core delivered.
	protected void OnEditorManagerInitOwner(SCR_EditorManagerEntity editorManager)
	{
		if(!editorManager) return;

		m_EditorManager = editorManager;

		ScriptInvoker opened = editorManager.GetOnOpened();
		if(opened)
		{
			opened.Remove(OnEditorOpened);
			opened.Insert(OnEditorOpened);
		}

		ScriptInvoker closed = editorManager.GetOnClosed();
		if(closed)
		{
			closed.Remove(OnEditorClosed);
			closed.Insert(OnEditorClosed);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The editor opened: start polling, unless this is a limited editor.
	//!
	//! The IsLimited() check is POLITENESS, not security - a regular player's photo-mode editor
	//! generates no traffic at all - and a modified client can skip it, which is exactly why the real
	//! gate is IsAuthorizedGM() inside the handler.
	protected void OnEditorOpened()
	{
		if(!IsLocalControllerOwner()) return;

		if(m_EditorManager && m_EditorManager.IsLimited()) return;

		GetGame().GetCallqueue().Remove(RequestSnapshot);

		RequestSnapshot();

		GetGame().GetCallqueue().CallLater(RequestSnapshot, m_fPollIntervalMs, true);
	}

	//------------------------------------------------------------------------------------------------
	//! The editor closed: the poll stops, the store empties, consumers are told. From here the
	//! feature costs nothing at all until an editor opens again.
	protected void OnEditorClosed()
	{
		GetGame().GetCallqueue().Remove(RequestSnapshot);

		m_bStaging = false;
		m_iStagingSeq = -1;
		m_Staging.Clear();

		// ABOVE the early return, deliberately: a session that received a waypoint route but never a
		// campaign snapshot would otherwise keep that route forever. No separate "route cleared"
		// invoker - consumers of both stores already subscribe to GetOnStateCleared().
		m_bRouteStaging = false;
		m_iRouteStagingSeq = -1;
		m_RouteStaging.Clear();
		m_Route.Clear();

		if(!m_State.HasData()) return;

		m_State.Clear();

		if(m_OnStateCleared) m_OnStateCleared.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	//! Ask the server for a fresh campaign snapshot. Public so the poll timer and a future consumer
	//! (a manual refresh button) can both drive it.
	//!
	//! The Replication.IsServer() branch is the listen-server half: an RplRcver.Server RPC marshalled
	//! BY the server is delivered to nobody, so a host must call its own handler directly.
	void RequestSnapshot()
	{
		if(!IsLocalControllerOwner()) return;

		m_iSeq += 1;

		int requestType = OVT_EGMRequestType.CAMPAIGN_SNAPSHOT;

		if(Replication.IsServer())
		{
			RpcAsk_Snapshot(requestType, m_iSeq);
		}else{
			Rpc(RpcAsk_Snapshot, requestType, m_iSeq);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ask the server for one AI group's waypoint route. Public: the GM waypoint renderer calls it when
	//! the editor selection changes to a group, and nothing else does.
	//!
	//! The live route is CLEARED IMMEDIATELY rather than on arrival, so a consumer never draws the
	//! previous group's route against the new selection while the fan is in flight. The route sequence
	//! is this component's second, independent counter - see the class header for why it is not m_iSeq.
	//!
	//! Same listen-server branch as RequestSnapshot(): an RplRcver.Server RPC marshalled BY the server
	//! is delivered to nobody, so a host calls its own handler directly.
	//! \param[in] groupRplId The group entity's RplId, read from its RplComponent by the caller.
	void RequestGroupWaypoints(RplId groupRplId)
	{
		if(!IsLocalControllerOwner()) return;

		m_iRouteSeq += 1;

		m_bRouteStaging = false;
		m_iRouteStagingSeq = -1;
		m_RouteStaging.Clear();
		m_Route.Clear();

		int requestType = OVT_EGMRequestType.GROUP_WAYPOINTS;

		if(Replication.IsServer())
		{
			RpcAsk_GroupWaypoints(requestType, m_iRouteSeq, groupRplId);
		}else{
			Rpc(RpcAsk_GroupWaypoints, requestType, m_iRouteSeq, groupRplId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the controller entity this component sits on is the LOCAL player's own.
	//!
	//! A client holds a replicated instance of this component for every connected player's
	//! controller. Only the local player's may send a request: an RPC sent on somebody else's
	//! controller would be resolved by the server as coming from THAT player.
	//! \return True when this is the local player's controller.
	protected bool IsLocalControllerOwner()
	{
		OVT_OverthrowController localController = OVT_Global.GetController();
		if(!localController) return false;

		IEntity localEntity = localController;

		return localEntity == GetOwner();
	}

	//------------------------------------------------------------------------------------------------
	// SERVER
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: a GM client wants a snapshot.
	//!
	//! IDENTITY IS NEVER A PARAMETER - it comes from the entity the RPC arrived on, which is what
	//! makes it unspoofable. THE GATE IS THE NEXT LINE AFTER IT, before any campaign read.
	//! \param[in] requestType An OVT_EGMRequestType value.
	//! \param[in] seq The client's sequence id, echoed untouched into every response.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Snapshot(int requestType, int seq)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		if(!IsAuthorizedGM(playerId))
		{
			LogRefusal(playerId);
			return;
		}

		if(requestType != OVT_EGMRequestType.CAMPAIGN_SNAPSHOT) return;

		SendCampaignSnapshot(playerId, seq);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a GM client wants one group's waypoint route.
	//!
	//! IDENTITY IS NEVER A PARAMETER - the same rule as RpcAsk_Snapshot, and THE GATE IS AGAIN THE
	//! LINE AFTER IT, before the RplId is resolved to anything at all. An unauthorized caller gets no
	//! reply of any kind, not even an empty one: a reply would confirm the handler exists.
	//!
	//! Past the gate, every outcome answers. An RplId that resolves to nothing, or to an entity that
	//! is not an AIGroup, is answered with an EMPTY ROUTE rather than silence - see the class header.
	//! \param[in] requestType An OVT_EGMRequestType value.
	//! \param[in] seq The client's route sequence id, echoed untouched into every response.
	//! \param[in] groupRplId The group entity's RplId. RplId, never EntityID: an EntityID from a client
	//! names a different entity (or nothing) on the server.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_GroupWaypoints(int requestType, int seq, RplId groupRplId)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		if(!IsAuthorizedGM(playerId))
		{
			LogRefusal(playerId);
			return;
		}

		if(requestType != OVT_EGMRequestType.GROUP_WAYPOINTS) return;

		IEntity entity = ResolveEntity(groupRplId);

		AIGroup group = AIGroup.Cast(entity);

		SendGroupWaypoints(playerId, seq, groupRplId, group);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one throttled audit line for a refused request, and NOTHING sent back. An
	//! unauthorized poll is a bug or an attack, and neither deserves feedback that confirms the
	//! handler exists.
	//! \param[in] playerId The refused player.
	protected void LogRefusal(int playerId)
	{
		float now = 0;
		BaseWorld world = GetGame().GetWorld();
		if(world) now = world.GetWorldTime();

		if(m_fLastRefusalLog > 0 && (now - m_fLastRefusalLog) < (REFUSAL_LOG_INTERVAL * 1000)) return;

		m_fLastRefusalLog = now;

		Print(string.Format("[Overthrow.GMRequest] Player %1 requested a GM campaign snapshot without Game Master or admin rights - refused", playerId), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: read the campaign's numbers and fan them to one player.
	//!
	//! STRICTLY READ-ONLY. Every call in here is a getter or a pure static; nothing accumulates,
	//! allocates or spends. In particular the distribution amount is PredictResourceGain(), not
	//! GainResources().
	//! \param[in] playerId The GM to answer.
	//! \param[in] seq The client's sequence id.
	protected void SendCampaignSnapshot(int playerId, int seq)
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if(!occupying) return;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(!config) return;

		float threat = occupying.GetThreatFloat();
		int ofResources = occupying.m_iResources;

		int deploymentResources = 0;
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if(deployments) deploymentResources = deployments.GetFactionResources(config.GetOccupyingFactionIndex());

		int playerCount = 0;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if(playerManager) playerCount = playerManager.GetPlayerCount();

		int flags = 0;
		if(occupying.m_CurrentQRF) flags |= OVT_GMCampaignState.FLAG_DISTRIBUTION_SUPPRESSED_QRF;
		if(playerCount == 0) flags |= OVT_GMCampaignState.FLAG_PAYOUT_SUPPRESSED_NO_PLAYERS;

		int distributionAmount = OVT_GMSchedule.PredictResourceGain(
			config.m_Difficulty.baseResourcesPerTick,
			config.m_Difficulty.resourcesPerTick,
			threat,
			playerCount);

		int payoutAmount = 0;
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(economy) payoutAmount = economy.GetDonationIncome() + economy.GetTaxIncome();

		// Both loops fire on the in-game 6-hour marks, so both countdowns share one derivation. The
		// conversion uses GetDayDuration() - real seconds per in-game day under the acceleration
		// CURRENTLY in force - and never the day-only multiplier, which is simply wrong at night.
		float realSeconds = SecondsToNextMark();

		// The per-entity walk, run BEFORE the fan opens so SnapshotEnd can carry the true total. It
		// is strictly read-only - see OVT_GMSnapshotBuilder's header for the mutating methods it must
		// never call and why Sweep() is the one exception.
		if(!m_Builder) m_Builder = new OVT_GMSnapshotBuilder();

		int perEntityRecords = m_Builder.Build(m_iMaxRecordsPerSnapshot, m_bDebugSnapshotTiming);

		// The occupying faction's current objective. Read HERE rather than in the builder because it is a
		// campaign-wide scalar of exactly the same kind as threat and both resource pools above, not a
		// per-entity record - and because both calls are PURE GETTERS on the director, which is the
		// read-only rule the builder's header states and this fan inherits (T8.8).
		string objectiveName = "";
		int objectivePhase = OVT_EObjectivePhase.IDLE;

		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		if(director)
		{
			objectiveName = director.GetObjectiveDisplayName();
			objectivePhase = director.GetPhase();
		}

		SendSnapshotBegin(playerId, seq);
		SendCampaignResources(playerId, seq, threat, ofResources, deploymentResources, flags);
		SendCampaignSchedule(playerId, seq, distributionAmount, realSeconds, payoutAmount, realSeconds);
		SendCampaignObjective(playerId, seq, objectiveName, objectivePhase);
		SendRecordFan(playerId, seq);
		SendSnapshotEnd(playerId, seq, CAMPAIGN_RECORD_COUNT + perEntityRecords);

		if(m_bDebugSnapshotTiming)
		{
			Print(string.Format("[Overthrow.GMRequest] Built campaign snapshot seq %1 for player %2: threat %3, reserve %4, deployment pool %5, flags %6, next mark in %7s, %8 per-entity record(s)",
				seq, playerId, threat, ofResources, deploymentResources, flags, realSeconds, perEntityRecords));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the per-entity half of the fan - one RPC per base, non-empty base upgrade, active
	//! deployment and tagged AI group, in that order, all under the same seq as the campaign records
	//! that preceded them and the End that follows.
	//!
	//! ORDER IS PART OF THE CONTRACT for one reason only: base records arrive before nothing in
	//! particular, but every one of these must arrive AFTER the Begin that opened staging and BEFORE
	//! the End that commits it. The client drops anything else on the seq check, so a late record from
	//! a superseded poll is discarded rather than merged into the current snapshot.
	//! \param[in] playerId Recipient.
	//! \param[in] seq The client's sequence id.
	protected void SendRecordFan(int playerId, int seq)
	{
		foreach(OVT_GMBaseRecord baseRecord : m_Builder.m_aBases)
		{
			SendBase(playerId, seq, baseRecord.m_iBaseIndex, baseRecord.m_iResources, baseRecord.m_iGroups, baseRecord.m_iUpgrades);
		}

		foreach(OVT_GMBaseUpgradeRecord upgradeRecord : m_Builder.m_aBaseUpgrades)
		{
			SendBaseUpgrade(playerId, seq, upgradeRecord.m_iBaseIndex, upgradeRecord.m_sType, upgradeRecord.m_iResources, upgradeRecord.m_iGroups);
		}

		foreach(OVT_GMDeploymentRecord deploymentRecord : m_Builder.m_aDeployments)
		{
			SendDeployment(playerId, seq, deploymentRecord.m_RplId, deploymentRecord.m_sName, deploymentRecord.m_iFaction, deploymentRecord.m_iResourcesInvested, deploymentRecord.m_bActive);
		}

		foreach(OVT_GMGroupRecord groupRecord : m_Builder.m_aGroups)
		{
			SendGroup(playerId, seq, groupRecord.m_RplId, groupRecord.m_iOriginType, groupRecord.m_iOriginIndex, groupRecord.m_sReason);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: read one group's route and fan it to one player, Begin ... Waypoint x n ... End.
	//!
	//! STRICTLY READ-ONLY - OVT_GMWaypointWalk touches no mutating waypoint or group API, and this
	//! method adds only GetOrigin() and a prefab-name read on top of it.
	//!
	//! A NULL GROUP IS NOT A REASON TO STAY SILENT. Collect() answers 0 for it, and the client gets a
	//! complete, empty route: the consumer fetches once per selection, so silence here would leave the
	//! previously selected group's route drawn until the editor closed.
	//! \param[in] playerId The GM to answer.
	//! \param[in] seq The client's route sequence id.
	//! \param[in] groupRplId The RplId that was asked about, echoed back so the client can confirm the
	//! answer belongs to the selection it still has.
	//! \param[in] group The resolved group, or null when the RplId named nothing or a non-group.
	protected void SendGroupWaypoints(int playerId, int seq, RplId groupRplId, AIGroup group)
	{
		array<AIWaypoint> waypoints = new array<AIWaypoint>();
		int currentIndex = -1;
		bool cyclic = false;
		bool truncated = false;

		int count = OVT_GMWaypointWalk.Collect(group, m_iMaxWaypointsPerGroup, waypoints, currentIndex, cyclic, truncated);

		int flags = 0;
		if(cyclic) flags |= OVT_GMWaypointRoute.FLAG_CYCLIC;

		if(truncated)
		{
			flags |= OVT_GMWaypointRoute.FLAG_TRUNCATED;
			LogRouteTruncation(groupRplId, group, count);
		}

		vector groupPos = vector.Zero;
		if(group) groupPos = group.GetOrigin();

		SendWaypointsBegin(playerId, seq, groupRplId, count, currentIndex, flags);

		int sent = 0;

		foreach(int i, AIWaypoint waypoint : waypoints)
		{
			if(!waypoint) continue;

			// The kind is a pure function of the prefab resource name - there is no runtime API for it
			// (see OVT_GMWaypointFormat's header) - and an unrecognised name classifies UNKNOWN.
			int type = OVT_GMWaypointFormat.ClassifyPrefab(OVT_Global.GetPrefabName(waypoint));

			SendWaypoint(playerId, seq, i, waypoint.GetOrigin(), type);

			sent += 1;
		}

		SendWaypointsEnd(playerId, seq, sent);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one throttled warning that a group's route was cut off by m_iMaxWaypointsPerGroup.
	//!
	//! Real Overthrow routes are nine waypoints or fewer (a perimeter patrol is 4 patrol + 4 wait), so
	//! this should NEVER fire - which is exactly why it must be loud when it does. The true count is
	//! re-read with a deliberately generous probe cap so the line says how far over the group actually
	//! was; the probe runs only on this cold path.
	//! \param[in] groupRplId The group's RplId, so a triage session can name the same entity the client
	//! asked about.
	//! \param[in] group The group, re-walked for its true waypoint count.
	//! \param[in] sentCount How many waypoints the capped walk emitted.
	protected void LogRouteTruncation(RplId groupRplId, AIGroup group, int sentCount)
	{
		float now = 0;
		BaseWorld world = GetGame().GetWorld();
		if(world) now = world.GetWorldTime();

		if(m_fLastTruncationLog > 0 && (now - m_fLastTruncationLog) < (TRUNCATION_LOG_INTERVAL * 1000)) return;

		m_fLastTruncationLog = now;

		array<AIWaypoint> probe = new array<AIWaypoint>();
		int probeCurrent = -1;
		bool probeCyclic = false;
		bool probeTruncated = false;

		int trueCount = OVT_GMWaypointWalk.Collect(group, TRUNCATION_PROBE_CAP, probe, probeCurrent, probeCyclic, probeTruncated);

		Print(string.Format("[Overthrow.GMRequest] GM waypoint route for group RplId %1 was TRUNCATED: %2 waypoint(s) sent of %3 (cap %4). Real Overthrow routes are 9 or fewer - this group is pathological or the cap is wrong.",
			groupRplId, sentCount, trueCount, m_iMaxWaypointsPerGroup), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: real seconds from now until the next in-game 6-hour schedule mark.
	//!
	//! Re-sent every poll rather than sent once as an absolute timestamp, deliberately: a countdown
	//! that spans a day/night acceleration switch is off by the ratio of the two accelerations for
	//! the part that crosses, and re-sending makes that error shrink monotonically as the deadline
	//! nears instead of keeping it forever.
	//! \return Real seconds to the next mark, or 0 when the clock cannot be read.
	protected float SecondsToNextMark()
	{
		if(!m_Time) return 0;

		TimeContainer time = m_Time.GetTime();
		if(!time) return 0;

		float inGameSeconds = OVT_GMSchedule.InGameSecondsToNextMark(time.m_iHours, time.m_iMinutes, time.m_iSeconds);

		return OVT_GMSchedule.RealSecondsFor(inGameSeconds, m_Time.GetDayDuration());
	}

	//------------------------------------------------------------------------------------------------
	// SERVER -> OWNER SEND SITES
	//
	// Every one of them carries the ShouldRespondLocally short-circuit, and the Rpc() call stays here
	// at the call site rather than behind a helper on purpose: Rpc() is an untyped variadic
	// prototype, so a wrong argument count compiles clean and dies silently at the wire (BUG-090).
	// Each pair below must be arity-diffed against its handler by eye.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: open the fan.
	//! \param[in] playerId Recipient.
	//! \param[in] seq The client's sequence id.
	protected void SendSnapshotBegin(int playerId, int seq)
	{
		if(ShouldRespondLocally(playerId))
		{
			RpcDo_SnapshotBegin(seq, WIRE_VERSION);
			return;
		}

		Rpc(RpcDo_SnapshotBegin, seq, WIRE_VERSION);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the campaign's resource scalars.
	//! \param[in] playerId Recipient.
	//! \param[in] seq The client's sequence id.
	//! \param[in] threat Campaign threat at float precision.
	//! \param[in] ofResources The occupying faction's reserve.
	//! \param[in] ofDeploymentResources The occupying faction's deployment pool.
	//! \param[in] flags Suppression bitfield (OVT_GMCampaignState.FLAG_*).
	protected void SendCampaignResources(int playerId, int seq, float threat, int ofResources, int ofDeploymentResources, int flags)
	{
		if(ShouldRespondLocally(playerId))
		{
			RpcDo_CampaignResources(seq, threat, ofResources, ofDeploymentResources, flags);
			return;
		}

		Rpc(RpcDo_CampaignResources, seq, threat, ofResources, ofDeploymentResources, flags);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the two upcoming ticks, as amounts plus real-seconds countdowns.
	//! \param[in] playerId Recipient.
	//! \param[in] seq The client's sequence id.
	//! \param[in] distAmount Predicted occupying-faction distribution.
	//! \param[in] distSeconds Real seconds to it.
	//! \param[in] payoutAmount Predicted resistance payout.
	//! \param[in] payoutSeconds Real seconds to it.
	protected void SendCampaignSchedule(int playerId, int seq, int distAmount, float distSeconds, int payoutAmount, float payoutSeconds)
	{
		if(ShouldRespondLocally(playerId))
		{
			RpcDo_CampaignSchedule(seq, distAmount, distSeconds, payoutAmount, payoutSeconds);
			return;
		}

		Rpc(RpcDo_CampaignSchedule, seq, distAmount, distSeconds, payoutAmount, payoutSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the occupying faction's current objective and how far along the ramp it is.
	//!
	//! ⚠ A NEW PAIR, NOT A WIDENED ONE, AND DELIBERATELY. Adding two arguments to SendCampaignSchedule
	//! would have been fewer lines and one fewer record; it would also have been an untyped variadic
	//! Rpc() call whose argument count no longer matched its handler on any build that had only half the
	//! change (BUG-090 - a wrong argument count compiles clean and dies silently at the wire). A new pair
	//! cannot half-land: the record either exists on both ends or on neither, and WIRE_VERSION says which.
	//!
	//! ARITY, DIFFED BY EYE as the block header above instructs: Rpc(handler, seq, name, phase) is three
	//! payload arguments and RpcDo_CampaignObjective(int, string, int) takes three.
	//! \param[in] playerId Recipient.
	//! \param[in] seq The client's sequence id.
	//! \param[in] name The objective's display name, or "" when the occupying faction has no target.
	//! \param[in] phase OVT_EObjectivePhase as an integer. IDLE (0) when there is no objective.
	protected void SendCampaignObjective(int playerId, int seq, string name, int phase)
	{
		if(ShouldRespondLocally(playerId))
		{
			RpcDo_CampaignObjective(seq, name, phase);
			return;
		}

		Rpc(RpcDo_CampaignObjective, seq, name, phase);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one base's aggregate.
	//! \param[in] playerId Recipient.
	//! \param[in] seq The client's sequence id.
	//! \param[in] baseIndex Positional index into OVT_OccupyingFactionManager.m_Bases - THE join key.
	//! \param[in] resources Summed resources over the base's upgrades.
	//! \param[in] groups Summed group count over the base's upgrades.
	//! \param[in] upgrades How many upgrades the base runs, empty ones included.
	protected void SendBase(int playerId, int seq, int baseIndex, int resources, int groups, int upgrades)
	{
		if(ShouldRespondLocally(playerId))
		{
			RpcDo_Base(seq, baseIndex, resources, groups, upgrades);
			return;
		}

		Rpc(RpcDo_Base, seq, baseIndex, resources, groups, upgrades);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one non-empty base upgrade.
	//! \param[in] playerId Recipient.
	//! \param[in] seq The client's sequence id.
	//! \param[in] baseIndex Positional index of the owning base.
	//! \param[in] type The upgrade's concrete ClassName().
	//! \param[in] resources Its resources.
	//! \param[in] groups Its live + banked group count, or 0 for an upgrade that fields none.
	protected void SendBaseUpgrade(int playerId, int seq, int baseIndex, string type, int resources, int groups)
	{
		if(ShouldRespondLocally(playerId))
		{
			RpcDo_BaseUpgrade(seq, baseIndex, type, resources, groups);
			return;
		}

		Rpc(RpcDo_BaseUpgrade, seq, baseIndex, type, resources, groups);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one active deployment.
	//! \param[in] playerId Recipient.
	//! \param[in] seq The client's sequence id.
	//! \param[in] rplId The deployment entity's RplId - THE join key. Position is not sent; a consumer
	//! resolves the RplId and asks the entity.
	//! \param[in] name The deployment config's display name.
	//! \param[in] faction Controlling faction index.
	//! \param[in] invested Resources spent on it so far.
	//! \param[in] active Whether it is currently running.
	protected void SendDeployment(int playerId, int seq, RplId rplId, string name, int faction, int invested, bool active)
	{
		if(ShouldRespondLocally(playerId))
		{
			RpcDo_Deployment(seq, rplId, name, faction, invested, active);
			return;
		}

		Rpc(RpcDo_Deployment, seq, rplId, name, faction, invested, active);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one tagged AI group's provenance.
	//! \param[in] playerId Recipient.
	//! \param[in] seq The client's sequence id.
	//! \param[in] rplId The group entity's RplId - THE join key.
	//! \param[in] originType An OVT_EGroupOrigin value.
	//! \param[in] originIndex Base index / town id / tower id, or -1.
	//! \param[in] reason Concrete producer name.
	protected void SendGroup(int playerId, int seq, RplId rplId, int originType, int originIndex, string reason)
	{
		if(ShouldRespondLocally(playerId))
		{
			RpcDo_Group(seq, rplId, originType, originIndex, reason);
			return;
		}

		Rpc(RpcDo_Group, seq, rplId, originType, originIndex, reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: close the fan. The record count is what makes a truncated fan visible on the client.
	//! \param[in] playerId Recipient.
	//! \param[in] seq The client's sequence id.
	//! \param[in] recordCount How many records were sent between Begin and End.
	protected void SendSnapshotEnd(int playerId, int seq, int recordCount)
	{
		if(ShouldRespondLocally(playerId))
		{
			RpcDo_SnapshotEnd(seq, recordCount);
			return;
		}

		Rpc(RpcDo_SnapshotEnd, seq, recordCount);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: open a route fan. Sent for EVERY authorized request, including the ones that found no
	//! group and no waypoints - count 0 with currentIndex -1 is the answer "that group has no route".
	//! \param[in] playerId Recipient.
	//! \param[in] seq The client's route sequence id.
	//! \param[in] groupRplId The group's RplId, echoed back.
	//! \param[in] count How many waypoint records follow.
	//! \param[in] currentIndex Index of the current waypoint, or -1 (a first-class answer).
	//! \param[in] flags OVT_GMWaypointRoute.FLAG_* bitfield.
	protected void SendWaypointsBegin(int playerId, int seq, RplId groupRplId, int count, int currentIndex, int flags)
	{
		if(ShouldRespondLocally(playerId))
		{
			RpcDo_WaypointsBegin(seq, WIRE_VERSION, groupRplId, count, currentIndex, flags);
			return;
		}

		Rpc(RpcDo_WaypointsBegin, seq, WIRE_VERSION, groupRplId, count, currentIndex, flags);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one waypoint of the route.
	//!
	//! POSITION IS SENT because a waypoint entity has no RplComponent and therefore does not exist on
	//! the client at all - it is the one GM record whose position cannot be resolved from an RplId.
	//! \param[in] playerId Recipient.
	//! \param[in] seq The client's route sequence id.
	//! \param[in] index Position in the route, 0-based, in walking order.
	//! \param[in] pos The waypoint entity's origin.
	//! \param[in] type An OVT_EGMWaypointType value.
	protected void SendWaypoint(int playerId, int seq, int index, vector pos, int type)
	{
		if(ShouldRespondLocally(playerId))
		{
			RpcDo_Waypoint(seq, index, pos, type);
			return;
		}

		Rpc(RpcDo_Waypoint, seq, index, pos, type);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: close the route fan. The sent count is what makes a fan that lost records visible.
	//! \param[in] playerId Recipient.
	//! \param[in] seq The client's route sequence id.
	//! \param[in] sent How many waypoint records were sent between Begin and End.
	protected void SendWaypointsEnd(int playerId, int seq, int sent)
	{
		if(ShouldRespondLocally(playerId))
		{
			RpcDo_WaypointsEnd(seq, sent);
			return;
		}

		Rpc(RpcDo_WaypointsEnd, seq, sent);
	}

	//------------------------------------------------------------------------------------------------
	// CLIENT STAGING
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Client: a snapshot begins. A version this build does not know REFUSES TO STAGE and leaves the
	//! previously committed state untouched - a mismatched build must fail loudly and completely
	//! rather than half-overwrite a store with fields it has mis-parsed.
	//! \param[in] seq The sequence id this fan belongs to.
	//! \param[in] wireVersion The server's WIRE_VERSION.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SnapshotBegin(int seq, int wireVersion)
	{
		if(wireVersion != WIRE_VERSION)
		{
			m_bStaging = false;

			if(!m_bLoggedVersionMismatch)
			{
				m_bLoggedVersionMismatch = true;
				Print(string.Format("[Overthrow.GMRequest] GM snapshot wire version mismatch: server sent %1, this build speaks %2. No GM campaign state will be shown until client and server builds match.", wireVersion, WIRE_VERSION), LogLevel.WARNING);
			}

			return;
		}

		m_iStagingSeq = seq;
		m_bStaging = true;
		m_Staging.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the campaign's resource scalars for the staging snapshot.
	//! \param[in] seq Sequence id; anything but the staging sequence is dropped.
	//! \param[in] threat Campaign threat.
	//! \param[in] ofResources Occupying-faction reserve.
	//! \param[in] ofDeploymentResources Occupying-faction deployment pool.
	//! \param[in] flags Suppression bitfield.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_CampaignResources(int seq, float threat, int ofResources, int ofDeploymentResources, int flags)
	{
		if(!IsStagingRecord(seq)) return;

		m_Staging.m_fThreat = threat;
		m_Staging.m_iOFResources = ofResources;
		m_Staging.m_iOFDeploymentResources = ofDeploymentResources;
		m_Staging.m_iFlags = flags;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the two upcoming ticks for the staging snapshot.
	//! \param[in] seq Sequence id; anything but the staging sequence is dropped.
	//! \param[in] distAmount Predicted occupying-faction distribution.
	//! \param[in] distSeconds Real seconds to it.
	//! \param[in] payoutAmount Predicted resistance payout.
	//! \param[in] payoutSeconds Real seconds to it.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_CampaignSchedule(int seq, int distAmount, float distSeconds, int payoutAmount, float payoutSeconds)
	{
		if(!IsStagingRecord(seq)) return;

		m_Staging.m_iDistributionAmount = distAmount;
		m_Staging.m_fDistributionSeconds = distSeconds;
		m_Staging.m_iPayoutAmount = payoutAmount;
		m_Staging.m_fPayoutSeconds = payoutSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the occupying faction's current objective for the staging snapshot.
	//!
	//! ⚠ THE PHASE IS STORED AS THE INTEGER IT ARRIVED AS, not cast to the enum. The wire carries the
	//! ordinal, this build may be older or newer than the one that sent it, and the panel's formatter
	//! already answers "unknown" for a value it does not recognise - converting here would only move
	//! that decision somewhere with less information.
	//! \param[in] seq Sequence id; anything but the staging sequence is dropped.
	//! \param[in] name The objective's display name, or "" when there is no objective.
	//! \param[in] phase OVT_EObjectivePhase as an integer.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_CampaignObjective(int seq, string name, int phase)
	{
		if(!IsStagingRecord(seq)) return;

		m_Staging.m_sObjectiveName = name;
		m_Staging.m_iObjectivePhase = phase;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: one base's aggregate for the staging snapshot.
	//! \param[in] seq Sequence id; anything but the staging sequence is dropped.
	//! \param[in] baseIndex Positional index into the occupying faction's base list.
	//! \param[in] resources Summed resources over the base's upgrades.
	//! \param[in] groups Summed group count over the base's upgrades.
	//! \param[in] upgrades How many upgrades the base runs.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Base(int seq, int baseIndex, int resources, int groups, int upgrades)
	{
		if(!IsStagingRecord(seq)) return;

		OVT_GMBaseRecord record = new OVT_GMBaseRecord();
		record.m_iBaseIndex = baseIndex;
		record.m_iResources = resources;
		record.m_iGroups = groups;
		record.m_iUpgrades = upgrades;
		m_Staging.m_aBases.Insert(record);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: one non-empty base upgrade for the staging snapshot.
	//! \param[in] seq Sequence id; anything but the staging sequence is dropped.
	//! \param[in] baseIndex Positional index of the owning base.
	//! \param[in] type The upgrade's concrete ClassName().
	//! \param[in] resources Its resources.
	//! \param[in] groups Its group count.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_BaseUpgrade(int seq, int baseIndex, string type, int resources, int groups)
	{
		if(!IsStagingRecord(seq)) return;

		OVT_GMBaseUpgradeRecord record = new OVT_GMBaseUpgradeRecord();
		record.m_iBaseIndex = baseIndex;
		record.m_sType = type;
		record.m_iResources = resources;
		record.m_iGroups = groups;
		m_Staging.m_aBaseUpgrades.Insert(record);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: one active deployment for the staging snapshot.
	//! \param[in] seq Sequence id; anything but the staging sequence is dropped.
	//! \param[in] rplId The deployment entity's RplId.
	//! \param[in] name The deployment config's display name.
	//! \param[in] faction Controlling faction index.
	//! \param[in] invested Resources spent on it so far.
	//! \param[in] active Whether it is currently running.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Deployment(int seq, RplId rplId, string name, int faction, int invested, bool active)
	{
		if(!IsStagingRecord(seq)) return;

		OVT_GMDeploymentRecord record = new OVT_GMDeploymentRecord();
		record.m_RplId = rplId;
		record.m_sName = name;
		record.m_iFaction = faction;
		record.m_iResourcesInvested = invested;
		record.m_bActive = active;
		m_Staging.m_aDeployments.Insert(record);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: one tagged AI group's provenance for the staging snapshot.
	//! \param[in] seq Sequence id; anything but the staging sequence is dropped.
	//! \param[in] rplId The group entity's RplId.
	//! \param[in] originType An OVT_EGroupOrigin value.
	//! \param[in] originIndex Base index / town id / tower id, or -1.
	//! \param[in] reason Concrete producer name.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Group(int seq, RplId rplId, int originType, int originIndex, string reason)
	{
		if(!IsStagingRecord(seq)) return;

		OVT_GMGroupRecord record = new OVT_GMGroupRecord();
		record.m_RplId = rplId;
		record.m_iOriginType = originType;
		record.m_iOriginIndex = originIndex;
		record.m_sReason = reason;
		m_Staging.m_aGroups.Insert(record);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the fan is complete - commit staging into the live store in one step and tell
	//! consumers. The arrival stamp written here is what both countdown readers extrapolate from.
	//! \param[in] seq Sequence id; anything but the staging sequence is dropped.
	//! \param[in] recordCount How many records the server said it sent.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SnapshotEnd(int seq, int recordCount)
	{
		if(!IsStagingRecord(seq)) return;

		m_bStaging = false;

		// Stamped before the commit so the store carries what the SERVER said it sent: a consumer
		// (or a triage session) comparing it against the array counts sees a fan that lost records or
		// was truncated by the server's cap, rather than quietly believing a short snapshot.
		m_Staging.m_iReportedRecordCount = recordCount;

		m_State.CopyFrom(m_Staging);

		float now = 0;
		BaseWorld world = GetGame().GetWorld();
		if(world) now = world.GetWorldTime();

		m_State.m_fReceivedWorldTime = now;

		if(m_bDebugSnapshotTiming)
		{
			Print(string.Format("[Overthrow.GMRequest] Committed GM snapshot seq %1 (%2 record(s) claimed): threat %3, reserve %4, pool %5, next distribution %6 in %7s",
				seq, recordCount, m_State.m_fThreat, m_State.m_iOFResources, m_State.m_iOFDeploymentResources, m_State.m_iDistributionAmount, m_State.GetDistributionSecondsRemaining()));

			Print(string.Format("[Overthrow.GMRequest] Snapshot seq %1 staged %2 base(s), %3 upgrade(s), %4 deployment(s), %5 group(s)",
				seq, m_State.m_aBases.Count(), m_State.m_aBaseUpgrades.Count(), m_State.m_aDeployments.Count(), m_State.m_aGroups.Count()));
		}

		if(m_OnSnapshotUpdated) m_OnSnapshotUpdated.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: whether a record belongs to the snapshot currently being staged. THE STALE-DISCARD
	//! RULE - a record from a superseded request is dropped, never merged into the current one.
	//! \param[in] seq The record's sequence id.
	//! \return True when the record may be staged.
	protected bool IsStagingRecord(int seq)
	{
		return m_bStaging && seq == m_iStagingSeq;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a route fan begins. A version this build does not know REFUSES TO STAGE, exactly as the
	//! snapshot does, and shares m_bLoggedVersionMismatch so a mismatched build logs ONCE for the whole
	//! component rather than once per surface.
	//! \param[in] seq The route sequence id this fan belongs to.
	//! \param[in] wireVersion The server's WIRE_VERSION.
	//! \param[in] groupRplId The group this route describes.
	//! \param[in] count How many waypoint records follow.
	//! \param[in] currentIndex Index of the current waypoint, or -1 for none.
	//! \param[in] flags OVT_GMWaypointRoute.FLAG_* bitfield.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_WaypointsBegin(int seq, int wireVersion, RplId groupRplId, int count, int currentIndex, int flags)
	{
		if(wireVersion != WIRE_VERSION)
		{
			m_bRouteStaging = false;

			if(!m_bLoggedVersionMismatch)
			{
				m_bLoggedVersionMismatch = true;
				Print(string.Format("[Overthrow.GMRequest] GM snapshot wire version mismatch: server sent %1, this build speaks %2. No GM campaign state will be shown until client and server builds match.", wireVersion, WIRE_VERSION), LogLevel.WARNING);
			}

			return;
		}

		m_iRouteStagingSeq = seq;
		m_bRouteStaging = true;

		m_RouteStaging.Clear();
		m_RouteStaging.m_GroupRplId = groupRplId;
		m_RouteStaging.m_iCurrentIndex = currentIndex;
		m_RouteStaging.m_iFlags = flags;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: one waypoint of the staging route. A record whose seq is not the staging one is DROPPED
	//! silently - it belongs to a request this client has already superseded.
	//! \param[in] seq Route sequence id; anything but the staging sequence is dropped.
	//! \param[in] index Position in the route, 0-based.
	//! \param[in] pos The waypoint's world position.
	//! \param[in] type An OVT_EGMWaypointType value.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Waypoint(int seq, int index, vector pos, int type)
	{
		if(!IsStagingRouteRecord(seq)) return;

		OVT_GMWaypointRecord record = new OVT_GMWaypointRecord();
		record.m_iIndex = index;
		record.m_vPos = pos;
		record.m_iType = type;
		m_RouteStaging.m_aWaypoints.Insert(record);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the route fan is complete - commit staging into the live route in ONE step and tell
	//! consumers. An empty route commits and fires like any other: that is how a consumer learns the
	//! selected group has no waypoints and stops drawing the previous one.
	//! \param[in] seq Route sequence id; anything but the staging sequence is dropped.
	//! \param[in] sent How many waypoint records the server said it sent.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_WaypointsEnd(int seq, int sent)
	{
		if(!IsStagingRouteRecord(seq)) return;

		m_bRouteStaging = false;

		m_RouteStaging.m_bComplete = true;

		m_Route.CopyFrom(m_RouteStaging);

		if(m_OnRouteUpdated) m_OnRouteUpdated.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: whether a record belongs to the route fan currently being staged. The stale-discard rule
	//! again, on the ROUTE's own sequence - the snapshot poll's m_iSeq must never invalidate a route.
	//! \param[in] seq The record's route sequence id.
	//! \return True when the record may be staged.
	protected bool IsStagingRouteRecord(int seq)
	{
		return m_bRouteStaging && seq == m_iRouteStagingSeq;
	}
}
