//------------------------------------------------------------------------------------------------
//! One persisted resource line: a stable resource id and how many units are held.
//!
//! FROZEN, AND DECLARED HERE ON PURPOSE. The binary container writes the concrete CLASS NAME into
//! every save as a `$type` discriminator and creates the instance from it on load, so the class name
//! and the field order are both the save format - a new field may only ever be APPENDED. The
//! serializer that reads and writes it references this declaration; it must not re-declare it.
//!
//! Deliberately not OVT_ResourceAmount, whose shape the wire and the UI still get to change.
//------------------------------------------------------------------------------------------------
class OVT_PersistedResourceLine
{
	string resourceId;
	int quantity;
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Overthrow/Components", description: "Volume-capped resource store for one holder: a truck, a crate pile or a warehouse")]
class OVT_ResourceStoreComponentClass : OVT_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! One resource holder: a ledger of units, a litre cap and a display name.
//!
//! Authored on three prefab families (the same-GUID delta of vanilla Wheeled_Truck_Base.et, the
//! crate-pile prop and the same-GUID delta of vanilla Warehouse_01_Base.et) so every truck any system
//! spawns can haul. There is no runtime component creation in EnforceScript, so prefab authoring is
//! the only seam there is.
//!
//! THE CONTENTS REPLICATE. A resource ledger is at most a handful of lines, so the whole thing rides
//! ONE packed RplProp string: the cargo HUD, the pile label, the map rows and a requirement readout
//! are all local reads with no pull protocol and no async first frame. m_Ledger is server truth; on a
//! proxy it is a MIRROR rebuilt from m_sPacked.
//!
//! CAPACITY IS NOT REPLICATED. It is a prefab attribute converted once in OnPostInit, so every machine
//! reads the same number out of the same file - unlike OVT_StorageComponent, whose AUTO mode resolved
//! from a server-built catalogue and therefore had to replicate the result. There is no deferred
//! resolve here and so no capacity-0-on-the-spawn-frame window.
//!
//! VOLUME IS INTEGER LITRES throughout. The [Attribute] is a friendly float in m3 and is converted
//! exactly once; every comparison after that is an exact integer comparison.
//------------------------------------------------------------------------------------------------
class OVT_ResourceStoreComponent : OVT_Component
{
	//! Capacity value meaning "no cap" - a pile or a warehouse. Matches OVT_ResourceLedger's contract.
	static const int UNLIMITED_CAPACITY = -1;

	//! Capacity 0 means the holder has NO resource store at all, not a full one. Authored on the truck
	//! base so a tanker, ammo, repair, arsenal, command or engineer variant inherits no cargo hold -
	//! only the four transport/mobile-FOB deltas set a positive volume. OVT_ResourceUtils.GetStore()
	//! returns null for an inert store, so every consumer refuses without a per-caller check.
	static const int NO_CAPACITY = 0;

	//! The one place m3 becomes litres.
	static const int LITRES_PER_CUBIC_METRE = 1000;

	//-----------------------------------------------------------------------------------------------
	// ATTRIBUTES
	//-----------------------------------------------------------------------------------------------

	[Attribute(defvalue: "-1", desc: "Cargo volume in cubic metres. -1 means unlimited (a pile, a warehouse). 0 means this holder has no resource store at all.")]
	protected float m_fCargoVolume;

	[Attribute("", desc: "Localization key used as the display name when the prefab carries no UIInfo of its own")]
	protected string m_sDefaultNameKey;

	//-----------------------------------------------------------------------------------------------
	// MEMBER VARIABLES
	//-----------------------------------------------------------------------------------------------

	//! Server truth. On a proxy this is a mirror of m_sPacked and is never mutated locally.
	protected ref OVT_ResourceLedger m_Ledger;

	//! The ENTIRE replicated surface. Written only by PublishContents().
	[RplProp(onRplName: "OnContentsChanged")]
	protected string m_sPacked;

	//! Litres, or UNLIMITED_CAPACITY. Written ONLY in OnPostInit - it is prefab data, not state.
	protected int m_iCapacityLitres;

	//! (OVT_ResourceStoreComponent holder). Fires on the authority from PublishContents() and on a
	//! proxy from the RplProp callback, so a listen host and a remote client behave the same.
	protected ref ScriptInvoker m_OnContentsChanged;

	//! The packed string the mirror currently reflects. A proxy rebuilds when it drifts from m_sPacked,
	//! so a stream-in that arrives without firing the callback still lands.
	protected string m_sMirrorSource;

	//! Latch for EnsureTracked(), so the persistence lookup happens once per holder.
	protected bool m_bTrackingEnsured;

