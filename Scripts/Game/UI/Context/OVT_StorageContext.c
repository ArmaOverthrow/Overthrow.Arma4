//------------------------------------------------------------------------------------------------
//! Open Storage: one Take mode over a holder's ledger, with the holder's own vanilla inventory and
//! every nearby holder as destinations. All widget work belongs to OVT_TransferContext.
//!
//! THE LIST IS ASYNC. Contents never replicate - they are pulled on open by the one player looking,
//! in a Begin...Line...End fan. BuildEntries fires that pull and returns empty on the first frame;
//! the arrival of the fan calls Refresh() again, which is why no new base hook was needed
//! (OVT_TransferContext.Refresh() is already public).
//!
//! ⚠ ON A LISTEN HOST OR IN SINGLE PLAYER THE WHOLE FAN ARRIVES INSIDE RequestOpenStorage(), i.e.
//! inside BuildEntries, i.e. BuildEntries re-entering itself. m_bWantPull is cleared BEFORE the call
//! and that ordering is what terminates the recursion; m_bBuildingEntries stops the inner Refresh so
//! the outer one draws the list exactly once.
//------------------------------------------------------------------------------------------------
class OVT_StorageContext : OVT_TransferContext
{
	//! Coalescing delay for a live re-pull. Between the warehouse's 50 ms local invoker and the
	//! shop's 400 ms recheck, because a network round trip sits in between.
	protected const int LIVE_REFRESH_MS = 250;

	//! The holder this screen is open on, and its networked name.
	protected IEntity m_Holder;
	protected RplId m_HolderId;

	//! The holder's component. Cached for the count subscription, which outlives the layout.
	protected OVT_StorageComponent m_SubscribedStorage;

	//! The request component this screen subscribed to. Cached for the same reason.
	protected OVT_StorageRequestComponent m_SubscribedRequests;

	//! THE LATCH. True when the snapshot in hand (if any) is not trusted and a pull is owed.
	protected bool m_bWantPull;

	//! True while BuildEntries is running, so a fan that lands inside it does not re-enter Refresh.
	protected bool m_bBuildingEntries;

	//! True once a request was refused, so the loading message stops instead of standing forever.
	protected bool m_bPullFailed;

	//! The refusal waiting to be drawn. Empty when there is none.
	protected string m_sPendingError;

	//! Prefab -> localized display name. UIInfo resolution loads and scans a prefab container, far
	//! too expensive to repeat per row per sort comparison; names never change.
	protected ref map<string, string> m_mDisplayNames = new map<string, string>();

