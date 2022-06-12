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
		
	protected ref array<EntityID> m_aAllShops;
	protected ref array<EntityID> m_aGunDealers;
	
	//Streamed to clients..
	protected ref map<ResourceName, int> m_mItemCosts;		
	protected ref map<int, int> m_mMoney;
	protected int m_iResistanceMoney = 0;
	
	//Events
	ref ScriptInvoker m_OnPlayerMoneyChanged = new ref ScriptInvoker();
	ref ScriptInvoker m_OnResistanceMoneyChanged = new ref ScriptInvoker();
		
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
	
	int GetPrice(ResourceName res, vector pos = "0 0 0")
	{
		if(!m_mItemCosts.Contains(res)) return 0;
		
		int price = m_mItemCosts[res];
		if(pos[0] != 0)
		{
			OVT_TownData town = OVT_TownManagerComponent.GetInstance().GetNearestTown(pos);
			if(town)
			{
				//price should go up as stability goes down				
				float stability = town.stability / 100;
				price = Math.Round(price + (price * 0.1 * (1 - stability)));				
				
				//smaller towns slightly more expensive
				if(town.size < 3)
				{
					price = Math.Round(price * 1.05);
				}
			}
		}
		
		return price;
	}
	
	array<EntityID> GetAllShops()
	{
		return m_aAllShops;
	}
	
	array<EntityID> GetGunDealers()
	{
		return m_aGunDealers;
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
		Rpc(RpcAsk_AddPlayerMoney, playerId, amount);
	}
	
	void TakePlayerMoney(int playerId, int amount)
	{
		Rpc(RpcAsk_TakePlayerMoney, playerId, amount);
	}
	
	void AddResistanceMoney(int amount)
	{
		Rpc(RpcAsk_AddResistanceMoney, amount);
	}
	
	void TakeResistanceMoney(int amount)
	{
		Rpc(RpcAsk_TakeResistanceMoney, amount);		
	}
	
	bool ResistanceHasMoney(int amount)
	{
		return m_iResistanceMoney >= amount;
	}
	
	int GetResistanceMoney()
	{
		return m_iResistanceMoney;
	}
	
	void RegisterGunDealer(EntityID id)
	{
		m_aGunDealers.Insert(id);
	}
	
	void Init(IEntity owner)
	{		
		m_mMoney = new map<int, int>;
		m_aGunDealers = new array<EntityID>;	
		
		if(!Replication.IsServer()) return;
		
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
	
	//RPC Methods
	override bool RplSave(ScriptBitWriter writer)
	{
		//Send JIP price list
		writer.Write(m_mItemCosts.Count(), 32); 
		for(int i; i<m_mItemCosts.Count(); i++)
		{
			string key = m_mItemCosts.GetKey(i);			
			writer.Write(key.Length(),32);
			writer.Write(key, 8 * key.Length());
			writer.Write(m_mItemCosts[key],32);
		}
		
		//Send JIP money map
		writer.Write(m_mMoney.Count(), 32); 
		for(int i; i<m_mMoney.Count(); i++)
		{		
			writer.Write(m_mMoney.GetKey(i),32);
			writer.Write(m_mMoney.GetElement(i), 32);
		}
		writer.Write(m_iResistanceMoney, 32);
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{
		//Recieve JIP price list
		int length, keylength, price, playerId;
		string key;
		
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{
			if (!reader.Read(keylength, 32)) return false;
			key = "";
			if (!reader.Read(key, keylength)) return false;			
			if (!reader.Read(price, 32)) return false;
			m_mItemCosts[key] = price;
		}
		
		//Recieve JIP money map
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{
			if (!reader.Read(playerId, 32)) return false;			
			if (!reader.Read(price, 32)) return false;
			m_mMoney[playerId] = price;
		}
		if (!reader.Read(m_iResistanceMoney, 32)) return false;
		return true;
	}
	
	protected void StreamPlayerMoney(int playerId)
	{
		Rpc(RpcDo_SetPlayerMoney, playerId, m_mMoney[playerId]);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetPlayerMoney(int playerId, int amount)
	{
		m_mMoney[playerId] = amount;
		m_OnPlayerMoneyChanged.Invoke(playerId, m_mMoney[playerId]);
	}
	
	protected void StreamResistanceMoney()
	{
		Rpc(RpcDo_SetResistanceMoney, m_iResistanceMoney);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetResistanceMoney(int amount)
	{
		m_iResistanceMoney = amount;
		m_OnResistanceMoneyChanged.Invoke(m_iResistanceMoney);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddPlayerMoney(int playerId, int amount)
	{
		if(!m_mMoney.Contains(playerId)) m_mMoney[playerId] = 0;
		m_mMoney[playerId] = m_mMoney[playerId] + amount;
		StreamPlayerMoney(playerId);
		m_OnPlayerMoneyChanged.Invoke(playerId, m_mMoney[playerId]);
	}
	
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TakePlayerMoney(int playerId, int amount)
	{
		if(!m_mMoney.Contains(playerId)) return;
		m_mMoney[playerId] = m_mMoney[playerId] - amount;
		if(m_mMoney[playerId] < 0) m_mMoney[playerId] = 0;
		StreamPlayerMoney(playerId);	
		m_OnPlayerMoneyChanged.Invoke(playerId, m_mMoney[playerId]);
	}
	
	protected void RpcAsk_AddResistanceMoney(int amount)
	{
		m_iResistanceMoney -= amount;
		if(m_iResistanceMoney < 0) m_iResistanceMoney = 0;
		StreamResistanceMoney();
	}
	
	protected void RpcAsk_TakeResistanceMoney(int amount)
	{
		m_iResistanceMoney -= amount;
		if(m_iResistanceMoney < 0) m_iResistanceMoney = 0;
		StreamResistanceMoney();
	}
}