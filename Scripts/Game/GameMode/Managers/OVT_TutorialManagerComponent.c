//------------------------------------------------------------------------------------------------
//! Server-side registry and dispatcher for the tutorial system.
//!
//! Owns three things and nothing else:
//!   1. The entry registry - the authored OVT_TutorialEntryConfig list, validated once at campaign
//!      start into an id -> entry map.
//!   2. The subscriptions - one handler per catalogued server-side manager ScriptInvoker. NOTHING is
//!      instrumented at the call sites; every trigger hangs off an invoker that already existed.
//!   3. The routing decision - which player caused the event, and (for the two events that have no
//!      acting player at all) which players are close enough to care.
//!
//! It deliberately does NOT decide whether a player should SEE an entry. That is the client's call,
//! made against its own per-machine seen store, because the server has no visibility into seen state
//! (plan decision D5). The one thing the server tracks is a volatile per-session sent-set, purely so
//! that a veteran's every shop purchase does not generate a fresh RPC for the rest of the session.
//------------------------------------------------------------------------------------------------
class OVT_TutorialManagerComponentClass: OVT_ComponentClass
{
};

//------------------------------------------------------------------------------------------------
//! The tutorial manager. Lives on OVT_OverthrowGameMode; server-authoritative.
//------------------------------------------------------------------------------------------------
class OVT_TutorialManagerComponent: OVT_Component
{
	[Attribute("", UIWidgets.Object, desc: "Tutorial entries. One .conf per entry under Configs/Tutorials/.")]
	protected ref array<ref OVT_TutorialEntryConfig> m_aEntries; //!< Authored tutorial entries.

	//! Fan-out radius for TOWN_CONTROL_CHANGE, in metres. A heuristic, not a guarantee - see D11.
	static const float NEAR_RADIUS_TOWN = 500;

	//! Fan-out radius for BASE_CONTROL_CHANGE, in metres. Tighter than a town: bases are smaller and
	//! a capture is only meaningful to someone who was actually there.
	static const float NEAR_RADIUS_BASE = 300;

	//! Lowest runtime player id the engine ever issues. Anything below this is "no acting player".
	static const int FIRST_VALID_PLAYER_ID = 1;

	//! id -> entry, built once in PostGameStart. Shares ownership with m_aEntries by design.
	protected ref map<string, ref OVT_TutorialEntryConfig> m_mEntriesById;

	//! persistent id -> set of entry ids already sent to that player THIS SESSION.
	//!
	//! Keyed on persistent id, not runtime player id: runtime ids are recycled across reconnects, so
	//! a rejoining player would inherit whatever the previous holder of their id had been sent.
	//! Never cleared - a session-lifetime set of at most (entries x players) short strings.
	protected ref map<string, ref set<string>> m_mSentByPersistentId;

	//! Reusable result buffer for OVT_TutorialMatcher.FindMatches. Cleared by the matcher every call.
	protected ref array<string> m_aMatchBuffer;

	//! True once the registry validated cleanly and the invoker subscriptions were made.
	protected bool m_bStarted;

	static OVT_TutorialManagerComponent s_Instance; //!< Singleton instance.