	//------------------------------------------------------------------------------------------------
	//! An RplId member is an engine handle, not an int - it is given a value here so IsValid() is
	//! answerable before any holder has been set.
	void OVT_StorageContext()
	{
		m_HolderId = RplId.Invalid();
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the holder before ShowContext, the way OVT_VehicleMenuContext sets a warehouse.
	//! \param[in] holder The entity whose ledger this screen draws.
	void SetHolder(IEntity holder)
	{
		m_Holder = holder;
		m_HolderId = OVT_StorageUtils.GetHolderId(holder);
		m_bWantPull = true;
		m_bPullFailed = false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The holder this screen is open on, or null.
	IEntity GetHolder()
	{
		return m_Holder;
	}

	//-----------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Subscribes BEFORE super.OnShow(), because super.OnShow() calls Refresh() and on a listen host
	//! the whole contents fan lands inside that call.
	override void OnShow()
	{
		m_bWantPull = true;
		m_bPullFailed = false;
		m_bBuildingEntries = false;
		m_sPendingError = "";

		// The holder may have been re-created since SetHolder (a re-open after a respawn).
		if(m_Holder) m_HolderId = OVT_StorageUtils.GetHolderId(m_Holder);

		Subscribe();

		super.OnShow();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes exactly what OnShow inserted: three request invokers, the holder's count invoker and
	//! the two pending callbacks. All of them outlive the layout.
	override void OnClose()
	{
		GetGame().GetCallqueue().Remove(LiveRefresh);
		GetGame().GetCallqueue().Remove(ShowStorageError);

		Unsubscribe();

		super.OnClose();

		m_bWantPull = false;
		m_bBuildingEntries = false;
		m_bPullFailed = false;
		m_sPendingError = "";
	}

	//------------------------------------------------------------------------------------------------
	//! The loading message is set AFTER the base's refresh, because RefreshCheckout() clears a
	//! persistent message whenever the cart validates - which an empty cart always does.
	override void Refresh()
	{
		super.Refresh();

		if(!m_bIsActive || !m_wRoot) return;

		if(IsLoading()) ShowPersistentMessage("#OVT-Storage_Loading");
	}

	//-----------------------------------------------------------------------
	// THE EIGHT HOOKS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	override void BuildModes(out array<int> modes, out array<string> labelKeys)
	{
		modes.Clear();
		labelKeys.Clear();

		modes.Insert(0);
		labelKeys.Insert("#OVT-Storage_Take");
	}

	//------------------------------------------------------------------------------------------------
	//! Rows come from the staged snapshot; the pull that fills it is fired from here, latched.
	override void BuildEntries(int mode, OVT_TransferListModel model)
	{
		if(!model) return;

		OVT_StorageRequestComponent requests = GetRequests();
		if(!requests || !m_HolderId.IsValid()) return;

		m_bBuildingEntries = true;
		EnsureSnapshot(requests);
		m_bBuildingEntries = false;

		if(!requests.HasSnapshotFor(m_HolderId)) return;

		OVT_StorageSnapshot snapshot = requests.GetSnapshot();
		if(!snapshot) return;

		foreach(OVT_StorageLine line : snapshot.m_aLines)
		{
			if(!line || line.m_iCount <= 0) continue;

			OVT_TransferEntry entry = new OVT_TransferEntry();
			entry.m_sId = line.m_sRes;
			entry.m_sDisplayName = ResolveDisplayName(line.m_sRes);
			entry.m_eImageKind = EOVT_TransferImageKind.PREFAB;
			entry.m_sImage = line.m_sRes;
			entry.m_iValue = line.m_iCount;
			entry.m_eValueKind = EOVT_TransferValueKind.QUANTITY;
			entry.m_iMaxQuantity = line.m_iCount;
			entry.m_iCategoryId = ResolveCategory(line.m_sRes);
			entry.m_bEnabled = true;
			entry.m_sDisabledReasonKey = "";

			model.Add(entry);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return The shop browse category for a prefab, the same mapping the port screen uses. Converted
	//! battlefield loot is often unregistered, and an unregistered prefab resolves to id 0 - i.e. some
	//! other item's category - so it is filed under OTHER instead.
	protected int ResolveCategory(ResourceName prefab)
	{
		if(!m_Economy) return OVT_ShopCategory.OTHER;
		if(!m_Economy.IsRegisteredResource(prefab)) return OVT_ShopCategory.OTHER;

		return m_Economy.GetItemCategory(m_Economy.GetInventoryId(prefab));
	}

	//------------------------------------------------------------------------------------------------
	override string GetCategoryLabelKey(int categoryId)
	{
		OVT_ShopCategory category = categoryId;
		return OVT_ShopCategoryHelper.GetLabelKey(category);
	}

	//------------------------------------------------------------------------------------------------
	//! THE MOD'S FIRST PICKER WITH MORE THAN ONE ENTRY: this holder's own vanilla inventory, then
	//! every other usable holder within the request component's radius, nearest first.
	//!
	//! Nearest-first rather than query order, which is spatially arbitrary and would reshuffle the
	//! picker between two refreshes that found the same holders.
	override void BuildDestinations(out array<ref OVT_TransferDestination> dests)
	{
		dests.Clear();

		if(!m_Holder) return;

		// A warehouse building has no vanilla inventory, so "this container" would be a dead row.
		if(OVT_StorageUtils.GetInventoryManager(m_Holder))
		{
			OVT_TransferDestination own = new OVT_TransferDestination();
			own.m_sId = "inventory";
			own.m_sLabel = "#OVT-Storage_DestinationInventory";
			own.m_Entity = m_Holder;

			dests.Insert(own);
		}

		OVT_StorageRequestComponent requests = GetRequests();
		if(!requests) return;

		array<IEntity> holders = new array<IEntity>();

		// One query object per call. The shared-accumulator singleton is the defect this feature
		// exists to stop repeating (OVT_InventoryManagerComponent:497).
		OVT_StorageHolderQuery query = new OVT_StorageHolderQuery();
		query.Run(m_Holder.GetOrigin(), requests.GetHolderRadius(), holders);

		array<IEntity> sorted = new array<IEntity>();
		SortByDistance(holders, m_Holder.GetOrigin(), sorted);

		int index = 0;
		foreach(IEntity holder : sorted)
		{
			if(holder == m_Holder) continue;

			OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(holder);
			if(!storage) continue;

			RplId id = OVT_StorageUtils.GetHolderId(holder);
			if(!id.IsValid()) continue;

			OVT_TransferDestination dest = new OVT_TransferDestination();
			dest.m_sId = "holder" + index.ToString();
			dest.m_sLabel = storage.GetDisplayName();
			dest.m_Entity = holder;

			dests.Insert(dest);
			index++;
		}
	}

	//------------------------------------------------------------------------------------------------
	override void FillDetails(OVT_TransferEntry entry, out string name, out string value, out string body)
	{
		name = entry.m_sDisplayName;
		value = FormatValue(entry.m_iValue, entry.m_eValueKind);
		body = "";

		UIInfo info = OVT_PrefabUtils.GetItemUIInfo(entry.m_sImage);
		if(info) body = info.GetDescription();
	}

	//------------------------------------------------------------------------------------------------
	//! Taking from a holder the player may already open is free, so a whole stack is one press.
	override bool IsAddAllAllowed(int mode)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Fit is deliberately NOT checked here. The server stops early and reports the shortfall, which
	//! is the only answer that can be right: capacity is server state and the cart is client state.
	override string ValidateCart(array<ref OVT_TransferCartLine> lines, OVT_TransferDestination dest)
	{
		if(!dest || !dest.m_Entity) return "#OVT-Storage_NoDestination";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! ONE CHECKOUT, NOT ONE REQUEST PER LINE - this is the seam that supersedes logistics/ui D10.
	//!
	//! ⚠ On a listen host the entire checkout runs inside this method: every ask invokes its handler
	//! directly, so VALIDATE, the move and the batch reply have all happened before it returns. The
	//! batch-result handler therefore schedules a redraw rather than running one, or it would
	//! re-enter the base's Accept() while Accept() is still clearing the cart.
	override void OnAccept(array<ref OVT_TransferCartLine> lines, OVT_TransferDestination dest)
	{
		if(!lines || lines.IsEmpty()) return;
		if(!dest || !dest.m_Entity) return;
		if(!m_HolderId.IsValid()) return;

		OVT_StorageRequestComponent requests = GetRequests();
		if(!requests) return;

		RplId destId = m_HolderId;
		int opKind = EOVT_StorageOp.TO_INVENTORY;

		if(dest.m_Entity != m_Holder)
		{
			destId = OVT_StorageUtils.GetHolderId(dest.m_Entity);
			opKind = EOVT_StorageOp.TO_HOLDER;

			if(!destId.IsValid())
			{
				ShowPersistentMessage("#OVT-Storage_NotFound");
				return;
			}
		}

		int seq = requests.RequestBatchBegin(m_HolderId, destId, opKind, lines.Count());
		if(seq == OVT_StorageRequestComponent.SEQ_NONE) return;

		for(int i = 0; i < lines.Count(); i++)
		{
			OVT_TransferCartLine line = lines[i];
			if(!line) continue;

			requests.RequestBatchLine(seq, i, line.m_sId, line.m_iQuantity);
		}

		requests.RequestBatchCommit(seq, lines.Count());
	}

	//-----------------------------------------------------------------------
	// THE PULL
	//-----------------------------------------------------------------------

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

		int seq = requests.RequestOpenStorage(m_HolderId);

		// Nothing was sent (not the local controller, or the holder is unreachable here); re-arm so a
		// later refresh tries again rather than showing an empty list forever.
		if(seq == OVT_StorageRequestComponent.SEQ_NONE) m_bWantPull = true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while the screen has nothing to draw and is waiting for a fan.
	protected bool IsLoading()
	{
		if(m_bPullFailed) return false;

		OVT_StorageRequestComponent requests = GetRequests();
		if(!requests || !m_HolderId.IsValid()) return false;

		return !requests.HasSnapshotFor(m_HolderId);
	}

	//------------------------------------------------------------------------------------------------
	//! A contents fan committed.
	protected void OnContentsUpdated()
	{
		if(!m_bIsActive || !m_wRoot) return;

		// Listen host: this fired from inside our own BuildEntries. The Refresh that is already
		// running will read the snapshot itself, so redrawing here would build the list twice.
		if(m_bBuildingEntries) return;

		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! A request was refused. Stops the loading message and says why.
	//!
	//! ⚠ THE MESSAGE IS DRAWN ON THE NEXT CALL-QUEUE PASS, NEVER HERE. On a listen host the refusal
	//! lands inside BuildEntries (a pull) or inside OnAccept (a checkout), and both sit inside a base
	//! call that clears every persistent message before it returns - RefreshCheckout() over the emptied
	//! cart, then Accept()'s own "#OVT-Transfer_Accepted". Drawing synchronously means the player is
	//! told the order succeeded when the server refused it.
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
	//! A checkout finished. Scheduled, never immediate - see OnAccept.
	//! \param[in] moved How many items were moved.
	//! \param[in] shortfall How many were asked for and did not arrive.
	//! \param[in] earned Money paid out; always 0 on this screen.
	protected void OnBatchResult(int moved, int shortfall, int earned)
	{
		if(!m_bIsActive || !m_wRoot) return;

		ScheduleLiveRefresh();
	}

	//------------------------------------------------------------------------------------------------
	//! The holder's replicated count changed - by this player's own batch, or by another player's.
	//! \param[in] storage The component that changed.
	protected void OnHolderCountChanged(OVT_StorageComponent storage)
	{
		if(!m_bIsActive || !m_wRoot) return;
		if(storage != m_SubscribedStorage) return;

		ScheduleLiveRefresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Coalesces a burst of count changes into one re-pull. Remove-before-CallLater keeps exactly one
	//! pending callback, so a 6-line checkout costs one round trip and not six.
	protected void ScheduleLiveRefresh()
	{
		GetGame().GetCallqueue().Remove(LiveRefresh);
		GetGame().GetCallqueue().CallLater(LiveRefresh, LIVE_REFRESH_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! The coalesced re-pull. Re-arms the latch so BuildEntries asks the server again; Refresh()
	//! reconciles the cart and restores focus, so no selection repair is needed here.
	protected void LiveRefresh()
	{
		if(!m_bIsActive || !m_wRoot) return;

		m_bWantPull = true;

		Refresh();
	}

	//-----------------------------------------------------------------------
	// SUBSCRIPTIONS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Every subscription here has a matching Remove in Unsubscribe(). Both targets outlive the
	//! layout, so a missed Remove is an extra redraw per holder for the rest of the session.
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

		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(m_Holder);
		if(storage)
		{
			m_SubscribedStorage = storage;
			m_SubscribedStorage.GetOnCountChanged().Insert(OnHolderCountChanged);
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

		if(m_SubscribedStorage)
			m_SubscribedStorage.GetOnCountChanged().Remove(OnHolderCountChanged);

		m_SubscribedRequests = null;
		m_SubscribedStorage = null;
	}

	//-----------------------------------------------------------------------
	// HELPERS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \return The local player's storage request component, or null.
	protected OVT_StorageRequestComponent GetRequests()
	{
		if(m_SubscribedRequests) return m_SubscribedRequests;

		return OVT_ControllerComponent<OVT_StorageRequestComponent>.Get();
	}

	//------------------------------------------------------------------------------------------------
	//! Insertion sort by distance, ascending and stable.
	//! \param[in] holders The entities to order.
	//! \param[in] origin The point to measure from.
	//! \param[out] sorted Receives the ordered entities; cleared first.
	protected void SortByDistance(array<IEntity> holders, vector origin, out array<IEntity> sorted)
	{
		sorted.Clear();
		if(!holders) return;

		foreach(IEntity holder : holders)
		{
			if(!holder) continue;

			float distance = vector.Distance(holder.GetOrigin(), origin);

			int insertAt = sorted.Count();
			for(int i = 0; i < sorted.Count(); i++)
			{
				if(vector.Distance(sorted[i].GetOrigin(), origin) > distance)
				{
					insertAt = i;
					break;
				}
			}

			sorted.InsertAt(holder, insertAt);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Localized display name for a prefab, memoised for the life of this context. Translated, not
	//! raw: the sort must order by what the player reads, not by "#AR-Item_..." keys.
	//! \param[in] res The prefab to resolve.
	//! \return A non-empty, sortable name.
	protected string ResolveDisplayName(string res)
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
}
