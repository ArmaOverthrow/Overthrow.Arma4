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
		
	protected ref array<RplId> m_aAllShops;
	protected ref array<RplId> m_aGunDealers;
	
	protected InventoryStorageManagerComponent m_Storage;
	
	protected ref map<ResourceName,EntityID> m_mSpawnedItems;
	
	//Streamed to clients..
	protected ref map<RplId, int> m_mItemCosts;		
	protected ref map<string, int> m_mMoney;
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
	
	void OVT_EconomyManagerComponent()
	{
		m_aAllShops = new array<RplId>;	
		m_mItemCosts = new map<RplId, int>;
		m_mMoney = new map<string, int>;
		m_aGunDealers = new array<RplId>;
		m_mSpawnedItems = new map<ResourceName,EntityID>;
	}
	
	void SetPrice(RplId id, int cost)
	{
		m_mItemCosts[id] = cost;
	}
	
	int GetPrice(RplId id, vector pos = "0 0 0")
	{
		if(!m_mItemCosts.Contains(id)) return 0;
		
		int price = m_mItemCosts[id];
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
	
	array<RplId> GetAllShops()
	{
		return m_aAllShops;
	}
	
	array<RplId> GetGunDealers()
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
	
	int GetPlayerMoney(string playerId)
	{
		if(!m_mMoney.Contains(playerId)) return 0;
		return m_mMoney[playerId];
	}
	
	bool PlayerHasMoney(string playerId, int amount)
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
		
		if(!Replication.IsServer()) return;
		
		m_Storage = InventoryStorageManagerComponent.Cast(GetOwner().FindComponent(InventoryStorageManagerComponent));
		
		InitializeShops();
	}
	
	protected void InitializeShops()
	{
		#ifdef OVERTHROW_DEBUG
		Print("Finding shops");
		#endif
		
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, CheckShopInit, FilterShopEntities, EQueryEntitiesFlags.STATIC);
		GetGame().GetCallqueue().CallLater(InitShopInventory, 0);
	}
	
	bool HasPrice(RplId id)
	{
		return m_mItemCosts.Contains(id);
	}
	
	RplId GetInventoryId(ResourceName res)
	{
		IEntity spawnedItem;
		RplComponent rpl;
		if(!m_mSpawnedItems.Contains(res))
		{			
			spawnedItem = GetGame().SpawnEntityPrefab(Resource.Load(res));		
			rpl = RplComponent.Cast(spawnedItem.FindComponent(RplComponent));			
			m_mSpawnedItems[res] = spawnedItem.GetID();
		}else{
			spawnedItem = GetGame().GetWorld().FindEntityByID(m_mSpawnedItems[res]);
			rpl = RplComponent.Cast(spawnedItem.FindComponent(RplComponent));
		}
		return rpl.Id();
	}
	
	protected void InitShopInventory()
	{
		foreach(EntityID entid : m_aAllShops)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(entid);
			OVT_ShopComponent shop = OVT_ShopComponent.Cast(entity.FindComponent(OVT_ShopComponent));
		
			OVT_ShopInventoryConfig config = GetShopConfig(shop.m_ShopType);
			foreach(OVT_ShopInventoryItem item : config.m_aInventoryItems)
			{
				RplId id = GetInventoryId(item.prefab);
				if(!HasPrice(id)){
					SetPrice(id, item.cost);
				}
				
				int num = Math.Round(s_AIRandomGenerator.RandFloatXY(1,item.maxAtStart));
				
				shop.AddToInventory(id, num);
				
				shop.m_aInventoryItems.Insert(item);
			}
		}
	}
	
	protected bool CheckShopInit(IEntity entity)
	{	
		#ifdef OVERTHROW_DEBUG
		Print("Found Shop");
		#endif
		
		m_aAllShops.Insert(entity.GetID());
		
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
			writer.WriteRplId(m_mItemCosts.GetKey(i));
			writer.Write(m_mItemCosts.GetElement(i),32);
		}
		
		//Send JIP money map
		writer.Write(m_mMoney.Count(), 32); 
		for(int i; i<m_mMoney.Count(); i++)
		{			
			RPL_WritePlayerID(writer, m_mMoney.GetKey(i));
			writer.Write(m_mMoney.GetElement(i), 32);
		}
		writer.Write(m_iResistanceMoney, 32);
		
		//Send JIP Shops
		writer.Write(m_aAllShops.Count(), 32); 
		for(int i; i<m_aAllShops.Count(); i++)
		{
			writer.WriteRplId(m_aAllShops[i]);
		}
		
		//Send JIP Gun Dealers
		writer.Write(m_aGunDealers.Count(), 32); 
		for(int i; i<m_aGunDealers.Count(); i++)
		{
			writer.WriteRplId(m_aGunDealers[i]);
		}
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{
		//Recieve JIP price list
		int length, keylength, price;
		string playerId;
		RplId key;
		
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{
			if (!reader.ReadRplId(key)) return false;
			if (!reader.Read(price, 32)) return false;
			m_mItemCosts[key] = price;
		}
		
		//Recieve JIP money map
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{
			if(!RPL_ReadPlayerID(reader, playerId)) return false;
			if (!reader.Read(price, 32)) return false;
			m_mMoney[playerId] = price;
		}
		if (!reader.Read(m_iResistanceMoney, 32)) return false;
		
		//Recieve JIP shops		
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{			
			if (!reader.ReadRplId(key)) return false;
			m_aAllShops.Insert(key);
		}
		
		//Recieve JIP gun dealers		
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{			
			if (!reader.ReadRplId(key)) return false;
			m_aGunDealers.Insert(key);
		}
		
		return true;
	}
	
	
	protected void StreamPlayerMoney(int playerId)
	{
		string persId = OVT_PlayerManagerComponent.GetInstance().GetPersistentIDFromPlayerID(playerId);
		Rpc(RpcDo_SetPlayerMoney, playerId, m_mMoney[persId]);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetPlayerMoney(int playerId, int amount)
	{
		string persId = OVT_PlayerManagerComponent.GetInstance().GetPersistentIDFromPlayerID(playerId);
		m_mMoney[persId] = amount;
		m_OnPlayerMoneyChanged.Invoke(persId, m_mMoney[persId]);
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
		string persId = OVT_PlayerManagerComponent.GetInstance().GetPersistentIDFromPlayerID(playerId);
		if(!m_mMoney.Contains(persId)) m_mMoney[persId] = 0;
		m_mMoney[persId] = m_mMoney[persId] + amount;
		StreamPlayerMoney(playerId);
		m_OnPlayerMoneyChanged.Invoke(persId, m_mMoney[persId]);
	}
	
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TakePlayerMoney(int playerId, int amount)
	{
		string persId = OVT_PlayerManagerComponent.GetInstance().GetPersistentIDFromPlayerID(playerId);
		if(!m_mMoney.Contains(persId)) return;
		m_mMoney[persId] = m_mMoney[persId] - amount;
		if(m_mMoney[persId] < 0) m_mMoney[persId] = 0;
		StreamPlayerMoney(playerId);	
		m_OnPlayerMoneyChanged.Invoke(persId, m_mMoney[persId]);
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