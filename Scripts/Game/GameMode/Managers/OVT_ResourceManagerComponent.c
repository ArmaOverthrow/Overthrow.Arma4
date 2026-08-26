class OVT_ResourceManagerComponentClass: OVT_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Owns the resource catalogue and the LIVE price table.
//!
//! The catalogue is built once from resources.conf into a pure OVT_ResourceDefs, so every rule that
//! needs a definition can be written and tested without a world. The price table is index-aligned
//! with it and is initialised to base at startup - nothing anywhere reads OVT_ResourceDefs base
//! prices as if they were live, which is the predictable bug this feature was warned about.
//!
//! Prices drift on this manager's OWN six-hour tick (D6), never on the economy manager's - economy is
//! read by this feature and never written. The tick is server-gated, latched to one step per window,
//! broadcast per changed price and carried to a joining client by RplSave/RplLoad.
//!
//! It also owns pile lifetime: SpawnOrMergePile is the one place a crate pile is created, and
//! DeletePileIfEmpty the one place one is destroyed.
//------------------------------------------------------------------------------------------------
class OVT_ResourceManagerComponent: OVT_Component
{
	//-----------------------------------------------------------------------
	// CONSTANTS
	//-----------------------------------------------------------------------

	//! Drift tick period in ms before the day-time multiplier, matching the economy's cadence (D6).
	static const int PRICE_UPDATE_FREQUENCY = 60000;

	//-----------------------------------------------------------------------
	// ATTRIBUTES
	//-----------------------------------------------------------------------

	[Attribute(desc: "Resource catalogue config")]
	ResourceName m_rResourcesConfigFile;

	[Attribute(defvalue: "0.5", desc: "Lowest a drifted price may go, as a fraction of base")]
	protected float m_fPriceBandMin;

	[Attribute(defvalue: "2.0", desc: "Highest a drifted price may go, as a fraction of base")]
	protected float m_fPriceBandMax;

	[Attribute(defvalue: "0.08", desc: "One drift step, as a fraction of base")]
	protected float m_fPriceStepFraction;

	[Attribute(defvalue: "0.5", desc: "Sell price as a fraction of the live buy price")]
	protected float m_fSellRatio;

	[Attribute(defvalue: "6", desc: "Metres within which a dropped load merges into an existing pile")]
	protected float m_fMergeRadius;

	[Attribute(defvalue: "30", desc: "Metres a construction site reaches for piles")]
	protected float m_fSupplyRadius;

	[Attribute(defvalue: "2000", desc: "Threat value that maps to full upward war pressure")]
	protected float m_fThreatReference;

	[Attribute("", UIWidgets.ResourceNamePicker, "", "et", desc: "Crate pile prefab")]
	protected ResourceName m_rPilePrefab;

	[Attribute("", UIWidgets.ResourceNamePicker, "", "et", desc: "Generic construction site prefab")]
	protected ResourceName m_rDefaultSitePrefab;

	//-----------------------------------------------------------------------
	// MEMBER VARIABLES
	//-----------------------------------------------------------------------

	//! Named m_ResourcesConfig, not m_Config - OVT_Component already owns m_Config.
	protected ref OVT_ResourcesConfig m_ResourcesConfig;

	//! The pure table every rule reads. Identical on every machine.
	protected ref OVT_ResourceDefs m_Defs;

	//! Stored (pre-multiplier) price per definition index. Server truth; the ONLY live price source.
	protected ref array<int> m_aCurrentPrice;

	//! The m_iHourPaid* latch idiom (OVT_EconomyManagerComponent.c:99-101): the hour prices were last
	//! walked on. -1 until the first drift.
	protected int m_iHourPricesDrifted = -1;

	//! BUG-183's shape. The latch is not persisted while the in-game clock IS, so a loaded campaign
	//! would drift the hour its save was taken in all over again. Asserted from the clock once, on the
	//! first tick that can read it.
	protected bool m_bLatchAsserted;

	//-----------------------------------------------------------------------
	// STATIC
	//-----------------------------------------------------------------------

	static OVT_ResourceManagerComponent s_Instance;

