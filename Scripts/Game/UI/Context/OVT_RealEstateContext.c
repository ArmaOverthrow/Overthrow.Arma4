//------------------------------------------------------------------------------------------------
//! The real-estate menu for the building the player is standing in: buy, sell, rent, stop renting,
//! set as home - for their own wallet or (as an officer) for the resistance's.
//!
//! Every action here ASKS the server and then redraws optimistically: the asks are asynchronous, so
//! the Refresh() that follows one still shows the pre-ask ownership state, and the real state arrives
//! a moment later through the owner-manager's own replication. That is deliberate and unchanged -
//! what Phase 7 added is that a refused action now SAYS why instead of returning in silence.
//------------------------------------------------------------------------------------------------
class OVT_RealEstateContext : OVT_UIContext
{
	//! How long an in-menu message stays up.
	protected const int MESSAGE_TIMEOUT_MS = 4000;

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
		if(!m_wRoot) return;

		HideMessage();

		SCR_ButtonTextComponent b = FindTextButton("Rent");
		if(b) b.m_OnClicked.Insert(Rent);

		b = FindTextButton("StopRenting");
		if(b) b.m_OnClicked.Insert(StopRenting);

		b = FindTextButton("Buy");
		if(b) b.m_OnClicked.Insert(Buy);

		b = FindTextButton("Sell");
		if(b) b.m_OnClicked.Insert(Sell);

		b = FindTextButton("SetAsHome");
		if(b) b.m_OnClicked.Insert(SetAsHome);

		Widget closeButton = m_wRoot.FindAnyWidget("CloseButton");
		if(closeButton)
		{
			SCR_InputButtonComponent nb = SCR_InputButtonComponent.Cast(closeButton.FindHandler(SCR_InputButtonComponent));
			if(nb) nb.m_OnClicked.Insert(CloseLayout);
		}

		Widget spinner = m_wRoot.FindAnyWidget("AccountSpinner");
		if(spinner)
		{
			SCR_SpinBoxComponent spin = SCR_SpinBoxComponent.Cast(spinner.FindHandler(SCR_SpinBoxComponent));
			if(spin)
			{
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
			}
		}

		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Finds one text button's component. A stale layout degrades to a missing button, not an error.
	//! \param[in] name Widget name in RealEstate.layout.
	//! \return The button's component, or null when the layout has no such button.
	protected SCR_ButtonTextComponent FindTextButton(string name)
	{
		if(!m_wRoot) return null;

		ButtonWidget btn = ButtonWidget.Cast(m_wRoot.FindAnyWidget(name));
		if(!btn) return null;

		return SCR_ButtonTextComponent.Cast(btn.FindHandler(SCR_ButtonTextComponent));
	}

	override void OnClose()
	{
		// The layout is about to be destroyed; a pending auto-hide would poke a dead widget.
		GetGame().GetCallqueue().Remove(HideMessage);
	}

