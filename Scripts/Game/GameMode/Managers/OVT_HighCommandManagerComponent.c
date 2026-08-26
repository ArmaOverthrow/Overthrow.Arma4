[BaseContainerProps(configRoot: true)]
class OVT_HighCommandManagerComponentClass : OVT_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! What was last SENT for one group's heartbeat.
//!
//! Deliberately NOT the record: the record is also written by the save-point sweep and by the load
//! walk, so comparing against it would suppress a heartbeat that was never actually sent.
//------------------------------------------------------------------------------------------------
class OVT_HighCommandStatusEcho
{
	vector m_vPosition;
	int m_iStatusFlags;
	int m_iAliveMembers;
}

//------------------------------------------------------------------------------------------------
//! Records, JIP, deltas, heartbeat, caps and ticks for player-owned High Command groups
//! (implementation.md §3.1). The record table is THE truth on the server and a JIP + delta mirror
//! on every client - nothing on a client ever dereferences a group entity.
//!
//! ⚠ THE JIP PAYLOAD IS POSITIONAL. RplSave writes the resolved member cap, then a count, then one
//! fixed block per group; RplLoad reads them back in exactly that order. A NEW FIELD MAY ONLY BE
//! APPENDED to the end of the per-group block - inserting one re-points every field after it. Both
//! sides of this wire ship together, so there is no version to negotiate: the order IS the format.
//------------------------------------------------------------------------------------------------
class OVT_HighCommandManagerComponent : OVT_Component
{
	//! How often HC groups are checked for warehouse rearm, in ms (Phase 10 - RearmTick).
	[Attribute(defvalue: "60000", desc: "How often High Command groups are checked for warehouse rearm, in ms")]
	int m_iRearmIntervalMs;

	//! How often HC vehicle groups are checked for refuel, in ms (Phase 10 - RefuelTick).
	[Attribute(defvalue: "60000", desc: "How often High Command vehicle groups are checked for refuel, in ms")]
	int m_iRefuelIntervalMs;

