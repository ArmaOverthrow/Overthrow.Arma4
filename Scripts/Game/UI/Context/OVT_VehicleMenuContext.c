class OVT_VehicleMenuContext : OVT_UIContext
{
	OVT_TownManagerComponent m_TownManager;
	OVT_RealEstateManagerComponent m_RealEstate;
		
	override void PostInit()
	{		
		m_TownManager = OVT_Global.GetTowns();
		m_RealEstate = OVT_Global.GetRealEstate();
	}
	
	override bool CanShowLayout()
	{
		SCR_CompartmentAccessComponent compartment = SCR_CompartmentAccessComponent.Cast(m_Owner.FindComponent(SCR_CompartmentAccessComponent));
		if(!compartment) return false;
		
		if(compartment.IsInCompartment() && compartment.GetCompartmentType(compartment.GetCompartment()) == ECompartmentType.Pilot){
			return true;
		}
		return false;
	}
	
	override void OnShow()
	{
		OVT_TownData town = m_TownManager.GetNearestTown(m_Owner.GetOrigin());
		
		SCR_CompartmentAccessComponent compartment = SCR_CompartmentAccessComponent.Cast(m_Owner.FindComponent(SCR_CompartmentAccessComponent));
		if(!compartment) return;
				
		IEntity entity = compartment.GetVehicle();
		if(entity)
		{	
			SCR_EditableVehicleComponent veh = SCR_EditableVehicleComponent.Cast(entity.FindComponent(SCR_EditableVehicleComponent));
			SCR_EditableEntityUIInfo info = SCR_EditableEntityUIInfo.Cast(veh.GetInfo());
			string name = info.GetName();
			TextWidget w = TextWidget.Cast(m_wRoot.FindAnyWidget("VehicleNameText"));
			w.SetText(name);			
		}
		
		string owner = OVT_Global.GetVehicles().GetOwnerID(entity);
		int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(owner);
		string ownerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		
		TextWidget w = TextWidget.Cast(m_wRoot.FindAnyWidget("VehicleInfoText"));
		w.SetText("#OVT-Owner: " + ownerName);
		
		SCR_ButtonTextComponent comp = SCR_ButtonTextComponent.GetButtonText("PutInWarehouse", m_wRoot);
		if (comp)
		{
			GetGame().GetWorkspace().SetFocusedWidget(comp.GetRootWidget());
			comp.m_OnClicked.Insert(PutInWarehouse);
		}
	}
	
	protected void PutInWarehouse()
	{
		SCR_CompartmentAccessComponent compartment = SCR_CompartmentAccessComponent.Cast(m_Owner.FindComponent(SCR_CompartmentAccessComponent));
		if(!compartment) return;
				
		IEntity nearestVeh = compartment.GetVehicle();
		
		SCR_VehicleInventoryStorageManagerComponent vehicleStorage = SCR_VehicleInventoryStorageManagerComponent.Cast(nearestVeh.FindComponent(SCR_VehicleInventoryStorageManagerComponent));
		if(!vehicleStorage)
		{
			return;
		}		
		
		array<IEntity> items = new array<IEntity>;
		vehicleStorage.GetItems(items);
		if(items.Count() == 0) {
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-VehicleEmpty");
			return;
		}
		
		OVT_Global.GetServer().TransferToWarehouse(nearestVeh);	
		SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-VehicleUnloaded");
	}
}