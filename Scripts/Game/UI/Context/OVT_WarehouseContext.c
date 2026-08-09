//------------------------------------------------------------------------------------------------
//! The warehouse menu: what one warehouse holds, with Take 1 / 10 / 100 / All into the vehicle the
//! player is sitting in.
//!
//! The take buttons are wired one handler per widget (BUG-081 was three buttons stacked on one
//! handler, so every button took 1). Adding a button here means adding its OWN handler and removing
//! that same handler in OnClose - never inserting a second handler on an existing button.
//------------------------------------------------------------------------------------------------
class OVT_WarehouseContext : OVT_UIContext
{
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Inventory Item Layout", params: "layout")]
	ResourceName m_ItemLayout;

	//! Coalescing delay for warehouse stock changes. A Take All streams one update per resource line,
	//! so a refresh per event would rebuild the whole list several times in one tick.
	protected const int STOCK_REFRESH_MS = 50;

	protected OVT_WarehouseData m_Warehouse;
	protected int m_SelectedResource = -1;
	protected ResourceName m_SelectedResourceName;

	//! Resource name -> localized display name. UIInfo resolution loads and scans a prefab container,
	//! far too expensive to repeat per row per refresh; names never change, so one lookup each is enough.
	protected ref map<string, string> m_mDisplayNames = new map<string, string>();

	//! Handlers cached at show time so OnClose removes exactly what OnShow inserted.
	protected SCR_InputButtonComponent m_TakeOneAction;
	protected SCR_InputButtonComponent m_TakeTenAction;
	protected SCR_InputButtonComponent m_TakeHundredAction;
	protected SCR_InputButtonComponent m_TakeAllAction;
	protected SCR_InputButtonComponent m_CloseAction;

	//! The real-estate manager this context subscribed to. It outlives the menu, so a missed Remove is
	//! one extra Refresh per warehouse visit for the rest of the session.
	protected OVT_RealEstateManagerComponent m_SubscribedRealEstate;

