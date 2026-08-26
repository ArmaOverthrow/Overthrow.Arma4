class OVT_ResourceProductionManagerComponentClass: OVT_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! One production site's mutable state.
//!
//! Shaped after OVT_WarehouseData minus the surrogate id: the site's POSITION is the identity on
//! every wire and in the save, because world-query order is not contractually identical across
//! machines and an index would be a silent mis-address.
//------------------------------------------------------------------------------------------------
class OVT_ProductionSiteData : Managed
{
	//! The site entity's origin. The identity on every wire and in the save.
	vector location;

	//! "" unowned, "resistance", or a player persistent id.
	string owner;

	//! Meaningless while unowned; set true on purchase.
	bool isPrivate;

	//! SERVER ONLY. Fractional production not yet worth a whole unit. Never replicated.
	float carry;

	//! Local resolve from discovery. NOT replicated, NOT persisted.
	IEntity entity;

	//! The authored units-per-hour, copied at discovery. Lets the carry advance for a site whose
	//! entity is not reachable, which is what makes production world state rather than observer state.
	float rate;
}

//------------------------------------------------------------------------------------------------
//! One JIP ownership triple that arrived before this machine had a site to put it on.
//------------------------------------------------------------------------------------------------
class OVT_PendingSiteOwnership : Managed
{
	vector location;
	string owner;
	bool isPrivate;
}

//------------------------------------------------------------------------------------------------
//! Owns every production site's ownership, privacy and fractional carry, and runs the hourly drip.
//!
//! DISCOVERY, NOT REGISTRATION. The set of sites is authored in the world file and is therefore
//! identical on every machine, so InitializeSites() builds the list locally on client and server
//! alike and only owner/isPrivate are server truth. That is why nothing here needs a replicated
//! registry and why OVT_MapMarkerComponent - which exists for runtime-spawned entities - is not used.
//!
//! THE DRIP IS SERVER ONLY and runs one batch per in-game HOUR, latched by hour rather than by the
//! tick period, so a load or a sleep can neither double-pay nor lose a day (BUG-183).
//------------------------------------------------------------------------------------------------
class OVT_ResourceProductionManagerComponent: OVT_Component
{
	//-----------------------------------------------------------------------
	// CONSTANTS
	//-----------------------------------------------------------------------

	//! Drip tick period in ms before the day-time multiplier. The HOUR latch, not this number, is what
	//! makes production once-per-in-game-hour.
	static const int PRODUCTION_UPDATE_FREQUENCY = 60000;

	//! Metres within which a position names a site.
	static const float SITE_MATCH_RANGE = 10;

	//! The SQUARED match range. Every position decision is taken on squared distances because
	//! vector.Distance is not correctly rounded and an exact-boundary decision on it is a coin flip.
	static const float SITE_MATCH_RANGE_SQ = 100;

	//! JIP payload version. Positional and version-first, like every other stream in the mod.
	static const int JIP_VERSION = 1;

	//! How long the one retry of a persisted payload waits for the world to finish being built.
	static const int STAGED_APPLY_RETRY_MS = 1000;

	//! Passes allowed over a staged payload: the first one, plus EXACTLY ONE retry. What is still
	//! unmatched after that is named at ERROR and dropped rather than retried forever.
	static const int STAGED_APPLY_ATTEMPTS = 2;

	//-----------------------------------------------------------------------
	// MEMBER VARIABLES
	//-----------------------------------------------------------------------

	//! Every discovered site. Built on EVERY machine; only owner/isPrivate are server truth.
	protected ref array<ref OVT_ProductionSiteData> m_aSites;

	//! The hour the last production batch ran on. -1 until the first batch.
	protected int m_iHourProduced = -1;

	//! BUG-183's shape. The latch is not persisted while the in-game clock IS, so a loaded campaign
	//! would produce the hour its save was taken in all over again. Asserted from the clock once, on
	//! the first tick that can read it.
	protected bool m_bLatchAsserted;

	//! The save payload waiting to be applied, retained after a successful pass so a second
	//! ApplyStaged() lands on the same state instead of doing nothing.
	protected ref array<ref OVT_PersistedProductionSite> m_aStagedSites;

