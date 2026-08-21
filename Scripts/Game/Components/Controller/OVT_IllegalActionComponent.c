//------------------------------------------------------------------------------------------------
//! The illegal acts a player can be caught in the middle of. The client names one of these and
//! nothing else - what it costs and how long it lasts is decided server-side, in
//! OVT_IllegalActionComponent.
//------------------------------------------------------------------------------------------------
enum OVT_EIllegalAction
{
	//! Raising the flag in an occupied town (OVT_StartUprisingAction)
	UPRISING,
	//! Calling the assault on an occupying-faction base (OVT_CaptureBaseAction)
	BASE_ASSAULT
}

[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative illegal-action wanted escalation for one player")]
class OVT_IllegalActionComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Tells the server that this player has STARTED an illegal act that takes time to commit - the
//! timed hold actions that call an uprising or an assault on a base. Being seen anywhere in that
//! hold makes them wanted, exactly as sabotage and propaganda already do at the instant they
//! complete (OVT_TowerSabotageComponent, OVT_ResistanceFactionManager).
//!
//! WHY A WINDOW AND NOT A ONE-SHOT AT THE END. These are 15-second holds performed in the open, in
//! front of whoever is in the town or on the base. Judging them only on completion would mean a
//! patrol could watch the whole thing and the player would walk away clean if the patrol turned
//! away at the final second - and judging them only at the start would let a player begin while
//! unobserved and be joined by a patrol halfway through with no consequence. The window closes on
//! its own, and closes early if the player lets go of the key.
//!
//! Follows the controller discipline (overthrow-controller.md): no new client->server RPCs go on
//! the deprecated OVT_PlayerCommsComponent, the payload names a KIND of act rather than a wanted
//! level or a duration, and identity is resolved server-side from the controller entity this
//! component sits on (ResolveOwningPlayerId), never from the payload.
//!
//! Nothing here can hurt anyone but the sender: the worst a crafted packet achieves is making its
//! own player wanted.
//------------------------------------------------------------------------------------------------
class OVT_IllegalActionComponent : OVT_ControllerRequestComponent
{
	//! How long each act leaves the player catchable. Both holds are configured at 15 s
	//! (Duration on the actions in OVT_TownController.et / OVT_BaseController.et); the slack covers
	//! the round trip and the moment of completion itself.
	protected const int UPRISING_SECONDS = 20;
	protected const int BASE_ASSAULT_SECONDS = 20;

	//------------------------------------------------------------------------------------------------
	//! Tell the server an illegal hold has started.
	//!
	//! The authority never loops an RplRcver.Server RPC back to itself, so a listen host / SP player
	//! sending this would be sending it into a void (BUG-164) - the handler is already server-side,
	//! so run it in place instead.
	//! \param[in] action Which act, from OVT_EIllegalAction
	void ReportActionStarted(OVT_EIllegalAction action)
	{
		// Sent as a plain int: every RPC in this project carries primitives, and an untyped variadic
		// Rpc() gives no compile-time check that an enum would survive the wire (BUG-090)
		int actionId = action;

		if(Replication.IsServer())
			RpcAsk_ActionStarted(actionId);
		else
			Rpc(RpcAsk_ActionStarted, actionId);
	}

	//------------------------------------------------------------------------------------------------
	//! Tell the server the hold was abandoned before it completed.
	void ReportActionCancelled()
	{
		if(Replication.IsServer())
			RpcAsk_ActionCancelled();
		else
			Rpc(RpcAsk_ActionCancelled);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: open the seen-while-doing-it window on the requesting player's character.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_ActionStarted(int actionId)
	{
		if(!Replication.IsServer()) return;

		OVT_PlayerWantedComponent wanted = ResolveOwnerWanted();
		if(!wanted) return;

		string reason;
		int seconds;

		switch(actionId)
		{
			case OVT_EIllegalAction.UPRISING:
			{
				reason = "WantedUprising";
				seconds = UPRISING_SECONDS;
				break;
			}
			case OVT_EIllegalAction.BASE_ASSAULT:
			{
				reason = "WantedBaseAssault";
				seconds = BASE_ASSAULT_SECONDS;
				break;
			}
			default:
			{
				// An action kind this server does not know - nothing to charge the player with
				return;
			}
		}

		wanted.BeginIllegalAction(reason, seconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: close the window early.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_ActionCancelled()
	{
		if(!Replication.IsServer()) return;

		OVT_PlayerWantedComponent wanted = ResolveOwnerWanted();
		if(!wanted) return;

		wanted.EndIllegalAction();
	}

	//------------------------------------------------------------------------------------------------
	//! The wanted component of the character belonging to this controller, resolved entirely from
	//! server state.
	//! \return The component, or null when this controller has no live character.
	protected OVT_PlayerWantedComponent ResolveOwnerWanted()
	{
		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return null;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!character) return null;

		return OVT_PlayerWantedComponent.Cast(character.FindComponent(OVT_PlayerWantedComponent));
	}
}