	//------------------------------------------------------------------------------------------------
	override void OnShow()
	{
		// One handler per button, each on its OWN widget - see the class comment on BUG-081.
		m_TakeOneAction = FindButton("Take1Button");
		if(m_TakeOneAction) m_TakeOneAction.m_OnActivated.Insert(TakeOne);

		m_TakeTenAction = FindButton("Take10Button");
		if(m_TakeTenAction) m_TakeTenAction.m_OnActivated.Insert(TakeTen);

		m_TakeHundredAction = FindButton("Take100Button");
		if(m_TakeHundredAction) m_TakeHundredAction.m_OnActivated.Insert(TakeHundred);

		m_TakeAllAction = FindButton("TakeAllButton");
		if(m_TakeAllAction) m_TakeAllAction.m_OnActivated.Insert(TakeAll);

		Widget closeButton = m_wRoot.FindAnyWidget("CloseButton");
		if(closeButton)
		{
			m_CloseAction = SCR_InputButtonComponent.Cast(closeButton.FindHandler(SCR_InputButtonComponent));
			if(m_CloseAction) m_CloseAction.m_OnClicked.Insert(CloseLayout);
		}

		SubscribeWarehouseChanges();

		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Tears down everything OnShow inserted. The widget handlers go with the rebuilt layout, but the
	//! warehouse invoker lives on a manager that outlives this menu.
	override void OnClose()
	{
		GetGame().GetCallqueue().Remove(RefreshSelected);

		if(m_TakeOneAction) m_TakeOneAction.m_OnActivated.Remove(TakeOne);
		if(m_TakeTenAction) m_TakeTenAction.m_OnActivated.Remove(TakeTen);
		if(m_TakeHundredAction) m_TakeHundredAction.m_OnActivated.Remove(TakeHundred);
		if(m_TakeAllAction) m_TakeAllAction.m_OnActivated.Remove(TakeAll);
		if(m_CloseAction) m_CloseAction.m_OnClicked.Remove(CloseLayout);

		if(m_SubscribedRealEstate && m_SubscribedRealEstate.m_OnWarehouseInventoryChanged)
			m_SubscribedRealEstate.m_OnWarehouseInventoryChanged.Remove(OnWarehouseInventoryChanged);

		m_TakeOneAction = null;
		m_TakeTenAction = null;
		m_TakeHundredAction = null;
		m_TakeAllAction = null;
		m_CloseAction = null;
		m_SubscribedRealEstate = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Finds one take button's input component. A stale layout degrades to a missing button rather than
	//! a script error.
	//! \param[in] name Widget name in WarehouseMenu.layout.
	//! \return The button's input component, or null when the layout has no such button.
	protected SCR_InputButtonComponent FindButton(string name)
	{
		if(!m_wRoot) return null;

		Widget w = m_wRoot.FindAnyWidget(name);
		if(!w) return null;

		return SCR_InputButtonComponent.Cast(w.FindHandler(SCR_InputButtonComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Listens for warehouse stock changes, so a take redraws when the server's number lands rather
	//! than optimistically right after the async ask.
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

		ScheduleRefresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Coalesces a burst of stock changes into one redraw. Remove-before-CallLater keeps exactly one
	//! pending callback, so a Take All of six resource lines costs one Refresh, not six.
	protected void ScheduleRefresh()
	{
		GetGame().GetCallqueue().Remove(RefreshSelected);
		GetGame().GetCallqueue().CallLater(RefreshSelected, STOCK_REFRESH_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the list and re-reads the details pane for the row the player had selected.
	void RefreshSelected()
	{
		if(!m_bIsActive || !m_wRoot) return;

		// A Take All empties its row out of the list entirely. Dropping the selection here lets Refresh
		// land on the first surviving row instead of leaving the details pane on a row that is gone.
		if(m_Warehouse && !m_SelectedResourceName.IsEmpty())
		{
			bool stillStocked = m_Warehouse.inventory.Contains(m_SelectedResourceName);
			if(stillStocked && m_Warehouse.inventory[m_SelectedResourceName] == 0) stillStocked = false;

			if(!stillStocked)
			{
				m_SelectedResource = -1;
				m_SelectedResourceName = ResourceName.Empty;
			}
		}

		Refresh();

		if(!m_SelectedResourceName.IsEmpty()) SelectItem(m_SelectedResourceName);
	}

	void Refresh()
	{
		if(!m_Warehouse) return;
		if(!m_wRoot) return;

		Widget container = m_wRoot.FindAnyWidget("Inventory");
		if(!container) return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();

		int wi = 0;

		while(container.GetChildren())
			container.RemoveChild(container.GetChildren());

		// Alphabetical by what the player READS, so the list is scannable instead of following map
		// insertion order (Phase 7.2).
		array<string> ids = new array<string>();
		CollectSortedIds(ids);

		foreach(string id : ids)
		{
			int qty = m_Warehouse.inventory[id];

			if(wi == 0 && m_SelectedResource == -1){
				SelectItem(id);
			}

			Widget ww = workspace.CreateWidgets(m_ItemLayout, container);
			if(!ww) continue;

			OVT_WarehouseInventoryItemComponent card = OVT_WarehouseInventoryItemComponent.Cast(ww.FindHandler(OVT_WarehouseInventoryItemComponent));
			if(!card) continue;

			card.Init(id, qty, this);

			wi++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Lists the warehouse's non-empty resource ids, ordered alphabetically by display name.
	//!
	//! Insertion sort with a strictly-greater case-insensitive comparison, the same idiom (and the same
	//! stability guarantee) OVT_ShopBrowserModel.SortByDisplayName uses. The model itself is not reused
	//! because these rows are resource-name keyed strings, not browser items.
	//! \param[out] ids Receives the sorted resource name strings. Cleared first.
	protected void CollectSortedIds(out array<string> ids)
	{
		if(!ids) return;

		ids.Clear();

		for(int i = 0; i < m_Warehouse.inventory.Count(); i++)
		{
			string id = m_Warehouse.inventory.GetKey(i);
			if(m_Warehouse.inventory[id] == 0) continue;

			string name = ResolveDisplayName(id);

			int j = ids.Count() - 1;
			ids.Insert(id);

			while(j >= 0 && ResolveDisplayName(ids[j]).Compare(name, false) > 0)
			{
				ids[j + 1] = ids[j];
				j--;
			}

			ids[j + 1] = id;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Localized display name for a resource, memoised for the life of this context.
	//! \param[in] res The prefab to resolve.
	//! \return A non-empty, sortable name.
	protected string ResolveDisplayName(string res)
	{
		if(m_mDisplayNames.Contains(res)) return m_mDisplayNames[res];

		string name = "";

		UIInfo info = OVT_Global.GetItemUIInfo(res);
		if(info) name = info.GetName();

		if(name == "") name = res;

		string translated = WidgetManager.Translate(name);
		if(translated != "") name = translated;

		m_mDisplayNames[res] = name;
		return name;
	}

	override void SelectItem(ResourceName res)
	{
		if(!m_wRoot) return;

		int id = m_Economy.GetInventoryId(res);
		m_SelectedResource = id;
		m_SelectedResourceName = res;
		TextWidget typeName = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedTypeName"));
		ItemPreviewWidget img = ItemPreviewWidget.Cast(m_wRoot.FindAnyWidget("SelectedTypeImage"));
		TextWidget details = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDetails"));
		TextWidget desc = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDescription"));
		if(img) img.SetResolutionScale(1, 1);

		UIInfo info = OVT_Global.GetItemUIInfo(res);
		if(info)
		{
			if(typeName) typeName.SetText(info.GetName());
			if(details) details.SetText(OVT_MoneyFormat.FormatMoney(m_Economy.GetPrice(id)));
			if(desc) desc.SetText(info.GetDescription());
		}

		ChimeraWorld world = GetGame().GetWorld();
		ItemPreviewManagerEntity manager = world.GetItemPreviewManager();
		if (!manager)
			return;

		if(img) manager.SetPreviewItemFromPrefab(img, res);
	}

	void SetWarehouse(OVT_WarehouseData warehouse)
	{
		m_Warehouse = warehouse;
	}

	override void RegisterInputs()
	{
		super.RegisterInputs();
		if(!m_InputManager) return;

		m_InputManager.AddActionListener("MenuBack", EActionTrigger.DOWN, CloseLayout);
	}

	override void UnregisterInputs()
	{
		super.UnregisterInputs();
		if(!m_InputManager) return;

		m_InputManager.RemoveActionListener("MenuBack", EActionTrigger.DOWN, CloseLayout);
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the server to move up to qty of the selected resource into the player's vehicle.
	//!
	//! Deliberately does NOT refresh here: the ask is asynchronous, and the pre-Phase-7 immediate
	//! Refresh() redrew the quantity the warehouse had BEFORE the take. The redraw now happens when the
	//! server's new quantity arrives on m_OnWarehouseInventoryChanged.
	//! \param[in] qty Requested amount. Clamped to what the warehouse currently holds.
	void Take(int qty)
	{
		if(!m_Warehouse) return;
		if(m_SelectedResourceName.IsEmpty()) return;
		if(!m_Warehouse.inventory.Contains(m_SelectedResourceName)) return;

		if(m_Warehouse.inventory[m_SelectedResourceName] < qty)
		{
			qty = m_Warehouse.inventory[m_SelectedResourceName];
		}
		if(qty <= 0) return;

		SCR_CompartmentAccessComponent compartment = SCR_CompartmentAccessComponent.Cast(m_Owner.FindComponent(SCR_CompartmentAccessComponent));
		if(!compartment) return;

		IEntity entity = compartment.GetVehicle();
		if(!entity) return;

		OVT_Global.GetServer().TakeFromWarehouseToVehicle(m_Warehouse.id, m_SelectedResourceName, qty, entity);
	}

	void TakeOne(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		Take(1);
	}

	void TakeTen(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		Take(10);
	}

	void TakeHundred(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		Take(100);
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the whole selected row. Take() clamps to what the warehouse holds, and the server clamps
	//! again to what actually fits in the vehicle, so the row's full quantity is a safe request.
	void TakeAll(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(!m_Warehouse) return;
		if(m_SelectedResourceName.IsEmpty()) return;
		if(!m_Warehouse.inventory.Contains(m_SelectedResourceName)) return;

		Take(m_Warehouse.inventory[m_SelectedResourceName]);
	}
}
