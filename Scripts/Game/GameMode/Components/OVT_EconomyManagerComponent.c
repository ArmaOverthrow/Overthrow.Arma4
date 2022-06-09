class OVT_EconomyManagerComponentClass: OVT_ComponentClass
{
};

class OVT_ShopInventoryItem : ScriptAndConfig
{
	[Attribute("Item Prefab", UIWidgets.ResourceNamePicker)]
	ResourceName prefab;
	
	[Attribute("50")]
	int cost;
	
	[Attribute("10")]
	int maxAtStart;
	
	[Attribute(defvalue: "0.1", desc: "Demand Per Population Per Day")]
	float demand;
}

class OVT_ShopInventoryConfig : ScriptAndConfig
{
	[Attribute("1", UIWidgets.ComboBox, "Shop type", "", ParamEnumArray.FromEnum(OVT_ShopType) )]
	OVT_ShopType type;
	
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_ShopInventoryItem> m_aInventoryItems;
	ref array<ref OVT_ShopInventoryItem> m_aInventoryItemsPacked = new array<ref OVT_ShopInventoryItem>();

}

class OVT_EconomyManagerComponent: OVT_Component
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_ShopInventoryConfig> m_aShopConfigs;
	ref array<ref OVT_ShopInventoryConfig> m_aShopConfigsPacked = new array<ref OVT_ShopInventoryConfig>();

	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_ShopInventoryItem> m_aGunDealerItems;
	ref array<ref OVT_ShopInventoryItem> m_aGunDealerItemsPacked = new array<ref OVT_ShopInventoryItem>();
	
	protected ref map<ResourceName, int> m_mItemCosts;
	protected ref map<int, int> m_mMoney;
	protected int m_iResistanceMoney = 0;
	
	protected ref array<EntityID> m_aAllShops;
	
	static OVT_EconomyManagerComponent s_Instance;
	
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
	
	void SetPrice(ResourceName res, int cost)
	{
		m_mItemCosts[res] = cost;
	}
	
	int GetPrice(ResourceName res)
	{
		if(!m_mItemCosts.Contains(res)) return 0;
		
		return m_mItemCosts[res];
	}
	
	OVT_ShopInventoryConfig GetShopConfig(OVT_ShopType shopType)
	{		
		foreach(OVT_ShopInventoryConfig config : m_aShopConfigs)
		{
			if(config.type == shopType) return config;
		}
		return new OVT_ShopInventoryConfig();
	}
	
	int GetPlayerMoney(int playerId)
	{
		if(!m_mMoney.Contains(playerId)) return 0;
		return m_mMoney[playerId];
	}
	
	bool PlayerHasMoney(int playerId, int amount)
	{
		if(!m_mMoney.Contains(playerId)) return false;
		return m_mMoney[playerId] >= amount;
	}
	
	void AddPlayerMoney(int playerId, int amount)
	{
		if(!m_mMoney.Contains(playerId)) m_mMoney[playerId] = 0;
		m_mMoney[playerId] = m_mMoney[playerId] + amount;
	}
	
	void TakePlayerMoney(int playerId, int amount)
	{
		if(!m_mMoney.Contains(playerId)) return;
		m_mMoney[playerId] = m_mMoney[playerId] - amount;
		if(m_mMoney[playerId] < 0) m_mMoney[playerId] = 0;
	}
	
	void AddResistanceMoney(int amount)
	{
		m_iResistanceMoney += amount;
	}
	
	void TakeResistanceMoney(int amount)
	{
		m_iResistanceMoney -= amount;
		if(m_iResistanceMoney < 0) m_iResistanceMoney = 0;
	}
	
	bool ResistanceHasMoney(int amount)
	{
		return m_iResistanceMoney >= amount;
	}
	
	int GetResistanceMoney()
	{
		return m_iResistanceMoney;
	}
	
	void Init(IEntity owner)
	{		
		m_mMoney = new map<int, int>;
		
		InitializeShops();
	}
	
	protected void InitializeShops()
	{
		#ifdef OVERTHROW_DEBUG
		Print("Finding shops");
		#endif
		
		m_aAllShops = new array<EntityID>;	
		m_mItemCosts = new map<ResourceName, int>;	
		
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, CheckShopInit, FilterShopEntities, EQueryEntitiesFlags.STATIC);
		
	}
	
	bool HasPrice(ResourceName res)
	{
		return m_mItemCosts.Contains(res);
	}
	
	protected bool CheckShopInit(IEntity entity)
	{	
		#ifdef OVERTHROW_DEBUG
		Print("Found Shop");
		#endif
		
		m_aAllShops.Insert(entity.GetID());
		
		OVT_ShopComponent shop = OVT_ShopComponent.Cast(entity.FindComponent(OVT_ShopComponent));
		
		OVT_ShopInventoryConfig config = GetShopConfig(shop.m_ShopType);
		foreach(OVT_ShopInventoryItem item : config.m_aInventoryItems)
		{
			if(!HasPrice(item.prefab))
			{
				SetPrice(item.prefab, item.cost);
			}
			int num = Math.Round(s_AIRandomGenerator.RandFloatXY(1,item.maxAtStart));
			
			shop.AddToInventory(item.prefab, num);
			
			shop.m_aInventoryItems.Insert(item);
		}
		
		return true;
	}
	
	protected bool FilterShopEntities(IEntity entity)
	{	
		if(entity.ClassName() == "SCR_DestructibleBuildingEntity")
		{
			OVT_ShopComponent shop = OVT_ShopComponent.Cast(entity.FindComponent(OVT_ShopComponent));
			if(shop) return true;
		}
		return false;
	}
	
	
}