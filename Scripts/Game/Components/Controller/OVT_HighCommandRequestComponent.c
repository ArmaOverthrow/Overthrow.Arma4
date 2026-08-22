[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative High Command requests (quote, purchase, order, convert, dismiss) for one player")]
class OVT_HighCommandRequestComponentClass : OVT_ControllerRequestComponentClass {};

//------------------------------------------------------------------------------------------------
//! The ONLY client->server seam for High Command (implementation.md §3.6). Quote, purchase, order,
//! convert and dismiss are all live. Every ask always answers RpcDo_HCResult - a silent return is a
//! defect.
//!
//! No request carries an identity argument - the caller is always ResolveOwningPlayerId().
//!
//! THE PURCHASE GATE LADDER IS ORDERED AND THE FUNDS CHECK IS LAST. A player must never be told
//! "you cannot afford this" for a group the cap or the town would have refused anyway, and the
//! effects run spawn -> stock -> supporters -> money so a failed spawn charges nothing.
//------------------------------------------------------------------------------------------------
class OVT_HighCommandRequestComponent : OVT_ControllerRequestComponent
{
	//! The request was carried out.
	static const int RESULT_OK = 0;
	//! The caller's identity could not be resolved from its controller entity.
	static const int RESULT_NO_PLAYER = 1;
	//! The caller has no controlled character in the world.
	static const int RESULT_NO_ACTOR = 2;
	//! No OVT_BarracksComponent within range of the caller.
	static const int RESULT_NOT_AT_BARRACKS = 3;
	//! The barracks exists but is ruined.
	static const int RESULT_BARRACKS_RUINED = 4;
	//! The nearest base is not friendly to the player faction.
	static const int RESULT_BASE_NOT_FRIENDLY = 5;
	//! entryIndex does not name a purchasable OVT_HighCommandGroupEntry.
	static const int RESULT_BAD_ENTRY = 6;
	//! The purchase or conversion would exceed the owner's member cap.
	static const int RESULT_AT_CAP = 7;
	//! The nearest town has no supporter to give.
	static const int RESULT_NO_SUPPORTERS = 8;
	//! The quote is more than the player has.
	static const int RESULT_CANNOT_AFFORD = 9;
	//! The group failed to spawn; nothing was charged.
	static const int RESULT_SPAWN_FAILED = 10;
	//! groupId does not name a live High Command group.
	static const int RESULT_NO_GROUP = 11;
	//! The group exists but belongs to another player.
	static const int RESULT_NOT_OWNER = 12;
	//! stance does not name a real OVT_EHighCommandStance.
	static const int RESULT_BAD_STANCE = 13;
	//! destination is not a legal order destination.
	static const int RESULT_BAD_DESTINATION = 14;
	//! anchorRecruitId does not resolve to an inactive recruit group.
	static const int RESULT_NO_RECRUIT_GROUP = 15;
	//! Converting the named recruit group would exceed the owner's HC member cap.
	static const int RESULT_CONVERT_AT_CAP = 16;

	//! Purchase-path notification tags. A tag with no preset in Configs/overthrowBroadcastMessages.conf
	//! is silently dropped (BUG-120), so every one of these is authored there.
	static const string NOTIFY_PURCHASED = "HCGroupPurchased";
	static const string NOTIFY_NOT_AT_BARRACKS = "HCPurchaseNotAtBarracks";
	static const string NOTIFY_BARRACKS_RUINED = "HCPurchaseBarracksRuined";
	static const string NOTIFY_BASE_NOT_FRIENDLY = "HCPurchaseBaseNotFriendly";
	static const string NOTIFY_BAD_ENTRY = "HCPurchaseBadEntry";
	static const string NOTIFY_AT_CAP = "HCPurchaseAtCap";
	static const string NOTIFY_NO_SUPPORTERS = "HCPurchaseNoSupporters";
	static const string NOTIFY_CANNOT_AFFORD = "HCPurchaseCannotAfford";
	static const string NOTIFY_SPAWN_FAILED = "HCPurchaseSpawnFailed";

	//! CLIENT: fires when a quote answer arrives, whatever it says. Args match RpcDo_HCQuote - the
	//! screen must compare entryIndex against what it is showing, since a fast selection change can
	//! make an older answer arrive after a newer one.
	ref ScriptInvoker m_OnHCQuote = new ScriptInvoker();

	//! CLIENT: fires when any ask this component sent is answered. Args match RpcDo_HCResult.
	ref ScriptInvoker m_OnHCResult = new ScriptInvoker();

	//------------------------------------------------------------------------------------------------
	//! Ask the server to quote the price of an authored group entry.
	//! \param[in] entryIndex Index into the FIA faction's m_aHighCommandGroups.
	void RequestQuoteGroup(int entryIndex)
	{
		if (Replication.IsServer())
			RpcAsk_QuoteGroup(entryIndex);
		else
			Rpc(RpcAsk_QuoteGroup, entryIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! Ask the server to purchase an authored group entry at the barracks the caller is standing at.
	//! \param[in] entryIndex Index into the FIA faction's m_aHighCommandGroups.
	void RequestPurchaseGroup(int entryIndex)
	{
		if (Replication.IsServer())
			RpcAsk_PurchaseGroup(entryIndex);
		else
			Rpc(RpcAsk_PurchaseGroup, entryIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! Ask the server to order an owned group to a destination and stance.
	//! \param[in] groupId The group to order.
	//! \param[in] stance An OVT_EHighCommandStance ordinal.
	//! \param[in] destination Where to send it.
	void RequestOrderGroup(string groupId, int stance, vector destination)
	{
		if (Replication.IsServer())
			RpcAsk_OrderGroup(groupId, stance, destination);
		else
			Rpc(RpcAsk_OrderGroup, groupId, stance, destination);
	}

	//------------------------------------------------------------------------------------------------
	//! Ask the server to convert an inactive recruit group into a High Command group.
	//! \param[in] anchorRecruitId Any recruit id belonging to the target inactive group.
	void RequestConvertRecruitGroup(string anchorRecruitId)
	{
		if (Replication.IsServer())
			RpcAsk_ConvertRecruitGroup(anchorRecruitId);
		else
			Rpc(RpcAsk_ConvertRecruitGroup, anchorRecruitId);
	}

	//------------------------------------------------------------------------------------------------
	//! Ask the server to dismiss an owned group.
	//! \param[in] groupId The group to dismiss.
	void RequestDismissGroup(string groupId)
	{
		if (Replication.IsServer())
			RpcAsk_DismissGroup(groupId);
		else
			Rpc(RpcAsk_DismissGroup, groupId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: quote an authored group entry - the first eight gates of the purchase ladder, then the
	//! price. No funds check: a quote is what tells a player whether they can afford it.
	//!
	//! The quote is ADVISORY (D19). Warehouse stock moves, so the purchase re-derives this from
	//! scratch and charges that instead; the screen labels its number as an estimate.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_QuoteGroup(int entryIndex)
	{
		if (!Replication.IsServer())
			return;

		int playerId = ResolveOwningPlayerId();
		if (playerId <= 0)
		{
			ReplyQuote(playerId, entryIndex, 0, 0, 0, 0, RESULT_NO_PLAYER);
			return;
		}

		IEntity actor = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!actor)
		{
			ReplyQuote(playerId, entryIndex, 0, 0, 0, 0, RESULT_NO_ACTOR);
			return;
		}

		vector position = actor.GetOrigin();

		OVT_BarracksComponent barracks = OVT_BarracksComponent.FindNearest(position, OVT_BarracksComponent.BARRACKS_USE_RADIUS);
		if (!barracks)
		{
			ReplyQuote(playerId, entryIndex, 0, 0, 0, 0, RESULT_NOT_AT_BARRACKS);
			return;
		}

		if (!OVT_StructureDamage.IsUsable(barracks.GetOwner()))
		{
			ReplyQuote(playerId, entryIndex, 0, 0, 0, 0, RESULT_BARRACKS_RUINED);
			return;
		}

		if (!IsAtFriendlyBase(position))
		{
			ReplyQuote(playerId, entryIndex, 0, 0, 0, 0, RESULT_BASE_NOT_FRIENDLY);
			return;
		}

		OVT_HighCommandManagerComponent manager = OVT_Global.GetHighCommand();
		if (!manager)
		{
			ReplyQuote(playerId, entryIndex, 0, 0, 0, 0, RESULT_BAD_ENTRY);
			return;
		}

		OVT_HighCommandGroupEntry entry = manager.GetEntryByIndex(entryIndex);
		if (!entry || entry.m_sKey == "")
		{
			ReplyQuote(playerId, entryIndex, 0, 0, 0, 0, RESULT_BAD_ENTRY);
			return;
		}

		string persistentId = ResolveCallerPersistentId(playerId);
		if (persistentId == "")
		{
			ReplyQuote(playerId, entryIndex, 0, 0, 0, 0, RESULT_NO_PLAYER);
			return;
		}

		OVT_HighCommandQuote quote = manager.QuoteEntry(entry, position, persistentId, playerId);
		if (!quote)
		{
			ReplyQuote(playerId, entryIndex, 0, 0, 0, 0, RESULT_BAD_ENTRY);
			return;
		}

		if (!OVT_HighCommandRules.FitsUnderCap(manager.GetMemberCount(persistentId), quote.m_iMemberCount, manager.GetMemberCap()))
		{
			ReplyQuote(playerId, entryIndex, quote.m_iTotal, quote.m_iCoveredValue, quote.m_iMemberCount, quote.m_iSupportersRequired, RESULT_AT_CAP);
			return;
		}

		if (!NearestTownCanSupport(position, quote.m_iSupportersRequired))
		{
			ReplyQuote(playerId, entryIndex, quote.m_iTotal, quote.m_iCoveredValue, quote.m_iMemberCount, quote.m_iSupportersRequired, RESULT_NO_SUPPORTERS);
			return;
		}

		ReplyQuote(playerId, entryIndex, quote.m_iTotal, quote.m_iCoveredValue, quote.m_iMemberCount, quote.m_iSupportersRequired, RESULT_OK);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: purchase an authored group entry at the barracks the caller is standing at.
	//!
	//! THE NINE-STEP LADDER, IN ORDER (§3.6): no player, no actor, not at a barracks, barracks ruined,
	//! base not friendly, bad entry, at cap, no supporters, cannot afford. Funds LAST.
	//!
	//! THEN THE EFFECTS, IN ORDER: spawn -> consume warehouse stock -> take supporters -> take money.
	//! Nothing is charged until the group is standing in the world.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_PurchaseGroup(int entryIndex)
	{
		if (!Replication.IsServer())
			return;

		int playerId = ResolveOwningPlayerId();
		if (playerId <= 0)
		{
			ReplyPurchase(playerId, RESULT_NO_PLAYER, 0, "", 0);
			return;
		}

		IEntity actor = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!actor)
		{
			ReplyPurchase(playerId, RESULT_NO_ACTOR, 0, "", 0);
			return;
		}

		// The caller's own position, not the barracks origin: the player is standing within
		// BARRACKS_USE_RADIUS on ground they walked to, which the inside of a building is not.
		vector position = actor.GetOrigin();

		OVT_BarracksComponent barracks = OVT_BarracksComponent.FindNearest(position, OVT_BarracksComponent.BARRACKS_USE_RADIUS);
		if (!barracks)
		{
			ReplyPurchase(playerId, RESULT_NOT_AT_BARRACKS, 0, "", 0);
			return;
		}

		if (!OVT_StructureDamage.IsUsable(barracks.GetOwner()))
		{
			ReplyPurchase(playerId, RESULT_BARRACKS_RUINED, 0, "", 0);
			return;
		}

		if (!IsAtFriendlyBase(position))
		{
			ReplyPurchase(playerId, RESULT_BASE_NOT_FRIENDLY, 0, "", 0);
			return;
		}

		OVT_HighCommandManagerComponent manager = OVT_Global.GetHighCommand();
		if (!manager)
		{
			ReplyPurchase(playerId, RESULT_BAD_ENTRY, 0, "", 0);
			return;
		}

		OVT_HighCommandGroupEntry entry = manager.GetEntryByIndex(entryIndex);
		if (!entry || entry.m_sKey == "")
		{
			ReplyPurchase(playerId, RESULT_BAD_ENTRY, 0, "", 0);
			return;
		}

		string persistentId = ResolveCallerPersistentId(playerId);
		if (persistentId == "")
		{
			ReplyPurchase(playerId, RESULT_NO_PLAYER, 0, "", 0);
			return;
		}

		// Re-derived here and nowhere else: what is checked is what is charged (D19).
		OVT_HighCommandQuote quote = manager.QuoteEntry(entry, position, persistentId, playerId);
		if (!quote)
		{
			ReplyPurchase(playerId, RESULT_BAD_ENTRY, 0, "", 0);
			return;
		}

		if (!OVT_HighCommandRules.FitsUnderCap(manager.GetMemberCount(persistentId), quote.m_iMemberCount, manager.GetMemberCap()))
		{
			ReplyPurchase(playerId, RESULT_AT_CAP, 0, "", quote.m_iMemberCount);
			return;
		}

		if (!NearestTownCanSupport(position, quote.m_iSupportersRequired))
		{
			ReplyPurchase(playerId, RESULT_NO_SUPPORTERS, 0, "", quote.m_iSupportersRequired);
			return;
		}

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy || !economy.PlayerHasMoney(persistentId, quote.m_iTotal))
		{
			ReplyPurchase(playerId, RESULT_CANNOT_AFFORD, 0, "", quote.m_iTotal);
			return;
		}

		// The barracks the ladder already found, not a second query. A host that authors no points
		// hands back null and the group spawns at the caller's position, as it always has (R8).
		OVT_SpawnPointComponent spawnPoints;
		IEntity barracksEntity = barracks.GetOwner();
		if (barracksEntity)
			spawnPoints = OVT_SpawnPointComponent.Cast(barracksEntity.FindComponent(OVT_SpawnPointComponent));

		OVT_HighCommandRecord record = manager.SpawnGroupFromEntry(entry.m_sKey, position, persistentId, null, spawnPoints);
		if (!record)
		{
			ReplyPurchase(playerId, RESULT_SPAWN_FAILED, 0, "", 0);
			return;
		}

		manager.ConsumeQuotedStock(quote);

		if (quote.m_iSupportersRequired > 0)
		{
			OVT_TownManagerComponent towns = OVT_Global.GetTowns();
			if (towns)
				towns.TakeSupportersFromNearestTown(position, quote.m_iSupportersRequired);
		}

		economy.TakePlayerMoneyPersistentId(persistentId, quote.m_iTotal);

		ReplyPurchase(playerId, RESULT_OK, quote.m_iTotal, record.m_sGroupId, quote.m_iMemberCount);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: order an owned group to a destination and stance. Phase 6 adds the broadcast that tells
	//! every other client about it.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_OrderGroup(string groupId, int stance, vector destination)
	{
		if (!Replication.IsServer())
			return;

		int playerId = ResolveOwningPlayerId();
		if (playerId <= 0)
		{
			ReplyResult(playerId, RESULT_NO_PLAYER, 0, groupId);
			return;
		}

		OVT_HighCommandManagerComponent manager = OVT_Global.GetHighCommand();
		if (!manager)
		{
			ReplyResult(playerId, RESULT_NO_GROUP, 0, groupId);
			return;
		}

		OVT_HighCommandRecord record = manager.GetGroup(groupId);
		if (!record)
		{
			ReplyResult(playerId, RESULT_NO_GROUP, 0, groupId);
			return;
		}

		if (record.m_sOwnerPersistentId != ResolveCallerPersistentId(playerId))
		{
			ReplyResult(playerId, RESULT_NOT_OWNER, 0, groupId);
			return;
		}

		if (!OVT_HighCommandRules.IsStanceValid(stance))
		{
			ReplyResult(playerId, RESULT_BAD_STANCE, 0, groupId);
			return;
		}

		if (!OVT_HighCommandRules.IsDestinationLegal(destination))
		{
			ReplyResult(playerId, RESULT_BAD_DESTINATION, 0, groupId);
			return;
		}

		if (!manager.OrderGroup(groupId, stance, destination))
		{
			ReplyResult(playerId, RESULT_NO_GROUP, 0, groupId);
			return;
		}

		ReplyResult(playerId, RESULT_OK, 0, groupId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: convert the inactive recruit group the named recruit is parked in (T9.1-T9.3).
	//!
	//! THE LADDER, IN ORDER: no player, no recruit record, not the caller's recruit, not parked in one
	//! of our inactive groups, nothing collectable in it, over the member cap. The cap is checked
	//! BEFORE anything moves, so a refusal costs the player nothing.
	//!
	//! ONE WAY. There is no ask, here or anywhere, that turns a High Command group back into recruits.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_ConvertRecruitGroup(string anchorRecruitId)
	{
		if (!Replication.IsServer())
			return;

		int playerId = ResolveOwningPlayerId();
		if (playerId <= 0)
		{
			ReplyResult(playerId, RESULT_NO_PLAYER, 0, "");
			return;
		}

		string persistentId = ResolveCallerPersistentId(playerId);
		if (persistentId == "")
		{
			ReplyResult(playerId, RESULT_NO_PLAYER, 0, "");
			return;
		}

		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		OVT_HighCommandManagerComponent manager = OVT_Global.GetHighCommand();
		if (!recruits || !manager)
		{
			ReplyResult(playerId, RESULT_NO_RECRUIT_GROUP, 0, "");
			return;
		}

		OVT_RecruitData recruit = recruits.GetRecruit(anchorRecruitId);
		if (!recruit)
		{
			ReplyResult(playerId, RESULT_NO_RECRUIT_GROUP, 0, "");
			return;
		}

		if (recruit.m_sOwnerPersistentId != persistentId)
		{
			ReplyResult(playerId, RESULT_NOT_OWNER, 0, "");
			return;
		}

		// An ACTIVE recruit stands in its owner's slave group, which carries no inactive marker, so it
		// answers null here - only a parked squad can be converted.
		SCR_AIGroup hostGroup = manager.FindInactiveRecruitGroup(anchorRecruitId);
		if (!hostGroup)
		{
			ReplyResult(playerId, RESULT_NO_RECRUIT_GROUP, 0, "");
			return;
		}

		array<string> recruitIds = {};
		array<IEntity> bodies = {};
		int incoming = manager.CollectInactiveGroupRecruits(persistentId, hostGroup, recruitIds, bodies);
		if (incoming < 1)
		{
			ReplyResult(playerId, RESULT_NO_RECRUIT_GROUP, 0, "");
			return;
		}

		if (!OVT_HighCommandRules.FitsUnderCap(manager.GetMemberCount(persistentId), incoming, manager.GetMemberCap()))
		{
			ReplyResult(playerId, RESULT_CONVERT_AT_CAP, 0, "");
			return;
		}

		OVT_HighCommandRecord record = manager.ConvertRecruitGroup(persistentId, recruitIds, bodies);
		if (!record)
		{
			ReplyResult(playerId, RESULT_SPAWN_FAILED, 0, "");
			return;
		}

		ReplyResult(playerId, RESULT_OK, 0, record.m_sGroupId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: dismiss an owned group - members, vehicle, waypoints, observer and record. Phase 6 adds
	//! the broadcast that removes it from every other client's map.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_DismissGroup(string groupId)
	{
		if (!Replication.IsServer())
			return;

		int playerId = ResolveOwningPlayerId();
		if (playerId <= 0)
		{
			ReplyResult(playerId, RESULT_NO_PLAYER, 0, groupId);
			return;
		}

		OVT_HighCommandManagerComponent manager = OVT_Global.GetHighCommand();
		if (!manager)
		{
			ReplyResult(playerId, RESULT_NO_GROUP, 0, groupId);
			return;
		}

		OVT_HighCommandRecord record = manager.GetGroup(groupId);
		if (!record)
		{
			ReplyResult(playerId, RESULT_NO_GROUP, 0, groupId);
			return;
		}

		if (record.m_sOwnerPersistentId != ResolveCallerPersistentId(playerId))
		{
			ReplyResult(playerId, RESULT_NOT_OWNER, 0, groupId);
			return;
		}

		if (!manager.DismissGroup(groupId))
		{
			ReplyResult(playerId, RESULT_NO_GROUP, 0, groupId);
			return;
		}

		ReplyResult(playerId, RESULT_OK, 0, groupId);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the caller is standing at a base the resistance holds - the ManageBase / FuelDepot rule.
	//! \param[in] position The caller's position.
	//! \return True when the nearest base is within baseRange and is not the occupying faction's.
	protected bool IsAtFriendlyBase(vector position)
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return false;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config || !config.m_Difficulty)
			return false;

		OVT_BaseData base = occupying.GetNearestBase(position);
		if (!base)
			return false;

		if (base.IsOccupyingFaction())
			return false;

		return vector.Distance(base.location, position) < config.m_Difficulty.baseRange;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the nearest town can give up the supporters a purchase needs.
	//!
	//! A rate of ZERO must never refuse. NearestTownHasSupporters answers false when there is no town
	//! at all, which for a draw-down of nothing would refuse a purchase that costs the town nothing.
	//! \param[in] position The caller's position.
	//! \param[in] required How many supporters the purchase draws down.
	//! \return True when the purchase may proceed.
	protected bool NearestTownCanSupport(vector position, int required)
	{
		if (required <= 0)
			return true;

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
			return false;

		return towns.NearestTownHasSupporters(position, required);
	}

	//------------------------------------------------------------------------------------------------
	//! The one exit of RpcAsk_QuoteGroup. A refusal answers the quote row AND RpcDo_HCResult - the
	//! screen draws the row, the generic reason handler reads the result. No path returns silently.
	//! \param[in] playerId The caller.
	//! \param[in] entryIndex The entry that was asked about.
	//! \param[in] total The quoted price, or 0 when there is none.
	//! \param[in] coveredValue What nearby warehouse stock covers, at buy price.
	//! \param[in] memberCount How many members the entry buys.
	//! \param[in] supportersNeeded How many town supporters it draws down.
	//! \param[in] refusalCode RESULT_OK, or why the entry cannot be bought here.
	protected void ReplyQuote(int playerId, int entryIndex, int total, int coveredValue, int memberCount, int supportersNeeded, int refusalCode)
	{
		if (ShouldRespondLocally(playerId))
			RpcDo_HCQuote(entryIndex, total, coveredValue, memberCount, supportersNeeded, refusalCode);
		else
			Rpc(RpcDo_HCQuote, entryIndex, total, coveredValue, memberCount, supportersNeeded, refusalCode);

		if (refusalCode != RESULT_OK)
			ReplyResult(playerId, refusalCode, 0, "");
	}

	//------------------------------------------------------------------------------------------------
	//! The one exit of RpcAsk_PurchaseGroup: the result RPC, then the notification for it.
	//! \param[in] playerId The caller.
	//! \param[in] resultCode One of this class's RESULT_* constants.
	//! \param[in] charged How much was charged; 0 for every refusal.
	//! \param[in] groupId The new group's id, or "" when none was created.
	//! \param[in] detail The number the notification quotes back - members, supporters or price.
	protected void ReplyPurchase(int playerId, int resultCode, int charged, string groupId, int detail)
	{
		ReplyResult(playerId, resultCode, charged, groupId);

		string tag = PurchaseNotificationTag(resultCode);
		if (tag == "")
			return;

		OVT_NotificationManagerComponent notify = OVT_Global.GetNotify();
		if (!notify)
			return;

		if (resultCode == RESULT_OK)
			notify.SendTextNotification(tag, playerId, detail.ToString(), charged.ToString());
		else
			notify.SendTextNotification(tag, playerId, detail.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! Which broadcast-message preset a purchase outcome shows.
	//!
	//! NO_PLAYER and NO_ACTOR map to nothing on purpose: they mean the server could not name the caller
	//! or find their body, and there is no useful sentence to show someone in that state. Every other
	//! code has an authored preset - a tag without one is dropped silently (BUG-120).
	//! \param[in] resultCode One of this class's RESULT_* constants.
	//! \return The notification tag, or "" when the outcome has no message.
	protected string PurchaseNotificationTag(int resultCode)
	{
		if (resultCode == RESULT_OK)
			return NOTIFY_PURCHASED;

		if (resultCode == RESULT_NOT_AT_BARRACKS)
			return NOTIFY_NOT_AT_BARRACKS;

		if (resultCode == RESULT_BARRACKS_RUINED)
			return NOTIFY_BARRACKS_RUINED;

		if (resultCode == RESULT_BASE_NOT_FRIENDLY)
			return NOTIFY_BASE_NOT_FRIENDLY;

		if (resultCode == RESULT_BAD_ENTRY)
			return NOTIFY_BAD_ENTRY;

		if (resultCode == RESULT_AT_CAP)
			return NOTIFY_AT_CAP;

		if (resultCode == RESULT_NO_SUPPORTERS)
			return NOTIFY_NO_SUPPORTERS;

		if (resultCode == RESULT_CANNOT_AFFORD)
			return NOTIFY_CANNOT_AFFORD;

		if (resultCode == RESULT_SPAWN_FAILED)
			return NOTIFY_SPAWN_FAILED;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The caller's persistent id, derived from the runtime id this component already resolved from
	//! its own owner entity - never from anything on the wire.
	//! \param[in] playerId The resolved runtime player id.
	//! \return The persistent id, or "" when the player manager cannot name one.
	protected string ResolveCallerPersistentId(int playerId)
	{
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
			return "";

		return players.GetPersistentIDFromPlayerID(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! The one reply every ask ends on - see the listen-host short-circuit on ShouldRespondLocally().
	//! \param[in] playerId The caller, or <= 0 when unresolved (the RPC still goes out; a dedicated
	//! server's ShouldRespondLocally() is always false, so this only ever short-circuits for a host).
	//! \param[in] resultCode One of this class's RESULT_* constants.
	//! \param[in] charged How much was charged, or 0 when nothing was.
	//! \param[in] groupId The group the request concerned, or "" when none existed yet.
	protected void ReplyResult(int playerId, int resultCode, int charged, string groupId)
	{
		if (ShouldRespondLocally(playerId))
		{
			RpcDo_HCResult(resultCode, charged, groupId);
			return;
		}

		Rpc(RpcDo_HCResult, resultCode, charged, groupId);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the result of an ask.
	//! \param[in] resultCode One of this class's RESULT_* constants.
	//! \param[in] charged How much was charged.
	//! \param[in] groupId The group the request concerned, or "".
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_HCResult(int resultCode, int charged, string groupId)
	{
		if (m_OnHCResult)
			m_OnHCResult.Invoke(resultCode, charged, groupId);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the server's price for one catalog entry. Phase 4's purchase screen renders it; the
	//! client never computes a price of its own, because storage contents are not replicated.
	//! \param[in] entryIndex The entry this quote is for.
	//! \param[in] total The quoted price.
	//! \param[in] coveredValue What nearby warehouse stock covers, at buy price.
	//! \param[in] memberCount How many members the entry buys.
	//! \param[in] supportersNeeded How many town supporters it draws down.
	//! \param[in] refusalCode RESULT_OK, or why the entry cannot be bought here.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_HCQuote(int entryIndex, int total, int coveredValue, int memberCount, int supportersNeeded, int refusalCode)
	{
		if (m_OnHCQuote)
			m_OnHCQuote.Invoke(entryIndex, total, coveredValue, memberCount, supportersNeeded, refusalCode);
	}
}
