[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative admin chat commands for one player")]
class OVT_AdminCommandsComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative admin commands, on the per-player OVT_OverthrowController entity.
//!
//! Commands:
//!   "/give-money <amount>"      adds money to the calling player's account (admin-gated);
//!   "/give-resources [amount]"  credits the occupying faction's RESERVE (admin-gated) - see
//!                               OnGiveResourcesCommand for why the reserve and not the pool;
//!   "/tick-resources"           the same, but for exactly one tick's worth, computed rather than
//!                               typed (admin-gated) - see OnTickResourcesCommand;
//!   "/ruin-structure"           ruins the nearest built structure to the caller (admin-gated);
//!   "/repair-structure"         restores it (admin-gated);
//!   "/give-pool [amount]"       credits the deployment POOL directly, skipping the six-hour
//!                               drip that "/give-resources" is subject to (admin-gated);
//!   "/capture-base"             hands the nearest base to the resistance (admin-gated);
//!   "/capture-town"             hands the nearest town to the resistance (admin-gated);
//!   "/max-support"              takes the nearest town to 100 % support (admin-gated);
//!   "/respawn-screen"           toggles the local respawn screen (no gate, no state change).
//!
//! The three capture/support commands exist because the debug menu is unreachable from a real
//! multiplayer client, which is the only place an MP-only fault reproduces (author, 2026-08-24).
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

	//! How far "/ruin-structure" and "/repair-structure" look for a structure, measured from the
	//! caller's own character on the SERVER. Interaction range, not build range: the command is meant
	//! for the thing the admin is standing at.
	protected const float STRUCTURE_COMMAND_RADIUS = 15;

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

		// "/tick-resources" - no argument, so nothing to mistype, but it creates resources exactly as
		// "/give-resources" does and therefore needs the same Remove()-then-Insert() discipline.
		invoker = chat.GetCommandInvoker("tick-resources");
		if (invoker)
		{
			invoker.Remove(OnTickResourcesCommand);
			invoker.Insert(OnTickResourcesCommand);
		}

		invoker = chat.GetCommandInvoker("tickresources");
		if (invoker)
		{
			invoker.Remove(OnTickResourcesCommand);
			invoker.Insert(OnTickResourcesCommand);
		}

		// "/ruin-structure" and "/repair-structure" - same hyphenated/unhyphenated pairs and the same
		// Remove()-then-Insert() discipline as everything else here.
		invoker = chat.GetCommandInvoker("ruin-structure");
		if (invoker)
		{
			invoker.Remove(OnRuinStructureCommand);
			invoker.Insert(OnRuinStructureCommand);
		}

		invoker = chat.GetCommandInvoker("ruinstructure");
		if (invoker)
		{
			invoker.Remove(OnRuinStructureCommand);
			invoker.Insert(OnRuinStructureCommand);
		}

		invoker = chat.GetCommandInvoker("repair-structure");
		if (invoker)
		{
			invoker.Remove(OnRepairStructureCommand);
			invoker.Insert(OnRepairStructureCommand);
		}

		invoker = chat.GetCommandInvoker("repairstructure");
		if (invoker)
		{
			invoker.Remove(OnRepairStructureCommand);
			invoker.Insert(OnRepairStructureCommand);
		}

		// "/capture-base", "/capture-town", "/max-support" - the debug-menu equivalents for a client
		// that has no debug menu (a real MP client, which is the only place MP-only faults show up).
		// Same hyphenated/unhyphenated pairs and the same Remove()-then-Insert() as everything above.
		invoker = chat.GetCommandInvoker("capture-base");
		if (invoker)
		{
			invoker.Remove(OnCaptureBaseCommand);
			invoker.Insert(OnCaptureBaseCommand);
		}

		invoker = chat.GetCommandInvoker("capturebase");
		if (invoker)
		{
			invoker.Remove(OnCaptureBaseCommand);
			invoker.Insert(OnCaptureBaseCommand);
		}

		invoker = chat.GetCommandInvoker("capture-town");
		if (invoker)
		{
			invoker.Remove(OnCaptureTownCommand);
			invoker.Insert(OnCaptureTownCommand);
		}

		invoker = chat.GetCommandInvoker("capturetown");
		if (invoker)
		{
			invoker.Remove(OnCaptureTownCommand);
			invoker.Insert(OnCaptureTownCommand);
		}

		invoker = chat.GetCommandInvoker("max-support");
		if (invoker)
		{
			invoker.Remove(OnMaxSupportCommand);
			invoker.Insert(OnMaxSupportCommand);
		}

		invoker = chat.GetCommandInvoker("maxsupport");
		if (invoker)
		{
			invoker.Remove(OnMaxSupportCommand);
			invoker.Insert(OnMaxSupportCommand);
		}

		// "/give-pool" - the pool twin of "/give-resources". It CREATES resources, so it needs the same
		// Remove()-then-Insert() discipline as the other two creating commands.
		invoker = chat.GetCommandInvoker("give-pool");
		if (invoker)
		{
			invoker.Remove(OnGivePoolCommand);
			invoker.Insert(OnGivePoolCommand);
		}

		invoker = chat.GetCommandInvoker("givepool");
		if (invoker)
		{
			invoker.Remove(OnGivePoolCommand);
			invoker.Insert(OnGivePoolCommand);
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
	// CAPTURE + SUPPORT - "/capture-base", "/capture-town", "/max-support".
	//
	// The debug menu's equivalents, as chat commands, because the debug menu is not reachable from a
	// real multiplayer client - and a real client is the only place an MP-only fault reproduces
	// (author, 2026-08-24, hunting a horn that never sounds in single-player).
	//
	// ⚠ EACH ONE DRIVES THE SAME METHOD THE CAMPAIGN ITSELF DRIVES - ChangeBaseControl(),
	// ChangeTownControl(), AddSupport() - rather than writing the fields. A capture made from chat is
	// therefore the same capture in every respect: it notifies, it replicates and it saves. Writing
	// town.support directly would set it on the server only, and the client's HUD would disagree with
	// the server for the rest of the session.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Chat callback for "/capture-base". Runs on the typing player's client.
	//! \param[in] panel The chat panel the command was typed into (unused).
	//! \param[in] data Everything after the command word (unused).
	protected void OnCaptureBaseCommand(SCR_ChatPanel panel, string data)
	{
		RequestCaptureBase();
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the server to hand the nearest base to the resistance. Admin-gated server-side.
	void RequestCaptureBase()
	{
		if (Replication.IsServer())
			RpcAsk_CaptureBase();
		else
			Rpc(RpcAsk_CaptureBase);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_CaptureBase()
	{
		if (!Replication.IsServer())
			return;

		int playerId = ResolveOwningPlayerId();
		if (playerId <= 0)
			return;

		if (!AssertAdmin(playerId, "/capture-base"))
			return;

		vector pos;
		if (!ResolveCallerPosition(playerId, pos))
			return;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!occupying || !config)
			return;

		OVT_BaseData baseData = occupying.GetNearestBase(pos);
		if (!baseData)
		{
			Print(string.Format("[Overthrow] /capture-base: no base found near player %1", playerId), LogLevel.WARNING);
			return;
		}

		// The marker's controller, not the record: ChangeBaseControl drives the controller, which is
		// what owns the garrison, the map marker and the notification.
		OVT_BaseControllerComponent base = occupying.GetBase(baseData.entId);
		if (!base)
		{
			Print(string.Format("[Overthrow] /capture-base: the nearest base to player %1 has no live controller", playerId), LogLevel.WARNING);
			return;
		}

		int resistance = config.GetPlayerFactionIndex();
		if (baseData.faction == resistance)
		{
			Print(string.Format("[Overthrow] /capture-base: the nearest base to player %1 is already the resistance's", playerId), LogLevel.WARNING);
			return;
		}

		occupying.ChangeBaseControl(base, resistance);

		Print(string.Format("[Overthrow] Admin (player %1) captured the nearest base via /capture-base", playerId), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Chat callback for "/capture-town". Runs on the typing player's client.
	//! \param[in] panel The chat panel the command was typed into (unused).
	//! \param[in] data Everything after the command word (unused).
	protected void OnCaptureTownCommand(SCR_ChatPanel panel, string data)
	{
		RequestCaptureTown();
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the server to hand the nearest town to the resistance. Admin-gated server-side.
	void RequestCaptureTown()
	{
		if (Replication.IsServer())
			RpcAsk_CaptureTown();
		else
			Rpc(RpcAsk_CaptureTown);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_CaptureTown()
	{
		if (!Replication.IsServer())
			return;

		int playerId = ResolveOwningPlayerId();
		if (playerId <= 0)
			return;

		if (!AssertAdmin(playerId, "/capture-town"))
			return;

		vector pos;
		if (!ResolveCallerPosition(playerId, pos))
			return;

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!towns || !config)
			return;

		OVT_TownData town = towns.GetNearestTown(pos);
		if (!town)
		{
			Print(string.Format("[Overthrow] /capture-town: no town found near player %1", playerId), LogLevel.WARNING);
			return;
		}

		int resistance = config.GetPlayerFactionIndex();
		if (town.faction == resistance)
		{
			Print(string.Format("[Overthrow] /capture-town: the nearest town to player %1 is already the resistance's", playerId), LogLevel.WARNING);
			return;
		}

		towns.ChangeTownControl(town, resistance);

		Print(string.Format("[Overthrow] Admin (player %1) captured the nearest town via /capture-town", playerId), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Chat callback for "/max-support". Runs on the typing player's client.
	//! \param[in] panel The chat panel the command was typed into (unused).
	//! \param[in] data Everything after the command word (unused).
	protected void OnMaxSupportCommand(SCR_ChatPanel panel, string data)
	{
		RequestMaxSupport();
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the server to take the nearest town's support to 100 %. Admin-gated server-side.
	void RequestMaxSupport()
	{
		if (Replication.IsServer())
			RpcAsk_MaxSupport();
		else
			Rpc(RpcAsk_MaxSupport);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_MaxSupport()
	{
		if (!Replication.IsServer())
			return;

		int playerId = ResolveOwningPlayerId();
		if (playerId <= 0)
			return;

		if (!AssertAdmin(playerId, "/max-support"))
			return;

		vector pos;
		if (!ResolveCallerPosition(playerId, pos))
			return;

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
			return;

		OVT_TownData town = towns.GetNearestTown(pos);
		if (!town)
		{
			Print(string.Format("[Overthrow] /max-support: no town found near player %1", playerId), LogLevel.WARNING);
			return;
		}

		// ⚠ SUPPORT IS A HEADCOUNT, NOT A PERCENTAGE (OVT_TownData.SupportPercentage divides by
		// population). 100 % is therefore "every civilian", and AddSupport already clamps to the
		// population - so asking for the whole population is both the right number and unable to
		// overshoot it. It is also the one broadcasting write path, so clients agree.
		towns.AddSupport(pos, town.population);

		Print(string.Format("[Overthrow] Admin (player %1) took '%2' to %3 %% support via /max-support",
			playerId, towns.GetTownName(towns.GetTownID(town)), town.SupportPercentage()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Chat callback for "/give-pool [amount]". Runs on the typing player's client.
	//!
	//! THE POOL TWIN OF "/give-resources", and the difference is the whole point: that command credits
	//! the RESERVE, which reaches the pool only through the six-hour defense share paid one slice an
	//! hour, so a tester waits up to an in-game hour per slice. This lands in the pool immediately
	//! (author, 2026-08-24: "to save me waiting for the drip").
	//! \param[in] panel The chat panel the command was typed into (unused).
	//! \param[in] data Everything after the command word - an optional amount.
	protected void OnGivePoolCommand(SCR_ChatPanel panel, string data)
	{
		data.TrimInPlace();

		int amount = GIVE_RESOURCES_DEFAULT;
		if (data != "")
		{
			// Same text check as "/give-resources": ToInt() answers 0 for "abc" and parses the leading
			// digits of "50x", so the result alone cannot tell a typo from an amount.
			if (!IsPositiveInteger(data))
			{
				Print(string.Format("[Overthrow] Usage: /give-pool [amount] - '%1' is not a whole positive number (default %2)", data, GIVE_RESOURCES_DEFAULT), LogLevel.WARNING);
				return;
			}

			amount = data.ToInt();
		}

		if (amount <= 0)
		{
			Print(string.Format("[Overthrow] Usage: /give-pool [amount] - amount must be greater than zero (default %1)", GIVE_RESOURCES_DEFAULT), LogLevel.WARNING);
			return;
		}

		RequestGivePool(amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the server to credit the occupying faction's deployment pool. Admin-gated server-side.
	//! \param[in] amount Resources to credit. Re-validated and clamped on the server.
	void RequestGivePool(int amount)
	{
		if (Replication.IsServer())
			RpcAsk_GivePool(amount);
		else
			Rpc(RpcAsk_GivePool, amount);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_GivePool(int amount)
	{
		if (!Replication.IsServer())
			return;

		int playerId = ResolveOwningPlayerId();
		if (playerId <= 0)
			return;

		if (!AssertAdmin(playerId, "/give-pool"))
			return;

		// ⚠ RE-VALIDATED HERE, not trusted from the wire: the client-side parse is a convenience, this
		// is the bound. Same reasoning as every other amount in this file.
		if (amount <= 0)
			return;

		if (amount > GIVE_RESOURCES_MAX)
			amount = GIVE_RESOURCES_MAX;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!occupying || !deployments || !config)
			return;

		occupying.DebugCreditPool(amount);

		int pool = deployments.GetFactionResources(config.GetOccupyingFactionIndex());

		// Server console record: resources were created from nothing, an audit line is the least it costs.
		Print(string.Format("[Overthrow] Admin (player %1) credited %2 resources straight to the deployment pool via /give-pool - pool is now %3", playerId, amount, pool), LogLevel.NORMAL);

		OVT_NotificationManagerComponent notify = OVT_Global.GetNotify();
		if (notify)
			notify.SendTextNotification("AdminResourcesAdded", playerId, amount.ToString(), pool.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! The admin gate, shared by the capture/support/give-pool commands. The older commands still
	//! carry their own inline copy - deliberately not refactored mid-session; they are play-tested.
	//! \param[in] playerId The caller.
	//! \param[in] via The command name, for the log line.
	//! \return True when the caller may proceed.
	protected bool AssertAdmin(int playerId, string via)
	{
		if (SCR_Global.IsAdmin(playerId))
			return true;

		Print(string.Format("[Overthrow] Player %1 used %2 without admin rights - refused", playerId, via), LogLevel.WARNING);

		OVT_NotificationManagerComponent notify = OVT_Global.GetNotify();
		if (notify)
			notify.SendTextNotification("AdminCommandRefused", playerId);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the calling admin is standing. Everything here acts on "nearest to me".
	//! \param[in] playerId The caller.
	//! \param[out] pos Their position. Untouched on a false return.
	//! \return True when the caller has a body in the world.
	protected bool ResolveCallerPosition(int playerId, out vector pos)
	{
		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!character)
		{
			Print(string.Format("[Overthrow] Admin command: player %1 has no controlled entity", playerId), LogLevel.WARNING);
			return false;
		}

		pos = character.GetOrigin();
		return true;
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

		CreditAndDistribute(occupying, amount, playerId, "/give-resources", notify);
	}

	//------------------------------------------------------------------------------------------------
	//! Chat callback for "/tick-resources". Runs on the typing player's client. Takes no argument.
	//!
	//! WHY IT TAKES NO ARGUMENT AND CANNOT. The amount is one resource tick's gain, and that is a
	//! function of live SERVER state - the campaign's threat and the connected player count - so the
	//! client typing the command is not in a position to compute it. It is therefore resolved inside the
	//! RPC, on the server, and the client sends nothing but the request.
	//! \param[in] panel The chat panel the command was typed into (unused).
	//! \param[in] data Everything after the command word. Ignored, but reported if present rather than
	//!            silently swallowed - a tester who types "/tick-resources 500" expecting an amount
	//!            should be told it did something else.
	protected void OnTickResourcesCommand(SCR_ChatPanel panel, string data)
	{
		data.TrimInPlace();

		if (data != "")
			Print(string.Format("[Overthrow] /tick-resources takes no amount - ignoring '%1' and crediting exactly one tick's worth. Use /give-resources <amount> to choose the figure", data), LogLevel.WARNING);

		RequestTickResources();
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the server to credit exactly one resource tick's worth. Admin-gated server-side.
	void RequestTickResources()
	{
		if (Replication.IsServer())
		{
			RpcAsk_TickResources();
		}
		else
		{
			Rpc(RpcAsk_TickResources);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Credits and distributes exactly what the next resource tick would pay.
	//!
	//! ⚠ THE AMOUNT IS THE GM PANEL'S "Next Distribution" FIGURE, BY CONSTRUCTION RATHER THAN BY
	//! COINCIDENCE. It calls OVT_GMSchedule.PredictResourceGain() with the same four arguments
	//! OVT_GMRequestComponent hands it when building a snapshot - the difficulty's two per-tick
	//! numbers, the campaign threat and the connected player count - so the number credited here and
	//! the number on the panel cannot drift apart without one of them being edited. That is the whole
	//! request: "give what they would get on the next tick, as reported by Next Distribution".
	//!
	//! ⚠ AND IT IS THE SAME FUNCTION THE REAL TICK USES, not a reimplementation of it.
	//! OVT_OccupyingFactionManager.GainResources() calls PredictResourceGain() too - the prediction
	//! seam exists precisely so the panel can show what the tick will pay without running it. So this
	//! command is a real tick's worth, not an approximation of one.
	//!
	//! ⚠ IT DOES NOT HONOUR THE QRF SUPPRESSION, DELIBERATELY. A real tick is skipped while a battle is
	//! engaged (the panel flags this separately), but an admin asking for a tick has asked for one; a
	//! debug command that silently did nothing would be worse than one that overrides. The override is
	//! reported in the audit line rather than hidden.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TickResources()
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
			Print(string.Format("[Overthrow] Player %1 used /tick-resources without admin rights - refused", playerId), LogLevel.WARNING);
			if (notify)
				notify.SendTextNotification("AdminCommandRefused", playerId);
			return;
		}

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config || !config.m_Difficulty)
			return;

		int playerCount = 0;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager)
			playerCount = playerManager.GetPlayerCount();

		int amount = OVT_GMSchedule.PredictResourceGain(
			config.m_Difficulty.baseResourcesPerTick,
			config.m_Difficulty.resourcesPerTick,
			occupying.GetThreatFloat(),
			playerCount);

		// A difficulty authored with no income at all is a legitimate configuration, and "the next tick
		// pays nothing" is the honest answer to it rather than an error - but crediting zero and
		// printing an audit line about it would read as a broken command, so say so instead.
		if (amount <= 0)
		{
			Print(string.Format("[Overthrow] /tick-resources: the next tick would pay %1 - nothing credited. Threat is %2 with %3 player(s) online", amount, occupying.GetThreatFloat(), playerCount), LogLevel.WARNING);
			return;
		}

		if (amount > GIVE_RESOURCES_MAX)
			amount = GIVE_RESOURCES_MAX;

		if (occupying.m_CurrentQRF)
			Print("[Overthrow] /tick-resources: a battle is live, so the REAL tick would currently be suppressed - crediting anyway because an admin asked for it", LogLevel.WARNING);

		CreditAndDistribute(occupying, amount, playerId, "/tick-resources", notify);
	}

	//------------------------------------------------------------------------------------------------
	//! THE ONE CREDIT PATH BOTH RESOURCE COMMANDS TAKE. Credits the reserve, then distributes it as if a
	//! resource tick had just happened.
	//!
	//! AND DISTRIBUTE IT IMMEDIATELY, as if a resource tick had just happened.
	//!
	//! Crediting the reserve alone made "/give-resources" nearly useless for its actual purpose (user,
	//! during play-test): the deployment pool is what every visible thing spends, so a tester who
	//! wants the occupying faction to DO something had to credit, then wait out an in-game minute
	//! for the transfer, with nothing on screen explaining the delay.
	//!
	//! This is the organic path, not a shortcut past it: TransferDefenseShareToPool() is the same
	//! method the live tick and the sleep replay both call, it takes the tick's gain, applies the
	//! authored defense share, clamps to the reserve and moves the money through the one sanctioned
	//! credit point. So the accounting identity holds exactly as it does on any other tick, and this
	//! adds NO new caller to AllocateDeploymentResources - which is the thing that must never grow a
	//! fourth one without a reason written down.
	//!
	//! ⚠ EXTRACTED RATHER THAN COPIED when "/tick-resources" was added (2026-08-19). The two commands
	//! differ ONLY in where the number comes from - one is typed, one is computed - and a second copy of
	//! the credit-then-transfer pair is exactly the kind of duplication that lets one of them quietly
	//! stop matching a real tick.
	//! \param[in] occupying The occupying faction manager.
	//! \param[in] amount How much to credit. Already validated and clamped by the caller.
	//! \param[in] playerId The admin who asked, for the audit line and the notification.
	//! \param[in] via Which command did this, for the audit line.
	//! \param[in] notify The notification manager, or null.
	protected void CreditAndDistribute(notnull OVT_OccupyingFactionManager occupying, int amount, int playerId, string via, OVT_NotificationManagerComponent notify)
	{
		occupying.DebugCreditReserve(amount);

		occupying.TransferDefenseShareToPool(amount);

		int reserve = occupying.m_iResources;

		// Server console record: resources were created from nothing, an audit line is the least it costs.
		Print(string.Format("[Overthrow] Admin (player %1) credited %2 resources to the occupying faction and ran a distribution via %3 - reserve is now %4", playerId, amount, via, reserve), LogLevel.NORMAL);

		if (notify)
			notify.SendTextNotification("AdminResourcesAdded", playerId, amount.ToString(), reserve.ToString());
	}

	//------------------------------------------------------------------------------------------------
	// STRUCTURE DESTRUCTION - "/ruin-structure" and "/repair-structure".
	//
	// The admin route into core/damage: they drive OVT_StructureDamage exactly as sabotage and the
	// repair action do, so a ruin made from chat is the same ruin in every respect - it saves, it
	// replicates and it repairs. They are also how the feature is play-tested at all, since a ruin
	// otherwise only appears after the occupying faction sabotages a built structure.
	//------------------------------------------------------------------------------------------------

	//! Best structure found by the query in flight, and how far away it was. One command runs at a time.
	protected IEntity m_QueryStructure;
	protected vector m_vQueryOrigin;
	protected float m_fQueryBestDistance;

	//------------------------------------------------------------------------------------------------
	//! Chat callback for "/ruin-structure". Takes no argument: the target is resolved server-side from
	//! the caller's own character, never from client-supplied aim.
	//! \param[in] panel The chat panel the command was typed into (unused).
	//! \param[in] data Everything after the command word (unused).
	protected void OnRuinStructureCommand(SCR_ChatPanel panel, string data)
	{
		RequestRuinStructure();
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the server to ruin the structure the caller is standing at. Admin-gated server-side.
	void RequestRuinStructure()
	{
		if (Replication.IsServer())
		{
			RpcAsk_RuinStructure();
		}
		else
		{
			Rpc(RpcAsk_RuinStructure);
		}
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RuinStructure()
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
			Print(string.Format("[Overthrow] Player %1 used /ruin-structure without admin rights - refused", playerId), LogLevel.WARNING);
			if (notify)
				notify.SendTextNotification("AdminCommandRefused", playerId);
			return;
		}

		IEntity structure = FindNearestStructure(playerId);
		if (!structure)
		{
			Print(string.Format("[Overthrow] /ruin-structure: player %1 has no destructible built structure within %2 m", playerId, STRUCTURE_COMMAND_RADIUS), LogLevel.WARNING);
			return;
		}

		if (OVT_StructureDamage.IsRuined(structure))
		{
			Print(string.Format("[Overthrow] /ruin-structure: the nearest structure to player %1 is already a ruin", playerId), LogLevel.WARNING);
			return;
		}

		OVT_StructureDamage.Ruin(structure);

		Print(string.Format("[Overthrow] Admin (player %1) ruined a structure via /ruin-structure", playerId), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Chat callback for "/repair-structure". Same target resolution as "/ruin-structure".
	//! \param[in] panel The chat panel the command was typed into (unused).
	//! \param[in] data Everything after the command word (unused).
	protected void OnRepairStructureCommand(SCR_ChatPanel panel, string data)
	{
		RequestRepairStructure();
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the server to repair the ruin the caller is standing at. Admin-gated server-side.
	void RequestRepairStructure()
	{
		if (Replication.IsServer())
		{
			RpcAsk_RepairStructure();
		}
		else
		{
			Rpc(RpcAsk_RepairStructure);
		}
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RepairStructure()
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
			Print(string.Format("[Overthrow] Player %1 used /repair-structure without admin rights - refused", playerId), LogLevel.WARNING);
			if (notify)
				notify.SendTextNotification("AdminCommandRefused", playerId);
			return;
		}

		IEntity structure = FindNearestStructure(playerId);
		if (!structure)
		{
			Print(string.Format("[Overthrow] /repair-structure: player %1 has no destructible built structure within %2 m", playerId, STRUCTURE_COMMAND_RADIUS), LogLevel.WARNING);
			return;
		}

		if (!OVT_StructureDamage.IsRuined(structure))
		{
			Print(string.Format("[Overthrow] /repair-structure: the nearest structure to player %1 is not a ruin", playerId), LogLevel.WARNING);
			return;
		}

		OVT_StructureDamage.Repair(structure);

		Print(string.Format("[Overthrow] Admin (player %1) repaired a structure via /repair-structure", playerId), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER-SIDE TARGET RESOLUTION. Nearest built structure to the caller's own character that
	//! actually carries a destruction component. No entity parameter and no client-supplied position:
	//! no command in this class takes one and none should start.
	//! \param[in] playerId The calling admin.
	//! \return The structure to drive through OVT_StructureDamage, or null.
	protected IEntity FindNearestStructure(int playerId)
	{
		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!character)
			return null;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return null;

		m_QueryStructure = null;
		m_vQueryOrigin = character.GetOrigin();
		m_fQueryBestDistance = STRUCTURE_COMMAND_RADIUS + 1;

		world.QueryEntitiesBySphere(
			m_vQueryOrigin,
			STRUCTURE_COMMAND_RADIUS,
			CollectStructureCallback,
			FilterBuildableCallback,
			EQueryEntitiesFlags.ALL);

		IEntity found = m_QueryStructure;
		m_QueryStructure = null;

		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Filter callback - built structures only.
	//! \param[in] entity The entity being offered.
	//! \return True to pass it to the collect callback.
	protected bool FilterBuildableCallback(IEntity entity)
	{
		if (!entity)
			return false;

		return OVT_BuildableComponent.Cast(entity.FindComponent(OVT_BuildableComponent)) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Collect callback - keeps the nearest structure that has something to drive.
	//! \param[in] entity The entity that passed the filter.
	//! \return Always true, to keep searching.
	protected bool CollectStructureCallback(IEntity entity)
	{
		if (!entity)
			return true;

		float distance = vector.Distance(entity.GetOrigin(), m_vQueryOrigin);
		if (distance >= m_fQueryBestDistance)
			return true;

		if (!OVT_StructureDamage.IsDestructible(entity))
			return true;

		m_fQueryBestDistance = distance;
		m_QueryStructure = entity;

		return true;
	}
}
