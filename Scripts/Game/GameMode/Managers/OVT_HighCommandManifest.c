//------------------------------------------------------------------------------------------------
//! One priced High Command purchase: what it costs, what nearby warehouse stock covers, and the
//! stores that coverage was measured against. SERVER ONLY, built by
//! OVT_HighCommandManagerComponent.QuoteEntry() and never held past the frame it was made in.
//!
//! m_aStores holds PLAIN POINTERS - a component's lifetime belongs to its entity, and a strong
//! reference from here is how a deleted holder is kept alive as a zombie.
//------------------------------------------------------------------------------------------------
class OVT_HighCommandQuote : Managed
{
	int m_iMemberCount;

	//! baseRecruitCost x members, before any gear.
	int m_iMemberCost;

	//! The gear NOT covered by stock, at local buy price, before the fee multiplier.
	int m_iChargedSubtotal;

	//! The group's vehicle at FULL shop buy price, or 0 for a foot group. Never fee-multiplied and
	//! never covered by warehouse stock - a vehicle is bought, not kitted out (R7).
	int m_iVehicleCost;

	//! What the player actually owes: member cost + the fee-multiplied charged subtotal + the vehicle.
	int m_iTotal;

	int m_iCoveredUnits;
	int m_iCoveredValue;
	int m_iChargedUnits;

	int m_iSupportersRequired;

	ref array<ref OVT_ItemSourcingLine> m_aManifest;
	ref array<OVT_StorageComponent> m_aStores;
}

//------------------------------------------------------------------------------------------------
//! What a High Command group's members are CARRYING, and what that is worth (implementation.md §3.4,
//! D4). SERVER ONLY - owned by OVT_HighCommandManagerComponent, never reached from a client.
//!
//! WHY IT SPAWNS ANYTHING AT ALL. Gear on a vanilla FIA character is authored across five separate
//! mechanisms spread over a three-deep inheritance chain (weapon slot, grenade slot, loadout manager
//! slots, initial inventory items, equipment storage slots). Reading that from IEntitySource means
//! hand-walking all five plus the ancestry the engine resolves for free; ONE spawned character
//! resolves the lot in a single inventory read. Composition is the opposite - the member list comes
//! off the prefab source for nothing - and the manager does that half.
//!
//! THE COST IS BOUNDED AND PAID ONCE. Ten to fifteen distinct character prefabs, once per session,
//! ONE PER CALL-QUEUE HOP, each untracked and deleted immediately. It never spawns a group prefab.
//!
//! CAPTURE IS DEFERRED ONE FURTHER FRAME AND THEN RETRIED. InitialInventoryItems land during entity
//! init, not synchronously with the spawn call, so the frame after the spawn frequently still reads
//! an empty inventory. On expiry the prefab records an EMPTY manifest and a WARNING names it - an
//! unpriceable member costs baseRecruitCost only, it never blocks a purchase.
//!
//! UNREGISTERED RESOURCES ARE OMITTED, NOT PRICED AT ZERO. GetInventoryId() is a bare map index and
//! resolves an unregistered prefab to id 0 - some other item's price - so registration is checked
//! before every lookup. A line that fails it is dropped rather than carried at price 0, because a
//! zero-price line would still consume warehouse stock the player was never charged for.
//------------------------------------------------------------------------------------------------
class OVT_HighCommandManifest : Managed
{
	//! How many frames a spawned character's inventory may read empty before it is written off.
	static const int MANIFEST_CAPTURE_ATTEMPTS = 5;

	//! How far inside the world's bounding-box corner the scratch position sits, in metres.
	static const float SCRATCH_INSET = 200.0;

	//! Depth limit on the container walk - a bag inside a bag inside a bag is already unreal.
	static const int MAX_NESTING_DEPTH = 6;

	//! Captured gear per character prefab: one line per resource with m_iNeeded set and m_iUnitPrice
	//! left at 0. PRICES ARE NEVER CACHED - they depend on the shop position and the buying player.
	protected ref map<ResourceName, ref array<ref OVT_ItemSourcingLine>> m_mCharacterLines;

	//! Summed gear per authored entry key. Dropped whenever a capture lands, so a sum computed while
	//! the build was still running is replaced rather than trusted.
	protected ref map<string, ref array<ref OVT_ItemSourcingLine>> m_mEntryLines;

	//! Character prefabs still waiting to be captured, in order.
	protected ref array<ResourceName> m_aPending;

	//! True while a capture is in flight, so the pump is never started twice.
	protected bool m_bCapturing;

	//! The prefab currently being captured, and the scratch body doing it.
	protected ResourceName m_sCaptureResource;
	protected EntityID m_CaptureEntityId;

	//! Resolved once per session by ResolveScratchPosition().
	protected vector m_vScratch;
	protected bool m_bScratchResolved;

