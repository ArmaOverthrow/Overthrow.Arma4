class OVT_ShopMenuCardComponent : SCR_ScriptedWidgetComponent
{
	protected int m_Resource;
	protected OVT_ShopContext m_Context;
	
	void Init(int id, int cost, int qty, OVT_ShopContext context)
	{	
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();	
		IEntity spawnedItem = GetGame().SpawnEntityPrefabLocal(Resource.Load(economy.GetResource(id)));
		
		m_Resource = id;
		m_Context = context;
		
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		
		TextWidget text = TextWidget.Cast(m_wRoot.FindAnyWidget("EntityName"));
		
		TextWidget costWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("Cost"));
		costWidget.SetText("$" + cost);
		
		TextWidget qtyWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("Stock"));
		qtyWidget.SetText(qty.ToString());
		
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
				PreviewRenderAttributes previewAttr = PreviewRenderAttributes.Cast(inv.GetAttributes().FindAttribute(PreviewRenderAttributes));
				if(previewAttr){
					manager.SetPreviewItem(img, spawnedItem, previewAttr);
				}else{
					manager.SetPreviewItem(img, spawnedItem);
				}				
				
				SCR_ItemAttributeCollection attr = SCR_ItemAttributeCollection.Cast(inv.GetAttributes());
				if(attr)
				{
					UIInfo info = attr.GetUIInfo();
					text.SetText(info.GetName());
				}
			}
		}		
		
		SCR_Global.DeleteEntityAndChildren(spawnedItem);	
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