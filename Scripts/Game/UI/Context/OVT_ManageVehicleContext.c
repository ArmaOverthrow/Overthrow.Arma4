class OVT_ManageVehicleContext : OVT_UIContext
{
	Vehicle m_Vehicle;
	OVT_VehicleUpgrade m_SelectedUpgrade;
	
	ref ButtonActionComponent m_UpgradeAction;
	ref ButtonActionComponent m_RepairAction;
		
	override void OnShow()
	{			
		m_Vehicle = null;
		GetGame().GetWorld().QueryEntitiesBySphere(m_Owner.GetOrigin(), 5, null, FilterVehicleEntities, EQueryEntitiesFlags.ALL);
					
		Widget upgradeButton = m_wRoot.FindAnyWidget("UpgradeButton");
		m_UpgradeAction = ButtonActionComponent.Cast(upgradeButton.FindHandler(ButtonActionComponent));
		
		m_UpgradeAction.GetOnAction().Insert(Upgrade);
		
		Widget repairButton = m_wRoot.FindAnyWidget("RepairButton");
		m_RepairAction = ButtonActionComponent.Cast(repairButton.FindHandler(ButtonActionComponent));
		
		m_RepairAction.GetOnAction().Insert(Repair);
		
		if(!m_Vehicle){
			CloseLayout();
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-NoVehicleOnRamp");
			return;
		}
		
		Widget closeButton = m_wRoot.FindAnyWidget("CloseButton");
		SCR_InputButtonComponent btn = SCR_InputButtonComponent.Cast(closeButton.FindHandler(SCR_InputButtonComponent));		
		btn.m_OnClicked.Insert(CloseLayout);
		
		Refresh();		
	}
	
	override void OnClose()
	{		
		m_UpgradeAction.GetOnAction().Remove(Upgrade);
		m_RepairAction.GetOnAction().Remove(Repair);
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
		TextWidget text = TextWidget.Cast(m_wRoot.FindAnyWidget("VehicleName"));
		Widget grid = m_wRoot.FindAnyWidget("UpgradesGrid");
				
		SCR_EditableVehicleComponent veh = SCR_EditableVehicleComponent.Cast(m_Vehicle.FindComponent(SCR_EditableVehicleComponent));
		if(!veh) return;
		SCR_EditableEntityUIInfo info = SCR_EditableEntityUIInfo.Cast(veh.GetInfo());
		if(!info) return;
		
		text.SetText(info.GetName());		
		
		ImageWidget tex = ImageWidget.Cast(m_wRoot.FindAnyWidget("VehiclePreview"));
		tex.LoadImageTexture(0, info.GetImage());
		
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
		
		Widget healthWidget = m_wRoot.FindAnyWidget("HealthBar");
		SCR_WLibProgressBarComponent healthBar = SCR_WLibProgressBarComponent.Cast(healthWidget.FindHandler(SCR_WLibProgressBarComponent));
		
		float health = 1;
		SCR_VehicleDamageManagerComponent dmg = SCR_VehicleDamageManagerComponent.Cast(m_Vehicle.FindComponent(SCR_VehicleDamageManagerComponent));
		if(dmg)
		{
			health = dmg.GetHealth() / dmg.GetMaxHealth();
		}		
		healthBar.SetValue(health);
		
		Widget fuelWidget = m_wRoot.FindAnyWidget("FuelBar");
		SCR_WLibProgressBarComponent fuelBar = SCR_WLibProgressBarComponent.Cast(fuelWidget.FindHandler(SCR_WLibProgressBarComponent));		
		float fuel = 1;
		
		SCR_FuelConsumptionComponent f = SCR_FuelConsumptionComponent.Cast(m_Vehicle.FindComponent(SCR_FuelConsumptionComponent));
		if(f)
		{
			BaseFuelNode node = f.GetCurrentFuelTank();
			fuel = node.GetFuel() / node.GetMaxFuel();
		}		
		fuelBar.SetValue(fuel);
		
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
		CloseLayout();	
		ShowHint("#OVT-VehicleUpgraded");	
	}
	
	void Repair()
	{
		if(!m_Vehicle) return;
		
		OVT_Global.GetServer().RepairVehicle(m_Vehicle);
		CloseLayout();	
		ShowHint("#OVT-VehicleRepaired");		
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