	//! Memoised fallback name. Immutable for the life of the entity.
	protected string m_sResolvedDefaultName;

	protected bool m_bDefaultNameResolved;

	//-----------------------------------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Creates the ledger, converts the authored volume to litres once, and asserts the holder is
	//! replicated.
	//! \param[in] owner The holder entity.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		// A throwaway ItemPreview icon (the real-estate screen renders the warehouse this way): no
		// ledger and no RplComponent complaint.
		if (IsPreviewInstance(owner))
			return;

		m_iCapacityLitres = ResolveCapacityLitres();

		// An inert store builds no ledger and warns about no RplComponent: it is not a holder.
		if (m_iCapacityLitres == NO_CAPACITY)
			return;

		m_Ledger = new OVT_ResourceLedger();

		// BUG-193 is exactly this failure found late: without an RplComponent the holder has no RplId,
		// so no client can ever name it and its contents can never replicate.
		if (!GetRpl())
			Print(string.Format("[Overthrow] OVT_ResourceStoreComponent on '%1' has no RplComponent. The holder cannot be named across the network and its contents will never reach a client.", OVT_PrefabUtils.GetPrefabName(owner)), LogLevel.ERROR);

		// A BUILDING holder must be tracked BEFORE it holds anything, or its record can never be
		// matched back to it - the map-placed warehouse deadlock, see OVT_StorageComponent's
		// EnsureTracked() header. A truck or a pile is tracked by another route and is NOT touched
		// here: at OnPostInit its own registration may not have landed yet, and tracking it a second
		// time from here would be guessing at a lifetime this component does not own.
		if (Replication.IsServer() && Building.Cast(owner))
			EnsureTracked();
	}

