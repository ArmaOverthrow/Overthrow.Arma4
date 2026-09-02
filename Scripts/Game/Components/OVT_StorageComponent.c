//------------------------------------------------------------------------------------------------
//! How a holder's item capacity is decided.
//------------------------------------------------------------------------------------------------
enum EOVT_StorageCapacityMode
{
	//! Derived from the economy's vehicle catalogue: truck -> unlimited, registered legal vehicle ->
	//! m_iAutoVehicleCapacity, illegal/armed/unknown vehicle -> none, anything that is not a vehicle ->
	//! unlimited. The default, so a shared vehicle base authors nothing but the component itself.
	AUTO,

	//! No cap.
	UNLIMITED,

	//! m_iFixedCapacity, read nowhere else.
	FIXED,

	//! No storage at all - the holder is not a holder.
	NONE
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Overthrow/Components", description: "Item ledger for one storage holder: a vehicle, an ammo box or a warehouse building")]
class OVT_StorageComponentClass : OVT_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! One storage holder: an item ledger, a resolved capacity and a display name.
//!
//! Authored on three shared prefab bases (Wheeled_Base.et, OVT_AmmoBox_Base.et and the same-GUID
//! delta of vanilla Warehouse_01_Base.et) so every wheeled vehicle, ammo box and warehouse variant
//! carries one. There is no runtime component creation in EnforceScript, so prefab authoring is the
//! only seam there is.
//!
//! THE CONTENTS NEVER LEAVE THE SERVER. m_Ledger is server-side state: it exists on a client (so no
//! caller has to null-check it) but is always empty there and is never read there. Only the three
//! RplProps below travel, and they are free for join-in-progress because RplProp carries streamed-in
//! state.
//!
//! CAPACITY IS RESOLVED ONCE, ON THE SERVER, AND REPLICATED (D7). AUTO resolution reads the economy's
//! vehicle catalogue, which is built during server init - whether a client's copy is populated, and
//! populated at the same moment, is an assumption worth not making.
//------------------------------------------------------------------------------------------------
class OVT_StorageComponent : OVT_Component
{
	//! Capacity value meaning "no cap". Matches OVT_StorageLedger's capacity contract.
	static const int UNLIMITED_CAPACITY = -1;

	//! Capacity value meaning "this holder holds nothing". Also the value before a resolve lands, so
	//! an unresolved holder fails closed rather than open.
	static const int NO_CAPACITY = 0;

	//! Gap between AUTO resolve attempts while the economy catalogue is still being built.
	static const int RESOLVE_RETRY_MS = 1000;

	//! Retries before AUTO resolution answers with what it has and logs.
	static const int MAX_RESOLVE_ATTEMPTS = 10;

	//-----------------------------------------------------------------------------------------------
	// ATTRIBUTES
	//-----------------------------------------------------------------------------------------------

	[Attribute("0", UIWidgets.ComboBox, "How this holder's capacity is decided", "", ParamEnumArray.FromEnum(EOVT_StorageCapacityMode))]
	protected EOVT_StorageCapacityMode m_eCapacityMode;

	[Attribute("0", desc: "Item capacity when the mode is FIXED. Read in no other mode.")]
	protected int m_iFixedCapacity;

	[Attribute("300", desc: "Item capacity an AUTO-resolved registered, legal, non-truck vehicle gets")]
	protected int m_iAutoVehicleCapacity;

	[Attribute("100", desc: "Item capacity an AUTO-resolved registered but illegal or armed vehicle gets")]
	protected int m_iArmedVehicleCapacity;

	[Attribute("", desc: "Localization key used as the display name when the prefab carries no UIInfo of its own")]
	protected string m_sDefaultNameKey;

	//-----------------------------------------------------------------------------------------------
	// MEMBER VARIABLES
	//-----------------------------------------------------------------------------------------------

	//! Server-side content. Non-null on clients, always empty there, never read there.
	protected ref OVT_StorageLedger m_Ledger;

	//! The label's number and the live-refresh trigger. Written only by PublishCount().
	[RplProp(onRplName: "OnCountChanged")]
	protected int m_iTotalCount;

	//! Empty means "use the resolved default name".
	[RplProp()]
	protected string m_sCustomName;

	//! The RESOLVED capacity: -1 unlimited, 0 none, otherwise an item cap. No client ever re-derives it.
	[RplProp()]
	protected int m_iCapacity;

	//! (OVT_StorageComponent holder). Fires on the authority from PublishCount() and on a proxy from
	//! the RplProp callback, so a listen host and a remote client behave the same.
	protected ref ScriptInvoker m_OnCountChanged;

	protected bool m_bCapacityResolved;

	protected int m_iResolveAttempts;

	//! Latch for EnsureTracked(), so the persistence lookup happens once per holder and not once per
	//! finished job.
	protected bool m_bTrackingEnsured;