	//------------------------------------------------------------------------------------------------
	//! Gets the singleton instance of the tutorial manager.
	//! \return The static OVT_TutorialManagerComponent instance, or null if the game mode has none.
	static OVT_TutorialManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_TutorialManagerComponent.Cast(pGameMode.FindComponent(OVT_TutorialManagerComponent));
		}

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Constructor. Allocates the collections so that every accessor is safe before PostGameStart.
	void OVT_TutorialManagerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_mEntriesById = new map<string, ref OVT_TutorialEntryConfig>();
		m_mSentByPersistentId = new map<string, ref set<string>>();
		m_aMatchBuffer = new array<string>();
	}

	//------------------------------------------------------------------------------------------------
	//! Initializes the component. Nothing to resolve here: every manager this one listens to is
	//! reached lazily in PostGameStart, which runs after all of them have been initialized.
	//! \param owner The entity this component is attached to.
	void Init(IEntity owner)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Validates the entry registry and subscribes to every catalogued server-side invoker.
	//!
	//! SERVER ONLY. A client has the same authored entries (the game mode entity exists there too) but
	//! must never subscribe: its managers' invokers fire off replicated state, so a client that
	//! subscribed would dispatch phantom events for actions it merely observed.
	void PostGameStart()
	{
		if (!Replication.IsServer())
			return;

		if (m_bStarted)
			return;

		if (!BuildRegistry())
		{
			// Loud, named and terminal. An ambiguous registry means an id could resolve to either of
			// two entries, and the id IS the permanent key of every player's seen store - a mismatch
			// there silently and permanently suppresses the wrong tip on every machine that saw it.
			Print("[Overthrow.Tutorial] Registry rejected - no tutorial triggers will be subscribed this session. Fix the entries listed above on Prefabs/GameMode/OVT_OverthrowGameMode.et.", LogLevel.ERROR);
			return;
		}

		SubscribeToInvokers();
		m_bStarted = true;

		Print("[Overthrow.Tutorial] Loaded " + m_mEntriesById.Count().ToString() + " tutorial entries");
	}

	//-----------------------------------------------------------------------------------------------
	// REGISTRY
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Builds the id -> entry map, naming every offender it finds before giving up.
	//!
	//! Deliberately reports ALL bad entries rather than the first: an author who fixes one and
	//! relaunches to find another is being made to pay the campaign-start cost once per mistake.
	//! \return True when every entry is valid. False when at least one id was empty or duplicated.
	protected bool BuildRegistry()
	{
		m_mEntriesById.Clear();

		if (!m_aEntries)
			return true;

		bool valid = true;

		for (int i = 0; i < m_aEntries.Count(); i++)
		{
			OVT_TutorialEntryConfig entry = m_aEntries.Get(i);
			if (!entry)
			{
				Print("[Overthrow.Tutorial] Entry at index " + i.ToString() + " is null - an empty row was left in m_aTutorialEntries on the game mode prefab", LogLevel.ERROR);
				valid = false;
				continue;
			}

			if (entry.m_sId == "")
			{
				Print("[Overthrow.Tutorial] Entry at index " + i.ToString() + " has an empty m_sId. An id is the entry's permanent identity on the wire and in every player's seen store; it cannot be blank.", LogLevel.ERROR);
				valid = false;
				continue;
			}

			if (m_mEntriesById.Contains(entry.m_sId))
			{
				Print("[Overthrow.Tutorial] Duplicate tutorial entry id '" + entry.m_sId + "' at index " + i.ToString() + ". Ids must be unique and are never reused - retire an entry with m_bEnabled 0 instead of recycling its id.", LogLevel.ERROR);
				valid = false;
				continue;
			}

			m_mEntriesById.Insert(entry.m_sId, entry);
		}

		if (!valid)
			m_mEntriesById.Clear();

		return valid;
	}

	//-----------------------------------------------------------------------------------------------
	// SUBSCRIPTIONS
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Subscribes one handler to each catalogued server-side invoker.
	//!
	//! Every manager is null-guarded individually: a stripped-down game mode prefab must cost the
	//! tutorials that depend on that manager and nothing else.
	protected void SubscribeToInvokers()
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (economy)
		{
			economy.m_OnPlayerBuy.Insert(OnPlayerBuy);
			economy.m_OnPlayerSell.Insert(OnPlayerSell);
			economy.m_OnPlayerTransaction.Insert(OnPlayerTransaction);
		}

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (resistance)
		{
			resistance.m_OnPlace.Insert(OnPlace);
			resistance.m_OnBuild.Insert(OnBuild);
		}

		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if (recruits)
			recruits.m_OnRecruitAdded.Insert(OnRecruitAdded);

		OVT_SkillManagerComponent skills = OVT_Global.GetSkills();
		if (skills)
			skills.m_OnPlayerSkill.Insert(OnPlayerSkill);

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (towns)
			towns.m_OnTownControlChange.Insert(OnTownControlChange);

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (occupying)
			occupying.m_OnBaseControlChanged.Insert(OnBaseControlChanged);

		// Static and lazily allocated (plan decision D13 + the Phase 0 refinement): the getter builds
		// the invoker on first use, so it can never return null and needs no guard, and one
		// subscription covers every character the player will ever respawn into.
		OVT_PlayerWantedComponent.GetOnWantedLevelChanged().Insert(OnWantedLevelChanged);
	}

	//-----------------------------------------------------------------------------------------------
	// HANDLERS - per-player events
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! A player bought something. Args come straight from OVT_EconomyManagerComponent.m_OnPlayerBuy.
	//! \param playerId Runtime id of the buying player.
	//! \param actualCost What they actually paid, after partial-purchase adjustment.
	protected void OnPlayerBuy(int playerId, int actualCost)
	{
		DispatchToPlayer(OVT_TutorialEvent.PLAYER_BUY, playerId, actualCost, "");
	}

	//------------------------------------------------------------------------------------------------
	//! A player was credited for a sale.
	//!
	//! NOT shop-specific: m_OnPlayerSell also fires from OVT_EconomyManagerComponent.AddPlayerMoney
	//! whenever doEvent is set, so job rewards land here too. An entry that must mean "sold something
	//! at a shop" should bind PLAYER_TRANSACTION, which carries the shop.
	//! \param playerId Runtime id of the credited player.
	//! \param total Amount credited.
	protected void OnPlayerSell(int playerId, int total)
	{
		DispatchToPlayer(OVT_TutorialEvent.PLAYER_SELL, playerId, total, "");
	}

	//------------------------------------------------------------------------------------------------
	//! A player completed a shop transaction, in either direction.
	//! \param playerId Runtime id of the transacting player.
	//! \param shop The shop involved. May be null; the filter is then empty.
	//! \param isBuying True for a purchase, false for a sale. Not exposed to triggers - PLAYER_BUY and
	//! PLAYER_SELL already separate the two directions.
	//! \param amount Money moved.
	protected void OnPlayerTransaction(int playerId, OVT_ShopComponent shop, bool isBuying, int amount)
	{
		string shopType = "";
		if (shop)
			shopType = SCR_Enum.GetEnumName(OVT_ShopType, shop.m_ShopType);

		DispatchToPlayer(OVT_TutorialEvent.PLAYER_TRANSACTION, playerId, amount, shopType);
	}

	//------------------------------------------------------------------------------------------------
	//! A player placed a placeable.
	//! \param entity The spawned entity. Unused - the context is flattened to plain data.
	//! \param placeable The placeable definition. May be null; the filter is then empty.
	//! \param playerId Runtime id of the placing player.
	protected void OnPlace(IEntity entity, OVT_Placeable placeable, int playerId)
	{
		string name = "";
		if (placeable)
			name = placeable.m_sName;

		DispatchToPlayer(OVT_TutorialEvent.PLAYER_PLACE, playerId, 0, name);
	}

	//------------------------------------------------------------------------------------------------
	//! A player finished a buildable.
	//! \param entity The spawned entity. Unused.
	//! \param buildable The buildable definition. May be null; the filter is then empty.
	//! \param playerId Runtime id of the building player.
	protected void OnBuild(IEntity entity, OVT_Buildable buildable, int playerId)
	{
		string name = "";
		if (buildable)
			name = buildable.m_sName;

		DispatchToPlayer(OVT_TutorialEvent.PLAYER_BUILD, playerId, 0, name);
	}

	//------------------------------------------------------------------------------------------------
	//! A recruit joined someone's roster.
	//!
	//! The only catalogued event that carries no player id at all: the recruit knows its OWNER by
	//! persistent id, so the runtime id has to be looked back up. An owner who is currently offline
	//! resolves to -1 and the event is dropped, which is correct - there is no client to tell.
	//! \param recruit The new recruit.
	protected void OnRecruitAdded(OVT_RecruitData recruit)
	{
		if (!recruit)
			return;

		int playerId = ResolvePlayerIdFromPersistentId(recruit.m_sOwnerPersistentId);
		DispatchToPlayer(OVT_TutorialEvent.PLAYER_RECRUIT_ADDED, playerId, 0, "");
	}

	//------------------------------------------------------------------------------------------------
	//! A player's skill levels changed.
	//!
	//! m_OnPlayerSkill carries the skill key alongside the player id, so a PLAYER_SKILL trigger can
	//! filter on exactly which skill moved - m_sFilter is that key, per section 5 of the plan.
	//! \param playerId Runtime id of the player whose skills changed.
	//! \param skillKey Key of the skill that gained a level.
	protected void OnPlayerSkill(int playerId, string skillKey)
	{
		DispatchToPlayer(OVT_TutorialEvent.PLAYER_SKILL, playerId, 0, skillKey);
	}

	//------------------------------------------------------------------------------------------------
	//! A player's wanted level rose. Only escalations reach here; the decay path does not fire.
	//! \param playerId Runtime id of the wanted player.
	//! \param newLevel The level just reached - what a PLAYER_WANTED threshold compares against.
	//! \param oldLevel The level before the escalation. Unused.
	protected void OnWantedLevelChanged(int playerId, int newLevel, int oldLevel)
	{
		DispatchToPlayer(OVT_TutorialEvent.PLAYER_WANTED, playerId, newLevel, "");
	}

	//-----------------------------------------------------------------------------------------------
	// HANDLERS - global events (no acting player exists)
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! A town changed hands. There is no acting player by construction, so this fans out by proximity
	//! rather than fabricating one or broadcasting to everybody (plan decision D11).
	//! \param town The town that changed control.
	protected void OnTownControlChange(OVT_TownData town)
	{
		if (!town)
			return;

		DispatchToPlayersNear(OVT_TutorialEvent.TOWN_CONTROL_CHANGE, town.location, NEAR_RADIUS_TOWN, 0, "");
	}

	//------------------------------------------------------------------------------------------------
	//! A base changed hands. Global, same reasoning as OnTownControlChange.
	//! \param baseController The base whose controlling faction changed.
	protected void OnBaseControlChanged(OVT_BaseControllerComponent baseController)
	{
		if (!baseController)
			return;

		IEntity owner = baseController.GetOwner();
		if (!owner)
			return;

		DispatchToPlayersNear(OVT_TutorialEvent.BASE_CONTROL_CHANGE, owner.GetOrigin(), NEAR_RADIUS_BASE, 0, "");
	}

	//-----------------------------------------------------------------------------------------------
	// DISPATCH
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Matches an event occurrence against the registry and sends every hit to the acting player.
	//! \param evt The event that occurred.
	//! \param playerId Runtime id of the acting player. Anything below FIRST_VALID_PLAYER_ID is
	//! treated as "no acting player" and the event is dropped silently.
	//! \param value The event's numeric payload.
	//! \param filter The event's string payload.
	protected void DispatchToPlayer(OVT_TutorialEvent evt, int playerId, int value, string filter)
	{
		if (playerId < FIRST_VALID_PLAYER_ID)
			return;

		if (!Match(evt, playerId, value, filter))
			return;

		foreach (string entryId : m_aMatchBuffer)
		{
			SendToPlayer(playerId, entryId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Matches a playerId-less event and fans every hit out to the players within radius.
	//! \param evt The event that occurred.
	//! \param pos World position the event happened at.
	//! \param radius Fan-out radius in metres.
	//! \param value The event's numeric payload.
	//! \param filter The event's string payload.
	protected void DispatchToPlayersNear(OVT_TutorialEvent evt, vector pos, float radius, int value, string filter)
	{
		// -1 is the honest playerId for a global event, and it is what a trigger sees. No id is ever
		// fabricated to make the per-player path fit.
		if (!Match(evt, -1, value, filter))
			return;

		foreach (string entryId : m_aMatchBuffer)
		{
			SendToPlayersNear(pos, radius, entryId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the pure matcher over the registry and leaves the hits in m_aMatchBuffer.
	//! \param evt The event that occurred.
	//! \param playerId Acting player, or -1 for a global event.
	//! \param value The event's numeric payload.
	//! \param filter The event's string payload.
	//! \return True when m_aMatchBuffer holds at least one entry id.
	protected bool Match(OVT_TutorialEvent evt, int playerId, int value, string filter)
	{
		if (!m_bStarted)
			return false;

		OVT_TutorialEventContext ctx = new OVT_TutorialEventContext();
		ctx.m_eEvent = evt;
		ctx.m_iPlayerId = playerId;
		ctx.m_iValue = value;
		ctx.m_sFilter = filter;

		OVT_TutorialMatcher.FindMatches(m_aEntries, ctx, m_aMatchBuffer);

		return m_aMatchBuffer.Count() > 0;
	}

	//-----------------------------------------------------------------------------------------------
	// DELIVERY
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Sends one entry to one player, at most once per player per session.
	//!
	//! The sent-set here is a traffic control, not a correctness mechanism: the client's per-machine
	//! store is what actually decides whether a tip is shown. Dropping a repeat send is therefore
	//! always safe - the only entry the client could have wanted is one it has never seen, and it has
	//! never seen this one only if this is the first send.
	//! \param playerId Runtime id of the target player.
	//! \param entryId Id of the entry to send.
	void SendToPlayer(int playerId, string entryId)
	{
		if (playerId < FIRST_VALID_PLAYER_ID || entryId == "")
			return;

		string key = GetSentSetKey(playerId);

		set<string> sent = m_mSentByPersistentId.Get(key);
		if (!sent)
		{
			sent = new set<string>();
			m_mSentByPersistentId.Insert(key, sent);
		}

		if (sent.Contains(entryId))
			return;

		sent.Insert(entryId);

		Deliver(playerId, entryId);
	}

	//------------------------------------------------------------------------------------------------
	//! Sends one entry to every living player within radius of a position.
	//!
	//! The honest routing for an event with no actor (plan decision D11). It is a heuristic and is
	//! documented as one; section 5 tells entry authors to prefer a per-player trigger where one exists.
	//! \param pos Centre of the fan-out.
	//! \param radius Radius in metres.
	//! \param entryId Id of the entry to send.
	void SendToPlayersNear(vector pos, float radius, string entryId)
	{
		if (entryId == "")
			return;

		PlayerManager mgr = GetGame().GetPlayerManager();
		if (!mgr)
			return;

		array<int> players = new array<int>();
		mgr.GetPlayers(players);

		foreach (int playerId : players)
		{
			IEntity controlled = mgr.GetPlayerControlledEntity(playerId);
			if (!controlled)
				continue;

			if (vector.Distance(controlled.GetOrigin(), pos) > radius)
				continue;

			SendToPlayer(playerId, entryId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Hands one entry id to one player's client, and to nobody else.
	//!
	//! The whole authority boundary is this method. It resolves the acting player's own
	//! OVT_OverthrowController and calls the RplRcver.Owner RPC on that controller's
	//! OVT_TutorialComponent, so the entry id reaches exactly one machine. Nothing is broadcast and
	//! nothing is filtered client-side - which is the difference between this and the notification
	//! manager, and the reason BUG-037's per-player delivery failure cannot recur here.
	//!
	//! A player whose controller has not been created or registered yet is DROPPED, not retried. The
	//! sent-set has already recorded the send, so the tip is lost for the session rather than
	//! duplicated - and the only window in which that can happen is the few seconds between connect
	//! and OVT_OverthrowController.RpcDo_NotifyOwnerAssignment, during which the player cannot have
	//! caused a trigger themselves.
	//! \param playerId Runtime id of the target player.
	//! \param entryId Id of the entry to deliver.
	protected void Deliver(int playerId, string entryId)
	{
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
			return;

		OVT_OverthrowController controller = players.GetController(playerId);
		if (!controller)
			return;

		OVT_TutorialComponent tutorials = OVT_TutorialComponent.Cast(controller.FindComponent(OVT_TutorialComponent));
		if (!tutorials)
			return;

		tutorials.Notify(entryId);
	}

	//-----------------------------------------------------------------------------------------------
	// HELPERS
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Resolves the sent-set key for a runtime player id.
	//!
	//! Persistent id is the key that matters (D5). A player whose persistent id is not resolvable yet
	//! still gets a key - a runtime-scoped one - because losing the dedup entirely for that player is
	//! worse than a key that a reconnect will not match.
	//! \param playerId Runtime player id.
	//! \return A stable non-empty key.
	protected string GetSentSetKey(int playerId)
	{
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (players)
		{
			string persistentId = players.GetPersistentIDFromPlayerID(playerId);
			if (persistentId != "")
				return persistentId;
		}

		return "runtime:" + playerId.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves a persistent id to the runtime player id currently holding it.
	//! \param persistentId The persistent id. Empty resolves to -1.
	//! \return The runtime player id, or -1 when that player is not currently connected.
	protected int ResolvePlayerIdFromPersistentId(string persistentId)
	{
		if (persistentId == "")
			return -1;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
			return -1;

		return players.GetPlayerIDFromPersistentID(persistentId);
	}

	//-----------------------------------------------------------------------------------------------
	// GETTERS
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The authored entry list, exactly as it appears on the game-mode prefab.
	//! \return The entry array. May be null if the prefab declares none.
	array<ref OVT_TutorialEntryConfig> GetEntries()
	{
		return m_aEntries;
	}

	//------------------------------------------------------------------------------------------------
	//! Looks up one entry by its id.
	//! \param entryId The entry id.
	//! \return The entry, or null when the registry has no such id (including when it was rejected).
	OVT_TutorialEntryConfig GetEntry(string entryId)
	{
		return m_mEntriesById.Get(entryId);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the registry validated and the invoker subscriptions were made.
	//! \return True on a server that started cleanly; false on a client, or after a rejected registry.
	bool IsStarted()
	{
		return m_bStarted;
	}
}
