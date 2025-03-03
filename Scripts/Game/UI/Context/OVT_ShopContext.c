class OVT_ShopContext : OVT_UIContext
{	
	protected OVT_ShopComponent m_Shop;
	protected int m_iPageNum = 0;
	protected int m_SelectedResource = -1;
	protected ResourceName m_SelectedResourceName;
	protected int m_iNumPages = 0;
		
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
			money.SetText("$" + amount);
		}
	}
	
	//SPARKNUTZ CHANGED CODE
	
	override void OnShow()
	{	
		m_iPageNum = 0;	
		
		// Set up the buy button
		Widget buyButton = m_wRoot.FindAnyWidget("BuyButton");
		SCR_InputButtonComponent action = SCR_InputButtonComponent.Cast(buyButton.FindHandler(SCR_InputButtonComponent));
		action.m_OnActivated.Insert(Buy);
		
		// Set up the sell button with updated visibility logic
		Widget sellButton = m_wRoot.FindAnyWidget("SellButton");
		bool showSellButton = ShouldShowSellButton(m_Shop.m_ShopType);
		sellButton.SetVisible(showSellButton);
		if (showSellButton)
		{
			SCR_InputButtonComponent sellAction = SCR_InputButtonComponent.Cast(sellButton.FindHandler(SCR_InputButtonComponent));
			sellAction.m_OnActivated.Insert(Sell);
		}
		
		// Set up the previous button
		Widget prevButton = m_wRoot.FindAnyWidget("PrevButton");
		SCR_InputButtonComponent btn = SCR_InputButtonComponent.Cast(prevButton.FindHandler(SCR_InputButtonComponent));
		btn.m_OnActivated.Insert(PreviousPage);
		
		// Set up the next button
		Widget nextButton = m_wRoot.FindAnyWidget("NextButton");
		btn = SCR_InputButtonComponent.Cast(nextButton.FindHandler(SCR_InputButtonComponent));
		btn.m_OnActivated.Insert(NextPage);
		
		// Set up the close button
		Widget closeButton = m_wRoot.FindAnyWidget("CloseButton");
		btn = SCR_InputButtonComponent.Cast(closeButton.FindHandler(SCR_InputButtonComponent));		
		btn.m_OnActivated.Insert(CloseLayout);
		
		Refresh();		
	}
	
	// Helper function to determine sell button visibility
	private bool ShouldShowSellButton(OVT_ShopType shopType)
	{
		if (shopType == OVT_ShopType.SHOP_VEHICLE)
		{
			return false; // Hide for vehicle shops
		}
		if (shopType == OVT_ShopType.SHOP_GUNDEALER)
		{
			float multiplier = OVT_Global.GetConfig().m_Difficulty.gunDealerSellPriceMultiplier;
			Print("gunDealerSellPriceMultiplier: " + multiplier); // Retain debug print
			return multiplier != 0; // Show only if multiplier is not zero
		}
		return true; // Show for all other shop types
	}
	
		//SPARKNUTZ CHANGED CODE
	
