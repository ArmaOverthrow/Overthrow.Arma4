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
	
	// Enhanced tracking for icon refresh and retry logic
	protected float m_fLastRefreshTime = 0;
	protected float m_fRefreshInterval = 3.0; // More frequent refresh
	protected float m_fRetryInterval = 1.0; // Quick retry for failed icons
	protected float m_fLastRetryTime = 0;
	
	// Retry tracking for failed replication lookups
	protected ref array<RplId> m_aFailedGunDealers;
	protected ref array<RplId> m_aFailedShops;
	protected ref array<RplId> m_aFailedPorts;
	protected ref map<RplId, vector> m_mCachedPositions; // Cache last known positions
	protected ref map<RplId, string> m_mCachedIconTypes; // Cache icon types
	
	protected static ref array<ref OVT_MapPOIData> m_aPOIs = {};
	
	static void RegisterPOI(SCR_UIInfo info, vector pos, bool mustOwnBase=false)
	{
		OVT_MapPOIData data();
		data.m_UiInfo = info;
		data.m_vPos = pos;
		data.m_bMustOwnBase = mustOwnBase;
		
		m_aPOIs.Insert(data);
	}
	
	// Initialize retry tracking arrays
	void InitializeRetryTracking()
	{
		if(!m_aFailedGunDealers) m_aFailedGunDealers = new array<RplId>;
		if(!m_aFailedShops) m_aFailedShops = new array<RplId>;
		if(!m_aFailedPorts) m_aFailedPorts = new array<RplId>;
		if(!m_mCachedPositions) m_mCachedPositions = new map<RplId, vector>;
		if(!m_mCachedIconTypes) m_mCachedIconTypes = new map<RplId, string>;
	}
	
	// Method to attempt creating icons from failed replication IDs
	void RetryFailedIcons()
	{
		bool anyRetrySuccess = false;
		
		// Retry failed gun dealers
		for(int i = m_aFailedGunDealers.Count() - 1; i >= 0; i--)
		{
			RplId id = m_aFailedGunDealers[i];
			if(TryCreateGunDealerIcon(id))
			{
				m_aFailedGunDealers.RemoveOrdered(i);
				anyRetrySuccess = true;
			}
		}
		
		// Retry failed shops
		for(int i = m_aFailedShops.Count() - 1; i >= 0; i--)
		{
			RplId id = m_aFailedShops[i];
			if(TryCreateShopIcon(id))
			{
				m_aFailedShops.RemoveOrdered(i);
				anyRetrySuccess = true;
			}
		}
		
		// Retry failed ports
		for(int i = m_aFailedPorts.Count() - 1; i >= 0; i--)
		{
			RplId id = m_aFailedPorts[i];
			if(TryCreatePortIcon(id))
			{
				m_aFailedPorts.RemoveOrdered(i);
				anyRetrySuccess = true;
			}
		}
		
		if(anyRetrySuccess)
		{
			Print("Successfully retried some failed icons");
		}
	}
	
	// Helper method to try creating gun dealer icon
	bool TryCreateGunDealerIcon(RplId id)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
		if(!rpl) return false;
		
		IEntity ent = rpl.GetEntity();
		if(!ent || !ent.GetWorld()) return false;
		
		// Cache the position for future fallback
		vector pos = ent.GetOrigin();
		m_mCachedPositions[id] = pos;
		
		m_Centers.Insert(pos);
		m_Ranges.Insert(m_fCeiling);
		
		Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
		if(!w) return false;
		
		ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
		if(image)
			image.LoadImageFromSet(0, m_Imageset, "gundealer");
		
		m_Widgets.Insert(w);
		return true;
	}
	
	// Helper method to try creating shop icon
	bool TryCreateShopIcon(RplId id)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
		if(!rpl) return false;
		
		IEntity ent = rpl.GetEntity();
		if(!ent || !ent.GetWorld()) return false;
		
		vector pos = ent.GetOrigin();
		m_mCachedPositions[id] = pos;
		
		m_Centers.Insert(pos);
		m_Ranges.Insert(m_fCeiling);
		
		Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
		if(!w) return false;
		
		ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
		OVT_ShopComponent shop = OVT_ShopComponent.Cast(ent.FindComponent(OVT_ShopComponent));
		
		string icon = "shop";
		if(shop)
		{
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
		}
		
		// Cache the icon type
		m_mCachedIconTypes[id] = icon;
		
		if(image)
			image.LoadImageFromSet(0, m_Imageset, icon);
		
		m_Widgets.Insert(w);
		return true;
	}
	
	// Helper method to try creating port icon
	bool TryCreatePortIcon(RplId id)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
		if(!rpl) return false;
		
		IEntity ent = rpl.GetEntity();
		if(!ent || !ent.GetWorld()) return false;
		
		vector pos = ent.GetOrigin();
		m_mCachedPositions[id] = pos;
		
		m_Centers.Insert(pos);
		m_Ranges.Insert(0);
		
		Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
		if(!w) return false;
		
		ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
		if(image)
			image.LoadImageFromSet(0, m_Imageset, "port");
		
		m_Widgets.Insert(w);
		return true;
	}
	
	// Create fallback icons using cached positions
	void CreateFallbackIcons()
	{
		Print("Creating fallback icons from cached positions");
		
		foreach(RplId id, vector pos : m_mCachedPositions)
		{
			// Check if this ID is in any of our failed arrays
			bool isFailedGunDealer = m_aFailedGunDealers.Find(id) != -1;
			bool isFailedShop = m_aFailedShops.Find(id) != -1;
			bool isFailedPort = m_aFailedPorts.Find(id) != -1;
			
			if(isFailedGunDealer || isFailedShop || isFailedPort)
			{
				m_Centers.Insert(pos);
				
				if(isFailedPort)
					m_Ranges.Insert(0);
				else
					m_Ranges.Insert(m_fCeiling);
				
				Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
				if(!w) continue;
				
				ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
				if(image)
				{
					string icon = "shop"; // Default
					if(isFailedGunDealer)
						icon = "gundealer";
					else if(isFailedPort)
						icon = "port";
					else if(m_mCachedIconTypes.Contains(id))
						icon = m_mCachedIconTypes[id];
					
					image.LoadImageFromSet(0, m_Imageset, icon);
					// Make fallback icons slightly transparent to indicate they're cached
					image.SetOpacity(0.7);
				}
				
				m_Widgets.Insert(w);
			}
		}
	}
		
	override void Update(float timeSlice)
	{
		// Add validation checks
		if(!m_Widgets || !m_POIWidgets || !m_Centers || !m_Ranges)
			return;
			
		float currentTime = GetGame().GetWorld().GetWorldTime();
		
		// Quick retry logic for failed icons
		if(currentTime - m_fLastRetryTime > m_fRetryInterval)
		{
			if(m_aFailedGunDealers && m_aFailedGunDealers.Count() > 0 ||
			   m_aFailedShops && m_aFailedShops.Count() > 0 ||
			   m_aFailedPorts && m_aFailedPorts.Count() > 0)
			{
				RetryFailedIcons();
			}
			m_fLastRetryTime = currentTime;
		}
		
		// Periodically validate replication references
		if(currentTime - m_fLastRefreshTime > m_fRefreshInterval)
		{
			ValidateReplicationReferences();
			m_fLastRefreshTime = currentTime;
		}
		
		foreach(int i, Widget w : m_Widgets)
		{
			if(!w || !w.GetParent()) // Check if widget still exists and has parent
			{
				continue;
			}
			
			if(m_MapEntity.GetCurrentZoom() < m_Ranges[i])
			{
				w.SetOpacity(0);
				w.SetVisible(false);
				continue;
			}else{
				w.SetOpacity(1);
				w.SetVisible(true);
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
			if(!w || !w.GetParent() || i >= m_aPOIs.Count()) // Add bounds check
			{
				continue;
			}
			
			OVT_MapPOIData poi = m_aPOIs[i];
			if(!poi) continue; // Check if POI data is valid
			
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
			SizeLayoutWidget size = SizeLayoutWidget.Cast(w.FindAnyWidget("ImageLayout"));
			
			if(!image || !size) continue; // Validate widgets exist
			
			if(m_MapEntity.GetCurrentZoom() < 1)
			{
				if(m_POIDefaultIcon)
					m_POIDefaultIcon.SetIconTo(image);
				FrameSlot.SetSize(size, 12, 12);
			}else{
				if(poi.m_UiInfo)
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
	
	// Enhanced validation method for replication references
	void ValidateReplicationReferences()
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy)
		{
			Print("Economy manager is null during validation");
			return;
		}
		
		int invalidCount = 0;
		
		// Check gun dealers more thoroughly
		array<RplId> gunDealers = economy.GetGunDealers();
		if(gunDealers && gunDealers.Count() > 0)
		{
			foreach(RplId id : gunDealers)
			{
				RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
				if(!rpl || !rpl.GetEntity() || !rpl.GetEntity().GetWorld())
				{
					invalidCount++;
					// Only add to failed list if not already there
					if(m_aFailedGunDealers.Find(id) == -1)
					{
						m_aFailedGunDealers.Insert(id);
					}
				}
			}
		}
		
		// Check shops
		array<RplId> allShops = economy.GetAllShops();
		if(allShops && allShops.Count() > 0)
		{
			foreach(RplId id : allShops)
			{
				RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
				if(!rpl || !rpl.GetEntity() || !rpl.GetEntity().GetWorld())
				{
					invalidCount++;
					if(m_aFailedShops.Find(id) == -1)
					{
						m_aFailedShops.Insert(id);
					}
				}
			}
		}
		
		// Check ports
		array<RplId> allPorts = economy.GetAllPorts();
		if(allPorts && allPorts.Count() > 0)
		{
			foreach(RplId id : allPorts)
			{
				RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
				if(!rpl || !rpl.GetEntity() || !rpl.GetEntity().GetWorld())
				{
					invalidCount++;
					if(m_aFailedPorts.Find(id) == -1)
					{
						m_aFailedPorts.Insert(id);
					}
				}
			}
		}
		
		if(invalidCount > 0)
		{
			Print("Found " + invalidCount + " invalid replication references. Failed arrays: GD=" + 
				  m_aFailedGunDealers.Count() + " S=" + m_aFailedShops.Count() + " P=" + m_aFailedPorts.Count());
		}
	}
		
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);
		
		// Initialize retry tracking
		InitializeRetryTracking();
		
		// Initialize arrays with null checks
		if(!m_Centers) m_Centers = new array<vector>;
		if(!m_Widgets) m_Widgets = new array<ref Widget>;
		if(!m_Ranges) m_Ranges = new array<float>;
		if(!m_POIWidgets) m_POIWidgets = new array<ref Widget>;
		
		// Clear existing data
		m_Centers.Clear();
		m_Widgets.Clear();
		m_Ranges.Clear();
		m_POIWidgets.Clear();
		
		// Clear failed arrays for fresh start
		m_aFailedGunDealers.Clear();
		m_aFailedShops.Clear();
		m_aFailedPorts.Clear();
		
		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		OVT_OverthrowConfigComponent otconfig = OVT_Global.GetConfig();
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		
		// Add null checks for managers
		if(!realEstate || !economy || !vehicles || !resistance || !occupying)
		{
			Print("One or more manager components are null in OnMapOpen");
			return;
		}
		
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
		
		// Clean up POI array
		if(!m_aPOIs)
		{
			m_aPOIs = new array<ref OVT_MapPOIData>;
		}
		
		// Remove any null POIs
		for(int i = m_aPOIs.Count() - 1; i >= 0; i--)
		{
			if(!m_aPOIs[i])
			{
				m_aPOIs.RemoveOrdered(i);
			}
		}
		
		foreach(OVT_MapPOIData data : m_aPOIs)
		{	
			if(!data) continue; // Skip null data
			
			if(data.m_bMustOwnBase)
			{
				OVT_BaseData base = occupying.GetNearestBase(data.m_vPos);
				if(!base || base.IsOccupyingFaction()) continue;				
			}		
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			if(!w) continue; // Check widget creation succeeded
			
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
			if(image && data.m_UiInfo)
				data.m_UiInfo.SetIconTo(image);	
			
			m_POIWidgets.Insert(w);
		}
		
		if(houses)
		{
			foreach(EntityID id : houses)
			{
				IEntity ent = world.FindEntityByID(id);
				if(!ent) continue; // Add entity validation
				
				OVT_RealEstateConfig bdgConfig = realEstate.GetConfig(ent);
				if(!bdgConfig || bdgConfig.m_IsWarehouse)
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
				if(!w) continue;
				
				ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
				if(image)
					image.LoadImageFromSet(0, m_Imageset, "house");					
				
				m_Widgets.Insert(w);
			}
		}
		
		houses = realEstate.GetRented(persId);
		
		if(houses)
		{
			foreach(EntityID id : houses)
			{
				IEntity ent = world.FindEntityByID(id);
				if(!ent) continue; // Add entity validation
				
				OVT_RealEstateConfig bdgConfig = realEstate.GetConfig(ent);
				if(!bdgConfig || bdgConfig.m_IsWarehouse)
				{
					continue;
				}
				m_Centers.Insert(ent.GetOrigin());
				m_Ranges.Insert(m_fCeiling);
				
				Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
				if(!w) continue;
				
				ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
				if(image)
				{
					image.LoadImageFromSet(0, m_Imageset, "house");
					image.SetColor(Color.Gray25);
				}
				
				m_Widgets.Insert(w);
			}
		}
		
		//Public Owned Warehouses
		
		for(int i = 0; i < realEstate.m_mOwners.Count(); i++)
		{
			vector pos = realEstate.m_mOwners.GetKey(i).ToVector();			
			IEntity ent = realEstate.GetNearestBuilding(pos);
			if(!ent) continue; // Add entity validation
			
			OVT_RealEstateConfig bdgConfig = realEstate.GetConfig(ent);
			if(!bdgConfig || !bdgConfig.m_IsWarehouse)
			{
				continue;
			}
			m_Centers.Insert(ent.GetOrigin());
			m_Ranges.Insert(0);
			
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			if(!w) continue;
			
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
			if(image)
				image.LoadImageFromSet(0, m_Imageset, "warehouse");
			
			m_Widgets.Insert(w);
		}
		
		//Public Rented Warehouses
		
		for(int i = 0; i < realEstate.m_mRenters.Count(); i++)
		{
			vector pos = realEstate.m_mRenters.GetKey(i).ToVector();
			IEntity ent = realEstate.GetNearestBuilding(pos);
			if(!ent) continue; // Add entity validation
			
			OVT_RealEstateConfig bdgConfig = realEstate.GetConfig(ent);
			if(!bdgConfig || !bdgConfig.m_IsWarehouse)
			{
				continue;
			}
			m_Centers.Insert(ent.GetOrigin());
			m_Ranges.Insert(0);
			
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			if(!w) continue;
			
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
			if(image)
			{
				image.LoadImageFromSet(0, m_Imageset, "warehouse");
				image.SetColor(Color.Gray25);
			}
			
			m_Widgets.Insert(w);
		}
		
		// Gun Dealers with retry logic
		array<RplId> gunDealers = economy.GetGunDealers();
		if(gunDealers)
		{
			foreach(RplId id : gunDealers)
			{
				if(!TryCreateGunDealerIcon(id))
				{
					// Add to failed list for retry
					m_aFailedGunDealers.Insert(id);
					Print("Failed to create gun dealer icon, added to retry list");
				}
			}
		}
		
		foreach(OVT_CampData camp : resistance.m_Camps)
		{			
			// Only show public camps or camps owned by the current player
			if(camp.isPrivate && camp.owner != persId)
				continue;
				
			m_Centers.Insert(camp.location);
			m_Ranges.Insert(m_fCeiling);
			
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
						
			image.LoadImageFromSet(0, m_Imageset, "camp");
			
			Faction faction = GetGame().GetFactionManager().GetFactionByKey("FIA");
			image.SetColor(faction.GetFactionColor());
			
			m_Widgets.Insert(w);
		}
		
		foreach(OVT_FOBData fob : resistance.m_FOBs)
		{			
			m_Centers.Insert(fob.location);
			
			// Priority FOBs are always visible (range 0), regular FOBs hidden when zoomed out
			float range = m_fCeiling;
			if (fob.isPriority)
				range = 0;
			m_Ranges.Insert(range);
			
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
			ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
			
			// Use different icon for priority FOB
			string iconName = "fob";
			if (fob.isPriority)
				iconName = "fob_priority";
			image.LoadImageFromSet(0, m_Imageset, iconName);
			
			Faction faction = GetGame().GetFactionManager().GetFactionByKey("FIA");
			Color fobColor = faction.GetFactionColor();
						
			image.SetColor(fobColor);
			
			m_Widgets.Insert(w);
		}
		
		// Shops with retry logic
		array<RplId> allShops = economy.GetAllShops();
		if(allShops)
		{
			foreach(RplId id : allShops)
			{
				if(!TryCreateShopIcon(id))
				{
					// Add to failed list for retry
					m_aFailedShops.Insert(id);
					Print("Failed to create shop icon, added to retry list");
				}
			}
		}
		
		// Vehicles
		set<EntityID> ownedVehicles = vehicles.GetOwned(persId);
		if(ownedVehicles)
		{
			foreach(EntityID id : ownedVehicles)
			{
				IEntity ent = world.FindEntityByID(id);
				if(!ent) continue;
				m_Centers.Insert(ent.GetOrigin());
				m_Ranges.Insert(m_fCeiling);
				
				Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
				if(!w) continue;
				
				ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
				if(image)
				{
					image.LoadImageFromSet(0, m_Imageset, "vehicle");
					
					vector angles = ent.GetYawPitchRoll();
					image.SetRotation(angles[0]);
				}
				
				m_Widgets.Insert(w);
			}
		}
		
		
		// Radio Towers with enhanced validation
		if(occupying.m_RadioTowers)
		{
			foreach(OVT_RadioTowerData tower : occupying.m_RadioTowers)
			{	
				if(!tower) continue; // Add null check
				
				m_Centers.Insert(tower.location);
				m_Ranges.Insert(0);
				
				Widget w = GetGame().GetWorkspace().CreateWidgets(m_Layout, m_RootWidget);
				if(!w) continue;
				
				ImageWidget image = ImageWidget.Cast(w.FindAnyWidget("Image"));
				if(image)
				{
					image.LoadImageFromSet(0, m_Imageset, "tower");	
					
					Faction faction = GetGame().GetFactionManager().GetFactionByIndex(tower.faction);
					if(faction)
						image.SetColor(faction.GetFactionColor());
				}
									
				m_Widgets.Insert(w);
			}
		}
				
		// Ports with retry logic
		array<RplId> allPorts = economy.GetAllPorts();
		if(allPorts)
		{
			foreach(RplId id : allPorts)
			{
				if(!TryCreatePortIcon(id))
				{
					// Add to failed list for retry
					m_aFailedPorts.Insert(id);
					Print("Failed to create port icon, added to retry list");
				}
			}
		}
		
		// Create fallback icons for any that failed initially
		if(m_aFailedGunDealers.Count() > 0 || m_aFailedShops.Count() > 0 || m_aFailedPorts.Count() > 0)
		{
			Print("Creating fallback icons for " + (m_aFailedGunDealers.Count() + m_aFailedShops.Count() + m_aFailedPorts.Count()) + " failed icons");
			CreateFallbackIcons();
		}
	}
	
	override void OnMapClose(MapConfiguration config)
	{
		if(m_Widgets)
		{
			foreach(Widget w : m_Widgets)
			{
				if(w)
					w.RemoveFromHierarchy();
			}
			m_Widgets.Clear();
		}
		
		if(m_POIWidgets)
		{
			foreach(Widget w : m_POIWidgets)
			{
				if(w)
					w.RemoveFromHierarchy();
			}
			m_POIWidgets.Clear();
		}
		
		// Clean up arrays
		if(m_Widgets) m_Widgets = null;
		if(m_Centers) 
		{
			m_Centers.Clear();
			m_Centers = null;
		}
		if(m_Ranges)
		{
			m_Ranges.Clear();
			m_Ranges = null;
		}
		if(m_POIWidgets) m_POIWidgets = null;
		
		// Note: We keep retry arrays and cached positions for next map open
		// This helps with persistence across map sessions
	}
}