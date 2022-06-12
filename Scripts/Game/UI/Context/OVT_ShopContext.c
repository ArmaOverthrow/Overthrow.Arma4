class OVT_ShopContext : OVT_UIContext
{	
	protected OVT_ShopComponent m_Shop;
	protected int m_iPageNum = 0;
	protected ResourceName m_SelectedResource;
		
	override void PostInit()
	{
		if(SCR_Global.IsEditMode()) return;
		m_Economy.m_OnPlayerMoneyChanged.Insert(OnPlayerMoneyChanged);
	}
	
	protected void OnPlayerMoneyChanged(int playerId, int amount)
	{
		if(playerId == m_iPlayerID && m_bIsActive)
		{
			TextWidget money = TextWidget.Cast(m_wRoot.FindAnyWidget("PlayerMoney"));		
			money.SetText("$" + amount);
		}
	}
	
	override void OnShow()
	{	
		m_iPageNum = 0;	
		
		Widget buyButton = m_wRoot.FindAnyWidget("BuyButton");
		ButtonActionComponent action = ButtonActionComponent.Cast(buyButton.FindHandler(ButtonActionComponent));
		
		action.GetOnAction().Insert(Buy);
		
		Widget sellButton = m_wRoot.FindAnyWidget("SellButton");
		if(m_Shop.m_ShopType == OVT_ShopType.SHOP_GUNDEALER || m_Shop.m_ShopType == OVT_ShopType.SHOP_VEHICLE)
		{
			sellButton.SetVisible(false);
		}else{
			ButtonActionComponent sellAction = ButtonActionComponent.Cast(sellButton.FindHandler(ButtonActionComponent));
		
			sellAction.GetOnAction().Insert(Sell);
		}
		
		Refresh();		
	}
	
	void Refresh()
	{
		if(!m_Shop) return;
		
		TextWidget money = TextWidget.Cast(m_wRoot.FindAnyWidget("PlayerMoney"));
		
		money.SetText("$" + m_Economy.GetPlayerMoney(m_iPlayerID));
		
		TextWidget pages = TextWidget.Cast(m_wRoot.FindAnyWidget("Pages"));
		
		Widget grid = m_wRoot.FindAnyWidget("BrowserGrid");
		
		int numPages = Math.Ceil(m_Shop.m_aInventory.Count() / 15);
		if(m_iPageNum >= numPages) m_iPageNum = 0;
		string pageNumText = (m_iPageNum + 1).ToString();
		
		pages.SetText(pageNumText + "/" + numPages);
		
		int wi = 0;
		
		//We only read m_Shop.m_aInventory because m_Shop.m_aInventoryItems is not replicated
		for(int i = m_iPageNum * 15; i < (m_iPageNum + 1) * 15 && i < m_Shop.m_aInventory.Count(); i++)
		{			
			ResourceName prefab = m_Shop.m_aInventory.GetKey(i);
						
			if(wi == 0 && !m_SelectedResource){
				SelectItem(prefab);
			}
			
			Widget w = grid.FindWidget("ShopMenu_Card" + wi);
			w.SetOpacity(1);
			OVT_ShopMenuCardComponent card = OVT_ShopMenuCardComponent.Cast(w.FindHandler(OVT_ShopMenuCardComponent));
			
			int cost = m_Economy.GetPrice(prefab, m_Shop.GetOwner().GetOrigin());
			int qty = m_Shop.GetStock(prefab);
			
			card.Init(prefab, cost, qty, this);
			
			wi++;
		}
		
		for(; wi < 15; wi++)
		{
			Widget w = grid.FindWidget("ShopMenu_Card" + wi);			
			w.SetOpacity(0);
		}
		
	}
	
	void SelectItem(ResourceName res)
	{
		m_SelectedResource = res;
		TextWidget typeName = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedTypeName"));
		TextWidget details = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDetails"));
		TextWidget desc = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDescription"));
		
		int cost = m_Economy.GetPrice(res, m_Shop.GetOwner().GetOrigin());
		int qty = m_Shop.GetStock(res);
				
		IEntity spawnedItem = GetGame().SpawnEntityPrefabLocal(Resource.Load(res));
		
		SCR_EditableVehicleComponent veh = SCR_EditableVehicleComponent.Cast(spawnedItem.FindComponent(SCR_EditableVehicleComponent));
		if(veh){
			SCR_EditableEntityUIInfo info = SCR_EditableEntityUIInfo.Cast(veh.GetInfo());
			if(info)
			{
				typeName.SetText(info.GetName());
				details.SetText("$" + cost + "\n" + qty + " #OVT-Shop_InStock");
				desc.SetText(info.GetDescription());
			}
		}else{		
			InventoryItemComponent inv = InventoryItemComponent.Cast(spawnedItem.FindComponent(InventoryItemComponent));
			if(inv){
				SCR_ItemAttributeCollection attr = SCR_ItemAttributeCollection.Cast(inv.GetAttributes());
				if(attr)
				{
					UIInfo info = attr.GetUIInfo();
					typeName.SetText(info.GetName());
					details.SetText("$" + cost + "\n" + qty + " #OVT-Shop_InStock");
					desc.SetText(info.GetDescription());
				}
			}
		}
		
		SCR_Global.DeleteEntityAndChildren(spawnedItem);
	}
	
	void SetShop(OVT_ShopComponent shop)
	{
		m_Shop = shop;
	}
	
	override void RegisterInputs()
	{
		super.RegisterInputs();
		
		//m_InputManager.AddActionListener("OverthrowShopBuy", EActionTrigger.DOWN, Buy);
		m_InputManager.AddActionListener("MenuBack", EActionTrigger.DOWN, CloseLayout);
	}
	
	override void UnregisterInputs()
	{
		super.UnregisterInputs();
		
		//m_InputManager.RemoveActionListener("OverthrowShopBuy", EActionTrigger.DOWN, Buy);
		m_InputManager.RemoveActionListener("MenuBack", EActionTrigger.DOWN, CloseLayout);
	}		
	
	void Buy(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(m_Shop.GetStock(m_SelectedResource) < 1) return;
		
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(m_iPlayerID);
		if(!player) return;
		
		int cost = m_Economy.GetPrice(m_SelectedResource, m_Shop.GetOwner().GetOrigin());
		
		if(!m_Economy.PlayerHasMoney(m_iPlayerID, cost)) return;
				
		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(player.FindComponent( SCR_InventoryStorageManagerComponent ));
		if(!inventory) return;
		
		if(m_Shop.m_ShopType == OVT_ShopType.SHOP_VEHICLE)
		{
			if(OVT_VehicleManagerComponent.GetInstance().SpawnVehicleBehind(m_SelectedResource, player, m_iPlayerID))
			{
				m_Economy.TakePlayerMoney(m_iPlayerID, cost);
				m_Shop.TakeFromInventory(m_SelectedResource, 1);
				Refresh();
				SelectItem(m_SelectedResource);
			}
			return;
		}
		
		IEntity item = GetGame().SpawnEntityPrefabLocal(Resource.Load(m_SelectedResource));
		
		if(inventory.TryInsertItem(item))
		{
			m_Economy.TakePlayerMoney(m_iPlayerID, cost);
			m_Shop.TakeFromInventory(m_SelectedResource, 1);
		}
	}
	
	void Sell(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(m_iPlayerID);
		if(!player) return;
		
		int cost = m_Economy.GetPrice(m_SelectedResource, m_Shop.GetOwner().GetOrigin());
		
		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(player.FindComponent( SCR_InventoryStorageManagerComponent ));
		if(!inventory) return;
		
		array<IEntity> items = new array<IEntity>;
		inventory.GetItems(items);
		
		foreach(IEntity ent : items)
		{
			if(ent.GetPrefabData().GetPrefabName() == m_SelectedResource)
			{
				if(inventory.TryDeleteItem(ent))
				{
					m_Economy.AddPlayerMoney(m_iPlayerID, cost);
					m_Shop.AddToInventory(m_SelectedResource, 1);
					break;
				}
			}
		}
	}
	
	void ~OVT_ShopContext()
	{
		if(!m_Economy) return;
		m_Economy.m_OnPlayerMoneyChanged.Remove(OnPlayerMoneyChanged);
	}
}