	//! Memoised fallback name (everything in the chain below the custom name). Immutable for the life
	//! of the entity, so it is resolved at most once.
	protected string m_sResolvedDefaultName;

	protected bool m_bDefaultNameResolved;

	//! Prefabs already reported as vehicles the economy does not know. The ERROR is once per prefab,
	//! not once per instance: an unregistered truck spawns in numbers.
	protected static ref array<ResourceName> s_aUnregisteredReported;

	//-----------------------------------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Creates the ledger, asserts the holder is replicated, and queues the capacity resolve.
	//! \param[in] owner The holder entity.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		// A throwaway ItemPreview icon (the real-estate screen renders the warehouse this way): no
		// ledger, no RplComponent complaint and no resolve.
		if (IsPreviewInstance(owner))
			return;

		m_Ledger = new OVT_StorageLedger();

		// BUG-193 is exactly this failure found late: without an RplComponent the holder has no RplId,
		// so no client can ever name it and every request against it is unroutable.
		if (!GetRpl())
			Print(string.Format("[Overthrow] OVT_StorageComponent on '%1' has no RplComponent. The holder cannot be named across the network, so nobody can open it.", OVT_PrefabUtils.GetPrefabName(owner)), LogLevel.ERROR);

		if (!Replication.IsServer())
			return;

		if (!GetGame() || !GetGame().GetCallqueue())
			return;

		GetGame().GetCallqueue().CallLater(TryResolveCapacity, 0, false);

		// A BUILDING holder must be tracked BEFORE it holds anything - see EnsureTracked()'s header.
		EnsureTracked();
	}

	//------------------------------------------------------------------------------------------------
	//! Cancels a pending resolve so a holder destroyed in its first frames never calls back into a
	//! deleted component.
	//! \param[in] owner The holder entity.
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(TryResolveCapacity);

