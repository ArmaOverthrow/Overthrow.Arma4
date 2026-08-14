class OVT_ControllerRequestComponentClass : OVT_ComponentClass {};

//------------------------------------------------------------------------------------------------
//! Shared base for SERVER-AUTHORITATIVE request components living on OVT_OverthrowController.
//!
//! It carries the four things every one of them needs and nothing else. There is deliberately no
//! domain state, no RPC and no lifecycle here: a request component is a thin seam onto a fat manager,
//! and anything that starts to look like gameplay logic on this hierarchy is a defect.
//!
//! THE IDENTITY MODEL, WHICH IS THE WHOLE REASON THIS BASE EXISTS. The legacy comms monolith lives on
//! the CALLER'S CHARACTER, so it answers "who asked?" with GetOwner(). A controller component lives on
//! the CALLER'S CONTROLLER ENTITY, so it answers with ResolveOwningPlayerId(). Both derive the caller
//! from the entity the RPC arrived on - never from a client-supplied id - and that is what makes an
//! identity parameter unnecessary and therefore unspoofable. Migrated handlers DROP the parameter from
//! the signature rather than ignoring it: an argument that does not exist cannot be trusted by a
//! future edit.
//!
//! WHY IT IS A BASE CLASS RATHER THAN A UTILITY CLASS OF STATICS. ResolveOwningPlayerId() is a question
//! about THIS component's owner entity; making it static would mean passing GetOwner() in at every call
//! site, which is the thing being removed. The three components that had private copies of these
//! helpers (shop transactions, admin commands, and the travel/respawn/tower trio) proved the copies
//! drift: the same method appeared with and without its guards.
//!
//! WHAT DOES NOT EXTEND THIS: OVT_BaseServerProgressComponent and its subclasses. Progress reporting
//! is a separate axis with its own owner test (IsLocalPlayerOwner(), which asks the player-manager map
//! rather than comparing ids), and EnforceScript has no multiple inheritance to combine them. That
//! hierarchy is deliberately left alone.
//------------------------------------------------------------------------------------------------
class OVT_ControllerRequestComponent : OVT_Component
{
	//------------------------------------------------------------------------------------------------
	//! Which player this controller belongs to, resolved on the SERVER from the controller entity this
	//! component sits on.
	//!
	//! This is the whole anti-spoofing story: a remote client can only reach a handler on the controller
	//! entity it owns, so the caller's identity comes from the entity, never from the payload. The scan
	//! is over CONNECTED players only, which is single digits.
	//!
	//! Answers -1 for anything unresolvable, including a call on a client (where the connected-player
	//! scan is meaningless) - every caller must treat "<= 0" as "reject the request".
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

	//------------------------------------------------------------------------------------------------
	//! RplId -> entity. RplId, never EntityID, is the only entity reference that means the same thing on
	//! both machines - an EntityID from a client names a different entity (or nothing) on the server.
	//! \param[in] rplId The networked reference sent by the client.
	//! \return The entity, or null when the id resolves to nothing (always a rejection, never a retry).
	protected IEntity ResolveEntity(RplId rplId)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(rplId));
		if(!rpl) return null;

		return rpl.GetEntity();
	}

	//------------------------------------------------------------------------------------------------
	//! The RplComponent of an entity, or null when it is not replicated (and therefore cannot be named
	//! across the network at all).
	//! \param[in] entity The entity to name.
	//! \return Its RplComponent, or null.
	protected RplComponent GetEntityRpl(IEntity entity)
	{
		if(!entity) return null;

		return RplComponent.Cast(entity.FindComponent(RplComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Whether an owner-targeted response to this player must be delivered by calling the handler
	//! DIRECTLY instead of sending the RPC.
	//!
	//! THE LISTEN-SERVER TRAP THIS EXISTS FOR (BUG-090 family): the engine never loops an Rpc back to
	//! the sender, so an RplRcver.Owner response sent by a listen-server host to its OWN controller is
	//! silently dropped and the host sees nothing at all. Every owner response therefore reads:
	//!
	//!     if (ShouldRespondLocally(playerId)) { RpcDo_Thing(a, b); return; }
	//!     Rpc(RpcDo_Thing, a, b);
	//!
	//! The Rpc() call itself stays at the call site on purpose - Rpc() is an untyped variadic prototype,
	//! so wrapping it here would hide an arity mistake that already compiles clean and dies at the wire.
	//!
	//! On a DEDICATED server GetLocalPlayerId() is 0, so this is always false and every response goes
	//! over the wire exactly as before.
	//! \param[in] playerId The player the response is aimed at.
	//! \return True when this machine's local player is the recipient.
	protected bool ShouldRespondLocally(int playerId)
	{
		return playerId > 0 && playerId == SCR_PlayerController.GetLocalPlayerId();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a player is allowed to act on a vehicle at all: an unlocked vehicle, or one with no
	//! recorded owner, is fair game; a locked vehicle answers only to its owner.
	//!
	//! Hoisted here in P2 from OVT_ShopTransactionComponent (where it was lifted from
	//! OVT_UnloadStorageAction) because the vehicle request component needs the identical rule for
	//! upgrade and repair. Two copies of "may this player touch this vehicle" is exactly the drift this
	//! base exists to prevent - the sell path and the upgrade path must never disagree about who owns a
	//! car.
	//!
	//! NOT a proximity check and not an ownership *claim* check. Callers add their own distance gate,
	//! and RpcAsk_SetVehicleLock/ClaimUnownedVehicle deliberately use the STRICTER "uid equality" rule
	//! instead: an unlocked vehicle may be repaired by a passer-by, but it may never be locked or
	//! claimed by one (BUG-087).
	//! \param[in] playerId The requesting player's runtime id.
	//! \param[in] vehicle The vehicle.
	//! \return True when the player may use this vehicle.
	protected bool PlayerMayUseVehicle(int playerId, IEntity vehicle)
	{
		if(!vehicle) return false;

		OVT_PlayerOwnerComponent playerOwner = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(vehicle);
		if(!playerOwner || !playerOwner.IsLocked()) return true;

		string ownerUid = playerOwner.GetPlayerOwnerUid();
		if(ownerUid == "") return true;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return false;

		return ownerUid == players.GetPersistentIDFromPlayerID(playerId);
	}
}