	//! The always-live group entity every High Command group is built on. MUST carry
	//! OVT_HighCommandGroupComponent or the manager refuses to use it: that component is what removes
	//! the group's AI observer and deletes its waypoints.
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "High Command Group Prefab", params: "et")]
	ResourceName m_sGroupPrefab;

	//! Legs in a PATROL order's ring.
	static const int PATROL_RING_POINTS = 4;

	//! How long a patrolling group holds at each leg, in seconds.
	static const float PATROL_WAIT_SECONDS = 60.0;

	//! How deep the crew-seat walk descends into a vehicle's children. A turret is one level down; a
	//! gun mounted on a turret is two.
	static const int SEAT_SEARCH_DEPTH = 3;

	//! Ring radius used to place members when a source group prefab's formation cannot be resolved.
	static const float MEMBER_FALLBACK_RADIUS = 2.0;

	//! Bearing step for that fallback ring, in degrees.
	static const float MEMBER_FALLBACK_SPACING_DEGREES = 45.0;

	//! The heartbeat's own tick, and the cadence a group that is still TRAVELLING reports at (R4).
	static const int STATUS_SYNC_INTERVAL_MS = 2000;

	//! Sweep ticks a PARKED group waits between reads. 5 x STATUS_SYNC_INTERVAL_MS is the 10 s cadence
	//! the heartbeat shipped with, so a group standing still costs exactly what it always did.
	static const int STATUS_IDLE_SWEEP_TICKS = 5;

	//! How far a group's position must move before the heartbeat is worth a packet, in metres. Below
	//! this a marker would not move a pixel at any map zoom, so a parked group stays silent.
	static const float STATUS_POSITION_THRESHOLD = 5.0;

	//! How long to wait for a stored member body before its prefab stand-in keeps the slot (ms).
	static const int BODY_SPAWN_TIMEOUT_MS = 15000;

	//! The persistence collection member bodies belong to - the recruit bodies' collection, because an
	//! HC member is the same Character_Base.et-derived character matched by the same vanilla rule.
	static const string MEMBER_BODY_COLLECTION = "Character";

	//! Monotonic tiebreaker for group ids, so two groups minted in one second cannot collide.
	protected int m_iGroupIdSalt;

	//! Monotonic sweep counter. A parked group is read on one tick in STATUS_IDLE_SWEEP_TICKS of it.
	protected int m_iStatusSweepTick;

	static OVT_HighCommandManagerComponent s_Instance;

	//! Every group, keyed by group id - THE truth on the server, a JIP+delta mirror on a client.
	//! Read directly by OVT_HighCommandManagerSerializer, the OVT_RecruitManagerComponent precedent.
	[NonSerialized()]
	ref map<string, ref OVT_HighCommandRecord> m_mGroups;

	//! Group ids owned by each player, keyed by owner persistent id.
	[NonSerialized()]
	ref map<string, ref array<string>> m_mGroupsByOwner;

	//! SERVER ONLY - live group entity id to group id, for fast entity->record lookup.
	[NonSerialized()]
	ref map<EntityID, string> m_mEntityToGroup;

	//! SERVER ONLY - composition and required-item cache for the authored catalog (§3.4).
	[NonSerialized()]
	ref OVT_HighCommandManifest m_Manifest;

	//! SERVER ONLY - member count per authored entry key, so the cap gate is not a Resource.Load.
	[NonSerialized()]
	ref map<string, int> m_mEntryMemberCount;

	//! The catalog entry a CONVERTED recruit group names. Synthesized rather than authored: the
	//! purchase list enumerates the faction's own array by index, so an entry that is not in it can
	//! never be bought, while the map, the roster and the restore walk's catalog gate all resolve it.
	[NonSerialized()]
	ref OVT_HighCommandGroupEntry m_ConvertedEntry;

	//! SERVER ONLY - member BODY entity to group id. Written BEFORE the group add, which is what keeps
	//! the body tracked: the modded SCR_AIGroup hooks untrack every agent that is not a player body, a
	//! registered recruit or - through this map - a registered High Command member (T6.7).
	[NonSerialized()]
	ref map<EntityID, string> m_mMemberToGroup;

	//! What the heartbeat last SENT per group id. The change filter, and nothing else, reads it.
	[NonSerialized()]
	ref map<string, ref OVT_HighCommandStatusEcho> m_mStatusEcho;

	//! SERVER ONLY - vehicle prefabs already reported as unpriceable, so a quote per selection change
	//! does not reprint the same warning.
	[NonSerialized()]
	ref array<ResourceName> m_aUnpriceableVehicles;

	//! SERVER ONLY - the refuel tick's fractional-dollar carry, keyed by GROUP id rather than by
	//! owner: two groups owned by the same player must never share one pot (Phase 10). Cleared in
	//! RemoveRecord, the single choke point every removal route already passes through.
	[NonSerialized()]
	ref OVT_FuelChargeLedger m_FuelChargeLedger;

	//! SERVER ONLY - group ids still waiting for their save-point rebuild, one per call-queue hop.
	[NonSerialized()]
	ref array<string> m_aRestoreQueue;

	//! SERVER ONLY - "<groupId>|<memberIndex>" for every stored body request in flight.
	//!
	//! RequestSpawn is ASYNCHRONOUS and its callback can also fire from INSIDE RequestSpawn for an
	//! instance already in memory, so a token is inserted before the request and consumed by whichever
	//! of the callback and the timeout arrives first.
	[NonSerialized()]
	ref array<string> m_aPendingBodySpawns;

	//! SERVER ONLY - prefab stand-ins a restored group was rebuilt with, per group id, in spawn order.
	//! One is retired for each stored body that actually comes back.
	[NonSerialized()]
	ref map<string, ref array<EntityID>> m_mRestorePlaceholders;

	//! CLIENT - the member cap the server sent in its JIP payload. The cap is a server-only difficulty
	//! field (§3.12), so a client has no other way to render "n / cap" and CONFIG_STREAM_VERSION does
	//! not move for it.
	protected int m_iReplicatedMemberCap;

	//! The persistence collection member bodies are stored in, resolved once and kept.
	//! Held without `ref`: the collection is owned by the persistence system (SCR_SpawnLogic's shape).
	protected PersistenceCollection m_MemberBodyCollection;

	//! Fires on EVERY machine when a group record appears. Arg: OVT_HighCommandRecord.
	ref ScriptInvoker m_OnHCGroupAdded = new ScriptInvoker();

	//! Fires on EVERY machine when a group record changes (order or heartbeat). Arg: the record.
	ref ScriptInvoker m_OnHCGroupUpdated = new ScriptInvoker();

	//! Fires on EVERY machine when a group record goes. Arg: the record, captured before the drop.
	ref ScriptInvoker m_OnHCGroupRemoved = new ScriptInvoker();

	//------------------------------------------------------------------------------------------------
	//! \return The manager singleton, or null before the game mode's OnPostInit has run.
	static OVT_HighCommandManagerComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		s_Instance = this;

		m_mGroups = new map<string, ref OVT_HighCommandRecord>();
		m_mGroupsByOwner = new map<string, ref array<string>>();
		m_mEntityToGroup = new map<EntityID, string>();
		m_Manifest = new OVT_HighCommandManifest();
		m_mEntryMemberCount = new map<string, int>();
		m_mMemberToGroup = new map<EntityID, string>();
		m_mStatusEcho = new map<string, ref OVT_HighCommandStatusEcho>();
		m_aUnpriceableVehicles = new array<ResourceName>();
		m_aRestoreQueue = new array<string>();
		m_aPendingBodySpawns = new array<string>();
		m_mRestorePlaceholders = new map<string, ref array<EntityID>>();
		m_FuelChargeLedger = new OVT_FuelChargeLedger();

		if (!GetGame() || !GetGame().GetCallqueue())
			return;

		// Started UNCONDITIONALLY and guarded inside the tick, the recruit sweep's rule: a timer that
		// starts and then refuses costs one branch every STATUS_SYNC_INTERVAL_MS on a client, while a
		// timer that never started because a guard read false too early costs the whole feature with
		// no symptom. The rearm and refuel ticks follow the same rule.
		GetGame().GetCallqueue().CallLater(SweepStatus, STATUS_SYNC_INTERVAL_MS, true);
		GetGame().GetCallqueue().CallLater(RearmTick, m_iRearmIntervalMs, true);
		GetGame().GetCallqueue().CallLater(RefuelTick, m_iRefuelIntervalMs, true);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] groupId A group's id.
	//! \return Its record, or null when unknown.
	OVT_HighCommandRecord GetGroup(string groupId)
	{
		if (!m_mGroups.Contains(groupId))
			return null;

		return m_mGroups[groupId];
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] ownerPersistentId The owning player.
	//! \return Every group that player owns, in table order. Never null.
	array<ref OVT_HighCommandRecord> GetGroupsByOwner(string ownerPersistentId)
	{
		array<ref OVT_HighCommandRecord> result = new array<ref OVT_HighCommandRecord>();

		if (!m_mGroupsByOwner.Contains(ownerPersistentId))
			return result;

		array<string> groupIds = m_mGroupsByOwner[ownerPersistentId];
		foreach (string groupId : groupIds)
		{
			OVT_HighCommandRecord record = GetGroup(groupId);
			if (record)
				result.Insert(record);
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] ownerPersistentId The owning player.
	//! \return Total members across every group that player owns (D12: the cap counts members).
	int GetMemberCount(string ownerPersistentId)
	{
		int total = 0;

		array<ref OVT_HighCommandRecord> groups = GetGroupsByOwner(ownerPersistentId);
		foreach (OVT_HighCommandRecord record : groups)
		{
			total += record.m_iTotalMembers;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The resolved per-player member cap (OVT_DifficultySettings.highCommandMemberCap),
	//! or OVT_HighCommandRules.DEFAULT_MEMBER_CAP before the config exists.
	//!
	//! On a CLIENT the difficulty settings are server-only, so the cap the server put at the head of
	//! its JIP payload wins - that is the whole reason it is in the payload (§3.12).
	int GetMemberCap()
	{
		if (!Replication.IsServer() && m_iReplicatedMemberCap > 0)
			return m_iReplicatedMemberCap;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config || !config.m_Difficulty)
			return OVT_HighCommandRules.DEFAULT_MEMBER_CAP;

		return config.GetHighCommandMemberCap();
	}

	//------------------------------------------------------------------------------------------------
	// PHASE 3 - THE AUTHORED CATALOG: composition, the required-item manifest and the quote
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! SERVER: starts the load-time spawn-inspect build of the required-item manifest (§3.4).
	//!
	//! ONE PASS OVER THE CATALOG, then one character prefab per call-queue hop inside the manifest.
	//! Idempotent - a prefab already captured or already queued is ignored.
	void PostGameStart()
	{
		if (!Replication.IsServer())
			return;

		// §3.11's spawn-on-load walk, queued FIRST so a save's groups are standing before anything
		// else competes for the call queue. One record per hop.
		QueueRestoreWalk();

		BuildManifestQueue();
	}

	//------------------------------------------------------------------------------------------------
	//! One pass over the catalog, collecting the distinct character prefabs the manifest must inspect.
	protected void BuildManifestQueue()
	{
		if (!m_Manifest)
			return;

		array<ResourceName> distinct = {};

		int count = GetEntryCount();
		for (int i = 0; i < count; i++)
		{
			OVT_HighCommandGroupEntry entry = GetEntryByIndex(i);
			if (!entry)
				continue;

			array<ResourceName> members = {};
			if (GetEntryMemberPrefabs(entry, members) < 1)
				continue;

			foreach (ResourceName member : members)
			{
				if (distinct.Find(member) == -1)
					distinct.Insert(member);
			}
		}

		if (distinct.IsEmpty())
			return;

		m_Manifest.QueueCharacters(distinct);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The faction whose m_aHighCommandGroups is the purchasable catalog, or null.
	OVT_Faction GetCatalogFaction()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return null;

		OVT_OverthrowFactionManager factions = OVT_Global.GetFactions();
		if (!factions)
			return null;

		return factions.GetOverthrowFactionByKey(config.m_sPlayerFaction);
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many entries a player may buy from; 0 when the catalog is unauthored.
	int GetEntryCount()
	{
		OVT_Faction faction = GetCatalogFaction();
		if (!faction)
			return 0;

		return faction.GetHighCommandGroupCount();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] index The index the client sent. Out of range answers null, never a clamp.
	//! \return The authored entry, or null.
	OVT_HighCommandGroupEntry GetEntryByIndex(int index)
	{
		OVT_Faction faction = GetCatalogFaction();
		if (!faction)
			return null;

		return faction.GetHighCommandGroup(index);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entryKey The entry's stable key, as stored on a record and in a save.
	//! \return The authored entry, the synthesized converted-recruit entry, or null when the catalog
	//! no longer publishes that key.
	OVT_HighCommandGroupEntry GetEntryByKey(string entryKey)
	{
		OVT_Faction faction = GetCatalogFaction();

		OVT_HighCommandGroupEntry authored;
		if (faction)
			authored = faction.GetHighCommandGroupByKey(entryKey);

		if (authored)
			return authored;

		// The converted-recruit key is answered LAST, so an authored entry always wins if a campaign
		// ever publishes one under it.
		if (entryKey == OVT_HighCommandRules.CONVERTED_ENTRY_KEY)
			return GetConvertedEntry();

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The synthesized entry every converted recruit group names (Phase 9).
	//!
	//! It has NO group prefab: a converted group's composition is the bodies that were moved into it,
	//! and a restored one's is the roster the save wrote. Its only real job is to give the map and the
	//! roster a name and an icon, and to satisfy the restore walk's "is this key still published?"
	//! gate - a converted group must not be dropped on load for not being purchasable.
	//! \return The converted-group entry. Never null.
	protected OVT_HighCommandGroupEntry GetConvertedEntry()
	{
		if (m_ConvertedEntry)
			return m_ConvertedEntry;

		m_ConvertedEntry = new OVT_HighCommandGroupEntry();
		m_ConvertedEntry.m_sKey = OVT_HighCommandRules.CONVERTED_ENTRY_KEY;
		m_ConvertedEntry.m_sTitle = "#OVT-HC_ConvertedGroupName";
		m_ConvertedEntry.m_sDescription = "#OVT-HC_ConvertedGroupDesc";
		m_ConvertedEntry.m_sGroupPrefab = ResourceName.Empty;
		m_ConvertedEntry.m_sVehiclePrefab = ResourceName.Empty;
		m_ConvertedEntry.m_sMapIcon = "Infantry_Friend";

		return m_ConvertedEntry;
	}

	//------------------------------------------------------------------------------------------------
	//! One entry's composition, read from the prefab source with nothing spawned.
	//! \param[in] entry The authored entry.
	//! \param[out] memberPrefabs Receives one entry per member.
	//! \return How many members the entry's group prefab defines.
	int GetEntryMemberPrefabs(notnull OVT_HighCommandGroupEntry entry, out array<ResourceName> memberPrefabs)
	{
		array<vector> offsets = {};
		int count = ReadComposition(entry.m_sGroupPrefab, memberPrefabs, offsets);

		if (entry.m_sKey != "")
			m_mEntryMemberCount.Set(entry.m_sKey, count);

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! How many members one entry buys. Cached - the cap gate reads it on every quote.
	//! \param[in] entry The authored entry.
	//! \return Its member count, or 0 when its group prefab defines none.
	int GetEntryMemberCount(notnull OVT_HighCommandGroupEntry entry)
	{
		if (entry.m_sKey != "" && m_mEntryMemberCount.Contains(entry.m_sKey))
			return m_mEntryMemberCount.Get(entry.m_sKey);

		array<ResourceName> memberPrefabs = {};
		return GetEntryMemberPrefabs(entry, memberPrefabs);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: the whole price of one entry at one place, for one player.
	//!
	//! THE ONLY PLACE A HIGH COMMAND PRICE IS MADE. The quote the player is shown and the amount the
	//! server takes are both this result, derived on the same machine at the same moment - the quote is
	//! advisory and re-derived at purchase time (D19), so a stale screen can never charge a stale price.
	//!
	//! The fee arithmetic is OVT_RecruitPurchaseRules', unchanged and unreimplemented: the warehouse
	//! split simply feeds TotalPrice a smaller gear subtotal. The VEHICLE is added on top at its full
	//! shop buy price, outside the fee multiplier and outside warehouse coverage (R7).
	//! \param[in] entry The authored entry.
	//! \param[in] pos Where the buying happens - the barracks.
	//! \param[in] persistentId The buying player, for the warehouse accessibility rule.
	//! \param[in] playerId Runtime id of the buyer, for their own price multiplier.
	//! \return The quote, or null when the entry has no composition at all.
	OVT_HighCommandQuote QuoteEntry(notnull OVT_HighCommandGroupEntry entry, vector pos, string persistentId, int playerId)
	{
		array<ResourceName> memberPrefabs = {};
		int memberCount = GetEntryMemberPrefabs(entry, memberPrefabs);
		if (memberCount < 1)
			return null;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config || !config.m_Difficulty)
			return null;

		OVT_HighCommandQuote quote = new OVT_HighCommandQuote();
		quote.m_iMemberCount = memberCount;
		quote.m_iMemberCost = config.m_Difficulty.baseRecruitCost * memberCount;
		quote.m_iSupportersRequired = OVT_HighCommandRules.SupportersRequired(memberCount, config.GetHighCommandSupportersPerMember());
		quote.m_aManifest = new array<ref OVT_ItemSourcingLine>();
		quote.m_aStores = new array<OVT_StorageComponent>();
		quote.m_iCoveredUnits = 0;
		quote.m_iCoveredValue = 0;
		quote.m_iChargedUnits = 0;
		quote.m_iChargedSubtotal = 0;
		quote.m_iVehicleCost = PriceVehicle(entry.m_sVehiclePrefab, pos, playerId);
		quote.m_iTotal = OVT_HighCommandRules.PurchaseTotal(quote.m_iMemberCost, quote.m_iVehicleCost);

		if (m_Manifest)
			m_Manifest.BuildEntryManifest(entry.m_sKey, memberPrefabs, pos, playerId, quote.m_aManifest);

		OVT_WarehouseStockUtils.CollectStores(pos, OVT_HighCommandRules.WAREHOUSE_RANGE, persistentId, quote.m_aStores);

		array<int> availablePerLine = {};
		foreach (OVT_ItemSourcingLine line : quote.m_aManifest)
		{
			availablePerLine.Insert(OVT_WarehouseStockUtils.CountAvailable(quote.m_aStores, line.m_sResource));
		}

		quote.m_iChargedSubtotal = OVT_ItemSourcingRules.SplitCoverage(quote.m_aManifest, availablePerLine, quote.m_iCoveredUnits, quote.m_iCoveredValue, quote.m_iChargedUnits);

		int crewTotal = OVT_RecruitPurchaseRules.TotalPrice(quote.m_iMemberCost, quote.m_iChargedSubtotal, config.m_Difficulty.recruitLoadoutFeeMultiplier);
		quote.m_iTotal = OVT_HighCommandRules.PurchaseTotal(crewTotal, quote.m_iVehicleCost);

		return quote;
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: what an entry's vehicle adds to its price - the FULL shop buy price a player would pay
	//! for the same vehicle here (R7, user's decision).
	//!
	//! REGISTRATION BEFORE THE LOOKUP, the manifest's rule: GetInventoryId() is a bare map index and
	//! answers 0 for an unregistered prefab, which is some other resource's price. An unpriceable
	//! vehicle is NOT silently free - it costs 0 and says so in the log, because that is a catalog
	//! defect somebody has to fix, not a discount.
	//! \param[in] vehiclePrefab The entry's vehicle; empty for a foot group.
	//! \param[in] pos Where the buying happens.
	//! \param[in] playerId Runtime id of the buyer, for their own price multiplier.
	//! \return The vehicle's buy price, or 0 when there is no vehicle or it cannot be priced.
	protected int PriceVehicle(ResourceName vehiclePrefab, vector pos, int playerId)
	{
		if (vehiclePrefab.IsEmpty())
			return 0;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
		{
			WarnUnpriceableVehicle(vehiclePrefab, "cannot be priced - there is no economy manager");
			return 0;
		}

		int price = economy.GetBuyPriceForPrefab(vehiclePrefab, pos, playerId);
		if (price < 0)
		{
			WarnUnpriceableVehicle(vehiclePrefab, "is not a registered economy resource and inherits none");
			return 0;
		}

		return price;
	}

	//------------------------------------------------------------------------------------------------
	//! Reports an unpriceable vehicle ONCE per prefab. A quote runs on every selection change, so an
	//! unconditional print would bury the log the moment somebody scrolled the purchase list.
	//! \param[in] vehiclePrefab The vehicle that could not be priced.
	//! \param[in] reason Why, as a sentence fragment.
	protected void WarnUnpriceableVehicle(ResourceName vehiclePrefab, string reason)
	{
		if (!m_aUnpriceableVehicles)
			m_aUnpriceableVehicles = new array<ResourceName>();

		if (m_aUnpriceableVehicles.Find(vehiclePrefab) != -1)
			return;

		m_aUnpriceableVehicles.Insert(vehiclePrefab);

		Print("[Overthrow] High Command vehicle '" + vehiclePrefab + "' " + reason + " - it is NOT being charged for", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: consumes the covered half of a quote from the warehouses it was quoted against.
	//!
	//! The CLAMP IS THE LEDGER'S: each line asks for its whole need and TakeUpTo answers with what was
	//! actually there, which is the same min(need, available) the coverage split used a moment earlier -
	//! one rule, one place, no second copy to drift.
	//! \param[in] quote The quote that was charged.
	//! \return How many units were taken.
	int ConsumeQuotedStock(notnull OVT_HighCommandQuote quote)
	{
		if (!Replication.IsServer())
			return 0;

		if (!quote.m_aStores || quote.m_aStores.IsEmpty() || !quote.m_aManifest)
			return 0;

		int taken = 0;

		foreach (OVT_ItemSourcingLine line : quote.m_aManifest)
		{
			if (!line)
				continue;

			taken += OVT_WarehouseStockUtils.TakeUpTo(quote.m_aStores, line.m_sResource, line.m_iNeeded);
		}

		return taken;
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: builds one group from an AUTHORED ENTRY, resolving its prefabs from the catalog.
	//!
	//! The entry-key resolver in front of SpawnGroup(). entryKey is what a record stores and what a
	//! save restores, so this is the only spawn path a purchase or a reload should use.
	//! \param[in] entryKey The authored entry's key.
	//! \param[in] position Where to put the group.
	//! \param[in] ownerPersistentId The owning player.
	//! \param[in] restoring A persisted record being rebuilt (§3.11), or null for a fresh purchase.
	//! \param[in] spawnPoints The host barracks' authored spawn points, or null to use `position`.
	//! \return The new record, or null when nothing was spawned.
	OVT_HighCommandRecord SpawnGroupFromEntry(string entryKey, vector position, string ownerPersistentId, OVT_HighCommandRecord restoring = null, OVT_SpawnPointComponent spawnPoints = null)
	{
		OVT_HighCommandGroupEntry entry = GetEntryByKey(entryKey);
		if (!entry)
		{
			Print("[Overthrow] No High Command entry is authored under the key '" + entryKey + "' - nothing spawned", LogLevel.ERROR);
			return null;
		}

		return SpawnGroup(entryKey, entry.m_sGroupPrefab, entry.m_sVehiclePrefab, position, ownerPersistentId, restoring, null, spawnPoints);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: registers a group record, indexing it by owner. Overwrites an existing id, if any.
	//! \param[in] record The record to add.
	void AddRecord(notnull OVT_HighCommandRecord record)
	{
		if (!Replication.IsServer())
			return;

		IndexRecord(record);
	}

	//------------------------------------------------------------------------------------------------
	//! Writes one record into the table and the owner index, on EITHER machine.
	//!
	//! Ungated on purpose: it is what AddRecord does after its server gate, what deserialization does
	//! before that gate is meaningful, and what a client's delta handler does with no gate at all.
	//! \param[in] record The record to index. A record with no id is ignored.
	protected void IndexRecord(notnull OVT_HighCommandRecord record)
	{
		if (record.m_sGroupId == "")
			return;

		m_mGroups[record.m_sGroupId] = record;

		if (!m_mGroupsByOwner.Contains(record.m_sOwnerPersistentId))
			m_mGroupsByOwner[record.m_sOwnerPersistentId] = new array<string>();

		if (m_mGroupsByOwner[record.m_sOwnerPersistentId].Find(record.m_sGroupId) == -1)
			m_mGroupsByOwner[record.m_sOwnerPersistentId].Insert(record.m_sGroupId);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: drops a group record everywhere - here and on every client (delta #9).
	//!
	//! EVERY removal route lands here (dismissal, a wipe through the group's OnDelete, a save record
	//! that could not be rebuilt), so the broadcast belongs here and nowhere else. A second call for
	//! the same id returns before broadcasting, because the record has already gone.
	//! \param[in] groupId The group to remove.
	void RemoveRecord(string groupId)
	{
		if (!Replication.IsServer())
			return;

		if (!GetGroup(groupId))
			return;

		// The one choke point every removal route passes through - dismissal, a wipe, a save record
		// that could not be rebuilt - so it is also the one place the refuel tick's fractional-dollar
		// carry can be dropped without growing unbounded for the length of a campaign.
		if (m_FuelChargeLedger)
			m_FuelChargeLedger.Clear(groupId);

		BroadcastGroupRemoved(groupId);
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the record table from a save - RECORDS ONLY. Nothing is spawned here, so every live
	//! entity id stays EntityID.INVALID; PostGameStart's restore walk (§3.11) is what puts these groups
	//! back in the world, and it identifies them by exactly that INVALID id.
	//! \param[in] records Persisted group records, as read by OVT_HighCommandManagerSerializer.
	void ApplyPersistedGroups(array<ref OVT_PersistedHighCommandGroup> records)
	{
		if (!records)
			return;

		// Written directly, not through AddRecord() - deserialization runs while the world is still
		// being built, before Replication.IsServer() is meaningful (the recruit manager's
		// ApplyPersistedRecruits precedent).
		if (!m_mGroups)
			m_mGroups = new map<string, ref OVT_HighCommandRecord>();

		if (!m_mGroupsByOwner)
			m_mGroupsByOwner = new map<string, ref array<string>>();

		foreach (OVT_PersistedHighCommandGroup persisted : records)
		{
			if (!persisted || persisted.groupId == "" || persisted.owner == "")
				continue;

			// THE LIVE WORLD WINS. Saved data can also be re-applied to an already running session
			// (OVT_PersistenceManagerComponent.ReapplyLatestSaveData), and replacing the record of a group
			// that is standing in the world would orphan the entity: nothing would ever point at it again.
			OVT_HighCommandRecord live = GetGroup(persisted.groupId);
			if (live && GetGroupEntity(live))
				continue;

			OVT_HighCommandRecord record = new OVT_HighCommandRecord();
			record.m_sGroupId = persisted.groupId;
			record.m_sOwnerPersistentId = persisted.owner;
			record.m_sEntryKey = persisted.entryKey;
			record.m_iStance = persisted.stance;
			record.m_vDestination = persisted.destination;
			record.m_vLastKnownPosition = persisted.lastPosition;
			// Not persisted: status is measured, and the first heartbeat after the rebuild sets it.
			record.m_iStatusFlags = 0;
			record.m_aMemberBodyIds = persisted.memberBodyIds;
			record.m_aMemberPrefabs = persisted.memberPrefabs;
			// The prefab list is the one that is always written (D8's fallback); a body id per member
			// only exists once a member body has been captured.
			record.m_iTotalMembers = Math.Max(persisted.memberBodyIds.Count(), persisted.memberPrefabs.Count());
			record.m_iAliveMembers = record.m_iTotalMembers;
			record.m_GroupEntityId = EntityID.INVALID;
			record.m_VehicleEntityId = EntityID.INVALID;

			IndexRecord(record);
		}
	}

	//------------------------------------------------------------------------------------------------
	// PHASE 2 - THE LIVE GROUP: spawn, faction, vehicle, stances, dismissal, save-point sweep
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! SERVER: builds one High Command group - entity, members, faction, record - and holds it where
	//! it was spawned.
	//!
	//! ⚠ THE GROUP MUST GAIN ITS FIRST MEMBER IN THE FRAME IT IS CREATED. m_bDeleteWhenEmpty deletes a
	//! group whose last member LEAVES, and a group that never received one is ours to destroy - which
	//! is what the failure path below does (CreateInactiveGroupFor's rule).
	//!
	//! RESTORING (§3.11): pass the persisted record and it keeps its id, owner, stance, destination and
	//! stored body ids, and is rebuilt from THE ROSTER IT WAS SAVED WITH rather than the catalog entry's
	//! - a squad that lost two men must not reload at full strength (F18).
	//! \param[in] entryKey The authored entry this group was bought from.
	//! \param[in] groupPrefab Source group prefab - its m_aUnitPrefabSlots IS the composition.
	//! \param[in] vehiclePrefab Optional vehicle; empty means a foot group.
	//! \param[in] position Where to put the group.
	//! \param[in] ownerPersistentId The owning player.
	//! ADOPTING (Phase 9): pass live bodies and NOTHING is spawned for members - they are moved out of
	//! whatever group they are in and into this one, which is what keeps a converted recruit's own body
	//! (and therefore its gear) instead of building a new man from a prefab.
	//! SPAWN POINTS (R8): a barracks that authors OVT_SpawnPointComponent places the group and its
	//! vehicle on its own points - `position` is then only the fallback for a host that authors none.
	//! \param[in] restoring A persisted record being rebuilt, or null for a fresh purchase.
	//! \param[in] adopting Live bodies to move into the new group instead of spawning members.
	//! \param[in] spawnPoints The host's authored spawn points, or null to use `position`.
	//! \return The new record, or null when nothing was spawned. On failure nothing is left behind.
	OVT_HighCommandRecord SpawnGroup(string entryKey, ResourceName groupPrefab, ResourceName vehiclePrefab, vector position, string ownerPersistentId, OVT_HighCommandRecord restoring = null, array<IEntity> adopting = null, OVT_SpawnPointComponent spawnPoints = null)
	{
		if (!Replication.IsServer())
			return null;

		if (m_sGroupPrefab.IsEmpty())
		{
			Print("[Overthrow] No High Command group prefab is set on the High Command manager - no group can be spawned", LogLevel.ERROR);
			return null;
		}

		array<ResourceName> memberPrefabs = {};
		array<vector> memberOffsets = {};

		if (adopting)
		{
			if (adopting.IsEmpty())
			{
				Print("[Overthrow] A High Command conversion was handed no bodies - nothing to convert", LogLevel.ERROR);
				return null;
			}
		}
		else if (!ReadRestoredComposition(restoring, memberPrefabs, memberOffsets) && ReadComposition(groupPrefab, memberPrefabs, memberOffsets) < 1)
		{
			Print("[Overthrow] High Command entry '" + entryKey + "' names a group prefab with no members (" + groupPrefab + ") - nothing to spawn", LogLevel.ERROR);
			return null;
		}

		// Minted before anything is spawned: a record the table would refuse must cost nothing. A
		// restored group keeps the id everything already persisted about it refers to.
		string groupId;
		if (restoring)
			groupId = restoring.m_sGroupId;
		else
			groupId = MintGroupId(ownerPersistentId);

		if (groupId == "")
		{
			Print("[Overthrow] Could not mint a unique High Command group id for '" + ownerPersistentId + "' - nothing spawned", LogLevel.ERROR);
			return null;
		}

		// The authored point when the host has one, so the group entity, the DEFEND destination and the
		// members all agree on where "here" is - a building's own origin is inside the building (R8).
		vector groupPosition = ResolveSpawnAnchor(spawnPoints, position);

		SCR_AIGroup group = SCR_AIGroup.Cast(OVT_Global.SpawnEntityPrefab(m_sGroupPrefab, groupPosition));
		if (!group)
		{
			Print("[Overthrow] The High Command group prefab did not produce an SCR_AIGroup - entry '" + entryKey + "' cannot be spawned", LogLevel.ERROR);
			return null;
		}

		OVT_HighCommandGroupComponent marker = OVT_HighCommandGroupComponent.Cast(group.FindComponent(OVT_HighCommandGroupComponent));
		if (!marker)
		{
			// REFUSED, NOT TOLERATED: without the marker nothing would ever remove this group's
			// observer or delete its waypoints.
			SCR_EntityHelper.DeleteEntityAndChildren(group);
			Print("[Overthrow] The High Command group prefab has no OVT_HighCommandGroupComponent - entry '" + entryKey + "' cannot be spawned", LogLevel.ERROR);
			return null;
		}

		// While the group is still EMPTY: SetFaction rewrites the affiliation of every agent already
		// in it (SCR_AIGroup.c:2112-2120).
		StampGroupFaction(group);

		array<IEntity> members = {};
		array<ResourceName> spawnedPrefabs = {};
		if (adopting)
			AdoptMembers(group, groupId, adopting, members, spawnedPrefabs);
		else
			SpawnMembers(group, groupId, memberPrefabs, memberOffsets, groupPosition, spawnPoints, members, spawnedPrefabs);

		if (members.IsEmpty())
		{
			// A group that never received its first agent is never deleted by vanilla - its own
			// attribute says so - so it is ours.
			SCR_EntityHelper.DeleteEntityAndChildren(group);
			Print("[Overthrow] No member of High Command entry '" + entryKey + "' could be spawned or joined - deleting the group", LogLevel.ERROR);
			return null;
		}

		OVT_HighCommandRecord record = restoring;
		if (!record)
		{
			record = new OVT_HighCommandRecord();
			record.m_sGroupId = groupId;
			record.m_sOwnerPersistentId = ownerPersistentId;
			record.m_sEntryKey = entryKey;
			record.m_iStance = OVT_EHighCommandStance.DEFEND;
			record.m_vDestination = groupPosition;
			record.m_iStatusFlags = 0;
			record.m_aMemberBodyIds = {};
		}

		record.m_vLastKnownPosition = groupPosition;
		record.m_iAliveMembers = members.Count();
		record.m_iTotalMembers = members.Count();
		record.m_aMemberPrefabs = {};
		record.m_GroupEntityId = group.GetID();
		record.m_VehicleEntityId = EntityID.INVALID;

		// One entry per member that ACTUALLY joined (D8's rebuild fallback), not per authored slot.
		foreach (ResourceName memberPrefab : spawnedPrefabs)
		{
			record.m_aMemberPrefabs.Insert(memberPrefab);
		}

		marker.SetGroupId(record.m_sGroupId);
		marker.SetOwnerPersistentId(ownerPersistentId);

		AddRecord(record);
		m_mEntityToGroup[record.m_GroupEntityId] = record.m_sGroupId;

		// Every member is a stand-in until its stored body answers; one is retired per body that does.
		if (restoring)
			RememberRestorePlaceholders(groupId, members);

		AttachVehicle(record, marker, vehiclePrefab, groupPosition, spawnPoints);

		ApplyStance(record);

		BroadcastGroupCreated(record);

		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: re-points a group at a destination and stance, replacing its waypoints.
	//! \param[in] groupId The group to order.
	//! \param[in] stance An OVT_EHighCommandStance ordinal.
	//! \param[in] destination Where to send it.
	//! \return True when the order was applied.
	bool OrderGroup(string groupId, int stance, vector destination)
	{
		if (!Replication.IsServer())
			return false;

		OVT_HighCommandRecord record = GetGroup(groupId);
		if (!record)
			return false;

		if (!OVT_HighCommandRules.IsStanceValid(stance))
			return false;

		if (!OVT_HighCommandRules.IsDestinationLegal(destination))
			return false;

		record.m_iStance = stance;
		record.m_vDestination = ClampToTerrain(destination);

		ApplyStance(record);

		BroadcastGroupOrdered(record);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: takes a group out of the world entirely - members, vehicle, waypoints, observer, record.
	//!
	//! ORDER IS LOAD-BEARING, three times over:
	//!   - delete-when-empty is switched OFF first, so emptying the group cannot queue a second
	//!     deletion of an entity this method is about to delete itself;
	//!   - the members go before the vehicle, so no vehicle is ever deleted over an occupant;
	//!   - the group entity goes last, and its OnDelete is what removes the observer, deletes the
	//!     waypoints and drops the record.
	//! \param[in] groupId The group to dismiss.
	//! \return True when a group was dismissed.
	bool DismissGroup(string groupId)
	{
		if (!Replication.IsServer())
			return false;

		OVT_HighCommandRecord record = GetGroup(groupId);
		if (!record)
			return false;

		EntityID groupEntityId = record.m_GroupEntityId;
		SCR_AIGroup group = GetGroupEntity(record);
		IEntity vehicle = GetVehicleEntity(record);

		if (group)
		{
			group.SetDeleteWhenEmpty(false);

			array<IEntity> members = {};
			CollectMembers(group, members);

			foreach (IEntity member : members)
			{
				UnregisterMemberBody(member.GetID());
				SCR_EntityHelper.DeleteEntityAndChildren(member);
			}
		}

		if (vehicle)
			OVT_WorldUtils.DeleteEntityTree(vehicle);

		if (group)
			SCR_EntityHelper.DeleteEntityAndChildren(group);

		// The group's OnDelete already did both of these; a group entity that had gone missing did not.
		m_mEntityToGroup.Remove(groupEntityId);
		RemoveRecord(groupId);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes every live group's current position back onto its record, and drops entity mappings whose
	//! entity has gone. Called before every save (the SyncRecruitPositions precedent), never per frame.
	void SyncGroupPositions()
	{
		if (!Replication.IsServer())
			return;

		if (!m_mEntityToGroup)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		array<EntityID> staleEntities = {};

		foreach (EntityID entityId, string groupId : m_mEntityToGroup)
		{
			SCR_AIGroup group = SCR_AIGroup.Cast(world.FindEntityByID(entityId));
			if (!group)
			{
				staleEntities.Insert(entityId);
				continue;
			}

			OVT_HighCommandRecord record = GetGroup(groupId);
			if (!record)
				continue;

			record.m_vLastKnownPosition = ResolveGroupPosition(group);

			// Save-point work, never tick work: this MATERIALISES a persistence record per member, and
			// the save point is about to write those characters anyway (the recruit sweep's rule).
			CaptureMemberRoster(record, group);
		}

		// Removing inside the loop above would invalidate the iteration.
		foreach (EntityID staleId : staleEntities)
		{
			m_mEntityToGroup.Remove(staleId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The group entity told the manager it is being destroyed - by dismissal, by a wipe (vanilla's
	//! delete-when-empty) or by world teardown. The record goes with it.
	//! \param[in] groupId The record the dying entity was stamped with.
	//! \param[in] entityId The dying entity.
	void OnGroupEntityDeleted(string groupId, EntityID entityId)
	{
		if (m_mEntityToGroup)
			m_mEntityToGroup.Remove(entityId);

		if (groupId == "")
			return;

		RemoveRecord(groupId);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER ONLY BY CONSTRUCTION. m_GroupEntityId is never replicated, so on a client this is always
	//! a lookup of EntityID.INVALID - a client reads records and nothing else (T6.4).
	//! \param[in] record A group record.
	//! \return Its live group entity, or null when it has none.
	SCR_AIGroup GetGroupEntity(notnull OVT_HighCommandRecord record)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return null;

		return SCR_AIGroup.Cast(world.FindEntityByID(record.m_GroupEntityId));
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] record A group record.
	//! \return Its live vehicle, or null for a foot group.
	IEntity GetVehicleEntity(notnull OVT_HighCommandRecord record)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return null;

		return world.FindEntityByID(record.m_VehicleEntityId);
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces a group's waypoints with the kit its stance asks for (§3.5).
	//!
	//! Every waypoint spawned here is handed to the group component, which is what deletes it on the
	//! next order and on the group's death. Every position is terrain-clamped: an unclamped waypoint
	//! sits at the caller's Y and the AI walks to a spot nobody can stand on.
	//! \param[in] record The group being ordered.
	void ApplyStance(notnull OVT_HighCommandRecord record)
	{
		SCR_AIGroup group = GetGroupEntity(record);
		if (!group)
			return;

		OVT_HighCommandGroupComponent marker = OVT_HighCommandGroupComponent.Cast(group.FindComponent(OVT_HighCommandGroupComponent));
		if (!marker)
			return;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return;

		marker.ClearOwnedWaypoints();

		vector destination = ClampToTerrain(record.m_vDestination);

		if (record.m_iStance == OVT_EHighCommandStance.ATTACK)
		{
			GiveWaypoint(group, marker, config.SpawnSearchAndDestroyWaypoint(destination));
			return;
		}

		GiveWaypoint(group, marker, config.SpawnMoveWaypoint(destination));

		if (record.m_iStance == OVT_EHighCommandStance.PATROL)
		{
			GivePatrolRing(group, marker, config, destination);
			return;
		}

		GiveWaypoint(group, marker, config.SpawnDefendWaypoint(destination, 0));
	}

	//------------------------------------------------------------------------------------------------
	//! Reads a source group prefab's composition WITHOUT spawning anything (D4).
	//! \param[in] groupPrefab The source group prefab.
	//! \param[out] memberPrefabs One entry per member.
	//! \param[out] memberOffsets Formation offset per member, in group-local space.
	//! \return How many members the prefab defines.
	protected int ReadComposition(ResourceName groupPrefab, out array<ResourceName> memberPrefabs, out array<vector> memberOffsets)
	{
		memberPrefabs.Clear();
		memberOffsets.Clear();

		if (groupPrefab.IsEmpty())
			return 0;

		Resource resource = Resource.Load(groupPrefab);
		if (!resource || !resource.IsValid())
			return 0;

		BaseResourceObject resourceObject = resource.GetResource();
		if (!resourceObject)
			return 0;

		IEntitySource source = resourceObject.ToEntitySource();
		if (!source)
			return 0;

		// The static lives on the COMPONENT CLASS, not the entity class (SCR_AIGroupClass.c:20).
		SCR_AIGroupClass.GetMembers(source, memberPrefabs, memberOffsets);

		// GetMembers answers 0 for a prefab whose formation cannot be resolved (no AIWorld, an
		// unknown DefaultFormation) and never reads the slots at all. The slots are the composition;
		// the offsets are only where they stand, so fall back to a ring rather than refuse a purchase.
		if (memberPrefabs.IsEmpty())
			source.Get("m_aUnitPrefabSlots", memberPrefabs);

		FillMemberOffsets(memberPrefabs, memberOffsets);

		return memberPrefabs.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns and joins every member of a composition. Synchronous, so the group has its first member
	//! in the frame it was created.
	//! \param[in] group The group being filled.
	//! \param[in] groupId The record id every member is registered against BEFORE it joins (T6.7).
	//! \param[in] memberPrefabs The composition.
	//! \param[in] memberOffsets Where each member stands, relative to the group.
	//! \param[in] groupPosition The group's position.
	//! \param[in] spawnPoints The host's authored spawn points, or null to use the formation offsets.
	//! \param[out] members Every member that actually joined.
	//! \param[out] spawnedPrefabs The prefab of each of those members, in the same order.
	protected void SpawnMembers(notnull SCR_AIGroup group, string groupId, notnull array<ResourceName> memberPrefabs, notnull array<vector> memberOffsets, vector groupPosition, OVT_SpawnPointComponent spawnPoints, out array<IEntity> members, out array<ResourceName> spawnedPrefabs)
	{
		members.Clear();
		spawnedPrefabs.Clear();

		bool useAuthoredPoints = spawnPoints && spawnPoints.HasSpawnPoints();

		for (int i = 0; i < memberPrefabs.Count(); i++)
		{
			// GetSpawnPoint() picks a random authored point, applies the building's rotation and clamps
			// to the surface - it is what a barracks was authored for (R8).
			vector memberPosition;
			if (useAuthoredPoints)
				memberPosition = spawnPoints.GetSpawnPoint();
			else
				memberPosition = ClampToTerrain(groupPosition + memberOffsets[i]);

			SCR_ChimeraCharacter character = OVT_WorldUtils.SpawnCharacterEntity(memberPrefabs[i], memberPosition);
			if (!character)
				continue;

			// ⚠ BEFORE THE GROUP ADD, NOT AFTER. The modded SCR_AIGroup untracks every agent that joins
			// any group unless it is a player body or a REGISTERED recruit/High Command member, and an
			// untracked body has no persistence id - which is D8's whole mechanism.
			RegisterMemberBody(character, groupId);

			if (!group.AddAIEntityToGroup(character) || FindParentGroup(character) != group)
			{
				// AddAIEntityToGroup answers TRUE for a member some other system got to first, so the
				// agent hierarchy is the only honest confirmation.
				UnregisterMemberBody(character.GetID());
				SCR_EntityHelper.DeleteEntityAndChildren(character);
				continue;
			}

			StampMemberFaction(character);
			members.Insert(character);
			spawnedPrefabs.Insert(memberPrefabs[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Moves LIVE bodies into a new group instead of spawning members (Phase 9's conversion seam).
	//!
	//! ⚠ THE OLD GROUP HAS TO LET GO FIRST. AddAIEntityToGroup refuses to re-parent an agent that
	//! already belongs to a group (Entities/SCR_AIGroup.c:1933-1934), so without the exit every body
	//! would silently stay where it was and the conversion would produce an empty group.
	//!
	//! ⚠ AND THE REGISTRATION HAS TO COME BEFORE THE ADD. The modded SCR_AIGroup untracks every agent
	//! that joins any group unless a manager has already claimed it, and the persistence id an
	//! untracked body loses IS this man's gear (T6.7).
	//!
	//! A body that could not be moved is put back where it came from and left out of the group: its
	//! recruit record is kept for the same reason (DeactivateRecruit's rollback rule).
	//! \param[in] group The group being filled.
	//! \param[in] groupId The record id every body is registered against BEFORE it joins.
	//! \param[in] bodies The live bodies to move in.
	//! \param[out] members Every body that actually joined.
	//! \param[out] memberPrefabs The prefab of each of those bodies, in the same order.
	protected void AdoptMembers(notnull SCR_AIGroup group, string groupId, notnull array<IEntity> bodies, out array<IEntity> members, out array<ResourceName> memberPrefabs)
	{
		members.Clear();
		memberPrefabs.Clear();

		foreach (IEntity body : bodies)
		{
			if (!body)
				continue;

			SCR_AIGroup previous = FindParentGroup(body);
			if (previous)
				previous.RemoveAgentFromControlledEntity(body);

			RegisterMemberBody(body, groupId);

			if (!group.AddAIEntityToGroup(body) || FindParentGroup(body) != group)
			{
				UnregisterMemberBody(body.GetID());

				// Best effort: if that was the old group's last member, vanilla has already queued its
				// deletion and the body ends up commanded by nobody either way.
				if (previous)
					previous.AddAIEntityToGroup(body);

				Print("[Overthrow] A body could not be moved into High Command group " + groupId + " - it is left out of the group and keeps whatever record it had", LogLevel.WARNING);
				continue;
			}

			StampMemberFaction(body);

			members.Insert(body);

			EntityPrefabData prefabData = body.GetPrefabData();
			if (prefabData)
				memberPrefabs.Insert(prefabData.GetPrefabName());
			else
				memberPrefabs.Insert(ResourceName.Empty);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the group's vehicle and starts seating its crew. A foot group, or a vehicle with nowhere
	//! clear to stand, simply stays on foot.
	//! \param[in] record The group's record.
	//! \param[in] marker The group's component.
	//! \param[in] vehiclePrefab The vehicle to spawn; empty means a foot group.
	//! \param[in] groupPosition Where the group is.
	//! \param[in] spawnPoints The host's authored spawn points, or null to search near the group.
	protected void AttachVehicle(notnull OVT_HighCommandRecord record, notnull OVT_HighCommandGroupComponent marker, ResourceName vehiclePrefab, vector groupPosition, OVT_SpawnPointComponent spawnPoints)
	{
		if (vehiclePrefab.IsEmpty())
			return;

		vector vehiclePosition;
		vector vehicleAngles;

		// The authored vehicle point first - it carries an angle, so the truck arrives facing the way
		// it was parked rather than at whatever bearing a ring search happened to land on (R8).
		bool placed = false;
		if (spawnPoints && spawnPoints.HasVehicleSpawnPoints())
			placed = spawnPoints.GetVehicleSpawnPoint(vehiclePosition, vehicleAngles);

		if (!placed)
			placed = OVT_WorldUtils.FindVehicleSpawnNear(groupPosition, vehiclePosition, vehicleAngles);

		if (!placed)
		{
			Print("[Overthrow] No clear spot for High Command group " + record.m_sGroupId + "'s vehicle - it stays on foot", LogLevel.WARNING);
			return;
		}

		IEntity vehicle = OVT_Global.SpawnEntityPrefab(vehiclePrefab, vehiclePosition, vehicleAngles);
		if (!vehicle)
		{
			Print("[Overthrow] High Command group " + record.m_sGroupId + "'s vehicle prefab failed to spawn (" + vehiclePrefab + ") - it stays on foot", LogLevel.WARNING);
			return;
		}

		// Rebuilt with its group on the next boot, so a persistence record for it would come back
		// alongside the rebuilt one (BUG-118's rule, applied to the whole HC footprint).
		OVT_PersistenceManagerComponent.UntrackTransient(vehicle);

		record.m_VehicleEntityId = vehicle.GetID();
		marker.SetVehicleEntityId(record.m_VehicleEntityId);

		GetGame().GetCallqueue().CallLater(SeatCrew, 0, false, record.m_sGroupId);
	}

	//------------------------------------------------------------------------------------------------
	//! Seats a group's crew: driver's seat, then every turret, then cargo (R6).
	//!
	//! ⚠ EVERY MAN IS GIVEN AN EXPLICIT, DISTINCT COMPARTMENT SLOT, not a compartment TYPE. The type
	//! form of MoveInVehicle() searches for a free slot itself and only keeps two men out of one seat
	//! by locking the slot it hands out for a frame - and the occupancy that would otherwise separate
	//! them is not established by the call, it arrives with the owner RPC the call sends. Seating one
	//! man per call-queue hop raced that lock: whichever of the two call-queue entries ran first
	//! decided whether the second man was offered the driver's seat again, and when he was, he ended
	//! up nowhere at all. Distinct slots make the question moot, so the whole crew is seated in one
	//! pass (the EnsureTurretsManned() shape, OVT_VehicleSpawningDeploymentModule.c:315).
	//!
	//! ⚠ THE TURRET IS NOT ON THE VEHICLE. On a technical the gun is a SlotManagerComponent child with
	//! its own compartment manager (UAZ469_PKM_FIA.et -> UAZ469_FIA_weapon_mount_6T5_PKM.et), so the
	//! seat walk has to descend into children or there is no gunner's seat to find.
	//! \param[in] groupId The group being seated. Re-resolved here, so a dismissed group seats nobody.
	protected void SeatCrew(string groupId)
	{
		OVT_HighCommandRecord record = GetGroup(groupId);
		if (!record)
			return;

		SCR_AIGroup group = GetGroupEntity(record);
		if (!group)
			return;

		IEntity vehicle = GetVehicleEntity(record);
		if (!vehicle)
			return;

		array<BaseCompartmentSlot> seats = {};
		CollectFreeSeats(vehicle, seats);

		if (seats.IsEmpty())
		{
			Print("[Overthrow] High Command group " + groupId + "'s vehicle offered no free compartment - the crew stays on foot", LogLevel.WARNING);
			return;
		}

		array<AIAgent> agents = {};
		group.GetAgents(agents);

		int seatIndex = 0;
		int wanting = 0;
		int seated = 0;

		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity character = agent.GetControlledEntity();
			if (!character)
				continue;

			SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(character.FindComponent(SCR_CompartmentAccessComponent));
			if (!access)
				continue;

			// Already aboard - a restored body that replaced a seated stand-in, or a man re-seated by
			// the load walk. He keeps his seat and does NOT consume one from the plan.
			if (access.IsInCompartment())
				continue;

			if (seatIndex >= seats.Count())
				break;

			wanting++;

			while (seatIndex < seats.Count())
			{
				BaseCompartmentSlot seat = seats[seatIndex];
				seatIndex++;

				if (access.MoveInVehicle(vehicle, seat.GetType(), false, seat))
				{
					seated++;
					break;
				}
			}
		}

		// The R6 tell: a crewman left standing beside a vehicle that had a seat for him. Names the
		// counts, because "2 free compartments, 1 seated" is a refused seat and "1 free compartment,
		// 1 seated" is a vehicle with no gunner's position at all.
		if (seated < wanting)
			Print("[Overthrow] High Command group " + groupId + " seated " + seated.ToString() + " of " + wanting.ToString() + " crew into " + seats.Count().ToString() + " free compartments - the rest are on foot", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Every free compartment on a vehicle, in crew order: the driver's seat, then turrets, then cargo.
	//!
	//! Descends into child entities because a turret is usually one (see SeatCrew's header), and
	//! de-duplicates by slot because a child's compartments can also be registered with its parent's
	//! manager - two entries for one seat would silently cost a man his place.
	//! \param[in] vehicle The vehicle to read.
	//! \param[out] seats Receives the free slots in crew order; cleared first.
	protected void CollectFreeSeats(notnull IEntity vehicle, out array<BaseCompartmentSlot> seats)
	{
		seats.Clear();

		array<BaseCompartmentSlot> all = {};
		CollectCompartments(vehicle, all, 0);

		AppendFreeSeatsOfType(all, ECompartmentType.PILOT, seats);
		AppendFreeSeatsOfType(all, ECompartmentType.TURRET, seats);
		AppendFreeSeatsOfType(all, ECompartmentType.CARGO, seats);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entity The vehicle or one of its children.
	//! \param[out] found Accumulates every compartment slot reachable from it.
	//! \param[in] depth Current recursion depth.
	protected void CollectCompartments(notnull IEntity entity, notnull array<BaseCompartmentSlot> found, int depth)
	{
		if (depth > SEAT_SEARCH_DEPTH)
			return;

		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(entity.FindComponent(BaseCompartmentManagerComponent));
		if (compartments)
		{
			array<BaseCompartmentSlot> slots = {};
			compartments.GetCompartments(slots);

			foreach (BaseCompartmentSlot slot : slots)
			{
				if (!slot)
					continue;

				if (found.Find(slot) == -1)
					found.Insert(slot);
			}
		}

		IEntity child = entity.GetChildren();
		while (child)
		{
			CollectCompartments(child, found, depth + 1);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] all Every compartment found on a vehicle.
	//! \param[in] type The compartment type to take this pass.
	//! \param[out] seats Receives the free ones of that type, in the order they were found.
	protected void AppendFreeSeatsOfType(notnull array<BaseCompartmentSlot> all, ECompartmentType type, notnull array<BaseCompartmentSlot> seats)
	{
		foreach (BaseCompartmentSlot slot : all)
		{
			if (!slot || slot.GetType() != type)
				continue;

			if (!slot.IsCompartmentAccessible())
				continue;

			if (slot.GetOccupant())
				continue;

			seats.Insert(slot);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The road-snapped [patrol -> wait] ring of §3.5, built here rather than through
	//! OVT_OverthrowConfigComponent.GivePatrolWaypoints because that helper hands nothing back and its
	//! nine waypoints could never be owned - which for a group a player re-orders is a leak per order.
	//! \param[in] group The group.
	//! \param[in] marker Its component, which takes ownership of every waypoint spawned here.
	//! \param[in] config The waypoint prefab source.
	//! \param[in] centre The destination the ring is drawn around.
	protected void GivePatrolRing(notnull SCR_AIGroup group, notnull OVT_HighCommandGroupComponent marker, notnull OVT_OverthrowConfigComponent config, vector centre)
	{
		array<AIWaypoint> ring = {};

		float angle = 0;
		for (int i = 0; i < PATROL_RING_POINTS; i++)
		{
			vector point = centre + (Vector(0, angle, 0).AnglesToVector() * OVT_HighCommandRules.PATROL_RADIUS);
			point = ClampToTerrain(OVT_WorldUtils.FindNearestRoad(point));

			AIWaypoint patrol = config.SpawnPatrolWaypoint(point);
			if (patrol)
			{
				ring.Insert(patrol);
				marker.AddOwnedWaypoint(patrol);
			}

			AIWaypoint wait = config.SpawnWaitWaypoint(point, PATROL_WAIT_SECONDS);
			if (wait)
			{
				ring.Insert(wait);
				marker.AddOwnedWaypoint(wait);
			}

			angle += 360.0 / PATROL_RING_POINTS;
		}

		if (ring.IsEmpty())
			return;

		// Owned BEFORE the cast, so a cycle prefab that produced the wrong type is still disposed of.
		AIWaypoint cycleWaypoint = config.SpawnBasicCycleWaypoint(centre);
		marker.AddOwnedWaypoint(cycleWaypoint);

		AIWaypointCycle cycle = AIWaypointCycle.Cast(cycleWaypoint);
		if (!cycle)
		{
			// No cycle: give the group the first leg so it walks the perimeter once instead of
			// standing still. Everything spawned above is already owned either way.
			group.AddWaypoint(ring[0]);
			return;
		}

		cycle.SetWaypoints(ring);
		cycle.SetRerunCounter(-1);
		group.AddWaypoint(cycle);
	}

	//------------------------------------------------------------------------------------------------
	//! Wires one waypoint to a group and hands it to the component that will delete it.
	//! \param[in] group The group.
	//! \param[in] marker Its component.
	//! \param[in] waypoint The waypoint; null is a no-op.
	protected void GiveWaypoint(notnull SCR_AIGroup group, notnull OVT_HighCommandGroupComponent marker, AIWaypoint waypoint)
	{
		if (!waypoint)
			return;

		group.AddWaypoint(waypoint);
		marker.AddOwnedWaypoint(waypoint);
	}

	//------------------------------------------------------------------------------------------------
	//! Stamps the group with the configured player faction. Called while it is still empty.
	//! \param[in] group The group.
	protected void StampGroupFaction(notnull SCR_AIGroup group)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return;

		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return;

		Faction playerFaction = factionManager.GetFactionByKey(config.m_sPlayerFaction);
		if (playerFaction)
			group.SetFaction(playerFaction);
	}

	//------------------------------------------------------------------------------------------------
	//! Affiliates one member with the configured player faction - the SetRecruitFaction body.
	//!
	//! Always the configured resistance faction, never vanilla's per-player faction registry: Overthrow
	//! players never go through faction selection, so a guard against it leaves members CIV, which is
	//! friendly to everyone and never fights (BUG-146).
	//! \param[in] character The member.
	protected void StampMemberFaction(notnull IEntity character)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return;

		SCR_CharacterFactionAffiliationComponent affiliation = SCR_CharacterFactionAffiliationComponent.Cast(
			character.FindComponent(SCR_CharacterFactionAffiliationComponent)
		);

		if (!affiliation)
		{
			Print("[Overthrow] A High Command member has no character faction component - it will not fight for the resistance", LogLevel.WARNING);
			return;
		}

		affiliation.SetAffiliatedFactionByKey(config.m_sPlayerFaction);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] group The group to read.
	//! \param[out] members Every member body currently in it.
	protected void CollectMembers(notnull SCR_AIGroup group, out array<IEntity> members)
	{
		members.Clear();

		array<AIAgent> agents = {};
		group.GetAgents(agents);

		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity character = agent.GetControlledEntity();
			if (character)
				members.Insert(character);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Where a group actually IS. The group ENTITY does not follow its members, so its own origin is
	//! where it was spawned and nothing else.
	//! \param[in] group The group.
	//! \return Its leader's position, its first member's, or the group entity's.
	protected vector ResolveGroupPosition(notnull SCR_AIGroup group)
	{
		IEntity leader = group.GetLeaderEntity();
		if (leader)
			return leader.GetOrigin();

		array<IEntity> members = {};
		CollectMembers(group, members);

		if (!members.IsEmpty())
			return members[0].GetOrigin();

		return group.GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	//! Is one of this command's groups holding ground here?
	//!
	//! ⚠ NOT A DORMANCY WORKAROUND. A high command group is an AI OBSERVER
	//! (OVT_HighCommandGroupComponent installs one) and its lifecycle policy is deliberately Manual, so
	//! its men are always materialised and OVT_ResistancePresence's entity query finds them. This is a
	//! cheap early answer - a walk over a handful of records instead of a sphere over the world - and a
	//! belt to that braces if a group is ever spawned with observers switched off
	//! (GetHighCommandGroupsAreObservers).
	//! \param[in] position Centre of the search.
	//! \param[in] radius How close counts, in metres. Non-positive is never held.
	//! \return True when at least one high command group with living members is inside it.
	bool HasLivingGroupWithin(vector position, float radius)
	{
		if (radius <= 0 || !m_mEntityToGroup)
			return false;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		foreach (EntityID entityId, string groupId : m_mEntityToGroup)
		{
			SCR_AIGroup group = SCR_AIGroup.Cast(world.FindEntityByID(entityId));
			if (!group)
				continue;

			int alive = group.GetAgentsCount();
			if (group.IsDormant())
				alive = group.GetDormantAliveCount();

			if (alive < 1)
				continue;

			if (vector.Distance(ResolveGroupPosition(group), position) <= radius)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] character A member body.
	//! \return The group its agent belongs to, straight from the agent hierarchy.
	protected SCR_AIGroup FindParentGroup(notnull IEntity character)
	{
		AIControlComponent control = AIControlComponent.Cast(character.FindComponent(AIControlComponent));
		if (!control)
			return null;

		AIAgent agent = control.GetAIAgent();
		if (!agent)
			return null;

		return SCR_AIGroup.Cast(agent.GetParentGroup());
	}

	//------------------------------------------------------------------------------------------------
	//! Where a new group is anchored: the host's authored spawn point when there is one, otherwise the
	//! caller's own position (R8).
	//!
	//! HasSpawnPoints() IS THE GATE, not a null check on the component. GetSpawnPoint() falls back to
	//! the holder's own origin when nothing is authored, and for a barracks that is a point inside the
	//! building - exactly what this rider exists to stop.
	//! \param[in] spawnPoints The host's spawn points, or null.
	//! \param[in] fallback The caller's position.
	//! \return The anchor, at terrain height.
	protected vector ResolveSpawnAnchor(OVT_SpawnPointComponent spawnPoints, vector fallback)
	{
		if (spawnPoints && spawnPoints.HasSpawnPoints())
			return spawnPoints.GetSpawnPoint();

		return ClampToTerrain(fallback);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] position Any position.
	//! \return The same position at terrain height.
	protected vector ClampToTerrain(vector position)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return position;

		position[1] = world.GetSurfaceY(position[0], position[2]);
		return position;
	}

	//------------------------------------------------------------------------------------------------
	//! Mints an id no live record already holds.
	//!
	//! The salt is monotonic within a session, so the walk visits one more distinct id than there are
	//! records and cannot run out - a pigeonhole bound, not a retry budget.
	//! \param[in] ownerPersistentId The buying player.
	//! \return A fresh group id, or "" if the impossible happened.
	protected string MintGroupId(string ownerPersistentId)
	{
		int unixTime = System.GetUnixTime();
		int limit = m_mGroups.Count() + 1;

		for (int i = 0; i <= limit; i++)
		{
			m_iGroupIdSalt += 1;

			string candidate = OVT_HighCommandRules.MintGroupId(ownerPersistentId, unixTime, m_iGroupIdSalt);
			if (!m_mGroups.Contains(candidate))
				return candidate;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	// PHASE 6 - REPLICATION: JIP, the four broadcast deltas, the heartbeat and the client mirror
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Writes the whole group table for a joining client (F19).
	//!
	//! FORMAT - POSITIONAL, AND A NEW FIELD GOES LAST:
	//!   int    resolved member cap        (first, so a roster can render "n / cap" with no config
	//!                                      stream move - it is a server-only difficulty field, §3.12)
	//!   int    group count
	//!   per group, in this order: groupId, owner, entryKey, stance, destination, lastKnownPosition,
	//!   statusFlags, aliveMembers, totalMembers.
	//!
	//! THE COUNT IS TAKEN FROM A SNAPSHOT, NOT FROM m_mGroups.Count(). Writing a count and then
	//! skipping a null element mid-loop desynchronises the stream for every field after it, and the
	//! reader has no way to notice.
	//!
	//! m_aMemberBodyIds / m_aMemberPrefabs are DELIBERATELY NOT SENT: they are save-only (D8), a
	//! client can do nothing with a body UUID, and they would put an unbounded array on the wire.
	//! \param[in] writer The JIP stream.
	//! \return True - the payload is always writable.
	override bool RplSave(ScriptBitWriter writer)
	{
		writer.WriteInt(GetMemberCap());

		array<OVT_HighCommandRecord> snapshot = {};
		if (m_mGroups)
		{
			for (int i = 0; i < m_mGroups.Count(); i++)
			{
				OVT_HighCommandRecord record = m_mGroups.GetElement(i);
				if (!record || record.m_sGroupId == "")
					continue;

				snapshot.Insert(record);
			}
		}

		writer.WriteInt(snapshot.Count());

		foreach (OVT_HighCommandRecord group : snapshot)
		{
			writer.WriteString(group.m_sGroupId);
			writer.WriteString(group.m_sOwnerPersistentId);
			writer.WriteString(group.m_sEntryKey);
			writer.WriteInt(group.m_iStance);
			writer.WriteVector(group.m_vDestination);
			writer.WriteVector(group.m_vLastKnownPosition);
			writer.WriteInt(group.m_iStatusFlags);
			writer.WriteInt(group.m_iAliveMembers);

			// APPENDED LAST, and it must stay last - see the class header's append rule.
			writer.WriteInt(group.m_iTotalMembers);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the client's record mirror from the JIP stream. READ ORDER EQUALS WRITE ORDER.
	//!
	//! The mirror is records only - no entity is resolved, nothing is spawned, and every live-entity
	//! id stays EntityID.INVALID, because a client has no group entity to point at.
	//! \param[in] reader The JIP stream.
	//! \return False on the first unreadable field; the caller drops the payload.
	override bool RplLoad(ScriptBitReader reader)
	{
		int memberCap;
		if (!reader.ReadInt(memberCap)) return false;

		int groupCount;
		if (!reader.ReadInt(groupCount)) return false;

		m_iReplicatedMemberCap = memberCap;

		if (!m_mGroups)
			m_mGroups = new map<string, ref OVT_HighCommandRecord>();

		if (!m_mGroupsByOwner)
			m_mGroupsByOwner = new map<string, ref array<string>>();

		m_mGroups.Clear();
		m_mGroupsByOwner.Clear();

		for (int i = 0; i < groupCount; i++)
		{
			string groupId, ownerPersistentId, entryKey;
			int stance, statusFlags, aliveMembers, totalMembers;
			vector destination, lastKnownPosition;

			if (!reader.ReadString(groupId)) return false;
			if (!reader.ReadString(ownerPersistentId)) return false;
			if (!reader.ReadString(entryKey)) return false;
			if (!reader.ReadInt(stance)) return false;
			if (!reader.ReadVector(destination)) return false;
			if (!reader.ReadVector(lastKnownPosition)) return false;
			if (!reader.ReadInt(statusFlags)) return false;
			if (!reader.ReadInt(aliveMembers)) return false;

			// LAST in the per-group block, matching RplSave.
			if (!reader.ReadInt(totalMembers)) return false;

			OVT_HighCommandRecord record = new OVT_HighCommandRecord();
			record.m_sGroupId = groupId;
			record.m_sOwnerPersistentId = ownerPersistentId;
			record.m_sEntryKey = entryKey;
			record.m_iStance = stance;
			record.m_vDestination = destination;
			record.m_vLastKnownPosition = lastKnownPosition;
			record.m_iStatusFlags = statusFlags;
			record.m_iAliveMembers = aliveMembers;
			record.m_iTotalMembers = totalMembers;
			record.m_aMemberBodyIds = {};
			record.m_aMemberPrefabs = {};
			record.m_GroupEntityId = EntityID.INVALID;
			record.m_VehicleEntityId = EntityID.INVALID;

			IndexRecord(record);
			SeedStatusEcho(record);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: tells every client a group now exists (delta #8).
	//!
	//! THE LOCAL APPLY RUNS FIRST, then the one Rpc() beside it at a fixed literal arity with the
	//! identical argument list. A listen host is a client too and the engine never loops a broadcast
	//! back to its sender, so without the direct call the host's own listeners never fire.
	//! \param[in] record The new record.
	void BroadcastGroupCreated(notnull OVT_HighCommandRecord record)
	{
		if (!Replication.IsServer())
			return;

		ApplyGroupCreated(record.m_sGroupId, record.m_sOwnerPersistentId, record.m_sEntryKey, record.m_vLastKnownPosition, record.m_iTotalMembers);
		Rpc(RpcDo_HCCreated, record.m_sGroupId, record.m_sOwnerPersistentId, record.m_sEntryKey, record.m_vLastKnownPosition, record.m_iTotalMembers);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: tells every client a group has gone (delta #9). Applies locally first - see #8.
	//! \param[in] groupId The group that has gone.
	void BroadcastGroupRemoved(string groupId)
	{
		if (!Replication.IsServer())
			return;

		ApplyGroupRemoved(groupId);
		Rpc(RpcDo_HCRemoved, groupId);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: tells every client a group has new orders (delta #10). Applies locally first - see #8.
	//! \param[in] record The ordered record.
	void BroadcastGroupOrdered(notnull OVT_HighCommandRecord record)
	{
		if (!Replication.IsServer())
			return;

		ApplyGroupOrdered(record.m_sGroupId, record.m_iStance, record.m_vDestination);
		Rpc(RpcDo_HCOrdered, record.m_sGroupId, record.m_iStance, record.m_vDestination);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: the heartbeat (delta #11). Applies locally first - see #8.
	//!
	//! ⚠ THE ONLY SEND SITE OF RpcDo_HCStatus. It has exactly two callers: SweepStatus(), which only
	//! reaches it when HasStatusChanged() said so - that is what makes a parked group silent - and
	//! FinishRestoreIfSettled(), once per group at load, which is also what seeds that group's echo.
	//! \param[in] record The record whose status has actually changed.
	void BroadcastGroupStatus(notnull OVT_HighCommandRecord record)
	{
		if (!Replication.IsServer())
			return;

		ApplyGroupStatus(record.m_sGroupId, record.m_vLastKnownPosition, record.m_iStatusFlags, record.m_iAliveMembers);
		Rpc(RpcDo_HCStatus, record.m_sGroupId, record.m_vLastKnownPosition, record.m_iStatusFlags, record.m_iAliveMembers);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: a group appeared. Arity 5 - matches BroadcastGroupCreated exactly.
	//! \param[in] groupId The group's id.
	//! \param[in] ownerPersistentId The owning player.
	//! \param[in] entryKey The authored entry it was bought from.
	//! \param[in] position Where it is.
	//! \param[in] totalMembers How many members it has.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_HCCreated(string groupId, string ownerPersistentId, string entryKey, vector position, int totalMembers)
	{
		ApplyGroupCreated(groupId, ownerPersistentId, entryKey, position, totalMembers);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: a group has gone. Arity 1 - matches BroadcastGroupRemoved exactly.
	//! \param[in] groupId The group that has gone.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_HCRemoved(string groupId)
	{
		ApplyGroupRemoved(groupId);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: a group has new orders. Arity 3 - matches BroadcastGroupOrdered exactly.
	//! \param[in] groupId The ordered group.
	//! \param[in] stance An OVT_EHighCommandStance ordinal.
	//! \param[in] destination Where it was sent.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_HCOrdered(string groupId, int stance, vector destination)
	{
		ApplyGroupOrdered(groupId, stance, destination);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT: the heartbeat. Arity 4 - matches BroadcastGroupStatus exactly.
	//!
	//! ITS OWN RPC ON PURPOSE: the recruit equivalent it is modelled on is at the eight-parameter
	//! ceiling with nowhere to grow, and Rpc() is an untyped variadic prototype, so a wrong arity
	//! compiles clean and dies at the wire (BUG-090).
	//! \param[in] groupId The group.
	//! \param[in] position Where it is now.
	//! \param[in] statusFlags An OVT_HighCommandStatus mask.
	//! \param[in] aliveMembers How many members are still standing.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_HCStatus(string groupId, vector position, int statusFlags, int aliveMembers)
	{
		ApplyGroupStatus(groupId, position, statusFlags, aliveMembers);
	}

	//------------------------------------------------------------------------------------------------
	//! Adds or refreshes one record, on EITHER machine.
	//! \param[in] groupId The group's id.
	//! \param[in] ownerPersistentId The owning player.
	//! \param[in] entryKey The authored entry it was bought from.
	//! \param[in] position Where it is.
	//! \param[in] totalMembers How many members it has.
	protected void ApplyGroupCreated(string groupId, string ownerPersistentId, string entryKey, vector position, int totalMembers)
	{
		if (groupId == "")
			return;

		OVT_HighCommandRecord record = GetGroup(groupId);
		if (!record)
		{
			record = new OVT_HighCommandRecord();
			record.m_sGroupId = groupId;
			record.m_sOwnerPersistentId = ownerPersistentId;
			record.m_sEntryKey = entryKey;
			record.m_iStance = OVT_EHighCommandStance.DEFEND;
			record.m_vDestination = position;
			record.m_vLastKnownPosition = position;
			record.m_iStatusFlags = 0;
			record.m_iAliveMembers = totalMembers;
			record.m_iTotalMembers = totalMembers;
			record.m_aMemberBodyIds = {};
			record.m_aMemberPrefabs = {};
			record.m_GroupEntityId = EntityID.INVALID;
			record.m_VehicleEntityId = EntityID.INVALID;

			IndexRecord(record);
		}
		else
		{
			// The authority already holds this record; the delta is what makes its own listeners fire.
			record.m_sOwnerPersistentId = ownerPersistentId;
			record.m_sEntryKey = entryKey;
			record.m_iTotalMembers = totalMembers;
		}

		// The first heartbeat after a group appears must be silent for a group that has not moved.
		SeedStatusEcho(record);

		m_OnHCGroupAdded.Invoke(record);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops one record and everything keyed on it, on EITHER machine.
	//! \param[in] groupId The group that has gone.
	protected void ApplyGroupRemoved(string groupId)
	{
		OVT_HighCommandRecord record = GetGroup(groupId);
		if (!record)
			return;

		if (m_mGroupsByOwner && m_mGroupsByOwner.Contains(record.m_sOwnerPersistentId))
			m_mGroupsByOwner[record.m_sOwnerPersistentId].RemoveItem(groupId);

		m_mGroups.Remove(groupId);

		if (m_mStatusEcho)
			m_mStatusEcho.Remove(groupId);

		if (m_mRestorePlaceholders)
			m_mRestorePlaceholders.Remove(groupId);

		// The record is handed over AFTER the drop, so a listener that asks the table gets the truth.
		m_OnHCGroupRemoved.Invoke(record);
	}

	//------------------------------------------------------------------------------------------------
	//! Applies an order to one record, on EITHER machine. Records only - a client never re-points a
	//! waypoint, because a client has no group entity.
	//! \param[in] groupId The ordered group.
	//! \param[in] stance An OVT_EHighCommandStance ordinal.
	//! \param[in] destination Where it was sent.
	protected void ApplyGroupOrdered(string groupId, int stance, vector destination)
	{
		OVT_HighCommandRecord record = GetGroup(groupId);
		if (!record)
			return;

		record.m_iStance = stance;
		record.m_vDestination = destination;

		m_OnHCGroupUpdated.Invoke(record);
	}

	//------------------------------------------------------------------------------------------------
	//! Applies a heartbeat to one record, on EITHER machine, and refreshes the change filter's memory.
	//!
	//! THE ECHO IS WRITTEN HERE AND NOWHERE ELSE, so "what was last sent" can only ever be updated by
	//! something that was actually sent.
	//! \param[in] groupId The group.
	//! \param[in] position Where it is now.
	//! \param[in] statusFlags An OVT_HighCommandStatus mask.
	//! \param[in] aliveMembers How many members are still standing.
	protected void ApplyGroupStatus(string groupId, vector position, int statusFlags, int aliveMembers)
	{
		OVT_HighCommandRecord record = GetGroup(groupId);
		if (!record)
			return;

		record.m_vLastKnownPosition = position;
		record.m_iStatusFlags = statusFlags;
		record.m_iAliveMembers = aliveMembers;

		SeedStatusEcho(record);

		m_OnHCGroupUpdated.Invoke(record);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: every STATUS_SYNC_INTERVAL_MS, re-measure the live groups that are DUE and push only
	//! what changed.
	//!
	//! WHY A PUSH AND NOT A CLIENT-SIDE READ (the recruit sweep's D11, restated): an HC group is by
	//! definition usually somewhere no player is, so its entities are not streamed to those clients
	//! and a client-side read would report every distant group as unarmed and stationary.
	//!
	//! THE CADENCE IS ADAPTIVE (R4). The tick runs at the travelling cadence, and
	//! OVT_HighCommandRules.IsStatusReadDue() decides per group whether this is one of its ticks: a
	//! group still on its way is read every time, a parked one every STATUS_IDLE_SWEEP_TICKS. A parked
	//! group therefore costs exactly what it did before, and - because HasStatusChanged() still gates
	//! the send - still puts nothing on the wire.
	//!
	//! THE RECORD IS ALWAYS WRITTEN; ONLY THE WIRE IS FILTERED. The record is the save's input and the
	//! host's own map source, so it must stay current even for a group nobody needs to be told about.
	//!
	//! THE ID LIST IS SNAPSHOTTED FIRST: a broadcast can reach a listener that removes a record, and
	//! walking m_mGroups live would then be mutating it mid-iteration.
	protected void SweepStatus()
	{
		if (!Replication.IsServer())
			return;

		m_iStatusSweepTick++;
		bool idleTick = (m_iStatusSweepTick % STATUS_IDLE_SWEEP_TICKS) == 0;

		// Registry maintenance stays on the parked cadence - it is not what R4 made faster.
		if (idleTick)
			PruneMemberRegistry();

		if (!m_mGroups || m_mGroups.IsEmpty())
			return;

		array<string> groupIds = {};
		for (int i = 0; i < m_mGroups.Count(); i++)
		{
			groupIds.Insert(m_mGroups.GetKey(i));
		}

		foreach (string groupId : groupIds)
		{
			OVT_HighCommandRecord record = GetGroup(groupId);
			if (!record)
				continue;

			// Measured from the LAST measured position, so a group ordered somewhere reads as moving
			// on the very next tick and one that just arrived drops back to the cheap cadence.
			if (!OVT_HighCommandRules.IsStatusReadDue(record.m_vLastKnownPosition, record.m_vDestination, m_iStatusSweepTick, STATUS_IDLE_SWEEP_TICKS))
				continue;

			// No entity yet (a save record still queued for its rebuild) or no entity any more: the
			// last known position is already the best answer anyone has.
			SCR_AIGroup group = GetGroupEntity(record);
			if (!group)
				continue;

			vector position = ResolveGroupPosition(group);
			int aliveMembers;
			int statusFlags = ReadGroupStatus(group, position, record.m_vDestination, aliveMembers);

			record.m_vLastKnownPosition = position;
			record.m_iStatusFlags = statusFlags;
			record.m_iAliveMembers = aliveMembers;

			if (!HasStatusChanged(groupId, position, statusFlags, aliveMembers))
				continue;

			BroadcastGroupStatus(record);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Whether one group's heartbeat is worth a packet.
	//!
	//! A GROUP WITH NO ECHO IS ALWAYS WORTH ONE - that is a group nobody has been told about yet.
	//! \param[in] groupId The group.
	//! \param[in] position Its measured position.
	//! \param[in] statusFlags Its measured status mask.
	//! \param[in] aliveMembers Its measured member count.
	//! \return True when the flags or the member count differ, or the position moved further than
	//! STATUS_POSITION_THRESHOLD. Squared distance throughout - vector.Distance is not correctly
	//! rounded, so an exact-boundary decision would be a coin flip.
	protected bool HasStatusChanged(string groupId, vector position, int statusFlags, int aliveMembers)
	{
		if (!m_mStatusEcho || !m_mStatusEcho.Contains(groupId))
			return true;

		OVT_HighCommandStatusEcho echo = m_mStatusEcho[groupId];
		if (!echo)
			return true;

		if (echo.m_iStatusFlags != statusFlags)
			return true;

		if (echo.m_iAliveMembers != aliveMembers)
			return true;

		float thresholdSq = STATUS_POSITION_THRESHOLD * STATUS_POSITION_THRESHOLD;
		return vector.DistanceSq(echo.m_vPosition, position) > thresholdSq;
	}

	//------------------------------------------------------------------------------------------------
	//! Remembers what a record's status looked like at the moment it was sent.
	//! \param[in] record The record that was just sent.
	protected void SeedStatusEcho(notnull OVT_HighCommandRecord record)
	{
		if (!m_mStatusEcho)
			m_mStatusEcho = new map<string, ref OVT_HighCommandStatusEcho>();

		OVT_HighCommandStatusEcho echo = new OVT_HighCommandStatusEcho();
		echo.m_vPosition = record.m_vLastKnownPosition;
		echo.m_iStatusFlags = record.m_iStatusFlags;
		echo.m_iAliveMembers = record.m_iAliveMembers;

		m_mStatusEcho.Set(record.m_sGroupId, echo);
	}

	//------------------------------------------------------------------------------------------------
	//! Reads one live group's fighting condition into the mask its owner and every map is shown.
	//!
	//! READS ONLY. Nothing here writes to an entity, a record or the world; the four measured facts
	//! are handed to OVT_HighCommandStatus.Derive(), which owns the packing and is Logic-tier pinned.
	//!
	//! The walk STOPS EARLY once all three per-member facts are true, because nothing after that can
	//! change the answer - at the 48-member cap the read, not the send, is what costs.
	//! \param[in] group The live group.
	//! \param[in] position Its current position.
	//! \param[in] destination Its ordered destination.
	//! \param[out] aliveMembers How many members it still holds.
	//! \return An OVT_HighCommandStatus mask.
	protected int ReadGroupStatus(notnull SCR_AIGroup group, vector position, vector destination, out int aliveMembers)
	{
		array<IEntity> members = {};
		CollectMembers(group, members);

		aliveMembers = members.Count();

		bool contact = false;
		bool anyAmmo = false;
		bool mounted = false;

		foreach (IEntity member : members)
		{
			if (!contact && HasContact(member))
				contact = true;

			if (!anyAmmo && CanFireOrReload(member))
				anyAmmo = true;

			if (!mounted && IsMounted(member))
				mounted = true;

			if (contact && anyAmmo && mounted)
				break;
		}

		return OVT_HighCommandStatus.Derive(contact, anyAmmo, OVT_HighCommandRules.IsMoving(position, destination), mounted);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] member A member body.
	//! \return True when this member is currently engaging something.
	protected bool HasContact(notnull IEntity member)
	{
		SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(member.FindComponent(SCR_AICombatComponent));
		if (!combat)
			return false;

		return combat.GetCurrentTarget() != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether one member can put rounds downrange now or after a reload - ReadRecruitStatus's shape.
	//!
	//! WEAPONS ARE READ FROM EVERY SLOT, NOT FROM THE HANDS: a slung rifle lives in the weapon
	//! manager's slots and is invisible to an in-hands read (BUG-044). "Has ammo" means a muzzle
	//! reports rounds (a chambered one counts) OR the inventory holds a magazine that fits one of the
	//! carried weapons - the engine's own GetMagazineCountByWeapon, so magazine-well compatibility is
	//! its answer and not a hand-rolled one.
	//! \param[in] member A member body.
	//! \return True when this member is not out of ammo.
	protected bool CanFireOrReload(notnull IEntity member)
	{
		BaseWeaponManagerComponent weaponManager = BaseWeaponManagerComponent.Cast(member.FindComponent(BaseWeaponManagerComponent));
		if (!weaponManager)
			return false;

		InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(member.FindComponent(InventoryStorageManagerComponent));

		array<WeaponSlotComponent> weaponSlots = {};
		weaponManager.GetWeaponsSlots(weaponSlots);

		foreach (WeaponSlotComponent slot : weaponSlots)
		{
			if (!slot)
				continue;

			IEntity weaponEntity = slot.GetWeaponEntity();
			if (!weaponEntity)
				continue;

			BaseWeaponComponent weapon = BaseWeaponComponent.Cast(weaponEntity.FindComponent(BaseWeaponComponent));
			if (!weapon)
				continue;

			BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
			if (muzzle && muzzle.GetAmmoCount() > 0)
				return true;

			if (inventory && inventory.GetMagazineCountByWeapon(weapon) > 0)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] member A member body.
	//! \return True when this member is sitting in a vehicle.
	protected bool IsMounted(notnull IEntity member)
	{
		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(member.FindComponent(SCR_CompartmentAccessComponent));
		if (!access)
			return false;

		return access.IsInCompartment();
	}

	//------------------------------------------------------------------------------------------------
	// T6.7 - THE MEMBER BODY REGISTRY: what keeps an HC member's body tracked, and therefore D8 alive
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! SERVER: claims one body as a High Command member's, BEFORE it is added to the group.
	//!
	//! ⚠ THE ORDERING IS THE MECHANISM, not a preference. Scripts/Game/Modded/SCR_AIGroup.c untracks
	//! every entity that reaches any group - unconditionally in AddAIEntityToGroup, and through the
	//! catch-all OnAgentAdded for anything that got there another way - excluding only player bodies
	//! and bodies a manager has already claimed. An untracked body has no persistence id, and the id
	//! IS D8. This mirrors the recruit precedent exactly; there is no second mechanism.
	//! \param[in] body The member body.
	//! \param[in] groupId The group it belongs to.
	void RegisterMemberBody(notnull IEntity body, string groupId)
	{
		if (!Replication.IsServer())
			return;

		if (groupId == "")
			return;

		if (!m_mMemberToGroup)
			m_mMemberToGroup = new map<EntityID, string>();

		m_mMemberToGroup.Set(body.GetID(), groupId);

		// A body that reached here through a path that already queued a deferred release - a scratch
		// character, a town civilian - is being promoted into a category that must STAY tracked.
		OVT_PersistenceManagerComponent.CancelUntrackTransient(body);

		// CancelUntrackTransient only withdraws a release that is still QUEUED. A parked recruit's body
		// has already been through AddAIEntityToGroup once and may have lost its record there, and an
		// untracked body has no persistence id at all - which is D8. AddRecruit pairs the same two
		// calls at the same kind of promotion boundary (BUG-131).
		if (!OVT_PersistenceTracking.IsTracked(body))
			OVT_PersistenceTracking.Track(body);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: releases a body's claim. Safe for a body that was never claimed.
	//! \param[in] bodyId The body's entity id.
	void UnregisterMemberBody(EntityID bodyId)
	{
		if (!m_mMemberToGroup)
			return;

		m_mMemberToGroup.Remove(bodyId);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a body is a claimed High Command member. Called for EVERY agent that joins ANY group in
	//! the world, so it is a single map lookup and nothing else.
	//! \param[in] body The body to ask about.
	//! \return True when this body belongs to a High Command group.
	bool IsMemberBody(IEntity body)
	{
		if (!body || !m_mMemberToGroup)
			return false;

		return m_mMemberToGroup.Contains(body.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! Drops claims whose body has left the world - a man killed in a firefight, a stand-in retired by
	//! the load walk. Without it the registry grows for the length of a campaign and, worse, an entity
	//! id the engine hands out again would be treated as a member of a group it never joined.
	//!
	//! Collected then removed: removing inside the iteration over the same map invalidates it.
	protected void PruneMemberRegistry()
	{
		if (!m_mMemberToGroup || m_mMemberToGroup.IsEmpty())
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		array<EntityID> stale = {};

		foreach (EntityID bodyId, string groupId : m_mMemberToGroup)
		{
			if (!world.FindEntityByID(bodyId))
				stale.Insert(bodyId);
		}

		foreach (EntityID staleId : stale)
		{
			m_mMemberToGroup.Remove(staleId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] body A member body.
	//! \return The group id it belongs to, or "" when it belongs to none.
	string GetGroupIdFromMemberEntity(IEntity body)
	{
		if (!body || !m_mMemberToGroup)
			return "";

		EntityID bodyId = body.GetID();
		if (!m_mMemberToGroup.Contains(bodyId))
			return "";

		return m_mMemberToGroup[bodyId];
	}

	//------------------------------------------------------------------------------------------------
	//! Writes one group's live roster - a persistence id and a prefab per member - onto its record.
	//!
	//! THE TWO ARRAYS ARE INDEX-ALIGNED and that is what makes the load walk work: body id i is the
	//! stored character for the member whose fallback prefab is prefabs[i]. A member with no id yet
	//! contributes an empty string rather than being skipped, so the alignment survives.
	//! \param[in] record The record to write.
	//! \param[in] group Its live group.
	protected void CaptureMemberRoster(notnull OVT_HighCommandRecord record, notnull SCR_AIGroup group)
	{
		array<IEntity> members = {};
		CollectMembers(group, members);

		record.m_aMemberBodyIds = {};
		record.m_aMemberPrefabs = {};

		foreach (IEntity member : members)
		{
			string bodyId = OVT_PersistenceTracking.GetPersistentId(member);
			if (bodyId == "")
			{
				// Registration is lazy, so a body the system has never written has no identity yet.
				// Writing its record is what gives it one.
				OVT_PersistenceTracking.Save(member);
				bodyId = OVT_PersistenceTracking.GetPersistentId(member);
			}

			record.m_aMemberBodyIds.Insert(bodyId);

			EntityPrefabData prefabData = member.GetPrefabData();
			if (prefabData)
				record.m_aMemberPrefabs.Insert(prefabData.GetPrefabName());
			else
				record.m_aMemberPrefabs.Insert(ResourceName.Empty);
		}

		record.m_iTotalMembers = members.Count();
		record.m_iAliveMembers = members.Count();
	}

	//------------------------------------------------------------------------------------------------
	// PHASE 9 - THE RECRUIT CONVERSION SEAM (§4 T9.1-T9.3)
	//
	// ONE WAY. Nothing here, and nothing anywhere else, turns a High Command group back into recruits.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! SERVER: the inactive-recruit group one recruit's body is standing in.
	//!
	//! An ACTIVE recruit stands in its owner's slave group, which carries no marker, so this answers
	//! null for it - which is what makes "only parked squads convert" true without a second flag read.
	//! \param[in] recruitId The anchor recruit.
	//! \return Its host group, or null when it has no body, no group, or a group that is not ours.
	SCR_AIGroup FindInactiveRecruitGroup(string recruitId)
	{
		if (!Replication.IsServer())
			return null;

		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		if (!recruits)
			return null;

		IEntity body = recruits.FindRecruitEntity(recruitId);
		if (!body)
			return null;

		SCR_AIGroup group = FindParentGroup(body);
		if (!group)
			return null;

		if (!OVT_InactiveRecruitGroupComponent.Cast(group.FindComponent(OVT_InactiveRecruitGroupComponent)))
			return null;

		return group;
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: every one of an owner's INACTIVE recruits whose body stands in this group.
	//!
	//! Driven from the owner's own RECORDS, not from the group's agents: every body collected here is
	//! then guaranteed to have a record this owner may drop, so nothing in the world can be converted
	//! that the caller does not own.
	//!
	//! The two arrays are INDEX-ALIGNED - recruitIds[i] is the record for bodies[i].
	//! \param[in] ownerPersistentId The owning player.
	//! \param[in] hostGroup The inactive group being converted.
	//! \param[out] recruitIds The records that would be dropped.
	//! \param[out] bodies Their bodies, in the same order.
	//! \return How many were found.
	int CollectInactiveGroupRecruits(string ownerPersistentId, notnull SCR_AIGroup hostGroup, out array<string> recruitIds, out array<IEntity> bodies)
	{
		recruitIds.Clear();
		bodies.Clear();

		if (!Replication.IsServer())
			return 0;

		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		if (!recruits)
			return 0;

		// A FRESH array, which is also what makes the FindRecruitEntity() calls below safe: that method
		// prunes stale entries from the manager's own entity map as it walks.
		array<ref OVT_RecruitData> parked = recruits.GetPlayerRecruitsByState(ownerPersistentId, true);

		foreach (OVT_RecruitData recruit : parked)
		{
			if (!recruit)
				continue;

			IEntity body = recruits.FindRecruitEntity(recruit.m_sRecruitId);
			if (!body)
				continue;

			if (FindParentGroup(body) != hostGroup)
				continue;

			recruitIds.Insert(recruit.m_sRecruitId);
			bodies.Insert(body);
		}

		return recruitIds.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: turns one parked recruit squad into a High Command group, one way (T9.2).
	//!
	//! A NEW GROUP ENTITY, NOT A STAMPED ONE. A scripted component cannot be added to a live entity
	//! from script at all (WorldEditorAPI.CreateComponent is Workbench-only), and even if it could, the
	//! old entity would then carry two components that each install and remove an AI observer for it
	//! and each dispose of "their" waypoints. The bodies move instead; the emptied recruit group is
	//! deleted by vanilla's delete-when-empty a frame later, and OVT_InactiveRecruitGroupComponent's
	//! OnDelete takes its hold waypoints and its observer with it - neither of which the High Command
	//! component ever owned.
	//!
	//! ORDER IS LOAD-BEARING: the group is built and the bodies are moved FIRST, and only the recruits
	//! that actually arrived have their records dropped. A body that could not be moved keeps its
	//! record, so nothing in the world is ever left owned by nobody.
	//! \param[in] ownerPersistentId The owning player.
	//! \param[in] recruitIds The records to drop, index-aligned with bodies.
	//! \param[in] bodies The bodies to move.
	//! \return The new group's record, or null when nothing was converted.
	OVT_HighCommandRecord ConvertRecruitGroup(string ownerPersistentId, notnull array<string> recruitIds, notnull array<IEntity> bodies)
	{
		if (!Replication.IsServer())
			return null;

		if (ownerPersistentId == "")
			return null;

		if (bodies.IsEmpty() || bodies.Count() != recruitIds.Count())
		{
			Print("[Overthrow] A High Command conversion was asked for with " + bodies.Count().ToString() + " body(ies) against " + recruitIds.Count().ToString() + " record(s) - refused", LogLevel.ERROR);
			return null;
		}

		if (!bodies[0])
			return null;

		OVT_HighCommandRecord record = SpawnGroup(OVT_HighCommandRules.CONVERTED_ENTRY_KEY, ResourceName.Empty, ResourceName.Empty, bodies[0].GetOrigin(), ownerPersistentId, null, bodies);
		if (!record)
			return null;

		DropConvertedRecruitRecords(record.m_sGroupId, recruitIds, bodies);

		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the recruit records of the bodies that reached a converted group.
	//!
	//! ⚠ THE BODY ID IS CLEARED BEFORE THE RECORD IS DROPPED, and that ordering is the point: the body
	//! belongs to a High Command group now, and a dropped record that still named it could ask the
	//! persistence system to spawn that same character back a second time. The precedent is the recruit
	//! manager's own death path, which clears the id for exactly this reason before removing.
	//!
	//! THE REMOVAL IS THE SHIPPED ONE (RemoveRecruit) - it broadcasts, scrubs the entity and
	//! replication maps, and frees the recruit cap. This seam adds no second removal path (DoD I8).
	//! \param[in] groupId The group the bodies were supposed to join.
	//! \param[in] recruitIds The candidate records, index-aligned with bodies.
	//! \param[in] bodies Their bodies.
	protected void DropConvertedRecruitRecords(string groupId, notnull array<string> recruitIds, notnull array<IEntity> bodies)
	{
		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		if (!recruits)
		{
			Print("[Overthrow] High Command group " + groupId + " was converted but the recruit manager has gone - its members still hold recruit records", LogLevel.ERROR);
			return;
		}

		for (int i = 0; i < recruitIds.Count(); i++)
		{
			IEntity body = bodies[i];
			if (!body)
				continue;

			// The honest post-condition: AdoptMembers unregisters a body it could not move, so this is
			// false for exactly the bodies that are still somewhere else.
			if (GetGroupIdFromMemberEntity(body) != groupId)
			{
				Print("[Overthrow] Recruit " + recruitIds[i] + " did not reach High Command group " + groupId + " - it keeps its recruit record", LogLevel.WARNING);
				continue;
			}

			OVT_RecruitData recruit = recruits.GetRecruit(recruitIds[i]);
			if (!recruit)
				continue;

			recruit.m_sBodyPersistenceId = "";

			recruits.RemoveRecruit(recruitIds[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	// T6.6 - THE SPAWN-ON-LOAD WALK (§3.11)
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! SERVER: queues every record a save left without a live group, one per call-queue hop.
	protected void QueueRestoreWalk()
	{
		if (!m_aRestoreQueue)
			m_aRestoreQueue = new array<string>();

		m_aRestoreQueue.Clear();

		if (!m_mGroups)
			return;

		for (int i = 0; i < m_mGroups.Count(); i++)
		{
			OVT_HighCommandRecord record = m_mGroups.GetElement(i);
			if (!record)
				continue;

			// A record with a live entity was created this session and needs nothing.
			if (GetGroupEntity(record))
				continue;

			m_aRestoreQueue.Insert(m_mGroups.GetKey(i));
		}

		if (m_aRestoreQueue.IsEmpty())
			return;

		Print("[Overthrow] Rebuilding " + m_aRestoreQueue.Count().ToString() + " High Command group(s) from the save point");

		GetGame().GetCallqueue().CallLater(RestoreNextGroup, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds ONE saved group, then queues the next. One per hop: each rebuild spawns a whole squad.
	protected void RestoreNextGroup()
	{
		if (!m_aRestoreQueue || m_aRestoreQueue.IsEmpty())
			return;

		string groupId = m_aRestoreQueue[0];
		m_aRestoreQueue.RemoveOrdered(0);

		RestoreGroup(groupId);

		if (!m_aRestoreQueue.IsEmpty())
			GetGame().GetCallqueue().CallLater(RestoreNextGroup, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Puts one saved group back in the world at full strength, then asks for its stored bodies.
	//!
	//! THE PREFAB FALLBACK IS PRE-EMPTIVE, NOT REACTIVE. §3.11 describes falling back to
	//! memberPrefabs[i] when a stored body times out; doing that literally would mean an EMPTY group
	//! waiting on an asynchronous answer, and vanilla's delete-when-empty deletes an emptied group one
	//! frame later. So the saved roster's prefabs are spawned FIRST - the group is never empty and is
	//! never one man short of what the player owned - and each stored body that does come back joins
	//! and retires one stand-in. A timeout is then simply the stand-in keeping the slot.
	//!
	//! A record that cannot be read is DROPPED WITH AN ERROR NAMING IT rather than silently skipped:
	//! its owner would otherwise carry a group against their cap that nobody could ever see.
	//! \param[in] groupId The saved group to rebuild.
	protected void RestoreGroup(string groupId)
	{
		OVT_HighCommandRecord record = GetGroup(groupId);
		if (!record)
			return;

		if (record.m_sEntryKey == "" || !GetEntryByKey(record.m_sEntryKey))
		{
			Print("[Overthrow] High Command group '" + groupId + "' was saved against the entry '" + record.m_sEntryKey + "', which this campaign's faction no longer publishes - the group is dropped", LogLevel.ERROR);
			RemoveRecord(groupId);
			return;
		}

		vector position = record.m_vLastKnownPosition;
		if (position == vector.Zero)
			position = record.m_vDestination;

		if (position == vector.Zero)
		{
			Print("[Overthrow] High Command group '" + groupId + "' was saved with neither a position nor a destination - the group is dropped", LogLevel.ERROR);
			RemoveRecord(groupId);
			return;
		}

		if (!SpawnGroupFromEntry(record.m_sEntryKey, position, record.m_sOwnerPersistentId, record))
		{
			Print("[Overthrow] High Command group '" + groupId + "' could not be rebuilt from the save point - the group is dropped", LogLevel.ERROR);
			RemoveRecord(groupId);
			return;
		}

		RequestRestoredBodies(record);

		// A no-op while anything is in flight. It is here because RequestRestoredBodies can make NO
		// requests at all - a record with no stored ids, a persistence system that is not active, a
		// missing collection - and the stand-in list would then never be released.
		FinishRestoreIfSettled(record.m_sGroupId);
	}

	//------------------------------------------------------------------------------------------------
	//! Notes every stand-in a restored group was rebuilt with, in spawn order.
	//! \param[in] groupId The restored group.
	//! \param[in] members The bodies that were just spawned and joined.
	protected void RememberRestorePlaceholders(string groupId, notnull array<IEntity> members)
	{
		if (!m_mRestorePlaceholders)
			m_mRestorePlaceholders = new map<string, ref array<EntityID>>();

		array<EntityID> placeholders = new array<EntityID>();
		foreach (IEntity member : members)
		{
			placeholders.Insert(member.GetID());
		}

		m_mRestorePlaceholders.Set(groupId, placeholders);
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the persistence system for every stored body this record names.
	//!
	//! One request per saved id, filtered to that id, so each callback is invoked once. A stored id
	//! that is not a UUID can never resolve - a null UUID still stringifies to a zero-filled value, so
	//! never compare, always ask (the recruit path's rule).
	//! \param[in] record The restored record.
	protected void RequestRestoredBodies(notnull OVT_HighCommandRecord record)
	{
		if (!record.m_aMemberBodyIds || record.m_aMemberBodyIds.IsEmpty())
			return;

		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetScriptedInstance();
		if (!persistence)
			return;

		// Asking a system that is still loading would answer UNAVAILABLE (SCR_SpawnLogic's guard).
		if (persistence.GetState() != EPersistenceSystemState.ACTIVE)
			return;

		PersistenceCollection collection = GetMemberBodyCollection(persistence);
		if (!collection)
			return;

		if (!m_aPendingBodySpawns)
			m_aPendingBodySpawns = new array<string>();

		for (int i = 0; i < record.m_aMemberBodyIds.Count(); i++)
		{
			string storedId = record.m_aMemberBodyIds[i];
			if (!UUID.IsUUID(storedId))
				continue;

			string token = record.m_sGroupId + "|" + i.ToString();
			if (m_aPendingBodySpawns.Find(token) != -1)
				continue;

			// MUST be pending BEFORE the request: an instance the system already holds completes the
			// callback from INSIDE RequestSpawn, and the callback's first act is to consume this.
			m_aPendingBodySpawns.Insert(token);

			UUID bodyId = storedId;

			PersistenceSpawnRequest request();
			request.Collection = collection;
			request.Include = {bodyId};

			Tuple2<string, string> spawnContext(record.m_sGroupId, token);
			PersistenceResultCallback callback(OnMemberBodySpawned, spawnContext);
			persistence.RequestSpawn(request, callback);

			// Insurance, and a no-op once the callback has answered.
			GetGame().GetCallqueue().CallLater(OnMemberBodySpawnTimeout, BODY_SPAWN_TIMEOUT_MS, false, record.m_sGroupId, token);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! PersistenceResultCallback delegate for RequestRestoredBodies(). Arity must match
	//! PersistenceResultDelegate exactly (the model is SCR_SpawnLogic.c:378).
	//! \param[in] statusCode OK when the stored body was found and instantiated.
	//! \param[in] result The spawned instance on OK; on failure the id that could not be fetched.
	//! \param[in] isLast True on the final result of the request (always, for a single-id request).
	//! \param[in] context Tuple2 of group id and pending token.
	protected void OnMemberBodySpawned(EPersistenceStatusCode statusCode, Managed result, bool isLast, Managed context)
	{
		Tuple2<string, string> spawnContext = Tuple2<string, string>.Cast(context);
		if (!spawnContext)
			return;

		string groupId = spawnContext.param1;
		string token = spawnContext.param2;

		if (!m_aPendingBodySpawns)
			return;

		// Only the first answer for a token is acted on: a duplicate result, or a late answer to a
		// request the timeout already gave up on, must not build a second body.
		int pendingIndex = m_aPendingBodySpawns.Find(token);
		if (pendingIndex == -1)
			return;

		m_aPendingBodySpawns.Remove(pendingIndex);

		IEntity body;
		if (statusCode == EPersistenceStatusCode.OK)
			body = IEntity.Cast(result);

		if (!body)
		{
			// The stand-in spawned with the group already holds this slot.
			FinishRestoreIfSettled(groupId);
			return;
		}

		OVT_HighCommandRecord record = GetGroup(groupId);
		SCR_AIGroup group;
		if (record)
			group = GetGroupEntity(record);

		if (!group || !AdoptRestoredBody(record, group, body))
		{
			// Nobody owns this character: the group was dismissed or wiped while it was in flight, or
			// it could not be joined. Drop its stored data too, or it is spawnable again forever.
			OVT_PersistenceTracking.Untrack(body, false);
			SCR_EntityHelper.DeleteEntityAndChildren(body);
		}

		FinishRestoreIfSettled(groupId);
	}

	//------------------------------------------------------------------------------------------------
	//! Gives up on a stored body request that never answered. No-op in the normal case.
	//! \param[in] groupId The group the request was made for.
	//! \param[in] token The pending token.
	protected void OnMemberBodySpawnTimeout(string groupId, string token)
	{
		if (!m_aPendingBodySpawns)
			return;

		int pendingIndex = m_aPendingBodySpawns.Find(token);
		if (pendingIndex == -1)
			return;

		m_aPendingBodySpawns.Remove(pendingIndex);

		Print("[Overthrow] No answer to High Command group " + groupId + "'s stored body request - the stand-in spawned from its saved prefab keeps the slot", LogLevel.WARNING);

		FinishRestoreIfSettled(groupId);
	}

	//------------------------------------------------------------------------------------------------
	//! Joins a stored body to its restored group and retires one stand-in for it.
	//!
	//! ⚠ JOIN FIRST, RETIRE SECOND. The count goes n -> n+1 -> n and never touches zero; the other
	//! order would empty a one-man group for a frame and vanilla's delete-when-empty would delete it.
	//! \param[in] record The restored record.
	//! \param[in] group Its live group.
	//! \param[in] body The body that came back.
	//! \return True when the body is now a member.
	protected bool AdoptRestoredBody(notnull OVT_HighCommandRecord record, notnull SCR_AIGroup group, notnull IEntity body)
	{
		RegisterMemberBody(body, record.m_sGroupId);

		if (!group.AddAIEntityToGroup(body) || FindParentGroup(body) != group)
		{
			UnregisterMemberBody(body.GetID());
			return false;
		}

		StampMemberFaction(body);
		RetireOneRestorePlaceholder(record.m_sGroupId);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes one of a restored group's stand-ins, now that a real body has taken its place.
	//! \param[in] groupId The restored group.
	protected void RetireOneRestorePlaceholder(string groupId)
	{
		if (!m_mRestorePlaceholders || !m_mRestorePlaceholders.Contains(groupId))
			return;

		array<EntityID> placeholders = m_mRestorePlaceholders[groupId];
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		while (!placeholders.IsEmpty())
		{
			EntityID placeholderId = placeholders[0];
			placeholders.RemoveOrdered(0);

			IEntity placeholder = world.FindEntityByID(placeholderId);
			if (!placeholder)
				continue;

			UnregisterMemberBody(placeholderId);
			SCR_EntityHelper.DeleteEntityAndChildren(placeholder);
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Closes out a restored group once nothing is still in flight for it.
	//! \param[in] groupId The restored group.
	protected void FinishRestoreIfSettled(string groupId)
	{
		if (HasPendingBodyRequests(groupId))
			return;

		if (m_mRestorePlaceholders)
			m_mRestorePlaceholders.Remove(groupId);

		OVT_HighCommandRecord record = GetGroup(groupId);
		if (!record)
			return;

		SCR_AIGroup group = GetGroupEntity(record);
		if (!group)
			return;

		record.m_iAliveMembers = group.GetAgentsCount();
		record.m_iTotalMembers = record.m_iAliveMembers;

		// A vehicle group re-seats: SeatCrew skips anybody already in a compartment and only takes
		// seats that read free, so this only picks up the bodies that replaced a seated stand-in.
		if (GetVehicleEntity(record))
			GetGame().GetCallqueue().CallLater(SeatCrew, 0, false, groupId);

		BroadcastGroupStatus(record);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] groupId The group to ask about.
	//! \return True when any stored body request for it is still in flight.
	protected bool HasPendingBodyRequests(string groupId)
	{
		if (!m_aPendingBodySpawns || m_aPendingBodySpawns.IsEmpty())
			return false;

		string prefix = groupId + "|";

		foreach (string token : m_aPendingBodySpawns)
		{
			if (token.IndexOf(prefix) == 0)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The persistence collection member bodies are stored in, resolved once and kept (the
	//! SCR_SpawnLogic.SetupPersistenceCollections shape - the lookup does not change during a session).
	//! \param[in] persistence The live persistence system.
	//! \return The collection, or null when the loaded configuration does not contain it.
	protected PersistenceCollection GetMemberBodyCollection(notnull SCR_PersistenceSystem persistence)
	{
		if (!m_MemberBodyCollection)
		{
			m_MemberBodyCollection = persistence.FindCollection(MEMBER_BODY_COLLECTION);

			if (!m_MemberBodyCollection)
				Print("[Overthrow] No '" + MEMBER_BODY_COLLECTION + "' persistence collection - High Command members cannot be spawned back with their gear", LogLevel.WARNING);
		}

		return m_MemberBodyCollection;
	}

	//------------------------------------------------------------------------------------------------
	//! A restored group's composition: the roster it was SAVED with, not the catalog entry's.
	//! \param[in] restoring The record being rebuilt, or null for a fresh purchase.
	//! \param[out] memberPrefabs Receives the saved roster.
	//! \param[out] memberOffsets Receives a synthesised ring, one per member.
	//! \return True when a saved roster was used; false to fall through to the authored composition.
	protected bool ReadRestoredComposition(OVT_HighCommandRecord restoring, out array<ResourceName> memberPrefabs, out array<vector> memberOffsets)
	{
		if (!restoring || !restoring.m_aMemberPrefabs || restoring.m_aMemberPrefabs.IsEmpty())
			return false;

		memberPrefabs.Clear();
		memberOffsets.Clear();

		foreach (ResourceName saved : restoring.m_aMemberPrefabs)
		{
			memberPrefabs.Insert(saved);
		}

		FillMemberOffsets(memberPrefabs, memberOffsets);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Tops a member-offset list up to one entry per member, on the fallback ring.
	//! \param[in] memberPrefabs The composition.
	//! \param[out] memberOffsets The offsets to extend.
	protected void FillMemberOffsets(notnull array<ResourceName> memberPrefabs, out array<vector> memberOffsets)
	{
		for (int i = memberOffsets.Count(); i < memberPrefabs.Count(); i++)
		{
			float angle = i * MEMBER_FALLBACK_SPACING_DEGREES;
			memberOffsets.Insert(Vector(0, angle, 0).AnglesToVector() * MEMBER_FALLBACK_RADIUS);
		}
	}

	//------------------------------------------------------------------------------------------------
	// PHASE 10 - SUPPLY: rearm from warehouse stock, refuel from any covering fuel source
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! SERVER: tops up every live group's deficient magazines from registered warehouses in range, on
	//! m_iRearmIntervalMs.
	//!
	//! THE ID LIST IS SNAPSHOTTED FIRST, the SweepStatus rule: rearming a group can indirectly reach
	//! code that removes a record (none does today, but the shape must not assume otherwise) while
	//! m_mGroups is being walked.
	protected void RearmTick()
	{
		if (!Replication.IsServer())
			return;

		if (!m_mGroups || m_mGroups.IsEmpty())
			return;

		array<string> groupIds = {};
		for (int i = 0; i < m_mGroups.Count(); i++)
		{
			groupIds.Insert(m_mGroups.GetKey(i));
		}

		foreach (string groupId : groupIds)
		{
			OVT_HighCommandRecord record = GetGroup(groupId);
			if (!record)
				continue;

			SCR_AIGroup group = GetGroupEntity(record);
			if (!group)
				continue;

			if (group.GetAgentsCount() <= 0)
				continue;

			RearmGroup(record, group);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: rearms one group's deficient magazines from registered warehouses in range.
	//!
	//! ELIGIBILITY - which magazines are deficient - is a plain read, the OVT_VehicleRearmUtils.
	//! AnyAmmoMissing test applied per character instead of per vehicle. MUTATION (TakeUpTo,
	//! SetAmmoCount) is gated by Replication.IsServer() at the top, like every other write in this
	//! class.
	//!
	//! THE NO_AMMO BADGE CLEARS ON THE NEXT STATUS READ, NOT HERE. SweepStatus re-measures every
	//! live group's ammo from scratch and only sends when HasStatusChanged() says so; writing the
	//! flag here as well would be a second, redundant source of truth for the same bit and would make
	//! BroadcastGroupStatus's documented "exactly two callers" invariant a third. A parked group is
	//! re-measured at worst every STATUS_IDLE_SWEEP_TICKS x STATUS_SYNC_INTERVAL_MS (10 s), which is
	//! well inside any sane m_iRearmIntervalMs.
	//! \param[in] record The group's record - its owner and last known position.
	//! \param[in] group The live group entity.
	protected void RearmGroup(notnull OVT_HighCommandRecord record, notnull SCR_AIGroup group)
	{
		if (!Replication.IsServer())
			return;

		array<IEntity> members = {};
		CollectMembers(group, members);

		array<BaseMagazineComponent> deficient = {};
		array<string> resourcesNeeded = {};
		CollectDeficientMagazines(members, deficient, resourcesNeeded);

		if (deficient.IsEmpty())
			return;

		map<string, int> needByResource = new map<string, int>();
		OVT_HighCommandRules.AggregateResourceNeeds(resourcesNeeded, needByResource);

		array<OVT_StorageComponent> stores = {};
		OVT_WarehouseStockUtils.CollectStores(record.m_vLastKnownPosition, OVT_HighCommandRules.WAREHOUSE_RANGE, record.m_sOwnerPersistentId, stores);

		if (stores.IsEmpty())
			return;

		map<string, int> budgetByResource = new map<string, int>();
		foreach (string resource, int neededCount : needByResource)
		{
			int taken = OVT_WarehouseStockUtils.TakeUpTo(stores, resource, neededCount);
			if (taken > 0)
				budgetByResource.Set(resource, taken);
		}

		if (budgetByResource.IsEmpty())
			return;

		// Delivered in the order the deficiency was found, spending each resource's budget one
		// magazine at a time - nobody gets more units than the warehouse actually gave up.
		foreach (int i, BaseMagazineComponent magazine : deficient)
		{
			string resource = resourcesNeeded[i];
			if (!budgetByResource.Contains(resource))
				continue;

			int remaining = budgetByResource.Get(resource);
			if (remaining <= 0)
				continue;

			magazine.SetAmmoCount(magazine.GetMaxAmmoCount());
			budgetByResource.Set(resource, remaining - 1);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! READ ONLY: every currently-loaded magazine below its own max ammo, across every member's
	//! current muzzle - the OVT_VehicleRearmUtils.AnyAmmoMissing test, per character instead of per
	//! vehicle. A muzzle with no magazine loaded at all is not counted: like
	//! OVT_VehicleRearmUtils.PerformRearm, this tops up magazines that exist rather than conjuring a
	//! new one into an empty weapon.
	//! \param[in] members The group's living members.
	//! \param[out] deficient Every deficient magazine component found.
	//! \param[out] resources Index-aligned with deficient - the warehouse resource a replacement is
	//! drawn from (the muzzle's own default magazine name).
	protected void CollectDeficientMagazines(notnull array<IEntity> members, out array<BaseMagazineComponent> deficient, out array<string> resources)
	{
		deficient.Clear();
		resources.Clear();

		foreach (IEntity member : members)
		{
			if (!member)
				continue;

			BaseWeaponManagerComponent weaponManager = BaseWeaponManagerComponent.Cast(member.FindComponent(BaseWeaponManagerComponent));
			if (!weaponManager)
				continue;

			array<WeaponSlotComponent> weaponSlots = {};
			weaponManager.GetWeaponsSlots(weaponSlots);

			foreach (WeaponSlotComponent slot : weaponSlots)
			{
				if (!slot)
					continue;

				IEntity weaponEntity = slot.GetWeaponEntity();
				if (!weaponEntity)
					continue;

				BaseWeaponComponent weapon = BaseWeaponComponent.Cast(weaponEntity.FindComponent(BaseWeaponComponent));
				if (!weapon)
					continue;

				BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
				if (!muzzle)
					continue;

				BaseMagazineComponent magazine = muzzle.GetMagazine();
				if (!magazine)
					continue;

				if (magazine.GetAmmoCount() >= magazine.GetMaxAmmoCount())
					continue;

				string resource = muzzle.GetDefaultMagazineOrProjectileName();
				if (resource == "")
					continue;

				deficient.Insert(magazine);
				resources.Insert(resource);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: refuels every live vehicle group from the best covering fuel source, on
	//! m_iRefuelIntervalMs. A foot group is skipped - GetVehicleEntity answers null for it.
	protected void RefuelTick()
	{
		if (!Replication.IsServer())
			return;

		if (!m_mGroups || m_mGroups.IsEmpty())
			return;

		array<string> groupIds = {};
		for (int i = 0; i < m_mGroups.Count(); i++)
		{
			groupIds.Insert(m_mGroups.GetKey(i));
		}

		foreach (string groupId : groupIds)
		{
			OVT_HighCommandRecord record = GetGroup(groupId);
			if (!record)
				continue;

			SCR_AIGroup group = GetGroupEntity(record);
			if (!group)
				continue;

			if (group.GetAgentsCount() <= 0)
				continue;

			IEntity vehicle = GetVehicleEntity(record);
			if (!vehicle)
				continue;

			RefuelGroup(record, vehicle);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: fills one group's vehicle from the best covering fuel source, using EXACTLY the fast
	//! Fill action's own maths - OVT_FuelUtils for the source rule and the price, and
	//! OVT_FuelPricing.ComputeFillPlan for the balance-bounded litres - so a background tick and a
	//! player holding the Fill action can never disagree about what a litre costs here. Neither
	//! OVT_FuelUtils nor OVT_FuelPricing is modified (I6); this is a third caller of both, exactly
	//! like OVT_FuelRequestComponent is the second.
	//!
	//! CHARGED FOR WHAT ARRIVED, NEVER FOR WHAT WAS PLANNED (the OVT_FuelRequestComponent rule), and
	//! the sub-dollar remainder is accrued PER GROUP, not per player: two groups owned by the same
	//! player must never share one pot.
	//! \param[in] record The group's record - its owner and id.
	//! \param[in] vehicle The group's live vehicle.
	protected void RefuelGroup(notnull OVT_HighCommandRecord record, notnull IEntity vehicle)
	{
		if (!Replication.IsServer())
			return;

		SCR_FuelManagerComponent target = OVT_FuelUtils.GetOwnFuelManager(vehicle);
		if (!target)
			return;

		float needed = OVT_FuelUtils.GetRefuelableCapacity(target);
		if (needed <= 0)
			return;

		SCR_FuelSupportStationComponent source = OVT_FuelUtils.FindBestFillSource(vehicle);
		if (!source)
			return;

		SCR_FuelManagerComponent sourceFuel = OVT_FuelUtils.GetStationFuelManager(source);
		float available = needed;
		if (sourceFuel)
			available = OVT_FuelUtils.GetProvidableFuel(sourceFuel);

		if (available <= 0)
			return;

		float price = OVT_FuelUtils.GetFuelCostPerLitre(source);

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
			return;

		int balance = economy.GetPlayerMoney(record.m_sOwnerPersistentId);

		OVT_FuelFillPlan plan = OVT_FuelPricing.ComputeFillPlan(needed, available, balance, price);
		if (!plan.HasFuel())
			return;

		float delivered = AddFuelToManager(target, plan.m_fLitres);
		if (delivered <= 0)
			return;

		if (sourceFuel)
			DrainFuelFromManager(sourceFuel, delivered);

		if (!m_FuelChargeLedger)
			m_FuelChargeLedger = new OVT_FuelChargeLedger();

		int whole = m_FuelChargeLedger.Accrue(record.m_sGroupId, delivered, price);
		if (whole > 0)
			economy.TakePlayerMoneyPersistentId(record.m_sOwnerPersistentId, whole);
	}

	//------------------------------------------------------------------------------------------------
	//! Pours litres into every node of ONE fuel manager that can receive them. SERVER ONLY.
	//!
	//! The OVT_FuelRequestComponent.AddFuelToManager shape, duplicated rather than shared: that
	//! method is not exposed for reuse and this feature does not modify economy/fuel files (I6).
	//! \param[in] manager The fuel manager being filled.
	//! \param[in] litres How much to add in total.
	//! \return Litres actually placed in this manager's nodes.
	protected float AddFuelToManager(notnull SCR_FuelManagerComponent manager, float litres)
	{
		if (litres <= 0)
			return 0;

		array<SCR_FuelNode> nodes = {};
		manager.GetScriptedFuelNodesList(nodes, SCR_EFuelNodeTypeFlag.CAN_RECEIVE_FUEL);

		float remaining = litres;
		float delivered = 0;

		foreach (SCR_FuelNode node : nodes)
		{
			if (!node)
				continue;

			if (remaining <= 0)
				break;

			float room = node.GetMaxFuel() - node.GetFuel();
			if (room <= 0)
				continue;

			float add = remaining;
			if (add > room)
				add = room;

			node.SetFuel(node.GetFuel() + add);

			remaining -= add;
			delivered += add;
		}

		return delivered;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes litres back out of a source's providing nodes. SERVER ONLY - the conservation half of
	//! the transfer; see AddFuelToManager's header for why this is not shared with economy/fuel.
	//! \param[in] manager The source's fuel manager.
	//! \param[in] litres How much to remove in total.
	protected void DrainFuelFromManager(notnull SCR_FuelManagerComponent manager, float litres)
	{
		if (litres <= 0)
			return;

		array<SCR_FuelNode> nodes = {};
		manager.GetScriptedFuelNodesList(nodes, SCR_EFuelNodeTypeFlag.CAN_PROVIDE_FUEL);

		float remaining = litres;

		foreach (SCR_FuelNode node : nodes)
		{
			if (!node)
				continue;

			if (remaining <= 0)
				break;

			float fuel = node.GetFuel();
			if (fuel <= 0)
				continue;

			float take = remaining;
			if (take > fuel)
				take = fuel;

			node.SetFuel(fuel - take);

			remaining -= take;
		}
	}
}