	//! Passes over m_aStagedSites that left at least one record unmatched. Only failing passes count.
	protected int m_iStagedApplyAttempts;

	//! True once InitializeSites() has actually walked the world. What decides whether a staged
	//! payload waits for Init() or can be applied where it lands.
	protected bool m_bSitesDiscovered;

	//! True once Init() has run to completion on this machine.
	protected bool m_bInitialised;

	//! CLIENT ONLY. JIP triples whose site this machine had not discovered when RplLoad ran. Drained
	//! at the end of Init(), after discovery.
	protected ref array<ref OVT_PendingSiteOwnership> m_aPendingOwnership;

	//-----------------------------------------------------------------------
	// STATIC
	//-----------------------------------------------------------------------

	static OVT_ResourceProductionManagerComponent s_Instance;

	//------------------------------------------------------------------------------------------------
	//! \return The manager on the game mode, or null before it exists.
	static OVT_ResourceProductionManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_ResourceProductionManagerComponent.Cast(pGameMode.FindComponent(OVT_ResourceProductionManagerComponent));
		}

		return s_Instance;
	}

	//-----------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------

	void OVT_ResourceProductionManagerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_aSites = new array<ref OVT_ProductionSiteData>();
	}

	//------------------------------------------------------------------------------------------------
	//! Discovers every site on this machine, then starts the drip on the server.
	//! Called from OVT_OverthrowGameMode.EOnInit like every other manager.
	//! \param[in] owner The game mode entity.
	void Init(IEntity owner)
	{
		InitializeSites();

		Print(string.Format("[Overthrow] Found %1 resource production sites", m_aSites.Count().ToString()));

		// AFTER discovery, unconditionally: a save read before this line has been staged and is
		// waiting for records to land on. A client never has one.
		ApplyStaged();

		// A joining client's JIP payload can arrive before the world is walkable. Whatever it could
		// not place then is placed now, on the list discovery has just built.
		ApplyPendingOwnership();

		m_bInitialised = true;

		if (!Replication.IsServer())
			return;

		float timeMul = 1;
		OVT_TimeAndWeatherHandlerComponent tw = OVT_TimeAndWeatherHandlerComponent.Cast(GetGame().GetGameMode().FindComponent(OVT_TimeAndWeatherHandlerComponent));
		if (tw)
			timeMul = tw.GetDayTimeMultiplier();

		if (timeMul <= 0)
			timeMul = 1;

		// Belt and braces: if the persisted clock is already restored this closes the duplicate batch
		// immediately, and if it is not the one-shot in CheckProduction re-asserts from the clock.
		AssertHourLatchFromClock();

		GetGame().GetCallqueue().CallLater(CheckProduction, PRODUCTION_UPDATE_FREQUENCY / timeMul, true, GetOwner());
	}

	//-----------------------------------------------------------------------
	// DISCOVERY
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Finds every map-authored site. Runs on every machine, and is safe to run more than once - a
	//! record already at a discovered position keeps its state and simply re-adopts the entity, so a
	//! second pass can never orphan ownership that has already arrived.
	void InitializeSites()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		world.QueryEntitiesBySphere("0 0 0", 99999999, CheckSiteAdd, FilterSiteEntities, EQueryEntitiesFlags.STATIC);

		m_bSitesDiscovered = true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] ent A candidate the query offered.
	//! \return Always true - the query walks every static entity in the world.
	protected bool CheckSiteAdd(IEntity ent)
	{
		OVT_ResourceProductionComponent production = OVT_ComponentFinder<OVT_ResourceProductionComponent>.Find(ent);
		if (!production)
			return true;

		OVT_ProductionSiteData existing = GetSite(ent.GetOrigin());
		if (existing)
		{
			existing.entity = ent;
			existing.rate = production.GetUnitsPerHour();
			return true;
		}

		OVT_ProductionSiteData data = new OVT_ProductionSiteData();
		data.location = ent.GetOrigin();
		data.owner = "";
		data.isPrivate = true;
		data.carry = 0;
		data.entity = ent;
		data.rate = production.GetUnitsPerHour();

		m_aSites.Insert(data);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entity A candidate the query offered.
	//! \return True when the entity is a production site.
	protected bool FilterSiteEntities(IEntity entity)
	{
		if (OVT_ComponentFinder<OVT_ResourceProductionComponent>.Find(entity))
			return true;

		return false;
	}

	//-----------------------------------------------------------------------
	// CLIENT-SAFE GETTERS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \return Every discovered site. Never null.
	array<ref OVT_ProductionSiteData> GetSites()
	{
		return m_aSites;
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest record within SITE_MATCH_RANGE of a position. SQUARED comparison throughout.
	//! \param[in] pos The claimed position.
	//! \return The record, or null when nothing is close enough.
	OVT_ProductionSiteData GetSite(vector pos)
	{
		OVT_ProductionSiteData best;
		float bestSq = SITE_MATCH_RANGE_SQ;

		foreach (OVT_ProductionSiteData rec : m_aSites)
		{
			if (!rec)
				continue;

			float sq = vector.DistanceSq(rec.location, pos);
			if (sq >= bestSq)
				continue;

			bestSq = sq;
			best = rec;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] site A site entity.
	//! \return Its record, or null when the entity is not a discovered site.
	OVT_ProductionSiteData GetSiteForEntity(IEntity site)
	{
		if (!site)
			return null;

		foreach (OVT_ProductionSiteData rec : m_aSites)
		{
			if (rec && rec.entity == site)
				return rec;
		}

		return GetSite(site.GetOrigin());
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] site A site entity.
	//! \return "" unowned or unknown, "resistance", or a player persistent id.
	string GetSiteOwner(IEntity site)
	{
		OVT_ProductionSiteData rec = GetSiteForEntity(site);
		if (!rec)
			return "";

		return rec.owner;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] site A site entity.
	//! \return True when the site is private. Meaningless while unowned.
	bool IsSitePrivate(IEntity site)
	{
		OVT_ProductionSiteData rec = GetSiteForEntity(site);
		if (!rec)
			return true;

		return rec.isPrivate;
	}

	//-----------------------------------------------------------------------
	// SERVER API
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: sets a site's owner and publishes it to every machine.
	//!
	//! The SERVER's copy of the position is what the broadcast carries, never the caller's - a
	//! client's slightly different float would match here and miss on the far end.
	//! \param[in] pos The claimed position.
	//! \param[in] owner "" unowned, "resistance", or a player persistent id.
	void SetSiteOwner(vector pos, string owner)
	{
		if (!Replication.IsServer())
			return;

		OVT_ProductionSiteData rec = GetSite(pos);
		if (!rec)
			return;

		RpcDo_SetSiteOwner(rec.location, owner);
		Rpc(RpcDo_SetSiteOwner, rec.location, owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sets a site's privacy flag and publishes it to every machine.
	//! \param[in] pos The claimed position.
	//! \param[in] isPrivate True to close the site to everyone but its owner.
	void SetSitePrivacy(vector pos, bool isPrivate)
	{
		if (!Replication.IsServer())
			return;

		OVT_ProductionSiteData rec = GetSite(pos);
		if (!rec)
			return;

		RpcDo_SetSitePrivacy(rec.location, isPrivate);
		Rpc(RpcDo_SetSitePrivacy, rec.location, isPrivate);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: empties a site's stock. Called in the same server call that takes the money for it.
	//!
	//! OVT_ResourceLedger.Clear() deliberately fires no change event, so the republish is the caller's
	//! job - without it every other machine keeps showing the stock the buyer just destroyed.
	//! \param[in] site The site entity.
	void ClearSiteStock(IEntity site)
	{
		if (!Replication.IsServer())
			return;

		if (!site)
			return;

		OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(site);
		if (!store)
			return;

		OVT_ResourceLedger ledger = store.GetLedger();
		if (!ledger)
			return;

		ledger.Clear();
		store.PublishContents();
	}

	//-----------------------------------------------------------------------
	// PERSISTENCE
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Accepts a save payload for application. Called only from OVT_ResourceProductionManagerSerializer.
	//!
	//! THE SERIALIZER DELIBERATELY APPLIES NOTHING ITSELF (R4). Deserialization runs while the world
	//! is still being built, so a load that reached into a site's store from there would find no
	//! store and lose every site's stock, silently, on every load.
	//!
	//! Two arrival orders are both correct. When the payload lands BEFORE Init() it simply waits, and
	//! Init() applies it after discovery. When it lands AFTER (an in-session re-application of a
	//! running campaign, where Init() will not run again) discovery has already proven the entities
	//! exist, so it is applied where it lands.
	//! \param[in] sites The persisted records. Null or empty clears the queue.
	void StagePersisted(array<ref OVT_PersistedProductionSite> sites)
	{
		GetGame().GetCallqueue().Remove(ApplyStaged);

		m_aStagedSites = new array<ref OVT_PersistedProductionSite>();
		m_iStagedApplyAttempts = 0;

		if (sites)
		{
			foreach (OVT_PersistedProductionSite record : sites)
			{
				if (!record)
					continue;

				m_aStagedSites.Insert(record);
			}
		}

		if (m_bSitesDiscovered)
			ApplyStaged();
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the staged payload onto the discovered sites.
	//!
	//! IDEMPOTENT. Every field is SET, never accumulated, and the stock replaces the ledger rather
	//! than adding to it, so a second pass lands on exactly the same state. The payload is retained
	//! after a successful pass for that reason.
	//!
	//! A record that matches no discovered site - or whose site is not reachable yet - is retried
	//! ONCE, a second after the first pass, with discovery re-run first (InitializeSites() is
	//! idempotent). What is still unmatched after that is named at ERROR, one line per record, and
	//! dropped: a campaign's ownership disappearing without a word is the failure this exists against.
	void ApplyStaged()
	{
		if (!m_aStagedSites || m_aStagedSites.IsEmpty())
			return;

		if (m_iStagedApplyAttempts > 0)
			InitializeSites();

		array<ref OVT_PersistedProductionSite> matched = new array<ref OVT_PersistedProductionSite>();
		array<ref OVT_PersistedProductionSite> unmatched = new array<ref OVT_PersistedProductionSite>();

		foreach (OVT_PersistedProductionSite record : m_aStagedSites)
		{
			if (ApplyPersistedSite(record))
				matched.Insert(record);
			else
				unmatched.Insert(record);
		}

		if (unmatched.IsEmpty())
			return;

		m_iStagedApplyAttempts += 1;

		if (m_iStagedApplyAttempts < STAGED_APPLY_ATTEMPTS)
		{
			GetGame().GetCallqueue().Remove(ApplyStaged);
			GetGame().GetCallqueue().CallLater(ApplyStaged, STAGED_APPLY_RETRY_MS, false);
			return;
		}

		foreach (OVT_PersistedProductionSite dropped : unmatched)
		{
			int droppedLines = 0;
			if (dropped.stock)
				droppedLines = dropped.stock.Count();

			Print(string.Format("[OVT_ResourceProductionManagerComponent] Persisted production site at %1 FAILED to reach a discovered site within %2m after %3 attempt(s). Its owner ('%4'), privacy, carry and %5 stock line(s) are lost. Check that the save and the loaded world file agree on where the sites are - a site moved further than the match range starts unowned and empty.",
				dropped.location.ToString(),
				SITE_MATCH_RANGE.ToString(),
				m_iStagedApplyAttempts.ToString(),
				dropped.owner,
				droppedLines.ToString()), LogLevel.ERROR);
		}

		// The dropped records are gone for good; the ones that DID land stay staged, so the payload
		// remains re-appliable and idempotent.
		m_aStagedSites = matched;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies one persisted record onto its discovered site.
	//! \param[in] record The persisted record.
	//! \return True when it landed; false to retry it.
	protected bool ApplyPersistedSite(OVT_PersistedProductionSite record)
	{
		if (!record)
			return true;

		OVT_ProductionSiteData live = GetSite(record.location);
		if (!live)
			return false;

		// The two REPLICATED fields go through their broadcasting writers, never by assignment: a
		// save re-applied into a live session (ReapplyLatestSaveData) would otherwise leave every
		// connected client on the pre-reapply ownership, privacy, map colour and storage gate. Both
		// writers set-then-broadcast and both are server-guarded, so a client-side apply is a no-op.
		SetSiteOwner(live.location, record.owner);
		SetSitePrivacy(live.location, record.isPrivate);

		live.carry = record.carry;

		return ApplyPersistedStock(live, record);
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces one site's ledger with EVERY persisted stock line.
	//!
	//! An EMPTY payload still clears the store: a site that was empty when the save was taken must be
	//! empty after it is applied, which is also what keeps a second pass from accumulating.
	//! ApplyPersisted() ends in PublishContents(), so this must not republish (Q6).
	//! \param[in] live The discovered record.
	//! \param[in] record The persisted record.
	//! \return True when the stock landed, or when there was none to land; false to retry.
	protected bool ApplyPersistedStock(notnull OVT_ProductionSiteData live, notnull OVT_PersistedProductionSite record)
	{
		array<ref OVT_PersistedResourceLine> lines = new array<ref OVT_PersistedResourceLine>();

		if (record.stock)
		{
			foreach (OVT_PersistedResourceLine line : record.stock)
			{
				if (!line || line.resourceId == "" || line.quantity <= 0)
					continue;

				lines.Insert(line);
			}
		}

		bool carriesStock = !lines.IsEmpty();

		if (!live.entity)
			return !carriesStock;

		OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(live.entity);
		if (!store)
			return !carriesStock;

		if (carriesStock)
		{
			// The catalogue turns units into litres. Without it ApplyPersisted() would write an empty
			// ledger and report nothing, so wait for it rather than eat the stock.
			OVT_ResourceDefs defs = ResolveDefs();
			if (!defs)
				return false;

			foreach (OVT_PersistedResourceLine known : lines)
			{
				if (defs.IndexOf(known.resourceId) == -1)
					return false;
			}
		}

		store.ApplyPersisted(lines);

		return true;
	}

	//-----------------------------------------------------------------------
	// THE DRIP
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: one production batch per in-game hour, whoever owns the site and whether or not anyone
	//! is watching.
	void CheckProduction()
	{
		if (!Replication.IsServer())
			return;

		// Not a one-shot until the clock read actually succeeds - a failed read leaves the guard down
		// so the next tick tries again.
		if (!m_bLatchAsserted && AssertHourLatchFromClock())
			m_bLatchAsserted = true;

		TimeContainer time = ResolveGameTime();
		if (!time)
			return;

		if (!OVT_ResourceProductionRules.ShouldProduce(time.m_iHours, m_iHourProduced))
			return;

		m_iHourProduced = time.m_iHours;

		ProduceForHours(1);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: runs one batch of `hours` in-game hours across every site.
	//! \param[in] hours Whole in-game hours to produce for. Non-positive is ignored.
	void ProduceForHours(int hours)
	{
		if (!Replication.IsServer())
			return;

		if (hours <= 0)
			return;

		OVT_ResourceDefs defs = ResolveDefs();

		foreach (OVT_ProductionSiteData rec : m_aSites)
		{
			if (!rec)
				continue;

			ProduceForSite(rec, hours, defs);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one site's batch. The carry advances FIRST and unconditionally - a site this machine
	//! cannot reach still produced, it simply has nowhere to put the whole units.
	//! \param[in] rec The site's record.
	//! \param[in] hours Whole in-game hours.
	//! \param[in] defs The definition table, or null when the catalogue is not up yet.
	protected void ProduceForSite(notnull OVT_ProductionSiteData rec, int hours, OVT_ResourceDefs defs)
	{
		float carryOut;
		int units = OVT_ResourceProductionRules.Produce(rec.rate, hours, rec.carry, carryOut);
		rec.carry = carryOut;

		if (units <= 0)
			return;

		if (!defs)
			return;

		OVT_ResourceProductionComponent production;
		if (rec.entity)
			production = OVT_ComponentFinder<OVT_ResourceProductionComponent>.Find(rec.entity);

		if (!production)
			return;

		OVT_ResourceStoreComponent store = production.GetStore();
		if (!store)
			return;

		OVT_ResourceLedger ledger = store.GetLedger();
		if (!ledger)
			return;

		string id = production.GetResourceId();

		int index = defs.IndexOf(id);
		if (index == -1)
			return;

		int fitted = OVT_ResourceProductionRules.FitProduction(units, store.GetFreeLitres(), defs.LitresAt(index));
		if (fitted <= 0)
			return;

		int added = ledger.Add(id, fitted, defs, store.GetCapacityLitres());
		if (added <= 0)
			return;

		store.PublishContents();
	}

	//------------------------------------------------------------------------------------------------
	//! Replays the production an in-game time skip would otherwise swallow.
	//!
	//! CONTRACT, shared with the economy and occupying-faction sweeps: called on the AUTHORITY, BEFORE
	//! the world clock is advanced. The latch is left asserted at the hour the player wakes in, so the
	//! resumed tick cannot pay that hour a second time.
	//! \param[in] hours Length of the skip in whole in-game hours. Non-positive is ignored.
	void HandleTimeSkip(int hours)
	{
		if (!Replication.IsServer())
			return;

		if (hours <= 0)
			return;

		TimeContainer time = ResolveGameTime();

		int startHour = -1;
		if (time)
			startHour = time.m_iHours;

		ProduceForHours(hours);

		if (startHour < 0)
			return;

		m_iHourProduced = OVT_SleepSchedule.LandingHour(startHour, hours);
		m_bLatchAsserted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the live clock and asserts the production latch to its hour.
	//! \return True when the clock was readable and the latch was written.
	protected bool AssertHourLatchFromClock()
	{
		TimeContainer time = ResolveGameTime();
		if (!time)
			return false;

		if (time.m_iHours < 0)
			return false;

		m_iHourProduced = time.m_iHours;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the in-game clock, re-acquiring the time manager if OnPostInit ran before the world
	//! had one.
	//! \return The current time, or null when the time manager is unreachable.
	protected TimeContainer ResolveGameTime()
	{
		if (!m_Time)
		{
			IEntity owner = GetOwner();
			if (!owner)
				return null;

			ChimeraWorld world = owner.GetWorld();
			if (!world)
				return null;

			m_Time = world.GetTimeAndWeatherManager();
		}

		if (!m_Time)
			return null;

		return m_Time.GetTime();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The resource catalogue, or null before the resource manager is up.
	protected OVT_ResourceDefs ResolveDefs()
	{
		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
			return null;

		return resources.GetDefs();
	}

	//-----------------------------------------------------------------------
	// BROADCASTS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Everyone: one site's owner changed.
	//!
	//! ⚠ THE Rpc() LINE IN SetSiteOwner SITS DIRECTLY UNDER A COMPILER-CHECKED CALL TO THIS HANDLER
	//! WITH THE SAME ARGUMENTS (BUG-090: Rpc() is an untyped variadic prototype). Do not hoist either
	//! half. ARITY 2.
	//! \param[in] pos The site's position, as the server holds it.
	//! \param[in] owner "" unowned, "resistance", or a player persistent id.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetSiteOwner(vector pos, string owner)
	{
		OVT_ProductionSiteData rec = GetSite(pos);
		if (!rec)
			return;

		rec.owner = owner;
	}

	//------------------------------------------------------------------------------------------------
	//! Everyone: one site's privacy flag changed. ARITY 2.
	//! \param[in] pos The site's position, as the server holds it.
	//! \param[in] isPrivate True when the site is closed to everyone but its owner.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetSitePrivacy(vector pos, bool isPrivate)
	{
		OVT_ProductionSiteData rec = GetSite(pos);
		if (!rec)
			return;

		rec.isPrivate = isPrivate;
	}

	//-----------------------------------------------------------------------
	// JIP
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The connect-time ownership payload. The site SET is not sent at all - every machine discovers
	//! it from the world file - so this carries only the two fields that are server truth. The carry
	//! is deliberately absent: it is server-only state.
	//! \param[in] writer The JIP stream.
	//! \return True - the payload is self-describing and cannot fail to write.
	override bool RplSave(ScriptBitWriter writer)
	{
		writer.WriteInt(JIP_VERSION);
		writer.WriteInt(m_aSites.Count());

		for (int i = 0; i < m_aSites.Count(); i++)
		{
			OVT_ProductionSiteData rec = m_aSites[i];

			writer.WriteVector(rec.location);
			writer.WriteString(rec.owner);
			writer.WriteBool(rec.isPrivate);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! EVERY triple is read whatever this machine discovered - components share one JIP stream, so a
	//! short read here would desync every component serialized after this one. A position this machine
	//! has no site at is QUEUED for Init() to place, never dropped here.
	//! \param[in] reader The JIP stream.
	//! \return False on a truncated payload, which fails the join loudly.
	override bool RplLoad(ScriptBitReader reader)
	{
		int version;
		if (!reader.ReadInt(version)) return false;

		if (version != JIP_VERSION)
			Print(string.Format("[Overthrow] JIP production payload is version %1, this machine speaks %2 - check for a version mismatch between client and server.", version.ToString(), JIP_VERSION.ToString()), LogLevel.WARNING);

		int count;
		if (!reader.ReadInt(count)) return false;

		// The world may not have been walked yet on this machine. Discovery is idempotent, so asking
		// again here costs nothing and is what keeps a joining client's ownership from landing on an
		// empty list.
		if (m_aSites.IsEmpty())
			InitializeSites();

		for (int i = 0; i < count; i++)
		{
			vector location;
			if (!reader.ReadVector(location)) return false;

			string owner;
			if (!reader.ReadString(owner)) return false;

			bool isPrivate;
			if (!reader.ReadBool(isPrivate)) return false;

			OVT_ProductionSiteData rec = GetSite(location);
			if (!rec)
			{
				// NOT dropped: discovery may simply not have run yet on this machine, and Init()
				// re-runs it. Dropping here loses a joining client's whole ownership payload.
				QueuePendingOwnership(location, owner, isPrivate);
				continue;
			}

			rec.owner = owner;
			rec.isPrivate = isPrivate;
		}

		// Init() has already been and gone, so nothing else will drain the queue - answer now, which
		// is the WARNING an unmatched triple has always got.
		if (m_bInitialised)
			ApplyPendingOwnership();

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: holds one JIP triple until discovery has something to put it on.
	//! \param[in] location The site's position, as the server holds it.
	//! \param[in] owner "" unowned, "resistance", or a player persistent id.
	//! \param[in] isPrivate True when the site is closed to everyone but its owner.
	protected void QueuePendingOwnership(vector location, string owner, bool isPrivate)
	{
		if (!m_aPendingOwnership)
			m_aPendingOwnership = new array<ref OVT_PendingSiteOwnership>();

		OVT_PendingSiteOwnership pending = new OVT_PendingSiteOwnership();
		pending.location = location;
		pending.owner = owner;
		pending.isPrivate = isPrivate;

		m_aPendingOwnership.Insert(pending);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: applies every queued JIP triple onto the sites discovery has since found. A triple that
	//! still matches nothing is named at WARNING and dropped, which is the answer it always got.
	protected void ApplyPendingOwnership()
	{
		if (!m_aPendingOwnership || m_aPendingOwnership.IsEmpty())
			return;

		foreach (OVT_PendingSiteOwnership entry : m_aPendingOwnership)
		{
			if (!entry)
				continue;

			OVT_ProductionSiteData rec = GetSite(entry.location);
			if (!rec)
			{
				Print(string.Format("[Overthrow] The server holds a production site at %1 that this machine did not discover. Its ownership is dropped - check that both machines load the same world file.", entry.location.ToString()), LogLevel.WARNING);
				continue;
			}

			rec.owner = entry.owner;
			rec.isPrivate = entry.isPrivate;
		}

		m_aPendingOwnership = null;
	}
}
