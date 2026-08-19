[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative admin chat commands for one player")]
class OVT_AdminCommandsComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative admin commands, on the per-player OVT_OverthrowController entity.
//!
//! Commands:
//!   "/give-money <amount>"      adds money to the calling player's account (admin-gated);
//!   "/give-resources [amount]"  credits the occupying faction's RESERVE (admin-gated) - see
//!                               OnGiveResourcesCommand for why the reserve and not the pool;
//!   "/respawn-screen"           toggles the local respawn screen (no gate, no state change).
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
//!
//! ONE EXCEPTION TO "every client", and it is a product rule rather than a security one: a SHIPPED
//! single-player campaign registers nothing at all, because the engine makes an offline player an
//! admin by default and these commands are not meant to be part of a normal solo game. A Workbench
//! play-test still gets them. See the note on RegisterChatCommands() for the exact mechanism.
//------------------------------------------------------------------------------------------------
class OVT_AdminCommandsComponent : OVT_ControllerRequestComponent
{
	//! Upper bound per command invocation. Not a security boundary (admins can repeat the command);
	//! it exists so a typo cannot overflow the int economy or produce a nonsense balance.
	protected const int GIVE_MONEY_MAX = 1000000;

	//! Upper bound per "/give-resources" invocation, same reasoning as GIVE_MONEY_MAX: not security,
	//! just a typo bound. A whole campaign's opening budget is in the low thousands, so 100000 is
	//! already far past anything a tester needs in one go.
	protected const int GIVE_RESOURCES_MAX = 100000;

	//! What "/give-resources" credits when the tester types no amount - roughly a couple of deployments'
	//! worth, enough to see the occupying faction react without flooding the pool.
	protected const int GIVE_RESOURCES_DEFAULT = 2000;

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
	//!
	//! THE SHIPPED-SINGLE-PLAYER REFUSAL below is the ONE place the rule "no admin commands in a normal
	//! single-player game" is enforced, and it therefore covers EVERY command in this class - the two
	//! that exist today ("/give-money", "/give-resources", the latter added 2026-08-19) and every one
	//! added later. Add a command by adding a subscription here and it is governed automatically; do
	//! not re-implement the rule per command.
	//!
	//! WHY THE SHAPE IS A RUNTIME TEST INSIDE A COMPILE-TIME GUARD, and not either one alone:
	//!  - #ifdef WORKBENCH alone would strip the commands from every SHIPPED build, including the
	//!    dedicated servers whose admins this class was written for (see the header at the top).
	//!  - the runtime test alone would also refuse the Workbench single-player play-test, which is the
	//!    session developers actually test these commands in.
	//! Intersecting them refuses exactly one context - a shipped, offline campaign - and leaves the
	//! dedicated-server client, the listen host and the Workbench play-test untouched.
	//!
	//! RplMode.None IS the offline predicate, and it is the engine's, not a heuristic of ours: the enum
	//! is None/Client/Listen/Dedicated (ArmaReforger scripts/GameLib/generated/RplMode.c), a LISTEN HOST
	//! reports RplMode.Listen and so is never caught here, and vanilla itself spells this exact
	//! comparison "isSingleplayer" (scripts/Game/Plugins/Persistence/System/Serializers/Components/
	//! Character/SCR_CharacterCameraHandlerComponentSerializer.c:13). Overthrow already relies on the
	//! same enum separating a host from single player at OVT_OverthrowGameMode.c:222.
	//!
	//! IT RETURNS BEFORE THE GUARD FLAG, DELIBERATELY. Refusing must not consume the one-shot: leaving
	//! m_bChatCommandsRegistered false keeps the flag meaning "the commands ARE subscribed" rather than
	//! "somebody called this once", which is what the Remove()/Insert() pairs below rely on. The usual
	//! danger of an early return that does not latch - a later call in a different context sneaking the
	//! registration through after all - cannot happen here, because RplSession.Mode() is fixed for the
	//! lifetime of a session: an offline session never becomes a hosted one without a new session, so
	//! every retry of this method re-reads the same answer and refuses again.
	void RegisterChatCommands()
	{
#ifndef WORKBENCH
		// Shipped build, offline session: no admin commands at all. Compiled out of the Workbench
		// binary entirely, so a play-test never reaches this line.
		//
		// ! COMPILE-CHECK BLIND SPOT: tools/compile-check.sh runs IN Workbench, so WORKBENCH is defined
		// and these two lines are never compiled by our gate. An edit here can be syntactically broken
		// and still pass. The statement as written was proved to compile by temporarily unguarding it
		// (2026-08-19); do the same if you change it.
		if (RplSession.Mode() == RplMode.None)
			return;
#endif

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

		// "/give-resources" - same hyphenated/unhyphenated pair, and covered by the same guard flag and
		// the same Remove()-then-Insert() as "/give-money": it creates resources, so a double
		// subscription would double the credit exactly as it once doubled the payout.
		invoker = chat.GetCommandInvoker("give-resources");
		if (invoker)
		{
			invoker.Remove(OnGiveResourcesCommand);
			invoker.Insert(OnGiveResourcesCommand);
		}

		invoker = chat.GetCommandInvoker("giveresources");
		if (invoker)
		{
			invoker.Remove(OnGiveResourcesCommand);
			invoker.Insert(OnGiveResourcesCommand);
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
	//! ! WHERE THE COMMANDS IN THIS CLASS EXIST - corrected 2026-08-19. This block used to claim they
	//! were unreachable in single player and that "dying is the way in". THAT WAS WRONG, and it was
	//! wrong by inference rather than by test: OBSERVED, in a Workbench single-player play-test,
	//! "/give-money" is present and pays out (user report, 2026-08-19). Do not re-derive this the hard
	//! way. The mechanism is the engine's RPC routing table (ArmaReforger
	//! scripts/GameLib/replication/RplDocs.c:549-557 + the example at :1844-1853): the sole caller,
	//! OVT_OverthrowController.RpcDo_NotifyOwnerAssignment, is an RplRcver.Owner RPC sent BY the
	//! authority, and when the authority is also the owner - a single-player session, and by the same
	//! rule a listen host - the engine invokes the body directly instead of putting it on a wire. So
	//! the registration runs on a dedicated-server client (over the wire), in single player (locally),
	//! and, INFERRED FROM THE TABLE BUT NOT YET OBSERVED, on a listen-server host.
	//!
	//! That same play-test also proves the server-side gate passes there: SCR_Global.IsAdmin() answers
	//! true for the local player of an offline session, so a single-player player is an admin as far as
	//! the engine's role flags are concerned - which is precisely why a SHIPPED single-player campaign
	//! would otherwise hand every player a money cheat.
	//!
	//! ! SO THE RULE IS NOW: registered on a dedicated-server client, on a listen host, and in a
	//! WORKBENCH single-player play-test; refused in a shipped single-player campaign, and only there.
	//! The one enforcement point is the #ifndef WORKBENCH / RplMode.None refusal at the top of
	//! RegisterChatCommands() - read the note there before changing anything about it, and do not add a
	//! second copy of the rule to an individual command.
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

	//------------------------------------------------------------------------------------------------
	//! Chat callback for "/give-resources [amount]". Runs on the typing player's client.
	//!
	//! WHAT IT CREDITS, AND WHY THE POOL DOES NOT MOVE IMMEDIATELY. The occupying faction has two
	//! accounts: the RESERVE (OVT_OccupyingFactionManager.m_iResources, which also sizes QRFs) and the
	//! DEPLOYMENT POOL that the deployment framework spends. This command credits the RESERVE only.
	//! The pool has exactly one credit point - AllocateDeploymentResources() - with three callers and a
	//! written rule against a fourth, so a debug command must not become one. The reserve reaches the
	//! pool on the next resource tick, when TransferDefenseShareToPool() moves 80 % of it across; that
	//! is within about a minute of game time, and it is the same route every legitimate resource takes.
	//! A tester who watches the pool for an instant jump will see nothing and must not read that as a
	//! failure - the confirmation message says so.
	//! \param[in] panel The chat panel the command was typed into (unused).
	//! \param[in] data Everything after the command word. Empty means GIVE_RESOURCES_DEFAULT.
	protected void OnGiveResourcesCommand(SCR_ChatPanel panel, string data)
	{
		data.TrimInPlace();

		int amount = GIVE_RESOURCES_DEFAULT;
		if (data != "")
		{
			// ToInt() answers 0 for "abc" and parses the leading digits of "50x", so neither a typo nor
			// a pasted word can be told from a real amount by its result alone - check the text instead.
			if (!IsPositiveInteger(data))
			{
				Print(string.Format("[Overthrow] Usage: /give-resources [amount] - '%1' is not a whole positive number (default %2)", data, GIVE_RESOURCES_DEFAULT), LogLevel.WARNING);
				return;
			}

			amount = data.ToInt();
		}

		if (amount <= 0)
		{
			Print(string.Format("[Overthrow] Usage: /give-resources [amount] - amount must be greater than zero (default %1)", GIVE_RESOURCES_DEFAULT), LogLevel.WARNING);
			return;
		}

		if (amount > GIVE_RESOURCES_MAX)
		{
			Print(string.Format("[Overthrow] /give-resources: %1 is above the per-command limit of %2 - run the command again if you really need more", amount, GIVE_RESOURCES_MAX), LogLevel.WARNING);
			return;
		}

		RequestGiveResources(amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether every character of a trimmed string is a decimal digit, and there is at least one.
	//! Deliberately rejects signs, spaces, decimal points and thousands separators: this is a chat
	//! argument, and anything that is not plain digits is a typo worth reporting rather than guessing.
	//! \param[in] text Trimmed candidate.
	//! \return True if text is a non-empty run of 0-9.
	protected bool IsPositiveInteger(string text)
	{
		int length = text.Length();
		if (length == 0)
			return false;

		for (int i = 0; i < length; i++)
		{
			int code = text.ToAscii(i);
			if (code < 48 || code > 57)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the server to credit the occupying faction's reserve. Admin-gated server-side.
	//! \param[in] amount How much to credit. Clamped server-side to 1..GIVE_RESOURCES_MAX.
	void RequestGiveResources(int amount)
	{
		if (Replication.IsServer())
		{
			RpcAsk_GiveResources(amount);
		}
		else
		{
			Rpc(RpcAsk_GiveResources, amount);
		}
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_GiveResources(int amount)
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
			Print(string.Format("[Overthrow] Player %1 used /give-resources without admin rights - refused", playerId), LogLevel.WARNING);
			if (notify)
				notify.SendTextNotification("AdminCommandRefused", playerId);
			return;
		}

		if (amount <= 0)
			return;
		if (amount > GIVE_RESOURCES_MAX)
			amount = GIVE_RESOURCES_MAX;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return;

		occupying.DebugCreditReserve(amount);

		// AND DISTRIBUTE IT IMMEDIATELY, as if a resource tick had just happened.
		//
		// Crediting the reserve alone made this command nearly useless for its actual purpose (user,
		// during play-test): the deployment pool is what every visible thing spends, so a tester who
		// wants the occupying faction to DO something had to credit, then wait out an in-game minute
		// for the transfer, with nothing on screen explaining the delay.
		//
		// This is the organic path, not a shortcut past it: TransferDefenseShareToPool() is the same
		// method the live tick and the sleep replay both call, it takes the tick's gain, applies the
		// authored defense share, clamps to the reserve and moves the money through the one sanctioned
		// credit point. So the accounting identity holds exactly as it does on any other tick, and this
		// adds NO new caller to AllocateDeploymentResources - which is the thing that must never grow a
		// fourth one without a reason written down.
		occupying.TransferDefenseShareToPool(amount);

		int reserve = occupying.m_iResources;

		// Server console record: resources were created from nothing, an audit line is the least it costs.
		Print(string.Format("[Overthrow] Admin (player %1) credited %2 resources to the occupying faction and ran a distribution via /give-resources - reserve is now %3", playerId, amount, reserve), LogLevel.NORMAL);

		if (notify)
			notify.SendTextNotification("AdminResourcesAdded", playerId, amount.ToString(), reserve.ToString());
	}
}