	//------------------------------------------------------------------------------------------------
	//! Shows a transient localized message. This menu's refusals used to be silent returns, which read
	//! as a dead button (Phase 7.4).
	//!
	//! Deliberately NOT routed through OVT_NotificationManagerComponent: notifications only render in
	//! the HUD strip, and showing a menu hides the HUD, so the player would never see them.
	//! \param[in] key #OVT- localization key.
	protected void ShowMessage(string key)
	{
		if(!m_wRoot) return;

		TextWidget message = TextWidget.Cast(m_wRoot.FindAnyWidget("Message"));
		if(!message) return;

		message.SetText(key);
		message.SetVisible(true);

		GetGame().GetCallqueue().Remove(HideMessage);
		GetGame().GetCallqueue().CallLater(HideMessage, MESSAGE_TIMEOUT_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Clears the message line.
	protected void HideMessage()
	{
		if(!m_wRoot) return;

		TextWidget message = TextWidget.Cast(m_wRoot.FindAnyWidget("Message"));
		if(!message) return;

		message.SetText("");
		message.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	//! How many buildings this player owns.
	//!
	//! m_mOwned has NO entry at all for a player who owns nothing, so the raw m_mOwned[m_sPlayerID]
	//! lookups this replaced dereferenced null the moment a fresh player opened this menu (Q7).
	//! \return The owned-building count, 0 when the player owns none.
	protected int GetOwnedCount()
	{
		if(!m_RealEstate || !m_RealEstate.m_mOwned) return 0;
		if(!m_RealEstate.m_mOwned.Contains(m_sPlayerID)) return 0;

		array<string> ownedBuildings = m_RealEstate.m_mOwned[m_sPlayerID];
		if(!ownedBuildings) return 0;

		return ownedBuildings.Count();
	}
	
	protected int GetCurrentAccount()
	{
		Widget spinner = m_wRoot.FindAnyWidget("AccountSpinner");
		SCR_SpinBoxComponent spin = SCR_SpinBoxComponent.Cast(spinner.FindHandler(SCR_SpinBoxComponent));
		
		return spin.GetCurrentIndex();
	}
	
	protected void Refresh()
	{			
		if(!m_wRoot || !m_RealEstate) return;

		int account = GetCurrentAccount();
		IEntity building = m_RealEstate.GetNearestBuilding(m_Owner.GetOrigin());
		if(!building) return;

		int buy = m_RealEstate.GetBuyPrice(building);
		int rent = m_RealEstate.GetRentPrice(building);
		
		TextWidget w = TextWidget.Cast(m_wRoot.FindAnyWidget("BuyPrice"));
		if(w) w.SetText(OVT_MoneyFormat.FormatMoney(buy));

		w = TextWidget.Cast(m_wRoot.FindAnyWidget("RentPrice"));
		if(w) w.SetText(OVT_MoneyFormat.FormatMoney(rent));
		
		EntityID id = building.GetID();
		
		bool isRented = m_RealEstate.IsRented(id);
		bool isOwned = m_RealEstate.IsOwned(id);
		bool isRenter = m_RealEstate.IsRenter(m_sPlayerID, id);
		bool isOwner = m_RealEstate.IsOwner(m_sPlayerID, id);
		bool isHome = m_RealEstate.IsHome(m_sPlayerID, id);
		bool isOnlyHouse = GetOwnedCount() == 1;
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
		ChimeraWorld world = GetGame().GetWorld();
		ItemPreviewManagerEntity manager = world.GetItemPreviewManager();
		if (!manager)
			return;
						
		img.SetResolutionScale(1, 1);		
		manager.SetPreviewItemFromPrefab(img, building.GetPrefabData().GetPrefab().GetResourceName());
	}
	
	protected void Buy(SCR_ButtonTextComponent btn)
	{
		int account = GetCurrentAccount();
		
		IEntity building = m_RealEstate.GetNearestBuilding(m_Owner.GetOrigin());
		if(!building) return;

		EntityID id = building.GetID();

		bool isRented = m_RealEstate.IsRented(id);
		bool isOwned = m_RealEstate.IsOwned(id);

		if(isRented || isOwned)
		{
			ShowMessage("#OVT-RealEstate_NotForSale");
			return;
		}

		int cost = m_RealEstate.GetBuyPrice(building);

		if(account == 0)
		{
			if(!m_Economy.PlayerHasMoney(m_sPlayerID, cost))
			{
				ShowMessage("#OVT-RealEstate_CantAfford");
				return;
			}

			OVT_Global.GetServer().BuyBuilding(m_iPlayerID, false);
		}else if(account == 1)
		{
			if(!m_Economy.ResistanceHasMoney(cost))
			{
				ShowMessage("#OVT-RealEstate_CantAfford");
				return;
			}

			OVT_Global.GetServer().BuyBuilding(m_iPlayerID, true);
		}

		// Optimistic: BuyBuilding is an asynchronous ask, so this redraw still shows the pre-ask
		// ownership state. The authoritative state lands moments later through the owner manager's own
		// replication, which redraws again when the menu is reopened.
		Refresh();
	}
	
	protected void Sell(SCR_ButtonTextComponent btn)
	{
		int account = GetCurrentAccount();
		
		IEntity building = m_RealEstate.GetNearestBuilding(m_Owner.GetOrigin());
		if(!building) return;

		EntityID id = building.GetID();

		bool isOwner = m_RealEstate.IsOwner(m_sPlayerID, id);
		bool isOnlyHouse = GetOwnedCount() == 1;

		bool isResistanceOwned = false;
		isResistanceOwned = m_RealEstate.GetOwnerID(building) == "resistance";
		if(isResistanceOwned && account == 1)
		{
			isOwner = true;
		}

		if(!isOwner)
		{
			ShowMessage("#OVT-RealEstate_NotOwner");
			return;
		}
		if(isOnlyHouse)
		{
			ShowMessage("#OVT-RealEstate_OnlyHouse");
			return;
		}

		if(m_RealEstate.IsHome(m_sPlayerID, id))
		{
			ShowMessage("#OVT-RealEstate_IsHome");
			return;
		}

		OVT_Global.GetServer().SellBuilding(m_iPlayerID, account == 1);

		// Optimistic - see Buy().
		Refresh();
	}
	
	protected void Rent(SCR_ButtonTextComponent btn)
	{
		int account = GetCurrentAccount();
		
		IEntity building = m_RealEstate.GetNearestBuilding(m_Owner.GetOrigin());
		if(!building) return;

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
		
		if(isHome || isRented || (isOwned && !isOwner))
		{
			ShowMessage("#OVT-RealEstate_CantRent");
			return;
		}

		if(!isOwner)
		{
			int cost = m_RealEstate.GetRentPrice(building);

			if(account == 0)
			{
				if(!m_Economy.PlayerHasMoney(m_sPlayerID, cost))
				{
					ShowMessage("#OVT-RealEstate_CantAfford");
					return;
				}
			}else if(account == 1)
			{
				if(!m_Economy.ResistanceHasMoney(cost))
				{
					ShowMessage("#OVT-RealEstate_CantAfford");
					return;
				}
			}
		}

		OVT_Global.GetServer().RentBuilding(m_iPlayerID, account == 1);

		// Optimistic - see Buy().
		Refresh();
	}
	
	protected void StopRenting(SCR_ButtonTextComponent btn)
	{
		int account = GetCurrentAccount();
		
		IEntity building = m_RealEstate.GetNearestBuilding(m_Owner.GetOrigin());
		if(!building) return;

		EntityID id = building.GetID();

		bool isRenter = m_RealEstate.IsRenter(m_sPlayerID, id);
		bool isResistanceRented = false;

		isResistanceRented = m_RealEstate.GetRenterID(building) == "resistance";
		if(isResistanceRented && account == 1)
		{
			isRenter = true;
		}

		if(!isRenter)
		{
			ShowMessage("#OVT-RealEstate_NotRenter");
			return;
		}

		OVT_Global.GetServer().StopRentingBuilding(m_iPlayerID, account == 1);

		// Optimistic - see Buy().
		Refresh();
	}
	
	protected void SetAsHome(SCR_ButtonTextComponent btn)
	{
		IEntity building = m_RealEstate.GetNearestBuilding(m_Owner.GetOrigin());
		if(!building) return;

		EntityID id = building.GetID();

		bool isOwner = m_RealEstate.IsOwner(m_sPlayerID, id);
		bool isHome = m_RealEstate.IsHome(m_sPlayerID, id);

		if(!isOwner)
		{
			ShowMessage("#OVT-RealEstate_NotOwner");
			return;
		}
		if(isHome)
		{
			ShowMessage("#OVT-RealEstate_AlreadyHome");
			return;
		}

		OVT_Global.GetServer().SetHome(m_iPlayerID);

		// Optimistic - see Buy().
		Refresh();
	}
}