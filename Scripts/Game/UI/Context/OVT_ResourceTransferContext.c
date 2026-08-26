//------------------------------------------------------------------------------------------------
//! Resource transfer: Take out of the holder you opened, or Put into it from a holder beside it.
//!
//! A logistics/ui consumer and nothing else - the eight hooks plus the GetSummaryText() override the
//! base reserved for a consumer whose value column is a volume. No widget code lives here.
//!
//! THE LIST IS NOT ASYNC. Unlike OVT_StorageContext, a resource ledger REPLICATES (one packed
//! RplProp string), so BuildEntries reads local state: no pull, no latch on open, no empty first
//! frame. The only latch in this class guards a checkout in flight.
//!
//! VOCABULARY IS TAKE / PUT with the opened holder as the anchor (D8). Take: source = the holder you
//! opened, receiver = the picked destination (a nearby holder, or the ground). Put: source = the
//! picked holder, receiver = the one you opened. BuildEntries always lists the SOURCE; ValidateCart
//! always measures the RECEIVER.
//------------------------------------------------------------------------------------------------
class OVT_ResourceTransferContext : OVT_TransferContext
{
	//! The two modes, in BuildModes order.
	static const int MODE_TAKE = 0;
	static const int MODE_PUT = 1;

	//! Entry-id prefix. A resource id can never collide with a ResourceName, which always starts "{".
	static const string RES_PREFIX = "res:";

	//! Destination id of the synthetic "drop it here" entry.
	static const string DEST_GROUND = "ground";

	//! Coalescing delay for a contents change, matching OVT_StorageContext.
	protected const int LIVE_REFRESH_MS = 250;

	//! The holder this screen was opened on, and its networked name.
	protected IEntity m_Holder;
	protected RplId m_HolderId;

	//! The store whose contents invoker this screen subscribed to. Cached because it outlives the
	//! layout and OnClose must remove exactly what OnShow inserted.
	protected OVT_ResourceStoreComponent m_SubscribedStore;

	//! The request component this screen subscribed to, for the same reason.
	protected OVT_ResourceRequestComponent m_SubscribedRequests;

	//! The picker this screen subscribed to. The base only re-validates on a picker change, but in Put
	//! mode the picked holder IS the source, so the list has to be rebuilt as well.
	protected SCR_SpinBoxComponent m_SubscribedSpinBox;

	//! THE LATCH. True between an opened checkout and its single reply; set before the ask, because on
	//! a listen host the whole reply fan runs inside it.
	protected bool m_bCheckoutPending;

	//! The refusal waiting to be drawn. Empty when there is none.
	protected string m_sPendingError;

