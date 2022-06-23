class OVT_MapIcons : SCR_MapUIBaseComponent
{		
	[Attribute()]
	protected ResourceName m_Layout;
	
	[Attribute()]
	protected ResourceName m_Imageset;
	
	[Attribute()]
	protected int m_iCeiling;
	
	protected ref array<vector> m_Centers;
	protected ref array<float> m_Ranges;
	protected ref array<ref Widget> m_Widgets;
		
	override void Update()
	{
		foreach(int i, Widget w : m_Widgets)
		{
			if(m_MapEntity.GetCurrentZoom() < m_Ranges[i])
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
		m_Ranges = new array<float>;
		
		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		OVT_OverthrowConfigComponent otconfig = OVT_Global.GetConfig();
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		OVT_JobManagerComponent jobs = OVT_Global.GetJobs();
		
		ChimeraCharacter playerEntity = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!playerEntity)
		{
			return;
		}
		
		int playerID = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(playerEntity);
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerID);
			
		BaseWorld world = GetGame().GetWorld();
		
		set<EntityID> houses = realEstate.GetOwned(persId);
		
		foreach(EntityID id : houses)
		{
			IEntity ent = world.FindEntityByID(id);
			m_Centers.Insert(ent.GetOrigin());
			m_Ranges.Insert(m_iCeiling);
			
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
			image.LoadImageFromSet(0, m_Imageset, "house");
			
			m_Widgets.Insert(w);
		}
		
		foreach(RplId id : economy.GetGunDealers())
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			if(!rpl) continue;
			IEntity ent = rpl.GetEntity();
			m_Centers.Insert(ent.GetOrigin());
			m_Ranges.Insert(m_iCeiling);
			
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
			image.LoadImageFromSet(0, m_Imageset, "gundealer");
			
			m_Widgets.Insert(w);
		}
		
		foreach(RplId id : economy.GetAllShops())
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			if(!rpl) continue;
			IEntity ent = rpl.GetEntity();
			m_Centers.Insert(ent.GetOrigin());
			m_Ranges.Insert(m_iCeiling);
			
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
		
		foreach(EntityID id : vehicles.GetOwned(persId))
		{
			IEntity ent = world.FindEntityByID(id);
			m_Centers.Insert(ent.GetOrigin());
			m_Ranges.Insert(m_iCeiling);
			
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
			image.LoadImageFromSet(0, m_Imageset, "vehicle");
			
			vector angles = ent.GetYawPitchRoll();
			
			image.SetRotation(angles[0]);
			
			m_Widgets.Insert(w);
		}
		
		foreach(RplId id : resistance.m_FOBs)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			if(!rpl) continue;
			IEntity ent = rpl.GetEntity();
			m_Centers.Insert(ent.GetOrigin());
			m_Ranges.Insert(0);
			
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
						
			image.LoadImageFromSet(0, m_Imageset, "fob");			
			m_Widgets.Insert(w);
		}
		
		if(jobs.m_vCurrentWaypoint)
		{
			m_Centers.Insert(jobs.m_vCurrentWaypoint);
			m_Ranges.Insert(0);
			
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
						
			image.LoadImageFromSet(0, m_Imageset, "waypoint");			
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
		m_Widgets = null;
		m_Centers.Clear();
		m_Centers = null;
	}
}