		super.OnDelete(owner);
	}

	//-----------------------------------------------------------------------------------------------
	// SERVER API
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The item ledger. Server-side content; a client gets an empty one and must not read it.
	//! \return The ledger, never null after OnPostInit.
	OVT_StorageLedger GetLedger()
	{
		return m_Ledger;
	}

	//------------------------------------------------------------------------------------------------
	//! Republishes the replicated count from the ledger and bumps ONCE.
	//!
	//! The only writer of m_iTotalCount. Called at batch boundaries - once per holder per finished job -
	//! never per item: a per-item bump would replace one network spike with another. It must also be
	//! called by hand after OVT_StorageLedger.Clear(), which deliberately fires no change event.
	//!
	//! It is also where a BUILDING holder joins the save point - see EnsureTracked().
	void PublishCount()
	{
		if (!m_Ledger)
			return;

		m_iTotalCount = m_Ledger.Total();
		Replication.BumpMe();

		if (m_iTotalCount > 0)
			EnsureTracked();

		if (m_OnCountChanged)
			m_OnCountChanged.Invoke(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the player-facing name. Server only; permission is the request component's gate, not this
	//! setter's.
	//! \param[in] name The new name, or an empty string to fall back to the resolved default.
	void SetCustomName(string name)
	{
		if (m_sCustomName == name)
			return;

		m_sCustomName = name;
		Replication.BumpMe();

		if (name != "")
			EnsureTracked();
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the whole ledger and the custom name from a save payload. Server only.
	//!
	//! Lines are added at UNLIMITED capacity deliberately. Capacity is not persisted (D8) and may not
	//! even be resolved yet when a save is applied, so clamping here would silently eat a player's
	//! stock every time a prefab or a price config was retuned.
	//! \param[in] customName The persisted name, possibly empty.
	//! \param[in] lines The persisted ledger lines; null or empty leaves the holder empty.
	void ApplyPersisted(string customName, array<ref OVT_StorageLine> lines)
	{
		if (!m_Ledger)
			m_Ledger = new OVT_StorageLedger();

		m_Ledger.Clear();

		if (lines)
		{
			foreach (OVT_StorageLine line : lines)
			{
				if (!line)
					continue;

				// Folds a pre-existing dirty-variant line into its clean stack. Add() merges by key, so
				// a save holding both ends up with one line and no count is lost.
				m_Ledger.Add(OVT_PrefabUtils.ResolveCleanVariant(line.m_sRes), line.m_iCount, UNLIMITED_CAPACITY);
			}
		}

		m_sCustomName = customName;

		// Clear() deliberately fires no change event, so the republish is not optional: without it a
		// holder loaded as empty would keep broadcasting its pre-load count.
		PublishCount();
	}

	//-----------------------------------------------------------------------------------------------
	// CLIENT-SAFE GETTERS
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \return The replicated item count. Correct within one replication tick on every machine.
	int GetTotalCount()
	{
		return m_iTotalCount;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The resolved capacity: -1 unlimited, 0 none, otherwise an item cap.
	int GetCapacity()
	{
		return m_iCapacity;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The authored capacity mode.
	EOVT_StorageCapacityMode GetCapacityMode()
	{
		return m_eCapacityMode;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the server has finished deciding this holder's capacity. Server-side state - a client
	//! reads GetCapacity() and nothing else.
	//! \return True once a capacity has been applied.
	bool IsCapacityResolved()
	{
		return m_bCapacityResolved;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The player-set name, or an empty string when there is none.
	string GetCustomName()
	{
		return m_sCustomName;
	}

	//------------------------------------------------------------------------------------------------
	//! The name to show: custom name, else the prefab's own UIInfo, else the authored default key,
	//! else the prefab file stem. Everything below the custom name is memoised.
	//! \return A non-empty name, possibly a localization key.
	string GetDisplayName()
	{
		if (m_sCustomName != "")
			return m_sCustomName;

		if (!m_bDefaultNameResolved)
		{
			m_bDefaultNameResolved = true;
			m_sResolvedDefaultName = ResolveDefaultName();
		}

		return m_sResolvedDefaultName;
	}

	//------------------------------------------------------------------------------------------------
	//! Fires when the replicated count changes, on the authority and on proxies alike.
	//! \return The invoker, allocated on first ask.
	ScriptInvoker GetOnCountChanged()
	{
		if (!m_OnCountChanged)
			m_OnCountChanged = new ScriptInvoker();

		return m_OnCountChanged;
	}

	//-----------------------------------------------------------------------------------------------
	// PROTECTED
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Proxy-side RplProp callback. The authority never gets here, which is why PublishCount() invokes
	//! the same invoker itself.
	protected void OnCountChanged()
	{
		if (m_OnCountChanged)
			m_OnCountChanged.Invoke(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Makes a BUILDING holder known to the persistence system. Server only, latched, and never undone -
	//! nothing here ever untracks.
	//!
	//! Vanilla registers an intact building with the persistence system NEVER; it registers one only
	//! when it is destroyed (SCR_DestructibleBuildingComponent.GoToDestroyedState). So without this a
	//! warehouse's ledger has no record to be written into, and the serializer bound to the vanilla
	//! Building configuration would simply never run.
	//!
	//! ⚠ CALLED AT OnPostInit, NOT ON FIRST CONTENT. It used to run only when the ledger became
	//! non-empty or a name was set, and that is a DEADLOCK for the one holder class that is neither
	//! spawned nor self-spawned. A vehicle carries vanilla's own Persistence component and registers
	//! itself; a placed box and a BUILT warehouse have SelfSpawn and are re-created from their record.
	//! A map-placed warehouse building is already standing at load and must have its record MATCHED to
	//! it - which cannot happen if it only registers once it has content, because its content is what
	//! the record was going to supply. Every other storage holder survived a restart; this one did not,
	//! and this is why.
	//!
	//! Tracking an empty warehouse costs one small record per warehouse building on the map. It is not
	//! an orphan (the BUG-118 shape): the same building re-registers under the same deterministic id
	//! next boot and claims it.
	//!
	//! Vehicles and placed boxes latch out on the class test or the IsTracked check and never reach the
	//! lookup, so nothing else on the map gains a record from this.
	protected void EnsureTracked()
	{
		if (m_bTrackingEnsured)
			return;

		if (!Replication.IsServer())
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		if (!Building.Cast(owner))
		{
			m_bTrackingEnsured = true;
			return;
		}

		if (OVT_PersistenceTracking.IsTracked(owner))
		{
			m_bTrackingEnsured = true;
			return;
		}

		// lazy=false, the vanilla building call site's own choice (SCR_DestructibleBuildingComponent
		// :1339): the owner is a world entity that is already live, not one being spawned.
		if (OVT_PersistenceTracking.Track(owner))
			m_bTrackingEnsured = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Decides this holder's capacity, server-side, retrying while the economy catalogue is still
	//! being built.
	//!
	//! Deferred out of OnPostInit because BuildResourceDatabase() runs on a call-queue hop of its own
	//! after the economy manager's Init: a vehicle placed by a world layer post-inits first and would
	//! read an empty catalogue, i.e. "unregistered", i.e. no storage.
	protected void TryResolveCapacity()
	{
		if (m_bCapacityResolved)
			return;

		if (!Replication.IsServer())
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		if (m_eCapacityMode != EOVT_StorageCapacityMode.AUTO)
		{
			ApplyCapacity(ResolveAuthoredCapacity());
			return;
		}

		// Vehicle-ness comes from the entity class, never from the economy: an unregistered prefab
		// resolves to inventory id 0 - some other item's identity - so asking the economy would call an
		// unknown truck "not a vehicle" and hand it unlimited storage.
		bool isVehicle = (Vehicle.Cast(owner) != null);
		if (!isVehicle)
		{
			ApplyCapacity(OVT_StorageRules.ResolveAutoCapacity(false, false, false, OVT_ParkingType.PARKING_CAR, m_iAutoVehicleCapacity, m_iArmedVehicleCapacity));
			return;
		}

		ResourceName prefab = OVT_PrefabUtils.GetPrefabName(owner);

		// A vehicle variant in no catalogue takes its legality and parking from the registered
		// prefab it inherits.
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		bool registered = false;
		if (economy)
		{
			ResourceName pricing = economy.ResolvePricingResource(prefab);
			registered = !pricing.IsEmpty();
			if (registered)
				prefab = pricing;
		}

		if (!registered && m_iResolveAttempts < MAX_RESOLVE_ATTEMPTS)
		{
			m_iResolveAttempts = m_iResolveAttempts + 1;

			if (GetGame() && GetGame().GetCallqueue())
				GetGame().GetCallqueue().CallLater(TryResolveCapacity, RESOLVE_RETRY_MS, false);

			return;
		}

		bool isLegal = false;
		OVT_ParkingType parking = OVT_ParkingType.PARKING_CAR;

		if (registered)
		{
			int id = economy.GetInventoryId(prefab);
			isLegal = economy.IsLegalVehicle(id);
			parking = economy.GetParkingType(id);
		}
		else if (economy)
		{
			// No economy at all means a world without Overthrow, not a mis-registered prefab.
			ReportUnregisteredVehicle(prefab);
		}

		ApplyCapacity(OVT_StorageRules.ResolveAutoCapacity(true, registered, isLegal, parking, m_iAutoVehicleCapacity, m_iArmedVehicleCapacity));
	}

	//------------------------------------------------------------------------------------------------
	//! \return The capacity the non-AUTO modes ask for.
	protected int ResolveAuthoredCapacity()
	{
		if (m_eCapacityMode == EOVT_StorageCapacityMode.UNLIMITED)
			return UNLIMITED_CAPACITY;

		if (m_eCapacityMode == EOVT_StorageCapacityMode.FIXED)
			return m_iFixedCapacity;

		return NO_CAPACITY;
	}

	//------------------------------------------------------------------------------------------------
	//! Latches the resolved capacity and replicates it once.
	//! \param[in] capacity The resolved value.
	protected void ApplyCapacity(int capacity)
	{
		m_bCapacityResolved = true;
		m_iCapacity = capacity;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Logs an unknown vehicle prefab once. A silent 0 capacity is undiagnosable - the holder simply
	//! has no storage actions and nothing anywhere says why.
	//! \param[in] prefab The prefab the economy does not carry.
	protected void ReportUnregisteredVehicle(ResourceName prefab)
	{
		if (!s_aUnregisteredReported)
			s_aUnregisteredReported = new array<ResourceName>();

		if (s_aUnregisteredReported.Contains(prefab))
			return;

		s_aUnregisteredReported.Insert(prefab);

		Print(string.Format("[Overthrow] OVT_StorageComponent: '%1' is a vehicle the economy does not know, so it gets no storage. Add it to Configs/Pricing/vehiclePrices.conf or to a faction entity catalogue.", prefab), LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	//! Everything in the display-name chain below the custom name. Called at most once per instance.
	//! \return A non-empty name, possibly a localization key.
	protected string ResolveDefaultName()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return m_sDefaultNameKey;

		// Vehicles and editor-visible structures carry their name here; this is the only step that can
		// answer for a vehicle at all, since a vehicle has no InventoryItemComponent.
		SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(owner.FindComponent(SCR_EditableEntityComponent));
		if (editable)
		{
			SCR_UIInfo editableInfo = editable.GetInfo();
			if (editableInfo && editableInfo.GetName() != "")
				return editableInfo.GetName();
		}

		ResourceName prefab = OVT_PrefabUtils.GetPrefabName(owner);

		UIInfo itemInfo = OVT_PrefabUtils.GetItemUIInfo(prefab);
		if (itemInfo && itemInfo.GetName() != "")
			return itemInfo.GetName();

		if (m_sDefaultNameKey != "")
			return m_sDefaultNameKey;

		return PrefabStem(prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! Last-resort name: the prefab's file name without its GUID, path or extension.
	//! \param[in] prefab The prefab resource name.
	//! \return The stem, or an empty string when there is no prefab.
	protected string PrefabStem(ResourceName prefab)
	{
		string path = prefab;

		int brace = path.LastIndexOf("}");
		if (brace != -1)
			path = path.Substring(brace + 1, path.Length() - brace - 1);

		int slash = path.LastIndexOf("/");
		if (slash != -1)
			path = path.Substring(slash + 1, path.Length() - slash - 1);

		int dot = path.LastIndexOf(".");
		if (dot != -1)
			path = path.Substring(0, dot);

		return path;
	}
}
