class OVT_MapIcons : SCR_MapUIBaseComponent
{		
	[Attribute()]
	protected ResourceName m_Layout;
	
	[Attribute()]
	protected ResourceName m_Imageset;
	
	[Attribute()]
	protected int m_iCeiling;
	
	protected ref array<vector> m_Centers;
	protected ref array<ref Widget> m_Widgets;
		
	override void Update()
	{
		foreach(int i, Widget w : m_Widgets)
		{
			if(m_MapEntity.GetCurrentZoom() < m_iCeiling)
			{
				w.SetOpacity(0);
				continue;
			}else{
				w.SetOpacity(1);
			}
			vector pos = m_Centers[i];
			
			float x, y;
			m_MapEntity.WorldToScreen(pos[0], pos[2], x, y, true);
	
			x = GetGame().GetWorkspace().DPIUnscale(x);
			y = GetGame().GetWorkspace().DPIUnscale(y);
			
			FrameSlot.SetPos(w, x, y);
		}
	}
		
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);
		
		m_Centers = new array<vector>;
		m_Widgets = new array<ref Widget>;
		
		OVT_RealEstateManagerComponent realEstate = OVT_RealEstateManagerComponent.GetInstance();
		OVT_EconomyManagerComponent economy = OVT_EconomyManagerComponent.GetInstance();
		OVT_OverthrowConfigComponent otconfig = OVT_OverthrowConfigComponent.GetInstance();
		OVT_VehicleManagerComponent vehicles = OVT_VehicleManagerComponent.GetInstance();
		
		ChimeraCharacter playerEntity = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!playerEntity)
		{
			return;
		}
		
		int playerID = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(playerEntity);
			
		BaseWorld world = GetGame().GetWorld();
		
		set<EntityID> houses = realEstate.GetOwned(playerID);
		
		foreach(EntityID id : houses)
		{
			IEntity ent = world.FindEntityByID(id);
			m_Centers.Insert(ent.GetOrigin());
			
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
			image.LoadImageFromSet(0, m_Imageset, "house");
			
			m_Widgets.Insert(w);
		}
		
		foreach(EntityID id : economy.GetGunDealers())
		{
			IEntity ent = world.FindEntityByID(id);
			m_Centers.Insert(ent.GetOrigin());
			
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
			image.LoadImageFromSet(0, m_Imageset, "gundealer");
			
			m_Widgets.Insert(w);
		}
		
		foreach(EntityID id : economy.GetAllShops())
		{
			IEntity ent = world.FindEntityByID(id);
			m_Centers.Insert(ent.GetOrigin());
			
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
			OVT_ShopComponent shop = OVT_ShopComponent.Cast(ent.FindComponent(OVT_ShopComponent));
			
			string icon = "shop";
			switch(shop.m_ShopType)
			{
				case OVT_ShopType.SHOP_ELECTRONIC:
					icon = "electronics";
					break;
				case OVT_ShopType.SHOP_CLOTHES:
					icon = "clothes";
					break;
				case OVT_ShopType.SHOP_DRUG:
					icon = "pharmacy";
					break;
				case OVT_ShopType.SHOP_VEHICLE:
					icon = "vehicles";
					break;
			}
			
			image.LoadImageFromSet(0, m_Imageset, icon);
			
			m_Widgets.Insert(w);
		}
		
		foreach(EntityID id : vehicles.GetOwned(playerID))
		{
			IEntity ent = world.FindEntityByID(id);
			m_Centers.Insert(ent.GetOrigin());
			
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
			image.LoadImageFromSet(0, m_Imageset, "vehicle");
			
			vector angles = ent.GetYawPitchRoll();
			
			image.SetRotation(angles[0]);
			
			m_Widgets.Insert(w);
		}
		
	}
	
	override void OnMapClose(MapConfiguration config)
	{
		foreach(Widget w : m_Widgets)
		{
			w.RemoveFromHierarchy();
		}
		m_Widgets.Clear();
		m_Centers.Clear();
	}
}