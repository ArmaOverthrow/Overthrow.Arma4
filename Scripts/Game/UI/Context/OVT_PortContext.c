//------------------------------------------------------------------------------------------------
//! The port screen: Import (a priced catalogue bought into the occupied vehicle) and Export (that
//! vehicle's own ledger sold back at the port). All widget work belongs to OVT_TransferContext.
//!
//! ⚠ THE EXPORT LIST IS ASYNC, for the same reason OVT_StorageContext's is: a holder's contents
//! never replicate, they are pulled on open by the one player looking. BuildEntries fires that pull
//! latched and returns empty on the first frame; the fan's arrival calls Refresh() again. On a
//! listen host the whole fan lands INSIDE that call, i.e. BuildEntries re-entering itself - the
//! latch is cleared before the ask and m_bBuildingEntries stops the inner Refresh.
//!
//! The Import half is unchanged: same catalogue, same illegal gate, same max-100 cap, same shop
//! category mapping, one ImportToVehicle per line.
//!
//! RESOURCES ARE A CATEGORY, NOT A MODE (D9). Both shipped modes gain rows whose ids carry the
//! "res:" prefix, filed under CATEGORY_RESOURCES so GetPopulatedCategories lands them on the last
//! tab. Every line-consuming hook partitions on that prefix BEFORE it reaches GetInventoryId, which
//! maps an unknown string to id 0 - i.e. some other item's identity.
//------------------------------------------------------------------------------------------------
class OVT_PortContext : OVT_TransferContext
{
	//! OVT_VehicleRequestComponent.IMPORT_MAX_QUANTITY is protected, so the cap is mirrored here; the
	//! server rejects anything above it. It is a sanity bound on one request, NOT the real limit - what
	//! actually stops an import is the destination's free space, enforced in ValidateCart.
	protected const int IMPORT_MAX_QUANTITY = 10000;

	//! Mode ids. Import is first, so it titles the screen and is what the port opens on.
	protected const int MODE_IMPORT = 0;
	protected const int MODE_EXPORT = 1;

	//! One past OVT_ShopCategory.OTHER, so the resource tab sorts last in both modes.
	protected const int CATEGORY_RESOURCES = 9;

	//! Live price bands for the drift readout, as a fraction of base.
	protected const float DRIFT_FAR_BELOW = 0.75;
	protected const float DRIFT_BELOW = 0.95;
	protected const float DRIFT_ABOVE = 1.05;
	protected const float DRIFT_FAR_ABOVE = 1.5;

	//! The vehicle Export is selling, and its holder id. Re-resolved every build: the player can
	//! leave the vehicle with the screen open.
	protected IEntity m_ExportHolder;
	protected RplId m_ExportHolderId;

	//! The request component this screen subscribed to. Cached for OnClose - it outlives the layout.
	protected OVT_StorageRequestComponent m_SubscribedRequests;

	//! The resource request component this screen subscribed to, for the same reason.
	protected OVT_ResourceRequestComponent m_SubscribedResourceRequests;

	//! THE RESOURCE LATCH. True between an opened resource checkout and its single reply; set before
	//! the ask, because on a listen host the whole reply fan runs inside it.
	protected bool m_bResourceCheckoutPending;

	//! THE LATCH. True when the snapshot in hand (if any) is not trusted and a pull is owed.
	protected bool m_bWantPull;

	//! True while BuildEntries is running, so a fan that lands inside it does not re-enter Refresh.
	protected bool m_bBuildingEntries;

	//! True once a pull was refused, so the loading message stops instead of standing forever.
	protected bool m_bPullFailed;

	//! The refusal waiting to be drawn. Empty when there is none.
	protected string m_sPendingError;

	//! Prefab -> localized display name. UIInfo resolution loads and scans a prefab container, which is
	//! far too expensive to repeat per row per sort comparison; names never change.
	protected ref map<string, string> m_mDisplayNames = new map<string, string>();

	//------------------------------------------------------------------------------------------------
	//! An RplId member is an engine handle, not an int - it is given a value here so IsValid() is
	//! answerable before any vehicle has been resolved.
	void OVT_PortContext()
	{
		m_ExportHolderId = RplId.Invalid();
	}

