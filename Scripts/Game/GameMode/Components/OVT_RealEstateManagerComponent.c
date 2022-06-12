class OVT_RealEstateManagerComponentClass: OVT_ComponentClass
{
};

class OVT_RealEstateManagerComponent: OVT_Component
{
	ref map<int, RplId> m_mHomes;
	ref map<int, ref set<RplId>> m_mOwned;
	
	protected OVT_TownManagerComponent m_Town;
	
	static OVT_RealEstateManagerComponent s_Instance;
	
	static OVT_RealEstateManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_RealEstateManagerComponent.Cast(pGameMode.FindComponent(OVT_RealEstateManagerComponent));
		}

		return s_Instance;
	}
	
	override void OnPostInit(IEntity owner)
	{	
		super.OnPostInit(owner);
		
		if (SCR_Global.IsEditMode()) return;
		
		m_mHomes = new map<int, RplId>;
		m_mOwned = new map<int, ref set<RplId>>;
		m_Town = OVT_TownManagerComponent.Cast(GetOwner().FindComponent(OVT_TownManagerComponent));	
	}
	
	void SetHome(int playerId, IEntity building)
	{	
		RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
		Rpc(RpcAsk_SetHome, playerId, rpl.Id());		
	}
	
	void SetOwner(int playerId, IEntity building)
	{
		RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
		Rpc(RpcAsk_SetOwner, playerId, rpl.Id());	
	}
	
	bool IsOwner(int playerId, EntityID entityId)
	{
		if(!m_mOwned.Contains(playerId)) return false;
		IEntity building = GetGame().GetWorld().FindEntityByID(entityId);
		RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
		set<RplId> owner = m_mOwned[playerId];
		return owner.Contains(rpl.Id());
	}
	
	set<EntityID> GetOwned(int playerId)
	{
		if(!m_mOwned.Contains(playerId)) return new set<EntityID>;
		set<EntityID> entities = new set<EntityID>;
		foreach(RplId id : m_mOwned[playerId])
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			entities.Insert(rpl.GetEntity().GetID());
		}
		return entities;
	}
	
	IEntity GetNearestOwned(int playerId, vector pos)
	{
		if(!m_mOwned.Contains(playerId)) return null;
		
		BaseWorld world = GetGame().GetWorld();
		
		float nearest = 999999;
		IEntity nearestEnt;		
		
		set<RplId> owner = m_mOwned[playerId];
		foreach(RplId id : owner)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			IEntity ent = rpl.GetEntity();
			float dist = vector.Distance(ent.GetOrigin(), pos);
			if(dist < nearest)
			{
				nearest = dist;
				nearestEnt = ent;
			}
		}
		
		return nearestEnt;
	}
	
	IEntity GetHome(int playerId)
	{
		
		if(!m_mHomes.Contains(playerId))
		{
			IEntity newHome = m_Town.GetRandomHouse();
			SetOwner(playerId, newHome);
			SetHome(playerId, newHome);			
			OVT_VehicleManagerComponent.GetInstance().SpawnStartingCar(newHome, playerId);
			return newHome;
		}
		
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(m_mHomes[playerId]));
		if(rpl)
		{
			return rpl.GetEntity();
		}
		
		return null;
	}
	
	//RPC Methods
	override bool RplSave(ScriptBitWriter writer)
	{
		//Send JIP homes
		writer.Write(m_mHomes.Count(), 32); 
		for(int i; i<m_mHomes.Count(); i++)
		{			
			writer.Write(m_mHomes.GetKey(i),32);
			writer.WriteRplId(m_mHomes.GetElement(i));
		}
		
		//Send JIP owned buildings
		writer.Write(m_mOwned.Count(), 32); 
		for(int i; i<m_mOwned.Count(); i++)
		{		
			set<RplId> ownedArray = m_mOwned.GetElement(i);
			
			writer.Write(m_mOwned.GetKey(i),32);
			writer.Write(ownedArray.Count(),32);
			for(int t; t<ownedArray.Count(); t++)
			{	
				writer.WriteRplId(ownedArray[t]);
			}
		}
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{
		//Recieve JIP homes
		int length, playerId, ownedlength;
		RplId id;
		
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{
			if (!reader.Read(playerId, 32)) return false;
			if (!reader.ReadRplId(id)) return false;		
			
			m_mHomes[playerId] = id;
		}
		
		//Recieve JIP owned buildings
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{
			if (!reader.Read(playerId, 32)) return false;
			m_mOwned[playerId] = new set<RplId>;
			
			if (!reader.Read(ownedlength, 32)) return false;
			for(int t; t<ownedlength; t++)
			{
				if (!reader.ReadRplId(id)) return false;
				m_mOwned[playerId].Insert(id);
			}			
		}
		return true;
	}
		
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_SetOwner(int playerId, RplId id)
	{
		DoSetOwner(playerId, id);		
		Rpc(RpcDo_SetOwner, playerId, id);		
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetOwner(int playerId, RplId id)
	{
		DoSetOwner(playerId, id);	
	}
	
	void DoSetOwner(int playerId, RplId id)
	{
		if(!m_mOwned.Contains(playerId)) m_mOwned[playerId] = new set<RplId>;
		set<RplId> owner = m_mOwned[playerId];
		owner.Insert(id);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_SetHome(int playerId, RplId id)
	{
		DoSetHome(playerId, id);		
		Rpc(RpcDo_SetHome, playerId, id);		
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetHome(int playerId, RplId id)
	{
		DoSetHome(playerId, id);	
	}
	
	void DoSetHome(int playerId, RplId id)
	{
		m_mHomes[playerId] = id;
	}
}