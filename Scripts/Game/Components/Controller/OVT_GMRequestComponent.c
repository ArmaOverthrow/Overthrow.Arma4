[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative, GM-gated campaign state snapshots for one player")]
class OVT_GMRequestComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! What a GM client may ask this seam for. Versioned and extensible on purpose: gm-map adds
//! THREAT_GRID here later and gets its own record RPC under the same seq/version framing, without
//! changing a single existing signature.
//------------------------------------------------------------------------------------------------
enum OVT_EGMRequestType
{
	CAMPAIGN_SNAPSHOT
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
	static const int WIRE_VERSION = 1;

	//! Minimum real seconds between two refusal log lines from this player. The component is
	//! per-player, so one field rate-limits per player and a spamming client cannot flood the log.
	static const float REFUSAL_LOG_INTERVAL = 10;

	//! CLI switch that authorises everyone. Read SERVER-SIDE ONLY, so a client cannot set it.
	//! Follows the existing -ovtDevUid precedent (OVT_Global.c:4).
	static const string DEV_CLI_PARAM = "ovtGmDev";

	//! Campaign-wide records in every fan: CampaignResources and CampaignSchedule. They are outside
	//! m_iMaxRecordsPerSnapshot's budget on purpose - the cap exists to bound the PER-ENTITY fan, and
	//! a snapshot that dropped its campaign scalars to make room for a base record would be useless.
	static const int CAMPAIGN_RECORD_COUNT = 2;

	[Attribute(defvalue: "8000", desc: "How often a GM client re-requests the campaign snapshot while the editor is open, in milliseconds")]
	protected float m_fPollIntervalMs;

	[Attribute(defvalue: "400", desc: "Safety valve: most records one snapshot may carry (used by the per-entity fan)")]
	protected int m_iMaxRecordsPerSnapshot;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Print snapshot build info server-side and staging commits client-side")]
	protected bool m_bDebugSnapshotTiming;

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

	//! SERVER: world time (ms) of the last refusal log line for this player. See REFUSAL_LOG_INTERVAL.
	protected float m_fLastRefusalLog;

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

		SendSnapshotBegin(playerId, seq);
		SendCampaignResources(playerId, seq, threat, ofResources, deploymentResources, flags);
		SendCampaignSchedule(playerId, seq, distributionAmount, realSeconds, payoutAmount, realSeconds);
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
}
