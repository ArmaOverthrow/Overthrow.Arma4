class OVT_EconomyManagerComponentClass: OVT_ComponentClass
{
};

class OVT_PrefabItemCostConfig : ScriptAndConfig
{
	[Attribute(desc: "Prefab of entity", UIWidgets.ResourcePickerThumbnail, params: "et")]
	ResourceName m_sEntityPrefab;
	
	[Attribute("50", desc: "The cost")]
	int cost;

	[Attribute("1", desc: "Minimum number to stock")]
	int minStock;
	
	[Attribute("10", desc: "Maximum number to stock")]
	int maxStock;
}

class OVT_VehicleShopConfig : OVT_PrefabItemCostConfig
{
}

class OVT_ShopInventoryItem : ScriptAndConfig
{
	[Attribute("2", desc: "Type of item", uiwidget: UIWidgets.SearchComboBox, enums: ParamEnumArray.FromEnum(SCR_EArsenalItemType))]
	SCR_EArsenalItemType m_eItemType;
	
	[Attribute("2", desc: "Item mode", uiwidget: UIWidgets.SearchComboBox, enums: ParamEnumArray.FromEnum(SCR_EArsenalItemMode))]
	SCR_EArsenalItemMode m_eItemMode;
	
	[Attribute(desc: "String to search in prefab name, blank for all")]
	string m_sFind;

	[Attribute("true", desc: "Include buy/sell occupying faction's gear for this item")]
	bool m_bIncludeOccupyingFactionItems;
	
	[Attribute("true", desc: "Include buy/sell supporting faction's gear for this item")]
	bool m_bIncludeSupportingFactionItems;
	
	[Attribute("true", desc: "Include buy/sell other faction's gear for this item")]
	bool m_bIncludeOtherFactionItems;
		
	[Attribute(desc: "Choose a single and random item from this category")]
	bool m_bSingleRandomItem;
}

//------------------------------------------------------------------------------------------------
//! Manages the overall economy in the Overthrow game mode.
//! Handles prices, stock levels, player and resistance money, taxes, donations, and shop/port registration.
//! This is a singleton component accessible via OVT_EconomyManagerComponent.GetInstance().
class OVT_EconomyManagerComponent: OVT_Component
{
	[Attribute("", UIWidgets.Object)]
	ref OVT_ShopConfig m_ShopConfig; //!< Configuration for general shops.

	[Attribute("", UIWidgets.Object)]
	ref OVT_GunDealerConfig m_GunDealerConfig; //!< Configuration specific to gun dealers.
		
	[Attribute("", UIWidgets.Object)]
	ref OVT_PricesConfig m_PriceConfig; //!< Configuration for item prices.
	
	[Attribute("", UIWidgets.Object)]
	ref OVT_VehiclePricesConfig m_VehiclePriceConfig; //!< Configuration for vehicle prices.
			
	protected ref array<RplId> m_aAllShops; //!< List of all registered shop RplIds.
	protected ref array<RplId> m_aAllPorts; //!< List of all registered port RplIds.
	protected ref array<RplId> m_aGunDealers; //!< List of all registered gun dealer RplIds.
		
	const int ECONOMY_UPDATE_FREQUENCY = 60000; //!< Frequency (in ms) for periodic economy updates (income, stock).
	
	protected ref array<ref ResourceName> m_aResources; //!< Database of all known item/vehicle ResourceNames.
	protected ref map<ref ResourceName,int> m_aResourceIndex; //!< Mapping from ResourceName to its integer ID in m_aResources.
	protected ref array<ref SCR_EntityCatalogEntry> m_aEntityCatalogEntries; //!< Cached entity catalog entries for faster lookup.
	protected ref map<int,ref array<int>> m_mFactionResources; //!< Mapping from Faction ID to an array of resource IDs belonging to that faction.
	
	ref map<int, ref array<RplId>> m_mTownShops; //!< Mapping from Town ID to an array of shop RplIds within that town.
	
	protected ref map<int, int> m_mItemCosts; //!< Mapping from Resource ID to its base cost.
	protected ref map<int, int> m_mItemDemand; //!< Mapping from Resource ID to its demand value (influences stock and prices).
	
	protected ref array<int> m_aLegalVehicles; //!< List of resource IDs for vehicles considered legal.
	protected ref array<int> m_aAllVehicles; //!< List of resource IDs for all vehicles.
	protected ref map<int,OVT_ParkingType> m_mVehicleParking; //!< Mapping from vehicle resource ID to its required parking type.

  	protected int m_iHourPaidIncome = -1; //!< Tracks the hour when income was last calculated to prevent double payments.
	protected int m_iHourPaidStock = -1; //!< Tracks the hour when stock was last replenished.
	protected int m_iHourPaidRent = -1; //!< Tracks the hour when rent was last calculated.
	
	//Streamed to clients..			
	int m_iResistanceMoney = 0; //!< Current amount of money held by the resistance faction. Streamed to clients.
	float m_fResistanceTax = 0; //!< Current tax rate applied to player income, benefiting the resistance. Streamed to clients.
	
	//Events
	ref ScriptInvoker m_OnPlayerMoneyChanged = new ScriptInvoker(); //!< Invoked when a player's money changes. Args: string persId, int newAmount
	ref ScriptInvoker m_OnResistanceMoneyChanged = new ScriptInvoker(); //!< Invoked when the resistance money changes. Args: int newAmount
	ref ScriptInvoker m_OnPlayerBuy = new ScriptInvoker(); //!< Invoked when a player buys an item. Args: int playerId, int cost, ResourceName item, int quantity
	ref ScriptInvoker m_OnPlayerSell = new ScriptInvoker(); //!< Invoked when a player sells an item. Args: int playerId, int cost, ResourceName item, int quantity
	ref ScriptInvoker m_OnPlayerTransaction = new ScriptInvoker(); //!< Invoked when any transaction occurs (server-side). Args: int playerId, OVT_ShopComponent shop, bool isBuying, int amount
		
	static OVT_EconomyManagerComponent s_Instance; //!< Static instance for singleton access.
	