	//-----------------------------------------------------------------------------------------------
	// SERVER API
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Republishes the packed contents from the ledger and bumps ONCE.
	//!
	//! The only writer of m_sPacked and the only Replication.BumpMe() call site in this component.
	//! Called once per finished request, never per line: a per-line bump would replace one network
	//! spike with another. It must also be called by hand after OVT_ResourceLedger.Clear(), which
	//! deliberately fires no change event.
	void PublishContents()
	{
		if (!m_Ledger)
			return;

		m_sPacked = OVT_ResourcePack.Encode(m_Ledger, ResolveDefs());
		m_sMirrorSource = m_sPacked;

		Replication.BumpMe();

		if (m_Ledger.LineCount() > 0)
			EnsureTracked();

		if (m_OnContentsChanged)
			m_OnContentsChanged.Invoke(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the whole ledger from a save payload. Server only.
	//!
	//! Lines are added at UNLIMITED capacity deliberately. Capacity is prefab data, so clamping here
	//! would silently eat a player's stock every time a truck's cargo volume was retuned. Over-cap on
	//! load is the strictly better failure.
	//! \param[in] lines The persisted lines; null or empty leaves the holder empty.
	void ApplyPersisted(array<ref OVT_PersistedResourceLine> lines)
	{
		if (!m_Ledger)
			m_Ledger = new OVT_ResourceLedger();

		m_Ledger.Clear();

		OVT_ResourceDefs defs = ResolveDefs();

		if (lines && lines.Count() > 0 && !defs)
			Print("[Overthrow] OVT_ResourceStoreComponent.ApplyPersisted() ran before the resource catalogue existed, so the saved stock could not be restored.", LogLevel.ERROR);

		if (lines && defs)
		{
			foreach (OVT_PersistedResourceLine line : lines)
			{
				if (!line)
					continue;

				m_Ledger.Add(line.resourceId, line.quantity, defs, UNLIMITED_CAPACITY);
			}
		}

		// Clear() deliberately fires no change event, so the republish is not optional.
		PublishContents();
	}

	//-----------------------------------------------------------------------------------------------
	// CLIENT-SAFE GETTERS
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The ledger. Server truth on the authority; on a proxy a mirror of the replicated contents,
	//! rebuilt here if it has drifted.
	//! \return The ledger, or null on a worldless preview instance.
	OVT_ResourceLedger GetLedger()
	{
		EnsureMirror();

		return m_Ledger;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Litres this holder may hold, or -1 when it is unlimited.
	int GetCapacityLitres()
	{
		return m_iCapacityLitres;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Litres currently held. Correct within one replication tick on every machine.
	int GetUsedLitres()
	{
		EnsureMirror();

		if (!m_Ledger)
			return 0;

		return m_Ledger.TotalLitres(ResolveDefs());
	}

	//------------------------------------------------------------------------------------------------
	//! \return Litres still free; int.MAX when the holder is unlimited.
	int GetFreeLitres()
	{
		EnsureMirror();

		if (!m_Ledger)
			return 0;

		return m_Ledger.FreeLitres(ResolveDefs(), m_iCapacityLitres);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The replicated packed contents, for a consumer that wants the raw string.
	string GetPackedContents()
	{
		return m_sPacked;
	}

	//------------------------------------------------------------------------------------------------
	//! The name to show: the prefab's own UIInfo, else the authored default key, else the prefab file
	//! stem. Everything is memoised - it cannot change for the life of the entity.
	//! \return A non-empty name, possibly a localization key.
	string GetDisplayName()
	{
		if (!m_bDefaultNameResolved)
		{
			m_bDefaultNameResolved = true;
			m_sResolvedDefaultName = ResolveDefaultName();
		}

		return m_sResolvedDefaultName;
	}

	//------------------------------------------------------------------------------------------------
	//! Fires when the contents change, on the authority and on proxies alike.
	//! \return The invoker, allocated on first ask.
	ScriptInvoker GetOnContentsChanged()
	{
		if (!m_OnContentsChanged)
			m_OnContentsChanged = new ScriptInvoker();

		return m_OnContentsChanged;
	}

	//-----------------------------------------------------------------------------------------------
	// PROTECTED
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Proxy-side RplProp callback. The authority never gets here, which is why PublishContents()
	//! invokes the same invoker itself.
	protected void OnContentsChanged()
	{
		RebuildMirror();

		if (m_OnContentsChanged)
			m_OnContentsChanged.Invoke(this);
	}

	//------------------------------------------------------------------------------------------------
	//! The authored cubic metres as integer litres. The ONE float-to-int conversion in this feature's
	//! capacity path; the sentinel is tested after the conversion so no capacity comparison is a float
	//! comparison.
	//! \return Litres, or UNLIMITED_CAPACITY.
	//! \return True when this component is authored as "no store" and must be treated as absent.
	bool IsInert()
	{
		return m_iCapacityLitres == NO_CAPACITY;
	}

	//------------------------------------------------------------------------------------------------
	protected int ResolveCapacityLitres()
	{
		int litres = Math.Round(m_fCargoVolume * LITRES_PER_CUBIC_METRE);

		if (litres < 0)
			return UNLIMITED_CAPACITY;

		return litres;
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the proxy mirror when the replicated string has moved on. The authority is excluded
	//! outright: its ledger is the truth the packed string is derived FROM, and decoding back into it
	//! could only lose a line the definition table no longer knows.
	protected void EnsureMirror()
	{
		if (Replication.IsServer())
			return;

		if (m_sMirrorSource == m_sPacked)
			return;

		RebuildMirror();
	}

	//------------------------------------------------------------------------------------------------
	//! Decodes m_sPacked into the mirror. A failure leaves m_sMirrorSource stale on purpose, so the
	//! next read retries once the catalogue exists.
	protected void RebuildMirror()
	{
		if (!m_Ledger)
			m_Ledger = new OVT_ResourceLedger();

		OVT_ResourceDefs defs = ResolveDefs();
		if (!defs)
			return;

		if (!OVT_ResourcePack.Decode(m_sPacked, defs, m_Ledger))
		{
			Print(string.Format("[Overthrow] OVT_ResourceStoreComponent could not decode the replicated contents '%1'", m_sPacked), LogLevel.ERROR);
			return;
		}

		m_sMirrorSource = m_sPacked;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The shared definition table, or null before the manager has built it.
	protected OVT_ResourceDefs ResolveDefs()
	{
		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
			return null;

		return resources.GetDefs();
	}

	//------------------------------------------------------------------------------------------------
	//! Makes a BUILDING holder known to the persistence system the first time it holds something.
	//! Server only, latched, never undone.
	//!
	//! Vanilla registers an intact building with the persistence system NEVER - only a destroyed one -
	//! so without this a warehouse's resource stock has no record to be written into. Vehicles carry
	//! vanilla's own Persistence component and a pile is tracked where it is spawned, so both latch out
	//! on the IsTracked check and never reach a second lookup.
	protected void EnsureTracked()
	{
		if (m_bTrackingEnsured)
			return;

		if (!Replication.IsServer())
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		if (OVT_PersistenceTracking.IsTracked(owner))
		{
			m_bTrackingEnsured = true;
			return;
		}

		// lazy=false, the vanilla building call site's own choice (SCR_DestructibleBuildingComponent
		// :1339): a world entity that is already live, not one being spawned.
		if (OVT_PersistenceTracking.Track(owner))
			m_bTrackingEnsured = true;
	}

	//------------------------------------------------------------------------------------------------
	//! The display-name chain. Called at most once per instance.
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