	//------------------------------------------------------------------------------------------------
	//! \return The manager on the game mode, or null before it exists.
	static OVT_ResourceManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_ResourceManagerComponent.Cast(pGameMode.FindComponent(OVT_ResourceManagerComponent));
		}

		return s_Instance;
	}

	//-----------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------

	void OVT_ResourceManagerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_Defs = new OVT_ResourceDefs();
		m_aCurrentPrice = new array<int>();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode()) return;

		LoadConfigs();
		BuildDefs();
		InitPricesToBase();
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the price drift tick. Called from OVT_OverthrowGameMode.EOnInit like every other manager.
	//! \param[in] owner The game mode entity.
	void Init(IEntity owner)
	{
		if (!Replication.IsServer())
			return;

		float timeMul = 6;
		OVT_TimeAndWeatherHandlerComponent tw = OVT_TimeAndWeatherHandlerComponent.Cast(GetGame().GetGameMode().FindComponent(OVT_TimeAndWeatherHandlerComponent));
		if (tw)
			timeMul = tw.GetDayTimeMultiplier();

		if (timeMul <= 0)
			timeMul = 1;

		// Belt and braces: if the persisted clock is already restored this closes the duplicate walk
		// immediately, and if it is not the one-shot in CheckPrices re-asserts from the clock. Nothing
		// reads the latch in between, so a meaningless hour here costs nothing.
		AssertHourLatchFromClock();

		GetGame().GetCallqueue().CallLater(CheckPrices, PRICE_UPDATE_FREQUENCY / timeMul, true, GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	protected void LoadConfigs()
	{
		Resource holder = BaseContainerTools.LoadContainer(m_rResourcesConfigFile);
		if (holder)
		{
			OVT_ResourcesConfig obj = OVT_ResourcesConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(holder.GetResource().ToBaseContainer()));
			if (obj)
				m_ResourcesConfig = obj;
		}

		if (!m_ResourcesConfig)
			Print("[Overthrow] OVT_ResourceManagerComponent could not load a resource catalogue from " + m_rResourcesConfigFile, LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	//! Flattens the config into the pure definition table. A duplicate or empty id is dropped by
	//! AddDef, which returns -1; the index of everything after it therefore shifts, so it is logged.
	protected void BuildDefs()
	{
		m_Defs = new OVT_ResourceDefs();

		if (!m_ResourcesConfig || !m_ResourcesConfig.m_aResources)
			return;

		foreach (OVT_Resource res : m_ResourcesConfig.m_aResources)
		{
			if (!res)
				continue;

			int litres = Math.Round(res.m_fCubicMetresPerUnit * 1000);
			int index = m_Defs.AddDef(res.m_sId, litres, res.m_fKgPerUnit, res.m_iBasePrice, res.m_iImportable, res.m_iIllegal);
			if (index == -1)
				Print("[Overthrow] resources.conf rejected the entry '" + res.m_sId + "' - the id is empty or already defined", LogLevel.ERROR);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every live price starts at base, held in m_aCurrentPrice. A save replaces these afterwards.
	protected void InitPricesToBase()
	{
		m_aCurrentPrice = new array<int>();

		for (int i = 0; i < m_Defs.Count(); i++)
		{
			m_aCurrentPrice.Insert(m_Defs.BasePriceAt(i));
		}
	}

	//-----------------------------------------------------------------------
	// PRICES
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The buy price a player is quoted, level multiplier applied after the band clamp.
	//! \param[in] index Definition index.
	//! \return The live price, or 0 when the index is unknown.
	int GetPrice(int index)
	{
		int stored = GetStoredPrice(index);
		if (stored == 0)
			return 0;

		return OVT_ResourceRules.ApplyLevelMultiplier(stored, GetPriceMultiplier());
	}

	//------------------------------------------------------------------------------------------------
	//! What a port pays for one unit.
	//! \param[in] index Definition index.
	//! \return The sell price, or 0 when the index is unknown.
	int GetSellPrice(int index)
	{
		int live = GetPrice(index);
		if (live == 0)
			return 0;

		return OVT_ResourceRules.SellPrice(live, m_fSellRatio);
	}

	//------------------------------------------------------------------------------------------------
	//! The CONFIG base. Only the drift maths and "relative to base" readouts may read this.
	//! \param[in] index Definition index.
	//! \return The base price, or 0 when the index is unknown.
	int GetBasePrice(int index)
	{
		return m_Defs.BasePriceAt(index);
	}

	//------------------------------------------------------------------------------------------------
	//! The walked, band-clamped price before the difficulty multiplier. The persisted value.
	//! \param[in] index Definition index.
	//! \return The stored price, or 0 when the index is unknown.
	int GetStoredPrice(int index)
	{
		if (index < 0 || index >= m_aCurrentPrice.Count())
			return 0;

		return m_aCurrentPrice[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Writes one stored price. Broadcast and persistence are wired in later phases; this exists now
	//! so a round-trip case can drift a price by hand.
	//! \param[in] index Definition index.
	//! \param[in] price The stored price to hold. Floored at 1.
	//! \return True when the index existed and was written.
	bool SetStoredPrice(int index, int price)
	{
		if (index < 0 || index >= m_aCurrentPrice.Count())
			return false;

		int value = price;
		if (value < 1)
			value = 1;

		m_aCurrentPrice[index] = value;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the whole stored price table from a save payload, keyed by stable resource id.
	//!
	//! The table is reset to base first, so a resource the payload does not mention starts at base and
	//! a second apply lands on the same table. An id resources.conf no longer defines is dropped with
	//! a warning rather than shifting every price after it.
	//! \param[in] priceIds Stable resource ids, index-aligned with priceValues.
	//! \param[in] priceValues The stored (pre-multiplier) price for each id.
	void ApplyPersistedPrices(array<string> priceIds, array<int> priceValues)
	{
		InitPricesToBase();

		if (!priceIds || !priceValues)
			return;

		int count = priceIds.Count();
		if (priceValues.Count() < count)
			count = priceValues.Count();

		for (int i = 0; i < count; i++)
		{
			int index = m_Defs.IndexOf(priceIds[i]);
			if (index == -1)
			{
				Print("[Overthrow] A saved resource price names '" + priceIds[i] + "', which resources.conf no longer defines. The price is dropped and everything else keeps its saved value.", LogLevel.WARNING);
				continue;
			}

			SetStoredPrice(index, priceValues[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Difficulty scaling applied to a stored price at read time, AFTER the band clamp.
	//! \return The level multiplier, or 1 when no config has resolved yet.
	protected float GetPriceMultiplier()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return 1.0;

		return config.GetResourcePriceMultiplier();
	}

	//-----------------------------------------------------------------------
	// DRIFT
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: walks every stored price once per six-hour window (00, 06, 12, 18).
	void CheckPrices()
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

		if (!OVT_ResourceRules.ShouldDrift(time.m_iHours, m_iHourPricesDrifted))
			return;

		m_iHourPricesDrifted = time.m_iHours;

		DriftPrices();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one drift step per resource, broadcast for each price that actually moved.
	protected void DriftPrices()
	{
		float pressure = OVT_ResourceRules.WarPressure(ResolveThreat(), m_fThreatReference, ResolveControlledPortFraction());
		float volatility = ResolvePriceVolatility();

		for (int i = 0; i < m_aCurrentPrice.Count(); i++)
		{
			int current = m_aCurrentPrice[i];
			float roll = Math.RandomFloat(-1, 1);

			int next = OVT_ResourceRules.DriftStep(GetBasePrice(i), current, roll, pressure, volatility, m_fPriceStepFraction, m_fPriceBandMin, m_fPriceBandMax);
			if (next == current)
				continue;

			BroadcastPrice(i, next);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return The occupying faction's threat, or 0 when it has not resolved.
	protected float ResolveThreat()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return 0;

		return occupying.GetThreatFloat();
	}

	//------------------------------------------------------------------------------------------------
	//! The global answer requirement C wants - per-port variation is out of scope. Ports are few, so
	//! this is cheap once per six-hour window.
	//! \return Fraction of the map's ports whose area the resistance holds, in [0, 1].
	protected float ResolveControlledPortFraction()
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
			return 0;

		array<RplId> ports = economy.GetAllPorts();
		if (!ports || ports.Count() == 0)
			return 0;

		float total = ports.Count();
		float controlled = 0;

		foreach (RplId portId : ports)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(portId));
			if (!rpl)
				continue;

			IEntity port = rpl.GetEntity();
			if (!port)
				continue;

			if (economy.ResistanceControlsNearestPort(port.GetOrigin()))
				controlled = controlled + 1;
		}

		return controlled / total;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The difficulty's volatility, or 1 when no config has resolved yet.
	protected float ResolvePriceVolatility()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return 1.0;

		return config.GetResourcePriceVolatility();
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the live clock and asserts the drift latch to its hour.
	//! \return True when the clock was readable and the latch was written.
	protected bool AssertHourLatchFromClock()
	{
		TimeContainer time = ResolveGameTime();
		if (!time)
			return false;

		if (time.m_iHours < 0)
			return false;

		m_iHourPricesDrifted = time.m_iHours;

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
	//! Server: apply one drifted price locally and publish it to every machine.
	//!
	//! The direct call is what the listen host applies; the Rpc() beside it is what everyone else gets.
	//! ⚠ THE Rpc() LINE SITS DIRECTLY UNDER A COMPILER-CHECKED CALL TO THE SAME HANDLER WITH THE SAME
	//! ARGUMENTS (BUG-090: Rpc() is an untyped variadic prototype). Do not hoist either half.
	//!
	//! DriftPrices() is the only caller. Applying the same value twice is a no-op, so the host is safe
	//! whether or not the engine loops a broadcast back to its sender.
	//! \param[in] index Definition index.
	//! \param[in] price The stored (pre-multiplier) price.
	void BroadcastPrice(int index, int price)
	{
		if (!Replication.IsServer())
			return;

		RpcDo_SetPrice(index, price);
		Rpc(RpcDo_SetPrice, index, price);
	}

	//------------------------------------------------------------------------------------------------
	//! Everyone: one price changed. The index is the definition index - resources.conf is a mod file
	//! and identical on every machine, so an index means the same thing on both ends.
	//! \param[in] resIndex Definition index.
	//! \param[in] price The stored (pre-multiplier) price. Floored at 1.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetPrice(int resIndex, int price)
	{
		SetStoredPrice(resIndex, price);
	}

	//-----------------------------------------------------------------------
	// JIP
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The connect-time price table. The game mode's RplComponent is Streamable Disabled, so this is
	//! the whole of a joining client's price state; RpcDo_SetPrice covers every drift after it.
	//! \param[in] writer The JIP stream.
	//! \return True - the payload is self-describing and cannot fail to write.
	override bool RplSave(ScriptBitWriter writer)
	{
		writer.WriteInt(m_aCurrentPrice.Count());

		for (int i = 0; i < m_aCurrentPrice.Count(); i++)
		{
			writer.WriteInt(m_aCurrentPrice[i]);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! EVERY int is read whatever the local catalogue holds - components share one JIP stream, so a
	//! short read here would desync every component serialized after this one. A price the local
	//! catalogue has no slot for is read and dropped by SetStoredPrice.
	//! \param[in] reader The JIP stream.
	//! \return False on a truncated payload, which fails the join loudly.
	override bool RplLoad(ScriptBitReader reader)
	{
		int count;
		if (!reader.ReadInt(count)) return false;

		if (count != m_aCurrentPrice.Count())
		{
			Print(string.Format("[Overthrow] JIP resource price count mismatch: the server sent %1 prices, this machine's catalogue holds %2 - check for a resources.conf mismatch between client and server", count.ToString(), m_aCurrentPrice.Count().ToString()), LogLevel.WARNING);
		}

		int price;
		for (int i = 0; i < count; i++)
		{
			if (!reader.ReadInt(price)) return false;

			SetStoredPrice(i, price);
		}

		return true;
	}

	//-----------------------------------------------------------------------
	// PILES
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: puts a dropped load on the ground, merging into a nearby pile when there is one.
	//!
	//! The query object is created PER CALL and never shared - OVT_InventoryManagerComponent's static
	//! accumulator (:497) is the concurrency defect this project keeps re-learning.
	//!
	//! The merge decision is taken on SQUARED distances because vector.Distance is not correctly
	//! rounded and an exact-boundary decision taken on it is a coin flip.
	//! \param[in] pos Where the load was dropped. The spawn height is snapped to the terrain.
	//! \param[in] amounts What was dropped.
	//! \return The pile that now holds the load, or null when nothing could hold it - in which case
	//! NOTHING was added anywhere and the caller still owns the goods.
	IEntity SpawnOrMergePile(vector pos, notnull array<ref OVT_ResourceAmount> amounts)
	{
		if (!Replication.IsServer())
			return null;

		array<IEntity> nearby = {};
		OVT_ResourcePileQuery query = new OVT_ResourcePileQuery();
		query.Run(pos, m_fMergeRadius, nearby);

		array<IEntity> candidates = {};
		array<float> sqDistances = {};
		array<int> litres = {};

		foreach (IEntity pile : nearby)
		{
			OVT_ResourceStoreComponent pileStore = OVT_ResourceUtils.GetStore(pile);
			if (!pileStore)
				continue;

			candidates.Insert(pile);
			sqDistances.Insert(vector.DistanceSq(pos, pile.GetOrigin()));
			litres.Insert(pileStore.GetUsedLitres());
		}

		int target = OVT_ResourceRules.SelectMergeTarget(sqDistances, litres, m_fMergeRadius * m_fMergeRadius);

		IEntity holder;
		if (target == -1)
		{
			holder = SpawnPile(pos);
			if (!holder)
				return null;
		}
		else
		{
			holder = candidates[target];
		}

		OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(holder);
		if (!store)
		{
			Print("[Overthrow] OVT_ResourceManagerComponent.SpawnOrMergePile() reached a pile with no resource store. The dropped load has NOT been placed.", LogLevel.ERROR);
			return null;
		}

		OVT_ResourceLedger ledger = store.GetLedger();
		if (!ledger)
		{
			Print("[Overthrow] OVT_ResourceManagerComponent.SpawnOrMergePile() reached a pile with no ledger. The dropped load has NOT been placed.", LogLevel.ERROR);
			return null;
		}

		foreach (OVT_ResourceAmount amount : amounts)
		{
			if (!amount)
				continue;

			int fitted = ledger.Add(amount.m_sId, amount.m_iQuantity, m_Defs, store.GetCapacityLitres());
			if (fitted < amount.m_iQuantity)
				Print(string.Format("[Overthrow] A crate pile took only %1 of the %2 '%3' dropped on it. A pile is authored at unlimited capacity, so this means the prefab has been retuned.", fitted.ToString(), amount.m_iQuantity.ToString(), amount.m_sId), LogLevel.ERROR);
		}

		store.PublishContents();

		return holder;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: deletes a crate pile whose ledger has reached zero.
	//!
	//! Untrack FIRST with keepData false - deleting a tracked entity without releasing it leaves the
	//! save holding a record for a pile that no longer exists.
	//! \param[in] pile The candidate. Anything that is not an empty crate pile is left alone.
	//! \return True when the pile was deleted, so the caller knows not to publish its contents.
	bool DeletePileIfEmpty(IEntity pile)
	{
		if (!Replication.IsServer())
			return false;

		if (!pile || !OVT_ResourceUtils.IsPile(pile))
			return false;

		OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(pile);
		if (!store)
			return false;

		OVT_ResourceLedger ledger = store.GetLedger();
		if (!ledger || ledger.LineCount() > 0)
			return false;

		OVT_PersistenceTracking.Untrack(pile, false);
		SCR_EntityHelper.DeleteEntityAndChildren(pile);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns one crate pile, snapped to the terrain and registered with the persistence system.
	//! \param[in] pos Where to put it. Only the height is adjusted.
	//! \return The pile, or null when the prefab is unwired or failed to load.
	protected IEntity SpawnPile(vector pos)
	{
		if (m_rPilePrefab == "")
		{
			Print("[Overthrow] OVT_ResourceManagerComponent has no pile prefab wired on the game mode, so a dropped load has nowhere to land.", LogLevel.ERROR);
			return null;
		}

		vector spawnPos = pos;

		BaseWorld world = GetGame().GetWorld();
		if (world)
			spawnPos[1] = world.GetSurfaceY(spawnPos[0], spawnPos[2]);

		IEntity pile = OVT_WorldUtils.SpawnEntityPrefab(m_rPilePrefab, spawnPos);
		if (!pile)
		{
			Print("[Overthrow] OVT_ResourceManagerComponent could not spawn the crate pile prefab " + m_rPilePrefab, LogLevel.ERROR);
			return null;
		}

		// A pile carries no native Persistence component, so this is the only route by which it is ever
		// written to a save.
		OVT_PersistenceTracking.Track(pile);

		return pile;
	}

	//-----------------------------------------------------------------------
	// GETTERS
	//-----------------------------------------------------------------------

	//! \return The pure definition table. Never null.
	OVT_ResourceDefs GetDefs()
	{
		return m_Defs;
	}

	//! \return The loaded catalogue, or null when the config failed to load.
	OVT_ResourcesConfig GetResourcesConfig()
	{
		return m_ResourcesConfig;
	}

	//! \return Metres a construction site reaches for piles.
	float GetSupplyRadius()
	{
		return m_fSupplyRadius;
	}

	//! \return Metres within which a dropped load merges into an existing pile.
	float GetMergeRadius()
	{
		return m_fMergeRadius;
	}

	//! \return Sell price as a fraction of the live buy price.
	float GetSellRatio()
	{
		return m_fSellRatio;
	}

	//! \return The crate pile prefab.
	ResourceName GetPilePrefab()
	{
		return m_rPilePrefab;
	}

	//! \return The generic construction site prefab.
	ResourceName GetDefaultSitePrefab()
	{
		return m_rDefaultSitePrefab;
	}
}