//	override void OnShow()
//	{	
//		m_iPageNum = 0;	
//		
//		
//		Widget buyButton = m_wRoot.FindAnyWidget("BuyButton");
//		SCR_InputButtonComponent action = SCR_InputButtonComponent.Cast(buyButton.FindHandler(SCR_InputButtonComponent));
//				
//		action.m_OnActivated.Insert(Buy);
//		
//		Widget sellButton = m_wRoot.FindAnyWidget("SellButton");
//		//if(m_Shop.m_ShopType == OVT_ShopType.SHOP_GUNDEALER || m_Shop.m_ShopType == OVT_ShopType.SHOP_VEHICLE) - Original Changes by Chris
//		if(m_Shop.m_ShopType == OVT_ShopType.SHOP_VEHICLE)
//		{
//			sellButton.SetVisible(false);
//		}else{
//			//Sparknutz - adding a print for debugging//
//			Print("gunDealerSellPriceMultiplier: " + OVT_Global.GetConfig().m_Difficulty.gunDealerSellPriceMultiplier);
//			if(m_Shop.m_ShopType == OVT_ShopType.SHOP_GUNDEALER && OVT_Global.GetConfig().m_Difficulty.gunDealerSellPriceMultiplier == 0)
//			{
//				sellButton.SetVisible(false);
//			}else{
//				SCR_InputButtonComponent sellAction = SCR_InputButtonComponent.Cast(sellButton.FindHandler(SCR_InputButtonComponent));
//			
//				sellAction.m_OnActivated.Insert(Sell);
//			}
//		}
//		
//		
//		
//		Widget prevButton = m_wRoot.FindAnyWidget("PrevButton");
//		SCR_InputButtonComponent btn = SCR_InputButtonComponent.Cast(prevButton.FindHandler(SCR_InputButtonComponent));
//		
//		btn.m_OnActivated.Insert(PreviousPage);
//		
//		Widget nextButton = m_wRoot.FindAnyWidget("NextButton");
//		btn = SCR_InputButtonComponent.Cast(nextButton.FindHandler(SCR_InputButtonComponent));
//		
//		btn.m_OnActivated.Insert(NextPage);
//		
//		Widget closeButton = m_wRoot.FindAnyWidget("CloseButton");
//		btn = SCR_InputButtonComponent.Cast(closeButton.FindHandler(SCR_InputButtonComponent));		
//		btn.m_OnActivated.Insert(CloseLayout);
//		
//		Refresh();		
//	}
	
	void PreviousPage()
	{
		if(!m_wRoot) return;
		m_iPageNum--;
		if(m_iPageNum < 0) m_iPageNum = 0;
		
		Refresh();
	}
	
	void NextPage()
	{
		if(!m_wRoot) return;
		m_iPageNum++;
		if(m_iPageNum > m_iNumPages-1) m_iPageNum = m_iNumPages-1;
		
		Refresh();
	}
	
	void Refresh()
	{
		if(!m_Shop) return;
		if(!m_wRoot) return;
		
		TextWidget money = TextWidget.Cast(m_wRoot.FindAnyWidget("PlayerMoney"));
		
		money.SetText("$" + m_Economy.GetPlayerMoney(m_sPlayerID));
		
		TextWidget pages = TextWidget.Cast(m_wRoot.FindAnyWidget("Pages"));
		
		Widget grid = m_wRoot.FindAnyWidget("BrowserGrid");
				
		int wi = 0;
		
		if(m_Shop.m_bProcurement)
		{
			OVT_ParkingComponent parking = EPF_Component<OVT_ParkingComponent>.Find(m_Shop.GetOwner());
			array<OVT_ParkingType> parkingTypes();
			parking.GetParkingTypes(parkingTypes);
			
			array<ResourceName> vehicles();
			m_Economy.GetAllNonOccupyingFactionVehiclesByParking(vehicles, parkingTypes, true);
			
			m_iNumPages = Math.Ceil(vehicles.Count() / 15);
			if(m_iPageNum >= m_iNumPages) m_iPageNum = 0;
			string pageNumText = (m_iPageNum + 1).ToString();
			
			pages.SetText(pageNumText + "/" + m_iNumPages);
						
			for(int i = m_iPageNum * 15; i < (m_iPageNum + 1) * 15 && i < vehicles.Count(); i++)
			{	
				ResourceName res = vehicles[i];
				int id = m_Economy.GetInventoryId(res);
				if(wi == 0 && m_SelectedResource == -1){
					SelectItem(res);
				}
				Widget w = grid.FindWidget("ShopMenu_Card" + wi);
				w.SetOpacity(1);
				OVT_ShopMenuCardComponent card = OVT_ShopMenuCardComponent.Cast(w.FindHandler(OVT_ShopMenuCardComponent));
				
				int buy = m_Economy.GetPrice(id);
				buy = buy * OVT_Global.GetConfig().m_Difficulty.procurementMultiplier;
								
				card.Init(res, buy, 100, this);
				
				wi++;
			}
		}else{
			m_iNumPages = Math.Ceil(m_Shop.m_aInventory.Count() / 15);
			if(m_iPageNum >= m_iNumPages) m_iPageNum = 0;
			string pageNumText = (m_iPageNum + 1).ToString();
			
			pages.SetText(pageNumText + "/" + m_iNumPages);
			
			//We only read m_Shop.m_aInventory because m_Shop.m_aInventoryItems is not replicated
			for(int i = m_iPageNum * 15; i < (m_iPageNum + 1) * 15 && i < m_Shop.m_aInventory.Count(); i++)
			{			
				int id = m_Shop.m_aInventory.GetKey(i);
				ResourceName res = m_Economy.GetResource(id);
							
				if(wi == 0 && m_SelectedResource == -1){
					SelectItem(res);
				}
				
				Widget w = grid.FindWidget("ShopMenu_Card" + wi);
				w.SetOpacity(1);
				OVT_ShopMenuCardComponent card = OVT_ShopMenuCardComponent.Cast(w.FindHandler(OVT_ShopMenuCardComponent));
				
				int buy = m_Economy.GetBuyPrice(id, m_Shop.GetOwner().GetOrigin(),m_iPlayerID);
				int qty = m_Shop.GetStock(id);
				
				card.Init(res, buy, qty, this);
				
				wi++;
			}
		}
		
		for(; wi < 15; wi++)
		{
			Widget w = grid.FindWidget("ShopMenu_Card" + wi);			
			w.SetOpacity(0);
		}
		
	}
	
	override void SelectItem(ResourceName res)
	{
		int id = m_Economy.GetInventoryId(res);
		m_SelectedResource = id;
		m_SelectedResourceName = res;
		TextWidget typeName = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedTypeName"));
		TextWidget details = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDetails"));
		TextWidget desc = TextWidget.Cast(m_wRoot.FindAnyWidget("SelectedDescription"));
		
		int buy, sell, qty, max;
		
		if(m_Shop.m_bProcurement)
		{
			buy = m_Economy.GetPrice(id);
			buy = buy * OVT_Global.GetConfig().m_Difficulty.procurementMultiplier;
			sell = buy;
			qty = 100;
			max = 100;
		}else{
			buy = m_Economy.GetBuyPrice(id, m_Shop.GetOwner().GetOrigin(),m_iPlayerID);
			sell = m_Economy.GetSellPrice(id, m_Shop.GetOwner().GetOrigin());
			if(m_Shop.m_ShopType == OVT_ShopType.SHOP_GUNDEALER)
			{
				sell = sell * OVT_Global.GetConfig().m_Difficulty.gunDealerSellPriceMultiplier;
			}
			qty = m_Shop.GetStock(id);
			OVT_TownData town = m_Shop.GetTown();
			int townID = OVT_Global.GetTowns().GetTownID(town);
			max = m_Economy.GetTownMaxStock(townID, id);
		}	
		
		if(m_Economy.IsVehicle(res))
		{
			SCR_EditableVehicleUIInfo info = OVT_Global.GetVehicleUIInfo(res);
			if(info)
			{
				typeName.SetText(info.GetName());				
				desc.SetText(info.GetDescription());
			}else{
				SCR_EditableEntityUIInfo uiinfo = OVT_Global.GetEditableUIInfo(res);
				typeName.SetText(uiinfo.GetName());				
				desc.SetText(uiinfo.GetDescription());
			}
			if(m_Shop.m_bProcurement)
			{
				details.SetText("$" + buy);				
			}else{
				details.SetText("$" + buy + "\n" + qty + " #OVT-Shop_InStock");				
			}
		}else{
			UIInfo info = OVT_Global.GetItemUIInfo(res);
			if(info)
			{
				typeName.SetText(info.GetName());
				details.SetText("#OVT-Shop_Buying: $" + buy + "\n#OVT-Shop_Selling: $" + sell + "\n" + qty + "/" + max + " #OVT-Shop_InStock");
				desc.SetText(info.GetDescription());
			}
		}
	}
	
	void SetShop(OVT_ShopComponent shop)
	{
		m_Shop = shop;
	}		
	
	void Buy(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(!m_Shop.m_bProcurement && m_Shop.GetStock(m_SelectedResource) < 1) return;
		
		int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(m_sPlayerID);
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;
		
		int cost = m_Economy.GetBuyPrice(m_SelectedResource, m_Shop.GetOwner().GetOrigin(),m_iPlayerID);
		
		if(m_Shop.m_bProcurement)
		{
			cost = m_Economy.GetPrice(m_SelectedResource);
		}
		
		if(!m_Economy.PlayerHasMoney(m_sPlayerID, cost)) return;
				
		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(player.FindComponent( SCR_InventoryStorageManagerComponent ));
		if(!inventory) return;
		
		if(m_Shop.m_ShopType == OVT_ShopType.SHOP_VEHICLE)
		{
			OVT_Global.GetServer().BuyVehicle(m_Shop, m_SelectedResource, m_iPlayerID);	
			CloseLayout();
			return;
		}	
		
		OVT_Global.GetServer().Buy(m_Shop, m_SelectedResource, 1, m_iPlayerID);	
		SelectItem(m_SelectedResourceName);
	}
	
	void Sell(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(m_sPlayerID);
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(!player) return;
		
		int cost = m_Economy.GetSellPrice(m_SelectedResource, m_Shop.GetOwner().GetOrigin());
		if(m_Shop.m_ShopType == OVT_ShopType.SHOP_GUNDEALER)
		{
			cost = cost * OVT_Global.GetConfig().m_Difficulty.gunDealerSellPriceMultiplier;
		}
		
		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(player.FindComponent( SCR_InventoryStorageManagerComponent ));
		if(!inventory) return;
		
		autoptr array<IEntity> items = new array<IEntity>;
		inventory.GetItems(items);
		
		ResourceName res = m_Economy.GetResource(m_SelectedResource);
		
		foreach(IEntity ent : items)
		//Chris - Make this work better for variants
		{
			string prefab = ent.GetPrefabData().GetPrefabName();
			if (prefab == "{63E8322E2ADD4AA7}Prefabs/Weapons/Rifles/AK74/Rifle_AK74_GP25.et")
			{
			prefab = "{FA5C25BF66A53DCF}Prefabs/Weapons/Rifles/AK74/Rifle_AK74.et";
			}
			if (prefab == "{EB404DC9E1BCB750}Prefabs/Weapons/Rifles/AK74/Rifle_AK74N_1P29.et" || prefab == "{BC6C9476FB3219A7}Prefabs/Weapons/Rifles/AK74/Rifle_AK74N_GP25.et")
			{
			prefab = "{96DFD2E7E63B3386}Prefabs/Weapons/Rifles/AK74/Rifle_AK74N.et";
			}
			if (res == "{7A82FE978603F137}Prefabs/Weapons/Launchers/RPG7/Launcher_RPG7.et" && prefab == "{E8A55396050E1762}Prefabs/Weapons/Launchers/RPG7/Launcher_RPG7_PGO7.et")
			{
			prefab = res;
			}			
			if (res == "{E8A55396050E1762}Prefabs/Weapons/Launchers/RPG7/Launcher_RPG7_PGO7.et" && prefab == "{7A82FE978603F137}Prefabs/Weapons/Launchers/RPG7/Launcher_RPG7.et")
			{
			prefab = res;
			}			
			if(prefab == res)
			{
				if(inventory.TryDeleteItem(ent))
				{
					m_Economy.AddPlayerMoney(m_iPlayerID, cost, true);
					m_Shop.AddToInventory(m_SelectedResource, 1);
					SelectItem(m_SelectedResourceName);
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
