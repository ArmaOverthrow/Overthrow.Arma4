[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative admin chat commands for one player")]
class OVT_AdminCommandsComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative admin commands, on the per-player OVT_OverthrowController entity.
//!
//! First command: "/givemoney <amount>" adds money to the calling player's account so server
//! admins can legitimately seed the economy - buy stock from gun dealers into real storage
//! containers instead of spawning Game-Master arsenal boxes, whose deposited contents are never
//! saved (BUG-116: vanilla's Arsenal.conf has no storage serializer; anything players put in a GM
//! arsenal box silently vanishes on every restart).
//!
//! Project rule (overthrow-controller.md): new client->server operations live here, on a controller
//! component - the legacy comms monolith they used to ride was deleted in Phase 10.
//!
//! AUTHORITY MODEL. The chat command is registered on every client (the chat manager is a global
//! game core and command registration is not a permission), but the ONLY gate that matters is on
//! the server: SCR_Global.IsAdmin() against the engine's own role flags for the calling
//! connection. A modified client can send the RPC; it cannot make itself an admin.
//------------------------------------------------------------------------------------------------
class OVT_AdminCommandsComponent : OVT_ControllerRequestComponent
{
	//! Upper bound per command invocation. Not a security boundary (admins can repeat the command);
	//! it exists so a typo cannot overflow the int economy or produce a nonsense balance.
	protected const int GIVE_MONEY_MAX = 1000000;

	//! Whether this component has already put its callbacks into the chat command invokers.
	//!
	//! REQUIRED, NOT DEFENSIVE. Its caller fires once per ownership ASSIGNMENT, not once per player:
	//! OVT_PlayerManagerComponent.SetupPlayer() re-assigns on the already-mapped branch, so a reconnect
	//! or a Continue calls this a second time (see OVT_OverthrowController.RpcDo_NotifyOwnerAssignment).
	//! ChatCommandInvoker is a ScriptInvoker: a second Insert() of the same method is a second
	//! subscription, so "/givemoney 500" after a reconnect paid out twice and printed two audit lines.
	protected bool m_bChatCommandsRegistered;

	//------------------------------------------------------------------------------------------------
	//! Registers this component's chat commands on the owning client. Called from
	//! OVT_OverthrowController.RpcDo_NotifyOwnerAssignment - the one place that is both client-side and
	//! unambiguous about WHICH controller belongs to the local player.
	//!
	//! IDEMPOTENT BY CONTRACT, BY TWO INDEPENDENT MECHANISMS. Calling it twice registers ONE set of
	//! commands: the guard flag turns the second call into a no-op, and every subscription below is
	//! Remove()-then-Insert() so even a future edit that loses the flag cannot double-subscribe. Both,
	//! because the failure this prevents is silent - a doubly-registered "/givemoney 500" pays out
	//! $1000 and nothing anywhere says why.
	//!
	//! The flag is set only AFTER the chat manager resolves, so a call made before the chat panels
	//! exist is retried by the next ownership assignment rather than silently marked done.
	//! (chat.GetCommandInvoker() itself never answers null for a non-empty name - it creates the
	//! invoker on demand - so a null chat manager is the only way this bails.)
	void RegisterChatCommands()
	{
		if (m_bChatCommandsRegistered)
			return;

		SCR_ChatPanelManager chat = SCR_ChatPanelManager.GetInstance();
		if (!chat)
			return;

		m_bChatCommandsRegistered = true;

		// "/give-money" is the documented form (announced to server admins); the unhyphenated
		// variant is accepted because it is the obvious mistyping.
		ChatCommandInvoker invoker = chat.GetCommandInvoker("give-money");
		if (invoker)
		{
			invoker.Remove(OnGiveMoneyCommand);
			invoker.Insert(OnGiveMoneyCommand);
		}

		invoker = chat.GetCommandInvoker("givemoney");
		if (invoker)
		{
			invoker.Remove(OnGiveMoneyCommand);
			invoker.Insert(OnGiveMoneyCommand);
		}

		// Debug affordance for map/respawn, kept deliberately - see OnRespawnScreenCommand.
		invoker = chat.GetCommandInvoker("respawn-screen");
		if (invoker)
		{
			invoker.Remove(OnRespawnScreenCommand);
			invoker.Insert(OnRespawnScreenCommand);
		}

		invoker = chat.GetCommandInvoker("respawnscreen");
		if (invoker)
		{
			invoker.Remove(OnRespawnScreenCommand);
			invoker.Insert(OnRespawnScreenCommand);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! "/respawn-screen" toggles the respawn screen on the typing player's own machine.
	//!
	//! KEPT ON PURPOSE, not spike residue. The respawn screen is the one screen in the mod a player
	//! cannot open, cannot dismiss and can only reach by dying, and it is the screen with the most to
	//! verify by hand: the SPAWNSCREEN map's pan/zoom/cursor, eligible-only markers, the info panel,
	//! the gamepad glyphs. Reaching it by dying costs a full death-and-respawn cycle per look and
	//! changes world state on the way (the respawn charge, a lootable corpse). This costs nothing.
	//!
	//! It drives the SHIPPED path - OVT_RespawnScreenHandlerComponent on the local player controller -
	//! rather than a copy of it, so what it puts on screen is exactly what a death puts on screen,
	//! input contexts and map included. It sends no RPC and mutates no game state.
	//!
	//! Why no admin gate: it shows a screen to the caller and nothing else. The gated commands in this
	//! class mutate server state; this one cannot.
	//!
	//! ! THREE THINGS A TESTER MUST KNOW. The screen has no dismiss action by design, so typing the
	//! command again is the only way out - chat is reachable from it, because MapContext carries
	//! ChatToggle. Pressing a respawn button while ALIVE sends a real request that the server answers
	//! NOT_ELIGIBLE, because the player holds no awaiting-respawn claim: the reason appears on the
	//! status line, no character is created, and that is correct behaviour rather than a fault.
	//!
	//! ! And every command in this class is unreachable in SINGLE PLAYER, which is not this feature's
	//! doing but is the first thing a tester will trip over. RegisterChatCommands has exactly one
	//! caller - OVT_OverthrowController.RpcDo_NotifyOwnerAssignment, an RplRcver.Owner RPC - and in
	//! RplMode.None nothing is replicated, so the registration never runs and no command in this class
	//! exists. The same is likely true on a listen-server host, which is the known "a host never
	//! receives its own owner-targeted RPC" class this project short-circuits elsewhere. On a
	//! dedicated-server client the RPC arrives and the command works. Dying is the way in until that
	//! registration path grows the same short-circuit.
	//! \param[in] panel The chat panel the command was typed into (unused).
	//! \param[in] data Everything after the command word (unused).
	protected void OnRespawnScreenCommand(SCR_ChatPanel panel, string data)
	{
		OVT_RespawnScreenHandlerComponent handler = OVT_RespawnScreenHandlerComponent.GetLocalInstance();
		if (!handler)
		{
			Print("[Overthrow] /respawn-screen: no OVT_RespawnScreenHandlerComponent on the local player controller - check Prefabs/Characters/Core/OVT_PlayerController.et", LogLevel.ERROR);
			return;
		}

		if (handler.IsScreenShown())
		{
			handler.CloseRespawnScreen();
			return;
		}

		handler.ShowRespawnScreen();
	}

	//------------------------------------------------------------------------------------------------
	//! Chat callback for "/givemoney <amount>". Runs on the typing player's client.
	//! \param[in] panel The chat panel the command was typed into (unused).
	//! \param[in] data Everything after the command word.
	protected void OnGiveMoneyCommand(SCR_ChatPanel panel, string data)
	{
		data.TrimInPlace();
		int amount = data.ToInt();
		if (amount <= 0)
		{
			Print("[Overthrow] Usage: /givemoney <amount> - amount must be a positive number", LogLevel.WARNING);
			return;
		}

		RequestGiveMoney(amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the server to add money to the calling player's account. Admin-gated server-side.
	//! \param[in] amount How much to add. Clamped server-side to 1..GIVE_MONEY_MAX.
	void RequestGiveMoney(int amount)
	{
		if (Replication.IsServer())
		{
			RpcAsk_GiveMoney(amount);
		}
		else
		{
			Rpc(RpcAsk_GiveMoney, amount);
		}
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_GiveMoney(int amount)
	{
		if (!Replication.IsServer())
			return;

		int playerId = ResolveOwningPlayerId();
		if (playerId <= 0)
			return;

		OVT_NotificationManagerComponent notify = OVT_Global.GetNotify();

		// The one gate that counts: the engine's own role flags for this connection.
		if (!SCR_Global.IsAdmin(playerId))
		{
			Print(string.Format("[Overthrow] Player %1 used /givemoney without admin rights - refused", playerId), LogLevel.WARNING);
			if (notify)
				notify.SendTextNotification("AdminCommandRefused", playerId);
			return;
		}

		if (amount <= 0)
			return;
		if (amount > GIVE_MONEY_MAX)
			amount = GIVE_MONEY_MAX;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
			return;

		economy.AddPlayerMoney(playerId, amount, true);

		// Server console record: money was created from nothing, an audit line is the least it costs.
		Print(string.Format("[Overthrow] Admin (player %1) added $%2 to their account via /givemoney", playerId, amount), LogLevel.NORMAL);

		if (notify)
			notify.SendTextNotification("AdminFundsAdded", playerId, amount.ToString());
	}
}
