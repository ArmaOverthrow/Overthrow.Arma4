[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative admin chat commands for one player")]
class OVT_AdminCommandsComponentClass : OVT_ComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative admin commands, on the per-player OVT_OverthrowController entity.
//!
//! First command: "/givemoney <amount>" adds money to the calling player's account so server
//! admins can legitimately seed the economy - buy stock from gun dealers into real storage
//! containers instead of spawning Game-Master arsenal boxes, whose deposited contents are never
//! saved (BUG-116: vanilla's Arsenal.conf has no storage serializer; anything players put in a GM
//! arsenal box silently vanishes on every restart).
//!
//! Project rule (overthrow-controller.md): new client->server operations live here, never on the
//! legacy OVT_PlayerCommsComponent.
//!
//! AUTHORITY MODEL. The chat command is registered on every client (the chat manager is a global
//! game core and command registration is not a permission), but the ONLY gate that matters is on
//! the server: SCR_Global.IsAdmin() against the engine's own role flags for the calling
//! connection. A modified client can send the RPC; it cannot make itself an admin.
//------------------------------------------------------------------------------------------------
class OVT_AdminCommandsComponent : OVT_Component
{
	//! Upper bound per command invocation. Not a security boundary (admins can repeat the command);
	//! it exists so a typo cannot overflow the int economy or produce a nonsense balance.
	protected const int GIVE_MONEY_MAX = 1000000;

	//------------------------------------------------------------------------------------------------
	//! Registers this component's chat commands on the owning client. Called from
	//! OVT_OverthrowController.RpcDo_NotifyOwnerAssignment, which runs exactly once on the client
	//! that owns this controller - the one place that is both client-side and unambiguous about
	//! WHICH controller belongs to the local player.
	void RegisterChatCommands()
	{
		SCR_ChatPanelManager chat = SCR_ChatPanelManager.GetInstance();
		if (!chat)
			return;

		// "/give-money" is the documented form (announced to server admins); the unhyphenated
		// variant is accepted because it is the obvious mistyping.
		ChatCommandInvoker invoker = chat.GetCommandInvoker("give-money");
		if (invoker)
			invoker.Insert(OnGiveMoneyCommand);

		invoker = chat.GetCommandInvoker("givemoney");
		if (invoker)
			invoker.Insert(OnGiveMoneyCommand);
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
	//! The player id owning this controller, resolved server-side (same pattern as
	//! OVT_ShopTransactionComponent - the RPC caller is implied by which controller instance ran it).
	protected int ResolveOwningPlayerId()
	{
		OVT_OverthrowController owner = OVT_OverthrowController.Cast(GetOwner());
		if (!owner)
			return -1;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
			return -1;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return -1;

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);

		foreach (int playerId : playerIds)
		{
			if (players.GetController(playerId) == owner)
				return playerId;
		}

		return -1;
	}
}
