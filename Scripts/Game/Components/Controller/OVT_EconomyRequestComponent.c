[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative money, tax and skill requests for one player")]
class OVT_EconomyRequestComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative money movement, on the per-player OVT_OverthrowController entity.
//!
//! Phase 4 of the controller migration (docs/features/core/controller-migration/implementation.md §4).
//! Replaced seven handlers and one response on the legacy comms monolith (deleted in Phase 10): drug
//! selling, donate-to-resistance, send-resistance-funds, send-money-to-player, take-player-money (plus
//! its done reply), set-resistance-tax and buy-skill. Project rule (overthrow-controller.md): every
//! client->server RPC lives on a controller component like this one.
//!
//! Shop BUYING and SELLING are deliberately NOT here - they ride OVT_ShopTransactionComponent, which
//! owns the shop's price model, stock table and 30 m gate (plan D5). This component is the money seam:
//! everything on it moves a balance without touching an inventory, except the one drug sale that is a
//! dealer interaction rather than a shop.
//!
//! WHAT CHANGED IN THE MOVE:
//!
//! 1. NO REQUEST CARRIES AN IDENTITY ARGUMENT (plan G3/D3). Every handler here used to take a
//!    playerId/fromPlayerId that ResolveSenderPlayerId() then overwrote. The parameter is DELETED, not
//!    ignored. What remains is `toPlayerId` on the two transfer handlers - a TARGET, not the caller, and
//!    it is validated as such (see the BUG-016 note on ResolveTransferTarget).
//!
//! 2. THREE NOTIFICATIONS START WORKING (plan §3.6(b)). PlayerDonated, PlayerSentFunds and
//!    PlayerSentMoney were sent through the legacy comms monolith's SendNotification(), which was an
//!    unconditional Rpc() to its own server-received notification handler. All three of its callers
//!    were inside server-side handlers, and an RplRcver.Server RPC marshalled by the authority is
//!    delivered to nobody (the BUG-045/052/088 family), so none of the three notifications has ever
//!    reached a screen. They are now direct OVT_NotificationManagerComponent calls.
//!
//! 3. THE PUBLIC ENTRY POINTS BRANCH ON Replication.IsServer(). The monolith's did not, so on a
//!    LISTEN-SERVER HOST - who is the authority - every one of these requests was an RplRcver.Server Rpc
//!    marshalled by the server and therefore delivered to nobody. Donating, sending money, setting the
//!    tax and buying a skill silently did nothing for a host. Same class of defect as P2-5.
//------------------------------------------------------------------------------------------------
class OVT_EconomyRequestComponent : OVT_ControllerRequestComponent
{
	//! How far the selling player may be from the dealer before the server rejects a drug sale (the
	//! user action requires interaction range, plus latency/movement slack). Carried verbatim.
	protected const float DEALER_MAX_DISTANCE = 10;

	//! CLIENT-SIDE money-glitch latch. Set when a TakePlayerMoney request goes out and cleared by
	//! RpcDo_DoneTakingMoney, so a mashed button cannot send the same debit twice before the first one
	//! has landed. Carried from the legacy comms monolith's takingMoney - and so is the reason
	//! RpcAsk_TakePlayerMoney ALWAYS replies: a handler that returns early without answering leaves this
	//! latched true forever and the player can never spend again.
	protected bool m_bTakingMoney = false;

	//------------------------------------------------------------------------------------------------
	// PUBLIC ENTRY POINTS - client side.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Sell the drugs the local player is carrying to a street dealer.
	//! \param[in] dealer The dealer entity the action was performed on.
	void SellDrugs(IEntity dealer)
	{
		RplComponent rpl = GetEntityRpl(dealer);
		if(!rpl) return;

		if(Replication.IsServer())
		{
			RpcAsk_SellDrugs(rpl.Id());
		}else{
			Rpc(RpcAsk_SellDrugs, rpl.Id());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Donate the local player's own money to the resistance funds.
	//! \param[in] amount How much.
	void DonateToResistance(int amount)
	{
		if(amount <= 0) return;

		if(Replication.IsServer())
		{
			RpcAsk_DonateToResistance(amount);
		}else{
			Rpc(RpcAsk_DonateToResistance, amount);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Send resistance funds to another player. Officer only, enforced server-side.
	//! \param[in] toPlayerId Runtime id of the recipient.
	//! \param[in] amount How much.
	void SendResistanceFunds(int toPlayerId, int amount)
	{
		if(amount <= 0) return;

		if(Replication.IsServer())
		{
			RpcAsk_SendResistanceFunds(toPlayerId, amount);
		}else{
			Rpc(RpcAsk_SendResistanceFunds, toPlayerId, amount);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Send the local player's own money to another player.
	//! \param[in] toPlayerId Runtime id of the recipient.
	//! \param[in] amount How much.
	void SendMoneyToPlayer(int toPlayerId, int amount)
	{
		if(amount <= 0) return;

		if(Replication.IsServer())
		{
			RpcAsk_SendMoneyToPlayer(toPlayerId, amount);
		}else{
			Rpc(RpcAsk_SendMoneyToPlayer, toPlayerId, amount);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Debit the local player's own balance.
	//!
	//! The one-in-flight latch is the money-glitch guard carried from the monolith. On a listen-server
	//! host the handler runs in-process and replies before this returns, so the latch is already clear
	//! again by the time the next click can happen.
	//! \param[in] amount How much to take.
	void TakePlayerMoney(int amount)
	{
		if(amount <= 0) return;
		if(m_bTakingMoney) return;

		m_bTakingMoney = true;

		if(Replication.IsServer())
		{
			RpcAsk_TakePlayerMoney(amount);
		}else{
			Rpc(RpcAsk_TakePlayerMoney, amount);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Set the resistance tax rate. Officer only, enforced server-side.
	//! \param[in] amount The new rate, clamped to 0..1 server-side.
	void SetResistanceTax(float amount)
	{
		if(Replication.IsServer())
		{
			RpcAsk_SetResistanceTax(amount);
		}else{
			Rpc(RpcAsk_SetResistanceTax, amount);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Spend one of the local player's skill points.
	//! \param[in] key The skill's config key.
	void BuySkill(string key)
	{
		if(key == "") return;

		if(Replication.IsServer())
		{
			RpcAsk_BuySkill(key);
		}else{
			Rpc(RpcAsk_BuySkill, key);
		}
	}

	//------------------------------------------------------------------------------------------------
	// SERVER HANDLERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: sell one carried stack of weed to a nearby dealer.
	//!
	//! Carried verbatim, including the deliberate `break` after the first match: this sells ONE item per
	//! action, which is what the hold action's repeat cadence is balanced around. The DrugsWeed_01
	//! substring filter and the 1.25x dealer premium are also carried as-is - both are gameplay, and
	//! this migration does not change gameplay (plan G6).
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SellDrugs(RplId dealerId)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;

		// The user action's proximity gate is client-side only - re-check it here
		IEntity dealer = ResolveEntity(dealerId);
		if(!dealer) return;

		if(vector.Distance(player.GetOrigin(), dealer.GetOrigin()) > DEALER_MAX_DISTANCE) return;

		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(player.FindComponent(SCR_InventoryStorageManagerComponent));
		if(!inventory) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		array<IEntity> items = new array<IEntity>;
		inventory.GetItems(items);

		foreach(IEntity ent : items)
		{
			if(!ent) continue;

			ResourceName res = OVT_Global.GetPrefabName(ent);
			if(!res.Contains("DrugsWeed_01")) continue;

			int id = economy.GetInventoryId(res);
			if(inventory.TryDeleteItem(ent))
			{
				int cost = economy.GetBuyPrice(id, player.GetOrigin()) * 1.25;
				economy.DoAddPlayerMoney(playerId, cost);
			}

			break;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: move the caller's own money into the resistance funds.
	//!
	//! §3.6(b) fix: the PlayerDonated notification is sent directly. It was previously routed through
	//! the comms component's SendNotification(), which is an Rpc to an RplRcver.Server handler, called
	//! from here - i.e. from the authority - so it has never fired.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_DonateToResistance(int amount)
	{
		if(!Replication.IsServer()) return;

		if(amount <= 0) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return;

		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if(persId == "") return;

		if(!economy.PlayerHasMoney(persId, amount))
		{
			RejectEconomyRequest(playerId, "donate", "insufficient funds");
			return;
		}

		economy.DoTakePlayerMoney(playerId, amount);
		economy.DoAddResistanceMoney(amount);

		Notify("PlayerDonated", -1, players.GetPlayerName(playerId), amount.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: move money out of the resistance funds to a named player.
	//!
	//! Officer-gated (the menu's gate is client-side only) and bounded by what the resistance actually
	//! holds. The recipient is resolved through ResolveTransferTarget - see the BUG-016 note there.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SendResistanceFunds(int toPlayerId, int amount)
	{
		if(!Replication.IsServer()) return;

		if(amount <= 0) return;

		int fromPlayerId = ResolveOwningPlayerId();
		if(fromPlayerId <= 0) return;

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if(!resistance) return;

		if(!resistance.IsOfficer(fromPlayerId))
		{
			RejectEconomyRequest(fromPlayerId, "send resistance funds", "the caller is not an officer");
			return;
		}

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		if(!economy.ResistanceHasMoney(amount))
		{
			RejectEconomyRequest(fromPlayerId, "send resistance funds", "the resistance cannot afford it");
			return;
		}

		if(!ResolveTransferTarget(toPlayerId))
		{
			RejectEconomyRequest(fromPlayerId, "send resistance funds", "no player record for the named recipient");
			return;
		}

		economy.DoTakeResistanceMoney(amount);
		economy.DoAddPlayerMoney(toPlayerId, amount);

		Notify("PlayerSentFunds", toPlayerId, amount.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: move the caller's own money to another player.
	//!
	//! The caller is the controller's owner, so "from" cannot be spoofed; "to" is a runtime id from the
	//! client and is validated through ResolveTransferTarget.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SendMoneyToPlayer(int toPlayerId, int amount)
	{
		if(!Replication.IsServer()) return;

		if(amount <= 0) return;

		int fromPlayerId = ResolveOwningPlayerId();
		if(fromPlayerId <= 0) return;

		if(fromPlayerId == toPlayerId) return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return;

		string fromPersId = players.GetPersistentIDFromPlayerID(fromPlayerId);
		if(fromPersId == "") return;

		if(!economy.PlayerHasMoney(fromPersId, amount))
		{
			RejectEconomyRequest(fromPlayerId, "send money", "insufficient funds");
			return;
		}

		if(!ResolveTransferTarget(toPlayerId))
		{
			RejectEconomyRequest(fromPlayerId, "send money", "no player record for the named recipient");
			return;
		}

		economy.DoTakePlayerMoney(fromPlayerId, amount);
		economy.DoAddPlayerMoney(toPlayerId, amount);

		Notify("PlayerSentMoney", toPlayerId, players.GetPlayerName(fromPlayerId), amount.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: debit the caller's own balance.
	//!
	//! ALWAYS REPLIES. This is a contract, not a courtesy: the client latches m_bTakingMoney before
	//! sending and only RpcDo_DoneTakingMoney clears it, so any path out of here that does not answer
	//! locks that player out of every future debit for the rest of the session. Hence the single reply
	//! at the end and no early returns after the caller has been resolved.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TakePlayerMoney(int amount)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		if(amount > 0)
		{
			OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
			if(economy)
				economy.DoTakePlayerMoney(playerId, amount);
		}

		// On a listen server the requester IS this machine's local player, and an Owner-targeted RPC to
		// ourselves is never delivered - so the handler is called directly and nothing is sent.
		if(ShouldRespondLocally(playerId))
		{
			RpcDo_DoneTakingMoney();
			return;
		}

		Rpc(RpcDo_DoneTakingMoney);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the debit has landed; the next one may go out.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_DoneTakingMoney()
	{
		m_bTakingMoney = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: set the resistance tax rate.
	//!
	//! The tax slider's officer gate is client-side only, so it is re-derived here. Unlike the monolith
	//! there is no "senderId of -1 means server-initiated" escape hatch: this component only ever exists
	//! on a player's controller, so an unresolvable caller is a rejection. Server-side code that wants to
	//! set the tax calls OVT_EconomyManagerComponent.DoSetResistanceTax() directly.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SetResistanceTax(float amount)
	{
		if(!Replication.IsServer()) return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if(!resistance) return;

		if(!resistance.IsOfficer(playerId))
		{
			RejectEconomyRequest(playerId, "set resistance tax", "the caller is not an officer");
			return;
		}

		if(amount < 0) amount = 0;
		if(amount > 1) amount = 1;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return;

		economy.DoSetResistanceTax(amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: spend one of the caller's skill points.
	//!
	//! DELIBERATELY THIN. OVT_SkillManagerComponent.AddSkillLevel already re-derives spendable points,
	//! the level cap and whether the key names a real skill (BUG-032,
	//! OVT_SkillManagerComponent.c:116-125). Duplicating those here would create a second copy of the
	//! rule free to drift from the manager's; the seam's only job is to say WHO asked.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_BuySkill(string key)
	{
		if(!Replication.IsServer()) return;

		if(key == "") return;

		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0) return;

		OVT_SkillManagerComponent skills = OVT_Global.GetSkills();
		if(!skills) return;

		skills.AddSkillLevel(playerId, key);
	}

	//------------------------------------------------------------------------------------------------
	// HELPERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether a client-named recipient is a real, currently-known player.
	//!
	//! BUG-016 AND RISK R4, WHICH IS THE WHOLE POINT OF THIS METHOD. `toPlayerId` is a RUNTIME id, and
	//! runtime ids are session-scoped and REUSED: a stale one held by a client's player spinner can name
	//! a different joiner than the one the sender picked. The mitigation the monolith already used and
	//! this carries verbatim is to resolve runtime id -> persistent id and confirm the persistent record
	//! exists IN THE SAME TICK as the transfer, never caching the result across frames. It cannot make
	//! an aliased id name the right person, but it does guarantee money never lands on a record that is
	//! not there.
	//! \param[in] toPlayerId Runtime id supplied by the client.
	//! \return True when a player record exists for that id right now.
	protected bool ResolveTransferTarget(int toPlayerId)
	{
		if(toPlayerId <= 0) return false;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return false;

		string toPersId = players.GetPersistentIDFromPlayerID(toPlayerId);
		if(toPersId == "") return false;

		return players.GetPlayer(toPersId) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Sends a notification directly through the notification manager.
	//!
	//! §3.6(b): this replaced the legacy comms monolith's SendNotification(), which wrapped exactly this
	//! call in an Rpc to an RplRcver.Server handler. Every one of its callers was server-side, so the
	//! wrapper only ever managed to drop the message. It was also, as a network endpoint, an unvalidated
	//! "any client may show any notification to any player" - which is why it is deleted rather than
	//! moved (plan §3.7/D6).
	//! \param[in] tag Notification tag from the notification config.
	//! \param[in] playerId Recipient, or -1 for everybody.
	//! \param[in] param1 First substitution.
	//! \param[in] param2 Second substitution.
	protected void Notify(string tag, int playerId, string param1 = "", string param2 = "")
	{
		OVT_NotificationManagerComponent notify = OVT_Global.GetNotify();
		if(notify)
			notify.SendTextNotification(tag, playerId, param1, param2);
	}

	//------------------------------------------------------------------------------------------------
	//! Logs a rejected money request with its reason (quality bar Q9: a rejection is never
	//! indistinguishable from a dropped packet).
	//!
	//! Logs rather than notifies because every one of these rejections is already gated client-side by
	//! the menu that sends it - a player who has never seen a message here should not start seeing one.
	//! \param[in] playerId The rejected player.
	//! \param[in] request Which request was rejected.
	//! \param[in] reason Why.
	protected void RejectEconomyRequest(int playerId, string request, string reason)
	{
		Print(string.Format("[OVT_EconomyRequestComponent] Rejected %1 request from player %2: %3", request, playerId.ToString(), reason), LogLevel.WARNING);
	}
}
