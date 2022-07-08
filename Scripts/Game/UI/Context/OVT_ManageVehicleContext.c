class OVT_ManageVehicleContext : OVT_UIContext
{
	Vehicle m_Vehicle;
	OVT_VehicleUpgrade m_SelectedUpgrade;
		
	override void OnShow()
	{			
		m_Vehicle = null;
		GetGame().GetWorld().QueryEntitiesBySphere(m_Owner.GetOrigin(), 5, null, FilterVehicleEntities, EQueryEntitiesFlags.ALL);
			
		if(!m_Vehicle){
			CloseLayout();
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-NoVehicleOnRamp");
			return;
		}
		
		Widget upgradeButton = m_wRoot.FindAnyWidget("UpgradeButton");
		ButtonActionComponent action = ButtonActionComponent.Cast(upgradeButton.FindHandler(ButtonActionComponent));
		
		action.GetOnAction().Insert(Upgrade);
		
		Refresh();		
	}
	
	override void OnClose()
	{
		
	}
	
	protected bool FilterVehicleEntities(IEntity entity)
	{
		if(entity.ClassName() == "Vehicle")
		{
			m_Vehicle = Vehicle.Cast(entity);
		}
		return false;
	}
	
	protected void Refresh()
	{
		ItemPreviewWidget img = ItemPreviewWidget.Cast(m_wRoot.FindAnyWidget("VehiclePreview"));
		TextWidget text = TextWidget.Cast(m_wRoot.FindAnyWidget("VehicleName"));
		Widget grid = m_wRoot.FindAnyWidget("UpgradesGrid");
						
		ItemPreviewManagerEntity manager = GetGame().GetItemPreviewManager();
		if (!manager)
			return;
		
		// Set rendering and preview properties 		
		img.SetResolutionScale(1, 1);
		
		SCR_EditableVehicleComponent veh = SCR_EditableVehicleComponent.Cast(m_Vehicle.FindComponent(SCR_EditableVehicleComponent));
		if(!veh) return;
		SCR_EditableEntityUIInfo info = SCR_EditableEntityUIInfo.Cast(veh.GetInfo());
		if(!info) return;
		
		text.SetText(info.GetName());		
		manager.SetPreviewItem(img, m_Vehicle);		
		
		ResourceName vehres = m_Vehicle.GetPrefabData().GetPrefabName();
		
		OVT_VehicleUpgrades upgrades;
		foreach(OVT_VehicleUpgrades	u : OVT_Global.GetResistanceFaction().m_aVehicleUpgrades)
		{
			if(vehres == u.m_pBasePrefab)
			{
				upgrades = u;
				break;
			}
		}
		
		if(!upgrades)
		{
			m_wRoot.FindAnyWidget("UpgradesHeader").SetOpacity(0);
			m_wRoot.FindAnyWidget("UpgradesGrid").SetOpacity(0);
		}else{
			m_wRoot.FindAnyWidget("UpgradesHeader").SetOpacity(1);
			m_wRoot.FindAnyWidget("UpgradesGrid").SetOpacity(1);
			
			int wi = 0;
			foreach(int i, OVT_VehicleUpgrade upgrade : upgrades.m_aUpgrades)
			{
				Widget w = grid.FindWidget("ShopMenu_Card" + wi);	
				if(wi == 0) SelectItem(upgrade.m_pUpgradePrefab);			
			
				w.SetOpacity(1);
				OVT_ShopMenuCardComponent card = OVT_ShopMenuCardComponent.Cast(w.FindHandler(OVT_ShopMenuCardComponent));
								
				card.Init(upgrade.m_pUpgradePrefab, upgrade.m_iCost, -1, this);
				
				wi++;
			}
			
			for(; wi < 4; wi++)
			{
				Widget w = grid.FindWidget("ShopMenu_Card" + wi);			
				w.SetOpacity(0);
			}
		}
	}
	
	void Upgrade()
	{
		if(!m_SelectedUpgrade) return;
		if(!m_Vehicle) return;
		
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy.LocalPlayerHasMoney(m_SelectedUpgrade.m_iCost))
		{
			CloseLayout();
			ShowHint("#OVT-CannotAfford");
			return;
		}
		
		economy.TakeLocalPlayerMoney(m_SelectedUpgrade.m_iCost);
		
		OVT_Global.GetServer().UpgradeVehicle(m_Vehicle, m_SelectedUpgrade);		
	}
	
	override void SelectItem(ResourceName res)
	{		
		foreach(OVT_VehicleUpgrades	u : OVT_Global.GetResistanceFaction().m_aVehicleUpgrades)
		{
			foreach(OVT_VehicleUpgrade upgrade : u.m_aUpgrades)
			{
				if(upgrade.m_pUpgradePrefab == res)
				{
					m_SelectedUpgrade = upgrade;
					return;
				}
			}
		}
	}
}