	//------------------------------------------------------------------------------------------------
	//! Gets the singleton instance of the OVT_EconomyManagerComponent.
	//! \return The static instance or null if not found.
	static OVT_EconomyManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_EconomyManagerComponent.Cast(pGameMode.FindComponent(OVT_EconomyManagerComponent));
		}

		return s_Instance;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Constructor for the OVT_EconomyManagerComponent. Initializes internal data structures.
	void OVT_EconomyManagerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_aResources = new array<ref ResourceName>;
		m_aAllShops = new array<RplId>;	
		m_aAllPorts = new array<RplId>;
		m_mItemCosts = new map<int, int>;
		m_mItemDemand = new map<int, int>;
		m_aGunDealers = new array<RplId>;
		m_mTownShops = new map<int, ref array<RplId>>;
		m_aResourceIndex = new map<ref ResourceName,int>;
		m_mFactionResources = new map<int,ref array<int>>;
		m_aEntityCatalogEntries = new array<ref SCR_EntityCatalogEntry>;
		m_aLegalVehicles = new array<int>;
		m_aAllVehicles = new array<int>;
		m_mVehicleParking = new map<int,OVT_ParkingType>;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Periodically checks game time to trigger economy updates like income calculation, stock replenishment, and rent collection.
	//! Called automatically by a timer.
	void CheckUpdate()
	{
		if(!m_Time) 
		{
			ChimeraWorld world = GetOwner().GetWorld();
			m_Time = world.GetTimeAndWeatherManager();
		}
		
		PlayerManager mgr = GetGame().GetPlayerManager();		
		if(mgr.GetPlayerCount() == 0)
		{
			return;
		}
		
		TimeContainer time = m_Time.GetTime();		
		
		//Every 6 hrs get paid
		if((time.m_iHours == 0 
			|| time.m_iHours == 6 
			|| time.m_iHours == 12 
			|| time.m_iHours == 18)
			 && 
			m_iHourPaidIncome != time.m_iHours)
		{
			m_iHourPaidIncome = time.m_iHours;
			CalculateIncome();
			UpdateShops();
		}
		
		//Every morning at 7am replenish stock
		if(time.m_iHours == 7)
		{
			if (m_iHourPaidStock != time.m_iHours) {
				m_iHourPaidStock = time.m_iHours;
				ReplenishStock();
			}
		} else {
			m_iHourPaidStock = -1;
		}
		
		//Every midnight calculate rents
		if(time.m_iHours == 0)
		{
			if (m_iHourPaidRent != time.m_iHours) {
				m_iHourPaidRent = time.m_iHours;
				UpdateRents();
			}
		} else {
			m_iHourPaidRent = -1;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the base price of an item by its resource ID.
	//! \param[in] id The resource ID of the item.
	//! \return The base price, or 500 if not found.
	int GetPrice(int id)
	{
		if(!m_mItemCosts.Contains(id)) return 500;
		return m_mItemCosts[id];
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the demand value for an item by its resource ID.
	//! \param[in] id The resource ID of the item.
	//! \return The demand value, or 5 if not found.
	int GetDemand(int id)
	{
		if(!m_mItemDemand.Contains(id)) return 5;
		return m_mItemDemand[id];
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the required parking type for a vehicle by its resource ID.
	//! \param[in] id The resource ID of the vehicle.
	//! \return The OVT_ParkingType, defaulting to PARKING_CAR if not found.
	OVT_ParkingType GetParkingType(int id)
	{
		if(!m_mVehicleParking.Contains(id)) return OVT_ParkingType.PARKING_CAR;
		return m_mVehicleParking[id];
	}
	
	//------------------------------------------------------------------------------------------------
	//! Calculates and distributes rent payments for all rented properties at midnight.
	//! Owners receive rent, renters pay rent (from player or resistance funds).
	protected void UpdateRents()
	{
		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		for(int i = 0; i < realEstate.m_mRenters.Count(); i++)
		{
			string posString = realEstate.m_mRenters.GetKey(i);
			vector pos = posString.ToVector();			
			string playerId = realEstate.m_mRenters[posString];

			IEntity building = realEstate.GetNearestBuilding(pos);
			EntityID id = building.GetID();
			int cost = realEstate.GetRentPrice(building);
			
			bool isRenter = realEstate.IsRenter(playerId, id);
			bool isOwner = realEstate.IsOwner(playerId, id);
			bool isResistanceOwned = realEstate.GetOwnerID(building) == "resistance";
			bool isResistanceRented = realEstate.GetRenterID(building) == "resistance";				
			
			if(isResistanceOwned)
			{
				AddResistanceMoney(cost);
				return;
			}else if(isResistanceRented)
			{
				if(!ResistanceHasMoney(cost))
				{
					realEstate.SetRenter(-1, building);
				}else{
					TakeResistanceMoney(cost);
				}
				return;
			}
			
			if(isOwner)
			{
				AddPlayerMoneyPersistentId(playerId, cost);
			}else{			
				if(!PlayerHasMoney(playerId, cost))
				{
					realEstate.SetRenter(-1, building);
				}else{
					TakePlayerMoneyPersistentId(playerId, cost);
				}
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Replenishes stock in shops and gun dealers based on configured minimums and maximums.
	//! Typically occurs once per day (e.g., at 7 AM).
	protected void ReplenishStock()
	{
		//Shops restocking		
		foreach(OVT_TownData town : OVT_Global.GetTowns().GetTowns())
		{
			int townID = OVT_Global.GetTowns().GetTownID(town);
			if(!m_mTownShops.Contains(townID)) continue;			
			
			//split shops into types
			map<int, ref array<RplId>> typeShops = GetShopTypes(townID);
			
			array<int> types = new array<int>;			
			for(int i=0; i<typeShops.Count(); i++)
			{
				types.Insert(typeShops.GetKey(i));
			}			
			
			foreach(RplId shopId : m_aAllShops)
			{
				OVT_ShopComponent shop = GetShopByRplId(shopId);
				for(int i = 0; i<shop.m_aInventory.Count(); i++)
				{
					int id = shop.m_aInventory.GetKey(i);
					int max = GetTownMaxStock(townID, id);
					int numShops = 1;					
					if(typeShops.Contains(shop.m_ShopType))
						numShops = typeShops[shop.m_ShopType].Count();
					max = Math.Round(max / numShops);
					int stock = shop.GetStock(id);
					int half = Math.Round(stock * 0.5);
					if(stock < half)
					{
						shop.AddToInventory(id, half - stock);
					}
				}
			}
		}

		//Gun dealer restocking (Item prefabs only ie weed)
		foreach(RplId id : m_aGunDealers)
		{
			OVT_ShopComponent shop = GetShopByRplId(id);
			if(!shop) continue;
			foreach(OVT_PrefabItemCostConfig item : m_GunDealerConfig.m_aGunDealerItemPrefabs)
			{
				int currentStock = shop.GetStock(GetInventoryId(item.m_sEntityPrefab));
				if(currentStock < item.minStock)
				{
					int num = Math.Round(s_AIRandomGenerator.RandInt(item.minStock,item.maxStock));
					shop.AddToInventory(GetInventoryId(item.m_sEntityPrefab), num);
				}				
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Simulates NPCs buying items from shops based on town population, stability, and item demand.
	//! Drives stock reduction and generates income for shop owners (indirectly).
	protected void UpdateShops()
	{
		//NPCs Buying stock
		foreach(OVT_TownData town : OVT_Global.GetTowns().GetTowns())
		{
			int townID = OVT_Global.GetTowns().GetTownID(town);
			if(!m_mTownShops.Contains(townID)) continue;
			int maxToBuy = (town.population * OVT_Global.GetConfig().m_fNPCBuyRate) * (town.stability / 100);
			
			int numToBuy = s_AIRandomGenerator.RandInt(1,maxToBuy);
			
			//split shops into types
			map<int, ref array<RplId>> typeShops = GetShopTypes(townID);
			
			array<int> types = new array<int>;			
			for(int i=0; i<typeShops.Count(); i++)
			{
				types.Insert(typeShops.GetKey(i));
			}			
			
			//buy stuff
			for(int i=0; i<numToBuy; i++)
			{
				//pick a random shop type
				int typeIndex = s_AIRandomGenerator.RandInt(0,types.Count()-1);
				//pick a random shop within that type
				int shopIndex = s_AIRandomGenerator.RandInt(0,typeShops[types[typeIndex]].Count()-1);
				OVT_ShopComponent shop = GetShopByRplId(typeShops[types[typeIndex]][shopIndex]);
				if(!shop) continue;
				//pick a random inventory item
				int itemIndex = s_AIRandomGenerator.RandInt(0,shop.m_aInventory.Count()-1);
				int id = shop.m_aInventory.GetKey(itemIndex);
				int qty = s_AIRandomGenerator.RandInt(1,GetDemand(id));
				int stock = shop.GetStock(id);
				if(stock < qty) qty = stock;
				if(qty > 0)
				{
					shop.HandleNPCSale(id, qty);
				}
			}			
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Groups shops within a town by their OVT_ShopType.
	//! \param[in] townId The ID of the town.
	//! \return A map where keys are shop types (int) and values are arrays of RplIds for shops of that type.
	protected map<int, ref array<RplId>> GetShopTypes(int townId)
	{
		map<int, ref array<RplId>> typeShops = new map<int, ref array<RplId>>;
		for(int i=0; i<m_mTownShops[townId].Count(); i++)
		{				
			RplId shopId = m_mTownShops[townId][i];
			OVT_ShopComponent shop = GetShopByRplId(shopId);
			if(!shop || shop.m_ShopType == -1) continue;
			if(!typeShops.Contains(shop.m_ShopType))
			{
				typeShops[shop.m_ShopType] = new array<RplId>;
			}
			typeShops[shop.m_ShopType].Insert(shopId);
		}
		return typeShops;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Retrieves the OVT_ShopComponent associated with a given RplId.
	//! \param[in] shopId The RplId of the shop entity.
	//! \return The OVT_ShopComponent instance, or null if the entity or component is not found.
	OVT_ShopComponent GetShopByRplId(RplId shopId)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(shopId));
		if(!rpl) return null;
		IEntity entity = rpl.GetEntity();
		return OVT_ShopComponent.Cast(entity.FindComponent(OVT_ShopComponent));
	}
	
	//------------------------------------------------------------------------------------------------
	//! Calculates income from donations and taxes, applies the resistance tax, and distributes the remaining amount to online players.
	//! Typically called periodically (e.g., every 6 hours).
	protected void CalculateIncome()
	{
		//Support donations
		int income = GetDonationIncome();
		income += GetTaxIncome();
		
		if(income == 0) return;
		
		int taxed = Math.Round(income * m_fResistanceTax);
		income -= taxed;
		AddResistanceMoney(taxed);
		
		PlayerManager mgr = GetGame().GetPlayerManager();
		int count = mgr.GetPlayerCount();
		if(count == 0)
		{
			AddResistanceMoney(income);
			return;
		}
		//Distribute remaining to all players online
		int incomePerPlayer = Math.Round(income / count);
		
		autoptr array<int> players = new array<int>;
		mgr.GetPlayers(players);
		foreach(int playerId : players)
		{			
			AddPlayerMoney(playerId, incomePerPlayer);			
		}
		OVT_Global.GetNotify().SendTextNotification("TaxesDonationsPlayer",-1,incomePerPlayer.ToString());
	}
	
	//------------------------------------------------------------------------------------------------
	//! Calculates total income generated from town support donations.
	//! Higher support and stability increase donations.
	//! \return The total donation income.
	int GetDonationIncome()
	{
		int income = 0;
		foreach(OVT_TownData town : OVT_Global.GetTowns().GetTowns())
		{
			int increase = OVT_Global.GetConfig().m_Difficulty.donationIncome * town.support;
			if(town.stability > 75)
			{
				increase *= 2;
			}			
			income += increase;
		}
		return income;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Calculates total income generated from taxes based on town population and stability (excluding occupying faction towns).
	//! \return The total tax income.
	int GetTaxIncome()
	{
		int income = 0;
		
		foreach(OVT_TownData town : OVT_Global.GetTowns().GetTowns())
		{
			if(town.IsOccupyingFaction()) continue;
			income += (int)Math.Round(OVT_Global.GetConfig().m_Difficulty.taxIncome * town.population * (town.stability / 100));
		}
		
		return income;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Sets the base price for an item. Used during initialization.
	//! \param[in] id The resource ID of the item.
	//! \param[in] cost The new base cost.
	void SetPrice(int id, int cost)
	{
		m_mItemCosts[id] = cost;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Sets the demand value for an item. Used during initialization.
	//! \param[in] id The resource ID of the item.
	//! \param[in] demand The new demand value.
	void SetDemand(int id, int demand)
	{
		m_mItemDemand[id] = demand;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Calculates the dynamic selling price of an item at a specific location.
	//! Price is adjusted based on local town stock levels and distance to the nearest port.
	//! For vehicles, returns the base price.
	//! \param[in] id The resource ID of the item.
	//! \param[in] pos The world position where the item is being sold (optional, uses base price if "0 0 0").
	//! \return The calculated sell price.
	int GetSellPrice(int id, vector pos = "0 0 0")
	{		
		int price = GetPrice(id);
		if(m_aAllVehicles.Contains(id)) return price;
		if(pos[0] != 0)
		{
			OVT_TownData town = OVT_Global.GetTowns().GetNearestTown(pos);
			if(town)
			{
				int townID = OVT_Global.GetTowns().GetTownID(town);
				int stock_level = GetTownStock(townID, id);
				int max_stock = GetTownMaxStock(townID, id);
				float distance_to_port = DistanceToNearestPort(pos);
				price = Math.Round(price + ((1 - (stock_level / max_stock)) * (price * 0.1)) + (price * distance_to_port * 0.0001));
				if(price < 0) price = 1;
			}
		}
		
		return price;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Calculates the dynamic buying price of an item at a specific location for a specific player.
	//! Based on the sell price plus a profit margin, potentially modified by player-specific multipliers.
	//! \param[in] id The resource ID of the item.
	//! \param[in] pos The world position where the item is being bought (optional).
	//! \param[in] playerId The ID of the buying player (optional, influences price multipliers).
	//! \return The calculated buy price.
	int GetBuyPrice(int id, vector pos = "0 0 0", int playerId=-1)
	{
		int price = GetSellPrice(id, pos);
		int buy = Math.Round(price + (price * OVT_Global.GetConfig().m_fShopProfitMargin));
		if(playerId > -1)
		{
			OVT_PlayerData player = OVT_PlayerData.Get(playerId);
			if(player)
				buy = Math.Round(buy * player.priceMultiplier);
		}
		if(buy == price) buy += 1;
		return buy;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the total stock level of a specific item across all shops in a given town.
	//! \param[in] townId The ID of the town.
	//! \param[in] id The resource ID of the item.
	//! \return The total stock count, or 0 if the town has no shops.
	int GetTownStock(int townId, int id)
	{
		if(!m_mTownShops.Contains(townId)) return 0;
		int stock = 0;
		foreach(RplId shopId : m_mTownShops[townId])
		{
			OVT_ShopComponent shop = GetShopByRplId(shopId);
			stock += shop.GetStock(id);
		}
		return stock;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Calculates the theoretical maximum stock level for an item in a town.
	//! Based on population, buy rate, item demand, and town stability.
	//! \param[in] townId The ID of the town.
	//! \param[in] id The resource ID of the item.
	//! \return The calculated maximum stock capacity.
	int GetTownMaxStock(int townId, int id)
	{
		OVT_TownData town = OVT_Global.GetTowns().GetTown(townId);
		if(!town) return 100;
		return Math.Round(1 + (town.population * OVT_Global.GetConfig().m_fNPCBuyRate * GetDemand(id) * ((float)town.stability / 100)));
	}	
	
	//------------------------------------------------------------------------------------------------
	//! Gets the dynamic sell price of an item by its ResourceName. Convenience wrapper for GetSellPrice.
	//! \param[in] res The ResourceName of the item.
	//! \param[in] pos The world position (optional).
	//! \return The calculated sell price.
	int GetPriceByResource(ResourceName res, vector pos = "0 0 0")
	{
		int id = GetInventoryId(res);
		return GetSellPrice(id, pos);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Checks if a specific item resource is typically sold at a given shop type based on its configuration.
	//! \param[in] res The ResourceName of the item.
	//! \param[in] shopType The type of shop to check against.
	//! \return True if the item is found in the shop type's inventory configuration, false otherwise.
	bool IsSoldAtShop(ResourceName res, OVT_ShopType shopType)
	{
		OVT_ShopInventoryConfig config = GetShopConfig(shopType);
		foreach(OVT_ShopInventoryItem item : config.m_aInventoryItems)
		{
			array<SCR_EntityCatalogEntry> entries();
			FindInventoryItems(item.m_eItemType, item.m_eItemMode, item.m_sFind, entries);
			
			foreach(SCR_EntityCatalogEntry entry : entries)
			{
				if(res == entry.GetPrefab()) return true;
			}
		}
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Finds the RplId of the nearest port entity to a given position.
	//! \param[in] pos The world position to check from.
	//! \return The RplId of the nearest port, or an invalid RplId if no ports are registered.
	RplId GetNearestPort(vector pos)
	{
		RplId nearestPort;
		float nearest = -1;
		foreach(RplId id : m_aAllPorts)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			float distance = vector.Distance(pos, rpl.GetEntity().GetOrigin());
			if(nearest == -1 || distance < nearest){
				nearest = distance;
				nearestPort = id;
			}
		}
		return nearestPort;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Calculates the distance to the nearest registered port from a given position.
	//! \param[in] pos The world position to check from.
	//! \return The distance in meters, or -1 if no ports are registered.
	float DistanceToNearestPort(vector pos)
	{
		float nearest = -1;
		foreach(RplId id : m_aAllPorts)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			float distance = vector.Distance(pos, rpl.GetEntity().GetOrigin());
			if(nearest == -1 || distance < nearest){
				nearest = distance;
			}
		}
		return nearest;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets a list of RplIds for all registered shops.
	//! \return An array containing the RplIds of all shops.
	array<RplId> GetAllShops()
	{
		return m_aAllShops;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets a list of RplIds for all registered ports.
	//! \return An array containing the RplIds of all ports.
	array<RplId> GetAllPorts()
	{
		return m_aAllPorts;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets a list of RplIds for all registered gun dealers.
	//! \return An array containing the RplIds of all gun dealers.
	array<RplId> GetGunDealers()
	{
		return m_aGunDealers;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Checks if an item (by resource ID) belongs to a specific faction.
	//! \param[in] id The resource ID of the item.
	//! \param[in] factionId The index of the faction.
	//! \return True if the item belongs to the faction, false otherwise.
	bool ItemIsFromFaction(int id, int factionId)
	{
		if(!m_mFactionResources.Contains(factionId)) return false;
		return m_mFactionResources[factionId].Contains(id);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets a list of RplIds for all shops located within the radius of a given town.
	//! \param[in] town The OVT_TownData object representing the town.
	//! \return An array of RplIds for shops within the town's range.
	array<RplId> GetAllShopsInTown(OVT_TownData town)
	{
		int range = OVT_Global.GetTowns().m_iCityRange;
		if(town.size == 2) range = OVT_Global.GetTowns().m_iTownRange;
		if(town.size == 1) range = OVT_Global.GetTowns().m_iVillageRange;
		array<RplId> shops = new array<RplId>;
		foreach(RplId id : m_aAllShops)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			float dist = vector.Distance(town.location, rpl.GetEntity().GetOrigin());
			if(dist <= range) shops.Insert(id);
		}
		return shops;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Retrieves the inventory configuration for a specific shop type.
	//! \param[in] shopType The OVT_ShopType enum value.
	//! \return The corresponding OVT_ShopInventoryConfig, or a new empty config if not found.
	OVT_ShopInventoryConfig GetShopConfig(OVT_ShopType shopType)
	{		
		foreach(OVT_ShopInventoryConfig config : m_ShopConfig.m_aShopConfigs)
		{
			if(config.type == shopType) return config;
		}
		return new OVT_ShopInventoryConfig();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the current amount of money for a player identified by their persistent ID.
	//! \param[in] playerId The persistent string ID of the player.
	//! \return The player's money amount, or 0 if the player data is not found.
	int GetPlayerMoney(string playerId)
	{
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(playerId);
		if(!player) return 0;
		return player.money;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the current amount of money for the local player.
	//! \return The local player's money amount.
	int GetLocalPlayerMoney()
	{
		string playerId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(SCR_PlayerController.GetLocalPlayerId());
		
		return GetPlayerMoney(playerId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Checks if the local player has a sufficient amount of money.
	//! \param[in] amount The amount to check against.
	//! \return True if the local player has enough money, false otherwise.
	int LocalPlayerHasMoney(int amount)
	{
		string playerId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(SCR_PlayerController.GetLocalPlayerId());		
		return PlayerHasMoney(playerId, amount);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Checks if a specific player (by persistent ID) has a sufficient amount of money.
	//! \param[in] playerId The persistent string ID of the player.
	//! \param[in] amount The amount to check against.
	//! \return True if the player has enough money, false otherwise.
	bool PlayerHasMoney(string playerId, int amount)
	{
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(playerId);
		if(!player) return false;
		return player.money >= amount;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Adds money to a player's account. Handles server/client distinction.
	//! Use this method for external calls.
	//! \param[in] playerId The runtime Player ID.
	//! \param[in] amount The amount of money to add.
	//! \param[in] doEvent If true, invokes the m_OnPlayerMoneyChanged event (currently unused).
	void AddPlayerMoney(int playerId, int amount, bool doEvent=false)
	{
		if(Replication.IsServer())
		{
			DoAddPlayerMoney(playerId, amount);
			return;
		}
		OVT_Global.GetServer().AddPlayerMoney(playerId, amount, doEvent);		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Server-side implementation to add money to a player's account and stream the update.
	//! \param[in] playerId The runtime Player ID.
	//! \param[in] amount The amount of money to add.
	void DoAddPlayerMoney(int playerId, int amount)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(persId);
		if(!player) return;
		
		player.money = player.money + amount;
		OVT_Global.GetEconomy().StreamPlayerMoney(playerId);
		m_OnPlayerMoneyChanged.Invoke(persId, player.money);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Adds money to the resistance faction's funds. Handles server/client distinction.
	//! \param[in] amount The amount of money to add.
	void AddResistanceMoney(int amount)
	{
		if(Replication.IsServer())
		{
			DoAddResistanceMoney(amount);
			return;
		}
		OVT_Global.GetServer().AddResistanceMoney(amount);		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Server-side implementation to add money to resistance funds and stream the update.
	//! \param[in] amount The amount of money to add.
	void DoAddResistanceMoney(int amount)
	{		
		RpcDo_SetResistanceMoney(m_iResistanceMoney + amount);
		StreamResistanceMoney();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Takes money from the resistance faction's funds. Handles server/client distinction.
	//! \param[in] amount The amount of money to take.
	void TakeResistanceMoney(int amount)
	{
		if(Replication.IsServer())
		{
			DoTakeResistanceMoney(amount);
			return;
		}
		OVT_Global.GetServer().TakeResistanceMoney(amount);		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Server-side implementation to take money from resistance funds and stream the update.
	//! \param[in] amount The amount of money to take.
	void DoTakeResistanceMoney(int amount)
	{		
		RpcDo_SetResistanceMoney(m_iResistanceMoney - amount);
		StreamResistanceMoney();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Sets the resistance tax rate. Handles server/client distinction.
	//! \param[in] amount The new tax rate (0.0 to 1.0).
	void SetResistanceTax(float amount)
	{
		if(Replication.IsServer())
		{
			DoSetResistanceTax(amount);
			return;
		}
		OVT_Global.GetServer().SetResistanceTax(amount);		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Server-side implementation to set the resistance tax rate and stream the update.
	//! \param[in] amount The new tax rate.
	void DoSetResistanceTax(float amount)
	{		
		RpcDo_SetResistanceTax(amount);
		StreamResistanceTax();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Takes money from the local player's account. Convenience wrapper for TakePlayerMoney.
	//! \param[in] amount The amount of money to take.
	void TakeLocalPlayerMoney(int amount)
	{
		int id = SCR_PlayerController.GetLocalPlayerId();
		TakePlayerMoney(id, amount);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Takes money from a player's account. Handles server/client distinction.
	//! Use this method for external calls.
	//! \param[in] playerId The runtime Player ID.
	//! \param[in] amount The amount of money to take.
	void TakePlayerMoney(int playerId, int amount)
	{
		if(Replication.IsServer())
		{
			DoTakePlayerMoney(playerId, amount);
			return;
		}
		OVT_Global.GetServer().TakePlayerMoney(playerId, amount);	
	}
	
	//------------------------------------------------------------------------------------------------
	//! Server-side method to take money directly using the persistent ID. Does not handle server/client distinction.
	//! Updates player data and invokes the money changed event. Streams update if player is online.
	//! \param[in] persId The persistent string ID of the player.
	//! \param[in] amount The amount of money to take.
	void TakePlayerMoneyPersistentId(string persId, int amount)
	{
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(persId);
		if(!player) return;		
		
		player.money = player.money - amount;
		if(player.money < 0) player.money = 0;
		
		int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(persId);
		if(playerId > -1){
			StreamPlayerMoney(playerId);	
		}
		m_OnPlayerMoneyChanged.Invoke(persId, player.money);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Server-side method to add money directly using the persistent ID. Does not handle server/client distinction.
	//! Updates player data and invokes the money changed event. Streams update if player is online.
	//! \param[in] persId The persistent string ID of the player.
	//! \param[in] amount The amount of money to add.
	void AddPlayerMoneyPersistentId(string persId, int amount)
	{
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(persId);
		if(!player) return;
		
		player.money = player.money + amount;
		
		int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(persId);
		if(playerId > -1){
			StreamPlayerMoney(playerId);	
		}
		m_OnPlayerMoneyChanged.Invoke(persId, player.money);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Server-side implementation to take money from a player's account and stream the update.
	//! \param[in] playerId The runtime Player ID.
	//! \param[in] amount The amount of money to take.
	void DoTakePlayerMoney(int playerId, int amount)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(persId);
		if(!player) return;
		
		player.money = player.money - amount;
		if(player.money < 0) player.money = 0;
		StreamPlayerMoney(playerId);	
		m_OnPlayerMoneyChanged.Invoke(persId, player.money);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Checks if the resistance faction has a sufficient amount of money.
	//! \param[in] amount The amount to check against.
	//! \return True if the resistance has enough money, false otherwise.
	bool ResistanceHasMoney(int amount)
	{
		return m_iResistanceMoney >= amount;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the current amount of money held by the resistance faction.
	//! \return The resistance money amount.
	int GetResistanceMoney()
	{
		return m_iResistanceMoney;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Registers a gun dealer entity with the economy manager.
	//! Adds the entity's RplId to the list of gun dealers.
	//! \param[in] id The EntityID of the gun dealer entity.
	void RegisterGunDealer(EntityID id)
	{
		IEntity entity = GetGame().GetWorld().FindEntityByID(id);
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if(rpl)
			m_aGunDealers.Insert(rpl.Id());
	}
	
	//------------------------------------------------------------------------------------------------
	//! Initialization logic called shortly after the component is created.
	//! Sets up timers and calls AfterInit.
	//! \param[in] owner The owner entity of this component.
	void Init(IEntity owner)
	{			
		float timeMul = 6;
		OVT_TimeAndWeatherHandlerComponent tw = OVT_TimeAndWeatherHandlerComponent.Cast(GetGame().GetGameMode().FindComponent(OVT_TimeAndWeatherHandlerComponent));
		
		if(tw) timeMul = tw.GetDayTimeMultiplier();
		
		OVT_Global.GetTowns() = OVT_Global.GetTowns();
		OVT_Global.GetPlayers() = OVT_Global.GetPlayers();
		
		GetGame().GetCallqueue().CallLater(AfterInit, 0);		
		
		if(!Replication.IsServer()) return;
		GetGame().GetCallqueue().CallLater(CheckUpdate, ECONOMY_UPDATE_FREQUENCY / timeMul, true, GetOwner());
		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Post-initialization logic called after Init.
	//! Builds the resource database and initializes ports and shops.
	protected void AfterInit()
	{
		BuildResourceDatabase();
		InitializePorts();
		
		if(!Replication.IsServer()) return;		
		InitializeShops();		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Registers a resource (item or vehicle prefab) in the internal database if not already present.
	//! \param[in] res The ResourceName to register.
	void RegisterResource(ResourceName res)
	{
		if(!m_aResources.Contains(res))
		{
			m_aResources.Insert(res);
			int id = m_aResources.Count()-1;
			m_aResourceIndex[res] = id;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Builds the complete database of resources (items, vehicles) from configurations and faction data.
	//! Assigns resource IDs, sets prices, determines legality, parking types, and faction ownership.
	void BuildResourceDatabase()
	{	
		//Process non EntityCatalog vehicles
		foreach(OVT_VehiclePriceConfig cfg : m_VehiclePriceConfig.m_aPrices)
		{
			if(cfg.m_sFind == "" && cfg.prefab != "")
			{
				ResourceName res = cfg.prefab;
				if(!m_aResources.Contains(res))
				{
					m_aResources.Insert(res);
					int id = m_aResources.Count()-1;
					m_aResourceIndex[res] = id;
					m_aAllVehicles.Insert(id);
					SetPrice(id, cfg.cost);
					m_mVehicleParking[id] = cfg.parking;
					if(!cfg.illegal) m_aLegalVehicles.Insert(id);
				}
			}
		}
		
		FactionManager factionMgr = GetGame().GetFactionManager();
		array<Faction> factions = new array<Faction>;
		factionMgr.GetFactionsList(factions);
		foreach(Faction faction : factions)
		{
			OVT_Faction fac = OVT_Global.GetFactions().GetOverthrowFactionByKey(faction.GetFactionKey());
			int factionId = factionMgr.GetFactionIndex(faction);
			m_mFactionResources[factionId] = new array<int>;
			array<SCR_EntityCatalogEntry> items = new array<SCR_EntityCatalogEntry>;
			fac.GetAllInventoryItems(items);
			foreach(SCR_EntityCatalogEntry item : items) 
			{
				ResourceName res = item.GetPrefab();
				if(res == "") continue;
				if(!m_aResources.Contains(res))
				{
					m_aResources.Insert(res);
					int id = m_aResources.Count()-1;
					m_aResourceIndex[res] = id;
					m_aEntityCatalogEntries.Insert(item);
					m_mFactionResources[factionId].Insert(id);
				}
			}
			
			array<SCR_EntityCatalogEntry> vehicles = new array<SCR_EntityCatalogEntry>;
			
			fac.GetAllVehicles(vehicles);
			foreach(SCR_EntityCatalogEntry item : vehicles) 
			{
				ResourceName res = item.GetPrefab();
				if(res == "") continue;
				if(res.IndexOf("Campaign") > -1) continue;
				if(!m_aResources.Contains(res))
				{
					bool illegal = false;
					bool hidden = false;
					int cost = 500000;
					OVT_ParkingType parkingType = OVT_ParkingType.PARKING_CAR;
					//Set it's price
					foreach(OVT_VehiclePriceConfig cfg : m_VehiclePriceConfig.m_aPrices)
					{
						if(cfg.prefab != "") continue;
						if(cfg.m_sFind == "" || res.IndexOf(cfg.m_sFind) > -1)
						{							
							if(cfg.hidden) {
								hidden = true;
								break;
							}
							cost = cfg.cost;
							illegal = cfg.illegal;
							parkingType = cfg.parking;
						}
					}
					
					if(hidden) continue;
					
					if(fac.GetFactionKey() == "CIV") {
						illegal = false;
					}
										
					m_aResources.Insert(res);
					int id = m_aResources.Count()-1;
					m_aResourceIndex[res] = id;
					m_aEntityCatalogEntries.Insert(item);
					m_mFactionResources[factionId].Insert(id);
					m_aAllVehicles.Insert(id);					
					
					m_mVehicleParking[id] = parkingType;
					SetPrice(id, cost);
					if(!illegal) m_aLegalVehicles.Insert(id);
				}
			}
		}		
		
		foreach(OVT_PrefabItemCostConfig item : m_GunDealerConfig.m_aGunDealerItemPrefabs)
		{
			ResourceName res = item.m_sEntityPrefab;
			if(res == "") continue;
			if(!m_aResources.Contains(res))
			{
				m_aResources.Insert(res);
				int id = m_aResources.Count()-1;
				m_aResourceIndex[res] = id;
				SetPrice(id, item.cost);
			}			
		}
		
		//Set Prices
		foreach(OVT_PriceConfig config : m_PriceConfig.m_aPrices)
		{
			array<SCR_EntityCatalogEntry> items = new array<SCR_EntityCatalogEntry>;
			FindInventoryItems(config.m_eItemType, config.m_eItemMode, config.m_sFind, items);
			foreach(SCR_EntityCatalogEntry entry : items)
			{
				int id = GetInventoryId(entry.GetPrefab());
				SetPrice(id, config.cost);
				SetDemand(id, config.demand);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Finds inventory items matching specific criteria within the cached entity catalog entries.
	//! \param[in] type The SCR_EArsenalItemType to filter by.
	//! \param[in] mode The SCR_EArsenalItemMode to filter by (DEFAULT matches any).
	//! \param[in] search A string to search within the prefab name (case-sensitive). Blank matches all.
	//! \param[out] inventoryItems An array to populate with matching SCR_EntityCatalogEntry objects.
	//! \return True if the search was performed (doesn't guarantee items were found).
	bool FindInventoryItems(SCR_EArsenalItemType type, SCR_EArsenalItemMode mode, string search, out array<SCR_EntityCatalogEntry> inventoryItems)
	{	
		foreach(SCR_EntityCatalogEntry entry : m_aEntityCatalogEntries)
		{
			SCR_ArsenalItem item = SCR_ArsenalItem.Cast(entry.GetEntityDataOfType(SCR_ArsenalItem));
			if(item)
			{
				if(item.GetItemType() != type) continue;
				if(search != ""){
					ResourceName prefab = entry.GetPrefab();
					if(prefab.IndexOf(search) == -1) continue;
				}
				if(mode != SCR_EArsenalItemMode.DEFAULT)
				{
					if(item.GetItemMode() != mode) continue;
				}
				inventoryItems.Insert(entry);				
			}
		}
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Retrieves all non-clothing items belonging to the occupying faction.
	//! \param[out] resources An array to populate with the ResourceNames of the matching items.
	//! \return True.
	bool GetAllNonClothingOccupyingFactionItems(out array<ResourceName> resources)
	{
		array<SCR_EArsenalItemType> ignore = {
			SCR_EArsenalItemType.LEGS,
			SCR_EArsenalItemType.TORSO,
			SCR_EArsenalItemType.FOOTWEAR,
			SCR_EArsenalItemType.HEADWEAR,
			SCR_EArsenalItemType.BACKPACK,
			SCR_EArsenalItemType.VEST_AND_WAIST
		};
		
		foreach(SCR_EntityCatalogEntry entry : m_aEntityCatalogEntries)
		{
			ResourceName res = entry.GetPrefab();
			int id = GetInventoryId(res);
			if(!ItemIsFromFaction(id, OVT_Global.GetConfig().GetOccupyingFactionIndex())) continue;
			
			SCR_ArsenalItem item = SCR_ArsenalItem.Cast(entry.GetEntityDataOfType(SCR_ArsenalItem));
			if(item)
			{
				if(ignore.Contains(item.GetItemType())) continue;				
				resources.Insert(entry.GetPrefab());				
			}
		}
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Checks if a resource ID corresponds to a vehicle.
	//! \param[in] id The resource ID.
	//! \return True if the ID is in the list of all vehicles, false otherwise.
	bool IsVehicle(int id)
	{
		return m_aAllVehicles.Contains(id);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Checks if a ResourceName corresponds to a vehicle.
	//! \param[in] res The ResourceName.
	//! \return True if the resource is a vehicle, false otherwise.
	bool IsVehicle(ResourceName res)
	{
		int id = GetInventoryId(res);
		return IsVehicle(id);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Retrieves all arsenal items (non-vehicles) that do not belong to the occupying faction.
	//! \param[out] resources An array to populate with the ResourceNames of the matching items.
	//! \return True.
	bool GetAllNonOccupyingFactionItems(out array<ResourceName> resources)
	{
		int occupyingFactionIndex = OVT_Global.GetConfig().GetOccupyingFactionIndex();
		foreach(SCR_EntityCatalogEntry entry : m_aEntityCatalogEntries)
		{
			SCR_ArsenalItem item = SCR_ArsenalItem.Cast(entry.GetEntityDataOfType(SCR_ArsenalItem));
			if(item)
			{
				ResourceName prefab = entry.GetPrefab();
				int id = GetInventoryId(prefab);
				if(ItemIsFromFaction(id, occupyingFactionIndex)) continue;
				resources.Insert(prefab);				
			}
		}
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Retrieves all vehicles that do not belong to the occupying faction.
	//! \param[out] resources An array to populate with the ResourceNames of the matching vehicles.
	//! \param[in] includeIllegal If true, includes vehicles marked as illegal. Defaults to false.
	//! \return True.
	bool GetAllNonOccupyingFactionVehicles(out array<ResourceName> resources, bool includeIllegal = false)
	{
		int occupyingFactionIndex = OVT_Global.GetConfig().GetOccupyingFactionIndex();
		
		foreach(int id : m_aAllVehicles)
		{
			ResourceName prefab = GetResource(id);
			if(ItemIsFromFaction(id, occupyingFactionIndex)) continue;
			if(!includeIllegal && !m_aLegalVehicles.Contains(id)) continue;
			resources.Insert(prefab);			
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Retrieves all non-occupying faction vehicles that match specific parking types.
	//! \param[out] resources An array to populate with the ResourceNames of the matching vehicles.
	//! \param[in] parkingTypes An array of OVT_ParkingType enums to filter by.
	//! \param[in] includeIllegal If true, includes vehicles marked as illegal. Defaults to false.
	//! \return True.
	bool GetAllNonOccupyingFactionVehiclesByParking(out array<ResourceName> resources, array<OVT_ParkingType> parkingTypes, bool includeIllegal = false)
	{
		int occupyingFactionIndex = OVT_Global.GetConfig().GetOccupyingFactionIndex();
		
		foreach(int id : m_aAllVehicles)
		{
			ResourceName prefab = GetResource(id);
			if(ItemIsFromFaction(id, occupyingFactionIndex)) continue;
			if(!includeIllegal && !m_aLegalVehicles.Contains(id)) continue;
			if(!parkingTypes.Contains(GetParkingType(id))) continue;
			resources.Insert(prefab);			
		}
		
		return true;
	}
			
	//------------------------------------------------------------------------------------------------
	//! Called after the game has fully started. Triggers shop inventory initialization.
	void PostGameStart()
	{		
		GetGame().GetCallqueue().CallLater(InitShopInventory, 0);				
	}
	
	//------------------------------------------------------------------------------------------------
	//! Finds and registers all port entities in the world.
	protected void InitializePorts()
	{
		#ifdef OVERTHROW_DEBUG
		Print("Finding ports");
		#endif
		
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, CheckPortInit, FilterPortEntities);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Finds and registers all shop entities in the world (Server only).
	protected void InitializeShops()
	{
		#ifdef OVERTHROW_DEBUG
		Print("Finding shops");
		#endif
		
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, CheckShopInit, FilterShopEntities, EQueryEntitiesFlags.STATIC);
	}
		
	//------------------------------------------------------------------------------------------------
	//! Gets the internal integer ID for a given resource name.
	//! Requires the resource to have been registered via BuildResourceDatabase or RegisterResource.
	//! \param[in] res The ResourceName.
	//! \return The integer ID, or potentially an error/invalid ID if not found.
	int GetInventoryId(ResourceName res)
	{		
		return m_aResourceIndex[res];
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the ResourceName corresponding to an internal integer ID.
	//! \param[in] id The integer ID.
	//! \return The ResourceName, or potentially null/empty if the ID is invalid.
	ResourceName GetResource(int id)
	{
		return m_aResources[id];
	}
	
	//------------------------------------------------------------------------------------------------
	//! Initializes the inventory of all registered shops based on their type configuration and town context.
	//! Populates shops with appropriate items and initial stock levels. (Server only).
	protected void InitShopInventory()
	{
		foreach(RplId shopId : m_aAllShops)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(shopId));
			if(!rpl) continue;
			IEntity entity = rpl.GetEntity();
			OVT_ShopComponent shop = OVT_ShopComponent.Cast(entity.FindComponent(OVT_ShopComponent));
			OVT_TownData town = shop.GetTown();
			
			int occupyingFactionId = OVT_Global.GetConfig().GetOccupyingFactionIndex();
			int supportingFactionId = OVT_Global.GetConfig().GetSupportingFactionIndex();
			
			int townID = OVT_Global.GetTowns().GetTownID(town);			
			
			if(shop.m_ShopType == OVT_ShopType.SHOP_VEHICLE)
			{
				array<ResourceName> vehicles();
				GetAllNonOccupyingFactionVehicles(vehicles);
				foreach(ResourceName res : vehicles)
				{
					int id = GetInventoryId(res);
					shop.AddToInventory(id, 100);
				}
			}else{		
				OVT_ShopInventoryConfig config = GetShopConfig(shop.m_ShopType);
				foreach(OVT_ShopInventoryItem item : config.m_aInventoryItems)
				{
					array<SCR_EntityCatalogEntry> entries();
					FindInventoryItems(item.m_eItemType, item.m_eItemMode, item.m_sFind, entries);
					
					foreach(SCR_EntityCatalogEntry entry : entries)
					{
						int id = GetInventoryId(entry.GetPrefab());
						
						if(!item.m_bIncludeOccupyingFactionItems && ItemIsFromFaction(id, occupyingFactionId)) continue;
						if(!item.m_bIncludeSupportingFactionItems && ItemIsFromFaction(id, supportingFactionId)) continue;
						if(!item.m_bIncludeOtherFactionItems) continue;
						int max = GetTownMaxStock(townID, id);
					
						int num = Math.Round(s_AIRandomGenerator.RandFloatXY(1,max));
						
						shop.AddToInventory(id, num);
					}
				}
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Callback function used by QueryEntitiesBySphere during shop initialization.
	//! Registers the shop's RplId, associates it with the nearest town, and sets the town ID on the shop component.
	//! \param[in] entity The potential shop entity found by the query.
	//! \return True to continue the query.
	protected bool CheckShopInit(IEntity entity)
	{		
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if(rpl)
		{
			RplId id = rpl.Id();
			m_aAllShops.Insert(id);
		
			OVT_TownData town = OVT_Global.GetTowns().GetNearestTown(entity.GetOrigin());
			int townID = OVT_Global.GetTowns().GetTownID(town);
			
			if(!m_mTownShops.Contains(townID))
			{				
				m_mTownShops[townID] = {};	
			}
			
			m_mTownShops[townID].Insert(id);
			
			OVT_ShopComponent shop = GetShopByRplId(id);
			shop.m_iTownId = townID;
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Filter function used by QueryEntitiesBySphere during shop initialization.
	//! Ensures the entity is a building with an OVT_ShopComponent that is not a procurement shop.
	//! \param[in] entity The entity to filter.
	//! \return True if the entity is a valid shop building, false otherwise.
	protected bool FilterShopEntities(IEntity entity)
	{	
		if(entity.ClassName() == "SCR_DestructibleBuildingEntity")
		{
			OVT_ShopComponent shop = OVT_ShopComponent.Cast(entity.FindComponent(OVT_ShopComponent));
			if(shop) return !shop.m_bProcurement;
		}
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Callback function used by QueryEntitiesBySphere during port initialization.
	//! Registers the port's RplId.
	//! \param[in] entity The potential port entity found by the query.
	//! \return True to continue the query.
	protected bool CheckPortInit(IEntity entity)
	{		
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if(rpl)
		{
			RplId id = rpl.Id();
			m_aAllPorts.Insert(id);
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Filter function used by QueryEntitiesBySphere during port initialization.
	//! Ensures the entity has an OVT_PortControllerComponent.
	//! \param[in] entity The entity to filter.
	//! \return True if the entity has the port controller component, false otherwise.
	protected bool FilterPortEntities(IEntity entity)
	{	

		OVT_PortControllerComponent port = OVT_PortControllerComponent.Cast(entity.FindComponent(OVT_PortControllerComponent));
		if(port) return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Charges a player a fee upon respawning, if they have sufficient funds.
	//! \param[in] playerId The runtime Player ID of the respawning player.
	void ChargeRespawn(int playerId)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		int money = GetPlayerMoney(persId);
		if (money > 500) {
			int cost = OVT_Global.GetConfig().m_Difficulty.respawnCost;
			TakePlayerMoney(playerId, cost);
		}
	}
	
	//RPC Methods
	
	
	//------------------------------------------------------------------------------------------------
	//! Saves component state for persistence or JIP.
	override bool RplSave(ScriptBitWriter writer)
	{		
		writer.WriteInt(m_iResistanceMoney);
		writer.WriteFloat(m_fResistanceTax);
		
		//Send JIP Shops
		writer.WriteInt(m_aAllShops.Count()); 
		for(int i=0; i<m_aAllShops.Count(); i++)
		{
			writer.WriteRplId(m_aAllShops[i]);
		}
		writer.WriteInt(m_mTownShops.Count()); 
		for(int i=0; i<m_mTownShops.Count(); i++)
		{			
			int key = m_mTownShops.GetKey(i);
			writer.WriteInt(key);
			writer.WriteInt(m_mTownShops[key].Count());
			for(int t=0; t<m_mTownShops[key].Count(); t++)
			{	
				writer.WriteRplId(m_mTownShops[key][t]);
			}			
		}
		
		//Send JIP Gun Dealers
		writer.WriteInt(m_aGunDealers.Count()); 
		for(int i=0; i<m_aGunDealers.Count(); i++)
		{
			writer.WriteRplId(m_aGunDealers[i]);
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Loads component state for persistence or JIP.
	override bool RplLoad(ScriptBitReader reader)
	{	
		int length, keylength, price;
		string playerId;
		RplId id;
		int key;
		
		if (!reader.ReadInt(m_iResistanceMoney)) return false;
		if (!reader.ReadFloat(m_fResistanceTax)) return false;
		
		//Recieve JIP shops		
		if (!reader.ReadInt(length)) return false;
		for(int i=0; i<length; i++)
		{			
			if (!reader.ReadRplId(id)) return false;
			m_aAllShops.Insert(id);
		}
		
		if (!reader.ReadInt(length)) return false;
		for(int i=0; i<length; i++)
		{	
			if (!reader.ReadInt(key)) return false;
			m_mTownShops[key] = new array<RplId>;
			if (!reader.ReadInt(keylength)) return false;
			for(int t=0; t<keylength; t++)
			{
				if (!reader.ReadRplId(id)) return false;
				m_mTownShops[key].Insert(id);
			}
		}
		
		//Recieve JIP gun dealers		
		if (!reader.ReadInt(length)) return false;
		for(int i=0; i<length; i++)
		{			
			if (!reader.ReadRplId(id)) return false;
			m_aGunDealers.Insert(id);
		}
		
		return true;
	}
	
	
	//------------------------------------------------------------------------------------------------
	//! Streams the latest money amount for a specific player to all clients via RPC.
	//! \param[in] playerId The runtime Player ID.
	void StreamPlayerMoney(int playerId)
	{		
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(persId);
		if(!player) return;
		
		Rpc(RpcDo_SetPlayerMoney, playerId, player.money);
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC called on clients to update a player's money locally and invoke the change event.
	//! \param[in] playerId The runtime Player ID.
	//! \param[in] amount The new money amount.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetPlayerMoney(int playerId, int amount)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(persId);
		if(!player) return;
		player.money = amount;
		m_OnPlayerMoneyChanged.Invoke(persId, player.money);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Streams the latest resistance money amount to all clients via RPC.
	protected void StreamResistanceMoney()
	{
		Rpc(RpcDo_SetResistanceMoney, m_iResistanceMoney);
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC called on clients to update the resistance money locally and invoke the change event.
	//! \param[in] amount The new resistance money amount.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetResistanceMoney(int amount)
	{
		m_iResistanceMoney = amount;
		m_OnResistanceMoneyChanged.Invoke(m_iResistanceMoney);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Streams the latest resistance tax rate to all clients via RPC.
	protected void StreamResistanceTax()
	{
		Rpc(RpcDo_SetResistanceTax, m_fResistanceTax);
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC called on clients to update the resistance tax rate locally.
	//! \param[in] amount The new resistance tax rate.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetResistanceTax(float amount)
	{
		m_fResistanceTax = amount;
	}
		
}