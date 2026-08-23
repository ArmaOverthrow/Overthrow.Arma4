[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Client->server relay for recruit squad commands")]
class OVT_RecruitCommandComponentClass : OVT_ComponentClass {};

//------------------------------------------------------------------------------------------------
//! Everything the server worked out about one equipped-recruit purchase request, in one object.
//!
//! WHY A HOLDER AND NOT A PILE OF out PARAMETERS. The quote and the purchase run the SAME validation
//! - that is the point of it, and it is what makes the price a player is shown the price they are
//! charged - so the validation has to hand back the tent, the loadout, the persistent id, the two
//! halves of the price and the refusal code together. As out parameters that is seven, in an order
//! nobody would remember; as one object it is a value the caller reads by name.
//!
//! ! IT IS SERVER-SIDE AND SHORT-LIVED. It holds two weak pointers - an entity and a loadout, both
//! owned by something that outlives the handler - and it is built, read and dropped inside a single
//! synchronous RPC handler. Nothing caches one; a stored quote would be a price that can go stale.
//!
//! m_iRefusalCode is the first thing to read. Anything other than OVT_RecruitCommandComponent's
//! RESULT_OK means the rest of the object is partial, and the price in it (when there is one) is for
//! DISPLAY ONLY - a refused quote still carries its number so the picker can show a price next to
//! the reason it cannot be bought.
//------------------------------------------------------------------------------------------------
class OVT_EquippedRecruitQuote : Managed
{
	//! RESULT_OK, or the first rule that refused.
	int m_iRefusalCode;

	//! The tent's root entity. Weak: the world owns it.
	IEntity m_Tent;

	//! Where the tent is. Read once, so every later step uses the same position.
	vector m_vTentPosition;

	//! The buyer's persistent id, resolved server-side from the controller entity.
	string m_sPersistentId;

	//! The loadout being bought. Weak: the loadout manager owns it.
	OVT_PlayerLoadout m_Loadout;

	//! What the plain Recruit Civilian action would charge for the recruit alone.
	int m_iRecruitCost;

	//! The kit at local shop buy price, before the fee multiplier.
	int m_iGearSubtotal;

	//! How many resources went into that subtotal, nesting and attachments included.
	int m_iItemCount;

	//! How many entries the loadout's own item array has - the denominator the apply grades against.
	int m_iTopLevelCount;

	//! The whole quoted price: recruit cost plus the gear fee.
	int m_iTotalPrice;

	//! Value of the gear covered by nearby warehouse stock - the tent half of the shipped
	//! "//To-do: factoring in warehouse" (implementation.md §3.8).
	int m_iCoveredValue;

	//! Per-resource manifest this quote was split against. Held so a successful purchase can consume
	//! exactly what it was quoted, not a stock figure re-read a round trip later.
	ref array<ref OVT_ItemSourcingLine> m_aManifest;

	//! The warehouses collected for this quote, in query order.
	ref array<OVT_StorageComponent> m_aStores;

	//! The item with no catalog price, when that is why this was refused.
	string m_sUnpriceableResource;
}

//------------------------------------------------------------------------------------------------
//! Client->server relay for recruit squad commands, on the per-player OVT_OverthrowController
//! entity (project rule: no new client->server RPCs on OVT_PlayerCommsComponent, which is
//! deprecated).
//!
//! WHAT LIVES HERE AND WHAT DOES NOT. This component decides NOTHING about recruits. It checks the
//! shape of a request, resolves WHO is asking from the controller entity it sits on, re-checks that
//! the asker owns the recruit, and hands off to OVT_RecruitManagerComponent.SetRecruitInactive() -
//! the one server entry point for that state, which validates existence, liveness and
//! "is this even a change" for itself. This component never writes m_bInactive, never touches a
//! group and NEVER broadcasts: SetRecruitInactive() already broadcasts to every client through
//! RpcDo_RecruitActiveStateChanged, and a second broadcast from here would be a second truth.
//!
//! OWNERSHIP IS RE-CHECKED HERE BECAUSE ONLY HERE KNOWS WHO IS ASKING. SetRecruitInactive() is also
//! driven by paths where nobody is asking (respawn, persistence re-apply), so it deliberately does
//! not check ownership. The check therefore belongs at the seam a client can reach, which is this
//! one - and the identity it checks against comes from ResolveOwningPlayerId(), never from the
//! payload. The only client-supplied value is a recruit id, and an id the sender does not own is
//! refused.
//!
//! ! Rpc()'s prototype is untyped variadic, so a wrong argument count compiles clean and dies
//! silently at the wire (BUG-090). compile-check.sh cannot see it and no test in this project can:
//! the autotest world has no second machine, so no RPC in it ever crosses a wire. The practical
//! defence is to have as few signatures as possible and to read every call against its handler by
//! hand. There are exactly NINE RPCs here and exactly nine Rpc() call sites, one per RPC:
//!   Rpc(RpcAsk_SetRecruitInactive, recruitId, inactive)          -> (string, bool)      2 args
//!   Rpc(RpcDo_RecruitCommandResult, resultCode)                  -> (int)               1 arg
//!   Rpc(RpcDo_RecruitStatus, recruitId, position, statusFlags)   -> (string, vector, int) 3 args
//!   Rpc(RpcAsk_SwapLoadout, recruitEntityRplId)                  -> (RplId)             1 arg
//!   Rpc(RpcDo_SwapLoadoutResult, code, exchanged, misplaced,
//!       failed, dropped)                                         -> (int x5)            5 args
//!   Rpc(RpcAsk_QuoteEquippedRecruit, tentRplId, loadoutName)     -> (RplId, string)     2 args
//!   Rpc(RpcDo_RecruitQuote, loadoutName, price, itemCount,
//!       refusalCode, unpriceableResource, coveredValue)           -> (string, int, int,
//!                                                                    int, string, int)  6 args
//!   Rpc(RpcAsk_BuyEquippedRecruit, tentRplId, loadoutName)       -> (RplId, string)     2 args
//!   Rpc(RpcDo_EquippedRecruitResult, code, charged, applied,
//!       total)                                                   -> (int x4)            4 args
//! If that list and the handler signatures below ever stop matching, the symptom is silence.
//!
//! THE EQUIPPED-RECRUIT PURCHASE IS THE ONE REQUEST THAT MOVES MONEY, and it is the reason four of
//! those nine exist. A player standing at a recruitment tent picks one of their own saved loadouts
//! and buys a recruit already wearing it. Everything about that transaction happens HERE, on the
//! server, in one handler:
//!
//!   - THE CLIENT NEVER COMPUTES A PRICE, and cannot: loadout JIP replicates metadata only, so every
//!     client's copy of a loadout has an empty item array (OVT_LoadoutManagerComponent.c:40). The
//!     server quotes, the client displays the number it was handed, and the quote and the charge come
//!     out of the same routine on the same machine - so they cannot drift (decisions D18/D19).
//!   - THE QUOTE AND THE PURCHASE RUN THE SAME VALIDATION, ValidateTentPurchase(), which is why the
//!     picker can never offer something the server would then refuse.
//!   - THE APPLY TARGET IS NEVER NAMED BY A CLIENT. ApplyLoadoutToEntity() CLEARS the target first
//!     (OVT_LoadoutManagerComponent.c:450, :1278-1303 delete every item on it), so pointed at an
//!     existing recruit it would destroy gear the player paid for. It is only ever pointed at the
//!     body this handler spawned two lines earlier, which is a local variable no payload can reach
//!     (risk R13).
//!   - NOTHING SPAWNS BEFORE THE FUNDS CHECK PASSES, and the charge is taken by a direct server-side
//!     call - never through OVT_Global.GetServer(), which would be an Rpc() out of a server handler
//!     and simply be dropped (BUG-161).
//!
//! The purchase re-opens a network route to the loadout engine's SPAWNING apply, which was once
//! deleted as a free-item hole (BUG-043). Five independent gates replace it: identity from
//! ResolveOwningPlayerId(); the loadout fetched by that RESOLVED persistent id, so another player's
//! kit is unreachable; the apply target derived, never named; the price computed server-side from the
//! same record that is applied; and the transaction costing a town supporter and a slot against the
//! 16-recruit cap, so the campaign itself rate-limits it.
//!
//! ! LISTEN SERVER. The authority never loops an RplRcver.Server RPC back to itself, and a listen
//! host never receives its own RplRcver.Owner RPCs (BUG-164). Both directions therefore short-
//! circuit to a direct call when this machine is the target, which is why a single-player host can
//! use this at all.
//!
//! THE STATUS CACHE IS THE ONE PIECE OF STATE THIS COMPONENT OWNS, and it is client-side only:
//! flags measured on the server (OVT_RecruitManagerComponent.ReadRecruitStatus) arrive here every
//! ten seconds for the recruits of THIS controller's owner, and the map layer and roster read them
//! back through GetStatusFlags(). It is a cache of something else's truth, never a truth: a missing
//! entry answers 0, which every consumer renders as "no tag", and the next sweep fills it in.
//!
//! THE LOADOUT SWAP IS THE ONE REQUEST THAT NAMES AN ENTITY rather than a record. It has to: the kit
//! being exchanged belongs to a BODY, not to a row in a table, and the action that starts it is
//! standing in front of that body. The name it uses is an RplId, which is the only entity reference
//! that means the same thing on both machines - and it is treated as hostile: an RplId that resolves
//! to nothing, to the requester themselves, or to a character that is not one of their recruits is
//! refused, and the SECOND half of the swap (the requester's own character) is never sent at all,
//! only derived server-side from the identity this controller resolved.
//------------------------------------------------------------------------------------------------
class OVT_RecruitCommandComponent : OVT_Component
{
	//! How far the player may be from the recruit when the swap hold completes, in metres.
	//!
	//! The action's own UserActionContext radius is 2 m, so this is that plus slack for the recruit
	//! taking a step during the five-second hold. Same shape and the same reason as
	//! OVT_TowerSabotageComponent.SABOTAGE_MAX_DISTANCE - the point is not to be exact, it is to make
	//! a forged request from across the map fail.
	static const float SWAP_MAX_DISTANCE = 5;

	//! The request was carried out. Deliberately silent on the client - the recruit visibly changes
	//! what it is doing, which says it better than a hint would.
	static const int RESULT_OK = 0;

	//! No record with that id exists on the server. Not reachable from Overthrow's own UI.
	static const int RESULT_NO_RECRUIT = 1;

	//! The recruit exists but belongs to somebody else. The one code that means a rule stopped an
	//! abuse rather than a state being wrong.
	static const int RESULT_NOT_OWNER = 2;

	//! The recruit has no body in the world, so there is nothing to park or unpark. This is the
	//! roster's disabled-button condition, re-checked here because a body can vanish between the
	//! client deciding and the server answering.
	static const int RESULT_NO_BODY = 3;

	//! Refused: the body is sitting in a vehicle compartment. Parking moves the recruit between AI
	//! groups but does NOT move the body, so a recruit parked in a seat would be "holding position"
	//! inside a vehicle that can then drive away with it.
	static const int RESULT_IN_VEHICLE = 4;

	//! The recruit is already in the requested state. A no-op, not a failure - re-running a
	//! deactivation would build the recruit a second inactive group.
	static const int RESULT_NO_CHANGE = 5;

	//! The manager refused or could not complete the transition; it has already logged why.
	static const int RESULT_FAILED = 6;

	// ---------------------------------------------------------------------------------------------
	// LOADOUT SWAP CODES. Appended, never renumbered and never reused - a client built against an
	// older numbering would mis-read every hint, and these values only exist to pick a sentence.
	// ---------------------------------------------------------------------------------------------

	//! Every item changed hands and landed in its matching slot.
	static const int RESULT_SWAP_OK = 7;

	//! Some of the kit changed hands and some did not. The counts that come with this code say how
	//! much of each, because "partial" on its own tells the player nothing they can act on.
	static const int RESULT_SWAP_PARTIAL = 8;

	//! The swap ran and moved nothing at all, or could not run against those two characters.
	static const int RESULT_SWAP_FAILED = 9;

	//! Refused: the recruit is parked. A swap needs a recruit that is following you - a parked one is
	//! by definition somewhere you left it, and its kit is what makes it able to hold that position.
	static const int RESULT_RECRUIT_INACTIVE = 10;

	//! Refused: the requester is too far from the recruit. Only reachable by a forged request; the
	//! action cannot even be seen at that range.
	static const int RESULT_TOO_FAR = 11;

	//! Refused: the recruit is dead or unconscious. An unconscious body may be carrying an applied
	//! tourniquet or saline bag, and stripping someone who is bleeding out is not a kit swap.
	static const int RESULT_RECRUIT_DOWN = 12;

	//! Refused: the requesting player has no character in the world to swap WITH. Not reachable from
	//! Overthrow's own UI - you cannot hold an action without a body.
	static const int RESULT_NO_ACTOR = 13;

	// ---------------------------------------------------------------------------------------------
	// EQUIPPED TENT RECRUIT CODES. Appended, never renumbered and never reused, for the same reason
	// the swap codes are: these values only exist to pick a sentence, and a client built against an
	// older numbering would pick the wrong one for every outcome.
	//
	// Three of them CARRY COUNTS (14, 15, 23) and assemble their own sentences in
	// RpcDo_EquippedRecruitResult; ReasonKeyFor() is never asked about those three. The rest are
	// ordinary refusals with a key each.
	// ---------------------------------------------------------------------------------------------

	//! Recruit bought and the whole kit arrived.
	static const int RESULT_BUY_OK = 14;

	//! Recruit bought, but some of the kit did not arrive. The counts in the reply say how much.
	static const int RESULT_BUY_PARTIAL = 15;

	//! Refused: the named entity is not a recruitment tent, or the buyer is not standing at it.
	static const int RESULT_NO_TENT = 16;

	//! Refused: no loadout of that name belongs to the sender. Named loadouts are looked up under the
	//! persistent id THIS component resolved, so another player's kit answers here rather than being
	//! quoted.
	static const int RESULT_NO_LOADOUT = 17;

	//! Refused: the loadout records no items, so there is nothing to sell.
	static const int RESULT_LOADOUT_EMPTY = 18;

	//! Refused: something in the loadout has no catalog price. The reply names it. Never priced at
	//! zero and never guessed - see OVT_RecruitLoadoutPricing (decision D20).
	static const int RESULT_UNPRICEABLE = 19;

	//! Refused: the quote is more than the player has. Checked explicitly and BEFORE anything spawns,
	//! because the charge itself clamps at zero and can never report a shortfall.
	static const int RESULT_CANNOT_AFFORD = 20;

	//! Refused: the buyer is already at MAX_RECRUITS_PER_PLAYER.
	static const int RESULT_AT_CAP = 21;

	//! Refused: the nearest town has no supporter to give. An equipped recruit is a tent recruit that
	//! arrives dressed, so it costs a supporter like any other (decision D23).
	static const int RESULT_NO_SUPPORTERS = 22;

	//! The recruit spawned but NOT ONE PIECE of the kit arrived. Only the recruit cost is charged -
	//! the gear fee is not (decision D19).
	static const int RESULT_GEAR_FAILED = 23;

	//! Nothing spawned, so nothing was charged and no supporter was taken.
	static const int RESULT_SPAWN_FAILED = 24;

	//! The buildable type a recruitment tent declares (Prefabs/Structures/Military/FOB/
	//! OVT_RecruitmentTent.et:4-6). An RplId that resolves to anything else is refused.
	static const string RECRUITMENT_TENT_TYPE = "RecruitmentTent";

	//! CLIENT: the last status mask the server pushed for each of this owner's recruits, keyed by
	//! recruit id. Server-side this map stays empty on a dedicated server and is incidentally
	//! populated on a listen host (whose sends short-circuit into the handler); nothing reads it
	//! there, and nothing may start to - the server has the bodies and reads them directly.
	protected ref map<string, int> m_mStatusFlags;

	//! Whether m_OnRecruitRemoved has been subscribed to. The recruit manager can be absent when this
	//! component initialises (JIP order is not guaranteed), so the subscription is made on the first
	//! use that proves a manager exists, and this flag keeps that from happening twice.
	protected bool m_bSubscribedToRemoval;

	//! CLIENT: fires when a tent purchase quote arrives.
	//! Args: (string loadoutName, int price, int itemCount, int refusalCode, string unpriceableResource,
	//!         int coveredValue)
	//!
	//! For the picker: a quote is asked for when the highlighted loadout changes, and it arrives one
	//! round trip later. A screen that only polls would show the previous loadout's price for a frame
	//! or two; subscribing means the button and the price line update the moment the answer lands.
	//! The cached getters below exist for a screen that would rather poll.
	ref ScriptInvoker m_OnRecruitQuote = new ScriptInvoker();

	//! CLIENT: fires when a tent purchase completes or is refused.
	//! Args: (int resultCode, int charged, int applied, int total)
	ref ScriptInvoker m_OnEquippedRecruitResult = new ScriptInvoker();

	//! CLIENT: the loadout the last quote was for. Empty until one arrives.
	//!
	//! ! A CONSUMER MUST COMPARE THIS AGAINST THE LOADOUT IT IS SHOWING. Quotes are per-selection and
	//! answers can arrive out of order after a fast scroll, so a price is only this row's price if
	//! the name matches - which is why the name is carried in the reply at all.
	protected string m_sLastQuoteLoadout;

	//! CLIENT: the price the last quote reported. Meaningful only with m_iLastQuoteRefusal == RESULT_OK
	//! or RESULT_CANNOT_AFFORD; other refusals never got as far as computing one.
	protected int m_iLastQuotePrice;

	//! CLIENT: how many items the last quote priced.
	protected int m_iLastQuoteItemCount;

	//! CLIENT: RESULT_OK when the last quote may be bought, otherwise why not.
	protected int m_iLastQuoteRefusal = RESULT_NO_LOADOUT;

	//! CLIENT: the unpriceable item named by the last quote, when that was the refusal.
	protected string m_sLastQuoteUnpriceable;

	//! CLIENT: value of the gear the last quote reported as covered by nearby warehouse stock.
	protected int m_iLastQuoteCoveredValue;

	//------------------------------------------------------------------------------------------------
	//! \param src Component source.
	//! \param ent The controller entity this component is attached to.
	//! \param parent Parent entity, if any.
	void OVT_RecruitCommandComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_mStatusFlags = new map<string, int>();
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the invoker subscription with the component.
	//!
	//! This controller is deleted every time its player disconnects, and a ScriptInvoker holding a
	//! method pointer into a deleted component is the classic Overthrow leak - the manager outlives
	//! every controller in the session.
	//! \param owner The controller entity this component is attached to.
	override void OnDelete(IEntity owner)
	{
		if (m_bSubscribedToRemoval)
		{
			OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
			if (recruits && recruits.m_OnRecruitRemoved)
				recruits.m_OnRecruitRemoved.Remove(OnRecruitRemoved);

			m_bSubscribedToRemoval = false;
		}

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Ask the server to park this recruit (inactive) or bring it back into the squad (active).
	//!
	//! The client names a recruit and a desired state and nothing else. It does not name itself
	//! (identity comes from the controller entity this component sits on) and it does not describe
	//! the recruit - everything else is re-derived server-side.
	//! \param[in] recruitId The recruit to change.
	//! \param[in] inactive True to park it, false to bring it back.
	void RequestSetInactive(string recruitId, bool inactive)
	{
		if(Replication.IsServer())
		{
			RpcAsk_SetRecruitInactive(recruitId, inactive);
		}else{
			Rpc(RpcAsk_SetRecruitInactive, recruitId, inactive);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: validate the request, resolve who is asking, hand off, and report - in that order.
	//!
	//!  1. we are the server
	//!  2. the request has a recruit id at all
	//!  3. the requesting player is resolved from THIS controller entity, never from the payload
	//!  4. the recruit exists AND is owned by the sender - the anti-spoofing check
	//!  5. it has a body, and that body is not sitting in a vehicle
	//!  6. it is actually a change (the manager refuses a no-op too, but it cannot tell the client
	//!     apart from a real failure, and those two want different hints)
	//!  7. the manager performs the transition and broadcasts it; nothing is broadcast from here
	//!  8. every outcome with an identified requester is reported, including OK
	//!
	//! Steps 1-3 return without reporting because they run BEFORE an identity exists - there is no
	//! client to report to yet.
	//! \param[in] recruitId The recruit to change.
	//! \param[in] inactive True to park it, false to bring it back.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetRecruitInactive(string recruitId, bool inactive)
	{
		// 1
		if(!Replication.IsServer()) return;

		// 2 - BUG-090 diagnostic: this line firing proves the request left the client, reached this
		// machine and was decoded with the right arity. If the hold completes and this never prints,
		// the component is missing from Prefabs/GameMode/OVT_OverthrowController.et or the Rpc()
		// arity is wrong; if it prints and nothing happens, a rule refused - see the result print.
		Print("[Overthrow] Recruit command received: setInactive=" + inactive.ToString() + " recruit=" + recruitId, LogLevel.NORMAL);

		if(recruitId.IsEmpty()) return;

		// 3
		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0)
		{
			Print("[Overthrow] Recruit command refused: could not resolve the requesting player", LogLevel.WARNING);
			return;
		}

		// 4
		SendResult(ValidateAndApply(playerId, recruitId, inactive));
	}

	//------------------------------------------------------------------------------------------------
	//! The rules, separated from the wire so the ordering above stays readable.
	//!
	//! Split out rather than inlined because every refusal has to answer with a CODE rather than
	//! returning early into silence: a client that gets no answer cannot tell "refused" from "the
	//! packet never arrived", and this whole path exists to make that distinction.
	//! \param[in] playerId Runtime id of the requesting player, resolved from the controller entity.
	//! \param[in] recruitId The recruit to change.
	//! \param[in] inactive The requested state.
	//! \return One of the RESULT_ codes.
	protected int ValidateAndApply(int playerId, string recruitId, bool inactive)
	{
		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if(!recruits) return RESULT_FAILED;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return RESULT_FAILED;

		OVT_RecruitData recruit = recruits.GetRecruit(recruitId);
		if(!recruit) return RESULT_NO_RECRUIT;

		// THE ANTI-SPOOFING CHECK. The persistent id is the server's own, looked up from the runtime
		// id this controller entity resolved to - the client never supplies either.
		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if(persId.IsEmpty()) return RESULT_FAILED;

		if(recruit.m_sOwnerPersistentId != persId)
		{
			Print("[Overthrow] Recruit command refused: player " + playerId.ToString() + " does not own recruit " + recruitId, LogLevel.WARNING);
			return RESULT_NOT_OWNER;
		}

		if(recruit.m_bInactive == inactive) return RESULT_NO_CHANGE;

		IEntity recruitEntity = recruits.FindRecruitEntity(recruitId);
		if(!recruitEntity) return RESULT_NO_BODY;

		// Only PARKING is refused in a vehicle. Deactivation changes which AI group commands the
		// body, not where the body is, so a recruit parked in a seat stays in that seat and its
		// hold-position waypoint is wherever the vehicle happened to be - and the vehicle can then
		// drive off with a garrison member inside it. Reactivation has no such problem: the recruit
		// rejoins its owner's group and the owner is normally in the same vehicle.
		if(inactive && IsInCompartment(recruitEntity)) return RESULT_IN_VEHICLE;

		// The manager owns the transition AND the broadcast. Nothing below this line touches state.
		if(!recruits.SetRecruitInactive(recruitId, inactive)) return RESULT_FAILED;

		return RESULT_OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a character is currently sitting in a vehicle compartment.
	//!
	//! Same shape as OVT_TravelRequestComponent.IsInCompartment - the engine-level
	//! CompartmentAccessComponent, not the SCR_ subclass, because a recruit body only needs the base
	//! query and the base type is the one every character is guaranteed to carry.
	//! \param[in] entity The character to test.
	//! \return True when the entity is in a compartment.
	protected bool IsInCompartment(IEntity entity)
	{
		if(!entity) return false;

		CompartmentAccessComponent compartmentAccess = CompartmentAccessComponent.Cast(entity.FindComponent(CompartmentAccessComponent));
		if(!compartmentAccess) return false;

		return compartmentAccess.IsInCompartment();
	}

	//------------------------------------------------------------------------------------------------
	//! Ask the server to exchange your entire kit with one of your recruits.
	//!
	//! The client names the recruit's BODY by RplId and nothing else. It does not name itself - the
	//! other half of the swap is the requester's own character, which the server derives from the
	//! identity this controller entity resolves to.
	//! \param[in] recruitEntityRplId The recruit body to swap with.
	void RequestSwapLoadout(RplId recruitEntityRplId)
	{
		if(Replication.IsServer())
		{
			RpcAsk_SwapLoadout(recruitEntityRplId);
		}else{
			Rpc(RpcAsk_SwapLoadout, recruitEntityRplId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: validate the request, resolve BOTH characters, swap, and report.
	//!
	//! The order is the same one RpcAsk_SetRecruitInactive uses and for the same reason: the steps
	//! that run before an identity exists cannot report, so they return silently; everything after
	//! answers with a code.
	//! \param[in] recruitEntityRplId The recruit body the client named.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SwapLoadout(RplId recruitEntityRplId)
	{
		if(!Replication.IsServer()) return;

		// BUG-090 diagnostic, same role as the one in RpcAsk_SetRecruitInactive: this line firing
		// proves the request left the client, arrived here and decoded with the right arity.
		Print("[Overthrow] Recruit loadout swap received", LogLevel.NORMAL);

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0)
		{
			Print("[Overthrow] Recruit loadout swap refused: could not resolve the requesting player", LogLevel.WARNING);
			return;
		}

		IEntity actor;
		IEntity recruitEntity;

		int refusal = ValidateSwapRequest(playerId, recruitEntityRplId, actor, recruitEntity);
		if(refusal != RESULT_OK)
		{
			SendSwapResult(refusal, 0, 0, 0, 0);
			return;
		}

		// From here on nothing this component knows is used again: the swap routine owns the whole
		// operation, and it can only move entities that already exist (decision D13).
		OVT_LoadoutSwapResult result = OVT_LoadoutSwap.Swap(actor, recruitEntity);

		SendSwapResult(SwapResultCodeFor(result), result.m_iExchanged, result.m_iRelocated, result.m_iFailed, result.m_iDropped);
	}

	//------------------------------------------------------------------------------------------------
	//! The swap's rules, every one of them re-derived on this machine.
	//!
	//! NOTHING IN THE PAYLOAD IS TRUSTED EXCEPT THE RplId, AND THAT IS VALIDATED. In order:
	//!
	//!  1. the requester has a character in the world at all - it is looked up from the player id this
	//!     controller resolved to, never sent;
	//!  2. the RplId resolves to a live entity. A junk or stale id from a client answers null here,
	//!     which is a refusal, not a crash;
	//!  3. that entity is not the requester themselves - swapping a character with itself is the one
	//!     input the swap routine cannot make sense of, and it refuses it too;
	//!  4. that entity is a recruit, and its record says the requester owns it. This is the
	//!     anti-spoofing check: naming somebody else's recruit gets you RESULT_NOT_OWNER and nothing
	//!     else - no confirmation that the id was even real;
	//!  5. the recruit is ACTIVE. Parking is a deliberate state and a parked recruit's kit is what
	//!     lets it hold its ground;
	//!  6. the recruit is alive and conscious;
	//!  7. the two characters are actually next to each other. Derived from both origins server-side,
	//!     so a forged request from across the map fails on geometry rather than on trust.
	//! \param[in] playerId Runtime id of the requesting player.
	//! \param[in] recruitEntityRplId The recruit body the client named.
	//! \param[out] actor The requester's character, on success.
	//! \param[out] recruitEntity The recruit's body, on success.
	//! \return RESULT_OK when the swap may proceed, otherwise the refusal code.
	protected int ValidateSwapRequest(int playerId, RplId recruitEntityRplId, out IEntity actor, out IEntity recruitEntity)
	{
		actor = null;
		recruitEntity = null;

		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if(!recruits) return RESULT_FAILED;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return RESULT_FAILED;

		// 1
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if(!playerManager) return RESULT_FAILED;

		actor = playerManager.GetPlayerControlledEntity(playerId);
		if(!actor) return RESULT_NO_ACTOR;

		// 2
		recruitEntity = ResolveEntity(recruitEntityRplId);
		if(!recruitEntity) return RESULT_NO_RECRUIT;

		// 3
		if(recruitEntity == actor) return RESULT_NO_RECRUIT;

		// 4
		OVT_RecruitData recruit = recruits.GetRecruitFromEntity(recruitEntity);
		if(!recruit) return RESULT_NO_RECRUIT;

		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if(persId.IsEmpty()) return RESULT_FAILED;

		if(recruit.m_sOwnerPersistentId != persId)
		{
			Print("[Overthrow] Recruit loadout swap refused: player " + playerId.ToString() + " does not own recruit " + recruit.m_sRecruitId, LogLevel.WARNING);
			return RESULT_NOT_OWNER;
		}

		// 5
		if(recruit.m_bInactive) return RESULT_RECRUIT_INACTIVE;

		// 6
		if(!IsAliveAndConscious(recruitEntity)) return RESULT_RECRUIT_DOWN;

		// 7
		if(vector.Distance(actor.GetOrigin(), recruitEntity.GetOrigin()) > SWAP_MAX_DISTANCE) return RESULT_TOO_FAR;

		return RESULT_OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Which sentence the swap earned.
	//!
	//! An empty swap between two characters carrying nothing is COMPLETE, not failed: afterwards each
	//! of them is wearing exactly what the other was, which is what was asked for.
	//! \param[in] result What the swap routine reported.
	//! \return One of the RESULT_SWAP_ codes.
	protected int SwapResultCodeFor(notnull OVT_LoadoutSwapResult result)
	{
		if(result.IsComplete()) return RESULT_SWAP_OK;
		if(result.IsPartial()) return RESULT_SWAP_PARTIAL;

		return RESULT_SWAP_FAILED;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a character is up and awake.
	//!
	//! Both questions, because they are different ones: GetLifeState() answers DEAD/INCAPACITATED, and
	//! IsUnconscious() additionally covers the wake-up animation, during which the life state has
	//! already flipped back to ALIVE (CharacterControllerComponent.c:262-267).
	//! \param[in] entity The character to test.
	//! \return True when it is alive and conscious.
	protected bool IsAliveAndConscious(IEntity entity)
	{
		if(!entity) return false;

		CharacterControllerComponent controller = CharacterControllerComponent.Cast(entity.FindComponent(CharacterControllerComponent));
		if(!controller) return false;

		if(controller.GetLifeState() != ECharacterLifeState.ALIVE) return false;

		return !controller.IsUnconscious();
	}

	//------------------------------------------------------------------------------------------------
	//! RplId -> entity, null-guarded at every step.
	//!
	//! Same helper as OVT_ShopTransactionComponent.ResolveEntity (:750). An RplId is the only entity
	//! reference that means the same thing on both machines, and one that names nothing simply answers
	//! null - which is exactly what a hostile client sending a made-up id has to get.
	//! \param[in] rplId The networked entity reference.
	//! \return The entity, or null.
	protected IEntity ResolveEntity(RplId rplId)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(rplId));
		if(!rpl) return null;

		return rpl.GetEntity();
	}

	//------------------------------------------------------------------------------------------------
	//! Delivers the outcome of a loadout swap to the requesting client.
	//!
	//! Its own RPC rather than SendResult()'s, because the swap is the one request whose answer is not
	//! a single fact: "partial" without counts tells a player nothing they can act on, and a hint that
	//! says which items stayed behind is the difference between a bug report and a shrug. Listen-server
	//! shape is SendResult()'s, verbatim and for the same reason (BUG-164).
	//! \param[in] resultCode One of the RESULT_ codes.
	//! \param[in] exchanged Items that reached the other character in their matching slot.
	//! \param[in] misplaced Items that reached the other character but not in their matching slot.
	//! \param[in] failed Items that stayed with the character they started on.
	//! \param[in] dropped Items that ended up on the ground.
	void SendSwapResult(int resultCode, int exchanged, int misplaced, int failed, int dropped)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();

		Print(string.Format("[Overthrow] Recruit loadout swap result %1 for player %2 (%3 exchanged, %4 misplaced, %5 failed, %6 dropped)",
			resultCode, playerId, exchanged, misplaced, failed, dropped), LogLevel.NORMAL);

		if(playerId > 0 && playerId == SCR_PlayerController.GetLocalPlayerId())
		{
			RpcDo_SwapLoadoutResult(resultCode, exchanged, misplaced, failed, dropped);
			return;
		}

		Rpc(RpcDo_SwapLoadoutResult, resultCode, exchanged, misplaced, failed, dropped);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the outcome of a loadout swap, for display only.
	//!
	//! ! THIS RPC NEVER MUTATES STATE, like its sibling. The items themselves are replicated entities;
	//! the client learns where they went by seeing them, and this only names what it cannot see - the
	//! pieces that did NOT come along.
	//!
	//! The three swap sentences are LITERAL ENGLISH with their keys named in comments, following the
	//! Phase 5/6 precedent: they carry counts, so they have to be assembled at runtime, and a key with
	//! a %1 in it that has not been exported yet renders as the raw key WITH the number missing - a
	//! strictly worse failure than plain English. The keys exist in Language/localization_Overthrow.st
	//! (decision D14: the .st master only, never the generated runtime exports) and this switches over
	//! in one edit once the user regenerates them.
	//! \param[in] resultCode One of the RESULT_ codes.
	//! \param[in] exchanged Items that reached the other character in their matching slot.
	//! \param[in] misplaced Items that reached the other character but not in their matching slot.
	//! \param[in] failed Items that stayed with the character they started on.
	//! \param[in] dropped Items that ended up on the ground.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SwapLoadoutResult(int resultCode, int exchanged, int misplaced, int failed, int dropped)
	{
		if(resultCode == RESULT_SWAP_OK)
		{
			// TODO localization: #OVT-Recruit_SwapComplete
			OVT_Global.ShowHint("Kit swapped");
			return;
		}

		if(resultCode == RESULT_SWAP_PARTIAL)
		{
			// TODO localization: #OVT-Recruit_SwapPartial
			OVT_Global.ShowHint(BuildPartialSwapHint(exchanged, misplaced, failed, dropped));
			return;
		}

		if(resultCode == RESULT_SWAP_FAILED)
		{
			// TODO localization: #OVT-Recruit_SwapFailed
			OVT_Global.ShowHint("Nothing could be exchanged");
			return;
		}

		// Everything else is an ordinary refusal and shares the toggle's hint table.
		string reason = ReasonKeyFor(resultCode);
		if(reason.IsEmpty()) return;

		OVT_Global.ShowHint(reason);
	}

	//------------------------------------------------------------------------------------------------
	//! The sentence for a partial swap.
	//!
	//! Built from the parts that are non-zero, because a player who is told "2 items could not be
	//! exchanged" when the real problem is "1 item is on the grass behind you" will not find it.
	//! \param[in] exchanged Items that reached the other character in their matching slot.
	//! \param[in] misplaced Items that reached the other character but not in their matching slot.
	//! \param[in] failed Items that stayed with the character they started on.
	//! \param[in] dropped Items that ended up on the ground.
	//! \return A hint string.
	protected string BuildPartialSwapHint(int exchanged, int misplaced, int failed, int dropped)
	{
		string hint = string.Format("Kit swapped: %1 items exchanged", exchanged + misplaced);

		if(misplaced > 0)
			hint = hint + string.Format(", %1 stowed instead of worn", misplaced);

		if(failed > 0)
			hint = hint + string.Format(", %1 could not be exchanged", failed);

		if(dropped > 0)
			hint = hint + string.Format(", %1 on the ground", dropped);

		return hint;
	}

	//------------------------------------------------------------------------------------------------
	//! Ask the server what a recruit wearing this loadout would cost at this tent.
	//!
	//! Called by the picker whenever the highlighted loadout changes. A quote NEVER mutates anything
	//! and never charges - it is the same validation the purchase runs, stopped one step short of
	//! doing it.
	//! \param[in] tentRplId The recruitment tent the player is standing at.
	//! \param[in] loadoutName One of the player's own saved loadouts.
	void RequestRecruitLoadoutQuote(RplId tentRplId, string loadoutName)
	{
		if(Replication.IsServer())
		{
			RpcAsk_QuoteEquippedRecruit(tentRplId, loadoutName);
		}else{
			Rpc(RpcAsk_QuoteEquippedRecruit, tentRplId, loadoutName);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: price a purchase without performing it.
	//!
	//! ! READ-ONLY, AND THAT IS A RULE RATHER THAN AN OBSERVATION. Nothing below this line may spawn,
	//! charge, take a supporter or write a record: a picker asks for a quote every time the selection
	//! moves, so anything with a side effect here would fire on scrolling.
	//! \param[in] tentRplId The tent the client named.
	//! \param[in] loadoutName The loadout the client named.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_QuoteEquippedRecruit(RplId tentRplId, string loadoutName)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0)
		{
			Print("[Overthrow] Equipped recruit quote refused: could not resolve the requesting player", LogLevel.WARNING);
			return;
		}

		OVT_EquippedRecruitQuote quote = ValidateTentPurchase(playerId, tentRplId, loadoutName);

		SendRecruitQuote(loadoutName, quote.m_iTotalPrice, quote.m_iItemCount, quote.m_iRefusalCode, quote.m_sUnpriceableResource, quote.m_iCoveredValue);
	}

	//------------------------------------------------------------------------------------------------
	//! Ask the server to buy a recruit already wearing one of your saved loadouts.
	//!
	//! The client names a tent and a loadout. It does not name itself, it does not name a body, and it
	//! does not send a price - all three are derived server-side.
	//! \param[in] tentRplId The recruitment tent the player is standing at.
	//! \param[in] loadoutName One of the player's own saved loadouts.
	void RequestBuyEquippedRecruit(RplId tentRplId, string loadoutName)
	{
		if(Replication.IsServer())
		{
			RpcAsk_BuyEquippedRecruit(tentRplId, loadoutName);
		}else{
			Rpc(RpcAsk_BuyEquippedRecruit, tentRplId, loadoutName);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the whole transaction, in the one order that cannot take a player's money for nothing.
	//!
	//!  1. resolve who is asking, from the controller entity this component sits on
	//!  2. RE-VALIDATE EVERYTHING with the same routine the quote ran: the tent resolves and is a
	//!     recruitment tent, the buyer is at it, is under the cap, the town has a supporter, the
	//!     loadout is theirs and is priceable, and they can afford the price it produces
	//!  3. spawn and own the recruit. Nothing has been charged yet
	//!  4. equip it from prefabs - the ONE ApplyLoadoutToEntity call in this file, and its target is
	//!     the local body from step 3, never anything a client named (risk R13)
	//!  5. take the town supporter, but only if a recruit actually exists
	//!  6. charge, server-side and directly. A failed spawn charges nothing; a kit that landed nothing
	//!     charges only the recruit cost (decision D19)
	//!  7. report, with the amount actually taken so the client can compare it against the quote
	//!
	//! Steps 3-6 are in this order because the shipped tent handler is: costs are taken only once the
	//! recruit exists (OVT_PlayerCommsComponent.c:1528). Charging first and failing to spawn is the
	//! one ordering that can take money for nothing.
	//! \param[in] tentRplId The tent the client named.
	//! \param[in] loadoutName The loadout the client named.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_BuyEquippedRecruit(RplId tentRplId, string loadoutName)
	{
		if(!Replication.IsServer()) return;

		// BUG-090 diagnostic, same role as its two siblings: this line firing proves the request left
		// the client, arrived here and decoded with the right arity.
		Print("[Overthrow] Equipped recruit purchase received: loadout=" + loadoutName, LogLevel.NORMAL);

		// 1
		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0)
		{
			Print("[Overthrow] Equipped recruit purchase refused: could not resolve the requesting player", LogLevel.WARNING);
			return;
		}

		// 2
		OVT_EquippedRecruitQuote quote = ValidateTentPurchase(playerId, tentRplId, loadoutName);
		if(quote.m_iRefusalCode != RESULT_OK)
		{
			SendPurchaseResult(quote.m_iRefusalCode, 0, 0, 0);
			return;
		}

		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if(!recruits)
		{
			SendPurchaseResult(RESULT_FAILED, 0, 0, 0);
			return;
		}

		// 3 - nothing has been charged at this point, and nothing will be if this returns null
		SCR_ChimeraCharacter body = recruits.SpawnTentRecruit(quote.m_Tent, quote.m_vTentPosition, playerId);

		int applied = 0;
		int total = quote.m_iTopLevelCount;

		// 4
		if(body)
		{
			OVT_LoadoutManagerComponent loadouts = OVT_Global.GetLoadouts();
			if(loadouts)
			{
				// The bool is deliberately ignored: it only says "at least one item landed", and the
				// counters behind it say how many - which is what grades the outcome. They are written
				// by the apply itself immediately before it returns, so they describe this apply.
				loadouts.ApplyLoadoutToEntity(quote.m_Loadout, body);

				applied = loadouts.GetLastApplySuccessCount();
				total = loadouts.GetLastApplyTotalCount();
			}
		}

		int outcome = OVT_RecruitPurchaseRules.OutcomeFor(body != null, applied, total);
		int charge = OVT_RecruitPurchaseRules.ChargeFor(outcome, quote.m_iRecruitCost, quote.m_iTotalPrice);

		// 5 - a supporter is a cost, so it follows the same rule the money does: only once the recruit
		// is real. A failed spawn must leave the town exactly as it found it.
		if(body)
		{
			OVT_TownManagerComponent towns = OVT_Global.GetTowns();
			if(towns)
				towns.TakeSupportersFromNearestTown(quote.m_vTentPosition, 1);
		}

		// 5b - consume the warehouse stock this quote covered, now that the recruit exists but before
		// any money changes hands (T5.3, implementation.md §3.8). Gated on the SAME exception ChargeFor
		// itself uses (decision D19): a total gear failure charges no gear fee, so it must not drain a
		// warehouse for gear that never landed on the body either.
		if(body && outcome != RESULT_GEAR_FAILED)
			ConsumeQuotedGearStock(quote);

		// 6 - TakePlayerMoneyPersistentId is the DIRECT server-side call, and the runtime-player-id
		// variant of it must never appear in this file: that one forwards through
		// OVT_Global.GetServer(), which off the server is an Rpc() out of a server handler - dropped in
		// silence (BUG-161). A grep for it over this file is one of the phase's acceptance checks.
		if(charge > 0)
		{
			OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
			if(economy)
				economy.TakePlayerMoneyPersistentId(quote.m_sPersistentId, charge);
		}

		// 7
		SendPurchaseResult(outcome, charge, applied, total);
	}

	//------------------------------------------------------------------------------------------------
	//! Consumes the warehouse stock a re-derived quote covered. SERVER, once a recruit body is real.
	//!
	//! The OVT_HighCommandManagerComponent.ConsumeQuotedStock shape: each line asks TakeUpTo for its
	//! whole need and the ledger answers with what it actually had - the same min(need, available)
	//! SplitCoverage used a moment earlier, expressed once.
	//! \param[in] quote The quote re-derived for this purchase, carrying its own manifest and stores.
	protected void ConsumeQuotedGearStock(notnull OVT_EquippedRecruitQuote quote)
	{
		if(!quote.m_aStores || quote.m_aStores.IsEmpty() || !quote.m_aManifest) return;

		foreach(OVT_ItemSourcingLine line : quote.m_aManifest)
		{
			if(!line) continue;

			OVT_WarehouseStockUtils.TakeUpTo(quote.m_aStores, line.m_sResource, line.m_iNeeded);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! THE ONE ROUTINE BOTH THE QUOTE AND THE PURCHASE RUN. Every rule, re-derived on this machine.
	//!
	//! Nothing in the payload is trusted except an RplId and a string, and both are validated. In
	//! order, and the order matters:
	//!
	//!  1. the requester has a persistent identity;
	//!  2. the RplId resolves to a live entity, and that entity carries an OVT_BuildableComponent
	//!     declaring itself a recruitment tent. This is strictly stronger than the legacy tent handler,
	//!     which trusts a bare vector and never checks that a tent is there at all;
	//!  3. the ordinary tent rules - under the cap, the nearest town has a supporter, and the buyer is
	//!     within TENT_MAX_DISTANCE of the tent - asked of the recruit manager, so an equipped purchase
	//!     obeys exactly the same rules as the plain action;
	//!  4. the loadout, fetched by the RESOLVED persistent id. Naming another player's loadout finds
	//!     nothing, because the store is keyed by owner;
	//!  5. it records at least one item;
	//!  6. every resource in it has a catalog price. One that does not refuses the whole purchase and
	//!     names itself - never priced at zero, never guessed (decision D20);
	//!  7. LAST, the funds check. Last on purpose: by then the price exists, so a player who cannot
	//!     afford it is still shown what it would have cost instead of a bare "no".
	//!
	//! \param[in] playerId Runtime id of the requesting player, resolved from the controller entity.
	//! \param[in] tentRplId The tent the client named.
	//! \param[in] loadoutName The loadout the client named.
	//! \return A quote. Check m_iRefusalCode before spending anything in it.
	protected OVT_EquippedRecruitQuote ValidateTentPurchase(int playerId, RplId tentRplId, string loadoutName)
	{
		OVT_EquippedRecruitQuote quote = new OVT_EquippedRecruitQuote();
		quote.m_iRefusalCode = RESULT_FAILED;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return quote;

		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if(!recruits) return quote;

		OVT_LoadoutManagerComponent loadouts = OVT_Global.GetLoadouts();
		if(!loadouts) return quote;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return quote;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(!config || !config.m_Difficulty) return quote;

		// 1
		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if(persId.IsEmpty()) return quote;

		quote.m_sPersistentId = persId;

		// 2 - the tent is treated as hostile, exactly as the swap treats the recruit body it is given
		IEntity tent = ResolveEntity(tentRplId);
		if(!tent)
		{
			quote.m_iRefusalCode = RESULT_NO_TENT;
			return quote;
		}

		OVT_BuildableComponent buildable = OVT_BuildableComponent.Cast(tent.FindComponent(OVT_BuildableComponent));
		if(!buildable || buildable.GetBuildableType() != RECRUITMENT_TENT_TYPE)
		{
			quote.m_iRefusalCode = RESULT_NO_TENT;
			return quote;
		}

		quote.m_Tent = tent;
		quote.m_vTentPosition = tent.GetOrigin();

		// 3
		int tentRefusal = recruits.ValidateTentRecruit(quote.m_vTentPosition, playerId);
		if(tentRefusal != OVT_RecruitManagerComponent.TENT_RECRUIT_OK)
		{
			quote.m_iRefusalCode = TentRefusalToResult(tentRefusal);
			return quote;
		}

		// 4 - THE ANTI-SPOOFING CHECK for the loadout half. The store is keyed playerId_loadoutName
		// (OVT_LoadoutManagerComponent.c:868), and the playerId here is the one this controller
		// resolved, never one the client sent.
		OVT_PlayerLoadout loadout = loadouts.GetLoadout(persId, loadoutName);
		if(!loadout)
		{
			quote.m_iRefusalCode = RESULT_NO_LOADOUT;
			return quote;
		}

		quote.m_Loadout = loadout;
		quote.m_iTopLevelCount = loadout.GetItemCount();

		// 5
		if(quote.m_iTopLevelCount <= 0)
		{
			quote.m_iRefusalCode = RESULT_LOADOUT_EMPTY;
			return quote;
		}

		// 6
		OVT_RecruitLoadoutPrice price = OVT_RecruitLoadoutPricing.Price(loadout, quote.m_vTentPosition, playerId);
		if(!price.IsPriceable())
		{
			quote.m_sUnpriceableResource = price.m_sUnpriceableResource;

			// An empty name means the walk never ran at all (no economy, no server), which is a broken
			// machine rather than a broken loadout - and the two want different sentences.
			if(!quote.m_sUnpriceableResource.IsEmpty())
				quote.m_iRefusalCode = RESULT_UNPRICEABLE;

			return quote;
		}

		quote.m_iGearSubtotal = price.m_iSubtotal;
		quote.m_iItemCount = price.m_iItemCount;
		quote.m_aManifest = price.m_aManifest;

		// 6b - the tent half of the shipped warehouse TODO (implementation.md §3.8). With nothing in
		// range CollectStores returns empty, CountAvailable answers 0 for every line, and
		// SplitCoverage's charged subtotal collapses to price.m_iSubtotal exactly - so a campaign with
		// no warehouse quotes byte-identically to before this phase.
		quote.m_aStores = new array<OVT_StorageComponent>();
		OVT_WarehouseStockUtils.CollectStores(quote.m_vTentPosition, OVT_HighCommandRules.WAREHOUSE_RANGE, persId, quote.m_aStores);

		// THE TENT'S OWN CRATE IS THE FIRST SOURCE. It is prepended rather than collected because
		// CollectStores only offers registered WAREHOUSES, and the tent is a buildable with a ledger -
		// which is the point: this is the pre-warehouse supply line, filled with early-game loot.
		OVT_WarehouseStockUtils.PrependStore(quote.m_aStores, OVT_StorageUtils.GetStorage(quote.m_Tent));

		array<int> availablePerLine = {};
		foreach (OVT_ItemSourcingLine line : quote.m_aManifest)
		{
			availablePerLine.Insert(OVT_WarehouseStockUtils.CountAvailable(quote.m_aStores, line.m_sResource));
		}

		int coveredUnits, coveredValue, chargedUnits;
		int chargedGearSubtotal = OVT_ItemSourcingRules.SplitCoverage(quote.m_aManifest, availablePerLine, coveredUnits, coveredValue, chargedUnits);
		quote.m_iCoveredValue = coveredValue;

		// The SAME half-price the plain tent action charges (OVT_PlayerCommsComponent.c:1510), because
		// an equipped recruit is a tent recruit that arrives dressed (decision D23).
		quote.m_iRecruitCost = Math.Round(config.m_Difficulty.baseRecruitCost * 0.5);
		quote.m_iTotalPrice = OVT_RecruitPurchaseRules.TotalPrice(quote.m_iRecruitCost, chargedGearSubtotal, config.m_Difficulty.recruitLoadoutFeeMultiplier);

		// 7 - explicit, because the charge itself clamps at zero and can never report a shortfall
		// (OVT_EconomyManagerComponent.DoTakePlayerMoney :1206-1216).
		if(!economy.PlayerHasMoney(persId, quote.m_iTotalPrice))
		{
			quote.m_iRefusalCode = RESULT_CANNOT_AFFORD;
			return quote;
		}

		quote.m_iRefusalCode = RESULT_OK;
		return quote;
	}

	//------------------------------------------------------------------------------------------------
	//! Translate the recruit manager's tent-validation vocabulary into this component's wire codes.
	//!
	//! Two vocabularies rather than one because the manager has no business knowing what a client is
	//! told: its codes describe rules, these describe sentences. TOO_FAR becomes NO_TENT deliberately -
	//! to a player "you are not at a tent" and "you walked away from the tent" are the same fact, and
	//! neither reveals anything about a tent they cannot see.
	//! \param[in] tentRefusal One of OVT_RecruitManagerComponent's TENT_RECRUIT_ codes.
	//! \return One of the RESULT_ codes.
	protected int TentRefusalToResult(int tentRefusal)
	{
		if(tentRefusal == OVT_RecruitManagerComponent.TENT_RECRUIT_AT_CAP) return RESULT_AT_CAP;
		if(tentRefusal == OVT_RecruitManagerComponent.TENT_RECRUIT_NO_SUPPORTERS) return RESULT_NO_SUPPORTERS;
		if(tentRefusal == OVT_RecruitManagerComponent.TENT_RECRUIT_TOO_FAR) return RESULT_NO_TENT;
		if(tentRefusal == OVT_RecruitManagerComponent.TENT_RECRUIT_NO_IDENTITY) return RESULT_NO_ACTOR;

		return RESULT_FAILED;
	}

	//------------------------------------------------------------------------------------------------
	//! Delivers a purchase quote to the asking client.
	//!
	//! Listen-server shape is SendResult()'s, verbatim and for the same reason (BUG-164).
	//!
	//! Deliberately quiet - no Print. A picker asks for a quote every time the selection moves, and on
	//! a pad that is once per row of a scroll.
	//! \param[in] loadoutName The loadout this quote is for. Carried so a consumer can tell a stale
	//!        answer from the current one.
	//! \param[in] price The whole quoted price, or 0 when it never got that far.
	//! \param[in] itemCount How many items were priced.
	//! \param[in] refusalCode RESULT_OK, or why it cannot be bought.
	//! \param[in] unpriceableResource The item with no price, when that is the refusal.
	//! \param[in] coveredValue Value of the gear covered by nearby warehouse stock.
	void SendRecruitQuote(string loadoutName, int price, int itemCount, int refusalCode, string unpriceableResource, int coveredValue)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();

		if(playerId > 0 && playerId == SCR_PlayerController.GetLocalPlayerId())
		{
			RpcDo_RecruitQuote(loadoutName, price, itemCount, refusalCode, unpriceableResource, coveredValue);
			return;
		}

		Rpc(RpcDo_RecruitQuote, loadoutName, price, itemCount, refusalCode, unpriceableResource, coveredValue);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: adopt a purchase quote.
	//!
	//! ! THIS RPC NEVER MUTATES GAME STATE. It writes a display cache and fires an invoker; the price
	//! it carries is the SERVER'S number and is never recomputed here, because a client cannot compute
	//! one (its copy of the loadout has no items).
	//! \param[in] loadoutName The loadout this quote is for.
	//! \param[in] price The whole quoted price.
	//! \param[in] itemCount How many items were priced.
	//! \param[in] refusalCode RESULT_OK, or why it cannot be bought.
	//! \param[in] unpriceableResource The item with no price, when that is the refusal.
	//! \param[in] coveredValue Value of the gear covered by nearby warehouse stock.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_RecruitQuote(string loadoutName, int price, int itemCount, int refusalCode, string unpriceableResource, int coveredValue)
	{
		m_sLastQuoteLoadout = loadoutName;
		m_iLastQuotePrice = price;
		m_iLastQuoteItemCount = itemCount;
		m_iLastQuoteRefusal = refusalCode;
		m_sLastQuoteUnpriceable = unpriceableResource;
		m_iLastQuoteCoveredValue = coveredValue;

		if(m_OnRecruitQuote)
			m_OnRecruitQuote.Invoke(loadoutName, price, itemCount, refusalCode, unpriceableResource, coveredValue);
	}

	//------------------------------------------------------------------------------------------------
	//! Delivers the outcome of an equipped-recruit purchase to the buying client.
	//!
	//! Its own RPC rather than SendResult()'s, for the same reason the swap has one: the answer is not
	//! a single fact. "Partial" without counts, or a charge without a number, tells a player nothing
	//! they can check against the price they were shown.
	//! \param[in] resultCode One of the RESULT_ codes.
	//! \param[in] charged What was actually taken.
	//! \param[in] applied Top-level items that landed on the recruit.
	//! \param[in] total Top-level items the loadout recorded.
	void SendPurchaseResult(int resultCode, int charged, int applied, int total)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();

		Print(string.Format("[Overthrow] Equipped recruit purchase result %1 for player %2 (charged %3, %4/%5 items applied)",
			resultCode, playerId, charged, applied, total), LogLevel.NORMAL);

		if(playerId > 0 && playerId == SCR_PlayerController.GetLocalPlayerId())
		{
			RpcDo_EquippedRecruitResult(resultCode, charged, applied, total);
			return;
		}

		Rpc(RpcDo_EquippedRecruitResult, resultCode, charged, applied, total);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the outcome of an equipped-recruit purchase, for display only.
	//!
	//! ! THIS RPC NEVER MUTATES STATE. The recruit, its gear and the player's balance all arrive
	//! through their own replicated channels; this only names what the player cannot see - how much
	//! was taken, and how much of the kit made it.
	//!
	//! The three count-bearing sentences are LITERAL ENGLISH with their keys named in comments,
	//! following the Phase 7 precedent: a key with a %1 in it that has not been exported yet renders
	//! as the raw key with the numbers missing, which is strictly worse than plain English. The keys
	//! exist in Language/localization_Overthrow.st (decision D14) and this switches over in one edit.
	//! \param[in] resultCode One of the RESULT_ codes.
	//! \param[in] charged What was actually taken.
	//! \param[in] applied Top-level items that landed on the recruit.
	//! \param[in] total Top-level items the loadout recorded.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_EquippedRecruitResult(int resultCode, int charged, int applied, int total)
	{
		if(m_OnEquippedRecruitResult)
			m_OnEquippedRecruitResult.Invoke(resultCode, charged, applied, total);

		if(resultCode == RESULT_BUY_OK)
		{
			// TODO localization: #OVT-Recruit_BuyComplete
			OVT_Global.ShowHint(string.Format("Recruit hired and equipped ($%1)", charged));
			return;
		}

		if(resultCode == RESULT_BUY_PARTIAL)
		{
			// TODO localization: #OVT-Recruit_BuyPartial
			OVT_Global.ShowHint(string.Format("Recruit hired for $%1, but only %2 of %3 items arrived", charged, applied, total));
			return;
		}

		if(resultCode == RESULT_GEAR_FAILED)
		{
			// TODO localization: #OVT-Recruit_BuyGearFailed
			OVT_Global.ShowHint(string.Format("Recruit hired for $%1 - none of the kit could be issued, so the gear was not charged for", charged));
			return;
		}

		// Everything else is an ordinary refusal and shares the toggle's hint table.
		string reason = ReasonKeyFor(resultCode);
		if(reason.IsEmpty()) return;

		OVT_Global.ShowHint(reason);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: the loadout the most recent quote was for.
	//!
	//! ! COMPARE THIS AGAINST THE ROW YOU ARE SHOWING before trusting the price. Quotes are asked
	//! per-selection and answered a round trip later, so during a fast scroll the cached price belongs
	//! to whichever answer arrived last - which is not necessarily the row now highlighted.
	//! \return The loadout name, or empty when no quote has arrived.
	string GetLastQuoteLoadout()
	{
		return m_sLastQuoteLoadout;
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: the price the most recent quote reported.
	//! \return The price, or 0 when the quote never got as far as computing one.
	int GetLastQuotePrice()
	{
		return m_iLastQuotePrice;
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: how many items the most recent quote priced, nesting and attachments included.
	//! \return The item count.
	int GetLastQuoteItemCount()
	{
		return m_iLastQuoteItemCount;
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: whether the most recent quote may be bought.
	//! \return RESULT_OK, or the refusal code.
	int GetLastQuoteRefusal()
	{
		return m_iLastQuoteRefusal;
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: the unpriceable item the most recent quote named, if any.
	//! \return A resource name, or empty.
	string GetLastQuoteUnpriceable()
	{
		return m_sLastQuoteUnpriceable;
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: value of the gear the most recent quote reported as covered by nearby warehouse stock.
	//! \return An amount of money, 0 when nothing was covered.
	int GetLastQuoteCoveredValue()
	{
		return m_iLastQuoteCoveredValue;
	}

	//------------------------------------------------------------------------------------------------
	//! A prefab resource reduced to something a player can read.
	//!
	//! "{GUID}Prefabs/Weapons/Rifles/AK74/Rifle_AK74.et" is not a sentence, and the only part of it a
	//! player can act on is the last component. Static so the picker can use the same shortening the
	//! hints do - one appearance of a resource name should look the same everywhere.
	//! \param[in] resourceName A full resource name, possibly with a GUID prefix.
	//! \return The file name without its path, GUID or .et suffix; the input unchanged if it has none.
	static string ShortResourceName(string resourceName)
	{
		if(resourceName.IsEmpty()) return resourceName;

		string name = resourceName;

		int slash = name.LastIndexOf("/");
		if(slash > -1 && slash < name.Length() - 1)
			name = name.Substring(slash + 1, name.Length() - slash - 1);

		int dot = name.LastIndexOf(".");
		if(dot > 0)
			name = name.Substring(0, dot);

		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! Delivers the outcome of a recruit command to the requesting client.
	//!
	//! The requester is this controller's owner by construction, so the player id is resolved here
	//! rather than passed in - there is no caller that could legitimately name a different player,
	//! and not taking one means no caller can get it wrong.
	//!
	//! On a listen server the requester IS this machine's local player, and a listen host never
	//! receives its own owner-targeted RPCs, so the handler is called directly and nothing is sent.
	//! On a dedicated server GetLocalPlayerId() is 0, no player id can match it, and this always goes
	//! over the wire. Same shape as OVT_RespawnRequestComponent.SendRespawnResult.
	//! \param[in] resultCode One of the RESULT_ codes.
	void SendResult(int resultCode)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();

		// Pairs with the arrival Print in RpcAsk_SetRecruitInactive: neither line means the request
		// never arrived, both lines mean the server answered, and this one carries the player id.
		Print("[Overthrow] Recruit command result " + resultCode.ToString() + " for player " + playerId.ToString(), LogLevel.NORMAL);

		if(playerId > 0 && playerId == SCR_PlayerController.GetLocalPlayerId())
		{
			RpcDo_RecruitCommandResult(resultCode);
			return;
		}

		Rpc(RpcDo_RecruitCommandResult, resultCode);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the outcome of a recruit command, for display only.
	//!
	//! ! THIS RPC NEVER MUTATES STATE. The recruit's state arrives through
	//! RpcDo_RecruitActiveStateChanged, which is broadcast to everybody by the manager; a lost packet
	//! here costs a hint, not a state change. Writing m_bInactive from here would create a second,
	//! client-local truth that could disagree with the broadcast.
	//!
	//! A plain OK is silent: the recruit visibly stops following or starts again. Every refusal
	//! hints, because server authority means a request can be refused after the hold has already
	//! played out and the player has no other feedback channel.
	//!
	//! The keys ReasonKeyFor returns are not in the generated localization exports yet (the .st
	//! master carries them and the user regenerates in Workbench), so they render raw until then.
	//! Using the key is still correct - a literal here would have to be found and removed later.
	//! \param[in] resultCode One of the RESULT_ codes.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_RecruitCommandResult(int resultCode)
	{
		string reason = ReasonKeyFor(resultCode);
		if(reason.IsEmpty()) return;

		OVT_Global.ShowHint(reason);
	}

	//------------------------------------------------------------------------------------------------
	//! The hint key for a result code.
	//!
	//! An if/else chain rather than a lookup table because EnforceScript has no ternary and a
	//! ten-entry map would have to be built and kept alive for a value read once per request.
	//!
	//! Six codes are absent on purpose, and they are the six whose sentences carry NUMBERS: the three
	//! RESULT_SWAP_ ones, assembled in RpcDo_SwapLoadoutResult, and RESULT_BUY_OK / RESULT_BUY_PARTIAL
	//! / RESULT_GEAR_FAILED, assembled in RpcDo_EquippedRecruitResult. Neither handler asks this method
	//! about them, so a key returned for one of them would silently never be shown.
	//! \param[in] resultCode One of the RESULT_ codes.
	//! \return A localization key, or an empty string when the outcome needs no hint.
	static string ReasonKeyFor(int resultCode)
	{
		if(resultCode == RESULT_OK) return "";

		// A no-op is not a failure and not worth a hint: the recruit is already doing what was asked.
		if(resultCode == RESULT_NO_CHANGE) return "";

		if(resultCode == RESULT_NOT_OWNER) return "#OVT-Recruit_NotYours";
		if(resultCode == RESULT_NO_RECRUIT) return "#OVT-Recruit_NotYours";
		if(resultCode == RESULT_NO_BODY) return "#OVT-Recruit_NoBody";
		if(resultCode == RESULT_IN_VEHICLE) return "#OVT-Recruit_CannotParkInVehicle";

		// A missing body for the REQUESTER reads the same way to a player as a missing body for the
		// recruit, and neither is reachable from Overthrow's own UI.
		if(resultCode == RESULT_NO_ACTOR) return "#OVT-Recruit_NoBody";

		if(resultCode == RESULT_RECRUIT_INACTIVE) return "#OVT-Recruit_CannotSwapParked";
		if(resultCode == RESULT_RECRUIT_DOWN) return "#OVT-Recruit_CannotSwapDown";
		if(resultCode == RESULT_TOO_FAR) return "#OVT-Recruit_TooFarAway";

		// EQUIPPED TENT PURCHASE. Three of these reuse keys the plain tent action already shows for
		// the same three rules - a player who is at the cap should read the same sentence whichever
		// action they tried, and those keys are already in the runtime exports.
		if(resultCode == RESULT_NO_TENT) return "#OVT-Recruit_NotAtTent";
		if(resultCode == RESULT_NO_LOADOUT) return "#OVT-Recruit_NoSuchLoadout";
		if(resultCode == RESULT_LOADOUT_EMPTY) return "#OVT-Recruit_LoadoutEmpty";
		if(resultCode == RESULT_UNPRICEABLE) return "#OVT-Recruit_LoadoutUnpriceable";
		if(resultCode == RESULT_CANNOT_AFFORD) return "#OVT-CannotAfford";
		if(resultCode == RESULT_AT_CAP) return "#OVT-RecruitLimitReached";
		if(resultCode == RESULT_NO_SUPPORTERS) return "#OVT-NoSupportersAvailable";
		if(resultCode == RESULT_SPAWN_FAILED) return "#OVT-Recruit_BuyFailed";

		return "#OVT-Recruit_CommandFailed";
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: push one recruit's measured condition and position to this controller's owner.
	//!
	//! Called once per online owner per recruit per sweep by
	//! OVT_RecruitManagerComponent.SweepRecruitStatus(), which is the only caller and the only place
	//! the values are measured. Same listen-server shape as SendResult(): a listen host never
	//! receives its own owner-targeted RPCs (BUG-164), so when the target is this machine's local
	//! player the handler is called directly and nothing is sent.
	//!
	//! Deliberately silent - no Print. This runs 1.6 times a second per player at the recruit cap,
	//! and a log line at that rate would bury every other diagnostic in the file.
	//! \param[in] recruitId The recruit being described.
	//! \param[in] position Where its body is right now.
	//! \param[in] statusFlags A mask of OVT_RecruitStatus flags.
	void SendRecruitStatus(string recruitId, vector position, int statusFlags)
	{
		if(!Replication.IsServer()) return;
		if(recruitId.IsEmpty()) return;

		int playerId = ResolveOwningPlayerId();

		if(playerId > 0 && playerId == SCR_PlayerController.GetLocalPlayerId())
		{
			RpcDo_RecruitStatus(recruitId, position, statusFlags);
			return;
		}

		Rpc(RpcDo_RecruitStatus, recruitId, position, statusFlags);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: adopt a status push.
	//!
	//! TWO WRITES, BOTH OF THEM CACHES OF SERVER TRUTH. The flags go into this component's own map,
	//! which nothing else writes. The position goes onto the local replica record, because that is
	//! where the map layer looks when a recruit's entity is not streamed to this client - which for a
	//! parked recruit is the normal case, and the whole reason status is pushed rather than read
	//! locally. Neither write can create a record: a push for a recruit this client has never heard
	//! of updates the flag cache only, and the record arrives (or does not) through its own channels.
	//!
	//! ! IT NEVER TOUCHES m_bInactive. Membership arrives through the manager's broadcast
	//! RpcDo_RecruitActiveStateChanged; writing it from here would be a second, client-local truth.
	//! \param[in] recruitId The recruit being described.
	//! \param[in] position Where its body was when the server measured it.
	//! \param[in] statusFlags A mask of OVT_RecruitStatus flags.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_RecruitStatus(string recruitId, vector position, int statusFlags)
	{
		if(recruitId.IsEmpty()) return;

		if(!m_mStatusFlags) m_mStatusFlags = new map<string, int>();
		m_mStatusFlags.Set(recruitId, statusFlags);

		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if(!recruits) return;

		EnsureRemovalSubscription(recruits);

		OVT_RecruitData recruit = recruits.GetRecruit(recruitId);
		if(!recruit) return;

		recruit.m_vLastKnownPosition = position;
	}

	//------------------------------------------------------------------------------------------------
	//! The last status the server reported for a recruit.
	//!
	//! ZERO IS THE ANSWER FOR ANYTHING UNKNOWN, and it is not an error: a recruit hired ten seconds
	//! ago, a recruit belonging to another player, or a session whose first sweep has not run yet all
	//! read 0, which every consumer renders as an unarmed marker and an empty icon row rather than as
	//! a blank or a placeholder. The alternative - an "unknown" sentinel - would have to be handled
	//! by every caller to produce exactly the same picture.
	//! \param[in] recruitId The recruit to look up.
	//! \return A mask of OVT_RecruitStatus flags, or 0 when nothing has been reported.
	int GetStatusFlags(string recruitId)
	{
		if(!m_mStatusFlags) return 0;
		if(!m_mStatusFlags.Contains(recruitId)) return 0;

		return m_mStatusFlags.Get(recruitId);
	}

	//------------------------------------------------------------------------------------------------
	//! Subscribe to recruit removal once, the first time a manager is known to exist.
	//!
	//! Not done at init: this component is created with the controller, and on a joining client the
	//! recruit manager may not have resolved yet. Every entry point that could put something in the
	//! cache goes through here first, so the subscription exists by the time there is anything to
	//! prune.
	//! \param[in] recruits The live recruit manager.
	protected void EnsureRemovalSubscription(notnull OVT_RecruitManagerComponent recruits)
	{
		if(m_bSubscribedToRemoval) return;
		if(!recruits.m_OnRecruitRemoved) return;

		recruits.m_OnRecruitRemoved.Insert(OnRecruitRemoved);
		m_bSubscribedToRemoval = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Drop a dismissed or killed recruit's cached status.
	//!
	//! Without this the cache grows for the life of the session and, worse, a recruit id that is
	//! reused would inherit a dead recruit's flags. The invoker fires on both sides (the manager
	//! invokes it from its own removal path and from the client-side removal RPC), so this is correct
	//! wherever the component happens to be running.
	//! \param[in] recruit The record being removed.
	protected void OnRecruitRemoved(OVT_RecruitData recruit)
	{
		if(!recruit) return;
		if(!m_mStatusFlags) return;

		// Guarded: removing a key a map does not hold is not something to rely on, and most recruits
		// removed in a session were never in this cache (they belong to other players).
		if(!m_mStatusFlags.Contains(recruit.m_sRecruitId)) return;

		m_mStatusFlags.Remove(recruit.m_sRecruitId);
	}

	//------------------------------------------------------------------------------------------------
	//! Which player this controller belongs to, resolved on the SERVER from the controller entity
	//! this component sits on.
	//!
	//! This is the whole anti-spoofing story: a remote client can only reach this handler through the
	//! controller entity it owns, so the identity comes from the entity, never from the payload (the
	//! legacy comms component solves the same problem by living on the player's character). The scan
	//! is over connected players only, which is single digits.
	//!
	//! Copied verbatim from OVT_TowerSabotageComponent, itself shared with
	//! OVT_ShopTransactionComponent, OVT_TravelRequestComponent and OVT_RespawnRequestComponent -
	//! now five controller components deep, and a strong candidate for a shared base class. Copied
	//! rather than extracted because introducing that base class here would edit four working server
	//! paths for no behavioural gain.
	//! \return Runtime player id, or -1.
	protected int ResolveOwningPlayerId()
	{
		OVT_OverthrowController owner = OVT_OverthrowController.Cast(GetOwner());
		if(!owner) return -1;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return -1;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if(!playerManager) return -1;

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);

		foreach(int playerId : playerIds)
		{
			OVT_OverthrowController controller = players.GetController(playerId);
			if(controller == owner) return playerId;
		}

		return -1;
	}
}
