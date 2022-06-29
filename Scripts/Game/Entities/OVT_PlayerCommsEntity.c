class OVT_PlayerCommsEntityClass: GenericEntityClass
{
};

class OVT_PlayerCommsEntity: GenericEntity
{
	void OVT_PlayerCommsEntity(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT);
	}
	
	override void EOnInit(IEntity owner)
	{
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		mode.m_Server = this;
	}
	
	void RegisterPersistentID(string persistentID)
	{
		int int1,int2,int3;
		OVT_PlayerManagerComponent.EncodeIDAsInts(persistentID, int1, int2, int3);
		Print("Registering persistent ID with server: " + persistentID);
		int playerId = SCR_PlayerController.GetLocalPlayerId();
		
		Rpc(RpcAsk_SetID, playerId, int1, int2, int3);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_SetID(int playerId, int id1, int id2, int id3)
	{
		string persistentID = ""+id1+id2+id3;				
		OVT_Global.GetPlayers().RegisterPlayer(playerId, persistentID);
	}	
	
	void StartBaseCapture(vector loc)
	{		
		Rpc(RpcAsk_StartBaseCapture, loc);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_StartBaseCapture(vector loc)
	{	
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		OVT_BaseData data = of.GetNearestBase(loc);
		OVT_BaseControllerComponent base = of.GetBase(data.entId);
		of.StartBaseQRF(base);
	}
	
	//SHOPS
	
	void AddToShopInventory(OVT_ShopComponent shop, RplId id, int num)
	{
		RplComponent rpl = RplComponent.Cast(shop.GetOwner().FindComponent(RplComponent));
		Rpc(RpcAsk_AddToInventory, rpl.Id(), id, num);
	}
		
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddToInventory(RplId shopId, RplId id, int num)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(shopId));
		OVT_ShopComponent shop = OVT_ShopComponent.Cast(rpl.GetEntity().FindComponent(OVT_ShopComponent));
		
		if(!shop.m_aInventory.Contains(id))
		{
			shop.m_aInventory[id] = 0;
		}		
		shop.m_aInventory[id] = shop.m_aInventory[id] + num;
		shop.StreamInventory(id);
	}
	
	void TakeFromShopInventory(OVT_ShopComponent shop, RplId id, int num)
	{
		RplComponent rpl = RplComponent.Cast(shop.GetOwner().FindComponent(RplComponent));
		Rpc(RpcAsk_TakeFromInventory, rpl.Id(), id, num);
	}	
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TakeFromInventory(RplId shopId, RplId id, int num)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(shopId));
		OVT_ShopComponent shop = OVT_ShopComponent.Cast(rpl.GetEntity().FindComponent(OVT_ShopComponent));
				
		if(!shop.m_aInventory.Contains(id)) return;		
		shop.m_aInventory[id] = shop.m_aInventory[id] - num;		
		if(shop.m_aInventory[id] < 0) shop.m_aInventory[id] = 0;
		shop.StreamInventory(id);
	}
	
	//ECONOMY
	
	void AddPlayerMoney(int playerId, int amount)
	{
		Rpc(RpcAsk_AddPlayerMoney, playerId, amount);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddPlayerMoney(int playerId, int amount)
	{
		OVT_Global.GetEconomy().DoAddPlayerMoney(playerId, amount);		
	}
	
	void TakePlayerMoney(int playerId, int amount)
	{
		Rpc(RpcAsk_TakePlayerMoney, playerId, amount);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TakePlayerMoney(int playerId, int amount)
	{
		OVT_Global.GetEconomy().DoTakePlayerMoney(playerId, amount);		
	}
	
	//PLACING
	void PlaceItem(int placeableIndex, int prefabIndex, vector pos, vector angles, int playerId)
	{
		Rpc(RpcAsk_PlaceItem, placeableIndex, prefabIndex, pos, angles);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_PlaceItem(int placeableIndex, int prefabIndex, vector pos, vector angles, int playerId)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		OVT_Placeable placeable = config.m_aPlaceables[placeableIndex];
		ResourceName res = placeable.m_aPrefabs[prefabIndex];
		
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		vector mat[4];
		Math3D.AnglesToMatrix(angles, mat);
		mat[3] = pos;
		params.Transform = mat;
		
		IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(res), GetGame().GetWorld(), params);
		
		if(placeable.handler)
		{
			placeable.handler.OnPlace(entity, playerId);
		}
	}
}