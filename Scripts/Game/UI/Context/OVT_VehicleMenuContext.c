class OVT_VehicleMenuContext : OVT_UIContext
{
	//! How far the vehicle may be from a warehouse RECORD for the two warehouse buttons to appear.
	//! Unchanged; a warehouse is ~40 m long and the vehicle is parked at one end of it.
	protected const int WAREHOUSE_SEARCH_RANGE = 40;
	
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
		
		if(compartment.IsInCompartment() && compartment.GetCompartment().GetType() == ECompartmentType.PILOT){
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
			if(player)
				ownerName = player.name;
		}
		
		TextWidget w = TextWidget.Cast(m_wRoot.FindAnyWidget("VehicleInfoText"));
		w.SetText("#OVT-Owner: " + ownerName);
		
		bool isAccessible = ResolveAccessibleWarehouse(pos) != null;
		
		SCR_ButtonTextComponent comp = SCR_ButtonTextComponent.GetButtonText("PutInWarehouse", m_wRoot);
		if (comp)
		{
			if(isAccessible){
				comp.SetVisible(true);
				GetGame().GetWorkspace().SetFocusedWidget(comp.GetRootWidget());
				comp.m_OnClicked.Insert(PutInWarehouse);
			}else{
				comp.SetVisible(false);
			}
		}
		
		comp = SCR_ButtonTextComponent.GetButtonText("TakeFromWarehouse", m_wRoot);
		if (comp)
		{
			if(isAccessible){
				comp.SetVisible(true);
				GetGame().GetWorkspace().SetFocusedWidget(comp.GetRootWidget());
				comp.m_OnClicked.Insert(TakeFromWarehouse);
			}else{
				comp.SetVisible(false);
			}
		}
		
		comp = SCR_ButtonTextComponent.GetButtonText("Import", m_wRoot);
		if (comp)
		{
			RplId port = m_Economy.GetNearestPort(pos);			
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(port));
			float dist = vector.Distance(pos, rpl.GetEntity().GetOrigin()); 
			if(dist < 20){
				comp.SetVisible(true);
				GetGame().GetWorkspace().SetFocusedWidget(comp.GetRootWidget());
				comp.m_OnClicked.Insert(Import);
			}else{
				comp.SetVisible(false);
			}
		}
		
	}
	
	//------------------------------------------------------------------------------------------------
	//! The nearby warehouse building this player may use, if any.
	//!
	//! Both warehouse buttons and the HUD prompt gate on this, and the ACCESSIBILITY half is
	//! OVT_RealEstateManagerComponent.PlayerMayUseWarehouse() - the same body the server's
	//! MayUseHolder gate calls (I5). The storage component is required as well: the buttons move a
	//! ledger, so a warehouse without one is a button that can only fail.
	//! \param[in] pos The vehicle's position.
	//! \return The building, or null.
	protected IEntity ResolveAccessibleWarehouse(vector pos)
	{
		if(!m_RealEstate) return null;
		
		OVT_WarehouseData warehouse = m_RealEstate.GetNearestWarehouse(pos, WAREHOUSE_SEARCH_RANGE);
		if(!warehouse) return null;
		
		IEntity building = m_RealEstate.GetNearestBuilding(warehouse.location, OVT_RealEstateManagerComponent.WAREHOUSE_MATCH_RANGE);
		if(!building) return null;
		
		if(!OVT_StorageUtils.GetStorage(building)) return null;
		
		if(!m_RealEstate.PlayerMayUseWarehouse(m_sPlayerID, building)) return null;
		
		return building;
	}
	
	//------------------------------------------------------------------------------------------------
	//! The one-button truck dump, now a whole-ledger move with the vanilla inventory swept into the
	//! vehicle's ledger first (logistics/storage D9).
	protected void PutInWarehouse()
	{
		SCR_CompartmentAccessComponent compartment = SCR_CompartmentAccessComponent.Cast(m_Owner.FindComponent(SCR_CompartmentAccessComponent));
		if(!compartment) return;
				
		IEntity nearestVeh = compartment.GetVehicle();
		if(!nearestVeh) return;
		
		IEntity building = ResolveAccessibleWarehouse(m_Owner.GetOrigin());
		if(!building) return;
		
		RplId source = OVT_StorageUtils.GetHolderId(nearestVeh);
		RplId dest = OVT_StorageUtils.GetHolderId(building);
		if(!source.IsValid() || !dest.IsValid()) return;
		
		OVT_StorageRequestComponent requests = OVT_ControllerComponent<OVT_StorageRequestComponent>.Get();
		if(!requests) return;
		
		CloseLayout();
		
		// The hint goes before the request: on a listen host the request runs the server's gates
		// inline, and a refusal hint drawn inside it would be overwritten by this one.
		SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-VehicleUnloaded");
		
		requests.RequestMoveAllToHolder(source, dest, true);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Opens the shared Open Storage screen on the warehouse building. There is no warehouse-only
	//! screen any more - the building is a holder like every box and truck.
	protected void TakeFromWarehouse()
	{		
		IEntity building = ResolveAccessibleWarehouse(m_Owner.GetOrigin());
		if(!building) return;
		
		OVT_StorageContext context = OVT_StorageContext.Cast(m_UIManager.GetContext(OVT_StorageContext));
		if(!context) return;
		
		context.SetHolder(building);
		
		m_UIManager.ShowContext(OVT_StorageContext);
		
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