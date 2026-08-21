//------------------------------------------------------------------------------------------------
//! What a resource checkout is doing. APPEND-ONLY: the value travels on the wire as a bare int, so
//! renumbering an existing member silently re-points every client build that has not been rebuilt.
//------------------------------------------------------------------------------------------------
enum EOVT_ResourceOp
{
	HOLDER_TO_HOLDER,
	HOLDER_TO_GROUND,
	PORT_IMPORT,
	PORT_EXPORT
}

//------------------------------------------------------------------------------------------------
//! One line of a streamed cart: which definition index, and how many units.
//!
//! The wire carries the definition INDEX, not the string id - resources.conf is a mod file and is
//! byte-identical on every machine, so the index means the same thing on both ends and costs four
//! bytes instead of a string.
//------------------------------------------------------------------------------------------------
class OVT_ResourceCartLine : Managed
{
	int m_iResIndex;

	int m_iQuantity;

	//------------------------------------------------------------------------------------------------
	void OVT_ResourceCartLine()
	{
		m_iResIndex = -1;
		m_iQuantity = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! One player's in-flight checkout: opened by Begin, filled by Line, validated and run by Commit.
//!
//! It holds no derived state at all - not the litre total, not the money total, not a resolved
//! holder. Commit RE-READS both holders and RE-DERIVES every number from the live ledgers, because
//! anything cached here would be a snapshot taken before the lines arrived and would let a cart
//! validated against one world state mutate another.
//------------------------------------------------------------------------------------------------
class OVT_ResourceCheckout : Managed
{
	//! Who opened it, resolved on the server from the controller entity. Never a wire parameter.
	int m_iPlayerId;

	//! The client's sequence. Echoed untouched into the single reply this checkout gets.
	int m_iSeq;

	//! An EOVT_ResourceOp value.
	int m_iOp;

	RplId m_SourceId;

	//! Equal to m_SourceId for HOLDER_TO_GROUND and for both port ops.
	RplId m_DestId;

	//! Set when a streamed line arrived malformed. The refusal is DEFERRED to Commit so one bad line
	//! produces one refusal for the order rather than one per line.
	bool m_bMalformed;

	//! In the order the client streamed them. The index of a line is observable - RpcAsk_TransferLine
	//! requires index == LineCount() - so nothing may ever be removed from this array out of order.
	ref array<ref OVT_ResourceCartLine> m_aLines;

	//------------------------------------------------------------------------------------------------
	void OVT_ResourceCheckout()
	{
		m_iPlayerId = -1;
		m_iSeq = 0;
		m_iOp = EOVT_ResourceOp.HOLDER_TO_HOLDER;
		m_SourceId = RplId.Invalid();
		m_DestId = RplId.Invalid();
		m_bMalformed = false;
		m_aLines = new array<ref OVT_ResourceCartLine>();
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many lines have been accepted so far.
	int LineCount()
	{
		return m_aLines.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] resIndex Definition index.
	//! \return True when a line already names this resource.
	bool Holds(int resIndex)
	{
		foreach (OVT_ResourceCartLine line : m_aLines)
		{
			if (line.m_iResIndex == resIndex)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Appends one line.
	//! \param[in] resIndex Definition index.
	//! \param[in] qty How many units.
	void AddLine(int resIndex, int qty)
	{
		OVT_ResourceCartLine line = new OVT_ResourceCartLine();
		line.m_iResIndex = resIndex;
		line.m_iQuantity = qty;

		m_aLines.Insert(line);
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative resource transfers for one player")]
class OVT_ResourceRequestComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative resource transfers, on the per-player OVT_OverthrowController entity.
//!
//! IDENTITY IS NEVER A PARAMETER. It comes from the entity the RPC arrived on
//! (OVT_ControllerRequestComponent.ResolveOwningPlayerId), which is what makes it unspoofable.
//!
//! THE WIRE IS A Begin ... Line ... Commit FAN, not one ask per line (D7). Whole-cart atomicity (D1)
//! is a correctness requirement, and per-line asks cannot be atomic: three lines that each fit
//! against a running total, with a fourth that does not, IS a clamp. Nothing in this component ever
//! clamps a cart - it is accepted whole or refused whole.
//!
//! ONE ANSWER PER CHECKOUT, AND EXACTLY ONE.
//!   - Begin refuses everything decidable without the lines, and NULLS the checkout as it does, so
//!     the Line and Commit that are already in flight behind it find nothing and answer nothing.
//!   - Line NEVER answers. The client streams Begin, up to m_iMaxCartLines lines and Commit back to
//!     back before any reply can arrive, so answering here would send one refusal per line for one
//!     order. A malformed line is remembered on the checkout and refused once, at Commit.
//!   - Commit answers exactly once: RpcDo_ResourceError, or RpcDo_TransferResult.
//!
//! ⚠ ARITY IS HAND-AUDITED (BUG-090). Rpc() is an untyped variadic prototype, so an argument count
//! that does not match the handler compiles clean and dies silently at the wire. Every Rpc() in this
//! file therefore appears EXACTLY ONCE per handler, on the line immediately after a DIRECT call to
//! the same handler with the same argument list - the direct call is type-checked by the compiler and
//! the Rpc() beside it is a two-line visual diff. There is no generic send helper anywhere: a
//! forwarder would hide the one mistake reading this file is meant to catch.
//!
//! ⚠ ON A LISTEN HOST THE WHOLE REPLY FAN RUNS SYNCHRONOUSLY INSIDE THE ASK. A caller must latch
//! BEFORE calling, never after.
//------------------------------------------------------------------------------------------------
class OVT_ResourceRequestComponent : OVT_ControllerRequestComponent
{
	//! "This reply belongs to no checkout" - the action path (RpcAsk_BuildFromSite) carries no
	//! sequence, so its refusals are surfaced as a hint instead. The screen's counter starts at 1.
	static const int SEQ_NONE = 0;

	//! Sanity bound on a client-supplied unit count, matching OVT_VehicleRequestComponent's
	//! IMPORT_MAX_QUANTITY. NOT a gameplay limit - a truck's capacity refuses long before this.
	//!
	//! IT IS AN OVERFLOW GUARD. A port import has no source holder to bound the quantity against, so
	//! without it a cart of 16 lines at int.MAX units overflows the litre sum and the money total into
	//! NEGATIVE numbers, which passes both the free-litre test and PlayerHasMoney() and then pays the
	//! player to take the goods. The running totals are tripwired against negatives as well, so raising
	//! this constant cannot quietly reopen it.
	static const int MAX_LINE_QUANTITY = 10000;

	//-----------------------------------------------------------------------------------------------
	// ATTRIBUTES
	//-----------------------------------------------------------------------------------------------

	[Attribute(defvalue: "25", desc: "Metres the destination picker reaches for other holders")]
	protected float m_fHolderRadius;

	[Attribute(defvalue: "30", desc: "Metres the caller may stand from a holder and still use it")]
	protected float m_fUseRadius;

	[Attribute(defvalue: "30", desc: "Metres the caller and the holder may stand from a port")]
	protected float m_fPortRadius;

	[Attribute(defvalue: "16", desc: "Most lines one checkout may carry")]
	protected int m_iMaxCartLines;

	[Attribute(defvalue: "3", desc: "Metres a dropped pile clears its source: aft of the hull when unloading a vehicle, in front of the player otherwise")]
	protected float m_fUnloadOffset;

	//-----------------------------------------------------------------------------------------------
	// SERVER STATE
	//-----------------------------------------------------------------------------------------------

	//! The checkout being streamed. Null between checkouts, and nulled by a Begin refusal so the
	//! trailing lines and commit have nothing to attach themselves to.
	protected ref OVT_ResourceCheckout m_Checkout;

	//-----------------------------------------------------------------------------------------------
	// CLIENT STATE
	//-----------------------------------------------------------------------------------------------

	//! The live checkout sequence. Starts at 1 and only ever increments; 0 is SEQ_NONE.
	protected int m_iSeq;

	//! True between Begin and the single reply. The caller latches before the ask, not after.
	protected bool m_bAwaitingResult;

	//! (int movedLitres, int earned, int spent).
	protected ref ScriptInvoker m_OnTransferResult;

	//! (string messageKey).
	protected ref ScriptInvoker m_OnResourceError;

	//-----------------------------------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Drops any half-streamed checkout so a controller destroyed mid-fan leaves nothing behind.
	//! \param[in] owner The controller entity.
	override void OnDelete(IEntity owner)
	{
		m_Checkout = null;

		super.OnDelete(owner);
	}

	//-----------------------------------------------------------------------------------------------
	// CLIENT -> SERVER
	//
	// Each entry point does the client-side wrapping and then either calls the handler directly (we
	// are the authority - an RplRcver.Server RPC marshalled BY the server is delivered to nobody) or
	// sends it.
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Client: open a checkout. Follow with one RequestTransferLine per cart line, then
	//! RequestTransferCommit - one order, not one request per line.
	//!
	//! ⚠ THE CALLER MUST LATCH ON THE RETURNED SEQUENCE BEFORE THIS RETURNS IS IMPOSSIBLE - on a
	//! listen host the reply has already run by the time it does. Latch on m_bAwaitingResult, which
	//! is set here before anything is sent.
	//! \param[in] source The holder the resources come out of.
	//! \param[in] dest Where they go. Both slots always carry a VALID holder; the op kind decides
	//! which is read, and HOLDER_TO_GROUND passes the source in both.
	//! \param[in] opKind An EOVT_ResourceOp value.
	//! \param[in] lineCount How many lines will follow.
	//! \return The sequence this checkout runs under, or SEQ_NONE when nothing was sent.
	int RequestTransferBegin(RplId source, RplId dest, int opKind, int lineCount)
	{
		if (!IsLocalControllerOwner())
			return SEQ_NONE;

		m_iSeq += 1;
		m_bAwaitingResult = true;

		if (Replication.IsServer())
		{
			RpcAsk_TransferBegin(source, dest, opKind, m_iSeq, lineCount);
			return m_iSeq;
		}

		Rpc(RpcAsk_TransferBegin, source, dest, opKind, m_iSeq, lineCount);

		return m_iSeq;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: one cart line of an open checkout.
	//! \param[in] seq The sequence RequestTransferBegin returned.
	//! \param[in] index Position in the cart, counting from 0.
	//! \param[in] resIndex Definition index.
	//! \param[in] qty How many units.
	void RequestTransferLine(int seq, int index, int resIndex, int qty)
	{
		if (!IsLocalControllerOwner())
			return;

		if (Replication.IsServer())
		{
			RpcAsk_TransferLine(seq, index, resIndex, qty);
			return;
		}

		Rpc(RpcAsk_TransferLine, seq, index, resIndex, qty);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: close a checkout and run it.
	//! \param[in] seq The sequence RequestTransferBegin returned.
	//! \param[in] lineCount How many lines were sent; repeated so a short stream is detectable.
	void RequestTransferCommit(int seq, int lineCount)
	{
		if (!IsLocalControllerOwner())
			return;

		if (Replication.IsServer())
		{
			RpcAsk_TransferCommit(seq, lineCount);
			return;
		}

		Rpc(RpcAsk_TransferCommit, seq, lineCount);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: consume the piles around a construction site and complete it.
	//!
	//! The action path, not the screen path: it carries no sequence, so every answer arrives under
	//! SEQ_NONE and is surfaced as a hint.
	//! \param[in] site The construction site's RplId.
	void RequestBuildFromSite(RplId site)
	{
		if (!IsLocalControllerOwner())
			return;

		if (Replication.IsServer())
		{
			RpcAsk_BuildFromSite(site);
			return;
		}

		Rpc(RpcAsk_BuildFromSite, site);
	}

	//-----------------------------------------------------------------------------------------------
	// SERVER - THE ASKS
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: open a checkout.
	//!
	//! THE WHOLE CHECKOUT IS REFUSED HERE, before the lines stream, for everything decidable without
	//! them. m_Checkout is nulled first, so a refusal leaves nothing for the trailing lines and commit
	//! to attach themselves to and they answer nothing at all.
	//! \param[in] source The holder the resources come out of.
	//! \param[in] dest Where they go.
	//! \param[in] opKind An EOVT_ResourceOp value.
	//! \param[in] seq The client's checkout sequence.
	//! \param[in] lineCount How many lines the client says will follow.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TransferBegin(RplId source, RplId dest, int opKind, int seq, int lineCount)
	{
		// NOT A REFUSAL: the ask was delivered on a machine that owns nothing, so there is no request
		// here to refuse and nobody on this machine to answer.
		if (!Replication.IsServer())
			return;

		// A checkout opened and never committed is superseded here, and a refusal below must leave
		// nothing for the lines and commit already in flight to attach themselves to.
		m_Checkout = null;

		int playerId = ResolveOwningPlayerId();

		// SEQ_NONE names no checkout, so a refusal echoed under it would attach to nothing. It is
		// answered under SEQ_NONE deliberately - visible to the player as a hint, attached to no screen.
		if (seq == SEQ_NONE)
		{
			SendResourceError(playerId, SEQ_NONE, "#OVT-Resource_BadRequest");
			return;
		}

		if (playerId <= 0)
		{
			SendResourceError(playerId, seq, "#OVT-Resource_NoPlayer");
			return;
		}

		if (!IsKnownOp(opKind))
		{
			SendResourceError(playerId, seq, "#OVT-Resource_BadRequest");
			return;
		}

		if (lineCount <= 0 || lineCount > m_iMaxCartLines)
		{
			SendResourceError(playerId, seq, "#OVT-Resource_BadRequest");
			return;
		}

		if (opKind == EOVT_ResourceOp.HOLDER_TO_HOLDER && source == dest)
		{
			SendResourceError(playerId, seq, "#OVT-Resource_BadRequest");
			return;
		}

		string rejectKey;
		if (!HoldersUsable(playerId, source, dest, opKind, rejectKey))
		{
			SendResourceError(playerId, seq, rejectKey);
			return;
		}

		OVT_ResourceCheckout order = new OVT_ResourceCheckout();
		order.m_iPlayerId = playerId;
		order.m_iSeq = seq;
		order.m_iOp = opKind;
		order.m_SourceId = source;
		order.m_DestId = dest;

		m_Checkout = order;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one cart line.
	//!
	//! ⚠ THE ONE HANDLER IN THIS FILE THAT ANSWERS NOTHING, AND DELIBERATELY. See the class header.
	//! \param[in] seq The client's checkout sequence.
	//! \param[in] index Position in the cart. Reliable channels are ordered, so it must equal the
	//! number of lines already accepted; anything else means this is not the stream Begin opened.
	//! \param[in] resIndex Definition index.
	//! \param[in] qty How many units.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TransferLine(int seq, int index, int resIndex, int qty)
	{
		// NOT A REFUSAL - see RpcAsk_TransferBegin.
		if (!Replication.IsServer())
			return;

		// NOT A REFUSAL: this checkout was already answered at Begin. One answer per checkout.
		if (!m_Checkout || m_Checkout.m_iSeq != seq)
			return;

		if (index != m_Checkout.LineCount())
		{
			m_Checkout.m_bMalformed = true;
			return;
		}

		if (resIndex < 0 || qty <= 0 || qty > MAX_LINE_QUANTITY || m_Checkout.LineCount() >= m_iMaxCartLines)
		{
			m_Checkout.m_bMalformed = true;
			return;
		}

		// A repeated resource would be counted twice in the litre sum and taken twice from the source.
		if (m_Checkout.Holds(resIndex))
		{
			m_Checkout.m_bMalformed = true;
			return;
		}

		m_Checkout.AddLine(resIndex, qty);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: VALIDATE THE WHOLE CART, then run it.
	//!
	//! WHOLE-CART ATOMICITY (D1). Everything above the mutation marker re-reads the holders, re-derives
	//! every line's litres from the live catalogue, sums them and compares the sum against the
	//! destination's free litres and the money total against the player's balance. Any failure refuses
	//! the WHOLE cart with a key naming the shortfall. NOTHING CLAMPS, EVER - not a line, not a
	//! quantity, not a price. Nothing below the marker can refuse.
	//! \param[in] seq The client's checkout sequence.
	//! \param[in] lineCount How many lines the client says it sent.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TransferCommit(int seq, int lineCount)
	{
		// NOT A REFUSAL - see RpcAsk_TransferBegin.
		if (!Replication.IsServer())
			return;

		// NOT A REFUSAL: this checkout was already answered at Begin, or this is a duplicate commit.
		if (!m_Checkout || m_Checkout.m_iSeq != seq)
			return;

		OVT_ResourceCheckout order = m_Checkout;

		// Taken before anything can refuse, so a duplicated commit finds nothing and stays silent.
		m_Checkout = null;

		int playerId = ResolveOwningPlayerId();
		if (playerId <= 0)
		{
			SendResourceError(order.m_iPlayerId, seq, "#OVT-Resource_NoPlayer");
			return;
		}

		if (order.m_bMalformed)
		{
			SendResourceError(playerId, seq, "#OVT-Resource_BadRequest");
			return;
		}

		if (lineCount != order.LineCount() || order.LineCount() == 0)
		{
			SendResourceError(playerId, seq, "#OVT-Resource_BadRequest");
			return;
		}

		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
		{
			SendResourceError(playerId, seq, "#OVT-Resource_NoCatalogue");
			return;
		}

		OVT_ResourceDefs defs = resources.GetDefs();
		if (!defs || defs.Count() == 0)
		{
			SendResourceError(playerId, seq, "#OVT-Resource_NoCatalogue");
			return;
		}

		// Re-read BOTH holders. Everything the checkout remembers is what the client said at Begin;
		// the world may have moved since, and a holder that died mid-stream must refuse here.
		IEntity source = null;
		IEntity dest = null;
		OVT_ResourceStoreComponent sourceStore = null;
		OVT_ResourceStoreComponent destStore = null;

		string rejectKey;

		if (OpReadsSource(order.m_iOp))
		{
			source = ResolveEntity(order.m_SourceId);
			if (!MayUseHolder(playerId, source, rejectKey))
			{
				SendResourceError(playerId, seq, rejectKey);
				return;
			}

			sourceStore = OVT_ResourceUtils.GetStore(source);
			if (!sourceStore || !sourceStore.GetLedger())
			{
				SendResourceError(playerId, seq, "#OVT-Resource_NoStore");
				return;
			}
		}

		if (OpReadsDest(order.m_iOp))
		{
			dest = ResolveEntity(order.m_DestId);
			if (!MayUseHolder(playerId, dest, rejectKey))
			{
				SendResourceError(playerId, seq, rejectKey);
				return;
			}

			destStore = OVT_ResourceUtils.GetStore(dest);
			if (!destStore || !destStore.GetLedger())
			{
				SendResourceError(playerId, seq, "#OVT-Resource_NoStore");
				return;
			}
		}

		// The ground drop's position is derived BEFORE anything moves: a drop with nowhere to land must
		// refuse, not take the load off the truck and then discover it.
		vector dropPos = vector.Zero;
		if (order.m_iOp == EOVT_ResourceOp.HOLDER_TO_GROUND)
		{
			if (resources.GetPilePrefab() == "")
			{
				SendResourceError(playerId, seq, "#OVT-Resource_NoPilePrefab");
				return;
			}

			if (!ResolveDropPosition(playerId, source, dropPos))
			{
				SendResourceError(playerId, seq, "#OVT-Resource_NoPosition");
				return;
			}
		}

		bool isPort = order.m_iOp == EOVT_ResourceOp.PORT_IMPORT || order.m_iOp == EOVT_ResourceOp.PORT_EXPORT;

		OVT_EconomyManagerComponent economy = null;
		string persId = "";
		bool hasIllegalPermission = false;
		bool resistanceHoldsPort = false;

		if (isPort)
		{
			IEntity portHolder = source;
			if (order.m_iOp == EOVT_ResourceOp.PORT_IMPORT)
				portHolder = dest;

			economy = OVT_Global.GetEconomy();
			OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
			if (!economy || !players)
			{
				SendResourceError(playerId, seq, "#OVT-Resource_Failed");
				return;
			}

			if (!AtAPort(playerId, portHolder))
			{
				SendResourceError(playerId, seq, "#OVT-Resource_NotAtPort");
				return;
			}

			persId = players.GetPersistentIDFromPlayerID(playerId);

			OVT_PlayerData player = players.GetPlayer(persId);
			if (player && player.HasPermission("IllegalImports"))
				hasIllegalPermission = true;

			resistanceHoldsPort = economy.ResistanceControlsNearestPort(portHolder.GetOrigin());
		}

		// Re-derive every line. NOT ONE UNIT MOVES IN THIS LOOP.
		int totalLitres = 0;
		int moneyTotal = 0;
		int earnTotal = 0;

		foreach (OVT_ResourceCartLine line : order.m_aLines)
		{
			if (line.m_iResIndex < 0 || line.m_iResIndex >= defs.Count())
			{
				SendResourceError(playerId, seq, "#OVT-Resource_BadRequest");
				return;
			}

			if (line.m_iQuantity <= 0 || line.m_iQuantity > MAX_LINE_QUANTITY)
			{
				SendResourceError(playerId, seq, "#OVT-Resource_BadRequest");
				return;
			}

			string id = defs.IdAt(line.m_iResIndex);
			if (id == "")
			{
				SendResourceError(playerId, seq, "#OVT-Resource_BadRequest");
				return;
			}

			totalLitres = totalLitres + (defs.LitresAt(line.m_iResIndex) * line.m_iQuantity);

			// Tripwire, not arithmetic: a negative running total can only mean the per-line bound above
			// was widened past what an int holds, and a negative total passes every test below it.
			if (totalLitres < 0)
			{
				SendResourceError(playerId, seq, "#OVT-Resource_BadRequest");
				return;
			}

			if (sourceStore && sourceStore.GetLedger().Count(id) < line.m_iQuantity)
			{
				SendResourceError(playerId, seq, "#OVT-Resource_NotEnough");
				return;
			}

			if (order.m_iOp == EOVT_ResourceOp.PORT_IMPORT)
			{
				if (!OVT_ResourceRules.MayImport(defs, line.m_iResIndex))
				{
					SendResourceError(playerId, seq, "#OVT-Resource_NotImportable");
					return;
				}

				if (!OVT_ResourceRules.IllegalGateOpen(defs, line.m_iResIndex, hasIllegalPermission, resistanceHoldsPort))
				{
					SendResourceError(playerId, seq, "#OVT-Resource_Illegal");
					return;
				}

				moneyTotal = moneyTotal + (resources.GetPrice(line.m_iResIndex) * line.m_iQuantity);

				// Same tripwire as the litre total: PlayerHasMoney() accepts a negative amount, and
				// TakePlayerMoney() of a negative amount PAYS the player.
				if (moneyTotal < 0)
				{
					SendResourceError(playerId, seq, "#OVT-Resource_BadRequest");
					return;
				}
			}

			if (order.m_iOp == EOVT_ResourceOp.PORT_EXPORT)
			{
				if (!OVT_ResourceRules.MayExport(defs, line.m_iResIndex))
				{
					SendResourceError(playerId, seq, "#OVT-Resource_NotSellable");
					return;
				}

				if (!OVT_ResourceRules.IllegalGateOpen(defs, line.m_iResIndex, hasIllegalPermission, resistanceHoldsPort))
				{
					SendResourceError(playerId, seq, "#OVT-Resource_Illegal");
					return;
				}

				earnTotal = earnTotal + (resources.GetSellPrice(line.m_iResIndex) * line.m_iQuantity);

				if (earnTotal < 0)
				{
					SendResourceError(playerId, seq, "#OVT-Resource_BadRequest");
					return;
				}
			}
		}

		// ----------------------------------------------------------------------------------------
		// THE ALL-OR-NOTHING BRANCH. The cart is measured whole and paid for whole, or it is refused
		// whole. There is no partial acceptance and no clamp anywhere below.
		// ----------------------------------------------------------------------------------------
		if (destStore && destStore.GetFreeLitres() < totalLitres)
		{
			SendResourceError(playerId, seq, "#OVT-Resource_NoSpace");
			return;
		}

		if (order.m_iOp == EOVT_ResourceOp.PORT_IMPORT && !economy.PlayerHasMoney(persId, moneyTotal))
		{
			SendResourceError(playerId, seq, "#OVT-Resource_NoMoney");
			return;
		}

		// ================= NOTHING ABOVE THIS LINE HAS MUTATED ANYTHING =================
		// The ground drop below is the last step that may still refuse, and it refuses having mutated
		// nothing: SpawnOrMergePile answers null only on paths taken BEFORE it adds anything.

		// The pile is created and filled FIRST. A failed spawn then costs the player nothing, and the
		// Take below cannot fail because availability was proven above and nothing here yields.
		if (order.m_iOp == EOVT_ResourceOp.HOLDER_TO_GROUND)
		{
			array<ref OVT_ResourceAmount> dropped = new array<ref OVT_ResourceAmount>();

			foreach (OVT_ResourceCartLine line : order.m_aLines)
			{
				OVT_ResourceAmount amount = new OVT_ResourceAmount();
				amount.m_sId = defs.IdAt(line.m_iResIndex);
				amount.m_iQuantity = line.m_iQuantity;

				dropped.Insert(amount);
			}

			if (!resources.SpawnOrMergePile(dropPos, dropped))
			{
				SendResourceError(playerId, seq, "#OVT-Resource_DropFailed");
				return;
			}
		}

		int movedLitres = 0;

		foreach (OVT_ResourceCartLine line : order.m_aLines)
		{
			string id = defs.IdAt(line.m_iResIndex);
			int units = line.m_iQuantity;

			if (sourceStore)
				units = sourceStore.GetLedger().Take(id, line.m_iQuantity);

			if (destStore)
			{
				int fitted = destStore.GetLedger().Add(id, units, defs, destStore.GetCapacityLitres());
				if (fitted < units)
				{
					Print(string.Format("[Overthrow] OVT_ResourceRequestComponent moved %1 of '%2' into a holder that took only %3 after the whole-cart fit check passed. The load has been returned to the source.", units.ToString(), id, fitted.ToString()), LogLevel.ERROR);

					if (sourceStore)
						sourceStore.GetLedger().Add(id, units - fitted, defs, sourceStore.GetCapacityLitres());

					units = fitted;
				}
			}

			// P1-b: OVT_ResourceLedger.Add returns UNITS, not litres. The litre figure the reply carries
			// is derived here from the catalogue, never from a ledger return value.
			movedLitres = movedLitres + (units * defs.LitresAt(line.m_iResIndex));
		}

		int earned = 0;
		int spent = 0;

		if (order.m_iOp == EOVT_ResourceOp.PORT_IMPORT)
		{
			economy.TakePlayerMoney(playerId, moneyTotal);
			spent = moneyTotal;
		}

		if (order.m_iOp == EOVT_ResourceOp.PORT_EXPORT)
		{
			economy.DoAddPlayerMoney(playerId, earnTotal);
			earned = earnTotal;
		}

		// A pile drained to nothing is untracked and deleted in the same request that emptied it, so it
		// is never published as an empty crate stack and never saved as one.
		if (sourceStore && !resources.DeletePileIfEmpty(source))
			sourceStore.PublishContents();

		if (destStore)
			destStore.PublishContents();

		SendTransferResult(playerId, seq, movedLitres, earned, spent);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: consume the piles around a construction site and complete it.
	//!
	//! The identity, ruin and distance gates above are the same four this ask has always had; only the
	//! terminal branch changed when construction sites arrived. An RplId that names anything without an
	//! OVT_ConstructionSiteComponent still answers #OVT-Resource_NoSite, which is also what a site
	//! demolished between the click and the ask produces.
	//! \param[in] site The construction site's RplId.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_BuildFromSite(RplId site)
	{
		// NOT A REFUSAL - see RpcAsk_TransferBegin.
		if (!Replication.IsServer())
			return;

		int playerId = ResolveOwningPlayerId();
		if (playerId <= 0)
		{
			SendResourceError(playerId, SEQ_NONE, "#OVT-Resource_NoPlayer");
			return;
		}

		IEntity siteEntity = ResolveEntity(site);
		if (!siteEntity)
		{
			SendResourceError(playerId, SEQ_NONE, "#OVT-Resource_NoSite");
			return;
		}

		if (!OVT_StructureDamage.IsUsable(siteEntity))
		{
			SendResourceError(playerId, SEQ_NONE, "#OVT-Resource_Ruined");
			return;
		}

		if (!CallerIsWithin(playerId, siteEntity.GetOrigin(), m_fUseRadius))
		{
			SendResourceError(playerId, SEQ_NONE, "#OVT-Resource_TooFar");
			return;
		}

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance)
		{
			SendResourceError(playerId, SEQ_NONE, "#OVT-Resource_Failed");
			return;
		}

		string reason;
		if (!resistance.CompleteSite(siteEntity, playerId, reason))
			SendResourceError(playerId, SEQ_NONE, reason);
	}

	//-----------------------------------------------------------------------------------------------
	// SERVER -> OWNER
	//
	// ⚠ EVERY SENDER BELOW TAKES THE ShouldRespondLocally() DIRECT BRANCH FIRST. The engine never
	// loops an Rpc back to the sender, so an RplRcver.Owner reply a listen host sends to its OWN
	// controller is silently dropped and the host sees nothing at all.
	//
	// ⚠ THE Rpc() LINE SITS DIRECTLY UNDER A COMPILER-CHECKED CALL TO THE SAME HANDLER WITH THE SAME
	// ARGUMENTS. That two-line diff is the only reading that catches BUG-090; do not hoist either half
	// into a shared helper.
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: refuse a request. THE ONLY REFUSAL CHANNEL - no ask returns silently on a refusal path.
	//! \param[in] playerId The player the answer is aimed at.
	//! \param[in] seq The refused checkout's sequence, or SEQ_NONE for the action path.
	//! \param[in] messageKey Localization key naming the refusal.
	protected void SendResourceError(int playerId, int seq, string messageKey)
	{
		if (ShouldRespondLocally(playerId))
		{
			RpcDo_ResourceError(seq, messageKey);
			return;
		}

		Rpc(RpcDo_ResourceError, seq, messageKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: report a finished checkout.
	//! \param[in] playerId The player the answer is aimed at.
	//! \param[in] seq The checkout's sequence.
	//! \param[in] movedLitres Litres that actually moved, derived from the catalogue.
	//! \param[in] earned Money paid to the player by a port export; 0 otherwise.
	//! \param[in] spent Money charged for a port import; 0 otherwise.
	protected void SendTransferResult(int playerId, int seq, int movedLitres, int earned, int spent)
	{
		if (ShouldRespondLocally(playerId))
		{
			RpcDo_TransferResult(seq, movedLitres, earned, spent);
			return;
		}

		Rpc(RpcDo_TransferResult, seq, movedLitres, earned, spent);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a checkout finished.
	//! \param[in] seq Sequence id; anything but the live checkout is dropped.
	//! \param[in] movedLitres Litres that moved.
	//! \param[in] earned Money paid to the player.
	//! \param[in] spent Money charged to the player.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_TransferResult(int seq, int movedLitres, int earned, int spent)
	{
		if (seq != m_iSeq)
			return;

		m_bAwaitingResult = false;

		if (m_OnTransferResult)
			m_OnTransferResult.Invoke(movedLitres, earned, spent);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a request was refused.
	//!
	//! SEQ_NONE BELONGS TO NO SCREEN - it is the action path, and its refusals go to the hint the way
	//! every other user action in the mod reports one.
	//! \param[in] seq The refused checkout's sequence, or SEQ_NONE.
	//! \param[in] messageKey Localization key naming the refusal.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_ResourceError(int seq, string messageKey)
	{
		if (seq == SEQ_NONE)
		{
			ShowRefusalHint(messageKey);
			return;
		}

		if (seq != m_iSeq)
			return;

		m_bAwaitingResult = false;

		if (m_OnResourceError)
			m_OnResourceError.Invoke(messageKey);
	}

	//-----------------------------------------------------------------------------------------------
	// THE SERVER GATE
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether a player may act on a holder at all. THE SINGLE GATE - every ask goes through it, and
	//! the ladder below is in the order the plan fixes it.
	//! \param[in] playerId The caller, resolved from the entity the RPC arrived on.
	//! \param[in] holder The holder, already resolved from its RplId.
	//! \param[out] rejectKey Localization key naming the refusal; "" when the gate passes.
	//! \return True when the caller may act on this holder.
	protected bool MayUseHolder(int playerId, IEntity holder, out string rejectKey)
	{
		rejectKey = "";

		if (playerId <= 0)
		{
			rejectKey = "#OVT-Resource_NoPlayer";
			return false;
		}

		if (!holder || !OVT_ResourceUtils.GetStore(holder))
		{
			rejectKey = "#OVT-Resource_NoStore";
			return false;
		}

		if (!OVT_StructureDamage.IsUsable(holder))
		{
			rejectKey = "#OVT-Resource_Ruined";
			return false;
		}

		if (!CallerIsWithin(playerId, holder.GetOrigin(), m_fUseRadius))
		{
			rejectKey = "#OVT-Resource_TooFar";
			return false;
		}

		// One call covers BOTH a locked vehicle and any holder carrying an OVT_PlayerOwnerComponent.
		if (!OVT_ControllerRequestComponent.PlayerMayUseVehicleFor(playerId, holder))
		{
			rejectKey = "#OVT-Resource_Locked";
			return false;
		}

		if (!WarehouseIsAccessible(playerId, holder))
		{
			rejectKey = "#OVT-Resource_NoAccess";
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Runs MayUseHolder over exactly the holders an op reads.
	//! \param[in] playerId The caller.
	//! \param[in] source The source holder id.
	//! \param[in] dest The destination holder id.
	//! \param[in] opKind An EOVT_ResourceOp value.
	//! \param[out] rejectKey Localization key naming the refusal; "" when both pass.
	//! \return True when every holder this op reads is usable.
	protected bool HoldersUsable(int playerId, RplId source, RplId dest, int opKind, out string rejectKey)
	{
		rejectKey = "";

		if (OpReadsSource(opKind) && !MayUseHolder(playerId, ResolveEntity(source), rejectKey))
			return false;

		if (OpReadsDest(opKind) && !MayUseHolder(playerId, ResolveEntity(dest), rejectKey))
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a real-estate warehouse building is open to a player. Anything that is not a warehouse
	//! passes - PlayerMayUseWarehouse answers true for a building whose config is absent or not flagged
	//! m_IsWarehouse, so the flag test lives in exactly one place.
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
	//! Whether the caller's controlled character is within a radius of a position. A caller with no
	//! character fails, which is correct: every verb here is performed by a body standing at a holder.
	//!
	//! A LOCAL COPY of OVT_StorageRequestComponent.CallerIsWithin (:2803) on purpose. Lifting it to
	//! OVT_ControllerRequestComponent would edit a walled storage file; the duplication is recorded
	//! tech debt, not an oversight.
	//! \param[in] playerId The caller.
	//! \param[in] pos The position to test against.
	//! \param[in] maxDistance The radius in metres.
	//! \return True when the caller has a character and it is close enough.
	protected bool CallerIsWithin(int playerId, vector pos, float maxDistance)
	{
		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!character)
			return false;

		return vector.Distance(pos, character.GetOrigin()) <= maxDistance;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether both the caller and the holder are standing at a port. Same two-ended rule the shipped
	//! port import and storage export enforce, so a sale is never accepted where a purchase at the
	//! same spot would be refused.
	//! \param[in] playerId The caller.
	//! \param[in] holder The holder being traded through.
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
		if (callerDistance < 0 || callerDistance > m_fPortRadius)
			return false;

		float holderDistance = economy.DistanceToNearestPort(holder.GetOrigin());
		if (holderDistance < 0 || holderDistance > m_fPortRadius)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Where a ground drop lands. Unloading a VEHICLE drops clear of its tail, measured from the hull's
	//! own aft extent so a long truck and a short one both clear it; anything else drops in front of the
	//! caller's body. Keying a vehicle drop off the character put the pile through the cab, because a
	//! player at the driver's door faces the truck. Height is snapped to terrain by the manager.
	//! \param[in] playerId The caller.
	//! \param[in] sourceHolder The holder being drained, or null.
	//! \param[out] pos Receives the drop position; untouched when no position could be derived.
	//! \return True when a position was derived.
	protected bool ResolveDropPosition(int playerId, IEntity sourceHolder, out vector pos)
	{
		vector mat[4];

		Vehicle vehicle = Vehicle.Cast(sourceHolder);
		if (vehicle)
		{
			vehicle.GetWorldTransform(mat);

			vector mins, maxs;
			vehicle.GetBounds(mins, maxs);

			// mat[2] is forward, so the hull's aft face sits at mins[2] (negative) along it.
			pos = mat[3] + (mat[2] * (mins[2] - m_fUnloadOffset));
			return true;
		}

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!character)
			return false;

		character.GetWorldTransform(mat);

		// mat[2] is the forward axis, mat[3] the position.
		pos = mat[3] + (mat[2] * m_fUnloadOffset);

		return true;
	}

	//-----------------------------------------------------------------------------------------------
	// OP SHAPE
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \param[in] opKind The value the client sent.
	//! \return True when it names a member of EOVT_ResourceOp. One line per member, so appending one
	//! is a visible edit here rather than a silent range widening.
	protected bool IsKnownOp(int opKind)
	{
		if (opKind == EOVT_ResourceOp.HOLDER_TO_HOLDER)
			return true;

		if (opKind == EOVT_ResourceOp.HOLDER_TO_GROUND)
			return true;

		if (opKind == EOVT_ResourceOp.PORT_IMPORT)
			return true;

		if (opKind == EOVT_ResourceOp.PORT_EXPORT)
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] opKind An EOVT_ResourceOp value.
	//! \return True when the op draws from the holder in the SOURCE slot.
	protected bool OpReadsSource(int opKind)
	{
		return opKind != EOVT_ResourceOp.PORT_IMPORT;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] opKind An EOVT_ResourceOp value.
	//! \return True when the op fills the holder in the DEST slot. HOLDER_TO_GROUND fills a pile that
	//! does not exist yet, and PORT_EXPORT fills nothing.
	protected bool OpReadsDest(int opKind)
	{
		if (opKind == EOVT_ResourceOp.HOLDER_TO_HOLDER)
			return true;

		return opKind == EOVT_ResourceOp.PORT_IMPORT;
	}

	//-----------------------------------------------------------------------------------------------
	// CLIENT STATE
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
	//! Client: draw a refusal that belongs to no screen.
	//! \param[in] messageKey Localization key naming the refusal.
	protected void ShowRefusalHint(string messageKey)
	{
		if (messageKey.IsEmpty())
			return;

		SCR_HintManagerComponent hints = SCR_HintManagerComponent.GetInstance();
		if (hints)
			hints.ShowCustom(messageKey);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True between an opened checkout and its single reply.
	bool IsAwaitingResult()
	{
		return m_bAwaitingResult;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The live checkout sequence, or SEQ_NONE before the first one.
	int GetSeq()
	{
		return m_iSeq;
	}

	//------------------------------------------------------------------------------------------------
	//! Fires when a checkout finishes: (int movedLitres, int earned, int spent).
	//! \return The invoker, allocated on first ask.
	ScriptInvoker GetOnTransferResult()
	{
		if (!m_OnTransferResult)
			m_OnTransferResult = new ScriptInvoker();

		return m_OnTransferResult;
	}

	//------------------------------------------------------------------------------------------------
	//! Fires when a request is refused: (string messageKey).
	//! \return The invoker, allocated on first ask.
	ScriptInvoker GetOnResourceError()
	{
		if (!m_OnResourceError)
			m_OnResourceError = new ScriptInvoker();

		return m_OnResourceError;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Metres the destination picker reaches for other holders.
	float GetHolderRadius()
	{
		return m_fHolderRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Metres the caller may stand from a holder and still use it.
	float GetUseRadius()
	{
		return m_fUseRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Most lines one checkout may carry.
	int GetMaxCartLines()
	{
		return m_iMaxCartLines;
	}
}
