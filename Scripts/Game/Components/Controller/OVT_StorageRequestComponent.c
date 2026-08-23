//------------------------------------------------------------------------------------------------
//! What a checkout asks the server to do with the cart it just streamed.
//!
//! ⚠ THE ORDINAL IS WHAT CROSSES THE WIRE. Append only - inserting a value above an existing name
//! re-points every older client's opKind at a different operation.
//------------------------------------------------------------------------------------------------
enum EOVT_StorageOp
{
	//! Ledger -> the source holder's own vanilla inventory. Requires dest == source.
	TO_INVENTORY,

	//! Ledger -> another holder's ledger. Zero spawns.
	TO_HOLDER,

	//! A holder's vanilla inventory -> its own ledger (the sweep).
	TO_STORAGE,

	//! Ledger -> money, at a port.
	EXPORT,

	//! Empty a holder's VANILLA inventory. Never touches the ledger.
	CLEAR,

	//! Battlefield bodies and every loose item around them -> the holder's ledger.
	LOOT,

	//! Several nearby containers -> one destination holder's ledger. Server-side only: no checkout
	//! ever names it, so it never crosses the wire as an opKind.
	COLLECT
}

//------------------------------------------------------------------------------------------------
//! One holder's contents as ONE client learned them from a pull-on-open fan.
//!
//! Client-side only. The server never holds one of these: it reads the ledger directly.
//------------------------------------------------------------------------------------------------
class OVT_StorageSnapshot : Managed
{
	//! Which holder this snapshot describes. RplId.Invalid() when there is nothing staged.
	RplId m_HolderId;

	//! One entry per ledger line, in the order the server sent them.
	ref array<ref OVT_StorageLine> m_aLines;

	//! How many lines the server's Begin said it would send. Compared against m_aLines at End, so a
	//! fan that lost records is visible rather than quietly short.
	int m_iReportedLineCount;

	//------------------------------------------------------------------------------------------------
	void OVT_StorageSnapshot()
	{
		m_aLines = new array<ref OVT_StorageLine>();
		m_HolderId = RplId.Invalid();
	}

