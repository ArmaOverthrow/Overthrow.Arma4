[ComponentEditorProps(category: "Overthrow/Components/Controller", description: "Server-authoritative respawn choice for one player")]
class OVT_RespawnRequestComponentClass : OVT_ComponentClass {};

//------------------------------------------------------------------------------------------------
//! Server-authoritative respawn, on the per-player OVT_OverthrowController entity.
//!
//! Copied from OVT_TravelRequestComponent including its discipline: the RplRcver.Server ask /
//! RplRcver.Owner result split, ResolveOwningPlayerId, the listen-server short-circuits, and the
//! arrival/result Print pair that lets a play-test tell "the request never left the client" from
//! "a rule refused". Project rule (overthrow-controller.md): every client->server RPC lives on a
//! controller component like this one; the legacy comms monolith was deleted in Phase 10. Before that,
//! map/legacy-retirement had already deleted four unvalidated RPCs from it, one of them a two-line
//! unpaid teleport to an arbitrary vector callable by any client. That defect is the reason this class
//! exists here instead of there.
//!
//! THE RULE BOTH VERBS NOW FOLLOW: a client-sent vector is validated against the server's OWN
//! enumeration and the move then uses the SERVER's recorded vector. The client names a place; it does
//! not supply a coordinate, and a crafted vector matches nothing. Respawn was built this way from the
//! start; fast travel gained it later (OVT_FastTravelService.ResolveFastTravelDestination, reached
//! from OVT_TravelRequestComponent step 6b) and both now share one enumeration in
//! OVT_RespawnService.CollectEligiblePositions. The two differ only in what a miss costs: a respawn
//! falls back home, because a dead player must end up alive; a trip is simply refused.
//!
//! THIS COMPONENT NEVER SPAWNS ANYTHING AND HOLDS NO STATE. It checks the request shape, resolves
//! who is asking, hands off to the spawn system and reports back. The awaiting-respawn state, the
//! eligibility enumeration and the character creation all live elsewhere, on the server, keyed by
//! player - so there is nothing here a client can leave armed and nothing here to go stale.
//!
//! ! Rpc()'s prototype is untyped variadic, so a wrong argument count compiles clean and dies
//! silently at the wire (BUG-090). compile-check.sh cannot see it and no test in this project can:
//! the autotest world has no players, so no RPC in it ever crosses a wire. The practical defence is
//! to have as few signatures as possible and to read every call against its handler by hand. There
//! are exactly three RPCs here and exactly three Rpc() call sites, one per RPC.
//!
//! ! THE TWO-VALUE ASK IS DELIBERATE. There is no player id in the payload: identity is resolved
//! server-side from the controller entity this component sits on, and a remote client can only
//! reach this handler through the controller entity it owns. There is nothing to spoof.
//------------------------------------------------------------------------------------------------
class OVT_RespawnRequestComponent : OVT_Component
{
	//! Fired on the dead player's own machine when the server asks for the respawn screen. No args.
	//!
	//! ! SUBSCRIBERS MUST BE IDEMPOTENT. The server re-asks on a repeating tick while the player is
	//! still awaiting a choice, so this fires again every few seconds until they pick - that
	//! unbounded re-ask is how a dropped packet or a not-yet-ready client still ends up with a
	//! screen. Showing an already-shown screen must be a no-op, not a second screen.
	ref ScriptInvoker m_OnShowRespawnScreen = new ScriptInvoker();

	//! Fired on the requesting client only. Args: (int OVT_RespawnResult).
	//! Display only - see RpcDo_RespawnResult. Nothing is moved and no state is mutated from here.
	ref ScriptInvoker m_OnRespawnResult = new ScriptInvoker();

