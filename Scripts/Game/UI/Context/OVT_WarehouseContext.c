class OVT_WarehouseContext : OVT_UIContext
{	
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Inventory Item Layout", params: "layout")]
	ResourceName m_ItemLayout;
	
	protected OVT_WarehouseData m_Warehouse;
	protected int m_SelectedResource = -1;
	protected ResourceName m_SelectedResourceName;
	
	
	override void OnShow()
	{	
		
		Widget take1 = m_wRoot.FindAnyWidget("Take1Button");
		ButtonActionComponent action = ButtonActionComponent.Cast(take1.FindHandler(ButtonActionComponent));		
		action.GetOnAction().Insert(TakeOne);
		
		Widget take10 = m_wRoot.FindAnyWidget("Take10Button");
		action = ButtonActionComponent.Cast(take10.FindHandler(ButtonActionComponent));		
		action.GetOnAction().Insert(TakeTen);
		
		Widget take100 = m_wRoot.FindAnyWidget("Take100Button");
		action = ButtonActionComponent.Cast(take100.FindHandler(ButtonActionComponent));		
		action.GetOnAction().Insert(TakeHundred);
		
		Widget closeButton = m_wRoot.FindAnyWidget("CloseButton");
		SCR_NavigationButtonComponent btn = SCR_NavigationButtonComponent.Cast(closeButton.FindHandler(SCR_NavigationButtonComponent));		
		btn.m_OnClicked.Insert(CloseLayout);
		
		Refresh();		
	}
	
	void Refresh()
	{
		if(!m_Warehouse) return;
		
		Widget container = m_wRoot.FindAnyWidget("Inventory");
		WorkspaceWidget workspace = GetGame().GetWorkspace(); 
		
		int wi = 0;
		
		while(container.GetChildren())
			container.RemoveChild(container.GetChildren());
				
		for(int i = 0; i < m_Warehouse.inventory.Count(); i++)
		{			
			string id = m_Warehouse.inventory.GetKey(i);
			int qty = m_Warehouse.inventory[id];
			if(qty == 0) continue;
						
			if(wi == 0 && m_SelectedResource == -1){
				SelectItem(id);
			}
			
			Widget ww = workspace.CreateWidgets(m_ItemLayout, container);
			OVT_WarehouseInventoryItemComponent card = OVT_WarehouseInventoryItemComponent.Cast(ww.FindHandler(OVT_WarehouseInventoryItemComponent));
									
			card.Init(id, qty, this);
			
			wi++;
		}		
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

		int qty = m_Warehouse.inventory[res];
						
		UIInfo info = OVT_Global.GetItemUIInfo(res);
		if(info)
		{
			typeName.SetText(info.GetName());
			details.SetText("$" + m_Economy.GetPrice(id).ToString());
			desc.SetText(info.GetDescription());
		}	
		
		ItemPreviewManagerEntity manager = GetGame().GetItemPreviewManager();
		if (!manager)
			return;
		
		manager.SetPreviewItemFromPrefab(img, res);		
	}
	
	void SetWarehouse(OVT_WarehouseData warehouse)
	{
		m_Warehouse = warehouse;
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
	
	void Take(int qty)
	{
		if(m_Warehouse.inventory[m_SelectedResourceName] < qty)
		{
			qty = m_Warehouse.inventory[m_SelectedResourceName];
		}
		if(qty > 0){
			SCR_CompartmentAccessComponent compartment = SCR_CompartmentAccessComponent.Cast(m_Owner.FindComponent(SCR_CompartmentAccessComponent));
			if(!compartment) return;
				
			IEntity entity = compartment.GetVehicle();
			if(entity)
			{	
				OVT_Global.GetServer().TakeFromWarehouseToVehicle(m_Warehouse.id, m_SelectedResourceName, qty, entity);
			}
		}
		Refresh();
		SelectItem(m_SelectedResourceName);
	}	
	
	void TakeOne(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		Take(1);
	}
	
	void TakeTen(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		Take(10);
	}
	
	void TakeHundred(Widget src, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		Take(100);
	}
}