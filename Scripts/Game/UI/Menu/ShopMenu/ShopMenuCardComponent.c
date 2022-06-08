class OVT_ShopMenuCardComponent : SCR_ScriptedWidgetComponent
{
	protected ResourceName m_Resource;
	protected OVT_ShopContext m_Context;
	
	void Init(ResourceName res, int cost, int qty, OVT_ShopContext context)
	{
		m_Resource = res;
		m_Context = context;
		
		OVT_OverthrowConfigComponent config = OVT_OverthrowConfigComponent.GetInstance();
		
		TextWidget text = TextWidget.Cast(m_wRoot.FindAnyWidget("EntityName"));
		
		TextWidget costWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("Cost"));
		costWidget.SetText("$" + cost);
		
		TextWidget qtyWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("Stock"));
		qtyWidget.SetText(qty.ToString());
		
		ItemPreviewWidget img = ItemPreviewWidget.Cast(m_wRoot.FindAnyWidget("Image"));
		
		IEntity spawnedItem = GetGame().SpawnEntityPrefabLocal(Resource.Load(res));
		
		ItemPreviewManagerEntity manager = GetGame().GetItemPreviewManager();
		if (!manager)
			return;
		
		// Set rendering and preview properties 
		manager.SetPreviewItem(img, spawnedItem);
		img.SetResolutionScale(1, 1);
		
		InventoryItemComponent inv = InventoryItemComponent.Cast(spawnedItem.FindComponent(InventoryItemComponent));
		if(inv){
			SCR_ItemAttributeCollection attr = SCR_ItemAttributeCollection.Cast(inv.GetAttributes());
			if(attr)
			{
				UIInfo info = attr.GetUIInfo();
				text.SetText(info.GetName());
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