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
		vector pos = m_Owner.GetOrigin();
		
		OVT_TownData town = m_TownManager.GetNearestTown(pos);
		
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
		string ownerName = "";
		if(owner != "")
		{
			OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(owner);
			ownerName = player.name;
		}
		
		TextWidget w = TextWidget.Cast(m_wRoot.FindAnyWidget("VehicleInfoText"));
		w.SetText("#OVT-Owner: " + ownerName);
		
		OVT_WarehouseData warehouse = m_RealEstate.GetNearestWarehouse(pos, 40);
		bool isAccessible = false;
		if(warehouse)
		{
			IEntity warehouseEntity = m_RealEstate.GetNearestBuilding(warehouse.location, 10);
			if(warehouseEntity)
			{
				EntityID id = warehouseEntity.GetID();
				bool isOwned = m_RealEstate.IsOwned(id);
				bool isOwner = m_RealEstate.IsOwner(m_sPlayerID, id);
				bool isRented = m_RealEstate.IsRented(id);
				isAccessible = (!warehouse.isPrivate && isOwned && !isRented) || (warehouse.isPrivate && isOwner && !isRented) || isRented;
			}			
		}
		
		SCR_ButtonTextComponent comp = SCR_ButtonTextComponent.GetButtonText("PutInWarehouse", m_wRoot);
		if (comp)
		{
			if(isAccessible){
				comp.SetEnabled(true);
				GetGame().GetWorkspace().SetFocusedWidget(comp.GetRootWidget());
				comp.m_OnClicked.Insert(PutInWarehouse);
			}else{
				comp.SetEnabled(false);
			}
		}
		
		comp = SCR_ButtonTextComponent.GetButtonText("TakeFromWarehouse", m_wRoot);
		if (comp)
		{
			if(isAccessible){
				comp.SetEnabled(true);
				GetGame().GetWorkspace().SetFocusedWidget(comp.GetRootWidget());
				comp.m_OnClicked.Insert(TakeFromWarehouse);
			}else{
				comp.SetEnabled(false);
			}
		}
		
		comp = SCR_ButtonTextComponent.GetButtonText("Import", m_wRoot);
		if (comp)
		{
			RplId port = m_Economy.GetNearestPort(pos);			
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(port));
			float dist = vector.Distance(pos, rpl.GetEntity().GetOrigin()); 
			if(dist < 20){
				comp.SetEnabled(true);
				GetGame().GetWorkspace().SetFocusedWidget(comp.GetRootWidget());
				comp.m_OnClicked.Insert(Import);
			}else{
				comp.SetEnabled(false);
			}
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
		CloseLayout();
		SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-VehicleUnloaded");
	}
	
	protected void TakeFromWarehouse()
	{		
		vector pos = m_Owner.GetOrigin();
		OVT_WarehouseData warehouse = m_RealEstate.GetNearestWarehouse(pos, 40);
		if(!warehouse) return;
		
		bool isAccessible = false;
		if(warehouse)
		{
			IEntity warehouseEntity = m_RealEstate.GetNearestBuilding(warehouse.location, 10);
			if(warehouseEntity)
			{
				EntityID id = warehouseEntity.GetID();
				bool isOwned = m_RealEstate.IsOwned(id);
				bool isOwner = m_RealEstate.IsOwner(m_sPlayerID, id);
				bool isRented = m_RealEstate.IsRented(id);
				isAccessible = (!warehouse.isPrivate && isOwned && !isRented) || (warehouse.isPrivate && isOwner && !isRented) || isRented;
			}			
		}
		if(!isAccessible) return;
		
		OVT_WarehouseContext context = OVT_WarehouseContext.Cast(m_UIManager.GetContext(OVT_WarehouseContext));
		if(!context) return;
		
		context.SetWarehouse(warehouse);
		
		m_UIManager.ShowContext(OVT_WarehouseContext);
		
		CloseLayout();
	}
	
	protected void Import()
	{	
		if(!m_PlayerData.HasPermission("Import"))	
		{
			CloseLayout();
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-CannotImport");			
			return;
		}
		vector pos = m_Owner.GetOrigin();
		RplId port = m_Economy.GetNearestPort(pos);
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(port));
		float dist = vector.Distance(pos, rpl.GetEntity().GetOrigin()); 
		if(dist > 20){
			return;
		}
		
		m_UIManager.ShowContext(OVT_PortContext);
		
		CloseLayout();
	}
}