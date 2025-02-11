class OVT_MapPOIData : Managed
{
	ref SCR_UIInfo m_UiInfo;
	bool m_bMustOwnBase;
	vector m_vPos;
}

class OVT_MapIcons : SCR_MapUIBaseComponent
{		
	[Attribute()]
	protected ResourceName m_Layout;
	
	[Attribute()]
	protected ResourceName m_Imageset;
	
	[Attribute()]
	protected ref SCR_UIInfo m_POIDefaultIcon;
	
	[Attribute()]
	protected float m_fCeiling;
	
	protected ref array<vector> m_Centers;
	protected ref array<float> m_Ranges;
	protected ref array<ref Widget> m_Widgets;
	protected ref array<ref Widget> m_POIWidgets;
	
	protected static ref array<ref OVT_MapPOIData> m_aPOIs = {};
	
	static void RegisterPOI(SCR_UIInfo info, vector pos, bool mustOwnBase=false)
	{
		OVT_MapPOIData data();
		data.m_UiInfo = info;
		data.m_vPos = pos;
		data.m_bMustOwnBase = mustOwnBase;
		
		m_aPOIs.Insert(data);
	}
		
	override void Update(float timeSlice)
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
		
		foreach(int i, Widget w : m_POIWidgets)
		{
			OVT_MapPOIData poi = m_aPOIs[i];
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
			SizeLayoutWidget size = SizeLayoutWidget.Cast(w.FindAnyWidget("ImageLayout"));
			if(m_MapEntity.GetCurrentZoom() < 1)
			{
				m_POIDefaultIcon.SetIconTo(image);
				FrameSlot.SetSize(size, 12, 12);
			}else{
				poi.m_UiInfo.SetIconTo(image);
				FrameSlot.SetSize(size, 44, 44);
			}
			vector pos = poi.m_vPos;
			
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
		m_POIWidgets = new array<ref Widget>;
		
		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		OVT_OverthrowConfigComponent otconfig = OVT_Global.GetConfig();
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
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
		
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(persId);
		
		foreach(OVT_MapPOIData data : m_aPOIs)
		{	
			if(data.m_bMustOwnBase)
			{
				OVT_BaseData base = occupying.GetNearestBase(data.m_vPos);
				if(!base || base.IsOccupyingFaction()) continue;				
			}		
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
			data.m_UiInfo.SetIconTo(image);	
			
			m_POIWidgets.Insert(w);
		}
		
		foreach(EntityID id : houses)
		{
			IEntity ent = world.FindEntityByID(id);
			OVT_RealEstateConfig bdgConfig = realEstate.GetConfig(ent);
			if(bdgConfig.m_IsWarehouse)
			{
				continue;
			}
			
			int range = m_fCeiling;
			if(realEstate.IsHome(persId, ent.GetID()))
			{
				range = 0;
			}
			
			m_Centers.Insert(ent.GetOrigin());
			m_Ranges.Insert(range);			
			
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
			image.LoadImageFromSet(0, m_Imageset, "house");					
			
			m_Widgets.Insert(w);
		}
		
		houses = realEstate.GetRented(persId);
		
		foreach(EntityID id : houses)
		{
			IEntity ent = world.FindEntityByID(id);
			OVT_RealEstateConfig bdgConfig = realEstate.GetConfig(ent);
			if(bdgConfig.m_IsWarehouse)
			{
				continue;
			}
			m_Centers.Insert(ent.GetOrigin());
			m_Ranges.Insert(m_fCeiling);
			
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
			image.LoadImageFromSet(0, m_Imageset, "house");
			image.SetColor(Color.Gray25);
			
			m_Widgets.Insert(w);
		}
		
		//Public Owned Warehouses
		
		for(int i = 0; i < realEstate.m_mOwners.Count(); i++)
		{
			vector pos = realEstate.m_mOwners.GetKey(i);			
			IEntity ent = realEstate.GetNearestBuilding(pos);
			OVT_RealEstateConfig bdgConfig = realEstate.GetConfig(ent);
			if(!bdgConfig.m_IsWarehouse)
			{
				continue;
			}
			m_Centers.Insert(ent.GetOrigin());
			m_Ranges.Insert(0);
			
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
			image.LoadImageFromSet(0, m_Imageset, "warehouse");
			
			m_Widgets.Insert(w);
		}
		
		//Public Rented Warehouses
		
		for(int i = 0; i < realEstate.m_mRenters.Count(); i++)
		{
			vector pos = realEstate.m_mRenters.GetKey(i);
			IEntity ent = realEstate.GetNearestBuilding(pos);
			OVT_RealEstateConfig bdgConfig = realEstate.GetConfig(ent);
			if(!bdgConfig.m_IsWarehouse)
			{
				continue;
			}
			m_Centers.Insert(ent.GetOrigin());
			m_Ranges.Insert(0);
			
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
			image.LoadImageFromSet(0, m_Imageset, "warehouse");
			image.SetColor(Color.Gray25);
			
			m_Widgets.Insert(w);
		}
		
		foreach(RplId id : economy.GetGunDealers())
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			if(!rpl) continue;
			IEntity ent = rpl.GetEntity();
			m_Centers.Insert(ent.GetOrigin());
			m_Ranges.Insert(m_fCeiling);
			
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
			m_Ranges.Insert(m_fCeiling);
			
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
			if(!ent) continue;
			m_Centers.Insert(ent.GetOrigin());
			m_Ranges.Insert(m_fCeiling);
			
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
			image.LoadImageFromSet(0, m_Imageset, "vehicle");
			
			vector angles = ent.GetYawPitchRoll();
			
			image.SetRotation(angles[0]);
			
			m_Widgets.Insert(w);
		}
		
		foreach(OVT_FOBData fob : resistance.m_FOBs)
		{			
			m_Centers.Insert(fob.location);
			m_Ranges.Insert(0);
			
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
						
			image.LoadImageFromSet(0, m_Imageset, "fob");			
			
			Faction faction = GetGame().GetFactionManager().GetFactionByIndex(fob.faction);
			image.SetColor(faction.GetFactionColor());
			
			m_Widgets.Insert(w);
		}
		
		foreach(OVT_RadioTowerData tower : occupying.m_RadioTowers)
		{			
			m_Centers.Insert(tower.location);
			m_Ranges.Insert(0);
			
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
						
			image.LoadImageFromSet(0, m_Imageset, "tower");	
			
			Faction faction = GetGame().GetFactionManager().GetFactionByIndex(tower.faction);
			image.SetColor(faction.GetFactionColor());
								
			m_Widgets.Insert(w);
		}
		
		if(player.camp[0] != 0)
		{
			m_Centers.Insert(player.camp);
			m_Ranges.Insert(0);
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
						
			image.LoadImageFromSet(0, m_Imageset, "camp");			
			m_Widgets.Insert(w);
		}
		
		foreach(RplId id : economy.GetAllPorts())
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			m_Centers.Insert(rpl.GetEntity().GetOrigin());
			m_Ranges.Insert(0);
			
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
						
			image.LoadImageFromSet(0, m_Imageset, "port");			
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