	//-----------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Subscribes BEFORE super.OnShow(), because super.OnShow() calls Refresh() and on a listen host
	//! a contents fan lands inside that call.
	override void OnShow()
	{
		m_bWantPull = true;
		m_bPullFailed = false;
		m_bBuildingEntries = false;
		m_bResourceCheckoutPending = false;
		m_sPendingError = "";
		m_ExportHolder = null;
		m_ExportHolderId = RplId.Invalid();

		Subscribe();

		super.OnShow();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes exactly what OnShow inserted. The three request invokers and the pending refusal draw
	//! all outlive the layout.
	override void OnClose()
	{
		GetGame().GetCallqueue().Remove(ShowStorageError);

		Unsubscribe();

		super.OnClose();

		m_bWantPull = false;
		m_bBuildingEntries = false;
		m_bPullFailed = false;
		m_bResourceCheckoutPending = false;
		m_sPendingError = "";
	}

	//------------------------------------------------------------------------------------------------
	//! The loading message is set AFTER the base's refresh: RefreshCheckout() clears a persistent
	//! message whenever the cart validates, and an empty cart always does.
	override void Refresh()
	{
		super.Refresh();

		if(!m_bIsActive || !m_wRoot) return;
		if(m_iMode != MODE_EXPORT) return;

		// On foot there is nothing to sell and no destination, so the empty list needs a reason.
		if(!m_ExportHolder || !m_ExportHolderId.IsValid())
		{
			ShowPersistentMessage("#OVT-Transfer_NoVehicle");
			return;
		}

		if(ExportIsLoading()) ShowPersistentMessage("#OVT-Storage_Loading");
	}

	//-----------------------------------------------------------------------
	// THE EIGHT HOOKS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	override void BuildModes(out array<int> modes, out array<string> labelKeys)
	{
		modes.Clear();
		labelKeys.Clear();

		modes.Insert(MODE_IMPORT);
		labelKeys.Insert("#OVT-Import");

		modes.Insert(MODE_EXPORT);
		labelKeys.Insert("#OVT-Export");
	}

	//------------------------------------------------------------------------------------------------
	override void BuildEntries(int mode, OVT_TransferListModel model)
	{
		if(!model || !m_Economy) return;

		if(mode == MODE_EXPORT)
		{
			// Appended outside BuildExportEntries: that method returns early while the item snapshot is
			// in flight, and a replicated resource ledger has nothing to wait for.
			BuildExportEntries(model);
			AppendExportResourceRows(model);
			return;
		}

		array<ResourceName> prefabs = new array<ResourceName>();
		CollectImportables(prefabs);

		foreach(ResourceName prefab : prefabs)
		{
			int id = m_Economy.GetInventoryId(prefab);

			OVT_TransferEntry entry = new OVT_TransferEntry();
			entry.m_sId = prefab;
			entry.m_sDisplayName = ResolveDisplayName(prefab);
			entry.m_eImageKind = EOVT_TransferImageKind.PREFAB;
			entry.m_sImage = prefab;
			entry.m_iValue = m_Economy.GetPrice(id);
			entry.m_eValueKind = EOVT_TransferValueKind.PRICE;
			entry.m_iMaxQuantity = IMPORT_MAX_QUANTITY;
			entry.m_iCategoryId = ResolveCategory(prefab, id);
			entry.m_bEnabled = true;
			entry.m_sDisabledReasonKey = "";

			model.Add(entry);
		}

		AppendImportResourceRows(model);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The shop browse category for a prefab, resolved through the registered prefab it is or
	//! inherits. A prefab with no registered ancestor would resolve to id 0, i.e. some other item's
	//! category, so it is filed under OTHER instead.
	protected int ResolveCategory(ResourceName prefab, int id)
	{
		ResourceName pricing = m_Economy.ResolvePricingResource(prefab);
		if(pricing.IsEmpty()) return OVT_ShopCategory.OTHER;

		return m_Economy.GetItemCategory(m_Economy.GetInventoryId(pricing));
	}

	//------------------------------------------------------------------------------------------------
	override string GetCategoryLabelKey(int categoryId)
	{
		// Ahead of the helper, whose fall-through answers "#OVT-ShopCategory_Other" for any id it does
		// not know - and it does not know this one.
		if(categoryId == CATEGORY_RESOURCES) return "#OVT-ShopCategory_Resources";

		OVT_ShopCategory category = categoryId;
		return OVT_ShopCategoryHelper.GetLabelKey(category);
	}

	//------------------------------------------------------------------------------------------------
	override void FillDetails(OVT_TransferEntry entry, out string name, out string value, out string body)
	{
		name = entry.m_sDisplayName;
		value = FormatValue(entry.m_iValue, entry.m_eValueKind);
		body = "";

		if(IsResourceId(entry.m_sId))
		{
			FillResourceDetails(entry, body);
			return;
		}

		// A refused row leaves the body empty so the base can put the reason there instead.
		if(m_iMode == MODE_EXPORT)
		{
			if(entry.m_bEnabled) body = "#OVT-Export_Body";
			return;
		}

		UIInfo info = OVT_PrefabUtils.GetItemUIInfo(entry.m_sImage);
		if(info) body = info.GetDescription();
	}

	//------------------------------------------------------------------------------------------------
	//! Import buys, so there is no "Add all" for something the player pays for by the unit. Export
	//! sells stock the player already owns, so a whole stack is one press.
	override bool IsAddAllAllowed(int mode)
	{
		if(mode == MODE_EXPORT) return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The base money summary with the truck's projected cargo volume appended, so a resource order
	//! shows what it will do to the hold before it is committed.
	//!
	//! Only appended when the cart actually orders resources - an item-only import has nothing to say
	//! about volume. Deliberately NOT clamped to the capacity: an over-capacity cart must read as over
	//! capacity, which is the same thing ValidateResourceImportCart refuses.
	//! \return Already-resolved text, empty on an empty cart.
	override string GetSummaryText()
	{
		string summary = super.GetSummaryText();
		if(summary == "") return summary;

		array<ref OVT_TransferCartLine> itemLines = new array<ref OVT_TransferCartLine>();
		array<ref OVT_TransferCartLine> resourceLines = new array<ref OVT_TransferCartLine>();
		PartitionLines(m_Cart.GetLines(), itemLines, resourceLines);

		if(resourceLines.IsEmpty()) return summary;

		OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(GetOccupiedVehicle());
		if(!store) return summary;

		int capacity = store.GetCapacityLitres();
		if(capacity == OVT_ResourceStoreComponent.UNLIMITED_CAPACITY) return summary;

		int cartLitres = ResourceCartLitres(resourceLines);
		int projected = store.GetUsedLitres() + cartLitres;
		if(m_iMode == MODE_EXPORT)
			projected = Math.Max(0, store.GetUsedLitres() - cartLitres);

		string volume = WidgetManager.Translate("#OVT-Resource_SummaryVolume",
			FormatCubicMetres(projected), FormatCubicMetres(capacity));

		return WidgetManager.Translate("#OVT-Port_SummaryVolume", summary, volume);
	}

	//------------------------------------------------------------------------------------------------
	override void BuildDestinations(out array<ref OVT_TransferDestination> dests)
	{
		dests.Clear();

		IEntity vehicle = GetOccupiedVehicle();
		if(!vehicle) return;

		OVT_TransferDestination dest = new OVT_TransferDestination();
		dest.m_sId = "vehicle";
		dest.m_sLabel = "#OVT-Transfer_DestinationVehicle";
		dest.m_Entity = vehicle;

		dests.Insert(dest);
	}

	//------------------------------------------------------------------------------------------------
	override string ValidateCart(array<ref OVT_TransferCartLine> lines, OVT_TransferDestination dest)
	{
		if(!dest || !dest.m_Entity) return "#OVT-Transfer_NoVehicle";

		if(m_iMode == MODE_EXPORT) return ValidateExportCart(lines, dest);

		if(m_Economy && m_Cart.TotalValue() > m_Economy.GetPlayerMoney(m_sPlayerID))
			return "#OVT-CannotAfford";

		array<ref OVT_TransferCartLine> itemLines = new array<ref OVT_TransferCartLine>();
		array<ref OVT_TransferCartLine> resourceLines = new array<ref OVT_TransferCartLine>();
		PartitionLines(lines, itemLines, resourceLines);

		// The server clamps a too-large import to what fitted and charges only for that, which reads as
		// a half-broken purchase. Refusing the whole cart up front is the honest version.
		int free = FreeSpaceIn(dest.m_Entity);
		if(free >= 0 && TotalQuantityOf(itemLines) > free)
			return "#OVT-Transfer_NoSpace";

		return ValidateResourceImportCart(resourceLines);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] holder The destination entity.
	//! \return Items the destination can still take, or -1 when it is unlimited or has no ledger.
	protected int FreeSpaceIn(IEntity holder)
	{
		if(!holder) return -1;

		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(holder);
		if(!storage) return -1;

		int capacity = storage.GetCapacity();
		if(capacity < 0) return -1;

		return Math.Max(0, capacity - storage.GetTotalCount());
	}

	//------------------------------------------------------------------------------------------------
	//! One existing ImportToVehicle per line - no batching, no new RPC (D10). The port has no change
	//! invoker, so the one coalesced refresh is what repaints money and prices.
	override void OnAccept(array<ref OVT_TransferCartLine> lines, OVT_TransferDestination dest)
	{
		if(!lines || !dest || !dest.m_Entity) return;
		if(!m_Economy) return;

		// THE ROUTING COMES FIRST. GetInventoryId below is a bare map index: a "res:" id would resolve
		// to 0, i.e. some other item's identity, and import that instead.
		array<ref OVT_TransferCartLine> itemLines = new array<ref OVT_TransferCartLine>();
		array<ref OVT_TransferCartLine> resourceLines = new array<ref OVT_TransferCartLine>();
		PartitionLines(lines, itemLines, resourceLines);

		if(m_iMode == MODE_EXPORT)
		{
			AcceptExport(itemLines);
			AcceptResourcePort(resourceLines, EOVT_ResourceOp.PORT_EXPORT);
			return;
		}

		AcceptResourcePort(resourceLines, EOVT_ResourceOp.PORT_IMPORT);

		OVT_VehicleRequestComponent vehicles = OVT_ControllerComponent<OVT_VehicleRequestComponent>.Get();
		if(!vehicles) return;

		foreach(OVT_TransferCartLine line : itemLines)
		{
			if(line.m_iQuantity <= 0) continue;

			int id = m_Economy.GetInventoryId(line.m_sId);
			vehicles.ImportToVehicle(id, line.m_iQuantity, dest.m_Entity);
		}

		ScheduleRefresh();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The vehicle the player occupies, or null.
	protected IEntity GetOccupiedVehicle()
	{
		if(!m_Owner) return null;

		SCR_CompartmentAccessComponent compartment = SCR_CompartmentAccessComponent.Cast(m_Owner.FindComponent(SCR_CompartmentAccessComponent));
		if(!compartment) return null;

		return compartment.GetVehicle();
	}

	//------------------------------------------------------------------------------------------------
	//! Lists every prefab this player may import, de-duplicated. Same two branches and same membership
	//! rules as the pre-transfer-UI port screen - only the drawing moved out.
	//! \param[out] prefabs Receives the importable prefabs. Cleared first.
	protected void CollectImportables(out array<ResourceName> prefabs)
	{
		if(!prefabs) return;

		prefabs.Clear();

		// Trade L5 unlocks the extended catalogue anywhere; a resistance-held port area (closest
		// town or base to the port, whichever is nearer) unlocks it for everyone at that port.
		bool illegalImports = HasIllegalImportsPermission();
		if(!illegalImports)
			illegalImports = ResistanceHoldsPort();

		if(illegalImports)
		{
			array<ResourceName> resources();
			m_Economy.GetAllNonOccupyingFactionItems(resources);
			foreach(ResourceName prefab : resources)
			{
				if(prefabs.Contains(prefab)) continue;
				prefabs.Insert(prefab);
			}
			return;
		}

		// Occupying-faction items must be excluded here too: RpcAsk_ImportToVehicle rejects them
		// outright, so listing them offers a row that can only ever do nothing (BUG-033, BUG-102).
		int occupyingFactionIndex = OVT_Global.GetConfig().GetOccupyingFactionIndex();

		foreach(OVT_ShopInventoryConfig shop : m_Economy.m_ShopConfig.m_aShopConfigs)
		{
			if(shop.type == OVT_ShopType.SHOP_VEHICLE) continue;

			foreach(OVT_ShopInventoryItem item : shop.m_aInventoryItems)
			{
				array<SCR_EntityCatalogEntry> entries();
				m_Economy.FindInventoryItems(item.m_eItemType, item.m_eItemMode, item.m_sFind, entries, item.m_bIncludeSupportStationItems);

				foreach(SCR_EntityCatalogEntry entry : entries)
				{
					ResourceName prefab = entry.GetPrefab();
					if(prefabs.Contains(prefab)) continue;

					// GetInventoryId is a bare map index: an unregistered prefab resolves to id 0, i.e.
					// some other item's faction. Only ask about registered ones.
					if(m_Economy.IsRegisteredResource(prefab))
					{
						int id = m_Economy.GetInventoryId(prefab);
						if(m_Economy.ItemIsFromFaction(id, occupyingFactionIndex)) continue;
					}

					prefabs.Insert(prefab);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Localized display name for a prefab, memoised for the life of this context. Translated, not
	//! raw: the sort must order by what the player reads, not by "#AR-Item_..." keys.
	//! \param[in] res The prefab to resolve.
	//! \return A non-empty, sortable name.
	protected string ResolveDisplayName(ResourceName res)
	{
		if(m_mDisplayNames.Contains(res)) return m_mDisplayNames[res];

		string name = "";

		UIInfo info = OVT_PrefabUtils.GetItemUIInfo(res);
		if(info) name = info.GetName();

		if(name == "") name = res;

		string translated = WidgetManager.Translate(name);
		if(translated != "") name = translated;

		m_mDisplayNames[res] = name;
		return name;
	}

	//-----------------------------------------------------------------------
	// EXPORT
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The occupied vehicle's ledger, priced at what the port would pay. A line the port refuses -
	//! unregistered, or illegal without the gate - is listed DISABLED with the reason rather than
	//! hidden, so the player can see the truck is carrying something unsellable.
	//! \param[in] model The model to fill.
	protected void BuildExportEntries(OVT_TransferListModel model)
	{
		OVT_StorageRequestComponent requests = GetRequests();
		if(!requests) return;

		ResolveExportHolder();
		if(!m_ExportHolder || !m_ExportHolderId.IsValid()) return;

		m_bBuildingEntries = true;
		EnsureSnapshot(requests);
		m_bBuildingEntries = false;

		if(!requests.HasSnapshotFor(m_ExportHolderId)) return;

		OVT_StorageSnapshot snapshot = requests.GetSnapshot();
		if(!snapshot) return;

		vector pos = m_ExportHolder.GetOrigin();

		foreach(OVT_StorageLine line : snapshot.m_aLines)
		{
			if(!line || line.m_iCount <= 0) continue;

			ResourceName prefab = line.m_sRes;
			int unitPrice = requests.GetExportUnitPrice(m_PlayerData, pos, line.m_sRes);

			OVT_TransferEntry entry = new OVT_TransferEntry();
			entry.m_sId = line.m_sRes;
			entry.m_sDisplayName = ResolveDisplayName(prefab);
			entry.m_eImageKind = EOVT_TransferImageKind.PREFAB;
			entry.m_sImage = line.m_sRes;
			entry.m_iValue = unitPrice;
			entry.m_eValueKind = EOVT_TransferValueKind.PRICE;
			entry.m_iMaxQuantity = line.m_iCount;
			entry.m_iCategoryId = ResolveCategory(prefab, m_Economy.GetInventoryId(prefab));
			entry.m_bEnabled = unitPrice > 0;
			entry.m_sDisabledReasonKey = "";

			if(unitPrice <= 0) entry.m_sDisabledReasonKey = "#OVT-Export_NotSellable";

			model.Add(entry);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Re-reads which vehicle Export is selling. A change re-arms the pull: the snapshot in hand
	//! describes a holder the player is no longer in.
	protected void ResolveExportHolder()
	{
		IEntity vehicle = GetOccupiedVehicle();
		if(vehicle == m_ExportHolder) return;

		m_ExportHolder = vehicle;
		m_ExportHolderId = OVT_StorageUtils.GetHolderId(vehicle);
		m_bWantPull = true;
		m_bPullFailed = false;
	}

	//------------------------------------------------------------------------------------------------
	//! The port gate and the per-line gate, re-checked at commit time. Both are also enforced on the
	//! server (OVT_StorageRequestComponent.AtAPort / ResolveExportUnitPrice) - opKind arrives from a
	//! client, so this is the courtesy message, not the protection.
	//! \param[in] lines The cart.
	//! \param[in] dest The chosen destination.
	//! \return A reason key, or "" when the sale may go ahead.
	protected string ValidateExportCart(array<ref OVT_TransferCartLine> lines, OVT_TransferDestination dest)
	{
		if(!AtAPort(dest.m_Entity)) return "#OVT-Storage_NotAtPort";

		OVT_StorageRequestComponent requests = GetRequests();
		if(!requests) return "#OVT-Storage_NotFound";

		vector pos = dest.m_Entity.GetOrigin();

		array<ref OVT_TransferCartLine> itemLines = new array<ref OVT_TransferCartLine>();
		array<ref OVT_TransferCartLine> resourceLines = new array<ref OVT_TransferCartLine>();
		PartitionLines(lines, itemLines, resourceLines);

		foreach(OVT_TransferCartLine line : itemLines)
		{
			if(!line) continue;

			if(requests.GetExportUnitPrice(m_PlayerData, pos, line.m_sId) <= 0)
				return "#OVT-Export_NotSellable";
		}

		return ValidateResourceExportCart(resourceLines);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the player and the vehicle are both standing at a port, on the server's own numbers.
	//! \param[in] holder The vehicle being sold.
	//! \return True when the sale is in range.
	protected bool AtAPort(IEntity holder)
	{
		if(!m_Economy || !m_Owner || !holder) return false;

		float callerDistance = m_Economy.DistanceToNearestPort(m_Owner.GetOrigin());
		if(callerDistance < 0 || callerDistance > OVT_StorageRequestComponent.EXPORT_MAX_PORT_DISTANCE)
			return false;

		float holderDistance = m_Economy.DistanceToNearestPort(holder.GetOrigin());
		if(holderDistance < 0 || holderDistance > OVT_StorageRequestComponent.EXPORT_MAX_PORT_DISTANCE)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! ONE CHECKOUT for the whole cart, source and destination both the vehicle.
	//!
	//! ⚠ On a listen host the entire sale runs inside this method - EXPORT is a one-step job - so the
	//! batch reply has already landed before it returns. The result handler therefore schedules a
	//! redraw rather than running one.
	//! \param[in] lines The cart.
	protected void AcceptExport(array<ref OVT_TransferCartLine> lines)
	{
		if(!lines || lines.IsEmpty()) return;
		if(!m_ExportHolderId.IsValid()) return;

		OVT_StorageRequestComponent requests = GetRequests();
		if(!requests) return;

		int seq = requests.RequestBatchBegin(m_ExportHolderId, m_ExportHolderId, EOVT_StorageOp.EXPORT, lines.Count());
		if(seq == OVT_StorageRequestComponent.SEQ_NONE) return;

		for(int i = 0; i < lines.Count(); i++)
		{
			OVT_TransferCartLine line = lines[i];
			if(!line) continue;

			requests.RequestBatchLine(seq, i, line.m_sId, line.m_iQuantity);
		}

		requests.RequestBatchCommit(seq, lines.Count());
	}

	//------------------------------------------------------------------------------------------------
	//! Fires the contents pull at most once per (holder, sequence).
	//!
	//! ⚠ THE ORDER OF THE TWO STATEMENTS IS LOAD-BEARING. On a listen host RequestOpenStorage runs
	//! the whole fan before it returns, so the invoker re-enters BuildEntries from inside this call.
	//! Clearing the latch first is what makes that second visit find nothing to do.
	//! \param[in] requests The local player's request component.
	protected void EnsureSnapshot(OVT_StorageRequestComponent requests)
	{
		if(!m_bWantPull) return;

		m_bWantPull = false;
		m_bPullFailed = false;

		int seq = requests.RequestOpenStorage(m_ExportHolderId);

		if(seq == OVT_StorageRequestComponent.SEQ_NONE) m_bWantPull = true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while Export has nothing to draw and is waiting for a fan.
	protected bool ExportIsLoading()
	{
		if(m_bPullFailed) return false;
		if(!m_ExportHolderId.IsValid()) return false;

		OVT_StorageRequestComponent requests = GetRequests();
		if(!requests) return false;

		return !requests.HasSnapshotFor(m_ExportHolderId);
	}

	//------------------------------------------------------------------------------------------------
	//! A contents fan committed.
	protected void OnContentsUpdated()
	{
		if(!m_bIsActive || !m_wRoot) return;

		// Listen host: this fired from inside our own BuildEntries. The Refresh already running will
		// read the snapshot itself, so redrawing here would build the list twice.
		if(m_bBuildingEntries) return;

		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! A storage request was refused. Stops the loading message and says why.
	//!
	//! ⚠ THE MESSAGE IS DRAWN ON THE NEXT CALL-QUEUE PASS, NEVER HERE. On a listen host the refusal
	//! lands inside BuildEntries (a pull) or inside OnAccept (a sale), and both sit inside a base call
	//! that clears every persistent message before it returns - RefreshCheckout() over the emptied
	//! cart, then Accept()'s own "#OVT-Transfer_Accepted". Drawing synchronously means the player is
	//! told the sale succeeded when the server refused it.
	//! \param[in] messageKey Localization key describing the refusal.
	protected void OnStorageError(string messageKey)
	{
		if(!m_bIsActive || !m_wRoot) return;

		m_bPullFailed = true;
		m_sPendingError = messageKey;

		GetGame().GetCallqueue().Remove(ShowStorageError);
		GetGame().GetCallqueue().CallLater(ShowStorageError, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Draws the refusal once the base call that carried it has unwound.
	protected void ShowStorageError()
	{
		if(!m_bIsActive || !m_wRoot) return;
		if(m_sPendingError == "") return;

		ShowPersistentMessage(m_sPendingError);
		m_sPendingError = "";
	}

	//------------------------------------------------------------------------------------------------
	//! A sale finished. Scheduled, never immediate - see AcceptExport.
	//! \param[in] moved How many items were sold.
	//! \param[in] shortfall How many were asked for and did not sell.
	//! \param[in] earned Money paid out.
	protected void OnBatchResult(int moved, int shortfall, int earned)
	{
		if(!m_bIsActive || !m_wRoot) return;

		m_bWantPull = true;

		ScheduleRefresh();
	}

	//-----------------------------------------------------------------------
	// SUBSCRIPTIONS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Every subscription here has a matching Remove in Unsubscribe(). The target outlives the
	//! layout, so a missed Remove is an extra redraw per port visit for the rest of the session.
	protected void Subscribe()
	{
		OVT_StorageRequestComponent requests = OVT_ControllerComponent<OVT_StorageRequestComponent>.Get();
		if(requests)
		{
			m_SubscribedRequests = requests;
			m_SubscribedRequests.GetOnContentsUpdated().Insert(OnContentsUpdated);
			m_SubscribedRequests.GetOnStorageError().Insert(OnStorageError);
			m_SubscribedRequests.GetOnBatchResult().Insert(OnBatchResult);
		}

		OVT_ResourceRequestComponent resourceRequests = OVT_ControllerComponent<OVT_ResourceRequestComponent>.Get();
		if(resourceRequests)
		{
			m_SubscribedResourceRequests = resourceRequests;
			m_SubscribedResourceRequests.GetOnTransferResult().Insert(OnResourceTransferResult);
			m_SubscribedResourceRequests.GetOnResourceError().Insert(OnResourceRequestError);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Unsubscribe()
	{
		if(m_SubscribedRequests)
		{
			m_SubscribedRequests.GetOnContentsUpdated().Remove(OnContentsUpdated);
			m_SubscribedRequests.GetOnStorageError().Remove(OnStorageError);
			m_SubscribedRequests.GetOnBatchResult().Remove(OnBatchResult);
		}

		if(m_SubscribedResourceRequests)
		{
			m_SubscribedResourceRequests.GetOnTransferResult().Remove(OnResourceTransferResult);
			m_SubscribedResourceRequests.GetOnResourceError().Remove(OnResourceRequestError);
		}

		m_SubscribedRequests = null;
		m_SubscribedResourceRequests = null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The local player's storage request component, or null.
	protected OVT_StorageRequestComponent GetRequests()
	{
		if(m_SubscribedRequests) return m_SubscribedRequests;

		return OVT_ControllerComponent<OVT_StorageRequestComponent>.Get();
	}

	//-----------------------------------------------------------------------
	// RESOURCES
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! One Import row per resource this player may actually buy here. A resource the port will never
	//! sell is NOT listed - a row whose only outcome is a no-op click is a bug (BUG-102) - but a
	//! resource the player simply has nowhere to put IS, disabled and with the reason on it.
	//! \param[in] model The model to append to.
	protected void AppendImportResourceRows(OVT_TransferListModel model)
	{
		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if(!resources) return;

		OVT_ResourceDefs defs = resources.GetDefs();
		if(!defs) return;

		bool hasPermission = HasIllegalImportsPermission();
		bool resistancePort = false;
		if(!hasPermission) resistancePort = ResistanceHoldsPort();

		OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(GetOccupiedVehicle());

		int freeLitres = 0;
		if(store) freeLitres = store.GetFreeLitres();

		for(int i = 0; i < defs.Count(); i++)
		{
			if(!OVT_ResourceRules.MayImport(defs, i)) continue;
			if(!OVT_ResourceRules.IllegalGateOpen(defs, i, hasPermission, resistancePort)) continue;

			string id = defs.IdAt(i);
			int litres = defs.LitresAt(i);

			int maxQuantity = IMPORT_MAX_QUANTITY;
			if(litres > 0) maxQuantity = Math.Min(IMPORT_MAX_QUANTITY, freeLitres / litres);

			OVT_TransferEntry entry = new OVT_TransferEntry();
			entry.m_sId = OVT_ResourceTransferContext.RES_PREFIX + id;
			entry.m_sDisplayName = ResolveResourceName(id);
			entry.m_eImageKind = EOVT_TransferImageKind.TEXTURE;
			entry.m_sImage = ResolveResourceIcon(id);
			entry.m_iValue = resources.GetPrice(i);
			entry.m_eValueKind = EOVT_TransferValueKind.PRICE;
			entry.m_iMaxQuantity = maxQuantity;
			entry.m_iCategoryId = CATEGORY_RESOURCES;
			entry.m_bEnabled = true;
			entry.m_sDisabledReasonKey = "";

			if(!store)
			{
				entry.m_bEnabled = false;
				entry.m_sDisabledReasonKey = "#OVT-Resource_NeedTruck";
			}
			else if(maxQuantity <= 0)
			{
				entry.m_bEnabled = false;
				entry.m_sDisabledReasonKey = "#OVT-Resource_NoCargoSpace";
			}

			model.Add(entry);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One Export row per resource the occupied vehicle is carrying, importable or not - hauling home
	//! something the port will not sell you is the point of a non-importable resource.
	//! \param[in] model The model to append to.
	protected void AppendExportResourceRows(OVT_TransferListModel model)
	{
		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if(!resources) return;

		OVT_ResourceDefs defs = resources.GetDefs();
		if(!defs) return;

		OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(GetOccupiedVehicle());
		if(!store) return;

		OVT_ResourceLedger ledger = store.GetLedger();
		if(!ledger) return;

		bool hasPermission = HasIllegalImportsPermission();
		bool resistancePort = false;
		if(!hasPermission) resistancePort = ResistanceHoldsPort();

		array<string> ids = new array<string>();
		array<int> quantities = new array<int>();
		ledger.GetLines(ids, quantities);

		for(int i = 0; i < ids.Count(); i++)
		{
			string id = ids[i];
			int qty = quantities[i];
			if(qty <= 0) continue;

			int index = defs.IndexOf(id);
			if(index < 0) continue;

			OVT_TransferEntry entry = new OVT_TransferEntry();
			entry.m_sId = OVT_ResourceTransferContext.RES_PREFIX + id;
			entry.m_sDisplayName = ResolveResourceName(id);
			entry.m_eImageKind = EOVT_TransferImageKind.TEXTURE;
			entry.m_sImage = ResolveResourceIcon(id);
			entry.m_iValue = resources.GetSellPrice(index);
			entry.m_eValueKind = EOVT_TransferValueKind.PRICE;
			entry.m_iMaxQuantity = qty;
			entry.m_iCategoryId = CATEGORY_RESOURCES;
			entry.m_bEnabled = true;
			entry.m_sDisabledReasonKey = "";

			if(!OVT_ResourceRules.MayExport(defs, index))
			{
				entry.m_bEnabled = false;
				entry.m_sDisabledReasonKey = "#OVT-Resource_NotSellable";
			}
			else if(!OVT_ResourceRules.IllegalGateOpen(defs, index, hasPermission, resistancePort))
			{
				entry.m_bEnabled = false;
				entry.m_sDisabledReasonKey = "#OVT-Resource_Illegal";
			}

			model.Add(entry);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The description plus one line of drift, in words (D10 - FillDetails has no image channel and
	//! the row image already carries the resource icon). A refused row leaves the body empty so the
	//! base can put the reason there instead.
	//! \param[in] entry The resource row.
	//! \param[out] body Receives the details body.
	protected void FillResourceDetails(OVT_TransferEntry entry, out string body)
	{
		body = "";

		if(!entry.m_bEnabled) return;

		string id = ResourceIdOf(entry.m_sId);

		OVT_Resource res = FindResource(id);
		if(res && res.m_sDescription != "") body = WidgetManager.Translate(res.m_sDescription);

		string drift = DriftText(id);
		if(drift == "") return;

		if(body == "")
		{
			body = drift;
			return;
		}

		body = body + "\n" + drift;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the DRIFTED price sits against the config base. Reads the stored (band-clamped) price,
	//! not GetPrice: the difficulty level multiplier is a flat scaling applied at read time, so on
	//! Hard or Insane it would peg every resource "above base" forever and describe no drift at all.
	//! Neither figure is ever quoted as a price.
	//! \param[in] id A bare resource id.
	//! \return A localization key, or "" when the resource or its base price is unknown.
	protected string DriftText(string id)
	{
		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if(!resources) return "";

		OVT_ResourceDefs defs = resources.GetDefs();
		if(!defs) return "";

		int index = defs.IndexOf(id);
		if(index < 0) return "";

		int basePrice = resources.GetBasePrice(index);
		if(basePrice <= 0) return "";

		float ratio = resources.GetStoredPrice(index);
		ratio = ratio / basePrice;

		if(ratio < DRIFT_FAR_BELOW) return "#OVT-Resource_PriceFarBelow";
		if(ratio < DRIFT_BELOW) return "#OVT-Resource_PriceBelow";
		if(ratio <= DRIFT_ABOVE) return "#OVT-Resource_PriceNormal";
		if(ratio <= DRIFT_FAR_ABOVE) return "#OVT-Resource_PriceAbove";

		return "#OVT-Resource_PriceFarAbove";
	}

	//------------------------------------------------------------------------------------------------
	//! Whole-cart fit for the resource half of an Import (D1). Nothing clamps: a cart that does not
	//! fit is refused entire, exactly as the item half is.
	//! \param[in] lines The resource lines of the cart.
	//! \return "" when the resources may be bought, otherwise a reason.
	protected string ValidateResourceImportCart(array<ref OVT_TransferCartLine> lines)
	{
		if(!lines || lines.IsEmpty()) return "";

		OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(GetOccupiedVehicle());
		if(!store) return "#OVT-Resource_NeedTruck";

		int freeLitres = store.GetFreeLitres();
		int cartLitres = ResourceCartLitres(lines);

		if(cartLitres <= freeLitres) return "";

		return WidgetManager.Translate("#OVT-Resource_NoSpaceVolume",
			FormatCubicMetres(cartLitres), FormatCubicMetres(freeLitres));
	}

	//------------------------------------------------------------------------------------------------
	//! The per-line gate for the resource half of an Export, re-checked at commit time. The server
	//! enforces the same two rules; this is the courtesy message, not the protection.
	//! \param[in] lines The resource lines of the cart.
	//! \return "" when the resources may be sold, otherwise a reason.
	protected string ValidateResourceExportCart(array<ref OVT_TransferCartLine> lines)
	{
		if(!lines || lines.IsEmpty()) return "";

		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if(!resources) return "#OVT-Resource_NoCatalogue";

		OVT_ResourceDefs defs = resources.GetDefs();
		if(!defs) return "#OVT-Resource_NoCatalogue";

		bool hasPermission = HasIllegalImportsPermission();
		bool resistancePort = false;
		if(!hasPermission) resistancePort = ResistanceHoldsPort();

		foreach(OVT_TransferCartLine line : lines)
		{
			if(!line) continue;

			int index = defs.IndexOf(ResourceIdOf(line.m_sId));

			if(!OVT_ResourceRules.MayExport(defs, index)) return "#OVT-Resource_NotSellable";
			if(!OVT_ResourceRules.IllegalGateOpen(defs, index, hasPermission, resistancePort))
				return "#OVT-Resource_Illegal";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! ONE CHECKOUT for the whole resource half of the cart: TransferBegin, one TransferLine per line,
	//! TransferCommit. Both RplId slots carry the vehicle; the op kind decides which one is read.
	//!
	//! ⚠ On a listen host the entire checkout runs inside this method - every ask invokes its handler
	//! directly - so THE LATCH IS SET BEFORE THE FIRST ASK. Setting it after would latch over a reply
	//! that already cleared it.
	//! \param[in] lines The resource lines of the cart.
	//! \param[in] opKind EOVT_ResourceOp.PORT_IMPORT or PORT_EXPORT.
	protected void AcceptResourcePort(array<ref OVT_TransferCartLine> lines, int opKind)
	{
		if(!lines || lines.IsEmpty()) return;
		if(m_bResourceCheckoutPending) return;

		OVT_ResourceRequestComponent requests = GetResourceRequests();
		if(!requests) return;

		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if(!resources) return;

		OVT_ResourceDefs defs = resources.GetDefs();
		if(!defs) return;

		RplId holderId = OVT_ResourceUtils.GetHolderId(GetOccupiedVehicle());
		if(!holderId.IsValid())
		{
			ShowResourceRefusal("#OVT-Resource_NeedTruck");
			return;
		}

		m_bResourceCheckoutPending = true;

		int seq = requests.RequestTransferBegin(holderId, holderId, opKind, lines.Count());
		if(seq == OVT_ResourceRequestComponent.SEQ_NONE)
		{
			m_bResourceCheckoutPending = false;
			return;
		}

		for(int i = 0; i < lines.Count(); i++)
		{
			OVT_TransferCartLine line = lines[i];
			if(!line) continue;

			requests.RequestTransferLine(seq, i, defs.IndexOf(ResourceIdOf(line.m_sId)), line.m_iQuantity);
		}

		requests.RequestTransferCommit(seq, lines.Count());
	}

	//------------------------------------------------------------------------------------------------
	//! A resource checkout finished.
	//! \param[in] movedLitres Litres that moved.
	//! \param[in] earned Money paid to the player.
	//! \param[in] spent Money charged to the player.
	protected void OnResourceTransferResult(int movedLitres, int earned, int spent)
	{
		m_bResourceCheckoutPending = false;

		if(!m_bIsActive || !m_wRoot) return;

		ScheduleRefresh();
	}

	//------------------------------------------------------------------------------------------------
	//! A resource request was refused. Drawn deferred for the reason OnStorageError documents.
	//! \param[in] messageKey Localization key naming the refusal.
	protected void OnResourceRequestError(string messageKey)
	{
		m_bResourceCheckoutPending = false;

		if(!m_bIsActive || !m_wRoot) return;

		ShowResourceRefusal(messageKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Queues a refusal for the next call-queue pass. Never draws here: on a listen host this runs
	//! inside Accept(), which prints its own "order placed" message afterwards.
	//! \param[in] messageKey Localization key naming the refusal.
	protected void ShowResourceRefusal(string messageKey)
	{
		m_sPendingError = messageKey;

		GetGame().GetCallqueue().Remove(ShowStorageError);
		GetGame().GetCallqueue().CallLater(ShowStorageError, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Splits a cart on the "res:" prefix. Every hook that consumes lines calls this BEFORE it does
	//! anything else with an id.
	//! \param[in] lines The whole cart.
	//! \param[out] itemLines Receives the prefab lines. Cleared first.
	//! \param[out] resourceLines Receives the resource lines. Cleared first.
	protected void PartitionLines(array<ref OVT_TransferCartLine> lines, out array<ref OVT_TransferCartLine> itemLines, out array<ref OVT_TransferCartLine> resourceLines)
	{
		if(!itemLines || !resourceLines) return;

		itemLines.Clear();
		resourceLines.Clear();

		if(!lines) return;

		foreach(OVT_TransferCartLine line : lines)
		{
			if(!line) continue;

			if(IsResourceId(line.m_sId))
			{
				resourceLines.Insert(line);
				continue;
			}

			itemLines.Insert(line);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] lines Some cart lines.
	//! \return How many units they order in total.
	protected int TotalQuantityOf(array<ref OVT_TransferCartLine> lines)
	{
		if(!lines) return 0;

		int total = 0;
		foreach(OVT_TransferCartLine line : lines)
		{
			if(!line) continue;

			total += line.m_iQuantity;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] lines The resource lines of a cart.
	//! \return Their volume in litres.
	protected int ResourceCartLitres(array<ref OVT_TransferCartLine> lines)
	{
		if(!lines) return 0;

		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if(!resources) return 0;

		OVT_ResourceDefs defs = resources.GetDefs();
		if(!defs) return 0;

		int total = 0;
		foreach(OVT_TransferCartLine line : lines)
		{
			if(!line) continue;

			total += line.m_iQuantity * defs.LitresPerUnit(ResourceIdOf(line.m_sId));
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entryId A row or cart-line id.
	//! \return True when it names a resource rather than a prefab.
	protected bool IsResourceId(string entryId)
	{
		return entryId.StartsWith(OVT_ResourceTransferContext.RES_PREFIX);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entryId A row or cart-line id.
	//! \return The bare resource id behind it.
	protected string ResourceIdOf(string entryId)
	{
		if(!IsResourceId(entryId)) return entryId;

		int prefix = OVT_ResourceTransferContext.RES_PREFIX.Length();

		return entryId.Substring(prefix, entryId.Length() - prefix);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id A bare resource id.
	//! \return Its catalogue entry, or null when the catalogue does not know it.
	protected OVT_Resource FindResource(string id)
	{
		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if(!resources) return null;

		OVT_ResourcesConfig config = resources.GetResourcesConfig();
		if(!config || !config.m_aResources) return null;

		foreach(OVT_Resource res : config.m_aResources)
		{
			if(res && res.m_sId == id) return res;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id A bare resource id.
	//! \return The translated title, falling back to the id so a row is never blank.
	protected string ResolveResourceName(string id)
	{
		return OVT_ResourceUtils.ResolveResourceTitle(id);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id A bare resource id.
	//! \return The row icon, or "" when the catalogue authors none.
	protected ResourceName ResolveResourceIcon(string id)
	{
		OVT_Resource res = FindResource(id);
		if(!res) return "";

		return res.m_tIcon;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] litres A volume in integer litres.
	//! \return The same volume in m3, one decimal.
	protected string FormatCubicMetres(int litres)
	{
		return OVT_ResourceUtils.FormatCubicMetres(litres);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether the player carries the smuggling permission.
	protected bool HasIllegalImportsPermission()
	{
		return m_PlayerData && m_PlayerData.HasPermission("IllegalImports");
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether the resistance holds the port area the player is standing in.
	protected bool ResistanceHoldsPort()
	{
		if(!m_Economy || !m_Owner) return false;

		return m_Economy.ResistanceControlsNearestPort(m_Owner.GetOrigin());
	}

	//------------------------------------------------------------------------------------------------
	//! \return The local player's resource request component, or null.
	protected OVT_ResourceRequestComponent GetResourceRequests()
	{
		if(m_SubscribedResourceRequests) return m_SubscribedResourceRequests;

		return OVT_ControllerComponent<OVT_ResourceRequestComponent>.Get();
	}
}
