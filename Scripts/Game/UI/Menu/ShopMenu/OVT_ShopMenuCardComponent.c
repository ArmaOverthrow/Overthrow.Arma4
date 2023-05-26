class OVT_ShopMenuCardComponent : SCR_ScriptedWidgetComponent
{
	protected ResourceName m_Resource;
	protected OVT_UIContext m_Context;
	
	void Init(ResourceName res, int cost, int qty, OVT_UIContext context)
	{	
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();	
		IEntity spawnedItem = GetGame().SpawnEntityPrefabLocal(Resource.Load(res));
		EPF_PersistenceComponent persist = EPF_Component<EPF_PersistenceComponent>.Find(spawnedItem);
		if(persist)
			persist.Delete();
		
		m_Resource = res;
		m_Context = context;
		
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		
		TextWidget text = TextWidget.Cast(m_wRoot.FindAnyWidget("EntityName"));
		
		TextWidget costWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("Cost"));
		if(cost == -1)
		{
			costWidget.SetVisible(false);
		}else{
			costWidget.SetText("$" + cost);
		}		
		
		TextWidget qtyWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("Stock"));
		if(qty == -1)
		{
			qtyWidget.SetVisible(false);
		}else{
			qtyWidget.SetText(qty.ToString());
		}
		
		
		ItemPreviewWidget img = ItemPreviewWidget.Cast(m_wRoot.FindAnyWidget("Image"));
		ImageWidget tex = ImageWidget.Cast(m_wRoot.FindAnyWidget("Texture"));
				
		ItemPreviewManagerEntity manager = GetGame().GetItemPreviewManager();
		if (!manager)
			return;
		
		// Set rendering and preview properties 
		
		img.SetResolutionScale(1, 1);
		
		SCR_EditableVehicleComponent veh = SCR_EditableVehicleComponent.Cast(spawnedItem.FindComponent(SCR_EditableVehicleComponent));
		if(veh){
			SCR_EditableEntityUIInfo info = SCR_EditableEntityUIInfo.Cast(veh.GetInfo());
			if(info)
			{
				text.SetText(info.GetName());
				img.SetVisible(false);
				tex.SetVisible(true);
				tex.LoadImageTexture(0, info.GetImage());
			}
		}else{
			InventoryItemComponent inv = InventoryItemComponent.Cast(spawnedItem.FindComponent(InventoryItemComponent));
			if(inv){
				manager.SetPreviewItemFromPrefab(img, res);			
				
				SCR_ItemAttributeCollection attr = SCR_ItemAttributeCollection.Cast(inv.GetAttributes());
				if(attr)
				{
					UIInfo info = attr.GetUIInfo();
					text.SetText(info.GetName());
				}
			}
		}		
		
		SCR_EntityHelper.DeleteEntityAndChildren(spawnedItem);	
	}
	
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if(!m_Context) return false;
		
		super.OnClick(w, x, y, button);
		if (button != 0)
			return false;
		
		//m_Context.StartPlace(m_Placeable);
		m_Context.SelectItem(m_Resource);
		
		return true;
	}
}