	//------------------------------------------------------------------------------------------------
	void OVT_HighCommandManifest()
	{
		m_mCharacterLines = new map<ResourceName, ref array<ref OVT_ItemSourcingLine>>();
		m_mEntryLines = new map<string, ref array<ref OVT_ItemSourcingLine>>();
		m_aPending = new array<ResourceName>();
		m_bCapturing = false;
		m_bScratchResolved = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Queues character prefabs for capture and starts the pump. Idempotent: a prefab already captured
	//! or already queued is ignored, so calling this twice costs nothing.
	//! \param[in] prefabs Distinct character prefabs across every authored entry.
	void QueueCharacters(notnull array<ResourceName> prefabs)
	{
		if (!Replication.IsServer())
			return;

		foreach (ResourceName prefab : prefabs)
		{
			if (prefab.IsEmpty())
				continue;

			if (m_mCharacterLines.Contains(prefab))
				continue;

			if (m_aPending.Find(prefab) != -1)
				continue;

			m_aPending.Insert(prefab);
		}

		StartPump();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when every queued character prefab has been captured.
	bool IsComplete()
	{
		return m_aPending.IsEmpty() && !m_bCapturing;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] prefab A character prefab.
	//! \return True when its gear has been captured (successfully or as an empty write-off).
	bool HasCharacter(ResourceName prefab)
	{
		return m_mCharacterLines.Contains(prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! The priced required-item manifest for one authored entry.
	//!
	//! Every returned line is a FRESH object: the cached lines carry counts only and are never handed
	//! out, so no caller can stamp a price onto another caller's manifest.
	//!
	//! A member whose gear has not been captured yet contributes nothing and is promoted to the front
	//! of the capture queue, so the next quote is right (D19 - the purchase re-derives anyway).
	//! \param[in] entryKey The authored entry's key, used as the sum's cache key.
	//! \param[in] memberPrefabs The entry's composition, one entry per member.
	//! \param[in] pos Where the buying happens - feeds the shop price's town-stock term.
	//! \param[in] playerId Runtime id of the buyer, for their own price multiplier.
	//! \param[out] manifest Receives the priced lines. Allocated if null, cleared otherwise.
	//! \return How many lines the manifest has.
	int BuildEntryManifest(string entryKey, notnull array<ResourceName> memberPrefabs, vector pos, int playerId, out array<ref OVT_ItemSourcingLine> manifest)
	{
		if (!manifest)
			manifest = new array<ref OVT_ItemSourcingLine>();

		manifest.Clear();

		array<ref OVT_ItemSourcingLine> counted = ResolveEntryCounts(entryKey, memberPrefabs);
		if (!counted)
			return 0;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
			return 0;

		foreach (OVT_ItemSourcingLine source : counted)
		{
			if (!source || source.m_iNeeded <= 0)
				continue;

			// Registered, inherited or classified - only a prefab no route can price is omitted
			// (GetBuyPriceForPrefab never indexes an unregistered name to id 0).
			int unitPrice = economy.GetBuyPriceForPrefab(source.m_sResource, pos, playerId);
			if (unitPrice < 0)
				continue;

			OVT_ItemSourcingLine line = new OVT_ItemSourcingLine();
			line.m_sResource = source.m_sResource;
			line.m_iNeeded = source.m_iNeeded;
			line.m_iUnitPrice = unitPrice;

			manifest.Insert(line);
		}

		return manifest.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The per-resource sum over one entry's members, counts only.
	//! \param[in] entryKey Cache key.
	//! \param[in] memberPrefabs The entry's composition.
	//! \return The summed lines. Never null.
	protected array<ref OVT_ItemSourcingLine> ResolveEntryCounts(string entryKey, notnull array<ResourceName> memberPrefabs)
	{
		if (entryKey != "" && m_mEntryLines.Contains(entryKey))
			return m_mEntryLines[entryKey];

		array<ref OVT_ItemSourcingLine> summed = new array<ref OVT_ItemSourcingLine>();
		map<string, int> totals = new map<string, int>();
		array<string> order = new array<string>();

		bool complete = true;

		foreach (ResourceName memberPrefab : memberPrefabs)
		{
			if (memberPrefab.IsEmpty())
				continue;

			if (!m_mCharacterLines.Contains(memberPrefab))
			{
				complete = false;
				Promote(memberPrefab);
				continue;
			}

			array<ref OVT_ItemSourcingLine> memberLines = m_mCharacterLines[memberPrefab];
			foreach (OVT_ItemSourcingLine line : memberLines)
			{
				if (!line)
					continue;

				if (!totals.Contains(line.m_sResource))
				{
					totals.Set(line.m_sResource, line.m_iNeeded);
					order.Insert(line.m_sResource);
				}
				else
				{
					totals.Set(line.m_sResource, totals.Get(line.m_sResource) + line.m_iNeeded);
				}
			}
		}

		foreach (string res : order)
		{
			OVT_ItemSourcingLine line = new OVT_ItemSourcingLine();
			line.m_sResource = res;
			line.m_iNeeded = totals.Get(res);
			line.m_iUnitPrice = 0;
			summed.Insert(line);
		}

		// A partial sum is never cached - the capture it is missing is already queued, and the next
		// quote must see the real number rather than this one.
		if (complete && entryKey != "")
			m_mEntryLines.Set(entryKey, summed);

		return summed;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves a character prefab to the front of the capture queue.
	//! \param[in] prefab The prefab a quote just found missing.
	protected void Promote(ResourceName prefab)
	{
		if (m_aPending.Find(prefab) != -1)
			return;

		m_aPending.InsertAt(prefab, 0);
		StartPump();
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the one-per-hop capture pump, unless one is already running.
	protected void StartPump()
	{
		if (m_bCapturing || m_aPending.IsEmpty())
			return;

		if (!GetGame() || !GetGame().GetCallqueue())
			return;

		m_bCapturing = true;
		GetGame().GetCallqueue().CallLater(CaptureNext, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the next pending character at the scratch position. The read is a frame later.
	protected void CaptureNext()
	{
		if (m_aPending.IsEmpty())
		{
			m_bCapturing = false;
			return;
		}

		if (!GetGame() || !GetGame().GetCallqueue())
		{
			m_bCapturing = false;
			return;
		}

		ResourceName prefab = m_aPending[0];
		m_aPending.RemoveOrdered(0);

		if (prefab.IsEmpty() || m_mCharacterLines.Contains(prefab))
		{
			GetGame().GetCallqueue().CallLater(CaptureNext, 0, false);
			return;
		}

		SCR_ChimeraCharacter character = OVT_WorldUtils.SpawnCharacterEntity(prefab, ResolveScratchPosition());
		if (!character)
		{
			RecordLines(prefab, null);
			Print("[Overthrow] High Command could not spawn '" + prefab + "' to read its gear - members of that prefab will be priced at the recruit cost only", LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(CaptureNext, 0, false);
			return;
		}

		OVT_PersistenceManagerComponent.UntrackTransient(character);

		m_sCaptureResource = prefab;
		m_CaptureEntityId = character.GetID();

		// ONE FURTHER FRAME: the inventory is filled during entity init, not by the spawn call.
		GetGame().GetCallqueue().CallLater(CaptureAttempt, 0, false, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the scratch body's gear, retrying while it still reads empty.
	//! \param[in] attempt 1-based attempt number; MANIFEST_CAPTURE_ATTEMPTS is the budget.
	protected void CaptureAttempt(int attempt)
	{
		if (!GetGame() || !GetGame().GetCallqueue())
		{
			m_bCapturing = false;
			return;
		}

		BaseWorld world = GetGame().GetWorld();
		IEntity character;
		if (world)
			character = world.FindEntityByID(m_CaptureEntityId);

		if (!character)
		{
			RecordLines(m_sCaptureResource, null);
			Print("[Overthrow] The scratch body for High Command prefab '" + m_sCaptureResource + "' disappeared before its gear could be read - members of that prefab will be priced at the recruit cost only", LogLevel.WARNING);
			FinishCapture();
			return;
		}

		map<string, int> counts = new map<string, int>();
		ReadGear(character, counts);

		if (counts.IsEmpty() && attempt < MANIFEST_CAPTURE_ATTEMPTS)
		{
			GetGame().GetCallqueue().CallLater(CaptureAttempt, 0, false, attempt + 1);
			return;
		}

		if (counts.IsEmpty())
			Print("[Overthrow] High Command read no gear at all off '" + m_sCaptureResource + "' after " + MANIFEST_CAPTURE_ATTEMPTS.ToString() + " frames - members of that prefab will be priced at the recruit cost only", LogLevel.WARNING);

		RecordLines(m_sCaptureResource, counts);

		// Untracked again immediately before the delete: the spawn-side call may still be sitting in
		// the retry queue waiting for the native lazy registration to land.
		OVT_PersistenceManagerComponent.UntrackTransient(character);
		SCR_EntityHelper.DeleteEntityAndChildren(character);

		FinishCapture();
	}

	//------------------------------------------------------------------------------------------------
	//! Ends one capture and queues the next.
	protected void FinishCapture()
	{
		m_sCaptureResource = ResourceName.Empty;
		m_CaptureEntityId = EntityID.INVALID;

		if (!GetGame() || !GetGame().GetCallqueue())
		{
			m_bCapturing = false;
			return;
		}

		GetGame().GetCallqueue().CallLater(CaptureNext, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Writes one character prefab's captured gear into the cache and invalidates every entry sum.
	//! \param[in] prefab The character prefab.
	//! \param[in] counts Resource -> quantity, or null for a write-off.
	protected void RecordLines(ResourceName prefab, map<string, int> counts)
	{
		if (prefab.IsEmpty())
			return;

		array<ref OVT_ItemSourcingLine> lines = new array<ref OVT_ItemSourcingLine>();

		if (counts)
		{
			foreach (string res, int qty : counts)
			{
				if (res == "" || qty <= 0)
					continue;

				OVT_ItemSourcingLine line = new OVT_ItemSourcingLine();
				line.m_sResource = res;
				line.m_iNeeded = qty;
				line.m_iUnitPrice = 0;
				lines.Insert(line);
			}
		}

		m_mCharacterLines.Set(prefab, lines);
		m_mEntryLines.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Everything one character is carrying, counted by prefab.
	//! \param[in] character The scratch body.
	//! \param[in] counts Receives resource -> quantity.
	protected void ReadGear(notnull IEntity character, notnull map<string, int> counts)
	{
		InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		if (!inventory)
			return;

		array<IEntity> items = new array<IEntity>();
		inventory.GetItems(items);

		// An item reachable from two registered storages must not be counted twice.
		map<EntityID, int> seen = new map<EntityID, int>();

		foreach (IEntity item : items)
		{
			CountItem(item, counts, seen, 0);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Counts one item and everything inside it.
	//! \param[in] item The item.
	//! \param[in] counts Running resource -> quantity tally.
	//! \param[in] seen Entities already counted on this walk.
	//! \param[in] depth Current nesting depth.
	protected void CountItem(IEntity item, notnull map<string, int> counts, notnull map<EntityID, int> seen, int depth)
	{
		if (!item || depth > MAX_NESTING_DEPTH)
			return;

		EntityID id = item.GetID();
		if (seen.Contains(id))
			return;

		seen.Set(id, 1);

		ResourceName res = OVT_PrefabUtils.GetPrefabName(item);
		if (!res.IsEmpty())
		{
			string key = res;
			if (counts.Contains(key))
				counts.Set(key, counts.Get(key) + 1);
			else
				counts.Set(key, 1);
		}

		array<Managed> storages = new array<Managed>();
		item.FindComponents(BaseInventoryStorageComponent, storages);

		foreach (Managed found : storages)
		{
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(found);
			if (!storage)
				continue;

			array<IEntity> contents = new array<IEntity>();
			storage.GetAll(contents);

			foreach (IEntity child : contents)
			{
				CountItem(child, counts, seen, depth + 1);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Where the scratch bodies stand: a corner of the world, well away from every base.
	//!
	//! It is checked against the base registry rather than assumed, because a scratch character
	//! standing inside a base is an armed stranger the occupying faction would react to for the two
	//! frames it exists.
	//! \return The scratch position, resolved once per session.
	protected vector ResolveScratchPosition()
	{
		if (m_bScratchResolved)
			return m_vScratch;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return vector.Zero;

		vector worldMin;
		vector worldMax;
		world.GetBoundBox(worldMin, worldMax);

		array<vector> candidates = new array<vector>();
		candidates.Insert(Vector(worldMin[0] + SCRATCH_INSET, 0, worldMin[2] + SCRATCH_INSET));
		candidates.Insert(Vector(worldMax[0] - SCRATCH_INSET, 0, worldMin[2] + SCRATCH_INSET));
		candidates.Insert(Vector(worldMin[0] + SCRATCH_INSET, 0, worldMax[2] - SCRATCH_INSET));
		candidates.Insert(Vector(worldMax[0] - SCRATCH_INSET, 0, worldMax[2] - SCRATCH_INSET));

		m_vScratch = candidates[0];
		m_vScratch[1] = world.GetSurfaceY(m_vScratch[0], m_vScratch[2]);

		foreach (vector candidate : candidates)
		{
			vector clamped = candidate;
			clamped[1] = world.GetSurfaceY(clamped[0], clamped[2]);

			if (!IsClearOfEveryBase(clamped))
				continue;

			m_vScratch = clamped;
			break;
		}

		m_bScratchResolved = true;
		return m_vScratch;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] position A candidate scratch position.
	//! \return True when no base sits within its own baseRange of it.
	protected bool IsClearOfEveryBase(vector position)
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!occupying || !config || !config.m_Difficulty)
			return true;

		OVT_BaseData base = occupying.GetNearestBase(position);
		if (!base)
			return true;

		return vector.Distance(base.location, position) > config.m_Difficulty.baseRange;
	}
}
