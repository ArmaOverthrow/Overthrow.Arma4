class OVT_OwnerManagerComponentClass: OVT_ComponentClass
{
	
}

class OVT_OwnerManagerComponent: OVT_Component
{
	ref map<string, ref set<RplId>> m_mOwned;
	ref map<ref RplId, string> m_mOwners;
	
	void OVT_OwnerManagerComponent()
	{
		m_mOwned = new map<string, ref set<RplId>>;
		m_mOwners = new map<ref RplId, string>;
	}
	
	void SetOwner(int playerId, IEntity building)
	{
		RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
		DoSetOwner(playerId, rpl.Id());		
		Rpc(RpcDo_SetOwner, playerId, rpl.Id());		
	}
	
	string GetOwnerID(IEntity building)
	{
		RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
		if(!m_mOwners.Contains(rpl.Id())) return "";
		return m_mOwners[rpl.Id()];
	}
	
	bool IsOwner(string playerId, EntityID entityId)
	{
		if(!m_mOwned.Contains(playerId)) return false;
		IEntity building = GetGame().GetWorld().FindEntityByID(entityId);
		RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
		set<RplId> owner = m_mOwned[playerId];
		return owner.Contains(rpl.Id());
	}
	
	bool IsOwned(EntityID entityId)
	{
		IEntity building = GetGame().GetWorld().FindEntityByID(entityId);
		RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
		for(int i=0; i< m_mOwned.Count(); i++)
		{
			set<RplId> owner = m_mOwned.GetElement(i);
			if(owner.Contains(rpl.Id()))
			{
				return true;
			}
		}
		return false;
	}
	
	set<EntityID> GetOwned(string playerId)
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
	
	//RPC Methods
	
	override bool RplSave(ScriptBitWriter writer)
	{		
		//Send JIP owned
		writer.Write(m_mOwned.Count(), 32); 
		for(int i; i<m_mOwned.Count(); i++)
		{		
			set<RplId> ownedArray = m_mOwned.GetElement(i);
			RPL_WritePlayerID(writer, m_mOwned.GetKey(i));			
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
		int length, ownedlength;
		string playerId;
		RplId id;
			
		//Recieve JIP owned
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{
			if (!RPL_ReadPlayerID(reader, playerId)) return false;
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
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetOwner(int playerId, RplId id)
	{
		DoSetOwner(playerId, id);	
	}
	
	void DoSetOwner(int playerId, RplId id)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		if(!m_mOwned.Contains(persId)) m_mOwned[persId] = new set<RplId>;
		set<RplId> owner = m_mOwned[persId];
		owner.Insert(id);
		
		m_mOwners[id] = persId;
	}
	
	void ~OVT_OwnerManagerComponent()
	{
		if(m_mOwned)
		{
			m_mOwned.Clear();
			m_mOwned = null;
		}
	}
}