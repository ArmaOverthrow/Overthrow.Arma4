//------------------------------------------------------------------------------------------------
//! The warehouse Take screen: one mode, a stock list for one warehouse, and the occupied vehicle as
//! the only destination. All widget work belongs to OVT_TransferContext.
//------------------------------------------------------------------------------------------------
class OVT_WarehouseContext : OVT_TransferContext
{
	//! Coalescing delay for warehouse stock changes. One Accept streams an update per resource line,
	//! so a refresh per event would rebuild the whole list several times in one tick.
	protected const int STOCK_REFRESH_MS = 50;

	protected OVT_WarehouseData m_Warehouse;

	//! Resource name -> localized display name. UIInfo resolution loads and scans a prefab container,
	//! far too expensive to repeat per row per refresh; names never change.
	protected ref map<string, string> m_mDisplayNames = new map<string, string>();

	//! The real-estate manager this context subscribed to. It outlives the menu, so a missed Remove is
	//! one extra Refresh per warehouse visit for the rest of the session.
	protected OVT_RealEstateManagerComponent m_SubscribedRealEstate;

	//------------------------------------------------------------------------------------------------
	//! \param[in] warehouse The warehouse this screen draws. Signature is called by
	//! OVT_VehicleMenuContext and must not change.
	void SetWarehouse(OVT_WarehouseData warehouse)
	{
		m_Warehouse = warehouse;
	}

	//------------------------------------------------------------------------------------------------
	override void OnShow()
	{
		super.OnShow();

		SubscribeWarehouseChanges();
	}

	//------------------------------------------------------------------------------------------------
	override void OnClose()
	{
		GetGame().GetCallqueue().Remove(RefreshStock);

		if(m_SubscribedRealEstate && m_SubscribedRealEstate.m_OnWarehouseInventoryChanged)
			m_SubscribedRealEstate.m_OnWarehouseInventoryChanged.Remove(OnWarehouseInventoryChanged);

		m_SubscribedRealEstate = null;

		super.OnClose();
	}

	//------------------------------------------------------------------------------------------------
	override void BuildModes(out array<int> modes, out array<string> labelKeys)
	{
		modes.Clear();
		labelKeys.Clear();

		modes.Insert(0);
		labelKeys.Insert("#OVT-TakeFromWarehouse");
	}

	//------------------------------------------------------------------------------------------------
	override void BuildEntries(int mode, OVT_TransferListModel model)
	{
		if(!model || !m_Warehouse) return;

		for(int i = 0; i < m_Warehouse.inventory.Count(); i++)
		{
			string id = m_Warehouse.inventory.GetKey(i);

			int stock = m_Warehouse.inventory[id];
			if(stock <= 0) continue;

			OVT_TransferEntry entry = new OVT_TransferEntry();
			entry.m_sId = id;
			entry.m_sDisplayName = ResolveDisplayName(id);
			entry.m_eImageKind = EOVT_TransferImageKind.PREFAB;
			entry.m_sImage = id;
			entry.m_iValue = stock;
			entry.m_eValueKind = EOVT_TransferValueKind.QUANTITY;
			entry.m_iMaxQuantity = stock;
			entry.m_iCategoryId = 0;
			entry.m_bEnabled = true;
			entry.m_sDisabledReasonKey = "";

			model.Add(entry);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Taking from a warehouse the player already owns is free, so a whole stack is one press.
	override bool IsAddAllAllowed(int mode)
	{
		return true;
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
	//! No affordability check - the stock is already the player's.
	override string ValidateCart(array<ref OVT_TransferCartLine> lines, OVT_TransferDestination dest)
	{
		if(!dest || !dest.m_Entity) return "#OVT-Transfer_NoVehicle";

		return "";
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
	//! One existing TakeFromWarehouseToVehicle per line - no batching, no new RPC (D10). Deliberately
	//! does NOT schedule a refresh: the ask is async, so the redraw waits for
	//! m_OnWarehouseInventoryChanged rather than repainting the pre-take quantity (D9).
	override void OnAccept(array<ref OVT_TransferCartLine> lines, OVT_TransferDestination dest)
	{
		if(!lines || !dest || !dest.m_Entity) return;
		if(!m_Warehouse) return;

		OVT_RealEstateRequestComponent realEstate = OVT_ControllerComponent<OVT_RealEstateRequestComponent>.Get();
		if(!realEstate) return;

		foreach(OVT_TransferCartLine line : lines)
		{
			if(line.m_iQuantity <= 0) continue;

			realEstate.TakeFromWarehouseToVehicle(m_Warehouse.id, line.m_sId, line.m_iQuantity, dest.m_Entity);
		}
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
	//! Listens for warehouse stock changes, so a take redraws when the server's number arrives.
	protected void SubscribeWarehouseChanges()
	{
		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if(!realEstate || !realEstate.m_OnWarehouseInventoryChanged) return;

		m_SubscribedRealEstate = realEstate;
		m_SubscribedRealEstate.m_OnWarehouseInventoryChanged.Insert(OnWarehouseInventoryChanged);
	}

	//------------------------------------------------------------------------------------------------
	//! One warehouse stock line changed.
	//! \param[in] warehouseId The warehouse that changed.
	//! \param[in] id Resource name string whose quantity changed.
	//! \param[in] qty The new quantity.
	protected void OnWarehouseInventoryChanged(int warehouseId, string id, int qty)
	{
		if(!m_bIsActive || !m_wRoot) return;
		if(!m_Warehouse || m_Warehouse.id != warehouseId) return;

		ScheduleStockRefresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Coalesces a burst of stock changes into one redraw. Remove-before-CallLater keeps exactly one
	//! pending callback, so an Accept over six lines costs one Refresh, not six. Its own timer rather
	//! than the base's 400 ms ScheduleRefresh, which exists for a consumer with no invoker.
	protected void ScheduleStockRefresh()
	{
		GetGame().GetCallqueue().Remove(RefreshStock);
		GetGame().GetCallqueue().CallLater(RefreshStock, STOCK_REFRESH_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! The coalesced redraw. Refresh() rebuilds the list, reconciles the cart against the new stock
	//! and restores focus, so no selection repair is needed here.
	protected void RefreshStock()
	{
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Localized display name for a resource, memoised for the life of this context.
	//!
	//! Translated rather than raw, because the sort is supposed to be alphabetical by what the player
	//! READS - sorting raw "#AR-Item_..." keys would produce an order with no relation to the list.
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