	//------------------------------------------------------------------------------------------------
	//! Ask the server to create this player's new character.
	//!
	//! The client names a destination and, for LOCATION, the place it picked. It does NOT name itself
	//! (S-4: identity comes from the controller entity) and it does NOT name a position to spawn at:
	//! targetPos is a LOOKUP KEY matched against the server's own eligible set, and the spawn uses
	//! the server's vector on a match and home on a miss.
	//! \param[in] destination An OVT_RespawnDestination value. Anything else is refused by the server.
	//! \param[in] targetPos The picked location's position. Meaningless for HOME - pass vector.Zero;
	//! home is not a location record, carries no vector and is resolved by the server's home chain.
	void RequestRespawn(int destination, vector targetPos)
	{
		if(Replication.IsServer())
		{
			RpcAsk_Respawn(destination, targetPos);
		}else{
			Rpc(RpcAsk_Respawn, destination, targetPos);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: check the request shape, resolve who is asking, spawn, and report - in that order.
	//!
	//! The step order is the whole point of this handler and it is finished: the spawn system lands
	//! behind the CompleteRespawn seam below without this method changing.
	//!  1. we are the server
	//!  2. the arrival Print, BEFORE any refusal can return, so its absence means the request never
	//!     arrived rather than that a rule refused it
	//!  3. the request shape - an out-of-range destination is refused outright, never defaulted.
	//!     A value that is neither HOME nor LOCATION must not fall through to a branch that skips
	//!     validation, which is the exact defect the travel component's unknown-verb guard exists
	//!     for: there, an unknown verb matched neither rule set and would have been waved through
	//!  4. the requesting player is resolved from THIS controller entity, never from the payload
	//!  5. the spawn system decides and does; this component never creates a character
	//!  6. EVERY outcome is reported, including OK. Silence is indistinguishable from a request that
	//!     never left the client, and this screen is one the player cannot leave on their own
	//!
	//! Steps 3 and 4 return without reporting because they run BEFORE an identity exists - there is
	//! no client to report to yet. Neither is reachable from Overthrow's own UI: both mean a crafted
	//! packet or a code bug, and both leave the player on the screen, free to press again.
	//! \param[in] destination An OVT_RespawnDestination value.
	//! \param[in] targetPos The picked location's position. A key, not a position.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Respawn(int destination, vector targetPos)
	{
		// 1
		if(!Replication.IsServer()) return;

		// 2 - BUG-090 diagnostic: this line firing proves the request left the client, reached this
		// machine and was decoded with the right arity. If respawn does nothing and this never
		// prints, the component is missing from the controller prefab or the Rpc() arity is wrong;
		// if it prints and nothing happens, a rule refused - see the matching result print below.
		// The vector is included so two clients picking two different locations can be told apart in
		// one log; the result line carries the player id.
		Print("[Overthrow] Respawn request received: destination=" + destination.ToString() + " pos=" + targetPos.ToString(), LogLevel.NORMAL);

		// 3
		if(destination != OVT_RespawnDestination.HOME && destination != OVT_RespawnDestination.LOCATION)
		{
			Print("[Overthrow] Respawn request refused: unknown destination " + destination.ToString(), LogLevel.WARNING);
			return;
		}

		// 4
		int playerId = ResolveOwningPlayerId();
		if(playerId <= 0)
		{
			Print("[Overthrow] Respawn request refused: could not resolve the requesting player", LogLevel.WARNING);
			return;
		}

		// 5
		int result = CompleteRespawn(playerId, destination, targetPos);

		// 6
		SendRespawnResult(playerId, result);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the outcome of a respawn request, for display only.
	//!
	//! ! THIS RPC NEVER MOVES ANYTHING AND NEVER MUTATES STATE. The character is created on the
	//! server and arrives through the engine's own replication; a lost packet here costs a hint, not
	//! a spawn. Closing the screen is the subscriber's decision, not this method's.
	//!
	//! Every non-OK code hints, because server authority means a pick can be refused after the fact
	//! and the player is sitting on a screen with no other feedback channel. A plain OK is
	//! deliberately silent: the player is standing where they asked to stand, which says it better
	//! than a hint would. OK_FELL_BACK_HOME is NOT that case - it is a successful spawn somewhere
	//! the player did not pick, so it must say so, and ReasonKeyFor maps it to
	//! "#OVT-Respawn_FellBackHome" for exactly that reason.
	//!
	//! The keys ReasonKeyFor returns are not in the generated localization exports yet (map/respawn
	//! Phase 7 adds them to the .st master and the user regenerates), so they render raw until then.
	//! Using the key is still correct - a literal here would have to be found and removed later.
	//! \param[in] result An OVT_RespawnResult value.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_RespawnResult(int result)
	{
		if(result != OVT_RespawnResult.OK)
		{
			// Guarded rather than assumed: ReasonKeyFor answers "" for OK and only for OK, so this
			// cannot normally be empty - but hinting an empty string would be a silent no-op, the
			// one outcome this whole path exists to prevent.
			string reason = OVT_RespawnService.ReasonKeyFor(result);
			if(!reason.IsEmpty())
				OVT_Global.ShowHint(reason);
		}

		if(m_OnRespawnResult)
			m_OnRespawnResult.Invoke(result);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: ask one player's machine to put the respawn screen up.
	//!
	//! Called by the spawn system when a player starts awaiting a choice, and again on its re-ask
	//! tick for as long as they are still awaiting. Safe to call repeatedly by design - see
	//! m_OnShowRespawnScreen.
	//!
	//! On a listen server the target IS this machine's local player, and a listen-server host never
	//! receives its own owner-targeted RPCs - a known bug class in this project - so the handler is
	//! called directly and nothing is sent. On a dedicated server GetLocalPlayerId() is 0, no player
	//! id can match it, and this always goes over the wire. Same shape as SendRespawnResult below and
	//! as OVT_TravelRequestComponent.SendTravelResult / OVT_ShopTransactionComponent.SendSellResult.
	//!
	//! Unlike those two this guards on Replication.IsServer() first, because it is a PUBLIC entry
	//! point rather than a continuation of an already-server-guarded handler: a stray client call
	//! would otherwise fire an owner-targeted Rpc() from a client, which does nothing at best.
	//! \param[in] playerId Runtime id of the player who must choose.
	void AskShowRespawnScreen(int playerId)
	{
		if(!Replication.IsServer()) return;

		if(playerId > 0 && playerId == SCR_PlayerController.GetLocalPlayerId())
		{
			RpcDo_ShowRespawnScreen();
			return;
		}

		Rpc(RpcDo_ShowRespawnScreen);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the server wants this player to choose where to respawn.
	//!
	//! Carries no payload on purpose. Everything the screen needs it derives locally - the eligible
	//! markers come from the map config running the same predicates the server runs, and the local
	//! persistent id comes from OVT_Global.GetLocalPersistentId(), which survives having no
	//! controlled entity. Nothing about the player's entitlements is shipped in this packet, so
	//! there is no state here to arrive stale or out of order.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_ShowRespawnScreen()
	{
		if(m_OnShowRespawnScreen)
			m_OnShowRespawnScreen.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	//! Hands the validated request to the spawn system and reports what it did.
	//!
	//! THE WHOLE SEAM. This component decides nothing about respawning: it has checked the shape of the
	//! request and who is asking, and everything that follows - whether this player is entitled to be
	//! spawned at all, where, and whether that has to fall back to home - belongs to the spawn system,
	//! which is the only thing that knows. Keeping the hand-off behind one method is what let the
	//! validation order above be finished and frozen before any of it existed.
	//!
	//! OVT_SpawnLogic.CompleteRespawn consumes the player's awaiting-respawn claim BEFORE it builds
	//! anything, so a duplicate ask, a late packet or an ask from a player who is not dead finds no
	//! claim and returns without spawning. That is why this component can be as thin as it is: it
	//! cannot cause a second character no matter how many times it is called.
	//! \param[in] playerId Runtime id of the requesting player, resolved from the controller entity.
	//! \param[in] destination An OVT_RespawnDestination value, already range-checked.
	//! \param[in] targetPos The picked location's position. A lookup key, never a spawn position.
	//! \return An OVT_RespawnResult code.
	protected int CompleteRespawn(int playerId, int destination, vector targetPos)
	{
		OVT_SpawnLogic spawnLogic = OVT_SpawnLogic.GetInstance();
		if(!spawnLogic)
		{
			Print("[Overthrow] Respawn request refused: no OVT_SpawnLogic instance", LogLevel.ERROR);
			return OVT_RespawnResult.SPAWN_FAILED;
		}

		return spawnLogic.CompleteRespawn(playerId, destination, targetPos);
	}

	//------------------------------------------------------------------------------------------------
	//! Delivers the outcome to the requesting client.
	//!
	//! On a listen server the requester IS this machine's local player and a listen-server host never
	//! receives its own owner-targeted RPCs, so the handler is called directly and nothing is sent.
	//! On a dedicated server GetLocalPlayerId() is 0 and this always goes over the wire.
	//!
	//! Called for every outcome that has an identified requester, including OK. The result Print
	//! pairs with the arrival Print in RpcAsk_Respawn: neither line means the request never arrived,
	//! both lines mean the server answered, and this one carries the player id - which is what makes
	//! "player A's request never resolved to player B" checkable on a two-client play-test.
	//! \param[in] playerId Runtime id of the requesting player.
	//! \param[in] result An OVT_RespawnResult value.
	protected void SendRespawnResult(int playerId, int result)
	{
		Print("[Overthrow] Respawn result " + result.ToString() + " for player " + playerId.ToString(), LogLevel.NORMAL);

		if(playerId > 0 && playerId == SCR_PlayerController.GetLocalPlayerId())
		{
			RpcDo_RespawnResult(result);
			return;
		}

		Rpc(RpcDo_RespawnResult, result);
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
	//! Copied verbatim from OVT_TravelRequestComponent, itself copied from OVT_ShopTransactionComponent
	//! and OVT_TowerSabotageComponent - now four controller components deep, and a stronger candidate
	//! than ever for a shared base class. Copied rather than extracted here because introducing that
	//! base class mid-feature would edit three working server paths for no behavioural gain.
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
