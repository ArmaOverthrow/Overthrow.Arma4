class OVT_PortContext : OVT_UIContext
{	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Inventory Item Layout", params: "layout")]
	ResourceName m_ItemLayout;
	
	protected int m_SelectedResource = -1;
	protected ResourceName m_SelectedResourceName;

	//! Prefab -> localized display name. UIInfo resolution loads and scans a prefab container, which is
	//! far too expensive to repeat per row per sort comparison; names never change.
	protected ref map<string, string> m_mDisplayNames = new map<string, string>();

	override void PostInit()
	{
		if(SCR_Global.IsEditMode()) return;
		m_Economy.m_OnPlayerMoneyChanged.Insert(OnPlayerMoneyChanged);
	}
	
	protected void OnPlayerMoneyChanged(string playerId, int amount)
	{
		if(playerId == m_sPlayerID && m_bIsActive)
		{
			TextWidget money = TextWidget.Cast(m_wRoot.FindAnyWidget("PlayerMoney"));
			if(money) money.SetText(OVT_MoneyFormat.FormatMoney(amount));
		}
	}
	
	override void OnShow()
	{	
		Widget ww = m_wRoot.FindAnyWidget("Buy10Button");
		SCR_InputButtonComponent btn = SCR_InputButtonComponent.Cast(ww.FindHandler(SCR_InputButtonComponent));		
		btn.m_OnActivated.Insert(BuyTen);
		
		ww = m_wRoot.FindAnyWidget("Buy100Button");
		btn = SCR_InputButtonComponent.Cast(ww.FindHandler(SCR_InputButtonComponent));		
		btn.m_OnActivated.Insert(BuyHundred);
		
		ww = m_wRoot.FindAnyWidget("CloseButton");
		btn = SCR_InputButtonComponent.Cast(ww.FindHandler(SCR_InputButtonComponent));		
		btn.m_OnActivated.Insert(CloseLayout);		
		
		Refresh();		
	}
	
	void Refresh()
	{
		if(!m_wRoot) return;

		Widget container = m_wRoot.FindAnyWidget("Inventory");
		if(!container) return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();

		TextWidget money = TextWidget.Cast(m_wRoot.FindAnyWidget("PlayerMoney"));
		if(money) money.SetText(OVT_MoneyFormat.FormatMoney(m_Economy.GetPlayerMoney(m_sPlayerID)));

		int wi = 0;

		while(container.GetChildren())
			container.RemoveChild(container.GetChildren());

		// Collect first, sort second, draw third. The importable set is assembled from two unrelated
		// sources (the illegal-imports permission list, or every non-vehicle shop config rule), and the
		// player reads one alphabetical list either way (Phase 7.2).
		array<ResourceName> prefabs = new array<ResourceName>();
		CollectImportables(prefabs);
		SortByDisplayName(prefabs);

		foreach(ResourceName prefab : prefabs)
		{
			if(wi == 0 && m_SelectedResource == -1){
				SelectItem(prefab);
			}

			Widget ww = workspace.CreateWidgets(m_ItemLayout, container);
			if(!ww) continue;

			OVT_PortItemComponent card = OVT_PortItemComponent.Cast(ww.FindHandler(OVT_PortItemComponent));
			if(!card) continue;

			int id = m_Economy.GetInventoryId(prefab);

			card.Init(prefab, m_Economy.GetPrice(id), this);

			wi++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Lists every prefab this player may import, de-duplicated. Same two branches and same membership
	//! rules as before Phase 7 - only the drawing moved out.
	//! \param[out] prefabs Receives the importable prefabs. Cleared first.
	protected void CollectImportables(out array<ResourceName> prefabs)
	{
		if(!prefabs) return;

		prefabs.Clear();

		if(m_PlayerData && m_PlayerData.HasPermission("IllegalImports"))
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

		foreach(OVT_ShopInventoryConfig shop : m_Economy.m_ShopConfig.m_aShopConfigs)
		{
			if(shop.type == OVT_ShopType.SHOP_VEHICLE) continue;

			foreach(OVT_ShopInventoryItem item : shop.m_aInventoryItems)
			{
				array<SCR_EntityCatalogEntry> entries();
				m_Economy.FindInventoryItems(item.m_eItemType, item.m_eItemMode, item.m_sFind, entries);

				foreach(SCR_EntityCatalogEntry entry : entries)
				{
					ResourceName prefab = entry.GetPrefab();
					if(prefabs.Contains(prefab)) continue;
					prefabs.Insert(prefab);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Sorts prefabs alphabetically by their localized display name, case-insensitively and stably.
	//!
	//! Insertion sort with a strictly-greater comparison, the same idiom (and stability guarantee) as
	//! OVT_ShopBrowserModel.SortByDisplayName. The model itself is not reused because these rows are
	//! bare prefabs, not browser items.
	//! \param[inout] prefabs The list to order in place.
	protected void SortByDisplayName(inout array<ResourceName> prefabs)
	{
		int count = prefabs.Count();

		for(int i = 1; i < count; i++)
		{
			ResourceName moving = prefabs[i];
			string movingName = ResolveDisplayName(moving);
			int j = i - 1;

			while(j >= 0 && ResolveDisplayName(prefabs[j]).Compare(movingName, false) > 0)
			{
				prefabs[j + 1] = prefabs[j];
				j--;
			}

			prefabs[j + 1] = moving;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Localized display name for a prefab, memoised for the life of this context.
	//!
	//! Translated rather than raw, because the sort is supposed to be alphabetical by what the player
	//! READS - sorting raw "#AR-Item_..." keys would produce an order with no relation to the list.
	//! \param[in] res The prefab to resolve.
	//! \return A non-empty, sortable name.
	protected string ResolveDisplayName(ResourceName res)
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
		int id = m_Economy.GetInventoryId(res);
		m_SelectedResource = id;
		m_SelectedResourceName = res;
		TextWidget typeName = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedTypeName"));
		ItemPreviewWidget img = ItemPreviewWidget.Cast(m_wRoot.FindAnyWidget("SelectedTypeImage"));
		TextWidget details = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDetails"));
		TextWidget desc = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDescription"));
		img.SetResolutionScale(1, 1);
		
		UIInfo info = OVT_Global.GetItemUIInfo(res);
		if(info)
		{
			typeName.SetText(info.GetName());
			details.SetText(OVT_MoneyFormat.FormatMoney(m_Economy.GetPrice(id)));
			desc.SetText(info.GetDescription());
		}	
		ChimeraWorld world = GetGame().GetWorld();
		ItemPreviewManagerEntity manager = world.GetItemPreviewManager();
		if (!manager)
			return;
		
		manager.SetPreviewItemFromPrefab(img, res);		
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
	
	void Buy(int qty)
	{		
		SCR_CompartmentAccessComponent compartment = SCR_CompartmentAccessComponent.Cast(m_Owner.FindComponent(SCR_CompartmentAccessComponent));
		if(!compartment) return;
			
		IEntity entity = compartment.GetVehicle();
		if(entity)
		{	
			OVT_Global.GetServer().ImportToVehicle(m_SelectedResource, qty, entity, m_iPlayerID);
		}		
	}	
	
	void BuyTen(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		Buy(10);
	}
	
	void BuyHundred(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		Buy(100);
	}
	
}