	//------------------------------------------------------------------------------------------------
	//! Empties the snapshot and forgets which holder it belonged to.
	void Clear()
	{
		m_aLines.Clear();
		m_HolderId = RplId.Invalid();
		m_iReportedLineCount = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Appends one line.
	//! \param[in] res Prefab ResourceName.
	//! \param[in] qty How many are held.
	void AddLine(string res, int qty)
	{
		OVT_StorageLine line = new OVT_StorageLine();
		line.m_sRes = res;
		line.m_iCount = qty;
		m_aLines.Insert(line);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Total items across every line.
	int Total()
	{
		int total = 0;
		foreach (OVT_StorageLine line : m_aLines)
		{
			if (line)
				total += line.m_iCount;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces this snapshot's contents with a DEEP copy of another's, so the staging buffer and the
	//! committed snapshot never alias the same line objects.
	//! \param[in] other The snapshot to copy.
	void CopyFrom(OVT_StorageSnapshot other)
	{
		Clear();

		if (!other)
			return;

		m_HolderId = other.m_HolderId;
		m_iReportedLineCount = other.m_iReportedLineCount;

		foreach (OVT_StorageLine line : other.m_aLines)
		{
			if (line)
				AddLine(line.m_sRes, line.m_iCount);
		}
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative storage requests and the pull-on-open contents fan for one player")]
class OVT_StorageRequestComponentClass : OVT_BaseServerProgressComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! The one seam every storage mutation goes through, on the per-player OVT_OverthrowController.
//!
//! NO CLIENT EVER WRITES A LEDGER, INCLUDING ON A LISTEN HOST. Every verb below is a request; the
//! server resolves who asked from the entity the RPC arrived on, runs MayUseHolder, and only then
//! touches anything. Contents never replicate - the one player who opened a holder pulls them in a
//! Begin ... Line ... End fan addressed to that player alone.
//!
//! IT IS NOT AN OVT_ControllerRequestComponent. It needs OVT_BaseServerProgressComponent's progress
//! plumbing for the job engine and EnforceScript has no multiple inheritance, so - exactly as
//! OVT_ContainerTransferComponent does - identity comes from the STATIC
//! OVT_ControllerRequestComponent.ResolveOwningPlayerIdFor(GetOwner()) and owner replies use the
//! inherited IsLocalPlayerOwner() rather than ShouldRespondLocally().
//!
//! THE RULES THAT ARE NOT NEGOTIABLE:
//!  - No array<...> on any RPC. Both directions are fans; that is why the fans exist.
//!  - Every Rpc() call site is hand-audited against its handler and sits IMMEDIATELY BEFORE it.
//!    Rpc() is an untyped variadic prototype, so a wrong argument count compiles clean and dies
//!    silently at the wire (BUG-090). The audit table in
//!    docs/features/logistics/storage/context.md is the only check that exists.
//!  - Every RpcDo_* send takes the IsLocalPlayerOwner() direct branch FIRST. The engine never loops
//!    an Rpc back to the sender, so a listen host that skipped it would never receive its own reply.
//!  - Every rejection answers RpcDo_StorageError with a key the player sees. A log-only refusal is
//!    the shape this feature is replacing.
//!
//! TWO SEQUENCE SPACES, MADE DISJOINT BY PARITY. The contents pull and the checkout run on
//! independent counters, because a re-pull must never invalidate a checkout still in flight. They
//! are also disjoint on the wire - contents seqs are EVEN, checkout seqs are ODD - so
//! RpcDo_StorageError, the one reply both spaces share, can tell which request it is refusing
//! without a discriminator argument its plan-fixed arity has no room for.
//!
//! PHASE 4 SCOPE. Verbs 1, 7 and 8 are complete. The batch verbs 2-6 are declared and refuse with
//! #OVT-Storage_Busy until Phase 5 lands the job engine; RpcAsk_ClearVanillaInventory validates in
//! full and refuses at the same point, because the clear itself is a chunked CLEAR job.
//------------------------------------------------------------------------------------------------
class OVT_StorageRequestComponent : OVT_BaseServerProgressComponent
{
	//! Wire format version, echoed in every ContentsBegin. A client that does not recognise it
	//! REFUSES TO STAGE rather than mis-parsing a fan from a mismatched build.
	//!
	//! ⚠ BUMP IT WHENEVER THE SHAPE OF THE FAN CHANGES, not only when a field changes meaning.
	static const int WIRE_VERSION = 1;

	//! "This reply belongs to no fan" - the single-shot verbs (5, 6, 7, 8) carry no sequence number,
	//! so their refusals are always surfaced. It is even, but no live contents sequence is ever 0.
	static const int SEQ_NONE = 0;

	//! Longest custom name a holder may be given. The recruit-rename precedent
	//! (OVT_RecruitManagerComponent.RenameRecruit) is 1-32 and this matches it deliberately.
	static const int NAME_MAX_LENGTH = 32;

	//! How far the caller and the holder may be from a port and still export. The number port import
	//! already enforces on both ends (OVT_VehicleRequestComponent.IMPORT_MAX_PORT_DISTANCE), so a sale
	//! is never accepted where a purchase at the same spot would be refused.
	static const float EXPORT_MAX_PORT_DISTANCE = 30;

	//! Upper bound on a loot job's collection radius. The radius drives a world sphere query on the
	//! SERVER, so an unbounded one is a denial of service rather than a big loot run.
	static const float LOOT_MAX_RADIUS = 50;

	//! Stripping bodies in the open is a crime. The window is re-armed at every chunk and closed when
	//! the run ends, so it covers exactly as long as the looting takes plus this much slack.
	protected const int LOOT_ILLEGAL_SECONDS = 8;

	//! Notification tag for the escalation - #OVT-Msg-WantedLooting.
	protected const string LOOT_ILLEGAL_REASON = "WantedLooting";

	//! What a loot job collects from when the caller does not say.
	static const float LOOT_DEFAULT_RADIUS = 25;

	//! What a collection sweeps when the caller does not say. The shipped FOB undeploy footprint.
	static const float COLLECT_DEFAULT_RADIUS = 75;

	//! Upper bound on a collection radius, for the same reason LOOT_MAX_RADIUS exists: the number
	//! drives a world sphere query on the SERVER.
	static const float COLLECT_MAX_RADIUS = 100;

	//! ConvertItemToLedger: the item is deliberately left where it is.
	static const int CONVERT_SKIPPED = 0;

	//! ConvertItemToLedger: the item became a ledger line.
	static const int CONVERT_MOVED = 1;

	//! ConvertItemToLedger: the item should have converted and would not go.
	static const int CONVERT_FAILED = 2;

	//! ConvertItemToLedger: the ledger is full and nothing more will fit.
	static const int CONVERT_FULL = 3;

	//-----------------------------------------------------------------------------------------------
	// ATTRIBUTES (authored on Prefabs/GameMode/OVT_OverthrowController.et)
	//-----------------------------------------------------------------------------------------------

	[Attribute(defvalue: "25", desc: "Radius the destination picker collects nearby holders from, in metres")]
	protected float m_fHolderRadius;

	[Attribute(defvalue: "30", desc: "How far the caller may be from a holder and still act on it, in metres")]
	protected float m_fUseRadius;

	[Attribute(defvalue: "0.5", desc: "Fraction of the import price a port export pays out")]
	protected float m_fExportPriceRatio;

	[Attribute(defvalue: "64", desc: "Most lines one checkout may carry")]
	protected int m_iMaxCartLines;

	[Attribute(defvalue: "5", desc: "Items the job engine converts per chunk")]
	protected int m_iItemsPerChunk;

	[Attribute(defvalue: "50", desc: "Milliseconds between job engine chunks")]
	protected int m_iChunkDelayMs;

	//-----------------------------------------------------------------------------------------------
	// CLIENT STATE
	//-----------------------------------------------------------------------------------------------

	//! EVEN, and never 0 once a pull has been made. Bumped by two per contents request.
	protected int m_iContentsSeq;

	//! ODD. Bumped by two per checkout.
	protected int m_iBatchSeq = -1;

	protected bool m_bContentsStaging;

	protected int m_iContentsStagingSeq;

	protected bool m_bAwaitingBatch;

	protected bool m_bLoggedVersionMismatch;

	//! The fan currently arriving.
	protected ref OVT_StorageSnapshot m_Staging;

	//! The last fan that arrived complete. What the screen reads.
	protected ref OVT_StorageSnapshot m_Snapshot;

	//! () - a contents fan committed. Carries no payload: an RplId has never been passed through a
	//! ScriptInvoker anywhere in this project or in vanilla, and the consumer reads GetSnapshot().
	protected ref ScriptInvoker m_OnContentsUpdated;

	//! (string messageKey) - a request was refused.
	protected ref ScriptInvoker m_OnStorageError;

	//! (int moved, int shortfall, int earned) - a checkout finished.
	protected ref ScriptInvoker m_OnBatchResult;

	//-----------------------------------------------------------------------------------------------
	// SERVER STATE - THE JOB ENGINE
	//
	// ONE JOB PER PLAYER, AND NO QUEUE. m_Job is the latch; a second request is answered
	// #OVT-Storage_Busy and forgotten. Both fields are per-player because this component is per-
	// controller, which is the whole reason the engine does not live on the shared
	// OVT_InventoryManagerComponent singleton.
	//-----------------------------------------------------------------------------------------------

	//! The job currently running, or null. Never queued behind another.
	protected ref OVT_StorageJob m_Job;

	//! The checkout being streamed: created by BatchBegin, filled by BatchLine, validated and started
	//! by BatchCommit. Null between checkouts.
	protected ref OVT_StorageJob m_Checkout;

	//------------------------------------------------------------------------------------------------
	//! \param[in] owner The controller entity.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_Staging = new OVT_StorageSnapshot();
		m_Snapshot = new OVT_StorageSnapshot();
	}

	//------------------------------------------------------------------------------------------------
	//! Drops a running job's pending chunk so a controller destroyed mid-transfer never calls back
	//! into a deleted component.
	//! \param[in] owner The controller entity.
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(StepJob);

		m_Job = null;
		m_Checkout = null;

		super.OnDelete(owner);
	}

	//-----------------------------------------------------------------------------------------------
	// CLIENT -> SERVER
	//
	// Each Rpc() call site sits immediately before the handler it targets, so the pair can be
	// arity-diffed by eye. Rpc() is an untyped variadic prototype (BUG-090): nothing else checks it.
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Client: ask the server for a holder's contents. Latched by the caller, not here - the screen
	//! fires this once per (holder, sequence) from BuildEntries.
	//!
	//! ⚠ ON A LISTEN HOST OR IN SINGLE PLAYER THE WHOLE FAN ARRIVES INSIDE THIS CALL. The ask is
	//! invoked directly, so Begin, every line, End and the m_OnContentsUpdated invoke all run before
	//! this method returns. Two consequences, both binding on the caller: the sequence is bumped
	//! BEFORE the send (a fan naming a sequence the client has not adopted yet is dropped as stale by
	//! its own handler), and a caller that fires this from inside its own list build MUST set its
	//! latch before calling, or it will be re-entered from within itself.
	//! \param[in] holder The holder's RplId.
	//! \return The sequence this pull runs under, or SEQ_NONE when nothing was sent.
	int RequestOpenStorage(RplId holder)
	{
		if (!IsLocalControllerOwner())
			return SEQ_NONE;

		if (!holder.IsValid())
			return SEQ_NONE;

		m_iContentsSeq += 2;

		m_bContentsStaging = false;
		m_iContentsStagingSeq = SEQ_NONE;
		m_Staging.Clear();

		if (Replication.IsServer())
		{
			RpcAsk_OpenStorage(holder, m_iContentsSeq);
			return m_iContentsSeq;
		}

		Rpc(RpcAsk_OpenStorage, holder, m_iContentsSeq);

		return m_iContentsSeq;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: fan one holder's contents to the caller and to nobody else.
	//!
	//! An empty holder is still answered - Begin(0) + End - because the screen fires the pull once
	//! and would otherwise show a loading message forever.
	//! \param[in] holder The holder's RplId.
	//! \param[in] seq The client's contents sequence, echoed untouched into every reply.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_OpenStorage(RplId holder, int seq)
	{
		if (!Replication.IsServer())
			return;

		int playerId = ResolveCallerPlayerId();

		IEntity entity = OVT_StorageUtils.ResolveHolder(holder);

		string rejectKey;
		if (!MayUseHolder(playerId, entity, rejectKey))
		{
			SendStorageError(seq, rejectKey);
			return;
		}

		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(entity);
		OVT_StorageLedger ledger = storage.GetLedger();
		if (!ledger)
		{
			SendStorageError(seq, "#OVT-Storage_Failed");
			return;
		}

		array<string> res = new array<string>();
		array<int> counts = new array<int>();
		ledger.GetLines(res, counts);

		SendContentsBegin(holder, seq, res.Count(), WIRE_VERSION);

		for (int i = 0; i < res.Count(); i++)
		{
			SendContentsLine(seq, res[i], counts[i]);
		}

		SendContentsEnd(seq);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: open a checkout. Follow with one RequestBatchLine per cart line, then
	//! RequestBatchCommit - one order, not one request per line.
	//! \param[in] source The holder the items come out of.
	//! \param[in] dest Where they go. Equal to \a source for TO_INVENTORY.
	//! \param[in] opKind An EOVT_StorageOp value.
	//! \param[in] lineCount How many lines will follow.
	//! \return The sequence this checkout runs under, or SEQ_NONE when nothing was sent.
	int RequestBatchBegin(RplId source, RplId dest, int opKind, int lineCount)
	{
		if (!IsLocalControllerOwner())
			return SEQ_NONE;

		m_iBatchSeq += 2;
		m_bAwaitingBatch = true;

		if (Replication.IsServer())
		{
			RpcAsk_BatchBegin(source, dest, opKind, m_iBatchSeq, lineCount);
			return m_iBatchSeq;
		}

		Rpc(RpcAsk_BatchBegin, source, dest, opKind, m_iBatchSeq, lineCount);

		return m_iBatchSeq;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: open a checkout.
	//!
	//! The whole checkout is refused HERE, before the lines stream - see RpcAsk_BatchLine for why the
	//! lines and the commit that follow a refusal answer nothing. Everything that can be decided
	//! without the lines is decided here; the line-dependent half (clamping against live ledger
	//! membership, the count match) waits for Commit, which is the only other place that may refuse
	//! and refuses at most once.
	//! \param[in] source The holder the items come out of.
	//! \param[in] dest Where they go.
	//! \param[in] opKind An EOVT_StorageOp value.
	//! \param[in] seq The client's checkout sequence.
	//! \param[in] lineCount How many lines the client says will follow.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_BatchBegin(RplId source, RplId dest, int opKind, int seq, int lineCount)
	{
		if (!Replication.IsServer())
			return;

		// Any checkout that was opened and never committed is superseded, and a refusal below must
		// leave nothing behind for the trailing lines and commit to attach themselves to.
		m_Checkout = null;

		// A checkout sequence is ODD. Answering an even one would put a checkout's refusal into the
		// contents space and cancel whatever pull the client had in flight, so it is answered under
		// SEQ_NONE instead - visible to the player, attached to neither space.
		if (IsContentsSeq(seq))
		{
			SendStorageError(SEQ_NONE, "#OVT-Storage_BadRequest");
			return;
		}

		if (lineCount <= 0 || lineCount > m_iMaxCartLines)
		{
			SendStorageError(seq, "#OVT-Storage_BadRequest");
			return;
		}

		if (!EngineIsIdle())
		{
			SendStorageError(seq, "#OVT-Storage_Busy");
			return;
		}

		int playerId = ResolveCallerPlayerId();

		string rejectKey;
		if (!CheckoutHoldersUsable(playerId, source, dest, opKind, rejectKey))
		{
			SendStorageError(seq, rejectKey);
			return;
		}

		OVT_StorageJob job = new OVT_StorageJob();
		job.m_iPlayerId = playerId;
		job.m_iSeq = seq;
		job.m_eOp = opKind;
		job.m_SourceId = source;
		job.m_DestId = dest;

		m_Checkout = job;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: one cart line of an open checkout.
	//! \param[in] seq The sequence RequestBatchBegin returned.
	//! \param[in] index Position in the cart.
	//! \param[in] res Prefab ResourceName.
	//! \param[in] qty How many.
	void RequestBatchLine(int seq, int index, string res, int qty)
	{
		if (!IsLocalControllerOwner())
			return;

		if (Replication.IsServer())
		{
			RpcAsk_BatchLine(seq, index, res, qty);
			return;
		}

		Rpc(RpcAsk_BatchLine, seq, index, res, qty);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one cart line.
	//!
	//! ⚠ THE ONE PLACE IN THIS COMPONENT THAT ANSWERS NOTHING, AND DELIBERATELY. The client streams
	//! Begin, every line and Commit back to back before any reply can arrive, so answering here would
	//! send the player one refusal per line instead of one per order. A malformed line is REMEMBERED
	//! on the checkout and refused once, at Commit. ONE REFUSAL PER CHECKOUT.
	//! \param[in] seq The client's checkout sequence.
	//! \param[in] index Position in the cart. Reliable channels are ordered, so it must equal the
	//! number of lines already accepted; anything else means the stream is not the one Begin opened.
	//! \param[in] res Prefab ResourceName.
	//! \param[in] qty How many.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_BatchLine(int seq, int index, string res, int qty)
	{
		if (!Replication.IsServer())
			return;

		if (!m_Checkout || m_Checkout.m_iSeq != seq)
			return;

		if (index != m_Checkout.LineCount())
		{
			m_Checkout.m_bMalformed = true;
			return;
		}

		if (res == "" || qty <= 0 || m_Checkout.LineCount() >= m_iMaxCartLines)
		{
			m_Checkout.m_bMalformed = true;
			return;
		}

		m_Checkout.AddLine(res, qty);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: close a checkout and run it.
	//! \param[in] seq The sequence RequestBatchBegin returned.
	//! \param[in] lineCount How many lines were sent; repeated so the server can spot a short stream.
	void RequestBatchCommit(int seq, int lineCount)
	{
		if (!IsLocalControllerOwner())
			return;

		if (Replication.IsServer())
		{
			RpcAsk_BatchCommit(seq, lineCount);
			return;
		}

		Rpc(RpcAsk_BatchCommit, seq, lineCount);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: VALIDATE, then run a checkout.
	//!
	//! A commit that names no open checkout returns SILENTLY - that checkout was already refused at
	//! Begin, and a second refusal for the same order is exactly what RpcAsk_BatchLine's rule
	//! forbids. A commit that names an OPEN checkout may refuse once, because those refusals (a short
	//! stream, a holder that died mid-stream, a cart that clamped to nothing) could not have been
	//! known at Begin.
	//! \param[in] seq The client's checkout sequence.
	//! \param[in] lineCount How many lines the client says it sent.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_BatchCommit(int seq, int lineCount)
	{
		if (!Replication.IsServer())
			return;

		if (!m_Checkout || m_Checkout.m_iSeq != seq)
			return;

		OVT_StorageJob job = m_Checkout;

		// Taken before anything can refuse, so a duplicated commit finds nothing and stays silent.
		m_Checkout = null;

		string rejectKey;
		if (!ValidateCheckout(job, lineCount, rejectKey))
		{
			SendStorageError(seq, rejectKey);
			return;
		}

		BeginJob(job);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: convert a holder's whole vanilla inventory into its own ledger.
	//! \param[in] holder The holder's RplId.
	void RequestTransferAllToStorage(RplId holder)
	{
		if (!IsLocalControllerOwner())
			return;

		if (Replication.IsServer())
		{
			RpcAsk_TransferAllToStorage(holder);
			return;
		}

		Rpc(RpcAsk_TransferAllToStorage, holder);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sweep a holder's vanilla inventory into its own ledger.
	//! \param[in] holder The holder's RplId.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TransferAllToStorage(RplId holder)
	{
		if (!Replication.IsServer())
			return;

		if (!EngineIsIdle())
		{
			SendStorageError(SEQ_NONE, "#OVT-Storage_Busy");
			return;
		}

		int playerId = ResolveCallerPlayerId();

		IEntity entity = OVT_StorageUtils.ResolveHolder(holder);

		string rejectKey;
		if (!MayUseHolder(playerId, entity, rejectKey))
		{
			SendStorageError(SEQ_NONE, rejectKey);
			return;
		}

		OVT_StorageJob job = BuildSweepJob(playerId, holder);
		if (job.m_aPending.IsEmpty())
		{
			SendStorageError(SEQ_NONE, "#OVT-Storage_NothingToMove");
			return;
		}

		BeginJob(job);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: move a holder's whole ledger into another holder.
	//! \param[in] source The holder emptied.
	//! \param[in] dest The holder filled.
	//! \param[in] sweepFirst Whether the source's vanilla inventory is converted first.
	void RequestMoveAllToHolder(RplId source, RplId dest, bool sweepFirst)
	{
		if (!IsLocalControllerOwner())
			return;

		if (Replication.IsServer())
		{
			RpcAsk_MoveAllToHolder(source, dest, sweepFirst);
			return;
		}

		Rpc(RpcAsk_MoveAllToHolder, source, dest, sweepFirst);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: whole-ledger move, optionally sweeping the source's vanilla inventory into its ledger
	//! first.
	//!
	//! sweepFirst is the ONE chaining case in this engine: a TO_STORAGE job whose m_NextJob is the
	//! TO_HOLDER move. The move's lines are built when the move actually starts, not now, because the
	//! sweep is exactly what changes them.
	//! \param[in] source The holder emptied.
	//! \param[in] dest The holder filled.
	//! \param[in] sweepFirst Whether the source's vanilla inventory is converted first.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_MoveAllToHolder(RplId source, RplId dest, bool sweepFirst)
	{
		if (!Replication.IsServer())
			return;

		if (!EngineIsIdle())
		{
			SendStorageError(SEQ_NONE, "#OVT-Storage_Busy");
			return;
		}

		int playerId = ResolveCallerPlayerId();

		IEntity sourceEntity = OVT_StorageUtils.ResolveHolder(source);
		IEntity destEntity = OVT_StorageUtils.ResolveHolder(dest);

		string rejectKey;
		if (!MayUseHolder(playerId, sourceEntity, rejectKey))
		{
			SendStorageError(SEQ_NONE, rejectKey);
			return;
		}

		if (!MayUseHolder(playerId, destEntity, rejectKey))
		{
			SendStorageError(SEQ_NONE, rejectKey);
			return;
		}

		if (source == dest)
		{
			SendStorageError(SEQ_NONE, "#OVT-Storage_BadRequest");
			return;
		}

		OVT_StorageJob move = new OVT_StorageJob();
		move.m_iPlayerId = playerId;
		move.m_iSeq = SEQ_NONE;
		move.m_eOp = EOVT_StorageOp.TO_HOLDER;
		move.m_SourceId = source;
		move.m_DestId = dest;

		if (!sweepFirst)
		{
			if (!FillWholeLedgerLines(move))
			{
				SendStorageError(SEQ_NONE, "#OVT-Storage_NothingToMove");
				return;
			}

			BeginJob(move);
			return;
		}

		OVT_StorageJob sweep = BuildSweepJob(playerId, source);
		sweep.m_NextJob = move;

		if (sweep.m_aPending.IsEmpty())
		{
			// Nothing to convert, so the chain collapses to the move it was chained to.
			if (!FillWholeLedgerLines(move))
			{
				SendStorageError(SEQ_NONE, "#OVT-Storage_NothingToMove");
				return;
			}

			BeginJob(move);
			return;
		}

		BeginJob(sweep);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: empty a holder's VANILLA inventory. Officer-only, enforced on the server.
	//! \param[in] holder The holder's RplId.
	void RequestClearVanillaInventory(RplId holder)
	{
		if (!IsLocalControllerOwner())
			return;

		if (Replication.IsServer())
		{
			RpcAsk_ClearVanillaInventory(holder);
			return;
		}

		Rpc(RpcAsk_ClearVanillaInventory, holder);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: empty a holder's VANILLA inventory, never its ledger.
	//!
	//! Validated in full here; the deletion itself is a chunked CLEAR job.
	//! \param[in] holder The holder's RplId.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_ClearVanillaInventory(RplId holder)
	{
		if (!Replication.IsServer())
			return;

		if (!EngineIsIdle())
		{
			SendStorageError(SEQ_NONE, "#OVT-Storage_Busy");
			return;
		}

		int playerId = ResolveCallerPlayerId();

		IEntity entity = OVT_StorageUtils.ResolveHolder(holder);

		string rejectKey;
		if (!MayUseHolder(playerId, entity, rejectKey))
		{
			SendStorageError(SEQ_NONE, rejectKey);
			return;
		}

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance || !resistance.IsOfficer(playerId))
		{
			SendStorageError(SEQ_NONE, "#OVT-Storage_NotOfficer");
			return;
		}

		OVT_StorageJob job = new OVT_StorageJob();
		job.m_iPlayerId = playerId;
		job.m_iSeq = SEQ_NONE;
		job.m_eOp = EOVT_StorageOp.CLEAR;
		job.m_SourceId = holder;
		job.m_DestId = holder;
		job.m_sProgressKey = "#OVT-Progress-StorageClearing";

		// The clear is the one sweep-shaped op that does NOT strip weapons or spare part-used
		// magazines: discarding them is the whole point of the action.
		CollectInventoryItems(entity, job.m_aPending);
		job.m_iTotalUnits = job.m_aPending.Count();

		if (job.m_aPending.IsEmpty())
		{
			SendStorageError(SEQ_NONE, "#OVT-Storage_NothingToMove");
			return;
		}

		BeginJob(job);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: rename a holder.
	//! \param[in] holder The holder's RplId.
	//! \param[in] name The new name, 1-32 characters.
	void RequestRenameHolder(RplId holder, string name)
	{
		if (!IsLocalControllerOwner())
			return;

		if (Replication.IsServer())
		{
			RpcAsk_RenameHolder(holder, name);
			return;
		}

		Rpc(RpcAsk_RenameHolder, holder, name);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: rename a holder. Permission is "anyone who may open it" - the same MayUseHolder gate as
	//! everything else, never a client-side check.
	//!
	//! The length rule is enforced HERE because the name is replicated to every client and shown in
	//! action labels, pickers and on the map. An empty name is refused rather than treated as a reset:
	//! there is no client that asks for one.
	//! \param[in] holder The holder's RplId.
	//! \param[in] name The new name.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RenameHolder(RplId holder, string name)
	{
		if (!Replication.IsServer())
			return;

		int playerId = ResolveCallerPlayerId();

		IEntity entity = OVT_StorageUtils.ResolveHolder(holder);

		string rejectKey;
		if (!MayUseHolder(playerId, entity, rejectKey))
		{
			SendStorageError(SEQ_NONE, rejectKey);
			return;
		}

		if (name.IsEmpty() || name.Length() > NAME_MAX_LENGTH)
		{
			SendStorageError(SEQ_NONE, "#OVT-Storage_NameInvalid");
			return;
		}

		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(entity);
		storage.SetCustomName(name);
	}

	//-----------------------------------------------------------------------------------------------
	// SERVER -> OWNER
	//
	// Same rule as above: one Rpc() per handler, immediately before it, arity-diffed by eye. Every
	// send takes the IsLocalPlayerOwner() direct branch first - the engine never loops an Rpc back to
	// the sender, so without it a listen host receives none of its own replies (BUG-090).
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: open a contents fan.
	//! \param[in] holder The holder being described.
	//! \param[in] seq The client's contents sequence.
	//! \param[in] lineCount How many lines follow.
	//! \param[in] wireVersion This build's WIRE_VERSION.
	protected void SendContentsBegin(RplId holder, int seq, int lineCount, int wireVersion)
	{
		if (IsLocalPlayerOwner())
		{
			RpcDo_ContentsBegin(holder, seq, lineCount, wireVersion);
			return;
		}

		Rpc(RpcDo_ContentsBegin, holder, seq, lineCount, wireVersion);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a contents fan begins. A version this build does not know REFUSES TO STAGE and leaves
	//! the previous snapshot untouched.
	//! \param[in] holder The holder being described.
	//! \param[in] seq The sequence this fan belongs to.
	//! \param[in] lineCount How many lines the server says it will send.
	//! \param[in] wireVersion The server's WIRE_VERSION.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_ContentsBegin(RplId holder, int seq, int lineCount, int wireVersion)
	{
		if (wireVersion != WIRE_VERSION)
		{
			m_bContentsStaging = false;

			if (!m_bLoggedVersionMismatch)
			{
				m_bLoggedVersionMismatch = true;
				Print(string.Format("[Overthrow.StorageRequest] Storage wire version mismatch: server sent %1, this build speaks %2. No storage contents will be shown until client and server builds match.", wireVersion, WIRE_VERSION), LogLevel.WARNING);
			}

			return;
		}

		// A fan for a superseded pull must not clobber the one being staged now.
		if (seq != m_iContentsSeq)
			return;

		m_bContentsStaging = true;
		m_iContentsStagingSeq = seq;

		m_Staging.Clear();
		m_Staging.m_HolderId = holder;
		m_Staging.m_iReportedLineCount = lineCount;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one line of a contents fan.
	//! \param[in] seq The client's contents sequence.
	//! \param[in] res Prefab ResourceName.
	//! \param[in] qty How many are held.
	protected void SendContentsLine(int seq, string res, int qty)
	{
		if (IsLocalPlayerOwner())
		{
			RpcDo_ContentsLine(seq, res, qty);
			return;
		}

		Rpc(RpcDo_ContentsLine, seq, res, qty);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: one line of the fan being staged.
	//! \param[in] seq Sequence id; anything but the staging sequence is dropped.
	//! \param[in] res Prefab ResourceName.
	//! \param[in] qty How many are held.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_ContentsLine(int seq, string res, int qty)
	{
		if (!IsStagingContents(seq))
			return;

		m_Staging.AddLine(res, qty);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: close a contents fan.
	//! \param[in] seq The client's contents sequence.
	protected void SendContentsEnd(int seq)
	{
		if (IsLocalPlayerOwner())
		{
			RpcDo_ContentsEnd(seq);
			return;
		}

		Rpc(RpcDo_ContentsEnd, seq);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: commit the staged fan and tell the screen.
	//! \param[in] seq Sequence id; anything but the staging sequence is dropped.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_ContentsEnd(int seq)
	{
		if (!IsStagingContents(seq))
			return;

		m_bContentsStaging = false;

		if (m_Staging.m_aLines.Count() != m_Staging.m_iReportedLineCount)
			Print(string.Format("[Overthrow.StorageRequest] Storage fan seq %1 arrived with %2 line(s), the server said %3. The list below is short.", seq, m_Staging.m_aLines.Count(), m_Staging.m_iReportedLineCount), LogLevel.WARNING);

		m_Snapshot.CopyFrom(m_Staging);

		if (m_OnContentsUpdated)
			m_OnContentsUpdated.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: refuse a request, with a key the player sees.
	//! \param[in] seq The sequence of the refused request, or SEQ_NONE for a single-shot verb.
	//! \param[in] messageKey Localization key describing the refusal.
	protected void SendStorageError(int seq, string messageKey)
	{
		if (IsLocalPlayerOwner())
		{
			RpcDo_StorageError(seq, messageKey);
			return;
		}

		Rpc(RpcDo_StorageError, seq, messageKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a request was refused.
	//!
	//! The only reply the two sequence spaces share, which is what their disjoint parity is for: an
	//! even seq refuses a contents pull, an odd one refuses a checkout, and either is dropped when it
	//! names a request this client has already superseded.
	//!
	//! SEQ_NONE BELONGS TO NO SCREEN. The single-shot verbs are fired from user actions, and both
	//! m_OnStorageError subscribers drop anything arriving while their screen is closed, so those
	//! refusals go to the hint instead - the way every other user action in the mod reports one.
	//! \param[in] seq The sequence of the refused request, or SEQ_NONE.
	//! \param[in] messageKey Localization key describing the refusal.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_StorageError(int seq, string messageKey)
	{
		if (seq == SEQ_NONE)
		{
			ShowRefusalHint(messageKey);
			return;
		}

		if (IsContentsSeq(seq))
		{
			if (seq != m_iContentsSeq)
				return;

			m_bContentsStaging = false;
			m_iContentsStagingSeq = SEQ_NONE;
			m_Staging.Clear();
		}
		else
		{
			if (seq != m_iBatchSeq)
				return;

			m_bAwaitingBatch = false;
		}

		if (m_OnStorageError)
			m_OnStorageError.Invoke(messageKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: draw a refusal that belongs to no screen.
	//! \param[in] messageKey Localization key describing the refusal.
	protected void ShowRefusalHint(string messageKey)
	{
		if (messageKey.IsEmpty())
			return;

		SCR_HintManagerComponent hints = SCR_HintManagerComponent.GetInstance();
		if (hints)
			hints.ShowCustom(messageKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: report a finished checkout. PHASE 5 CALLS THIS.
	//! \param[in] seq The client's checkout sequence.
	//! \param[in] moved How many items were moved.
	//! \param[in] shortfall How many were asked for and did not fit or were not there.
	//! \param[in] earned Money paid out, for EXPORT; 0 otherwise.
	protected void SendBatchResult(int seq, int moved, int shortfall, int earned)
	{
		if (IsLocalPlayerOwner())
		{
			RpcDo_BatchResult(seq, moved, shortfall, earned);
			return;
		}

		Rpc(RpcDo_BatchResult, seq, moved, shortfall, earned);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a checkout finished.
	//! \param[in] seq Sequence id; anything but the current checkout is dropped.
	//! \param[in] moved How many items were moved.
	//! \param[in] shortfall How many were not.
	//! \param[in] earned Money paid out.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_BatchResult(int seq, int moved, int shortfall, int earned)
	{
		if (seq != m_iBatchSeq)
			return;

		m_bAwaitingBatch = false;

		if (m_OnBatchResult)
			m_OnBatchResult.Invoke(moved, shortfall, earned);
	}

	//-----------------------------------------------------------------------------------------------
	// THE JOB ENGINE
	//
	// VALIDATE -> RUN -> (STEP)* -> FINISH | ABORT. One job per player, never queued: a second
	// request is answered #OVT-Storage_Busy and forgotten.
	//
	// THE THREE ORDERING RULES ARE THE POINT OF THIS SECTION, and each lives in exactly one place:
	//  - the SWEEP checks capacity BEFORE it deletes, and credits after (StepSweep) - the other order
	//    lets a full ledger eat an item;
	//  - TO_INVENTORY debits ONLY AFTER a spawn succeeds (StepToInventory) - a refused spawn costs
	//    nothing;
	//  - a TO_HOLDER move returns the un-added remainder TO THE SOURCE
	//    (OVT_StorageRules.TransferLedgerLine);
	//  - LOOT prices a whole tree BEFORE it destroys any of it (StepLoot) - the ground has no
	//    inventory manager to delete one item through, so the delete is one call at the root.
	// Each is a pair of statements with no chunk boundary between them, so the worst a mid-transfer
	// crash can cost is one item and never a stack.
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether a new job may start. THE ONE-JOB-PER-PLAYER LATCH.
	//! \return True when nothing is running on this player's controller.
	protected bool EngineIsIdle()
	{
		return !m_Job && !m_bIsRunning;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while this player has a storage job in flight.
	bool IsBusy()
	{
		return !EngineIsIdle();
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side entry point for the battlefield loot job. NOT an RPC: Phase 8 calls it from the
	//! loot user action's own validated handler, which has already resolved and gated the caller.
	//! \param[in] playerId The looting player.
	//! \param[in] holder The vehicle whose LEDGER the loot goes into.
	//! \param[in] radius How far around the holder to collect from.
	//! \return True when a job was started.
	bool StartLootJob(int playerId, RplId holder, float radius)
	{
		if (!Replication.IsServer())
			return false;

		if (!EngineIsIdle())
		{
			SendStorageError(SEQ_NONE, "#OVT-Storage_Busy");
			return false;
		}

		IEntity entity = OVT_StorageUtils.ResolveHolder(holder);

		string rejectKey;
		if (!MayUseHolder(playerId, entity, rejectKey))
		{
			SendStorageError(SEQ_NONE, rejectKey);
			return false;
		}

		// The loot lands in the LEDGER, so a holder with no ledger cannot receive it. MayUseHolder has
		// already refused a capacity-0 holder.
		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(entity);
		if (!storage || !storage.GetLedger())
		{
			SendStorageError(SEQ_NONE, "#OVT-Storage_BadRequest");
			return false;
		}

		// The radius drives a world sphere query on the SERVER, so it is bounded here whatever the
		// caller asked for - the same reason OVT_ContainerTransferComponent caps its own.
		if (radius <= 0)
			radius = LOOT_DEFAULT_RADIUS;

		if (radius > LOOT_MAX_RADIUS)
			radius = LOOT_MAX_RADIUS;

		OVT_StorageJob job = new OVT_StorageJob();
		job.m_iPlayerId = playerId;
		job.m_iSeq = SEQ_NONE;
		job.m_eOp = EOVT_StorageOp.LOOT;
		job.m_SourceId = holder;
		job.m_DestId = holder;
		job.m_fRadius = radius;

		array<IEntity> lootables = new array<IEntity>();
		OVT_StorageLootQuery query = new OVT_StorageLootQuery();
		query.Run(entity.GetOrigin(), radius, lootables);

		foreach (IEntity lootable : lootables)
		{
			if (lootable && lootable != entity)
				job.m_aPending.Insert(lootable.GetID());
		}

		job.m_iTotalUnits = job.m_aPending.Count();

		if (job.m_aPending.IsEmpty())
		{
			SendStorageError(SEQ_NONE, "#OVT-Storage_NothingToMove");
			return false;
		}

		BeginJob(job);

		ArmLootIllegalWindow(playerId);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side entry point for the FOB undeploy collection. NOT an RPC: its one caller,
	//! OVT_ResistanceFactionManager.UndeployFOB, is server-side and sits behind the FOB seam's own
	//! gate (OVT_FOBRequestComponent.RpcAsk_UndeployFOB: 15 m + PlayerMayUseVehicle on the FOB).
	//!
	//! THE CONTAINERS ARE NOT RE-GATED ONE BY ONE. They are spread over the whole FOB footprint and
	//! MayUseHolder's 30 m rule would refuse the far half of them, which is not what undeploy has ever
	//! meant. The destination is gated instead, because that is the holder the caller is standing at.
	//! \param[in] playerId The undeploying player.
	//! \param[in] anchor The deployed FOB: the centre of the search, and a source in its own right.
	//! \param[in] dest The mobile FOB the stock ends up in.
	//! \param[in] radius How far around \a anchor containers are collected from.
	//! \param[in] progressKey The caption for the progress HUD.
	//! EVERY REFUSAL RETURNS false AND EMITS NOTHING. The caller latches FOB state before calling and
	//! clears it from this component's m_OnOperationError, so reporting a refusal through that channel
	//! would fire a completion callback for a job that never started - and, when the refusal is "the
	//! engine is busy", would also clear the RUNNING job's m_bIsRunning out from under it. The caller
	//! unwinds its own state on false instead.
	//! \return True when a job was started.
	bool StartCollectionJob(int playerId, RplId anchor, RplId dest, float radius, string progressKey)
	{
		if (!Replication.IsServer())
			return false;

		if (!EngineIsIdle())
		{
			RejectCollection(playerId, "a storage job is already running on this player's controller");
			return false;
		}

		if (playerId <= 0)
		{
			RejectCollection(playerId, "the requesting player could not be resolved");
			return false;
		}

		IEntity anchorEntity = OVT_StorageUtils.ResolveHolder(anchor);
		IEntity destEntity = OVT_StorageUtils.ResolveHolder(dest);

		if (!anchorEntity || !destEntity)
		{
			RejectCollection(playerId, "the collection anchor or its destination no longer exists");
			return false;
		}

		if (!OVT_StorageUtils.GetStorage(destEntity))
		{
			RejectCollection(playerId, "the destination carries no OVT_StorageComponent");
			return false;
		}

		if (!OVT_ControllerRequestComponent.PlayerMayUseVehicleFor(playerId, destEntity))
		{
			RejectCollection(playerId, "the destination is locked to another player");
			return false;
		}

		if (radius <= 0)
			radius = COLLECT_DEFAULT_RADIUS;

		if (radius > COLLECT_MAX_RADIUS)
			radius = COLLECT_MAX_RADIUS;

		OVT_StorageJob job = new OVT_StorageJob();
		job.m_iPlayerId = playerId;
		job.m_iSeq = SEQ_NONE;
		job.m_eOp = EOVT_StorageOp.COLLECT;
		job.m_SourceId = dest;
		job.m_DestId = dest;
		job.m_fRadius = radius;
		job.m_sProgressKey = progressKey;

		// The anchor is drained first and queued explicitly: it is not a placed container, so the
		// query below never offers it, and it is the one holder the caller deletes outright.
		if (anchorEntity != destEntity)
			job.m_aHolders.Insert(anchorEntity.GetID());

		array<IEntity> containers = new array<IEntity>();
		OVT_StorageContainerQuery query = new OVT_StorageContainerQuery();
		query.Run(anchorEntity.GetOrigin(), radius, containers);

		foreach (IEntity container : containers)
		{
			if (container && container != destEntity && container != anchorEntity)
				job.m_aHolders.Insert(container.GetID());
		}

		job.m_iTotalUnits = job.m_aHolders.Count();

		// An empty footprint still starts a job. The caller's completion handler is what deletes the
		// FOB and reactivates the truck's physics, and it only ever runs from FINISH.
		BeginJob(job);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Logs a refused collection. Deliberately log-only - see StartCollectionJob.
	//! \param[in] playerId The caller, or -1.
	//! \param[in] reason Why the collection was refused.
	protected void RejectCollection(int playerId, string reason)
	{
		Print(string.Format("[OVT_StorageRequestComponent] Refused a collection job for player %1: %2", playerId.ToString(), reason), LogLevel.WARNING);
	}

	//-----------------------------------------------------------------------------------------------
	// VALIDATE
	//-----------------------------------------------------------------------------------------------

	//! Everything a checkout can be refused for that does not depend on its lines. Runs at Begin so
	//! the player hears about it before the cart streams, and again at Commit because a holder can
	//! die, lock or drive off in between.
	//! \param[in] playerId The caller.
	//! \param[in] source The holder items come out of.
	//! \param[in] dest Where they go.
	//! \param[in] opKind An EOVT_StorageOp value.
	//! \param[out] rejectKey Localization key describing the refusal.
	//! \return True when the checkout may proceed.
	protected bool CheckoutHoldersUsable(int playerId, RplId source, RplId dest, int opKind, out string rejectKey)
	{
		rejectKey = "";

		IEntity sourceEntity = OVT_StorageUtils.ResolveHolder(source);
		if (!MayUseHolder(playerId, sourceEntity, rejectKey))
			return false;

		if (dest != source && !MayUseHolder(playerId, OVT_StorageUtils.ResolveHolder(dest), rejectKey))
			return false;

		if (opKind == EOVT_StorageOp.TO_INVENTORY)
		{
			if (dest != source || !OVT_StorageUtils.GetInventoryManager(sourceEntity))
			{
				rejectKey = "#OVT-Storage_BadRequest";
				return false;
			}

			return true;
		}

		if (opKind == EOVT_StorageOp.TO_HOLDER)
		{
			if (dest == source)
			{
				rejectKey = "#OVT-Storage_BadRequest";
				return false;
			}

			return true;
		}

		if (opKind == EOVT_StorageOp.EXPORT)
		{
			if (dest != source)
			{
				rejectKey = "#OVT-Storage_BadRequest";
				return false;
			}

			// The port gate is enforced HERE and not only in the screen that offers Export: opKind
			// arrives from a client, so without it a modified client would have a money faucet.
			if (!AtAPort(playerId, sourceEntity))
			{
				rejectKey = "#OVT-Storage_NotAtPort";
				return false;
			}

			return true;
		}

		// A checkout carries cart lines. The entity-list ops (TO_STORAGE, CLEAR, LOOT) have no lines
		// and reach the engine only through their own verbs.
		rejectKey = "#OVT-Storage_BadRequest";
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! VALIDATE for a streamed checkout: the stream is whole, the holders are still usable, and every
	//! line is clamped to what the source ledger actually holds.
	//! \param[in] job The checkout.
	//! \param[in] lineCount How many lines the client says it sent.
	//! \param[out] rejectKey Localization key describing the refusal.
	//! \return True when the job may run.
	protected bool ValidateCheckout(OVT_StorageJob job, int lineCount, out string rejectKey)
	{
		rejectKey = "";

		if (job.m_bMalformed || job.LineCount() != lineCount)
		{
			rejectKey = "#OVT-Storage_BadRequest";
			return false;
		}

		if (!EngineIsIdle())
		{
			rejectKey = "#OVT-Storage_Busy";
			return false;
		}

		if (!CheckoutHoldersUsable(job.m_iPlayerId, job.m_SourceId, job.m_DestId, job.m_eOp, rejectKey))
			return false;

		OVT_StorageComponent source = ResolveStorage(job.m_SourceId);
		if (!source)
		{
			rejectKey = "#OVT-Storage_NotFound";
			return false;
		}

		OVT_StorageLedger ledger = source.GetLedger();
		if (!ledger)
		{
			rejectKey = "#OVT-Storage_Failed";
			return false;
		}

		// The denominator is what the PLAYER asked for, taken before the clamp, so a cart that was
		// half stale still shows an honest bar.
		job.m_iTotalUnits = job.RemainingUnits();

		ClampLinesToLedger(job, ledger);

		if (job.LineCount() == 0)
		{
			rejectKey = "#OVT-Storage_NothingToMove";
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Clamps every cart line to what the source ledger holds RIGHT NOW, dropping the ones that clamp
	//! to nothing and counting the difference as shortfall.
	//!
	//! D12: LEDGER MEMBERSHIP IS THE TAKE-OUT GATE, and it is strictly stronger than
	//! IsRegisteredResource - a line can only exist because the server deleted a real entity, so the
	//! registry check that traps converted loot on the shipped warehouse path is not repeated here.
	//! Only port import, which mints stock from a client-chosen name, keeps it.
	//!
	//! Claimed-so-far is tracked per resource because one cart may name the same resource twice;
	//! without it two lines of 50 would both pass against a single stock of 50. The kept lines are
	//! rebuilt in order rather than removed in place - cart order is what the shortfall report reads
	//! back in.
	//! \param[in] job The checkout.
	//! \param[in] ledger The source holder's ledger.
	protected void ClampLinesToLedger(OVT_StorageJob job, OVT_StorageLedger ledger)
	{
		map<string, int> claimed = new map<string, int>();
		array<string> keptRes = new array<string>();
		array<int> keptQty = new array<int>();

		for (int i = 0; i < job.LineCount(); i++)
		{
			string res = job.m_aRes[i];
			int qty = job.m_aQty[i];

			int already = 0;
			if (claimed.Contains(res))
				already = claimed.Get(res);

			int available = ledger.Count(res) - already;
			if (available < 0)
				available = 0;

			if (qty > available)
			{
				job.m_iShortfall += qty - available;
				qty = available;
			}

			if (qty <= 0)
				continue;

			keptRes.Insert(res);
			keptQty.Insert(qty);
			claimed.Set(res, already + qty);
		}

		job.m_aRes.Clear();
		job.m_aQty.Clear();

		for (int i = 0; i < keptRes.Count(); i++)
		{
			job.AddLine(keptRes[i], keptQty[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Fills a job's lines with the source holder's ENTIRE ledger.
	//!
	//! Called when the move actually starts rather than when it is requested, because the sweep a
	//! chained move follows is exactly what changes the answer.
	//! \param[in] job The move.
	//! \return True when there was anything to move.
	protected bool FillWholeLedgerLines(OVT_StorageJob job)
	{
		OVT_StorageComponent source = ResolveStorage(job.m_SourceId);
		if (!source)
			return false;

		OVT_StorageLedger ledger = source.GetLedger();
		if (!ledger)
			return false;

		array<string> res = new array<string>();
		array<int> counts = new array<int>();
		ledger.GetLines(res, counts);

		job.m_aRes.Clear();
		job.m_aQty.Clear();

		for (int i = 0; i < res.Count(); i++)
		{
			job.AddLine(res[i], counts[i]);
		}

		job.m_iTotalUnits = job.RemainingUnits();

		return job.LineCount() > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds the sweep job for one holder: its vanilla inventory converted into its own ledger.
	//! \param[in] playerId The caller.
	//! \param[in] holder The holder.
	//! \return The job, with an empty work list when there is nothing to convert.
	protected OVT_StorageJob BuildSweepJob(int playerId, RplId holder)
	{
		OVT_StorageJob job = new OVT_StorageJob();
		job.m_iPlayerId = playerId;
		job.m_iSeq = SEQ_NONE;
		job.m_eOp = EOVT_StorageOp.TO_STORAGE;
		job.m_SourceId = holder;
		job.m_DestId = holder;

		CollectSweepItems(OVT_StorageUtils.ResolveHolder(holder), job.m_aPending);
		job.m_iTotalUnits = job.m_aPending.Count();

		return job;
	}

	//-----------------------------------------------------------------------------------------------
	// RUN / STEP / FINISH / ABORT
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! RUN: latch the job, light the progress HUD, and either finish it on the spot or schedule the
	//! first chunk.
	//! \param[in] job A job that has already passed VALIDATE.
	protected void BeginJob(OVT_StorageJob job)
	{
		if (!job)
			return;

		m_Job = job;

		if (job.m_sProgressKey == "")
			job.m_sProgressKey = ProgressKeyFor(job.m_eOp);

		StartOperation(job.m_sProgressKey);

		// The two ledger-only ops are arithmetic over two maps bounded by m_iMaxCartLines. Chunking
		// them would buy nothing and make the player watch a bar for one frame.
		if (job.m_eOp == EOVT_StorageOp.TO_HOLDER)
		{
			RunLedgerMove(job);
			FinishJob();
			return;
		}

		if (job.m_eOp == EOVT_StorageOp.EXPORT)
		{
			RunExport(job);
			FinishJob();
			return;
		}

		ScheduleStep();
	}

	//------------------------------------------------------------------------------------------------
	//! Arms the next chunk.
	//!
	//! Deliberately NOT a repeating CallLater: a repeat has to be Remove()d from inside its own
	//! callback, and every chunked path in the mod re-arms a one-shot instead.
	protected void ScheduleStep()
	{
		if (!GetGame() || !GetGame().GetCallqueue())
			return;

		int delay = m_iChunkDelayMs;
		if (delay < 0)
			delay = 0;

		GetGame().GetCallqueue().CallLater(StepJob, delay, false);
	}

	//------------------------------------------------------------------------------------------------
	//! STEP: liveness, then one chunk of work, then either finish or re-arm.
	protected void StepJob()
	{
		if (!Replication.IsServer())
			return;

		OVT_StorageJob job = m_Job;
		if (!job)
			return;

		// LIVENESS FIRST, at the chunk boundary. Every ledger mutation below is a whole unit with no
		// yield inside it, so an abort here always lands between two units and leaves both ledgers
		// consistent.
		string abortKey;
		if (!JobStillValid(job, abortKey))
		{
			AbortJob(abortKey);
			return;
		}

		bool done = true;

		if (job.m_eOp == EOVT_StorageOp.TO_INVENTORY)
			done = StepToInventory(job);
		else if (job.m_eOp == EOVT_StorageOp.TO_STORAGE)
			done = StepSweep(job);
		else if (job.m_eOp == EOVT_StorageOp.CLEAR)
			done = StepClear(job);
		else if (job.m_eOp == EOVT_StorageOp.LOOT)
		{
			done = StepLoot(job);
			ArmLootIllegalWindow(job.m_iPlayerId);
		}
		else if (job.m_eOp == EOVT_StorageOp.COLLECT)
			done = StepCollect(job);

		SendProgressUpdate(job.Progress(), job.Processed(), job.m_iTotalUnits, job.m_sProgressKey);

		if (done)
		{
			FinishJob();
			return;
		}

		ScheduleStep();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a running job may take another chunk: the holders are still there and the player who
	//! asked for it is still connected.
	//! \param[in] job The running job.
	//! \param[out] reasonKey Localization key describing the abort.
	//! \return True when the job may continue.
	protected bool JobStillValid(OVT_StorageJob job, out string reasonKey)
	{
		reasonKey = "";

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players || !players.IsPlayerConnected(job.m_iPlayerId))
		{
			reasonKey = "#OVT-Storage_Failed";
			return false;
		}

		if (!ResolveStorage(job.m_SourceId))
		{
			reasonKey = "#OVT-Storage_NotFound";
			return false;
		}

		if (job.m_DestId != job.m_SourceId && !ResolveStorage(job.m_DestId))
		{
			reasonKey = "#OVT-Storage_NotFound";
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! FINISH: republish, report, and start the chained job if there is one.
	protected void FinishJob()
	{
		OVT_StorageJob job = m_Job;
		if (!job)
			return;

		PublishTouchedHolders(job);

		m_Job = null;

		if (job.m_eOp == EOVT_StorageOp.LOOT)
			ClearLootIllegalWindow(job.m_iPlayerId);

		SendOperationComplete(job.m_iMoved, job.m_iShortfall);

		if (job.m_iSeq != SEQ_NONE)
			SendBatchResult(job.m_iSeq, job.m_iMoved, job.m_iShortfall, job.m_iEarned);

		OVT_StorageJob next = job.m_NextJob;
		if (!next)
			return;

		// A listener on m_OnOperationComplete runs before this line and could have started something
		// of its own. The chain never clobbers a live job.
		if (!EngineIsIdle())
			return;

		// Seconds passed inside the job this one was chained to, so the chained job is re-gated rather
		// than trusting the permission and distance check its request made.
		string rejectKey;
		if (!CheckoutHoldersUsable(next.m_iPlayerId, next.m_SourceId, next.m_DestId, next.m_eOp, rejectKey))
		{
			SendStorageError(next.m_iSeq, rejectKey);
			return;
		}

		if (next.m_eOp == EOVT_StorageOp.TO_HOLDER && !FillWholeLedgerLines(next))
		{
			SendStorageError(next.m_iSeq, "#OVT-Storage_NothingToMove");
			return;
		}

		BeginJob(next);
	}

	//------------------------------------------------------------------------------------------------
	//! ABORT: stop between two units, republish what did change, and owe the rest as shortfall.
	//!
	//! A chained job is dropped: the reason the first one stopped (a dead holder, a gone player) is
	//! the reason the second must not run either.
	//! \param[in] reasonKey Localization key describing the abort.
	protected void AbortJob(string reasonKey)
	{
		OVT_StorageJob job = m_Job;
		if (!job)
			return;

		job.m_iShortfall += job.RemainingUnits() + job.RemainingPending();

		PublishTouchedHolders(job);

		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(StepJob);

		m_Job = null;

		if (job.m_eOp == EOVT_StorageOp.LOOT)
			ClearLootIllegalWindow(job.m_iPlayerId);

		SendOperationError(reasonKey);

		if (job.m_iSeq != SEQ_NONE)
			SendBatchResult(job.m_iSeq, job.m_iMoved, job.m_iShortfall, job.m_iEarned);
	}

	//------------------------------------------------------------------------------------------------
	//! R5: THE ONLY PLACE A JOB REPUBLISHES, AND IT REPUBLISHES EACH HOLDER ONCE.
	//!
	//! PublishCount() is one Replication.BumpMe(), so a per-item call would replace the network spike
	//! this feature exists to remove with an identical one. Reached from FINISH and from ABORT, never
	//! from both, so a holder is bumped at most once per job. CLEAR writes no ledger at all and
	//! therefore publishes nothing.
	//! \param[in] job The finished or aborted job.
	protected void PublishTouchedHolders(OVT_StorageJob job)
	{
		if (!JobWritesSourceLedger(job.m_eOp))
			return;

		OVT_StorageComponent source = ResolveStorage(job.m_SourceId);
		if (source)
			source.PublishCount();

		if (job.m_eOp != EOVT_StorageOp.TO_HOLDER)
			return;

		if (job.m_DestId == job.m_SourceId)
			return;

		OVT_StorageComponent dest = ResolveStorage(job.m_DestId);
		if (dest)
			dest.PublishCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether an op changes the source holder's ledger at all.
	//! \param[in] op The operation.
	//! \return False only for CLEAR, the one op that touches nothing but a vanilla inventory.
	protected bool JobWritesSourceLedger(EOVT_StorageOp op)
	{
		if (op == EOVT_StorageOp.CLEAR)
			return false;

		return true;
	}

	//-----------------------------------------------------------------------------------------------
	// THE SIX OPS
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! TO_HOLDER: ledger -> ledger, zero spawns, one step.
	//!
	//! The clamp against live membership, the take, the capped add and the RETURN OF THE UN-ADDED
	//! REMAINDER TO THE SOURCE are one pure function, so that ordering is the one this feature can
	//! assert in the Logic tier rather than only by reading.
	//! \param[in] job The move.
	protected void RunLedgerMove(OVT_StorageJob job)
	{
		OVT_StorageComponent source = ResolveStorage(job.m_SourceId);
		OVT_StorageComponent dest = ResolveStorage(job.m_DestId);
		if (!source || !dest)
			return;

		OVT_StorageLedger from = source.GetLedger();
		OVT_StorageLedger to = dest.GetLedger();
		if (!from || !to)
			return;

		int capacity = dest.GetCapacity();

		while (job.LineCount() > 0)
		{
			string res = job.m_aRes[0];
			int qty = job.m_aQty[0];

			int shortfall;
			job.m_iMoved += OVT_StorageRules.TransferLedgerLine(from, to, res, qty, capacity, shortfall);
			job.m_iShortfall += shortfall;

			job.DropFrontLine();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! TO_INVENTORY: ledger -> the holder's own vanilla inventory, chunked.
	//!
	//! ⚠ SPAWN THEN DEBIT. The ledger is only debited once a real entity exists, so a refused spawn
	//! costs the player nothing. The first refusal on a line ends THAT LINE, not the job: items are
	//! different sizes, and a rifle that will not fit says nothing about the next magazine.
	//! \param[in] job The take.
	//! \return True when every line is done.
	protected bool StepToInventory(OVT_StorageJob job)
	{
		OVT_StorageComponent storage = ResolveStorage(job.m_SourceId);
		if (!storage)
			return true;

		OVT_StorageLedger ledger = storage.GetLedger();
		InventoryStorageManagerComponent inventory = OVT_StorageUtils.GetInventoryManager(storage.GetOwner());
		if (!ledger || !inventory)
			return true;

		int budget = ChunkBudget();

		while (budget > 0 && job.LineCount() > 0)
		{
			budget--;

			string res = job.m_aRes[0];
			int wanted = job.m_aQty[0];

			// R9: re-clamped against LIVE membership at every step, not only at VALIDATE, so a second
			// player batching on the same holder can never drive this line negative.
			int held = ledger.Count(res);
			if (wanted > held)
			{
				job.m_iShortfall += wanted - held;
				wanted = held;
				job.m_aQty.Set(0, wanted);
			}

			if (wanted <= 0)
			{
				job.DropFrontLine();
				continue;
			}

			bool holderHasOwnStorage;
			BaseInventoryStorageComponent target = ResolveHolderStorage(inventory, null, res, holderHasOwnStorage);

			// A holder with its own storage but no room in it is FULL. Falling back to a null storage
			// here would let the engine nest the withdrawal inside a bag that was itself just taken out.
			if (holderHasOwnStorage && !target)
			{
				job.m_iShortfall += wanted;
				job.DropFrontLine();
				continue;
			}

			if (!inventory.TrySpawnPrefabToStorage(res, target, -1, EStoragePurpose.PURPOSE_ANY, null, 1))
			{
				job.m_iShortfall += wanted;
				job.DropFrontLine();
				continue;
			}

			ledger.Take(res, 1);
			job.m_iMoved++;

			wanted--;
			job.m_aQty.Set(0, wanted);

			if (wanted <= 0)
				job.DropFrontLine();
		}

		return job.LineCount() == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! TO_STORAGE: the holder's vanilla inventory -> its own ledger, chunked. THE SWEEP.
	//!
	//! ⚠ CAPACITY IS CHECKED BEFORE THE DELETE, AND THE CREDIT FOLLOWS THE DELETE. Checking after
	//! would let a full ledger eat an item; crediting first would mint one if the delete failed.
	//! \param[in] job The sweep.
	//! \return True when the work list is exhausted, or when the holder filled up.
	protected bool StepSweep(OVT_StorageJob job)
	{
		OVT_StorageComponent storage = ResolveStorage(job.m_SourceId);
		if (!storage)
			return true;

		OVT_StorageLedger ledger = storage.GetLedger();
		InventoryStorageManagerComponent inventory = OVT_StorageUtils.GetInventoryManager(storage.GetOwner());
		if (!ledger || !inventory)
			return true;

		int capacity = storage.GetCapacity();
		int budget = ChunkBudget();

		while (budget > 0 && job.m_iCursor < job.m_aPending.Count())
		{
			budget--;

			EntityID id = job.m_aPending[job.m_iCursor];
			job.m_iCursor++;

			int outcome = ConvertItemToLedger(GetGame().GetWorld().FindEntityByID(id), inventory, ledger, capacity);

			if (outcome == CONVERT_MOVED)
			{
				job.m_iMoved++;
				continue;
			}

			if (outcome == CONVERT_FAILED)
			{
				job.m_iShortfall++;
				continue;
			}

			if (outcome == CONVERT_FULL)
			{
				job.m_iShortfall += job.RemainingPending() + 1;
				job.m_iCursor = job.m_aPending.Count();
				return true;
			}
		}

		return job.m_iCursor >= job.m_aPending.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Converts ONE vanilla-inventory item into ONE ledger line.
	//!
	//! CAPACITY IS CHECKED BEFORE THE DELETE, AND THE CREDIT FOLLOWS THE DELETE. Checking after would
	//! let a full ledger eat an item; crediting first would mint one if the delete failed. The sweep
	//! and the FOB collection both run through here so the two can never drift on that ordering.
	//! \param[in] item The candidate; a vanished one is a silent skip.
	//! \param[in] inventory The manager that currently owns the item.
	//! \param[in] ledger The ledger being credited.
	//! \param[in] capacity The credited holder's capacity.
	//! \return CONVERT_MOVED, CONVERT_SKIPPED, CONVERT_FAILED or CONVERT_FULL.
	protected int ConvertItemToLedger(IEntity item, InventoryStorageManagerComponent inventory, OVT_StorageLedger ledger, int capacity)
	{
		if (!item || !ledger)
			return CONVERT_SKIPPED;

		ResourceName prefab = OVT_PrefabUtils.GetPrefabName(item);
		if (prefab == "")
			return CONVERT_SKIPPED;

		// A part its holder's prefab declares goes with the holder and is recreated by respawning it.
		// Crediting one would mint an item on every withdrawal.
		if (OVT_PrefabPartUtils.IsDeclaredPart(item))
			return CONVERT_SKIPPED;

		// A part-used magazine stays in the vanilla inventory: a ledger line is a COUNT and has
		// nowhere to record "27 of 30". The officer clear action is how they are discarded. It is
		// ejected from any container it sits in first, or ItemStillHoldsSomething strands the whole
		// container over one half-magazine.
		if (!MagazineConverts(item))
		{
			EjectToHolderStorage(inventory, item);
			return CONVERT_SKIPPED;
		}

		// TryDeleteItem cascades into a container's contents (proven at runtime during BUG-083). The
		// contents were queued AHEAD of the container, so this only fires when one of them failed -
		// and then the container is left alone rather than destroyed with them inside.
		if (ItemStillHoldsSomething(item))
			return CONVERT_SKIPPED;

		if (ledger.FreeSpace(capacity) <= 0)
			return CONVERT_FULL;

		if (!DeleteItem(inventory, item))
			return CONVERT_FAILED;

		// The FreeSpace gate above owns the cap. This credit must never be allowed to refuse, because
		// by the time it runs the entity is already gone.
		ledger.Add(prefab, 1, OVT_StorageComponent.UNLIMITED_CAPACITY);

		return CONVERT_MOVED;
	}

	//------------------------------------------------------------------------------------------------
	//! CLEAR: empty the holder's VANILLA inventory, chunked. The ledger is never touched.
	//! \param[in] job The clear.
	//! \return True when the work list is exhausted.
	protected bool StepClear(OVT_StorageJob job)
	{
		IEntity holder = OVT_StorageUtils.ResolveHolder(job.m_SourceId);
		InventoryStorageManagerComponent inventory = OVT_StorageUtils.GetInventoryManager(holder);
		if (!inventory)
			return true;

		int budget = ChunkBudget();

		while (budget > 0 && job.m_iCursor < job.m_aPending.Count())
		{
			budget--;

			EntityID id = job.m_aPending[job.m_iCursor];
			job.m_iCursor++;

			IEntity item = GetGame().GetWorld().FindEntityByID(id);
			if (!item)
				continue;

			if (DeleteItem(inventory, item))
				job.m_iMoved++;
			else
				job.m_iShortfall++;
		}

		return job.m_iCursor >= job.m_aPending.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! LOOT: dead bodies and everything lying around them -> the holder's LEDGER, chunked.
	//!
	//! ONE PENDING ENTITY IS ONE ALL-OR-NOTHING TREE. The whole tree is priced against free space
	//! BEFORE anything is destroyed, then the root is deleted once and every line is credited. The
	//! sweep's per-item delete-then-credit cannot be used here: half of what a loot run collects is
	//! lying on the ground, and the ground has no inventory manager to delete an item through.
	//! \param[in] job The loot run.
	//! \return True when the work list is exhausted, or when the ledger filled up.
	protected bool StepLoot(OVT_StorageJob job)
	{
		OVT_StorageComponent storage = ResolveStorage(job.m_SourceId);
		if (!storage)
			return true;

		OVT_StorageLedger ledger = storage.GetLedger();
		if (!ledger)
			return true;

		// A holder spawned this frame has not resolved its capacity yet - the resolve is a call-queue
		// hop - and reading the unresolved 0 would report a full ledger on an empty truck.
		int capacity = OVT_StorageComponent.UNLIMITED_CAPACITY;
		if (storage.IsCapacityResolved())
			capacity = storage.GetCapacity();

		int budget = ChunkBudget();

		while (budget > 0 && job.m_iCursor < job.m_aPending.Count())
		{
			budget--;

			EntityID id = job.m_aPending[job.m_iCursor];
			job.m_iCursor++;

			IEntity root = GetGame().GetWorld().FindEntityByID(id);
			if (!root)
				continue;

			array<string> lines = new array<string>();
			array<EntityID> seen = new array<EntityID>();
			int discarded = CollectLootTree(root, true, lines, seen);

			// Nothing to credit and nothing to throw away: a body wearing only its three base garments
			// is the common case, and it is left where it fell rather than deleted for no gain.
			if (lines.IsEmpty() && discarded <= 0)
				continue;

			if (ledger.FreeSpace(capacity) < lines.Count())
			{
				job.m_iShortfall += job.RemainingPending() + 1;
				job.m_iCursor = job.m_aPending.Count();
				return true;
			}

			// ONE delete for the whole tree, and the credit follows it. Crediting first would mint
			// lines if the delete failed.
			SCR_EntityHelper.DeleteEntityAndChildren(root);

			foreach (string res : lines)
			{
				// The FreeSpace gate above owns the cap. This credit must never be allowed to refuse:
				// by the time it runs the entities are already gone.
				ledger.Add(res, 1, OVT_StorageComponent.UNLIMITED_CAPACITY);
			}

			job.m_iMoved += lines.Count();
			job.m_iShortfall += discarded;
		}

		return job.m_iCursor >= job.m_aPending.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Prices one loot tree: one prefab name per item in it that becomes a ledger line.
	//!
	//! NOTHING IS MOVED, DETACHED OR DELETED HERE. The caller deletes the root once and credits the
	//! list, so a tree that will not fit costs nothing. Two things do not become lines:
	//!  - a BASE GARMENT worn on a body, which is left on it (its pockets are still emptied, and a
	//!    garment lying on the ground is ordinary loot because it is then the root);
	//!  - a PART-USED MAGAZINE, which is DISCARDED - a ledger line is a count and has nowhere to
	//!    record "27 of 30", so it goes with the tree and is reported as shortfall.
	//! Everything else is taken, including magazines loaded in weapons and mounted attachments.
	//! \param[in] item The tree root, or a node of it during the walk.
	//! \param[in] isRoot True only for the entity the query found.
	//! \param[out] lines Receives one prefab name per creditable item.
	//! \param[in,out] seen Every id already priced. GetItems is native and may or may not recurse, so
	//!        a node visited twice must cost nothing rather than credit twice.
	//! \return How many items were discarded rather than credited.
	protected int CollectLootTree(IEntity item, bool isRoot, out array<string> lines, out array<EntityID> seen)
	{
		if (!item || !lines || !seen)
			return 0;

		EntityID id = item.GetID();
		if (seen.Find(id) != -1)
			return 0;

		seen.Insert(id);

		int discarded = 0;

		// A body's gear lives in loadout storages, which are NOT universal storages - only its own
		// manager can enumerate them. The body itself is never a ledger line.
		ChimeraCharacter character = ChimeraCharacter.Cast(item);
		if (character)
		{
			InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(item.FindComponent(InventoryStorageManagerComponent));
			if (!manager)
				return 0;

			array<IEntity> carried = new array<IEntity>();
			manager.GetItems(carried);

			foreach (IEntity gear : carried)
			{
				discarded += CollectLootTree(gear, false, lines, seen);
			}

			return discarded;
		}

		array<Managed> storages = new array<Managed>();
		item.FindComponents(BaseUniversalInventoryStorageComponent, storages);

		foreach (Managed found : storages)
		{
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(found);
			if (!storage)
				continue;

			array<IEntity> contents = new array<IEntity>();
			storage.GetAll(contents);

			foreach (IEntity contained : contents)
			{
				discarded += CollectLootTree(contained, false, lines, seen);
			}
		}

		// A slot-declared part is a CHILD ENTITY, not storage content, so the loop above cannot see it.
		// It is never priced itself - the guard below drops it - but a harness pouch holds magazines
		// that would otherwise be destroyed with the vest.
		array<IEntity> attached = new array<IEntity>();
		OVT_PrefabPartUtils.CollectAttachedParts(item, attached);

		foreach (IEntity part : attached)
		{
			discarded += CollectLootTree(part, false, lines, seen);
		}

		// A WeaponAttachmentsStorageComponent is not a universal storage, so a loaded magazine and a
		// mounted optic are invisible to the loop above and would be destroyed with the weapon.
		BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
		if (weapon)
		{
			BaseMagazineComponent loaded = weapon.GetCurrentMagazine();
			if (loaded)
				discarded += CollectLootTree(loaded.GetOwner(), false, lines, seen);

			array<AttachmentSlotComponent> slots = new array<AttachmentSlotComponent>();
			weapon.GetAttachments(slots);

			foreach (AttachmentSlotComponent slot : slots)
			{
				if (slot)
					discarded += CollectLootTree(slot.GetAttachedEntity(), false, lines, seen);
			}
		}

		// R10: TYPENAMES, never ClassName() strings. The pre-storage filter compared eight strings and
		// two of them were not vanilla classes at all, so those branches never matched.
		if (!isRoot)
		{
			BaseLoadoutClothComponent cloth = BaseLoadoutClothComponent.Cast(item.FindComponent(BaseLoadoutClothComponent));
			if (cloth && cloth.GetAreaType() && OVT_StorageRules.IsBaseClothingArea(cloth.GetAreaType().Type()))
				return discarded;
		}

		// Priced as part of its holder's prefab, which is what recreates it.
		if (!isRoot && OVT_PrefabPartUtils.IsDeclaredPart(item))
			return discarded;

		if (!MagazineConverts(item))
			return discarded + 1;

		ResourceName prefab = OVT_PrefabUtils.GetPrefabName(item);
		if (prefab == "")
			return discarded;

		lines.Insert(prefab);

		return discarded;
	}

	//------------------------------------------------------------------------------------------------
	//! COLLECT: several nearby containers -> one holder's ledger. Chunked by ITEM, reported by
	//! CONTAINER.
	//!
	//! Each container is drained exactly the way the sweep drains one - delete then credit, part-used
	//! magazines left behind - and then its OWN ledger is moved across, so both halves of F10 land in
	//! the destination. The container is left empty for the caller to delete.
	//! \param[in] job The collection.
	//! \return True when every container has been drained.
	protected bool StepCollect(OVT_StorageJob job)
	{
		OVT_StorageComponent dest = ResolveStorage(job.m_DestId);
		if (!dest)
			return true;

		OVT_StorageLedger destLedger = dest.GetLedger();
		if (!destLedger)
			return true;

		// A holder spawned this frame has not resolved its capacity yet - the resolve is a call-queue
		// hop - and reading the unresolved 0 would report a full ledger on an empty truck.
		int capacity = OVT_StorageComponent.UNLIMITED_CAPACITY;
		if (dest.IsCapacityResolved())
			capacity = dest.GetCapacity();

		int budget = ChunkBudget();

		while (budget > 0 && job.m_iHolderCursor < job.m_aHolders.Count())
		{
			// A container costs one work item of its own, so a base with a hundred empty crates still
			// chunks instead of walking the whole queue in one frame.
			budget--;

			IEntity container = GetGame().GetWorld().FindEntityByID(job.m_aHolders[job.m_iHolderCursor]);
			if (!container)
			{
				job.AdvanceHolder();
				continue;
			}

			if (!job.m_bHolderOpened)
			{
				CollectSweepItems(container, job.m_aPending);
				job.m_iCursor = 0;
				job.m_bHolderOpened = true;
			}

			InventoryStorageManagerComponent inventory = OVT_StorageUtils.GetInventoryManager(container);

			while (budget > 0 && job.m_iCursor < job.m_aPending.Count())
			{
				budget--;

				EntityID id = job.m_aPending[job.m_iCursor];
				job.m_iCursor++;

				int outcome = ConvertItemToLedger(GetGame().GetWorld().FindEntityByID(id), inventory, destLedger, capacity);

				if (outcome == CONVERT_MOVED)
					job.m_iMoved++;
				else if (outcome == CONVERT_FAILED || outcome == CONVERT_FULL)
					job.m_iShortfall++;
			}

			// Out of budget part-way through this container: resume at the same cursor next chunk.
			if (job.m_iCursor < job.m_aPending.Count())
				return false;

			MergeHolderLedger(container, dest, destLedger, capacity, job);
			job.AdvanceHolder();
		}

		return job.m_iHolderCursor >= job.m_aHolders.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Moves a drained container's own ledger into the collection's destination.
	//! \param[in] container The container just drained.
	//! \param[in] dest The destination holder.
	//! \param[in] destLedger Its ledger.
	//! \param[in] capacity Its capacity.
	//! \param[in] job The collection, for the moved/shortfall tally.
	protected void MergeHolderLedger(IEntity container, OVT_StorageComponent dest, OVT_StorageLedger destLedger, int capacity, OVT_StorageJob job)
	{
		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(container);
		if (!storage || storage == dest)
			return;

		OVT_StorageLedger from = storage.GetLedger();
		if (!from || from.Total() <= 0)
			return;

		array<string> res = new array<string>();
		array<int> counts = new array<int>();
		from.GetLines(res, counts);

		for (int i = 0; i < res.Count(); i++)
		{
			int shortfall;
			job.m_iMoved += OVT_StorageRules.TransferLedgerLine(from, destLedger, res[i], counts[i], capacity, shortfall);
			job.m_iShortfall += shortfall;
		}

		// One BumpMe for a container the caller is about to delete, so a client watching it when the
		// job aborts is not left reading the pre-collection number.
		storage.PublishCount();
	}

	//------------------------------------------------------------------------------------------------
	//! EXPORT: ledger -> money, one step, at a port.
	//! \param[in] job The export.
	protected void RunExport(OVT_StorageJob job)
	{
		OVT_StorageComponent source = ResolveStorage(job.m_SourceId);
		if (!source)
			return;

		OVT_StorageLedger ledger = source.GetLedger();
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!ledger || !economy)
			return;

		OVT_PlayerData player = OVT_PlayerData.Get(job.m_iPlayerId);
		vector pos = source.GetOwner().GetOrigin();

		while (job.LineCount() > 0)
		{
			string res = job.m_aRes[0];
			int qty = job.m_aQty[0];

			job.DropFrontLine();

			int unitPrice = ResolveExportUnitPrice(economy, player, pos, res);
			if (unitPrice <= 0)
			{
				job.m_iShortfall += qty;
				continue;
			}

			int held = ledger.Count(res);
			if (qty > held)
			{
				job.m_iShortfall += qty - held;
				qty = held;
			}

			if (qty <= 0)
				continue;

			int taken = ledger.Take(res, qty);

			job.m_iEarned += taken * unitPrice;
			job.m_iMoved += taken;
			job.m_iShortfall += qty - taken;
		}

		if (job.m_iEarned > 0)
			economy.DoAddPlayerMoney(job.m_iPlayerId, job.m_iEarned);
	}

	//-----------------------------------------------------------------------------------------------
	// SWEEP ENUMERATION
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Builds the sweep's work list: every item the holder is carrying, with each container's contents
	//! and each weapon's parts queued AHEAD of the thing that holds them.
	//!
	//! There is NO REGISTRY GATE anywhere in here. The string came off an entity the server is about
	//! to delete, so gating on IsRegisteredResource would delete a player's looted occupying-faction
	//! gear and credit nothing for it.
	//! \param[in] holder The holder being swept.
	//! \param[out] pending Receives the work list, in processing order.
	protected void CollectSweepItems(IEntity holder, out array<EntityID> pending)
	{
		if (!pending)
			pending = new array<EntityID>();

		pending.Clear();

		if (!holder)
			return;

		InventoryStorageManagerComponent inventory = OVT_StorageUtils.GetInventoryManager(holder);
		if (!inventory)
			return;

		array<IEntity> items = new array<IEntity>();
		inventory.GetItems(items);

		foreach (IEntity item : items)
		{
			if (!item)
				continue;

			BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
			if (weapon)
				StripWeapon(inventory, weapon, pending);

			QueueStoredContents(item, pending);

			pending.Insert(item.GetID());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Flat list of everything a holder carries, for the officer clear. No stripping and no recursion:
	//! the deletion is meant to cascade.
	//! \param[in] holder The holder.
	//! \param[out] pending Receives the work list.
	protected void CollectInventoryItems(IEntity holder, out array<EntityID> pending)
	{
		if (!pending)
			pending = new array<EntityID>();

		pending.Clear();

		InventoryStorageManagerComponent inventory = OVT_StorageUtils.GetInventoryManager(holder);
		if (!inventory)
			return;

		array<IEntity> items = new array<IEntity>();
		inventory.GetItems(items);

		foreach (IEntity item : items)
		{
			if (item)
				pending.Insert(item.GetID());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Detaches a weapon's loaded magazine and every attachment, and queues them as items in their own
	//! right so each converts to its own ledger line.
	//! \param[in] inventory The HOLDER's inventory manager - a weapon has none of its own.
	//! \param[in] weapon The weapon component.
	//! \param[out] pending The work list being built.
	protected void StripWeapon(InventoryStorageManagerComponent inventory, BaseWeaponComponent weapon, out array<EntityID> pending)
	{
		BaseMagazineComponent magazine = weapon.GetCurrentMagazine();
		if (magazine)
			DetachWeaponPart(inventory, magazine.GetOwner(), pending);

		array<AttachmentSlotComponent> slots = new array<AttachmentSlotComponent>();
		weapon.GetAttachments(slots);

		foreach (AttachmentSlotComponent slot : slots)
		{
			if (!slot)
				continue;

			IEntity part = slot.GetAttachedEntity();

			// A scope the weapon's own prefab declares stays mounted: detaching it would strand a
			// loose optic in the container, since it converts to nothing.
			if (OVT_PrefabPartUtils.IsDeclaredPart(part))
				continue;

			DetachWeaponPart(inventory, part, pending);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Removes one part from the weapon storage it is slotted into, THROUGH THE HOLDER'S MANAGER.
	//!
	//! A weapon entity carries no InventoryStorageManagerComponent, so the operation is performed by
	//! whichever manager owns the transfer - the shape
	//! SCR_MultiPartDeployableItemComponent.TryRemoveMagazineFromWeapons uses. A part that will not
	//! come off is NOT queued, and its weapon is skipped later by ItemStillHoldsSomething rather than
	//! deleted with the part still in it.
	//! \param[in] inventory The holder's inventory manager.
	//! \param[in] part The magazine or attachment.
	//! \param[out] pending The work list being built.
	protected void DetachWeaponPart(InventoryStorageManagerComponent inventory, IEntity part, out array<EntityID> pending)
	{
		if (!part || !inventory)
			return;

		InventoryItemComponent item = InventoryItemComponent.Cast(part.FindComponent(InventoryItemComponent));
		if (!item)
			return;

		InventoryStorageSlot slot = item.GetParentSlot();
		if (!slot)
			return;

		BaseInventoryStorageComponent storage = slot.GetStorage();
		if (!storage)
			return;

		if (!inventory.TryRemoveItemFromStorage(part, storage, null))
			return;

		pending.Insert(part.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! Queues everything inside an item ahead of the item itself.
	//!
	//! A duplicate id in the work list is harmless: the second visit resolves to a deleted entity and
	//! is skipped, which is also how an item something else removed mid-sweep is handled.
	//! \param[in] item The candidate container.
	//! \param[out] pending The work list being built.
	protected void QueueStoredContents(IEntity item, out array<EntityID> pending)
	{
		// A harness carries no storage of its own - its pouches do, and they are child entities that
		// go with it when it converts. Their contents must be queued AHEAD of it or they are deleted
		// uncredited.
		array<IEntity> attached = new array<IEntity>();
		OVT_PrefabPartUtils.CollectAttachedParts(item, attached);

		foreach (IEntity part : attached)
		{
			QueueStoredContents(part, pending);
		}

		array<Managed> storages = new array<Managed>();
		item.FindComponents(BaseUniversalInventoryStorageComponent, storages);

		foreach (Managed found : storages)
		{
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(found);
			if (!storage)
				continue;

			array<IEntity> contents = new array<IEntity>();
			storage.GetAll(contents);

			foreach (IEntity contained : contents)
			{
				if (contained)
					pending.Insert(contained.GetID());
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Whether deleting this item would destroy something else that has not been credited.
	//!
	//! The universal-storage half is the rule OVT_ShopTransactionComponent.HasStoredContents already
	//! enforces, so "container" means the same thing on both paths. The weapon half exists because a
	//! WeaponAttachmentsStorageComponent is NOT a universal storage, so a loaded magazine and a
	//! mounted optic are invisible to the first half and would go with the weapon.
	//! \param[in] item The candidate about to be deleted.
	//! \return True when something is still inside it.
	protected bool ItemStillHoldsSomething(IEntity item)
	{
		if (!item)
			return false;

		array<Managed> storages = new array<Managed>();
		item.FindComponents(BaseUniversalInventoryStorageComponent, storages);

		foreach (Managed found : storages)
		{
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(found);
			if (!storage)
				continue;

			array<InventoryItemComponent> contents = new array<InventoryItemComponent>();
			storage.GetOwnedItems(contents);

			if (!contents.IsEmpty())
				return true;
		}

		array<IEntity> attached = new array<IEntity>();
		OVT_PrefabPartUtils.CollectAttachedParts(item, attached);

		foreach (IEntity part : attached)
		{
			if (ItemStillHoldsSomething(part))
				return true;
		}

		BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
		if (!weapon)
			return false;

		if (weapon.GetCurrentMagazine())
			return true;

		array<AttachmentSlotComponent> slots = new array<AttachmentSlotComponent>();
		weapon.GetAttachments(slots);

		foreach (AttachmentSlotComponent slot : slots)
		{
			// A declared attachment is meant to go with the weapon; only a mounted part that would
			// have converted on its own blocks the delete.
			if (slot && slot.GetAttachedEntity() && !OVT_PrefabPartUtils.IsDeclaredPart(slot.GetAttachedEntity()))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether an item may become a ledger line at all. Only magazines can answer no.
	//! \param[in] item The candidate.
	//! \return True unless it is a part-used magazine.
	protected bool MagazineConverts(IEntity item)
	{
		BaseMagazineComponent magazine = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
		if (!magazine)
			return true;

		return OVT_StorageRules.MagazineIsFull(magazine.GetAmmoCount(), magazine.GetMaxAmmoCount());
	}


	//------------------------------------------------------------------------------------------------
	//! A storage the HOLDER itself owns - never one that is an item stored inside it.
	//!
	//! TrySpawnPrefabToStorage with a null storage picks any storage in the hierarchy, so a bag
	//! withdrawn a moment ago swallows everything withdrawn after it. Withdrawals land in the holder's
	//! own storage or they do not land.
	//! \param[in] inventory The holder's manager.
	//! \param[in] item The entity about to be moved, or null when a prefab is about to be spawned.
	//! \param[in] prefab The prefab about to be spawned; ignored when item is set.
	//! \param[out] holderHasOwnStorage False when the holder owns no un-nested storage at all, which is
	//!             the only case a caller may fall back to letting the engine choose.
	//! \return The storage to use, or null when none of the holder's own will take it.
	protected BaseInventoryStorageComponent ResolveHolderStorage(InventoryStorageManagerComponent inventory, IEntity item, ResourceName prefab, out bool holderHasOwnStorage)
	{
		holderHasOwnStorage = false;

		if (!inventory)
			return null;

		array<BaseInventoryStorageComponent> storages = new array<BaseInventoryStorageComponent>();
		inventory.GetStorages(storages, EStoragePurpose.PURPOSE_ANY);

		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!storage || StorageIsNested(storage))
				continue;

			holderHasOwnStorage = true;

			if (item)
			{
				if (inventory.CanInsertItemInStorage(item, storage))
					return storage;

				continue;
			}

			if (inventory.CanInsertResourceInStorage(prefab, storage))
				return storage;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a storage is itself an item inside another storage - a bag in the bed, a pouch in a bag.
	//!
	//! A vehicle's cargo storage sits on a structural child entity, which is in no inventory slot, so
	//! it answers false; a stored container answers true through whichever of the two item components
	//! carries its slot.
	//! \param[in] storage The storage to classify.
	//! \return True when it lives inside another storage.
	protected bool StorageIsNested(BaseInventoryStorageComponent storage)
	{
		if (storage.GetParentSlot())
			return true;

		IEntity owner = storage.GetOwner();
		if (!owner)
			return false;

		InventoryItemComponent item = InventoryItemComponent.Cast(owner.FindComponent(InventoryItemComponent));
		if (!item)
			return false;

		return item.GetParentSlot() != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves an item that cannot become a ledger line out of the container holding it.
	//!
	//! The sweep queues contents AHEAD of their container, so by the time the container is examined
	//! this has already run for everything in it. A no-op when the item is already loose in the holder
	//! or when nothing of the holder's own will take it - the container is then skipped as before.
	//! \param[in] inventory The holder's manager.
	//! \param[in] item The item to eject.
	protected void EjectToHolderStorage(InventoryStorageManagerComponent inventory, IEntity item)
	{
		if (!inventory || !item)
			return;

		InventoryItemComponent itemComp = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		if (!itemComp)
			return;

		InventoryStorageSlot slot = itemComp.GetParentSlot();
		if (!slot)
			return;

		BaseInventoryStorageComponent current = slot.GetStorage();
		if (!current || !StorageIsNested(current))
			return;

		bool holderHasOwnStorage;
		BaseInventoryStorageComponent target = ResolveHolderStorage(inventory, item, "", holderHasOwnStorage);
		if (!target)
			return;

		inventory.TryMoveItemToStorage(item, target);
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the "seen doing it" window for a loot run.
	//!
	//! Re-armed at every chunk rather than once for the whole run: the run's length is the size of the
	//! battlefield and is not known when it starts, and the question the wanted system asks is whether
	//! anyone saw you AT ANY POINT while you did it - the uprising hold's rule, applied to a job whose
	//! duration is data.
	//! \param[in] playerId The looting player.
	protected void ArmLootIllegalWindow(int playerId)
	{
		OVT_PlayerWantedComponent wanted = ResolvePlayerWanted(playerId);
		if (!wanted)
			return;

		wanted.BeginIllegalAction(LOOT_ILLEGAL_REASON, LOOT_ILLEGAL_SECONDS);
	}

	//------------------------------------------------------------------------------------------------
	//! Closes the loot window, but only when it is still OURS - a window another act opened in the
	//! meantime is not this job's to close.
	//! \param[in] playerId The looting player.
	protected void ClearLootIllegalWindow(int playerId)
	{
		OVT_PlayerWantedComponent wanted = ResolvePlayerWanted(playerId);
		if (!wanted)
			return;

		if (wanted.GetIllegalActionReason() != LOOT_ILLEGAL_REASON)
			return;

		wanted.EndIllegalAction();
	}

	//------------------------------------------------------------------------------------------------
	//! The wanted component on a player's live character.
	//! \param[in] playerId Runtime player id.
	//! \return The component, or null when the player has no character (a recruit-driven job included).
	protected OVT_PlayerWantedComponent ResolvePlayerWanted(int playerId)
	{
		if (playerId <= 0)
			return null;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!character)
			return null;

		return OVT_PlayerWantedComponent.Cast(character.FindComponent(OVT_PlayerWantedComponent));
	}

	//-----------------------------------------------------------------------------------------------
	// ENGINE HELPERS
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Destroys one item.
	//!
	//! TryDeleteItem first, because it keeps the owning storage's bookkeeping straight. The fallback
	//! is for the parts a strip has already detached, which no longer belong to any storage the
	//! manager owns - the same two-step OVT_LoadoutManagerComponent.ClearEntityEquipment ships.
	//! \param[in] inventory The holder's inventory manager.
	//! \param[in] item The item to destroy.
	//! \return True when the item is gone.
	protected bool DeleteItem(InventoryStorageManagerComponent inventory, IEntity item)
	{
		if (!item)
			return false;

		if (inventory && inventory.TryDeleteItem(item))
			return true;

		SCR_EntityHelper.DeleteEntityAndChildren(item);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The storage component behind a holder id.
	//! \param[in] id The holder id.
	//! \return The component, or null when the holder is gone.
	protected OVT_StorageComponent ResolveStorage(RplId id)
	{
		return OVT_StorageUtils.GetStorage(OVT_StorageUtils.ResolveHolder(id));
	}

	//------------------------------------------------------------------------------------------------
	//! Work items one chunk may examine.
	//!
	//! Read from the attribute every chunk, never captured at job start, and never the hard-coded
	//! literal the shipped OVT_StorageOperationConfig's argument order silently turns every caller's 5
	//! into a 1.
	//! \return At least one.
	protected int ChunkBudget()
	{
		if (m_iItemsPerChunk < 1)
			return 1;

		return m_iItemsPerChunk;
	}

	//------------------------------------------------------------------------------------------------
	//! The progress caption for an operation.
	//! \param[in] op The operation.
	//! \return A localization key.
	protected string ProgressKeyFor(EOVT_StorageOp op)
	{
		if (op == EOVT_StorageOp.TO_INVENTORY)
			return "#OVT-Progress-StorageTaking";

		if (op == EOVT_StorageOp.TO_HOLDER)
			return "#OVT-Progress-StorageMoving";

		if (op == EOVT_StorageOp.TO_STORAGE)
			return "#OVT-Progress-StorageStoring";

		if (op == EOVT_StorageOp.EXPORT)
			return "#OVT-Progress-StorageExporting";

		if (op == EOVT_StorageOp.CLEAR)
			return "#OVT-Progress-StorageClearing";

		if (op == EOVT_StorageOp.COLLECT)
			return "#OVT-Progress-CollectingContainers";

		return "#OVT-Progress-LootingBattlefield";
	}

	//------------------------------------------------------------------------------------------------
	//! Whether both the caller and the holder are standing at a port.
	//!
	//! The distances are the ones port import already enforces on both ends, so a sale is never
	//! accepted where a purchase at the same spot would be refused.
	//! \param[in] playerId The caller.
	//! \param[in] holder The holder being sold.
	//! \return True when both are at a port.
	protected bool AtAPort(int playerId, IEntity holder)
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy || !holder)
			return false;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!character)
			return false;

		float callerDistance = economy.DistanceToNearestPort(character.GetOrigin());
		if (callerDistance < 0 || callerDistance > EXPORT_MAX_PORT_DISTANCE)
			return false;

		float holderDistance = economy.DistanceToNearestPort(holder.GetOrigin());
		if (holderDistance < 0 || holderDistance > EXPORT_MAX_PORT_DISTANCE)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! What the port pays for one of a resource, or 0 when it will not take it.
	//!
	//! The illegal gate is the SAME expression port import applies to the same catalogue
	//! (OVT_VehicleRequestComponent.RpcAsk_ImportToVehicle): an item no ordinary shop stocks moves
	//! only under the Trade L5 permission or at a resistance-held port.
	//! \param[in] economy The economy manager.
	//! \param[in] player The caller's record; may be null.
	//! \param[in] pos Where the sale happens - prices are location dependent.
	//! \param[in] res Prefab ResourceName.
	//! \return Dollars per item, or 0 when the port refuses it.
	protected int ResolveExportUnitPrice(OVT_EconomyManagerComponent economy, OVT_PlayerData player, vector pos, string res)
	{
		// An uncatalogued variant exports as the registered prefab it inherits.
		ResourceName pricing = economy.ResolvePricingResource(res);
		if (pricing.IsEmpty())
			return 0;

		int id = economy.GetInventoryId(pricing);

		bool soldAtShop = economy.IsSoldAtAnyNonVehicleShop(pricing);

		int minShopBuyPrice = -1;
		if (soldAtShop)
		{
			minShopBuyPrice = economy.GetBuyPrice(id, pos, -1);
		}
		else
		{
			bool permitted = false;
			if (player && player.HasPermission("IllegalImports"))
				permitted = true;

			if (!permitted && economy.ResistanceControlsNearestPort(pos))
				permitted = true;

			if (!permitted)
				return 0;
		}

		return OVT_StorageRules.ExportUnitPrice(economy.GetPrice(id), m_fExportPriceRatio, minShopBuyPrice);
	}

	//-----------------------------------------------------------------------------------------------
	// SERVER GATE
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! THE single server gate. Every one of the eight asks runs it, and every refusal it produces is
	//! answered to the player rather than logged and dropped.
	//!
	//! Steps 4 and 5 of the plan's ladder are ONE call: PlayerMayUseVehicleFor works on any entity, so
	//! a locked vehicle and a locked crate answer to the same rule, and an entity with no
	//! OVT_PlayerOwnerComponent passes it. That is the same collapse OVT_ContainerTransferComponent's
	//! CallerMayReach already makes, and it is what keeps the sell, upgrade, FOB and storage paths
	//! from disagreeing about who may empty a given vehicle.
	//! \param[in] playerId The caller, resolved from the entity the RPC arrived on.
	//! \param[in] holder The holder, already resolved from its RplId.
	//! \param[out] rejectKey Localization key describing the refusal; "" when the gate passes.
	//! \return True when the caller may act on this holder.
	protected bool MayUseHolder(int playerId, IEntity holder, out string rejectKey)
	{
		rejectKey = "";

		if (playerId <= 0)
		{
			rejectKey = "#OVT-Storage_Failed";
			return false;
		}

		if (!holder)
		{
			rejectKey = "#OVT-Storage_NotFound";
			return false;
		}

		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(holder);
		if (!storage)
		{
			rejectKey = "#OVT-Storage_NotFound";
			return false;
		}

		if (storage.GetCapacity() == OVT_StorageComponent.NO_CAPACITY)
		{
			rejectKey = "#OVT-Storage_NoCapacity";
			return false;
		}

		if (!CallerIsWithin(playerId, holder.GetOrigin(), m_fUseRadius))
		{
			rejectKey = "#OVT-Storage_TooFar";
			return false;
		}

		if (!OVT_ControllerRequestComponent.PlayerMayUseVehicleFor(playerId, holder))
		{
			rejectKey = "#OVT-Storage_Locked";
			return false;
		}

		if (!WarehouseIsAccessible(playerId, holder))
		{
			rejectKey = "#OVT-Storage_NoAccess";
			return false;
		}

		if (!OVT_StructureDamage.IsUsable(holder))
		{
			rejectKey = "#OVT-Storage_Ruined";
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a real-estate warehouse building is open to a player. Anything that is not one passes.
	//!
	//! ONE BODY, on the real-estate manager, shared with the client's warehouse-button visibility check
	//! and the storage user actions so the button and this gate cannot drift (I5).
	//! \param[in] playerId The caller.
	//! \param[in] holder The candidate building.
	//! \return True when the caller may use it, or when it is not a warehouse at all.
	protected bool WarehouseIsAccessible(int playerId, IEntity holder)
	{
		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if (!realEstate)
			return true;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
			return false;

		return realEstate.PlayerMayUseWarehouse(players.GetPersistentIDFromPlayerID(playerId), holder);
	}

	//------------------------------------------------------------------------------------------------
	//! Which player owns the controller this component sits on, on the SERVER.
	//!
	//! Delegates to the identity rule's one body on OVT_ControllerRequestComponent, which this class
	//! cannot inherit (it needs the progress hierarchy and EnforceScript has no multiple inheritance).
	//! \return Runtime player id, or -1 - which every caller treats as a rejection.
	protected int ResolveCallerPlayerId()
	{
		return OVT_ControllerRequestComponent.ResolveOwningPlayerIdFor(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the caller's controlled character is within a radius of a position. A caller with no
	//! character fails, which is correct: every verb here is performed by a body standing at a holder.
	//!
	//! The range test itself is OVT_StorageRules.HolderIsInRange, so the Logic case that pins its
	//! boundary behaviour (vector.Distance is not correctly rounded) pins the shipped gate and not a
	//! copy of it.
	//! \param[in] playerId The caller.
	//! \param[in] pos The position to test against.
	//! \param[in] maxDistance The radius.
	//! \return True when the caller has a character and it is close enough.
	protected bool CallerIsWithin(int playerId, vector pos, float maxDistance)
	{
		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!character)
			return false;

		return OVT_StorageRules.HolderIsInRange(pos, character.GetOrigin(), maxDistance);
	}

	//-----------------------------------------------------------------------------------------------
	// CLIENT STATE ACCESSORS
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether the controller this component sits on is the LOCAL player's own.
	//!
	//! A client holds a replicated instance of this component for every connected player's controller.
	//! Only the local player's may send a request: an RPC sent on somebody else's would be resolved by
	//! the server as coming from that player.
	//! \return True when this is the local player's controller.
	protected bool IsLocalControllerOwner()
	{
		OVT_OverthrowController localController = OVT_Global.GetController();
		if (!localController)
			return false;

		IEntity localEntity = localController;

		return localEntity == GetOwner();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a record belongs to the contents fan currently being staged. THE STALE-DISCARD RULE.
	//! \param[in] seq The record's sequence id.
	//! \return True when the record may be staged.
	protected bool IsStagingContents(int seq)
	{
		return m_bContentsStaging && seq == m_iContentsStagingSeq;
	}

	//------------------------------------------------------------------------------------------------
	//! Which sequence space a number belongs to. Contents sequences are even and non-zero; checkout
	//! sequences are odd.
	//! \param[in] seq The sequence id.
	//! \return True when it names a contents pull.
	protected bool IsContentsSeq(int seq)
	{
		return (seq % 2) == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The last complete contents fan. Never null; empty until one arrives.
	//! \return The committed snapshot.
	OVT_StorageSnapshot GetSnapshot()
	{
		return m_Snapshot;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the committed snapshot describes a particular holder.
	//! \param[in] holder The holder's RplId.
	//! \return True when the snapshot is that holder's.
	bool HasSnapshotFor(RplId holder)
	{
		return holder.IsValid() && m_Snapshot.m_HolderId == holder;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while a contents fan is arriving.
	bool IsAwaitingContents()
	{
		return m_bContentsStaging;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while a checkout is unanswered.
	bool IsAwaitingBatch()
	{
		return m_bAwaitingBatch;
	}

	//------------------------------------------------------------------------------------------------
	//! Fires when a contents fan commits. Read GetSnapshot() for which holder it described.
	//! \return The invoker, allocated on first ask.
	ScriptInvoker GetOnContentsUpdated()
	{
		if (!m_OnContentsUpdated)
			m_OnContentsUpdated = new ScriptInvoker();

		return m_OnContentsUpdated;
	}

	//------------------------------------------------------------------------------------------------
	//! Fires (string messageKey) when a request is refused.
	//! \return The invoker, allocated on first ask.
	ScriptInvoker GetOnStorageError()
	{
		if (!m_OnStorageError)
			m_OnStorageError = new ScriptInvoker();

		return m_OnStorageError;
	}

	//------------------------------------------------------------------------------------------------
	//! Fires (int moved, int shortfall, int earned) when a checkout finishes.
	//! \return The invoker, allocated on first ask.
	ScriptInvoker GetOnBatchResult()
	{
		if (!m_OnBatchResult)
			m_OnBatchResult = new ScriptInvoker();

		return m_OnBatchResult;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Radius the destination picker collects nearby holders from.
	float GetHolderRadius()
	{
		return m_fHolderRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How far the caller may be from a holder and still act on it.
	float GetUseRadius()
	{
		return m_fUseRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Fraction of the import price a port export pays out.
	float GetExportPriceRatio()
	{
		return m_fExportPriceRatio;
	}

	//------------------------------------------------------------------------------------------------
	//! What the port would pay for one of a resource. Reads config and replicated state only, so the
	//! Export screen prices its rows through the SAME method the server bills with - there is no
	//! second pricing expression to drift from.
	//! \param[in] player The caller's record; may be null.
	//! \param[in] pos Where the sale would happen - prices are location dependent.
	//! \param[in] res Prefab ResourceName.
	//! \return Dollars per item, or 0 when the port refuses it.
	int GetExportUnitPrice(OVT_PlayerData player, vector pos, string res)
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
			return 0;

		return ResolveExportUnitPrice(economy, player, pos, res);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Most lines one checkout may carry.
	int GetMaxCartLines()
	{
		return m_iMaxCartLines;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Items the job engine converts per chunk.
	int GetItemsPerChunk()
	{
		return m_iItemsPerChunk;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Milliseconds between job engine chunks.
	int GetChunkDelayMs()
	{
		return m_iChunkDelayMs;
	}
}
