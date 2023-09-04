class OVT_RealEstateContext : OVT_UIContext
{		
	protected OVT_TownManagerComponent m_Towns;
	protected OVT_RealEstateManagerComponent m_RealEstate;
	
	override void PostInit()
	{
		if(SCR_Global.IsEditMode()) return;
		m_Towns = OVT_Global.GetTowns();
		m_RealEstate = OVT_Global.GetRealEstate();
	}
	
	override void OnShow()
	{		
		ButtonWidget btn = ButtonWidget.Cast(m_wRoot.FindAnyWidget("Rent"));
		SCR_ButtonTextComponent b = SCR_ButtonTextComponent.Cast(btn.FindHandler(SCR_ButtonTextComponent));
		b.m_OnClicked.Insert(Rent);
		
		btn = ButtonWidget.Cast(m_wRoot.FindAnyWidget("StopRenting"));
		b = SCR_ButtonTextComponent.Cast(btn.FindHandler(SCR_ButtonTextComponent));
		b.m_OnClicked.Insert(StopRenting);
		
		btn = ButtonWidget.Cast(m_wRoot.FindAnyWidget("Buy"));
		b = SCR_ButtonTextComponent.Cast(btn.FindHandler(SCR_ButtonTextComponent));
		b.m_OnClicked.Insert(Buy);
		
		btn = ButtonWidget.Cast(m_wRoot.FindAnyWidget("Sell"));
		b = SCR_ButtonTextComponent.Cast(btn.FindHandler(SCR_ButtonTextComponent));
		b.m_OnClicked.Insert(Sell);
		
		btn = ButtonWidget.Cast(m_wRoot.FindAnyWidget("SetAsHome"));
		b = SCR_ButtonTextComponent.Cast(btn.FindHandler(SCR_ButtonTextComponent));
		b.m_OnClicked.Insert(SetAsHome);
		
		Widget closeButton = m_wRoot.FindAnyWidget("CloseButton");
		SCR_NavigationButtonComponent nb = SCR_NavigationButtonComponent.Cast(closeButton.FindHandler(SCR_NavigationButtonComponent));		
		nb.m_OnClicked.Insert(CloseLayout);
		
		Widget spinner = m_wRoot.FindAnyWidget("AccountSpinner");
		SCR_SpinBoxComponent spin = SCR_SpinBoxComponent.Cast(spinner.FindHandler(SCR_SpinBoxComponent));
		spin.AddItem("#OVT-MyAccount");
		if(OVT_Global.GetPlayers().LocalPlayerIsOfficer())
		{
			spin.AddItem("#OVT-ResistanceFunds");
			spinner.SetEnabled(true);
		}else{
			spinner.SetEnabled(false);
		}
		spin.GetOnLeftArrowClick().Insert(Refresh);
		spin.GetOnRightArrowClick().Insert(Refresh);
		
		Refresh();		
	}
	
	override void OnClose()
	{
		
	}
	
	protected int GetCurrentAccount()
	{
		Widget spinner = m_wRoot.FindAnyWidget("AccountSpinner");
		SCR_SpinBoxComponent spin = SCR_SpinBoxComponent.Cast(spinner.FindHandler(SCR_SpinBoxComponent));
		
		return spin.GetCurrentIndex();
	}
	
	protected void Refresh()
	{			
		int account = GetCurrentAccount();
		IEntity building = m_RealEstate.GetNearestBuilding(m_Owner.GetOrigin());
				
		int buy = m_RealEstate.GetBuyPrice(building);
		int rent = m_RealEstate.GetRentPrice(building);
		
		TextWidget w = TextWidget.Cast(m_wRoot.FindAnyWidget("BuyPrice"));
		w.SetText("$" + buy.ToString());
		
		w = TextWidget.Cast(m_wRoot.FindAnyWidget("RentPrice"));
		w.SetText("$" + rent.ToString());
		
		EntityID id = building.GetID();
		
		bool isRented = m_RealEstate.IsRented(id);
		bool isOwned = m_RealEstate.IsOwned(id);
		bool isRenter = m_RealEstate.IsRenter(m_sPlayerID, id);
		bool isOwner = m_RealEstate.IsOwner(m_sPlayerID, id);
		bool isHome = m_RealEstate.IsHome(m_sPlayerID, id);
		bool isOnlyHouse = m_RealEstate.m_mOwned[m_sPlayerID].Count() == 1;
		bool isOfficer = OVT_Global.GetPlayers().LocalPlayerIsOfficer();
		bool isResistanceOwned = false;
		if(isOwned)
		{
			isResistanceOwned = m_RealEstate.GetOwnerID(building) == "resistance";
			if(isResistanceOwned && account == 1)
			{
				isOwner = true;
			}
		}
		bool isResistanceRented = false;
		if(isRented)
		{
			isResistanceRented = m_RealEstate.GetRenterID(building) == "resistance";
			if(isResistanceRented && account == 1)
			{
				isRenter = true;
			}
		}
		
		OverlayWidget o = OverlayWidget.Cast(m_wRoot.FindAnyWidget("Renting"));
		if(!isRenter)
		{
			o.SetVisible(false);
		}else{
			o.SetVisible(true);
			if(isOwner)
			{
				w = TextWidget.Cast(m_wRoot.FindAnyWidget("RentLabel"));
				w.SetText("#OVT-RentIncome");
			}
		}
		o = OverlayWidget.Cast(m_wRoot.FindAnyWidget("Owned"));
		if(!isOwner)
		{
			o.SetVisible(false);
		}else{
			o.SetVisible(true);
			w = TextWidget.Cast(m_wRoot.FindAnyWidget("BuyLabel"));
			w.SetText("#OVT-Shop_Selling");
		}
		
		o = OverlayWidget.Cast(m_wRoot.FindAnyWidget("Rent Price"));
		if(isHome || (isRented && !isOwner))
		{
			o.SetVisible(false);
		}else{
			o.SetVisible(true);
		}
		
		o = OverlayWidget.Cast(m_wRoot.FindAnyWidget("IsHome"));
		if(isHome)
		{
			o.SetVisible(true);
		}else{
			o.SetVisible(false);
		}
		
		//Buttons
		ButtonWidget btn = ButtonWidget.Cast(m_wRoot.FindAnyWidget("Rent"));
		if(isHome || isRented || (isOwned && !isOwner))
		{
			btn.SetEnabled(false);
		}else{
			btn.SetEnabled(true);
		}
		btn = ButtonWidget.Cast(m_wRoot.FindAnyWidget("StopRenting"));
		if(isRenter)
		{
			btn.SetEnabled(true);
		}else{
			btn.SetEnabled(false);
		}
		btn = ButtonWidget.Cast(m_wRoot.FindAnyWidget("Buy"));
		if(isOwned)
		{
			btn.SetEnabled(false);
		}else{
			btn.SetEnabled(true);
		}
		btn = ButtonWidget.Cast(m_wRoot.FindAnyWidget("Sell"));
		if(isOwner && !isHome && !isRenter && !isOnlyHouse)
		{
			btn.SetEnabled(true);
		}else{
			btn.SetEnabled(false);
		}
		
		btn = ButtonWidget.Cast(m_wRoot.FindAnyWidget("SetAsHome"));
		if(!isHome && isOwner && !isRenter)
		{
			btn.SetEnabled(true);
		}else{
			btn.SetEnabled(false);
		}
		
		ItemPreviewWidget img = ItemPreviewWidget.Cast(m_wRoot.FindAnyWidget("Image"));
		ItemPreviewManagerEntity manager = GetGame().GetItemPreviewManager();
		if (!manager)
			return;
						
		img.SetResolutionScale(1, 1);		
		manager.SetPreviewItemFromPrefab(img, building.GetPrefabData().GetPrefab().GetResourceName());
	}
	
	protected void Buy(SCR_ButtonTextComponent btn)
	{
		int account = GetCurrentAccount();
		
		IEntity building = m_RealEstate.GetNearestBuilding(m_Owner.GetOrigin());
		EntityID id = building.GetID();
		
		bool isRented = m_RealEstate.IsRented(id);
		bool isOwned = m_RealEstate.IsOwned(id);
		
		if(isRented || isOwned) return;
				
		int cost = m_RealEstate.GetBuyPrice(building);
		
		if(account == 0)
		{				
			if(!m_Economy.PlayerHasMoney(m_sPlayerID, cost)) return;
			
			m_Economy.TakePlayerMoney(m_iPlayerID, cost);
			OVT_Global.GetServer().SetBuildingOwner(m_iPlayerID, building);
		}else if(account == 1)
		{
			if(!m_Economy.ResistanceHasMoney(cost)) return;
			
			m_Economy.TakeResistanceMoney(cost);
			OVT_Global.GetServer().SetBuildingOwner("resistance", building);
		}
		
		Refresh();
	}
	
	protected void Sell(SCR_ButtonTextComponent btn)
	{
		int account = GetCurrentAccount();
		
		IEntity building = m_RealEstate.GetNearestBuilding(m_Owner.GetOrigin());
		EntityID id = building.GetID();
		
		bool isOwner = m_RealEstate.IsOwner(m_sPlayerID, id);	
		bool isOnlyHouse = m_RealEstate.m_mOwned[m_sPlayerID].Count() == 1;
		
		bool isResistanceOwned = false;		
		isResistanceOwned = m_RealEstate.GetOwnerID(building) == "resistance";
		if(isResistanceOwned && account == 1)
		{
			isOwner = true;
		}
	
		
		if(!isOwner) return;
		if(isOnlyHouse) return;
		
		if(m_RealEstate.IsHome(m_sPlayerID, id)) return;
				
		int cost = m_RealEstate.GetBuyPrice(building);
		
		if(account == 0)
		{
			m_Economy.AddPlayerMoney(m_iPlayerID, cost);
		}else if(account == 1)
		{
			m_Economy.AddResistanceMoney(cost);
		}
		
		OVT_Global.GetServer().SetBuildingOwner(-1, building);
		
		Refresh();
	}
	
	protected void Rent(SCR_ButtonTextComponent btn)
	{
		int account = GetCurrentAccount();
		
		IEntity building = m_RealEstate.GetNearestBuilding(m_Owner.GetOrigin());
		EntityID id = building.GetID();
		
		bool isRented = m_RealEstate.IsRented(id);
		bool isOwned = m_RealEstate.IsOwned(id);
		bool isOwner = m_RealEstate.IsOwner(m_sPlayerID, id);
		bool isHome = m_RealEstate.IsHome(m_sPlayerID, id);
		bool isResistanceOwned = false;
		if(isOwned)
		{
			isResistanceOwned = m_RealEstate.GetOwnerID(building) == "resistance";
			if(isResistanceOwned && account == 1)
			{
				isOwner = true;
			}
		}
		
		if(isHome || isRented || (isOwned && !isOwner)) return;
							
		if(!isOwner)
		{
			int cost = m_RealEstate.GetRentPrice(building);	
			
			if(account == 0)			
			{
				if(!m_Economy.PlayerHasMoney(m_sPlayerID, cost)) return;		
				m_Economy.TakePlayerMoney(m_iPlayerID, cost);			
			}else if(account == 1)
			{
				if(!m_Economy.ResistanceHasMoney(cost)) return;		
				m_Economy.TakeResistanceMoney(cost);	
			}
		}
		
		
		if(account == 0)			
		{
			OVT_Global.GetServer().SetBuildingRenter(m_iPlayerID, building.GetOrigin());
		}else if(account == 1)
		{
			OVT_Global.GetServer().SetBuildingRenter("resistance", building.GetOrigin());
		}
		
		Refresh();
	}
	
	protected void StopRenting(SCR_ButtonTextComponent btn)
	{
		int account = GetCurrentAccount();
		
		IEntity building = m_RealEstate.GetNearestBuilding(m_Owner.GetOrigin());
		EntityID id = building.GetID();
		
		bool isRenter = m_RealEstate.IsRenter(m_sPlayerID, id);
		bool isResistanceRented = false;
		
		isResistanceRented = m_RealEstate.GetRenterID(building) == "resistance";
		if(isResistanceRented && account == 1)
		{
			isRenter = true;
		}
		
		
		if(!isRenter) return;
				
		OVT_Global.GetServer().SetBuildingRenter(-1, building.GetOrigin());
		
		Refresh();
	}
	
	protected void SetAsHome(SCR_ButtonTextComponent btn)
	{
		IEntity building = m_RealEstate.GetNearestBuilding(m_Owner.GetOrigin());
		EntityID id = building.GetID();
		
		bool isOwner = m_RealEstate.IsOwner(m_sPlayerID, id);
		bool isHome = m_RealEstate.IsHome(m_sPlayerID, id);
		
		if(!isOwner || isHome) return;
		
		OVT_Global.GetServer().SetHome(m_iPlayerID);
		
		Refresh();
	}
}