	//------------------------------------------------------------------------------------------------
	//! An RplId member is an engine handle, not an int - it is given a value here so IsValid() is
	//! answerable before any holder has been set.
	void OVT_ResourceTransferContext()
	{
		m_HolderId = RplId.Invalid();
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the holder before ShowContext, the OVT_StorageContext shape.
	//! \param[in] holder The entity whose store this screen is anchored on.
	void SetHolder(IEntity holder)
	{
		m_Holder = holder;
		m_HolderId = OVT_ResourceUtils.GetHolderId(holder);
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
	//! Subscribes BEFORE super.OnShow(), which draws the first frame.
	override void OnShow()
	{
		m_bCheckoutPending = false;
		m_sPendingError = "";

		// The holder may have been re-created since SetHolder (a re-open after a respawn).
		if(m_Holder) m_HolderId = OVT_ResourceUtils.GetHolderId(m_Holder);

		Subscribe();

		super.OnShow();

		// The picker is wired by super.OnShow()'s WireWidgets, so this can only be added after it.
		SubscribeDestinationPicker();

		// Refresh() builds the entries BEFORE the destinations, so the first pass had no receiver to
		// measure rows against. One more pass applies the fit flags with the picker populated.
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes exactly what OnShow inserted: the store's contents invoker, both request invokers, the
	//! picker subscription and both pending callbacks. All of them outlive the layout.
	override void OnClose()
	{
		GetGame().GetCallqueue().Remove(LiveRefresh);
		GetGame().GetCallqueue().Remove(ShowResourceError);

		UnsubscribeDestinationPicker();
		Unsubscribe();

		super.OnClose();

		m_bCheckoutPending = false;
		m_sPendingError = "";
	}

	//-----------------------------------------------------------------------
	// THE EIGHT HOOKS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	override void BuildModes(out array<int> modes, out array<string> labelKeys)
	{
		modes.Clear();
		labelKeys.Clear();

		modes.Insert(MODE_TAKE);
		labelKeys.Insert("#OVT-Resource_Take");

		modes.Insert(MODE_PUT);
		labelKeys.Insert("#OVT-Resource_Put");
	}

	//------------------------------------------------------------------------------------------------
	//! One row per non-zero line of the SOURCE's replicated ledger. Contents replicate, so this is a
	//! local read with nothing to wait for.
	override void BuildEntries(int mode, OVT_TransferListModel model)
	{
		if(!model) return;

		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if(!resources) return;

		OVT_ResourceDefs defs = resources.GetDefs();
		if(!defs) return;

		OVT_ResourceStoreComponent source = OVT_ResourceUtils.GetStore(ResolveSource(mode));
		if(!source) return;

		OVT_ResourceLedger ledger = source.GetLedger();
		if(!ledger) return;

		int freeLitres = ReceiverFreeLitres(mode);

		array<string> ids = new array<string>();
		array<int> quantities = new array<int>();
		ledger.GetLines(ids, quantities);

		for(int i = 0; i < ids.Count(); i++)
		{
			string id = ids[i];
			int qty = quantities[i];
			if(qty <= 0) continue;

			OVT_TransferEntry entry = new OVT_TransferEntry();
			entry.m_sId = RES_PREFIX + id;
			entry.m_sDisplayName = ResolveDisplayName(id);
			entry.m_eImageKind = EOVT_TransferImageKind.TEXTURE;
			entry.m_sImage = ResolveIcon(id);
			entry.m_iValue = qty;
			entry.m_eValueKind = EOVT_TransferValueKind.QUANTITY;
			entry.m_iMaxQuantity = qty;
			entry.m_iCategoryId = 0;
			entry.m_bEnabled = true;
			entry.m_sDisabledReasonKey = "";

			if(freeLitres < defs.LitresPerUnit(id))
			{
				entry.m_bEnabled = false;
				entry.m_sDisabledReasonKey = "#OVT-Resource_NoCargoSpace";
			}

			model.Add(entry);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One category, so no tab row.
	override string GetCategoryLabelKey(int categoryId)
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Take: every other usable holder within the request component's picker radius, nearest first,
	//! then the ground. Put: the same holders without the ground.
	//!
	//! Nearest-first rather than query order, which is spatially arbitrary and would reshuffle the
	//! picker under the player's selection between two refreshes that found the same holders. The
	//! ground goes LAST so a real holder keeps its index when the mode changes.
	override void BuildDestinations(out array<ref OVT_TransferDestination> dests)
	{
		dests.Clear();

		if(!m_Holder) return;

		OVT_ResourceRequestComponent requests = GetRequests();
		if(!requests) return;

		array<IEntity> holders = new array<IEntity>();

		// One query object per call; the shared-accumulator singleton is the defect this feature
		// exists to stop repeating (OVT_InventoryManagerComponent:497).
		OVT_ResourceHolderQuery query = new OVT_ResourceHolderQuery();
		query.Run(m_Holder.GetOrigin(), requests.GetHolderRadius(), holders);

		array<IEntity> sorted = new array<IEntity>();
		SortByDistance(holders, m_Holder.GetOrigin(), sorted);

		int index = 0;
		foreach(IEntity holder : sorted)
		{
			if(holder == m_Holder) continue;

			OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(holder);
			if(!store) continue;

			RplId id = OVT_ResourceUtils.GetHolderId(holder);
			if(!id.IsValid()) continue;

			OVT_TransferDestination dest = new OVT_TransferDestination();
			dest.m_sId = "holder" + index.ToString();
			dest.m_sLabel = store.GetDisplayName();
			dest.m_Entity = holder;

			dests.Insert(dest);
			index++;
		}

		if(m_iMode != MODE_TAKE) return;

		// A REAL entity, deliberately: the base never sees a null destination, and the server spawns
		// the pile at the caller's own position anyway (ResolveDropPosition).
		if(!m_Owner) return;

		OVT_TransferDestination ground = new OVT_TransferDestination();
		ground.m_sId = DEST_GROUND;
		ground.m_sLabel = "#OVT-Resource_DestGround";
		ground.m_Entity = m_Owner;

		dests.Insert(ground);
	}

	//------------------------------------------------------------------------------------------------
	override void FillDetails(OVT_TransferEntry entry, out string name, out string value, out string body)
	{
		name = entry.m_sDisplayName;
		value = FormatValue(entry.m_iValue, entry.m_eValueKind);
		body = "";

		string id = ResourceIdOf(entry.m_sId);

		OVT_Resource res = FindResource(id);
		if(res && res.m_sDescription != "") body = WidgetManager.Translate(res.m_sDescription);

		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if(!resources) return;

		OVT_ResourceDefs defs = resources.GetDefs();
		if(!defs) return;

		int defIndex = defs.IndexOf(id);
		if(defIndex < 0) return;

		float cubicMetres = defs.LitresAt(defIndex) / 1000.0;

		string perUnit = WidgetManager.Translate("#OVT-Resource_PerUnit",
			cubicMetres.ToString(-1, 2), defs.KgAt(defIndex).ToString(-1, 1));

		if(body == "")
		{
			body = perUnit;
			return;
		}

		body = body + "\n" + perUnit;
	}

	//------------------------------------------------------------------------------------------------
	//! Both directions move goods the player may already reach, so a whole line is one press.
	override bool IsAddAllAllowed(int mode)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whole-cart fit (D1): nothing ever clamps, here or on the server.
	//! \param[in] lines The cart's lines.
	//! \param[in] dest The picker's selection - the receiver in Take, the SOURCE in Put.
	//! \return "" when the cart may be committed, otherwise an #OVT- key or an already-resolved reason.
	override string ValidateCart(array<ref OVT_TransferCartLine> lines, OVT_TransferDestination dest)
	{
		if(!dest || !dest.m_Entity) return "#OVT-Resource_NoDestination";

		// The ground is a pile that does not exist yet; a pile is unlimited, so a drop always fits.
		if(dest.m_sId == DEST_GROUND) return "";

		OVT_ResourceStoreComponent receiver = OVT_ResourceUtils.GetStore(ResolveReceiver(m_iMode));
		if(!receiver) return "#OVT-Resource_NoDestination";

		int freeLitres = receiver.GetFreeLitres();
		int cartLitres = CartLitres(lines);

		if(cartLitres <= freeLitres) return "";

		return WidgetManager.Translate("#OVT-Resource_NoSpaceVolume",
			FormatCubicMetres(cartLitres), FormatCubicMetres(freeLitres));
	}

	//------------------------------------------------------------------------------------------------
	//! ONE CHECKOUT, NOT ONE REQUEST PER LINE: TransferBegin, one TransferLine per line, TransferCommit.
	//!
	//! ⚠ On a listen host the entire checkout runs inside this method - every ask invokes its handler
	//! directly, so the move and the reply have both happened before it returns. THE LATCH IS SET
	//! BEFORE THE FIRST ASK for exactly that reason; setting it after would latch over a reply that
	//! already cleared it.
	//! \param[in] lines The cart's lines.
	//! \param[in] dest The picker's selection.
	override void OnAccept(array<ref OVT_TransferCartLine> lines, OVT_TransferDestination dest)
	{
		if(!lines || lines.IsEmpty()) return;
		if(!dest || !dest.m_Entity) return;
		if(!m_HolderId.IsValid()) return;
		if(m_bCheckoutPending) return;

		OVT_ResourceRequestComponent requests = GetRequests();
		if(!requests) return;

		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if(!resources) return;

		OVT_ResourceDefs defs = resources.GetDefs();
		if(!defs) return;

		RplId sourceId = m_HolderId;
		RplId destId = m_HolderId;
		int opKind = EOVT_ResourceOp.HOLDER_TO_HOLDER;

		if(m_iMode == MODE_PUT)
		{
			sourceId = OVT_ResourceUtils.GetHolderId(ResolveSource(MODE_PUT));
		}
		else if(dest.m_sId == DEST_GROUND)
		{
			// Both slots always carry a valid holder; the op kind decides which is read.
			opKind = EOVT_ResourceOp.HOLDER_TO_GROUND;
		}
		else
		{
			destId = OVT_ResourceUtils.GetHolderId(dest.m_Entity);
		}

		if(!sourceId.IsValid() || !destId.IsValid())
		{
			ShowPersistentMessage("#OVT-Resource_NoStore");
			return;
		}

		m_bCheckoutPending = true;

		int seq = requests.RequestTransferBegin(sourceId, destId, opKind, lines.Count());
		if(seq == OVT_ResourceRequestComponent.SEQ_NONE)
		{
			m_bCheckoutPending = false;
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
	//! The volume readout the base reserved this override for: what the receiver will hold once the
	//! cart lands. An unlimited receiver - a pile, a warehouse, the ground - has no denominator.
	//! \return Already-resolved text, empty on an empty cart.
	override string GetSummaryText()
	{
		if(m_Cart.Count() == 0) return "";

		int cartLitres = CartLitres(m_Cart.GetLines());

		OVT_TransferDestination dest = GetSelectedDestination();
		if(dest && dest.m_sId == DEST_GROUND)
			return WidgetManager.Translate("#OVT-Resource_SummaryUnlimited", FormatCubicMetres(cartLitres));

		OVT_ResourceStoreComponent receiver = OVT_ResourceUtils.GetStore(ResolveReceiver(m_iMode));
		if(!receiver)
			return WidgetManager.Translate("#OVT-Resource_SummaryUnlimited", FormatCubicMetres(cartLitres));

		int capacity = receiver.GetCapacityLitres();
		if(capacity == OVT_ResourceStoreComponent.UNLIMITED_CAPACITY)
			return WidgetManager.Translate("#OVT-Resource_SummaryUnlimited", FormatCubicMetres(cartLitres));

		return WidgetManager.Translate("#OVT-Resource_SummaryVolume",
			FormatCubicMetres(receiver.GetUsedLitres() + cartLitres), FormatCubicMetres(capacity));
	}

	//-----------------------------------------------------------------------
	// SOURCE AND RECEIVER
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \param[in] mode MODE_TAKE or MODE_PUT.
	//! \return The holder the rows are listed from, or null.
	protected IEntity ResolveSource(int mode)
	{
		if(mode != MODE_PUT) return m_Holder;

		return PickedHolder();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] mode MODE_TAKE or MODE_PUT.
	//! \return The holder the cart lands in, or null when it is the ground or unknown.
	protected IEntity ResolveReceiver(int mode)
	{
		if(mode == MODE_PUT) return m_Holder;

		OVT_TransferDestination dest = GetSelectedDestination();
		if(!dest) return null;
		if(dest.m_sId == DEST_GROUND) return null;

		return dest.m_Entity;
	}

	//------------------------------------------------------------------------------------------------
	//! The picked destination as a real store-bearing holder.
	//!
	//! Refresh() builds the entries BEFORE the destinations, so on the frame the mode changes to Put
	//! the selection may still point at the ground entry Take offered. Falling back to the LAST
	//! store-bearing entry reproduces exactly what RefreshDestinations is about to clamp the index to.
	//! \return The picked holder, or null when the picker offers none.
	protected IEntity PickedHolder()
	{
		OVT_TransferDestination dest = GetSelectedDestination();
		if(dest && dest.m_sId != DEST_GROUND && dest.m_Entity) return dest.m_Entity;

		for(int i = m_aDestinations.Count() - 1; i >= 0; i--)
		{
			OVT_TransferDestination candidate = m_aDestinations[i];
			if(!candidate || candidate.m_sId == DEST_GROUND) continue;
			if(candidate.m_Entity) return candidate.m_Entity;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] mode MODE_TAKE or MODE_PUT.
	//! \return Litres the receiver can still take; int.MAX for the ground and for an unlimited holder.
	protected int ReceiverFreeLitres(int mode)
	{
		OVT_ResourceStoreComponent receiver = OVT_ResourceUtils.GetStore(ResolveReceiver(mode));
		if(!receiver) return int.MAX;

		return receiver.GetFreeLitres();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] lines The cart's lines.
	//! \return The whole cart's volume in litres.
	protected int CartLitres(array<ref OVT_TransferCartLine> lines)
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

	//-----------------------------------------------------------------------
	// LIVE REFRESH
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The holder's replicated contents changed - by this player's own checkout, or by another's.
	//! \param[in] store The store that changed.
	protected void OnContentsChanged(OVT_ResourceStoreComponent store)
	{
		if(!m_bIsActive || !m_wRoot) return;

		ScheduleLiveRefresh();
	}

	//------------------------------------------------------------------------------------------------
	//! A checkout finished.
	//! \param[in] movedLitres Litres that moved.
	//! \param[in] earned Money paid to the player; always 0 on this screen.
	//! \param[in] spent Money charged to the player; always 0 on this screen.
	protected void OnTransferResult(int movedLitres, int earned, int spent)
	{
		m_bCheckoutPending = false;

		if(!m_bIsActive || !m_wRoot) return;

		ScheduleLiveRefresh();
	}

	//------------------------------------------------------------------------------------------------
	//! A request was refused.
	//!
	//! ⚠ THE MESSAGE IS DRAWN ON THE NEXT CALL-QUEUE PASS, NEVER HERE. On a listen host the refusal
	//! lands inside OnAccept, which sits inside the base's Accept() - and Accept() prints its own
	//! "order placed" message after OnAccept returns. Drawing synchronously tells the player the order
	//! succeeded when the server refused it.
	//! \param[in] messageKey Localization key naming the refusal.
	protected void OnResourceError(string messageKey)
	{
		m_bCheckoutPending = false;

		if(!m_bIsActive || !m_wRoot) return;

		m_sPendingError = messageKey;

		GetGame().GetCallqueue().Remove(ShowResourceError);
		GetGame().GetCallqueue().CallLater(ShowResourceError, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Draws the refusal once the base call that carried it has unwound.
	protected void ShowResourceError()
	{
		if(!m_bIsActive || !m_wRoot) return;
		if(m_sPendingError == "") return;

		ShowPersistentMessage(m_sPendingError);
		m_sPendingError = "";
	}

	//------------------------------------------------------------------------------------------------
	//! Coalesces a burst of contents changes into one redraw. Remove-before-CallLater keeps exactly
	//! one pending callback, so a six-line checkout costs one rebuild and not six.
	protected void ScheduleLiveRefresh()
	{
		GetGame().GetCallqueue().Remove(LiveRefresh);
		GetGame().GetCallqueue().CallLater(LiveRefresh, LIVE_REFRESH_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! The coalesced redraw. Refresh() reconciles the cart and restores focus by itself.
	protected void LiveRefresh()
	{
		if(!m_bIsActive || !m_wRoot) return;

		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! The picker moved. In Put mode the picked holder is the SOURCE, so the row set changes with it;
	//! the base only re-validates the cart, which is all its own consumers need.
	//! \param[in] spinbox The picker.
	//! \param[in] index The newly selected destination index.
	protected void OnPickerChanged(SCR_SpinBoxComponent spinbox, int index)
	{
		// ClearAll / AddItem both fire m_OnChanged while the picker is being filled. Without this the
		// refresh below would re-enter the refresh that is building the picker, forever.
		if(m_bRebuildingDestinations) return;
		if(!m_bIsActive || !m_wRoot) return;

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
		OVT_ResourceRequestComponent requests = OVT_ControllerComponent<OVT_ResourceRequestComponent>.Get();
		if(requests)
		{
			m_SubscribedRequests = requests;
			m_SubscribedRequests.GetOnTransferResult().Insert(OnTransferResult);
			m_SubscribedRequests.GetOnResourceError().Insert(OnResourceError);
		}

		OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(m_Holder);
		if(store)
		{
			m_SubscribedStore = store;
			m_SubscribedStore.GetOnContentsChanged().Insert(OnContentsChanged);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Unsubscribe()
	{
		if(m_SubscribedRequests)
		{
			m_SubscribedRequests.GetOnTransferResult().Remove(OnTransferResult);
			m_SubscribedRequests.GetOnResourceError().Remove(OnResourceError);
		}

		if(m_SubscribedStore)
			m_SubscribedStore.GetOnContentsChanged().Remove(OnContentsChanged);

		m_SubscribedRequests = null;
		m_SubscribedStore = null;
	}

	//------------------------------------------------------------------------------------------------
	//! The picker's own invoker, added after WireWidgets has found it.
	protected void SubscribeDestinationPicker()
	{
		if(m_SubscribedSpinBox) return;
		if(!m_DestinationSpinBox || !m_DestinationSpinBox.m_OnChanged) return;

		m_SubscribedSpinBox = m_DestinationSpinBox;
		m_SubscribedSpinBox.m_OnChanged.Insert(OnPickerChanged);
	}

	//------------------------------------------------------------------------------------------------
	//! Removed before super.OnClose(), which nulls the base's own handle to the same component.
	protected void UnsubscribeDestinationPicker()
	{
		if(!m_SubscribedSpinBox) return;

		if(m_SubscribedSpinBox.m_OnChanged)
			m_SubscribedSpinBox.m_OnChanged.Remove(OnPickerChanged);

		m_SubscribedSpinBox = null;
	}

	//-----------------------------------------------------------------------
	// HELPERS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \return The local player's resource request component, or null.
	protected OVT_ResourceRequestComponent GetRequests()
	{
		if(m_SubscribedRequests) return m_SubscribedRequests;

		return OVT_ControllerComponent<OVT_ResourceRequestComponent>.Get();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entryId A row or cart-line id.
	//! \return The bare resource id behind it.
	protected string ResourceIdOf(string entryId)
	{
		if(!entryId.StartsWith(RES_PREFIX)) return entryId;

		return entryId.Substring(RES_PREFIX.Length(), entryId.Length() - RES_PREFIX.Length());
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
	protected string ResolveDisplayName(string id)
	{
		return OVT_ResourceUtils.ResolveResourceTitle(id);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id A bare resource id.
	//! \return The row icon, or "" when the catalogue authors none (the base then draws no image).
	protected ResourceName ResolveIcon(string id)
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
}
